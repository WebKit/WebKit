/*
 * Copyright (C) 2026 Igalia, S.L. All rights reserved.
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

static void testContentScriptsParsing(Test* test, gconstpointer)
{
    GUniqueOutPtr<GError> error;
    auto parseExtensionManifest = [&](const gchar* contentScripts) {
        auto manifestString = makeString("{ \"manifest_version\": 2, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\", \"content_scripts\": "_s, String::fromUTF8(contentScripts), " }"_s);
        GRefPtr extension = adoptGRef(webkitWebExtensionCreate({ { "manifest.json"_s, createGBytes(manifestString.utf8().data()) } }, &error.outPtr()));
        test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(extension.get()));
        return extension;
    };

    GRefPtr<WebKitWebExtension> extension = parseExtensionManifest("[{ \"js\": [ \"test.js\", 1, \"\" ], \"css\": [ false, \"test.css\", \"\" ], \"matches\": [ \"*://*/\" ] }]");
    g_assert_no_error(error.get());
    GRefPtr<WebKitWebExtensionContext> context = adoptGRef(webkit_web_extension_context_new_for_extension(extension.get(), &error.outPtr()));
    g_assert_no_error(error.get());

    g_assert_true(webkit_web_extension_context_get_has_injected_content(context.get()));
    g_assert_true(webkit_web_extension_context_has_injected_content_for_uri(context.get(), "https://webkit.org/"));
    g_assert_true(webkit_web_extension_context_has_injected_content_for_uri(context.get(), "https://example.com/"));

    extension = parseExtensionManifest("[{ \"js\": [ \"test.js\", 1, \"\" ], \"css\": [ false, \"test.css\", \"\" ], \"matches\": [ \"*://*/\" ], \"exclude_matches\": [ \"*://*.example.com/\" ] }]");
    g_assert_no_error(error.get());
    context = adoptGRef(webkit_web_extension_context_new_for_extension(extension.get(), &error.outPtr()));
    test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(context.get()));
    g_assert_no_error(error.get());

    g_assert_true(webkit_web_extension_context_get_has_injected_content(context.get()));
    g_assert_true(webkit_web_extension_context_has_injected_content_for_uri(context.get(), "https://webkit.org/"));
    g_assert_false(webkit_web_extension_context_has_injected_content_for_uri(context.get(), "https://example.com/"));

    extension = parseExtensionManifest("[{ \"js\": [ \"test.js\", 1, \"\" ], \"css\": [ false, \"test.css\", \"\" ], \"matches\": [ \"*://*.example.com/\" ] }]");
    g_assert_no_error(error.get());
    context = adoptGRef(webkit_web_extension_context_new_for_extension(extension.get(), &error.outPtr()));
    test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(context.get()));
    g_assert_no_error(error.get());

    g_assert_true(webkit_web_extension_context_get_has_injected_content(context.get()));
    g_assert_false(webkit_web_extension_context_has_injected_content_for_uri(context.get(), "https://webkit.org/"));
    g_assert_true(webkit_web_extension_context_has_injected_content_for_uri(context.get(), "https://example.com/"));

    extension = parseExtensionManifest("[{ \"css\": [ false, \"test.css\", \"\" ], \"css_origin\": \"user\", \"matches\": [ \"*://*.example.com/\" ] }]");
    g_assert_no_error(error.get());
    context = adoptGRef(webkit_web_extension_context_new_for_extension(extension.get(), &error.outPtr()));
    test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(context.get()));
    g_assert_no_error(error.get());

    g_assert_true(webkit_web_extension_context_get_has_injected_content(context.get()));
    g_assert_false(webkit_web_extension_context_has_injected_content_for_uri(context.get(), "https://webkit.org/"));
    g_assert_true(webkit_web_extension_context_has_injected_content_for_uri(context.get(), "https://example.com/"));

    extension = parseExtensionManifest("[{ \"css\": [ false, \"test.css\", \"\" ], \"css_origin\": \"author\", \"matches\": [ \"*://*.example.com/\" ] }]");
    g_assert_no_error(error.get());
    context = adoptGRef(webkit_web_extension_context_new_for_extension(extension.get(), &error.outPtr()));
    test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(context.get()));
    g_assert_no_error(error.get());

    g_assert_true(webkit_web_extension_context_get_has_injected_content(context.get()));
    g_assert_false(webkit_web_extension_context_has_injected_content_for_uri(context.get(), "https://webkit.org/"));
    g_assert_true(webkit_web_extension_context_has_injected_content_for_uri(context.get(), "https://example.com/"));

    // Invalid cases

    extension = parseExtensionManifest("[]");
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);
    context = adoptGRef(webkit_web_extension_context_new_for_extension(extension.get(), &error.outPtr()));
    test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(context.get()));
    g_assert_no_error(error.get());

    g_assert_false(webkit_web_extension_context_get_has_injected_content(context.get()));
    g_assert_false(webkit_web_extension_context_has_injected_content_for_uri(context.get(), "https://webkit.org/"));
    g_assert_false(webkit_web_extension_context_has_injected_content_for_uri(context.get(), "https://example.com/"));

    extension = parseExtensionManifest("{ \"invalid\": true }");
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);
    context = adoptGRef(webkit_web_extension_context_new_for_extension(extension.get(), &error.outPtr()));
    test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(context.get()));
    g_assert_no_error(error.get());

    g_assert_false(webkit_web_extension_context_get_has_injected_content(context.get()));
    g_assert_false(webkit_web_extension_context_has_injected_content_for_uri(context.get(), "https://webkit.org/"));
    g_assert_false(webkit_web_extension_context_has_injected_content_for_uri(context.get(), "https://example.com/"));

    extension = parseExtensionManifest("[{ \"js\": [ \"test.js\" ], \"matches\": [] }]");
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);
    context = adoptGRef(webkit_web_extension_context_new_for_extension(extension.get(), &error.outPtr()));
    test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(context.get()));
    g_assert_no_error(error.get());

    g_assert_false(webkit_web_extension_context_get_has_injected_content(context.get()));
    g_assert_false(webkit_web_extension_context_has_injected_content_for_uri(context.get(), "https://webkit.org/"));
    g_assert_false(webkit_web_extension_context_has_injected_content_for_uri(context.get(), "https://example.com/"));

    extension = parseExtensionManifest("[{ \"js\": [ \"test.js\" ], \"matches\": [ \"*://*.example.com/\" ], \"world\": \"INVALID\" }]");
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);
    context = adoptGRef(webkit_web_extension_context_new_for_extension(extension.get(), &error.outPtr()));
    test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(context.get()));
    g_assert_no_error(error.get());

    g_assert_true(webkit_web_extension_context_get_has_injected_content(context.get()));
    g_assert_false(webkit_web_extension_context_has_injected_content_for_uri(context.get(), "https://webkit.org/"));
    g_assert_true(webkit_web_extension_context_has_injected_content_for_uri(context.get(), "https://example.com/"));

    extension = parseExtensionManifest("[{ \"css\": [ false, \"test.css\", \"\" ], \"css_origin\": \"bad\", \"matches\": [ \"*://*.example.com/\" ] }]");
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);
    context = adoptGRef(webkit_web_extension_context_new_for_extension(extension.get(), &error.outPtr()));
    test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(context.get()));
    g_assert_no_error(error.get());

    g_assert_true(webkit_web_extension_context_get_has_injected_content(context.get()));
    g_assert_false(webkit_web_extension_context_has_injected_content_for_uri(context.get(), "https://webkit.org/"));
    g_assert_true(webkit_web_extension_context_has_injected_content_for_uri(context.get(), "https://example.com/"));
}

static void testOptionsPageURIParsing(Test* test, gconstpointer)
{
    GUniqueOutPtr<GError> error;
    auto parseExtensionManifest = [&](const gchar* manifestString) {
        GRefPtr extension = adoptGRef(webkitWebExtensionCreate({ { "manifest.json"_s, createGBytes(manifestString) } }, &error.outPtr()));
        test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(extension.get()));
        return extension;
    };

    GRefPtr<WebKitWebExtension> extension = parseExtensionManifest("{ \"options_page\": \"options.html\", \"manifest_version\": 3, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\" }");
    g_assert_no_error(error.get());
    GRefPtr<WebKitWebExtensionContext> context = adoptGRef(webkit_web_extension_context_new_for_extension(extension.get(), &error.outPtr()));
    test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(context.get()));
    g_assert_no_error(error.get());

    GUniquePtr<char> optionsURI(g_strconcat(webkit_web_extension_context_get_base_uri(context.get()), "options.html", nullptr));
    g_assert_cmpstr(webkit_web_extension_context_get_options_page_uri(context.get()), ==, optionsURI.get());

    extension = parseExtensionManifest("{ \"options_page\": 123, \"manifest_version\": 3, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\" }");
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);
    context = adoptGRef(webkit_web_extension_context_new_for_extension(extension.get(), &error.outPtr()));
    test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(context.get()));
    g_assert_no_error(error.get());

    g_assert_null(webkit_web_extension_context_get_options_page_uri(context.get()));

    extension = parseExtensionManifest("{ \"options_ui\": { \"page\": \"options.html\" }, \"manifest_version\": 3, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\" }");
    g_assert_no_error(error.get());
    context = adoptGRef(webkit_web_extension_context_new_for_extension(extension.get(), &error.outPtr()));
    test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(context.get()));
    g_assert_no_error(error.get());

    optionsURI.reset(g_strconcat(webkit_web_extension_context_get_base_uri(context.get()), "options.html", nullptr));
    g_assert_cmpstr(webkit_web_extension_context_get_options_page_uri(context.get()), ==, optionsURI.get());

    extension = parseExtensionManifest("{ \"options_ui\": { \"page\": 123 }, \"manifest_version\": 3, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\" }");
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);
    context = adoptGRef(webkit_web_extension_context_new_for_extension(extension.get(), &error.outPtr()));
    test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(context.get()));
    g_assert_no_error(error.get());

    g_assert_null(webkit_web_extension_context_get_options_page_uri(context.get()));

    extension = parseExtensionManifest("{ \"options_page\": \"\", \"manifest_version\": 3, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\" }");
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);
    context = adoptGRef(webkit_web_extension_context_new_for_extension(extension.get(), &error.outPtr()));
    test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(context.get()));
    g_assert_no_error(error.get());

    g_assert_null(webkit_web_extension_context_get_options_page_uri(context.get()));

    extension = parseExtensionManifest("{ \"options_ui\": { }, \"manifest_version\": 3, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\" }");
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);
    context = adoptGRef(webkit_web_extension_context_new_for_extension(extension.get(), &error.outPtr()));
    test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(context.get()));
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

    GUniquePtr<char> newTabPageURI(g_strconcat(webkit_web_extension_context_get_base_uri(context.get()), "newtab.html", nullptr));
    g_assert_cmpstr(webkit_web_extension_context_get_override_new_tab_page_uri(context.get()), ==, newTabPageURI.get());

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

    newTabPageURI.reset(g_strconcat(webkit_web_extension_context_get_base_uri(context.get()), "newtab.html", nullptr));
    g_assert_cmpstr(webkit_web_extension_context_get_override_new_tab_page_uri(context.get()), ==, newTabPageURI.get());

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

void beforeAll()
{
    Test::add("WebKitWebExtensionContext", "content-scripts-parsing", testContentScriptsParsing);
    Test::add("WebKitWebExtensionContext", "options-page-uri-parsing", testOptionsPageURIParsing);
    Test::add("WebKitWebExtensionContext", "uri-overrides-parsing", testURIOverridesParsing);
}

void afterAll()
{
}

#endif // ENABLE(WK_WEB_EXTENSIONS)
