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
#include <wtf/Noncopyable.h>
#include <wtf/RefPtr.h>

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

class RemoteScrollingCoordinatorProxy {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(RemoteScrollingCoordinatorProxy);
public:
    explicit RemoteScrollingCoordinatorProxy(WebPageProxy&);
    virtual ~RemoteScrollingCoordinatorProxy();

    // Inform the web process that the scroll position changed (called from the scrolling tree)
    void scrollingTreeNodeDidScroll(WebCore::ScrollingNodeID, const WebCore::FloatPoint& newScrollPosition, const Optional<WebCore::FloatPoint>& layoutViewportOrigin, WebCore::ScrollingLayerPositionAction);
    void scrollingTreeNodeRequestsScroll(WebCore::ScrollingNodeID, const WebCore::FloatPoint& scrollPosition, WebCore::ScrollType, WebCore::ScrollClamping);

    WebCore::TrackingType eventTrackingTypeForPoint(const AtomString& eventName, WebCore::IntPoint) const;

    // Called externally when native views move around.
    void viewportChangedViaDelegatedScrolling(const WebCore::FloatPoint& scrollPosition, const WebCore::FloatRect& layoutViewport, double scale);

    void applyScrollingTreeLayerPositionsAfterCommit();

    void currentSnapPointIndicesDidChange(WebCore::ScrollingNodeID, unsigned horizontal, unsigned vertical);

    // FIXME: expose the tree and pass this to that?
    bool handleWheelEvent(const WebCore::PlatformWheelEvent&);
    void handleMouseEvent(const WebCore::PlatformMouseEvent&);

    WebCore::ScrollingNodeID rootScrollingNodeID() const;

    const RemoteLayerTreeHost* layerTreeHost() const;
    WebPageProxy& webPageProxy() const { return m_webPageProxy; }

    struct RequestedScrollInfo {
        bool requestsScrollPositionUpdate { };
        bool requestIsProgrammaticScroll { };
        WebCore::FloatPoint requestedScrollPosition;
    };
    void commitScrollingTreeState(const RemoteScrollingCoordinatorTransaction&, RequestedScrollInfo&);

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
#if ENABLE(CSS_SCROLL_SNAP)
    void adjustTargetContentOffsetForSnapping(CGSize maxScrollDimensions, CGPoint velocity, CGFloat topInset, CGPoint* targetContentOffset);
    bool hasActiveSnapPoint() const;
    CGPoint nearestActiveContentInsetAdjustedSnapPoint(CGFloat topInset, const CGPoint&) const;
    bool shouldSetScrollViewDecelerationRateFast() const;
#endif
#endif

    String scrollingTreeAsText() const;

    OptionSet<WebCore::TouchAction> activeTouchActionsForTouchIdentifier(unsigned touchIdentifier) const;
    void setTouchActionsForTouchIdentifier(OptionSet<WebCore::TouchAction>, unsigned);
    void clearTouchActionsForTouchIdentifier(unsigned);
    
    void resetStateAfterProcessExited();

private:
    void connectStateNodeLayers(WebCore::ScrollingStateTree&, const RemoteLayerTreeHost&);
    void establishLayerTreeScrollingRelations(const RemoteLayerTreeHost&);

#if ENABLE(CSS_SCROLL_SNAP)
    bool shouldSnapForMainFrameScrolling(WebCore::ScrollEventAxis) const;
    float closestSnapOffsetForMainFrameScrolling(WebCore::ScrollEventAxis, float scrollDestination, float velocity, unsigned& closestIndex) const;
#endif

    void sendUIStateChangedIfNecessary();

    WebPageProxy& m_webPageProxy;
    RefPtr<RemoteScrollingTree> m_scrollingTree;
    HashMap<unsigned, OptionSet<WebCore::TouchAction>> m_touchActionsByTouchIdentifier;
    RequestedScrollInfo* m_requestedScrollInfo { nullptr };
    RemoteScrollingUIState m_uiState;
#if ENABLE(CSS_SCROLL_SNAP)
    unsigned m_currentHorizontalSnapPointIndex { 0 };
    unsigned m_currentVerticalSnapPointIndex { 0 };
#endif
    bool m_propagatesMainFrameScrolls { true };
    HashSet<WebCore::GraphicsLayer::PlatformLayerID> m_layersWithScrollingRelations;
};

} // namespace WebKit

#endif // ENABLE(UI_SIDE_COMPOSITING)
