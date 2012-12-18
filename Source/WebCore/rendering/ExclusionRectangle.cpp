/*
 * Copyright (C) 2012 Adobe Systems Incorporated. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ExclusionRectangle.h"

#include <wtf/MathExtras.h>

namespace WebCore {

static inline float ellipseXIntercept(float y, float rx, float ry)
{
    ASSERT(ry > 0);
    return rx * sqrt(1 - (y * y) / (ry * ry));
}

static inline float ellipseYIntercept(float x, float rx, float ry)
{
    ASSERT(rx > 0);
    return ry * sqrt(1 - (x * x) / (rx * rx));
}

void ExclusionRectangle::getExcludedIntervals(float logicalTop, float logicalHeight, SegmentList& result) const
{
    if (isEmpty())
        return;

    float y1 = logicalTop;
    float y2 = y1 + logicalHeight;

    if (y2 < m_y || y1 >= m_y + m_height)
        return;

    float x1 = m_x;
    float x2 = m_x + m_width;

    if (m_ry > 0) {
        if (y2 < m_y + m_ry) {
            float yi = y2 - m_y - m_ry;
            float xi = ellipseXIntercept(yi, m_rx, m_ry);
            x1 = m_x + m_rx - xi;
            x2 = m_x + m_width - m_rx + xi;
        } else if (y1 > m_y + m_height - m_ry) {
            float yi =  y1 - (m_y + m_height - m_ry);
            float xi = ellipseXIntercept(yi, m_rx, m_ry);
            x1 = m_x + m_rx - xi;
            x2 = m_x + m_width - m_rx + xi;
        }
    }

    result.append(LineSegment(x1, x2));
}

void ExclusionRectangle::getIncludedIntervals(float logicalTop, float logicalHeight, SegmentList& result) const
{
    if (isEmpty())
        return;

    float y1 = logicalTop;
    float y2 = y1 + logicalHeight;

    if (y1 < m_y || y2 > m_y + m_height)
        return;

    float x1 = m_x;
    float x2 = m_x + m_width;

    if (m_ry > 0) {
        bool y1InterceptsCorner = y1 < m_y + m_ry;
        bool y2InterceptsCorner = y2 > m_y + m_height - m_ry;
        float xi = 0;

        if (y1InterceptsCorner && y2InterceptsCorner) {
            if  (y1 < m_height + 2*m_y - y2) {
                float yi = y1 - m_y - m_ry;
                xi = ellipseXIntercept(yi, m_rx, m_ry);
            } else {
                float yi =  y2 - (m_y + m_height - m_ry);
                xi = ellipseXIntercept(yi, m_rx, m_ry);
            }
        } else if (y1InterceptsCorner) {
            float yi = y1 - m_y - m_ry;
            xi = ellipseXIntercept(yi, m_rx, m_ry);
        } else if (y2InterceptsCorner) {
            float yi =  y2 - (m_y + m_height - m_ry);
            xi = ellipseXIntercept(yi, m_rx, m_ry);
        }

        if (y1InterceptsCorner || y2InterceptsCorner) {
            x1 = m_x + m_rx - xi;
            x2 = m_x + m_width - m_rx + xi;
        }
    }

    result.append(LineSegment(x1, x2));
}

FloatPoint ExclusionRectangle::cornerInterceptForWidth(float width) const
{
    float xi = (m_width - width) / 2;
    float yi = m_ry - ellipseYIntercept(m_rx - xi, m_rx, m_ry);
    return FloatPoint(xi, yi);
}

bool ExclusionRectangle::firstIncludedIntervalLogicalTop(float minLogicalIntervalTop, const FloatSize& minLogicalIntervalSize, float& result) const
{
    if (minLogicalIntervalSize.width() > m_width)
        return false;

    float minY = std::max(m_y, minLogicalIntervalTop);
    float maxY = minY + minLogicalIntervalSize.height();

    if (maxY > m_y + m_height)
        return false;

    bool intervalOverlapsMinCorner = minY < m_y + m_ry;
    bool intervalOverlapsMaxCorner = maxY > m_y + m_height - m_ry;

    if (!intervalOverlapsMinCorner && !intervalOverlapsMaxCorner) {
        result = minY;
        return true;
    }

    float centerY = m_y + m_height / 2;
    bool minCornerDefinesX = fabs(centerY - minY) > fabs(centerY - maxY);
    bool intervalFitsWithinCorners = minLogicalIntervalSize.width() + 2 * m_rx <= m_width;
    FloatPoint cornerIntercept = cornerInterceptForWidth(minLogicalIntervalSize.width());

    if (intervalOverlapsMinCorner && (!intervalOverlapsMaxCorner || minCornerDefinesX)) {
        if (intervalFitsWithinCorners || m_y + cornerIntercept.y() < minY) {
            result = minY;
            return true;
        }
        if (minLogicalIntervalSize.height() < m_height - (2 * cornerIntercept.y())) {
            result = m_y + cornerIntercept.y();
            return true;
        }
    }

    if (intervalOverlapsMaxCorner && (!intervalOverlapsMinCorner || !minCornerDefinesX)) {
        if (intervalFitsWithinCorners || minY <=  m_y + m_height - cornerIntercept.y() - minLogicalIntervalSize.height()) {
            result = minY;
            return true;
        }
    }

    return false;
}

} // namespace WebCore
