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

#if ENABLE(CSS_SHAPES)

#include "ShapeOutsideInfo.h"

#include "FloatingObjects.h"
#include "RenderBlockFlow.h"
#include "RenderBox.h"

namespace WebCore {

bool ShapeOutsideInfo::isEnabledFor(const RenderBox& box)
{
    ShapeValue* shapeValue = box.style().shapeOutside();
    if (!box.isFloating() || !shapeValue)
        return false;

    switch (shapeValue->type()) {
    case ShapeValue::Shape:
        return shapeValue->shape();
    case ShapeValue::Image:
        return shapeValue->isImageValid() && checkShapeImageOrigin(box.document(), *(shapeValue->image()->cachedImage()));
    case ShapeValue::Box:
        return true;
    case ShapeValue::Outside:
        break;
    }
    ASSERT_NOT_REACHED();
    return false;
}

void ShapeOutsideInfo::updateDeltasForContainingBlockLine(const RenderBlockFlow& containingBlock, const FloatingObject& floatingObject, LayoutUnit lineTop, LayoutUnit lineHeight)
{
    LayoutUnit borderBoxTop = containingBlock.logicalTopForFloat(&floatingObject) + std::max(LayoutUnit(), containingBlock.marginBeforeForChild(m_renderer));
    LayoutUnit borderBoxLineTop = lineTop - borderBoxTop;

    if (isShapeDirty() || m_borderBoxLineTop != borderBoxLineTop || m_lineHeight != lineHeight) {
        m_borderBoxLineTop = borderBoxLineTop;
        m_referenceBoxLineTop = borderBoxLineTop - logicalTopOffset();
        m_lineHeight = lineHeight;

        LayoutUnit floatMarginBoxWidth = containingBlock.logicalWidthForFloat(&floatingObject);

        if (lineOverlapsShapeBounds()) {
            SegmentList segments = computeSegmentsForLine(borderBoxLineTop, lineHeight);
            if (segments.size()) {
                LayoutUnit rawLeftMarginBoxDelta = segments.first().logicalLeft + containingBlock.marginStartForChild(m_renderer);
                m_leftMarginBoxDelta = clampTo<LayoutUnit>(rawLeftMarginBoxDelta, LayoutUnit(), floatMarginBoxWidth);

                LayoutUnit rawRightMarginBoxDelta = segments.last().logicalRight - containingBlock.logicalWidthForChild(m_renderer) - containingBlock.marginEndForChild(m_renderer);
                m_rightMarginBoxDelta = clampTo<LayoutUnit>(rawRightMarginBoxDelta, -floatMarginBoxWidth, LayoutUnit());
                m_lineOverlapsShape = true;
                return;
            }
        }

        // Lines that do not overlap the shape should act as if the float
        // wasn't there for layout purposes. So we set the deltas to remove the
        // entire width of the float
        m_leftMarginBoxDelta = floatMarginBoxWidth;
        m_rightMarginBoxDelta = -floatMarginBoxWidth;
        m_lineOverlapsShape = false;
    }
}

ShapeValue* ShapeOutsideInfo::shapeValue() const
{
    return m_renderer.style().shapeOutside();
}

WritingMode ShapeOutsideInfo::writingMode() const
{
    ASSERT(m_renderer.containingBlock());
    return m_renderer.containingBlock()->style().writingMode();
}

}

#endif
