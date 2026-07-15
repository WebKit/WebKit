/*
 * Copyright (C) 2025 Igalia S.L.
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
#include <wtf/URLParser.h>
#include <wtf/glib/WTFGType.h>

constexpr auto WEBKIT_CONTEXT_ERROR_DOMAIN = "WKWebExtensionContextErrorDomain"_s;

/**
 * WebKitWebExtensionContextPermission:
 *
 * Represents a Permission with its expiration dates. A permission that doesn't expire will have a distant future date.
 *
 * Since: 2.54
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
 * If no expiration date is provided, or the permission should not expire, a date in the distant future will be used.
 *
 * Returns: the newly created [struct@WebExtensionContextPermission]
 *
 * Since: 2.54
 */
WebKitWebExtensionContextPermission* webkit_web_extension_context_permission_new(const char* permission, GDateTime* expiration)
{
    g_return_val_if_fail(permission, nullptr);
    WallTime expirationDate;

    if (expiration)
        expirationDate = WallTime::fromRawSeconds(g_date_time_to_unix(expiration));
    else {
        GRefPtr<GDateTime> dt = adoptGRef(g_date_time_new_utc(9999, 12, 31, 23, 59, 00));
        expirationDate = WallTime::fromRawSeconds(g_date_time_to_unix(dt.get()));
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
 * Since: 2.54
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
 * Since: 2.54
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
 * Since: 2.54
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
 * Since: 2.54
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
 * Since: 2.54
 */
struct _WebKitWebExtensionContextMatchPattern {
#if ENABLE(WK_WEB_EXTENSIONS)
    explicit _WebKitWebExtensionContextMatchPattern(Ref<WebKit::WebExtensionMatchPattern> pattern, const WallTime& expiration)
        : pattern(webkitWebExtensionMatchPatternCreate(pattern))
        , expiration(adoptGRef(g_date_time_new_from_unix_utc(expiration.secondsSinceEpoch().secondsAs<gint64>())))
    {
    }
    WebKitWebExtensionMatchPattern* pattern;
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
 * @pattern: The #WebKitWebExtensionMatchPattern to represent
 * @expiration: (nullable): The expiration date for this permission
 *
 * Create a new [struct@WebExtensionContextMatchPattern] for the provided match pattern and expiration date.
 * 
 * If no expiration date is provided, or the match pattern should not expire, a date in the distant future will be used.
 * 
 * Returns: the newly created [struct@WebExtensionContextMatchPattern]
 *
 * Since: 2.54
 */
WebKitWebExtensionContextMatchPattern* webkit_web_extension_context_match_pattern_new(WebKitWebExtensionMatchPattern* pattern, GDateTime* expiration)
{
    g_return_val_if_fail(pattern, nullptr);

    auto expirationDate = expiration ? WallTime::fromRawSeconds(g_date_time_to_unix(expiration)) : WallTime::fromRawSeconds(g_date_time_to_unix(g_date_time_new_utc(9999, 12, 31, 23, 59, 00)));
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
 * Since: 2.54
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
 * Since: 2.54
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
 * @pattern: a #WebKitWebExtensionMatchPattern
 *
 * Get the match pattern of @pattern.
 *
 * Returns: (transfer none): The #WebKitWebExtensionMatchPattern.
 *
 * Since: 2.54
 */
WebKitWebExtensionMatchPattern* webkit_web_extension_context_match_pattern_get_match_pattern(WebKitWebExtensionContextMatchPattern* pattern)
{
    g_return_val_if_fail(pattern, nullptr);

    return pattern->pattern;
}

/**
 * webkit_web_extension_context_match_pattern_get_expiration_date:
 * @pattern: (nullable): a [struct@WebExtensionContextMatchPattern]
 *
 * Get the expiration date of @pattern. If the match pattern does not expire, a distant future date will be returned instead.
 *
 * Returns: (transfer none): The expiration date of @pattern
 *
 * Since: 2.54
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
 * Since: 2.54
 */

static void gInitableInterfaceInit(GInitableIface*);

struct _WebKitWebExtensionContextPrivate {
#if ENABLE(WK_WEB_EXTENSIONS)
    RefPtr<WebKit::WebExtensionContext> context;
    GRefPtr<WebKitWebExtension> extension;
    CString baseURI;
    CString uniqueIdentifier;
    CString inspectionName;
    CString optionsPageURI;
    CString overrideNewTabPageURI;
    GRefPtr<GPtrArray> unsupportedAPIs;
    GRefPtr<GPtrArray> currentPermissions;
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
    PROP_UNIQUE_IDENTIFIER,
    PROP_IS_INSPECTABLE,
    PROP_INSPECTION_NAME,
    PROP_UNSUPPORTED_APIS,
    PROP_OPTIONS_PAGE_URI,
    PROP_OVERRIDE_NEW_TAB_PAGE_URI,
    PROP_HAS_REQUESTED_OPTIONAL_ACCESS_TO_ALL_HOSTS,
    PROP_HAS_ACCESS_TO_PRIVATE_DATA,
    PROP_CURRENT_PERMISSIONS,
    PROP_HAS_ACCESS_TO_ALL_URIS,
    PROP_HAS_ACCESS_TO_ALL_HOSTS,
    PROP_HAS_INJECTED_CONTENT,
    PROP_HAS_CONTENT_MODIFICATION_RULES,
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
    case PROP_UNIQUE_IDENTIFIER:
        g_value_set_string(value, webkit_web_extension_context_get_unique_identifier(context));
        break;
    case PROP_IS_INSPECTABLE:
        g_value_set_boolean(value, webkit_web_extension_context_get_is_inspectable(context));
        break;
    case PROP_INSPECTION_NAME:
        g_value_set_string(value, webkit_web_extension_context_get_inspection_name(context));
        break;
    case PROP_UNSUPPORTED_APIS:
        g_value_set_boxed(value, webkit_web_extension_context_get_unsupported_apis(context));
        break;
    case PROP_OPTIONS_PAGE_URI:
        g_value_set_string(value, webkit_web_extension_context_get_options_page_uri(context));
        break;
    case PROP_OVERRIDE_NEW_TAB_PAGE_URI:
        g_value_set_string(value, webkit_web_extension_context_get_override_new_tab_page_uri(context));
        break;
    case PROP_HAS_REQUESTED_OPTIONAL_ACCESS_TO_ALL_HOSTS:
        g_value_set_boolean(value, webkit_web_extension_context_get_has_requested_optional_access_to_all_hosts(context));
        break;
    case PROP_HAS_ACCESS_TO_PRIVATE_DATA:
        g_value_set_boolean(value, webkit_web_extension_context_get_has_access_to_private_data(context));
        break;
    case PROP_CURRENT_PERMISSIONS:
        g_value_set_boxed(value, webkit_web_extension_context_get_current_permissions(context));
        break;
    case PROP_HAS_ACCESS_TO_ALL_URIS:
        g_value_set_boolean(value, webkit_web_extension_context_get_has_access_to_all_uris(context));
        break;
    case PROP_HAS_ACCESS_TO_ALL_HOSTS:
        g_value_set_boolean(value, webkit_web_extension_context_get_has_access_to_all_hosts(context));
        break;
    case PROP_HAS_INJECTED_CONTENT:
        g_value_set_boolean(value, webkit_web_extension_context_get_has_injected_content(context));
        break;
    case PROP_HAS_CONTENT_MODIFICATION_RULES:
        g_value_set_boolean(value, webkit_web_extension_context_get_has_content_modification_rules(context));
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
    case PROP_UNIQUE_IDENTIFIER:
        webkit_web_extension_context_set_unique_identifier(context, g_value_get_string(value));
        break;
    case PROP_IS_INSPECTABLE:
        webkit_web_extension_context_set_is_inspectable(context, g_value_get_boolean(value));
        break;
    case PROP_INSPECTION_NAME:
        webkit_web_extension_context_set_inspection_name(context, g_value_get_string(value));
        break;
    case PROP_UNSUPPORTED_APIS:
        webkit_web_extension_context_set_unsupported_apis(context, static_cast<const gchar**>(g_value_get_boxed(value)));
        break;
    case PROP_HAS_REQUESTED_OPTIONAL_ACCESS_TO_ALL_HOSTS:
        webkit_web_extension_context_set_has_requested_optional_access_to_all_hosts(context, g_value_get_boolean(value));
        break;
    case PROP_HAS_ACCESS_TO_PRIVATE_DATA:
        webkit_web_extension_context_set_has_access_to_private_data(context, g_value_get_boolean(value));
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
     * Since: 2.54
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
     * Since: 2.54
     */
    properties[PROP_BASE_URI] =
        g_param_spec_string(
            "base-uri",
            nullptr, nullptr,
            nullptr,
            WEBKIT_PARAM_READWRITE
        );

    /**
     * WebKitWebExtensionContext:unique-identifier:
     * 
     * The unique identifier of this context.
     * See [method@WebExtensionContext.get_unique_identifier] for more details.
     *
     * Since: 2.54
     */
    properties[PROP_UNIQUE_IDENTIFIER] =
        g_param_spec_string(
            "unique-identifier",
            nullptr, nullptr,
            nullptr,
            WEBKIT_PARAM_READWRITE
        );

    /**
     * WebKitWebExtensionContext:is-inspectable:
     * 
     * Whether Web Inspector can inspect the [class@WebView] instances of this context.
     * See [method@WebExtensionContext.get_is_inspectable] for more details.
     *
     * Since: 2.54
     */
    properties[PROP_IS_INSPECTABLE] =
        g_param_spec_boolean(
            "is-inspectable",
            nullptr, nullptr,
            FALSE,
            WEBKIT_PARAM_READWRITE
        );

    /**
     * WebKitWebExtensionContext:inspection-name:
     * 
     * The name shown when inspecting the background web view.
     * See [method@WebExtensionContext.get_inspection_name] for more details.
     *
     * Since: 2.54
     */
    properties[PROP_INSPECTION_NAME] =
        g_param_spec_string(
            "inspection-name",
            nullptr, nullptr,
            nullptr,
            WEBKIT_PARAM_READWRITE
        );

    /**
     * WebKitWebExtensionContext:unsupported-apis:
     * 
     * A list of the unsupported APIs for this extension
     * See [method@WebExtensionContext.get_unsupported_apis] for more details.
     *
     * Since: 2.54
     */
    properties[PROP_UNSUPPORTED_APIS] =
        g_param_spec_boxed(
            "unsupported-apis",
            nullptr, nullptr,
            G_TYPE_STRV,
            WEBKIT_PARAM_READWRITE
        );

    /**
     * WebKitWebExtensionContext:options-page-uri:
     * 
     * The URI of the extension's options page.
     * See [method@WebExtensionContext.get_options_page_uri] for more details.
     *
     * Since: 2.54
     */
    properties[PROP_OPTIONS_PAGE_URI] =
        g_param_spec_string(
            "options-page-uri",
            nullptr, nullptr,
            nullptr,
            WEBKIT_PARAM_READWRITE
        );

    /**
     * WebKitWebExtensionContext:override-new-tab-page-uri:
     * 
     * The URI to use as an alternative to the default new tab page.
     * See [method@WebExtensionContext.get_override_new_tab_page_uri] for more details.
     *
     * Since: 2.54
     */
    properties[PROP_OVERRIDE_NEW_TAB_PAGE_URI] =
        g_param_spec_string(
            "override-new-tab-page-uri",
            nullptr, nullptr,
            nullptr,
            WEBKIT_PARAM_READWRITE
        );

    /**
     * WebKitWebExtensionContext:has-requested-optional-access-to-all-hosts:
     * 
     * Whether the extension has requested optional access to all hosts.
     * See [method@WebExtensionContext.get_has_requested_optional_access_to_all_hosts] for more details.
     *
     * Since: 2.54
     */
    properties[PROP_HAS_REQUESTED_OPTIONAL_ACCESS_TO_ALL_HOSTS] =
        g_param_spec_boolean(
            "has-requested-optional-access-to-all-hosts",
            nullptr, nullptr,
            FALSE,
            WEBKIT_PARAM_READWRITE
        );

    /**
     * WebKitWebExtensionContext:has-access-to-private-data:
     * 
     * Whether the extension has access to private data.
     * See [method@WebExtensionContext.get_has_access_to_private_data] for more details.
     *
     * Since: 2.54
     */
    properties[PROP_HAS_ACCESS_TO_PRIVATE_DATA] =
        g_param_spec_boolean(
            "has-access-to-private-data",
            nullptr, nullptr,
            FALSE,
            WEBKIT_PARAM_READWRITE
        );

    /**
     * WebKitWebExtensionContext:current-permissions:
     * 
     * The currently granted permissions that have not expired.
     * See [method@WebExtensionContext.get_current_permissions] for more details.
     *
     * Since: 2.54
     */
    properties[PROP_CURRENT_PERMISSIONS] =
        g_param_spec_boxed(
            "current-permissions",
            nullptr, nullptr,
            G_TYPE_STRV,
            WEBKIT_PARAM_READWRITE
        );

    /**
     * WebKitWebExtensionContext:has-access-to-all-uris:
     * 
     * Whether the currently granted permission match patterns set contains the `<all_urls>` pattern.
     * See [method@WebExtensionContext.get_has_access_to_all_uris] for more details.
     *
     * Since: 2.54
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
     * Since: 2.54
     */
    properties[PROP_HAS_ACCESS_TO_ALL_HOSTS] =
        g_param_spec_boolean(
            "has-access-to-all-hosts",
            nullptr, nullptr,
            FALSE,
            WEBKIT_PARAM_READABLE
        );

    /**
     * WebKitWebExtensionContext:has-injected-content:
     * 
     * Whether the extension has script or stylesheet content that can be injected into webpages.
     * See [method@WebExtensionContext.get_has_injected_content] for more details.
     *
     * Since: 2.54
     */
    properties[PROP_HAS_INJECTED_CONTENT] =
        g_param_spec_boolean(
            "has-injected-content",
            nullptr, nullptr,
            FALSE,
            WEBKIT_PARAM_READABLE
        );

    /**
     * WebKitWebExtensionContext:has-content-modification-rules:
     * 
     * Whether the extension includes rules used for content modification or blocking.
     * See [method@WebExtensionContext.get_has_content_modification_rules] for more details.
     *
     * Since: 2.54
     */
    properties[PROP_HAS_CONTENT_MODIFICATION_RULES] =
        g_param_spec_boolean(
            "has-content-modification-rules",
            nullptr, nullptr,
            FALSE,
            WEBKIT_PARAM_READABLE
        );

    g_object_class_install_properties(objectClass, properties.size(), properties.data());

    /**
     * WebKitWebExtensionContext::granted-permissions-were-removed:
     * @context: the [class@WebExtensionContext]
     *
     * This signal is emitted whenever previously granted permissions were
     * removed.
     *
     * Since: 2.54
     */
    signals[GRANTED_PERMISSIONS_WERE_REMOVED] =
        g_signal_new("granted-permissions-were-removed",
            G_TYPE_FROM_CLASS(objectClass),
            G_SIGNAL_RUN_LAST,
            0, 0, 0,
            g_cclosure_marshal_VOID__VOID,
            G_TYPE_NONE, 0);

    /**
     * WebKitWebExtensionContext::granted-permission-match-patterns-were-removed:
     * @context: the [class@WebExtensionContext]
     *
     * This signal is emitted whenever previously granted permission match patterns
     * were removed.
     *
     * Since: 2.54
     */
    signals[GRANTED_PERMISSION_MATCH_PATTERNS_WERE_REMOVED] =
        g_signal_new("granted-permission-match-patterns-were-removed",
            G_TYPE_FROM_CLASS(objectClass),
            G_SIGNAL_RUN_LAST,
            0, 0, 0,
            g_cclosure_marshal_VOID__VOID,
            G_TYPE_NONE, 0);

    /**
     * WebKitWebExtensionContext::denied-permissions-were-removed:
     * @context: the [class@WebExtensionContext]
     *
     * This signal is emitted whenever previously denied permissions
     * were removed.
     *
     * Since: 2.54
     */
    signals[DENIED_PERMISSIONS_WERE_REMOVED] =
        g_signal_new("denied-permissions-were-removed",
            G_TYPE_FROM_CLASS(objectClass),
            G_SIGNAL_RUN_LAST,
            0, 0, 0,
            g_cclosure_marshal_VOID__VOID,
            G_TYPE_NONE, 0);

    /**
     * WebKitWebExtensionContext::denied-permission-match-patterns-were-removed:
     * @context: the [class@WebExtensionContext]
     *
     * This signal is emitted whenever previously denied permission match patterns
     * were removed.
     *
     * Since: 2.54
     */
    signals[DENIED_PERMISSION_MATCH_PATTERNS_WERE_REMOVED] =
        g_signal_new("denied-permission-match-patterns-were-removed",
            G_TYPE_FROM_CLASS(objectClass),
            G_SIGNAL_RUN_LAST,
            0, 0, 0,
            g_cclosure_marshal_VOID__VOID,
            G_TYPE_NONE, 0);

    /**
     * WebKitWebExtensionContext::permissions-were-denied:
     * @context: the [class@WebExtensionContext]
     *
     * This signal is emitted whenever permissions were denied.
     *
     * Since: 2.54
     */
    signals[PERMISSIONS_WERE_DENIED] =
        g_signal_new("permissions-were-denied",
            G_TYPE_FROM_CLASS(objectClass),
            G_SIGNAL_RUN_LAST,
            0, 0, 0,
            g_cclosure_marshal_VOID__VOID,
            G_TYPE_NONE, 0);

    /**
     * WebKitWebExtensionContext::permissions-were-granted:
     * @context: the [class@WebExtensionContext]
     *
     * This signal is emitted whenever permissions were granted.
     *
     * Since: 2.54
     */
    signals[PERMISSIONS_WERE_GRANTED] =
        g_signal_new("permissions-were-granted",
            G_TYPE_FROM_CLASS(objectClass),
            G_SIGNAL_RUN_LAST,
            0, 0, 0,
            g_cclosure_marshal_VOID__VOID,
            G_TYPE_NONE, 0);

    /**
     * WebKitWebExtensionContext::permission-match-patterns-were-denied:
     * @context: the [class@WebExtensionContext]
     *
     * This signal is emitted whenever permission match patterns were denied.
     *
     * Since: 2.54
     */
    signals[PERMISSION_MATCH_PATTERNS_WERE_DENIED] =
        g_signal_new("permission-match-patterns-were-denied",
            G_TYPE_FROM_CLASS(objectClass),
            G_SIGNAL_RUN_LAST,
            0, 0, 0,
            g_cclosure_marshal_VOID__VOID,
            G_TYPE_NONE, 0);

    /**
     * WebKitWebExtensionContext::permission-match-patterns-were-granted:
     * @context: the [class@WebExtensionContext]
     *
     * This signal is emitted whenever permission match patterns were granted.
     *
     * Since: 2.54
     */
    signals[PERMISSION_MATCH_PATTERNS_WERE_GRANTED] =
        g_signal_new("permission-match-patterns-were-granted",
            G_TYPE_FROM_CLASS(objectClass),
            G_SIGNAL_RUN_LAST,
            0, 0, 0,
            g_cclosure_marshal_VOID__VOID,
            G_TYPE_NONE, 0);
}

static gboolean webkitWebExtensionContextInitableInit(GInitable* initable, GCancellable* cancellable, GError** error)
{
#if ENABLE(WK_WEB_EXTENSIONS)
    WebKitWebExtensionContext* self = WEBKIT_WEB_EXTENSION_CONTEXT(initable);
    if (!webkit_web_extension_context_get_web_extension(self))
        return FALSE;

    auto webExtension = webkitWebExtensionToImpl(webkit_web_extension_context_get_web_extension(self));

    Ref context = WebKit::WebExtensionContext::create(*webExtension, adoptGRef(self));

    // We only want to return errors that are in the Context error domain here. It is assumed that any errors that came up for the WebKitWebExtension would have been handled before adding to a context
    if (!context->errors().isEmpty()) {
        for (Ref internalError : context->errors()) {
            if (internalError->domain() == WEBKIT_CONTEXT_ERROR_DOMAIN) {
                g_set_error(error, webkit_web_extension_context_error_quark(),
                    toWebKitWebExtensionContextError(internalError->errorCode()), internalError->localizedDescription().utf8().data(), nullptr);
                return FALSE;
            }
        }
    }

    self->priv->context = WTF::move(context);
    g_object_weak_ref(G_OBJECT(self->priv->extension.get()), +[](gpointer userData, GObject*) {
        WebKitWebExtensionContext* context = WEBKIT_WEB_EXTENSION_CONTEXT(userData);
        context->priv->extension = nullptr;
    }, self);

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
    g_return_if_fail(WEBKIT_IS_WEB_EXTENSION_CONTEXT(context));

    if (extension)
        context->priv->extension = extension;
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
 * Since: 2.52
 */
WebKitWebExtensionContext* webkit_web_extension_context_new_for_extension(WebKitWebExtension* extension, GError** error)
{
    if (auto object = g_initable_new (WEBKIT_TYPE_WEB_EXTENSION_CONTEXT, nullptr, error, "web-extension", extension, nullptr))
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
 * Since: 2.54
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
 * Since: 2.54
 */
const gchar* webkit_web_extension_context_get_base_uri(WebKitWebExtensionContext* context)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION_CONTEXT(context), nullptr);
    g_return_val_if_fail(context->priv->extension, nullptr);

    WebKitWebExtensionContextPrivate* priv = context->priv;
    if (!priv->baseURI.isNull())
        return priv->baseURI.data();

    auto baseURI = priv->context->baseURL();
    g_return_val_if_fail(!baseURI.isEmpty(), nullptr);

    priv->baseURI = baseURI.string().utf8();
    return priv->baseURI.data();
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
 * Since: 2.54
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
 * webkit_web_extension_context_get_unique_identifier:
 * @context: a [class@WebExtensionContext]
 *
 * Get the unique identifier used to distinguish the extension from other extensions and target it for messages.
 * 
 * The default value is a unique value that matches the host in the default base URI. The identifier can be any
 * value that is unique. This value is accessible by the extension via `browser.runtime.id` and is used for
 * messaging the extension via `browser.runtime.sendMessage()`.
 * 
 * Returns: the unique identifier
 * 
 * Since: 2.54
 */
const gchar* webkit_web_extension_context_get_unique_identifier(WebKitWebExtensionContext* context)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION_CONTEXT(context), nullptr);
    g_return_val_if_fail(context->priv->extension, nullptr);

    WebKitWebExtensionContextPrivate* priv = context->priv;
    if (!priv->uniqueIdentifier.isNull())
        return priv->uniqueIdentifier.data();

    auto uniqueIdentifier = priv->context->uniqueIdentifier();
    g_return_val_if_fail(!uniqueIdentifier.isEmpty(), nullptr);

    priv->uniqueIdentifier = uniqueIdentifier.utf8();
    return priv->uniqueIdentifier.data();
}

/**
 * webkit_web_extension_context_set_unique_identifier:
 * @context: a [class@WebExtensionContext]
 * @unique_identifier: (nullable): The unique identifier to use for this context.
 *
 * Sets the unique identifier used to distinguish the extension from other extensions and target it for messages.
 * 
 * The identifier can be any value that is unique. Setting is only allowed when the context is not loaded. If the identifier
 * is %NULL, a randomly generated identifier will be used instead.
 * 
 * Since: 2.54
 */
void webkit_web_extension_context_set_unique_identifier(WebKitWebExtensionContext* context, const gchar* uniqueIdentifier)
{
    g_return_if_fail(WEBKIT_IS_WEB_EXTENSION_CONTEXT(context));
    g_return_if_fail(context->priv->extension);

    WebKitWebExtensionContextPrivate* priv = context->priv;
    priv->context->setUniqueIdentifier(String::fromUTF8(uniqueIdentifier));
}

/**
 * webkit_web_extension_context_get_is_inspectable:
 * @context: a [class@WebExtensionContext]
 *
 * Gets whether the remote Web Inspector can inspect the [class@WebView] instances for this context.
 * 
 * A context can control multiple [class@WebView] instances, from the background content, to the popover.
 * You should set this to `TRUE` when needed for debugging purposes. The default value is `FALSE`.
 * 
 * Returns: `TRUE` if the Web Inspector can inspect the web views.
 * 
 * Since: 2.54
 */
gboolean webkit_web_extension_context_get_is_inspectable(WebKitWebExtensionContext* context)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION_CONTEXT(context), FALSE);
    g_return_val_if_fail(context->priv->extension, FALSE);

    WebKitWebExtensionContextPrivate* priv = context->priv;
    return priv->context->isInspectable();
}

/**
 * webkit_web_extension_context_set_is_inspectable:
 * @context: a [class@WebExtensionContext]
 * @is_inspectable: Whether the Web Inspector can inspect the #WebKitWebView instances
 *
 * Sets whether the remote Web Inspector can inspect the #WebKitWebView instances for this context.
 * 
 * A context can control multiple #WebKitWebView instances, from the background content, to the popover.
 * You should set this to `TRUE` when needed for debugging purposes. The default value is `FALSE`.
 * 
 * Since: 2.54
 */
void webkit_web_extension_context_set_is_inspectable(WebKitWebExtensionContext* context, gboolean isInspectable)
{
    g_return_if_fail(WEBKIT_IS_WEB_EXTENSION_CONTEXT(context));
    g_return_if_fail(context->priv->extension);

    WebKitWebExtensionContextPrivate* priv = context->priv;
    priv->context->setInspectable(isInspectable);
}

/**
 * webkit_web_extension_context_get_inspection_name:
 * @context: a [class@WebExtensionContext]
 *
 * Get the name shown when inspecting the background web view.
 * 
 * Returns: (nullable): the name shown, or %NULL if there is no name set.
 * 
 * Since: 2.54
 */
const gchar* webkit_web_extension_context_get_inspection_name(WebKitWebExtensionContext* context)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION_CONTEXT(context), nullptr);
    g_return_val_if_fail(context->priv->extension, nullptr);

    WebKitWebExtensionContextPrivate* priv = context->priv;
    if (!priv->inspectionName.isNull())
        return priv->inspectionName.data();

    auto inspectionName = priv->context->backgroundWebViewInspectionName();
    if (inspectionName.isEmpty())
        return nullptr;

    priv->inspectionName = inspectionName.utf8();
    return priv->inspectionName.data();
}

/**
 * webkit_web_extension_context_set_inspection_name:
 * @context: a [class@WebExtensionContext]
 * @inspection_name: The name to show when inspecting the background web view
 *
 * Sets the name shown when inspecting the background web view.
 * 
 * Since: 2.54
 */
void webkit_web_extension_context_set_inspection_name(WebKitWebExtensionContext* context, const gchar* inspectionName)
{
    g_return_if_fail(WEBKIT_IS_WEB_EXTENSION_CONTEXT(context));
    g_return_if_fail(context->priv->extension);
    g_return_if_fail(inspectionName);
    g_return_if_fail(g_strcmp0(inspectionName, ""));

    WebKitWebExtensionContextPrivate* priv = context->priv;
    auto backgroundInspectionName = String::fromUTF8(inspectionName);
    priv->context->setBackgroundWebViewInspectionName(backgroundInspectionName);
}

/**
 * webkit_web_extension_context_get_unsupported_apis:
 * @context: a [class@WebExtensionContext]
 *
 * Get the unsupported APIs for this extension.
 * 
 * This allows the app to specify a subset of web extension APIs that it chooses not to support, effectively making
 * these APIs `undefined` within the extension's JavaScript contexts. This enables extensions to employ feature detection techniques
 * for unsupported APIs, allowing them to adapt their behavior based on the APIs actually supported by the app.
 * 
 * Returns: (nullable) (array zero-terminated=1) (transfer full): a %NULL-terminated array of strings representing
 * APIs that are currently marked as unsupported, and therefore `undefined` in this extension's JavaScript contexts,
 * or %NULL otherwise. This array and its contents are owned by WebKit and should not be modified or freed.
 * 
 * Since: 2.54
 */
const gchar* const * webkit_web_extension_context_get_unsupported_apis(WebKitWebExtensionContext* context)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION_CONTEXT(context), nullptr);
    g_return_val_if_fail(context->priv->extension, nullptr);

    WebKitWebExtensionContextPrivate* priv = context->priv;
    auto unsupportedAPIs = priv->context->unsupportedAPIs();
    if (unsupportedAPIs.isEmpty())
        return nullptr;

    priv->unsupportedAPIs = adoptGRef(g_ptr_array_new_full(unsupportedAPIs.size(), g_free));
    for (auto api : unsupportedAPIs)
        g_ptr_array_add(priv->unsupportedAPIs.get(), g_strdup(api.utf8().data()));
    g_ptr_array_add(priv->unsupportedAPIs.get(), nullptr);

    return reinterpret_cast<gchar**>(priv->unsupportedAPIs->pdata);
}

/**
 * webkit_web_extension_context_set_unsupported_apis:
 * @context: a [class@WebExtensionContext]
 * @unsupported_apis: (allow-none) (array zero-terminated=1) (element-type utf8) (transfer none): A %NULL-terminated array of stringsrepresenting APIs that should be marked as unsupported,
 * and therefore `undefined` in this extension's JavaScript contexts.
 *
 * Specify unsupported APIs for this extension, making them `undefined` in JavaScript.
 * 
 * This allows the app to specify a subset of web extension APIs that it chooses not to support, effectively making
 * these APIs `undefined` within the extension's JavaScript contexts. This enables extensions to employ feature detection techniques
 * for unsupported APIs, allowing them to adapt their behavior based on the APIs actually supported by the app. Setting is only allowed when
 * the context is not loaded. Only certain APIs can be specified here, particularly those within the `browser` namespace and other dynamic
 * functions and properties, anything else will be silently ignored.
 * 
 * For example, specifying `"browser.windows.create"` and `"browser.storage"` in this set will result in the
 * `browser.windows.create()` function and `browser.storage` property being `undefined`.
 * 
 * Since: 2.54
 */
void webkit_web_extension_context_set_unsupported_apis(WebKitWebExtensionContext* context, const gchar* const* unsupportedAPIs)
{
    g_return_if_fail(WEBKIT_IS_WEB_EXTENSION_CONTEXT(context));
    g_return_if_fail(context->priv->extension);
    g_return_if_fail(unsupportedAPIs);

    WebKitWebExtensionContextPrivate* priv = context->priv;
    HashSet<String> unsupportedAPISet;
    for (const char* api : span(const_cast<char**>(unsupportedAPIs)))
        unsupportedAPISet.add(String::fromUTF8(api));

    priv->context->setUnsupportedAPIs(WTF::move(unsupportedAPISet));
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
 * Since: 2.54
 */
const gchar* webkit_web_extension_context_get_options_page_uri(WebKitWebExtensionContext* context)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION_CONTEXT(context), nullptr);
    g_return_val_if_fail(context->priv->extension, nullptr);

    WebKitWebExtensionContextPrivate* priv = context->priv;
    if (!priv->optionsPageURI.isNull())
        return priv->optionsPageURI.data();

    auto optionsPageURI = priv->context->optionsPageURL();
    if (optionsPageURI.isEmpty())
        return nullptr;

    priv->optionsPageURI = optionsPageURI.string().utf8();
    return priv->optionsPageURI.data();
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
 * Since: 2.54
 */
const gchar* webkit_web_extension_context_get_override_new_tab_page_uri(WebKitWebExtensionContext* context)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION_CONTEXT(context), nullptr);
    g_return_val_if_fail(context->priv->extension, nullptr);

    WebKitWebExtensionContextPrivate* priv = context->priv;
    if (!priv->overrideNewTabPageURI.isNull())
        return priv->overrideNewTabPageURI.data();

    auto overrideNewTabPageURI = priv->context->overrideNewTabPageURL();
    if (overrideNewTabPageURI.isEmpty())
        return nullptr;

    priv->overrideNewTabPageURI = overrideNewTabPageURI.string().utf8();
    return priv->overrideNewTabPageURI.data();
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
 * Since: 2.54
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
 * Since: 2.54
 */
void webkit_web_extension_context_set_granted_permissions(WebKitWebExtensionContext *context, WebKitWebExtensionContextPermission **grantedPermissions)
{
    g_return_if_fail(WEBKIT_IS_WEB_EXTENSION_CONTEXT(context));
    g_return_if_fail(context->priv->extension);

    WebKitWebExtensionContextPrivate* priv = context->priv;
    HashMap<String, WallTime> grantedPermissionsMap;

    WebKitWebExtensionContextPermission** permission = grantedPermissions;
    if (permission) {
        // We are using a null-terminated C array as input here, there is unfortunately not a great way to loop without pointer arithmetic.
        WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
        for (; *permission; permission++)
            grantedPermissionsMap.add(String::fromUTF8(webkit_web_extension_context_permission_get_permission_name(*permission)), WallTime::fromRawSeconds(g_date_time_to_unix(webkit_web_extension_context_permission_get_expiration_date(*permission))));
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
 * Since: 2.54
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
 * Since: 2.54
 */
void webkit_web_extension_context_set_granted_permission_match_patterns(WebKitWebExtensionContext *context, WebKitWebExtensionContextMatchPattern **grantedPermissionMatchPatterns)
{
    g_return_if_fail(WEBKIT_IS_WEB_EXTENSION_CONTEXT(context));
    g_return_if_fail(context->priv->extension);

    WebKitWebExtensionContextPrivate* priv = context->priv;
    HashMap<Ref<WebKit::WebExtensionMatchPattern>, WallTime> grantedPermissionMatchPatternsMap;

    WebKitWebExtensionContextMatchPattern** pattern = grantedPermissionMatchPatterns;
    if (pattern) {
        WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
        for (; *pattern != nullptr; pattern++)
            grantedPermissionMatchPatternsMap.add(*webkitWebExtensionMatchPatternToImpl(webkit_web_extension_context_match_pattern_get_match_pattern(*pattern)), WallTime::fromRawSeconds(g_date_time_to_unix(webkit_web_extension_context_match_pattern_get_expiration_date(*pattern))));
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
 * Since: 2.54
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
    for (auto permission : permissions) {
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
 * @denied_permissions: (allow-none) (array zero-terminated=1) (element-type WebKitWebExtensionContextPermission) (transfer none): a %NULL-terminated list of permissions and their expiration dates
 *
 * Set the currently granted permissions and their expiration dates.
 * This will replace all existing denied permissions. Use this for saving and restoring permission status in bulk.
 * Permissions in this dictionary should be explicitly granted by the user before being added. Any permissions in this collection will not be
 * presented for approval again until they expire. This value should be saved and restored as needed by the app.
 * 
 * Since: 2.54
 */
void webkit_web_extension_context_set_denied_permissions(WebKitWebExtensionContext *context, WebKitWebExtensionContextPermission **deniedPermissions)
{
    g_return_if_fail(WEBKIT_IS_WEB_EXTENSION_CONTEXT(context));
    g_return_if_fail(context->priv->extension);

    WebKitWebExtensionContextPrivate* priv = context->priv;
    HashMap<String, WallTime> deniedPermissionsMap;

    WebKitWebExtensionContextPermission** permission = deniedPermissions;
    if (permission) {
        WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
        for (; *permission != nullptr; permission++)
            deniedPermissionsMap.add(String::fromUTF8(webkit_web_extension_context_permission_get_permission_name(*permission)), WallTime::fromRawSeconds(g_date_time_to_unix(webkit_web_extension_context_permission_get_expiration_date(*permission))));
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
 * Since: 2.54
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
    for (auto matchPattern : matchPatterns) {
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
 * Since: 2.54
 */
void webkit_web_extension_context_set_denied_permission_match_patterns(WebKitWebExtensionContext *context, WebKitWebExtensionContextMatchPattern **deniedPermissionMatchPatterns)
{
    g_return_if_fail(WEBKIT_IS_WEB_EXTENSION_CONTEXT(context));
    g_return_if_fail(context->priv->extension);

    WebKitWebExtensionContextPrivate* priv = context->priv;
    HashMap<Ref<WebKit::WebExtensionMatchPattern>, WallTime> deniedPermissionMatchPatternsMap;

    WebKitWebExtensionContextMatchPattern** pattern = deniedPermissionMatchPatterns;
    if (pattern) {
        WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
        for (; *pattern != nullptr; pattern++)
            deniedPermissionMatchPatternsMap.add(*webkitWebExtensionMatchPatternToImpl(webkit_web_extension_context_match_pattern_get_match_pattern(*pattern)), WallTime::fromRawSeconds(g_date_time_to_unix(webkit_web_extension_context_match_pattern_get_expiration_date(*pattern))));
        WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
    }

    priv->context->setDeniedPermissionMatchPatterns(WTF::move(deniedPermissionMatchPatternsMap));
}

/**
 * webkit_web_extension_context_get_has_requested_optional_access_to_all_hosts:
 * @context: a [class@WebExtensionContext]
 *
 * Get whether the extension has requested optional access to all hosts.
 * 
 * If this property is `TRUE`, the extension has asked for access to all hosts in a call to `browser.runtime.permissions.request()`,
 * and future permission checks will present discrete hosts for approval as being implicitly requested.
 * 
 * This value should be saved and restored as needed by the app via
 * webkit_web_extension_context_set_has_requested_optional_access_to_all_hosts().
 * 
 * Returns: %TRUE if the extension has requested access to all hosts.
 * 
 * Since: 2.54
 */
gboolean webkit_web_extension_context_get_has_requested_optional_access_to_all_hosts(WebKitWebExtensionContext* context)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION_CONTEXT(context), FALSE);
    g_return_val_if_fail(context->priv->extension, FALSE);

    WebKitWebExtensionContextPrivate* priv = context->priv;
    return priv->context->requestedOptionalAccessToAllHosts();
}

/**
 * webkit_web_extension_context_set_has_requested_optional_access_to_all_hosts:
 * @context: a [class@WebExtensionContext]
 * @requested_access_to_all_hosts: Whether to request optional access to all hosts
 *
 * Set whether the extension has requested optional access to all hosts.
 * 
 * If this property is `TRUE`, the extension has asked for access to all hosts in a call to `browser.runtime.permissions.request()`,
 * and future permission checks will present discrete hosts for approval as being implicitly requested.
 * 
 * Since: 2.54
 */
void webkit_web_extension_context_set_has_requested_optional_access_to_all_hosts(WebKitWebExtensionContext* context, gboolean requestedAccessToAllHosts)
{
    g_return_if_fail(WEBKIT_IS_WEB_EXTENSION_CONTEXT(context));
    g_return_if_fail(context->priv->extension);

    WebKitWebExtensionContextPrivate* priv = context->priv;
    priv->context->setRequestedOptionalAccessToAllHosts(requestedAccessToAllHosts);

    g_object_notify_by_pspec(G_OBJECT(context), properties[PROP_HAS_REQUESTED_OPTIONAL_ACCESS_TO_ALL_HOSTS]);
}

/**
 * webkit_web_extension_context_get_has_access_to_private_data:
 * @context: a [class@WebExtensionContext]
 *
 * Get whether the extension has access to private data.
 * 
 * If this property is `TRUE`, the extension is granted permission to interact with private windows, tabs, and cookies. Access to private data
 * should be explicitly allowed by the user before setting this property.
 * 
 * This value should be saved and restored as needed by the app via
 * webkit_web_extension_context_set_has_access_to_private_data().
 * 
 * Returns: %TRUE is the extension has been granted permission to access private data.
 * 
 * Since: 2.54
 */
gboolean webkit_web_extension_context_get_has_access_to_private_data(WebKitWebExtensionContext* context)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION_CONTEXT(context), FALSE);
    g_return_val_if_fail(context->priv->extension, FALSE);

    WebKitWebExtensionContextPrivate* priv = context->priv;
    return priv->context->hasAccessToPrivateData();
}

/**
 * webkit_web_extension_context_set_has_access_to_private_data:
 * @context: a [class@WebExtensionContext]
 * @has_access_to_private_data: Whether the extension should have access to private data.
 *
 * Sets whether the extension has access to private data.
 * 
 * If this property is `TRUE`, the extension is granted permission to interact with private windows, tabs, and cookies. Access to private data
 * should be explicitly allowed by the user before setting this property.
 * 
 * To ensure proper isolation between private and non-private data, web views associated with private data must use a
 * different [class@WebsiteDataManager]. Likewise, to be identified as a private web view and to ensure that cookies and other
 * website data is not shared, private web views must be configured to use a non-persistent [class@WebsiteDataManager].
 * 
 * Since: 2.54
 */
void webkit_web_extension_context_set_has_access_to_private_data(WebKitWebExtensionContext* context, gboolean hasAccessToPrivateData)
{
    g_return_if_fail(WEBKIT_IS_WEB_EXTENSION_CONTEXT(context));
    g_return_if_fail(context->priv->extension);

    WebKitWebExtensionContextPrivate* priv = context->priv;
    priv->context->setHasAccessToPrivateData(hasAccessToPrivateData);

    g_object_notify_by_pspec(G_OBJECT(context), properties[PROP_HAS_ACCESS_TO_PRIVATE_DATA]);
}

/**
 * webkit_web_extension_context_get_current_permissions:
 * @context: a [class@WebExtensionContext]
 *
 * Get the currently granted permissions that have not expired.
 * To get all currently granted permissions, use [method@WebExtensionContext.get_granted_permissions].
 * 
 * Returns: (nullable) (array zero-terminated=1) (transfer full): A %NULL-terminated list of permissions that have
 * been granted to the extension and have not expired, or %NULL otherwise.
 * 
 * Since: 2.54
 */
const gchar* const * webkit_web_extension_context_get_current_permissions(WebKitWebExtensionContext* context)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION_CONTEXT(context), nullptr);
    g_return_val_if_fail(context->priv->extension, nullptr);

    WebKitWebExtensionContextPrivate* priv = context->priv;
    auto currentPermissions = priv->context->currentPermissions();
    if (currentPermissions.isEmpty())
        return nullptr;

    priv->currentPermissions = adoptGRef(g_ptr_array_new_full(currentPermissions.size(), g_free));
    for (auto permission : currentPermissions)
        g_ptr_array_add(priv->currentPermissions.get(), g_strdup(permission.utf8().data()));
    g_ptr_array_add(priv->currentPermissions.get(), nullptr);

    return reinterpret_cast<gchar**>(priv->currentPermissions->pdata);
}

/**
 * webkit_web_extension_context_get_current_permission_match_patterns:
 * @context: a [class@WebExtensionContext]
 *
 * Get the currently granted permission match patterns that have not expired.
 * To get all currently granted permission match patterns, use [method@WebExtensionContext.get_granted_permission_match_patterns].
 * 
 * Returns: (nullable) (array zero-terminated=1) (transfer full): A %NULL-terminated list of match patterns that have
 * been granted to the extension and have not expired, or %NULL otherwise.
 * 
 * Since: 2.54
 */
WebKitWebExtensionMatchPattern** webkit_web_extension_context_get_current_permission_match_patterns(WebKitWebExtensionContext* context)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION_CONTEXT(context), nullptr);
    g_return_val_if_fail(context->priv->extension, nullptr);

    WebKitWebExtensionContextPrivate* priv = context->priv;
    auto matchPatterns = priv->context->currentPermissionMatchPatterns();
    if (matchPatterns.isEmpty())
        return nullptr;

    GPtrArray* currentPermissionMatchPatterns = g_ptr_array_new_full(matchPatterns.size(), g_free);
    for (Ref pattern : matchPatterns)
        g_ptr_array_add(currentPermissionMatchPatterns, webkitWebExtensionMatchPatternCreate(pattern));
    g_ptr_array_add(currentPermissionMatchPatterns, nullptr);

    return reinterpret_cast<WebKitWebExtensionMatchPattern**>(g_ptr_array_free(currentPermissionMatchPatterns, FALSE));
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
 * Since: 2.54
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
 * Since: 2.54
 */
gboolean webkit_web_extension_context_has_access_to_uri(WebKitWebExtensionContext* context, const gchar* uri)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION_CONTEXT(context), FALSE);
    g_return_val_if_fail(context->priv->extension, FALSE);
    g_return_val_if_fail(uri, FALSE);

    WebKitWebExtensionContextPrivate* priv = context->priv;
    return priv->context->hasPermission(URL { String::fromUTF8(uri) }, nullptr);
}

/**
 * webkit_web_extension_context_get_has_access_to_all_uris:
 * @context: a [class@WebExtensionContext]
 *
 * Get whether the currently granted permission match patterns set contains the `<all_urls>` pattern.
 * 
 * This does not check for any `*` host patterns. In most cases you should use the broader
 * webkit_web_extension_context_get_has_access_to_all_hosts().
 * 
 * Returns: %TRUE if the `<all_urls>` pattern is present.
 * 
 * Since: 2.54
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
 * Since: 2.54
 */
gboolean webkit_web_extension_context_get_has_access_to_all_hosts(WebKitWebExtensionContext* context)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION_CONTEXT(context), FALSE);
    g_return_val_if_fail(context->priv->extension, FALSE);

    WebKitWebExtensionContextPrivate* priv = context->priv;
    return priv->context->hasAccessToAllHosts();
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
 * Since: 2.54
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
 * Since: 2.54
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
 * webkit_web_extension_context_get_has_content_modification_rules:
 * @context: a [class@WebExtensionContext]
 *
 * Get whether the extension includes rules used for content modification or blocking.
 * 
 * This includes both static rules available in the extension's manifest and dynamic rules applied during a browsing session.
 * 
 * Returns: %TRUE if the extension includes rules used for content modification or blocking.
 * 
 * Since: 2.54
 */
gboolean webkit_web_extension_context_get_has_content_modification_rules(WebKitWebExtensionContext* context)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION_CONTEXT(context), FALSE);
    g_return_val_if_fail(context->priv->extension, FALSE);

    WebKitWebExtensionContextPrivate* priv = context->priv;
    return priv->context->hasContentModificationRules();
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
 * webkit_web_extension_context_permission_status_for_permission:
 * @context: a [class@WebExtensionContext]
 * @permission: The permission for which to return the status.
 *
 * Checks the specified permission against the currently denied, granted, and requested permissions.
 * 
 * Returns: the status of the requested permission
 * 
 * Since: 2.54
 */
WebKitWebExtensionContextPermissionStatus webkit_web_extension_context_permission_status_for_permission(WebKitWebExtensionContext* context, const gchar* permission)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION_CONTEXT(context), WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_UNKNOWN);
    g_return_val_if_fail(context->priv->extension, WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_UNKNOWN);
    g_return_val_if_fail(permission, WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_UNKNOWN);

    WebKitWebExtensionContextPrivate* priv = context->priv;
    return toAPI(priv->context->permissionState(String::fromUTF8(permission), nullptr));
}

/**
 * webkit_web_extension_context_permission_status_for_uri:
 * @context: a [class@WebExtensionContext]
 * @uri: The URI for which to return the status.
 *
 * Checks the specified URL against the currently denied, granted, and requested permission match patterns.
 * 
 * Returns: the permission status of the requested URI
 * 
 * Since: 2.54
 */
WebKitWebExtensionContextPermissionStatus webkit_web_extension_context_permission_status_for_uri(WebKitWebExtensionContext* context, const gchar* uri)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION_CONTEXT(context), WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_UNKNOWN);
    g_return_val_if_fail(context->priv->extension, WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_UNKNOWN);
    g_return_val_if_fail(uri, WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_UNKNOWN);

    WebKitWebExtensionContextPrivate* priv = context->priv;
    return toAPI(priv->context->permissionState(URL { String::fromUTF8(uri) }, nullptr));
}

/**
 * webkit_web_extension_context_permission_status_for_match_pattern:
 * @context: a [class@WebExtensionContext]
 * @pattern: The pattern for which to return the status.
 *
 * Checks the specified match pattern against the currently denied, granted, and requested permission match patterns.
 * 
 * Returns: the permission status of the requested match pattern
 * 
 * Since: 2.54
 */
WebKitWebExtensionContextPermissionStatus webkit_web_extension_context_permission_status_for_match_pattern(WebKitWebExtensionContext* context, WebKitWebExtensionMatchPattern* pattern)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION_CONTEXT(context), WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_UNKNOWN);
    g_return_val_if_fail(context->priv->extension, WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_UNKNOWN);
    g_return_val_if_fail(pattern, WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_UNKNOWN);

    WebKitWebExtensionContextPrivate* priv = context->priv;
    return toAPI(priv->context->permissionState(*webkitWebExtensionMatchPatternToImpl(pattern), nullptr));
}

/**
 * webkit_web_extension_context_set_permission_status_for_permission:
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
 * Since: 2.54
 */
void webkit_web_extension_context_set_permission_status_for_permission(WebKitWebExtensionContext* context, const gchar* permission, WebKitWebExtensionContextPermissionStatus status, GDateTime *expirationDate)
{
    g_return_if_fail(WEBKIT_IS_WEB_EXTENSION_CONTEXT(context));
    g_return_if_fail(context->priv->extension);
    g_return_if_fail(permission);
    g_return_if_fail(status == WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_DENIED_EXPLICITLY || status == WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_UNKNOWN || status == WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_GRANTED_EXPLICITLY);

    WebKitWebExtensionContextPrivate* priv = context->priv;

    if (expirationDate)
        priv->context->setPermissionState(toImpl(status), String::fromUTF8(permission), WallTime::fromRawSeconds(g_date_time_to_unix(expirationDate)));
    else
        priv->context->setPermissionState(toImpl(status), String::fromUTF8(permission));
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
 * Since: 2.54
 */
void webkit_web_extension_context_set_permission_status_for_uri(WebKitWebExtensionContext* context, const gchar* uri, WebKitWebExtensionContextPermissionStatus status, GDateTime* expirationDate)
{
    g_return_if_fail(WEBKIT_IS_WEB_EXTENSION_CONTEXT(context));
    g_return_if_fail(context->priv->extension);
    g_return_if_fail(uri);
    g_return_if_fail(status == WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_DENIED_EXPLICITLY || status == WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_UNKNOWN || status == WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_GRANTED_EXPLICITLY);

    WebKitWebExtensionContextPrivate* priv = context->priv;
    if (expirationDate)
        priv->context->setPermissionState(toImpl(status), URL { String::fromUTF8(uri) }, WallTime::fromRawSeconds(g_date_time_to_unix(expirationDate)));
    else
        priv->context->setPermissionState(toImpl(status), URL { String::fromUTF8(uri) });
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
 * Since: 2.54
 */
void webkit_web_extension_context_set_permission_status_for_match_pattern(WebKitWebExtensionContext* context, WebKitWebExtensionMatchPattern* pattern, WebKitWebExtensionContextPermissionStatus status, GDateTime* expirationDate)
{
    g_return_if_fail(WEBKIT_IS_WEB_EXTENSION_CONTEXT(context));
    g_return_if_fail(context->priv->extension);
    g_return_if_fail(pattern);
    g_return_if_fail(status == WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_DENIED_EXPLICITLY || status == WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_UNKNOWN || status == WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_GRANTED_EXPLICITLY);

    WebKitWebExtensionContextPrivate* priv = context->priv;
    if (expirationDate)
        priv->context->setPermissionState(toImpl(status), *webkitWebExtensionMatchPatternToImpl(pattern), WallTime::fromRawSeconds(g_date_time_to_unix(expirationDate)));
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
 * Since: 2.54
 */
void webkit_web_extension_context_load_background_content(WebKitWebExtensionContext* context, GCancellable* cancellable, GAsyncReadyCallback callback, gpointer userData)
{
    g_return_if_fail(WEBKIT_IS_WEB_EXTENSION_CONTEXT(context));
    g_return_if_fail(context->priv->extension);

    GRefPtr<GTask> task = adoptGRef(g_task_new(context, cancellable, callback, userData));
    if (!context->priv->context->isLoaded()) {
        auto errorStr = "Extension context is not loaded";
        g_task_return_new_error(task.get(), webkit_web_extension_context_error_quark(),
            WEBKIT_WEB_EXTENSION_CONTEXT_ERROR_NOT_LOADED, errorStr, nullptr);
        return;
    }

    context->priv->context->loadBackgroundContent([task = WTF::move(task)](RefPtr<API::Error> error) {
        if (error) {
            g_task_return_new_error(task.get(), webkit_web_extension_context_error_quark(),
                toWebKitWebExtensionContextError(error->errorCode()), error->localizedDescription().utf8().data(), nullptr);
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
 * Finish an asynchronous operation started with [method@WebExtensionContext.load_background_content_finish].
 * 
 * An error will occur if the extension does not have any background content to load or loading fails.
 * 
 * Returns: %TRUE if the background content was loaded or %FALSE in case of error.
 * 
 * Since: 2.54
 */
gboolean webkit_web_extension_context_load_background_content_finish(WebKitWebExtensionContext* context, GAsyncResult* result, GError** error)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION_CONTEXT(context), FALSE);
    g_return_val_if_fail(context->priv->extension, FALSE);
    g_return_val_if_fail(g_task_is_valid(result, context), FALSE);

    return g_task_propagate_boolean(G_TASK(result), error);
}

#else // ENABLE(WK_WEB_EXTENSIONS)

WebKitWebExtensionContext* webkit_web_extension_context_new_for_extension(WebKitWebExtension* extension, GError** error)
{
    return nullptr;
}

void webkitWebExtensionContextSetWebExtension(WebKitWebExtensionContext* context, WebKitWebExtension* extension)
{
    return;
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

const gchar* webkit_web_extension_context_get_unique_identifier(WebKitWebExtensionContext* context)
{
    return "";
}

void webkit_web_extension_context_set_unique_identifier(WebKitWebExtensionContext* context, const gchar* uniqueIentifier)
{
    return;
}

gboolean webkit_web_extension_context_get_is_inspectable(WebKitWebExtensionContext* context)
{
    return 0;
}

void webkit_web_extension_context_set_is_inspectable(WebKitWebExtensionContext* context, gboolean isInspectable)
{
    return;
}

const gchar* webkit_web_extension_context_get_inspection_name(WebKitWebExtensionContext* context)
{
    return "";
}

void webkit_web_extension_context_set_inspection_name(WebKitWebExtensionContext* context, const gchar* inspectionName)
{
    return;
}

const gchar* const * webkit_web_extension_context_get_unsupported_apis(WebKitWebExtensionContext* context)
{
    return nullptr;
}

void webkit_web_extension_context_set_unsupported_apis(WebKitWebExtensionContext* context, const gchar* const* unsupportedApis)
{
    return;
}

const gchar* webkit_web_extension_context_get_options_page_uri(WebKitWebExtensionContext* context)
{
    return "";
}

const gchar* webkit_web_extension_context_get_override_new_tab_page_uri(WebKitWebExtensionContext* context)
{
    return "";
}

WebKitWebExtensionContextPermission** webkit_web_extension_context_get_granted_permissions(WebKitWebExtensionContext* context)
{
    return nullptr;
}

WebKitWebExtensionContextMatchPattern** webkit_web_extension_context_get_granted_permission_match_patterns(WebKitWebExtensionContext* context)
{
    return nullptr;
}

WebKitWebExtensionContextPermission** webkit_web_extension_context_get_denied_permissions(WebKitWebExtensionContext* context)
{
    return nullptr;
}

WebKitWebExtensionContextMatchPattern** webkit_web_extension_context_get_denied_permission_match_patterns(WebKitWebExtensionContext* context)
{
    return nullptr;
}

gboolean webkit_web_extension_context_get_has_requested_optional_access_to_all_hosts(WebKitWebExtensionContext* context)
{
    return 0;
}

void webkit_web_extension_context_set_has_requested_optional_access_to_all_hosts(WebKitWebExtensionContext* context, gboolean requestedOptionalAccessToAllHosts)
{
    return;
}

gboolean webkit_web_extension_context_get_has_access_to_private_data(WebKitWebExtensionContext* context)
{
    return 0;
}

void webkit_web_extension_context_set_has_access_to_private_data(WebKitWebExtensionContext* context, gboolean hasAccessToPrivateData)
{
    return;
}

const gchar* const * webkit_web_extension_context_get_current_permissions(WebKitWebExtensionContext* context)
{
    return nullptr;
}

WebKitWebExtensionMatchPattern** webkit_web_extension_context_get_current_permission_match_patterns(WebKitWebExtensionContext* context)
{
    return nullptr;
}

gboolean webkit_web_extension_context_has_permission(WebKitWebExtensionContext* context, const gchar* permission)
{
    return 0;
}

gboolean webkit_web_extension_context_has_access_to_uri(WebKitWebExtensionContext* context, const gchar* uri)
{
    return 0;
}

gboolean webkit_web_extension_context_get_has_access_to_all_uris(WebKitWebExtensionContext* context)
{
    return 0;
}

gboolean webkit_web_extension_context_get_has_access_to_all_hosts(WebKitWebExtensionContext* context)
{
    return 0;
}

gboolean webkit_web_extension_context_get_has_injected_content(WebKitWebExtensionContext* context)
{
    return 0;
}

gboolean webkit_web_extension_context_has_injected_content_for_uri(WebKitWebExtensionContext* context, const gchar* uri)
{
    return 0;
}

gboolean webkit_web_extension_context_get_has_content_modification_rules(WebKitWebExtensionContext* context)
{
    return 0;
}

WebKitWebExtensionContextPermissionStatus webkit_web_extension_context_permission_status_for_permission(WebKitWebExtensionContext* context, const gchar* permission)
{
    return WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_UNKNOWN;
}

WebKitWebExtensionContextPermissionStatus webkit_web_extension_context_permission_status_for_uri(WebKitWebExtensionContext* context, const gchar* uri)
{
    return WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_UNKNOWN;
}

WebKitWebExtensionContextPermissionStatus webkit_web_extension_context_permission_status_for_match_pattern(WebKitWebExtensionContext* context, WebKitWebExtensionMatchPattern* matchPattern)
{
    return WEBKIT_WEB_EXTENSION_CONTEXT_PERMISSION_STATUS_UNKNOWN;
}

void webkit_web_extension_context_set_permission_status_for_permission(WebKitWebExtensionContext* context, const gchar* permission, WebKitWebExtensionContextPermissionStatus permissionStatus, GDateTime* expirationDate)
{
    return;
}

void webkit_web_extension_context_set_permission_status_for_uri(WebKitWebExtensionContext* context, const gchar* uri, WebKitWebExtensionContextPermissionStatus permissionStatus, GDateTime* expirationDate)
{
    return;
}

void webkit_web_extension_context_set_permission_status_for_match_pattern(WebKitWebExtensionContext* context, WebKitWebExtensionMatchPattern* matchPattern, WebKitWebExtensionContextPermissionStatus permissionStatus, GDateTime* expirationDate)
{
    return;
}

void webkit_web_extension_context_load_background_content(WebKitWebExtensionContext* context, GCancellable* cancellable, GAsyncReadyCallback callback, gpointer userData)
{
    return;
}

gboolean webkit_web_extension_context_load_background_content_finish(WebKitWebExtensionContext* context, GAsyncResult* result, GError** error)
{
    return 0;
}

#endif // ENABLE(WK_WEB_EXTENSIONS)

#endif // ENABLE(2022_GLIB_API)
