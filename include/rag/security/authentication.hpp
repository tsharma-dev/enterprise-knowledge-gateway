#pragma once

#include <optional>
#include <string>

#include <libpq-fe.h>

namespace rag::security {

struct AuthenticatedUser {
    std::string user_id;
    std::string department;
    std::string role;
    std::string token;
};

class AuthenticationService {
public:
    AuthenticationService();
    ~AuthenticationService();

    AuthenticationService(const AuthenticationService&) = delete;
    AuthenticationService& operator=(const AuthenticationService&) = delete;

    [[nodiscard]]
    std::optional<AuthenticatedUser> login(
        const std::string& username,
        const std::string& password) const;

    [[nodiscard]]
    std::optional<AuthenticatedUser> authenticate(
        const std::string& token) const;

private:
    PGconn* connection_;
};

} // namespace rag::security