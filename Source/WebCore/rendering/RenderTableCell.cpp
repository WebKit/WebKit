/*
 * Copyright (C) 1997 Martin Jones (mjones@kde.org)
 *           (C) 1997 Torben Weis (weis@kde.org)
 *           (C) 1998 Waldo Bastian (bastian@kde.org)
 *           (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003-2026 Apple Inc. All rights reserved.
 * Copyright (C) 2016-2017 Google Inc. All rights reserved.
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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
#include "ElementInlines.h"
#include "FloatQuad.h"
#include "HTMLNames.h"
#include "HTMLTableCellElement.h"
#include "HTMLTableRowElement.h"
#include "LayoutScope.h"
#include "PaintInfo.h"
#include "RenderBoxInlines.h"
#include "RenderBoxModelObjectInlines.h"
#include "RenderElementInlines.h"
#include "RenderFlexibleBox.h"
#include "RenderObjectInlines.h"
#include "RenderTableCellInlines.h"
#include "RenderTableCol.h"
#include "RenderTableInlines.h"
#include "RenderTheme.h"
#include "RenderView.h"
#include "Settings.h"
#include "StylePrimitiveNumericTypes+Evaluation.h"
#include "StyleProperties.h"
#include "TransformState.h"
#include <ranges>
#include <wtf/SetForScope.h>
#include <wtf/StackStats.h>
#include <wtf/TZoneMallocInlines.h>

#if ENABLE(MATHML)
#include "MathMLElement.h"
#include "MathMLNames.h"
#endif

namespace WebCore {

using namespace HTMLNames;

WTF_MAKE_TZONE_ALLOCATED_IMPL(RenderTableCell);

struct SameSizeAsRenderTableCell : public RenderBlockFlow {
    unsigned bitfields;
    LayoutUnit padding[2];
    bool isComputingPreferredSize;
    std::optional<LayoutUnit> orthogonalCellContentIntrinsicHeight;
};

static_assert(sizeof(RenderTableCell) == sizeof(SameSizeAsRenderTableCell), "RenderTableCell should stay small");
static_assert(sizeof(CollapsedBorderValue) <= 24, "CollapsedBorderValue should stay small");

RenderTableCell::RenderTableCell(Element& element, UniqueRef<Style::ComputedStyle>&& style)
    : RenderBlockFlow(Type::TableCell, element, WTF::move(style))
{
    // We only update the flags when notified of DOM changes in colSpanOrRowSpanChanged()
    // so we need to set their initial values here in case something asks for colSpan()/rowSpan() before then.
    updateColAndRowSpanFlags();
    ASSERT(isRenderTableCell());
}

RenderTableCell::RenderTableCell(Document& document, UniqueRef<Style::ComputedStyle>&& style)
    : RenderBlockFlow(Type::TableCell, document, WTF::move(style))
{
    ASSERT(isRenderTableCell());
}

RenderTableCell::~RenderTableCell() = default;

ASCIILiteral RenderTableCell::renderName() const
{
    return (isAnonymous() || isPseudoElement()) ? "RenderTableCell (anonymous)"_s : "RenderTableCell"_s;
}

void RenderTableCell::willBeRemovedFromTree()
{
    RenderBlockFlow::willBeRemovedFromTree();
    if (!table() || !section())
        return;
    RenderTableSection* section = this->section();
    table()->invalidateCollapsedBorders();
    section->removeCachedCollapsedBorders(*this);
    section->setNeedsCellRecalc();
}

unsigned RenderTableCell::parseColSpanFromDOM() const
{
    ASSERT(element());
    if (RefPtr cell = dynamicDowncast<HTMLTableCellElement>(*element()))
        return std::min<unsigned>(cell->colSpan(), maxColumnIndex);
#if ENABLE(MATHML)
    RefPtr mathMLElement = dynamicDowncast<MathMLElement>(*element());
    if (mathMLElement && mathMLElement->hasTagName(MathMLNames::mtdTag))
        return std::min<unsigned>(mathMLElement->colSpan(), maxColumnIndex);
#endif
    return 1;
}

unsigned RenderTableCell::parseRowSpanFromDOM() const
{
    ASSERT(element());
    if (RefPtr cell = dynamicDowncast<HTMLTableCellElement>(*element()))
        return std::min<unsigned>(cell->rowSpan(), maxRowIndex);
#if ENABLE(MATHML)
    RefPtr mathMLElement = dynamicDowncast<MathMLElement>(*element());
    if (mathMLElement && mathMLElement->hasTagName(MathMLNames::mtdTag))
        return std::min<unsigned>(mathMLElement->rowSpan(), maxRowIndex);
#endif
    return 1;
}

unsigned RenderTableCell::calculateRowSpanForRowSpanZero() const
{
    // Handle rowspan="0" which means "span all remaining rows in the row group"
    // Per HTML spec: https://html.spec.whatwg.org/multipage/tables.html#attr-tdth-rowspan
    if (CheckedPtr renderSection = this->section()) {
        unsigned totalRows = renderSection->rowCount();
        unsigned currentRow = this->rowIndex();
        if (currentRow < totalRows)
            return totalRows - currentRow;
    }
    // Fallback: couldn't get section
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

    setNeedsLayoutAndInvalidateContentLogicalWidths();
    if (parent() && section())
        section()->setNeedsCellRecalc();
}

Style::PreferredSize RenderTableCell::logicalWidthFromColumns(RenderTableCol* firstColForThisCell, const Style::PreferredSize& widthFromStyle) const
{
    ASSERT(firstColForThisCell && firstColForThisCell == table()->colElement(col()));
    RenderTableCol* tableCol = firstColForThisCell;

    unsigned colSpanCount = colSpan();
    LayoutUnit colWidthSum;
    for (unsigned i = 1; i <= colSpanCount; i++) {
        auto& colWidth = tableCol->style().logicalWidth();

        auto fixedColWidth = colWidth.tryFixed();

        // Percentage value should be returned only for colSpan == 1.
        // Otherwise we return original width for the cell.
        if (!fixedColWidth) {
            if (colSpanCount > 1)
                return widthFromStyle;
            return colWidth;
        }

        colWidthSum += fixedColWidth->resolveZoom(tableCol->style().usedZoomForLength());
        tableCol = tableCol->nextColumn();
        // If no next <col> tag found for the span we just return what we have for now.
        if (!tableCol)
            break;
    }

    // Column widths specified on <col> apply to the border box of the cell, see bug 8126.
    // FIXME: Why is border/padding ignored in the negative width case?
    if (colWidthSum > 0)
        return Style::PreferredSize::Fixed { std::max<LayoutUnit>(0, colWidthSum - borderAndPaddingLogicalWidth()) };
    return Style::PreferredSize::Fixed { colWidthSum };
}

std::pair<LayoutUnit, LayoutUnit> RenderTableCell::computeIntrinsicLogicalWidthContributions()
{
    // The child cells rely on the grids up in the sections to do their computeIntrinsicLogicalWidthContributions work.
    // Normally the sections are set up early, as table cells are added, but relayout can cause the cells to be freed, leaving stale pointers in the sections' grids.
    // We must refresh those grids before the child cells try to use them.
    table()->recalcSectionsIfNeeded();

    // We don't want the preferred width from children to be affected by any
    // notional height on the cell, such as can happen when a percent sized image
    // scales up its width to match the available height. Setting a zero override
    // height prevents this from happening.
    auto overridingLogicalHeight = this->overridingBorderBoxLogicalHeight();
    if (overridingLogicalHeight)
        setOverridingBorderBoxLogicalHeight({ });
    auto contributions = RenderBlockFlow::computeIntrinsicLogicalWidthContributions();
    if (overridingLogicalHeight)
        setOverridingBorderBoxLogicalHeight(*overridingLogicalHeight);
    return contributions;
}

void RenderTableCell::constrainIntrinsicLogicalWidthContributionsByMinMax(LayoutUnit& minContentLogicalWidthContribution, LayoutUnit& maxContentLogicalWidthContribution) const
{
    RenderBlockFlow::constrainIntrinsicLogicalWidthContributionsByMinMax(minContentLogicalWidthContribution, maxContentLogicalWidthContribution);

    if (!element() || !document().inQuirksMode() || !element()->hasAttributeWithoutSynchronization(nowrapAttr))
        return;

    auto [ logicalWidth, usedZoom ] = styleOrColLogicalWidth();
    if (auto fixedLogicalWidth = logicalWidth.tryFixed()) {
        // In quirks mode, when nowrap is set on a cell that also has an explicit fixed width,
        // WinIE/Moz treat the fixed width as the minimum width of the cell. Affected the top
        // of hiptop.com.
        minContentLogicalWidthContribution = std::max(adjustBorderBoxLogicalWidthForBoxSizing(logicalWidth, LayoutUnit(fixedLogicalWidth->resolveZoom(usedZoom)), 0_lu), minContentLogicalWidthContribution);
    }
}

bool RenderTableCell::computeIntrinsicPadding(LayoutUnit heightConstraint)
{
    auto oldIntrinsicPaddingBefore = LayoutUnit { };
    auto oldIntrinsicPaddingAfter = LayoutUnit { };
    // logicalHeight() here means orginal border box height (i.e. no column stretching for orthogonal box).
    auto borderBoxLogicalHeight = LayoutUnit { };
    if (!isOrthogonal()) {
        oldIntrinsicPaddingBefore = intrinsicPaddingBefore();
        oldIntrinsicPaddingAfter = intrinsicPaddingAfter();
        borderBoxLogicalHeight = logicalHeight();
    } else {
        ASSERT(m_orthogonalCellContentIntrinsicHeight);
        borderBoxLogicalHeight = m_orthogonalCellContentIntrinsicHeight.value_or(0_lu) + RenderBlockFlow::paddingBefore() + RenderBlockFlow::paddingAfter() + borderBefore() + borderAfter();
        oldIntrinsicPaddingBefore = { };
        oldIntrinsicPaddingAfter = { };
    }
    auto borderBoxLogicalHeightWithoutIntrinsicPadding = borderBoxLogicalHeight - (oldIntrinsicPaddingBefore + oldIntrinsicPaddingAfter);

    auto intrinsicPaddingBefore = oldIntrinsicPaddingBefore;
    auto alignment = verticalAlignForTableCell(style(), document().settings());
    if (auto alignContent = style().alignContent(); !alignContent.isNormal()) {
        auto resolveAlignedContent = alignContent.resolve();
        // align-content overrides vertical-align
        if (resolveAlignedContent.position() == ContentPosition::Baseline)
            alignment = CSS::Keyword::Baseline { };
        else if (resolveAlignedContent.isCentered())
            alignment = CSS::Keyword::Middle { };
        else if (resolveAlignedContent.isStartward())
            alignment = CSS::Keyword::Top { };
        else if (resolveAlignedContent.isEndward())
            alignment = CSS::Keyword::Bottom { };
    }

    auto applyStandard = [&] {
        auto baseline = cellBaselinePosition();
        if (!firstInFlowChild() && baseline == borderAndPaddingBefore()) {
            // If baseline equals borderAndPaddingBefore(), there is no real content.
            intrinsicPaddingBefore = 0;
            return;
        }

        auto needsIntrinsicPadding = baseline > borderAndPaddingBefore() || !borderBoxLogicalHeight;
        if (needsIntrinsicPadding)
            intrinsicPaddingBefore = section()->rowBaseline(rowIndex()) - (baseline - oldIntrinsicPaddingBefore);
    };

    WTF::switchOn(alignment,
        [&](const CSS::Keyword::Sub&) {
            applyStandard();
        },
        [&](const CSS::Keyword::Super&) {
            applyStandard();
        },
        [&](const CSS::Keyword::TextTop&) {
            applyStandard();
        },
        [&](const CSS::Keyword::TextBottom&) {
            applyStandard();
        },
        [&](const CSS::Keyword::Baseline&) {
            applyStandard();
        },
        [&](const CSS::Keyword::Top&) {
            // Do nothing.
        },
        [&](const CSS::Keyword::Middle&) {
            intrinsicPaddingBefore = (heightConstraint - borderBoxLogicalHeightWithoutIntrinsicPadding) / 2;
        },
        [&](const CSS::Keyword::Bottom&) {
            intrinsicPaddingBefore = heightConstraint - borderBoxLogicalHeightWithoutIntrinsicPadding;
        },
        [&](const CSS::Keyword::WebkitBaselineMiddle&) {
            // Do nothing.
        },
        [&](const Style::VerticalAlign::LengthPercentage&) {
            applyStandard();
        }
    );

    LayoutUnit intrinsicPaddingAfter = heightConstraint - borderBoxLogicalHeightWithoutIntrinsicPadding - intrinsicPaddingBefore;
    setIntrinsicPaddingBefore(intrinsicPaddingBefore);
    setIntrinsicPaddingAfter(intrinsicPaddingAfter);

    return intrinsicPaddingBefore != oldIntrinsicPaddingBefore || intrinsicPaddingAfter != oldIntrinsicPaddingAfter;
}

RenderBox::LogicalExtentComputedValues RenderTableCell::computeLogicalHeight(LayoutUnit logicalHeight, LayoutUnit logicalTop) const
{
    if (isOrthogonal()) {
        // Note that at this point, contentBoxLogicalHeight means content height.
        m_orthogonalCellContentIntrinsicHeight = contentBoxLogicalHeight();
    }
    return RenderBlockFlow::computeLogicalHeight(logicalHeight, logicalTop);
}

void RenderTableCell::updateLogicalWidth()
{
    if (isComputingPreferredSize()) {
        // While table layout sets the final logical width for cells, in case of
        // preferred width computation for orthogonal content, we have
        // to follow normal layout flow to be able to compute logical height.
        RenderBlockFlow::updateLogicalWidth();
        return;
    }
    if (auto logicalWidth = overridingBorderBoxLogicalWidth())
        setLogicalWidth(*logicalWidth);
}

LayoutUnit RenderTableCell::logicalHeightForRowSizing() const
{
    auto logicalSizeForRowSizing = [&] {
        if (!isOrthogonal())
            return logicalHeight() - (intrinsicPaddingBefore() + intrinsicPaddingAfter());
        LogicalExtentComputedValues values;
        computeLogicalWidth(values);
        return values.extent;
    };
    // FIXME: This function does too much work, and is very hot during table layout!
    auto usedLogicalSize = logicalSizeForRowSizing();
    auto specifiedSize = !isOrthogonal() ? style().logicalHeight() : style().logicalWidth();
    if (!specifiedSize.isSpecified())
        return usedLogicalSize;
    auto computedLogicalSize = Style::evaluate<LayoutUnit>(specifiedSize, 0_lu, style().usedZoomForLength());
    // In strict mode, box-sizing: content-box do the right thing and actually add in the border and padding.
    // Call computedCSSPadding* directly to avoid including implicitPadding.
    if (!document().inQuirksMode() && style().boxSizing() != BoxSizing::BorderBox)
        computedLogicalSize += computedCSSPaddingBefore() + computedCSSPaddingAfter() + borderBefore() + borderAfter();
    return std::max(computedLogicalSize, usedLogicalSize);
}

void RenderTableCell::setCellLogicalWidth(LayoutUnit logicalWidthInTableDirection)
{
    auto logicalSizeInTableDirection = !isOrthogonal() ? logicalWidth() : logicalHeight();
    if (logicalWidthInTableDirection == logicalSizeInTableDirection)
        return;

    setNeedsLayout(MarkingBehavior::OnlyThis);
    setCellWidthChanged(true);

    if (!isOrthogonal()) {
        setLogicalWidth(logicalWidthInTableDirection);
        return;
    }

    setLogicalHeight(logicalWidthInTableDirection);
    // As table layout drives the size of table cells, we have to prevent regular layout flow from
    // overriding this height value. This is similar to how RenderTableCell handles logical widths by overriding ::updateLogicalWidth.
    setOverridingBorderBoxLogicalHeight(logicalWidthInTableDirection);
}

void RenderTableCell::layout()
{
    StackStats::LayoutCheckPoint layoutCheckPoint;

    int oldCellBaseline = cellBaselinePosition();

    auto scope = LayoutScope { *this };
    layoutBlock(cellWidthChanged() ? RelayoutChildren::Yes : RelayoutChildren::No);

    // If we have replaced content, the intrinsic height of our content may have changed since the last time we laid out. If that's the case the intrinsic padding we used
    // for layout (the padding required to push the contents of the cell down to the row's baseline) is included in our new height and baseline and makes both
    // of them wrong. So if our content's intrinsic height has changed push the new content up into the intrinsic padding and relayout so that the rest of
    // table and row layout can use the correct baseline and height for this cell.
    if (isBaselineAligned() && section()->rowBaseline(rowIndex()) && cellBaselinePosition() > section()->rowBaseline(rowIndex())) {
        LayoutUnit newIntrinsicPaddingBefore = std::max<LayoutUnit>(0, intrinsicPaddingBefore() - std::max<LayoutUnit>(0, cellBaselinePosition() - oldCellBaseline));
        setIntrinsicPaddingBefore(newIntrinsicPaddingBefore);
        setNeedsLayout(MarkingBehavior::OnlyThis);
        layoutBlock(cellWidthChanged() ? RelayoutChildren::Yes : RelayoutChildren::No);
    }
    invalidateHasEmptyCollapsedBorders();
    
    // FIXME: This value isn't the intrinsic content logical height, but we need
    // to update the value as its used by flexbox layout. crbug.com/367324
    if (CheckedPtr flexContainer = dynamicDowncast<RenderFlexibleBox>(parent()))
        flexContainer->setFlexItemContentLogicalHeightFromLayout(*this, contentBoxLogicalHeight());

    setCellWidthChanged(false);
}

RectEdges<LayoutUnit> RenderTableCell::padding() const
{
    auto top = computedCSSPaddingTop();
    auto right = computedCSSPaddingRight();
    auto bottom = computedCSSPaddingBottom();
    auto left = computedCSSPaddingLeft();

    if (isHorizontalWritingMode()) {
        bool isTopToBottom = writingMode().isBlockTopToBottom();
        top += isTopToBottom ? intrinsicPaddingBefore() : intrinsicPaddingAfter();
        bottom += isTopToBottom ? intrinsicPaddingAfter() : intrinsicPaddingBefore();
    } else {
        bool isLeftToRight = writingMode().isBlockLeftToRight();
        left += isLeftToRight ? intrinsicPaddingBefore() : intrinsicPaddingAfter();
        right += isLeftToRight ? intrinsicPaddingAfter() : intrinsicPaddingBefore();
    }

    return {
        top,
        right,
        bottom,
        left
    };
}

LayoutUnit RenderTableCell::paddingTop() const
{
    LayoutUnit result = computedCSSPaddingTop();
    if (!isHorizontalWritingMode())
        return result;
    return result + (writingMode().isBlockTopToBottom() ? intrinsicPaddingBefore() : intrinsicPaddingAfter());
}

LayoutUnit RenderTableCell::paddingBottom() const
{
    LayoutUnit result = computedCSSPaddingBottom();
    if (!isHorizontalWritingMode())
        return result;
    return result + (writingMode().isBlockTopToBottom()? intrinsicPaddingAfter() : intrinsicPaddingBefore());
}

LayoutUnit RenderTableCell::paddingLeft() const
{
    LayoutUnit result = computedCSSPaddingLeft();
    if (isHorizontalWritingMode())
        return result;
    return result + (writingMode().isBlockLeftToRight() ? intrinsicPaddingBefore() : intrinsicPaddingAfter());
}

LayoutUnit RenderTableCell::paddingRight() const
{   
    LayoutUnit result = computedCSSPaddingRight();
    if (isHorizontalWritingMode())
        return result;
    return result + (writingMode().isBlockLeftToRight() ? intrinsicPaddingAfter() : intrinsicPaddingBefore());
}

LayoutUnit RenderTableCell::paddingBefore() const
{
    return computedCSSPaddingBefore() + intrinsicPaddingBefore();
}

LayoutUnit RenderTableCell::paddingAfter() const
{
    return computedCSSPaddingAfter() + intrinsicPaddingAfter();
}

void RenderTableCell::setOverridingLogicalHeightFromRowHeight(LayoutUnit rowHeight)
{
    clearIntrinsicPadding();
    setOverridingBorderBoxLogicalHeight(rowHeight);
}

LayoutUnit RenderTableCell::minLogicalWidthForColumnSizing()
{
    if (!isOrthogonal())
        return RenderBlockFlow::minContentLogicalWidthContribution();

    auto computingPreferredSize = SetForScope<bool> { m_isComputingPreferredSize, true };
    setNeedsLayout(MarkingBehavior::OnlyThis);
    layoutIfNeeded();
    ASSERT(m_orthogonalCellContentIntrinsicHeight.has_value());
    return std::max(logicalHeight(), m_orthogonalCellContentIntrinsicHeight.value_or(0_lu));
}

LayoutUnit RenderTableCell::maxLogicalWidthForColumnSizing()
{
    if (!isOrthogonal())
        return RenderBlockFlow::maxContentLogicalWidthContribution();

    auto computingPreferredSize = SetForScope<bool> { m_isComputingPreferredSize, true };
    setNeedsLayout(MarkingBehavior::OnlyThis);
    layoutIfNeeded();
    ASSERT(m_orthogonalCellContentIntrinsicHeight.has_value());
    return std::max(logicalHeight(), m_orthogonalCellContentIntrinsicHeight.value_or(0_lu));
}

LayoutSize RenderTableCell::offsetFromContainer(const RenderElement& container, const LayoutPoint& point, bool* offsetDependsOnPoint) const
{
    ASSERT(&container == this->container());
    return RenderBlockFlow::offsetFromContainer(container, point, offsetDependsOnPoint);
}

auto RenderTableCell::localRectsForRepaint(RepaintOutlineBounds repaintOutlineBounds) const -> RepaintRects
{
    // If the table grid is dirty, we cannot get reliable information about adjoining cells,
    // so we ignore outside borders. This should not be a problem because it means that
    // the table is going to recalculate the grid, relayout and repaint its current rect, which
    // includes any outside borders of this cell.
    if (!table()->collapseBorders() || table()->needsSectionRecalc())
        return RenderBlockFlow::localRectsForRepaint(repaintOutlineBounds);

    bool flippedInline = tableWritingMode().isInlineFlipped();
    LayoutUnit outlineSize { usedStyle().outlineSize() };
    LayoutUnit left = std::max(borderHalfLeft(true), outlineSize);
    LayoutUnit right = std::max(borderHalfRight(true), outlineSize);
    LayoutUnit top = std::max(borderHalfTop(true), outlineSize);
    LayoutUnit bottom = std::max(borderHalfBottom(true), outlineSize);
    if ((left && !flippedInline) || (right && flippedInline)) {
        if (RenderTableCell* before = table()->cellBefore(this)) {
            top = std::max(top, before->borderHalfTop(true));
            bottom = std::max(bottom, before->borderHalfBottom(true));
        }
    }
    if ((left && flippedInline) || (right && !flippedInline)) {
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
    auto overflowRect = LayoutRect(-location.x(), -location.y(), location.x() + std::max(borderBoxWidth() + right, visualOverflowRect().maxX()), location.y() + std::max(borderBoxHeight() + bottom, visualOverflowRect().maxY()));

    // FIXME: layoutDelta needs to be applied in parts before/after transforms and
    // repaint containers. https://bugs.webkit.org/show_bug.cgi?id=23308
    overflowRect.move(view().frameView().layoutContext().layoutDelta());

    auto rects = RepaintRects { overflowRect };
    if (repaintOutlineBounds == RepaintOutlineBounds::Yes)
        rects.outlineBoundsRect = localOutlineBoundsRepaintRect();

    return rects;
}

LayoutUnit RenderTableCell::containingBlockLogicalWidthForContent() const
{
    // For percentage padding/width resolution in table cells, we need to resolve
    // against the sum of column widths plus interleaved border spacing (but not
    // the leading/trailing border spacing).
    CheckedPtr renderTable = table();
    if (!renderTable)
        return RenderBlockFlow::containingBlockLogicalWidthForContent();

    // The table's columnPositions() array contains the cumulative positions of columns.
    // The last element gives us the total width including all columns and interleaved spacing.
    // We need to subtract the leading border spacing to get the correct basis.
    auto& columnPositions = renderTable->columnPositions();
    if (columnPositions.isEmpty())
        return RenderBlockFlow::containingBlockLogicalWidthForContent();

    LayoutUnit totalWidth = columnPositions.last();

    // Subtract the leading border spacing
    if (totalWidth > 0)
        totalWidth -= renderTable->hBorderSpacing();

    return totalWidth;
}

auto RenderTableCell::computeVisibleRectsInContainer(const RepaintRects& rects, const RenderLayerModelObject* container, const VisibleRectContext& context, VisibleRectState state) const -> std::optional<RepaintRects>
{
    if (container == this)
        return rects;

    auto adjustedRects = rects;
    return RenderBlockFlow::computeVisibleRectsInContainer(adjustedRects, container, context, state);
}

LayoutUnit RenderTableCell::cellBaselinePosition() const
{
    // <http://www.w3.org/TR/2007/CR-CSS21-20070719/tables.html#height-layout>: The baseline of a cell is the baseline of
    // the first in-flow line box in the cell, or the first in-flow table-row in the cell, whichever comes first. If there
    // is no such line box or table-row, the baseline is the bottom of content edge of the cell box.
    if (!isOrthogonal())
        return firstLineBaseline().value_or(borderAndPaddingBefore() + contentBoxLogicalHeight());
    return { };
}

static inline void markCellDirtyWhenCollapsedBorderChanges(RenderTableCell* cell)
{
    if (!cell)
        return;
    cell->setNeedsLayoutAndInvalidateContentLogicalWidths();
}

void RenderTableCell::styleDidChange(Style::Difference diff, const Style::ComputedStyle* oldStyle)
{
    ASSERT(style().display() == Style::DisplayType::TableCell);
    ASSERT(!row() || row()->rowIndexWasSet());

    RenderBlockFlow::styleDidChange(diff, oldStyle);

    if (parent() && section() && oldStyle) {
        auto rowWritingMode = row()->writingMode();
        if (style().logicalHeight(rowWritingMode) != oldStyle->logicalHeight(rowWritingMode))
            section()->rowLogicalHeightChanged(rowIndex());
    }

    // Our intrinsic padding pushes us down to align with the baseline of other cells on the row. If our vertical-align
    // has changed then so will the padding needed to align with other cells - clear it so we can recalculate it from scratch.
    // With 'vertical-align' as the CSS Inline 3 shorthand, what the cell acts on is derived from its longhands.
    auto verticalAlignChanged = [&] {
        auto& settings = document().settings();
        if (settings.cssVerticalAlignShorthandEnabled()) [[unlikely]]
            return style().alignmentBaseline() != oldStyle->alignmentBaseline() || style().baselineShift() != oldStyle->baselineShift();
        return Style::effectiveVerticalAlign(style(), settings) != Style::effectiveVerticalAlign(*oldStyle, settings);
    };
    if (oldStyle && (verticalAlignChanged() || style().alignContent() != oldStyle->alignContent()))
        clearIntrinsicPadding();

    if (CheckedPtr table = this->table(); table && oldStyle) {
        table->invalidateCollapsedBordersAfterStyleChangeIfNeeded(*oldStyle, style(), this);
        if (table->collapseBorders() && diff == Style::DifferenceResult::Layout) {
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
NODELETE static bool compareBorders(const CollapsedBorderValue& border1, const CollapsedBorderValue& border2)
{
    // Sanity check the values passed in. The null border have lowest priority.
    if (!border2.exists())
        return false;
    if (!border1.exists())
        return true;

    // Rule #1 above.
    if (border1.style() == BorderStyle::Hidden)
        return false;
    if (border2.style() == BorderStyle::Hidden)
        return true;
    
    // Rule #2 above.  A style of 'none' has lowest priority and always loses to any other border.
    if (border2.style() == BorderStyle::None)
        return false;
    if (border1.style() == BorderStyle::None)
        return true;

    // The first part of rule #3 above. Wider borders win.
    if (border1.width() != border2.width())
        return border1.width() < border2.width();
    
    // The borders have equal width.  Sort by border style.
    if (border1.style() != border2.style())
        return border1.style() < border2.style();
    
    // The border have the same width and style.  Rely on precedence (cell over row over row group, etc.)
    return border1.precedence() < border2.precedence();
}

NODELETE static CollapsedBorderValue chooseBorder(const CollapsedBorderValue& border1, const CollapsedBorderValue& border2)
{
    const CollapsedBorderValue& border = compareBorders(border1, border2) ? border2 : border1;
    return border.style() == BorderStyle::Hidden ? CollapsedBorderValue() : border;
}

bool RenderTableCell::hasStartBorderAdjoiningTable() const
{
    return !col();
}

bool RenderTableCell::hasEndBorderAdjoiningTable() const
{
    return table()->colToEffCol(col() + colSpan() - 1) == table()->numEffCols() - 1;
}

CollapsedBorderValue RenderTableCell::emptyBorder() const
{
    return CollapsedBorderValue(BorderValue(), Color(), BorderPrecedence::Cell, style().usedZoomForLength(), style().deviceScaleFactor());
}

CollapsedBorderValue RenderTableCell::collapsedStartBorder(IncludeBorderColorOrNot includeColor) const
{
    if (!table() || !section())
        return emptyBorder();

    if (m_hasEmptyCollapsedStartBorder)
        return emptyBorder();

    if (table()->collapsedBordersAreValid())
        return protect(section())->cachedCollapsedBorder(*this, CollapsedBorderSide::Start);

    CollapsedBorderValue result = computeCollapsedStartBorder(includeColor);
    setHasEmptyCollapsedBorder(CollapsedBorderSide::Start, !result.width());
    if (includeColor && !m_hasEmptyCollapsedStartBorder)
        protect(section())->setCachedCollapsedBorder(*this, CollapsedBorderSide::Start, result);
    return result;
}

static Color resolvedBorderColor(const Style::ComputedStyle& style, CSSPropertyID borderColor)
{
    switch (borderColor) {
    case CSSPropertyID::BorderTopColor:
        return style.visitedDependentBorderTopColorApplyingColorFilter();
    case CSSPropertyID::BorderRightColor:
        return style.visitedDependentBorderRightColorApplyingColorFilter();
    case CSSPropertyID::BorderBottomColor:
        return style.visitedDependentBorderBottomColorApplyingColorFilter();
    case CSSPropertyID::BorderLeftColor:
        return style.visitedDependentBorderLeftColorApplyingColorFilter();
    default:
        ASSERT_NOT_REACHED();
        return Color::black;
    }
}

CollapsedBorderValue RenderTableCell::computeCollapsedStartBorder(IncludeBorderColorOrNot includeColor) const
{
    // For the start border, we need to check, in order of precedence:
    // (1) Our start border.
    CSSPropertyID startColorProperty = includeColor ? CSSProperty::resolveDirectionAwareProperty(CSSPropertyID::BorderInlineStartColor, tableWritingMode()) : CSSPropertyID::Invalid;
    CSSPropertyID endColorProperty = includeColor ? CSSProperty::resolveDirectionAwareProperty(CSSPropertyID::BorderInlineEndColor, tableWritingMode()) : CSSPropertyID::Invalid;
    auto deviceScaleFactor = style().deviceScaleFactor();
    CollapsedBorderValue result(style().borderStart(tableWritingMode()), includeColor ? resolvedBorderColor(protect(style()), startColorProperty) : Color(), BorderPrecedence::Cell, style().usedZoomForLength(), deviceScaleFactor);

    CheckedPtr table = this->table();
    if (!table)
        return result;
    // (2) The end border of the preceding cell.
    CheckedPtr cellBefore = table->cellBefore(this);
    if (cellBefore) {
        CollapsedBorderValue cellBeforeAdjoiningBorder = CollapsedBorderValue(cellBefore->borderAdjoiningCellAfter(*this), includeColor ? resolvedBorderColor(protect(cellBefore->style()), endColorProperty) : Color(), BorderPrecedence::Cell, cellBefore->style().usedZoomForLength(), deviceScaleFactor);
        // |result| should be the 2nd argument as |cellBefore| should win in case of equality per CSS 2.1 (Border conflict resolution, point 4).
        result = chooseBorder(cellBeforeAdjoiningBorder, result);
        if (!result.exists())
            return result;
    }

    bool startBorderAdjoinsTable = hasStartBorderAdjoiningTable();
    if (startBorderAdjoinsTable) {
        // (3) Our row's start border.
        result = chooseBorder(result, CollapsedBorderValue(row()->borderAdjoiningStartCell(*this), includeColor ? resolvedBorderColor(protect(parent()->style()), startColorProperty) : Color(), BorderPrecedence::Row, row()->style().usedZoomForLength(), deviceScaleFactor));
        if (!result.exists())
            return result;

        // (4) Our row group's start border.
        result = chooseBorder(result, CollapsedBorderValue(section()->borderAdjoiningStartCell(*this), includeColor ? resolvedBorderColor(protect(section()->style()), startColorProperty) : Color(), BorderPrecedence::RowGroup, section()->style().usedZoomForLength(), deviceScaleFactor));
        if (!result.exists())
            return result;
    }
    
    // (5) Our column and column group's start borders.
    bool startColEdge;
    bool endColEdge;
    if (CheckedPtr colElt = table->colElement(col(), &startColEdge, &endColEdge)) {
        if (colElt->isTableColumnGroup() && startColEdge) {
            // The |colElt| is a column group and is also the first colgroup (in case of spanned colgroups).
            result = chooseBorder(result, CollapsedBorderValue(colElt->borderAdjoiningCellStartBorder(), includeColor ? resolvedBorderColor(protect(colElt->style()), startColorProperty) : Color(), BorderPrecedence::ColumnGroup, colElt->style().usedZoomForLength(), deviceScaleFactor));
            if (!result.exists())
                return result;
        } else if (!colElt->isTableColumnGroup()) {
            // We first consider the |colElt| and irrespective of whether it is a spanned col or not, we apply
            // its start border. This is as per HTML5 which states that: "For the purposes of the CSS table model,
            // the col element is expected to be treated as if it was present as many times as its span attribute specifies".
            result = chooseBorder(result, CollapsedBorderValue(colElt->borderAdjoiningCellStartBorder(), includeColor ? resolvedBorderColor(protect(colElt->style()), startColorProperty) : Color(), BorderPrecedence::Column, colElt->style().usedZoomForLength(), deviceScaleFactor));
            if (!result.exists())
                return result;
            // Next, apply the start border of the enclosing colgroup but only if it is adjacent to the cell's edge.
            if (CheckedPtr enclosingColumnGroup = colElt->enclosingColumnGroupIfAdjacentBefore()) {
                result = chooseBorder(result, CollapsedBorderValue(enclosingColumnGroup->borderAdjoiningCellStartBorder(), includeColor ? resolvedBorderColor(protect(enclosingColumnGroup->style()), startColorProperty) : Color(), BorderPrecedence::ColumnGroup, enclosingColumnGroup->style().usedZoomForLength(), deviceScaleFactor));
                if (!result.exists())
                    return result;
            }
        }
    }
    
    // (6) The end border of the preceding column.
    if (cellBefore) {
        if (CheckedPtr colElt = table->colElement(col() - 1, &startColEdge, &endColEdge)) {
            if (colElt->isTableColumnGroup() && endColEdge) {
                // The element is a colgroup and is also the last colgroup (in case of spanned colgroups).
                result = chooseBorder(CollapsedBorderValue(colElt->borderAdjoiningCellAfter(*this), includeColor ? resolvedBorderColor(protect(colElt->style()), endColorProperty) : Color(), BorderPrecedence::ColumnGroup, colElt->style().usedZoomForLength(), deviceScaleFactor), result);
                if (!result.exists())
                    return result;
            } else if (colElt->isTableColumn()) {
                // Resolve the collapsing border against the col's border ignoring any 'span' as per HTML5.
                result = chooseBorder(CollapsedBorderValue(colElt->borderAdjoiningCellAfter(*this), includeColor ? resolvedBorderColor(protect(colElt->style()), endColorProperty) : Color(), BorderPrecedence::Column, colElt->style().usedZoomForLength(), deviceScaleFactor), result);
                if (!result.exists())
                    return result;
                // Next, if the previous col has a parent colgroup then its end border should be applied
                // but only if it is adjacent to the cell's edge.
                if (CheckedPtr enclosingColumnGroup = colElt->enclosingColumnGroupIfAdjacentAfter()) {
                    result = chooseBorder(CollapsedBorderValue(enclosingColumnGroup->borderAdjoiningCellEndBorder(), includeColor ? resolvedBorderColor(protect(enclosingColumnGroup->style()), endColorProperty) : Color(), BorderPrecedence::ColumnGroup, enclosingColumnGroup->style().usedZoomForLength(), deviceScaleFactor), result);
                    if (!result.exists())
                        return result;
                }
            }
        }
    }

    if (startBorderAdjoinsTable) {
        // (7) The table's start border.
        result = chooseBorder(result, CollapsedBorderValue(table->style().borderStart(), includeColor ? resolvedBorderColor(protect(table->style()), startColorProperty) : Color(), BorderPrecedence::Table, table->style().usedZoomForLength(), deviceScaleFactor));
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
        return protect(section())->cachedCollapsedBorder(*this, CollapsedBorderSide::End);

    CollapsedBorderValue result = computeCollapsedEndBorder(includeColor);
    setHasEmptyCollapsedBorder(CollapsedBorderSide::End, !result.width());
    if (includeColor && !m_hasEmptyCollapsedEndBorder)
        protect(section())->setCachedCollapsedBorder(*this, CollapsedBorderSide::End, result);
    return result;
}

CollapsedBorderValue RenderTableCell::computeCollapsedEndBorder(IncludeBorderColorOrNot includeColor) const
{
    // For end border, we need to check, in order of precedence:
    // (1) Our end border.
    CSSPropertyID startColorProperty = includeColor
        ? CSSProperty::resolveDirectionAwareProperty(CSSPropertyID::BorderInlineStartColor, tableWritingMode()) : CSSPropertyID::Invalid;
    CSSPropertyID endColorProperty = includeColor ? CSSProperty::resolveDirectionAwareProperty(CSSPropertyID::BorderInlineEndColor, tableWritingMode()) : CSSPropertyID::Invalid;
    auto deviceScaleFactor = style().deviceScaleFactor();
    CollapsedBorderValue result = CollapsedBorderValue(style().borderEnd(tableWritingMode()), includeColor ? resolvedBorderColor(protect(style()), endColorProperty) : Color(), BorderPrecedence::Cell, style().usedZoomForLength(), deviceScaleFactor);

    CheckedPtr table = this->table();
    if (!table)
        return result;
    // Note: We have to use the effective column information instead of whether we have a cell after as a table doesn't
    // have to be regular (any row can have less cells than the total cell count).
    bool isEndColumn = table->colToEffCol(col() + colSpan() - 1) == table->numEffCols() - 1;
    // (2) The start border of the following cell.
    if (!isEndColumn) {
        if (CheckedPtr cellAfter = table->cellAfter(this)) {
            CollapsedBorderValue cellAfterAdjoiningBorder = CollapsedBorderValue(cellAfter->borderAdjoiningCellBefore(*this), includeColor ? resolvedBorderColor(protect(cellAfter->style()), startColorProperty) : Color(), BorderPrecedence::Cell, cellAfter->style().usedZoomForLength(), deviceScaleFactor);
            result = chooseBorder(result, cellAfterAdjoiningBorder);
            if (!result.exists())
                return result;
        }
    }

    bool endBorderAdjoinsTable = hasEndBorderAdjoiningTable();
    if (endBorderAdjoinsTable) {
        // (3) Our row's end border.
        result = chooseBorder(result, CollapsedBorderValue(row()->borderAdjoiningEndCell(*this), includeColor ? resolvedBorderColor(protect(parent()->style()), endColorProperty) : Color(), BorderPrecedence::Row, row()->style().usedZoomForLength(), deviceScaleFactor));
        if (!result.exists())
            return result;
        
        // (4) Our row group's end border.
        result = chooseBorder(result, CollapsedBorderValue(section()->borderAdjoiningEndCell(*this), includeColor ? resolvedBorderColor(protect(section()->style()), endColorProperty) : Color(), BorderPrecedence::RowGroup, section()->style().usedZoomForLength(), deviceScaleFactor));
        if (!result.exists())
            return result;
    }
    
    // (5) Our column and column group's end borders.
    bool startColEdge;
    bool endColEdge;
    if (CheckedPtr colElt = table->colElement(col() + colSpan() - 1, &startColEdge, &endColEdge)) {
        if (colElt->isTableColumnGroup() && endColEdge) {
            // The element is a colgroup and is also the last colgroup (in case of spanned colgroups).
            result = chooseBorder(result, CollapsedBorderValue(colElt->borderAdjoiningCellEndBorder(), includeColor ? resolvedBorderColor(protect(colElt->style()), endColorProperty) : Color(), BorderPrecedence::ColumnGroup, colElt->style().usedZoomForLength(), deviceScaleFactor));
            if (!result.exists())
                return result;
        } else if (!colElt->isTableColumnGroup()) {
            // First apply the end border of the column irrespective of whether it is spanned or not. This is as per
            // HTML5 which states that: "For the purposes of the CSS table model, the col element is expected to be
            // treated as if it was present as many times as its span attribute specifies".
            result = chooseBorder(result, CollapsedBorderValue(colElt->borderAdjoiningCellEndBorder(), includeColor ? resolvedBorderColor(protect(colElt->style()), endColorProperty) : Color(), BorderPrecedence::Column, colElt->style().usedZoomForLength(), deviceScaleFactor));
            if (!result.exists())
                return result;
            // Next, if it has a parent colgroup then we apply its end border but only if it is adjacent to the cell.
            if (CheckedPtr enclosingColumnGroup = colElt->enclosingColumnGroupIfAdjacentAfter()) {
                result = chooseBorder(result, CollapsedBorderValue(enclosingColumnGroup->borderAdjoiningCellEndBorder(), includeColor ? resolvedBorderColor(protect(enclosingColumnGroup->style()), endColorProperty) : Color(), BorderPrecedence::ColumnGroup, enclosingColumnGroup->style().usedZoomForLength(), deviceScaleFactor));
                if (!result.exists())
                    return result;
            }
        }
    }
    
    // (6) The start border of the next column.
    if (!isEndColumn) {
        if (CheckedPtr colElt = table->colElement(col() + colSpan(), &startColEdge, &endColEdge)) {
            if (colElt->isTableColumnGroup() && startColEdge) {
                // This case is a colgroup without any col, we only compute it if it is adjacent to the cell's edge.
                result = chooseBorder(result, CollapsedBorderValue(colElt->borderAdjoiningCellBefore(*this), includeColor ? resolvedBorderColor(protect(colElt->style()), startColorProperty) : Color(), BorderPrecedence::ColumnGroup, colElt->style().usedZoomForLength(), deviceScaleFactor));
                if (!result.exists())
                    return result;
            } else if (colElt->isTableColumn()) {
                // Resolve the collapsing border against the col's border ignoring any 'span' as per HTML5.
                result = chooseBorder(result, CollapsedBorderValue(colElt->borderAdjoiningCellBefore(*this), includeColor ? resolvedBorderColor(protect(colElt->style()), startColorProperty) : Color(), BorderPrecedence::Column, colElt->style().usedZoomForLength(), deviceScaleFactor));
                if (!result.exists())
                    return result;
                // If we have a parent colgroup, resolve the border only if it is adjacent to the cell.
                if (CheckedPtr enclosingColumnGroup = colElt->enclosingColumnGroupIfAdjacentBefore()) {
                    result = chooseBorder(result, CollapsedBorderValue(enclosingColumnGroup->borderAdjoiningCellStartBorder(), includeColor ? resolvedBorderColor(protect(enclosingColumnGroup->style()), startColorProperty) : Color(), BorderPrecedence::ColumnGroup, enclosingColumnGroup->style().usedZoomForLength(), deviceScaleFactor));
                    if (!result.exists())
                        return result;
                }
            }
        }
    }

    if (endBorderAdjoinsTable) {
        // (7) The table's end border.
        result = chooseBorder(result, CollapsedBorderValue(table->style().borderEnd(), includeColor ? resolvedBorderColor(protect(table->style()), endColorProperty) : Color(), BorderPrecedence::Table, table->style().usedZoomForLength(), deviceScaleFactor));
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
        return protect(section())->cachedCollapsedBorder(*this, CollapsedBorderSide::Before);

    CollapsedBorderValue result = computeCollapsedBeforeBorder(includeColor);
    setHasEmptyCollapsedBorder(CollapsedBorderSide::Before, !result.width());
    if (includeColor && !m_hasEmptyCollapsedBeforeBorder)
        protect(section())->setCachedCollapsedBorder(*this, CollapsedBorderSide::Before, result);
    return result;
}

CollapsedBorderValue RenderTableCell::computeCollapsedBeforeBorder(IncludeBorderColorOrNot includeColor) const
{
    // For before border, we need to check, in order of precedence:
    // (1) Our before border.
    CSSPropertyID beforeColorProperty = includeColor ? CSSProperty::resolveDirectionAwareProperty(CSSPropertyID::BorderBlockStartColor, tableWritingMode()) : CSSPropertyID::Invalid;
    CSSPropertyID afterColorProperty = includeColor ? CSSProperty::resolveDirectionAwareProperty(CSSPropertyID::BorderBlockEndColor, tableWritingMode()) : CSSPropertyID::Invalid;
    auto deviceScaleFactor = style().deviceScaleFactor();
    CollapsedBorderValue result = CollapsedBorderValue(style().borderBefore(tableWritingMode()), includeColor ? resolvedBorderColor(protect(style()), beforeColorProperty) : Color(), BorderPrecedence::Cell, style().usedZoomForLength(), deviceScaleFactor);

    CheckedPtr table = this->table();
    if (!table)
        return result;
    CheckedPtr previousCell = table->cellAbove(this);
    if (previousCell) {
        // (2) A before cell's after border.
        result = chooseBorder(CollapsedBorderValue(previousCell->style().borderAfter(tableWritingMode()), includeColor ? resolvedBorderColor(protect(previousCell->style()), afterColorProperty) : Color(), BorderPrecedence::Cell, previousCell->style().usedZoomForLength(), deviceScaleFactor), result);
        if (!result.exists())
            return result;
    }
    
    // (3) Our row's before border.
    result = chooseBorder(result, CollapsedBorderValue(parent()->style().borderBefore(tableWritingMode()), includeColor ? resolvedBorderColor(protect(parent()->style()), beforeColorProperty) : Color(), BorderPrecedence::Row, parent()->style().usedZoomForLength(), deviceScaleFactor));
    if (!result.exists())
        return result;
    
    // (4) The previous row's after border.
    if (previousCell) {
        RenderTableRow* previousRow = nullptr;
        if (previousCell->section() == section())
            previousRow = dynamicDowncast<RenderTableRow>(parent()->previousSibling());
        else
            previousRow = previousCell->section()->lastRow();
    
        if (previousRow) {
            result = chooseBorder(CollapsedBorderValue(previousRow->style().borderAfter(tableWritingMode()), includeColor ? resolvedBorderColor(protect(previousRow->style()), afterColorProperty) : Color(), BorderPrecedence::Row, previousRow->style().usedZoomForLength(), deviceScaleFactor), result);
            if (!result.exists())
                return result;
        }
    }
    
    // Now check row groups.
    CheckedPtr currSection = section();
    if (!rowIndex()) {
        // (5) Our row group's before border.
        result = chooseBorder(result, CollapsedBorderValue(currSection->style().borderBefore(tableWritingMode()), includeColor ? resolvedBorderColor(protect(currSection->style()), beforeColorProperty) : Color(), BorderPrecedence::RowGroup, currSection->style().usedZoomForLength(), deviceScaleFactor));
        if (!result.exists())
            return result;
        
        // (6) Previous row group's after border.
        currSection = table->sectionAbove(currSection, SkipEmptySections::Yes);
        if (currSection) {
            result = chooseBorder(CollapsedBorderValue(currSection->style().borderAfter(tableWritingMode()), includeColor ? resolvedBorderColor(protect(currSection->style()), afterColorProperty) : Color(), BorderPrecedence::RowGroup, currSection->style().usedZoomForLength(), deviceScaleFactor), result);
            if (!result.exists())
                return result;
        }
    }
    
    if (!currSection) {
        // (8) Our column and column group's before borders.
        CheckedPtr colElt = table->colElement(col());
        if (colElt) {
            result = chooseBorder(result, CollapsedBorderValue(colElt->style().borderBefore(tableWritingMode()), includeColor ? resolvedBorderColor(protect(colElt->style()), beforeColorProperty) : Color(), BorderPrecedence::Column, colElt->style().usedZoomForLength(), deviceScaleFactor));
            if (!result.exists())
                return result;
            if (CheckedPtr enclosingColumnGroup = colElt->enclosingColumnGroup()) {
                result = chooseBorder(result, CollapsedBorderValue(enclosingColumnGroup->style().borderBefore(tableWritingMode()), includeColor ? resolvedBorderColor(protect(enclosingColumnGroup->style()), beforeColorProperty) : Color(), BorderPrecedence::ColumnGroup, enclosingColumnGroup->style().usedZoomForLength(), deviceScaleFactor));
                if (!result.exists())
                    return result;
            }
        }
        
        // (9) The table's before border.
        result = chooseBorder(result, CollapsedBorderValue(table->style().borderBefore(), includeColor ? resolvedBorderColor(protect(table->style()), beforeColorProperty) : Color(), BorderPrecedence::Table, table->style().usedZoomForLength(), deviceScaleFactor));
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
        return protect(section())->cachedCollapsedBorder(*this, CollapsedBorderSide::After);

    CollapsedBorderValue result = computeCollapsedAfterBorder(includeColor);
    setHasEmptyCollapsedBorder(CollapsedBorderSide::After, !result.width());
    if (includeColor && !m_hasEmptyCollapsedAfterBorder)
        protect(section())->setCachedCollapsedBorder(*this, CollapsedBorderSide::After, result);
    return result;
}

CollapsedBorderValue RenderTableCell::computeCollapsedAfterBorder(IncludeBorderColorOrNot includeColor) const
{
    // For after border, we need to check, in order of precedence:
    // (1) Our after border.
    CSSPropertyID beforeColorProperty = includeColor ? CSSProperty::resolveDirectionAwareProperty(CSSPropertyID::BorderBlockStartColor, tableWritingMode()) : CSSPropertyID::Invalid;
    CSSPropertyID afterColorProperty = includeColor ? CSSProperty::resolveDirectionAwareProperty(CSSPropertyID::BorderBlockEndColor, tableWritingMode()) : CSSPropertyID::Invalid;
    auto deviceScaleFactor = style().deviceScaleFactor();
    CollapsedBorderValue result = CollapsedBorderValue(style().borderAfter(tableWritingMode()), includeColor ? resolvedBorderColor(protect(style()), afterColorProperty) : Color(), BorderPrecedence::Cell, style().usedZoomForLength(), deviceScaleFactor);

    CheckedPtr table = this->table();
    if (!table)
        return result;
    CheckedPtr nextCell = table->cellBelow(this);
    if (nextCell) {
        // (2) An after cell's before border.
        result = chooseBorder(result, CollapsedBorderValue(nextCell->style().borderBefore(tableWritingMode()), includeColor ? resolvedBorderColor(protect(nextCell->style()), beforeColorProperty) : Color(), BorderPrecedence::Cell, nextCell->style().usedZoomForLength(), deviceScaleFactor));
        if (!result.exists())
            return result;
    }
    
    // (3) Our row's after border. Account for rowspan by using the last row in the span.
    // For a cell spanning multiple rows, the after border should match with the border
    // of the last row in the span, not the first row.
    size_t lastRowIndex = rowIndex() + rowSpan() - 1;
    if (CheckedPtr lastRowInSpan = section()->rowRendererAt(lastRowIndex)) {
        result = chooseBorder(result, CollapsedBorderValue(lastRowInSpan->style().borderAfter(tableWritingMode()), includeColor ? resolvedBorderColor(protect(lastRowInSpan->style()), afterColorProperty) : Color(), BorderPrecedence::Row, lastRowInSpan->style().usedZoomForLength(), deviceScaleFactor));
        if (!result.exists())
            return result;
    }

    // (4) The next row's before border.
    if (nextCell) {
        result = chooseBorder(result, CollapsedBorderValue(nextCell->parent()->style().borderBefore(tableWritingMode()), includeColor ? resolvedBorderColor(protect(nextCell->parent()->style()), beforeColorProperty) : Color(), BorderPrecedence::Row, nextCell->parent()->style().usedZoomForLength(), deviceScaleFactor));
        if (!result.exists())
            return result;
    }
    
    // Now check row groups.
    CheckedPtr currSection = section();
    if (rowIndex() + rowSpan() >= currSection->numRows()) {
        // (5) Our row group's after border.
        result = chooseBorder(result, CollapsedBorderValue(currSection->style().borderAfter(tableWritingMode()), includeColor ? resolvedBorderColor(protect(currSection->style()), afterColorProperty) : Color(), BorderPrecedence::RowGroup, currSection->style().usedZoomForLength(), deviceScaleFactor));
        if (!result.exists())
            return result;
        
        // (6) Following row group's before border.
        currSection = table->sectionBelow(currSection, SkipEmptySections::Yes);
        if (currSection) {
            result = chooseBorder(result, CollapsedBorderValue(currSection->style().borderBefore(tableWritingMode()), includeColor ? resolvedBorderColor(protect(currSection->style()), beforeColorProperty) : Color(), BorderPrecedence::RowGroup, currSection->style().usedZoomForLength(), deviceScaleFactor));
            if (!result.exists())
                return result;
        }
    }
    
    if (!currSection) {
        // (8) Our column and column group's after borders.
        CheckedPtr colElt = table->colElement(col());
        if (colElt) {
            result = chooseBorder(result, CollapsedBorderValue(colElt->style().borderAfter(tableWritingMode()), includeColor ? resolvedBorderColor(protect(colElt->style()), afterColorProperty) : Color(), BorderPrecedence::Column, colElt->style().usedZoomForLength(), deviceScaleFactor));
            if (!result.exists()) return result;
            if (CheckedPtr enclosingColumnGroup = colElt->enclosingColumnGroup()) {
                result = chooseBorder(result, CollapsedBorderValue(enclosingColumnGroup->style().borderAfter(tableWritingMode()), includeColor ? resolvedBorderColor(protect(enclosingColumnGroup->style()), afterColorProperty) : Color(), BorderPrecedence::ColumnGroup, enclosingColumnGroup->style().usedZoomForLength(), deviceScaleFactor));
                if (!result.exists())
                    return result;
            }
        }
        
        // (9) The table's after border.
        result = chooseBorder(result, CollapsedBorderValue(table->style().borderAfter(tableWritingMode()), includeColor ? resolvedBorderColor(protect(table->style()), afterColorProperty) : Color(), BorderPrecedence::Table, table->style().usedZoomForLength(), deviceScaleFactor));
        if (!result.exists())
            return result;
    }
    
    return result;    
}

CollapsedBorderValue RenderTableCell::cachedCollapsedLeftBorder(const WritingMode writingMode) const
{
    CheckedPtr section = this->section();
    if (writingMode.isHorizontal())
        return writingMode.isInlineLeftToRight() ? section->cachedCollapsedBorder(*this, CollapsedBorderSide::Start) : section->cachedCollapsedBorder(*this, CollapsedBorderSide::End);
    return writingMode.isBlockLeftToRight() ? section->cachedCollapsedBorder(*this, CollapsedBorderSide::Before) : section->cachedCollapsedBorder(*this, CollapsedBorderSide::After);
}

CollapsedBorderValue RenderTableCell::cachedCollapsedRightBorder(const WritingMode writingMode) const
{
    CheckedPtr section = this->section();
    if (writingMode.isHorizontal())
        return writingMode.isInlineLeftToRight() ? section->cachedCollapsedBorder(*this, CollapsedBorderSide::End) : section->cachedCollapsedBorder(*this, CollapsedBorderSide::Start);
    return writingMode.isBlockLeftToRight() ? section->cachedCollapsedBorder(*this, CollapsedBorderSide::After) : section->cachedCollapsedBorder(*this, CollapsedBorderSide::Before);
}

CollapsedBorderValue RenderTableCell::cachedCollapsedTopBorder(const WritingMode writingMode) const
{
    CheckedPtr section = this->section();
    if (writingMode.isHorizontal())
        return writingMode.isBlockTopToBottom() ? section->cachedCollapsedBorder(*this, CollapsedBorderSide::Before) : section->cachedCollapsedBorder(*this, CollapsedBorderSide::After);
    return writingMode.isInlineTopToBottom() ? section->cachedCollapsedBorder(*this, CollapsedBorderSide::Start) : section->cachedCollapsedBorder(*this, CollapsedBorderSide::End);
}

CollapsedBorderValue RenderTableCell::cachedCollapsedBottomBorder(const WritingMode writingMode) const
{
    CheckedPtr section = this->section();
    if (writingMode.isHorizontal())
        return writingMode.isBlockTopToBottom() ? section->cachedCollapsedBorder(*this, CollapsedBorderSide::After) : section->cachedCollapsedBorder(*this, CollapsedBorderSide::Before);
    return writingMode.isInlineTopToBottom() ? section->cachedCollapsedBorder(*this, CollapsedBorderSide::End) : section->cachedCollapsedBorder(*this, CollapsedBorderSide::Start);
}

RectEdges<LayoutUnit> RenderTableCell::borderWidths() const
{
    RenderTable* table = this->table();
    if (!table)
        return RenderBlockFlow::borderWidths();

    if (!table->collapseBorders())
        return RenderBlockFlow::borderWidths();

    return {
        borderHalfTop(false),
        borderHalfRight(false),
        borderHalfBottom(false),
        borderHalfLeft(false)
    };
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
    WritingMode writingMode = this->tableWritingMode();
    if (writingMode.isHorizontal())
        return writingMode.isInlineLeftToRight() ? borderHalfStart(outer) : borderHalfEnd(outer);
    return writingMode.isBlockLeftToRight() ? borderHalfBefore(outer) : borderHalfAfter(outer);
}

LayoutUnit RenderTableCell::borderHalfRight(bool outer) const
{
    WritingMode writingMode = this->tableWritingMode();
    if (writingMode.isHorizontal())
        return writingMode.isInlineLeftToRight() ? borderHalfEnd(outer) : borderHalfStart(outer);
    return writingMode.isBlockLeftToRight() ? borderHalfAfter(outer) : borderHalfBefore(outer);
}

LayoutUnit RenderTableCell::borderHalfTop(bool outer) const
{
    WritingMode writingMode = this->tableWritingMode();
    if (writingMode.isHorizontal())
        return writingMode.isBlockTopToBottom() ? borderHalfBefore(outer) : borderHalfAfter(outer);
    return writingMode.isInlineTopToBottom() ? borderHalfStart(outer) : borderHalfEnd(outer);
}

LayoutUnit RenderTableCell::borderHalfBottom(bool outer) const
{
    WritingMode writingMode = this->tableWritingMode();
    if (writingMode.isHorizontal())
        return writingMode.isBlockTopToBottom() ? borderHalfAfter(outer) : borderHalfBefore(outer);
    return writingMode.isInlineTopToBottom() ? borderHalfEnd(outer) : borderHalfStart(outer);
}

LayoutUnit RenderTableCell::borderHalfStart(bool outer) const
{
    CollapsedBorderValue border = collapsedStartBorder(DoNotIncludeBorderColor);
    if (border.exists())
        return CollapsedBorderValue::adjustedCollapsedBorderWidth(border.width(), document().deviceScaleFactor(), !(tableWritingMode().isInlineFlipped() ^ outer));
    return 0;
}
    
LayoutUnit RenderTableCell::borderHalfEnd(bool outer) const
{
    CollapsedBorderValue border = collapsedEndBorder(DoNotIncludeBorderColor);
    if (border.exists())
        return CollapsedBorderValue::adjustedCollapsedBorderWidth(border.width(), document().deviceScaleFactor(), tableWritingMode().isInlineFlipped() ^ outer);
    return 0;
}

LayoutUnit RenderTableCell::borderHalfBefore(bool outer) const
{
    CollapsedBorderValue border = collapsedBeforeBorder(DoNotIncludeBorderColor);
    if (border.exists())
        return CollapsedBorderValue::adjustedCollapsedBorderWidth(border.width(), document().deviceScaleFactor(), !(tableWritingMode().isBlockFlipped() ^ outer));
    return 0;
}

LayoutUnit RenderTableCell::borderHalfAfter(bool outer) const
{
    CollapsedBorderValue border = collapsedAfterBorder(DoNotIncludeBorderColor);
    if (border.exists())
        return CollapsedBorderValue::adjustedCollapsedBorderWidth(border.width(), document().deviceScaleFactor(), tableWritingMode().isBlockFlipped() ^ outer);
    return 0;
}

void RenderTableCell::paint(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    ASSERT(paintInfo.phase != PaintPhase::CollapsedTableBorders);
    RenderBlockFlow::paint(paintInfo, paintOffset);
}

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

void RenderTableCell::sortBorderValues(RenderTable::CollapsedBorderValues& borderValues)
{
    std::ranges::sort(borderValues, compareBorders);
}

bool RenderTableCell::isHiddenEmptyCell() const
{
    return !table()->collapseBorders() && style().emptyCells() == EmptyCell::Hide && !firstChild();
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
    if ((WTF::holdsAlternative<CSS::Keyword::Middle>(verticalAlignForTableCell(style(), document().settings())) && style().alignContent().isNormal())
        || style().alignContent().resolve().isCentered()) {
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

bool RenderTableCell::hasLineIfEmpty() const
{
    if (element() && protect(element())->hasEditableStyle())
        return true;

    return RenderBlock::hasLineIfEmpty();
}

} // namespace WebCore
