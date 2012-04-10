/*
 * Copyright (C) 2009 Alex Milowski (alex@milowski.com). All rights reserved.
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

#include "RenderMathMLBlock.h"

#include "GraphicsContext.h"
#include "MathMLNames.h"

#if ENABLE(DEBUG_MATH_LAYOUT)
#include "PaintInfo.h"
#endif

namespace WebCore {
    
using namespace MathMLNames;
    
RenderMathMLBlock::RenderMathMLBlock(Node* container) 
    : RenderBlock(container)
    , m_intrinsicPaddingBefore(0)
    , m_intrinsicPaddingAfter(0)
    , m_intrinsicPaddingStart(0)
    , m_intrinsicPaddingEnd(0)
{
}

bool RenderMathMLBlock::isChildAllowed(RenderObject* child, RenderStyle*) const
{
    return child->node() && child->node()->nodeType() == Node::ELEMENT_NODE;
}

LayoutUnit RenderMathMLBlock::paddingTop(PaddingOptions paddingOption) const
{
    LayoutUnit result = RenderBlock::paddingTop(ExcludeIntrinsicPadding);
    if (paddingOption == ExcludeIntrinsicPadding)
        return result;
    switch (style()->writingMode()) {
    case TopToBottomWritingMode:
        return result + m_intrinsicPaddingBefore;
    case BottomToTopWritingMode:
        return result + m_intrinsicPaddingAfter;
    case LeftToRightWritingMode:
    case RightToLeftWritingMode:
        return result + (style()->isLeftToRightDirection() ? m_intrinsicPaddingStart : m_intrinsicPaddingEnd);
    }
    ASSERT_NOT_REACHED();
    return result;
}

LayoutUnit RenderMathMLBlock::paddingBottom(PaddingOptions paddingOption) const
{
    LayoutUnit result = RenderBlock::paddingBottom(ExcludeIntrinsicPadding);
    if (paddingOption == ExcludeIntrinsicPadding)
        return result;
    switch (style()->writingMode()) {
    case TopToBottomWritingMode:
        return result + m_intrinsicPaddingAfter;
    case BottomToTopWritingMode:
        return result + m_intrinsicPaddingBefore;
    case LeftToRightWritingMode:
    case RightToLeftWritingMode:
        return result + (style()->isLeftToRightDirection() ? m_intrinsicPaddingEnd : m_intrinsicPaddingStart);
    }
    ASSERT_NOT_REACHED();
    return result;
}

LayoutUnit RenderMathMLBlock::paddingLeft(PaddingOptions paddingOption) const
{
    LayoutUnit result = RenderBlock::paddingLeft(ExcludeIntrinsicPadding);
    if (paddingOption == ExcludeIntrinsicPadding)
        return result;
    switch (style()->writingMode()) {
    case LeftToRightWritingMode:
        return result + m_intrinsicPaddingBefore;
    case RightToLeftWritingMode:
        return result + m_intrinsicPaddingAfter;
    case TopToBottomWritingMode:
    case BottomToTopWritingMode:
        return result + (style()->isLeftToRightDirection() ? m_intrinsicPaddingStart : m_intrinsicPaddingEnd);
    }
    ASSERT_NOT_REACHED();
    return result;
}

LayoutUnit RenderMathMLBlock::paddingRight(PaddingOptions paddingOption) const
{
    LayoutUnit result = RenderBlock::paddingRight(ExcludeIntrinsicPadding);
    if (paddingOption == ExcludeIntrinsicPadding)
        return result;
    switch (style()->writingMode()) {
    case RightToLeftWritingMode:
        return result + m_intrinsicPaddingBefore;
    case LeftToRightWritingMode:
        return result + m_intrinsicPaddingAfter;
    case TopToBottomWritingMode:
    case BottomToTopWritingMode:
        return result + (style()->isLeftToRightDirection() ? m_intrinsicPaddingEnd : m_intrinsicPaddingStart);
    }
    ASSERT_NOT_REACHED();
    return result;
}

LayoutUnit RenderMathMLBlock::paddingBefore(PaddingOptions paddingOption) const
{
    return RenderBlock::paddingBefore(ExcludeIntrinsicPadding) + (paddingOption == IncludeIntrinsicPadding ? m_intrinsicPaddingBefore : 0);
}

LayoutUnit RenderMathMLBlock::paddingAfter(PaddingOptions paddingOption) const
{
    return RenderBlock::paddingAfter(ExcludeIntrinsicPadding) + (paddingOption == IncludeIntrinsicPadding ? m_intrinsicPaddingAfter : 0);
}

LayoutUnit RenderMathMLBlock::paddingStart(PaddingOptions paddingOption) const
{
    return RenderBlock::paddingStart(ExcludeIntrinsicPadding) + (paddingOption == IncludeIntrinsicPadding ? m_intrinsicPaddingStart : 0);
}

LayoutUnit RenderMathMLBlock::paddingEnd(PaddingOptions paddingOption) const
{
    return RenderBlock::paddingEnd(ExcludeIntrinsicPadding) + (paddingOption == IncludeIntrinsicPadding ? m_intrinsicPaddingEnd : 0);
}

RenderMathMLBlock* RenderMathMLBlock::createAlmostAnonymousBlock(EDisplay display)
{
    RefPtr<RenderStyle> newStyle = RenderStyle::createAnonymousStyle(style());
    newStyle->setDisplay(display);
    
    RenderMathMLBlock* newBlock = new (renderArena()) RenderMathMLBlock(node() /* "almost" anonymous block */);
    newBlock->setStyle(newStyle.release());
    return newBlock;
}

void RenderMathMLBlock::stretchToHeight(int height) 
{
    for (RenderObject* current = firstChild(); current; current = current->nextSibling())
       if (current->isRenderMathMLBlock()) {
          RenderMathMLBlock* block = toRenderMathMLBlock(current);
          block->stretchToHeight(height);
       }
}

#if ENABLE(DEBUG_MATH_LAYOUT)
void RenderMathMLBlock::paint(PaintInfo& info, const LayoutPoint& paintOffset)
{
    RenderBlock::paint(info, paintOffset);
    
    if (info.context->paintingDisabled() || info.phase != PaintPhaseForeground)
        return;

    IntPoint adjustedPaintOffset = roundedIntPoint(paintOffset + location());

    GraphicsContextStateSaver stateSaver(*info.context);
    
    info.context->setStrokeThickness(1.0f);
    info.context->setStrokeStyle(SolidStroke);
    info.context->setStrokeColor(Color(0, 0, 255), ColorSpaceSRGB);
    
    info.context->drawLine(adjustedPaintOffset, IntPoint(adjustedPaintOffset.x() + pixelSnappedOffsetWidth(), adjustedPaintOffset.y()));
    info.context->drawLine(IntPoint(adjustedPaintOffset.x() + pixelSnappedOffsetWidth(), adjustedPaintOffset.y()), IntPoint(adjustedPaintOffset.x() + pixelSnappedOffsetWidth(), adjustedPaintOffset.y() + pixelSnappedOffsetHeight()));
    info.context->drawLine(IntPoint(adjustedPaintOffset.x(), adjustedPaintOffset.y() + pixelSnappedOffsetHeight()), IntPoint(adjustedPaintOffset.x() + pixelSnappedOffsetWidth(), adjustedPaintOffset.y() + pixelSnappedOffsetHeight()));
    info.context->drawLine(adjustedPaintOffset, IntPoint(adjustedPaintOffset.x(), adjustedPaintOffset.y() + pixelSnappedOffsetHeight()));
    
    int topStart = paddingTop();
    
    info.context->setStrokeColor(Color(0, 255, 0), ColorSpaceSRGB);
    
    info.context->drawLine(IntPoint(adjustedPaintOffset.x(), adjustedPaintOffset.y() + topStart), IntPoint(adjustedPaintOffset.x() + pixelSnappedOffsetWidth(), adjustedPaintOffset.y() + topStart));
    
    int baseline = roundToInt(baselinePosition(AlphabeticBaseline, true, HorizontalLine));
    
    info.context->setStrokeColor(Color(255, 0, 0), ColorSpaceSRGB);
    
    info.context->drawLine(IntPoint(adjustedPaintOffset.x(), adjustedPaintOffset.y() + baseline), IntPoint(adjustedPaintOffset.x() + pixelSnappedOffsetWidth(), adjustedPaintOffset.y() + baseline));
}
#endif // ENABLE(DEBUG_MATH_LAYOUT)

}    

#endif
