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
#include <WebCore/Color.h>
#include <WebCore/FloatRect.h>
#include <wtf/BlockPtr.h>
#include <wtf/MonotonicTime.h>
#include <wtf/RetainPtr.h>
#include <wtf/RunLoop.h>
#include <wtf/WeakPtr.h>

OBJC_CLASS CALayer;

#if PLATFORM(IOS_FAMILY)
OBJC_CLASS UIGestureRecognizer;
OBJC_CLASS UIView;
OBJC_CLASS WKSwipeTransitionController;
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
    void platformTeardown();

    void disconnectFromProcess();
    void connectToProcess();
    
    enum class ViewGestureType {
        None,
#if PLATFORM(MAC)
        Magnification,
        SmartMagnification,
#endif
        Swipe
    };

    enum class SwipeDirection {
        Back,
        Forward
    };

    typedef uint64_t GestureID;

#if PLATFORM(MAC)
    double magnification() const;

    void handleMagnificationGestureEvent(NSEvent *, WebCore::FloatPoint origin);

    bool hasActiveMagnificationGesture() const { return m_activeGestureType == ViewGestureType::Magnification; }

    void handleSmartMagnificationGesture(WebCore::FloatPoint origin);

    bool handleScrollWheelEvent(NSEvent *);
    void wheelEventWasNotHandledByWebCore(NSEvent *event) { m_pendingSwipeTracker.eventWasNotHandledByWebCore(event); }
    void gestureEventWasNotHandledByWebCore(NSEvent *, WebCore::FloatPoint origin);

    void setCustomSwipeViews(Vector<RetainPtr<NSView>> views) { m_customSwipeViews = WTFMove(views); }
    void setCustomSwipeViewsTopContentInset(float topContentInset) { m_customSwipeViewsTopContentInset = topContentInset; }
    WebCore::FloatRect windowRelativeBoundsForCustomSwipeViews() const;
    void setDidMoveSwipeSnapshotCallback(BlockPtr<void (CGRect)>&& callback) { m_didMoveSwipeSnapshotCallback = WTFMove(callback); }

    bool shouldIgnorePinnedState() { return m_pendingSwipeTracker.shouldIgnorePinnedState(); }
    void setShouldIgnorePinnedState(bool ignore) { m_pendingSwipeTracker.setShouldIgnorePinnedState(ignore); }

    bool isPhysicallySwipingLeft(SwipeDirection) const;
#else
    bool isNavigationSwipeGestureRecognizer(UIGestureRecognizer *) const;
    void installSwipeHandler(UIView *gestureRecognizerView, UIView *swipingView);
    void beginSwipeGesture(_UINavigationInteractiveTransitionBase *, SwipeDirection);
    void endSwipeGesture(WebBackForwardListItem* targetItem, _UIViewControllerTransitionContext *, bool cancelled);
    void willCommitPostSwipeTransitionLayerTree(bool);
    void setRenderTreeSize(uint64_t);
#endif
    
    void setAlternateBackForwardListSourcePage(WebPageProxy*);
    
    bool canSwipeInDirection(SwipeDirection) const;

    WebCore::Color backgroundColorForCurrentSnapshot() const { return m_backgroundColorForCurrentSnapshot; }

    void didStartProvisionalLoadForMainFrame();
    void didFinishLoadForMainFrame() { didReachMainFrameLoadTerminalState(); }
    void didFailLoadForMainFrame() { didReachMainFrameLoadTerminalState(); }
    void didFirstVisuallyNonEmptyLayoutForMainFrame();
    void didRepaintAfterNavigation();
    void didHitRenderTreeSizeThreshold();
    void didRestoreScrollPosition();
    void didReachMainFrameLoadTerminalState();
    void didSameDocumentNavigationForMainFrame(SameDocumentNavigationType);

    void checkForActiveLoads();

    void removeSwipeSnapshot();

    void setSwipeGestureEnabled(bool enabled) { m_swipeGestureEnabled = enabled; }
    bool isSwipeGestureEnabled() { return m_swipeGestureEnabled; }

    // Testing
    bool beginSimulatedSwipeInDirectionForTesting(SwipeDirection);
    bool completeSimulatedSwipeInDirectionForTesting(SwipeDirection);

private:
    // IPC::MessageReceiver.
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;

    static ViewGestureController* controllerForGesture(uint64_t pageID, GestureID);

    static GestureID takeNextGestureID();
    void willBeginGesture(ViewGestureType);
    void didEndGesture();

    class SnapshotRemovalTracker {
    public:
        enum Event : uint8_t {
            VisuallyNonEmptyLayout = 1 << 0,
            RenderTreeSizeThreshold = 1 << 1,
            RepaintAfterNavigation = 1 << 2,
            MainFrameLoad = 1 << 3,
            SubresourceLoads = 1 << 4,
            ScrollPositionRestoration = 1 << 5
        };
        typedef uint8_t Events;

        SnapshotRemovalTracker();

        void start(Events, WTF::Function<void()>&&);
        void reset();
        
        void pause() { m_paused = true; }
        void resume();
        bool isPaused() const { return m_paused; }

        bool eventOccurred(Events);
        bool cancelOutstandingEvent(Events);

        void startWatchdog(Seconds);

    private:
        static String eventsDescription(Events);
        void log(const String&) const;

        void fireRemovalCallbackImmediately();
        void fireRemovalCallbackIfPossible();
        void watchdogTimerFired();

        bool stopWaitingForEvent(Events, const String& logReason);

        Events m_outstandingEvents { 0 };
        WTF::Function<void()> m_removalCallback;
        MonotonicTime m_startTime;

        RunLoop::Timer<SnapshotRemovalTracker> m_watchdogTimer;
        
        bool m_paused { true };
    };

#if PLATFORM(MAC)
    // Message handlers.
    void didCollectGeometryForMagnificationGesture(WebCore::FloatRect visibleContentBounds, bool frameHandlesMagnificationGesture);
    void didCollectGeometryForSmartMagnificationGesture(WebCore::FloatPoint origin, WebCore::FloatRect renderRect, WebCore::FloatRect visibleContentBounds, bool fitEntireRect, double viewportMinimumScale, double viewportMaximumScale);

    void endMagnificationGesture();

    WebCore::FloatPoint scaledMagnificationOrigin(WebCore::FloatPoint origin, double scale);

    void trackSwipeGesture(NSEvent *, SwipeDirection);
    void beginSwipeGesture(WebBackForwardListItem* targetItem, SwipeDirection);
    void handleSwipeGesture(WebBackForwardListItem* targetItem, double progress, SwipeDirection);
    void willEndSwipeGesture(WebBackForwardListItem& targetItem, bool cancelled);
    void endSwipeGesture(WebBackForwardListItem* targetItem, bool cancelled);
    bool shouldUseSnapshotForSize(ViewSnapshot&, WebCore::FloatSize swipeLayerSize, float topContentInset);

    CALayer *determineSnapshotLayerParent() const;
    CALayer *determineLayerAdjacentToSnapshotForParent(SwipeDirection, CALayer *snapshotLayerParent) const;
    void applyDebuggingPropertiesToSwipeViews();
    void didMoveSwipeSnapshotLayer();

    void forceRepaintIfNeeded();

    class PendingSwipeTracker {
    public:
        PendingSwipeTracker(WebPageProxy&, ViewGestureController&);
        bool handleEvent(NSEvent *);
        void eventWasNotHandledByWebCore(NSEvent *);

        void reset(const char* resetReasonForLogging);

        bool shouldIgnorePinnedState() { return m_shouldIgnorePinnedState; }
        void setShouldIgnorePinnedState(bool ignore) { m_shouldIgnorePinnedState = ignore; }

    private:
        bool tryToStartSwipe(NSEvent *);
        bool scrollEventCanBecomeSwipe(NSEvent *, SwipeDirection&);

        enum class State {
            None,
            WaitingForWebCore,
            InsufficientMagnitude
        };

        State m_state { State::None };
        SwipeDirection m_direction;
        WebCore::FloatSize m_cumulativeDelta;

        bool m_shouldIgnorePinnedState { false };

        ViewGestureController& m_viewGestureController;
        WebPageProxy& m_webPageProxy;
    };
#endif

    WebPageProxy& m_webPageProxy;
    ViewGestureType m_activeGestureType { ViewGestureType::None };

    bool m_swipeGestureEnabled { true };

    RunLoop::Timer<ViewGestureController> m_swipeActiveLoadMonitoringTimer;

    WebCore::Color m_backgroundColorForCurrentSnapshot;
    
    WeakPtr<WebPageProxy> m_alternateBackForwardListSourcePage;
    RefPtr<WebPageProxy> m_webPageProxyForBackForwardListForCurrentSwipe;

    GestureID m_currentGestureID;

#if PLATFORM(MAC)
    RefPtr<ViewSnapshot> m_currentSwipeSnapshot;

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

    Vector<RetainPtr<NSView>> m_customSwipeViews;
    float m_customSwipeViewsTopContentInset { 0 };
    WebCore::FloatRect m_currentSwipeCustomViewBounds;

    PendingSwipeTracker m_pendingSwipeTracker;

    BlockPtr<void (CGRect)> m_didMoveSwipeSnapshotCallback;

    bool m_hasOutstandingRepaintRequest { false };
#else    
    UIView *m_liveSwipeView { nullptr };
    RetainPtr<UIView> m_liveSwipeViewClippingView;
    RetainPtr<UIView> m_snapshotView;
    RetainPtr<UIView> m_transitionContainerView;
    RetainPtr<WKSwipeTransitionController> m_swipeInteractiveTransitionDelegate;
    RetainPtr<_UIViewControllerOneToOneTransitionContext> m_swipeTransitionContext;
    uint64_t m_snapshotRemovalTargetRenderTreeSize { 0 };
#endif
    bool m_isConnectedToProcess { false };

    SnapshotRemovalTracker m_snapshotRemovalTracker;
    WTF::Function<void()> m_provisionalLoadCallback;
};

} // namespace WebKit

#endif // ViewGestureController_h
