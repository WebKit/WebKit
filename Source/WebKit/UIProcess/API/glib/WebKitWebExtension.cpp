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

#if ENABLE(2022_GLIB_API)

#include "config.h"
#include "WebKitWebExtension.h"

#include "WebExtension.h"
#include "WebKitError.h"
#include "WebKitPrivate.h"
#include "WebKitWebExtensionInternal.h"
#include "WebKitWebExtensionMatchPatternPrivate.h"
#include <WebCore/Icon.h>
#include <wtf/RefPtr.h>
#include <wtf/glib/WTFGType.h>
#include <wtf/text/CString.h>

using namespace WebKit;

/**
 * WebKitWebExtension:
 *
 * Represents a [WebExtension](https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions).
 * 
 * A #WebKitWebExtension object encapsulates a web extension’s
 * resources that are defined by a [`manifest.json` file](https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/manifest.json).
 * 
 * This class handles the reading and parsing of the manifest file
 * along with the supporting resources like icons and localizations.
 *
 * Since: 2.52
 */

static void gInitableInterfaceInit(GInitableIface*);

struct _WebKitWebExtensionPrivate {
#if ENABLE(WK_WEB_EXTENSIONS)
    GRefPtr<GFile> path;
    RefPtr<WebExtension> extension;
    CString defaultLocale;
    CString displayName;
    CString displayShortName;
    CString displayVersion;
    CString displayDescription;
    CString displayActionLabel;
    CString version;
    GRefPtr<GPtrArray> requestedPermissions;
    GRefPtr<GPtrArray> optionalPermissions;
#endif
};

WEBKIT_DEFINE_FINAL_TYPE_WITH_CODE(
    WebKitWebExtension, webkit_web_extension, G_TYPE_OBJECT, GObject,
    G_IMPLEMENT_INTERFACE(G_TYPE_INITABLE, gInitableInterfaceInit))

static void webkitWebExtensionSetPath(WebKitWebExtension*, const char*);

enum {
    PROP_0,
    PROP_PATH,
    PROP_MANIFEST_VERSION,
    PROP_DEFAULT_LOCALE,
    PROP_DISPLAY_NAME,
    PROP_DISPLAY_SHORT_NAME,
    PROP_DISPLAY_VERSION,
    PROP_DISPLAY_DESCRIPTION,
    PROP_DISPLAY_ACTION_LABEL,
    PROP_VERSION,
    PROP_REQUESTED_PERMISSIONS,
    PROP_OPTIONAL_PERMISSIONS,
    PROP_HAS_BACKGROUND_CONTENT,
    PROP_HAS_PERSISTENT_BACKGROUND_CONTENT,
    PROP_HAS_INJECTED_CONTENT,
    PROP_HAS_OPTIONS_PAGE,
    PROP_HAS_OVERRIDE_NEW_TAB_PAGE,
    PROP_HAS_COMMANDS,
    PROP_HAS_CONTENT_MODIFICATION_RULES,
    N_PROPERTIES,
};

static std::array<GParamSpec*, N_PROPERTIES> properties;

static void webkitWebExtensionGetProperty(GObject* object, guint propId, GValue* value, GParamSpec* paramSpec)
{
    WebKitWebExtension* extension = WEBKIT_WEB_EXTENSION(object);

    switch (propId) {
    case PROP_PATH:
        g_value_set_string(value, webkit_web_extension_get_path(extension));
        break;
    case PROP_MANIFEST_VERSION:
        g_value_set_double(value, webkit_web_extension_get_manifest_version(extension));
        break;
    case PROP_DEFAULT_LOCALE:
        g_value_set_string(value, webkit_web_extension_get_default_locale(extension));
        break;
    case PROP_DISPLAY_NAME:
        g_value_set_string(value, webkit_web_extension_get_display_name(extension));
        break;
    case PROP_DISPLAY_SHORT_NAME:
        g_value_set_string(value, webkit_web_extension_get_display_short_name(extension));
        break;
    case PROP_DISPLAY_VERSION:
        g_value_set_string(value, webkit_web_extension_get_display_version(extension));
        break;
    case PROP_DISPLAY_DESCRIPTION:
        g_value_set_string(value, webkit_web_extension_get_display_description(extension));
        break;
    case PROP_DISPLAY_ACTION_LABEL:
        g_value_set_string(value, webkit_web_extension_get_display_action_label(extension));
        break;
    case PROP_VERSION:
        g_value_set_string(value, webkit_web_extension_get_version(extension));
        break;
    case PROP_REQUESTED_PERMISSIONS:
        g_value_set_boxed(value, webkit_web_extension_get_requested_permissions(extension));
        break;
    case PROP_OPTIONAL_PERMISSIONS:
        g_value_set_boxed(value, webkit_web_extension_get_optional_permissions(extension));
        break;
    case PROP_HAS_BACKGROUND_CONTENT:
        g_value_set_boolean(value, webkit_web_extension_get_has_background_content(extension));
        break;
    case PROP_HAS_PERSISTENT_BACKGROUND_CONTENT:
        g_value_set_boolean(value, webkit_web_extension_get_has_persistent_background_content(extension));
        break;
    case PROP_HAS_INJECTED_CONTENT:
        g_value_set_boolean(value, webkit_web_extension_get_has_injected_content(extension));
        break;
    case PROP_HAS_OPTIONS_PAGE:
        g_value_set_boolean(value, webkit_web_extension_get_has_options_page(extension));
        break;
    case PROP_HAS_OVERRIDE_NEW_TAB_PAGE:
        g_value_set_boolean(value, webkit_web_extension_get_has_override_new_tab_page(extension));
        break;
    case PROP_HAS_COMMANDS:
        g_value_set_boolean(value, webkit_web_extension_get_has_commands(extension));
        break;
    case PROP_HAS_CONTENT_MODIFICATION_RULES:
        g_value_set_boolean(value, webkit_web_extension_get_has_content_modification_rules(extension));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, paramSpec);
    }
}

static void webkitWebExtensionSetProperty(GObject* object, guint propId, const GValue* value, GParamSpec* paramSpec)
{
    WebKitWebExtension* extension = WEBKIT_WEB_EXTENSION(object);

    switch (propId) {
    case PROP_PATH:
        webkitWebExtensionSetPath(extension, g_value_get_string(value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, paramSpec);
    }
}

static void webkit_web_extension_class_init(WebKitWebExtensionClass* klass)
{
    GObjectClass* objectClass = G_OBJECT_CLASS(klass);
    objectClass->get_property = webkitWebExtensionGetProperty;
    objectClass->set_property = webkitWebExtensionSetProperty;

    /**
     * WebKitWebExtension:path:
     * 
     * A string pointing to the folder containing the extension manifest and resources.
     * See webkit_web_extension_get_path() for more details.
     *
     * Since: 2.52
     */
    properties[PROP_PATH] =
        g_param_spec_string(
            "path",
            nullptr, nullptr,
            nullptr,
            static_cast<GParamFlags>(WEBKIT_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

    /**
     * WebKitWebExtension:manifest-version:
     * 
     * The parsed manifest version of the #WebKitWebExtension.
     * See webkit_web_extension_get_manifest_version() for more details.
     *
     * Since: 2.52
     */
    properties[PROP_MANIFEST_VERSION] =
        g_param_spec_string(
            "manifest-version",
            nullptr, nullptr,
            nullptr,
            WEBKIT_PARAM_READABLE);

    /**
     * WebKitWebExtension:default-locale:
     * 
     * The default locale for the #WebKitWebExtension.
     * See webkit_web_extension_get_default_locale() for more details.
     *
     * Since: 2.52
     */
    properties[PROP_DEFAULT_LOCALE] =
        g_param_spec_string(
            "default-locale",
            nullptr, nullptr,
            nullptr,
            WEBKIT_PARAM_READABLE);

    /**
     * WebKitWebExtension:display-name:
     * 
     * The localized name of the #WebKitWebExtension.
     * See webkit_web_extension_get_display_name() for more details.
     * 
     * Since: 2.52
     */
    properties[PROP_DISPLAY_NAME] =
        g_param_spec_string(
            "display-name",
            nullptr, nullptr,
            nullptr,
            WEBKIT_PARAM_READABLE);

    /**
     * WebKitWebExtension:display-short-name:
     * 
     * The localized short name of the #WebKitWebExtension.
     * See webkit_web_extension_get_display_short_name() for more details.
     * 
     * Since: 2.52
     */
    properties[PROP_DISPLAY_SHORT_NAME] =
        g_param_spec_string(
            "display-short-name",
            nullptr, nullptr,
            nullptr,
            WEBKIT_PARAM_READABLE);

    /**
     * WebKitWebExtension:display-version:
     * 
     * The localized display version of the #WebKitWebExtension.
     * See webkit_web_extension_get_display_version() for more details.
     * 
     * Since: 2.52
     */
    properties[PROP_DISPLAY_VERSION] =
        g_param_spec_string(
            "display-version",
            nullptr, nullptr,
            nullptr,
            WEBKIT_PARAM_READABLE);

    /**
     * WebKitWebExtension:display-description:
     * 
     * The localized description of the #WebKitWebExtension.
     * See webkit_web_extension_get_display_description() for more details.
     * 
     * Since: 2.52
     */
    properties[PROP_DISPLAY_DESCRIPTION] =
        g_param_spec_string(
            "display-description",
            nullptr, nullptr,
            nullptr,
            WEBKIT_PARAM_READABLE);

    /**
     * WebKitWebExtension:display-action-label:
     * 
     * The localized extension action label of the #WebKitWebExtension.
     * See webkit_web_extension_get_display_action_label() for more details.
     * 
     * Since: 2.52
     */
    properties[PROP_DISPLAY_ACTION_LABEL] =
        g_param_spec_string(
            "display-action-label",
            nullptr, nullptr,
            nullptr,
            WEBKIT_PARAM_READABLE);

    /**
     * WebKitWebExtension:version:
     * 
     * The version of the #WebKitWebExtension.
     * See webkit_web_extension_get_version() for more details.
     * 
     * Since: 2.52
     */
    properties[PROP_VERSION] =
        g_param_spec_string(
            "version",
            nullptr, nullptr,
            nullptr,
            WEBKIT_PARAM_READABLE);

    /**
     * WebKitWebExtension:requested-permissions:
     * 
     * The set of permissions that the #WebKitWebExtension requires for its base functionality.
     * See webkit_web_extension_get_requested_permissions() for more details.
     * 
     * Since: 2.52
     */
    properties[PROP_REQUESTED_PERMISSIONS] =
        g_param_spec_boxed(
            "requested-permissions",
            nullptr, nullptr,
            G_TYPE_STRV,
            WEBKIT_PARAM_READABLE);

    /**
     * WebKitWebExtension:optional-permissions:
     * 
     * The set of permissions that the #WebKitWebExtension may need for optional functionality.
     * See webkit_web_extension_get_optional_permissions() for more details.
     * 
     * Since: 2.52
     */
    properties[PROP_OPTIONAL_PERMISSIONS] =
        g_param_spec_boxed(
            "optional-permissions",
            nullptr, nullptr,
            G_TYPE_STRV,
            WEBKIT_PARAM_READABLE);

    /**
     * WebKitWebExtension:has-background-content:
     * 
     * Whether the #WebKitWebExtension has background content that can run when needed.
     * See webkit_web_extension_get_has_background_content() for more details.
     * 
     * Since: 2.52
     */
    properties[PROP_HAS_BACKGROUND_CONTENT] =
        g_param_spec_boolean(
            "has-background-content",
            nullptr, nullptr,
            FALSE,
            WEBKIT_PARAM_READABLE);

    /**
     * WebKitWebExtension:has-persistent-background-content:
     * 
     * Whether the #WebKitWebExtension has background content that stays in memory as long as the extension is loaded.
     * See webkit_web_extension_get_has_persistent_background_content() for more details.
     * 
     * Since: 2.52
     */
    properties[PROP_HAS_PERSISTENT_BACKGROUND_CONTENT] =
        g_param_spec_boolean(
            "has-persistent-background-content",
            nullptr, nullptr,
            FALSE,
            WEBKIT_PARAM_READABLE);

    /**
     * WebKitWebExtension:has-injected-content:
     * 
     * Whether the #WebKitWebExtension has script or stylesheet content that can be injected into webpages.
     * See webkit_web_extension_get_has_injected_content() for more details.
     * 
     * Since: 2.52
     */
    properties[PROP_HAS_INJECTED_CONTENT] =
        g_param_spec_boolean(
            "has-injected-content",
            nullptr, nullptr,
            FALSE,
            WEBKIT_PARAM_READABLE);

    /**
     * WebKitWebExtension:has-options-page:
     * 
     * Whether the #WebKitWebExtension has an options page.
     * See webkit_web_extension_get_has_options_page() for more details.
     * 
     * Since: 2.52
     */
    properties[PROP_HAS_OPTIONS_PAGE] =
        g_param_spec_boolean(
            "has-options-page",
            nullptr, nullptr,
            FALSE,
            WEBKIT_PARAM_READABLE);

    /**
     * WebKitWebExtension:has-override-new-tab-page:
     * 
     * Whether the #WebKitWebExtension provides an alternative to the default new tab page.
     * See webkit_web_extension_get_has_override_new_tab_page() for more details.
     * 
     * Since: 2.52
     */
    properties[PROP_HAS_OVERRIDE_NEW_TAB_PAGE] =
        g_param_spec_boolean(
            "has-override-new-tab-page",
            nullptr, nullptr,
            FALSE,
            WEBKIT_PARAM_READABLE);

    /**
     * WebKitWebExtension:has-commands:
     * 
     * Whether the #WebKitWebExtension includes commands that users can invoke.
     * See webkit_web_extension_get_has_commands() for more details.
     * 
     * Since: 2.52
     */
    properties[PROP_HAS_COMMANDS] =
        g_param_spec_boolean(
            "has-commands",
            nullptr, nullptr,
            FALSE,
            WEBKIT_PARAM_READABLE);

    /**
     * WebKitWebExtension:has-content-modification-rules:
     * 
     * Whether the #WebKitWebExtension includes rules used for content modification or blocking.
     * See webkit_web_extension_get_content_modification_rules() for more details.
     * 
     * Since: 2.52
     */
    properties[PROP_HAS_CONTENT_MODIFICATION_RULES] =
        g_param_spec_boolean(
            "has-content-modification-rules",
            nullptr, nullptr,
            FALSE,
            WEBKIT_PARAM_READABLE);

    g_object_class_install_properties(objectClass, properties.size(), properties.data());
}

static gboolean webkitWebExtensionInitableInit(GInitable* initable, GCancellable* cancellable, GError** error)
{
#if ENABLE(WK_WEB_EXTENSIONS)
    WebKitWebExtension* self = WEBKIT_WEB_EXTENSION(initable);
    GRefPtr<GFile> extensionPath = self->priv->path;
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION(self), FALSE);
    g_return_val_if_fail(extensionPath && G_IS_FILE(extensionPath.get()), FALSE);
    g_return_val_if_fail(g_file_query_file_type(extensionPath.get(), G_FILE_QUERY_INFO_NONE, nullptr) == G_FILE_TYPE_DIRECTORY, FALSE);

    RefPtr<API::Error> internalError;
    Ref extension = WebKit::WebExtension::create(extensionPath.get(), internalError);
    if (internalError) {
        g_set_error(error, webkit_web_extension_error_quark(),
            toWebKitWebExtensionError(internalError->errorCode()), internalError->localizedDescription().utf8().data(), nullptr);
        return FALSE;
    }

    self->priv->extension = WTFMove(extension);

    return TRUE;
#else
    return FALSE;
#endif
}

static void gInitableInterfaceInit(GInitableIface* iface)
{
    iface->init = webkitWebExtensionInitableInit;
}

#if ENABLE(WK_WEB_EXTENSIONS)

WebKitWebExtension* webkitWebExtensionCreate(HashMap<String, GRefPtr<GBytes>>&& resources, GError** error)
{
    WebExtension::Resources newResources;
    for (auto& it : resources)
        newResources.add(it.key, API::Data::createWithoutCopying(WTFMove(it.value)));

    Ref extension = WebKit::WebExtension::create(WTFMove(newResources));

    if (!extension->errors().isEmpty()) {
        Ref internalError = extension->errors().last();
        g_set_error(error, webkit_web_extension_error_quark(),
            toWebKitWebExtensionError(internalError->errorCode()), internalError->localizedDescription().utf8().data(), nullptr);
    }

    WebKitWebExtension* object = WEBKIT_WEB_EXTENSION(g_object_new(WEBKIT_TYPE_WEB_EXTENSION, nullptr));
    object->priv->extension = WTFMove(extension);
    return object;
}

/**
 * webkit_web_extension_new:
 * @extension_path: (transfer none): A string pointing to the folder containing the extension manifest and resources
 * @error: return location for error or %NULL to ignore
 * 
 * Creates a new WebKitWebExtension from a folder containing the extension contents. The folder must
 * contain a `manifest.json` file. If the manifest is invalid or missing, an error will be returned.
 * 
 * Returns: (nullable): the new #WebKitWebExtension, or %NULL if the extension failed to be created
 * 
 * Since: 2.52
 */
WebKitWebExtension* webkit_web_extension_new(const char *extensionPath, GError **error)
{
    return WEBKIT_WEB_EXTENSION(g_initable_new (WEBKIT_TYPE_WEB_EXTENSION, nullptr, error, "path", extensionPath, nullptr));
}

void webkitWebExtensionSetPath(WebKitWebExtension* extension, const char* file)
{
    g_return_if_fail(WEBKIT_IS_WEB_EXTENSION(extension));

    if (file)
        extension->priv->path = adoptGRef(g_file_new_for_path(file));
}

/**
 * webkit_web_extension_get_path:
 * @extension: a #WebKitWebExtension
 *
 * Get the path pointing to the folder containing the extension manifest and resources
 * 
 * Returns: (transfer none): the path of the extension folder
 * 
 * Since: 2.52
 */
const char* webkit_web_extension_get_path(WebKitWebExtension* extension)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION(extension), 0);

    return g_file_peek_path(extension->priv->path.get());
}

/**
 * webkit_web_extension_get_manifest_version:
 * @extension: a #WebKitWebExtension
 *
 * Get the parsed manifest version, or `0` if there is no
 * version specified in the manifest.
 *
 * A [error@WebKit.WebExtensionError.UNSUPPORTED_MANIFEST_VERSION] error will be
 * reported if the manifest version isn't specified.
 * 
 * Returns: the parsed manifest version.
 * 
 * Since: 2.52
 */
gdouble webkit_web_extension_get_manifest_version(WebKitWebExtension* extension)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION(extension), 0);

    return extension->priv->extension->manifestVersion();
}

/**
 * webkit_web_extension_supports_manifest_version:
 * @extension: a #WebKitWebExtension
 * @manifest_version: the version number to check
 * 
 * Checks if a manifest version is supported by the extension.
 * 
 * Returns: `TRUE` if the extension specified a manifest version
 * that is greater than or equal to @manifest_version.
 * 
 * Since: 2.52
 */
gboolean webkit_web_extension_supports_manifest_version(WebKitWebExtension* extension, gdouble manifestVersion)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION(extension), FALSE);

    return extension->priv->extension->supportsManifestVersion(manifestVersion);
}

/**
 * webkit_web_extension_get_default_locale:
 * @extension: a #WebKitWebExtension
 *
 * Get the default locale for the extension.
 * 
 * Returns: (nullable): the default locale, or %NULL if there was no default
 * locale specified.
 * 
 * Since: 2.52
 */
const gchar* webkit_web_extension_get_default_locale(WebKitWebExtension* extension)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION(extension), nullptr);

    WebKitWebExtensionPrivate* priv = extension->priv;
    if (!priv->defaultLocale.isNull())
        return priv->defaultLocale.data();

    auto defaultLocale = priv->extension->defaultLocale();
    if (defaultLocale.isEmpty())
        return nullptr;

    priv->defaultLocale = defaultLocale.utf8();
    return priv->defaultLocale.data();
}

/**
 * webkit_web_extension_get_display_name:
 * @extension: a #WebKitWebExtension
 *
 * Get the localized name for the extension.
 * 
 * Returns: (nullable): the localized name, or %NULL if there was no
 * name specified.
 * 
 * Since: 2.52
 */
const gchar* webkit_web_extension_get_display_name(WebKitWebExtension* extension)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION(extension), nullptr);

    WebKitWebExtensionPrivate* priv = extension->priv;
    if (!priv->displayName.isNull())
        return priv->displayName.data();

    auto displayName = priv->extension->displayName();
    if (displayName.isEmpty())
        return nullptr;

    priv->displayName = displayName.utf8();
    return priv->displayName.data();
}

/**
 * webkit_web_extension_get_display_short_name:
 * @extension: a #WebKitWebExtension
 *
 * Get the localized short name for the extension.
 * 
 * Returns: (nullable): the localized name, or %NULL if there was no
 * short name specified.
 * 
 * Since: 2.52
 */
const gchar* webkit_web_extension_get_display_short_name(WebKitWebExtension* extension)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION(extension), nullptr);

    WebKitWebExtensionPrivate* priv = extension->priv;
    if (!priv->displayShortName.isNull())
        return priv->displayShortName.data();

    auto displayShortName = priv->extension->displayShortName();
    if (displayShortName.isEmpty())
        return nullptr;

    priv->displayShortName = displayShortName.utf8();
    return priv->displayShortName.data();
}

/**
 * webkit_web_extension_get_display_version:
 * @extension: a #WebKitWebExtension
 *
 * Get the localized display version for the extension.
 * 
 * Returns: (nullable): the localized display version, or %NULL if there was no
 * display version specified.
 * 
 * Since: 2.52
 */
const gchar* webkit_web_extension_get_display_version(WebKitWebExtension* extension)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION(extension), nullptr);

    WebKitWebExtensionPrivate* priv = extension->priv;
    if (!priv->displayVersion.isNull())
        return priv->displayVersion.data();

    auto displayVersion = priv->extension->displayVersion();
    if (displayVersion.isEmpty())
        return nullptr;

    priv->displayVersion = displayVersion.utf8();
    return priv->displayVersion.data();
}

/**
 * webkit_web_extension_get_display_description:
 * @extension: a #WebKitWebExtension
 *
 * Get the localized display description for the extension.
 * 
 * Returns: (nullable): the localized display description, or %NULL if there
 * was no display description specified.
 * 
 * Since: 2.52
 */
const gchar* webkit_web_extension_get_display_description(WebKitWebExtension* extension)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION(extension), nullptr);

    WebKitWebExtensionPrivate* priv = extension->priv;
    if (!priv->displayDescription.isNull())
        return priv->displayDescription.data();

    auto displayDescription = priv->extension->displayDescription();
    if (displayDescription.isEmpty())
        return nullptr;

    priv->displayDescription = displayDescription.utf8();
    return priv->displayDescription.data();
}

/**
 * webkit_web_extension_get_display_action_label:
 * @extension: a #WebKitWebExtension
 *
 * Get the localized display action label for the extension.
 * 
 * This label serves as a default and should be used to represent the extension in contexts like action sheets or toolbars prior to 
 * the extension being loaded into an extension context.
 * Once the extension is loaded, use the ``actionForTab:`` API to get the tab-specific label.
 * 
 * Returns: (nullable): the localized display action label, or %NULL if there
 * was no display action label specified.
 * 
 * Since: 2.52
 */
const gchar* webkit_web_extension_get_display_action_label(WebKitWebExtension* extension)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION(extension), nullptr);

    WebKitWebExtensionPrivate* priv = extension->priv;
    if (!priv->displayActionLabel.isNull())
        return priv->displayActionLabel.data();

    auto displayActionLabel = priv->extension->displayActionLabel();
    if (displayActionLabel.isEmpty())
        return nullptr;

    priv->displayActionLabel = displayActionLabel.utf8();
    return priv->displayActionLabel.data();
}

/**
 * webkit_web_extension_get_icon:
 * @extension: a #WebKitWebExtension
 * @width: The width to use when looking up the icon.
 * @height: The height to use when looking up the icon.
 *
 * Returns the extension's icon image for the specified size.
 * This icon should represent the extension in settings or other areas that show the extension.
 * The returned image will be the best match for the specified size that is available in the extension's
 * icon set. If no matching icon can be found, the method will return %NULL.
 * 
 * Returns: (nullable) (transfer none): the icon image, or %NULL if no icon could be loaded.
 * 
 * Since: 2.52
 */
GIcon* webkit_web_extension_get_icon(WebKitWebExtension* extension, gdouble width, gdouble height)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION(extension), nullptr);

    WebKitWebExtensionPrivate* priv = extension->priv;
    auto icon = priv->extension->icon(WebCore::FloatSize(width, height));
    if (!icon)
        return nullptr;
    return icon->icon();
}

/**
 * webkit_web_extension_get_action_icon:
 * @extension: a #WebKitWebExtension
 * @width: The width to use when looking up the icon.
 * @height: The height to use when looking up the icon.
 *
 * Returns the extension's default action icon image for the specified size.
 * This icon serves as a default and should be used to represent the extension in contexts like action sheets or toolbars prior to 
 * the extension being loaded into an extension context. Once the extension is loaded, use the
 * ``actionForTab:`` API to get the tab-specific icon.
 * The returned image will be the best match for the specified size that is available in the extension's action icon set. If no matching icon is available,
 * the method will fall back to the extension's icon.
 * 
 * Returns: (nullable) (transfer none): the icon image, or %NULL if no icon could be loaded.
 * 
 * Since: 2.52
 */
GIcon* webkit_web_extension_get_action_icon(WebKitWebExtension* extension, gdouble width, gdouble height)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION(extension), nullptr);

    WebKitWebExtensionPrivate* priv = extension->priv;
    auto icon = priv->extension->actionIcon(WebCore::FloatSize(width, height));
    if (!icon)
        return nullptr;
    return icon->icon();
}

/**
 * webkit_web_extension_get_version:
 * @extension: a #WebKitWebExtension
 *
 * Get the version for the extension.
 * 
 * Returns: (nullable): the version, or %NULL if there was no
 * version specified.
 * 
 * Since: 2.52
 */
const gchar* webkit_web_extension_get_version(WebKitWebExtension* extension)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION(extension), nullptr);

    WebKitWebExtensionPrivate* priv = extension->priv;
    if (!priv->version.isNull())
        return priv->version.data();

    auto version = priv->extension->version();
    if (version.isEmpty())
        return nullptr;

    priv->version = version.utf8();
    return priv->version.data();
}

/**
 * webkit_web_extension_get_requested_permissions:
 * @extension: a #WebKitWebExtension
 *
 * Get the set of permissions that the extension requires
 * for its base functionality.
 * 
 * Returns: (nullable) (array zero-terminated=1) (transfer none): a
 * %NULL-terminated array of strings containing permission names,
 * or %NULL otherwise. This array and its contents are owned by
 * WebKit and should not be modified or freed.
 * 
 * Since: 2.52
 */
const gchar* const * webkit_web_extension_get_requested_permissions(WebKitWebExtension* extension)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION(extension), nullptr);

    WebKitWebExtensionPrivate* priv = extension->priv;
    if (priv->requestedPermissions)
        return reinterpret_cast<gchar**>(priv->requestedPermissions->pdata);

    auto requestedPermissions = priv->extension->requestedPermissions();
    if (!requestedPermissions.size())
        return nullptr;

    priv->requestedPermissions = adoptGRef(g_ptr_array_new_with_free_func(g_free));
    for (auto permission : requestedPermissions)
        g_ptr_array_add(priv->requestedPermissions.get(), g_strdup(permission.utf8().data()));
    g_ptr_array_add(priv->requestedPermissions.get(), nullptr);

    return reinterpret_cast<gchar**>(priv->requestedPermissions->pdata);
}

/**
 * webkit_web_extension_get_optional_permissions:
 * @extension: a #WebKitWebExtension
 *
 * Get the set of permissions that the extension may need for
 * optional functionality. These permissions can be requested
 * by the extension at a later time.
 * 
 * Returns: (nullable) (array zero-terminated=1) (transfer none): a
 * %NULL-terminated array of strings containing permission names,
 * or %NULL otherwise. This array and its contents are owned by
 * WebKit and should not be modified or freed.
 * 
 * Since: 2.52
 */
const gchar* const * webkit_web_extension_get_optional_permissions(WebKitWebExtension* extension)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION(extension), nullptr);

    WebKitWebExtensionPrivate* priv = extension->priv;
    if (priv->optionalPermissions)
        return reinterpret_cast<gchar**>(priv->optionalPermissions->pdata);

    auto optionalPermissions = priv->extension->optionalPermissions();
    if (!optionalPermissions.size())
        return nullptr;

    priv->optionalPermissions = adoptGRef(g_ptr_array_new_with_free_func(g_free));
    for (auto permission : optionalPermissions)
        g_ptr_array_add(priv->optionalPermissions.get(), g_strdup(permission.utf8().data()));
    g_ptr_array_add(priv->optionalPermissions.get(), nullptr);

    return reinterpret_cast<gchar**>(priv->optionalPermissions->pdata);
}

/**
 * webkit_web_extension_get_requested_permission_match_patterns:
 * @extension: a #WebKitWebExtension
 *
 * Get the set of websites that the extension requires access to for its base functionality.
 *
 * Returns: (array zero-terminated=1) (element-type WebKitWebExtensionMatchPattern) (transfer full): a
 *    %NULL-terminated array of match patterns matching the required websites.
 * 
 * Since: 2.52
 */
WebKitWebExtensionMatchPattern** webkit_web_extension_get_requested_permission_match_patterns(WebKitWebExtension* extension)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION(extension), nullptr);

    auto matchPatternSet = extension->priv->extension->requestedPermissionMatchPatterns();
    if (!matchPatternSet.size())
        return nullptr;

    GPtrArray* returnValue = g_ptr_array_new_with_free_func(g_free);
    for (Ref pattern : matchPatternSet)
        g_ptr_array_add(returnValue, webkitWebExtensionMatchPatternCreate(pattern));
    g_ptr_array_add(returnValue, nullptr);

    return reinterpret_cast<WebKitWebExtensionMatchPattern**>(g_ptr_array_free(returnValue, FALSE));
}

/**
 * webkit_web_extension_get_optional_permission_match_patterns:
 * @extension: a #WebKitWebExtension
 *
 * Get the set of websites that the extension may need access to for optional functionality.
 * These match patterns can be requested by the extension at a later time.
 *
 * Returns: (array zero-terminated=1) (element-type WebKitWebExtensionMatchPattern) (transfer full): a
 *    %NULL-terminated array of match patterns matching the optional websites.
 * 
 * Since: 2.52
 */
WebKitWebExtensionMatchPattern** webkit_web_extension_get_optional_permission_match_patterns(WebKitWebExtension* extension)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION(extension), nullptr);

    auto matchPatternSet = extension->priv->extension->optionalPermissionMatchPatterns();
    if (!matchPatternSet.size())
        return nullptr;

    GPtrArray* returnValue = g_ptr_array_new_with_free_func(g_free);
    for (Ref pattern : matchPatternSet)
        g_ptr_array_add(returnValue, webkitWebExtensionMatchPatternCreate(pattern));
    g_ptr_array_add(returnValue, nullptr);

    return reinterpret_cast<WebKitWebExtensionMatchPattern**>(g_ptr_array_free(returnValue, FALSE));
}

/**
 * webkit_web_extension_get_all_requested_match_patterns:
 * @extension: a #WebKitWebExtension
 *
 * Get the set of websites that the extension requires access to for injected content
 * and for receiving messages from websites.
 *
 * Returns: (array zero-terminated=1) (element-type WebKitWebExtensionMatchPattern) (transfer full): a
 *    %NULL-terminated array of match patterns matching all requested websites.
 * 
 * Since: 2.52
 */
WebKitWebExtensionMatchPattern** webkit_web_extension_get_all_requested_match_patterns(WebKitWebExtension* extension)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION(extension), nullptr);

    auto matchPatternSet = extension->priv->extension->allRequestedMatchPatterns();
    if (!matchPatternSet.size())
        return nullptr;

    GPtrArray* returnValue = g_ptr_array_new_with_free_func(g_free);
    for (Ref pattern : matchPatternSet)
        g_ptr_array_add(returnValue, webkitWebExtensionMatchPatternCreate(pattern));
    g_ptr_array_add(returnValue, nullptr);

    return reinterpret_cast<WebKitWebExtensionMatchPattern**>(g_ptr_array_free(returnValue, FALSE));
}

/**
 * webkit_web_extension_get_has_background_content:
 * @extension: a #WebKitWebExtension
 * 
 * Get whether the extension has background content that can run when needed.
 * 
 * Returns: `TRUE` if the extension can run in the background even when no
 * webpages are open.
 * 
 * Since: 2.52
 */
gboolean webkit_web_extension_get_has_background_content(WebKitWebExtension* extension)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION(extension), FALSE);

    return extension->priv->extension->hasBackgroundContent();
}

gboolean webkit_web_extension_get_has_service_worker_background_content(WebKitWebExtension* extension)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION(extension), FALSE);

    return extension->priv->extension->backgroundContentIsServiceWorker();
}

gboolean webkit_web_extension_get_has_modular_background_content(WebKitWebExtension* extension)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION(extension), FALSE);

    return extension->priv->extension->backgroundContentUsesModules();
}

/**
 * webkit_web_extension_get_has_persistent_background_content:
 * @extension: a #WebKitWebExtension
 * 
 * Get whether the extension has background content that stays in memory as long
 * as the extension is loaded.
 * 
 * Returns: `TRUE` if the extension can run in the background.
 * 
 * Since: 2.52
 */
gboolean webkit_web_extension_get_has_persistent_background_content(WebKitWebExtension* extension)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION(extension), FALSE);

    return extension->priv->extension->backgroundContentIsPersistent();
}

/**
 * webkit_web_extension_get_has_injected_content:
 * @extension: a #WebKitWebExtension
 * 
 * Get whether the extension has script or stylesheet content
 * that can be injected into webpages.
 * 
 * Once the extension is loaded, use the ``hasInjectedContent``
 * property on an extension context, as the injectable content
 * can change after the extension is loaded.
 * 
 * Returns: `TRUE` if the extension has content that can be
 * injected by matching against the extension's
 * requested match patterns.
 * 
 * Since: 2.52
 */
gboolean webkit_web_extension_get_has_injected_content(WebKitWebExtension* extension)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION(extension), FALSE);

    return extension->priv->extension->hasStaticInjectedContent();
}

/**
 * webkit_web_extension_get_has_options_page:
 * @extension: a #WebKitWebExtension
 * 
 * Get whether the extension has an options page.
 * 
 * The app should provide access to this page through a
 * user interface element, which can be accessed via
 * ``optionsPageURL`` on an extension context.
 * 
 * Returns: `TRUE` if the extension includes a dedicated options
 * page where users can customize settings.
 * 
 * Since: 2.52
 */
gboolean webkit_web_extension_get_has_options_page(WebKitWebExtension* extension)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION(extension), FALSE);

    return extension->priv->extension->hasOptionsPage();
}

/**
 * webkit_web_extension_get_has_override_new_tab_page:
 * @extension: a #WebKitWebExtension
 * 
 * Get whether the extension provides an alternative to
 * the default new tab page.
 * 
 * The app should prompt the user for permission to use
 * the extension's new tab page as the default, which can
 * be accessed via ``overrideNewTabPageURL``
 * on an extension context.
 * 
 * Returns: `TRUE` if the extension can specify a custom page
 * that can be displayed when a new tab is opened in the app,
 * instead of the default new tab page.
 * 
 * Since: 2.52
 */
gboolean webkit_web_extension_get_has_override_new_tab_page(WebKitWebExtension* extension)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION(extension), FALSE);

    return extension->priv->extension->hasOverrideNewTabPage();
}

/**
 * webkit_web_extension_get_has_commands:
 * @extension: a #WebKitWebExtension
 * 
 * Get whether the extension includes commands that users can invoke.
 * 
 * These commands should be accessible via keyboard shortcuts,
 * menu items, or other user interface elements provided
 * by the app. The list of commands can be accessed
 * via ``commands`` on an extension context, and
 * invoked via ``performCommand:``.
 * 
 * Returns: `TRUE` if the extension contains one or more commands
 * that can be performed by the user.
 * 
 * Since: 2.52
 */
gboolean webkit_web_extension_get_has_commands(WebKitWebExtension* extension)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION(extension), FALSE);

    return extension->priv->extension->hasCommands();
}

/**
 * webkit_web_extension_get_has_content_modification_rules:
 * @extension: a #WebKitWebExtension
 * 
 * Get whether the extension includes rules used for
 * content modification or blocking.
 * 
 * Returns: `TRUE` if the extension contains one or more rules
 * for content modification.
 * 
 * Since: 2.52
 */
gboolean webkit_web_extension_get_has_content_modification_rules(WebKitWebExtension* extension)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION(extension), FALSE);

    return extension->priv->extension->hasContentModificationRules();
}

#else // ENABLE(WK_WEB_EXTENSIONS)

WebKitWebExtension* webkit_web_extension_new(const char *extensionPath, GError **error)
{
    return nullptr;
}

void webkitWebExtensionSetPath(WebKitWebExtension* extension, const char* file)
{
    return;
}

const char* webkit_web_extension_get_path(WebKitWebExtension* extension)
{
    return nullptr;
}

gdouble webkit_web_extension_get_manifest_version(WebKitWebExtension* extension)
{
    return 0;
}

gboolean webkit_web_extension_supports_manifest_version(WebKitWebExtension* extension, gdouble manifestVersion)
{
    return 0;
}

const gchar* webkit_web_extension_get_default_locale(WebKitWebExtension* extension)
{
    return "";
}

const gchar* webkit_web_extension_get_display_name(WebKitWebExtension* extension)
{
    return "";
}

const gchar* webkit_web_extension_get_display_short_name(WebKitWebExtension* extension)
{
    return "";
}

const gchar* webkit_web_extension_get_display_version(WebKitWebExtension* extension)
{
    return "";
}

const gchar* webkit_web_extension_get_display_description(WebKitWebExtension* extension)
{
    return "";
}

const gchar* webkit_web_extension_get_display_action_label(WebKitWebExtension* extension)
{
    return "";
}

GIcon* webkit_web_extension_get_icon(WebKitWebExtension* extension, gdouble width, gdouble height)
{
    return nullptr;
}

GIcon* webkit_web_extension_get_action_icon(WebKitWebExtension* extension, gdouble width, gdouble height)
{
    return nullptr;
}

const gchar* webkit_web_extension_get_version(WebKitWebExtension* extension)
{
    return "";
}

const gchar* const * webkit_web_extension_get_requested_permissions(WebKitWebExtension* extension)
{
    return nullptr;
}

const gchar* const * webkit_web_extension_get_optional_permissions(WebKitWebExtension* extension)
{
    return nullptr;
}

WebKitWebExtensionMatchPattern** webkit_web_extension_get_requested_permission_match_patterns(WebKitWebExtension* extension)
{
    return nullptr;
}

WebKitWebExtensionMatchPattern** webkit_web_extension_get_optional_permission_match_patterns(WebKitWebExtension* extension)
{
    return nullptr;
}

WebKitWebExtensionMatchPattern** webkit_web_extension_get_all_requested_match_patterns(WebKitWebExtension* extension)
{
    return nullptr;
}

gboolean webkit_web_extension_get_has_background_content(WebKitWebExtension* extension)
{
    return 0;
}

gboolean webkit_web_extension_get_has_service_worker_background_content(WebKitWebExtension* extension)
{
    return 0;
}

gboolean webkit_web_extension_get_has_modular_background_content(WebKitWebExtension* extension)
{
    return 0;
}

gboolean webkit_web_extension_get_has_persistent_background_content(WebKitWebExtension* extension)
{
    return 0;
}

gboolean webkit_web_extension_get_has_injected_content(WebKitWebExtension* extension)
{
    return 0;
}

gboolean webkit_web_extension_get_has_options_page(WebKitWebExtension* extension)
{
    return 0;
}

gboolean webkit_web_extension_get_has_override_new_tab_page(WebKitWebExtension* extension)
{
    return 0;
}

gboolean webkit_web_extension_get_has_commands(WebKitWebExtension* extension)
{
    return 0;
}

gboolean webkit_web_extension_get_has_content_modification_rules(WebKitWebExtension* extension)
{
    return 0;
}

#endif // ENABLE(WK_WEB_EXTENSIONS)

#endif // ENABLE(2022_GLIB_API)
