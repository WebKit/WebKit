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
#include <WebKitWebExtensionMatchPattern.h>

using namespace TestWebKitAPI;

static WebKitWebExtensionMatchPattern* toPattern(const char* string, GError** error)
{
    return webkit_web_extension_match_pattern_new_with_string (string, error);
}

static WebKitWebExtensionMatchPattern* toPattern(const char* scheme, const char* host, const char* path, GError** error)
{
    return webkit_web_extension_match_pattern_new_with_scheme (scheme, host, path, error);
}

static void testPatternValidity(Test*, gconstpointer)
{
    GUniqueOutPtr<GError> error;

    g_assert_null(toPattern("", &error.outPtr()));
    g_assert_cmpstr(error.get()->message, ==, "\"\" cannot be parsed because it doesn't have a scheme.");

    g_assert_null(toPattern("http://www.example.com", &error.outPtr()));
    g_assert_cmpstr(error.get()->message, ==, "\"http://www.example.com\" cannot be parsed because it doesn't have a path.");

    g_assert_null(toPattern("http://www.example.com:8080/", &error.outPtr()));
    g_assert_cmpstr(error.get()->message, ==, "\"http://www.example.com:8080/\" cannot be parsed because the host \"www.example.com:8080\" is invalid.");

    g_assert_null(toPattern("http://[::1]:8080/", &error.outPtr()));
    g_assert_cmpstr(error.get()->message, ==, "\"http://[::1]:8080/\" cannot be parsed because the host \"[::1]:8080\" is invalid.");

    g_assert_null(toPattern("http://user@www.example.com/", &error.outPtr()));
    g_assert_cmpstr(error.get()->message, ==, "\"http://user@www.example.com/\" cannot be parsed because the host \"user@www.example.com\" is invalid.");

    g_assert_null(toPattern("http://user:password@www.example.com/", &error.outPtr()));
    g_assert_cmpstr(error.get()->message, ==, "\"http://user:password@www.example.com/\" cannot be parsed because the host \"user:password@www.example.com\" is invalid.");

    g_assert_null(toPattern("file://localhost", &error.outPtr()));
    g_assert_cmpstr(error.get()->message, ==, "\"file://localhost\" cannot be parsed because it doesn't have a path.");

    g_assert_null(toPattern("file://", &error.outPtr()));
    g_assert_cmpstr(error.get()->message, ==, "\"file://\" cannot be parsed because it doesn't have a path.");

    g_assert_null(toPattern("http://*foo/bar", &error.outPtr()));
    g_assert_cmpstr(error.get()->message, ==, "\"http://*foo/bar\" cannot be parsed because the host \"*foo\" is invalid.");

    g_assert_null(toPattern("http://foo.*.bar/baz", &error.outPtr()));
    g_assert_cmpstr(error.get()->message, ==, "\"http://foo.*.bar/baz\" cannot be parsed because the host \"foo.*.bar\" is invalid.");

    g_assert_null(toPattern("http:/bar", &error.outPtr()));
    g_assert_cmpstr(error.get()->message, ==, "\"http:/bar\" cannot be parsed because it doesn't have a scheme.");

    g_assert_null(toPattern("foo://*", &error.outPtr()));
    g_assert_cmpstr(error.get()->message, ==, "\"foo://*\" cannot be parsed because the scheme \"foo\" is invalid.");

    g_assert_null(toPattern("foo", "*", "/", &error.outPtr()));
    g_assert_cmpstr(error.get()->message, ==, "Scheme \"foo\" is invalid.");

    g_assert_null(toPattern("https", "example.*", "/", &error.outPtr()));
    g_assert_cmpstr(error.get()->message, ==, "Host \"example.*\" is invalid.");

    g_assert_null(toPattern("https", "*.example.com:8080", "/", &error.outPtr()));
    g_assert_cmpstr(error.get()->message, ==, "Host \"*.example.com:8080\" is invalid.");

    g_assert_null(toPattern("https", "[::1]:8080", "/", &error.outPtr()));
    g_assert_cmpstr(error.get()->message, ==, "Host \"[::1]:8080\" is invalid.");

    g_assert_null(toPattern("https", "user@example.*", "/", &error.outPtr()));
    g_assert_cmpstr(error.get()->message, ==, "Host \"user@example.*\" is invalid.");

    g_assert_null(toPattern("https", "user@example.*", "/", &error.outPtr()));
    g_assert_cmpstr(error.get()->message, ==, "Host \"user@example.*\" is invalid.");

    g_assert_null(toPattern("https", "user:password@example.*", "/", &error.outPtr()));
    g_assert_cmpstr(error.get()->message, ==, "Host \"user:password@example.*\" is invalid.");

    g_assert_null(toPattern("https", "example.com", "*", &error.outPtr()));
    g_assert_cmpstr(error.get()->message, ==, "Path \"*\" is invalid.");
}

static void testMatchesPattern(Test*, gconstpointer)
{
    // Matches any URL that uses the http scheme.
    g_assert_true(webkit_web_extension_match_pattern_matches_pattern(
        toPattern("http://*/*", nullptr),
        toPattern("http://www.example.com/", nullptr),
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));
    g_assert_true(webkit_web_extension_match_pattern_matches_pattern(
        toPattern("http://*/*", nullptr),
        toPattern("http://example.com/foo/bar.html", nullptr),
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));

    // Matches any URL that uses the http scheme, on any host, as long as the path starts with /foo.
    g_assert_true(webkit_web_extension_match_pattern_matches_pattern(
        toPattern("http://*/foo*", nullptr),
        toPattern("http://example.com/foo/bar.html", nullptr),
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));
    g_assert_true(webkit_web_extension_match_pattern_matches_pattern(
        toPattern("http://*/*", nullptr),
        toPattern("http://example.com/foo", nullptr),
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));

    // Matches any URL that uses the https scheme, is on a example.com host (such as www.example.com, bar.example.com,
    // or example.com), as long as the path starts with /foo and ends with bar.
    g_assert_true(webkit_web_extension_match_pattern_matches_pattern(
        toPattern("https://*.example.com/foo*bar", nullptr),
        toPattern("https://www.example.com/foo/baz/bar", nullptr),
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));
    g_assert_true(webkit_web_extension_match_pattern_matches_pattern(
        toPattern("https://*.example.com/foo*bar", nullptr),
        toPattern("https://bar.example.com/foobar", nullptr),
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));

    // Matches the specified URL.
    g_assert_true(webkit_web_extension_match_pattern_matches_pattern(
        toPattern("http://example.com/foo/bar.html", nullptr),
        toPattern("http://example.com/foo/bar.html", nullptr),
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));

    // Matches any file whose path starts with /foo.
    g_assert_true(webkit_web_extension_match_pattern_matches_pattern(
        toPattern("file:///foo*", nullptr),
        toPattern("file:///foo/bar.html", nullptr),
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));
    g_assert_true(webkit_web_extension_match_pattern_matches_pattern(
        toPattern("file:///foo*", nullptr),
        toPattern("file:///foo", nullptr),
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));
    g_assert_true(webkit_web_extension_match_pattern_matches_pattern(
        toPattern("file://localhost/foo*", nullptr),
        toPattern("file://localhost/foo", nullptr),
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));
    g_assert_true(webkit_web_extension_match_pattern_matches_pattern(
        toPattern("file://localhost/foo*", nullptr),
        toPattern("file:///foo", nullptr),
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));
    g_assert_true(webkit_web_extension_match_pattern_matches_pattern(
        toPattern("file:///foo*", nullptr),
        toPattern("file://localhost/foo", nullptr),
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));
    g_assert_true(webkit_web_extension_match_pattern_matches_pattern(
        toPattern("file://*/foo*", nullptr),
        toPattern("file:///foo/bar.html", nullptr),
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));
    g_assert_true(webkit_web_extension_match_pattern_matches_pattern(
        toPattern("file://*/foo*", nullptr),
        toPattern("file://localhost/foo/bar.html", nullptr),
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));
    g_assert_true(webkit_web_extension_match_pattern_matches_pattern(
        toPattern("file://*/foo*", nullptr),
        toPattern("file://test.local/foo/bar.html", nullptr),
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));
    g_assert_true(webkit_web_extension_match_pattern_matches_pattern(
        toPattern("file://*/foo*", nullptr),
        toPattern("file://apple.com/foo/bar.html", nullptr),
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));
    g_assert_true(webkit_web_extension_match_pattern_matches_pattern(
        toPattern("file://*.local/foo*", nullptr),
        toPattern("file://test.local/foo", nullptr),
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));
    g_assert_false(webkit_web_extension_match_pattern_matches_pattern(
        toPattern("file://*.local/foo*", nullptr),
        toPattern("file://apple.com/foo", nullptr),
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));

    // Matches ignoring scheme.
    g_assert_false(webkit_web_extension_match_pattern_matches_pattern(
        toPattern("http://*.example.com/*", nullptr),
        toPattern("https://*.example.com/*", nullptr),
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));
    g_assert_false(webkit_web_extension_match_pattern_matches_pattern(
        toPattern("https://*.example.com/*", nullptr),
        toPattern("http://*.example.com/*", nullptr),
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));
    g_assert_false(webkit_web_extension_match_pattern_matches_pattern(
        toPattern("http://*.example.com/*", nullptr),
        toPattern("*://*.example.com/*", nullptr),
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));
    g_assert_true(webkit_web_extension_match_pattern_matches_pattern(
        toPattern("http://*.example.com/*", nullptr),
        toPattern("https://*.example.com/*", nullptr),
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_IGNORE_SCHEMES
    ));
    g_assert_true(webkit_web_extension_match_pattern_matches_pattern(
        toPattern("https://*.example.com/*", nullptr),
        toPattern("http://*.example.com/*", nullptr),
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_IGNORE_SCHEMES
    ));
    g_assert_true(webkit_web_extension_match_pattern_matches_pattern(
        toPattern("http://*.example.com/*", nullptr),
        toPattern("*://*.example.com/*", nullptr),
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_IGNORE_SCHEMES
    ));

    // Matches ignoring path.
    g_assert_false(webkit_web_extension_match_pattern_matches_pattern(
        toPattern("https://*.example.com/foo*bar", nullptr),
        toPattern("https://www.example.com/baz", nullptr),
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));
    g_assert_false(webkit_web_extension_match_pattern_matches_pattern(
        toPattern("*://*.example.com/foo*bar", nullptr),
        toPattern("http://www.example.com/test", nullptr),
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));
    g_assert_false(webkit_web_extension_match_pattern_matches_pattern(
        toPattern("*://*.example.com/test*", nullptr),
        toPattern("*://*.example.com/bar", nullptr),
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));
    g_assert_false(webkit_web_extension_match_pattern_matches_pattern(
        toPattern("*://*.example.com/*bar", nullptr),
        toPattern("*://example.com/baz", nullptr),
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));
    g_assert_true(webkit_web_extension_match_pattern_matches_pattern(
        toPattern("https://*.example.com/foo*bar", nullptr),
        toPattern("https://www.example.com/baz", nullptr),
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_IGNORE_PATHS
    ));
    g_assert_true(webkit_web_extension_match_pattern_matches_pattern(
        toPattern("*://*.example.com/foo*bar", nullptr),
        toPattern("http://www.example.com/test", nullptr),
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_IGNORE_PATHS
    ));
    g_assert_true(webkit_web_extension_match_pattern_matches_pattern(
        toPattern("*://*.example.com/test*", nullptr),
        toPattern("*://*.example.com/bar", nullptr),
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_IGNORE_PATHS
    ));
    g_assert_true(webkit_web_extension_match_pattern_matches_pattern(
        toPattern("*://*.example.com/*bar", nullptr),
        toPattern("*://example.com/baz", nullptr),
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_IGNORE_PATHS
    ));

    // Matches any URL that uses the http scheme and is on the host 127.0.0.1.
    g_assert_true(webkit_web_extension_match_pattern_matches_pattern(
        toPattern("http://127.0.0.1/*", nullptr),
        toPattern("http://127.0.0.1/", nullptr),
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));
    g_assert_true(webkit_web_extension_match_pattern_matches_pattern(
        toPattern("http://127.0.0.1/*", nullptr),
        toPattern("http://127.0.0.1/foo/bar.html", nullptr),
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));

    // Matches any URL that uses the http scheme and is on the host [::1].
    g_assert_true(webkit_web_extension_match_pattern_matches_pattern(
        toPattern("http://[::1]/*", nullptr),
        toPattern("http://[::1]/", nullptr),
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));
    g_assert_true(webkit_web_extension_match_pattern_matches_pattern(
        toPattern("http://[::1]/*", nullptr),
        toPattern("http://[::1]/foo/bar.html", nullptr),
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));

    // Matches any URL that starts with http://foo.example.com or https://foo.example.com.
    g_assert_true(webkit_web_extension_match_pattern_matches_pattern(
        toPattern("*://foo.example.com/*", nullptr),
        toPattern("http://foo.example.com/foo/baz/bar", nullptr),
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));
    g_assert_true(webkit_web_extension_match_pattern_matches_pattern(
        toPattern("*://foo.example.com/*", nullptr),
        toPattern("https://foo.example.com/foobar", nullptr),
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));

    // Test missing hosts.
    g_assert_false(webkit_web_extension_match_pattern_matches_pattern(
        toPattern("*:///*", nullptr),
        toPattern("https://example.com/foobar", nullptr),
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));
    g_assert_false(webkit_web_extension_match_pattern_matches_pattern(
        toPattern("https:///*", nullptr),
        toPattern("https://example.com/foobar", nullptr),
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));
    g_assert_false(webkit_web_extension_match_pattern_matches_pattern(
        toPattern("ftp:///*", nullptr),
        toPattern("ftp://example.com/foobar", nullptr),
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));
    g_assert_true(webkit_web_extension_match_pattern_matches_pattern(
        toPattern("file:///*", nullptr),
        toPattern("file:///foobar", nullptr),
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));

    // Matches any URL that uses a permitted scheme. (See the beginning of this section for the list of permitted schemes.)
    g_assert_true(webkit_web_extension_match_pattern_matches_pattern(
        toPattern("<all_urls>", nullptr),
        toPattern("http://example.com/foo/bar.html", nullptr),
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));
    g_assert_false(webkit_web_extension_match_pattern_matches_pattern(
        toPattern("<all_urls>", nullptr),
        toPattern("file:///bar/baz.html", nullptr),
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));

    // All matches.
    g_assert_true(webkit_web_extension_match_pattern_matches_pattern(
        toPattern("<all_urls>", nullptr),
        toPattern("<all_urls>", nullptr),
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));
    g_assert_true(webkit_web_extension_match_pattern_matches_pattern(
        toPattern("<all_urls>", nullptr),
        toPattern("*://*/*", nullptr),
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));
    g_assert_false(webkit_web_extension_match_pattern_matches_pattern(
        toPattern("*://*/*", nullptr),
        toPattern("<all_urls>", nullptr),
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));
    g_assert_true(webkit_web_extension_match_pattern_matches_pattern(
        toPattern("*://*/*", nullptr),
        toPattern("*://*/*", nullptr),
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));

    // Matching domain patterns.
    g_assert_true(webkit_web_extension_match_pattern_matches_pattern(
        toPattern("*://*.example.com/*", nullptr),
        toPattern("*://www.example.com/test/*", nullptr),
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));
    g_assert_true(webkit_web_extension_match_pattern_matches_pattern(
        toPattern("*://*/*", nullptr),
        toPattern("*://*.example.com/*", nullptr),
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));
    g_assert_false(webkit_web_extension_match_pattern_matches_pattern(
        toPattern("*://*.example.com/*", nullptr),
        toPattern("*://*/*", nullptr),
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));
    g_assert_true(webkit_web_extension_match_pattern_matches_pattern(
        toPattern("<all_urls>", nullptr),
        toPattern("*://*.example.com/*", nullptr),
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));
    g_assert_true(webkit_web_extension_match_pattern_matches_pattern(
        toPattern("*://*.example.com/*", nullptr),
        toPattern("*://www.example.com/test/*", nullptr),
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));
    g_assert_false(webkit_web_extension_match_pattern_matches_pattern(
        toPattern("*://*.example.com/*", nullptr),
        toPattern("*://the-example.com/test/*", nullptr),
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));
    g_assert_false(webkit_web_extension_match_pattern_matches_pattern(
        toPattern("*://*.example.com/*", nullptr),
        toPattern("*://www.the-example.com/test/*", nullptr),
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));

    // Bidirectional matching.
    g_assert_false(webkit_web_extension_match_pattern_matches_pattern(
        toPattern("*://*.example.com/*", nullptr),
        toPattern("*://*/*", nullptr),
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));
    g_assert_false(webkit_web_extension_match_pattern_matches_pattern(
        toPattern("*://*.example.com/*", nullptr),
        toPattern("<all_urls>", nullptr),
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));
    g_assert_false(webkit_web_extension_match_pattern_matches_pattern(
        toPattern("http://*.example.com/*", nullptr),
        toPattern("<all_urls>", nullptr),
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));
    g_assert_false(webkit_web_extension_match_pattern_matches_pattern(
        toPattern("ftp://*.example.com/*", nullptr),
        toPattern("<all_urls>", nullptr),
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));
    g_assert_false(webkit_web_extension_match_pattern_matches_pattern(
        toPattern("*://*.en.wikipedia.org/*", nullptr),
        toPattern("*://*.wikipedia.org/*", nullptr),
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));
    g_assert_false(webkit_web_extension_match_pattern_matches_pattern(
        toPattern("https://*.en.wikipedia.org/*", nullptr),
        toPattern("*://*.wikipedia.org/*", nullptr),
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));
    g_assert_false(webkit_web_extension_match_pattern_matches_pattern(
        toPattern("*://*.en.wikipedia.org/*", nullptr),
        toPattern("https://*.wikipedia.org/*", nullptr),
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));
    g_assert_false(webkit_web_extension_match_pattern_matches_pattern(
        toPattern("https://*/*", nullptr),
        toPattern("*://*.example.com/*", nullptr),
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));
    g_assert_false(webkit_web_extension_match_pattern_matches_pattern(
        toPattern("http://*/*", nullptr),
        toPattern("*://*.example.com/*", nullptr),
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));
    g_assert_false(webkit_web_extension_match_pattern_matches_pattern(
        toPattern("http://*/*", nullptr),
        toPattern("*://*/foo*", nullptr),
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));
    g_assert_false(webkit_web_extension_match_pattern_matches_pattern(
        toPattern("*://*.example.com/*", nullptr),
        toPattern("https://*/*", nullptr),
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));
    g_assert_false(webkit_web_extension_match_pattern_matches_pattern(
        toPattern("*://*.example.com/*", nullptr),
        toPattern("http://*/*", nullptr),
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));
    g_assert_true(webkit_web_extension_match_pattern_matches_pattern(
        toPattern("*://*.example.com/*", nullptr),
        toPattern("*://*/*", nullptr),
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_MATCH_BIDIRECTIONALLY
    ));
    g_assert_true(webkit_web_extension_match_pattern_matches_pattern(
        toPattern("*://*.example.com/*", nullptr),
        toPattern("<all_urls>", nullptr),
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_MATCH_BIDIRECTIONALLY
    ));
    g_assert_true(webkit_web_extension_match_pattern_matches_pattern(
        toPattern("http://*.example.com/*", nullptr),
        toPattern("<all_urls>", nullptr),
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_MATCH_BIDIRECTIONALLY
    ));
    g_assert_false(webkit_web_extension_match_pattern_matches_pattern(
        toPattern("ftp://*.example.com/*", nullptr),
        toPattern("<all_urls>", nullptr),
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_MATCH_BIDIRECTIONALLY
    ));
    g_assert_true(webkit_web_extension_match_pattern_matches_pattern(
        toPattern("*://*.en.wikipedia.org/*", nullptr),
        toPattern("*://*.wikipedia.org/*", nullptr),
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_MATCH_BIDIRECTIONALLY
    ));
    g_assert_true(webkit_web_extension_match_pattern_matches_pattern(
        toPattern("https://*.en.wikipedia.org/*", nullptr),
        toPattern("*://*.wikipedia.org/*", nullptr),
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_MATCH_BIDIRECTIONALLY
    ));
    g_assert_true(webkit_web_extension_match_pattern_matches_pattern(
        toPattern("*://*.en.wikipedia.org/*", nullptr),
        toPattern("https://*.wikipedia.org/*", nullptr),
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_MATCH_BIDIRECTIONALLY
    ));
    g_assert_true(webkit_web_extension_match_pattern_matches_pattern(
        toPattern("https://*/*", nullptr),
        toPattern("*://*.example.com/*", nullptr),
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_MATCH_BIDIRECTIONALLY
    ));
    g_assert_true(webkit_web_extension_match_pattern_matches_pattern(
        toPattern("http://*/*", nullptr),
        toPattern("*://*.example.com/*", nullptr),
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_MATCH_BIDIRECTIONALLY
    ));
    g_assert_true(webkit_web_extension_match_pattern_matches_pattern(
        toPattern("http://*/*", nullptr),
        toPattern("*://*/foo*", nullptr),
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_MATCH_BIDIRECTIONALLY
    ));
    g_assert_true(webkit_web_extension_match_pattern_matches_pattern(
        toPattern("*://*.example.com/*", nullptr),
        toPattern("https://*/*", nullptr),
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_MATCH_BIDIRECTIONALLY
    ));
    g_assert_true(webkit_web_extension_match_pattern_matches_pattern(
        toPattern("*://*.example.com/*", nullptr),
        toPattern("http://*/*", nullptr),
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_MATCH_BIDIRECTIONALLY
    ));

    // Matches with regex special characters in pattern.
    g_assert_true(webkit_web_extension_match_pattern_matches_pattern(
        toPattern("*://*/foo?bar*", nullptr),
        toPattern("*://*/foo?bar", nullptr),
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));
    g_assert_false(webkit_web_extension_match_pattern_matches_pattern(
        toPattern("*://*/foo?bar*", nullptr),
        toPattern("*://*/fobar", nullptr),
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));
    g_assert_true(webkit_web_extension_match_pattern_matches_pattern(
        toPattern("*://*/foo[ba]r*", nullptr),
        toPattern("*://*/foo[ba]r", nullptr),
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));
    g_assert_false(webkit_web_extension_match_pattern_matches_pattern(
        toPattern("*://*/foo[ba]r*", nullptr),
        toPattern("*://*/fooar", nullptr),
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));
    g_assert_true(webkit_web_extension_match_pattern_matches_pattern(
        toPattern("*://*/foo|bar*", nullptr),
        toPattern("*://*/foo|bar", nullptr),
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));
    g_assert_false(webkit_web_extension_match_pattern_matches_pattern(
        toPattern("*://*/foo|bar*", nullptr),
        toPattern("*://*/foo", nullptr),
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));

    // Matches a URL that is less permissive.
    g_assert_false(webkit_web_extension_match_pattern_matches_pattern(
        toPattern("https://www.apple.com/foo/bar/baz/*", nullptr),
        toPattern("*://www.apple.com/foo/*", nullptr),
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));
    g_assert_true(webkit_web_extension_match_pattern_matches_pattern(
        toPattern("https://www.apple.com/foo/bar/baz/*", nullptr),
        toPattern("*://www.apple.com/foo/*", nullptr),
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_MATCH_BIDIRECTIONALLY
    ));

    // Connivence methods
    g_assert_cmpstr(webkit_web_extension_match_pattern_get_string(webkit_web_extension_match_pattern_new_all_urls()), ==, "<all_urls>");
    g_assert_cmpstr(webkit_web_extension_match_pattern_get_string(webkit_web_extension_match_pattern_new_all_hosts_and_schemes()), ==, "*://*/*");
}

static void testMatchesURL(Test*, gconstpointer)
{
    // Matches any URL that uses the http scheme.
    g_assert_true(webkit_web_extension_match_pattern_matches_url(
        toPattern("http://*/*", nullptr),
        "http://www.example.com/",
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));
    g_assert_true(webkit_web_extension_match_pattern_matches_url(
        toPattern("http://*/*", nullptr),
        "http://example.com/foo/bar.html",
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));

    // Matches any URL that uses the http scheme, on any host, as long as the path starts with /foo.
    g_assert_true(webkit_web_extension_match_pattern_matches_url(
        toPattern("http://*/foo*", nullptr),
        "http://example.com/foo/bar.html",
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));
    g_assert_true(webkit_web_extension_match_pattern_matches_url(
        toPattern("http://*/*", nullptr),
        "http://www.example.com/foo",
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));

    // Matches any URL that uses the https scheme, is on a example.com host (such as www.example.com, bar.example.com,
    // or example.com), as long as the path starts with /foo and ends with bar.
    g_assert_true(webkit_web_extension_match_pattern_matches_url(
        toPattern("https://*.example.com/foo*bar", nullptr),
        "https://www.example.com/foo/baz/bar",
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));
    g_assert_true(webkit_web_extension_match_pattern_matches_url(
        toPattern("https://*.example.com/foo*bar", nullptr),
        "https://bar.example.com/foobar",
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));

    // Matches the specified URL.
    g_assert_true(webkit_web_extension_match_pattern_matches_url(
        toPattern("http://example.com/foo/bar.html", nullptr),
        "http://example.com/foo/bar.html",
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));

    // Matches any file whose path starts with /foo.
    g_assert_true(webkit_web_extension_match_pattern_matches_url(
        toPattern("file:///foo*", nullptr),
        "file:///foo/bar.html",
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));
    g_assert_true(webkit_web_extension_match_pattern_matches_url(
        toPattern("file:///foo*", nullptr),
        "file:///foo",
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));
    g_assert_true(webkit_web_extension_match_pattern_matches_url(
        toPattern("file://localhost/foo*", nullptr),
        "file://localhost/foo",
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));
    g_assert_true(webkit_web_extension_match_pattern_matches_url(
        toPattern("file://localhost/foo*", nullptr),
        "file:///foo",
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));
    g_assert_true(webkit_web_extension_match_pattern_matches_url(
        toPattern("file:///foo*", nullptr),
        "file://localhost/foo",
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));
    g_assert_true(webkit_web_extension_match_pattern_matches_url(
        toPattern("file://*/foo*", nullptr),
        "file:///foo/bar.html",
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));
    g_assert_true(webkit_web_extension_match_pattern_matches_url(
        toPattern("file://*/foo*", nullptr),
        "file://localhost/foo/bar.html",
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));
    g_assert_true(webkit_web_extension_match_pattern_matches_url(
        toPattern("file://*/foo*", nullptr),
        "file://test.local/foo/bar.html",
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));
    g_assert_true(webkit_web_extension_match_pattern_matches_url(
        toPattern("file://*/foo*", nullptr),
        "file://apple.com/foo/bar.html",
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));
    g_assert_true(webkit_web_extension_match_pattern_matches_url(
        toPattern("file://*.local/foo*", nullptr),
        "file://test.local/foo",
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));
    g_assert_false(webkit_web_extension_match_pattern_matches_url(
        toPattern("file://*.local/foo*", nullptr),
        "file://apple.com/foo",
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));

    // Matches ignoring scheme.
    g_assert_false(webkit_web_extension_match_pattern_matches_url(
        toPattern("http://*.example.com/*", nullptr),
        "https://example.com/",
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));
    g_assert_false(webkit_web_extension_match_pattern_matches_url(
        toPattern("https://*.example.com/*", nullptr),
        "http://example.com/",
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));
    g_assert_false(webkit_web_extension_match_pattern_matches_url(
        toPattern("http://*.example.com/*", nullptr),
        "ftp://example.com/",
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));
    g_assert_true(webkit_web_extension_match_pattern_matches_url(
        toPattern("http://*.example.com/*", nullptr),
        "https://example.com/",
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_IGNORE_SCHEMES
    ));
    g_assert_true(webkit_web_extension_match_pattern_matches_url(
        toPattern("https://*.example.com/*", nullptr),
        "http://example.com/",
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_IGNORE_SCHEMES
    ));
    g_assert_true(webkit_web_extension_match_pattern_matches_url(
        toPattern("http://*.example.com/*", nullptr),
        "ftp://example.com/",
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_IGNORE_SCHEMES
    ));

    // Matches ignoring path.
    g_assert_false(webkit_web_extension_match_pattern_matches_url(
        toPattern("https://*.example.com/foo*bar", nullptr),
        "https://www.example.com/baz",
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));
    g_assert_false(webkit_web_extension_match_pattern_matches_url(
        toPattern("*://*.example.com/foo*bar", nullptr),
        "http://www.example.com/test",
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));
    g_assert_false(webkit_web_extension_match_pattern_matches_url(
        toPattern("*://*.example.com/test*", nullptr),
        "https://www.example.com/bar",
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));
    g_assert_false(webkit_web_extension_match_pattern_matches_url(
        toPattern("*://*.example.com/*bar", nullptr),
        "http://example.com/baz",
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));
    g_assert_true(webkit_web_extension_match_pattern_matches_url(
        toPattern("https://*.example.com/foo*bar", nullptr),
        "https://www.example.com/baz",
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_IGNORE_PATHS
    ));
    g_assert_true(webkit_web_extension_match_pattern_matches_url(
        toPattern("*://*.example.com/foo*bar", nullptr),
        "http://www.example.com/test",
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_IGNORE_PATHS
    ));
    g_assert_true(webkit_web_extension_match_pattern_matches_url(
        toPattern("*://*.example.com/test*", nullptr),
        "https://www.example.com/bar",
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_IGNORE_PATHS
    ));
    g_assert_true(webkit_web_extension_match_pattern_matches_url(
        toPattern("*://*.example.com/*bar", nullptr),
        "http://example.com/baz",
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_IGNORE_PATHS
    ));

    // Matches host.
    g_assert_true(webkit_web_extension_match_pattern_matches_url(
        toPattern("https://*.example.com/*", nullptr),
        "https://example.com/",
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));
    g_assert_true(webkit_web_extension_match_pattern_matches_url(
        toPattern("https://*.example.com/*", nullptr),
        "https://www.example.com/",
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));
    g_assert_false(webkit_web_extension_match_pattern_matches_url(
        toPattern("https://*.example.com/*", nullptr),
        "https://the-example.com/",
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));
    g_assert_false(webkit_web_extension_match_pattern_matches_url(
        toPattern("https://*.example.com/*", nullptr),
        "https://www.the-example.com/",
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));

    // Matches any URL that uses the http scheme and is on the host 127.0.0.1.
    g_assert_true(webkit_web_extension_match_pattern_matches_url(
        toPattern("http://127.0.0.1/*", nullptr),
        "http://127.0.0.1/",
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));
    g_assert_true(webkit_web_extension_match_pattern_matches_url(
        toPattern("http://127.0.0.1/*", nullptr),
        "http://127.0.0.1/foo/bar.html",
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));
    g_assert_true(webkit_web_extension_match_pattern_matches_url(
        toPattern("http://127.0.0.1/*", nullptr),
        "http://127.0.0.1:8080/foo/bar.html",
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));

    // Matches any URL that uses the http scheme and is on the host [::1].
    g_assert_true(webkit_web_extension_match_pattern_matches_url(
        toPattern("http://[::1]/*", nullptr),
        "http://[::1]/",
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));
    g_assert_true(webkit_web_extension_match_pattern_matches_url(
        toPattern("http://[::1]/*", nullptr),
        "http://[::1]/foo/bar.html",
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));
    g_assert_true(webkit_web_extension_match_pattern_matches_url(
        toPattern("http://[::1]/*", nullptr),
        "http://[::1]:8080/foo/bar.html",
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));

    // Matches with username and password
    g_assert_true(webkit_web_extension_match_pattern_matches_url(
        toPattern("https://*.example.com/*", nullptr),
        "https://user@example.com/",
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));
    g_assert_true(webkit_web_extension_match_pattern_matches_url(
        toPattern("https://*.example.com/*", nullptr),
        "https://user:password@example.com/",
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));

    // Matches any URL that starts with http://foo.example.com or https://foo.example.com.
    g_assert_true(webkit_web_extension_match_pattern_matches_url(
        toPattern("*://foo.example.com/*", nullptr),
        "http://foo.example.com/foo/baz/bar",
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));
    g_assert_true(webkit_web_extension_match_pattern_matches_url(
        toPattern("*://foo.example.com/*", nullptr),
        "https://foo.example.com/foobar",
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));

    // Test missing hosts.
    g_assert_false(webkit_web_extension_match_pattern_matches_url(
        toPattern("*:///*", nullptr),
        "https://example.com/foobar",
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));
    g_assert_false(webkit_web_extension_match_pattern_matches_url(
        toPattern("https:///*", nullptr),
        "https://example.com/foobar",
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));
    g_assert_false(webkit_web_extension_match_pattern_matches_url(
        toPattern("ftp:///*", nullptr),
        "ftp://example.com/foobar",
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));
    g_assert_true(webkit_web_extension_match_pattern_matches_url(
        toPattern("file:///*", nullptr),
        "file:///foobar",
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));

    // Matches any URL that uses a permitted scheme. (See the beginning of this section for the list of permitted schemes.)
    g_assert_true(webkit_web_extension_match_pattern_matches_url(
        toPattern("<all_urls>", nullptr),
        "http://example.com/foo/bar.html",
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));
    g_assert_false(webkit_web_extension_match_pattern_matches_url(
        toPattern("<all_urls>", nullptr),
        "file:///bar/baz.html",
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));
    g_assert_false(webkit_web_extension_match_pattern_matches_url(
        toPattern("<all_urls>", nullptr),
        "favorites://",
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));
    g_assert_false(webkit_web_extension_match_pattern_matches_url(
        toPattern("<all_urls>", nullptr),
        "bookmarks://",
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));
    g_assert_false(webkit_web_extension_match_pattern_matches_url(
        toPattern("<all_urls>", nullptr),
        "history://",
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));

    // Matches with regex and percent encoded special characters in pattern.
    g_assert_true(webkit_web_extension_match_pattern_matches_url(
        toPattern("*://*/foo%3Fbar*", nullptr),
        "https://example.com/foo%3Fbar",
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));
    g_assert_false(webkit_web_extension_match_pattern_matches_url(
        toPattern("*://*/foo?bar*", nullptr),
        "https://example.com/foo%3Fbar",
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));
    g_assert_false(webkit_web_extension_match_pattern_matches_url(
        toPattern("*://*/foo?bar*", nullptr),
        "https://example.com/fobar",
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));
    g_assert_true(webkit_web_extension_match_pattern_matches_url(
        toPattern("*://*/foo%5Bba%5Dr*", nullptr),
        "https://example.com/foo%5Bba%5Dr",
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));
    g_assert_false(webkit_web_extension_match_pattern_matches_url(
        toPattern("*://*/foo[ba]r*", nullptr),
        "https://example.com/foo%5Bba%5Dr",
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));
    g_assert_false(webkit_web_extension_match_pattern_matches_url(
        toPattern("*://*/foo[ba]r*", nullptr),
        "https://example.com/fooar",
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));
    g_assert_true(webkit_web_extension_match_pattern_matches_url(
        toPattern("*://*/foo%7Cbar*", nullptr),
        "https://example.com/foo%7Cbar",
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));
    g_assert_false(webkit_web_extension_match_pattern_matches_url(
        toPattern("*://*/foo|bar*", nullptr),
        "https://example.com/foo%7Cbar",
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));
    g_assert_false(webkit_web_extension_match_pattern_matches_url(
        toPattern("*://*/foo|bar*", nullptr),
        "https://example.com/foo",
        WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE
    ));
}

static void testMatchesAllHosts(Test*, gconstpointer)
{
    g_assert_true(webkit_web_extension_match_pattern_get_matches_all_hosts(toPattern("<all_urls>", nullptr)));
    g_assert_true(webkit_web_extension_match_pattern_get_matches_all_hosts(toPattern("*://*/*", nullptr)));
    g_assert_true(webkit_web_extension_match_pattern_get_matches_all_hosts(toPattern("http://*/*", nullptr)));
    g_assert_true(webkit_web_extension_match_pattern_get_matches_all_hosts(toPattern("https://*/*", nullptr)));
    g_assert_true(webkit_web_extension_match_pattern_get_matches_all_hosts(toPattern("file://*/*", nullptr)));
    g_assert_false(webkit_web_extension_match_pattern_get_matches_all_hosts(toPattern("file:///*", nullptr)));
}

static void testMatchesAllURLs(Test*, gconstpointer)
{
    g_assert_true(webkit_web_extension_match_pattern_get_matches_all_urls(toPattern("<all_urls>", nullptr)));
    g_assert_false(webkit_web_extension_match_pattern_get_matches_all_urls(toPattern("*://*/*", nullptr)));
    g_assert_false(webkit_web_extension_match_pattern_get_matches_all_urls(toPattern("http://*/*", nullptr)));
    g_assert_false(webkit_web_extension_match_pattern_get_matches_all_urls(toPattern("https://*/*", nullptr)));
    g_assert_false(webkit_web_extension_match_pattern_get_matches_all_urls(toPattern("file://*/*", nullptr)));
    g_assert_false(webkit_web_extension_match_pattern_get_matches_all_urls(toPattern("file:///*", nullptr)));
}

static void testCustomURLScheme(Test*, gconstpointer)
{
    GUniqueOutPtr<GError> error;
    g_assert_null(toPattern("foo", "*", "/", &error.outPtr()));
    g_assert_cmpstr(error.get()->message, ==, "Scheme \"foo\" is invalid.");

    g_assert_null(toPattern("bar", "*", "/", &error.outPtr()));
    g_assert_cmpstr(error.get()->message, ==, "Scheme \"bar\" is invalid.");

    webkit_web_extension_match_pattern_register_custom_url_scheme("foo");

    g_assert_nonnull(toPattern("foo", "*", "/", &error.outPtr()));
    g_assert_no_error(error.get());

    g_assert_null(toPattern("bar", "*", "/", &error.outPtr()));
    g_assert_cmpstr(error.get()->message, ==, "Scheme \"bar\" is invalid.");

    webkit_web_extension_match_pattern_register_custom_url_scheme("bar");

    g_assert_nonnull(toPattern("foo", "*", "/", &error.outPtr()));
    g_assert_no_error(error.get());

    g_assert_nonnull(toPattern("bar", "*", "/", &error.outPtr()));
    g_assert_no_error(error.get());
}

void beforeAll()
{
    Test::add("WebKitWebExtensionMatchPattern", "pattern-validity", testPatternValidity);
    Test::add("WebKitWebExtensionMatchPattern", "matches-pattern", testMatchesPattern);
    Test::add("WebKitWebExtensionMatchPattern", "matches-url", testMatchesURL);
    Test::add("WebKitWebExtensionMatchPattern", "matches-all-hosts", testMatchesAllHosts);
    Test::add("WebKitWebExtensionMatchPattern", "matches-all-urls", testMatchesAllURLs);
    Test::add("WebKitWebExtensionMatchPattern", "custom-url-scheme", testCustomURLScheme);
}

void afterAll()
{
}

#endif // ENABLE(WK_WEB_EXTENSIONS)
