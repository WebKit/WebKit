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

#if ENABLE(ASYNC_SCROLLING) && ENABLE(SCROLLING_THREAD)

#include "AsyncScrollingCoordinator.h"
#include "Logging.h"
#include "PlatformWheelEvent.h"
#include "ScrollingThread.h"
#include "ScrollingTreeFrameScrollingNode.h"
#include "ScrollingTreeNode.h"
#include "ScrollingTreeOverflowScrollProxyNode.h"
#include "ScrollingTreeScrollingNode.h"
#include <wtf/RunLoop.h>
#include <wtf/SetForScope.h>
#include <wtf/SystemTracing.h>
#include <wtf/text/TextStream.h>
#include <wtf/threads/BinarySemaphore.h>

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

ScrollingEventResult ThreadedScrollingTree::tryToHandleWheelEvent(const PlatformWheelEvent& wheelEvent, CompletionFunction&& completionFunction)
{
    if (shouldHandleWheelEventSynchronously(wheelEvent))
        return ScrollingEventResult::SendToMainThread;

    RefPtr<ThreadedScrollingTree> protectedThis(this);
    ScrollingThread::dispatch([protectedThis, wheelEvent, completionFunc = WTFMove(completionFunction)] {
        auto result = protectedThis->handleWheelEvent(wheelEvent);
        if (completionFunc)
            completionFunc(result);
    });
    
    return ScrollingEventResult::SendToScrollingThread;
}

ScrollingEventResult ThreadedScrollingTree::handleWheelEvent(const PlatformWheelEvent& wheelEvent)
{
    ASSERT(ScrollingThread::isCurrentThread());
    return ScrollingTree::handleWheelEvent(wheelEvent);
}

ScrollingEventResult ThreadedScrollingTree::handleWheelEventAfterMainThread(const PlatformWheelEvent& wheelEvent)
{
    SetForScope<bool> disallowLatchingScope(m_allowLatching, false);
    return handleWheelEvent(wheelEvent);
}

void ThreadedScrollingTree::invalidate()
{
    // Invalidate is dispatched by the ScrollingCoordinator class on the ScrollingThread
    // to break the reference cycle between ScrollingTree and ScrollingCoordinator when the
    // ScrollingCoordinator's page is destroyed.
    ASSERT(ScrollingThread::isCurrentThread());

    LockHolder treeLocker(m_treeMutex);
    m_delayedRenderingUpdateDetectionTimer = nullptr;

    // Since this can potentially be the last reference to the scrolling coordinator,
    // we need to release it on the main thread since it has member variables (such as timers)
    // that expect to be destroyed from the main thread.
    RunLoop::main().dispatch([scrollingCoordinator = WTFMove(m_scrollingCoordinator)] {
    });
}

void ThreadedScrollingTree::propagateSynchronousScrollingReasons(const HashSet<ScrollingNodeID>& synchronousScrollingNodes)
{
    auto propagateStateToAncestors = [&](ScrollingTreeNode& node) {
        ASSERT(is<ScrollingTreeScrollingNode>(node) && !downcast<ScrollingTreeScrollingNode>(node).synchronousScrollingReasons().isEmpty());

        auto currNode = node.parent();
        
        while (currNode) {
            if (is<ScrollingTreeScrollingNode>(currNode))
                downcast<ScrollingTreeScrollingNode>(*currNode).addSynchronousScrollingReason(SynchronousScrollingReason::DescendantScrollersHaveSynchronousScrolling);

            if (is<ScrollingTreeOverflowScrollProxyNode>(currNode)) {
                currNode = nodeForID(downcast<ScrollingTreeOverflowScrollProxyNode>(*currNode).overflowScrollingNodeID());
                continue;
            }

            currNode = currNode->parent();
        }
    };

    for (auto nodeID : synchronousScrollingNodes) {
        if (auto node = nodeForID(nodeID))
            propagateStateToAncestors(*node);
    }
}

void ThreadedScrollingTree::scrollingTreeNodeDidScroll(ScrollingTreeScrollingNode& node, ScrollingLayerPositionAction scrollingLayerPositionAction)
{
    if (!m_scrollingCoordinator)
        return;

    auto scrollPosition = node.currentScrollPosition();
    if (node.isRootNode())
        setMainFrameScrollPosition(scrollPosition);

    if (isHandlingProgrammaticScroll())
        return;

    Optional<FloatPoint> layoutViewportOrigin;
    if (is<ScrollingTreeFrameScrollingNode>(node))
        layoutViewportOrigin = downcast<ScrollingTreeFrameScrollingNode>(node).layoutViewport().location();

#if PLATFORM(MAC)
    if (isMonitoringWheelEvents())
        deferWheelEventTestCompletionForReason(reinterpret_cast<WheelEventTestMonitor::ScrollableAreaIdentifier>(node.scrollingNodeID()), WheelEventTestMonitor::ScrollingThreadSyncNeeded);
#endif

    LOG_WITH_STREAM(Scrolling, stream << "ThreadedScrollingTree::scrollingTreeNodeDidScroll " << node.scrollingNodeID() << " to " << scrollPosition << " bouncing to main thread");

    RunLoop::main().dispatch([strongThis = makeRef(*this), nodeID = node.scrollingNodeID(), scrollPosition, layoutViewportOrigin, scrollingLayerPositionAction] {
        if (auto* scrollingCoordinator = strongThis->m_scrollingCoordinator.get())
            scrollingCoordinator->scheduleUpdateScrollPositionAfterAsyncScroll(nodeID, scrollPosition, layoutViewportOrigin, scrollingLayerPositionAction);
    });
}

void ThreadedScrollingTree::reportSynchronousScrollingReasonsChanged(MonotonicTime timestamp, OptionSet<SynchronousScrollingReason> reasons)
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
void ThreadedScrollingTree::handleWheelEventPhase(ScrollingNodeID nodeID, PlatformWheelEventPhase phase)
{
    if (!m_scrollingCoordinator)
        return;

    RunLoop::main().dispatch([scrollingCoordinator = m_scrollingCoordinator, nodeID, phase] {
        scrollingCoordinator->handleWheelEventPhase(nodeID, phase);
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

void ThreadedScrollingTree::scrollingTreeNodeRequestsScroll(ScrollingNodeID nodeID, const FloatPoint& /*scrollPosition*/, ScrollType, ScrollClamping)
{
    removeWheelEventTestCompletionDeferralForReason(reinterpret_cast<WheelEventTestMonitor::ScrollableAreaIdentifier>(nodeID), WheelEventTestMonitor::RequestedScrollPosition);
}
#endif

void ThreadedScrollingTree::willStartRenderingUpdate()
{
    ASSERT(isMainThread());

    tracePoint(ScrollingThreadRenderUpdateSyncStart);

    // Wait for the scrolling thread to acquire m_treeMutex. This ensures that any pending wheel events are processed.
    BinarySemaphore semaphore;
    ScrollingThread::dispatch([protectedThis = makeRef(*this), &semaphore]() {
        LockHolder treeLocker(protectedThis->m_treeMutex);
        semaphore.signal();
        protectedThis->waitForRenderingUpdateCompletionOrTimeout();
    });
    semaphore.wait();
    m_state = SynchronizationState::InRenderingUpdate;
}

Seconds ThreadedScrollingTree::maxAllowableRenderingUpdateDurationForSynchronization()
{
    constexpr double allowableFrameFraction = 0.5;
    auto displayFPS = nominalFramesPerSecond().valueOr(60);
    Seconds frameDuration = 1_s / (double)displayFPS;
    return allowableFrameFraction * frameDuration;
}

// This code allows the main thread about half a frame to complete its rendering udpate. If the main thread
// is responsive (i.e. managing to render every frame), then we expect to get a didCompleteRenderingUpdate()
// within 8ms of willStartRenderingUpdate(). We time this via m_stateCondition, which blocks the scrolling
// thread (with m_treeMutex locked at the start and end) so that we don't handle wheel events while waiting.
// If the condition times out, we know the main thread is being slow, and allow the scrolling thread to
// commit layer positions.
void ThreadedScrollingTree::waitForRenderingUpdateCompletionOrTimeout()
{
    ASSERT(ScrollingThread::isCurrentThread());
    ASSERT(m_treeMutex.isLocked());

    if (m_delayedRenderingUpdateDetectionTimer)
        m_delayedRenderingUpdateDetectionTimer->stop();

    auto startTime = MonotonicTime::now();
    auto timeoutTime = startTime + maxAllowableRenderingUpdateDurationForSynchronization();

    bool becameIdle = m_stateCondition.waitUntil(m_treeMutex, timeoutTime, [&] {
        return m_state == SynchronizationState::Idle;
    });

    ASSERT(m_treeMutex.isLocked());

    if (!becameIdle) {
        m_state = SynchronizationState::Desynchronized;
        // At this point we know the main thread is taking too long in the rendering update,
        // so we give up trying to sync with the main thread and update layers here on the scrolling thread.
        applyLayerPositionsInternal();
        tracePoint(ScrollingThreadRenderUpdateSyncEnd, 1);
    } else
        tracePoint(ScrollingThreadRenderUpdateSyncEnd);
}

void ThreadedScrollingTree::didCompleteRenderingUpdate()
{
    ASSERT(isMainThread());
    LockHolder treeLocker(m_treeMutex);

    if (m_state == SynchronizationState::InRenderingUpdate)
        m_stateCondition.notifyOne();

    m_state = SynchronizationState::Idle;
}

void ThreadedScrollingTree::scheduleDelayedRenderingUpdateDetectionTimer(Seconds delay)
{
    ASSERT(ScrollingThread::isCurrentThread());
    ASSERT(m_treeMutex.isLocked());

    if (!m_delayedRenderingUpdateDetectionTimer)
        m_delayedRenderingUpdateDetectionTimer = makeUnique<RunLoop::Timer<ThreadedScrollingTree>>(RunLoop::current(), this, &ThreadedScrollingTree::delayedRenderingUpdateDetectionTimerFired);

    m_delayedRenderingUpdateDetectionTimer->startOneShot(delay);
}

void ThreadedScrollingTree::delayedRenderingUpdateDetectionTimerFired()
{
    ASSERT(ScrollingThread::isCurrentThread());

    LockHolder treeLocker(m_treeMutex);
    applyLayerPositionsInternal();
    m_state = SynchronizationState::Desynchronized;
}

void ThreadedScrollingTree::displayDidRefreshOnScrollingThread()
{
    TraceScope tracingScope(ScrollingThreadDisplayDidRefreshStart, ScrollingThreadDisplayDidRefreshEnd);
    ASSERT(ScrollingThread::isCurrentThread());

    LockHolder treeLocker(m_treeMutex);

    if (m_state != SynchronizationState::Idle)
        applyLayerPositionsInternal();

    switch (m_state) {
    case SynchronizationState::Idle: {
        m_state = SynchronizationState::WaitingForRenderingUpdate;
        constexpr auto maxStartRenderingUpdateDelay = 1_ms;
        scheduleDelayedRenderingUpdateDetectionTimer(maxStartRenderingUpdateDelay);
        break;
    }
    case SynchronizationState::WaitingForRenderingUpdate:
    case SynchronizationState::InRenderingUpdate:
    case SynchronizationState::Desynchronized:
        break;
    }
}

void ThreadedScrollingTree::displayDidRefresh(PlatformDisplayID displayID)
{
    if (displayID != this->displayID())
        return;

    // We're on the EventDispatcher thread here.

#if ENABLE(SCROLLING_THREAD)
    ScrollingThread::dispatch([protectedThis = makeRef(*this)]() {
        protectedThis->displayDidRefreshOnScrollingThread();
    });
#endif
}

} // namespace WebCore

#endif // ENABLE(ASYNC_SCROLLING) && ENABLE(SCROLLING_THREAD)
