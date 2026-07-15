/*
 * Copyright (C) 2025 Igalia, S.L. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
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

#if ENABLE(WK_WEB_EXTENSIONS)

#include "TestMain.h"
#include "WebExtensionUtilities.h"
#include <WebKitWebExtensionInternal.h>
#include <wtf/HashMap.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

static GRefPtr<GBytes> createGBytes(const gchar* string)
{
    return adoptGRef(g_bytes_new_static(string, strlen(string)));
}

static void testDefaultPermissionChecks(Test*, gconstpointer)
{
    GUniqueOutPtr<GError> error;
    auto parseExtensionManifest = [&](const gchar* manifestString) {
        return adoptGRef(webkitWebExtensionCreate({ { "manifest.json"_s, createGBytes(manifestString) } }, &error.outPtr()));
    };

    GRefPtr<WebKitWebExtension> extension = parseExtensionManifest("{ \"manifest_version\": 2, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\", \"permissions\": [] }");
    g_assert_no_error(error.get());
    GRefPtr<WebKitWebExtensionContext> context = webkit_web_extension_context_new_for_extension(extension.get(), &error.outPtr());
    g_assert_no_error(error.get());

    g_assert_false(webkit_web_extension_context_has_permission(context.get(), "tabs"));
    g_assert_false(webkit_web_extension_context_has_permission(context.get(), "cookies"));
    g_assert_false(webkit_web_extension_context_get_has_access_to_all_uris(context.get()));
    g_assert_false(webkit_web_extension_context_get_has_access_to_all_hosts(context.get()));
    g_assert_false(webkit_web_extension_context_has_access_to_uri(context.get(), "https://example.com/"));
    g_assert_false(webkit_web_extension_context_has_access_to_uri(context.get(), "https://webkit.org"));
    g_assert_cmpint(webkit_web_extension_context_permission_status_for_permission(context.get(), "tabs"), ==, WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_UNKNOWN);
    g_assert_cmpint(webkit_web_extension_context_permission_status_for_permission(context.get(), "cookies"), ==, WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_UNKNOWN);
    g_assert_cmpint(webkit_web_extension_context_permission_status_for_uri(context.get(), "https://example.com/"), ==, WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_UNKNOWN);
    g_assert_cmpint(webkit_web_extension_context_permission_status_for_uri(context.get(), "https://webkit.org/"), ==, WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_UNKNOWN);
    g_assert_cmpint(webkit_web_extension_context_permission_status_for_uri(context.get(), "https://unknown.com/"), ==, WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_UNKNOWN);
    g_assert_null(webkit_web_extension_context_get_granted_permissions(context.get()));
    g_assert_null(webkit_web_extension_context_get_granted_permission_match_patterns(context.get()));
    g_assert_null(webkit_web_extension_context_get_denied_permissions(context.get()));
    g_assert_null(webkit_web_extension_context_get_denied_permission_match_patterns(context.get()));

    extension = parseExtensionManifest("{ \"manifest_version\": 2, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\", \"permissions\": [ \"tabs\", \"https://*.example.com/*\" ] }");
    g_assert_no_error(error.get());
    context = webkit_web_extension_context_new_for_extension(extension.get(), &error.outPtr());
    g_assert_no_error(error.get());

    g_assert_false(webkit_web_extension_context_has_permission(context.get(), "tabs"));
    g_assert_false(webkit_web_extension_context_has_permission(context.get(), "cookies"));
    g_assert_false(webkit_web_extension_context_get_has_access_to_all_uris(context.get()));
    g_assert_false(webkit_web_extension_context_get_has_access_to_all_hosts(context.get()));
    g_assert_false(webkit_web_extension_context_has_access_to_uri(context.get(), "https://example.com/"));
    g_assert_false(webkit_web_extension_context_has_access_to_uri(context.get(), "https://webkit.org"));
    g_assert_cmpint(webkit_web_extension_context_permission_status_for_permission(context.get(), "tabs"), ==, WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_REQUESTED_EXPLICITLY);
    g_assert_cmpint(webkit_web_extension_context_permission_status_for_permission(context.get(), "cookies"), ==, WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_UNKNOWN);
    g_assert_cmpint(webkit_web_extension_context_permission_status_for_uri(context.get(), "https://example.com/"), ==, WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_REQUESTED_EXPLICITLY);
    g_assert_cmpint(webkit_web_extension_context_permission_status_for_uri(context.get(), "https://webkit.org/"), ==, WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_UNKNOWN);
    g_assert_cmpint(webkit_web_extension_context_permission_status_for_uri(context.get(), "https://unknown.com/"), ==, WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_UNKNOWN);
    g_assert_null(webkit_web_extension_context_get_granted_permissions(context.get()));
    g_assert_null(webkit_web_extension_context_get_granted_permission_match_patterns(context.get()));
    g_assert_null(webkit_web_extension_context_get_denied_permissions(context.get()));
    g_assert_null(webkit_web_extension_context_get_denied_permission_match_patterns(context.get()));

    extension = parseExtensionManifest("{ \"manifest_version\": 2, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\", \"permissions\": [ \"tabs\", \"<all_urls>\" ] }");
    g_assert_no_error(error.get());
    context = webkit_web_extension_context_new_for_extension(extension.get(), &error.outPtr());
    g_assert_no_error(error.get());

    g_assert_false(webkit_web_extension_context_has_permission(context.get(), "tabs"));
    g_assert_false(webkit_web_extension_context_has_permission(context.get(), "cookies"));
    g_assert_false(webkit_web_extension_context_get_has_access_to_all_uris(context.get()));
    g_assert_false(webkit_web_extension_context_get_has_access_to_all_hosts(context.get()));
    g_assert_false(webkit_web_extension_context_has_access_to_uri(context.get(), "https://example.com/"));
    g_assert_false(webkit_web_extension_context_has_access_to_uri(context.get(), "https://webkit.org"));
    g_assert_cmpint(webkit_web_extension_context_permission_status_for_permission(context.get(), "tabs"), ==, WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_REQUESTED_EXPLICITLY);
    g_assert_cmpint(webkit_web_extension_context_permission_status_for_permission(context.get(), "cookies"), ==, WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_UNKNOWN);
    g_assert_cmpint(webkit_web_extension_context_permission_status_for_uri(context.get(), "https://example.com/"), ==, WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_REQUESTED_IMPLICITLY);
    g_assert_cmpint(webkit_web_extension_context_permission_status_for_uri(context.get(), "https://webkit.org/"), ==, WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_REQUESTED_IMPLICITLY);
    g_assert_cmpint(webkit_web_extension_context_permission_status_for_uri(context.get(), "https://unknown.com/"), ==, WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_REQUESTED_IMPLICITLY);
    g_assert_null(webkit_web_extension_context_get_granted_permissions(context.get()));
    g_assert_null(webkit_web_extension_context_get_granted_permission_match_patterns(context.get()));
    g_assert_null(webkit_web_extension_context_get_denied_permissions(context.get()));
    g_assert_null(webkit_web_extension_context_get_denied_permission_match_patterns(context.get()));

    extension = parseExtensionManifest("{ \"manifest_version\": 2, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\", \"permissions\": [ \"tabs\", \"*://*/*\" ] }");
    g_assert_no_error(error.get());
    context = webkit_web_extension_context_new_for_extension(extension.get(), &error.outPtr());
    g_assert_no_error(error.get());

    g_assert_false(webkit_web_extension_context_has_permission(context.get(), "tabs"));
    g_assert_false(webkit_web_extension_context_has_permission(context.get(), "cookies"));
    g_assert_false(webkit_web_extension_context_get_has_access_to_all_uris(context.get()));
    g_assert_false(webkit_web_extension_context_get_has_access_to_all_hosts(context.get()));
    g_assert_false(webkit_web_extension_context_has_access_to_uri(context.get(), "https://example.com/"));
    g_assert_false(webkit_web_extension_context_has_access_to_uri(context.get(), "https://webkit.org"));
    g_assert_cmpint(webkit_web_extension_context_permission_status_for_permission(context.get(), "tabs"), ==, WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_REQUESTED_EXPLICITLY);
    g_assert_cmpint(webkit_web_extension_context_permission_status_for_permission(context.get(), "cookies"), ==, WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_UNKNOWN);
    g_assert_cmpint(webkit_web_extension_context_permission_status_for_uri(context.get(), "https://example.com/"), ==, WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_REQUESTED_IMPLICITLY);
    g_assert_cmpint(webkit_web_extension_context_permission_status_for_uri(context.get(), "https://webkit.org/"), ==, WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_REQUESTED_IMPLICITLY);
    g_assert_cmpint(webkit_web_extension_context_permission_status_for_uri(context.get(), "https://unknown.com/"), ==, WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_REQUESTED_IMPLICITLY);
    g_assert_null(webkit_web_extension_context_get_granted_permissions(context.get()));
    g_assert_null(webkit_web_extension_context_get_granted_permission_match_patterns(context.get()));
    g_assert_null(webkit_web_extension_context_get_denied_permissions(context.get()));
    g_assert_null(webkit_web_extension_context_get_denied_permission_match_patterns(context.get()));

    extension = parseExtensionManifest("{ \"manifest_version\": 3, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\", \"permissions\": [ ], \"host_permissions\": [ ] }");
    g_assert_no_error(error.get());
    context = webkit_web_extension_context_new_for_extension(extension.get(), &error.outPtr());
    g_assert_no_error(error.get());

    g_assert_false(webkit_web_extension_context_has_permission(context.get(), "tabs"));
    g_assert_false(webkit_web_extension_context_has_permission(context.get(), "cookies"));
    g_assert_false(webkit_web_extension_context_get_has_access_to_all_uris(context.get()));
    g_assert_false(webkit_web_extension_context_get_has_access_to_all_hosts(context.get()));
    g_assert_false(webkit_web_extension_context_has_access_to_uri(context.get(), "https://example.com/"));
    g_assert_false(webkit_web_extension_context_has_access_to_uri(context.get(), "https://webkit.org"));
    g_assert_cmpint(webkit_web_extension_context_permission_status_for_permission(context.get(), "tabs"), ==, WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_UNKNOWN);
    g_assert_cmpint(webkit_web_extension_context_permission_status_for_permission(context.get(), "cookies"), ==, WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_UNKNOWN);
    g_assert_cmpint(webkit_web_extension_context_permission_status_for_uri(context.get(), "https://example.com/"), ==, WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_UNKNOWN);
    g_assert_cmpint(webkit_web_extension_context_permission_status_for_uri(context.get(), "https://webkit.org/"), ==, WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_UNKNOWN);
    g_assert_cmpint(webkit_web_extension_context_permission_status_for_uri(context.get(), "https://unknown.com/"), ==, WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_UNKNOWN);
    g_assert_null(webkit_web_extension_context_get_granted_permissions(context.get()));
    g_assert_null(webkit_web_extension_context_get_granted_permission_match_patterns(context.get()));
    g_assert_null(webkit_web_extension_context_get_denied_permissions(context.get()));
    g_assert_null(webkit_web_extension_context_get_denied_permission_match_patterns(context.get()));

    extension = parseExtensionManifest("{ \"manifest_version\": 3, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\", \"permissions\": [ \"tabs\" ], \"host_permissions\": [ \"https://*.example.com/*\" ] }");
    g_assert_no_error(error.get());
    context = webkit_web_extension_context_new_for_extension(extension.get(), &error.outPtr());
    g_assert_no_error(error.get());

    g_assert_false(webkit_web_extension_context_has_permission(context.get(), "tabs"));
    g_assert_false(webkit_web_extension_context_has_permission(context.get(), "cookies"));
    g_assert_false(webkit_web_extension_context_get_has_access_to_all_uris(context.get()));
    g_assert_false(webkit_web_extension_context_get_has_access_to_all_hosts(context.get()));
    g_assert_false(webkit_web_extension_context_has_access_to_uri(context.get(), "https://example.com/"));
    g_assert_false(webkit_web_extension_context_has_access_to_uri(context.get(), "https://webkit.org"));
    g_assert_cmpint(webkit_web_extension_context_permission_status_for_permission(context.get(), "tabs"), ==, WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_REQUESTED_EXPLICITLY);
    g_assert_cmpint(webkit_web_extension_context_permission_status_for_permission(context.get(), "cookies"), ==, WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_UNKNOWN);
    g_assert_cmpint(webkit_web_extension_context_permission_status_for_uri(context.get(), "https://example.com/"), ==, WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_REQUESTED_EXPLICITLY);
    g_assert_cmpint(webkit_web_extension_context_permission_status_for_uri(context.get(), "https://webkit.org/"), ==, WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_UNKNOWN);
    g_assert_cmpint(webkit_web_extension_context_permission_status_for_uri(context.get(), "https://unknown.com/"), ==, WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_UNKNOWN);
    g_assert_null(webkit_web_extension_context_get_granted_permissions(context.get()));
    g_assert_null(webkit_web_extension_context_get_granted_permission_match_patterns(context.get()));
    g_assert_null(webkit_web_extension_context_get_denied_permissions(context.get()));
    g_assert_null(webkit_web_extension_context_get_denied_permission_match_patterns(context.get()));

    extension = parseExtensionManifest("{ \"manifest_version\": 3, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\", \"permissions\": [ \"tabs\" ], \"host_permissions\": [ \"<all_urls>\" ] }");
    g_assert_no_error(error.get());
    context = webkit_web_extension_context_new_for_extension(extension.get(), &error.outPtr());
    g_assert_no_error(error.get());

    g_assert_false(webkit_web_extension_context_has_permission(context.get(), "tabs"));
    g_assert_false(webkit_web_extension_context_has_permission(context.get(), "cookies"));
    g_assert_false(webkit_web_extension_context_get_has_access_to_all_uris(context.get()));
    g_assert_false(webkit_web_extension_context_get_has_access_to_all_hosts(context.get()));
    g_assert_false(webkit_web_extension_context_has_access_to_uri(context.get(), "https://example.com/"));
    g_assert_false(webkit_web_extension_context_has_access_to_uri(context.get(), "https://webkit.org"));
    g_assert_cmpint(webkit_web_extension_context_permission_status_for_permission(context.get(), "tabs"), ==, WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_REQUESTED_EXPLICITLY);
    g_assert_cmpint(webkit_web_extension_context_permission_status_for_permission(context.get(), "cookies"), ==, WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_UNKNOWN);
    g_assert_cmpint(webkit_web_extension_context_permission_status_for_uri(context.get(), "https://example.com/"), ==, WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_REQUESTED_IMPLICITLY);
    g_assert_cmpint(webkit_web_extension_context_permission_status_for_uri(context.get(), "https://webkit.org/"), ==, WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_REQUESTED_IMPLICITLY);
    g_assert_cmpint(webkit_web_extension_context_permission_status_for_uri(context.get(), "https://unknown.com/"), ==, WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_REQUESTED_IMPLICITLY);
    g_assert_null(webkit_web_extension_context_get_granted_permissions(context.get()));
    g_assert_null(webkit_web_extension_context_get_granted_permission_match_patterns(context.get()));
    g_assert_null(webkit_web_extension_context_get_denied_permissions(context.get()));
    g_assert_null(webkit_web_extension_context_get_denied_permission_match_patterns(context.get()));

    extension = parseExtensionManifest("{ \"manifest_version\": 3, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\", \"permissions\": [ \"tabs\" ], \"host_permissions\": [ \"*://*/*\" ] }");
    g_assert_no_error(error.get());
    context = webkit_web_extension_context_new_for_extension(extension.get(), &error.outPtr());
    g_assert_no_error(error.get());

    g_assert_false(webkit_web_extension_context_has_permission(context.get(), "tabs"));
    g_assert_false(webkit_web_extension_context_has_permission(context.get(), "cookies"));
    g_assert_false(webkit_web_extension_context_get_has_access_to_all_uris(context.get()));
    g_assert_false(webkit_web_extension_context_get_has_access_to_all_hosts(context.get()));
    g_assert_false(webkit_web_extension_context_has_access_to_uri(context.get(), "https://example.com/"));
    g_assert_false(webkit_web_extension_context_has_access_to_uri(context.get(), "https://webkit.org"));
    g_assert_cmpint(webkit_web_extension_context_permission_status_for_permission(context.get(), "tabs"), ==, WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_REQUESTED_EXPLICITLY);
    g_assert_cmpint(webkit_web_extension_context_permission_status_for_permission(context.get(), "cookies"), ==, WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_UNKNOWN);
    g_assert_cmpint(webkit_web_extension_context_permission_status_for_uri(context.get(), "https://example.com/"), ==, WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_REQUESTED_IMPLICITLY);
    g_assert_cmpint(webkit_web_extension_context_permission_status_for_uri(context.get(), "https://webkit.org/"), ==, WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_REQUESTED_IMPLICITLY);
    g_assert_cmpint(webkit_web_extension_context_permission_status_for_uri(context.get(), "https://unknown.com/"), ==, WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_REQUESTED_IMPLICITLY);
    g_assert_null(webkit_web_extension_context_get_granted_permissions(context.get()));
    g_assert_null(webkit_web_extension_context_get_granted_permission_match_patterns(context.get()));
    g_assert_null(webkit_web_extension_context_get_denied_permissions(context.get()));
    g_assert_null(webkit_web_extension_context_get_denied_permission_match_patterns(context.get()));
}

static void testPermissionGranting(Test*, gconstpointer)
{
    GUniqueOutPtr<GError> error;
    auto parseExtensionManifest = [&](const gchar* manifestString) {
        return adoptGRef(webkitWebExtensionCreate({ { "manifest.json"_s, createGBytes(manifestString) } }, &error.outPtr()));
    };

    auto getArraySize = [&](auto array) {
        guint arrayLength = 0;
        for (; *array != nullptr; array++)
            arrayLength++;
        return arrayLength;
    };

    GRefPtr<WebKitWebExtension> extension = parseExtensionManifest("{ \"manifest_version\": 2, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\", \"permissions\": [ \"tabs\", \"https://*.example.com/*\" ] }");
    g_assert_no_error(error.get());
    GRefPtr<WebKitWebExtensionContext> context = webkit_web_extension_context_new_for_extension(extension.get(), &error.outPtr());
    g_assert_no_error(error.get());

    g_assert_false(webkit_web_extension_context_has_permission(context.get(), "tabs"));
    g_assert_false(webkit_web_extension_context_has_permission(context.get(), "cookies"));
    g_assert_false(webkit_web_extension_context_get_has_access_to_all_uris(context.get()));
    g_assert_false(webkit_web_extension_context_get_has_access_to_all_hosts(context.get()));
    g_assert_false(webkit_web_extension_context_has_access_to_uri(context.get(), "https://example.com/"));
    g_assert_false(webkit_web_extension_context_has_access_to_uri(context.get(), "https://webkit.org"));
    g_assert_cmpint(webkit_web_extension_context_permission_status_for_permission(context.get(), "tabs"), ==, WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_REQUESTED_EXPLICITLY);
    g_assert_cmpint(webkit_web_extension_context_permission_status_for_permission(context.get(), "cookies"), ==, WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_UNKNOWN);
    g_assert_cmpint(webkit_web_extension_context_permission_status_for_uri(context.get(), "https://example.com/"), ==, WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_REQUESTED_EXPLICITLY);
    g_assert_cmpint(webkit_web_extension_context_permission_status_for_uri(context.get(), "https://webkit.org/"), ==, WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_UNKNOWN);
    g_assert_null(webkit_web_extension_context_get_granted_permissions(context.get()));
    g_assert_null(webkit_web_extension_context_get_granted_permission_match_patterns(context.get()));
    g_assert_null(webkit_web_extension_context_get_denied_permissions(context.get()));
    g_assert_null(webkit_web_extension_context_get_denied_permission_match_patterns(context.get()));

    // Grant a specific permission
    webkit_web_extension_context_set_permission_status_for_permission(context.get(), "tabs", WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_GRANTED_EXPLICITLY, nullptr);

    g_assert_true(webkit_web_extension_context_has_permission(context.get(), "tabs"));
    g_assert_cmpint(getArraySize(webkit_web_extension_context_get_granted_permissions(context.get())), ==, 1);

    // Grant a specific URI
    webkit_web_extension_context_set_permission_status_for_uri(context.get(), "https://example.com/", WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_GRANTED_EXPLICITLY, nullptr);

    g_assert_true(webkit_web_extension_context_has_access_to_uri(context.get(), "https://example.com/"));
    g_assert_cmpint(getArraySize(webkit_web_extension_context_get_granted_permission_match_patterns(context.get())), ==, 1);

    // Deny a specific URI
    webkit_web_extension_context_set_permission_status_for_uri(context.get(), "https://example.com/", WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_DENIED_EXPLICITLY, nullptr);

    g_assert_false(webkit_web_extension_context_has_access_to_uri(context.get(), "https://example.com/"));
    g_assert_null(webkit_web_extension_context_get_granted_permission_match_patterns(context.get()));
    g_assert_cmpint(getArraySize(webkit_web_extension_context_get_denied_permission_match_patterns(context.get())), ==, 1);

    // Deny a specific permission
    webkit_web_extension_context_set_permission_status_for_permission(context.get(), "tabs", WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_DENIED_EXPLICITLY, nullptr);
    g_assert_null(webkit_web_extension_context_get_granted_permissions(context.get()));
    g_assert_cmpint(getArraySize(webkit_web_extension_context_get_denied_permissions(context.get())), ==, 1);

    // Reset all permissions
    webkit_web_extension_context_set_permission_status_for_uri(context.get(), "https://example.com/", WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_UNKNOWN, nullptr);
    webkit_web_extension_context_set_permission_status_for_permission(context.get(), "tabs", WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_UNKNOWN, nullptr);

    g_assert_null(webkit_web_extension_context_get_granted_permissions(context.get()));
    g_assert_null(webkit_web_extension_context_get_granted_permission_match_patterns(context.get()));
    g_assert_null(webkit_web_extension_context_get_denied_permissions(context.get()));
    g_assert_null(webkit_web_extension_context_get_denied_permission_match_patterns(context.get()));

    // Grant the all URLs match pattern.
    webkit_web_extension_context_set_permission_status_for_match_pattern(context.get(), webkit_web_extension_match_pattern_new_all_urls(), WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_GRANTED_EXPLICITLY, nullptr);

    g_assert_cmpint(getArraySize(webkit_web_extension_context_get_granted_permission_match_patterns(context.get())), ==, 1);
    g_assert_true(webkit_web_extension_context_has_access_to_uri(context.get(), "https://example.com/"));
    g_assert_cmpint(webkit_web_extension_context_permission_status_for_uri(context.get(), "https://example.com/"), ==, WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_GRANTED_IMPLICITLY);
    g_assert_cmpint(webkit_web_extension_context_permission_status_for_uri(context.get(), "https://webkit.org/"), ==, WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_GRANTED_IMPLICITLY);

    // Reset a specific URL (should do nothing).
    webkit_web_extension_context_set_permission_status_for_uri(context.get(), "https://example.com/", WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_UNKNOWN, nullptr);

    g_assert_cmpint(getArraySize(webkit_web_extension_context_get_granted_permission_match_patterns(context.get())), ==, 1);
    g_assert_true(webkit_web_extension_context_has_access_to_uri(context.get(), "https://example.com/"));
    g_assert_cmpint(webkit_web_extension_context_permission_status_for_uri(context.get(), "https://example.com/"), ==, WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_GRANTED_IMPLICITLY);

    // Deny a specific URL.
    webkit_web_extension_context_set_permission_status_for_uri(context.get(), "https://example.com/", WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_DENIED_EXPLICITLY, nullptr);

    g_assert_cmpint(getArraySize(webkit_web_extension_context_get_granted_permission_match_patterns(context.get())), ==, 1);
    g_assert_cmpint(getArraySize(webkit_web_extension_context_get_denied_permission_match_patterns(context.get())), ==, 1);
    g_assert_false(webkit_web_extension_context_has_access_to_uri(context.get(), "https://example.com/"));
    g_assert_cmpint(webkit_web_extension_context_permission_status_for_uri(context.get(), "https://example.com/"), ==, WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_DENIED_EXPLICITLY);
    g_assert_cmpint(webkit_web_extension_context_permission_status_for_uri(context.get(), "https://webkit.org/"), ==, WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_GRANTED_IMPLICITLY);

    // Reset all match patterns
    webkit_web_extension_context_set_granted_permission_match_patterns(context.get(), nullptr);
    webkit_web_extension_context_set_denied_permission_match_patterns(context.get(), nullptr);

    g_assert_null(webkit_web_extension_context_get_granted_permission_match_patterns(context.get()));
    g_assert_null(webkit_web_extension_context_get_denied_permission_match_patterns(context.get()));

    // Mass grant with the permission setter
    std::array<WebKitWebExtensionContextPermission*, 2> grantedAndDeniedPermissions { { webkit_web_extension_context_permission_new("tabs", nullptr), nullptr } };
    webkit_web_extension_context_set_granted_permissions(context.get(), grantedAndDeniedPermissions.data());

    g_assert_true(webkit_web_extension_context_has_permission(context.get(), "tabs"));
    g_assert_cmpint(getArraySize(webkit_web_extension_context_get_granted_permissions(context.get())), ==, 1);

    // Mass deny with the permission setter
    webkit_web_extension_context_set_denied_permissions(context.get(), grantedAndDeniedPermissions.data());

    g_assert_false(webkit_web_extension_context_has_permission(context.get(), "tabs"));
    g_assert_cmpint(getArraySize(webkit_web_extension_context_get_denied_permissions(context.get())), ==, 1);
    g_assert_null(webkit_web_extension_context_get_granted_permissions(context.get()));

    // Mass grant with the permission setter again
    webkit_web_extension_context_set_granted_permissions(context.get(), grantedAndDeniedPermissions.data());

    g_assert_true(webkit_web_extension_context_has_permission(context.get(), "tabs"));
    g_assert_cmpint(getArraySize(webkit_web_extension_context_get_granted_permissions(context.get())), ==, 1);
    g_assert_null(webkit_web_extension_context_get_denied_permissions(context.get()));

    // Mass grant with the match pattern setter
    std::array<WebKitWebExtensionContextMatchPattern*, 2> grantedAndDeniedPermissionMatchPatterns { { webkit_web_extension_context_match_pattern_new(webkit_web_extension_match_pattern_new_all_urls(), nullptr), nullptr } };
    webkit_web_extension_context_set_granted_permission_match_patterns(context.get(), grantedAndDeniedPermissionMatchPatterns.data());

    g_assert_true(webkit_web_extension_context_get_has_access_to_all_uris(context.get()));
    g_assert_true(webkit_web_extension_context_has_access_to_uri(context.get(), "https://example.com/"));
    g_assert_cmpint(webkit_web_extension_context_permission_status_for_uri(context.get(), "https://example.com/"), ==, WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_GRANTED_IMPLICITLY);
    g_assert_cmpint(getArraySize(webkit_web_extension_context_get_granted_permission_match_patterns(context.get())), ==, 1);
    g_assert_null(webkit_web_extension_context_get_denied_permission_match_patterns(context.get()));

    // Mass deny with the match pattern setter
    webkit_web_extension_context_set_denied_permission_match_patterns(context.get(), grantedAndDeniedPermissionMatchPatterns.data());

    g_assert_false(webkit_web_extension_context_get_has_access_to_all_uris(context.get()));
    g_assert_false(webkit_web_extension_context_has_access_to_uri(context.get(), "https://example.com/"));
    g_assert_cmpint(getArraySize(webkit_web_extension_context_get_denied_permission_match_patterns(context.get())), ==, 1);
    g_assert_null(webkit_web_extension_context_get_granted_permission_match_patterns(context.get()));

    // Mass grant with the match pattern setter again
    webkit_web_extension_context_set_granted_permission_match_patterns(context.get(), grantedAndDeniedPermissionMatchPatterns.data());

    g_assert_true(webkit_web_extension_context_get_has_access_to_all_uris(context.get()));
    g_assert_true(webkit_web_extension_context_has_access_to_uri(context.get(), "https://example.com/"));
    g_assert_cmpint(webkit_web_extension_context_permission_status_for_uri(context.get(), "https://example.com/"), ==, WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_GRANTED_IMPLICITLY);
    g_assert_cmpint(getArraySize(webkit_web_extension_context_get_granted_permission_match_patterns(context.get())), ==, 1);
    g_assert_null(webkit_web_extension_context_get_denied_permission_match_patterns(context.get()));

    // Reset all permissions
    webkit_web_extension_context_set_granted_permissions(context.get(), nullptr);
    webkit_web_extension_context_set_granted_permission_match_patterns(context.get(), nullptr);
    webkit_web_extension_context_set_denied_permissions(context.get(), nullptr);
    webkit_web_extension_context_set_denied_permission_match_patterns(context.get(), nullptr);

    g_assert_null(webkit_web_extension_context_get_granted_permissions(context.get()));
    g_assert_null(webkit_web_extension_context_get_granted_permission_match_patterns(context.get()));
    g_assert_null(webkit_web_extension_context_get_denied_permissions(context.get()));
    g_assert_null(webkit_web_extension_context_get_denied_permission_match_patterns(context.get()));

    // Test granting a match pattern that expire in 1 second.
    auto expirationDate = g_date_time_new_now_local();
    expirationDate = g_date_time_add_seconds(expirationDate, 1);
    webkit_web_extension_context_set_permission_status_for_match_pattern(context.get(), webkit_web_extension_match_pattern_new_all_urls(), WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_GRANTED_EXPLICITLY, expirationDate);

    g_assert_true(webkit_web_extension_context_get_has_access_to_all_uris(context.get()));
    g_assert_true(webkit_web_extension_context_has_access_to_uri(context.get(), "https://example.com/"));
    g_assert_cmpint(webkit_web_extension_context_permission_status_for_uri(context.get(), "https://example.com/"), ==, WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_GRANTED_IMPLICITLY);
    g_assert_cmpint(getArraySize(webkit_web_extension_context_get_granted_permission_match_patterns(context.get())), ==, 1);

    // Sleep until after the match pattern expires.
    g_usleep(2 * G_USEC_PER_SEC);

    g_assert_false(webkit_web_extension_context_get_has_access_to_all_uris(context.get()));
    g_assert_false(webkit_web_extension_context_has_access_to_uri(context.get(), "https://example.com/"));
    g_assert_cmpint(webkit_web_extension_context_permission_status_for_uri(context.get(), "https://example.com/"), ==, WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_REQUESTED_EXPLICITLY);
    g_assert_null(webkit_web_extension_context_get_granted_permission_match_patterns(context.get()));

    // Test granting a permission that expires in 1 second
    expirationDate = g_date_time_new_now_local();
    expirationDate = g_date_time_add_seconds(expirationDate, 1);
    webkit_web_extension_context_set_permission_status_for_permission(context.get(), "tabs", WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_GRANTED_EXPLICITLY, expirationDate);

    g_assert_true(webkit_web_extension_context_has_permission(context.get(), "tabs"));
    g_assert_cmpint(getArraySize(webkit_web_extension_context_get_granted_permissions(context.get())), ==, 1);

    // Sleep until after the permission expires.
    g_usleep(2 * G_USEC_PER_SEC);

    g_assert_false(webkit_web_extension_context_has_permission(context.get(), "tabs"));
    g_assert_null(webkit_web_extension_context_get_granted_permissions(context.get()));
}

static void testContentScriptsParsing(Test*, gconstpointer)
{
    GUniqueOutPtr<GError> error;
    auto parseExtensionManifest = [&](const gchar* contentScripts) {
        auto manifestString = makeString("{ \"manifest_version\": 2, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\", \"content_scripts\": "_s, String::fromUTF8(contentScripts), " }"_s);

        return adoptGRef(webkitWebExtensionCreate({ { "manifest.json"_s, createGBytes(manifestString.utf8().data()) } }, &error.outPtr()));
    };

    GRefPtr<WebKitWebExtension> extension = parseExtensionManifest("[{ \"js\": [ \"test.js\", 1, \"\" ], \"css\": [ false, \"test.css\", \"\" ], \"matches\": [ \"*://*/\" ] }]");
    g_assert_no_error(error.get());
    GRefPtr<WebKitWebExtensionContext> context = webkit_web_extension_context_new_for_extension(extension.get(), &error.outPtr());
    g_assert_no_error(error.get());

    g_assert_true(webkit_web_extension_context_get_has_injected_content(context.get()));
    g_assert_true(webkit_web_extension_context_has_injected_content_for_uri(context.get(), "https://webkit.org/"));
    g_assert_true(webkit_web_extension_context_has_injected_content_for_uri(context.get(), "https://example.com/"));

    extension = parseExtensionManifest("[{ \"js\": [ \"test.js\", 1, \"\" ], \"css\": [ false, \"test.css\", \"\" ], \"matches\": [ \"*://*/\" ], \"exclude_matches\": [ \"*://*.example.com/\" ] }]");
    g_assert_no_error(error.get());
    context = webkit_web_extension_context_new_for_extension(extension.get(), &error.outPtr());
    g_assert_no_error(error.get());

    g_assert_true(webkit_web_extension_context_get_has_injected_content(context.get()));
    g_assert_true(webkit_web_extension_context_has_injected_content_for_uri(context.get(), "https://webkit.org/"));
    g_assert_false(webkit_web_extension_context_has_injected_content_for_uri(context.get(), "https://example.com/"));

    extension = parseExtensionManifest("[{ \"js\": [ \"test.js\", 1, \"\" ], \"css\": [ false, \"test.css\", \"\" ], \"matches\": [ \"*://*.example.com/\" ] }]");
    g_assert_no_error(error.get());
    context = webkit_web_extension_context_new_for_extension(extension.get(), &error.outPtr());
    g_assert_no_error(error.get());

    g_assert_true(webkit_web_extension_context_get_has_injected_content(context.get()));
    g_assert_false(webkit_web_extension_context_has_injected_content_for_uri(context.get(), "https://webkit.org/"));
    g_assert_true(webkit_web_extension_context_has_injected_content_for_uri(context.get(), "https://example.com/"));

    extension = parseExtensionManifest("[{ \"css\": [ false, \"test.css\", \"\" ], \"css_origin\": \"user\", \"matches\": [ \"*://*.example.com/\" ] }]");
    g_assert_no_error(error.get());
    context = webkit_web_extension_context_new_for_extension(extension.get(), &error.outPtr());
    g_assert_no_error(error.get());

    g_assert_true(webkit_web_extension_context_get_has_injected_content(context.get()));
    g_assert_false(webkit_web_extension_context_has_injected_content_for_uri(context.get(), "https://webkit.org/"));
    g_assert_true(webkit_web_extension_context_has_injected_content_for_uri(context.get(), "https://example.com/"));

    extension = parseExtensionManifest("[{ \"css\": [ false, \"test.css\", \"\" ], \"css_origin\": \"author\", \"matches\": [ \"*://*.example.com/\" ] }]");
    g_assert_no_error(error.get());
    context = webkit_web_extension_context_new_for_extension(extension.get(), &error.outPtr());
    g_assert_no_error(error.get());

    g_assert_true(webkit_web_extension_context_get_has_injected_content(context.get()));
    g_assert_false(webkit_web_extension_context_has_injected_content_for_uri(context.get(), "https://webkit.org/"));
    g_assert_true(webkit_web_extension_context_has_injected_content_for_uri(context.get(), "https://example.com/"));

    // Invalid cases

    extension = parseExtensionManifest("[]");
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);
    context = webkit_web_extension_context_new_for_extension(extension.get(), &error.outPtr());
    g_assert_no_error(error.get());

    g_assert_false(webkit_web_extension_context_get_has_injected_content(context.get()));
    g_assert_false(webkit_web_extension_context_has_injected_content_for_uri(context.get(), "https://webkit.org/"));
    g_assert_false(webkit_web_extension_context_has_injected_content_for_uri(context.get(), "https://example.com/"));

    extension = parseExtensionManifest("{ \"invalid\": true }");
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);
    context = webkit_web_extension_context_new_for_extension(extension.get(), &error.outPtr());
    g_assert_no_error(error.get());

    g_assert_false(webkit_web_extension_context_get_has_injected_content(context.get()));
    g_assert_false(webkit_web_extension_context_has_injected_content_for_uri(context.get(), "https://webkit.org/"));
    g_assert_false(webkit_web_extension_context_has_injected_content_for_uri(context.get(), "https://example.com/"));

    extension = parseExtensionManifest("[{ \"js\": [ \"test.js\" ], \"matches\": [] }]");
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);
    context = webkit_web_extension_context_new_for_extension(extension.get(), &error.outPtr());
    g_assert_no_error(error.get());

    g_assert_false(webkit_web_extension_context_get_has_injected_content(context.get()));
    g_assert_false(webkit_web_extension_context_has_injected_content_for_uri(context.get(), "https://webkit.org/"));
    g_assert_false(webkit_web_extension_context_has_injected_content_for_uri(context.get(), "https://example.com/"));

    extension = parseExtensionManifest("[{ \"js\": [ \"test.js\" ], \"matches\": [ \"*://*.example.com/\" ], \"world\": \"INVALID\" }]");
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);
    context = webkit_web_extension_context_new_for_extension(extension.get(), &error.outPtr());
    g_assert_no_error(error.get());

    g_assert_true(webkit_web_extension_context_get_has_injected_content(context.get()));
    g_assert_false(webkit_web_extension_context_has_injected_content_for_uri(context.get(), "https://webkit.org/"));
    g_assert_true(webkit_web_extension_context_has_injected_content_for_uri(context.get(), "https://example.com/"));

    extension = parseExtensionManifest("[{ \"css\": [ false, \"test.css\", \"\" ], \"css_origin\": \"bad\", \"matches\": [ \"*://*.example.com/\" ] }]");
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);
    context = webkit_web_extension_context_new_for_extension(extension.get(), &error.outPtr());
    g_assert_no_error(error.get());

    g_assert_true(webkit_web_extension_context_get_has_injected_content(context.get()));
    g_assert_false(webkit_web_extension_context_has_injected_content_for_uri(context.get(), "https://webkit.org/"));
    g_assert_true(webkit_web_extension_context_has_injected_content_for_uri(context.get(), "https://example.com/"));
}

static void testOptionsPageURIParsing(Test*, gconstpointer)
{
    GUniqueOutPtr<GError> error;
    auto parseExtensionManifest = [&](const gchar* manifestString) {
        return adoptGRef(webkitWebExtensionCreate({ { "manifest.json"_s, createGBytes(manifestString) } }, &error.outPtr()));
    };

    GRefPtr<WebKitWebExtension> extension = parseExtensionManifest("{ \"options_page\": \"options.html\", \"manifest_version\": 3, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\" }");
    g_assert_no_error(error.get());
    GRefPtr<WebKitWebExtensionContext> context = webkit_web_extension_context_new_for_extension(extension.get(), &error.outPtr());
    g_assert_no_error(error.get());

    g_assert_cmpstr(webkit_web_extension_context_get_options_page_uri(context.get()), ==, g_strconcat(webkit_web_extension_context_get_base_uri(context.get()), "options.html", nullptr));

    extension = parseExtensionManifest("{ \"options_page\": 123, \"manifest_version\": 3, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\" }");
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);
    context = webkit_web_extension_context_new_for_extension(extension.get(), &error.outPtr());
    g_assert_no_error(error.get());

    g_assert_null(webkit_web_extension_context_get_options_page_uri(context.get()));

    extension = parseExtensionManifest("{ \"options_ui\": { \"page\": \"options.html\" }, \"manifest_version\": 3, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\" }");
    g_assert_no_error(error.get());
    context = webkit_web_extension_context_new_for_extension(extension.get(), &error.outPtr());
    g_assert_no_error(error.get());

    g_assert_cmpstr(webkit_web_extension_context_get_options_page_uri(context.get()), ==, g_strconcat(webkit_web_extension_context_get_base_uri(context.get()), "options.html", nullptr));

    extension = parseExtensionManifest("{ \"options_ui\": { \"page\": 123 }, \"manifest_version\": 3, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\" }");
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);
    context = webkit_web_extension_context_new_for_extension(extension.get(), &error.outPtr());
    g_assert_no_error(error.get());

    g_assert_null(webkit_web_extension_context_get_options_page_uri(context.get()));

    extension = parseExtensionManifest("{ \"options_page\": \"\", \"manifest_version\": 3, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\" }");
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);
    context = webkit_web_extension_context_new_for_extension(extension.get(), &error.outPtr());
    g_assert_no_error(error.get());

    g_assert_null(webkit_web_extension_context_get_options_page_uri(context.get()));

    extension = parseExtensionManifest("{ \"options_ui\": { }, \"manifest_version\": 3, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\" }");
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);
    context = webkit_web_extension_context_new_for_extension(extension.get(), &error.outPtr());
    g_assert_no_error(error.get());

    g_assert_null(webkit_web_extension_context_get_options_page_uri(context.get()));
}

static void testURIOverridesParsing(Test*, gconstpointer)
{
    GUniqueOutPtr<GError> error;
    auto parseExtensionManifest = [&](const gchar* manifestString) {
        return adoptGRef(webkitWebExtensionCreate({ { "manifest.json"_s, createGBytes(manifestString) } }, &error.outPtr()));
    };

    GRefPtr<WebKitWebExtension> extension = parseExtensionManifest("{ \"browser_url_overrides\": { \"newtab\": \"newtab.html\" }, \"manifest_version\": 3, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\" }");
    g_assert_no_error(error.get());
    GRefPtr<WebKitWebExtensionContext> context = webkit_web_extension_context_new_for_extension(extension.get(), &error.outPtr());
    g_assert_no_error(error.get());

    g_assert_cmpstr(webkit_web_extension_context_get_override_new_tab_page_uri(context.get()), ==, g_strconcat(webkit_web_extension_context_get_base_uri(context.get()), "newtab.html", nullptr));

    extension = parseExtensionManifest("{ \"browser_url_overrides\": { \"newtab\": 123 }, \"manifest_version\": 3, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\" }");
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);
    context = webkit_web_extension_context_new_for_extension(extension.get(), &error.outPtr());
    g_assert_no_error(error.get());

    g_assert_null(webkit_web_extension_context_get_override_new_tab_page_uri(context.get()));

    extension = parseExtensionManifest("{ \"browser_url_overrides\": { }, \"manifest_version\": 3, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\" }");
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);
    context = webkit_web_extension_context_new_for_extension(extension.get(), &error.outPtr());
    g_assert_no_error(error.get());

    g_assert_null(webkit_web_extension_context_get_override_new_tab_page_uri(context.get()));

    extension = parseExtensionManifest("{ \"browser_url_overrides\": { \"newtab\": \"\" }, \"manifest_version\": 3, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\" }");
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);
    context = webkit_web_extension_context_new_for_extension(extension.get(), &error.outPtr());
    g_assert_no_error(error.get());

    g_assert_null(webkit_web_extension_context_get_override_new_tab_page_uri(context.get()));

    extension = parseExtensionManifest("{ \"chrome_url_overrides\": { \"newtab\": \"newtab.html\" }, \"manifest_version\": 3, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\" }");
    g_assert_no_error(error.get());
    context = webkit_web_extension_context_new_for_extension(extension.get(), &error.outPtr());
    g_assert_no_error(error.get());

    g_assert_cmpstr(webkit_web_extension_context_get_override_new_tab_page_uri(context.get()), ==, g_strconcat(webkit_web_extension_context_get_base_uri(context.get()), "newtab.html", nullptr));

    extension = parseExtensionManifest("{ \"chrome_url_overrides\": { \"newtab\": 123 }, \"manifest_version\": 3, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\" }");
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);
    context = webkit_web_extension_context_new_for_extension(extension.get(), &error.outPtr());
    g_assert_no_error(error.get());

    g_assert_null(webkit_web_extension_context_get_override_new_tab_page_uri(context.get()));

    extension = parseExtensionManifest("{ \"chrome_url_overrides\": { }, \"manifest_version\": 3, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\" }");
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);
    context = webkit_web_extension_context_new_for_extension(extension.get(), &error.outPtr());
    g_assert_no_error(error.get());

    g_assert_null(webkit_web_extension_context_get_override_new_tab_page_uri(context.get()));

    extension = parseExtensionManifest("{ \"chrome_url_overrides\": { \"newtab\": \"\" }, \"manifest_version\": 3, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\" }");
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);
    context = webkit_web_extension_context_new_for_extension(extension.get(), &error.outPtr());
    g_assert_no_error(error.get());

    g_assert_null(webkit_web_extension_context_get_override_new_tab_page_uri(context.get()));
}

static void testLoadBackgroundContent(Test*, gconstpointer)
{
    GUniqueOutPtr<GError> error;
    auto parseExtensionManifest = [&](const gchar* manifestString, const gchar* backgroundScript) {
        return adoptGRef(webkitWebExtensionCreate({ { "manifest.json"_s, createGBytes(manifestString) }, { "background.js"_s, createGBytes(backgroundScript) } }, &error.outPtr()));
    };

    GRefPtr<WebKitWebExtension> extension = parseExtensionManifest("{ \"manifest_version\": 3, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\", \"background\": { \"scripts\": [ \"background.js\" ], \"type\": \"module\", \"persistent\": false } }", "const img = new Image(); img.src = 'non-existent-image.png';");
    g_assert_no_error(error.get());
    GRefPtr<WebKitWebExtensionContext> context = webkit_web_extension_context_new_for_extension(extension.get(), &error.outPtr());
    g_assert_no_error(error.get());

    webkit_web_extension_context_load_background_content(context.get(), nullptr, [](GObject* object, GAsyncResult* result, gpointer userData) mutable {
        GUniqueOutPtr<GError> error;
        g_assert_false(webkit_web_extension_context_load_background_content_finish(WEBKIT_WEB_EXTENSION_CONTEXT(object), result, &error.outPtr()));
        g_assert_no_error(error.get());
    }, nullptr);
}

void beforeAll()
{
    Test::add("WebKitWebExtensionContext", "default-permission-checks", testDefaultPermissionChecks);
    Test::add("WebKitWebExtensionContext", "permission-granting", testPermissionGranting);
    Test::add("WebKitWebExtensionContext", "content-scripts-parsing", testContentScriptsParsing);
    Test::add("WebKitWebExtensionContext", "options-page-uri-parsing", testOptionsPageURIParsing);
    Test::add("WebKitWebExtensionContext", "uri-overrides-parsing", testURIOverridesParsing);
    Test::add("WebKitWebExtensionContext", "load-background-content", testLoadBackgroundContent);

    // Some code in WebExtensionContext increases the amount of time alloted to a particular process
    // when running in the Test Runner. Set a consistent Program Name (we can't set an Application ID since the Test Runner doesn't use GApplication)
    // so WebExtensionContext can detect the test runner
#if PLATFORM(WPE)
    g_set_prgname("org.webkit.app-TestWebKitWPE");
#else
    g_set_prgname("org.webkit.app-TestWebKitGTK");
#endif
}

void afterAll()
{
}

#endif // ENABLE(WK_WEB_EXTENSIONS)
