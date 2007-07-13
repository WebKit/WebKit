/*
 * Copyright (C) 2004, 2006, 2007 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "RenderHTMLCanvas.h"

#include "Document.h"
#include "GraphicsContext.h"
#include "HTMLCanvasElement.h"
#include "HTMLNames.h"
#include "RenderView.h"

namespace WebCore {

using namespace HTMLNames;

RenderHTMLCanvas::RenderHTMLCanvas(HTMLCanvasElement* element)
    : RenderReplaced(element, element->size())
{
}

void RenderHTMLCanvas::paint(PaintInfo& paintInfo, int tx, int ty)
{
    if (!shouldPaint(paintInfo, tx, ty))
        return;

    int x = tx + m_x;
    int y = ty + m_y;

    if (hasBoxDecorations() && (paintInfo.phase == PaintPhaseForeground || paintInfo.phase == PaintPhaseSelection)) 
        paintBoxDecorations(paintInfo, x, y);

    if ((paintInfo.phase == PaintPhaseOutline || paintInfo.phase == PaintPhaseSelfOutline) && style()->outlineWidth() && style()->visibility() == VISIBLE)
        paintOutline(paintInfo.context, x, y, width(), height(), style());

    if (paintInfo.phase != PaintPhaseForeground && paintInfo.phase != PaintPhaseSelection)
        return;

    if (!shouldPaintWithinRoot(paintInfo))
        return;

    bool drawSelectionTint = selectionState() != SelectionNone && !document()->printing();
    if (paintInfo.phase == PaintPhaseSelection) {
        if (selectionState() == SelectionNone)
            return;
        drawSelectionTint = false;
    }

    static_cast<HTMLCanvasElement*>(node())->paint(paintInfo.context,
        IntRect(x + borderLeft() + paddingLeft(), y + borderTop() + paddingTop(), contentWidth(), contentHeight()));

    if (drawSelectionTint)
        paintInfo.context->fillRect(selectionRect(), selectionBackgroundColor());
}

void RenderHTMLCanvas::layout()
{
    ASSERT(needsLayout());

    IntRect oldBounds;
    IntRect oldOutlineBox;
    bool checkForRepaint = checkForRepaintDuringLayout();
    if (checkForRepaint) {
        oldBounds = absoluteClippedOverflowRect();
        oldOutlineBox = absoluteOutlineBox();
    }
    calcWidth();
    calcHeight();
    adjustOverflowForBoxShadow();
    if (checkForRepaint)
        repaintAfterLayoutIfNeeded(oldBounds, oldOutlineBox);

    setNeedsLayout(false);
}

void RenderHTMLCanvas::canvasSizeChanged()
{
    IntSize size = static_cast<HTMLCanvasElement*>(node())->size();
    if (size == intrinsicSize())
        return;

    setIntrinsicSize(size);

    if (!prefWidthsDirty())
        setPrefWidthsDirty(true);

    IntSize oldSize = IntSize(m_width, m_height);
    calcWidth();
    calcHeight();
    if (oldSize == IntSize(m_width, m_height))
        return;

    if (!selfNeedsLayout())
        setNeedsLayout(true);
}

} // namespace WebCore
