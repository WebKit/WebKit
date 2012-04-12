/*
 * Copyright (C) 2009 Alex Milowski (alex@milowski.com). All rights reserved.
 * Copyright (C) 2010 François Sausset (sausset@gmail.com). All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(MATHML)

#include "RenderMathMLRoot.h"

#include "GraphicsContext.h"
#include "MathMLNames.h"
#include "PaintInfo.h"

using namespace std;

namespace WebCore {
    
using namespace MathMLNames;

// FIXME: This whole file should be changed to work with various writing modes. See https://bugs.webkit.org/show_bug.cgi?id=48951.

// Threshold above which the radical shape is modified to look nice with big bases (em)
const float gThresholdBaseHeightEms = 1.5f;
// Normal width of the front of the radical sign, before the base & overbar (em)
const float gFrontWidthEms = 0.75f;
// Gap between the base and overbar (em)
const float gSpaceAboveEms = 0.2f;
// Horizontal position of the bottom point of the radical (* frontWidth)
const float gRadicalBottomPointXFront = 0.5f;
// Lower the radical sign's bottom point (px)
const int gRadicalBottomPointLower = 3;
// Horizontal position of the top left point of the radical "dip" (* frontWidth)
const float gRadicalDipLeftPointXFront = 0.8f;
// Vertical position of the top left point of the radical "dip" (* baseHeight)
const float gRadicalDipLeftPointYPos = 0.625f; 
// Vertical shift of the left end point of the radical (em)
const float gRadicalLeftEndYShiftEms = 0.05f;
// Additional bottom root padding if baseHeight > threshold (em)
const float gBigRootBottomPaddingEms = 0.2f;

// Radical line thickness (em)
const float gRadicalLineThicknessEms = 0.02f;
// Radical thick line thickness (em)
const float gRadicalThickLineThicknessEms = 0.1f;
    
RenderMathMLRoot::RenderMathMLRoot(Element* element)
    : RenderMathMLBlock(element)
{
}

RenderBoxModelObject* RenderMathMLRoot::index() const
{
    if (!firstChild())
        return 0;
    RenderObject* index = firstChild()->nextSibling();
    if (!index || !index->isBoxModelObject())
        return 0;
    return toRenderBoxModelObject(index);
}

void RenderMathMLRoot::paint(PaintInfo& info, const LayoutPoint& paintOffset)
{
    RenderMathMLBlock::paint(info, paintOffset);
    
    if (info.context->paintingDisabled())
        return;
    
    if (!index())
        return;
    
    IntPoint adjustedPaintOffset = roundedIntPoint(paintOffset + location() + computedCSSContentBoxRect().location());
    
    int baseHeight = roundToInt(getBoxModelObjectHeight(firstChild()));
    
    int overbarWidth = roundToInt(getBoxModelObjectWidth(firstChild())) + m_overbarLeftPointShift;
    int indexWidth = index()->pixelSnappedOffsetWidth();
    int frontWidth = static_cast<int>(roundf(gFrontWidthEms * style()->fontSize()));
    int startX = adjustedPaintOffset.x() + indexWidth + m_overbarLeftPointShift;
    
    int rootPad = static_cast<int>(roundf(gSpaceAboveEms * style()->fontSize()));
    adjustedPaintOffset.setY(adjustedPaintOffset.y() + m_intrinsicPaddingBefore - rootPad);
    
    FloatPoint overbarLeftPoint(startX - m_overbarLeftPointShift, adjustedPaintOffset.y());
    FloatPoint bottomPoint(startX - gRadicalBottomPointXFront * frontWidth, adjustedPaintOffset.y() + baseHeight + gRadicalBottomPointLower);
    FloatPoint dipLeftPoint(startX - gRadicalDipLeftPointXFront * frontWidth, adjustedPaintOffset.y() + gRadicalDipLeftPointYPos * baseHeight);
    FloatPoint leftEnd(startX - frontWidth, dipLeftPoint.y() + gRadicalLeftEndYShiftEms * style()->fontSize());
    
    GraphicsContextStateSaver stateSaver(*info.context);
    
    info.context->setStrokeThickness(gRadicalLineThicknessEms * style()->fontSize());
    info.context->setStrokeStyle(SolidStroke);
    info.context->setStrokeColor(style()->visitedDependentColor(CSSPropertyColor), ColorSpaceDeviceRGB);
    info.context->setLineJoin(MiterJoin);
    info.context->setMiterLimit(style()->fontSize());
    
    Path root;
    
    root.moveTo(FloatPoint(overbarLeftPoint.x() + overbarWidth, adjustedPaintOffset.y()));
    // draw top
    root.addLineTo(overbarLeftPoint);
    // draw from top left corner to bottom point of radical
    root.addLineTo(bottomPoint);
    // draw from bottom point to top of left part of radical base "dip"
    root.addLineTo(dipLeftPoint);
    // draw to end
    root.addLineTo(leftEnd);
    
    info.context->strokePath(root);
    
    GraphicsContextStateSaver maskStateSaver(*info.context);
    
    // Build a mask to draw the thick part of the root.
    Path mask;
    
    mask.moveTo(overbarLeftPoint);
    mask.addLineTo(bottomPoint);
    mask.addLineTo(dipLeftPoint);
    mask.addLineTo(FloatPoint(2 * dipLeftPoint.x() - leftEnd.x(), 2 * dipLeftPoint.y() - leftEnd.y()));
    
    info.context->clip(mask);
    
    // Draw the thick part of the root.
    info.context->setStrokeThickness(gRadicalThickLineThicknessEms * style()->fontSize());
    info.context->setLineCap(SquareCap);
    
    Path line;
    line.moveTo(bottomPoint);
    line.addLineTo(dipLeftPoint);
    
    info.context->strokePath(line);
}

void RenderMathMLRoot::layout()
{
    RenderBlock::layout();

    if (!index())
        return;

    int baseHeight = roundToInt(getBoxModelObjectHeight(firstChild()));
    
    // Base height above which the shape of the root changes
    float thresholdHeight = gThresholdBaseHeightEms * style()->fontSize();
    if (baseHeight > thresholdHeight && thresholdHeight) {
        float shift = min<float>((baseHeight - thresholdHeight) / thresholdHeight, 1.0f);
        int frontWidth = static_cast<int>(roundf(gFrontWidthEms * style()->fontSize()));
        m_overbarLeftPointShift = static_cast<int>(shift * gRadicalBottomPointXFront * frontWidth);
        m_intrinsicPaddingAfter = static_cast<int>(roundf(gBigRootBottomPaddingEms * style()->fontSize()));
    } else {
        m_overbarLeftPointShift = 0;
        m_intrinsicPaddingAfter = 0;
    }
    
    RenderBoxModelObject* index = this->index();
    
    m_intrinsicPaddingStart = index->pixelSnappedOffsetWidth() + m_overbarLeftPointShift;
    
    int rootPad = static_cast<int>(roundf(gSpaceAboveEms * style()->fontSize()));
    int partDipHeight = static_cast<int>(roundf((1 - gRadicalDipLeftPointYPos) * baseHeight));
    int rootExtraTop = partDipHeight + index->pixelSnappedOffsetHeight() - (baseHeight + rootPad);
    m_intrinsicPaddingBefore = rootPad + max(rootExtraTop, 0);
    
    setNeedsLayout(true, MarkOnlyThis);
    setPreferredLogicalWidthsDirty(true, MarkOnlyThis); // FIXME: Can this really be right?
    // FIXME: Preferred logical widths are currently wrong the first time through, relying on layout() to set m_intrinsicPaddingStart.
    RenderBlock::layout();
    
    // |index| should be a RenderBlock here, unless the user has overriden its { position: absolute }.
    if (rootExtraTop < 0 && index->isBox())
        toRenderBox(index)->setLogicalTop(-rootExtraTop);
}

}

#endif // ENABLE(MATHML)
