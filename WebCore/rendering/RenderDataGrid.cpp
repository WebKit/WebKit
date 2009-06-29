/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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

#if ENABLE(DATAGRID)

#include "RenderDataGrid.h"

#include "FocusController.h"
#include "Frame.h"
#include "GraphicsContext.h"
#include "Page.h"
#include "Scrollbar.h"

using std::min;

namespace WebCore {

static const int cDefaultWidth = 300;

RenderDataGrid::RenderDataGrid(Element* elt)
    : RenderBlock(elt)
{
}

RenderDataGrid::~RenderDataGrid()
{
}

void RenderDataGrid::calcPrefWidths()
{
    m_minPrefWidth = 0;
    m_maxPrefWidth = 0;

    if (style()->width().isFixed() && style()->width().value() > 0)
        m_minPrefWidth = m_maxPrefWidth = calcContentBoxWidth(style()->width().value());
    else
        m_maxPrefWidth = calcContentBoxWidth(cDefaultWidth);

    if (style()->minWidth().isFixed() && style()->minWidth().value() > 0) {
        m_maxPrefWidth = max(m_maxPrefWidth, calcContentBoxWidth(style()->minWidth().value()));
        m_minPrefWidth = max(m_minPrefWidth, calcContentBoxWidth(style()->minWidth().value()));
    } else if (style()->width().isPercent() || (style()->width().isAuto() && style()->height().isPercent()))
        m_minPrefWidth = 0;
    else
        m_minPrefWidth = m_maxPrefWidth;

    if (style()->maxWidth().isFixed() && style()->maxWidth().value() != undefinedLength) {
        m_maxPrefWidth = min(m_maxPrefWidth, calcContentBoxWidth(style()->maxWidth().value()));
        m_minPrefWidth = min(m_minPrefWidth, calcContentBoxWidth(style()->maxWidth().value()));
    }

    int toAdd = paddingLeft() + paddingRight() + borderLeft() + borderRight();
    m_minPrefWidth += toAdd;
    m_maxPrefWidth += toAdd;
                                
    setPrefWidthsDirty(false);
}

void RenderDataGrid::paintObject(PaintInfo& paintInfo, int tx, int ty)
{
    if (style()->visibility() != VISIBLE)
        return;

    // Paint our background and border.
    RenderBlock::paintObject(paintInfo, tx, ty);
    
    if (paintInfo.phase != PaintPhaseForeground)
        return;

    // Paint our column headers first.
    paintColumnHeaders(paintInfo, tx, ty);
}

void RenderDataGrid::paintColumnHeaders(PaintInfo&, int, int)
{
    gridElement()->columns();
    
}

void RenderDataGrid::rebuildColumns()
{
}

// Scrolling implementation functions
void RenderDataGrid::valueChanged(Scrollbar*)
{
    // FIXME: Implement.
}

void RenderDataGrid::invalidateScrollbarRect(Scrollbar*, const IntRect&)
{
    // FIXME: Implement.
}

bool RenderDataGrid::isActive() const
{
    Page* page = document()->frame()->page();
    return page && page->focusController()->isActive();
}

}

#endif
