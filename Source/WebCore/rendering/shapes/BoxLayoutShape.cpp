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
#include "BoxLayoutShape.h"

#include "BorderShape.h"
#include "Path.h"
#include "RenderBoxInlines.h"
#include <limits>
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

static inline LayoutRoundedRect::Radii computeMarginBoxShapeRadii(const LayoutRoundedRect::Radii& radii, const RenderBox& renderer)
{
    return LayoutRoundedRect::Radii(computeMarginBoxShapeRadius(radii.topLeft(), LayoutSize(renderer.marginLeft(), renderer.marginTop())),
        computeMarginBoxShapeRadius(radii.topRight(), LayoutSize(renderer.marginRight(), renderer.marginTop())),
        computeMarginBoxShapeRadius(radii.bottomLeft(), LayoutSize(renderer.marginLeft(), renderer.marginBottom())),
        computeMarginBoxShapeRadius(radii.bottomRight(), LayoutSize(renderer.marginRight(), renderer.marginBottom())));
}

static BorderShape marginBoxBorderShape(const Style::ComputedStyle& style, const RenderBox& renderer)
{
    auto marginBox = renderer.marginBoxRect();
    auto borderShape = BorderShape::shapeForBorderRect(style, renderer.borderBoxRect());
    auto radii = computeMarginBoxShapeRadii(borderShape.radii(), renderer);
    radii.scale(calcBorderRadiiConstraintScaleFor(marginBox, radii));
    if (!radii.areRenderableInRect(marginBox))
        radii.makeRenderableInRect(marginBox);
    return BorderShape { marginBox, { }, radii, borderShape.cornerCurvatures() };
}

LayoutRoundedRect computeRoundedRectForBoxShape(CSSBoxType box, const RenderBox& renderer)
{
    CheckedRef style = renderer.style();
    switch (box) {
    case CSSBoxType::MarginBox: {
        if (!style->border().hasBorderRadius())
            return LayoutRoundedRect(renderer.marginBoxRect(), LayoutRoundedRect::Radii());
        return marginBoxBorderShape(style, renderer).shapedRectForOuterShape();
    }
    case CSSBoxType::PaddingBox:
        return BorderShape::shapeForBorderRect(style, renderer.borderBoxRect()).shapedRectForInnerShape();
    // fill-box compute to content-box for HTML elements.
    case CSSBoxType::FillBox:
    case CSSBoxType::ContentBox: {
        auto borderShape = renderer.borderShapeForContentClipping(renderer.borderBoxRect());
        return borderShape.shapedRectForInnerShape();
    }
    // stroke-box, view-box compute to border-box for HTML elements.
    case CSSBoxType::BorderBox:
    case CSSBoxType::StrokeBox:
    case CSSBoxType::ViewBox:
    case CSSBoxType::BoxMissing:
        return BorderShape::shapeForBorderRect(style, renderer.borderBoxRect()).shapedRectForOuterShape();
    }

    ASSERT_NOT_REACHED();
    return BorderShape::shapeForBorderRect(style, renderer.borderBoxRect()).shapedRectForOuterShape();
}

std::optional<Path> computePathForBoxShape(CSSBoxType box, const RenderBox& renderer, float deviceScaleFactor)
{
    CheckedRef style = renderer.style();
    if (!style->border().hasBorderRadius())
        return std::nullopt;

    auto pathIfNotRounded = [&](const BorderShape& borderShape, bool inner) -> std::optional<Path> {
        if (!borderShape.hasNonRoundCornerShape())
            return std::nullopt;
        return inner ? borderShape.pathForInnerShape(deviceScaleFactor) : borderShape.pathForOuterShape(deviceScaleFactor);
    };

    switch (box) {
    case CSSBoxType::PaddingBox:
        return pathIfNotRounded(BorderShape::shapeForBorderRect(style, renderer.borderBoxRect()), true);
    case CSSBoxType::FillBox:
    case CSSBoxType::ContentBox:
        return pathIfNotRounded(renderer.borderShapeForContentClipping(renderer.borderBoxRect()), true);
    case CSSBoxType::BorderBox:
    case CSSBoxType::StrokeBox:
    case CSSBoxType::ViewBox:
    case CSSBoxType::BoxMissing:
        return pathIfNotRounded(BorderShape::shapeForBorderRect(style, renderer.borderBoxRect()), false);
    case CSSBoxType::MarginBox:
        return pathIfNotRounded(marginBoxBorderShape(style, renderer), false);
    }

    ASSERT_NOT_REACHED();
    return std::nullopt;
}

LayoutRect BoxLayoutShape::shapeMarginLogicalBoundingBox() const
{
    FloatRect marginBounds(m_bounds.rect());
    if (shapeMargin() > 0)
        marginBounds.inflate(shapeMargin());
    return static_cast<LayoutRect>(marginBounds);
}

FloatRoundedRect BoxLayoutShape::shapeMarginBounds() const
{
    auto shapeMargin = this->shapeMargin();
    if (!shapeMargin)
        return m_bounds;

    auto marginBounds = FloatRoundedRect { m_bounds };
    marginBounds.inflate(shapeMargin);
    auto expandedRadii = marginBounds.radii();
    expandedRadii.expandEvenIfZero(shapeMargin);
    marginBounds.setRadii(expandedRadii);
    return marginBounds;
}

static std::optional<LineSegment> contourExtentBetween(const Vector<FloatPoint>& contour, float bandTop, float bandBottom)
{
    float minimumX = std::numeric_limits<float>::max();
    float maximumX = std::numeric_limits<float>::lowest();
    auto include = [&](float candidateX) {
        minimumX = std::min(minimumX, candidateX);
        maximumX = std::max(maximumX, candidateX);
    };

    for (size_t index = 0; index + 1 < contour.size(); ++index) {
        auto start = contour[index];
        auto end = contour[index + 1];

        if (start.y() >= bandTop && start.y() <= bandBottom)
            include(start.x());

        if (start.y() == end.y())
            continue;

        for (float bandEdge : { bandTop, bandBottom }) {
            if (bandEdge < std::min(start.y(), end.y()) || bandEdge > std::max(start.y(), end.y()))
                continue;
            auto fraction = (bandEdge - start.y()) / (end.y() - start.y());
            include(start.x() + fraction * (end.x() - start.x()));
        }
    }

    if (minimumX > maximumX)
        return std::nullopt;
    return LineSegment(minimumX, maximumX);
}

LineSegment BoxLayoutShape::getExcludedInterval(LayoutUnit logicalTop, LayoutUnit logicalHeight) const
{
    const FloatRoundedRect& marginBounds = shapeMarginBounds();
    if (marginBounds.isEmpty() || !lineOverlapsShapeMarginBounds(logicalTop, logicalHeight))
        return LineSegment();

    float y1 = logicalTop;
    float y2 = logicalTop + logicalHeight;
    const FloatRect& rect = marginBounds.rect();

    if (!m_contour.isEmpty()) {
        if (auto extent = contourExtentBetween(m_contour, y1, y2))
            return *extent;
        return LineSegment();
    }

    if (!marginBounds.hasNonZeroRadii())
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

void BoxLayoutShape::buildDisplayPaths(DisplayPaths& paths) const
{
    paths.shape.addRoundedRect(m_bounds, PathRoundedRect::Strategy::PreferBezier);
    if (shapeMargin())
        paths.marginShape.addRoundedRect(shapeMarginBounds(), PathRoundedRect::Strategy::PreferBezier);
}

} // namespace WebCore
