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

#include "RenderBox.h"
#include <wtf/MathExtras.h>

namespace WebCore {

static inline LayoutUnit adjustRadiusForMarginBoxShape(LayoutUnit radius, LayoutUnit margin)
{
    // This algorithm is defined in the CSS Shapes specifcation
    if (!margin)
        return radius;

    LayoutUnit ratio = radius / margin;
    if (ratio < 1)
        return LayoutUnit(radius + (margin * (1 + pow(ratio - 1, 3.0))));

    return radius + margin;
}

static inline LayoutSize computeMarginBoxShapeRadius(const LayoutSize& radius, const LayoutSize& adjacentMargins)
{
    return LayoutSize(adjustRadiusForMarginBoxShape(radius.width(), adjacentMargins.width()),
        adjustRadiusForMarginBoxShape(radius.height(), adjacentMargins.height()));
}

static inline RoundedRect::Radii computeMarginBoxShapeRadii(const RoundedRect::Radii& radii, const RenderBox& renderer)
{
    return RoundedRect::Radii(computeMarginBoxShapeRadius(radii.topLeft(), LayoutSize(renderer.marginLeft(), renderer.marginTop())),
        computeMarginBoxShapeRadius(radii.topRight(), LayoutSize(renderer.marginRight(), renderer.marginTop())),
        computeMarginBoxShapeRadius(radii.bottomLeft(), LayoutSize(renderer.marginLeft(), renderer.marginBottom())),
        computeMarginBoxShapeRadius(radii.bottomRight(), LayoutSize(renderer.marginRight(), renderer.marginBottom())));
}

RoundedRect computeRoundedRectForBoxShape(CSSBoxType box, const RenderBox& renderer)
{
    const RenderStyle& style = renderer.style();
    switch (box) {
    case CSSBoxType::MarginBox: {
        if (!style.hasBorderRadius())
            return RoundedRect(renderer.marginBoxRect(), RoundedRect::Radii());

        LayoutRect marginBox = renderer.marginBoxRect();
        RoundedRect::Radii radii = computeMarginBoxShapeRadii(style.getRoundedBorderFor(renderer.borderBoxRect()).radii(), renderer);
        radii.scale(calcBorderRadiiConstraintScaleFor(marginBox, radii));
        return RoundedRect(marginBox, radii);
    }
    case CSSBoxType::PaddingBox:
        return style.getRoundedInnerBorderFor(renderer.borderBoxRect());
    case CSSBoxType::ContentBox:
        return style.getRoundedInnerBorderFor(renderer.borderBoxRect(),
            renderer.paddingTop() + renderer.borderTop(), renderer.paddingBottom() + renderer.borderBottom(),
            renderer.paddingLeft() + renderer.borderLeft(), renderer.paddingRight() + renderer.borderRight());
    // fill, stroke, view-box compute to border-box for HTML elements.
    case CSSBoxType::BorderBox:
    case CSSBoxType::FillBox:
    case CSSBoxType::StrokeBox:
    case CSSBoxType::ViewBox:
    case CSSBoxType::BoxMissing:
        return style.getRoundedBorderFor(renderer.borderBoxRect());
    }

    ASSERT_NOT_REACHED();
    return style.getRoundedBorderFor(renderer.borderBoxRect());
}

LayoutRect BoxShape::shapeMarginLogicalBoundingBox() const
{
    FloatRect marginBounds(m_bounds.rect());
    if (shapeMargin() > 0)
        marginBounds.inflate(shapeMargin());
    return static_cast<LayoutRect>(marginBounds);
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

LineSegment BoxShape::getExcludedInterval(LayoutUnit logicalTop, LayoutUnit logicalHeight) const
{
    const FloatRoundedRect& marginBounds = shapeMarginBounds();
    if (marginBounds.isEmpty() || !lineOverlapsShapeMarginBounds(logicalTop, logicalHeight))
        return LineSegment();

    float y1 = logicalTop;
    float y2 = logicalTop + logicalHeight;
    const FloatRect& rect = marginBounds.rect();

    if (!marginBounds.isRounded())
        return LineSegment(rect.x(), rect.maxX());

    float topCornerMaxY = std::max<float>(marginBounds.topLeftCorner().maxY(), marginBounds.topRightCorner().maxY());
    float bottomCornerMinY = std::min<float>(marginBounds.bottomLeftCorner().y(), marginBounds.bottomRightCorner().y());

    if (topCornerMaxY <= bottomCornerMinY && y1 <= topCornerMaxY && y2 >= bottomCornerMinY)
        return LineSegment(rect.x(), rect.maxX());

    float x1 = rect.maxX();
    float x2 = rect.x();
    float minXIntercept;
    float maxXIntercept;

    if (y1 <= marginBounds.topLeftCorner().maxY() && y2 >= marginBounds.bottomLeftCorner().y())
        x1 = rect.x();

    if (y1 <= marginBounds.topRightCorner().maxY() && y2 >= marginBounds.bottomRightCorner().y())
        x2 = rect.maxX();

    if (marginBounds.xInterceptsAtY(y1, minXIntercept, maxXIntercept)) {
        x1 = std::min<float>(x1, minXIntercept);
        x2 = std::max<float>(x2, maxXIntercept);
    }

    if (marginBounds.xInterceptsAtY(y2, minXIntercept, maxXIntercept)) {
        x1 = std::min<float>(x1, minXIntercept);
        x2 = std::max<float>(x2, maxXIntercept);
    }

    ASSERT(x2 >= x1);
    return LineSegment(x1, x2);
}

void BoxShape::buildDisplayPaths(DisplayPaths& paths) const
{
    paths.shape.addRoundedRect(m_bounds, Path::PreferBezierRoundedRect);
    if (shapeMargin())
        paths.marginShape.addRoundedRect(shapeMarginBounds(), Path::PreferBezierRoundedRect);
}

} // namespace WebCore
