/*
* Copyright (C) 2010 Google Inc. All rights reserved.
* Copyright (C) 2014-2016 Apple Inc. All rights reserved.
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

#pragma once

#include "CSSSelector.h"
#include "CanvasBase.h"
#include "CanvasRenderingContext.h"
#include "Database.h"
#include "DeclarativeAnimation.h"
#include "DocumentThreadableLoader.h"
#include "Element.h"
#include "Event.h"
#include "EventTarget.h"
#include "FormData.h"
#include "Frame.h"
#include "FrameView.h"
#include "HitTestResult.h"
#include "InspectorInstrumentationPublic.h"
#include "Page.h"
#include "ResourceLoader.h"
#include "ResourceLoaderIdentifier.h"
#include "StorageArea.h"
#include "WebAnimation.h"
#include "WorkerInspectorProxy.h"
#include <JavaScriptCore/ConsoleMessage.h>
#include <initializer_list>
#include <wtf/CompletionHandler.h>
#include <wtf/Function.h>
#include <wtf/MemoryPressureHandler.h>
#include <wtf/ObjectIdentifier.h>
#include <wtf/RefPtr.h>

#if ENABLE(WEBGL)
#include "WebGLRenderingContextBase.h"
#endif

namespace Inspector {
class ConsoleMessage;
class ScriptArguments;
class ScriptCallStack;
}

namespace WebCore {

class CachedResource;
class CharacterData;
class DOMWindow;
class DOMWrapperWorld;
class Document;
class DocumentLoader;
class EventListener;
class HTTPHeaderMap;
class InspectorTimelineAgent;
class InstrumentingAgents;
class KeyframeEffect;
class NetworkLoadMetrics;
class Node;
class PseudoElement;
class RegisteredEventListener;
class RenderLayer;
class RenderObject;
class ResourceLoader;
class ResourceRequest;
class ResourceResponse;
class ScriptExecutionContext;
class SecurityOrigin;
class ShadowRoot;
class FragmentedSharedBuffer;
class TimerBase;
class WebKitNamedFlow;
class WebSocketChannel;
class WorkerOrWorkletGlobalScope;

struct Styleable;

#if ENABLE(WEBGL)
class WebGLProgram;
#endif

using WebSocketChannelIdentifier = ObjectIdentifier<WebSocketChannel>;
enum class StorageType : uint8_t;

struct ComputedEffectTiming;
struct WebSocketFrame;

class InspectorInstrumentation {
public:
    static void didClearWindowObjectInWorld(Frame&, DOMWrapperWorld&);
    static bool isDebuggerPaused(Frame*);

    static int identifierForNode(Node&);
    static void addEventListenersToNode(Node&);
    static void willInsertDOMNode(Document&, Node& parent);
    static void didInsertDOMNode(Document&, Node&);
    static void willRemoveDOMNode(Document&, Node&);
    static void didRemoveDOMNode(Document&, Node&);
    static void willDestroyDOMNode(Node&);
    static void didChangeRendererForDOMNode(Node&);
    static void didAddOrRemoveScrollbars(FrameView&);
    static void didAddOrRemoveScrollbars(RenderObject&);
    static void willModifyDOMAttr(Document&, Element&, const AtomString& oldValue, const AtomString& newValue);
    static void didModifyDOMAttr(Document&, Element&, const AtomString& name, const AtomString& value);
    static void didRemoveDOMAttr(Document&, Element&, const AtomString& name);
    static void characterDataModified(Document&, CharacterData&);
    static void willInvalidateStyleAttr(Element&);
    static void didInvalidateStyleAttr(Element&);
    static void documentDetached(Document&);
    static void frameWindowDiscarded(Frame&, DOMWindow*);
    static void mediaQueryResultChanged(Document&);
    static void activeStyleSheetsUpdated(Document&);
    static void didPushShadowRoot(Element& host, ShadowRoot&);
    static void willPopShadowRoot(Element& host, ShadowRoot&);
    static void didChangeCustomElementState(Element&);
    static void pseudoElementCreated(Page*, PseudoElement&);
    static void pseudoElementDestroyed(Page*, PseudoElement&);
    static void didCreateNamedFlow(Document*, WebKitNamedFlow&);
    static void willRemoveNamedFlow(Document*, WebKitNamedFlow&);
    static void didChangeRegionOverset(Document&, WebKitNamedFlow&);
    static void didRegisterNamedFlowContentElement(Document&, WebKitNamedFlow&, Node& contentElement, Node* nextContentElement = nullptr);
    static void didUnregisterNamedFlowContentElement(Document&, WebKitNamedFlow&, Node& contentElement);

    static void mouseDidMoveOverElement(Page&, const HitTestResult&, unsigned modifierFlags);
    static bool handleMousePress(Frame&);
    static bool handleTouchEvent(Frame&, Node&);
    static bool forcePseudoState(const Element&, CSSSelector::PseudoClassType);

    static void willSendXMLHttpRequest(ScriptExecutionContext*, const String& url);
    static void willFetch(ScriptExecutionContext&, const String& url);
    static void didInstallTimer(ScriptExecutionContext&, int timerId, Seconds timeout, bool singleShot);
    static void didRemoveTimer(ScriptExecutionContext&, int timerId);

    static int willPostMessage(Frame&);
    static void didPostMessage(Frame&, int postTimerIdentifier, JSC::JSGlobalObject&);
    static void didFailPostMessage(Frame&, int postTimerIdentifier);
    static void willDispatchPostMessage(Frame&, int postTimerIdentifier);
    static void didDispatchPostMessage(Frame&, int postTimerIdentifier);

    static void willCallFunction(ScriptExecutionContext*, const String& scriptName, int scriptLine, int scriptColumn);
    static void didCallFunction(ScriptExecutionContext*);
    static void didAddEventListener(EventTarget&, const AtomString& eventType, EventListener&, bool capture);
    static void willRemoveEventListener(EventTarget&, const AtomString& eventType, EventListener&, bool capture);
    static bool isEventListenerDisabled(EventTarget&, const AtomString& eventType, EventListener&, bool capture);
    static void willDispatchEvent(Document&, const Event&);
    static void didDispatchEvent(Document&, const Event&);
    static void willHandleEvent(ScriptExecutionContext&, Event&, const RegisteredEventListener&);
    static void didHandleEvent(ScriptExecutionContext&, Event&, const RegisteredEventListener&);
    static void willDispatchEventOnWindow(Frame*, const Event&, DOMWindow&);
    static void didDispatchEventOnWindow(Frame*, const Event&);
    static void eventDidResetAfterDispatch(const Event&);
    static void willEvaluateScript(Frame&, const String& url, int lineNumber, int columnNumber);
    static void didEvaluateScript(Frame&);
    static void willFireTimer(ScriptExecutionContext&, int timerId, bool oneShot);
    static void didFireTimer(ScriptExecutionContext&, int timerId, bool oneShot);
    static void didInvalidateLayout(Frame&);
    static void willLayout(Frame&);
    static void didLayout(Frame&, RenderObject&);
    static void didScroll(Page&);
    static void willComposite(Frame&);
    static void didComposite(Frame&);
    static void willPaint(RenderObject&);
    static void didPaint(RenderObject&, const LayoutRect&);
    static void willRecalculateStyle(Document&);
    static void didRecalculateStyle(Document&);
    static void didScheduleStyleRecalculation(Document&);
    static void applyUserAgentOverride(Frame&, String&);
    static void applyEmulatedMedia(Frame&, AtomString&);

    static void flexibleBoxRendererBeganLayout(const RenderObject&);
    static void flexibleBoxRendererWrappedToNextLine(const RenderObject&, size_t lineStartItemIndex);

    static void willSendRequest(Frame*, ResourceLoaderIdentifier, DocumentLoader*, ResourceRequest&, const ResourceResponse& redirectResponse, const CachedResource*, ResourceLoader*);
    static void didLoadResourceFromMemoryCache(Page&, DocumentLoader*, CachedResource*);
    static void didReceiveResourceResponse(Frame&, ResourceLoaderIdentifier, DocumentLoader*, const ResourceResponse&, ResourceLoader*);
    static void didReceiveThreadableLoaderResponse(DocumentThreadableLoader&, ResourceLoaderIdentifier);
    static void didReceiveData(Frame*, ResourceLoaderIdentifier, const SharedBuffer*, int encodedDataLength);
    static void didFinishLoading(Frame*, DocumentLoader*, ResourceLoaderIdentifier, const NetworkLoadMetrics&, ResourceLoader*);
    static void didFailLoading(Frame*, DocumentLoader*, ResourceLoaderIdentifier, const ResourceError&);

    static void willSendRequest(WorkerOrWorkletGlobalScope&, ResourceLoaderIdentifier, ResourceRequest&);
    static void didReceiveResourceResponse(WorkerOrWorkletGlobalScope&, ResourceLoaderIdentifier, const ResourceResponse&);
    static void didReceiveData(WorkerOrWorkletGlobalScope&, ResourceLoaderIdentifier, const SharedBuffer&);
    static void didFinishLoading(WorkerOrWorkletGlobalScope&, ResourceLoaderIdentifier, const NetworkLoadMetrics&);
    static void didFailLoading(WorkerOrWorkletGlobalScope&, ResourceLoaderIdentifier, const ResourceError&);

    // Some network requests do not go through the normal network loading path.
    // These network requests have to issue their own willSendRequest / didReceiveResponse / didFinishLoading / didFailLoading
    // instrumentation calls. Some of these loads are for resources that lack a CachedResource::Type.
    enum class LoadType { Ping, Beacon };
    static void willSendRequestOfType(Frame*, ResourceLoaderIdentifier, DocumentLoader*, ResourceRequest&, LoadType);

    static void continueAfterXFrameOptionsDenied(Frame&, ResourceLoaderIdentifier, DocumentLoader&, const ResourceResponse&);
    static void continueWithPolicyDownload(Frame&, ResourceLoaderIdentifier, DocumentLoader&, const ResourceResponse&);
    static void continueWithPolicyIgnore(Frame&, ResourceLoaderIdentifier, DocumentLoader&, const ResourceResponse&);
    static void willLoadXHRSynchronously(ScriptExecutionContext*);
    static void didLoadXHRSynchronously(ScriptExecutionContext*);
    static void scriptImported(ScriptExecutionContext&, ResourceLoaderIdentifier, const String& sourceString);
    static void scriptExecutionBlockedByCSP(ScriptExecutionContext*, const String& directiveText);
    static void didReceiveScriptResponse(ScriptExecutionContext*, ResourceLoaderIdentifier);
    static void domContentLoadedEventFired(Frame&);
    static void loadEventFired(Frame*);
    static void frameDetachedFromParent(Frame&);
    static void didCommitLoad(Frame&, DocumentLoader*);
    static void frameDocumentUpdated(Frame&);
    static void loaderDetachedFromFrame(Frame&, DocumentLoader&);
    static void frameStartedLoading(Frame&);
    static void frameStoppedLoading(Frame&);
    static void frameScheduledNavigation(Frame&, Seconds delay);
    static void frameClearedScheduledNavigation(Frame&);
#if ENABLE(DARK_MODE_CSS) || HAVE(OS_DARK_MODE_SUPPORT)
    static void defaultAppearanceDidChange(Page&, bool useDarkAppearance);
#endif
    static void willDestroyCachedResource(CachedResource&);

    static bool willIntercept(const Frame*, const ResourceRequest&);
    static bool shouldInterceptRequest(const ResourceLoader&);
    static bool shouldInterceptResponse(const Frame&, const ResourceResponse&);
    static void interceptRequest(ResourceLoader&, Function<void(const ResourceRequest&)>&&);
    static void interceptResponse(const Frame&, const ResourceResponse&, ResourceLoaderIdentifier, CompletionHandler<void(const ResourceResponse&, RefPtr<FragmentedSharedBuffer>)>&&);

    static void addMessageToConsole(Page&, std::unique_ptr<Inspector::ConsoleMessage>);
    static void addMessageToConsole(WorkerOrWorkletGlobalScope&, std::unique_ptr<Inspector::ConsoleMessage>);

    static void consoleCount(Page&, JSC::JSGlobalObject*, const String& label);
    static void consoleCount(WorkerOrWorkletGlobalScope&, JSC::JSGlobalObject*, const String& label);
    static void consoleCountReset(Page&, JSC::JSGlobalObject*, const String& label);
    static void consoleCountReset(WorkerOrWorkletGlobalScope&, JSC::JSGlobalObject*, const String& label);

    static void takeHeapSnapshot(Frame&, const String& title);
    static void startConsoleTiming(Frame&, JSC::JSGlobalObject*, const String& label);
    static void startConsoleTiming(WorkerOrWorkletGlobalScope&, JSC::JSGlobalObject*, const String& label);
    static void logConsoleTiming(Frame&, JSC::JSGlobalObject*, const String& label, Ref<Inspector::ScriptArguments>&&);
    static void logConsoleTiming(WorkerOrWorkletGlobalScope&, JSC::JSGlobalObject*, const String& label, Ref<Inspector::ScriptArguments>&&);
    static void stopConsoleTiming(Frame&, JSC::JSGlobalObject*, const String& label);
    static void stopConsoleTiming(WorkerOrWorkletGlobalScope&, JSC::JSGlobalObject*, const String& label);
    static void consoleTimeStamp(Frame&, Ref<Inspector::ScriptArguments>&&);
    static void startProfiling(Page&, JSC::JSGlobalObject*, const String& title);
    static void stopProfiling(Page&, JSC::JSGlobalObject*, const String& title);
    static void consoleStartRecordingCanvas(CanvasRenderingContext&, JSC::JSGlobalObject&, JSC::JSObject* options);
    static void consoleStopRecordingCanvas(CanvasRenderingContext&);

    static void didRequestAnimationFrame(Document&, int callbackId);
    static void didCancelAnimationFrame(Document&, int callbackId);
    static void willFireAnimationFrame(Document&, int callbackId);
    static void didFireAnimationFrame(Document&, int callbackId);

    static void willFireObserverCallback(ScriptExecutionContext&, const String& callbackType);
    static void didFireObserverCallback(ScriptExecutionContext&);

    static void didOpenDatabase(Database&);

    static void didDispatchDOMStorageEvent(Page&, const String& key, const String& oldValue, const String& newValue, StorageType, const SecurityOrigin&);

    static bool shouldWaitForDebuggerOnStart(ScriptExecutionContext&);
    static void workerStarted(WorkerInspectorProxy&);
    static void workerTerminated(WorkerInspectorProxy&);

    static void didCreateWebSocket(Document*, WebSocketChannelIdentifier, const URL& requestURL);
    static void willSendWebSocketHandshakeRequest(Document*, WebSocketChannelIdentifier, const ResourceRequest&);
    static void didReceiveWebSocketHandshakeResponse(Document*, WebSocketChannelIdentifier, const ResourceResponse&);
    static void didCloseWebSocket(Document*, WebSocketChannelIdentifier);
    static void didReceiveWebSocketFrame(Document*, WebSocketChannelIdentifier, const WebSocketFrame&);
    static void didSendWebSocketFrame(Document*, WebSocketChannelIdentifier, const WebSocketFrame&);
    static void didReceiveWebSocketFrameError(Document*, WebSocketChannelIdentifier, const String& errorMessage);

#if ENABLE(RESOURCE_USAGE)
    static void didHandleMemoryPressure(Page&, Critical);
#endif

    static void didChangeCSSCanvasClientNodes(CanvasBase&);
    static void didCreateCanvasRenderingContext(CanvasRenderingContext&);
    static void didChangeCanvasMemory(CanvasRenderingContext&);
    static void didFinishRecordingCanvasFrame(CanvasRenderingContext&, bool forceDispatch = false);
#if ENABLE(WEBGL)
    static void didEnableExtension(WebGLRenderingContextBase&, const String&);
    static void didCreateWebGLProgram(WebGLRenderingContextBase&, WebGLProgram&);
    static void willDestroyWebGLProgram(WebGLProgram&);
    static bool isWebGLProgramDisabled(WebGLRenderingContextBase&, WebGLProgram&);
    static bool isWebGLProgramHighlighted(WebGLRenderingContextBase&, WebGLProgram&);
#endif

    static void willApplyKeyframeEffect(const Styleable&, KeyframeEffect&, ComputedEffectTiming);
    static void didChangeWebAnimationName(WebAnimation&);
    static void didSetWebAnimationEffect(WebAnimation&);
    static void didChangeWebAnimationEffectTiming(WebAnimation&);
    static void didChangeWebAnimationEffectTarget(WebAnimation&);
    static void didCreateWebAnimation(WebAnimation&);
    static void willDestroyWebAnimation(WebAnimation&);

    static void networkStateChanged(Page&);
    static void updateApplicationCacheStatus(Frame*);

    static void layerTreeDidChange(Page*);
    static void renderLayerDestroyed(Page*, const RenderLayer&);

    static void frontendCreated();
    static void frontendDeleted();
    static bool hasFrontends() { return InspectorInstrumentationPublic::hasFrontends(); }

    static void firstFrontendCreated();
    static void lastFrontendDeleted();

    static bool consoleAgentEnabled(ScriptExecutionContext*);
    static bool timelineAgentTracking(ScriptExecutionContext*);

    static InstrumentingAgents* instrumentingAgents(Page*);
    static InstrumentingAgents* instrumentingAgents(ScriptExecutionContext*);

    static void registerInstrumentingAgents(InstrumentingAgents&);
    static void unregisterInstrumentingAgents(InstrumentingAgents&);

private:
    static void didClearWindowObjectInWorldImpl(InstrumentingAgents&, Frame&, DOMWrapperWorld&);
    static bool isDebuggerPausedImpl(InstrumentingAgents&);

    static int identifierForNodeImpl(InstrumentingAgents&, Node&);
    static void addEventListenersToNodeImpl(InstrumentingAgents&, Node&);
    static void willInsertDOMNodeImpl(InstrumentingAgents&, Node& parent);
    static void didInsertDOMNodeImpl(InstrumentingAgents&, Node&);
    static void willRemoveDOMNodeImpl(InstrumentingAgents&, Node&);
    static void didRemoveDOMNodeImpl(InstrumentingAgents&, Node&);
    static void willDestroyDOMNodeImpl(InstrumentingAgents&, Node&);
    static void didChangeRendererForDOMNodeImpl(InstrumentingAgents&, Node&);
    static void didAddOrRemoveScrollbarsImpl(InstrumentingAgents&, FrameView&);
    static void didAddOrRemoveScrollbarsImpl(InstrumentingAgents&, RenderObject&);
    static void willModifyDOMAttrImpl(InstrumentingAgents&, Element&, const AtomString& oldValue, const AtomString& newValue);
    static void didModifyDOMAttrImpl(InstrumentingAgents&, Element&, const AtomString& name, const AtomString& value);
    static void didRemoveDOMAttrImpl(InstrumentingAgents&, Element&, const AtomString& name);
    static void characterDataModifiedImpl(InstrumentingAgents&, CharacterData&);
    static void willInvalidateStyleAttrImpl(InstrumentingAgents&, Element&);
    static void didInvalidateStyleAttrImpl(InstrumentingAgents&, Element&);
    static void documentDetachedImpl(InstrumentingAgents&, Document&);
    static void frameWindowDiscardedImpl(InstrumentingAgents&, DOMWindow*);
    static void mediaQueryResultChangedImpl(InstrumentingAgents&);
    static void activeStyleSheetsUpdatedImpl(InstrumentingAgents&, Document&);
    static void didPushShadowRootImpl(InstrumentingAgents&, Element& host, ShadowRoot&);
    static void willPopShadowRootImpl(InstrumentingAgents&, Element& host, ShadowRoot&);
    static void didChangeCustomElementStateImpl(InstrumentingAgents&, Element&);
    static void pseudoElementCreatedImpl(InstrumentingAgents&, PseudoElement&);
    static void pseudoElementDestroyedImpl(InstrumentingAgents&, PseudoElement&);
    static void didCreateNamedFlowImpl(InstrumentingAgents&, Document*, WebKitNamedFlow&);
    static void willRemoveNamedFlowImpl(InstrumentingAgents&, Document*, WebKitNamedFlow&);
    static void didChangeRegionOversetImpl(InstrumentingAgents&, Document&, WebKitNamedFlow&);
    static void didRegisterNamedFlowContentElementImpl(InstrumentingAgents&, Document&, WebKitNamedFlow&, Node& contentElement, Node* nextContentElement = nullptr);
    static void didUnregisterNamedFlowContentElementImpl(InstrumentingAgents&, Document&, WebKitNamedFlow&, Node& contentElement);

    static void mouseDidMoveOverElementImpl(InstrumentingAgents&, const HitTestResult&, unsigned modifierFlags);
    static bool handleMousePressImpl(InstrumentingAgents&);
    static bool handleTouchEventImpl(InstrumentingAgents&, Node&);
    static bool forcePseudoStateImpl(InstrumentingAgents&, const Element&, CSSSelector::PseudoClassType);

    static void willSendXMLHttpRequestImpl(InstrumentingAgents&, const String& url);
    static void willFetchImpl(InstrumentingAgents&, const String& url);
    static void didInstallTimerImpl(InstrumentingAgents&, int timerId, Seconds timeout, bool singleShot, ScriptExecutionContext&);
    static void didRemoveTimerImpl(InstrumentingAgents&, int timerId, ScriptExecutionContext&);

    static int willPostMessageImpl(InstrumentingAgents&);
    static void didPostMessageImpl(InstrumentingAgents&, int postMessageIdentifier, JSC::JSGlobalObject&);
    static void didFailPostMessageImpl(InstrumentingAgents&, int postMessageIdentifier);
    static void willDispatchPostMessageImpl(InstrumentingAgents&, int postMessageIdentifier);
    static void didDispatchPostMessageImpl(InstrumentingAgents&, int postMessageIdentifier);

    static void willCallFunctionImpl(InstrumentingAgents&, const String& scriptName, int scriptLine, int scriptColumn, ScriptExecutionContext*);
    static void didCallFunctionImpl(InstrumentingAgents&, ScriptExecutionContext*);
    static void didAddEventListenerImpl(InstrumentingAgents&, EventTarget&, const AtomString& eventType, EventListener&, bool capture);
    static void willRemoveEventListenerImpl(InstrumentingAgents&, EventTarget&, const AtomString& eventType, EventListener&, bool capture);
    static bool isEventListenerDisabledImpl(InstrumentingAgents&, EventTarget&, const AtomString& eventType, EventListener&, bool capture);
    static void willDispatchEventImpl(InstrumentingAgents&, Document&, const Event&);
    static void willHandleEventImpl(InstrumentingAgents&, ScriptExecutionContext&, Event&, const RegisteredEventListener&);
    static void didHandleEventImpl(InstrumentingAgents&, ScriptExecutionContext&, Event&, const RegisteredEventListener&);
    static void didDispatchEventImpl(InstrumentingAgents&, const Event&);
    static void willDispatchEventOnWindowImpl(InstrumentingAgents&, const Event&, DOMWindow&);
    static void didDispatchEventOnWindowImpl(InstrumentingAgents&, const Event&);
    static void eventDidResetAfterDispatchImpl(InstrumentingAgents&, const Event&);
    static void willEvaluateScriptImpl(InstrumentingAgents&, Frame&, const String& url, int lineNumber, int columnNumber);
    static void didEvaluateScriptImpl(InstrumentingAgents&, Frame&);
    static void willFireTimerImpl(InstrumentingAgents&, int timerId, bool oneShot, ScriptExecutionContext&);
    static void didFireTimerImpl(InstrumentingAgents&, int timerId, bool oneShot);
    static void didInvalidateLayoutImpl(InstrumentingAgents&, Frame&);
    static void willLayoutImpl(InstrumentingAgents&, Frame&);
    static void didLayoutImpl(InstrumentingAgents&, RenderObject&);
    static void didScrollImpl(InstrumentingAgents&);
    static void willCompositeImpl(InstrumentingAgents&, Frame&);
    static void didCompositeImpl(InstrumentingAgents&);
    static void willPaintImpl(InstrumentingAgents&, RenderObject&);
    static void didPaintImpl(InstrumentingAgents&, RenderObject&, const LayoutRect&);
    static void willRecalculateStyleImpl(InstrumentingAgents&, Document&);
    static void didRecalculateStyleImpl(InstrumentingAgents&);
    static void didScheduleStyleRecalculationImpl(InstrumentingAgents&, Document&);
    static void applyUserAgentOverrideImpl(InstrumentingAgents&, String&);
    static void applyEmulatedMediaImpl(InstrumentingAgents&, AtomString&);

    static void flexibleBoxRendererBeganLayoutImpl(InstrumentingAgents&, const RenderObject&);
    static void flexibleBoxRendererWrappedToNextLineImpl(InstrumentingAgents&, const RenderObject&, size_t lineStartItemIndex);

    static void willSendRequestImpl(InstrumentingAgents&, ResourceLoaderIdentifier, DocumentLoader*, ResourceRequest&, const ResourceResponse& redirectResponse, const CachedResource*, ResourceLoader*);
    static void willSendRequestOfTypeImpl(InstrumentingAgents&, ResourceLoaderIdentifier, DocumentLoader*, ResourceRequest&, LoadType);
    static void markResourceAsCachedImpl(InstrumentingAgents&, ResourceLoaderIdentifier);
    static void didLoadResourceFromMemoryCacheImpl(InstrumentingAgents&, DocumentLoader*, CachedResource*);
    static void didReceiveResourceResponseImpl(InstrumentingAgents&, ResourceLoaderIdentifier, DocumentLoader*, const ResourceResponse&, ResourceLoader*);
    static void didReceiveThreadableLoaderResponseImpl(InstrumentingAgents&, DocumentThreadableLoader&, ResourceLoaderIdentifier);
    static void didReceiveDataImpl(InstrumentingAgents&, ResourceLoaderIdentifier, const SharedBuffer*, int encodedDataLength);
    static void didFinishLoadingImpl(InstrumentingAgents&, ResourceLoaderIdentifier, DocumentLoader*, const NetworkLoadMetrics&, ResourceLoader*);
    static void didFailLoadingImpl(InstrumentingAgents&, ResourceLoaderIdentifier, DocumentLoader*, const ResourceError&);
    static void willLoadXHRSynchronouslyImpl(InstrumentingAgents&);
    static void didLoadXHRSynchronouslyImpl(InstrumentingAgents&);
    static void scriptImportedImpl(InstrumentingAgents&, ResourceLoaderIdentifier, const String& sourceString);
    static void scriptExecutionBlockedByCSPImpl(InstrumentingAgents&, const String& directiveText);
    static void didReceiveScriptResponseImpl(InstrumentingAgents&, ResourceLoaderIdentifier);
    static void domContentLoadedEventFiredImpl(InstrumentingAgents&, Frame&);
    static void loadEventFiredImpl(InstrumentingAgents&, Frame*);
    static void frameDetachedFromParentImpl(InstrumentingAgents&, Frame&);
    static void didCommitLoadImpl(InstrumentingAgents&, Frame&, DocumentLoader*);
    static void frameDocumentUpdatedImpl(InstrumentingAgents&, Frame&);
    static void loaderDetachedFromFrameImpl(InstrumentingAgents&, DocumentLoader&);
    static void frameStartedLoadingImpl(InstrumentingAgents&, Frame&);
    static void frameStoppedLoadingImpl(InstrumentingAgents&, Frame&);
    static void frameScheduledNavigationImpl(InstrumentingAgents&, Frame&, Seconds delay);
    static void frameClearedScheduledNavigationImpl(InstrumentingAgents&, Frame&);
#if ENABLE(DARK_MODE_CSS) || HAVE(OS_DARK_MODE_SUPPORT)
    static void defaultAppearanceDidChangeImpl(InstrumentingAgents&, bool useDarkAppearance);
#endif
    static void willDestroyCachedResourceImpl(CachedResource&);

    static bool willInterceptImpl(InstrumentingAgents&, const ResourceRequest&);
    static bool shouldInterceptRequestImpl(InstrumentingAgents&, const ResourceLoader&);
    static bool shouldInterceptResponseImpl(InstrumentingAgents&, const ResourceResponse&);
    static void interceptRequestImpl(InstrumentingAgents&, ResourceLoader&, Function<void(const ResourceRequest&)>&&);
    static void interceptResponseImpl(InstrumentingAgents&, const ResourceResponse&, ResourceLoaderIdentifier, CompletionHandler<void(const ResourceResponse&, RefPtr<FragmentedSharedBuffer>)>&&);

    static void addMessageToConsoleImpl(InstrumentingAgents&, std::unique_ptr<Inspector::ConsoleMessage>);

    static void consoleCountImpl(InstrumentingAgents&, JSC::JSGlobalObject*, const String& label);
    static void consoleCountResetImpl(InstrumentingAgents&, JSC::JSGlobalObject*, const String& label);
    static void takeHeapSnapshotImpl(InstrumentingAgents&, const String& title);
    static void startConsoleTimingImpl(InstrumentingAgents&, Frame&, JSC::JSGlobalObject*, const String& label);
    static void startConsoleTimingImpl(InstrumentingAgents&, JSC::JSGlobalObject*, const String& label);
    static void logConsoleTimingImpl(InstrumentingAgents&, JSC::JSGlobalObject*, const String& label, Ref<Inspector::ScriptArguments>&&);
    static void stopConsoleTimingImpl(InstrumentingAgents&, Frame&, JSC::JSGlobalObject*, const String& label);
    static void stopConsoleTimingImpl(InstrumentingAgents&, JSC::JSGlobalObject*, const String& label);
    static void consoleTimeStampImpl(InstrumentingAgents&, Frame&, Ref<Inspector::ScriptArguments>&&);
    static void startProfilingImpl(InstrumentingAgents&, JSC::JSGlobalObject*, const String& title);
    static void stopProfilingImpl(InstrumentingAgents&, JSC::JSGlobalObject*, const String& title);
    static void consoleStartRecordingCanvasImpl(InstrumentingAgents&, CanvasRenderingContext&, JSC::JSGlobalObject&, JSC::JSObject* options);
    static void consoleStopRecordingCanvasImpl(InstrumentingAgents&, CanvasRenderingContext&);

    static void didRequestAnimationFrameImpl(InstrumentingAgents&, int callbackId, Document&);
    static void didCancelAnimationFrameImpl(InstrumentingAgents&, int callbackId, Document&);
    static void willFireAnimationFrameImpl(InstrumentingAgents&, int callbackId, Document&);
    static void didFireAnimationFrameImpl(InstrumentingAgents&, int callbackId);

    static void willFireObserverCallbackImpl(InstrumentingAgents&, const String&, ScriptExecutionContext&);
    static void didFireObserverCallbackImpl(InstrumentingAgents&);

    static void didOpenDatabaseImpl(InstrumentingAgents&, Database&);

    static void didDispatchDOMStorageEventImpl(InstrumentingAgents&, const String& key, const String& oldValue, const String& newValue, StorageType, const SecurityOrigin&);

    static bool shouldWaitForDebuggerOnStartImpl(InstrumentingAgents&);
    static void workerStartedImpl(InstrumentingAgents&, WorkerInspectorProxy&);
    static void workerTerminatedImpl(InstrumentingAgents&, WorkerInspectorProxy&);

    static void didCreateWebSocketImpl(InstrumentingAgents&, WebSocketChannelIdentifier, const URL& requestURL);
    static void willSendWebSocketHandshakeRequestImpl(InstrumentingAgents&, WebSocketChannelIdentifier, const ResourceRequest&);
    static void didReceiveWebSocketHandshakeResponseImpl(InstrumentingAgents&, WebSocketChannelIdentifier, const ResourceResponse&);
    static void didCloseWebSocketImpl(InstrumentingAgents&, WebSocketChannelIdentifier);
    static void didReceiveWebSocketFrameImpl(InstrumentingAgents&, WebSocketChannelIdentifier, const WebSocketFrame&);
    static void didSendWebSocketFrameImpl(InstrumentingAgents&, WebSocketChannelIdentifier, const WebSocketFrame&);
    static void didReceiveWebSocketFrameErrorImpl(InstrumentingAgents&, WebSocketChannelIdentifier, const String&);

#if ENABLE(RESOURCE_USAGE)
    static void didHandleMemoryPressureImpl(InstrumentingAgents&, Critical);
#endif

    static void networkStateChangedImpl(InstrumentingAgents&);
    static void updateApplicationCacheStatusImpl(InstrumentingAgents&, Frame&);

    static void didChangeCSSCanvasClientNodesImpl(InstrumentingAgents&, CanvasBase&);
    static void didCreateCanvasRenderingContextImpl(InstrumentingAgents&, CanvasRenderingContext&);
    static void didChangeCanvasMemoryImpl(InstrumentingAgents&, CanvasRenderingContext&);
    static void didFinishRecordingCanvasFrameImpl(InstrumentingAgents&, CanvasRenderingContext&, bool forceDispatch = false);
#if ENABLE(WEBGL)
    static void didEnableExtensionImpl(InstrumentingAgents&, WebGLRenderingContextBase&, const String&);
    static void didCreateWebGLProgramImpl(InstrumentingAgents&, WebGLRenderingContextBase&, WebGLProgram&);
    static void willDestroyWebGLProgramImpl(InstrumentingAgents&, WebGLProgram&);
    static bool isWebGLProgramDisabledImpl(InstrumentingAgents&, WebGLProgram&);
    static bool isWebGLProgramHighlightedImpl(InstrumentingAgents&, WebGLProgram&);
#endif

    static void willApplyKeyframeEffectImpl(InstrumentingAgents&, const Styleable&, KeyframeEffect&, ComputedEffectTiming);
    static void didChangeWebAnimationNameImpl(InstrumentingAgents&, WebAnimation&);
    static void didSetWebAnimationEffectImpl(InstrumentingAgents&, WebAnimation&);
    static void didChangeWebAnimationEffectTimingImpl(InstrumentingAgents&, WebAnimation&);
    static void didChangeWebAnimationEffectTargetImpl(InstrumentingAgents&, WebAnimation&);
    static void didCreateWebAnimationImpl(InstrumentingAgents&, WebAnimation&);
    static void willDestroyWebAnimationImpl(InstrumentingAgents&, WebAnimation&);

    static void layerTreeDidChangeImpl(InstrumentingAgents&);
    static void renderLayerDestroyedImpl(InstrumentingAgents&, const RenderLayer&);

    static InstrumentingAgents& instrumentingAgents(Page&);
    static InstrumentingAgents& instrumentingAgents(WorkerOrWorkletGlobalScope&);

    static InstrumentingAgents* instrumentingAgents(const Frame&);
    static InstrumentingAgents* instrumentingAgents(const Frame*);
    static InstrumentingAgents* instrumentingAgents(ScriptExecutionContext&);
    static InstrumentingAgents* instrumentingAgents(Document&);
    static InstrumentingAgents* instrumentingAgents(Document*);
    static InstrumentingAgents* instrumentingAgents(const RenderObject&);
    static InstrumentingAgents* instrumentingAgents(WorkerOrWorkletGlobalScope*);
};

inline void InspectorInstrumentation::didClearWindowObjectInWorld(Frame& frame, DOMWrapperWorld& world)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(frame))
        didClearWindowObjectInWorldImpl(*agents, frame, world);
}

inline bool InspectorInstrumentation::isDebuggerPaused(Frame* frame)
{
    FAST_RETURN_IF_NO_FRONTENDS(false);
    if (auto* agents = instrumentingAgents(frame))
        return isDebuggerPausedImpl(*agents);
    return false;
}

inline int InspectorInstrumentation::identifierForNode(Node& node)
{
    FAST_RETURN_IF_NO_FRONTENDS(0);
    if (auto* agents = instrumentingAgents(node.document()))
        return identifierForNodeImpl(*agents, node);
    return 0;
}

inline void InspectorInstrumentation::addEventListenersToNode(Node& node)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(node.document()))
        addEventListenersToNodeImpl(*agents, node);
}

inline void InspectorInstrumentation::willInsertDOMNode(Document& document, Node& parent)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(document))
        willInsertDOMNodeImpl(*agents, parent);
}

inline void InspectorInstrumentation::didInsertDOMNode(Document& document, Node& node)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(document))
        didInsertDOMNodeImpl(*agents, node);
}

inline void InspectorInstrumentation::willRemoveDOMNode(Document& document, Node& node)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(document))
        willRemoveDOMNodeImpl(*agents, node);
}

inline void InspectorInstrumentation::didRemoveDOMNode(Document& document, Node& node)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(document))
        didRemoveDOMNodeImpl(*agents, node);
}

inline void InspectorInstrumentation::willDestroyDOMNode(Node& node)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(node.document()))
        willDestroyDOMNodeImpl(*agents, node);
}

inline void InspectorInstrumentation::didChangeRendererForDOMNode(Node& node)
{
    ASSERT(InspectorInstrumentationPublic::hasFrontends());
    if (auto* agents = instrumentingAgents(node.document()))
        didChangeRendererForDOMNodeImpl(*agents, node);
}

inline void InspectorInstrumentation::didAddOrRemoveScrollbars(FrameView& frameView)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(frameView.frame().document()))
        didAddOrRemoveScrollbarsImpl(*agents, frameView);
}

inline void InspectorInstrumentation::didAddOrRemoveScrollbars(RenderObject& renderer)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(renderer))
        didAddOrRemoveScrollbarsImpl(*agents, renderer);
}

inline void InspectorInstrumentation::willModifyDOMAttr(Document& document, Element& element, const AtomString& oldValue, const AtomString& newValue)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(document))
        willModifyDOMAttrImpl(*agents, element, oldValue, newValue);
}

inline void InspectorInstrumentation::didModifyDOMAttr(Document& document, Element& element, const AtomString& name, const AtomString& value)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(document))
        didModifyDOMAttrImpl(*agents, element, name, value);
}

inline void InspectorInstrumentation::didRemoveDOMAttr(Document& document, Element& element, const AtomString& name)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(document))
        didRemoveDOMAttrImpl(*agents, element, name);
}

inline void InspectorInstrumentation::willInvalidateStyleAttr(Element& element)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(element.document()))
        willInvalidateStyleAttrImpl(*agents, element);
}

inline void InspectorInstrumentation::didInvalidateStyleAttr(Element& element)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(element.document()))
        didInvalidateStyleAttrImpl(*agents, element);
}

inline void InspectorInstrumentation::documentDetached(Document& document)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(document))
        documentDetachedImpl(*agents, document);
}

inline void InspectorInstrumentation::frameWindowDiscarded(Frame& frame, DOMWindow* domWindow)
{
    if (auto* agents = instrumentingAgents(frame))
        frameWindowDiscardedImpl(*agents, domWindow);
}

inline void InspectorInstrumentation::mediaQueryResultChanged(Document& document)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(document))
        mediaQueryResultChangedImpl(*agents);
}

inline void InspectorInstrumentation::activeStyleSheetsUpdated(Document& document)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(document))
        activeStyleSheetsUpdatedImpl(*agents, document);
}

inline void InspectorInstrumentation::didPushShadowRoot(Element& host, ShadowRoot& root)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(host.document()))
        didPushShadowRootImpl(*agents, host, root);
}

inline void InspectorInstrumentation::willPopShadowRoot(Element& host, ShadowRoot& root)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(host.document()))
        willPopShadowRootImpl(*agents, host, root);
}

inline void InspectorInstrumentation::didChangeCustomElementState(Element& element)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(element.document()))
        didChangeCustomElementStateImpl(*agents, element);
}

inline void InspectorInstrumentation::pseudoElementCreated(Page* page, PseudoElement& pseudoElement)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(page))
        pseudoElementCreatedImpl(*agents, pseudoElement);
}

inline void InspectorInstrumentation::pseudoElementDestroyed(Page* page, PseudoElement& pseudoElement)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(page))
        pseudoElementDestroyedImpl(*agents, pseudoElement);
}

inline void InspectorInstrumentation::didCreateNamedFlow(Document* document, WebKitNamedFlow& namedFlow)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(document))
        didCreateNamedFlowImpl(*agents, document, namedFlow);
}

inline void InspectorInstrumentation::willRemoveNamedFlow(Document* document, WebKitNamedFlow& namedFlow)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(document))
        willRemoveNamedFlowImpl(*agents, document, namedFlow);
}

inline void InspectorInstrumentation::didChangeRegionOverset(Document& document, WebKitNamedFlow& namedFlow)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(document))
        didChangeRegionOversetImpl(*agents, document, namedFlow);
}

inline void InspectorInstrumentation::didRegisterNamedFlowContentElement(Document& document, WebKitNamedFlow& namedFlow, Node& contentElement, Node* nextContentElement)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(document))
        didRegisterNamedFlowContentElementImpl(*agents, document, namedFlow, contentElement, nextContentElement);
}

inline void InspectorInstrumentation::didUnregisterNamedFlowContentElement(Document& document, WebKitNamedFlow& namedFlow, Node& contentElement)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(document))
        didUnregisterNamedFlowContentElementImpl(*agents, document, namedFlow, contentElement);
}

inline void InspectorInstrumentation::mouseDidMoveOverElement(Page& page, const HitTestResult& result, unsigned modifierFlags)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    mouseDidMoveOverElementImpl(instrumentingAgents(page), result, modifierFlags);
}

inline bool InspectorInstrumentation::handleTouchEvent(Frame& frame, Node& node)
{
    FAST_RETURN_IF_NO_FRONTENDS(false);
    if (auto* agents = instrumentingAgents(frame))
        return handleTouchEventImpl(*agents, node);
    return false;
}

inline bool InspectorInstrumentation::handleMousePress(Frame& frame)
{
    FAST_RETURN_IF_NO_FRONTENDS(false);
    if (auto* agents = instrumentingAgents(frame))
        return handleMousePressImpl(*agents);
    return false;
}

inline bool InspectorInstrumentation::forcePseudoState(const Element& element, CSSSelector::PseudoClassType pseudoState)
{
    FAST_RETURN_IF_NO_FRONTENDS(false);
    if (auto* agents = instrumentingAgents(element.document()))
        return forcePseudoStateImpl(*agents, element, pseudoState);
    return false;
}

inline void InspectorInstrumentation::characterDataModified(Document& document, CharacterData& characterData)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(document))
        characterDataModifiedImpl(*agents, characterData);
}

inline void InspectorInstrumentation::willSendXMLHttpRequest(ScriptExecutionContext* context, const String& url)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(context))
        willSendXMLHttpRequestImpl(*agents, url);
}

inline void InspectorInstrumentation::willFetch(ScriptExecutionContext& context, const String& url)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(context))
        willFetchImpl(*agents, url);
}

inline void InspectorInstrumentation::didInstallTimer(ScriptExecutionContext& context, int timerId, Seconds timeout, bool singleShot)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(context))
        didInstallTimerImpl(*agents, timerId, timeout, singleShot, context);
}

inline void InspectorInstrumentation::didRemoveTimer(ScriptExecutionContext& context, int timerId)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(context))
        didRemoveTimerImpl(*agents, timerId, context);
}

inline void InspectorInstrumentation::didAddEventListener(EventTarget& target, const AtomString& eventType, EventListener& listener, bool capture)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(target.scriptExecutionContext()))
        didAddEventListenerImpl(*agents, target, eventType, listener, capture);
}

inline void InspectorInstrumentation::willRemoveEventListener(EventTarget& target, const AtomString& eventType, EventListener& listener, bool capture)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(target.scriptExecutionContext()))
        willRemoveEventListenerImpl(*agents, target, eventType, listener, capture);
}

inline bool InspectorInstrumentation::isEventListenerDisabled(EventTarget& target, const AtomString& eventType, EventListener& listener, bool capture)
{
    FAST_RETURN_IF_NO_FRONTENDS(false);
    if (auto* agents = instrumentingAgents(target.scriptExecutionContext()))
        return isEventListenerDisabledImpl(*agents, target, eventType, listener, capture);
    return false;
}

inline int InspectorInstrumentation::willPostMessage(Frame& frame)
{
    FAST_RETURN_IF_NO_FRONTENDS(0);
    if (auto* agents = instrumentingAgents(frame))
        return willPostMessageImpl(*agents);
    return 0;
}

inline void InspectorInstrumentation::didPostMessage(Frame& frame, int postMessageIdentifier, JSC::JSGlobalObject& state)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(frame))
        didPostMessageImpl(*agents, postMessageIdentifier, state);
}

inline void InspectorInstrumentation::didFailPostMessage(Frame& frame, int postMessageIdentifier)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(frame))
        didFailPostMessageImpl(*agents, postMessageIdentifier);
}

inline void InspectorInstrumentation::willDispatchPostMessage(Frame& frame, int postMessageIdentifier)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(frame))
        willDispatchPostMessageImpl(*agents, postMessageIdentifier);
}

inline void InspectorInstrumentation::didDispatchPostMessage(Frame& frame, int postMessageIdentifier)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(frame))
        didDispatchPostMessageImpl(*agents, postMessageIdentifier);
}

inline void InspectorInstrumentation::willCallFunction(ScriptExecutionContext* context, const String& scriptName, int scriptLine, int scriptColumn)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(context))
        willCallFunctionImpl(*agents, scriptName, scriptLine, scriptColumn, context);
}

inline void InspectorInstrumentation::didCallFunction(ScriptExecutionContext* context)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(context))
        didCallFunctionImpl(*agents, context);
}

inline void InspectorInstrumentation::willDispatchEvent(Document& document, const Event& event)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(document))
        willDispatchEventImpl(*agents, document, event);
}

inline void InspectorInstrumentation::didDispatchEvent(Document& document, const Event& event)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(document))
        didDispatchEventImpl(*agents, event);
}

inline void InspectorInstrumentation::willHandleEvent(ScriptExecutionContext& context, Event& event, const RegisteredEventListener& listener)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(context))
        return willHandleEventImpl(*agents, context, event, listener);
}

inline void InspectorInstrumentation::didHandleEvent(ScriptExecutionContext& context, Event& event, const RegisteredEventListener& listener)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(context))
        return didHandleEventImpl(*agents, context, event, listener);
}

inline void InspectorInstrumentation::willDispatchEventOnWindow(Frame* frame, const Event& event, DOMWindow& window)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(frame))
        willDispatchEventOnWindowImpl(*agents, event, window);
}

inline void InspectorInstrumentation::didDispatchEventOnWindow(Frame* frame, const Event& event)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(frame))
        didDispatchEventOnWindowImpl(*agents, event);
}

inline void InspectorInstrumentation::eventDidResetAfterDispatch(const Event& event)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());

    if (!is<Node>(event.target()))
        return;

    auto* node = downcast<Node>(event.target());
    if (auto* agents = instrumentingAgents(node->scriptExecutionContext()))
        return eventDidResetAfterDispatchImpl(*agents, event);
}

inline void InspectorInstrumentation::willEvaluateScript(Frame& frame, const String& url, int lineNumber, int columnNumber)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(frame))
        willEvaluateScriptImpl(*agents, frame, url, lineNumber, columnNumber);
}

inline void InspectorInstrumentation::didEvaluateScript(Frame& frame)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(frame))
        didEvaluateScriptImpl(*agents, frame);
}

inline void InspectorInstrumentation::willFireTimer(ScriptExecutionContext& context, int timerId, bool oneShot)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(context))
        willFireTimerImpl(*agents, timerId, oneShot, context);
}

inline void InspectorInstrumentation::didFireTimer(ScriptExecutionContext& context, int timerId, bool oneShot)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(context))
        didFireTimerImpl(*agents, timerId, oneShot);
}

inline void InspectorInstrumentation::didInvalidateLayout(Frame& frame)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(frame))
        didInvalidateLayoutImpl(*agents, frame);
}

inline void InspectorInstrumentation::willLayout(Frame& frame)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(frame))
        willLayoutImpl(*agents, frame);
}

inline void InspectorInstrumentation::didLayout(Frame& frame, RenderObject& root)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(frame))
        didLayoutImpl(*agents, root);
}

inline void InspectorInstrumentation::didScroll(Page& page)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    didScrollImpl(instrumentingAgents(page));
}

inline void InspectorInstrumentation::willComposite(Frame& frame)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(frame))
        willCompositeImpl(*agents, frame);
}

inline void InspectorInstrumentation::didComposite(Frame& frame)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(frame))
        didCompositeImpl(*agents);
}

inline void InspectorInstrumentation::willPaint(RenderObject& renderer)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(renderer))
        return willPaintImpl(*agents, renderer);
}

inline void InspectorInstrumentation::didPaint(RenderObject& renderer, const LayoutRect& rect)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(renderer))
        didPaintImpl(*agents, renderer, rect);
}

inline void InspectorInstrumentation::willRecalculateStyle(Document& document)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(document))
        willRecalculateStyleImpl(*agents, document);
}

inline void InspectorInstrumentation::didRecalculateStyle(Document& document)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(document))
        didRecalculateStyleImpl(*agents);
}

inline void InspectorInstrumentation::didScheduleStyleRecalculation(Document& document)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(document))
        didScheduleStyleRecalculationImpl(*agents, document);
}

inline void InspectorInstrumentation::applyUserAgentOverride(Frame& frame, String& userAgent)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(frame))
        applyUserAgentOverrideImpl(*agents, userAgent);
}

inline void InspectorInstrumentation::applyEmulatedMedia(Frame& frame, AtomString& media)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(frame))
        applyEmulatedMediaImpl(*agents, media);
}

inline void InspectorInstrumentation::flexibleBoxRendererBeganLayout(const RenderObject& renderer)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(renderer))
        flexibleBoxRendererBeganLayoutImpl(*agents, renderer);
}

inline void InspectorInstrumentation::flexibleBoxRendererWrappedToNextLine(const RenderObject& renderer, size_t lineStartItemIndex)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(renderer))
        flexibleBoxRendererWrappedToNextLineImpl(*agents, renderer, lineStartItemIndex);
}

inline void InspectorInstrumentation::willSendRequest(Frame* frame, ResourceLoaderIdentifier identifier, DocumentLoader* loader, ResourceRequest& request, const ResourceResponse& redirectResponse, const CachedResource* cachedResource, ResourceLoader* resourceLoader)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(frame))
        willSendRequestImpl(*agents, identifier, loader, request, redirectResponse, cachedResource, resourceLoader);
}

inline void InspectorInstrumentation::willSendRequest(WorkerOrWorkletGlobalScope& globalScope, ResourceLoaderIdentifier identifier, ResourceRequest& request)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    willSendRequestImpl(instrumentingAgents(globalScope), identifier, nullptr, request, ResourceResponse { }, nullptr, nullptr);
}

inline void InspectorInstrumentation::willSendRequestOfType(Frame* frame, ResourceLoaderIdentifier identifier, DocumentLoader* loader, ResourceRequest& request, LoadType loadType)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(frame))
        willSendRequestOfTypeImpl(*agents, identifier, loader, request, loadType);
}

inline void InspectorInstrumentation::didLoadResourceFromMemoryCache(Page& page, DocumentLoader* loader, CachedResource* resource)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    didLoadResourceFromMemoryCacheImpl(instrumentingAgents(page), loader, resource);
}

inline void InspectorInstrumentation::didReceiveResourceResponse(Frame& frame, ResourceLoaderIdentifier identifier, DocumentLoader* loader, const ResourceResponse& response, ResourceLoader* resourceLoader)
{
    if (auto* agents = instrumentingAgents(frame))
        didReceiveResourceResponseImpl(*agents, identifier, loader, response, resourceLoader);
}

inline void InspectorInstrumentation::didReceiveResourceResponse(WorkerOrWorkletGlobalScope& globalScope, ResourceLoaderIdentifier identifier, const ResourceResponse& response)
{
    didReceiveResourceResponseImpl(instrumentingAgents(globalScope), identifier, nullptr, response, nullptr);
}

inline void InspectorInstrumentation::didReceiveThreadableLoaderResponse(DocumentThreadableLoader& documentThreadableLoader, ResourceLoaderIdentifier identifier)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(documentThreadableLoader.document()))
        didReceiveThreadableLoaderResponseImpl(*agents, documentThreadableLoader, identifier);
}
    
inline void InspectorInstrumentation::didReceiveData(Frame* frame, ResourceLoaderIdentifier identifier, const SharedBuffer* buffer, int encodedDataLength)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(frame))
        didReceiveDataImpl(*agents, identifier, buffer, encodedDataLength);
}

inline void InspectorInstrumentation::didReceiveData(WorkerOrWorkletGlobalScope& globalScope, ResourceLoaderIdentifier identifier, const SharedBuffer& buffer)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    didReceiveDataImpl(instrumentingAgents(globalScope), identifier, &buffer, buffer.size());
}

inline void InspectorInstrumentation::didFinishLoading(Frame* frame, DocumentLoader* loader, ResourceLoaderIdentifier identifier, const NetworkLoadMetrics& networkLoadMetrics, ResourceLoader* resourceLoader)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(frame))
        didFinishLoadingImpl(*agents, identifier, loader, networkLoadMetrics, resourceLoader);
}

inline void InspectorInstrumentation::didFinishLoading(WorkerOrWorkletGlobalScope& globalScope, ResourceLoaderIdentifier identifier, const NetworkLoadMetrics& networkLoadMetrics)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    didFinishLoadingImpl(instrumentingAgents(globalScope), identifier, nullptr, networkLoadMetrics, nullptr);
}

inline void InspectorInstrumentation::didFailLoading(Frame* frame, DocumentLoader* loader, ResourceLoaderIdentifier identifier, const ResourceError& error)
{
    if (auto* agents = instrumentingAgents(frame))
        didFailLoadingImpl(*agents, identifier, loader, error);
}

inline void InspectorInstrumentation::didFailLoading(WorkerOrWorkletGlobalScope& globalScope, ResourceLoaderIdentifier identifier, const ResourceError& error)
{
    didFailLoadingImpl(instrumentingAgents(globalScope), identifier, nullptr, error);
}

inline void InspectorInstrumentation::continueAfterXFrameOptionsDenied(Frame& frame, ResourceLoaderIdentifier identifier, DocumentLoader& loader, const ResourceResponse& response)
{
    // Treat the same as didReceiveResponse.
    if (auto* agents = instrumentingAgents(frame))
        didReceiveResourceResponseImpl(*agents, identifier, &loader, response, nullptr);
}

inline void InspectorInstrumentation::continueWithPolicyDownload(Frame& frame, ResourceLoaderIdentifier identifier, DocumentLoader& loader, const ResourceResponse& response)
{
    // Treat the same as didReceiveResponse.
    if (auto* agents = instrumentingAgents(frame))
        didReceiveResourceResponseImpl(*agents, identifier, &loader, response, nullptr);
}

inline void InspectorInstrumentation::continueWithPolicyIgnore(Frame& frame, ResourceLoaderIdentifier identifier, DocumentLoader& loader, const ResourceResponse& response)
{
    // Treat the same as didReceiveResponse.
    if (auto* agents = instrumentingAgents(frame))
        didReceiveResourceResponseImpl(*agents, identifier, &loader, response, nullptr);
}

inline void InspectorInstrumentation::willLoadXHRSynchronously(ScriptExecutionContext* context)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(context))
        willLoadXHRSynchronouslyImpl(*agents);
}

inline void InspectorInstrumentation::didLoadXHRSynchronously(ScriptExecutionContext* context)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(context))
        didLoadXHRSynchronouslyImpl(*agents);
}

inline void InspectorInstrumentation::scriptImported(ScriptExecutionContext& context, ResourceLoaderIdentifier identifier, const String& sourceString)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(context))
        scriptImportedImpl(*agents, identifier, sourceString);
}

inline void InspectorInstrumentation::scriptExecutionBlockedByCSP(ScriptExecutionContext* context, const String& directiveText)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(context))
        scriptExecutionBlockedByCSPImpl(*agents, directiveText);
}

inline void InspectorInstrumentation::didReceiveScriptResponse(ScriptExecutionContext* context, ResourceLoaderIdentifier identifier)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(context))
        didReceiveScriptResponseImpl(*agents, identifier);
}

inline void InspectorInstrumentation::domContentLoadedEventFired(Frame& frame)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(frame))
        domContentLoadedEventFiredImpl(*agents, frame);
}

inline void InspectorInstrumentation::loadEventFired(Frame* frame)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(frame))
        loadEventFiredImpl(*agents, frame);
}

inline void InspectorInstrumentation::frameDetachedFromParent(Frame& frame)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(frame))
        frameDetachedFromParentImpl(*agents, frame);
}

inline void InspectorInstrumentation::didCommitLoad(Frame& frame, DocumentLoader* loader)
{
    if (auto* agents = instrumentingAgents(frame))
        didCommitLoadImpl(*agents, frame, loader);
}

inline void InspectorInstrumentation::frameDocumentUpdated(Frame& frame)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(frame))
        frameDocumentUpdatedImpl(*agents, frame);
}

inline void InspectorInstrumentation::loaderDetachedFromFrame(Frame& frame, DocumentLoader& loader)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(frame))
        loaderDetachedFromFrameImpl(*agents, loader);
}

inline void InspectorInstrumentation::frameStartedLoading(Frame& frame)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(frame))
        frameStartedLoadingImpl(*agents, frame);
}

inline void InspectorInstrumentation::frameStoppedLoading(Frame& frame)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(frame))
        frameStoppedLoadingImpl(*agents, frame);
}

inline void InspectorInstrumentation::frameScheduledNavigation(Frame& frame, Seconds delay)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(frame))
        frameScheduledNavigationImpl(*agents, frame, delay);
}

inline void InspectorInstrumentation::frameClearedScheduledNavigation(Frame& frame)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(frame))
        frameClearedScheduledNavigationImpl(*agents, frame);
}

#if ENABLE(DARK_MODE_CSS) || HAVE(OS_DARK_MODE_SUPPORT)
inline void InspectorInstrumentation::defaultAppearanceDidChange(Page& page, bool useDarkAppearance)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    defaultAppearanceDidChangeImpl(instrumentingAgents(page), useDarkAppearance);
}
#endif

inline void InspectorInstrumentation::willDestroyCachedResource(CachedResource& cachedResource)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    willDestroyCachedResourceImpl(cachedResource);
}

inline bool InspectorInstrumentation::willIntercept(const Frame* frame, const ResourceRequest& request)
{
    FAST_RETURN_IF_NO_FRONTENDS(false);
    if (auto* agents = instrumentingAgents(frame))
        return willInterceptImpl(*agents, request);
    return false;
}

inline bool InspectorInstrumentation::shouldInterceptRequest(const ResourceLoader& loader)
{
    ASSERT(InspectorInstrumentationPublic::hasFrontends());
    if (auto* agents = instrumentingAgents(loader.frame()))
        return shouldInterceptRequestImpl(*agents, loader);
    return false;
}

inline bool InspectorInstrumentation::shouldInterceptResponse(const Frame& frame, const ResourceResponse& response)
{
    ASSERT(InspectorInstrumentationPublic::hasFrontends());
    if (auto* agents = instrumentingAgents(frame))
        return shouldInterceptResponseImpl(*agents, response);
    return false;
}

inline void InspectorInstrumentation::interceptRequest(ResourceLoader& loader, Function<void(const ResourceRequest&)>&& handler)
{
    ASSERT(InspectorInstrumentation::shouldInterceptRequest(loader));
    if (auto* agents = instrumentingAgents(loader.frame()))
        interceptRequestImpl(*agents, loader, WTFMove(handler));
}

inline void InspectorInstrumentation::interceptResponse(const Frame& frame, const ResourceResponse& response, ResourceLoaderIdentifier identifier, CompletionHandler<void(const ResourceResponse&, RefPtr<FragmentedSharedBuffer>)>&& handler)
{
    ASSERT(InspectorInstrumentation::shouldInterceptResponse(frame, response));
    if (auto* agents = instrumentingAgents(frame))
        interceptResponseImpl(*agents, response, identifier, WTFMove(handler));
}

inline void InspectorInstrumentation::didOpenDatabase(Database& database)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(database.document()))
        didOpenDatabaseImpl(*agents, database);
}

inline void InspectorInstrumentation::didDispatchDOMStorageEvent(Page& page, const String& key, const String& oldValue, const String& newValue, StorageType storageType, const SecurityOrigin& securityOrigin)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    didDispatchDOMStorageEventImpl(instrumentingAgents(page), key, oldValue, newValue, storageType, securityOrigin);
}

inline bool InspectorInstrumentation::shouldWaitForDebuggerOnStart(ScriptExecutionContext& context)
{
    FAST_RETURN_IF_NO_FRONTENDS(false);
    if (auto* agents = instrumentingAgents(context))
        return shouldWaitForDebuggerOnStartImpl(*agents);
    return false;
}

inline void InspectorInstrumentation::workerStarted(WorkerInspectorProxy& proxy)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(proxy.scriptExecutionContext()))
        workerStartedImpl(*agents, proxy);
}

inline void InspectorInstrumentation::workerTerminated(WorkerInspectorProxy& proxy)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(proxy.scriptExecutionContext()))
        workerTerminatedImpl(*agents, proxy);
}

inline void InspectorInstrumentation::didCreateWebSocket(Document* document, WebSocketChannelIdentifier identifier, const URL& requestURL)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(document))
        didCreateWebSocketImpl(*agents, identifier, requestURL);
}

inline void InspectorInstrumentation::willSendWebSocketHandshakeRequest(Document* document, WebSocketChannelIdentifier identifier, const ResourceRequest& request)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(document))
        willSendWebSocketHandshakeRequestImpl(*agents, identifier, request);
}

inline void InspectorInstrumentation::didReceiveWebSocketHandshakeResponse(Document* document, WebSocketChannelIdentifier identifier, const ResourceResponse& response)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(document))
        didReceiveWebSocketHandshakeResponseImpl(*agents, identifier, response);
}

inline void InspectorInstrumentation::didCloseWebSocket(Document* document, WebSocketChannelIdentifier identifier)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(document))
        didCloseWebSocketImpl(*agents, identifier);
}

inline void InspectorInstrumentation::didReceiveWebSocketFrame(Document* document, WebSocketChannelIdentifier identifier, const WebSocketFrame& frame)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(document))
        didReceiveWebSocketFrameImpl(*agents, identifier, frame);
}

inline void InspectorInstrumentation::didReceiveWebSocketFrameError(Document* document, WebSocketChannelIdentifier identifier, const String& errorMessage)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(document))
        didReceiveWebSocketFrameErrorImpl(*agents, identifier, errorMessage);
}

inline void InspectorInstrumentation::didSendWebSocketFrame(Document* document, WebSocketChannelIdentifier identifier, const WebSocketFrame& frame)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(document))
        didSendWebSocketFrameImpl(*agents, identifier, frame);
}

#if ENABLE(RESOURCE_USAGE)
inline void InspectorInstrumentation::didHandleMemoryPressure(Page& page, Critical critical)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    didHandleMemoryPressureImpl(instrumentingAgents(page), critical);
}
#endif

inline void InspectorInstrumentation::didChangeCSSCanvasClientNodes(CanvasBase& canvasBase)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(canvasBase.scriptExecutionContext()))
        didChangeCSSCanvasClientNodesImpl(*agents, canvasBase);
}

inline void InspectorInstrumentation::didCreateCanvasRenderingContext(CanvasRenderingContext& context)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(context.canvasBase().scriptExecutionContext()))
        didCreateCanvasRenderingContextImpl(*agents, context);
}

inline void InspectorInstrumentation::didChangeCanvasMemory(CanvasRenderingContext& context)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(context.canvasBase().scriptExecutionContext()))
        didChangeCanvasMemoryImpl(*agents, context);
}

inline void InspectorInstrumentation::didFinishRecordingCanvasFrame(CanvasRenderingContext& context, bool forceDispatch)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(context.canvasBase().scriptExecutionContext()))
        didFinishRecordingCanvasFrameImpl(*agents, context, forceDispatch);
}

#if ENABLE(WEBGL)
inline void InspectorInstrumentation::didEnableExtension(WebGLRenderingContextBase& contextWebGLBase, const String& extension)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(contextWebGLBase.canvasBase().scriptExecutionContext()))
        didEnableExtensionImpl(*agents, contextWebGLBase, extension);
}

inline void InspectorInstrumentation::didCreateWebGLProgram(WebGLRenderingContextBase& contextWebGLBase, WebGLProgram& program)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(contextWebGLBase.canvasBase().scriptExecutionContext()))
        didCreateWebGLProgramImpl(*agents, contextWebGLBase, program);
}

inline void InspectorInstrumentation::willDestroyWebGLProgram(WebGLProgram& program)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(program.scriptExecutionContext()))
        willDestroyWebGLProgramImpl(*agents, program);
}

inline bool InspectorInstrumentation::isWebGLProgramDisabled(WebGLRenderingContextBase& contextWebGLBase, WebGLProgram& program)
{
    FAST_RETURN_IF_NO_FRONTENDS(false);
    if (auto* agents = instrumentingAgents(contextWebGLBase.canvasBase().scriptExecutionContext()))
        return isWebGLProgramDisabledImpl(*agents, program);
    return false;
}

inline bool InspectorInstrumentation::isWebGLProgramHighlighted(WebGLRenderingContextBase& contextWebGLBase, WebGLProgram& program)
{
    FAST_RETURN_IF_NO_FRONTENDS(false);
    if (auto* agents = instrumentingAgents(contextWebGLBase.canvasBase().scriptExecutionContext()))
        return isWebGLProgramHighlightedImpl(*agents, program);
    return false;
}
#endif

inline void InspectorInstrumentation::willApplyKeyframeEffect(const Styleable& target, KeyframeEffect& effect, ComputedEffectTiming computedTiming)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(target.element.document()))
        willApplyKeyframeEffectImpl(*agents, target, effect, computedTiming);
}

inline void InspectorInstrumentation::didChangeWebAnimationName(WebAnimation& animation)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(animation.scriptExecutionContext()))
        didChangeWebAnimationNameImpl(*agents, animation);
}

inline void InspectorInstrumentation::didSetWebAnimationEffect(WebAnimation& animation)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(animation.scriptExecutionContext()))
        didSetWebAnimationEffectImpl(*agents, animation);
}

inline void InspectorInstrumentation::didChangeWebAnimationEffectTiming(WebAnimation& animation)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(animation.scriptExecutionContext()))
        didChangeWebAnimationEffectTimingImpl(*agents, animation);
}

inline void InspectorInstrumentation::didChangeWebAnimationEffectTarget(WebAnimation& animation)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(animation.scriptExecutionContext()))
        didChangeWebAnimationEffectTargetImpl(*agents, animation);
}

inline void InspectorInstrumentation::didCreateWebAnimation(WebAnimation& animation)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(animation.scriptExecutionContext()))
        didCreateWebAnimationImpl(*agents, animation);
}

inline void InspectorInstrumentation::willDestroyWebAnimation(WebAnimation& animation)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(animation.scriptExecutionContext()))
        willDestroyWebAnimationImpl(*agents, animation);
}

inline void InspectorInstrumentation::networkStateChanged(Page& page)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    networkStateChangedImpl(instrumentingAgents(page));
}

inline void InspectorInstrumentation::updateApplicationCacheStatus(Frame* frame)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(frame))
        updateApplicationCacheStatusImpl(*agents, *frame);
}

inline void InspectorInstrumentation::addMessageToConsole(Page& page, std::unique_ptr<Inspector::ConsoleMessage> message)
{
    addMessageToConsoleImpl(instrumentingAgents(page), WTFMove(message));
}

inline void InspectorInstrumentation::addMessageToConsole(WorkerOrWorkletGlobalScope& globalScope, std::unique_ptr<Inspector::ConsoleMessage> message)
{
    addMessageToConsoleImpl(instrumentingAgents(globalScope), WTFMove(message));
}

inline void InspectorInstrumentation::consoleCount(Page& page, JSC::JSGlobalObject* state, const String& label)
{
    consoleCountImpl(instrumentingAgents(page), state, label);
}

inline void InspectorInstrumentation::consoleCount(WorkerOrWorkletGlobalScope& globalScope, JSC::JSGlobalObject* state, const String& label)
{
    consoleCountImpl(instrumentingAgents(globalScope), state, label);
}

inline void InspectorInstrumentation::consoleCountReset(Page& page, JSC::JSGlobalObject* state, const String& label)
{
    consoleCountResetImpl(instrumentingAgents(page), state, label);
}

inline void InspectorInstrumentation::consoleCountReset(WorkerOrWorkletGlobalScope& globalScope, JSC::JSGlobalObject* state, const String& label)
{
    consoleCountResetImpl(instrumentingAgents(globalScope), state, label);
}

inline void InspectorInstrumentation::takeHeapSnapshot(Frame& frame, const String& title)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(frame))
        takeHeapSnapshotImpl(*agents, title);
}

inline void InspectorInstrumentation::startConsoleTiming(Frame& frame, JSC::JSGlobalObject* exec, const String& label)
{
    if (auto* agents = instrumentingAgents(frame))
        startConsoleTimingImpl(*agents, frame, exec, label);
}

inline void InspectorInstrumentation::startConsoleTiming(WorkerOrWorkletGlobalScope& globalScope, JSC::JSGlobalObject* exec, const String& label)
{
    startConsoleTimingImpl(instrumentingAgents(globalScope), exec, label);
}

inline void InspectorInstrumentation::logConsoleTiming(Frame& frame, JSC::JSGlobalObject* exec, const String& label, Ref<Inspector::ScriptArguments>&& arguments)
{
    if (auto* agents = instrumentingAgents(frame))
        logConsoleTimingImpl(*agents, exec, label, WTFMove(arguments));
}

inline void InspectorInstrumentation::logConsoleTiming(WorkerOrWorkletGlobalScope& globalScope, JSC::JSGlobalObject* exec, const String& label, Ref<Inspector::ScriptArguments>&& arguments)
{
    logConsoleTimingImpl(instrumentingAgents(globalScope), exec, label, WTFMove(arguments));
}

inline void InspectorInstrumentation::stopConsoleTiming(Frame& frame, JSC::JSGlobalObject* exec, const String& label)
{
    if (auto* agents = instrumentingAgents(frame))
        stopConsoleTimingImpl(*agents, frame, exec, label);
}

inline void InspectorInstrumentation::stopConsoleTiming(WorkerOrWorkletGlobalScope& globalScope, JSC::JSGlobalObject* exec, const String& label)
{
    stopConsoleTimingImpl(instrumentingAgents(globalScope), exec, label);
}

inline void InspectorInstrumentation::consoleTimeStamp(Frame& frame, Ref<Inspector::ScriptArguments>&& arguments)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(frame))
        consoleTimeStampImpl(*agents, frame, WTFMove(arguments));
}

inline void InspectorInstrumentation::startProfiling(Page& page, JSC::JSGlobalObject* exec, const String &title)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    startProfilingImpl(instrumentingAgents(page), exec, title);
}

inline void InspectorInstrumentation::stopProfiling(Page& page, JSC::JSGlobalObject* exec, const String &title)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    stopProfilingImpl(instrumentingAgents(page), exec, title);
}

inline void InspectorInstrumentation::consoleStartRecordingCanvas(CanvasRenderingContext& context, JSC::JSGlobalObject& exec, JSC::JSObject* options)
{
    if (auto* agents = instrumentingAgents(context.canvasBase().scriptExecutionContext()))
        consoleStartRecordingCanvasImpl(*agents, context, exec, options);
}

inline void InspectorInstrumentation::consoleStopRecordingCanvas(CanvasRenderingContext& context)
{
    if (auto* agents = instrumentingAgents(context.canvasBase().scriptExecutionContext()))
        consoleStopRecordingCanvasImpl(*agents, context);
}

inline void InspectorInstrumentation::didRequestAnimationFrame(Document& document, int callbackId)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(document))
        didRequestAnimationFrameImpl(*agents, callbackId, document);
}

inline void InspectorInstrumentation::didCancelAnimationFrame(Document& document, int callbackId)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(document))
        didCancelAnimationFrameImpl(*agents, callbackId, document);
}

inline void InspectorInstrumentation::willFireAnimationFrame(Document& document, int callbackId)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(document))
        willFireAnimationFrameImpl(*agents, callbackId, document);
}

inline void InspectorInstrumentation::didFireAnimationFrame(Document& document, int callbackId)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(document))
        didFireAnimationFrameImpl(*agents, callbackId);
}

inline void InspectorInstrumentation::willFireObserverCallback(ScriptExecutionContext& context, const String& callbackType)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(context))
        willFireObserverCallbackImpl(*agents, callbackType, context);
}

inline void InspectorInstrumentation::didFireObserverCallback(ScriptExecutionContext& context)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(context))
        didFireObserverCallbackImpl(*agents);
}

inline void InspectorInstrumentation::layerTreeDidChange(Page* page)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(page))
        layerTreeDidChangeImpl(*agents);
}

inline void InspectorInstrumentation::renderLayerDestroyed(Page* page, const RenderLayer& renderLayer)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* agents = instrumentingAgents(page))
        renderLayerDestroyedImpl(*agents, renderLayer);
}

inline InstrumentingAgents* InspectorInstrumentation::instrumentingAgents(ScriptExecutionContext* context)
{
    return context ? instrumentingAgents(*context) : nullptr;
}

inline InstrumentingAgents* InspectorInstrumentation::instrumentingAgents(const Frame* frame)
{
    return frame ? instrumentingAgents(*frame) : nullptr;
}

inline InstrumentingAgents* InspectorInstrumentation::instrumentingAgents(const Frame& frame)
{
    return instrumentingAgents(frame.page());
}

inline InstrumentingAgents* InspectorInstrumentation::instrumentingAgents(Document* document)
{
    return document ? instrumentingAgents(*document) : nullptr;
}

inline InstrumentingAgents* InspectorInstrumentation::instrumentingAgents(Document& document)
{
    Page* page = document.page();
    if (!page && document.templateDocumentHost())
        page = document.templateDocumentHost()->page();
    return instrumentingAgents(page);
}

inline InstrumentingAgents* InspectorInstrumentation::instrumentingAgents(Page* page)
{
    return page ? &instrumentingAgents(*page) : nullptr;
}

inline InstrumentingAgents* InspectorInstrumentation::instrumentingAgents(WorkerOrWorkletGlobalScope* globalScope)
{
    return globalScope ? &instrumentingAgents(*globalScope) : nullptr;
}

inline void InspectorInstrumentation::frontendCreated()
{
    ASSERT(isMainThread());
    int frontendCount = ++InspectorInstrumentationPublic::s_frontendCounter;

    if (frontendCount == 1)
        InspectorInstrumentation::firstFrontendCreated();
}

inline void InspectorInstrumentation::frontendDeleted()
{
    ASSERT(isMainThread());
    int frontendCount = --InspectorInstrumentationPublic::s_frontendCounter;

    if (!frontendCount)
        InspectorInstrumentation::lastFrontendDeleted();
}

} // namespace WebCore
