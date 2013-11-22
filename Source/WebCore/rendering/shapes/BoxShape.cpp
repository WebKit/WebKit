/*
 * Copyright (C) 2013 Adobe Systems Incorporated. All rights reserved.
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
#include "BoxShape.h"

#include <wtf/MathExtras.h>

namespace WebCore {

BoxShape::BoxShape(const FloatRoundedRect& bounds)
    : Shape()
    , m_bounds(bounds)
    , m_marginBounds(bounds)
    , m_paddingBounds(bounds)
{
    if (shapeMargin() > 0) {
        m_marginBounds.inflate(shapeMargin());
        m_marginBounds.expandRadii(shapeMargin());
    }
    if (shapePadding() > 0) {
        m_paddingBounds.inflate(-shapePadding());
        m_paddingBounds.expandRadii(-shapePadding());
    }
}

void BoxShape::getExcludedIntervals(LayoutUnit logicalTop, LayoutUnit logicalHeight, SegmentList& result) const
{
    if (m_marginBounds.isEmpty())
        return;

    float y1 = logicalTop;
    float y2 = logicalTop + logicalHeight;
    const FloatRect& rect = m_marginBounds.rect();

    if (y2 <= rect.y() || y1 >= rect.maxY())
        return;

    if (!m_marginBounds.isRounded()) {
        result.append(LineSegment(m_marginBounds.rect().x(), m_marginBounds.rect().maxX()));
        return;
    }

    float x1 = rect.maxX();
    float x2 = rect.x();
    float minXIntercept;
    float maxXIntercept;

    if (m_marginBounds.xInterceptsAtY(y1, minXIntercept, maxXIntercept)) {
        x1 = std::min<float>(x1, minXIntercept);
        x2 = std::max<float>(x2, maxXIntercept);
    }

    if (m_marginBounds.xInterceptsAtY(y1, minXIntercept, maxXIntercept)) {
        x1 = std::min<float>(x1, minXIntercept);
        x2 = std::max<float>(x2, maxXIntercept);
    }

    ASSERT(x2 >= x1);
    result.append(LineSegment(x1, x2));
}

void BoxShape::getIncludedIntervals(LayoutUnit logicalTop, LayoutUnit logicalHeight, SegmentList& result) const
{
    if (m_paddingBounds.isEmpty())
        return;

    const FloatRect& rect = m_paddingBounds.rect();
    if (logicalTop < rect.y() || logicalTop + logicalHeight > rect.maxY())
        return;

    // FIXME: this method is only a stub, https://bugs.webkit.org/show_bug.cgi?id=124605.

    result.append(LineSegment(rect.x(), rect.maxX()));
}

bool BoxShape::firstIncludedIntervalLogicalTop(LayoutUnit minLogicalIntervalTop, const LayoutSize&, LayoutUnit& result) const
{
    // FIXME: this method is only a stub, https://bugs.webkit.org/show_bug.cgi?id=124606.

    result = minLogicalIntervalTop;
    return true;
}

void BoxShape::buildPath(Path& path) const
{
    const FloatRect& rect = m_bounds.rect();
    const FloatRoundedRect::Radii& radii = m_bounds.radii();
    path.addRoundedRect(rect, radii.topLeft(), radii.topRight(), radii.bottomLeft(), radii.bottomRight(), Path::PreferBezierRoundedRect);
}

} // namespace WebCore
