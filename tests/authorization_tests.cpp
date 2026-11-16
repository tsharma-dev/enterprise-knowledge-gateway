#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <iostream>

#include <rag/security/authorization.hpp>

int main() {
    using rag::Metadata;
    using rag::security::AuthorizationPolicy;
    using rag::security::UserContext;

    // ------------------------------------------------------------
    // Test 1: Engineering employee can access engineering documents
    // ------------------------------------------------------------
    {
        UserContext user{
            "alice",
            "engineering",
            "employee"
        };

        AuthorizationPolicy policy(user);
        auto filter = policy.filter();

        Metadata engineering_doc{
            {"department", "engineering"},
            {"role", "employee"}
        };

        assert(filter(engineering_doc));
    }

    // ------------------------------------------------------------
    // Test 2: Engineering employee cannot access HR documents
    // ------------------------------------------------------------
    {
        UserContext user{
            "alice",
            "engineering",
            "employee"
        };

        AuthorizationPolicy policy(user);
        auto filter = policy.filter();

        Metadata hr_doc{
            {"department", "hr"},
            {"role", "employee"}
        };

        assert(!filter(hr_doc));
    }

    // ------------------------------------------------------------
    // Test 3: HR employee can access HR documents
    // ------------------------------------------------------------
    {
        UserContext user{
            "bob",
            "hr",
            "employee"
        };

        AuthorizationPolicy policy(user);
        auto filter = policy.filter();

        Metadata hr_doc{
            {"department", "hr"},
            {"role", "employee"}
        };

        assert(filter(hr_doc));
    }

    // ------------------------------------------------------------
    // Test 4: HR employee cannot access Engineering documents
    // ------------------------------------------------------------
    {
        UserContext user{
            "bob",
            "hr",
            "employee"
        };

        AuthorizationPolicy policy(user);
        auto filter = policy.filter();

        Metadata engineering_doc{
            {"department", "engineering"},
            {"role", "employee"}
        };

        assert(!filter(engineering_doc));
    }

    // ------------------------------------------------------------
    // Test 5: Everyone can access public documents
    // ------------------------------------------------------------
    {
        UserContext user{
            "alice",
            "engineering",
            "employee"
        };

        AuthorizationPolicy policy(user);
        auto filter = policy.filter();

        Metadata public_doc{
            {"department", "public"},
            {"role", "any"}
        };

        assert(filter(public_doc));
    }

    // ------------------------------------------------------------
    // Test 6: Missing department must fail closed
    // ------------------------------------------------------------
    {
        UserContext user{
            "alice",
            "engineering",
            "employee"
        };

        AuthorizationPolicy policy(user);
        auto filter = policy.filter();

        Metadata unknown_doc{
            {"role", "employee"}
        };

        assert(!filter(unknown_doc));
    }

    // ------------------------------------------------------------
    // Test 7: Admin can read across departments in this demo
    // ------------------------------------------------------------
    {
        UserContext user{"admin", "admin", "admin"};
        AuthorizationPolicy policy(user);
        auto filter = policy.filter();

        Metadata hr_manager_doc{
            {"department", "hr"},
            {"role", "manager"}
        };
        Metadata engineering_doc{
            {"department", "engineering"},
            {"role", "employee"}
        };
        Metadata finance_doc{
            {"department", "finance"},
            {"role", "employee"}
        };

        assert(filter(hr_manager_doc));
        assert(filter(engineering_doc));
        assert(filter(finance_doc));
    }

    // ------------------------------------------------------------
    // Test 8: Engineering employee cannot read manager-only docs
    // ------------------------------------------------------------
    {
        UserContext user{"alice", "engineering", "employee"};
        AuthorizationPolicy policy(user);
        auto filter = policy.filter();

        Metadata engineering_manager_doc{
            {"department", "engineering"},
            {"role", "manager"}
        };

        assert(!filter(engineering_manager_doc));
    }
    std::cout << "All authorization tests passed.\n";

    return 0;
}