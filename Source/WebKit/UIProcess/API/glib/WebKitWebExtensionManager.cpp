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
#include "WebKitWebExtensionManager.h"

#include <glib.h>

#if ENABLE(2022_GLIB_API)

#include "WebExtensionController.h"
#include "WebKitError.h"
#include "WebKitPrivate.h"
#include "WebKitWebExtensionContextPrivate.h"
#include "WebKitWebExtensionManagerInternal.h"
#include "WebKitWebExtensionPrivate.h"

/**
 * WebKitWebExtensionManager:
 *
 * Manages a set of loaded extension contexts.
 *
 * You can have one or more extension manager instances, allowing different parts of the app to use different sets of extensions.
 * 
 * Since: 2.56
 */
struct _WebKitWebExtensionManagerPrivate {
#if ENABLE(WK_WEB_EXTENSIONS)
    RefPtr<WebKit::WebExtensionController> controller;
    gboolean persistence = true;
    UTF8CString identifier;
    UTF8CString storageDirectory;
#endif
};

WEBKIT_DEFINE_FINAL_TYPE(WebKitWebExtensionManager, webkit_web_extension_manager, G_TYPE_OBJECT, GObject)

enum {
    PROP_0,
    PROP_PERSISTENCE,
    PROP_IDENTIFIER,
    PROP_SETTINGS,
    N_PROPERTIES
};

static std::array<GParamSpec*, N_PROPERTIES> properties;

static void webkitWebExtensionManagerGetProperty(GObject* object, guint propId, GValue* value, GParamSpec* paramSpec)
{
    WebKitWebExtensionManager* manager = WEBKIT_WEB_EXTENSION_MANAGER(object);

    switch (propId) {
    case PROP_PERSISTENCE:
        g_value_set_boolean(value, webkit_web_extension_manager_get_persistence(manager));
        break;
    case PROP_IDENTIFIER:
        g_value_set_string(value, webkit_web_extension_manager_get_identifier(manager));
        break;
    case PROP_SETTINGS:
        g_value_set_object(value, webkit_web_extension_manager_get_settings(manager));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, paramSpec);
    }
}

#if ENABLE(WK_WEB_EXTENSIONS)
static void webkitWebExtensionManagerSetProperty(GObject* object, guint propId, const GValue* value, GParamSpec* paramSpec)
{
    WebKitWebExtensionManager* manager = WEBKIT_WEB_EXTENSION_MANAGER(object);

    switch (propId) {
    case PROP_SETTINGS:
        webkit_web_extension_manager_set_settings(manager, WEBKIT_SETTINGS(g_value_get_object(value)));
        break;
    case PROP_PERSISTENCE:
        manager->priv->persistence = g_value_get_boolean(value);
        break;
    case PROP_IDENTIFIER:
        manager->priv->identifier = UTF8CString::unsafeFromUTF8(g_value_get_string(value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, paramSpec);
    }
}

static void webkitWebExtensionManagerConstructed(GObject* object)
{
    G_OBJECT_CLASS(webkit_web_extension_manager_parent_class)->constructed(object);

    WebKitWebExtensionManager* manager = WEBKIT_WEB_EXTENSION_MANAGER(object);

    RefPtr<WebKit::WebExtensionController> controller;
    if (manager->priv->identifier.length()) {
        auto uuid = WTF::UUID::parse(String::fromUTF8(manager->priv->identifier.legacyCStringPointer()));
        if (uuid)
            controller = WebKit::WebExtensionController::create(WebKit::WebExtensionControllerConfiguration::create(uuid.value()));
        else {
            g_critical("WebKitWebExtensionManager identifier property is an invalid UUID");
            controller = WebKit::WebExtensionController::create(WebKit::WebExtensionControllerConfiguration::createDefault());
        }
    } else if (!manager->priv->persistence)
        controller = WebKit::WebExtensionController::create(WebKit::WebExtensionControllerConfiguration::createNonPersistent());
    else
        controller = WebKit::WebExtensionController::create(WebKit::WebExtensionControllerConfiguration::createDefault());

    manager->priv->controller = WTF::move(controller);
}
#endif

static void webkit_web_extension_manager_class_init(WebKitWebExtensionManagerClass* klass)
{
    GObjectClass* objectClass = G_OBJECT_CLASS(klass);
    objectClass->get_property = webkitWebExtensionManagerGetProperty;
#if ENABLE(WK_WEB_EXTENSIONS)
    objectClass->set_property = webkitWebExtensionManagerSetProperty;
    objectClass->constructed = webkitWebExtensionManagerConstructed;
#endif

     /**
     * WebKitWebExtensionManager:persistence:
     * 
     * Whether this [class@WebExtensionManager] should save data to the file system.
     * See [method@WebExtensionManager.get_persistence] for more details.
     *
     * Since: 2.56
     */
    properties[PROP_PERSISTENCE] =
        g_param_spec_boolean(
            "persistence",
            nullptr, nullptr,
            TRUE,
            static_cast<GParamFlags>(WEBKIT_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY)
        );

    /**
     * WebKitWebExtensionManager:identifier:
     * 
     * A unique identifier to save data to the file system under.
     * See [method@WebExtensionManager.get_identifier] for more details.
     *
     * Since: 2.56
     */
    properties[PROP_IDENTIFIER] =
        g_param_spec_string(
            "identifier",
            nullptr, nullptr,
            nullptr,
            static_cast<GParamFlags>(WEBKIT_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY)
        );
    /**
     * WebKitWebExtensionManager:settings:
     * 
     * A [class@Settings] to use for any web views created by extensions
     * connected to this [class@WebExtensionManager].
     * See [method@WebExtensionManager.get_settings] for more details.
     *
     * Since: 2.56
     */
    properties[PROP_SETTINGS] =
        g_param_spec_object(
            "settings",
            nullptr, nullptr,
            WEBKIT_TYPE_SETTINGS,
            WEBKIT_PARAM_READWRITE
        );

    g_object_class_install_properties(objectClass, properties.size(), properties.data());
}

#if ENABLE(WK_WEB_EXTENSIONS)

WebKitWebExtensionManager* webkitWebExtensionManagerNewWithTemporaryConfiguration()
{
    WebKitWebExtensionManager* object = WEBKIT_WEB_EXTENSION_MANAGER(g_object_new(WEBKIT_TYPE_WEB_EXTENSION_MANAGER, nullptr));
    Ref controller = WebKit::WebExtensionController::create(WebKit::WebExtensionControllerConfiguration::createTemporary());

    object->priv->controller = WTF::move(controller);
    return object;
}

gboolean webkitWebExtensionManagerGetIsTemporary(WebKitWebExtensionManager *manager)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION_MANAGER(manager), false);

    WebKitWebExtensionManagerPrivate* priv = manager->priv;
    return protect(priv->controller->configuration())->storageIsTemporary();
}

const gchar* webkitWebExtensionManagerGetStorageDirectoryPath(WebKitWebExtensionManager *manager)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION_MANAGER(manager), nullptr);

    WebKitWebExtensionManagerPrivate* priv = manager->priv;
    if (!priv->storageDirectory.isNull())
        return priv->storageDirectory.legacyCStringPointer();

    auto directory = protect(priv->controller->configuration())->storageDirectory();
    if (directory.isEmpty())
        return nullptr;

    priv->storageDirectory = directory.utf8();
    return priv->storageDirectory.legacyCStringPointer();
}

/**
 * webkit_web_extension_manager_new:
 *
 * Creates a new [class@WebExtensionManager] with the default settings.
 * 
 * The default settings are to be persistent and not unique.
 * This manager, therefore, will not have a unique identifier, and data will be written
 * to the file system in a common location. When using multiple extension managers,
 * each manager should use a unique identifier to avoid conflicts.
 * 
 * Returns: the newly created [class@WebExtensionManager].
 * 
 * Since: 2.56
 */
WebKitWebExtensionManager* webkit_web_extension_manager_new()
{
    return WEBKIT_WEB_EXTENSION_MANAGER(g_object_new(WEBKIT_TYPE_WEB_EXTENSION_MANAGER, nullptr));
}

/**
 * webkit_web_extension_manager_new_with_identifier:
 * @identifier: A valid UUID
 *
 * Creates a new [class@WebExtensionManager] that is persistent and unique.
 * 
 * Data will be written to the file system in a unique location based on the specified identifier.
 * See also [ctor@WebExtensionManager.new].
 *
 * Returns: (nullable): the newly created [class@WebExtensionManager], or %NULL if the identifier is not a valid UUID.
 * 
 * Since: 2.56
 */
WebKitWebExtensionManager* webkit_web_extension_manager_new_with_identifier(const gchar* identifierUUID)
{
    if (!g_uuid_string_is_valid(identifierUUID))
        return nullptr;

    auto identifier = WTF::UUID::parse(String::fromUTF8(identifierUUID));
    if (!identifier)
        return nullptr;

    return WEBKIT_WEB_EXTENSION_MANAGER(gObjectNew(WEBKIT_TYPE_WEB_EXTENSION_MANAGER,
        "identifier", identifier.value().toString().utf8()));
}

/**
 * webkit_web_extension_manager_new_with_non_persistent_settings:
 *
 * Creates a new [class@WebExtensionManager] with non-persistent and non-unique settings.
 * 
 * When not configured to be persistent, no data will be written to the file system. This is
 * useful for extensions in "private browsing" situations.
 * 
 * Returns: the newly created [class@WebExtensionManager].
 * 
 * Since: 2.56
 */
WebKitWebExtensionManager* webkit_web_extension_manager_new_with_non_persistent_settings()
{
    return WEBKIT_WEB_EXTENSION_MANAGER(g_object_new(WEBKIT_TYPE_WEB_EXTENSION_MANAGER,
        "persistence", false,
        nullptr));
}

/**
 * webkit_web_extension_manager_get_persistence:
 * @manager: A [class@WebExtensionManager]
 *
 * Gets whether this [class@WebExtensionManager] will write data to the file system.
 * 
 * If this is %TRUE, then any connected [class@WebExtensionContext] will write extension data to the file system,
 * such as extension storage and settings. If this is %FALSE, the data will not be saved to the file system,
 * and will only be stored in memory.
 * 
 * Returns: %TRUE if this [class@WebExtensionManager] will write data to the file system.
 * 
 * Since: 2.56
 */
gboolean webkit_web_extension_manager_get_persistence(WebKitWebExtensionManager* manager)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION_MANAGER(manager), false);

    WebKitWebExtensionManagerPrivate* priv = manager->priv;
    return priv->controller->storageIsPersistent();
}

/**
 * webkit_web_extension_manager_get_identifier:
 * @manager: A [class@WebExtensionManager]
 *
 * Gets the unique identifier used for persistent storage.
 * 
 * When not configured to be persistent, or the identifier is not unique, this value will be %NULL.
 * See [method@WebExtensionManager.get_persistence] to check whether this [class@WebExtensionManager] is persistent.
 * 
 * Returns: (nullable): the unique identifier used for persistent storage, or %NULL if it is the default or not persistent.
 * 
 * Since: 2.56
 */
const gchar* webkit_web_extension_manager_get_identifier(WebKitWebExtensionManager* manager)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION_MANAGER(manager), nullptr);

    WebKitWebExtensionManagerPrivate* priv = manager->priv;
    if (!priv->identifier.isNull())
        return priv->identifier.legacyCStringPointer();

    auto identifier = protect(priv->controller->configuration())->identifier();
    if (!identifier)
        return nullptr;

    priv->identifier = identifier.value().toString().utf8();
    return priv->identifier.legacyCStringPointer();
}

/**
 * webkit_web_extension_manager_get_settings:
 * @manager: A [class@WebExtensionManager]
 *
 * Gets the [class@Settings] to be used as a basis for configuring web views in any connected contexts.
 * 
 * Returns: (transfer none): a [class@Settings] to be used for any [class@WebView] instances created by any connected contexts.
 * 
 * Since: 2.56
 */
WebKitSettings* webkit_web_extension_manager_get_settings(WebKitWebExtensionManager* manager)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION_MANAGER(manager), nullptr);

    WebKitWebExtensionManagerPrivate* priv = manager->priv;
    return protect(priv->controller->configuration())->webViewConfiguration();
}

/**
 * webkit_web_extension_manager_set_settings:
 * @manager: A [class@WebExtensionManager]
 * @settings: (nullable): A [class@Settings] to set
 *
 * Sets the [class@Settings] to be used as a basis for configuring web views in any connected contexts.
 * 
 * When set to %NULL, any newly created [class@WebView] instances will use the default settings.
 * 
 * Since: 2.56
 */
void webkit_web_extension_manager_set_settings(WebKitWebExtensionManager* manager, WebKitSettings* settings)
{
    g_return_if_fail(WEBKIT_IS_WEB_EXTENSION_MANAGER(manager));

    WebKitWebExtensionManagerPrivate* priv = manager->priv;
    protect(priv->controller->configuration())->setWebViewConfiguration(settings);
}

/**
 * webkit_web_extension_manager_get_loaded_extensions:
 * @manager: A [class@WebExtensionManager]
 *
 * Gets a list of [class@WebExtension] currently loaded by any connected [class@WebExtensionContext] instances.
 * To get the list of currently loaded extension contexts, see [method@WebExtensionManager.get_loaded_contexts].
 * 
 * Returns: (nullable) (array zero-terminated=1) (transfer container): A %NULL-terminated list of [class@WebExtension] instances,
 * or %NULL otherwise.
 *
 * Since: 2.56
 */
WebKitWebExtension** webkit_web_extension_manager_get_loaded_extensions(WebKitWebExtensionManager* manager)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION_MANAGER(manager), nullptr);
    g_return_val_if_fail(manager->priv->controller, nullptr);

    WebKitWebExtensionManagerPrivate* priv = manager->priv;
    auto extensions = priv->controller->extensions();
    if (extensions.isEmpty())
        return nullptr;

    GRefPtr<GPtrArray> loadedExtensions = adoptGRef(g_ptr_array_new_full(extensions.size(), nullptr));
    for (Ref extension : extensions) {
        if (!extension->wrapper())
            continue;
        g_ptr_array_add(loadedExtensions.get(), extension->wrapper());
    }
    g_ptr_array_add(loadedExtensions.get(), nullptr);

    return reinterpret_cast<WebKitWebExtension**>(g_ptr_array_free(loadedExtensions.leakRef(), FALSE));
}

/**
 * webkit_web_extension_manager_get_loaded_contexts:
 * @manager: A [class@WebExtensionManager]
 *
 * Gets a list of [class@WebExtensionContext] currently loaded by this @manager.
 * To get the list of currently loaded extensions, see [method@WebExtensionManager.get_loaded_extensions].
 * 
 * Returns: (nullable) (array zero-terminated=1) (transfer container): A %NULL-terminated list of [class@WebExtensionContext] instances,
 * or %NULL otherwise.
 *
 * Since: 2.56
 */
WebKitWebExtensionContext** webkit_web_extension_manager_get_loaded_contexts(WebKitWebExtensionManager* manager)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION_MANAGER(manager), nullptr);
    g_return_val_if_fail(manager->priv->controller, nullptr);

    WebKitWebExtensionManagerPrivate* priv = manager->priv;
    auto contexts = priv->controller->extensionContexts();
    if (contexts.isEmpty())
        return nullptr;

    GRefPtr<GPtrArray> loadedContexts = adoptGRef(g_ptr_array_new_full(contexts.size(), nullptr));
    for (Ref ctx : contexts) {
        if (!ctx->wrapper())
            continue;
        g_ptr_array_add(loadedContexts.get(), ctx->wrapper());
    }
    g_ptr_array_add(loadedContexts.get(), nullptr);

    return reinterpret_cast<WebKitWebExtensionContext**>(g_ptr_array_free(loadedContexts.leakRef(), FALSE));
}

/**
 * webkit_web_extension_manager_extension_context_for_extension:
 * @manager: A [class@WebExtensionManager]
 * @extension: A [class@WebExtension]
 *
 * Returns a loaded extension context for the specified extension
 * 
 * Returns: (nullable) (transfer none): The loaded extension context or %NULL otherwise.
 *
 * Since: 2.56
 */
WebKitWebExtensionContext* webkit_web_extension_manager_extension_context_for_extension(WebKitWebExtensionManager *manager, WebKitWebExtension *extension)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION_MANAGER(manager), nullptr);
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION(extension), nullptr);
    g_return_val_if_fail(manager->priv->controller, nullptr);

    WebKitWebExtensionManagerPrivate* priv = manager->priv;
    if (auto extensionContext = priv->controller->extensionContext(*webkitWebExtensionToImpl(extension)))
        return extensionContext->wrapper();

    return nullptr;
}

/**
 * webkit_web_extension_manager_load_extension_context:
 * @manager: A [class@WebExtensionManager]
 * @context: A [class@WebExtensionContext] to load
 * @error: return location for error or %NULL to ignore
 *
 * Loads the specified [class@WebExtensionContext]. This causes the context to start,
 * loading any background content, and injecting any content into relevant tabs.
 * 
 * Returns: %TRUE if the [class@WebExtensionContext] was loaded successfully or %FALSE in case of error.
 * 
 * Since: 2.56
 */
gboolean webkit_web_extension_manager_load_extension_context(WebKitWebExtensionManager *manager, WebKitWebExtensionContext *context, GError **error)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION_MANAGER(manager), false);
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION_CONTEXT(context), false);

    WebKitWebExtensionManagerPrivate* priv = manager->priv;
    auto loadResult = priv->controller->load(*webkitWebExtensionContextToImpl(context));
    if (!loadResult) {
        RefPtr internalError = loadResult.error();
        g_set_error_literal(error, webkit_web_extension_context_error_quark(),
            toWebKitWebExtensionContextError(internalError->errorCode()), internalError->localizedDescription().utf8().legacyCStringPointer());
        return false;
    }

    return loadResult.value();
}

/**
 * webkit_web_extension_manager_unload_extension_context:
 * @manager: A [class@WebExtensionManager]
 * @context: A [class@WebExtensionContext] to unload
 * @error: return location for error or %NULL to ignore
 *
 * Unloads the specified [class@WebExtensionContext]. This causes the context to stop running.
 * 
 * Returns: %TRUE if the [class@WebExtensionContext] was unloaded successfully or %FALSE in case of error.
 * 
 * Since: 2.56
 */
gboolean webkit_web_extension_manager_unload_extension_context(WebKitWebExtensionManager *manager, WebKitWebExtensionContext *context, GError **error)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION_MANAGER(manager), false);
    g_return_val_if_fail(WEBKIT_IS_WEB_EXTENSION_CONTEXT(context), false);

    WebKitWebExtensionManagerPrivate* priv = manager->priv;
    auto unloadResult = priv->controller->unload(*webkitWebExtensionContextToImpl(context));
    if (!unloadResult) {
        RefPtr internalError = unloadResult.error();
        g_set_error_literal(error, webkit_web_extension_context_error_quark(),
            toWebKitWebExtensionContextError(internalError->errorCode()), internalError->localizedDescription().utf8().legacyCStringPointer());
        return false;
    }

    return unloadResult.value();
}

#else // ENABLE(WK_WEB_EXTENSIONS)

WebKitWebExtensionManager* webkit_web_extension_manager_new()
{
    return nullptr;
}

WebKitWebExtensionManager* webkit_web_extension_manager_new_with_identifier(const gchar* identifier)
{
    return nullptr;
}

WebKitWebExtensionManager* webkit_web_extension_manager_new_with_non_persistent_settings()
{
    return nullptr;
}

gboolean webkit_web_extension_manager_get_persistence(WebKitWebExtensionManager* manager)
{
    return false;
}

const gchar* webkit_web_extension_manager_get_identifier(WebKitWebExtensionManager* manager)
{
    return nullptr;
}

WebKitSettings* webkit_web_extension_manager_get_settings(WebKitWebExtensionManager* manager)
{
    return nullptr;
}

void webkit_web_extension_manager_set_settings(WebKitWebExtensionManager* manager, WebKitSettings* settings)
{
    return;
}

WebKitWebExtension** webkit_web_extension_manager_get_loaded_extensions(WebKitWebExtensionManager* manager)
{
    return nullptr;
}

WebKitWebExtensionContext** webkit_web_extension_manager_get_loaded_contexts(WebKitWebExtensionManager* manager)
{
    return nullptr;
}

WebKitWebExtensionContext* webkit_web_extension_manager_extension_context_for_extension(WebKitWebExtensionManager *manager, WebKitWebExtension *extension)
{
    return nullptr;
}

gboolean webkit_web_extension_manager_load_extension_context(WebKitWebExtensionManager *manager, WebKitWebExtensionContext *context, GError **error)
{
    return FALSE;
}

gboolean webkit_web_extension_manager_unload_extension_context(WebKitWebExtensionManager *manager, WebKitWebExtensionContext *context, GError **error)
{
    return FALSE;
}

#endif // ENABLE(WK_WEB_EXTENSIONS)

#endif // ENABLE(2022_GLIB_API)
