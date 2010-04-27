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

namespace WebCore {
    
using namespace MathMLNames;

// Left margin of the radical (px)
const int gRadicalLeftMargin = 3;
// Bottom padding of the radical (px)
const int gRadicalBasePad = 3;
// Threshold above which the radical shape is modified to look nice with big bases (%)
const float gThresholdBaseHeight = 1.5;
// Radical width (%)
const float gRadicalWidth = 0.75;
// Horizontal position of the bottom point of the radical (%)
const float gRadicalBottomPointXPos= 0.5;
// Horizontal position of the top left point of the radical (%)
const float gRadicalTopLeftPointXPos = 0.8;
// Vertical position of the top left point of the radical (%)
const float gRadicalTopLeftPointYPos = 0.625; 
// Vertical shift of the left end point of the radical (%)
const float gRadicalLeftEndYShift = 0.05;
// Root padding around the base (%)
const float gRootPadding = 0.2;
// Additional bottom root padding (%)
const float gRootBottomPadding = 0.2;
    
// Radical line thickness (%)
const float gRadicalLineThickness = 0.02;
// Radical thick line thickness (%)
const float gRadicalThickLineThickness = 0.1;
    
RenderMathMLRoot::RenderMathMLRoot(Node *expression) 
: RenderMathMLBlock(expression) 
{
}

void RenderMathMLRoot::addChild(RenderObject* child, RenderObject* )
{
    if (isEmpty()) {
        // Add a block for the index
        RenderBlock* block = new (renderArena()) RenderBlock(node());
        RefPtr<RenderStyle> indexStyle = makeBlockStyle();
        indexStyle->setDisplay(INLINE_BLOCK);
        block->setStyle(indexStyle.release());
        RenderBlock::addChild(block);
        
        // FIXME: the wrapping does not seem to be needed anymore.
        // this is the base, so wrap it so we can pad it
        block = new (renderArena()) RenderBlock(node());
        RefPtr<RenderStyle> baseStyle = makeBlockStyle();
        baseStyle->setDisplay(INLINE_BLOCK);
        baseStyle->setPaddingLeft(Length(5 * gRadicalWidth , Percent));
        block->setStyle(baseStyle.release());
        RenderBlock::addChild(block);
        block->addChild(child);
    } else {
        // always add to the index
        firstChild()->addChild(child);
    }
}
    
void RenderMathMLRoot::paint(PaintInfo& info, int tx, int ty)
{
    RenderMathMLBlock::paint(info , tx , ty);
    
    tx += x();
    ty += y();
    
    RenderBoxModelObject* indexBox = toRenderBoxModelObject(lastChild());
    
    int maxHeight = indexBox->offsetHeight();
    // default to the font size in pixels if we're empty
    if (!maxHeight)
        maxHeight = style()->fontSize();
    int width = indexBox->offsetWidth();
    
    int indexWidth = 0;
    RenderObject* current = firstChild();
    while (current != lastChild()) {
        if (current->isBoxModelObject()) {
            RenderBoxModelObject* box = toRenderBoxModelObject(current);
            indexWidth += box->offsetWidth();
        }
        current = current->nextSibling();
    }
    
    int frontWidth = static_cast<int>(style()->fontSize() * gRadicalWidth);
    int topStartShift = 0;
    // Base height above which the shape of the root changes
    int thresholdHeight = static_cast<int>(gThresholdBaseHeight * style()->fontSize());
    
    if (maxHeight > thresholdHeight && thresholdHeight) {
        float shift = (maxHeight - thresholdHeight) / static_cast<float>(thresholdHeight);
        if (shift > 1.)
            shift = 1.;
        topStartShift = static_cast<int>(gRadicalBottomPointXPos * frontWidth * shift);
    }
    
    width += topStartShift;
    
    int rootPad = static_cast<int>(gRootPadding * style()->fontSize());
    int start = tx + indexWidth + gRadicalLeftMargin + style()->paddingLeft().value() - rootPad;
    ty += style()->paddingTop().value() - rootPad;
    
    FloatPoint topStart(start - topStartShift, ty);
    FloatPoint bottomLeft(start - gRadicalBottomPointXPos * frontWidth , ty + maxHeight + gRadicalBasePad);
    FloatPoint topLeft(start - gRadicalTopLeftPointXPos * frontWidth , ty + gRadicalTopLeftPointYPos * maxHeight);
    FloatPoint leftEnd(start - frontWidth , topLeft.y() + gRadicalLeftEndYShift * style()->fontSize());
    
    info.context->save();
    
    info.context->setStrokeThickness(gRadicalLineThickness * style()->fontSize());
    info.context->setStrokeStyle(SolidStroke);
    info.context->setStrokeColor(style()->color(), sRGBColorSpace);
    info.context->setLineJoin(MiterJoin);
    info.context->setMiterLimit(style()->fontSize());
    
    Path root;
    
    root.moveTo(FloatPoint(topStart.x() + width, ty));
    // draw top
    root.addLineTo(topStart);
    // draw from top left corner to bottom point of radical
    root.addLineTo(bottomLeft);
    // draw from bottom point to top of left part of radical base "pocket"
    root.addLineTo(topLeft);
    // draw to end
    root.addLineTo(leftEnd);
    
    info.context->beginPath();
    info.context->addPath(root);
    info.context->strokePath();
    
    info.context->save();
    
    // Build a mask to draw the thick part of the root.
    Path mask;
    
    mask.moveTo(topStart);
    mask.addLineTo(bottomLeft);
    mask.addLineTo(topLeft);
    mask.addLineTo(FloatPoint(2 * topLeft.x() - leftEnd.x(), 2 * topLeft.y() - leftEnd.y()));
    
    info.context->beginPath();
    info.context->addPath(mask);
    info.context->clip(mask);
    
    // Draw the thick part of the root.
    info.context->setStrokeThickness(gRadicalThickLineThickness * style()->fontSize());
    info.context->setLineCap(SquareCap);
    
    Path line;
    
    line = line.createLine(bottomLeft, topLeft);
    
    info.context->beginPath();
    info.context->addPath(line);
    info.context->strokePath();
    
    info.context->restore();
    
    info.context->restore();

}

void RenderMathMLRoot::layout()
{
    RenderBlock::layout();
    
    int maxHeight = toRenderBoxModelObject(lastChild())->offsetHeight();
    
    RenderObject* current = lastChild()->firstChild();
    
    toRenderMathMLBlock(current)->style()->setVerticalAlign(BASELINE);
    
    if (!maxHeight)
        maxHeight = style()->fontSize();
    
    // Base height above which the shape of the root changes
    int thresholdHeight = static_cast<int>(gThresholdBaseHeight * style()->fontSize());
    int topStartShift = 0;
    
    if (maxHeight > thresholdHeight && thresholdHeight) {
        float shift = (maxHeight - thresholdHeight) / static_cast<float>(thresholdHeight);
        if (shift > 1.)
            shift = 1.;
        int frontWidth = static_cast<int>(style()->fontSize() * gRadicalWidth);
        topStartShift = static_cast<int>(gRadicalBottomPointXPos * frontWidth * shift);
        
        style()->setPaddingBottom(Length(static_cast<int>(gRootBottomPadding * style()->fontSize()), Fixed));
    }
    
    // Positioning of the index
    RenderBoxModelObject* indexBox = toRenderBoxModelObject(firstChild()->firstChild());
    
    int indexShift = indexBox->offsetWidth() + topStartShift;
    int radicalHeight = static_cast<int>((1 - gRadicalTopLeftPointYPos) * maxHeight);
    int rootMarginTop = radicalHeight + style()->paddingBottom().value() + indexBox->offsetHeight() - (maxHeight + static_cast<int>(gRootPadding * style()->fontSize()));
    
    style()->setPaddingLeft(Length(indexShift, Fixed));
    if (rootMarginTop > 0)
        style()->setPaddingTop(Length(rootMarginTop + static_cast<int>(gRootPadding * style()->fontSize()), Fixed));
    
    setNeedsLayoutAndPrefWidthsRecalc();
    markContainingBlocksForLayout();
    RenderBlock::layout();
    
    indexBox->style()->setBottom(Length(radicalHeight + style()->paddingBottom().value(), Fixed));
    
    indexBox->layout();
}
    
}

#endif // ENABLE(MATHML)


