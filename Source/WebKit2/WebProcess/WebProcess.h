/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#ifndef WebProcess_h
#define WebProcess_h

#include "CacheModel.h"
#include "ChildProcess.h"
#include "DownloadManager.h"
#include "DrawingArea.h"
#include "EventDispatcher.h"
#include "MessageReceiverMap.h"
#include "PluginInfoStore.h"
#include "ResourceCachesToClear.h"
#include "SandboxExtension.h"
#include "SharedMemory.h"
#include "TextCheckerState.h"
#include "VisitedLinkTable.h"
#include "WebConnectionToUIProcess.h"
#include "WebGeolocationManager.h"
#include "WebIconDatabaseProxy.h"
#include "WebPageGroupProxy.h"
#include <WebCore/LinkHash.h>
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>

#if USE(SOUP)
#include "WebSoupRequestManager.h"
#endif

#if PLATFORM(QT)
QT_BEGIN_NAMESPACE
class QNetworkAccessManager;
QT_END_NAMESPACE
#endif

#if PLATFORM(MAC)
#include <dispatch/dispatch.h>
#endif

#if ENABLE(BATTERY_STATUS)
#include "WebBatteryManager.h"
#endif

#if ENABLE(NETWORK_INFO)
#include "WebNetworkInfoManager.h"
#endif

#if ENABLE(NOTIFICATIONS) || ENABLE(LEGACY_NOTIFICATIONS)
#include "WebNotificationManager.h"
#endif

#if ENABLE(NETWORK_PROCESS)
#include "WebResourceLoadScheduler.h"
#endif

#if ENABLE(PLUGIN_PROCESS)
#include "PluginProcessConnectionManager.h"
#endif

namespace WebCore {
    class IntSize;
    class PageGroup;
#if ENABLE(WEB_INTENTS)
    class PlatformMessagePortChannel;
#endif
    class ResourceRequest;
    class ResourceResponse;
}

namespace WebKit {

class InjectedBundle;
class WebFrame;
class WebPage;
struct WebPageCreationParameters;
struct WebPageGroupData;
struct WebPreferencesStore;
struct WebProcessCreationParameters;

#if ENABLE(NETWORK_PROCESS)
class NetworkProcessConnection;
#endif

#if USE(SECURITY_FRAMEWORK)
class SecItemResponseData;
#endif

class WebProcess : public ChildProcess, private CoreIPC::Connection::QueueClient, private DownloadManager::Client {
public:
    static WebProcess& shared();

    void initialize(CoreIPC::Connection::Identifier, WebCore::RunLoop*);

    CoreIPC::Connection* connection() const { return m_connection.get(); }
    WebCore::RunLoop* runLoop() const { return m_runLoop; }

    void addMessageReceiver(CoreIPC::StringReference messageReceiverName, CoreIPC::MessageReceiver*);
    void addMessageReceiver(CoreIPC::StringReference messageReceiverName, uint64_t destinationID, CoreIPC::MessageReceiver*);
    void removeMessageReceiver(CoreIPC::StringReference messageReceiverName, uint64_t destinationID);

    WebConnectionToUIProcess* webConnectionToUIProcess() const { return m_webConnection.get(); }

    WebPage* webPage(uint64_t pageID) const;
    void createWebPage(uint64_t pageID, const WebPageCreationParameters&);
    void removeWebPage(uint64_t pageID);
    WebPage* focusedWebPage() const;

#if ENABLE(WEB_INTENTS) 
    uint64_t addMessagePortChannel(PassRefPtr<WebCore::PlatformMessagePortChannel>);
    WebCore::PlatformMessagePortChannel* messagePortChannel(uint64_t);
    void removeMessagePortChannel(uint64_t);
#endif

    InjectedBundle* injectedBundle() const { return m_injectedBundle.get(); }

    bool isSeparateProcess() const;

#if PLATFORM(MAC)
    void initializeShim();
    void initializeSandbox(const String& clientIdentifier);

#if USE(ACCELERATED_COMPOSITING)
    mach_port_t compositingRenderServerPort() const { return m_compositingRenderServerPort; }
#endif
#endif
    
    void setShouldTrackVisitedLinks(bool);
    void addVisitedLink(WebCore::LinkHash);
    bool isLinkVisited(WebCore::LinkHash) const;

    bool isPlugInAutoStartOrigin(unsigned plugInOriginhash);
    void addPlugInAutoStartOrigin(const String& pageOrigin, unsigned plugInOriginHash);

    bool fullKeyboardAccessEnabled() const { return m_fullKeyboardAccessEnabled; }

    WebFrame* webFrame(uint64_t) const;
    void addWebFrame(uint64_t, WebFrame*);
    void removeWebFrame(uint64_t);

    WebPageGroupProxy* webPageGroup(uint64_t pageGroupID);
    WebPageGroupProxy* webPageGroup(const WebPageGroupData&);
#if PLATFORM(MAC)
    pid_t presenterApplicationPid() const { return m_presenterApplicationPid; }
    bool shouldForceScreenFontSubstitution() const { return m_shouldForceScreenFontSubstitution; }
#endif
    
#if PLATFORM(QT)
    QNetworkAccessManager* networkAccessManager() { return m_networkAccessManager; }
#endif

    // Text Checking
    const TextCheckerState& textCheckerState() const { return m_textCheckerState; }

    // Geolocation
    WebGeolocationManager& geolocationManager() { return m_geolocationManager; }

#if ENABLE(BATTERY_STATUS)
    WebBatteryManager& batteryManager() { return m_batteryManager; }
#endif

#if ENABLE(NETWORK_INFO)
    WebNetworkInfoManager& networkInfoManager() { return m_networkInfoManager; }
#endif
    
#if ENABLE(NOTIFICATIONS) || ENABLE(LEGACY_NOTIFICATIONS)
    WebNotificationManager& notificationManager() { return m_notificationManager; }
#endif

    void clearResourceCaches(ResourceCachesToClear = AllResourceCaches);
    
    const String& localStorageDirectory() const { return m_localStorageDirectory; }

#if ENABLE(PLUGIN_PROCESS)
    PluginProcessConnectionManager& pluginProcessConnectionManager() { return m_pluginProcessConnectionManager; }
#endif

    EventDispatcher& eventDispatcher() { return m_eventDispatcher; }

#if USE(SOUP)
    WebSoupRequestManager& soupRequestManager() { return m_soupRequestManager; }
#endif

#if ENABLE(NETWORK_PROCESS)
    NetworkProcessConnection* networkConnection();
    void networkProcessConnectionClosed(NetworkProcessConnection*);
    bool usesNetworkProcess() const { return m_usesNetworkProcess; }
    WebResourceLoadScheduler& webResourceLoadScheduler() { return m_webResourceLoadScheduler; }
#endif

    void setCacheModel(uint32_t);

    void ensurePrivateBrowsingSession();
    void destroyPrivateBrowsingSession();

    DownloadManager& downloadManager();

private:
    WebProcess();

    // DownloadManager::Client.
    virtual void didCreateDownload() OVERRIDE;
    virtual void didDestroyDownload() OVERRIDE;

    void initializeWebProcess(const WebProcessCreationParameters&, CoreIPC::MessageDecoder&);
    void platformInitializeWebProcess(const WebProcessCreationParameters&, CoreIPC::MessageDecoder&);
    void platformTerminate();
    void registerURLSchemeAsEmptyDocument(const String&);
    void registerURLSchemeAsSecure(const String&) const;
    void setDomainRelaxationForbiddenForURLScheme(const String&) const;
    void registerURLSchemeAsLocal(const String&) const;
    void registerURLSchemeAsNoAccess(const String&) const;
    void registerURLSchemeAsDisplayIsolated(const String&) const;
    void registerURLSchemeAsCORSEnabled(const String&) const;
    void setDefaultRequestTimeoutInterval(double);
    void setAlwaysUsesComplexTextCodePath(bool);
    void setShouldUseFontSmoothing(bool);
    void userPreferredLanguagesChanged(const Vector<String>&) const;
    void fullKeyboardAccessModeChanged(bool fullKeyboardAccessEnabled);
#if PLATFORM(WIN)
    void setShouldPaintNativeControls(bool);
#endif

    void setVisitedLinkTable(const SharedMemory::Handle&);
    void visitedLinkStateChanged(const Vector<WebCore::LinkHash>& linkHashes);
    void allVisitedLinkStateChanged();

    void didAddPlugInAutoStartOrigin(unsigned plugInOriginHash);

    void platformSetCacheModel(CacheModel);
    static void calculateCacheSizes(CacheModel cacheModel, uint64_t memorySize, uint64_t diskFreeSize,
        unsigned& cacheTotalCapacity, unsigned& cacheMinDeadCapacity, unsigned& cacheMaxDeadCapacity, double& deadDecodedDataDeletionInterval,
        unsigned& pageCacheCapacity, unsigned long& urlCacheMemoryCapacity, unsigned long& urlCacheDiskCapacity);
    void platformClearResourceCaches(ResourceCachesToClear);
    void clearApplicationCache();

    void setEnhancedAccessibility(bool);
    
#if !ENABLE(PLUGIN_PROCESS)
    void getSitesWithPluginData(const Vector<String>& pluginPaths, uint64_t callbackID);
    void clearPluginSiteData(const Vector<String>& pluginPaths, const Vector<String>& sites, uint64_t flags, uint64_t maxAgeInSeconds, uint64_t callbackID);
#endif

#if ENABLE(NETWORK_PROCESS)
    void networkProcessCrashed(CoreIPC::Connection*);
#endif
#if ENABLE(PLUGIN_PROCESS)
    void pluginProcessCrashed(CoreIPC::Connection*, const String& pluginPath, uint32_t processType);
#endif

    void startMemorySampler(const SandboxExtension::Handle&, const String&, const double);
    void stopMemorySampler();

    void downloadRequest(uint64_t downloadID, uint64_t initiatingPageID, const WebCore::ResourceRequest&);
    void cancelDownload(uint64_t downloadID);
#if PLATFORM(QT)
    void startTransfer(uint64_t downloadID, const String& destination);
#endif

    void setTextCheckerState(const TextCheckerState&);
    
    void getWebCoreStatistics(uint64_t callbackID);
    void garbageCollectJavaScriptObjects();
    void setJavaScriptGarbageCollectorTimerEnabled(bool flag);

    void postInjectedBundleMessage(const CoreIPC::DataReference& messageData);

#if USE(SECURITY_FRAMEWORK)
    void secItemResponse(CoreIPC::Connection*, uint64_t requestID, const SecItemResponseData&);
#endif

    // ChildProcess
    virtual bool shouldTerminate();
    virtual void terminate();

    // CoreIPC::Connection::Client
    friend class WebConnectionToUIProcess;
    virtual void didReceiveMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::MessageDecoder&);
    virtual void didReceiveSyncMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::MessageDecoder&, OwnPtr<CoreIPC::MessageEncoder>&);
    virtual void didClose(CoreIPC::Connection*);
    virtual void didReceiveInvalidMessage(CoreIPC::Connection*, CoreIPC::StringReference messageReceiverName, CoreIPC::StringReference messageName) OVERRIDE;
#if PLATFORM(WIN)
    virtual Vector<HWND> windowsToReceiveSentMessagesWhileWaitingForSyncReply();
#endif

    // CoreIPC::Connection::QueueClient
    virtual void didReceiveMessageOnConnectionWorkQueue(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::MessageDecoder&, bool& didHandleMessage) OVERRIDE;

    // Implemented in generated WebProcessMessageReceiver.cpp
    void didReceiveWebProcessMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::MessageDecoder&);
    void didReceiveWebProcessMessageOnConnectionWorkQueue(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::MessageDecoder&, bool& didHandleMessage);

#if ENABLE(NETSCAPE_PLUGIN_API)
    void didGetPlugins(CoreIPC::Connection*, uint64_t requestID, const Vector<WebCore::PluginInfo>&);
#endif

#if ENABLE(CUSTOM_PROTOCOLS)
    void registerSchemeForCustomProtocol(const WTF::String&);
    void unregisterSchemeForCustomProtocol(const WTF::String&);
#endif

    RefPtr<CoreIPC::Connection> m_connection;
    CoreIPC::MessageReceiverMap m_messageReceiverMap;

    RefPtr<WebConnectionToUIProcess> m_webConnection;

    HashMap<uint64_t, RefPtr<WebPage> > m_pageMap;
    HashMap<uint64_t, RefPtr<WebPageGroupProxy> > m_pageGroupMap;
    RefPtr<InjectedBundle> m_injectedBundle;

    EventDispatcher m_eventDispatcher;

    bool m_inDidClose;

    WebCore::RunLoop* m_runLoop;

    // FIXME: The visited link table should not be per process.
    VisitedLinkTable m_visitedLinkTable;
    bool m_shouldTrackVisitedLinks;

    HashSet<unsigned> m_plugInAutoStartOrigins;

    bool m_hasSetCacheModel;
    CacheModel m_cacheModel;

#if USE(ACCELERATED_COMPOSITING) && PLATFORM(MAC)
    mach_port_t m_compositingRenderServerPort;
#endif
#if PLATFORM(MAC)
    pid_t m_presenterApplicationPid;
    dispatch_group_t m_clearResourceCachesDispatchGroup;
    bool m_shouldForceScreenFontSubstitution;
#endif

    bool m_fullKeyboardAccessEnabled;

#if PLATFORM(QT)
    QNetworkAccessManager* m_networkAccessManager;
#endif

    HashMap<uint64_t, WebFrame*> m_frameMap;

#if ENABLE(WEB_INTENTS)
    HashMap<uint64_t, RefPtr<WebCore::PlatformMessagePortChannel> > m_messagePortChannels;
#endif

    TextCheckerState m_textCheckerState;
    WebGeolocationManager m_geolocationManager;
#if ENABLE(BATTERY_STATUS)
    WebBatteryManager m_batteryManager;
#endif
#if ENABLE(NETWORK_INFO)
    WebNetworkInfoManager m_networkInfoManager;
#endif
#if ENABLE(NOTIFICATIONS) || ENABLE(LEGACY_NOTIFICATIONS)
    WebNotificationManager m_notificationManager;
#endif
    WebIconDatabaseProxy m_iconDatabaseProxy;
    
    String m_localStorageDirectory;

#if ENABLE(NETWORK_PROCESS)
    void ensureNetworkProcessConnection();
    RefPtr<NetworkProcessConnection> m_networkProcessConnection;
    bool m_usesNetworkProcess;
    WebResourceLoadScheduler m_webResourceLoadScheduler;
#endif

#if ENABLE(PLUGIN_PROCESS)
    PluginProcessConnectionManager m_pluginProcessConnectionManager;
#endif

#if USE(SOUP)
    WebSoupRequestManager m_soupRequestManager;
#endif

};

} // namespace WebKit

#endif // WebProcess_h
