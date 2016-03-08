/*
* Copyright (C) 2011 Google Inc. All rights reserved.
* Copyright (C) 2014, 2015 Apple Inc. All rights reserved.
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

#include "CSSRule.h"
#include "CSSStyleRule.h"
#include "DOMWindow.h"
#include "DOMWrapperWorld.h"
#include "Database.h"
#include "DocumentLoader.h"
#include "Event.h"
#include "EventDispatcher.h"
#include "InspectorApplicationCacheAgent.h"
#include "InspectorCSSAgent.h"
#include "InspectorController.h"
#include "InspectorDOMAgent.h"
#include "InspectorDOMDebuggerAgent.h"
#include "InspectorDOMStorageAgent.h"
#include "InspectorDatabaseAgent.h"
#include "InspectorLayerTreeAgent.h"
#include "InspectorNetworkAgent.h"
#include "InspectorPageAgent.h"
#include "InspectorTimelineAgent.h"
#include "InstrumentingAgents.h"
#include "MainFrame.h"
#include "PageDebuggerAgent.h"
#include "PageRuntimeAgent.h"
#include "RenderObject.h"
#include "RenderView.h"
#include "ScriptController.h"
#include "StyleResolver.h"
#include "StyleRule.h"
#include "WebConsoleAgent.h"
#include "XMLHttpRequest.h"
#include <inspector/ConsoleMessage.h>
#include <inspector/ScriptArguments.h>
#include <inspector/ScriptCallStack.h>
#include <inspector/agents/InspectorDebuggerAgent.h>
#include <profiler/Profile.h>
#include <runtime/ConsoleTypes.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/CString.h>

#if ENABLE(WEB_REPLAY)
#include "InspectorReplayAgent.h"
#include "ReplayController.h" // for ReplayPosition.
#endif

using namespace Inspector;

namespace WebCore {

static const char* const requestAnimationFrameEventName = "requestAnimationFrame";
static const char* const cancelAnimationFrameEventName = "cancelAnimationFrame";
static const char* const animationFrameFiredEventName = "animationFrameFired";
static const char* const setTimerEventName = "setTimer";
static const char* const clearTimerEventName = "clearTimer";
static const char* const timerFiredEventName = "timerFired";

namespace {
static HashSet<InstrumentingAgents*>* s_instrumentingAgentsSet = nullptr;
}

int InspectorInstrumentation::s_frontendCounter = 0;

static Frame* frameForScriptExecutionContext(ScriptExecutionContext* context)
{
    Frame* frame = nullptr;
    if (is<Document>(*context))
        frame = downcast<Document>(*context).frame();
    return frame;
}

void InspectorInstrumentation::didClearWindowObjectInWorldImpl(InstrumentingAgents& instrumentingAgents, Frame& frame, DOMWrapperWorld& world)
{
    InspectorPageAgent* pageAgent = instrumentingAgents.inspectorPageAgent();
    if (pageAgent)
        pageAgent->didClearWindowObjectInWorld(&frame, world);
    if (PageDebuggerAgent* debuggerAgent = instrumentingAgents.pageDebuggerAgent()) {
        if (pageAgent && &world == &mainThreadNormalWorld() && &frame == &pageAgent->mainFrame())
            debuggerAgent->didClearMainFrameWindowObject();
    }
    if (PageRuntimeAgent* pageRuntimeAgent = instrumentingAgents.pageRuntimeAgent()) {
        if (&world == &mainThreadNormalWorld())
            pageRuntimeAgent->didCreateMainWorldContext(frame);
    }
}

bool InspectorInstrumentation::isDebuggerPausedImpl(InstrumentingAgents& instrumentingAgents)
{
    if (InspectorDebuggerAgent* debuggerAgent = instrumentingAgents.inspectorDebuggerAgent())
        return debuggerAgent->isPaused();
    return false;
}

void InspectorInstrumentation::willInsertDOMNodeImpl(InstrumentingAgents& instrumentingAgents, Node& parent)
{
    if (InspectorDOMDebuggerAgent* domDebuggerAgent = instrumentingAgents.inspectorDOMDebuggerAgent())
        domDebuggerAgent->willInsertDOMNode(parent);
}

void InspectorInstrumentation::didInsertDOMNodeImpl(InstrumentingAgents& instrumentingAgents, Node& node)
{
    if (InspectorDOMAgent* domAgent = instrumentingAgents.inspectorDOMAgent())
        domAgent->didInsertDOMNode(node);
    if (InspectorDOMDebuggerAgent* domDebuggerAgent = instrumentingAgents.inspectorDOMDebuggerAgent())
        domDebuggerAgent->didInsertDOMNode(node);
}

void InspectorInstrumentation::willRemoveDOMNodeImpl(InstrumentingAgents& instrumentingAgents, Node& node)
{
    if (InspectorDOMDebuggerAgent* domDebuggerAgent = instrumentingAgents.inspectorDOMDebuggerAgent())
        domDebuggerAgent->willRemoveDOMNode(node);
}

void InspectorInstrumentation::didRemoveDOMNodeImpl(InstrumentingAgents& instrumentingAgents, Node& node)
{
    if (InspectorDOMDebuggerAgent* domDebuggerAgent = instrumentingAgents.inspectorDOMDebuggerAgent())
        domDebuggerAgent->didRemoveDOMNode(node);
    if (InspectorDOMAgent* domAgent = instrumentingAgents.inspectorDOMAgent())
        domAgent->didRemoveDOMNode(node);
}

void InspectorInstrumentation::willModifyDOMAttrImpl(InstrumentingAgents& instrumentingAgents, Element& element, const AtomicString& oldValue, const AtomicString& newValue)
{
    if (InspectorDOMDebuggerAgent* domDebuggerAgent = instrumentingAgents.inspectorDOMDebuggerAgent())
        domDebuggerAgent->willModifyDOMAttr(element);
    if (InspectorDOMAgent* domAgent = instrumentingAgents.inspectorDOMAgent())
        domAgent->willModifyDOMAttr(element, oldValue, newValue);
}

void InspectorInstrumentation::didModifyDOMAttrImpl(InstrumentingAgents& instrumentingAgents, Element& element, const AtomicString& name, const AtomicString& value)
{
    if (InspectorDOMAgent* domAgent = instrumentingAgents.inspectorDOMAgent())
        domAgent->didModifyDOMAttr(element, name, value);
}

void InspectorInstrumentation::didRemoveDOMAttrImpl(InstrumentingAgents& instrumentingAgents, Element& element, const AtomicString& name)
{
    if (InspectorDOMAgent* domAgent = instrumentingAgents.inspectorDOMAgent())
        domAgent->didRemoveDOMAttr(element, name);
}

void InspectorInstrumentation::didInvalidateStyleAttrImpl(InstrumentingAgents& instrumentingAgents, Node& node)
{
    if (InspectorDOMAgent* domAgent = instrumentingAgents.inspectorDOMAgent())
        domAgent->didInvalidateStyleAttr(node);
    if (InspectorDOMDebuggerAgent* domDebuggerAgent = instrumentingAgents.inspectorDOMDebuggerAgent())
        domDebuggerAgent->didInvalidateStyleAttr(node);
}

void InspectorInstrumentation::documentDetachedImpl(InstrumentingAgents& instrumentingAgents, Document& document)
{
    if (InspectorCSSAgent* cssAgent = instrumentingAgents.inspectorCSSAgent())
        cssAgent->documentDetached(document);
}

void InspectorInstrumentation::frameWindowDiscardedImpl(InstrumentingAgents& instrumentingAgents, DOMWindow* window)
{
    if (WebConsoleAgent* consoleAgent = instrumentingAgents.webConsoleAgent())
        consoleAgent->frameWindowDiscarded(window);
}

void InspectorInstrumentation::mediaQueryResultChangedImpl(InstrumentingAgents& instrumentingAgents)
{
    if (InspectorCSSAgent* cssAgent = instrumentingAgents.inspectorCSSAgent())
        cssAgent->mediaQueryResultChanged();
}

void InspectorInstrumentation::activeStyleSheetsUpdatedImpl(InstrumentingAgents& instrumentingAgents, Document& document)
{
    if (InspectorCSSAgent* cssAgent = instrumentingAgents.inspectorCSSAgent())
        cssAgent->activeStyleSheetsUpdated(document);
}

void InspectorInstrumentation::didPushShadowRootImpl(InstrumentingAgents& instrumentingAgents, Element& host, ShadowRoot& root)
{
    if (InspectorDOMAgent* domAgent = instrumentingAgents.inspectorDOMAgent())
        domAgent->didPushShadowRoot(host, root);
}

void InspectorInstrumentation::willPopShadowRootImpl(InstrumentingAgents& instrumentingAgents, Element& host, ShadowRoot& root)
{
    if (InspectorDOMAgent* domAgent = instrumentingAgents.inspectorDOMAgent())
        domAgent->willPopShadowRoot(host, root);
}

void InspectorInstrumentation::pseudoElementCreatedImpl(InstrumentingAgents& instrumentingAgents, PseudoElement& pseudoElement)
{
    if (InspectorDOMAgent* domAgent = instrumentingAgents.inspectorDOMAgent())
        domAgent->pseudoElementCreated(pseudoElement);
}

void InspectorInstrumentation::pseudoElementDestroyedImpl(InstrumentingAgents& instrumentingAgents, PseudoElement& pseudoElement)
{
    if (InspectorDOMAgent* domAgent = instrumentingAgents.inspectorDOMAgent())
        domAgent->pseudoElementDestroyed(pseudoElement);
    if (InspectorLayerTreeAgent* layerTreeAgent = instrumentingAgents.inspectorLayerTreeAgent())
        layerTreeAgent->pseudoElementDestroyed(pseudoElement);
}

void InspectorInstrumentation::didCreateNamedFlowImpl(InstrumentingAgents& instrumentingAgents, Document* document, WebKitNamedFlow& namedFlow)
{
    if (!document)
        return;

    if (InspectorCSSAgent* cssAgent = instrumentingAgents.inspectorCSSAgent())
        cssAgent->didCreateNamedFlow(*document, namedFlow);
}

void InspectorInstrumentation::willRemoveNamedFlowImpl(InstrumentingAgents& instrumentingAgents, Document* document, WebKitNamedFlow& namedFlow)
{
    if (!document)
        return;

    if (InspectorCSSAgent* cssAgent = instrumentingAgents.inspectorCSSAgent())
        cssAgent->willRemoveNamedFlow(*document, namedFlow);
}

void InspectorInstrumentation::didChangeRegionOversetImpl(InstrumentingAgents& instrumentingAgents, Document& document, WebKitNamedFlow& namedFlow)
{
    if (InspectorCSSAgent* cssAgent = instrumentingAgents.inspectorCSSAgent())
        cssAgent->didChangeRegionOverset(document, namedFlow);
}

void InspectorInstrumentation::didRegisterNamedFlowContentElementImpl(InstrumentingAgents& instrumentingAgents, Document& document, WebKitNamedFlow& namedFlow, Node& contentElement, Node* nextContentElement)
{
    if (InspectorCSSAgent* cssAgent = instrumentingAgents.inspectorCSSAgent())
        cssAgent->didRegisterNamedFlowContentElement(document, namedFlow, contentElement, nextContentElement);
}

void InspectorInstrumentation::didUnregisterNamedFlowContentElementImpl(InstrumentingAgents& instrumentingAgents, Document& document, WebKitNamedFlow& namedFlow, Node& contentElement)
{
    if (InspectorCSSAgent* cssAgent = instrumentingAgents.inspectorCSSAgent())
        cssAgent->didUnregisterNamedFlowContentElement(document, namedFlow, contentElement);
}

void InspectorInstrumentation::mouseDidMoveOverElementImpl(InstrumentingAgents& instrumentingAgents, const HitTestResult& result, unsigned modifierFlags)
{
    if (InspectorDOMAgent* domAgent = instrumentingAgents.inspectorDOMAgent())
        domAgent->mouseDidMoveOverElement(result, modifierFlags);
}

void InspectorInstrumentation::didScrollImpl(InstrumentingAgents& instrumentingAgents)
{
    if (InspectorPageAgent* pageAgent = instrumentingAgents.inspectorPageAgent())
        pageAgent->didScroll();
}

bool InspectorInstrumentation::handleTouchEventImpl(InstrumentingAgents& instrumentingAgents, Node& node)
{
    if (InspectorDOMAgent* domAgent = instrumentingAgents.inspectorDOMAgent())
        return domAgent->handleTouchEvent(node);
    return false;
}

bool InspectorInstrumentation::handleMousePressImpl(InstrumentingAgents& instrumentingAgents)
{
    if (InspectorDOMAgent* domAgent = instrumentingAgents.inspectorDOMAgent())
        return domAgent->handleMousePress();
    return false;
}

bool InspectorInstrumentation::forcePseudoStateImpl(InstrumentingAgents& instrumentingAgents, const Element& element, CSSSelector::PseudoClassType pseudoState)
{
    if (InspectorCSSAgent* cssAgent = instrumentingAgents.inspectorCSSAgent())
        return cssAgent->forcePseudoState(element, pseudoState);
    return false;
}

void InspectorInstrumentation::characterDataModifiedImpl(InstrumentingAgents& instrumentingAgents, CharacterData& characterData)
{
    if (InspectorDOMAgent* domAgent = instrumentingAgents.inspectorDOMAgent())
        domAgent->characterDataModified(characterData);
}

void InspectorInstrumentation::willSendXMLHttpRequestImpl(InstrumentingAgents& instrumentingAgents, const String& url)
{
    if (InspectorDOMDebuggerAgent* domDebuggerAgent = instrumentingAgents.inspectorDOMDebuggerAgent())
        domDebuggerAgent->willSendXMLHttpRequest(url);
}

void InspectorInstrumentation::didInstallTimerImpl(InstrumentingAgents& instrumentingAgents, int timerId, std::chrono::milliseconds timeout, bool singleShot, ScriptExecutionContext* context)
{
    pauseOnNativeEventIfNeeded(instrumentingAgents, false, setTimerEventName, true);
    if (InspectorTimelineAgent* timelineAgent = instrumentingAgents.inspectorTimelineAgent())
        timelineAgent->didInstallTimer(timerId, timeout, singleShot, frameForScriptExecutionContext(context));
}

void InspectorInstrumentation::didRemoveTimerImpl(InstrumentingAgents& instrumentingAgents, int timerId, ScriptExecutionContext* context)
{
    pauseOnNativeEventIfNeeded(instrumentingAgents, false, clearTimerEventName, true);
    if (InspectorTimelineAgent* timelineAgent = instrumentingAgents.inspectorTimelineAgent())
        timelineAgent->didRemoveTimer(timerId, frameForScriptExecutionContext(context));
}

InspectorInstrumentationCookie InspectorInstrumentation::willCallFunctionImpl(InstrumentingAgents& instrumentingAgents, const String& scriptName, int scriptLine, ScriptExecutionContext* context)
{
    int timelineAgentId = 0;
    if (InspectorTimelineAgent* timelineAgent = instrumentingAgents.inspectorTimelineAgent()) {
        timelineAgent->willCallFunction(scriptName, scriptLine, frameForScriptExecutionContext(context));
        timelineAgentId = timelineAgent->id();
    }
    return InspectorInstrumentationCookie(instrumentingAgents, timelineAgentId);
}

void InspectorInstrumentation::didCallFunctionImpl(const InspectorInstrumentationCookie& cookie, ScriptExecutionContext* context)
{
    if (InspectorTimelineAgent* timelineAgent = retrieveTimelineAgent(cookie))
        timelineAgent->didCallFunction(frameForScriptExecutionContext(context));
}

InspectorInstrumentationCookie InspectorInstrumentation::willDispatchEventImpl(InstrumentingAgents& instrumentingAgents, Document& document, const Event& event, bool hasEventListeners)
{
    int timelineAgentId = 0;
    InspectorTimelineAgent* timelineAgent = instrumentingAgents.inspectorTimelineAgent();
    if (timelineAgent && hasEventListeners) {
        timelineAgent->willDispatchEvent(event, document.frame());
        timelineAgentId = timelineAgent->id();
    }
#if ENABLE(WEB_REPLAY)
    if (InspectorReplayAgent* replayAgent = instrumentingAgents.inspectorReplayAgent())
        replayAgent->willDispatchEvent(event, document.frame());
#endif
    return InspectorInstrumentationCookie(instrumentingAgents, timelineAgentId);
}

InspectorInstrumentationCookie InspectorInstrumentation::willHandleEventImpl(InstrumentingAgents& instrumentingAgents, const Event& event)
{
    pauseOnNativeEventIfNeeded(instrumentingAgents, true, event.type(), false);
    return InspectorInstrumentationCookie(instrumentingAgents, 0);
}

void InspectorInstrumentation::didHandleEventImpl(const InspectorInstrumentationCookie& cookie)
{
    if (cookie.isValid())
        cancelPauseOnNativeEvent(*cookie.instrumentingAgents());
}

void InspectorInstrumentation::didDispatchEventImpl(const InspectorInstrumentationCookie& cookie)
{
    if (InspectorTimelineAgent* timelineAgent = retrieveTimelineAgent(cookie))
        timelineAgent->didDispatchEvent();
}

InspectorInstrumentationCookie InspectorInstrumentation::willDispatchEventOnWindowImpl(InstrumentingAgents& instrumentingAgents, const Event& event, DOMWindow& window)
{
    int timelineAgentId = 0;
    InspectorTimelineAgent* timelineAgent = instrumentingAgents.inspectorTimelineAgent();
    if (timelineAgent && window.hasEventListeners(event.type())) {
        timelineAgent->willDispatchEvent(event, window.frame());
        timelineAgentId = timelineAgent->id();
    }
#if ENABLE(WEB_REPLAY)
    if (InspectorReplayAgent* replayAgent = instrumentingAgents.inspectorReplayAgent())
        replayAgent->willDispatchEvent(event, window.frame());
#endif
    return InspectorInstrumentationCookie(instrumentingAgents, timelineAgentId);
}

void InspectorInstrumentation::didDispatchEventOnWindowImpl(const InspectorInstrumentationCookie& cookie)
{
    if (InspectorTimelineAgent* timelineAgent = retrieveTimelineAgent(cookie))
        timelineAgent->didDispatchEvent();
}

InspectorInstrumentationCookie InspectorInstrumentation::willEvaluateScriptImpl(InstrumentingAgents& instrumentingAgents, Frame& frame, const String& url, int lineNumber)
{
    int timelineAgentId = 0;
    if (InspectorTimelineAgent* timelineAgent = instrumentingAgents.inspectorTimelineAgent()) {
        timelineAgent->willEvaluateScript(url, lineNumber, frame);
        timelineAgentId = timelineAgent->id();
    }
    return InspectorInstrumentationCookie(instrumentingAgents, timelineAgentId);
}

void InspectorInstrumentation::didEvaluateScriptImpl(const InspectorInstrumentationCookie& cookie, Frame& frame)
{
    if (InspectorTimelineAgent* timelineAgent = retrieveTimelineAgent(cookie))
        timelineAgent->didEvaluateScript(frame);
}

void InspectorInstrumentation::scriptsEnabledImpl(InstrumentingAgents& instrumentingAgents, bool isEnabled)
{
    if (InspectorPageAgent* pageAgent = instrumentingAgents.inspectorPageAgent())
        pageAgent->scriptsEnabled(isEnabled);
}

InspectorInstrumentationCookie InspectorInstrumentation::willFireTimerImpl(InstrumentingAgents& instrumentingAgents, int timerId, ScriptExecutionContext* context)
{
    pauseOnNativeEventIfNeeded(instrumentingAgents, false, timerFiredEventName, false);

    int timelineAgentId = 0;
    if (InspectorTimelineAgent* timelineAgent = instrumentingAgents.inspectorTimelineAgent()) {
        timelineAgent->willFireTimer(timerId, frameForScriptExecutionContext(context));
        timelineAgentId = timelineAgent->id();
    }
    return InspectorInstrumentationCookie(instrumentingAgents, timelineAgentId);
}

void InspectorInstrumentation::didFireTimerImpl(const InspectorInstrumentationCookie& cookie)
{
    if (cookie.isValid())
        cancelPauseOnNativeEvent(*cookie.instrumentingAgents());

    if (InspectorTimelineAgent* timelineAgent = retrieveTimelineAgent(cookie))
        timelineAgent->didFireTimer();
}

void InspectorInstrumentation::didInvalidateLayoutImpl(InstrumentingAgents& instrumentingAgents, Frame& frame)
{
    if (InspectorTimelineAgent* timelineAgent = instrumentingAgents.inspectorTimelineAgent())
        timelineAgent->didInvalidateLayout(frame);
}

InspectorInstrumentationCookie InspectorInstrumentation::willLayoutImpl(InstrumentingAgents& instrumentingAgents, Frame& frame)
{
    int timelineAgentId = 0;
    if (InspectorTimelineAgent* timelineAgent = instrumentingAgents.inspectorTimelineAgent()) {
        timelineAgent->willLayout(frame);
        timelineAgentId = timelineAgent->id();
    }
    return InspectorInstrumentationCookie(instrumentingAgents, timelineAgentId);
}

void InspectorInstrumentation::didLayoutImpl(const InspectorInstrumentationCookie& cookie, RenderObject* root)
{
    if (InspectorTimelineAgent* timelineAgent = retrieveTimelineAgent(cookie))
        timelineAgent->didLayout(root);

    if (InspectorPageAgent* pageAgent = cookie.instrumentingAgents()->inspectorPageAgent())
        pageAgent->didLayout();
}

void InspectorInstrumentation::willCompositeImpl(InstrumentingAgents& instrumentingAgents, Frame& frame)
{
    if (InspectorTimelineAgent* timelineAgent = instrumentingAgents.inspectorTimelineAgent())
        timelineAgent->willComposite(frame);
}

void InspectorInstrumentation::didCompositeImpl(InstrumentingAgents& instrumentingAgents)
{
    if (InspectorTimelineAgent* timelineAgent = instrumentingAgents.inspectorTimelineAgent())
        timelineAgent->didComposite();
}

void InspectorInstrumentation::willPaintImpl(InstrumentingAgents& instrumentingAgents, RenderObject* renderer)
{
    if (InspectorTimelineAgent* timelineAgent = instrumentingAgents.inspectorTimelineAgent())
        timelineAgent->willPaint(renderer->frame());
}

void InspectorInstrumentation::didPaintImpl(InstrumentingAgents& instrumentingAgents, RenderObject* renderer, const LayoutRect& rect)
{
    if (InspectorTimelineAgent* timelineAgent = instrumentingAgents.inspectorTimelineAgent())
        timelineAgent->didPaint(renderer, rect);

    if (InspectorPageAgent* pageAgent = instrumentingAgents.inspectorPageAgent())
        pageAgent->didPaint(renderer, rect);
}

InspectorInstrumentationCookie InspectorInstrumentation::willRecalculateStyleImpl(InstrumentingAgents& instrumentingAgents, Document& document)
{
    int timelineAgentId = 0;
    if (InspectorTimelineAgent* timelineAgent = instrumentingAgents.inspectorTimelineAgent()) {
        timelineAgent->willRecalculateStyle(document.frame());
        timelineAgentId = timelineAgent->id();
    }
    if (InspectorNetworkAgent* networkAgent = instrumentingAgents.inspectorNetworkAgent())
        networkAgent->willRecalculateStyle();
    return InspectorInstrumentationCookie(instrumentingAgents, timelineAgentId);
}

void InspectorInstrumentation::didRecalculateStyleImpl(const InspectorInstrumentationCookie& cookie)
{
    if (!cookie.isValid())
        return;
    
    InstrumentingAgents& instrumentingAgents = *cookie.instrumentingAgents();

    if (InspectorTimelineAgent* timelineAgent = retrieveTimelineAgent(cookie))
        timelineAgent->didRecalculateStyle();
    if (InspectorNetworkAgent* networkAgent = instrumentingAgents.inspectorNetworkAgent())
        networkAgent->didRecalculateStyle();
    if (InspectorPageAgent* pageAgent = instrumentingAgents.inspectorPageAgent())
        pageAgent->didRecalculateStyle();
}

void InspectorInstrumentation::didScheduleStyleRecalculationImpl(InstrumentingAgents& instrumentingAgents, Document& document)
{
    if (InspectorTimelineAgent* timelineAgent = instrumentingAgents.inspectorTimelineAgent())
        timelineAgent->didScheduleStyleRecalculation(document.frame());
    if (InspectorNetworkAgent* networkAgent = instrumentingAgents.inspectorNetworkAgent())
        networkAgent->didScheduleStyleRecalculation(document);
}

void InspectorInstrumentation::applyEmulatedMediaImpl(InstrumentingAgents& instrumentingAgents, String& media)
{
    if (InspectorPageAgent* pageAgent = instrumentingAgents.inspectorPageAgent())
        pageAgent->applyEmulatedMedia(media);
}

void InspectorInstrumentation::willSendRequestImpl(InstrumentingAgents& instrumentingAgents, unsigned long identifier, DocumentLoader* loader, ResourceRequest& request, const ResourceResponse& redirectResponse)
{
    if (!loader)
        return;

    if (InspectorNetworkAgent* networkAgent = instrumentingAgents.inspectorNetworkAgent())
        networkAgent->willSendRequest(identifier, *loader, request, redirectResponse);
}

void InspectorInstrumentation::continueAfterPingLoaderImpl(InstrumentingAgents& instrumentingAgents, unsigned long identifier, DocumentLoader* loader, ResourceRequest& request, const ResourceResponse& response)
{
    willSendRequestImpl(instrumentingAgents, identifier, loader, request, response);
}

void InspectorInstrumentation::markResourceAsCachedImpl(InstrumentingAgents& instrumentingAgents, unsigned long identifier)
{
    if (InspectorNetworkAgent* networkAgent = instrumentingAgents.inspectorNetworkAgent())
        networkAgent->markResourceAsCached(identifier);
}

void InspectorInstrumentation::didLoadResourceFromMemoryCacheImpl(InstrumentingAgents& instrumentingAgents, DocumentLoader* loader, CachedResource* cachedResource)
{
    if (!instrumentingAgents.inspectorEnvironment().developerExtrasEnabled())
        return;
    
    if (!loader || !cachedResource)
        return;

    if (InspectorNetworkAgent* networkAgent = instrumentingAgents.inspectorNetworkAgent())
        networkAgent->didLoadResourceFromMemoryCache(*loader, *cachedResource);
}

InspectorInstrumentationCookie InspectorInstrumentation::willReceiveResourceResponseImpl(InstrumentingAgents& instrumentingAgents)
{
    return InspectorInstrumentationCookie(instrumentingAgents, 0);
}

void InspectorInstrumentation::didReceiveResourceResponseImpl(const InspectorInstrumentationCookie& cookie, unsigned long identifier, DocumentLoader* loader, const ResourceResponse& response, ResourceLoader* resourceLoader)
{
    if (!cookie.isValid())
        return;

    if (!loader)
        return;

    InstrumentingAgents& instrumentingAgents = *cookie.instrumentingAgents();

    if (InspectorNetworkAgent* networkAgent = instrumentingAgents.inspectorNetworkAgent())
        networkAgent->didReceiveResponse(identifier, *loader, response, resourceLoader);
    if (WebConsoleAgent* consoleAgent = instrumentingAgents.webConsoleAgent())
        consoleAgent->didReceiveResponse(identifier, response); // This should come AFTER resource notification, front-end relies on this.
}

void InspectorInstrumentation::didReceiveResourceResponseButCanceledImpl(Frame* frame, DocumentLoader& loader, unsigned long identifier, const ResourceResponse& r)
{
    if (!frame)
        return;

    InspectorInstrumentationCookie cookie = InspectorInstrumentation::willReceiveResourceResponse(frame);
    InspectorInstrumentation::didReceiveResourceResponse(cookie, identifier, &loader, r, nullptr);
}

void InspectorInstrumentation::continueAfterXFrameOptionsDeniedImpl(Frame* frame, DocumentLoader& loader, unsigned long identifier, const ResourceResponse& r)
{
    didReceiveResourceResponseButCanceledImpl(frame, loader, identifier, r);
}

void InspectorInstrumentation::continueWithPolicyDownloadImpl(Frame* frame, DocumentLoader& loader, unsigned long identifier, const ResourceResponse& r)
{
    didReceiveResourceResponseButCanceledImpl(frame, loader, identifier, r);
}

void InspectorInstrumentation::continueWithPolicyIgnoreImpl(Frame* frame, DocumentLoader& loader, unsigned long identifier, const ResourceResponse& r)
{
    didReceiveResourceResponseButCanceledImpl(frame, loader, identifier, r);
}

void InspectorInstrumentation::didReceiveDataImpl(InstrumentingAgents& instrumentingAgents, unsigned long identifier, const char* data, int dataLength, int encodedDataLength)
{
    if (InspectorNetworkAgent* networkAgent = instrumentingAgents.inspectorNetworkAgent())
        networkAgent->didReceiveData(identifier, data, dataLength, encodedDataLength);
}

void InspectorInstrumentation::didFinishLoadingImpl(InstrumentingAgents& instrumentingAgents, unsigned long identifier, DocumentLoader* loader, double finishTime)
{
    if (!loader)
        return;

    if (InspectorNetworkAgent* networkAgent = instrumentingAgents.inspectorNetworkAgent())
        networkAgent->didFinishLoading(identifier, *loader, finishTime);
}

void InspectorInstrumentation::didFailLoadingImpl(InstrumentingAgents& instrumentingAgents, unsigned long identifier, DocumentLoader* loader, const ResourceError& error)
{
    if (!loader)
        return;

    if (InspectorNetworkAgent* networkAgent = instrumentingAgents.inspectorNetworkAgent())
        networkAgent->didFailLoading(identifier, *loader, error);
    if (WebConsoleAgent* consoleAgent = instrumentingAgents.webConsoleAgent())
        consoleAgent->didFailLoading(identifier, error); // This should come AFTER resource notification, front-end relies on this.
}

void InspectorInstrumentation::didFinishXHRLoadingImpl(InstrumentingAgents& instrumentingAgents, ThreadableLoaderClient* client, unsigned long identifier, const String& sourceString, const String& url, const String& sendURL, unsigned sendLineNumber, unsigned sendColumnNumber)
{
    if (WebConsoleAgent* consoleAgent = instrumentingAgents.webConsoleAgent())
        consoleAgent->didFinishXHRLoading(identifier, url, sendURL, sendLineNumber, sendColumnNumber);
    if (InspectorNetworkAgent* networkAgent = instrumentingAgents.inspectorNetworkAgent())
        networkAgent->didFinishXHRLoading(client, identifier, sourceString);
}

void InspectorInstrumentation::didReceiveXHRResponseImpl(InstrumentingAgents& instrumentingAgents, unsigned long identifier)
{
    if (InspectorNetworkAgent* networkAgent = instrumentingAgents.inspectorNetworkAgent())
        networkAgent->didReceiveXHRResponse(identifier);
}

void InspectorInstrumentation::willLoadXHRSynchronouslyImpl(InstrumentingAgents& instrumentingAgents)
{
    if (InspectorNetworkAgent* networkAgent = instrumentingAgents.inspectorNetworkAgent())
        networkAgent->willLoadXHRSynchronously();
}

void InspectorInstrumentation::didLoadXHRSynchronouslyImpl(InstrumentingAgents& instrumentingAgents)
{
    if (InspectorNetworkAgent* networkAgent = instrumentingAgents.inspectorNetworkAgent())
        networkAgent->didLoadXHRSynchronously();
}

void InspectorInstrumentation::scriptImportedImpl(InstrumentingAgents& instrumentingAgents, unsigned long identifier, const String& sourceString)
{
    if (InspectorNetworkAgent* networkAgent = instrumentingAgents.inspectorNetworkAgent())
        networkAgent->setInitialScriptContent(identifier, sourceString);
}

void InspectorInstrumentation::scriptExecutionBlockedByCSPImpl(InstrumentingAgents& instrumentingAgents, const String& directiveText)
{
    if (InspectorDebuggerAgent* debuggerAgent = instrumentingAgents.inspectorDebuggerAgent())
        debuggerAgent->scriptExecutionBlockedByCSP(directiveText);
}

void InspectorInstrumentation::didReceiveScriptResponseImpl(InstrumentingAgents& instrumentingAgents, unsigned long identifier)
{
    if (InspectorNetworkAgent* networkAgent = instrumentingAgents.inspectorNetworkAgent())
        networkAgent->didReceiveScriptResponse(identifier);
}

void InspectorInstrumentation::domContentLoadedEventFiredImpl(InstrumentingAgents& instrumentingAgents, Frame& frame)
{
    if (!frame.isMainFrame())
        return;

    if (InspectorDOMAgent* domAgent = instrumentingAgents.inspectorDOMAgent())
        domAgent->mainFrameDOMContentLoaded();

    if (InspectorPageAgent* pageAgent = instrumentingAgents.inspectorPageAgent())
        pageAgent->domContentEventFired();
}

void InspectorInstrumentation::loadEventFiredImpl(InstrumentingAgents& instrumentingAgents, Frame* frame)
{
    if (!frame || !frame->isMainFrame())
        return;

    if (InspectorPageAgent* pageAgent = instrumentingAgents.inspectorPageAgent())
        pageAgent->loadEventFired();
}

void InspectorInstrumentation::frameDetachedFromParentImpl(InstrumentingAgents& instrumentingAgents, Frame& frame)
{
    if (InspectorPageAgent* pageAgent = instrumentingAgents.inspectorPageAgent())
        pageAgent->frameDetached(frame);

#if ENABLE(WEB_REPLAY)
    if (!frame.isMainFrame())
        return;

    if (InspectorReplayAgent* replayAgent = instrumentingAgents.inspectorReplayAgent())
        replayAgent->frameDetached(frame);
#endif
}

void InspectorInstrumentation::didCommitLoadImpl(InstrumentingAgents& instrumentingAgents, Page* page, DocumentLoader* loader)
{
    if (!instrumentingAgents.inspectorEnvironment().developerExtrasEnabled())
        return;

    if (!page || !loader || !loader->frame())
        return;

    if (loader->frame()->isMainFrame()) {
        if (WebConsoleAgent* consoleAgent = instrumentingAgents.webConsoleAgent())
            consoleAgent->reset();

        if (InspectorNetworkAgent* networkAgent = instrumentingAgents.inspectorNetworkAgent())
            networkAgent->mainFrameNavigated(*loader);

        if (InspectorCSSAgent* cssAgent = instrumentingAgents.inspectorCSSAgent())
            cssAgent->reset();

        if (InspectorDatabaseAgent* databaseAgent = instrumentingAgents.inspectorDatabaseAgent())
            databaseAgent->clearResources();

        if (InspectorDOMAgent* domAgent = instrumentingAgents.inspectorDOMAgent())
            domAgent->setDocument(page->mainFrame().document());

        if (InspectorLayerTreeAgent* layerTreeAgent = instrumentingAgents.inspectorLayerTreeAgent())
            layerTreeAgent->reset();

        if (PageDebuggerAgent* pageDebuggerAgent = instrumentingAgents.pageDebuggerAgent())
            pageDebuggerAgent->mainFrameNavigated();
    }

    if (InspectorDOMAgent* domAgent = instrumentingAgents.inspectorDOMAgent())
        domAgent->didCommitLoad(loader->frame()->document());

    if (InspectorPageAgent* pageAgent = instrumentingAgents.inspectorPageAgent())
        pageAgent->frameNavigated(loader);

#if ENABLE(WEB_REPLAY)
    if (InspectorReplayAgent* replayAgent = instrumentingAgents.inspectorReplayAgent())
        replayAgent->frameNavigated(loader);
#endif
}

void InspectorInstrumentation::frameDocumentUpdatedImpl(InstrumentingAgents& instrumentingAgents, Frame* frame)
{
    if (!instrumentingAgents.inspectorEnvironment().developerExtrasEnabled())
        return;
    if (InspectorDOMAgent* domAgent = instrumentingAgents.inspectorDOMAgent())
        domAgent->frameDocumentUpdated(frame);
}

void InspectorInstrumentation::loaderDetachedFromFrameImpl(InstrumentingAgents& instrumentingAgents, DocumentLoader& loader)
{
    if (InspectorPageAgent* inspectorPageAgent = instrumentingAgents.inspectorPageAgent())
        inspectorPageAgent->loaderDetachedFromFrame(loader);
}

void InspectorInstrumentation::frameStartedLoadingImpl(InstrumentingAgents& instrumentingAgents, Frame& frame)
{
    if (frame.isMainFrame()) {
        if (PageDebuggerAgent* pageDebuggerAgent = instrumentingAgents.pageDebuggerAgent())
            pageDebuggerAgent->mainFrameStartedLoading();
    }

    if (InspectorPageAgent* inspectorPageAgent = instrumentingAgents.inspectorPageAgent())
        inspectorPageAgent->frameStartedLoading(frame);
}

void InspectorInstrumentation::frameStoppedLoadingImpl(InstrumentingAgents& instrumentingAgents, Frame& frame)
{
    if (frame.isMainFrame()) {
        if (PageDebuggerAgent* pageDebuggerAgent = instrumentingAgents.pageDebuggerAgent())
            pageDebuggerAgent->mainFrameStoppedLoading();
    }

    if (InspectorPageAgent* inspectorPageAgent = instrumentingAgents.inspectorPageAgent())
        inspectorPageAgent->frameStoppedLoading(frame);
}

void InspectorInstrumentation::frameScheduledNavigationImpl(InstrumentingAgents& instrumentingAgents, Frame& frame, double delay)
{
    if (InspectorPageAgent* inspectorPageAgent = instrumentingAgents.inspectorPageAgent())
        inspectorPageAgent->frameScheduledNavigation(frame, delay);
}

void InspectorInstrumentation::frameClearedScheduledNavigationImpl(InstrumentingAgents& instrumentingAgents, Frame& frame)
{
    if (InspectorPageAgent* inspectorPageAgent = instrumentingAgents.inspectorPageAgent())
        inspectorPageAgent->frameClearedScheduledNavigation(frame);
}

InspectorInstrumentationCookie InspectorInstrumentation::willRunJavaScriptDialogImpl(InstrumentingAgents& instrumentingAgents, const String& message)
{
    if (InspectorPageAgent* inspectorPageAgent = instrumentingAgents.inspectorPageAgent())
        inspectorPageAgent->willRunJavaScriptDialog(message);
    return InspectorInstrumentationCookie(instrumentingAgents, 0);
}

void InspectorInstrumentation::didRunJavaScriptDialogImpl(const InspectorInstrumentationCookie& cookie)
{
    if (InspectorPageAgent* inspectorPageAgent = cookie.instrumentingAgents()->inspectorPageAgent())
        inspectorPageAgent->didRunJavaScriptDialog();
}

void InspectorInstrumentation::willDestroyCachedResourceImpl(CachedResource& cachedResource)
{
    if (!s_instrumentingAgentsSet)
        return;

    for (auto* instrumentingAgent : *s_instrumentingAgentsSet) {
        if (InspectorNetworkAgent* inspectorNetworkAgent = instrumentingAgent->inspectorNetworkAgent())
            inspectorNetworkAgent->willDestroyCachedResource(cachedResource);
    }
}

// JavaScriptCore InspectorDebuggerAgent should know Console MessageTypes.
static bool isConsoleAssertMessage(MessageSource source, MessageType type)
{
    return source == MessageSource::ConsoleAPI && type == MessageType::Assert;
}

void InspectorInstrumentation::addMessageToConsoleImpl(InstrumentingAgents& instrumentingAgents, std::unique_ptr<ConsoleMessage> message)
{
    MessageSource source = message->source();
    MessageType type = message->type();
    String messageText = message->message();

    if (WebConsoleAgent* consoleAgent = instrumentingAgents.webConsoleAgent())
        consoleAgent->addMessageToConsole(WTFMove(message));
    // FIXME: This should just pass the message on to the debugger agent. JavaScriptCore InspectorDebuggerAgent should know Console MessageTypes.
    if (InspectorDebuggerAgent* debuggerAgent = instrumentingAgents.inspectorDebuggerAgent()) {
        if (isConsoleAssertMessage(source, type))
            debuggerAgent->handleConsoleAssert(messageText);
    }
}

void InspectorInstrumentation::consoleCountImpl(InstrumentingAgents& instrumentingAgents, JSC::ExecState* state, RefPtr<ScriptArguments>&& arguments)
{
    if (WebConsoleAgent* consoleAgent = instrumentingAgents.webConsoleAgent())
        consoleAgent->count(state, arguments);
}

void InspectorInstrumentation::startConsoleTimingImpl(InstrumentingAgents& instrumentingAgents, Frame& frame, const String& title)
{
    if (InspectorTimelineAgent* timelineAgent = instrumentingAgents.inspectorTimelineAgent())
        timelineAgent->time(frame, title);
    if (WebConsoleAgent* consoleAgent = instrumentingAgents.webConsoleAgent())
        consoleAgent->startTiming(title);
}

void InspectorInstrumentation::stopConsoleTimingImpl(InstrumentingAgents& instrumentingAgents, Frame& frame, const String& title, RefPtr<ScriptCallStack>&& stack)
{
    if (WebConsoleAgent* consoleAgent = instrumentingAgents.webConsoleAgent())
        consoleAgent->stopTiming(title, stack);
    if (InspectorTimelineAgent* timelineAgent = instrumentingAgents.inspectorTimelineAgent())
        timelineAgent->timeEnd(frame, title);
}

void InspectorInstrumentation::consoleTimeStampImpl(InstrumentingAgents& instrumentingAgents, Frame& frame, RefPtr<ScriptArguments>&& arguments)
{
    if (InspectorTimelineAgent* timelineAgent = instrumentingAgents.inspectorTimelineAgent()) {
        String message;
        arguments->getFirstArgumentAsString(message);
        timelineAgent->didTimeStamp(frame, message);
     }
}

void InspectorInstrumentation::startProfilingImpl(InstrumentingAgents& instrumentingAgents, JSC::ExecState* exec, const String& title)
{
    if (InspectorTimelineAgent* timelineAgent = instrumentingAgents.persistentInspectorTimelineAgent())
        timelineAgent->startFromConsole(exec, title);
}

RefPtr<JSC::Profile> InspectorInstrumentation::stopProfilingImpl(InstrumentingAgents& instrumentingAgents, JSC::ExecState* exec, const String& title)
{
    if (InspectorTimelineAgent* timelineAgent = instrumentingAgents.persistentInspectorTimelineAgent())
        return timelineAgent->stopFromConsole(exec, title);
    return nullptr;
}

void InspectorInstrumentation::didOpenDatabaseImpl(InstrumentingAgents& instrumentingAgents, RefPtr<Database>&& database, const String& domain, const String& name, const String& version)
{
    if (!instrumentingAgents.inspectorEnvironment().developerExtrasEnabled())
        return;
    if (InspectorDatabaseAgent* dbAgent = instrumentingAgents.inspectorDatabaseAgent())
        dbAgent->didOpenDatabase(WTFMove(database), domain, name, version);
}

void InspectorInstrumentation::didDispatchDOMStorageEventImpl(InstrumentingAgents& instrumentingAgents, const String& key, const String& oldValue, const String& newValue, StorageType storageType, SecurityOrigin* securityOrigin, Page* page)
{
    if (InspectorDOMStorageAgent* domStorageAgent = instrumentingAgents.inspectorDOMStorageAgent())
        domStorageAgent->didDispatchDOMStorageEvent(key, oldValue, newValue, storageType, securityOrigin, page);
}

#if ENABLE(WEB_SOCKETS)
void InspectorInstrumentation::didCreateWebSocketImpl(InstrumentingAgents& instrumentingAgents, unsigned long identifier, const URL& requestURL)
{
    if (!instrumentingAgents.inspectorEnvironment().developerExtrasEnabled())
        return;

    if (InspectorNetworkAgent* networkAgent = instrumentingAgents.inspectorNetworkAgent())
        networkAgent->didCreateWebSocket(identifier, requestURL);
}

void InspectorInstrumentation::willSendWebSocketHandshakeRequestImpl(InstrumentingAgents& instrumentingAgents, unsigned long identifier, const ResourceRequest& request)
{
    if (InspectorNetworkAgent* networkAgent = instrumentingAgents.inspectorNetworkAgent())
        networkAgent->willSendWebSocketHandshakeRequest(identifier, request);
}

void InspectorInstrumentation::didReceiveWebSocketHandshakeResponseImpl(InstrumentingAgents& instrumentingAgents, unsigned long identifier, const ResourceResponse& response)
{
    if (InspectorNetworkAgent* networkAgent = instrumentingAgents.inspectorNetworkAgent())
        networkAgent->didReceiveWebSocketHandshakeResponse(identifier, response);
}

void InspectorInstrumentation::didCloseWebSocketImpl(InstrumentingAgents& instrumentingAgents, unsigned long identifier)
{
    if (InspectorNetworkAgent* networkAgent = instrumentingAgents.inspectorNetworkAgent())
        networkAgent->didCloseWebSocket(identifier);
}

void InspectorInstrumentation::didReceiveWebSocketFrameImpl(InstrumentingAgents& instrumentingAgents, unsigned long identifier, const WebSocketFrame& frame)
{
    if (InspectorNetworkAgent* networkAgent = instrumentingAgents.inspectorNetworkAgent())
        networkAgent->didReceiveWebSocketFrame(identifier, frame);
}

void InspectorInstrumentation::didReceiveWebSocketFrameErrorImpl(InstrumentingAgents& instrumentingAgents, unsigned long identifier, const String& errorMessage)
{
    if (InspectorNetworkAgent* networkAgent = instrumentingAgents.inspectorNetworkAgent())
        networkAgent->didReceiveWebSocketFrameError(identifier, errorMessage);
}

void InspectorInstrumentation::didSendWebSocketFrameImpl(InstrumentingAgents& instrumentingAgents, unsigned long identifier, const WebSocketFrame& frame)
{
    if (InspectorNetworkAgent* networkAgent = instrumentingAgents.inspectorNetworkAgent())
        networkAgent->didSendWebSocketFrame(identifier, frame);
}
#endif

#if ENABLE(WEB_REPLAY)
void InspectorInstrumentation::sessionCreatedImpl(InstrumentingAgents& instrumentingAgents, RefPtr<ReplaySession>&& session)
{
    if (InspectorReplayAgent* replayAgent = instrumentingAgents.inspectorReplayAgent())
        replayAgent->sessionCreated(WTFMove(session));
}

void InspectorInstrumentation::sessionLoadedImpl(InstrumentingAgents& instrumentingAgents, RefPtr<ReplaySession>&& session)
{
    if (InspectorReplayAgent* replayAgent = instrumentingAgents.inspectorReplayAgent())
        replayAgent->sessionLoaded(WTFMove(session));
}

void InspectorInstrumentation::sessionModifiedImpl(InstrumentingAgents& instrumentingAgents, RefPtr<ReplaySession>&& session)
{
    if (InspectorReplayAgent* replayAgent = instrumentingAgents.inspectorReplayAgent())
        replayAgent->sessionModified(WTFMove(session));
}

void InspectorInstrumentation::segmentCreatedImpl(InstrumentingAgents& instrumentingAgents, RefPtr<ReplaySessionSegment>&& segment)
{
    if (InspectorReplayAgent* replayAgent = instrumentingAgents.inspectorReplayAgent())
        replayAgent->segmentCreated(WTFMove(segment));
}

void InspectorInstrumentation::segmentCompletedImpl(InstrumentingAgents& instrumentingAgents, RefPtr<ReplaySessionSegment>&& segment)
{
    if (InspectorReplayAgent* replayAgent = instrumentingAgents.inspectorReplayAgent())
        replayAgent->segmentCompleted(WTFMove(segment));
}

void InspectorInstrumentation::segmentLoadedImpl(InstrumentingAgents& instrumentingAgents, RefPtr<ReplaySessionSegment>&& segment)
{
    if (InspectorReplayAgent* replayAgent = instrumentingAgents.inspectorReplayAgent())
        replayAgent->segmentLoaded(WTFMove(segment));
}

void InspectorInstrumentation::segmentUnloadedImpl(InstrumentingAgents& instrumentingAgents)
{
    if (InspectorReplayAgent* replayAgent = instrumentingAgents.inspectorReplayAgent())
        replayAgent->segmentUnloaded();
}

void InspectorInstrumentation::captureStartedImpl(InstrumentingAgents& instrumentingAgents)
{
    if (InspectorReplayAgent* replayAgent = instrumentingAgents.inspectorReplayAgent())
        replayAgent->captureStarted();
}

void InspectorInstrumentation::captureStoppedImpl(InstrumentingAgents& instrumentingAgents)
{
    if (InspectorReplayAgent* replayAgent = instrumentingAgents.inspectorReplayAgent())
        replayAgent->captureStopped();
}

void InspectorInstrumentation::playbackStartedImpl(InstrumentingAgents& instrumentingAgents)
{
    if (InspectorReplayAgent* replayAgent = instrumentingAgents.inspectorReplayAgent())
        replayAgent->playbackStarted();
}

void InspectorInstrumentation::playbackPausedImpl(InstrumentingAgents& instrumentingAgents, const ReplayPosition& position)
{
    if (InspectorReplayAgent* replayAgent = instrumentingAgents.inspectorReplayAgent())
        replayAgent->playbackPaused(position);
}

void InspectorInstrumentation::playbackHitPositionImpl(InstrumentingAgents& instrumentingAgents, const ReplayPosition& position)
{
    if (InspectorReplayAgent* replayAgent = instrumentingAgents.inspectorReplayAgent())
        replayAgent->playbackHitPosition(position);
}

void InspectorInstrumentation::playbackFinishedImpl(InstrumentingAgents& instrumentingAgents)
{
    if (InspectorReplayAgent* replayAgent = instrumentingAgents.inspectorReplayAgent())
        replayAgent->playbackFinished();
}
#endif

void InspectorInstrumentation::networkStateChangedImpl(InstrumentingAgents& instrumentingAgents)
{
    if (InspectorApplicationCacheAgent* applicationCacheAgent = instrumentingAgents.inspectorApplicationCacheAgent())
        applicationCacheAgent->networkStateChanged();
}

void InspectorInstrumentation::updateApplicationCacheStatusImpl(InstrumentingAgents& instrumentingAgents, Frame* frame)
{
    if (InspectorApplicationCacheAgent* applicationCacheAgent = instrumentingAgents.inspectorApplicationCacheAgent())
        applicationCacheAgent->updateApplicationCacheStatus(frame);
}

bool InspectorInstrumentation::consoleAgentEnabled(ScriptExecutionContext* scriptExecutionContext)
{
    InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(scriptExecutionContext);
    InspectorConsoleAgent* consoleAgent = instrumentingAgents ? instrumentingAgents->webConsoleAgent() : nullptr;
    return consoleAgent && consoleAgent->enabled();
}

bool InspectorInstrumentation::timelineAgentEnabled(ScriptExecutionContext* scriptExecutionContext)
{
    InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(scriptExecutionContext);
    return instrumentingAgents && instrumentingAgents->inspectorTimelineAgent();
}

bool InspectorInstrumentation::replayAgentEnabled(ScriptExecutionContext* scriptExecutionContext)
{
#if ENABLE(WEB_REPLAY)
    InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(scriptExecutionContext);
    return instrumentingAgents && instrumentingAgents->inspectorReplayAgent();
#else
    UNUSED_PARAM(scriptExecutionContext);
    return false;
#endif
}

void InspectorInstrumentation::pauseOnNativeEventIfNeeded(InstrumentingAgents& instrumentingAgents, bool isDOMEvent, const String& eventName, bool synchronous)
{
    if (InspectorDOMDebuggerAgent* domDebuggerAgent = instrumentingAgents.inspectorDOMDebuggerAgent())
        domDebuggerAgent->pauseOnNativeEventIfNeeded(isDOMEvent, eventName, synchronous);
}

void InspectorInstrumentation::cancelPauseOnNativeEvent(InstrumentingAgents& instrumentingAgents)
{
    if (InspectorDebuggerAgent* debuggerAgent = instrumentingAgents.inspectorDebuggerAgent())
        debuggerAgent->cancelPauseOnNextStatement();
}

void InspectorInstrumentation::didRequestAnimationFrameImpl(InstrumentingAgents& instrumentingAgents, int callbackId, Frame* frame)
{
    pauseOnNativeEventIfNeeded(instrumentingAgents, false, requestAnimationFrameEventName, true);

    if (InspectorTimelineAgent* timelineAgent = instrumentingAgents.inspectorTimelineAgent())
        timelineAgent->didRequestAnimationFrame(callbackId, frame);
}

void InspectorInstrumentation::didCancelAnimationFrameImpl(InstrumentingAgents& instrumentingAgents, int callbackId, Frame* frame)
{
    pauseOnNativeEventIfNeeded(instrumentingAgents, false, cancelAnimationFrameEventName, true);

    if (InspectorTimelineAgent* timelineAgent = instrumentingAgents.inspectorTimelineAgent())
        timelineAgent->didCancelAnimationFrame(callbackId, frame);
}

InspectorInstrumentationCookie InspectorInstrumentation::willFireAnimationFrameImpl(InstrumentingAgents& instrumentingAgents, int callbackId, Frame* frame)
{
    pauseOnNativeEventIfNeeded(instrumentingAgents, false, animationFrameFiredEventName, false);

    int timelineAgentId = 0;
    if (InspectorTimelineAgent* timelineAgent = instrumentingAgents.inspectorTimelineAgent()) {
        timelineAgent->willFireAnimationFrame(callbackId, frame);
        timelineAgentId = timelineAgent->id();
    }
    return InspectorInstrumentationCookie(instrumentingAgents, timelineAgentId);
}

void InspectorInstrumentation::didFireAnimationFrameImpl(const InspectorInstrumentationCookie& cookie)
{
    if (InspectorTimelineAgent* timelineAgent = retrieveTimelineAgent(cookie))
        timelineAgent->didFireAnimationFrame();
}

void InspectorInstrumentation::registerInstrumentingAgents(InstrumentingAgents& instrumentingAgents)
{
    if (!s_instrumentingAgentsSet)
        s_instrumentingAgentsSet = new HashSet<InstrumentingAgents*>();

    s_instrumentingAgentsSet->add(&instrumentingAgents);
}

void InspectorInstrumentation::unregisterInstrumentingAgents(InstrumentingAgents& instrumentingAgents)
{
    if (!s_instrumentingAgentsSet)
        return;

    s_instrumentingAgentsSet->remove(&instrumentingAgents);
    if (s_instrumentingAgentsSet->isEmpty()) {
        delete s_instrumentingAgentsSet;
        s_instrumentingAgentsSet = nullptr;
    }
}

InspectorTimelineAgent* InspectorInstrumentation::retrieveTimelineAgent(const InspectorInstrumentationCookie& cookie)
{
    if (!cookie.isValid())
        return nullptr;

    InspectorTimelineAgent* timelineAgent = cookie.instrumentingAgents()->inspectorTimelineAgent();
    if (timelineAgent && cookie.hasMatchingTimelineAgentId(timelineAgent->id()))
        return timelineAgent;
    return nullptr;
}

InstrumentingAgents* InspectorInstrumentation::instrumentingAgentsForPage(Page* page)
{
    return page ? &instrumentingAgentsForPage(*page) : nullptr;
}

InstrumentingAgents& InspectorInstrumentation::instrumentingAgentsForPage(Page& page)
{
    ASSERT(isMainThread());
    return page.inspectorController().m_instrumentingAgents.get();
}

InstrumentingAgents* InspectorInstrumentation::instrumentingAgentsForRenderer(RenderObject* renderer)
{
    return instrumentingAgentsForFrame(renderer->frame());
}

void InspectorInstrumentation::layerTreeDidChangeImpl(InstrumentingAgents& instrumentingAgents)
{
    if (InspectorLayerTreeAgent* layerTreeAgent = instrumentingAgents.inspectorLayerTreeAgent())
        layerTreeAgent->layerTreeDidChange();
}

void InspectorInstrumentation::renderLayerDestroyedImpl(InstrumentingAgents& instrumentingAgents, const RenderLayer& renderLayer)
{
    if (InspectorLayerTreeAgent* layerTreeAgent = instrumentingAgents.inspectorLayerTreeAgent())
        layerTreeAgent->renderLayerDestroyed(renderLayer);
}

} // namespace WebCore
