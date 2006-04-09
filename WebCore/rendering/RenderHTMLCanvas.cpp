/*
 * Copyright (C) 2004, 2006 Apple Computer, Inc.  All rights reserved.
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

#include "GraphicsContext.h"
#include "HTMLCanvasElement.h"
#include "HTMLNames.h"

namespace WebCore {

using namespace HTMLNames;

RenderHTMLCanvas::RenderHTMLCanvas(Node* n)
    : RenderReplaced(n)
{
}

const char* RenderHTMLCanvas::renderName() const
{
    return "RenderHTMLCanvas";
}

void RenderHTMLCanvas::paint(PaintInfo& i, int tx, int ty)
{
    if (!shouldPaint(i, tx, ty))
        return;

    int x = tx + m_x;
    int y = ty + m_y;

    if (shouldPaintBackgroundOrBorder() && (i.phase == PaintPhaseForeground || i.phase == PaintPhaseSelection)) 
        paintBoxDecorations(i, x, y);

    if ((i.phase == PaintPhaseOutline || i.phase == PaintPhaseSelfOutline) && style()->outlineWidth() && style()->visibility() == VISIBLE)
        paintOutline(i.p, x, y, width(), height(), style());
    
    if (i.phase != PaintPhaseForeground && i.phase != PaintPhaseSelection)
        return;

    if (!shouldPaintWithinRoot(i))
        return;

    bool drawSelectionTint = selectionState() != SelectionNone && !i.p->printing();
    if (i.phase == PaintPhaseSelection) {
        if (selectionState() == SelectionNone)
            return;
        drawSelectionTint = false;
    }

    if (element() && element()->hasTagName(canvasTag))
        static_cast<HTMLCanvasElement*>(element())->paint(i.p,
            IntRect(x + borderLeft() + paddingLeft(), y + borderTop() + paddingTop(), contentWidth(), contentHeight()));

    if (drawSelectionTint)
        i.p->fillRect(selectionRect(), selectionColor(i.p));
}

void RenderHTMLCanvas::layout()
{
    ASSERT(needsLayout());
    ASSERT(minMaxKnown());

    IntRect oldBounds;
    bool checkForRepaint = checkForRepaintDuringLayout();
    if (checkForRepaint)
        oldBounds = getAbsoluteRepaintRect();
    calcWidth();
    calcHeight();
    if (checkForRepaint)
        repaintAfterLayoutIfNeeded(oldBounds, oldBounds);

    setNeedsLayout(false);
}

}
