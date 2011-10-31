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
    CCSchedulerStateMachine::COMMIT_STATE_READY_TO_COMMIT
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
        EXPECT_EQ(CCSchedulerStateMachine::ACTION_NONE, state.nextAction(CCSchedulerStateMachine::IMMEDIATE_STATE_NONE));
        EXPECT_EQ(CCSchedulerStateMachine::ACTION_NONE, state.nextAction(CCSchedulerStateMachine::IMMEDIATE_STATE_INSIDE_VSYNC));
    }

    // If commit requested, begin a frame
    {
        StateMachine state;
        state.setCommitState(CCSchedulerStateMachine::COMMIT_STATE_IDLE);
        state.setNeedsRedraw(false);
        state.setNeedsCommit(true);
        state.setUpdateMoreResourcesPending(false);
        EXPECT_EQ(CCSchedulerStateMachine::ACTION_BEGIN_FRAME, state.nextAction(CCSchedulerStateMachine::IMMEDIATE_STATE_NONE));
    }

    // Begin the frame, make sure needsCommit and commitState update correctly.
    {
        StateMachine state;
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

        // Case 1: needsCommit=false updateMoreResourcesPending=false vsync=false.
        state.setNeedsCommit(false);
        state.setUpdateMoreResourcesPending(false);
        EXPECT_NE(CCSchedulerStateMachine::ACTION_DRAW, state.nextAction(CCSchedulerStateMachine::IMMEDIATE_STATE_NONE));

        // Case 2: needsCommit=false updateMoreResourcesPending=true vsync=false.
        state.setNeedsCommit(false);
        state.setUpdateMoreResourcesPending(true);
        EXPECT_NE(CCSchedulerStateMachine::ACTION_DRAW, state.nextAction(CCSchedulerStateMachine::IMMEDIATE_STATE_NONE));

        // Case 3: needsCommit=true updateMoreResourcesPending=false vsync=false.
        state.setNeedsCommit(true);
        state.setUpdateMoreResourcesPending(false);
        EXPECT_NE(CCSchedulerStateMachine::ACTION_DRAW, state.nextAction(CCSchedulerStateMachine::IMMEDIATE_STATE_NONE));

        // Case 4: needsCommit=true updateMoreResourcesPending=true vsync=false.
        state.setNeedsCommit(true);
        state.setUpdateMoreResourcesPending(true);
        EXPECT_NE(CCSchedulerStateMachine::ACTION_DRAW, state.nextAction(CCSchedulerStateMachine::IMMEDIATE_STATE_NONE));
    }

    // When on vsync, you should always draw. Expect if you're ready to commit, in which case commit.
    for (size_t i = 0; i < numCommitStates; ++i) {
        StateMachine state;
        state.setCommitState(allCommitStates[i]);
        state.setNeedsRedraw(true);
        CCSchedulerStateMachine::Action expectedAction;
        if (allCommitStates[i] != CCSchedulerStateMachine::COMMIT_STATE_READY_TO_COMMIT)
            expectedAction = CCSchedulerStateMachine::ACTION_DRAW;
        else
            expectedAction = CCSchedulerStateMachine::ACTION_COMMIT;

        // Case 1: needsCommit=false updateMoreResourcesPending=false vsync=true.
        state.setNeedsCommit(false);
        state.setUpdateMoreResourcesPending(false);
        EXPECT_EQ(expectedAction, state.nextAction(CCSchedulerStateMachine::IMMEDIATE_STATE_INSIDE_VSYNC));

        // Case 2: needsCommit=false updateMoreResourcesPending=true vsync=true.
        state.setNeedsCommit(false);
        state.setUpdateMoreResourcesPending(true);
        EXPECT_EQ(expectedAction, state.nextAction(CCSchedulerStateMachine::IMMEDIATE_STATE_INSIDE_VSYNC));

        // Case 3: needsCommit=true updateMoreResourcesPending=false vsync=true.
        state.setNeedsCommit(true);
        state.setUpdateMoreResourcesPending(false);
        EXPECT_EQ(expectedAction, state.nextAction(CCSchedulerStateMachine::IMMEDIATE_STATE_INSIDE_VSYNC));

        // Case 4: needsCommit=true updateMoreResourcesPending=true vsync=true.
        state.setNeedsCommit(true);
        state.setUpdateMoreResourcesPending(true);
        EXPECT_EQ(expectedAction, state.nextAction(CCSchedulerStateMachine::IMMEDIATE_STATE_INSIDE_VSYNC));
    }
}

TEST(CCSchedulerStateMachineTest, TestUpdates_NoRedraw_OneRoundOfUpdates)
{
    StateMachine state;
    state.setCommitState(CCSchedulerStateMachine::COMMIT_STATE_UPDATING_RESOURCES);
    state.setNeedsRedraw(false);
    state.setUpdateMoreResourcesPending(false);
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_BEGIN_UPDATE_MORE_RESOURCES, state.nextAction(CCSchedulerStateMachine::IMMEDIATE_STATE_NONE));
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_BEGIN_UPDATE_MORE_RESOURCES, state.nextAction(CCSchedulerStateMachine::IMMEDIATE_STATE_INSIDE_VSYNC));

    // Begin an update.
    state.updateState(CCSchedulerStateMachine::ACTION_BEGIN_UPDATE_MORE_RESOURCES);
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_NONE, state.nextAction(CCSchedulerStateMachine::IMMEDIATE_STATE_NONE));
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_NONE, state.nextAction(CCSchedulerStateMachine::IMMEDIATE_STATE_INSIDE_VSYNC));

    // End update with no more updates pending.
    state.beginUpdateMoreResourcesComplete(false);
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_COMMIT, state.nextAction(CCSchedulerStateMachine::IMMEDIATE_STATE_NONE));
}

TEST(CCSchedulerStateMachineTest, TestUpdates_NoRedraw_TwoRoundsOfUpdates)
{
    StateMachine state;
    state.setCommitState(CCSchedulerStateMachine::COMMIT_STATE_UPDATING_RESOURCES);
    state.setNeedsRedraw(false);
    state.setUpdateMoreResourcesPending(false);
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_BEGIN_UPDATE_MORE_RESOURCES, state.nextAction(CCSchedulerStateMachine::IMMEDIATE_STATE_NONE));
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_BEGIN_UPDATE_MORE_RESOURCES, state.nextAction(CCSchedulerStateMachine::IMMEDIATE_STATE_INSIDE_VSYNC));

    // Begin an udpate.
    state.updateState(CCSchedulerStateMachine::ACTION_BEGIN_UPDATE_MORE_RESOURCES);
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_NONE, state.nextAction(CCSchedulerStateMachine::IMMEDIATE_STATE_NONE));
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_NONE, state.nextAction(CCSchedulerStateMachine::IMMEDIATE_STATE_INSIDE_VSYNC));

    // Ack the update with more pending.
    state.beginUpdateMoreResourcesComplete(true);
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_BEGIN_UPDATE_MORE_RESOURCES, state.nextAction(CCSchedulerStateMachine::IMMEDIATE_STATE_NONE));
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_BEGIN_UPDATE_MORE_RESOURCES, state.nextAction(CCSchedulerStateMachine::IMMEDIATE_STATE_INSIDE_VSYNC));

    // Begin another update, end it with none epending.
    state.updateState(CCSchedulerStateMachine::ACTION_BEGIN_UPDATE_MORE_RESOURCES);
    state.beginUpdateMoreResourcesComplete(false);
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_COMMIT, state.nextAction(CCSchedulerStateMachine::IMMEDIATE_STATE_NONE));
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_COMMIT, state.nextAction(CCSchedulerStateMachine::IMMEDIATE_STATE_INSIDE_VSYNC));
}

TEST(CCSchedulerStateMachineTest, TestUpdates_WithRedraw_OneRoundOfUpdates)
{
    StateMachine state;
    state.setCommitState(CCSchedulerStateMachine::COMMIT_STATE_UPDATING_RESOURCES);
    state.setNeedsRedraw(true);
    state.setUpdateMoreResourcesPending(false);
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_BEGIN_UPDATE_MORE_RESOURCES, state.nextAction(CCSchedulerStateMachine::IMMEDIATE_STATE_NONE));

    // Begin an update.
    state.updateState(CCSchedulerStateMachine::ACTION_BEGIN_UPDATE_MORE_RESOURCES);
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_DRAW, state.nextAction(CCSchedulerStateMachine::IMMEDIATE_STATE_INSIDE_VSYNC));

    // Draw.
    state.updateState(CCSchedulerStateMachine::ACTION_DRAW);
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_NONE, state.nextAction(CCSchedulerStateMachine::IMMEDIATE_STATE_INSIDE_VSYNC));

    // Finish update, levae more resources pending.
    state.beginUpdateMoreResourcesComplete(true);
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_BEGIN_UPDATE_MORE_RESOURCES, state.nextAction(CCSchedulerStateMachine::IMMEDIATE_STATE_NONE));
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_BEGIN_UPDATE_MORE_RESOURCES, state.nextAction(CCSchedulerStateMachine::IMMEDIATE_STATE_INSIDE_VSYNC));

    // Begin another update. Finish it immediately.
    state.updateState(CCSchedulerStateMachine::ACTION_BEGIN_UPDATE_MORE_RESOURCES);
    state.beginUpdateMoreResourcesComplete(false);
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_COMMIT, state.nextAction(CCSchedulerStateMachine::IMMEDIATE_STATE_NONE));
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_COMMIT, state.nextAction(CCSchedulerStateMachine::IMMEDIATE_STATE_INSIDE_VSYNC));
}

TEST(CCSchedulerStateMachineTest, TestSetNeedsCommitIsNotLost)
{
    StateMachine state;
    state.setNeedsCommit(true);

    // Begin the frame.
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_BEGIN_FRAME, state.nextAction(CCSchedulerStateMachine::IMMEDIATE_STATE_NONE));
    state.updateState(state.nextAction(CCSchedulerStateMachine::IMMEDIATE_STATE_NONE));
    EXPECT_EQ(CCSchedulerStateMachine::COMMIT_STATE_FRAME_IN_PROGRESS, state.commitState());

    // Now, while the frame is in progress, set another commit.
    state.setNeedsCommit(true);
    EXPECT_TRUE(state.needsCommit());

    // Let the frame finish.
    state.beginFrameComplete();
    EXPECT_EQ(CCSchedulerStateMachine::COMMIT_STATE_UPDATING_RESOURCES, state.commitState());
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_BEGIN_UPDATE_MORE_RESOURCES, state.nextAction(CCSchedulerStateMachine::IMMEDIATE_STATE_NONE));
    state.updateState(CCSchedulerStateMachine::ACTION_BEGIN_UPDATE_MORE_RESOURCES);
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_NONE, state.nextAction(CCSchedulerStateMachine::IMMEDIATE_STATE_NONE));
    state.beginUpdateMoreResourcesComplete(false);
    EXPECT_EQ(CCSchedulerStateMachine::COMMIT_STATE_READY_TO_COMMIT, state.commitState());
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_COMMIT, state.nextAction(CCSchedulerStateMachine::IMMEDIATE_STATE_NONE));
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_COMMIT, state.nextAction(CCSchedulerStateMachine::IMMEDIATE_STATE_INSIDE_VSYNC));
    state.updateState(CCSchedulerStateMachine::ACTION_COMMIT);
    EXPECT_EQ(CCSchedulerStateMachine::COMMIT_STATE_IDLE, state.commitState());

    // Verify that another commit will begin.
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_BEGIN_FRAME, state.nextAction(CCSchedulerStateMachine::IMMEDIATE_STATE_NONE));
}

TEST(CCSchedulerStateMachineTest, TestFullCycle)
{
    StateMachine state;

    // Start clean and set commit.
    state.setNeedsCommit(true);
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_BEGIN_FRAME, state.nextAction(CCSchedulerStateMachine::IMMEDIATE_STATE_NONE));

    // Begin the frame.
    state.updateState(CCSchedulerStateMachine::ACTION_BEGIN_FRAME);
    EXPECT_EQ(CCSchedulerStateMachine::COMMIT_STATE_FRAME_IN_PROGRESS, state.commitState());
    EXPECT_FALSE(state.needsCommit());
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_NONE, state.nextAction(CCSchedulerStateMachine::IMMEDIATE_STATE_NONE));

    // Tell the scheduler the frame finished.
    state.beginFrameComplete();
    EXPECT_EQ(CCSchedulerStateMachine::COMMIT_STATE_UPDATING_RESOURCES, state.commitState());
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_BEGIN_UPDATE_MORE_RESOURCES, state.nextAction(CCSchedulerStateMachine::IMMEDIATE_STATE_NONE));

    // Tell the scheduler the update began and finished
    state.updateState(CCSchedulerStateMachine::ACTION_BEGIN_UPDATE_MORE_RESOURCES);
    state.beginUpdateMoreResourcesComplete(false);
    EXPECT_EQ(CCSchedulerStateMachine::COMMIT_STATE_READY_TO_COMMIT, state.commitState());
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_COMMIT, state.nextAction(CCSchedulerStateMachine::IMMEDIATE_STATE_NONE));

    // Commit.
    state.updateState(CCSchedulerStateMachine::ACTION_COMMIT);
    EXPECT_EQ(CCSchedulerStateMachine::COMMIT_STATE_IDLE, state.commitState());
    EXPECT_TRUE(state.needsRedraw());

    // Expect to do nothing until vsync.
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_NONE, state.nextAction(CCSchedulerStateMachine::IMMEDIATE_STATE_NONE));

    // At vsync, draw.
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_DRAW, state.nextAction(CCSchedulerStateMachine::IMMEDIATE_STATE_INSIDE_VSYNC));
    state.updateState(CCSchedulerStateMachine::ACTION_DRAW);

    // Should be synchronized, no draw needed, no action needed.
    EXPECT_EQ(CCSchedulerStateMachine::COMMIT_STATE_IDLE, state.commitState());
    EXPECT_FALSE(state.needsRedraw());
    EXPECT_EQ(CCSchedulerStateMachine::ACTION_NONE, state.nextAction(CCSchedulerStateMachine::IMMEDIATE_STATE_NONE));
}

TEST(CCSchedulerStateMachineTest, TestFullCycleWithCommitRequestInbetween)
{
}

TEST(CCSchedulerStateMachineTest, TestFullCycleWithRedrawInbetween)
{
}

}
