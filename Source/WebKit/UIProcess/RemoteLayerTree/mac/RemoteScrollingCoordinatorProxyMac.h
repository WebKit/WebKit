/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#if PLATFORM(MAC) && ENABLE(UI_SIDE_COMPOSITING)

#include "RemoteLayerTreeEventDispatcher.h"
#include "RemoteScrollingCoordinatorProxy.h"
#include <wtf/TZoneMalloc.h>

namespace WebKit {

class RemoteLayerTreeEventDispatcher;

class RemoteScrollingCoordinatorProxyMac final : public RemoteScrollingCoordinatorProxy {
    WTF_MAKE_TZONE_ALLOCATED(RemoteScrollingCoordinatorProxyMac);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(RemoteScrollingCoordinatorProxyMac);
public:
    explicit RemoteScrollingCoordinatorProxyMac(WebPageProxy&);
    ~RemoteScrollingCoordinatorProxyMac();

private:
    void cacheWheelEventScrollingAccelerationCurve(const NativeWebWheelEvent&) override;

    void handleWheelEvent(const WebWheelEvent&, WebCore::RectEdges<WebCore::RubberBandingBehavior> rubberBandableEdges) override;
    void wheelEventHandlingCompleted(const WebCore::PlatformWheelEvent&, std::optional<WebCore::ScrollingNodeID>, std::optional<WebCore::WheelScrollGestureState>, bool wasHandled) override;

    bool scrollingTreeNodeRequestsScroll(WebCore::ScrollingNodeID, const WebCore::RequestedScrollData&) override;
    bool scrollingTreeNodeRequestsKeyboardScroll(WebCore::ScrollingNodeID, const WebCore::RequestedKeyboardScrollData&) override;
    void hasNodeWithAnimatedScrollChanged(bool) override;
    void setRubberBandingInProgressForNode(WebCore::ScrollingNodeID, bool isRubberBanding) override;

    void scrollingTreeNodeWillStartScroll(WebCore::ScrollingNodeID) override;
    void scrollingTreeNodeDidEndScroll(WebCore::ScrollingNodeID) override;
    void clearNodesWithUserScrollInProgress() override;

    void scrollingTreeNodeDidBeginScrollSnapping(WebCore::ScrollingNodeID) override;
    void scrollingTreeNodeDidEndScrollSnapping(WebCore::ScrollingNodeID) override;

    void connectStateNodeLayers(WebCore::ScrollingStateTree&, const RemoteLayerTreeHost&) override;
    void establishLayerTreeScrollingRelations(const RemoteLayerTreeHost&) override;

    void displayDidRefresh(WebCore::PlatformDisplayID) override;
    void windowScreenWillChange() override;
    void windowScreenDidChange(WebCore::PlatformDisplayID, std::optional<WebCore::FramesPerSecond>) override;

    void applyScrollingTreeLayerPositionsAfterCommit() override;

#if ENABLE(THREADED_ANIMATIONS)
    void willCommitLayerAndScrollingTrees() override WTF_ACQUIRES_LOCK(m_eventDispatcher->m_animationLock);
    void didCommitLayerAndScrollingTrees() override WTF_RELEASES_LOCK(m_eventDispatcher->m_animationLock);

    void animationsWereAddedToNode(RemoteLayerTreeNode&) override;
    void animationsWereRemovedFromNode(RemoteLayerTreeNode&) override;
    void updateTimelineRegistration(WebCore::ProcessIdentifier, const HashSet<Ref<WebCore::AcceleratedTimeline>>&, MonotonicTime) override;
    RefPtr<const RemoteAnimationTimeline> timeline(const TimelineID&) const override;
    RefPtr<const RemoteAnimationStack> animationStackForNodeWithIDForTesting(WebCore::PlatformLayerIdentifier) const override;
#else
    void willCommitLayerAndScrollingTrees() override;
    void didCommitLayerAndScrollingTrees() override;
#endif

    const Ref<RemoteLayerTreeEventDispatcher> m_eventDispatcher;
};

} // namespace WebKit

SPECIALIZE_TYPE_TRAITS_REMOTE_SCROLLING_COORDINATOR_PROXY(RemoteScrollingCoordinatorProxyMac, isRemoteScrollingCoordinatorProxyMac());

#endif // PLATFORM(MAC) && ENABLE(UI_SIDE_COMPOSITING)
