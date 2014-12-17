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

#import "config.h"
#import "ViewGestureController.h"

#if PLATFORM(IOS)

#import "ViewGestureControllerMessages.h"
#import "ViewGestureGeometryCollectorMessages.h"
#import "ViewSnapshotStore.h"
#import "WKBackForwardListItemInternal.h"
#import "WKWebViewInternal.h"
#import "WebBackForwardList.h"
#import "WebPageGroup.h"
#import "WebPageMessages.h"
#import "WebPageProxy.h"
#import "WebProcessProxy.h"
#import <QuartzCore/QuartzCorePrivate.h>
#import <UIKit/UIScreenEdgePanGestureRecognizer.h>
#import <UIKit/UIViewControllerTransitioning_Private.h>
#import <UIKit/UIWebTouchEventsGestureRecognizer.h>
#import <UIKit/_UINavigationInteractiveTransition.h>
#import <UIKit/_UINavigationParallaxTransition.h>
#import <WebCore/IOSurface.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/text/StringBuilder.h>

using namespace WebCore;

@interface WKSwipeTransitionController : NSObject <_UINavigationInteractiveTransitionBaseDelegate>
- (instancetype)initWithViewGestureController:(WebKit::ViewGestureController*)gestureController gestureRecognizerView:(UIView *)gestureRecognizerView;
- (void)invalidate;
@end

@interface _UIViewControllerTransitionContext (WKDetails)
@property (nonatomic, copy, setter=_setInteractiveUpdateHandler:)  void (^_interactiveUpdateHandler)(BOOL interactionIsOver, CGFloat percentComplete, BOOL transitionCompleted, _UIViewControllerTransitionContext *);
@end

@implementation WKSwipeTransitionController
{
    WebKit::ViewGestureController *_gestureController;
    RetainPtr<_UINavigationInteractiveTransitionBase> _backTransitionController;
    RetainPtr<_UINavigationInteractiveTransitionBase> _forwardTransitionController;
}

static const float swipeSnapshotRemovalRenderTreeSizeTargetFraction = 0.5;
static const std::chrono::seconds swipeSnapshotRemovalWatchdogDuration = 3_s;

// The key in this map is the associated page ID.
static HashMap<uint64_t, WebKit::ViewGestureController*>& viewGestureControllersForAllPages()
{
    static NeverDestroyed<HashMap<uint64_t, WebKit::ViewGestureController*>> viewGestureControllers;
    return viewGestureControllers.get();
}

- (instancetype)initWithViewGestureController:(WebKit::ViewGestureController*)gestureController gestureRecognizerView:(UIView *)gestureRecognizerView
{
    self = [super init];
    if (self) {
        _gestureController = gestureController;

        _backTransitionController = adoptNS([_UINavigationInteractiveTransitionBase alloc]);
        _backTransitionController = [_backTransitionController initWithGestureRecognizerView:gestureRecognizerView animator:nil delegate:self];
        
        _forwardTransitionController = adoptNS([_UINavigationInteractiveTransitionBase alloc]);
        _forwardTransitionController = [_forwardTransitionController initWithGestureRecognizerView:gestureRecognizerView animator:nil delegate:self];
        [_forwardTransitionController setShouldReverseTranslation:YES];
    }
    return self;
}

- (void)invalidate
{
    _gestureController = nullptr;
}

- (WebKit::ViewGestureController::SwipeDirection)directionForTransition:(_UINavigationInteractiveTransitionBase *)transition
{
    return transition == _backTransitionController ? WebKit::ViewGestureController::SwipeDirection::Left : WebKit::ViewGestureController::SwipeDirection::Right;
}

- (void)startInteractiveTransition:(_UINavigationInteractiveTransitionBase *)transition
{
    _gestureController->beginSwipeGesture(transition, [self directionForTransition:transition]);
}

- (BOOL)shouldBeginInteractiveTransition:(_UINavigationInteractiveTransitionBase *)transition
{
    return _gestureController->canSwipeInDirection([self directionForTransition:transition]);
}

- (BOOL)interactiveTransition:(_UINavigationInteractiveTransitionBase *)transition gestureRecognizer:(UIGestureRecognizer *)gestureRecognizer shouldRecognizeSimultaneouslyWithGestureRecognizer:(UIGestureRecognizer *)otherGestureRecognizer
{
    return [otherGestureRecognizer isKindOfClass:[UITapGestureRecognizer class]];
}

- (BOOL)interactiveTransition:(_UINavigationInteractiveTransitionBase *)transition gestureRecognizer:(UIGestureRecognizer *)gestureRecognizer shouldReceiveTouch:(UITouch *)touch
{
    return YES;
}

- (UIPanGestureRecognizer *)gestureRecognizerForInteractiveTransition:(_UINavigationInteractiveTransitionBase *)transition WithTarget:(id)target action:(SEL)action
{
    UIScreenEdgePanGestureRecognizer *recognizer = [[UIScreenEdgePanGestureRecognizer alloc] initWithTarget:target action:action];
    switch ([self directionForTransition:transition]) {
    case WebKit::ViewGestureController::SwipeDirection::Left:
        [recognizer setEdges:UIRectEdgeLeft];
        break;
    case WebKit::ViewGestureController::SwipeDirection::Right:
        [recognizer setEdges:UIRectEdgeRight];
        break;
    }
    return [recognizer autorelease];
}

@end

namespace WebKit {

ViewGestureController::ViewGestureController(WebPageProxy& webPageProxy)
    : m_webPageProxy(webPageProxy)
    , m_activeGestureType(ViewGestureType::None)
    , m_swipeWatchdogTimer(RunLoop::main(), this, &ViewGestureController::swipeSnapshotWatchdogTimerFired)
    , m_snapshotRemovalTargetRenderTreeSize(0)
    , m_shouldRemoveSnapshotWhenTargetRenderTreeSizeHit(false)
    , m_gesturePendingSnapshotRemoval(0)
{
    viewGestureControllersForAllPages().add(webPageProxy.pageID(), this);
}

ViewGestureController::~ViewGestureController()
{
    [m_swipeTransitionContext _setTransitionIsInFlight:NO];
    [m_swipeTransitionContext _setInteractor:nil];
    [m_swipeTransitionContext _setAnimator:nil];
    [m_swipeInteractiveTransitionDelegate invalidate];
    viewGestureControllersForAllPages().remove(m_webPageProxy.pageID());
}

void ViewGestureController::setAlternateBackForwardListSourceView(WKWebView *view)
{
    m_alternateBackForwardListSourceView = view;
}

void ViewGestureController::installSwipeHandler(UIView *gestureRecognizerView, UIView *swipingView)
{
    ASSERT(!m_swipeInteractiveTransitionDelegate);
    m_swipeInteractiveTransitionDelegate = adoptNS([[WKSwipeTransitionController alloc] initWithViewGestureController:this gestureRecognizerView:gestureRecognizerView]);
    m_liveSwipeView = swipingView;
}

#if ENABLE(VIEW_GESTURE_CONTROLLER_TRACING)
static void addLogEntry(Vector<String>& entries, const String& message)
{
    void* stack[32];
    int size = WTF_ARRAY_LENGTH(stack);
    WTFGetBacktrace(stack, &size);
    StringBuilder stringBuilder;
    stringBuilder.append(String::format("%f [ ", CFAbsoluteTimeGetCurrent()));
    for (int i = 2; i < size; ++i) {
        if (i > 2)
            stringBuilder.appendLiteral(", ");
        stringBuilder.append(String::format("%p", stack[i]));
    }
    stringBuilder.appendLiteral(" ] ");
    stringBuilder.append(message);
    entries.append(stringBuilder.toString());
}

static void dumpLogEntries(const Vector<String>& entries, WebPageProxy* webPageProxy)
{
    for (const auto& entry: entries)
        WTFLogAlways(entry.utf8().data());
    WTFLogAlways("m_webPageProxy: %p", webPageProxy);
}
#endif

void ViewGestureController::beginSwipeGesture(_UINavigationInteractiveTransitionBase *transition, SwipeDirection direction)
{
    if (m_activeGestureType != ViewGestureType::None)
        return;

    m_webPageProxy.recordNavigationSnapshot();

    m_webPageProxyForBackForwardListForCurrentSwipe = m_alternateBackForwardListSourceView.get() ? m_alternateBackForwardListSourceView.get()->_page : &m_webPageProxy;
#if ENABLE(VIEW_GESTURE_CONTROLLER_TRACING)
    addLogEntry(m_logEntries, String::format("m_webPageProxyForBackForwardListForCurrentSwipe: %p", m_webPageProxyForBackForwardListForCurrentSwipe.get()));
#endif
    m_webPageProxyForBackForwardListForCurrentSwipe->navigationGestureDidBegin();

    auto& backForwardList = m_webPageProxyForBackForwardListForCurrentSwipe->backForwardList();

    // Copy the snapshot from this view to the one that owns the back forward list, so that
    // swiping forward will have the correct snapshot.
    if (m_webPageProxyForBackForwardListForCurrentSwipe != &m_webPageProxy)
        backForwardList.currentItem()->setSnapshot(m_webPageProxy.backForwardList().currentItem()->snapshot());

    RefPtr<WebBackForwardListItem> targetItem = direction == SwipeDirection::Left ? backForwardList.backItem() : backForwardList.forwardItem();

    CGRect liveSwipeViewFrame = [m_liveSwipeView frame];

    RetainPtr<UIViewController> snapshotViewController = adoptNS([[UIViewController alloc] init]);
    m_snapshotView = adoptNS([[UIView alloc] initWithFrame:liveSwipeViewFrame]);

    RetainPtr<UIColor> backgroundColor = [UIColor whiteColor];
    if (ViewSnapshot* snapshot = targetItem->snapshot()) {
        float deviceScaleFactor = m_webPageProxy.deviceScaleFactor();
        FloatSize swipeLayerSizeInDeviceCoordinates(liveSwipeViewFrame.size);
        swipeLayerSizeInDeviceCoordinates.scale(deviceScaleFactor);
        if (snapshot->hasImage() && snapshot->size() == swipeLayerSizeInDeviceCoordinates && deviceScaleFactor == snapshot->deviceScaleFactor())
            [m_snapshotView layer].contents = snapshot->asLayerContents();
        Color coreColor = snapshot->backgroundColor();
        if (coreColor.isValid())
            backgroundColor = adoptNS([[UIColor alloc] initWithCGColor:cachedCGColor(coreColor, ColorSpaceDeviceRGB)]);
    }

    [m_snapshotView setBackgroundColor:backgroundColor.get()];
    [m_snapshotView layer].contentsGravity = kCAGravityTopLeft;
    [m_snapshotView layer].contentsScale = m_liveSwipeView.window.screen.scale;
    [snapshotViewController setView:m_snapshotView.get()];

    m_transitionContainerView = adoptNS([[UIView alloc] initWithFrame:liveSwipeViewFrame]);
    m_liveSwipeViewClippingView = adoptNS([[UIView alloc] initWithFrame:liveSwipeViewFrame]);
    [m_liveSwipeViewClippingView setClipsToBounds:YES];

    [m_liveSwipeView.superview insertSubview:m_transitionContainerView.get() belowSubview:m_liveSwipeView];
    [m_liveSwipeViewClippingView addSubview:m_liveSwipeView];
    [m_transitionContainerView addSubview:m_liveSwipeViewClippingView.get()];

    RetainPtr<UIViewController> targettedViewController = adoptNS([[UIViewController alloc] init]);
    [targettedViewController setView:m_liveSwipeViewClippingView.get()];

    UINavigationControllerOperation transitionOperation = direction == SwipeDirection::Left ? UINavigationControllerOperationPop : UINavigationControllerOperationPush;
    RetainPtr<_UINavigationParallaxTransition> animationController = adoptNS([[_UINavigationParallaxTransition alloc] initWithCurrentOperation:transitionOperation]);

    m_swipeTransitionContext = adoptNS([[_UIViewControllerOneToOneTransitionContext alloc] init]);
    [m_swipeTransitionContext _setFromViewController:targettedViewController.get()];
    [m_swipeTransitionContext _setToViewController:snapshotViewController.get()];
    [m_swipeTransitionContext _setContainerView:m_transitionContainerView.get()];
    [m_swipeTransitionContext _setFromStartFrame:liveSwipeViewFrame];
    [m_swipeTransitionContext _setToEndFrame:liveSwipeViewFrame];
    [m_swipeTransitionContext _setToStartFrame:CGRectZero];
    [m_swipeTransitionContext _setFromEndFrame:CGRectZero];
    [m_swipeTransitionContext _setAnimator:animationController.get()];
    [m_swipeTransitionContext _setInteractor:transition];
    [m_swipeTransitionContext _setTransitionIsInFlight:YES];
    [m_swipeTransitionContext _setInteractiveUpdateHandler:^(BOOL finish, CGFloat percent, BOOL transitionCompleted, _UIViewControllerTransitionContext *) {
        if (finish)
            m_webPageProxyForBackForwardListForCurrentSwipe->navigationGestureWillEnd(transitionCompleted, *targetItem);
    }];
    uint64_t pageID = m_webPageProxy.pageID();
    [m_swipeTransitionContext _setCompletionHandler:^(_UIViewControllerTransitionContext *context, BOOL didComplete) {
        auto gestureControllerIter = viewGestureControllersForAllPages().find(pageID);
        if (gestureControllerIter != viewGestureControllersForAllPages().end())
            gestureControllerIter->value->endSwipeGesture(targetItem.get(), context, !didComplete);
    }];
    [m_swipeTransitionContext _setInteractiveUpdateHandler:^(BOOL, CGFloat, BOOL, _UIViewControllerTransitionContext *) { }];

    [transition setAnimationController:animationController.get()];
    [transition startInteractiveTransition:m_swipeTransitionContext.get()];

    m_activeGestureType = ViewGestureType::Swipe;
}

bool ViewGestureController::canSwipeInDirection(SwipeDirection direction)
{
    auto& backForwardList = m_alternateBackForwardListSourceView.get() ? m_alternateBackForwardListSourceView.get()->_page->backForwardList() : m_webPageProxy.backForwardList();
    if (direction == SwipeDirection::Left)
        return !!backForwardList.backItem();
    return !!backForwardList.forwardItem();
}

void ViewGestureController::endSwipeGesture(WebBackForwardListItem* targetItem, _UIViewControllerTransitionContext *context, bool cancelled)
{
    [context _setTransitionIsInFlight:NO];
    [context _setInteractor:nil];
    [context _setAnimator:nil];
    
    [[m_transitionContainerView superview] insertSubview:m_snapshotView.get() aboveSubview:m_transitionContainerView.get()];
    [[m_transitionContainerView superview] insertSubview:m_liveSwipeView aboveSubview:m_transitionContainerView.get()];
    [m_liveSwipeViewClippingView removeFromSuperview];
    m_liveSwipeViewClippingView = nullptr;
    [m_transitionContainerView removeFromSuperview];
    m_transitionContainerView = nullptr;

    if (cancelled) {
        // removeSwipeSnapshot will clear m_webPageProxyForBackForwardListForCurrentSwipe, so hold on to it here.
        RefPtr<WebPageProxy> webPageProxyForBackForwardListForCurrentSwipe = m_webPageProxyForBackForwardListForCurrentSwipe;
        removeSwipeSnapshot();
#if ENABLE(VIEW_GESTURE_CONTROLLER_TRACING)
        if (!webPageProxyForBackForwardListForCurrentSwipe)
            dumpLogEntries(m_logEntries, &m_webPageProxy);
        m_logEntries.clear();
#endif

        webPageProxyForBackForwardListForCurrentSwipe->navigationGestureDidEnd(false, *targetItem);

        return;
    }

    m_snapshotRemovalTargetRenderTreeSize = 0;
    if (ViewSnapshot* snapshot = targetItem->snapshot())
        m_snapshotRemovalTargetRenderTreeSize = snapshot->renderTreeSize() * swipeSnapshotRemovalRenderTreeSizeTargetFraction;

#if ENABLE(VIEW_GESTURE_CONTROLLER_TRACING)
    if (!m_webPageProxyForBackForwardListForCurrentSwipe)
        dumpLogEntries(m_logEntries, &m_webPageProxy);
    m_logEntries.clear();
#endif

    m_webPageProxyForBackForwardListForCurrentSwipe->navigationGestureDidEnd(true, *targetItem);
    m_webPageProxyForBackForwardListForCurrentSwipe->goToBackForwardItem(targetItem);

    if (auto drawingArea = m_webPageProxy.drawingArea()) {
        uint64_t pageID = m_webPageProxy.pageID();
        uint64_t gesturePendingSnapshotRemoval = m_gesturePendingSnapshotRemoval;
        drawingArea->dispatchAfterEnsuringDrawing([pageID, gesturePendingSnapshotRemoval] (CallbackBase::Error error) {
            auto gestureControllerIter = viewGestureControllersForAllPages().find(pageID);
            if (gestureControllerIter != viewGestureControllersForAllPages().end() && gestureControllerIter->value->m_gesturePendingSnapshotRemoval == gesturePendingSnapshotRemoval)
                gestureControllerIter->value->willCommitPostSwipeTransitionLayerTree(error == CallbackBase::Error::None);
        });
    } else {
        removeSwipeSnapshot();
        return;
    }

    m_swipeWatchdogTimer.startOneShot(swipeSnapshotRemovalWatchdogDuration.count());
}

void ViewGestureController::willCommitPostSwipeTransitionLayerTree(bool successful)
{
#if ENABLE(VIEW_GESTURE_CONTROLLER_TRACING)
    addLogEntry(m_logEntries, String::format("successful: %d; m_activeGestureType: %d; m_webPageProxyForBackForwardListForCurrentSwipe: %p", successful, m_activeGestureType, m_webPageProxyForBackForwardListForCurrentSwipe.get()));
#endif
    if (m_activeGestureType != ViewGestureType::Swipe)
        return;

    if (!successful) {
        removeSwipeSnapshot();
        return;
    }

    m_shouldRemoveSnapshotWhenTargetRenderTreeSizeHit = true;
}

void ViewGestureController::setRenderTreeSize(uint64_t renderTreeSize)
{
    if (m_activeGestureType != ViewGestureType::Swipe)
        return;

    if (!m_shouldRemoveSnapshotWhenTargetRenderTreeSizeHit)
        return;

    if (!m_snapshotRemovalTargetRenderTreeSize || renderTreeSize > m_snapshotRemovalTargetRenderTreeSize)
        removeSwipeSnapshot();
}

void ViewGestureController::swipeSnapshotWatchdogTimerFired()
{
    removeSwipeSnapshot();
}

void ViewGestureController::removeSwipeSnapshot()
{
    m_shouldRemoveSnapshotWhenTargetRenderTreeSizeHit = false;

    m_swipeWatchdogTimer.stop();

#if ENABLE(VIEW_GESTURE_CONTROLLER_TRACING)
    addLogEntry(m_logEntries, String::format("m_activeGestureType: %d; m_webPageProxyForBackForwardListForCurrentSwipe: %p", m_activeGestureType, m_webPageProxyForBackForwardListForCurrentSwipe.get()));
#endif
    if (m_activeGestureType != ViewGestureType::Swipe)
        return;
    
    ++m_gesturePendingSnapshotRemoval;

#if USE(IOSURFACE)
    if (m_currentSwipeSnapshotSurface)
        m_currentSwipeSnapshotSurface->setIsVolatile(true);
    m_currentSwipeSnapshotSurface = nullptr;
#endif
    
    [m_snapshotView removeFromSuperview];
    m_snapshotView = nullptr;
    
    m_snapshotRemovalTargetRenderTreeSize = 0;
    m_activeGestureType = ViewGestureType::None;

    m_webPageProxyForBackForwardListForCurrentSwipe->navigationGestureSnapshotWasRemoved();
    m_webPageProxyForBackForwardListForCurrentSwipe = nullptr;

    m_swipeTransitionContext = nullptr;
}

} // namespace WebKit

#endif // PLATFORM(IOS)
