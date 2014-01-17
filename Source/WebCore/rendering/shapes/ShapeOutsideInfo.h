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

#ifndef ShapeOutsideInfo_h
#define ShapeOutsideInfo_h

#if ENABLE(CSS_SHAPES)

#include "LayoutSize.h"
#include "ShapeInfo.h"

namespace WebCore {

class RenderBlockFlow;
class RenderBox;
class FloatingObject;

class ShapeOutsideInfo final : public ShapeInfo<RenderBox>, public MappedInfo<RenderBox, ShapeOutsideInfo> {
public:
    ShapeOutsideInfo(const RenderBox& renderer)
        : ShapeInfo<RenderBox>(renderer)
        , m_lineOverlapsShape(false)
    {
    }

    static bool isEnabledFor(const RenderBox&);

    LayoutUnit leftMarginBoxDelta() const { return m_leftMarginBoxDelta; }
    LayoutUnit rightMarginBoxDelta() const { return m_rightMarginBoxDelta; }
    bool lineOverlapsShape() const { return m_lineOverlapsShape; }

    void updateDeltasForContainingBlockLine(const RenderBlockFlow&, const FloatingObject&, LayoutUnit lineTop, LayoutUnit lineHeight);

    virtual bool lineOverlapsShapeBounds() const override
    {
        return computedShape().lineOverlapsShapeMarginBounds(m_shapeLineTop, m_lineHeight);
    }

protected:
    virtual LayoutBox resolvedLayoutBox() const override
    {
        if (shapeValue()->layoutBox() == BoxMissing) {
            if (shapeValue()->type() == ShapeValue::Image)
                return ContentBox;
            return MarginBox;
        }
        return shapeValue()->layoutBox();
    }

private:
    virtual LayoutRect computedShapeLogicalBoundingBox() const override { return computedShape().shapeMarginLogicalBoundingBox(); }
    virtual ShapeValue* shapeValue() const override;
    virtual void getIntervals(LayoutUnit lineTop, LayoutUnit lineHeight, SegmentList& segments) const override
    {
        return computedShape().getExcludedIntervals(lineTop, lineHeight, segments);
    }

    virtual WritingMode writingMode() const;

    LayoutUnit m_leftMarginBoxDelta;
    LayoutUnit m_rightMarginBoxDelta;
    LayoutUnit m_lineTop;
    bool m_lineOverlapsShape;
};

}
#endif
#endif
