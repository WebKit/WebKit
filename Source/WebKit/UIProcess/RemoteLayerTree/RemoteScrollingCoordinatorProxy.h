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
    
    constexpr bool isRemoteScrollingCoordinatorProxyIOS() const
    {
#if PLATFORM(IOS_FAMILY)
        return true;
#else
        return false;
#endif
    }

    constexpr bool isRemoteScrollingCoordinatorProxyMac() const
    {
#if PLATFORM(MAC)
        return true;
#else
        return false;
#endif
    }

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
    
    virtual WebCore::PlatformWheelEvent filteredWheelEvent(const WebCore::PlatformWheelEvent& wheelEvent) { return wheelEvent; }

    WebCore::ScrollingNodeID rootScrollingNodeID() const;

    const RemoteLayerTreeHost* layerTreeHost() const;
    WebPageProxy& webPageProxy() const { return m_webPageProxy; }

    std::optional<WebCore::RequestedScrollData> commitScrollingTreeState(const RemoteScrollingCoordinatorTransaction&);

    bool hasFixedOrSticky() const { return m_scrollingTree->hasFixedOrSticky(); }
    bool hasScrollableMainFrame() const;
    bool hasScrollableOrZoomedMainFrame() const;
    
    virtual bool propagatesMainFrameScrolls() const { return true; }

    virtual void scrollingTreeNodeWillStartPanGesture(WebCore::ScrollingNodeID) { }
    virtual void scrollingTreeNodeWillStartScroll(WebCore::ScrollingNodeID) { }
    virtual void scrollingTreeNodeDidEndScroll(WebCore::ScrollingNodeID) { }
    virtual void hasNodeWithAnimatedScrollChanged(bool) { }
    virtual void setRootNodeIsInUserScroll(bool) { }

    String scrollingTreeAsText() const;

    void resetStateAfterProcessExited();
    WebCore::ScrollingTreeScrollingNode* rootNode() const;

    virtual void displayDidRefresh(WebCore::PlatformDisplayID);
    void reportExposedUnfilledArea(MonotonicTime, unsigned unfilledArea);
    void reportSynchronousScrollingReasonsChanged(MonotonicTime, OptionSet<WebCore::SynchronousScrollingReason>);

protected:
    RemoteScrollingTree* scrollingTree() const { return m_scrollingTree.get(); }

    virtual void connectStateNodeLayers(WebCore::ScrollingStateTree&, const RemoteLayerTreeHost&) = 0;
    virtual void establishLayerTreeScrollingRelations(const RemoteLayerTreeHost&) = 0;

    virtual void didReceiveWheelEvent(bool /* wasHandled */) { }

    void sendUIStateChangedIfNecessary();
    void sendScrollingTreeNodeDidScroll();
    void receivedLastScrollingTreeNodeDidScrollReply();

private:
    WebPageProxy& m_webPageProxy;
    RefPtr<RemoteScrollingTree> m_scrollingTree;

protected:
    std::optional<WebCore::RequestedScrollData> m_requestedScroll;
    RemoteScrollingUIState m_uiState;
    std::optional<unsigned> m_currentHorizontalSnapPointIndex;
    std::optional<unsigned> m_currentVerticalSnapPointIndex;
    bool m_waitingForDidScrollReply { false };
    HashSet<WebCore::GraphicsLayer::PlatformLayerID> m_layersWithScrollingRelations;
};

} // namespace WebKit

#define SPECIALIZE_TYPE_TRAITS_REMOTE_SCROLLING_COORDINATOR_PROXY(ToValueTypeName, predicate) \
SPECIALIZE_TYPE_TRAITS_BEGIN(WebKit::ToValueTypeName) \
    static bool isType(const WebKit::RemoteScrollingCoordinatorProxy& scrollingCoordinatorProxy) { return scrollingCoordinatorProxy.predicate; } \
SPECIALIZE_TYPE_TRAITS_END()

#endif // ENABLE(UI_SIDE_COMPOSITING)
