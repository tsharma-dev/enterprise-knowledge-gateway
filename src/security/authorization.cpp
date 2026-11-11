#include <rag/security/authorization.hpp>

#include <utility>

namespace rag::security {

AuthorizationPolicy::AuthorizationPolicy(UserContext user)
    : user_(std::move(user)) {}

index::MetaFilter AuthorizationPolicy::filter() const {
    const UserContext user = user_;

    return [user](const Metadata& meta) -> bool {
        const auto get = [&meta](const char* key) -> std::string {
            const auto it = meta.find(key);

            if (it == meta.end())
                return {};

            return it->second;
        };

        const std::string department = get("department");
        const std::string role = get("role");

        // Public documents are visible to everyone.
        if (department == "public")
            return true;

        // Private documents must belong to the user's department.
        if (department != user.department)
            return false;

        // "any" means all users in the department can access it.
        if (role.empty() || role == "any")
            return true;

        // Otherwise the user's role must match.
        return role == user.role;
    };
}

} // namespace rag::security