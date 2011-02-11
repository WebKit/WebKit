/*
 * Copyright (C) 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef InspectorAgent_h
#define InspectorAgent_h

#include "CharacterData.h"
#include "Console.h"
#include "Cookie.h"
#include "Page.h"
#include "PlatformString.h"
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/ListHashSet.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>
#include <wtf/text/StringHash.h>

namespace WebCore {

class CachedResource;
class CharacterData;
class Database;
class DOMWrapperWorld;
class Document;
class DocumentLoader;
class FloatRect;
class GraphicsContext;
class HTTPHeaderMap;
class HitTestResult;
class InjectedScript;
class InjectedScriptHost;
class InspectorArray;
class InspectorBrowserDebuggerAgent;
class InspectorClient;
class InspectorConsoleAgent;
class InspectorCSSAgent;
class InspectorDOMAgent;
class InspectorDOMStorageAgent;
class InspectorDOMStorageResource;
class InspectorDatabaseAgent;
class InspectorDatabaseResource;
class InspectorDebuggerAgent;
class InspectorFrontend;
class InspectorFrontendClient;
class InspectorObject;
class InspectorProfilerAgent;
class InspectorResourceAgent;
class InspectorRuntimeAgent;
class InspectorState;
class InspectorStorageAgent;
class InspectorTimelineAgent;
class InspectorValue;
class InspectorWorkerResource;
class IntRect;
class KURL;
class Node;
class Page;
class ResourceRequest;
class ResourceResponse;
class ResourceError;
class ScriptArguments;
class ScriptCallStack;
class ScriptProfile;
class SharedBuffer;
class StorageArea;

#if ENABLE(OFFLINE_WEB_APPLICATIONS)
class InspectorApplicationCacheAgent;
#endif

#if ENABLE(WEB_SOCKETS)
class WebSocketHandshakeRequest;
class WebSocketHandshakeResponse;
#endif

class InspectorAgent {
    WTF_MAKE_NONCOPYABLE(InspectorAgent);
    WTF_MAKE_FAST_ALLOCATED;
public:
    InspectorAgent(Page*, InspectorClient*);
    virtual ~InspectorAgent();

    InspectorClient* inspectorClient() { return m_client; }
    InjectedScriptHost* injectedScriptHost() { return m_injectedScriptHost.get(); }

    void inspectedPageDestroyed();

    bool enabled() const;

    Page* inspectedPage() const { return m_inspectedPage; }
    KURL inspectedURL() const;
    KURL inspectedURLWithoutFragment() const;
    void reloadPage(bool ignoreCache);
    void showConsole();

    void restoreInspectorStateFromCookie(const String& inspectorCookie);

    void highlight(Node*);
    void hideHighlight();
    void inspect(Node*);
    void highlightDOMNode(long nodeId);
    void hideDOMNodeHighlight() { hideHighlight(); }

    void highlightFrame(unsigned long frameId);
    void hideFrameHighlight() { hideHighlight(); }

    void setFrontend(InspectorFrontend*);
    InspectorFrontend* frontend() const { return m_frontend; }
    void disconnectFrontend();

    InspectorResourceAgent* resourceAgent();

    InspectorAgent* inspectorAgent() { return this; }
    InspectorConsoleAgent* consoleAgent() { return m_consoleAgent.get(); }
    InspectorCSSAgent* cssAgent() { return m_cssAgent.get(); }
    InspectorDOMAgent* domAgent() { return m_domAgent.get(); }
    InjectedScriptHost* injectedScriptAgent() { return m_injectedScriptHost.get(); }
    InspectorRuntimeAgent* runtimeAgent() { return m_runtimeAgent.get(); }
    InspectorTimelineAgent* timelineAgent() { return m_timelineAgent.get(); }
#if ENABLE(DATABASE)
    InspectorDatabaseAgent* databaseAgent() { return m_databaseAgent.get(); }
#endif
#if ENABLE(DOM_STORAGE)
    InspectorDOMStorageAgent* domStorageAgent() { return m_domStorageAgent.get(); }
#endif
#if ENABLE(JAVASCRIPT_DEBUGGER)
    InspectorBrowserDebuggerAgent* browserDebuggerAgent() const { return m_browserDebuggerAgent.get(); }
    InspectorDebuggerAgent* debuggerAgent() const { return m_debuggerAgent.get(); }
    InspectorProfilerAgent* profilerAgent() const { return m_profilerAgent.get(); }
#endif
#if ENABLE(OFFLINE_WEB_APPLICATIONS)
    InspectorApplicationCacheAgent* applicationCacheAgent() { return m_applicationCacheAgent.get(); }
#endif

    bool handleMousePress();
    bool searchingForNodeInPage() const;
    void mouseDidMoveOverElement(const HitTestResult&, unsigned modifierFlags);

    void didClearWindowObjectInWorld(Frame*, DOMWrapperWorld*);

    void didCommitLoad(DocumentLoader*);

    void startTimelineProfiler();
    void stopTimelineProfiler();

    void getCookies(RefPtr<InspectorArray>* cookies, WTF::String* cookiesString);
    void deleteCookie(const String& cookieName, const String& domain);

    void domContentLoadedEventFired(DocumentLoader*, const KURL&);
    void loadEventFired(DocumentLoader*, const KURL&);

#if ENABLE(WORKERS)
    enum WorkerAction { WorkerCreated, WorkerDestroyed };

    void postWorkerNotificationToFrontend(const InspectorWorkerResource&, WorkerAction);
    void didCreateWorker(intptr_t, const String& url, bool isSharedWorker);
    void didDestroyWorker(intptr_t);
#endif

#if ENABLE(DATABASE)
    void didOpenDatabase(PassRefPtr<Database>, const String& domain, const String& name, const String& version);
#endif

#if ENABLE(DOM_STORAGE)
    void didUseDOMStorage(StorageArea*, bool isLocalStorage, Frame*);
#endif

#if ENABLE(WEB_SOCKETS)
    void didCreateWebSocket(unsigned long identifier, const KURL& requestURL, const KURL& documentURL);
    void willSendWebSocketHandshakeRequest(unsigned long identifier, const WebSocketHandshakeRequest&);
    void didReceiveWebSocketHandshakeResponse(unsigned long identifier, const WebSocketHandshakeResponse&);
    void didCloseWebSocket(unsigned long identifier);
#endif

    bool hasFrontend() const { return m_frontend; }

    void drawNodeHighlight(GraphicsContext&) const;
    void openInInspectedWindow(const String& url);
    void drawElementTitle(GraphicsContext&, const IntRect& boundingBox, const FloatRect& overlayRect, WebCore::Settings*) const;

#if ENABLE(JAVASCRIPT_DEBUGGER)
    bool isRecordingUserInitiatedProfile() const;
    void startProfiling() { startUserInitiatedProfiling(); }
    void startUserInitiatedProfiling();
    void stopProfiling() { stopUserInitiatedProfiling(); }
    void stopUserInitiatedProfiling();
    void enableProfiler();
    void disableProfiler();
    bool profilerEnabled() const;

    void showAndEnableDebugger();
    void enableDebugger() { enableDebugger(false); }
    void enableDebugger(bool eraseStickyBreakpoints);
    void disableDebugger();
    bool debuggerEnabled() const { return m_debuggerAgent; }
    void resume();
#endif

    // Generic code called from custom implementations.
    void evaluateForTestInFrontend(long testCallId, const String& script);

    void addScriptToEvaluateOnLoad(const String& source);
    void removeAllScriptsToEvaluateOnLoad();
    void setInspectorExtensionAPI(const String& source);

    InspectorState* state() { return m_state.get(); }

    // InspectorAgent API
    void getInspectorState(RefPtr<InspectorObject>* state);
    void setMonitoringXHREnabled(bool enabled, bool* newState);
    void populateScriptObjects();
    // Following are used from InspectorBackend and internally.
    void setSearchingForNode(bool enabled, bool* newState);
    void didEvaluateForTestInFrontend(long callId, const String& jsonResult);

    void setUserAgentOverride(const String& userAgent);
    void applyUserAgentOverride(String* userAgent) const;

private:
    void showPanel(const String& panel);
    void pushDataCollectedOffline();
    void restoreDebugger(bool eraseStickyBreakpoints);
    enum ProfilerRestoreAction {
        ProfilerRestoreNoAction = 0,
        ProfilerRestoreResetAgent = 1
    };
    void restoreProfiler(ProfilerRestoreAction);
    void unbindAllResources();
    void setSearchingForNode(bool enabled);

    void releaseFrontendLifetimeAgents();
    void createFrontendLifetimeAgents();

#if ENABLE(JAVASCRIPT_DEBUGGER)
    void toggleRecordButton(bool);
#endif

    PassRefPtr<InspectorObject> buildObjectForCookie(const Cookie&);
    PassRefPtr<InspectorArray> buildArrayForCookies(ListHashSet<Cookie>&);

    void focusNode();
    bool isMainResourceLoader(DocumentLoader*, const KURL& requestUrl);

    Page* m_inspectedPage;
    InspectorClient* m_client;
    InspectorFrontend* m_frontend;
    OwnPtr<InspectorCSSAgent> m_cssAgent;
    OwnPtr<InspectorDOMAgent> m_domAgent;

#if ENABLE(DATABASE)
    OwnPtr<InspectorDatabaseAgent> m_databaseAgent;
#endif

#if ENABLE(DOM_STORAGE)
    OwnPtr<InspectorDOMStorageAgent> m_domStorageAgent;
#endif

    OwnPtr<InspectorTimelineAgent> m_timelineAgent;
    OwnPtr<InspectorState> m_state;

#if ENABLE(OFFLINE_WEB_APPLICATIONS)
    OwnPtr<InspectorApplicationCacheAgent> m_applicationCacheAgent;
#endif

    RefPtr<Node> m_highlightedNode;
    RefPtr<Node> m_nodeToFocus;
    RefPtr<InspectorResourceAgent> m_resourceAgent;
    OwnPtr<InspectorRuntimeAgent> m_runtimeAgent;

#if ENABLE(DATABASE)
    typedef HashMap<int, RefPtr<InspectorDatabaseResource> > DatabaseResourcesMap;
    DatabaseResourcesMap m_databaseResources;
#endif
#if ENABLE(DOM_STORAGE)
    typedef HashMap<int, RefPtr<InspectorDOMStorageResource> > DOMStorageResourcesMap;
    DOMStorageResourcesMap m_domStorageResources;
#endif

    RefPtr<InjectedScriptHost> m_injectedScriptHost;
    OwnPtr<InspectorConsoleAgent> m_consoleAgent;

    Vector<pair<long, String> > m_pendingEvaluateTestCommands;
    String m_requiredPanel;
    Vector<String> m_scriptsToEvaluateOnLoad;
    String m_inspectorExtensionAPI;
#if ENABLE(JAVASCRIPT_DEBUGGER)
    OwnPtr<InspectorDebuggerAgent> m_debuggerAgent;
    OwnPtr<InspectorBrowserDebuggerAgent> m_browserDebuggerAgent;
    OwnPtr<InspectorProfilerAgent> m_profilerAgent;
#endif
    String m_userAgentOverride;
#if ENABLE(WORKERS)
    typedef HashMap<intptr_t, RefPtr<InspectorWorkerResource> > WorkersMap;

    WorkersMap m_workers;
#endif
};

} // namespace WebCore

#endif // !defined(InspectorAgent_h)
