/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#import "ViewGestureControllerMessages.h"
#import "ViewGestureGeometryCollectorMessages.h"
#import "WebPageProxy.h"
#import "WebProcessProxy.h"

using namespace WebCore;

static const double minMagnification = 1;
static const double maxMagnification = 3;

static const double minElasticMagnification = 0.75;
static const double maxElasticMagnification = 4;

static const double zoomOutBoost = 1.6;
static const double zoomOutResistance = 0.10;

static const float smartMagnificationElementPadding = 0.05;
static const float smartMagnificationPanScrollThreshold = 100;

namespace WebKit {

ViewGestureController::ViewGestureController(WebPageProxy& webPageProxy)
    : m_webPageProxy(webPageProxy)
    , m_lastSmartMagnificationUnscaledTargetRectIsValid(false)
    , m_activeGestureType(ViewGestureType::None)
    , m_visibleContentRectIsValid(false)
    , m_frameHandlesMagnificationGesture(false)
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
    double scaleDistance = abs(limit - currentScale);
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

void ViewGestureController::didCollectGeometryForSmartMagnificationGesture(FloatPoint origin, FloatRect renderRect, FloatRect visibleContentRect, bool isReplacedElement, bool frameHandlesMagnificationGesture)
{
    if (frameHandlesMagnificationGesture)
        return;

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
    // similar to the last one, in which case we'll zoom all the way out.
    if (currentScaleFactor > 1
        && m_lastSmartMagnificationUnscaledTargetRectIsValid
        && maximumRectangleComponentDelta(m_lastSmartMagnificationUnscaledTargetRect, unscaledTargetRect) < smartMagnificationPanScrollThreshold)
        targetMagnification = 1;

    FloatRect targetRect(unscaledTargetRect);
    targetRect.scale(targetMagnification);
    FloatPoint targetOrigin(visibleContentRect.center());
    targetOrigin.moveBy(-targetRect.center());

    m_webPageProxy.drawingArea()->adjustTransientZoom(m_webPageProxy.pageScaleFactor(), scaledMagnificationOrigin(FloatPoint(), m_webPageProxy.pageScaleFactor()));
    m_webPageProxy.drawingArea()->commitTransientZoom(targetMagnification, targetOrigin);

    m_lastSmartMagnificationUnscaledTargetRect = unscaledTargetRect;
    m_lastSmartMagnificationUnscaledTargetRectIsValid = true;
}

void ViewGestureController::endActiveGesture()
{
    switch (m_activeGestureType) {
    case ViewGestureType::None:
    case ViewGestureType::SmartMagnification:
        break;
    case ViewGestureType::Magnification:
        endMagnificationGesture();
    }

    m_visibleContentRectIsValid = false;
    m_activeGestureType = ViewGestureType::None;
}

double ViewGestureController::magnification() const
{
    if (m_activeGestureType == ViewGestureType::Magnification)
        return m_magnification;

    return m_webPageProxy.pageScaleFactor();
}

} // namespace WebKit
