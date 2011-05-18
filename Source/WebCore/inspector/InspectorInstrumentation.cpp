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
#include "InspectorDatabaseAgent.h"
#include "InspectorDOMAgent.h"
#include "InspectorDOMStorageAgent.h"
#include "InspectorDebuggerAgent.h"
#include "InspectorPageAgent.h"
#include "InspectorProfilerAgent.h"
#include "InspectorResourceAgent.h"
#include "InspectorTimelineAgent.h"
#include "InspectorWorkerAgent.h"
#include "InstrumentingAgents.h"
#include "ScriptArguments.h"
#include "ScriptCallStack.h"
#include "ScriptProfile.h"
#include "XMLHttpRequest.h"
#include <wtf/StdLibExtras.h>
#include <wtf/text/CString.h>

namespace WebCore {

static const char* const listenerEventCategoryType = "listener";
static const char* const instrumentationEventCategoryType = "instrumentation";

static const char* const setTimerEventName = "setTimer";
static const char* const clearTimerEventName = "clearTimer";
static const char* const timerFiredEventName = "timerFired";

InspectorInstrumentation::InstrumentingAgentsMap& InspectorInstrumentation::instrumentingAgents()
{
    DEFINE_STATIC_LOCAL(InstrumentingAgentsMap, agents, ());
    return agents;
}

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
    if (InspectorPageAgent* pageAgent = instrumentingAgents->inspectorPageAgent())
        pageAgent->didClearWindowObjectInWorld(frame, world);
    if (InspectorAgent* inspectorAgent = instrumentingAgents->inspectorAgent())
        inspectorAgent->didClearWindowObjectInWorld(frame, world);
}

void InspectorInstrumentation::inspectedPageDestroyedImpl(InstrumentingAgents* instrumentingAgents)
{
    if (InspectorAgent* inspectorAgent = instrumentingAgents->inspectorAgent())
        inspectorAgent->inspectedPageDestroyed();
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

void InspectorInstrumentation::didModifyDOMAttrImpl(InstrumentingAgents* instrumentingAgents, Element* element)
{
    if (InspectorDOMAgent* domAgent = instrumentingAgents->inspectorDOMAgent())
        domAgent->didModifyDOMAttr(element);
}

void InspectorInstrumentation::didInvalidateStyleAttrImpl(InstrumentingAgents* instrumentingAgents, Node* node)
{
    if (InspectorDOMAgent* domAgent = instrumentingAgents->inspectorDOMAgent())
        domAgent->didInvalidateStyleAttr(node);
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

InspectorInstrumentationCookie InspectorInstrumentation::willCallFunctionImpl(InstrumentingAgents* instrumentingAgents, const String& scriptName, int scriptLine)
{
    int timelineAgentId = 0;
    if (InspectorTimelineAgent* timelineAgent = instrumentingAgents->inspectorTimelineAgent()) {
        timelineAgent->willCallFunction(scriptName, scriptLine);
        timelineAgentId = timelineAgent->id();
    }
    return InspectorInstrumentationCookie(instrumentingAgents, timelineAgentId);
}

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

InspectorInstrumentationCookie InspectorInstrumentation::willPaintImpl(InstrumentingAgents* instrumentingAgents, const IntRect& rect)
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
    return InspectorInstrumentationCookie(instrumentingAgents, timelineAgentId);
}

void InspectorInstrumentation::didRecalculateStyleImpl(const InspectorInstrumentationCookie& cookie)
{
    if (InspectorTimelineAgent* timelineAgent = retrieveTimelineAgent(cookie))
        timelineAgent->didRecalculateStyle();
}

void InspectorInstrumentation::applyUserAgentOverrideImpl(InstrumentingAgents* instrumentingAgents, String* userAgent)
{
    if (InspectorResourceAgent* resourceAgent = instrumentingAgents->inspectorResourceAgent())
        resourceAgent->applyUserAgentOverride(userAgent);
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

void InspectorInstrumentation::didLoadResourceFromMemoryCacheImpl(InstrumentingAgents* instrumentingAgents, DocumentLoader* loader, const CachedResource* cachedResource)
{
    InspectorAgent* inspectorAgent = instrumentingAgents->inspectorAgent();
    if (!inspectorAgent || !inspectorAgent->enabled())
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

void InspectorInstrumentation::didReceiveContentLengthImpl(InstrumentingAgents* instrumentingAgents, unsigned long identifier, int dataLength, int encodedDataLength)
{
    if (InspectorResourceAgent* resourceAgent = instrumentingAgents->inspectorResourceAgent())
        resourceAgent->didReceiveContentLength(identifier, dataLength, encodedDataLength);
}

void InspectorInstrumentation::didFinishLoadingImpl(InstrumentingAgents* instrumentingAgents, unsigned long identifier, double finishTime)
{
    if (InspectorTimelineAgent* timelineAgent = instrumentingAgents->inspectorTimelineAgent())
        timelineAgent->didFinishLoadingResource(identifier, false, finishTime);
    if (InspectorResourceAgent* resourceAgent = instrumentingAgents->inspectorResourceAgent())
        resourceAgent->didFinishLoading(identifier, finishTime);
}

void InspectorInstrumentation::didFailLoadingImpl(InstrumentingAgents* instrumentingAgents, unsigned long identifier, const ResourceError& error)
{
    if (InspectorTimelineAgent* timelineAgent = instrumentingAgents->inspectorTimelineAgent())
        timelineAgent->didFinishLoadingResource(identifier, true, 0);
    if (InspectorResourceAgent* resourceAgent = instrumentingAgents->inspectorResourceAgent())
        resourceAgent->didFailLoading(identifier, error);
    if (InspectorConsoleAgent* consoleAgent = instrumentingAgents->inspectorConsoleAgent())
        consoleAgent->didFailLoading(identifier, error); // This should come AFTER resource notification, front-end relies on this.
}

void InspectorInstrumentation::resourceRetrievedByXMLHttpRequestImpl(InstrumentingAgents* instrumentingAgents, unsigned long identifier, const String& sourceString, const String& url, const String& sendURL, unsigned sendLineNumber)
{
    if (InspectorConsoleAgent* consoleAgent = instrumentingAgents->inspectorConsoleAgent())
        consoleAgent->resourceRetrievedByXMLHttpRequest(url, sendURL, sendLineNumber);
    if (InspectorResourceAgent* resourceAgent = instrumentingAgents->inspectorResourceAgent())
        resourceAgent->setInitialXHRContent(identifier, sourceString);
}

void InspectorInstrumentation::scriptImportedImpl(InstrumentingAgents* instrumentingAgents, unsigned long identifier, const String& sourceString)
{
    if (InspectorResourceAgent* resourceAgent = instrumentingAgents->inspectorResourceAgent())
        resourceAgent->setInitialScriptContent(identifier, sourceString);
}

void InspectorInstrumentation::domContentLoadedEventFiredImpl(InstrumentingAgents* instrumentingAgents, Frame* frame, const KURL& url)
{
    DocumentLoader* documentLoader = frame->loader()->documentLoader();
    ASSERT(documentLoader);

    if (frame->page()->mainFrame() != frame || url != documentLoader->requestURL())
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

void InspectorInstrumentation::loadEventFiredImpl(InstrumentingAgents* instrumentingAgents, Frame* frame, const KURL& url)
{
    DocumentLoader* documentLoader = frame->loader()->documentLoader();
    ASSERT(documentLoader);

    if (InspectorDOMAgent* domAgent = instrumentingAgents->inspectorDOMAgent())
        domAgent->loadEventFired(documentLoader->frame()->document());

    if (frame->page()->mainFrame() != frame || url != documentLoader->requestURL())
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
    if (!inspectorAgent || !inspectorAgent->enabled())
        return;

    if (InspectorPageAgent* pageAgent = instrumentingAgents->inspectorPageAgent())
        pageAgent->frameNavigated(loader);

    Frame* mainFrame = page->mainFrame();
    if (loader->frame() != mainFrame)
        return;

    if (InspectorConsoleAgent* consoleAgent = instrumentingAgents->inspectorConsoleAgent())
        consoleAgent->reset();
#if ENABLE(JAVASCRIPT_DEBUGGER)
    if (InspectorDebuggerAgent* debuggerAgent = instrumentingAgents->inspectorDebuggerAgent()) {
        KURL url = inspectorAgent->inspectedURLWithoutFragment();
        debuggerAgent->inspectedURLChanged(url);
    }
#endif
#if ENABLE(JAVASCRIPT_DEBUGGER) && USE(JSC)
    if (InspectorProfilerAgent* profilerAgent = instrumentingAgents->inspectorProfilerAgent()) {
        profilerAgent->stopUserInitiatedProfiling(true);
        profilerAgent->resetState();
    }
#endif
    if (InspectorCSSAgent* cssAgent = instrumentingAgents->inspectorCSSAgent())
        cssAgent->reset();
#if ENABLE(DATABASE)
    if (InspectorDatabaseAgent* databaseAgent = instrumentingAgents->inspectorDatabaseAgent())
        databaseAgent->clearResources();
#endif
#if ENABLE(DOM_STORAGE)
    if (InspectorDOMStorageAgent* domStorageAgent = instrumentingAgents->inspectorDOMStorageAgent())
        domStorageAgent->clearResources();
#endif
    if (InspectorDOMAgent* domAgent = instrumentingAgents->inspectorDOMAgent())
        domAgent->setDocument(mainFrame->document());

    inspectorAgent->didCommitLoad();
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

void InspectorInstrumentation::addMessageToConsoleImpl(InstrumentingAgents* instrumentingAgents, MessageSource source, MessageType type, MessageLevel level, const String& message, unsigned lineNumber, const String& sourceID)
{
    if (InspectorConsoleAgent* consoleAgent = instrumentingAgents->inspectorConsoleAgent())
        consoleAgent->addMessageToConsole(source, type, level, message, lineNumber, sourceID);
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

void InspectorInstrumentation::consoleMarkTimelineImpl(InstrumentingAgents* instrumentingAgents, PassRefPtr<ScriptArguments> arguments)
{
    if (InspectorTimelineAgent* timelineAgent = instrumentingAgents->inspectorTimelineAgent()) {
        String message;
        arguments->getFirstArgumentAsString(message);
        timelineAgent->didMarkTimeline(message);
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

#if ENABLE(DATABASE)
void InspectorInstrumentation::didOpenDatabaseImpl(InstrumentingAgents* instrumentingAgents, PassRefPtr<Database> database, const String& domain, const String& name, const String& version)
{
    InspectorAgent* inspectorAgent = instrumentingAgents->inspectorAgent();
    if (!inspectorAgent || !inspectorAgent->enabled())
        return;
    if (InspectorDatabaseAgent* dbAgent = instrumentingAgents->inspectorDatabaseAgent())
        dbAgent->didOpenDatabase(database, domain, name, version);
}
#endif

#if ENABLE(DOM_STORAGE)
void InspectorInstrumentation::didUseDOMStorageImpl(InstrumentingAgents* instrumentingAgents, StorageArea* storageArea, bool isLocalStorage, Frame* frame)
{
    InspectorAgent* inspectorAgent = instrumentingAgents->inspectorAgent();
    if (!inspectorAgent || !inspectorAgent->enabled())
        return;
    if (InspectorDOMStorageAgent* domStorageAgent = instrumentingAgents->inspectorDOMStorageAgent())
        domStorageAgent->didUseDOMStorage(storageArea, isLocalStorage, frame);
}
#endif

#if ENABLE(WORKERS)
void InspectorInstrumentation::didStartWorkerContextImpl(InstrumentingAgents* instrumentingAgents, WorkerContextProxy* workerContextProxy)
{
    if (InspectorWorkerAgent* workerAgent = instrumentingAgents->inspectorWorkerAgent())
        workerAgent->didStartWorkerContext(workerContextProxy);
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
#endif

#if ENABLE(WEB_SOCKETS)
void InspectorInstrumentation::didCreateWebSocketImpl(InstrumentingAgents* instrumentingAgents, unsigned long identifier, const KURL& requestURL, const KURL&)
{
    InspectorAgent* inspectorAgent = instrumentingAgents->inspectorAgent();
    if (!inspectorAgent || !inspectorAgent->enabled())
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

#if ENABLE(OFFLINE_WEB_APPLICATIONS)
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
#endif

bool InspectorInstrumentation::collectingHTMLParseErrors(InstrumentingAgents* instrumentingAgents)
{
    if (InspectorAgent* inspectorAgent = instrumentingAgents->inspectorAgent())
        return inspectorAgent->hasFrontend();
    return false;
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

InspectorTimelineAgent* InspectorInstrumentation::retrieveTimelineAgent(const InspectorInstrumentationCookie& cookie)
{
    if (!cookie.first)
        return 0;
    InspectorTimelineAgent* timelineAgent = cookie.first->inspectorTimelineAgent();
    if (timelineAgent && timelineAgent->id() == cookie.second)
        return timelineAgent;
    return 0;
}

} // namespace WebCore

#endif // !ENABLE(INSPECTOR)
