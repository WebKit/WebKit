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
#include "ExclusionShapeInsideInfo.h"

#if ENABLE(CSS_EXCLUSIONS)

#include "InlineIterator.h"
#include "RenderBlock.h"

namespace WebCore {

LineSegmentRange::LineSegmentRange(const InlineIterator& start, const InlineIterator& end)
    : start(start.root(), start.object(), start.offset())
    , end(end.root(), end.object(), end.offset())
    {
    }

bool ExclusionShapeInsideInfo::isEnabledFor(const RenderBlock* renderer)
{
    ExclusionShapeValue* shapeValue = renderer->style()->resolvedShapeInside();
    return (shapeValue && shapeValue->type() == ExclusionShapeValue::SHAPE) ? shapeValue->shape() : 0;
}

bool ExclusionShapeInsideInfo::computeSegmentsForLine(LayoutUnit lineTop, LayoutUnit lineHeight)
{
    ASSERT(lineHeight >= 0);
    m_shapeLineTop = lineTop - logicalTopOffset();
    m_lineHeight = lineHeight;
    m_segments.clear();
    m_segmentRanges.clear();

    if (lineOverlapsShapeBounds())
        computedShape()->getIncludedIntervals(m_shapeLineTop, std::min(m_lineHeight, shapeLogicalBottom() - lineTop), m_segments);

    LayoutUnit logicalLeftOffset = this->logicalLeftOffset();
    for (size_t i = 0; i < m_segments.size(); i++) {
        m_segments[i].logicalLeft += logicalLeftOffset;
        m_segments[i].logicalRight += logicalLeftOffset;
    }
    return m_segments.size();
}

bool ExclusionShapeInsideInfo::adjustLogicalLineTop(float minSegmentWidth)
{
    const ExclusionShape* shape = computedShape();
    if (!shape || m_lineHeight <= 0 || logicalLineTop() > shapeLogicalBottom())
        return false;

    float floatNewLineTop;
    if (shape->firstIncludedIntervalLogicalTop(m_shapeLineTop, FloatSize(minSegmentWidth, m_lineHeight), floatNewLineTop)) {
        LayoutUnit newLineTop = floatLogicalTopToLayoutUnit(floatNewLineTop);
        if (newLineTop > m_shapeLineTop) {
            m_shapeLineTop = newLineTop;
            return true;
        }
    }

    return false;
}

}
#endif
