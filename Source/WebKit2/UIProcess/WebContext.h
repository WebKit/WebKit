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
#include "PluginInfoStore.h"
#include "ProcessModel.h"
#include "VisitedLinkProvider.h"
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
class WebApplicationCacheManagerProxy;
class WebCookieManagerProxy;
class WebDatabaseManagerProxy;
class WebGeolocationManagerProxy;
class WebIconDatabase;
class WebKeyValueStorageManagerProxy;
class WebMediaCacheManagerProxy;
class WebPageGroup;
class WebPageProxy;
class WebResourceCacheManagerProxy;
struct WebProcessCreationParameters;

class WebContext : public APIObject {
public:
    static const Type APIType = TypeContext;

    static WebContext* sharedProcessContext();
    static WebContext* sharedThreadContext();

    static PassRefPtr<WebContext> create(const String& injectedBundlePath);
    virtual ~WebContext();

    static const Vector<WebContext*>& allContexts();

    void initializeInjectedBundleClient(const WKContextInjectedBundleClient*);
    void initializeHistoryClient(const WKContextHistoryClient*);
    void initializeDownloadClient(const WKContextDownloadClient*);

    ProcessModel processModel() const { return m_processModel; }
    WebProcessProxy* process() const { return m_process.get(); }

    template<typename U> bool sendToAllProcesses(const U& message);
    template<typename U> bool sendToAllProcessesRelaunchingThemIfNecessary(const U& message);
    
    void processDidFinishLaunching(WebProcessProxy*);

    // Disconnect the process from the context.
    void disconnectProcess(WebProcessProxy*);

    PassRefPtr<WebPageProxy> createWebPage(PageClient*, WebPageGroup*);

    WebProcessProxy* relaunchProcessIfNecessary();

    const String& injectedBundlePath() const { return m_injectedBundlePath; }

    DownloadProxy* download(WebPageProxy* initiatingPage, const WebCore::ResourceRequest&);

    void setInjectedBundleInitializationUserData(PassRefPtr<APIObject> userData) { m_injectedBundleInitializationUserData = userData; }
    APIObject* injectedBundleInitializationUserData() const { return m_injectedBundleInitializationUserData.get(); }

    void postMessageToInjectedBundle(const String&, APIObject*);

    // InjectedBundle client
    void didReceiveMessageFromInjectedBundle(const String&, APIObject*);
    void didReceiveSynchronousMessageFromInjectedBundle(const String&, APIObject*, RefPtr<APIObject>& returnData);

    void populateVisitedLinks();
    
    void setAdditionalPluginsDirectory(const String&);

    PluginInfoStore* pluginInfoStore() { return &m_pluginInfoStore; }
    String applicationCacheDirectory();

    void setAlwaysUsesComplexTextCodePath(bool);
    
    void registerURLSchemeAsEmptyDocument(const String&);
    void registerURLSchemeAsSecure(const String&);
    void setDomainRelaxationForbiddenForURLScheme(const String&);

    void addVisitedLink(const String&);
    void addVisitedLinkHash(WebCore::LinkHash);

    void didReceiveMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::ArgumentDecoder*);
    CoreIPC::SyncReplyMode didReceiveSyncMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::ArgumentDecoder*, CoreIPC::ArgumentEncoder*);

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

    static HashSet<String, CaseFoldingHash> pdfAndPostScriptMIMETypes();

    WebApplicationCacheManagerProxy* applicationCacheManagerProxy() const { return m_applicationCacheManagerProxy.get(); }
    WebCookieManagerProxy* cookieManagerProxy() const { return m_cookieManagerProxy.get(); }
    WebDatabaseManagerProxy* databaseManagerProxy() const { return m_databaseManagerProxy.get(); }
    WebGeolocationManagerProxy* geolocationManagerProxy() const { return m_geolocationManagerProxy.get(); }
    WebIconDatabase* iconDatabase() const { return m_iconDatabase.get(); }
    WebKeyValueStorageManagerProxy* keyValueStorageManagerProxy() const { return m_keyValueStorageManagerProxy.get(); }
    WebMediaCacheManagerProxy* mediaCacheManagerProxy() const { return m_mediaCacheManagerProxy.get(); }
    WebPluginSiteDataManager* pluginSiteDataManager() const { return m_pluginSiteDataManager.get(); }
    WebResourceCacheManagerProxy* resourceCacheManagerProxy() const { return m_resourceCacheManagerProxy.get(); }

    struct Statistics {
        unsigned wkViewCount;
        unsigned wkPageCount;
        unsigned wkFrameCount;
    };
    static Statistics& statistics();    

    void setDatabaseDirectory(const String& dir) { m_overrideDatabaseDirectory = dir; }
    void setIconDatabasePath(const String&);
    void setLocalStorageDirectory(const String& dir) { m_overrideLocalStorageDirectory = dir; }

    void ensureWebProcess();

    bool shouldTerminate(WebProcessProxy*);

    void disableProcessTermination() { m_processTerminationEnabled = false; }
    void enableProcessTermination();

    // Defaults to false.
    void setHTTPPipeliningEnabled(bool);
    bool httpPipeliningEnabled();

private:
    WebContext(ProcessModel, const String& injectedBundlePath);

    virtual Type type() const { return APIType; }

    void platformInitializeWebProcess(WebProcessCreationParameters&);
    void platformInvalidateContext();
    
    // History client
    void didNavigateWithNavigationData(uint64_t pageID, const WebNavigationDataStore& store, uint64_t frameID);
    void didPerformClientRedirect(uint64_t pageID, const String& sourceURLString, const String& destinationURLString, uint64_t frameID);
    void didPerformServerRedirect(uint64_t pageID, const String& sourceURLString, const String& destinationURLString, uint64_t frameID);
    void didUpdateHistoryTitle(uint64_t pageID, const String& title, const String& url, uint64_t frameID);

    // Plugins
    void getPlugins(bool refresh, Vector<WebCore::PluginInfo>& plugins);
    void getPluginPath(const String& mimeType, const String& urlString, String& pluginPath);
#if !ENABLE(PLUGIN_PROCESS)
    void didGetSitesWithPluginData(const Vector<String>& sites, uint64_t callbackID);
    void didClearPluginSiteData(uint64_t callbackID);
#endif
        
    // Implemented in generated WebContextMessageReceiver.cpp
    void didReceiveWebContextMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::ArgumentDecoder*);
    CoreIPC::SyncReplyMode didReceiveSyncWebContextMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::ArgumentDecoder*, CoreIPC::ArgumentEncoder*);

    static void languageChanged(void* context);
    void languageChanged();

    String databaseDirectory() const;
    String platformDefaultDatabaseDirectory() const;

    String iconDatabasePath() const;
    String platformDefaultIconDatabasePath() const;

    String localStorageDirectory() const;
    String platformDefaultLocalStorageDirectory() const;

    ProcessModel m_processModel;
    
    // FIXME: In the future, this should be one or more WebProcessProxies.
    RefPtr<WebProcessProxy> m_process;

    RefPtr<WebPageGroup> m_defaultPageGroup;

    RefPtr<APIObject> m_injectedBundleInitializationUserData;
    String m_injectedBundlePath;
    WebContextInjectedBundleClient m_injectedBundleClient;

    WebHistoryClient m_historyClient;

    PluginInfoStore m_pluginInfoStore;
    VisitedLinkProvider m_visitedLinkProvider;
        
    HashSet<String> m_schemesToRegisterAsEmptyDocument;
    HashSet<String> m_schemesToRegisterAsSecure;
    HashSet<String> m_schemesToSetDomainRelaxationForbiddenFor;

    bool m_alwaysUsesComplexTextCodePath;

    Vector<pair<String, RefPtr<APIObject> > > m_pendingMessagesToPostToInjectedBundle;

    CacheModel m_cacheModel;

    WebDownloadClient m_downloadClient;
    HashMap<uint64_t, RefPtr<DownloadProxy> > m_downloads;
    
    bool m_memorySamplerEnabled;
    double m_memorySamplerInterval;

    RefPtr<WebApplicationCacheManagerProxy> m_applicationCacheManagerProxy;
    RefPtr<WebCookieManagerProxy> m_cookieManagerProxy;
    RefPtr<WebDatabaseManagerProxy> m_databaseManagerProxy;
    RefPtr<WebGeolocationManagerProxy> m_geolocationManagerProxy;
    RefPtr<WebIconDatabase> m_iconDatabase;
    RefPtr<WebKeyValueStorageManagerProxy> m_keyValueStorageManagerProxy;
    RefPtr<WebMediaCacheManagerProxy> m_mediaCacheManagerProxy;
    RefPtr<WebPluginSiteDataManager> m_pluginSiteDataManager;
    RefPtr<WebResourceCacheManagerProxy> m_resourceCacheManagerProxy;

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
};

template<typename U> inline bool WebContext::sendToAllProcesses(const U& message)
{
    if (!m_process || !m_process->canSendMessage())
        return false;

    return m_process->send(message, 0);
}

template<typename U> bool WebContext::sendToAllProcessesRelaunchingThemIfNecessary(const U& message)
{
    relaunchProcessIfNecessary();

    return m_process->send(message, 0);
}

} // namespace WebKit

#endif // WebContext_h
