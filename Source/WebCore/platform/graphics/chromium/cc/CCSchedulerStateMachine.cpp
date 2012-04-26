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
    , m_needsForcedCommit(false)
    , m_mainThreadNeedsLayerTextures(false)
    , m_updateMoreResourcesPending(false)
    , m_insideVSync(false)
    , m_visible(false)
    , m_canBeginFrame(false)
    , m_canDraw(true)
    , m_drawIfPossibleFailed(false)
    , m_textureState(LAYER_TEXTURE_STATE_UNLOCKED)
    , m_contextState(CONTEXT_ACTIVE)
{
}

bool CCSchedulerStateMachine::hasDrawnThisFrame() const
{
    return m_currentFrameNumber == m_lastFrameNumberWhereDrawWasCalled;
}

bool CCSchedulerStateMachine::drawSuspendedUntilCommit() const
{
    if (!m_canDraw)
        return true;
    if (m_textureState == LAYER_TEXTURE_STATE_ACQUIRED_BY_MAIN_THREAD)
        return true;
    return false;
}

bool CCSchedulerStateMachine::scheduledToDraw() const
{
    if (!m_needsRedraw)
        return false;
    if (!m_visible)
        return false;
    if (drawSuspendedUntilCommit())
        return false;
    return true;
}

bool CCSchedulerStateMachine::shouldDraw() const
{
    if (m_needsForcedRedraw)
        return true;

    if (!scheduledToDraw())
        return false;
    if (!m_insideVSync)
        return false;
    if (hasDrawnThisFrame())
        return false;
    if (m_contextState != CONTEXT_ACTIVE)
        return false;
    return true;
}

bool CCSchedulerStateMachine::shouldAcquireLayerTexturesForMainThread() const
{
    if (!m_mainThreadNeedsLayerTextures)
        return false;
    if (m_textureState == LAYER_TEXTURE_STATE_UNLOCKED)
        return true;
    ASSERT(m_textureState == LAYER_TEXTURE_STATE_ACQUIRED_BY_IMPL_THREAD);
    // Transfer the lock from impl thread to main thread immediately if the
    // impl thread is not even scheduled to draw. Guards against deadlocking.
    if (!scheduledToDraw())
        return true;
    if (!vsyncCallbackNeeded())
        return true;
    return false;
}

CCSchedulerStateMachine::Action CCSchedulerStateMachine::nextAction() const
{
    if (shouldAcquireLayerTexturesForMainThread())
        return ACTION_ACQUIRE_LAYER_TEXTURES_FOR_MAIN_THREAD;
    switch (m_commitState) {
    case COMMIT_STATE_IDLE:
        if (m_contextState != CONTEXT_ACTIVE && m_needsForcedRedraw)
            return ACTION_DRAW_FORCED;
        if (m_contextState != CONTEXT_ACTIVE && m_needsForcedCommit)
            return ACTION_BEGIN_FRAME;
        if (m_contextState == CONTEXT_LOST)
            return ACTION_BEGIN_CONTEXT_RECREATION;
        if (m_contextState == CONTEXT_RECREATING)
            return ACTION_NONE;
        if (shouldDraw())
            return m_needsForcedRedraw ? ACTION_DRAW_FORCED : ACTION_DRAW_IF_POSSIBLE;
        if (m_needsCommit && ((m_visible && m_canBeginFrame) || m_needsForcedCommit))
            return ACTION_BEGIN_FRAME;
        return ACTION_NONE;

    case COMMIT_STATE_FRAME_IN_PROGRESS:
        if (shouldDraw())
            return m_needsForcedRedraw ? ACTION_DRAW_FORCED : ACTION_DRAW_IF_POSSIBLE;
        return ACTION_NONE;

    case COMMIT_STATE_UPDATING_RESOURCES:
        if (shouldDraw())
            return m_needsForcedRedraw ? ACTION_DRAW_FORCED : ACTION_DRAW_IF_POSSIBLE;
        if (!m_updateMoreResourcesPending)
            return ACTION_BEGIN_UPDATE_MORE_RESOURCES;
        return ACTION_NONE;

    case COMMIT_STATE_READY_TO_COMMIT:
        return ACTION_COMMIT;

    case COMMIT_STATE_WAITING_FOR_FIRST_DRAW:
        if (shouldDraw() || m_contextState == CONTEXT_LOST)
            return m_needsForcedRedraw ? ACTION_DRAW_FORCED : ACTION_DRAW_IF_POSSIBLE;
        // COMMIT_STATE_WAITING_FOR_FIRST_DRAW wants to enforce a draw. If m_canDraw is false
        // or textures are not available, proceed to the next step (similar as in COMMIT_STATE_IDLE).
        bool canCommit = m_visible || m_needsForcedCommit;
        if (m_needsCommit && canCommit && drawSuspendedUntilCommit())
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
        ASSERT(m_visible || m_needsForcedCommit);
        m_commitState = COMMIT_STATE_FRAME_IN_PROGRESS;
        m_needsCommit = false;
        m_needsForcedCommit = false;
        return;

    case ACTION_BEGIN_UPDATE_MORE_RESOURCES:
        ASSERT(m_commitState == COMMIT_STATE_UPDATING_RESOURCES);
        m_updateMoreResourcesPending = true;
        return;

    case ACTION_COMMIT:
        if ((m_needsCommit || !m_visible) && !m_needsForcedCommit)
            m_commitState = COMMIT_STATE_WAITING_FOR_FIRST_DRAW;
        else
            m_commitState = COMMIT_STATE_IDLE;
        m_needsRedraw = true;
        if (m_drawIfPossibleFailed)
            m_lastFrameNumberWhereDrawWasCalled = -1;
        m_textureState = LAYER_TEXTURE_STATE_ACQUIRED_BY_IMPL_THREAD;
        return;

    case ACTION_DRAW_FORCED:
    case ACTION_DRAW_IF_POSSIBLE:
        m_needsRedraw = false;
        m_needsForcedRedraw = false;
        m_drawIfPossibleFailed = false;
        if (m_insideVSync)
            m_lastFrameNumberWhereDrawWasCalled = m_currentFrameNumber;
        if (m_commitState == COMMIT_STATE_WAITING_FOR_FIRST_DRAW) {
            ASSERT(m_needsCommit || !m_visible);
            m_commitState = COMMIT_STATE_IDLE;
        }
        if (m_textureState == LAYER_TEXTURE_STATE_ACQUIRED_BY_IMPL_THREAD)
            m_textureState = LAYER_TEXTURE_STATE_UNLOCKED;
        return;

    case ACTION_BEGIN_CONTEXT_RECREATION:
        ASSERT(m_commitState == COMMIT_STATE_IDLE);
        ASSERT(m_contextState == CONTEXT_LOST);
        m_contextState = CONTEXT_RECREATING;
        return;

    case ACTION_ACQUIRE_LAYER_TEXTURES_FOR_MAIN_THREAD:
        m_textureState = LAYER_TEXTURE_STATE_ACQUIRED_BY_MAIN_THREAD;
        m_mainThreadNeedsLayerTextures = false;
        if (m_commitState != COMMIT_STATE_FRAME_IN_PROGRESS)
            m_needsCommit = true;
        return;
    }
}

void CCSchedulerStateMachine::setMainThreadNeedsLayerTextures()
{
    ASSERT(!m_mainThreadNeedsLayerTextures);
    ASSERT(m_textureState != LAYER_TEXTURE_STATE_ACQUIRED_BY_MAIN_THREAD);
    m_mainThreadNeedsLayerTextures = true;
}

bool CCSchedulerStateMachine::vsyncCallbackNeeded() const
{
    if (!m_visible || m_contextState != CONTEXT_ACTIVE) {
        if (m_needsForcedRedraw || m_commitState == COMMIT_STATE_UPDATING_RESOURCES)
            return true;

        return false;
    }

    return m_needsRedraw || m_needsForcedRedraw || m_commitState == COMMIT_STATE_UPDATING_RESOURCES;
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

void CCSchedulerStateMachine::didDrawIfPossibleCompleted(bool success)
{
    m_drawIfPossibleFailed = !success;
    if (m_drawIfPossibleFailed) {
        m_needsRedraw = true;
        m_needsCommit = true;
    }
}

void CCSchedulerStateMachine::setNeedsCommit()
{
    m_needsCommit = true;
}

void CCSchedulerStateMachine::setNeedsForcedCommit()
{
    m_needsForcedCommit = true;
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

void CCSchedulerStateMachine::didLoseContext()
{
    if (m_contextState == CONTEXT_LOST || m_contextState == CONTEXT_RECREATING)
        return;
    m_contextState = CONTEXT_LOST;
}

void CCSchedulerStateMachine::didRecreateContext()
{
    ASSERT(m_contextState == CONTEXT_RECREATING);
    m_contextState = CONTEXT_ACTIVE;
    setNeedsCommit();
}

}
