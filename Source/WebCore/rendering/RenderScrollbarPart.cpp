/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "RenderScrollbarPart.h"

#include "PaintInfo.h"
#include "RenderScrollbar.h"
#include "RenderScrollbarTheme.h"
#include "RenderView.h"

using namespace std;

namespace WebCore {

RenderScrollbarPart::RenderScrollbarPart(Node* node, RenderScrollbar* scrollbar, ScrollbarPart part)
    : RenderBlock(node)
    , m_scrollbar(scrollbar)
    , m_part(part)
{
}

RenderScrollbarPart::~RenderScrollbarPart()
{
}

void RenderScrollbarPart::layout()
{
    setLocation(LayoutPoint()); // We don't worry about positioning ourselves. We're just determining our minimum width/height.
    if (m_scrollbar->orientation() == HorizontalScrollbar)
        layoutHorizontalPart();
    else
        layoutVerticalPart();

    setNeedsLayout(false);
}

void RenderScrollbarPart::layoutHorizontalPart()
{
    if (m_part == ScrollbarBGPart) {
        setWidth(m_scrollbar->width());
        computeScrollbarHeight();
    } else {
        computeScrollbarWidth();
        setHeight(m_scrollbar->height());
    }
}

void RenderScrollbarPart::layoutVerticalPart()
{
    if (m_part == ScrollbarBGPart) {
        computeScrollbarWidth();
        setHeight(m_scrollbar->height());
    } else {
        setWidth(m_scrollbar->width());
        computeScrollbarHeight();
    } 
}

static int calcScrollbarThicknessUsing(const Length& length, int containingLength, RenderView* renderView)
{
    if (length.isIntrinsicOrAuto())
        return ScrollbarTheme::theme()->scrollbarThickness();
    return minimumValueForLength(length, containingLength, renderView);
}

void RenderScrollbarPart::computeScrollbarWidth()
{
    if (!m_scrollbar->owningRenderer())
        return;
    RenderView* renderView = view();
    int visibleSize = m_scrollbar->owningRenderer()->width() - m_scrollbar->owningRenderer()->borderLeft() - m_scrollbar->owningRenderer()->borderRight();
    int w = calcScrollbarThicknessUsing(style()->width(), visibleSize, renderView);
    int minWidth = calcScrollbarThicknessUsing(style()->minWidth(), visibleSize, renderView);
    int maxWidth = style()->maxWidth().isUndefined() ? w : calcScrollbarThicknessUsing(style()->maxWidth(), visibleSize, renderView);
    setWidth(max(minWidth, min(maxWidth, w)));
    
    // Buttons and track pieces can all have margins along the axis of the scrollbar. 
    m_marginLeft = minimumValueForLength(style()->marginLeft(), visibleSize, renderView);
    m_marginRight = minimumValueForLength(style()->marginRight(), visibleSize, renderView);
}

void RenderScrollbarPart::computeScrollbarHeight()
{
    if (!m_scrollbar->owningRenderer())
        return;
    RenderView* renderView = view();
    int visibleSize = m_scrollbar->owningRenderer()->height() -  m_scrollbar->owningRenderer()->borderTop() - m_scrollbar->owningRenderer()->borderBottom();
    int h = calcScrollbarThicknessUsing(style()->height(), visibleSize, renderView);
    int minHeight = calcScrollbarThicknessUsing(style()->minHeight(), visibleSize, renderView);
    int maxHeight = style()->maxHeight().isUndefined() ? h : calcScrollbarThicknessUsing(style()->maxHeight(), visibleSize, renderView);
    setHeight(max(minHeight, min(maxHeight, h)));

    // Buttons and track pieces can all have margins along the axis of the scrollbar. 
    m_marginTop = minimumValueForLength(style()->marginTop(), visibleSize, renderView);
    m_marginBottom = minimumValueForLength(style()->marginBottom(), visibleSize, renderView);
}

void RenderScrollbarPart::computePreferredLogicalWidths()
{
    if (!preferredLogicalWidthsDirty())
        return;
    
    m_minPreferredLogicalWidth = m_maxPreferredLogicalWidth = 0;

    setPreferredLogicalWidthsDirty(false);
}

void RenderScrollbarPart::styleWillChange(StyleDifference diff, const RenderStyle* newStyle)
{
    RenderBlock::styleWillChange(diff, newStyle);
    setInline(false);
}

void RenderScrollbarPart::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderBlock::styleDidChange(diff, oldStyle);
    setInline(false);
    setPositioned(false);
    setFloating(false);
    setHasOverflowClip(false);
    if (oldStyle && m_scrollbar && m_part != NoPart && diff >= StyleDifferenceRepaint)
        m_scrollbar->theme()->invalidatePart(m_scrollbar, m_part);
}

void RenderScrollbarPart::imageChanged(WrappedImagePtr image, const IntRect* rect)
{
    if (m_scrollbar && m_part != NoPart)
        m_scrollbar->theme()->invalidatePart(m_scrollbar, m_part);
    else {
        if (FrameView* frameView = view()->frameView()) {
            if (frameView->isFrameViewScrollCorner(this)) {
                frameView->invalidateScrollCorner(frameView->scrollCornerRect());
                return;
            }
        }
        
        RenderBlock::imageChanged(image, rect);
    }
}

void RenderScrollbarPart::paintIntoRect(GraphicsContext* graphicsContext, const LayoutPoint& paintOffset, const LayoutRect& rect)
{
    // Make sure our dimensions match the rect.
    setLocation(rect.location() - toSize(paintOffset));
    setWidth(rect.width());
    setHeight(rect.height());

    if (graphicsContext->paintingDisabled())
        return;

    // Now do the paint.
    PaintInfo paintInfo(graphicsContext, pixelSnappedIntRect(rect), PaintPhaseBlockBackground, false, 0, 0, 0);
    paint(paintInfo, paintOffset);
    paintInfo.phase = PaintPhaseChildBlockBackgrounds;
    paint(paintInfo, paintOffset);
    paintInfo.phase = PaintPhaseFloat;
    paint(paintInfo, paintOffset);
    paintInfo.phase = PaintPhaseForeground;
    paint(paintInfo, paintOffset);
    paintInfo.phase = PaintPhaseOutline;
    paint(paintInfo, paintOffset);
}

}
