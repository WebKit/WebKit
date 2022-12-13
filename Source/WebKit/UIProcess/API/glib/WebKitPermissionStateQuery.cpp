/*
 * Copyright (C) 2022 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "WebKitPermissionStateQuery.h"

#include "APISecurityOrigin.h"
#include "WebKitPermissionStateQueryPrivate.h"
#include "WebKitSecurityOriginPrivate.h"
#include <wtf/glib/WTFGType.h>

/**
 * WebKitPermissionStateQuery:
 * @See_also: #WebKitWebView
 *
 * This query represents a user's choice to allow or deny access to "powerful features" of the
 * platform, as specified in the [Permissions W3C
 * Specification](https://w3c.github.io/permissions/).
 *
 * When signalled by the #WebKitWebView through the `query-permission-state` signal, the application
 * has to eventually respond, via `webkit_permission_state_query_finish()`, whether it grants,
 * denies or requests a dedicated permission prompt for the given query.
 *
 * When a #WebKitPermissionStateQuery is not handled by the user, the user-agent is instructed to
 * `prompt` the user for the given permission.
 */

struct _WebKitPermissionStateQuery {
    explicit _WebKitPermissionStateQuery(const WTF::String& permissionName, API::SecurityOrigin& origin, CompletionHandler<void(std::optional<WebCore::PermissionState>)>&& completionHandler)
        : permissionName(permissionName.utf8())
        , securityOrigin(webkitSecurityOriginCreate(origin.securityOrigin().isolatedCopy()))
        , completionHandler(WTFMove(completionHandler))
    {
    }

    ~_WebKitPermissionStateQuery()
    {
        // Fallback to Prompt response unless the completion handler was already called.
        if (completionHandler)
            completionHandler(WebCore::PermissionState::Prompt);

        webkit_security_origin_unref(securityOrigin);
    }

    CString permissionName;
    WebKitSecurityOrigin* securityOrigin;
    CompletionHandler<void(std::optional<WebCore::PermissionState>)> completionHandler;
    int referenceCount { 1 };
};

G_DEFINE_BOXED_TYPE(WebKitPermissionStateQuery, webkit_permission_state_query, webkit_permission_state_query_ref, webkit_permission_state_query_unref)

WebKitPermissionStateQuery* webkitPermissionStateQueryCreate(const WTF::String& permissionName, API::SecurityOrigin& origin, CompletionHandler<void(std::optional<WebCore::PermissionState>)>&& completionHandler)
{
    WebKitPermissionStateQuery* query = static_cast<WebKitPermissionStateQuery*>(fastMalloc(sizeof(WebKitPermissionStateQuery)));
    new (query) WebKitPermissionStateQuery(permissionName, origin, WTFMove(completionHandler));
    return query;
}

/**
 * webkit_permission_state_query_ref:
 * @query: a #WebKitPermissionStateQuery
 *
 * Atomically increments the reference count of @query by one.
 *
 * This function is MT-safe and may be called from any thread.
 *
 * Returns: The passed #WebKitPermissionStateQuery
 *
 * Since: 2.40
 */
WebKitPermissionStateQuery* webkit_permission_state_query_ref(WebKitPermissionStateQuery* query)
{
    g_return_val_if_fail(query, nullptr);

    g_atomic_int_inc(&query->referenceCount);
    return query;
}

/**
 * webkit_permission_state_query_unref:
 * @query: a #WebKitPermissionStateQuery
 *
 * Atomically decrements the reference count of @query by one.
 *
 * If the reference count drops to 0, all memory allocated by #WebKitPermissionStateQuery is
 * released. This function is MT-safe and may be called from any thread.
 *
 * Since: 2.40
 */
void webkit_permission_state_query_unref(WebKitPermissionStateQuery* query)
{
    g_return_if_fail(query);

    if (g_atomic_int_dec_and_test(&query->referenceCount)) {
        query->~WebKitPermissionStateQuery();
        fastFree(query);
    }
}

/**
 * webkit_permission_state_query_get_name:
 * @query: a #WebKitPermissionStateQuery
 *
 * Get the permission name for which access is being queried.
 *
 * Returns: the permission name for @query
 *
 * Since: 2.40
 */
const gchar*
webkit_permission_state_query_get_name(WebKitPermissionStateQuery* query)
{
    g_return_val_if_fail(query, nullptr);

    return query->permissionName.data();
}

/**
 * webkit_permission_state_query_get_security_origin:
 * @query: a #WebKitPermissionStateQuery
 *
 * Get the permission origin for which access is being queried.
 *
 * Returns: (transfer none): A #WebKitSecurityOrigin representing the origin from which the
 * @query was emitted.
 *
 * Since: 2.40
 */
WebKitSecurityOrigin *
webkit_permission_state_query_get_security_origin(WebKitPermissionStateQuery* query)
{
    g_return_val_if_fail(query, nullptr);

    return query->securityOrigin;
}

/**
 * webkit_permission_state_query_finish:
 * @query: a #WebKitPermissionStateQuery
 * @state: a #WebKitPermissionState
 *
 * Notify the web-engine of the selected permission state for the given query. This function should
 * only be called as a response to the `WebKitWebView::query-permission-state` signal.
 *
 * Since: 2.40
 */
void
webkit_permission_state_query_finish(WebKitPermissionStateQuery* query, WebKitPermissionState state)
{
    g_return_if_fail(query);
    g_return_if_fail(query->completionHandler);

    switch (state) {
    case WEBKIT_PERMISSION_STATE_GRANTED:
        query->completionHandler(WebCore::PermissionState::Granted);
        break;
    case WEBKIT_PERMISSION_STATE_DENIED:
        query->completionHandler(WebCore::PermissionState::Denied);
        break;
    case WEBKIT_PERMISSION_STATE_PROMPT:
        query->completionHandler(WebCore::PermissionState::Prompt);
        break;
    }
}
