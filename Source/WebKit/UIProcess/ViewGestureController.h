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

#pragma once

#include "MessageReceiver.h"
#include "SameDocumentNavigationType.h"
#include "WebPageProxyIdentifier.h"
#include <WebCore/Color.h>
#include <WebCore/FloatRect.h>
#include <wtf/MonotonicTime.h>
#include <wtf/RetainPtr.h>
#include <wtf/RunLoop.h>
#include <wtf/WeakPtr.h>

#if PLATFORM(COCOA)
#include <wtf/BlockPtr.h>
#endif

#if PLATFORM(GTK)
#include <WebCore/CairoUtilities.h>
#include <gtk/gtk.h>
#include <wtf/glib/GRefPtr.h>
#endif

#if PLATFORM(COCOA)
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
#endif

#if PLATFORM(MAC)
typedef NSEvent* PlatformScrollEvent;
#elif PLATFORM(GTK)
typedef struct _GdkEventScroll GdkEventScroll;
typedef GdkEventScroll* PlatformScrollEvent;
#endif

namespace WebKit {

class ViewSnapshot;
class WebBackForwardListItem;
class WebPageProxy;
class WebProcessProxy;

class ViewGestureController : private IPC::MessageReceiver {
    WTF_MAKE_FAST_ALLOCATED;
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

#if !PLATFORM(IOS_FAMILY)
    bool handleScrollWheelEvent(PlatformScrollEvent);
    void wheelEventWasNotHandledByWebCore(PlatformScrollEvent event) { m_pendingSwipeTracker.eventWasNotHandledByWebCore(event); }

    bool shouldIgnorePinnedState() { return m_pendingSwipeTracker.shouldIgnorePinnedState(); }
    void setShouldIgnorePinnedState(bool ignore) { m_pendingSwipeTracker.setShouldIgnorePinnedState(ignore); }

    bool isPhysicallySwipingLeft(SwipeDirection) const;
#endif

#if PLATFORM(MAC)
    double magnification() const;

    void handleMagnificationGestureEvent(PlatformScrollEvent, WebCore::FloatPoint origin);

    bool hasActiveMagnificationGesture() const { return m_activeGestureType == ViewGestureType::Magnification; }

    void handleSmartMagnificationGesture(WebCore::FloatPoint origin);

    void gestureEventWasNotHandledByWebCore(PlatformScrollEvent, WebCore::FloatPoint origin);

    void setCustomSwipeViews(Vector<RetainPtr<NSView>> views) { m_customSwipeViews = WTFMove(views); }
    void setCustomSwipeViewsTopContentInset(float topContentInset) { m_customSwipeViewsTopContentInset = topContentInset; }
    WebCore::FloatRect windowRelativeBoundsForCustomSwipeViews() const;
    void setDidMoveSwipeSnapshotCallback(BlockPtr<void (CGRect)>&& callback) { m_didMoveSwipeSnapshotCallback = WTFMove(callback); }
#elif PLATFORM(IOS_FAMILY)
    bool isNavigationSwipeGestureRecognizer(UIGestureRecognizer *) const;
    void installSwipeHandler(UIView *gestureRecognizerView, UIView *swipingView);
    void beginSwipeGesture(_UINavigationInteractiveTransitionBase *, SwipeDirection);
    void willEndSwipeGesture(WebBackForwardListItem& targetItem, bool cancelled);
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

#if PLATFORM(GTK)
    void cancelSwipe();
    void draw(cairo_t*, cairo_pattern_t*);
#endif

    // Testing
    bool beginSimulatedSwipeInDirectionForTesting(SwipeDirection);
    bool completeSimulatedSwipeInDirectionForTesting(SwipeDirection);

private:
    // IPC::MessageReceiver.
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;

    static ViewGestureController* controllerForGesture(WebPageProxyIdentifier, GestureID);

    static GestureID takeNextGestureID();
    void willBeginGesture(ViewGestureType);
    void didEndGesture();

    void didStartProvisionalOrSameDocumentLoadForMainFrame();

    class SnapshotRemovalTracker {
    public:
        enum Event : uint8_t {
            VisuallyNonEmptyLayout = 1 << 0,
            RenderTreeSizeThreshold = 1 << 1,
            RepaintAfterNavigation = 1 << 2,
            MainFrameLoad = 1 << 3,
            SubresourceLoads = 1 << 4,
            ScrollPositionRestoration = 1 << 5,
            SwipeAnimationEnd = 1 << 6
        };
        typedef uint8_t Events;

        SnapshotRemovalTracker();

        void start(Events, WTF::Function<void()>&&);
        void reset();

        void pause() { m_paused = true; }
        void resume();
        bool isPaused() const { return m_paused; }
        bool hasRemovalCallback() const { return !!m_removalCallback; }

        enum class ShouldIgnoreEventIfPaused : bool { No, Yes };
        bool eventOccurred(Events, ShouldIgnoreEventIfPaused = ShouldIgnoreEventIfPaused::Yes);
        bool cancelOutstandingEvent(Events);
        bool hasOutstandingEvent(Event);

        void startWatchdog(Seconds);

        uint64_t renderTreeSizeThreshold() const { return m_renderTreeSizeThreshold; }
        void setRenderTreeSizeThreshold(uint64_t threshold) { m_renderTreeSizeThreshold = threshold; }

    private:
        static String eventsDescription(Events);
        void log(const String&) const;

        void fireRemovalCallbackImmediately();
        void fireRemovalCallbackIfPossible();
        void watchdogTimerFired();

        bool stopWaitingForEvent(Events, const String& logReason, ShouldIgnoreEventIfPaused = ShouldIgnoreEventIfPaused::Yes);

        Events m_outstandingEvents { 0 };
        WTF::Function<void()> m_removalCallback;
        MonotonicTime m_startTime;

        uint64_t m_renderTreeSizeThreshold { 0 };

        RunLoop::Timer<SnapshotRemovalTracker> m_watchdogTimer;

        bool m_paused { true };
    };

#if PLATFORM(MAC)
    // Message handlers.
    void didCollectGeometryForMagnificationGesture(WebCore::FloatRect visibleContentBounds, bool frameHandlesMagnificationGesture);
    void didCollectGeometryForSmartMagnificationGesture(WebCore::FloatPoint origin, WebCore::FloatRect renderRect, WebCore::FloatRect visibleContentBounds, bool fitEntireRect, double viewportMinimumScale, double viewportMaximumScale);

    void endMagnificationGesture();

    WebCore::FloatPoint scaledMagnificationOrigin(WebCore::FloatPoint origin, double scale);
#endif

#if !PLATFORM(IOS_FAMILY)
    void startSwipeGesture(PlatformScrollEvent, SwipeDirection);
    void trackSwipeGesture(PlatformScrollEvent, SwipeDirection, RefPtr<WebBackForwardListItem>);

    void beginSwipeGesture(WebBackForwardListItem* targetItem, SwipeDirection);
    void handleSwipeGesture(WebBackForwardListItem* targetItem, double progress, SwipeDirection);

    void willEndSwipeGesture(WebBackForwardListItem& targetItem, bool cancelled);
    void endSwipeGesture(WebBackForwardListItem* targetItem, bool cancelled);
    bool shouldUseSnapshotForSize(ViewSnapshot&, WebCore::FloatSize swipeLayerSize, float topContentInset);

#if PLATFORM(MAC)
    CALayer* determineSnapshotLayerParent() const;
    CALayer* determineLayerAdjacentToSnapshotForParent(SwipeDirection, CALayer* snapshotLayerParent) const;
    void applyDebuggingPropertiesToSwipeViews();
    void didMoveSwipeSnapshotLayer();
#endif

    void requestRenderTreeSizeNotificationIfNeeded();
    void forceRepaintIfNeeded();

    class PendingSwipeTracker {
    public:
        PendingSwipeTracker(WebPageProxy&, ViewGestureController&);
        bool handleEvent(PlatformScrollEvent);
        void eventWasNotHandledByWebCore(PlatformScrollEvent);

        void reset(const char* resetReasonForLogging);

        bool shouldIgnorePinnedState() { return m_shouldIgnorePinnedState; }
        void setShouldIgnorePinnedState(bool ignore) { m_shouldIgnorePinnedState = ignore; }

    private:
        bool tryToStartSwipe(PlatformScrollEvent);
        bool scrollEventCanBecomeSwipe(PlatformScrollEvent, SwipeDirection&);

        bool scrollEventCanStartSwipe(PlatformScrollEvent);
        bool scrollEventCanEndSwipe(PlatformScrollEvent);
        bool scrollEventCanInfluenceSwipe(PlatformScrollEvent);
        WebCore::FloatSize scrollEventGetScrollingDeltas(PlatformScrollEvent);

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

#if PLATFORM(GTK)
    GRefPtr<GtkStyleContext> createStyleContext(const char*);
#endif

    WebPageProxy& m_webPageProxy;
    ViewGestureType m_activeGestureType { ViewGestureType::None };

    bool m_swipeGestureEnabled { true };

    RunLoop::Timer<ViewGestureController> m_swipeActiveLoadMonitoringTimer;

    WebCore::Color m_backgroundColorForCurrentSnapshot;

    WeakPtr<WebPageProxy> m_alternateBackForwardListSourcePage;
    RefPtr<WebPageProxy> m_webPageProxyForBackForwardListForCurrentSwipe;

    GestureID m_currentGestureID;

#if !PLATFORM(IOS_FAMILY)
    RefPtr<ViewSnapshot> m_currentSwipeSnapshot;

    PendingSwipeTracker m_pendingSwipeTracker;

    bool m_hasOutstandingRepaintRequest { false };
#endif

#if PLATFORM(MAC)
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

    BlockPtr<void (CGRect)> m_didMoveSwipeSnapshotCallback;
#elif PLATFORM(IOS_FAMILY)
    UIView* m_liveSwipeView { nullptr };
    RetainPtr<UIView> m_liveSwipeViewClippingView;
    RetainPtr<UIView> m_snapshotView;
    RetainPtr<UIView> m_transitionContainerView;
    RetainPtr<WKSwipeTransitionController> m_swipeInteractiveTransitionDelegate;
    RetainPtr<_UIViewControllerOneToOneTransitionContext> m_swipeTransitionContext;
    uint64_t m_snapshotRemovalTargetRenderTreeSize { 0 };
    bool m_didCallWillEndSwipeGesture { false };
#endif

#if PLATFORM(GTK)
    class SwipeProgressTracker {
    public:
        SwipeProgressTracker(WebPageProxy&, ViewGestureController&);
        void startTracking(RefPtr<WebBackForwardListItem>&&, SwipeDirection);
        void reset();
        bool handleEvent(PlatformScrollEvent);
        float progress() const { return m_progress; }
        SwipeDirection direction() const { return m_direction; }

    private:
        enum class State {
            None,
            Pending,
            Scrolling,
            Animating,
            Finishing
        };

        bool shouldCancel();

        void startAnimation();
        gboolean onAnimationTick(GdkFrameClock*);
        void endAnimation();

        State m_state { State::None };

        SwipeDirection m_direction { SwipeDirection::Back };
        RefPtr<WebBackForwardListItem> m_targetItem;
        unsigned m_tickCallbackID { 0 };

        Seconds m_prevTime;
        double m_velocity { 0 };
        double m_distance { 0 };

        Seconds m_startTime;
        Seconds m_endTime;

        float m_progress { 0 };
        float m_startProgress { 0 };
        float m_endProgress { 0 };
        bool m_cancelled { false };

        ViewGestureController& m_viewGestureController;
        WebPageProxy& m_webPageProxy;
    };

    SwipeProgressTracker m_swipeProgressTracker;

    RefPtr<cairo_pattern_t> m_currentSwipeSnapshotPattern;
    RefPtr<cairo_pattern_t> m_swipeDimmingPattern;
    RefPtr<cairo_pattern_t> m_swipeShadowPattern;
    RefPtr<cairo_pattern_t> m_swipeBorderPattern;
    RefPtr<cairo_pattern_t> m_swipeOutlinePattern;
    int m_swipeShadowSize;
    int m_swipeBorderSize;
    int m_swipeOutlineSize;
    GRefPtr<GtkCssProvider> m_cssProvider;

    bool m_isSimulatedSwipe { false };
#endif

    bool m_isConnectedToProcess { false };
    bool m_didStartProvisionalLoad { false };

    bool m_didCallEndSwipeGesture { false };
    bool m_removeSnapshotImmediatelyWhenGestureEnds { false };

    SnapshotRemovalTracker m_snapshotRemovalTracker;
    WTF::Function<void()> m_loadCallback;
};

} // namespace WebKit
