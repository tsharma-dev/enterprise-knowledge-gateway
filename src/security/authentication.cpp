#include <rag/security/authentication.hpp>

#include <cstdlib>
#include <stdexcept>

namespace rag::security {

namespace {

std::string get_environment_variable(
    const char* name,
    const char* default_value = "") {

    const char* value = std::getenv(name);

    if (value == nullptr) {
        return default_value;
    }

    return value;
}

} // namespace

AuthenticationService::AuthenticationService()
    : connection_(nullptr) {

    const std::string host =
        get_environment_variable("RAG_DB_HOST", "127.0.0.1");

    const std::string port =
        get_environment_variable("RAG_DB_PORT", "5432");

    const std::string database =
        get_environment_variable("RAG_DB_NAME", "knowledge_gateway");

    const std::string user =
        get_environment_variable("RAG_DB_USER", "postgres");

    const std::string password =
        get_environment_variable("RAG_DB_PASSWORD");

    if (password.empty()) {
        throw std::runtime_error(
            "RAG_DB_PASSWORD environment variable is not set");
    }

    const std::string connection_string =
        "host=" + host +
        " port=" + port +
        " dbname=" + database +
        " user=" + user +
        " password=" + password;

    connection_ =
        PQconnectdb(connection_string.c_str());

    if (connection_ == nullptr ||
        PQstatus(connection_) != CONNECTION_OK) {

        const std::string error =
            connection_ != nullptr
                ? PQerrorMessage(connection_)
                : "Unable to allocate PostgreSQL connection";

        if (connection_ != nullptr) {
            PQfinish(connection_);
            connection_ = nullptr;
        }

        throw std::runtime_error(
            "PostgreSQL connection failed: " + error);
    }
}

AuthenticationService::~AuthenticationService() {

    if (connection_ != nullptr) {
        PQfinish(connection_);
        connection_ = nullptr;
    }
}

std::optional<AuthenticatedUser>
AuthenticationService::login(
    const std::string& username,
    const std::string& password) const {

    if (connection_ == nullptr) {
        return std::nullopt;
    }

    const char* parameters[] = {
        username.c_str(),
        password.c_str()
    };

    PGresult* result = PQexecParams(
        connection_,
        R"SQL(
            SELECT username, department, role, token
            FROM users
            WHERE username = $1
              AND crypt($2, password_hash) = password_hash
            LIMIT 1
        )SQL",
        2,
        nullptr,
        parameters,
        nullptr,
        nullptr,
        0
    );

    if (result == nullptr ||
        PQresultStatus(result) != PGRES_TUPLES_OK) {

        if (result != nullptr) {
            PQclear(result);
        }

        return std::nullopt;
    }

    std::optional<AuthenticatedUser> authenticated;

    if (PQntuples(result) == 1) {

        authenticated = AuthenticatedUser{
            PQgetvalue(result, 0, 0),
            PQgetvalue(result, 0, 1),
            PQgetvalue(result, 0, 2),
            PQgetvalue(result, 0, 3)
        };
    }

    PQclear(result);

    return authenticated;
}

std::optional<AuthenticatedUser>
AuthenticationService::authenticate(
    const std::string& token) const {

    if (connection_ == nullptr) {
        return std::nullopt;
    }

    const char* parameters[] = {
        token.c_str()
    };

    PGresult* result = PQexecParams(
        connection_,
        R"SQL(
            SELECT username, department, role, token
            FROM users
            WHERE token = $1
            LIMIT 1
        )SQL",
        1,
        nullptr,
        parameters,
        nullptr,
        nullptr,
        0
    );

    if (result == nullptr ||
        PQresultStatus(result) != PGRES_TUPLES_OK) {

        if (result != nullptr) {
            PQclear(result);
        }

        return std::nullopt;
    }

    std::optional<AuthenticatedUser> authenticated;

    if (PQntuples(result) == 1) {

        authenticated = AuthenticatedUser{
            PQgetvalue(result, 0, 0),
            PQgetvalue(result, 0, 1),
            PQgetvalue(result, 0, 2),
            PQgetvalue(result, 0, 3)
        };
    }

    PQclear(result);

    return authenticated;
}

} // namespace rag::security