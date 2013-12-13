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
#import "WebPageProxy.h"
#import "WebProcessProxy.h"

using namespace WebCore;

static const CGFloat minMagnification = 0.75;
static const CGFloat maxMagnification = 3;

static const CGFloat zoomOutBoost = 1.6;
static const CGFloat zoomOutResistance = 0.10;

namespace WebKit {

ViewGestureController::ViewGestureController(WebPageProxy& webPageProxy)
    : m_webPageProxy(webPageProxy)
    , m_activeGestureType(ViewGestureType::None)
    , m_visibleContentRectIsValid(false)
{
    m_webPageProxy.process().addMessageReceiver(Messages::ViewGestureController::messageReceiverName(), m_webPageProxy.pageID(), *this);
}

ViewGestureController::~ViewGestureController()
{
    m_webPageProxy.process().removeMessageReceiver(Messages::ViewGestureController::messageReceiverName(), m_webPageProxy.pageID());
}

static double resistanceForDelta(double deltaScale, double currentScale)
{
    // Zoom in with no acceleration.
    // FIXME: Should have resistance when zooming in past the limit.
    if (deltaScale > 0)
        return 1;

    // Zoom out with slight acceleration, until we reach unit scale.
    if (currentScale > 1)
        return zoomOutBoost;

    // Beyond scale=1, resist zooming out.
    double scaleDistance = minMagnification - currentScale;
    double scalePercent = std::min(std::max(scaleDistance / minMagnification, 0.), 1.);
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

void ViewGestureController::didBeginTransientZoom(FloatRect visibleContentRect)
{
    m_activeGestureType = ViewGestureType::Magnification;
    m_visibleContentRect = visibleContentRect;
    m_visibleContentRectIsValid = true;
}

void ViewGestureController::handleMagnificationGesture(double scale, FloatPoint origin)
{
    ASSERT(m_activeGestureType == ViewGestureType::None || m_activeGestureType == ViewGestureType::Magnification);

    if (m_activeGestureType == ViewGestureType::None) {
        // FIXME: We drop the first frame of the gesture on the floor, because we don't have the visible content bounds yet.
        m_magnification = m_webPageProxy.pageScaleFactor();
        m_webPageProxy.drawingArea()->beginTransientZoom();
        return;
    }

    // We're still waiting for the DidBeginTransientZoom callback.
    if (!m_visibleContentRectIsValid)
        return;

    double scaleWithResistance = resistanceForDelta(scale, m_magnification) * scale;

    m_magnification += m_magnification * scaleWithResistance;
    m_magnification = std::min(std::max(m_magnification, minMagnification), maxMagnification);

    m_magnificationOrigin = origin;

    m_webPageProxy.drawingArea()->adjustTransientZoom(m_magnification, scaledMagnificationOrigin(origin, m_magnification));

    m_activeGestureType = ViewGestureType::Magnification;
}

void ViewGestureController::endMagnificationGesture()
{
    ASSERT(m_activeGestureType == ViewGestureType::Magnification);

    // FIXME: Should rubber-band back when zoomed in or out past the limit.
    double newMagnification = std::max(m_magnification, 1.);

    FloatPoint scaledOrigin = scaledMagnificationOrigin(m_magnificationOrigin, newMagnification);
    scaledOrigin.moveBy(-m_visibleContentRect.location());

    m_webPageProxy.drawingArea()->commitTransientZoom(newMagnification, -scaledOrigin);
}

void ViewGestureController::endActiveGesture()
{
    switch (m_activeGestureType) {
    case ViewGestureType::None:
        break;
    case ViewGestureType::Magnification:
        endMagnificationGesture();
    }

    m_visibleContentRectIsValid = false;
    m_activeGestureType = ViewGestureType::None;
}

} // namespace WebKit
