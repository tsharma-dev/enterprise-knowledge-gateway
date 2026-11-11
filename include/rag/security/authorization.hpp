#pragma once

#include <string>

#include <rag/core/document.hpp>
#include <rag/index/corpus.hpp>

namespace rag::security {

struct UserContext {
    std::string user_id;
    std::string department;
    std::string role;
};

class AuthorizationPolicy {
public:
    explicit AuthorizationPolicy(UserContext user);

    [[nodiscard]]
    index::MetaFilter filter() const;

private:
    UserContext user_;
};

} // namespace rag::security