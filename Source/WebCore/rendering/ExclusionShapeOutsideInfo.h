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

#ifndef ExclusionShapeOutsideInfo_h
#define ExclusionShapeOutsideInfo_h

#if ENABLE(CSS_EXCLUSIONS)

#include "ExclusionShapeInfo.h"
#include "LayoutSize.h"

namespace WebCore {

class RenderBox;

class ExclusionShapeOutsideInfo : public ExclusionShapeInfo<RenderBox, &RenderStyle::shapeOutside, &ExclusionShape::getExcludedIntervals>, public MappedInfo<RenderBox, ExclusionShapeOutsideInfo> {
public:
    LayoutSize shapeLogicalOffset() const { return LayoutSize(shapeLogicalLeft(), shapeLogicalTop()); }

    LayoutUnit leftSegmentShapeBoundingBoxDelta() const { return m_leftSegmentShapeBoundingBoxDelta; }
    LayoutUnit rightSegmentShapeBoundingBoxDelta() const { return m_rightSegmentShapeBoundingBoxDelta; }

    virtual bool computeSegmentsForLine(LayoutUnit lineTop, LayoutUnit lineHeight) OVERRIDE;

    static PassOwnPtr<ExclusionShapeOutsideInfo> createInfo(const RenderBox* renderer) { return adoptPtr(new ExclusionShapeOutsideInfo(renderer)); }
    static bool isEnabledFor(const RenderBox*);
private:
    ExclusionShapeOutsideInfo(const RenderBox* renderer) : ExclusionShapeInfo<RenderBox, &RenderStyle::shapeOutside, &ExclusionShape::getExcludedIntervals>(renderer) { }

    LayoutUnit m_leftSegmentShapeBoundingBoxDelta;
    LayoutUnit m_rightSegmentShapeBoundingBoxDelta;
    LayoutUnit m_lineTop;
};

}
#endif
#endif
