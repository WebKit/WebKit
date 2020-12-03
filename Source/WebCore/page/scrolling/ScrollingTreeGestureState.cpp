/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ScrollingTreeGestureState.h"

#if ENABLE(ASYNC_SCROLLING)

#include "PlatformWheelEvent.h"
#include "ScrollingTree.h"

namespace WebCore {


ScrollingTreeGestureState::ScrollingTreeGestureState(ScrollingTree& scrollingTree)
    : m_scrollingTree(scrollingTree)
{
}

void ScrollingTreeGestureState::receivedWheelEvent(const PlatformWheelEvent& event)
{
    if (event.isGestureStart()) {
        clearAllNodes();
        return;
    }
}

bool ScrollingTreeGestureState::handleGestureCancel(const PlatformWheelEvent& event)
{
    if (event.isGestureCancel()) {
        if (m_mayBeginNodeID)
            m_scrollingTree.handleWheelEventPhase(*m_mayBeginNodeID, PlatformWheelEventPhase::Cancelled);
        return true;
    }
    
    return false;
}

void ScrollingTreeGestureState::nodeDidHandleEvent(ScrollingNodeID nodeID, const PlatformWheelEvent& event)
{
    switch (event.phase()) {
    case PlatformWheelEventPhase::MayBegin:
        m_mayBeginNodeID = nodeID;
        m_scrollingTree.handleWheelEventPhase(nodeID, event.phase());
        break;
    case PlatformWheelEventPhase::Cancelled:
        // handleGestureCancel() should have been called first.
        ASSERT_NOT_REACHED();
        break;
    case PlatformWheelEventPhase::Began:
        m_activeNodeID = nodeID;
        m_scrollingTree.handleWheelEventPhase(nodeID, event.phase());
        break;
    case PlatformWheelEventPhase::Ended:
        if (m_activeNodeID)
            m_scrollingTree.handleWheelEventPhase(*m_activeNodeID, event.phase());
        break;
    case PlatformWheelEventPhase::Changed:
    case PlatformWheelEventPhase::Stationary:
    case PlatformWheelEventPhase::None:
        break;
    }

    switch (event.momentumPhase()) {
    case PlatformWheelEventPhase::MayBegin:
    case PlatformWheelEventPhase::Cancelled:
        ASSERT_NOT_REACHED();
        break;
    case PlatformWheelEventPhase::Began:
        m_activeNodeID = nodeID;
        m_scrollingTree.handleWheelEventPhase(nodeID, event.momentumPhase());
        break;
    case PlatformWheelEventPhase::Ended:
        if (m_activeNodeID)
            m_scrollingTree.handleWheelEventPhase(*m_activeNodeID, event.momentumPhase());
        break;
    case PlatformWheelEventPhase::Changed:
    case PlatformWheelEventPhase::Stationary:
    case PlatformWheelEventPhase::None:
        break;
    }
}

void ScrollingTreeGestureState::clearAllNodes()
{
    m_mayBeginNodeID = 0;
    m_activeNodeID = 0;
}

};

#endif // ENABLE(ASYNC_SCROLLING)
