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
#include "CallTracerTypes.h"
#include "CanvasBase.h"
#include "CanvasRenderingContext.h"
#include "DocumentThreadableLoader.h"
#include "Element.h"
#include "EventTarget.h"
#include "FormData.h"
#include "Frame.h"
#include "HitTestResult.h"
#include "InspectorController.h"
#include "InspectorInstrumentationCookie.h"
#include "OffscreenCanvas.h"
#include "Page.h"
#include "StorageArea.h"
#include "WorkerGlobalScope.h"
#include "WorkerInspectorController.h"
#include <JavaScriptCore/JSCInlines.h>
#include <wtf/MemoryPressureHandler.h>
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
class Database;
class Document;
class DocumentLoader;
class EventListener;
class HTTPHeaderMap;
class InspectorTimelineAgent;
class InstrumentingAgents;
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
class TimerBase;
#if ENABLE(WEBGL)
class WebGLProgram;
#endif
class WebKitNamedFlow;
class WorkerInspectorProxy;

enum class StorageType;

struct WebSocketFrame;

#define FAST_RETURN_IF_NO_FRONTENDS(value) if (LIKELY(!hasFrontends())) return value;

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
    static void willModifyDOMAttr(Document&, Element&, const AtomicString& oldValue, const AtomicString& newValue);
    static void didModifyDOMAttr(Document&, Element&, const AtomicString& name, const AtomicString& value);
    static void didRemoveDOMAttr(Document&, Element&, const AtomicString& name);
    static void characterDataModified(Document&, CharacterData&);
    static void didInvalidateStyleAttr(Document&, Node&);
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

    static void didPostMessage(Frame&, TimerBase&, JSC::ExecState&);
    static void didFailPostMessage(Frame&, TimerBase&);
    static void willDispatchPostMessage(Frame&, TimerBase&);
    static void didDispatchPostMessage(Frame&, TimerBase&);

    static InspectorInstrumentationCookie willCallFunction(ScriptExecutionContext*, const String& scriptName, int scriptLine, int scriptColumn);
    static void didCallFunction(const InspectorInstrumentationCookie&, ScriptExecutionContext*);
    static void didAddEventListener(EventTarget&, const AtomicString& eventType, EventListener&, bool capture);
    static void willRemoveEventListener(EventTarget&, const AtomicString& eventType, EventListener&, bool capture);
    static bool isEventListenerDisabled(EventTarget&, const AtomicString& eventType, EventListener&, bool capture);
    static InspectorInstrumentationCookie willDispatchEvent(Document&, const Event&, bool hasEventListeners);
    static void didDispatchEvent(const InspectorInstrumentationCookie&);
    static void willHandleEvent(ScriptExecutionContext&, const Event&, const RegisteredEventListener&);
    static void didHandleEvent(ScriptExecutionContext&);
    static InspectorInstrumentationCookie willDispatchEventOnWindow(Frame*, const Event&, DOMWindow&);
    static void didDispatchEventOnWindow(const InspectorInstrumentationCookie&);
    static void eventDidResetAfterDispatch(const Event&);
    static InspectorInstrumentationCookie willEvaluateScript(Frame&, const String& url, int lineNumber, int columnNumber);
    static void didEvaluateScript(const InspectorInstrumentationCookie&, Frame&);
    static InspectorInstrumentationCookie willFireTimer(ScriptExecutionContext&, int timerId, bool oneShot);
    static void didFireTimer(const InspectorInstrumentationCookie&);
    static void didInvalidateLayout(Frame&);
    static InspectorInstrumentationCookie willLayout(Frame&);
    static void didLayout(const InspectorInstrumentationCookie&, RenderObject&);
    static void didScroll(Page&);
    static void willComposite(Frame&);
    static void didComposite(Frame&);
    static void willPaint(RenderObject&);
    static void didPaint(RenderObject&, const LayoutRect&);
    static InspectorInstrumentationCookie willRecalculateStyle(Document&);
    static void didRecalculateStyle(const InspectorInstrumentationCookie&);
    static void didScheduleStyleRecalculation(Document&);
    static void applyUserAgentOverride(Frame&, String&);
    static void applyEmulatedMedia(Frame&, String&);

    static void willSendRequest(Frame*, unsigned long identifier, DocumentLoader*, ResourceRequest&, const ResourceResponse& redirectResponse);
    static void didLoadResourceFromMemoryCache(Page&, DocumentLoader*, CachedResource*);
    static void didReceiveResourceResponse(Frame&, unsigned long identifier, DocumentLoader*, const ResourceResponse&, ResourceLoader*);
    static void didReceiveThreadableLoaderResponse(DocumentThreadableLoader&, unsigned long identifier);
    static void didReceiveData(Frame*, unsigned long identifier, const char* data, int dataLength, int encodedDataLength);
    static void didFinishLoading(Frame*, DocumentLoader*, unsigned long identifier, const NetworkLoadMetrics&, ResourceLoader*);
    static void didFailLoading(Frame*, DocumentLoader*, unsigned long identifier, const ResourceError&);

    static void willSendRequest(WorkerGlobalScope&, unsigned long identifier, ResourceRequest&);
    static void didReceiveResourceResponse(WorkerGlobalScope&, unsigned long identifier, const ResourceResponse&);
    static void didReceiveData(WorkerGlobalScope&, unsigned long identifier, const char* data, int dataLength);
    static void didFinishLoading(WorkerGlobalScope&, unsigned long identifier, const NetworkLoadMetrics&);
    static void didFailLoading(WorkerGlobalScope&, unsigned long identifier, const ResourceError&);

    // Some network requests do not go through the normal network loading path.
    // These network requests have to issue their own willSendRequest / didReceiveResponse / didFinishLoading / didFailLoading
    // instrumentation calls. Some of these loads are for resources that lack a CachedResource::Type.
    enum class LoadType { Ping, Beacon };
    static void willSendRequestOfType(Frame*, unsigned long identifier, DocumentLoader*, ResourceRequest&, LoadType);

    static void continueAfterXFrameOptionsDenied(Frame&, unsigned long identifier, DocumentLoader&, const ResourceResponse&);
    static void continueWithPolicyDownload(Frame&, unsigned long identifier, DocumentLoader&, const ResourceResponse&);
    static void continueWithPolicyIgnore(Frame&, unsigned long identifier, DocumentLoader&, const ResourceResponse&);
    static void willLoadXHRSynchronously(ScriptExecutionContext*);
    static void didLoadXHRSynchronously(ScriptExecutionContext*);
    static void scriptImported(ScriptExecutionContext&, unsigned long identifier, const String& sourceString);
    static void scriptExecutionBlockedByCSP(ScriptExecutionContext*, const String& directiveText);
    static void didReceiveScriptResponse(ScriptExecutionContext*, unsigned long identifier);
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
    static void defaultAppearanceDidChange(Page&, bool useDarkAppearance);
    static void willDestroyCachedResource(CachedResource&);

    static void addMessageToConsole(Page&, std::unique_ptr<Inspector::ConsoleMessage>);
    static void addMessageToConsole(WorkerGlobalScope&, std::unique_ptr<Inspector::ConsoleMessage>);

    static void consoleCount(Page&, JSC::ExecState*, Ref<Inspector::ScriptArguments>&&);
    static void consoleCount(WorkerGlobalScope&, JSC::ExecState*, Ref<Inspector::ScriptArguments>&&);
    static void takeHeapSnapshot(Frame&, const String& title);
    static void startConsoleTiming(Frame&, const String& title);
    static void startConsoleTiming(WorkerGlobalScope&, const String& title);
    static void stopConsoleTiming(Frame&, const String& title, Ref<Inspector::ScriptCallStack>&&);
    static void stopConsoleTiming(WorkerGlobalScope&, const String& title, Ref<Inspector::ScriptCallStack>&&);
    static void consoleTimeStamp(Frame&, Ref<Inspector::ScriptArguments>&&);
    static void startProfiling(Page&, JSC::ExecState*, const String& title);
    static void stopProfiling(Page&, JSC::ExecState*, const String& title);
    static void consoleStartRecordingCanvas(CanvasRenderingContext&, JSC::ExecState&, JSC::JSObject* options);

    static void didRequestAnimationFrame(Document&, int callbackId);
    static void didCancelAnimationFrame(Document&, int callbackId);
    static InspectorInstrumentationCookie willFireAnimationFrame(Document&, int callbackId);
    static void didFireAnimationFrame(const InspectorInstrumentationCookie&);

    static InspectorInstrumentationCookie willFireObserverCallback(ScriptExecutionContext&, const String& callbackType);
    static void didFireObserverCallback(const InspectorInstrumentationCookie&);

    static void didOpenDatabase(ScriptExecutionContext*, RefPtr<Database>&&, const String& domain, const String& name, const String& version);

    static void didDispatchDOMStorageEvent(Page&, const String& key, const String& oldValue, const String& newValue, StorageType, SecurityOrigin*);

    static bool shouldWaitForDebuggerOnStart(ScriptExecutionContext&);
    static void workerStarted(ScriptExecutionContext&, WorkerInspectorProxy*, const URL&);
    static void workerTerminated(ScriptExecutionContext&, WorkerInspectorProxy*);

    static void didCreateWebSocket(Document*, unsigned long identifier, const URL& requestURL);
    static void willSendWebSocketHandshakeRequest(Document*, unsigned long identifier, const ResourceRequest&);
    static void didReceiveWebSocketHandshakeResponse(Document*, unsigned long identifier, const ResourceResponse&);
    static void didCloseWebSocket(Document*, unsigned long identifier);
    static void didReceiveWebSocketFrame(Document*, unsigned long identifier, const WebSocketFrame&);
    static void didSendWebSocketFrame(Document*, unsigned long identifier, const WebSocketFrame&);
    static void didReceiveWebSocketFrameError(Document*, unsigned long identifier, const String& errorMessage);

#if ENABLE(RESOURCE_USAGE)
    static void didHandleMemoryPressure(Page&, Critical);
#endif

    static void didChangeCSSCanvasClientNodes(CanvasBase&);
    static void didCreateCanvasRenderingContext(CanvasRenderingContext&);
    static void didChangeCanvasMemory(CanvasRenderingContext&);
    static void recordCanvasAction(CanvasRenderingContext&, const String&, Vector<RecordCanvasActionVariant>&& = { });
    static void didFinishRecordingCanvasFrame(CanvasRenderingContext&, bool forceDispatch = false);
#if ENABLE(WEBGL)
    static void didEnableExtension(WebGLRenderingContextBase&, const String&);
    static void didCreateProgram(WebGLRenderingContextBase&, WebGLProgram&);
    static void willDeleteProgram(WebGLRenderingContextBase&, WebGLProgram&);
    static bool isShaderProgramDisabled(WebGLRenderingContextBase&, WebGLProgram&);
    static bool isShaderProgramHighlighted(WebGLRenderingContextBase&, WebGLProgram&);
#endif

    static void networkStateChanged(Page&);
    static void updateApplicationCacheStatus(Frame*);

    static void layerTreeDidChange(Page*);
    static void renderLayerDestroyed(Page*, const RenderLayer&);

    static void frontendCreated();
    static void frontendDeleted();
    static bool hasFrontends() { return s_frontendCounter; }

    static void firstFrontendCreated();
    static void lastFrontendDeleted();

    static bool consoleAgentEnabled(ScriptExecutionContext*);
    static bool timelineAgentEnabled(ScriptExecutionContext*);

    static InstrumentingAgents* instrumentingAgentsForPage(Page*);

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
    static void willModifyDOMAttrImpl(InstrumentingAgents&, Element&, const AtomicString& oldValue, const AtomicString& newValue);
    static void didModifyDOMAttrImpl(InstrumentingAgents&, Element&, const AtomicString& name, const AtomicString& value);
    static void didRemoveDOMAttrImpl(InstrumentingAgents&, Element&, const AtomicString& name);
    static void characterDataModifiedImpl(InstrumentingAgents&, CharacterData&);
    static void didInvalidateStyleAttrImpl(InstrumentingAgents&, Node&);
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

    static void didPostMessageImpl(InstrumentingAgents&, const TimerBase&, JSC::ExecState&);
    static void didFailPostMessageImpl(InstrumentingAgents&, const TimerBase&);
    static void willDispatchPostMessageImpl(InstrumentingAgents&, const TimerBase&);
    static void didDispatchPostMessageImpl(InstrumentingAgents&, const TimerBase&);

    static InspectorInstrumentationCookie willCallFunctionImpl(InstrumentingAgents&, const String& scriptName, int scriptLine, int scriptColumn, ScriptExecutionContext*);
    static void didCallFunctionImpl(const InspectorInstrumentationCookie&, ScriptExecutionContext*);
    static void didAddEventListenerImpl(InstrumentingAgents&, EventTarget&, const AtomicString& eventType, EventListener&, bool capture);
    static void willRemoveEventListenerImpl(InstrumentingAgents&, EventTarget&, const AtomicString& eventType, EventListener&, bool capture);
    static bool isEventListenerDisabledImpl(InstrumentingAgents&, EventTarget&, const AtomicString& eventType, EventListener&, bool capture);
    static InspectorInstrumentationCookie willDispatchEventImpl(InstrumentingAgents&, Document&, const Event&, bool hasEventListeners);
    static void willHandleEventImpl(InstrumentingAgents&, const Event&, const RegisteredEventListener&);
    static void didHandleEventImpl(InstrumentingAgents&);
    static void didDispatchEventImpl(const InspectorInstrumentationCookie&);
    static InspectorInstrumentationCookie willDispatchEventOnWindowImpl(InstrumentingAgents&, const Event&, DOMWindow&);
    static void didDispatchEventOnWindowImpl(const InspectorInstrumentationCookie&);
    static void eventDidResetAfterDispatchImpl(InstrumentingAgents&, const Event&);
    static InspectorInstrumentationCookie willEvaluateScriptImpl(InstrumentingAgents&, Frame&, const String& url, int lineNumber, int columnNumber);
    static void didEvaluateScriptImpl(const InspectorInstrumentationCookie&, Frame&);
    static InspectorInstrumentationCookie willFireTimerImpl(InstrumentingAgents&, int timerId, bool oneShot, ScriptExecutionContext&);
    static void didFireTimerImpl(const InspectorInstrumentationCookie&);
    static void didInvalidateLayoutImpl(InstrumentingAgents&, Frame&);
    static InspectorInstrumentationCookie willLayoutImpl(InstrumentingAgents&, Frame&);
    static void didLayoutImpl(const InspectorInstrumentationCookie&, RenderObject&);
    static void didScrollImpl(InstrumentingAgents&);
    static void willCompositeImpl(InstrumentingAgents&, Frame&);
    static void didCompositeImpl(InstrumentingAgents&);
    static void willPaintImpl(InstrumentingAgents&, RenderObject&);
    static void didPaintImpl(InstrumentingAgents&, RenderObject&, const LayoutRect&);
    static InspectorInstrumentationCookie willRecalculateStyleImpl(InstrumentingAgents&, Document&);
    static void didRecalculateStyleImpl(const InspectorInstrumentationCookie&);
    static void didScheduleStyleRecalculationImpl(InstrumentingAgents&, Document&);
    static void applyUserAgentOverrideImpl(InstrumentingAgents&, String&);
    static void applyEmulatedMediaImpl(InstrumentingAgents&, String&);

    static void willSendRequestImpl(InstrumentingAgents&, unsigned long identifier, DocumentLoader*, ResourceRequest&, const ResourceResponse& redirectResponse);
    static void willSendRequestOfTypeImpl(InstrumentingAgents&, unsigned long identifier, DocumentLoader*, ResourceRequest&, LoadType);
    static void markResourceAsCachedImpl(InstrumentingAgents&, unsigned long identifier);
    static void didLoadResourceFromMemoryCacheImpl(InstrumentingAgents&, DocumentLoader*, CachedResource*);
    static void didReceiveResourceResponseImpl(InstrumentingAgents&, unsigned long identifier, DocumentLoader*, const ResourceResponse&, ResourceLoader*);
    static void didReceiveThreadableLoaderResponseImpl(InstrumentingAgents&, DocumentThreadableLoader&, unsigned long identifier);
    static void didReceiveDataImpl(InstrumentingAgents&, unsigned long identifier, const char* data, int dataLength, int encodedDataLength);
    static void didFinishLoadingImpl(InstrumentingAgents&, unsigned long identifier, DocumentLoader*, const NetworkLoadMetrics&, ResourceLoader*);
    static void didFailLoadingImpl(InstrumentingAgents&, unsigned long identifier, DocumentLoader*, const ResourceError&);
    static void willLoadXHRSynchronouslyImpl(InstrumentingAgents&);
    static void didLoadXHRSynchronouslyImpl(InstrumentingAgents&);
    static void scriptImportedImpl(InstrumentingAgents&, unsigned long identifier, const String& sourceString);
    static void scriptExecutionBlockedByCSPImpl(InstrumentingAgents&, const String& directiveText);
    static void didReceiveScriptResponseImpl(InstrumentingAgents&, unsigned long identifier);
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
    static void defaultAppearanceDidChangeImpl(InstrumentingAgents&, bool useDarkAppearance);
    static void willDestroyCachedResourceImpl(CachedResource&);

    static void addMessageToConsoleImpl(InstrumentingAgents&, std::unique_ptr<Inspector::ConsoleMessage>);

    static void consoleCountImpl(InstrumentingAgents&, JSC::ExecState*, Ref<Inspector::ScriptArguments>&&);
    static void takeHeapSnapshotImpl(InstrumentingAgents&, const String& title);
    static void startConsoleTimingImpl(InstrumentingAgents&, Frame&, const String& title);
    static void startConsoleTimingImpl(InstrumentingAgents&, const String& title);
    static void stopConsoleTimingImpl(InstrumentingAgents&, Frame&, const String& title, Ref<Inspector::ScriptCallStack>&&);
    static void stopConsoleTimingImpl(InstrumentingAgents&, const String& title, Ref<Inspector::ScriptCallStack>&&);
    static void consoleTimeStampImpl(InstrumentingAgents&, Frame&, Ref<Inspector::ScriptArguments>&&);
    static void startProfilingImpl(InstrumentingAgents&, JSC::ExecState*, const String& title);
    static void stopProfilingImpl(InstrumentingAgents&, JSC::ExecState*, const String& title);
    static void consoleStartRecordingCanvasImpl(InstrumentingAgents&, CanvasRenderingContext&, JSC::ExecState&, JSC::JSObject* options);

    static void didRequestAnimationFrameImpl(InstrumentingAgents&, int callbackId, Document&);
    static void didCancelAnimationFrameImpl(InstrumentingAgents&, int callbackId, Document&);
    static InspectorInstrumentationCookie willFireAnimationFrameImpl(InstrumentingAgents&, int callbackId, Document&);
    static void didFireAnimationFrameImpl(const InspectorInstrumentationCookie&);

    static InspectorInstrumentationCookie willFireObserverCallbackImpl(InstrumentingAgents&, const String&, ScriptExecutionContext&);
    static void didFireObserverCallbackImpl(const InspectorInstrumentationCookie&);

    static void didOpenDatabaseImpl(InstrumentingAgents&, RefPtr<Database>&&, const String& domain, const String& name, const String& version);

    static void didDispatchDOMStorageEventImpl(InstrumentingAgents&, const String& key, const String& oldValue, const String& newValue, StorageType, SecurityOrigin*);

    static bool shouldWaitForDebuggerOnStartImpl(InstrumentingAgents&);
    static void workerStartedImpl(InstrumentingAgents&, WorkerInspectorProxy*, const URL&);
    static void workerTerminatedImpl(InstrumentingAgents&, WorkerInspectorProxy*);

    static void didCreateWebSocketImpl(InstrumentingAgents&, unsigned long identifier, const URL& requestURL);
    static void willSendWebSocketHandshakeRequestImpl(InstrumentingAgents&, unsigned long identifier, const ResourceRequest&);
    static void didReceiveWebSocketHandshakeResponseImpl(InstrumentingAgents&, unsigned long identifier, const ResourceResponse&);
    static void didCloseWebSocketImpl(InstrumentingAgents&, unsigned long identifier);
    static void didReceiveWebSocketFrameImpl(InstrumentingAgents&, unsigned long identifier, const WebSocketFrame&);
    static void didSendWebSocketFrameImpl(InstrumentingAgents&, unsigned long identifier, const WebSocketFrame&);
    static void didReceiveWebSocketFrameErrorImpl(InstrumentingAgents&, unsigned long identifier, const String&);

#if ENABLE(RESOURCE_USAGE)
    static void didHandleMemoryPressureImpl(InstrumentingAgents&, Critical);
#endif

    static void networkStateChangedImpl(InstrumentingAgents&);
    static void updateApplicationCacheStatusImpl(InstrumentingAgents&, Frame&);

    static void didChangeCSSCanvasClientNodesImpl(InstrumentingAgents&, CanvasBase&);
    static void didCreateCanvasRenderingContextImpl(InstrumentingAgents&, CanvasRenderingContext&);
    static void didChangeCanvasMemoryImpl(InstrumentingAgents&, CanvasRenderingContext&);
    static void recordCanvasActionImpl(InstrumentingAgents&, CanvasRenderingContext&, const String&, Vector<RecordCanvasActionVariant>&& = { });
    static void didFinishRecordingCanvasFrameImpl(InstrumentingAgents&, CanvasRenderingContext&, bool forceDispatch = false);
#if ENABLE(WEBGL)
    static void didEnableExtensionImpl(InstrumentingAgents&, WebGLRenderingContextBase&, const String&);
    static void didCreateProgramImpl(InstrumentingAgents&, WebGLRenderingContextBase&, WebGLProgram&);
    static void willDeleteProgramImpl(InstrumentingAgents&, WebGLProgram&);
    static bool isShaderProgramDisabledImpl(InstrumentingAgents&, WebGLProgram&);
    static bool isShaderProgramHighlightedImpl(InstrumentingAgents&, WebGLProgram&);
#endif

    static void layerTreeDidChangeImpl(InstrumentingAgents&);
    static void renderLayerDestroyedImpl(InstrumentingAgents&, const RenderLayer&);

    static InstrumentingAgents& instrumentingAgentsForPage(Page&);
    static InstrumentingAgents& instrumentingAgentsForWorkerGlobalScope(WorkerGlobalScope&);

    static InstrumentingAgents* instrumentingAgentsForFrame(Frame&);
    static InstrumentingAgents* instrumentingAgentsForFrame(Frame*);
    static InstrumentingAgents* instrumentingAgentsForContext(ScriptExecutionContext*);
    static InstrumentingAgents* instrumentingAgentsForContext(ScriptExecutionContext&);
    static InstrumentingAgents* instrumentingAgentsForDocument(Document&);
    static InstrumentingAgents* instrumentingAgentsForDocument(Document*);
    static InstrumentingAgents* instrumentingAgentsForRenderer(RenderObject&);
    static InstrumentingAgents* instrumentingAgentsForWorkerGlobalScope(WorkerGlobalScope*);

    static InspectorTimelineAgent* retrieveTimelineAgent(const InspectorInstrumentationCookie&);

    WEBCORE_EXPORT static int s_frontendCounter;
};

inline void InspectorInstrumentation::didClearWindowObjectInWorld(Frame& frame, DOMWrapperWorld& world)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        didClearWindowObjectInWorldImpl(*instrumentingAgents, frame, world);
}

inline bool InspectorInstrumentation::isDebuggerPaused(Frame* frame)
{
    FAST_RETURN_IF_NO_FRONTENDS(false);
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        return isDebuggerPausedImpl(*instrumentingAgents);
    return false;
}

inline int InspectorInstrumentation::identifierForNode(Node& node)
{
    FAST_RETURN_IF_NO_FRONTENDS(0);
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(node.document()))
        return identifierForNodeImpl(*instrumentingAgents, node);
    return 0;
}

inline void InspectorInstrumentation::addEventListenersToNode(Node& node)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* instrumentingAgents = instrumentingAgentsForDocument(node.document()))
        addEventListenersToNodeImpl(*instrumentingAgents, node);
}

inline void InspectorInstrumentation::willInsertDOMNode(Document& document, Node& parent)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        willInsertDOMNodeImpl(*instrumentingAgents, parent);
}

inline void InspectorInstrumentation::didInsertDOMNode(Document& document, Node& node)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        didInsertDOMNodeImpl(*instrumentingAgents, node);
}

inline void InspectorInstrumentation::willRemoveDOMNode(Document& document, Node& node)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        willRemoveDOMNodeImpl(*instrumentingAgents, node);
}

inline void InspectorInstrumentation::didRemoveDOMNode(Document& document, Node& node)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        didRemoveDOMNodeImpl(*instrumentingAgents, node);
}

inline void InspectorInstrumentation::willModifyDOMAttr(Document& document, Element& element, const AtomicString& oldValue, const AtomicString& newValue)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        willModifyDOMAttrImpl(*instrumentingAgents, element, oldValue, newValue);
}

inline void InspectorInstrumentation::didModifyDOMAttr(Document& document, Element& element, const AtomicString& name, const AtomicString& value)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        didModifyDOMAttrImpl(*instrumentingAgents, element, name, value);
}

inline void InspectorInstrumentation::didRemoveDOMAttr(Document& document, Element& element, const AtomicString& name)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        didRemoveDOMAttrImpl(*instrumentingAgents, element, name);
}

inline void InspectorInstrumentation::didInvalidateStyleAttr(Document& document, Node& node)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        didInvalidateStyleAttrImpl(*instrumentingAgents, node);
}

inline void InspectorInstrumentation::documentDetached(Document& document)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        documentDetachedImpl(*instrumentingAgents, document);
}

inline void InspectorInstrumentation::frameWindowDiscarded(Frame& frame, DOMWindow* domWindow)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        frameWindowDiscardedImpl(*instrumentingAgents, domWindow);
}

inline void InspectorInstrumentation::mediaQueryResultChanged(Document& document)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        mediaQueryResultChangedImpl(*instrumentingAgents);
}

inline void InspectorInstrumentation::activeStyleSheetsUpdated(Document& document)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        activeStyleSheetsUpdatedImpl(*instrumentingAgents, document);
}

inline void InspectorInstrumentation::didPushShadowRoot(Element& host, ShadowRoot& root)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(host.document()))
        didPushShadowRootImpl(*instrumentingAgents, host, root);
}

inline void InspectorInstrumentation::willPopShadowRoot(Element& host, ShadowRoot& root)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(host.document()))
        willPopShadowRootImpl(*instrumentingAgents, host, root);
}

inline void InspectorInstrumentation::didChangeCustomElementState(Element& element)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(element.document()))
        didChangeCustomElementStateImpl(*instrumentingAgents, element);
}

inline void InspectorInstrumentation::pseudoElementCreated(Page* page, PseudoElement& pseudoElement)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForPage(page))
        pseudoElementCreatedImpl(*instrumentingAgents, pseudoElement);
}

inline void InspectorInstrumentation::pseudoElementDestroyed(Page* page, PseudoElement& pseudoElement)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForPage(page))
        pseudoElementDestroyedImpl(*instrumentingAgents, pseudoElement);
}

inline void InspectorInstrumentation::didCreateNamedFlow(Document* document, WebKitNamedFlow& namedFlow)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        didCreateNamedFlowImpl(*instrumentingAgents, document, namedFlow);
}

inline void InspectorInstrumentation::willRemoveNamedFlow(Document* document, WebKitNamedFlow& namedFlow)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        willRemoveNamedFlowImpl(*instrumentingAgents, document, namedFlow);
}

inline void InspectorInstrumentation::didChangeRegionOverset(Document& document, WebKitNamedFlow& namedFlow)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        didChangeRegionOversetImpl(*instrumentingAgents, document, namedFlow);
}

inline void InspectorInstrumentation::didRegisterNamedFlowContentElement(Document& document, WebKitNamedFlow& namedFlow, Node& contentElement, Node* nextContentElement)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        didRegisterNamedFlowContentElementImpl(*instrumentingAgents, document, namedFlow, contentElement, nextContentElement);
}

inline void InspectorInstrumentation::didUnregisterNamedFlowContentElement(Document& document, WebKitNamedFlow& namedFlow, Node& contentElement)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        didUnregisterNamedFlowContentElementImpl(*instrumentingAgents, document, namedFlow, contentElement);
}

inline void InspectorInstrumentation::mouseDidMoveOverElement(Page& page, const HitTestResult& result, unsigned modifierFlags)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    mouseDidMoveOverElementImpl(instrumentingAgentsForPage(page), result, modifierFlags);
}

inline bool InspectorInstrumentation::handleTouchEvent(Frame& frame, Node& node)
{
    FAST_RETURN_IF_NO_FRONTENDS(false);
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        return handleTouchEventImpl(*instrumentingAgents, node);
    return false;
}

inline bool InspectorInstrumentation::handleMousePress(Frame& frame)
{
    FAST_RETURN_IF_NO_FRONTENDS(false);
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        return handleMousePressImpl(*instrumentingAgents);
    return false;
}

inline bool InspectorInstrumentation::forcePseudoState(const Element& element, CSSSelector::PseudoClassType pseudoState)
{
    FAST_RETURN_IF_NO_FRONTENDS(false);
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(element.document()))
        return forcePseudoStateImpl(*instrumentingAgents, element, pseudoState);
    return false;
}

inline void InspectorInstrumentation::characterDataModified(Document& document, CharacterData& characterData)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        characterDataModifiedImpl(*instrumentingAgents, characterData);
}

inline void InspectorInstrumentation::willSendXMLHttpRequest(ScriptExecutionContext* context, const String& url)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        willSendXMLHttpRequestImpl(*instrumentingAgents, url);
}

inline void InspectorInstrumentation::willFetch(ScriptExecutionContext& context, const String& url)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        willFetchImpl(*instrumentingAgents, url);
}

inline void InspectorInstrumentation::didInstallTimer(ScriptExecutionContext& context, int timerId, Seconds timeout, bool singleShot)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        didInstallTimerImpl(*instrumentingAgents, timerId, timeout, singleShot, context);
}

inline void InspectorInstrumentation::didRemoveTimer(ScriptExecutionContext& context, int timerId)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        didRemoveTimerImpl(*instrumentingAgents, timerId, context);
}

inline void InspectorInstrumentation::didAddEventListener(EventTarget& target, const AtomicString& eventType, EventListener& listener, bool capture)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(target.scriptExecutionContext()))
        didAddEventListenerImpl(*instrumentingAgents, target, eventType, listener, capture);
}

inline void InspectorInstrumentation::willRemoveEventListener(EventTarget& target, const AtomicString& eventType, EventListener& listener, bool capture)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(target.scriptExecutionContext()))
        willRemoveEventListenerImpl(*instrumentingAgents, target, eventType, listener, capture);
}

inline bool InspectorInstrumentation::isEventListenerDisabled(EventTarget& target, const AtomicString& eventType, EventListener& listener, bool capture)
{
    FAST_RETURN_IF_NO_FRONTENDS(false);
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(target.scriptExecutionContext()))
        return isEventListenerDisabledImpl(*instrumentingAgents, target, eventType, listener, capture);
    return false;
}

inline void InspectorInstrumentation::didPostMessage(Frame& frame, TimerBase& timer, JSC::ExecState& state)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        didPostMessageImpl(*instrumentingAgents, timer, state);
}

inline void InspectorInstrumentation::didFailPostMessage(Frame& frame, TimerBase& timer)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        didFailPostMessageImpl(*instrumentingAgents, timer);
}

inline void InspectorInstrumentation::willDispatchPostMessage(Frame& frame, TimerBase& timer)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        willDispatchPostMessageImpl(*instrumentingAgents, timer);
}

inline void InspectorInstrumentation::didDispatchPostMessage(Frame& frame, TimerBase& timer)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        didDispatchPostMessageImpl(*instrumentingAgents, timer);
}

inline InspectorInstrumentationCookie InspectorInstrumentation::willCallFunction(ScriptExecutionContext* context, const String& scriptName, int scriptLine, int scriptColumn)
{
    FAST_RETURN_IF_NO_FRONTENDS(InspectorInstrumentationCookie());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        return willCallFunctionImpl(*instrumentingAgents, scriptName, scriptLine, scriptColumn, context);
    return InspectorInstrumentationCookie();
}

inline void InspectorInstrumentation::didCallFunction(const InspectorInstrumentationCookie& cookie, ScriptExecutionContext* context)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (cookie.isValid())
        didCallFunctionImpl(cookie, context);
}

inline InspectorInstrumentationCookie InspectorInstrumentation::willDispatchEvent(Document& document, const Event& event, bool hasEventListeners)
{
    FAST_RETURN_IF_NO_FRONTENDS(InspectorInstrumentationCookie());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        return willDispatchEventImpl(*instrumentingAgents, document, event, hasEventListeners);
    return InspectorInstrumentationCookie();
}

inline void InspectorInstrumentation::didDispatchEvent(const InspectorInstrumentationCookie& cookie)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (cookie.isValid())
        didDispatchEventImpl(cookie);
}

inline void InspectorInstrumentation::willHandleEvent(ScriptExecutionContext& context, const Event& event, const RegisteredEventListener& listener)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        return willHandleEventImpl(*instrumentingAgents, event, listener);
}

inline void InspectorInstrumentation::didHandleEvent(ScriptExecutionContext& context)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        return didHandleEventImpl(*instrumentingAgents);
}

inline InspectorInstrumentationCookie InspectorInstrumentation::willDispatchEventOnWindow(Frame* frame, const Event& event, DOMWindow& window)
{
    FAST_RETURN_IF_NO_FRONTENDS(InspectorInstrumentationCookie());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        return willDispatchEventOnWindowImpl(*instrumentingAgents, event, window);
    return InspectorInstrumentationCookie();
}

inline void InspectorInstrumentation::didDispatchEventOnWindow(const InspectorInstrumentationCookie& cookie)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (cookie.isValid())
        didDispatchEventOnWindowImpl(cookie);
}

inline void InspectorInstrumentation::eventDidResetAfterDispatch(const Event& event)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());

    if (!is<Node>(event.target()))
        return;

    auto* node = downcast<Node>(event.target());
    if (auto* instrumentingAgents = instrumentingAgentsForContext(node->scriptExecutionContext()))
        return eventDidResetAfterDispatchImpl(*instrumentingAgents, event);
}

inline InspectorInstrumentationCookie InspectorInstrumentation::willEvaluateScript(Frame& frame, const String& url, int lineNumber, int columnNumber)
{
    FAST_RETURN_IF_NO_FRONTENDS(InspectorInstrumentationCookie());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        return willEvaluateScriptImpl(*instrumentingAgents, frame, url, lineNumber, columnNumber);
    return InspectorInstrumentationCookie();
}

inline void InspectorInstrumentation::didEvaluateScript(const InspectorInstrumentationCookie& cookie, Frame& frame)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (cookie.isValid())
        didEvaluateScriptImpl(cookie, frame);
}

inline InspectorInstrumentationCookie InspectorInstrumentation::willFireTimer(ScriptExecutionContext& context, int timerId, bool oneShot)
{
    FAST_RETURN_IF_NO_FRONTENDS(InspectorInstrumentationCookie());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        return willFireTimerImpl(*instrumentingAgents, timerId, oneShot, context);
    return InspectorInstrumentationCookie();
}

inline void InspectorInstrumentation::didFireTimer(const InspectorInstrumentationCookie& cookie)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (cookie.isValid())
        didFireTimerImpl(cookie);
}

inline void InspectorInstrumentation::didInvalidateLayout(Frame& frame)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        didInvalidateLayoutImpl(*instrumentingAgents, frame);
}

inline InspectorInstrumentationCookie InspectorInstrumentation::willLayout(Frame& frame)
{
    FAST_RETURN_IF_NO_FRONTENDS(InspectorInstrumentationCookie());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        return willLayoutImpl(*instrumentingAgents, frame);
    return InspectorInstrumentationCookie();
}

inline void InspectorInstrumentation::didLayout(const InspectorInstrumentationCookie& cookie, RenderObject& root)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (cookie.isValid())
        didLayoutImpl(cookie, root);
}

inline void InspectorInstrumentation::didScroll(Page& page)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    didScrollImpl(instrumentingAgentsForPage(page));
}

inline void InspectorInstrumentation::willComposite(Frame& frame)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        willCompositeImpl(*instrumentingAgents, frame);
}

inline void InspectorInstrumentation::didComposite(Frame& frame)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        didCompositeImpl(*instrumentingAgents);
}

inline void InspectorInstrumentation::willPaint(RenderObject& renderer)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForRenderer(renderer))
        return willPaintImpl(*instrumentingAgents, renderer);
}

inline void InspectorInstrumentation::didPaint(RenderObject& renderer, const LayoutRect& rect)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForRenderer(renderer))
        didPaintImpl(*instrumentingAgents, renderer, rect);
}

inline InspectorInstrumentationCookie InspectorInstrumentation::willRecalculateStyle(Document& document)
{
    FAST_RETURN_IF_NO_FRONTENDS(InspectorInstrumentationCookie());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        return willRecalculateStyleImpl(*instrumentingAgents, document);
    return InspectorInstrumentationCookie();
}

inline void InspectorInstrumentation::didRecalculateStyle(const InspectorInstrumentationCookie& cookie)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (cookie.isValid())
        didRecalculateStyleImpl(cookie);
}

inline void InspectorInstrumentation::didScheduleStyleRecalculation(Document& document)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        didScheduleStyleRecalculationImpl(*instrumentingAgents, document);
}

inline void InspectorInstrumentation::applyUserAgentOverride(Frame& frame, String& userAgent)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        applyUserAgentOverrideImpl(*instrumentingAgents, userAgent);
}

inline void InspectorInstrumentation::applyEmulatedMedia(Frame& frame, String& media)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        applyEmulatedMediaImpl(*instrumentingAgents, media);
}

inline void InspectorInstrumentation::willSendRequest(Frame* frame, unsigned long identifier, DocumentLoader* loader, ResourceRequest& request, const ResourceResponse& redirectResponse)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        willSendRequestImpl(*instrumentingAgents, identifier, loader, request, redirectResponse);
}

inline void InspectorInstrumentation::willSendRequest(WorkerGlobalScope& workerGlobalScope, unsigned long identifier, ResourceRequest& request)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    willSendRequestImpl(instrumentingAgentsForWorkerGlobalScope(workerGlobalScope), identifier, nullptr, request, ResourceResponse { });
}

inline void InspectorInstrumentation::willSendRequestOfType(Frame* frame, unsigned long identifier, DocumentLoader* loader, ResourceRequest& request, LoadType loadType)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        willSendRequestOfTypeImpl(*instrumentingAgents, identifier, loader, request, loadType);
}

inline void InspectorInstrumentation::didLoadResourceFromMemoryCache(Page& page, DocumentLoader* loader, CachedResource* resource)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    didLoadResourceFromMemoryCacheImpl(instrumentingAgentsForPage(page), loader, resource);
}

inline void InspectorInstrumentation::didReceiveResourceResponse(Frame& frame, unsigned long identifier, DocumentLoader* loader, const ResourceResponse& response, ResourceLoader* resourceLoader)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        didReceiveResourceResponseImpl(*instrumentingAgents, identifier, loader, response, resourceLoader);
}

inline void InspectorInstrumentation::didReceiveResourceResponse(WorkerGlobalScope& workerGlobalScope, unsigned long identifier, const ResourceResponse& response)
{
    didReceiveResourceResponseImpl(instrumentingAgentsForWorkerGlobalScope(workerGlobalScope), identifier, nullptr, response, nullptr);
}

inline void InspectorInstrumentation::didReceiveThreadableLoaderResponse(DocumentThreadableLoader& documentThreadableLoader, unsigned long identifier)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(documentThreadableLoader.document()))
        didReceiveThreadableLoaderResponseImpl(*instrumentingAgents, documentThreadableLoader, identifier);
}
    
inline void InspectorInstrumentation::didReceiveData(Frame* frame, unsigned long identifier, const char* data, int dataLength, int encodedDataLength)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        didReceiveDataImpl(*instrumentingAgents, identifier, data, dataLength, encodedDataLength);
}

inline void InspectorInstrumentation::didReceiveData(WorkerGlobalScope& workerGlobalScope, unsigned long identifier, const char* data, int dataLength)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    didReceiveDataImpl(instrumentingAgentsForWorkerGlobalScope(workerGlobalScope), identifier, data, dataLength, dataLength);
}

inline void InspectorInstrumentation::didFinishLoading(Frame* frame, DocumentLoader* loader, unsigned long identifier, const NetworkLoadMetrics& networkLoadMetrics, ResourceLoader* resourceLoader)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        didFinishLoadingImpl(*instrumentingAgents, identifier, loader, networkLoadMetrics, resourceLoader);
}

inline void InspectorInstrumentation::didFinishLoading(WorkerGlobalScope& workerGlobalScope, unsigned long identifier, const NetworkLoadMetrics& networkLoadMetrics)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    didFinishLoadingImpl(instrumentingAgentsForWorkerGlobalScope(workerGlobalScope), identifier, nullptr, networkLoadMetrics, nullptr);
}

inline void InspectorInstrumentation::didFailLoading(Frame* frame, DocumentLoader* loader, unsigned long identifier, const ResourceError& error)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        didFailLoadingImpl(*instrumentingAgents, identifier, loader, error);
}

inline void InspectorInstrumentation::didFailLoading(WorkerGlobalScope& workerGlobalScope, unsigned long identifier, const ResourceError& error)
{
    didFailLoadingImpl(instrumentingAgentsForWorkerGlobalScope(workerGlobalScope), identifier, nullptr, error);
}

inline void InspectorInstrumentation::continueAfterXFrameOptionsDenied(Frame& frame, unsigned long identifier, DocumentLoader& loader, const ResourceResponse& response)
{
    // Treat the same as didReceiveResponse.
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        didReceiveResourceResponseImpl(*instrumentingAgents, identifier, &loader, response, nullptr);
}

inline void InspectorInstrumentation::continueWithPolicyDownload(Frame& frame, unsigned long identifier, DocumentLoader& loader, const ResourceResponse& response)
{
    // Treat the same as didReceiveResponse.
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        didReceiveResourceResponseImpl(*instrumentingAgents, identifier, &loader, response, nullptr);
}

inline void InspectorInstrumentation::continueWithPolicyIgnore(Frame& frame, unsigned long identifier, DocumentLoader& loader, const ResourceResponse& response)
{
    // Treat the same as didReceiveResponse.
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        didReceiveResourceResponseImpl(*instrumentingAgents, identifier, &loader, response, nullptr);
}

inline void InspectorInstrumentation::willLoadXHRSynchronously(ScriptExecutionContext* context)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        willLoadXHRSynchronouslyImpl(*instrumentingAgents);
}

inline void InspectorInstrumentation::didLoadXHRSynchronously(ScriptExecutionContext* context)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        didLoadXHRSynchronouslyImpl(*instrumentingAgents);
}

inline void InspectorInstrumentation::scriptImported(ScriptExecutionContext& context, unsigned long identifier, const String& sourceString)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        scriptImportedImpl(*instrumentingAgents, identifier, sourceString);
}

inline void InspectorInstrumentation::scriptExecutionBlockedByCSP(ScriptExecutionContext* context, const String& directiveText)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        scriptExecutionBlockedByCSPImpl(*instrumentingAgents, directiveText);
}

inline void InspectorInstrumentation::didReceiveScriptResponse(ScriptExecutionContext* context, unsigned long identifier)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        didReceiveScriptResponseImpl(*instrumentingAgents, identifier);
}

inline void InspectorInstrumentation::domContentLoadedEventFired(Frame& frame)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        domContentLoadedEventFiredImpl(*instrumentingAgents, frame);
}

inline void InspectorInstrumentation::loadEventFired(Frame* frame)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        loadEventFiredImpl(*instrumentingAgents, frame);
}

inline void InspectorInstrumentation::frameDetachedFromParent(Frame& frame)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        frameDetachedFromParentImpl(*instrumentingAgents, frame);
}

inline void InspectorInstrumentation::didCommitLoad(Frame& frame, DocumentLoader* loader)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        didCommitLoadImpl(*instrumentingAgents, frame, loader);
}

inline void InspectorInstrumentation::frameDocumentUpdated(Frame& frame)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        frameDocumentUpdatedImpl(*instrumentingAgents, frame);
}

inline void InspectorInstrumentation::loaderDetachedFromFrame(Frame& frame, DocumentLoader& loader)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        loaderDetachedFromFrameImpl(*instrumentingAgents, loader);
}

inline void InspectorInstrumentation::frameStartedLoading(Frame& frame)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        frameStartedLoadingImpl(*instrumentingAgents, frame);
}

inline void InspectorInstrumentation::frameStoppedLoading(Frame& frame)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        frameStoppedLoadingImpl(*instrumentingAgents, frame);
}

inline void InspectorInstrumentation::frameScheduledNavigation(Frame& frame, Seconds delay)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        frameScheduledNavigationImpl(*instrumentingAgents, frame, delay);
}

inline void InspectorInstrumentation::frameClearedScheduledNavigation(Frame& frame)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        frameClearedScheduledNavigationImpl(*instrumentingAgents, frame);
}

inline void InspectorInstrumentation::defaultAppearanceDidChange(Page& page, bool useDarkAppearance)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    defaultAppearanceDidChangeImpl(instrumentingAgentsForPage(page), useDarkAppearance);
}

inline void InspectorInstrumentation::willDestroyCachedResource(CachedResource& cachedResource)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    willDestroyCachedResourceImpl(cachedResource);
}

inline void InspectorInstrumentation::didOpenDatabase(ScriptExecutionContext* context, RefPtr<Database>&& database, const String& domain, const String& name, const String& version)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        didOpenDatabaseImpl(*instrumentingAgents, WTFMove(database), domain, name, version);
}

inline void InspectorInstrumentation::didDispatchDOMStorageEvent(Page& page, const String& key, const String& oldValue, const String& newValue, StorageType storageType, SecurityOrigin* securityOrigin)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    didDispatchDOMStorageEventImpl(instrumentingAgentsForPage(page), key, oldValue, newValue, storageType, securityOrigin);
}

inline bool InspectorInstrumentation::shouldWaitForDebuggerOnStart(ScriptExecutionContext& context)
{
    FAST_RETURN_IF_NO_FRONTENDS(false);
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        return shouldWaitForDebuggerOnStartImpl(*instrumentingAgents);
    return false;
}

inline void InspectorInstrumentation::workerStarted(ScriptExecutionContext& context, WorkerInspectorProxy* proxy, const URL& url)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        workerStartedImpl(*instrumentingAgents, proxy, url);
}

inline void InspectorInstrumentation::workerTerminated(ScriptExecutionContext& context, WorkerInspectorProxy* proxy)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        workerTerminatedImpl(*instrumentingAgents, proxy);
}

inline void InspectorInstrumentation::didCreateWebSocket(Document* document, unsigned long identifier, const URL& requestURL)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        didCreateWebSocketImpl(*instrumentingAgents, identifier, requestURL);
}

inline void InspectorInstrumentation::willSendWebSocketHandshakeRequest(Document* document, unsigned long identifier, const ResourceRequest& request)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        willSendWebSocketHandshakeRequestImpl(*instrumentingAgents, identifier, request);
}

inline void InspectorInstrumentation::didReceiveWebSocketHandshakeResponse(Document* document, unsigned long identifier, const ResourceResponse& response)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        didReceiveWebSocketHandshakeResponseImpl(*instrumentingAgents, identifier, response);
}

inline void InspectorInstrumentation::didCloseWebSocket(Document* document, unsigned long identifier)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        didCloseWebSocketImpl(*instrumentingAgents, identifier);
}

inline void InspectorInstrumentation::didReceiveWebSocketFrame(Document* document, unsigned long identifier, const WebSocketFrame& frame)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        didReceiveWebSocketFrameImpl(*instrumentingAgents, identifier, frame);
}

inline void InspectorInstrumentation::didReceiveWebSocketFrameError(Document* document, unsigned long identifier, const String& errorMessage)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        didReceiveWebSocketFrameErrorImpl(*instrumentingAgents, identifier, errorMessage);
}

inline void InspectorInstrumentation::didSendWebSocketFrame(Document* document, unsigned long identifier, const WebSocketFrame& frame)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        didSendWebSocketFrameImpl(*instrumentingAgents, identifier, frame);
}

#if ENABLE(RESOURCE_USAGE)
inline void InspectorInstrumentation::didHandleMemoryPressure(Page& page, Critical critical)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    didHandleMemoryPressureImpl(instrumentingAgentsForPage(page), critical);
}
#endif

inline void InspectorInstrumentation::didChangeCSSCanvasClientNodes(CanvasBase& canvasBase)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(canvasBase.scriptExecutionContext()))
        didChangeCSSCanvasClientNodesImpl(*instrumentingAgents, canvasBase);
}

inline void InspectorInstrumentation::didCreateCanvasRenderingContext(CanvasRenderingContext& context)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context.canvasBase().scriptExecutionContext()))
        didCreateCanvasRenderingContextImpl(*instrumentingAgents, context);
}

inline void InspectorInstrumentation::didChangeCanvasMemory(CanvasRenderingContext& context)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context.canvasBase().scriptExecutionContext()))
        didChangeCanvasMemoryImpl(*instrumentingAgents, context);
}

inline void InspectorInstrumentation::recordCanvasAction(CanvasRenderingContext& context, const String& name, Vector<RecordCanvasActionVariant>&& parameters)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context.canvasBase().scriptExecutionContext()))
        recordCanvasActionImpl(*instrumentingAgents, context, name, WTFMove(parameters));
}

inline void InspectorInstrumentation::didFinishRecordingCanvasFrame(CanvasRenderingContext& context, bool forceDispatch)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context.canvasBase().scriptExecutionContext()))
        didFinishRecordingCanvasFrameImpl(*instrumentingAgents, context, forceDispatch);
}

#if ENABLE(WEBGL)
inline void InspectorInstrumentation::didEnableExtension(WebGLRenderingContextBase& contextWebGLBase, const String& extension)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(contextWebGLBase.canvasBase().scriptExecutionContext()))
        didEnableExtensionImpl(*instrumentingAgents, contextWebGLBase, extension);
}

inline void InspectorInstrumentation::didCreateProgram(WebGLRenderingContextBase& contextWebGLBase, WebGLProgram& program)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(contextWebGLBase.canvasBase().scriptExecutionContext()))
        didCreateProgramImpl(*instrumentingAgents, contextWebGLBase, program);
}

inline void InspectorInstrumentation::willDeleteProgram(WebGLRenderingContextBase& contextWebGLBase, WebGLProgram& program)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(contextWebGLBase.canvasBase().scriptExecutionContext()))
        willDeleteProgramImpl(*instrumentingAgents, program);
}

inline bool InspectorInstrumentation::isShaderProgramDisabled(WebGLRenderingContextBase& contextWebGLBase, WebGLProgram& program)
{
    FAST_RETURN_IF_NO_FRONTENDS(false);
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(contextWebGLBase.canvasBase().scriptExecutionContext()))
        return isShaderProgramDisabledImpl(*instrumentingAgents, program);
    return false;
}

inline bool InspectorInstrumentation::isShaderProgramHighlighted(WebGLRenderingContextBase& contextWebGLBase, WebGLProgram& program)
{
    FAST_RETURN_IF_NO_FRONTENDS(false);
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(contextWebGLBase.canvasBase().scriptExecutionContext()))
        return isShaderProgramHighlightedImpl(*instrumentingAgents, program);
    return false;
}
#endif

inline void InspectorInstrumentation::networkStateChanged(Page& page)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    networkStateChangedImpl(instrumentingAgentsForPage(page));
}

inline void InspectorInstrumentation::updateApplicationCacheStatus(Frame* frame)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (auto* instrumentingAgents = instrumentingAgentsForFrame(frame))
        updateApplicationCacheStatusImpl(*instrumentingAgents, *frame);
}

inline void InspectorInstrumentation::addMessageToConsole(Page& page, std::unique_ptr<Inspector::ConsoleMessage> message)
{
    addMessageToConsoleImpl(instrumentingAgentsForPage(page), WTFMove(message));
}

inline void InspectorInstrumentation::addMessageToConsole(WorkerGlobalScope& workerGlobalScope, std::unique_ptr<Inspector::ConsoleMessage> message)
{
    addMessageToConsoleImpl(instrumentingAgentsForWorkerGlobalScope(workerGlobalScope), WTFMove(message));
}

inline void InspectorInstrumentation::consoleCount(Page& page, JSC::ExecState* state, Ref<Inspector::ScriptArguments>&& arguments)
{
    consoleCountImpl(instrumentingAgentsForPage(page), state, WTFMove(arguments));
}

inline void InspectorInstrumentation::consoleCount(WorkerGlobalScope& workerGlobalScope, JSC::ExecState* state, Ref<Inspector::ScriptArguments>&& arguments)
{
    consoleCountImpl(instrumentingAgentsForWorkerGlobalScope(workerGlobalScope), state, WTFMove(arguments));
}

inline void InspectorInstrumentation::takeHeapSnapshot(Frame& frame, const String& title)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        takeHeapSnapshotImpl(*instrumentingAgents, title);
}

inline void InspectorInstrumentation::startConsoleTiming(Frame& frame, const String& title)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        startConsoleTimingImpl(*instrumentingAgents, frame, title);
}

inline void InspectorInstrumentation::startConsoleTiming(WorkerGlobalScope& workerGlobalScope, const String& title)
{
    startConsoleTimingImpl(instrumentingAgentsForWorkerGlobalScope(workerGlobalScope), title);
}

inline void InspectorInstrumentation::stopConsoleTiming(Frame& frame, const String& title, Ref<Inspector::ScriptCallStack>&& stack)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        stopConsoleTimingImpl(*instrumentingAgents, frame, title, WTFMove(stack));
}

inline void InspectorInstrumentation::stopConsoleTiming(WorkerGlobalScope& workerGlobalScope, const String& title, Ref<Inspector::ScriptCallStack>&& stack)
{
    stopConsoleTimingImpl(instrumentingAgentsForWorkerGlobalScope(workerGlobalScope), title, WTFMove(stack));
}

inline void InspectorInstrumentation::consoleTimeStamp(Frame& frame, Ref<Inspector::ScriptArguments>&& arguments)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        consoleTimeStampImpl(*instrumentingAgents, frame, WTFMove(arguments));
}

inline void InspectorInstrumentation::startProfiling(Page& page, JSC::ExecState* exec, const String &title)
{
    startProfilingImpl(instrumentingAgentsForPage(page), exec, title);
}

inline void InspectorInstrumentation::stopProfiling(Page& page, JSC::ExecState* exec, const String &title)
{
    stopProfilingImpl(instrumentingAgentsForPage(page), exec, title);
}

inline void InspectorInstrumentation::consoleStartRecordingCanvas(CanvasRenderingContext& context, JSC::ExecState& exec, JSC::JSObject* options)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context.canvasBase().scriptExecutionContext()))
        consoleStartRecordingCanvasImpl(*instrumentingAgents, context, exec, options);
}

inline void InspectorInstrumentation::didRequestAnimationFrame(Document& document, int callbackId)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        didRequestAnimationFrameImpl(*instrumentingAgents, callbackId, document);
}

inline void InspectorInstrumentation::didCancelAnimationFrame(Document& document, int callbackId)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        didCancelAnimationFrameImpl(*instrumentingAgents, callbackId, document);
}

inline InspectorInstrumentationCookie InspectorInstrumentation::willFireAnimationFrame(Document& document, int callbackId)
{
    FAST_RETURN_IF_NO_FRONTENDS(InspectorInstrumentationCookie());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        return willFireAnimationFrameImpl(*instrumentingAgents, callbackId, document);
    return InspectorInstrumentationCookie();
}

inline void InspectorInstrumentation::didFireAnimationFrame(const InspectorInstrumentationCookie& cookie)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (cookie.isValid())
        didFireAnimationFrameImpl(cookie);
}

inline InspectorInstrumentationCookie InspectorInstrumentation::willFireObserverCallback(ScriptExecutionContext& context, const String& callbackType)
{
    FAST_RETURN_IF_NO_FRONTENDS(InspectorInstrumentationCookie());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        return willFireObserverCallbackImpl(*instrumentingAgents, callbackType, context);
    return InspectorInstrumentationCookie();
}

inline void InspectorInstrumentation::didFireObserverCallback(const InspectorInstrumentationCookie& cookie)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (cookie.isValid())
        didFireObserverCallbackImpl(cookie);
}

inline void InspectorInstrumentation::layerTreeDidChange(Page* page)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForPage(page))
        layerTreeDidChangeImpl(*instrumentingAgents);
}

inline void InspectorInstrumentation::renderLayerDestroyed(Page* page, const RenderLayer& renderLayer)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForPage(page))
        renderLayerDestroyedImpl(*instrumentingAgents, renderLayer);
}

inline InstrumentingAgents* InspectorInstrumentation::instrumentingAgentsForContext(ScriptExecutionContext* context)
{
    return context ? instrumentingAgentsForContext(*context) : nullptr;
}

inline InstrumentingAgents* InspectorInstrumentation::instrumentingAgentsForContext(ScriptExecutionContext& context)
{
    if (is<Document>(context))
        return instrumentingAgentsForPage(downcast<Document>(context).page());
    if (is<WorkerGlobalScope>(context))
        return &instrumentingAgentsForWorkerGlobalScope(downcast<WorkerGlobalScope>(context));
    return nullptr;
}

inline InstrumentingAgents* InspectorInstrumentation::instrumentingAgentsForFrame(Frame* frame)
{
    return frame ? instrumentingAgentsForFrame(*frame) : nullptr;
}

inline InstrumentingAgents* InspectorInstrumentation::instrumentingAgentsForFrame(Frame& frame)
{
    return instrumentingAgentsForPage(frame.page());
}

inline InstrumentingAgents* InspectorInstrumentation::instrumentingAgentsForDocument(Document* document)
{
    return document ? instrumentingAgentsForDocument(*document) : nullptr;
}

inline InstrumentingAgents* InspectorInstrumentation::instrumentingAgentsForDocument(Document& document)
{
    Page* page = document.page();
    if (!page && document.templateDocumentHost())
        page = document.templateDocumentHost()->page();
    return instrumentingAgentsForPage(page);
}

inline InstrumentingAgents* InspectorInstrumentation::instrumentingAgentsForPage(Page* page)
{
    return page ? &instrumentingAgentsForPage(*page) : nullptr;
}

inline InstrumentingAgents& InspectorInstrumentation::instrumentingAgentsForPage(Page& page)
{
    ASSERT(isMainThread());
    return page.inspectorController().m_instrumentingAgents.get();
}

inline InstrumentingAgents* InspectorInstrumentation::instrumentingAgentsForWorkerGlobalScope(WorkerGlobalScope* workerGlobalScope)
{
    return workerGlobalScope ? &instrumentingAgentsForWorkerGlobalScope(*workerGlobalScope) : nullptr;
}

inline InstrumentingAgents& InspectorInstrumentation::instrumentingAgentsForWorkerGlobalScope(WorkerGlobalScope& workerGlobalScope)
{
    return workerGlobalScope.inspectorController().m_instrumentingAgents;
}

inline void InspectorInstrumentation::frontendCreated()
{
    ASSERT(isMainThread());
    s_frontendCounter++;

    if (s_frontendCounter == 1)
        InspectorInstrumentation::firstFrontendCreated();
}

inline void InspectorInstrumentation::frontendDeleted()
{
    ASSERT(isMainThread());
    s_frontendCounter--;

    if (!s_frontendCounter)
        InspectorInstrumentation::lastFrontendDeleted();
}

} // namespace WebCore
