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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER “AS IS” AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"
#include "ShapeInsideInfo.h"

#if ENABLE(CSS_SHAPES) && ENABLE(CSS_SHAPE_INSIDE)

#include "InlineIterator.h"
#include "RenderBlock.h"

namespace WebCore {

LineSegmentRange::LineSegmentRange(const InlineIterator& start, const InlineIterator& end)
    : start(start.root(), start.renderer(), start.offset())
    , end(end.root(), end.renderer(), end.offset())
    {
    }

bool ShapeInsideInfo::isEnabledFor(const RenderBlock& renderer)
{
    ShapeValue* shapeValue = renderer.style().resolvedShapeInside();
    if (!shapeValue)
        return false;

    switch (shapeValue->type()) {
    case ShapeValue::Shape:
        return shapeValue->shape() && shapeValue->shape()->type() != BasicShape::BasicShapeInsetRectangleType && shapeValue->shape()->type() != BasicShape::BasicShapeInsetType;
    case ShapeValue::Image:
        return shapeValue->isImageValid() && checkShapeImageOrigin(renderer.document(), *(shapeValue->image()->cachedImage()));
    case ShapeValue::Box:
        return true;
    case ShapeValue::Outside:
        // Outside value must already be resolved
        break;
    }
    ASSERT_NOT_REACHED();
    return false;
}

bool ShapeInsideInfo::updateSegmentsForLine(LayoutSize lineOffset, LayoutUnit lineHeight)
{
    m_segmentRanges.clear();
    bool result = updateSegmentsForLine(lineOffset.height(), lineHeight);
    for (size_t i = 0; i < m_segments.size(); i++) {
        m_segments[i].logicalLeft -= lineOffset.width();
        m_segments[i].logicalRight -= lineOffset.width();
    }
    return result;
}

bool ShapeInsideInfo::updateSegmentsForLine(LayoutUnit lineTop, LayoutUnit lineHeight)
{
    ASSERT(lineHeight >= 0);
    m_shapeLineTop = lineTop - logicalTopOffset();
    m_lineHeight = lineHeight;
    m_segments.clear();
    m_segmentRanges.clear();

    if (lineOverlapsShapeBounds())
        m_segments = computeSegmentsForLine(lineTop, lineHeight);

    return m_segments.size();
}

bool ShapeInsideInfo::adjustLogicalLineTop(float minSegmentWidth)
{
    const Shape& shape = computedShape();
    if (m_lineHeight <= 0 || logicalLineTop() > shapeLogicalBottom())
        return false;

    LayoutUnit newLineTop;
    if (shape.firstIncludedIntervalLogicalTop(m_shapeLineTop, FloatSize(minSegmentWidth, m_lineHeight), newLineTop)) {
        if (newLineTop > m_shapeLineTop) {
            m_shapeLineTop = newLineTop;
            return true;
        }
    }

    return false;
}

ShapeValue* ShapeInsideInfo::shapeValue() const
{
    return m_renderer.style().resolvedShapeInside();
}

LayoutUnit ShapeInsideInfo::computeFirstFitPositionForFloat(const FloatSize floatSize) const
{
    if (!floatSize.width() || shapeLogicalBottom() < logicalLineTop())
        return 0;

    LayoutUnit firstFitPosition = 0;
    if (computedShape().firstIncludedIntervalLogicalTop(m_shapeLineTop, floatSize, firstFitPosition) && (m_shapeLineTop <= firstFitPosition))
        return firstFitPosition;

    return 0;
}

}
#endif
