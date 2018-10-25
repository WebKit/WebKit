/*
* Copyright (C) 2011 Google Inc. All rights reserved.
* Copyright (C) 2014-2017 Apple Inc. All rights reserved.
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

#include "CachedResource.h"
#include "DOMWindow.h"
#include "DOMWrapperWorld.h"
#include "Database.h"
#include "DocumentLoader.h"
#include "Event.h"
#include "Frame.h"
#include "InspectorApplicationCacheAgent.h"
#include "InspectorCSSAgent.h"
#include "InspectorCanvasAgent.h"
#include "InspectorDOMAgent.h"
#include "InspectorDOMDebuggerAgent.h"
#include "InspectorDOMStorageAgent.h"
#include "InspectorDatabaseAgent.h"
#include "InspectorLayerTreeAgent.h"
#include "InspectorMemoryAgent.h"
#include "InspectorNetworkAgent.h"
#include "InspectorPageAgent.h"
#include "InspectorTimelineAgent.h"
#include "InspectorWorkerAgent.h"
#include "InstrumentingAgents.h"
#include "LoaderStrategy.h"
#include "PageDebuggerAgent.h"
#include "PageHeapAgent.h"
#include "PageRuntimeAgent.h"
#include "PlatformStrategies.h"
#include "RenderObject.h"
#include "RenderView.h"
#include "ScriptController.h"
#include "ScriptExecutionContext.h"
#include "WebConsoleAgent.h"
#include "WebGLRenderingContextBase.h"
#include "WebSocketFrame.h"
#include <JavaScriptCore/ConsoleMessage.h>
#include <JavaScriptCore/ConsoleTypes.h>
#include <JavaScriptCore/InspectorDebuggerAgent.h>
#include <JavaScriptCore/ScriptArguments.h>
#include <JavaScriptCore/ScriptCallStack.h>
#include <wtf/StdLibExtras.h>

namespace WebCore {

using namespace Inspector;

namespace {
static HashSet<InstrumentingAgents*>* s_instrumentingAgentsSet = nullptr;
}

int InspectorInstrumentation::s_frontendCounter = 0;

void InspectorInstrumentation::firstFrontendCreated()
{
    platformStrategies()->loaderStrategy()->setCaptureExtraNetworkLoadMetricsEnabled(true);
}

void InspectorInstrumentation::lastFrontendDeleted()
{
    platformStrategies()->loaderStrategy()->setCaptureExtraNetworkLoadMetricsEnabled(false);
}

static Frame* frameForScriptExecutionContext(ScriptExecutionContext* context)
{
    Frame* frame = nullptr;
    if (is<Document>(*context))
        frame = downcast<Document>(*context).frame();
    return frame;
}

static Frame* frameForScriptExecutionContext(ScriptExecutionContext& context)
{
    Frame* frame = nullptr;
    if (is<Document>(context))
        frame = downcast<Document>(context).frame();
    return frame;
}

void InspectorInstrumentation::didClearWindowObjectInWorldImpl(InstrumentingAgents& instrumentingAgents, Frame& frame, DOMWrapperWorld& world)
{
    InspectorPageAgent* pageAgent = instrumentingAgents.inspectorPageAgent();
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

int InspectorInstrumentation::identifierForNodeImpl(InstrumentingAgents& instrumentingAgents, Node& node)
{
    if (InspectorDOMAgent* domAgent = instrumentingAgents.inspectorDOMAgent())
        return domAgent->identifierForNode(node);
    return 0;
}

void InspectorInstrumentation::addEventListenersToNodeImpl(InstrumentingAgents& instrumentingAgents, Node& node)
{
    if (InspectorDOMAgent* domAgent = instrumentingAgents.inspectorDOMAgent())
        domAgent->addEventListenersToNode(node);
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

void InspectorInstrumentation::didChangeCustomElementStateImpl(InstrumentingAgents& instrumentingAgents, Element& element)
{
    if (InspectorDOMAgent* domAgent = instrumentingAgents.inspectorDOMAgent())
        domAgent->didChangeCustomElementState(element);
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

void InspectorInstrumentation::didInstallTimerImpl(InstrumentingAgents& instrumentingAgents, int timerId, Seconds timeout, bool singleShot, ScriptExecutionContext& context)
{
    if (InspectorDebuggerAgent* debuggerAgent = instrumentingAgents.inspectorDebuggerAgent())
        debuggerAgent->didScheduleAsyncCall(context.execState(), InspectorDebuggerAgent::AsyncCallType::DOMTimer, timerId, singleShot);

    if (InspectorTimelineAgent* timelineAgent = instrumentingAgents.inspectorTimelineAgent())
        timelineAgent->didInstallTimer(timerId, timeout, singleShot, frameForScriptExecutionContext(context));
}

void InspectorInstrumentation::didRemoveTimerImpl(InstrumentingAgents& instrumentingAgents, int timerId, ScriptExecutionContext& context)
{
    if (InspectorDebuggerAgent* debuggerAgent = instrumentingAgents.inspectorDebuggerAgent())
        debuggerAgent->didCancelAsyncCall(InspectorDebuggerAgent::AsyncCallType::DOMTimer, timerId);
    if (InspectorTimelineAgent* timelineAgent = instrumentingAgents.inspectorTimelineAgent())
        timelineAgent->didRemoveTimer(timerId, frameForScriptExecutionContext(context));
}

void InspectorInstrumentation::didAddEventListenerImpl(InstrumentingAgents& instrumentingAgents, EventTarget& target, const AtomicString& eventType, EventListener& listener, bool capture)
{
    if (PageDebuggerAgent* pageDebuggerAgent = instrumentingAgents.pageDebuggerAgent())
        pageDebuggerAgent->didAddEventListener(target, eventType, listener, capture);
    if (InspectorDOMAgent* domAgent = instrumentingAgents.inspectorDOMAgent())
        domAgent->didAddEventListener(target);
}

void InspectorInstrumentation::willRemoveEventListenerImpl(InstrumentingAgents& instrumentingAgents, EventTarget& target, const AtomicString& eventType, EventListener& listener, bool capture)
{
    if (PageDebuggerAgent* pageDebuggerAgent = instrumentingAgents.pageDebuggerAgent())
        pageDebuggerAgent->willRemoveEventListener(target, eventType, listener, capture);
    if (InspectorDOMAgent* domAgent = instrumentingAgents.inspectorDOMAgent())
        domAgent->willRemoveEventListener(target, eventType, listener, capture);
}

bool InspectorInstrumentation::isEventListenerDisabledImpl(InstrumentingAgents& instrumentingAgents, EventTarget& target, const AtomicString& eventType, EventListener& listener, bool capture)
{
    if (InspectorDOMAgent* domAgent = instrumentingAgents.inspectorDOMAgent())
        return domAgent->isEventListenerDisabled(target, eventType, listener, capture);
    return false;
}

void InspectorInstrumentation::didPostMessageImpl(InstrumentingAgents& instrumentingAgents, const TimerBase& timer, JSC::ExecState& state)
{
    if (PageDebuggerAgent* pageDebuggerAgent = instrumentingAgents.pageDebuggerAgent())
        pageDebuggerAgent->didPostMessage(timer, state);
}

void InspectorInstrumentation::didFailPostMessageImpl(InstrumentingAgents& instrumentingAgents, const TimerBase& timer)
{
    if (PageDebuggerAgent* pageDebuggerAgent = instrumentingAgents.pageDebuggerAgent())
        pageDebuggerAgent->didFailPostMessage(timer);
}

void InspectorInstrumentation::willDispatchPostMessageImpl(InstrumentingAgents& instrumentingAgents, const TimerBase& timer)
{
    if (PageDebuggerAgent* pageDebuggerAgent = instrumentingAgents.pageDebuggerAgent())
        pageDebuggerAgent->willDispatchPostMessage(timer);
}

void InspectorInstrumentation::didDispatchPostMessageImpl(InstrumentingAgents& instrumentingAgents, const TimerBase& timer)
{
    if (PageDebuggerAgent* pageDebuggerAgent = instrumentingAgents.pageDebuggerAgent())
        pageDebuggerAgent->didDispatchPostMessage(timer);
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

    return InspectorInstrumentationCookie(instrumentingAgents, timelineAgentId);
}

void InspectorInstrumentation::willHandleEventImpl(InstrumentingAgents& instrumentingAgents, const Event& event, const RegisteredEventListener& listener)
{
    if (PageDebuggerAgent* pageDebuggerAgent = instrumentingAgents.pageDebuggerAgent())
        pageDebuggerAgent->willHandleEvent(listener);

    if (InspectorDOMDebuggerAgent* domDebuggerAgent = instrumentingAgents.inspectorDOMDebuggerAgent())
        domDebuggerAgent->willHandleEvent(event, listener);
}

void InspectorInstrumentation::didHandleEventImpl(InstrumentingAgents& instrumentingAgents)
{
    if (InspectorDebuggerAgent* debuggerAgent = instrumentingAgents.inspectorDebuggerAgent())
        debuggerAgent->didDispatchAsyncCall();
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

    return InspectorInstrumentationCookie(instrumentingAgents, timelineAgentId);
}

void InspectorInstrumentation::didDispatchEventOnWindowImpl(const InspectorInstrumentationCookie& cookie)
{
    if (InspectorTimelineAgent* timelineAgent = retrieveTimelineAgent(cookie))
        timelineAgent->didDispatchEvent();
}

void InspectorInstrumentation::eventDidResetAfterDispatchImpl(InstrumentingAgents& instrumentingAgents, const Event& event)
{
    if (auto* domAgent = instrumentingAgents.inspectorDOMAgent())
        domAgent->eventDidResetAfterDispatch(event);
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

InspectorInstrumentationCookie InspectorInstrumentation::willFireTimerImpl(InstrumentingAgents& instrumentingAgents, int timerId, bool oneShot, ScriptExecutionContext& context)
{
    if (InspectorDebuggerAgent* debuggerAgent = instrumentingAgents.inspectorDebuggerAgent())
        debuggerAgent->willDispatchAsyncCall(InspectorDebuggerAgent::AsyncCallType::DOMTimer, timerId);

    if (InspectorDOMDebuggerAgent* domDebuggerAgent = instrumentingAgents.inspectorDOMDebuggerAgent())
        domDebuggerAgent->willFireTimer(oneShot);

    int timelineAgentId = 0;
    if (InspectorTimelineAgent* timelineAgent = instrumentingAgents.inspectorTimelineAgent()) {
        timelineAgent->willFireTimer(timerId, frameForScriptExecutionContext(context));
        timelineAgentId = timelineAgent->id();
    }
    return InspectorInstrumentationCookie(instrumentingAgents, timelineAgentId);
}

void InspectorInstrumentation::didFireTimerImpl(const InspectorInstrumentationCookie& cookie)
{
    if (InspectorDebuggerAgent* debuggerAgent = cookie.instrumentingAgents()->inspectorDebuggerAgent())
        debuggerAgent->didDispatchAsyncCall();
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

void InspectorInstrumentation::didLayoutImpl(const InspectorInstrumentationCookie& cookie, RenderObject& root)
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

void InspectorInstrumentation::willPaintImpl(InstrumentingAgents& instrumentingAgents, RenderObject& renderer)
{
    if (InspectorTimelineAgent* timelineAgent = instrumentingAgents.inspectorTimelineAgent())
        timelineAgent->willPaint(renderer.frame());
}

void InspectorInstrumentation::didPaintImpl(InstrumentingAgents& instrumentingAgents, RenderObject& renderer, const LayoutRect& rect)
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
    if (InspectorNetworkAgent* networkAgent = instrumentingAgents.inspectorNetworkAgent())
        networkAgent->willSendRequest(identifier, loader, request, redirectResponse);
}

void InspectorInstrumentation::willSendRequestOfTypeImpl(InstrumentingAgents& instrumentingAgents, unsigned long identifier, DocumentLoader* loader, ResourceRequest& request, LoadType loadType)
{
    if (InspectorNetworkAgent* networkAgent = instrumentingAgents.inspectorNetworkAgent())
        networkAgent->willSendRequestOfType(identifier, loader, request, loadType);
}

void InspectorInstrumentation::didLoadResourceFromMemoryCacheImpl(InstrumentingAgents& instrumentingAgents, DocumentLoader* loader, CachedResource* cachedResource)
{
    if (!loader || !cachedResource)
        return;

    if (InspectorNetworkAgent* networkAgent = instrumentingAgents.inspectorNetworkAgent())
        networkAgent->didLoadResourceFromMemoryCache(loader, *cachedResource);
}

void InspectorInstrumentation::didReceiveResourceResponseImpl(InstrumentingAgents& instrumentingAgents, unsigned long identifier, DocumentLoader* loader, const ResourceResponse& response, ResourceLoader* resourceLoader)
{
    if (InspectorNetworkAgent* networkAgent = instrumentingAgents.inspectorNetworkAgent())
        networkAgent->didReceiveResponse(identifier, loader, response, resourceLoader);
    if (WebConsoleAgent* consoleAgent = instrumentingAgents.webConsoleAgent())
        consoleAgent->didReceiveResponse(identifier, response); // This should come AFTER resource notification, front-end relies on this.
}

void InspectorInstrumentation::didReceiveThreadableLoaderResponseImpl(InstrumentingAgents& instrumentingAgents, DocumentThreadableLoader& documentThreadableLoader, unsigned long identifier)
{
    if (InspectorNetworkAgent* networkAgent = instrumentingAgents.inspectorNetworkAgent())
        networkAgent->didReceiveThreadableLoaderResponse(identifier, documentThreadableLoader);
}

void InspectorInstrumentation::didReceiveDataImpl(InstrumentingAgents& instrumentingAgents, unsigned long identifier, const char* data, int dataLength, int encodedDataLength)
{
    if (InspectorNetworkAgent* networkAgent = instrumentingAgents.inspectorNetworkAgent())
        networkAgent->didReceiveData(identifier, data, dataLength, encodedDataLength);
}

void InspectorInstrumentation::didFinishLoadingImpl(InstrumentingAgents& instrumentingAgents, unsigned long identifier, DocumentLoader* loader, const NetworkLoadMetrics& networkLoadMetrics, ResourceLoader* resourceLoader)
{
    if (InspectorNetworkAgent* networkAgent = instrumentingAgents.inspectorNetworkAgent())
        networkAgent->didFinishLoading(identifier, loader, networkLoadMetrics, resourceLoader);
}

void InspectorInstrumentation::didFailLoadingImpl(InstrumentingAgents& instrumentingAgents, unsigned long identifier, DocumentLoader* loader, const ResourceError& error)
{
    if (InspectorNetworkAgent* networkAgent = instrumentingAgents.inspectorNetworkAgent())
        networkAgent->didFailLoading(identifier, loader, error);
    if (WebConsoleAgent* consoleAgent = instrumentingAgents.webConsoleAgent())
        consoleAgent->didFailLoading(identifier, error); // This should come AFTER resource notification, front-end relies on this.
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
}

void InspectorInstrumentation::didCommitLoadImpl(InstrumentingAgents& instrumentingAgents, Frame& frame, DocumentLoader* loader)
{
    if (!instrumentingAgents.inspectorEnvironment().developerExtrasEnabled())
        return;

    if (!frame.page())
        return;

    if (!loader)
        return;

    ASSERT(loader->frame() == &frame);

    if (frame.isMainFrame()) {
        if (WebConsoleAgent* consoleAgent = instrumentingAgents.webConsoleAgent())
            consoleAgent->reset();

        if (InspectorNetworkAgent* networkAgent = instrumentingAgents.inspectorNetworkAgent())
            networkAgent->mainFrameNavigated(*loader);

        if (InspectorCSSAgent* cssAgent = instrumentingAgents.inspectorCSSAgent())
            cssAgent->reset();

        if (InspectorDatabaseAgent* databaseAgent = instrumentingAgents.inspectorDatabaseAgent())
            databaseAgent->clearResources();

        if (InspectorDOMAgent* domAgent = instrumentingAgents.inspectorDOMAgent())
            domAgent->setDocument(frame.document());

        if (InspectorLayerTreeAgent* layerTreeAgent = instrumentingAgents.inspectorLayerTreeAgent())
            layerTreeAgent->reset();

        if (PageDebuggerAgent* pageDebuggerAgent = instrumentingAgents.pageDebuggerAgent())
            pageDebuggerAgent->mainFrameNavigated();

        if (PageHeapAgent* pageHeapAgent = instrumentingAgents.pageHeapAgent())
            pageHeapAgent->mainFrameNavigated();
    }

    if (InspectorCanvasAgent* canvasAgent = instrumentingAgents.inspectorCanvasAgent())
        canvasAgent->frameNavigated(frame);

    if (InspectorDOMAgent* domAgent = instrumentingAgents.inspectorDOMAgent())
        domAgent->didCommitLoad(frame.document());

    if (InspectorPageAgent* pageAgent = instrumentingAgents.inspectorPageAgent())
        pageAgent->frameNavigated(frame);

    if (frame.isMainFrame()) {
        if (InspectorTimelineAgent* timelineAgent = instrumentingAgents.inspectorTimelineAgent())
            timelineAgent->mainFrameNavigated();
    }
}

void InspectorInstrumentation::frameDocumentUpdatedImpl(InstrumentingAgents& instrumentingAgents, Frame& frame)
{
    if (InspectorDOMAgent* domAgent = instrumentingAgents.inspectorDOMAgent())
        domAgent->frameDocumentUpdated(frame);

    if (InspectorDOMDebuggerAgent* domDebuggerAgent = instrumentingAgents.inspectorDOMDebuggerAgent())
        domDebuggerAgent->frameDocumentUpdated(frame);
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
        if (InspectorTimelineAgent* timelineAgent = instrumentingAgents.persistentInspectorTimelineAgent())
            timelineAgent->mainFrameStartedLoading();
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

void InspectorInstrumentation::frameScheduledNavigationImpl(InstrumentingAgents& instrumentingAgents, Frame& frame, Seconds delay)
{
    if (InspectorPageAgent* inspectorPageAgent = instrumentingAgents.inspectorPageAgent())
        inspectorPageAgent->frameScheduledNavigation(frame, delay);
}

void InspectorInstrumentation::frameClearedScheduledNavigationImpl(InstrumentingAgents& instrumentingAgents, Frame& frame)
{
    if (InspectorPageAgent* inspectorPageAgent = instrumentingAgents.inspectorPageAgent())
        inspectorPageAgent->frameClearedScheduledNavigation(frame);
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

void InspectorInstrumentation::consoleCountImpl(InstrumentingAgents& instrumentingAgents, JSC::ExecState* state, Ref<ScriptArguments>&& arguments)
{
    if (WebConsoleAgent* consoleAgent = instrumentingAgents.webConsoleAgent())
        consoleAgent->count(state, WTFMove(arguments));
}

void InspectorInstrumentation::takeHeapSnapshotImpl(InstrumentingAgents& instrumentingAgents, const String& title)
{
    if (WebConsoleAgent* consoleAgent = instrumentingAgents.webConsoleAgent())
        consoleAgent->takeHeapSnapshot(title);
}

void InspectorInstrumentation::startConsoleTimingImpl(InstrumentingAgents& instrumentingAgents, Frame& frame, const String& title)
{
    if (InspectorTimelineAgent* timelineAgent = instrumentingAgents.inspectorTimelineAgent())
        timelineAgent->time(frame, title);
    if (WebConsoleAgent* consoleAgent = instrumentingAgents.webConsoleAgent())
        consoleAgent->startTiming(title);
}

void InspectorInstrumentation::startConsoleTimingImpl(InstrumentingAgents& instrumentingAgents, const String& title)
{
    if (WebConsoleAgent* consoleAgent = instrumentingAgents.webConsoleAgent())
        consoleAgent->startTiming(title);
}

void InspectorInstrumentation::stopConsoleTimingImpl(InstrumentingAgents& instrumentingAgents, Frame& frame, const String& title, Ref<ScriptCallStack>&& stack)
{
    if (WebConsoleAgent* consoleAgent = instrumentingAgents.webConsoleAgent())
        consoleAgent->stopTiming(title, WTFMove(stack));
    if (InspectorTimelineAgent* timelineAgent = instrumentingAgents.inspectorTimelineAgent())
        timelineAgent->timeEnd(frame, title);
}

void InspectorInstrumentation::stopConsoleTimingImpl(InstrumentingAgents& instrumentingAgents, const String& title, Ref<ScriptCallStack>&& stack)
{
    if (WebConsoleAgent* consoleAgent = instrumentingAgents.webConsoleAgent())
        consoleAgent->stopTiming(title, WTFMove(stack));
}

void InspectorInstrumentation::consoleTimeStampImpl(InstrumentingAgents& instrumentingAgents, Frame& frame, Ref<ScriptArguments>&& arguments)
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

void InspectorInstrumentation::stopProfilingImpl(InstrumentingAgents& instrumentingAgents, JSC::ExecState* exec, const String& title)
{
    if (InspectorTimelineAgent* timelineAgent = instrumentingAgents.persistentInspectorTimelineAgent())
        timelineAgent->stopFromConsole(exec, title);
}

void InspectorInstrumentation::consoleStartRecordingCanvasImpl(InstrumentingAgents& instrumentingAgents, CanvasRenderingContext& context, JSC::ExecState& exec, JSC::JSObject* options)
{
    if (InspectorCanvasAgent* canvasAgent = instrumentingAgents.inspectorCanvasAgent())
        canvasAgent->consoleStartRecordingCanvas(context, exec, options);
}

void InspectorInstrumentation::didOpenDatabaseImpl(InstrumentingAgents& instrumentingAgents, RefPtr<Database>&& database, const String& domain, const String& name, const String& version)
{
    if (!instrumentingAgents.inspectorEnvironment().developerExtrasEnabled())
        return;
    if (InspectorDatabaseAgent* dbAgent = instrumentingAgents.inspectorDatabaseAgent())
        dbAgent->didOpenDatabase(WTFMove(database), domain, name, version);
}

void InspectorInstrumentation::didDispatchDOMStorageEventImpl(InstrumentingAgents& instrumentingAgents, const String& key, const String& oldValue, const String& newValue, StorageType storageType, SecurityOrigin* securityOrigin)
{
    if (InspectorDOMStorageAgent* domStorageAgent = instrumentingAgents.inspectorDOMStorageAgent())
        domStorageAgent->didDispatchDOMStorageEvent(key, oldValue, newValue, storageType, securityOrigin);
}

bool InspectorInstrumentation::shouldWaitForDebuggerOnStartImpl(InstrumentingAgents& instrumentingAgents)
{
    if (InspectorWorkerAgent* workerAgent = instrumentingAgents.inspectorWorkerAgent())
        return workerAgent->shouldWaitForDebuggerOnStart();
    return false;
}

void InspectorInstrumentation::workerStartedImpl(InstrumentingAgents& instrumentingAgents, WorkerInspectorProxy* proxy, const URL& url)
{
    if (InspectorWorkerAgent* workerAgent = instrumentingAgents.inspectorWorkerAgent())
        workerAgent->workerStarted(proxy, url);
}

void InspectorInstrumentation::workerTerminatedImpl(InstrumentingAgents& instrumentingAgents, WorkerInspectorProxy* proxy)
{
    if (InspectorWorkerAgent* workerAgent = instrumentingAgents.inspectorWorkerAgent())
        workerAgent->workerTerminated(proxy);
}

void InspectorInstrumentation::didCreateWebSocketImpl(InstrumentingAgents& instrumentingAgents, unsigned long identifier, const URL& requestURL)
{
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

void InspectorInstrumentation::didChangeCSSCanvasClientNodesImpl(InstrumentingAgents& instrumentingAgents, CanvasBase& canvasBase)
{
    if (InspectorCanvasAgent* canvasAgent = instrumentingAgents.inspectorCanvasAgent())
        canvasAgent->didChangeCSSCanvasClientNodes(canvasBase);
}

void InspectorInstrumentation::didCreateCanvasRenderingContextImpl(InstrumentingAgents& instrumentingAgents, CanvasRenderingContext& context)
{
    if (InspectorCanvasAgent* canvasAgent = instrumentingAgents.inspectorCanvasAgent())
        canvasAgent->didCreateCanvasRenderingContext(context);
}

void InspectorInstrumentation::didChangeCanvasMemoryImpl(InstrumentingAgents& instrumentingAgents, CanvasRenderingContext& context)
{
    if (InspectorCanvasAgent* canvasAgent = instrumentingAgents.inspectorCanvasAgent())
        canvasAgent->didChangeCanvasMemory(context);
}

void InspectorInstrumentation::recordCanvasActionImpl(InstrumentingAgents& instrumentingAgents, CanvasRenderingContext& canvasRenderingContext, const String& name, Vector<RecordCanvasActionVariant>&& parameters)
{
    if (InspectorCanvasAgent* canvasAgent = instrumentingAgents.inspectorCanvasAgent())
        canvasAgent->recordCanvasAction(canvasRenderingContext, name, WTFMove(parameters));
}

void InspectorInstrumentation::didFinishRecordingCanvasFrameImpl(InstrumentingAgents& instrumentingAgents, CanvasRenderingContext& context, bool forceDispatch)
{
    if (InspectorCanvasAgent* canvasAgent = instrumentingAgents.inspectorCanvasAgent())
        canvasAgent->didFinishRecordingCanvasFrame(context, forceDispatch);
}

#if ENABLE(WEBGL)
void InspectorInstrumentation::didEnableExtensionImpl(InstrumentingAgents& instrumentingAgents, WebGLRenderingContextBase& contextWebGLBase, const String& extension)
{
    if (InspectorCanvasAgent* canvasAgent = instrumentingAgents.inspectorCanvasAgent())
        canvasAgent->didEnableExtension(contextWebGLBase, extension);
}

void InspectorInstrumentation::didCreateProgramImpl(InstrumentingAgents& instrumentingAgents, WebGLRenderingContextBase& contextWebGLBase, WebGLProgram& program)
{
    if (InspectorCanvasAgent* canvasAgent = instrumentingAgents.inspectorCanvasAgent())
        canvasAgent->didCreateProgram(contextWebGLBase, program);
}

void InspectorInstrumentation::willDeleteProgramImpl(InstrumentingAgents& instrumentingAgents, WebGLProgram& program)
{
    if (InspectorCanvasAgent* canvasAgent = instrumentingAgents.inspectorCanvasAgent())
        canvasAgent->willDeleteProgram(program);
}

bool InspectorInstrumentation::isShaderProgramDisabledImpl(InstrumentingAgents& instrumentingAgents, WebGLProgram& program)
{
    if (InspectorCanvasAgent* canvasAgent = instrumentingAgents.inspectorCanvasAgent())
        return canvasAgent->isShaderProgramDisabled(program);
    return false;
}

bool InspectorInstrumentation::isShaderProgramHighlightedImpl(InstrumentingAgents& instrumentingAgents, WebGLProgram& program)
{
    if (InspectorCanvasAgent* canvasAgent = instrumentingAgents.inspectorCanvasAgent())
        return canvasAgent->isShaderProgramHighlighted(program);
    return false;
}
#endif

#if ENABLE(RESOURCE_USAGE)
void InspectorInstrumentation::didHandleMemoryPressureImpl(InstrumentingAgents& instrumentingAgents, Critical critical)
{
    if (InspectorMemoryAgent* memoryAgent = instrumentingAgents.inspectorMemoryAgent())
        memoryAgent->didHandleMemoryPressure(critical);
}
#endif

void InspectorInstrumentation::networkStateChangedImpl(InstrumentingAgents& instrumentingAgents)
{
    if (InspectorApplicationCacheAgent* applicationCacheAgent = instrumentingAgents.inspectorApplicationCacheAgent())
        applicationCacheAgent->networkStateChanged();
}

void InspectorInstrumentation::updateApplicationCacheStatusImpl(InstrumentingAgents& instrumentingAgents, Frame& frame)
{
    if (auto* applicationCacheAgent = instrumentingAgents.inspectorApplicationCacheAgent())
        applicationCacheAgent->updateApplicationCacheStatus(&frame);
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

void InspectorInstrumentation::didRequestAnimationFrameImpl(InstrumentingAgents& instrumentingAgents, int callbackId, Document& document)
{
    if (PageDebuggerAgent* pageDebuggerAgent = instrumentingAgents.pageDebuggerAgent())
        pageDebuggerAgent->didRequestAnimationFrame(callbackId, document);
    if (InspectorTimelineAgent* timelineAgent = instrumentingAgents.inspectorTimelineAgent())
        timelineAgent->didRequestAnimationFrame(callbackId, document.frame());
}

void InspectorInstrumentation::didCancelAnimationFrameImpl(InstrumentingAgents& instrumentingAgents, int callbackId, Document& document)
{
    if (PageDebuggerAgent* pageDebuggerAgent = instrumentingAgents.pageDebuggerAgent())
        pageDebuggerAgent->didCancelAnimationFrame(callbackId);
    if (InspectorTimelineAgent* timelineAgent = instrumentingAgents.inspectorTimelineAgent())
        timelineAgent->didCancelAnimationFrame(callbackId, document.frame());
}

InspectorInstrumentationCookie InspectorInstrumentation::willFireAnimationFrameImpl(InstrumentingAgents& instrumentingAgents, int callbackId, Document& document)
{
    if (PageDebuggerAgent* pageDebuggerAgent = instrumentingAgents.pageDebuggerAgent())
        pageDebuggerAgent->willFireAnimationFrame(callbackId);

    if (InspectorDOMDebuggerAgent* domDebuggerAgent = instrumentingAgents.inspectorDOMDebuggerAgent())
        domDebuggerAgent->willFireAnimationFrame();

    int timelineAgentId = 0;
    if (InspectorTimelineAgent* timelineAgent = instrumentingAgents.inspectorTimelineAgent()) {
        timelineAgent->willFireAnimationFrame(callbackId, document.frame());
        timelineAgentId = timelineAgent->id();
    }
    return InspectorInstrumentationCookie(instrumentingAgents, timelineAgentId);
}

void InspectorInstrumentation::didFireAnimationFrameImpl(const InspectorInstrumentationCookie& cookie)
{
    if (InspectorDebuggerAgent* debuggerAgent = cookie.instrumentingAgents()->inspectorDebuggerAgent())
        debuggerAgent->didDispatchAsyncCall();
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

InstrumentingAgents* InspectorInstrumentation::instrumentingAgentsForRenderer(RenderObject& renderer)
{
    return instrumentingAgentsForFrame(renderer.frame());
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
