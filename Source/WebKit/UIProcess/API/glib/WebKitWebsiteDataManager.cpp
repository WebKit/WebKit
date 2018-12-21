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
#include "WebKitCookieManagerPrivate.h"
#include "WebKitPrivate.h"
#include "WebKitWebsiteDataManagerPrivate.h"
#include "WebKitWebsiteDataPrivate.h"
#include "WebsiteDataFetchOption.h"
#include <WebCore/FileSystem.h>
#include <glib/gi18n-lib.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/glib/WTFGType.h>

using namespace WebKit;

/**
 * SECTION: WebKitWebsiteDataManager
 * @Short_description: Website data manager
 * @Title: WebKitWebsiteDataManager
 * @See_also: #WebKitWebContext, #WebKitWebsiteData
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
 * A WebKitWebsiteDataManager can also be ephemeral and then all the directories configuration
 * is not needed because website data will never persist. You can create an ephemeral WebKitWebsiteDataManager
 * with webkit_website_data_manager_new_ephemeral(). Then you can pass an ephemeral WebKitWebsiteDataManager to
 * a #WebKitWebContext to make it ephemeral or use webkit_web_context_new_ephemeral() and the WebKitWebsiteDataManager
 * will be automatically created by the #WebKitWebContext.
 *
 * WebKitWebsiteDataManager can also be used to fetch websites data, remove data
 * stored by particular websites, or clear data for all websites modified since a given
 * period of time.
 *
 * Since: 2.10
 */

using namespace WebKit;

enum {
    PROP_0,

    PROP_BASE_DATA_DIRECTORY,
    PROP_BASE_CACHE_DIRECTORY,
    PROP_LOCAL_STORAGE_DIRECTORY,
    PROP_DISK_CACHE_DIRECTORY,
    PROP_APPLICATION_CACHE_DIRECTORY,
    PROP_INDEXEDDB_DIRECTORY,
    PROP_WEBSQL_DIRECTORY,
    PROP_IS_EPHEMERAL
};

struct _WebKitWebsiteDataManagerPrivate {
    ~_WebKitWebsiteDataManagerPrivate()
    {
        ASSERT(processPools.isEmpty());
    }

    RefPtr<API::WebsiteDataStore> websiteDataStore;
    GUniquePtr<char> baseDataDirectory;
    GUniquePtr<char> baseCacheDirectory;
    GUniquePtr<char> localStorageDirectory;
    GUniquePtr<char> diskCacheDirectory;
    GUniquePtr<char> applicationCacheDirectory;
    GUniquePtr<char> indexedDBDirectory;
    GUniquePtr<char> webSQLDirectory;

    GRefPtr<WebKitCookieManager> cookieManager;
    Vector<WebProcessPool*> processPools;
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
    case PROP_IS_EPHEMERAL:
        g_value_set_boolean(value, webkit_website_data_manager_is_ephemeral(manager));
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
    case PROP_IS_EPHEMERAL:
        if (g_value_get_boolean(value))
            manager->priv->websiteDataStore = API::WebsiteDataStore::createNonPersistentDataStore();
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

    /**
     * WebKitWebsiteDataManager:is-ephemeral:
     *
     * Whether the #WebKitWebsiteDataManager is ephemeral. An ephemeral #WebKitWebsiteDataManager
     * handles all websites data as non-persistent, and nothing will be written to the client
     * storage. Note that if you create an ephemeral #WebKitWebsiteDataManager all other construction
     * parameters to configure data directories will be ignored.
     *
     * Since: 2.16
     */
    g_object_class_install_property(
        gObjectClass,
        PROP_IS_EPHEMERAL,
        g_param_spec_boolean(
            "is-ephemeral",
            "Is Ephemeral",
            _("Whether the WebKitWebsiteDataManager is ephemeral"),
            FALSE,
            static_cast<GParamFlags>(WEBKIT_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY)));
}

WebKitWebsiteDataManager* webkitWebsiteDataManagerCreate(Ref<WebsiteDataStoreConfiguration>&& configuration)
{
    WebKitWebsiteDataManager* manager = WEBKIT_WEBSITE_DATA_MANAGER(g_object_new(WEBKIT_TYPE_WEBSITE_DATA_MANAGER, nullptr));
    manager->priv->websiteDataStore = API::WebsiteDataStore::createLegacy(WTFMove(configuration));

    return manager;
}

API::WebsiteDataStore& webkitWebsiteDataManagerGetDataStore(WebKitWebsiteDataManager* manager)
{
    WebKitWebsiteDataManagerPrivate* priv = manager->priv;
    if (!priv->websiteDataStore) {
        auto configuration = WebsiteDataStoreConfiguration::create();
        configuration->setLocalStorageDirectory(!priv->localStorageDirectory ?
            API::WebsiteDataStore::defaultLocalStorageDirectory() : WebCore::FileSystem::stringFromFileSystemRepresentation(priv->localStorageDirectory.get()));
        configuration->setNetworkCacheDirectory(!priv->diskCacheDirectory ?
            API::WebsiteDataStore::defaultNetworkCacheDirectory() : WebCore::FileSystem::pathByAppendingComponent(WebCore::FileSystem::stringFromFileSystemRepresentation(priv->diskCacheDirectory.get()), networkCacheSubdirectory));
        configuration->setApplicationCacheDirectory(!priv->applicationCacheDirectory ?
            API::WebsiteDataStore::defaultApplicationCacheDirectory() : WebCore::FileSystem::stringFromFileSystemRepresentation(priv->applicationCacheDirectory.get()));
        configuration->setWebSQLDatabaseDirectory(!priv->webSQLDirectory ?
            API::WebsiteDataStore::defaultWebSQLDatabaseDirectory() : WebCore::FileSystem::stringFromFileSystemRepresentation(priv->webSQLDirectory.get()));
        configuration->setMediaKeysStorageDirectory(API::WebsiteDataStore::defaultMediaKeysStorageDirectory());
        priv->websiteDataStore = API::WebsiteDataStore::createLegacy(WTFMove(configuration));
    }

    return *priv->websiteDataStore;
}

void webkitWebsiteDataManagerAddProcessPool(WebKitWebsiteDataManager* manager, WebProcessPool& processPool)
{
    ASSERT(!manager->priv->processPools.contains(&processPool));
    manager->priv->processPools.append(&processPool);
}

void webkitWebsiteDataManagerRemoveProcessPool(WebKitWebsiteDataManager* manager, WebProcessPool& processPool)
{
    ASSERT(manager->priv->processPools.contains(&processPool));
    manager->priv->processPools.removeFirst(&processPool);
}

const Vector<WebProcessPool*>& webkitWebsiteDataManagerGetProcessPools(WebKitWebsiteDataManager* manager)
{
    return manager->priv->processPools;
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
 * webkit_website_data_manager_new_ephemeral:
 *
 * Creates an ephemeral #WebKitWebsiteDataManager. See #WebKitWebsiteDataManager:is-ephemeral for more details.
 *
 * Returns: (transfer full): a new ephemeral #WebKitWebsiteDataManager.
 *
 * Since: 2.16
 */
WebKitWebsiteDataManager* webkit_website_data_manager_new_ephemeral()
{
    return WEBKIT_WEBSITE_DATA_MANAGER(g_object_new(WEBKIT_TYPE_WEBSITE_DATA_MANAGER, "is-ephemeral", TRUE, nullptr));
}

/**
 * webkit_website_data_manager_is_ephemeral:
 * @manager: a #WebKitWebsiteDataManager
 *
 * Get whether a #WebKitWebsiteDataManager is ephemeral. See #WebKitWebsiteDataManager:is-ephemeral for more details.
 *
 * Returns: %TRUE if @manager is epheral or %FALSE otherwise.
 *
 * Since: 2.16
 */
gboolean webkit_website_data_manager_is_ephemeral(WebKitWebsiteDataManager* manager)
{
    g_return_val_if_fail(WEBKIT_IS_WEBSITE_DATA_MANAGER(manager), FALSE);

    return manager->priv->websiteDataStore && !manager->priv->websiteDataStore->isPersistent();
}

/**
 * webkit_website_data_manager_get_base_data_directory:
 * @manager: a #WebKitWebsiteDataManager
 *
 * Get the #WebKitWebsiteDataManager:base-data-directory property.
 *
 * Returns: (allow-none): the base directory for Website data, or %NULL if
 *    #WebKitWebsiteDataManager:base-data-directory was not provided or @manager is ephemeral.
 *
 * Since: 2.10
 */
const gchar* webkit_website_data_manager_get_base_data_directory(WebKitWebsiteDataManager* manager)
{
    g_return_val_if_fail(WEBKIT_IS_WEBSITE_DATA_MANAGER(manager), nullptr);

    if (manager->priv->websiteDataStore && !manager->priv->websiteDataStore->isPersistent())
        return nullptr;

    return manager->priv->baseDataDirectory.get();
}

/**
 * webkit_website_data_manager_get_base_cache_directory:
 * @manager: a #WebKitWebsiteDataManager
 *
 * Get the #WebKitWebsiteDataManager:base-cache-directory property.
 *
 * Returns: (allow-none): the base directory for Website cache, or %NULL if
 *    #WebKitWebsiteDataManager:base-cache-directory was not provided or @manager is ephemeral.
 *
 * Since: 2.10
 */
const gchar* webkit_website_data_manager_get_base_cache_directory(WebKitWebsiteDataManager* manager)
{
    g_return_val_if_fail(WEBKIT_IS_WEBSITE_DATA_MANAGER(manager), nullptr);

    if (manager->priv->websiteDataStore && !manager->priv->websiteDataStore->isPersistent())
        return nullptr;

    return manager->priv->baseCacheDirectory.get();
}

/**
 * webkit_website_data_manager_get_local_storage_directory:
 * @manager: a #WebKitWebsiteDataManager
 *
 * Get the #WebKitWebsiteDataManager:local-storage-directory property.
 *
 * Returns: (allow-none): the directory where local storage data is stored or %NULL if @manager is ephemeral.
 *
 * Since: 2.10
 */
const gchar* webkit_website_data_manager_get_local_storage_directory(WebKitWebsiteDataManager* manager)
{
    g_return_val_if_fail(WEBKIT_IS_WEBSITE_DATA_MANAGER(manager), nullptr);

    WebKitWebsiteDataManagerPrivate* priv = manager->priv;
    if (priv->websiteDataStore && !priv->websiteDataStore->isPersistent())
        return nullptr;

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
 * Returns: (allow-none): the directory where HTTP disk cache is stored or %NULL if @manager is ephemeral.
 *
 * Since: 2.10
 */
const gchar* webkit_website_data_manager_get_disk_cache_directory(WebKitWebsiteDataManager* manager)
{
    g_return_val_if_fail(WEBKIT_IS_WEBSITE_DATA_MANAGER(manager), nullptr);

    WebKitWebsiteDataManagerPrivate* priv = manager->priv;
    if (priv->websiteDataStore && !priv->websiteDataStore->isPersistent())
        return nullptr;

    if (!priv->diskCacheDirectory) {
        // The default directory already has the subdirectory.
        priv->diskCacheDirectory.reset(g_strdup(WebCore::FileSystem::directoryName(API::WebsiteDataStore::defaultNetworkCacheDirectory()).utf8().data()));
    }
    return priv->diskCacheDirectory.get();
}

/**
 * webkit_website_data_manager_get_offline_application_cache_directory:
 * @manager: a #WebKitWebsiteDataManager
 *
 * Get the #WebKitWebsiteDataManager:offline-application-cache-directory property.
 *
 * Returns: (allow-none): the directory where offline web application cache is stored or %NULL if @manager is ephemeral.
 *
 * Since: 2.10
 */
const gchar* webkit_website_data_manager_get_offline_application_cache_directory(WebKitWebsiteDataManager* manager)
{
    g_return_val_if_fail(WEBKIT_IS_WEBSITE_DATA_MANAGER(manager), nullptr);

    WebKitWebsiteDataManagerPrivate* priv = manager->priv;
    if (priv->websiteDataStore && !priv->websiteDataStore->isPersistent())
        return nullptr;

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
 * Returns: (allow-none): the directory where IndexedDB databases are stored or %NULL if @manager is ephemeral.
 *
 * Since: 2.10
 */
const gchar* webkit_website_data_manager_get_indexeddb_directory(WebKitWebsiteDataManager* manager)
{
    g_return_val_if_fail(WEBKIT_IS_WEBSITE_DATA_MANAGER(manager), nullptr);

    WebKitWebsiteDataManagerPrivate* priv = manager->priv;
    if (priv->websiteDataStore && !priv->websiteDataStore->isPersistent())
        return nullptr;

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
 * Returns: (allow-none): the directory where WebSQL databases are stored or %NULL if @manager is ephemeral.
 *
 * Since: 2.10
 */
const gchar* webkit_website_data_manager_get_websql_directory(WebKitWebsiteDataManager* manager)
{
    g_return_val_if_fail(WEBKIT_IS_WEBSITE_DATA_MANAGER(manager), nullptr);

    WebKitWebsiteDataManagerPrivate* priv = manager->priv;
    if (priv->websiteDataStore && !priv->websiteDataStore->isPersistent())
        return nullptr;

    if (!priv->webSQLDirectory)
        priv->webSQLDirectory.reset(g_strdup(API::WebsiteDataStore::defaultWebSQLDatabaseDirectory().utf8().data()));
    return priv->webSQLDirectory.get();
}

/**
 * webkit_website_data_manager_get_cookie_manager:
 * @manager: a #WebKitWebsiteDataManager
 *
 * Get the #WebKitCookieManager of @manager.
 *
 * Returns: (transfer none): a #WebKitCookieManager
 *
 * Since: 2.16
 */
WebKitCookieManager* webkit_website_data_manager_get_cookie_manager(WebKitWebsiteDataManager* manager)
{
    g_return_val_if_fail(WEBKIT_IS_WEBSITE_DATA_MANAGER(manager), nullptr);

    if (!manager->priv->cookieManager)
        manager->priv->cookieManager = adoptGRef(webkitCookieManagerCreate(manager));

    return manager->priv->cookieManager.get();
}

static OptionSet<WebsiteDataType> toWebsiteDataTypes(WebKitWebsiteDataTypes types)
{
    OptionSet<WebsiteDataType> returnValue;
    if (types & WEBKIT_WEBSITE_DATA_MEMORY_CACHE)
        returnValue.add(WebsiteDataType::MemoryCache);
    if (types & WEBKIT_WEBSITE_DATA_DISK_CACHE)
        returnValue.add(WebsiteDataType::DiskCache);
    if (types & WEBKIT_WEBSITE_DATA_OFFLINE_APPLICATION_CACHE)
        returnValue.add(WebsiteDataType::OfflineWebApplicationCache);
    if (types & WEBKIT_WEBSITE_DATA_SESSION_STORAGE)
        returnValue.add(WebsiteDataType::SessionStorage);
    if (types & WEBKIT_WEBSITE_DATA_LOCAL_STORAGE)
        returnValue.add(WebsiteDataType::LocalStorage);
    if (types & WEBKIT_WEBSITE_DATA_WEBSQL_DATABASES)
        returnValue.add(WebsiteDataType::WebSQLDatabases);
    if (types & WEBKIT_WEBSITE_DATA_INDEXEDDB_DATABASES)
        returnValue.add(WebsiteDataType::IndexedDBDatabases);
#if ENABLE(NETSCAPE_PLUGIN_API)
    if (types & WEBKIT_WEBSITE_DATA_PLUGIN_DATA)
        returnValue.add(WebsiteDataType::PlugInData);
#endif
    if (types & WEBKIT_WEBSITE_DATA_COOKIES)
        returnValue.add(WebsiteDataType::Cookies);
    if (types & WEBKIT_WEBSITE_DATA_DEVICE_ID_HASH_SALT)
        returnValue.add(WebsiteDataType::DeviceIdHashSalt);
    return returnValue;
}

/**
 * webkit_website_data_manager_fetch:
 * @manager: a #WebKitWebsiteDataManager
 * @types: #WebKitWebsiteDataTypes
 * @cancellable: (allow-none): a #GCancellable or %NULL to ignore
 * @callback: (scope async): a #GAsyncReadyCallback to call when the request is satisfied
 * @user_data: (closure): the data to pass to callback function
 *
 * Asynchronously get the list of #WebKitWebsiteData for the given @types.
 *
 * When the operation is finished, @callback will be called. You can then call
 * webkit_website_data_manager_fetch_finish() to get the result of the operation.
 *
 * Since: 2.16
 */
void webkit_website_data_manager_fetch(WebKitWebsiteDataManager* manager, WebKitWebsiteDataTypes types, GCancellable* cancellable, GAsyncReadyCallback callback, gpointer userData)
{
    g_return_if_fail(WEBKIT_IS_WEBSITE_DATA_MANAGER(manager));

    GRefPtr<GTask> task = adoptGRef(g_task_new(manager, cancellable, callback, userData));
    manager->priv->websiteDataStore->websiteDataStore().fetchData(toWebsiteDataTypes(types), WebsiteDataFetchOption::ComputeSizes, [task = WTFMove(task)] (Vector<WebsiteDataRecord> records) {
        GList* dataList = nullptr;
        while (!records.isEmpty()) {
            if (auto* data = webkitWebsiteDataCreate(records.takeLast()))
                dataList = g_list_prepend(dataList, data);
        }

        g_task_return_pointer(task.get(), dataList, [](gpointer data) {
            g_list_free_full(static_cast<GList*>(data), reinterpret_cast<GDestroyNotify>(webkit_website_data_unref));
        });
    });
}

/**
 * webkit_website_data_manager_fetch_finish:
 * @manager: a #WebKitWebsiteDataManager
 * @result: a #GAsyncResult
 * @error: return location for error or %NULL to ignore
 *
 * Finish an asynchronous operation started with webkit_website_data_manager_fetch().
 *
 * Returns: (element-type WebKitWebsiteData) (transfer full): a #GList of #WebKitWebsiteData. You must free the #GList with
 *    g_list_free() and unref the #WebKitWebsiteData<!-- -->s with webkit_website_data_unref() when you're done with them.
 *
 * Since: 2.16
 */
GList* webkit_website_data_manager_fetch_finish(WebKitWebsiteDataManager* manager, GAsyncResult* result, GError** error)
{
    g_return_val_if_fail(WEBKIT_IS_WEBSITE_DATA_MANAGER(manager), nullptr);
    g_return_val_if_fail(g_task_is_valid(result, manager), nullptr);

    return static_cast<GList*>(g_task_propagate_pointer(G_TASK(result), error));
}

/**
 * webkit_website_data_manager_remove:
 * @manager: a #WebKitWebsiteDataManager
 * @types: #WebKitWebsiteDataTypes
 * @website_data: (element-type WebKitWebsiteData): a #GList of #WebKitWebsiteData
 * @cancellable: (allow-none): a #GCancellable or %NULL to ignore
 * @callback: (scope async): a #GAsyncReadyCallback to call when the request is satisfied
 * @user_data: (closure): the data to pass to callback function
 *
 * Asynchronously removes the website data of the for the given @types for websites in the given @website_data list.
 * Use webkit_website_data_manager_clear() if you want to remove the website data for all sites.
 *
 * When the operation is finished, @callback will be called. You can then call
 * webkit_website_data_manager_remove_finish() to get the result of the operation.
 *
 * Since: 2.16
 */
void webkit_website_data_manager_remove(WebKitWebsiteDataManager* manager, WebKitWebsiteDataTypes types, GList* websiteData, GCancellable* cancellable, GAsyncReadyCallback callback, gpointer userData)
{
    g_return_if_fail(WEBKIT_IS_WEBSITE_DATA_MANAGER(manager));
    g_return_if_fail(websiteData);

    // We have to remove the hash salts when cookies are removed.
    if (types & WEBKIT_WEBSITE_DATA_COOKIES)
        types = static_cast<WebKitWebsiteDataTypes>(types | WEBKIT_WEBSITE_DATA_DEVICE_ID_HASH_SALT);

    Vector<WebsiteDataRecord> records;
    for (GList* item = websiteData; item; item = g_list_next(item)) {
        WebKitWebsiteData* data = static_cast<WebKitWebsiteData*>(item->data);

        if (webkit_website_data_get_types(data) & types)
            records.append(webkitWebsiteDataGetRecord(data));
    }

    GRefPtr<GTask> task = adoptGRef(g_task_new(manager, cancellable, callback, userData));
    if (records.isEmpty()) {
        g_task_return_boolean(task.get(), TRUE);
        return;
    }

    manager->priv->websiteDataStore->websiteDataStore().removeData(toWebsiteDataTypes(types), records, [task = WTFMove(task)] {
        g_task_return_boolean(task.get(), TRUE);
    });
}

/**
 * webkit_website_data_manager_remove_finish:
 * @manager: a #WebKitWebsiteDataManager
 * @result: a #GAsyncResult
 * @error: return location for error or %NULL to ignore
 *
 * Finish an asynchronous operation started with webkit_website_data_manager_remove().
 *
 * Returns: %TRUE if website data resources were successfully removed, or %FALSE otherwise.
 *
 * Since: 2.16
 */
gboolean webkit_website_data_manager_remove_finish(WebKitWebsiteDataManager* manager, GAsyncResult* result, GError** error)
{
    g_return_val_if_fail(WEBKIT_IS_WEBSITE_DATA_MANAGER(manager), FALSE);
    g_return_val_if_fail(g_task_is_valid(result, manager), FALSE);

    return g_task_propagate_boolean(G_TASK(result), error);
}

/**
 * webkit_website_data_manager_clear:
 * @manager: a #WebKitWebsiteDataManager
 * @types: #WebKitWebsiteDataTypes
 * @timespan: a #GTimeSpan
 * @cancellable: (allow-none): a #GCancellable or %NULL to ignore
 * @callback: (scope async): a #GAsyncReadyCallback to call when the request is satisfied
 * @user_data: (closure): the data to pass to callback function
 *
 * Asynchronously clear the website data of the given @types modified in the past @timespan.
 * If @timespan is 0, all website data will be removed.
 *
 * When the operation is finished, @callback will be called. You can then call
 * webkit_website_data_manager_clear_finish() to get the result of the operation.
 *
 * Due to implementation limitations, this function does not currently delete
 * any stored cookies if @timespan is nonzero. This behavior may change in the
 * future.
 *
 * Since: 2.16
 */
void webkit_website_data_manager_clear(WebKitWebsiteDataManager* manager, WebKitWebsiteDataTypes types, GTimeSpan timeSpan, GCancellable* cancellable, GAsyncReadyCallback callback, gpointer userData)
{
    g_return_if_fail(WEBKIT_IS_WEBSITE_DATA_MANAGER(manager));

    WallTime timePoint = timeSpan ? WallTime::now() - Seconds::fromMicroseconds(timeSpan) : WallTime::fromRawSeconds(0);
    GRefPtr<GTask> task = adoptGRef(g_task_new(manager, cancellable, callback, userData));
    manager->priv->websiteDataStore->websiteDataStore().removeData(toWebsiteDataTypes(types), timePoint, [task = WTFMove(task)] {
        g_task_return_boolean(task.get(), TRUE);
    });
}

/**
 * webkit_website_data_manager_clear_finish:
 * @manager: a #WebKitWebsiteDataManager
 * @result: a #GAsyncResult
 * @error: return location for error or %NULL to ignore
 *
 * Finish an asynchronous operation started with webkit_website_data_manager_clear()
 *
 * Returns: %TRUE if website data was successfully cleared, or %FALSE otherwise.
 *
 * Since: 2.16
 */
gboolean webkit_website_data_manager_clear_finish(WebKitWebsiteDataManager* manager, GAsyncResult* result, GError** error)
{
    g_return_val_if_fail(WEBKIT_IS_WEBSITE_DATA_MANAGER(manager), FALSE);
    g_return_val_if_fail(g_task_is_valid(result, manager), FALSE);

    return g_task_propagate_boolean(G_TASK(result), error);
}
