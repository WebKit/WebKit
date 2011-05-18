/*
* Copyright (C) 2010 Google Inc. All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are
* met:
*
*     * Redistributions of source code must retain the above copyright
* notice, this list of conditions and the following disclaimer.
*     * Redistributions in binary form must reproduce the above
* copyright notice, this list of conditions and the following disclaimer
* in the documentation and/or other materials provided with the
* distribution.
*     * Neither the name of Google Inc. nor the names of its
* contributors may be used to endorse or promote products derived from
* this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
* "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
* LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
* A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
* OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
* SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
* DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
* THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef InspectorInstrumentation_h
#define InspectorInstrumentation_h

#include "ConsoleTypes.h"
#include "Document.h"
#include "Frame.h"
#include "Page.h"
#include "ScriptExecutionContext.h"
#include <wtf/HashMap.h>

namespace WebCore {

class CharacterData;
class DOMWrapperWorld;
class Database;
class Element;
class EventContext;
class DocumentLoader;
class HitTestResult;
class InspectorTimelineAgent;
class InstrumentingAgents;
class KURL;
class Node;
class ResourceRequest;
class ResourceResponse;
class ScriptArguments;
class ScriptCallStack;
class ScriptExecutionContext;
class ScriptProfile;
class StorageArea;
class WorkerContextProxy;
class XMLHttpRequest;

#if ENABLE(WEB_SOCKETS)
class WebSocketHandshakeRequest;
class WebSocketHandshakeResponse;
#endif

#define FAST_RETURN_IF_NO_FRONTENDS(value) if (!hasFrontends()) return value;

typedef pair<InstrumentingAgents*, int> InspectorInstrumentationCookie;

class InspectorInstrumentation {
public:
    static void didClearWindowObjectInWorld(Frame*, DOMWrapperWorld*);
    static void inspectedPageDestroyed(Page*);

    static void willInsertDOMNode(Document*, Node*, Node* parent);
    static void didInsertDOMNode(Document*, Node*);
    static void willRemoveDOMNode(Document*, Node*);
    static void willModifyDOMAttr(Document*, Element*);
    static void didModifyDOMAttr(Document*, Element*);
    static void characterDataModified(Document*, CharacterData*);
    static void didInvalidateStyleAttr(Document*, Node*);

    static void mouseDidMoveOverElement(Page*, const HitTestResult&, unsigned modifierFlags);
    static bool handleMousePress(Page*);

    static void willSendXMLHttpRequest(ScriptExecutionContext*, const String& url);
    static void didScheduleResourceRequest(Document*, const String& url);
    static void didInstallTimer(ScriptExecutionContext*, int timerId, int timeout, bool singleShot);
    static void didRemoveTimer(ScriptExecutionContext*, int timerId);

    static InspectorInstrumentationCookie willCallFunction(Frame*, const String& scriptName, int scriptLine);
    static void didCallFunction(const InspectorInstrumentationCookie&);
    static InspectorInstrumentationCookie willChangeXHRReadyState(ScriptExecutionContext*, XMLHttpRequest* request);
    static void didChangeXHRReadyState(const InspectorInstrumentationCookie&);
    static InspectorInstrumentationCookie willDispatchEvent(Document*, const Event& event, DOMWindow* window, Node* node, const Vector<EventContext>& ancestors);
    static void didDispatchEvent(const InspectorInstrumentationCookie&);
    static InspectorInstrumentationCookie willDispatchEventOnWindow(Frame*, const Event& event, DOMWindow* window);
    static void didDispatchEventOnWindow(const InspectorInstrumentationCookie&);
    static InspectorInstrumentationCookie willEvaluateScript(Frame*, const String& url, int lineNumber);
    static void didEvaluateScript(const InspectorInstrumentationCookie&);
    static InspectorInstrumentationCookie willFireTimer(ScriptExecutionContext*, int timerId);
    static void didFireTimer(const InspectorInstrumentationCookie&);
    static InspectorInstrumentationCookie willLayout(Frame*);
    static void didLayout(const InspectorInstrumentationCookie&);
    static InspectorInstrumentationCookie willLoadXHR(ScriptExecutionContext*, XMLHttpRequest*);
    static void didLoadXHR(const InspectorInstrumentationCookie&);
    static InspectorInstrumentationCookie willPaint(Frame*, const IntRect& rect);
    static void didPaint(const InspectorInstrumentationCookie&);
    static InspectorInstrumentationCookie willRecalculateStyle(Document*);
    static void didRecalculateStyle(const InspectorInstrumentationCookie&);

    static void applyUserAgentOverride(Frame*, String*);
    static void willSendRequest(Frame*, unsigned long identifier, DocumentLoader*, ResourceRequest&, const ResourceResponse& redirectResponse);
    static void continueAfterPingLoader(Frame*, unsigned long identifier, DocumentLoader*, ResourceRequest&, const ResourceResponse&);
    static void markResourceAsCached(Page*, unsigned long identifier);
    static void didLoadResourceFromMemoryCache(Page*, DocumentLoader*, const CachedResource*);
    static InspectorInstrumentationCookie willReceiveResourceData(Frame*, unsigned long identifier);
    static void didReceiveResourceData(const InspectorInstrumentationCookie&);
    static InspectorInstrumentationCookie willReceiveResourceResponse(Frame*, unsigned long identifier, const ResourceResponse&);
    static void didReceiveResourceResponse(const InspectorInstrumentationCookie&, unsigned long identifier, DocumentLoader*, const ResourceResponse&);
    static void continueAfterXFrameOptionsDenied(Frame*, DocumentLoader*, unsigned long identifier, const ResourceResponse&);
    static void continueWithPolicyDownload(Frame*, DocumentLoader*, unsigned long identifier, const ResourceResponse&);
    static void continueWithPolicyIgnore(Frame*, DocumentLoader*, unsigned long identifier, const ResourceResponse&);
    static void didReceiveContentLength(Frame*, unsigned long identifier, int dataLength, int encodedDataLength);
    static void didFinishLoading(Frame*, unsigned long identifier, double finishTime);
    static void didFailLoading(Frame*, unsigned long identifier, const ResourceError&);
    static void resourceRetrievedByXMLHttpRequest(ScriptExecutionContext*, unsigned long identifier, const String& sourceString, const String& url, const String& sendURL, unsigned sendLineNumber);
    static void scriptImported(ScriptExecutionContext*, unsigned long identifier, const String& sourceString);
    static void domContentLoadedEventFired(Frame*, const KURL&);
    static void loadEventFired(Frame*, const KURL&);
    static void frameDetachedFromParent(Frame*);
    static void didCommitLoad(Frame*, DocumentLoader*);

    static InspectorInstrumentationCookie willWriteHTML(Document*, unsigned int length, unsigned int startLine);
    static void didWriteHTML(const InspectorInstrumentationCookie&, unsigned int endLine);

    static void addMessageToConsole(Page*, MessageSource, MessageType, MessageLevel, const String& message, PassRefPtr<ScriptArguments>, PassRefPtr<ScriptCallStack>);
    static void addMessageToConsole(Page*, MessageSource, MessageType, MessageLevel, const String& message, unsigned lineNumber, const String&);
    static void consoleCount(Page*, PassRefPtr<ScriptArguments>, PassRefPtr<ScriptCallStack>);
    static void startConsoleTiming(Page*, const String& title);
    static void stopConsoleTiming(Page*, const String& title, PassRefPtr<ScriptCallStack>);
    static void consoleMarkTimeline(Page*, PassRefPtr<ScriptArguments>);

#if ENABLE(JAVASCRIPT_DEBUGGER)
    static void addStartProfilingMessageToConsole(Page*, const String& title, unsigned lineNumber, const String& sourceURL);
    static void addProfile(Page*, RefPtr<ScriptProfile>, PassRefPtr<ScriptCallStack>);
    static String getCurrentUserInitiatedProfileName(Page*, bool incrementProfileNumber);
    static bool profilerEnabled(Page*);
#endif

#if ENABLE(DATABASE)
    static void didOpenDatabase(ScriptExecutionContext*, PassRefPtr<Database>, const String& domain, const String& name, const String& version);
#endif

#if ENABLE(DOM_STORAGE)
    static void didUseDOMStorage(Page*, StorageArea*, bool isLocalStorage, Frame*);
#endif

#if ENABLE(WORKERS)
    static bool willStartWorkerContext(ScriptExecutionContext*, WorkerContextProxy*);
    static void didStartWorkerContext(ScriptExecutionContext*, WorkerContextProxy*, bool paused);
    static void didCreateWorker(ScriptExecutionContext*, intptr_t id, const String& url, bool isSharedWorker);
    static void didDestroyWorker(ScriptExecutionContext*, intptr_t id);
#endif

#if ENABLE(WEB_SOCKETS)
    static void didCreateWebSocket(ScriptExecutionContext*, unsigned long identifier, const KURL& requestURL, const KURL& documentURL);
    static void willSendWebSocketHandshakeRequest(ScriptExecutionContext*, unsigned long identifier, const WebSocketHandshakeRequest&);
    static void didReceiveWebSocketHandshakeResponse(ScriptExecutionContext*, unsigned long identifier, const WebSocketHandshakeResponse&);
    static void didCloseWebSocket(ScriptExecutionContext*, unsigned long identifier);
#endif

    static void networkStateChanged(Page*);

#if ENABLE(OFFLINE_WEB_APPLICATIONS)
    static void updateApplicationCacheStatus(Frame*);
#endif

#if ENABLE(INSPECTOR)
    static void bindInstrumentingAgents(Page* page, InstrumentingAgents* agents) { instrumentingAgents().set(page, agents); }
    static void unbindInstrumentingAgents(Page* page) { instrumentingAgents().remove(page); }
    static void frontendCreated() { s_frontendCounter += 1; }
    static void frontendDeleted() { s_frontendCounter -= 1; }
    static bool hasFrontends() { return s_frontendCounter; }
    static bool collectingHTMLParseErrors(Page*);
#else
    static bool hasFrontends() { return false; }
    static bool collectingHTMLParseErrors(Page*) { return false; }
#endif

private:
#if ENABLE(INSPECTOR)
    static void didClearWindowObjectInWorldImpl(InstrumentingAgents*, Frame*, DOMWrapperWorld*);
    static void inspectedPageDestroyedImpl(InstrumentingAgents*);

    static void willInsertDOMNodeImpl(InstrumentingAgents*, Node*, Node* parent);
    static void didInsertDOMNodeImpl(InstrumentingAgents*, Node*);
    static void willRemoveDOMNodeImpl(InstrumentingAgents*, Node*);
    static void didRemoveDOMNodeImpl(InstrumentingAgents*, Node*);
    static void willModifyDOMAttrImpl(InstrumentingAgents*, Element*);
    static void didModifyDOMAttrImpl(InstrumentingAgents*, Element*);
    static void characterDataModifiedImpl(InstrumentingAgents*, CharacterData*);
    static void didInvalidateStyleAttrImpl(InstrumentingAgents*, Node*);

    static void mouseDidMoveOverElementImpl(InstrumentingAgents*, const HitTestResult&, unsigned modifierFlags);
    static bool handleMousePressImpl(InstrumentingAgents*);

    static void willSendXMLHttpRequestImpl(InstrumentingAgents*, const String& url);
    static void didScheduleResourceRequestImpl(InstrumentingAgents*, const String& url);
    static void didInstallTimerImpl(InstrumentingAgents*, int timerId, int timeout, bool singleShot);
    static void didRemoveTimerImpl(InstrumentingAgents*, int timerId);

    static InspectorInstrumentationCookie willCallFunctionImpl(InstrumentingAgents*, const String& scriptName, int scriptLine);
    static void didCallFunctionImpl(const InspectorInstrumentationCookie&);
    static InspectorInstrumentationCookie willChangeXHRReadyStateImpl(InstrumentingAgents*, XMLHttpRequest*);
    static void didChangeXHRReadyStateImpl(const InspectorInstrumentationCookie&);
    static InspectorInstrumentationCookie willDispatchEventImpl(InstrumentingAgents*, const Event&, DOMWindow*, Node*, const Vector<EventContext>& ancestors);
    static void didDispatchEventImpl(const InspectorInstrumentationCookie&);
    static InspectorInstrumentationCookie willDispatchEventOnWindowImpl(InstrumentingAgents*, const Event&, DOMWindow*);
    static void didDispatchEventOnWindowImpl(const InspectorInstrumentationCookie&);
    static InspectorInstrumentationCookie willEvaluateScriptImpl(InstrumentingAgents*, const String& url, int lineNumber);
    static void didEvaluateScriptImpl(const InspectorInstrumentationCookie&);
    static InspectorInstrumentationCookie willFireTimerImpl(InstrumentingAgents*, int timerId);
    static void didFireTimerImpl(const InspectorInstrumentationCookie&);
    static InspectorInstrumentationCookie willLayoutImpl(InstrumentingAgents*);
    static void didLayoutImpl(const InspectorInstrumentationCookie&);
    static InspectorInstrumentationCookie willLoadXHRImpl(InstrumentingAgents*, XMLHttpRequest*);
    static void didLoadXHRImpl(const InspectorInstrumentationCookie&);
    static InspectorInstrumentationCookie willPaintImpl(InstrumentingAgents*, const IntRect&);
    static void didPaintImpl(const InspectorInstrumentationCookie&);
    static InspectorInstrumentationCookie willRecalculateStyleImpl(InstrumentingAgents*);
    static void didRecalculateStyleImpl(const InspectorInstrumentationCookie&);

    static void applyUserAgentOverrideImpl(InstrumentingAgents*, String*);
    static void willSendRequestImpl(InstrumentingAgents*, unsigned long identifier, DocumentLoader*, ResourceRequest&, const ResourceResponse& redirectResponse);
    static void continueAfterPingLoaderImpl(InstrumentingAgents*, unsigned long identifier, DocumentLoader*, ResourceRequest&, const ResourceResponse&);
    static void markResourceAsCachedImpl(InstrumentingAgents*, unsigned long identifier);
    static void didLoadResourceFromMemoryCacheImpl(InstrumentingAgents*, DocumentLoader*, const CachedResource*);
    static InspectorInstrumentationCookie willReceiveResourceDataImpl(InstrumentingAgents*, unsigned long identifier);
    static void didReceiveResourceDataImpl(const InspectorInstrumentationCookie&);
    static InspectorInstrumentationCookie willReceiveResourceResponseImpl(InstrumentingAgents*, unsigned long identifier, const ResourceResponse&);
    static void didReceiveResourceResponseImpl(const InspectorInstrumentationCookie&, unsigned long identifier, DocumentLoader*, const ResourceResponse&);
    static void didReceiveResourceResponseButCanceledImpl(Frame*, DocumentLoader*, unsigned long identifier, const ResourceResponse&);
    static void continueAfterXFrameOptionsDeniedImpl(Frame*, DocumentLoader*, unsigned long identifier, const ResourceResponse&);
    static void continueWithPolicyDownloadImpl(Frame*, DocumentLoader*, unsigned long identifier, const ResourceResponse&);
    static void continueWithPolicyIgnoreImpl(Frame*, DocumentLoader*, unsigned long identifier, const ResourceResponse&);
    static void didReceiveContentLengthImpl(InstrumentingAgents*, unsigned long identifier, int dataLength, int encodedDataLength);
    static void didFinishLoadingImpl(InstrumentingAgents*, unsigned long identifier, double finishTime);
    static void didFailLoadingImpl(InstrumentingAgents*, unsigned long identifier, const ResourceError&);
    static void resourceRetrievedByXMLHttpRequestImpl(InstrumentingAgents*, unsigned long identifier, const String& sourceString, const String& url, const String& sendURL, unsigned sendLineNumber);
    static void scriptImportedImpl(InstrumentingAgents*, unsigned long identifier, const String& sourceString);
    static void domContentLoadedEventFiredImpl(InstrumentingAgents*, Frame*, const KURL&);
    static void loadEventFiredImpl(InstrumentingAgents*, Frame*, const KURL&);
    static void frameDetachedFromParentImpl(InstrumentingAgents*, Frame*);
    static void didCommitLoadImpl(InstrumentingAgents*, Page*, DocumentLoader*);

    static InspectorInstrumentationCookie willWriteHTMLImpl(InstrumentingAgents*, unsigned int length, unsigned int startLine);
    static void didWriteHTMLImpl(const InspectorInstrumentationCookie&, unsigned int endLine);

    static void addMessageToConsoleImpl(InstrumentingAgents*, MessageSource, MessageType, MessageLevel, const String& message, PassRefPtr<ScriptArguments>, PassRefPtr<ScriptCallStack>);
    static void addMessageToConsoleImpl(InstrumentingAgents*, MessageSource, MessageType, MessageLevel, const String& message, unsigned lineNumber, const String& sourceID);
    static void consoleCountImpl(InstrumentingAgents*, PassRefPtr<ScriptArguments>, PassRefPtr<ScriptCallStack>);
    static void startConsoleTimingImpl(InstrumentingAgents*, const String& title);
    static void stopConsoleTimingImpl(InstrumentingAgents*, const String& title, PassRefPtr<ScriptCallStack>);
    static void consoleMarkTimelineImpl(InstrumentingAgents*, PassRefPtr<ScriptArguments>);

#if ENABLE(JAVASCRIPT_DEBUGGER)
    static void addStartProfilingMessageToConsoleImpl(InstrumentingAgents*, const String& title, unsigned lineNumber, const String& sourceURL);
    static void addProfileImpl(InstrumentingAgents*, RefPtr<ScriptProfile>, PassRefPtr<ScriptCallStack>);
    static String getCurrentUserInitiatedProfileNameImpl(InstrumentingAgents*, bool incrementProfileNumber);
    static bool profilerEnabledImpl(InstrumentingAgents*);
#endif

#if ENABLE(DATABASE)
    static void didOpenDatabaseImpl(InstrumentingAgents*, PassRefPtr<Database>, const String& domain, const String& name, const String& version);
#endif

#if ENABLE(DOM_STORAGE)
    static void didUseDOMStorageImpl(InstrumentingAgents*, StorageArea*, bool isLocalStorage, Frame*);
#endif

#if ENABLE(WORKERS)
    static void didStartWorkerContextImpl(InstrumentingAgents*, WorkerContextProxy*);
    static void didCreateWorkerImpl(InstrumentingAgents*, intptr_t id, const String& url, bool isSharedWorker);
    static void didDestroyWorkerImpl(InstrumentingAgents*, intptr_t id);
#endif

#if ENABLE(WEB_SOCKETS)
    static void didCreateWebSocketImpl(InstrumentingAgents*, unsigned long identifier, const KURL& requestURL, const KURL& documentURL);
    static void willSendWebSocketHandshakeRequestImpl(InstrumentingAgents*, unsigned long identifier, const WebSocketHandshakeRequest&);
    static void didReceiveWebSocketHandshakeResponseImpl(InstrumentingAgents*, unsigned long identifier, const WebSocketHandshakeResponse&);
    static void didCloseWebSocketImpl(InstrumentingAgents*, unsigned long identifier);
#endif

#if ENABLE(OFFLINE_WEB_APPLICATIONS)
    static void networkStateChangedImpl(InstrumentingAgents*);
    static void updateApplicationCacheStatusImpl(InstrumentingAgents*, Frame*);
#endif

    static InstrumentingAgents* instrumentingAgentsForPage(Page*);
    static InstrumentingAgents* instrumentingAgentsForFrame(Frame*);
    static InstrumentingAgents* instrumentingAgentsForContext(ScriptExecutionContext*);
    static InstrumentingAgents* instrumentingAgentsForDocument(Document*);

    static bool collectingHTMLParseErrors(InstrumentingAgents*);
    static void pauseOnNativeEventIfNeeded(InstrumentingAgents*, const String& categoryType, const String& eventName, bool synchronous);
    static void cancelPauseOnNativeEvent(InstrumentingAgents*);
    static InspectorTimelineAgent* retrieveTimelineAgent(const InspectorInstrumentationCookie&);

    typedef HashMap<Page*, InstrumentingAgents*> InstrumentingAgentsMap;
    static InstrumentingAgentsMap& instrumentingAgents();
    static int s_frontendCounter;
#endif
};

inline void InspectorInstrumentation::didClearWindowObjectInWorld(Frame* frame, DOMWrapperWorld* world)
{
#if ENABLE(INSPECTOR)
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        didClearWindowObjectInWorldImpl(instrumentingAgents, frame, world);
#endif
}

inline void InspectorInstrumentation::inspectedPageDestroyed(Page* page)
{
#if ENABLE(INSPECTOR)
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForPage(page))
        inspectedPageDestroyedImpl(instrumentingAgents);
#endif
}

inline void InspectorInstrumentation::willInsertDOMNode(Document* document, Node* node, Node* parent)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        willInsertDOMNodeImpl(instrumentingAgents, node, parent);
#endif
}

inline void InspectorInstrumentation::didInsertDOMNode(Document* document, Node* node)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        didInsertDOMNodeImpl(instrumentingAgents, node);
#endif
}

inline void InspectorInstrumentation::willRemoveDOMNode(Document* document, Node* node)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document)) {
        willRemoveDOMNodeImpl(instrumentingAgents, node);
        didRemoveDOMNodeImpl(instrumentingAgents, node);
    }
#endif
}

inline void InspectorInstrumentation::willModifyDOMAttr(Document* document, Element* element)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        willModifyDOMAttrImpl(instrumentingAgents, element);
#endif
}

inline void InspectorInstrumentation::didModifyDOMAttr(Document* document, Element* element)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        didModifyDOMAttrImpl(instrumentingAgents, element);
#endif
}

inline void InspectorInstrumentation::didInvalidateStyleAttr(Document* document, Node* node)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        didInvalidateStyleAttrImpl(instrumentingAgents, node);
#endif
}

inline void InspectorInstrumentation::mouseDidMoveOverElement(Page* page, const HitTestResult& result, unsigned modifierFlags)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForPage(page))
        mouseDidMoveOverElementImpl(instrumentingAgents, result, modifierFlags);
#endif
}

inline bool InspectorInstrumentation::handleMousePress(Page* page)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(false);
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForPage(page))
        return handleMousePressImpl(instrumentingAgents);
#endif
    return false;
}

inline void InspectorInstrumentation::characterDataModified(Document* document, CharacterData* characterData)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        characterDataModifiedImpl(instrumentingAgents, characterData);
#endif
}

inline void InspectorInstrumentation::willSendXMLHttpRequest(ScriptExecutionContext* context, const String& url)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        willSendXMLHttpRequestImpl(instrumentingAgents, url);
#endif
}

inline void InspectorInstrumentation::didScheduleResourceRequest(Document* document, const String& url)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        didScheduleResourceRequestImpl(instrumentingAgents, url);
#endif
}

inline void InspectorInstrumentation::didInstallTimer(ScriptExecutionContext* context, int timerId, int timeout, bool singleShot)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        didInstallTimerImpl(instrumentingAgents, timerId, timeout, singleShot);
#endif
}

inline void InspectorInstrumentation::didRemoveTimer(ScriptExecutionContext* context, int timerId)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        didRemoveTimerImpl(instrumentingAgents, timerId);
#endif
}


inline InspectorInstrumentationCookie InspectorInstrumentation::willCallFunction(Frame* frame, const String& scriptName, int scriptLine)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(InspectorInstrumentationCookie());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        return willCallFunctionImpl(instrumentingAgents, scriptName, scriptLine);
#endif
    return InspectorInstrumentationCookie();
}

inline void InspectorInstrumentation::didCallFunction(const InspectorInstrumentationCookie& cookie)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (cookie.first)
        didCallFunctionImpl(cookie);
#endif
}

inline InspectorInstrumentationCookie InspectorInstrumentation::willChangeXHRReadyState(ScriptExecutionContext* context, XMLHttpRequest* request)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(InspectorInstrumentationCookie());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        return willChangeXHRReadyStateImpl(instrumentingAgents, request);
#endif
    return InspectorInstrumentationCookie();
}

inline void InspectorInstrumentation::didChangeXHRReadyState(const InspectorInstrumentationCookie& cookie)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (cookie.first)
        didChangeXHRReadyStateImpl(cookie);
#endif
}

inline InspectorInstrumentationCookie InspectorInstrumentation::willDispatchEvent(Document* document, const Event& event, DOMWindow* window, Node* node, const Vector<EventContext>& ancestors)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(InspectorInstrumentationCookie());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        return willDispatchEventImpl(instrumentingAgents, event, window, node, ancestors);
#endif
    return InspectorInstrumentationCookie();
}

inline void InspectorInstrumentation::didDispatchEvent(const InspectorInstrumentationCookie& cookie)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (cookie.first)
        didDispatchEventImpl(cookie);
#endif
}

inline InspectorInstrumentationCookie InspectorInstrumentation::willDispatchEventOnWindow(Frame* frame, const Event& event, DOMWindow* window)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(InspectorInstrumentationCookie());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        return willDispatchEventOnWindowImpl(instrumentingAgents, event, window);
#endif
    return InspectorInstrumentationCookie();
}

inline void InspectorInstrumentation::didDispatchEventOnWindow(const InspectorInstrumentationCookie& cookie)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (cookie.first)
        didDispatchEventOnWindowImpl(cookie);
#endif
}

inline InspectorInstrumentationCookie InspectorInstrumentation::willEvaluateScript(Frame* frame, const String& url, int lineNumber)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(InspectorInstrumentationCookie());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        return willEvaluateScriptImpl(instrumentingAgents, url, lineNumber);
#endif
    return InspectorInstrumentationCookie();
}

inline void InspectorInstrumentation::didEvaluateScript(const InspectorInstrumentationCookie& cookie)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (cookie.first)
        didEvaluateScriptImpl(cookie);
#endif
}

inline InspectorInstrumentationCookie InspectorInstrumentation::willFireTimer(ScriptExecutionContext* context, int timerId)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(InspectorInstrumentationCookie());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        return willFireTimerImpl(instrumentingAgents, timerId);
#endif
    return InspectorInstrumentationCookie();
}

inline void InspectorInstrumentation::didFireTimer(const InspectorInstrumentationCookie& cookie)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (cookie.first)
        didFireTimerImpl(cookie);
#endif
}

inline InspectorInstrumentationCookie InspectorInstrumentation::willLayout(Frame* frame)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(InspectorInstrumentationCookie());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        return willLayoutImpl(instrumentingAgents);
#endif
    return InspectorInstrumentationCookie();
}

inline void InspectorInstrumentation::didLayout(const InspectorInstrumentationCookie& cookie)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (cookie.first)
        didLayoutImpl(cookie);
#endif
}

inline InspectorInstrumentationCookie InspectorInstrumentation::willLoadXHR(ScriptExecutionContext* context, XMLHttpRequest* request)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(InspectorInstrumentationCookie());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        return willLoadXHRImpl(instrumentingAgents, request);
#endif
    return InspectorInstrumentationCookie();
}

inline void InspectorInstrumentation::didLoadXHR(const InspectorInstrumentationCookie& cookie)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (cookie.first)
        didLoadXHRImpl(cookie);
#endif
}

inline InspectorInstrumentationCookie InspectorInstrumentation::willPaint(Frame* frame, const IntRect& rect)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(InspectorInstrumentationCookie());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        return willPaintImpl(instrumentingAgents, rect);
#endif
    return InspectorInstrumentationCookie();
}

inline void InspectorInstrumentation::didPaint(const InspectorInstrumentationCookie& cookie)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (cookie.first)
        didPaintImpl(cookie);
#endif
}

inline InspectorInstrumentationCookie InspectorInstrumentation::willRecalculateStyle(Document* document)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(InspectorInstrumentationCookie());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        return willRecalculateStyleImpl(instrumentingAgents);
#endif
    return InspectorInstrumentationCookie();
}

inline void InspectorInstrumentation::didRecalculateStyle(const InspectorInstrumentationCookie& cookie)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (cookie.first)
        didRecalculateStyleImpl(cookie);
#endif
}

inline void InspectorInstrumentation::applyUserAgentOverride(Frame* frame, String* userAgent)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        applyUserAgentOverrideImpl(instrumentingAgents, userAgent);
#endif
}

inline void InspectorInstrumentation::willSendRequest(Frame* frame, unsigned long identifier, DocumentLoader* loader, ResourceRequest& request, const ResourceResponse& redirectResponse)
{
#if ENABLE(INSPECTOR)
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        willSendRequestImpl(instrumentingAgents, identifier, loader, request, redirectResponse);
#endif
}

inline void InspectorInstrumentation::continueAfterPingLoader(Frame* frame, unsigned long identifier, DocumentLoader* loader, ResourceRequest& request, const ResourceResponse& response)
{
#if ENABLE(INSPECTOR)
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        InspectorInstrumentation::continueAfterPingLoaderImpl(instrumentingAgents, identifier, loader, request, response);
#endif
}

inline void InspectorInstrumentation::markResourceAsCached(Page* page, unsigned long identifier)
{
#if ENABLE(INSPECTOR)
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForPage(page))
        markResourceAsCachedImpl(instrumentingAgents, identifier); 
#endif
}

inline void InspectorInstrumentation::didLoadResourceFromMemoryCache(Page* page, DocumentLoader* loader, const CachedResource* resource)
{
#if ENABLE(INSPECTOR)
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForPage(page))
        didLoadResourceFromMemoryCacheImpl(instrumentingAgents, loader, resource);
#endif
}

inline InspectorInstrumentationCookie InspectorInstrumentation::willReceiveResourceData(Frame* frame, unsigned long identifier)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(InspectorInstrumentationCookie());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        return willReceiveResourceDataImpl(instrumentingAgents, identifier);
#endif
    return InspectorInstrumentationCookie();
}

inline void InspectorInstrumentation::didReceiveResourceData(const InspectorInstrumentationCookie& cookie)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (cookie.first)
        didReceiveResourceDataImpl(cookie);
#endif
}

inline InspectorInstrumentationCookie InspectorInstrumentation::willReceiveResourceResponse(Frame* frame, unsigned long identifier, const ResourceResponse& response)
{
#if ENABLE(INSPECTOR)
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        return willReceiveResourceResponseImpl(instrumentingAgents, identifier, response);
#endif
    return InspectorInstrumentationCookie();
}

inline void InspectorInstrumentation::didReceiveResourceResponse(const InspectorInstrumentationCookie& cookie, unsigned long identifier, DocumentLoader* loader, const ResourceResponse& response)
{
#if ENABLE(INSPECTOR)
    // Call this unconditionally so that we're able to log to console with no front-end attached.
    didReceiveResourceResponseImpl(cookie, identifier, loader, response);
#endif
}

inline void InspectorInstrumentation::continueAfterXFrameOptionsDenied(Frame* frame, DocumentLoader* loader, unsigned long identifier, const ResourceResponse& r)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(void());
    InspectorInstrumentation::continueAfterXFrameOptionsDeniedImpl(frame, loader, identifier, r);
#endif
}

inline void InspectorInstrumentation::continueWithPolicyDownload(Frame* frame, DocumentLoader* loader, unsigned long identifier, const ResourceResponse& r)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(void());
    InspectorInstrumentation::continueWithPolicyDownloadImpl(frame, loader, identifier, r);
#endif
}

inline void InspectorInstrumentation::continueWithPolicyIgnore(Frame* frame, DocumentLoader* loader, unsigned long identifier, const ResourceResponse& r)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(void());
    InspectorInstrumentation::continueWithPolicyIgnoreImpl(frame, loader, identifier, r);
#endif
}

inline void InspectorInstrumentation::didReceiveContentLength(Frame* frame, unsigned long identifier, int dataLength, int encodedDataLength)
{
#if ENABLE(INSPECTOR)
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        didReceiveContentLengthImpl(instrumentingAgents, identifier, dataLength, encodedDataLength);
#endif
}

inline void InspectorInstrumentation::didFinishLoading(Frame* frame, unsigned long identifier, double finishTime)
{
#if ENABLE(INSPECTOR)
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        didFinishLoadingImpl(instrumentingAgents, identifier, finishTime);
#endif
}

inline void InspectorInstrumentation::didFailLoading(Frame* frame, unsigned long identifier, const ResourceError& error)
{
#if ENABLE(INSPECTOR)
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        didFailLoadingImpl(instrumentingAgents, identifier, error);
#endif
}

inline void InspectorInstrumentation::resourceRetrievedByXMLHttpRequest(ScriptExecutionContext* context, unsigned long identifier, const String& sourceString, const String& url, const String& sendURL, unsigned sendLineNumber)
{
#if ENABLE(INSPECTOR)
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        resourceRetrievedByXMLHttpRequestImpl(instrumentingAgents, identifier, sourceString, url, sendURL, sendLineNumber);
#endif
}

inline void InspectorInstrumentation::scriptImported(ScriptExecutionContext* context, unsigned long identifier, const String& sourceString)
{
#if ENABLE(INSPECTOR)
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        scriptImportedImpl(instrumentingAgents, identifier, sourceString);
#endif
}

inline void InspectorInstrumentation::domContentLoadedEventFired(Frame* frame, const KURL& url)
{
#if ENABLE(INSPECTOR)
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        domContentLoadedEventFiredImpl(instrumentingAgents, frame, url);
#endif
}

inline void InspectorInstrumentation::loadEventFired(Frame* frame, const KURL& url)
{
#if ENABLE(INSPECTOR)
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        loadEventFiredImpl(instrumentingAgents, frame, url);
#endif
}

inline void InspectorInstrumentation::frameDetachedFromParent(Frame* frame)
{
#if ENABLE(INSPECTOR)
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        frameDetachedFromParentImpl(instrumentingAgents, frame);
#endif
}

inline void InspectorInstrumentation::didCommitLoad(Frame* frame, DocumentLoader* loader)
{
#if ENABLE(INSPECTOR)
    if (!frame)
        return;
    Page* page = frame->page();
    if (!page)
        return;
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForPage(page))
        didCommitLoadImpl(instrumentingAgents, page, loader);
#endif
}

inline InspectorInstrumentationCookie InspectorInstrumentation::willWriteHTML(Document* document, unsigned int length, unsigned int startLine)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(InspectorInstrumentationCookie());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        return willWriteHTMLImpl(instrumentingAgents, length, startLine);
#endif
    return InspectorInstrumentationCookie();
}

inline void InspectorInstrumentation::didWriteHTML(const InspectorInstrumentationCookie& cookie, unsigned int endLine)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (cookie.first)
        didWriteHTMLImpl(cookie, endLine);
#endif
}

#if ENABLE(DOM_STORAGE)
inline void InspectorInstrumentation::didUseDOMStorage(Page* page, StorageArea* storageArea, bool isLocalStorage, Frame* frame)
{
#if ENABLE(INSPECTOR)
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForPage(page))
        didUseDOMStorageImpl(instrumentingAgents, storageArea, isLocalStorage, frame);
#endif
}
#endif

#if ENABLE(WORKERS)
inline bool InspectorInstrumentation::willStartWorkerContext(ScriptExecutionContext*, WorkerContextProxy*)
{
    return false;
}

inline void InspectorInstrumentation::didStartWorkerContext(ScriptExecutionContext* context, WorkerContextProxy* proxy, bool paused)
{
#if ENABLE(INSPECTOR)
    if (!paused)
        return;
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        didStartWorkerContextImpl(instrumentingAgents, proxy);
#endif
}

inline void InspectorInstrumentation::didCreateWorker(ScriptExecutionContext* context, intptr_t id, const String& url, bool isSharedWorker)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        didCreateWorkerImpl(instrumentingAgents, id, url, isSharedWorker);
#endif
}

inline void InspectorInstrumentation::didDestroyWorker(ScriptExecutionContext* context, intptr_t id)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        didDestroyWorkerImpl(instrumentingAgents, id);
#endif
}
#endif


#if ENABLE(WEB_SOCKETS)
inline void InspectorInstrumentation::didCreateWebSocket(ScriptExecutionContext* context, unsigned long identifier, const KURL& requestURL, const KURL& documentURL)
{
#if ENABLE(INSPECTOR)
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        didCreateWebSocketImpl(instrumentingAgents, identifier, requestURL, documentURL);
#endif
}

inline void InspectorInstrumentation::willSendWebSocketHandshakeRequest(ScriptExecutionContext* context, unsigned long identifier, const WebSocketHandshakeRequest& request)
{
#if ENABLE(INSPECTOR)
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        willSendWebSocketHandshakeRequestImpl(instrumentingAgents, identifier, request);
#endif
}

inline void InspectorInstrumentation::didReceiveWebSocketHandshakeResponse(ScriptExecutionContext* context, unsigned long identifier, const WebSocketHandshakeResponse& response)
{
#if ENABLE(INSPECTOR)
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        didReceiveWebSocketHandshakeResponseImpl(instrumentingAgents, identifier, response);
#endif
}

inline void InspectorInstrumentation::didCloseWebSocket(ScriptExecutionContext* context, unsigned long identifier)
{
#if ENABLE(INSPECTOR)
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        didCloseWebSocketImpl(instrumentingAgents, identifier);
#endif
}
#endif

inline void InspectorInstrumentation::networkStateChanged(Page* page)
{
#if ENABLE(INSPECTOR) && ENABLE(OFFLINE_WEB_APPLICATIONS)
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForPage(page))
        networkStateChangedImpl(instrumentingAgents);
#endif
}

#if ENABLE(OFFLINE_WEB_APPLICATIONS)
inline void InspectorInstrumentation::updateApplicationCacheStatus(Frame* frame)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        updateApplicationCacheStatusImpl(instrumentingAgents, frame);
#endif
}
#endif

#if ENABLE(INSPECTOR)
inline bool InspectorInstrumentation::collectingHTMLParseErrors(Page* page)
{
    FAST_RETURN_IF_NO_FRONTENDS(false);
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForPage(page))
        return collectingHTMLParseErrors(instrumentingAgents);
    return false;
}

inline InstrumentingAgents* InspectorInstrumentation::instrumentingAgentsForContext(ScriptExecutionContext* context)
{
    if (context && context->isDocument())
        return instrumentingAgentsForPage(static_cast<Document*>(context)->page());
    return 0;
}

inline InstrumentingAgents* InspectorInstrumentation::instrumentingAgentsForPage(Page* page)
{
    if (!page)
        return 0;
    return instrumentingAgents().get(page);
}

inline InstrumentingAgents* InspectorInstrumentation::instrumentingAgentsForFrame(Frame* frame)
{
    if (frame)
        return instrumentingAgentsForPage(frame->page());
    return 0;
}

inline InstrumentingAgents* InspectorInstrumentation::instrumentingAgentsForDocument(Document* document)
{
    if (document)
        return instrumentingAgentsForPage(document->page());
    return 0;
}
#endif

} // namespace WebCore

#endif // !defined(InspectorInstrumentation_h)
