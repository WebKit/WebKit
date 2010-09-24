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

#include "CSSStyleSelector.h"
#include "FocusController.h"
#include "Frame.h"
#include "GraphicsContext.h"
#include "Page.h"
#include "RenderView.h"
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

void RenderDataGrid::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderBlock::styleDidChange(diff, oldStyle);
    recalcStyleForColumns();
}

void RenderDataGrid::recalcStyleForColumns()
{
    DataGridColumnList* columns = gridElement()->columns();
    unsigned length = columns->length();
    for (unsigned i = 0; i < length; ++i)
        recalcStyleForColumn(columns->item(i));
}

void RenderDataGrid::recalcStyleForColumn(DataGridColumn* column)
{
    if (!column->columnStyle())
        column->setColumnStyle(document()->styleSelector()->pseudoStyleForDataGridColumn(column, style()));
    if (!column->headerStyle())
        column->setHeaderStyle(document()->styleSelector()->pseudoStyleForDataGridColumnHeader(column, style()));
}

RenderStyle* RenderDataGrid::columnStyle(DataGridColumn* column)
{
    if (!column->columnStyle())
        recalcStyleForColumn(column);
    return column->columnStyle();
}

RenderStyle* RenderDataGrid::headerStyle(DataGridColumn* column)
{
    if (!column->headerStyle())
        recalcStyleForColumn(column);
    return column->headerStyle();
}

void RenderDataGrid::calcPrefWidths()
{
    m_minPrefWidth = 0;
    m_maxPrefWidth = 0;

    if (style()->width().isFixed() && style()->width().value() > 0)
        m_minPrefWidth = m_maxPrefWidth = computeContentBoxLogicalWidth(style()->width().value());
    else
        m_maxPrefWidth = computeContentBoxLogicalWidth(cDefaultWidth);

    if (style()->minWidth().isFixed() && style()->minWidth().value() > 0) {
        m_maxPrefWidth = max(m_maxPrefWidth, computeContentBoxLogicalWidth(style()->minWidth().value()));
        m_minPrefWidth = max(m_minPrefWidth, computeContentBoxLogicalWidth(style()->minWidth().value()));
    } else if (style()->width().isPercent() || (style()->width().isAuto() && style()->height().isPercent()))
        m_minPrefWidth = 0;
    else
        m_minPrefWidth = m_maxPrefWidth;

    if (style()->maxWidth().isFixed() && style()->maxWidth().value() != undefinedLength) {
        m_maxPrefWidth = min(m_maxPrefWidth, computeContentBoxLogicalWidth(style()->maxWidth().value()));
        m_minPrefWidth = min(m_minPrefWidth, computeContentBoxLogicalWidth(style()->maxWidth().value()));
    }

    int toAdd = borderAndPaddingWidth();
    m_minPrefWidth += toAdd;
    m_maxPrefWidth += toAdd;
                                
    setPrefWidthsDirty(false);
}

void RenderDataGrid::layout()
{
    RenderBlock::layout();
    layoutColumns();
}

void RenderDataGrid::layoutColumns()
{
    // FIXME: Implement.
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

void RenderDataGrid::paintColumnHeaders(PaintInfo& paintInfo, int tx, int ty)
{
    DataGridColumnList* columns = gridElement()->columns();
    unsigned length = columns->length();
    for (unsigned i = 0; i < length; ++i) {
        DataGridColumn* column = columns->item(i);
        RenderStyle* columnStyle = headerStyle(column);

        // Don't render invisible columns.
        if (!columnStyle || columnStyle->display() == NONE || columnStyle->visibility() != VISIBLE)
            continue;
        
        // Paint the column header if it intersects the dirty rect.
        IntRect columnRect(column->rect());
        columnRect.move(tx, ty);
        if (columnRect.intersects(paintInfo.rect))
            paintColumnHeader(column, paintInfo, tx, ty);
    }
}

void RenderDataGrid::paintColumnHeader(DataGridColumn*, PaintInfo&, int, int)
{
    // FIXME: Implement.
}

// Scrolling implementation functions
int RenderDataGrid::scrollSize(ScrollbarOrientation orientation) const
{
    return ((orientation == VerticallScrollbar) && m_vBar) ? (m_vBar->totalSize() - m_vBar->visibleSize()) : 0;
}

void RenderDataGrid::setScrollOffsetFromAnimation(const IntPoint& offset)
{
    if (m_vBar)
        m_vBar->setValue(offset.y(), Scrollbar::FromScrollAnimator);
}

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
    Page* page = frame()->page();
    return page && page->focusController()->isActive();
}


IntRect RenderDataGrid::convertFromScrollbarToContainingView(const Scrollbar* scrollbar, const IntRect& scrollbarRect) const
{
    RenderView* view = this->view();
    if (!view)
        return scrollbarRect;

    IntRect rect = scrollbarRect;

    int scrollbarLeft = width() - borderRight() - scrollbar->width();
    int scrollbarTop = borderTop();
    rect.move(scrollbarLeft, scrollbarTop);

    return view->frameView()->convertFromRenderer(this, rect);
}

IntRect RenderDataGrid::convertFromContainingViewToScrollbar(const Scrollbar* scrollbar, const IntRect& parentRect) const
{
    RenderView* view = this->view();
    if (!view)
        return parentRect;

    IntRect rect = view->frameView()->convertToRenderer(this, parentRect);

    int scrollbarLeft = width() - borderRight() - scrollbar->width();
    int scrollbarTop = borderTop();
    rect.move(-scrollbarLeft, -scrollbarTop);
    return rect;
}

IntPoint RenderDataGrid::convertFromScrollbarToContainingView(const Scrollbar* scrollbar, const IntPoint& scrollbarPoint) const
{
    RenderView* view = this->view();
    if (!view)
        return scrollbarPoint;

    IntPoint point = scrollbarPoint;

    int scrollbarLeft = width() - borderRight() - scrollbar->width();
    int scrollbarTop = borderTop();
    point.move(scrollbarLeft, scrollbarTop);

    return view->frameView()->convertFromRenderer(this, point);
}

IntPoint RenderDataGrid::convertFromContainingViewToScrollbar(const Scrollbar* scrollbar, const IntPoint& parentPoint) const
{
    RenderView* view = this->view();
    if (!view)
        return parentPoint;

    IntPoint point = view->frameView()->convertToRenderer(this, parentPoint);

    int scrollbarLeft = width() - borderRight() - scrollbar->width();
    int scrollbarTop = borderTop();
    point.move(-scrollbarLeft, -scrollbarTop);
    return point;
}

}

#endif
