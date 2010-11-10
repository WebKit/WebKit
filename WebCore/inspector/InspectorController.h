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

#ifndef InspectorController_h
#define InspectorController_h

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
class ConsoleMessage;
class Database;
class Document;
class DocumentLoader;
class FloatRect;
class GraphicsContext;
class HitTestResult;
class InjectedScript;
class InjectedScriptHost;
class InspectorArray;
class InspectorBackend;
class InspectorBackendDispatcher;
class InspectorClient;
class InspectorCSSAgent;
class InspectorCSSStore;
class InspectorDOMAgent;
class InspectorDOMStorageResource;
class InspectorDatabaseResource;
class InspectorDebuggerAgent;
class InspectorFrontend;
class InspectorFrontendClient;
class InspectorObject;
class InspectorProfilerAgent;
class InspectorResourceAgent;
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
class Storage;
class StorageArea;

#if ENABLE(OFFLINE_WEB_APPLICATIONS)
class InspectorApplicationCacheAgent;
#endif

#if ENABLE(FILE_SYSTEM)
class InspectorFileSystemAgent;
#endif

#if ENABLE(WEB_SOCKETS)
class WebSocketHandshakeRequest;
class WebSocketHandshakeResponse;
#endif

class InspectorController : public Noncopyable {
public:
    typedef HashMap<int, RefPtr<InspectorDatabaseResource> > DatabaseResourcesMap;
    typedef HashMap<int, RefPtr<InspectorDOMStorageResource> > DOMStorageResourcesMap;

    static const char* const ConsolePanel;
    static const char* const ElementsPanel;
    static const char* const ProfilesPanel;
    static const char* const ScriptsPanel;

    InspectorController(Page*, InspectorClient*);
    ~InspectorController();

    InspectorBackend* inspectorBackend() { return m_inspectorBackend.get(); }
    InspectorBackendDispatcher* inspectorBackendDispatcher() { return m_inspectorBackendDispatcher.get(); }
    InspectorClient* inspectorClient() { return m_client; }
    InjectedScriptHost* injectedScriptHost() { return m_injectedScriptHost.get(); }

    void inspectedPageDestroyed();

    bool enabled() const;

    Page* inspectedPage() const { return m_inspectedPage; }
    void reloadPage();

    void restoreInspectorStateFromCookie(const String& inspectorCookie);

    void inspect(Node*);
    void highlight(Node*);
    void hideHighlight();
    void highlightDOMNode(long nodeId);
    void hideDOMNodeHighlight() { hideHighlight(); }

    void highlightFrame(unsigned long frameId);
    void hideFrameHighlight() { hideHighlight(); }

    void show();
    void showPanel(const String&);
    void close();

    void connectFrontend();
    void reuseFrontend();
    void disconnectFrontend();

    void setConsoleMessagesEnabled(bool enabled, bool* newState);
    void addMessageToConsole(MessageSource, MessageType, MessageLevel, const String& message, PassOwnPtr<ScriptArguments> arguments, PassOwnPtr<ScriptCallStack>);
    void addMessageToConsole(MessageSource, MessageType, MessageLevel, const String& message, unsigned lineNumber, const String&);
    void clearConsoleMessages();
    const Vector<OwnPtr<ConsoleMessage> >& consoleMessages() const { return m_consoleMessages; }

    bool searchingForNodeInPage() const;
    void mouseDidMoveOverElement(const HitTestResult&, unsigned modifierFlags);
    void handleMousePress();

    void setInspectorFrontendClient(PassOwnPtr<InspectorFrontendClient> client);
    bool hasInspectorFrontendClient() const { return m_inspectorFrontendClient; }

    void inspectedWindowScriptObjectCleared(Frame*);

    void didCommitLoad(DocumentLoader*);
    void frameDetachedFromParent(Frame*);
    void didLoadResourceFromMemoryCache(DocumentLoader*, const CachedResource*);

    void identifierForInitialRequest(unsigned long identifier, DocumentLoader*, const ResourceRequest&);
    void willSendRequest(unsigned long identifier, ResourceRequest&, const ResourceResponse& redirectResponse);
    void markResourceAsCached(unsigned long identifier);
    void didReceiveResponse(unsigned long identifier, DocumentLoader*, const ResourceResponse&);
    void didReceiveContentLength(unsigned long identifier, int lengthReceived);
    void didFinishLoading(unsigned long identifier, double finishTime);
    void didFailLoading(unsigned long identifier, const ResourceError&);
    void resourceRetrievedByXMLHttpRequest(unsigned long identifier, const String& sourceString, const String& url, const String& sendURL, unsigned sendLineNumber);
    void scriptImported(unsigned long identifier, const String& sourceString);

    void ensureSettingsLoaded();

    void startTimelineProfiler();
    void stopTimelineProfiler();
    InspectorTimelineAgent* timelineAgent() { return m_timelineAgent.get(); }

    void getCookies(RefPtr<InspectorArray>* cookies, WTF::String* cookiesString);
    void deleteCookie(const String& cookieName, const String& domain);

#if ENABLE(OFFLINE_WEB_APPLICATIONS)
    InspectorApplicationCacheAgent* applicationCacheAgent() { return m_applicationCacheAgent.get(); }
#endif

#if ENABLE(FILE_SYSTEM)
    InspectorFileSystemAgent* fileSystemAgent() { return m_fileSystemAgent.get(); }
#endif 

    void mainResourceFiredLoadEvent(DocumentLoader*, const KURL&);
    void mainResourceFiredDOMContentEvent(DocumentLoader*, const KURL&);

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
    void didUseDOMStorage(StorageArea* storageArea, bool isLocalStorage, Frame* frame);
    void selectDOMStorage(Storage* storage);
    void getDOMStorageEntries(long storageId, RefPtr<InspectorArray>* entries);
    void setDOMStorageItem(long storageId, const String& key, const String& value, bool* success);
    void removeDOMStorageItem(long storageId, const String& key, bool* success);
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

    void count(const String& title, unsigned lineNumber, const String& sourceID);

    void startTiming(const String& title);
    bool stopTiming(const String& title, double& elapsed);

    void startGroup(PassOwnPtr<ScriptArguments>, PassOwnPtr<ScriptCallStack> callFrame, bool collapsed = false);
    void endGroup(MessageSource source, unsigned lineNumber, const String& sourceURL);

    void markTimeline(const String& message);

#if ENABLE(JAVASCRIPT_DEBUGGER)
    void addProfile(PassRefPtr<ScriptProfile>, unsigned lineNumber, const String& sourceURL);
    void addProfileFinishedMessageToConsole(PassRefPtr<ScriptProfile>, unsigned lineNumber, const String& sourceURL);
    void addStartProfilingMessageToConsole(const String& title, unsigned lineNumber, const String& sourceURL);
    bool isRecordingUserInitiatedProfile() const;
    String getCurrentUserInitiatedProfileName(bool incrementProfileNumber = false);
    void startProfiling() { startUserInitiatedProfiling(); }
    void startUserInitiatedProfiling();
    void stopProfiling() { stopUserInitiatedProfiling(); }
    void stopUserInitiatedProfiling();
    void enableProfiler(bool always = false, bool skipRecompile = false);
    void disableProfiler(bool always = false);
    bool profilerEnabled() const;
    InspectorProfilerAgent* profilerAgent() const { return m_profilerAgent.get(); }

    void enableDebugger();
    void disableDebugger(bool always = false);
    bool debuggerEnabled() const { return m_debuggerAgent; }
    InspectorDebuggerAgent* debuggerAgent() const { return m_debuggerAgent.get(); }
    void resume();

    void setNativeBreakpoint(PassRefPtr<InspectorObject> breakpoint, String* breakpointId);
    void removeNativeBreakpoint(const String& breakpointId);
#endif

    void evaluateForTestInFrontend(long testCallId, const String& script);

    InjectedScript injectedScriptForNodeId(long id);

    void addScriptToEvaluateOnLoad(const String& source);
    void removeAllScriptsToEvaluateOnLoad();
    void setInspectorExtensionAPI(const String& source);

    bool inspectorStartsAttached();
    void setInspectorStartsAttached(bool);
    void setInspectorAttachedHeight(long height);
    int inspectorAttachedHeight() const;

    static const unsigned defaultAttachedHeight;

private:
    void getInspectorState(RefPtr<InspectorObject>* state);
    void setConsoleMessagesEnabled(bool enabled);

    friend class InspectorBackend;
    friend class InspectorBackendDispatcher;
    friend class InspectorInstrumentation;
    friend class InjectedScriptHost;

    enum ProfilerRestoreAction {
        ProfilerRestoreNoAction = 0,
        ProfilerRestoreResetAgent = 1
    };
    
    void populateScriptObjects();
    void restoreDebugger();
    void restoreProfiler(ProfilerRestoreAction action);
    void unbindAllResources();
    void setSearchingForNode(bool enabled);

    // Following are used from InspectorBackend and internally.
    void setSearchingForNode(bool enabled, bool* newState);

    void setMonitoringXHREnabled(bool enabled, bool* newState);
    InspectorCSSAgent* cssAgent() { return m_cssAgent.get(); }
    InspectorDOMAgent* domAgent() { return m_domAgent.get(); }
    void releaseFrontendLifetimeAgents();

#if ENABLE(JAVASCRIPT_DEBUGGER)
    void toggleRecordButton(bool);
    void enableDebuggerFromFrontend(bool always);

    String findEventListenerBreakpoint(const String& eventName);
    String findXHRBreakpoint(const String& url);
    void clearNativeBreakpoints();
#endif
#if ENABLE(DATABASE)
    void selectDatabase(Database* database);
    Database* databaseForId(long databaseId);
#endif
#if ENABLE(DOM_STORAGE)
    InspectorDOMStorageResource* getDOMStorageResourceForId(long storageId);
#endif

    PassRefPtr<InspectorObject> buildObjectForCookie(const Cookie&);
    PassRefPtr<InspectorArray> buildArrayForCookies(ListHashSet<Cookie>&);

    void focusNode();

    void addConsoleMessage(PassOwnPtr<ConsoleMessage>);

    bool isMainResourceLoader(DocumentLoader* loader, const KURL& requestUrl);

    void didEvaluateForTestInFrontend(long callId, const String& jsonResult);

#if ENABLE(JAVASCRIPT_DEBUGGER)
    friend class InspectorDebuggerAgent;
    String breakpointsSettingKey();
    PassRefPtr<InspectorValue> loadBreakpoints();
    void saveBreakpoints(PassRefPtr<InspectorObject> breakpoints);
#endif

    Page* m_inspectedPage;
    InspectorClient* m_client;
    OwnPtr<InspectorFrontendClient> m_inspectorFrontendClient;
    bool m_openingFrontend;
    OwnPtr<InspectorFrontend> m_frontend;
    OwnPtr<InspectorCSSAgent> m_cssAgent;
    RefPtr<InspectorDOMAgent> m_domAgent;
    RefPtr<InspectorStorageAgent> m_storageAgent;
    OwnPtr<InspectorCSSStore> m_cssStore;
    OwnPtr<InspectorTimelineAgent> m_timelineAgent;
    OwnPtr<InspectorState> m_state;

#if ENABLE(OFFLINE_WEB_APPLICATIONS)
    OwnPtr<InspectorApplicationCacheAgent> m_applicationCacheAgent;
#endif
    
#if ENABLE(FILE_SYSTEM)
    RefPtr<InspectorFileSystemAgent> m_fileSystemAgent;
#endif 

    RefPtr<Node> m_nodeToFocus;
    RefPtr<InspectorResourceAgent> m_resourceAgent;
    unsigned long m_mainResourceIdentifier;
    Vector<OwnPtr<ConsoleMessage> > m_consoleMessages;
    unsigned m_expiredConsoleMessageCount;
    HashMap<String, double> m_times;
    HashMap<String, unsigned> m_counts;
#if ENABLE(DATABASE)
    DatabaseResourcesMap m_databaseResources;
#endif
#if ENABLE(DOM_STORAGE)
    DOMStorageResourcesMap m_domStorageResources;
#endif
    String m_showAfterVisible;
    RefPtr<Node> m_highlightedNode;
    unsigned m_groupLevel;
    ConsoleMessage* m_previousMessage;
    bool m_settingsLoaded;
    RefPtr<InspectorBackend> m_inspectorBackend;
    OwnPtr<InspectorBackendDispatcher> m_inspectorBackendDispatcher;
    RefPtr<InjectedScriptHost> m_injectedScriptHost;

    typedef HashMap<String, String> Settings;
    mutable Settings m_settings;

    Vector<pair<long, String> > m_pendingEvaluateTestCommands;
    Vector<String> m_scriptsToEvaluateOnLoad;
    String m_inspectorExtensionAPI;
#if ENABLE(JAVASCRIPT_DEBUGGER)
    bool m_attachDebuggerWhenShown;
    OwnPtr<InspectorDebuggerAgent> m_debuggerAgent;

    HashMap<String, String> m_nativeBreakpoints;
    HashSet<String> m_eventListenerBreakpoints;
    HashMap<String, String> m_XHRBreakpoints;

    unsigned int m_lastBreakpointId;

    OwnPtr<InspectorProfilerAgent> m_profilerAgent;
#endif
#if ENABLE(WORKERS)
    typedef HashMap<intptr_t, RefPtr<InspectorWorkerResource> > WorkersMap;

    WorkersMap m_workers;
#endif
};

} // namespace WebCore

#endif // !defined(InspectorController_h)
