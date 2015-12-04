/*
* Copyright (C) 2010 Google Inc. All rights reserved.
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

#ifndef InspectorInstrumentation_h
#define InspectorInstrumentation_h

#include "CSSSelector.h"
#include "Element.h"
#include "FormData.h"
#include "Frame.h"
#include "HitTestResult.h"
#include "InspectorInstrumentationCookie.h"
#include "Page.h"
#include "ScriptExecutionContext.h"
#include "ScriptState.h"
#include "StorageArea.h"
#include "WebSocketFrame.h"
#include <runtime/ConsoleTypes.h>
#include <wtf/RefPtr.h>

#if ENABLE(WEB_REPLAY)
#include "ReplaySession.h"
#include "ReplaySessionSegment.h"
#endif


namespace Deprecated {
class ScriptObject;
}

namespace Inspector {
class ConsoleMessage;
class ScriptArguments;
class ScriptCallStack;
}

namespace JSC {
class Profile;
}

namespace WebCore {

class CSSRule;
class CachedResource;
class CharacterData;
class DOMWindow;
class DOMWrapperWorld;
class Database;
class Document;
class DocumentLoader;
class Element;
class GraphicsContext;
class HTTPHeaderMap;
class InspectorCSSAgent;
class InspectorCSSOMWrappers;
class InspectorInstrumentation;
class InspectorTimelineAgent;
class InstrumentingAgents;
class Node;
class PseudoElement;
class RenderLayer;
class RenderLayerBacking;
class RenderObject;
class ResourceRequest;
class ResourceResponse;
class ScriptExecutionContext;
class SecurityOrigin;
class ShadowRoot;
class StorageArea;
class StyleResolver;
class StyleRule;
class ThreadableLoaderClient;
class URL;
class WorkerGlobalScope;
class XMLHttpRequest;

struct ReplayPosition;

#define FAST_RETURN_IF_NO_FRONTENDS(value) if (LIKELY(!hasFrontends())) return value;

class InspectorInstrumentation {
public:
    static void didClearWindowObjectInWorld(Frame&, DOMWrapperWorld&);
    static bool isDebuggerPaused(Frame*);

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
    static void frameWindowDiscarded(Frame*, DOMWindow*);
    static void mediaQueryResultChanged(Document&);
    static void activeStyleSheetsUpdated(Document&);
    static void didPushShadowRoot(Element& host, ShadowRoot&);
    static void willPopShadowRoot(Element& host, ShadowRoot&);
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
    static bool forcePseudoState(Element&, CSSSelector::PseudoClassType);

    static void willSendXMLHttpRequest(ScriptExecutionContext*, const String& url);
    static void didInstallTimer(ScriptExecutionContext*, int timerId, int timeout, bool singleShot);
    static void didRemoveTimer(ScriptExecutionContext*, int timerId);

    static InspectorInstrumentationCookie willCallFunction(ScriptExecutionContext*, const String& scriptName, int scriptLine);
    static void didCallFunction(const InspectorInstrumentationCookie&, ScriptExecutionContext*);
    static InspectorInstrumentationCookie willDispatchEvent(Document&, const Event&, bool hasEventListeners);
    static void didDispatchEvent(const InspectorInstrumentationCookie&);
    static InspectorInstrumentationCookie willHandleEvent(ScriptExecutionContext*, const Event&);
    static void didHandleEvent(const InspectorInstrumentationCookie&);
    static InspectorInstrumentationCookie willDispatchEventOnWindow(Frame*, const Event&, DOMWindow&);
    static void didDispatchEventOnWindow(const InspectorInstrumentationCookie&);
    static InspectorInstrumentationCookie willEvaluateScript(Frame&, const String& url, int lineNumber);
    static void didEvaluateScript(const InspectorInstrumentationCookie&, Frame&);
    static void scriptsEnabled(Page&, bool isEnabled);
    static InspectorInstrumentationCookie willFireTimer(ScriptExecutionContext*, int timerId);
    static void didFireTimer(const InspectorInstrumentationCookie&);
    static void didInvalidateLayout(Frame&);
    static InspectorInstrumentationCookie willLayout(Frame&);
    static void didLayout(const InspectorInstrumentationCookie&, RenderObject*);
    static void didScroll(Page&);
    static void willComposite(Frame&);
    static void didComposite(Frame&);
    static void willPaint(RenderObject*);
    static void didPaint(RenderObject*, const LayoutRect&);
    static InspectorInstrumentationCookie willRecalculateStyle(Document&);
    static void didRecalculateStyle(const InspectorInstrumentationCookie&);
    static void didScheduleStyleRecalculation(Document&);

    static void applyEmulatedMedia(Frame&, String&);
    static void willSendRequest(Frame*, unsigned long identifier, DocumentLoader*, ResourceRequest&, const ResourceResponse& redirectResponse);
    static void continueAfterPingLoader(Frame&, unsigned long identifier, DocumentLoader*, ResourceRequest&, const ResourceResponse&);
    static void markResourceAsCached(Page&, unsigned long identifier);
    static void didLoadResourceFromMemoryCache(Page&, DocumentLoader*, CachedResource*);
    static InspectorInstrumentationCookie willReceiveResourceResponse(Frame*);
    static void didReceiveResourceResponse(const InspectorInstrumentationCookie&, unsigned long identifier, DocumentLoader*, const ResourceResponse&, ResourceLoader*);
    static void continueAfterXFrameOptionsDenied(Frame*, DocumentLoader&, unsigned long identifier, const ResourceResponse&);
    static void continueWithPolicyDownload(Frame*, DocumentLoader&, unsigned long identifier, const ResourceResponse&);
    static void continueWithPolicyIgnore(Frame*, DocumentLoader&, unsigned long identifier, const ResourceResponse&);
    static void didReceiveData(Frame*, unsigned long identifier, const char* data, int dataLength, int encodedDataLength);
    static void didFinishLoading(Frame*, DocumentLoader*, unsigned long identifier, double finishTime);
    static void didFailLoading(Frame*, DocumentLoader*, unsigned long identifier, const ResourceError&);
    static void didFinishXHRLoading(ScriptExecutionContext*, ThreadableLoaderClient*, unsigned long identifier, const String& sourceString, const String& url, const String& sendURL, unsigned sendLineNumber, unsigned sendColumnNumber);
    static void didReceiveXHRResponse(ScriptExecutionContext*, unsigned long identifier);
    static void willLoadXHRSynchronously(ScriptExecutionContext*);
    static void didLoadXHRSynchronously(ScriptExecutionContext*);
    static void scriptImported(ScriptExecutionContext*, unsigned long identifier, const String& sourceString);
    static void scriptExecutionBlockedByCSP(ScriptExecutionContext*, const String& directiveText);
    static void didReceiveScriptResponse(ScriptExecutionContext*, unsigned long identifier);
    static void domContentLoadedEventFired(Frame&);
    static void loadEventFired(Frame*);
    static void frameDetachedFromParent(Frame&);
    static void didCommitLoad(Frame&, DocumentLoader*);
    static void frameDocumentUpdated(Frame*);
    static void loaderDetachedFromFrame(Frame&, DocumentLoader&);
    static void frameStartedLoading(Frame&);
    static void frameStoppedLoading(Frame&);
    static void frameScheduledNavigation(Frame&, double delay);
    static void frameClearedScheduledNavigation(Frame&);
    static InspectorInstrumentationCookie willRunJavaScriptDialog(Page&, const String& message);
    static void didRunJavaScriptDialog(const InspectorInstrumentationCookie&);
    static void willDestroyCachedResource(CachedResource&);

    static void addMessageToConsole(Page&, std::unique_ptr<Inspector::ConsoleMessage>);

    // FIXME: Convert to ScriptArguments to match non-worker context.
    static void addMessageToConsole(WorkerGlobalScope*, std::unique_ptr<Inspector::ConsoleMessage>);

    static void consoleCount(Page&, JSC::ExecState*, RefPtr<Inspector::ScriptArguments>&&);
    static void startConsoleTiming(Frame&, const String& title);
    static void stopConsoleTiming(Frame&, const String& title, RefPtr<Inspector::ScriptCallStack>&&);
    static void consoleTimeStamp(Frame&, RefPtr<Inspector::ScriptArguments>&&);

    static void didRequestAnimationFrame(Document*, int callbackId);
    static void didCancelAnimationFrame(Document*, int callbackId);
    static InspectorInstrumentationCookie willFireAnimationFrame(Document*, int callbackId);
    static void didFireAnimationFrame(const InspectorInstrumentationCookie&);

    static void startProfiling(Page&, JSC::ExecState*, const String& title);
    static RefPtr<JSC::Profile> stopProfiling(Page&, JSC::ExecState*, const String& title);

    static void didOpenDatabase(ScriptExecutionContext*, RefPtr<Database>&&, const String& domain, const String& name, const String& version);

    static void didDispatchDOMStorageEvent(const String& key, const String& oldValue, const String& newValue, StorageType, SecurityOrigin*, Page*);

#if ENABLE(WEB_REPLAY)
    static void sessionCreated(Page&, RefPtr<ReplaySession>&&);
    static void sessionLoaded(Page&, RefPtr<ReplaySession>&&);
    static void sessionModified(Page&, RefPtr<ReplaySession>&&);

    static void segmentCreated(Page&, RefPtr<ReplaySessionSegment>&&);
    static void segmentCompleted(Page&, RefPtr<ReplaySessionSegment>&&);
    static void segmentLoaded(Page&, RefPtr<ReplaySessionSegment>&&);
    static void segmentUnloaded(Page&);

    static void captureStarted(Page&);
    static void captureStopped(Page&);

    static void playbackStarted(Page&);
    static void playbackPaused(Page&, const ReplayPosition&);
    static void playbackHitPosition(Page&, const ReplayPosition&);
    static void playbackFinished(Page&);
#endif

#if ENABLE(WEB_SOCKETS)
    static void didCreateWebSocket(Document*, unsigned long identifier, const URL& requestURL);
    static void willSendWebSocketHandshakeRequest(Document*, unsigned long identifier, const ResourceRequest&);
    static void didReceiveWebSocketHandshakeResponse(Document*, unsigned long identifier, const ResourceResponse&);
    static void didCloseWebSocket(Document*, unsigned long identifier);
    static void didReceiveWebSocketFrame(Document*, unsigned long identifier, const WebSocketFrame&);
    static void didSendWebSocketFrame(Document*, unsigned long identifier, const WebSocketFrame&);
    static void didReceiveWebSocketFrameError(Document*, unsigned long identifier, const String& errorMessage);
#endif

    static Deprecated::ScriptObject wrapCanvas2DRenderingContextForInstrumentation(Document*, const Deprecated::ScriptObject&);
#if ENABLE(WEBGL)
    static Deprecated::ScriptObject wrapWebGLRenderingContextForInstrumentation(Document*, const Deprecated::ScriptObject&);
#endif

    static void networkStateChanged(Page*);
    static void updateApplicationCacheStatus(Frame*);

    static void layerTreeDidChange(Page*);
    static void renderLayerDestroyed(Page*, const RenderLayer&);

    static void frontendCreated() { s_frontendCounter += 1; }
    static void frontendDeleted() { s_frontendCounter -= 1; }
    static bool hasFrontends() { return s_frontendCounter; }
    static bool consoleAgentEnabled(ScriptExecutionContext*);
    static bool timelineAgentEnabled(ScriptExecutionContext*);
    static bool replayAgentEnabled(ScriptExecutionContext*);

    WEBCORE_EXPORT static InstrumentingAgents* instrumentingAgentsForPage(Page*);

    static void registerInstrumentingAgents(InstrumentingAgents&);
    static void unregisterInstrumentingAgents(InstrumentingAgents&);

private:
    static void didClearWindowObjectInWorldImpl(InstrumentingAgents&, Frame&, DOMWrapperWorld&);
    static bool isDebuggerPausedImpl(InstrumentingAgents&);

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
    static void pseudoElementCreatedImpl(InstrumentingAgents&, PseudoElement&);
    static void pseudoElementDestroyedImpl(InstrumentingAgents&, PseudoElement&);
    static void didCreateNamedFlowImpl(InstrumentingAgents&, Document*, WebKitNamedFlow&);
    static void willRemoveNamedFlowImpl(InstrumentingAgents&, Document*, WebKitNamedFlow&);
    static void didChangeRegionOversetImpl(InstrumentingAgents&, Document&, WebKitNamedFlow&);
    static void didRegisterNamedFlowContentElementImpl(InstrumentingAgents&, Document&, WebKitNamedFlow&, Node& contentElement, Node* nextContentElement = nullptr);
    static void didUnregisterNamedFlowContentElementImpl(InstrumentingAgents&, Document&, WebKitNamedFlow&, Node& contentElement);

    static void mouseDidMoveOverElementImpl(InstrumentingAgents&, const HitTestResult&, unsigned modifierFlags);
    static bool handleTouchEventImpl(InstrumentingAgents&, Node&);
    static bool handleMousePressImpl(InstrumentingAgents&);
    static bool forcePseudoStateImpl(InstrumentingAgents&, Element&, CSSSelector::PseudoClassType);

    static void willSendXMLHttpRequestImpl(InstrumentingAgents&, const String& url);
    static void didInstallTimerImpl(InstrumentingAgents&, int timerId, int timeout, bool singleShot, ScriptExecutionContext*);
    static void didRemoveTimerImpl(InstrumentingAgents&, int timerId, ScriptExecutionContext*);

    static InspectorInstrumentationCookie willCallFunctionImpl(InstrumentingAgents&, const String& scriptName, int scriptLine, ScriptExecutionContext*);
    static void didCallFunctionImpl(const InspectorInstrumentationCookie&, ScriptExecutionContext*);
    static InspectorInstrumentationCookie willDispatchEventImpl(InstrumentingAgents&, Document&, const Event&, bool hasEventListeners);
    static InspectorInstrumentationCookie willHandleEventImpl(InstrumentingAgents&, const Event&);
    static void didHandleEventImpl(const InspectorInstrumentationCookie&);
    static void didDispatchEventImpl(const InspectorInstrumentationCookie&);
    static InspectorInstrumentationCookie willDispatchEventOnWindowImpl(InstrumentingAgents&, const Event&, DOMWindow&);
    static void didDispatchEventOnWindowImpl(const InspectorInstrumentationCookie&);
    static InspectorInstrumentationCookie willEvaluateScriptImpl(InstrumentingAgents&, Frame&, const String& url, int lineNumber);
    static void didEvaluateScriptImpl(const InspectorInstrumentationCookie&, Frame&);
    static void scriptsEnabledImpl(InstrumentingAgents&, bool isEnabled);
    static InspectorInstrumentationCookie willFireTimerImpl(InstrumentingAgents&, int timerId, ScriptExecutionContext*);
    static void didFireTimerImpl(const InspectorInstrumentationCookie&);
    static void didInvalidateLayoutImpl(InstrumentingAgents&, Frame&);
    static InspectorInstrumentationCookie willLayoutImpl(InstrumentingAgents&, Frame&);
    static void didLayoutImpl(const InspectorInstrumentationCookie&, RenderObject*);
    static void didScrollImpl(InstrumentingAgents&);
    static void willCompositeImpl(InstrumentingAgents&, Frame&);
    static void didCompositeImpl(InstrumentingAgents&);
    static void willPaintImpl(InstrumentingAgents&, RenderObject*);
    static void didPaintImpl(InstrumentingAgents&, RenderObject*, const LayoutRect&);
    static InspectorInstrumentationCookie willRecalculateStyleImpl(InstrumentingAgents&, Document&);
    static void didRecalculateStyleImpl(const InspectorInstrumentationCookie&);
    static void didScheduleStyleRecalculationImpl(InstrumentingAgents&, Document&);

    static void applyEmulatedMediaImpl(InstrumentingAgents&, String&);
    static void willSendRequestImpl(InstrumentingAgents&, unsigned long identifier, DocumentLoader*, ResourceRequest&, const ResourceResponse& redirectResponse);
    static void continueAfterPingLoaderImpl(InstrumentingAgents&, unsigned long identifier, DocumentLoader*, ResourceRequest&, const ResourceResponse&);
    static void markResourceAsCachedImpl(InstrumentingAgents&, unsigned long identifier);
    static void didLoadResourceFromMemoryCacheImpl(InstrumentingAgents&, DocumentLoader*, CachedResource*);
    static InspectorInstrumentationCookie willReceiveResourceResponseImpl(InstrumentingAgents&);
    static void didReceiveResourceResponseImpl(const InspectorInstrumentationCookie&, unsigned long identifier, DocumentLoader*, const ResourceResponse&, ResourceLoader*);
    static void didReceiveResourceResponseButCanceledImpl(Frame*, DocumentLoader&, unsigned long identifier, const ResourceResponse&);
    static void continueAfterXFrameOptionsDeniedImpl(Frame*, DocumentLoader&, unsigned long identifier, const ResourceResponse&);
    static void continueWithPolicyDownloadImpl(Frame*, DocumentLoader&, unsigned long identifier, const ResourceResponse&);
    static void continueWithPolicyIgnoreImpl(Frame*, DocumentLoader&, unsigned long identifier, const ResourceResponse&);
    static void didReceiveDataImpl(InstrumentingAgents&, unsigned long identifier, const char* data, int dataLength, int encodedDataLength);
    static void didFinishLoadingImpl(InstrumentingAgents&, unsigned long identifier, DocumentLoader*, double finishTime);
    static void didFailLoadingImpl(InstrumentingAgents&, unsigned long identifier, DocumentLoader*, const ResourceError&);
    static void willLoadXHRImpl(InstrumentingAgents&, ThreadableLoaderClient*, const String&, const URL&, bool, RefPtr<FormData>&&, const HTTPHeaderMap&, bool);
    static void didFailXHRLoadingImpl(InstrumentingAgents&, ThreadableLoaderClient*);
    static void didFinishXHRLoadingImpl(InstrumentingAgents&, ThreadableLoaderClient*, unsigned long identifier, const String& sourceString, const String& url, const String& sendURL, unsigned sendLineNumber, unsigned sendColumnNumber);
    static void didReceiveXHRResponseImpl(InstrumentingAgents&, unsigned long identifier);
    static void willLoadXHRSynchronouslyImpl(InstrumentingAgents&);
    static void didLoadXHRSynchronouslyImpl(InstrumentingAgents&);
    static void scriptImportedImpl(InstrumentingAgents&, unsigned long identifier, const String& sourceString);
    static void scriptExecutionBlockedByCSPImpl(InstrumentingAgents&, const String& directiveText);
    static void didReceiveScriptResponseImpl(InstrumentingAgents&, unsigned long identifier);
    static void domContentLoadedEventFiredImpl(InstrumentingAgents&, Frame&);
    static void loadEventFiredImpl(InstrumentingAgents&, Frame*);
    static void frameDetachedFromParentImpl(InstrumentingAgents&, Frame&);
    static void didCommitLoadImpl(InstrumentingAgents&, Page*, DocumentLoader*);
    static void frameDocumentUpdatedImpl(InstrumentingAgents&, Frame*);
    static void loaderDetachedFromFrameImpl(InstrumentingAgents&, DocumentLoader&);
    static void frameStartedLoadingImpl(InstrumentingAgents&, Frame&);
    static void frameStoppedLoadingImpl(InstrumentingAgents&, Frame&);
    static void frameScheduledNavigationImpl(InstrumentingAgents&, Frame&, double delay);
    static void frameClearedScheduledNavigationImpl(InstrumentingAgents&, Frame&);
    static InspectorInstrumentationCookie willRunJavaScriptDialogImpl(InstrumentingAgents&, const String& message);
    static void didRunJavaScriptDialogImpl(const InspectorInstrumentationCookie&);
    static void willDestroyCachedResourceImpl(CachedResource&);

    static void addMessageToConsoleImpl(InstrumentingAgents&, std::unique_ptr<Inspector::ConsoleMessage>);

    static void consoleCountImpl(InstrumentingAgents&, JSC::ExecState*, RefPtr<Inspector::ScriptArguments>&&);
    static void startConsoleTimingImpl(InstrumentingAgents&, Frame&, const String& title);
    static void stopConsoleTimingImpl(InstrumentingAgents&, Frame&, const String& title, RefPtr<Inspector::ScriptCallStack>&&);
    static void consoleTimeStampImpl(InstrumentingAgents&, Frame&, RefPtr<Inspector::ScriptArguments>&&);

    static void didRequestAnimationFrameImpl(InstrumentingAgents&, int callbackId, Frame*);
    static void didCancelAnimationFrameImpl(InstrumentingAgents&, int callbackId, Frame*);
    static InspectorInstrumentationCookie willFireAnimationFrameImpl(InstrumentingAgents&, int callbackId, Frame*);
    static void didFireAnimationFrameImpl(const InspectorInstrumentationCookie&);

    static void startProfilingImpl(InstrumentingAgents&, JSC::ExecState*, const String& title);
    static RefPtr<JSC::Profile> stopProfilingImpl(InstrumentingAgents&, JSC::ExecState*, const String& title);

    static void didOpenDatabaseImpl(InstrumentingAgents&, RefPtr<Database>&&, const String& domain, const String& name, const String& version);

    static void didDispatchDOMStorageEventImpl(InstrumentingAgents&, const String& key, const String& oldValue, const String& newValue, StorageType, SecurityOrigin*, Page*);

#if ENABLE(WEB_REPLAY)
    static void sessionCreatedImpl(InstrumentingAgents&, RefPtr<ReplaySession>&&);
    static void sessionLoadedImpl(InstrumentingAgents&, RefPtr<ReplaySession>&&);
    static void sessionModifiedImpl(InstrumentingAgents&, RefPtr<ReplaySession>&&);

    static void segmentCreatedImpl(InstrumentingAgents&, RefPtr<ReplaySessionSegment>&&);
    static void segmentCompletedImpl(InstrumentingAgents&, RefPtr<ReplaySessionSegment>&&);
    static void segmentLoadedImpl(InstrumentingAgents&, RefPtr<ReplaySessionSegment>&&);
    static void segmentUnloadedImpl(InstrumentingAgents&);

    static void captureStartedImpl(InstrumentingAgents&);
    static void captureStoppedImpl(InstrumentingAgents&);

    static void playbackStartedImpl(InstrumentingAgents&);
    static void playbackPausedImpl(InstrumentingAgents&, const ReplayPosition&);
    static void playbackHitPositionImpl(InstrumentingAgents&, const ReplayPosition&);
    static void playbackFinishedImpl(InstrumentingAgents&);
#endif

#if ENABLE(WEB_SOCKETS)
    static void didCreateWebSocketImpl(InstrumentingAgents&, unsigned long identifier, const URL& requestURL);
    static void willSendWebSocketHandshakeRequestImpl(InstrumentingAgents&, unsigned long identifier, const ResourceRequest&);
    static void didReceiveWebSocketHandshakeResponseImpl(InstrumentingAgents&, unsigned long identifier, const ResourceResponse&);
    static void didCloseWebSocketImpl(InstrumentingAgents&, unsigned long identifier);
    static void didReceiveWebSocketFrameImpl(InstrumentingAgents&, unsigned long identifier, const WebSocketFrame&);
    static void didSendWebSocketFrameImpl(InstrumentingAgents&, unsigned long identifier, const WebSocketFrame&);
    static void didReceiveWebSocketFrameErrorImpl(InstrumentingAgents&, unsigned long identifier, const String&);
#endif

    static void networkStateChangedImpl(InstrumentingAgents&);
    static void updateApplicationCacheStatusImpl(InstrumentingAgents&, Frame*);

    static void layerTreeDidChangeImpl(InstrumentingAgents&);
    static void renderLayerDestroyedImpl(InstrumentingAgents&, const RenderLayer&);

    static InstrumentingAgents& instrumentingAgentsForPage(Page&);
    static InstrumentingAgents* instrumentingAgentsForFrame(Frame&);
    static InstrumentingAgents* instrumentingAgentsForFrame(Frame*);
    static InstrumentingAgents* instrumentingAgentsForContext(ScriptExecutionContext*);
    static InstrumentingAgents* instrumentingAgentsForDocument(Document&);
    static InstrumentingAgents* instrumentingAgentsForDocument(Document*);
    static InstrumentingAgents* instrumentingAgentsForRenderer(RenderObject*);

    static InspectorTimelineAgent* retrieveTimelineAgent(const InspectorInstrumentationCookie&);

    static void pauseOnNativeEventIfNeeded(InstrumentingAgents&, bool isDOMEvent, const String& eventName, bool synchronous);
    static void cancelPauseOnNativeEvent(InstrumentingAgents&);

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

inline void InspectorInstrumentation::frameWindowDiscarded(Frame* frame, DOMWindow* domWindow)
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

inline bool InspectorInstrumentation::forcePseudoState(Element& element, CSSSelector::PseudoClassType pseudoState)
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

inline void InspectorInstrumentation::didInstallTimer(ScriptExecutionContext* context, int timerId, int timeout, bool singleShot)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        didInstallTimerImpl(*instrumentingAgents, timerId, timeout, singleShot, context);
}

inline void InspectorInstrumentation::didRemoveTimer(ScriptExecutionContext* context, int timerId)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        didRemoveTimerImpl(*instrumentingAgents, timerId, context);
}

inline InspectorInstrumentationCookie InspectorInstrumentation::willCallFunction(ScriptExecutionContext* context, const String& scriptName, int scriptLine)
{
    FAST_RETURN_IF_NO_FRONTENDS(InspectorInstrumentationCookie());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        return willCallFunctionImpl(*instrumentingAgents, scriptName, scriptLine, context);
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

inline InspectorInstrumentationCookie InspectorInstrumentation::willHandleEvent(ScriptExecutionContext* context, const Event& event)
{
    FAST_RETURN_IF_NO_FRONTENDS(InspectorInstrumentationCookie());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        return willHandleEventImpl(*instrumentingAgents, event);
    return InspectorInstrumentationCookie();
}

inline void InspectorInstrumentation::didHandleEvent(const InspectorInstrumentationCookie& cookie)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (cookie.isValid())
        didHandleEventImpl(cookie);
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

inline InspectorInstrumentationCookie InspectorInstrumentation::willEvaluateScript(Frame& frame, const String& url, int lineNumber)
{
    FAST_RETURN_IF_NO_FRONTENDS(InspectorInstrumentationCookie());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        return willEvaluateScriptImpl(*instrumentingAgents, frame, url, lineNumber);
    return InspectorInstrumentationCookie();
}

inline void InspectorInstrumentation::didEvaluateScript(const InspectorInstrumentationCookie& cookie, Frame& frame)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (cookie.isValid())
        didEvaluateScriptImpl(cookie, frame);
}

inline void InspectorInstrumentation::scriptsEnabled(Page& page, bool isEnabled)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    return scriptsEnabledImpl(instrumentingAgentsForPage(page), isEnabled);
}

inline InspectorInstrumentationCookie InspectorInstrumentation::willFireTimer(ScriptExecutionContext* context, int timerId)
{
    FAST_RETURN_IF_NO_FRONTENDS(InspectorInstrumentationCookie());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        return willFireTimerImpl(*instrumentingAgents, timerId, context);
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

inline void InspectorInstrumentation::didLayout(const InspectorInstrumentationCookie& cookie, RenderObject* root)
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
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(&frame))
        willCompositeImpl(*instrumentingAgents, frame);
}

inline void InspectorInstrumentation::didComposite(Frame& frame)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(&frame))
        didCompositeImpl(*instrumentingAgents);
}

inline void InspectorInstrumentation::willPaint(RenderObject* renderer)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForRenderer(renderer))
        return willPaintImpl(*instrumentingAgents, renderer);
}

inline void InspectorInstrumentation::didPaint(RenderObject* renderer, const LayoutRect& rect)
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

inline void InspectorInstrumentation::applyEmulatedMedia(Frame& frame, String& media)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        applyEmulatedMediaImpl(*instrumentingAgents, media);
}

inline void InspectorInstrumentation::willSendRequest(Frame* frame, unsigned long identifier, DocumentLoader* loader, ResourceRequest& request, const ResourceResponse& redirectResponse)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        willSendRequestImpl(*instrumentingAgents, identifier, loader, request, redirectResponse);
}

inline void InspectorInstrumentation::continueAfterPingLoader(Frame& frame, unsigned long identifier, DocumentLoader* loader, ResourceRequest& request, const ResourceResponse& response)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(&frame))
        InspectorInstrumentation::continueAfterPingLoaderImpl(*instrumentingAgents, identifier, loader, request, response);
}

inline void InspectorInstrumentation::markResourceAsCached(Page& page, unsigned long identifier)
{
    markResourceAsCachedImpl(instrumentingAgentsForPage(page), identifier);
}

inline void InspectorInstrumentation::didLoadResourceFromMemoryCache(Page& page, DocumentLoader* loader, CachedResource* resource)
{
    didLoadResourceFromMemoryCacheImpl(instrumentingAgentsForPage(page), loader, resource);
}

inline InspectorInstrumentationCookie InspectorInstrumentation::willReceiveResourceResponse(Frame* frame)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        return willReceiveResourceResponseImpl(*instrumentingAgents);
    return InspectorInstrumentationCookie();
}

inline void InspectorInstrumentation::didReceiveResourceResponse(const InspectorInstrumentationCookie& cookie, unsigned long identifier, DocumentLoader* loader, const ResourceResponse& response, ResourceLoader* resourceLoader)
{
    // Call this unconditionally so that we're able to log to console with no front-end attached.
    if (cookie.isValid())
        didReceiveResourceResponseImpl(cookie, identifier, loader, response, resourceLoader);
}

inline void InspectorInstrumentation::continueAfterXFrameOptionsDenied(Frame* frame, DocumentLoader& loader, unsigned long identifier, const ResourceResponse& r)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    InspectorInstrumentation::continueAfterXFrameOptionsDeniedImpl(frame, loader, identifier, r);
}

inline void InspectorInstrumentation::continueWithPolicyDownload(Frame* frame, DocumentLoader& loader, unsigned long identifier, const ResourceResponse& r)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    InspectorInstrumentation::continueWithPolicyDownloadImpl(frame, loader, identifier, r);
}

inline void InspectorInstrumentation::continueWithPolicyIgnore(Frame* frame, DocumentLoader& loader, unsigned long identifier, const ResourceResponse& r)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    InspectorInstrumentation::continueWithPolicyIgnoreImpl(frame, loader, identifier, r);
}

inline void InspectorInstrumentation::didReceiveData(Frame* frame, unsigned long identifier, const char* data, int dataLength, int encodedDataLength)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        didReceiveDataImpl(*instrumentingAgents, identifier, data, dataLength, encodedDataLength);
}

inline void InspectorInstrumentation::didFinishLoading(Frame* frame, DocumentLoader* loader, unsigned long identifier, double finishTime)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        didFinishLoadingImpl(*instrumentingAgents, identifier, loader, finishTime);
}

inline void InspectorInstrumentation::didFailLoading(Frame* frame, DocumentLoader* loader, unsigned long identifier, const ResourceError& error)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        didFailLoadingImpl(*instrumentingAgents, identifier, loader, error);
}

inline void InspectorInstrumentation::didFinishXHRLoading(ScriptExecutionContext* context, ThreadableLoaderClient* client, unsigned long identifier, const String& sourceString, const String& url, const String& sendURL, unsigned sendLineNumber, unsigned sendColumnNumber)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        didFinishXHRLoadingImpl(*instrumentingAgents, client, identifier, sourceString, url, sendURL, sendLineNumber, sendColumnNumber);
}

inline void InspectorInstrumentation::didReceiveXHRResponse(ScriptExecutionContext* context, unsigned long identifier)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        didReceiveXHRResponseImpl(*instrumentingAgents, identifier);
}

inline void InspectorInstrumentation::willLoadXHRSynchronously(ScriptExecutionContext* context)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        willLoadXHRSynchronouslyImpl(*instrumentingAgents);
}

inline void InspectorInstrumentation::didLoadXHRSynchronously(ScriptExecutionContext* context)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        didLoadXHRSynchronouslyImpl(*instrumentingAgents);
}

inline void InspectorInstrumentation::scriptImported(ScriptExecutionContext* context, unsigned long identifier, const String& sourceString)
{
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
        didCommitLoadImpl(*instrumentingAgents, frame.page(), loader);
}

inline void InspectorInstrumentation::frameDocumentUpdated(Frame* frame)
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
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(&frame))
        frameStartedLoadingImpl(*instrumentingAgents, frame);
}

inline void InspectorInstrumentation::frameStoppedLoading(Frame& frame)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(&frame))
        frameStoppedLoadingImpl(*instrumentingAgents, frame);
}

inline void InspectorInstrumentation::frameScheduledNavigation(Frame& frame, double delay)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(&frame))
        frameScheduledNavigationImpl(*instrumentingAgents, frame, delay);
}

inline void InspectorInstrumentation::frameClearedScheduledNavigation(Frame& frame)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(&frame))
        frameClearedScheduledNavigationImpl(*instrumentingAgents, frame);
}

inline InspectorInstrumentationCookie InspectorInstrumentation::willRunJavaScriptDialog(Page& page, const String& message)
{
    FAST_RETURN_IF_NO_FRONTENDS(InspectorInstrumentationCookie());
    return willRunJavaScriptDialogImpl(instrumentingAgentsForPage(page), message);
}

inline void InspectorInstrumentation::didRunJavaScriptDialog(const InspectorInstrumentationCookie& cookie)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (cookie.isValid())
        didRunJavaScriptDialogImpl(cookie);
}

inline void InspectorInstrumentation::willDestroyCachedResource(CachedResource& cachedResource)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    willDestroyCachedResourceImpl(cachedResource);
}

inline void InspectorInstrumentation::didOpenDatabase(ScriptExecutionContext* context, RefPtr<Database>&& database, const String& domain, const String& name, const String& version)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        didOpenDatabaseImpl(*instrumentingAgents, WTF::move(database), domain, name, version);
}

inline void InspectorInstrumentation::didDispatchDOMStorageEvent(const String& key, const String& oldValue, const String& newValue, StorageType storageType, SecurityOrigin* securityOrigin, Page* page)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForPage(page))
        didDispatchDOMStorageEventImpl(*instrumentingAgents, key, oldValue, newValue, storageType, securityOrigin, page);
}

#if ENABLE(WEB_SOCKETS)
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
#endif // ENABLE(WEB_SOCKETS)

#if ENABLE(WEB_REPLAY)
inline void InspectorInstrumentation::sessionCreated(Page& page, RefPtr<ReplaySession>&& session)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    sessionCreatedImpl(instrumentingAgentsForPage(page), WTF::move(session));
}

inline void InspectorInstrumentation::sessionLoaded(Page& page, RefPtr<ReplaySession>&& session)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    sessionLoadedImpl(instrumentingAgentsForPage(page), WTF::move(session));
}

inline void InspectorInstrumentation::sessionModified(Page& page, RefPtr<ReplaySession>&& session)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    sessionModifiedImpl(instrumentingAgentsForPage(page), WTF::move(session));
}

inline void InspectorInstrumentation::segmentCreated(Page& page, RefPtr<ReplaySessionSegment>&& segment)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    segmentCreatedImpl(instrumentingAgentsForPage(page), WTF::move(segment));
}

inline void InspectorInstrumentation::segmentCompleted(Page& page, RefPtr<ReplaySessionSegment>&& segment)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    segmentCompletedImpl(instrumentingAgentsForPage(page), WTF::move(segment));
}

inline void InspectorInstrumentation::segmentLoaded(Page& page, RefPtr<ReplaySessionSegment>&& segment)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    segmentLoadedImpl(instrumentingAgentsForPage(page), WTF::move(segment));
}

inline void InspectorInstrumentation::segmentUnloaded(Page& page)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    segmentUnloadedImpl(instrumentingAgentsForPage(page));
}

inline void InspectorInstrumentation::captureStarted(Page& page)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    captureStartedImpl(instrumentingAgentsForPage(page));
}

inline void InspectorInstrumentation::captureStopped(Page& page)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    captureStoppedImpl(instrumentingAgentsForPage(page));
}

inline void InspectorInstrumentation::playbackStarted(Page& page)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
        playbackStartedImpl(instrumentingAgentsForPage(page));
}

inline void InspectorInstrumentation::playbackPaused(Page& page, const ReplayPosition& position)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
        playbackPausedImpl(instrumentingAgentsForPage(page), position);
}

inline void InspectorInstrumentation::playbackFinished(Page& page)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
        playbackFinishedImpl(instrumentingAgentsForPage(page));
}

inline void InspectorInstrumentation::playbackHitPosition(Page& page, const ReplayPosition& position)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
        playbackHitPositionImpl(instrumentingAgentsForPage(page), position);
}
#endif // ENABLE(WEB_REPLAY)

inline void InspectorInstrumentation::networkStateChanged(Page* page)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForPage(page))
        networkStateChangedImpl(*instrumentingAgents);
}

inline void InspectorInstrumentation::updateApplicationCacheStatus(Frame* frame)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        updateApplicationCacheStatusImpl(*instrumentingAgents, frame);
}

inline void InspectorInstrumentation::didRequestAnimationFrame(Document* document, int callbackId)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        didRequestAnimationFrameImpl(*instrumentingAgents, callbackId, document->frame());
}

inline void InspectorInstrumentation::didCancelAnimationFrame(Document* document, int callbackId)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        didCancelAnimationFrameImpl(*instrumentingAgents, callbackId, document->frame());
}

inline InspectorInstrumentationCookie InspectorInstrumentation::willFireAnimationFrame(Document* document, int callbackId)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        return willFireAnimationFrameImpl(*instrumentingAgents, callbackId, document->frame());
    return InspectorInstrumentationCookie();
}

inline void InspectorInstrumentation::didFireAnimationFrame(const InspectorInstrumentationCookie& cookie)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (cookie.isValid())
        didFireAnimationFrameImpl(cookie);
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
    if (!context)
        return nullptr;
    if (is<Document>(*context))
        return instrumentingAgentsForPage(downcast<Document>(context)->page());
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
#if ENABLE(TEMPLATE_ELEMENT)
    if (!page && document.templateDocumentHost())
        page = document.templateDocumentHost()->page();
#endif
    return instrumentingAgentsForPage(page);
}

} // namespace WebCore

#endif // !defined(InspectorInstrumentation_h)
