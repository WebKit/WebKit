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

#if ENABLE(INSPECTOR)

#include "InspectorTimelineAgent.h"

#include "Event.h"
#include "Frame.h"
#include "FrameView.h"
#include "InspectorClient.h"
#include "InspectorInstrumentation.h"
#include "InspectorPageAgent.h"
#include "InspectorWebFrontendDispatchers.h"
#include "InstrumentingAgents.h"
#include "IntRect.h"
#include "JSDOMWindow.h"
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

using namespace Inspector;

namespace WebCore {

void TimelineTimeConverter::reset()
{
    m_startOffset = monotonicallyIncreasingTime() - currentTime();
}

InspectorTimelineAgent::~InspectorTimelineAgent()
{
}

void InspectorTimelineAgent::didCreateFrontendAndBackend(Inspector::InspectorFrontendChannel* frontendChannel, InspectorBackendDispatcher* backendDispatcher)
{
    m_frontendDispatcher = std::make_unique<InspectorTimelineFrontendDispatcher>(frontendChannel);
    m_backendDispatcher = InspectorTimelineBackendDispatcher::create(backendDispatcher, this);

    m_instrumentingAgents->setPersistentInspectorTimelineAgent(this);

    if (m_scriptDebugServer)
        m_scriptDebugServer->recompileAllJSFunctions();
}

void InspectorTimelineAgent::willDestroyFrontendAndBackend(InspectorDisconnectReason reason)
{
    m_frontendDispatcher = nullptr;
    m_backendDispatcher.clear();

    m_instrumentingAgents->setPersistentInspectorTimelineAgent(nullptr);

    if (reason != InspectorDisconnectReason::InspectedTargetDestroyed) {
        if (m_scriptDebugServer)
            m_scriptDebugServer->recompileAllJSFunctions();
    }

    ErrorString error;
    stop(&error);
}

void InspectorTimelineAgent::start(ErrorString*, const int* maxCallStackDepth)
{
    m_enabledFromFrontend = true;

    internalStart(maxCallStackDepth);
}

void InspectorTimelineAgent::stop(ErrorString*)
{
    internalStop();

    m_enabledFromFrontend = false;
}

void InspectorTimelineAgent::internalStart(const int* maxCallStackDepth)
{
    if (maxCallStackDepth && *maxCallStackDepth > 0)
        m_maxCallStackDepth = *maxCallStackDepth;
    else
        m_maxCallStackDepth = 5;

    m_timeConverter.reset();

    m_instrumentingAgents->setInspectorTimelineAgent(this);

    if (m_scriptDebugServer)
        m_scriptDebugServer->addListener(this);

    m_enabled = true;

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

    clearRecordStack();

    m_enabled = false;

    if (m_frontendDispatcher)
        m_frontendDispatcher->recordingStopped();
}

void InspectorTimelineAgent::setPageScriptDebugServer(PageScriptDebugServer* scriptDebugServer)
{
    ASSERT(!m_enabled);
    ASSERT(!m_scriptDebugServer);

    m_scriptDebugServer = scriptDebugServer;
}

static inline void startProfiling(JSC::ExecState* exec, const String& title)
{
    JSC::LegacyProfiler::profiler()->startProfiling(exec, title);
}

static inline PassRefPtr<JSC::Profile> stopProfiling(JSC::ExecState* exec, const String& title)
{
    return JSC::LegacyProfiler::profiler()->stopProfiling(exec, title);
}

static inline void startProfiling(Frame* frame, const String& title)
{
    startProfiling(toJSDOMWindow(frame, debuggerWorld())->globalExec(), title);
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
            record.data->getString(ASCIILiteral("title"), &recordTitle);
            if (recordTitle == title)
                return;
        }
    }

    if (!m_enabled && m_pendingConsoleProfileRecords.isEmpty())
        internalStart();

    startProfiling(exec, title);

    m_pendingConsoleProfileRecords.append(createRecordEntry(TimelineRecordFactory::createConsoleProfileData(title), TimelineRecordType::ConsoleProfile, true, frameFromExecState(exec)));
}

PassRefPtr<JSC::Profile> InspectorTimelineAgent::stopFromConsole(JSC::ExecState* exec, const String& title)
{
    // Stop profiles in reverse order. If the title is empty, then stop the last profile.
    // Otherwise, match the title of the profile to stop.
    for (ptrdiff_t i = m_pendingConsoleProfileRecords.size() - 1; i >= 0; --i) {
        const TimelineRecordEntry& record = m_pendingConsoleProfileRecords[i];

        String recordTitle;
        record.data->getString(ASCIILiteral("title"), &recordTitle);

        if (title.isEmpty() || recordTitle == title) {
            RefPtr<JSC::Profile> profile = stopProfiling(exec, title);
            if (profile)
                TimelineRecordFactory::appendProfile(record.data.get(), profile);

            didCompleteRecordEntry(record);

            m_pendingConsoleProfileRecords.remove(i);

            if (!m_enabledFromFrontend && m_pendingConsoleProfileRecords.isEmpty())
                internalStop();

            return profile.release();
        }
    }

    return nullptr;
}

void InspectorTimelineAgent::willCallFunction(const String& scriptName, int scriptLine, Frame* frame)
{
    pushCurrentRecord(TimelineRecordFactory::createFunctionCallData(scriptName, scriptLine), TimelineRecordType::FunctionCall, true, frame);

    if (frame && !m_recordingProfileDepth) {
        ++m_recordingProfileDepth;
        startProfiling(frame, ASCIILiteral("Timeline FunctionCall"));
    }
}

void InspectorTimelineAgent::didCallFunction(Frame* frame)
{
    if (frame && m_recordingProfileDepth) {
        --m_recordingProfileDepth;
        ASSERT(m_recordingProfileDepth >= 0);

        if (!m_recordingProfileDepth) {
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

void InspectorTimelineAgent::didInvalidateLayout(Frame* frame)
{
    appendRecord(InspectorObject::create(), TimelineRecordType::InvalidateLayout, true, frame);
}

void InspectorTimelineAgent::willLayout(Frame* frame)
{
    RenderObject* root = frame->view()->layoutRoot();
    bool partialLayout = !!root;

    if (!partialLayout)
        root = frame->contentRenderer();

    unsigned dirtyObjects = 0;
    unsigned totalObjects = 0;
    for (RenderObject* o = root; o; o = o->nextInPreOrder(root)) {
        ++totalObjects;
        if (o->needsLayout())
            ++dirtyObjects;
    }
    pushCurrentRecord(TimelineRecordFactory::createLayoutData(dirtyObjects, totalObjects, partialLayout), TimelineRecordType::Layout, true, frame);
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

void InspectorTimelineAgent::willPaint(Frame* frame)
{
    pushCurrentRecord(InspectorObject::create(), TimelineRecordType::Paint, true, frame);
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

void InspectorTimelineAgent::willScroll(Frame* frame)
{
    pushCurrentRecord(InspectorObject::create(), TimelineRecordType::ScrollLayer, false, frame);
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
        entry.data->setNumber("endLine", endLine);
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

void InspectorTimelineAgent::willEvaluateScript(const String& url, int lineNumber, Frame* frame)
{
    pushCurrentRecord(TimelineRecordFactory::createEvaluateScriptData(url, lineNumber), TimelineRecordType::EvaluateScript, true, frame);

    if (frame && !m_recordingProfileDepth) {
        ++m_recordingProfileDepth;
        startProfiling(frame, ASCIILiteral("Timeline EvaluateScript"));
    }
}

void InspectorTimelineAgent::didEvaluateScript(Frame* frame)
{
    if (frame && m_recordingProfileDepth) {
        --m_recordingProfileDepth;
        ASSERT(m_recordingProfileDepth >= 0);
        
        if (!m_recordingProfileDepth) {
            if (m_recordStack.isEmpty())
                return;

            TimelineRecordEntry& entry = m_recordStack.last();
            ASSERT(entry.type == TimelineRecordType::EvaluateScript);

            RefPtr<JSC::Profile> profile = stopProfiling(frame, ASCIILiteral("Timeline EvaluateScript"));
            if (profile)
                TimelineRecordFactory::appendProfile(entry.data.get(), profile.release());
        }
    }

    didCompleteCurrentRecord(TimelineRecordType::EvaluateScript);
}

void InspectorTimelineAgent::didScheduleResourceRequest(const String& url, Frame* frame)
{
    appendRecord(TimelineRecordFactory::createScheduleResourceRequestData(url), TimelineRecordType::ScheduleResourceRequest, true, frame);
}

void InspectorTimelineAgent::willSendResourceRequest(unsigned long identifier, const ResourceRequest& request, Frame* frame)
{
    String requestId = IdentifiersFactory::requestId(identifier);
    appendRecord(TimelineRecordFactory::createResourceSendRequestData(requestId, request), TimelineRecordType::ResourceSendRequest, true, frame);
}

void InspectorTimelineAgent::willReceiveResourceData(unsigned long identifier, Frame* frame, int length)
{
    String requestId = IdentifiersFactory::requestId(identifier);
    pushCurrentRecord(TimelineRecordFactory::createReceiveResourceData(requestId, length), TimelineRecordType::ResourceReceivedData, false, frame);
}

void InspectorTimelineAgent::didReceiveResourceData()
{
    didCompleteCurrentRecord(TimelineRecordType::ResourceReceivedData);
}

void InspectorTimelineAgent::willReceiveResourceResponse(unsigned long identifier, const ResourceResponse& response, Frame* frame)
{
    String requestId = IdentifiersFactory::requestId(identifier);
    pushCurrentRecord(TimelineRecordFactory::createResourceReceiveResponseData(requestId, response), TimelineRecordType::ResourceReceiveResponse, false, frame);
}

void InspectorTimelineAgent::didReceiveResourceResponse()
{
    didCompleteCurrentRecord(TimelineRecordType::ResourceReceiveResponse);
}

void InspectorTimelineAgent::didFinishLoadingResource(unsigned long identifier, bool didFail, double finishTime, Frame* frame)
{
    appendRecord(TimelineRecordFactory::createResourceFinishData(IdentifiersFactory::requestId(identifier), didFail, finishTime * 1000), TimelineRecordType::ResourceFinish, false, frame);
}

void InspectorTimelineAgent::didTimeStamp(Frame* frame, const String& message)
{
    appendRecord(TimelineRecordFactory::createTimeStampData(message), TimelineRecordType::TimeStamp, true, frame);
}

void InspectorTimelineAgent::time(Frame* frame, const String& message)
{
    appendRecord(TimelineRecordFactory::createTimeStampData(message), TimelineRecordType::Time, true, frame);
}

void InspectorTimelineAgent::timeEnd(Frame* frame, const String& message)
{
    appendRecord(TimelineRecordFactory::createTimeStampData(message), TimelineRecordType::TimeEnd, true, frame);
}

void InspectorTimelineAgent::didMarkDOMContentEvent(Frame* frame)
{
    bool isMainFrame = frame && m_pageAgent && (frame == m_pageAgent->mainFrame());
    appendRecord(TimelineRecordFactory::createMarkData(isMainFrame), TimelineRecordType::MarkDOMContent, false, frame);
}

void InspectorTimelineAgent::didMarkLoadEvent(Frame* frame)
{
    bool isMainFrame = frame && m_pageAgent && (frame == m_pageAgent->mainFrame());
    appendRecord(TimelineRecordFactory::createMarkData(isMainFrame), TimelineRecordType::MarkLoad, false, frame);
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

void InspectorTimelineAgent::breakpointActionProbe(JSC::ExecState* exec, const Inspector::ScriptBreakpointAction& action, int hitCount, const Deprecated::ScriptValue&)
{
    ASSERT(exec);

    appendRecord(TimelineRecordFactory::createProbeSampleData(action, hitCount), TimelineRecordType::ProbeSample, false, frameFromExecState(exec));
}

static Inspector::TypeBuilder::Timeline::EventType::Enum toProtocol(TimelineRecordType type)
{
    switch (type) {
    case TimelineRecordType::EventDispatch:
        return Inspector::TypeBuilder::Timeline::EventType::EventDispatch;
    case TimelineRecordType::ScheduleStyleRecalculation:
        return Inspector::TypeBuilder::Timeline::EventType::ScheduleStyleRecalculation;
    case TimelineRecordType::RecalculateStyles:
        return Inspector::TypeBuilder::Timeline::EventType::RecalculateStyles;
    case TimelineRecordType::InvalidateLayout:
        return Inspector::TypeBuilder::Timeline::EventType::InvalidateLayout;
    case TimelineRecordType::Layout:
        return Inspector::TypeBuilder::Timeline::EventType::Layout;
    case TimelineRecordType::Paint:
        return Inspector::TypeBuilder::Timeline::EventType::Paint;
    case TimelineRecordType::ScrollLayer:
        return Inspector::TypeBuilder::Timeline::EventType::ScrollLayer;
    case TimelineRecordType::ResizeImage:
        return Inspector::TypeBuilder::Timeline::EventType::ResizeImage;

    case TimelineRecordType::ParseHTML:
        return Inspector::TypeBuilder::Timeline::EventType::ParseHTML;

    case TimelineRecordType::TimerInstall:
        return Inspector::TypeBuilder::Timeline::EventType::TimerInstall;
    case TimelineRecordType::TimerRemove:
        return Inspector::TypeBuilder::Timeline::EventType::TimerRemove;
    case TimelineRecordType::TimerFire:
        return Inspector::TypeBuilder::Timeline::EventType::TimerFire;

    case TimelineRecordType::EvaluateScript:
        return Inspector::TypeBuilder::Timeline::EventType::EvaluateScript;

    case TimelineRecordType::MarkLoad:
        return Inspector::TypeBuilder::Timeline::EventType::MarkLoad;
    case TimelineRecordType::MarkDOMContent:
        return Inspector::TypeBuilder::Timeline::EventType::MarkDOMContent;

    case TimelineRecordType::TimeStamp:
        return Inspector::TypeBuilder::Timeline::EventType::TimeStamp;
    case TimelineRecordType::Time:
        return Inspector::TypeBuilder::Timeline::EventType::Time;
    case TimelineRecordType::TimeEnd:
        return Inspector::TypeBuilder::Timeline::EventType::TimeEnd;

    case TimelineRecordType::ScheduleResourceRequest:
        return Inspector::TypeBuilder::Timeline::EventType::ScheduleResourceRequest;
    case TimelineRecordType::ResourceSendRequest:
        return Inspector::TypeBuilder::Timeline::EventType::ResourceSendRequest;
    case TimelineRecordType::ResourceReceiveResponse:
        return Inspector::TypeBuilder::Timeline::EventType::ResourceReceiveResponse;
    case TimelineRecordType::ResourceReceivedData:
        return Inspector::TypeBuilder::Timeline::EventType::ResourceReceivedData;
    case TimelineRecordType::ResourceFinish:
        return Inspector::TypeBuilder::Timeline::EventType::ResourceFinish;

    case TimelineRecordType::XHRReadyStateChange:
        return Inspector::TypeBuilder::Timeline::EventType::XHRReadyStateChange;
    case TimelineRecordType::XHRLoad:
        return Inspector::TypeBuilder::Timeline::EventType::XHRLoad;

    case TimelineRecordType::FunctionCall:
        return Inspector::TypeBuilder::Timeline::EventType::FunctionCall;
    case TimelineRecordType::ProbeSample:
        return Inspector::TypeBuilder::Timeline::EventType::ProbeSample;
    case TimelineRecordType::ConsoleProfile:
        return Inspector::TypeBuilder::Timeline::EventType::ConsoleProfile;

    case TimelineRecordType::RequestAnimationFrame:
        return Inspector::TypeBuilder::Timeline::EventType::RequestAnimationFrame;
    case TimelineRecordType::CancelAnimationFrame:
        return Inspector::TypeBuilder::Timeline::EventType::CancelAnimationFrame;
    case TimelineRecordType::FireAnimationFrame:
        return Inspector::TypeBuilder::Timeline::EventType::FireAnimationFrame;

    case TimelineRecordType::WebSocketCreate:
        return Inspector::TypeBuilder::Timeline::EventType::WebSocketCreate;
    case TimelineRecordType::WebSocketSendHandshakeRequest:
        return Inspector::TypeBuilder::Timeline::EventType::WebSocketSendHandshakeRequest;
    case TimelineRecordType::WebSocketReceiveHandshakeResponse:
        return Inspector::TypeBuilder::Timeline::EventType::WebSocketReceiveHandshakeResponse;
    case TimelineRecordType::WebSocketDestroy:
        return Inspector::TypeBuilder::Timeline::EventType::WebSocketDestroy;
    }

    return Inspector::TypeBuilder::Timeline::EventType::TimeStamp;
}

void InspectorTimelineAgent::addRecordToTimeline(PassRefPtr<InspectorObject> prpRecord, TimelineRecordType type)
{
    prpRecord->setString("type", Inspector::TypeBuilder::getWebEnumConstantValue(toProtocol(type)));

    RefPtr<Inspector::TypeBuilder::Timeline::TimelineEvent> record = Inspector::TypeBuilder::Timeline::TimelineEvent::runtimeCast(prpRecord);

    if (m_recordStack.isEmpty())
        sendEvent(record.release());
    else {
        const TimelineRecordEntry& parent = m_recordStack.last();
        parent.children->pushObject(record.release());
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
    entry.record->setNumber(ASCIILiteral("endTime"), timestamp());
    addRecordToTimeline(entry.record, entry.type);
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
    , m_scriptDebugServer(nullptr)
    , m_id(1)
    , m_maxCallStackDepth(5)
    , m_inspectorType(type)
    , m_client(client)
    , m_recordingProfileDepth(0)
    , m_enabled(false)
    , m_enabledFromFrontend(false)
{
}

void InspectorTimelineAgent::appendRecord(PassRefPtr<InspectorObject> data, TimelineRecordType type, bool captureCallStack, Frame* frame)
{
    RefPtr<InspectorObject> record = TimelineRecordFactory::createGenericRecord(timestamp(), captureCallStack ? m_maxCallStackDepth : 0);
    record->setObject("data", data);
    setFrameIdentifier(record.get(), frame);
    addRecordToTimeline(record.release(), type);
}

void InspectorTimelineAgent::sendEvent(PassRefPtr<InspectorObject> event)
{
    if (!m_frontendDispatcher)
        return;

    // FIXME: runtimeCast is a hack. We do it because we can't build TimelineEvent directly now.
    RefPtr<Inspector::TypeBuilder::Timeline::TimelineEvent> recordChecked = Inspector::TypeBuilder::Timeline::TimelineEvent::runtimeCast(event);
    m_frontendDispatcher->eventRecorded(recordChecked.release());
}

InspectorTimelineAgent::TimelineRecordEntry InspectorTimelineAgent::createRecordEntry(PassRefPtr<InspectorObject> data, TimelineRecordType type, bool captureCallStack, Frame* frame)
{
    RefPtr<InspectorObject> record = TimelineRecordFactory::createGenericRecord(timestamp(), captureCallStack ? m_maxCallStackDepth : 0);
    setFrameIdentifier(record.get(), frame);
    return TimelineRecordEntry(record.release(), data, InspectorArray::create(), type);
}

void InspectorTimelineAgent::pushCurrentRecord(PassRefPtr<InspectorObject> data, TimelineRecordType type, bool captureCallStack, Frame* frame)
{
    pushCurrentRecord(createRecordEntry(data, type, captureCallStack, frame));
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

double InspectorTimelineAgent::timestamp()
{
    return m_timeConverter.fromMonotonicallyIncreasingTime(monotonicallyIncreasingTime());
}

Page* InspectorTimelineAgent::page()
{
    return m_pageAgent ? m_pageAgent->page() : nullptr;
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR)
