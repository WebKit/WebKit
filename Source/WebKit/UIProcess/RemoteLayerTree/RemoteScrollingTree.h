/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#if ENABLE(UI_SIDE_COMPOSITING)

#include <WebCore/ScrollingConstraints.h>
#include <WebCore/ScrollingCoordinatorTypes.h>
#include <WebCore/ScrollingTree.h>
#include <WebCore/WheelEventTestMonitor.h>
#include <wtf/WeakPtr.h>

namespace WebCore {
class PlatformMouseEvent;
};

namespace WebKit {

class RemoteScrollingCoordinatorProxy;

class RemoteScrollingTree : public WebCore::ScrollingTree {
public:
    static Ref<RemoteScrollingTree> create(RemoteScrollingCoordinatorProxy&);
    virtual ~RemoteScrollingTree();

    bool isRemoteScrollingTree() const final { return true; }

    void invalidate() final;

    virtual void willSendEventForDefaultHandling(const WebCore::PlatformWheelEvent&) { }
    virtual void waitForEventDefaultHandlingCompletion(const WebCore::PlatformWheelEvent&) { }
    virtual void receivedEventAfterDefaultHandling(const WebCore::PlatformWheelEvent&, std::optional<WebCore::WheelScrollGestureState>) { };
    virtual WebCore::WheelEventHandlingResult handleWheelEventAfterDefaultHandling(const WebCore::PlatformWheelEvent&, WebCore::ScrollingNodeID, std::optional<WebCore::WheelScrollGestureState>) { return WebCore::WheelEventHandlingResult::unhandled(); }

    RemoteScrollingCoordinatorProxy* scrollingCoordinatorProxy() const;

    void scrollingTreeNodeDidScroll(WebCore::ScrollingTreeScrollingNode&, WebCore::ScrollingLayerPositionAction = WebCore::ScrollingLayerPositionAction::Sync) override;
    void scrollingTreeNodeDidStopAnimatedScroll(WebCore::ScrollingTreeScrollingNode&) override;
    bool scrollingTreeNodeRequestsScroll(WebCore::ScrollingNodeID, const WebCore::RequestedScrollData&) override;
    bool scrollingTreeNodeRequestsKeyboardScroll(WebCore::ScrollingNodeID, const WebCore::RequestedKeyboardScrollData&) override;

    void scrollingTreeNodeWillStartScroll(WebCore::ScrollingNodeID) override;
    void scrollingTreeNodeDidEndScroll(WebCore::ScrollingNodeID) override;
    void clearNodesWithUserScrollInProgress() override;

    void scrollingTreeNodeDidBeginScrollSnapping(WebCore::ScrollingNodeID) override;
    void scrollingTreeNodeDidEndScrollSnapping(WebCore::ScrollingNodeID) override;

    void currentSnapPointIndicesDidChange(WebCore::ScrollingNodeID, std::optional<unsigned> horizontal, std::optional<unsigned> vertical) override;
    void reportExposedUnfilledArea(MonotonicTime, unsigned unfilledArea) override;
    void reportSynchronousScrollingReasonsChanged(MonotonicTime, OptionSet<WebCore::SynchronousScrollingReason>) override;

    void tryToApplyLayerPositions();

protected:
    explicit RemoteScrollingTree(RemoteScrollingCoordinatorProxy&);

    Ref<WebCore::ScrollingTreeNode> createScrollingTreeNode(WebCore::ScrollingNodeType, WebCore::ScrollingNodeID) override;

    void receivedWheelEventWithPhases(WebCore::PlatformWheelEventPhase phase, WebCore::PlatformWheelEventPhase momentumPhase) override;
    void deferWheelEventTestCompletionForReason(WebCore::ScrollingNodeID, WebCore::WheelEventTestMonitor::DeferReason) override;
    void removeWheelEventTestCompletionDeferralForReason(WebCore::ScrollingNodeID, WebCore::WheelEventTestMonitor::DeferReason) override;
    void propagateSynchronousScrollingReasons(const HashSet<WebCore::ScrollingNodeID>&) WTF_REQUIRES_LOCK(m_treeLock) override;

    // This gets nulled out via invalidate(), since the scrolling thread can hold a ref to the ScrollingTree after the RemoteScrollingCoordinatorProxy has gone away.
    WeakPtr<RemoteScrollingCoordinatorProxy> m_scrollingCoordinatorProxy;
    bool m_hasNodesWithSynchronousScrollingReasons WTF_GUARDED_BY_LOCK(m_treeLock) { false };
};

class RemoteLayerTreeHitTestLocker {
public:
    RemoteLayerTreeHitTestLocker(RemoteScrollingTree& scrollingTree)
        : m_scrollingTree(scrollingTree)
    {
        m_scrollingTree->lockLayersForHitTesting();
    }
    
    ~RemoteLayerTreeHitTestLocker()
    {
        m_scrollingTree->unlockLayersForHitTesting();
    }

private:
    Ref<RemoteScrollingTree> m_scrollingTree;
};

} // namespace WebKit

SPECIALIZE_TYPE_TRAITS_SCROLLING_TREE(WebKit::RemoteScrollingTree, isRemoteScrollingTree());

#endif // ENABLE(UI_SIDE_COMPOSITING)
