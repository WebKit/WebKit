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
#include "ewk_context.h"

class Ewk_Url_Scheme_Request;
class Ewk_Cookie_Manager;
class Ewk_Favicon_Database;

namespace WebKit {
class ContextHistoryClientEfl;
class RequestManagerClientEfl;
#if ENABLE(BATTERY_STATUS)
class BatteryProvider;
#endif
#if ENABLE(NETWORK_INFO)
class NetworkInfoProvider;
#endif
#if ENABLE(VIBRATION)
class VibrationProvider;
#endif
}

class Ewk_Context : public RefCounted<Ewk_Context> {
public:
    static PassRefPtr<Ewk_Context> create(WKContextRef context);
    static PassRefPtr<Ewk_Context> create();
    static PassRefPtr<Ewk_Context> create(const String& injectedBundlePath);

    static PassRefPtr<Ewk_Context> defaultContext();

    ~Ewk_Context();

    Ewk_Cookie_Manager* cookieManager();

    Ewk_Favicon_Database* faviconDatabase();

    WebKit::RequestManagerClientEfl* requestManager();

#if ENABLE(VIBRATION)
    PassRefPtr<WebKit::VibrationProvider> vibrationProvider();
#endif

    void addVisitedLink(const String& visitedURL);

    void setCacheModel(Ewk_Cache_Model);

    Ewk_Cache_Model cacheModel() const;

    WKContextRef wkContext();

    void urlSchemeRequestReceived(Ewk_Url_Scheme_Request*);

    WebKit::DownloadManagerEfl* downloadManager() const;

    WebKit::ContextHistoryClientEfl* historyClient();

private:
    explicit Ewk_Context(WKContextRef);

    WKRetainPtr<WKContextRef> m_context;

    OwnPtr<Ewk_Cookie_Manager> m_cookieManager;
    OwnPtr<Ewk_Favicon_Database> m_faviconDatabase;
#if ENABLE(BATTERY_STATUS)
    RefPtr<WebKit::BatteryProvider> m_batteryProvider;
#endif
#if ENABLE(NETWORK_INFO)
    RefPtr<WebKit::NetworkInfoProvider> m_networkInfoProvider;
#endif
#if ENABLE(VIBRATION)
    RefPtr<WebKit::VibrationProvider> m_vibrationProvider;
#endif
    OwnPtr<WebKit::DownloadManagerEfl> m_downloadManager;
    OwnPtr<WebKit::RequestManagerClientEfl> m_requestManagerClient;

    OwnPtr<WebKit::ContextHistoryClientEfl> m_historyClient;
};

#endif // ewk_context_private_h
