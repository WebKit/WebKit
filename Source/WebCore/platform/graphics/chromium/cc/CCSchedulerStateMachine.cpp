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
    , m_needsRedraw(false)
    , m_needsCommit(false)
    , m_updatedThisFrame(false) { }

CCSchedulerStateMachine::CCSchedulerStateMachine(const CCSchedulerStateMachine& that)
{
    *this = that;
}

CCSchedulerStateMachine::CCSchedulerStateMachine& CCSchedulerStateMachine::operator=(const CCSchedulerStateMachine& that)
{
    m_commitState = that.m_commitState;
    m_needsRedraw = that.m_needsRedraw;
    m_needsCommit = that.m_needsCommit;
    m_updatedThisFrame = that.m_updatedThisFrame;
    return *this;
}

CCSchedulerStateMachine::Action CCSchedulerStateMachine::nextAction(bool insideVSyncTick) const
{
    switch (m_commitState) {
    case COMMIT_STATE_IDLE:
        if (m_needsRedraw && insideVSyncTick)
            return ACTION_DRAW;
        if (m_needsCommit)
            return ACTION_BEGIN_FRAME;
        return ACTION_NONE;

    case COMMIT_STATE_FRAME_IN_PROGRESS:
        if (m_needsRedraw && insideVSyncTick)
            return ACTION_DRAW;
        return ACTION_NONE;

    case COMMIT_STATE_UPDATING_RESOURCES:
        if (m_needsRedraw && insideVSyncTick)
            return ACTION_DRAW;
        if (!m_updatedThisFrame)
            return ACTION_UPDATE_MORE_RESOURCES;
        return ACTION_NONE;

    case COMMIT_STATE_READY_TO_COMMIT:
        return ACTION_COMMIT;
    }
}

void CCSchedulerStateMachine::updateState(Action action)
{
    switch (action) {
    case ACTION_NONE:
        return;

    case ACTION_BEGIN_FRAME:
        m_commitState = COMMIT_STATE_FRAME_IN_PROGRESS;
        m_needsCommit = false;
        return;

    case ACTION_UPDATE_MORE_RESOURCES:
        ASSERT(m_commitState == COMMIT_STATE_UPDATING_RESOURCES);
        // getNextState() in COMMIT_STATE_UPDATING_RESOURCES will only return
        // ACTION_UPDATE_MORE_RESOURCES when m_updatedThisFrame is true. Here,
        // set m_updatedThisFrame to true to prevent additional
        // ACTION_UPDATE_MORE_RESOURCES from being issued.
        // This will then be cleared by a draw action, effectively limiting us
        // to one ACTION_UPDATE_MORE_RESOURCES per frame.
        m_updatedThisFrame = true;
        return;

    case ACTION_COMMIT:
        m_commitState = COMMIT_STATE_IDLE;
        m_needsRedraw = true;
        return;

    case ACTION_DRAW:
        m_updatedThisFrame = false;
        m_needsRedraw = false;
        return;
    }
}

void CCSchedulerStateMachine::setNeedsRedraw()
{
    m_needsRedraw = true;
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

void CCSchedulerStateMachine::updateResourcesComplete()
{
    ASSERT(m_commitState == COMMIT_STATE_UPDATING_RESOURCES);
    m_commitState = COMMIT_STATE_READY_TO_COMMIT;
}

}
