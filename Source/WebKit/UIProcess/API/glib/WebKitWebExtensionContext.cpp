/*
 * Copyright (C) 2026 Igalia S.L.
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
#include "WebKitWebExtensionContext.h"

#if ENABLE(2022_GLIB_API)

#include "WebExtensionContext.h"
#include "WebKitError.h"
#include "WebKitPrivate.h"
#include "WebKitWebExtensionMatchPatternPrivate.h"
#include "WebKitWebExtensionPrivate.h"
#include <WebCore/platform/LegacySchemeRegistry.h>
#include <glib/gi18n.h>
#include <wtf/URLParser.h>
#include <wtf/glib/GWeakPtr.h>
#include <wtf/glib/WTFGType.h>

#if ENABLE(WK_WEB_EXTENSIONS)
constexpr auto WEBKIT_CONTEXT_ERROR_DOMAIN = "WKWebExtensionContextErrorDomain"_s;
#endif

/**
 * WebKitWebExtensionContextPermission:
 *
 * Represents a Permission with its expiration dates. A permission that doesn't expire will have a distant future date.
 *
 * Since: 2.56
 */
struct _WebKitWebExtensionContextPermission {
#if ENABLE(WK_WEB_EXTENSIONS)
    _WebKitWebExtensionContextPermission(const String& permission, const WallTime& expiration)
        : permission(permission.utf8())
        , expiration(adoptGRef(g_date_time_new_from_unix_utc(expiration.secondsSinceEpoch().secondsAs<gint64>())))
    {
    }

    CString permission;
    GRefPtr<GDateTime> expiration;
    int referenceCount { 1 };
#else
    _WebKitWebExtensionContextPermission()
    {
    }
#endif
};

G_DEFINE_BOXED_TYPE(WebKitWebExtensionContextPermission, webkit_web_extension_context_permission, webkit_web_extension_context_permission_ref, webkit_web_extension_context_permission_unref)

#if ENABLE(WK_WEB_EXTENSIONS)

static WebKitWebExtensionContextPermission* webKitWebExtensionContextPermissionCreate(const String& permissionName, const WallTime& expiration)
{
    auto* permission = static_cast<WebKitWebExtensionContextPermission*>(fastMalloc(sizeof(WebKitWebExtensionContextPermission)));
    new (permission) WebKitWebExtensionContextPermission(permissionName, expiration);
    return permission;
}

/**
 * webkit_web_extension_context_permission_new:
 * @permission: The permission to represent
 * @expiration: (nullable): The expiration date for this permission
 *
 * Create a new [struct@WebExtensionContextPermission] for the provided permission and expiration date.
 *
 * If no expiration date is provided, the permission will never expire.
 *
 * Returns: the newly created [struct@WebExtensionContextPermission]
 *
 * Since: 2.56
 */
WebKitWebExtensionContextPermission* webkit_web_extension_context_permission_new(const char* permission, GDateTime* expiration)
{
    g_return_val_if_fail(permission, nullptr);
    WallTime expirationDate;

    if (expiration)
        expirationDate = WallTime::fromSecondsSinceEpoch(Seconds(g_date_time_to_unix(expiration)));
    else {
        GRefPtr<GDateTime> dt = adoptGRef(g_date_time_new_utc(9999, 12, 31, 23, 59, 00));
        expirationDate = WallTime::fromSecondsSinceEpoch(Seconds(g_date_time_to_unix(dt.get())));
    }

    return webKitWebExtensionContextPermissionCreate(String::fromUTF8(permission), expirationDate);
}

/**
 * webkit_web_extension_context_permission_ref:
 * @permission: a [struct@WebExtensionContextPermission]
 *
 * Atomically increments the reference count of @permission by one.
 *
 * This function is MT-safe and may be called from any thread.
 *
 * Returns: The passed [struct@WebExtensionContextPermission]
 *
 * Since: 2.56
 */
WebKitWebExtensionContextPermission* webkit_web_extension_context_permission_ref(WebKitWebExtensionContextPermission* permission)
{
    g_return_val_if_fail(permission, nullptr);

    g_atomic_int_inc(&permission->referenceCount);
    return permission;
}

/**
 * webkit_web_extension_context_permission_unref:
 * @permission: a [struct@WebExtensionContextPermission]
 *
 * Atomically decrements the reference count of @permission by one.
 *
 * If the reference count drops to 0, all memory allocated by
 * [struct@WebExtensionContextPermission] is released. This function is MT-safe and may be
 * called from any thread.
 *
 * Since: 2.56
 */
void webkit_web_extension_context_permission_unref(WebKitWebExtensionContextPermission* permission)
{
    g_return_if_fail(permission);

    if (g_atomic_int_dec_and_test(&permission->referenceCount)) {
        permission->~WebKitWebExtensionContextPermission();
        fastFree(permission);
    }
}

/**
 * webkit_web_extension_context_permission_get_permission_name:
 * @permission: a [struct@WebExtensionContextPermission]
 *
 * Get the permission name of @permission.
 *
 * Returns: The permission name
 *
 * Since: 2.56
 */
const char* webkit_web_extension_context_permission_get_permission_name(WebKitWebExtensionContextPermission* permission)
{
    g_return_val_if_fail(permission, nullptr);

    return permission->permission.data();
}

/**
 * webkit_web_extension_context_permission_get_expiration_date:
 * @permission: a [struct@WebExtensionContextPermission]
 *
 * Get the expiration date of @permission. If the permission does not expire, a distant future date will be returned instead.
 *
 * Returns: (transfer none): The expiration date of @permission
 *
 * Since: 2.56
 */
GDateTime* webkit_web_extension_context_permission_get_expiration_date(WebKitWebExtensionContextPermission* permission)
{
    g_return_val_if_fail(permission, nullptr);

    return permission->expiration.get();
}

#else // ENABLE(WK_WEB_EXTENSIONS)

WebKitWebExtensionContextPermission* webkit_web_extension_context_permission_new(const char* permission, GDateTime* expiration)
{
    return nullptr;
}

WebKitWebExtensionContextPermission* webkit_web_extension_context_permission_ref(WebKitWebExtensionContextPermission* permission)
{
    return nullptr;
}

void webkit_web_extension_context_permission_unref(WebKitWebExtensionContextPermission* permission)
{
    return;
}

const char* webkit_web_extension_context_permission_get_permission_name(WebKitWebExtensionContextPermission* permission)
{
    return "";
}

GDateTime* webkit_web_extension_context_permission_get_expiration_date(WebKitWebExtensionContextPermission* permission)
{
    return nullptr;
}

#endif // ENABLE(WK_WEB_EXTENSIONS)

/**
 * WebKitWebExtensionContextMatchPattern:
 * 
 * Represents a Match Pattern with its expiration dates. A match pattern that doesn't expire will have a distant future date.
 * 
 * Since: 2.56
 */
struct _WebKitWebExtensionContextMatchPattern {
#if ENABLE(WK_WEB_EXTENSIONS)
    _WebKitWebExtensionContextMatchPattern(Ref<WebKit::WebExtensionMatchPattern> pattern, const WallTime& expiration)
        : pattern(webkitWebExtensionMatchPatternCreate(pattern))
        , expiration(adoptGRef(g_date_time_new_from_unix_utc(expiration.secondsSinceEpoch().secondsAs<gint64>())))
    {
    }
    GRefPtr<WebKitWebExtensionMatchPattern> pattern;
    GRefPtr<GDateTime> expiration;
    int referenceCount { 1 };
#else
    _WebKitWebExtensionContextMatchPattern()
    {
    }
#endif
};

G_DEFINE_BOXED_TYPE(WebKitWebExtensionContextMatchPattern, webkit_web_extension_context_match_pattern, webkit_web_extension_context_match_pattern_ref, webkit_web_extension_context_match_pattern_unref)

#if ENABLE(WK_WEB_EXTENSIONS)

static WebKitWebExtensionContextMatchPattern* webKitWebExtensionContextMatchPatternCreate(Ref<WebKit::WebExtensionMatchPattern>& pattern, const WallTime& expiration)
{
    auto* matchPattern = static_cast<WebKitWebExtensionContextMatchPattern*>(fastMalloc(sizeof(WebKitWebExtensionContextMatchPattern)));
    new (matchPattern) WebKitWebExtensionContextMatchPattern(pattern, expiration);
    return matchPattern;
}

/**
 * webkit_web_extension_context_match_pattern_new:
 * @pattern: The [struct@WebExtensionMatchPattern] to represent
 * @expiration: (nullable): The expiration date for this permission
 *
 * Create a new [struct@WebExtensionContextMatchPattern] for the provided match pattern and expiration date.
 * 
 * If no expiration date is provided, the match pattern will never expire.
 * 
 * Returns: the newly created [struct@WebExtensionContextMatchPattern]
 *
 * Since: 2.56
 */
WebKitWebExtensionContextMatchPattern* webkit_web_extension_context_match_pattern_new(WebKitWebExtensionMatchPattern* pattern, GDateTime* expiration)
{
    g_return_val_if_fail(pattern, nullptr);

    auto expirationDate = expiration ? WallTime::fromSecondsSinceEpoch(Seconds(g_date_time_to_unix(expiration))) : WallTime::fromSecondsSinceEpoch(Seconds(g_date_time_to_unix(g_date_time_new_utc(9999, 12, 31, 23, 59, 00))));
    auto* matchPattern = static_cast<WebKitWebExtensionContextMatchPattern*>(fastMalloc(sizeof(WebKitWebExtensionContextMatchPattern)));
    new (matchPattern) WebKitWebExtensionContextMatchPattern(webkitWebExtensionMatchPatternToImpl(pattern).releaseNonNull(), expirationDate);
    return matchPattern;
}

/**
 * webkit_web_extension_context_match_pattern_ref:
 * @pattern: a [struct@WebExtensionContextMatchPattern]
 *
 * Atomically increments the reference count of @pattern by one.
 *
 * This function is MT-safe and may be called from any thread.
 *
 * Returns: The passed [struct@WebExtensionContextMatchPattern]
 *
 * Since: 2.56
 */
WebKitWebExtensionContextMatchPattern* webkit_web_extension_context_match_pattern_ref(WebKitWebExtensionContextMatchPattern* pattern)
{
    g_return_val_if_fail(pattern, nullptr);

    g_atomic_int_inc(&pattern->referenceCount);
    return pattern;
}

/**
 * webkit_web_extension_context_match_pattern_unref:
 * @pattern: a [struct@WebExtensionContextMatchPattern]
 *
 * Atomically decrements the reference count of @pattern by one.
 *
 * If the reference count drops to 0, all memory allocated by
 * [struct@WebExtensionContextMatchPattern] is released. This function is MT-safe and may be
 * called from any thread.
 *
 * Since: 2.56
 */
void webkit_web_extension_context_match_pattern_unref(WebKitWebExtensionContextMatchPattern* pattern)
{
    g_return_if_fail(pattern);

    if (g_atomic_int_dec_and_test(&pattern->referenceCount)) {
        pattern->~WebKitWebExtensionContextMatchPattern();
        fastFree(pattern);
    }
}

/**
 * webkit_web_extension_context_match_pattern_get_match_pattern:
 * @pattern: a [struct@WebExtensionMatchPattern]
 *
 * Get the match pattern of @pattern.
 *
 * Returns: (transfer none): The [struct@WebExtensionMatchPattern].
 *
 * Since: 2.56
 */
WebKitWebExtensionMatchPattern* webkit_web_extension_context_match_pattern_get_match_pattern(WebKitWebExtensionContextMatchPattern* pattern)
{
    g_return_val_if_fail(pattern, nullptr);

    return pattern->pattern.get();
}

/**
 * webkit_web_extension_context_match_pattern_get_expiration_date:
 * @pattern: a [struct@WebExtensionContextMatchPattern]
 *
 * Get the expiration date of @pattern. If the match pattern does not expire, a distant future date will be returned instead.
 *
 * Returns: (transfer none): The expiration date of @pattern
 *
 * Since: 2.56
 */
GDateTime* webkit_web_extension_context_match_pattern_get_expiration_date(WebKitWebExtensionContextMatchPattern* pattern)
{
    g_return_val_if_fail(pattern, nullptr);

    return pattern->expiration.get();
}

#else // ENABLE(WK_WEB_EXTENSIONS)

WebKitWebExtensionContextMatchPattern* webkit_web_extension_context_match_pattern_new(WebKitWebExtensionMatchPattern* pattern, GDateTime* expiration)
{
    return nullptr;
}

WebKitWebExtensionContextMatchPattern* webkit_web_extension_context_match_pattern_ref(WebKitWebExtensionContextMatchPattern* pattern)
{
    return nullptr;
}

void webkit_web_extension_context_match_pattern_unref(WebKitWebExtensionContextMatchPattern* pattern)
{
    return;
}

WebKitWebExtensionMatchPattern* webkit_web_extension_context_match_pattern_get_match_pattern(WebKitWebExtensionContextMatchPattern* pattern)
{
    return nullptr;
}

GDateTime* webkit_web_extension_context_match_pattern_get_expiration_date(WebKitWebExtensionContextMatchPattern* pattern)
{
    return nullptr;
}

#endif // ENABLE(WK_WEB_EXTENSIONS)

/**
 * WebKitWebExtensionContext:
 *
 * Represents the runtime environment for a [WebExtension](https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions).
 *
 * A [class@WebExtensionContext] object provides methods for managing the extension's permissions, allowing it to inject content,
 * run background logic, show popovers, and display other web-based UI to the user.
 *
 * Since: 2.56
 */

static void gInitableInterfaceInit(GInitableIface*);

struct _WebKitWebExtensionContextPrivate {
#if ENABLE(WK_WEB_EXTENSIONS)
    RefPtr<WebKit::WebExtensionContext> context;
    GWeakPtr<WebKitWebExtension> extension;
    UTF8CString baseURI;
    UTF8CString optionsPageURI;
    UTF8CString overrideNewTabPageURI;
#endif
};

WEBKIT_DEFINE_FINAL_TYPE_WITH_CODE(
    WebKitWebExtensionContext, webkit_web_extension_context, G_TYPE_OBJECT, GObject,
    G_IMPLEMENT_INTERFACE(G_TYPE_INITABLE, gInitableInterfaceInit))

static void webkitWebExtensionContextSetWebExtension(WebKitWebExtensionContext*, WebKitWebExtension*);

enum {
    PROP_0,
    PROP_WEB_EXTENSION,
    PROP_BASE_URI,
    PROP_OPTIONS_PAGE_URI,
    PROP_HAS_INJECTED_CONTENT,
    PROP_OVERRIDE_NEW_TAB_PAGE_URI,
    PROP_HAS_ACCESS_TO_ALL_URIS,
    PROP_HAS_ACCESS_TO_ALL_HOSTS,
    N_PROPERTIES,
};

static std::array<GParamSpec*, N_PROPERTIES> properties;

enum {
    GRANTED_PERMISSIONS_WERE_REMOVED,
    GRANTED_PERMISSION_MATCH_PATTERNS_WERE_REMOVED,
    DENIED_PERMISSIONS_WERE_REMOVED,
    DENIED_PERMISSION_MATCH_PATTERNS_WERE_REMOVED,
    PERMISSIONS_WERE_DENIED,
    PERMISSIONS_WERE_GRANTED,
    PERMISSION_MATCH_PATTERNS_WERE_DENIED,
    PERMISSION_MATCH_PATTERNS_WERE_GRANTED,

    LAST_SIGNAL
};

static std::array<unsigned, LAST_SIGNAL> signals;

static void webkitWebExtensionContextGetProperty(GObject* object, guint propId, GValue* value, GParamSpec* paramSpec)
{
    WebKitWebExtensionContext* context = WEBKIT_WEB_EXTENSION_CONTEXT(object);

    switch (propId) {
    case PROP_WEB_EXTENSION:
        g_value_set_object(value, webkit_web_extension_context_get_web_extension(context));
        break;
    case PROP_BASE_URI:
        g_value_set_string(value, webkit_web_extension_context_get_base_uri(context));
        break;
    case PROP_OPTIONS_PAGE_URI:
        g_value_set_string(value, webkit_web_extension_context_get_options_page_uri(context));
        break;
    case PROP_HAS_INJECTED_CONTENT:
        g_value_set_boolean(value, webkit_web_extension_context_get_has_injected_content(context));
        break;
    case PROP_OVERRIDE_NEW_TAB_PAGE_URI:
        g_value_set_string(value, webkit_web_extension_context_get_override_new_tab_page_uri(context));
        break;
    case PROP_HAS_ACCESS_TO_ALL_URIS:
        g_value_set_boolean(value, webkit_web_extension_context_get_has_access_to_all_uris(context));
        break;
    case PROP_HAS_ACCESS_TO_ALL_HOSTS:
        g_value_set_boolean(value, webkit_web_extension_context_get_has_access_to_all_hosts(context));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, paramSpec);
    }
}

static void webkitWebExtensionContextSetProperty(GObject* object, guint propId, const GValue* value, GParamSpec* paramSpec)
{
    WebKitWebExtensionContext* context = WEBKIT_WEB_EXTENSION_CONTEXT(object);

    switch (propId) {
    case PROP_WEB_EXTENSION:
        webkitWebExtensionContextSetWebExtension(context, WEBKIT_WEB_EXTENSION(g_value_get_object(value)));
        break;
    case PROP_BASE_URI:
        webkit_web_extension_context_set_base_uri(context, g_value_get_string(value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, paramSpec);
    }
}

static void webkit_web_extension_context_class_init(WebKitWebExtensionContextClass* klass)
{
    GObjectClass* objectClass = G_OBJECT_CLASS(klass);
    objectClass->get_property = webkitWebExtensionContextGetProperty;
    objectClass->set_property = webkitWebExtensionContextSetProperty;

    /**
     * WebKitWebExtensionContext:web-extension:
     * 
     * The [class@WebExtension] this context represents.
     * See [method@WebExtensionContext.get_web_extension] for more details.
     *
     * Since: 2.56
     */
    properties[PROP_WEB_EXTENSION] =
        g_param_spec_object(
            "web-extension",
            nullptr, nullptr,
            WEBKIT_TYPE_WEB_EXTENSION,
            static_cast<GParamFlags>(WEBKIT_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY)
        );

    /**
     * WebKitWebExtensionContext:base-uri:
     * 
     * The base URI of this context.
     * See [method@WebExtensionContext.get_base_uri] for more details.
     *
     * Since: 2.56
     */
    properties[PROP_BASE_URI] =
        g_param_spec_string(
            "base-uri",
            nullptr, nullptr,
            nullptr,
            WEBKIT_PARAM_READWRITE
        );

    /**
     * WebKitWebExtensionContext:options-page-uri:
     * 
     * The URI of the extension's options page.
     * See [method@WebExtensionContext.get_options_page_uri] for more details.
     *
     * Since: 2.56
     */
    properties[PROP_OPTIONS_PAGE_URI] =
        g_param_spec_string(
            "options-page-uri",
            nullptr, nullptr,
            nullptr,
            WEBKIT_PARAM_READABLE
        );

    /**
     * WebKitWebExtensionContext:has-injected-content:
     * 
     * Whether the extension has script or stylesheet content that can be injected into webpages.
     * See [method@WebExtensionContext.get_has_injected_content] for more details.
     *
     * Since: 2.56
     */
    properties[PROP_HAS_INJECTED_CONTENT] =
        g_param_spec_boolean(
            "has-injected-content",
            nullptr, nullptr,
            FALSE,
            WEBKIT_PARAM_READABLE
        );

    /**
     * WebKitWebExtensionContext:override-new-tab-page-uri:
     * 
     * The URI to use as an alternative to the default new tab page.
     * See [method@WebExtensionContext.get_override_new_tab_page_uri] for more details.
     *
     * Since: 2.56
     */
    properties[PROP_OVERRIDE_NEW_TAB_PAGE_URI] =
        g_param_spec_string(
            "override-new-tab-page-uri",
            nullptr, nullptr,
            nullptr,
            WEBKIT_PARAM_READABLE
        );

    /**
     * WebKitWebExtensionContext:has-access-to-all-uris:
     * 
     * Whether the currently granted permission match patterns set contains the `<all_urls>` pattern.
     * See [method@WebExtensionContext.get_has_access_to_all_uris] for more details.
     *
     * Since: 2.56
     */
    properties[PROP_HAS_ACCESS_TO_ALL_URIS] =
        g_param_spec_boolean(
            "has-access-to-all-uris",
            nullptr, nullptr,
            FALSE,
            WEBKIT_PARAM_READABLE
        );

    /**
     * WebKitWebExtensionContext:has-access-to-all-hosts:
     * 
     * whether the currently granted permission match patterns set contains the `<all_urls>` pattern or any `*` host patterns.
     * See [method@WebExtensionContext.get_has_access_to_all_hosts] for more details.
     *
     * Since: 2.56
     */
    properties[PROP_HAS_ACCESS_TO_ALL_HOSTS] =
        g_param_spec_boolean(
            "has-access-to-all-hosts",
            nullptr, nullptr,
            FALSE,
            WEBKIT_PARAM_READABLE
        );

    g_object_class_install_properties(objectClass, properties.size(), properties.data());

    /**
     * WebKitWebExtensionContext::granted-permissions-were-removed:
     * @context: the [class@WebExtensionContext]
     * @permissions: (array zero-terminated=1): an array of removed permissions
     *
     * This signal is emitted whenever previously granted permissions were
     * removed.
     *
     * Since: 2.56
     */
    signals[GRANTED_PERMISSIONS_WERE_REMOVED] =
        g_signal_new("granted-permissions-were-removed",
            G_TYPE_FROM_CLASS(objectClass),
            G_SIGNAL_RUN_LAST,
            0, 0, 0,
            nullptr,
            G_TYPE_NONE, 1,
            G_TYPE_STRV);

    /**
     * WebKitWebExtensionContext::granted-permission-match-patterns-were-removed:
     * @context: the [class@WebExtensionContext]
     * @match_patterns: (array zero-terminated=1) (element-type WebKitWebExtensionMatchPattern): an array of removed match patterns
     *
     * This signal is emitted whenever previously granted permission match patterns
     * were removed.
     *
     * Since: 2.56
     */
    signals[GRANTED_PERMISSION_MATCH_PATTERNS_WERE_REMOVED] =
        g_signal_new("granted-permission-match-patterns-were-removed",
            G_TYPE_FROM_CLASS(objectClass),
            G_SIGNAL_RUN_LAST,
            0, 0, 0,
            nullptr,
            G_TYPE_NONE, 1,
            G_TYPE_POINTER);

    /**
     * WebKitWebExtensionContext::denied-permissions-were-removed:
     * @context: the [class@WebExtensionContext]
     * @permissions: (array zero-terminated=1): an array of removed permissions
     *
     * This signal is emitted whenever previously denied permissions
     * were removed.
     *
     * Since: 2.56
     */
    signals[DENIED_PERMISSIONS_WERE_REMOVED] =
        g_signal_new("denied-permissions-were-removed",
            G_TYPE_FROM_CLASS(objectClass),
            G_SIGNAL_RUN_LAST,
            0, 0, 0,
            nullptr,
            G_TYPE_NONE, 1,
            G_TYPE_STRV);

    /**
     * WebKitWebExtensionContext::denied-permission-match-patterns-were-removed:
     * @context: the [class@WebExtensionContext]
     * @match_patterns: (array zero-terminated=1) (element-type WebKitWebExtensionMatchPattern): an array of removed match patterns
     *
     * This signal is emitted whenever previously denied permission match patterns
     * were removed.
     *
     * Since: 2.56
     */
    signals[DENIED_PERMISSION_MATCH_PATTERNS_WERE_REMOVED] =
        g_signal_new("denied-permission-match-patterns-were-removed",
            G_TYPE_FROM_CLASS(objectClass),
            G_SIGNAL_RUN_LAST,
            0, 0, 0,
            nullptr,
            G_TYPE_NONE, 1,
            G_TYPE_POINTER);

    /**
     * WebKitWebExtensionContext::permissions-were-denied:
     * @context: the [class@WebExtensionContext]
     * @permissions: (array zero-terminated=1): an array of denied permissions
     *
     * This signal is emitted whenever permissions were denied.
     *
     * Since: 2.56
     */
    signals[PERMISSIONS_WERE_DENIED] =
        g_signal_new("permissions-were-denied",
            G_TYPE_FROM_CLASS(objectClass),
            G_SIGNAL_RUN_LAST,
            0, 0, 0,
            nullptr,
            G_TYPE_NONE, 1,
            G_TYPE_STRV);

    /**
     * WebKitWebExtensionContext::permissions-were-granted:
     * @context: the [class@WebExtensionContext]
     * @permissions: (array zero-terminated=1): an array of granted permissions
     *
     * This signal is emitted whenever permissions were granted.
     *
     * Since: 2.56
     */
    signals[PERMISSIONS_WERE_GRANTED] =
        g_signal_new("permissions-were-granted",
            G_TYPE_FROM_CLASS(objectClass),
            G_SIGNAL_RUN_LAST,
            0, 0, 0,
            nullptr,
            G_TYPE_NONE, 1,
            G_TYPE_STRV);

    /**
     * WebKitWebExtensionContext::permission-match-patterns-were-denied:
     * @context: the [class@WebExtensionContext]
     * @match_patterns: (array zero-terminated=1) (element-type WebKitWebExtensionMatchPattern): an array of denied match patterns
     *
     * This signal is emitted whenever permission match patterns were denied.
     *
     * Since: 2.56
     */
    signals[PERMISSION_MATCH_PATTERNS_WERE_DENIED] =
        g_signal_new("permission-match-patterns-were-denied",
            G_TYPE_FROM_CLASS(objectClass),
            G_SIGNAL_RUN_LAST,
            0, 0, 0,
            nullptr,
            G_TYPE_NONE, 1,
            G_TYPE_POINTER);

    /**
     * WebKitWebExtensionContext::permission-match-patterns-were-granted:
     * @context: the [class@WebExtensionContext]
     * @match_patterns: (array zero-terminated=1) (element-type WebKitWebExtensionMatchPattern): an array of granted match patterns
     *
     * This signal is emitted whenever permission match patterns were granted.
     *
     * Since: 2.56
     */
    signals[PERMISSION_MATCH_PATTERNS_WERE_GRANTED] =
        g_signal_new("permission-match-patterns-were-granted",
            G_TYPE_FROM_CLASS(objectClass),
            G_SIGNAL_RUN_LAST,
            0, 0, 0,
            nullptr,
            G_TYPE_NONE, 1,
            G_TYPE_POINTER);
}

static gboolean webkitWebExtensionContextInitableInit(GInitable* initable, GCancellable* cancellable, GError** error)
{
#if ENABLE(WK_WEB_EXTENSIONS)
    WebKitWebExtensionContext* self = WEBKIT_WEB_EXTENSION_CONTEXT(initable);
    if (!self->priv->extension) {
        g_set_error_literal(error, webkit_web_extension_context_error_quark(),
            WEBKIT_WEB_EXTENSION_CONTEXT_ERROR_UNKNOWN, "No WebKitWebExtension was set");
        return FALSE;
    }

    auto webExtension = webkitWebExtensionToImpl(self->priv->extension.get());

    Ref context = WebKit::WebExtensionContext::create(self);

    // We only want to return errors that are in the Context error domain here. It is assumed that any errors that came up for the WebKitWebExtension would have been handled before adding to a context
    if (!context->errors().isEmpty()) {
        for (Ref internalError : context->errors()) {
            if (internalError->domain() == WEBKIT_CONTEXT_ERROR_DOMAIN) {
                g_set_error_literal(error, webkit_web_extension_context_error_quark(),
                    toWebKitWebExtensionContextError(internalError->errorCode()), internalError->localizedDescription().utf8().legacyCStringPointer());
                return FALSE;
            }
        }
    }

    self->priv->context = WTF::move(context);

    return TRUE;
#else
    return FALSE;
#endif
}

static void gInitableInterfaceInit(GInitableIface* iface)
{
    iface->init = webkitWebExtensionContextInitableInit;
}

#if ENABLE(WK_WEB_EXTENSIONS)

void webkitWebExtensionContextSetWebExtension(WebKitWebExtensionContext* context, WebKitWebExtension* extension)
{
    ASSERT(WEBKIT_IS_WEB_EXTENSION_CONTEXT(context));

    context->priv->extension.reset(extension);
}

RefPtr<WebKit::WebExtensionContext> webkitWebExtensionContextToImpl(WebKitWebExtensionContext* context)
{
    ASSERT(WEBKIT_IS_WEB_EXTENSION_CONTEXT(context));

    return context->priv->context;
}

/**
 * webkit_web_extension_context_new_for_extension:
 * @extension: (transfer none): a [class@WebExtension]
 * @error: return location for error or %NULL to ignore
 *
 * Create a new Context for the provided [class@WebExtension].
 * 
 * Returns: the newly created context
 * 
 * Since: 2.56
 */
WebKitWebExtensionContext* webkit_web_extension_context_new_for_extension(WebKitWebExtension* extension, GError** error)
{
    if (auto object = g_initable_new(WEBKIT_TYPE_WEB_EXTENSION_CONTEXT, nullptr, error, "web-extension", extension, nullptr))
        return WEBKIT_WEB_EXTENSION_CONTEXT (object);
    return nullptr;
}

/**
 * webkit_web_extension_context_get_web_extension:
 * @context: a [class@WebExtensionContext]
 *
 * Get the [class@WebExtension] this context represents.
 * 
 * Returns: (nullable) (transfer none): a [class@WebExtension], or %NULL if no web extension is available.
 * 
 * Since: 2.56
 */
WebKitWebExtension* webkit_web_extension_context_get_web_extension(WebKitWebExtensionContext* context)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION_CONTEXT(context), nullptr);

    return context->priv->extension.get();
}

/**
 * webkit_web_extension_context_get_base_uri:
 * @context: a [class@WebExtensionContext]
 *
 * Get the base URI this context uses for loading extension resources or injecting content into webpages.
 * The default value is a unique URI using the `webkit-extension` scheme.
 * 
 * Returns: the base URI
 * 
 * Since: 2.56
 */
const gchar* webkit_web_extension_context_get_base_uri(WebKitWebExtensionContext* context)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION_CONTEXT(context), nullptr);
    g_return_val_if_fail(context->priv->extension, nullptr);

    WebKitWebExtensionContextPrivate* priv = context->priv;
    if (!priv->baseURI.isNull())
        return priv->baseURI.legacyCStringPointer();

    auto baseURI = priv->context->baseURL();
    g_return_val_if_fail(!baseURI.isEmpty(), nullptr);

    priv->baseURI = baseURI.string().utf8();
    return priv->baseURI.legacyCStringPointer();
}

/**
 * webkit_web_extension_context_set_base_uri:
 * @context: a [class@WebExtensionContext]
 * @base_uri: The base URI to use for this context.
 *
 * Sets the base URI this context uses for loading extension resources or injecting content into webpages.
 * 
 * The base URI can be set to any URI, but only the scheme and host will be used. The scheme cannot be a scheme that is
 * already supported by [class@WebView] (e.g. http, https, etc.) Setting is only allowed when the context is not loaded.
 * 
 * Since: 2.56
 */
void webkit_web_extension_context_set_base_uri(WebKitWebExtensionContext* context, const gchar* baseURI)
{
    g_return_if_fail(WEBKIT_IS_WEB_EXTENSION_CONTEXT(context));
    g_return_if_fail(context->priv->extension);
    g_return_if_fail(baseURI);
    auto baseURL = URL { String::fromUTF8(baseURI) };
    g_return_if_fail(baseURL.isValid());
    g_return_if_fail(WTF::URLParser::maybeCanonicalizeScheme(baseURL.protocol()));
    g_return_if_fail(WebKit::WebExtensionMatchPattern::extensionSchemes().contains(baseURL.protocol().toStringWithoutCopying()));
    g_return_if_fail(WebCore::LegacySchemeRegistry::isBuiltinScheme(baseURL.protocol().toStringWithoutCopying()));
    g_return_if_fail(baseURL.path().isEmpty() || baseURL.path() == "/");

    WebKitWebExtensionContextPrivate* priv = context->priv;
    priv->context->setBaseURL(WTF::move(baseURL));
}

/**
 * webkit_web_extension_context_get_options_page_uri:
 * @context: a [class@WebExtensionContext]
 *
 * Get the URI of the extension's options page, if the extension has one.
 * 
 * Provides the URI for the dedicated options page, if provided by the extension; otherwise %NULL if no page is defined.
 * The app should provide access to this page through a user interface element. Navigation to the options page is only
 * possible after this extension has been loaded.
 * 
 * Returns: (nullable): the URI of the extension's options page, or %NULL if the extension does not have one.
 * 
 * Since: 2.56
 */
const gchar* webkit_web_extension_context_get_options_page_uri(WebKitWebExtensionContext* context)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION_CONTEXT(context), nullptr);
    g_return_val_if_fail(context->priv->extension, nullptr);

    WebKitWebExtensionContextPrivate* priv = context->priv;
    if (!priv->optionsPageURI.isNull())
        return priv->optionsPageURI.legacyCStringPointer();

    auto optionsPageURI = priv->context->optionsPageURL();
    if (optionsPageURI.isEmpty())
        return nullptr;

    priv->optionsPageURI = optionsPageURI.string().utf8();
    return priv->optionsPageURI.legacyCStringPointer();
}

/**
 * webkit_web_extension_context_get_has_injected_content:
 * @context: a [class@WebExtensionContext]
 *
 * Get whether the extension has script or stylesheet content that can be injected into webpages.
 * 
 * If this property is %TRUE, the extension has content that can be injected by matching against the extension's requested match patterns.
 * 
 * Returns: %TRUE if the extension contains content that can be injected into webpages.
 * 
 * Since: 2.56
 */
gboolean webkit_web_extension_context_get_has_injected_content(WebKitWebExtensionContext* context)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION_CONTEXT(context), FALSE);
    g_return_val_if_fail(context->priv->extension, FALSE);

    WebKitWebExtensionContextPrivate* priv = context->priv;
    return priv->context->hasInjectedContent();
}

/**
 * webkit_web_extension_context_has_injected_content_for_uri:
 * @context: a [class@WebExtensionContext]
 * @uri: The webpage URI to check
 *
 * Checks if the extension has script or stylesheet content that can be injected into the specified URL.
 * 
 * The extension context will still need to be loaded and have granted website permissions for its content to actually be injected.
 * 
 * Returns: %TRUE if the extension has content that can be injected by matching the URL against the extension's requested match patterns.
 * 
 * Since: 2.56
 */
gboolean webkit_web_extension_context_has_injected_content_for_uri(WebKitWebExtensionContext* context, const gchar* uri)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION_CONTEXT(context), FALSE);
    g_return_val_if_fail(context->priv->extension, FALSE);
    g_return_val_if_fail(uri, FALSE);

    WebKitWebExtensionContextPrivate* priv = context->priv;
    return priv->context->hasInjectedContentForURL(URL { String::fromUTF8(uri) });
}

/**
 * webkit_web_extension_context_get_override_new_tab_page_uri:
 * @context: a [class@WebExtensionContext]
 *
 * Get the URI to use as an alternative to the default new tab page, if the extension has one.
 * 
 * Provides the URI for a new tab page, if provided by the extension; otherwise %NULL if no page is defined.
 * The app should prompt the user for permission to use the extension's new tab page as the default.
 * Navigation to the override new tab page is only possible after this extension has been loaded.
 * 
 * Returns: (nullable): the URI to use as an alternative to the default new tab page, or %NULL if the extension
 * does not have one.
 * 
 * Since: 2.56
 */
const gchar* webkit_web_extension_context_get_override_new_tab_page_uri(WebKitWebExtensionContext* context)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION_CONTEXT(context), nullptr);
    g_return_val_if_fail(context->priv->extension, nullptr);

    WebKitWebExtensionContextPrivate* priv = context->priv;
    if (!priv->overrideNewTabPageURI.isNull())
        return priv->overrideNewTabPageURI.legacyCStringPointer();

    auto overrideNewTabPageURI = priv->context->overrideNewTabPageURL();
    if (overrideNewTabPageURI.isEmpty())
        return nullptr;

    priv->overrideNewTabPageURI = overrideNewTabPageURI.string().utf8();
    return priv->overrideNewTabPageURI.legacyCStringPointer();
}

/**
 * webkit_web_extension_context_get_has_access_to_all_uris:
 * @context: a [class@WebExtensionContext]
 *
 * Get whether the currently granted permission match patterns set contains the `<all_urls>` pattern.
 * 
 * This does not check for any `*` host patterns. In most cases you should use the broader
 * [method@WebExtensionContext.get_has_access_to_all_hosts] 
 * Returns: %TRUE if the `<all_urls>` pattern is present.
 * 
 * Since: 2.56
 */
gboolean webkit_web_extension_context_get_has_access_to_all_uris(WebKitWebExtensionContext* context)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION_CONTEXT(context), FALSE);
    g_return_val_if_fail(context->priv->extension, FALSE);

    WebKitWebExtensionContextPrivate* priv = context->priv;
    return priv->context->hasAccessToAllURLs();
}

/**
 * webkit_web_extension_context_get_has_access_to_all_hosts:
 * @context: a [class@WebExtensionContext]
 *
 * Get whether the currently granted permission match patterns set contains the `<all_urls>` pattern or any `*` host patterns.
 * 
 * Returns: %TRUE if the `<all_urls>` pattern or any `*` host patterns are present.
 * 
 * Since: 2.56
 */
gboolean webkit_web_extension_context_get_has_access_to_all_hosts(WebKitWebExtensionContext* context)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION_CONTEXT(context), FALSE);
    g_return_val_if_fail(context->priv->extension, FALSE);

    WebKitWebExtensionContextPrivate* priv = context->priv;
    return priv->context->hasAccessToAllHosts();
}

/**
 * webkit_web_extension_context_get_granted_permissions:
 * @context: a [class@WebExtensionContext]
 *
 * Get the currently granted permissions and their expiration dates.
 * 
 * Permissions that don't expire will have a distant future date. This will never include expired entries at time of access.
 * 
 * Returns: (nullable) (array zero-terminated=1) (transfer full): A %NULL-terminated list of permissions that have
 * been granted to the extension and their expiration dates, or %NULL otherwise.
 * 
 * Since: 2.56
 */
WebKitWebExtensionContextPermission** webkit_web_extension_context_get_granted_permissions(WebKitWebExtensionContext* context)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION_CONTEXT(context), nullptr);
    g_return_val_if_fail(context->priv->extension, nullptr);

    WebKitWebExtensionContextPrivate* priv = context->priv;
    auto permissions = priv->context->grantedPermissions();
    if (permissions.isEmpty())
        return nullptr;

    GPtrArray* grantedPermissions = g_ptr_array_new_full(permissions.size(), g_free);
    for (auto& permission : permissions) {
        auto [permissionName, expiration] = permission;
        auto* contextPermission = webKitWebExtensionContextPermissionCreate(permissionName, expiration);
        g_ptr_array_add(grantedPermissions, contextPermission);
    }
    g_ptr_array_add(grantedPermissions, nullptr);

    return reinterpret_cast<WebKitWebExtensionContextPermission**>(g_ptr_array_free(grantedPermissions, FALSE));
}

/**
 * webkit_web_extension_context_set_granted_permissions:
 * @context: a [class@WebExtensionContext]
 * @granted_permissions: (allow-none) (array zero-terminated=1) (element-type WebKitWebExtensionContextPermission) (transfer none): a %NULL-terminated list of permissions and their expiration dates
 *
 * Set the currently granted permissions and their expiration dates.
 * This will replace all existing granted permissions. Use this for saving and restoring permission status in bulk.
 * Permissions in this dictionary should be explicitly granted by the user before being added. Any permissions in this collection will not be
 * presented for approval again until they expire. This value should be saved and restored as needed by the app.
 * 
 * Since: 2.56
 */
void webkit_web_extension_context_set_granted_permissions(WebKitWebExtensionContext* context, WebKitWebExtensionContextPermission** grantedPermissions)
{
    g_return_if_fail(WEBKIT_IS_WEB_EXTENSION_CONTEXT(context));
    g_return_if_fail(context->priv->extension);

    WebKitWebExtensionContextPrivate* priv = context->priv;
    HashMap<String, WallTime> grantedPermissionsMap;

    WebKitWebExtensionContextPermission** permission = grantedPermissions;
    if (permission) {
        // We are using a null-terminated C array as input here, there is unfortunately not a great way to loop without pointer arithmetic.
        WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
        for (; *permission; permission++) {
            WallTime expirationDate;
            if (auto expiration = webkit_web_extension_context_permission_get_expiration_date(*permission))
                expirationDate = WallTime::fromSecondsSinceEpoch(Seconds(g_date_time_to_unix(expiration)));
            else
                expirationDate = WallTime::infinity();
            grantedPermissionsMap.add(String::fromUTF8(webkit_web_extension_context_permission_get_permission_name(*permission)), expirationDate);
        }
        WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
    }

    priv->context->setGrantedPermissions(WTF::move(grantedPermissionsMap));
}

/**
 * webkit_web_extension_context_get_granted_permission_match_patterns:
 * @context: a [class@WebExtensionContext]
 *
 * Get the currently granted permission match patterns and their expiration dates.
 * 
 * Permissions that don't expire will have a distant future date. This will never include expired entries at time of access.
 * 
 * Returns: (nullable) (array zero-terminated=1) (transfer full): A %NULL-terminated list of permission match patterns
 * that have been granted to the extension and their expiration dates, or %NULL otherwise.
 * 
 * Since: 2.56
 */
WebKitWebExtensionContextMatchPattern** webkit_web_extension_context_get_granted_permission_match_patterns(WebKitWebExtensionContext* context)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION_CONTEXT(context), nullptr);
    g_return_val_if_fail(context->priv->extension, nullptr);

    WebKitWebExtensionContextPrivate* priv = context->priv;
    auto matchPatterns = priv->context->grantedPermissionMatchPatterns();
    if (matchPatterns.isEmpty())
        return nullptr;

    GPtrArray* grantedMatchPatterns = g_ptr_array_new_full(matchPatterns.size(), g_free);
    for (auto& matchPattern : matchPatterns) {
        Ref internalPattern = matchPattern.key;
        auto expiration = matchPattern.value;
        auto pattern = webKitWebExtensionContextMatchPatternCreate(internalPattern, expiration);
        g_ptr_array_add(grantedMatchPatterns, pattern);
    }
    g_ptr_array_add(grantedMatchPatterns, nullptr);

    return reinterpret_cast<WebKitWebExtensionContextMatchPattern**>(g_ptr_array_free(grantedMatchPatterns, FALSE));
}

/**
 * webkit_web_extension_context_set_granted_permission_match_patterns:
 * @context: a [class@WebExtensionContext]
 * @granted_permission_match_patterns: (allow-none) (array zero-terminated=1) (element-type WebKitWebExtensionContextMatchPattern) (transfer none): a %NULL-terminated list of permission match patterns and their expiration dates
 *
 * Set the currently granted permission match patterns and their expiration dates.
 * This will replace all existing granted permissions. Use this for saving and restoring permission status in bulk.
 * Permissions in this dictionary should be explicitly granted by the user before being added. Any permissions in this collection will not be
 * presented for approval again until they expire. This value should be saved and restored as needed by the app.
 * 
 * Since: 2.56
 */
void webkit_web_extension_context_set_granted_permission_match_patterns(WebKitWebExtensionContext* context, WebKitWebExtensionContextMatchPattern** grantedPermissionMatchPatterns)
{
    g_return_if_fail(WEBKIT_IS_WEB_EXTENSION_CONTEXT(context));
    g_return_if_fail(context->priv->extension);

    WebKitWebExtensionContextPrivate* priv = context->priv;
    HashMap<Ref<WebKit::WebExtensionMatchPattern>, WallTime> grantedPermissionMatchPatternsMap;

    WebKitWebExtensionContextMatchPattern** pattern = grantedPermissionMatchPatterns;
    if (pattern) {
        WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
        for (; *pattern; pattern++) {
            WallTime expirationDate;
            if (auto expiration = webkit_web_extension_context_match_pattern_get_expiration_date(*pattern))
                expirationDate = WallTime::fromSecondsSinceEpoch(Seconds(g_date_time_to_unix(expiration)));
            else
                expirationDate = WallTime::infinity();
            grantedPermissionMatchPatternsMap.add(*webkitWebExtensionMatchPatternToImpl(webkit_web_extension_context_match_pattern_get_match_pattern(*pattern)), expirationDate);
        }
        WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
    }

    priv->context->setGrantedPermissionMatchPatterns(WTF::move(grantedPermissionMatchPatternsMap));
}

/**
 * webkit_web_extension_context_get_denied_permissions:
 * @context: a [class@WebExtensionContext]
 *
 * Get the currently denied permissions and their expiration dates.
 * 
 * Permissions that don't expire will have a distant future date. This will never include expired entries at time of access.
 * 
 * Returns: (nullable) (array zero-terminated=1) (transfer full): A %NULL-terminated list of permissions that have
 * been denied from the extension and their expiration dates, or %NULL otherwise.
 * 
 * Since: 2.56
 */
WebKitWebExtensionContextPermission** webkit_web_extension_context_get_denied_permissions(WebKitWebExtensionContext* context)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION_CONTEXT(context), nullptr);
    g_return_val_if_fail(context->priv->extension, nullptr);

    WebKitWebExtensionContextPrivate* priv = context->priv;
    auto permissions = priv->context->deniedPermissions();
    if (permissions.isEmpty())
        return nullptr;

    GPtrArray* deniedPermissions = g_ptr_array_new_full(permissions.size(), g_free);
    for (auto& permission : permissions) {
        auto [permissionName, expiration] = permission;
        auto* contextPermission = webKitWebExtensionContextPermissionCreate(permissionName, expiration);
        g_ptr_array_add(deniedPermissions, contextPermission);
    }
    g_ptr_array_add(deniedPermissions, nullptr);

    return reinterpret_cast<WebKitWebExtensionContextPermission**>(g_ptr_array_free(deniedPermissions, FALSE));
}

/**
 * webkit_web_extension_context_set_denied_permissions:
 * @context: a [class@WebExtensionContext]
 * @denied_permissions: (nullable) (array zero-terminated=1) (element-type WebKitWebExtensionContextPermission) (transfer none): a %NULL-terminated list of permissions and their expiration dates
 *
 * Set the currently denied permissions and their expiration dates.
 * This will replace all existing denied permissions. Use this for saving and restoring permission status in bulk.
 * Permissions in this dictionary should be explicitly denied by the user before being added. Any permissions in this collection will not be
 * presented for approval again until they expire. This value should be saved and restored as needed by the app.
 * 
 * Since: 2.56
 */
void webkit_web_extension_context_set_denied_permissions(WebKitWebExtensionContext* context, WebKitWebExtensionContextPermission** deniedPermissions)
{
    g_return_if_fail(WEBKIT_IS_WEB_EXTENSION_CONTEXT(context));
    g_return_if_fail(context->priv->extension);

    WebKitWebExtensionContextPrivate* priv = context->priv;
    HashMap<String, WallTime> deniedPermissionsMap;

    WebKitWebExtensionContextPermission** permission = deniedPermissions;
    if (permission) {
        WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
        for (; *permission; permission++) {
            WallTime expirationDate;
            if (auto expiration = webkit_web_extension_context_permission_get_expiration_date(*permission))
                expirationDate = WallTime::fromSecondsSinceEpoch(Seconds(g_date_time_to_unix(expiration)));
            else
                expirationDate = WallTime::infinity();
            deniedPermissionsMap.add(String::fromUTF8(webkit_web_extension_context_permission_get_permission_name(*permission)), expirationDate);
        }
        WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
    }

    priv->context->setDeniedPermissions(WTF::move(deniedPermissionsMap));
}

/**
 * webkit_web_extension_context_get_denied_permission_match_patterns:
 * @context: a [class@WebExtensionContext]
 *
 * Get the currently denied permission match patterns and their expiration dates.
 * 
 * Permissions that don't expire will have a distant future date. This will never include expired entries at time of access.
 * 
 * Returns: (nullable) (array zero-terminated=1) (transfer full): A %NULL-terminated list of permission match patterns
 * that have been denied from the extension and their expiration dates, or %NULL otherwise.
 * 
 * Since: 2.56
 */
WebKitWebExtensionContextMatchPattern** webkit_web_extension_context_get_denied_permission_match_patterns(WebKitWebExtensionContext* context)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION_CONTEXT(context), nullptr);
    g_return_val_if_fail(context->priv->extension, nullptr);

    WebKitWebExtensionContextPrivate* priv = context->priv;
    auto matchPatterns = priv->context->deniedPermissionMatchPatterns();
    if (matchPatterns.isEmpty())
        return nullptr;
    GPtrArray* deniedMatchPatterns = g_ptr_array_new_full(matchPatterns.size(), g_free);
    for (auto& matchPattern : matchPatterns) {
        Ref internalPattern = matchPattern.key;
        auto expiration = matchPattern.value;
        auto pattern = webKitWebExtensionContextMatchPatternCreate(internalPattern, expiration);
        g_ptr_array_add(deniedMatchPatterns, pattern);
    }
    g_ptr_array_add(deniedMatchPatterns, nullptr);
    return reinterpret_cast<WebKitWebExtensionContextMatchPattern**>(g_ptr_array_free(deniedMatchPatterns, FALSE));
}

/**
 * webkit_web_extension_context_set_denied_permission_match_patterns:
 * @context: a [class@WebExtensionContext]
 * @denied_permission_match_patterns: (allow-none) (array zero-terminated=1) (element-type WebKitWebExtensionContextMatchPattern) (transfer none): a %NULL-terminated list of permission match patterns and their expiration dates
 *
 * Set the currently denied permission match patterns and their expiration dates.
 * This will replace all existing denied permissions. Use this for saving and restoring permission status in bulk.
 * Permissions in this dictionary should be explicitly denied by the user before being added. Any permissions in this collection will not be
 * presented for approval again until they expire. This value should be saved and restored as needed by the app.
 * 
 * Since: 2.56
 */
void webkit_web_extension_context_set_denied_permission_match_patterns(WebKitWebExtensionContext* context, WebKitWebExtensionContextMatchPattern** deniedPermissionMatchPatterns)
{
    g_return_if_fail(WEBKIT_IS_WEB_EXTENSION_CONTEXT(context));
    g_return_if_fail(context->priv->extension);
    WebKitWebExtensionContextPrivate* priv = context->priv;
    HashMap<Ref<WebKit::WebExtensionMatchPattern>, WallTime> deniedPermissionMatchPatternsMap;

    WebKitWebExtensionContextMatchPattern** pattern = deniedPermissionMatchPatterns;
    if (pattern) {
        WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
        for (; *pattern; pattern++) {
            WallTime expirationDate;
            if (auto expiration = webkit_web_extension_context_match_pattern_get_expiration_date(*pattern))
                expirationDate = WallTime::fromSecondsSinceEpoch(Seconds(g_date_time_to_unix(expiration)));
            else
                expirationDate = WallTime::infinity();
            deniedPermissionMatchPatternsMap.add(*webkitWebExtensionMatchPatternToImpl(webkit_web_extension_context_match_pattern_get_match_pattern(*pattern)), expirationDate);
        }
        WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
    }
    priv->context->setDeniedPermissionMatchPatterns(WTF::move(deniedPermissionMatchPatternsMap));
}

/**
 * webkit_web_extension_context_has_permission:
 * @context: a [class@WebExtensionContext]
 * @permission: The permission for which to return the status
 *
 * Checks the specified permission against the currently granted permissions.
 * 
 * Returns: %TRUE if the extension has been granted the specified permission.
 * 
 * Since: 2.56
 */
gboolean webkit_web_extension_context_has_permission(WebKitWebExtensionContext* context, const gchar* permission)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION_CONTEXT(context), FALSE);
    g_return_val_if_fail(context->priv->extension, FALSE);
    g_return_val_if_fail(permission, FALSE);
    WebKitWebExtensionContextPrivate* priv = context->priv;
    return priv->context->hasPermission(String::fromUTF8(permission), nullptr);
}

/**
 * webkit_web_extension_context_has_access_to_uri:
 * @context: a [class@WebExtensionContext]
 * @uri: The URI for which to return the status
 *
 * Checks the specified URI against the currently granted permission match patterns.
 * 
 * Returns: %TRUE if the URI is accessible by the extension.
 * 
 * Since: 2.56
 */
gboolean webkit_web_extension_context_has_access_to_uri(WebKitWebExtensionContext* context, const gchar* uri)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION_CONTEXT(context), FALSE);
    g_return_val_if_fail(context->priv->extension, FALSE);
    g_return_val_if_fail(uri, FALSE);

    WebKitWebExtensionContextPrivate* priv = context->priv;
    return priv->context->hasPermission(URL { String::fromUTF8(uri) });
}

static inline WebKitWebExtensionContextPermissionStatus toAPI(WebKit::WebExtensionContext::PermissionState status)
{
    switch (status) {
    case WebKit::WebExtensionContext::PermissionState::DeniedExplicitly:
        return WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_DENIED_EXPLICITLY;
    case WebKit::WebExtensionContext::PermissionState::DeniedImplicitly:
        return WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_DENIED_IMPLICITLY;
    case WebKit::WebExtensionContext::PermissionState::RequestedImplicitly:
        return WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_REQUESTED_IMPLICITLY;
    case WebKit::WebExtensionContext::PermissionState::Unknown:
        return WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_UNKNOWN;
    case WebKit::WebExtensionContext::PermissionState::RequestedExplicitly:
        return WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_REQUESTED_EXPLICITLY;
    case WebKit::WebExtensionContext::PermissionState::GrantedImplicitly:
        return WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_GRANTED_IMPLICITLY;
    case WebKit::WebExtensionContext::PermissionState::GrantedExplicitly:
        return WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_GRANTED_EXPLICITLY;
    }
    return WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_UNKNOWN;
}

static inline WebKit::WebExtensionContext::PermissionState toImpl(WebKitWebExtensionContextPermissionStatus status)
{
    switch (status) {
    case WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_DENIED_EXPLICITLY:
        return WebKit::WebExtensionContext::PermissionState::DeniedExplicitly;
    case WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_DENIED_IMPLICITLY:
        return WebKit::WebExtensionContext::PermissionState::DeniedImplicitly;
    case WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_REQUESTED_IMPLICITLY:
        return WebKit::WebExtensionContext::PermissionState::RequestedImplicitly;
    case WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_UNKNOWN:
        return WebKit::WebExtensionContext::PermissionState::Unknown;
    case WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_REQUESTED_EXPLICITLY:
        return WebKit::WebExtensionContext::PermissionState::RequestedExplicitly;
    case WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_GRANTED_IMPLICITLY:
        return WebKit::WebExtensionContext::PermissionState::GrantedImplicitly;
    case WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_GRANTED_EXPLICITLY:
        return WebKit::WebExtensionContext::PermissionState::GrantedExplicitly;
    }
    return WebKit::WebExtensionContext::PermissionState::Unknown;
}

/**
 * webkit_web_extension_context_get_permission_status:
 * @context: a [class@WebExtensionContext]
 * @permission: The permission for which to return the status.
 *
 * Checks the specified permission against the currently denied, granted, and requested permissions.
 * 
 * Returns: the status of the requested permission
 * 
 * Since: 2.56
 */
WebKitWebExtensionContextPermissionStatus webkit_web_extension_context_get_permission_status(WebKitWebExtensionContext* context, const gchar* permission)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION_CONTEXT(context), WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_UNKNOWN);
    g_return_val_if_fail(context->priv->extension, WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_UNKNOWN);
    g_return_val_if_fail(permission, WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_UNKNOWN);

    WebKitWebExtensionContextPrivate* priv = context->priv;
    return toAPI(priv->context->permissionState(String::fromUTF8(permission), nullptr));
}

/**
 * webkit_web_extension_context_set_permission_status:
 * @context: a [class@WebExtensionContext]
 * @permission: The permission for which to set the status
 * @status: The new permission status to set for the given permission.
 * @expiration_date: (nullable): The expiration date for the new permission status, or %NULL for distant future.
 *
 * Sets the status of a permission. Passing a %NULL expiration date will be treated as a distant future date.
 * 
 * This method will update [method@WebExtensionContext.get_granted_permissions] and [method@WebExtensionContext.get_denied_permissions]. Use this method for changing a single permission's status.
 * Only [enum@WebKit.WebExtensionContextPermissionStatus.DENIED_EXPLICITLY], [enum@WebKit.WebExtensionContextPermissionStatus.UNKNOWN],
 * and [enum@WebKit.WebExtensionContextPermissionStatus.GRANTED_EXPLICITLY] states are allowed to be set using this method.
 * 
 * Since: 2.56
 */
void webkit_web_extension_context_set_permission_status(WebKitWebExtensionContext* context, const gchar* permission, WebKitWebExtensionContextPermissionStatus status, GDateTime* expirationDate)
{
    g_return_if_fail(WEBKIT_IS_WEB_EXTENSION_CONTEXT(context));
    g_return_if_fail(context->priv->extension);
    g_return_if_fail(permission);
    g_return_if_fail(status == WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_DENIED_EXPLICITLY || status == WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_UNKNOWN || status == WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_GRANTED_EXPLICITLY);

    WebKitWebExtensionContextPrivate* priv = context->priv;
    if (expirationDate)
        priv->context->setPermissionState(toImpl(status), String::fromUTF8(permission), WallTime::fromSecondsSinceEpoch(Seconds(g_date_time_to_unix(expirationDate))));
    else
        priv->context->setPermissionState(toImpl(status), String::fromUTF8(permission));
}

/**
 * webkit_web_extension_context_get_permission_status_for_uri:
 * @context: a [class@WebExtensionContext]
 * @uri: The URI for which to return the status.
 *
 * Checks the specified URL against the currently denied, granted, and requested permission match patterns.
 * 
 * Returns: the permission status of the requested URI
 * 
 * Since: 2.56
 */
WebKitWebExtensionContextPermissionStatus webkit_web_extension_context_get_permission_status_for_uri(WebKitWebExtensionContext* context, const gchar* uri)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION_CONTEXT(context), WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_UNKNOWN);
    g_return_val_if_fail(context->priv->extension, WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_UNKNOWN);
    g_return_val_if_fail(uri, WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_UNKNOWN);

    WebKitWebExtensionContextPrivate* priv = context->priv;
    return toAPI(priv->context->permissionState(URL { String::fromUTF8(uri) }, nullptr));
}

/**
 * webkit_web_extension_context_set_permission_status_for_uri:
 * @context: a [class@WebExtensionContext]
 * @uri: The URI for which to set the status
 * @status: The new permission status to set for the given permission.
 * @expiration_date: (nullable): The expiration date for the new permission status, or %NULL for distant future.
 *
 * Sets the permission status of a URL. Passing a %NULL expiration date will be treated as a distant future date.
 * 
 * The URL is converted into a match pattern and will update [method@WebExtensionContext.get_granted_permission_match_patterns] and [method@WebExtensionContext.get_denied_permission_match_patterns]..
 * Use this method for changing a single URL's status.
 * Only [enum@WebKit.WebExtensionContextPermissionStatus.DENIED_EXPLICITLY], [enum@WebKit.WebExtensionContextPermissionStatus.UNKNOWN],
 * and [enum@WebKit.WebExtensionContextPermissionStatus.GRANTED_EXPLICITLY] states are allowed to be set using this method.
 * 
 * Since: 2.56
 */
void webkit_web_extension_context_set_permission_status_for_uri(WebKitWebExtensionContext* context, const gchar* uri, WebKitWebExtensionContextPermissionStatus status, GDateTime* expirationDate)
{
    g_return_if_fail(WEBKIT_IS_WEB_EXTENSION_CONTEXT(context));
    g_return_if_fail(context->priv->extension);
    g_return_if_fail(uri);
    g_return_if_fail(status == WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_DENIED_EXPLICITLY || status == WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_UNKNOWN || status == WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_GRANTED_EXPLICITLY);

    WebKitWebExtensionContextPrivate* priv = context->priv;
    if (expirationDate)
        priv->context->setPermissionState(toImpl(status), URL { String::fromUTF8(uri) }, WallTime::fromSecondsSinceEpoch(Seconds(g_date_time_to_unix(expirationDate))));
    else
        priv->context->setPermissionState(toImpl(status), URL { String::fromUTF8(uri) });
}

/**
 * webkit_web_extension_context_get_permission_status_for_match_pattern:
 * @context: a [class@WebExtensionContext]
 * @pattern: The pattern for which to return the status.
 *
 * Checks the specified match pattern against the currently denied, granted, and requested permission match patterns.
 * 
 * Returns: the permission status of the requested match pattern
 * 
 * Since: 2.56
 */
WebKitWebExtensionContextPermissionStatus webkit_web_extension_context_get_permission_status_for_match_pattern(WebKitWebExtensionContext* context, WebKitWebExtensionMatchPattern* pattern)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION_CONTEXT(context), WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_UNKNOWN);
    g_return_val_if_fail(context->priv->extension, WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_UNKNOWN);
    g_return_val_if_fail(pattern, WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_UNKNOWN);

    WebKitWebExtensionContextPrivate* priv = context->priv;
    return toAPI(priv->context->permissionState(*webkitWebExtensionMatchPatternToImpl(pattern), nullptr));
}

/**
 * webkit_web_extension_context_set_permission_status_for_match_pattern:
 * @context: a [class@WebExtensionContext]
 * @pattern: The match pattern for which to set the status
 * @status: The new permission status to set for the given permission.
 * @expiration_date: (nullable): The expiration date for the new permission status, or %NULL for distant future.
 *
 * Sets the status of a match pattern. Passing a %NULL expiration date will be treated as a distant future date.
 * 
 * This method will update [method@WebExtensionContext.get_granted_permission_match_patterns] and [method@WebExtensionContext.get_denied_permission_match_patterns]..
 * Use this method for changing a single match pattern's status.
 * Only [enum@WebKit.WebExtensionContextPermissionStatus.DENIED_EXPLICITLY], [enum@WebKit.WebExtensionContextPermissionStatus.UNKNOWN],
 * and [enum@WebKit.WebExtensionContextPermissionStatus.GRANTED_EXPLICITLY] states are allowed to be set using this method.
 * 
 * Since: 2.56
 */
void webkit_web_extension_context_set_permission_status_for_match_pattern(WebKitWebExtensionContext* context, WebKitWebExtensionMatchPattern* pattern, WebKitWebExtensionContextPermissionStatus status, GDateTime* expirationDate)
{
    g_return_if_fail(WEBKIT_IS_WEB_EXTENSION_CONTEXT(context));
    g_return_if_fail(context->priv->extension);
    g_return_if_fail(pattern);
    g_return_if_fail(status == WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_DENIED_EXPLICITLY || status == WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_UNKNOWN || status == WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_GRANTED_EXPLICITLY);

    WebKitWebExtensionContextPrivate* priv = context->priv;
    if (expirationDate)
        priv->context->setPermissionState(toImpl(status), *webkitWebExtensionMatchPatternToImpl(pattern), WallTime::fromSecondsSinceEpoch(Seconds(g_date_time_to_unix(expirationDate))));
    else
        priv->context->setPermissionState(toImpl(status), *webkitWebExtensionMatchPatternToImpl(pattern));
}

/**
 * webkit_web_extension_context_load_background_content:
 * @context: a [class@WebExtensionContext]
 * @cancellable: (allow-none): a #GCancellable or %NULL to ignore
 * @callback: (scope async): a #GAsyncReadyCallback to call when the request is satisfied
 * @user_data: the data to pass to callback function
 *
 * Asynchronously loads the background content if needed for the extension.
 * 
 * This method forces the loading of the background content for the extension that will otherwise be loaded on-demand during specific events.
 * It is useful when the app requires the background content to be loaded for other reasons.
 * 
 * When the operation is finished, or if the background content is already loaded, @callback will be called. 
 * 
 * You can then call [method@WebExtensionContext.load_background_content_finish] to get the result of the operation.
 * 
 * Since: 2.56
 */
void webkit_web_extension_context_load_background_content(WebKitWebExtensionContext* context, GCancellable* cancellable, GAsyncReadyCallback callback, gpointer userData)
{
    g_return_if_fail(WEBKIT_IS_WEB_EXTENSION_CONTEXT(context));
    g_return_if_fail(context->priv->extension);

    GRefPtr<GTask> task = adoptGRef(g_task_new(context, cancellable, callback, userData));
    if (!context->priv->context->isLoaded()) {
        auto errorStr = _("Extension context is not loaded");
        g_task_return_new_error(task.get(), webkit_web_extension_context_error_quark(),
            WEBKIT_WEB_EXTENSION_CONTEXT_ERROR_NOT_LOADED, "%s", errorStr);
        return;
    }

    context->priv->context->loadBackgroundContent([task = WTF::move(task)](RefPtr<API::Error> error) {
        if (error) {
            g_task_return_new_error(task.get(), webkit_web_extension_context_error_quark(),
                toWebKitWebExtensionContextError(error->errorCode()), "%s", error->localizedDescription().utf8().legacyCStringPointer());
        } else
            g_task_return_boolean(task.get(), TRUE);
    });
}

/**
 * webkit_web_extension_context_load_background_content_finish:
 * @context: a [class@WebExtensionContext]
 * @result: a #GAsyncResult
 * @error: return location for error or %NULL to ignore
 *
 * Finish an asynchronous operation started with [method@WebExtensionContext.load_background_content].
 * 
 * An error will occur if the extension does not have any background content to load or loading fails.
 * 
 * Returns: %TRUE if the background content was loaded or %FALSE in case of error.
 * 
 * Since: 2.56
 */
gboolean webkit_web_extension_context_load_background_content_finish(WebKitWebExtensionContext* context, GAsyncResult* result, GError** error)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION_CONTEXT(context), FALSE);
    g_return_val_if_fail(context->priv->extension, FALSE);
    g_return_val_if_fail(g_task_is_valid(result, context), FALSE);

    return g_task_propagate_boolean(G_TASK(result), error);
}

#else // ENABLE(WK_WEB_EXTENSIONS)

void webkitWebExtensionContextSetWebExtension(WebKitWebExtensionContext* context, WebKitWebExtension* extension)
{
    return;
}

WebKitWebExtensionContext* webkit_web_extension_context_new_for_extension(WebKitWebExtension* extension, GError** error)
{
    return nullptr;
}

WebKitWebExtension* webkit_web_extension_context_get_web_extension(WebKitWebExtensionContext* context)
{
    return nullptr;
}

const gchar* webkit_web_extension_context_get_base_uri(WebKitWebExtensionContext* context)
{
    return "";
}

void webkit_web_extension_context_set_base_uri(WebKitWebExtensionContext* context, const gchar* baseURI)
{
    return;
}

const gchar* webkit_web_extension_context_get_options_page_uri(WebKitWebExtensionContext* context)
{
    return "";
}

gboolean webkit_web_extension_context_get_has_injected_content(WebKitWebExtensionContext* context)
{
    return FALSE;
}

gboolean webkit_web_extension_context_has_injected_content_for_uri(WebKitWebExtensionContext* context, const gchar* uri)
{
    return FALSE;
}

const gchar* webkit_web_extension_context_get_override_new_tab_page_uri(WebKitWebExtensionContext* context)
{
    return "";
}

gboolean webkit_web_extension_context_get_has_access_to_all_uris(WebKitWebExtensionContext* context)
{
    return FALSE;
}

gboolean webkit_web_extension_context_get_has_access_to_all_hosts(WebKitWebExtensionContext* context)
{
    return FALSE;
}
WebKitWebExtensionContextPermission** webkit_web_extension_context_get_granted_permissions(WebKitWebExtensionContext* context)
{
    return nullptr;
}

void webkit_web_extension_context_set_granted_permissions(WebKitWebExtensionContext* context, WebKitWebExtensionContextPermission** grantedPermissions)
{
    return;
}

WebKitWebExtensionContextMatchPattern** webkit_web_extension_context_get_granted_permission_match_patterns(WebKitWebExtensionContext* context)
{
    return nullptr;
}

void webkit_web_extension_context_set_granted_permission_match_patterns(WebKitWebExtensionContext* context, WebKitWebExtensionContextMatchPattern** grantedPermissionMatchPatterns)
{
    return;
}

WebKitWebExtensionContextPermission** webkit_web_extension_context_get_denied_permissions(WebKitWebExtensionContext* context)
{
    return nullptr;
}

void webkit_web_extension_context_set_denied_permissions(WebKitWebExtensionContext* context, WebKitWebExtensionContextPermission** deniedPermissions)
{
    return;
}

WebKitWebExtensionContextMatchPattern** webkit_web_extension_context_get_denied_permission_match_patterns(WebKitWebExtensionContext* context)
{
    return nullptr;
}

void webkit_web_extension_context_set_denied_permission_match_patterns(WebKitWebExtensionContext* context, WebKitWebExtensionContextMatchPattern** deniedPermissionMatchPatterns)
{
    return;
}

gboolean webkit_web_extension_context_has_permission(WebKitWebExtensionContext* context, const gchar* permission)
{
    return FALSE;
}

gboolean webkit_web_extension_context_has_access_to_uri(WebKitWebExtensionContext* context, const gchar* uri)
{
    return FALSE;
}

WebKitWebExtensionContextPermissionStatus webkit_web_extension_context_get_permission_status(WebKitWebExtensionContext* context, const gchar* permission)
{
    return WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_UNKNOWN;
}

void webkit_web_extension_context_set_permission_status(WebKitWebExtensionContext* context, const gchar* permission, WebKitWebExtensionContextPermissionStatus permissionStatus, GDateTime* expirationDate)
{
    return;
}

WebKitWebExtensionContextPermissionStatus webkit_web_extension_context_get_permission_status_for_uri(WebKitWebExtensionContext* context, const gchar* uri)
{
    return WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_UNKNOWN;
}

void webkit_web_extension_context_set_permission_status_for_uri(WebKitWebExtensionContext* context, const gchar* uri, WebKitWebExtensionContextPermissionStatus permissionStatus, GDateTime* expirationDate)
{
    return;
}

WebKitWebExtensionContextPermissionStatus webkit_web_extension_context_get_permission_status_for_match_pattern(WebKitWebExtensionContext* context, WebKitWebExtensionMatchPattern* matchPattern)
{
    return WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_UNKNOWN;
}

void webkit_web_extension_context_set_permission_status_for_match_pattern(WebKitWebExtensionContext* context, WebKitWebExtensionMatchPattern* matchPattern, WebKitWebExtensionContextPermissionStatus permissionStatus, GDateTime* expirationDate)
{
    return;
}

void webkit_web_extension_context_load_background_content(WebKitWebExtensionContext* context, GCancellable* cancellable, GAsyncReadyCallback callback, gpointer userData)
{
    GRefPtr<GTask> task = adoptGRef(g_task_new(context, cancellable, callback, userData));
        auto errorStr = _("Unsupported");
        g_task_return_new_error(task.get(), webkit_web_extension_context_error_quark(),
            WEBKIT_WEB_EXTENSION_CONTEXT_ERROR_UNKNOWN, "%s", errorStr);
    return;
}

gboolean webkit_web_extension_context_load_background_content_finish(WebKitWebExtensionContext* context, GAsyncResult* result, GError** error)
{
    return FALSE;
}

#endif // ENABLE(WK_WEB_EXTENSIONS)

#endif // ENABLE(2022_GLIB_API)
