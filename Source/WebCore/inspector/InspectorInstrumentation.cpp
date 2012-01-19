/*
* Copyright (C) 2011 Google Inc. All rights reserved.
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

#include "config.h"
#include "InspectorInstrumentation.h"

#if ENABLE(INSPECTOR)

#include "CSSRule.h"
#include "CSSStyleRule.h"
#include "DOMFileSystem.h"
#include "DOMWindow.h"
#include "Database.h"
#include "DocumentLoader.h"
#include "Event.h"
#include "EventContext.h"
#include "InspectorAgent.h"
#include "InspectorApplicationCacheAgent.h"
#include "InspectorDOMDebuggerAgent.h"
#include "InspectorCSSAgent.h"
#include "InspectorConsoleAgent.h"
#include "InspectorController.h"
#include "WorkerInspectorController.h"
#include "InspectorDatabaseAgent.h"
#include "InspectorDOMAgent.h"
#include "InspectorDOMStorageAgent.h"
#include "InspectorDebuggerAgent.h"
#include "InspectorFileSystemAgent.h"
#include "InspectorPageAgent.h"
#include "InspectorProfilerAgent.h"
#include "InspectorResourceAgent.h"
#include "InspectorRuntimeAgent.h"
#include "InspectorTimelineAgent.h"
#include "InspectorWorkerAgent.h"
#include "InstrumentingAgents.h"
#include "ScriptArguments.h"
#include "ScriptCallStack.h"
#include "ScriptProfile.h"
#include "WorkerContext.h"
#include "WorkerThread.h"
#include "XMLHttpRequest.h"
#include <wtf/StdLibExtras.h>
#include <wtf/text/CString.h>

namespace WebCore {

static const char* const listenerEventCategoryType = "listener";
static const char* const instrumentationEventCategoryType = "instrumentation";

static const char* const setTimerEventName = "setTimer";
static const char* const clearTimerEventName = "clearTimer";
static const char* const timerFiredEventName = "timerFired";

int InspectorInstrumentation::s_frontendCounter = 0;

static bool eventHasListeners(const AtomicString& eventType, DOMWindow* window, Node* node, const Vector<EventContext>& ancestors)
{
    if (window && window->hasEventListeners(eventType))
        return true;

    if (node->hasEventListeners(eventType))
        return true;

    for (size_t i = 0; i < ancestors.size(); i++) {
        Node* ancestor = ancestors[i].node();
        if (ancestor->hasEventListeners(eventType))
            return true;
    }

    return false;
}

void InspectorInstrumentation::didClearWindowObjectInWorldImpl(InstrumentingAgents* instrumentingAgents, Frame* frame, DOMWrapperWorld* world)
{
    InspectorPageAgent* pageAgent = instrumentingAgents->inspectorPageAgent();
    if (pageAgent)
        pageAgent->didClearWindowObjectInWorld(frame, world);
    if (InspectorAgent* inspectorAgent = instrumentingAgents->inspectorAgent())
        inspectorAgent->didClearWindowObjectInWorld(frame, world);
#if ENABLE(JAVASCRIPT_DEBUGGER)
    if (InspectorDebuggerAgent* debuggerAgent = instrumentingAgents->inspectorDebuggerAgent()) {
        if (pageAgent && world == mainThreadNormalWorld() && frame == pageAgent->mainFrame())
            debuggerAgent->didClearMainFrameWindowObject();
    }
#endif
}

void InspectorInstrumentation::willInsertDOMNodeImpl(InstrumentingAgents* instrumentingAgents, Node* node, Node* parent)
{
#if ENABLE(JAVASCRIPT_DEBUGGER)
    if (InspectorDOMDebuggerAgent* domDebuggerAgent = instrumentingAgents->inspectorDOMDebuggerAgent())
        domDebuggerAgent->willInsertDOMNode(node, parent);
#endif
}

void InspectorInstrumentation::didInsertDOMNodeImpl(InstrumentingAgents* instrumentingAgents, Node* node)
{
    if (InspectorDOMAgent* domAgent = instrumentingAgents->inspectorDOMAgent())
        domAgent->didInsertDOMNode(node);
#if ENABLE(JAVASCRIPT_DEBUGGER)
    if (InspectorDOMDebuggerAgent* domDebuggerAgent = instrumentingAgents->inspectorDOMDebuggerAgent())
        domDebuggerAgent->didInsertDOMNode(node);
#endif
}

void InspectorInstrumentation::willRemoveDOMNodeImpl(InstrumentingAgents* instrumentingAgents, Node* node)
{
#if ENABLE(JAVASCRIPT_DEBUGGER)
    if (InspectorDOMDebuggerAgent* domDebuggerAgent = instrumentingAgents->inspectorDOMDebuggerAgent())
        domDebuggerAgent->willRemoveDOMNode(node);
#endif
}

void InspectorInstrumentation::didRemoveDOMNodeImpl(InstrumentingAgents* instrumentingAgents, Node* node)
{
#if ENABLE(JAVASCRIPT_DEBUGGER)
    if (InspectorDOMDebuggerAgent* domDebuggerAgent = instrumentingAgents->inspectorDOMDebuggerAgent())
        domDebuggerAgent->didRemoveDOMNode(node);
#endif
    if (InspectorDOMAgent* domAgent = instrumentingAgents->inspectorDOMAgent())
        domAgent->didRemoveDOMNode(node);
}

void InspectorInstrumentation::willModifyDOMAttrImpl(InstrumentingAgents* instrumentingAgents, Element* element)
{
#if ENABLE(JAVASCRIPT_DEBUGGER)
    if (InspectorDOMDebuggerAgent* domDebuggerAgent = instrumentingAgents->inspectorDOMDebuggerAgent())
        domDebuggerAgent->willModifyDOMAttr(element);
#endif
}

void InspectorInstrumentation::didModifyDOMAttrImpl(InstrumentingAgents* instrumentingAgents, Element* element, const AtomicString& name, const AtomicString& value)
{
    if (InspectorDOMAgent* domAgent = instrumentingAgents->inspectorDOMAgent())
        domAgent->didModifyDOMAttr(element, name, value);
}

void InspectorInstrumentation::didRemoveDOMAttrImpl(InstrumentingAgents* instrumentingAgents, Element* element, const AtomicString& name)
{
    if (InspectorDOMAgent* domAgent = instrumentingAgents->inspectorDOMAgent())
        domAgent->didRemoveDOMAttr(element, name);
}

void InspectorInstrumentation::didInvalidateStyleAttrImpl(InstrumentingAgents* instrumentingAgents, Node* node)
{
    if (InspectorDOMAgent* domAgent = instrumentingAgents->inspectorDOMAgent())
        domAgent->didInvalidateStyleAttr(node);
#if ENABLE(JAVASCRIPT_DEBUGGER)
    if (InspectorDOMDebuggerAgent* domDebuggerAgent = instrumentingAgents->inspectorDOMDebuggerAgent())
        domDebuggerAgent->didInvalidateStyleAttr(node);
#endif
}

void InspectorInstrumentation::frameWindowDiscardedImpl(InstrumentingAgents* instrumentingAgents, DOMWindow* window)
{
    if (InspectorConsoleAgent* consoleAgent = instrumentingAgents->inspectorConsoleAgent())
        consoleAgent->frameWindowDiscarded(window);
}

void InspectorInstrumentation::mediaQueryResultChangedImpl(InstrumentingAgents* instrumentingAgents)
{
    if (InspectorCSSAgent* cssAgent = instrumentingAgents->inspectorCSSAgent())
        cssAgent->mediaQueryResultChanged();
}

void InspectorInstrumentation::mouseDidMoveOverElementImpl(InstrumentingAgents* instrumentingAgents, const HitTestResult& result, unsigned modifierFlags)
{
    if (InspectorDOMAgent* domAgent = instrumentingAgents->inspectorDOMAgent())
        domAgent->mouseDidMoveOverElement(result, modifierFlags);
}

bool InspectorInstrumentation::handleMousePressImpl(InstrumentingAgents* instrumentingAgents)
{
    if (InspectorDOMAgent* domAgent = instrumentingAgents->inspectorDOMAgent())
        return domAgent->handleMousePress();
    return false;
}

bool InspectorInstrumentation::forcePseudoStateImpl(InstrumentingAgents* instrumentingAgents, Element* element, CSSSelector::PseudoType pseudoState)
{
    if (InspectorCSSAgent* cssAgent = instrumentingAgents->inspectorCSSAgent())
        return cssAgent->forcePseudoState(element, pseudoState);
    return false;
}

void InspectorInstrumentation::characterDataModifiedImpl(InstrumentingAgents* instrumentingAgents, CharacterData* characterData)
{
    if (InspectorDOMAgent* domAgent = instrumentingAgents->inspectorDOMAgent())
        domAgent->characterDataModified(characterData);
}

void InspectorInstrumentation::willSendXMLHttpRequestImpl(InstrumentingAgents* instrumentingAgents, const String& url)
{
#if ENABLE(JAVASCRIPT_DEBUGGER)
    if (InspectorDOMDebuggerAgent* domDebuggerAgent = instrumentingAgents->inspectorDOMDebuggerAgent())
        domDebuggerAgent->willSendXMLHttpRequest(url);
#endif
}

void InspectorInstrumentation::didScheduleResourceRequestImpl(InstrumentingAgents* instrumentingAgents, const String& url)
{
    if (InspectorTimelineAgent* timelineAgent = instrumentingAgents->inspectorTimelineAgent())
        timelineAgent->didScheduleResourceRequest(url);
}

void InspectorInstrumentation::didInstallTimerImpl(InstrumentingAgents* instrumentingAgents, int timerId, int timeout, bool singleShot)
{
    pauseOnNativeEventIfNeeded(instrumentingAgents, instrumentationEventCategoryType, setTimerEventName, true);
    if (InspectorTimelineAgent* timelineAgent = instrumentingAgents->inspectorTimelineAgent())
        timelineAgent->didInstallTimer(timerId, timeout, singleShot);
}

void InspectorInstrumentation::didRemoveTimerImpl(InstrumentingAgents* instrumentingAgents, int timerId)
{
    pauseOnNativeEventIfNeeded(instrumentingAgents, instrumentationEventCategoryType, clearTimerEventName, true);
    if (InspectorTimelineAgent* timelineAgent = instrumentingAgents->inspectorTimelineAgent())
        timelineAgent->didRemoveTimer(timerId);
}

#if USE(JSC) // It is disabled for JSC see WK-BUG 40119
InspectorInstrumentationCookie InspectorInstrumentation::willCallFunctionImpl(InstrumentingAgents*, const String&, int)
{
    return InspectorInstrumentationCookie();
}
#else
InspectorInstrumentationCookie InspectorInstrumentation::willCallFunctionImpl(InstrumentingAgents* instrumentingAgents, const String& scriptName, int scriptLine)
{
    int timelineAgentId = 0;
    if (InspectorTimelineAgent* timelineAgent = instrumentingAgents->inspectorTimelineAgent()) {
        timelineAgent->willCallFunction(scriptName, scriptLine);
        timelineAgentId = timelineAgent->id();
    }
    return InspectorInstrumentationCookie(instrumentingAgents, timelineAgentId);
}
#endif

void InspectorInstrumentation::didCallFunctionImpl(const InspectorInstrumentationCookie& cookie)
{
    if (InspectorTimelineAgent* timelineAgent = retrieveTimelineAgent(cookie))
        timelineAgent->didCallFunction();
}

InspectorInstrumentationCookie InspectorInstrumentation::willChangeXHRReadyStateImpl(InstrumentingAgents* instrumentingAgents, XMLHttpRequest* request)
{
    int timelineAgentId = 0;
    InspectorTimelineAgent* timelineAgent = instrumentingAgents->inspectorTimelineAgent();
    if (timelineAgent && request->hasEventListeners(eventNames().readystatechangeEvent)) {
        timelineAgent->willChangeXHRReadyState(request->url().string(), request->readyState());
        timelineAgentId = timelineAgent->id();
    }
    return InspectorInstrumentationCookie(instrumentingAgents, timelineAgentId);
}

void InspectorInstrumentation::didChangeXHRReadyStateImpl(const InspectorInstrumentationCookie& cookie)
{
    if (InspectorTimelineAgent* timelineAgent = retrieveTimelineAgent(cookie))
        timelineAgent->didChangeXHRReadyState();
}

InspectorInstrumentationCookie InspectorInstrumentation::willDispatchEventImpl(InstrumentingAgents* instrumentingAgents, const Event& event, DOMWindow* window, Node* node, const Vector<EventContext>& ancestors)
{
    pauseOnNativeEventIfNeeded(instrumentingAgents, listenerEventCategoryType, event.type(), false);

    int timelineAgentId = 0;
    InspectorTimelineAgent* timelineAgent = instrumentingAgents->inspectorTimelineAgent();
    if (timelineAgent && eventHasListeners(event.type(), window, node, ancestors)) {
        timelineAgent->willDispatchEvent(event);
        timelineAgentId = timelineAgent->id();
    }
    return InspectorInstrumentationCookie(instrumentingAgents, timelineAgentId);
}

void InspectorInstrumentation::didDispatchEventImpl(const InspectorInstrumentationCookie& cookie)
{
    cancelPauseOnNativeEvent(cookie.first);

    if (InspectorTimelineAgent* timelineAgent = retrieveTimelineAgent(cookie))
        timelineAgent->didDispatchEvent();
}

InspectorInstrumentationCookie InspectorInstrumentation::willDispatchEventOnWindowImpl(InstrumentingAgents* instrumentingAgents, const Event& event, DOMWindow* window)
{
    pauseOnNativeEventIfNeeded(instrumentingAgents, listenerEventCategoryType, event.type(), false);

    int timelineAgentId = 0;
    InspectorTimelineAgent* timelineAgent = instrumentingAgents->inspectorTimelineAgent();
    if (timelineAgent && window->hasEventListeners(event.type())) {
        timelineAgent->willDispatchEvent(event);
        timelineAgentId = timelineAgent->id();
    }
    return InspectorInstrumentationCookie(instrumentingAgents, timelineAgentId);
}

void InspectorInstrumentation::didDispatchEventOnWindowImpl(const InspectorInstrumentationCookie& cookie)
{
    cancelPauseOnNativeEvent(cookie.first);

    if (InspectorTimelineAgent* timelineAgent = retrieveTimelineAgent(cookie))
        timelineAgent->didDispatchEvent();
}

InspectorInstrumentationCookie InspectorInstrumentation::willEvaluateScriptImpl(InstrumentingAgents* instrumentingAgents, const String& url, int lineNumber)
{
    int timelineAgentId = 0;
    if (InspectorTimelineAgent* timelineAgent = instrumentingAgents->inspectorTimelineAgent()) {
        timelineAgent->willEvaluateScript(url, lineNumber);
        timelineAgentId = timelineAgent->id();
    }
    return InspectorInstrumentationCookie(instrumentingAgents, timelineAgentId);
}

void InspectorInstrumentation::didEvaluateScriptImpl(const InspectorInstrumentationCookie& cookie)
{
    if (InspectorTimelineAgent* timelineAgent = retrieveTimelineAgent(cookie))
        timelineAgent->didEvaluateScript();
}

InspectorInstrumentationCookie InspectorInstrumentation::willFireTimerImpl(InstrumentingAgents* instrumentingAgents, int timerId)
{
    pauseOnNativeEventIfNeeded(instrumentingAgents, instrumentationEventCategoryType, timerFiredEventName, false);

    int timelineAgentId = 0;
    if (InspectorTimelineAgent* timelineAgent = instrumentingAgents->inspectorTimelineAgent()) {
        timelineAgent->willFireTimer(timerId);
        timelineAgentId = timelineAgent->id();
    }
    return InspectorInstrumentationCookie(instrumentingAgents, timelineAgentId);
}

void InspectorInstrumentation::didFireTimerImpl(const InspectorInstrumentationCookie& cookie)
{
    cancelPauseOnNativeEvent(cookie.first);

    if (InspectorTimelineAgent* timelineAgent = retrieveTimelineAgent(cookie))
        timelineAgent->didFireTimer();
}

InspectorInstrumentationCookie InspectorInstrumentation::willLayoutImpl(InstrumentingAgents* instrumentingAgents)
{
    int timelineAgentId = 0;
    if (InspectorTimelineAgent* timelineAgent = instrumentingAgents->inspectorTimelineAgent()) {
        timelineAgent->willLayout();
        timelineAgentId = timelineAgent->id();
    }
    return InspectorInstrumentationCookie(instrumentingAgents, timelineAgentId);
}

void InspectorInstrumentation::didLayoutImpl(const InspectorInstrumentationCookie& cookie)
{
    if (InspectorTimelineAgent* timelineAgent = retrieveTimelineAgent(cookie))
        timelineAgent->didLayout();
}

InspectorInstrumentationCookie InspectorInstrumentation::willLoadXHRImpl(InstrumentingAgents* instrumentingAgents, XMLHttpRequest* request)
{
    int timelineAgentId = 0;
    InspectorTimelineAgent* timelineAgent = instrumentingAgents->inspectorTimelineAgent();
    if (timelineAgent && request->hasEventListeners(eventNames().loadEvent)) {
        timelineAgent->willLoadXHR(request->url());
        timelineAgentId = timelineAgent->id();
    }
    return InspectorInstrumentationCookie(instrumentingAgents, timelineAgentId);
}

void InspectorInstrumentation::didLoadXHRImpl(const InspectorInstrumentationCookie& cookie)
{
    if (InspectorTimelineAgent* timelineAgent = retrieveTimelineAgent(cookie))
        timelineAgent->didLoadXHR();
}

InspectorInstrumentationCookie InspectorInstrumentation::willPaintImpl(InstrumentingAgents* instrumentingAgents, const LayoutRect& rect)
{
    int timelineAgentId = 0;
    if (InspectorTimelineAgent* timelineAgent = instrumentingAgents->inspectorTimelineAgent()) {
        timelineAgent->willPaint(rect);
        timelineAgentId = timelineAgent->id();
    }
    return InspectorInstrumentationCookie(instrumentingAgents, timelineAgentId);
}

void InspectorInstrumentation::didPaintImpl(const InspectorInstrumentationCookie& cookie)
{
    if (InspectorTimelineAgent* timelineAgent = retrieveTimelineAgent(cookie))
        timelineAgent->didPaint();
}

InspectorInstrumentationCookie InspectorInstrumentation::willRecalculateStyleImpl(InstrumentingAgents* instrumentingAgents)
{
    int timelineAgentId = 0;
    if (InspectorTimelineAgent* timelineAgent = instrumentingAgents->inspectorTimelineAgent()) {
        timelineAgent->willRecalculateStyle();
        timelineAgentId = timelineAgent->id();
    }
    if (InspectorResourceAgent* resourceAgent = instrumentingAgents->inspectorResourceAgent())
        resourceAgent->willRecalculateStyle();
    return InspectorInstrumentationCookie(instrumentingAgents, timelineAgentId);
}

void InspectorInstrumentation::didRecalculateStyleImpl(const InspectorInstrumentationCookie& cookie)
{
    if (InspectorTimelineAgent* timelineAgent = retrieveTimelineAgent(cookie))
        timelineAgent->didRecalculateStyle();
    InstrumentingAgents* instrumentingAgents = cookie.first;
    if (!instrumentingAgents)
        return;
    if (InspectorResourceAgent* resourceAgent = instrumentingAgents->inspectorResourceAgent())
        resourceAgent->didRecalculateStyle();
}

void InspectorInstrumentation::didScheduleStyleRecalculationImpl(InstrumentingAgents* instrumentingAgents, Document* document)
{
    if (InspectorResourceAgent* resourceAgent = instrumentingAgents->inspectorResourceAgent())
        resourceAgent->didScheduleStyleRecalculation(document);
}

InspectorInstrumentationCookie InspectorInstrumentation::willMatchRuleImpl(InstrumentingAgents* instrumentingAgents, const CSSStyleRule* rule)
{
    InspectorCSSAgent* cssAgent = instrumentingAgents->inspectorCSSAgent();
    if (cssAgent) {
        cssAgent->willMatchRule(rule);
        return InspectorInstrumentationCookie(instrumentingAgents, 1);
    }

    return InspectorInstrumentationCookie();
}

void InspectorInstrumentation::didMatchRuleImpl(const InspectorInstrumentationCookie& cookie, bool matched)
{
    InspectorCSSAgent* cssAgent = cookie.first->inspectorCSSAgent();
    if (cssAgent)
        cssAgent->didMatchRule(matched);
}

InspectorInstrumentationCookie InspectorInstrumentation::willProcessRuleImpl(InstrumentingAgents* instrumentingAgents, const CSSRule* rule)
{
    if (!rule->isStyleRule())
        return InspectorInstrumentationCookie();

    InspectorCSSAgent* cssAgent = instrumentingAgents->inspectorCSSAgent();
    if (cssAgent) {
        cssAgent->willProcessRule(static_cast<const CSSStyleRule*>(rule));
        return InspectorInstrumentationCookie(instrumentingAgents, 1);
    }

    return InspectorInstrumentationCookie();
}

void InspectorInstrumentation::didProcessRuleImpl(const InspectorInstrumentationCookie& cookie)
{
    InspectorCSSAgent* cssAgent = cookie.first->inspectorCSSAgent();
    if (cssAgent)
        cssAgent->didProcessRule();
}

void InspectorInstrumentation::applyUserAgentOverrideImpl(InstrumentingAgents* instrumentingAgents, String* userAgent)
{
    if (InspectorResourceAgent* resourceAgent = instrumentingAgents->inspectorResourceAgent())
        resourceAgent->applyUserAgentOverride(userAgent);
}

void InspectorInstrumentation::applyScreenWidthOverrideImpl(InstrumentingAgents* instrumentingAgents, long* width)
{
    if (InspectorPageAgent* pageAgent = instrumentingAgents->inspectorPageAgent())
        pageAgent->applyScreenWidthOverride(width);
}

void InspectorInstrumentation::applyScreenHeightOverrideImpl(InstrumentingAgents* instrumentingAgents, long* height)
{
    if (InspectorPageAgent* pageAgent = instrumentingAgents->inspectorPageAgent())
        pageAgent->applyScreenHeightOverride(height);
}

void InspectorInstrumentation::willSendRequestImpl(InstrumentingAgents* instrumentingAgents, unsigned long identifier, DocumentLoader* loader, ResourceRequest& request, const ResourceResponse& redirectResponse)
{
    if (InspectorTimelineAgent* timelineAgent = instrumentingAgents->inspectorTimelineAgent())
        timelineAgent->willSendResourceRequest(identifier, request);
    if (InspectorResourceAgent* resourceAgent = instrumentingAgents->inspectorResourceAgent())
        resourceAgent->willSendRequest(identifier, loader, request, redirectResponse);
}

void InspectorInstrumentation::continueAfterPingLoaderImpl(InstrumentingAgents* instrumentingAgents, unsigned long identifier, DocumentLoader* loader, ResourceRequest& request, const ResourceResponse& response)
{
    willSendRequestImpl(instrumentingAgents, identifier, loader, request, response);
}

void InspectorInstrumentation::markResourceAsCachedImpl(InstrumentingAgents* instrumentingAgents, unsigned long identifier)
{
    if (InspectorResourceAgent* resourceAgent = instrumentingAgents->inspectorResourceAgent())
        resourceAgent->markResourceAsCached(identifier);
}

void InspectorInstrumentation::didLoadResourceFromMemoryCacheImpl(InstrumentingAgents* instrumentingAgents, DocumentLoader* loader, CachedResource* cachedResource)
{
    InspectorAgent* inspectorAgent = instrumentingAgents->inspectorAgent();
    if (!inspectorAgent || !inspectorAgent->developerExtrasEnabled())
        return;
    if (InspectorResourceAgent* resourceAgent = instrumentingAgents->inspectorResourceAgent())
        resourceAgent->didLoadResourceFromMemoryCache(loader, cachedResource);
}

InspectorInstrumentationCookie InspectorInstrumentation::willReceiveResourceDataImpl(InstrumentingAgents* instrumentingAgents, unsigned long identifier)
{
    int timelineAgentId = 0;
    if (InspectorTimelineAgent* timelineAgent = instrumentingAgents->inspectorTimelineAgent()) {
        timelineAgent->willReceiveResourceData(identifier);
        timelineAgentId = timelineAgent->id();
    }
    return InspectorInstrumentationCookie(instrumentingAgents, timelineAgentId);
}

void InspectorInstrumentation::didReceiveResourceDataImpl(const InspectorInstrumentationCookie& cookie)
{
    if (InspectorTimelineAgent* timelineAgent = retrieveTimelineAgent(cookie))
        timelineAgent->didReceiveResourceData();
}

InspectorInstrumentationCookie InspectorInstrumentation::willReceiveResourceResponseImpl(InstrumentingAgents* instrumentingAgents, unsigned long identifier, const ResourceResponse& response)
{
    int timelineAgentId = 0;
    InspectorTimelineAgent* timelineAgent = instrumentingAgents->inspectorTimelineAgent();
    if (timelineAgent) {
        timelineAgent->willReceiveResourceResponse(identifier, response);
        timelineAgentId = timelineAgent->id();
    }
    return InspectorInstrumentationCookie(instrumentingAgents, timelineAgentId);
}

void InspectorInstrumentation::didReceiveResourceResponseImpl(const InspectorInstrumentationCookie& cookie, unsigned long identifier, DocumentLoader* loader, const ResourceResponse& response)
{
    if (InspectorTimelineAgent* timelineAgent = retrieveTimelineAgent(cookie))
        timelineAgent->didReceiveResourceResponse();
    if (!loader)
        return;
    InstrumentingAgents* instrumentingAgents = cookie.first;
    if (!instrumentingAgents)
        return;
    if (InspectorResourceAgent* resourceAgent = instrumentingAgents->inspectorResourceAgent())
        resourceAgent->didReceiveResponse(identifier, loader, response);
    if (InspectorConsoleAgent* consoleAgent = instrumentingAgents->inspectorConsoleAgent())
        consoleAgent->didReceiveResponse(identifier, response); // This should come AFTER resource notification, front-end relies on this.
}

void InspectorInstrumentation::didReceiveResourceResponseButCanceledImpl(Frame* frame, DocumentLoader* loader, unsigned long identifier, const ResourceResponse& r)
{
    InspectorInstrumentationCookie cookie = InspectorInstrumentation::willReceiveResourceResponse(frame, identifier, r);
    InspectorInstrumentation::didReceiveResourceResponse(cookie, identifier, loader, r);
}

void InspectorInstrumentation::continueAfterXFrameOptionsDeniedImpl(Frame* frame, DocumentLoader* loader, unsigned long identifier, const ResourceResponse& r)
{
    didReceiveResourceResponseButCanceledImpl(frame, loader, identifier, r);
}

void InspectorInstrumentation::continueWithPolicyDownloadImpl(Frame* frame, DocumentLoader* loader, unsigned long identifier, const ResourceResponse& r)
{
    didReceiveResourceResponseButCanceledImpl(frame, loader, identifier, r);
}

void InspectorInstrumentation::continueWithPolicyIgnoreImpl(Frame* frame, DocumentLoader* loader, unsigned long identifier, const ResourceResponse& r)
{
    didReceiveResourceResponseButCanceledImpl(frame, loader, identifier, r);
}

void InspectorInstrumentation::didReceiveDataImpl(InstrumentingAgents* instrumentingAgents, unsigned long identifier, const char* data, int dataLength, int encodedDataLength)
{
    if (InspectorResourceAgent* resourceAgent = instrumentingAgents->inspectorResourceAgent())
        resourceAgent->didReceiveData(identifier, data, dataLength, encodedDataLength);
}

void InspectorInstrumentation::didFinishLoadingImpl(InstrumentingAgents* instrumentingAgents, unsigned long identifier, DocumentLoader* loader, double monotonicFinishTime)
{
    InspectorTimelineAgent* timelineAgent = instrumentingAgents->inspectorTimelineAgent();
    InspectorResourceAgent* resourceAgent = instrumentingAgents->inspectorResourceAgent();
    if (!timelineAgent && !resourceAgent)
        return;

    double finishTime = 0.0;
    // FIXME: Expose all of the timing details to inspector and have it calculate finishTime.
    if (monotonicFinishTime)
        finishTime = loader->timing()->convertMonotonicTimeToDocumentTime(monotonicFinishTime);

    if (timelineAgent)
        timelineAgent->didFinishLoadingResource(identifier, false, finishTime);
    if (resourceAgent)
        resourceAgent->didFinishLoading(identifier, loader, finishTime);
}

void InspectorInstrumentation::didFailLoadingImpl(InstrumentingAgents* instrumentingAgents, unsigned long identifier, DocumentLoader* loader, const ResourceError& error)
{
    if (InspectorTimelineAgent* timelineAgent = instrumentingAgents->inspectorTimelineAgent())
        timelineAgent->didFinishLoadingResource(identifier, true, 0);
    if (InspectorResourceAgent* resourceAgent = instrumentingAgents->inspectorResourceAgent())
        resourceAgent->didFailLoading(identifier, loader, error);
    if (InspectorConsoleAgent* consoleAgent = instrumentingAgents->inspectorConsoleAgent())
        consoleAgent->didFailLoading(identifier, error); // This should come AFTER resource notification, front-end relies on this.
}

void InspectorInstrumentation::resourceRetrievedByXMLHttpRequestImpl(InstrumentingAgents* instrumentingAgents, unsigned long identifier, const String& sourceString, const String& url, const String& sendURL, unsigned sendLineNumber)
{
    if (InspectorConsoleAgent* consoleAgent = instrumentingAgents->inspectorConsoleAgent())
        consoleAgent->resourceRetrievedByXMLHttpRequest(identifier, url, sendURL, sendLineNumber);
    if (InspectorResourceAgent* resourceAgent = instrumentingAgents->inspectorResourceAgent())
        resourceAgent->setInitialXHRContent(identifier, sourceString);
}

void InspectorInstrumentation::didReceiveXHRResponseImpl(InstrumentingAgents* instrumentingAgents, unsigned long identifier)
{
    if (InspectorResourceAgent* resourceAgent = instrumentingAgents->inspectorResourceAgent())
        resourceAgent->didReceiveXHRResponse(identifier);
}

void InspectorInstrumentation::willLoadXHRSynchronouslyImpl(InstrumentingAgents* instrumentingAgents)
{
    if (InspectorResourceAgent* resourceAgent = instrumentingAgents->inspectorResourceAgent())
        resourceAgent->willLoadXHRSynchronously();
}

void InspectorInstrumentation::didLoadXHRSynchronouslyImpl(InstrumentingAgents* instrumentingAgents)
{
    if (InspectorResourceAgent* resourceAgent = instrumentingAgents->inspectorResourceAgent())
        resourceAgent->didLoadXHRSynchronously();
}

void InspectorInstrumentation::scriptImportedImpl(InstrumentingAgents* instrumentingAgents, unsigned long identifier, const String& sourceString)
{
    if (InspectorResourceAgent* resourceAgent = instrumentingAgents->inspectorResourceAgent())
        resourceAgent->setInitialScriptContent(identifier, sourceString);
}

void InspectorInstrumentation::didReceiveScriptResponseImpl(InstrumentingAgents* instrumentingAgents, unsigned long identifier)
{
    if (InspectorResourceAgent* resourceAgent = instrumentingAgents->inspectorResourceAgent())
        resourceAgent->didReceiveScriptResponse(identifier);
}

void InspectorInstrumentation::domContentLoadedEventFiredImpl(InstrumentingAgents* instrumentingAgents, Frame* frame)
{
    if (frame->page()->mainFrame() != frame)
        return;

    if (InspectorAgent* inspectorAgent = instrumentingAgents->inspectorAgent())
        inspectorAgent->domContentLoadedEventFired();

    if (InspectorDOMAgent* domAgent = instrumentingAgents->inspectorDOMAgent())
        domAgent->mainFrameDOMContentLoaded();

    if (InspectorTimelineAgent* timelineAgent = instrumentingAgents->inspectorTimelineAgent())
        timelineAgent->didMarkDOMContentEvent();

    if (InspectorPageAgent* pageAgent = instrumentingAgents->inspectorPageAgent())
        pageAgent->domContentEventFired();
}

void InspectorInstrumentation::loadEventFiredImpl(InstrumentingAgents* instrumentingAgents, Frame* frame)
{
    if (InspectorDOMAgent* domAgent = instrumentingAgents->inspectorDOMAgent())
        domAgent->loadEventFired(frame->document());

    if (frame->page()->mainFrame() != frame)
        return;

    if (InspectorTimelineAgent* timelineAgent = instrumentingAgents->inspectorTimelineAgent())
        timelineAgent->didMarkLoadEvent();

    if (InspectorPageAgent* pageAgent = instrumentingAgents->inspectorPageAgent())
        pageAgent->loadEventFired();
}

void InspectorInstrumentation::frameDetachedFromParentImpl(InstrumentingAgents* instrumentingAgents, Frame* frame)
{
    if (InspectorPageAgent* pageAgent = instrumentingAgents->inspectorPageAgent())
        pageAgent->frameDetached(frame);
}

void InspectorInstrumentation::didCommitLoadImpl(InstrumentingAgents* instrumentingAgents, Page* page, DocumentLoader* loader)
{
    InspectorAgent* inspectorAgent = instrumentingAgents->inspectorAgent();
    if (!inspectorAgent || !inspectorAgent->developerExtrasEnabled())
        return;

    Frame* mainFrame = page->mainFrame();
    if (loader->frame() == mainFrame) {
        if (InspectorConsoleAgent* consoleAgent = instrumentingAgents->inspectorConsoleAgent())
            consoleAgent->reset();

        if (InspectorResourceAgent* resourceAgent = instrumentingAgents->inspectorResourceAgent())
            resourceAgent->mainFrameNavigated(loader);
#if ENABLE(JAVASCRIPT_DEBUGGER) && USE(JSC)
        if (InspectorProfilerAgent* profilerAgent = instrumentingAgents->inspectorProfilerAgent())
            profilerAgent->resetState();
#endif
        if (InspectorCSSAgent* cssAgent = instrumentingAgents->inspectorCSSAgent())
            cssAgent->reset();
#if ENABLE(SQL_DATABASE)
        if (InspectorDatabaseAgent* databaseAgent = instrumentingAgents->inspectorDatabaseAgent())
            databaseAgent->clearResources();
#endif
        if (InspectorDOMStorageAgent* domStorageAgent = instrumentingAgents->inspectorDOMStorageAgent())
            domStorageAgent->clearResources();
        if (InspectorDOMAgent* domAgent = instrumentingAgents->inspectorDOMAgent())
            domAgent->setDocument(mainFrame->document());

        inspectorAgent->didCommitLoad();
    }
    if (InspectorPageAgent* pageAgent = instrumentingAgents->inspectorPageAgent())
        pageAgent->frameNavigated(loader);
}

void InspectorInstrumentation::loaderDetachedFromFrameImpl(InstrumentingAgents* instrumentingAgents, DocumentLoader* loader)
{
    if (InspectorPageAgent* inspectorPageAgent = instrumentingAgents->inspectorPageAgent())
        inspectorPageAgent->loaderDetachedFromFrame(loader);
}

InspectorInstrumentationCookie InspectorInstrumentation::willWriteHTMLImpl(InstrumentingAgents* instrumentingAgents, unsigned int length, unsigned int startLine)
{
    int timelineAgentId = 0;
    if (InspectorTimelineAgent* timelineAgent = instrumentingAgents->inspectorTimelineAgent()) {
        timelineAgent->willWriteHTML(length, startLine);
        timelineAgentId = timelineAgent->id();
    }
    return InspectorInstrumentationCookie(instrumentingAgents, timelineAgentId);
}

void InspectorInstrumentation::didWriteHTMLImpl(const InspectorInstrumentationCookie& cookie, unsigned int endLine)
{
    if (InspectorTimelineAgent* timelineAgent = retrieveTimelineAgent(cookie))
        timelineAgent->didWriteHTML(endLine);
}

void InspectorInstrumentation::addMessageToConsoleImpl(InstrumentingAgents* instrumentingAgents, MessageSource source, MessageType type, MessageLevel level, const String& message, PassRefPtr<ScriptArguments> arguments, PassRefPtr<ScriptCallStack> callStack)
{
    if (InspectorConsoleAgent* consoleAgent = instrumentingAgents->inspectorConsoleAgent())
        consoleAgent->addMessageToConsole(source, type, level, message, arguments, callStack);
}

void InspectorInstrumentation::addMessageToConsoleImpl(InstrumentingAgents* instrumentingAgents, MessageSource source, MessageType type, MessageLevel level, const String& message, const String& scriptId, unsigned lineNumber)
{
    if (InspectorConsoleAgent* consoleAgent = instrumentingAgents->inspectorConsoleAgent())
        consoleAgent->addMessageToConsole(source, type, level, message, scriptId, lineNumber);
}

void InspectorInstrumentation::consoleCountImpl(InstrumentingAgents* instrumentingAgents, PassRefPtr<ScriptArguments> arguments, PassRefPtr<ScriptCallStack> stack)
{
    if (InspectorConsoleAgent* consoleAgent = instrumentingAgents->inspectorConsoleAgent())
        consoleAgent->count(arguments, stack);
}

void InspectorInstrumentation::startConsoleTimingImpl(InstrumentingAgents* instrumentingAgents, const String& title)
{
    if (InspectorConsoleAgent* consoleAgent = instrumentingAgents->inspectorConsoleAgent())
        consoleAgent->startTiming(title);
}

void InspectorInstrumentation::stopConsoleTimingImpl(InstrumentingAgents* instrumentingAgents, const String& title, PassRefPtr<ScriptCallStack> stack)
{
    if (InspectorConsoleAgent* consoleAgent = instrumentingAgents->inspectorConsoleAgent())
        consoleAgent->stopTiming(title, stack);
}

void InspectorInstrumentation::consoleTimeStampImpl(InstrumentingAgents* instrumentingAgents, PassRefPtr<ScriptArguments> arguments)
{
    if (InspectorTimelineAgent* timelineAgent = instrumentingAgents->inspectorTimelineAgent()) {
        String message;
        arguments->getFirstArgumentAsString(message);
        timelineAgent->didTimeStamp(message);
     }
}

#if ENABLE(JAVASCRIPT_DEBUGGER)
void InspectorInstrumentation::addStartProfilingMessageToConsoleImpl(InstrumentingAgents* instrumentingAgents, const String& title, unsigned lineNumber, const String& sourceURL)
{
    if (InspectorProfilerAgent* profilerAgent = instrumentingAgents->inspectorProfilerAgent())
        profilerAgent->addStartProfilingMessageToConsole(title, lineNumber, sourceURL);
}

void InspectorInstrumentation::addProfileImpl(InstrumentingAgents* instrumentingAgents, RefPtr<ScriptProfile> profile, PassRefPtr<ScriptCallStack> callStack)
{
    if (InspectorProfilerAgent* profilerAgent = instrumentingAgents->inspectorProfilerAgent()) {
        const ScriptCallFrame& lastCaller = callStack->at(0);
        profilerAgent->addProfile(profile, lastCaller.lineNumber(), lastCaller.sourceURL());
    }
}

String InspectorInstrumentation::getCurrentUserInitiatedProfileNameImpl(InstrumentingAgents* instrumentingAgents, bool incrementProfileNumber)
{
    if (InspectorProfilerAgent* profilerAgent = instrumentingAgents->inspectorProfilerAgent())
        return profilerAgent->getCurrentUserInitiatedProfileName(incrementProfileNumber);
    return "";
}

bool InspectorInstrumentation::profilerEnabledImpl(InstrumentingAgents* instrumentingAgents)
{
    if (InspectorProfilerAgent* profilerAgent = instrumentingAgents->inspectorProfilerAgent())
        return profilerAgent->enabled();
    return false;
}
#endif

#if ENABLE(SQL_DATABASE)
void InspectorInstrumentation::didOpenDatabaseImpl(InstrumentingAgents* instrumentingAgents, PassRefPtr<Database> database, const String& domain, const String& name, const String& version)
{
    InspectorAgent* inspectorAgent = instrumentingAgents->inspectorAgent();
    if (!inspectorAgent || !inspectorAgent->developerExtrasEnabled())
        return;
    if (InspectorDatabaseAgent* dbAgent = instrumentingAgents->inspectorDatabaseAgent())
        dbAgent->didOpenDatabase(database, domain, name, version);
}
#endif

#if ENABLE(FILE_SYSTEM)
void InspectorInstrumentation::didOpenFileSystemImpl(InstrumentingAgents* instrumentingAgents, PassRefPtr<DOMFileSystem> fileSystem)
{
    if (InspectorFileSystemAgent* fileSystemAgent = instrumentingAgents->inspectorFileSystemAgent())
        fileSystemAgent->didOpenFileSystem(fileSystem);
}
#endif // ENABLE(FILE_SYSTEM)

void InspectorInstrumentation::didUseDOMStorageImpl(InstrumentingAgents* instrumentingAgents, StorageArea* storageArea, bool isLocalStorage, Frame* frame)
{
    InspectorAgent* inspectorAgent = instrumentingAgents->inspectorAgent();
    if (!inspectorAgent || !inspectorAgent->developerExtrasEnabled())
        return;
    if (InspectorDOMStorageAgent* domStorageAgent = instrumentingAgents->inspectorDOMStorageAgent())
        domStorageAgent->didUseDOMStorage(storageArea, isLocalStorage, frame);
}

#if ENABLE(WORKERS)
bool InspectorInstrumentation::shouldPauseDedicatedWorkerOnStartImpl(InstrumentingAgents* instrumentingAgents)
{
    if (InspectorWorkerAgent* workerAgent = instrumentingAgents->inspectorWorkerAgent())
        return workerAgent->shouldPauseDedicatedWorkerOnStart();
    return false;
}

void InspectorInstrumentation::didStartWorkerContextImpl(InstrumentingAgents* instrumentingAgents, WorkerContextProxy* workerContextProxy, const KURL& url)
{
    if (InspectorWorkerAgent* workerAgent = instrumentingAgents->inspectorWorkerAgent())
        workerAgent->didStartWorkerContext(workerContextProxy, url);
}

void InspectorInstrumentation::didCreateWorkerImpl(InstrumentingAgents* instrumentingAgents, intptr_t id, const String& url, bool isSharedWorker)
{
    if (InspectorAgent* inspectorAgent = instrumentingAgents->inspectorAgent())
        inspectorAgent->didCreateWorker(id, url, isSharedWorker);
}

void InspectorInstrumentation::didDestroyWorkerImpl(InstrumentingAgents* instrumentingAgents, intptr_t id)
{
    if (InspectorAgent* inspectorAgent = instrumentingAgents->inspectorAgent())
        inspectorAgent->didDestroyWorker(id);
}

void InspectorInstrumentation::willEvaluateWorkerScript(WorkerContext* workerContext, int workerThreadStartMode)
{
    if (workerThreadStartMode != PauseWorkerContextOnStart)
        return;
    InstrumentingAgents* instrumentingAgents = instrumentationForWorkerContext(workerContext);
    if (!instrumentingAgents)
        return;
#if ENABLE(JAVASCRIPT_DEBUGGER)
    if (InspectorRuntimeAgent* runtimeAgent = instrumentingAgents->inspectorRuntimeAgent())
        runtimeAgent->pauseWorkerContext(workerContext);
#endif
}

void InspectorInstrumentation::workerContextTerminatedImpl(InstrumentingAgents* instrumentingAgents, WorkerContextProxy* proxy)
{
    if (InspectorWorkerAgent* workerAgent = instrumentingAgents->inspectorWorkerAgent())
        workerAgent->workerContextTerminated(proxy);
}
#endif

#if ENABLE(WEB_SOCKETS)
void InspectorInstrumentation::didCreateWebSocketImpl(InstrumentingAgents* instrumentingAgents, unsigned long identifier, const KURL& requestURL, const KURL&)
{
    InspectorAgent* inspectorAgent = instrumentingAgents->inspectorAgent();
    if (!inspectorAgent || !inspectorAgent->developerExtrasEnabled())
        return;
    if (InspectorResourceAgent* resourceAgent = instrumentingAgents->inspectorResourceAgent())
        resourceAgent->didCreateWebSocket(identifier, requestURL);
}

void InspectorInstrumentation::willSendWebSocketHandshakeRequestImpl(InstrumentingAgents* instrumentingAgents, unsigned long identifier, const WebSocketHandshakeRequest& request)
{
    if (InspectorResourceAgent* resourceAgent = instrumentingAgents->inspectorResourceAgent())
        resourceAgent->willSendWebSocketHandshakeRequest(identifier, request);
}

void InspectorInstrumentation::didReceiveWebSocketHandshakeResponseImpl(InstrumentingAgents* instrumentingAgents, unsigned long identifier, const WebSocketHandshakeResponse& response)
{
    if (InspectorResourceAgent* resourceAgent = instrumentingAgents->inspectorResourceAgent())
        resourceAgent->didReceiveWebSocketHandshakeResponse(identifier, response);
}

void InspectorInstrumentation::didCloseWebSocketImpl(InstrumentingAgents* instrumentingAgents, unsigned long identifier)
{
    if (InspectorResourceAgent* resourceAgent = instrumentingAgents->inspectorResourceAgent())
        resourceAgent->didCloseWebSocket(identifier);
}
#endif

void InspectorInstrumentation::networkStateChangedImpl(InstrumentingAgents* instrumentingAgents)
{
    if (InspectorApplicationCacheAgent* applicationCacheAgent = instrumentingAgents->inspectorApplicationCacheAgent())
        applicationCacheAgent->networkStateChanged();
}

void InspectorInstrumentation::updateApplicationCacheStatusImpl(InstrumentingAgents* instrumentingAgents, Frame* frame)
{
    if (InspectorApplicationCacheAgent* applicationCacheAgent = instrumentingAgents->inspectorApplicationCacheAgent())
        applicationCacheAgent->updateApplicationCacheStatus(frame);
}

bool InspectorInstrumentation::collectingHTMLParseErrors(InstrumentingAgents* instrumentingAgents)
{
    if (InspectorAgent* inspectorAgent = instrumentingAgents->inspectorAgent())
        return inspectorAgent->hasFrontend();
    return false;
}

bool InspectorInstrumentation::hasFrontendForScriptContext(ScriptExecutionContext* scriptExecutionContext)
{
    if (!scriptExecutionContext)
        return false;

#if ENABLE(WORKERS)
    if (scriptExecutionContext->isWorkerContext()) {
        WorkerContext* workerContext = static_cast<WorkerContext*>(scriptExecutionContext);
        WorkerInspectorController* workerInspectorController = workerContext->workerInspectorController();
        return workerInspectorController && workerInspectorController->hasFrontend();
    }
#endif

    ASSERT(scriptExecutionContext->isDocument());
    Document* document = static_cast<Document*>(scriptExecutionContext);
    Page* page = document->page();
    return page && page->inspectorController()->hasFrontend();
}

void InspectorInstrumentation::pauseOnNativeEventIfNeeded(InstrumentingAgents* instrumentingAgents, const String& categoryType, const String& eventName, bool synchronous)
{
#if ENABLE(JAVASCRIPT_DEBUGGER)
    if (InspectorDOMDebuggerAgent* domDebuggerAgent = instrumentingAgents->inspectorDOMDebuggerAgent())
        domDebuggerAgent->pauseOnNativeEventIfNeeded(categoryType, eventName, synchronous);
#endif
}

void InspectorInstrumentation::cancelPauseOnNativeEvent(InstrumentingAgents* instrumentingAgents)
{
#if ENABLE(JAVASCRIPT_DEBUGGER)
    if (InspectorDebuggerAgent* debuggerAgent = instrumentingAgents->inspectorDebuggerAgent())
        debuggerAgent->cancelPauseOnNextStatement();
#endif
}

void InspectorInstrumentation::didRegisterAnimationFrameCallbackImpl(InstrumentingAgents* instrumentingAgents, int callbackId)
{
    if (InspectorTimelineAgent* timelineAgent = instrumentingAgents->inspectorTimelineAgent())
        timelineAgent->didRegisterAnimationFrameCallback(callbackId);
}

void InspectorInstrumentation::didCancelAnimationFrameCallbackImpl(InstrumentingAgents* instrumentingAgents, int callbackId)
{
    if (InspectorTimelineAgent* timelineAgent = instrumentingAgents->inspectorTimelineAgent())
        timelineAgent->didCancelAnimationFrameCallback(callbackId);
}

InspectorInstrumentationCookie InspectorInstrumentation::willFireAnimationFrameEventImpl(InstrumentingAgents* instrumentingAgents, int callbackId)
{
    int timelineAgentId = 0;
    if (InspectorTimelineAgent* timelineAgent = instrumentingAgents->inspectorTimelineAgent()) {
        timelineAgent->willFireAnimationFrameEvent(callbackId);
        timelineAgentId = timelineAgent->id();
    }
    return InspectorInstrumentationCookie(instrumentingAgents, timelineAgentId);
}

void InspectorInstrumentation::didFireAnimationFrameEventImpl(const InspectorInstrumentationCookie& cookie)
{
    if (InspectorTimelineAgent* timelineAgent = retrieveTimelineAgent(cookie))
        timelineAgent->didFireAnimationFrameEvent();
}

InspectorTimelineAgent* InspectorInstrumentation::retrieveTimelineAgent(const InspectorInstrumentationCookie& cookie)
{
    if (!cookie.first)
        return 0;
    InspectorTimelineAgent* timelineAgent = cookie.first->inspectorTimelineAgent();
    if (timelineAgent && timelineAgent->id() == cookie.second)
        return timelineAgent;
    return 0;
}

InstrumentingAgents* InspectorInstrumentation::instrumentingAgentsForPage(Page* page)
{
    if (!page)
        return 0;
    return instrumentationForPage(page);
}

#if ENABLE(WORKERS)
InstrumentingAgents* InspectorInstrumentation::instrumentingAgentsForWorkerContext(WorkerContext* workerContext)
{
    if (!workerContext)
        return 0;
    return instrumentationForWorkerContext(workerContext);
}
#endif

} // namespace WebCore

#endif // !ENABLE(INSPECTOR)
