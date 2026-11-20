/*
 * Copyright (C) 2017-2024 Apple Inc. All rights reserved.
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

#include "RenderElementInlines.h"
#include "RenderObjectStyle.h"
#include "RenderTableCaption.h"
#include "RenderTableCell.h"
#include "RenderTableCol.h"
#include "RenderTableRow.h"
#include "RenderTreeBuilder.h"
#include "StyleComputedStyle+GettersInlines.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RenderTreeBuilder::Table);

RenderTreeBuilder::Table::Table(RenderTreeBuilder& builder)
    : m_builder(builder)
{
}

CheckedRef<RenderElement> RenderTreeBuilder::Table::findOrCreateParentForChild(RenderTableRow& parent, const RenderObject& child, RenderObject*& beforeChild)
{
    if (is<RenderTableCell>(child))
        return parent;

    if (beforeChild && !beforeChild->isAnonymous() && beforeChild->parent() == &parent) {
        auto* previousSibling = beforeChild->previousSibling();
        if (auto* tableCell = dynamicDowncast<RenderTableCell>(previousSibling); tableCell && tableCell->isAnonymous()) {
            beforeChild = nullptr;
            return *tableCell;
        }
    }

    auto createAnonymousTableCell = [&](RenderTableRow& parent) -> CheckedRef<RenderTableCell> {
        auto newCell = createAnonymousTableCellWithStyle(protect(parent.document()), protect(parent.style()));
        CheckedRef cell = *newCell;
        m_builder.attach(parent, WTF::move(newCell), beforeChild);
        beforeChild = nullptr;
        return cell;
    };

    CheckedPtr<RenderObject> lastChild = beforeChild ? beforeChild : parent.lastCell();
    if (lastChild) {
        if (auto* tableCell = dynamicDowncast<RenderTableCell>(*lastChild); tableCell && tableCell->isAnonymous() && !tableCell->isBeforeOrAfterContent()) {
            if (beforeChild == lastChild)
                beforeChild = tableCell->firstChild();
            return *tableCell;
        }

        // Try to find an anonymous container for the child.
        if (CheckedPtr lastChildParent = lastChild->parent()) {
            if (lastChildParent->isAnonymous() && !lastChildParent->isBeforeOrAfterContent()) {
                // If beforeChild is inside an anonymous COLGROUP, create a cell for the new renderer.
                if (is<RenderTableCol>(*lastChildParent))
                    return createAnonymousTableCell(parent);
                // If beforeChild is inside an anonymous cell, insert into the cell.
                if (!is<RenderTableCell>(*lastChild))
                    return *lastChildParent;
                // If beforeChild is inside an anonymous row, insert into the row.
                if (CheckedPtr tableRow = dynamicDowncast<RenderTableRow>(*lastChildParent))
                    return createAnonymousTableCell(*tableRow);
            }
        }
    }
    return createAnonymousTableCell(parent);
}

CheckedRef<RenderElement> RenderTreeBuilder::Table::findOrCreateParentForChild(RenderTableSection& parent, const RenderObject& child, RenderObject*& beforeChild)
{
    if (is<RenderTableRow>(child))
        return parent;

    CheckedPtr<RenderObject> lastChild = beforeChild ? beforeChild : parent.lastRow();
    if (auto* tableRow = dynamicDowncast<RenderTableRow>(lastChild); tableRow && tableRow->isAnonymous() && !tableRow->isBeforeOrAfterContent()) {
        if (beforeChild == lastChild)
            beforeChild = tableRow->firstCell();
        return *tableRow;
    }

    if (beforeChild && !beforeChild->isAnonymous() && beforeChild->parent() == &parent) {
        auto* row = beforeChild->previousSibling();
        if (auto* tableRow = dynamicDowncast<RenderTableRow>(row); tableRow && tableRow->isAnonymous()) {
            beforeChild = nullptr;
            return *tableRow;
        }
    }

    // If beforeChild is inside an anonymous cell/row, insert into the cell or into
    // the anonymous row containing it, if there is one.
    CheckedPtr parentCandidate = lastChild;
    while (parentCandidate && parentCandidate->parent() && parentCandidate->parent()->isAnonymous() && !is<RenderTableRow>(*parentCandidate))
        parentCandidate = parentCandidate->parent();
    if (auto* tableRow = dynamicDowncast<RenderTableRow>(parentCandidate); tableRow && tableRow->isAnonymous() && !tableRow->isBeforeOrAfterContent())
        return *tableRow;

    auto newRow = createAnonymousTableRowWithStyle(protect(parent.document()), protect(parent.style()));
    CheckedRef row = *newRow;
    m_builder.attach(parent, WTF::move(newRow), beforeChild);
    beforeChild = nullptr;
    return row;
}

CheckedRef<RenderElement> RenderTreeBuilder::Table::findOrCreateParentForChild(RenderTable& parent, const RenderObject& child, RenderObject*& beforeChild)
{
    if (isAnyOf<RenderTableCaption, RenderTableSection>(child))
        return parent;

    if (CheckedPtr tableColumn = dynamicDowncast<RenderTableCol>(child)) {
        if (!tableColumn->element() || tableColumn->style().display() == Style::DisplayType::TableColumnGroup) {
            // COLGROUPs and anonymous RenderTableCols (generated wrappers for COLs) are direct children of the table renderer.
            return parent;
        }
        auto colGroupStyle = Style::ComputedStyle::createAnonymousStyleWithDisplay(protect(parent.style()), Style::DisplayType::TableColumnGroup);
        auto newColGroup = createRenderer<RenderTableCol>(protect(parent.document()), WTF::move(colGroupStyle));
        newColGroup->initializeStyle();
        CheckedRef colGroup = *newColGroup;
        m_builder.attach(parent, WTF::move(newColGroup), beforeChild);
        beforeChild = nullptr;
        return colGroup;
    }

    CheckedPtr lastChild = parent.lastChild();
    if (auto* tableSection = dynamicDowncast<RenderTableSection>(lastChild); !beforeChild && tableSection && tableSection->isAnonymous() && !tableSection->isBeforeContent())
        return *tableSection;

    if (beforeChild && !beforeChild->isAnonymous() && beforeChild->parent() == &parent) {
        auto* section = beforeChild->previousSibling();
        if (auto* tableSection = dynamicDowncast<RenderTableSection>(section); tableSection && tableSection->isAnonymous()) {
            beforeChild = nullptr;
            return *tableSection;
        }
    }

    CheckedPtr parentCandidate = beforeChild;
    while (parentCandidate && parentCandidate->parent()->isAnonymous()
        && !is<RenderTableSection>(*parentCandidate)
        && parentCandidate->style().display() != Style::DisplayType::TableCaption
        && parentCandidate->style().display() != Style::DisplayType::TableColumnGroup)
        parentCandidate = parentCandidate->parent();

    if (parentCandidate) {
        if (beforeChild && !beforeChild->isAnonymous() && parentCandidate->parent() == &parent) {
            auto* section = parentCandidate->previousSibling();
            if (auto* tableSection = dynamicDowncast<RenderTableSection>(section); tableSection && tableSection->isAnonymous()) {
                beforeChild = nullptr;
                return *tableSection;
            }
        }

        if (auto* parentTableSection = dynamicDowncast<RenderTableSection>(*parentCandidate); parentTableSection && parentTableSection->isAnonymous() && !parent.isAfterContent(parentTableSection)) {
            if (beforeChild == parentCandidate)
                beforeChild = parentTableSection->firstRow();
            return *parentTableSection;
        }
    }

    if (beforeChild && !is<RenderTableSection>(*beforeChild)
        && beforeChild->style().display() != Style::DisplayType::TableCaption
        && beforeChild->style().display() != Style::DisplayType::TableColumnGroup)
        beforeChild = nullptr;

    auto newSection = createAnonymousTableSectionWithStyle(protect(parent.document()), protect(parent.style()));
    CheckedRef section = *newSection;
    m_builder.attach(parent, WTF::move(newSection), beforeChild);
    beforeChild = nullptr;
    return section;
}

void RenderTreeBuilder::Table::attach(RenderTableRow& parent, RenderPtr<RenderObject> child, RenderObject* originalBeforeChild)
{
    CheckedPtr beforeChild = originalBeforeChild;
    if (beforeChild && beforeChild->parent() != &parent)
        beforeChild = m_builder.splitAnonymousBoxesAroundChild(parent, *beforeChild);

    CheckedRef newChild = *child;
    ASSERT(!beforeChild || is<RenderTableCell>(*beforeChild));
    m_builder.attachToRenderElement(parent, WTF::move(child), beforeChild);
    // FIXME: child should always be a RenderTableCell at this point.
    if (CheckedPtr renderTableCell = dynamicDowncast<RenderTableCell>(WTF::move(newChild)))
        parent.didInsertTableCell(*renderTableCell, beforeChild);
}

void RenderTreeBuilder::Table::attach(RenderTableSection& parent, RenderPtr<RenderObject> child, RenderObject* originalBeforeChild)
{
    CheckedPtr beforeChild = originalBeforeChild;
    if (beforeChild && beforeChild->parent() != &parent)
        beforeChild = m_builder.splitAnonymousBoxesAroundChild(parent, *beforeChild);

    // FIXME: child should always be a RenderTableRow at this point.
    if (CheckedPtr renderTableRow = dynamicDowncast<RenderTableRow>(child))
        parent.willInsertTableRow(*renderTableRow, beforeChild);
    ASSERT(!beforeChild || is<RenderTableRow>(*beforeChild));
    m_builder.attachToRenderElement(parent, WTF::move(child), beforeChild);
}

void RenderTreeBuilder::Table::attach(RenderTable& parent, RenderPtr<RenderObject> child, RenderObject* originalBeforeChild)
{
    CheckedPtr beforeChild = originalBeforeChild;
    if (beforeChild && beforeChild->parent() != &parent)
        beforeChild = m_builder.splitAnonymousBoxesAroundChild(parent, *beforeChild);

    CheckedRef newChild = *child;
    if (CheckedPtr renderTableSection = dynamicDowncast<RenderTableSection>(newChild))
        parent.willInsertTableSection(*renderTableSection, beforeChild);
    else if (auto* renderTableCol = dynamicDowncast<RenderTableCol>(newChild))
        parent.willInsertTableColumn(*renderTableCol, beforeChild);

    m_builder.attachToRenderElement(parent, WTF::move(child), beforeChild);
}

bool RenderTreeBuilder::Table::childRequiresTable(const RenderElement& parent, const RenderObject& child)
{
    if (auto* newTableColumn = dynamicDowncast<RenderTableCol>(child)) {
        bool isColumnInColumnGroup = newTableColumn->isTableColumn() && is<RenderTableCol>(parent);
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

NODELETE static inline bool canCollapseNextSibling(const RenderBox& previousSibling, const RenderBox& nextSibling)
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
    return m_builder.detach(*parent, *nextSibling, WillBeDestroyed::Yes);
}

void RenderTreeBuilder::Table::collapseAndDestroyAnonymousSiblingCells(const RenderTableCell& willBeDestroyed)
{
    if (auto nextCellToDestroy = collapseAndDetachAnonymousNextSibling<RenderTableRow, RenderTableCell>(protect(willBeDestroyed.row()), protect(willBeDestroyed.previousCell()), protect(willBeDestroyed.nextCell())))
        downcast<RenderTableCell>(*nextCellToDestroy).invalidateLineLayout(RenderBlockFlow::InvalidationReason::InternalMove);
}

void RenderTreeBuilder::Table::collapseAndDestroyAnonymousSiblingRows(const RenderTableRow& willBeDestroyed)
{
    auto toDestroy = collapseAndDetachAnonymousNextSibling<RenderTableSection, RenderTableRow>(protect(willBeDestroyed.section()), protect(willBeDestroyed.previousRow()), protect(willBeDestroyed.nextRow()));
}

RenderPtr<RenderTable> RenderTreeBuilder::Table::createAnonymousTableWithStyle(Document& document, const Style::ComputedStyle& style)
{
    auto tableStyle = Style::ComputedStyle::createAnonymousStyleWithDisplay(style, style.display() == Style::DisplayType::InlineFlow ? Style::DisplayType::InlineTable : Style::DisplayType::BlockTable);
    auto table = createRenderer<RenderTable>(RenderObject::Type::Table, document, WTF::move(tableStyle));
    table->initializeStyle();
    return table;
}

RenderPtr<RenderTableCell> RenderTreeBuilder::Table::createAnonymousTableCellWithStyle(Document& document, const Style::ComputedStyle& style)
{
    auto cellStyle = Style::ComputedStyle::createAnonymousStyleWithDisplay(style, Style::DisplayType::TableCell);
    auto cell = createRenderer<RenderTableCell>(document, WTF::move(cellStyle));
    cell->initializeStyle();
    return cell;
}

RenderPtr<RenderTableRow> RenderTreeBuilder::Table::createAnonymousTableRowWithStyle(Document& document, const Style::ComputedStyle& style)
{
    auto rowStyle = Style::ComputedStyle::createAnonymousStyleWithDisplay(style, Style::DisplayType::TableRow);
    auto row = createRenderer<RenderTableRow>(document, WTF::move(rowStyle));
    row->initializeStyle();
    return row;
}

RenderPtr<RenderTableSection> RenderTreeBuilder::Table::createAnonymousTableSectionWithStyle(Document& document, const Style::ComputedStyle& style)
{
    auto sectionStyle = Style::ComputedStyle::createAnonymousStyleWithDisplay(style, Style::DisplayType::TableRowGroup);
    auto section = createRenderer<RenderTableSection>(document, WTF::move(sectionStyle));
    section->initializeStyle();
    return section;
}

}
