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

#ifndef WebContext_h
#define WebContext_h

#include "APIObject.h"
#include "PluginInfoStore.h"
#include "ProcessModel.h"
#include "VisitedLinkProvider.h"
#include "WebContextInjectedBundleClient.h"
#include "WebHistoryClient.h"
#include "WebProcessProxy.h"
#include <WebCore/LinkHash.h>
#include <wtf/Forward.h>
#include <wtf/HashSet.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

struct WKContextStatistics;

namespace WebKit {

class WebPageNamespace;
class WebPageProxy;
class WebPreferences;

class WebContext : public APIObject {
public:
    static const Type APIType = TypeContext;

    static WebContext* sharedProcessContext();
    static WebContext* sharedThreadContext();

    static PassRefPtr<WebContext> create(const WTF::String& injectedBundlePath);

    ~WebContext();

    void initializeInjectedBundleClient(const WKContextInjectedBundleClient*);
    void initializeHistoryClient(const WKContextHistoryClient*);

    ProcessModel processModel() const { return m_processModel; }
    WebProcessProxy* process() const { return m_process.get(); }

    void processDidFinishLaunching(WebProcessProxy*);

    WebPageProxy* createWebPage(WebPageNamespace*);

    void reviveIfNecessary();

    WebPageNamespace* createPageNamespace();
    void pageNamespaceWasDestroyed(WebPageNamespace*);

    void setPreferences(WebPreferences*);
    WebPreferences* preferences() const;
    void preferencesDidChange();

    const WTF::String& injectedBundlePath() const { return m_injectedBundlePath; }

    void postMessageToInjectedBundle(const WTF::String&, APIObject*);

    // InjectedBundle client
    void didReceiveMessageFromInjectedBundle(const WTF::String&, APIObject*);

    // History client
    void didNavigateWithNavigationData(WebFrameProxy*, const WebNavigationDataStore&); 
    void didPerformClientRedirect(WebFrameProxy*, const WTF::String& sourceURLString, const WTF::String& destinationURLString);
    void didPerformServerRedirect(WebFrameProxy*, const WTF::String& sourceURLString, const WTF::String& destinationURLString);
    void didUpdateHistoryTitle(WebFrameProxy*, const WTF::String& title, const WTF::String& url);
    void populateVisitedLinks();
    
    void getStatistics(WKContextStatistics* statistics);
    void setAdditionalPluginsDirectory(const WTF::String&);

    PluginInfoStore* pluginInfoStore() { return &m_pluginInfoStore; }
    WTF::String applicationCacheDirectory();
    
    void registerURLSchemeAsEmptyDocument(const WTF::String&);
    
    void addVisitedLink(const WTF::String&);
    void addVisitedLink(WebCore::LinkHash);

    void didReceiveMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::ArgumentDecoder*);

#if PLATFORM(WIN)
    void setShouldPaintNativeControls(bool);
#endif

private:
    WebContext(ProcessModel, const WTF::String& injectedBundlePath);

    virtual Type type() const { return APIType; }

    void ensureWebProcess();
    bool hasValidProcess() const { return m_process && m_process->isValid(); }
    void platformSetUpWebProcess();

    ProcessModel m_processModel;
    
    // FIXME: In the future, this should be one or more WebProcessProxies.
    RefPtr<WebProcessProxy> m_process;

    HashSet<WebPageNamespace*> m_pageNamespaces;
    RefPtr<WebPreferences> m_preferences;

    WTF::String m_injectedBundlePath;
    WebContextInjectedBundleClient m_injectedBundleClient;

    WebHistoryClient m_historyClient;

    PluginInfoStore m_pluginInfoStore;
    VisitedLinkProvider m_visitedLinkProvider;
        
    HashSet<WTF::String> m_schemesToRegisterAsEmptyDocument;
    Vector<pair<String, RefPtr<APIObject> > > m_pendingMessagesToPostToInjectedBundle;

#if PLATFORM(WIN)
    bool m_shouldPaintNativeControls;
#endif
};

} // namespace WebKit

#endif // WebContext_h
