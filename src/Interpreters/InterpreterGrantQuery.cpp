#include <Interpreters/InterpreterGrantQuery.h>
#include <Parsers/ASTGrantQuery.h>
#include <Interpreters/Context.h>
#include <Access/AccessControlManager.h>
#include <Access/ContextAccess.h>
#include <Access/ExtendedRoleSet.h>
#include <Access/User.h>
#include <Access/Role.h>
#include <boost/range/algorithm/copy.hpp>


namespace DB
{
namespace
{
    template <typename T>
    void updateFromQueryImpl(T & grantee, const ASTGrantQuery & query, const std::vector<UUID> & roles_from_query, const String & current_database)
    {
        using Kind = ASTGrantQuery::Kind;
        if (!query.access_rights_elements.empty())
        {
            if (query.kind == Kind::GRANT)
            {
                grantee.access.grant(query.access_rights_elements, current_database);
                if (query.grant_option)
                    grantee.access_with_grant_option.grant(query.access_rights_elements, current_database);
            }
            else
            {
                grantee.access_with_grant_option.revoke(query.access_rights_elements, current_database);
                if (!query.grant_option)
                    grantee.access.revoke(query.access_rights_elements, current_database);
            }
        }

        if (!roles_from_query.empty())
        {
            if (query.kind == Kind::GRANT)
            {
                boost::range::copy(roles_from_query, std::inserter(grantee.granted_roles, grantee.granted_roles.end()));
                if (query.admin_option)
                    boost::range::copy(roles_from_query, std::inserter(grantee.granted_roles_with_admin_option, grantee.granted_roles_with_admin_option.end()));
            }
            else
            {
                for (const UUID & role_from_query : roles_from_query)
                {
                    grantee.granted_roles_with_admin_option.erase(role_from_query);
                    if (!query.admin_option)
                        grantee.granted_roles.erase(role_from_query);
                    if constexpr (std::is_same_v<T, User>)
                        grantee.default_roles.ids.erase(role_from_query);
                }
            }
        }
    }
}


BlockIO InterpreterGrantQuery::execute()
{
    const auto & query = query_ptr->as<const ASTGrantQuery &>();
    auto & access_control = context.getAccessControlManager();
    auto access = context.getAccess();
    access->checkGrantOption(query.access_rights_elements);

    std::vector<UUID> roles_from_query;
    if (query.roles)
    {
        roles_from_query = ExtendedRoleSet{*query.roles, access_control}.getMatchingIDs(access_control);
        for (const UUID & role_from_query : roles_from_query)
            access->checkAdminOption(role_from_query);
    }

    std::vector<UUID> to_roles = ExtendedRoleSet{*query.to_roles, access_control, context.getUserID()}.getMatchingIDs(access_control);
    String current_database = context.getCurrentDatabase();

    auto update_func = [&](const AccessEntityPtr & entity) -> AccessEntityPtr
    {
        auto clone = entity->clone();
        if (auto user = typeid_cast<std::shared_ptr<User>>(clone))
        {
            updateFromQueryImpl(*user, query, roles_from_query, current_database);
            return user;
        }
        else if (auto role = typeid_cast<std::shared_ptr<Role>>(clone))
        {
            updateFromQueryImpl(*role, query, roles_from_query, current_database);
            return role;
        }
        else
            return entity;
    };

    access_control.update(to_roles, update_func);

    return {};
}


void InterpreterGrantQuery::updateUserFromQuery(User & user, const ASTGrantQuery & query)
{
    std::vector<UUID> roles_from_query;
    if (query.roles)
        roles_from_query = ExtendedRoleSet{*query.roles}.getMatchingIDs();
    updateFromQueryImpl(user, query, roles_from_query, {});
}


void InterpreterGrantQuery::updateRoleFromQuery(Role & role, const ASTGrantQuery & query)
{
    std::vector<UUID> roles_from_query;
    if (query.roles)
        roles_from_query = ExtendedRoleSet{*query.roles}.getMatchingIDs();
    updateFromQueryImpl(role, query, roles_from_query, {});
}

}
