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

#pragma once

#if ENABLE(UI_SIDE_COMPOSITING)

#include "MessageReceiver.h"
#include "RemoteScrollingCoordinator.h"
#include "RemoteScrollingTree.h"
#include "RemoteScrollingUIState.h"
#include <WebCore/GraphicsLayer.h>
#include <WebCore/ScrollSnapOffsetsInfo.h>
#include <wtf/Noncopyable.h>
#include <wtf/RefPtr.h>
#include <wtf/WeakPtr.h>

OBJC_CLASS UIScrollView;

namespace WebCore {
class FloatPoint;
class PlatformWheelEvent;
}

namespace WebKit {

class RemoteLayerTreeHost;
class RemoteScrollingCoordinatorTransaction;
class RemoteScrollingTree;
class WebPageProxy;

class RemoteScrollingCoordinatorProxy : public CanMakeWeakPtr<RemoteScrollingCoordinatorProxy> {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(RemoteScrollingCoordinatorProxy);
public:
    explicit RemoteScrollingCoordinatorProxy(WebPageProxy&);
    virtual ~RemoteScrollingCoordinatorProxy();

    // Inform the web process that the scroll position changed (called from the scrolling tree)
    void scrollingTreeNodeDidScroll(WebCore::ScrollingNodeID, const WebCore::FloatPoint& newScrollPosition, const std::optional<WebCore::FloatPoint>& layoutViewportOrigin, WebCore::ScrollingLayerPositionAction);
    virtual bool scrollingTreeNodeRequestsScroll(WebCore::ScrollingNodeID, const WebCore::RequestedScrollData&);
    void scrollingTreeNodeDidStopAnimatedScroll(WebCore::ScrollingNodeID);

    WebCore::TrackingType eventTrackingTypeForPoint(WebCore::EventTrackingRegions::EventType, WebCore::IntPoint) const;

    // Called externally when native views move around.
    void viewportChangedViaDelegatedScrolling(const WebCore::FloatPoint& scrollPosition, const WebCore::FloatRect& layoutViewport, double scale);

    void applyScrollingTreeLayerPositionsAfterCommit();

    void currentSnapPointIndicesDidChange(WebCore::ScrollingNodeID, std::optional<unsigned> horizontal, std::optional<unsigned> vertical);

    // FIXME: expose the tree and pass this to that?
    bool handleWheelEvent(const WebCore::PlatformWheelEvent&);
    void handleMouseEvent(const WebCore::PlatformMouseEvent&);

    WebCore::ScrollingNodeID rootScrollingNodeID() const;

    const RemoteLayerTreeHost* layerTreeHost() const;
    WebPageProxy& webPageProxy() const { return m_webPageProxy; }

    std::optional<WebCore::RequestedScrollData> commitScrollingTreeState(const RemoteScrollingCoordinatorTransaction&);

    void setPropagatesMainFrameScrolls(bool propagatesMainFrameScrolls) { m_propagatesMainFrameScrolls = propagatesMainFrameScrolls; }
    bool propagatesMainFrameScrolls() const { return m_propagatesMainFrameScrolls; }
    bool hasFixedOrSticky() const { return m_scrollingTree->hasFixedOrSticky(); }
    bool hasScrollableMainFrame() const;
    bool hasScrollableOrZoomedMainFrame() const;

#if PLATFORM(IOS_FAMILY)
    UIScrollView *scrollViewForScrollingNodeID(WebCore::ScrollingNodeID) const;

    WebCore::FloatRect currentLayoutViewport() const;
    void scrollingTreeNodeWillStartPanGesture(WebCore::ScrollingNodeID);
    void scrollingTreeNodeWillStartScroll(WebCore::ScrollingNodeID);
    void scrollingTreeNodeDidEndScroll(WebCore::ScrollingNodeID);
    void adjustTargetContentOffsetForSnapping(CGSize maxScrollDimensions, CGPoint velocity, CGFloat topInset, CGPoint currentContentOffset, CGPoint* targetContentOffset);
    bool hasActiveSnapPoint() const;
    CGPoint nearestActiveContentInsetAdjustedSnapOffset(CGFloat topInset, const CGPoint&) const;
    bool shouldSetScrollViewDecelerationRateFast() const;
    void setRootNodeIsInUserScroll(bool);
#endif

    String scrollingTreeAsText() const;

    OptionSet<WebCore::TouchAction> activeTouchActionsForTouchIdentifier(unsigned touchIdentifier) const;
    void setTouchActionsForTouchIdentifier(OptionSet<WebCore::TouchAction>, unsigned);
    void clearTouchActionsForTouchIdentifier(unsigned);
    
    void resetStateAfterProcessExited();
    WebCore::ScrollingTreeScrollingNode* rootNode() const;

#if ENABLE(OVERLAY_REGIONS_IN_EVENT_REGION)
    void removeFixedScrollingNodeLayerIDs(const Vector<WebCore::GraphicsLayer::PlatformLayerID>&);
    const HashSet<WebCore::GraphicsLayer::PlatformLayerID>& fixedScrollingNodeLayerIDs() const { return m_fixedScrollingNodeLayerIDs; }
#endif

protected:
    RemoteScrollingTree* scrollingTree() const { return m_scrollingTree.get(); }

private:
    void connectStateNodeLayers(WebCore::ScrollingStateTree&, const RemoteLayerTreeHost&);
    void establishLayerTreeScrollingRelations(const RemoteLayerTreeHost&);

    bool shouldSnapForMainFrameScrolling(WebCore::ScrollEventAxis) const;
    std::pair<float, std::optional<unsigned>> closestSnapOffsetForMainFrameScrolling(WebCore::ScrollEventAxis, float currentScrollOffset, WebCore::FloatPoint scrollDestination, float velocity) const;

    virtual void didReceiveWheelEvent(bool /* wasHandled */) { }

    void sendUIStateChangedIfNecessary();
    void sendScrollingTreeNodeDidScroll();
    void receivedLastScrollingTreeNodeDidScrollReply();

    WebPageProxy& m_webPageProxy;
    RefPtr<RemoteScrollingTree> m_scrollingTree;
    HashMap<unsigned, OptionSet<WebCore::TouchAction>> m_touchActionsByTouchIdentifier;
    std::optional<WebCore::RequestedScrollData> m_requestedScroll;
    RemoteScrollingUIState m_uiState;
    std::optional<unsigned> m_currentHorizontalSnapPointIndex;
    std::optional<unsigned> m_currentVerticalSnapPointIndex;
    bool m_propagatesMainFrameScrolls { true };
    bool m_waitingForDidScrollReply { false };
    HashSet<WebCore::GraphicsLayer::PlatformLayerID> m_layersWithScrollingRelations;
#if ENABLE(OVERLAY_REGIONS_IN_EVENT_REGION)
    HashSet<WebCore::GraphicsLayer::PlatformLayerID> m_fixedScrollingNodeLayerIDs;
#endif
};

} // namespace WebKit

#endif // ENABLE(UI_SIDE_COMPOSITING)
