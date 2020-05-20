/*
 * Copyright (C) 1997 Martin Jones (mjones@kde.org)
 *           (C) 1997 Torben Weis (weis@kde.org)
 *           (C) 1998 Waldo Bastian (bastian@kde.org)
 *           (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
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
#include "RenderTableCell.h"

#include "CollapsedBorderValue.h"
#include "FloatQuad.h"
#include "GraphicsContext.h"
#include "HTMLNames.h"
#include "HTMLTableCellElement.h"
#include "PaintInfo.h"
#include "RenderTableCol.h"
#include "RenderTheme.h"
#include "RenderView.h"
#include "Settings.h"
#include "StyleProperties.h"
#include "TransformState.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/StackStats.h>

#if ENABLE(MATHML)
#include "MathMLElement.h"
#include "MathMLNames.h"
#endif

namespace WebCore {

using namespace HTMLNames;

WTF_MAKE_ISO_ALLOCATED_IMPL(RenderTableCell);

struct SameSizeAsRenderTableCell : public RenderBlockFlow {
    unsigned bitfields;
    LayoutUnit paddings[2];
};

COMPILE_ASSERT(sizeof(RenderTableCell) == sizeof(SameSizeAsRenderTableCell), RenderTableCell_should_stay_small);
COMPILE_ASSERT(sizeof(CollapsedBorderValue) <= 24, CollapsedBorderValue_should_stay_small);

RenderTableCell::RenderTableCell(Element& element, RenderStyle&& style)
    : RenderBlockFlow(element, WTFMove(style))
    , m_column(unsetColumnIndex)
    , m_cellWidthChanged(false)
    , m_hasColSpan(false)
    , m_hasRowSpan(false)
    , m_hasEmptyCollapsedBeforeBorder(false)
    , m_hasEmptyCollapsedAfterBorder(false)
    , m_hasEmptyCollapsedStartBorder(false)
    , m_hasEmptyCollapsedEndBorder(false)
{
    // We only update the flags when notified of DOM changes in colSpanOrRowSpanChanged()
    // so we need to set their initial values here in case something asks for colSpan()/rowSpan() before then.
    updateColAndRowSpanFlags();
}

RenderTableCell::RenderTableCell(Document& document, RenderStyle&& style)
    : RenderBlockFlow(document, WTFMove(style))
    , m_column(unsetColumnIndex)
    , m_cellWidthChanged(false)
    , m_hasColSpan(false)
    , m_hasRowSpan(false)
    , m_hasEmptyCollapsedBeforeBorder(false)
    , m_hasEmptyCollapsedAfterBorder(false)
    , m_hasEmptyCollapsedStartBorder(false)
    , m_hasEmptyCollapsedEndBorder(false)
{
}

void RenderTableCell::willBeRemovedFromTree()
{
    RenderBlockFlow::willBeRemovedFromTree();
    if (!table() || !section())
        return;
    RenderTableSection* section = this->section();
    table()->invalidateCollapsedBorders();
    section->setNeedsCellRecalc();
}

unsigned RenderTableCell::parseColSpanFromDOM() const
{
    ASSERT(element());
    if (is<HTMLTableCellElement>(*element()))
        return std::min<unsigned>(downcast<HTMLTableCellElement>(*element()).colSpan(), maxColumnIndex);
#if ENABLE(MATHML)
    if (element()->hasTagName(MathMLNames::mtdTag))
        return std::min<unsigned>(downcast<MathMLElement>(*element()).colSpan(), maxColumnIndex);
#endif
    return 1;
}

unsigned RenderTableCell::parseRowSpanFromDOM() const
{
    ASSERT(element());
    if (is<HTMLTableCellElement>(*element()))
        return std::min<unsigned>(downcast<HTMLTableCellElement>(*element()).rowSpan(), maxRowIndex);
#if ENABLE(MATHML)
    if (element()->hasTagName(MathMLNames::mtdTag))
        return std::min<unsigned>(downcast<MathMLElement>(*element()).rowSpan(), maxRowIndex);
#endif
    return 1;
}

void RenderTableCell::updateColAndRowSpanFlags()
{
    // The vast majority of table cells do not have a colspan or rowspan,
    // so we keep a bool to know if we need to bother reading from the DOM.
    m_hasColSpan = element() && parseColSpanFromDOM() != 1;
    m_hasRowSpan = element() && parseRowSpanFromDOM() != 1;
}

void RenderTableCell::colSpanOrRowSpanChanged()
{
    ASSERT(element());
#if ENABLE(MATHML)
    ASSERT(element()->hasTagName(tdTag) || element()->hasTagName(thTag) || element()->hasTagName(MathMLNames::mtdTag));
#else
    ASSERT(element()->hasTagName(tdTag) || element()->hasTagName(thTag));
#endif

    updateColAndRowSpanFlags();

    // FIXME: I suspect that we could return early here if !m_hasColSpan && !m_hasRowSpan.

    setNeedsLayoutAndPrefWidthsRecalc();
    if (parent() && section())
        section()->setNeedsCellRecalc();
}

Length RenderTableCell::logicalWidthFromColumns(RenderTableCol* firstColForThisCell, Length widthFromStyle) const
{
    ASSERT(firstColForThisCell && firstColForThisCell == table()->colElement(col()));
    RenderTableCol* tableCol = firstColForThisCell;

    unsigned colSpanCount = colSpan();
    LayoutUnit colWidthSum;
    for (unsigned i = 1; i <= colSpanCount; i++) {
        Length colWidth = tableCol->style().logicalWidth();

        // Percentage value should be returned only for colSpan == 1.
        // Otherwise we return original width for the cell.
        if (!colWidth.isFixed()) {
            if (colSpanCount > 1)
                return widthFromStyle;
            return colWidth;
        }

        colWidthSum += colWidth.value();
        tableCol = tableCol->nextColumn();
        // If no next <col> tag found for the span we just return what we have for now.
        if (!tableCol)
            break;
    }

    // Column widths specified on <col> apply to the border box of the cell, see bug 8126.
    // FIXME: Why is border/padding ignored in the negative width case?
    if (colWidthSum > 0)
        return Length(std::max<LayoutUnit>(0, colWidthSum - borderAndPaddingLogicalWidth()), Fixed);
    return Length(colWidthSum, Fixed);
}

void RenderTableCell::computePreferredLogicalWidths()
{
    // The child cells rely on the grids up in the sections to do their computePreferredLogicalWidths work.  Normally the sections are set up early, as table
    // cells are added, but relayout can cause the cells to be freed, leaving stale pointers in the sections'
    // grids.  We must refresh those grids before the child cells try to use them.
    table()->recalcSectionsIfNeeded();

    RenderBlockFlow::computePreferredLogicalWidths();
    if (!element() || !style().autoWrap() || !element()->hasAttributeWithoutSynchronization(nowrapAttr))
        return;

    Length w = styleOrColLogicalWidth();
    if (w.isFixed()) {
        // Nowrap is set, but we didn't actually use it because of the
        // fixed width set on the cell. Even so, it is a WinIE/Moz trait
        // to make the minwidth of the cell into the fixed width. They do this
        // even in strict mode, so do not make this a quirk. Affected the top
        // of hiptop.com.
        m_minPreferredLogicalWidth = std::max(LayoutUnit(w.value()), m_minPreferredLogicalWidth);
    }
}

void RenderTableCell::computeIntrinsicPadding(LayoutUnit rowHeight)
{
    LayoutUnit oldIntrinsicPaddingBefore = intrinsicPaddingBefore();
    LayoutUnit oldIntrinsicPaddingAfter = intrinsicPaddingAfter();
    LayoutUnit logicalHeightWithoutIntrinsicPadding = logicalHeight() - oldIntrinsicPaddingBefore - oldIntrinsicPaddingAfter;

    LayoutUnit intrinsicPaddingBefore;
    switch (style().verticalAlign()) {
    case VerticalAlign::Sub:
    case VerticalAlign::Super:
    case VerticalAlign::TextTop:
    case VerticalAlign::TextBottom:
    case VerticalAlign::Length:
    case VerticalAlign::Baseline: {
        LayoutUnit baseline = cellBaselinePosition();
        if (baseline > borderAndPaddingBefore())
            intrinsicPaddingBefore = section()->rowBaseline(rowIndex()) - (baseline - oldIntrinsicPaddingBefore);
        break;
    }
    case VerticalAlign::Top:
        break;
    case VerticalAlign::Middle:
        intrinsicPaddingBefore = (rowHeight - logicalHeightWithoutIntrinsicPadding) / 2;
        break;
    case VerticalAlign::Bottom:
        intrinsicPaddingBefore = rowHeight - logicalHeightWithoutIntrinsicPadding;
        break;
    case VerticalAlign::BaselineMiddle:
        break;
    }

    LayoutUnit intrinsicPaddingAfter = rowHeight - logicalHeightWithoutIntrinsicPadding - intrinsicPaddingBefore;
    setIntrinsicPaddingBefore(intrinsicPaddingBefore);
    setIntrinsicPaddingAfter(intrinsicPaddingAfter);

    // FIXME: Changing an intrinsic padding shouldn't trigger a relayout as it only shifts the cell inside the row but
    // doesn't change the logical height.
    if (intrinsicPaddingBefore != oldIntrinsicPaddingBefore || intrinsicPaddingAfter != oldIntrinsicPaddingAfter)
        setNeedsLayout(MarkOnlyThis);
}

void RenderTableCell::updateLogicalWidth()
{
}

void RenderTableCell::setCellLogicalWidth(LayoutUnit tableLayoutLogicalWidth)
{
    if (tableLayoutLogicalWidth == logicalWidth())
        return;

    setNeedsLayout(MarkOnlyThis);
    row()->setChildNeedsLayout(MarkOnlyThis);

    if (!table()->selfNeedsLayout() && checkForRepaintDuringLayout())
        repaint();

    setLogicalWidth(tableLayoutLogicalWidth);
    setCellWidthChanged(true);
}

void RenderTableCell::layout()
{
    StackStats::LayoutCheckPoint layoutCheckPoint;

    int oldCellBaseline = cellBaselinePosition();
    layoutBlock(cellWidthChanged());

    // If we have replaced content, the intrinsic height of our content may have changed since the last time we laid out. If that's the case the intrinsic padding we used
    // for layout (the padding required to push the contents of the cell down to the row's baseline) is included in our new height and baseline and makes both
    // of them wrong. So if our content's intrinsic height has changed push the new content up into the intrinsic padding and relayout so that the rest of
    // table and row layout can use the correct baseline and height for this cell.
    if (isBaselineAligned() && section()->rowBaseline(rowIndex()) && cellBaselinePosition() > section()->rowBaseline(rowIndex())) {
        LayoutUnit newIntrinsicPaddingBefore = std::max<LayoutUnit>(0, intrinsicPaddingBefore() - std::max<LayoutUnit>(0, cellBaselinePosition() - oldCellBaseline));
        setIntrinsicPaddingBefore(newIntrinsicPaddingBefore);
        setNeedsLayout(MarkOnlyThis);
        layoutBlock(cellWidthChanged());
    }
    invalidateHasEmptyCollapsedBorders();
    
    // FIXME: This value isn't the intrinsic content logical height, but we need
    // to update the value as its used by flexbox layout. crbug.com/367324
    cacheIntrinsicContentLogicalHeightForFlexItem(contentLogicalHeight());

    setCellWidthChanged(false);
}

LayoutUnit RenderTableCell::paddingTop() const
{
    LayoutUnit result = computedCSSPaddingTop();
    if (!isHorizontalWritingMode())
        return result;
    return result + (style().writingMode() == TopToBottomWritingMode ? intrinsicPaddingBefore() : intrinsicPaddingAfter());
}

LayoutUnit RenderTableCell::paddingBottom() const
{
    LayoutUnit result = computedCSSPaddingBottom();
    if (!isHorizontalWritingMode())
        return result;
    return result + (style().writingMode() == TopToBottomWritingMode ? intrinsicPaddingAfter() : intrinsicPaddingBefore());
}

LayoutUnit RenderTableCell::paddingLeft() const
{
    LayoutUnit result = computedCSSPaddingLeft();
    if (isHorizontalWritingMode())
        return result;
    return result + (style().writingMode() == LeftToRightWritingMode ? intrinsicPaddingBefore() : intrinsicPaddingAfter());
}

LayoutUnit RenderTableCell::paddingRight() const
{   
    LayoutUnit result = computedCSSPaddingRight();
    if (isHorizontalWritingMode())
        return result;
    return result + (style().writingMode() == LeftToRightWritingMode ? intrinsicPaddingAfter() : intrinsicPaddingBefore());
}

LayoutUnit RenderTableCell::paddingBefore() const
{
    return computedCSSPaddingBefore() + intrinsicPaddingBefore();
}

LayoutUnit RenderTableCell::paddingAfter() const
{
    return computedCSSPaddingAfter() + intrinsicPaddingAfter();
}

void RenderTableCell::setOverrideContentLogicalHeightFromRowHeight(LayoutUnit rowHeight)
{
    clearIntrinsicPadding();
    setOverrideContentLogicalHeight(std::max<LayoutUnit>(0, rowHeight - borderAndPaddingLogicalHeight()));
}

LayoutSize RenderTableCell::offsetFromContainer(RenderElement& container, const LayoutPoint& point, bool* offsetDependsOnPoint) const
{
    ASSERT(&container == this->container());

    LayoutSize offset = RenderBlockFlow::offsetFromContainer(container, point, offsetDependsOnPoint);
    if (parent())
        offset -= parentBox()->locationOffset();

    return offset;
}

LayoutRect RenderTableCell::clippedOverflowRectForRepaint(const RenderLayerModelObject* repaintContainer) const
{
    // If the table grid is dirty, we cannot get reliable information about adjoining cells,
    // so we ignore outside borders. This should not be a problem because it means that
    // the table is going to recalculate the grid, relayout and repaint its current rect, which
    // includes any outside borders of this cell.
    if (!table()->collapseBorders() || table()->needsSectionRecalc())
        return RenderBlockFlow::clippedOverflowRectForRepaint(repaintContainer);

    bool rtl = !styleForCellFlow().isLeftToRightDirection();
    LayoutUnit outlineSize { style().outlineSize() };
    LayoutUnit left = std::max(borderHalfLeft(true), outlineSize);
    LayoutUnit right = std::max(borderHalfRight(true), outlineSize);
    LayoutUnit top = std::max(borderHalfTop(true), outlineSize);
    LayoutUnit bottom = std::max(borderHalfBottom(true), outlineSize);
    if ((left && !rtl) || (right && rtl)) {
        if (RenderTableCell* before = table()->cellBefore(this)) {
            top = std::max(top, before->borderHalfTop(true));
            bottom = std::max(bottom, before->borderHalfBottom(true));
        }
    }
    if ((left && rtl) || (right && !rtl)) {
        if (RenderTableCell* after = table()->cellAfter(this)) {
            top = std::max(top, after->borderHalfTop(true));
            bottom = std::max(bottom, after->borderHalfBottom(true));
        }
    }
    if (top) {
        if (RenderTableCell* above = table()->cellAbove(this)) {
            left = std::max(left, above->borderHalfLeft(true));
            right = std::max(right, above->borderHalfRight(true));
        }
    }
    if (bottom) {
        if (RenderTableCell* below = table()->cellBelow(this)) {
            left = std::max(left, below->borderHalfLeft(true));
            right = std::max(right, below->borderHalfRight(true));
        }
    }
    LayoutPoint location(std::max<LayoutUnit>(left, -visualOverflowRect().x()), std::max<LayoutUnit>(top, -visualOverflowRect().y()));
    LayoutRect r(-location.x(), -location.y(), location.x() + std::max(width() + right, visualOverflowRect().maxX()), location.y() + std::max(height() + bottom, visualOverflowRect().maxY()));

    // FIXME: layoutDelta needs to be applied in parts before/after transforms and
    // repaint containers. https://bugs.webkit.org/show_bug.cgi?id=23308
    r.move(view().frameView().layoutContext().layoutDelta());
    return computeRectForRepaint(r, repaintContainer);
}

Optional<LayoutRect> RenderTableCell::computeVisibleRectInContainer(const LayoutRect& rect, const RenderLayerModelObject* container, VisibleRectContext context) const
{
    if (container == this)
        return rect;
    LayoutRect adjustedRect = rect;
    if ((!view().frameView().layoutContext().isPaintOffsetCacheEnabled() || container || context.options.contains(VisibleRectContextOption::UseEdgeInclusiveIntersection)) && parent())
        adjustedRect.moveBy(-parentBox()->location()); // Rows are in the same coordinate space, so don't add their offset in.
    return RenderBlockFlow::computeVisibleRectInContainer(adjustedRect, container, context);
}

LayoutUnit RenderTableCell::cellBaselinePosition() const
{
    // <http://www.w3.org/TR/2007/CR-CSS21-20070719/tables.html#height-layout>: The baseline of a cell is the baseline of
    // the first in-flow line box in the cell, or the first in-flow table-row in the cell, whichever comes first. If there
    // is no such line box or table-row, the baseline is the bottom of content edge of the cell box.
    return firstLineBaseline().valueOr(borderAndPaddingBefore() + contentLogicalHeight());
}

static inline void markCellDirtyWhenCollapsedBorderChanges(RenderTableCell* cell)
{
    if (!cell)
        return;
    cell->setNeedsLayoutAndPrefWidthsRecalc();
}

void RenderTableCell::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    ASSERT(style().display() == DisplayType::TableCell);
    ASSERT(!row() || row()->rowIndexWasSet());

    RenderBlockFlow::styleDidChange(diff, oldStyle);
    setHasVisibleBoxDecorations(true); // FIXME: Optimize this to only set to true if necessary.

    if (parent() && section() && oldStyle && style().height() != oldStyle->height())
        section()->rowLogicalHeightChanged(rowIndex());

    // Our intrinsic padding pushes us down to align with the baseline of other cells on the row. If our vertical-align
    // has changed then so will the padding needed to align with other cells - clear it so we can recalculate it from scratch.
    if (oldStyle && style().verticalAlign() != oldStyle->verticalAlign())
        clearIntrinsicPadding();

    // If border was changed, notify table.
    RenderTable* table = this->table();
    if (table && oldStyle && oldStyle->border() != style().border()) {
        table->invalidateCollapsedBorders(this);
        if (table->collapseBorders() && diff == StyleDifference::Layout) {
            markCellDirtyWhenCollapsedBorderChanges(table->cellBelow(this));
            markCellDirtyWhenCollapsedBorderChanges(table->cellAbove(this));
            markCellDirtyWhenCollapsedBorderChanges(table->cellBefore(this));
            markCellDirtyWhenCollapsedBorderChanges(table->cellAfter(this));
        }
    }
}

// The following rules apply for resolving conflicts and figuring out which border
// to use.
// (1) Borders with the 'border-style' of 'hidden' take precedence over all other conflicting 
// borders. Any border with this value suppresses all borders at this location.
// (2) Borders with a style of 'none' have the lowest priority. Only if the border properties of all 
// the elements meeting at this edge are 'none' will the border be omitted (but note that 'none' is 
// the default value for the border style.)
// (3) If none of the styles are 'hidden' and at least one of them is not 'none', then narrow borders 
// are discarded in favor of wider ones. If several have the same 'border-width' then styles are preferred 
// in this order: 'double', 'solid', 'dashed', 'dotted', 'ridge', 'outset', 'groove', and the lowest: 'inset'.
// (4) If border styles differ only in color, then a style set on a cell wins over one on a row, 
// which wins over a row group, column, column group and, lastly, table. It is undefined which color 
// is used when two elements of the same type disagree.
static int compareBorders(const CollapsedBorderValue& border1, const CollapsedBorderValue& border2)
{
    // Sanity check the values passed in. The null border have lowest priority.
    if (!border2.exists()) {
        if (!border1.exists())
            return 0;
        return 1;
    }
    if (!border1.exists())
        return -1;

    // Rule #1 above.
    if (border2.style() == BorderStyle::Hidden) {
        if (border1.style() == BorderStyle::Hidden)
            return 0;
        return -1;
    }
    if (border1.style() == BorderStyle::Hidden)
        return 1;
    
    // Rule #2 above.  A style of 'none' has lowest priority and always loses to any other border.
    if (border2.style() == BorderStyle::None) {
        if (border1.style() == BorderStyle::None)
            return 0;
        return 1;
    }
    if (border1.style() == BorderStyle::None)
        return -1;

    // The first part of rule #3 above. Wider borders win.
    if (border1.width() != border2.width())
        return border1.width() < border2.width() ? -1 : 1;
    
    // The borders have equal width.  Sort by border style.
    if (border1.style() != border2.style())
        return border1.style() < border2.style() ? -1 : 1;
    
    // The border have the same width and style.  Rely on precedence (cell over row over row group, etc.)
    if (border1.precedence() == border2.precedence())
        return 0;
    return border1.precedence() < border2.precedence() ? -1 : 1;
}

static CollapsedBorderValue chooseBorder(const CollapsedBorderValue& border1, const CollapsedBorderValue& border2)
{
    const CollapsedBorderValue& border = compareBorders(border1, border2) < 0 ? border2 : border1;
    return border.style() == BorderStyle::Hidden ? CollapsedBorderValue() : border;
}

bool RenderTableCell::hasStartBorderAdjoiningTable() const
{
    bool isStartColumn = !col();
    bool isEndColumn = table()->colToEffCol(col() + colSpan() - 1) == table()->numEffCols() - 1;
    bool hasSameDirectionAsTable = isDirectionSame(this, section());

    // The table direction determines the row direction. In mixed directionality, we cannot guarantee that
    // we have a common border with the table (think a ltr table with rtl start cell).
    return (isStartColumn && hasSameDirectionAsTable) || (isEndColumn && !hasSameDirectionAsTable);
}

bool RenderTableCell::hasEndBorderAdjoiningTable() const
{
    bool isStartColumn = !col();
    bool isEndColumn = table()->colToEffCol(col() + colSpan() - 1) == table()->numEffCols() - 1;
    bool hasSameDirectionAsTable = isDirectionSame(this, section());

    // The table direction determines the row direction. In mixed directionality, we cannot guarantee that
    // we have a common border with the table (think a ltr table with ltr end cell).
    return (isStartColumn && !hasSameDirectionAsTable) || (isEndColumn && hasSameDirectionAsTable);
}

static CollapsedBorderValue emptyBorder()
{
    return CollapsedBorderValue(BorderValue(), Color(), BorderPrecedence::Cell);
}

CollapsedBorderValue RenderTableCell::collapsedStartBorder(IncludeBorderColorOrNot includeColor) const
{
    if (!table() || !section())
        return emptyBorder();

    if (m_hasEmptyCollapsedStartBorder)
        return emptyBorder();

    if (table()->collapsedBordersAreValid())
        return section()->cachedCollapsedBorder(*this, CBSStart);

    CollapsedBorderValue result = computeCollapsedStartBorder(includeColor);
    setHasEmptyCollapsedBorder(CBSStart, !result.width());
    if (includeColor && !m_hasEmptyCollapsedStartBorder)
        section()->setCachedCollapsedBorder(*this, CBSStart, result);
    return result;
}

CollapsedBorderValue RenderTableCell::computeCollapsedStartBorder(IncludeBorderColorOrNot includeColor) const
{
    // For the start border, we need to check, in order of precedence:
    // (1) Our start border.
    CSSPropertyID startColorProperty = includeColor ? CSSProperty::resolveDirectionAwareProperty(CSSPropertyBorderInlineStartColor, styleForCellFlow().direction(), styleForCellFlow().writingMode()) : CSSPropertyInvalid;
    CSSPropertyID endColorProperty = includeColor ? CSSProperty::resolveDirectionAwareProperty(CSSPropertyBorderInlineEndColor, styleForCellFlow().direction(), styleForCellFlow().writingMode()) : CSSPropertyInvalid;
    CollapsedBorderValue result(style().borderStart(), includeColor ? style().visitedDependentColorWithColorFilter(startColorProperty) : Color(), BorderPrecedence::Cell);

    RenderTable* table = this->table();
    if (!table)
        return result;
    // (2) The end border of the preceding cell.
    RenderTableCell* cellBefore = table->cellBefore(this);
    if (cellBefore) {
        CollapsedBorderValue cellBeforeAdjoiningBorder = CollapsedBorderValue(cellBefore->borderAdjoiningCellAfter(*this), includeColor ? cellBefore->style().visitedDependentColorWithColorFilter(endColorProperty) : Color(), BorderPrecedence::Cell);
        // |result| should be the 2nd argument as |cellBefore| should win in case of equality per CSS 2.1 (Border conflict resolution, point 4).
        result = chooseBorder(cellBeforeAdjoiningBorder, result);
        if (!result.exists())
            return result;
    }

    bool startBorderAdjoinsTable = hasStartBorderAdjoiningTable();
    if (startBorderAdjoinsTable) {
        // (3) Our row's start border.
        result = chooseBorder(result, CollapsedBorderValue(row()->borderAdjoiningStartCell(*this), includeColor ? parent()->style().visitedDependentColorWithColorFilter(startColorProperty) : Color(), BorderPrecedence::Row));
        if (!result.exists())
            return result;

        // (4) Our row group's start border.
        result = chooseBorder(result, CollapsedBorderValue(section()->borderAdjoiningStartCell(*this), includeColor ? section()->style().visitedDependentColorWithColorFilter(startColorProperty) : Color(), BorderPrecedence::RowGroup));
        if (!result.exists())
            return result;
    }
    
    // (5) Our column and column group's start borders.
    bool startColEdge;
    bool endColEdge;
    if (RenderTableCol* colElt = table->colElement(col(), &startColEdge, &endColEdge)) {
        if (colElt->isTableColumnGroup() && startColEdge) {
            // The |colElt| is a column group and is also the first colgroup (in case of spanned colgroups).
            result = chooseBorder(result, CollapsedBorderValue(colElt->borderAdjoiningCellStartBorder(), includeColor ? colElt->style().visitedDependentColorWithColorFilter(startColorProperty) : Color(), BorderPrecedence::ColumnGroup));
            if (!result.exists())
                return result;
        } else if (!colElt->isTableColumnGroup()) {
            // We first consider the |colElt| and irrespective of whether it is a spanned col or not, we apply
            // its start border. This is as per HTML5 which states that: "For the purposes of the CSS table model,
            // the col element is expected to be treated as if it was present as many times as its span attribute specifies".
            result = chooseBorder(result, CollapsedBorderValue(colElt->borderAdjoiningCellStartBorder(), includeColor ? colElt->style().visitedDependentColorWithColorFilter(startColorProperty) : Color(), BorderPrecedence::Column));
            if (!result.exists())
                return result;
            // Next, apply the start border of the enclosing colgroup but only if it is adjacent to the cell's edge.
            if (RenderTableCol* enclosingColumnGroup = colElt->enclosingColumnGroupIfAdjacentBefore()) {
                result = chooseBorder(result, CollapsedBorderValue(enclosingColumnGroup->borderAdjoiningCellStartBorder(), includeColor ? enclosingColumnGroup->style().visitedDependentColorWithColorFilter(startColorProperty) : Color(), BorderPrecedence::ColumnGroup));
                if (!result.exists())
                    return result;
            }
        }
    }
    
    // (6) The end border of the preceding column.
    if (cellBefore) {
        if (RenderTableCol* colElt = table->colElement(col() - 1, &startColEdge, &endColEdge)) {
            if (colElt->isTableColumnGroup() && endColEdge) {
                // The element is a colgroup and is also the last colgroup (in case of spanned colgroups).
                result = chooseBorder(CollapsedBorderValue(colElt->borderAdjoiningCellAfter(*this), includeColor ? colElt->style().visitedDependentColorWithColorFilter(endColorProperty) : Color(), BorderPrecedence::ColumnGroup), result);
                if (!result.exists())
                    return result;
            } else if (colElt->isTableColumn()) {
                // Resolve the collapsing border against the col's border ignoring any 'span' as per HTML5.
                result = chooseBorder(CollapsedBorderValue(colElt->borderAdjoiningCellAfter(*this), includeColor ? colElt->style().visitedDependentColorWithColorFilter(endColorProperty) : Color(), BorderPrecedence::Column), result);
                if (!result.exists())
                    return result;
                // Next, if the previous col has a parent colgroup then its end border should be applied
                // but only if it is adjacent to the cell's edge.
                if (RenderTableCol* enclosingColumnGroup = colElt->enclosingColumnGroupIfAdjacentAfter()) {
                    result = chooseBorder(CollapsedBorderValue(enclosingColumnGroup->borderAdjoiningCellEndBorder(), includeColor ? enclosingColumnGroup->style().visitedDependentColorWithColorFilter(endColorProperty) : Color(), BorderPrecedence::ColumnGroup), result);
                    if (!result.exists())
                        return result;
                }
            }
        }
    }

    if (startBorderAdjoinsTable) {
        // (7) The table's start border.
        result = chooseBorder(result, CollapsedBorderValue(table->tableStartBorderAdjoiningCell(*this), includeColor ? table->style().visitedDependentColorWithColorFilter(startColorProperty) : Color(), BorderPrecedence::Table));
        if (!result.exists())
            return result;
    }
    
    return result;
}

CollapsedBorderValue RenderTableCell::collapsedEndBorder(IncludeBorderColorOrNot includeColor) const
{
    if (!table() || !section())
        return emptyBorder();

    if (m_hasEmptyCollapsedEndBorder)
        return emptyBorder();

    if (table()->collapsedBordersAreValid())
        return section()->cachedCollapsedBorder(*this, CBSEnd);

    CollapsedBorderValue result = computeCollapsedEndBorder(includeColor);
    setHasEmptyCollapsedBorder(CBSEnd, !result.width());
    if (includeColor && !m_hasEmptyCollapsedEndBorder)
        section()->setCachedCollapsedBorder(*this, CBSEnd, result);
    return result;
}

CollapsedBorderValue RenderTableCell::computeCollapsedEndBorder(IncludeBorderColorOrNot includeColor) const
{
    // For end border, we need to check, in order of precedence:
    // (1) Our end border.
    CSSPropertyID startColorProperty = includeColor ? CSSProperty::resolveDirectionAwareProperty(CSSPropertyBorderInlineStartColor, styleForCellFlow().direction(), styleForCellFlow().writingMode()) : CSSPropertyInvalid;
    CSSPropertyID endColorProperty = includeColor ? CSSProperty::resolveDirectionAwareProperty(CSSPropertyBorderInlineEndColor, styleForCellFlow().direction(), styleForCellFlow().writingMode()) : CSSPropertyInvalid;
    CollapsedBorderValue result = CollapsedBorderValue(style().borderEnd(), includeColor ? style().visitedDependentColorWithColorFilter(endColorProperty) : Color(), BorderPrecedence::Cell);

    RenderTable* table = this->table();
    if (!table)
        return result;
    // Note: We have to use the effective column information instead of whether we have a cell after as a table doesn't
    // have to be regular (any row can have less cells than the total cell count).
    bool isEndColumn = table->colToEffCol(col() + colSpan() - 1) == table->numEffCols() - 1;
    // (2) The start border of the following cell.
    if (!isEndColumn) {
        if (RenderTableCell* cellAfter = table->cellAfter(this)) {
            CollapsedBorderValue cellAfterAdjoiningBorder = CollapsedBorderValue(cellAfter->borderAdjoiningCellBefore(*this), includeColor ? cellAfter->style().visitedDependentColorWithColorFilter(startColorProperty) : Color(), BorderPrecedence::Cell);
            result = chooseBorder(result, cellAfterAdjoiningBorder);
            if (!result.exists())
                return result;
        }
    }

    bool endBorderAdjoinsTable = hasEndBorderAdjoiningTable();
    if (endBorderAdjoinsTable) {
        // (3) Our row's end border.
        result = chooseBorder(result, CollapsedBorderValue(row()->borderAdjoiningEndCell(*this), includeColor ? parent()->style().visitedDependentColorWithColorFilter(endColorProperty) : Color(), BorderPrecedence::Row));
        if (!result.exists())
            return result;
        
        // (4) Our row group's end border.
        result = chooseBorder(result, CollapsedBorderValue(section()->borderAdjoiningEndCell(*this), includeColor ? section()->style().visitedDependentColorWithColorFilter(endColorProperty) : Color(), BorderPrecedence::RowGroup));
        if (!result.exists())
            return result;
    }
    
    // (5) Our column and column group's end borders.
    bool startColEdge;
    bool endColEdge;
    if (RenderTableCol* colElt = table->colElement(col() + colSpan() - 1, &startColEdge, &endColEdge)) {
        if (colElt->isTableColumnGroup() && endColEdge) {
            // The element is a colgroup and is also the last colgroup (in case of spanned colgroups).
            result = chooseBorder(result, CollapsedBorderValue(colElt->borderAdjoiningCellEndBorder(), includeColor ? colElt->style().visitedDependentColorWithColorFilter(endColorProperty) : Color(), BorderPrecedence::ColumnGroup));
            if (!result.exists())
                return result;
        } else if (!colElt->isTableColumnGroup()) {
            // First apply the end border of the column irrespective of whether it is spanned or not. This is as per
            // HTML5 which states that: "For the purposes of the CSS table model, the col element is expected to be
            // treated as if it was present as many times as its span attribute specifies".
            result = chooseBorder(result, CollapsedBorderValue(colElt->borderAdjoiningCellEndBorder(), includeColor ? colElt->style().visitedDependentColorWithColorFilter(endColorProperty) : Color(), BorderPrecedence::Column));
            if (!result.exists())
                return result;
            // Next, if it has a parent colgroup then we apply its end border but only if it is adjacent to the cell.
            if (RenderTableCol* enclosingColumnGroup = colElt->enclosingColumnGroupIfAdjacentAfter()) {
                result = chooseBorder(result, CollapsedBorderValue(enclosingColumnGroup->borderAdjoiningCellEndBorder(), includeColor ? enclosingColumnGroup->style().visitedDependentColorWithColorFilter(endColorProperty) : Color(), BorderPrecedence::ColumnGroup));
                if (!result.exists())
                    return result;
            }
        }
    }
    
    // (6) The start border of the next column.
    if (!isEndColumn) {
        if (RenderTableCol* colElt = table->colElement(col() + colSpan(), &startColEdge, &endColEdge)) {
            if (colElt->isTableColumnGroup() && startColEdge) {
                // This case is a colgroup without any col, we only compute it if it is adjacent to the cell's edge.
                result = chooseBorder(result, CollapsedBorderValue(colElt->borderAdjoiningCellBefore(*this), includeColor ? colElt->style().visitedDependentColorWithColorFilter(startColorProperty) : Color(), BorderPrecedence::ColumnGroup));
                if (!result.exists())
                    return result;
            } else if (colElt->isTableColumn()) {
                // Resolve the collapsing border against the col's border ignoring any 'span' as per HTML5.
                result = chooseBorder(result, CollapsedBorderValue(colElt->borderAdjoiningCellBefore(*this), includeColor ? colElt->style().visitedDependentColorWithColorFilter(startColorProperty) : Color(), BorderPrecedence::Column));
                if (!result.exists())
                    return result;
                // If we have a parent colgroup, resolve the border only if it is adjacent to the cell.
                if (RenderTableCol* enclosingColumnGroup = colElt->enclosingColumnGroupIfAdjacentBefore()) {
                    result = chooseBorder(result, CollapsedBorderValue(enclosingColumnGroup->borderAdjoiningCellStartBorder(), includeColor ? enclosingColumnGroup->style().visitedDependentColorWithColorFilter(startColorProperty) : Color(), BorderPrecedence::ColumnGroup));
                    if (!result.exists())
                        return result;
                }
            }
        }
    }

    if (endBorderAdjoinsTable) {
        // (7) The table's end border.
        result = chooseBorder(result, CollapsedBorderValue(table->tableEndBorderAdjoiningCell(*this), includeColor ? table->style().visitedDependentColorWithColorFilter(endColorProperty) : Color(), BorderPrecedence::Table));
        if (!result.exists())
            return result;
    }
    
    return result;
}

CollapsedBorderValue RenderTableCell::collapsedBeforeBorder(IncludeBorderColorOrNot includeColor) const
{
    if (!table() || !section())
        return emptyBorder();

    if (m_hasEmptyCollapsedBeforeBorder)
        return emptyBorder();

    if (table()->collapsedBordersAreValid())
        return section()->cachedCollapsedBorder(*this, CBSBefore);

    CollapsedBorderValue result = computeCollapsedBeforeBorder(includeColor);
    setHasEmptyCollapsedBorder(CBSBefore, !result.width());
    if (includeColor && !m_hasEmptyCollapsedBeforeBorder)
        section()->setCachedCollapsedBorder(*this, CBSBefore, result);
    return result;
}

CollapsedBorderValue RenderTableCell::computeCollapsedBeforeBorder(IncludeBorderColorOrNot includeColor) const
{
    // For before border, we need to check, in order of precedence:
    // (1) Our before border.
    CSSPropertyID beforeColorProperty = includeColor ? CSSProperty::resolveDirectionAwareProperty(CSSPropertyBorderBlockStartColor, styleForCellFlow().direction(), styleForCellFlow().writingMode()) : CSSPropertyInvalid;
    CSSPropertyID afterColorProperty = includeColor ? CSSProperty::resolveDirectionAwareProperty(CSSPropertyBorderBlockEndColor, styleForCellFlow().direction(), styleForCellFlow().writingMode()) : CSSPropertyInvalid;
    CollapsedBorderValue result = CollapsedBorderValue(style().borderBefore(), includeColor ? style().visitedDependentColorWithColorFilter(beforeColorProperty) : Color(), BorderPrecedence::Cell);
    
    RenderTable* table = this->table();
    if (!table)
        return result;
    RenderTableCell* prevCell = table->cellAbove(this);
    if (prevCell) {
        // (2) A before cell's after border.
        result = chooseBorder(CollapsedBorderValue(prevCell->style().borderAfter(), includeColor ? prevCell->style().visitedDependentColorWithColorFilter(afterColorProperty) : Color(), BorderPrecedence::Cell), result);
        if (!result.exists())
            return result;
    }
    
    // (3) Our row's before border.
    result = chooseBorder(result, CollapsedBorderValue(parent()->style().borderBefore(), includeColor ? parent()->style().visitedDependentColorWithColorFilter(beforeColorProperty) : Color(), BorderPrecedence::Row));
    if (!result.exists())
        return result;
    
    // (4) The previous row's after border.
    if (prevCell) {
        RenderObject* prevRow = 0;
        if (prevCell->section() == section())
            prevRow = parent()->previousSibling();
        else
            prevRow = prevCell->section()->lastRow();
    
        if (prevRow) {
            result = chooseBorder(CollapsedBorderValue(prevRow->style().borderAfter(), includeColor ? prevRow->style().visitedDependentColorWithColorFilter(afterColorProperty) : Color(), BorderPrecedence::Row), result);
            if (!result.exists())
                return result;
        }
    }
    
    // Now check row groups.
    RenderTableSection* currSection = section();
    if (!rowIndex()) {
        // (5) Our row group's before border.
        result = chooseBorder(result, CollapsedBorderValue(currSection->style().borderBefore(), includeColor ? currSection->style().visitedDependentColorWithColorFilter(beforeColorProperty) : Color(), BorderPrecedence::RowGroup));
        if (!result.exists())
            return result;
        
        // (6) Previous row group's after border.
        currSection = table->sectionAbove(currSection, SkipEmptySections);
        if (currSection) {
            result = chooseBorder(CollapsedBorderValue(currSection->style().borderAfter(), includeColor ? currSection->style().visitedDependentColorWithColorFilter(afterColorProperty) : Color(), BorderPrecedence::RowGroup), result);
            if (!result.exists())
                return result;
        }
    }
    
    if (!currSection) {
        // (8) Our column and column group's before borders.
        RenderTableCol* colElt = table->colElement(col());
        if (colElt) {
            result = chooseBorder(result, CollapsedBorderValue(colElt->style().borderBefore(), includeColor ? colElt->style().visitedDependentColorWithColorFilter(beforeColorProperty) : Color(), BorderPrecedence::Column));
            if (!result.exists())
                return result;
            if (RenderTableCol* enclosingColumnGroup = colElt->enclosingColumnGroup()) {
                result = chooseBorder(result, CollapsedBorderValue(enclosingColumnGroup->style().borderBefore(), includeColor ? enclosingColumnGroup->style().visitedDependentColorWithColorFilter(beforeColorProperty) : Color(), BorderPrecedence::ColumnGroup));
                if (!result.exists())
                    return result;
            }
        }
        
        // (9) The table's before border.
        result = chooseBorder(result, CollapsedBorderValue(table->style().borderBefore(), includeColor ? table->style().visitedDependentColorWithColorFilter(beforeColorProperty) : Color(), BorderPrecedence::Table));
        if (!result.exists())
            return result;
    }
    
    return result;
}

CollapsedBorderValue RenderTableCell::collapsedAfterBorder(IncludeBorderColorOrNot includeColor) const
{
    if (!table() || !section())
        return emptyBorder();

    if (m_hasEmptyCollapsedAfterBorder)
        return emptyBorder();

    if (table()->collapsedBordersAreValid())
        return section()->cachedCollapsedBorder(*this, CBSAfter);

    CollapsedBorderValue result = computeCollapsedAfterBorder(includeColor);
    setHasEmptyCollapsedBorder(CBSAfter, !result.width());
    if (includeColor && !m_hasEmptyCollapsedAfterBorder)
        section()->setCachedCollapsedBorder(*this, CBSAfter, result);
    return result;
}

CollapsedBorderValue RenderTableCell::computeCollapsedAfterBorder(IncludeBorderColorOrNot includeColor) const
{
    // For after border, we need to check, in order of precedence:
    // (1) Our after border.
    CSSPropertyID beforeColorProperty = includeColor ? CSSProperty::resolveDirectionAwareProperty(CSSPropertyBorderBlockStartColor, styleForCellFlow().direction(), styleForCellFlow().writingMode()) : CSSPropertyInvalid;
    CSSPropertyID afterColorProperty = includeColor ? CSSProperty::resolveDirectionAwareProperty(CSSPropertyBorderBlockEndColor, styleForCellFlow().direction(), styleForCellFlow().writingMode()) : CSSPropertyInvalid;
    CollapsedBorderValue result = CollapsedBorderValue(style().borderAfter(), includeColor ? style().visitedDependentColorWithColorFilter(afterColorProperty) : Color(), BorderPrecedence::Cell);
    
    RenderTable* table = this->table();
    if (!table)
        return result;
    RenderTableCell* nextCell = table->cellBelow(this);
    if (nextCell) {
        // (2) An after cell's before border.
        result = chooseBorder(result, CollapsedBorderValue(nextCell->style().borderBefore(), includeColor ? nextCell->style().visitedDependentColorWithColorFilter(beforeColorProperty) : Color(), BorderPrecedence::Cell));
        if (!result.exists())
            return result;
    }
    
    // (3) Our row's after border. (FIXME: Deal with rowspan!)
    result = chooseBorder(result, CollapsedBorderValue(parent()->style().borderAfter(), includeColor ? parent()->style().visitedDependentColorWithColorFilter(afterColorProperty) : Color(), BorderPrecedence::Row));
    if (!result.exists())
        return result;
    
    // (4) The next row's before border.
    if (nextCell) {
        result = chooseBorder(result, CollapsedBorderValue(nextCell->parent()->style().borderBefore(), includeColor ? nextCell->parent()->style().visitedDependentColorWithColorFilter(beforeColorProperty) : Color(), BorderPrecedence::Row));
        if (!result.exists())
            return result;
    }
    
    // Now check row groups.
    RenderTableSection* currSection = section();
    if (rowIndex() + rowSpan() >= currSection->numRows()) {
        // (5) Our row group's after border.
        result = chooseBorder(result, CollapsedBorderValue(currSection->style().borderAfter(), includeColor ? currSection->style().visitedDependentColorWithColorFilter(afterColorProperty) : Color(), BorderPrecedence::RowGroup));
        if (!result.exists())
            return result;
        
        // (6) Following row group's before border.
        currSection = table->sectionBelow(currSection, SkipEmptySections);
        if (currSection) {
            result = chooseBorder(result, CollapsedBorderValue(currSection->style().borderBefore(), includeColor ? currSection->style().visitedDependentColorWithColorFilter(beforeColorProperty) : Color(), BorderPrecedence::RowGroup));
            if (!result.exists())
                return result;
        }
    }
    
    if (!currSection) {
        // (8) Our column and column group's after borders.
        RenderTableCol* colElt = table->colElement(col());
        if (colElt) {
            result = chooseBorder(result, CollapsedBorderValue(colElt->style().borderAfter(), includeColor ? colElt->style().visitedDependentColorWithColorFilter(afterColorProperty) : Color(), BorderPrecedence::Column));
            if (!result.exists()) return result;
            if (RenderTableCol* enclosingColumnGroup = colElt->enclosingColumnGroup()) {
                result = chooseBorder(result, CollapsedBorderValue(enclosingColumnGroup->style().borderAfter(), includeColor ? enclosingColumnGroup->style().visitedDependentColorWithColorFilter(afterColorProperty) : Color(), BorderPrecedence::ColumnGroup));
                if (!result.exists())
                    return result;
            }
        }
        
        // (9) The table's after border.
        result = chooseBorder(result, CollapsedBorderValue(table->style().borderAfter(), includeColor ? table->style().visitedDependentColorWithColorFilter(afterColorProperty) : Color(), BorderPrecedence::Table));
        if (!result.exists())
            return result;
    }
    
    return result;    
}

inline CollapsedBorderValue RenderTableCell::cachedCollapsedLeftBorder(const RenderStyle& styleForCellFlow) const
{
    if (styleForCellFlow.isHorizontalWritingMode())
        return styleForCellFlow.isLeftToRightDirection() ? section()->cachedCollapsedBorder(*this, CBSStart) : section()->cachedCollapsedBorder(*this, CBSEnd);
    return styleForCellFlow.isFlippedBlocksWritingMode() ? section()->cachedCollapsedBorder(*this, CBSAfter) : section()->cachedCollapsedBorder(*this, CBSBefore);
}

inline CollapsedBorderValue RenderTableCell::cachedCollapsedRightBorder(const RenderStyle& styleForCellFlow) const
{
    if (styleForCellFlow.isHorizontalWritingMode())
        return styleForCellFlow.isLeftToRightDirection() ? section()->cachedCollapsedBorder(*this, CBSEnd) : section()->cachedCollapsedBorder(*this, CBSStart);
    return styleForCellFlow.isFlippedBlocksWritingMode() ? section()->cachedCollapsedBorder(*this, CBSBefore) : section()->cachedCollapsedBorder(*this, CBSAfter);
}

inline CollapsedBorderValue RenderTableCell::cachedCollapsedTopBorder(const RenderStyle& styleForCellFlow) const
{
    if (styleForCellFlow.isHorizontalWritingMode())
        return styleForCellFlow.isFlippedBlocksWritingMode() ? section()->cachedCollapsedBorder(*this, CBSAfter) : section()->cachedCollapsedBorder(*this, CBSBefore);
    return styleForCellFlow.isLeftToRightDirection() ? section()->cachedCollapsedBorder(*this, CBSStart) : section()->cachedCollapsedBorder(*this, CBSEnd);
}

inline CollapsedBorderValue RenderTableCell::cachedCollapsedBottomBorder(const RenderStyle& styleForCellFlow) const
{
    if (styleForCellFlow.isHorizontalWritingMode())
        return styleForCellFlow.isFlippedBlocksWritingMode() ? section()->cachedCollapsedBorder(*this, CBSBefore) : section()->cachedCollapsedBorder(*this, CBSAfter);
    return styleForCellFlow.isLeftToRightDirection() ? section()->cachedCollapsedBorder(*this, CBSEnd) : section()->cachedCollapsedBorder(*this, CBSStart);
}

LayoutUnit RenderTableCell::borderLeft() const
{
    RenderTable* table = this->table();
    if (!table)
        return RenderBlockFlow::borderLeft();
    return table->collapseBorders() ? borderHalfLeft(false) : RenderBlockFlow::borderLeft();
}

LayoutUnit RenderTableCell::borderRight() const
{
    RenderTable* table = this->table();
    if (!table)
        return RenderBlockFlow::borderRight();
    return table->collapseBorders() ? borderHalfRight(false) : RenderBlockFlow::borderRight();
}

LayoutUnit RenderTableCell::borderTop() const
{
    RenderTable* table = this->table();
    if (!table)
        return RenderBlockFlow::borderTop();
    return table->collapseBorders() ? borderHalfTop(false) : RenderBlockFlow::borderTop();
}

LayoutUnit RenderTableCell::borderBottom() const
{
    RenderTable* table = this->table();
    if (!table)
        return RenderBlockFlow::borderBottom();
    return table->collapseBorders() ? borderHalfBottom(false) : RenderBlockFlow::borderBottom();
}

// FIXME: https://bugs.webkit.org/show_bug.cgi?id=46191, make the collapsed border drawing
// work with different block flow values instead of being hard-coded to top-to-bottom.
LayoutUnit RenderTableCell::borderStart() const
{
    RenderTable* table = this->table();
    if (!table)
        return RenderBlockFlow::borderStart();
    return table->collapseBorders() ? borderHalfStart(false) : RenderBlockFlow::borderStart();
}

LayoutUnit RenderTableCell::borderEnd() const
{
    RenderTable* table = this->table();
    if (!table)
        return RenderBlockFlow::borderEnd();
    return table->collapseBorders() ? borderHalfEnd(false) : RenderBlockFlow::borderEnd();
}

LayoutUnit RenderTableCell::borderBefore() const
{
    RenderTable* table = this->table();
    if (!table)
        return RenderBlockFlow::borderBefore();
    return table->collapseBorders() ? borderHalfBefore(false) : RenderBlockFlow::borderBefore();
}

LayoutUnit RenderTableCell::borderAfter() const
{
    RenderTable* table = this->table();
    if (!table)
        return RenderBlockFlow::borderAfter();
    return table->collapseBorders() ? borderHalfAfter(false) : RenderBlockFlow::borderAfter();
}

LayoutUnit RenderTableCell::borderHalfLeft(bool outer) const
{
    const RenderStyle& styleForCellFlow = this->styleForCellFlow();
    if (styleForCellFlow.isHorizontalWritingMode())
        return styleForCellFlow.isLeftToRightDirection() ? borderHalfStart(outer) : borderHalfEnd(outer);
    return styleForCellFlow.isFlippedBlocksWritingMode() ? borderHalfAfter(outer) : borderHalfBefore(outer);
}

LayoutUnit RenderTableCell::borderHalfRight(bool outer) const
{
    const RenderStyle& styleForCellFlow = this->styleForCellFlow();
    if (styleForCellFlow.isHorizontalWritingMode())
        return styleForCellFlow.isLeftToRightDirection() ? borderHalfEnd(outer) : borderHalfStart(outer);
    return styleForCellFlow.isFlippedBlocksWritingMode() ? borderHalfBefore(outer) : borderHalfAfter(outer);
}

LayoutUnit RenderTableCell::borderHalfTop(bool outer) const
{
    const RenderStyle& styleForCellFlow = this->styleForCellFlow();
    if (styleForCellFlow.isHorizontalWritingMode())
        return styleForCellFlow.isFlippedBlocksWritingMode() ? borderHalfAfter(outer) : borderHalfBefore(outer);
    return styleForCellFlow.isLeftToRightDirection() ? borderHalfStart(outer) : borderHalfEnd(outer);
}

LayoutUnit RenderTableCell::borderHalfBottom(bool outer) const
{
    const RenderStyle& styleForCellFlow = this->styleForCellFlow();
    if (styleForCellFlow.isHorizontalWritingMode())
        return styleForCellFlow.isFlippedBlocksWritingMode() ? borderHalfBefore(outer) : borderHalfAfter(outer);
    return styleForCellFlow.isLeftToRightDirection() ? borderHalfEnd(outer) : borderHalfStart(outer);
}

LayoutUnit RenderTableCell::borderHalfStart(bool outer) const
{
    CollapsedBorderValue border = collapsedStartBorder(DoNotIncludeBorderColor);
    if (border.exists())
        return CollapsedBorderValue::adjustedCollapsedBorderWidth(border.width(), document().deviceScaleFactor(), styleForCellFlow().isLeftToRightDirection() ^ outer);
    return 0;
}
    
LayoutUnit RenderTableCell::borderHalfEnd(bool outer) const
{
    CollapsedBorderValue border = collapsedEndBorder(DoNotIncludeBorderColor);
    if (border.exists())
        return CollapsedBorderValue::adjustedCollapsedBorderWidth(border.width(), document().deviceScaleFactor(), !(styleForCellFlow().isLeftToRightDirection() ^ outer));
    return 0;
}

LayoutUnit RenderTableCell::borderHalfBefore(bool outer) const
{
    CollapsedBorderValue border = collapsedBeforeBorder(DoNotIncludeBorderColor);
    if (border.exists())
        return CollapsedBorderValue::adjustedCollapsedBorderWidth(border.width(), document().deviceScaleFactor(), !(styleForCellFlow().isFlippedBlocksWritingMode() ^ outer));
    return 0;
}

LayoutUnit RenderTableCell::borderHalfAfter(bool outer) const
{
    CollapsedBorderValue border = collapsedAfterBorder(DoNotIncludeBorderColor);
    if (border.exists())
        return CollapsedBorderValue::adjustedCollapsedBorderWidth(border.width(), document().deviceScaleFactor(), styleForCellFlow().isFlippedBlocksWritingMode() ^ outer);
    return 0;
}

void RenderTableCell::paint(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    ASSERT(paintInfo.phase != PaintPhase::CollapsedTableBorders);
    RenderBlockFlow::paint(paintInfo, paintOffset);
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
    CollapsedBorders()
        : m_count(0)
    {
    }
    
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

    CollapsedBorder* nextBorder()
    {
        for (unsigned i = 0; i < m_count; i++) {
            if (m_borders[i].borderValue.exists() && m_borders[i].shouldPaint) {
                m_borders[i].shouldPaint = false;
                return &m_borders[i];
            }
        }
        
        return 0;
    }
    
    CollapsedBorder m_borders[4];
    unsigned m_count;
};

static void addBorderStyle(RenderTable::CollapsedBorderValues& borderValues,
                           CollapsedBorderValue borderValue)
{
    if (!borderValue.exists())
        return;
    size_t count = borderValues.size();
    for (size_t i = 0; i < count; ++i)
        if (borderValues[i].isSameIgnoringColor(borderValue))
            return;
    borderValues.append(borderValue);
}

void RenderTableCell::collectBorderValues(RenderTable::CollapsedBorderValues& borderValues) const
{
    addBorderStyle(borderValues, collapsedStartBorder());
    addBorderStyle(borderValues, collapsedEndBorder());
    addBorderStyle(borderValues, collapsedBeforeBorder());
    addBorderStyle(borderValues, collapsedAfterBorder());
}

static int compareBorderValuesForQSort(const void* pa, const void* pb)
{
    const CollapsedBorderValue* a = static_cast<const CollapsedBorderValue*>(pa);
    const CollapsedBorderValue* b = static_cast<const CollapsedBorderValue*>(pb);
    if (a->isSameIgnoringColor(*b))
        return 0;
    return compareBorders(*a, *b);
}

void RenderTableCell::sortBorderValues(RenderTable::CollapsedBorderValues& borderValues)
{
    qsort(borderValues.data(), borderValues.size(), sizeof(CollapsedBorderValue),
        compareBorderValuesForQSort);
}

void RenderTableCell::paintCollapsedBorders(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    ASSERT(paintInfo.phase == PaintPhase::CollapsedTableBorders);

    if (!paintInfo.shouldPaintWithinRoot(*this) || style().visibility() != Visibility::Visible)
        return;

    LayoutRect localRepaintRect = paintInfo.rect;
    LayoutRect paintRect = LayoutRect(paintOffset + location(), frameRect().size());
    if (paintRect.y() - table()->outerBorderTop() >= localRepaintRect.maxY())
        return;

    if (paintRect.maxY() + table()->outerBorderBottom() <= localRepaintRect.y())
        return;

    GraphicsContext& graphicsContext = paintInfo.context();
    if (!table()->currentBorderValue() || graphicsContext.paintingDisabled())
        return;

    const RenderStyle& styleForCellFlow = this->styleForCellFlow();
    CollapsedBorderValue leftVal = cachedCollapsedLeftBorder(styleForCellFlow);
    CollapsedBorderValue rightVal = cachedCollapsedRightBorder(styleForCellFlow);
    CollapsedBorderValue topVal = cachedCollapsedTopBorder(styleForCellFlow);
    CollapsedBorderValue bottomVal = cachedCollapsedBottomBorder(styleForCellFlow);
     
    // Adjust our x/y/width/height so that we paint the collapsed borders at the correct location.
    LayoutUnit topWidth = topVal.width();
    LayoutUnit bottomWidth = bottomVal.width();
    LayoutUnit leftWidth = leftVal.width();
    LayoutUnit rightWidth = rightVal.width();

    float deviceScaleFactor = document().deviceScaleFactor();
    LayoutUnit leftHalfCollapsedBorder = CollapsedBorderValue::adjustedCollapsedBorderWidth(leftWidth , deviceScaleFactor, false);
    LayoutUnit topHalfCollapsedBorder = CollapsedBorderValue::adjustedCollapsedBorderWidth(topWidth, deviceScaleFactor, false);
    LayoutUnit righHalftCollapsedBorder = CollapsedBorderValue::adjustedCollapsedBorderWidth(rightWidth, deviceScaleFactor, true);
    LayoutUnit bottomHalfCollapsedBorder = CollapsedBorderValue::adjustedCollapsedBorderWidth(bottomWidth, deviceScaleFactor, true);
    
    LayoutRect borderRect = LayoutRect(paintRect.x() - leftHalfCollapsedBorder,
        paintRect.y() - topHalfCollapsedBorder,
        paintRect.width() + leftHalfCollapsedBorder + righHalftCollapsedBorder,
        paintRect.height() + topHalfCollapsedBorder + bottomHalfCollapsedBorder);

    BorderStyle topStyle = collapsedBorderStyle(topVal.style());
    BorderStyle bottomStyle = collapsedBorderStyle(bottomVal.style());
    BorderStyle leftStyle = collapsedBorderStyle(leftVal.style());
    BorderStyle rightStyle = collapsedBorderStyle(rightVal.style());
    
    bool renderTop = topStyle > BorderStyle::Hidden && !topVal.isTransparent() && floorToDevicePixel(topWidth, deviceScaleFactor);
    bool renderBottom = bottomStyle > BorderStyle::Hidden && !bottomVal.isTransparent() && floorToDevicePixel(bottomWidth, deviceScaleFactor);
    bool renderLeft = leftStyle > BorderStyle::Hidden && !leftVal.isTransparent() && floorToDevicePixel(leftWidth, deviceScaleFactor);
    bool renderRight = rightStyle > BorderStyle::Hidden && !rightVal.isTransparent() && floorToDevicePixel(rightWidth, deviceScaleFactor);

    // We never paint diagonals at the joins.  We simply let the border with the highest
    // precedence paint on top of borders with lower precedence.  
    CollapsedBorders borders;
    borders.addBorder(topVal, BSTop, renderTop, borderRect.x(), borderRect.y(), borderRect.maxX(), borderRect.y() + topWidth, topStyle);
    borders.addBorder(bottomVal, BSBottom, renderBottom, borderRect.x(), borderRect.maxY() - bottomWidth, borderRect.maxX(), borderRect.maxY(), bottomStyle);
    borders.addBorder(leftVal, BSLeft, renderLeft, borderRect.x(), borderRect.y(), borderRect.x() + leftWidth, borderRect.maxY(), leftStyle);
    borders.addBorder(rightVal, BSRight, renderRight, borderRect.maxX() - rightWidth, borderRect.y(), borderRect.maxX(), borderRect.maxY(), rightStyle);

    bool antialias = shouldAntialiasLines(graphicsContext);
    
    for (CollapsedBorder* border = borders.nextBorder(); border; border = borders.nextBorder()) {
        if (border->borderValue.isSameIgnoringColor(*table()->currentBorderValue()))
            drawLineForBoxSide(graphicsContext, LayoutRect(LayoutPoint(border->x1, border->y1), LayoutPoint(border->x2, border->y2)), border->side,
                border->borderValue.color(), border->style, 0, 0, antialias);
    }
}

void RenderTableCell::paintBackgroundsBehindCell(PaintInfo& paintInfo, const LayoutPoint& paintOffset, RenderElement* backgroundObject)
{
    if (!paintInfo.shouldPaintWithinRoot(*this))
        return;

    if (!backgroundObject)
        return;

    if (style().visibility() != Visibility::Visible)
        return;

    RenderTable* tableElt = table();
    if (!tableElt->collapseBorders() && style().emptyCells() == EmptyCell::Hide && !firstChild())
        return;

    LayoutPoint adjustedPaintOffset = paintOffset;
    if (backgroundObject != this)
        adjustedPaintOffset.moveBy(location());

    const auto& style = backgroundObject->style();
    auto& bgLayer = style.backgroundLayers();

    auto color = style.visitedDependentColor(CSSPropertyBackgroundColor);
    auto compositeOp = document().compositeOperatorForBackgroundColor(color, *this);

    color = style.colorByApplyingColorFilter(color);

    if (bgLayer.hasImage() || color.isValid()) {
        // We have to clip here because the background would paint
        // on top of the borders otherwise.  This only matters for cells and rows.
        bool shouldClip = backgroundObject->hasLayer() && (backgroundObject == this || backgroundObject == parent()) && tableElt->collapseBorders();
        GraphicsContextStateSaver stateSaver(paintInfo.context(), shouldClip);
        if (shouldClip) {
            LayoutRect clipRect(adjustedPaintOffset.x() + borderLeft(), adjustedPaintOffset.y() + borderTop(),
                width() - borderLeft() - borderRight(), height() - borderTop() - borderBottom());
            paintInfo.context().clip(clipRect);
        }
        paintFillLayers(paintInfo, color, bgLayer, LayoutRect(adjustedPaintOffset, frameRect().size()), BackgroundBleedNone, compositeOp, backgroundObject);
    }
}

void RenderTableCell::paintBoxDecorations(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (!paintInfo.shouldPaintWithinRoot(*this))
        return;

    RenderTable* table = this->table();
    if (!table->collapseBorders() && style().emptyCells() == EmptyCell::Hide && !firstChild())
        return;

    LayoutRect paintRect = LayoutRect(paintOffset, frameRect().size());
    adjustBorderBoxRectForPainting(paintRect);

    paintBoxShadow(paintInfo, paintRect, style(), ShadowStyle::Normal);
    
    // Paint our cell background.
    paintBackgroundsBehindCell(paintInfo, paintOffset, this);

    paintBoxShadow(paintInfo, paintRect, style(), ShadowStyle::Inset);

    if (!style().hasBorder() || table->collapseBorders())
        return;

    paintBorder(paintInfo, paintRect, style());
}

void RenderTableCell::paintMask(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (style().visibility() != Visibility::Visible || paintInfo.phase != PaintPhase::Mask)
        return;

    RenderTable* tableElt = table();
    if (!tableElt->collapseBorders() && style().emptyCells() == EmptyCell::Hide && !firstChild())
        return;
   
    LayoutRect paintRect = LayoutRect(paintOffset, frameRect().size());
    adjustBorderBoxRectForPainting(paintRect);

    paintMaskImages(paintInfo, paintRect);
}

bool RenderTableCell::boxShadowShouldBeAppliedToBackground(const LayoutPoint&, BackgroundBleedAvoidance, InlineFlowBox*) const
{
    return false;
}

void RenderTableCell::scrollbarsChanged(bool horizontalScrollbarChanged, bool verticalScrollbarChanged)
{
    LayoutUnit scrollbarHeight = scrollbarLogicalHeight();
    if (!scrollbarHeight)
        return; // Not sure if we should be doing something when a scrollbar goes away or not.
    
    // We only care if the scrollbar that affects our intrinsic padding has been added.
    if ((isHorizontalWritingMode() && !horizontalScrollbarChanged) ||
        (!isHorizontalWritingMode() && !verticalScrollbarChanged))
        return;

    // Shrink our intrinsic padding as much as possible to accommodate the scrollbar.
    if (style().verticalAlign() == VerticalAlign::Middle) {
        LayoutUnit totalHeight = logicalHeight();
        LayoutUnit heightWithoutIntrinsicPadding = totalHeight - intrinsicPaddingBefore() - intrinsicPaddingAfter();
        totalHeight -= scrollbarHeight;
        LayoutUnit newBeforePadding = (totalHeight - heightWithoutIntrinsicPadding) / 2;
        LayoutUnit newAfterPadding = totalHeight - heightWithoutIntrinsicPadding - newBeforePadding;
        setIntrinsicPaddingBefore(newBeforePadding);
        setIntrinsicPaddingAfter(newAfterPadding);
    } else
        setIntrinsicPaddingAfter(intrinsicPaddingAfter() - scrollbarHeight);
}

RenderPtr<RenderTableCell> RenderTableCell::createTableCellWithStyle(Document& document, const RenderStyle& style)
{
    auto cell = createRenderer<RenderTableCell>(document, RenderStyle::createAnonymousStyleWithDisplay(style, DisplayType::TableCell));
    cell->initializeStyle();
    return cell;
}

RenderPtr<RenderTableCell> RenderTableCell::createAnonymousWithParentRenderer(const RenderTableRow& parent)
{
    return RenderTableCell::createTableCellWithStyle(parent.document(), parent.style());
}

bool RenderTableCell::hasLineIfEmpty() const
{
    if (element() && element()->hasEditableStyle())
        return true;

    return RenderBlock::hasLineIfEmpty();
}

} // namespace WebCore
