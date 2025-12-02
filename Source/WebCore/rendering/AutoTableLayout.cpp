/*
 * Copyright (C) 2002 Lars Knoll (knoll@kde.org)
 *           (C) 2002 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003-2025 Apple Inc. All rights reserved.
 * Copyright (C) 2014-2017 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License.
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
#include "AutoTableLayout.h"

#include "RenderBoxInlines.h"
#include "RenderChildIterator.h"
#include "RenderFlexibleBox.h"
#include "RenderGrid.h"
#include "RenderTableCellInlines.h"
#include "RenderTableCol.h"
#include "RenderTableInlines.h"
#include "RenderTableSection.h"
#include "RenderView.h"
#include "StylePreferredSize.h"

namespace WebCore {

AutoTableLayout::AutoTableLayout(RenderTable* table)
    : TableLayout(table)
    , m_hasPercent(false)
    , m_effectiveLogicalWidthDirty(true)
{
}

AutoTableLayout::~AutoTableLayout() = default;

void AutoTableLayout::recalcColumn(unsigned effCol)
{
    Layout& columnLayout = m_layoutStruct[effCol];

    RenderTableCell* fixedContributor = nullptr;
    RenderTableCell* maxContributor = nullptr;

    for (auto& child : childrenOfType<RenderObject>(*m_table)) {
        if (CheckedPtr column = dynamicDowncast<RenderTableCol>(child)) {
            // RenderTableCols don't have the concept of preferred logical width, but we need to clear their dirty bits
            // so that if we call setPreferredWidthsDirty(true) on a col or one of its descendants, we'll mark its
            // ancestors as dirty.
            column->clearNeedsPreferredLogicalWidthsUpdate();
        } else if (CheckedPtr section = dynamicDowncast<RenderTableSection>(child)) {
            unsigned numRows = section->numRows();
            for (unsigned i = 0; i < numRows; ++i) {
                auto current = section->cellAt(i, effCol);
                auto* cell = current.primaryCell();
                
                if (current.inColSpan || !cell)
                    continue;

                bool cellHasContent = cell->firstChild() || cell->style().hasBorder() || !Style::isKnownZero(cell->style().paddingBox()) || cell->style().hasBackground();
                if (cellHasContent)
                    columnLayout.emptyCellsOnly = false;

                // A cell originates in this column. Ensure we have
                // a min/max width of at least 1px for this column now.
                columnLayout.minLogicalWidth = std::max(columnLayout.minLogicalWidth, 0.f);
                columnLayout.maxLogicalWidth = std::max(columnLayout.maxLogicalWidth, 0.f);

                if (cell->isOrthogonal())
                    cell->clearIntrinsicPadding();

                if (cell->colSpan() == 1) {
                    columnLayout.minLogicalWidth = std::max(cell->minLogicalWidthForColumnSizing().ceilToFloat(), columnLayout.minLogicalWidth);
                    float maxPreferredWidth = cell->maxLogicalWidthForColumnSizing().ceilToFloat();
                    if (maxPreferredWidth > columnLayout.maxLogicalWidth) {
                        columnLayout.maxLogicalWidth = maxPreferredWidth;
                        maxContributor = cell;
                    }

                    // All browsers implement a size limit on the cell's max width. 
                    // Our limit is based on KHTML's representation that used 16 bits widths.
                    // FIXME: Other browsers have a lower limit for the cell's max width. 
                    const float cCellMaxWidth = 32760;
                    auto [ cellLogicalWidth, cellUsedZoom ] = cell->styleOrColLogicalWidth();
                    if (auto fixedCellLogicalWidth = cellLogicalWidth.tryFixed()) {
                        if (fixedCellLogicalWidth->resolveZoom(cellUsedZoom) > cCellMaxWidth)
                            cellLogicalWidth = Style::PreferredSize::Fixed { cCellMaxWidth };
                    }
                    WTF::switchOn(cellLogicalWidth,
                        [&](const Style::PreferredSize::Fixed& fixedCellLogicalWidth) {
                            // ignore width=0
                            if (fixedCellLogicalWidth.isPositive() && !columnLayout.logicalWidth.isPercentOrCalculated()) {
                                float logicalWidth = cell->adjustBorderBoxLogicalWidthForBoxSizing(fixedCellLogicalWidth);
                                if (auto fixedColumnLayoutLogicalWidth = columnLayout.logicalWidth.tryFixed()) {
                                    // Nav/IE weirdness
                                    if ((logicalWidth > fixedColumnLayoutLogicalWidth->resolveZoom(cellUsedZoom))
                                        || ((fixedColumnLayoutLogicalWidth->resolveZoom(cellUsedZoom) == logicalWidth) && (maxContributor == cell))) {
                                        columnLayout.logicalWidth = Style::PreferredSize::Fixed { logicalWidth };
                                        fixedContributor = cell;
                                    }
                                } else {
                                    columnLayout.logicalWidth = Style::PreferredSize::Fixed { logicalWidth };
                                    fixedContributor = cell;
                                }
                            }
                        },
                        [&](const Style::PreferredSize::Percentage& percentageCellLogicalWidth) {
                            m_hasPercent = true;
                            if (auto percentageColumnLayoutLogicalWidth = columnLayout.logicalWidth.tryPercentage(); percentageCellLogicalWidth.value > 0 && (!percentageColumnLayoutLogicalWidth || percentageCellLogicalWidth.value > percentageColumnLayoutLogicalWidth->value))
                                columnLayout.logicalWidth = cellLogicalWidth;
                        },
                        [&](const Style::PreferredSize::Calc&) {
                            columnLayout.logicalWidth = CSS::Keyword::Auto { };
                        },
                        [&](const auto&) { }
                    );
                } else if (!effCol || section->primaryCellAt(i, effCol - 1) != cell) {
                    // If a cell originates in this spanning column ensure we have a min/max width of at least 1px for it.
                    columnLayout.minLogicalWidth = std::max(columnLayout.minLogicalWidth, cell->maxLogicalWidthForColumnSizing() ? 1.f : 0.f);

                    // This spanning cell originates in this column. Insert the cell into spanning cells list.
                    insertSpanCell(cell);
                }
            }
        }
    }

    // Nav/IE weirdness
    if (auto fixedColumnLayoutLogicalWidth = columnLayout.logicalWidth.tryFixed()) {
        if (m_table->document().inQuirksMode() && columnLayout.maxLogicalWidth > fixedColumnLayoutLogicalWidth->resolveZoom(Style::ZoomFactor { columnLayout.usedZoom, m_table->style().deviceScaleFactor() })
        && fixedContributor != maxContributor) {
            columnLayout.logicalWidth = CSS::Keyword::Auto { };
            fixedContributor = nullptr;
        }
    }

    columnLayout.maxLogicalWidth = std::max(columnLayout.maxLogicalWidth, columnLayout.minLogicalWidth);
}

void AutoTableLayout::fullRecalc()
{
    m_hasPercent = false;
    m_effectiveLogicalWidthDirty = true;

    unsigned nEffCols = m_table->numEffCols();
    m_layoutStruct.resizeToFit(nEffCols);
    m_layoutStruct.fill(Layout());
    m_spanCells.fill(0);

    Style::PreferredSize groupLogicalWidth = CSS::Keyword::Auto { };
    unsigned currentColumn = 0;
    for (RenderTableCol* column = m_table->firstColumn(); column; column = column->nextColumn()) {
        if (column->isTableColumnGroupWithColumnChildren())
            groupLogicalWidth = column->style().logicalWidth();
        else {
            auto colLogicalWidth = column->style().logicalWidth();
            // FIXME: calc() on tables should be handled consistently with other lengths.
            if (colLogicalWidth.isCalculated() || colLogicalWidth.isAuto())
                colLogicalWidth = groupLogicalWidth;
            if (colLogicalWidth.isSpecified() && colLogicalWidth.isKnownZero())
                colLogicalWidth = CSS::Keyword::Auto { };
            unsigned effCol = m_table->colToEffCol(currentColumn);
            unsigned span = column->span();
            if (!colLogicalWidth.isAuto() && span == 1 && effCol < nEffCols && m_table->spanOfEffCol(effCol) == 1) {
                m_layoutStruct[effCol].usedZoom = column->style().usedZoom();
                m_layoutStruct[effCol].logicalWidth = colLogicalWidth;
                if (auto fixedColLogicalWidth = colLogicalWidth.tryFixed(); fixedColLogicalWidth && m_layoutStruct[effCol].maxLogicalWidth < fixedColLogicalWidth->resolveZoom(column->style().usedZoomForLength()))
                    m_layoutStruct[effCol].maxLogicalWidth = fixedColLogicalWidth->resolveZoom(column->style().usedZoomForLength());
            }
            currentColumn += span;
        }

        // For the last column in a column-group, we invalidate our group logical width.
        if (column->isTableColumn() && !column->nextSibling())
            groupLogicalWidth = CSS::Keyword::Auto { };
    }

    for (unsigned i = 0; i < nEffCols; i++)
        recalcColumn(i);

    for (auto& section : childrenOfType<RenderTableSection>(*m_table)) {
        section.clearNeedsPreferredWidthsUpdate();
        for (auto* row = section.firstRow(); row; row = row->nextRow())
            row->clearNeedsPreferredWidthsUpdate();
    }
}

static bool shouldScaleColumnsForParent(const RenderTable& table)
{
    RenderBlock* containingBlock = table.containingBlock();
    while (containingBlock && !is<RenderView>(containingBlock)) {
        // It doesn't matter if our table is auto or fixed: auto means we don't
        // scale. Fixed doesn't care if we do or not because it doesn't depend
        // on the cell contents' preferred widths.
        if (is<RenderTableCell>(containingBlock))
            return false;
        // The max logical width of a table may be "infinity" (or tableMaxWidth, to be more exact) if the sum if the
        // columns' percentages is 100% or more, AND there is at least one column that has a non-percentage-based positive
        // logical width. In such situations no table logical width will be large enough to satisfy the constraint
        // set by the contents. So the idea is to use ~infinity to make sure we use all available size in the containing
        // block. However, this just doesn't work if this is a flex or grid item, so disallow scaling in that case.
        if (is<RenderFlexibleBox>(containingBlock) || is<RenderGrid>(containingBlock))
            return false;
        containingBlock = containingBlock->containingBlock();
    }
    return true;
}

void AutoTableLayout::computeIntrinsicLogicalWidths(LayoutUnit& minWidth, LayoutUnit& maxWidth, TableIntrinsics intrinsics)
{
    fullRecalc();

    float spanMaxLogicalWidth = calcEffectiveLogicalWidth();
    minWidth = 0;
    maxWidth = 0;
    float maxPercent = 0;
    float maxNonPercent = 0;
    bool scaleColumnsForSelf = intrinsics == TableIntrinsics::ForLayout;

    float remainingPercent = 100;
    for (size_t i = 0; i < m_layoutStruct.size(); ++i) {
        minWidth += m_layoutStruct[i].effectiveMinLogicalWidth;
        maxWidth += m_layoutStruct[i].effectiveMaxLogicalWidth;
        if (scaleColumnsForSelf) {
            if (auto percentageEffectiveLogicalWidth = m_layoutStruct[i].effectiveLogicalWidth.tryPercentage()) {
                float percent = std::min(percentageEffectiveLogicalWidth->value, remainingPercent);
                // When percent columns meet or exceed 100% and there are remaining
                // columns, the other browsers (FF, Edge) use an artificially high max
                // width, so we do too. Instead of division by zero, logicalWidth and
                // maxNonPercent are set to tableMaxWidth.
                // Issue: https://github.com/w3c/csswg-drafts/issues/1501
                float logicalWidth = (percent > 0) ? m_layoutStruct[i].effectiveMaxLogicalWidth * 100 / percent : tableMaxWidth;
                maxPercent = std::max(logicalWidth,  maxPercent);
                remainingPercent -= percent;
            } else
                maxNonPercent += m_layoutStruct[i].effectiveMaxLogicalWidth;
        }
    }

    if (scaleColumnsForSelf) {
        if (maxNonPercent > 0)
            maxNonPercent = (remainingPercent > 0) ? maxNonPercent * 100 / remainingPercent : tableMaxWidth;
        m_scaledWidthFromPercentColumns = std::min(LayoutUnit(tableMaxWidth), LayoutUnit(std::max(maxPercent, maxNonPercent)));
        if (m_scaledWidthFromPercentColumns > maxWidth && shouldScaleColumnsForParent(*m_table))
            maxWidth = m_scaledWidthFromPercentColumns;
    }

    if (intrinsics == TableIntrinsics::ForKeyword && m_layoutStruct.isEmpty()) {
        ASSERT(!minWidth);
        ASSERT(!maxWidth);
        minWidth = m_table->bordersPaddingAndSpacingInRowDirection();
        maxWidth = minWidth;
    }

    maxWidth = std::max(maxWidth, LayoutUnit(spanMaxLogicalWidth));
}

void AutoTableLayout::applyPreferredLogicalWidthQuirks(LayoutUnit& minWidth, LayoutUnit& maxWidth) const
{
    if (auto fixedTableLogicalWidth = m_table->style().logicalWidth().tryFixed(); fixedTableLogicalWidth && fixedTableLogicalWidth->isPositive()) {
        LayoutUnit minContentWidth = minWidth;
        LayoutUnit tableFixedWidth = m_table->overridingBorderBoxLogicalWidth().value_or(LayoutUnit { fixedTableLogicalWidth->resolveZoom(m_table->style().usedZoomForLength()) });
        LayoutUnit clampedWidth = std::max(minContentWidth, std::max(minWidth, tableFixedWidth));
        minWidth = clampedWidth;
        maxWidth = clampedWidth;

        if (auto fixedMaxWidth = m_table->style().logicalMaxWidth().tryFixed()) {
            LayoutUnit maxAllowedWidth { fixedMaxWidth->resolveZoom(m_table->style().usedZoomForLength()) };
            minWidth = std::min(minWidth, maxAllowedWidth);
            minWidth = std::max(minWidth, minContentWidth);
            maxWidth = minWidth;
        }
    }
}

/*
  This method takes care of colspans.
  effWidth is the same as width for cells without colspans. If we have colspans, they get modified.
 */
float AutoTableLayout::calcEffectiveLogicalWidth()
{
    float maxLogicalWidth = 0;

    size_t nEffCols = m_layoutStruct.size();
    float spacingInRowDirection = m_table->hBorderSpacing();

    for (size_t i = 0; i < nEffCols; ++i) {
        m_layoutStruct[i].effectiveLogicalWidth = m_layoutStruct[i].logicalWidth;
        m_layoutStruct[i].effectiveMinLogicalWidth = m_layoutStruct[i].minLogicalWidth;
        m_layoutStruct[i].effectiveMaxLogicalWidth = m_layoutStruct[i].maxLogicalWidth;
    }

    for (size_t i = 0; i < m_spanCells.size(); ++i) {
        RenderTableCell* cell = m_spanCells[i];
        if (!cell)
            break;

        unsigned span = cell->colSpan();

        auto [ cellLogicalWidth, cellUsedZoom ] = cell->styleOrColLogicalWidth();
        if (cellLogicalWidth.isKnownZero())
            cellLogicalWidth = CSS::Keyword::Auto { };

        unsigned effCol = m_table->colToEffCol(cell->col());
        size_t lastCol = effCol;
        if (cell->isOrthogonal())
            cell->clearIntrinsicPadding();
        float cellMinLogicalWidth = cell->minLogicalWidthForColumnSizing() + spacingInRowDirection;
        float cellMaxLogicalWidth = cell->maxLogicalWidthForColumnSizing() + spacingInRowDirection;
        float totalPercent = 0;
        float spanMinLogicalWidth = 0;
        float spanMaxLogicalWidth = 0;
        bool allColsArePercent = true;
        bool allColsAreFixed = true;
        bool haveAuto = false;
        bool spanHasEmptyCellsOnly = true;
        float fixedWidth = 0;
        while (lastCol < nEffCols && span > 0) {
            auto& columnLayout = m_layoutStruct[lastCol];

            auto fallbackCase = [&] {
                // If the column is a percentage width, do not let the spanning cell overwrite the
                // width value.  This caused a mis-rendering on amazon.com.
                // Sample snippet:
                // <table border=2 width=100%><
                //   <tr><td>1</td><td colspan=2>2-3</tr>
                //   <tr><td>1</td><td colspan=2 width=100%>2-3</td></tr>
                // </table>
                if (auto percentageEffectiveLogicalWidth = columnLayout.effectiveLogicalWidth.tryPercentage())
                    totalPercent += percentageEffectiveLogicalWidth->value;
                else {
                    columnLayout.effectiveLogicalWidth = CSS::Keyword::Auto { };
                    allColsArePercent = false;
                }
                allColsAreFixed = false;
            };

            WTF::switchOn(columnLayout.logicalWidth,
                [&](const Style::PreferredSize::Percentage& percentage) {
                    totalPercent += percentage.value;
                    allColsAreFixed = false;
                },
                [&](const Style::PreferredSize::Fixed& fixed) {
                    if (fixed.isPositive()) {
                        fixedWidth += fixed.resolveZoom(Style::ZoomFactor { columnLayout.usedZoom, m_table->style().deviceScaleFactor() });
                        allColsArePercent = false;
                        // IE resets effWidth to Auto here, but this breaks the konqueror about page and seems to be some bad
                        // legacy behavior anyway. mozilla doesn't do this so I decided we don't neither.
                        return;
                    }
                    haveAuto = true;
                    fallbackCase();
                },
                [&](const CSS::Keyword::Auto&) {
                    haveAuto = true;
                    fallbackCase();
                },
                [&](const auto&) {
                    fallbackCase();
                }
            );
            if (!columnLayout.emptyCellsOnly)
                spanHasEmptyCellsOnly = false;
            span -= m_table->spanOfEffCol(lastCol);
            spanMinLogicalWidth += columnLayout.effectiveMinLogicalWidth;
            spanMaxLogicalWidth += columnLayout.effectiveMaxLogicalWidth;
            lastCol++;
            cellMinLogicalWidth -= spacingInRowDirection;
            cellMaxLogicalWidth -= spacingInRowDirection;
        }

        // adjust table max width if needed
        if (auto percentageCellLogicalWidth = cellLogicalWidth.tryPercentage()) {
            if (totalPercent > percentageCellLogicalWidth->value || allColsArePercent) {
                // can't satisfy this condition, treat as variable
                cellLogicalWidth = CSS::Keyword::Auto { };
            } else {
                maxLogicalWidth = std::max(maxLogicalWidth, std::max(spanMaxLogicalWidth, cellMaxLogicalWidth) * 100  / percentageCellLogicalWidth->value);

                // all non percent columns in the span get percent values to sum up correctly.
                float percentMissing = percentageCellLogicalWidth->value - totalPercent;
                float totalWidth = 0;
                for (unsigned pos = effCol; pos < lastCol; ++pos) {
                    if (!m_layoutStruct[pos].effectiveLogicalWidth.isPercentOrCalculated())
                        totalWidth += m_layoutStruct[pos].effectiveMaxLogicalWidth;
                }

                for (unsigned pos = effCol; pos < lastCol; ++pos) {
                    if (!m_layoutStruct[pos].effectiveLogicalWidth.isPercentOrCalculated()) {
                        // Handle the case when there's only one cell with 'width: percent' and it's empty.
                        auto percent = percentMissing * (totalWidth ? m_layoutStruct[pos].effectiveMaxLogicalWidth / totalWidth : 1);
                        totalWidth -= m_layoutStruct[pos].effectiveMaxLogicalWidth;
                        percentMissing -= percent;
                        if (percent > 0)
                            m_layoutStruct[pos].effectiveLogicalWidth = Style::PreferredSize::Percentage { percent };
                        else
                            m_layoutStruct[pos].effectiveLogicalWidth = CSS::Keyword::Auto { };
                    }
                    if (totalWidth <= 0)
                        break;
                }
            }
        }

        // make sure minWidth and maxWidth of the spanning cell are honored
        if (cellMinLogicalWidth > spanMinLogicalWidth) {
            if (allColsAreFixed) {
                for (unsigned pos = effCol; fixedWidth > 0 && pos < lastCol; ++pos) {
                    // NOTE: The unchecked use of tryFixed() here is allowed because `allColsAreFixed` is true.
                    // FIXME: Find a more type safe way to enforce this invariant.
                    auto fixedLogicalWidth = m_layoutStruct[pos].logicalWidth.tryFixed()->resolveZoom(Style::ZoomFactor { m_layoutStruct[effCol].usedZoom, m_table->style().deviceScaleFactor() });

                    float cellLogicalWidth = std::max(m_layoutStruct[pos].effectiveMinLogicalWidth, cellMinLogicalWidth * fixedLogicalWidth / fixedWidth);
                    fixedWidth -= fixedLogicalWidth;
                    cellMinLogicalWidth -= cellLogicalWidth;
                    m_layoutStruct[pos].effectiveMinLogicalWidth = cellLogicalWidth;
                }
            } else if (allColsArePercent) {
                // In this case, we just split the colspan's min and max widths following the percentage.
#if ASSERT_ENABLED
                float allocatedMinLogicalWidth = 0;
#endif
                float allocatedMaxLogicalWidth = 0;
                for (unsigned pos = effCol; pos < lastCol; ++pos) {
                    ASSERT(m_layoutStruct[pos].logicalWidth.isPercent() || m_layoutStruct[pos].effectiveLogicalWidth.isPercent());
                    // |allColsArePercent| means that either the logicalWidth *or* the effectiveLogicalWidth are percents, handle both of them here.
                    auto percentageLogicalWidth = m_layoutStruct[pos].logicalWidth.tryPercentage();
                    auto percentageEffectiveLogicalWidth = m_layoutStruct[pos].effectiveLogicalWidth.tryPercentage();
                    ASSERT(percentageLogicalWidth || percentageEffectiveLogicalWidth);
                    float percent = percentageLogicalWidth ? percentageLogicalWidth->value : percentageEffectiveLogicalWidth->value;

                    float columnMinLogicalWidth = percent * cellMinLogicalWidth / totalPercent;
                    float columnMaxLogicalWidth = percent * cellMaxLogicalWidth / totalPercent;
                    m_layoutStruct[pos].effectiveMinLogicalWidth = std::max(m_layoutStruct[pos].effectiveMinLogicalWidth, columnMinLogicalWidth);
                    m_layoutStruct[pos].effectiveMaxLogicalWidth = columnMaxLogicalWidth;
#if ASSERT_ENABLED
                    allocatedMinLogicalWidth += columnMinLogicalWidth;
#endif
                    allocatedMaxLogicalWidth += columnMaxLogicalWidth;
                }
                ASSERT(allocatedMinLogicalWidth < cellMinLogicalWidth || WTF::areEssentiallyEqual(allocatedMinLogicalWidth, cellMinLogicalWidth));
                ASSERT(allocatedMaxLogicalWidth < cellMaxLogicalWidth || WTF::areEssentiallyEqual(allocatedMaxLogicalWidth, cellMaxLogicalWidth));
                cellMaxLogicalWidth -= allocatedMaxLogicalWidth;
            } else {
                float remainingMaxLogicalWidth = spanMaxLogicalWidth;
                float remainingMinLogicalWidth = spanMinLogicalWidth;
                
                // Give min to variable first, to fixed second, and to others third.
                for (unsigned pos = effCol; remainingMaxLogicalWidth >= 0 && pos < lastCol; ++pos) {
                    if (auto fixedLogicalWidth = m_layoutStruct[pos].logicalWidth.tryFixed(); fixedLogicalWidth && haveAuto && fixedWidth <= cellMinLogicalWidth) {
                        auto deviceScaleFactor = m_table->style().deviceScaleFactor();
                        float colMinLogicalWidth = std::max(m_layoutStruct[pos].effectiveMinLogicalWidth, fixedLogicalWidth->resolveZoom(Style::ZoomFactor { m_layoutStruct[pos].usedZoom, deviceScaleFactor }));
                        fixedWidth -= fixedLogicalWidth->resolveZoom(Style::ZoomFactor { m_layoutStruct[pos].usedZoom, deviceScaleFactor });
                        remainingMinLogicalWidth -= m_layoutStruct[pos].effectiveMinLogicalWidth;
                        remainingMaxLogicalWidth -= m_layoutStruct[pos].effectiveMaxLogicalWidth;
                        cellMinLogicalWidth -= colMinLogicalWidth;
                        m_layoutStruct[pos].effectiveMinLogicalWidth = colMinLogicalWidth;
                    }
                }

                for (unsigned pos = effCol; remainingMaxLogicalWidth >= 0 && pos < lastCol && remainingMinLogicalWidth < cellMinLogicalWidth; ++pos) {
                    if (!(m_layoutStruct[pos].logicalWidth.isFixed() && haveAuto && fixedWidth <= cellMinLogicalWidth)) {
                        float colMinLogicalWidth = std::max(m_layoutStruct[pos].effectiveMinLogicalWidth, remainingMaxLogicalWidth ? cellMinLogicalWidth * m_layoutStruct[pos].effectiveMaxLogicalWidth / remainingMaxLogicalWidth : cellMinLogicalWidth);
                        colMinLogicalWidth = std::min(m_layoutStruct[pos].effectiveMinLogicalWidth + (cellMinLogicalWidth - remainingMinLogicalWidth), colMinLogicalWidth);
                        remainingMaxLogicalWidth -= m_layoutStruct[pos].effectiveMaxLogicalWidth;
                        remainingMinLogicalWidth -= m_layoutStruct[pos].effectiveMinLogicalWidth;
                        cellMinLogicalWidth -= colMinLogicalWidth;
                        m_layoutStruct[pos].effectiveMinLogicalWidth = colMinLogicalWidth;
                    }
                }
            }
        }
        if (!cellLogicalWidth.isPercentOrCalculated()) {
            if (cellMaxLogicalWidth > spanMaxLogicalWidth) {
                for (unsigned pos = effCol; spanMaxLogicalWidth >= 0 && pos < lastCol; ++pos) {
                    float colMaxLogicalWidth = std::max(m_layoutStruct[pos].effectiveMaxLogicalWidth, spanMaxLogicalWidth ? cellMaxLogicalWidth * m_layoutStruct[pos].effectiveMaxLogicalWidth / spanMaxLogicalWidth : cellMaxLogicalWidth);
                    spanMaxLogicalWidth -= m_layoutStruct[pos].effectiveMaxLogicalWidth;
                    cellMaxLogicalWidth -= colMaxLogicalWidth;
                    m_layoutStruct[pos].effectiveMaxLogicalWidth = colMaxLogicalWidth;
                }
            }
        } else {
            for (unsigned pos = effCol; pos < lastCol; ++pos)
                m_layoutStruct[pos].maxLogicalWidth = std::max(m_layoutStruct[pos].maxLogicalWidth, m_layoutStruct[pos].minLogicalWidth);
        }
        // treat span ranges consisting of empty cells only as if they had content
        if (spanHasEmptyCellsOnly) {
            for (unsigned pos = effCol; pos < lastCol; ++pos)
                m_layoutStruct[pos].emptyCellsOnly = false;
        }
    }
    m_effectiveLogicalWidthDirty = false;

    return std::min<float>(maxLogicalWidth, tableMaxWidth);
}

/* gets all cells that originate in a column and have a cellspan > 1
   Sorts them by increasing cellspan
*/
void AutoTableLayout::insertSpanCell(RenderTableCell* cell)
{
    ASSERT_ARG(cell, cell && cell->colSpan() != 1);
    if (!cell || cell->colSpan() == 1)
        return;

    unsigned size = m_spanCells.size();
    if (!size || m_spanCells[size-1] != 0) {
        m_spanCells.grow(size + 10);
        for (unsigned i = 0; i < 10; i++)
            m_spanCells[size + i] = 0;
        size += 10;
    }

    // add them in sort. This is a slow algorithm, and a binary search or a fast sorting after collection would be better
    unsigned pos = 0;
    unsigned span = cell->colSpan();
    while (pos < m_spanCells.size() && m_spanCells[pos] && span > m_spanCells[pos]->colSpan())
        ++pos;
    memmoveSpan(m_spanCells.mutableSpan().subspan(pos + 1), m_spanCells.subspan(pos, size - (pos + 1)));
    m_spanCells[pos] = cell;
}

void AutoTableLayout::layout()
{
    // table layout based on the values collected in the layout structure.
    float tableLogicalWidth = m_table->logicalWidth() - m_table->bordersPaddingAndSpacingInRowDirection();
    float available = tableLogicalWidth;
    size_t nEffCols = m_table->numEffCols();

    // FIXME: It is possible to be called without having properly updated our internal representation.
    // This means that our preferred logical widths were not recomputed as expected.
    if (nEffCols != m_layoutStruct.size()) {
        fullRecalc();
        // FIXME: Table layout shouldn't modify our table structure (but does due to columns and column-groups).
        nEffCols = m_table->numEffCols();
    }

    if (m_effectiveLogicalWidthDirty)
        calcEffectiveLogicalWidth();

    bool havePercent = false;
    int numFixed = 0;
    size_t numberOfNonEmptyAuto = 0;
    std::optional<float> totalAuto;
    float totalFixed = 0;
    float totalPercent = 0;
    float allocAuto = 0;
    unsigned numAutoEmptyCellsOnly = 0;

    // fill up every cell with its minWidth
    for (size_t i = 0; i < nEffCols; ++i) {
        float cellLogicalWidth = m_layoutStruct[i].effectiveMinLogicalWidth;
        m_layoutStruct[i].computedLogicalWidth = cellLogicalWidth;
        available -= cellLogicalWidth;
        WTF::switchOn(m_layoutStruct[i].effectiveLogicalWidth,
            [&](const Style::PreferredSize::Fixed&) {
                numFixed++;
                totalFixed += m_layoutStruct[i].effectiveMaxLogicalWidth;
            },
            [&](const Style::PreferredSize::Percentage& percentageLogicalWidth) {
                havePercent = true;
                totalPercent += percentageLogicalWidth.value;
            },
            [&](const CSS::Keyword::Auto&) {
                if (m_layoutStruct[i].emptyCellsOnly)
                    numAutoEmptyCellsOnly++;
                else {
                    ++numberOfNonEmptyAuto;
                    totalAuto = totalAuto.value_or(0.f) + m_layoutStruct[i].effectiveMaxLogicalWidth;
                    allocAuto += cellLogicalWidth;
                }
            },
            [&](const auto&) { }
        );
    }

    // allocate width to percent cols
    if (available > 0 && havePercent) {
        for (size_t i = 0; i < nEffCols; ++i) {
            auto& logicalWidth = m_layoutStruct[i].effectiveLogicalWidth;
            if (logicalWidth.isPercentOrCalculated()) {
                float cellLogicalWidth = std::max<float>(m_layoutStruct[i].effectiveMinLogicalWidth, Style::evaluateMinimum<float>(logicalWidth, tableLogicalWidth, Style::ZoomFactor { m_layoutStruct[i].usedZoom, m_table->style().deviceScaleFactor() }));
                available += m_layoutStruct[i].computedLogicalWidth - cellLogicalWidth;
                m_layoutStruct[i].computedLogicalWidth = cellLogicalWidth;
            }
        }
        if (totalPercent > 100) {
            // remove overallocated space from the last columns
            float excess = tableLogicalWidth * (totalPercent - 100) / 100;
            for (unsigned i = nEffCols; i; ) {
                --i;
                if (m_layoutStruct[i].effectiveLogicalWidth.isPercentOrCalculated()) {
                    float cellLogicalWidth = m_layoutStruct[i].computedLogicalWidth;
                    float reduction = std::min(cellLogicalWidth,  excess);
                    // the lines below might look inconsistent, but that's the way it's handled in mozilla
                    excess -= reduction;
                    float newLogicalWidth = std::max(m_layoutStruct[i].effectiveMinLogicalWidth, cellLogicalWidth - reduction);
                    available += cellLogicalWidth - newLogicalWidth;
                    m_layoutStruct[i].computedLogicalWidth = newLogicalWidth;
                }
            }
        }
    }
    
    // then allocate width to fixed cols
    if (available > 0) {
        for (size_t i = 0; i < nEffCols; ++i) {
            auto& logicalWidth = m_layoutStruct[i].effectiveLogicalWidth;
            auto usedZoom = m_layoutStruct[i].usedZoom;
            auto deviceScaleFactor = m_table->style().deviceScaleFactor();
            if (auto fixedLogicalWidth = logicalWidth.tryFixed(); fixedLogicalWidth && fixedLogicalWidth->resolveZoom(Style::ZoomFactor { usedZoom, deviceScaleFactor }) > m_layoutStruct[i].computedLogicalWidth) {
                available += m_layoutStruct[i].computedLogicalWidth - fixedLogicalWidth->resolveZoom(Style::ZoomFactor { usedZoom, deviceScaleFactor });
                m_layoutStruct[i].computedLogicalWidth = fixedLogicalWidth->resolveZoom(Style::ZoomFactor { usedZoom, deviceScaleFactor });
            }
        }
    }

    // now satisfy variable
    if (available > 0 && numberOfNonEmptyAuto) {
        ASSERT(totalAuto);
        available += allocAuto; // this gets redistributed.
        auto equalWidthForZeroLengthColumns = std::optional<float> { };
        if (!*totalAuto) {
            // All columns in this table are (non-empty)zero length with 'width: auto'.
            equalWidthForZeroLengthColumns = available / numberOfNonEmptyAuto;
        }
        for (size_t i = 0; i < nEffCols; ++i) {
            auto& column = m_layoutStruct[i];
            if (!column.effectiveLogicalWidth.isAuto() || column.emptyCellsOnly)
                continue;
            auto columnWidthCandidate = equalWidthForZeroLengthColumns ? *equalWidthForZeroLengthColumns : available * column.effectiveMaxLogicalWidth / *totalAuto;
            column.computedLogicalWidth = std::max(column.computedLogicalWidth, columnWidthCandidate);
            available -= column.computedLogicalWidth;
            if (!equalWidthForZeroLengthColumns) {
                *totalAuto -= column.effectiveMaxLogicalWidth;
                if (*totalAuto <= 0)
                    break;
            }
        }
    }

    // spread over fixed columns
    if (available > 0 && numFixed) {
        for (size_t i = 0; i < nEffCols; ++i) {
            auto& logicalWidth = m_layoutStruct[i].effectiveLogicalWidth;
            if (logicalWidth.isFixed()) {
                float cellLogicalWidth = available * m_layoutStruct[i].effectiveMaxLogicalWidth / totalFixed;
                available -= cellLogicalWidth;
                totalFixed -= m_layoutStruct[i].effectiveMaxLogicalWidth;
                m_layoutStruct[i].computedLogicalWidth += cellLogicalWidth;
            }
        }
    }

    // spread over percent columns
    if (available > 0 && m_hasPercent && totalPercent < 100) {
        for (size_t i = 0; i < nEffCols; ++i) {
            auto& logicalWidth = m_layoutStruct[i].effectiveLogicalWidth;
            if (auto percentageLogicalWidth = logicalWidth.tryPercentage()) {
                float cellLogicalWidth = available * percentageLogicalWidth->value / totalPercent;
                available -= cellLogicalWidth;
                totalPercent -= percentageLogicalWidth->value;
                m_layoutStruct[i].computedLogicalWidth += cellLogicalWidth;
                if (!available || !totalPercent)
                    break;
            }
        }
    }

    // spread over the rest
    if (available > 0 && nEffCols > numAutoEmptyCellsOnly) {
        unsigned total = nEffCols - numAutoEmptyCellsOnly;
        // still have some width to spread
        for (unsigned i = nEffCols; i; ) {
            --i;
            // variable columns with empty cells only don't get any width
            if (m_layoutStruct[i].effectiveLogicalWidth.isAuto() && m_layoutStruct[i].emptyCellsOnly)
                continue;
            float cellLogicalWidth = available / total;
            available -= cellLogicalWidth;
            total--;
            m_layoutStruct[i].computedLogicalWidth += cellLogicalWidth;
        }
    }

    if (available > 0 && numAutoEmptyCellsOnly && nEffCols == numAutoEmptyCellsOnly) {
        // All columns in this table are empty with 'width: auto'.
        auto equalWidthForColumns = available / numAutoEmptyCellsOnly;
        for (size_t i = 0; i < nEffCols; ++i) {
            auto& column = m_layoutStruct[i];
            column.computedLogicalWidth = equalWidthForColumns;
            available -= column.computedLogicalWidth;
        }
    }

    // If we have over-allocated, reduce every cell according to the difference between desired width and min-width
    // this seems to produce to the pixel exact results with IE. Wonder if some of this also holds for width distributing.
    // Need to reduce cells with the following prioritization:
    // This is basically the reverse of how we grew the cells.
    if (available < 0)
        available = shrinkCellWidthForType<CSS::Keyword::Auto>(available);
    if (available < 0)
        available = shrinkCellWidthForType<Style::PreferredSize::Fixed>(available);
    if (available < 0)
        available = shrinkCellWidthForType<Style::PreferredSize::Percentage>(available);

    LayoutUnit pos;
    for (size_t i = 0; i < nEffCols; ++i) {
        m_table->setColumnPosition(i, pos);
        pos += LayoutUnit::fromFloatCeil(m_layoutStruct[i].computedLogicalWidth) + m_table->hBorderSpacing();
    }
    m_table->setColumnPosition(m_table->columnPositions().size() - 1, pos);
}

template<typename T> float AutoTableLayout::shrinkCellWidthForType(float available)
{
    unsigned nEffCols = m_table->numEffCols();
    float logicalWidthBeyondMin = 0;
    for (unsigned i = nEffCols; i; ) {
        --i;
        auto& logicalWidth = m_layoutStruct[i].effectiveLogicalWidth;
        if (WTF::holdsAlternative<T>(logicalWidth))
            logicalWidthBeyondMin += m_layoutStruct[i].computedLogicalWidth - m_layoutStruct[i].effectiveMinLogicalWidth;
    }

    for (unsigned i = nEffCols; i && logicalWidthBeyondMin > 0; ) {
        --i;
        auto& logicalWidth = m_layoutStruct[i].effectiveLogicalWidth;
        if (WTF::holdsAlternative<T>(logicalWidth)) {
            float minMaxDiff = m_layoutStruct[i].computedLogicalWidth - m_layoutStruct[i].effectiveMinLogicalWidth;
            float reduce = available * minMaxDiff / logicalWidthBeyondMin;
            m_layoutStruct[i].computedLogicalWidth += reduce;
            available -= reduce;
            logicalWidthBeyondMin -= minMaxDiff;
            if (available >= 0)
                break;
        }
    }

    return available;
}

}
