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

#include "RenderMathMLFraction.h"

#include "GraphicsContext.h"
#include "MathMLNames.h"
#include "PaintInfo.h"
#include "RenderText.h"

namespace WebCore {
    
using namespace MathMLNames;

static const float gHorizontalPad = 0.2f;
static const float gLineThin = 0.33f;
static const float gLineMedium = 1.f;
static const float gLineThick = 3.f;
static const float gFractionBarWidth = 0.05f;
static const float gDenominatorPad = 0.1f;

RenderMathMLFraction::RenderMathMLFraction(Element* element)
    : RenderMathMLBlock(element)
    , m_lineThickness(gLineMedium)
{
    setChildrenInline(false);
}

void RenderMathMLFraction::updateFromElement()
{
    // FIXME: mfrac where bevelled=true will need to reorganize the descendants
    if (isEmpty()) 
        return;
    
    Element* fraction = static_cast<Element*>(node());
    
    RenderObject* numeratorWrapper = firstChild();
    String nalign = fraction->getAttribute(MathMLNames::numalignAttr);
    if (equalIgnoringCase(nalign, "left"))
        numeratorWrapper->style()->setTextAlign(LEFT);
    else if (equalIgnoringCase(nalign, "right"))
        numeratorWrapper->style()->setTextAlign(RIGHT);
    else
        numeratorWrapper->style()->setTextAlign(CENTER);
    
    RenderObject* denominatorWrapper = numeratorWrapper->nextSibling();
    if (!denominatorWrapper)
        return;
    
    String dalign = fraction->getAttribute(MathMLNames::denomalignAttr);
    if (equalIgnoringCase(dalign, "left"))
        denominatorWrapper->style()->setTextAlign(LEFT);
    else if (equalIgnoringCase(dalign, "right"))
        denominatorWrapper->style()->setTextAlign(RIGHT);
    else
        denominatorWrapper->style()->setTextAlign(CENTER);
    
    // FIXME: parse units
    String thickness = fraction->getAttribute(MathMLNames::linethicknessAttr);
    m_lineThickness = gLineMedium;
    if (equalIgnoringCase(thickness, "thin"))
        m_lineThickness = gLineThin;
    else if (equalIgnoringCase(thickness, "medium"))
        m_lineThickness = gLineMedium;
    else if (equalIgnoringCase(thickness, "thick"))
        m_lineThickness = gLineThick;
    else if (equalIgnoringCase(thickness, "0"))
        m_lineThickness = 0;

    // Update the style for the padding of the denominator for the line thickness
    lastChild()->style()->setPaddingTop(Length(static_cast<int>(m_lineThickness + style()->fontSize() * gDenominatorPad), Fixed));
}

void RenderMathMLFraction::addChild(RenderObject* child, RenderObject* beforeChild)
{
    RenderBlock* row = createAlmostAnonymousBlock();
    
    row->style()->setTextAlign(CENTER);
    Length pad(static_cast<int>(style()->fontSize() * gHorizontalPad), Fixed);
    row->style()->setPaddingLeft(pad);
    row->style()->setPaddingRight(pad);
    
    // Only add padding for rows as denominators
    bool isNumerator = isEmpty();
    if (!isNumerator) 
        row->style()->setPaddingTop(Length(2, Fixed));
    
    RenderBlock::addChild(row, beforeChild);
    row->addChild(child);
    updateFromElement();
}

RenderMathMLOperator* RenderMathMLFraction::unembellishedOperator()
{
    RenderObject* numeratorWrapper = firstChild();
    if (!numeratorWrapper)
        return 0;
    RenderObject* numerator = numeratorWrapper->firstChild();
    if (!numerator || !numerator->isRenderMathMLBlock())
        return 0;
    return toRenderMathMLBlock(numerator)->unembellishedOperator();
}

void RenderMathMLFraction::layout()
{
    updateFromElement();

    // Adjust the fraction line thickness for the zoom
    if (lastChild() && lastChild()->isRenderBlock())
        m_lineThickness *= ceilf(gFractionBarWidth * style()->fontSize());

    RenderBlock::layout();
}

void RenderMathMLFraction::paint(PaintInfo& info, const LayoutPoint& paintOffset)
{
    RenderMathMLBlock::paint(info, paintOffset);
    if (info.context->paintingDisabled() || info.phase != PaintPhaseForeground)
        return;
    
    if (!firstChild() ||!m_lineThickness)
        return;

    int verticalOffset = 0;
    // The children are always RenderMathMLBlock instances
    if (firstChild()->isRenderMathMLBlock()) {
        int adjustForThickness = m_lineThickness > 1 ? int(m_lineThickness / 2) : 1;
        if (int(m_lineThickness) % 2 == 1)
            adjustForThickness++;
        // FIXME: This is numeratorWrapper, not numerator.
        RenderMathMLBlock* numerator = toRenderMathMLBlock(firstChild());
        if (numerator->isRenderMathMLRow())
            verticalOffset = numerator->pixelSnappedOffsetHeight() + adjustForThickness;
        else 
            verticalOffset = numerator->pixelSnappedOffsetHeight();        
    }
    
    IntPoint adjustedPaintOffset = roundedIntPoint(paintOffset + location());
    adjustedPaintOffset.setY(adjustedPaintOffset.y() + verticalOffset);
    
    GraphicsContextStateSaver stateSaver(*info.context);
    
    info.context->setStrokeThickness(m_lineThickness);
    info.context->setStrokeStyle(SolidStroke);
    info.context->setStrokeColor(style()->visitedDependentColor(CSSPropertyColor), ColorSpaceSRGB);
    
    info.context->drawLine(adjustedPaintOffset, IntPoint(adjustedPaintOffset.x() + pixelSnappedOffsetWidth(), adjustedPaintOffset.y()));
}

LayoutUnit RenderMathMLFraction::baselinePosition(FontBaseline, bool firstLine, LineDirectionMode lineDirection, LinePositionMode linePositionMode) const
{
    if (firstChild() && firstChild()->isRenderMathMLBlock()) {
        RenderMathMLBlock* numeratorWrapper = toRenderMathMLBlock(firstChild());
        RenderStyle* refStyle = style();
        if (previousSibling())
            refStyle = previousSibling()->style();
        else if (nextSibling())
            refStyle = nextSibling()->style();
        int shift = int(ceil((refStyle->fontMetrics().xHeight() + 1) / 2));
        return numeratorWrapper->pixelSnappedOffsetHeight() + shift;
    }
    return RenderBlock::baselinePosition(AlphabeticBaseline, firstLine, lineDirection, linePositionMode);
}

}

#endif // ENABLE(MATHML)
