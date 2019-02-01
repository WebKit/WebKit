/*
 * Copyright (C) 2012 Igalia S.L.
 * Copyright (C) 2017 Endless Mobile, Inc.
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

#include "SoupCookiePersistentStorageType.h"
#include "WebCookieManagerProxy.h"
#include "WebKitCookieManagerPrivate.h"
#include "WebKitEnumTypes.h"
#include "WebKitWebsiteDataManagerPrivate.h"
#include "WebKitWebsiteDataPrivate.h"
#include "WebsiteDataRecord.h"
#include <glib/gi18n-lib.h>
#include <pal/SessionID.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/WTFGType.h>
#include <wtf/text/CString.h>

using namespace WebKit;

/**
 * SECTION: WebKitCookieManager
 * @Short_description: Defines how to handle cookies in a #WebKitWebContext
 * @Title: WebKitCookieManager
 *
 * The WebKitCookieManager defines how to set up and handle cookies.
 * You can get it from a #WebKitWebsiteDataManager with
 * webkit_website_data_manager_get_cookie_manager(), and use it to set where to
 * store cookies with webkit_cookie_manager_set_persistent_storage(),
 * or to set the acceptance policy, with webkit_cookie_manager_get_accept_policy().
 */

enum {
    CHANGED,

    LAST_SIGNAL
};

struct _WebKitCookieManagerPrivate {
    PAL::SessionID sessionID() const
    {
        ASSERT(dataManager);
        return webkitWebsiteDataManagerGetDataStore(dataManager).websiteDataStore().sessionID();
    }

    ~_WebKitCookieManagerPrivate()
    {
        for (auto* processPool : webkitWebsiteDataManagerGetProcessPools(dataManager))
            processPool->supplement<WebCookieManagerProxy>()->setCookieObserverCallback(sessionID(), nullptr);
    }

    WebKitWebsiteDataManager* dataManager;
};

static guint signals[LAST_SIGNAL] = { 0, };

WEBKIT_DEFINE_TYPE(WebKitCookieManager, webkit_cookie_manager, G_TYPE_OBJECT)

static inline SoupCookiePersistentStorageType toSoupCookiePersistentStorageType(WebKitCookiePersistentStorage kitStorage)
{
    switch (kitStorage) {
    case WEBKIT_COOKIE_PERSISTENT_STORAGE_TEXT:
        return SoupCookiePersistentStorageType::Text;
    case WEBKIT_COOKIE_PERSISTENT_STORAGE_SQLITE:
        return SoupCookiePersistentStorageType::SQLite;
    default:
        ASSERT_NOT_REACHED();
        return SoupCookiePersistentStorageType::Text;
    }
}

static inline WebKitCookieAcceptPolicy toWebKitCookieAcceptPolicy(HTTPCookieAcceptPolicy httpPolicy)
{
    switch (httpPolicy) {
    case HTTPCookieAcceptPolicyAlways:
        return WEBKIT_COOKIE_POLICY_ACCEPT_ALWAYS;
    case HTTPCookieAcceptPolicyNever:
        return WEBKIT_COOKIE_POLICY_ACCEPT_NEVER;
    case HTTPCookieAcceptPolicyOnlyFromMainDocumentDomain:
        return WEBKIT_COOKIE_POLICY_ACCEPT_NO_THIRD_PARTY;
    default:
        ASSERT_NOT_REACHED();
        return WEBKIT_COOKIE_POLICY_ACCEPT_ALWAYS;
    }
}

static inline HTTPCookieAcceptPolicy toHTTPCookieAcceptPolicy(WebKitCookieAcceptPolicy kitPolicy)
{
    switch (kitPolicy) {
    case WEBKIT_COOKIE_POLICY_ACCEPT_ALWAYS:
        return HTTPCookieAcceptPolicyAlways;
    case WEBKIT_COOKIE_POLICY_ACCEPT_NEVER:
        return HTTPCookieAcceptPolicyNever;
    case WEBKIT_COOKIE_POLICY_ACCEPT_NO_THIRD_PARTY:
        return HTTPCookieAcceptPolicyOnlyFromMainDocumentDomain;
    default:
        ASSERT_NOT_REACHED();
        return HTTPCookieAcceptPolicyAlways;
    }
}

static void webkit_cookie_manager_class_init(WebKitCookieManagerClass* findClass)
{
    GObjectClass* gObjectClass = G_OBJECT_CLASS(findClass);

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

WebKitCookieManager* webkitCookieManagerCreate(WebKitWebsiteDataManager* dataManager)
{
    WebKitCookieManager* manager = WEBKIT_COOKIE_MANAGER(g_object_new(WEBKIT_TYPE_COOKIE_MANAGER, nullptr));
    manager->priv->dataManager = dataManager;
    for (auto* processPool : webkitWebsiteDataManagerGetProcessPools(manager->priv->dataManager)) {
        processPool->supplement<WebCookieManagerProxy>()->setCookieObserverCallback(manager->priv->sessionID(), [manager] {
            g_signal_emit(manager, signals[CHANGED], 0);
        });
    }
    return manager;
}

/**
 * webkit_cookie_manager_set_persistent_storage:
 * @cookie_manager: a #WebKitCookieManager
 * @filename: the filename to read to/write from
 * @storage: a #WebKitCookiePersistentStorage
 *
 * Set the @filename where non-session cookies are stored persistently using
 * @storage as the format to read/write the cookies.
 * Cookies are initially read from @filename to create an initial set of cookies.
 * Then, non-session cookies will be written to @filename when the WebKitCookieManager::changed
 * signal is emitted.
 * By default, @cookie_manager doesn't store the cookies persistently, so you need to call this
 * method to keep cookies saved across sessions.
 *
 * This method should never be called on a #WebKitCookieManager associated to an ephemeral #WebKitWebsiteDataManager.
 */
void webkit_cookie_manager_set_persistent_storage(WebKitCookieManager* manager, const char* filename, WebKitCookiePersistentStorage storage)
{
    g_return_if_fail(WEBKIT_IS_COOKIE_MANAGER(manager));
    g_return_if_fail(filename);
    g_return_if_fail(!webkit_website_data_manager_is_ephemeral(manager->priv->dataManager));

    auto sessionID = manager->priv->sessionID();
    if (sessionID.isEphemeral())
        return;

    for (auto* processPool : webkitWebsiteDataManagerGetProcessPools(manager->priv->dataManager))
        processPool->supplement<WebCookieManagerProxy>()->setCookiePersistentStorage(sessionID, String::fromUTF8(filename), toSoupCookiePersistentStorageType(storage));
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

    for (auto* processPool : webkitWebsiteDataManagerGetProcessPools(manager->priv->dataManager))
        processPool->supplement<WebCookieManagerProxy>()->setHTTPCookieAcceptPolicy(manager->priv->sessionID(), toHTTPCookieAcceptPolicy(policy), [](CallbackBase::Error) { });
}

/**
 * webkit_cookie_manager_get_accept_policy:
 * @cookie_manager: a #WebKitCookieManager
 * @cancellable: (allow-none): a #GCancellable or %NULL to ignore
 * @callback: (scope async): a #GAsyncReadyCallback to call when the request is satisfied
 * @user_data: (closure): the data to pass to callback function
 *
 * Asynchronously get the cookie acceptance policy of @cookie_manager.
 *
 * When the operation is finished, @callback will be called. You can then call
 * webkit_cookie_manager_get_accept_policy_finish() to get the result of the operation.
 */
void webkit_cookie_manager_get_accept_policy(WebKitCookieManager* manager, GCancellable* cancellable, GAsyncReadyCallback callback, gpointer userData)
{
    g_return_if_fail(WEBKIT_IS_COOKIE_MANAGER(manager));

    GRefPtr<GTask> task = adoptGRef(g_task_new(manager, cancellable, callback, userData));

    // The policy is the same in all process pools having the same session ID, so just ask any.
    const auto& processPools = webkitWebsiteDataManagerGetProcessPools(manager->priv->dataManager);
    if (processPools.isEmpty()) {
        g_task_return_int(task.get(), WEBKIT_COOKIE_POLICY_ACCEPT_NO_THIRD_PARTY);
        return;
    }

    processPools[0]->supplement<WebCookieManagerProxy>()->getHTTPCookieAcceptPolicy(manager->priv->sessionID(), [task = WTFMove(task)](HTTPCookieAcceptPolicy policy, CallbackBase::Error) {
        g_task_return_int(task.get(), toWebKitCookieAcceptPolicy(policy));
    });
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
    g_return_val_if_fail(g_task_is_valid(result, manager), WEBKIT_COOKIE_POLICY_ACCEPT_NO_THIRD_PARTY);

    gssize returnValue = g_task_propagate_int(G_TASK(result), error);
    return returnValue == -1 ? WEBKIT_COOKIE_POLICY_ACCEPT_NO_THIRD_PARTY : static_cast<WebKitCookieAcceptPolicy>(returnValue);
}

/**
 * webkit_cookie_manager_add_cookie:
 * @cookie_manager: a #WebKitCookieManager
 * @cookie: the #SoupCookie to be added
 * @cancellable: (allow-none): a #GCancellable or %NULL to ignore
 * @callback: (scope async): a #GAsyncReadyCallback to call when the request is satisfied
 * @user_data: (closure): the data to pass to callback function
 *
 * Asynchronously add a #SoupCookie to the underlying storage.
 *
 * When the operation is finished, @callback will be called. You can then call
 * webkit_cookie_manager_add_cookie_finish() to get the result of the operation.
 *
 * Since: 2.20
 */
void webkit_cookie_manager_add_cookie(WebKitCookieManager* manager, SoupCookie* cookie, GCancellable* cancellable, GAsyncReadyCallback callback, gpointer userData)
{
    g_return_if_fail(WEBKIT_IS_COOKIE_MANAGER(manager));
    g_return_if_fail(cookie);

    GRefPtr<GTask> task = adoptGRef(g_task_new(manager, cancellable, callback, userData));

    // Cookies are read/written from/to the same SQLite database on disk regardless
    // of the process we access them from, so just use the first process pool.
    const auto& processPools = webkitWebsiteDataManagerGetProcessPools(manager->priv->dataManager);
    processPools[0]->supplement<WebCookieManagerProxy>()->setCookie(manager->priv->sessionID(), WebCore::Cookie(cookie), [task = WTFMove(task)](CallbackBase::Error error) {
        if (error != CallbackBase::Error::None) {
            // This can only happen in cases where the web process is not available,
            // consider the operation "cancelled" from the point of view of the client.
            g_task_return_new_error(task.get(), G_IO_ERROR, G_IO_ERROR_CANCELLED, _("Operation was cancelled"));
            return;
        }

        g_task_return_boolean(task.get(), TRUE);
    });
}

/**
 * webkit_cookie_manager_add_cookie_finish:
 * @cookie_manager: a #WebKitCookieManager
 * @result: a #GAsyncResult
 * @error: return location for error or %NULL to ignore
 *
 * Finish an asynchronous operation started with webkit_cookie_manager_add_cookie().
 *
 * Returns: %TRUE if the cookie was added or %FALSE in case of error.
 *
 * Since: 2.20
 */
gboolean webkit_cookie_manager_add_cookie_finish(WebKitCookieManager* manager, GAsyncResult* result, GError** error)
{
    g_return_val_if_fail(WEBKIT_IS_COOKIE_MANAGER(manager), FALSE);
    g_return_val_if_fail(g_task_is_valid(result, manager), FALSE);

    return g_task_propagate_boolean(G_TASK(result), error);
}

/**
 * webkit_cookie_manager_get_cookies:
 * @cookie_manager: a #WebKitCookieManager
 * @uri: the URI associated to the cookies to be retrieved
 * @cancellable: (allow-none): a #GCancellable or %NULL to ignore
 * @callback: (scope async): a #GAsyncReadyCallback to call when the request is satisfied
 * @user_data: (closure): the data to pass to callback function
 *
 * Asynchronously get a list of #SoupCookie from @cookie_manager associated with @uri, which
 * must be either an HTTP or an HTTPS URL.
 *
 * When the operation is finished, @callback will be called. You can then call
 * webkit_cookie_manager_get_cookies_finish() to get the result of the operation.
 *
 * Since: 2.20
 */
void webkit_cookie_manager_get_cookies(WebKitCookieManager* manager, const gchar* uri, GCancellable* cancellable, GAsyncReadyCallback callback, gpointer userData)
{
    g_return_if_fail(WEBKIT_IS_COOKIE_MANAGER(manager));
    g_return_if_fail(uri);

    GRefPtr<GTask> task = adoptGRef(g_task_new(manager, cancellable, callback, userData));

    // Cookies are read/written from/to the same SQLite database on disk regardless
    // of the process we access them from, so just use the first process pool.
    const auto& processPools = webkitWebsiteDataManagerGetProcessPools(manager->priv->dataManager);
    processPools[0]->supplement<WebCookieManagerProxy>()->getCookies(manager->priv->sessionID(), URL(URL(), String::fromUTF8(uri)), [task = WTFMove(task)](const Vector<WebCore::Cookie>& cookies, CallbackBase::Error error) {
        if (error != CallbackBase::Error::None) {
            // This can only happen in cases where the web process is not available,
            // consider the operation "cancelled" from the point of view of the client.
            g_task_return_new_error(task.get(), G_IO_ERROR, G_IO_ERROR_CANCELLED, _("Operation was cancelled"));
            return;
        }

        GList* cookiesList = nullptr;
        for (auto& cookie : cookies)
            cookiesList = g_list_prepend(cookiesList, cookie.toSoupCookie());

        g_task_return_pointer(task.get(), g_list_reverse(cookiesList), [](gpointer data) {
            g_list_free_full(static_cast<GList*>(data), reinterpret_cast<GDestroyNotify>(soup_cookie_free));
        });
    });
}

/**
 * webkit_cookie_manager_get_cookies_finish:
 * @cookie_manager: a #WebKitCookieManager
 * @result: a #GAsyncResult
 * @error: return location for error or %NULL to ignore
 *
 * Finish an asynchronous operation started with webkit_cookie_manager_get_cookies().
 * The return value is a #GSList of #SoupCookie instances which should be released
 * with g_list_free_full() and soup_cookie_free().
 *
 * Returns: (element-type SoupCookie) (transfer full): A #GList of #SoupCookie instances.
 *
 * Since: 2.20
 */
GList* webkit_cookie_manager_get_cookies_finish(WebKitCookieManager* manager, GAsyncResult* result, GError** error)
{
    g_return_val_if_fail(WEBKIT_IS_COOKIE_MANAGER(manager), nullptr);
    g_return_val_if_fail(g_task_is_valid(result, manager), nullptr);

    return reinterpret_cast<GList*>(g_task_propagate_pointer(G_TASK(result), error));
}

/**
 * webkit_cookie_manager_delete_cookie:
 * @cookie_manager: a #WebKitCookieManager
 * @cookie: the #SoupCookie to be deleted
 * @cancellable: (allow-none): a #GCancellable or %NULL to ignore
 * @callback: (scope async): a #GAsyncReadyCallback to call when the request is satisfied
 * @user_data: (closure): the data to pass to callback function
 *
 * Asynchronously delete a #SoupCookie from the current session.
 *
 * When the operation is finished, @callback will be called. You can then call
 * webkit_cookie_manager_delete_cookie_finish() to get the result of the operation.
 *
 * Since: 2.20
 */
void webkit_cookie_manager_delete_cookie(WebKitCookieManager* manager, SoupCookie* cookie, GCancellable* cancellable, GAsyncReadyCallback callback, gpointer userData)
{
    g_return_if_fail(WEBKIT_IS_COOKIE_MANAGER(manager));
    g_return_if_fail(cookie);

    GRefPtr<GTask> task = adoptGRef(g_task_new(manager, cancellable, callback, userData));

    // Cookies are read/written from/to the same SQLite database on disk regardless
    // of the process we access them from, so just use the first process pool.
    const auto& processPools = webkitWebsiteDataManagerGetProcessPools(manager->priv->dataManager);
    processPools[0]->supplement<WebCookieManagerProxy>()->deleteCookie(manager->priv->sessionID(), WebCore::Cookie(cookie), [task = WTFMove(task)](CallbackBase::Error error) {
        if (error != CallbackBase::Error::None) {
            // This can only happen in cases where the web process is not available,
            // consider the operation "cancelled" from the point of view of the client.
            g_task_return_new_error(task.get(), G_IO_ERROR, G_IO_ERROR_CANCELLED, _("Operation was cancelled"));
            return;
        }

        g_task_return_boolean(task.get(), TRUE);
    });
}

/**
 * webkit_cookie_manager_delete_cookie_finish:
 * @cookie_manager: a #WebKitCookieManager
 * @result: a #GAsyncResult
 * @error: return location for error or %NULL to ignore
 *
 * Finish an asynchronous operation started with webkit_cookie_manager_delete_cookie().
 *
 * Returns: %TRUE if the cookie was deleted or %FALSE in case of error.
 *
 * Since: 2.20
 */
gboolean webkit_cookie_manager_delete_cookie_finish(WebKitCookieManager* manager, GAsyncResult* result, GError** error)
{
    g_return_val_if_fail(WEBKIT_IS_COOKIE_MANAGER(manager), FALSE);
    g_return_val_if_fail(g_task_is_valid(result, manager), FALSE);

    return g_task_propagate_boolean(G_TASK(result), error);
}

#if PLATFORM(GTK)
/**
 * webkit_cookie_manager_get_domains_with_cookies:
 * @cookie_manager: a #WebKitCookieManager
 * @cancellable: (allow-none): a #GCancellable or %NULL to ignore
 * @callback: (scope async): a #GAsyncReadyCallback to call when the request is satisfied
 * @user_data: (closure): the data to pass to callback function
 *
 * Asynchronously get the list of domains for which @cookie_manager contains cookies.
 *
 * When the operation is finished, @callback will be called. You can then call
 * webkit_cookie_manager_get_domains_with_cookies_finish() to get the result of the operation.
 *
 * Deprecated: 2.16: Use webkit_website_data_manager_fetch() instead.
 */
void webkit_cookie_manager_get_domains_with_cookies(WebKitCookieManager* manager, GCancellable* cancellable, GAsyncReadyCallback callback, gpointer userData)
{
    g_return_if_fail(WEBKIT_IS_COOKIE_MANAGER(manager));

    GTask* task = g_task_new(manager, cancellable, callback, userData);
    webkit_website_data_manager_fetch(manager->priv->dataManager, WEBKIT_WEBSITE_DATA_COOKIES, cancellable, [](GObject* object, GAsyncResult* result, gpointer userData) {
        GRefPtr<GTask> task = adoptGRef(G_TASK(userData));
        GError* error = nullptr;
        GUniquePtr<GList> dataList(webkit_website_data_manager_fetch_finish(WEBKIT_WEBSITE_DATA_MANAGER(object), result, &error));
        if (error) {
            g_task_return_error(task.get(), error);
            return;
        }

        GPtrArray* domains = g_ptr_array_sized_new(g_list_length(dataList.get()));
        for (GList* item = dataList.get(); item; item = g_list_next(item)) {
            auto* data = static_cast<WebKitWebsiteData*>(item->data);
            g_ptr_array_add(domains, g_strdup(webkit_website_data_get_name(data)));
            webkit_website_data_unref(data);
        }
        g_ptr_array_add(domains, nullptr);
        g_task_return_pointer(task.get(), g_ptr_array_free(domains, FALSE), reinterpret_cast<GDestroyNotify>(g_strfreev));
    }, task);
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
 *
 * Deprecated: 2.16: Use webkit_website_data_manager_fetch_finish() instead.
 */
gchar** webkit_cookie_manager_get_domains_with_cookies_finish(WebKitCookieManager* manager, GAsyncResult* result, GError** error)
{
    g_return_val_if_fail(WEBKIT_IS_COOKIE_MANAGER(manager), nullptr);
    g_return_val_if_fail(g_task_is_valid(result, manager), nullptr);

    return reinterpret_cast<char**>(g_task_propagate_pointer(G_TASK(result), error));
}

/**
 * webkit_cookie_manager_delete_cookies_for_domain:
 * @cookie_manager: a #WebKitCookieManager
 * @domain: a domain name
 *
 * Remove all cookies of @cookie_manager for the given @domain.
 *
 * Deprecated: 2.16: Use webkit_website_data_manager_remove() instead.
 */
void webkit_cookie_manager_delete_cookies_for_domain(WebKitCookieManager* manager, const gchar* domain)
{
    g_return_if_fail(WEBKIT_IS_COOKIE_MANAGER(manager));
    g_return_if_fail(domain);

    WebsiteDataRecord record;
    record.addCookieHostName(String::fromUTF8(domain));
    auto* data = webkitWebsiteDataCreate(WTFMove(record));
    GList dataList = { data, nullptr, nullptr };
    webkit_website_data_manager_remove(manager->priv->dataManager, WEBKIT_WEBSITE_DATA_COOKIES, &dataList, nullptr, nullptr, nullptr);
    webkit_website_data_unref(data);
}

/**
 * webkit_cookie_manager_delete_all_cookies:
 * @cookie_manager: a #WebKitCookieManager
 *
 * Delete all cookies of @cookie_manager
 *
 * Deprecated: 2.16: Use webkit_website_data_manager_clear() instead.
 */
void webkit_cookie_manager_delete_all_cookies(WebKitCookieManager* manager)
{
    g_return_if_fail(WEBKIT_IS_COOKIE_MANAGER(manager));

    webkit_website_data_manager_clear(manager->priv->dataManager, WEBKIT_WEBSITE_DATA_COOKIES, 0, nullptr, nullptr, nullptr);
}
#endif
