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

#include "WKAPICast.h"
#include "WKRetainPtr.h"
#include "ewk_context_history_client_private.h"

class Ewk_Download_Job;
class Ewk_Url_Scheme_Request;
class Ewk_Cookie_Manager;
class Ewk_Favicon_Database;
#if ENABLE(BATTERY_STATUS)
class BatteryProvider;
#endif
#if ENABLE(NETWORK_INFO)
class NetworkInfoProvider;
#endif
#if ENABLE(VIBRATION)
class VibrationProvider;
#endif

class Ewk_Context : public RefCounted<Ewk_Context> {
public:
    static PassRefPtr<Ewk_Context> create(WKContextRef context)
    {
        return adoptRef(new Ewk_Context(context));
    }

    static PassRefPtr<Ewk_Context> create()
    {
        return adoptRef(new Ewk_Context(adoptWK(WKContextCreate()).get()));
    }

    static PassRefPtr<Ewk_Context> create(const String& injectedBundlePath);

    static PassRefPtr<Ewk_Context> defaultContext();

    ~Ewk_Context();

    Ewk_Cookie_Manager* cookieManager();

    Ewk_Favicon_Database* faviconDatabase();

    bool registerURLScheme(const String& scheme, Ewk_Url_Scheme_Request_Cb callback, void* userData);

#if ENABLE(VIBRATION)
    PassRefPtr<VibrationProvider> vibrationProvider();
#endif

    void addVisitedLink(const String& visitedURL);

    void setCacheModel(Ewk_Cache_Model);

    Ewk_Cache_Model cacheModel() const;

    WKContextRef wkContext();

    WKSoupRequestManagerRef requestManager();

    void urlSchemeRequestReceived(Ewk_Url_Scheme_Request*);

    void addDownloadJob(Ewk_Download_Job*);
    Ewk_Download_Job* downloadJob(uint64_t downloadId);
    void removeDownloadJob(uint64_t downloadId);

    const Ewk_Context_History_Client& historyClient() const  { return m_historyClient; }
    Ewk_Context_History_Client& historyClient() { return m_historyClient; }

private:
    explicit Ewk_Context(WKContextRef);

    WKRetainPtr<WKContextRef> m_context;

    OwnPtr<Ewk_Cookie_Manager> m_cookieManager;
    OwnPtr<Ewk_Favicon_Database> m_faviconDatabase;
#if ENABLE(BATTERY_STATUS)
    RefPtr<BatteryProvider> m_batteryProvider;
#endif
#if ENABLE(NETWORK_INFO)
    RefPtr<NetworkInfoProvider> m_networkInfoProvider;
#endif
#if ENABLE(VIBRATION)
    RefPtr<VibrationProvider> m_vibrationProvider;
#endif
    HashMap<uint64_t, RefPtr<Ewk_Download_Job> > m_downloadJobs;

    WKRetainPtr<WKSoupRequestManagerRef> m_requestManager;

    typedef HashMap<String, class Ewk_Url_Scheme_Handler> URLSchemeHandlerMap;
    URLSchemeHandlerMap m_urlSchemeHandlers;

    Ewk_Context_History_Client m_historyClient;
};

#endif // ewk_context_private_h
