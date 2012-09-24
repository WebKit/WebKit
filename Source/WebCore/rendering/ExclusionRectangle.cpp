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
    return rx * sqrt(1 - (y*y) / (ry*ry));
}

void ExclusionRectangle::getExcludedIntervals(float logicalTop, float logicalBottom, SegmentList& result) const
{
    float y1 = minYForLogicalLine(logicalTop, logicalBottom);
    float y2 = maxYForLogicalLine(logicalTop, logicalBottom);

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

void ExclusionRectangle::getIncludedIntervals(float logicalTop, float logicalBottom, SegmentList& result) const
{
    float y1 = minYForLogicalLine(logicalTop, logicalBottom);
    float y2 = maxYForLogicalLine(logicalTop, logicalBottom);

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

} // namespace WebCore
