/*
 * Copyright (C) 2012 Samsung Electronics
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

#ifndef ewk_context_private_h
#define ewk_context_private_h

#include "DownloadManagerEfl.h"
#include "WKAPICast.h"
#include "WKRetainPtr.h"
#include "WebContext.h"
#include "ewk_context.h"
#include "ewk_object_private.h"

using namespace WebKit;

class EwkCookieManager;
class EwkFaviconDatabase;

namespace WebKit {
class ContextHistoryClientEfl;
class RequestManagerClientEfl;
#if ENABLE(BATTERY_STATUS)
class BatteryProvider;
#endif
#if ENABLE(NETWORK_INFO)
class NetworkInfoProvider;
#endif
}

class EwkContext : public EwkObject {
public:
    EWK_OBJECT_DECLARE(EwkContext)

    static PassRefPtr<EwkContext> create(PassRefPtr<WebContext> context);
    static PassRefPtr<EwkContext> create();
    static PassRefPtr<EwkContext> create(const String& injectedBundlePath);

    static PassRefPtr<EwkContext> defaultContext();

    ~EwkContext();

    EwkCookieManager* cookieManager();

    EwkDatabaseManager* databaseManager();

    bool setFaviconDatabaseDirectoryPath(const String& databaseDirectory);
    EwkFaviconDatabase* faviconDatabase();

    EwkStorageManager* storageManager() const;

    WebKit::RequestManagerClientEfl* requestManager();

    void addVisitedLink(const String& visitedURL);

    void setCacheModel(Ewk_Cache_Model);

    Ewk_Cache_Model cacheModel() const;

    PassRefPtr<WebContext> webContext() { return m_context; }

    WebKit::DownloadManagerEfl* downloadManager() const;

    WebKit::ContextHistoryClientEfl* historyClient();

#if ENABLE(NETSCAPE_PLUGIN_API)
    void setAdditionalPluginPath(const String&);
#endif

    void clearResourceCache();

private:
    explicit EwkContext(PassRefPtr<WebContext>);

    void ensureFaviconDatabase();

    RefPtr<WebContext> m_context;

    OwnPtr<EwkCookieManager> m_cookieManager;
    OwnPtr<EwkDatabaseManager> m_databaseManager;
    OwnPtr<EwkFaviconDatabase> m_faviconDatabase;
    OwnPtr<EwkStorageManager> m_storageManager;
#if ENABLE(BATTERY_STATUS)
    RefPtr<WebKit::BatteryProvider> m_batteryProvider;
#endif
#if ENABLE(NETWORK_INFO)
    RefPtr<WebKit::NetworkInfoProvider> m_networkInfoProvider;
#endif
    OwnPtr<WebKit::DownloadManagerEfl> m_downloadManager;
    OwnPtr<WebKit::RequestManagerClientEfl> m_requestManagerClient;

    OwnPtr<WebKit::ContextHistoryClientEfl> m_historyClient;
};

#endif // ewk_context_private_h
