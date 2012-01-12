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

namespace WebCore {

CCSchedulerStateMachine::CCSchedulerStateMachine()
    : m_commitState(COMMIT_STATE_IDLE)
    , m_currentFrameNumber(0)
    , m_lastFrameNumberWhereDrawWasCalled(-1)
    , m_needsRedraw(false)
    , m_needsForcedRedraw(false)
    , m_needsCommit(false)
    , m_updateMoreResourcesPending(false)
    , m_insideVSync(false)
    , m_visible(false)
    , m_canDraw(true) { }

CCSchedulerStateMachine::Action CCSchedulerStateMachine::nextAction() const
{
    bool canDraw = m_currentFrameNumber != m_lastFrameNumberWhereDrawWasCalled;
    bool shouldDraw = (m_needsRedraw && m_insideVSync && m_visible && canDraw && m_canDraw) || m_needsForcedRedraw;
    switch (m_commitState) {
    case COMMIT_STATE_IDLE:
        if (shouldDraw)
            return ACTION_DRAW;
        if (m_needsCommit && m_visible)
            return ACTION_BEGIN_FRAME;
        return ACTION_NONE;

    case COMMIT_STATE_FRAME_IN_PROGRESS:
        if (shouldDraw)
            return ACTION_DRAW;
        return ACTION_NONE;

    case COMMIT_STATE_UPDATING_RESOURCES:
        if (shouldDraw)
            return ACTION_DRAW;
        if (!m_updateMoreResourcesPending)
            return ACTION_BEGIN_UPDATE_MORE_RESOURCES;
        return ACTION_NONE;

    case COMMIT_STATE_READY_TO_COMMIT:
        return ACTION_COMMIT;

    case COMMIT_STATE_WAITING_FOR_FIRST_DRAW:
        if (shouldDraw)
            return ACTION_DRAW;
        // COMMIT_STATE_WAITING_FOR_FIRST_DRAW wants to enforce a draw. If m_canDraw is false,
        // proceed to the next step (similar as in COMMIT_STATE_IDLE).
        if (!m_canDraw && m_needsCommit && m_visible)
            return ACTION_BEGIN_FRAME;
        return ACTION_NONE;
    }
    ASSERT_NOT_REACHED();
    return ACTION_NONE;
}

void CCSchedulerStateMachine::updateState(Action action)
{
    switch (action) {
    case ACTION_NONE:
        return;

    case ACTION_BEGIN_FRAME:
        ASSERT(m_visible);
        m_commitState = COMMIT_STATE_FRAME_IN_PROGRESS;
        m_needsCommit = false;
        return;

    case ACTION_BEGIN_UPDATE_MORE_RESOURCES:
        ASSERT(m_commitState == COMMIT_STATE_UPDATING_RESOURCES);
        m_updateMoreResourcesPending = true;
        return;

    case ACTION_COMMIT:
        if (m_needsCommit || !m_visible)
            m_commitState = COMMIT_STATE_WAITING_FOR_FIRST_DRAW;
        else
            m_commitState = COMMIT_STATE_IDLE;
        m_needsRedraw = true;
        return;

    case ACTION_DRAW:
        m_needsRedraw = false;
        m_needsForcedRedraw = false;
        if (m_insideVSync)
            m_lastFrameNumberWhereDrawWasCalled = m_currentFrameNumber;
        if (m_commitState == COMMIT_STATE_WAITING_FOR_FIRST_DRAW) {
            ASSERT(m_needsCommit || !m_visible);
            m_commitState = COMMIT_STATE_IDLE;
        }
        return;
    }
}

bool CCSchedulerStateMachine::vsyncCallbackNeeded() const
{
    if (!m_visible) {
        if (m_needsForcedRedraw)
            return true;

        return false;
    }

    return m_needsRedraw || m_needsForcedRedraw || m_updateMoreResourcesPending;
}

void CCSchedulerStateMachine::didEnterVSync()
{
    m_insideVSync = true;
}

void CCSchedulerStateMachine::didLeaveVSync()
{
    m_currentFrameNumber++;
    m_insideVSync = false;
}

void CCSchedulerStateMachine::setVisible(bool visible)
{
    m_visible = visible;
}

void CCSchedulerStateMachine::setNeedsRedraw()
{
    m_needsRedraw = true;
}

void CCSchedulerStateMachine::setNeedsForcedRedraw()
{
    m_needsForcedRedraw = true;
}

void CCSchedulerStateMachine::setNeedsCommit()
{
    m_needsCommit = true;
}

void CCSchedulerStateMachine::beginFrameComplete()
{
    ASSERT(m_commitState == COMMIT_STATE_FRAME_IN_PROGRESS);
    m_commitState = COMMIT_STATE_UPDATING_RESOURCES;
}

void CCSchedulerStateMachine::beginUpdateMoreResourcesComplete(bool morePending)
{
    ASSERT(m_commitState == COMMIT_STATE_UPDATING_RESOURCES);
    ASSERT(m_updateMoreResourcesPending);
    m_updateMoreResourcesPending = false;
    if (!morePending)
        m_commitState = COMMIT_STATE_READY_TO_COMMIT;
}

}
