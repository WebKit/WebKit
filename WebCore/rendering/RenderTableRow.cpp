/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1997 Martin Jones (mjones@kde.org)
 *           (C) 1997 Torben Weis (weis@kde.org)
 *           (C) 1998 Waldo Bastian (bastian@kde.org)
 *           (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006 Apple Computer, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"
#include "RenderTableRow.h"
#include "RenderTableCell.h"
#include "Document.h"
#include "HTMLNames.h"

namespace WebCore {

using namespace HTMLNames;

RenderTableRow::RenderTableRow(Node* node)
    : RenderContainer(node)
{
    // init RenderObject attributes
    setInline(false);   // our object is not Inline
}

void RenderTableRow::destroy()
{
    RenderTableSection *s = section();
    if (s)
        s->setNeedCellRecalc();
    
    RenderContainer::destroy();
}

void RenderTableRow::setStyle(RenderStyle* style)
{
    style->setDisplay(TABLE_ROW);
    RenderContainer::setStyle(style);
}

void RenderTableRow::addChild(RenderObject* child, RenderObject* beforeChild)
{
    bool isTableRow = element() && element()->hasTagName(trTag);
    
    if (!child->isTableCell()) {
        if (isTableRow && child->element() && child->element()->hasTagName(formTag)) {
            RenderContainer::addChild(child, beforeChild);
            return;
        }

        RenderObject* last = beforeChild;
        if (!last)
            last = lastChild();
        if (last && last->isAnonymous() && last->isTableCell()) {
            last->addChild(child);
            return;
        }

        // If beforeChild is inside an anonymous cell, insert into the cell.
        if (last && !last->isTableCell() && last->parent() && last->parent()->isAnonymous()) {
            last->parent()->addChild(child, beforeChild);
            return;
        }

        RenderTableCell* cell = new (renderArena()) RenderTableCell(document() /* anonymous object */);
        RenderStyle* newStyle = new (renderArena()) RenderStyle();
        newStyle->inheritFrom(style());
        newStyle->setDisplay(TABLE_CELL);
        cell->setStyle(newStyle);
        addChild(cell, beforeChild);
        cell->addChild(child);
        return;
    }

    RenderTableCell* cell = static_cast<RenderTableCell*>(child);

    section()->addCell(cell, this);
    
    RenderContainer::addChild(cell, beforeChild);

    if (beforeChild || nextSibling())
        section()->setNeedCellRecalc();
}

void RenderTableRow::layout()
{
    KHTMLAssert(needsLayout());
    KHTMLAssert(minMaxKnown());

    for (RenderObject *child = firstChild(); child; child = child->nextSibling()) {
        if (child->isTableCell()) {
            RenderTableCell *cell = static_cast<RenderTableCell *>(child);
            if (child->needsLayout()) {
                cell->calcVerticalMargins();
                cell->layout();
            }
        }
    }
    setNeedsLayout(false);
}

IntRect RenderTableRow::getAbsoluteRepaintRect()
{
    // For now, just repaint the whole table.
    // FIXME: Find a better way to do this, e.g., need to repaint all the cells that we
    // might have propagated a background color into.
    RenderTable* parentTable = table();
    if (parentTable)
        return parentTable->getAbsoluteRepaintRect();
    else
        return IntRect();
}

// Hit Testing
bool RenderTableRow::nodeAtPoint(NodeInfo& info, int x, int y, int tx, int ty, HitTestAction action)
{
    // Table rows cannot ever be hit tested.  Effectively they do not exist.
    // Just forward to our children always.
    for (RenderObject* child = lastChild(); child; child = child->previousSibling()) {
        // FIXME: We have to skip over inline flows, since they can show up inside table rows
        // at the moment (a demoted inline <form> for example). If we ever implement a
        // table-specific hit-test method (which we should do for performance reasons anyway),
        // then we can remove this check.
        if (!child->layer() && !child->isInlineFlow() && child->nodeAtPoint(info, x, y, tx, ty, action)) {
            setInnerNode(info);
            return true;
        }
    }
    
    return false;
}

void RenderTableRow::paint(PaintInfo& i, int tx, int ty)
{
    assert(m_layer);
    if (!m_layer)
        return;

    for (RenderObject* child = firstChild(); child; child = child->nextSibling()) {
        if (child->isTableCell()) {
            // Paint the row background behind the cell.
            if (i.phase == PaintPhaseBlockBackground || i.phase == PaintPhaseChildBlockBackground) {
                RenderTableCell* cell = static_cast<RenderTableCell*>(child);
                cell->paintBackgroundsBehindCell(i, tx, ty, this);
            }
            if (!child->layer())
                child->paint(i, tx, ty);
        }
    }
}

}
