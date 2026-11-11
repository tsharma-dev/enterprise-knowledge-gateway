// cli/main.cpp — the `ragcpp` turnkey command-line tool.
//
//   ragcpp index  <dir> <out.ragdb> [--ext=.md] [--semantic] [--proposition] [--source]
//   ragcpp query  <db.ragdb> "<query>" [-k N] [--mmr] [--explain]
//   ragcpp serve  <db.ragdb> [--http PORT] [--write] [--graph] [--memory] [--feedback] [--all]
//   ragcpp eval   <beir-dir> [--split=test]
//   ragcpp info   <db.ragdb>
//
// A thin driver over the library so you can build, search, and SERVE a corpus
// without writing a line of C++. Lexical/BM25 by default (no model, no
// network); attach an embedder in code for hybrid. `serve` brings up a
// conformant RCP/1 endpoint (github.com/1ay1/rcp) over stdio or HTTP.

#include <cctype>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include <rag/rag.hpp>
#if RAGCPP_WITH_RCP
#include <rag/rcp/rcp.hpp>
#endif
#include <rag/security/authorization.hpp>

namespace {

int usage() {
    std::printf(
        "ragcpp — a type-theoretic RAG engine\n\n"
        "usage:\n"
        "  ragcpp index <dir> <out.ragdb> [--ext=.md] [--semantic] [--proposition] [--source] [--contextual]\n"
        "  ragcpp query <db.ragdb> \"<query>\" [-k N] [--mmr] [--explain]\n"
        "  ragcpp serve <db.ragdb> [--http PORT] [--write] [--graph] [--memory] [--feedback] [--all]\n"
        "  ragcpp eval  <beir-dir> [--split=test]\n"
        "  ragcpp info  <db.ragdb>\n"
        "  ragcpp list  [embedders|rerankers]\n");
    return 2;
}

std::string opt(const std::vector<std::string>& a, std::string_view key, std::string def) {
    for (const auto& s : a)
        if (s.rfind(key, 0) == 0 && s.size() > key.size() && s[key.size()] == '=')
            return s.substr(key.size() + 1);
    return def;
}
bool flag(const std::vector<std::string>& a, std::string_view f) {
    for (const auto& s : a) if (s == f) return true;
    return false;
}
rag::security::UserContext parse_user_context(
    const std::vector<std::string>& args) {

    rag::security::UserContext user;

    user.user_id = opt(args, "--user", "");
    user.department = opt(args, "--department", "");
    user.role = opt(args, "--role", "");

    return user;
}

int cmd_index(const std::vector<std::string>& args) {
    if (args.size() < 2) return usage();

    const std::string& dir = args[0];
    const std::string& out = args[1];

    rag::index::CorpusConfig ccfg;

    if (flag(args, "--semantic"))
        ccfg.chunking = rag::index::CorpusConfig::Chunking::semantic;

    if (flag(args, "--proposition"))
        ccfg.chunking = rag::index::CorpusConfig::Chunking::proposition;

    if (flag(args, "--source"))
        ccfg.chunking = rag::index::CorpusConfig::Chunking::source;

    if (flag(args, "--contextual"))
        ccfg.contextual = true;

    // Use Engine so the Ollama embedder is attached during indexing.
    rag::Engine engine{ccfg};

    auto embedder_result = engine.with_embedder_spec(
        nlohmann::json{
            {"type", "ollama"},
            {"host", "127.0.0.1"},
            {"port", 11434},
            {"model", "nomic-embed-text"},
            {"dim", 768}
        });

    if (!embedder_result) {
        std::printf(
            "embedder error: %s\n",
            embedder_result.error().message.c_str()
        );
        return 1;
    }

    rag::loaders::DirOptions lo;

    std::vector<std::string> exts;

    auto add_ext = [&](std::string e) {
        if (e.empty()) return;

        if (e.rfind("*.", 0) == 0)
            e.erase(0, 1);

        if (e[0] != '.')
            e = "." + e;

        for (char& c : e)
            c = static_cast<char>(
                std::tolower(static_cast<unsigned char>(c)));

        exts.push_back(std::move(e));
    };

    for (const std::string& a : args) {
        std::string_view v(a);
        std::string_view pfx;

        if (v.rfind("--ext=", 0) == 0)
            pfx = "--ext=";
        else if (v.rfind("--glob=", 0) == 0)
            pfx = "--glob=";
        else
            continue;

        std::string_view rest = v.substr(pfx.size());

        std::size_t start = 0;

        while (start <= rest.size()) {
            std::size_t comma = rest.find(',', start);

            if (comma == std::string_view::npos)
                comma = rest.size();

            add_ext(
                std::string(rest.substr(
                    start,
                    comma - start)));

            start = comma + 1;
        }
    }

    if (!exts.empty())
        lo.include_ext = std::move(exts);

    rag::Result<std::vector<rag::loaders::LoadedDoc>> docs =
        std::vector<rag::loaders::LoadedDoc>{};

    {
        std::filesystem::path p{dir};

        auto ext_lower = p.extension().string();

        for (auto& ch : ext_lower)
            ch = static_cast<char>(
                std::tolower(static_cast<unsigned char>(ch)));

        const bool is_table =
            std::filesystem::is_regular_file(p) &&
            (ext_lower == ".csv" ||
             ext_lower == ".tsv" ||
             ext_lower == ".tab");

        if (is_table) {
            rag::loaders::CsvOptions co;

            co.title_column = opt(args, "--csv-title", "");
            co.id_column    = opt(args, "--csv-id", "");

            // For enterprise RAG, only index the actual content columns.
            // Keep id/department/role as metadata for authorization/filtering.
            std::string cols = opt(args, "--csv-text", "title,text");

            if (!cols.empty()) {

                std::size_t start = 0;

                while (start <= cols.size()) {
                    auto comma = cols.find(',', start);

                    auto piece =
                        cols.substr(
                            start,
                            comma == std::string::npos
                                ? std::string::npos
                                : comma - start);

                    if (!piece.empty())
                        co.text_columns.push_back(piece);

                    if (comma == std::string::npos)
                        break;

                    start = comma + 1;
                }
            }

            docs = rag::loaders::load_csv(p, co);

        } else {
            docs = rag::loaders::load_directory(dir, lo);
        }
    }

    if (!docs) {
        std::printf(
            "load error: %s\n",
            docs.error().message.c_str());
        return 1;
    }

    std::size_t n = 0;

    for (auto& d : *docs) {
        auto r = engine.corpus().upsert_document(
            d.uri,
            d.text,
            d.meta,
            d.title);

        if (r)
            ++n;
        else
            std::printf(
                "add error: %s\n",
                r.error().message.c_str());
    }

    if (auto b = engine.build(); !b) {
        std::printf(
            "build error: %s\n",
            b.error().message.c_str());
        return 1;
    }

    if (auto s = engine.save(out); !s) {
        std::printf(
            "save error: %s\n",
            s.error().message.c_str());
        return 1;
    }

    std::printf(
        "indexed %zu documents, %zu chunks -> %s\n",
        n,
        engine.corpus().chunk_count(),
        out.c_str());

    return 0;
}

int cmd_query(const std::vector<std::string>& args) {
    if (args.size() < 2)
        return usage();

    auto engine = rag::Engine::open(args[0]);

    if (!engine) {
        std::printf(
            "open error: %s\n",
            engine.error().message.c_str());
        return 1;
    }

    // The embedder is intentionally not serialized inside .ragdb.
    // Reattach the same embedding backend used during indexing.
    auto embedder_result = engine->with_embedder_spec(
        nlohmann::json{
            {"type", "ollama"},
            {"host", "127.0.0.1"},
            {"port", 11434},
            {"model", "nomic-embed-text"},
            {"dim", 768}
        });

    auto user = parse_user_context(args);

    if (user.user_id.empty() ||
        user.department.empty() ||
        user.role.empty()) {
        std::printf(
            "authorization error: "
            "--user, --department and --role are required\n");
        return 1;
    }

    rag::security::AuthorizationPolicy policy(user);
    auto authorization_filter = policy.filter();

    if (!embedder_result) {
        std::printf(
            "embedder error: %s\n",
            embedder_result.error().message.c_str());
        return 1;
    }

    const std::string& q = args[1];

    std::size_t k = 5;

    for (std::size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "-k" &&
            i + 1 < args.size()) {

            k = std::stoul(args[i + 1]);
        }
    }

    std::vector<std::string> trace;

    auto results =
        engine->search(
            q,
            k,
            authorization_filter,
            flag(args, "--explain")
                ? &trace
                : nullptr);

    if (!results) {
        std::printf(
            "query error: %s\n",
            results.error().message.c_str());
        return 1;
    }

    std::printf(
        "query: %s  (%zu results)\n",
        q.c_str(),
        results->size());

    std::size_t citation_number = 1;

    for (const auto& r : *results) {
        std::printf(
            "\n[%zu] Source\n"
            "    uri:    %s\n"
            "    chunk:  %llu\n"
            "    score:  %.3f\n",
            citation_number++,
            r.uri.c_str(),
            static_cast<unsigned long long>(r.chunk.get()),
            r.score.get());

        if (r.start_line != 0 || r.end_line != 0) {
            std::printf(
                "    lines:  %u-%u\n",
                r.start_line,
                r.end_line);
        }

        std::printf(
            "    text:   %.500s\n",
            r.text.c_str());
    }

    if (flag(args, "--explain")) {
        std::printf("\npipeline:\n");

        for (const auto& stage : trace)
            std::printf("  -> %s\n", stage.c_str());
    }

    return 0;
}

int cmd_eval(const std::vector<std::string>& args) {
    if (args.empty()) return usage();
    std::string split = opt(args, "--split", "test");
    auto ds = rag::eval::BeirDataset::load(args[0], split);
    if (!ds) { std::printf("load error: %s\n", ds.error().message.c_str()); return 1; }
    rag::index::Corpus corpus;
    auto m = rag::eval::evaluate_corpus(*ds, corpus);
    if (!m) { std::printf("eval error: %s\n", m.error().message.c_str()); return 1; }
    std::printf("%s\n", m->report().c_str());
    return 0;
}

#if RAGCPP_WITH_RCP
// Bring up an RCP/1 server backed by a saved corpus. stdio by default (the
// convention for editor / agent integration); `--http PORT` for loopback HTTP.
// `--write` advertises a writable index; `--graph` advertises GraphRAG.
int cmd_serve(const std::vector<std::string>& args) {
    if (args.empty()) return usage();
    auto engine = rag::Engine::open(args[0]);
    if (!engine) { std::printf("open error: %s\n", engine.error().message.c_str()); return 1; }

    // Resolve --http PORT (space-separated) and boolean feature flags.
    int http_port = -1;
    for (std::size_t i = 1; i < args.size(); ++i)
        if (args[i] == "--http" && i + 1 < args.size()) http_port = std::stoi(args[i + 1]);

    rag::rcp::Options opts;
    opts.named("ragcpp", "1.0");
    if (flag(args, "--write")) {
        opts.with_index(true);
        // A writable index that never writes anything back is a lie to the
        // client: index/add and index/delete were accepted, acknowledged, and
        // then lost when the process exited. Persist to the same file we were
        // opened from, so a mutation that was acknowledged is a mutation that
        // survives a restart.
        opts.persisting_to(args[0]);

        // ...and do it through a write-ahead log, so "acknowledged" costs an
        // O(record) append instead of an O(corpus) snapshot rewrite. Measured:
        // index/add over RCP on a 20k-document corpus went from 25.1 ms to
        // 0.25 ms, and the old cost grew with the corpus while the new one does
        // not. The log sits beside the index as <db>.wal.
        //
        // Opened AFTER the engine loads, because open_wal replays any log left
        // by a previous crash into the corpus — that replay IS the recovery.
        const std::string wal_path = args[0] + ".wal";
        if (auto w = engine->corpus().open_wal(wal_path); !w) {
            std::printf("wal error: %s\n", w.error().message.c_str());
            return 1;
        }
        opts.with_wal(wal_path);
    }
    if (flag(args, "--graph")) opts.with_graph(true);
    // memory/* and feedback/* are backed entirely by the engine itself — they
    // need no external model, no API key and no extra process. Leaving them off
    // by default meant the turnkey server advertised strictly less than it could
    // actually do; --all turns on everything that costs nothing to provide.
    if (flag(args, "--all")) {
        opts.with_memory(true);
        opts.with_feedback(true);
        opts.with_graph(true);
    } else {
        if (flag(args, "--memory"))   opts.with_memory(true);
        if (flag(args, "--feedback")) opts.with_feedback(true);
    }

    if (http_port > 0) {
        std::fprintf(stderr, "ragcpp: RCP/1 server on http://127.0.0.1:%d  (corpus: %s)\n",
                     http_port, args[0].c_str());
        auto r = rag::rcp::serve_http(*engine, static_cast<std::uint16_t>(http_port), std::move(opts));
        if (!r) { std::printf("serve error: %s\n", r.error().message.c_str()); return 1; }
        return 0;
    }
    std::fprintf(stderr, "ragcpp: RCP/1 server on stdio  (corpus: %s)\n", args[0].c_str());
    rag::rcp::serve_stdio(*engine, std::move(opts));
    return 0;
}
#else
int cmd_serve(const std::vector<std::string>&) {
    std::printf("serve: rebuild with -DRAGCPP_WITH_RCP=ON to enable the RCP/1 server\n");
    return 1;
}
#endif

int cmd_info(const std::vector<std::string>& args) {
    if (args.empty())
        return usage();

    auto corpus = rag::index::Corpus::load(args[0]);

    if (!corpus) {
        std::printf(
            "open error: %s\n",
            corpus.error().message.c_str());
        return 1;
    }

    bool has_embeddings = false;

    for (std::size_t i = 0;
         i < corpus->chunk_count();
         ++i) {

        auto ch = corpus->chunk(
            rag::ChunkId{
                static_cast<std::uint32_t>(i)
            });

        if (ch && !ch->embedding.empty()) {
            has_embeddings = true;
            break;
        }
    }

    std::printf(
        "%s\n"
        "  documents: %zu (live %zu)\n"
        "  chunks:    %zu\n"
        "  embeddings: %s\n"
        "  backend:   Ollama / nomic-embed-text\n",
        args[0].c_str(),
        corpus->document_count(),
        corpus->live_document_count(),
        corpus->chunk_count(),
        has_embeddings ? "yes" : "no");

    return 0;
}

// List every registered backend and the config keys it takes — the user-facing
// face of the plugin registry's describe(). Answers "what can I put in a config?"
// without reading source. A plugin dir can be loaded first so third-party names
// show up too: `ragcpp list embedders --plugins=./plugins`.
int cmd_list(const std::vector<std::string>& args) {
    rag::plugin::ensure_builtins_registered();
    if (std::string dir = opt(args, "--plugins", ""); !dir.empty())
        (void)rag::plugin::load_plugin_dir(dir);

    std::string which = args.empty() || args[0].rfind("--", 0) == 0 ? "" : args[0];
    auto dump = [](const char* title, auto&& rows) {
        std::printf("%s:\n", title);
        for (const auto& [name, desc] : rows)
            std::printf("  %-14s %s\n", name.c_str(), desc.empty() ? "(no description)" : desc.c_str());
    };
    if (which.empty() || which == "embedders")
        dump("embedders", rag::plugin::Registry<rag::plugin::AnyEmbedder>::instance().describe());
    if (which.empty() || which == "rerankers")
        dump("rerankers", rag::plugin::Registry<rag::plugin::AnyReranker>::instance().describe());
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) return usage();
    std::string cmd = argv[1];
    std::vector<std::string> args(argv + 2, argv + argc);
    if (cmd == "index") return cmd_index(args);
    if (cmd == "query") return cmd_query(args);
    if (cmd == "serve") return cmd_serve(args);
    if (cmd == "eval")  return cmd_eval(args);
    if (cmd == "info")  return cmd_info(args);
    if (cmd == "list")  return cmd_list(args);
    return usage();
}
