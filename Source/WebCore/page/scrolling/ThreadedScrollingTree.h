/*
 * Copyright (C) 2014-2017 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(ASYNC_SCROLLING)

#include "ScrollingStateTree.h"
#include "ScrollingTree.h"
#include <wtf/Condition.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class AsyncScrollingCoordinator;

// The ThreadedScrollingTree class lives almost exclusively on the scrolling thread and manages the
// hierarchy of scrollable regions on the page. It's also responsible for dispatching events
// to the correct scrolling tree nodes or dispatching events back to the ScrollingCoordinator
// object on the main thread if they can't be handled on the scrolling thread for various reasons.
class ThreadedScrollingTree : public ScrollingTree {
public:
    virtual ~ThreadedScrollingTree();

    void commitTreeState(std::unique_ptr<ScrollingStateTree>) override;

    ScrollingEventResult handleWheelEvent(const PlatformWheelEvent&) override;

    // Can be called from any thread. Will try to handle the wheel event on the scrolling thread.
    // Returns true if the wheel event can be handled on the scrolling thread and false if the
    // event must be sent again to the WebCore event handler.
    ScrollingEventResult tryToHandleWheelEvent(const PlatformWheelEvent&) override;

    void invalidate() override;

    void incrementPendingCommitCount();
    void decrementPendingCommitCount();

protected:
    explicit ThreadedScrollingTree(AsyncScrollingCoordinator&);

    void scrollingTreeNodeDidScroll(ScrollingTreeScrollingNode&, ScrollingLayerPositionAction = ScrollingLayerPositionAction::Sync) override;
#if PLATFORM(MAC)
    void handleWheelEventPhase(PlatformWheelEventPhase) override;
    void setActiveScrollSnapIndices(ScrollingNodeID, unsigned horizontalIndex, unsigned verticalIndex) override;
    void deferTestsForReason(WheelEventTestTrigger::ScrollableAreaIdentifier, WheelEventTestTrigger::DeferTestTriggerReason) override;
    void removeTestDeferralForReason(WheelEventTestTrigger::ScrollableAreaIdentifier, WheelEventTestTrigger::DeferTestTriggerReason) override;
#endif

#if PLATFORM(COCOA)
    void currentSnapPointIndicesDidChange(ScrollingNodeID, unsigned horizontal, unsigned vertical) override;
#endif

    void reportExposedUnfilledArea(MonotonicTime, unsigned unfilledArea) override;
    void reportSynchronousScrollingReasonsChanged(MonotonicTime, SynchronousScrollingReasons) override;

private:
    bool isThreadedScrollingTree() const override { return true; }
    void applyLayerPositions() override;

    RefPtr<AsyncScrollingCoordinator> m_scrollingCoordinator;

    void waitForPendingCommits();

    Lock m_pendingCommitCountMutex;
    unsigned m_pendingCommitCount { 0 };
    Condition m_commitCondition;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_SCROLLING_TREE(WebCore::ThreadedScrollingTree, isThreadedScrollingTree())

#endif // ENABLE(ASYNC_SCROLLING)
