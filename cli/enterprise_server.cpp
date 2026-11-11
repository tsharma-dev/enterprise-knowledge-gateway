#include <winsock2.h>
#include <ws2tcpip.h>

#include <rag/engine.hpp>
#include <rag/security/authorization.hpp>
#include <rag/security/authentication.hpp>

#include <nlohmann/json.hpp>

#include <cstdint>
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_set>

namespace {

constexpr std::uint16_t PORT = 8000;
constexpr int BUFFER_SIZE = 8192;


// ============================================================
// Send HTTP response
// ============================================================

void send_response(
    SOCKET client,
    int status_code,
    const std::string& status,
    const std::string& content_type,
    const std::string& body) {

    std::ostringstream response;

    response
        << "HTTP/1.1 " << status_code << " " << status << "\r\n"
        << "Content-Type: " << content_type << "\r\n"
        << "Content-Length: " << body.size() << "\r\n"
        << "Access-Control-Allow-Origin: *\r\n"
        << "Connection: close\r\n"
        << "\r\n"
        << body;

    const std::string data = response.str();

    send(
        client,
        data.c_str(),
        static_cast<int>(data.size()),
        0);
}


// ============================================================
// JSON error helper
// ============================================================

std::string json_error(
    const std::string& message) {

    nlohmann::json body;

    body["error"] = message;

    return body.dump(2);
}


// ============================================================
// Get HTTP header value
// ============================================================

std::string get_header(
    const std::string& headers,
    const std::string& name) {

    std::istringstream stream(headers);

    std::string line;

    const std::string prefix =
        name + ":";

    while (std::getline(stream, line)) {

        if (
            !line.empty() &&
            line.back() == '\r') {

            line.pop_back();
        }

        if (
            line.rfind(prefix, 0) == 0) {

            std::string value =
                line.substr(prefix.size());

            // Remove leading spaces.
            const std::size_t first =
                value.find_first_not_of(" \t");

            if (first == std::string::npos) {
                return {};
            }

            return value.substr(first);
        }
    }

    return {};
}


// ============================================================
// Serve frontend static files
// ============================================================

bool serve_static_file(
    SOCKET client,
    const std::string& path) {

    std::string file_path;
    std::string content_type;


    // --------------------------------------------------------
    // Allowed frontend files
    // --------------------------------------------------------

    if (path == "/") {

        file_path =
            "C:\\Users\\hp\\rag-cpp\\web\\index.html";

        content_type =
            "text/html; charset=utf-8";
    }
    else if (path == "/style.css") {

        file_path =
            "C:\\Users\\hp\\rag-cpp\\web\\style.css";

        content_type =
            "text/css; charset=utf-8";
    }
    else if (path == "/app.js") {

        file_path =
            "C:\\Users\\hp\\rag-cpp\\web\\app.js";

        content_type =
            "application/javascript; charset=utf-8";
    }
    else {

        return false;
    }


    // --------------------------------------------------------
    // Open file
    // --------------------------------------------------------

    std::ifstream file(
        file_path,
        std::ios::binary);


    if (!file) {

        send_response(
            client,
            404,
            "Not Found",
            "application/json",
            json_error(
                "Frontend file not found: " +
                file_path));

        return true;
    }


    // --------------------------------------------------------
    // Read file
    // --------------------------------------------------------

    std::ostringstream contents;

    contents << file.rdbuf();

    const std::string body =
        contents.str();


    // --------------------------------------------------------
    // Send file
    // --------------------------------------------------------

    send_response(
        client,
        200,
        "OK",
        content_type,
        body);

    return true;
}


// ============================================================
// Handle HTTP request
// ============================================================

void handle_request(
    SOCKET client,
    rag::Engine& engine,
    const rag::security::AuthenticationService& auth_service) {

    std::string request;

    char buffer[BUFFER_SIZE];


    // --------------------------------------------------------
    // Read until complete HTTP headers
    // --------------------------------------------------------

    while (
        request.find("\r\n\r\n") ==
        std::string::npos) {

        const int received =
            recv(
                client,
                buffer,
                sizeof(buffer),
                0);


        if (received <= 0) {
            return;
        }


        request.append(
            buffer,
            received);
    }


    // --------------------------------------------------------
    // Parse request line
    // --------------------------------------------------------

    std::istringstream request_stream(
        request);

    std::string method;
    std::string path;
    std::string version;


    request_stream
        >> method
        >> path
        >> version;


    // --------------------------------------------------------
    // Find header/body boundary
    // --------------------------------------------------------

    const std::size_t header_end =
        request.find("\r\n\r\n");


    if (header_end ==
        std::string::npos) {

        send_response(
            client,
            400,
            "Bad Request",
            "application/json",
            json_error(
                "Invalid HTTP request"));

        return;
    }


    const std::size_t body_start =
        header_end + 4;


    // --------------------------------------------------------
    // Read Content-Length
    // --------------------------------------------------------

    std::size_t content_length = 0;


    const std::string headers =
        request.substr(
            0,
            header_end);


    std::istringstream header_stream(
        headers);

    std::string header_line;


    while (
        std::getline(
            header_stream,
            header_line)) {

        if (
            !header_line.empty() &&
            header_line.back() == '\r') {

            header_line.pop_back();
        }


        const std::string prefix =
            "Content-Length:";


        if (
            header_line.rfind(
                prefix,
                0) == 0) {

            content_length =
                static_cast<std::size_t>(
                    std::stoull(
                        header_line.substr(
                            prefix.size())));

            break;
        }
    }


    // --------------------------------------------------------
    // Receive remaining request body
    // --------------------------------------------------------

    while (
        request.size() - body_start <
        content_length) {

        const int received =
            recv(
                client,
                buffer,
                sizeof(buffer),
                0);


        if (received <= 0) {
            return;
        }


        request.append(
            buffer,
            received);
    }


    const std::string body =
        request.substr(
            body_start,
            content_length);


    // ========================================================
    // Login endpoint
    // ========================================================

    if (
        method == "POST" &&
        path == "/login") {

        try {

            const auto input =
                nlohmann::json::parse(body);

            // ------------------------------------------------
            // Validate request
            // ------------------------------------------------

            if (
                !input.contains("username") ||
                !input.contains("password")) {

                send_response(
                    client,
                    400,
                    "Bad Request",
                    "application/json",
                    json_error(
                        "username and password are required"));

                return;
            }

            // ------------------------------------------------
            // Extract credentials
            // ------------------------------------------------

            const std::string username =
                input.at("username")
                    .get<std::string>();

            const std::string password =
                input.at("password")
                    .get<std::string>();

            // ------------------------------------------------
            // Authenticate
            // ------------------------------------------------

            const auto authenticated =
                auth_service.login(
                    username,
                    password);

            if (!authenticated) {

                send_response(
                    client,
                    401,
                    "Unauthorized",
                    "application/json",
                    json_error(
                        "Invalid username or password"));

                return;
            }

            // ------------------------------------------------
            // Build response
            // ------------------------------------------------

            nlohmann::json response;

            response["status"] =
                "authenticated";

            response["token"] =
                authenticated->token;

            response["user"] =
                authenticated->user_id;

            response["department"] =
                authenticated->department;

            response["role"] =
                authenticated->role;

            send_response(
                client,
                200,
                "OK",
                "application/json",
                response.dump(2));

            return;
        }
        catch (
            const std::exception& e) {

            send_response(
                client,
                400,
                "Bad Request",
                "application/json",
                json_error(
                    e.what()));

            return;
        }
    }


    // ========================================================
    // Health endpoint
    // ========================================================

    if (
        method == "GET" &&
        path == "/health") {

        nlohmann::json response;

        response["status"] =
            "ok";

        response["service"] =
            "enterprise-knowledge-gateway";


        send_response(
            client,
            200,
            "OK",
            "application/json",
            response.dump(2));

        return;
    }


    // ========================================================
    // Frontend
    // ========================================================

    if (method == "GET") {

        if (
            serve_static_file(
                client,
                path)) {

            return;
        }
    }


    // ========================================================
    // Query endpoint
    // ========================================================

    if (
        method == "POST" &&
        path == "/query") {

        try {

            const auto input =
                nlohmann::json::parse(
                    body);


            // ------------------------------------------------
            // Validate request
            // ------------------------------------------------

            if (!input.contains("query")) {

                send_response(
                    client,
                    400,
                    "Bad Request",
                    "application/json",
                    json_error(
                        "query is required"));

                return;
            }


            // ------------------------------------------------
            // Extract query
            // ------------------------------------------------

            const std::string query =
                input.at("query")
                    .get<std::string>();


            // ------------------------------------------------
            // Authenticate request
            // ------------------------------------------------

            const std::string authorization_header =
                get_header(
                    headers,
                    "Authorization");


            if (authorization_header.empty()) {

                send_response(
                    client,
                    401,
                    "Unauthorized",
                    "application/json",
                    json_error(
                        "Authorization header is required"));

                return;
            }


            const std::string bearer_prefix =
                "Bearer ";


            if (
                authorization_header.rfind(
                    bearer_prefix,
                    0) != 0) {

                send_response(
                    client,
                    401,
                    "Unauthorized",
                    "application/json",
                    json_error(
                        "Authorization must use Bearer token"));

                return;
            }


            const std::string token =
                authorization_header.substr(
                    bearer_prefix.size());


            const auto authenticated =
                auth_service.authenticate(
                    token);


            if (!authenticated) {

                send_response(
                    client,
                    401,
                    "Unauthorized",
                    "application/json",
                    json_error(
                        "Invalid authentication token"));

                return;
            }


            // ------------------------------------------------
            // Identity comes from authenticated token
            // ------------------------------------------------

            const std::string user_id =
                authenticated->user_id;

            const std::string department =
                authenticated->department;

            const std::string role =
                authenticated->role;


            // ------------------------------------------------
            // Number of results
            // ------------------------------------------------

            std::size_t k = 5;


            if (
                input.contains("k")) {

                k =
                    input.at("k")
                        .get<std::size_t>();
            }


            if (k == 0) {
                k = 1;
            }


            if (k > 20) {
                k = 20;
            }


            // =================================================
            // Authorization
            // =================================================

            rag::security::UserContext user{
                user_id,
                department,
                role
            };


            rag::security::AuthorizationPolicy policy(
                user);


            auto authorization_filter =
                policy.filter();


            // =================================================
            // Retrieval
            // =================================================

            auto results =
                engine.search(
                    query,
                    k,
                    authorization_filter);


            if (!results) {

                send_response(
                    client,
                    500,
                    "Internal Server Error",
                    "application/json",
                    json_error(
                        "Retrieval failed"));

                return;
            }


            // =================================================
            // Build JSON response
            // =================================================

            nlohmann::json response;


            response["query"] =
                query;


            response["user"] =
                user_id;


            response["department"] =
                department;


            response["role"] =
                role;


            response["results"] =
                nlohmann::json::array();


            // ------------------------------------------------
            // Add retrieved results
            // ------------------------------------------------

            for (
                const auto& result :
                *results) {

                nlohmann::json item;


                item["score"] =
                    result.score.get();


                item["text"] =
                    result.text;


                item["citation"] = {
                    {
                        "source",
                        result.uri
                    },
                    {
                        "chunk",
                        static_cast<unsigned long long>(
                            result.chunk.get())
                    }
                };


                if (
                    result.start_line != 0 ||
                    result.end_line != 0) {

                    item["citation"]["startLine"] =
                        result.start_line;


                    item["citation"]["endLine"] =
                        result.end_line;
                }


                response["results"]
                    .push_back(
                        std::move(item));
            }


            // ------------------------------------------------
            // Send response
            // ------------------------------------------------

            send_response(
                client,
                200,
                "OK",
                "application/json",
                response.dump(2));


            return;

        }
        catch (
            const std::exception& e) {

            send_response(
                client,
                400,
                "Bad Request",
                "application/json",
                json_error(
                    e.what()));

            return;
        }
    }


    // ========================================================
    // Retrieval comparison endpoint (Dense HNSW, BM25, explicit RRF)
    // ========================================================

    if (method == "POST" && path == "/compare") {
        try {
            const auto input = nlohmann::json::parse(body);
            if (!input.contains("query") || !input.at("query").is_string()) {
                send_response(client, 400, "Bad Request", "application/json",
                              json_error("query must be a string"));
                return;
            }
            const std::string authorization_header = get_header(headers, "Authorization");
            constexpr std::string_view bearer_prefix = "Bearer ";
            if (authorization_header.rfind(bearer_prefix, 0) != 0) {
                send_response(client, 401, "Unauthorized", "application/json",
                              json_error("Authorization must use Bearer token"));
                return;
            }
            const auto authenticated = auth_service.authenticate(
                authorization_header.substr(bearer_prefix.size()));
            if (!authenticated) {
                send_response(client, 401, "Unauthorized", "application/json",
                              json_error("Invalid authentication token"));
                return;
            }
            if (authenticated->role != "admin") {
                send_response(client, 403, "Forbidden", "application/json",
                              json_error("Administrator access is required"));
                return;
            }

            if (!engine.corpus().has_embedder()) {
                send_response(client, 503, "Service Unavailable", "application/json",
                              json_error("Dense comparison requires a configured embedding model"));
                return;
            }

            const std::string query = input.at("query").get<std::string>();
            std::size_t k = input.value("k", std::size_t{5});
            k = std::clamp<std::size_t>(k, 1, 20);
            const std::size_t pool = std::max<std::size_t>(60, k * 4);
            rag::security::UserContext user{authenticated->user_id,
                                            authenticated->department,
                                            authenticated->role};
            const auto filter = rag::security::AuthorizationPolicy(user).filter();

            using Clock = std::chrono::steady_clock;
            auto start = Clock::now();
            auto bm25 = engine.corpus().lexical_search(query, pool, filter);
            const double bm25_ms = std::chrono::duration<double, std::milli>(Clock::now() - start).count();

            start = Clock::now();
            auto dense = engine.corpus().dense_search(query, pool, filter);
            const double dense_ms = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
            if (!dense) {
                send_response(client, 503, "Service Unavailable", "application/json",
                              json_error("Dense retrieval failed"));
                return;
            }

            std::vector<rag::fusion::RankedList> lists;
            start = Clock::now();
            auto hybrid_bm25 = engine.corpus().lexical_search(query, pool, filter);
            auto hybrid_dense = engine.corpus().dense_search(query, pool, filter);
            if (!hybrid_dense) {
                send_response(client, 503, "Service Unavailable", "application/json",
                              json_error("Dense retrieval failed during hybrid search"));
                return;
            }
            lists.push_back(rag::fusion::bm25_list(std::move(hybrid_bm25)));
            lists.push_back(rag::fusion::cosine_list(std::move(*hybrid_dense)));
            rag::fusion::RrfParams rrf_params;
            rrf_params.k = 60.0f;
            auto hybrid = rag::fusion::rrf(lists, rrf_params, k);
            const double hybrid_ms = std::chrono::duration<double, std::milli>(Clock::now() - start).count();

            const auto lane_json = [&engine, k](const std::vector<rag::Hit>& hits,
                                                 const char* score_type, double latency_ms) {
                nlohmann::json lane;
                lane["scoreType"] = score_type;
                lane["latencyMs"] = latency_ms;
                lane["results"] = nlohmann::json::array();
                const std::size_t count = std::min(k, hits.size());
                for (std::size_t i = 0; i < count; ++i) {
                    const auto result = engine.corpus().resolve(hits[i]);
                    nlohmann::json item = {
                        {"rank", i + 1}, {"score", hits[i].score.get()},
                        {"text", result.text},
                        {"citation", {{"source", result.uri},
                                       {"chunk", static_cast<unsigned long long>(result.chunk.get())}}}
                    };
                    lane["results"].push_back(std::move(item));
                }
                return lane;
            };

            nlohmann::json response = {
                {"query", query}, {"rrfK", rrf_params.k},
                {"denseIndex", engine.corpus().uses_hnsw() ? "HNSW" : "exact cosine"},
                {"authorization", "authenticated-user-filter-applied-to-each-retriever"},
                {"strategies", {
                    {"bm25", lane_json(bm25, "BM25", bm25_ms)},
                    {"dense", lane_json(*dense, "cosine similarity", dense_ms)},
                    {"hybrid", lane_json(hybrid, "RRF score", hybrid_ms)}
                }}
            };

            std::unordered_set<unsigned long long> bm25_ids, dense_ids, hybrid_ids;
            for (std::size_t i = 0; i < std::min(k, bm25.size()); ++i) bm25_ids.insert(bm25[i].chunk.get());
            for (std::size_t i = 0; i < std::min(k, dense->size()); ++i) dense_ids.insert((*dense)[i].chunk.get());
            for (const auto& hit : hybrid) hybrid_ids.insert(hit.chunk.get());
            nlohmann::json overlap = nlohmann::json::array();
            for (auto id : bm25_ids) if (dense_ids.count(id) && hybrid_ids.count(id)) overlap.push_back(id);
            response["commonChunkIds"] = std::move(overlap);
            response["overlapCounts"] = {
                {"bm25Dense", std::count_if(bm25_ids.begin(), bm25_ids.end(), [&dense_ids](auto id) { return dense_ids.count(id) != 0; })},
                {"bm25Hybrid", std::count_if(bm25_ids.begin(), bm25_ids.end(), [&hybrid_ids](auto id) { return hybrid_ids.count(id) != 0; })},
                {"denseHybrid", std::count_if(dense_ids.begin(), dense_ids.end(), [&hybrid_ids](auto id) { return hybrid_ids.count(id) != 0; })}
            };
            response["uniqueChunkIds"] = {
                {"bm25", nlohmann::json::array()}, {"dense", nlohmann::json::array()},
                {"hybrid", nlohmann::json::array()}
            };
            for (auto id : bm25_ids) if (!dense_ids.count(id) && !hybrid_ids.count(id)) response["uniqueChunkIds"]["bm25"].push_back(id);
            for (auto id : dense_ids) if (!bm25_ids.count(id) && !hybrid_ids.count(id)) response["uniqueChunkIds"]["dense"].push_back(id);
            for (auto id : hybrid_ids) if (!bm25_ids.count(id) && !dense_ids.count(id)) response["uniqueChunkIds"]["hybrid"].push_back(id);
            send_response(client, 200, "OK", "application/json", response.dump(2));
            return;
        } catch (const std::exception& e) {
            send_response(client, 400, "Bad Request", "application/json", json_error(e.what()));
            return;
        }
    }

    // ========================================================
    // Admin document ingestion endpoint
    // ========================================================

    if (
        method == "POST" &&
        path == "/documents") {

        try {

            // ------------------------------------------------
            // Authenticate request
            // ------------------------------------------------

            const std::string authorization_header =
                get_header(
                    headers,
                    "Authorization");


            if (authorization_header.empty()) {

                send_response(
                    client,
                    401,
                    "Unauthorized",
                    "application/json",
                    json_error(
                        "Authorization header is required"));

                return;
            }


            const std::string bearer_prefix =
                "Bearer ";


            if (
                authorization_header.rfind(
                    bearer_prefix,
                    0) != 0) {

                send_response(
                    client,
                    401,
                    "Unauthorized",
                    "application/json",
                    json_error(
                        "Authorization must use Bearer token"));

                return;
            }


            const std::string token =
                authorization_header.substr(
                    bearer_prefix.size());


            const auto authenticated =
                auth_service.authenticate(
                    token);


            if (!authenticated) {

                send_response(
                    client,
                    401,
                    "Unauthorized",
                    "application/json",
                    json_error(
                        "Invalid authentication token"));

                return;
            }


            // ------------------------------------------------
            // Only administrators can upload documents
            // ------------------------------------------------

            if (authenticated->role != "admin") {

                send_response(
                    client,
                    403,
                    "Forbidden",
                    "application/json",
                    json_error(
                        "Administrator access required"));

                return;
            }


            // ------------------------------------------------
            // Parse document request
            // ------------------------------------------------

            const auto input =
                nlohmann::json::parse(body);


            // ------------------------------------------------
            // Validate request
            // ------------------------------------------------

            if (
                !input.contains("filename") ||
                !input.contains("content") ||
                !input.contains("department") ||
                !input.contains("role")) {

                send_response(
                    client,
                    400,
                    "Bad Request",
                    "application/json",
                    json_error(
                        "filename, content, department and role are required"));

                return;
            }


            // ------------------------------------------------
            // Extract fields
            // ------------------------------------------------

            const std::string filename =
                input.at("filename")
                    .get<std::string>();


            const std::string content =
                input.at("content")
                    .get<std::string>();


            const std::string department =
                input.at("department")
                    .get<std::string>();


            const std::string role =
                input.at("role")
                    .get<std::string>();


            // ------------------------------------------------
            // Basic validation
            // ------------------------------------------------

            if (
                filename.empty() ||
                content.empty() ||
                department.empty() ||
                role.empty()) {

                send_response(
                    client,
                    400,
                    "Bad Request",
                    "application/json",
                    json_error(
                        "filename, content, department and role cannot be empty"));

                return;
            }


            // ------------------------------------------------
            // Build metadata
            // ------------------------------------------------

            rag::Metadata metadata;

            metadata["department"] =
                department;

            metadata["role"] =
                role;


            // ------------------------------------------------
            // Use filename as stable document URI
            // ------------------------------------------------

            const std::string uri =
                "uploads/" + filename;


            // ------------------------------------------------
            // Upsert document
            // ------------------------------------------------

            auto result =
                engine.corpus().upsert_document(
                    uri,
                    content,
                    metadata,
                    filename);


            if (!result) {

                send_response(
                    client,
                    500,
                    "Internal Server Error",
                    "application/json",
                    json_error(
                        "Failed to add document"));

                return;
            }


            // ------------------------------------------------
            // Rebuild retrieval indexes
            // ------------------------------------------------

            auto build_result =
                engine.build();


            if (!build_result) {

                send_response(
                    client,
                    500,
                    "Internal Server Error",
                    "application/json",
                    json_error(
                        "Document added but index rebuild failed"));

                return;
            }


            // ------------------------------------------------
            // Persist database
            // ------------------------------------------------

            const std::string db_path =
                "C:\\Users\\hp\\rag-cpp\\enterprise_eval.ragdb";


            auto save_result =
                engine.save(
                    db_path);


            if (!save_result) {

                send_response(
                    client,
                    500,
                    "Internal Server Error",
                    "application/json",
                    json_error(
                        "Document indexed but database save failed"));

                return;
            }


            // ------------------------------------------------
            // Success response
            // ------------------------------------------------

            nlohmann::json response;

            response["status"] =
                "indexed";

            response["filename"] =
                filename;

            response["uri"] =
                uri;

            response["department"] =
                department;

            response["role"] =
                role;


            send_response(
                client,
                200,
                "OK",
                "application/json",
                response.dump(2));


            return;

        }
        catch (
            const std::exception& e) {

            send_response(
                client,
                400,
                "Bad Request",
                "application/json",
                json_error(
                    e.what()));

            return;
        }
    }


    // ========================================================
    // Not found
    // ========================================================

    send_response(
        client,
        404,
        "Not Found",
        "application/json",
        json_error(
            "Endpoint not found"));
}

} // namespace


// ============================================================
// Main
// ============================================================

int main() {

    std::cout
        << "========================================\n"
        << " Enterprise Knowledge Gateway\n"
        << "========================================\n\n";


    // ========================================================
    // Load database
    // ========================================================

    const std::string db_path =
        "C:\\Users\\hp\\rag-cpp\\enterprise_eval.ragdb";


    auto engine_result =
        rag::Engine::open(
            db_path);


    if (!engine_result) {

        std::cerr
            << "Failed to open database: "
            << db_path
            << "\n";

        return 1;
    }


    auto engine =
        std::move(
            *engine_result);


    // ========================================================
    // Attach Ollama embedder
    // ========================================================

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


    // ========================================================
    // Initialize Winsock
    // ========================================================

    WSADATA wsa_data{};


    const int wsa_result =
        WSAStartup(
            MAKEWORD(2, 2),
            &wsa_data);


    if (wsa_result != 0) {

        std::cerr
            << "WSAStartup failed: "
            << wsa_result
            << "\n";

        return 1;
    }


    // ========================================================
    // Create socket
    // ========================================================

    SOCKET server_socket =
        socket(
            AF_INET,
            SOCK_STREAM,
            IPPROTO_TCP);


    if (
        server_socket ==
        INVALID_SOCKET) {

        std::cerr
            << "Failed to create socket.\n";

        WSACleanup();

        return 1;
    }


    // ========================================================
    // Bind
    // ========================================================

    sockaddr_in server_address{};


    server_address.sin_family =
        AF_INET;


    server_address.sin_addr.s_addr =
        htonl(INADDR_ANY);


    server_address.sin_port =
        htons(PORT);


    if (
        bind(
            server_socket,
            reinterpret_cast<sockaddr*>(
                &server_address),
            sizeof(server_address))
        == SOCKET_ERROR) {

        std::cerr
            << "Failed to bind port "
            << PORT
            << ".\n";

        closesocket(
            server_socket);

        WSACleanup();

        return 1;
    }


    // ========================================================
    // Listen
    // ========================================================

    if (
        listen(
            server_socket,
            SOMAXCONN)
        == SOCKET_ERROR) {

        std::cerr
            << "Failed to listen.\n";

        closesocket(
            server_socket);

        WSACleanup();

        return 1;
    }


    // ========================================================
    // Startup information
    // ========================================================

    std::cout
        << "Server running on:\n"
        << "  http://localhost:"
        << PORT
        << "\n\n";


    std::cout
        << "Endpoints:\n"
        << "  GET  /\n"
        << "  POST /login\n"
        << "  GET  /health\n"
        << "  GET  /style.css\n"
        << "  GET  /app.js\n"
        << "  POST /query\n"
        << "  POST /compare  (admin; Dense + BM25 + RRF)\n"
        << "  POST /documents\n\n";


    // ========================================================
    // Accept requests
    // ========================================================

    rag::security::AuthenticationService auth_service;


    while (true) {

        sockaddr_in client_address{};


        int client_address_size =
            sizeof(client_address);


        SOCKET client =
            accept(
                server_socket,
                reinterpret_cast<sockaddr*>(
                    &client_address),
                &client_address_size);


        if (
            client ==
            INVALID_SOCKET) {

            std::cerr
                << "Failed to accept connection.\n";

            continue;
        }


        handle_request(
            client,
            engine,
            auth_service);


        closesocket(
            client);
    }


    // ========================================================
    // Cleanup
    // ========================================================

    closesocket(
        server_socket);

    WSACleanup();

    return 0;
}
