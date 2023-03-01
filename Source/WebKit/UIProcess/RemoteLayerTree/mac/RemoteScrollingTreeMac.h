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

#include "RemoteScrollingTree.h"

#if PLATFORM(MAC) && ENABLE(UI_SIDE_COMPOSITING)

#include "RemoteScrollingCoordinator.h"
#include <WebCore/ScrollingConstraints.h>
#include <WebCore/ScrollingTree.h>

namespace WebCore {
class PlatformMouseEvent;
};

namespace WebKit {

class RemoteScrollingCoordinatorProxy;

class RemoteScrollingTreeMac final : public RemoteScrollingTree {
public:
    explicit RemoteScrollingTreeMac(RemoteScrollingCoordinatorProxy&);
    virtual ~RemoteScrollingTreeMac();

    void handleMouseEvent(const WebCore::PlatformMouseEvent&) final;

private:
    void handleWheelEventPhase(WebCore::ScrollingNodeID, WebCore::PlatformWheelEventPhase) override;
    RefPtr<WebCore::ScrollingTreeNode> scrollingNodeForPoint(WebCore::FloatPoint) override;
#if ENABLE(WHEEL_EVENT_REGIONS)
    OptionSet<WebCore::EventListenerRegionType> eventListenerRegionTypesForPoint(WebCore::FloatPoint) const override;
#endif

    void didCommitTree() override;

    void scrollingTreeNodeDidScroll(WebCore::ScrollingTreeScrollingNode&, WebCore::ScrollingLayerPositionAction) override;
    void scrollingTreeNodeDidStopAnimatedScroll(WebCore::ScrollingTreeScrollingNode&) override;
    bool scrollingTreeNodeRequestsScroll(WebCore::ScrollingNodeID, const WebCore::RequestedScrollData&) override;
    void currentSnapPointIndicesDidChange(WebCore::ScrollingNodeID, std::optional<unsigned> horizontal, std::optional<unsigned> vertical) override;
    void reportExposedUnfilledArea(MonotonicTime, unsigned unfilledArea) override;
    void reportSynchronousScrollingReasonsChanged(MonotonicTime, OptionSet<WebCore::SynchronousScrollingReason>) override;
    void receivedWheelEventWithPhases(WebCore::PlatformWheelEventPhase, WebCore::PlatformWheelEventPhase momentumPhase) override;
    void deferWheelEventTestCompletionForReason(WebCore::ScrollingNodeID, WebCore::WheelEventTestMonitor::DeferReason) override;
    void removeWheelEventTestCompletionDeferralForReason(WebCore::ScrollingNodeID, WebCore::WheelEventTestMonitor::DeferReason) override;

    void hasNodeWithAnimatedScrollChanged(bool) override;
    void displayDidRefresh(WebCore::PlatformDisplayID) override;
    
    void startPendingScrollAnimations() WTF_REQUIRES_LOCK(m_treeLock);

    Ref<WebCore::ScrollingTreeNode> createScrollingTreeNode(WebCore::ScrollingNodeType, WebCore::ScrollingNodeID) override;

    HashMap<WebCore::ScrollingNodeID, WebCore::RequestedScrollData> m_nodesWithPendingScrollAnimations; // Guarded by m_treeLock but used via call chains that can't be annotated.
};

} // namespace WebKit

#endif // PLATFORM(MAC) && ENABLE(UI_SIDE_COMPOSITING)
