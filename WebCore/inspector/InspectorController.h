/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
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

#include "Console.h"
#include "Cookie.h"
#include "InspectorDOMAgent.h"
#include "PlatformString.h"
#include "ScriptArray.h"
#include "ScriptObject.h"
#include "ScriptProfile.h"
#include "ScriptState.h"
#include "ScriptValue.h"
#include "StringHash.h"
#include "Timer.h"

#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/ListHashSet.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>

#if ENABLE(JAVASCRIPT_DEBUGGER) && USE(JSC)
#include "JavaScriptDebugListener.h"

namespace JSC {
class UString;
}
#endif

namespace WebCore {

class CachedResource;
class Database;
class Document;
class DocumentLoader;
class Element;
class GraphicsContext;
class HitTestResult;
class InjectedScript;
class InjectedScriptHost;
class InspectorBackend;
class InspectorClient;
class InspectorFrontend;
class InspectorFrontendHost;
class InspectorTimelineAgent;
class JavaScriptCallFrame;
class KURL;
class Node;
class Page;
class ResourceRequest;
class ResourceResponse;
class ResourceError;
class ScriptCallStack;
class ScriptString;
class SharedBuffer;
class Storage;
class StorageArea;

class ConsoleMessage;
class InspectorDatabaseResource;
class InspectorDOMStorageResource;
class InspectorResource;

class InspectorController
#if ENABLE(JAVASCRIPT_DEBUGGER) && USE(JSC)
                          : JavaScriptDebugListener, public Noncopyable
#else
                          : public Noncopyable
#endif
                                                    {
public:
    typedef HashMap<unsigned long, RefPtr<InspectorResource> > ResourcesMap;
    typedef HashMap<RefPtr<Frame>, ResourcesMap*> FrameResourcesMap;
    typedef HashMap<int, RefPtr<InspectorDatabaseResource> > DatabaseResourcesMap;
    typedef HashMap<int, RefPtr<InspectorDOMStorageResource> > DOMStorageResourcesMap;

    typedef enum {
        CurrentPanel,
        ConsolePanel,
        ElementsPanel,
        ResourcesPanel,
        ScriptsPanel,
        TimelinePanel,
        ProfilesPanel,
        StoragePanel
    } SpecialPanels;

    InspectorController(Page*, InspectorClient*);
    ~InspectorController();

    InspectorBackend* inspectorBackend() { return m_inspectorBackend.get(); }
    InspectorFrontendHost* inspectorFrontendHost() { return m_inspectorFrontendHost.get(); }
    InjectedScriptHost* injectedScriptHost() { return m_injectedScriptHost.get(); }

    void inspectedPageDestroyed();
    void pageDestroyed() { m_page = 0; }

    bool enabled() const;

    Page* inspectedPage() const { return m_inspectedPage; }

    String setting(const String& key) const;
    void setSetting(const String& key, const String& value);

    void inspect(Node*);
    void highlight(Node*);
    void hideHighlight();

    void show();
    void showPanel(SpecialPanels);
    void close();

    bool windowVisible();
    void setWindowVisible(bool visible = true, bool attached = false);

    void addMessageToConsole(MessageSource, MessageType, MessageLevel, ScriptCallStack*);
    void addMessageToConsole(MessageSource, MessageType, MessageLevel, const String& message, unsigned lineNumber, const String& sourceID);
    void clearConsoleMessages();
    const Vector<ConsoleMessage*>& consoleMessages() const { return m_consoleMessages; }

    void attachWindow();
    void detachWindow();

    void toggleSearchForNodeInPage();
    bool searchingForNodeInPage() const { return m_searchingForNode; }
    void mouseDidMoveOverElement(const HitTestResult&, unsigned modifierFlags);
    void handleMousePressOnNode(Node*);

    void inspectedWindowScriptObjectCleared(Frame*);
    void windowScriptObjectAvailable();

    void setFrontendProxyObject(ScriptState* state, ScriptObject webInspectorObj, ScriptObject injectedScriptObj = ScriptObject());
    ScriptState* frontendScriptState() const { return m_frontendScriptState; }

    void populateScriptObjects();
    void resetScriptObjects();

    void didCommitLoad(DocumentLoader*);
    void frameDetachedFromParent(Frame*);

    void didLoadResourceFromMemoryCache(DocumentLoader*, const CachedResource*);

    void identifierForInitialRequest(unsigned long identifier, DocumentLoader*, const ResourceRequest&);
    void willSendRequest(unsigned long identifier, const ResourceRequest&, const ResourceResponse& redirectResponse);
    void didReceiveResponse(unsigned long identifier, const ResourceResponse&);
    void didReceiveContentLength(unsigned long identifier, int lengthReceived);
    void didFinishLoading(unsigned long identifier);
    void didFailLoading(unsigned long identifier, const ResourceError&);
    void resourceRetrievedByXMLHttpRequest(unsigned long identifier, const ScriptString& sourceString);
    void scriptImported(unsigned long identifier, const String& sourceString);

    void enableResourceTracking(bool always = false, bool reload = true);
    void disableResourceTracking(bool always = false);
    bool resourceTrackingEnabled() const { return m_resourceTrackingEnabled; }
    void ensureResourceTrackingSettingsLoaded();

    void startTimelineProfiler();
    void stopTimelineProfiler();
    InspectorTimelineAgent* timelineAgent() { return m_timelineAgent.get(); }

    void mainResourceFiredLoadEvent(DocumentLoader*, const KURL&);
    void mainResourceFiredDOMContentEvent(DocumentLoader*, const KURL&);
    
    void didInsertDOMNode(Node*);
    void didRemoveDOMNode(Node*);
    void didModifyDOMAttr(Element*);
                                                        
    void getCookies(long callId);

#if ENABLE(DATABASE)
    void didOpenDatabase(Database*, const String& domain, const String& name, const String& version);
#endif
#if ENABLE(DOM_STORAGE)
    void didUseDOMStorage(StorageArea* storageArea, bool isLocalStorage, Frame* frame);
    void selectDOMStorage(Storage* storage);
    void getDOMStorageEntries(int callId, int storageId);
    void setDOMStorageItem(long callId, long storageId, const String& key, const String& value);
    void removeDOMStorageItem(long callId, long storageId, const String& key);
#endif

    const ResourcesMap& resources() const { return m_resources; }

    void drawNodeHighlight(GraphicsContext&) const;

    void count(const String& title, unsigned lineNumber, const String& sourceID);

    void startTiming(const String& title);
    bool stopTiming(const String& title, double& elapsed);

    void startGroup(MessageSource source, ScriptCallStack* callFrame);
    void endGroup(MessageSource source, unsigned lineNumber, const String& sourceURL);

    void markTimeline(const String& message); 

#if ENABLE(JAVASCRIPT_DEBUGGER)
    void addProfile(PassRefPtr<ScriptProfile>, unsigned lineNumber, const String& sourceURL);
    void addProfileFinishedMessageToConsole(PassRefPtr<ScriptProfile>, unsigned lineNumber, const String& sourceURL);
    void addStartProfilingMessageToConsole(const String& title, unsigned lineNumber, const String& sourceURL);

    bool isRecordingUserInitiatedProfile() const { return m_recordingUserInitiatedProfile; }

    String getCurrentUserInitiatedProfileName(bool incrementProfileNumber);
    void startUserInitiatedProfiling(Timer<InspectorController>* = 0);
    void stopUserInitiatedProfiling();

    void enableProfiler(bool always = false, bool skipRecompile = false);
    void disableProfiler(bool always = false);
    bool profilerEnabled() const { return enabled() && m_profilerEnabled; }
#endif

#if ENABLE(JAVASCRIPT_DEBUGGER) && USE(JSC)
    void enableDebugger();
    void disableDebugger(bool always = false);
    bool debuggerEnabled() const { return m_debuggerEnabled; }

    void resumeDebugger();

    virtual void didParseSource(JSC::ExecState*, const JSC::SourceCode&);
    virtual void failedToParseSource(JSC::ExecState*, const JSC::SourceCode&, int errorLine, const JSC::UString& errorMessage);
    virtual void didPause();
    virtual void didContinue();
#endif

    void evaluateForTestInFrontend(long callId, const String& script);

    InjectedScript injectedScriptForNodeId(long id);

private:
    static const char* const FrontendSettingsSettingName;
    friend class InspectorBackend;
    friend class InspectorFrontendHost;
    friend class InjectedScriptHost;
    // Following are used from InspectorBackend and internally.
    void scriptObjectReady();
    void moveWindowBy(float x, float y) const;
    void setAttachedWindow(bool);
    void setAttachedWindowHeight(unsigned height);
    void storeLastActivePanel(const String& panelName);
    void closeWindow();
    InspectorDOMAgent* domAgent() { return m_domAgent.get(); }
    void releaseDOMAgent();

    void deleteCookie(const String& cookieName, const String& domain);

#if ENABLE(JAVASCRIPT_DEBUGGER)
    typedef HashMap<unsigned int, RefPtr<ScriptProfile> > ProfilesMap;

    void startUserInitiatedProfilingSoon();
    void toggleRecordButton(bool);
    void enableDebuggerFromFrontend(bool always);
    void getProfileHeaders(long callId);
    void getProfile(long callId, unsigned uid);
    ScriptObject createProfileHeader(const ScriptProfile& profile);
#endif
#if ENABLE(DATABASE)
    void selectDatabase(Database* database);
    Database* databaseForId(int databaseId);
#endif
#if ENABLE(DOM_STORAGE)
    InspectorDOMStorageResource* getDOMStorageResourceForId(int storageId);
#endif
                                                        
    ScriptObject buildObjectForCookie(const Cookie&);
    ScriptArray buildArrayForCookies(ListHashSet<Cookie>&);

    void focusNode();

    void addConsoleMessage(ScriptState*, ConsoleMessage*);

    void addResource(InspectorResource*);
    void removeResource(InspectorResource*);
    InspectorResource* getTrackedResource(unsigned long identifier);

    void pruneResources(ResourcesMap*, DocumentLoader* loaderToKeep = 0);
    void removeAllResources(ResourcesMap* map) { pruneResources(map); }

    void showWindow();

    bool isMainResourceLoader(DocumentLoader* loader, const KURL& requestUrl);

    SpecialPanels specialPanelForJSName(const String& panelName);

    void didEvaluateForTestInFrontend(long callId, const String& jsonResult);

    Page* m_inspectedPage;
    InspectorClient* m_client;
    OwnPtr<InspectorFrontend> m_frontend;
    RefPtr<InspectorDOMAgent> m_domAgent;
    OwnPtr<InspectorTimelineAgent> m_timelineAgent;
    Page* m_page;
    RefPtr<Node> m_nodeToFocus;
    RefPtr<InspectorResource> m_mainResource;
    ResourcesMap m_resources;
    HashSet<String> m_knownResources;
    FrameResourcesMap m_frameResources;
    Vector<ConsoleMessage*> m_consoleMessages;
    unsigned m_expiredConsoleMessageCount;
    HashMap<String, double> m_times;
    HashMap<String, unsigned> m_counts;
#if ENABLE(DATABASE)
    DatabaseResourcesMap m_databaseResources;
#endif
#if ENABLE(DOM_STORAGE)
    DOMStorageResourcesMap m_domStorageResources;
#endif
    ScriptState* m_frontendScriptState;
    bool m_windowVisible;
    SpecialPanels m_showAfterVisible;
    RefPtr<Node> m_highlightedNode;
    unsigned m_groupLevel;
    bool m_searchingForNode;
    ConsoleMessage* m_previousMessage;
    bool m_resourceTrackingEnabled;
    bool m_resourceTrackingSettingsLoaded;
    RefPtr<InspectorBackend> m_inspectorBackend;
    RefPtr<InspectorFrontendHost> m_inspectorFrontendHost;
    RefPtr<InjectedScriptHost> m_injectedScriptHost;

    typedef HashMap<String, String> Settings;
    mutable Settings m_settings;

    Vector<pair<long, String> > m_pendingEvaluateTestCommands;
#if ENABLE(JAVASCRIPT_DEBUGGER) && USE(JSC)
    bool m_debuggerEnabled;
    bool m_attachDebuggerWhenShown;
#endif
#if ENABLE(JAVASCRIPT_DEBUGGER)
    bool m_profilerEnabled;
    bool m_recordingUserInitiatedProfile;
    int m_currentUserInitiatedProfileNumber;
    unsigned m_nextUserInitiatedProfileNumber;
    Timer<InspectorController> m_startProfiling;
    ProfilesMap m_profiles;
#endif
};

inline void InspectorController::didInsertDOMNode(Node* node)
{
    if (m_domAgent)
        m_domAgent->didInsertDOMNode(node);
}

inline void InspectorController::didRemoveDOMNode(Node* node)
{
    if (m_domAgent)
        m_domAgent->didRemoveDOMNode(node);
}

inline void InspectorController::didModifyDOMAttr(Element* element)
{
    if (m_domAgent)
        m_domAgent->didModifyDOMAttr(element);
}

} // namespace WebCore

#endif // !defined(InspectorController_h)
