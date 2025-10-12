/*
 * Copyright (C) 2024 Igalia S.L.
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
#include "WebKitWebExtensionMatchPattern.h"

#include "WebExtensionMatchPattern.h"
#include "WebKitError.h"
#include "WebKitPrivate.h"

#include <wtf/URLParser.h>

using namespace WebKit;

/**
 * WebKitWebExtensionMatchPattern: (ref-func webkit_web_extension_match_pattern_ref) (unref-func webkit_web_extension_match_pattern_unref)
 * 
 * Represents a way to specify a group of URLs for use in WebExtensions.
 * 
 * All match patterns are specified as strings. Apart from the special `<all_urls>` pattern, match patterns
 * consist of three parts: scheme, host, and path.
 * 
 * Generally, match patterns are returned from a #WebKitWebExtension.
 *
 * Since: 2.48
 */
struct _WebKitWebExtensionMatchPattern {
#if ENABLE(WK_WEB_EXTENSIONS)
    explicit _WebKitWebExtensionMatchPattern(RefPtr<WebExtensionMatchPattern>&& matchPattern)
        : matchPattern(WTFMove(matchPattern))
    {
    }

    RefPtr<WebExtensionMatchPattern> matchPattern;
    CString string { matchPattern->string().utf8() };
    CString scheme { matchPattern->scheme().utf8() };
    CString host { matchPattern->host().utf8() };
    CString path { matchPattern->path().utf8() };
    bool matchesAllURLs { matchPattern->matchesAllURLs() };
    bool matchesAllHosts { matchPattern->matchesAllHosts() };
    int referenceCount { 1 };
#else
    _WebKitWebExtensionMatchPattern()
    {
    }
#endif
};

G_DEFINE_BOXED_TYPE(WebKitWebExtensionMatchPattern, webkit_web_extension_match_pattern, webkit_web_extension_match_pattern_ref, webkit_web_extension_match_pattern_unref)

#if ENABLE(WK_WEB_EXTENSIONS)

/**
 * webkit_web_extension_match_pattern_register_custom_url_scheme:
 * @urlScheme: The custom URL scheme to register
 *
 * Registers a custom URL scheme that can be used in match patterns.
 * 
 * This method should be used to register any custom URL schemes used by the app for the extension base URLs,
 * other than `webkit-extension`, or if extensions should have access to other supported URL schemes when using `<all_urls>`.
 *
 * Since: 2.52
 */
void webkit_web_extension_match_pattern_register_custom_url_scheme(const gchar* urlScheme)
{
    g_return_if_fail(WTF::URLParser::maybeCanonicalizeScheme(String::fromUTF8(urlScheme)));

    WebKit::WebExtensionMatchPattern::registerCustomURLScheme(String::fromUTF8(urlScheme));
}

/**
 * webkit_web_extension_match_pattern_register_custom_URL_scheme:
 * @urlScheme: The custom URL scheme to register
 *
 * Registers a custom URL scheme that can be used in match patterns.
 * 
 * This method should be used to register any custom URL schemes used by the app for the extension base URLs,
 * other than `webkit-extension`, or if extensions should have access to other supported URL schemes when using `<all_urls>`.
 *
 * Since: 2.48
 * 
 * Deprecated: 2.52: Use webkit_web_extension_match_pattern_register_custom_url_scheme() instead.
 */
void webkit_web_extension_match_pattern_register_custom_URL_scheme(const gchar* urlScheme)
{
    g_return_if_fail(WTF::URLParser::maybeCanonicalizeScheme(String::fromUTF8(urlScheme)));

    WebKit::WebExtensionMatchPattern::registerCustomURLScheme(String::fromUTF8(urlScheme));
}

WebKitWebExtensionMatchPattern* webkitWebExtensionMatchPatternCreate(const RefPtr<WebExtensionMatchPattern>& apiMatchPattern)
{
    if (!apiMatchPattern)
        return nullptr;

    ASSERT(API::Object::unwrap(static_cast<void*>(apiMatchPattern.get()))->type() == API::Object::Type::WebExtensionMatchPattern);
    WebKitWebExtensionMatchPattern* matchPattern = static_cast<WebKitWebExtensionMatchPattern*>(fastMalloc(sizeof(WebKitWebExtensionMatchPattern)));
    new (matchPattern) WebKitWebExtensionMatchPattern(static_pointer_cast<WebExtensionMatchPattern>(apiMatchPattern));
    return matchPattern;
}

WebKitWebExtensionMatchPattern* webkitWebExtensionMatchPatternCreate(Ref<WebExtensionMatchPattern>& matchPattern)
{
    RefPtr<WebExtensionMatchPattern> apiMatchPattern = adoptRef(matchPattern.get());

    return webkitWebExtensionMatchPatternCreate(apiMatchPattern);
}

/**
 * webkit_web_extension_match_pattern_ref:
 * @matchPattern: a #WebKitWebExtensionMatchPattern
 *
 * Atomically acquires a reference on the given @matchPattern.
 *
 * This function is MT-safe and may be called from any thread.
 *
 * Returns: The same @matchPattern with an additional reference.
 *
 * Since: 2.48
 */
WebKitWebExtensionMatchPattern* webkit_web_extension_match_pattern_ref(WebKitWebExtensionMatchPattern* matchPattern)
{
    g_return_val_if_fail(matchPattern, nullptr);
    g_atomic_int_inc(&matchPattern->referenceCount);
    return matchPattern;
}

/**
 * webkit_web_extension_match_pattern_unref:
 * @matchPattern: a #WebKitWebExtensionMatchPattern
 *
 * Atomically releases a reference on the given @matchPattern.
 *
 * If the reference was the last, the resources associated to the
 * @matchPattern are freed. This function is MT-safe and may be called from
 * any thread.
 *
 * Since: 2.48
 */
void webkit_web_extension_match_pattern_unref(WebKitWebExtensionMatchPattern* matchPattern)
{
    g_return_if_fail(matchPattern);
    if (g_atomic_int_dec_and_test(&matchPattern->referenceCount)) {
        matchPattern->~WebKitWebExtensionMatchPattern();
        fastFree(matchPattern);
    }
}

/**
 * webkit_web_extension_match_pattern_new_all_urls:
 *
 * Returns a new #WebKitWebExtensionMatchPattern for `<all_urls>`.
 *
 * Returns: (transfer full): a newly created #WebKitWebExtensionMatchPattern
 *
 * Since: 2.48
 */
WebKitWebExtensionMatchPattern* webkit_web_extension_match_pattern_new_all_urls()
{
    return webkitWebExtensionMatchPatternCreate(WebExtensionMatchPattern::allURLsMatchPattern());
}

/**
 * webkit_web_extension_match_pattern_new_all_hosts_and_schemes:
 *
 * Returns a new #WebKitWebExtensionMatchPattern that has `*` for scheme, host, and path.
 *
 * Returns: (transfer full): a newly created #WebKitWebExtensionMatchPattern
 *
 * Since: 2.48
 */
WebKitWebExtensionMatchPattern* webkit_web_extension_match_pattern_new_all_hosts_and_schemes()
{
    return webkitWebExtensionMatchPatternCreate(WebExtensionMatchPattern::allHostsAndSchemesMatchPattern());
}

/**
 * webkit_web_extension_match_pattern_new_with_string:
 * @string: A pattern string
 * @error: The return location for a recoverable error.
 *
 * Returns a new #WebKitWebExtensionMatchPattern for the specified @string.
 *
 * Returns: (transfer full) (nullable): a newly created #WebKitWebExtensionMatchPattern, or %NULL
 * if the pattern string is invalid.
 *
 * Since: 2.48
 */
WebKitWebExtensionMatchPattern* webkit_web_extension_match_pattern_new_with_string(const gchar* string, GError** error)
{
    if (!error)
        return webkitWebExtensionMatchPatternCreate(WebExtensionMatchPattern::getOrCreate(String::fromUTF8(string)));

    RefPtr<API::Error> internalError;
    RefPtr matchPattern = WebKit::WebExtensionMatchPattern::create(String::fromUTF8(string), internalError);

    if (error && internalError) {
        g_set_error(error, webkit_web_extension_match_pattern_error_quark(),
            toWebKitWebExtensionMatchPatternError(internalError->errorCode()), internalError->localizedDescription().utf8().data(), nullptr);
    }

    return webkitWebExtensionMatchPatternCreate(matchPattern);
}

/**
 * webkit_web_extension_match_pattern_new_with_scheme:
 * @scheme: A pattern URL scheme
 * @host: A pattern URL host
 * @path: A pattern URL path
 * @error: The return location for a recoverable error.
 *
 * Returns a new #WebKitWebExtensionMatchPattern for the specified @scheme, @host, and @path strings.
 *
 * Returns: (transfer full) (nullable): a newly created #WebKitWebExtensionMatchPattern, or %NULL
 * if any of the pattern strings are invalid.
 *
 * Since: 2.48 
 */
WebKitWebExtensionMatchPattern* webkit_web_extension_match_pattern_new_with_scheme(const gchar* scheme, const gchar* host, const gchar* path, GError** error)
{
    if (!error)
        return webkitWebExtensionMatchPatternCreate(WebExtensionMatchPattern::getOrCreate(String::fromUTF8(scheme), String::fromUTF8(host), String::fromUTF8(path)));

    RefPtr<API::Error> internalError;
    RefPtr matchPattern = WebKit::WebExtensionMatchPattern::create(String::fromUTF8(scheme), String::fromUTF8(host), String::fromUTF8(path), internalError);

    if (error && internalError) {
        g_set_error(error, webkit_web_extension_match_pattern_error_quark(),
            toWebKitWebExtensionMatchPatternError(internalError->errorCode()), internalError->localizedDescription().utf8().data(), nullptr);
    }

    return webkitWebExtensionMatchPatternCreate(matchPattern);
}

/**
 * webkit_web_extension_match_pattern_get_string:
 * @matchPattern: A #WebKitWebExtensionMatchPattern
 *
 * Gets the original pattern string.
 * 
 * Returns: (transfer none): The original pattern string.
 *
 * Since: 2.48
 */
const gchar* webkit_web_extension_match_pattern_get_string(WebKitWebExtensionMatchPattern* matchPattern)
{
    g_return_val_if_fail(matchPattern, nullptr);
    return matchPattern->string.data();
}

/**
 * webkit_web_extension_match_pattern_get_scheme:
 * @matchPattern: A #WebKitWebExtensionMatchPattern
 *
 * Gets the scheme part of the pattern string, unless `webkit_web_extension_match_pattern_get_matches_all_urls` is %TRUE.
 * 
 * Returns: (transfer none): The scheme string.
 *
 * Since: 2.48
 */
const gchar* webkit_web_extension_match_pattern_get_scheme(WebKitWebExtensionMatchPattern* matchPattern)
{
    g_return_val_if_fail(matchPattern, nullptr);
    return matchPattern->scheme.data();
}

/**
 * webkit_web_extension_match_pattern_get_host:
 * @matchPattern: A #WebKitWebExtensionMatchPattern
 *
 * Gets the host part of the pattern string, unless `webkit_web_extension_match_pattern_get_matches_all_urls` is %TRUE.
 * 
 * Returns: (transfer none): The host string.
 *
 * Since: 2.48
 */
const gchar* webkit_web_extension_match_pattern_get_host(WebKitWebExtensionMatchPattern* matchPattern)
{
    g_return_val_if_fail(matchPattern, nullptr);
    return matchPattern->host.data();
}

/**
 * webkit_web_extension_match_pattern_get_path:
 * @matchPattern: A #WebKitWebExtensionMatchPattern
 *
 * Gets the path part of the pattern string, unless [method@WebExtensionMatchPattern.get_matches_all_urls] is %TRUE.
 * 
 * Returns: (transfer none): The path string.
 *
 * Since: 2.48
 */
const gchar* webkit_web_extension_match_pattern_get_path(WebKitWebExtensionMatchPattern* matchPattern)
{
    g_return_val_if_fail(matchPattern, nullptr);
    return matchPattern->path.data();
}

/**
 * webkit_web_extension_match_pattern_get_matches_all_urls:
 * @matchPattern: A #WebKitWebExtensionMatchPattern
 *
 * Gets whether the match pattern matches all URLs, in other words, whether
 * the pattern is `<all_urls>`.
 * 
 * Returns: Whether this match pattern matches all URLs.
 *
 * Since: 2.48
 */
gboolean webkit_web_extension_match_pattern_get_matches_all_urls(WebKitWebExtensionMatchPattern* matchPattern)
{
    g_return_val_if_fail(matchPattern, FALSE);
    return matchPattern->matchesAllURLs;
}

/**
 * webkit_web_extension_match_pattern_get_matches_all_hosts:
 * @matchPattern: A #WebKitWebExtensionMatchPattern
 *
 * Gets whether the match pattern matches all host. This happens when
 * the pattern is `<all_urls>`, or if `*` is set as the host string.
 * 
 * Returns: Whether this match pattern matches all hosts.
 *
 * Since: 2.48
 */
gboolean webkit_web_extension_match_pattern_get_matches_all_hosts(WebKitWebExtensionMatchPattern* matchPattern)
{
    g_return_val_if_fail(matchPattern, FALSE);
    return matchPattern->matchesAllHosts;
}

static OptionSet<WebExtensionMatchPattern::Options> toImpl(WebKitWebExtensionMatchPatternOptions options)
{
    OptionSet<WebExtensionMatchPattern::Options> result;

    if (options & WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_IGNORE_SCHEMES)
        result.add(WebExtensionMatchPattern::Options::IgnoreSchemes);

    if (options & WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_IGNORE_PATHS)
        result.add(WebExtensionMatchPattern::Options::IgnorePaths);

    if (options & WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_MATCH_BIDIRECTIONALLY)
        result.add(WebExtensionMatchPattern::Options::MatchBidirectionally);

    return result;
}

/**
 * webkit_web_extension_match_pattern_matches_url:
 * @matchPattern: A #WebKitWebExtensionMatchPattern
 * @url: The URL to match against the pattern.
 * @options: The #WebKitWebExtensionMatchPatternOptions use while matching.
 *
 * Matches the @matchPattern against the specified URL with options.
 * 
 * Returns: Whether the pattern matches the specified URL.
 *
 * Since: 2.48
 */
gboolean webkit_web_extension_match_pattern_matches_url(WebKitWebExtensionMatchPattern* matchPattern, const gchar* url, WebKitWebExtensionMatchPatternOptions  options)
{
    g_return_val_if_fail(matchPattern, FALSE);
    g_return_val_if_fail(url, FALSE);

    if (options & WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_MATCH_BIDIRECTIONALLY)
        g_warning("Invalid parameter: WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_MATCH_BIDIRECTIONALLY is not valid when matching a URL");


    return matchPattern->matchPattern->matchesURL(URL(String::fromUTF8(url)), toImpl(options));
}

/**
 * webkit_web_extension_match_pattern_matches_pattern:
 * @matchPattern: A #WebKitWebExtensionMatchPattern to match against.
 * @pattern: The #WebKitWebExtensionMatchPattern to match with @matchPattern.
 * @options: The #WebKitWebExtensionMatchPatternOptions use while matching.
 *
 * Matches the @matchPattern against the specified @pattern with options.
 * 
 * Returns: Whether the pattern matches the specified @pattern.
 *
 * Since: 2.48
 */
gboolean webkit_web_extension_match_pattern_matches_pattern(WebKitWebExtensionMatchPattern* matchPattern, WebKitWebExtensionMatchPattern* pattern, WebKitWebExtensionMatchPatternOptions  options)
{
    g_return_val_if_fail(matchPattern, FALSE);
    g_return_val_if_fail(pattern, FALSE);

    return matchPattern->matchPattern->matchesPattern(*(pattern->matchPattern), toImpl(options));
}

#else // ENABLE(WK_WEB_EXTENSIONS)

WebKitWebExtensionMatchPattern* webkit_web_extension_match_pattern_ref(WebKitWebExtensionMatchPattern* matchPattern)
{
    return nullptr;
}

void webkit_web_extension_match_pattern_unref(WebKitWebExtensionMatchPattern* matchPattern)
{
    return;
}

WebKitWebExtensionMatchPattern* webkit_web_extension_match_pattern_new_all_urls()
{
    return nullptr;
}

WebKitWebExtensionMatchPattern* webkit_web_extension_match_pattern_new_all_hosts_and_schemes()
{
    return nullptr;
}

WebKitWebExtensionMatchPattern* webkit_web_extension_match_pattern_new_with_string(const gchar* string, GError** error)
{
    return nullptr;
}

WebKitWebExtensionMatchPattern* webkit_web_extension_match_pattern_new_with_scheme(const gchar* scheme, const gchar* host, const gchar* path, GError** error)
{
    return nullptr;
}

void webkit_web_extension_match_pattern_register_custom_url_scheme(const gchar* urlScheme)
{
    return;
}

const gchar* webkit_web_extension_match_pattern_get_string(WebKitWebExtensionMatchPattern* matchPattern)
{
    return "";
}

const gchar* webkit_web_extension_match_pattern_get_scheme(WebKitWebExtensionMatchPattern* matchPattern)
{
    return "";
}

const gchar* webkit_web_extension_match_pattern_get_host(WebKitWebExtensionMatchPattern* matchPattern)
{
    return "";
}

const gchar* webkit_web_extension_match_pattern_get_path(WebKitWebExtensionMatchPattern* matchPattern)
{
    return "";
}

gboolean webkit_web_extension_match_pattern_get_matches_all_urls(WebKitWebExtensionMatchPattern* matchPattern)
{
    return 0;
}

gboolean webkit_web_extension_match_pattern_get_matches_all_hosts(WebKitWebExtensionMatchPattern* matchPattern)
{
    return 0;
}

gboolean webkit_web_extension_match_pattern_matches_url(WebKitWebExtensionMatchPattern* matchPattern, const gchar* url, WebKitWebExtensionMatchPatternOptions  options)
{
    return 0;
}

gboolean webkit_web_extension_match_pattern_matches_pattern(WebKitWebExtensionMatchPattern* matchPattern, WebKitWebExtensionMatchPattern* pattern, WebKitWebExtensionMatchPatternOptions  options)
{
    return 0;
}

#endif // ENABLE(WK_WEB_EXTENSIONS)
