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
#include "Logging.h"
#include "MainFrame.h"
#include "Page.h"
#include "ReplaySession.h"
#include "ReplaySessionSegment.h"
#include "ReplayingInputCursor.h"
#include "ScriptController.h"
#include "WebReplayInputs.h"
#include <replay/EmptyInputCursor.h>
#include <wtf/text/CString.h>

namespace WebCore {

ReplayController::ReplayController(Page& page)
    : m_page(page)
    , m_loadedSegment(nullptr)
    , m_loadedSession(ReplaySession::create())
    , m_emptyCursor(EmptyInputCursor::create())
    , m_activeCursor(nullptr)
    , m_targetPosition(ReplayPosition(0, 0))
    , m_currentPosition(ReplayPosition(0, 0))
    , m_segmentState(SegmentState::Unloaded)
    , m_sessionState(SessionState::Inactive)
    , m_dispatchSpeed(DispatchSpeed::FastForward)
{
}

void ReplayController::switchSession(PassRefPtr<ReplaySession> session)
{
    ASSERT(m_segmentState == SegmentState::Unloaded);
    ASSERT(m_sessionState == SessionState::Inactive);

    m_loadedSession = session;
    m_currentPosition = ReplayPosition(0, 0);

    LOG(WebReplay, "%-20sSwitching sessions from %p to %p.\n", "ReplayController", m_loadedSession.get(), session.get());
    InspectorInstrumentation::sessionLoaded(&m_page, m_loadedSession);
}

void ReplayController::createSegment()
{
    ASSERT(m_sessionState == SessionState::Capturing);
    ASSERT(m_segmentState == SegmentState::Unloaded);

    m_segmentState = SegmentState::Appending;

    // Create a new segment but don't associate it with the current session
    // until we stop appending to it. This preserves the invariant that
    // segments associated with a replay session have immutable data.
    m_loadedSegment = ReplaySessionSegment::create();

    LOG(WebReplay, "%-20s Created segment: %p.\n", "ReplayController", m_loadedSegment.get());
    InspectorInstrumentation::segmentCreated(&m_page, m_loadedSegment);

    m_activeCursor = m_loadedSegment->createCapturingCursor(m_page);
    m_activeCursor->appendInput<BeginSegmentSentinel>();

    std::unique_ptr<InitialNavigation> navigationInput = InitialNavigation::createFromPage(m_page);
    // Dispatching this input schedules navigation of the main frame, causing a refresh.
    navigationInput->dispatch(*this);
    m_activeCursor->storeInput(std::move(navigationInput));
}

void ReplayController::completeSegment()
{
    ASSERT(m_sessionState == SessionState::Capturing);
    ASSERT(m_segmentState == SegmentState::Appending);

    m_activeCursor->appendInput<EndSegmentSentinel>();

    // Hold on to a reference so unloading the segment doesn't deallocate it.
    RefPtr<ReplaySessionSegment> segment = m_loadedSegment;
    m_segmentState = SegmentState::Loaded;
    bool shouldSuppressNotifications = true;
    unloadSegment(shouldSuppressNotifications);

    LOG(WebReplay, "%-20s Completed segment: %p.\n", "ReplayController", segment.get());
    InspectorInstrumentation::segmentCompleted(&m_page, segment);

    m_loadedSession->appendSegment(segment);
    InspectorInstrumentation::sessionModified(&m_page, m_loadedSession);
}

void ReplayController::loadSegment(PassRefPtr<ReplaySessionSegment> prpSegment)
{
    RefPtr<ReplaySessionSegment> segment = prpSegment;

    ASSERT(m_sessionState == SessionState::Replaying);
    ASSERT(m_segmentState == SegmentState::Unloaded);
    ASSERT(segment);
    ASSERT(!m_loadedSegment);

    m_loadedSegment = segment;
    m_segmentState = SegmentState::Loaded;

    m_activeCursor = m_loadedSegment->createReplayingCursor(m_page, this);
    dispatcher().setDispatchSpeed(m_dispatchSpeed);

    LOG(WebReplay, "%-20sLoading segment: %p.\n", "ReplayController", segment.get());
    InspectorInstrumentation::segmentLoaded(&m_page, segment);
}

void ReplayController::unloadSegment(bool suppressNotifications)
{
    ASSERT(m_sessionState != SessionState::Inactive);
    ASSERT(m_segmentState == SegmentState::Loaded);

    m_segmentState = SegmentState::Unloaded;

    LOG(WebReplay, "%-20s Clearing input cursors for page: %p\n", "ReplayController", &m_page);

    m_activeCursor = nullptr;
    RefPtr<ReplaySessionSegment> unloadedSegment = m_loadedSegment.release();
    for (Frame* frame = &m_page.mainFrame(); frame; frame = frame->tree().traverseNext()) {
        frame->script().globalObject(mainThreadNormalWorld())->setInputCursor(m_emptyCursor);
        frame->document()->setInputCursor(m_emptyCursor);
    }

    // When we stop capturing, don't send out segment unloaded events since we
    // didn't send out the corresponding segmentLoaded event at the start of capture.
    if (!suppressNotifications) {
        LOG(WebReplay, "%-20sUnloading segment: %p.\n", "ReplayController", unloadedSegment.get());
        InspectorInstrumentation::segmentUnloaded(&m_page);
    }
}

void ReplayController::startCapturing()
{
    ASSERT(m_sessionState == SessionState::Inactive);
    ASSERT(m_segmentState == SegmentState::Unloaded);

    m_sessionState = SessionState::Capturing;

    LOG(WebReplay, "%-20s Starting capture.\n", "ReplayController");
    InspectorInstrumentation::captureStarted(&m_page);

    m_currentPosition = ReplayPosition(0, 0);
    createSegment();
}

void ReplayController::stopCapturing()
{
    ASSERT(m_sessionState == SessionState::Capturing);
    ASSERT(m_segmentState == SegmentState::Appending);

    completeSegment();

    m_sessionState = SessionState::Inactive;

    LOG(WebReplay, "%-20s Stopping capture.\n", "ReplayController");
    InspectorInstrumentation::captureStopped(&m_page);
}

void ReplayController::startPlayback()
{
    ASSERT(m_sessionState == SessionState::Replaying);
    ASSERT(m_segmentState == SegmentState::Loaded);

    m_segmentState = SegmentState::Dispatching;

    LOG(WebReplay, "%-20s Starting playback to position (segment: %d, input: %d).\n", "ReplayController", m_targetPosition.segmentOffset, m_targetPosition.inputOffset);
    InspectorInstrumentation::playbackStarted(&m_page);

    dispatcher().run();
}

void ReplayController::pausePlayback()
{
    ASSERT(m_sessionState == SessionState::Replaying);
    ASSERT(m_segmentState == SegmentState::Dispatching);

    m_segmentState = SegmentState::Loaded;

    dispatcher().pause();

    LOG(WebReplay, "%-20s Pausing playback at position (segment: %d, input: %d).\n", "ReplayController", m_currentPosition.segmentOffset, m_currentPosition.inputOffset);
    InspectorInstrumentation::playbackPaused(&m_page, m_currentPosition);
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
}

void ReplayController::replayToPosition(const ReplayPosition& position, DispatchSpeed speed)
{
    ASSERT(m_sessionState != SessionState::Capturing);
    ASSERT(m_segmentState == SegmentState::Loaded || m_segmentState == SegmentState::Unloaded);
    ASSERT(position.segmentOffset < m_loadedSession->size());

    m_dispatchSpeed = speed;

    if (m_sessionState != SessionState::Replaying)
        m_sessionState = SessionState::Replaying;

    if (m_segmentState == SegmentState::Unloaded)
        loadSegment(m_loadedSession->at(position.segmentOffset));
    else if (position.segmentOffset != m_currentPosition.segmentOffset || m_currentPosition.inputOffset > position.inputOffset) {
        // If the desired segment is not loaded or we have gone past the desired input
        // offset, then unload the current segment and load the appropriate segment.
        unloadSegment();
        loadSegment(m_loadedSession->at(position.segmentOffset));
    }

    ASSERT(m_currentPosition.segmentOffset == position.segmentOffset);
    ASSERT(m_loadedSession->at(position.segmentOffset) == m_loadedSegment);

    m_targetPosition = position;
    startPlayback();
}

void ReplayController::frameNavigated(DocumentLoader* loader)
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
    loader->frame()->document()->setInputCursor(m_activeCursor.get());
    loader->frame()->script().globalObject(mainThreadNormalWorld())->setInputCursor(m_activeCursor.get());
}

void ReplayController::frameDetached(Frame* frame)
{
    ASSERT(m_sessionState != SessionState::Inactive);
    ASSERT(frame);

    if (!frame->document())
        return;

    // If the frame's cursor isn't capturing or replaying, we should do nothing.
    // This is the case for the "outbound" frame when starting capture, or when
    // we clear the input cursor to finish or prematurely unload a segment.
    if (frame->document()->inputCursor().isCapturing()) {
        ASSERT(m_segmentState == SegmentState::Appending);
        completeSegment();
    }

    // During playback, the segments are unloaded and loaded when the final
    // input has been dispatched. So, nothing needs to be done here.
}

PassRefPtr<ReplaySession> ReplayController::loadedSession() const
{
    return m_loadedSession;
}

PassRefPtr<ReplaySessionSegment> ReplayController::loadedSegment() const
{
    return m_loadedSegment;
}

InputCursor& ReplayController::activeInputCursor() const
{
    return m_activeCursor ? *m_activeCursor : *m_emptyCursor;
}

EventLoopInputDispatcher& ReplayController::dispatcher() const
{
    ASSERT(m_sessionState == SessionState::Replaying);
    ASSERT(m_segmentState == SegmentState::Dispatching);
    ASSERT(m_activeCursor);
    ASSERT(m_activeCursor->isReplaying());

    return static_cast<ReplayingInputCursor&>(*m_activeCursor).dispatcher();
}

void ReplayController::willDispatchInput(const EventLoopInputBase&)
{
    ASSERT(m_sessionState == SessionState::Replaying);
    ASSERT(m_segmentState == SegmentState::Dispatching);

    m_currentPosition.inputOffset++;
    if (m_currentPosition == m_targetPosition)
        pausePlayback();
}

void ReplayController::didDispatchInput(const EventLoopInputBase&)
{
    ASSERT(m_sessionState == SessionState::Replaying);
    ASSERT(m_segmentState == SegmentState::Dispatching);

    InspectorInstrumentation::playbackHitPosition(&m_page, m_currentPosition);
}

void ReplayController::didDispatchFinalInput()
{
    ASSERT(m_segmentState == SegmentState::Dispatching);

    pause();
    unloadSegment();

    // No more segments left to replay; stop.
    if (++m_currentPosition.segmentOffset == m_loadedSession->size())
        return;

    loadSegment(m_loadedSession->at(m_currentPosition.segmentOffset));
    startPlayback();
}

} // namespace WebCore

#endif // ENABLE(WEB_REPLAY)
