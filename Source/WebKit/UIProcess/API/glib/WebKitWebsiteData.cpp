/*
 * Copyright (C) 2017 Igalia S.L.
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
#include "WebKitWebsiteData.h"

#include "WebKitSecurityOriginPrivate.h"
#include "WebKitWebsiteDataPrivate.h"
#include <glib/gi18n-lib.h>
#include <wtf/HashTable.h>
#include <wtf/Vector.h>

using namespace WebKit;

/**
 * SECTION: WebKitWebsiteData
 * @Short_description: Website data
 * @Title: WebKitWebsiteData
 * @See_also: #WebKitWebsiteDataManager
 *
 * WebKitWebsiteData represents data stored in the client by a particular website.
 * A website is normally a set of URLs grouped by domain name. You can get the website name,
 * which is usually the domain, with webkit_website_data_get_name().
 * Documents loaded from the file system, like file:// URIs, are all grouped in the same WebKitWebsiteData
 * with the name "Local files".
 *
 * A website can store different types of data in the client side. #WebKitWebsiteDataTypes is an enum containing
 * all the possible data types; use webkit_website_data_get_types() to get the bitmask of data types.
 * It's also possible to know the size of the data stored for some of the #WebKitWebsiteDataTypes by using
 * webkit_website_data_get_size().
 *
 * A list of WebKitWebsiteData can be retrieved with webkit_website_data_manager_fetch(). See #WebKitWebsiteDataManager
 * for more information.
 *
 * Since: 2.16
 */
struct _WebKitWebsiteData {
    explicit _WebKitWebsiteData(WebsiteDataRecord&& websiteDataDecord)
        : record(WTFMove(websiteDataDecord))
    {
    }

    WebsiteDataRecord record;
    CString displayName;
    int referenceCount { 1 };
};

G_DEFINE_BOXED_TYPE(WebKitWebsiteData, webkit_website_data, webkit_website_data_ref, webkit_website_data_unref)

static bool recordContainsSupportedDataTypes(const WebsiteDataRecord& record)
{
    return record.types.containsAny({
        WebsiteDataType::MemoryCache,
        WebsiteDataType::DiskCache,
        WebsiteDataType::OfflineWebApplicationCache,
        WebsiteDataType::SessionStorage,
        WebsiteDataType::LocalStorage,
        WebsiteDataType::WebSQLDatabases,
        WebsiteDataType::IndexedDBDatabases,
        WebsiteDataType::HSTSCache,
#if ENABLE(NETSCAPE_PLUGIN_API)
        WebsiteDataType::PlugInData,
#endif
        WebsiteDataType::Cookies,
        WebsiteDataType::DeviceIdHashSalt,
        WebsiteDataType::ResourceLoadStatistics,
        WebsiteDataType::ServiceWorkerRegistrations
    });
}

static WebKitWebsiteDataTypes toWebKitWebsiteDataTypes(OptionSet<WebsiteDataType> types)
{
    uint32_t returnValue = 0;
    if (types.contains(WebsiteDataType::MemoryCache))
        returnValue |= WEBKIT_WEBSITE_DATA_MEMORY_CACHE;
    if (types.contains(WebsiteDataType::DiskCache))
        returnValue |= WEBKIT_WEBSITE_DATA_DISK_CACHE;
    if (types.contains(WebsiteDataType::OfflineWebApplicationCache))
        returnValue |= WEBKIT_WEBSITE_DATA_OFFLINE_APPLICATION_CACHE;
    if (types.contains(WebsiteDataType::SessionStorage))
        returnValue |= WEBKIT_WEBSITE_DATA_SESSION_STORAGE;
    if (types.contains(WebsiteDataType::LocalStorage))
        returnValue |= WEBKIT_WEBSITE_DATA_LOCAL_STORAGE;
    if (types.contains(WebsiteDataType::WebSQLDatabases))
        returnValue |= WEBKIT_WEBSITE_DATA_WEBSQL_DATABASES;
    if (types.contains(WebsiteDataType::IndexedDBDatabases))
        returnValue |= WEBKIT_WEBSITE_DATA_INDEXEDDB_DATABASES;
    if (types.contains(WebsiteDataType::HSTSCache))
        returnValue |= WEBKIT_WEBSITE_DATA_HSTS_CACHE;
#if ENABLE(NETSCAPE_PLUGIN_API)
    if (types.contains(WebsiteDataType::PlugInData))
        returnValue |= WEBKIT_WEBSITE_DATA_PLUGIN_DATA;
#endif
    if (types.contains(WebsiteDataType::Cookies))
        returnValue |= WEBKIT_WEBSITE_DATA_COOKIES;
    if (types.contains(WebsiteDataType::DeviceIdHashSalt))
        returnValue |= WEBKIT_WEBSITE_DATA_DEVICE_ID_HASH_SALT;
    if (types.contains(WebsiteDataType::ResourceLoadStatistics))
        returnValue |= WEBKIT_WEBSITE_DATA_ITP;
    if (types.contains(WebsiteDataType::ServiceWorkerRegistrations))
        returnValue |= WEBKIT_WEBSITE_DATA_SERVICE_WORKER_REGISTRATIONS;
    return static_cast<WebKitWebsiteDataTypes>(returnValue);
}

WebKitWebsiteData* webkitWebsiteDataCreate(WebsiteDataRecord&& record)
{
    if (!recordContainsSupportedDataTypes(record))
        return nullptr;

    WebKitWebsiteData* websiteData = static_cast<WebKitWebsiteData*>(fastMalloc(sizeof(WebKitWebsiteData)));
    new (websiteData) WebKitWebsiteData(WTFMove(record));
    return websiteData;
}

const WebKit::WebsiteDataRecord& webkitWebsiteDataGetRecord(WebKitWebsiteData* websiteData)
{
    ASSERT(websiteData);
    return websiteData->record;
}

/**
 * webkit_website_data_ref:
 * @website_data: a #WebKitWebsiteData
 *
 * Atomically increments the reference count of @website_data by one.
 * This function is MT-safe and may be called from any thread.
 *
 * Returns: The passed #WebKitWebsiteData
 *
 * Since: 2.16
 */
WebKitWebsiteData* webkit_website_data_ref(WebKitWebsiteData* websiteData)
{
    g_return_val_if_fail(websiteData, nullptr);

    g_atomic_int_inc(&websiteData->referenceCount);
    return websiteData;
}

/**
 * webkit_website_data_unref:
 * @website_data: A #WebKitWebsiteData
 *
 * Atomically decrements the reference count of @website_data by one.
 * If the reference count drops to 0, all memory allocated by
 * #WebKitWebsiteData is released. This function is MT-safe and may be
 * called from any thread.
 *
 * Since: 2.16
 */
void webkit_website_data_unref(WebKitWebsiteData* websiteData)
{
    g_return_if_fail(websiteData);

    if (g_atomic_int_dec_and_test(&websiteData->referenceCount)) {
        websiteData->~WebKitWebsiteData();
        fastFree(websiteData);
    }
}

/**
 * webkit_website_data_get_name:
 * @website_data: a #WebKitWebsiteData
 *
 * Gets the name of #WebKitWebsiteData. This is the website name, normally represented by
 * a domain or host name. All local documents are grouped in the same #WebKitWebsiteData using
 * the name "Local files".
 *
 * Returns: the website name of @website_data.
 *
 * Since: 2.16
 */
const char* webkit_website_data_get_name(WebKitWebsiteData* websiteData)
{
    g_return_val_if_fail(websiteData, nullptr);

    if (websiteData->displayName.isNull()) {
        if (websiteData->record.displayName == "Local documents on your computer")
            websiteData->displayName = _("Local files");
        else
            websiteData->displayName = websiteData->record.displayName.utf8();
    }
    return websiteData->displayName.data();
}

/**
 * webkit_website_data_get_types:
 * @website_data: a #WebKitWebsiteData
 *
 * Gets the types of data stored in the client for a #WebKitWebsiteData. These are the
 * types actually present, not the types queried with webkit_website_data_manager_fetch().
 *
 * Returns: a bitmask of #WebKitWebsiteDataTypes in @website_data
 *
 * Since: 2.16
 */
WebKitWebsiteDataTypes webkit_website_data_get_types(WebKitWebsiteData* websiteData)
{
    g_return_val_if_fail(websiteData, static_cast<WebKitWebsiteDataTypes>(0));

    return toWebKitWebsiteDataTypes(websiteData->record.types);
}

/**
 * webkit_website_data_get_size:
 * @website_data: a #WebKitWebsiteData
 * @types: a bitmask  of #WebKitWebsiteDataTypes
 *
 * Gets the size of the data of types @types in a #WebKitWebsiteData.
 * Note that currently the data size is only known for %WEBKIT_WEBSITE_DATA_DISK_CACHE data type
 * so for all other types 0 will be returned.
 *
 * Returns: the size of @website_data for the given @types.
 *
 * Since: 2.16
 */
guint64 webkit_website_data_get_size(WebKitWebsiteData* websiteData, WebKitWebsiteDataTypes types)
{
    g_return_val_if_fail(websiteData, 0);

    if (!types || !websiteData->record.size)
        return 0;

    guint64 totalSize = 0;
    for (auto type : websiteData->record.size->typeSizes.keys()) {
        if (type & types)
            totalSize += websiteData->record.size->typeSizes.get(type);
    }

    return totalSize;
}
