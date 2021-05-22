/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "RenderTreeBuilderTable.h"

#include "RenderTableCaption.h"
#include "RenderTableCell.h"
#include "RenderTableCol.h"
#include "RenderTableRow.h"
#include "RenderTreeBuilder.h"

namespace WebCore {

RenderTreeBuilder::Table::Table(RenderTreeBuilder& builder)
    : m_builder(builder)
{
}

RenderElement& RenderTreeBuilder::Table::findOrCreateParentForChild(RenderTableRow& parent, const RenderObject& child, RenderObject*& beforeChild)
{
    if (is<RenderTableCell>(child))
        return parent;

    if (beforeChild && !beforeChild->isAnonymous() && beforeChild->parent() == &parent) {
        auto* previousSibling = beforeChild->previousSibling();
        if (is<RenderTableCell>(previousSibling) && previousSibling->isAnonymous()) {
            beforeChild = nullptr;
            return downcast<RenderElement>(*previousSibling);
        }
    }

    auto createAnonymousTableCell = [&] (auto& parent) -> RenderTableCell& {
        auto newCell = RenderTableCell::createAnonymousWithParentRenderer(parent);
        auto& cell = *newCell;
        m_builder.attach(parent, WTFMove(newCell), beforeChild);
        beforeChild = nullptr;
        return cell;
    };

    auto* lastChild = beforeChild ? beforeChild : parent.lastCell();
    if (lastChild) {
        if (is<RenderTableCell>(*lastChild) && lastChild->isAnonymous() && !lastChild->isBeforeOrAfterContent()) {
            if (beforeChild == lastChild)
                beforeChild = downcast<RenderElement>(*lastChild).firstChild();
            return downcast<RenderElement>(*lastChild);
        }

        // Try to find an anonymous container for the child.
        if (auto* lastChildParent = lastChild->parent()) {
            if (lastChildParent->isAnonymous() && !lastChildParent->isBeforeOrAfterContent()) {
                // If beforeChild is inside an anonymous COLGROUP, create a cell for the new renderer.
                if (is<RenderTableCol>(*lastChildParent))
                    return createAnonymousTableCell(parent);
                // If beforeChild is inside an anonymous cell, insert into the cell.
                if (!is<RenderTableCell>(*lastChild))
                    return *lastChildParent;
                // If beforeChild is inside an anonymous row, insert into the row.
                if (is<RenderTableRow>(*lastChildParent))
                    return createAnonymousTableCell(downcast<RenderTableRow>(*lastChildParent));
            }
        }
    }
    return createAnonymousTableCell(parent);
}

RenderElement& RenderTreeBuilder::Table::findOrCreateParentForChild(RenderTableSection& parent, const RenderObject& child, RenderObject*& beforeChild)
{
    if (is<RenderTableRow>(child))
        return parent;

    auto* lastChild = beforeChild ? beforeChild : parent.lastRow();
    if (is<RenderTableRow>(lastChild) && lastChild->isAnonymous() && !lastChild->isBeforeOrAfterContent()) {
        if (beforeChild == lastChild)
            beforeChild = downcast<RenderTableRow>(*lastChild).firstCell();
        return downcast<RenderElement>(*lastChild);
    }

    if (beforeChild && !beforeChild->isAnonymous() && beforeChild->parent() == &parent) {
        auto* row = beforeChild->previousSibling();
        if (is<RenderTableRow>(row) && row->isAnonymous()) {
            beforeChild = nullptr;
            return downcast<RenderElement>(*row);
        }
    }

    // If beforeChild is inside an anonymous cell/row, insert into the cell or into
    // the anonymous row containing it, if there is one.
    auto* parentCandidate = lastChild;
    while (parentCandidate && parentCandidate->parent() && parentCandidate->parent()->isAnonymous() && !is<RenderTableRow>(*parentCandidate))
        parentCandidate = parentCandidate->parent();
    if (is<RenderTableRow>(parentCandidate) && parentCandidate->isAnonymous() && !parentCandidate->isBeforeOrAfterContent())
        return downcast<RenderElement>(*parentCandidate);

    auto newRow = RenderTableRow::createAnonymousWithParentRenderer(parent);
    auto& row = *newRow;
    m_builder.attach(parent, WTFMove(newRow), beforeChild);
    beforeChild = nullptr;
    return row;
}

RenderElement& RenderTreeBuilder::Table::findOrCreateParentForChild(RenderTable& parent, const RenderObject& child, RenderObject*& beforeChild)
{
    if (is<RenderTableCaption>(child) || is<RenderTableSection>(child))
        return parent;

    if (is<RenderTableCol>(child)) {
        if (!child.node() || child.style().display() == DisplayType::TableColumnGroup) {
            // COLGROUPs and anonymous RenderTableCols (generated wrappers for COLs) are direct children of the table renderer.
            return parent;
        }
        auto newColGroup = createRenderer<RenderTableCol>(parent.document(), RenderStyle::createAnonymousStyleWithDisplay(parent.style(), DisplayType::TableColumnGroup));
        newColGroup->initializeStyle();
        auto& colGroup = *newColGroup;
        m_builder.attach(parent, WTFMove(newColGroup), beforeChild);
        beforeChild = nullptr;
        return colGroup;
    }

    auto* lastChild = parent.lastChild();
    if (!beforeChild && is<RenderTableSection>(lastChild) && lastChild->isAnonymous() && !lastChild->isBeforeContent())
        return downcast<RenderElement>(*lastChild);

    if (beforeChild && !beforeChild->isAnonymous() && beforeChild->parent() == &parent) {
        auto* section = beforeChild->previousSibling();
        if (is<RenderTableSection>(section) && section->isAnonymous()) {
            beforeChild = nullptr;
            return downcast<RenderElement>(*section);
        }
    }

    auto* parentCandidate = beforeChild;
    while (parentCandidate && parentCandidate->parent()->isAnonymous()
        && !is<RenderTableSection>(*parentCandidate)
        && parentCandidate->style().display() != DisplayType::TableCaption
        && parentCandidate->style().display() != DisplayType::TableColumnGroup)
        parentCandidate = parentCandidate->parent();

    if (parentCandidate) {
        if (beforeChild && !beforeChild->isAnonymous() && parentCandidate->parent() == &parent) {
            auto* section = parentCandidate->previousSibling();
            if (is<RenderTableSection>(section) && section->isAnonymous()) {
                beforeChild = nullptr;
                return downcast<RenderElement>(*section);
            }
        }

        if (is<RenderTableSection>(*parentCandidate) && parentCandidate->isAnonymous() && !parent.isAfterContent(parentCandidate)) {
            if (beforeChild == parentCandidate)
                beforeChild = downcast<RenderTableSection>(*parentCandidate).firstRow();
            return downcast<RenderElement>(*parentCandidate);
        }
    }

    if (beforeChild && !is<RenderTableSection>(*beforeChild)
        && beforeChild->style().display() != DisplayType::TableCaption
        && beforeChild->style().display() != DisplayType::TableColumnGroup)
        beforeChild = nullptr;

    auto newSection = RenderTableSection::createAnonymousWithParentRenderer(parent);
    auto& section = *newSection;
    m_builder.attach(parent, WTFMove(newSection), beforeChild);
    beforeChild = nullptr;
    return section;
}

void RenderTreeBuilder::Table::attach(RenderTableRow& parent, RenderPtr<RenderObject> child, RenderObject* beforeChild)
{
    if (beforeChild && beforeChild->parent() != &parent)
        beforeChild = m_builder.splitAnonymousBoxesAroundChild(parent, *beforeChild);

    auto& newChild = *child.get();
    ASSERT(!beforeChild || is<RenderTableCell>(*beforeChild));
    m_builder.attachToRenderElement(parent, WTFMove(child), beforeChild);
    // FIXME: child should always be a RenderTableCell at this point.
    if (is<RenderTableCell>(newChild))
        parent.didInsertTableCell(downcast<RenderTableCell>(newChild), beforeChild);
}

void RenderTreeBuilder::Table::attach(RenderTableSection& parent, RenderPtr<RenderObject> child, RenderObject* beforeChild)
{
    if (beforeChild && beforeChild->parent() != &parent)
        beforeChild = m_builder.splitAnonymousBoxesAroundChild(parent, *beforeChild);

    // FIXME: child should always be a RenderTableRow at this point.
    if (is<RenderTableRow>(*child.get()))
        parent.willInsertTableRow(downcast<RenderTableRow>(*child.get()), beforeChild);
    ASSERT(!beforeChild || is<RenderTableRow>(*beforeChild));
    m_builder.attachToRenderElement(parent, WTFMove(child), beforeChild);
}

void RenderTreeBuilder::Table::attach(RenderTable& parent, RenderPtr<RenderObject> child, RenderObject* beforeChild)
{
    if (beforeChild && beforeChild->parent() != &parent)
        beforeChild = m_builder.splitAnonymousBoxesAroundChild(parent, *beforeChild);

    auto& newChild = *child.get();
    if (is<RenderTableSection>(newChild))
        parent.willInsertTableSection(downcast<RenderTableSection>(newChild), beforeChild);
    else if (is<RenderTableCol>(newChild))
        parent.willInsertTableColumn(downcast<RenderTableCol>(newChild), beforeChild);

    m_builder.attachToRenderElement(parent, WTFMove(child), beforeChild);
}

bool RenderTreeBuilder::Table::childRequiresTable(const RenderElement& parent, const RenderObject& child)
{
    if (is<RenderTableCol>(child)) {
        const RenderTableCol& newTableColumn = downcast<RenderTableCol>(child);
        bool isColumnInColumnGroup = newTableColumn.isTableColumn() && is<RenderTableCol>(parent);
        return !is<RenderTable>(parent) && !isColumnInColumnGroup;
    }
    if (is<RenderTableCaption>(child))
        return !is<RenderTable>(parent);

    if (is<RenderTableSection>(child))
        return !is<RenderTable>(parent);

    if (is<RenderTableRow>(child))
        return !is<RenderTableSection>(parent);

    if (is<RenderTableCell>(child))
        return !is<RenderTableRow>(parent);

    return false;
}

static inline bool canCollapseNextSibling(const RenderBox& previousSibling, const RenderBox& nextSibling)
{
    if (!previousSibling.isAnonymous() || !nextSibling.isAnonymous())
        return false;
    auto* previousSiblingFirstInFlowChild = previousSibling.firstInFlowChild();
    auto* nextSiblingFirstInFlowChild = nextSibling.firstInFlowChild();
    // Do not try to collapse and move inline level boxes over to a container with block level boxes (and vice versa).
    return !previousSiblingFirstInFlowChild || !nextSiblingFirstInFlowChild || previousSiblingFirstInFlowChild->isInline() == nextSiblingFirstInFlowChild->isInline();
}

template <typename Parent, typename Child>
RenderPtr<RenderObject> RenderTreeBuilder::Table::collapseAndDetachAnonymousNextSibling(Parent* parent, Child* previousSibling, Child* nextSibling)
{
    if (!parent || !previousSibling || !nextSibling)
        return { };
    if (!canCollapseNextSibling(*previousSibling, *nextSibling))
        return { };
    m_builder.moveAllChildren(*nextSibling, *previousSibling, RenderTreeBuilder::NormalizeAfterInsertion::No);
    previousSibling->setChildrenInline(!previousSibling->firstInFlowChild() || previousSibling->firstInFlowChild()->isInline());
    return m_builder.detach(*parent, *nextSibling);
}

void RenderTreeBuilder::Table::collapseAndDestroyAnonymousSiblingCells(const RenderTableCell& willBeDestroyed)
{
    if (auto nextCellToDestroy = collapseAndDetachAnonymousNextSibling(willBeDestroyed.row(), willBeDestroyed.previousCell(), willBeDestroyed.nextCell()))
        downcast<RenderTableCell>(*nextCellToDestroy).deleteLines();
}

void RenderTreeBuilder::Table::collapseAndDestroyAnonymousSiblingRows(const RenderTableRow& willBeDestroyed)
{
    auto toDestroy = collapseAndDetachAnonymousNextSibling(willBeDestroyed.section(), willBeDestroyed.previousRow(), willBeDestroyed.nextRow());
}

}
