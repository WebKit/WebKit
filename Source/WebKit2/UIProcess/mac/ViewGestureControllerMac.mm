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

#if !PLATFORM(IOS)

#import "NativeWebWheelEvent.h"
#import "WebPageGroup.h"
#import "ViewGestureControllerMessages.h"
#import "ViewGestureGeometryCollectorMessages.h"
#import "ViewSnapshotStore.h"
#import "WebBackForwardList.h"
#import "WebPageMessages.h"
#import "WebPageProxy.h"
#import "WebPreferences.h"
#import "WebProcessProxy.h"
#import <Cocoa/Cocoa.h>
#import <QuartzCore/QuartzCore.h>
#import <WebCore/IOSurface.h>
#import <WebCore/WebCoreCALayerExtras.h>

#if defined(__has_include) && __has_include(<QuartzCore/QuartzCorePrivate.h>)
#import <QuartzCore/QuartzCorePrivate.h>
#else
@interface CAFilter : NSObject <NSCopying, NSMutableCopying, NSCoding>
@end
#endif

@interface CAFilter (Details)
+ (CAFilter *)filterWithType:(NSString *)type;
@end

extern NSString * const kCAFilterGaussianBlur;

using namespace WebCore;

static const double minMagnification = 1;
static const double maxMagnification = 3;

static const double minElasticMagnification = 0.75;
static const double maxElasticMagnification = 4;

static const double zoomOutBoost = 1.6;
static const double zoomOutResistance = 0.10;

static const float smartMagnificationElementPadding = 0.05;
static const float smartMagnificationPanScrollThreshold = 100;

static const double swipeOverlayShadowOpacity = 0.66;
static const double swipeOverlayShadowRadius = 3;

static const float swipeSnapshotRemovalRenderTreeSizeTargetFraction = 0.5;
static const std::chrono::seconds swipeSnapshotRemovalWatchdogDuration = 3_s;

namespace WebKit {

ViewGestureController::ViewGestureController(WebPageProxy& webPageProxy)
    : m_webPageProxy(webPageProxy)
    , m_activeGestureType(ViewGestureType::None)
    , m_swipeWatchdogTimer(this, &ViewGestureController::swipeSnapshotWatchdogTimerFired)
    , m_lastMagnificationGestureWasSmartMagnification(false)
    , m_visibleContentRectIsValid(false)
    , m_frameHandlesMagnificationGesture(false)
    , m_swipeTransitionStyle(SwipeTransitionStyle::Overlap)
    
    , m_hasPendingSwipe(false)
{
    m_webPageProxy.process().addMessageReceiver(Messages::ViewGestureController::messageReceiverName(), m_webPageProxy.pageID(), *this);
}

ViewGestureController::~ViewGestureController()
{
    m_webPageProxy.process().removeMessageReceiver(Messages::ViewGestureController::messageReceiverName(), m_webPageProxy.pageID());
}

static double resistanceForDelta(double deltaScale, double currentScale)
{
    // Zoom out with slight acceleration, until we reach minimum scale.
    if (deltaScale < 0 && currentScale > minMagnification)
        return zoomOutBoost;

    // Zoom in with no acceleration, until we reach maximum scale.
    if (deltaScale > 0 && currentScale < maxMagnification)
        return 1;

    // Outside of the extremes, resist further scaling.
    double limit = currentScale < minMagnification ? minMagnification : maxMagnification;
    double scaleDistance = fabs(limit - currentScale);
    double scalePercent = std::min(std::max(scaleDistance / limit, 0.), 1.);
    double resistance = zoomOutResistance + scalePercent * (0.01 - zoomOutResistance);

    return resistance;
}

FloatPoint ViewGestureController::scaledMagnificationOrigin(FloatPoint origin, double scale)
{
    FloatPoint scaledMagnificationOrigin(origin);
    scaledMagnificationOrigin.moveBy(m_visibleContentRect.location());
    float magnificationOriginScale = 1 - (scale / m_webPageProxy.pageScaleFactor());
    scaledMagnificationOrigin.scale(magnificationOriginScale, magnificationOriginScale);
    return scaledMagnificationOrigin;
}

void ViewGestureController::didCollectGeometryForMagnificationGesture(FloatRect visibleContentRect, bool frameHandlesMagnificationGesture)
{
    m_activeGestureType = ViewGestureType::Magnification;
    m_visibleContentRect = visibleContentRect;
    m_visibleContentRectIsValid = true;
    m_frameHandlesMagnificationGesture = frameHandlesMagnificationGesture;
}

void ViewGestureController::handleMagnificationGesture(double scale, FloatPoint origin)
{
    ASSERT(m_activeGestureType == ViewGestureType::None || m_activeGestureType == ViewGestureType::Magnification);

    if (m_activeGestureType == ViewGestureType::None) {
        // FIXME: We drop the first frame of the gesture on the floor, because we don't have the visible content bounds yet.
        m_magnification = m_webPageProxy.pageScaleFactor();
        m_webPageProxy.process().send(Messages::ViewGestureGeometryCollector::CollectGeometryForMagnificationGesture(), m_webPageProxy.pageID());
        m_lastMagnificationGestureWasSmartMagnification = false;

        return;
    }

    // We're still waiting for the DidCollectGeometry callback.
    if (!m_visibleContentRectIsValid)
        return;

    m_activeGestureType = ViewGestureType::Magnification;

    double scaleWithResistance = resistanceForDelta(scale, m_magnification) * scale;

    m_magnification += m_magnification * scaleWithResistance;
    m_magnification = std::min(std::max(m_magnification, minElasticMagnification), maxElasticMagnification);

    m_magnificationOrigin = origin;

    if (m_frameHandlesMagnificationGesture)
        m_webPageProxy.scalePage(m_magnification, roundedIntPoint(origin));
    else
        m_webPageProxy.drawingArea()->adjustTransientZoom(m_magnification, scaledMagnificationOrigin(origin, m_magnification));
}

void ViewGestureController::endMagnificationGesture()
{
    ASSERT(m_activeGestureType == ViewGestureType::Magnification);

    double newMagnification = std::min(std::max(m_magnification, minMagnification), maxMagnification);

    if (m_frameHandlesMagnificationGesture)
        m_webPageProxy.scalePage(newMagnification, roundedIntPoint(m_magnificationOrigin));
    else
        m_webPageProxy.drawingArea()->commitTransientZoom(newMagnification, scaledMagnificationOrigin(m_magnificationOrigin, newMagnification));

    m_activeGestureType = ViewGestureType::None;
}

void ViewGestureController::handleSmartMagnificationGesture(FloatPoint origin)
{
    if (m_activeGestureType != ViewGestureType::None)
        return;

    m_webPageProxy.process().send(Messages::ViewGestureGeometryCollector::CollectGeometryForSmartMagnificationGesture(origin), m_webPageProxy.pageID());
}

static float maximumRectangleComponentDelta(FloatRect a, FloatRect b)
{
    return std::max(fabs(a.x() - b.x()), std::max(fabs(a.y() - b.y()), std::max(fabs(a.width() - b.width()), fabs(a.height() - b.height()))));
}

void ViewGestureController::didCollectGeometryForSmartMagnificationGesture(FloatPoint origin, FloatRect renderRect, FloatRect visibleContentRect, bool isReplacedElement, double viewportMinimumScale, double viewportMaximumScale)
{
    double currentScaleFactor = m_webPageProxy.pageScaleFactor();

    FloatRect unscaledTargetRect = renderRect;
    unscaledTargetRect.scale(1 / currentScaleFactor);
    unscaledTargetRect.inflateX(unscaledTargetRect.width() * smartMagnificationElementPadding);
    unscaledTargetRect.inflateY(unscaledTargetRect.height() * smartMagnificationElementPadding);

    double targetMagnification = visibleContentRect.width() / unscaledTargetRect.width();

    // For replaced elements like images, we want to fit the whole element
    // in the view, so scale it down enough to make both dimensions fit if possible.
    if (isReplacedElement)
        targetMagnification = std::min(targetMagnification, static_cast<double>(visibleContentRect.height() / unscaledTargetRect.height()));

    targetMagnification = std::min(std::max(targetMagnification, minMagnification), maxMagnification);

    // Allow panning between elements via double-tap while magnified, unless the target rect is
    // similar to the last one or the mouse has not moved, in which case we'll zoom all the way out.
    if (currentScaleFactor > 1 && m_lastMagnificationGestureWasSmartMagnification) {
        if (maximumRectangleComponentDelta(m_lastSmartMagnificationUnscaledTargetRect, unscaledTargetRect) < smartMagnificationPanScrollThreshold)
            targetMagnification = 1;

        if (m_lastSmartMagnificationOrigin == origin)
            targetMagnification = 1;
    }

    FloatRect targetRect(unscaledTargetRect);
    targetRect.scale(targetMagnification);
    FloatPoint targetOrigin(visibleContentRect.center());
    targetOrigin.moveBy(-targetRect.center());

    m_webPageProxy.drawingArea()->adjustTransientZoom(m_webPageProxy.pageScaleFactor(), scaledMagnificationOrigin(FloatPoint(), m_webPageProxy.pageScaleFactor()));
    m_webPageProxy.drawingArea()->commitTransientZoom(targetMagnification, targetOrigin);

    m_lastSmartMagnificationUnscaledTargetRect = unscaledTargetRect;
    m_lastSmartMagnificationOrigin = origin;

    m_lastMagnificationGestureWasSmartMagnification = true;
}

static bool scrollEventCanBecomeSwipe(NSEvent *event, WebPageProxy& webPageProxy, ViewGestureController::SwipeDirection& potentialSwipeDirection)
{
    if (event.phase != NSEventPhaseBegan)
        return false;

    if (!event.hasPreciseScrollingDeltas)
        return false;

    if (![NSEvent isSwipeTrackingFromScrollEventsEnabled])
        return false;

    if (fabs(event.scrollingDeltaX) <= fabs(event.scrollingDeltaY))
        return false;

    bool willSwipeLeft = event.scrollingDeltaX > 0 && webPageProxy.isPinnedToLeftSide() && webPageProxy.backForwardList().backItem();
    bool willSwipeRight = event.scrollingDeltaX < 0 && webPageProxy.isPinnedToRightSide() && webPageProxy.backForwardList().forwardItem();
    if (!willSwipeLeft && !willSwipeRight)
        return false;

    potentialSwipeDirection = willSwipeLeft ? ViewGestureController::SwipeDirection::Left : ViewGestureController::SwipeDirection::Right;

    return true;
}

bool ViewGestureController::handleScrollWheelEvent(NSEvent *event)
{
    m_hasPendingSwipe = false;

    if (m_activeGestureType != ViewGestureType::None)
        return false;

    SwipeDirection direction;
    if (!scrollEventCanBecomeSwipe(event, m_webPageProxy, direction))
        return false;

    if (m_webPageProxy.willHandleHorizontalScrollEvents()) {
        m_hasPendingSwipe = true;
        m_pendingSwipeDirection = direction;
        return false;
    }

    trackSwipeGesture(event, direction);

    return true;
}

void ViewGestureController::wheelEventWasNotHandledByWebCore(NSEvent *event)
{
    if (!m_hasPendingSwipe)
        return;

    m_hasPendingSwipe = false;

    SwipeDirection direction;
    if (!scrollEventCanBecomeSwipe(event, m_webPageProxy, direction))
        return;

    trackSwipeGesture(event, m_pendingSwipeDirection);
}

void ViewGestureController::trackSwipeGesture(NSEvent *event, SwipeDirection direction)
{
    ViewSnapshotStore::shared().recordSnapshot(m_webPageProxy);

    CGFloat maxProgress = (direction == SwipeDirection::Left) ? 1 : 0;
    CGFloat minProgress = (direction == SwipeDirection::Right) ? -1 : 0;
    RefPtr<WebBackForwardListItem> targetItem = (direction == SwipeDirection::Left) ? m_webPageProxy.backForwardList().backItem() : m_webPageProxy.backForwardList().forwardItem();
    __block bool swipeCancelled = false;

    [event trackSwipeEventWithOptions:0 dampenAmountThresholdMin:minProgress max:maxProgress usingHandler:^(CGFloat progress, NSEventPhase phase, BOOL isComplete, BOOL *stop) {
        if (phase == NSEventPhaseBegan)
            this->beginSwipeGesture(targetItem.get(), direction);
        CGFloat clampedProgress = std::min(std::max(progress, minProgress), maxProgress);
        this->handleSwipeGesture(targetItem.get(), clampedProgress, direction);
        if (phase == NSEventPhaseCancelled)
            swipeCancelled = true;
        if (isComplete)
            this->endSwipeGesture(targetItem.get(), swipeCancelled);
    }];
}

FloatRect ViewGestureController::windowRelativeBoundsForCustomSwipeViews() const
{
    FloatRect swipeArea;
    for (const auto& view : m_customSwipeViews)
        swipeArea.unite([view convertRect:[view bounds] toView:nil]);
    return swipeArea;
}

static CALayer *leastCommonAncestorLayer(Vector<RetainPtr<CALayer>>& layers)
{
    Vector<Vector<CALayer *>> liveLayerPathsFromRoot(layers.size());

    size_t shortestPathLength = std::numeric_limits<size_t>::max();

    for (size_t layerIndex = 0; layerIndex < layers.size(); layerIndex++) {
        CALayer *parent = [layers[layerIndex] superlayer];
        while (parent) {
            liveLayerPathsFromRoot[layerIndex].insert(0, parent);
            shortestPathLength = std::min(shortestPathLength, liveLayerPathsFromRoot[layerIndex].size());
            parent = parent.superlayer;
        }
    }

    for (size_t pathIndex = 0; pathIndex < shortestPathLength; pathIndex++) {
        CALayer *firstPathLayer = liveLayerPathsFromRoot[0][pathIndex];
        for (size_t layerIndex = 1; layerIndex < layers.size(); layerIndex++) {
            if (liveLayerPathsFromRoot[layerIndex][pathIndex] != firstPathLayer)
                return firstPathLayer;
        }
    }

    return liveLayerPathsFromRoot[0][shortestPathLength];
}

void ViewGestureController::beginSwipeGesture(WebBackForwardListItem* targetItem, SwipeDirection direction)
{
    ASSERT(m_currentSwipeLiveLayers.isEmpty());

    m_activeGestureType = ViewGestureType::Swipe;

    CALayer *rootContentLayer = m_webPageProxy.acceleratedCompositingRootLayer();

    m_swipeSnapshotLayer = adoptNS([[CALayer alloc] init]);
    [m_swipeSnapshotLayer setBackgroundColor:CGColorGetConstantColor(kCGColorWhite)];

    RefPtr<IOSurface> snapshot = ViewSnapshotStore::shared().snapshotAndRenderTreeSize(targetItem).first;

    if (snapshot && snapshot->setIsPurgeable(false) == IOSurface::SurfaceState::Valid) {
        m_currentSwipeSnapshotSurface = snapshot;
        [m_swipeSnapshotLayer setContents:(id)snapshot->surface()];
    }

    m_currentSwipeCustomViewBounds = windowRelativeBoundsForCustomSwipeViews();

    FloatRect swipeArea;
    if (!m_customSwipeViews.isEmpty()) {
        swipeArea = m_currentSwipeCustomViewBounds;

        for (const auto& view : m_customSwipeViews) {
            CALayer *layer = [view layer];
            ASSERT(layer);
            m_currentSwipeLiveLayers.append(layer);
        }
    } else {
        swipeArea = FloatRect(FloatPoint(), m_webPageProxy.drawingArea()->size());
        m_currentSwipeLiveLayers.append(rootContentLayer);
    }

    [m_swipeSnapshotLayer setContentsGravity:kCAGravityTopLeft];
    [m_swipeSnapshotLayer setContentsScale:m_webPageProxy.deviceScaleFactor()];
    [m_swipeSnapshotLayer setAnchorPoint:CGPointZero];
    [m_swipeSnapshotLayer setFrame:swipeArea];
    [m_swipeSnapshotLayer setName:@"Gesture Swipe Snapshot Layer"];
    [m_swipeSnapshotLayer web_disableAllActions];

    if (m_webPageProxy.preferences().viewGestureDebuggingEnabled()) {
        CAFilter* filter = [CAFilter filterWithType:kCAFilterGaussianBlur];
        [filter setValue:[NSNumber numberWithFloat:3] forKey:@"inputRadius"];
        [m_swipeSnapshotLayer setFilters:@[filter]];
    }

    // We don't know enough about the custom views' hierarchy to apply a shadow.
    if (m_swipeTransitionStyle == SwipeTransitionStyle::Overlap && m_customSwipeViews.isEmpty()) {
        RetainPtr<CGPathRef> shadowPath = adoptCF(CGPathCreateWithRect([m_swipeSnapshotLayer bounds], 0));

        if (direction == SwipeDirection::Left) {
            [rootContentLayer setShadowColor:CGColorGetConstantColor(kCGColorBlack)];
            [rootContentLayer setShadowOpacity:swipeOverlayShadowOpacity];
            [rootContentLayer setShadowRadius:swipeOverlayShadowRadius];
            [rootContentLayer setShadowPath:shadowPath.get()];
        } else {
            [m_swipeSnapshotLayer setShadowColor:CGColorGetConstantColor(kCGColorBlack)];
            [m_swipeSnapshotLayer setShadowOpacity:swipeOverlayShadowOpacity];
            [m_swipeSnapshotLayer setShadowRadius:swipeOverlayShadowRadius];
            [m_swipeSnapshotLayer setShadowPath:shadowPath.get()];
        }
    }

    // If we have custom swiping views, we assume that the views were passed to us in back-to-front z-order.
    CALayer *layerAdjacentToSnapshot = direction == SwipeDirection::Left ? m_currentSwipeLiveLayers.first().get() : m_currentSwipeLiveLayers.last().get();
    CALayer *snapshotLayerParent;

    if (m_currentSwipeLiveLayers.size() == 1)
        snapshotLayerParent = [m_currentSwipeLiveLayers[0] superlayer];
    else {
        // We insert our snapshot into the first shared superlayer of the custom views' layer, above the frontmost or below the bottommost layer.
        snapshotLayerParent = leastCommonAncestorLayer(m_currentSwipeLiveLayers);

        // If the layers are not all siblings, find the child of the layer we're going to insert the snapshot into which has the frontmost/bottommost layer as a child.
        while (snapshotLayerParent != layerAdjacentToSnapshot.superlayer)
            layerAdjacentToSnapshot = layerAdjacentToSnapshot.superlayer;
    }

    if (direction == SwipeDirection::Left)
        [snapshotLayerParent insertSublayer:m_swipeSnapshotLayer.get() below:layerAdjacentToSnapshot];
    else
        [snapshotLayerParent insertSublayer:m_swipeSnapshotLayer.get() above:layerAdjacentToSnapshot];
}

void ViewGestureController::handleSwipeGesture(WebBackForwardListItem* targetItem, double progress, SwipeDirection direction)
{
    ASSERT(m_activeGestureType == ViewGestureType::Swipe);

    double width;
    if (!m_customSwipeViews.isEmpty())
        width = m_currentSwipeCustomViewBounds.width();
    else
        width = m_webPageProxy.drawingArea()->size().width();

    double swipingLayerOffset = floor(width * progress);

    if (m_swipeTransitionStyle == SwipeTransitionStyle::Overlap) {
        if (direction == SwipeDirection::Right)
            [m_swipeSnapshotLayer setTransform:CATransform3DMakeTranslation(width + swipingLayerOffset, 0, 0)];
    } else if (m_swipeTransitionStyle == SwipeTransitionStyle::Push)
        [m_swipeSnapshotLayer setTransform:CATransform3DMakeTranslation((direction == SwipeDirection::Left ? -width : width) + swipingLayerOffset, 0, 0)];

    for (const auto& layer : m_currentSwipeLiveLayers) {
        if (m_swipeTransitionStyle == SwipeTransitionStyle::Overlap) {
            if (direction == SwipeDirection::Left)
                [layer setTransform:CATransform3DMakeTranslation(swipingLayerOffset, 0, 0)];
        } else if (m_swipeTransitionStyle == SwipeTransitionStyle::Push)
            [layer setTransform:CATransform3DMakeTranslation(swipingLayerOffset, 0, 0)];
    }
}

void ViewGestureController::endSwipeGesture(WebBackForwardListItem* targetItem, bool cancelled)
{
    ASSERT(m_activeGestureType == ViewGestureType::Swipe);

    CALayer *rootLayer = m_webPageProxy.acceleratedCompositingRootLayer();

    [rootLayer setShadowOpacity:0];
    [rootLayer setShadowRadius:0];

    if (cancelled) {
        removeSwipeSnapshot();
        return;
    }

    uint64_t renderTreeSize = ViewSnapshotStore::shared().snapshotAndRenderTreeSize(targetItem).second;
    m_webPageProxy.process().send(Messages::ViewGestureGeometryCollector::SetRenderTreeSizeNotificationThreshold(renderTreeSize * swipeSnapshotRemovalRenderTreeSizeTargetFraction), m_webPageProxy.pageID());

    // We don't want to replace the current back-forward item's snapshot
    // like we normally would when going back or forward, because we are
    // displaying the destination item's snapshot.
    ViewSnapshotStore::shared().disableSnapshotting();
    m_webPageProxy.goToBackForwardItem(targetItem);
    ViewSnapshotStore::shared().enableSnapshotting();

    if (!renderTreeSize) {
        removeSwipeSnapshot();
        return;
    }

    m_swipeWatchdogTimer.startOneShot(swipeSnapshotRemovalWatchdogDuration.count());
}

void ViewGestureController::didHitRenderTreeSizeThreshold()
{
    removeSwipeSnapshot();
}

void ViewGestureController::swipeSnapshotWatchdogTimerFired(WebCore::Timer<ViewGestureController>*)
{
    removeSwipeSnapshot();
}

void ViewGestureController::removeSwipeSnapshot()
{
    m_swipeWatchdogTimer.stop();

    if (m_activeGestureType != ViewGestureType::Swipe)
        return;

    if (m_currentSwipeSnapshotSurface)
        m_currentSwipeSnapshotSurface->setIsPurgeable(true);
    m_currentSwipeSnapshotSurface = nullptr;

    for (const auto& layer : m_currentSwipeLiveLayers)
        [layer setTransform:CATransform3DIdentity];

    [m_swipeSnapshotLayer removeFromSuperlayer];
    m_swipeSnapshotLayer = nullptr;

    m_currentSwipeLiveLayers.clear();

    m_activeGestureType = ViewGestureType::None;
}

void ViewGestureController::endActiveGesture()
{
    if (m_activeGestureType == ViewGestureType::Magnification) {
        endMagnificationGesture();
        m_visibleContentRectIsValid = false;
    }
}

double ViewGestureController::magnification() const
{
    if (m_activeGestureType == ViewGestureType::Magnification)
        return m_magnification;

    return m_webPageProxy.pageScaleFactor();
}

} // namespace WebKit

#endif // !PLATFORM(IOS)
