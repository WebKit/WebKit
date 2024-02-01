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
#include "ComputedEffectTiming.h"
#include "DOMWrapperWorld.h"
#include "DocumentLoader.h"
#include "Event.h"
#include "InspectorAnimationAgent.h"
#include "InspectorApplicationCacheAgent.h"
#include "InspectorCSSAgent.h"
#include "InspectorCanvasAgent.h"
#include "InspectorController.h"
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
#include "KeyframeEffect.h"
#include "LoaderStrategy.h"
#include "LocalDOMWindow.h"
#include "LocalFrame.h"
#include "PageCanvasAgent.h"
#include "PageDOMDebuggerAgent.h"
#include "PageDebuggerAgent.h"
#include "PageHeapAgent.h"
#include "PageRuntimeAgent.h"
#include "PlatformStrategies.h"
#include "RenderObject.h"
#include "RenderView.h"
#include "ScriptController.h"
#include "ScriptExecutionContext.h"
#include "WebConsoleAgent.h"
#include "WebDebuggerAgent.h"
#include "WebGLRenderingContextBase.h"
#include "WebSocketFrame.h"
#include "WorkerInspectorController.h"
#include "WorkerOrWorkletGlobalScope.h"
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

void InspectorInstrumentation::firstFrontendCreated()
{
    platformStrategies()->loaderStrategy()->setCaptureExtraNetworkLoadMetricsEnabled(true);
}

void InspectorInstrumentation::lastFrontendDeleted()
{
    platformStrategies()->loaderStrategy()->setCaptureExtraNetworkLoadMetricsEnabled(false);
}

static LocalFrame* frameForScriptExecutionContext(ScriptExecutionContext* context)
{
    if (RefPtr document = dynamicDowncast<Document>(*context))
        return document->frame();
    return nullptr;
}

static LocalFrame* frameForScriptExecutionContext(ScriptExecutionContext& context)
{
    if (RefPtr document = dynamicDowncast<Document>(context))
        return document->frame();
    return nullptr;
}

void InspectorInstrumentation::didClearWindowObjectInWorldImpl(InstrumentingAgents& instrumentingAgents, LocalFrame& frame, DOMWrapperWorld& world)
{
    if (auto* pageDebuggerAgent = instrumentingAgents.enabledPageDebuggerAgent())
        pageDebuggerAgent->didClearWindowObjectInWorld(frame, world);

    if (auto* pageRuntimeAgent = instrumentingAgents.enabledPageRuntimeAgent())
        pageRuntimeAgent->didClearWindowObjectInWorld(frame, world);

    if (auto* pageAgent = instrumentingAgents.enabledPageAgent())
        pageAgent->didClearWindowObjectInWorld(frame, world);
}

bool InspectorInstrumentation::isDebuggerPausedImpl(InstrumentingAgents& instrumentingAgents)
{
    if (auto* webDebuggerAgent = instrumentingAgents.enabledWebDebuggerAgent())
        return webDebuggerAgent->isPaused();
    return false;
}

int InspectorInstrumentation::identifierForNodeImpl(InstrumentingAgents& instrumentingAgents, Node& node)
{
    if (auto* domAgent = instrumentingAgents.persistentDOMAgent())
        return domAgent->identifierForNode(node);
    return 0;
}

void InspectorInstrumentation::addEventListenersToNodeImpl(InstrumentingAgents& instrumentingAgents, Node& node)
{
    if (auto* domAgent = instrumentingAgents.persistentDOMAgent())
        domAgent->addEventListenersToNode(node);
}

void InspectorInstrumentation::willInsertDOMNodeImpl(InstrumentingAgents& instrumentingAgents, Node& parent)
{
    if (auto* pageDOMDebuggerAgent = instrumentingAgents.enabledPageDOMDebuggerAgent())
        pageDOMDebuggerAgent->willInsertDOMNode(parent);
}

void InspectorInstrumentation::didInsertDOMNodeImpl(InstrumentingAgents& instrumentingAgents, Node& node)
{
    if (auto* domAgent = instrumentingAgents.persistentDOMAgent())
        domAgent->didInsertDOMNode(node);
}

void InspectorInstrumentation::willRemoveDOMNodeImpl(InstrumentingAgents& instrumentingAgents, Node& node)
{
    if (auto* pageDOMDebuggerAgent = instrumentingAgents.enabledPageDOMDebuggerAgent())
        pageDOMDebuggerAgent->willRemoveDOMNode(node);
}

void InspectorInstrumentation::didRemoveDOMNodeImpl(InstrumentingAgents& instrumentingAgents, Node& node)
{
    if (auto* pageDOMDebuggerAgent = instrumentingAgents.enabledPageDOMDebuggerAgent())
        pageDOMDebuggerAgent->didRemoveDOMNode(node);
    if (auto* domAgent = instrumentingAgents.persistentDOMAgent())
        domAgent->didRemoveDOMNode(node);
}

void InspectorInstrumentation::willDestroyDOMNodeImpl(InstrumentingAgents& instrumentingAgents, Node& node)
{
    if (auto* pageDOMDebuggerAgent = instrumentingAgents.enabledPageDOMDebuggerAgent())
        pageDOMDebuggerAgent->willDestroyDOMNode(node);
    if (auto* domAgent = instrumentingAgents.persistentDOMAgent())
        domAgent->willDestroyDOMNode(node);
}

void InspectorInstrumentation::didChangeRendererForDOMNodeImpl(InstrumentingAgents& instrumentingAgents, Node& node)
{
    if (auto* cssAgent = instrumentingAgents.enabledCSSAgent())
        cssAgent->didChangeRendererForDOMNode(node);
}

void InspectorInstrumentation::didAddOrRemoveScrollbarsImpl(InstrumentingAgents& instrumentingAgents, LocalFrameView& frameView)
{
    auto* cssAgent = instrumentingAgents.enabledCSSAgent();
    if (!cssAgent)
        return;
    auto* document = frameView.frame().document();
    if (!document)
        return;
    auto* documentElement = document->documentElement();
    if (!documentElement)
        return;
    cssAgent->didChangeRendererForDOMNode(*documentElement);
}

void InspectorInstrumentation::didAddOrRemoveScrollbarsImpl(InstrumentingAgents& instrumentingAgents, RenderObject& renderer)
{
    if (auto* cssAgent = instrumentingAgents.enabledCSSAgent()) {
        if (auto* node = renderer.node())
            cssAgent->didChangeRendererForDOMNode(*node);
    }
}

void InspectorInstrumentation::willModifyDOMAttrImpl(InstrumentingAgents& instrumentingAgents, Element& element, const AtomString& oldValue, const AtomString& newValue)
{
    if (auto* pageDOMDebuggerAgent = instrumentingAgents.enabledPageDOMDebuggerAgent())
        pageDOMDebuggerAgent->willModifyDOMAttr(element);
    if (auto* domAgent = instrumentingAgents.persistentDOMAgent())
        domAgent->willModifyDOMAttr(element, oldValue, newValue);
}

void InspectorInstrumentation::didModifyDOMAttrImpl(InstrumentingAgents& instrumentingAgents, Element& element, const AtomString& name, const AtomString& value)
{
    if (auto* domAgent = instrumentingAgents.persistentDOMAgent())
        domAgent->didModifyDOMAttr(element, name, value);
}

void InspectorInstrumentation::didRemoveDOMAttrImpl(InstrumentingAgents& instrumentingAgents, Element& element, const AtomString& name)
{
    if (auto* domAgent = instrumentingAgents.persistentDOMAgent())
        domAgent->didRemoveDOMAttr(element, name);
}

void InspectorInstrumentation::willInvalidateStyleAttrImpl(InstrumentingAgents& instrumentingAgents, Element& element)
{
    if (auto* pageDOMDebuggerAgent = instrumentingAgents.enabledPageDOMDebuggerAgent())
        pageDOMDebuggerAgent->willInvalidateStyleAttr(element);
}

void InspectorInstrumentation::didInvalidateStyleAttrImpl(InstrumentingAgents& instrumentingAgents, Element& element)
{
    if (auto* domAgent = instrumentingAgents.persistentDOMAgent())
        domAgent->didInvalidateStyleAttr(element);
}

void InspectorInstrumentation::documentDetachedImpl(InstrumentingAgents& instrumentingAgents, Document& document)
{
    if (auto* cssAgent = instrumentingAgents.enabledCSSAgent())
        cssAgent->documentDetached(document);
}

void InspectorInstrumentation::frameWindowDiscardedImpl(InstrumentingAgents& instrumentingAgents, LocalDOMWindow* window)
{
    if (LIKELY(!instrumentingAgents.inspectorEnvironment().developerExtrasEnabled()))
        return;

    if (!window)
        return;

    if (auto* consoleAgent = instrumentingAgents.webConsoleAgent())
        consoleAgent->frameWindowDiscarded(*window);
}

void InspectorInstrumentation::mediaQueryResultChangedImpl(InstrumentingAgents& instrumentingAgents)
{
    if (auto* cssAgent = instrumentingAgents.enabledCSSAgent())
        cssAgent->mediaQueryResultChanged();
}

void InspectorInstrumentation::activeStyleSheetsUpdatedImpl(InstrumentingAgents& instrumentingAgents, Document& document)
{
    if (auto* cssAgent = instrumentingAgents.enabledCSSAgent())
        cssAgent->activeStyleSheetsUpdated(document);
}

void InspectorInstrumentation::didPushShadowRootImpl(InstrumentingAgents& instrumentingAgents, Element& host, ShadowRoot& root)
{
    if (auto* domAgent = instrumentingAgents.persistentDOMAgent())
        domAgent->didPushShadowRoot(host, root);
}

void InspectorInstrumentation::willPopShadowRootImpl(InstrumentingAgents& instrumentingAgents, Element& host, ShadowRoot& root)
{
    if (auto* domAgent = instrumentingAgents.persistentDOMAgent())
        domAgent->willPopShadowRoot(host, root);
}

void InspectorInstrumentation::didChangeCustomElementStateImpl(InstrumentingAgents& instrumentingAgents, Element& element)
{
    if (auto* domAgent = instrumentingAgents.persistentDOMAgent())
        domAgent->didChangeCustomElementState(element);
}

void InspectorInstrumentation::pseudoElementCreatedImpl(InstrumentingAgents& instrumentingAgents, PseudoElement& pseudoElement)
{
    if (auto* domAgent = instrumentingAgents.persistentDOMAgent())
        domAgent->pseudoElementCreated(pseudoElement);
}

void InspectorInstrumentation::pseudoElementDestroyedImpl(InstrumentingAgents& instrumentingAgents, PseudoElement& pseudoElement)
{
    if (auto* domAgent = instrumentingAgents.persistentDOMAgent())
        domAgent->pseudoElementDestroyed(pseudoElement);
    if (auto* layerTreeAgent = instrumentingAgents.enabledLayerTreeAgent())
        layerTreeAgent->pseudoElementDestroyed(pseudoElement);
}

void InspectorInstrumentation::mouseDidMoveOverElementImpl(InstrumentingAgents& instrumentingAgents, const HitTestResult& result, OptionSet<PlatformEventModifier> modifiers)
{
    if (auto* domAgent = instrumentingAgents.persistentDOMAgent())
        domAgent->mouseDidMoveOverElement(result, modifiers);
}

void InspectorInstrumentation::didScrollImpl(InstrumentingAgents& instrumentingAgents)
{
    if (auto* pageAgent = instrumentingAgents.enabledPageAgent())
        pageAgent->didScroll();
}

bool InspectorInstrumentation::handleTouchEventImpl(InstrumentingAgents& instrumentingAgents, Node& node)
{
    if (auto* domAgent = instrumentingAgents.persistentDOMAgent())
        return domAgent->handleTouchEvent(node);
    return false;
}

bool InspectorInstrumentation::handleMousePressImpl(InstrumentingAgents& instrumentingAgents)
{
    if (auto* domAgent = instrumentingAgents.persistentDOMAgent())
        return domAgent->handleMousePress();
    return false;
}

bool InspectorInstrumentation::forcePseudoStateImpl(InstrumentingAgents& instrumentingAgents, const Element& element, CSSSelector::PseudoClass pseudoState)
{
    if (auto* cssAgent = instrumentingAgents.enabledCSSAgent())
        return cssAgent->forcePseudoState(element, pseudoState);
    return false;
}

void InspectorInstrumentation::characterDataModifiedImpl(InstrumentingAgents& instrumentingAgents, CharacterData& characterData)
{
    if (auto* domAgent = instrumentingAgents.persistentDOMAgent())
        domAgent->characterDataModified(characterData);
}

void InspectorInstrumentation::willSendXMLHttpRequestImpl(InstrumentingAgents& instrumentingAgents, const String& url)
{
    if (auto* domDebuggerAgent = instrumentingAgents.enabledDOMDebuggerAgent())
        domDebuggerAgent->willSendXMLHttpRequest(url);
}

void InspectorInstrumentation::willFetchImpl(InstrumentingAgents& instrumentingAgents, const String& url)
{
    if (auto* domDebuggerAgent = instrumentingAgents.enabledDOMDebuggerAgent())
        domDebuggerAgent->willFetch(url);
}

void InspectorInstrumentation::didInstallTimerImpl(InstrumentingAgents& instrumentingAgents, int timerId, Seconds timeout, bool singleShot, ScriptExecutionContext& context)
{
    if (auto* webDebuggerAgent = instrumentingAgents.enabledWebDebuggerAgent())
        webDebuggerAgent->didScheduleAsyncCall(context.globalObject(), InspectorDebuggerAgent::AsyncCallType::DOMTimer, timerId, singleShot);

    if (auto* timelineAgent = instrumentingAgents.trackingTimelineAgent())
        timelineAgent->didInstallTimer(timerId, timeout, singleShot, frameForScriptExecutionContext(context));
}

void InspectorInstrumentation::didRemoveTimerImpl(InstrumentingAgents& instrumentingAgents, int timerId, ScriptExecutionContext& context)
{
    if (auto* webDebuggerAgent = instrumentingAgents.enabledWebDebuggerAgent())
        webDebuggerAgent->didCancelAsyncCall(InspectorDebuggerAgent::AsyncCallType::DOMTimer, timerId);
    if (auto* timelineAgent = instrumentingAgents.trackingTimelineAgent())
        timelineAgent->didRemoveTimer(timerId, frameForScriptExecutionContext(context));
}

void InspectorInstrumentation::didAddEventListenerImpl(InstrumentingAgents& instrumentingAgents, EventTarget& target, const AtomString& eventType, EventListener& listener, bool capture)
{
    if (auto* webDebuggerAgent = instrumentingAgents.enabledWebDebuggerAgent())
        webDebuggerAgent->didAddEventListener(target, eventType, listener, capture);
    if (auto* domAgent = instrumentingAgents.persistentDOMAgent())
        domAgent->didAddEventListener(target);
    if (auto* cssAgent = instrumentingAgents.enabledCSSAgent())
        cssAgent->didAddEventListener(target);
}

void InspectorInstrumentation::willRemoveEventListenerImpl(InstrumentingAgents& instrumentingAgents, EventTarget& target, const AtomString& eventType, EventListener& listener, bool capture)
{
    if (auto* webDebuggerAgent = instrumentingAgents.enabledWebDebuggerAgent())
        webDebuggerAgent->willRemoveEventListener(target, eventType, listener, capture);
    if (auto* domAgent = instrumentingAgents.persistentDOMAgent())
        domAgent->willRemoveEventListener(target, eventType, listener, capture);
    if (auto* cssAgent = instrumentingAgents.enabledCSSAgent())
        cssAgent->willRemoveEventListener(target);
}

bool InspectorInstrumentation::isEventListenerDisabledImpl(InstrumentingAgents& instrumentingAgents, EventTarget& target, const AtomString& eventType, EventListener& listener, bool capture)
{
    if (auto* domAgent = instrumentingAgents.persistentDOMAgent())
        return domAgent->isEventListenerDisabled(target, eventType, listener, capture);
    return false;
}

int InspectorInstrumentation::willPostMessageImpl(InstrumentingAgents& instrumentingAgents)
{
    if (auto* webDebuggerAgent = instrumentingAgents.enabledWebDebuggerAgent())
        return webDebuggerAgent->willPostMessage();
    return 0;
}

void InspectorInstrumentation::didPostMessageImpl(InstrumentingAgents& instrumentingAgents, int postMessageIdentifier, JSC::JSGlobalObject& state)
{
    if (auto* webDebuggerAgent = instrumentingAgents.enabledWebDebuggerAgent())
        webDebuggerAgent->didPostMessage(postMessageIdentifier, state);
}

void InspectorInstrumentation::didFailPostMessageImpl(InstrumentingAgents& instrumentingAgents, int postMessageIdentifier)
{
    if (auto* webDebuggerAgent = instrumentingAgents.enabledWebDebuggerAgent())
        webDebuggerAgent->didFailPostMessage(postMessageIdentifier);
}

void InspectorInstrumentation::willDispatchPostMessageImpl(InstrumentingAgents& instrumentingAgents, int postMessageIdentifier)
{
    if (auto* webDebuggerAgent = instrumentingAgents.enabledWebDebuggerAgent())
        webDebuggerAgent->willDispatchPostMessage(postMessageIdentifier);
}

void InspectorInstrumentation::didDispatchPostMessageImpl(InstrumentingAgents& instrumentingAgents, int postMessageIdentifier)
{
    if (auto* webDebuggerAgent = instrumentingAgents.enabledWebDebuggerAgent())
        webDebuggerAgent->didDispatchPostMessage(postMessageIdentifier);
}

void InspectorInstrumentation::willCallFunctionImpl(InstrumentingAgents& instrumentingAgents, const String& scriptName, int scriptLine, int scriptColumn, ScriptExecutionContext* context)
{
    if (auto* timelineAgent = instrumentingAgents.trackingTimelineAgent())
        timelineAgent->willCallFunction(scriptName, scriptLine, scriptColumn, frameForScriptExecutionContext(context));
}

void InspectorInstrumentation::didCallFunctionImpl(InstrumentingAgents& instrumentingAgents, ScriptExecutionContext* context)
{
    if (auto* timelineAgent = instrumentingAgents.trackingTimelineAgent())
        timelineAgent->didCallFunction(frameForScriptExecutionContext(context));
}

void InspectorInstrumentation::willDispatchEventImpl(InstrumentingAgents& instrumentingAgents, Document& document, const Event& event)
{
    if (auto* timelineAgent = instrumentingAgents.trackingTimelineAgent())
        timelineAgent->willDispatchEvent(event, document.frame());
}

void InspectorInstrumentation::willHandleEventImpl(InstrumentingAgents& instrumentingAgents, ScriptExecutionContext& context, Event& event, const RegisteredEventListener& listener)
{
    if (auto* webDebuggerAgent = instrumentingAgents.enabledWebDebuggerAgent())
        webDebuggerAgent->willHandleEvent(listener);

    if (auto* domDebuggerAgent = instrumentingAgents.enabledDOMDebuggerAgent())
        domDebuggerAgent->willHandleEvent(context, event, listener);
}

void InspectorInstrumentation::didHandleEventImpl(InstrumentingAgents& instrumentingAgents, ScriptExecutionContext& context, Event& event, const RegisteredEventListener& listener)
{
    if (auto* webDebuggerAgent = instrumentingAgents.enabledWebDebuggerAgent())
        webDebuggerAgent->didHandleEvent(listener);

    if (auto* domDebuggerAgent = instrumentingAgents.enabledDOMDebuggerAgent())
        domDebuggerAgent->didHandleEvent(context, event, listener);
}

void InspectorInstrumentation::didDispatchEventImpl(InstrumentingAgents& instrumentingAgents, const Event& event)
{
    if (auto* timelineAgent = instrumentingAgents.trackingTimelineAgent())
        timelineAgent->didDispatchEvent(event.defaultPrevented());
}

void InspectorInstrumentation::willDispatchEventOnWindowImpl(InstrumentingAgents& instrumentingAgents, const Event& event, LocalDOMWindow& window)
{
    if (auto* timelineAgent = instrumentingAgents.trackingTimelineAgent())
        timelineAgent->willDispatchEvent(event, window.frame());
}

void InspectorInstrumentation::didDispatchEventOnWindowImpl(InstrumentingAgents& instrumentingAgents, const Event& event)
{
    if (auto* timelineAgent = instrumentingAgents.trackingTimelineAgent())
        timelineAgent->didDispatchEvent(event.defaultPrevented());
}

void InspectorInstrumentation::eventDidResetAfterDispatchImpl(InstrumentingAgents& instrumentingAgents, const Event& event)
{
    if (auto* domAgent = instrumentingAgents.persistentDOMAgent())
        domAgent->eventDidResetAfterDispatch(event);
}

void InspectorInstrumentation::willEvaluateScriptImpl(InstrumentingAgents& instrumentingAgents, LocalFrame& frame, const String& url, int lineNumber, int columnNumber)
{
    if (auto* timelineAgent = instrumentingAgents.trackingTimelineAgent())
        timelineAgent->willEvaluateScript(url, lineNumber, columnNumber, frame);
}

void InspectorInstrumentation::didEvaluateScriptImpl(InstrumentingAgents& instrumentingAgents, LocalFrame& frame)
{
    if (auto* timelineAgent = instrumentingAgents.trackingTimelineAgent())
        timelineAgent->didEvaluateScript(frame);
}

void InspectorInstrumentation::willFireTimerImpl(InstrumentingAgents& instrumentingAgents, int timerId, bool oneShot, ScriptExecutionContext& context)
{
    if (auto* webDebuggerAgent = instrumentingAgents.enabledWebDebuggerAgent())
        webDebuggerAgent->willDispatchAsyncCall(InspectorDebuggerAgent::AsyncCallType::DOMTimer, timerId);
    if (auto* domDebuggerAgent = instrumentingAgents.enabledDOMDebuggerAgent())
        domDebuggerAgent->willFireTimer(oneShot);
    if (auto* timelineAgent = instrumentingAgents.trackingTimelineAgent())
        timelineAgent->willFireTimer(timerId, frameForScriptExecutionContext(context));
}

void InspectorInstrumentation::didFireTimerImpl(InstrumentingAgents& instrumentingAgents, int timerId, bool oneShot)
{
    if (auto* webDebuggerAgent = instrumentingAgents.enabledWebDebuggerAgent())
        webDebuggerAgent->didDispatchAsyncCall(InspectorDebuggerAgent::AsyncCallType::DOMTimer, timerId);
    if (auto* domDebuggerAgent = instrumentingAgents.enabledDOMDebuggerAgent())
        domDebuggerAgent->didFireTimer(oneShot);
    if (auto* timelineAgent = instrumentingAgents.trackingTimelineAgent())
        timelineAgent->didFireTimer();
}

void InspectorInstrumentation::didInvalidateLayoutImpl(InstrumentingAgents& instrumentingAgents, LocalFrame& frame)
{
    if (auto* timelineAgent = instrumentingAgents.trackingTimelineAgent())
        timelineAgent->didInvalidateLayout(frame);
}

void InspectorInstrumentation::willLayoutImpl(InstrumentingAgents& instrumentingAgents, LocalFrame& frame)
{
    if (auto* timelineAgent = instrumentingAgents.trackingTimelineAgent())
        timelineAgent->willLayout(frame);
}

void InspectorInstrumentation::didLayoutImpl(InstrumentingAgents& instrumentingAgents, RenderObject& root)
{
    if (auto* timelineAgent = instrumentingAgents.trackingTimelineAgent())
        timelineAgent->didLayout(root);
    if (auto* pageAgent = instrumentingAgents.enabledPageAgent())
        pageAgent->didLayout();
}

void InspectorInstrumentation::willCompositeImpl(InstrumentingAgents& instrumentingAgents, LocalFrame& frame)
{
    if (auto* timelineAgent = instrumentingAgents.trackingTimelineAgent())
        timelineAgent->willComposite(frame);
}

void InspectorInstrumentation::didCompositeImpl(InstrumentingAgents& instrumentingAgents)
{
    if (auto* timelineAgent = instrumentingAgents.trackingTimelineAgent())
        timelineAgent->didComposite();
}

void InspectorInstrumentation::willPaintImpl(InstrumentingAgents& instrumentingAgents, RenderObject& renderer)
{
    if (auto* timelineAgent = instrumentingAgents.trackingTimelineAgent())
        timelineAgent->willPaint(renderer.frame());
}

void InspectorInstrumentation::didPaintImpl(InstrumentingAgents& instrumentingAgents, RenderObject& renderer, const LayoutRect& rect)
{
    if (auto* timelineAgent = instrumentingAgents.trackingTimelineAgent())
        timelineAgent->didPaint(renderer, rect);

    if (auto* pageAgent = instrumentingAgents.enabledPageAgent())
        pageAgent->didPaint(renderer, rect);
}

void InspectorInstrumentation::willRecalculateStyleImpl(InstrumentingAgents& instrumentingAgents, Document& document)
{
    if (auto* timelineAgent = instrumentingAgents.trackingTimelineAgent())
        timelineAgent->willRecalculateStyle(document.frame());
    if (auto* networkAgent = instrumentingAgents.enabledNetworkAgent())
        networkAgent->willRecalculateStyle();
}

void InspectorInstrumentation::didRecalculateStyleImpl(InstrumentingAgents& instrumentingAgents)
{
    if (auto* timelineAgent = instrumentingAgents.trackingTimelineAgent())
        timelineAgent->didRecalculateStyle();
    if (auto* networkAgent = instrumentingAgents.enabledNetworkAgent())
        networkAgent->didRecalculateStyle();
    if (auto* pageAgent = instrumentingAgents.enabledPageAgent())
        pageAgent->didRecalculateStyle();
}

void InspectorInstrumentation::didScheduleStyleRecalculationImpl(InstrumentingAgents& instrumentingAgents, Document& document)
{
    if (auto* timelineAgent = instrumentingAgents.trackingTimelineAgent())
        timelineAgent->didScheduleStyleRecalculation(document.frame());
    if (auto* networkAgent = instrumentingAgents.enabledNetworkAgent())
        networkAgent->didScheduleStyleRecalculation(document);
}

void InspectorInstrumentation::applyUserAgentOverrideImpl(InstrumentingAgents& instrumentingAgents, String& userAgent)
{
    if (auto* pageAgent = instrumentingAgents.enabledPageAgent())
        pageAgent->applyUserAgentOverride(userAgent);
}

void InspectorInstrumentation::applyEmulatedMediaImpl(InstrumentingAgents& instrumentingAgents, AtomString& media)
{
    if (auto* pageAgent = instrumentingAgents.enabledPageAgent())
        pageAgent->applyEmulatedMedia(media);
}

void InspectorInstrumentation::flexibleBoxRendererBeganLayoutImpl(InstrumentingAgents& instrumentingAgents, const RenderObject& renderer)
{
    if (auto* domAgent = instrumentingAgents.persistentDOMAgent())
        domAgent->flexibleBoxRendererBeganLayout(renderer);
}

void InspectorInstrumentation::flexibleBoxRendererWrappedToNextLineImpl(InstrumentingAgents& instrumentingAgents, const RenderObject& renderer, size_t lineStartItemIndex)
{
    if (auto* domAgent = instrumentingAgents.persistentDOMAgent())
        domAgent->flexibleBoxRendererWrappedToNextLine(renderer, lineStartItemIndex);
}

void InspectorInstrumentation::willSendRequestImpl(InstrumentingAgents& instrumentingAgents, ResourceLoaderIdentifier identifier, DocumentLoader* loader, ResourceRequest& request, const ResourceResponse& redirectResponse, const CachedResource* cachedResource, ResourceLoader* resourceLoader)
{
    if (auto* networkAgent = instrumentingAgents.enabledNetworkAgent())
        networkAgent->willSendRequest(identifier, loader, request, redirectResponse, cachedResource, resourceLoader);
    if (auto* domDebuggerAgent = instrumentingAgents.enabledDOMDebuggerAgent())
        domDebuggerAgent->willSendRequest(request);
}

void InspectorInstrumentation::willSendRequestOfTypeImpl(InstrumentingAgents& instrumentingAgents, ResourceLoaderIdentifier identifier, DocumentLoader* loader, ResourceRequest& request, LoadType loadType)
{
    if (auto* networkAgent = instrumentingAgents.enabledNetworkAgent())
        networkAgent->willSendRequestOfType(identifier, loader, request, loadType);
    if (auto* domDebuggerAgent = instrumentingAgents.enabledDOMDebuggerAgent())
        domDebuggerAgent->willSendRequestOfType(request);
}

void InspectorInstrumentation::didLoadResourceFromMemoryCacheImpl(InstrumentingAgents& instrumentingAgents, DocumentLoader* loader, CachedResource* cachedResource)
{
    if (!loader || !cachedResource)
        return;

    if (auto* networkAgent = instrumentingAgents.enabledNetworkAgent())
        networkAgent->didLoadResourceFromMemoryCache(loader, *cachedResource);
}

void InspectorInstrumentation::didReceiveResourceResponseImpl(InstrumentingAgents& instrumentingAgents, ResourceLoaderIdentifier identifier, DocumentLoader* loader, const ResourceResponse& response, ResourceLoader* resourceLoader)
{
    if (LIKELY(!instrumentingAgents.inspectorEnvironment().developerExtrasEnabled()))
        return;

    if (auto* networkAgent = instrumentingAgents.enabledNetworkAgent())
        networkAgent->didReceiveResponse(identifier, loader, response, resourceLoader);
    if (auto* consoleAgent = instrumentingAgents.webConsoleAgent())
        consoleAgent->didReceiveResponse(identifier, response); // This should come AFTER resource notification, front-end relies on this.
}

void InspectorInstrumentation::didReceiveThreadableLoaderResponseImpl(InstrumentingAgents& instrumentingAgents, DocumentThreadableLoader& documentThreadableLoader, ResourceLoaderIdentifier identifier)
{
    if (auto* networkAgent = instrumentingAgents.enabledNetworkAgent())
        networkAgent->didReceiveThreadableLoaderResponse(identifier, documentThreadableLoader);
}

void InspectorInstrumentation::didReceiveDataImpl(InstrumentingAgents& instrumentingAgents, ResourceLoaderIdentifier identifier, const SharedBuffer* buffer, int encodedDataLength)
{
    if (auto* networkAgent = instrumentingAgents.enabledNetworkAgent())
        networkAgent->didReceiveData(identifier, buffer, buffer ? buffer->size() : 0, encodedDataLength);
}

void InspectorInstrumentation::didFinishLoadingImpl(InstrumentingAgents& instrumentingAgents, ResourceLoaderIdentifier identifier, DocumentLoader* loader, const NetworkLoadMetrics& networkLoadMetrics, ResourceLoader* resourceLoader)
{
    if (auto* networkAgent = instrumentingAgents.enabledNetworkAgent())
        networkAgent->didFinishLoading(identifier, loader, networkLoadMetrics, resourceLoader);
}

void InspectorInstrumentation::didFailLoadingImpl(InstrumentingAgents& instrumentingAgents, ResourceLoaderIdentifier identifier, DocumentLoader* loader, const ResourceError& error)
{
    if (LIKELY(!instrumentingAgents.inspectorEnvironment().developerExtrasEnabled()))
        return;

    if (auto* networkAgent = instrumentingAgents.enabledNetworkAgent())
        networkAgent->didFailLoading(identifier, loader, error);
    if (auto* consoleAgent = instrumentingAgents.webConsoleAgent())
        consoleAgent->didFailLoading(identifier, error); // This should come AFTER resource notification, front-end relies on this.
}

void InspectorInstrumentation::willLoadXHRSynchronouslyImpl(InstrumentingAgents& instrumentingAgents)
{
    if (auto* networkAgent = instrumentingAgents.enabledNetworkAgent())
        networkAgent->willLoadXHRSynchronously();
}

void InspectorInstrumentation::didLoadXHRSynchronouslyImpl(InstrumentingAgents& instrumentingAgents)
{
    if (auto* networkAgent = instrumentingAgents.enabledNetworkAgent())
        networkAgent->didLoadXHRSynchronously();
}

void InspectorInstrumentation::scriptImportedImpl(InstrumentingAgents& instrumentingAgents, ResourceLoaderIdentifier identifier, const String& sourceString)
{
    if (auto* networkAgent = instrumentingAgents.enabledNetworkAgent())
        networkAgent->setInitialScriptContent(identifier, sourceString);
}

void InspectorInstrumentation::scriptExecutionBlockedByCSPImpl(InstrumentingAgents& instrumentingAgents, const String& directiveText)
{
    if (auto* webDebuggerAgent = instrumentingAgents.enabledWebDebuggerAgent())
        webDebuggerAgent->scriptExecutionBlockedByCSP(directiveText);
}

void InspectorInstrumentation::didReceiveScriptResponseImpl(InstrumentingAgents& instrumentingAgents, ResourceLoaderIdentifier identifier)
{
    if (auto* networkAgent = instrumentingAgents.enabledNetworkAgent())
        networkAgent->didReceiveScriptResponse(identifier);
}

void InspectorInstrumentation::domContentLoadedEventFiredImpl(InstrumentingAgents& instrumentingAgents, LocalFrame& frame)
{
    if (!frame.isMainFrame())
        return;

    if (auto* pageAgent = instrumentingAgents.enabledPageAgent())
        pageAgent->domContentEventFired();
}

void InspectorInstrumentation::loadEventFiredImpl(InstrumentingAgents& instrumentingAgents, LocalFrame* frame)
{
    if (!frame || !frame->isMainFrame())
        return;

    if (auto* pageAgent = instrumentingAgents.enabledPageAgent())
        pageAgent->loadEventFired();
}

void InspectorInstrumentation::frameDetachedFromParentImpl(InstrumentingAgents& instrumentingAgents, LocalFrame& frame)
{
    if (auto* pageAgent = instrumentingAgents.enabledPageAgent())
        pageAgent->frameDetached(frame);
}

void InspectorInstrumentation::didCommitLoadImpl(InstrumentingAgents& instrumentingAgents, LocalFrame& frame, DocumentLoader* loader)
{
    if (LIKELY(!instrumentingAgents.inspectorEnvironment().developerExtrasEnabled()))
        return;

    if (!frame.page())
        return;

    if (!loader)
        return;

    ASSERT(loader->frame() == &frame);

    if (frame.isMainFrame()) {
        if (auto* networkAgent = instrumentingAgents.enabledNetworkAgent())
            networkAgent->mainFrameNavigated(*loader);

        // The Web Inspector frontend relies on `networkAgent->mainFrameNavigated` being called first to establish the
        // type of navigation that has occured.
        if (auto* consoleAgent = instrumentingAgents.webConsoleAgent())
            consoleAgent->mainFrameNavigated();

        if (auto* cssAgent = instrumentingAgents.enabledCSSAgent())
            cssAgent->reset();

        if (auto* databaseAgent = instrumentingAgents.enabledDatabaseAgent())
            databaseAgent->didCommitLoad();

        if (auto* domAgent = instrumentingAgents.persistentDOMAgent())
            domAgent->setDocument(frame.document());

        if (auto* layerTreeAgent = instrumentingAgents.enabledLayerTreeAgent())
            layerTreeAgent->reset();

        if (auto* pageDebuggerAgent = instrumentingAgents.enabledPageDebuggerAgent())
            pageDebuggerAgent->mainFrameNavigated();

        if (auto* domDebuggerAgent = instrumentingAgents.enabledDOMDebuggerAgent())
            domDebuggerAgent->mainFrameNavigated();

        if (auto* enabledPageHeapAgent = instrumentingAgents.enabledPageHeapAgent())
            enabledPageHeapAgent->mainFrameNavigated();
    }

    if (auto* pageAgent = instrumentingAgents.enabledPageAgent())
        pageAgent->frameNavigated(frame);

    if (auto* pageRuntimeAgent = instrumentingAgents.enabledPageRuntimeAgent())
        pageRuntimeAgent->frameNavigated(frame);

    if (auto* pageCanvasAgent = instrumentingAgents.enabledPageCanvasAgent())
        pageCanvasAgent->frameNavigated(frame);

    if (auto* animationAgent = instrumentingAgents.enabledAnimationAgent())
        animationAgent->frameNavigated(frame);

    if (auto* domAgent = instrumentingAgents.persistentDOMAgent())
        domAgent->didCommitLoad(frame.document());

    if (frame.isMainFrame()) {
        if (auto* timelineAgent = instrumentingAgents.trackingTimelineAgent())
            timelineAgent->mainFrameNavigated();
    }
}

void InspectorInstrumentation::frameDocumentUpdatedImpl(InstrumentingAgents& instrumentingAgents, LocalFrame& frame)
{
    if (auto* domAgent = instrumentingAgents.persistentDOMAgent())
        domAgent->frameDocumentUpdated(frame);

    if (auto* pageDOMDebuggerAgent = instrumentingAgents.enabledPageDOMDebuggerAgent())
        pageDOMDebuggerAgent->frameDocumentUpdated(frame);
}

void InspectorInstrumentation::loaderDetachedFromFrameImpl(InstrumentingAgents& instrumentingAgents, DocumentLoader& loader)
{
    if (auto* inspectorPageAgent = instrumentingAgents.enabledPageAgent())
        inspectorPageAgent->loaderDetachedFromFrame(loader);
}

void InspectorInstrumentation::frameStartedLoadingImpl(InstrumentingAgents& instrumentingAgents, LocalFrame& frame)
{
    if (frame.isMainFrame()) {
        if (auto* pageDebuggerAgent = instrumentingAgents.enabledPageDebuggerAgent())
            pageDebuggerAgent->mainFrameStartedLoading();
        if (auto* timelineAgent = instrumentingAgents.enabledTimelineAgent())
            timelineAgent->mainFrameStartedLoading();
    }

    if (auto* inspectorPageAgent = instrumentingAgents.enabledPageAgent())
        inspectorPageAgent->frameStartedLoading(frame);
}

void InspectorInstrumentation::didCompleteRenderingFrameImpl(InstrumentingAgents& instrumentingAgents)
{
    if (auto* timelineAgent = instrumentingAgents.enabledTimelineAgent())
        timelineAgent->didCompleteRenderingFrame();
}

void InspectorInstrumentation::frameStoppedLoadingImpl(InstrumentingAgents& instrumentingAgents, LocalFrame& frame)
{
    if (frame.isMainFrame()) {
        if (auto* pageDebuggerAgent = instrumentingAgents.enabledPageDebuggerAgent())
            pageDebuggerAgent->mainFrameStoppedLoading();
    }

    if (auto* inspectorPageAgent = instrumentingAgents.enabledPageAgent())
        inspectorPageAgent->frameStoppedLoading(frame);
}

void InspectorInstrumentation::frameScheduledNavigationImpl(InstrumentingAgents& instrumentingAgents, Frame& frame, Seconds delay)
{
    if (auto* inspectorPageAgent = instrumentingAgents.enabledPageAgent())
        inspectorPageAgent->frameScheduledNavigation(frame, delay);
}

void InspectorInstrumentation::frameClearedScheduledNavigationImpl(InstrumentingAgents& instrumentingAgents, Frame& frame)
{
    if (auto* inspectorPageAgent = instrumentingAgents.enabledPageAgent())
        inspectorPageAgent->frameClearedScheduledNavigation(frame);
}

void InspectorInstrumentation::accessibilitySettingsDidChangeImpl(InstrumentingAgents& instrumentingAgents)
{
    if (auto* inspectorPageAgent = instrumentingAgents.enabledPageAgent())
        inspectorPageAgent->accessibilitySettingsDidChange();
}

#if ENABLE(DARK_MODE_CSS) || HAVE(OS_DARK_MODE_SUPPORT)
void InspectorInstrumentation::defaultAppearanceDidChangeImpl(InstrumentingAgents& instrumentingAgents)
{
    if (auto* inspectorPageAgent = instrumentingAgents.enabledPageAgent())
        inspectorPageAgent->defaultAppearanceDidChange();
}
#endif

void InspectorInstrumentation::willDestroyCachedResourceImpl(CachedResource& cachedResource)
{
    if (!s_instrumentingAgentsSet)
        return;

    for (auto* instrumentingAgent : *s_instrumentingAgentsSet) {
        if (auto* inspectorNetworkAgent = instrumentingAgent->enabledNetworkAgent())
            inspectorNetworkAgent->willDestroyCachedResource(cachedResource);
    }
}

bool InspectorInstrumentation::willInterceptImpl(InstrumentingAgents& instrumentingAgents, const ResourceRequest& request)
{
    if (auto* networkAgent = instrumentingAgents.enabledNetworkAgent())
        return networkAgent->willIntercept(request);
    return false;
}

bool InspectorInstrumentation::shouldInterceptRequestImpl(InstrumentingAgents& instrumentingAgents, const ResourceLoader& loader)
{
    if (auto* networkAgent = instrumentingAgents.enabledNetworkAgent())
        return networkAgent->shouldInterceptRequest(loader);
    return false;
}

bool InspectorInstrumentation::shouldInterceptResponseImpl(InstrumentingAgents& instrumentingAgents, const ResourceResponse& response)
{
    if (auto* networkAgent = instrumentingAgents.enabledNetworkAgent())
        return networkAgent->shouldInterceptResponse(response);
    return false;
}

void InspectorInstrumentation::interceptRequestImpl(InstrumentingAgents& instrumentingAgents, ResourceLoader& loader, Function<void(const ResourceRequest&)>&& handler)
{
    if (auto* networkAgent = instrumentingAgents.enabledNetworkAgent())
        networkAgent->interceptRequest(loader, WTFMove(handler));
}

void InspectorInstrumentation::interceptResponseImpl(InstrumentingAgents& instrumentingAgents, const ResourceResponse& response, ResourceLoaderIdentifier identifier, CompletionHandler<void(const ResourceResponse&, RefPtr<FragmentedSharedBuffer>)>&& handler)
{
    if (auto* networkAgent = instrumentingAgents.enabledNetworkAgent())
        networkAgent->interceptResponse(response, identifier, WTFMove(handler));
}

// JavaScriptCore InspectorDebuggerAgent should know Console MessageTypes.
static bool isConsoleAssertMessage(MessageSource source, MessageType type)
{
    return source == MessageSource::ConsoleAPI && type == MessageType::Assert;
}

void InspectorInstrumentation::addMessageToConsoleImpl(InstrumentingAgents& instrumentingAgents, std::unique_ptr<ConsoleMessage> message)
{
    if (LIKELY(!instrumentingAgents.inspectorEnvironment().developerExtrasEnabled()))
        return;

    MessageSource source = message->source();
    MessageType type = message->type();
    String messageText = message->message();

    if (auto* consoleAgent = instrumentingAgents.webConsoleAgent())
        consoleAgent->addMessageToConsole(WTFMove(message));
    // FIXME: This should just pass the message on to the debugger agent. JavaScriptCore InspectorDebuggerAgent should know Console MessageTypes.
    if (auto* webDebuggerAgent = instrumentingAgents.enabledWebDebuggerAgent()) {
        if (isConsoleAssertMessage(source, type))
            webDebuggerAgent->handleConsoleAssert(messageText);
    }
}

void InspectorInstrumentation::consoleCountImpl(InstrumentingAgents& instrumentingAgents, JSC::JSGlobalObject* state, const String& label)
{
    if (LIKELY(!instrumentingAgents.inspectorEnvironment().developerExtrasEnabled()))
        return;

    if (auto* consoleAgent = instrumentingAgents.webConsoleAgent())
        consoleAgent->count(state, label);
}

void InspectorInstrumentation::consoleCountResetImpl(InstrumentingAgents& instrumentingAgents, JSC::JSGlobalObject* state, const String& label)
{
    if (LIKELY(!instrumentingAgents.inspectorEnvironment().developerExtrasEnabled()))
        return;

    if (auto* consoleAgent = instrumentingAgents.webConsoleAgent())
        consoleAgent->countReset(state, label);
}

void InspectorInstrumentation::takeHeapSnapshotImpl(InstrumentingAgents& instrumentingAgents, const String& title)
{
    if (auto* consoleAgent = instrumentingAgents.webConsoleAgent())
        consoleAgent->takeHeapSnapshot(title);
}

void InspectorInstrumentation::startConsoleTimingImpl(InstrumentingAgents& instrumentingAgents, LocalFrame& frame, JSC::JSGlobalObject* exec, const String& label)
{
    if (LIKELY(!instrumentingAgents.inspectorEnvironment().developerExtrasEnabled()))
        return;

    if (auto* timelineAgent = instrumentingAgents.trackingTimelineAgent())
        timelineAgent->time(frame, label);
    if (auto* consoleAgent = instrumentingAgents.webConsoleAgent())
        consoleAgent->startTiming(exec, label);
}

void InspectorInstrumentation::startConsoleTimingImpl(InstrumentingAgents& instrumentingAgents, JSC::JSGlobalObject* exec, const String& label)
{
    if (LIKELY(!instrumentingAgents.inspectorEnvironment().developerExtrasEnabled()))
        return;

    if (auto* consoleAgent = instrumentingAgents.webConsoleAgent())
        consoleAgent->startTiming(exec, label);
}

void InspectorInstrumentation::logConsoleTimingImpl(InstrumentingAgents& instrumentingAgents, JSC::JSGlobalObject* exec, const String& label, Ref<Inspector::ScriptArguments>&& arguments)
{
    if (LIKELY(!instrumentingAgents.inspectorEnvironment().developerExtrasEnabled()))
        return;

    if (auto* consoleAgent = instrumentingAgents.webConsoleAgent())
        consoleAgent->logTiming(exec, label, WTFMove(arguments));
}

void InspectorInstrumentation::stopConsoleTimingImpl(InstrumentingAgents& instrumentingAgents, LocalFrame& frame, JSC::JSGlobalObject* exec, const String& label)
{
    if (LIKELY(!instrumentingAgents.inspectorEnvironment().developerExtrasEnabled()))
        return;

    if (auto* consoleAgent = instrumentingAgents.webConsoleAgent())
        consoleAgent->stopTiming(exec, label);
    if (auto* timelineAgent = instrumentingAgents.trackingTimelineAgent())
        timelineAgent->timeEnd(frame, label);
}

void InspectorInstrumentation::stopConsoleTimingImpl(InstrumentingAgents& instrumentingAgents, JSC::JSGlobalObject* exec, const String& label)
{
    if (LIKELY(!instrumentingAgents.inspectorEnvironment().developerExtrasEnabled()))
        return;

    if (auto* consoleAgent = instrumentingAgents.webConsoleAgent())
        consoleAgent->stopTiming(exec, label);
}

void InspectorInstrumentation::consoleTimeStampImpl(InstrumentingAgents& instrumentingAgents, LocalFrame& frame, Ref<ScriptArguments>&& arguments)
{
    if (auto* timelineAgent = instrumentingAgents.trackingTimelineAgent()) {
        String message;
        arguments->getFirstArgumentAsString(message);
        timelineAgent->didTimeStamp(frame, message);
     }
}

void InspectorInstrumentation::startProfilingImpl(InstrumentingAgents& instrumentingAgents, JSC::JSGlobalObject* exec, const String& title)
{
    if (auto* timelineAgent = instrumentingAgents.enabledTimelineAgent())
        timelineAgent->startFromConsole(exec, title);
}

void InspectorInstrumentation::stopProfilingImpl(InstrumentingAgents& instrumentingAgents, JSC::JSGlobalObject* exec, const String& title)
{
    if (auto* timelineAgent = instrumentingAgents.enabledTimelineAgent())
        timelineAgent->stopFromConsole(exec, title);
}

void InspectorInstrumentation::performanceMarkImpl(InstrumentingAgents& instrumentingAgents, const String& label, std::optional<MonotonicTime> timestamp, LocalFrame* frame)
{
    if (auto* timelineAgent = instrumentingAgents.trackingTimelineAgent())
        timelineAgent->didPerformanceMark(label, timestamp, frame);
}

void InspectorInstrumentation::consoleStartRecordingCanvasImpl(InstrumentingAgents& instrumentingAgents, CanvasRenderingContext& context, JSC::JSGlobalObject& exec, JSC::JSObject* options)
{
    if (auto* canvasAgent = instrumentingAgents.enabledCanvasAgent())
        canvasAgent->consoleStartRecordingCanvas(context, exec, options);
}

void InspectorInstrumentation::consoleStopRecordingCanvasImpl(InstrumentingAgents& instrumentingAgents, CanvasRenderingContext& context)
{
    if (auto* canvasAgent = instrumentingAgents.enabledCanvasAgent())
        canvasAgent->consoleStopRecordingCanvas(context);
}

void InspectorInstrumentation::didOpenDatabaseImpl(InstrumentingAgents& instrumentingAgents, Database& database)
{
    if (auto* databaseAgent = instrumentingAgents.enabledDatabaseAgent())
        databaseAgent->didOpenDatabase(database);
}

void InspectorInstrumentation::didDispatchDOMStorageEventImpl(InstrumentingAgents& instrumentingAgents, const String& key, const String& oldValue, const String& newValue, StorageType storageType, const SecurityOrigin& securityOrigin)
{
    if (auto* domStorageAgent = instrumentingAgents.enabledDOMStorageAgent())
        domStorageAgent->didDispatchDOMStorageEvent(key, oldValue, newValue, storageType, securityOrigin);
}

bool InspectorInstrumentation::shouldWaitForDebuggerOnStartImpl(InstrumentingAgents& instrumentingAgents)
{
    if (auto* workerAgent = instrumentingAgents.persistentWorkerAgent())
        return workerAgent->shouldWaitForDebuggerOnStart();
    return false;
}

void InspectorInstrumentation::workerStartedImpl(InstrumentingAgents& instrumentingAgents, WorkerInspectorProxy& proxy)
{
    if (auto* workerAgent = instrumentingAgents.persistentWorkerAgent())
        workerAgent->workerStarted(proxy);
}

void InspectorInstrumentation::workerTerminatedImpl(InstrumentingAgents& instrumentingAgents, WorkerInspectorProxy& proxy)
{
    if (auto* workerAgent = instrumentingAgents.persistentWorkerAgent())
        workerAgent->workerTerminated(proxy);
}

void InspectorInstrumentation::didCreateWebSocketImpl(InstrumentingAgents& instrumentingAgents, WebSocketChannelIdentifier identifier, const URL& requestURL)
{
    if (auto* networkAgent = instrumentingAgents.enabledNetworkAgent())
        networkAgent->didCreateWebSocket(identifier, requestURL);
}

void InspectorInstrumentation::willSendWebSocketHandshakeRequestImpl(InstrumentingAgents& instrumentingAgents, WebSocketChannelIdentifier identifier, const ResourceRequest& request)
{
    if (auto* networkAgent = instrumentingAgents.enabledNetworkAgent())
        networkAgent->willSendWebSocketHandshakeRequest(identifier, request);
}

void InspectorInstrumentation::didReceiveWebSocketHandshakeResponseImpl(InstrumentingAgents& instrumentingAgents, WebSocketChannelIdentifier identifier, const ResourceResponse& response)
{
    if (auto* networkAgent = instrumentingAgents.enabledNetworkAgent())
        networkAgent->didReceiveWebSocketHandshakeResponse(identifier, response);
}

void InspectorInstrumentation::didCloseWebSocketImpl(InstrumentingAgents& instrumentingAgents, WebSocketChannelIdentifier identifier)
{
    if (auto* networkAgent = instrumentingAgents.enabledNetworkAgent())
        networkAgent->didCloseWebSocket(identifier);
}

void InspectorInstrumentation::didReceiveWebSocketFrameImpl(InstrumentingAgents& instrumentingAgents, WebSocketChannelIdentifier identifier, const WebSocketFrame& frame)
{
    if (auto* networkAgent = instrumentingAgents.enabledNetworkAgent())
        networkAgent->didReceiveWebSocketFrame(identifier, frame);
}

void InspectorInstrumentation::didReceiveWebSocketFrameErrorImpl(InstrumentingAgents& instrumentingAgents, WebSocketChannelIdentifier identifier, const String& errorMessage)
{
    if (auto* networkAgent = instrumentingAgents.enabledNetworkAgent())
        networkAgent->didReceiveWebSocketFrameError(identifier, errorMessage);
}

void InspectorInstrumentation::didSendWebSocketFrameImpl(InstrumentingAgents& instrumentingAgents, WebSocketChannelIdentifier identifier, const WebSocketFrame& frame)
{
    if (auto* networkAgent = instrumentingAgents.enabledNetworkAgent())
        networkAgent->didSendWebSocketFrame(identifier, frame);
}

void InspectorInstrumentation::didChangeCSSCanvasClientNodesImpl(InstrumentingAgents& instrumentingAgents, CanvasBase& canvasBase)
{
    if (auto* pageCanvasAgent = instrumentingAgents.enabledPageCanvasAgent())
        pageCanvasAgent->didChangeCSSCanvasClientNodes(canvasBase);
}

void InspectorInstrumentation::didCreateCanvasRenderingContextImpl(InstrumentingAgents& instrumentingAgents, CanvasRenderingContext& context)
{
    if (auto* canvasAgent = instrumentingAgents.enabledCanvasAgent())
        canvasAgent->didCreateCanvasRenderingContext(context);
}

void InspectorInstrumentation::didChangeCanvasSizeImpl(InstrumentingAgents& instrumentingAgents, CanvasRenderingContext& context)
{
    if (auto* canvasAgent = instrumentingAgents.enabledCanvasAgent())
        canvasAgent->didChangeCanvasSize(context);
}

void InspectorInstrumentation::didChangeCanvasMemoryImpl(InstrumentingAgents& instrumentingAgents, CanvasRenderingContext& context)
{
    if (auto* canvasAgent = instrumentingAgents.enabledCanvasAgent())
        canvasAgent->didChangeCanvasMemory(context);
}

void InspectorInstrumentation::didFinishRecordingCanvasFrameImpl(InstrumentingAgents& instrumentingAgents, CanvasRenderingContext& context, bool forceDispatch)
{
    if (auto* canvasAgent = instrumentingAgents.enabledCanvasAgent())
        canvasAgent->didFinishRecordingCanvasFrame(context, forceDispatch);
}

#if ENABLE(WEBGL)
void InspectorInstrumentation::didEnableExtensionImpl(InstrumentingAgents& instrumentingAgents, WebGLRenderingContextBase& contextWebGLBase, const String& extension)
{
    if (auto* canvasAgent = instrumentingAgents.enabledCanvasAgent())
        canvasAgent->didEnableExtension(contextWebGLBase, extension);
}

void InspectorInstrumentation::didCreateWebGLProgramImpl(InstrumentingAgents& instrumentingAgents, WebGLRenderingContextBase& contextWebGLBase, WebGLProgram& program)
{
    if (auto* canvasAgent = instrumentingAgents.enabledCanvasAgent())
        canvasAgent->didCreateWebGLProgram(contextWebGLBase, program);
}

void InspectorInstrumentation::willDestroyWebGLProgramImpl(InstrumentingAgents& instrumentingAgents, WebGLProgram& program)
{
    if (auto* canvasAgent = instrumentingAgents.enabledCanvasAgent())
        canvasAgent->willDestroyWebGLProgram(program);
}

bool InspectorInstrumentation::isWebGLProgramDisabledImpl(InstrumentingAgents& instrumentingAgents, WebGLProgram& program)
{
    if (auto* canvasAgent = instrumentingAgents.enabledCanvasAgent())
        return canvasAgent->isWebGLProgramDisabled(program);
    return false;
}

bool InspectorInstrumentation::isWebGLProgramHighlightedImpl(InstrumentingAgents& instrumentingAgents, WebGLProgram& program)
{
    if (auto* canvasAgent = instrumentingAgents.enabledCanvasAgent())
        return canvasAgent->isWebGLProgramHighlighted(program);
    return false;
}
#endif

void InspectorInstrumentation::willApplyKeyframeEffectImpl(InstrumentingAgents& instrumentingAgents, const Styleable& target, KeyframeEffect& effect, const ComputedEffectTiming& computedTiming)
{
    if (auto* animationAgent = instrumentingAgents.trackingAnimationAgent())
        animationAgent->willApplyKeyframeEffect(target, effect, computedTiming);
}

void InspectorInstrumentation::didChangeWebAnimationNameImpl(InstrumentingAgents& instrumentingAgents, WebAnimation& animation)
{
    if (auto* animationAgent = instrumentingAgents.enabledAnimationAgent())
        animationAgent->didChangeWebAnimationName(animation);
}

void InspectorInstrumentation::didSetWebAnimationEffectImpl(InstrumentingAgents& instrumentingAgents, WebAnimation& animation)
{
    if (auto* animationAgent = instrumentingAgents.enabledAnimationAgent())
        animationAgent->didSetWebAnimationEffect(animation);
    else if (auto* animationAgent = instrumentingAgents.trackingAnimationAgent())
        animationAgent->didSetWebAnimationEffect(animation);
}

void InspectorInstrumentation::didChangeWebAnimationEffectTimingImpl(InstrumentingAgents& instrumentingAgents, WebAnimation& animation)
{
    if (auto* animationAgent = instrumentingAgents.enabledAnimationAgent())
        animationAgent->didChangeWebAnimationEffectTiming(animation);
}

void InspectorInstrumentation::didChangeWebAnimationEffectTargetImpl(InstrumentingAgents& instrumentingAgents, WebAnimation& animation)
{
    if (auto* animationAgent = instrumentingAgents.enabledAnimationAgent())
        animationAgent->didChangeWebAnimationEffectTarget(animation);
}

void InspectorInstrumentation::didCreateWebAnimationImpl(InstrumentingAgents& instrumentingAgents, WebAnimation& animation)
{
    if (auto* animationAgent = instrumentingAgents.enabledAnimationAgent())
        animationAgent->didCreateWebAnimation(animation);
}

void InspectorInstrumentation::willDestroyWebAnimationImpl(InstrumentingAgents& instrumentingAgents, WebAnimation& animation)
{
    if (auto* animationAgent = instrumentingAgents.enabledAnimationAgent())
        animationAgent->willDestroyWebAnimation(animation);
    else if (auto* animationAgent = instrumentingAgents.trackingAnimationAgent())
        animationAgent->willDestroyWebAnimation(animation);
}

#if ENABLE(RESOURCE_USAGE)
void InspectorInstrumentation::didHandleMemoryPressureImpl(InstrumentingAgents& instrumentingAgents, Critical critical)
{
    if (auto* memoryAgent = instrumentingAgents.enabledMemoryAgent())
        memoryAgent->didHandleMemoryPressure(critical);
}
#endif

void InspectorInstrumentation::networkStateChangedImpl(InstrumentingAgents& instrumentingAgents)
{
    if (auto* applicationCacheAgent = instrumentingAgents.enabledApplicationCacheAgent())
        applicationCacheAgent->networkStateChanged();
}

void InspectorInstrumentation::updateApplicationCacheStatusImpl(InstrumentingAgents& instrumentingAgents, LocalFrame& frame)
{
    if (auto* applicationCacheAgent = instrumentingAgents.enabledApplicationCacheAgent())
        applicationCacheAgent->updateApplicationCacheStatus(&frame);
}

bool InspectorInstrumentation::consoleAgentEnabled(ScriptExecutionContext* scriptExecutionContext)
{
    FAST_RETURN_IF_NO_FRONTENDS(false);
    if (auto* agents = instrumentingAgents(scriptExecutionContext)) {
        if (auto* webConsoleAgent = agents->webConsoleAgent())
            return webConsoleAgent->enabled();
    }
    return false;
}

bool InspectorInstrumentation::timelineAgentTracking(ScriptExecutionContext* scriptExecutionContext)
{
    FAST_RETURN_IF_NO_FRONTENDS(false);
    if (auto* agents = instrumentingAgents(scriptExecutionContext))
        return agents->trackingTimelineAgent();
    return false;
}

void InspectorInstrumentation::didRequestAnimationFrameImpl(InstrumentingAgents& instrumentingAgents, int callbackId, Document& document)
{
    if (auto* pageDebuggerAgent = instrumentingAgents.enabledPageDebuggerAgent())
        pageDebuggerAgent->didRequestAnimationFrame(callbackId, document);
    if (auto* timelineAgent = instrumentingAgents.trackingTimelineAgent())
        timelineAgent->didRequestAnimationFrame(callbackId, document.frame());
}

void InspectorInstrumentation::didCancelAnimationFrameImpl(InstrumentingAgents& instrumentingAgents, int callbackId, Document& document)
{
    if (auto* pageDebuggerAgent = instrumentingAgents.enabledPageDebuggerAgent())
        pageDebuggerAgent->didCancelAnimationFrame(callbackId);
    if (auto* timelineAgent = instrumentingAgents.trackingTimelineAgent())
        timelineAgent->didCancelAnimationFrame(callbackId, document.frame());
}

void InspectorInstrumentation::willFireAnimationFrameImpl(InstrumentingAgents& instrumentingAgents, int callbackId, Document& document)
{
    if (auto* pageDebuggerAgent = instrumentingAgents.enabledPageDebuggerAgent())
        pageDebuggerAgent->willFireAnimationFrame(callbackId);
    if (auto* pageDOMDebuggerAgent = instrumentingAgents.enabledPageDOMDebuggerAgent())
        pageDOMDebuggerAgent->willFireAnimationFrame();
    if (auto* timelineAgent = instrumentingAgents.trackingTimelineAgent())
        timelineAgent->willFireAnimationFrame(callbackId, document.frame());
}

void InspectorInstrumentation::didFireAnimationFrameImpl(InstrumentingAgents& instrumentingAgents, int callbackId)
{
    if (auto* pageDebuggerAgent = instrumentingAgents.enabledPageDebuggerAgent())
        pageDebuggerAgent->didFireAnimationFrame(callbackId);
    if (auto* pageDOMDebuggerAgent = instrumentingAgents.enabledPageDOMDebuggerAgent())
        pageDOMDebuggerAgent->didFireAnimationFrame();
    if (auto* timelineAgent = instrumentingAgents.trackingTimelineAgent())
        timelineAgent->didFireAnimationFrame();
}

void InspectorInstrumentation::willFireObserverCallbackImpl(InstrumentingAgents& instrumentingAgents, const String& callbackType, ScriptExecutionContext& context)
{
    if (auto* timelineAgent = instrumentingAgents.trackingTimelineAgent())
        timelineAgent->willFireObserverCallback(callbackType, frameForScriptExecutionContext(&context));
}

void InspectorInstrumentation::didFireObserverCallbackImpl(InstrumentingAgents& instrumentingAgents)
{
    if (auto* timelineAgent = instrumentingAgents.trackingTimelineAgent())
        timelineAgent->didFireObserverCallback();
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

InstrumentingAgents* InspectorInstrumentation::instrumentingAgents(const RenderObject& renderer)
{
    return instrumentingAgents(renderer.frame());
}

void InspectorInstrumentation::layerTreeDidChangeImpl(InstrumentingAgents& instrumentingAgents)
{
    if (auto* layerTreeAgent = instrumentingAgents.enabledLayerTreeAgent())
        layerTreeAgent->layerTreeDidChange();
}

void InspectorInstrumentation::renderLayerDestroyedImpl(InstrumentingAgents& instrumentingAgents, const RenderLayer& renderLayer)
{
    if (auto* layerTreeAgent = instrumentingAgents.enabledLayerTreeAgent())
        layerTreeAgent->renderLayerDestroyed(renderLayer);
}

InstrumentingAgents& InspectorInstrumentation::instrumentingAgents(WorkerOrWorkletGlobalScope& globalScope)
{
    return globalScope.inspectorController().m_instrumentingAgents;
}

InstrumentingAgents& InspectorInstrumentation::instrumentingAgents(Page& page)
{
    ASSERT(isMainThread());
    return page.inspectorController().m_instrumentingAgents.get();
}

InstrumentingAgents* InspectorInstrumentation::instrumentingAgents(ScriptExecutionContext& context)
{
    // Using RefPtr makes us hit the m_inRemovedLastRefFunction assert.
    if (WeakPtr document = dynamicDowncast<Document>(context))
        return instrumentingAgents(document->protectedPage().get());
    if (RefPtr workerOrWorkletGlobal = dynamicDowncast<WorkerOrWorkletGlobalScope>(context))
        return &instrumentingAgents(*workerOrWorkletGlobal);
    return nullptr;
}

} // namespace WebCore
