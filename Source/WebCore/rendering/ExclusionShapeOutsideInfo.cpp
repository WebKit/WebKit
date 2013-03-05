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

#if ENABLE(CSS_EXCLUSIONS)

#include "ExclusionShapeOutsideInfo.h"

#include "RenderBox.h"

namespace WebCore {
bool ExclusionShapeOutsideInfo::isEnabledFor(const RenderBox* box)
{
    // FIXME: Enable shape outside for non-rectangular shapes! (bug 98664)
    ExclusionShapeValue* value = box->style()->shapeOutside();
    return value && (value->type() == ExclusionShapeValue::SHAPE)
        && (
            (value->shape()->type() == BasicShape::BASIC_SHAPE_RECTANGLE)
            || (value->shape()->type() == BasicShape::BASIC_SHAPE_POLYGON)
        );
}

bool ExclusionShapeOutsideInfo::computeSegmentsForLine(LayoutUnit lineTop, LayoutUnit lineHeight)
{
    if (shapeSizeDirty() || m_lineTop != lineTop || m_lineHeight != lineHeight) {
        if (ExclusionShapeInfo<RenderBox, &RenderStyle::shapeOutside, &ExclusionShape::getExcludedIntervals>::computeSegmentsForLine(lineTop, lineHeight)) {
            m_leftSegmentShapeBoundingBoxDelta = m_segments[0].logicalLeft - shapeLogicalLeft();
            m_rightSegmentShapeBoundingBoxDelta = m_segments[m_segments.size()-1].logicalRight - shapeLogicalRight();
        } else {
            m_leftSegmentShapeBoundingBoxDelta = 0;
            m_rightSegmentShapeBoundingBoxDelta = 0;
        }
        m_lineTop = lineTop;
    }

    return m_segments.size();
}

}
#endif
