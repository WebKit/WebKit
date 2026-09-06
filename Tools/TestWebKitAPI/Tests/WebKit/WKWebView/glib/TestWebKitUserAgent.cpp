/*
 * Copyright (C) 2026 John Cardullo <john@jcarmedia.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "TestMain.h"
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/GUniquePtr.h>

static void testWebKitUserAgentDefault(Test*, gconstpointer)
{
    g_autoptr(WebKitUserAgent) ua = webkit_user_agent_new(WEBKIT_USER_AGENT_TYPE_DEFAULT);
    g_assert_nonnull(ua);
    g_assert_cmpint(webkit_user_agent_get_user_agent_type(ua), ==, WEBKIT_USER_AGENT_TYPE_DEFAULT);
    g_assert_null(webkit_user_agent_get_application_name(ua));
    g_assert_null(webkit_user_agent_get_application_version(ua));

    const char* uaString = webkit_user_agent_to_string(ua);
    g_assert_nonnull(uaString);
    g_assert_nonnull(g_strstr_len(uaString, -1, "Mozilla/5.0"));
    g_assert_nonnull(g_strstr_len(uaString, -1, "AppleWebKit"));
    g_assert_nonnull(g_strstr_len(uaString, -1, "Safari"));
}

static void testWebKitUserAgentTypes(Test*, gconstpointer)
{
    g_autoptr(WebKitUserAgent) ua = webkit_user_agent_new(WEBKIT_USER_AGENT_TYPE_DEFAULT);

    webkit_user_agent_set_user_agent_type(ua, WEBKIT_USER_AGENT_TYPE_MOBILE);
    g_assert_cmpint(webkit_user_agent_get_user_agent_type(ua), ==, WEBKIT_USER_AGENT_TYPE_MOBILE);
    const char* mobileUA = webkit_user_agent_to_string(ua);
    g_assert_nonnull(g_strstr_len(mobileUA, -1, "Mobile"));
    g_assert_nonnull(g_strstr_len(mobileUA, -1, "Android"));

    webkit_user_agent_set_user_agent_type(ua, WEBKIT_USER_AGENT_TYPE_DESKTOP);
    g_assert_cmpint(webkit_user_agent_get_user_agent_type(ua), ==, WEBKIT_USER_AGENT_TYPE_DESKTOP);
    const char* desktopUA = webkit_user_agent_to_string(ua);
    g_assert_null(g_strstr_len(desktopUA, -1, "Mobile"));
}

static void testWebKitUserAgentApplicationDetails(Test*, gconstpointer)
{
    g_autoptr(WebKitUserAgent) ua = webkit_user_agent_new_with_details(WEBKIT_USER_AGENT_TYPE_DESKTOP, "Epiphany", "45.0");
    g_assert_cmpint(webkit_user_agent_get_user_agent_type(ua), ==, WEBKIT_USER_AGENT_TYPE_DESKTOP);
    g_assert_cmpstr(webkit_user_agent_get_application_name(ua), ==, "Epiphany");
    g_assert_cmpstr(webkit_user_agent_get_application_version(ua), ==, "45.0");

    const char* uaString = webkit_user_agent_to_string(ua);
    g_assert_nonnull(g_strstr_len(uaString, -1, "Epiphany/45.0"));

    webkit_user_agent_set_application_name(ua, "WebCatGTK+");
    webkit_user_agent_set_application_version(ua, "3.4.5");
    uaString = webkit_user_agent_to_string(ua);
    g_assert_nonnull(g_strstr_len(uaString, -1, "WebCatGTK+/3.4.5"));
}

static void testWebKitUserAgentRefCopy(Test*, gconstpointer)
{
    WebKitUserAgent* ua = webkit_user_agent_new_with_details(WEBKIT_USER_AGENT_TYPE_MOBILE, "TestApp", "1.0");
    WebKitUserAgent* ref = webkit_user_agent_ref(ua);
    g_assert_true(ua == ref);

    webkit_user_agent_unref(ref);
    g_assert_cmpstr(webkit_user_agent_get_application_name(ua), ==, "TestApp");
    webkit_user_agent_unref(ua);
}

void beforeAll()
{
    Test::add("WebKitUserAgent", "default", testWebKitUserAgentDefault);
    Test::add("WebKitUserAgent", "types", testWebKitUserAgentTypes);
    Test::add("WebKitUserAgent", "application-details", testWebKitUserAgentApplicationDetails);
    Test::add("WebKitUserAgent", "ref-copy", testWebKitUserAgentRefCopy);
}

void afterAll()
{
}
