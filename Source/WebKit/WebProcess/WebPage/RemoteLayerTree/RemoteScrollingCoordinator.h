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

#if ENABLE(ASYNC_SCROLLING)

#include "MessageReceiver.h"
#include <WebCore/AsyncScrollingCoordinator.h>
#include <wtf/TZoneMalloc.h>

namespace IPC {
class Decoder;
class Encoder;
}

namespace WebKit {

class WebPage;
class RemoteScrollingCoordinatorTransaction;
class RemoteScrollingUIState;

class RemoteScrollingCoordinator final : public WebCore::AsyncScrollingCoordinator, public IPC::MessageReceiver {
    WTF_MAKE_TZONE_ALLOCATED(RemoteScrollingCoordinator);
public:
    static Ref<RemoteScrollingCoordinator> create(WebPage* page)
    {
        return adoptRef(*new RemoteScrollingCoordinator(page));
    }

    void ref() const final { WebCore::AsyncScrollingCoordinator::ref(); }
    void deref() const final { WebCore::AsyncScrollingCoordinator::deref(); }

    RemoteScrollingCoordinatorTransaction buildTransaction(WebCore::FrameIdentifier);

    void scrollingStateInUIProcessChanged(const RemoteScrollingUIState&);

    void addNodeWithActiveRubberBanding(WebCore::ScrollingNodeID);
    void removeNodeWithActiveRubberBanding(WebCore::ScrollingNodeID);
    
    void setCurrentWheelEventWillStartSwipe(std::optional<bool> value) { m_currentWheelEventWillStartSwipe = value; }

    struct NodeAndGestureState {
        std::optional<WebCore::ScrollingNodeID> wheelGestureNode;
        std::optional<WebCore::WheelScrollGestureState> wheelGestureState;
    };

    NodeAndGestureState takeCurrentWheelGestureInfo() { return std::exchange(m_currentWheelGestureInfo, { }); }

private:
    RemoteScrollingCoordinator(WebPage*);
    virtual ~RemoteScrollingCoordinator();

    bool isRemoteScrollingCoordinator() const override { return true; }
    
    // ScrollingCoordinator
    bool coordinatesScrollingForFrameView(const WebCore::LocalFrameView&) const override;
    void scheduleTreeStateCommit() override;

    bool isRubberBandInProgress(std::optional<WebCore::ScrollingNodeID>) const final;
    bool isUserScrollInProgress(std::optional<WebCore::ScrollingNodeID>) const final;
    bool isScrollSnapInProgress(std::optional<WebCore::ScrollingNodeID>) const final;

    void setScrollPinningBehavior(WebCore::ScrollPinningBehavior) override;
    
    void startMonitoringWheelEvents(bool clearLatchingState) final;

    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;
    
    // Respond to UI process changes.
    void scrollPositionChangedForNode(WebCore::ScrollingNodeID, const WebCore::FloatPoint& scrollPosition, std::optional<WebCore::FloatPoint> layoutViewportOrigin, bool syncLayerPosition, CompletionHandler<void()>&&);
    void animatedScrollDidEndForNode(WebCore::ScrollingNodeID);
    void currentSnapPointIndicesChangedForNode(WebCore::ScrollingNodeID, std::optional<unsigned> horizontal, std::optional<unsigned> vertical);

    void receivedWheelEventWithPhases(WebCore::PlatformWheelEventPhase phase, WebCore::PlatformWheelEventPhase momentumPhase);
    void startDeferringScrollingTestCompletionForNode(WebCore::ScrollingNodeID, OptionSet<WebCore::WheelEventTestMonitor::DeferReason>);
    void stopDeferringScrollingTestCompletionForNode(WebCore::ScrollingNodeID, OptionSet<WebCore::WheelEventTestMonitor::DeferReason>);
    void scrollingTreeNodeScrollbarVisibilityDidChange(WebCore::ScrollingNodeID, WebCore::ScrollbarOrientation, bool);
    void scrollingTreeNodeScrollbarMinimumThumbLengthDidChange(WebCore::ScrollingNodeID nodeID, WebCore::ScrollbarOrientation orientation, int minimumThumbLength);

    WebCore::WheelEventHandlingResult handleWheelEventForScrolling(const WebCore::PlatformWheelEvent&, WebCore::ScrollingNodeID, std::optional<WebCore::WheelScrollGestureState>) override;

    WeakPtr<WebPage> m_webPage;

    HashSet<WebCore::ScrollingNodeID> m_nodesWithActiveRubberBanding;
    HashSet<WebCore::ScrollingNodeID> m_nodesWithActiveScrollSnap;
    HashSet<WebCore::ScrollingNodeID> m_nodesWithActiveUserScrolls;

    NodeAndGestureState m_currentWheelGestureInfo;

    bool m_clearScrollLatchingInNextTransaction { false };
    
    std::optional<bool> m_currentWheelEventWillStartSwipe;
};

} // namespace WebKit

SPECIALIZE_TYPE_TRAITS_SCROLLING_COORDINATOR(WebKit::RemoteScrollingCoordinator, isRemoteScrollingCoordinator());

#endif // ENABLE(ASYNC_SCROLLING)
