/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef CCSchedulerStateMachine_h
#define CCSchedulerStateMachine_h

#include <wtf/Noncopyable.h>

namespace WebCore {

// The CCSchedulerStateMachine decides how to coordinate main thread activites
// like painting/running javascript with rendering and input activities on the
// impl thread.
//
// The state machine tracks internal state but is also influenced by external state.
// Internal state includes things like whether a frame has been requested, while
// external state includes things like the current time being near to the vblank time.
//
// The scheduler seperates "what to do next" from the updating of its internal state to
// make testing cleaner.
class CCSchedulerStateMachine {
public:
    CCSchedulerStateMachine();

    enum CommitState {
        COMMIT_STATE_IDLE,
        COMMIT_STATE_FRAME_IN_PROGRESS,
        COMMIT_STATE_UPDATING_RESOURCES,
        COMMIT_STATE_READY_TO_COMMIT,
        COMMIT_STATE_WAITING_FOR_FIRST_DRAW,
    };

    bool commitPending() const
    {
        return m_commitState != COMMIT_STATE_IDLE;
    }

    bool redrawPending() const { return m_needsRedraw; }

    enum Action {
        ACTION_NONE,
        ACTION_BEGIN_FRAME,
        ACTION_BEGIN_UPDATE_MORE_RESOURCES,
        ACTION_COMMIT,
        ACTION_DRAW,
    };
    Action nextAction() const;
    void updateState(Action);

    // Indicates that the system has entered and left a vsync callback.
    // The scheduler will not draw more than once in a given vsync callback.
    void didEnterVSync();
    void didLeaveVSync();

    // Indicates whether the LayerTreeHostImpl is visible.
    void setVisible(bool);

    // Indicates that a redraw is required, either due to the impl tree changing
    // or the screen being damaged and simply needing redisplay.
    void setNeedsRedraw();

    // As setNeedsRedraw(), but ensures the draw will definitely happen even if
    // we are not visible.
    void setNeedsForcedRedraw();

    // Indicates that a new commit flow needs to be performed, either to pull
    // updates from the main thread to the impl, or to push deltas from the impl
    // thread to main.
    void setNeedsCommit();

    // Call this only in response to receiving an ACTION_BEGIN_FRAME
    // from nextState. Indicates that all painting is complete and that
    // updating of compositor resources can begin.
    void beginFrameComplete();

    // Call this only in response to receiving an ACTION_UPDATE_MORE_RESOURCES
    // from nextState. Indicates that the specific update request completed.
    void beginUpdateMoreResourcesComplete(bool morePending);

    // Indicates whether drawing would, at this time, make sense.
    // canDraw can be used to supress flashes or checkerboarding
    // when such behavior would be undesirable.
    void setCanDraw(bool can) { m_canDraw = can; }

protected:
    CommitState m_commitState;

    int m_currentFrameNumber;
    int m_lastFrameNumberWhereDrawWasCalled;
    bool m_needsRedraw;
    bool m_needsForcedRedraw;
    bool m_needsCommit;
    bool m_updateMoreResourcesPending;
    bool m_insideVSync;
    bool m_visible;
    bool m_canDraw;
};

}

#endif // CCSchedulerStateMachine_h
