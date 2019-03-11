/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#import "SmartMagnificationController.h"

#if PLATFORM(IOS_FAMILY)

#import "SmartMagnificationControllerMessages.h"
#import "ViewGestureGeometryCollectorMessages.h"
#import "WKContentView.h"
#import "WKScrollView.h"
#import "WebPageGroup.h"
#import "WebPageMessages.h"
#import "WebPageProxy.h"
#import "WebProcessProxy.h"

ALLOW_DEPRECATED_DECLARATIONS_BEGIN

#import "UIKitSPI.h"

ALLOW_DEPRECATED_DECLARATIONS_END

static const float smartMagnificationPanScrollThresholdZoomedOut = 60;
static const float smartMagnificationPanScrollThresholdIPhone = 100;
static const float smartMagnificationPanScrollThresholdIPad = 150;
static const float smartMagnificationElementPadding = 0.05;

static const double smartMagnificationMaximumScale = 1.6;
static const double smartMagnificationMinimumScale = 0;

namespace WebKit {
using namespace WebCore;

SmartMagnificationController::SmartMagnificationController(WKContentView *contentView)
    : m_webPageProxy(*contentView.page)
    , m_contentView(contentView)
{
    m_webPageProxy.process().addMessageReceiver(Messages::SmartMagnificationController::messageReceiverName(), m_webPageProxy.pageID(), *this);
}

SmartMagnificationController::~SmartMagnificationController()
{
    m_webPageProxy.process().removeMessageReceiver(Messages::SmartMagnificationController::messageReceiverName(), m_webPageProxy.pageID());
}

void SmartMagnificationController::handleSmartMagnificationGesture(FloatPoint origin)
{
    m_webPageProxy.process().send(Messages::ViewGestureGeometryCollector::CollectGeometryForSmartMagnificationGesture(origin), m_webPageProxy.pageID());
}

void SmartMagnificationController::handleResetMagnificationGesture(FloatPoint origin)
{
    [m_contentView _zoomOutWithOrigin:origin];
}

std::tuple<FloatRect, double, double> SmartMagnificationController::smartMagnificationTargetRectAndZoomScales(FloatRect targetRect, double minimumScale, double maximumScale, bool addMagnificationPadding)
{
    FloatRect outTargetRect = targetRect;
    double outMinimumScale = minimumScale;
    double outMaximumScale = maximumScale;

    if (addMagnificationPadding) {
        outTargetRect.inflateX(smartMagnificationElementPadding * outTargetRect.width());
        outTargetRect.inflateY(smartMagnificationElementPadding * outTargetRect.height());
    }

    outMinimumScale = std::max(outMinimumScale, smartMagnificationMinimumScale);
    outMaximumScale = std::min(outMaximumScale, smartMagnificationMaximumScale);

    return { outTargetRect, outMinimumScale, outMaximumScale };
}

double SmartMagnificationController::zoomFactorForTargetRect(FloatRect targetRect, bool fitEntireRect, double viewportMinimumScale, double viewportMaximumScale)
{
    // FIXME: Share some of this code with didCollectGeometryForSmartMagnificationGesture?

    FloatRect adjustedTargetRect;
    double minimumScale = viewportMinimumScale;
    double maximumScale = viewportMaximumScale;
    std::tie(adjustedTargetRect, minimumScale, maximumScale) = smartMagnificationTargetRectAndZoomScales(targetRect, viewportMinimumScale, viewportMaximumScale, !fitEntireRect);

    double currentScale = [m_contentView _contentZoomScale];
    double targetScale = [m_contentView _targetContentZoomScaleForRect:adjustedTargetRect currentScale:currentScale fitEntireRect:fitEntireRect minimumScale:minimumScale maximumScale:maximumScale];

    if (targetScale == currentScale)
        targetScale = [m_contentView _initialScaleFactor];

    return targetScale;
}

void SmartMagnificationController::didCollectGeometryForSmartMagnificationGesture(FloatPoint origin, FloatRect targetRect, FloatRect visibleContentRect, bool fitEntireRect, double viewportMinimumScale, double viewportMaximumScale)
{
    if (targetRect.isEmpty()) {
        // FIXME: If we don't zoom, send the tap along to text selection (see <rdar://problem/6810344>).
        [m_contentView _zoomToInitialScaleWithOrigin:origin];
        return;
    }
    FloatRect adjustedTargetRect;
    double minimumScale = viewportMinimumScale;
    double maximumScale = viewportMaximumScale;
    std::tie(adjustedTargetRect, minimumScale, maximumScale) = smartMagnificationTargetRectAndZoomScales(targetRect, viewportMinimumScale, viewportMaximumScale, !fitEntireRect);

    // FIXME: Check if text selection wants to consume the double tap before we attempt magnification.

    // If the content already fits in the scroll view and we're already zoomed in to the target scale,
    // it is most likely that the user intended to scroll, so use a small distance threshold to initiate panning.
    float minimumScrollDistance;
    if ([m_contentView bounds].size.width <= m_webPageProxy.unobscuredContentRect().width())
        minimumScrollDistance = smartMagnificationPanScrollThresholdZoomedOut;
    else if (currentUserInterfaceIdiomIsPad())
        minimumScrollDistance = smartMagnificationPanScrollThresholdIPad;
    else
        minimumScrollDistance = smartMagnificationPanScrollThresholdIPhone;

    // For replaced elements like images, we want to fit the whole element
    // in the view, so scale it down enough to make both dimensions fit if possible.
    // For other elements, try to fit them horizontally.
    if ([m_contentView _zoomToRect:adjustedTargetRect withOrigin:origin fitEntireRect:fitEntireRect minimumScale:minimumScale maximumScale:maximumScale minimumScrollDistance:minimumScrollDistance])
        return;

    // FIXME: If we still don't zoom, send the tap along to text selection (see <rdar://problem/6810344>).
    [m_contentView _zoomToInitialScaleWithOrigin:origin];
}

void SmartMagnificationController::magnify(FloatPoint origin, FloatRect targetRect, FloatRect visibleContentRect, double viewportMinimumScale, double viewportMaximumScale)
{
    FloatRect adjustedTargetRect;
    double maximumScale = viewportMaximumScale;
    double minimumScale = viewportMinimumScale;
    std::tie(adjustedTargetRect, minimumScale, maximumScale) = smartMagnificationTargetRectAndZoomScales(targetRect, viewportMinimumScale, viewportMaximumScale, true);

    [m_contentView _zoomToRect:adjustedTargetRect withOrigin:origin fitEntireRect:NO minimumScale:minimumScale maximumScale:maximumScale minimumScrollDistance:0];
}

void SmartMagnificationController::scrollToRect(FloatPoint origin, FloatRect targetRect)
{
    [m_contentView _scrollToRect:targetRect withOrigin:origin minimumScrollDistance:0];
}

} // namespace WebKit

#endif // PLATFORM(IOS_FAMILY)
