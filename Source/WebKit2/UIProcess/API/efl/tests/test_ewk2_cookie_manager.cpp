/*
 * Copyright (C) 2012 Igalia S.L.
 * Copyright (C) 2012 Intel Corporation
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

#include "UnitTestUtils/EWK2UnitTestBase.h"
#include "UnitTestUtils/EWK2UnitTestEnvironment.h"
#include "UnitTestUtils/EWK2UnitTestServer.h"
#include <EWebKit2.h>
#include <Ecore.h>
#include <Eina.h>
#include <stdlib.h>
#include <unistd.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>

using namespace EWK2UnitTest;
using namespace WTF;

extern EWK2UnitTestEnvironment* environment;

static const char FIRST_PARTY_DOMAIN[] = "127.0.0.1";
static const char THIRD_PARTY_DOMAIN[] = "localhost";
static const char INDEX_HTML_STRING[] =
    "<html><body>"
    " <p>EFLWebKit2 Cookie Manager test</p>"
    " <img src='http://localhost:%u/image.png' width=5 height=5></img>"
    "</body></html>";

static void serverCallback(SoupServer* server, SoupMessage* message, const char* path, GHashTable*, SoupClientContext*, gpointer)
{
    if (message->method != SOUP_METHOD_GET) {
        soup_message_set_status(message, SOUP_STATUS_NOT_IMPLEMENTED);
        return;
    }

    soup_message_set_status(message, SOUP_STATUS_OK);
    if (!strcmp(path, "/index.html")) {
        Eina_Strbuf* buffer = eina_strbuf_new();
        eina_strbuf_append_printf(buffer, INDEX_HTML_STRING, soup_server_get_port(server));
        soup_message_headers_replace(message->response_headers, "Set-Cookie", "foo=bar; Max-Age=60");
        soup_message_body_append(message->response_body, SOUP_MEMORY_TAKE, eina_strbuf_string_steal(buffer), eina_strbuf_length_get(buffer));
        eina_strbuf_free(buffer);
    } else if (!strcmp(path, "/image.png"))
        soup_message_headers_replace(message->response_headers, "Set-Cookie", "baz=qux; Max-Age=60");
    else
        FAIL();
    soup_message_body_complete(message->response_body);
}

static void getAcceptPolicyCallback(Ewk_Cookie_Accept_Policy policy, Ewk_Web_Error* error, void* event_info)
{
    ASSERT_FALSE(error);
    Ewk_Cookie_Accept_Policy* ret = static_cast<Ewk_Cookie_Accept_Policy*>(event_info);
    *ret = policy;
    ecore_main_loop_quit();
}

static Ewk_Cookie_Accept_Policy getAcceptPolicy(Ewk_Cookie_Manager* manager)
{
    Ewk_Cookie_Accept_Policy policy = EWK_COOKIE_ACCEPT_POLICY_ALWAYS;
    ewk_cookie_manager_async_accept_policy_get(manager, getAcceptPolicyCallback, &policy);
    ecore_main_loop_begin();
    return policy;
}

static void getHostnamesWithCookiesCallback(Eina_List* hostnames, Ewk_Web_Error* error, void* event_info)
{
    ASSERT_FALSE(error);

    Eina_List** ret = static_cast<Eina_List**>(event_info);
    Eina_List* l;
    void* data;
    EINA_LIST_FOREACH(hostnames, l, data)
        *ret = eina_list_append(*ret, eina_stringshare_ref(static_cast<char*>(data)));
    ecore_main_loop_quit();
}

static Eina_List* getHostnamesWithCookies(Ewk_Cookie_Manager* manager)
{
    Eina_List* ret = 0;
    ewk_cookie_manager_async_hostnames_with_cookies_get(manager, getHostnamesWithCookiesCallback, &ret);
    ecore_main_loop_begin();
    return ret;
}

static void freeHostNames(Eina_List* hostnames)
{
    void* data;
    EINA_LIST_FREE(hostnames, data)
        eina_stringshare_del(static_cast<char*>(data));
}

static int countHostnamesWithCookies(Ewk_Cookie_Manager* manager)
{
    Eina_List* hostnames = getHostnamesWithCookies(manager);
    int count = eina_list_count(hostnames);
    freeHostNames(hostnames);
    return count;
}

TEST_F(EWK2UnitTestBase, ewk_cookie_manager_accept_policy)
{
    OwnPtr<EWK2UnitTestServer> httpServer = adoptPtr(new EWK2UnitTestServer);
    httpServer->run(serverCallback);

    Ewk_Cookie_Manager* cookieManager = ewk_context_cookie_manager_get(ewk_context_default_get());
    ASSERT_TRUE(cookieManager);

    // Default policy is EWK_COOKIE_ACCEPT_POLICY_NO_THIRD_PARTY.
    ASSERT_EQ(getAcceptPolicy(cookieManager), EWK_COOKIE_ACCEPT_POLICY_NO_THIRD_PARTY);
    loadUrlSync(httpServer->getURIForPath("/index.html").data());

    Eina_List* hostnames = getHostnamesWithCookies(cookieManager);
    ASSERT_EQ(eina_list_count(hostnames), 1);
    ASSERT_STREQ(static_cast<char*>(eina_list_nth(hostnames, 0)), FIRST_PARTY_DOMAIN);
    freeHostNames(hostnames);
    ewk_cookie_manager_cookies_clear(cookieManager);

    // Change policy to EWK_COOKIE_ACCEPT_POLICY_ALWAYS
    ewk_cookie_manager_accept_policy_set(cookieManager, EWK_COOKIE_ACCEPT_POLICY_ALWAYS);
    ASSERT_EQ(getAcceptPolicy(cookieManager), EWK_COOKIE_ACCEPT_POLICY_ALWAYS);
    loadUrlSync(httpServer->getURIForPath("/index.html").data());

    hostnames = getHostnamesWithCookies(cookieManager);
    ASSERT_EQ(eina_list_count(hostnames), 2);
    ASSERT_STREQ(static_cast<char*>(eina_list_nth(hostnames, 0)), FIRST_PARTY_DOMAIN);
    ASSERT_STREQ(static_cast<char*>(eina_list_nth(hostnames, 1)), THIRD_PARTY_DOMAIN);
    freeHostNames(hostnames);
    ewk_cookie_manager_cookies_clear(cookieManager);

    // Change policy to EWK_COOKIE_ACCEPT_POLICY_NEVER
    ewk_cookie_manager_accept_policy_set(cookieManager, EWK_COOKIE_ACCEPT_POLICY_NEVER);
    ASSERT_EQ(getAcceptPolicy(cookieManager), EWK_COOKIE_ACCEPT_POLICY_NEVER);
    loadUrlSync(httpServer->getURIForPath("/index.html").data());
    ASSERT_EQ(countHostnamesWithCookies(cookieManager), 0);
}

void onCookiesChanged(void *eventInfo)
{
    bool* cookiesChanged = static_cast<bool*>(eventInfo);
    *cookiesChanged = true;
}

TEST_F(EWK2UnitTestBase, ewk_cookie_manager_changes_watch)
{
    OwnPtr<EWK2UnitTestServer> httpServer = adoptPtr(new EWK2UnitTestServer);
    httpServer->run(serverCallback);

    Ewk_Cookie_Manager* cookieManager = ewk_context_cookie_manager_get(ewk_context_default_get());
    ASSERT_TRUE(cookieManager);

    ewk_cookie_manager_accept_policy_set(cookieManager, EWK_COOKIE_ACCEPT_POLICY_ALWAYS);
    ASSERT_EQ(getAcceptPolicy(cookieManager), EWK_COOKIE_ACCEPT_POLICY_ALWAYS);

    // Watch for changes
    bool cookiesChanged = false;
    ewk_cookie_manager_changes_watch(cookieManager, onCookiesChanged, &cookiesChanged);

    // Check for cookie changes notifications
    loadUrlSync(httpServer->getURIForPath("/index.html").data());

    while (!cookiesChanged)
        ecore_main_loop_iterate();
    ASSERT_TRUE(cookiesChanged);

    cookiesChanged = false;
    ewk_cookie_manager_cookies_clear(cookieManager);
    while (!cookiesChanged)
        ecore_main_loop_iterate();
    ASSERT_TRUE(cookiesChanged);

    // Stop watching for notifications
    ewk_cookie_manager_changes_watch(cookieManager, 0, 0);
    cookiesChanged = false;
    loadUrlSync(httpServer->getURIForPath("/index.html").data());
    ASSERT_EQ(countHostnamesWithCookies(cookieManager), 2);
    ASSERT_FALSE(cookiesChanged);

    // Watch again for notifications
    ewk_cookie_manager_changes_watch(cookieManager, onCookiesChanged, &cookiesChanged);

    // Make sure we don't get notifications when loading setting an existing persistent storage
    char textStorage1[] = "/tmp/txt-cookie.XXXXXX";
    ASSERT_TRUE(mktemp(textStorage1));
    char textStorage2[] = "/tmp/txt-cookie.XXXXXX";
    ASSERT_TRUE(mktemp(textStorage2));

    ewk_cookie_manager_persistent_storage_set(cookieManager, textStorage1, EWK_COOKIE_PERSISTENT_STORAGE_TEXT);
    loadUrlSync(httpServer->getURIForPath("/index.html").data());
    ASSERT_EQ(countHostnamesWithCookies(cookieManager), 2);

    cookiesChanged = false;
    ewk_cookie_manager_persistent_storage_set(cookieManager, textStorage2, EWK_COOKIE_PERSISTENT_STORAGE_TEXT);
    ASSERT_EQ(countHostnamesWithCookies(cookieManager), 0);

    ewk_cookie_manager_persistent_storage_set(cookieManager, textStorage1, EWK_COOKIE_PERSISTENT_STORAGE_TEXT);
    ASSERT_EQ(countHostnamesWithCookies(cookieManager), 2);

    ASSERT_FALSE(cookiesChanged);

    // Final clean up.
    ewk_cookie_manager_changes_watch(cookieManager, 0, 0);
    unlink(textStorage1);
    unlink(textStorage2);
}

TEST_F(EWK2UnitTestBase, ewk_cookie_manager_cookies_delete)
{
    OwnPtr<EWK2UnitTestServer> httpServer = adoptPtr(new EWK2UnitTestServer);
    httpServer->run(serverCallback);

    Ewk_Cookie_Manager* cookieManager = ewk_context_cookie_manager_get(ewk_context_default_get());
    ASSERT_TRUE(cookieManager);

    ewk_cookie_manager_accept_policy_set(cookieManager, EWK_COOKIE_ACCEPT_POLICY_ALWAYS);
    ASSERT_EQ(getAcceptPolicy(cookieManager), EWK_COOKIE_ACCEPT_POLICY_ALWAYS);

    loadUrlSync(httpServer->getURIForPath("/index.html").data());
    Eina_List* hostnames = getHostnamesWithCookies(cookieManager);
    ASSERT_EQ(eina_list_count(hostnames), 2);
    freeHostNames(hostnames);

    // Delete first party cookie
    ewk_cookie_manager_hostname_cookies_clear(cookieManager, FIRST_PARTY_DOMAIN);
    hostnames = getHostnamesWithCookies(cookieManager);
    ASSERT_EQ(eina_list_count(hostnames), 1);
    ASSERT_STREQ(static_cast<char*>(eina_list_nth(hostnames, 0)), THIRD_PARTY_DOMAIN);
    freeHostNames(hostnames);

    // Delete third party cookie
    ewk_cookie_manager_hostname_cookies_clear(cookieManager, THIRD_PARTY_DOMAIN);
    ASSERT_EQ(countHostnamesWithCookies(cookieManager), 0);

    // Get all cookies again
    loadUrlSync(httpServer->getURIForPath("/index.html").data());
    ASSERT_EQ(countHostnamesWithCookies(cookieManager), 2);

    // Clear all cookies
    ewk_cookie_manager_cookies_clear(cookieManager);
    ASSERT_EQ(countHostnamesWithCookies(cookieManager), 0);
}

TEST_F(EWK2UnitTestBase, ewk_cookie_manager_permanent_storage)
{
    OwnPtr<EWK2UnitTestServer> httpServer = adoptPtr(new EWK2UnitTestServer);
    httpServer->run(serverCallback);

    // Generate unique names for cookie storages.
    char textStorage[] = "/tmp/txt-cookie.XXXXXX";
    ASSERT_TRUE(mktemp(textStorage));
    char sqliteStorage[] = "/tmp/sqlite-cookie.XXXXXX";
    ASSERT_TRUE(mktemp(sqliteStorage));

    Ewk_Cookie_Manager* cookieManager = ewk_context_cookie_manager_get(ewk_context_default_get());
    ASSERT_TRUE(cookieManager);

    ewk_cookie_manager_accept_policy_set(cookieManager, EWK_COOKIE_ACCEPT_POLICY_ALWAYS);
    ASSERT_EQ(getAcceptPolicy(cookieManager), EWK_COOKIE_ACCEPT_POLICY_ALWAYS);

    // Text storage using a new file.
    ewk_cookie_manager_persistent_storage_set(cookieManager, textStorage, EWK_COOKIE_PERSISTENT_STORAGE_TEXT);
    ASSERT_EQ(countHostnamesWithCookies(cookieManager), 0);

    loadUrlSync(httpServer->getURIForPath("/index.html").data());
    ASSERT_EQ(countHostnamesWithCookies(cookieManager), 2);

    // SQLite storage using a new file.
    ewk_cookie_manager_persistent_storage_set(cookieManager, sqliteStorage, EWK_COOKIE_PERSISTENT_STORAGE_SQLITE);
    ASSERT_EQ(countHostnamesWithCookies(cookieManager), 0);

    loadUrlSync(httpServer->getURIForPath("/index.html").data());
    ASSERT_EQ(countHostnamesWithCookies(cookieManager), 2);

    // Text storage using an existing file.
    ewk_cookie_manager_persistent_storage_set(cookieManager, textStorage, EWK_COOKIE_PERSISTENT_STORAGE_TEXT);
    ASSERT_EQ(countHostnamesWithCookies(cookieManager), 2);
    ewk_cookie_manager_cookies_clear(cookieManager);
    ASSERT_EQ(countHostnamesWithCookies(cookieManager), 0);

    // SQLite storage with an existing file.
    ewk_cookie_manager_persistent_storage_set(cookieManager, sqliteStorage, EWK_COOKIE_PERSISTENT_STORAGE_SQLITE);
    ASSERT_EQ(countHostnamesWithCookies(cookieManager), 2);
    ewk_cookie_manager_cookies_clear(cookieManager);
    ASSERT_EQ(countHostnamesWithCookies(cookieManager), 0);

    // Final clean up.
    unlink(textStorage);
    unlink(sqliteStorage);
}

