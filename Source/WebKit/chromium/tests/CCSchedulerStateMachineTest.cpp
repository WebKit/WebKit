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

#include "config.h"

#include "cc/CCSchedulerStateMachine.h"

#include <gtest/gtest.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

using namespace WTF;
using namespace WebCore;

namespace {

const CCSchedulerStateMachine::CommitState allCommitStates[] = {
    CCSchedulerStateMachine::COMMIT_STATE_IDLE,
    CCSchedulerStateMachine::COMMIT_STATE_FRAME_IN_PROGRESS,
    CCSchedulerStateMachine::COMMIT_STATE_UPDATING_RESOURCES,
    CCSchedulerStateMachine::COMMIT_STATE_READY_TO_COMMIT,
    CCSchedulerStateMachine::COMMIT_STATE_WAITING_FOR_FIRST_DRAW
};

// Exposes the protected state fields of the CCSchedulerStateMachine for testing
class StateMachine : public CCSchedulerStateMachine {
public:
    void setCommitState(CommitState cs) { m_commitState = cs; }
    CommitState commitState() const { return  m_commitState; }

    void setNeedsCommit(bool b) { m_needsCommit = b; }
    bool needsCommit() const { return m_needsCommit; }

    void setNeedsRedraw(bool b) { m_needsRedraw = b; }
    bool needsRedraw() const { return m_needsRedraw; }

    bool insideVSync() const { return m_insideVSync; }
    bool visible() const { return m_visible; }

    void setUpdateMoreResourcesPending(bool b) { m_updateMoreResourcesPending = b; }
    bool updateMoreResourcesPending() const { return m_updateMoreResourcesPending; }
};

TEST(CCSchedulerStateMachineTest, TestNextActionBeginsFrameIfNeeded)
{
    // If no commit needed, do nothing
    {
        StateMachine state;
        state.setCommitState(CCSchedulerStateMachine::COMMIT_STATE_IDLE);
        state.setNeedsRedraw(false);
        state.setNeedsCommit(false);
        state.setUpdateMoreResourcesPending(false);
        state.setVisible(true);

        state.setInsideVSync(false);
        EXPECT_EQ(CCSchedulerStateMachine::ACTION_NONE, state.nextAction());
        state.setInsideVSync(true);
        EXPECT_EQ(CCSchedulerStateMachine::ACTION_NONE, state.nextAction());
    }

    // If commit requested, begin a frame
    {
        StateMachine state;
        state.setCommitState(CCSchedulerStateMachine::COMMIT_STATE_IDLE);
        state.setNeedsRedraw(false);
        state.setNeedsCommit(true);
        state.setUpdateMoreResourcesPending(false);
        state.setVisible(true);
    }

    // Begin the frame, make sure needsCommit and commitState update correctly.
    {
        StateMachine state;
        state.setVisible(true);
        state.updateState(CCSchedulerStateMachine::ACTION_BEGIN_FRAME);
        EXPECT_EQ(CCSchedulerStateMachine::COMMIT_STATE_FRAME_IN_PROGRESS, state.commitState());
        EXPECT_FALSE(state.needsCommit());
    }
}

TEST(CCSchedulerStateMachineTest, TestNextActionDrawsOnVSync)
{
    // When not on vsync, don't draw.
    size_t numCommitStates = sizeof(allCommitStates) / sizeof(CCSchedulerStateMachine::CommitState);
    for (size_t i = 0; i < numCommitStates; ++i) {
        StateMachine state;
        state.setCommitState(allCommitStates[i]);
        state.setNeedsRedraw(true);
        state.setVisible(true);
        state.setInsideVSync(false);

        // Case 1: needsCommit=false updateMoreResourcesPending=false.
        state.setNeedsCommit(false);
        state.setUpdateMoreResourcesPending(false);
        EXPECT_NE(CCSchedulerStateMachine::ACTION_DRAW, state.nextAction());

        // Case 2: needsCommit=false updateMoreResourcesPending=true.
        state.setNeedsCommit(false);
        state.setUpdateMoreResourcesPending(true);
        EXPECT_NE(CCSchedulerStateMachine::ACTION_DRAW, state.nextAction());

        // Case 3: needsCommit=true updateMoreResourcesPending=false.
        state.setNeedsCommit(true);
        state.setUpdateMoreResourcesPending(false);
        EXPECT_NE(CCSchedulerStateMachine::ACTION_DRAW, state.nextAction());

        // Case 4: needsCommit=true updateMoreResourcesPending=true.
        state.setNeedsCommit(true);
        state.setUpdateMoreResourcesPending(true);
        EXPECT_NE(CCSchedulerStateMachine::ACTION_DRAW, state.nextAction());
    }

    // When on vsync, you should always draw. Expect if you're ready to commit, in which case commit.
    for (size_t i = 0; i < numCommitStates; ++i) {
        StateMachine state;
        state.setCommitState(allCommitStates[i]);
        state.setNeedsRedraw(true);
        state.setVisible(true);
        state.setInsideVSync(true);
        CCSchedulerStateMachine::Action expectedAction;
        if (allCommitStates[i] != CCSchedulerStateMachine::COMMIT_STATE_READY_TO_COMMIT)
            expectedAction = CCSchedulerStateMachine::ACTION_DRAW;
        else
            expectedAction = CCSchedulerStateMachine::ACTION_COMMIT;

        // Case 1: needsCommit=false updateMoreResourcesPending=false.
        state.setNeedsCommit(false);
        state.setUpdateMoreResourcesPending(false);
        EXPECT_EQ(expectedAction, state.nextAction());

        // Case 2: needsCommit=false updateMoreResourcesPending=true.
        state.setNeedsCommit(false);
        state.setUpdateMoreResourcesPending(true);
        EXPECT_EQ(expectedAction, state.nextAction());

        // Case 3: needsCommit=true updateMoreResourcesPending=false.
        state.setNeedsCommit(true);
        state.setUpdateMoreResourcesPending(false);
        EXPECT_EQ(expectedAction, state.nextAction());

        // Case 4: needsCommit=true updateMoreResourcesPending=true.
        state.setNeedsCommit(true);
        state.setUpdateMoreResourcesPending(true);
        EXPECT_EQ(expectedAction, state.nextAction());
    }
}

TEST(CCSchedulerStateMachineTest, TestNoCommitStatesRedrawWhenInvisible)
{
    // If not visible, never redraw.
    size_t numCommitStates = sizeof(allCommitStates) / sizeof(CCSchedulerStateMachine::CommitState);
    for (size_t i = 0; i < numCommitStates; ++i) {
        StateMachine state;
        state.setCommitState(allCommitStates[i]);
        state.setNeedsRedraw(true);
        state.setInsideVSync(false);
        state.setVisible(false);

        // Case 1: needsCommit=false updateMoreResourcesPending=false.
        state.setNeedsCommit(false);
        state.setUpdateMoreResourcesPending(false);
        EXPECT_NE(CCSchedulerStateMachine::ACTION_DRAW, state.nextAction());

        // Case 2: needsCommit=false updateMoreResourcesPending=true.
        state.setNeedsCommit(false);
        state.setUpdateMoreResourcesPending(true);
        EXPECT_NE(CCSchedulerStateMachine::ACTION_DRAW, state.nextAction());

        // Case 3: needsCommit=true updateMoreResourcesPending=false.
        state.setNeedsCommit(true);
        state.setUpdateMoreResourcesPending(false);
        EXPECT_NE(CCSchedulerStateMachine::ACTION_DRAW, state.nextAction());

        // Case 4: needsCommit=true updateMoreResourcesPending=true.
        state.setNeedsCommit(true);
        state.setUpdateMoreResourcesPending(true);
        EXPECT_NE(CCSchedulerStateMachine::ACTION_DRAW, state.nextAction());
    }
}

TEST(CCSchedulerStateMachineTest, TestUpdates_NoRedraw_OneRoundOfUpdates)
{
    StateMachine state;
    state.setCommitState(CCSchedulerStateMachine::COMMIT_STATE_UPDATING_RESOURCES);
    state.setNeedsRedraw(false);
    state.setUpdateMoreResourcesPending(false);
    state.setVisible(true);

    // Verify we begin update, both for vsync and not vsync.
    state.setInsideVSync(false);
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_BEGIN_UPDATE_MORE_RESOURCES, state.nextAction());
    state.setInsideVSync(true);
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_BEGIN_UPDATE_MORE_RESOURCES, state.nextAction());

    // Begin an update.
    state.updateState(CCSchedulerStateMachine::ACTION_BEGIN_UPDATE_MORE_RESOURCES);

    // Veriify we don't do anything, both for vsync and not vsync.
    state.setInsideVSync(false);
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_NONE, state.nextAction());
    state.setInsideVSync(true);
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_NONE, state.nextAction());

    // End update with no more updates pending.
    state.beginUpdateMoreResourcesComplete(false);
    state.setInsideVSync(false);
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_COMMIT, state.nextAction());
}

TEST(CCSchedulerStateMachineTest, TestUpdates_NoRedraw_TwoRoundsOfUpdates)
{
    StateMachine state;
    state.setCommitState(CCSchedulerStateMachine::COMMIT_STATE_UPDATING_RESOURCES);
    state.setNeedsRedraw(false);
    state.setUpdateMoreResourcesPending(false);
    state.setVisible(true);

    // Verify the update begins, both for vsync and not vsync.
    state.setInsideVSync(false);
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_BEGIN_UPDATE_MORE_RESOURCES, state.nextAction());
    state.setInsideVSync(true);
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_BEGIN_UPDATE_MORE_RESOURCES, state.nextAction());

    // Begin an update.
    state.updateState(CCSchedulerStateMachine::ACTION_BEGIN_UPDATE_MORE_RESOURCES);

    // Verify we do nothing, both for vsync and not vsync.
    state.setInsideVSync(false);
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_NONE, state.nextAction());
    state.setInsideVSync(true);
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_NONE, state.nextAction());

    // Ack the update with more pending.
    state.beginUpdateMoreResourcesComplete(true);

    // Verify we update more, both for vsync and not vsync.
    state.setInsideVSync(false);
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_BEGIN_UPDATE_MORE_RESOURCES, state.nextAction());
    state.setInsideVSync(true);
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_BEGIN_UPDATE_MORE_RESOURCES, state.nextAction());

    // Begin another update, while inside vsync. And, it updating.
    state.setInsideVSync(true);
    state.updateState(CCSchedulerStateMachine::ACTION_BEGIN_UPDATE_MORE_RESOURCES);
    state.beginUpdateMoreResourcesComplete(false);

    // Make sure we commit, independent of vsync.
    state.setInsideVSync(false);
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_COMMIT, state.nextAction());
    state.setInsideVSync(true);
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_COMMIT, state.nextAction());
}

TEST(CCSchedulerStateMachineTest, TestUpdates_WithRedraw_OneRoundOfUpdates)
{
    StateMachine state;
    state.setCommitState(CCSchedulerStateMachine::COMMIT_STATE_UPDATING_RESOURCES);
    state.setNeedsRedraw(true);
    state.setUpdateMoreResourcesPending(false);
    state.setVisible(true);
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_BEGIN_UPDATE_MORE_RESOURCES, state.nextAction());

    // Begin an update.
    state.updateState(CCSchedulerStateMachine::ACTION_BEGIN_UPDATE_MORE_RESOURCES);

    // Ensure we draw on the next vsync even though an update is in-progress.
    state.setInsideVSync(true);
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_DRAW, state.nextAction());
    state.updateState(CCSchedulerStateMachine::ACTION_DRAW);

    // Ensure that we once we have drawn, we dont do anything else.
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_NONE, state.nextAction());

    // Leave the vsync before we finish the update.
    state.setInsideVSync(false);

    // Finish update but leave more resources pending.
    state.beginUpdateMoreResourcesComplete(true);

    // Verify that regardless of vsync, we update some more.
    state.setInsideVSync(false);
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_BEGIN_UPDATE_MORE_RESOURCES, state.nextAction());
    state.setInsideVSync(true);
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_BEGIN_UPDATE_MORE_RESOURCES, state.nextAction());

    // Begin another update. Finish it immediately. Inside the vsync.
    state.setInsideVSync(true);
    state.updateState(CCSchedulerStateMachine::ACTION_BEGIN_UPDATE_MORE_RESOURCES);
    state.setInsideVSync(false);
    state.beginUpdateMoreResourcesComplete(false);

    // Verify we commit regardless of vsync state
    state.setInsideVSync(false);
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_COMMIT, state.nextAction());
    state.setInsideVSync(true);
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_COMMIT, state.nextAction());
}

TEST(CCSchedulerStateMachineTest, TestSetNeedsCommitIsNotLost)
{
    StateMachine state;
    state.setNeedsCommit(true);
    state.setVisible(true);

    // Begin the frame.
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_BEGIN_FRAME, state.nextAction());
    state.updateState(state.nextAction());
    EXPECT_EQ(CCSchedulerStateMachine::COMMIT_STATE_FRAME_IN_PROGRESS, state.commitState());

    // Now, while the frame is in progress, set another commit.
    state.setNeedsCommit(true);
    EXPECT_TRUE(state.needsCommit());

    // Let the frame finish.
    state.beginFrameComplete();
    EXPECT_EQ(CCSchedulerStateMachine::COMMIT_STATE_UPDATING_RESOURCES, state.commitState());
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_BEGIN_UPDATE_MORE_RESOURCES, state.nextAction());
    state.updateState(CCSchedulerStateMachine::ACTION_BEGIN_UPDATE_MORE_RESOURCES);
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_NONE, state.nextAction());
    state.beginUpdateMoreResourcesComplete(false);
    EXPECT_EQ(CCSchedulerStateMachine::COMMIT_STATE_READY_TO_COMMIT, state.commitState());

    // Expect to commit regardless of vsync state.
    state.setInsideVSync(false);
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_COMMIT, state.nextAction());
    state.setInsideVSync(true);
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_COMMIT, state.nextAction());

    // Commit and make sure we draw on next vsync
    state.updateState(CCSchedulerStateMachine::ACTION_COMMIT);
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_DRAW, state.nextAction());
    EXPECT_EQ(CCSchedulerStateMachine::COMMIT_STATE_WAITING_FOR_FIRST_DRAW, state.commitState());
    state.updateState(CCSchedulerStateMachine::ACTION_DRAW);

    // Verify that another commit will begin.
    state.setInsideVSync(false);
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_BEGIN_FRAME, state.nextAction());
}

TEST(CCSchedulerStateMachineTest, TestFullCycle)
{
    StateMachine state;
    state.setVisible(true);

    // Start clean and set commit.
    state.setNeedsCommit(true);
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_BEGIN_FRAME, state.nextAction());

    // Begin the frame.
    state.updateState(CCSchedulerStateMachine::ACTION_BEGIN_FRAME);
    EXPECT_EQ(CCSchedulerStateMachine::COMMIT_STATE_FRAME_IN_PROGRESS, state.commitState());
    EXPECT_FALSE(state.needsCommit());
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_NONE, state.nextAction());

    // Tell the scheduler the frame finished.
    state.beginFrameComplete();
    EXPECT_EQ(CCSchedulerStateMachine::COMMIT_STATE_UPDATING_RESOURCES, state.commitState());
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_BEGIN_UPDATE_MORE_RESOURCES, state.nextAction());

    // Tell the scheduler the update began and finished
    state.updateState(CCSchedulerStateMachine::ACTION_BEGIN_UPDATE_MORE_RESOURCES);
    state.beginUpdateMoreResourcesComplete(false);
    EXPECT_EQ(CCSchedulerStateMachine::COMMIT_STATE_READY_TO_COMMIT, state.commitState());
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_COMMIT, state.nextAction());

    // Commit.
    state.updateState(CCSchedulerStateMachine::ACTION_COMMIT);
    EXPECT_EQ(CCSchedulerStateMachine::COMMIT_STATE_IDLE, state.commitState());
    EXPECT_TRUE(state.needsRedraw());

    // Expect to do nothing until vsync.
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_NONE, state.nextAction());

    // At vsync, draw.
    state.setInsideVSync(true);
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_DRAW, state.nextAction());
    state.updateState(CCSchedulerStateMachine::ACTION_DRAW);
    state.setInsideVSync(false);

    // Should be synchronized, no draw needed, no action needed.
    EXPECT_EQ(CCSchedulerStateMachine::COMMIT_STATE_IDLE, state.commitState());
    EXPECT_FALSE(state.needsRedraw());
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_NONE, state.nextAction());
}

TEST(CCSchedulerStateMachineTest, TestFullCycleWithCommitRequestInbetween)
{
    StateMachine state;
    state.setVisible(true);

    // Start clean and set commit.
    state.setNeedsCommit(true);
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_BEGIN_FRAME, state.nextAction());

    // Begin the frame.
    state.updateState(CCSchedulerStateMachine::ACTION_BEGIN_FRAME);
    EXPECT_EQ(CCSchedulerStateMachine::COMMIT_STATE_FRAME_IN_PROGRESS, state.commitState());
    EXPECT_FALSE(state.needsCommit());
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_NONE, state.nextAction());

    // Request another commit while the commit is in flight.
    state.setNeedsCommit(true);
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_NONE, state.nextAction());

    // Tell the scheduler the frame finished.
    state.beginFrameComplete();
    EXPECT_EQ(CCSchedulerStateMachine::COMMIT_STATE_UPDATING_RESOURCES, state.commitState());
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_BEGIN_UPDATE_MORE_RESOURCES, state.nextAction());

    // Tell the scheduler the update began and finished
    state.updateState(CCSchedulerStateMachine::ACTION_BEGIN_UPDATE_MORE_RESOURCES);
    state.beginUpdateMoreResourcesComplete(false);
    EXPECT_EQ(CCSchedulerStateMachine::COMMIT_STATE_READY_TO_COMMIT, state.commitState());
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_COMMIT, state.nextAction());

    // Commit.
    state.updateState(CCSchedulerStateMachine::ACTION_COMMIT);
    EXPECT_EQ(CCSchedulerStateMachine::COMMIT_STATE_WAITING_FOR_FIRST_DRAW, state.commitState());
    EXPECT_TRUE(state.needsRedraw());

    // Expect to do nothing until vsync.
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_NONE, state.nextAction());

    // At vsync, draw.
    state.setInsideVSync(true);
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_DRAW, state.nextAction());
    state.updateState(CCSchedulerStateMachine::ACTION_DRAW);
    state.setInsideVSync(false);

    // Should be synchronized, no draw needed, no action needed.
    EXPECT_EQ(CCSchedulerStateMachine::COMMIT_STATE_IDLE, state.commitState());
    EXPECT_FALSE(state.needsRedraw());
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_BEGIN_FRAME, state.nextAction());
}

TEST(CCSchedulerStateMachineTest, TestRequestCommitInvisible)
{
    StateMachine state;
    state.setNeedsCommit(true);
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_NONE, state.nextAction());
}

TEST(CCSchedulerStateMachineTest, TestGoesInvisibleMidCommit)
{
    StateMachine state;
    state.setVisible(true);

    // Start clean and set commit.
    state.setNeedsCommit(true);
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_BEGIN_FRAME, state.nextAction());

    // Begin the frame while visible.
    state.updateState(CCSchedulerStateMachine::ACTION_BEGIN_FRAME);
    EXPECT_EQ(CCSchedulerStateMachine::COMMIT_STATE_FRAME_IN_PROGRESS, state.commitState());
    EXPECT_FALSE(state.needsCommit());
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_NONE, state.nextAction());

    // Become invisible
    state.setVisible(false);

    // Tell the scheduler the frame finished
    state.beginFrameComplete();
    EXPECT_EQ(CCSchedulerStateMachine::COMMIT_STATE_UPDATING_RESOURCES, state.commitState());
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_BEGIN_UPDATE_MORE_RESOURCES, state.nextAction());

    // Tell the scheduler the update began and finished
    state.updateState(CCSchedulerStateMachine::ACTION_BEGIN_UPDATE_MORE_RESOURCES);
    state.beginUpdateMoreResourcesComplete(false);
    EXPECT_EQ(CCSchedulerStateMachine::COMMIT_STATE_READY_TO_COMMIT, state.commitState());
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_COMMIT, state.nextAction());

    // Commit in invisible state should leave us:
    // - COMMIT_STATE_WAITING_FOR_FIRST_DRAW
    // - Waiting for redraw.
    // - No commit needed
    state.updateState(CCSchedulerStateMachine::ACTION_COMMIT);
    EXPECT_EQ(CCSchedulerStateMachine::COMMIT_STATE_WAITING_FOR_FIRST_DRAW, state.commitState());
    EXPECT_TRUE(state.needsRedraw());
    EXPECT_FALSE(state.needsCommit());

    // Expect to do nothing, both in and out of vsync.
    state.setInsideVSync(false);
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_NONE, state.nextAction());
    state.setInsideVSync(true);
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_NONE, state.nextAction());
}

}
