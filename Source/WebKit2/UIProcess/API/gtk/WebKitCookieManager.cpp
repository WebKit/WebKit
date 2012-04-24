/*
 * Copyright (C) 2012 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2,1 of the License, or (at your option) any later version.
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
#include "WebKitCookieManager.h"

#include "WebKitCookieManagerPrivate.h"
#include "WebKitEnumTypes.h"
#include <wtf/gobject/GRefPtr.h>
#include <wtf/text/CString.h>

using namespace WebKit;

enum {
    CHANGED,

    LAST_SIGNAL
};

struct _WebKitCookieManagerPrivate {
    WKRetainPtr<WKCookieManagerRef> wkCookieManager;
};

static guint signals[LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE(WebKitCookieManager, webkit_cookie_manager, G_TYPE_OBJECT)

COMPILE_ASSERT_MATCHING_ENUM(WEBKIT_COOKIE_POLICY_ACCEPT_ALWAYS, kWKHTTPCookieAcceptPolicyAlways);
COMPILE_ASSERT_MATCHING_ENUM(WEBKIT_COOKIE_POLICY_ACCEPT_NEVER, kWKHTTPCookieAcceptPolicyNever);
COMPILE_ASSERT_MATCHING_ENUM(WEBKIT_COOKIE_POLICY_ACCEPT_NO_THIRD_PARTY, kWKHTTPCookieAcceptPolicyOnlyFromMainDocumentDomain);

static void webkit_cookie_manager_init(WebKitCookieManager* manager)
{
    WebKitCookieManagerPrivate* priv = G_TYPE_INSTANCE_GET_PRIVATE(manager, WEBKIT_TYPE_COOKIE_MANAGER, WebKitCookieManagerPrivate);
    manager->priv = priv;
    new (priv) WebKitCookieManagerPrivate();
}

static void webkitCookieManagerFinalize(GObject* object)
{
    WebKitCookieManagerPrivate* priv = WEBKIT_COOKIE_MANAGER(object)->priv;
    WKCookieManagerStopObservingCookieChanges(priv->wkCookieManager.get());
    priv->~WebKitCookieManagerPrivate();
    G_OBJECT_CLASS(webkit_cookie_manager_parent_class)->finalize(object);
}

static void webkit_cookie_manager_class_init(WebKitCookieManagerClass* findClass)
{
    GObjectClass* gObjectClass = G_OBJECT_CLASS(findClass);
    gObjectClass->finalize = webkitCookieManagerFinalize;

    g_type_class_add_private(findClass, sizeof(WebKitCookieManagerPrivate));

    /**
     * WebKitCookieManager::changed:
     * @cookie_manager: the #WebKitCookieManager
     *
     * This signal is emitted when cookies are added, removed or modified.
     */
    signals[CHANGED] =
        g_signal_new("changed",
                     G_TYPE_FROM_CLASS(gObjectClass),
                     G_SIGNAL_RUN_LAST,
                     0, 0, 0,
                     g_cclosure_marshal_VOID__VOID,
                     G_TYPE_NONE, 0);
}

static void cookiesDidChange(WKCookieManagerRef, const void* clientInfo)
{
    g_signal_emit(WEBKIT_COOKIE_MANAGER(clientInfo), signals[CHANGED], 0);
}

WebKitCookieManager* webkitCookieManagerCreate(WKCookieManagerRef wkCookieManager)
{
    WebKitCookieManager* manager = WEBKIT_COOKIE_MANAGER(g_object_new(WEBKIT_TYPE_COOKIE_MANAGER, NULL));
    manager->priv->wkCookieManager = wkCookieManager;

    WKCookieManagerClient wkCookieManagerClient = {
        kWKCookieManagerClientCurrentVersion,
        manager, // clientInfo
        cookiesDidChange
    };
    WKCookieManagerSetClient(wkCookieManager, &wkCookieManagerClient);
    WKCookieManagerStartObservingCookieChanges(wkCookieManager);

    return manager;
}

/**
 * webkit_cookie_manager_set_accept_policy:
 * @cookie_manager: a #WebKitCookieManager
 * @policy: a #WebKitCookieAcceptPolicy
 *
 * Set the cookie acceptance policy of @cookie_manager as @policy.
 */
void webkit_cookie_manager_set_accept_policy(WebKitCookieManager* manager, WebKitCookieAcceptPolicy policy)
{
    g_return_if_fail(WEBKIT_IS_COOKIE_MANAGER(manager));

    WKCookieManagerSetHTTPCookieAcceptPolicy(manager->priv->wkCookieManager.get(), policy);
}

static void webkitCookieManagerGetAcceptPolicyCallback(WKHTTPCookieAcceptPolicy policy, WKErrorRef, void* context)
{
    GRefPtr<GSimpleAsyncResult> result = adoptGRef(G_SIMPLE_ASYNC_RESULT(context));
    g_simple_async_result_set_op_res_gpointer(result.get(), GUINT_TO_POINTER(policy), 0);
    g_simple_async_result_complete(result.get());
}

/**
 * webkit_cookie_manager_get_accept_policy:
 * @cookie_manager: a #WebKitCookieManager
 * @callback: (scope async): a #GAsyncReadyCallback to call when the request is satisfied
 * @user_data: (closure): the data to pass to callback function
 *
 * Asynchronously get the cookie acceptance policy of @cookie_manager.
 *
 * When the operation is finished, @callback will be called. You can then call
 * webkit_cookie_manager_get_accept_policy_finish() to get the result of the operation.
 */
void webkit_cookie_manager_get_accept_policy(WebKitCookieManager* manager, GAsyncReadyCallback callback, gpointer userData)
{
    g_return_if_fail(WEBKIT_IS_COOKIE_MANAGER(manager));

    GSimpleAsyncResult* result = g_simple_async_result_new(G_OBJECT(manager), callback, userData,
                                                           reinterpret_cast<gpointer>(webkit_cookie_manager_get_accept_policy));
    WKCookieManagerGetHTTPCookieAcceptPolicy(manager->priv->wkCookieManager.get(), result, webkitCookieManagerGetAcceptPolicyCallback);
}

/**
 * webkit_cookie_manager_get_accept_policy_finish:
 * @cookie_manager: a #WebKitCookieManager
 * @result: a #GAsyncResult
 * @error: return location for error or %NULL to ignore
 *
 * Finish an asynchronous operation started with webkit_cookie_manager_get_accept_policy().
 *
 * Returns: the cookie acceptance policy of @cookie_manager as a #WebKitCookieAcceptPolicy.
 */
WebKitCookieAcceptPolicy webkit_cookie_manager_get_accept_policy_finish(WebKitCookieManager* manager, GAsyncResult* result, GError** error)
{
    g_return_val_if_fail(WEBKIT_IS_COOKIE_MANAGER(manager), WEBKIT_COOKIE_POLICY_ACCEPT_NO_THIRD_PARTY);
    g_return_val_if_fail(G_IS_ASYNC_RESULT(result), WEBKIT_COOKIE_POLICY_ACCEPT_NO_THIRD_PARTY);

    GSimpleAsyncResult* simpleResult = G_SIMPLE_ASYNC_RESULT(result);
    g_warn_if_fail(g_simple_async_result_get_source_tag(simpleResult) == webkit_cookie_manager_get_accept_policy);

    if (g_simple_async_result_propagate_error(simpleResult, error))
        return WEBKIT_COOKIE_POLICY_ACCEPT_NO_THIRD_PARTY;

    return static_cast<WebKitCookieAcceptPolicy>(GPOINTER_TO_UINT(g_simple_async_result_get_op_res_gpointer(simpleResult)));
}

static void webkitCookieManagerGetDomainsWithCookiesCallback(WKArrayRef wkDomains, WKErrorRef, void* context)
{
    GRefPtr<GSimpleAsyncResult> result = adoptGRef(G_SIMPLE_ASYNC_RESULT(context));
    GPtrArray* domains = g_ptr_array_new_with_free_func(g_free);
    for (size_t i = 0; i < WKArrayGetSize(wkDomains); ++i) {
        WKStringRef wkDomain = static_cast<WKStringRef>(WKArrayGetItemAtIndex(wkDomains, i));
        String domain = toImpl(wkDomain)->string();
        if (domain.isEmpty())
            continue;
        g_ptr_array_add(domains, g_strdup(domain.utf8().data()));
    }
    g_ptr_array_add(domains, 0);
    g_simple_async_result_set_op_res_gpointer(result.get(), domains, reinterpret_cast<GDestroyNotify>(g_ptr_array_unref));
    g_simple_async_result_complete(result.get());
}

/**
 * webkit_cookie_manager_get_domains_with_cookies:
 * @cookie_manager: a #WebKitCookieManager
 * @callback: (scope async): a #GAsyncReadyCallback to call when the request is satisfied
 * @user_data: (closure): the data to pass to callback function
 *
 * Asynchronously get the list of domains for which @cookie_manager contains cookies.
 *
 * When the operation is finished, @callback will be called. You can then call
 * webkit_cookie_manager_get_domains_with_cookies_finish() to get the result of the operation.
 */
void webkit_cookie_manager_get_domains_with_cookies(WebKitCookieManager* manager, GAsyncReadyCallback callback, gpointer userData)
{
    g_return_if_fail(WEBKIT_IS_COOKIE_MANAGER(manager));

    GSimpleAsyncResult* result = g_simple_async_result_new(G_OBJECT(manager), callback, userData,
                                                           reinterpret_cast<gpointer>(webkit_cookie_manager_get_domains_with_cookies));
    WKCookieManagerGetHostnamesWithCookies(manager->priv->wkCookieManager.get(), result, webkitCookieManagerGetDomainsWithCookiesCallback);
}

/**
 * webkit_cookie_manager_get_domains_with_cookies_finish:
 * @cookie_manager: a #WebKitCookieManager
 * @result: a #GAsyncResult
 * @error: return location for error or %NULL to ignore
 *
 * Finish an asynchronous operation started with webkit_cookie_manager_get_domains_with_cookies().
 * The return value is a %NULL terminated list of strings which should
 * be released with g_strfreev().
 *
 * Returns: (transfer full) (array zero-terminated=1): A %NULL terminated array of domain names
 *    or %NULL in case of error.
 */
gchar** webkit_cookie_manager_get_domains_with_cookies_finish(WebKitCookieManager* manager, GAsyncResult* result, GError** error)
{
    g_return_val_if_fail(WEBKIT_IS_COOKIE_MANAGER(manager), 0);
    g_return_val_if_fail(G_IS_ASYNC_RESULT(result), 0);

    GSimpleAsyncResult* simpleResult = G_SIMPLE_ASYNC_RESULT(result);
    g_warn_if_fail(g_simple_async_result_get_source_tag(simpleResult) == webkit_cookie_manager_get_domains_with_cookies);

    if (g_simple_async_result_propagate_error(simpleResult, error))
        return 0;

    GPtrArray* domains = static_cast<GPtrArray*>(g_simple_async_result_get_op_res_gpointer(simpleResult));
    return reinterpret_cast<char**>(g_ptr_array_free(domains, FALSE));
}

/**
 * webkit_cookie_manager_delete_cookies_for_domain:
 * @cookie_manager: a #WebKitCookieManager
 * @domain: a domain name
 *
 * Remove all cookies of @cookie_manager for the given @domain.
 */
void webkit_cookie_manager_delete_cookies_for_domain(WebKitCookieManager* manager, const gchar* domain)
{
    g_return_if_fail(WEBKIT_IS_COOKIE_MANAGER(manager));
    g_return_if_fail(domain);

    WKRetainPtr<WKStringRef> wkDomain(AdoptWK, WKStringCreateWithUTF8CString(domain));
    WKCookieManagerDeleteCookiesForHostname(manager->priv->wkCookieManager.get(), wkDomain.get());
}

/**
 * webkit_cookie_manager_delete_all_cookies:
 * @cookie_manager: a #WebKitCookieManager
 *
 * Delete all cookies of @cookie_manager
 */
void webkit_cookie_manager_delete_all_cookies(WebKitCookieManager* manager)
{
    g_return_if_fail(WEBKIT_IS_COOKIE_MANAGER(manager));

    WKCookieManagerDeleteAllCookies(manager->priv->wkCookieManager.get());
}
