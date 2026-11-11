#include <rag/engine.hpp>
#include <rag/security/authorization.hpp>

#include <nlohmann/json.hpp>

#include <chrono>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

struct QueryCase {
    std::string id;
    std::string query;
    std::string department;
    std::string role;
};

struct Metrics {
    double recall = 0.0;
    double mrr = 0.0;
    double latency_ms = 0.0;
    std::size_t results = 0;
    std::size_t unauthorized = 0;
};

using Qrels = std::map<std::string, std::set<std::string>>;


/*
 * ============================================================
 * Load evaluation queries
 *
 * queries.json contains:
 *
 * {
 *   "id": "q1",
 *   "query": "...",
 *   "department": "engineering",
 *   "role": "employee"
 * }
 *
 * Relevance information lives separately in qrels.json.
 * ============================================================
 */
std::vector<QueryCase> load_queries(
    const std::string& path) {

    std::ifstream file(path);

    if (!file) {
        throw std::runtime_error(
            "Could not open queries file: " + path);
    }

    nlohmann::json data;
    file >> data;

    if (!data.is_array()) {
        throw std::runtime_error(
            "Queries file must contain a JSON array.");
    }

    std::vector<QueryCase> queries;

    for (const auto& item : data) {

        QueryCase q;

        q.id =
            item.at("id").get<std::string>();

        q.query =
            item.at("query").get<std::string>();

        q.department =
            item.at("department").get<std::string>();

        q.role =
            item.at("role").get<std::string>();

        queries.push_back(
            std::move(q));
    }

    return queries;
}


/*
 * ============================================================
 * Load qrels
 *
 * qrels.json format:
 *
 * [
 *   {
 *     "q1": {
 *       "enterprise_documents.csv#1": 1,
 *       "enterprise_documents.csv#4": 1
 *     }
 *   }
 * ]
 *
 * Only documents with relevance > 0 are considered relevant.
 * ============================================================
 */
Qrels load_qrels(
    const std::string& path) {

    std::ifstream file(path);

    if (!file) {
        throw std::runtime_error(
            "Could not open qrels file: " + path);
    }

    nlohmann::json data;
    file >> data;

    if (!data.is_array()) {
        throw std::runtime_error(
            "Qrels file must contain a JSON array.");
    }

    Qrels qrels;

    for (const auto& item : data) {

        if (!item.is_object() ||
            item.size() != 1) {

            throw std::runtime_error(
                "Invalid qrels entry.");
        }

        const auto query_id =
            item.begin().key();

        const auto& documents =
            item.begin().value();

        if (!documents.is_object()) {

            throw std::runtime_error(
                "Invalid qrels document mapping for query: "
                + query_id);
        }

        for (auto it = documents.begin();
             it != documents.end();
             ++it) {

            const std::string uri =
                it.key();

            const int relevance =
                it.value().get<int>();

            if (relevance > 0) {
                qrels[query_id].insert(uri);
            }
        }
    }

    return qrels;
}


/*
 * ============================================================
 * Check authorization using actual persisted metadata.
 *
 * IMPORTANT:
 *
 * This intentionally does NOT use hard-coded document URIs.
 *
 * Authorization is determined from:
 *
 *   document.meta["department"]
 *   document.meta["role"]
 *
 * This makes the security benchmark independent of the
 * particular document IDs/URIs used in the corpus.
 * ============================================================
 */
bool is_authorized(
    const rag::Engine& engine,
    const rag::SearchResult& result,
    const QueryCase& query) {

    const auto* document =
        engine.corpus().document(result.doc);

    if (!document) {
        return false;
    }

    const auto get_metadata =
        [&document](const char* key) -> std::string {

            const auto it =
                document->meta.find(key);

            if (it == document->meta.end()) {
                return {};
            }

            return it->second;
        };

    const std::string department =
        get_metadata("department");

    const std::string role =
        get_metadata("role");


    /*
     * Public documents are visible to everyone.
     */
    if (department == "public") {
        return true;
    }


    /*
     * Private documents must belong to the
     * user's department.
     */
    if (department != query.department) {
        return false;
    }


    /*
     * Empty role or "any" means unrestricted role.
     */
    if (role.empty() ||
        role == "any") {

        return true;
    }


    /*
     * Otherwise the user's role must match.
     */
    return role == query.role;
}


/*
 * ============================================================
 * Evaluate retrieved results
 * ============================================================
 */
Metrics evaluate_results(
    const rag::Engine& engine,
    const std::vector<rag::SearchResult>& results,
    const QueryCase& query,
    const std::set<std::string>& relevant,
    double latency_ms) {

    Metrics metrics;

    metrics.latency_ms =
        latency_ms;

    metrics.results =
        results.size();

    std::set<std::string> retrieved_docs;

    std::size_t rank = 0;

    std::size_t first_relevant_rank = 0;


    /*
     * --------------------------------------------------------
     * Inspect retrieved documents
     * --------------------------------------------------------
     */
    for (const auto& result : results) {

        ++rank;

        retrieved_docs.insert(
            result.uri);


        /*
         * Independent security verification.
         *
         * The retrieval engine has already applied the
         * authorization filter.
         *
         * We independently inspect the actual document
         * metadata to verify that no unauthorized document
         * reached the result set.
         */
        if (!is_authorized(
                engine,
                result,
                query)) {

            ++metrics.unauthorized;
        }


        /*
         * Find the first relevant result for MRR.
         */
        if (relevant.contains(result.uri) &&
            first_relevant_rank == 0) {

            first_relevant_rank =
                rank;
        }
    }


    /*
     * --------------------------------------------------------
     * Recall@5
     *
     * Number of relevant documents retrieved
     * divided by total relevant documents.
     * --------------------------------------------------------
     */
    std::size_t relevant_found = 0;

    for (const auto& relevant_uri : relevant) {

        if (retrieved_docs.contains(relevant_uri)) {
            ++relevant_found;
        }
    }

    if (!relevant.empty()) {

        metrics.recall =
            static_cast<double>(
                relevant_found) /
            static_cast<double>(
                relevant.size());
    }


    /*
     * --------------------------------------------------------
     * MRR@5
     *
     * Reciprocal rank of the first relevant result.
     * --------------------------------------------------------
     */
    if (first_relevant_rank != 0) {

        metrics.mrr =
            1.0 /
            static_cast<double>(
                first_relevant_rank);
    }

    return metrics;
}


/*
 * ============================================================
 * Print aggregate metrics
 * ============================================================
 */
void print_metrics(
    const std::string& name,
    const std::vector<Metrics>& metrics) {

    double recall = 0.0;
    double mrr = 0.0;
    double latency = 0.0;

    std::size_t results = 0;
    std::size_t unauthorized = 0;


    for (const auto& m : metrics) {

        recall +=
            m.recall;

        mrr +=
            m.mrr;

        latency +=
            m.latency_ms;

        results +=
            m.results;

        unauthorized +=
            m.unauthorized;
    }


    const double count =
        static_cast<double>(
            metrics.size());


    const double avg_recall =
        count == 0.0
            ? 0.0
            : recall / count;


    const double avg_mrr =
        count == 0.0
            ? 0.0
            : mrr / count;


    const double avg_latency =
        count == 0.0
            ? 0.0
            : latency / count;


    const double leakage =
        results == 0
            ? 0.0
            : static_cast<double>(
                  unauthorized) /
              static_cast<double>(
                  results);


    std::cout
        << "\n"
        << name
        << "\n"
        << "----------------------------------------\n";


    std::cout
        << "Recall@5:              "
        << avg_recall
        << "\n";


    std::cout
        << "MRR@5:                 "
        << avg_mrr
        << "\n";


    std::cout
        << "Average latency:       "
        << avg_latency
        << " ms\n";


    std::cout
        << "Total results:         "
        << results
        << "\n";


    std::cout
        << "Unauthorized results:  "
        << unauthorized
        << "\n";


    std::cout
        << "Authorization leakage: "
        << leakage * 100.0
        << "%\n";
}

} // namespace


int main() {

    try {

        /*
         * ----------------------------------------------------
         * Paths
         *
         * The benchmark executable is normally launched from
         * the project root:
         *
         * C:\Users\hp\rag-cpp
         * ----------------------------------------------------
         */
        const std::string db_path =
            "enterprise_eval.ragdb";

        const std::string query_path =
            "evaluation/queries.json";

        const std::string qrels_path =
            "evaluation/qrels.json";

        constexpr std::size_t K = 5;


        std::cout
            << "========================================\n"
            << " Enterprise Retrieval Evaluation\n"
            << "========================================\n\n";


        /*
         * ----------------------------------------------------
         * Open persisted evaluation database.
         * ----------------------------------------------------
         */
        auto engine_result =
            rag::Engine::open(
                db_path);


        if (!engine_result) {

            std::cerr
                << "Failed to open database.\n";

            return 1;
        }


        auto engine =
            std::move(
                *engine_result);


        /*
         * ----------------------------------------------------
         * Reattach Ollama embedder.
         *
         * The embedder backend is runtime state and therefore
         * needs to be attached after loading the persisted DB.
         * ----------------------------------------------------
         */
        auto embedder_result =
            engine.with_embedder_spec(
                nlohmann::json{
                    {"type", "ollama"},
                    {"host", "127.0.0.1"},
                    {"port", 11434},
                    {"model", "nomic-embed-text"},
                    {"dim", 768}
                });


        if (!embedder_result) {

            std::cerr
                << "Failed to attach Ollama embedder.\n";

            return 1;
        }


        /*
         * ----------------------------------------------------
         * Load queries and relevance judgments.
         * ----------------------------------------------------
         */
        const auto queries =
            load_queries(
                query_path);


        const auto qrels =
            load_qrels(
                qrels_path);


        std::cout
            << "Loaded queries: "
            << queries.size()
            << "\n";


        std::cout
            << "Loaded qrels:   "
            << qrels.size()
            << "\n\n";


        /*
         * ----------------------------------------------------
         * Validate that every query has qrels.
         * ----------------------------------------------------
         */
        for (const auto& q : queries) {

            if (!qrels.contains(q.id)) {

                throw std::runtime_error(
                    "Missing qrels for query: "
                    + q.id);
            }
        }


        /*
         * ----------------------------------------------------
         * Validate that qrels do not reference unknown
         * queries.
         * ----------------------------------------------------
         */
        for (const auto& [query_id, relevant] : qrels) {

            (void)relevant;

            bool found = false;

            for (const auto& q : queries) {

                if (q.id == query_id) {

                    found = true;

                    break;
                }
            }

            if (!found) {

                throw std::runtime_error(
                    "Qrels contains unknown query: "
                    + query_id);
            }
        }


        std::vector<Metrics> bm25_metrics;
        std::vector<Metrics> dense_metrics;
        std::vector<Metrics> hybrid_metrics;


        /*
         * ====================================================
         * Evaluate every query
         * ====================================================
         */
        for (const auto& q : queries) {

            const auto& relevant =
                qrels.at(q.id);


            /*
             * ------------------------------------------------
             * Construct authorization context.
             * ------------------------------------------------
             */
            rag::security::UserContext user{
                "benchmark-user",
                q.department,
                q.role
            };


            rag::security::AuthorizationPolicy policy(
                user);


            auto authorization_filter =
                policy.filter();


            /*
             * =================================================
             * BM25
             * =================================================
             *
             * lexical_search() does not directly accept a
             * MetaFilter.
             *
             * Therefore:
             *
             *   BM25 candidate retrieval
             *       ↓
             *   authorization filtering
             *       ↓
             *   top K
             */
            {
                const auto start =
                    std::chrono::steady_clock::now();


                const auto hits =
                    engine.corpus()
                        .lexical_search(
                            q.query,
                            50);


                std::vector<rag::SearchResult> results;


                for (const auto& hit : hits) {

                    const auto result =
                        engine.corpus()
                            .resolve(hit);


                    const auto* document =
                        engine.corpus()
                            .document(
                                result.doc);


                    if (document &&
                        authorization_filter(
                            document->meta)) {

                        results.push_back(
                            result);
                    }


                    if (results.size() >= K) {
                        break;
                    }
                }


                const auto end =
                    std::chrono::steady_clock::now();


                const double latency =
                    std::chrono::duration<
                        double,
                        std::milli>(
                            end - start)
                        .count();


                bm25_metrics.push_back(
                    evaluate_results(
                        engine,
                        results,
                        q,
                        relevant,
                        latency));
            }


            /*
             * =================================================
             * Dense
             * =================================================
             *
             * Dense retrieval receives the authorization
             * filter directly during ANN traversal.
             */
            {
                const auto start =
                    std::chrono::steady_clock::now();


                auto hits =
                    engine.corpus()
                        .dense_search(
                            q.query,
                            K,
                            authorization_filter);


                std::vector<rag::SearchResult> results;


                if (hits) {

                    for (const auto& hit : *hits) {

                        results.push_back(
                            engine.corpus()
                                .resolve(hit));
                    }
                }


                const auto end =
                    std::chrono::steady_clock::now();


                const double latency =
                    std::chrono::duration<
                        double,
                        std::milli>(
                            end - start)
                        .count();


                dense_metrics.push_back(
                    evaluate_results(
                        engine,
                        results,
                        q,
                        relevant,
                        latency));
            }


            /*
             * =================================================
             * Hybrid
             * =================================================
             *
             * Production retrieval pipeline:
             *
             *   hybrid retrieval
             *       ↓
             *   authorization filter
             *       ↓
             *   feature reranking
             *       ↓
             *   top K
             */
            {
                const auto start =
                    std::chrono::steady_clock::now();


                auto results =
                    engine.search(
                        q.query,
                        K,
                        authorization_filter,
                        nullptr);


                const auto end =
                    std::chrono::steady_clock::now();


                const double latency =
                    std::chrono::duration<
                        double,
                        std::milli>(
                            end - start)
                        .count();


                if (!results) {

                    hybrid_metrics.push_back(
                        evaluate_results(
                            engine,
                            {},
                            q,
                            relevant,
                            latency));

                } else {

                    hybrid_metrics.push_back(
                        evaluate_results(
                            engine,
                            *results,
                            q,
                            relevant,
                            latency));
                }
            }


            std::cout
                << q.id
                << " evaluated\n";
        }


        /*
         * ====================================================
         * Final comparison
         * ====================================================
         */
        print_metrics(
            "BM25",
            bm25_metrics);


        print_metrics(
            "Dense",
            dense_metrics);


        print_metrics(
            "Hybrid",
            hybrid_metrics);


        /*
         * ====================================================
         * Security summary
         * ====================================================
         */
        std::size_t total_unauthorized = 0;


        for (const auto& m : bm25_metrics) {

            total_unauthorized +=
                m.unauthorized;
        }


        for (const auto& m : dense_metrics) {

            total_unauthorized +=
                m.unauthorized;
        }


        for (const auto& m : hybrid_metrics) {

            total_unauthorized +=
                m.unauthorized;
        }


        std::cout
            << "\n========================================\n"
            << " Security Summary\n"
            << "========================================\n";


        std::cout
            << "Unauthorized results across all modes: "
            << total_unauthorized
            << "\n";


        if (total_unauthorized == 0) {

            std::cout
                << "Authorization leakage: 0%\n"
                << "Security check passed.\n";

        } else {

            std::cout
                << "WARNING: Authorization leakage detected.\n";

            return 2;
        }


        return 0;

    } catch (const std::exception& e) {

        std::cerr
            << "ERROR: "
            << e.what()
            << "\n";

        return 1;
    }
}