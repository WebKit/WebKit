/*
 * Copyright (C) 1997 Martin Jones (mjones@kde.org)
 *           (C) 1997 Torben Weis (weis@kde.org)
 *           (C) 1998 Waldo Bastian (bastian@kde.org)
 *           (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "RenderTableCol.h"

#include "BorderData.h"
#include "HTMLNames.h"
#include "HTMLTableColElement.h"
#include "RenderBoxInlines.h"
#include "RenderBoxModelObjectInlines.h"
#include "RenderChildIterator.h"
#include "RenderIterator.h"
#include "RenderStyleInlines.h"
#include "RenderTable.h"
#include "RenderTableCaption.h"
#include "RenderTableCell.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

using namespace HTMLNames;

WTF_MAKE_ISO_ALLOCATED_IMPL(RenderTableCol);

RenderTableCol::RenderTableCol(Element& element, RenderStyle&& style)
    : RenderBox(Type::TableCol, element, WTFMove(style))
{
    // init RenderObject attributes
    setInline(true); // our object is not Inline
    updateFromElement();
    ASSERT(isRenderTableCol());
}

RenderTableCol::RenderTableCol(Document& document, RenderStyle&& style)
    : RenderBox(Type::TableCol, document, WTFMove(style))
{
    setInline(true);
    ASSERT(isRenderTableCol());
}

void RenderTableCol::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderBox::styleDidChange(diff, oldStyle);
    RenderTable* table = this->table();
    if (!table)
        return;
    // If border was changed, notify table.
    if (!oldStyle)
        return;
    if (!oldStyle->borderIsEquivalentForPainting(style())) {
        table->invalidateCollapsedBorders();
        return;
    }
    if (oldStyle->width() != style().width()) {
        table->recalcSectionsIfNeeded();
        for (auto& section : childrenOfType<RenderTableSection>(*table)) {
            unsigned nEffCols = table->numEffCols();
            for (unsigned j = 0; j < nEffCols; j++) {
                unsigned rowCount = section.numRows();
                for (unsigned i = 0; i < rowCount; i++) {
                    RenderTableCell* cell = section.primaryCellAt(i, j);
                    if (!cell)
                        continue;
                    cell->setPreferredLogicalWidthsDirty(true);
                }
            }
        }
    }
}

void RenderTableCol::updateFromElement()
{
    ASSERT(element());
    unsigned oldSpan = m_span;
    if (element()->hasTagName(colTag) || element()->hasTagName(colgroupTag)) {
        HTMLTableColElement& tc = static_cast<HTMLTableColElement&>(*element());
        m_span = tc.span();
    } else
        m_span = 1;
    if (m_span != oldSpan && hasInitializedStyle() && parent())
        setNeedsLayoutAndPrefWidthsRecalc();
}

void RenderTableCol::insertedIntoTree(IsInternalMove isInternalMove)
{
    RenderBox::insertedIntoTree(isInternalMove);
    table()->addColumn(this);
}

void RenderTableCol::willBeRemovedFromTree(IsInternalMove isInternalMove)
{
    RenderBox::willBeRemovedFromTree(isInternalMove);
    if (auto* table = this->table()) {
        // We only need to invalidate the column cache when only individual columns are being removed (as opposed to when the entire table is being collapsed).
        table->invalidateColumns();
    }
}

bool RenderTableCol::isChildAllowed(const RenderObject& child, const RenderStyle& style) const
{
    // We cannot use isTableColumn here as style() may return 0.
    return style.display() == DisplayType::TableColumn && child.isRenderTableCol();
}

bool RenderTableCol::canHaveChildren() const
{
    // Cols cannot have children. This is actually necessary to fix a bug
    // with libraries.uc.edu, which makes a <p> be a table-column.
    return isTableColumnGroup();
}

LayoutRect RenderTableCol::clippedOverflowRect(const RenderLayerModelObject* repaintContainer, VisibleRectContext context) const
{
    // For now, just repaint the whole table.
    // FIXME: Find a better way to do this, e.g., need to repaint all the cells that we
    // might have propagated a background color or borders into.
    // FIXME: check for repaintContainer each time here?

    auto* parentTable = table();
    if (!parentTable)
        return { };

    return parentTable->clippedOverflowRect(repaintContainer, context);
}

auto RenderTableCol::rectsForRepaintingAfterLayout(const RenderLayerModelObject* repaintContainer, RepaintOutlineBounds) const -> RepaintRects
{
    // Ignore RepaintOutlineBounds because it doesn't make sense to use the table's outline bounds to repaint a column.
    return { clippedOverflowRect(repaintContainer, visibleRectContextForRepaint()) };
}

void RenderTableCol::imageChanged(WrappedImagePtr, const IntRect*)
{
    // FIXME: Repaint only the rect the image paints in.
    repaint();
}

void RenderTableCol::clearPreferredLogicalWidthsDirtyBits()
{
    setPreferredLogicalWidthsDirty(false);

    for (auto& child : childrenOfType<RenderObject>(*this))
        child.setPreferredLogicalWidthsDirty(false);
}

RenderTable* RenderTableCol::table() const
{
    auto table = parent();
    if (table && !is<RenderTable>(*table))
        table = table->parent();
    return dynamicDowncast<RenderTable>(table);
}

RenderTableCol* RenderTableCol::enclosingColumnGroup() const
{
    auto* parentColumnGroup = dynamicDowncast<RenderTableCol>(*parent());
    if (!parentColumnGroup)
        return nullptr;

    ASSERT(parentColumnGroup->isTableColumnGroup());
    ASSERT(isTableColumn());
    return parentColumnGroup;
}

RenderTableCol* RenderTableCol::nextColumn() const
{
    // If |this| is a column-group, the next column is the colgroup's first child column.
    if (RenderObject* firstChild = this->firstChild())
        return downcast<RenderTableCol>(firstChild);

    // Otherwise it's the next column along.
    RenderObject* next = nextSibling();

    // Failing that, the child is the last column in a column-group, so the next column is the next column/column-group after its column-group.
    if (!next && is<RenderTableCol>(*parent()))
        next = parent()->nextSibling();

    for (; next; next = next->nextSibling()) {
        if (auto* column = dynamicDowncast<RenderTableCol>(*next))
            return column;
    }

    return nullptr;
}

const BorderValue& RenderTableCol::borderAdjoiningCellStartBorder() const
{
    return style().borderStart();
}

const BorderValue& RenderTableCol::borderAdjoiningCellEndBorder() const
{
    return style().borderEnd();
}

const BorderValue& RenderTableCol::borderAdjoiningCellBefore(const RenderTableCell& cell) const
{
    ASSERT_UNUSED(cell, table()->colElement(cell.col() + cell.colSpan()) == this);
    return style().borderStart();
}

const BorderValue& RenderTableCol::borderAdjoiningCellAfter(const RenderTableCell& cell) const
{
    ASSERT_UNUSED(cell, table()->colElement(cell.col() - 1) == this);
    return style().borderEnd();
}

LayoutUnit RenderTableCol::offsetLeft() const
{
    return table()->offsetLeftForColumn(*this);
}

LayoutUnit RenderTableCol::offsetTop() const
{
    return table()->offsetTopForColumn(*this);
}

LayoutUnit RenderTableCol::offsetWidth() const
{
    return table()->offsetWidthForColumn(*this);
}

LayoutUnit RenderTableCol::offsetHeight() const
{
    return table()->offsetHeightForColumn(*this);
}

}
