/*
 * Copyright (C) 2010, 2011, 2012 Apple Inc. All rights reserved.
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
#include "DownloadProxyMap.h"
#include "GenericCallback.h"
#include "MessageReceiver.h"
#include "MessageReceiverMap.h"
#include "PlugInAutoStartProvider.h"
#include "PluginInfoStore.h"
#include "ProcessModel.h"
#include "VisitedLinkProvider.h"
#include "WebContextConnectionClient.h"
#include "WebContextInjectedBundleClient.h"
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
class NetworkProcessProxy;
class WebApplicationCacheManagerProxy;
class WebCookieManagerProxy;
class WebDatabaseManagerProxy;
class WebGeolocationManagerProxy;
class WebIconDatabase;
class WebKeyValueStorageManagerProxy;
class WebMediaCacheManagerProxy;
class WebNotificationManagerProxy;
class WebPageGroup;
class WebPageProxy;
class WebResourceCacheManagerProxy;
struct StatisticsData;
struct WebProcessCreationParameters;
    
typedef GenericCallback<WKDictionaryRef> DictionaryCallback;

#if ENABLE(BATTERY_STATUS)
class WebBatteryManagerProxy;
#endif
#if ENABLE(NETWORK_INFO)
class WebNetworkInfoManagerProxy;
#endif
#if USE(SOUP)
class WebSoupRequestManagerProxy;
#endif
#if ENABLE(NETWORK_PROCESS)
struct NetworkProcessCreationParameters;
#endif

#if PLATFORM(MAC)
extern NSString *SchemeForCustomProtocolRegisteredNotificationName;
extern NSString *SchemeForCustomProtocolUnregisteredNotificationName;
#endif

class WebContext : public APIObject, private CoreIPC::MessageReceiver {
public:
    static const Type APIType = TypeContext;

    static PassRefPtr<WebContext> create(const String& injectedBundlePath);
    virtual ~WebContext();

    static const Vector<WebContext*>& allContexts();

    void addMessageReceiver(CoreIPC::StringReference messageReceiverName, CoreIPC::MessageReceiver*);
    void addMessageReceiver(CoreIPC::StringReference messageReceiverName, uint64_t destinationID, CoreIPC::MessageReceiver*);
    void removeMessageReceiver(CoreIPC::StringReference messageReceiverName, uint64_t destinationID);

    bool dispatchMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::MessageDecoder&);
    bool dispatchSyncMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::MessageDecoder&, OwnPtr<CoreIPC::MessageEncoder>&);

    void initializeInjectedBundleClient(const WKContextInjectedBundleClient*);
    void initializeConnectionClient(const WKContextConnectionClient*);
    void initializeHistoryClient(const WKContextHistoryClient*);
    void initializeDownloadClient(const WKContextDownloadClient*);

    void setProcessModel(ProcessModel); // Can only be called when there are no processes running.
    ProcessModel processModel() const { return m_processModel; }

    void setMaximumNumberOfProcesses(unsigned); // Can only be called when there are no processes running.
    unsigned maximumNumberOfProcesses() const { return m_webProcessCountLimit; }

    // FIXME (Multi-WebProcess): Remove. No code should assume that there is a shared process.
    WebProcessProxy* deprecatedSharedProcess();

    template<typename U> void sendToAllProcesses(const U& message);
    template<typename U> void sendToAllProcessesRelaunchingThemIfNecessary(const U& message);
    
    void processDidFinishLaunching(WebProcessProxy*);

    // Disconnect the process from the context.
    void disconnectProcess(WebProcessProxy*);

    PassRefPtr<WebPageProxy> createWebPage(PageClient*, WebPageGroup*, WebPageProxy* relatedPage = 0);

    const String& injectedBundlePath() const { return m_injectedBundlePath; }

    DownloadProxy* download(WebPageProxy* initiatingPage, const WebCore::ResourceRequest&);

    void setInjectedBundleInitializationUserData(PassRefPtr<APIObject> userData) { m_injectedBundleInitializationUserData = userData; }

    void postMessageToInjectedBundle(const String&, APIObject*);

    // InjectedBundle client
    void didReceiveMessageFromInjectedBundle(const String&, APIObject*);
    void didReceiveSynchronousMessageFromInjectedBundle(const String&, APIObject*, RefPtr<APIObject>& returnData);

    void populateVisitedLinks();

#if ENABLE(NETSCAPE_PLUGIN_API)
    void setAdditionalPluginsDirectory(const String&);

    PluginInfoStore& pluginInfoStore() { return m_pluginInfoStore; }
#endif
    String applicationCacheDirectory();

    void setAlwaysUsesComplexTextCodePath(bool);
    void setShouldUseFontSmoothing(bool);
    
    void registerURLSchemeAsEmptyDocument(const String&);
    void registerURLSchemeAsSecure(const String&);
    void setDomainRelaxationForbiddenForURLScheme(const String&);
    void registerURLSchemeAsLocal(const String&);
    void registerURLSchemeAsNoAccess(const String&);
    void registerURLSchemeAsDisplayIsolated(const String&);
    void registerURLSchemeAsCORSEnabled(const String&);

    void addVisitedLink(const String&);
    void addVisitedLinkHash(WebCore::LinkHash);

    // MessageReceiver.
    virtual void didReceiveMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::MessageDecoder&) OVERRIDE;
    virtual void didReceiveSyncMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::MessageDecoder&, OwnPtr<CoreIPC::MessageEncoder>&) OVERRIDE;

    void setCacheModel(CacheModel);
    CacheModel cacheModel() const { return m_cacheModel; }

    void setDefaultRequestTimeoutInterval(double);

    void startMemorySampler(const double interval);
    void stopMemorySampler();

#if PLATFORM(WIN)
    void setShouldPaintNativeControls(bool);
#endif
#if PLATFORM(WIN) || USE(SOUP)
    void setInitialHTTPCookieAcceptPolicy(HTTPCookieAcceptPolicy policy) { m_initialHTTPCookieAcceptPolicy = policy; }
#endif
    void setEnhancedAccessibility(bool);
    
    // Downloads.
    DownloadProxy* createDownloadProxy();
    WebDownloadClient& downloadClient() { return m_downloadClient; }

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
#if ENABLE(NETSCAPE_PLUGIN_API)
    WebPluginSiteDataManager* pluginSiteDataManager() const { return m_pluginSiteDataManager.get(); }
#endif
    WebResourceCacheManagerProxy* resourceCacheManagerProxy() const { return m_resourceCacheManagerProxy.get(); }
#if USE(SOUP)
    WebSoupRequestManagerProxy* soupRequestManagerProxy() const { return m_soupRequestManagerProxy.get(); }
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
    void setDiskCacheDirectory(const String& dir) { m_overrideDiskCacheDirectory = dir; }
    void setCookieStorageDirectory(const String& dir) { m_overrideCookieStorageDirectory = dir; }

    void allowSpecificHTTPSCertificateForHost(const WebCertificateInfo*, const String& host);

    WebProcessProxy* ensureSharedWebProcess();
    WebProcessProxy* createNewWebProcessRespectingProcessCountLimit(); // Will return an existing one if limit is met.
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


    // Network Process Management

    void setUsesNetworkProcess(bool);
    bool usesNetworkProcess() const;

#if ENABLE(NETWORK_PROCESS)
    void ensureNetworkProcess();
    NetworkProcessProxy* networkProcess() { return m_networkProcess.get(); }
    void removeNetworkProcessProxy(NetworkProcessProxy*);

    void getNetworkProcessConnection(PassRefPtr<Messages::WebProcessProxy::GetNetworkProcessConnection::DelayedReply>);
#endif


#if PLATFORM(MAC)
    static bool applicationIsOccluded() { return s_applicationIsOccluded; }
#endif

    static void willStartUsingPrivateBrowsing();
    static void willStopUsingPrivateBrowsing();

private:
    WebContext(ProcessModel, const String& injectedBundlePath);
    void platformInitialize();

    virtual Type type() const { return APIType; }

    void platformInitializeWebProcess(WebProcessCreationParameters&);
    void platformInvalidateContext();

    WebProcessProxy* createNewWebProcess();

#if ENABLE(NETWORK_PROCESS)
    void platformInitializeNetworkProcess(NetworkProcessCreationParameters&);
#endif

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
    void didReceiveWebContextMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::MessageDecoder&);
    void didReceiveSyncWebContextMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::MessageDecoder&, OwnPtr<CoreIPC::MessageEncoder>&);

    static void languageChanged(void* context);
    void languageChanged();

    String databaseDirectory() const;
    String platformDefaultDatabaseDirectory() const;

    String platformDefaultIconDatabasePath() const;

    String localStorageDirectory() const;
    String platformDefaultLocalStorageDirectory() const;

    String diskCacheDirectory() const;
    String platformDefaultDiskCacheDirectory() const;

    String cookieStorageDirectory() const;
    String platformDefaultCookieStorageDirectory() const;

#if PLATFORM(MAC)
    static void applicationBecameVisible(uint32_t, void*, uint32_t, void*, uint32_t);
    static void applicationBecameOccluded(uint32_t, void*, uint32_t, void*, uint32_t);
    static void initializeProcessSuppressionSupport();
    static void registerOcclusionNotificationHandlers();
    void registerNotificationObservers();
    void unregisterNotificationObservers();
#endif

#if ENABLE(CUSTOM_PROTOCOLS)
    void registerSchemeForCustomProtocol(const String&);
    void unregisterSchemeForCustomProtocol(const String&);
#endif

    void addPlugInAutoStartOriginHash(const String& pageOrigin, unsigned plugInOriginHash);

    CoreIPC::MessageReceiverMap m_messageReceiverMap;

    ProcessModel m_processModel;
    unsigned m_webProcessCountLimit; // The limit has no effect when process model is ProcessModelSharedSecondaryProcess.
    
    Vector<RefPtr<WebProcessProxy> > m_processes;
    bool m_haveInitialEmptyProcess;

    RefPtr<WebPageGroup> m_defaultPageGroup;

    RefPtr<APIObject> m_injectedBundleInitializationUserData;
    String m_injectedBundlePath;
    WebContextInjectedBundleClient m_injectedBundleClient;

    WebContextConnectionClient m_connectionClient;

    WebHistoryClient m_historyClient;

#if ENABLE(NETSCAPE_PLUGIN_API)
    PluginInfoStore m_pluginInfoStore;
#endif
    VisitedLinkProvider m_visitedLinkProvider;
    PlugInAutoStartProvider m_plugInAutoStartProvider;
        
    HashSet<String> m_schemesToRegisterAsEmptyDocument;
    HashSet<String> m_schemesToRegisterAsSecure;
    HashSet<String> m_schemesToSetDomainRelaxationForbiddenFor;
    HashSet<String> m_schemesToRegisterAsLocal;
    HashSet<String> m_schemesToRegisterAsNoAccess;
    HashSet<String> m_schemesToRegisterAsDisplayIsolated;
    HashSet<String> m_schemesToRegisterAsCORSEnabled;

    bool m_alwaysUsesComplexTextCodePath;
    bool m_shouldUseFontSmoothing;

    // How many times an API call was used to enable the preference.
    // The variable can be 0 when private browsing is used if it's enabled due to a persistent preference.
    static unsigned m_privateBrowsingEnterCount;

    // Messages that were posted before any pages were created.
    // The client should use initialization messages instead, so that a restarted process would get the same state.
    Vector<pair<String, RefPtr<APIObject> > > m_messagesToInjectedBundlePostedToEmptyContext;

    CacheModel m_cacheModel;

    WebDownloadClient m_downloadClient;
    
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
#if ENABLE(NETSCAPE_PLUGIN_API)
    RefPtr<WebPluginSiteDataManager> m_pluginSiteDataManager;
#endif
    RefPtr<WebResourceCacheManagerProxy> m_resourceCacheManagerProxy;
#if USE(SOUP)
    RefPtr<WebSoupRequestManagerProxy> m_soupRequestManagerProxy;
#endif

#if PLATFORM(WIN)
    bool m_shouldPaintNativeControls;
#endif
#if PLATFORM(WIN) || USE(SOUP)
    HTTPCookieAcceptPolicy m_initialHTTPCookieAcceptPolicy;
#endif

#if PLATFORM(MAC)
    RetainPtr<CFTypeRef> m_enhancedAccessibilityObserver;
    RetainPtr<CFTypeRef> m_customSchemeRegisteredObserver;
    RetainPtr<CFTypeRef> m_customSchemeUnregisteredObserver;
#endif

    String m_overrideDatabaseDirectory;
    String m_overrideIconDatabasePath;
    String m_overrideLocalStorageDirectory;
    String m_overrideDiskCacheDirectory;
    String m_overrideCookieStorageDirectory;

    bool m_processTerminationEnabled;

#if ENABLE(NETWORK_PROCESS)
    bool m_usesNetworkProcess;
    RefPtr<NetworkProcessProxy> m_networkProcess;
#endif
    
    HashMap<uint64_t, RefPtr<DictionaryCallback> > m_dictionaryCallbacks;

#if PLATFORM(MAC)
    static bool s_applicationIsOccluded;
#endif
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
// FIXME (Multi-WebProcess): WebContext doesn't track processes that have exited, so it cannot relaunch these. Perhaps this functionality won't be needed in this mode.
    if (m_processModel == ProcessModelSharedSecondaryProcess)
        ensureSharedWebProcess();
    sendToAllProcesses(message);
}

} // namespace WebKit

#endif // WebContext_h
