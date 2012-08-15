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

#include "CSSSelector.h"
#include "ConsoleTypes.h"
#include "Document.h"
#include "Element.h"
#include "Frame.h"
#include "Page.h"
#include "ScriptExecutionContext.h"
#include "StorageArea.h"

#if USE(JSC)
namespace JSC {
class ExecState;
}
#endif

namespace WebCore {

class CSSRule;
class CharacterData;
class DOMWindow;
class DOMWrapperWorld;
class Database;
class Element;
class EventContext;
class DocumentLoader;
class DeviceOrientationData;
class GeolocationPosition;
class GraphicsContext;
class HitTestResult;
class InspectorCSSAgent;
class InspectorTimelineAgent;
class InstrumentingAgents;
class KURL;
class Node;
class ResourceRequest;
class ResourceResponse;
class ScriptArguments;
class ScriptCallStack;
class ScriptExecutionContext;
class ScriptObject;
class ScriptProfile;
class SecurityOrigin;
class ShadowRoot;
class StorageArea;
class StyleRule;
class WorkerContext;
class WorkerContextProxy;
class XMLHttpRequest;

#if USE(JSC)
typedef JSC::ExecState ScriptState;
#else
class ScriptState;
#endif

#if ENABLE(WEB_SOCKETS)
struct WebSocketFrame;
class WebSocketHandshakeRequest;
class WebSocketHandshakeResponse;
#endif

#define FAST_RETURN_IF_NO_FRONTENDS(value) if (!hasFrontends()) return value;

typedef pair<InstrumentingAgents*, int> InspectorInstrumentationCookie;

class InspectorInstrumentation {
public:
    static void didClearWindowObjectInWorld(Frame*, DOMWrapperWorld*);
    static bool isDebuggerPaused(Frame*);

    static void willInsertDOMNode(Document*, Node* parent);
    static void didInsertDOMNode(Document*, Node*);
    static void willRemoveDOMNode(Document*, Node*);
    static void willModifyDOMAttr(Document*, Element*, const AtomicString& oldValue, const AtomicString& newValue);
    static void didModifyDOMAttr(Document*, Element*, const AtomicString& name, const AtomicString& value);
    static void didRemoveDOMAttr(Document*, Element*, const AtomicString& name);
    static void characterDataModified(Document*, CharacterData*);
    static void didInvalidateStyleAttr(Document*, Node*);
    static void frameWindowDiscarded(Frame*, DOMWindow*);
    static void mediaQueryResultChanged(Document*);
    static void didPushShadowRoot(Element* host, ShadowRoot*);
    static void willPopShadowRoot(Element* host, ShadowRoot*);
    static void didCreateNamedFlow(Document*, const AtomicString& name);
    static void didRemoveNamedFlow(Document*, const AtomicString& name);

    static void mouseDidMoveOverElement(Page*, const HitTestResult&, unsigned modifierFlags);
    static bool handleMousePress(Page*);
    static bool forcePseudoState(Element*, CSSSelector::PseudoType);

    static void willSendXMLHttpRequest(ScriptExecutionContext*, const String& url);
    static void didScheduleResourceRequest(Document*, const String& url);
    static void didInstallTimer(ScriptExecutionContext*, int timerId, int timeout, bool singleShot);
    static void didRemoveTimer(ScriptExecutionContext*, int timerId);

    static InspectorInstrumentationCookie willCallFunction(ScriptExecutionContext*, const String& scriptName, int scriptLine);
    static void didCallFunction(const InspectorInstrumentationCookie&);
    static InspectorInstrumentationCookie willChangeXHRReadyState(ScriptExecutionContext*, XMLHttpRequest* request);
    static void didChangeXHRReadyState(const InspectorInstrumentationCookie&);
    static InspectorInstrumentationCookie willDispatchEvent(Document*, const Event& event, DOMWindow* window, Node* node, const Vector<EventContext>& ancestors);
    static void didDispatchEvent(const InspectorInstrumentationCookie&);
    static InspectorInstrumentationCookie willHandleEvent(ScriptExecutionContext*, Event*);
    static void didHandleEvent(const InspectorInstrumentationCookie&);
    static InspectorInstrumentationCookie willDispatchEventOnWindow(Frame*, const Event& event, DOMWindow* window);
    static void didDispatchEventOnWindow(const InspectorInstrumentationCookie&);
    static InspectorInstrumentationCookie willEvaluateScript(Frame*, const String& url, int lineNumber);
    static void didEvaluateScript(const InspectorInstrumentationCookie&);
    static void didCreateIsolatedContext(Frame*, ScriptState*, SecurityOrigin*);
    static InspectorInstrumentationCookie willFireTimer(ScriptExecutionContext*, int timerId);
    static void didFireTimer(const InspectorInstrumentationCookie&);
    static void didBeginFrame(Page*);
    static void didCancelFrame(Page*);
    static InspectorInstrumentationCookie willLayout(Frame*);
    static void didLayout(const InspectorInstrumentationCookie&);
    static InspectorInstrumentationCookie willLoadXHR(ScriptExecutionContext*, XMLHttpRequest*);
    static void didLoadXHR(const InspectorInstrumentationCookie&);
    static InspectorInstrumentationCookie willPaint(Frame*, GraphicsContext*, const LayoutRect&);
    static void didPaint(const InspectorInstrumentationCookie&);
    static void willComposite(Page*);
    static void didComposite(Page*);
    static InspectorInstrumentationCookie willRecalculateStyle(Document*);
    static void didRecalculateStyle(const InspectorInstrumentationCookie&);
    static void didScheduleStyleRecalculation(Document*);
    static InspectorInstrumentationCookie willMatchRule(Document*, const StyleRule*);
    static void didMatchRule(const InspectorInstrumentationCookie&, bool matched);
    static InspectorInstrumentationCookie willProcessRule(Document*, const StyleRule*);
    static void didProcessRule(const InspectorInstrumentationCookie&);
    static void willProcessTask(Page*);
    static void didProcessTask(Page*);

    static void applyUserAgentOverride(Frame*, String*);
    static void applyScreenWidthOverride(Frame*, long*);
    static void applyScreenHeightOverride(Frame*, long*);
    static bool shouldApplyScreenWidthOverride(Frame*);
    static bool shouldApplyScreenHeightOverride(Frame*);
    static void willSendRequest(Frame*, unsigned long identifier, DocumentLoader*, ResourceRequest&, const ResourceResponse& redirectResponse);
    static void continueAfterPingLoader(Frame*, unsigned long identifier, DocumentLoader*, ResourceRequest&, const ResourceResponse&);
    static void markResourceAsCached(Page*, unsigned long identifier);
    static void didLoadResourceFromMemoryCache(Page*, DocumentLoader*, CachedResource*);
    static InspectorInstrumentationCookie willReceiveResourceData(Frame*, unsigned long identifier, int length);
    static void didReceiveResourceData(const InspectorInstrumentationCookie&);
    static InspectorInstrumentationCookie willReceiveResourceResponse(Frame*, unsigned long identifier, const ResourceResponse&);
    static void didReceiveResourceResponse(const InspectorInstrumentationCookie&, unsigned long identifier, DocumentLoader*, const ResourceResponse&, ResourceLoader*);
    static void continueAfterXFrameOptionsDenied(Frame*, DocumentLoader*, unsigned long identifier, const ResourceResponse&);
    static void continueWithPolicyDownload(Frame*, DocumentLoader*, unsigned long identifier, const ResourceResponse&);
    static void continueWithPolicyIgnore(Frame*, DocumentLoader*, unsigned long identifier, const ResourceResponse&);
    static void didReceiveData(Frame*, unsigned long identifier, const char* data, int dataLength, int encodedDataLength);
    static void didFinishLoading(Frame*, DocumentLoader*, unsigned long identifier, double finishTime);
    static void didFailLoading(Frame*, DocumentLoader*, unsigned long identifier, const ResourceError&);
    static void resourceRetrievedByXMLHttpRequest(ScriptExecutionContext*, unsigned long identifier, const String& sourceString, const String& url, const String& sendURL, unsigned sendLineNumber);
    static void didReceiveXHRResponse(ScriptExecutionContext*, unsigned long identifier);
    static void willLoadXHRSynchronously(ScriptExecutionContext*);
    static void didLoadXHRSynchronously(ScriptExecutionContext*);
    static void scriptImported(ScriptExecutionContext*, unsigned long identifier, const String& sourceString);
    static void didReceiveScriptResponse(ScriptExecutionContext*, unsigned long identifier);
    static void domContentLoadedEventFired(Frame*);
    static void loadEventFired(Frame*);
    static void frameDetachedFromParent(Frame*);
    static void didCommitLoad(Frame*, DocumentLoader*);
    static void loaderDetachedFromFrame(Frame*, DocumentLoader*);
    static void willDestroyCachedResource(CachedResource*);

    static InspectorInstrumentationCookie willWriteHTML(Document*, unsigned int length, unsigned int startLine);
    static void didWriteHTML(const InspectorInstrumentationCookie&, unsigned int endLine);

    static void addMessageToConsole(Page*, MessageSource, MessageType, MessageLevel, const String& message, PassRefPtr<ScriptArguments>, PassRefPtr<ScriptCallStack>);
    static void addMessageToConsole(Page*, MessageSource, MessageType, MessageLevel, const String& message, const String&, unsigned lineNumber);
#if ENABLE(WORKERS)
    static void addMessageToConsole(WorkerContext*, MessageSource, MessageType, MessageLevel, const String& message, PassRefPtr<ScriptArguments>, PassRefPtr<ScriptCallStack>);
    static void addMessageToConsole(WorkerContext*, MessageSource, MessageType, MessageLevel, const String& message, const String&, unsigned lineNumber);
#endif
    static void consoleCount(Page*, PassRefPtr<ScriptArguments>, PassRefPtr<ScriptCallStack>);
    static void startConsoleTiming(Frame*, const String& title);
    static void stopConsoleTiming(Frame*, const String& title, PassRefPtr<ScriptCallStack>);
    static void consoleTimeStamp(Frame*, PassRefPtr<ScriptArguments>);

    static void didRequestAnimationFrame(Document*, int callbackId);
    static void didCancelAnimationFrame(Document*, int callbackId);
    static InspectorInstrumentationCookie willFireAnimationFrame(Document*, int callbackId);
    static void didFireAnimationFrame(const InspectorInstrumentationCookie&);

#if ENABLE(JAVASCRIPT_DEBUGGER)
    static void addStartProfilingMessageToConsole(Page*, const String& title, unsigned lineNumber, const String& sourceURL);
    static void addProfile(Page*, RefPtr<ScriptProfile>, PassRefPtr<ScriptCallStack>);
    static String getCurrentUserInitiatedProfileName(Page*, bool incrementProfileNumber);
    static bool profilerEnabled(Page*);
#endif

#if ENABLE(SQL_DATABASE)
    static void didOpenDatabase(ScriptExecutionContext*, PassRefPtr<Database>, const String& domain, const String& name, const String& version);
#endif

    static void didUseDOMStorage(Page*, StorageArea*, bool isLocalStorage, Frame*);
    static void didDispatchDOMStorageEvent(const String& key, const String& oldValue, const String& newValue, StorageType, SecurityOrigin*, Page*);

#if ENABLE(WORKERS)
    static bool shouldPauseDedicatedWorkerOnStart(ScriptExecutionContext*);
    static void didStartWorkerContext(ScriptExecutionContext*, WorkerContextProxy*, const KURL&);
    static void workerContextTerminated(ScriptExecutionContext*, WorkerContextProxy*);
    static void willEvaluateWorkerScript(WorkerContext*, int workerThreadStartMode);
#endif

#if ENABLE(WEB_SOCKETS)
    static void didCreateWebSocket(ScriptExecutionContext*, unsigned long identifier, const KURL& requestURL, const KURL& documentURL);
    static void willSendWebSocketHandshakeRequest(ScriptExecutionContext*, unsigned long identifier, const WebSocketHandshakeRequest&);
    static void didReceiveWebSocketHandshakeResponse(ScriptExecutionContext*, unsigned long identifier, const WebSocketHandshakeResponse&);
    static void didCloseWebSocket(ScriptExecutionContext*, unsigned long identifier);
    static void didReceiveWebSocketFrame(Document*, unsigned long identifier, const WebSocketFrame&);
    static void didSendWebSocketFrame(Document*, unsigned long identifier, const WebSocketFrame&);
    static void didReceiveWebSocketFrameError(Document*, unsigned long identifier, const String& errorMessage);
#endif

#if ENABLE(WEBGL)
    static ScriptObject wrapWebGLRenderingContextForInstrumentation(Document*, const ScriptObject&);
#endif

    static void networkStateChanged(Page*);
    static void updateApplicationCacheStatus(Frame*);

#if ENABLE(INSPECTOR)
    static void frontendCreated() { s_frontendCounter += 1; }
    static void frontendDeleted() { s_frontendCounter -= 1; }
    static bool hasFrontends() { return s_frontendCounter; }
    static bool hasFrontendForScriptContext(ScriptExecutionContext*);
    static bool collectingHTMLParseErrors(Page*);
    static InspectorTimelineAgent* timelineAgentForOrphanEvents();
    static void setTimelineAgentForOrphanEvents(InspectorTimelineAgent*);
#else
    static bool hasFrontends() { return false; }
    static bool hasFrontendForScriptContext(ScriptExecutionContext*) { return false; }
    static bool collectingHTMLParseErrors(Page*) { return false; }
#endif

#if ENABLE(GEOLOCATION)
    static GeolocationPosition* overrideGeolocationPosition(Page*, GeolocationPosition*);
#endif

    static void registerInstrumentingAgents(InstrumentingAgents*);
    static void unregisterInstrumentingAgents(InstrumentingAgents*);

    static DeviceOrientationData* overrideDeviceOrientation(Page*, DeviceOrientationData*);

private:
#if ENABLE(INSPECTOR)
    static WTF::ThreadSpecific<InspectorTimelineAgent*>& threadSpecificTimelineAgentForOrphanEvents();

    static void didClearWindowObjectInWorldImpl(InstrumentingAgents*, Frame*, DOMWrapperWorld*);
    static bool isDebuggerPausedImpl(InstrumentingAgents*);

    static void willInsertDOMNodeImpl(InstrumentingAgents*, Node* parent);
    static void didInsertDOMNodeImpl(InstrumentingAgents*, Node*);
    static void willRemoveDOMNodeImpl(InstrumentingAgents*, Node*);
    static void didRemoveDOMNodeImpl(InstrumentingAgents*, Node*);
    static void willModifyDOMAttrImpl(InstrumentingAgents*, Element*, const AtomicString& oldValue, const AtomicString& newValue);
    static void didModifyDOMAttrImpl(InstrumentingAgents*, Element*, const AtomicString& name, const AtomicString& value);
    static void didRemoveDOMAttrImpl(InstrumentingAgents*, Element*, const AtomicString& name);
    static void characterDataModifiedImpl(InstrumentingAgents*, CharacterData*);
    static void didInvalidateStyleAttrImpl(InstrumentingAgents*, Node*);
    static void frameWindowDiscardedImpl(InstrumentingAgents*, DOMWindow*);
    static void mediaQueryResultChangedImpl(InstrumentingAgents*);
    static void didPushShadowRootImpl(InstrumentingAgents*, Element* host, ShadowRoot*);
    static void willPopShadowRootImpl(InstrumentingAgents*, Element* host, ShadowRoot*);
    static void didCreateNamedFlowImpl(InstrumentingAgents*, Document*, const AtomicString& name);
    static void didRemoveNamedFlowImpl(InstrumentingAgents*, Document*, const AtomicString& name);

    static void mouseDidMoveOverElementImpl(InstrumentingAgents*, const HitTestResult&, unsigned modifierFlags);
    static bool handleMousePressImpl(InstrumentingAgents*);
    static bool forcePseudoStateImpl(InstrumentingAgents*, Element*, CSSSelector::PseudoType);

    static void willSendXMLHttpRequestImpl(InstrumentingAgents*, const String& url);
    static void didScheduleResourceRequestImpl(InstrumentingAgents*, const String& url, Frame*);
    static void didInstallTimerImpl(InstrumentingAgents*, int timerId, int timeout, bool singleShot, ScriptExecutionContext*);
    static void didRemoveTimerImpl(InstrumentingAgents*, int timerId, ScriptExecutionContext*);

    static InspectorInstrumentationCookie willCallFunctionImpl(InstrumentingAgents*, const String& scriptName, int scriptLine, ScriptExecutionContext*);
    static void didCallFunctionImpl(const InspectorInstrumentationCookie&);
    static InspectorInstrumentationCookie willChangeXHRReadyStateImpl(InstrumentingAgents*, XMLHttpRequest*, ScriptExecutionContext*);
    static void didChangeXHRReadyStateImpl(const InspectorInstrumentationCookie&);
    static InspectorInstrumentationCookie willDispatchEventImpl(InstrumentingAgents*, const Event&, DOMWindow*, Node*, const Vector<EventContext>& ancestors, Document*);
    static InspectorInstrumentationCookie willHandleEventImpl(InstrumentingAgents*, Event*);
    static void didHandleEventImpl(const InspectorInstrumentationCookie&);
    static void didDispatchEventImpl(const InspectorInstrumentationCookie&);
    static InspectorInstrumentationCookie willDispatchEventOnWindowImpl(InstrumentingAgents*, const Event&, DOMWindow*);
    static void didDispatchEventOnWindowImpl(const InspectorInstrumentationCookie&);
    static InspectorInstrumentationCookie willEvaluateScriptImpl(InstrumentingAgents*, const String& url, int lineNumber, Frame*);
    static void didEvaluateScriptImpl(const InspectorInstrumentationCookie&);
    static void didCreateIsolatedContextImpl(InstrumentingAgents*, Frame*, ScriptState*, SecurityOrigin*);
    static InspectorInstrumentationCookie willFireTimerImpl(InstrumentingAgents*, int timerId, ScriptExecutionContext*);
    static void didFireTimerImpl(const InspectorInstrumentationCookie&);
    static void didBeginFrameImpl(InstrumentingAgents*);
    static void didCancelFrameImpl(InstrumentingAgents*);
    static InspectorInstrumentationCookie willLayoutImpl(InstrumentingAgents*, Frame*);
    static void didLayoutImpl(const InspectorInstrumentationCookie&);
    static InspectorInstrumentationCookie willLoadXHRImpl(InstrumentingAgents*, XMLHttpRequest*, ScriptExecutionContext*);
    static void didLoadXHRImpl(const InspectorInstrumentationCookie&);
    static InspectorInstrumentationCookie willPaintImpl(InstrumentingAgents*, GraphicsContext*, const LayoutRect&, Frame*);
    static void didPaintImpl(const InspectorInstrumentationCookie&);
    static void willCompositeImpl(InstrumentingAgents*);
    static void didCompositeImpl(InstrumentingAgents*);
    static InspectorInstrumentationCookie willRecalculateStyleImpl(InstrumentingAgents*, Frame*);
    static void didRecalculateStyleImpl(const InspectorInstrumentationCookie&);
    static void didScheduleStyleRecalculationImpl(InstrumentingAgents*, Document*);
    static InspectorInstrumentationCookie willMatchRuleImpl(InstrumentingAgents*, const StyleRule*);
    static void didMatchRuleImpl(const InspectorInstrumentationCookie&, bool matched);
    static InspectorInstrumentationCookie willProcessRuleImpl(InstrumentingAgents*, const StyleRule*);
    static void didProcessRuleImpl(const InspectorInstrumentationCookie&);
    static void willProcessTaskImpl(InstrumentingAgents*);
    static void didProcessTaskImpl(InstrumentingAgents*);

    static void applyUserAgentOverrideImpl(InstrumentingAgents*, String*);
    static void applyScreenWidthOverrideImpl(InstrumentingAgents*, long*);
    static void applyScreenHeightOverrideImpl(InstrumentingAgents*, long*);
    static bool shouldApplyScreenWidthOverrideImpl(InstrumentingAgents*);
    static bool shouldApplyScreenHeightOverrideImpl(InstrumentingAgents*);
    static void willSendRequestImpl(InstrumentingAgents*, unsigned long identifier, DocumentLoader*, ResourceRequest&, const ResourceResponse& redirectResponse);
    static void continueAfterPingLoaderImpl(InstrumentingAgents*, unsigned long identifier, DocumentLoader*, ResourceRequest&, const ResourceResponse&);
    static void markResourceAsCachedImpl(InstrumentingAgents*, unsigned long identifier);
    static void didLoadResourceFromMemoryCacheImpl(InstrumentingAgents*, DocumentLoader*, CachedResource*);
    static InspectorInstrumentationCookie willReceiveResourceDataImpl(InstrumentingAgents*, unsigned long identifier, Frame*, int length);
    static void didReceiveResourceDataImpl(const InspectorInstrumentationCookie&);
    static InspectorInstrumentationCookie willReceiveResourceResponseImpl(InstrumentingAgents*, unsigned long identifier, const ResourceResponse&, Frame*);
    static void didReceiveResourceResponseImpl(const InspectorInstrumentationCookie&, unsigned long identifier, DocumentLoader*, const ResourceResponse&, ResourceLoader*);
    static void didReceiveResourceResponseButCanceledImpl(Frame*, DocumentLoader*, unsigned long identifier, const ResourceResponse&);
    static void continueAfterXFrameOptionsDeniedImpl(Frame*, DocumentLoader*, unsigned long identifier, const ResourceResponse&);
    static void continueWithPolicyDownloadImpl(Frame*, DocumentLoader*, unsigned long identifier, const ResourceResponse&);
    static void continueWithPolicyIgnoreImpl(Frame*, DocumentLoader*, unsigned long identifier, const ResourceResponse&);
    static void didReceiveDataImpl(InstrumentingAgents*, unsigned long identifier, const char* data, int dataLength, int encodedDataLength);
    static void didFinishLoadingImpl(InstrumentingAgents*, unsigned long identifier, DocumentLoader*, double finishTime);
    static void didFailLoadingImpl(InstrumentingAgents*, unsigned long identifier, DocumentLoader*, const ResourceError&);
    static void resourceRetrievedByXMLHttpRequestImpl(InstrumentingAgents*, unsigned long identifier, const String& sourceString, const String& url, const String& sendURL, unsigned sendLineNumber);
    static void didReceiveXHRResponseImpl(InstrumentingAgents*, unsigned long identifier);
    static void willLoadXHRSynchronouslyImpl(InstrumentingAgents*);
    static void didLoadXHRSynchronouslyImpl(InstrumentingAgents*);
    static void scriptImportedImpl(InstrumentingAgents*, unsigned long identifier, const String& sourceString);
    static void didReceiveScriptResponseImpl(InstrumentingAgents*, unsigned long identifier);
    static void domContentLoadedEventFiredImpl(InstrumentingAgents*, Frame*);
    static void loadEventFiredImpl(InstrumentingAgents*, Frame*);
    static void frameDetachedFromParentImpl(InstrumentingAgents*, Frame*);
    static void didCommitLoadImpl(InstrumentingAgents*, Page*, DocumentLoader*);
    static void loaderDetachedFromFrameImpl(InstrumentingAgents*, DocumentLoader*);
    static void willDestroyCachedResourceImpl(CachedResource*);

    static InspectorInstrumentationCookie willWriteHTMLImpl(InstrumentingAgents*, unsigned int length, unsigned int startLine, Frame*);
    static void didWriteHTMLImpl(const InspectorInstrumentationCookie&, unsigned int endLine);

    static void addMessageToConsoleImpl(InstrumentingAgents*, MessageSource, MessageType, MessageLevel, const String& message, PassRefPtr<ScriptArguments>, PassRefPtr<ScriptCallStack>);
    static void addMessageToConsoleImpl(InstrumentingAgents*, MessageSource, MessageType, MessageLevel, const String& message, const String& scriptId, unsigned lineNumber);
    static void consoleCountImpl(InstrumentingAgents*, PassRefPtr<ScriptArguments>, PassRefPtr<ScriptCallStack>);
    static void startConsoleTimingImpl(InstrumentingAgents*, Frame*, const String& title);
    static void stopConsoleTimingImpl(InstrumentingAgents*, Frame*, const String& title, PassRefPtr<ScriptCallStack>);
    static void consoleTimeStampImpl(InstrumentingAgents*, Frame*, PassRefPtr<ScriptArguments>);

    static void didRequestAnimationFrameImpl(InstrumentingAgents*, int callbackId, Frame*);
    static void didCancelAnimationFrameImpl(InstrumentingAgents*, int callbackId, Frame*);
    static InspectorInstrumentationCookie willFireAnimationFrameImpl(InstrumentingAgents*, int callbackId, Frame*);
    static void didFireAnimationFrameImpl(const InspectorInstrumentationCookie&);

#if ENABLE(JAVASCRIPT_DEBUGGER)
    static void addStartProfilingMessageToConsoleImpl(InstrumentingAgents*, const String& title, unsigned lineNumber, const String& sourceURL);
    static void addProfileImpl(InstrumentingAgents*, RefPtr<ScriptProfile>, PassRefPtr<ScriptCallStack>);
    static String getCurrentUserInitiatedProfileNameImpl(InstrumentingAgents*, bool incrementProfileNumber);
    static bool profilerEnabledImpl(InstrumentingAgents*);
#endif

#if ENABLE(SQL_DATABASE)
    static void didOpenDatabaseImpl(InstrumentingAgents*, PassRefPtr<Database>, const String& domain, const String& name, const String& version);
#endif

    static void didUseDOMStorageImpl(InstrumentingAgents*, StorageArea*, bool isLocalStorage, Frame*);
    static void didDispatchDOMStorageEventImpl(InstrumentingAgents*, const String& key, const String& oldValue, const String& newValue, StorageType, SecurityOrigin*, Page*);

#if ENABLE(WORKERS)
    static bool shouldPauseDedicatedWorkerOnStartImpl(InstrumentingAgents*);
    static void didStartWorkerContextImpl(InstrumentingAgents*, WorkerContextProxy*, const KURL&);
    static void workerContextTerminatedImpl(InstrumentingAgents*, WorkerContextProxy*);
#endif

#if ENABLE(WEB_SOCKETS)
    static void didCreateWebSocketImpl(InstrumentingAgents*, unsigned long identifier, const KURL& requestURL, const KURL& documentURL);
    static void willSendWebSocketHandshakeRequestImpl(InstrumentingAgents*, unsigned long identifier, const WebSocketHandshakeRequest&);
    static void didReceiveWebSocketHandshakeResponseImpl(InstrumentingAgents*, unsigned long identifier, const WebSocketHandshakeResponse&);
    static void didCloseWebSocketImpl(InstrumentingAgents*, unsigned long identifier);
    static void didReceiveWebSocketFrameImpl(InstrumentingAgents*, unsigned long identifier, const WebSocketFrame&);
    static void didSendWebSocketFrameImpl(InstrumentingAgents*, unsigned long identifier, const WebSocketFrame&);
    static void didReceiveWebSocketFrameErrorImpl(InstrumentingAgents*, unsigned long identifier, const String&);
#endif

    static void networkStateChangedImpl(InstrumentingAgents*);
    static void updateApplicationCacheStatusImpl(InstrumentingAgents*, Frame*);

    static InstrumentingAgents* instrumentingAgentsForPage(Page*);
    static InstrumentingAgents* instrumentingAgentsForFrame(Frame*);
    static InstrumentingAgents* instrumentingAgentsForContext(ScriptExecutionContext*);
    static InstrumentingAgents* instrumentingAgentsForDocument(Document*);
#if ENABLE(WORKERS)
    static InstrumentingAgents* instrumentingAgentsForWorkerContext(WorkerContext*);
    static InstrumentingAgents* instrumentingAgentsForNonDocumentContext(ScriptExecutionContext*);
#endif

    static bool collectingHTMLParseErrors(InstrumentingAgents*);
    static void pauseOnNativeEventIfNeeded(InstrumentingAgents*, bool isDOMEvent, const String& eventName, bool synchronous);
    static void cancelPauseOnNativeEvent(InstrumentingAgents*);
    static InspectorTimelineAgent* retrieveTimelineAgent(const InspectorInstrumentationCookie&);

#if ENABLE(GEOLOCATION)
    static GeolocationPosition* overrideGeolocationPositionImpl(InstrumentingAgents*, GeolocationPosition*);
#endif

    static DeviceOrientationData* overrideDeviceOrientationImpl(InstrumentingAgents*, DeviceOrientationData*);
    static int s_frontendCounter;
#endif
};

inline void InspectorInstrumentation::didClearWindowObjectInWorld(Frame* frame, DOMWrapperWorld* world)
{
#if ENABLE(INSPECTOR)
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        didClearWindowObjectInWorldImpl(instrumentingAgents, frame, world);
#endif
}

inline bool InspectorInstrumentation::isDebuggerPaused(Frame* frame)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(false);
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        return isDebuggerPausedImpl(instrumentingAgents);
#endif
    return false;
}

inline void InspectorInstrumentation::willInsertDOMNode(Document* document, Node* parent)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        willInsertDOMNodeImpl(instrumentingAgents, parent);
#endif
}

inline void InspectorInstrumentation::didInsertDOMNode(Document* document, Node* node)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        didInsertDOMNodeImpl(instrumentingAgents, node);
#endif
}

inline void InspectorInstrumentation::willRemoveDOMNode(Document* document, Node* node)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document)) {
        willRemoveDOMNodeImpl(instrumentingAgents, node);
        didRemoveDOMNodeImpl(instrumentingAgents, node);
    }
#endif
}

inline void InspectorInstrumentation::willModifyDOMAttr(Document* document, Element* element, const AtomicString& oldValue, const AtomicString& newValue)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        willModifyDOMAttrImpl(instrumentingAgents, element, oldValue, newValue);
#endif
}

inline void InspectorInstrumentation::didModifyDOMAttr(Document* document, Element* element, const AtomicString& name, const AtomicString& value)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        didModifyDOMAttrImpl(instrumentingAgents, element, name, value);
#endif
}

inline void InspectorInstrumentation::didRemoveDOMAttr(Document* document, Element* element, const AtomicString& name)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        didRemoveDOMAttrImpl(instrumentingAgents, element, name);
#endif
}

inline void InspectorInstrumentation::didInvalidateStyleAttr(Document* document, Node* node)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        didInvalidateStyleAttrImpl(instrumentingAgents, node);
#endif
}

inline void InspectorInstrumentation::frameWindowDiscarded(Frame* frame, DOMWindow* domWindow)
{
#if ENABLE(INSPECTOR)
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        frameWindowDiscardedImpl(instrumentingAgents, domWindow);
#endif
}

inline void InspectorInstrumentation::mediaQueryResultChanged(Document* document)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        mediaQueryResultChangedImpl(instrumentingAgents);
#endif
}

inline void InspectorInstrumentation::didPushShadowRoot(Element* host, ShadowRoot* root)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(host->ownerDocument()))
        didPushShadowRootImpl(instrumentingAgents, host, root);
#endif
}

inline void InspectorInstrumentation::willPopShadowRoot(Element* host, ShadowRoot* root)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(host->ownerDocument()))
        willPopShadowRootImpl(instrumentingAgents, host, root);
#endif
}

inline void InspectorInstrumentation::didCreateNamedFlow(Document* document, const AtomicString& name)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        didCreateNamedFlowImpl(instrumentingAgents, document, name);
#endif
}

inline void InspectorInstrumentation::didRemoveNamedFlow(Document* document, const AtomicString& name)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        didRemoveNamedFlowImpl(instrumentingAgents, document, name);
#endif
}

inline void InspectorInstrumentation::mouseDidMoveOverElement(Page* page, const HitTestResult& result, unsigned modifierFlags)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForPage(page))
        mouseDidMoveOverElementImpl(instrumentingAgents, result, modifierFlags);
#endif
}

inline bool InspectorInstrumentation::handleMousePress(Page* page)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(false);
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForPage(page))
        return handleMousePressImpl(instrumentingAgents);
#endif
    return false;
}

inline bool InspectorInstrumentation::forcePseudoState(Element* element, CSSSelector::PseudoType pseudoState)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(false);
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(element->document()))
        return forcePseudoStateImpl(instrumentingAgents, element, pseudoState);
#endif
    return false;
}

inline void InspectorInstrumentation::characterDataModified(Document* document, CharacterData* characterData)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        characterDataModifiedImpl(instrumentingAgents, characterData);
#endif
}

inline void InspectorInstrumentation::willSendXMLHttpRequest(ScriptExecutionContext* context, const String& url)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        willSendXMLHttpRequestImpl(instrumentingAgents, url);
#endif
}

inline void InspectorInstrumentation::didScheduleResourceRequest(Document* document, const String& url)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        didScheduleResourceRequestImpl(instrumentingAgents, url, document->frame());
#endif
}

inline void InspectorInstrumentation::didInstallTimer(ScriptExecutionContext* context, int timerId, int timeout, bool singleShot)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        didInstallTimerImpl(instrumentingAgents, timerId, timeout, singleShot, context);
#endif
}

inline void InspectorInstrumentation::didRemoveTimer(ScriptExecutionContext* context, int timerId)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        didRemoveTimerImpl(instrumentingAgents, timerId, context);
#endif
}

inline InspectorInstrumentationCookie InspectorInstrumentation::willCallFunction(ScriptExecutionContext* context, const String& scriptName, int scriptLine)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(InspectorInstrumentationCookie());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        return willCallFunctionImpl(instrumentingAgents, scriptName, scriptLine, context);
#endif
    return InspectorInstrumentationCookie();
}


inline void InspectorInstrumentation::didCallFunction(const InspectorInstrumentationCookie& cookie)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (cookie.first)
        didCallFunctionImpl(cookie);
#endif
}

inline InspectorInstrumentationCookie InspectorInstrumentation::willChangeXHRReadyState(ScriptExecutionContext* context, XMLHttpRequest* request)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(InspectorInstrumentationCookie());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        return willChangeXHRReadyStateImpl(instrumentingAgents, request, context);
#endif
    return InspectorInstrumentationCookie();
}

inline void InspectorInstrumentation::didChangeXHRReadyState(const InspectorInstrumentationCookie& cookie)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (cookie.first)
        didChangeXHRReadyStateImpl(cookie);
#endif
}

inline InspectorInstrumentationCookie InspectorInstrumentation::willDispatchEvent(Document* document, const Event& event, DOMWindow* window, Node* node, const Vector<EventContext>& ancestors)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(InspectorInstrumentationCookie());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        return willDispatchEventImpl(instrumentingAgents, event, window, node, ancestors, document);
#endif
    return InspectorInstrumentationCookie();
}

inline void InspectorInstrumentation::didDispatchEvent(const InspectorInstrumentationCookie& cookie)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (cookie.first)
        didDispatchEventImpl(cookie);
#endif
}

inline InspectorInstrumentationCookie InspectorInstrumentation::willHandleEvent(ScriptExecutionContext* context, Event* event)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(InspectorInstrumentationCookie());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        return willHandleEventImpl(instrumentingAgents, event);
#endif
    return InspectorInstrumentationCookie();
}

inline void InspectorInstrumentation::didHandleEvent(const InspectorInstrumentationCookie& cookie)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (cookie.first)
        didHandleEventImpl(cookie);
#endif
}

inline InspectorInstrumentationCookie InspectorInstrumentation::willDispatchEventOnWindow(Frame* frame, const Event& event, DOMWindow* window)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(InspectorInstrumentationCookie());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        return willDispatchEventOnWindowImpl(instrumentingAgents, event, window);
#endif
    return InspectorInstrumentationCookie();
}

inline void InspectorInstrumentation::didDispatchEventOnWindow(const InspectorInstrumentationCookie& cookie)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (cookie.first)
        didDispatchEventOnWindowImpl(cookie);
#endif
}

inline InspectorInstrumentationCookie InspectorInstrumentation::willEvaluateScript(Frame* frame, const String& url, int lineNumber)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(InspectorInstrumentationCookie());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        return willEvaluateScriptImpl(instrumentingAgents, url, lineNumber, frame);
#endif
    return InspectorInstrumentationCookie();
}

inline void InspectorInstrumentation::didEvaluateScript(const InspectorInstrumentationCookie& cookie)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (cookie.first)
        didEvaluateScriptImpl(cookie);
#endif
}

inline void InspectorInstrumentation::didCreateIsolatedContext(Frame* frame, ScriptState* scriptState, SecurityOrigin* origin)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        return didCreateIsolatedContextImpl(instrumentingAgents, frame, scriptState, origin);
#endif
}

inline InspectorInstrumentationCookie InspectorInstrumentation::willFireTimer(ScriptExecutionContext* context, int timerId)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(InspectorInstrumentationCookie());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        return willFireTimerImpl(instrumentingAgents, timerId, context);
#endif
    return InspectorInstrumentationCookie();
}

inline void InspectorInstrumentation::didFireTimer(const InspectorInstrumentationCookie& cookie)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (cookie.first)
        didFireTimerImpl(cookie);
#endif
}

inline void InspectorInstrumentation::didBeginFrame(Page* page)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForPage(page))
        didBeginFrameImpl(instrumentingAgents);
#endif
}

inline void InspectorInstrumentation::didCancelFrame(Page* page)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForPage(page))
        didCancelFrameImpl(instrumentingAgents);
#endif
}

inline InspectorInstrumentationCookie InspectorInstrumentation::willLayout(Frame* frame)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(InspectorInstrumentationCookie());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        return willLayoutImpl(instrumentingAgents, frame);
#endif
    return InspectorInstrumentationCookie();
}

inline void InspectorInstrumentation::didLayout(const InspectorInstrumentationCookie& cookie)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (cookie.first)
        didLayoutImpl(cookie);
#endif
}

inline InspectorInstrumentationCookie InspectorInstrumentation::willLoadXHR(ScriptExecutionContext* context, XMLHttpRequest* request)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(InspectorInstrumentationCookie());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        return willLoadXHRImpl(instrumentingAgents, request, context);
#endif
    return InspectorInstrumentationCookie();
}

inline void InspectorInstrumentation::didLoadXHR(const InspectorInstrumentationCookie& cookie)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (cookie.first)
        didLoadXHRImpl(cookie);
#endif
}

inline InspectorInstrumentationCookie InspectorInstrumentation::willPaint(Frame* frame, GraphicsContext* context, const LayoutRect& rect)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(InspectorInstrumentationCookie());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        return willPaintImpl(instrumentingAgents, context, rect, frame);
#endif
    return InspectorInstrumentationCookie();
}

inline void InspectorInstrumentation::didPaint(const InspectorInstrumentationCookie& cookie)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (cookie.first)
        didPaintImpl(cookie);
#endif
}

inline void InspectorInstrumentation::willComposite(Page* page)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForPage(page))
        willCompositeImpl(instrumentingAgents);
#endif
}

inline void InspectorInstrumentation::didComposite(Page* page)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForPage(page))
        didCompositeImpl(instrumentingAgents);
#endif
}

inline InspectorInstrumentationCookie InspectorInstrumentation::willRecalculateStyle(Document* document)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(InspectorInstrumentationCookie());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        return willRecalculateStyleImpl(instrumentingAgents, document->frame());
#endif
    return InspectorInstrumentationCookie();
}

inline void InspectorInstrumentation::didRecalculateStyle(const InspectorInstrumentationCookie& cookie)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (cookie.first)
        didRecalculateStyleImpl(cookie);
#endif
}

inline void InspectorInstrumentation::didScheduleStyleRecalculation(Document* document)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        didScheduleStyleRecalculationImpl(instrumentingAgents, document);
#endif
}

inline InspectorInstrumentationCookie InspectorInstrumentation::willMatchRule(Document* document, const StyleRule* rule)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(InspectorInstrumentationCookie());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        return willMatchRuleImpl(instrumentingAgents, rule);
#endif
    return InspectorInstrumentationCookie();
}

inline void InspectorInstrumentation::didMatchRule(const InspectorInstrumentationCookie& cookie, bool matched)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (cookie.first)
        didMatchRuleImpl(cookie, matched);
#endif
}

inline InspectorInstrumentationCookie InspectorInstrumentation::willProcessRule(Document* document, const StyleRule* rule)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(InspectorInstrumentationCookie());
    if (!rule)
        return InspectorInstrumentationCookie();
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        return willProcessRuleImpl(instrumentingAgents, rule);
#endif
    return InspectorInstrumentationCookie();
}

inline void InspectorInstrumentation::didProcessRule(const InspectorInstrumentationCookie& cookie)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (cookie.first)
        didProcessRuleImpl(cookie);
#endif
}

inline void InspectorInstrumentation::willProcessTask(Page* page)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForPage(page))
        willProcessTaskImpl(instrumentingAgents);
#endif
}

inline void InspectorInstrumentation::didProcessTask(Page* page)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForPage(page))
        didProcessTaskImpl(instrumentingAgents);
#endif
}

inline void InspectorInstrumentation::applyUserAgentOverride(Frame* frame, String* userAgent)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        applyUserAgentOverrideImpl(instrumentingAgents, userAgent);
#endif
}

inline void InspectorInstrumentation::applyScreenWidthOverride(Frame* frame, long* width)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        applyScreenWidthOverrideImpl(instrumentingAgents, width);
#endif
}

inline void InspectorInstrumentation::applyScreenHeightOverride(Frame* frame, long* height)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        applyScreenHeightOverrideImpl(instrumentingAgents, height);
#endif
}

inline bool InspectorInstrumentation::shouldApplyScreenWidthOverride(Frame* frame)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(false);
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        return shouldApplyScreenWidthOverrideImpl(instrumentingAgents);
#endif
    return false;
}

inline bool InspectorInstrumentation::shouldApplyScreenHeightOverride(Frame* frame)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(false);
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        return shouldApplyScreenHeightOverrideImpl(instrumentingAgents);
#endif
    return false;
}

inline void InspectorInstrumentation::willSendRequest(Frame* frame, unsigned long identifier, DocumentLoader* loader, ResourceRequest& request, const ResourceResponse& redirectResponse)
{
#if ENABLE(INSPECTOR)
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        willSendRequestImpl(instrumentingAgents, identifier, loader, request, redirectResponse);
#endif
}

inline void InspectorInstrumentation::continueAfterPingLoader(Frame* frame, unsigned long identifier, DocumentLoader* loader, ResourceRequest& request, const ResourceResponse& response)
{
#if ENABLE(INSPECTOR)
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        InspectorInstrumentation::continueAfterPingLoaderImpl(instrumentingAgents, identifier, loader, request, response);
#endif
}

inline void InspectorInstrumentation::markResourceAsCached(Page* page, unsigned long identifier)
{
#if ENABLE(INSPECTOR)
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForPage(page))
        markResourceAsCachedImpl(instrumentingAgents, identifier);
#endif
}

inline void InspectorInstrumentation::didLoadResourceFromMemoryCache(Page* page, DocumentLoader* loader, CachedResource* resource)
{
#if ENABLE(INSPECTOR)
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForPage(page))
        didLoadResourceFromMemoryCacheImpl(instrumentingAgents, loader, resource);
#endif
}

inline InspectorInstrumentationCookie InspectorInstrumentation::willReceiveResourceData(Frame* frame, unsigned long identifier, int length)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(InspectorInstrumentationCookie());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        return willReceiveResourceDataImpl(instrumentingAgents, identifier, frame, length);
#endif
    return InspectorInstrumentationCookie();
}

inline void InspectorInstrumentation::didReceiveResourceData(const InspectorInstrumentationCookie& cookie)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (cookie.first)
        didReceiveResourceDataImpl(cookie);
#endif
}

inline InspectorInstrumentationCookie InspectorInstrumentation::willReceiveResourceResponse(Frame* frame, unsigned long identifier, const ResourceResponse& response)
{
#if ENABLE(INSPECTOR)
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        return willReceiveResourceResponseImpl(instrumentingAgents, identifier, response, frame);
#endif
    return InspectorInstrumentationCookie();
}

inline void InspectorInstrumentation::didReceiveResourceResponse(const InspectorInstrumentationCookie& cookie, unsigned long identifier, DocumentLoader* loader, const ResourceResponse& response, ResourceLoader* resourceLoader)
{
#if ENABLE(INSPECTOR)
    // Call this unconditionally so that we're able to log to console with no front-end attached.
    didReceiveResourceResponseImpl(cookie, identifier, loader, response, resourceLoader);
#endif
}

inline void InspectorInstrumentation::continueAfterXFrameOptionsDenied(Frame* frame, DocumentLoader* loader, unsigned long identifier, const ResourceResponse& r)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(void());
    InspectorInstrumentation::continueAfterXFrameOptionsDeniedImpl(frame, loader, identifier, r);
#endif
}

inline void InspectorInstrumentation::continueWithPolicyDownload(Frame* frame, DocumentLoader* loader, unsigned long identifier, const ResourceResponse& r)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(void());
    InspectorInstrumentation::continueWithPolicyDownloadImpl(frame, loader, identifier, r);
#endif
}

inline void InspectorInstrumentation::continueWithPolicyIgnore(Frame* frame, DocumentLoader* loader, unsigned long identifier, const ResourceResponse& r)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(void());
    InspectorInstrumentation::continueWithPolicyIgnoreImpl(frame, loader, identifier, r);
#endif
}

inline void InspectorInstrumentation::didReceiveData(Frame* frame, unsigned long identifier, const char* data, int dataLength, int encodedDataLength)
{
#if ENABLE(INSPECTOR)
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        didReceiveDataImpl(instrumentingAgents, identifier, data, dataLength, encodedDataLength);
#endif
}

inline void InspectorInstrumentation::didFinishLoading(Frame* frame, DocumentLoader* loader, unsigned long identifier, double finishTime)
{
#if ENABLE(INSPECTOR)
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        didFinishLoadingImpl(instrumentingAgents, identifier, loader, finishTime);
#endif
}

inline void InspectorInstrumentation::didFailLoading(Frame* frame, DocumentLoader* loader, unsigned long identifier, const ResourceError& error)
{
#if ENABLE(INSPECTOR)
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        didFailLoadingImpl(instrumentingAgents, identifier, loader, error);
#endif
}

inline void InspectorInstrumentation::resourceRetrievedByXMLHttpRequest(ScriptExecutionContext* context, unsigned long identifier, const String& sourceString, const String& url, const String& sendURL, unsigned sendLineNumber)
{
#if ENABLE(INSPECTOR)
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        resourceRetrievedByXMLHttpRequestImpl(instrumentingAgents, identifier, sourceString, url, sendURL, sendLineNumber);
#endif
}

inline void InspectorInstrumentation::didReceiveXHRResponse(ScriptExecutionContext* context, unsigned long identifier)
{
#if ENABLE(INSPECTOR)
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        didReceiveXHRResponseImpl(instrumentingAgents, identifier);
#endif
}

inline void InspectorInstrumentation::willLoadXHRSynchronously(ScriptExecutionContext* context)
{
#if ENABLE(INSPECTOR)
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        willLoadXHRSynchronouslyImpl(instrumentingAgents);
#endif
}

inline void InspectorInstrumentation::didLoadXHRSynchronously(ScriptExecutionContext* context)
{
#if ENABLE(INSPECTOR)
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        didLoadXHRSynchronouslyImpl(instrumentingAgents);
#endif
}

inline void InspectorInstrumentation::scriptImported(ScriptExecutionContext* context, unsigned long identifier, const String& sourceString)
{
#if ENABLE(INSPECTOR)
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        scriptImportedImpl(instrumentingAgents, identifier, sourceString);
#endif
}

inline void InspectorInstrumentation::didReceiveScriptResponse(ScriptExecutionContext* context, unsigned long identifier)
{
#if ENABLE(INSPECTOR)
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        didReceiveScriptResponseImpl(instrumentingAgents, identifier);
#endif
}

inline void InspectorInstrumentation::domContentLoadedEventFired(Frame* frame)
{
#if ENABLE(INSPECTOR)
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        domContentLoadedEventFiredImpl(instrumentingAgents, frame);
#endif
}

inline void InspectorInstrumentation::loadEventFired(Frame* frame)
{
#if ENABLE(INSPECTOR)
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        loadEventFiredImpl(instrumentingAgents, frame);
#endif
}

inline void InspectorInstrumentation::frameDetachedFromParent(Frame* frame)
{
#if ENABLE(INSPECTOR)
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        frameDetachedFromParentImpl(instrumentingAgents, frame);
#endif
}

inline void InspectorInstrumentation::didCommitLoad(Frame* frame, DocumentLoader* loader)
{
#if ENABLE(INSPECTOR)
    if (!frame)
        return;
    Page* page = frame->page();
    if (!page)
        return;
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForPage(page))
        didCommitLoadImpl(instrumentingAgents, page, loader);
#endif
}

inline void InspectorInstrumentation::loaderDetachedFromFrame(Frame* frame, DocumentLoader* loader)
{
#if ENABLE(INSPECTOR)
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        loaderDetachedFromFrameImpl(instrumentingAgents, loader);
#endif
}

inline void InspectorInstrumentation::willDestroyCachedResource(CachedResource* cachedResource)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(void());
    willDestroyCachedResourceImpl(cachedResource);
#endif
}

inline InspectorInstrumentationCookie InspectorInstrumentation::willWriteHTML(Document* document, unsigned int length, unsigned int startLine)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(InspectorInstrumentationCookie());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        return willWriteHTMLImpl(instrumentingAgents, length, startLine, document->frame());
#endif
    return InspectorInstrumentationCookie();
}

inline void InspectorInstrumentation::didWriteHTML(const InspectorInstrumentationCookie& cookie, unsigned int endLine)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (cookie.first)
        didWriteHTMLImpl(cookie, endLine);
#endif
}

inline void InspectorInstrumentation::didUseDOMStorage(Page* page, StorageArea* storageArea, bool isLocalStorage, Frame* frame)
{
#if ENABLE(INSPECTOR)
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForPage(page))
        didUseDOMStorageImpl(instrumentingAgents, storageArea, isLocalStorage, frame);
#endif
}

inline void InspectorInstrumentation::didDispatchDOMStorageEvent(const String& key, const String& oldValue, const String& newValue, StorageType storageType, SecurityOrigin* securityOrigin, Page* page)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForPage(page))
        didDispatchDOMStorageEventImpl(instrumentingAgents, key, oldValue, newValue, storageType, securityOrigin, page);
#endif // ENABLE(INSPECTOR)
}

#if ENABLE(WORKERS)
inline bool InspectorInstrumentation::shouldPauseDedicatedWorkerOnStart(ScriptExecutionContext* context)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(false);
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        return shouldPauseDedicatedWorkerOnStartImpl(instrumentingAgents);
#endif
    return false;
}

inline void InspectorInstrumentation::didStartWorkerContext(ScriptExecutionContext* context, WorkerContextProxy* proxy, const KURL& url)
{
#if ENABLE(INSPECTOR)
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        didStartWorkerContextImpl(instrumentingAgents, proxy, url);
#endif
}

inline void InspectorInstrumentation::workerContextTerminated(ScriptExecutionContext* context, WorkerContextProxy* proxy)
{
#if ENABLE(INSPECTOR)
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        workerContextTerminatedImpl(instrumentingAgents, proxy);
#endif
}

#endif


#if ENABLE(WEB_SOCKETS)
inline void InspectorInstrumentation::didCreateWebSocket(ScriptExecutionContext* context, unsigned long identifier, const KURL& requestURL, const KURL& documentURL)
{
#if ENABLE(INSPECTOR)
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        didCreateWebSocketImpl(instrumentingAgents, identifier, requestURL, documentURL);
#endif
}

inline void InspectorInstrumentation::willSendWebSocketHandshakeRequest(ScriptExecutionContext* context, unsigned long identifier, const WebSocketHandshakeRequest& request)
{
#if ENABLE(INSPECTOR)
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        willSendWebSocketHandshakeRequestImpl(instrumentingAgents, identifier, request);
#endif
}

inline void InspectorInstrumentation::didReceiveWebSocketHandshakeResponse(ScriptExecutionContext* context, unsigned long identifier, const WebSocketHandshakeResponse& response)
{
#if ENABLE(INSPECTOR)
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        didReceiveWebSocketHandshakeResponseImpl(instrumentingAgents, identifier, response);
#endif
}

inline void InspectorInstrumentation::didCloseWebSocket(ScriptExecutionContext* context, unsigned long identifier)
{
#if ENABLE(INSPECTOR)
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForContext(context))
        didCloseWebSocketImpl(instrumentingAgents, identifier);
#endif
}
inline void InspectorInstrumentation::didReceiveWebSocketFrame(Document* document, unsigned long identifier, const WebSocketFrame& frame)
{
#if ENABLE(INSPECTOR)
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        didReceiveWebSocketFrameImpl(instrumentingAgents, identifier, frame);
#endif
}
inline void InspectorInstrumentation::didReceiveWebSocketFrameError(Document* document, unsigned long identifier, const String& errorMessage)
{
#if ENABLE(INSPECTOR)
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        didReceiveWebSocketFrameErrorImpl(instrumentingAgents, identifier, errorMessage);
#endif
}
inline void InspectorInstrumentation::didSendWebSocketFrame(Document* document, unsigned long identifier, const WebSocketFrame& frame)
{
#if ENABLE(INSPECTOR)
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        didSendWebSocketFrameImpl(instrumentingAgents, identifier, frame);
#endif
}
#endif

inline void InspectorInstrumentation::networkStateChanged(Page* page)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForPage(page))
        networkStateChangedImpl(instrumentingAgents);
#endif
}

inline void InspectorInstrumentation::updateApplicationCacheStatus(Frame* frame)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        updateApplicationCacheStatusImpl(instrumentingAgents, frame);
#endif
}

inline void InspectorInstrumentation::didRequestAnimationFrame(Document* document, int callbackId)
{
#if ENABLE(INSPECTOR)
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        didRequestAnimationFrameImpl(instrumentingAgents, callbackId, document->frame());
#endif
}

inline void InspectorInstrumentation::didCancelAnimationFrame(Document* document, int callbackId)
{
#if ENABLE(INSPECTOR)
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        didCancelAnimationFrameImpl(instrumentingAgents, callbackId, document->frame());
#endif
}

inline InspectorInstrumentationCookie InspectorInstrumentation::willFireAnimationFrame(Document* document, int callbackId)
{
#if ENABLE(INSPECTOR)
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        return willFireAnimationFrameImpl(instrumentingAgents, callbackId, document->frame());
#endif
    return InspectorInstrumentationCookie();
}

inline void InspectorInstrumentation::didFireAnimationFrame(const InspectorInstrumentationCookie& cookie)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (cookie.first)
        didFireAnimationFrameImpl(cookie);
#endif
}

#if ENABLE(GEOLOCATION)
inline GeolocationPosition* InspectorInstrumentation::overrideGeolocationPosition(Page* page, GeolocationPosition* position)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(position);
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForPage(page))
        return overrideGeolocationPositionImpl(instrumentingAgents, position);
#endif
    return position;
}
#endif

inline DeviceOrientationData* InspectorInstrumentation::overrideDeviceOrientation(Page* page, DeviceOrientationData* deviceOrientation)
{
#if ENABLE(INSPECTOR)
    FAST_RETURN_IF_NO_FRONTENDS(deviceOrientation);
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForPage(page))
        return overrideDeviceOrientationImpl(instrumentingAgents, deviceOrientation);
#endif
    return deviceOrientation;
}

#if ENABLE(INSPECTOR)
inline bool InspectorInstrumentation::collectingHTMLParseErrors(Page* page)
{
    FAST_RETURN_IF_NO_FRONTENDS(false);
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForPage(page))
        return collectingHTMLParseErrors(instrumentingAgents);
    return false;
}

inline InstrumentingAgents* InspectorInstrumentation::instrumentingAgentsForContext(ScriptExecutionContext* context)
{
    if (!context)
        return 0;
    if (context->isDocument())
        return instrumentingAgentsForPage(static_cast<Document*>(context)->page());
#if ENABLE(WORKERS)
    return instrumentingAgentsForNonDocumentContext(context);
#else
    return 0;
#endif
}

inline InstrumentingAgents* InspectorInstrumentation::instrumentingAgentsForFrame(Frame* frame)
{
    if (frame)
        return instrumentingAgentsForPage(frame->page());
    return 0;
}

inline InstrumentingAgents* InspectorInstrumentation::instrumentingAgentsForDocument(Document* document)
{
    if (document)
        return instrumentingAgentsForPage(document->page());
    return 0;
}
#endif

} // namespace WebCore

#endif // !defined(InspectorInstrumentation_h)
