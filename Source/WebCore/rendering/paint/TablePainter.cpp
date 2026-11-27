/*
 * Copyright (C) 1997 Martin Jones (mjones@kde.org)
 *           (C) 1997 Torben Weis (weis@kde.org)
 *           (C) 1998 Waldo Bastian (bastian@kde.org)
 *           (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003-2026 Apple Inc. All rights reserved.
 * Copyright (C) 2014-2019 Google Inc. All rights reserved.
 * Copyright (C) 2006 Alexey Proskuryakov (ap@nypop.com)
 * Copyright (C) 2025-2026 Samuel Weinig <sam@webkit.org>
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
#include "TablePainter.h"

#include "AccessibilityRegionContext.h"
#include "BackgroundPainter.h"
#include "BorderPainter.h"
#include "BoxDecorationPainter.h"
#include "CollapsedBorderValue.h"
#include "Document.h"
#include "GraphicsContext.h"
#include "MaskPainter.h"
#include "PaintInfo.h"
#include "PaintInfoInlines.h"
#include "RenderBoxInlines.h"
#include "RenderBoxModelObjectInlines.h"
#include "RenderChildIterator.h"
#include "RenderElementInlines.h"
#include "RenderElementStyleInlines.h"
#include "RenderObjectInlines.h"
#include "RenderTable.h"
#include "RenderTableCaption.h"
#include "RenderTableCellInlines.h"
#include "RenderTableCol.h"
#include "RenderTableInlines.h"
#include "RenderTableRow.h"
#include "RenderTableSection.h"
#include "RenderTableSectionInlines.h"
#include "StyleBoxShadow.h"
#include "StylePrimitiveNumericTypes+Evaluation.h"
#include <ranges>
#include <wtf/Borrow.h>

namespace WebCore {
namespace Paint {

template<typename Function>
void TablePainter::paintCollapsedBorderPasses(RenderTable& table, const PaintInfo& paintInfo, NOESCAPE const Function& paintPass)
{
    ASSERT(table.collapseBorders());
    table.recalcCollapsedBorders();

    PaintInfo borderPaintInfo(paintInfo);
    borderPaintInfo.phase = PaintPhase::CollapsedTableBorders;

    for (auto& border : borrow(table.m_collapsedBorders))
        paintPass(borderPaintInfo, border);
}

void TablePainter::paint(RenderTable& table, PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    auto isSkippedContent = [&] {
        if (table.style().usedContentVisibility() == ContentVisibility::Visible)
            return false;
        // FIXME: Tables can never be skipped content roots. If a table is _inside_ a skipped subtree, we should have bailed out at the skipped root ancestor.
        if (CheckedPtr containingBlock = table.containingBlock(); containingBlock && containingBlock->isAnonymousBlock() && !containingBlock->style().isSkippedRootOrSkippedContent())
            return true;
        return false;
    };
    if (isSkippedContent())
        return;

    LayoutPoint adjustedPaintOffset = paintOffset + table.location();

    PaintPhase paintPhase = paintInfo.phase;

    if (!table.isDocumentElementRenderer()) {
        LayoutRect overflowBox = table.visualOverflowRect();
        table.blockStartRelativeToPhysical(overflowBox);
        overflowBox.moveBy(adjustedPaintOffset);
        if (!overflowBox.intersects(paintInfo.rect))
            return;
    }

    bool pushedClip = table.pushContentsClip(paintInfo, adjustedPaintOffset);
    paintObject(table, paintInfo, adjustedPaintOffset);
    if (pushedClip)
        table.popContentsClip(paintInfo, paintPhase, adjustedPaintOffset);
}

void TablePainter::paintObject(RenderTable& table, PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    PaintPhase paintPhase = paintInfo.phase;
    if ((paintPhase == PaintPhase::BlockBackground || paintPhase == PaintPhase::ChildBlockBackground) && table.hasVisibleBoxDecorations() && table.usedStyle().visibility() == UsedVisibility::Visible)
        BoxDecorationPainter { table, paintInfo, paintOffset }.paintBoxDecorations();

    if (paintPhase == PaintPhase::Mask) {
        MaskPainter { table, paintInfo, paintOffset }.paintMask();
        return;
    }

    if (paintPhase == PaintPhase::Accessibility)
        protect(paintInfo.accessibilityRegionContext())->takeBounds(table, paintOffset);

#if ENABLE(AX_CUSTOM_COLOR_MODE)
    if (paintPhase == PaintPhase::AXCustomColorCollectBackgrounds) {
        LayoutRect rect(paintOffset, table.borderBoxSize());
        table.adjustBorderBoxRectForPainting(rect);
        protect(paintInfo.axCustomColorBackdropContext())->recordBackdrop(table, FloatRect { rect }, paintInfo.paintBehavior);
    }
#endif

    // We're done. We don't bother painting any children.
    if (paintPhase == PaintPhase::BlockBackground)
        return;

    // We don't paint our own background, but we do let the kids paint their backgrounds.
    if (paintPhase == PaintPhase::ChildBlockBackgrounds)
        paintPhase = PaintPhase::ChildBlockBackground;

    PaintInfo info(paintInfo);
    info.phase = paintPhase;
    info.updateSubtreePaintRootForChildren(&table);

    for (CheckedRef box : childrenOfType<RenderBox>(table)) {
        if (!box->hasSelfPaintingLayer() && (box->isRenderTableSection() || box->isRenderTableCaption())) {
            LayoutPoint childPoint = table.blockStartRelativeToPhysicalForChild(box, paintOffset);
            box->paint(info, childPoint);
        }
    }

    if (table.collapseBorders() && paintPhase == PaintPhase::ChildBlockBackground && table.usedStyle().visibility() == UsedVisibility::Visible) {
        paintCollapsedBorderPasses(table, info, [&](PaintInfo& borderPaintInfo, const CollapsedBorderValue& currentBorder) {
            for (CheckedPtr section = table.bottomSection(); section; section = table.sectionAbove(section)) {
                LayoutPoint childPoint = table.blockStartRelativeToPhysicalForChild(*section, paintOffset);
                paintSection(*section, borderPaintInfo, childPoint, &currentBorder);
            }
        });
    }

    // Paint outline.
    if ((paintPhase == PaintPhase::Outline || paintPhase == PaintPhase::SelfOutline) && table.hasOutline() && table.usedStyle().visibility() == UsedVisibility::Visible)
        table.paintOutline(paintInfo, LayoutRect(paintOffset, table.borderBoxSize()));
}

void TablePainter::paintCollapsedBordersForRow(RenderTable& table, PaintInfo& paintInfo, RenderTableRow& row, const LayoutPoint& paintOffset)
{
    paintCollapsedBorderPasses(table, paintInfo, [&](PaintInfo& borderPaintInfo, const CollapsedBorderValue& currentBorder) {
        for (CheckedPtr cell = row.firstCell(); cell; cell = cell->nextCell()) {
            if (!cell->hasSelfPaintingLayer()) {
                auto cellPoint = row.blockStartRelativeToPhysicalForChild(*cell, paintOffset);
                paintCollapsedBorders(*cell, borderPaintInfo, cellPoint, currentBorder);
            }
        }
    });
}

NODELETE static inline bool compareCellPositions(const SingleThreadWeakPtr<RenderTableCell>& elem1, const SingleThreadWeakPtr<RenderTableCell>& elem2)
{
    return elem1->rowIndex() < elem2->rowIndex();
}

// This comparison is used only when we have overflowing cells as we have an unsorted array to sort. We thus need
// to sort both on rows and columns to properly repaint.
NODELETE static inline bool compareCellPositionsWithOverflowingCells(const SingleThreadWeakPtr<RenderTableCell>& elem1, const SingleThreadWeakPtr<RenderTableCell>& elem2)
{
    if (elem1->rowIndex() != elem2->rowIndex())
        return elem1->rowIndex() < elem2->rowIndex();

    return elem1->col() < elem2->col();
}

void TablePainter::paint(RenderTableSection& section, PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    paintSection(section, paintInfo, paintOffset, nullptr);
}

void TablePainter::paintSection(RenderTableSection& section, PaintInfo& paintInfo, const LayoutPoint& paintOffset, const CollapsedBorderValue* currentBorder)
{
    ASSERT(!section.needsLayout());
    // avoid crashing on bugs that cause us to paint with dirty layout
    if (section.needsLayout())
        return;

    unsigned totalRows = section.m_grid.size();
    unsigned totalCols = section.table()->columns().size();

    if (!totalRows || !totalCols)
        return;

    LayoutPoint adjustedPaintOffset = paintOffset + section.location();

    PaintPhase phase = paintInfo.phase;
    bool pushedClip = section.pushContentsClip(paintInfo, adjustedPaintOffset);
    paintContents(section, paintInfo, adjustedPaintOffset, currentBorder);
    if (pushedClip)
        section.popContentsClip(paintInfo, phase, adjustedPaintOffset);

    if ((phase == PaintPhase::Outline || phase == PaintPhase::SelfOutline) && section.usedStyle().visibility() == UsedVisibility::Visible)
        section.paintOutline(paintInfo, LayoutRect(adjustedPaintOffset, section.borderBoxSize()));
}

void TablePainter::paintObject(RenderTableSection& section, PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    paintContents(section, paintInfo, paintOffset, nullptr);
}

void TablePainter::paintCell(RenderTableSection& section, RenderTableCell& cell, PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    CheckedRef row = downcast<RenderTableRow>(*cell.parent());
    auto rowBackgroundPaintOffset = section.blockStartRelativeToPhysicalForChild(row, paintOffset);
    auto rowPaintOffset = rowBackgroundPaintOffset + row->location();
    auto cellPoint = row->blockStartRelativeToPhysicalForChild(cell, rowPaintOffset);
    PaintPhase paintPhase = paintInfo.phase;

    if (paintPhase == PaintPhase::BlockBackground || paintPhase == PaintPhase::ChildBlockBackground) {
        // We need to handle painting a stack of backgrounds. This stack (from bottom to top) consists of
        // the column group, column, row group, row, and then the cell.

        // Column groups and columns first.
        // FIXME: Columns and column groups do not currently support opacity, and they are being painted "too late" in
        // the stack, since we have already opened a transparency layer (potentially) for the table row group.
        // Note that we deliberately ignore whether or not the cell has a layer, since these backgrounds paint "behind" the
        // cell.
        if (CheckedPtr column = protect(section.table())->colElement(cell.col())) {
            if (CheckedPtr columnGroup = column->enclosingColumnGroup())
                paintBackgroundsBehindCell(cell, paintInfo, cellPoint, *columnGroup, cellPoint);
            paintBackgroundsBehindCell(cell, paintInfo, cellPoint, *column, cellPoint);
        }

        // Paint the row group next.
        paintBackgroundsBehindCell(cell, paintInfo, cellPoint, section, paintOffset);

        // Paint the row next, but only if it doesn't have a layer. If a row has a layer, it will be responsible for
        // painting the row background for the cell.
        if (!row->hasSelfPaintingLayer())
            paintBackgroundsBehindCell(cell, paintInfo, cellPoint, row, rowBackgroundPaintOffset);
    }
    if ((!cell.hasSelfPaintingLayer() && !row->hasSelfPaintingLayer()))
        cell.paint(paintInfo, cellPoint);
}

NODELETE static BoxSide physicalBorderForDirection(const WritingMode writingMode, CollapsedBorderSide side)
{
    // FIXME: Replace this with types/methods from BoxSides.h
    switch (side) {
    case CollapsedBorderSide::Start:
        if (writingMode.isHorizontal())
            return writingMode.isInlineLeftToRight() ? BoxSide::Left : BoxSide::Right;
        return writingMode.isInlineTopToBottom() ? BoxSide::Top : BoxSide::Bottom;
    case CollapsedBorderSide::End:
        if (writingMode.isHorizontal())
            return writingMode.isInlineLeftToRight() ? BoxSide::Right : BoxSide::Left;
        return writingMode.isInlineTopToBottom() ? BoxSide::Bottom : BoxSide::Top;
    case CollapsedBorderSide::Before:
        if (writingMode.isHorizontal())
            return writingMode.isBlockTopToBottom() ? BoxSide::Top : BoxSide::Bottom;
        return writingMode.isBlockLeftToRight() ? BoxSide::Left : BoxSide::Right;
    case CollapsedBorderSide::After:
        if (writingMode.isHorizontal())
            return writingMode.isBlockTopToBottom() ? BoxSide::Bottom : BoxSide::Top;
        return writingMode.isBlockLeftToRight() ? BoxSide::Right : BoxSide::Left;
    }
    ASSERT_NOT_REACHED();
    return BoxSide::Left;
}

void TablePainter::paintRowGroupBorder(RenderTableSection& section, const PaintInfo& paintInfo, bool antialias, LayoutRect rect, BoxSide side, CSSPropertyID borderColor, BorderStyle borderStyle, BorderStyle tableBorderStyle)
{
    if (tableBorderStyle == BorderStyle::Hidden)
        return;
    rect.intersect(paintInfo.rect);
    if (rect.isEmpty())
        return;
    BorderPainter::drawLineForBoxSide(paintInfo.context(), protect(section.document()), rect, side, section.rowGroupBorderColor(borderColor), borderStyle, 0, 0, antialias);
}

void TablePainter::paintRowGroupBorderIfRequired(RenderTableSection& section, const PaintInfo& paintInfo, const LayoutPoint& paintOffset, unsigned row, unsigned column, BoxSide borderSide, const CollapsedBorderValue& currentBorder, RenderTableCell* cell)
{
    if (currentBorder.precedence() > BorderPrecedence::RowGroup)
        return;
    if (paintInfo.context().paintingDisabled())
        return;

    CheckedRef style = section.style();
    bool antialias = BorderPainter::shouldAntialiasLines(paintInfo.context());
    LayoutRect rowGroupRect = LayoutRect(paintOffset, section.borderBoxSize());
    rowGroupRect.moveBy(-LayoutPoint(section.outerBorderLeft(section.table()->writingMode()), (borderSide == BoxSide::Right) ? 0_lu : section.outerBorderTop(section.table()->writingMode())));

    switch (borderSide) {
    case BoxSide::Top:
        paintRowGroupBorder(
            section,
            paintInfo,
            antialias,
            LayoutRect {
                paintOffset.x() + section.offsetLeftForRowGroupBorder(cell, rowGroupRect, row),
                rowGroupRect.y(),
                section.horizontalRowGroupBorderWidth(cell, rowGroupRect, row, column),
                Style::evaluate<LayoutUnit>(style->borderTop().width, style->usedZoomForLength(), style->deviceScaleFactor()),
            },
            BoxSide::Top,
            CSSPropertyID::BorderTopColor,
            style->borderTopStyle(),
            section.table()->style().borderTopStyle()
        );
        break;
    case BoxSide::Bottom:
        paintRowGroupBorder(
            section,
            paintInfo,
            antialias,
            LayoutRect {
                paintOffset.x() + section.offsetLeftForRowGroupBorder(cell, rowGroupRect, row),
                rowGroupRect.y() + rowGroupRect.height(),
                section.horizontalRowGroupBorderWidth(cell, rowGroupRect, row, column),
                Style::evaluate<LayoutUnit>(style->borderBottom().width, style->usedZoomForLength(), style->deviceScaleFactor()),
            },
            BoxSide::Bottom,
            CSSPropertyID::BorderBottomColor,
            style->borderBottomStyle(),
            section.table()->style().borderBottomStyle()
        );
        break;
    case BoxSide::Left:
        paintRowGroupBorder(
            section,
            paintInfo,
            antialias,
            LayoutRect {
                rowGroupRect.x(),
                rowGroupRect.y() + section.offsetTopForRowGroupBorder(cell, borderSide, row),
                Style::evaluate<LayoutUnit>(style->borderLeft().width, style->usedZoomForLength(), style->deviceScaleFactor()),
                section.verticalRowGroupBorderHeight(cell, rowGroupRect, row),
            },
            BoxSide::Left,
            CSSPropertyID::BorderLeftColor,
            style->borderLeftStyle(),
            section.table()->style().borderLeftStyle()
        );
        break;
    case BoxSide::Right:
        paintRowGroupBorder(
            section,
            paintInfo,
            antialias,
            LayoutRect {
                rowGroupRect.x() + rowGroupRect.width(),
                rowGroupRect.y() + section.offsetTopForRowGroupBorder(cell, borderSide, row),
                Style::evaluate<LayoutUnit>(style->borderRight().width, style->usedZoomForLength(), style->deviceScaleFactor()),
                section.verticalRowGroupBorderHeight(cell, rowGroupRect, row),
            },
            BoxSide::Right,
            CSSPropertyID::BorderRightColor,
            style->borderRightStyle(),
            section.table()->style().borderRightStyle()
        );
        break;
    default:
        break;
    }
}

void TablePainter::paintContents(RenderTableSection& section, PaintInfo& paintInfo, const LayoutPoint& paintOffset, const CollapsedBorderValue* currentBorder)
{
    ASSERT(!currentBorder == (paintInfo.phase != PaintPhase::CollapsedTableBorders));

    if (paintInfo.phase == PaintPhase::Accessibility) {
        if (CheckedPtr context = paintInfo.accessibilityRegionContext()) {
            context->takeBounds(section, paintOffset);
            for (auto& rowStruct : borrow(section.m_grid)) {
                if (CheckedPtr row = rowStruct.rowRenderer; row && !row->hasSelfPaintingLayer())
                    context->takeBounds(*row, paintOffset + row->location());
            }
        }
    }

    auto localRepaintRect = paintInfo.rect;
    localRepaintRect.moveBy(-paintOffset);

    CellSpan dirtiedRows { 0, 0 };
    CellSpan dirtiedColumns { 0, 0 };

    if (localRepaintRect.contains(section.borderBoxRectInContainer())) {
        dirtiedRows = section.fullTableRowSpan();
        dirtiedColumns = section.fullTableColumnSpan();
    } else {
        auto tableAlignedRect = section.logicalRectForWritingModeAndDirection(localRepaintRect);
        dirtiedRows = section.dirtiedRows(tableAlignedRect);
        dirtiedColumns = section.dirtiedColumns(tableAlignedRect);
    }

    if (dirtiedColumns.start == dirtiedColumns.end)
        return;

    auto paintOutlineForRow = [&](unsigned rowIndex, PaintPhase phase) {
        if (phase != PaintPhase::Outline && phase != PaintPhase::SelfOutline)
            return;

        CheckedPtr row = section.m_grid[rowIndex].rowRenderer;
        if (row && !row->hasSelfPaintingLayer() && row->hasOutline())
            paintRowOutline(*row, paintInfo, paintOffset);
    };

    auto paintShadowForRow = [&](unsigned rowIndex, PaintPhase phase) {
        if (phase != PaintPhase::BlockBackground && phase != PaintPhase::ChildBlockBackground)
            return;

        CheckedPtr row = section.m_grid[rowIndex].rowRenderer;
        if (row && !row->hasSelfPaintingLayer() && !row->style().boxShadow().isNone())
            paintRowShadow(*row, paintInfo, paintOffset);
    };

#if ENABLE(AX_CUSTOM_COLOR_MODE)
    auto recordRowBackdrop = [&](unsigned rowIndex, PaintPhase phase) {
        if (phase != PaintPhase::AXCustomColorCollectBackgrounds)
            return;

        CheckedPtr row = section.m_grid[rowIndex].rowRenderer;
        if (!row || row->hasSelfPaintingLayer())
            return;

        auto rowPaintOffset = section.blockStartRelativeToPhysicalForChild(*row, paintOffset) + row->location();
        auto rowBorderRect = FloatRect { LayoutRect(rowPaintOffset, row->borderBoxSize()) };
        protect(paintInfo.axCustomColorBackdropContext())->recordBackdrop(*row, rowBorderRect, paintInfo.paintBehavior);
    };
#endif

    auto paintContiguousCells = [&] {
        // Draw the dirty cells in the order that they appear.
        for (unsigned r = dirtiedRows.start; r < dirtiedRows.end; r++) {
            paintShadowForRow(r, paintInfo.phase);
            paintOutlineForRow(r, paintInfo.phase);
#if ENABLE(AX_CUSTOM_COLOR_MODE)
            recordRowBackdrop(r, paintInfo.phase);
#endif

            for (unsigned c = dirtiedColumns.start; c < dirtiedColumns.end; c++) {
                RenderTableSection::CellStruct& current = section.cellAt(r, c);
                CheckedPtr cell = current.primaryCell();
                if (!cell || (r > dirtiedRows.start && section.primaryCellAt(r - 1, c) == cell) || (c > dirtiedColumns.start && section.primaryCellAt(r, c - 1) == cell))
                    continue;
                paintCell(section, *cell, paintInfo, paintOffset);
            }
        }
    };

    auto paintContiguousCellsWithCollapsedBorders = [&] {
        // Collapsed borders are painted from the bottom right to the top left so that precedence
        // due to cell position is respected. We need to paint one row beyond the topmost dirtied
        // row to calculate its collapsed border value.
        unsigned startRow = dirtiedRows.start ? dirtiedRows.start - 1 : 0;
        for (unsigned r = dirtiedRows.end; r > startRow; r--) {
            unsigned row = r - 1;
            bool shouldPaintRowGroupBorder = false;
            for (unsigned c = dirtiedColumns.end; c > dirtiedColumns.start; c--) {
                unsigned col = c - 1;
                RenderTableSection::CellStruct& current = section.cellAt(row, col);
                CheckedPtr cell = current.primaryCell();
                if (!cell) {
                    if (!c)
                        paintRowGroupBorderIfRequired(section, paintInfo, paintOffset, row, col, physicalBorderForDirection(section.table()->writingMode(), CollapsedBorderSide::Start), *currentBorder);
                    else if (c == section.table()->numEffCols())
                        paintRowGroupBorderIfRequired(section, paintInfo, paintOffset, row, col, physicalBorderForDirection(section.table()->writingMode(), CollapsedBorderSide::End), *currentBorder);

                    shouldPaintRowGroupBorder = true;
                    continue;
                }

                if ((row > dirtiedRows.start && section.primaryCellAt(row - 1, col) == cell) || (col > dirtiedColumns.start && section.primaryCellAt(row, col - 1) == cell))
                    continue;

                // If we had a run of null cells paint their corresponding section of the row group's border if necessary. Note that
                // this will only happen once within a row as the null cells will always be clustered together on one end of the row.
                if (shouldPaintRowGroupBorder) {
                    if (r == section.m_grid.size())
                        paintRowGroupBorderIfRequired(section, paintInfo, paintOffset, row, col, physicalBorderForDirection(section.table()->writingMode(), CollapsedBorderSide::After), *currentBorder, cell);
                    else if (!row && !protect(section.table())->sectionAbove(&section))
                        paintRowGroupBorderIfRequired(section, paintInfo, paintOffset, row, col, physicalBorderForDirection(section.table()->writingMode(), CollapsedBorderSide::Before), *currentBorder, cell);

                    shouldPaintRowGroupBorder = false;
                }

                CheckedRef row = downcast<RenderTableRow>(*cell->parent());
                auto rowPaintOffset = section.blockStartRelativeToPhysicalForChild(row, paintOffset) + row->location();
                auto cellPoint = row->blockStartRelativeToPhysicalForChild(*cell, rowPaintOffset);
                paintCollapsedBorders(*cell, paintInfo, cellPoint, *currentBorder);
            }
        }
    };

    auto paintDirtyCells = [&] {
        // The overflowing cells should be scarce to avoid adding a lot of cells to the HashSet.
#if ASSERT_ENABLED
        unsigned totalRows = section.m_grid.size();
        unsigned totalCols = section.table()->columns().size();
        ASSERT(section.m_overflowingCells.computeSize() < totalRows * totalCols * RenderTableSection::maxAllowedOverflowingCellRatioForFastPaintPath);
#endif

        // To make sure we properly repaint the section, we repaint all the overflowing cells that we collected.
        auto cells = copyToVector(section.m_overflowingCells);

        HashSet<CheckedPtr<RenderTableCell>> spanningCells;

        for (unsigned r = dirtiedRows.start; r < dirtiedRows.end; r++) {
            paintOutlineForRow(r, paintInfo.phase);
#if ENABLE(AX_CUSTOM_COLOR_MODE)
            recordRowBackdrop(r, paintInfo.phase);
#endif

            for (unsigned c = dirtiedColumns.start; c < dirtiedColumns.end; c++) {
                RenderTableSection::CellStruct& current = section.cellAt(r, c);
                if (!current.hasCells())
                    continue;

                for (unsigned i = 0; i < current.cells.size(); ++i) {
                    CheckedPtr cell = current.cells[i];
                    if (section.m_overflowingCells.contains(*cell))
                        continue;

                    if (cell->rowSpan() > 1 || cell->colSpan() > 1) {
                        if (!spanningCells.add(cell).isNewEntry)
                            continue;
                    }

                    cells.append(cell.get());
                }
            }
        }

        // Sort the dirty cells by paint order.
        if (section.m_overflowingCells.computeIsEmpty())
            std::ranges::stable_sort(cells, compareCellPositions);
        else
            std::ranges::sort(cells, compareCellPositionsWithOverflowingCells);

        if (paintInfo.phase == PaintPhase::CollapsedTableBorders) {
            for (unsigned i = cells.size(); i > 0; --i) {
                CheckedRef cell = *cells[i - 1];
                CheckedRef row = downcast<RenderTableRow>(*cell->parent());
                auto rowPaintOffset = section.blockStartRelativeToPhysicalForChild(row, paintOffset) + row->location();
                auto cellPoint = row->blockStartRelativeToPhysicalForChild(cell, rowPaintOffset);
                paintCollapsedBorders(cell, paintInfo, cellPoint, *currentBorder);
            }
        } else {
            for (unsigned i = 0; i < cells.size(); ++i) {
                CheckedRef cell = *cells[i];
                paintCell(section, cell, paintInfo, paintOffset);
            }
        }
    };

    if (!section.m_hasMultipleCellLevels && section.m_overflowingCells.computeIsEmpty()) {
        if (paintInfo.phase == PaintPhase::CollapsedTableBorders)
            paintContiguousCellsWithCollapsedBorders();
        else
            paintContiguousCells();
    } else
        paintDirtyCells();
}

struct CollapsedBorder {
    CollapsedBorderValue borderValue;
    BoxSide side;
    bool shouldPaint;
    LayoutUnit x1;
    LayoutUnit y1;
    LayoutUnit x2;
    LayoutUnit y2;
    BorderStyle style;
};

class CollapsedBorders {
public:
    void addBorder(const CollapsedBorderValue& borderValue, BoxSide borderSide, bool shouldPaint,
        LayoutUnit x1, LayoutUnit y1, LayoutUnit x2, LayoutUnit y2, BorderStyle borderStyle)
    {
        if (borderValue.exists() && shouldPaint) {
            m_borders[m_count].borderValue = borderValue;
            m_borders[m_count].side = borderSide;
            m_borders[m_count].shouldPaint = shouldPaint;
            m_borders[m_count].x1 = x1;
            m_borders[m_count].x2 = x2;
            m_borders[m_count].y1 = y1;
            m_borders[m_count].y2 = y2;
            m_borders[m_count].style = borderStyle;
            m_count++;
        }
    }

    NODELETE CollapsedBorder* nextBorder()
    {
        for (unsigned i = 0; i < m_count; i++) {
            if (m_borders[i].borderValue.exists() && m_borders[i].shouldPaint) {
                m_borders[i].shouldPaint = false;
                return &m_borders[i];
            }
        }

        return nullptr;
    }

    std::array<CollapsedBorder, 4> m_borders;
    unsigned m_count { 0 };
};

NODELETE static LayoutRect backgroundRectForRow(const RenderBox& tableRow, const RenderTable& table)
{
    LayoutRect rect = { tableRow.location(), tableRow.borderBoxSize() };
    if (!table.collapseBorders()) {
        // Row border boxes include unwanted hSpacing on both inline ends.
        auto hSpacing = table.hBorderSpacing();
        LayoutUnit vSpacing = 0_lu;
        if (table.writingMode().isHorizontal())
            rect.contract({ vSpacing, hSpacing, vSpacing, hSpacing });
        else
            rect.contract({ hSpacing, vSpacing, hSpacing, vSpacing });
    }
    return rect;
}

NODELETE static LayoutRect backgroundRectForSection(const RenderTableSection& tableSection, const RenderTable& table)
{
    LayoutRect rect = { { }, tableSection.borderBoxSize() };
    if (!table.collapseBorders()) {
        auto hSpacing = table.hBorderSpacing();
        auto vSpacing = table.vBorderSpacing();
        // All sections' size()s include unwanted vSpacing at the block-end
        // position. The first section's size() includes additional unwanted
        // vSpacing at the block-start position. All sections' size()s include
        // unwanted hSpacing on both inline ends.
        auto beforeBlockSpacing = &tableSection == table.topSection() ? vSpacing : 0_lu;
        if (table.writingMode().isHorizontal())
            rect.contract({ beforeBlockSpacing, hSpacing, vSpacing, hSpacing });
        else if (table.writingMode().isBlockFlipped())
            rect.contract({ hSpacing, beforeBlockSpacing, hSpacing, vSpacing });
        else
            rect.contract({ hSpacing, vSpacing, hSpacing, beforeBlockSpacing });
    }
    return rect;
}

void TablePainter::paintRowOutline(RenderTableRow& row, PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    PaintPhase paintPhase = paintInfo.phase;
    if ((paintPhase == PaintPhase::Outline || paintPhase == PaintPhase::SelfOutline) && row.usedStyle().visibility() == UsedVisibility::Visible) {
        auto adjustedPaintOffset = paintOffset + row.location();
        row.paintOutline(paintInfo, LayoutRect(adjustedPaintOffset, row.borderBoxSize()));
    }
}

void TablePainter::paintRowShadow(RenderTableRow& row, PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (paintInfo.phase != PaintPhase::BlockBackground && paintInfo.phase != PaintPhase::ChildBlockBackground)
        return;

    auto adjustedPaintOffset = paintOffset + row.location();
    LayoutRect rect(adjustedPaintOffset, row.borderBoxSize());
    row.adjustBorderBoxRectForPainting(rect);

    BackgroundPainter backgroundPainter { row, paintInfo };
    backgroundPainter.paintBoxShadow(rect, row.usedStyle(), Style::ShadowStyle::Normal);
}

void TablePainter::paint(RenderTableRow& row, PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    ASSERT(row.hasSelfPaintingLayer());

    auto adjustedPaintOffset = paintOffset + row.location();

    if (paintInfo.phase == PaintPhase::Accessibility) {
        if (CheckedPtr context = paintInfo.accessibilityRegionContext())
            context->takeBounds(row, adjustedPaintOffset);
    }

    paintRowOutline(row, paintInfo, paintOffset);
    for (CheckedPtr cell = row.firstCell(); cell; cell = cell->nextCell()) {
        // Paint the row background behind the cell.
        if (paintInfo.phase == PaintPhase::BlockBackground || paintInfo.phase == PaintPhase::ChildBlockBackground)
            paintBackgroundsBehindCell(*cell, paintInfo, adjustedPaintOffset, row, paintOffset);
        if (!cell->hasSelfPaintingLayer())
            cell->paint(paintInfo, adjustedPaintOffset);
    }

    // When the row has a self-painting layer (position: relative), collapsed
    // borders must be painted here after cell backgrounds. Otherwise the cell
    // background (in this layer) covers the inner half of the border (painted
    // by the table's layer).
    if (CheckedPtr table = row.table(); table && table->collapseBorders() && paintInfo.phase == PaintPhase::ChildBlockBackgrounds)
        paintCollapsedBordersForRow(*table, paintInfo, row, adjustedPaintOffset);
}

void TablePainter::paintCollapsedBorders(RenderTableCell& cell, PaintInfo& paintInfo, const LayoutPoint& paintOffset, const CollapsedBorderValue& currentBorder)
{
    ASSERT(paintInfo.phase == PaintPhase::CollapsedTableBorders);

    if (!paintInfo.shouldPaintWithinRoot(cell) || cell.usedStyle().visibility() != UsedVisibility::Visible)
        return;

    LayoutRect localRepaintRect = paintInfo.rect;
    LayoutRect paintRect = LayoutRect(paintOffset + cell.location(), cell.borderBoxSize());
    if (paintRect.y() - protect(cell.table())->outerBorderTop() >= localRepaintRect.maxY())
        return;

    if (paintRect.maxY() + protect(cell.table())->outerBorderBottom() <= localRepaintRect.y())
        return;

    GraphicsContext& graphicsContext = paintInfo.context();
    if (graphicsContext.paintingDisabled())
        return;

    WritingMode writingMode = cell.tableWritingMode();
    CollapsedBorderValue leftVal = cell.cachedCollapsedLeftBorder(writingMode);
    CollapsedBorderValue rightVal = cell.cachedCollapsedRightBorder(writingMode);
    CollapsedBorderValue topVal = cell.cachedCollapsedTopBorder(writingMode);
    CollapsedBorderValue bottomVal = cell.cachedCollapsedBottomBorder(writingMode);

    // Adjust our x/y/width/height so that we paint the collapsed borders at the correct location.
    LayoutUnit topWidth = topVal.width();
    LayoutUnit bottomWidth = bottomVal.width();
    LayoutUnit leftWidth = leftVal.width();
    LayoutUnit rightWidth = rightVal.width();

    float deviceScaleFactor = cell.document().deviceScaleFactor();
    LayoutUnit leftHalfCollapsedBorder = CollapsedBorderValue::adjustedCollapsedBorderWidth(leftWidth, deviceScaleFactor, false);
    LayoutUnit topHalfCollapsedBorder = CollapsedBorderValue::adjustedCollapsedBorderWidth(topWidth, deviceScaleFactor, false);
    LayoutUnit rightHalfCollapsedBorder = CollapsedBorderValue::adjustedCollapsedBorderWidth(rightWidth, deviceScaleFactor, true);
    LayoutUnit bottomHalfCollapsedBorder = CollapsedBorderValue::adjustedCollapsedBorderWidth(bottomWidth, deviceScaleFactor, true);

    LayoutRect borderRect = LayoutRect(paintRect.x() - leftHalfCollapsedBorder,
        paintRect.y() - topHalfCollapsedBorder,
        paintRect.width() + leftHalfCollapsedBorder + rightHalfCollapsedBorder,
        paintRect.height() + topHalfCollapsedBorder + bottomHalfCollapsedBorder);

    BorderStyle topStyle = collapsedBorderStyle(topVal.style());
    BorderStyle bottomStyle = collapsedBorderStyle(bottomVal.style());
    BorderStyle leftStyle = collapsedBorderStyle(leftVal.style());
    BorderStyle rightStyle = collapsedBorderStyle(rightVal.style());

    bool renderTop = topStyle > BorderStyle::Hidden && !topVal.isTransparent() && floorToDevicePixel(topWidth, deviceScaleFactor);
    bool renderBottom = bottomStyle > BorderStyle::Hidden && !bottomVal.isTransparent() && floorToDevicePixel(bottomWidth, deviceScaleFactor);
    bool renderLeft = leftStyle > BorderStyle::Hidden && !leftVal.isTransparent() && floorToDevicePixel(leftWidth, deviceScaleFactor);
    bool renderRight = rightStyle > BorderStyle::Hidden && !rightVal.isTransparent() && floorToDevicePixel(rightWidth, deviceScaleFactor);

    // We never paint diagonals at the joins. We simply let the border with the highest
    // precedence paint on top of borders with lower precedence.
    CollapsedBorders borders;
    borders.addBorder(topVal, BoxSide::Top, renderTop, borderRect.x(), borderRect.y(), borderRect.maxX(), borderRect.y() + topWidth, topStyle);
    borders.addBorder(bottomVal, BoxSide::Bottom, renderBottom, borderRect.x(), borderRect.maxY() - bottomWidth, borderRect.maxX(), borderRect.maxY(), bottomStyle);
    borders.addBorder(leftVal, BoxSide::Left, renderLeft, borderRect.x(), borderRect.y(), borderRect.x() + leftWidth, borderRect.maxY(), leftStyle);
    borders.addBorder(rightVal, BoxSide::Right, renderRight, borderRect.maxX() - rightWidth, borderRect.y(), borderRect.maxX(), borderRect.maxY(), rightStyle);

    bool antialias = BorderPainter::shouldAntialiasLines(graphicsContext);

    for (CollapsedBorder* border = borders.nextBorder(); border; border = borders.nextBorder()) {
        if (!border->borderValue.isSameIgnoringColor(currentBorder))
            continue;
        BorderPainter::drawLineForBoxSide(graphicsContext, protect(cell.document()), LayoutRect(LayoutPoint(border->x1, border->y1), LayoutPoint(border->x2, border->y2)), border->side, border->borderValue.color(), border->style, 0, 0, antialias);
    }
}

void TablePainter::paintBackgroundsBehindCell(RenderTableCell& cell, PaintInfo& paintInfo, LayoutPoint paintOffset, RenderBox& backgroundObjectReference, LayoutPoint backgroundPaintOffset)
{
    CheckedPtr backgroundObject = &backgroundObjectReference;

    if (!paintInfo.shouldPaintWithinRoot(cell))
        return;

    if (cell.usedStyle().visibility() != UsedVisibility::Visible)
        return;

    if (cell.isHiddenEmptyCell())
        return;

    CheckedPtr tableElt = cell.table();
    CheckedRef style = backgroundObject->style();
    auto& bgLayers = style->backgroundLayers();

    auto color = style->visitedDependentUsedBackgroundColor();
    if (!Style::hasImageInAnyLayer(bgLayers) && !color.isVisible())
        return;

    Style::ColorResolver colorResolver { style.get() };
    color = colorResolver.colorApplyingColorFilter(color);

    LayoutPoint adjustedPaintOffset = paintOffset;
    if (backgroundObject != &cell)
        adjustedPaintOffset.moveBy(cell.location());

    // Background images attached to the row or row group must span the row
    // or row group. Draw them at the backgroundObject's dimensions, but
    // clipped to this cell.
    // FIXME: This should also apply to columns and column groups.
    bool paintBackgroundObject = backgroundObject != &cell && Style::hasImageInAnyLayer(bgLayers) && !is<RenderTableCol>(backgroundObject);
    // We have to clip here because the background would paint
    // on top of the borders otherwise. This only matters for cells and rows.
    bool shouldClip = paintBackgroundObject || (backgroundObject->hasLayer() && (backgroundObject == &cell || backgroundObject == cell.parent()) && tableElt->collapseBorders());
    GraphicsContextStateSaver stateSaver(paintInfo.context(), shouldClip);
    if (paintBackgroundObject)
        paintInfo.context().clip({ adjustedPaintOffset, cell.borderBoxSize() });
    else if (shouldClip) {
        LayoutRect clipRect(adjustedPaintOffset.x() + cell.borderLeft(), adjustedPaintOffset.y() + cell.borderTop(),
            cell.borderBoxWidth() - cell.borderLeft() - cell.borderRight(), cell.borderBoxHeight() - cell.borderTop() - cell.borderBottom());
        paintInfo.context().clip(clipRect);
    }
    LayoutRect fillRect;
    if (paintBackgroundObject) {
        if (auto* tableSectionRenderer = dynamicDowncast<RenderTableSection>(backgroundObject))
            fillRect = backgroundRectForSection(*tableSectionRenderer, *tableElt);
        else
            fillRect = backgroundRectForRow(*backgroundObject, *tableElt);
        fillRect.moveBy(backgroundPaintOffset);
    } else
        fillRect = LayoutRect { adjustedPaintOffset, cell.borderBoxSize() };

    // When painting a cell's own background with collapsed borders, inset the
    // fill rect at the table's outer edges to match this cell's collapsed
    // border extent. The cell's allocated width includes space for the maximum
    // collapsed border inner halves across all cells in the same column. If
    // this cell has narrower borders at the table edge, its background should
    // not paint into that extra space. Interior column edges are not adjusted
    // so that cell backgrounds remain continuous across the row.
    if (backgroundObject == &cell && tableElt->collapseBorders()) {
        bool isFirstColumn = !tableElt->colToEffCol(cell.col());
        bool isLastColumn = tableElt->colToEffCol(cell.col() + cell.colSpan() - 1) == tableElt->numEffCols() - 1;

        LayoutUnit leftInset = isFirstColumn ? tableElt->maxCollapsedInnerBorderHalfForFirstColumn() - cell.borderHalfLeft(false) : 0_lu;
        LayoutUnit rightInset = isLastColumn ? tableElt->maxCollapsedInnerBorderHalfForLastColumn() - cell.borderHalfRight(false) : 0_lu;

        if (leftInset || rightInset) {
            fillRect.shiftXEdgeBy(leftInset);
            fillRect.shiftMaxXEdgeBy(-rightInset);
        }
    }

    auto compositeOp = protect(cell.document())->compositeOperatorForBackgroundColor(color, cell);

    BackgroundPainter painter { cell, paintInfo };

    bool hasBackgroundClipText = false;
    for (auto& backgroundLayer : style->backgroundLayers().usedValues()) {
        if (backgroundLayer.clip() == FillBox::Text || backgroundLayer.clip() == FillBox::BorderAreaText)
            hasBackgroundClipText = true;
    }

    if (backgroundObject != &cell && !hasBackgroundClipText) {
        painter.setOverrideClip(FillBox::BorderBox);
        painter.setOverrideOrigin(FillBox::BorderBox);
    }
    painter.paintFillLayers(color, bgLayers, style->usedZoomForLength(), fillRect, BleedAvoidance::None, compositeOp, backgroundObject);
}

} // namespace Paint
} // namespace WebCore
