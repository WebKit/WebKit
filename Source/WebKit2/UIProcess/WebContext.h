/*
 * Copyright (C) 2010, 2011 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebContext_h
#define WebContext_h

#include "APIObject.h"
#include "GenericCallback.h"
#include "PluginInfoStore.h"
#include "ProcessModel.h"
#include "VisitedLinkProvider.h"
#include "WebContextInjectedBundleClient.h"
#include "WebContextConnectionClient.h"
#include "WebDownloadClient.h"
#include "WebHistoryClient.h"
#include "WebProcessProxy.h"
#include <WebCore/LinkHash.h>
#include <wtf/Forward.h>
#include <wtf/HashSet.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

namespace WebKit {

class DownloadProxy;
class WebApplicationCacheManagerProxy;
#if ENABLE(BATTERY_STATUS)
class WebBatteryManagerProxy;
#endif
class WebCookieManagerProxy;
class WebDatabaseManagerProxy;
class WebGeolocationManagerProxy;
class WebIconDatabase;
class WebKeyValueStorageManagerProxy;
class WebMediaCacheManagerProxy;
#if ENABLE(NETWORK_INFO)
class WebNetworkInfoManagerProxy;
#endif
class WebNotificationManagerProxy;
class WebPageGroup;
class WebPageProxy;
class WebResourceCacheManagerProxy;
#if USE(SOUP)
class WebSoupRequestManagerProxy;
#endif
#if ENABLE(VIBRATION)
class WebVibrationProxy;
#endif
struct StatisticsData;
struct WebProcessCreationParameters;
    
typedef GenericCallback<WKDictionaryRef> DictionaryCallback;

class WebContext : public APIObject {
public:
    static const Type APIType = TypeContext;

    static PassRefPtr<WebContext> create(const String& injectedBundlePath);
    virtual ~WebContext();

    static const Vector<WebContext*>& allContexts();

    void initializeInjectedBundleClient(const WKContextInjectedBundleClient*);
    void initializeConnectionClient(const WKContextConnectionClient*);
    void initializeHistoryClient(const WKContextHistoryClient*);
    void initializeDownloadClient(const WKContextDownloadClient*);

    ProcessModel processModel() const { return m_processModel; }

    // FIXME (Multi-WebProcess): Remove. No code should assume that there is a shared process.
    WebProcessProxy* deprecatedSharedProcess();

    template<typename U> void sendToAllProcesses(const U& message);
    template<typename U> void sendToAllProcessesRelaunchingThemIfNecessary(const U& message);
    
    void processDidFinishLaunching(WebProcessProxy*);

    // Disconnect the process from the context.
    void disconnectProcess(WebProcessProxy*);

    PassRefPtr<WebPageProxy> createWebPage(PageClient*, WebPageGroup*);

    WebProcessProxy* relaunchProcessIfNecessary();

    const String& injectedBundlePath() const { return m_injectedBundlePath; }

    DownloadProxy* download(WebPageProxy* initiatingPage, const WebCore::ResourceRequest&);

    void setInjectedBundleInitializationUserData(PassRefPtr<APIObject> userData) { m_injectedBundleInitializationUserData = userData; }

    void postMessageToInjectedBundle(const String&, APIObject*);

    // InjectedBundle client
    void didReceiveMessageFromInjectedBundle(const String&, APIObject*);
    void didReceiveSynchronousMessageFromInjectedBundle(const String&, APIObject*, RefPtr<APIObject>& returnData);

    void populateVisitedLinks();
    
    void setAdditionalPluginsDirectory(const String&);

    PluginInfoStore& pluginInfoStore() { return m_pluginInfoStore; }
    String applicationCacheDirectory();

    void setAlwaysUsesComplexTextCodePath(bool);
    void setShouldUseFontSmoothing(bool);
    
    void registerURLSchemeAsEmptyDocument(const String&);
    void registerURLSchemeAsSecure(const String&);
    void setDomainRelaxationForbiddenForURLScheme(const String&);

    void addVisitedLink(const String&);
    void addVisitedLinkHash(WebCore::LinkHash);

    void didReceiveMessage(WebProcessProxy*, CoreIPC::MessageID, CoreIPC::ArgumentDecoder*);
    void didReceiveSyncMessage(WebProcessProxy*, CoreIPC::MessageID, CoreIPC::ArgumentDecoder*, OwnPtr<CoreIPC::ArgumentEncoder>&);

    void setCacheModel(CacheModel);
    CacheModel cacheModel() const { return m_cacheModel; }

    void setDefaultRequestTimeoutInterval(double);

    void startMemorySampler(const double interval);
    void stopMemorySampler();

#if PLATFORM(WIN)
    void setShouldPaintNativeControls(bool);

    void setInitialHTTPCookieAcceptPolicy(HTTPCookieAcceptPolicy policy) { m_initialHTTPCookieAcceptPolicy = policy; }
#endif

    void setEnhancedAccessibility(bool);
    
    // Downloads.
    DownloadProxy* createDownloadProxy();
    WebDownloadClient& downloadClient() { return m_downloadClient; }
    void downloadFinished(DownloadProxy*);

    WebHistoryClient& historyClient() { return m_historyClient; }

    static HashSet<String, CaseFoldingHash> pdfAndPostScriptMIMETypes();

    WebApplicationCacheManagerProxy* applicationCacheManagerProxy() const { return m_applicationCacheManagerProxy.get(); }
#if ENABLE(BATTERY_STATUS)
    WebBatteryManagerProxy* batteryManagerProxy() const { return m_batteryManagerProxy.get(); }
#endif
    WebCookieManagerProxy* cookieManagerProxy() const { return m_cookieManagerProxy.get(); }
#if ENABLE(SQL_DATABASE)
    WebDatabaseManagerProxy* databaseManagerProxy() const { return m_databaseManagerProxy.get(); }
#endif
    WebGeolocationManagerProxy* geolocationManagerProxy() const { return m_geolocationManagerProxy.get(); }
    WebIconDatabase* iconDatabase() const { return m_iconDatabase.get(); }
    WebKeyValueStorageManagerProxy* keyValueStorageManagerProxy() const { return m_keyValueStorageManagerProxy.get(); }
    WebMediaCacheManagerProxy* mediaCacheManagerProxy() const { return m_mediaCacheManagerProxy.get(); }
#if ENABLE(NETWORK_INFO)
    WebNetworkInfoManagerProxy* networkInfoManagerProxy() const { return m_networkInfoManagerProxy.get(); }
#endif
    WebNotificationManagerProxy* notificationManagerProxy() const { return m_notificationManagerProxy.get(); }
    WebPluginSiteDataManager* pluginSiteDataManager() const { return m_pluginSiteDataManager.get(); }
    WebResourceCacheManagerProxy* resourceCacheManagerProxy() const { return m_resourceCacheManagerProxy.get(); }
#if USE(SOUP)
    WebSoupRequestManagerProxy* soupRequestManagerProxy() const { return m_soupRequestManagerProxy.get(); }
#endif
#if ENABLE(VIBRATION)
    WebVibrationProxy* vibrationProxy() const { return m_vibrationProxy.get(); }
#endif

    struct Statistics {
        unsigned wkViewCount;
        unsigned wkPageCount;
        unsigned wkFrameCount;
    };
    static Statistics& statistics();    

    void setDatabaseDirectory(const String& dir) { m_overrideDatabaseDirectory = dir; }
    void setIconDatabasePath(const String&);
    String iconDatabasePath() const;
    void setLocalStorageDirectory(const String& dir) { m_overrideLocalStorageDirectory = dir; }

    void ensureSharedWebProcess();
    PassRefPtr<WebProcessProxy> createNewWebProcess();
    void warmInitialProcess();

    bool shouldTerminate(WebProcessProxy*);

    void disableProcessTermination() { m_processTerminationEnabled = false; }
    void enableProcessTermination();

    // Defaults to false.
    void setHTTPPipeliningEnabled(bool);
    bool httpPipeliningEnabled() const;
    
    void getWebCoreStatistics(PassRefPtr<DictionaryCallback>);
    void garbageCollectJavaScriptObjects();
    void setJavaScriptGarbageCollectorTimerEnabled(bool flag);

#if PLATFORM(MAC)
    static bool omitPDFSupport();
#endif

    void fullKeyboardAccessModeChanged(bool fullKeyboardAccessEnabled);

    void textCheckerStateChanged();

private:
    WebContext(ProcessModel, const String& injectedBundlePath);

    virtual Type type() const { return APIType; }

    void platformInitializeWebProcess(WebProcessCreationParameters&);
    void platformInvalidateContext();
    
#if PLATFORM(MAC)
    void getPasteboardTypes(const String& pasteboardName, Vector<String>& pasteboardTypes);
    void getPasteboardPathnamesForType(const String& pasteboardName, const String& pasteboardType, Vector<String>& pathnames);
    void getPasteboardStringForType(const String& pasteboardName, const String& pasteboardType, String&);
    void getPasteboardBufferForType(const String& pasteboardName, const String& pasteboardType, SharedMemory::Handle&, uint64_t& size);
    void pasteboardCopy(const String& fromPasteboard, const String& toPasteboard);
    void getPasteboardChangeCount(const String& pasteboardName, uint64_t& changeCount);
    void getPasteboardUniqueName(String& pasteboardName);
    void getPasteboardColor(const String& pasteboardName, WebCore::Color&);
    void getPasteboardURL(const String& pasteboardName, WTF::String&);
    void addPasteboardTypes(const String& pasteboardName, const Vector<String>& pasteboardTypes);
    void setPasteboardTypes(const String& pasteboardName, const Vector<String>& pasteboardTypes);
    void setPasteboardPathnamesForType(const String& pasteboardName, const String& pasteboardType, const Vector<String>& pathnames);
    void setPasteboardStringForType(const String& pasteboardName, const String& pasteboardType, const String&);
    void setPasteboardBufferForType(const String& pasteboardName, const String& pasteboardType, const SharedMemory::Handle&, uint64_t size);
#endif

#if !PLATFORM(MAC)
    // FIXME: This a dummy message, to avoid breaking the build for platforms that don't require
    // any synchronous messages, and should be removed when <rdar://problem/8775115> is fixed.
    void dummy(bool&);
#endif

    void didGetWebCoreStatistics(const StatisticsData&, uint64_t callbackID);
        
    // Implemented in generated WebContextMessageReceiver.cpp
    void didReceiveWebContextMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::ArgumentDecoder*);
    void didReceiveSyncWebContextMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::ArgumentDecoder*, OwnPtr<CoreIPC::ArgumentEncoder>&);

    static void languageChanged(void* context);
    void languageChanged();

    String databaseDirectory() const;
    String platformDefaultDatabaseDirectory() const;

    String platformDefaultIconDatabasePath() const;

    String localStorageDirectory() const;
    String platformDefaultLocalStorageDirectory() const;

    ProcessModel m_processModel;
    
    Vector<RefPtr<WebProcessProxy> > m_processes;

    RefPtr<WebPageGroup> m_defaultPageGroup;

    RefPtr<APIObject> m_injectedBundleInitializationUserData;
    String m_injectedBundlePath;
    WebContextInjectedBundleClient m_injectedBundleClient;

    WebContextConnectionClient m_connectionClient;
    
    WebHistoryClient m_historyClient;

    PluginInfoStore m_pluginInfoStore;
    VisitedLinkProvider m_visitedLinkProvider;
        
    HashSet<String> m_schemesToRegisterAsEmptyDocument;
    HashSet<String> m_schemesToRegisterAsSecure;
    HashSet<String> m_schemesToSetDomainRelaxationForbiddenFor;

    bool m_alwaysUsesComplexTextCodePath;
    bool m_shouldUseFontSmoothing;

    Vector<pair<String, RefPtr<APIObject> > > m_pendingMessagesToPostToInjectedBundle;

    CacheModel m_cacheModel;

    WebDownloadClient m_downloadClient;
    HashMap<uint64_t, RefPtr<DownloadProxy> > m_downloads;
    
    bool m_memorySamplerEnabled;
    double m_memorySamplerInterval;

    RefPtr<WebApplicationCacheManagerProxy> m_applicationCacheManagerProxy;
#if ENABLE(BATTERY_STATUS)
    RefPtr<WebBatteryManagerProxy> m_batteryManagerProxy;
#endif
    RefPtr<WebCookieManagerProxy> m_cookieManagerProxy;
#if ENABLE(SQL_DATABASE)
    RefPtr<WebDatabaseManagerProxy> m_databaseManagerProxy;
#endif
    RefPtr<WebGeolocationManagerProxy> m_geolocationManagerProxy;
    RefPtr<WebIconDatabase> m_iconDatabase;
    RefPtr<WebKeyValueStorageManagerProxy> m_keyValueStorageManagerProxy;
    RefPtr<WebMediaCacheManagerProxy> m_mediaCacheManagerProxy;
#if ENABLE(NETWORK_INFO)
    RefPtr<WebNetworkInfoManagerProxy> m_networkInfoManagerProxy;
#endif
    RefPtr<WebNotificationManagerProxy> m_notificationManagerProxy;
    RefPtr<WebPluginSiteDataManager> m_pluginSiteDataManager;
    RefPtr<WebResourceCacheManagerProxy> m_resourceCacheManagerProxy;
#if USE(SOUP)
    RefPtr<WebSoupRequestManagerProxy> m_soupRequestManagerProxy;
#endif
#if ENABLE(VIBRATION)
    RefPtr<WebVibrationProxy> m_vibrationProxy;
#endif

#if PLATFORM(WIN)
    bool m_shouldPaintNativeControls;
    HTTPCookieAcceptPolicy m_initialHTTPCookieAcceptPolicy;
#endif

#if PLATFORM(MAC)
    RetainPtr<CFTypeRef> m_enhancedAccessibilityObserver;
#endif

    String m_overrideDatabaseDirectory;
    String m_overrideIconDatabasePath;
    String m_overrideLocalStorageDirectory;

    bool m_processTerminationEnabled;
    
    HashMap<uint64_t, RefPtr<DictionaryCallback> > m_dictionaryCallbacks;
};

template<typename U> inline void WebContext::sendToAllProcesses(const U& message)
{
    size_t processCount = m_processes.size();
    for (size_t i = 0; i < processCount; ++i) {
        WebProcessProxy* process = m_processes[i].get();
        if (process->canSendMessage())
            process->send(message, 0);
    }
}

template<typename U> void WebContext::sendToAllProcessesRelaunchingThemIfNecessary(const U& message)
{
    relaunchProcessIfNecessary();
    sendToAllProcesses(message);
}

} // namespace WebKit

#endif // WebContext_h
