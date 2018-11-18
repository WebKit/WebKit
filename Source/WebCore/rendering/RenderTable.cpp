/*
 * Copyright (C) 1997 Martin Jones (mjones@kde.org)
 *           (C) 1997 Torben Weis (weis@kde.org)
 *           (C) 1998 Waldo Bastian (bastian@kde.org)
 *           (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2014 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Alexey Proskuryakov (ap@nypop.com)
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
#include "RenderTable.h"

#include "AutoTableLayout.h"
#include "CollapsedBorderValue.h"
#include "Document.h"
#include "FixedTableLayout.h"
#include "FrameView.h"
#include "HitTestResult.h"
#include "HTMLNames.h"
#include "HTMLTableElement.h"
#include "LayoutRepainter.h"
#include "LayoutState.h"
#include "RenderBlockFlow.h"
#include "RenderChildIterator.h"
#include "RenderDescendantIterator.h"
#include "RenderIterator.h"
#include "RenderLayer.h"
#include "RenderTableCaption.h"
#include "RenderTableCell.h"
#include "RenderTableCol.h"
#include "RenderTableSection.h"
#include "RenderTreeBuilder.h"
#include "RenderView.h"
#include "StyleInheritedData.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/SetForScope.h>
#include <wtf/StackStats.h>

namespace WebCore {

using namespace HTMLNames;

WTF_MAKE_ISO_ALLOCATED_IMPL(RenderTable);

RenderTable::RenderTable(Element& element, RenderStyle&& style)
    : RenderBlock(element, WTFMove(style), 0)
    , m_currentBorder(nullptr)
    , m_collapsedBordersValid(false)
    , m_collapsedEmptyBorderIsPresent(false)
    , m_hasColElements(false)
    , m_needsSectionRecalc(false)
    , m_columnLogicalWidthChanged(false)
    , m_columnRenderersValid(false)
    , m_hasCellColspanThatDeterminesTableWidth(false)
    , m_borderStart(0)
    , m_borderEnd(0)
    , m_columnOffsetTop(-1)
    , m_columnOffsetHeight(-1)
{
    setChildrenInline(false);
    m_columnPos.fill(0, 1);
}

RenderTable::RenderTable(Document& document, RenderStyle&& style)
    : RenderBlock(document, WTFMove(style), 0)
    , m_currentBorder(nullptr)
    , m_collapsedBordersValid(false)
    , m_collapsedEmptyBorderIsPresent(false)
    , m_hasColElements(false)
    , m_needsSectionRecalc(false)
    , m_columnLogicalWidthChanged(false)
    , m_columnRenderersValid(false)
    , m_hasCellColspanThatDeterminesTableWidth(false)
    , m_borderStart(0)
    , m_borderEnd(0)
{
    setChildrenInline(false);
    m_columnPos.fill(0, 1);
}

RenderTable::~RenderTable() = default;

void RenderTable::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderBlock::styleDidChange(diff, oldStyle);
    propagateStyleToAnonymousChildren(PropagateToAllChildren);

    auto oldTableLayout = oldStyle ? oldStyle->tableLayout() : TableLayoutType::Auto;

    // In the collapsed border model, there is no cell spacing.
    m_hSpacing = collapseBorders() ? 0 : style().horizontalBorderSpacing();
    m_vSpacing = collapseBorders() ? 0 : style().verticalBorderSpacing();
    m_columnPos[0] = m_hSpacing;

    if (!m_tableLayout || style().tableLayout() != oldTableLayout) {
        // According to the CSS2 spec, you only use fixed table layout if an
        // explicit width is specified on the table.  Auto width implies auto table layout.
        if (style().tableLayout() == TableLayoutType::Fixed && !style().logicalWidth().isAuto())
            m_tableLayout = std::make_unique<FixedTableLayout>(this);
        else
            m_tableLayout = std::make_unique<AutoTableLayout>(this);
    }

    // If border was changed, invalidate collapsed borders cache.
    if (oldStyle && oldStyle->border() != style().border())
        invalidateCollapsedBorders();
}

static inline void resetSectionPointerIfNotBefore(WeakPtr<RenderTableSection>& section, RenderObject* before)
{
    if (!before || !section)
        return;
    auto* previousSibling = before->previousSibling();
    while (previousSibling && previousSibling != section)
        previousSibling = previousSibling->previousSibling();
    if (!previousSibling)
        section.clear();
}

void RenderTable::willInsertTableColumn(RenderTableCol&, RenderObject*)
{
    m_hasColElements = true;
}

void RenderTable::willInsertTableSection(RenderTableSection& child, RenderObject* beforeChild)
{
    switch (child.style().display()) {
    case DisplayType::TableHeaderGroup:
        resetSectionPointerIfNotBefore(m_head, beforeChild);
        if (!m_head)
            m_head = makeWeakPtr(child);
        else {
            resetSectionPointerIfNotBefore(m_firstBody, beforeChild);
            if (!m_firstBody)
                m_firstBody = makeWeakPtr(child);
        }
        break;
    case DisplayType::TableFooterGroup:
        resetSectionPointerIfNotBefore(m_foot, beforeChild);
        if (!m_foot) {
            m_foot = makeWeakPtr(child);
            break;
        }
        FALLTHROUGH;
    case DisplayType::TableRowGroup:
        resetSectionPointerIfNotBefore(m_firstBody, beforeChild);
        if (!m_firstBody)
            m_firstBody = makeWeakPtr(child);
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    setNeedsSectionRecalc();
}

void RenderTable::addCaption(RenderTableCaption& caption)
{
    ASSERT(m_captions.find(&caption) == notFound);
    m_captions.append(makeWeakPtr(caption));
}

void RenderTable::removeCaption(RenderTableCaption& oldCaption)
{
    bool removed = m_captions.removeFirst(&oldCaption);
    ASSERT_UNUSED(removed, removed);
}

void RenderTable::invalidateCachedColumns()
{
    m_columnRenderersValid = false;
    m_columnRenderers.shrink(0);
    m_effectiveColumnIndexMap.clear();
}

void RenderTable::invalidateCachedColumnOffsets()
{
    m_columnOffsetTop = -1;
    m_columnOffsetHeight = -1;
}

void RenderTable::addColumn(const RenderTableCol*)
{
    invalidateCachedColumns();
}

void RenderTable::removeColumn(const RenderTableCol*)
{
    invalidateCachedColumns();
    // We don't really need to recompute our sections, but we need to update our
    // column count and whether we have a column. Currently, we only have one
    // size-fit-all flag but we may have to consider splitting it.
    setNeedsSectionRecalc();
}

void RenderTable::updateLogicalWidth()
{
    recalcSectionsIfNeeded();

    if (isOutOfFlowPositioned()) {
        LogicalExtentComputedValues computedValues;
        computePositionedLogicalWidth(computedValues);
        setLogicalWidth(computedValues.m_extent);
        setLogicalLeft(computedValues.m_position);
        setMarginStart(computedValues.m_margins.m_start);
        setMarginEnd(computedValues.m_margins.m_end);
    }

    RenderBlock& cb = *containingBlock();

    LayoutUnit availableLogicalWidth = containingBlockLogicalWidthForContent();
    bool hasPerpendicularContainingBlock = cb.style().isHorizontalWritingMode() != style().isHorizontalWritingMode();
    LayoutUnit containerWidthInInlineDirection = hasPerpendicularContainingBlock ? perpendicularContainingBlockLogicalHeight() : availableLogicalWidth;

    Length styleLogicalWidth = style().logicalWidth();
    if ((styleLogicalWidth.isSpecified() && styleLogicalWidth.isPositive()) || styleLogicalWidth.isIntrinsic())
        setLogicalWidth(convertStyleLogicalWidthToComputedWidth(styleLogicalWidth, containerWidthInInlineDirection));
    else {
        // Subtract out any fixed margins from our available width for auto width tables.
        LayoutUnit marginStart = minimumValueForLength(style().marginStart(), availableLogicalWidth);
        LayoutUnit marginEnd = minimumValueForLength(style().marginEnd(), availableLogicalWidth);
        LayoutUnit marginTotal = marginStart + marginEnd;

        // Subtract out our margins to get the available content width.
        LayoutUnit availableContentLogicalWidth = std::max<LayoutUnit>(0, containerWidthInInlineDirection - marginTotal);
        if (shrinkToAvoidFloats() && cb.containsFloats() && !hasPerpendicularContainingBlock) {
            // FIXME: Work with regions someday.
            availableContentLogicalWidth = shrinkLogicalWidthToAvoidFloats(marginStart, marginEnd, cb, 0);
        }

        // Ensure we aren't bigger than our available width.
        setLogicalWidth(std::min(availableContentLogicalWidth, maxPreferredLogicalWidth()));
        LayoutUnit maxWidth = maxPreferredLogicalWidth();
        // scaledWidthFromPercentColumns depends on m_layoutStruct in TableLayoutAlgorithmAuto, which
        // maxPreferredLogicalWidth fills in. So scaledWidthFromPercentColumns has to be called after
        // maxPreferredLogicalWidth.
        LayoutUnit scaledWidth = m_tableLayout->scaledWidthFromPercentColumns() + bordersPaddingAndSpacingInRowDirection();
        maxWidth = std::max(scaledWidth, maxWidth);
        setLogicalWidth(std::min(availableContentLogicalWidth, maxWidth));
    }

    // Ensure we aren't smaller than our min preferred width.
    setLogicalWidth(std::max(logicalWidth(), minPreferredLogicalWidth()));

    
    // Ensure we aren't bigger than our max-width style.
    Length styleMaxLogicalWidth = style().logicalMaxWidth();
    if ((styleMaxLogicalWidth.isSpecified() && !styleMaxLogicalWidth.isNegative()) || styleMaxLogicalWidth.isIntrinsic()) {
        LayoutUnit computedMaxLogicalWidth = convertStyleLogicalWidthToComputedWidth(styleMaxLogicalWidth, availableLogicalWidth);
        setLogicalWidth(std::min(logicalWidth(), computedMaxLogicalWidth));
    }

    // Ensure we aren't smaller than our min-width style.
    Length styleMinLogicalWidth = style().logicalMinWidth();
    if ((styleMinLogicalWidth.isSpecified() && !styleMinLogicalWidth.isNegative()) || styleMinLogicalWidth.isIntrinsic()) {
        LayoutUnit computedMinLogicalWidth = convertStyleLogicalWidthToComputedWidth(styleMinLogicalWidth, availableLogicalWidth);
        setLogicalWidth(std::max(logicalWidth(), computedMinLogicalWidth));
    }

    // Finally, with our true width determined, compute our margins for real.
    setMarginStart(0);
    setMarginEnd(0);
    if (!hasPerpendicularContainingBlock) {
        LayoutUnit containerLogicalWidthForAutoMargins = availableLogicalWidth;
        if (avoidsFloats() && cb.containsFloats())
            containerLogicalWidthForAutoMargins = containingBlockAvailableLineWidthInFragment(0); // FIXME: Work with regions someday.
        ComputedMarginValues marginValues;
        bool hasInvertedDirection =  cb.style().isLeftToRightDirection() == style().isLeftToRightDirection();
        computeInlineDirectionMargins(cb, containerLogicalWidthForAutoMargins, logicalWidth(),
            hasInvertedDirection ? marginValues.m_start : marginValues.m_end,
            hasInvertedDirection ? marginValues.m_end : marginValues.m_start);
        setMarginStart(marginValues.m_start);
        setMarginEnd(marginValues.m_end);
    } else {
        setMarginStart(minimumValueForLength(style().marginStart(), availableLogicalWidth));
        setMarginEnd(minimumValueForLength(style().marginEnd(), availableLogicalWidth));
    }
}

// This method takes a RenderStyle's logical width, min-width, or max-width length and computes its actual value.
LayoutUnit RenderTable::convertStyleLogicalWidthToComputedWidth(const Length& styleLogicalWidth, LayoutUnit availableWidth)
{
    if (styleLogicalWidth.isIntrinsic())
        return computeIntrinsicLogicalWidthUsing(styleLogicalWidth, availableWidth, bordersPaddingAndSpacingInRowDirection());

    // HTML tables' width styles already include borders and paddings, but CSS tables' width styles do not.
    LayoutUnit borders;
    bool isCSSTable = !is<HTMLTableElement>(element());
    if (isCSSTable && styleLogicalWidth.isSpecified() && styleLogicalWidth.isPositive() && style().boxSizing() == BoxSizing::ContentBox)
        borders = borderStart() + borderEnd() + (collapseBorders() ? LayoutUnit() : paddingStart() + paddingEnd());

    return minimumValueForLength(styleLogicalWidth, availableWidth) + borders;
}

LayoutUnit RenderTable::convertStyleLogicalHeightToComputedHeight(const Length& styleLogicalHeight)
{
    LayoutUnit borderAndPaddingBefore = borderBefore() + (collapseBorders() ? LayoutUnit() : paddingBefore());
    LayoutUnit borderAndPaddingAfter = borderAfter() + (collapseBorders() ? LayoutUnit() : paddingAfter());
    LayoutUnit borderAndPadding = borderAndPaddingBefore + borderAndPaddingAfter;
    if (styleLogicalHeight.isFixed()) {
        // HTML tables size as though CSS height includes border/padding, CSS tables do not.
        LayoutUnit borders = LayoutUnit();
        // FIXME: We cannot apply box-sizing: content-box on <table> which other browsers allow.
        if (is<HTMLTableElement>(element()) || style().boxSizing() == BoxSizing::BorderBox) {
            borders = borderAndPadding;
        }
        return styleLogicalHeight.value() - borders;
    } else if (styleLogicalHeight.isPercentOrCalculated())
        return computePercentageLogicalHeight(styleLogicalHeight).value_or(0);
    else if (styleLogicalHeight.isIntrinsic())
        return computeIntrinsicLogicalContentHeightUsing(styleLogicalHeight, logicalHeight() - borderAndPadding, borderAndPadding).value_or(0);
    else
        ASSERT_NOT_REACHED();
    return LayoutUnit();
}

void RenderTable::layoutCaption(RenderTableCaption& caption)
{
    LayoutRect captionRect(caption.frameRect());

    if (caption.needsLayout()) {
        // The margins may not be available but ensure the caption is at least located beneath any previous sibling caption
        // so that it does not mistakenly think any floats in the previous caption intrude into it.
        caption.setLogicalLocation(LayoutPoint(caption.marginStart(), caption.marginBefore() + logicalHeight()));
        // If RenderTableCaption ever gets a layout() function, use it here.
        caption.layoutIfNeeded();
    }
    // Apply the margins to the location now that they are definitely available from layout
    caption.setLogicalLocation(LayoutPoint(caption.marginStart(), caption.marginBefore() + logicalHeight()));

    if (!selfNeedsLayout() && caption.checkForRepaintDuringLayout())
        caption.repaintDuringLayoutIfMoved(captionRect);

    setLogicalHeight(logicalHeight() + caption.logicalHeight() + caption.marginBefore() + caption.marginAfter());
}

void RenderTable::layoutCaptions(BottomCaptionLayoutPhase bottomCaptionLayoutPhase)
{
    if (m_captions.isEmpty())
        return;
    // FIXME: Collapse caption margin.
    for (unsigned i = 0; i < m_captions.size(); ++i) {
        if ((bottomCaptionLayoutPhase == BottomCaptionLayoutPhase::Yes && m_captions[i]->style().captionSide() != CaptionSide::Bottom)
            || (bottomCaptionLayoutPhase == BottomCaptionLayoutPhase::No && m_captions[i]->style().captionSide() == CaptionSide::Bottom))
            continue;
        layoutCaption(*m_captions[i]);
    }
}

void RenderTable::distributeExtraLogicalHeight(LayoutUnit extraLogicalHeight)
{
    if (extraLogicalHeight <= 0)
        return;

    // FIXME: Distribute the extra logical height between all table sections instead of giving it all to the first one.
    if (RenderTableSection* section = firstBody())
        extraLogicalHeight -= section->distributeExtraLogicalHeightToRows(extraLogicalHeight);

    // FIXME: We really would like to enable this ASSERT to ensure that all the extra space has been distributed.
    // However our current distribution algorithm does not round properly and thus we can have some remaining height.
    // ASSERT(!topSection() || !extraLogicalHeight);
}

void RenderTable::simplifiedNormalFlowLayout()
{
    layoutCaptions();
    for (RenderTableSection* section = topSection(); section; section = sectionBelow(section)) {
        section->layoutIfNeeded();
        section->computeOverflowFromCells();
    }
    layoutCaptions(BottomCaptionLayoutPhase::Yes);
}

void RenderTable::layout()
{
    StackStats::LayoutCheckPoint layoutCheckPoint;
    ASSERT(needsLayout());

    if (simplifiedLayout())
        return;

    recalcSectionsIfNeeded();
    // FIXME: We should do this recalc lazily in borderStart/borderEnd so that we don't have to make sure
    // to call this before we call borderStart/borderEnd to avoid getting a stale value.
    recalcBordersInRowDirection();
    bool sectionMoved = false;
    LayoutUnit movedSectionLogicalTop;

    LayoutRepainter repainter(*this, checkForRepaintDuringLayout());
    {
        LayoutStateMaintainer statePusher(*this, locationOffset(), hasTransform() || hasReflection() || style().isFlippedBlocksWritingMode());

        LayoutUnit oldLogicalWidth = logicalWidth();
        LayoutUnit oldLogicalHeight = logicalHeight();
        setLogicalHeight(0);
        updateLogicalWidth();

        if (logicalWidth() != oldLogicalWidth) {
            for (unsigned i = 0; i < m_captions.size(); i++)
                m_captions[i]->setNeedsLayout(MarkOnlyThis);
        }
        // FIXME: The optimisation below doesn't work since the internal table
        // layout could have changed. We need to add a flag to the table
        // layout that tells us if something has changed in the min max
        // calculations to do it correctly.
        //     if ( oldWidth != width() || columns.size() + 1 != columnPos.size() )
        m_tableLayout->layout();

        LayoutUnit totalSectionLogicalHeight;
        LayoutUnit oldTableLogicalTop;
        for (unsigned i = 0; i < m_captions.size(); i++) {
            if (m_captions[i]->style().captionSide() == CaptionSide::Bottom)
                continue;
            oldTableLogicalTop += m_captions[i]->logicalHeight() + m_captions[i]->marginBefore() + m_captions[i]->marginAfter();
        }

        bool collapsing = collapseBorders();

        for (auto& child : childrenOfType<RenderElement>(*this)) {
            if (is<RenderTableSection>(child)) {
                RenderTableSection& section = downcast<RenderTableSection>(child);
                if (m_columnLogicalWidthChanged)
                    section.setChildNeedsLayout(MarkOnlyThis);
                section.layoutIfNeeded();
                totalSectionLogicalHeight += section.calcRowLogicalHeight();
                if (collapsing)
                    section.recalcOuterBorder();
                ASSERT(!section.needsLayout());
            } else if (is<RenderTableCol>(child)) {
                downcast<RenderTableCol>(child).layoutIfNeeded();
                ASSERT(!child.needsLayout());
            }
        }

        // If any table section moved vertically, we will just repaint everything from that
        // section down (it is quite unlikely that any of the following sections
        // did not shift).
        layoutCaptions();
        if (!m_captions.isEmpty() && logicalHeight() != oldTableLogicalTop) {
            sectionMoved = true;
            movedSectionLogicalTop = std::min(logicalHeight(), oldTableLogicalTop);
        }

        LayoutUnit borderAndPaddingBefore = borderBefore() + (collapsing ? LayoutUnit() : paddingBefore());
        LayoutUnit borderAndPaddingAfter = borderAfter() + (collapsing ? LayoutUnit() : paddingAfter());

        setLogicalHeight(logicalHeight() + borderAndPaddingBefore);

        if (!isOutOfFlowPositioned())
            updateLogicalHeight();

        LayoutUnit computedLogicalHeight;
    
        Length logicalHeightLength = style().logicalHeight();
        if (logicalHeightLength.isIntrinsic() || (logicalHeightLength.isSpecified() && logicalHeightLength.isPositive()))
            computedLogicalHeight = convertStyleLogicalHeightToComputedHeight(logicalHeightLength);

        Length logicalMaxHeightLength = style().logicalMaxHeight();
        if (logicalMaxHeightLength.isIntrinsic() || (logicalMaxHeightLength.isSpecified() && !logicalMaxHeightLength.isNegative())) {
            LayoutUnit computedMaxLogicalHeight = convertStyleLogicalHeightToComputedHeight(logicalMaxHeightLength);
            computedLogicalHeight = std::min(computedLogicalHeight, computedMaxLogicalHeight);
        }

        Length logicalMinHeightLength = style().logicalMinHeight();
        if (logicalMinHeightLength.isIntrinsic() || (logicalMinHeightLength.isSpecified() && !logicalMinHeightLength.isNegative())) {
            LayoutUnit computedMinLogicalHeight = convertStyleLogicalHeightToComputedHeight(logicalMinHeightLength);
            computedLogicalHeight = std::max(computedLogicalHeight, computedMinLogicalHeight);
        }

        distributeExtraLogicalHeight(computedLogicalHeight - totalSectionLogicalHeight);

        for (RenderTableSection* section = topSection(); section; section = sectionBelow(section))
            section->layoutRows();

        if (!topSection() && computedLogicalHeight > totalSectionLogicalHeight && !document().inQuirksMode()) {
            // Completely empty tables (with no sections or anything) should at least honor specified height
            // in strict mode.
            setLogicalHeight(logicalHeight() + computedLogicalHeight);
        }

        LayoutUnit sectionLogicalLeft = style().isLeftToRightDirection() ? borderStart() : borderEnd();
        if (!collapsing)
            sectionLogicalLeft += style().isLeftToRightDirection() ? paddingStart() : paddingEnd();

        // position the table sections
        RenderTableSection* section = topSection();
        while (section) {
            if (!sectionMoved && section->logicalTop() != logicalHeight()) {
                sectionMoved = true;
                movedSectionLogicalTop = std::min(logicalHeight(), section->logicalTop()) + (style().isHorizontalWritingMode() ? section->visualOverflowRect().y() : section->visualOverflowRect().x());
            }
            section->setLogicalLocation(LayoutPoint(sectionLogicalLeft, logicalHeight()));

            setLogicalHeight(logicalHeight() + section->logicalHeight());
            section = sectionBelow(section);
        }

        setLogicalHeight(logicalHeight() + borderAndPaddingAfter);

        layoutCaptions(BottomCaptionLayoutPhase::Yes);

        if (isOutOfFlowPositioned())
            updateLogicalHeight();

        // table can be containing block of positioned elements.
        bool dimensionChanged = oldLogicalWidth != logicalWidth() || oldLogicalHeight != logicalHeight();
        layoutPositionedObjects(dimensionChanged);

        updateLayerTransform();

        // Layout was changed, so probably borders too.
        invalidateCollapsedBorders();

        // The location or height of one or more sections may have changed.
        invalidateCachedColumnOffsets();

        computeOverflow(clientLogicalBottom());
    }

    auto* layoutState = view().frameView().layoutContext().layoutState();
    if (layoutState->pageLogicalHeight())
        setPageLogicalOffset(layoutState->pageLogicalOffset(this, logicalTop()));

    bool didFullRepaint = repainter.repaintAfterLayout();
    // Repaint with our new bounds if they are different from our old bounds.
    if (!didFullRepaint && sectionMoved) {
        if (style().isHorizontalWritingMode())
            repaintRectangle(LayoutRect(visualOverflowRect().x(), movedSectionLogicalTop, visualOverflowRect().width(), visualOverflowRect().maxY() - movedSectionLogicalTop));
        else
            repaintRectangle(LayoutRect(movedSectionLogicalTop, visualOverflowRect().y(), visualOverflowRect().maxX() - movedSectionLogicalTop, visualOverflowRect().height()));
    }

    bool paginated = layoutState && layoutState->isPaginated();
    if (sectionMoved && paginated) {
        // FIXME: Table layout should always stabilize even when section moves (see webkit.org/b/174412).
        if (!m_inRecursiveSectionMovedWithPagination) {
            SetForScope<bool> paginatedSectionMoved(m_inRecursiveSectionMovedWithPagination, true);
            markForPaginationRelayoutIfNeeded();
            layoutIfNeeded();
        } else
            ASSERT_NOT_REACHED();
    }
    
    // FIXME: This value isn't the intrinsic content logical height, but we need
    // to update the value as its used by flexbox layout. crbug.com/367324
    cacheIntrinsicContentLogicalHeightForFlexItem(contentLogicalHeight());
    
    m_columnLogicalWidthChanged = false;
    clearNeedsLayout();
}

void RenderTable::invalidateCollapsedBorders(RenderTableCell* cellWithStyleChange)
{
    m_collapsedBordersValid = false;
    m_collapsedBorders.clear();

    for (auto& section : childrenOfType<RenderTableSection>(*this))
        section.clearCachedCollapsedBorders();

    if (!m_collapsedEmptyBorderIsPresent)
        return;

    if (cellWithStyleChange) {
        // It is enough to invalidate just the surrounding cells when cell border style changes.
        cellWithStyleChange->invalidateHasEmptyCollapsedBorders();
        if (auto* below = cellBelow(cellWithStyleChange))
            below->invalidateHasEmptyCollapsedBorders();
        if (auto* above = cellAbove(cellWithStyleChange))
            above->invalidateHasEmptyCollapsedBorders();
        if (auto* before = cellBefore(cellWithStyleChange))
            before->invalidateHasEmptyCollapsedBorders();
        if (auto* after = cellAfter(cellWithStyleChange))
            after->invalidateHasEmptyCollapsedBorders();
        return;
    }

    for (auto& section : childrenOfType<RenderTableSection>(*this)) {
        for (auto* row = section.firstRow(); row; row = row->nextRow()) {
            for (auto* cell = row->firstCell(); cell; cell = cell->nextCell()) {
                ASSERT(cell->table() == this);
                cell->invalidateHasEmptyCollapsedBorders();
            }
        }
    }
    m_collapsedEmptyBorderIsPresent = false;
}

// Collect all the unique border values that we want to paint in a sorted list.
void RenderTable::recalcCollapsedBorders()
{
    if (m_collapsedBordersValid)
        return;
    m_collapsedBorders.clear();
    for (auto& section : childrenOfType<RenderTableSection>(*this)) {
        for (RenderTableRow* row = section.firstRow(); row; row = row->nextRow()) {
            for (RenderTableCell* cell = row->firstCell(); cell; cell = cell->nextCell()) {
                ASSERT(cell->table() == this);
                cell->collectBorderValues(m_collapsedBorders);
            }
        }
    }
    RenderTableCell::sortBorderValues(m_collapsedBorders);
    m_collapsedBordersValid = true;
}

void RenderTable::addOverflowFromChildren()
{
    // Add overflow from borders.
    // Technically it's odd that we are incorporating the borders into layout overflow, which is only supposed to be about overflow from our
    // descendant objects, but since tables don't support overflow:auto, this works out fine.
    if (collapseBorders()) {
        LayoutUnit rightBorderOverflow = width() + outerBorderRight() - borderRight();
        LayoutUnit leftBorderOverflow = borderLeft() - outerBorderLeft();
        LayoutUnit bottomBorderOverflow = height() + outerBorderBottom() - borderBottom();
        LayoutUnit topBorderOverflow = borderTop() - outerBorderTop();
        LayoutRect borderOverflowRect(leftBorderOverflow, topBorderOverflow, rightBorderOverflow - leftBorderOverflow, bottomBorderOverflow - topBorderOverflow);
        if (borderOverflowRect != borderBoxRect()) {
            addLayoutOverflow(borderOverflowRect);
            addVisualOverflow(borderOverflowRect);
        }
    }

    // Add overflow from our caption.
    for (unsigned i = 0; i < m_captions.size(); i++) 
        addOverflowFromChild(m_captions[i].get());

    // Add overflow from our sections.
    for (RenderTableSection* section = topSection(); section; section = sectionBelow(section))
        addOverflowFromChild(section);
}

void RenderTable::paint(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    LayoutPoint adjustedPaintOffset = paintOffset + location();

    PaintPhase paintPhase = paintInfo.phase;

    if (!isDocumentElementRenderer()) {
        LayoutRect overflowBox = visualOverflowRect();
        flipForWritingMode(overflowBox);
        overflowBox.moveBy(adjustedPaintOffset);
        if (!overflowBox.intersects(paintInfo.rect))
            return;
    }

    bool pushedClip = pushContentsClip(paintInfo, adjustedPaintOffset);
    paintObject(paintInfo, adjustedPaintOffset);
    if (pushedClip)
        popContentsClip(paintInfo, paintPhase, adjustedPaintOffset);
}

void RenderTable::paintObject(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    PaintPhase paintPhase = paintInfo.phase;
    if ((paintPhase == PaintPhase::BlockBackground || paintPhase == PaintPhase::ChildBlockBackground) && hasVisibleBoxDecorations() && style().visibility() == Visibility::Visible)
        paintBoxDecorations(paintInfo, paintOffset);

    if (paintPhase == PaintPhase::Mask) {
        paintMask(paintInfo, paintOffset);
        return;
    }

    // We're done.  We don't bother painting any children.
    if (paintPhase == PaintPhase::BlockBackground)
        return;
    
    // We don't paint our own background, but we do let the kids paint their backgrounds.
    if (paintPhase == PaintPhase::ChildBlockBackgrounds)
        paintPhase = PaintPhase::ChildBlockBackground;

    PaintInfo info(paintInfo);
    info.phase = paintPhase;
    info.updateSubtreePaintRootForChildren(this);

    for (auto& box : childrenOfType<RenderBox>(*this)) {
        if (!box.hasSelfPaintingLayer() && (box.isTableSection() || box.isTableCaption())) {
            LayoutPoint childPoint = flipForWritingModeForChild(&box, paintOffset);
            box.paint(info, childPoint);
        }
    }
    
    if (collapseBorders() && paintPhase == PaintPhase::ChildBlockBackground && style().visibility() == Visibility::Visible) {
        recalcCollapsedBorders();
        // Using our cached sorted styles, we then do individual passes,
        // painting each style of border from lowest precedence to highest precedence.
        info.phase = PaintPhase::CollapsedTableBorders;
        size_t count = m_collapsedBorders.size();
        for (size_t i = 0; i < count; ++i) {
            m_currentBorder = &m_collapsedBorders[i];
            for (RenderTableSection* section = bottomSection(); section; section = sectionAbove(section)) {
                LayoutPoint childPoint = flipForWritingModeForChild(section, paintOffset);
                section->paint(info, childPoint);
            }
        }
        m_currentBorder = 0;
    }

    // Paint outline.
    if ((paintPhase == PaintPhase::Outline || paintPhase == PaintPhase::SelfOutline) && hasOutline() && style().visibility() == Visibility::Visible)
        paintOutline(paintInfo, LayoutRect(paintOffset, size()));
}

void RenderTable::adjustBorderBoxRectForPainting(LayoutRect& rect)
{
    for (unsigned i = 0; i < m_captions.size(); i++) {
        LayoutUnit captionLogicalHeight = m_captions[i]->logicalHeight() + m_captions[i]->marginBefore() + m_captions[i]->marginAfter();
        bool captionIsBefore = (m_captions[i]->style().captionSide() != CaptionSide::Bottom) ^ style().isFlippedBlocksWritingMode();
        if (style().isHorizontalWritingMode()) {
            rect.setHeight(rect.height() - captionLogicalHeight);
            if (captionIsBefore)
                rect.move(0, captionLogicalHeight);
        } else {
            rect.setWidth(rect.width() - captionLogicalHeight);
            if (captionIsBefore)
                rect.move(captionLogicalHeight, 0);
        }
    }
    
    RenderBlock::adjustBorderBoxRectForPainting(rect);
}

void RenderTable::paintBoxDecorations(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (!paintInfo.shouldPaintWithinRoot(*this))
        return;

    LayoutRect rect(paintOffset, size());
    adjustBorderBoxRectForPainting(rect);
    
    BackgroundBleedAvoidance bleedAvoidance = determineBackgroundBleedAvoidance(paintInfo.context());
    if (!boxShadowShouldBeAppliedToBackground(rect.location(), bleedAvoidance))
        paintBoxShadow(paintInfo, rect, style(), Normal);
    paintBackground(paintInfo, rect, bleedAvoidance);
    paintBoxShadow(paintInfo, rect, style(), Inset);

    if (style().hasVisibleBorderDecoration() && !collapseBorders())
        paintBorder(paintInfo, rect, style());
}

void RenderTable::paintMask(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (style().visibility() != Visibility::Visible || paintInfo.phase != PaintPhase::Mask)
        return;

    LayoutRect rect(paintOffset, size());
    adjustBorderBoxRectForPainting(rect);

    paintMaskImages(paintInfo, rect);
}

void RenderTable::computeIntrinsicLogicalWidths(LayoutUnit& minWidth, LayoutUnit& maxWidth) const
{
    recalcSectionsIfNeeded();
    // FIXME: Do the recalc in borderStart/borderEnd and make those const_cast this call.
    // Then m_borderStart/m_borderEnd will be transparent a cache and it removes the possibility
    // of reading out stale values.
    const_cast<RenderTable*>(this)->recalcBordersInRowDirection();
    // FIXME: Restructure the table layout code so that we can make this method const.
    const_cast<RenderTable*>(this)->m_tableLayout->computeIntrinsicLogicalWidths(minWidth, maxWidth);

    // FIXME: We should include captions widths here like we do in computePreferredLogicalWidths.
}

void RenderTable::computePreferredLogicalWidths()
{
    ASSERT(preferredLogicalWidthsDirty());

    computeIntrinsicLogicalWidths(m_minPreferredLogicalWidth, m_maxPreferredLogicalWidth);

    LayoutUnit bordersPaddingAndSpacing = bordersPaddingAndSpacingInRowDirection();
    m_minPreferredLogicalWidth += bordersPaddingAndSpacing;
    m_maxPreferredLogicalWidth += bordersPaddingAndSpacing;

    m_tableLayout->applyPreferredLogicalWidthQuirks(m_minPreferredLogicalWidth, m_maxPreferredLogicalWidth);

    for (unsigned i = 0; i < m_captions.size(); i++)
        m_minPreferredLogicalWidth = std::max(m_minPreferredLogicalWidth, m_captions[i]->minPreferredLogicalWidth());

    auto& styleToUse = style();
    // FIXME: This should probably be checking for isSpecified since you should be able to use percentage or calc values for min-width.
    if (styleToUse.logicalMinWidth().isFixed() && styleToUse.logicalMinWidth().value() > 0) {
        m_maxPreferredLogicalWidth = std::max(m_maxPreferredLogicalWidth, adjustContentBoxLogicalWidthForBoxSizing(styleToUse.logicalMinWidth().value()));
        m_minPreferredLogicalWidth = std::max(m_minPreferredLogicalWidth, adjustContentBoxLogicalWidthForBoxSizing(styleToUse.logicalMinWidth().value()));
    }

    // FIXME: This should probably be checking for isSpecified since you should be able to use percentage or calc values for maxWidth.
    if (styleToUse.logicalMaxWidth().isFixed()) {
        m_maxPreferredLogicalWidth = std::min(m_maxPreferredLogicalWidth, adjustContentBoxLogicalWidthForBoxSizing(styleToUse.logicalMaxWidth().value()));
        m_minPreferredLogicalWidth = std::min(m_minPreferredLogicalWidth, adjustContentBoxLogicalWidthForBoxSizing(styleToUse.logicalMaxWidth().value()));
    }

    // FIXME: We should be adding borderAndPaddingLogicalWidth here, but m_tableLayout->computePreferredLogicalWidths already does,
    // so a bunch of tests break doing this naively.
    setPreferredLogicalWidthsDirty(false);
}

RenderTableSection* RenderTable::topNonEmptySection() const
{
    RenderTableSection* section = topSection();
    if (section && !section->numRows())
        section = sectionBelow(section, SkipEmptySections);
    return section;
}

void RenderTable::splitColumn(unsigned position, unsigned firstSpan)
{
    // We split the column at "position", taking "firstSpan" cells from the span.
    ASSERT(m_columns[position].span > firstSpan);
    m_columns.insert(position, ColumnStruct(firstSpan));
    m_columns[position + 1].span -= firstSpan;

    // Propagate the change in our columns representation to the sections that don't need
    // cell recalc. If they do, they will be synced up directly with m_columns later.
    for (auto& section : childrenOfType<RenderTableSection>(*this)) {
        if (section.needsCellRecalc())
            continue;

        section.splitColumn(position, firstSpan);
    }

    m_columnPos.grow(numEffCols() + 1);
}

void RenderTable::appendColumn(unsigned span)
{
    unsigned newColumnIndex = m_columns.size();
    m_columns.append(ColumnStruct(span));

    // Unless the table has cell(s) with colspan that exceed the number of columns afforded
    // by the other rows in the table we can use the fast path when mapping columns to effective columns.
    m_hasCellColspanThatDeterminesTableWidth = m_hasCellColspanThatDeterminesTableWidth || span > 1;

    // Propagate the change in our columns representation to the sections that don't need
    // cell recalc. If they do, they will be synced up directly with m_columns later.
    for (auto& section : childrenOfType<RenderTableSection>(*this)) {
        if (section.needsCellRecalc())
            continue;

        section.appendColumn(newColumnIndex);
    }

    m_columnPos.grow(numEffCols() + 1);
}

RenderTableCol* RenderTable::firstColumn() const
{
    for (auto& child : childrenOfType<RenderObject>(*this)) {
        if (is<RenderTableCol>(child))
            return &const_cast<RenderTableCol&>(downcast<RenderTableCol>(child));

        // We allow only table-captions before columns or column-groups.
        if (!is<RenderTableCaption>(child))
            return nullptr;
    }

    return nullptr;
}

void RenderTable::updateColumnCache() const
{
    ASSERT(m_hasColElements);
    ASSERT(m_columnRenderers.isEmpty());
    ASSERT(m_effectiveColumnIndexMap.isEmpty());
    ASSERT(!m_columnRenderersValid);

    unsigned columnIndex = 0;
    for (RenderTableCol* columnRenderer = firstColumn(); columnRenderer; columnRenderer = columnRenderer->nextColumn()) {
        if (columnRenderer->isTableColumnGroupWithColumnChildren())
            continue;
        m_columnRenderers.append(makeWeakPtr(columnRenderer));
        // FIXME: We should look to compute the effective column index successively from previous values instead of
        // calling colToEffCol(), which is in O(numEffCols()). Although it's unlikely that this is a hot function.
        m_effectiveColumnIndexMap.add(columnRenderer, colToEffCol(columnIndex));
        columnIndex += columnRenderer->span();
    }
    m_columnRenderersValid = true;
}

unsigned RenderTable::effectiveIndexOfColumn(const RenderTableCol& column) const
{
    if (!m_columnRenderersValid)
        updateColumnCache();
    const RenderTableCol* columnToUse = &column;
    if (columnToUse->isTableColumnGroupWithColumnChildren())
        columnToUse = columnToUse->nextColumn(); // First column in column-group
    auto it = m_effectiveColumnIndexMap.find(columnToUse);
    ASSERT(it != m_effectiveColumnIndexMap.end());
    if (it == m_effectiveColumnIndexMap.end())
        return std::numeric_limits<unsigned>::max();
    return it->value;
}

LayoutUnit RenderTable::offsetTopForColumn(const RenderTableCol& column) const
{
    if (effectiveIndexOfColumn(column) >= numEffCols())
        return 0;
    if (m_columnOffsetTop >= 0) {
        ASSERT(!needsLayout());
        return m_columnOffsetTop;
    }
    RenderTableSection* section = topNonEmptySection();
    return m_columnOffsetTop = section ? section->offsetTop() : LayoutUnit(0);
}

LayoutUnit RenderTable::offsetLeftForColumn(const RenderTableCol& column) const
{
    unsigned columnIndex = effectiveIndexOfColumn(column);
    if (columnIndex >= numEffCols())
        return 0;
    return m_columnPos[columnIndex] + m_hSpacing + borderLeft();
}

LayoutUnit RenderTable::offsetWidthForColumn(const RenderTableCol& column) const
{
    const RenderTableCol* currentColumn = &column;
    bool hasColumnChildren;
    if ((hasColumnChildren = currentColumn->isTableColumnGroupWithColumnChildren()))
        currentColumn = currentColumn->nextColumn(); // First column in column-group
    unsigned numberOfEffectiveColumns = numEffCols();
    ASSERT_WITH_SECURITY_IMPLICATION(m_columnPos.size() >= numberOfEffectiveColumns + 1);
    LayoutUnit width;
    LayoutUnit spacing = m_hSpacing;
    while (currentColumn) {
        unsigned columnIndex = effectiveIndexOfColumn(*currentColumn);
        unsigned span = currentColumn->span();
        while (span && columnIndex < numberOfEffectiveColumns) {
            width += m_columnPos[columnIndex + 1] - m_columnPos[columnIndex] - spacing;
            span -= m_columns[columnIndex].span;
            ++columnIndex;
            if (span)
                width += spacing;
        }
        if (!hasColumnChildren)
            break;
        currentColumn = currentColumn->nextColumn();
        if (!currentColumn || currentColumn->isTableColumnGroup())
            break;
        width += spacing;
    }
    return width;
}

LayoutUnit RenderTable::offsetHeightForColumn(const RenderTableCol& column) const
{
    if (effectiveIndexOfColumn(column) >= numEffCols())
        return 0;
    if (m_columnOffsetHeight >= 0) {
        ASSERT(!needsLayout());
        return m_columnOffsetHeight;
    }
    LayoutUnit height;
    for (RenderTableSection* section = topSection(); section; section = sectionBelow(section))
        height += section->offsetHeight();
    m_columnOffsetHeight = height;
    return m_columnOffsetHeight;
}

RenderTableCol* RenderTable::slowColElement(unsigned col, bool* startEdge, bool* endEdge) const
{
    ASSERT(m_hasColElements);

    if (!m_columnRenderersValid)
        updateColumnCache();

    unsigned columnCount = 0;
    for (auto& columnRenderer : m_columnRenderers) {
        if (!columnRenderer)
            continue;
        unsigned span = columnRenderer->span();
        unsigned startCol = columnCount;
        ASSERT(span >= 1);
        unsigned endCol = columnCount + span - 1;
        columnCount += span;
        if (columnCount > col) {
            if (startEdge)
                *startEdge = startCol == col;
            if (endEdge)
                *endEdge = endCol == col;
            return columnRenderer.get();
        }
    }
    return nullptr;
}

void RenderTable::recalcSections() const
{
    ASSERT(m_needsSectionRecalc);

    m_head.clear();
    m_foot.clear();
    m_firstBody.clear();
    m_hasColElements = false;
    m_hasCellColspanThatDeterminesTableWidth = hasCellColspanThatDeterminesTableWidth();

    // We need to get valid pointers to caption, head, foot and first body again
    RenderObject* nextSibling;
    for (RenderObject* child = firstChild(); child; child = nextSibling) {
        nextSibling = child->nextSibling();
        switch (child->style().display()) {
        case DisplayType::TableColumn:
        case DisplayType::TableColumnGroup:
            m_hasColElements = true;
            break;
        case DisplayType::TableHeaderGroup:
            if (is<RenderTableSection>(*child)) {
                RenderTableSection& section = downcast<RenderTableSection>(*child);
                if (!m_head)
                    m_head = makeWeakPtr(section);
                else if (!m_firstBody)
                    m_firstBody = makeWeakPtr(section);
                section.recalcCellsIfNeeded();
            }
            break;
        case DisplayType::TableFooterGroup:
            if (is<RenderTableSection>(*child)) {
                RenderTableSection& section = downcast<RenderTableSection>(*child);
                if (!m_foot)
                    m_foot = makeWeakPtr(section);
                else if (!m_firstBody)
                    m_firstBody = makeWeakPtr(section);
                section.recalcCellsIfNeeded();
            }
            break;
        case DisplayType::TableRowGroup:
            if (is<RenderTableSection>(*child)) {
                RenderTableSection& section = downcast<RenderTableSection>(*child);
                if (!m_firstBody)
                    m_firstBody = makeWeakPtr(section);
                section.recalcCellsIfNeeded();
            }
            break;
        default:
            break;
        }
    }

    // repair column count (addChild can grow it too much, because it always adds elements to the last row of a section)
    unsigned maxCols = 0;
    for (auto& section : childrenOfType<RenderTableSection>(*this)) {
        unsigned sectionCols = section.numColumns();
        if (sectionCols > maxCols)
            maxCols = sectionCols;
    }
    
    m_columns.resize(maxCols);
    m_columnPos.resize(maxCols + 1);

    // Now that we know the number of maximum number of columns, let's shrink the sections grids if needed.
    for (auto& section : childrenOfType<RenderTableSection>(const_cast<RenderTable&>(*this)))
        section.removeRedundantColumns();

    ASSERT(selfNeedsLayout());

    m_needsSectionRecalc = false;
}

LayoutUnit RenderTable::calcBorderStart() const
{
    if (!collapseBorders())
        return RenderBlock::borderStart();

    // Determined by the first cell of the first row. See the CSS 2.1 spec, section 17.6.2.
    if (!numEffCols())
        return 0;

    float borderWidth = 0;

    const BorderValue& tableStartBorder = style().borderStart();
    if (tableStartBorder.style() == BorderStyle::Hidden)
        return 0;
    if (tableStartBorder.style() > BorderStyle::Hidden)
        borderWidth = tableStartBorder.width();

    if (RenderTableCol* column = colElement(0)) {
        // FIXME: We don't account for direction on columns and column groups.
        const BorderValue& columnAdjoiningBorder = column->style().borderStart();
        if (columnAdjoiningBorder.style() == BorderStyle::Hidden)
            return 0;
        if (columnAdjoiningBorder.style() > BorderStyle::Hidden)
            borderWidth = std::max(borderWidth, columnAdjoiningBorder.width());
        // FIXME: This logic doesn't properly account for the first column in the first column-group case.
    }

    if (const RenderTableSection* topNonEmptySection = this->topNonEmptySection()) {
        const BorderValue& sectionAdjoiningBorder = topNonEmptySection->borderAdjoiningTableStart();
        if (sectionAdjoiningBorder.style() == BorderStyle::Hidden)
            return 0;

        if (sectionAdjoiningBorder.style() > BorderStyle::Hidden)
            borderWidth = std::max(borderWidth, sectionAdjoiningBorder.width());

        if (const RenderTableCell* adjoiningStartCell = topNonEmptySection->firstRowCellAdjoiningTableStart()) {
            // FIXME: Make this work with perpendicular and flipped cells.
            const BorderValue& startCellAdjoiningBorder = adjoiningStartCell->borderAdjoiningTableStart();
            if (startCellAdjoiningBorder.style() == BorderStyle::Hidden)
                return 0;

            const BorderValue& firstRowAdjoiningBorder = adjoiningStartCell->row()->borderAdjoiningTableStart();
            if (firstRowAdjoiningBorder.style() == BorderStyle::Hidden)
                return 0;

            if (startCellAdjoiningBorder.style() > BorderStyle::Hidden)
                borderWidth = std::max(borderWidth, startCellAdjoiningBorder.width());
            if (firstRowAdjoiningBorder.style() > BorderStyle::Hidden)
                borderWidth = std::max(borderWidth, firstRowAdjoiningBorder.width());
        }
    }
    return CollapsedBorderValue::adjustedCollapsedBorderWidth(borderWidth, document().deviceScaleFactor(), !style().isLeftToRightDirection());
}

LayoutUnit RenderTable::calcBorderEnd() const
{
    if (!collapseBorders())
        return RenderBlock::borderEnd();

    // Determined by the last cell of the first row. See the CSS 2.1 spec, section 17.6.2.
    if (!numEffCols())
        return 0;

    float borderWidth = 0;

    const BorderValue& tableEndBorder = style().borderEnd();
    if (tableEndBorder.style() == BorderStyle::Hidden)
        return 0;
    if (tableEndBorder.style() > BorderStyle::Hidden)
        borderWidth = tableEndBorder.width();

    unsigned endColumn = numEffCols() - 1;
    if (RenderTableCol* column = colElement(endColumn)) {
        // FIXME: We don't account for direction on columns and column groups.
        const BorderValue& columnAdjoiningBorder = column->style().borderEnd();
        if (columnAdjoiningBorder.style() == BorderStyle::Hidden)
            return 0;
        if (columnAdjoiningBorder.style() > BorderStyle::Hidden)
            borderWidth = std::max(borderWidth, columnAdjoiningBorder.width());
        // FIXME: This logic doesn't properly account for the last column in the last column-group case.
    }

    if (const RenderTableSection* topNonEmptySection = this->topNonEmptySection()) {
        const BorderValue& sectionAdjoiningBorder = topNonEmptySection->borderAdjoiningTableEnd();
        if (sectionAdjoiningBorder.style() == BorderStyle::Hidden)
            return 0;

        if (sectionAdjoiningBorder.style() > BorderStyle::Hidden)
            borderWidth = std::max(borderWidth, sectionAdjoiningBorder.width());

        if (const RenderTableCell* adjoiningEndCell = topNonEmptySection->firstRowCellAdjoiningTableEnd()) {
            // FIXME: Make this work with perpendicular and flipped cells.
            const BorderValue& endCellAdjoiningBorder = adjoiningEndCell->borderAdjoiningTableEnd();
            if (endCellAdjoiningBorder.style() == BorderStyle::Hidden)
                return 0;

            const BorderValue& firstRowAdjoiningBorder = adjoiningEndCell->row()->borderAdjoiningTableEnd();
            if (firstRowAdjoiningBorder.style() == BorderStyle::Hidden)
                return 0;

            if (endCellAdjoiningBorder.style() > BorderStyle::Hidden)
                borderWidth = std::max(borderWidth, endCellAdjoiningBorder.width());
            if (firstRowAdjoiningBorder.style() > BorderStyle::Hidden)
                borderWidth = std::max(borderWidth, firstRowAdjoiningBorder.width());
        }
    }
    return CollapsedBorderValue::adjustedCollapsedBorderWidth(borderWidth, document().deviceScaleFactor(), style().isLeftToRightDirection());
}

void RenderTable::recalcBordersInRowDirection()
{
    // FIXME: We need to compute the collapsed before / after borders in the same fashion.
    m_borderStart = calcBorderStart();
    m_borderEnd = calcBorderEnd();
}

LayoutUnit RenderTable::borderBefore() const
{
    if (collapseBorders()) {
        recalcSectionsIfNeeded();
        return outerBorderBefore();
    }
    return RenderBlock::borderBefore();
}

LayoutUnit RenderTable::borderAfter() const
{
    if (collapseBorders()) {
        recalcSectionsIfNeeded();
        return outerBorderAfter();
    }
    return RenderBlock::borderAfter();
}

LayoutUnit RenderTable::outerBorderBefore() const
{
    if (!collapseBorders())
        return 0;
    LayoutUnit borderWidth;
    if (RenderTableSection* topSection = this->topSection()) {
        borderWidth = topSection->outerBorderBefore();
        if (borderWidth < 0)
            return 0;   // Overridden by hidden
    }
    const BorderValue& tb = style().borderBefore();
    if (tb.style() == BorderStyle::Hidden)
        return 0;
    if (tb.style() > BorderStyle::Hidden) {
        LayoutUnit collapsedBorderWidth = std::max<LayoutUnit>(borderWidth, tb.width() / 2);
        borderWidth = floorToDevicePixel(collapsedBorderWidth, document().deviceScaleFactor());
    }
    return borderWidth;
}

LayoutUnit RenderTable::outerBorderAfter() const
{
    if (!collapseBorders())
        return 0;
    LayoutUnit borderWidth;

    if (RenderTableSection* section = bottomSection()) {
        borderWidth = section->outerBorderAfter();
        if (borderWidth < 0)
            return 0; // Overridden by hidden
    }
    const BorderValue& tb = style().borderAfter();
    if (tb.style() == BorderStyle::Hidden)
        return 0;
    if (tb.style() > BorderStyle::Hidden) {
        float deviceScaleFactor = document().deviceScaleFactor();
        LayoutUnit collapsedBorderWidth = std::max<LayoutUnit>(borderWidth, (tb.width() + (1 / deviceScaleFactor)) / 2);
        borderWidth = floorToDevicePixel(collapsedBorderWidth, deviceScaleFactor);
    }
    return borderWidth;
}

LayoutUnit RenderTable::outerBorderStart() const
{
    if (!collapseBorders())
        return 0;

    LayoutUnit borderWidth;

    const BorderValue& tb = style().borderStart();
    if (tb.style() == BorderStyle::Hidden)
        return 0;
    if (tb.style() > BorderStyle::Hidden)
        return CollapsedBorderValue::adjustedCollapsedBorderWidth(tb.width(), document().deviceScaleFactor(), !style().isLeftToRightDirection());

    bool allHidden = true;
    for (RenderTableSection* section = topSection(); section; section = sectionBelow(section)) {
        LayoutUnit sw = section->outerBorderStart();
        if (sw < 0)
            continue;
        allHidden = false;
        borderWidth = std::max(borderWidth, sw);
    }
    if (allHidden)
        return 0;

    return borderWidth;
}

LayoutUnit RenderTable::outerBorderEnd() const
{
    if (!collapseBorders())
        return 0;

    LayoutUnit borderWidth;

    const BorderValue& tb = style().borderEnd();
    if (tb.style() == BorderStyle::Hidden)
        return 0;
    if (tb.style() > BorderStyle::Hidden)
        return CollapsedBorderValue::adjustedCollapsedBorderWidth(tb.width(), document().deviceScaleFactor(), style().isLeftToRightDirection());

    bool allHidden = true;
    for (RenderTableSection* section = topSection(); section; section = sectionBelow(section)) {
        LayoutUnit sw = section->outerBorderEnd();
        if (sw < 0)
            continue;
        allHidden = false;
        borderWidth = std::max(borderWidth, sw);
    }
    if (allHidden)
        return 0;

    return borderWidth;
}

RenderTableSection* RenderTable::sectionAbove(const RenderTableSection* section, SkipEmptySectionsValue skipEmptySections) const
{
    recalcSectionsIfNeeded();

    if (section == m_head)
        return nullptr;

    RenderObject* prevSection = section == m_foot ? lastChild() : section->previousSibling();
    while (prevSection) {
        if (is<RenderTableSection>(*prevSection) && prevSection != m_head && prevSection != m_foot && (skipEmptySections == DoNotSkipEmptySections || downcast<RenderTableSection>(*prevSection).numRows()))
            break;
        prevSection = prevSection->previousSibling();
    }
    if (!prevSection && m_head && (skipEmptySections == DoNotSkipEmptySections || m_head->numRows()))
        prevSection = m_head.get();
    return downcast<RenderTableSection>(prevSection);
}

RenderTableSection* RenderTable::sectionBelow(const RenderTableSection* section, SkipEmptySectionsValue skipEmptySections) const
{
    recalcSectionsIfNeeded();

    if (section == m_foot)
        return nullptr;

    RenderObject* nextSection = section == m_head ? firstChild() : section->nextSibling();
    while (nextSection) {
        if (is<RenderTableSection>(*nextSection) && nextSection != m_head && nextSection != m_foot && (skipEmptySections  == DoNotSkipEmptySections || downcast<RenderTableSection>(*nextSection).numRows()))
            break;
        nextSection = nextSection->nextSibling();
    }
    if (!nextSection && m_foot && (skipEmptySections == DoNotSkipEmptySections || m_foot->numRows()))
        nextSection = m_foot.get();
    return downcast<RenderTableSection>(nextSection);
}

RenderTableSection* RenderTable::bottomSection() const
{
    recalcSectionsIfNeeded();
    if (m_foot)
        return m_foot.get();
    for (RenderObject* child = lastChild(); child; child = child->previousSibling()) {
        if (is<RenderTableSection>(*child))
            return downcast<RenderTableSection>(child);
    }
    return nullptr;
}

RenderTableCell* RenderTable::cellAbove(const RenderTableCell* cell) const
{
    recalcSectionsIfNeeded();

    // Find the section and row to look in
    unsigned r = cell->rowIndex();
    RenderTableSection* section = nullptr;
    unsigned rAbove = 0;
    if (r > 0) {
        // cell is not in the first row, so use the above row in its own section
        section = cell->section();
        rAbove = r - 1;
    } else {
        section = sectionAbove(cell->section(), SkipEmptySections);
        if (section) {
            ASSERT(section->numRows());
            rAbove = section->numRows() - 1;
        }
    }

    // Look up the cell in the section's grid, which requires effective col index
    if (section) {
        unsigned effCol = colToEffCol(cell->col());
        RenderTableSection::CellStruct& aboveCell = section->cellAt(rAbove, effCol);
        return aboveCell.primaryCell();
    } else
        return nullptr;
}

RenderTableCell* RenderTable::cellBelow(const RenderTableCell* cell) const
{
    recalcSectionsIfNeeded();

    // Find the section and row to look in
    unsigned r = cell->rowIndex() + cell->rowSpan() - 1;
    RenderTableSection* section = nullptr;
    unsigned rBelow = 0;
    if (r < cell->section()->numRows() - 1) {
        // The cell is not in the last row, so use the next row in the section.
        section = cell->section();
        rBelow = r + 1;
    } else {
        section = sectionBelow(cell->section(), SkipEmptySections);
        if (section)
            rBelow = 0;
    }

    // Look up the cell in the section's grid, which requires effective col index
    if (section) {
        unsigned effCol = colToEffCol(cell->col());
        RenderTableSection::CellStruct& belowCell = section->cellAt(rBelow, effCol);
        return belowCell.primaryCell();
    } else
        return nullptr;
}

RenderTableCell* RenderTable::cellBefore(const RenderTableCell* cell) const
{
    recalcSectionsIfNeeded();

    RenderTableSection* section = cell->section();
    unsigned effCol = colToEffCol(cell->col());
    if (!effCol)
        return nullptr;
    
    // If we hit a colspan back up to a real cell.
    RenderTableSection::CellStruct& prevCell = section->cellAt(cell->rowIndex(), effCol - 1);
    return prevCell.primaryCell();
}

RenderTableCell* RenderTable::cellAfter(const RenderTableCell* cell) const
{
    recalcSectionsIfNeeded();

    unsigned effCol = colToEffCol(cell->col() + cell->colSpan());
    if (effCol >= numEffCols())
        return nullptr;
    return cell->section()->primaryCellAt(cell->rowIndex(), effCol);
}

RenderBlock* RenderTable::firstLineBlock() const
{
    return nullptr;
}

int RenderTable::baselinePosition(FontBaseline baselineType, bool firstLine, LineDirectionMode direction, LinePositionMode linePositionMode) const
{
    return valueOrCompute(firstLineBaseline(), [&] {
        return RenderBox::baselinePosition(baselineType, firstLine, direction, linePositionMode);
    });
}

std::optional<int> RenderTable::inlineBlockBaseline(LineDirectionMode) const
{
    // Tables are skipped when computing an inline-block's baseline.
    return std::optional<int>();
}

std::optional<int> RenderTable::firstLineBaseline() const
{
    // The baseline of a 'table' is the same as the 'inline-table' baseline per CSS 3 Flexbox (CSS 2.1
    // doesn't define the baseline of a 'table' only an 'inline-table').
    // This is also needed to properly determine the baseline of a cell if it has a table child.

    if (isWritingModeRoot())
        return std::optional<int>();

    recalcSectionsIfNeeded();

    const RenderTableSection* topNonEmptySection = this->topNonEmptySection();
    if (!topNonEmptySection)
        return std::optional<int>();

    if (std::optional<int> baseline = topNonEmptySection->firstLineBaseline())
        return std::optional<int>(topNonEmptySection->logicalTop() + baseline.value());

    // FIXME: A table row always has a baseline per CSS 2.1. Will this return the right value?
    return std::optional<int>();
}

LayoutRect RenderTable::overflowClipRect(const LayoutPoint& location, RenderFragmentContainer* fragment, OverlayScrollbarSizeRelevancy relevancy, PaintPhase phase)
{
    LayoutRect rect;
    // Don't clip out the table's side of the collapsed borders if we're in the paint phase that will ask the sections to paint them.
    // Likewise, if we're self-painting we avoid clipping them out as the clip rect that will be passed down to child layers from RenderLayer will do that instead.
    if (phase == PaintPhase::ChildBlockBackgrounds || layer()->isSelfPaintingLayer()) {
        rect = borderBoxRectInFragment(fragment);
        rect.setLocation(location + rect.location());
    } else
        rect = RenderBox::overflowClipRect(location, fragment, relevancy);

    // If we have a caption, expand the clip to include the caption.
    // FIXME: Technically this is wrong, but it's virtually impossible to fix this
    // for real until captions have been re-written.
    // FIXME: This code assumes (like all our other caption code) that only top/bottom are
    // supported.  When we actually support left/right and stop mapping them to top/bottom,
    // we might have to hack this code first (depending on what order we do these bug fixes in).
    if (!m_captions.isEmpty()) {
        if (style().isHorizontalWritingMode()) {
            rect.setHeight(height());
            rect.setY(location.y());
        } else {
            rect.setWidth(width());
            rect.setX(location.x());
        }
    }

    return rect;
}

bool RenderTable::nodeAtPoint(const HitTestRequest& request, HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction action)
{
    LayoutPoint adjustedLocation = accumulatedOffset + location();

    // Check kids first.
    if (!hasOverflowClip() || locationInContainer.intersects(overflowClipRect(adjustedLocation, nullptr))) {
        for (RenderObject* child = lastChild(); child; child = child->previousSibling()) {
            if (is<RenderBox>(*child) && !downcast<RenderBox>(*child).hasSelfPaintingLayer() && (child->isTableSection() || child->isTableCaption())) {
                LayoutPoint childPoint = flipForWritingModeForChild(downcast<RenderBox>(child), adjustedLocation);
                if (child->nodeAtPoint(request, result, locationInContainer, childPoint, action)) {
                    updateHitTestResult(result, toLayoutPoint(locationInContainer.point() - childPoint));
                    return true;
                }
            }
        }
    }

    // Check our bounds next.
    LayoutRect boundsRect(adjustedLocation, size());
    if (visibleToHitTesting() && (action == HitTestBlockBackground || action == HitTestChildBlockBackground) && locationInContainer.intersects(boundsRect)) {
        updateHitTestResult(result, flipForWritingMode(locationInContainer.point() - toLayoutSize(adjustedLocation)));
        if (result.addNodeToListBasedTestResult(element(), request, locationInContainer, boundsRect) == HitTestProgress::Stop)
            return true;
    }

    return false;
}

RenderPtr<RenderTable> RenderTable::createTableWithStyle(Document& document, const RenderStyle& style)
{
    auto table = createRenderer<RenderTable>(document, RenderStyle::createAnonymousStyleWithDisplay(style, style.display() == DisplayType::Inline ? DisplayType::InlineTable : DisplayType::Table));
    table->initializeStyle();
    return table;
}

RenderPtr<RenderTable> RenderTable::createAnonymousWithParentRenderer(const RenderElement& parent)
{
    return RenderTable::createTableWithStyle(parent.document(), parent.style());
}

const BorderValue& RenderTable::tableStartBorderAdjoiningCell(const RenderTableCell& cell) const
{
    ASSERT(cell.isFirstOrLastCellInRow());
    if (isDirectionSame(this, cell.row()))
        return style().borderStart();

    return style().borderEnd();
}

const BorderValue& RenderTable::tableEndBorderAdjoiningCell(const RenderTableCell& cell) const
{
    ASSERT(cell.isFirstOrLastCellInRow());
    if (isDirectionSame(this, cell.row()))
        return style().borderEnd();

    return style().borderStart();
}

void RenderTable::markForPaginationRelayoutIfNeeded()
{
    auto* layoutState = view().frameView().layoutContext().layoutState();
    if (!layoutState->isPaginated() || (!layoutState->pageLogicalHeightChanged() && (!layoutState->pageLogicalHeight() || layoutState->pageLogicalOffset(this, logicalTop()) == pageLogicalOffset())))
        return;
    
    // When a table moves, we have to dirty all of the sections too.
    if (!needsLayout())
        setChildNeedsLayout(MarkOnlyThis);
    for (auto& child : childrenOfType<RenderTableSection>(*this)) {
        if (!child.needsLayout())
            child.setChildNeedsLayout(MarkOnlyThis);
    }
}

}
