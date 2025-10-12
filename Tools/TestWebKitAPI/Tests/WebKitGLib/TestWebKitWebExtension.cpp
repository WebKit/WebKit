/*
 * Copyright (C) 2024 Igalia, S.L. All rights reserved.
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
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

using namespace TestWebKitAPI;

static GRefPtr<GBytes> createGBytes(const gchar* string)
{
    return adoptGRef(g_bytes_new_static(string, strlen(string)));
}

static void testExtensionCreationFromDirectory(Test*, gconstpointer)
{
    static const char* extensionManifest = "{ \"manifest_version\": 2, \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }";

    GUniqueOutPtr<GError> error;
    GUniquePtr<char> filePath(g_build_filename(Test::dataDirectory(), "manifest.json", nullptr));
    g_assert_true(g_file_set_contents(filePath.get(), extensionManifest, strlen(extensionManifest), &error.outPtr()));
    g_assert_no_error(error.get());

    GRefPtr<WebKitWebExtension> extension = adoptGRef(webkit_web_extension_new(Test::dataDirectory(), &error.outPtr()));
    g_assert_no_error(error.get());

    g_assert_cmpstr(webkit_web_extension_get_display_name(extension.get()), ==, "Test");
    g_assert_cmpstr(webkit_web_extension_get_display_short_name(extension.get()), ==,  "Test");
    g_assert_cmpstr(webkit_web_extension_get_display_version(extension.get()), ==,  "1.0");
    g_assert_cmpstr(webkit_web_extension_get_display_description(extension.get()), ==,  "Test description");
    g_assert_cmpstr(webkit_web_extension_get_version(extension.get()), ==,  "1.0");
    g_assert_cmpint(webkit_web_extension_get_manifest_version(extension.get()), ==, 2);
}

static void testDisplayStringParsing(Test*, gconstpointer)
{
    GUniqueOutPtr<GError> error;
    auto parse = [&](const gchar* manifestString) {
        return adoptGRef(webkitWebExtensionCreate({ { "manifest.json"_s, createGBytes(manifestString) } }, &error.outPtr()));
    };

    GRefPtr<WebKitWebExtension> extension = parse("{ \"manifest_version\": 2 }");

    g_assert_null(webkit_web_extension_get_display_name(extension.get()));
    g_assert_null(webkit_web_extension_get_display_short_name(extension.get()));
    g_assert_null(webkit_web_extension_get_display_version(extension.get()));
    g_assert_null(webkit_web_extension_get_display_description(extension.get()));
    g_assert_null(webkit_web_extension_get_version(extension.get()));
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);

    extension = parse("{ \"manifest_version\": 2, \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }");

    g_assert_cmpstr(webkit_web_extension_get_display_name(extension.get()), ==, "Test");
    g_assert_cmpstr(webkit_web_extension_get_display_short_name(extension.get()), ==,  "Test");
    g_assert_cmpstr(webkit_web_extension_get_display_version(extension.get()), ==,  "1.0");
    g_assert_cmpstr(webkit_web_extension_get_display_description(extension.get()), ==,  "Test description");
    g_assert_cmpstr(webkit_web_extension_get_version(extension.get()), ==,  "1.0");
    g_assert_cmpint(webkit_web_extension_get_manifest_version(extension.get()), ==, 2);
    g_assert_no_error(error.get());

    extension = parse("{ \"manifest_version\": 3, \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }");

    g_assert_cmpstr(webkit_web_extension_get_display_name(extension.get()), ==, "Test");
    g_assert_cmpstr(webkit_web_extension_get_display_short_name(extension.get()), ==,  "Test");
    g_assert_cmpstr(webkit_web_extension_get_display_version(extension.get()), ==,  "1.0");
    g_assert_cmpstr(webkit_web_extension_get_display_description(extension.get()), ==,  "Test description");
    g_assert_cmpstr(webkit_web_extension_get_version(extension.get()), ==,  "1.0");
    g_assert_cmpint(webkit_web_extension_get_manifest_version(extension.get()), ==, 3);
    g_assert_no_error(error.get());

    extension = parse("{ \"manifest_version\": 2, \"name\": \"Test\", \"short_name\": \"Tst\", \"version\": \"1.0\", \"version_name\": \"1.0 Final\", \"description\": \"Test description\" }");

    g_assert_cmpstr(webkit_web_extension_get_display_name(extension.get()), ==, "Test");
    g_assert_cmpstr(webkit_web_extension_get_display_short_name(extension.get()), ==,  "Tst");
    g_assert_cmpstr(webkit_web_extension_get_display_version(extension.get()), ==,  "1.0 Final");
    g_assert_cmpstr(webkit_web_extension_get_display_description(extension.get()), ==,  "Test description");
    g_assert_cmpstr(webkit_web_extension_get_version(extension.get()), ==,  "1.0");
    g_assert_no_error(error.get());
}

static void testDefaultLocaleParsing(Test*, gconstpointer)
{
    GUniqueOutPtr<GError> error;
    auto parse = [&](const gchar* manifestString, const gchar* localeFile) {
        return adoptGRef(webkitWebExtensionCreate({ { "manifest.json"_s, createGBytes(manifestString) }, { String::fromUTF8(localeFile), createGBytes("{}") } }, &error.outPtr()));
    };

    // Test no default locale
    GRefPtr<WebKitWebExtension> extension = adoptGRef(webkitWebExtensionCreate({ { "manifest.json"_s, createGBytes("{ \"manifest_version\": 2, \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }") } }, &error.outPtr()));
    g_assert_no_error(error.get());
    g_assert_null(webkit_web_extension_get_default_locale(extension.get()));

    // Test with language locale file existing
    extension = parse("{ \"manifest_version\": 2, \"default_locale\": \"en\", \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }", "_locales/en/messages.json");
    auto* defaultLocale = webkit_web_extension_get_default_locale(extension.get());
    g_assert_no_error(error.get());
    g_assert_cmpstr(webkit_web_extension_get_default_locale(extension.get()), ==, "en");

    // Test with language locale file existing
    extension = parse("{ \"manifest_version\": 2, \"default_locale\": \"en_US\", \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }", "_locales/en_US/messages.json");
    defaultLocale = webkit_web_extension_get_default_locale(extension.get());
    g_assert_no_error(error.get());
    g_assert_cmpstr(webkit_web_extension_get_default_locale(extension.get()), ==, "en_US");

    // Test with less specific locale file existing
    extension = parse("{ \"manifest_version\": 2, \"default_locale\": \"en_US\", \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }", "_locales/en/messages.json");
    defaultLocale = webkit_web_extension_get_default_locale(extension.get());
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);
    g_assert_null(defaultLocale);

    // Test with wrong locale file existing
    extension = parse("{ \"manifest_version\": 2, \"default_locale\": \"en_US\", \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }", "_locales/zh_CN/messages.json");
    defaultLocale = webkit_web_extension_get_default_locale(extension.get());
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);
    g_assert_null(defaultLocale);

    // Test with no locale file existing
    extension = parse("{ \"manifest_version\": 2, \"default_locale\": \"en_US\", \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }", "");
    defaultLocale = webkit_web_extension_get_default_locale(extension.get());
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);
    g_assert_null(defaultLocale);
}

static void testDisplayStringParsingWithLocalization(Test*, gconstpointer)
{
    GUniqueOutPtr<GError> error;

    const gchar* manifest =
        "{"
        "\"manifest_version\": 2,"
        "\"default_locale\": \"en_US\","
        "\"name\": \"__MSG_default_name__\","
        "\"short_name\": \"__MSG_regional_name__\","
        "\"version\": \"1.0\","
        "\"description\": \"__MSG_default_description__\""
        "}";

    const gchar* defaultMessages =
        "{"
        "\"default_name\": {"
        "\"message\": \"Default String\","
        "\"description\": \"The test name.\""
        "},"
        "\"default_description\": {"
        "\"message\": \"Default Description\","
        "\"description\": \"The test description.\""
        "}"
        "}";

    const gchar* regionalMessages =
        "{"
        "\"regional_name\": {"
        "\"message\": \"Regional String\","
        "\"description\": \"The regional name.\""
        "}"
        "}";

    HashMap<String, GRefPtr<GBytes>> resources = {
        { "manifest.json"_s, createGBytes(manifest) },
        { "_locales/en/messages.json"_s, createGBytes(defaultMessages) },
        { "_locales/en_US/messages.json"_s, createGBytes(regionalMessages) }
    };

    GRefPtr<WebKitWebExtension> extension = adoptGRef(webkitWebExtensionCreate(WTFMove(resources), &error.outPtr()));

    g_assert_cmpstr(webkit_web_extension_get_display_name(extension.get()), ==, "Default String");
    g_assert_cmpstr(webkit_web_extension_get_display_short_name(extension.get()), ==,  "Regional String");
    g_assert_cmpstr(webkit_web_extension_get_display_version(extension.get()), ==,  "1.0");
    g_assert_cmpstr(webkit_web_extension_get_display_description(extension.get()), ==,  "Default Description");
    g_assert_cmpstr(webkit_web_extension_get_version(extension.get()), ==,  "1.0");
    g_assert_no_error(error.get());

    manifest =
        "{"
        "\"manifest_version\": 2,"
        "\"default_locale\": \"en_US\","
        "\"name\": \"__MSG_default_name__\","
        "\"short_name\": \"__MSG_default_name__\","
        "\"version\": \"1.0\","
        "\"description\": \"__MSG_default_description__\""
        "}";
    resources = {
        { "manifest.json"_s, createGBytes(manifest) },
        { "_locales/en/messages.json"_s, createGBytes(defaultMessages) },
        { "_locales/en_US/messages.json"_s, createGBytes(regionalMessages) }
    };

    extension = adoptGRef(webkitWebExtensionCreate(WTFMove(resources), &error.outPtr()));

    g_assert_cmpstr(webkit_web_extension_get_display_short_name(extension.get()), ==,  "Default String");
    g_assert_no_error(error.get());
}

static void testActionParsing(Test*, gconstpointer)
{
    GUniqueOutPtr<GError> error;
    auto parse = [&](const gchar* manifestString) {
        return adoptGRef(webkitWebExtensionCreate({ { "manifest.json"_s, createGBytes(manifestString) } }, &error.outPtr()));
    };
    auto parseWithIcon = [&](const gchar* manifestString, GRefPtr<GBytes> imageData) {
        return adoptGRef(webkitWebExtensionCreate({
            { "manifest.json"_s, createGBytes(manifestString) },
            { "test.png"_s, imageData }
        }, &error.outPtr()));
    };

    GRefPtr<WebKitWebExtension> extension = parse("{ \"manifest_version\": 2, \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }");
    g_assert_no_error(error.get());
    g_assert_null(webkit_web_extension_get_display_action_label(extension.get()));
    g_assert_null(webkit_web_extension_get_action_icon(extension.get(), 16, 16));

    extension = parse("{ \"manifest_version\": 2, \"browser_action\": {}, \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }");
    g_assert_no_error(error.get());
    g_assert_null(webkit_web_extension_get_display_action_label(extension.get()));
    g_assert_null(webkit_web_extension_get_action_icon(extension.get(), 16, 16));

    extension = parse("{ \"manifest_version\": 2, \"page_action\": {}, \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }");
    g_assert_no_error(error.get());
    g_assert_null(webkit_web_extension_get_display_action_label(extension.get()));
    g_assert_null(webkit_web_extension_get_action_icon(extension.get(), 16, 16));

    extension = parse("{ \"manifest_version\": 2, \"browser_action\": {}, \"page_action\": {}, \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }");
    g_assert_no_error(error.get());
    g_assert_null(webkit_web_extension_get_display_action_label(extension.get()));
    g_assert_null(webkit_web_extension_get_action_icon(extension.get(), 16, 16));

    extension = parse("{ \"manifest_version\": 2, \"browser_action\": { \"default_title\": \"Button Title\" }, \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }");
    g_assert_no_error(error.get());
    g_assert_cmpstr(webkit_web_extension_get_display_action_label(extension.get()), ==, "Button Title");
    g_assert_null(webkit_web_extension_get_action_icon(extension.get(), 16, 16));

    extension = parse("{ \"manifest_version\": 2, \"page_action\": { \"default_title\": \"Button Title\" }, \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }");
    g_assert_no_error(error.get());
    g_assert_cmpstr(webkit_web_extension_get_display_action_label(extension.get()), ==, "Button Title");
    g_assert_null(webkit_web_extension_get_action_icon(extension.get(), 16, 16));

    // `action` should be ignored in manifest v2
    extension = parse("{ \"manifest_version\": 2, \"action\": { \"default_title\": \"Button Title\" }, \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }");
    g_assert_no_error(error.get());
    g_assert_null(webkit_web_extension_get_display_action_label(extension.get()));
    g_assert_null(webkit_web_extension_get_action_icon(extension.get(), 16, 16));

    // manifest v3 should look for an `action` key
    extension = parse("{ \"manifest_version\": 3, \"action\": { \"default_title\": \"Button Title\" }, \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }");
    g_assert_no_error(error.get());
    g_assert_cmpstr(webkit_web_extension_get_display_action_label(extension.get()), ==, "Button Title");
    g_assert_null(webkit_web_extension_get_action_icon(extension.get(), 16, 16));

    // Manifest v3 should never find a browser_action.
    extension = parse("{ \"manifest_version\": 3, \"browser_action\": { \"default_title\": \"Button Title\" }, \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }");
    g_assert_no_error(error.get());
    g_assert_null(webkit_web_extension_get_display_action_label(extension.get()));
    g_assert_null(webkit_web_extension_get_action_icon(extension.get(), 16, 16));

    // Or a page action
    extension = parse("{ \"manifest_version\": 3, \"page_action\": { \"default_title\": \"Button Title\" }, \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }");
    g_assert_no_error(error.get());
    g_assert_null(webkit_web_extension_get_display_action_label(extension.get()));
    g_assert_null(webkit_web_extension_get_action_icon(extension.get(), 16, 16));

    extension = parse("{ \"manifest_version\": 3, \"action\": { }, \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }");
    g_assert_no_error(error.get());
    g_assert_null(webkit_web_extension_get_display_action_label(extension.get()));
    g_assert_null(webkit_web_extension_get_action_icon(extension.get(), 16, 16));

    GRefPtr<GBytes> imageData = Util::makePNGData(16, 16, 0x008000);

    extension = parseWithIcon("{ \"manifest_version\": 3, \"action\": { \"default_icon\": \"test.png\", \"default_title\": \"Button Title\" }, \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }", imageData);
    g_assert_no_error(error.get());
    g_assert_cmpstr(webkit_web_extension_get_display_action_label(extension.get()), ==, "Button Title");
    g_assert_nonnull(webkit_web_extension_get_action_icon(extension.get(), 16, 16));

    extension = parseWithIcon("{ \"manifest_version\": 3, \"action\": { \"default_icon\": { \"16\": \"test.png\" }, \"default_title\": \"Button Title\" }, \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }", imageData);
    g_assert_no_error(error.get());
    g_assert_cmpstr(webkit_web_extension_get_display_action_label(extension.get()), ==, "Button Title");
    g_assert_nonnull(webkit_web_extension_get_action_icon(extension.get(), 16, 16));

    extension = parseWithIcon("{ \"manifest_version\": 3, \"icons\": { \"16\": \"test.png\" }, \"action\": { \"default_title\": \"Button Title\" }, \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }", imageData);
    g_assert_no_error(error.get());
    g_assert_cmpstr(webkit_web_extension_get_display_action_label(extension.get()), ==, "Button Title");
    g_assert_nonnull(webkit_web_extension_get_action_icon(extension.get(), 16, 16));
}

static void testContentScriptsParsing(Test*, gconstpointer)
{
    GUniqueOutPtr<GError> error;
    auto parse = [&](const gchar* manifestString) {
        return adoptGRef(webkitWebExtensionCreate({ { "manifest.json"_s, createGBytes(manifestString) } }, &error.outPtr()));
    };

    GRefPtr<WebKitWebExtension> extension = parse("{ \"content_scripts\": [{ \"js\": [\"\test.js\", 1, \"\"], \"css\": [false, \"test.css\", \"\"], \"matches\": [\"*://*/\"] }], \"manifest_version\": 2, \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test\" }");
    g_assert_no_error(error.get());
    g_assert_true(webkit_web_extension_get_has_injected_content(extension.get()));

    extension = parse("{ \"content_scripts\": [{ \"js\": [\"\test.js\", 1, \"\"], \"css\": [false, \"test.css\", \"\"], \"matches\": [\"*://*/\"], \"exclude_matches\": [\"*://*.example.com/\"] }], \"manifest_version\": 2, \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test\" }");
    g_assert_no_error(error.get());
    g_assert_true(webkit_web_extension_get_has_injected_content(extension.get()));

    extension = parse("{ \"content_scripts\": [{ \"js\": [\"\test.js\", 1, \"\"], \"css\": [false, \"test.css\", \"\"], \"matches\": [\"*://*.example.com/\"] }], \"manifest_version\": 2, \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test\" }");
    g_assert_no_error(error.get());
    g_assert_true(webkit_web_extension_get_has_injected_content(extension.get()));

    extension = parse("{ \"content_scripts\": [{ \"js\": [\"\test.js\"], \"matches\": [\"*://*.example.com/\"], \"world\": \"MAIN\" }], \"manifest_version\": 2, \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test\" }");
    g_assert_no_error(error.get());
    g_assert_true(webkit_web_extension_get_has_injected_content(extension.get()));

    extension = parse("{ \"content_scripts\": [{ \"css\": [false, \"test.css\", \"\"], \"matches\": [\"*://*.example.com/\"], \"css_origin\": \"user\" }], \"manifest_version\": 2, \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test\" }");
    g_assert_no_error(error.get());
    g_assert_true(webkit_web_extension_get_has_injected_content(extension.get()));

    extension = parse("{ \"content_scripts\": [{ \"css\": [false, \"test.css\", \"\"], \"matches\": [\"*://*.example.com/\"], \"css_origin\": \"author\" }], \"manifest_version\": 2, \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test\" }");
    g_assert_no_error(error.get());
    g_assert_true(webkit_web_extension_get_has_injected_content(extension.get()));

    // Invalid cases

    extension = parse("{ \"content_scripts\": [], \"manifest_version\": 2, \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test\" }");
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);
    g_assert_false(webkit_web_extension_get_has_injected_content(extension.get()));

    extension = parse("{ \"content_scripts\": { \"invalid\": true }, \"manifest_version\": 2, \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test\" }");
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);
    g_assert_false(webkit_web_extension_get_has_injected_content(extension.get()));

    extension = parse("{ \"content_scripts\": [{ \"js\": [ \"test.js\" ], \"matches\": [] }], \"manifest_version\": 2, \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test\" }");
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);
    g_assert_false(webkit_web_extension_get_has_injected_content(extension.get()));

    // Non-critical invalid cases

    extension = parse("{ \"content_scripts\": [{ \"js\": [ \"test.js\" ], \"matches\": [\"*://*.example.com/\"], \"run_at\": \"invalid\" }], \"manifest_version\": 2, \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test\" }");
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);
    g_assert_true(webkit_web_extension_get_has_injected_content(extension.get()));

    extension = parse("{ \"content_scripts\": [{ \"js\": [ \"test.js\" ], \"matches\": [\"*://*.example.com/\"], \"world\": \"INVALID\" }], \"manifest_version\": 2, \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test\" }");
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);
    g_assert_true(webkit_web_extension_get_has_injected_content(extension.get()));

    extension = parse("{ \"content_scripts\": [{ \"css\": [false, \"test.css\", \"\"], \"matches\": [\"*://*.example.com/\"], \"css_origin\": \"bad\" }], \"manifest_version\": 2, \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test\" }");
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);
    g_assert_true(webkit_web_extension_get_has_injected_content(extension.get()));
}

static void testPermissionsParsing(Test*, gconstpointer)
{
    GUniqueOutPtr<GError> error;
    auto parse = [&](const gchar* manifestString) {
        return adoptGRef(webkitWebExtensionCreate({ { "manifest.json"_s, createGBytes(manifestString) } }, &error.outPtr()));
    };

    // Failure Testing

    // Neither of the "permissions" and "optional_permissions" keys are defined.

    GRefPtr<WebKitWebExtension> extension = parse("{ \"manifest_version\": 2, \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }");
    const gchar* const* requestedPermissions = webkit_web_extension_get_requested_permissions(extension.get());
    const gchar* const* optionalPermissions = webkit_web_extension_get_optional_permissions(extension.get());
    g_assert_null(requestedPermissions);
    g_assert_null(webkit_web_extension_get_requested_permission_match_patterns(extension.get()));
    g_assert_null(optionalPermissions);
    g_assert_null(webkit_web_extension_get_optional_permission_match_patterns(extension.get()));

    // The "permissions" key alone is defined and empty

    extension = parse("{ \"manifest_version\": 2, \"permissions\": [], \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }");
    requestedPermissions = webkit_web_extension_get_requested_permissions(extension.get());
    optionalPermissions = webkit_web_extension_get_optional_permissions(extension.get());
    g_assert_null(requestedPermissions);
    g_assert_null(webkit_web_extension_get_requested_permission_match_patterns(extension.get()));
    g_assert_null(optionalPermissions);
    g_assert_null(webkit_web_extension_get_optional_permission_match_patterns(extension.get()));

    // The "optional_permissions" key alone is defined and empty

    extension = parse("{ \"manifest_version\": 2, \"optional_permissions\": [], \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }");
    requestedPermissions = webkit_web_extension_get_requested_permissions(extension.get());
    optionalPermissions = webkit_web_extension_get_optional_permissions(extension.get());
    g_assert_null(requestedPermissions);
    g_assert_null(webkit_web_extension_get_requested_permission_match_patterns(extension.get()));
    g_assert_null(optionalPermissions);
    g_assert_null(webkit_web_extension_get_optional_permission_match_patterns(extension.get()));

    // The "permissions" and "optional_permissions" keys are defined as invalid types

    extension = parse("{ \"manifest_version\": 2, \"permissions\": 2, \"optional_permissions\": \"foo\", \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }");
    requestedPermissions = webkit_web_extension_get_requested_permissions(extension.get());
    optionalPermissions = webkit_web_extension_get_optional_permissions(extension.get());
    g_assert_null(requestedPermissions);
    g_assert_null(webkit_web_extension_get_requested_permission_match_patterns(extension.get()));
    g_assert_null(optionalPermissions);
    g_assert_null(webkit_web_extension_get_optional_permission_match_patterns(extension.get()));

    // The "permissions" keys is defined with an invalid permission.

    extension = parse("{ \"manifest_version\": 2, \"permissions\": [ \"invalid\" ], \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }");
    requestedPermissions = webkit_web_extension_get_requested_permissions(extension.get());
    optionalPermissions = webkit_web_extension_get_optional_permissions(extension.get());
    g_assert_null(requestedPermissions);
    g_assert_null(webkit_web_extension_get_requested_permission_match_patterns(extension.get()));
    g_assert_null(optionalPermissions);
    g_assert_null(webkit_web_extension_get_optional_permission_match_patterns(extension.get()));

    // The "permissions" key is defined with a valid permission

    extension = parse("{ \"manifest_version\": 2, \"permissions\": [ \"tabs\" ], \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }");
    requestedPermissions = webkit_web_extension_get_requested_permissions(extension.get());
    optionalPermissions = webkit_web_extension_get_optional_permissions(extension.get());
    g_assert_nonnull(requestedPermissions);
    g_assert_null(webkit_web_extension_get_requested_permission_match_patterns(extension.get()));
    g_assert_null(optionalPermissions);
    g_assert_null(webkit_web_extension_get_optional_permission_match_patterns(extension.get()));
    g_assert_cmpstr(requestedPermissions[0], ==, "tabs");
    g_assert_null(requestedPermissions[1]);

    // The "permissions" key is defined with a valid & invalid permission

    extension = parse("{ \"manifest_version\": 2, \"permissions\": [ \"tabs\", \"invalid\" ], \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }");
    requestedPermissions = webkit_web_extension_get_requested_permissions(extension.get());
    optionalPermissions = webkit_web_extension_get_optional_permissions(extension.get());
    g_assert_nonnull(requestedPermissions);
    g_assert_null(webkit_web_extension_get_requested_permission_match_patterns(extension.get()));
    g_assert_null(optionalPermissions);
    g_assert_null(webkit_web_extension_get_optional_permission_match_patterns(extension.get()));
    g_assert_cmpstr(requestedPermissions[0], ==, "tabs");
    g_assert_null(requestedPermissions[1]);

    // The "permissions" key is defined with a valid permission & origin

    extension = parse("{ \"manifest_version\": 2, \"permissions\": [ \"tabs\", \"http://www.webkit.org/\" ], \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }");
    requestedPermissions = webkit_web_extension_get_requested_permissions(extension.get());
    optionalPermissions = webkit_web_extension_get_optional_permissions(extension.get());
    g_assert_nonnull(requestedPermissions);
    guint patternLength = 0;
    auto matchPatterns = webkit_web_extension_get_requested_permission_match_patterns(extension.get());
    for (; *matchPatterns != nullptr; matchPatterns++)
        patternLength++;
    g_assert_cmpint(patternLength, ==, 1);
    g_assert_cmpstr(webkit_web_extension_match_pattern_get_string(webkit_web_extension_get_requested_permission_match_patterns(extension.get())[0]), ==, "http://www.webkit.org/");
    g_assert_null(optionalPermissions);
    g_assert_null(webkit_web_extension_get_optional_permission_match_patterns(extension.get()));
    g_assert_cmpstr(requestedPermissions[0], ==, "tabs");
    g_assert_null(requestedPermissions[1]);

    // The "permissions" key is defined with a valid permission & invalid origin

    extension = parse("{ \"manifest_version\": 2, \"permissions\": [ \"tabs\", \"foo://www.webkit.org/\" ], \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }");
    requestedPermissions = webkit_web_extension_get_requested_permissions(extension.get());
    optionalPermissions = webkit_web_extension_get_optional_permissions(extension.get());
    g_assert_nonnull(requestedPermissions);
    g_assert_null(webkit_web_extension_get_requested_permission_match_patterns(extension.get()));
    g_assert_null(optionalPermissions);
    g_assert_null(webkit_web_extension_get_optional_permission_match_patterns(extension.get()));
    g_assert_cmpstr(requestedPermissions[0], ==, "tabs");
    g_assert_null(requestedPermissions[1]);

    // The "optional_permissions" keys is defined with an invalid permission.

    extension = parse("{ \"manifest_version\": 2, \"optional_permissions\": [ \"invalid\" ], \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }");
    requestedPermissions = webkit_web_extension_get_requested_permissions(extension.get());
    optionalPermissions = webkit_web_extension_get_optional_permissions(extension.get());
    g_assert_null(optionalPermissions);
    g_assert_null(webkit_web_extension_get_optional_permission_match_patterns(extension.get()));
    g_assert_null(requestedPermissions);
    g_assert_null(webkit_web_extension_get_requested_permission_match_patterns(extension.get()));

    // The "optional_permissions" key is defined with a valid permission

    extension = parse("{ \"manifest_version\": 2, \"optional_permissions\": [ \"tabs\" ], \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }");
    requestedPermissions = webkit_web_extension_get_requested_permissions(extension.get());
    optionalPermissions = webkit_web_extension_get_optional_permissions(extension.get());
    g_assert_nonnull(optionalPermissions);
    g_assert_null(webkit_web_extension_get_optional_permission_match_patterns(extension.get()));
    g_assert_null(requestedPermissions);
    g_assert_null(webkit_web_extension_get_requested_permission_match_patterns(extension.get()));
    g_assert_cmpstr(optionalPermissions[0], ==, "tabs");
    g_assert_null(optionalPermissions[1]);

    // The "optional_permissions" key is defined with a valid & invalid permission

    extension = parse("{ \"manifest_version\": 2, \"optional_permissions\": [ \"tabs\", \"invalid\" ], \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }");
    requestedPermissions = webkit_web_extension_get_requested_permissions(extension.get());
    optionalPermissions = webkit_web_extension_get_optional_permissions(extension.get());
    g_assert_nonnull(optionalPermissions);
    g_assert_null(webkit_web_extension_get_optional_permission_match_patterns(extension.get()));
    g_assert_null(requestedPermissions);
    g_assert_null(webkit_web_extension_get_optional_permission_match_patterns(extension.get()));
    g_assert_cmpstr(optionalPermissions[0], ==, "tabs");
    g_assert_null(optionalPermissions[1]);

    // The "optional_permissions" key is defined with a valid permission & origin

    extension = parse("{ \"manifest_version\": 2, \"optional_permissions\": [ \"tabs\", \"http://www.webkit.org/\" ], \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }");
    requestedPermissions = webkit_web_extension_get_requested_permissions(extension.get());
    optionalPermissions = webkit_web_extension_get_optional_permissions(extension.get());
    g_assert_null(requestedPermissions);
    g_assert_null(webkit_web_extension_get_requested_permission_match_patterns(extension.get()));
    g_assert_nonnull(optionalPermissions);
    patternLength = 0;
    matchPatterns = webkit_web_extension_get_optional_permission_match_patterns(extension.get());
    for (; *matchPatterns != nullptr; matchPatterns++)
        patternLength++;
    g_assert_cmpint(patternLength, ==, 1);
    g_assert_cmpstr(webkit_web_extension_match_pattern_get_string(webkit_web_extension_get_optional_permission_match_patterns(extension.get())[0]), ==, "http://www.webkit.org/");
    g_assert_cmpstr(optionalPermissions[0], ==, "tabs");
    g_assert_null(optionalPermissions[1]);

    // The "optional_permissions" key is defined with a valid permission & invalid origin

    extension = parse("{ \"manifest_version\": 2, \"optional_permissions\": [ \"tabs\", \"foo://www.webkit.org/\" ], \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }");
    requestedPermissions = webkit_web_extension_get_requested_permissions(extension.get());
    optionalPermissions = webkit_web_extension_get_optional_permissions(extension.get());
    g_assert_null(requestedPermissions);
    g_assert_null(webkit_web_extension_get_requested_permission_match_patterns(extension.get()));
    g_assert_nonnull(optionalPermissions);
    g_assert_null(webkit_web_extension_get_optional_permission_match_patterns(extension.get()));
    g_assert_cmpstr(optionalPermissions[0], ==, "tabs");
    g_assert_null(optionalPermissions[1]);

    // The "optional_permissions" key is defined with a valid & forbidden invalid permission

    extension = parse("{ \"manifest_version\": 2, \"optional_permissions\": [ \"tabs\", \"geolocation\" ], \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }");
    requestedPermissions = webkit_web_extension_get_requested_permissions(extension.get());
    optionalPermissions = webkit_web_extension_get_optional_permissions(extension.get());
    g_assert_nonnull(optionalPermissions);
    g_assert_null(webkit_web_extension_get_optional_permission_match_patterns(extension.get()));
    g_assert_null(requestedPermissions);
    g_assert_null(webkit_web_extension_get_requested_permission_match_patterns(extension.get()));
    g_assert_cmpstr(optionalPermissions[0], ==, "tabs");
    g_assert_null(optionalPermissions[1]);

    // The "optional_permissions" key is defined in "permissions"

    extension = parse("{ \"manifest_version\": 2, \"permissions\": [ \"tabs\", \"geolocation\" ], \"optional_permissions\": [ \"tabs\" ], \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }");
    requestedPermissions = webkit_web_extension_get_requested_permissions(extension.get());
    optionalPermissions = webkit_web_extension_get_optional_permissions(extension.get());
    g_assert_nonnull(requestedPermissions);
    g_assert_null(webkit_web_extension_get_requested_permission_match_patterns(extension.get()));
    g_assert_null(optionalPermissions);
    g_assert_null(webkit_web_extension_get_optional_permission_match_patterns(extension.get()));
    g_assert_cmpstr(requestedPermissions[0], ==, "tabs");
    g_assert_null(requestedPermissions[1]);

    // The "optional_permissions" key contains an origin defined in "permissions"

    extension = parse("{ \"manifest_version\": 2, \"permissions\": [ \"http://www.webkit.org/\" ], \"optional_permissions\": [ \"http://www.webkit.org/\" ], \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }");
    patternLength = 0;
    matchPatterns = webkit_web_extension_get_requested_permission_match_patterns(extension.get());
    for (; *matchPatterns != nullptr; matchPatterns++)
        patternLength++;
    g_assert_cmpint(patternLength, ==, 1);
    g_assert_cmpstr(webkit_web_extension_match_pattern_get_string(webkit_web_extension_get_requested_permission_match_patterns(extension.get())[0]), ==, "http://www.webkit.org/");
    g_assert_null(webkit_web_extension_get_optional_permission_match_patterns(extension.get()));

    // Make sure manifest v2 extensions ignore hosts from host_permissions (this should only be checked for manifest v3).

    extension = parse("{ \"manifest_version\": 2, \"permissions\": [ \"http://www.webkit.org/\" ], \"optional_permissions\": [ \"http://www.example.com/\" ], \"host_permissions\": [ \"https://webkit.org/\" ], \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }");
    patternLength = 0;
    matchPatterns = webkit_web_extension_get_requested_permission_match_patterns(extension.get());
    for (; *matchPatterns != nullptr; matchPatterns++)
        patternLength++;
    g_assert_cmpint(patternLength, ==, 1);
    g_assert_cmpstr(webkit_web_extension_match_pattern_get_string(webkit_web_extension_get_requested_permission_match_patterns(extension.get())[0]), ==, "http://www.webkit.org/");
    patternLength = 0;
    matchPatterns = webkit_web_extension_get_optional_permission_match_patterns(extension.get());
    for (; *matchPatterns != nullptr; matchPatterns++)
        patternLength++;
    g_assert_cmpint(patternLength, ==, 1);
    g_assert_cmpstr(webkit_web_extension_match_pattern_get_string(webkit_web_extension_get_optional_permission_match_patterns(extension.get())[0]), ==, "http://www.example.com/");

    // Make sure manifest v3 parses hosts from host_permissions, and ignores hosts in permissions and optional_permissions.

    extension = parse("{ \"manifest_version\": 3, \"permissions\": [ \"http://www.webkit.org/\" ], \"optional_permissions\": [ \"http://www.example.com/\" ], \"host_permissions\": [ \"https://webkit.org/\" ], \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }");
    patternLength = 0;
    matchPatterns = webkit_web_extension_get_requested_permission_match_patterns(extension.get());
    for (; *matchPatterns != nullptr; matchPatterns++)
        patternLength++;
    g_assert_cmpint(patternLength, ==, 1);
    g_assert_cmpstr(webkit_web_extension_match_pattern_get_string(webkit_web_extension_get_requested_permission_match_patterns(extension.get())[0]), ==, "https://webkit.org/");
    g_assert_null(webkit_web_extension_get_optional_permission_match_patterns(extension.get()));

    // Make sure manifest v3 parses optional_host_permissions.

    extension = parse("{ \"manifest_version\": 3, \"optional_host_permissions\": [ \"http://www.example.com/\" ], \"host_permissions\": [ \"https://webkit.org/\" ], \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }");
    patternLength = 0;
    matchPatterns = webkit_web_extension_get_requested_permission_match_patterns(extension.get());
    for (; *matchPatterns != nullptr; matchPatterns++)
        patternLength++;
    g_assert_cmpint(patternLength, ==, 1);
    g_assert_cmpstr(webkit_web_extension_match_pattern_get_string(webkit_web_extension_get_requested_permission_match_patterns(extension.get())[0]), ==, "https://webkit.org/");
    patternLength = 0;
    matchPatterns = webkit_web_extension_get_optional_permission_match_patterns(extension.get());
    for (; *matchPatterns != nullptr; matchPatterns++)
        patternLength++;
    g_assert_cmpint(patternLength, ==, 1);
    g_assert_cmpstr(webkit_web_extension_match_pattern_get_string(webkit_web_extension_get_optional_permission_match_patterns(extension.get())[0]), ==, "http://www.example.com/");
}

static void testBackgroundParsing(Test*, gconstpointer)
{
    GUniqueOutPtr<GError> error;
    auto parse = [&](const gchar* manifestString) {
        return adoptGRef(webkitWebExtensionCreate({ { "manifest.json"_s, createGBytes(manifestString) } }, &error.outPtr()));
    };

    GRefPtr<WebKitWebExtension> extension = parse("{ \"manifest_version\": 2, \"background\": { \"scripts\": [ \"test.js\" ] }, \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }");
    g_assert_true(webkit_web_extension_get_has_background_content(extension.get()));
    g_assert_true(webkit_web_extension_get_has_persistent_background_content(extension.get()));
    g_assert_false(webkit_web_extension_get_has_service_worker_background_content(extension.get()));
    g_assert_false(webkit_web_extension_get_has_modular_background_content(extension.get()));
    g_assert_no_error(error.get());

    extension = parse("{\"manifest_version\":2,\"background\":{\"page\":\"test.html\",\"persistent\":false},\"name\":\"Test\",\"version\":\"1.0\",\"description\":\"Test\"}");
    g_assert_no_error(error.get());
    g_assert_true(webkit_web_extension_get_has_background_content(extension.get()));
    g_assert_false(webkit_web_extension_get_has_persistent_background_content(extension.get()));
    g_assert_false(webkit_web_extension_get_has_service_worker_background_content(extension.get()));
    g_assert_false(webkit_web_extension_get_has_modular_background_content(extension.get()));

    extension = parse("{ \"manifest_version\": 2, \"background\": { \"scripts\": [ \"test-1.js\", \"\", \"test-2.js\" ], \"persistent\": true }, \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }");
    g_assert_true(webkit_web_extension_get_has_background_content(extension.get()));
    g_assert_true(webkit_web_extension_get_has_persistent_background_content(extension.get()));
    g_assert_false(webkit_web_extension_get_has_service_worker_background_content(extension.get()));
    g_assert_false(webkit_web_extension_get_has_modular_background_content(extension.get()));
    g_assert_no_error(error.get());

    extension = parse("{ \"manifest_version\": 2, \"background\": { \"service_worker\": \"test.js\" }, \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }");
    g_assert_true(webkit_web_extension_get_has_background_content(extension.get()));
    g_assert_false(webkit_web_extension_get_has_persistent_background_content(extension.get()));
    g_assert_true(webkit_web_extension_get_has_service_worker_background_content(extension.get()));
    g_assert_false(webkit_web_extension_get_has_modular_background_content(extension.get()));
    g_assert_no_error(error.get());

    extension = parse("{ \"manifest_version\": 2, \"background\": { \"scripts\": [ \"test-1.js\", \"test-2.js\" ], \"service_worker\": \"test.js\", \"persistent\": false }, \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }");
    g_assert_true(webkit_web_extension_get_has_background_content(extension.get()));
    g_assert_false(webkit_web_extension_get_has_persistent_background_content(extension.get()));
    g_assert_false(webkit_web_extension_get_has_service_worker_background_content(extension.get()));
    g_assert_false(webkit_web_extension_get_has_modular_background_content(extension.get()));
    g_assert_no_error(error.get());

    extension = parse("{ \"manifest_version\": 2, \"background\": { \"page\": \"test.html\", \"service_worker\": \"test.js\", \"persistent\": false }, \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }");
    g_assert_true(webkit_web_extension_get_has_background_content(extension.get()));
    g_assert_false(webkit_web_extension_get_has_persistent_background_content(extension.get()));
    g_assert_false(webkit_web_extension_get_has_service_worker_background_content(extension.get()));
    g_assert_false(webkit_web_extension_get_has_modular_background_content(extension.get()));
    g_assert_no_error(error.get());

    extension = parse("{ \"manifest_version\": 2, \"background\": { \"scripts\": [ \"test-1.js\", \"test-2.js\" ], \"page\": \"test.html\", \"service_worker\": \"test.js\", \"persistent\": false }, \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }");
    g_assert_true(webkit_web_extension_get_has_background_content(extension.get()));
    g_assert_false(webkit_web_extension_get_has_persistent_background_content(extension.get()));
    g_assert_false(webkit_web_extension_get_has_service_worker_background_content(extension.get()));
    g_assert_false(webkit_web_extension_get_has_modular_background_content(extension.get()));
    g_assert_no_error(error.get());

    extension = parse("{ \"manifest_version\": 2, \"background\": { \"service_worker\": \"test.js\", \"persistent\": false }, \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }");
    g_assert_true(webkit_web_extension_get_has_background_content(extension.get()));
    g_assert_false(webkit_web_extension_get_has_persistent_background_content(extension.get()));
    g_assert_true(webkit_web_extension_get_has_service_worker_background_content(extension.get()));
    g_assert_false(webkit_web_extension_get_has_modular_background_content(extension.get()));
    g_assert_no_error(error.get());

    extension = parse("{ \"manifest_version\": 2, \"background\": { \"service_worker\": \"test.js\", \"type\": \"module\", \"persistent\": false }, \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }");
    g_assert_true(webkit_web_extension_get_has_background_content(extension.get()));
    g_assert_false(webkit_web_extension_get_has_persistent_background_content(extension.get()));
    g_assert_true(webkit_web_extension_get_has_service_worker_background_content(extension.get()));
    g_assert_true(webkit_web_extension_get_has_modular_background_content(extension.get()));
    g_assert_no_error(error.get());

    extension = parse("{ \"manifest_version\": 2, \"background\": { \"scripts\": [ \"test-1.js\", \"test-2.js\" ], \"type\": \"module\", \"persistent\": false }, \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }");
    g_assert_true(webkit_web_extension_get_has_background_content(extension.get()));
    g_assert_false(webkit_web_extension_get_has_persistent_background_content(extension.get()));
    g_assert_false(webkit_web_extension_get_has_service_worker_background_content(extension.get()));
    g_assert_true(webkit_web_extension_get_has_modular_background_content(extension.get()));
    g_assert_no_error(error.get());

    // Invalid cases

    extension = parse("{ \"manifest_version\": 3, \"background\": { \"page\": \"test.html\", \"persistent\": true }, \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }");
    g_assert_true(webkit_web_extension_get_has_background_content(extension.get()));
    g_assert_false(webkit_web_extension_get_has_persistent_background_content(extension.get()));
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_BACKGROUND_PERSISTENCE);

    extension = parse("{ \"manifest_version\": 2, \"background\": { \"service_worker\": \"test.js\", \"persistent\": true }, \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }");
    g_assert_true(webkit_web_extension_get_has_background_content(extension.get()));
    g_assert_false(webkit_web_extension_get_has_persistent_background_content(extension.get()));
    g_assert_false(webkit_web_extension_get_has_modular_background_content(extension.get()));
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_BACKGROUND_PERSISTENCE);

    extension = parse("{ \"manifest_version\": 2, \"background\": { }, \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }");
    g_assert_false(webkit_web_extension_get_has_background_content(extension.get()));
    g_assert_false(webkit_web_extension_get_has_persistent_background_content(extension.get()));
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);

    extension = parse("{ \"manifest_version\": 2, \"background\": [ \"invalid\" ], \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }");
    g_assert_false(webkit_web_extension_get_has_background_content(extension.get()));
    g_assert_false(webkit_web_extension_get_has_persistent_background_content(extension.get()));
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);

    extension = parse("{ \"manifest_version\": 2, \"background\": { \"scripts\": [], \"persistent\": false }, \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }");
    g_assert_false(webkit_web_extension_get_has_background_content(extension.get()));
    g_assert_false(webkit_web_extension_get_has_persistent_background_content(extension.get()));
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);

    extension = parse("{ \"manifest_version\": 2, \"background\": { \"page\": \"\", \"persistent\": false }, \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }");
    g_assert_false(webkit_web_extension_get_has_background_content(extension.get()));
    g_assert_false(webkit_web_extension_get_has_persistent_background_content(extension.get()));
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);

    extension = parse("{ \"manifest_version\": 2, \"background\": { \"page\": [ \"test.html\" ], \"persistent\": false }, \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }");
    g_assert_false(webkit_web_extension_get_has_background_content(extension.get()));
    g_assert_false(webkit_web_extension_get_has_persistent_background_content(extension.get()));
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);

    extension = parse("{ \"manifest_version\": 2, \"background\": { \"scripts\": [ [ \"test.js\" ] ], \"persistent\": false }, \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }");
    g_assert_false(webkit_web_extension_get_has_background_content(extension.get()));
    g_assert_false(webkit_web_extension_get_has_persistent_background_content(extension.get()));
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);

    extension = parse("{ \"manifest_version\": 2, \"background\": { \"service_worker\": \"\", \"persistent\": false }, \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }");
    g_assert_false(webkit_web_extension_get_has_background_content(extension.get()));
    g_assert_false(webkit_web_extension_get_has_persistent_background_content(extension.get()));
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);

    extension = parse("{ \"manifest_version\": 2, \"background\": { \"service_worker\": [ \"test.js\" ], \"persistent\": false }, \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }");
    g_assert_false(webkit_web_extension_get_has_background_content(extension.get()));
    g_assert_false(webkit_web_extension_get_has_persistent_background_content(extension.get()));
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);
}

static void testBackgroundPreferredEnvironmentParsing(Test*, gconstpointer)
{
    GUniqueOutPtr<GError> error;
    auto parse = [&](const gchar* manifestString) {
        return adoptGRef(webkitWebExtensionCreate({ { "manifest.json"_s, createGBytes(manifestString) } }, &error.outPtr()));
    };

    GRefPtr<WebKitWebExtension> extension = parse("{ \"manifest_version\": 3, \"background\": { \"preferred_environment\": [ \"service_worker\", \"document\" ], \"service_worker\": \"background.js\", \"scripts\": [ \"background.js\" ], \"page\": \"background.html\" }, \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }");
    g_assert_true(webkit_web_extension_get_has_background_content(extension.get()));
    g_assert_true(webkit_web_extension_get_has_service_worker_background_content(extension.get()));
    g_assert_no_error(error.get());

    extension = parse("{ \"manifest_version\": 3, \"background\": { \"preferred_environment\": [ \"document\", \"service_worker\" ], \"service_worker\": \"background.js\", \"scripts\": [ \"background.js\" ], \"page\": \"background.html\" }, \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }");
    g_assert_true(webkit_web_extension_get_has_background_content(extension.get()));
    g_assert_false(webkit_web_extension_get_has_service_worker_background_content(extension.get()));
    g_assert_no_error(error.get());

    extension = parse("{ \"manifest_version\": 3, \"background\": { \"preferred_environment\": \"service_worker\", \"service_worker\": \"background.js\" }, \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }");
    g_assert_true(webkit_web_extension_get_has_background_content(extension.get()));
    g_assert_true(webkit_web_extension_get_has_service_worker_background_content(extension.get()));
    g_assert_no_error(error.get());

    extension = parse("{ \"manifest_version\": 3, \"background\": { \"preferred_environment\": [ \"document\" ], \"page\": \"background.html\" }, \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }");
    g_assert_true(webkit_web_extension_get_has_background_content(extension.get()));
    g_assert_false(webkit_web_extension_get_has_service_worker_background_content(extension.get()));
    g_assert_no_error(error.get());

    extension = parse("{ \"manifest_version\": 3, \"background\": { \"preferred_environment\": \"document\", \"scripts\": [ \"background.js\" ] }, \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }");
    g_assert_true(webkit_web_extension_get_has_background_content(extension.get()));
    g_assert_false(webkit_web_extension_get_has_service_worker_background_content(extension.get()));
    g_assert_no_error(error.get());

    extension = parse("{ \"manifest_version\": 3, \"background\": { \"preferred_environment\": [ \"document\", \"service_worker\" ], \"scripts\": [ \"background.js\" ] }, \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }");
    g_assert_true(webkit_web_extension_get_has_background_content(extension.get()));
    g_assert_false(webkit_web_extension_get_has_service_worker_background_content(extension.get()));
    g_assert_no_error(error.get());

    extension = parse("{ \"manifest_version\": 3, \"background\": { \"preferred_environment\": [ \"document\", 42, \"unknown\" ], \"scripts\": [ \"background.js\" ] }, \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }");
    g_assert_true(webkit_web_extension_get_has_background_content(extension.get()));
    g_assert_false(webkit_web_extension_get_has_service_worker_background_content(extension.get()));
    g_assert_no_error(error.get());

    extension = parse("{ \"manifest_version\": 3, \"background\": { \"preferred_environment\": [ \"unknown\", 42 ], \"page\": \"background.html\" }, \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }");
    g_assert_true(webkit_web_extension_get_has_background_content(extension.get()));
    g_assert_false(webkit_web_extension_get_has_service_worker_background_content(extension.get()));
    g_assert_no_error(error.get());

    extension = parse("{ \"manifest_version\": 3, \"background\": { \"preferred_environment\": \"unknown\", \"service_worker\": \"background.js\" }, \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }");
    g_assert_true(webkit_web_extension_get_has_background_content(extension.get()));
    g_assert_true(webkit_web_extension_get_has_service_worker_background_content(extension.get()));
    g_assert_no_error(error.get());

    extension = parse("{ \"manifest_version\": 3, \"background\": { \"preferred_environment\": [ \"unknown\", \"document\" ], \"service_worker\": \"background.js\", \"page\": \"background.html\" }, \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }");
    g_assert_true(webkit_web_extension_get_has_background_content(extension.get()));
    g_assert_false(webkit_web_extension_get_has_service_worker_background_content(extension.get()));
    g_assert_no_error(error.get());

    // Invalid cases

    extension = parse("{ \"manifest_version\": 3, \"background\": { \"preferred_environment\": [], \"service_worker\": \"background.js\" }, \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }");
    g_assert_true(webkit_web_extension_get_has_background_content(extension.get()));
    g_assert_true(webkit_web_extension_get_has_service_worker_background_content(extension.get()));
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);

    extension = parse("{ \"manifest_version\": 3, \"background\": { \"preferred_environment\": 42, \"service_worker\": \"background.js\", \"page\": \"background.html\" }, \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }");
    g_assert_true(webkit_web_extension_get_has_background_content(extension.get()));
    g_assert_false(webkit_web_extension_get_has_service_worker_background_content(extension.get()));
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);

    extension = parse("{ \"manifest_version\": 3, \"background\": { \"preferred_environment\": [ \"service_worker\", \"document\" ] }, \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }");
    g_assert_false(webkit_web_extension_get_has_background_content(extension.get()));
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);

    extension = parse("{ \"manifest_version\": 3, \"background\": { \"preferred_environment\": \"document\", \"service_worker\": \"background.js\" }, \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }");
    g_assert_false(webkit_web_extension_get_has_background_content(extension.get()));
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);

    extension = parse("{ \"manifest_version\": 3, \"background\": { \"preferred_environment\": \"service_worker\", \"page\": \"background.html\" }, \"name\": \"Test\", \"version\": \"1.0\", \"description\": \"Test description\" }");
    g_assert_false(webkit_web_extension_get_has_background_content(extension.get()));
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);
}

static void testOptionsPageParsing(Test*, gconstpointer)
{
    GUniqueOutPtr<GError> error;
    auto parse = [&](const gchar* manifestString) {
        return adoptGRef(webkitWebExtensionCreate({ { "manifest.json"_s, createGBytes(manifestString) } }, &error.outPtr()));
    };

    GRefPtr<WebKitWebExtension> extension = parse("{ \"manifest_version\": 3, \"options_page\": \"options.html\", \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\" }");
    g_assert_no_error(error.get());
    g_assert_true(webkit_web_extension_get_has_options_page(extension.get()));

    extension = parse("{ \"manifest_version\": 3, \"options_page\": \"\", \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\" }");
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);
    g_assert_false(webkit_web_extension_get_has_options_page(extension.get()));

    extension = parse("{ \"manifest_version\": 3, \"options_page\": 123, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\" }");
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);
    g_assert_false(webkit_web_extension_get_has_options_page(extension.get()));

    extension = parse("{ \"manifest_version\": 3, \"options_ui\": { \"page\": \"options.html\" }, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\" }");
    g_assert_no_error(error.get());
    g_assert_true(webkit_web_extension_get_has_options_page(extension.get()));

    extension = parse("{ \"manifest_version\": 3, \"options_ui\": { \"bad\": \"options.html\" }, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\" }");
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);
    g_assert_false(webkit_web_extension_get_has_options_page(extension.get()));

    extension = parse("{ \"manifest_version\": 3, \"options_ui\": { \"page\": 123 }, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\" }");
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);
    g_assert_false(webkit_web_extension_get_has_options_page(extension.get()));

    extension = parse("{ \"manifest_version\": 3, \"options_ui\": { }, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\" }");
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);
    g_assert_false(webkit_web_extension_get_has_options_page(extension.get()));

    extension = parse("{ \"manifest_version\": 3, \"options_ui\": { \"page\": \"\" }, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\" }");
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);
    g_assert_false(webkit_web_extension_get_has_options_page(extension.get()));
}

static void testURLOverridesParsing(Test*, gconstpointer)
{
    GUniqueOutPtr<GError> error;
    auto parse = [&](const gchar* manifestString) {
        return adoptGRef(webkitWebExtensionCreate({ { "manifest.json"_s, createGBytes(manifestString) } }, &error.outPtr()));
    };

    GRefPtr<WebKitWebExtension> extension = parse("{ \"manifest_version\": 3, \"browser_url_overrides\": { \"newtab\": \"newtab.html\" }, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\" }");
    g_assert_no_error(error.get());
    g_assert_true(webkit_web_extension_get_has_override_new_tab_page(extension.get()));

    extension = parse("{ \"manifest_version\": 3, \"browser_url_overrides\": { \"bad\": \"newtab.html\" }, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\" }");
    g_assert_no_error(error.get());
    g_assert_false(webkit_web_extension_get_has_override_new_tab_page(extension.get()));

    extension = parse("{ \"manifest_version\": 3, \"browser_url_overrides\": { \"newtab\": 123 }, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\" }");
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);
    g_assert_false(webkit_web_extension_get_has_override_new_tab_page(extension.get()));

    extension = parse("{ \"manifest_version\": 3, \"browser_url_overrides\": { }, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\" }");
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);
    g_assert_false(webkit_web_extension_get_has_override_new_tab_page(extension.get()));

    extension = parse("{ \"manifest_version\": 3, \"browser_url_overrides\": { \"newtab\": \"\" }, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\" }");
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);
    g_assert_false(webkit_web_extension_get_has_override_new_tab_page(extension.get()));

    extension = parse("{ \"manifest_version\": 3, \"chrome_url_overrides\": { \"newtab\": \"newtab.html\" }, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\" }");
    g_assert_no_error(error.get());
    g_assert_true(webkit_web_extension_get_has_override_new_tab_page(extension.get()));

    extension = parse("{ \"manifest_version\": 3, \"chrome_url_overrides\": { \"bad\": \"newtab.html\" }, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\" }");
    g_assert_no_error(error.get());
    g_assert_false(webkit_web_extension_get_has_override_new_tab_page(extension.get()));

    extension = parse("{ \"manifest_version\": 3, \"chrome_url_overrides\": { \"newtab\": 123 }, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\" }");
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);
    g_assert_false(webkit_web_extension_get_has_override_new_tab_page(extension.get()));

    extension = parse("{ \"manifest_version\": 3, \"chrome_url_overrides\": { }, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\" }");
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);
    g_assert_false(webkit_web_extension_get_has_override_new_tab_page(extension.get()));

    extension = parse("{ \"manifest_version\": 3, \"chrome_url_overrides\": { \"newtab\": \"\" }, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\" }");
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);
    g_assert_false(webkit_web_extension_get_has_override_new_tab_page(extension.get()));
}

static void testContentSecurityPolicyParsing(Test*, gconstpointer)
{
    GUniqueOutPtr<GError> error;
    auto parse = [&](const gchar* manifestString) {
        return adoptGRef(webkitWebExtensionCreate({ { "manifest.json"_s, createGBytes(manifestString) } }, &error.outPtr()));
    };

    // Manifest V3
    parse("{ \"manifest_version\": 3, \"content_security_policy\": { \"extension_pages\": \"script-src 'self'; object-src 'self'\" }, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\" }");
    g_assert_no_error(error.get());

    parse("{ \"manifest_version\": 3, \"content_security_policy\": { \"sandbox\": \"sandbox allow-scripts allow-forms allow-popups allow-modals; script-src 'self'\" }, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\" }");
    g_assert_no_error(error.get());

    parse("{ \"manifest_version\": 3, \"content_security_policy\": { }, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\" }");
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);

    parse("{ \"manifest_version\": 2, \"content_security_policy\": { \"extension_pages\": 123 }, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\" }");
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);

    // Manifest V2
    parse("{ \"manifest_version\": 2, \"content_security_policy\": \"script-src 'self'; object-src 'self'\", \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\" }");
    g_assert_no_error(error.get());

    parse("{ \"manifest_version\": 2, \"content_security_policy\": [ \"invalid\", \"type\" ], \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\" }");
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);

    parse("{ \"manifest_version\": 2, \"content_security_policy\": 123, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\" }");
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);

    parse("{ \"manifest_version\": 2, \"content_security_policy\": { \"extension_pages\": \"script-src 'self'; object-src 'self'\" }, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\" }");
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);
}

static void testWebAccessibleResourcesV2(Test*, gconstpointer)
{
    GUniqueOutPtr<GError> error;
    auto parse = [&](const gchar* manifestString) {
        return adoptGRef(webkitWebExtensionCreate({ { "manifest.json"_s, createGBytes(manifestString) } }, &error.outPtr()));
    };

    parse("{ \"manifest_version\": 2, \"web_accessible_resources\": [ \"images/*.png\", \"styles/*.css\" ], \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\" }");
    g_assert_no_error(error.get());

    parse("{ \"manifest_version\": 2, \"web_accessible_resources\": [ ], \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\" }");
    g_assert_no_error(error.get());

    parse("{ \"manifest_version\": 2, \"web_accessible_resources\": \"bad\", \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\" }");
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);

    parse("{ \"manifest_version\": 2, \"web_accessible_resources\": { }, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\" }");
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);
}

static void testWebAccessibleResourcesV3(Test*, gconstpointer)
{
    GUniqueOutPtr<GError> error;
    auto parse = [&](const gchar* manifestString) {
        return adoptGRef(webkitWebExtensionCreate({ { "manifest.json"_s, createGBytes(manifestString) } }, &error.outPtr()));
    };

    parse("{ \"web_accessible_resources\": [ { \"resources\": [ \"images/*.png\", \"styles/*.css\" ], \"matches\": [ \"<all_urls>\" ] }, { \"resources\": [ \"scripts/*.js\" ], \"matches\": [ \"*://localhost/*\" ] } ], \"manifest_version\": 3, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\" }");
    g_assert_no_error(error.get());

    parse("{ \"web_accessible_resources\": [ { \"resources\": [], \"matches\": [] } ], \"manifest_version\": 3, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\" }");
    g_assert_no_error(error.get());

    parse("{ \"web_accessible_resources\": [ { \"resources\": \"bad\", \"matches\": [ \"<all_urls>\" ] } ], \"manifest_version\": 3, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\" }");
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);

    parse("{ \"web_accessible_resources\": [ { \"resources\": [ \"images/*.png\" ], \"matches\": \"bad\" } ], \"manifest_version\": 3, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\" }");
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);

    parse("{ \"web_accessible_resources\": [ { \"matches\": [ \"<all_urls>\" ] } ], \"manifest_version\": 3, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\" }");
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);

    parse("{ \"web_accessible_resources\": [ { \"resources\": [] } ], \"manifest_version\": 3, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\" }");
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);
}

static void testCommandsParsing(Test*, gconstpointer)
{
    GUniqueOutPtr<GError> error;
    auto parse = [&](const gchar* manifestString) {
        return adoptGRef(webkitWebExtensionCreate({ { "manifest.json"_s, createGBytes(manifestString) } }, &error.outPtr()));
    };

    GRefPtr<WebKitWebExtension> extension = parse("{ \"commands\": { \"show-popup\": { \"suggested_key\": { \"default\": \"Ctrl+Shift+P\", \"linux\": \"Ctrl+Shift+A\" }, \"description\": \"Show the popup\" } }, \"manifest_version\": 3, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\" }");
    g_assert_true(webkit_web_extension_get_has_commands(extension.get()));
    g_assert_no_error(error.get());

    extension = parse("{ \"commands\": { }, \"action\": { \"default_title\": \"Test Action\" }, \"manifest_version\": 3, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\" }");
    g_assert_true(webkit_web_extension_get_has_commands(extension.get()));
    g_assert_no_error(error.get());

    extension = parse("{ \"commands\": { }, \"browser_action\": { \"default_title\": \"Test Action\" }, \"manifest_version\": 2, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\" }");
    g_assert_true(webkit_web_extension_get_has_commands(extension.get()));
    g_assert_no_error(error.get());

    extension = parse("{ \"commands\": { }, \"page_action\": { \"default_title\": \"Test Action\" }, \"manifest_version\": 2, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\" }");
    g_assert_true(webkit_web_extension_get_has_commands(extension.get()));
    g_assert_no_error(error.get());

    extension = parse("{ \"manifest_version\": 3, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\" }");
    g_assert_false(webkit_web_extension_get_has_commands(extension.get()));
    g_assert_no_error(error.get());

    extension = parse("{ \"action\": { \"default_title\": \"Test Action\" }, \"manifest_version\": 3, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\" }");
    g_assert_true(webkit_web_extension_get_has_commands(extension.get()));
    g_assert_no_error(error.get());

    extension = parse("{ \"browser_action\": { \"default_title\": \"Test Action\" }, \"manifest_version\": 2, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\" }");
    g_assert_true(webkit_web_extension_get_has_commands(extension.get()));
    g_assert_no_error(error.get());

    extension = parse("{ \"page_action\": { \"default_title\": \"Test Action\" }, \"manifest_version\": 2, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\" }");
    g_assert_true(webkit_web_extension_get_has_commands(extension.get()));
    g_assert_no_error(error.get());

    extension = parse("{ \"commands\": { \"show-popup\": \"Invalid\" }, \"manifest_version\": 3, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\" }");
    g_assert_false(webkit_web_extension_get_has_commands(extension.get()));
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);
}

static void testDeclarativeNetRequestParsing(Test*, gconstpointer)
{
    GUniqueOutPtr<GError> error;
    auto parse = [&](const gchar* manifestString) {
        return adoptGRef(webkitWebExtensionCreate({ { "manifest.json"_s, createGBytes(manifestString) } }, &error.outPtr()));
    };

    GRefPtr<WebKitWebExtension> extension = parse("{ \"declarative_net_request\": { \"rule_resources\": [{ \"id\": \"test\", \"enabled\": true, \"path\": \"rules.json\" }] }, \"manifest_version\": 3, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\", \"permissions\": [ \"declarativeNetRequest\"] }");
    g_assert_true(webkit_web_extension_get_has_content_modification_rules(extension.get()));
    g_assert_no_error(error.get());

    // Missing id
    extension = parse("{ \"declarative_net_request\": { \"rule_resources\": [{ \"enabled\": true, \"path\": \"rules.json\" }] }, \"manifest_version\": 3, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\", \"permissions\": [ \"declarativeNetRequest\"] }");
    g_assert_false(webkit_web_extension_get_has_content_modification_rules(extension.get()));
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_DECLARATIVE_NET_REQUEST_ENTRY);

    // Missing enabled
    extension = parse("{ \"declarative_net_request\": { \"rule_resources\": [{ \"id\": \"test\", \"path\": \"rules.json\" }] }, \"manifest_version\": 3, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\", \"permissions\": [ \"declarativeNetRequest\"] }");
    g_assert_false(webkit_web_extension_get_has_content_modification_rules(extension.get()));
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_DECLARATIVE_NET_REQUEST_ENTRY);

    // Missing path
    extension = parse("{ \"declarative_net_request\": { \"rule_resources\": [{ \"id\": \"test\", \"enabled\": true }] }, \"manifest_version\": 3, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\", \"permissions\": [ \"declarativeNetRequest\"] }");
    g_assert_false(webkit_web_extension_get_has_content_modification_rules(extension.get()));
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_DECLARATIVE_NET_REQUEST_ENTRY);

    // Duplicate names
    extension = parse("{ \"declarative_net_request\": { \"rule_resources\": [{ \"id\": \"test\", \"enabled\": true, \"path\": \"rules.json\" }, { \"id\": \"test\", \"enabled\": true, \"path\": \"rules2.json\" }] }, \"manifest_version\": 3, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\", \"permissions\": [ \"declarativeNetRequest\"] }");
    // The first rule should be loaded
    g_assert_true(webkit_web_extension_get_has_content_modification_rules(extension.get()));
    // But an error should still be emitted
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_DECLARATIVE_NET_REQUEST_ENTRY);

    // One valid rule, one invalid rule
    extension = parse("{ \"declarative_net_request\": { \"rule_resources\": [{ \"id\": \"test\", \"enabled\": true, \"path\": \"rules.json\" }, { \"enabled\": true, \"path\": \"rules2.json\" }] }, \"manifest_version\": 3, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\", \"permissions\": [ \"declarativeNetRequest\"] }");
    // The first rule should be loaded
    g_assert_true(webkit_web_extension_get_has_content_modification_rules(extension.get()));
    // But an error should still be emitted
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_DECLARATIVE_NET_REQUEST_ENTRY);

    // No rules
    extension = parse("{ \"declarative_net_request\": { \"rule_resources\": [] }, \"manifest_version\": 3, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\", \"permissions\": [ \"declarativeNetRequest\"] }");
    g_assert_false(webkit_web_extension_get_has_content_modification_rules(extension.get()));
    g_assert_no_error(error.get());
}

static void testExternallyConnectableParsing(Test*, gconstpointer)
{
    GUniqueOutPtr<GError> error;
    auto parse = [&](const gchar* manifestString) {
        return adoptGRef(webkitWebExtensionCreate({ { "manifest.json"_s, createGBytes(manifestString) } }, &error.outPtr()));
    };

    GRefPtr<WebKitWebExtension> extension = parse("{ \"externally_connectable\": {}, \"manifest_version\": 3, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\" }");
    g_assert_null(webkit_web_extension_get_all_requested_match_patterns(extension.get()));
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);

    // Expect an error since 'externally_connectable' is specified, but there are no valid match patterns or extension ids.
    extension = parse("{ \"externally_connectable\": { \"matches\": [] }, \"manifest_version\": 3, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\" }");
    g_assert_null(webkit_web_extension_get_all_requested_match_patterns(extension.get()));
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);

    extension = parse("{ \"externally_connectable\": { \"matches\": [ \"\" ] }, \"manifest_version\": 3, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\" }");
    g_assert_null(webkit_web_extension_get_all_requested_match_patterns(extension.get()));
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);

    extension = parse("{ \"externally_connectable\": { \"matches\": [], \"ids\": [ \"\" ] }, \"manifest_version\": 3, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\" }");
    g_assert_null(webkit_web_extension_get_all_requested_match_patterns(extension.get()));
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);

    // Expect an error if <all_urls> is specified.
    extension = parse("{ \"externally_connectable\": { \"matches\": [ \"<all_urls>\" ] }, \"manifest_version\": 3, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\" }");
    g_assert_null(webkit_web_extension_get_all_requested_match_patterns(extension.get()));
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);

    // Still expect the errork, but have a valid match pattern
    extension = parse("{ \"externally_connectable\": { \"matches\": [ \"*://*.example.com/\", \"<all_urls>\" ] }, \"manifest_version\": 3, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\" }");
    guint patternLength = 0;
    auto matchPatterns = webkit_web_extension_get_all_requested_match_patterns(extension.get());
    for (; *matchPatterns != nullptr; matchPatterns++)
        patternLength++;
    g_assert_cmpint(patternLength, ==, 1);
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);

    // Expect an error for not having a second level domain.
    extension = parse("{ \"externally_connectable\": { \"matches\": [ \"*://*.com/\" ] }, \"manifest_version\": 3, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\" }");
    g_assert_null(webkit_web_extension_get_all_requested_match_patterns(extension.get()));
    g_assert_error(error.get(), WEBKIT_WEB_EXTENSION_ERROR, WEBKIT_WEB_EXTENSION_ERROR_INVALID_MANIFEST_ENTRY);

    // Match for *://*.example.com/*
    auto* matchingPattern = webkit_web_extension_match_pattern_new_with_string("*://*.example.com/", nullptr);
    extension = parse("{ \"externally_connectable\": { \"matches\": [ \"*://*.example.com/\" ] }, \"manifest_version\": 3, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\" }");
    patternLength = 0;
    matchPatterns = webkit_web_extension_get_all_requested_match_patterns(extension.get());
    for (; *matchPatterns != nullptr; matchPatterns++)
        patternLength++;
    g_assert_cmpint(patternLength, ==, 1);
    g_assert_no_error(error.get());
    g_assert_true(webkit_web_extension_match_pattern_matches_pattern(webkit_web_extension_get_all_requested_match_patterns(extension.get())[0], matchingPattern, WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE));

    extension = parse("{ \"externally_connectable\": { \"matches\": [ \"*://*.example.com/\" ], \"ids\": [ \"*\"] }, \"manifest_version\": 3, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\" }");
    patternLength = 0;
    matchPatterns = webkit_web_extension_get_all_requested_match_patterns(extension.get());
    for (; *matchPatterns != nullptr; matchPatterns++)
        patternLength++;
    g_assert_cmpint(patternLength, ==, 1);
    g_assert_no_error(error.get());

    extension = parse("{ \"externally_connectable\": { \"ids\": [ \"*\"] }, \"manifest_version\": 3, \"name\": \"Test\", \"description\": \"Test\", \"version\": \"1.0\" }");
    g_assert_null(webkit_web_extension_get_all_requested_match_patterns(extension.get()));
    g_assert_no_error(error.get());

    // FIXME: <https://webkit.org/b/269299> Add more tests for externally_connectable "ids" keys.
}

void beforeAll()
{
    Test::add("WebKitWebExtension", "creation-from-directory", testExtensionCreationFromDirectory);
    Test::add("WebKitWebExtension", "display-string-parsing", testDisplayStringParsing);
    Test::add("WebKitWebExtension", "default-locale-parsing", testDefaultLocaleParsing);
    Test::add("WebKitWebExtension", "display-string-parsing-with-localization", testDisplayStringParsingWithLocalization);
    Test::add("WebKitWebExtension", "action-parsing", testActionParsing);
    Test::add("WebKitWebExtension", "content-scripts-parsing", testContentScriptsParsing);
    Test::add("WebKitWebExtension", "permissions-parsing", testPermissionsParsing);
    Test::add("WebKitWebExtension", "background-parsing", testBackgroundParsing);
    Test::add("WebKitWebExtension", "background-preferred-environment-parsing", testBackgroundPreferredEnvironmentParsing);
    Test::add("WebKitWebExtension", "options-page-parsing", testOptionsPageParsing);
    Test::add("WebKitWebExtension", "url-overrides-parsing", testURLOverridesParsing);
    Test::add("WebKitWebExtension", "content-security-policy-parsing", testContentSecurityPolicyParsing);
    Test::add("WebKitWebExtension", "web-accessible-resources-v2", testWebAccessibleResourcesV2);
    Test::add("WebKitWebExtension", "web-accessible-resources-v3", testWebAccessibleResourcesV3);
    Test::add("WebKitWebExtension", "commands", testCommandsParsing);
    Test::add("WebKitWebExtension", "declarative-net-request", testDeclarativeNetRequestParsing);
    Test::add("WebKitWebExtension", "externally-connectable", testExternallyConnectableParsing);
}

void afterAll()
{
}

#endif // ENABLE(WK_WEB_EXTENSIONS)
