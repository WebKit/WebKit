/*
 * Copyright (C) 2011, 2012 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2011 Benjamin Poulain <benjamin@webkit.org>
 * Copyright (C) 2014 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this program; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "SimpleViewportController.h"

#if USE(COORDINATED_GRAPHICS)

namespace WebKit {
using namespace WebCore;

SimpleViewportController::SimpleViewportController(const IntSize& size)
    : m_viewportSize(size)
{
    resetViewportToDefaultState();
}

void SimpleViewportController::didChangeViewportSize(const IntSize& newSize)
{
    if (newSize.isEmpty())
        return;

    m_viewportSize = newSize;
    updateMinimumScaleToFit();
}

void SimpleViewportController::didChangeContentsSize(const IntSize& newSize)
{
    m_contentsSize = newSize;

    updateMinimumScaleToFit();

    if (m_initiallyFitToViewport) {
        // Restrict scale factors to m_minimumScaleToFit.
        ASSERT(m_minimumScaleToFit > 0);
        m_rawAttributes.initialScale = m_minimumScaleToFit;
        restrictScaleFactorToInitialScaleIfNotUserScalable(m_rawAttributes);
    }
}

void SimpleViewportController::didChangeViewportAttributes(ViewportAttributes&& newAttributes)
{
    if (newAttributes.layoutSize.isEmpty()) {
        resetViewportToDefaultState();
        return;
    }

    m_hasViewportAttribute = true;

    m_rawAttributes = WTFMove(newAttributes);
    m_allowsUserScaling = m_rawAttributes.userScalable;
    m_initiallyFitToViewport = m_rawAttributes.initialScale < 0;

    if (!m_initiallyFitToViewport)
        restrictScaleFactorToInitialScaleIfNotUserScalable(m_rawAttributes);

    updateMinimumScaleToFit();
}

void SimpleViewportController::didScroll(const IntPoint& position)
{
    m_contentsPosition = position;
}

FloatRect SimpleViewportController::visibleContentsRect() const
{
    if (m_viewportSize.isEmpty() || m_contentsSize.isEmpty())
        return { };

    FloatRect visibleContentsRect(boundContentsPosition(m_contentsPosition), visibleContentsSize());
    visibleContentsRect.intersect(FloatRect(FloatPoint::zero(), m_contentsSize));
    return visibleContentsRect;
}

FloatSize SimpleViewportController::visibleContentsSize() const
{
    return FloatSize(m_viewportSize.width() / m_pageScaleFactor, m_viewportSize.height() / m_pageScaleFactor);
}

FloatPoint SimpleViewportController::boundContentsPositionAtScale(const FloatPoint& framePosition, float scale) const
{
    // We need to floor the viewport here as to allow aligning the content in device units. If not,
    // it might not be possible to scroll the last pixel and that affects fixed position elements.
    return FloatPoint(
        clampTo(framePosition.x(), .0f, std::max(.0f, m_contentsSize.width() - floorf(m_viewportSize.width() / scale))),
        clampTo(framePosition.y(), .0f, std::max(.0f, m_contentsSize.height() - floorf(m_viewportSize.height() / scale))));
}

FloatPoint SimpleViewportController::boundContentsPosition(const FloatPoint& framePosition) const
{
    return boundContentsPositionAtScale(framePosition, m_pageScaleFactor);
}

bool fuzzyCompare(float a, float b, float epsilon)
{
    return std::abs(a - b) < epsilon;
}

bool SimpleViewportController::updateMinimumScaleToFit()
{
    if (m_viewportSize.isEmpty() || m_contentsSize.isEmpty() || !m_hasViewportAttribute)
        return false;

    bool currentlyScaledToFit = fuzzyCompare(m_pageScaleFactor, m_minimumScaleToFit, 0.0001);

    float minimumScale = computeMinimumScaleFactorForContentContained(m_rawAttributes, roundedIntSize(m_viewportSize), roundedIntSize(m_contentsSize));

    if (minimumScale <= 0)
        return false;

    if (!fuzzyCompare(minimumScale, m_minimumScaleToFit, 0.0001)) {
        m_minimumScaleToFit = minimumScale;

        if (currentlyScaledToFit)
            m_pageScaleFactor = m_minimumScaleToFit;
        else {
            // Ensure the effective scale stays within bounds.
            float boundedScale = innerBoundedViewportScale(m_pageScaleFactor);
            if (!fuzzyCompare(boundedScale, m_pageScaleFactor, 0.0001))
                m_pageScaleFactor = boundedScale;
        }

        return true;
    }
    return false;
}

float SimpleViewportController::innerBoundedViewportScale(float viewportScale) const
{
    return clampTo(viewportScale, m_minimumScaleToFit, m_rawAttributes.maximumScale);
}

void SimpleViewportController::resetViewportToDefaultState()
{
    m_hasViewportAttribute = false;
    m_pageScaleFactor = 1;
    m_minimumScaleToFit = 1;

    // Initializing Viewport Raw Attributes to avoid random negative or infinity scale factors
    // if there is a race condition between the first layout and setting the viewport attributes for the first time.
    m_rawAttributes.minimumScale = 1;
    m_rawAttributes.maximumScale = 1;
    m_rawAttributes.userScalable = m_allowsUserScaling;

    // The initial scale might be implicit and set to -1, in this case we have to infer it
    // using the viewport size and the final layout size.
    // To be able to assert for valid scale we initialize it to -1.
    m_rawAttributes.initialScale = -1;
}

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS)
