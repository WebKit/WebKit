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

#include "Console.h"
#include "Frame.h"
#include "Page.h"
#include "ScriptExecutionContext.h"

#include <wtf/PassRefPtr.h>

namespace WebCore {

class CharacterData;
class DOMWrapperWorld;
class Database;
class Document;
class Element;
class EventContext;
class DocumentLoader;
class HitTestResult;
class InspectorController;
class InspectorResourceAgent;
class InspectorTimelineAgent;
class KURL;
class Node;
class ResourceRequest;
class ResourceResponse;
class ScriptArguments;
class ScriptCallStack;
class ScriptExecutionContext;
class StorageArea;
class XMLHttpRequest;

#if ENABLE(WEB_SOCKETS)
class WebSocketHandshakeRequest;
class WebSocketHandshakeResponse;
#endif

typedef pair<InspectorController*, int> InspectorInstrumentationCookie;

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

    static void identifierForInitialRequest(Frame*, unsigned long identifier, DocumentLoader*, const ResourceRequest&);
    static void willSendRequest(Frame*, unsigned long identifier, ResourceRequest&, const ResourceResponse& redirectResponse);
    static void markResourceAsCached(Page*, unsigned long identifier);
    static void didLoadResourceFromMemoryCache(Page*, DocumentLoader*, const CachedResource*);
    static InspectorInstrumentationCookie willReceiveResourceData(Frame*, unsigned long identifier);
    static void didReceiveResourceData(const InspectorInstrumentationCookie&);
    static InspectorInstrumentationCookie willReceiveResourceResponse(Frame*, unsigned long identifier, const ResourceResponse&);
    static void didReceiveResourceResponse(const InspectorInstrumentationCookie&, unsigned long identifier, DocumentLoader*, const ResourceResponse&);
    static void didReceiveContentLength(Frame*, unsigned long identifier, int lengthReceived);
    static void didFinishLoading(Frame*, unsigned long identifier, double finishTime);
    static void didFailLoading(Frame*, unsigned long identifier, const ResourceError&);
    static void resourceRetrievedByXMLHttpRequest(ScriptExecutionContext*, unsigned long identifier, const String& sourceString, const String& url, const String& sendURL, unsigned sendLineNumber);
    static void scriptImported(ScriptExecutionContext*, unsigned long identifier, const String& sourceString);
    static void mainResourceFiredLoadEvent(Frame*, const KURL&);
    static void mainResourceFiredDOMContentEvent(Frame*, const KURL&);
    static void frameDetachedFromParent(Frame*);
    static void didCommitLoad(Frame*, DocumentLoader*);

    static InspectorInstrumentationCookie willWriteHTML(Document*, unsigned int length, unsigned int startLine);
    static void didWriteHTML(const InspectorInstrumentationCookie&, unsigned int endLine);

    static void addMessageToConsole(Page*, MessageSource, MessageType, MessageLevel, const String& message, ScriptArguments*, ScriptCallStack*);
    static void addMessageToConsole(Page*, MessageSource, MessageType, MessageLevel, const String& message, unsigned lineNumber, const String&);
    static void consoleCount(Page*, ScriptArguments*, ScriptCallStack*);
    static void startConsoleTiming(Page*, const String& title);
    static void stopConsoleTiming(Page*, const String& title, ScriptCallStack*);
    static void consoleMarkTimeline(Page*, ScriptArguments*);

#if ENABLE(JAVASCRIPT_DEBUGGER)
    static void addStartProfilingMessageToConsole(Page*, const String& title, unsigned lineNumber, const String& sourceURL);
#endif

#if ENABLE(DATABASE)
    static void didOpenDatabase(ScriptExecutionContext*, Database*, const String& domain, const String& name, const String& version);
#endif

#if ENABLE(DOM_STORAGE)
    static void didUseDOMStorage(Page*, StorageArea*, bool isLocalStorage, Frame*);
#endif

#if ENABLE(WORKERS)
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
    static void frontendCreated() { s_frontendCounter += 1; }
    static void frontendDeleted() { s_frontendCounter -= 1; }
    static bool hasFrontends() { return s_frontendCounter; }
#else
    static bool hasFrontends() { return false; }
#endif

private:
#if ENABLE(INSPECTOR)
    static void didClearWindowObjectInWorldImpl(InspectorController*, Frame*, DOMWrapperWorld*);
    static void inspectedPageDestroyedImpl(InspectorController*);

    static void willInsertDOMNodeImpl(InspectorController*, Node* node, Node* parent);
    static void didInsertDOMNodeImpl(InspectorController*, Node*);
    static void willRemoveDOMNodeImpl(InspectorController*, Node*);
    static void didRemoveDOMNodeImpl(InspectorController*, Node*);
    static void willModifyDOMAttrImpl(InspectorController*, Element*);
    static void didModifyDOMAttrImpl(InspectorController*, Element*);
    static void characterDataModifiedImpl(InspectorController*, CharacterData*);

    static void mouseDidMoveOverElementImpl(InspectorController*, const HitTestResult&, unsigned modifierFlags);
    static bool handleMousePressImpl(InspectorController*);

    static void willSendXMLHttpRequestImpl(InspectorController*, const String& url);
    static void didScheduleResourceRequestImpl(InspectorController*, const String& url);
    static void didInstallTimerImpl(InspectorController*, int timerId, int timeout, bool singleShot);
    static void didRemoveTimerImpl(InspectorController*, int timerId);

    static InspectorInstrumentationCookie willCallFunctionImpl(InspectorController*, const String& scriptName, int scriptLine);
    static void didCallFunctionImpl(const InspectorInstrumentationCookie&);
    static InspectorInstrumentationCookie willChangeXHRReadyStateImpl(InspectorController*, XMLHttpRequest* request);
    static void didChangeXHRReadyStateImpl(const InspectorInstrumentationCookie&);
    static InspectorInstrumentationCookie willDispatchEventImpl(InspectorController*, const Event& event, DOMWindow* window, Node* node, const Vector<EventContext>& ancestors);
    static void didDispatchEventImpl(const InspectorInstrumentationCookie&);
    static InspectorInstrumentationCookie willDispatchEventOnWindowImpl(InspectorController*, const Event& event, DOMWindow* window);
    static void didDispatchEventOnWindowImpl(const InspectorInstrumentationCookie&);
    static InspectorInstrumentationCookie willEvaluateScriptImpl(InspectorController*, const String& url, int lineNumber);
    static void didEvaluateScriptImpl(const InspectorInstrumentationCookie&);
    static InspectorInstrumentationCookie willFireTimerImpl(InspectorController*, int timerId);
    static void didFireTimerImpl(const InspectorInstrumentationCookie&);
    static InspectorInstrumentationCookie willLayoutImpl(InspectorController*);
    static void didLayoutImpl(const InspectorInstrumentationCookie&);
    static InspectorInstrumentationCookie willLoadXHRImpl(InspectorController*, XMLHttpRequest* request);
    static void didLoadXHRImpl(const InspectorInstrumentationCookie&);
    static InspectorInstrumentationCookie willPaintImpl(InspectorController*, const IntRect& rect);
    static void didPaintImpl(const InspectorInstrumentationCookie&);
    static InspectorInstrumentationCookie willRecalculateStyleImpl(InspectorController*);
    static void didRecalculateStyleImpl(const InspectorInstrumentationCookie&);
    static void identifierForInitialRequestImpl(InspectorController*, unsigned long identifier, DocumentLoader*, const ResourceRequest&);
    static void willSendRequestImpl(InspectorController*, unsigned long identifier, ResourceRequest&, const ResourceResponse& redirectResponse);
    static void markResourceAsCachedImpl(InspectorController*, unsigned long identifier);
    static void didLoadResourceFromMemoryCacheImpl(InspectorController*, DocumentLoader*, const CachedResource*);
    static InspectorInstrumentationCookie willReceiveResourceDataImpl(InspectorController*, unsigned long identifier);
    static void didReceiveResourceDataImpl(const InspectorInstrumentationCookie&);
    static InspectorInstrumentationCookie willReceiveResourceResponseImpl(InspectorController*, unsigned long identifier, const ResourceResponse&);
    static void didReceiveResourceResponseImpl(const InspectorInstrumentationCookie&, unsigned long identifier, DocumentLoader*, const ResourceResponse&);
    static void didReceiveContentLengthImpl(InspectorController*, unsigned long identifier, int lengthReceived);
    static void didFinishLoadingImpl(InspectorController*, unsigned long identifier, double finishTime);
    static void didFailLoadingImpl(InspectorController*, unsigned long identifier, const ResourceError&);
    static void resourceRetrievedByXMLHttpRequestImpl(InspectorController*, unsigned long identifier, const String& sourceString, const String& url, const String& sendURL, unsigned sendLineNumber);
    static void scriptImportedImpl(InspectorController*, unsigned long identifier, const String& sourceString);
    static void mainResourceFiredLoadEventImpl(InspectorController*, Frame*, const KURL&);
    static void mainResourceFiredDOMContentEventImpl(InspectorController*, Frame*, const KURL&);
    static void frameDetachedFromParentImpl(InspectorController*, Frame*);
    static void didCommitLoadImpl(InspectorController*, DocumentLoader*);

    static InspectorInstrumentationCookie willWriteHTMLImpl(InspectorController*, unsigned int length, unsigned int startLine);
    static void didWriteHTMLImpl(const InspectorInstrumentationCookie&, unsigned int endLine);

    static void addMessageToConsoleImpl(InspectorController*, MessageSource, MessageType, MessageLevel, const String& message, ScriptArguments*, ScriptCallStack*);
    static void addMessageToConsoleImpl(InspectorController*, MessageSource, MessageType, MessageLevel, const String& message, unsigned lineNumber, const String& sourceID);
    static void consoleCountImpl(InspectorController*, ScriptArguments*, ScriptCallStack*);
    static void startConsoleTimingImpl(InspectorController*, const String& title);
    static void stopConsoleTimingImpl(InspectorController*, const String& title, ScriptCallStack*);
    static void consoleMarkTimelineImpl(InspectorController*, ScriptArguments*);

#if ENABLE(JAVASCRIPT_DEBUGGER)
    static void addStartProfilingMessageToConsoleImpl(InspectorController*, const String& title, unsigned lineNumber, const String& sourceURL);
#endif

#if ENABLE(DATABASE)
    static void didOpenDatabaseImpl(InspectorController*, Database*, const String& domain, const String& name, const String& version);
#endif

#if ENABLE(DOM_STORAGE)
    static void didUseDOMStorageImpl(InspectorController*, StorageArea*, bool isLocalStorage, Frame*);
#endif

#if ENABLE(WORKERS)
    static void didCreateWorkerImpl(InspectorController*, intptr_t id, const String& url, bool isSharedWorker);
    static void didDestroyWorkerImpl(InspectorController*, intptr_t id);
#endif

#if ENABLE(WEB_SOCKETS)
    static void didCreateWebSocketImpl(InspectorController*, unsigned long identifier, const KURL& requestURL, const KURL& documentURL);
    static void willSendWebSocketHandshakeRequestImpl(InspectorController*, unsigned long identifier, const WebSocketHandshakeRequest&);
    static void didReceiveWebSocketHandshakeResponseImpl(InspectorController*, unsigned long identifier, const WebSocketHandshakeResponse&);
    static void didCloseWebSocketImpl(InspectorController*, unsigned long identifier);
#endif

#if ENABLE(OFFLINE_WEB_APPLICATIONS)
    static void networkStateChangedImpl(InspectorController*);
    static void updateApplicationCacheStatusImpl(InspectorController*, Frame*);
#endif

    static InspectorController* inspectorControllerForFrame(Frame*);
    static InspectorController* inspectorControllerForContext(ScriptExecutionContext*);
    static InspectorController* inspectorControllerForPage(Page*);
    static InspectorController* inspectorControllerWithFrontendForContext(ScriptExecutionContext*);
    static InspectorController* inspectorControllerWithFrontendForDocument(Document*);
    static InspectorController* inspectorControllerWithFrontendForFrame(Frame*);
    static InspectorController* inspectorControllerWithFrontendForPage(Page*);

    static bool hasFrontend(InspectorController*);
    static void pauseOnNativeEventIfNeeded(InspectorController*, const String& categoryType, const String& eventName, bool synchronous);
    static void cancelPauseOnNativeEvent(InspectorController*);
    static InspectorTimelineAgent* retrieveTimelineAgent(InspectorController*);
    static InspectorTimelineAgent* retrieveTimelineAgent(const InspectorInstrumentationCookie&);
    static InspectorResourceAgent* retrieveResourceAgent(InspectorController*);

    static int s_frontendCounter;
#endif
};

inline void InspectorInstrumentation::didClearWindowObjectInWorld(Frame* frame, DOMWrapperWorld* world)
{
#if ENABLE(INSPECTOR)
    if (InspectorController* inspectorController = inspectorControllerForFrame(frame))
        didClearWindowObjectInWorldImpl(inspectorController, frame, world);
#endif
}

inline void InspectorInstrumentation::inspectedPageDestroyed(Page* page)
{
#if ENABLE(INSPECTOR)
    if (InspectorController* inspectorController = inspectorControllerForPage(page))
        inspectedPageDestroyedImpl(inspectorController);
#endif
}

inline void InspectorInstrumentation::willInsertDOMNode(Document* document, Node* node, Node* parent)
{
#if ENABLE(INSPECTOR)
    if (InspectorController* inspectorController = inspectorControllerWithFrontendForDocument(document))
        willInsertDOMNodeImpl(inspectorController, node, parent);
#endif
}

inline void InspectorInstrumentation::didInsertDOMNode(Document* document, Node* node)
{
#if ENABLE(INSPECTOR)
    if (InspectorController* inspectorController = inspectorControllerWithFrontendForDocument(document))
        didInsertDOMNodeImpl(inspectorController, node);
#endif
}

inline void InspectorInstrumentation::willRemoveDOMNode(Document* document, Node* node)
{
#if ENABLE(INSPECTOR)
    if (InspectorController* inspectorController = inspectorControllerWithFrontendForDocument(document)) {
        willRemoveDOMNodeImpl(inspectorController, node);
        didRemoveDOMNodeImpl(inspectorController, node);
    }
#endif
}

inline void InspectorInstrumentation::willModifyDOMAttr(Document* document, Element* element)
{
#if ENABLE(INSPECTOR)
    if (InspectorController* inspectorController = inspectorControllerWithFrontendForDocument(document))
        willModifyDOMAttrImpl(inspectorController, element);
#endif
}

inline void InspectorInstrumentation::didModifyDOMAttr(Document* document, Element* element)
{
#if ENABLE(INSPECTOR)
    if (InspectorController* inspectorController = inspectorControllerWithFrontendForDocument(document))
        didModifyDOMAttrImpl(inspectorController, element);
#endif
}

inline void InspectorInstrumentation::mouseDidMoveOverElement(Page* page, const HitTestResult& result, unsigned modifierFlags)
{
#if ENABLE(INSPECTOR)
    if (InspectorController* inspectorController = inspectorControllerWithFrontendForPage(page))
        mouseDidMoveOverElementImpl(inspectorController, result, modifierFlags);
#endif
}

inline bool InspectorInstrumentation::handleMousePress(Page* page)
{
#if ENABLE(INSPECTOR)
    if (InspectorController* inspectorController = inspectorControllerWithFrontendForPage(page))
        return handleMousePressImpl(inspectorController);
#endif
    return false;
}

inline void InspectorInstrumentation::characterDataModified(Document* document, CharacterData* characterData)
{
#if ENABLE(INSPECTOR)
    if (InspectorController* inspectorController = inspectorControllerWithFrontendForDocument(document))
        characterDataModifiedImpl(inspectorController, characterData);
#endif
}

inline void InspectorInstrumentation::willSendXMLHttpRequest(ScriptExecutionContext* context, const String& url)
{
#if ENABLE(INSPECTOR)
    if (InspectorController* inspectorController = inspectorControllerWithFrontendForContext(context))
        willSendXMLHttpRequestImpl(inspectorController, url);
#endif
}

inline void InspectorInstrumentation::didScheduleResourceRequest(Document* document, const String& url)
{
#if ENABLE(INSPECTOR)
    if (InspectorController* inspectorController = inspectorControllerWithFrontendForDocument(document))
        didScheduleResourceRequestImpl(inspectorController, url);
#endif
}

inline void InspectorInstrumentation::didInstallTimer(ScriptExecutionContext* context, int timerId, int timeout, bool singleShot)
{
#if ENABLE(INSPECTOR)
    if (InspectorController* inspectorController = inspectorControllerWithFrontendForContext(context))
        didInstallTimerImpl(inspectorController, timerId, timeout, singleShot);
#endif
}

inline void InspectorInstrumentation::didRemoveTimer(ScriptExecutionContext* context, int timerId)
{
#if ENABLE(INSPECTOR)
    if (InspectorController* inspectorController = inspectorControllerWithFrontendForContext(context))
        didRemoveTimerImpl(inspectorController, timerId);
#endif
}


inline InspectorInstrumentationCookie InspectorInstrumentation::willCallFunction(Frame* frame, const String& scriptName, int scriptLine)
{
#if ENABLE(INSPECTOR)
    if (InspectorController* inspectorController = inspectorControllerWithFrontendForFrame(frame))
        return willCallFunctionImpl(inspectorController, scriptName, scriptLine);
#endif
    return InspectorInstrumentationCookie();
}

inline void InspectorInstrumentation::didCallFunction(const InspectorInstrumentationCookie& cookie)
{
#if ENABLE(INSPECTOR)
    if (hasFrontends() && cookie.first)
        didCallFunctionImpl(cookie);
#endif
}

inline InspectorInstrumentationCookie InspectorInstrumentation::willChangeXHRReadyState(ScriptExecutionContext* context, XMLHttpRequest* request)
{
#if ENABLE(INSPECTOR)
    if (InspectorController* inspectorController = inspectorControllerWithFrontendForContext(context))
        return willChangeXHRReadyStateImpl(inspectorController, request);
#endif
    return InspectorInstrumentationCookie();
}

inline void InspectorInstrumentation::didChangeXHRReadyState(const InspectorInstrumentationCookie& cookie)
{
#if ENABLE(INSPECTOR)
    if (hasFrontends() && cookie.first)
        didChangeXHRReadyStateImpl(cookie);
#endif
}

inline InspectorInstrumentationCookie InspectorInstrumentation::willDispatchEvent(Document* document, const Event& event, DOMWindow* window, Node* node, const Vector<EventContext>& ancestors)
{
#if ENABLE(INSPECTOR)
    if (InspectorController* inspectorController = inspectorControllerWithFrontendForDocument(document))
        return willDispatchEventImpl(inspectorController, event, window, node, ancestors);
#endif
    return InspectorInstrumentationCookie();
}

inline void InspectorInstrumentation::didDispatchEvent(const InspectorInstrumentationCookie& cookie)
{
#if ENABLE(INSPECTOR)
    if (hasFrontends() && cookie.first)
        didDispatchEventImpl(cookie);
#endif
}

inline InspectorInstrumentationCookie InspectorInstrumentation::willDispatchEventOnWindow(Frame* frame, const Event& event, DOMWindow* window)
{
#if ENABLE(INSPECTOR)
    if (InspectorController* inspectorController = inspectorControllerWithFrontendForFrame(frame))
        return willDispatchEventOnWindowImpl(inspectorController, event, window);
#endif
    return InspectorInstrumentationCookie();
}

inline void InspectorInstrumentation::didDispatchEventOnWindow(const InspectorInstrumentationCookie& cookie)
{
#if ENABLE(INSPECTOR)
    if (hasFrontends() && cookie.first)
        didDispatchEventOnWindowImpl(cookie);
#endif
}

inline InspectorInstrumentationCookie InspectorInstrumentation::willEvaluateScript(Frame* frame, const String& url, int lineNumber)
{
#if ENABLE(INSPECTOR)
    if (InspectorController* inspectorController = inspectorControllerWithFrontendForFrame(frame))
        return willEvaluateScriptImpl(inspectorController, url, lineNumber);
#endif
    return InspectorInstrumentationCookie();
}

inline void InspectorInstrumentation::didEvaluateScript(const InspectorInstrumentationCookie& cookie)
{
#if ENABLE(INSPECTOR)
    if (hasFrontends() && cookie.first)
        didEvaluateScriptImpl(cookie);
#endif
}

inline InspectorInstrumentationCookie InspectorInstrumentation::willFireTimer(ScriptExecutionContext* context, int timerId)
{
#if ENABLE(INSPECTOR)
    if (InspectorController* inspectorController = inspectorControllerWithFrontendForContext(context))
        return willFireTimerImpl(inspectorController, timerId);
#endif
    return InspectorInstrumentationCookie();
}

inline void InspectorInstrumentation::didFireTimer(const InspectorInstrumentationCookie& cookie)
{
#if ENABLE(INSPECTOR)
    if (hasFrontends() && cookie.first)
        didFireTimerImpl(cookie);
#endif
}

inline InspectorInstrumentationCookie InspectorInstrumentation::willLayout(Frame* frame)
{
#if ENABLE(INSPECTOR)
    if (InspectorController* inspectorController = inspectorControllerWithFrontendForFrame(frame))
        return willLayoutImpl(inspectorController);
#endif
    return InspectorInstrumentationCookie();
}

inline void InspectorInstrumentation::didLayout(const InspectorInstrumentationCookie& cookie)
{
#if ENABLE(INSPECTOR)
    if (hasFrontends() && cookie.first)
        didLayoutImpl(cookie);
#endif
}

inline InspectorInstrumentationCookie InspectorInstrumentation::willLoadXHR(ScriptExecutionContext* context, XMLHttpRequest* request)
{
#if ENABLE(INSPECTOR)
    if (InspectorController* inspectorController = inspectorControllerWithFrontendForContext(context))
        return willLoadXHRImpl(inspectorController, request);
#endif
    return InspectorInstrumentationCookie();
}

inline void InspectorInstrumentation::didLoadXHR(const InspectorInstrumentationCookie& cookie)
{
#if ENABLE(INSPECTOR)
    if (hasFrontends() && cookie.first)
        didLoadXHRImpl(cookie);
#endif
}

inline InspectorInstrumentationCookie InspectorInstrumentation::willPaint(Frame* frame, const IntRect& rect)
{
#if ENABLE(INSPECTOR)
    if (InspectorController* inspectorController = inspectorControllerWithFrontendForFrame(frame))
        return willPaintImpl(inspectorController, rect);
#endif
    return InspectorInstrumentationCookie();
}

inline void InspectorInstrumentation::didPaint(const InspectorInstrumentationCookie& cookie)
{
#if ENABLE(INSPECTOR)
    if (hasFrontends() && cookie.first)
        didPaintImpl(cookie);
#endif
}

inline InspectorInstrumentationCookie InspectorInstrumentation::willRecalculateStyle(Document* document)
{
#if ENABLE(INSPECTOR)
    if (InspectorController* inspectorController = inspectorControllerWithFrontendForDocument(document))
        return willRecalculateStyleImpl(inspectorController);
#endif
    return InspectorInstrumentationCookie();
}

inline void InspectorInstrumentation::didRecalculateStyle(const InspectorInstrumentationCookie& cookie)
{
#if ENABLE(INSPECTOR)
    if (hasFrontends() && cookie.first)
        didRecalculateStyleImpl(cookie);
#endif
}

inline void InspectorInstrumentation::identifierForInitialRequest(Frame* frame, unsigned long identifier, DocumentLoader* loader, const ResourceRequest& request)
{
#if ENABLE(INSPECTOR)
    // This notification should be procecessed even in cases there is no frontend.
    if (!frame)
        return;
    if (InspectorController* ic = inspectorControllerForPage(frame->page()))
        identifierForInitialRequestImpl(ic, identifier, loader, request);
#endif
}

inline void InspectorInstrumentation::willSendRequest(Frame* frame, unsigned long identifier, ResourceRequest& request, const ResourceResponse& redirectResponse)
{
#if ENABLE(INSPECTOR)
    if (InspectorController* ic = inspectorControllerWithFrontendForFrame(frame))
        willSendRequestImpl(ic, identifier, request, redirectResponse);
#endif
}

inline void InspectorInstrumentation::markResourceAsCached(Page* page, unsigned long identifier)
{
#if ENABLE(INSPECTOR)
    markResourceAsCachedImpl(inspectorControllerForPage(page), identifier); 
#endif
}

inline void InspectorInstrumentation::didLoadResourceFromMemoryCache(Page* page, DocumentLoader* loader, const CachedResource* resource)
{
#if ENABLE(INSPECTOR)
    didLoadResourceFromMemoryCacheImpl(inspectorControllerForPage(page), loader, resource);
#endif
}

inline InspectorInstrumentationCookie InspectorInstrumentation::willReceiveResourceData(Frame* frame, unsigned long identifier)
{
#if ENABLE(INSPECTOR)
    if (InspectorController* inspectorController = inspectorControllerWithFrontendForFrame(frame))
        return willReceiveResourceDataImpl(inspectorController, identifier);
#endif
    return InspectorInstrumentationCookie();
}

inline void InspectorInstrumentation::didReceiveResourceData(const InspectorInstrumentationCookie& cookie)
{
#if ENABLE(INSPECTOR)
    if (hasFrontends() && cookie.first)
        didReceiveResourceDataImpl(cookie);
#endif
}

inline InspectorInstrumentationCookie InspectorInstrumentation::willReceiveResourceResponse(Frame* frame, unsigned long identifier, const ResourceResponse& response)
{
#if ENABLE(INSPECTOR)
    if (InspectorController* inspectorController = inspectorControllerWithFrontendForFrame(frame))
        return willReceiveResourceResponseImpl(inspectorController, identifier, response);
#endif
    return InspectorInstrumentationCookie();
}

inline void InspectorInstrumentation::didReceiveResourceResponse(const InspectorInstrumentationCookie& cookie, unsigned long identifier, DocumentLoader* loader, const ResourceResponse& response)
{
#if ENABLE(INSPECTOR)
    if (hasFrontends() && cookie.first)
        didReceiveResourceResponseImpl(cookie, identifier, loader, response);
#endif
}

inline void InspectorInstrumentation::didReceiveContentLength(Frame* frame, unsigned long identifier, int lengthReceived)
{
#if ENABLE(INSPECTOR)
    if (InspectorController* inspectorController = inspectorControllerWithFrontendForFrame(frame))
        didReceiveContentLengthImpl(inspectorController, identifier, lengthReceived);
#endif
}

inline void InspectorInstrumentation::didFinishLoading(Frame* frame, unsigned long identifier, double finishTime)
{
#if ENABLE(INSPECTOR)
    if (InspectorController* inspectorController = inspectorControllerWithFrontendForFrame(frame))
        didFinishLoadingImpl(inspectorController, identifier, finishTime);
#endif
}

inline void InspectorInstrumentation::didFailLoading(Frame* frame, unsigned long identifier, const ResourceError& error)
{
#if ENABLE(INSPECTOR)
    if (InspectorController* inspectorController = inspectorControllerWithFrontendForFrame(frame))
        didFailLoadingImpl(inspectorController, identifier, error);
#endif
}

inline void InspectorInstrumentation::resourceRetrievedByXMLHttpRequest(ScriptExecutionContext* context, unsigned long identifier, const String& sourceString, const String& url, const String& sendURL, unsigned sendLineNumber)
{
#if ENABLE(INSPECTOR)
    if (InspectorController* inspectorController = inspectorControllerForContext(context))
        resourceRetrievedByXMLHttpRequestImpl(inspectorController, identifier, sourceString, url, sendURL, sendLineNumber);
#endif
}

inline void InspectorInstrumentation::scriptImported(ScriptExecutionContext* context, unsigned long identifier, const String& sourceString)
{
#if ENABLE(INSPECTOR)
    if (InspectorController* inspectorController = inspectorControllerForContext(context))
        scriptImportedImpl(inspectorController, identifier, sourceString);
#endif
}

inline void InspectorInstrumentation::mainResourceFiredLoadEvent(Frame* frame, const KURL& url)
{
#if ENABLE(INSPECTOR)
    if (InspectorController* inspectorController = inspectorControllerWithFrontendForFrame(frame))
        mainResourceFiredLoadEventImpl(inspectorController, frame, url);
#endif
}

inline void InspectorInstrumentation::mainResourceFiredDOMContentEvent(Frame* frame, const KURL& url)
{
#if ENABLE(INSPECTOR)
    if (InspectorController* inspectorController = inspectorControllerWithFrontendForFrame(frame))
        mainResourceFiredDOMContentEventImpl(inspectorController, frame, url);
#endif
}

inline void InspectorInstrumentation::frameDetachedFromParent(Frame* frame)
{
#if ENABLE(INSPECTOR)
    if (InspectorController* inspectorController = inspectorControllerWithFrontendForFrame(frame))
        frameDetachedFromParentImpl(inspectorController, frame);
#endif
}

inline void InspectorInstrumentation::didCommitLoad(Frame* frame, DocumentLoader* loader)
{
#if ENABLE(INSPECTOR)
    if (InspectorController* inspectorController = inspectorControllerForFrame(frame))
        didCommitLoadImpl(inspectorController, loader);
#endif
}

inline InspectorInstrumentationCookie InspectorInstrumentation::willWriteHTML(Document* document, unsigned int length, unsigned int startLine)
{
#if ENABLE(INSPECTOR)
    if (InspectorController* inspectorController = inspectorControllerWithFrontendForDocument(document))
        return willWriteHTMLImpl(inspectorController, length, startLine);
#endif
    return InspectorInstrumentationCookie();
}

inline void InspectorInstrumentation::didWriteHTML(const InspectorInstrumentationCookie& cookie, unsigned int endLine)
{
#if ENABLE(INSPECTOR)
    if (hasFrontends() && cookie.first)
        didWriteHTMLImpl(cookie, endLine);
#endif
}

#if ENABLE(DATABASE)
inline void InspectorInstrumentation::didOpenDatabase(ScriptExecutionContext* context, Database* database, const String& domain, const String& name, const String& version)
{
#if ENABLE(INSPECTOR)
    if (InspectorController* inspectorController = inspectorControllerForContext(context))
        didOpenDatabaseImpl(inspectorController, database, domain, name, version);
#endif
}
#endif

#if ENABLE(DOM_STORAGE)
inline void InspectorInstrumentation::didUseDOMStorage(Page* page, StorageArea* storageArea, bool isLocalStorage, Frame* frame)
{
#if ENABLE(INSPECTOR)
    if (InspectorController* inspectorController = inspectorControllerForPage(page))
        didUseDOMStorageImpl(inspectorController, storageArea, isLocalStorage, frame);
#endif
}
#endif

#if ENABLE(WORKERS)
inline void InspectorInstrumentation::didCreateWorker(ScriptExecutionContext* context, intptr_t id, const String& url, bool isSharedWorker)
{
#if ENABLE(INSPECTOR)
    if (InspectorController* inspectorController = inspectorControllerWithFrontendForContext(context))
        didCreateWorkerImpl(inspectorController, id, url, isSharedWorker);
#endif
}

inline void InspectorInstrumentation::didDestroyWorker(ScriptExecutionContext* context, intptr_t id)
{
#if ENABLE(INSPECTOR)
    if (InspectorController* inspectorController = inspectorControllerWithFrontendForContext(context))
        didDestroyWorkerImpl(inspectorController, id);
#endif
}
#endif


#if ENABLE(WEB_SOCKETS)
inline void InspectorInstrumentation::didCreateWebSocket(ScriptExecutionContext* context, unsigned long identifier, const KURL& requestURL, const KURL& documentURL)
{
#if ENABLE(INSPECTOR)
    if (InspectorController* inspectorController = inspectorControllerWithFrontendForContext(context))
        didCreateWebSocketImpl(inspectorController, identifier, requestURL, documentURL);
#endif
}

inline void InspectorInstrumentation::willSendWebSocketHandshakeRequest(ScriptExecutionContext* context, unsigned long identifier, const WebSocketHandshakeRequest& request)
{
#if ENABLE(INSPECTOR)
    if (InspectorController* inspectorController = inspectorControllerWithFrontendForContext(context))
        willSendWebSocketHandshakeRequestImpl(inspectorController, identifier, request);
#endif
}

inline void InspectorInstrumentation::didReceiveWebSocketHandshakeResponse(ScriptExecutionContext* context, unsigned long identifier, const WebSocketHandshakeResponse& response)
{
#if ENABLE(INSPECTOR)
    if (InspectorController* inspectorController = inspectorControllerWithFrontendForContext(context))
        didReceiveWebSocketHandshakeResponseImpl(inspectorController, identifier, response);
#endif
}

inline void InspectorInstrumentation::didCloseWebSocket(ScriptExecutionContext* context, unsigned long identifier)
{
#if ENABLE(INSPECTOR)
    if (InspectorController* inspectorController = inspectorControllerWithFrontendForContext(context))
        didCloseWebSocketImpl(inspectorController, identifier);
#endif
}
#endif

inline void InspectorInstrumentation::networkStateChanged(Page* page)
{
#if ENABLE(INSPECTOR) && ENABLE(OFFLINE_WEB_APPLICATIONS)
    if (InspectorController* inspectorController = inspectorControllerWithFrontendForPage(page))
        networkStateChangedImpl(inspectorController);
#endif
}

#if ENABLE(OFFLINE_WEB_APPLICATIONS)
inline void InspectorInstrumentation::updateApplicationCacheStatus(Frame* frame)
{
#if ENABLE(INSPECTOR)
    if (InspectorController* inspectorController = inspectorControllerWithFrontendForFrame(frame))
        updateApplicationCacheStatusImpl(inspectorController, frame);
#endif
}
#endif

inline void InspectorInstrumentation::addMessageToConsole(Page* page, MessageSource source, MessageType type, MessageLevel level, const String& message, ScriptArguments* arguments, ScriptCallStack* callStack)
{
#if ENABLE(INSPECTOR)
    if (InspectorController* inspectorController = inspectorControllerForPage(page))
        addMessageToConsoleImpl(inspectorController, source, type, level, message, arguments, callStack);
#endif
}

inline void InspectorInstrumentation::addMessageToConsole(Page* page, MessageSource source, MessageType type, MessageLevel level, const String& message, unsigned lineNumber, const String& sourceID)
{
#if ENABLE(INSPECTOR)
    if (InspectorController* inspectorController = inspectorControllerForPage(page))
        addMessageToConsoleImpl(inspectorController, source, type, level, message, lineNumber, sourceID);
#endif
}

inline void InspectorInstrumentation::consoleCount(Page* page, ScriptArguments* arguments, ScriptCallStack* stack)
{
#if ENABLE(INSPECTOR)
    if (InspectorController* inspectorController = inspectorControllerForPage(page))
        consoleCountImpl(inspectorController, arguments, stack);
#endif
}

inline void InspectorInstrumentation::startConsoleTiming(Page* page, const String& title)
{
#if ENABLE(INSPECTOR)
    if (InspectorController* inspectorController = inspectorControllerForPage(page))
        startConsoleTimingImpl(inspectorController, title);
#endif
}

inline void InspectorInstrumentation::stopConsoleTiming(Page* page, const String& title, ScriptCallStack* stack)
{
#if ENABLE(INSPECTOR)
    if (InspectorController* inspectorController = inspectorControllerForPage(page))
        stopConsoleTimingImpl(inspectorController, title, stack);
#endif
}

inline void InspectorInstrumentation::consoleMarkTimeline(Page* page, ScriptArguments* arguments)
{
#if ENABLE(INSPECTOR)
    if (InspectorController* inspectorController = inspectorControllerWithFrontendForPage(page))
        consoleMarkTimelineImpl(inspectorController, arguments);
#endif
}

#if ENABLE(JAVASCRIPT_DEBUGGER)
inline void InspectorInstrumentation::addStartProfilingMessageToConsole(Page* page, const String& title, unsigned lineNumber, const String& sourceURL)
{
#if ENABLE(INSPECTOR)
    if (InspectorController* inspectorController = inspectorControllerForPage(page))
        addStartProfilingMessageToConsoleImpl(inspectorController, title, lineNumber, sourceURL);
#endif
}
#endif

#if ENABLE(INSPECTOR)
inline InspectorController* InspectorInstrumentation::inspectorControllerForContext(ScriptExecutionContext* context)
{
    if (context && context->isDocument())
        return inspectorControllerForPage(static_cast<Document*>(context)->page());
    return 0;
}

inline InspectorController* InspectorInstrumentation::inspectorControllerForFrame(Frame* frame)
{
    if (frame)
        return inspectorControllerForPage(frame->page());
    return 0;
}

inline InspectorController* InspectorInstrumentation::inspectorControllerForPage(Page* page)
{
    if (!page)
        return 0;
    return page->inspectorController();
}

inline InspectorController* InspectorInstrumentation::inspectorControllerWithFrontendForContext(ScriptExecutionContext* context)
{
    if (hasFrontends() && context && context->isDocument())
        return inspectorControllerWithFrontendForPage(static_cast<Document*>(context)->page());
    return 0;
}

inline InspectorController* InspectorInstrumentation::inspectorControllerWithFrontendForDocument(Document* document)
{
    if (hasFrontends() && document)
        return inspectorControllerWithFrontendForPage(document->page());
    return 0;
}

inline InspectorController* InspectorInstrumentation::inspectorControllerWithFrontendForFrame(Frame* frame)
{
    if (hasFrontends() && frame)
        return inspectorControllerWithFrontendForPage(frame->page());
    return 0;
}

inline InspectorController* InspectorInstrumentation::inspectorControllerWithFrontendForPage(Page* page)
{
    if (page) {
        if (InspectorController* inspectorController = inspectorControllerForPage(page)) {
            if (hasFrontend(inspectorController))
                return inspectorController;
        }
    }
    return 0;
}
#endif

} // namespace WebCore

#endif // !defined(InspectorInstrumentation_h)
