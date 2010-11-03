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

#include "Document.h"
#include "Frame.h"
#include "Page.h"
#include "ScriptExecutionContext.h"

namespace WebCore {

class CharacterData;
class Element;
class EventContext;
class InspectorController;
class InspectorTimelineAgent;
class KURL;
class Node;
class ResourceRequest;
class ResourceResponse;
class XMLHttpRequest;

#if ENABLE(WEB_SOCKETS)
class WebSocketHandshakeRequest;
class WebSocketHandshakeResponse;
#endif

typedef pair<InspectorController*, int> InspectorInstrumentationCookie;

class InspectorInstrumentation {
public:
    static void willInsertDOMNode(Document*, Node*, Node* parent);
    static void didInsertDOMNode(Document*, Node*);
    static void willRemoveDOMNode(Document*, Node*);
    static void willModifyDOMAttr(Document*, Element*);
    static void didModifyDOMAttr(Document*, Element*);
    static void characterDataModified(Document*, CharacterData*);

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
    static InspectorInstrumentationCookie willReceiveResourceData(Frame*, unsigned long identifier);
    static void didReceiveResourceData(const InspectorInstrumentationCookie&);
    static InspectorInstrumentationCookie willReceiveResourceResponse(Frame*, unsigned long identifier, const ResourceResponse& response);
    static void didReceiveResourceResponse(const InspectorInstrumentationCookie&);
    static InspectorInstrumentationCookie willWriteHTML(Document*, unsigned int length, unsigned int startLine);
    static void didWriteHTML(const InspectorInstrumentationCookie&, unsigned int endLine);

#if ENABLE(WEB_SOCKETS)
    static void didCreateWebSocket(ScriptExecutionContext*, unsigned long identifier, const KURL& requestURL, const KURL& documentURL);
    static void willSendWebSocketHandshakeRequest(ScriptExecutionContext*, unsigned long identifier, const WebSocketHandshakeRequest&);
    static void didReceiveWebSocketHandshakeResponse(ScriptExecutionContext*, unsigned long identifier, const WebSocketHandshakeResponse&);
    static void didCloseWebSocket(ScriptExecutionContext*, unsigned long identifier);
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
    static void willInsertDOMNodeImpl(InspectorController*, Node* node, Node* parent);
    static void didInsertDOMNodeImpl(InspectorController*, Node*);
    static void willRemoveDOMNodeImpl(InspectorController*, Node*);
    static void didRemoveDOMNodeImpl(InspectorController*, Node*);
    static void willModifyDOMAttrImpl(InspectorController*, Element*);
    static void didModifyDOMAttrImpl(InspectorController*, Element*);
    static void characterDataModifiedImpl(InspectorController*, CharacterData*);

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
    static InspectorInstrumentationCookie willReceiveResourceDataImpl(InspectorController*, unsigned long identifier);
    static void didReceiveResourceDataImpl(const InspectorInstrumentationCookie&);
    static InspectorInstrumentationCookie willReceiveResourceResponseImpl(InspectorController*, unsigned long identifier, const ResourceResponse& response);
    static void didReceiveResourceResponseImpl(const InspectorInstrumentationCookie&);
    static InspectorInstrumentationCookie willWriteHTMLImpl(InspectorController*, unsigned int length, unsigned int startLine);
    static void didWriteHTMLImpl(const InspectorInstrumentationCookie&, unsigned int endLine);

#if ENABLE(WEB_SOCKETS)
    static void didCreateWebSocketImpl(InspectorController*, unsigned long identifier, const KURL& requestURL, const KURL& documentURL);
    static void willSendWebSocketHandshakeRequestImpl(InspectorController*, unsigned long identifier, const WebSocketHandshakeRequest&);
    static void didReceiveWebSocketHandshakeResponseImpl(InspectorController*, unsigned long identifier, const WebSocketHandshakeResponse&);
    static void didCloseWebSocketImpl(InspectorController*, unsigned long identifier);
#endif

    static InspectorController* inspectorControllerForContext(ScriptExecutionContext*);
    static InspectorController* inspectorControllerForDocument(Document*);
    static InspectorController* inspectorControllerForFrame(Frame*);
    static InspectorController* inspectorControllerForPage(Page*);

    static bool hasFrontend(InspectorController*);
    static void pauseOnNativeEventIfNeeded(InspectorController*, const String& categoryType, const String& eventName, bool synchronous);
    static void cancelPauseOnNativeEvent(InspectorController*);
    static InspectorTimelineAgent* retrieveTimelineAgent(InspectorController*);
    static InspectorTimelineAgent* retrieveTimelineAgent(const InspectorInstrumentationCookie&);

    static int s_frontendCounter;
#endif
};

inline void InspectorInstrumentation::willInsertDOMNode(Document* document, Node* node, Node* parent)
{
#if ENABLE(INSPECTOR)
    if (InspectorController* inspectorController = inspectorControllerForDocument(document))
        willInsertDOMNodeImpl(inspectorController, node, parent);
#endif
}

inline void InspectorInstrumentation::didInsertDOMNode(Document* document, Node* node)
{
#if ENABLE(INSPECTOR)
    if (InspectorController* inspectorController = inspectorControllerForDocument(document))
        didInsertDOMNodeImpl(inspectorController, node);
#endif
}

inline void InspectorInstrumentation::willRemoveDOMNode(Document* document, Node* node)
{
#if ENABLE(INSPECTOR)
    if (InspectorController* inspectorController = inspectorControllerForDocument(document)) {
        willRemoveDOMNodeImpl(inspectorController, node);
        didRemoveDOMNodeImpl(inspectorController, node);
    }
#endif
}

inline void InspectorInstrumentation::willModifyDOMAttr(Document* document, Element* element)
{
#if ENABLE(INSPECTOR)
    if (InspectorController* inspectorController = inspectorControllerForDocument(document))
        willModifyDOMAttrImpl(inspectorController, element);
#endif
}

inline void InspectorInstrumentation::didModifyDOMAttr(Document* document, Element* element)
{
#if ENABLE(INSPECTOR)
    if (InspectorController* inspectorController = inspectorControllerForDocument(document))
        didModifyDOMAttrImpl(inspectorController, element);
#endif
}

inline void InspectorInstrumentation::characterDataModified(Document* document, CharacterData* characterData)
{
#if ENABLE(INSPECTOR)
    if (InspectorController* inspectorController = inspectorControllerForDocument(document))
        characterDataModifiedImpl(inspectorController, characterData);
#endif
}

inline void InspectorInstrumentation::willSendXMLHttpRequest(ScriptExecutionContext* context, const String& url)
{
#if ENABLE(INSPECTOR)
    if (InspectorController* inspectorController = inspectorControllerForContext(context))
        willSendXMLHttpRequestImpl(inspectorController, url);
#endif
}

inline void InspectorInstrumentation::didScheduleResourceRequest(Document* document, const String& url)
{
#if ENABLE(INSPECTOR)
    if (InspectorController* inspectorController = inspectorControllerForDocument(document))
        didScheduleResourceRequestImpl(inspectorController, url);
#endif
}

inline void InspectorInstrumentation::didInstallTimer(ScriptExecutionContext* context, int timerId, int timeout, bool singleShot)
{
#if ENABLE(INSPECTOR)
    if (InspectorController* inspectorController = inspectorControllerForContext(context))
        didInstallTimerImpl(inspectorController, timerId, timeout, singleShot);
#endif
}

inline void InspectorInstrumentation::didRemoveTimer(ScriptExecutionContext* context, int timerId)
{
#if ENABLE(INSPECTOR)
    if (InspectorController* inspectorController = inspectorControllerForContext(context))
        didRemoveTimerImpl(inspectorController, timerId);
#endif
}


inline InspectorInstrumentationCookie InspectorInstrumentation::willCallFunction(Frame* frame, const String& scriptName, int scriptLine)
{
#if ENABLE(INSPECTOR)
    if (InspectorController* inspectorController = inspectorControllerForFrame(frame))
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
    if (InspectorController* inspectorController = inspectorControllerForContext(context))
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
    if (InspectorController* inspectorController = inspectorControllerForDocument(document))
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
    if (InspectorController* inspectorController = inspectorControllerForFrame(frame))
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
    if (InspectorController* inspectorController = inspectorControllerForFrame(frame))
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
    if (InspectorController* inspectorController = inspectorControllerForContext(context))
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
    if (InspectorController* inspectorController = inspectorControllerForFrame(frame))
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
    if (InspectorController* inspectorController = inspectorControllerForContext(context))
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
    if (InspectorController* inspectorController = inspectorControllerForFrame(frame))
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
    if (InspectorController* inspectorController = inspectorControllerForDocument(document))
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

inline InspectorInstrumentationCookie InspectorInstrumentation::willReceiveResourceData(Frame* frame, unsigned long identifier)
{
#if ENABLE(INSPECTOR)
    if (InspectorController* inspectorController = inspectorControllerForFrame(frame))
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
    if (InspectorController* inspectorController = inspectorControllerForFrame(frame))
        return willReceiveResourceResponseImpl(inspectorController, identifier, response);
#endif
    return InspectorInstrumentationCookie();
}

inline void InspectorInstrumentation::didReceiveResourceResponse(const InspectorInstrumentationCookie& cookie)
{
#if ENABLE(INSPECTOR)
    if (hasFrontends() && cookie.first)
        didReceiveResourceResponseImpl(cookie);
#endif
}

inline InspectorInstrumentationCookie InspectorInstrumentation::willWriteHTML(Document* document, unsigned int length, unsigned int startLine)
{
#if ENABLE(INSPECTOR)
    if (InspectorController* inspectorController = inspectorControllerForDocument(document))
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

#if ENABLE(WEB_SOCKETS)
inline void InspectorInstrumentation::didCreateWebSocket(ScriptExecutionContext* context, unsigned long identifier, const KURL& requestURL, const KURL& documentURL)
{
#if ENABLE(INSPECTOR)
    if (InspectorController* inspectorController = inspectorControllerForContext(context))
        didCreateWebSocketImpl(inspectorController, identifier, requestURL, documentURL);
#endif
}

inline void InspectorInstrumentation::willSendWebSocketHandshakeRequest(ScriptExecutionContext* context, unsigned long identifier, const WebSocketHandshakeRequest& request)
{
#if ENABLE(INSPECTOR)
    if (InspectorController* inspectorController = inspectorControllerForContext(context))
        willSendWebSocketHandshakeRequestImpl(inspectorController, identifier, request);
#endif
}

inline void InspectorInstrumentation::didReceiveWebSocketHandshakeResponse(ScriptExecutionContext* context, unsigned long identifier, const WebSocketHandshakeResponse& response)
{
#if ENABLE(INSPECTOR)
    if (InspectorController* inspectorController = inspectorControllerForContext(context))
        didReceiveWebSocketHandshakeResponseImpl(inspectorController, identifier, response);
#endif
}

inline void InspectorInstrumentation::didCloseWebSocket(ScriptExecutionContext* context, unsigned long identifier)
{
#if ENABLE(INSPECTOR)
    if (InspectorController* inspectorController = inspectorControllerForContext(context))
        didCloseWebSocketImpl(inspectorController, identifier);
#endif
}
#endif

#if ENABLE(INSPECTOR)
inline InspectorController* InspectorInstrumentation::inspectorControllerForContext(ScriptExecutionContext* context)
{
    if (hasFrontends() && context && context->isDocument())
        return inspectorControllerForPage(static_cast<Document*>(context)->page());
    return 0;
}

inline InspectorController* InspectorInstrumentation::inspectorControllerForDocument(Document* document)
{
    if (hasFrontends() && document)
        return inspectorControllerForPage(document->page());
    return 0;
}

inline InspectorController* InspectorInstrumentation::inspectorControllerForFrame(Frame* frame)
{
    if (hasFrontends() && frame)
        return inspectorControllerForPage(frame->page());
    return 0;
}

inline InspectorController* InspectorInstrumentation::inspectorControllerForPage(Page* page)
{
    if (page) {
        if (InspectorController* inspectorController = page->inspectorController()) {
            if (hasFrontend(inspectorController))
                return inspectorController;
        }
    }
    return 0;
}
#endif

} // namespace WebCore

#endif // !defined(InspectorInstrumentation_h)
