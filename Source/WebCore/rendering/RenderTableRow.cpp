/**
 * Copyright (C) 1997 Martin Jones (mjones@kde.org)
 *           (C) 1997 Torben Weis (weis@kde.org)
 *           (C) 1998 Waldo Bastian (bastian@kde.org)
 *           (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
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
#include "RenderTableRow.h"

#include "Document.h"
#include "HTMLNames.h"
#include "HitTestResult.h"
#include "PaintInfo.h"
#include "RenderLayoutState.h"
#include "RenderTableCell.h"
#include "RenderTreeBuilder.h"
#include "RenderView.h"
#include "StyleInheritedData.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/StackStats.h>

namespace WebCore {

using namespace HTMLNames;

WTF_MAKE_ISO_ALLOCATED_IMPL(RenderTableRow);

RenderTableRow::RenderTableRow(Element& element, RenderStyle&& style)
    : RenderBox(element, WTFMove(style), 0)
    , m_rowIndex(unsetRowIndex)
{
    setInline(false);
    setIsTableRow();
}

RenderTableRow::RenderTableRow(Document& document, RenderStyle&& style)
    : RenderBox(document, WTFMove(style), 0)
    , m_rowIndex(unsetRowIndex)
{
    setInline(false);
    setIsTableRow();
}

void RenderTableRow::willBeRemovedFromTree(IsInternalMove isInternalMove)
{
    RenderBox::willBeRemovedFromTree(isInternalMove);

    section()->setNeedsCellRecalc();
}

static bool borderWidthChanged(const RenderStyle* oldStyle, const RenderStyle* newStyle)
{
    return oldStyle->borderLeftWidth() != newStyle->borderLeftWidth()
        || oldStyle->borderTopWidth() != newStyle->borderTopWidth()
        || oldStyle->borderRightWidth() != newStyle->borderRightWidth()
        || oldStyle->borderBottomWidth() != newStyle->borderBottomWidth();
}

void RenderTableRow::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    ASSERT(style().display() == DisplayType::TableRow);

    RenderBox::styleDidChange(diff, oldStyle);
    propagateStyleToAnonymousChildren(PropagateToAllChildren);

    if (section() && oldStyle && style().logicalHeight() != oldStyle->logicalHeight())
        section()->rowLogicalHeightChanged(rowIndex());

    // If border was changed, notify table.
    if (RenderTable* table = this->table()) {
        if (oldStyle && oldStyle->border() != style().border())
            table->invalidateCollapsedBorders();

        if (oldStyle && diff == StyleDifference::Layout && needsLayout() && table->collapseBorders() && borderWidthChanged(oldStyle, &style())) {
            // If the border width changes on a row, we need to make sure the cells in the row know to lay out again.
            // This only happens when borders are collapsed, since they end up affecting the border sides of the cell
            // itself.
            for (RenderTableCell* cell = firstCell(); cell; cell = cell->nextCell())
                cell->setChildNeedsLayout(MarkOnlyThis);
        }
    }
}

const BorderValue& RenderTableRow::borderAdjoiningStartCell(const RenderTableCell& cell) const
{
    ASSERT_UNUSED(cell, cell.isFirstOrLastCellInRow());
    // FIXME: https://webkit.org/b/79272 - Add support for mixed directionality at the cell level.
    return style().borderStart();
}

const BorderValue& RenderTableRow::borderAdjoiningEndCell(const RenderTableCell& cell) const
{
    ASSERT_UNUSED(cell, cell.isFirstOrLastCellInRow());
    // FIXME: https://webkit.org/b/79272 - Add support for mixed directionality at the cell level.
    return style().borderEnd();
}

void RenderTableRow::didInsertTableCell(RenderTableCell& child, RenderObject* beforeChild)
{
    // Generated content can result in us having a null section so make sure to null check our parent.
    if (auto* section = this->section()) {
        section->addCell(&child, this);
        if (beforeChild || nextRow())
            section->setNeedsCellRecalc();
    }
    if (auto* table = this->table())
        table->invalidateCollapsedBorders();
}

void RenderTableRow::layout()
{
    StackStats::LayoutCheckPoint layoutCheckPoint;
    ASSERT(needsLayout());

    // Table rows do not add translation.
    LayoutStateMaintainer statePusher(*this, LayoutSize(), hasTransform() || hasReflection() || style().isFlippedBlocksWritingMode());

    auto* layoutState = view().frameView().layoutContext().layoutState();
    bool paginated = layoutState->isPaginated();
                
    for (RenderTableCell* cell = firstCell(); cell; cell = cell->nextCell()) {
        if (!cell->needsLayout() && paginated && (layoutState->pageLogicalHeightChanged() || (layoutState->pageLogicalHeight() && layoutState->pageLogicalOffset(cell, cell->logicalTop()) != cell->pageLogicalOffset())))
            cell->setChildNeedsLayout(MarkOnlyThis);

        if (cell->needsLayout()) {
            cell->computeAndSetBlockDirectionMargins(*table());
            cell->layout();
        }
    }

    clearOverflow();
    addVisualEffectOverflow();
    // We only ever need to repaint if our cells didn't, which menas that they didn't need
    // layout, so we know that our bounds didn't change. This code is just making up for
    // the fact that we did not repaint in setStyle() because we had a layout hint.
    // We cannot call repaint() because our clippedOverflowRect() is taken from the
    // parent table, and being mid-layout, that is invalid. Instead, we repaint our cells.
    if (selfNeedsLayout() && checkForRepaintDuringLayout()) {
        for (RenderTableCell* cell = firstCell(); cell; cell = cell->nextCell())
            cell->repaint();
    }

    // RenderTableSection::layoutRows will set our logical height and width later, so it calls updateLayerTransform().
    clearNeedsLayout();
}

LayoutRect RenderTableRow::clippedOverflowRect(const RenderLayerModelObject* repaintContainer, VisibleRectContext context) const
{
    ASSERT(parent());
    // Rows and cells are in the same coordinate space. We need to both compute our overflow rect (which
    // will accommodate a row outline and any visual effects on the row itself), but we also need to add in
    // the repaint rects of cells.
    LayoutRect result = RenderBox::clippedOverflowRect(repaintContainer, context);
    for (RenderTableCell* cell = firstCell(); cell; cell = cell->nextCell()) {
        // Even if a cell is a repaint container, it's the row that paints the background behind it.
        // So we don't care if a cell is a repaintContainer here.
        result.uniteIfNonZero(cell->clippedOverflowRect(repaintContainer, context));
    }
    return result;
}

// Hit Testing
bool RenderTableRow::nodeAtPoint(const HitTestRequest& request, HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction action)
{
    // Table rows cannot ever be hit tested.  Effectively they do not exist.
    // Just forward to our children always.
    for (RenderTableCell* cell = lastCell(); cell; cell = cell->previousCell()) {
        // FIXME: We have to skip over inline flows, since they can show up inside table rows
        // at the moment (a demoted inline <form> for example). If we ever implement a
        // table-specific hit-test method (which we should do for performance reasons anyway),
        // then we can remove this check.
        if (!cell->hasSelfPaintingLayer()) {
            LayoutPoint cellPoint = flipForWritingModeForChild(cell, accumulatedOffset);
            if (cell->nodeAtPoint(request, result, locationInContainer, cellPoint, action)) {
                updateHitTestResult(result, locationInContainer.point() - toLayoutSize(cellPoint));
                return true;
            }
        }
    }

    return false;
}

void RenderTableRow::paintOutlineForRowIfNeeded(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    LayoutPoint adjustedPaintOffset = paintOffset + location();
    PaintPhase paintPhase = paintInfo.phase;
    if ((paintPhase == PaintPhase::Outline || paintPhase == PaintPhase::SelfOutline) && style().visibility() == Visibility::Visible)
        paintOutline(paintInfo, LayoutRect(adjustedPaintOffset, size()));
}

void RenderTableRow::paint(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    ASSERT(hasSelfPaintingLayer());

    paintOutlineForRowIfNeeded(paintInfo, paintOffset);
    for (RenderTableCell* cell = firstCell(); cell; cell = cell->nextCell()) {
        // Paint the row background behind the cell.
        if (paintInfo.phase == PaintPhase::BlockBackground || paintInfo.phase == PaintPhase::ChildBlockBackground)
            cell->paintBackgroundsBehindCell(paintInfo, paintOffset, this);
        if (!cell->hasSelfPaintingLayer())
            cell->paint(paintInfo, paintOffset);
    }
}

void RenderTableRow::imageChanged(WrappedImagePtr, const IntRect*)
{
    // FIXME: Examine cells and repaint only the rect the image paints in.
    repaint();
}

RenderPtr<RenderTableRow> RenderTableRow::createTableRowWithStyle(Document& document, const RenderStyle& style)
{
    auto row = createRenderer<RenderTableRow>(document, RenderStyle::createAnonymousStyleWithDisplay(style, DisplayType::TableRow));
    row->initializeStyle();
    return row;
}

RenderPtr<RenderTableRow> RenderTableRow::createAnonymousWithParentRenderer(const RenderTableSection& parent)
{
    return RenderTableRow::createTableRowWithStyle(parent.document(), parent.style());
}

} // namespace WebCore
