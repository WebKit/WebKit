/*
 * Copyright (C) 2011-2013 University of Washington. All rights reserved.
 * Copyright (C) 2014 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ReplayController.h"

#if ENABLE(WEB_REPLAY)

#include "AllReplayInputs.h"
#include "CapturingInputCursor.h"
#include "DOMWindow.h"
#include "DocumentLoader.h"
#include "Frame.h"
#include "FrameTree.h"
#include "InspectorInstrumentation.h"
#include "Location.h"
#include "Logging.h"
#include "MainFrame.h"
#include "Page.h"
#include "ReplaySession.h"
#include "ReplaySessionSegment.h"
#include "ReplayingInputCursor.h"
#include "ScriptController.h"
#include "SerializationMethods.h"
#include "Settings.h"
#include "UserInputBridge.h"
#include "WebReplayInputs.h"
#include <replay/EmptyInputCursor.h>
#include <wtf/text/CString.h>

#if ENABLE(ASYNC_SCROLLING)
#include "ScrollingCoordinator.h"
#endif

namespace WebCore {

#if !LOG_DISABLED
static void logDispatchedDOMEvent(const Event& event, bool eventIsUnrelated)
{
    EventTarget* target = event.target();
    if (!target)
        return;

    // A DOM event is unrelated if it is being dispatched to a document that is neither capturing nor replaying.
    if (Node* node = target->toNode()) {
        LOG(WebReplay, "%-20s --->%s DOM event: type=%s, target=%u/node[%p] %s\n", "ReplayEvents",
            (eventIsUnrelated) ? "Unrelated" : "Dispatching",
            event.type().string().utf8().data(),
            frameIndexFromDocument((node->isConnected()) ? &node->document() : node->ownerDocument()),
            node,
            node->nodeName().utf8().data());
    } else if (DOMWindow* window = target->toDOMWindow()) {
        LOG(WebReplay, "%-20s --->%s DOM event: type=%s, target=%u/window[%p] %s\n", "ReplayEvents",
            (eventIsUnrelated) ? "Unrelated" : "Dispatching",
            event.type().string().utf8().data(),
            frameIndexFromDocument(window->document()),
            window,
            window->location()->href().utf8().data());
    }
}

static const char* sessionStateToString(SessionState state)
{
    switch (state) {
    case SessionState::Capturing:
        return "Capturing";
    case SessionState::Inactive:
        return "Inactive";
    case SessionState::Replaying:
        return "Replaying";
    }
}

static const char* segmentStateToString(SegmentState state)
{
    switch (state) {
    case SegmentState::Appending:
        return "Appending";
    case SegmentState::Unloaded:
        return "Unloaded";
    case SegmentState::Loaded:
        return "Loaded";
    case SegmentState::Dispatching:
        return "Dispatching";
    }
}

#endif // !LOG_DISABLED

ReplayController::ReplayController(Page& page)
    : m_page(page)
    , m_loadedSession(ReplaySession::create())
    , m_emptyCursor(EmptyInputCursor::create())
    , m_targetPosition(ReplayPosition(0, 0))
    , m_currentPosition(ReplayPosition(0, 0))
    , m_segmentState(SegmentState::Unloaded)
    , m_sessionState(SessionState::Inactive)
    , m_dispatchSpeed(DispatchSpeed::FastForward)
{
}

void ReplayController::setForceDeterministicSettings(bool shouldForceDeterministicBehavior)
{
    ASSERT_ARG(shouldForceDeterministicBehavior, shouldForceDeterministicBehavior ^ (m_sessionState == SessionState::Inactive));

    if (shouldForceDeterministicBehavior) {
        m_savedSettings.usesPageCache = m_page.settings().usesPageCache();

        m_page.settings().setUsesPageCache(false);
    } else {
        m_page.settings().setUsesPageCache(m_savedSettings.usesPageCache);
    }

#if ENABLE(ASYNC_SCROLLING)
    if (ScrollingCoordinator* scrollingCoordinator = m_page.scrollingCoordinator())
        scrollingCoordinator->replaySessionStateDidChange();
#endif
}

void ReplayController::setSessionState(SessionState state)
{
    ASSERT_ARG(state, state != m_sessionState);

    LOG(WebReplay, "%-20s SessionState transition: %10s --> %10s.\n", "ReplayController", sessionStateToString(m_sessionState), sessionStateToString(state));

    switch (m_sessionState) {
    case SessionState::Capturing:
        ASSERT(state == SessionState::Inactive);

        m_sessionState = state;
        m_page.userInputBridge().setState(UserInputBridge::State::Open);
        break;

    case SessionState::Inactive:
        m_sessionState = state;
        m_page.userInputBridge().setState(state == SessionState::Capturing ? UserInputBridge::State::Capturing : UserInputBridge::State::Replaying);
        break;

    case SessionState::Replaying:
        ASSERT(state == SessionState::Inactive);

        m_sessionState = state;
        m_page.userInputBridge().setState(UserInputBridge::State::Open);
        break;
    }
}

void ReplayController::setSegmentState(SegmentState state)
{
    ASSERT_ARG(state, state != m_segmentState);

    LOG(WebReplay, "%-20s SegmentState transition: %10s --> %10s.\n", "ReplayController", segmentStateToString(m_segmentState), segmentStateToString(state));

    switch (m_segmentState) {
    case SegmentState::Appending:
        ASSERT(state == SegmentState::Unloaded);
        break;

    case SegmentState::Unloaded:
        ASSERT(state == SegmentState::Appending || state == SegmentState::Loaded);
        break;

    case SegmentState::Loaded:
        ASSERT(state == SegmentState::Unloaded || state == SegmentState::Dispatching);
        break;

    case SegmentState::Dispatching:
        ASSERT(state == SegmentState::Loaded);
        break;
    }

    m_segmentState = state;
}

void ReplayController::switchSession(RefPtr<ReplaySession>&& session)
{
    ASSERT_ARG(session, session);
    ASSERT(m_segmentState == SegmentState::Unloaded);
    ASSERT(m_sessionState == SessionState::Inactive);

    m_loadedSession = session;
    m_currentPosition = ReplayPosition(0, 0);

    LOG(WebReplay, "%-20sSwitching sessions from %p to %p.\n", "ReplayController", m_loadedSession.get(), session.get());
    InspectorInstrumentation::sessionLoaded(m_page, m_loadedSession.copyRef());
}

void ReplayController::createSegment()
{
    ASSERT(m_sessionState == SessionState::Capturing);
    ASSERT(m_segmentState == SegmentState::Unloaded);

    setSegmentState(SegmentState::Appending);

    // Create a new segment but don't associate it with the current session
    // until we stop appending to it. This preserves the invariant that
    // segments associated with a replay session have immutable data.
    m_loadedSegment = ReplaySessionSegment::create();

    LOG(WebReplay, "%-20s Created segment: %p.\n", "ReplayController", m_loadedSegment.get());
    InspectorInstrumentation::segmentCreated(m_page, m_loadedSegment.copyRef());

    m_activeCursor = CapturingInputCursor::create(m_loadedSegment.copyRef());
    m_activeCursor->appendInput<BeginSegmentSentinel>();

    std::unique_ptr<InitialNavigation> navigationInput = InitialNavigation::createFromPage(m_page);
    // Dispatching this input schedules navigation of the main frame, causing a refresh.
    navigationInput->dispatch(*this);
    m_activeCursor->storeInput(WTFMove(navigationInput));
}

void ReplayController::completeSegment()
{
    ASSERT(m_sessionState == SessionState::Capturing);
    ASSERT(m_segmentState == SegmentState::Appending);

    m_activeCursor->appendInput<EndSegmentSentinel>();

    // Hold on to a reference so unloading the segment doesn't deallocate it.
    RefPtr<ReplaySessionSegment> segment = m_loadedSegment;
    bool shouldSuppressNotifications = true;
    unloadSegment(shouldSuppressNotifications);

    LOG(WebReplay, "%-20s Completed segment: %p.\n", "ReplayController", segment.get());
    InspectorInstrumentation::segmentCompleted(m_page, segment.copyRef());

    m_loadedSession->appendSegment(segment.copyRef());
    InspectorInstrumentation::sessionModified(m_page, m_loadedSession.copyRef());
}

void ReplayController::loadSegmentAtIndex(size_t segmentIndex)
{
    ASSERT_ARG(segmentIndex, segmentIndex >= 0 && segmentIndex < m_loadedSession->size());
    RefPtr<ReplaySessionSegment> segment = m_loadedSession->at(segmentIndex);

    ASSERT(m_sessionState == SessionState::Replaying);
    ASSERT(m_segmentState == SegmentState::Unloaded);
    ASSERT(segment);
    ASSERT(!m_loadedSegment);

    m_loadedSegment = segment;
    setSegmentState(SegmentState::Loaded);

    m_currentPosition.segmentOffset = segmentIndex;
    m_currentPosition.inputOffset = 0;

    m_activeCursor = ReplayingInputCursor::create(m_loadedSegment.copyRef(), m_page, this);

    LOG(WebReplay, "%-20sLoading segment: %p.\n", "ReplayController", segment.get());
    InspectorInstrumentation::segmentLoaded(m_page, segment.copyRef());
}

void ReplayController::unloadSegment(bool suppressNotifications)
{
    ASSERT(m_sessionState != SessionState::Inactive);
    ASSERT(m_segmentState == SegmentState::Loaded || m_segmentState == SegmentState::Appending);

    setSegmentState(SegmentState::Unloaded);

    LOG(WebReplay, "%-20s Clearing input cursors for page: %p\n", "ReplayController", &m_page);

    m_activeCursor = nullptr;
    auto unloadedSegment = WTFMove(m_loadedSegment);
    for (Frame* frame = &m_page.mainFrame(); frame; frame = frame->tree().traverseNext()) {
        frame->script().globalObject(mainThreadNormalWorld())->setInputCursor(m_emptyCursor.copyRef());
        frame->document()->setInputCursor(m_emptyCursor.copyRef());
    }

    // When we stop capturing, don't send out segment unloaded events since we
    // didn't send out the corresponding segmentLoaded event at the start of capture.
    if (!suppressNotifications) {
        LOG(WebReplay, "%-20sUnloading segment: %p.\n", "ReplayController", unloadedSegment.get());
        InspectorInstrumentation::segmentUnloaded(m_page);
    }
}

void ReplayController::startCapturing()
{
    ASSERT(m_sessionState == SessionState::Inactive);
    ASSERT(m_segmentState == SegmentState::Unloaded);

    setSessionState(SessionState::Capturing);
    setForceDeterministicSettings(true);

    LOG(WebReplay, "%-20s Starting capture.\n", "ReplayController");
    InspectorInstrumentation::captureStarted(m_page);

    m_currentPosition = ReplayPosition(0, 0);

    createSegment();
}

void ReplayController::stopCapturing()
{
    ASSERT(m_sessionState == SessionState::Capturing);
    ASSERT(m_segmentState == SegmentState::Appending);

    completeSegment();

    setSessionState(SessionState::Inactive);
    setForceDeterministicSettings(false);

    LOG(WebReplay, "%-20s Stopping capture.\n", "ReplayController");
    InspectorInstrumentation::captureStopped(m_page);
}

void ReplayController::startPlayback()
{
    ASSERT(m_sessionState == SessionState::Replaying);
    ASSERT(m_segmentState == SegmentState::Loaded);

    setSegmentState(SegmentState::Dispatching);

    LOG(WebReplay, "%-20s Starting playback to position (segment: %d, input: %d).\n", "ReplayController", m_targetPosition.segmentOffset, m_targetPosition.inputOffset);
    InspectorInstrumentation::playbackStarted(m_page);

    dispatcher().setDispatchSpeed(m_dispatchSpeed);
    dispatcher().run();
}

void ReplayController::pausePlayback()
{
    ASSERT(m_sessionState == SessionState::Replaying);
    ASSERT(m_segmentState == SegmentState::Dispatching);

    if (dispatcher().isRunning())
        dispatcher().pause();

    setSegmentState(SegmentState::Loaded);

    LOG(WebReplay, "%-20s Pausing playback at position (segment: %d, input: %d).\n", "ReplayController", m_currentPosition.segmentOffset, m_currentPosition.inputOffset);
    InspectorInstrumentation::playbackPaused(m_page, m_currentPosition);
}

void ReplayController::cancelPlayback()
{
    ASSERT(m_sessionState == SessionState::Replaying);
    ASSERT(m_segmentState != SegmentState::Appending);

    if (m_segmentState == SegmentState::Unloaded)
        return;

    if (m_segmentState == SegmentState::Dispatching)
        pausePlayback();

    ASSERT(m_segmentState == SegmentState::Loaded);
    unloadSegment();
    m_sessionState = SessionState::Inactive;
    setForceDeterministicSettings(false);
    InspectorInstrumentation::playbackFinished(m_page);
}

void ReplayController::replayToPosition(const ReplayPosition& position, DispatchSpeed speed)
{
    ASSERT(m_sessionState != SessionState::Capturing);
    ASSERT(m_segmentState == SegmentState::Loaded || m_segmentState == SegmentState::Unloaded);
    ASSERT(position.segmentOffset < m_loadedSession->size());

    m_dispatchSpeed = speed;

    if (m_sessionState != SessionState::Replaying) {
        setSessionState(SessionState::Replaying);
        setForceDeterministicSettings(true);
    }

    if (m_segmentState == SegmentState::Unloaded)
        loadSegmentAtIndex(position.segmentOffset);
    else if (position.segmentOffset != m_currentPosition.segmentOffset || m_currentPosition.inputOffset > position.inputOffset) {
        // If the desired segment is not loaded or we have gone past the desired input
        // offset, then unload the current segment and load the appropriate segment.
        unloadSegment();
        loadSegmentAtIndex(position.segmentOffset);
    }

    ASSERT(m_currentPosition.segmentOffset == position.segmentOffset);
    ASSERT(m_loadedSession->at(position.segmentOffset) == m_loadedSegment);

    m_targetPosition = position;
    startPlayback();
}

void ReplayController::frameNavigated(Frame& frame)
{
    ASSERT(m_sessionState != SessionState::Inactive);
    
    // The initial capturing segment is created prior to main frame navigation.
    // Otherwise, the prior capturing segment was completed when the frame detached,
    // and it is now time to create a new segment.
    if (m_sessionState == SessionState::Capturing && m_segmentState == SegmentState::Unloaded) {
        m_currentPosition = ReplayPosition(m_currentPosition.segmentOffset + 1, 0);
        createSegment();
    }

    // During playback, the next segment is loaded when the final input is dispatched,
    // so nothing needs to be done here.

    // We store the input cursor in both Document and JSDOMWindow, so that
    // replay state is accessible from JavaScriptCore and script-free layout code.
    frame.document()->setInputCursor(*m_activeCursor);
    frame.script().globalObject(mainThreadNormalWorld())->setInputCursor(*m_activeCursor);
}

void ReplayController::frameDetached(Frame& frame)
{
    ASSERT(m_sessionState != SessionState::Inactive);

    if (!frame.document())
        return;

    // If the frame's cursor isn't capturing or replaying, we should do nothing.
    // This is the case for the "outbound" frame when starting capture, or when
    // we clear the input cursor to finish or prematurely unload a segment.
    if (frame.document()->inputCursor().isCapturing()) {
        ASSERT(m_segmentState == SegmentState::Appending);
        completeSegment();
    }

    // During playback, the segments are unloaded and loaded when the final
    // input has been dispatched. So, nothing needs to be done here.
}

void ReplayController::willDispatchEvent(const Event& event, Frame* frame)
{
    EventTarget* target = event.target();
    if (!target && !frame)
        return;

    Document* document = frame ? frame->document() : nullptr;
    // Fetch the document from the event target, because the target could be detached.
    if (Node* node = target->toNode())
        document = node->isConnected() ? &node->document() : node->ownerDocument();
    else if (DOMWindow* window = target->toDOMWindow())
        document = window->document();

    ASSERT(document);
    InputCursor& cursor = document->inputCursor();

#if !LOG_DISABLED
    bool eventIsUnrelated = !cursor.isCapturing() && !cursor.isReplaying();
    logDispatchedDOMEvent(event, eventIsUnrelated);
#else
    UNUSED_PARAM(cursor);
#endif

#if ENABLE_AGGRESSIVE_DETERMINISM_CHECKS
    // To ensure deterministic JS execution, all DOM events must be dispatched deterministically.
    // If these assertions fail, then this DOM event is being dispatched by a nondeterministic EventLoop
    // cycle, and may cause program execution to diverge if any JS code runs because of the DOM event.
    if (cursor.isCapturing() || cursor.isReplaying())
        ASSERT(cursor.withinEventLoopInputExtent());
    else if (cursor.isReplaying())
        ASSERT(dispatcher().isDispatching());
#endif
}

RefPtr<ReplaySession> ReplayController::loadedSession() const
{
    return m_loadedSession.copyRef();
}

RefPtr<ReplaySessionSegment> ReplayController::loadedSegment() const
{
    return m_loadedSegment.copyRef();
}

InputCursor& ReplayController::activeInputCursor()
{
    return m_activeCursor ? *m_activeCursor : m_emptyCursor.get();
}

EventLoopInputDispatcher& ReplayController::dispatcher() const
{
    ASSERT(m_sessionState == SessionState::Replaying);
    ASSERT(m_segmentState == SegmentState::Dispatching);
    ASSERT(m_activeCursor && m_activeCursor->isReplaying());

    return static_cast<ReplayingInputCursor&>(*m_activeCursor).dispatcher();
}

void ReplayController::willDispatchInput(const EventLoopInputBase&)
{
    ASSERT(m_sessionState == SessionState::Replaying);
    ASSERT(m_segmentState == SegmentState::Dispatching);

    m_currentPosition.inputOffset++;

    InspectorInstrumentation::playbackHitPosition(m_page, m_currentPosition);

    if (m_currentPosition == m_targetPosition)
        pausePlayback();
}

void ReplayController::didDispatchInput(const EventLoopInputBase&)
{
    ASSERT(m_sessionState == SessionState::Replaying);
    ASSERT(m_segmentState == SegmentState::Dispatching);
}

void ReplayController::didDispatchFinalInput()
{
    ASSERT(m_segmentState == SegmentState::Dispatching);

    // No more segments left to replay; stop.
    if (m_currentPosition.segmentOffset + 1 == m_loadedSession->size()) {
        // Normally the position is adjusted when loading the next segment.
        m_currentPosition.segmentOffset++;
        m_currentPosition.inputOffset = 0;

        cancelPlayback();
        return;
    }

    unloadSegment();
    loadSegmentAtIndex(m_currentPosition.segmentOffset + 1);
    startPlayback();
}

} // namespace WebCore

#endif // ENABLE(WEB_REPLAY)
