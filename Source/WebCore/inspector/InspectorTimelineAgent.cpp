/*
* Copyright (C) 2013 Google Inc. All rights reserved.
* Copyright (C) 2014 University of Washington.
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
#include "InspectorTimelineAgent.h"

#include "Event.h"
#include "Frame.h"
#include "FrameView.h"
#include "InspectorClient.h"
#include "InspectorInstrumentation.h"
#include "InspectorPageAgent.h"
#include "InstrumentingAgents.h"
#include "IntRect.h"
#include "JSDOMWindow.h"
#include "MainFrame.h"
#include "PageScriptDebugServer.h"
#include "RenderElement.h"
#include "RenderView.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include "ScriptState.h"
#include "TimelineRecordFactory.h"
#include <inspector/IdentifiersFactory.h>
#include <inspector/ScriptBreakpoint.h>
#include <profiler/LegacyProfiler.h>
#include <wtf/CurrentTime.h>

#if PLATFORM(IOS)
#include "RuntimeApplicationChecksIOS.h"
#include <WebCore/WebCoreThread.h>
#endif

#if PLATFORM(COCOA)
#include <WebCore/RunLoopObserver.h>
#endif

using namespace Inspector;

namespace WebCore {

#if PLATFORM(COCOA)
static const CFIndex frameStopRunLoopOrder = (CFIndex)RunLoopObserver::WellKnownRunLoopOrders::CoreAnimationCommit + 1;

static CFRunLoopRef currentRunLoop()
{
#if PLATFORM(IOS)
    // A race condition during WebView deallocation can lead to a crash if the layer sync run loop
    // observer is added to the main run loop <rdar://problem/9798550>. However, for responsiveness,
    // we still allow this, see <rdar://problem/7403328>. Since the race condition and subsequent
    // crash are especially troublesome for iBooks, we never allow the observer to be added to the
    // main run loop in iBooks.
    if (applicationIsIBooksOnIOS())
        return WebThreadRunLoop();
#endif
    return CFRunLoopGetCurrent();
}
#endif

InspectorTimelineAgent::~InspectorTimelineAgent()
{
}

void InspectorTimelineAgent::didCreateFrontendAndBackend(Inspector::FrontendChannel* frontendChannel, Inspector::BackendDispatcher* backendDispatcher)
{
    m_frontendDispatcher = std::make_unique<Inspector::TimelineFrontendDispatcher>(frontendChannel);
    m_backendDispatcher = Inspector::TimelineBackendDispatcher::create(backendDispatcher, this);

    m_instrumentingAgents->setPersistentInspectorTimelineAgent(this);

    if (m_scriptDebugServer)
        m_scriptDebugServer->recompileAllJSFunctions();
}

void InspectorTimelineAgent::willDestroyFrontendAndBackend(Inspector::DisconnectReason reason)
{
    m_frontendDispatcher = nullptr;
    m_backendDispatcher = nullptr;

    m_instrumentingAgents->setPersistentInspectorTimelineAgent(nullptr);

    if (reason != Inspector::DisconnectReason::InspectedTargetDestroyed) {
        if (m_scriptDebugServer)
            m_scriptDebugServer->recompileAllJSFunctions();
    }

    ErrorString unused;
    stop(unused);
}

void InspectorTimelineAgent::start(ErrorString&, const int* maxCallStackDepth)
{
    m_enabledFromFrontend = true;

    internalStart(maxCallStackDepth);
}

void InspectorTimelineAgent::stop(ErrorString&)
{
    internalStop();

    m_enabledFromFrontend = false;
}

void InspectorTimelineAgent::internalStart(const int* maxCallStackDepth)
{
    if (m_enabled)
        return;

    if (maxCallStackDepth && *maxCallStackDepth > 0)
        m_maxCallStackDepth = *maxCallStackDepth;
    else
        m_maxCallStackDepth = 5;

    m_instrumentingAgents->setInspectorTimelineAgent(this);

    if (m_scriptDebugServer)
        m_scriptDebugServer->addListener(this);

    m_enabled = true;

    // FIXME: Abstract away platform-specific code once https://bugs.webkit.org/show_bug.cgi?id=142748 is fixed.

#if PLATFORM(COCOA)
    m_frameStartObserver = RunLoopObserver::create(0, [this]() {
        if (!m_enabled || m_scriptDebugServer->isPaused())
            return;

        if (!m_runLoopNestingLevel)
            pushCurrentRecord(InspectorObject::create(), TimelineRecordType::RenderingFrame, false, nullptr);
        m_runLoopNestingLevel++;
    });

    m_frameStopObserver = RunLoopObserver::create(frameStopRunLoopOrder, [this]() {
        if (!m_enabled || m_scriptDebugServer->isPaused())
            return;

        ASSERT(m_runLoopNestingLevel > 0);
        m_runLoopNestingLevel--;
        if (m_runLoopNestingLevel)
            return;

        if (m_startedComposite)
            didComposite();

        didCompleteCurrentRecord(TimelineRecordType::RenderingFrame);
    });

    m_frameStartObserver->schedule(currentRunLoop(), kCFRunLoopEntry | kCFRunLoopAfterWaiting);
    m_frameStopObserver->schedule(currentRunLoop(), kCFRunLoopExit | kCFRunLoopBeforeWaiting);

    // Create a runloop record and increment the runloop nesting level, to capture the current turn of the main runloop
    // (which is the outer runloop if recording started while paused in the debugger).
    pushCurrentRecord(InspectorObject::create(), TimelineRecordType::RenderingFrame, false, nullptr);

    m_runLoopNestingLevel = 1;
#endif

    if (m_frontendDispatcher)
        m_frontendDispatcher->recordingStarted();
}

void InspectorTimelineAgent::internalStop()
{
    if (!m_enabled)
        return;

    m_instrumentingAgents->setInspectorTimelineAgent(nullptr);

    if (m_scriptDebugServer)
        m_scriptDebugServer->removeListener(this, true);

#if PLATFORM(COCOA)
    m_frameStartObserver = nullptr;
    m_frameStopObserver = nullptr;
    m_runLoopNestingLevel = 0;

    // Complete all pending records to prevent discarding events that are currently in progress.
    while (!m_recordStack.isEmpty())
        didCompleteCurrentRecord(m_recordStack.last().type);
#endif

    clearRecordStack();

    m_enabled = false;
    m_startedComposite = false;

    if (m_frontendDispatcher)
        m_frontendDispatcher->recordingStopped();
}

double InspectorTimelineAgent::timestamp()
{
    return m_instrumentingAgents->inspectorEnvironment().executionStopwatch()->elapsedTime();
}

void InspectorTimelineAgent::setPageScriptDebugServer(PageScriptDebugServer* scriptDebugServer)
{
    ASSERT(!m_enabled);
    ASSERT(!m_scriptDebugServer);

    m_scriptDebugServer = scriptDebugServer;
}

static inline void startProfiling(JSC::ExecState* exec, const String& title, PassRefPtr<Stopwatch> stopwatch)
{
    JSC::LegacyProfiler::profiler()->startProfiling(exec, title, stopwatch);
}

static inline PassRefPtr<JSC::Profile> stopProfiling(JSC::ExecState* exec, const String& title)
{
    return JSC::LegacyProfiler::profiler()->stopProfiling(exec, title);
}

static inline void startProfiling(Frame* frame, const String& title, PassRefPtr<Stopwatch> stopwatch)
{
    startProfiling(toJSDOMWindow(frame, debuggerWorld())->globalExec(), title, stopwatch);
}

static inline PassRefPtr<JSC::Profile> stopProfiling(Frame* frame, const String& title)
{
    return stopProfiling(toJSDOMWindow(frame, debuggerWorld())->globalExec(), title);
}

void InspectorTimelineAgent::startFromConsole(JSC::ExecState* exec, const String &title)
{
    // Only allow recording of a profile if it is anonymous (empty title) or does not match
    // the title of an already recording profile.
    if (!title.isEmpty()) {
        for (const TimelineRecordEntry& record : m_pendingConsoleProfileRecords) {
            String recordTitle;
            record.data->getString(ASCIILiteral("title"), recordTitle);
            if (recordTitle == title)
                return;
        }
    }

    if (!m_enabled && m_pendingConsoleProfileRecords.isEmpty())
        internalStart();

    startProfiling(exec, title, m_instrumentingAgents->inspectorEnvironment().executionStopwatch());

    m_pendingConsoleProfileRecords.append(createRecordEntry(TimelineRecordFactory::createConsoleProfileData(title), TimelineRecordType::ConsoleProfile, true, frameFromExecState(exec)));
}

PassRefPtr<JSC::Profile> InspectorTimelineAgent::stopFromConsole(JSC::ExecState* exec, const String& title)
{
    // Stop profiles in reverse order. If the title is empty, then stop the last profile.
    // Otherwise, match the title of the profile to stop.
    for (ptrdiff_t i = m_pendingConsoleProfileRecords.size() - 1; i >= 0; --i) {
        const TimelineRecordEntry& record = m_pendingConsoleProfileRecords[i];

        String recordTitle;
        record.data->getString(ASCIILiteral("title"), recordTitle);

        if (title.isEmpty() || recordTitle == title) {
            RefPtr<JSC::Profile> profile = stopProfiling(exec, title);
            if (profile)
                TimelineRecordFactory::appendProfile(record.data.get(), profile.copyRef());

            didCompleteRecordEntry(record);

            m_pendingConsoleProfileRecords.remove(i);

            if (!m_enabledFromFrontend && m_pendingConsoleProfileRecords.isEmpty())
                internalStop();

            return WTF::move(profile);
        }
    }

    return nullptr;
}

void InspectorTimelineAgent::willCallFunction(const String& scriptName, int scriptLine, Frame* frame)
{
    pushCurrentRecord(TimelineRecordFactory::createFunctionCallData(scriptName, scriptLine), TimelineRecordType::FunctionCall, true, frame);

    if (frame && !m_callStackDepth)
        startProfiling(frame, ASCIILiteral("Timeline FunctionCall"), m_instrumentingAgents->inspectorEnvironment().executionStopwatch());

    ++m_callStackDepth;
}

void InspectorTimelineAgent::didCallFunction(Frame* frame)
{
    if (frame && m_callStackDepth) {
        --m_callStackDepth;
        ASSERT(m_callStackDepth >= 0);

        if (!m_callStackDepth) {
            if (m_recordStack.isEmpty())
                return;

            TimelineRecordEntry& entry = m_recordStack.last();
            ASSERT(entry.type == TimelineRecordType::FunctionCall);

            RefPtr<JSC::Profile> profile = stopProfiling(frame, ASCIILiteral("Timeline FunctionCall"));
            if (profile)
                TimelineRecordFactory::appendProfile(entry.data.get(), profile.release());
        }
    }

    didCompleteCurrentRecord(TimelineRecordType::FunctionCall);
}

void InspectorTimelineAgent::willDispatchEvent(const Event& event, Frame* frame)
{
    pushCurrentRecord(TimelineRecordFactory::createEventDispatchData(event), TimelineRecordType::EventDispatch, false, frame);
}

void InspectorTimelineAgent::didDispatchEvent()
{
    didCompleteCurrentRecord(TimelineRecordType::EventDispatch);
}

void InspectorTimelineAgent::didInvalidateLayout(Frame& frame)
{
    appendRecord(InspectorObject::create(), TimelineRecordType::InvalidateLayout, true, &frame);
}

void InspectorTimelineAgent::willLayout(Frame& frame)
{
    RenderObject* root = frame.view()->layoutRoot();
    bool partialLayout = !!root;

    if (!partialLayout)
        root = frame.contentRenderer();

    unsigned dirtyObjects = 0;
    unsigned totalObjects = 0;
    for (RenderObject* o = root; o; o = o->nextInPreOrder(root)) {
        ++totalObjects;
        if (o->needsLayout())
            ++dirtyObjects;
    }
    pushCurrentRecord(TimelineRecordFactory::createLayoutData(dirtyObjects, totalObjects, partialLayout), TimelineRecordType::Layout, true, &frame);
}

void InspectorTimelineAgent::didLayout(RenderObject* root)
{
    if (m_recordStack.isEmpty())
        return;
    TimelineRecordEntry& entry = m_recordStack.last();
    ASSERT(entry.type == TimelineRecordType::Layout);
    Vector<FloatQuad> quads;
    root->absoluteQuads(quads);
    if (quads.size() >= 1)
        TimelineRecordFactory::appendLayoutRoot(entry.data.get(), quads[0]);
    else
        ASSERT_NOT_REACHED();
    didCompleteCurrentRecord(TimelineRecordType::Layout);
}

void InspectorTimelineAgent::didScheduleStyleRecalculation(Frame* frame)
{
    appendRecord(InspectorObject::create(), TimelineRecordType::ScheduleStyleRecalculation, true, frame);
}

void InspectorTimelineAgent::willRecalculateStyle(Frame* frame)
{
    pushCurrentRecord(InspectorObject::create(), TimelineRecordType::RecalculateStyles, true, frame);
}

void InspectorTimelineAgent::didRecalculateStyle()
{
    didCompleteCurrentRecord(TimelineRecordType::RecalculateStyles);
}

void InspectorTimelineAgent::willComposite(Frame& frame)
{
    ASSERT(!m_startedComposite);
    pushCurrentRecord(InspectorObject::create(), TimelineRecordType::Composite, true, &frame);
    m_startedComposite = true;
}

void InspectorTimelineAgent::didComposite()
{
    ASSERT(m_startedComposite);
    didCompleteCurrentRecord(TimelineRecordType::Composite);
    m_startedComposite = false;
}

void InspectorTimelineAgent::willPaint(Frame& frame)
{
    pushCurrentRecord(InspectorObject::create(), TimelineRecordType::Paint, true, &frame);
}

void InspectorTimelineAgent::didPaint(RenderObject* renderer, const LayoutRect& clipRect)
{
    TimelineRecordEntry& entry = m_recordStack.last();
    ASSERT(entry.type == TimelineRecordType::Paint);
    FloatQuad quad;
    localToPageQuad(*renderer, clipRect, &quad);
    entry.data = TimelineRecordFactory::createPaintData(quad);
    didCompleteCurrentRecord(TimelineRecordType::Paint);
}

void InspectorTimelineAgent::willScroll(Frame& frame)
{
    pushCurrentRecord(InspectorObject::create(), TimelineRecordType::ScrollLayer, false, &frame);
}

void InspectorTimelineAgent::didScroll()
{
    didCompleteCurrentRecord(TimelineRecordType::ScrollLayer);
}

void InspectorTimelineAgent::willWriteHTML(unsigned startLine, Frame* frame)
{
    pushCurrentRecord(TimelineRecordFactory::createParseHTMLData(startLine), TimelineRecordType::ParseHTML, true, frame);
}

void InspectorTimelineAgent::didWriteHTML(unsigned endLine)
{
    if (!m_recordStack.isEmpty()) {
        const TimelineRecordEntry& entry = m_recordStack.last();
        entry.data->setInteger("endLine", endLine);
        didCompleteCurrentRecord(TimelineRecordType::ParseHTML);
    }
}

void InspectorTimelineAgent::didInstallTimer(int timerId, int timeout, bool singleShot, Frame* frame)
{
    appendRecord(TimelineRecordFactory::createTimerInstallData(timerId, timeout, singleShot), TimelineRecordType::TimerInstall, true, frame);
}

void InspectorTimelineAgent::didRemoveTimer(int timerId, Frame* frame)
{
    appendRecord(TimelineRecordFactory::createGenericTimerData(timerId), TimelineRecordType::TimerRemove, true, frame);
}

void InspectorTimelineAgent::willFireTimer(int timerId, Frame* frame)
{
    pushCurrentRecord(TimelineRecordFactory::createGenericTimerData(timerId), TimelineRecordType::TimerFire, false, frame);
}

void InspectorTimelineAgent::didFireTimer()
{
    didCompleteCurrentRecord(TimelineRecordType::TimerFire);
}

void InspectorTimelineAgent::willDispatchXHRReadyStateChangeEvent(const String& url, int readyState, Frame* frame)
{
    pushCurrentRecord(TimelineRecordFactory::createXHRReadyStateChangeData(url, readyState), TimelineRecordType::XHRReadyStateChange, false, frame);
}

void InspectorTimelineAgent::didDispatchXHRReadyStateChangeEvent()
{
    didCompleteCurrentRecord(TimelineRecordType::XHRReadyStateChange);
}

void InspectorTimelineAgent::willDispatchXHRLoadEvent(const String& url, Frame* frame)
{
    pushCurrentRecord(TimelineRecordFactory::createXHRLoadData(url), TimelineRecordType::XHRLoad, true, frame);
}

void InspectorTimelineAgent::didDispatchXHRLoadEvent()
{
    didCompleteCurrentRecord(TimelineRecordType::XHRLoad);
}

void InspectorTimelineAgent::willEvaluateScript(const String& url, int lineNumber, Frame& frame)
{
    pushCurrentRecord(TimelineRecordFactory::createEvaluateScriptData(url, lineNumber), TimelineRecordType::EvaluateScript, true, &frame);

    if (!m_callStackDepth) {
        ++m_callStackDepth;
        startProfiling(&frame, ASCIILiteral("Timeline EvaluateScript"), m_instrumentingAgents->inspectorEnvironment().executionStopwatch());
    }
}

void InspectorTimelineAgent::didEvaluateScript(Frame& frame)
{
    if (m_callStackDepth) {
        --m_callStackDepth;
        ASSERT(m_callStackDepth >= 0);

        if (!m_callStackDepth) {
            if (m_recordStack.isEmpty())
                return;

            TimelineRecordEntry& entry = m_recordStack.last();
            ASSERT(entry.type == TimelineRecordType::EvaluateScript);

            RefPtr<JSC::Profile> profile = stopProfiling(&frame, ASCIILiteral("Timeline EvaluateScript"));
            if (profile)
                TimelineRecordFactory::appendProfile(entry.data.get(), profile.release());
        }
    }

    didCompleteCurrentRecord(TimelineRecordType::EvaluateScript);
}

void InspectorTimelineAgent::didTimeStamp(Frame& frame, const String& message)
{
    appendRecord(TimelineRecordFactory::createTimeStampData(message), TimelineRecordType::TimeStamp, true, &frame);
}

void InspectorTimelineAgent::time(Frame& frame, const String& message)
{
    appendRecord(TimelineRecordFactory::createTimeStampData(message), TimelineRecordType::Time, true, &frame);
}

void InspectorTimelineAgent::timeEnd(Frame& frame, const String& message)
{
    appendRecord(TimelineRecordFactory::createTimeStampData(message), TimelineRecordType::TimeEnd, true, &frame);
}

void InspectorTimelineAgent::didMarkDOMContentEvent(Frame& frame)
{
    appendRecord(TimelineRecordFactory::createMarkData(frame.isMainFrame()), TimelineRecordType::MarkDOMContent, false, &frame);
}

void InspectorTimelineAgent::didMarkLoadEvent(Frame& frame)
{
    appendRecord(TimelineRecordFactory::createMarkData(frame.isMainFrame()), TimelineRecordType::MarkLoad, false, &frame);
}

void InspectorTimelineAgent::didCommitLoad()
{
    clearRecordStack();
}

void InspectorTimelineAgent::didRequestAnimationFrame(int callbackId, Frame* frame)
{
    appendRecord(TimelineRecordFactory::createAnimationFrameData(callbackId), TimelineRecordType::RequestAnimationFrame, true, frame);
}

void InspectorTimelineAgent::didCancelAnimationFrame(int callbackId, Frame* frame)
{
    appendRecord(TimelineRecordFactory::createAnimationFrameData(callbackId), TimelineRecordType::CancelAnimationFrame, true, frame);
}

void InspectorTimelineAgent::willFireAnimationFrame(int callbackId, Frame* frame)
{
    pushCurrentRecord(TimelineRecordFactory::createAnimationFrameData(callbackId), TimelineRecordType::FireAnimationFrame, false, frame);
}

void InspectorTimelineAgent::didFireAnimationFrame()
{
    didCompleteCurrentRecord(TimelineRecordType::FireAnimationFrame);
}

#if ENABLE(WEB_SOCKETS)
void InspectorTimelineAgent::didCreateWebSocket(unsigned long identifier, const URL& url, const String& protocol, Frame* frame)
{
    appendRecord(TimelineRecordFactory::createWebSocketCreateData(identifier, url, protocol), TimelineRecordType::WebSocketCreate, true, frame);
}

void InspectorTimelineAgent::willSendWebSocketHandshakeRequest(unsigned long identifier, Frame* frame)
{
    appendRecord(TimelineRecordFactory::createGenericWebSocketData(identifier), TimelineRecordType::WebSocketSendHandshakeRequest, true, frame);
}

void InspectorTimelineAgent::didReceiveWebSocketHandshakeResponse(unsigned long identifier, Frame* frame)
{
    appendRecord(TimelineRecordFactory::createGenericWebSocketData(identifier), TimelineRecordType::WebSocketReceiveHandshakeResponse, false, frame);
}

void InspectorTimelineAgent::didDestroyWebSocket(unsigned long identifier, Frame* frame)
{
    appendRecord(TimelineRecordFactory::createGenericWebSocketData(identifier), TimelineRecordType::WebSocketDestroy, true, frame);
}
#endif // ENABLE(WEB_SOCKETS)

// ScriptDebugListener

void InspectorTimelineAgent::breakpointActionProbe(JSC::ExecState* exec, const Inspector::ScriptBreakpointAction& action, unsigned batchId, unsigned sampleId, const Deprecated::ScriptValue&)
{
    UNUSED_PARAM(batchId);
    ASSERT(exec);

    appendRecord(TimelineRecordFactory::createProbeSampleData(action, sampleId), TimelineRecordType::ProbeSample, false, frameFromExecState(exec));
}

static Inspector::Protocol::Timeline::EventType toProtocol(TimelineRecordType type)
{
    switch (type) {
    case TimelineRecordType::EventDispatch:
        return Inspector::Protocol::Timeline::EventType::EventDispatch;
    case TimelineRecordType::ScheduleStyleRecalculation:
        return Inspector::Protocol::Timeline::EventType::ScheduleStyleRecalculation;
    case TimelineRecordType::RecalculateStyles:
        return Inspector::Protocol::Timeline::EventType::RecalculateStyles;
    case TimelineRecordType::InvalidateLayout:
        return Inspector::Protocol::Timeline::EventType::InvalidateLayout;
    case TimelineRecordType::Layout:
        return Inspector::Protocol::Timeline::EventType::Layout;
    case TimelineRecordType::Paint:
        return Inspector::Protocol::Timeline::EventType::Paint;
    case TimelineRecordType::Composite:
        return Inspector::Protocol::Timeline::EventType::Composite;
    case TimelineRecordType::RenderingFrame:
        return Inspector::Protocol::Timeline::EventType::RenderingFrame;
    case TimelineRecordType::ScrollLayer:
        return Inspector::Protocol::Timeline::EventType::ScrollLayer;

    case TimelineRecordType::ParseHTML:
        return Inspector::Protocol::Timeline::EventType::ParseHTML;

    case TimelineRecordType::TimerInstall:
        return Inspector::Protocol::Timeline::EventType::TimerInstall;
    case TimelineRecordType::TimerRemove:
        return Inspector::Protocol::Timeline::EventType::TimerRemove;
    case TimelineRecordType::TimerFire:
        return Inspector::Protocol::Timeline::EventType::TimerFire;

    case TimelineRecordType::EvaluateScript:
        return Inspector::Protocol::Timeline::EventType::EvaluateScript;

    case TimelineRecordType::MarkLoad:
        return Inspector::Protocol::Timeline::EventType::MarkLoad;
    case TimelineRecordType::MarkDOMContent:
        return Inspector::Protocol::Timeline::EventType::MarkDOMContent;

    case TimelineRecordType::TimeStamp:
        return Inspector::Protocol::Timeline::EventType::TimeStamp;
    case TimelineRecordType::Time:
        return Inspector::Protocol::Timeline::EventType::Time;
    case TimelineRecordType::TimeEnd:
        return Inspector::Protocol::Timeline::EventType::TimeEnd;

    case TimelineRecordType::XHRReadyStateChange:
        return Inspector::Protocol::Timeline::EventType::XHRReadyStateChange;
    case TimelineRecordType::XHRLoad:
        return Inspector::Protocol::Timeline::EventType::XHRLoad;

    case TimelineRecordType::FunctionCall:
        return Inspector::Protocol::Timeline::EventType::FunctionCall;
    case TimelineRecordType::ProbeSample:
        return Inspector::Protocol::Timeline::EventType::ProbeSample;
    case TimelineRecordType::ConsoleProfile:
        return Inspector::Protocol::Timeline::EventType::ConsoleProfile;

    case TimelineRecordType::RequestAnimationFrame:
        return Inspector::Protocol::Timeline::EventType::RequestAnimationFrame;
    case TimelineRecordType::CancelAnimationFrame:
        return Inspector::Protocol::Timeline::EventType::CancelAnimationFrame;
    case TimelineRecordType::FireAnimationFrame:
        return Inspector::Protocol::Timeline::EventType::FireAnimationFrame;

    case TimelineRecordType::WebSocketCreate:
        return Inspector::Protocol::Timeline::EventType::WebSocketCreate;
    case TimelineRecordType::WebSocketSendHandshakeRequest:
        return Inspector::Protocol::Timeline::EventType::WebSocketSendHandshakeRequest;
    case TimelineRecordType::WebSocketReceiveHandshakeResponse:
        return Inspector::Protocol::Timeline::EventType::WebSocketReceiveHandshakeResponse;
    case TimelineRecordType::WebSocketDestroy:
        return Inspector::Protocol::Timeline::EventType::WebSocketDestroy;
    }

    return Inspector::Protocol::Timeline::EventType::TimeStamp;
}

void InspectorTimelineAgent::addRecordToTimeline(RefPtr<InspectorObject>&& record, TimelineRecordType type)
{
    ASSERT_ARG(record, record);
    record->setString("type", Inspector::Protocol::getEnumConstantValue(toProtocol(type)));

    if (m_recordStack.isEmpty()) {
        auto recordObject = BindingTraits<Inspector::Protocol::Timeline::TimelineEvent>::runtimeCast(WTF::move(record));
        sendEvent(WTF::move(recordObject));
    } else {
        const TimelineRecordEntry& parent = m_recordStack.last();
        // Nested paint records are an implementation detail and add no information not already contained in the parent.
        if (type == TimelineRecordType::Paint && parent.type == type)
            return;

        parent.children->pushObject(WTF::move(record));
    }
}

void InspectorTimelineAgent::setFrameIdentifier(InspectorObject* record, Frame* frame)
{
    if (!frame || !m_pageAgent)
        return;
    String frameId;
    if (frame && m_pageAgent)
        frameId = m_pageAgent->frameId(frame);
    record->setString("frameId", frameId);
}

void InspectorTimelineAgent::didCompleteRecordEntry(const TimelineRecordEntry& entry)
{
    entry.record->setObject(ASCIILiteral("data"), entry.data);
    entry.record->setArray(ASCIILiteral("children"), entry.children);
    entry.record->setDouble(ASCIILiteral("endTime"), timestamp());
    addRecordToTimeline(entry.record.copyRef(), entry.type);
}

void InspectorTimelineAgent::didCompleteCurrentRecord(TimelineRecordType type)
{
    // An empty stack could merely mean that the timeline agent was turned on in the middle of
    // an event.  Don't treat as an error.
    if (!m_recordStack.isEmpty()) {
        TimelineRecordEntry entry = m_recordStack.last();
        m_recordStack.removeLast();
        ASSERT_UNUSED(type, entry.type == type);
        didCompleteRecordEntry(entry);
    }
}

InspectorTimelineAgent::InspectorTimelineAgent(InstrumentingAgents* instrumentingAgents, InspectorPageAgent* pageAgent, InspectorType type, InspectorClient* client)
    : InspectorAgentBase(ASCIILiteral("Timeline"), instrumentingAgents)
    , m_pageAgent(pageAgent)
    , m_inspectorType(type)
    , m_client(client)
{
}

void InspectorTimelineAgent::appendRecord(RefPtr<InspectorObject>&& data, TimelineRecordType type, bool captureCallStack, Frame* frame)
{
    Ref<InspectorObject> record = TimelineRecordFactory::createGenericRecord(timestamp(), captureCallStack ? m_maxCallStackDepth : 0);
    record->setObject("data", WTF::move(data));
    setFrameIdentifier(&record.get(), frame);
    addRecordToTimeline(WTF::move(record), type);
}

void InspectorTimelineAgent::sendEvent(RefPtr<InspectorObject>&& event)
{
    if (!m_frontendDispatcher)
        return;

    // FIXME: runtimeCast is a hack. We do it because we can't build TimelineEvent directly now.
    auto recordChecked = BindingTraits<Inspector::Protocol::Timeline::TimelineEvent>::runtimeCast(WTF::move(event));
    m_frontendDispatcher->eventRecorded(WTF::move(recordChecked));
}

InspectorTimelineAgent::TimelineRecordEntry InspectorTimelineAgent::createRecordEntry(RefPtr<InspectorObject>&& data, TimelineRecordType type, bool captureCallStack, Frame* frame)
{
    Ref<InspectorObject> record = TimelineRecordFactory::createGenericRecord(timestamp(), captureCallStack ? m_maxCallStackDepth : 0);
    setFrameIdentifier(&record.get(), frame);
    return TimelineRecordEntry(WTF::move(record), WTF::move(data), InspectorArray::create(), type);
}

void InspectorTimelineAgent::pushCurrentRecord(RefPtr<InspectorObject>&& data, TimelineRecordType type, bool captureCallStack, Frame* frame)
{
    pushCurrentRecord(createRecordEntry(WTF::move(data), type, captureCallStack, frame));
}

void InspectorTimelineAgent::clearRecordStack()
{
    m_recordStack.clear();
    m_id++;
}

void InspectorTimelineAgent::localToPageQuad(const RenderObject& renderer, const LayoutRect& rect, FloatQuad* quad)
{
    const FrameView& frameView = renderer.view().frameView();
    FloatQuad absolute = renderer.localToAbsoluteQuad(FloatQuad(rect));
    quad->setP1(frameView.contentsToRootView(roundedIntPoint(absolute.p1())));
    quad->setP2(frameView.contentsToRootView(roundedIntPoint(absolute.p2())));
    quad->setP3(frameView.contentsToRootView(roundedIntPoint(absolute.p3())));
    quad->setP4(frameView.contentsToRootView(roundedIntPoint(absolute.p4())));
}

Page* InspectorTimelineAgent::page()
{
    return m_pageAgent ? m_pageAgent->page() : nullptr;
}

} // namespace WebCore
