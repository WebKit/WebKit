/*
 * Copyright (C) 2015 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2,1 of the License, or (at your option) any later version.
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
#include "WebKitWebsiteDataManager.h"

#include "APIWebsiteDataStore.h"
#include "WebKitWebsiteDataManagerPrivate.h"
#include <WebCore/FileSystem.h>
#include <glib/gi18n-lib.h>
#include <wtf/glib/GUniquePtr.h>

using namespace WebKit;

/**
 * SECTION: WebKitWebsiteDataManager
 * @Short_description: Website data manager
 * @Title: WebKitWebsiteDataManager
 * @See_also: #WebKitWebContext
 *
 * WebKitWebsiteDataManager allows you to manage the data that websites
 * can store in the client file system like databases or caches.
 * You can use WebKitWebsiteDataManager to configure the local directories
 * where the Website data will be stored, by creating a new manager with
 * webkit_website_data_manager_new() passing the values you want to set.
 * You can set all the possible configuration values or only some of them,
 * a default value will be used automatically for the configuration options
 * not provided. #WebKitWebsiteDataManager:base-data-directory and
 * #WebKitWebsiteDataManager:base-cache-directory are two special properties
 * that can be used to set a common base directory for all Website data and
 * caches. It's possible to provide both, a base directory and a specific value,
 * but in that case, the specific value takes precedence over the base directory.
 * The newly created WebKitWebsiteDataManager must be passed as a construct property
 * to a #WebKitWebContext, you can use webkit_web_context_new_with_website_data_manager()
 * to create a new #WebKitWebContext with a WebKitWebsiteDataManager.
 * In case you don't want to set any specific configuration, you don't need to create
 * a WebKitWebsiteDataManager, the #WebKitWebContext will create a WebKitWebsiteDataManager
 * with the default configuration. To get the WebKitWebsiteDataManager of a #WebKitWebContext
 * you can use webkit_web_context_get_website_data_manager().
 *
 * Since: 2.10
 */

enum {
    PROP_0,

    PROP_BASE_DATA_DIRECTORY,
    PROP_BASE_CACHE_DIRECTORY,
    PROP_LOCAL_STORAGE_DIRECTORY,
    PROP_DISK_CACHE_DIRECTORY,
    PROP_APPLICATION_CACHE_DIRECTORY,
    PROP_INDEXEDDB_DIRECTORY,
    PROP_WEBSQL_DIRECTORY
};

struct _WebKitWebsiteDataManagerPrivate {
    RefPtr<API::WebsiteDataStore> websiteDataStore;
    GUniquePtr<char> baseDataDirectory;
    GUniquePtr<char> baseCacheDirectory;
    GUniquePtr<char> localStorageDirectory;
    GUniquePtr<char> diskCacheDirectory;
    GUniquePtr<char> applicationCacheDirectory;
    GUniquePtr<char> indexedDBDirectory;
    GUniquePtr<char> webSQLDirectory;
};

WEBKIT_DEFINE_TYPE(WebKitWebsiteDataManager, webkit_website_data_manager, G_TYPE_OBJECT)

static void webkitWebsiteDataManagerGetProperty(GObject* object, guint propID, GValue* value, GParamSpec* paramSpec)
{
    WebKitWebsiteDataManager* manager = WEBKIT_WEBSITE_DATA_MANAGER(object);

    switch (propID) {
    case PROP_BASE_DATA_DIRECTORY:
        g_value_set_string(value, webkit_website_data_manager_get_base_data_directory(manager));
        break;
    case PROP_BASE_CACHE_DIRECTORY:
        g_value_set_string(value, webkit_website_data_manager_get_base_cache_directory(manager));
        break;
    case PROP_LOCAL_STORAGE_DIRECTORY:
        g_value_set_string(value, webkit_website_data_manager_get_local_storage_directory(manager));
        break;
    case PROP_DISK_CACHE_DIRECTORY:
        g_value_set_string(value, webkit_website_data_manager_get_disk_cache_directory(manager));
        break;
    case PROP_APPLICATION_CACHE_DIRECTORY:
        g_value_set_string(value, webkit_website_data_manager_get_offline_application_cache_directory(manager));
        break;
    case PROP_INDEXEDDB_DIRECTORY:
        g_value_set_string(value, webkit_website_data_manager_get_indexeddb_directory(manager));
        break;
    case PROP_WEBSQL_DIRECTORY:
        g_value_set_string(value, webkit_website_data_manager_get_websql_directory(manager));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propID, paramSpec);
    }
}

static void webkitWebsiteDataManagerSetProperty(GObject* object, guint propID, const GValue* value, GParamSpec* paramSpec)
{
    WebKitWebsiteDataManager* manager = WEBKIT_WEBSITE_DATA_MANAGER(object);

    switch (propID) {
    case PROP_BASE_DATA_DIRECTORY:
        manager->priv->baseDataDirectory.reset(g_value_dup_string(value));
        break;
    case PROP_BASE_CACHE_DIRECTORY:
        manager->priv->baseCacheDirectory.reset(g_value_dup_string(value));
        break;
    case PROP_LOCAL_STORAGE_DIRECTORY:
        manager->priv->localStorageDirectory.reset(g_value_dup_string(value));
        break;
    case PROP_DISK_CACHE_DIRECTORY:
        manager->priv->diskCacheDirectory.reset(g_value_dup_string(value));
        break;
    case PROP_APPLICATION_CACHE_DIRECTORY:
        manager->priv->applicationCacheDirectory.reset(g_value_dup_string(value));
        break;
    case PROP_INDEXEDDB_DIRECTORY:
        manager->priv->indexedDBDirectory.reset(g_value_dup_string(value));
        break;
    case PROP_WEBSQL_DIRECTORY:
        manager->priv->webSQLDirectory.reset(g_value_dup_string(value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propID, paramSpec);
    }
}

static void webkitWebsiteDataManagerConstructed(GObject* object)
{
    G_OBJECT_CLASS(webkit_website_data_manager_parent_class)->constructed(object);

    WebKitWebsiteDataManagerPrivate* priv = WEBKIT_WEBSITE_DATA_MANAGER(object)->priv;
    if (priv->baseDataDirectory) {
        if (!priv->localStorageDirectory)
            priv->localStorageDirectory.reset(g_build_filename(priv->baseDataDirectory.get(), "localstorage", nullptr));
        if (!priv->indexedDBDirectory)
            priv->indexedDBDirectory.reset(g_build_filename(priv->baseDataDirectory.get(), "databases", "indexeddb", nullptr));
        if (!priv->webSQLDirectory)
            priv->webSQLDirectory.reset(g_build_filename(priv->baseDataDirectory.get(), "databases", nullptr));
    }

    if (priv->baseCacheDirectory) {
        if (!priv->diskCacheDirectory)
            priv->diskCacheDirectory.reset(g_strdup(priv->baseCacheDirectory.get()));
        if (!priv->applicationCacheDirectory)
            priv->applicationCacheDirectory.reset(g_build_filename(priv->baseCacheDirectory.get(), "applications", nullptr));
    }
}

static void webkit_website_data_manager_class_init(WebKitWebsiteDataManagerClass* findClass)
{
    GObjectClass* gObjectClass = G_OBJECT_CLASS(findClass);

    gObjectClass->get_property = webkitWebsiteDataManagerGetProperty;
    gObjectClass->set_property = webkitWebsiteDataManagerSetProperty;
    gObjectClass->constructed = webkitWebsiteDataManagerConstructed;

    /**
     * WebKitWebsiteDataManager:base-data-directory:
     *
     * The base directory for Website data. This is used as a base directory
     * for any Website data when no specific data directory has been provided.
     *
     * Since: 2.10
     */
    g_object_class_install_property(
        gObjectClass,
        PROP_BASE_DATA_DIRECTORY,
        g_param_spec_string(
            "base-data-directory",
            _("Base Data Directory"),
            _("The base directory for Website data"),
            nullptr,
            static_cast<GParamFlags>(WEBKIT_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY)));

    /**
     * WebKitWebsiteDataManager:base-cache-directory:
     *
     * The base directory for Website cache. This is used as a base directory
     * for any Website cache when no specific cache directory has been provided.
     *
     * Since: 2.10
     */
    g_object_class_install_property(
        gObjectClass,
        PROP_BASE_CACHE_DIRECTORY,
        g_param_spec_string(
            "base-cache-directory",
            _("Base Cache Directory"),
            _("The base directory for Website cache"),
            nullptr,
            static_cast<GParamFlags>(WEBKIT_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY)));

    /**
     * WebKitWebsiteDataManager:local-storage-directory:
     *
     * The directory where local storage data will be stored.
     *
     * Since: 2.10
     */
    g_object_class_install_property(
        gObjectClass,
        PROP_LOCAL_STORAGE_DIRECTORY,
        g_param_spec_string(
            "local-storage-directory",
            _("Local Storage Directory"),
            _("The directory where local storage data will be stored"),
            nullptr,
            static_cast<GParamFlags>(WEBKIT_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY)));

    /**
     * WebKitWebsiteDataManager:disk-cache-directory:
     *
     * The directory where HTTP disk cache will be stored.
     *
     * Since: 2.10
     */
    g_object_class_install_property(
        gObjectClass,
        PROP_DISK_CACHE_DIRECTORY,
        g_param_spec_string(
            "disk-cache-directory",
            _("Disk Cache Directory"),
            _("The directory where HTTP disk cache will be stored"),
            nullptr,
            static_cast<GParamFlags>(WEBKIT_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY)));

    /**
     * WebKitWebsiteDataManager:offline-application-cache-directory:
     *
     * The directory where offline web application cache will be stored.
     *
     * Since: 2.10
     */
    g_object_class_install_property(
        gObjectClass,
        PROP_APPLICATION_CACHE_DIRECTORY,
        g_param_spec_string(
            "offline-application-cache-directory",
            _("Offline Web Application Cache Directory"),
            _("The directory where offline web application cache will be stored"),
            nullptr,
            static_cast<GParamFlags>(WEBKIT_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY)));

    /**
     * WebKitWebsiteDataManager:indexeddb-directory:
     *
     * The directory where IndexedDB databases will be stored.
     *
     * Since: 2.10
     */
    g_object_class_install_property(
        gObjectClass,
        PROP_INDEXEDDB_DIRECTORY,
        g_param_spec_string(
            "indexeddb-directory",
            _("IndexedDB Directory"),
            _("The directory where IndexedDB databases will be stored"),
            nullptr,
            static_cast<GParamFlags>(WEBKIT_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY)));

    /**
     * WebKitWebsiteDataManager:websql-directory:
     *
     * The directory where WebSQL databases will be stored.
     *
     * Since: 2.10
     */
    g_object_class_install_property(
        gObjectClass,
        PROP_WEBSQL_DIRECTORY,
        g_param_spec_string(
            "websql-directory",
            _("WebSQL Directory"),
            _("The directory where WebSQL databases will be stored"),
            nullptr,
            static_cast<GParamFlags>(WEBKIT_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY)));
}

WebKitWebsiteDataManager* webkitWebsiteDataManagerCreate(WebsiteDataStore::Configuration&& configuration)
{
    WebKitWebsiteDataManager* manager = WEBKIT_WEBSITE_DATA_MANAGER(g_object_new(WEBKIT_TYPE_WEBSITE_DATA_MANAGER, nullptr));
    manager->priv->websiteDataStore = API::WebsiteDataStore::create(WTFMove(configuration));

    return manager;
}

API::WebsiteDataStore& webkitWebsiteDataManagerGetDataStore(WebKitWebsiteDataManager* manager)
{
    WebKitWebsiteDataManagerPrivate* priv = manager->priv;
    if (!priv->websiteDataStore) {
        WebsiteDataStore::Configuration configuration;
        configuration.localStorageDirectory = !priv->localStorageDirectory ?
            API::WebsiteDataStore::defaultLocalStorageDirectory() : WebCore::filenameToString(priv->localStorageDirectory.get());
        configuration.networkCacheDirectory = !priv->diskCacheDirectory ?
            API::WebsiteDataStore::defaultNetworkCacheDirectory() : WebCore::pathByAppendingComponent(WebCore::filenameToString(priv->diskCacheDirectory.get()), networkCacheSubdirectory);
        configuration.applicationCacheDirectory = !priv->applicationCacheDirectory ?
            API::WebsiteDataStore::defaultApplicationCacheDirectory() : WebCore::filenameToString(priv->applicationCacheDirectory.get());
        configuration.webSQLDatabaseDirectory = !priv->webSQLDirectory ?
            API::WebsiteDataStore::defaultWebSQLDatabaseDirectory() : WebCore::filenameToString(priv->webSQLDirectory.get());
        configuration.mediaKeysStorageDirectory = API::WebsiteDataStore::defaultMediaKeysStorageDirectory();
        priv->websiteDataStore = API::WebsiteDataStore::create(WTFMove(configuration));
    }

    return *priv->websiteDataStore;
}

/**
 * webkit_website_data_manager_new:
 * @first_option_name: name of the first option to set
 * @...: value of first option, followed by more options, %NULL-terminated
 *
 * Creates a new #WebKitWebsiteDataManager with the given options. It must
 * be passed as construction parameter of a #WebKitWebContext.
 *
 * Returns: (transfer full): the newly created #WebKitWebsiteDataManager
 *
 * Since: 2.10
 */
WebKitWebsiteDataManager* webkit_website_data_manager_new(const gchar* firstOptionName, ...)
{
    va_list args;
    va_start(args, firstOptionName);
    WebKitWebsiteDataManager* manager = WEBKIT_WEBSITE_DATA_MANAGER(g_object_new_valist(WEBKIT_TYPE_WEBSITE_DATA_MANAGER, firstOptionName, args));
    va_end(args);

    return manager;
}

/**
 * webkit_website_data_manager_get_base_data_directory:
 * @manager: a #WebKitWebsiteDataManager
 *
 * Get the #WebKitWebsiteDataManager:base-data-directory property.
 *
 * Returns: the base directory for Website data, or %NULL if
 *    #WebKitWebsiteDataManager:base-data-directory was not provided.
 *
 * Since: 2.10
 */
const gchar* webkit_website_data_manager_get_base_data_directory(WebKitWebsiteDataManager* manager)
{
    g_return_val_if_fail(WEBKIT_IS_WEBSITE_DATA_MANAGER(manager), nullptr);

    return manager->priv->baseDataDirectory.get();
}

/**
 * webkit_website_data_manager_get_base_cache_directory:
 * @manager: a #WebKitWebsiteDataManager
 *
 * Get the #WebKitWebsiteDataManager:base-cache-directory property.
 *
 * Returns: the base directory for Website cache, or %NULL if
 *    #WebKitWebsiteDataManager:base-cache-directory was not provided.
 *
 * Since: 2.10
 */
const gchar* webkit_website_data_manager_get_base_cache_directory(WebKitWebsiteDataManager* manager)
{
    g_return_val_if_fail(WEBKIT_IS_WEBSITE_DATA_MANAGER(manager), nullptr);

    return manager->priv->baseCacheDirectory.get();
}

/**
 * webkit_website_data_manager_get_local_storage_directory:
 * @manager: a #WebKitWebsiteDataManager
 *
 * Get the #WebKitWebsiteDataManager:local-storage-directory property.
 *
 * Returns: the directory where local storage data is stored.
 *
 * Since: 2.10
 */
const gchar* webkit_website_data_manager_get_local_storage_directory(WebKitWebsiteDataManager* manager)
{
    g_return_val_if_fail(WEBKIT_IS_WEBSITE_DATA_MANAGER(manager), nullptr);

    WebKitWebsiteDataManagerPrivate* priv = manager->priv;
    if (!priv->localStorageDirectory)
        priv->localStorageDirectory.reset(g_strdup(API::WebsiteDataStore::defaultLocalStorageDirectory().utf8().data()));
    return priv->localStorageDirectory.get();
}

/**
 * webkit_website_data_manager_get_disk_cache_directory:
 * @manager: a #WebKitWebsiteDataManager
 *
 * Get the #WebKitWebsiteDataManager:disk-cache-directory property.
 *
 * Returns: the directory where HTTP disk cache is stored.
 *
 * Since: 2.10
 */
const gchar* webkit_website_data_manager_get_disk_cache_directory(WebKitWebsiteDataManager* manager)
{
    g_return_val_if_fail(WEBKIT_IS_WEBSITE_DATA_MANAGER(manager), nullptr);

    WebKitWebsiteDataManagerPrivate* priv = manager->priv;
    if (!priv->diskCacheDirectory) {
        // The default directory already has the subdirectory.
        priv->diskCacheDirectory.reset(g_strdup(WebCore::directoryName(API::WebsiteDataStore::defaultNetworkCacheDirectory()).utf8().data()));
    }
    return priv->diskCacheDirectory.get();
}

/**
 * webkit_website_data_manager_get_offline_application_cache_directory:
 * @manager: a #WebKitWebsiteDataManager
 *
 * Get the #WebKitWebsiteDataManager:offline-application-cache-directory property.
 *
 * Returns: the directory where offline web application cache is stored.
 *
 * Since: 2.10
 */
const gchar* webkit_website_data_manager_get_offline_application_cache_directory(WebKitWebsiteDataManager* manager)
{
    g_return_val_if_fail(WEBKIT_IS_WEBSITE_DATA_MANAGER(manager), nullptr);

    WebKitWebsiteDataManagerPrivate* priv = manager->priv;
    if (!priv->applicationCacheDirectory)
        priv->applicationCacheDirectory.reset(g_strdup(API::WebsiteDataStore::defaultApplicationCacheDirectory().utf8().data()));
    return priv->applicationCacheDirectory.get();
}

/**
 * webkit_website_data_manager_get_indexeddb_directory:
 * @manager: a #WebKitWebsiteDataManager
 *
 * Get the #WebKitWebsiteDataManager:indexeddb-directory property.
 *
 * Returns: the directory where IndexedDB databases are stored.
 *
 * Since: 2.10
 */
const gchar* webkit_website_data_manager_get_indexeddb_directory(WebKitWebsiteDataManager* manager)
{
    g_return_val_if_fail(WEBKIT_IS_WEBSITE_DATA_MANAGER(manager), nullptr);

    WebKitWebsiteDataManagerPrivate* priv = manager->priv;
    if (!priv->indexedDBDirectory)
        priv->indexedDBDirectory.reset(g_strdup(API::WebsiteDataStore::defaultIndexedDBDatabaseDirectory().utf8().data()));
    return priv->indexedDBDirectory.get();
}

/**
 * webkit_website_data_manager_get_websql_directory:
 * @manager: a #WebKitWebsiteDataManager
 *
 * Get the #WebKitWebsiteDataManager:websql-directory property.
 *
 * Returns: the directory where WebSQL databases are stored.
 *
 * Since: 2.10
 */
const gchar* webkit_website_data_manager_get_websql_directory(WebKitWebsiteDataManager* manager)
{
    g_return_val_if_fail(WEBKIT_IS_WEBSITE_DATA_MANAGER(manager), nullptr);

    WebKitWebsiteDataManagerPrivate* priv = manager->priv;
    if (!priv->webSQLDirectory)
        priv->webSQLDirectory.reset(g_strdup(API::WebsiteDataStore::defaultWebSQLDatabaseDirectory().utf8().data()));
    return priv->webSQLDirectory.get();
}
