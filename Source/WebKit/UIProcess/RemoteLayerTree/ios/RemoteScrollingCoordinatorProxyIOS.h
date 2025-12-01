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

#if PLATFORM(IOS_FAMILY)

#include "RemoteScrollingCoordinatorProxy.h"
#include <wtf/TZoneMalloc.h>

#if ENABLE(THREADED_ANIMATIONS)
#import "RemoteMonotonicTimelineRegistry.h"
#endif

OBJC_CLASS UIScrollView;
OBJC_CLASS WKBaseScrollView;

namespace WebCore {
enum class TouchAction : uint8_t;
}

namespace WebKit {

class RemoteLayerTreeDrawingAreaProxyIOS;
class RemoteLayerTreeNode;

class RemoteScrollingCoordinatorProxyIOS final : public RemoteScrollingCoordinatorProxy {
    WTF_MAKE_TZONE_ALLOCATED(RemoteScrollingCoordinatorProxyIOS);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(RemoteScrollingCoordinatorProxyIOS);
public:
    explicit RemoteScrollingCoordinatorProxyIOS(WebPageProxy&);

    UIScrollView *scrollViewForScrollingNodeID(std::optional<WebCore::ScrollingNodeID>) const;

    OptionSet<WebCore::TouchAction> activeTouchActionsForTouchIdentifier(unsigned touchIdentifier) const;
    void setTouchActionsForTouchIdentifier(OptionSet<WebCore::TouchAction>, unsigned);
    void clearTouchActionsForTouchIdentifier(unsigned);

    bool shouldSetScrollViewDecelerationRateFast() const;
    void setRootNodeIsInUserScroll(bool) override;

    void adjustTargetContentOffsetForSnapping(CGSize maxScrollDimensions, CGPoint velocity, CGFloat topInset, CGPoint currentContentOffset, CGPoint* targetContentOffset);
    bool hasActiveSnapPoint() const;
    CGPoint nearestActiveContentInsetAdjustedSnapOffset(CGFloat topInset, const CGPoint&) const;

#if ENABLE(OVERLAY_REGIONS_IN_EVENT_REGION)
    void removeDestroyedLayerIDs(const Vector<WebCore::PlatformLayerIdentifier>&);
    HashSet<WebCore::PlatformLayerIdentifier> fixedScrollingNodeLayerIDs() const;
    using OverlayRegionCandidatesMap = HashMap<RetainPtr<WKBaseScrollView>, HashSet<WebCore::PlatformLayerIdentifier>>;
    OverlayRegionCandidatesMap overlayRegionCandidates() const;
#endif

#if ENABLE(THREADED_ANIMATIONS)
    void animationsWereAddedToNode(RemoteLayerTreeNode&) override WTF_IGNORES_THREAD_SAFETY_ANALYSIS;
    void animationsWereRemovedFromNode(RemoteLayerTreeNode&) override;
    void updateTimelineRegistration(WebCore::ProcessIdentifier, const HashSet<Ref<WebCore::AcceleratedTimeline>>&, MonotonicTime) override;
    RefPtr<const RemoteAnimationTimeline> timeline(const TimelineID&) const override;
    void progressBasedTimelinesWereUpdatedForNode(const WebCore::ScrollingTreeScrollingNode&) override;
    void updateAnimations();
#endif

    void displayDidRefresh(WebCore::PlatformDisplayID) override;

private:
    RemoteLayerTreeDrawingAreaProxyIOS& drawingAreaIOS() const;

    void scrollingTreeNodeWillStartPanGesture(WebCore::ScrollingNodeID) override;
    void scrollingTreeNodeWillStartScroll(WebCore::ScrollingNodeID) override;
    void scrollingTreeNodeDidEndScroll(WebCore::ScrollingNodeID) override;

    void connectStateNodeLayers(WebCore::ScrollingStateTree&, const RemoteLayerTreeHost&) override;
    void establishLayerTreeScrollingRelations(const RemoteLayerTreeHost&) override;

    WebCore::FloatRect currentLayoutViewport() const;

    bool shouldSnapForMainFrameScrolling(WebCore::ScrollEventAxis) const;
    std::pair<float, std::optional<unsigned>> closestSnapOffsetForMainFrameScrolling(WebCore::ScrollEventAxis, float currentScrollOffset, WebCore::FloatPoint scrollDestination, float velocity) const;

    HashMap<unsigned, OptionSet<WebCore::TouchAction>> m_touchActionsByTouchIdentifier;

#if ENABLE(OVERLAY_REGIONS_IN_EVENT_REGION)
    HashMap<WebCore::PlatformLayerIdentifier, WebCore::ScrollingNodeID> m_fixedScrollingNodesByLayerID;
    HashMap<WebCore::PlatformLayerIdentifier, WebCore::ScrollingNodeID> m_scrollingNodesByLayerID;
#endif

#if ENABLE(THREADED_ANIMATIONS)
    HashSet<WebCore::PlatformLayerIdentifier> m_animatedNodeLayerIDs;
    std::unique_ptr<RemoteMonotonicTimelineRegistry> m_monotonicTimelineRegistry;
#endif
};

} // namespace WebKit

SPECIALIZE_TYPE_TRAITS_REMOTE_SCROLLING_COORDINATOR_PROXY(RemoteScrollingCoordinatorProxyIOS, isRemoteScrollingCoordinatorProxyIOS());

#endif // PLATFORM(IOS_FAMILY)
