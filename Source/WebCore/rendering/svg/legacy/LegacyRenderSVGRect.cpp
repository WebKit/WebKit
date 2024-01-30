/*
 * Copyright (C) 2011 University of Szeged
 * Copyright (C) 2011 Renata Hodovan <reni@webkit.org>
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY UNIVERSITY OF SZEGED ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL UNIVERSITY OF SZEGED OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "LegacyRenderSVGRect.h"

#include "LegacyRenderSVGShapeInlines.h"
#include "SVGElementTypeHelpers.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(LegacyRenderSVGRect);

LegacyRenderSVGRect::LegacyRenderSVGRect(SVGRectElement& element, RenderStyle&& style)
    : LegacyRenderSVGShape(Type::LegacySVGRect, element, WTFMove(style))
{
}

LegacyRenderSVGRect::~LegacyRenderSVGRect() = default;

SVGRectElement& LegacyRenderSVGRect::rectElement() const
{
    return downcast<SVGRectElement>(LegacyRenderSVGShape::graphicsElement());
}

void LegacyRenderSVGRect::updateShapeFromElement()
{
    // Before creating a new object we need to clear the cached bounding box
    // to avoid using garbage.
    clearPath();
    m_shapeType = ShapeType::Empty;
    m_fillBoundingBox = FloatRect();
    m_strokeBoundingBox = std::nullopt;
    m_approximateStrokeBoundingBox = std::nullopt;

    SVGLengthContext lengthContext(&rectElement());
    FloatSize boundingBoxSize(lengthContext.valueForLength(style().width(), SVGLengthMode::Width), lengthContext.valueForLength(style().height(), SVGLengthMode::Height));

    // Spec: "A negative value is illegal. A value of zero disables rendering of the element."
    if (boundingBoxSize.isEmpty())
        return;

    auto& svgStyle = style().svgStyle();
    if (lengthContext.valueForLength(svgStyle.rx(), SVGLengthMode::Width) > 0
        || lengthContext.valueForLength(svgStyle.ry(), SVGLengthMode::Height) > 0)
        m_shapeType = ShapeType::RoundedRectangle;
    else
        m_shapeType = ShapeType::Rectangle;

    if (m_shapeType != ShapeType::Rectangle || hasNonScalingStroke()) {
        // Fallback to path-based approach.
        m_fillBoundingBox = ensurePath().boundingRect();
        return;
    }

    m_fillBoundingBox = FloatRect(FloatPoint(lengthContext.valueForLength(svgStyle.x(), SVGLengthMode::Width),
        lengthContext.valueForLength(svgStyle.y(), SVGLengthMode::Height)),
        boundingBoxSize);

    auto strokeBoundingBox = m_fillBoundingBox;
    if (svgStyle.hasStroke())
        strokeBoundingBox.inflate(this->strokeWidth() / 2);

#if USE(CG)
    // CoreGraphics can inflate the stroke by 1px when drawing a rectangle with antialiasing disabled at non-integer coordinates, we need to compensate.
    if (svgStyle.shapeRendering() == ShapeRendering::CrispEdges)
        strokeBoundingBox.inflate(1);
#endif

    m_strokeBoundingBox = strokeBoundingBox;
}

void LegacyRenderSVGRect::fillShape(GraphicsContext& context) const
{
    if (hasPath()) {
        LegacyRenderSVGShape::fillShape(context);
        return;
    }

#if USE(CG)
    // FIXME: CG implementation of GraphicsContextCG::fillRect has an own
    // shadow drawing method, which draws an extra shadow.
    // This is a workaround for switching off the extra shadow.
    // https://bugs.webkit.org/show_bug.cgi?id=68899
    if (context.hasDropShadow()) {
        GraphicsContextStateSaver stateSaver(context);
        context.clearDropShadow();
        context.fillRect(m_fillBoundingBox);
        return;
    }
#endif

    context.fillRect(m_fillBoundingBox);
}

void LegacyRenderSVGRect::strokeShape(GraphicsContext& context) const
{
    if (!style().hasVisibleStroke())
        return;

    if (hasPath()) {
        LegacyRenderSVGShape::strokeShape(context);
        return;
    }

    context.strokeRect(m_fillBoundingBox, strokeWidth());
}

bool LegacyRenderSVGRect::canUseStrokeHitTestFastPath() const
{
    // Non-scaling-stroke needs special handling.
    if (hasNonScalingStroke())
        return false;

    // We can compute intersections with simple, continuous strokes on
    // regular rectangles without using a Path.
    return m_shapeType == ShapeType::Rectangle && definitelyHasSimpleStroke();
}

// Returns true if the stroke is continuous and definitely uses miter joins.
bool LegacyRenderSVGRect::definitelyHasSimpleStroke() const
{
    // The four angles of a rect are 90 degrees. Using the formula at:
    // http://www.w3.org/TR/SVG/painting.html#StrokeMiterlimitProperty
    // when the join style of the rect is "miter", the ratio of the miterLength
    // to the stroke-width is found to be
    // miterLength / stroke-width = 1 / sin(45 degrees)
    //                            = 1 / (1 / sqrt(2))
    //                            = sqrt(2)
    //                            = 1.414213562373095...
    // When sqrt(2) exceeds the miterlimit, then the join style switches to
    // "bevel". When the miterlimit is greater than or equal to sqrt(2) then
    // the join style remains "miter".
    //
    // An approximation of sqrt(2) is used here because at certain precise
    // miterlimits, the join style used might not be correct (e.g. a miterlimit
    // of 1.4142135 should result in bevel joins, but may be drawn using miter
    // joins).
    return style().svgStyle().strokeDashArray().isEmpty() && style().joinStyle() == LineJoin::Miter && style().strokeMiterLimit() >= 1.5;
}

bool LegacyRenderSVGRect::shapeDependentStrokeContains(const FloatPoint& point, PointCoordinateSpace pointCoordinateSpace)
{
    if (!canUseStrokeHitTestFastPath()) {
        ensurePath();
        return LegacyRenderSVGShape::shapeDependentStrokeContains(point, pointCoordinateSpace);
    }

    auto halfStrokeWidth = strokeWidth() / 2;
    auto halfWidth = m_fillBoundingBox.width() / 2;
    auto halfHeight = m_fillBoundingBox.height() / 2;

    auto fillBoundingBoxCenter = FloatPoint(m_fillBoundingBox.x() + halfWidth, m_fillBoundingBox.y() + halfHeight);
    auto absDeltaX = std::abs(point.x() - fillBoundingBoxCenter.x());
    auto absDeltaY = std::abs(point.y() - fillBoundingBoxCenter.y());

    if (!(absDeltaX <= halfWidth + halfStrokeWidth && absDeltaY <= halfHeight + halfStrokeWidth))
        return false;

    return (halfWidth - halfStrokeWidth <= absDeltaX) || (halfHeight - halfStrokeWidth <= absDeltaY);
}

bool LegacyRenderSVGRect::shapeDependentFillContains(const FloatPoint& point, const WindRule fillRule) const
{
    if (m_shapeType == ShapeType::Empty)
        return false;
    if (m_shapeType != ShapeType::Rectangle)
        return LegacyRenderSVGShape::shapeDependentFillContains(point, fillRule);
    return m_fillBoundingBox.contains(point.x(), point.y());
}

bool LegacyRenderSVGRect::isRenderingDisabled() const
{
    // A width or height of zero disables rendering for the element, and results in an empty bounding box.
    return m_fillBoundingBox.isEmpty();
}

}
