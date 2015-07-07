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

#ifndef ReplayController_h
#define ReplayController_h

#if ENABLE(WEB_REPLAY)

#include "EventLoopInputDispatcher.h"
#include <wtf/Noncopyable.h>
#include <wtf/Vector.h>

// Determinism assertions are guarded by this macro. When a user-facing error reporting and
// recovery mechanism is implemented, this guard can be removed. <https://webkit.org/b/131279>
#define ENABLE_AGGRESSIVE_DETERMINISM_CHECKS 0

namespace JSC {
class InputCursor;
}

namespace WebCore {

class DOMWindow;
class Document;
class DocumentLoader;
class Element;
class Event;
class EventLoopInputBase;
class Frame;
class Node;
class Page;
class ReplaySession;
class ReplaySessionSegment;

// Each state may transition to the state immediately above or below it.
// SessionState transitions are only allowed when SegmentState is Unloaded.
enum class SessionState {
    Capturing,
    // Neither capturing or replaying. m_currentPosition is not valid in this state.
    Inactive,
    Replaying,
};

// Each state may transition to the state immediately above or below it.
enum class SegmentState {
    // Inputs can be appended into an unassociated session segment.
    // We can stop capturing, which reverts to the Unloaded state.
    Appending,
    // No session segment is loaded.
    // We can start capturing, or load a segment (and then replay it).
    Unloaded,
    // A session segment is loaded.
    // We can unload the segment, or begin playback from m_currentPosition.
    Loaded,
    // The controller is actively dispatching event loop inputs.
    // We can pause or cancel playback, which reverts to the Loaded state.
    Dispatching,
};

struct ReplayPosition {
    ReplayPosition(unsigned segmentOffset, unsigned inputOffset)
        : segmentOffset(segmentOffset)
        , inputOffset(inputOffset)
    {
    }

    // By convention, this position represents the end of the last segment of the session.
    ReplayPosition()
        : segmentOffset(0)
        , inputOffset(0)
    {
    }

    bool operator<(const ReplayPosition& other)
    {
        return segmentOffset <= other.segmentOffset && inputOffset < other.inputOffset;
    }

    bool operator==(const ReplayPosition& other)
    {
        return segmentOffset == other.segmentOffset && inputOffset == other.inputOffset;
    }

    unsigned segmentOffset;
    unsigned inputOffset;
};

class ReplayController final : public EventLoopInputDispatcherClient {
    WTF_MAKE_NONCOPYABLE(ReplayController);
public:
    ReplayController(Page&);

    void startCapturing();
    void stopCapturing();

    // Start or resume playback with default speed and target replay position.
    void startPlayback();
    void pausePlayback();
    void cancelPlayback();

    void replayToPosition(const ReplayPosition&, DispatchSpeed = DispatchSpeed::FastForward);
    void replayToCompletion(DispatchSpeed speed = DispatchSpeed::FastForward)
    {
        replayToPosition(ReplayPosition(), speed);
    }

    void switchSession(PassRefPtr<ReplaySession>);

    // InspectorReplayAgent notifications.
    void frameNavigated(DocumentLoader*);
    void frameDetached(Frame*);
    void willDispatchEvent(const Event&, Frame*);

    Page& page() const { return m_page; }
    SessionState sessionState() const { return m_sessionState; }
    PassRefPtr<ReplaySession> loadedSession() const;
    PassRefPtr<ReplaySessionSegment> loadedSegment() const;
    JSC::InputCursor& activeInputCursor() const;

private:
    // EventLoopInputDispatcherClient API
    virtual void willDispatchInput(const EventLoopInputBase&) override;
    virtual void didDispatchInput(const EventLoopInputBase&) override;
    virtual void didDispatchFinalInput() override;

    void createSegment();
    void completeSegment();

    void loadSegmentAtIndex(size_t);
    void unloadSegment(bool suppressNotifications = false);

    EventLoopInputDispatcher& dispatcher() const;

    void setSessionState(SessionState);
    void setForceDeterministicSettings(bool);

    struct SavedSettings {
        bool usesPageCache;

        SavedSettings()
            : usesPageCache(false)
        { }
    };

    Page& m_page;

    RefPtr<ReplaySessionSegment> m_loadedSegment;
    RefPtr<ReplaySession> m_loadedSession;
    const RefPtr<JSC::InputCursor> m_emptyCursor;
    // The active cursor is set to nullptr when invalid.
    RefPtr<JSC::InputCursor> m_activeCursor;

    // This position is valid when SessionState == Replaying.
    ReplayPosition m_targetPosition;
    // This position is valid when SessionState != Inactive.
    ReplayPosition m_currentPosition;
    SegmentState m_segmentState;
    // This tracks state across multiple segments. When navigating the main frame,
    // there is a small interval during segment switching when no segment is loaded.
    SessionState m_sessionState;

    DispatchSpeed m_dispatchSpeed;
    SavedSettings m_savedSettings;
};

} // namespace WebCore

#endif // ENABLE(WEB_REPLAY)

#endif // ReplayController_h
