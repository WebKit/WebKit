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

namespace WebCore {
    
using namespace MathMLNames;

// Extra space on the left for the radical sign (px)
const int gRadicalLeftExtra = 3;
// Lower the radical sign's bottom point (px)
const int gRadicalBottomPointLower = 3;
// Threshold above which the radical shape is modified to look nice with big bases (em)
const float gThresholdBaseHeightEms = 1.5f;
// Front width (em)
const float gFrontWidthEms = 0.75f;
// Horizontal position of the bottom point of the radical (* frontWidth)
const float gRadicalBottomPointXFront = 0.5f;
// Horizontal position of the top left point of the radical "dip" (* frontWidth)
const float gRadicalDipLeftPointXFront = 0.8f;
// Vertical position of the top left point of the radical "dip" (* baseHeight)
const float gRadicalDipLeftPointYPos = 0.625f; 
// Vertical shift of the left end point of the radical (em)
const float gRadicalLeftEndYShiftEms = 0.05f;
// Root padding around the base (em) (mroot padding-top/left from mathml.css)
const float gRootPaddingEms = 0.2f;
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

void RenderMathMLRoot::addChild(RenderObject* child, RenderObject* )
{
    if (isEmpty()) {
        // Add a block for the index
        RenderBlock* indexWrapper = createAlmostAnonymousBlock(INLINE_BLOCK);
        RenderBlock::addChild(indexWrapper);
        
        // FIXME: the wrapping does not seem to be needed anymore.
        // this is the base, so wrap it so we can pad it
        RenderBlock* baseWrapper = createAlmostAnonymousBlock(INLINE_BLOCK);
        baseWrapper->style()->setPaddingLeft(Length(5 * gFrontWidthEms, Percent));
        RenderBlock::addChild(baseWrapper);
        baseWrapper->addChild(child);
    } else {
        // always add to the index
        firstChild()->addChild(child);
    }
}
    
void RenderMathMLRoot::paint(PaintInfo& info, const LayoutPoint& paintOffset)
{
    RenderMathMLBlock::paint(info, paintOffset);
    
    if (info.context->paintingDisabled())
        return;
    
    if (!firstChild() || !lastChild())
        return;
    
    IntPoint adjustedPaintOffset = roundedIntPoint(paintOffset + location());
    
    RenderBoxModelObject* baseWrapper = toRenderBoxModelObject(lastChild());
    
    int baseHeight = baseWrapper->pixelSnappedOffsetHeight();
    // default to the font size in pixels if we're empty
    if (!baseHeight)
        baseHeight = style()->fontSize();
    int overbarWidth = baseWrapper->pixelSnappedOffsetWidth();
    
    int indexWidth = 0;
    RenderObject* current = firstChild();
    while (current != lastChild()) {
        if (current->isBoxModelObject()) {
            RenderBoxModelObject* box = toRenderBoxModelObject(current);
            indexWidth += box->pixelSnappedOffsetWidth();
        }
        current = current->nextSibling();
    }
    
    int frontWidth = static_cast<int>(style()->fontSize() * gFrontWidthEms);
    int overbarLeftPointShift = 0;
    // Base height above which the shape of the root changes
    int thresholdHeight = static_cast<int>(gThresholdBaseHeightEms * style()->fontSize());
    
    if (baseHeight > thresholdHeight && thresholdHeight) {
        float shift = (baseHeight - thresholdHeight) / static_cast<float>(thresholdHeight);
        if (shift > 1.)
            shift = 1.0f;
        overbarLeftPointShift = static_cast<int>(gRadicalBottomPointXFront * frontWidth * shift);
    }
    
    overbarWidth += overbarLeftPointShift;
    
    int rootPad = static_cast<int>(gRootPaddingEms * style()->fontSize());
    int startX = adjustedPaintOffset.x() + indexWidth + gRadicalLeftExtra + style()->paddingLeft().value() - rootPad;
    adjustedPaintOffset.setY(adjustedPaintOffset.y() + style()->paddingTop().value() - rootPad);
    
    FloatPoint overbarLeftPoint(startX - overbarLeftPointShift, adjustedPaintOffset.y());
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

    if (!firstChild() || !lastChild())
        return;

    int baseHeight = toRenderBoxModelObject(lastChild())->pixelSnappedOffsetHeight();
    if (!baseHeight)
        baseHeight = style()->fontSize();
    
    RenderObject* base = lastChild()->firstChild();
    if (base)
        base->style()->setVerticalAlign(BASELINE); // FIXME: Can this style be modified?
    
    // Base height above which the shape of the root changes
    int thresholdHeight = static_cast<int>(gThresholdBaseHeightEms * style()->fontSize());
    int overbarLeftPointShift = 0;
    
    // FIXME: Can style() and indexBox->style() be modified (4 times below)?
    if (baseHeight > thresholdHeight && thresholdHeight) {
        float shift = (baseHeight - thresholdHeight) / static_cast<float>(thresholdHeight);
        if (shift > 1.)
            shift = 1.0f;
        int frontWidth = static_cast<int>(style()->fontSize() * gFrontWidthEms);
        overbarLeftPointShift = static_cast<int>(gRadicalBottomPointXFront * frontWidth * shift);
        
        style()->setPaddingBottom(Length(static_cast<int>(gBigRootBottomPaddingEms * style()->fontSize()), Fixed));
    }
    
    // Positioning of the index
    RenderObject* possibleIndex = firstChild()->firstChild();
    while (possibleIndex && !possibleIndex->isBoxModelObject())
        possibleIndex = possibleIndex->nextSibling();
    RenderBoxModelObject* indexBox = toRenderBoxModelObject(possibleIndex);
    if (!indexBox)
        return;
    
    int shiftForIndex = indexBox->pixelSnappedOffsetWidth() + overbarLeftPointShift;
    int partDipHeight = static_cast<int>((1 - gRadicalDipLeftPointYPos) * baseHeight);
    int rootExtraTop = partDipHeight + style()->paddingBottom().value() + indexBox->pixelSnappedOffsetHeight()
        - (baseHeight + static_cast<int>(gRootPaddingEms * style()->fontSize()));
    
    style()->setPaddingLeft(Length(shiftForIndex, Fixed));
    if (rootExtraTop > 0)
        style()->setPaddingTop(Length(rootExtraTop + static_cast<int>(gRootPaddingEms * style()->fontSize()), Fixed));
    
    setNeedsLayout(true);
    setPreferredLogicalWidthsDirty(true, false); // FIXME: Can this really be right?
    RenderBlock::layout();

    indexBox->style()->setBottom(Length(partDipHeight + style()->paddingBottom().value(), Fixed));

    // Now that we've potentially changed its position, we need layout the index again.
    indexBox->setNeedsLayout(true);
    indexBox->layout();
}
    
}

#endif // ENABLE(MATHML)
