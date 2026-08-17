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
#include "WebKitWebExtensionPrivate.h"
#include <WebCore/platform/LegacySchemeRegistry.h>
#include <wtf/URLParser.h>
#include <wtf/glib/GWeakPtr.h>
#include <wtf/glib/WTFGType.h>

#if ENABLE(WK_WEB_EXTENSIONS)
constexpr auto WEBKIT_CONTEXT_ERROR_DOMAIN = "WKWebExtensionContextErrorDomain"_s;
#endif

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
    CString baseURI;
    CString optionsPageURI;
    CString overrideNewTabPageURI;
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
    N_PROPERTIES,
};

static std::array<GParamSpec*, N_PROPERTIES> properties;

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

    g_object_class_install_properties(objectClass, properties.size(), properties.data());
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
                    toWebKitWebExtensionContextError(internalError->errorCode()), internalError->localizedDescription().utf8().data());
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
        return priv->optionsPageURI.data();

    auto optionsPageURI = priv->context->optionsPageURL();
    if (optionsPageURI.isEmpty())
        return nullptr;

    priv->optionsPageURI = optionsPageURI.string().utf8();
    return priv->optionsPageURI.data();
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
        return priv->overrideNewTabPageURI.data();

    auto overrideNewTabPageURI = priv->context->overrideNewTabPageURL();
    if (overrideNewTabPageURI.isEmpty())
        return nullptr;

    priv->overrideNewTabPageURI = overrideNewTabPageURI.string().utf8();
    return priv->overrideNewTabPageURI.data();
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

#endif // ENABLE(WK_WEB_EXTENSIONS)

#endif // ENABLE(2022_GLIB_API)
