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

LayoutRect BoxShape::shapeMarginLogicalBoundingBox() const
{
    FloatRect marginBounds(m_bounds.rect());
    if (shapeMargin() > 0)
        marginBounds.inflate(shapeMargin());
    return static_cast<LayoutRect>(marginBounds);
}

LayoutRect BoxShape::shapePaddingLogicalBoundingBox() const
{
    FloatRect paddingBounds(m_bounds.rect());
    if (shapePadding() > 0)
        paddingBounds.inflate(-shapePadding());
    return static_cast<LayoutRect>(paddingBounds);
}

FloatRoundedRect BoxShape::shapeMarginBounds() const
{
    FloatRoundedRect marginBounds(m_bounds);
    if (shapeMargin() > 0) {
        marginBounds.inflate(shapeMargin());
        marginBounds.expandRadii(shapeMargin());
    }
    return marginBounds;
}

FloatRoundedRect BoxShape::shapePaddingBounds() const
{
    FloatRoundedRect paddingBounds(m_bounds);
    if (shapePadding() > 0) {
        paddingBounds.inflate(-shapePadding());
        paddingBounds.expandRadii(-shapePadding());
    }
    return paddingBounds;
}

void BoxShape::getExcludedIntervals(LayoutUnit logicalTop, LayoutUnit logicalHeight, SegmentList& result) const
{
    const FloatRoundedRect& marginBounds = shapeMarginBounds();
    if (marginBounds.isEmpty() || !lineOverlapsShapeMarginBounds(logicalTop, logicalHeight))
        return;

    float y1 = logicalTop;
    float y2 = logicalTop + logicalHeight;
    const FloatRect& rect = marginBounds.rect();

    if (!marginBounds.isRounded()) {
        result.append(LineSegment(rect.x(), rect.maxX()));
        return;
    }

    float topCornerMaxY = std::max<float>(marginBounds.topLeftCorner().maxY(), marginBounds.topRightCorner().maxY());
    float bottomCornerMinY = std::min<float>(marginBounds.bottomLeftCorner().y(), marginBounds.bottomRightCorner().y());

    if (y1 <= topCornerMaxY && y2 >= bottomCornerMinY) {
        result.append(LineSegment(rect.x(), rect.maxX()));
        return;
    }

    float x1 = rect.maxX();
    float x2 = rect.x();
    float minXIntercept;
    float maxXIntercept;

    if (marginBounds.xInterceptsAtY(y1, minXIntercept, maxXIntercept)) {
        x1 = std::min<float>(x1, minXIntercept);
        x2 = std::max<float>(x2, maxXIntercept);
    }

    if (marginBounds.xInterceptsAtY(y2, minXIntercept, maxXIntercept)) {
        x1 = std::min<float>(x1, minXIntercept);
        x2 = std::max<float>(x2, maxXIntercept);
    }

    ASSERT(x2 >= x1);
    result.append(LineSegment(x1, x2));
}

void BoxShape::getIncludedIntervals(LayoutUnit logicalTop, LayoutUnit logicalHeight, SegmentList& result) const
{
    const FloatRoundedRect& paddingBounds = shapePaddingBounds();
    if (paddingBounds.isEmpty())
        return;

    const FloatRect& rect = paddingBounds.rect();
    float y1 = logicalTop;
    float y2 = logicalTop + logicalHeight;

    if (y1 < rect.y() || y2 > rect.maxY())
        return;

    if (!paddingBounds.isRounded()) {
        result.append(LineSegment(rect.x(), rect.maxX()));
        return;
    }

    float x1 = rect.x();
    float x2 = rect.maxX();
    float minXIntercept;
    float maxXIntercept;

    if (paddingBounds.xInterceptsAtY(y1, minXIntercept, maxXIntercept)) {
        x1 = std::max<float>(x1, minXIntercept);
        x2 = std::min<float>(x2, maxXIntercept);
    }

    if (paddingBounds.xInterceptsAtY(y2, minXIntercept, maxXIntercept)) {
        x1 = std::max<float>(x1, minXIntercept);
        x2 = std::min<float>(x2, maxXIntercept);
    }

    result.append(LineSegment(x1, x2));
}

bool BoxShape::firstIncludedIntervalLogicalTop(LayoutUnit minLogicalIntervalTop, const FloatSize&, LayoutUnit& result) const
{
    // FIXME: this method is only a stub, https://bugs.webkit.org/show_bug.cgi?id=124606.

    result = minLogicalIntervalTop;
    return true;
}

void BoxShape::buildDisplayPaths(DisplayPaths& paths) const
{
    paths.shape.addRoundedRect(m_bounds, Path::PreferBezierRoundedRect);
    if (shapeMargin())
        paths.marginShape.addRoundedRect(shapeMarginBounds(), Path::PreferBezierRoundedRect);
}

} // namespace WebCore
