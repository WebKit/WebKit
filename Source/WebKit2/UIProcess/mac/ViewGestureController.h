/*
 * Copyright (C) 2013, 2014 Apple Inc. All rights reserved.
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

#ifndef ViewGestureController_h
#define ViewGestureController_h

#include "MessageReceiver.h"
#include "SameDocumentNavigationType.h"
#include "WeakObjCPtr.h"
#include <WebCore/Color.h>
#include <WebCore/FloatRect.h>
#include <wtf/RetainPtr.h>
#include <wtf/RunLoop.h>

OBJC_CLASS CALayer;

#if PLATFORM(IOS)
OBJC_CLASS UIView;
OBJC_CLASS WKSwipeTransitionController;
OBJC_CLASS WKWebView;
OBJC_CLASS _UINavigationInteractiveTransitionBase;
OBJC_CLASS _UIViewControllerOneToOneTransitionContext;
OBJC_CLASS _UIViewControllerTransitionContext;
#else
OBJC_CLASS CAGradientLayer;
OBJC_CLASS NSEvent;
OBJC_CLASS NSView;
OBJC_CLASS WKSwipeCancellationTracker;
#endif

namespace WebCore {
class IOSurface;
}

namespace WebKit {

class ViewSnapshot;
class WebBackForwardListItem;
class WebPageProxy;

class ViewGestureController : private IPC::MessageReceiver {
    WTF_MAKE_NONCOPYABLE(ViewGestureController);
public:
    ViewGestureController(WebPageProxy&);
    ~ViewGestureController();
    
    enum class ViewGestureType {
        None,
#if PLATFORM(MAC)
        Magnification,
        SmartMagnification,
#endif
        Swipe
    };
    
    enum class SwipeTransitionStyle {
        Overlap,
        Push
    };
    
    enum class SwipeDirection {
        Back,
        Forward
    };

    enum class PendingSwipeReason {
        None,
        WebCoreMayScroll,
        InsufficientMagnitude
    };
    
#if PLATFORM(MAC)
    double magnification() const;

    void handleMagnificationGesture(double scale, WebCore::FloatPoint origin);
    void endMagnificationGesture();

    void handleSmartMagnificationGesture(WebCore::FloatPoint origin);

    bool handleScrollWheelEvent(NSEvent *);
    void wheelEventWasNotHandledByWebCore(NSEvent *);

    void setCustomSwipeViews(Vector<RetainPtr<NSView>> views) { m_customSwipeViews = WTF::move(views); }
    void setCustomSwipeViewsTopContentInset(float topContentInset) { m_customSwipeViewsTopContentInset = topContentInset; }
    WebCore::FloatRect windowRelativeBoundsForCustomSwipeViews() const;
    void setDidMoveSwipeSnapshotCallback(void(^)(CGRect));

    bool shouldIgnorePinnedState() { return m_shouldIgnorePinnedState; }
    void setShouldIgnorePinnedState(bool ignore) { m_shouldIgnorePinnedState = ignore; }

    void didFirstVisuallyNonEmptyLayoutForMainFrame();
#else
    void installSwipeHandler(UIView *gestureRecognizerView, UIView *swipingView);
    void setAlternateBackForwardListSourceView(WKWebView *);
    bool canSwipeInDirection(SwipeDirection);
    void beginSwipeGesture(_UINavigationInteractiveTransitionBase *, SwipeDirection);
    void endSwipeGesture(WebBackForwardListItem* targetItem, _UIViewControllerTransitionContext *, bool cancelled);
    void willCommitPostSwipeTransitionLayerTree(bool);
    void setRenderTreeSize(uint64_t);
    void didRestoreScrollPosition();
#endif

    void didFinishLoadForMainFrame() { mainFrameLoadDidReachTerminalState(); }
    void didFailLoadForMainFrame() { mainFrameLoadDidReachTerminalState(); }
    void mainFrameLoadDidReachTerminalState();
    void removeSwipeSnapshot();
    void didSameDocumentNavigationForMainFrame(SameDocumentNavigationType);

    WebCore::Color backgroundColorForCurrentSnapshot() const { return m_backgroundColorForCurrentSnapshot; }

private:
    // IPC::MessageReceiver.
    virtual void didReceiveMessage(IPC::Connection&, IPC::MessageDecoder&) override;

    void swipeSnapshotWatchdogTimerFired();
    void activeLoadMonitoringTimerFired();

#if PLATFORM(MAC)
    // Message handlers.
    void didCollectGeometryForMagnificationGesture(WebCore::FloatRect visibleContentBounds, bool frameHandlesMagnificationGesture);
    void didCollectGeometryForSmartMagnificationGesture(WebCore::FloatPoint origin, WebCore::FloatRect renderRect, WebCore::FloatRect visibleContentBounds, bool isReplacedElement, double viewportMinimumScale, double viewportMaximumScale);
    void didHitRenderTreeSizeThreshold();
    void removeSwipeSnapshotAfterRepaint();

    WebCore::FloatPoint scaledMagnificationOrigin(WebCore::FloatPoint origin, double scale);

    void trackSwipeGesture(NSEvent *, SwipeDirection);
    void beginSwipeGesture(WebBackForwardListItem* targetItem, SwipeDirection);
    void handleSwipeGesture(WebBackForwardListItem* targetItem, double progress, SwipeDirection);
    void willEndSwipeGesture(WebBackForwardListItem& targetItem, bool cancelled);
    void endSwipeGesture(WebBackForwardListItem* targetItem, bool cancelled);
    bool deltaIsSufficientToBeginSwipe(NSEvent *);
    bool scrollEventCanBecomeSwipe(NSEvent *, SwipeDirection&);
    bool shouldUseSnapshotForSize(ViewSnapshot&, WebCore::FloatSize swipeLayerSize, float topContentInset);

    CALayer *determineSnapshotLayerParent() const;
    CALayer *determineLayerAdjacentToSnapshotForParent(SwipeDirection, CALayer *snapshotLayerParent) const;
    void applyDebuggingPropertiesToSwipeViews();
    void didMoveSwipeSnapshotLayer();
#else
    void removeSwipeSnapshotIfReady();
#endif

    WebPageProxy& m_webPageProxy;
    ViewGestureType m_activeGestureType { ViewGestureType::None };

    RunLoop::Timer<ViewGestureController> m_swipeWatchdogTimer;
    RunLoop::Timer<ViewGestureController> m_swipeActiveLoadMonitoringTimer;

    WebCore::Color m_backgroundColorForCurrentSnapshot;

#if PLATFORM(MAC)
    RefPtr<ViewSnapshot> m_currentSwipeSnapshot;

    RunLoop::Timer<ViewGestureController> m_swipeWatchdogAfterFirstVisuallyNonEmptyLayoutTimer;

    double m_magnification;
    WebCore::FloatPoint m_magnificationOrigin;

    WebCore::FloatRect m_lastSmartMagnificationUnscaledTargetRect;
    bool m_lastMagnificationGestureWasSmartMagnification { false };
    WebCore::FloatPoint m_lastSmartMagnificationOrigin;

    WebCore::FloatRect m_visibleContentRect;
    bool m_visibleContentRectIsValid { false };
    bool m_frameHandlesMagnificationGesture { false };

    RetainPtr<WKSwipeCancellationTracker> m_swipeCancellationTracker;
    RetainPtr<CALayer> m_swipeLayer;
    RetainPtr<CALayer> m_swipeSnapshotLayer;
    RetainPtr<CAGradientLayer> m_swipeShadowLayer;
    RetainPtr<CALayer> m_swipeDimmingLayer;
    Vector<RetainPtr<CALayer>> m_currentSwipeLiveLayers;

    SwipeTransitionStyle m_swipeTransitionStyle { SwipeTransitionStyle::Overlap };
    Vector<RetainPtr<NSView>> m_customSwipeViews;
    float m_customSwipeViewsTopContentInset { 0 };
    WebCore::FloatRect m_currentSwipeCustomViewBounds;

    // If we need to wait for content to decide if it is going to consume
    // the scroll event that would have started a swipe, we'll fill these in.
    PendingSwipeReason m_pendingSwipeReason { PendingSwipeReason::None };
    SwipeDirection m_pendingSwipeDirection;
    WebCore::FloatSize m_cumulativeDeltaForPendingSwipe;

    void (^m_didMoveSwipeSnapshotCallback)(CGRect) { nullptr };

    bool m_shouldIgnorePinnedState { false };
    bool m_swipeInProgress { false };
#else    
    UIView *m_liveSwipeView { nullptr };
    RetainPtr<UIView> m_liveSwipeViewClippingView;
    RetainPtr<UIView> m_snapshotView;
    RetainPtr<UIView> m_transitionContainerView;
    RetainPtr<WKSwipeTransitionController> m_swipeInteractiveTransitionDelegate;
    RetainPtr<_UIViewControllerOneToOneTransitionContext> m_swipeTransitionContext;
    uint64_t m_snapshotRemovalTargetRenderTreeSize { 0 };
    WeakObjCPtr<WKWebView> m_alternateBackForwardListSourceView;
    RefPtr<WebPageProxy> m_webPageProxyForBackForwardListForCurrentSwipe;
    uint64_t m_gesturePendingSnapshotRemoval { 0 };
#endif

    bool m_swipeWaitingForVisuallyNonEmptyLayout { false };
    bool m_swipeWaitingForRenderTreeSizeThreshold { false };
    bool m_swipeWaitingForRepaint { false };
    bool m_swipeWaitingForTerminalLoadingState { false };
    bool m_swipeWaitingForSubresourceLoads { false };
    bool m_swipeWaitingForScrollPositionRestoration { false };
};

} // namespace WebKit

#endif // ViewGestureController_h
