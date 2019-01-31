/*
 * Copyright (C) 2014-2015 Apple Inc. All rights reserved.
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
#include "ThreadedScrollingTree.h"

#if ENABLE(ASYNC_SCROLLING)

#include "AsyncScrollingCoordinator.h"
#include "PlatformWheelEvent.h"
#include "ScrollingThread.h"
#include "ScrollingTreeNode.h"
#include "ScrollingTreeScrollingNode.h"
#include <wtf/RunLoop.h>

namespace WebCore {

ThreadedScrollingTree::ThreadedScrollingTree(AsyncScrollingCoordinator& scrollingCoordinator)
    : m_scrollingCoordinator(&scrollingCoordinator)
{
}

ThreadedScrollingTree::~ThreadedScrollingTree()
{
    // invalidate() should have cleared m_scrollingCoordinator.
    ASSERT(!m_scrollingCoordinator);
}

ScrollingEventResult ThreadedScrollingTree::tryToHandleWheelEvent(const PlatformWheelEvent& wheelEvent)
{
    if (shouldHandleWheelEventSynchronously(wheelEvent))
        return ScrollingEventResult::SendToMainThread;

    if (willWheelEventStartSwipeGesture(wheelEvent))
        return ScrollingEventResult::DidNotHandleEvent;

    RefPtr<ThreadedScrollingTree> protectedThis(this);
    ScrollingThread::dispatch([protectedThis, wheelEvent] {
        protectedThis->handleWheelEvent(wheelEvent);
    });
    
    return ScrollingEventResult::DidHandleEvent;
}

ScrollingEventResult ThreadedScrollingTree::handleWheelEvent(const PlatformWheelEvent& wheelEvent)
{
    ASSERT(ScrollingThread::isCurrentThread());
    return ScrollingTree::handleWheelEvent(wheelEvent);
}

void ThreadedScrollingTree::invalidate()
{
    // Invalidate is dispatched by the ScrollingCoordinator class on the ScrollingThread
    // to break the reference cycle between ScrollingTree and ScrollingCoordinator when the
    // ScrollingCoordinator's page is destroyed.
    ASSERT(ScrollingThread::isCurrentThread());

    // Since this can potentially be the last reference to the scrolling coordinator,
    // we need to release it on the main thread since it has member variables (such as timers)
    // that expect to be destroyed from the main thread.
    RunLoop::main().dispatch([scrollingCoordinator = WTFMove(m_scrollingCoordinator)] {
    });
}

void ThreadedScrollingTree::commitTreeState(std::unique_ptr<ScrollingStateTree> scrollingStateTree)
{
    ASSERT(ScrollingThread::isCurrentThread());
    ScrollingTree::commitTreeState(WTFMove(scrollingStateTree));
}

void ThreadedScrollingTree::scrollingTreeNodeDidScroll(ScrollingNodeID nodeID, const FloatPoint& scrollPosition, const Optional<FloatPoint>& layoutViewportOrigin, ScrollingLayerPositionAction scrollingLayerPositionAction)
{
    if (!m_scrollingCoordinator)
        return;

    if (nodeID == rootNode()->scrollingNodeID())
        setMainFrameScrollPosition(scrollPosition);

    RunLoop::main().dispatch([scrollingCoordinator = m_scrollingCoordinator, nodeID, scrollPosition, layoutViewportOrigin, localIsHandlingProgrammaticScroll = isHandlingProgrammaticScroll(), scrollingLayerPositionAction] {
        scrollingCoordinator->scheduleUpdateScrollPositionAfterAsyncScroll(nodeID, scrollPosition, layoutViewportOrigin, localIsHandlingProgrammaticScroll, scrollingLayerPositionAction);
    });
}

void ThreadedScrollingTree::reportSynchronousScrollingReasonsChanged(MonotonicTime timestamp, SynchronousScrollingReasons reasons)
{
    RunLoop::main().dispatch([scrollingCoordinator = m_scrollingCoordinator, timestamp, reasons] {
        scrollingCoordinator->reportSynchronousScrollingReasonsChanged(timestamp, reasons);
    });
}

void ThreadedScrollingTree::reportExposedUnfilledArea(MonotonicTime timestamp, unsigned unfilledArea)
{
    RunLoop::main().dispatch([scrollingCoordinator = m_scrollingCoordinator, timestamp, unfilledArea] {
        scrollingCoordinator->reportExposedUnfilledArea(timestamp, unfilledArea);
    });
}

#if PLATFORM(COCOA)
void ThreadedScrollingTree::currentSnapPointIndicesDidChange(ScrollingNodeID nodeID, unsigned horizontal, unsigned vertical)
{
    if (!m_scrollingCoordinator)
        return;

    RunLoop::main().dispatch([scrollingCoordinator = m_scrollingCoordinator, nodeID, horizontal, vertical] {
        scrollingCoordinator->setActiveScrollSnapIndices(nodeID, horizontal, vertical);
    });
}
#endif

#if PLATFORM(MAC)
void ThreadedScrollingTree::handleWheelEventPhase(PlatformWheelEventPhase phase)
{
    if (!m_scrollingCoordinator)
        return;

    RunLoop::main().dispatch([scrollingCoordinator = m_scrollingCoordinator, phase] {
        scrollingCoordinator->handleWheelEventPhase(phase);
    });
}

void ThreadedScrollingTree::setActiveScrollSnapIndices(ScrollingNodeID nodeID, unsigned horizontalIndex, unsigned verticalIndex)
{
    if (!m_scrollingCoordinator)
        return;
    
    RunLoop::main().dispatch([scrollingCoordinator = m_scrollingCoordinator, nodeID, horizontalIndex, verticalIndex] {
        scrollingCoordinator->setActiveScrollSnapIndices(nodeID, horizontalIndex, verticalIndex);
    });
}

void ThreadedScrollingTree::deferTestsForReason(WheelEventTestTrigger::ScrollableAreaIdentifier identifier, WheelEventTestTrigger::DeferTestTriggerReason reason)
{
    if (!m_scrollingCoordinator)
        return;

    RunLoop::main().dispatch([scrollingCoordinator = m_scrollingCoordinator, identifier, reason] {
        scrollingCoordinator->deferTestsForReason(identifier, reason);
    });
}

void ThreadedScrollingTree::removeTestDeferralForReason(WheelEventTestTrigger::ScrollableAreaIdentifier identifier, WheelEventTestTrigger::DeferTestTriggerReason reason)
{
    if (!m_scrollingCoordinator)
        return;
    
    RunLoop::main().dispatch([scrollingCoordinator = m_scrollingCoordinator, identifier, reason] {
        scrollingCoordinator->removeTestDeferralForReason(identifier, reason);
    });
}

#endif

} // namespace WebCore

#endif // ENABLE(ASYNC_SCROLLING)
