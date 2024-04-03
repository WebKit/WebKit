/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "RenderFlexibleBox.h"

#include "BaselineAlignment.h"
#include "FlexibleBoxAlgorithm.h"
#include "FontBaseline.h"
#include "HitTestResult.h"
#include "InspectorInstrumentation.h"
#include "LayoutIntegrationCoverage.h"
#include "LayoutIntegrationFlexLayout.h"
#include "LayoutRepainter.h"
#include "LayoutUnit.h"
#include "RenderBlockInlines.h"
#include "RenderBoxInlines.h"
#include "RenderBoxModelObjectInlines.h"
#include "RenderChildIterator.h"
#include "RenderElementInlines.h"
#include "RenderLayer.h"
#include "RenderLayoutState.h"
#include "RenderObjectEnums.h"
#include "RenderObjectInlines.h"
#include "RenderReplaced.h"
#include "RenderSVGRoot.h"
#include "RenderStyleConstants.h"
#include "RenderTable.h"
#include "RenderView.h"
#include "WritingMode.h"
#include <limits>
#include <wtf/IsoMallocInlines.h>
#include <wtf/MathExtras.h>
#include <wtf/SetForScope.h>
#include <wtf/TypeCasts.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(RenderFlexibleBox);

struct RenderFlexibleBox::LineState {
    LineState(LayoutUnit crossAxisOffset, LayoutUnit crossAxisExtent, std::optional<BaselineAlignmentState> baselineAlignmentState, FlexItems&& flexItems)
        : crossAxisOffset(crossAxisOffset)
        , crossAxisExtent(crossAxisExtent)
        , baselineAlignmentState(baselineAlignmentState)
        , flexItems(flexItems)
    {
    }
    
    LayoutUnit crossAxisOffset;
    LayoutUnit crossAxisExtent;
    std::optional<BaselineAlignmentState> baselineAlignmentState;
    FlexItems flexItems;
};

RenderFlexibleBox::RenderFlexibleBox(Type type, Element& element, RenderStyle&& style)
    : RenderBlock(type, element, WTFMove(style), TypeFlag::IsFlexibleBox)
{
    ASSERT(isRenderFlexibleBox());
    setChildrenInline(false); // All of our children must be block-level.
}

RenderFlexibleBox::RenderFlexibleBox(Type type, Document& document, RenderStyle&& style)
    : RenderBlock(type, document, WTFMove(style), TypeFlag::IsFlexibleBox)
{
    ASSERT(isRenderFlexibleBox());
    setChildrenInline(false); // All of our children must be block-level.
}

RenderFlexibleBox::~RenderFlexibleBox() = default;

ASCIILiteral RenderFlexibleBox::renderName() const
{
    return "RenderFlexibleBox"_s;
}

void RenderFlexibleBox::computeIntrinsicLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const
{
    auto addScrollbarWidth = [&]() {
        LayoutUnit scrollbarWidth(scrollbarLogicalWidth());
        maxLogicalWidth += scrollbarWidth;
        minLogicalWidth += scrollbarWidth;
    };

    if (shouldApplySizeOrInlineSizeContainment()) {
        if (auto width = explicitIntrinsicInnerLogicalWidth()) {
            minLogicalWidth = width.value();
            maxLogicalWidth = width.value();
        }
        addScrollbarWidth();
        return;
    }

    LayoutUnit childMinWidth;
    LayoutUnit childMaxWidth;
    bool hadExcludedChildren = computePreferredWidthsForExcludedChildren(childMinWidth, childMaxWidth);

    // FIXME: We're ignoring flex-basis here and we shouldn't. We can't start
    // honoring it though until the flex shorthand stops setting it to 0. See
    // https://bugs.webkit.org/show_bug.cgi?id=116117 and
    // https://crbug.com/240765.
    size_t numItemsWithNormalLayout = 0;
    for (RenderBox* child = firstChildBox(); child; child = child->nextSiblingBox()) {
        if (child->isOutOfFlowPositioned() || child->isExcludedFromNormalLayout())
            continue;
        ++numItemsWithNormalLayout;

        // Pre-layout orthogonal children in order to get a valid value for the preferred width.
        if (style().isHorizontalWritingMode() != child->style().isHorizontalWritingMode())
            child->layoutIfNeeded();

        LayoutUnit margin = marginIntrinsicLogicalWidthForChild(*child);

        LayoutUnit minPreferredLogicalWidth;
        LayoutUnit maxPreferredLogicalWidth;
        computeChildPreferredLogicalWidths(*child, minPreferredLogicalWidth, maxPreferredLogicalWidth);

        minPreferredLogicalWidth += margin;
        maxPreferredLogicalWidth += margin;

        if (!isColumnFlow()) {
            maxLogicalWidth += maxPreferredLogicalWidth;
            if (isMultiline()) {
                // For multiline, the min preferred width is if you put a break between
                // each item.
                minLogicalWidth = std::max(minLogicalWidth, minPreferredLogicalWidth);
            } else
                minLogicalWidth += minPreferredLogicalWidth;
        } else {
            minLogicalWidth = std::max(minPreferredLogicalWidth, minLogicalWidth);
            maxLogicalWidth = std::max(maxPreferredLogicalWidth, maxLogicalWidth);
        }
    }

    if (!isColumnFlow() && numItemsWithNormalLayout > 1) {
        LayoutUnit inlineGapSize = (numItemsWithNormalLayout - 1) * computeGap(GapType::BetweenItems);
        maxLogicalWidth += inlineGapSize;
        if (!isMultiline())
            minLogicalWidth += inlineGapSize;
    }

    maxLogicalWidth = std::max(minLogicalWidth, maxLogicalWidth);
    
    // Due to negative margins, it is possible that we calculated a negative
    // intrinsic width. Make sure that we never return a negative width.
    minLogicalWidth = std::max(0_lu, minLogicalWidth);
    maxLogicalWidth = std::max(0_lu, maxLogicalWidth);
    
    if (hadExcludedChildren) {
        minLogicalWidth = std::max(minLogicalWidth, childMinWidth);
        maxLogicalWidth = std::max(maxLogicalWidth, childMaxWidth);
    }

    addScrollbarWidth();
}

#define SET_OR_CLEAR_OVERRIDING_SIZE(box, SizeType, size)   \
    {                                                       \
        if (size)                                           \
            box.setOverridingLogical##SizeType(*size);      \
        else                                                \
            box.clearOverridingLogical##SizeType();         \
    }

// RAII class which defines a scope in which overriding sizes of a box are either:
//   1) replaced by other size in one axis if size is specified
//   2) cleared in both axis if size == std::nullopt
//
// In any case the previous overriding sizes are restored on destruction (in case of
// not having a previous value it's simply cleared).
class OverridingSizesScope {
public:
    enum class Axis {
        Inline,
        Block,
        Both
    };

    OverridingSizesScope(RenderBox& box, Axis axis, std::optional<LayoutUnit> size = std::nullopt)
        : m_box(box)
        , m_axis(axis)
    {
        ASSERT(!size || (axis != Axis::Both));
        if (axis == Axis::Both || axis == Axis::Inline) {
            if (box.hasOverridingLogicalWidth())
                m_overridingWidth = box.overridingLogicalWidth();
            SET_OR_CLEAR_OVERRIDING_SIZE(m_box, Width, size);
        }
        if (axis == Axis::Both || axis == Axis::Block) {
            if (box.hasOverridingLogicalHeight())
                m_overridingHeight = box.overridingLogicalHeight();
            SET_OR_CLEAR_OVERRIDING_SIZE(m_box, Height, size);
        }
    }
    ~OverridingSizesScope()
    {
        if (m_axis == Axis::Inline || m_axis == Axis::Both)
            SET_OR_CLEAR_OVERRIDING_SIZE(m_box, Width, m_overridingWidth);

        if (m_axis == Axis::Block || m_axis == Axis::Both)
            SET_OR_CLEAR_OVERRIDING_SIZE(m_box, Height, m_overridingHeight);
    }

private:
    RenderBox& m_box;
    Axis m_axis;
    std::optional<LayoutUnit> m_overridingWidth;
    std::optional<LayoutUnit> m_overridingHeight;
};

static void updateFlexItemDirtyBitsBeforeLayout(bool relayoutFlexItem, RenderBox& flexItem)
{
    if (flexItem.isOutOfFlowPositioned())
        return;

    // FIXME: Technically percentage height objects only need a relayout if their percentage isn't going to be turned into
    // an auto value. Add a method to determine this, so that we can avoid the relayout.
    if (relayoutFlexItem || flexItem.hasRelativeLogicalHeight())
        flexItem.setChildNeedsLayout(MarkOnlyThis);
}

void RenderFlexibleBox::computeChildIntrinsicLogicalWidths(RenderObject& childObject, LayoutUnit& minPreferredLogicalWidth, LayoutUnit& maxPreferredLogicalWidth) const
{
    RenderBox& child = downcast<RenderBox>(childObject);

    // If the item cross size should use the definite container cross size then set the overriding size now so
    // the intrinsic sizes are properly computed in the presence of aspect ratios. The only exception is when
    // we are both a flex item&container, because our parent might have already set our overriding size.
    if (childCrossSizeShouldUseContainerCrossSize(child) && !isFlexItem()) {
        auto axis = mainAxisIsChildInlineAxis(child) ? OverridingSizesScope::Axis::Block : OverridingSizesScope::Axis::Inline;
        OverridingSizesScope overridingSizeScope(child, axis, computeCrossSizeForChildUsingContainerCrossSize(child));
        RenderBlock::computeChildIntrinsicLogicalWidths(childObject, minPreferredLogicalWidth, maxPreferredLogicalWidth);
        return;
    }

    OverridingSizesScope cleanOverridingSizesScope(child, OverridingSizesScope::Axis::Both);
    RenderBlock::computeChildIntrinsicLogicalWidths(childObject, minPreferredLogicalWidth, maxPreferredLogicalWidth);
}

LayoutUnit RenderFlexibleBox::baselinePosition(FontBaseline, bool, LineDirectionMode direction, LinePositionMode) const
{
    auto baseline = firstLineBaseline();
    if (!baseline)
        return synthesizedBaseline(*this, *parentStyle(), direction, BorderBox) + marginLogicalHeight();

    return baseline.value() + (direction == HorizontalLine ? marginTop() : marginRight()).toInt();
}

std::optional<LayoutUnit> RenderFlexibleBox::firstLineBaseline() const
{
    if ((isWritingModeRoot() && !isFlexItem()) || !m_numberOfInFlowChildrenOnFirstLine || shouldApplyLayoutContainment())
        return std::optional<LayoutUnit>();
    RenderBox* baselineChild = getBaselineChild(ItemPosition::Baseline);
    
    if (!baselineChild)
        return std::optional<LayoutUnit>();

    if (!isColumnFlow() && !mainAxisIsChildInlineAxis(*baselineChild))
        return LayoutUnit { (crossAxisExtentForChild(*baselineChild) + baselineChild->logicalTop()).toInt() };
    if (isColumnFlow() && mainAxisIsChildInlineAxis(*baselineChild))
        return LayoutUnit { (mainAxisExtentForChild(*baselineChild) + baselineChild->logicalTop()).toInt() };

    std::optional<LayoutUnit> baseline = baselineChild->firstLineBaseline();
    if (!baseline) {
        // FIXME: We should pass |direction| into firstLineBoxBaseline and stop bailing out if we're a writing mode root.
        // This would also fix some cases where the flexbox is orthogonal to its container.
        LineDirectionMode direction = isHorizontalWritingMode() ? HorizontalLine : VerticalLine;
        return synthesizedBaseline(*baselineChild, style(), direction, BorderBox) + baselineChild->logicalTop();
    }

    return LayoutUnit { (baseline.value() + baselineChild->logicalTop()).toInt() };
}

std::optional <LayoutUnit> RenderFlexibleBox::lastLineBaseline() const
{
    if (isWritingModeRoot() || !m_numberOfInFlowChildrenOnLastLine || shouldApplyLayoutContainment())
        return std::optional<LayoutUnit>();
    RenderBox* baselineChild = getBaselineChild(ItemPosition::LastBaseline);
    
    if (!baselineChild)
        return std::optional<LayoutUnit>();

    if (!isColumnFlow() && !mainAxisIsChildInlineAxis(*baselineChild))
        return LayoutUnit { (crossAxisExtentForChild(*baselineChild) + baselineChild->logicalTop()).toInt() };
    if (isColumnFlow() && mainAxisIsChildInlineAxis(*baselineChild))
        return LayoutUnit { (mainAxisExtentForChild(*baselineChild) + baselineChild->logicalTop()).toInt() };

    auto baseline = baselineChild->lastLineBaseline();
    if (!baseline) {
        // FIXME: We should pass |direction| into firstLineBoxBaseline and stop bailing out if we're a writing mode root.
        // This would also fix some cases where the flexbox is orthogonal to its container.
        LineDirectionMode direction = isHorizontalWritingMode() ? HorizontalLine : VerticalLine;
        return synthesizedBaseline(*baselineChild, style(), direction, BorderBox) + baselineChild->logicalTop();
    }

    return LayoutUnit { (baseline.value() + baselineChild->logicalTop()).toInt() }; 
}

RenderBox* RenderFlexibleBox::getBaselineChild(ItemPosition alignment) const
{
    ASSERT(alignment == ItemPosition::Baseline || alignment == ItemPosition::LastBaseline);

    RenderBox* baselineChild = nullptr;
    auto childIterator = (alignment == ItemPosition::Baseline) ? m_orderIterator : m_orderIterator.reverse();

    size_t childNumber = 0;
    for (auto* child = childIterator.first(); child; child = childIterator.next()) {
        if (m_orderIterator.shouldSkipChild(*child))
            continue;
        if (alignmentForChild(*child) == alignment && !hasAutoMarginsInCrossAxis(*child)) {
            baselineChild = child;
            break;
        }
        if (!baselineChild)
            baselineChild = child;

        ++childNumber;
        auto numberOfChildrenOnLine = alignment == ItemPosition::Baseline ? m_numberOfInFlowChildrenOnFirstLine : m_numberOfInFlowChildrenOnLastLine;
        if (numberOfChildrenOnLine && childNumber == numberOfChildrenOnLine.value())
            break;
    }
    return baselineChild;
}

std::optional<LayoutUnit> RenderFlexibleBox::inlineBlockBaseline(LineDirectionMode) const
{
    return firstLineBaseline();
}

static const StyleContentAlignmentData& contentAlignmentNormalBehavior()
{
    // The justify-content property applies along the main axis, but since
    // flexing in the main axis is controlled by flex, stretch behaves as
    // flex-start (ignoring the specified fallback alignment, if any).
    // https://drafts.csswg.org/css-align/#distribution-flex
    static const StyleContentAlignmentData normalBehavior = { ContentPosition::Normal, ContentDistribution::Stretch};
    return normalBehavior;
}

void RenderFlexibleBox::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderBlock::styleDidChange(diff, oldStyle);
    if (!oldStyle || diff != StyleDifference::Layout)
        return;

    auto oldStyleAlignItemsIsStretch = oldStyle->resolvedAlignItems(selfAlignmentNormalBehavior()).position() == ItemPosition::Stretch;
    for (auto& flexItem : childrenOfType<RenderBox>(*this)) {
        if (flexItem.needsPreferredWidthsRecalculation())
            flexItem.setPreferredLogicalWidthsDirty(true, MarkingBehavior::MarkOnlyThis);

        // Flex items that were previously stretching need to be relayed out so we
        // can compute new available cross axis space. This is only necessary for
        // stretching since other alignment values don't change the size of the
        // box.
        if (oldStyleAlignItemsIsStretch) {
            ItemPosition previousAlignment = flexItem.style().resolvedAlignSelf(oldStyle, selfAlignmentNormalBehavior()).position();
            if (previousAlignment == ItemPosition::Stretch && previousAlignment != flexItem.style().resolvedAlignSelf(&style(), selfAlignmentNormalBehavior()).position())
                flexItem.setChildNeedsLayout(MarkOnlyThis);
        }
    }
}

bool RenderFlexibleBox::hitTestChildren(const HitTestRequest& request, HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& adjustedLocation, HitTestAction hitTestAction)
{
    if (hitTestAction != HitTestForeground)
        return false;

    LayoutPoint scrolledOffset = hasNonVisibleOverflow() ? adjustedLocation - toLayoutSize(scrollPosition()) : adjustedLocation;

    // If collecting the children in reverse order is bad for performance, this Vector could be determined at layout time.
    Vector<RenderBox*> reversedOrderIteratorForHitTesting;
    for (auto* child = m_orderIterator.first(); child; child = m_orderIterator.next()) {
        if (m_orderIterator.shouldSkipChild(*child))
            continue;
        reversedOrderIteratorForHitTesting.append(child);
    }
    reversedOrderIteratorForHitTesting.reverse();

    for (auto* child : reversedOrderIteratorForHitTesting) {
        if (child->hasSelfPaintingLayer())
            continue;
        auto childPoint = flipForWritingModeForChild(*child, scrolledOffset);
        if (child->hitTest(request, result, locationInContainer, childPoint)) {
            updateHitTestResult(result, flipForWritingMode(toLayoutPoint(locationInContainer.point() - adjustedLocation)));
            return true;
        }
    }

    return false;
}

void RenderFlexibleBox::layoutBlock(bool relayoutChildren, LayoutUnit)
{
    ASSERT(needsLayout());

    if (!relayoutChildren && simplifiedLayout())
        return;

    LayoutRepainter repainter(*this, checkForRepaintDuringLayout());

    resetLogicalHeightBeforeLayoutIfNeeded();
    m_relaidOutChildren.clear();
    
    bool oldInLayout = m_inLayout;
    m_inLayout = true;

    if (!style().marginTrim().isEmpty())
        initializeMarginTrimState();

    if (recomputeLogicalWidth())
        relayoutChildren = true;

    LayoutUnit previousHeight = logicalHeight();
    setLogicalHeight(borderAndPaddingLogicalHeight() + scrollbarLogicalHeight());
    {
        LayoutStateMaintainer statePusher(*this, locationOffset(), isTransformed() || hasReflection() || style().isFlippedBlocksWritingMode());

        preparePaginationBeforeBlockLayout(relayoutChildren);

        m_numberOfInFlowChildrenOnFirstLine = { };
        m_numberOfInFlowChildrenOnLastLine = { };

        beginUpdateScrollInfoAfterLayoutTransaction();

        prepareOrderIteratorAndMargins();

        // Fieldsets need to find their legend and position it inside the border of the object.
        // The legend then gets skipped during normal layout. The same is true for ruby text.
        // It doesn't get included in the normal layout process but is instead skipped.
        layoutExcludedChildren(relayoutChildren);

        ChildFrameRects oldChildRects;
        appendChildFrameRects(oldChildRects);

        layoutFlexItems(relayoutChildren);

        endAndCommitUpdateScrollInfoAfterLayoutTransaction();

        if (logicalHeight() != previousHeight)
            relayoutChildren = true;

        layoutPositionedObjects(relayoutChildren || isDocumentElementRenderer());

        repaintChildrenDuringLayoutIfMoved(oldChildRects);
        // FIXME: css3/flexbox/repaint-rtl-column.html seems to repaint more overflow than it needs to.
        computeOverflow(layoutOverflowLogicalBottom(*this));

        updateDescendantTransformsAfterLayout();
    }
    updateLayerTransform();

    // We have to reset this, because changes to our ancestors' style can affect
    // this value. Also, this needs to be before we call updateAfterLayout, as
    // that function may re-enter this one.
    resetHasDefiniteHeight();

    // Update our scroll information if we're overflow:auto/scroll/hidden now that we know if we overflow or not.
    updateScrollInfoAfterLayout();

    repainter.repaintAfterLayout();

    clearNeedsLayout();
    
    m_inLayout = oldInLayout;
}

void RenderFlexibleBox::appendChildFrameRects(ChildFrameRects& childFrameRects)
{
    for (RenderBox* child = m_orderIterator.first(); child; child = m_orderIterator.next()) {
        if (!child->isOutOfFlowPositioned())
            childFrameRects.append(child->frameRect());
    }
}

void RenderFlexibleBox::repaintChildrenDuringLayoutIfMoved(const ChildFrameRects& oldChildRects)
{
    size_t childIndex = 0;
    for (RenderBox* child = m_orderIterator.first(); child; child = m_orderIterator.next()) {
        if (child->isOutOfFlowPositioned())
            continue;

        // If the child moved, we have to repaint it as well as any floating/positioned
        // descendants. An exception is if we need a layout. In this case, we know we're going to
        // repaint ourselves (and the child) anyway.
        if (!selfNeedsLayout() && child->checkForRepaintDuringLayout())
            child->repaintDuringLayoutIfMoved(oldChildRects[childIndex]);
        ++childIndex;
    }
    ASSERT(childIndex == oldChildRects.size());
}

void RenderFlexibleBox::paintChildren(PaintInfo& paintInfo, const LayoutPoint& paintOffset, PaintInfo& paintInfoForChild, bool usePrintRect)
{
    for (RenderBox* child = m_orderIterator.first(); child; child = m_orderIterator.next()) {
        if (!paintChild(*child, paintInfo, paintOffset, paintInfoForChild, usePrintRect, PaintAsInlineBlock))
            return;
    }
}

void RenderFlexibleBox::repositionLogicalHeightDependentFlexItems(FlexLineStates& lineStates, LayoutUnit gapBetweenLines)
{
    LayoutUnit crossAxisStartEdge = lineStates.isEmpty() ? 0_lu : lineStates[0].crossAxisOffset;
    // If we have a single line flexbox, the line height is all the available space. For flex-direction: row,
    // this means we need to use the height, so we do this after calling updateLogicalHeight.
    if (!isMultiline() && !lineStates.isEmpty())
        lineStates[0].crossAxisExtent = crossAxisContentExtent();

    alignFlexLines(lineStates, gapBetweenLines);

    alignChildren(lineStates);
    
    if (style().flexWrap() == FlexWrap::Reverse)
        flipForWrapReverse(lineStates, crossAxisStartEdge);
    
    // direction:rtl + flex-direction:column means the cross-axis direction is
    // flipped.
    flipForRightToLeftColumn(lineStates);
}

bool RenderFlexibleBox::mainAxisIsChildInlineAxis(const RenderBox& child) const
{
    return isHorizontalFlow() == child.isHorizontalWritingMode();
}

bool RenderFlexibleBox::isColumnFlow() const
{
    return style().isColumnFlexDirection();
}

bool RenderFlexibleBox::isColumnOrRowReverse() const
{
    return style().flexDirection() == FlexDirection::ColumnReverse || style().flexDirection() == FlexDirection::RowReverse;
}

bool RenderFlexibleBox::isHorizontalFlow() const
{
    if (isHorizontalWritingMode())
        return !isColumnFlow();
    return isColumnFlow();
}

bool RenderFlexibleBox::isLeftToRightFlow() const
{
    if (isColumnFlow())
        return style().blockFlowDirection() == BlockFlowDirection::TopToBottom || style().blockFlowDirection() == BlockFlowDirection::LeftToRight;
    return style().isLeftToRightDirection() ^ (style().flexDirection() == FlexDirection::RowReverse);
}

bool RenderFlexibleBox::isMultiline() const
{
    return style().flexWrap() != FlexWrap::NoWrap;
}

// https://drafts.csswg.org/css-flexbox/#min-size-auto
bool RenderFlexibleBox::shouldApplyMinSizeAutoForChild(const RenderBox& child) const
{
    auto minSize = mainSizeLengthForChild(MinSize, child);
    // min, max and fit-content are equivalent to the automatic size for block sizes https://drafts.csswg.org/css-sizing-3/#valdef-width-min-content.
    bool childBlockSizeIsEquivalentToAutomaticSize  = !mainAxisIsChildInlineAxis(child) && (minSize.isMinContent() || minSize.isMaxContent() || minSize.isFitContent());

    return (minSize.isAuto() || childBlockSizeIsEquivalentToAutomaticSize) && (mainAxisOverflowForChild(child) == Overflow::Visible);
}

bool RenderFlexibleBox::shouldApplyMinBlockSizeAutoForChild(const RenderBox& child) const
{
    return !mainAxisIsChildInlineAxis(child) && shouldApplyMinSizeAutoForChild(child);
}

Length RenderFlexibleBox::flexBasisForChild(const RenderBox& child) const
{
    Length flexLength = child.style().flexBasis();
    if (flexLength.isAuto())
        flexLength = mainSizeLengthForChild(MainOrPreferredSize, child);
    return flexLength;
}

LayoutUnit RenderFlexibleBox::crossAxisExtentForChild(const RenderBox& child) const
{
    return isHorizontalFlow() ? child.height() : child.width();
}

LayoutUnit RenderFlexibleBox::cachedChildIntrinsicContentLogicalHeight(const RenderBox& child) const
{
    if (auto* renderReplaced = dynamicDowncast<RenderReplaced>(child))
        return renderReplaced->intrinsicLogicalHeight();
    
    if (m_intrinsicContentLogicalHeights.contains(child))
        return m_intrinsicContentLogicalHeights.get(child);
    
    return child.contentLogicalHeight();
}

void RenderFlexibleBox::setCachedChildIntrinsicContentLogicalHeight(const RenderBox& child, LayoutUnit height)
{
    if (child.isRenderReplaced())
        return; // Replaced elements know their intrinsic height already, so save space by not caching.
    m_intrinsicContentLogicalHeights.set(child, height);
}

void RenderFlexibleBox::clearCachedChildIntrinsicContentLogicalHeight(const RenderBox& child)
{
    if (child.isRenderReplaced())
        return; // Replaced elements know their intrinsic height already, so nothing to do.
    m_intrinsicContentLogicalHeights.remove(child);
}

LayoutUnit RenderFlexibleBox::childIntrinsicLogicalHeight(RenderBox& child) const
{
    // This should only be called if the logical height is the cross size
    ASSERT(mainAxisIsChildInlineAxis(child));
    if (needToStretchChildLogicalHeight(child)) {
        LayoutUnit childContentHeight = cachedChildIntrinsicContentLogicalHeight(child);
        LayoutUnit childLogicalHeight = childContentHeight + child.scrollbarLogicalHeight() + child.borderAndPaddingLogicalHeight();
        return child.constrainLogicalHeightByMinMax(childLogicalHeight, childContentHeight);
    }
    return child.logicalHeight();
}

LayoutUnit RenderFlexibleBox::childIntrinsicLogicalWidth(RenderBox& child)
{
    // This should only be called if the logical width is the cross size
    ASSERT(!mainAxisIsChildInlineAxis(child));
    if (childCrossSizeIsDefinite(child, child.style().logicalWidth()))
        return child.logicalWidth();

    LogicalExtentComputedValues values;
    {
        OverridingSizesScope cleanOverridingWidthScope(child, OverridingSizesScope::Axis::Inline);
        child.computeLogicalWidthInFragment(values);
    }
    return values.m_extent;
}

LayoutUnit RenderFlexibleBox::crossAxisIntrinsicExtentForChild(RenderBox& child)
{
    return mainAxisIsChildInlineAxis(child) ? childIntrinsicLogicalHeight(child) : childIntrinsicLogicalWidth(child);
}

LayoutUnit RenderFlexibleBox::mainAxisExtentForChild(const RenderBox& child) const
{
    return isHorizontalFlow() ? child.size().width() : child.size().height();
}

LayoutUnit RenderFlexibleBox::mainAxisContentExtentForChildIncludingScrollbar(const RenderBox& child) const
{
    return isHorizontalFlow() ? child.contentWidth() + child.verticalScrollbarWidth() : child.contentHeight() + child.horizontalScrollbarHeight();
}

LayoutUnit RenderFlexibleBox::crossAxisExtent() const
{
    return isHorizontalFlow() ? size().height() : size().width();
}

LayoutUnit RenderFlexibleBox::mainAxisExtent() const
{
    return isHorizontalFlow() ? size().width() : size().height();
}

LayoutUnit RenderFlexibleBox::crossAxisContentExtent() const
{
    return isHorizontalFlow() ? contentHeight() : contentWidth();
}

LayoutUnit RenderFlexibleBox::mainAxisContentExtent(LayoutUnit contentLogicalHeight)
{
    if (!isColumnFlow())
        return contentLogicalWidth();

    LayoutUnit borderPaddingAndScrollbar = borderAndPaddingLogicalHeight() + scrollbarLogicalHeight();
    LayoutUnit borderBoxLogicalHeight = contentLogicalHeight + borderPaddingAndScrollbar;
    auto computedValues = computeLogicalHeight(borderBoxLogicalHeight, logicalTop());
    if (computedValues.m_extent == LayoutUnit::max())
        return computedValues.m_extent;
    return std::max(0_lu, computedValues.m_extent - borderPaddingAndScrollbar);
}

// FIXME: consider adding this check to RenderBox::hasIntrinsicAspectRatio(). We could even make it
// virtual returning false by default. RenderReplaced will overwrite it with the current implementation
// plus this extra check. See wkb.ug/231955.
static bool isSVGRootWithIntrinsicAspectRatio(const RenderBox& child)
{
    if (!child.isRenderOrLegacyRenderSVGRoot())
        return false;
    // It's common for some replaced elements, such as SVGs, to have intrinsic aspect ratios but no intrinsic sizes.
    // That's why it isn't enough just to check for intrinsic sizes in those cases.
    return downcast<RenderReplaced>(child).computeIntrinsicAspectRatio() > 0;
};

static bool childHasAspectRatio(const RenderBox& child)
{
    return child.hasIntrinsicAspectRatio() || child.style().hasAspectRatio() || isSVGRootWithIntrinsicAspectRatio(child);
}

std::optional<LayoutUnit> RenderFlexibleBox::computeMainAxisExtentForChild(RenderBox& child, SizeType sizeType, const Length& size)
{
    // If we have a horizontal flow, that means the main size is the width.
    // That's the logical width for horizontal writing modes, and the logical
    // height in vertical writing modes. For a vertical flow, main size is the
    // height, so it's the inverse. So we need the logical width if we have a
    // horizontal flow and horizontal writing mode, or vertical flow and vertical
    // writing mode. Otherwise we need the logical height.
    if (!mainAxisIsChildInlineAxis(child)) {
        // We don't have to check for "auto" here - computeContentLogicalHeight
        // will just return a null Optional for that case anyway. It's safe to access
        // scrollbarLogicalHeight here because ComputeNextFlexLine will have
        // already forced layout on the child. We previously did a layout out the child
        // if necessary (see ComputeNextFlexLine and the call to
        // childHasIntrinsicMainAxisSize) so we can be sure that the two height
        // calls here will return up-to-date data.
        std::optional<LayoutUnit> height = child.computeContentLogicalHeight(sizeType, size, cachedChildIntrinsicContentLogicalHeight(child));
        if (!height)
            return height;
        // Tables interpret overriding sizes as the size of captions + rows. However the specified height of a table
        // only includes the size of the rows. That's why we need to add the size of the captions here so that the table
        // layout algorithm behaves appropiately.
        LayoutUnit captionsHeight;
        if (CheckedPtr table = dynamicDowncast<RenderTable>(child); table && childMainSizeIsDefinite(child, size))
            captionsHeight = table->sumCaptionsLogicalHeight();
        return *height + child.scrollbarLogicalHeight() + captionsHeight;
    }

    // computeLogicalWidth always re-computes the intrinsic widths. However, when
    // our logical width is auto, we can just use our cached value. So let's do
    // that here. (Compare code in RenderBlock::computePreferredLogicalWidths)
    if (child.style().logicalWidth().isAuto() && !childHasAspectRatio(child)) {
        if (size.isMinContent()) {
            if (child.needsPreferredWidthsRecalculation())
                child.setPreferredLogicalWidthsDirty(true, MarkOnlyThis);
            return child.minPreferredLogicalWidth() - child.borderAndPaddingLogicalWidth();
        }
        if (size.isMaxContent()) {
            if (child.needsPreferredWidthsRecalculation())
                child.setPreferredLogicalWidthsDirty(true, MarkOnlyThis);
            return child.maxPreferredLogicalWidth() - child.borderAndPaddingLogicalWidth();
        }
    }
    
    // FIXME: Figure out how this should work for regions and pass in the appropriate values.
    RenderFragmentContainer* fragment = nullptr;
    return child.computeLogicalWidthInFragmentUsing(sizeType, size, contentLogicalWidth(), *this, fragment) - child.borderAndPaddingLogicalWidth();
}

BlockFlowDirection RenderFlexibleBox::transformedBlockFlowDirection() const
{
    auto blockFlowDirection = style().blockFlowDirection();
    if (!isColumnFlow())
        return blockFlowDirection;
    
    switch (blockFlowDirection) {
    case BlockFlowDirection::TopToBottom:
    case BlockFlowDirection::BottomToTop:
        return style().isLeftToRightDirection() ? BlockFlowDirection::LeftToRight : BlockFlowDirection::RightToLeft;
    case BlockFlowDirection::LeftToRight:
    case BlockFlowDirection::RightToLeft:
        return style().isLeftToRightDirection() ? BlockFlowDirection::TopToBottom : BlockFlowDirection::BottomToTop;
    }
    ASSERT_NOT_REACHED();
    return BlockFlowDirection::TopToBottom;
}

LayoutUnit RenderFlexibleBox::flowAwareBorderStart() const
{
    if (isHorizontalFlow())
        return isLeftToRightFlow() ? borderLeft() : borderRight();
    return isLeftToRightFlow() ? borderTop() : borderBottom();
}

LayoutUnit RenderFlexibleBox::flowAwareBorderEnd() const
{
    if (isHorizontalFlow())
        return isLeftToRightFlow() ? borderRight() : borderLeft();
    return isLeftToRightFlow() ? borderBottom() : borderTop();
}

LayoutUnit RenderFlexibleBox::flowAwareBorderBefore() const
{
    switch (transformedBlockFlowDirection()) {
    case BlockFlowDirection::TopToBottom:
        return borderTop();
    case BlockFlowDirection::BottomToTop:
        return borderBottom();
    case BlockFlowDirection::LeftToRight:
        return borderLeft();
    case BlockFlowDirection::RightToLeft:
        return borderRight();
    }
    ASSERT_NOT_REACHED();
    return borderTop();
}

LayoutUnit RenderFlexibleBox::flowAwareBorderAfter() const
{
    switch (transformedBlockFlowDirection()) {
    case BlockFlowDirection::TopToBottom:
        return borderBottom();
    case BlockFlowDirection::BottomToTop:
        return borderTop();
    case BlockFlowDirection::LeftToRight:
        return borderRight();
    case BlockFlowDirection::RightToLeft:
        return borderLeft();
    }
    ASSERT_NOT_REACHED();
    return borderTop();
}

LayoutUnit RenderFlexibleBox::flowAwarePaddingStart() const
{
    if (isHorizontalFlow())
        return isLeftToRightFlow() ? paddingLeft() : paddingRight();
    return isLeftToRightFlow() ? paddingTop() : paddingBottom();
}

LayoutUnit RenderFlexibleBox::flowAwarePaddingEnd() const
{
    if (isHorizontalFlow())
        return isLeftToRightFlow() ? paddingRight() : paddingLeft();
    return isLeftToRightFlow() ? paddingBottom() : paddingTop();
}

LayoutUnit RenderFlexibleBox::flowAwarePaddingBefore() const
{
    switch (transformedBlockFlowDirection()) {
    case BlockFlowDirection::TopToBottom:
        return paddingTop();
    case BlockFlowDirection::BottomToTop:
        return paddingBottom();
    case BlockFlowDirection::LeftToRight:
        return paddingLeft();
    case BlockFlowDirection::RightToLeft:
        return paddingRight();
    }
    ASSERT_NOT_REACHED();
    return paddingTop();
}

LayoutUnit RenderFlexibleBox::flowAwarePaddingAfter() const
{
    switch (transformedBlockFlowDirection()) {
    case BlockFlowDirection::TopToBottom:
        return paddingBottom();
    case BlockFlowDirection::BottomToTop:
        return paddingTop();
    case BlockFlowDirection::LeftToRight:
        return paddingRight();
    case BlockFlowDirection::RightToLeft:
        return paddingLeft();
    }
    ASSERT_NOT_REACHED();
    return paddingTop();
}

LayoutUnit RenderFlexibleBox::flowAwareMarginStartForChild(const RenderBox& child) const
{
    if (isHorizontalFlow())
        return isLeftToRightFlow() ? child.marginLeft() : child.marginRight();
    return isLeftToRightFlow() ? child.marginTop() : child.marginBottom();
}

LayoutUnit RenderFlexibleBox::flowAwareMarginEndForChild(const RenderBox& child) const
{
    if (isHorizontalFlow())
        return isLeftToRightFlow() ? child.marginRight() : child.marginLeft();
    return isLeftToRightFlow() ? child.marginBottom() : child.marginTop();
}

LayoutUnit RenderFlexibleBox::flowAwareMarginBeforeForChild(const RenderBox& child) const
{
    switch (transformedBlockFlowDirection()) {
    case BlockFlowDirection::TopToBottom:
        return child.marginTop();
    case BlockFlowDirection::BottomToTop:
        return child.marginBottom();
    case BlockFlowDirection::LeftToRight:
        return child.marginLeft();
    case BlockFlowDirection::RightToLeft:
        return child.marginRight();
    }
    ASSERT_NOT_REACHED();
    return marginTop();
}

void RenderFlexibleBox::initializeMarginTrimState()
{
    // When computeIntrinsicLogicalWidth goes through each of the children, it
    // will include the margins when computing the flexbox's min and max widths.
    // We need to trim the margins of the first and last child early so that
    // these margins do not incorrectly constribute to the box's min/max width
    auto marginTrim = style().marginTrim();
    auto isRowsFlexbox = isHorizontalFlow();
    if (auto child = firstInFlowChildBox(); child && marginTrim.contains(MarginTrimType::InlineStart))
        isRowsFlexbox ? m_marginTrimItems.m_itemsAtFlexLineStart.add(*child) : m_marginTrimItems.m_itemsOnFirstFlexLine.add(*child);
    if (auto child = lastInFlowChildBox(); child && marginTrim.contains(MarginTrimType::InlineEnd))
        isRowsFlexbox ? m_marginTrimItems.m_itemsAtFlexLineEnd.add(*child) : m_marginTrimItems.m_itemsOnLastFlexLine.add(*child);
}

LayoutUnit RenderFlexibleBox::mainAxisMarginExtentForChild(const RenderBox& child) const
{
    if (!child.needsLayout())
        return isHorizontalFlow() ? child.horizontalMarginExtent() : child.verticalMarginExtent();

    LayoutUnit marginStart;
    LayoutUnit marginEnd;
    if (isHorizontalFlow())
        child.computeInlineDirectionMargins(*this, child.containingBlockLogicalWidthForContentInFragment(nullptr), child.logicalWidth(), { }, marginStart, marginEnd);
    else
        child.computeBlockDirectionMargins(*this, marginStart, marginEnd);
    return marginStart + marginEnd;
}

LayoutUnit RenderFlexibleBox::crossAxisMarginExtentForChild(const RenderBox& child) const
{
    if (!child.needsLayout())
        return isHorizontalFlow() ? child.verticalMarginExtent() : child.horizontalMarginExtent();

    LayoutUnit marginStart;
    LayoutUnit marginEnd;
    if (isHorizontalFlow())
        child.computeBlockDirectionMargins(*this, marginStart, marginEnd);
    else
        child.computeInlineDirectionMargins(*this, child.containingBlockLogicalWidthForContentInFragment(nullptr), child.logicalWidth(), { }, marginStart, marginEnd);
    return marginStart + marginEnd;
}

bool RenderFlexibleBox::isChildEligibleForMarginTrim(MarginTrimType marginTrimType, const RenderBox& child) const
{
    ASSERT(style().marginTrim().contains(marginTrimType));
    auto isMarginParallelWithMainAxis = [this](MarginTrimType marginTrimType) {
        if (isHorizontalFlow())
            return marginTrimType == MarginTrimType::BlockStart || marginTrimType == MarginTrimType::BlockEnd;
        return marginTrimType == MarginTrimType::InlineStart || marginTrimType == MarginTrimType::InlineEnd;
    };
    if (isMarginParallelWithMainAxis(marginTrimType))
        return (marginTrimType == MarginTrimType::BlockStart || marginTrimType == MarginTrimType::InlineStart) ? m_marginTrimItems.m_itemsOnFirstFlexLine.contains(child) : m_marginTrimItems.m_itemsOnLastFlexLine.contains(child);
    return (marginTrimType == MarginTrimType::BlockStart || marginTrimType == MarginTrimType::InlineStart) ? m_marginTrimItems.m_itemsAtFlexLineStart.contains(child) : m_marginTrimItems.m_itemsAtFlexLineEnd.contains(child);
}

bool RenderFlexibleBox::shouldTrimMainAxisMarginStart() const
{
    if (isHorizontalFlow())
        return style().marginTrim().contains(MarginTrimType::InlineStart);
    return style().marginTrim().contains(MarginTrimType::BlockStart);
}

bool RenderFlexibleBox::shouldTrimMainAxisMarginEnd() const
{
    if (isHorizontalFlow())
        return style().marginTrim().contains(MarginTrimType::InlineEnd);
    return style().marginTrim().contains(MarginTrimType::BlockEnd);
}

bool RenderFlexibleBox::shouldTrimCrossAxisMarginStart() const
{
    if (isHorizontalFlow())
        return style().marginTrim().contains(MarginTrimType::BlockStart);
    return style().marginTrim().contains(MarginTrimType::InlineStart);
}

bool RenderFlexibleBox::shouldTrimCrossAxisMarginEnd() const
{
    if (isHorizontalFlow())
        return style().marginTrim().contains(MarginTrimType::BlockEnd);
    return style().marginTrim().contains(MarginTrimType::InlineEnd);
}

void RenderFlexibleBox::trimMainAxisMarginStart(const FlexItem& flexItem)
{
    auto horizontalFlow = isHorizontalFlow();
    flexItem.mainAxisMargin -= horizontalFlow ? flexItem.box->marginStart(&style()) : flexItem.box->marginBefore(&style());
    if (horizontalFlow)
        setTrimmedMarginForChild(flexItem.box, MarginTrimType::InlineStart);
    else
        setTrimmedMarginForChild(flexItem.box, MarginTrimType::BlockStart);
    m_marginTrimItems.m_itemsAtFlexLineStart.add(flexItem.box);
}

void RenderFlexibleBox::trimMainAxisMarginEnd(const FlexItem& flexItem)
{
    auto horizontalFlow = isHorizontalFlow();
    flexItem.mainAxisMargin -= horizontalFlow ? flexItem.box->marginEnd(&style()) : flexItem.box->marginAfter(&style());
    if (horizontalFlow)
        setTrimmedMarginForChild(flexItem.box, MarginTrimType::InlineEnd);
    else
        setTrimmedMarginForChild(flexItem.box, MarginTrimType::BlockEnd);
    m_marginTrimItems.m_itemsAtFlexLineEnd.add(flexItem.box);
}

void RenderFlexibleBox::trimCrossAxisMarginStart(const FlexItem& flexItem)
{
    if (isHorizontalFlow())
        setTrimmedMarginForChild(flexItem.box, MarginTrimType::BlockStart);
    else
        setTrimmedMarginForChild(flexItem.box, MarginTrimType::InlineStart);
    m_marginTrimItems.m_itemsOnFirstFlexLine.add(flexItem.box);
}

void RenderFlexibleBox::trimCrossAxisMarginEnd(const FlexItem& flexItem)
{
    if (isHorizontalFlow())
        setTrimmedMarginForChild(flexItem.box, MarginTrimType::BlockEnd);
    else
        setTrimmedMarginForChild(flexItem.box, MarginTrimType::InlineEnd);
    m_marginTrimItems.m_itemsOnLastFlexLine.add(flexItem.box);
}

LayoutUnit RenderFlexibleBox::crossAxisScrollbarExtent() const
{
    return isHorizontalFlow() ? horizontalScrollbarHeight() : verticalScrollbarWidth();
}

LayoutPoint RenderFlexibleBox::flowAwareLocationForChild(const RenderBox& child) const
{
    return isHorizontalFlow() ? child.location() : child.location().transposedPoint();
}

Length RenderFlexibleBox::crossSizeLengthForChild(SizeType sizeType, const RenderBox& child) const
{
    switch (sizeType) {
    case MinSize:
        return isHorizontalFlow() ? child.style().minHeight() : child.style().minWidth();
    case MainOrPreferredSize:
        return isHorizontalFlow() ? child.style().height() : child.style().width();
    case MaxSize:
        return isHorizontalFlow() ? child.style().maxHeight() : child.style().maxWidth();
    }
    ASSERT_NOT_REACHED();
    return { };
}

Length RenderFlexibleBox::mainSizeLengthForChild(SizeType sizeType, const RenderBox& child) const
{
    switch (sizeType) {
    case MinSize:
        return isHorizontalFlow() ? child.style().minWidth() : child.style().minHeight();
    case MainOrPreferredSize:
        return isHorizontalFlow() ? child.style().width() : child.style().height();
    case MaxSize:
        return isHorizontalFlow() ? child.style().maxWidth() : child.style().maxHeight();
    }
    ASSERT_NOT_REACHED();
    return { };
}

// FIXME: computeMainSizeFromAspectRatioUsing may need to return an std::optional<LayoutUnit> in the future
// rather than returning indefinite sizes as 0/-1.
LayoutUnit RenderFlexibleBox::computeMainSizeFromAspectRatioUsing(const RenderBox& child, Length crossSizeLength) const
{
    ASSERT(childHasAspectRatio(child));

    auto adjustForBoxSizing = [this] (const RenderBox& box, LayoutUnit value) -> LayoutUnit {
        // We need to substract the border and padding extent from the cross axis.
        // Furthermore, the sizing calculations that floor the content box size at zero when applying box-sizing are also ignored.
        // https://drafts.csswg.org/css-flexbox/#algo-main-item.
        if (box.style().boxSizing() == BoxSizing::BorderBox)
            value -= isHorizontalFlow() ? box.verticalBorderAndPaddingExtent() : box.horizontalBorderAndPaddingExtent();
        return value;
    };

    LayoutUnit crossSize;
    // crossSize is border-box size if box-sizing is border-box, and content-box otherwise.
    if (crossSizeLength.isFixed())
        crossSize = LayoutUnit(crossSizeLength.value());
    else if (crossSizeLength.isAuto()) {
        ASSERT(childCrossSizeShouldUseContainerCrossSize(child));
        crossSize = computeCrossSizeForChildUsingContainerCrossSize(child);
    } else {
        ASSERT(crossSizeLength.isPercentOrCalculated());
        auto childSize = mainAxisIsChildInlineAxis(child) ? child.computePercentageLogicalHeight(crossSizeLength) : adjustBorderBoxLogicalWidthForBoxSizing(valueForLength(crossSizeLength, contentWidth()), crossSizeLength.type());
        if (!childSize)
            return 0_lu;
        crossSize = childSize.value();
    }

    double ratio;
    LayoutUnit borderAndPadding;
    if (child.isRenderOrLegacyRenderSVGRoot())
        ratio = downcast<RenderReplaced>(child).computeIntrinsicAspectRatio();
    else {
        auto childIntrinsicSize = child.intrinsicSize();
        if (child.style().aspectRatioType() == AspectRatioType::Ratio || (child.style().aspectRatioType() == AspectRatioType::AutoAndRatio && childIntrinsicSize.isEmpty())) {
            ratio = child.style().aspectRatioWidth() / child.style().aspectRatioHeight();
            if (child.style().boxSizingForAspectRatio() == BoxSizing::ContentBox)
                crossSize -= isHorizontalFlow() ? child.verticalBorderAndPaddingExtent() : child.horizontalBorderAndPaddingExtent();
            else
                borderAndPadding = isHorizontalFlow() ? child.horizontalBorderAndPaddingExtent() : child.verticalBorderAndPaddingExtent();
        } else {
            if (auto* replacedElement = dynamicDowncast<RenderReplaced>(child))
                ratio = replacedElement->computeIntrinsicAspectRatio();
            else {
                ASSERT(childIntrinsicSize.height());
                ratio = childIntrinsicSize.width().toFloat() / childIntrinsicSize.height().toFloat();
            }
            crossSize = adjustForBoxSizing(child, crossSize);
        }
    }
    if (isHorizontalFlow())
        return std::max(0_lu, LayoutUnit(crossSize * ratio) - borderAndPadding);
    return std::max(0_lu, LayoutUnit(crossSize / ratio) - borderAndPadding);
}

void RenderFlexibleBox::setFlowAwareLocationForChild(RenderBox& child, const LayoutPoint& location)
{
    if (isHorizontalFlow())
        child.setLocation(location);
    else
        child.setLocation(location.transposedPoint());
}

bool RenderFlexibleBox::canComputePercentageFlexBasis(const RenderBox& child, const Length& flexBasis, UpdatePercentageHeightDescendants updateDescendants)
{
    if (!isColumnFlow() || m_hasDefiniteHeight == SizeDefiniteness::Definite)
        return true;
    if (m_hasDefiniteHeight == SizeDefiniteness::Indefinite)
        return false;
    bool definite = child.computePercentageLogicalHeight(flexBasis, updateDescendants).has_value();
    if (m_inLayout && (isHorizontalWritingMode() == child.isHorizontalWritingMode())) {
        // We can reach this code even while we're not laying ourselves out, such
        // as from mainSizeForPercentageResolution.
        m_hasDefiniteHeight = definite ? SizeDefiniteness::Definite : SizeDefiniteness::Indefinite;
    }
    return definite;
}

bool RenderFlexibleBox::childMainSizeIsDefinite(const RenderBox& child, const Length& flexBasis)
{
    if (flexBasis.isAuto() || flexBasis.isContent())
        return false;
    if (!mainAxisIsChildInlineAxis(child) && (flexBasis.isIntrinsic() || flexBasis.type() == LengthType::Intrinsic))
        return false;
    if (flexBasis.isPercentOrCalculated())
        return canComputePercentageFlexBasis(child, flexBasis, UpdatePercentageHeightDescendants::No);
    return true;
}

bool RenderFlexibleBox::childHasComputableAspectRatio(const RenderBox& child) const
{
    if (!childHasAspectRatio(child))
        return false;
    return child.intrinsicSize().height() || child.style().hasAspectRatio() || isSVGRootWithIntrinsicAspectRatio(child);
}

bool RenderFlexibleBox::childHasComputableAspectRatioAndCrossSizeIsConsideredDefinite(const RenderBox& child)
{
    return childHasComputableAspectRatio(child)
        && (childCrossSizeIsDefinite(child, crossSizeLengthForChild(MainOrPreferredSize, child)) || childCrossSizeShouldUseContainerCrossSize(child));
}

bool RenderFlexibleBox::crossAxisIsPhysicalWidth() const
{
    return (isHorizontalWritingMode() && isColumnFlow()) || (!isHorizontalWritingMode() && !isColumnFlow());
}

bool RenderFlexibleBox::childCrossSizeShouldUseContainerCrossSize(const RenderBox& child) const
{
    // 9.8 https://drafts.csswg.org/css-flexbox/#definite-sizes
    // 1. If a single-line flex container has a definite cross size, the automatic preferred outer cross size of any
    // stretched flex items is the flex container's inner cross size (clamped to the flex item's min and max cross size)
    // and is considered definite.
    if (!isMultiline() && alignmentForChild(child) == ItemPosition::Stretch && !hasAutoMarginsInCrossAxis(child) && crossSizeLengthForChild(MainOrPreferredSize, child).isAuto()) {
        if (crossAxisIsPhysicalWidth())
            return true;
        // This must be kept in sync with computeMainSizeFromAspectRatioUsing().
        auto& crossSize = isHorizontalFlow() ? style().height() : style().width();
        return crossSize.isFixed() || (crossSize.isPercent() && availableLogicalHeightForPercentageComputation());  
    }
    return false;
}

bool RenderFlexibleBox::childCrossSizeIsDefinite(const RenderBox& child, const Length& length)
{
    if (length.isAuto())
        return false;

    if (length.isPercentOrCalculated()) {
        if (!mainAxisIsChildInlineAxis(child) || m_hasDefiniteHeight == SizeDefiniteness::Definite)
            return true;
        if (m_hasDefiniteHeight == SizeDefiniteness::Indefinite)
            return false;
        bool definite = bool(child.computePercentageLogicalHeight(length));
        m_hasDefiniteHeight = definite ? SizeDefiniteness::Definite : SizeDefiniteness::Indefinite;
        return definite;
    }
    // FIXME: Eventually we should support other types of sizes here.
    // Requires updating computeMainSizeFromAspectRatioUsing.
    return length.isFixed();
}

void RenderFlexibleBox::cacheChildMainSize(const RenderBox& child)
{
    ASSERT(!child.needsLayout());
    LayoutUnit mainSize;
    if (mainAxisIsChildInlineAxis(child))
        mainSize = child.maxPreferredLogicalWidth();
    else {
        auto flexBasis = flexBasisForChild(child);
        if (flexBasis.isPercentOrCalculated() && !childMainSizeIsDefinite(child, flexBasis))
            mainSize = cachedChildIntrinsicContentLogicalHeight(child) + child.borderAndPaddingLogicalHeight() + child.scrollbarLogicalHeight();
        else
            mainSize = child.logicalHeight();
    }
  
    m_intrinsicSizeAlongMainAxis.set(child, mainSize);
    m_relaidOutChildren.add(child);
}

void RenderFlexibleBox::clearCachedMainSizeForChild(const RenderBox& child)
{
    m_intrinsicSizeAlongMainAxis.remove(child);
}

// This is a RAII class that is used to temporarily set the flex basis as the child size in the main axis.
class ScopedFlexBasisAsChildMainSize {
public:
    ScopedFlexBasisAsChildMainSize(RenderBox& child, Length flexBasis, bool mainAxisIsInlineAxis)
        : m_child(child)
        , m_mainAxisIsInlineAxis(mainAxisIsInlineAxis)
    {
        if (m_mainAxisIsInlineAxis)
            m_child.setOverridingLogicalWidthLength(flexBasis);
        else
            m_child.setOverridingLogicalHeightLength(flexBasis);
    }
    ~ScopedFlexBasisAsChildMainSize()
    {
        if (m_mainAxisIsInlineAxis)
            m_child.clearOverridingLogicalWidthLength();
        else
            m_child.clearOverridingLogicalHeightLength();
    }

private:
    RenderBox& m_child;
    bool m_mainAxisIsInlineAxis;
};

// https://drafts.csswg.org/css-flexbox/#algo-main-item
LayoutUnit RenderFlexibleBox::computeFlexBaseSizeForChild(RenderBox& child, LayoutUnit mainAxisBorderAndPadding, bool relayoutChildren)
{
    Length flexBasis = flexBasisForChild(child);
    ScopedFlexBasisAsChildMainSize scoped(child, flexBasis.isContent() ? Length(LengthType::MaxContent) : flexBasis, mainAxisIsChildInlineAxis(child));

    maybeCacheChildMainIntrinsicSize(child, relayoutChildren);
    SetForScope<bool> computingBaseSizesScope(m_isComputingFlexBaseSizes, true);

    // 9.2.3 A.
    if (childMainSizeIsDefinite(child, flexBasis))
        return std::max(0_lu, computeMainAxisExtentForChild(child, MainOrPreferredSize, flexBasis).value());

    // 9.2.3 B.
    if (childHasComputableAspectRatioAndCrossSizeIsConsideredDefinite(child)) {
        const Length& crossSizeLength = crossSizeLengthForChild(MainOrPreferredSize, child);
        return adjustChildSizeForAspectRatioCrossAxisMinAndMax(child, computeMainSizeFromAspectRatioUsing(child, crossSizeLength));
    }

    // FIXME: 9.2.3 C.
    // FIXME: 9.2.3 D.

    // 9.2.3 E.
    LayoutUnit mainAxisExtent;
    if (!mainAxisIsChildInlineAxis(child)) {
        ASSERT(!child.needsLayout());
        ASSERT(m_intrinsicSizeAlongMainAxis.contains(child));
        mainAxisExtent = m_intrinsicSizeAlongMainAxis.get(child);
    } else {
        // We don't need to add scrollbarLogicalWidth here because the preferred
        // width includes the scrollbar, even for overflow: auto.
        mainAxisExtent = child.maxPreferredLogicalWidth();
    }
    return mainAxisExtent - mainAxisBorderAndPadding;
}

void RenderFlexibleBox::layoutFlexItems(bool relayoutChildren)
{
    if (LayoutIntegration::canUseForFlexLayout(*this))
        return layoutUsingFlexFormattingContext();
    FlexLineStates lineStates;
    LayoutUnit sumFlexBaseSize;
    double totalFlexGrow;
    double totalFlexShrink;
    double totalWeightedFlexShrink;
    LayoutUnit sumHypotheticalMainSize;
    
    // Set up our master list of flex items. All of the rest of the algorithm
    // should work off this list of a subset.
    // TODO(cbiesinger): That second part is not yet true.
    FlexItems allItems;
    m_orderIterator.first();
    for (RenderBox* child = m_orderIterator.currentChild(); child; child = m_orderIterator.next()) {
        if (m_orderIterator.shouldSkipChild(*child)) {
            // Out-of-flow children are not flex items, so we skip them here.
            if (child->isOutOfFlowPositioned())
                prepareChildForPositionedLayout(*child);
            continue;
        }
        allItems.append(constructFlexItem(*child, relayoutChildren));
        // constructFlexItem() might set the override containing block height so any value cached for definiteness might be incorrect.
        resetHasDefiniteHeight();
    }
    
    const LayoutUnit lineBreakLength = mainAxisContentExtent(LayoutUnit::max());
    LayoutUnit gapBetweenItems = computeGap(GapType::BetweenItems);
    LayoutUnit gapBetweenLines = computeGap(GapType::BetweenLines);
    FlexLayoutAlgorithm flexAlgorithm(*this, lineBreakLength, allItems, gapBetweenItems, gapBetweenLines);
    LayoutUnit crossAxisOffset = flowAwareBorderBefore() + flowAwarePaddingBefore();
    FlexItems lineItems;
    size_t nextIndex = 0;
    size_t numLines = 0;
    InspectorInstrumentation::flexibleBoxRendererBeganLayout(*this);
    while (flexAlgorithm.computeNextFlexLine(nextIndex, lineItems, sumFlexBaseSize, totalFlexGrow, totalFlexShrink, totalWeightedFlexShrink, sumHypotheticalMainSize)) {
        ++numLines;
        InspectorInstrumentation::flexibleBoxRendererWrappedToNextLine(*this, nextIndex);
        // Cross axis margins should only be trimmed if they are on the first/last flex line
        auto shouldTrimCrossAxisStart = shouldTrimCrossAxisMarginStart() && !lineStates.size();
        auto shouldTrimCrossAxisEnd = shouldTrimCrossAxisMarginEnd() && allItems.last().box.ptr() == lineItems.last().box.ptr();
        if (shouldTrimCrossAxisStart || shouldTrimCrossAxisEnd) {
            for (auto& flexItem : lineItems) {
                if (shouldTrimCrossAxisStart)
                    trimCrossAxisMarginStart(flexItem);
                if (shouldTrimCrossAxisEnd)
                    trimCrossAxisMarginEnd(flexItem);
            }
        }
        LayoutUnit containerMainInnerSize = mainAxisContentExtent(sumHypotheticalMainSize);
        // availableFreeSpace is the initial amount of free space in this flexbox.
        // remainingFreeSpace starts out at the same value but as we place and lay
        // out flex items we subtract from it. Note that both values can be
        // negative.
        LayoutUnit remainingFreeSpace = containerMainInnerSize - sumFlexBaseSize;
        FlexSign flexSign = (sumHypotheticalMainSize < containerMainInnerSize) ? FlexSign::PositiveFlexibility : FlexSign::NegativeFlexibility;
        freezeInflexibleItems(flexSign, lineItems, remainingFreeSpace, totalFlexGrow, totalFlexShrink, totalWeightedFlexShrink);
        // The initial free space gets calculated after freezing inflexible items.
        // https://drafts.csswg.org/css-flexbox/#resolve-flexible-lengths step 3
        const LayoutUnit initialFreeSpace = remainingFreeSpace;
        while (!resolveFlexibleLengths(flexSign, lineItems, initialFreeSpace, remainingFreeSpace, totalFlexGrow, totalFlexShrink, totalWeightedFlexShrink)) {
            ASSERT(totalFlexGrow >= 0);
            ASSERT(totalWeightedFlexShrink >= 0);
        }
        
        // Recalculate the remaining free space. The adjustment for flex factors
        // between 0..1 means we can't just use remainingFreeSpace here.
        remainingFreeSpace = containerMainInnerSize;
        for (size_t i = 0; i < lineItems.size(); ++i) {
            FlexItem& flexItem = lineItems[i];
            ASSERT(!flexItem.box->isOutOfFlowPositioned());
            remainingFreeSpace -= flexItem.flexedMarginBoxSize();
        }
        remainingFreeSpace -= (lineItems.size() - 1) * gapBetweenItems;

        // This will std::move lineItems into a newly-created LineState.
        layoutAndPlaceChildren(crossAxisOffset, lineItems, remainingFreeSpace, relayoutChildren, lineStates, gapBetweenItems);
    }

    if (hasLineIfEmpty()) {
        // Even if computeNextFlexLine returns true, the flexbox might not have
        // a line because all our children might be out of flow positioned.
        // Instead of just checking if we have a line, make sure the flexbox
        // has at least a line's worth of height to cover this case.
        LayoutUnit minHeight = borderAndPaddingLogicalHeight() + lineHeight(true, isHorizontalWritingMode() ? HorizontalLine : VerticalLine, PositionOfInteriorLineBoxes) + scrollbarLogicalHeight();
        if (size().height() < minHeight)
            setLogicalHeight(minHeight);
    }

    if (!isColumnFlow() && numLines > 1)
        setLogicalHeight(logicalHeight() + computeGap(GapType::BetweenLines) * (numLines - 1));

    updateLogicalHeight();
    repositionLogicalHeightDependentFlexItems(lineStates, gapBetweenLines);
}

LayoutUnit RenderFlexibleBox::autoMarginOffsetInMainAxis(const FlexItems& flexItems, LayoutUnit& availableFreeSpace)
{
    if (availableFreeSpace <= 0_lu)
        return 0_lu;
    
    int numberOfAutoMargins = 0;
    bool isHorizontal = isHorizontalFlow();
    for (size_t i = 0; i < flexItems.size(); ++i) {
        const auto& child = flexItems[i].box.get();
        ASSERT(!child.isOutOfFlowPositioned());
        if (isHorizontal) {
            if (child.style().marginLeft().isAuto())
                ++numberOfAutoMargins;
            if (child.style().marginRight().isAuto())
                ++numberOfAutoMargins;
        } else {
            if (child.style().marginTop().isAuto())
                ++numberOfAutoMargins;
            if (child.style().marginBottom().isAuto())
                ++numberOfAutoMargins;
        }
    }
    if (!numberOfAutoMargins)
        return 0_lu;
    
    LayoutUnit sizeOfAutoMargin = availableFreeSpace / numberOfAutoMargins;
    availableFreeSpace = 0_lu;
    return sizeOfAutoMargin;
}

void RenderFlexibleBox::updateAutoMarginsInMainAxis(RenderBox& child, LayoutUnit autoMarginOffset)
{
    ASSERT(autoMarginOffset >= 0_lu);
    
    if (isHorizontalFlow()) {
        if (child.style().marginLeft().isAuto())
            child.setMarginLeft(autoMarginOffset);
        if (child.style().marginRight().isAuto())
            child.setMarginRight(autoMarginOffset);
    } else {
        if (child.style().marginTop().isAuto())
            child.setMarginTop(autoMarginOffset);
        if (child.style().marginBottom().isAuto())
            child.setMarginBottom(autoMarginOffset);
    }
}

bool RenderFlexibleBox::hasAutoMarginsInCrossAxis(const RenderBox& child) const
{
    if (isHorizontalFlow())
        return child.style().marginTop().isAuto() || child.style().marginBottom().isAuto();
    return child.style().marginLeft().isAuto() || child.style().marginRight().isAuto();
}

LayoutUnit RenderFlexibleBox::availableAlignmentSpaceForChild(LayoutUnit lineCrossAxisExtent, const RenderBox& child)
{
    LayoutUnit childCrossExtent = crossAxisMarginExtentForChild(child) + crossAxisExtentForChild(child);
    return lineCrossAxisExtent - childCrossExtent;
}

bool RenderFlexibleBox::updateAutoMarginsInCrossAxis(RenderBox& child, LayoutUnit availableAlignmentSpace)
{
    ASSERT(!child.isOutOfFlowPositioned());
    ASSERT(availableAlignmentSpace >= 0_lu);
    
    bool isHorizontal = isHorizontalFlow();
    Length topOrLeft = isHorizontal ? child.style().marginTop() : child.style().marginLeft();
    Length bottomOrRight = isHorizontal ? child.style().marginBottom() : child.style().marginRight();
    if (topOrLeft.isAuto() && bottomOrRight.isAuto()) {
        adjustAlignmentForChild(child, availableAlignmentSpace / 2);
        if (isHorizontal) {
            child.setMarginTop(availableAlignmentSpace / 2);
            child.setMarginBottom(availableAlignmentSpace / 2);
        } else {
            child.setMarginLeft(availableAlignmentSpace / 2);
            child.setMarginRight(availableAlignmentSpace / 2);
        }
        return true;
    }
    bool shouldAdjustTopOrLeft = true;
    if (isColumnFlow() && !child.style().isLeftToRightDirection()) {
        // For column flows, only make this adjustment if topOrLeft corresponds to
        // the "before" margin, so that flipForRightToLeftColumn will do the right
        // thing.
        shouldAdjustTopOrLeft = false;
    }
    if (!isColumnFlow() && child.style().isFlippedBlocksWritingMode()) {
        // If we are a flipped writing mode, we need to adjust the opposite side.
        // This is only needed for row flows because this only affects the
        // block-direction axis.
        shouldAdjustTopOrLeft = false;
    }
    
    if (topOrLeft.isAuto()) {
        if (shouldAdjustTopOrLeft)
            adjustAlignmentForChild(child, availableAlignmentSpace);
        
        if (isHorizontal)
            child.setMarginTop(availableAlignmentSpace);
        else
            child.setMarginLeft(availableAlignmentSpace);
        return true;
    }

    if (bottomOrRight.isAuto()) {
        if (!shouldAdjustTopOrLeft)
            adjustAlignmentForChild(child, availableAlignmentSpace);
        
        if (isHorizontal)
            child.setMarginBottom(availableAlignmentSpace);
        else
            child.setMarginRight(availableAlignmentSpace);
        return true;
    }
    return false;
}

LayoutUnit RenderFlexibleBox::marginBoxAscentForChild(const RenderBox& child)
{
    auto isHorizontalFlow = this->isHorizontalFlow();
    auto direction = isHorizontalFlow ? HorizontalLine : VerticalLine;
    if (crossAxisIsPhysicalWidth() == child.isHorizontalWritingMode())
        return synthesizedBaseline(child, style(), direction, BorderBox) + flowAwareMarginBeforeForChild(child);
    auto ascent = alignmentForChild(child) == ItemPosition::LastBaseline ? child.lastLineBaseline() : child.firstLineBaseline();
    if (!ascent)
        return synthesizedBaseline(child, style(), direction, BorderBox) + flowAwareMarginBeforeForChild(child);

    // In either of these cases we require a translation of the ascent because it
    // was computed in a different coordinate space from the flex container's.
    // The first scenario below can occur when the flex container has column flex
    // specified and is in horizontal writing-mode with a vertical-rl flex item *or*
    // when they are both vertical and the child is flipped blocks.
    if ((!style().isFlippedBlocksWritingMode() && child.style().isFlippedBlocksWritingMode()) || (style().isFlippedBlocksWritingMode() && child.style().blockFlowDirection() == BlockFlowDirection::LeftToRight))
        ascent = child.logicalHeight() - ascent.value();

    if (isHorizontalFlow ? child.isScrollContainerY() : child.isScrollContainerX())
        return std::clamp(*ascent, 0_lu, crossAxisExtentForChild(child)) + flowAwareMarginBeforeForChild(child);
    return *ascent + flowAwareMarginBeforeForChild(child);;
}

LayoutUnit RenderFlexibleBox::computeChildMarginValue(Length margin)
{
    // When resolving the margins, we use the content size for resolving percent and calc (for percents in calc expressions) margins.
    // Fortunately, percent margins are always computed with respect to the block's width, even for margin-top and margin-bottom.
    LayoutUnit availableSize = contentLogicalWidth();
    return minimumValueForLength(margin, availableSize);
}

void RenderFlexibleBox::prepareOrderIteratorAndMargins()
{
    OrderIteratorPopulator populator(m_orderIterator);

    for (RenderBox* child = firstChildBox(); child; child = child->nextSiblingBox()) {
        if (!populator.collectChild(*child))
            continue;

        // Before running the flex algorithm, 'auto' has a margin of 0.
        // Also, if we're not auto sizing, we don't do a layout that computes the start/end margins.
        if (isHorizontalFlow()) {
            child->setMarginLeft(computeChildMarginValue(child->style().marginLeft()));
            child->setMarginRight(computeChildMarginValue(child->style().marginRight()));
        } else {
            child->setMarginTop(computeChildMarginValue(child->style().marginTop()));
            child->setMarginBottom(computeChildMarginValue(child->style().marginBottom()));
        }
    }
}

std::pair<LayoutUnit, LayoutUnit> RenderFlexibleBox::computeFlexItemMinMaxSizes(RenderBox& child)
{
    Length max = mainSizeLengthForChild(MaxSize, child);
    std::optional<LayoutUnit> maxExtent = std::nullopt;
    if (max.isSpecifiedOrIntrinsic())
        maxExtent = computeMainAxisExtentForChild(child, MaxSize, max);

    Length min = mainSizeLengthForChild(MinSize, child);
    // Intrinsic sizes in child's block axis are handled by the min-size:auto code path.
    if (min.isSpecified() || (min.isIntrinsic() && mainAxisIsChildInlineAxis(child))) {
        auto minExtent = computeMainAxisExtentForChild(child, MinSize, min).value_or(0_lu);
        // We must never return a min size smaller than the min preferred size for tables.
        if (child.isRenderTable() && mainAxisIsChildInlineAxis(child))
            minExtent = std::max(minExtent, child.minPreferredLogicalWidth());
        return { minExtent, maxExtent.value_or(LayoutUnit::max()) };
    }
    
    if (shouldApplyMinSizeAutoForChild(child)) {
        // FIXME: If the min value is expected to be valid here, we need to come up with a non optional version of computeMainAxisExtentForChild and
        // ensure it's valid through the virtual calls of computeIntrinsicLogicalContentHeightUsing.
        LayoutUnit contentSize;
        Length childCrossSizeLength = crossSizeLengthForChild(MainOrPreferredSize, child);

        bool canComputeSizeThroughAspectRatio = child.isRenderReplaced() && childHasComputableAspectRatio(child) && childCrossSizeIsDefinite(child, childCrossSizeLength);

        if (canComputeSizeThroughAspectRatio)
            contentSize = computeMainSizeFromAspectRatioUsing(child, childCrossSizeLength);
        else
            contentSize = computeMainAxisExtentForChild(child, MinSize, Length(LengthType::MinContent)).value_or(0_lu);

        if (childHasAspectRatio(child) && (!crossSizeLengthForChild(MinSize, child).isAuto() || !crossSizeLengthForChild(MaxSize, child).isAuto()))
            contentSize = adjustChildSizeForAspectRatioCrossAxisMinAndMax(child, contentSize);
        ASSERT(contentSize >= 0);
        contentSize = std::min(contentSize, maxExtent.value_or(contentSize));
        
        Length mainSize = mainSizeLengthForChild(MainOrPreferredSize, child);
        if (childMainSizeIsDefinite(child, mainSize)) {
            LayoutUnit resolvedMainSize = computeMainAxisExtentForChild(child, MainOrPreferredSize, mainSize).value_or(0);
            ASSERT(resolvedMainSize >= 0);
            LayoutUnit specifiedSize = std::min(resolvedMainSize, maxExtent.value_or(resolvedMainSize));
            return { std::min(specifiedSize, contentSize), maxExtent.value_or(LayoutUnit::max()) };
        }

        if (child.isRenderReplaced() && childHasComputableAspectRatioAndCrossSizeIsConsideredDefinite(child)) {
            LayoutUnit transferredSize = computeMainSizeFromAspectRatioUsing(child, childCrossSizeLength);
            transferredSize = adjustChildSizeForAspectRatioCrossAxisMinAndMax(child, transferredSize);
            return { std::min(transferredSize, contentSize), maxExtent.value_or(LayoutUnit::max()) };
        }

        return { contentSize, maxExtent.value_or(LayoutUnit::max()) };
    }

    return { 0_lu, maxExtent.value_or(LayoutUnit::max()) };
}
    
bool RenderFlexibleBox::useChildOverridingCrossSizeForPercentageResolution(const RenderBox& child)
{
    ASSERT(mainAxisIsChildInlineAxis(child));
    if (alignmentForChild(child) != ItemPosition::Stretch)
        return false;

    return child.hasOverridingLogicalHeight();
}

// This method is only called whenever a descendant of a flex item wants to resolve a percentage in its
// block axis (logical height). The key here is that percentages should be generally resolved before the
// flex item is flexed, meaning that they shouldn't be recomputed once the flex item has been flexed. There
// are some exceptions though that are implemented here, like the case of fully inflexible items with
// definite flex-basis, or whenever the flex container has a definite main size. See
// https://drafts.csswg.org/css-flexbox/#definite-sizes for additional details.
bool RenderFlexibleBox::useChildOverridingMainSizeForPercentageResolution(const RenderBox& child)
{
    ASSERT(!mainAxisIsChildInlineAxis(child));

    // The main size of a fully inflexible item with a definite flex basis is, by definition, definite.
    if (child.style().flexGrow() == 0.0 && child.style().flexShrink() == 0.0 && childMainSizeIsDefinite(child, flexBasisForChild(child)))
        return child.hasOverridingLogicalHeight();

    // This function implements section 9.8. Definite and Indefinite Sizes, case 2) of the flexbox spec.
    // If the flex container has a definite main size the flex item post-flexing main size is also treated
    // as definite. We make up a percentage to check whether we have a definite size.
    if (!canComputePercentageFlexBasis(child, Length(0, LengthType::Percent), UpdatePercentageHeightDescendants::Yes))
        return false;

    return child.hasOverridingLogicalHeight();
}

bool RenderFlexibleBox::useChildOverridingLogicalHeightForPercentageResolution(const RenderBox& child)
{
    if (mainAxisIsChildInlineAxis(child))
        return useChildOverridingCrossSizeForPercentageResolution(child);
    return useChildOverridingMainSizeForPercentageResolution(child);
}

LayoutUnit RenderFlexibleBox::adjustChildSizeForAspectRatioCrossAxisMinAndMax(const RenderBox& child, LayoutUnit childSize)
{
    Length crossMin = crossSizeLengthForChild(MinSize, child);
    Length crossMax = crossSizeLengthForChild(MaxSize, child);

    if (childCrossSizeIsDefinite(child, crossMax)) {
        LayoutUnit maxValue = computeMainSizeFromAspectRatioUsing(child, crossMax);
        childSize = std::min(maxValue, childSize);
    }

    if (childCrossSizeIsDefinite(child, crossMin)) {
        LayoutUnit minValue = computeMainSizeFromAspectRatioUsing(child, crossMin);
        childSize = std::max(minValue, childSize);
    }
    
    return childSize;
}

void RenderFlexibleBox::maybeCacheChildMainIntrinsicSize(RenderBox& child, bool relayoutChildren)
{
    if (!childHasIntrinsicMainAxisSize(child))
        return;

    // If this condition is true, then computeMainAxisExtentForChild will call
    // child.intrinsicContentLogicalHeight() and child.scrollbarLogicalHeight(),
    // so if the child has intrinsic min/max/preferred size, run layout on it now to make sure
    // its logical height and scroll bars are up to date.
    updateBlockChildDirtyBitsBeforeLayout(relayoutChildren, child);
    // Don't resolve percentages in children. This is especially important for the min-height calculation,
    // where we want percentages to be treated as auto. For flex-basis itself, this is not a problem because
    // by definition we have an indefinite flex basis here and thus percentages should not resolve.
    if (child.needsLayout() || !m_intrinsicSizeAlongMainAxis.contains(child)) {
        if (isHorizontalWritingMode() == child.isHorizontalWritingMode())
            child.setOverridingContainingBlockContentLogicalHeight(std::nullopt);
        else
            child.setOverridingContainingBlockContentLogicalWidth(std::nullopt);
        child.setChildNeedsLayout(MarkOnlyThis);
        child.layoutIfNeeded();
        cacheChildMainSize(child);
        child.clearOverridingContainingBlockContentSize();
    }
}

FlexItem RenderFlexibleBox::constructFlexItem(RenderBox& child, bool relayoutChildren)
{
    auto childHadLayout = child.everHadLayout();
    child.clearOverridingContentSize();
    if (CheckedPtr flexibleBox = dynamicDowncast<RenderFlexibleBox>(child))
        flexibleBox->resetHasDefiniteHeight();

    if (childHadLayout && child.hasTrimmedMargin(std::optional<MarginTrimType> { }))
        child.clearTrimmedMarginsMarkings();
    
    LayoutUnit borderAndPadding = isHorizontalFlow() ? child.horizontalBorderAndPaddingExtent() : child.verticalBorderAndPaddingExtent();
    LayoutUnit childInnerFlexBaseSize = computeFlexBaseSizeForChild(child, borderAndPadding, relayoutChildren);
    LayoutUnit margin = isHorizontalFlow() ? child.horizontalMarginExtent() : child.verticalMarginExtent();
    return FlexItem(child, childInnerFlexBaseSize, borderAndPadding, margin, computeFlexItemMinMaxSizes(child), childHadLayout);
}
    
void RenderFlexibleBox::freezeViolations(Vector<FlexItem*>& violations, LayoutUnit& availableFreeSpace, double& totalFlexGrow, double& totalFlexShrink, double& totalWeightedFlexShrink)
{
    for (size_t i = 0; i < violations.size(); ++i) {
        ASSERT(!violations[i]->frozen);
        const auto& child = violations[i]->box.get();
        LayoutUnit childSize = violations[i]->flexedContentSize;
        availableFreeSpace -= childSize - violations[i]->flexBaseContentSize;
        totalFlexGrow -= child.style().flexGrow();
        totalFlexShrink -= child.style().flexShrink();
        totalWeightedFlexShrink -= child.style().flexShrink() * violations[i]->flexBaseContentSize;
        // totalWeightedFlexShrink can be negative when we exceed the precision of
        // a double when we initially calcuate totalWeightedFlexShrink. We then
        // subtract each child's weighted flex shrink with full precision, now
        // leading to a negative result. See
        // css3/flexbox/large-flex-shrink-assert.html
        totalWeightedFlexShrink = std::max(totalWeightedFlexShrink, 0.0);
        violations[i]->frozen = true;
    }
}
    
void RenderFlexibleBox::freezeInflexibleItems(FlexSign flexSign, FlexItems& flexItems, LayoutUnit& remainingFreeSpace, double& totalFlexGrow, double& totalFlexShrink, double& totalWeightedFlexShrink)
{
    // Per https://drafts.csswg.org/css-flexbox/#resolve-flexible-lengths step 2,
    // we freeze all items with a flex factor of 0 as well as those with a min/max
    // size violation.
    Vector<FlexItem*> newInflexibleItems;
    for (size_t i = 0; i < flexItems.size(); ++i) {
        FlexItem& flexItem = flexItems[i];
        const auto& child = flexItem.box.get();
        ASSERT(!flexItem.box->isOutOfFlowPositioned());
        ASSERT(!flexItem.frozen);
        float flexFactor = (flexSign == FlexSign::PositiveFlexibility) ? child.style().flexGrow() : child.style().flexShrink();
        if (!flexFactor || (flexSign == FlexSign::PositiveFlexibility && flexItem.flexBaseContentSize > flexItem.hypotheticalMainContentSize) || (flexSign == FlexSign::NegativeFlexibility && flexItem.flexBaseContentSize < flexItem.hypotheticalMainContentSize)) {
            flexItem.flexedContentSize = flexItem.hypotheticalMainContentSize;
            newInflexibleItems.append(&flexItem);
        }
    }
    freezeViolations(newInflexibleItems, remainingFreeSpace, totalFlexGrow, totalFlexShrink, totalWeightedFlexShrink);
}

// Returns true if we successfully ran the algorithm and sized the flex items.
bool RenderFlexibleBox::resolveFlexibleLengths(FlexSign flexSign, FlexItems& flexItems, LayoutUnit initialFreeSpace, LayoutUnit& remainingFreeSpace, double& totalFlexGrow, double& totalFlexShrink, double& totalWeightedFlexShrink)
{
    LayoutUnit totalViolation;
    LayoutUnit usedFreeSpace;
    Vector<FlexItem*> minViolations;
    Vector<FlexItem*> maxViolations;

    double sumFlexFactors = (flexSign == FlexSign::PositiveFlexibility) ? totalFlexGrow : totalFlexShrink;
    if (sumFlexFactors > 0 && sumFlexFactors < 1) {
        LayoutUnit fractional(initialFreeSpace * sumFlexFactors);
        if (fractional.abs() < remainingFreeSpace.abs())
            remainingFreeSpace = fractional;
    }

    for (size_t i = 0; i < flexItems.size(); ++i) {
        FlexItem& flexItem = flexItems[i];
        auto& child = flexItem.box.get();
        
        // This check also covers out-of-flow children.
        if (flexItem.frozen)
            continue;
        
        LayoutUnit childSize = flexItem.flexBaseContentSize;
        double extraSpace = 0;
        if (remainingFreeSpace > 0 && totalFlexGrow > 0 && flexSign == FlexSign::PositiveFlexibility && std::isfinite(totalFlexGrow))
            extraSpace = remainingFreeSpace * child.style().flexGrow() / totalFlexGrow;
        else if (remainingFreeSpace < 0 && totalWeightedFlexShrink > 0 && flexSign == FlexSign::NegativeFlexibility && std::isfinite(totalWeightedFlexShrink) && child.style().flexShrink())
            extraSpace = remainingFreeSpace * child.style().flexShrink() * flexItem.flexBaseContentSize / totalWeightedFlexShrink;
        if (std::isfinite(extraSpace))
            childSize += LayoutUnit::fromFloatRound(extraSpace);

        LayoutUnit adjustedChildSize = flexItem.constrainSizeByMinMax(childSize);
        ASSERT(adjustedChildSize >= 0);
        flexItem.flexedContentSize = adjustedChildSize;
        usedFreeSpace += adjustedChildSize - flexItem.flexBaseContentSize;
        
        LayoutUnit violation = adjustedChildSize - childSize;
        if (violation > 0)
            minViolations.append(&flexItem);
        else if (violation < 0)
            maxViolations.append(&flexItem);
        totalViolation += violation;
    }
    
    if (totalViolation)
        freezeViolations(totalViolation < 0 ? maxViolations : minViolations, remainingFreeSpace, totalFlexGrow, totalFlexShrink, totalWeightedFlexShrink);
    else
        remainingFreeSpace -= usedFreeSpace;
    
    return !totalViolation;
}

static LayoutUnit initialJustifyContentOffset(const RenderStyle& style, LayoutUnit availableFreeSpace, unsigned numberOfChildren, bool isReversed)
{
    ContentPosition justifyContent = style.resolvedJustifyContentPosition(contentAlignmentNormalBehavior());
    ContentDistribution justifyContentDistribution = style.resolvedJustifyContentDistribution(contentAlignmentNormalBehavior());

    if (availableFreeSpace < 0 && style.justifyContent().overflow() == OverflowAlignment::Safe) {
        ASSERT(justifyContent != ContentPosition::Normal);
        justifyContent = ContentPosition::Start;
    }

    // First of all resolve Left and Right so we could convert it to their equivalent properties handled bellow.
    // If the property's axis is not parallel with either left<->right axis, this value behaves as start. Currently,
    // the only case where the property's axis is not parallel with either left<->right axis is in a column flexbox.
    // https: //www.w3.org/TR/css-align-3/#valdef-justify-content-left
    if (justifyContent == ContentPosition::Left || justifyContent == ContentPosition::Right) {
        auto leftRightAxisDirection = RenderFlexibleBox::leftRightAxisDirectionFromStyle(style);
        justifyContent = (style.justifyContent().isEndward(leftRightAxisDirection, isReversed))
            ? ContentPosition::End : ContentPosition::Start;
    }
    ASSERT(justifyContent != ContentPosition::Left);
    ASSERT(justifyContent != ContentPosition::Right);

    if (justifyContent == ContentPosition::FlexEnd
        || (justifyContent == ContentPosition::End && !isReversed)
        || (justifyContent == ContentPosition::Start && isReversed))
        return availableFreeSpace;
    if (justifyContent == ContentPosition::Center)
        return availableFreeSpace / 2;
    if (justifyContentDistribution == ContentDistribution::SpaceAround) {
        if (availableFreeSpace > 0 && numberOfChildren)
            return availableFreeSpace / (2 * numberOfChildren);
        else
            return availableFreeSpace / 2;
    }
    if (justifyContentDistribution == ContentDistribution::SpaceEvenly) {
        if (availableFreeSpace > 0 && numberOfChildren)
            return availableFreeSpace / (numberOfChildren + 1);
        // Fallback to 'center'
        return availableFreeSpace / 2;
    }
    return 0;
}

static LayoutUnit justifyContentSpaceBetweenChildren(LayoutUnit availableFreeSpace, ContentDistribution justifyContentDistribution, unsigned numberOfChildren)
{
    if (availableFreeSpace > 0 && numberOfChildren > 1) {
        if (justifyContentDistribution == ContentDistribution::SpaceBetween)
            return availableFreeSpace / (numberOfChildren - 1);
        if (justifyContentDistribution == ContentDistribution::SpaceAround)
            return availableFreeSpace / numberOfChildren;
        if (justifyContentDistribution == ContentDistribution::SpaceEvenly)
            return availableFreeSpace / (numberOfChildren + 1);
    }
    return 0;
}

static LayoutUnit alignmentOffset(LayoutUnit availableFreeSpace, ItemPosition position, std::optional<LayoutUnit> ascent, std::optional<LayoutUnit> maxAscent, bool isWrapReverse)
{
    switch (position) {
    case ItemPosition::Legacy:
    case ItemPosition::Auto:
    case ItemPosition::Normal:
        ASSERT_NOT_REACHED();
        break;
    case ItemPosition::Start:
    case ItemPosition::End:
    case ItemPosition::SelfStart:
    case ItemPosition::SelfEnd:
    case ItemPosition::Left:
    case ItemPosition::Right:
        ASSERT_NOT_REACHED("%u alignmentForChild should have transformed this position value to something we handle below.", static_cast<uint8_t>(position));
        break;
    case ItemPosition::Stretch:
        // Actual stretching must be handled by the caller. Since wrap-reverse
        // flips cross start and cross end, stretch children should be aligned
        // with the cross end. This matters because applyStretchAlignment
        // doesn't always stretch or stretch fully (explicit cross size given, or
        // stretching constrained by max-height/max-width). For flex-start and
        // flex-end this is handled by alignmentForChild().
        if (isWrapReverse)
            return availableFreeSpace;
        break;
    case ItemPosition::FlexStart:
        break;
    case ItemPosition::FlexEnd:
        return availableFreeSpace;
    case ItemPosition::Center:
        return availableFreeSpace / 2;
    case ItemPosition::Baseline:
    case ItemPosition::LastBaseline: 
        // FIXME: If we get here in columns, we want the use the descent, except
        // we currently can't get the ascent/descent of orthogonal children.
        // https://bugs.webkit.org/show_bug.cgi?id=98076
        return maxAscent.value_or(0_lu) - ascent.value_or(0_lu);
    }
    return 0;
}

void RenderFlexibleBox::setOverridingMainSizeForChild(RenderBox& child, LayoutUnit childPreferredSize)
{
    if (mainAxisIsChildInlineAxis(child))
        child.setOverridingLogicalWidth(childPreferredSize + child.borderAndPaddingLogicalWidth());
    else
        child.setOverridingLogicalHeight(childPreferredSize + child.borderAndPaddingLogicalHeight());
}

LayoutUnit RenderFlexibleBox::staticMainAxisPositionForPositionedChild(const RenderBox& child)
{
    auto childMainExtent = mainAxisMarginExtentForChild(child) + mainAxisExtentForChild(child);
    auto availableSpace = mainAxisContentExtent(contentLogicalHeight()) - childMainExtent;
    auto isReverse = isColumnOrRowReverse();
    LayoutUnit offset = initialJustifyContentOffset(style(), availableSpace, 1, isReverse);
    if (isReverse)
        offset = availableSpace - offset;
    return offset;
}

LayoutUnit RenderFlexibleBox::staticCrossAxisPositionForPositionedChild(const RenderBox& child)
{
    auto availableSpace = availableAlignmentSpaceForChild(crossAxisContentExtent(), child);
    auto safety = overflowAlignmentForChild(child);
    auto align = alignmentForChild(child);
    if (availableSpace < 0 && safety == OverflowAlignment::Safe)
        align = ItemPosition::FlexStart;
    return alignmentOffset(availableSpace, align, std::nullopt, std::nullopt, style().flexWrap() == FlexWrap::Reverse);
}

LayoutUnit RenderFlexibleBox::staticInlinePositionForPositionedChild(const RenderBox& child)
{
    return startOffsetForContent() + (isColumnFlow() ? staticCrossAxisPositionForPositionedChild(child) : staticMainAxisPositionForPositionedChild(child));
}

LayoutUnit RenderFlexibleBox::staticBlockPositionForPositionedChild(const RenderBox& child)
{
    return borderAndPaddingBefore() + (isColumnFlow() ? staticMainAxisPositionForPositionedChild(child) : staticCrossAxisPositionForPositionedChild(child));
}

bool RenderFlexibleBox::setStaticPositionForPositionedLayout(const RenderBox& child)
{
    bool positionChanged = false;
    auto* childLayer = child.layer();
    if (child.style().hasStaticInlinePosition(style().isHorizontalWritingMode())) {
        LayoutUnit inlinePosition = staticInlinePositionForPositionedChild(child);
        if (childLayer->staticInlinePosition() != inlinePosition) {
            childLayer->setStaticInlinePosition(inlinePosition);
            positionChanged = true;
        }
    }
    if (child.style().hasStaticBlockPosition(style().isHorizontalWritingMode())) {
        LayoutUnit blockPosition = staticBlockPositionForPositionedChild(child);
        if (childLayer->staticBlockPosition() != blockPosition) {
            childLayer->setStaticBlockPosition(blockPosition);
            positionChanged = true;
        }
    }
    return positionChanged;
}

// This refers to https://drafts.csswg.org/css-flexbox-1/#definite-sizes, section 1).
LayoutUnit RenderFlexibleBox::computeCrossSizeForChildUsingContainerCrossSize(const RenderBox& child) const
{
    if (crossAxisIsPhysicalWidth())
        return contentWidth();

    // Keep this sync'ed with childCrossSizeShouldUseContainerCrossSize().
    auto definiteSizeValue = [&] {
        // Let's compute the definite size value for the flex item (value that we can resolve without running layout).
        auto isHorizontal = isHorizontalFlow();
        auto size = isHorizontal ? style().height() : style().width();
        ASSERT(size.isFixed() || (size.isPercent() && availableLogicalHeightForPercentageComputation()));
        auto definiteValue = LayoutUnit { size.value() };
        if (size.isPercent())
            definiteValue = availableLogicalHeightForPercentageComputation().value_or(0_lu);

        auto maximumSize = isHorizontal ? style().maxHeight() : style().maxWidth();
        if (maximumSize.isFixed())
            definiteValue = std::min(definiteValue, LayoutUnit { maximumSize.value() });

        auto minimumSize = isHorizontal ? style().minHeight() : style().minWidth();
        if (minimumSize.isFixed())
            definiteValue = std::max(definiteValue, LayoutUnit { minimumSize.value() });

        return definiteValue;
    };
    return std::max(0_lu, definiteSizeValue() - crossAxisMarginExtentForChild(child));
}

void RenderFlexibleBox::prepareChildForPositionedLayout(RenderBox& child)
{
    ASSERT(child.isOutOfFlowPositioned());
    child.containingBlock()->insertPositionedObject(child);
    auto* childLayer = child.layer();
    LayoutUnit staticInlinePosition = flowAwareBorderStart() + flowAwarePaddingStart();
    if (childLayer->staticInlinePosition() != staticInlinePosition) {
        childLayer->setStaticInlinePosition(staticInlinePosition);
        if (child.style().hasStaticInlinePosition(style().isHorizontalWritingMode()))
            child.setChildNeedsLayout(MarkOnlyThis);
    }

    LayoutUnit staticBlockPosition = flowAwareBorderBefore() + flowAwarePaddingBefore();
    if (childLayer->staticBlockPosition() != staticBlockPosition) {
        childLayer->setStaticBlockPosition(staticBlockPosition);
        if (child.style().hasStaticBlockPosition(style().isHorizontalWritingMode()))
            child.setChildNeedsLayout(MarkOnlyThis);
    }
}

inline OverflowAlignment RenderFlexibleBox::overflowAlignmentForChild(const RenderBox& child) const
{
    return child.style().resolvedAlignSelf(&style(), selfAlignmentNormalBehavior()).overflow();
}

ItemPosition RenderFlexibleBox::alignmentForChild(const RenderBox& child) const
{
    ItemPosition align = child.style().resolvedAlignSelf(&style(), selfAlignmentNormalBehavior()).position();
    ASSERT(align != ItemPosition::Auto && align != ItemPosition::Normal);
    // Left and Right are only for justify-*.
    ASSERT(align != ItemPosition::Left && align != ItemPosition::Right);

    if (align == ItemPosition::Baseline && !mainAxisIsChildInlineAxis(child))
        align = ItemPosition::FlexStart;

    // We can safely return here because start/end are not affected by a reversed flex-wrap because the
    // alignment container is the flex line, and in a wrap reversed flex container the start and end within
    // a flex line are still the same. Contrary to this flex-start/flex-end depend on the flex container
    // start/end edges which are flipped in the case of wrap-reverse.
    if (align == ItemPosition::Start)
        return ItemPosition::FlexStart;
    if (align == ItemPosition::End)
        return ItemPosition::FlexEnd;

    if (align == ItemPosition::SelfStart || align == ItemPosition::SelfEnd) {
        // self-start corresponds to flex-start (and self-end to flex-end) in the majority of the cases
        // for orthogonal layouts except when the container is flipped blocks writing mode (vrl/hbt) and
        // the child is ltr or the other way around. For example:
        // 1) htb ltr child inside a vrl container: self-start corresponds to flex-end
        // 2) htb rtl child inside a vlr container: self-end corresponds to flex-start
        bool isOrthogonal = style().isHorizontalWritingMode() != child.style().isHorizontalWritingMode();
        if (isOrthogonal && (style().isFlippedBlocksWritingMode() == child.style().isLeftToRightDirection()))
            return align == ItemPosition::SelfStart ? ItemPosition::FlexEnd : ItemPosition::FlexStart;

        if (!isOrthogonal) {
            if (style().isFlippedLinesWritingMode() != child.style().isFlippedLinesWritingMode())
                return align == ItemPosition::SelfStart ? ItemPosition::FlexEnd : ItemPosition::FlexStart;
            if (style().isLeftToRightDirection() != child.style().isLeftToRightDirection())
                return align == ItemPosition::SelfStart ? ItemPosition::FlexEnd : ItemPosition::FlexStart;
        }

        return align == ItemPosition::SelfStart ? ItemPosition::FlexStart : ItemPosition::FlexEnd;
    }

    if (style().flexWrap() == FlexWrap::Reverse) {
        if (align == ItemPosition::FlexStart)
            align = ItemPosition::FlexEnd;
        else if (align == ItemPosition::FlexEnd)
            align = ItemPosition::FlexStart;
    }

    return align;
}

void RenderFlexibleBox::resetAutoMarginsAndLogicalTopInCrossAxis(RenderBox& child)
{
    if (hasAutoMarginsInCrossAxis(child)) {
        child.updateLogicalHeight();
        if (isHorizontalFlow()) {
            if (child.style().marginTop().isAuto())
                child.setMarginTop(0_lu);
            if (child.style().marginBottom().isAuto())
                child.setMarginBottom(0_lu);
        } else {
            if (child.style().marginLeft().isAuto())
                child.setMarginLeft(0_lu);
            if (child.style().marginRight().isAuto())
                child.setMarginRight(0_lu);
        }
    }
}

bool RenderFlexibleBox::needToStretchChildLogicalHeight(const RenderBox& child) const
{
    // This function is a little bit magical. It relies on the fact that blocks
    // intrinsically "stretch" themselves in their inline axis, i.e. a <div> has
    // an implicit width: 100%. So the child will automatically stretch if our
    // cross axis is the child's inline axis. That's the case if:
    // - We are horizontal and the child is in vertical writing mode
    // - We are vertical and the child is in horizontal writing mode
    // Otherwise, we need to stretch if the cross axis size is auto.
    if (alignmentForChild(child) != ItemPosition::Stretch)
        return false;

    if (isHorizontalFlow() != child.style().isHorizontalWritingMode())
        return false;

    // Aspect ratio is properly handled by RenderReplaced during layout.
    if (child.isRenderReplaced() && childHasAspectRatio(child))
        return false;

    return child.style().logicalHeight().isAuto();
}

bool RenderFlexibleBox::childHasIntrinsicMainAxisSize(const RenderBox& child)
{
    if (mainAxisIsChildInlineAxis(child))
        return false;

    Length childFlexBasis = flexBasisForChild(child);
    Length childMinSize = mainSizeLengthForChild(MinSize, child);
    Length childMaxSize = mainSizeLengthForChild(MaxSize, child);
    // FIXME: we must run childMainSizeIsDefinite() because it might end up calling computePercentageLogicalHeight()
    // which has some side effects like calling addPercentHeightDescendant() for example so it is not possible to skip
    // the call for example by moving it to the end of the conditional expression. This is error-prone and we should
    // refactor computePercentageLogicalHeight() at some point so that it only computes stuff without those side effects.
    if (!childMainSizeIsDefinite(child, childFlexBasis) || childMinSize.isIntrinsic() || childMaxSize.isIntrinsic())
        return true;

    if (shouldApplyMinSizeAutoForChild(child))
        return true;

    return false;
}

Overflow RenderFlexibleBox::mainAxisOverflowForChild(const RenderBox& child) const
{
    if (isHorizontalFlow())
        return child.style().overflowX();
    return child.style().overflowY();
}

Overflow RenderFlexibleBox::crossAxisOverflowForChild(const RenderBox& child) const
{
    if (isHorizontalFlow())
        return child.style().overflowY();
    return child.style().overflowX();
}

bool RenderFlexibleBox::childHasPercentHeightDescendants(const RenderBox& renderer) const
{
    // FIXME: This function can be removed soon after webkit.org/b/204318 is fixed. Evaluate whether the
    // skipContainingBlockForPercentHeightCalculation() check below should be moved to the caller in that case.
    CheckedPtr renderBlock = dynamicDowncast<RenderBlock>(renderer);
    if (!renderBlock)
        return false;

    // FlexibleBoxImpl's like RenderButton might wrap their children in anonymous blocks. Those anonymous blocks are
    // skipped for percentage height calculations in RenderBox::computePercentageLogicalHeight() and thus
    // addPercentHeightDescendant() is never called for them. This means that this method would always wrongly
    // return false for a child of a <button> with a percentage height.
    if (hasPercentHeightDescendants() && skipContainingBlockForPercentHeightCalculation(renderer, isHorizontalWritingMode() != renderer.isHorizontalWritingMode())) {
        for (auto& descendant : *percentHeightDescendants()) {
            if (renderBlock->isContainingBlockAncestorFor(descendant))
                return true;
        }
    }

    if (!renderBlock->hasPercentHeightDescendants())
        return false;

    auto* percentHeightDescendants = renderBlock->percentHeightDescendants();
    if (!percentHeightDescendants)
        return false;

    for (auto& descendant : *percentHeightDescendants) {
        bool hasOutOfFlowAncestor = false;
        for (auto* ancestor = descendant.containingBlock(); ancestor && ancestor != renderBlock.get(); ancestor = ancestor->containingBlock()) {
            if (ancestor->isOutOfFlowPositioned()) {
                hasOutOfFlowAncestor = true;
                break;
            }
        }
        if (!hasOutOfFlowAncestor)
            return true;
    }
    return false;
}

void RenderFlexibleBox::layoutAndPlaceChildren(LayoutUnit& crossAxisOffset, FlexItems& flexItems, LayoutUnit availableFreeSpace, bool relayoutChildren, FlexLineStates& lineStates, LayoutUnit gapBetweenItems)
{
    LayoutUnit autoMarginOffset = autoMarginOffsetInMainAxis(flexItems, availableFreeSpace);
    LayoutUnit mainAxisOffset = flowAwareBorderStart() + flowAwarePaddingStart();
    mainAxisOffset += initialJustifyContentOffset(style(), availableFreeSpace, flexItems.size(), isColumnOrRowReverse());
    if (style().flexDirection() == FlexDirection::RowReverse)
        mainAxisOffset += isHorizontalFlow() ? verticalScrollbarWidth() : horizontalScrollbarHeight();

    LayoutUnit totalMainExtent = mainAxisExtent();
    LayoutUnit maxAscent, maxDescent, lastBaselineMaxAscent; // Used when align-items: baseline.
    std::optional<BaselineAlignmentState> baselineAlignmentState;
    LayoutUnit maxChildCrossAxisExtent;
    ContentDistribution distribution = style().resolvedJustifyContentDistribution(contentAlignmentNormalBehavior());
    bool shouldFlipMainAxis = !isColumnFlow() && !isLeftToRightFlow();
    for (size_t i = 0; i < flexItems.size(); ++i) {
        const auto& flexItem = flexItems[i];
        auto& child = flexItem.box.get();

        ASSERT(!flexItem.box->isOutOfFlowPositioned());

        setOverridingMainSizeForChild(child, flexItem.flexedContentSize);
        // The flexed content size and the override size include the scrollbar
        // width, so we need to compare to the size including the scrollbar.
        // TODO(cbiesinger): Should it include the scrollbar?
        if (flexItem.flexedContentSize != mainAxisContentExtentForChildIncludingScrollbar(child))
            child.setChildNeedsLayout(MarkOnlyThis);
        else {
            // To avoid double applying margin changes in
            // updateAutoMarginsInCrossAxis, we reset the margins here.
            resetAutoMarginsAndLogicalTopInCrossAxis(child);
        }
        // We may have already forced relayout for orthogonal flowing children in
        // computeInnerFlexBaseSizeForChild.
        bool forceChildRelayout = relayoutChildren && !m_relaidOutChildren.contains(child);
        if (!forceChildRelayout && childHasPercentHeightDescendants(child)) {
            // Have to force another relayout even though the child is sized
            // correctly, because its descendants are not sized correctly yet. Our
            // previous layout of the child was done without an override height set.
            // So, redo it here.
            forceChildRelayout = true;
        }
        updateFlexItemDirtyBitsBeforeLayout(forceChildRelayout, child);
        if (!child.needsLayout())
            child.markForPaginationRelayoutIfNeeded();
        if (child.needsLayout())
            m_relaidOutChildren.add(child);
        child.layoutIfNeeded();
        if (!flexItem.everHadLayout && child.checkForRepaintDuringLayout()) {
            child.repaint();
            child.repaintOverhangingFloats(true);
        }

        updateAutoMarginsInMainAxis(child, autoMarginOffset);

        LayoutUnit childCrossAxisMarginBoxExtent;

        auto alignment = alignmentForChild(child);
        if ((alignment == ItemPosition::Baseline || alignment == ItemPosition::LastBaseline) && !hasAutoMarginsInCrossAxis(child)) {
            LayoutUnit ascent = marginBoxAscentForChild(child);
            LayoutUnit descent = (crossAxisMarginExtentForChild(child) + crossAxisExtentForChild(child)) - ascent;
            maxDescent = std::max(maxDescent, descent);

            if (baselineAlignmentState)
                baselineAlignmentState->updateSharedGroup(child, alignment, ascent);
            else
                baselineAlignmentState = { child, alignment, ascent };

            if (alignment == ItemPosition::Baseline) {
                maxAscent =  std::max(maxAscent, ascent);
                childCrossAxisMarginBoxExtent = maxAscent + maxDescent;
            } else {
                lastBaselineMaxAscent = std::max(lastBaselineMaxAscent, ascent);
                childCrossAxisMarginBoxExtent = lastBaselineMaxAscent + maxDescent;
            }

        } else
            childCrossAxisMarginBoxExtent = crossAxisIntrinsicExtentForChild(child) + crossAxisMarginExtentForChild(child);

        if (!isColumnFlow())
            setLogicalHeight(std::max(logicalHeight(), crossAxisOffset + flowAwareBorderAfter() + flowAwarePaddingAfter() + childCrossAxisMarginBoxExtent + crossAxisScrollbarExtent()));
        maxChildCrossAxisExtent = std::max(maxChildCrossAxisExtent, childCrossAxisMarginBoxExtent);

        mainAxisOffset += flowAwareMarginStartForChild(child);

        LayoutUnit childMainExtent = mainAxisExtentForChild(child);
        // In an RTL column situation, this will apply the margin-right/margin-end
        // on the left. This will be fixed later in flipForRightToLeftColumn.
        LayoutPoint childLocation(shouldFlipMainAxis ? totalMainExtent - mainAxisOffset - childMainExtent : mainAxisOffset, crossAxisOffset + flowAwareMarginBeforeForChild(child));
        setFlowAwareLocationForChild(child, childLocation);
        mainAxisOffset += childMainExtent + flowAwareMarginEndForChild(child);

        if (i != flexItems.size() - 1) {
            // The last item does not get extra space added.
            mainAxisOffset += justifyContentSpaceBetweenChildren(availableFreeSpace, distribution, flexItems.size()) + gapBetweenItems;
        }

        // FIXME: Deal with pagination.
    }

    if (isColumnFlow())
        setLogicalHeight(std::max(logicalHeight(), mainAxisOffset + flowAwareBorderEnd() + flowAwarePaddingEnd() + scrollbarLogicalHeight()));

    if (style().flexDirection() == FlexDirection::ColumnReverse) {
        // We have to do an extra pass for column-reverse to reposition the flex
        // items since the start depends on the height of the flexbox, which we
        // only know after we've positioned all the flex items.
        updateLogicalHeight();
        layoutColumnReverse(flexItems, crossAxisOffset, availableFreeSpace, gapBetweenItems);
    }

    auto numberOfItemsOnLine = flexItems.size();
    if (!m_numberOfInFlowChildrenOnFirstLine)
        m_numberOfInFlowChildrenOnFirstLine = numberOfItemsOnLine;
    m_numberOfInFlowChildrenOnLastLine = numberOfItemsOnLine;
    lineStates.append(LineState(crossAxisOffset, maxChildCrossAxisExtent, baselineAlignmentState, WTFMove(flexItems)));
    crossAxisOffset += maxChildCrossAxisExtent;
}

void RenderFlexibleBox::layoutColumnReverse(const FlexItems& flexItems, LayoutUnit crossAxisOffset, LayoutUnit availableFreeSpace, LayoutUnit gapBetweenItems)
{
    // This is similar to the logic in layoutAndPlaceChildren, except we place
    // the children starting from the end of the flexbox. We also don't need to
    // layout anything since we're just moving the children to a new position.
    LayoutUnit mainAxisOffset = logicalHeight() - flowAwareBorderEnd() - flowAwarePaddingEnd();
    mainAxisOffset -= initialJustifyContentOffset(style(), availableFreeSpace, flexItems.size(), isColumnOrRowReverse());
    mainAxisOffset -= isHorizontalFlow() ? verticalScrollbarWidth() : horizontalScrollbarHeight();

    ContentDistribution distribution = style().resolvedJustifyContentDistribution(contentAlignmentNormalBehavior());

    for (size_t i = 0; i < flexItems.size(); ++i) {
        auto& child = flexItems[i].box;
        ASSERT(!child->isOutOfFlowPositioned());
        mainAxisOffset -= mainAxisExtentForChild(child) + flowAwareMarginEndForChild(child);
        setFlowAwareLocationForChild(child, LayoutPoint(mainAxisOffset, crossAxisOffset + flowAwareMarginBeforeForChild(child)));
        mainAxisOffset -= flowAwareMarginStartForChild(child);
        
        if (i != flexItems.size() - 1) {
            // The last item does not get extra space added.
            mainAxisOffset -= justifyContentSpaceBetweenChildren(availableFreeSpace, distribution, flexItems.size()) + gapBetweenItems;
        }
    }
}
    
static LayoutUnit initialAlignContentOffset(LayoutUnit availableFreeSpace, ContentPosition alignContent, ContentDistribution alignContentDistribution, OverflowAlignment safety, unsigned numberOfLines, bool isReversed)
{
    if (availableFreeSpace < 0 && safety == OverflowAlignment::Safe) {
        ASSERT(alignContent != ContentPosition::Normal);
        alignContent = ContentPosition::Start;
    }

    if (alignContent == ContentPosition::FlexEnd
        || (alignContent == ContentPosition::End && !isReversed)
        || (alignContent == ContentPosition::Start && isReversed))
        return availableFreeSpace;
    if (alignContent == ContentPosition::Center)
        return availableFreeSpace / 2;
    if (alignContentDistribution == ContentDistribution::SpaceAround) {
        if (availableFreeSpace > 0 && numberOfLines)
            return availableFreeSpace / (2 * numberOfLines);
        if (availableFreeSpace < 0)
            return availableFreeSpace / 2;
    }
    if (alignContentDistribution == ContentDistribution::SpaceEvenly) {
        if (availableFreeSpace > 0)
            return availableFreeSpace / (numberOfLines + 1);
        // Fallback to 'center'
        return availableFreeSpace / 2;
    }
    return 0_lu;
}

static LayoutUnit alignContentSpaceBetweenChildren(LayoutUnit availableFreeSpace, ContentDistribution alignContentDistribution, unsigned numberOfLines)
{
    if (availableFreeSpace > 0 && numberOfLines > 1) {
        if (alignContentDistribution == ContentDistribution::SpaceBetween)
            return availableFreeSpace / (numberOfLines - 1);
        if (alignContentDistribution == ContentDistribution::SpaceAround || alignContentDistribution == ContentDistribution::Stretch)
            return availableFreeSpace / numberOfLines;
        if (alignContentDistribution == ContentDistribution::SpaceEvenly)
            return availableFreeSpace / (numberOfLines + 1);
    }
    return 0_lu;
}

void RenderFlexibleBox::alignFlexLines(FlexLineStates& lineStates, LayoutUnit gapBetweenLines)
{
    if (lineStates.isEmpty() || !isMultiline())
        return;

    ContentPosition position = style().resolvedAlignContentPosition(contentAlignmentNormalBehavior());
    ContentDistribution distribution = style().resolvedAlignContentDistribution(contentAlignmentNormalBehavior());
    OverflowAlignment safety = style().alignContent().overflow();

    if (position == ContentPosition::FlexStart && !gapBetweenLines && safety != OverflowAlignment::Safe)
        return;

    size_t numLines = lineStates.size();
    LayoutUnit availableCrossAxisSpace = crossAxisContentExtent() - (numLines - 1) * gapBetweenLines;
    for (size_t i = 0; i < numLines; ++i)
        availableCrossAxisSpace -= lineStates[i].crossAxisExtent;

    LayoutUnit lineOffset = initialAlignContentOffset(availableCrossAxisSpace, position, distribution, safety, numLines, style().flexWrap() == FlexWrap::Reverse);
    for (unsigned lineNumber = 0; lineNumber < numLines; ++lineNumber) {
        LineState& lineState = lineStates[lineNumber];
        lineState.crossAxisOffset += lineOffset;
        for (size_t childNumber = 0; childNumber < lineState.flexItems.size(); ++childNumber) {
            FlexItem& flexItem = lineState.flexItems[childNumber];
            adjustAlignmentForChild(flexItem.box, lineOffset);
        }
        
        if (distribution == ContentDistribution::Stretch && availableCrossAxisSpace > 0)
            lineStates[lineNumber].crossAxisExtent += availableCrossAxisSpace / static_cast<unsigned>(numLines);

        lineOffset += alignContentSpaceBetweenChildren(availableCrossAxisSpace, distribution, numLines) + gapBetweenLines;
    }
}
    
void RenderFlexibleBox::adjustAlignmentForChild(RenderBox& child, LayoutUnit delta)
{
    ASSERT(!child.isOutOfFlowPositioned());
    setFlowAwareLocationForChild(child, flowAwareLocationForChild(child) + LayoutSize(0_lu, delta));
}
    
void RenderFlexibleBox::alignChildren(FlexLineStates& lineStates)
{
    for (LineState& lineState : lineStates) {
        LayoutUnit lineCrossAxisExtent = lineState.crossAxisExtent;
        auto baselineAlignmentState = lineState.baselineAlignmentState;

        if (lineState.baselineAlignmentState)
            performBaselineAlignment(lineState);

        for (const auto& flexItem : lineState.flexItems) {
            ASSERT(!flexItem.box->isOutOfFlowPositioned());

            auto safety = overflowAlignmentForChild(flexItem.box);
            auto position = alignmentForChild(flexItem.box);
            if (updateAutoMarginsInCrossAxis(flexItem.box, std::max(0_lu, availableAlignmentSpaceForChild(lineCrossAxisExtent, flexItem.box))) || position == ItemPosition::Baseline || position == ItemPosition::LastBaseline)
                continue;
            
            if (position == ItemPosition::Stretch)
                applyStretchAlignmentToChild(flexItem.box, lineCrossAxisExtent);
            LayoutUnit availableSpace = availableAlignmentSpaceForChild(lineCrossAxisExtent, flexItem.box);
            if (availableSpace < 0 && safety == OverflowAlignment::Safe)
                position = ItemPosition::FlexStart; // See Start == FlexStart assumption in alignmentForChild().
            LayoutUnit offset = alignmentOffset(availableSpace, position, std::nullopt, std::nullopt, style().flexWrap() == FlexWrap::Reverse);
            adjustAlignmentForChild(flexItem.box, offset);
        }
    }
}

void RenderFlexibleBox::performBaselineAlignment(LineState& lineState)
{
    ASSERT(lineState.baselineAlignmentState);

    auto lineCrossAxisExtent = lineState.crossAxisExtent;
    bool containerHasWrapReverse = style().flexWrap() == FlexWrap::Reverse;

    auto flexItemWritingModeForBaselineAlignment = [&](const RenderBox& flexItem) {
        if (mainAxisIsChildInlineAxis(flexItem))
            return flexItem.style().writingMode();

        // css-align-3: 9.1. Determining the Baselines of a Box
        // In general, the writing mode of the box, shape, or other object being aligned is used to determine
        // the line-under and line-over edges for synthesis. However, when that writing mode’s block flow direction
        // is parallel to the axis of the alignment context, an axis-compatible writing mode must be assumed:

        // If the box establishing the alignment context has a block flow direction that is orthogonal to the
        // axis of the alignment context, use its writing mode.
        if (style().isRowFlexDirection())
            return style().writingMode();

        //   Otherwise:
        //
        // If the box’s own writing mode is vertical, assume horizontal-tb.
        // If the box’s own writing mode is horizontal, assume vertical-lr if
        // direction is ltr and vertical-rl if direction is rtl.
        if (!flexItem.isHorizontalWritingMode())
            return WritingMode::HorizontalTb;
        return style().direction() == TextDirection::LTR ? WritingMode::VerticalLr : WritingMode::VerticalRl;
    };

    auto shouldAdjustItemTowardsCrossAxisEnd = [&](const BlockFlowDirection& flexItemBlockFlowDirection, ItemPosition alignment) {
        ASSERT(alignment == ItemPosition::Baseline || alignment == ItemPosition::LastBaseline);

        // The direction in which we are aligning (i.e. direction of the cross axis) must be parallel with the direction of the flex item's used writing mode
        ASSERT_IMPLIES(crossAxisDirection() == RenderFlexibleBox::Direction::TopToBottom || crossAxisDirection() == RenderFlexibleBox::Direction::BottomToTop, flexItemBlockFlowDirection == RenderFlexibleBox::Direction::TopToBottom || flexItemBlockFlowDirection == RenderFlexibleBox::Direction::BottomToTop);
        ASSERT_IMPLIES(crossAxisDirection() == RenderFlexibleBox::Direction::LeftToRight || crossAxisDirection() == RenderFlexibleBox::Direction::RightToLeft, flexItemBlockFlowDirection == RenderFlexibleBox::Direction::LeftToRight || flexItemBlockFlowDirection == RenderFlexibleBox::Direction::RightToLeft);

        // For first baseline aligned items, if its block direction is the opposite of
        // the cross axis direction, then that means its fallback alignment (safe self-start)
        // is in the direction of the end of the cross axis
        //
        // For last baseline aligned items, if its block direction is in the same direction as
        // the cross axis direction, then that means its fallback alignment (safe self-end) is
        // in the direction of the end of the cross axis
        if (alignment == ItemPosition::Baseline)
            return crossAxisDirection() != flexItemBlockFlowDirection;
        return crossAxisDirection() == flexItemBlockFlowDirection;
    };

    for (auto& baselineSharingGroup : lineState.baselineAlignmentState.value().sharedGroups()) {
        LayoutUnit minMarginAfterBaseline = LayoutUnit::max();
        for (auto& flexItem : baselineSharingGroup) {
            auto position = alignmentForChild(flexItem);
            ASSERT(position == ItemPosition::Baseline || position == ItemPosition::LastBaseline);
            auto offset = alignmentOffset(availableAlignmentSpaceForChild(lineCrossAxisExtent, flexItem), position, marginBoxAscentForChild(flexItem), baselineSharingGroup.maxAscent(), containerHasWrapReverse);
            adjustAlignmentForChild(flexItem, offset);

            if (shouldAdjustItemTowardsCrossAxisEnd(writingModeToBlockFlowDirection(flexItemWritingModeForBaselineAlignment(flexItem)), position))
                minMarginAfterBaseline = std::min(minMarginAfterBaseline, availableAlignmentSpaceForChild(lineCrossAxisExtent, flexItem) - offset);
        }
        // css-align-3 9.3 part 3:
        // Position the aligned baseline-sharing group within the alignment container according to its
        // fallback alignment. The fallback alignment of a baseline-sharing group is the fallback alignment
        // of its items as resolved to physical directions.
        if (minMarginAfterBaseline) {
            for (auto& flexItem : baselineSharingGroup) {
                if (shouldAdjustItemTowardsCrossAxisEnd(writingModeToBlockFlowDirection(flexItemWritingModeForBaselineAlignment(flexItem)), alignmentForChild(flexItem)) && !hasAutoMarginsInCrossAxis(flexItem))
                    adjustAlignmentForChild(flexItem, minMarginAfterBaseline);
            }
        }
    }
}

void RenderFlexibleBox::applyStretchAlignmentToChild(RenderBox& child, LayoutUnit lineCrossAxisExtent)
{
    if (mainAxisIsChildInlineAxis(child) && child.style().logicalHeight().isAuto()) {
        LayoutUnit stretchedLogicalHeight = std::max(child.borderAndPaddingLogicalHeight(),
        lineCrossAxisExtent - crossAxisMarginExtentForChild(child));
        ASSERT(!child.needsLayout());
        LayoutUnit desiredLogicalHeight = child.constrainLogicalHeightByMinMax(stretchedLogicalHeight, cachedChildIntrinsicContentLogicalHeight(child));
        
        // FIXME: Can avoid laying out here in some cases. See https://webkit.org/b/87905.
        bool childNeedsRelayout = desiredLogicalHeight != child.logicalHeight();
        if (auto* block = dynamicDowncast<RenderBlock>(child); block && block->hasPercentHeightDescendants() && m_relaidOutChildren.contains(child)) {
            // Have to force another relayout even though the child is sized
            // correctly, because its descendants are not sized correctly yet. Our
            // previous layout of the child was done without an override height set.
            // So, redo it here.
            childNeedsRelayout = true;
        }
        if (childNeedsRelayout || !child.hasOverridingLogicalHeight())
            child.setOverridingLogicalHeight(desiredLogicalHeight);
        if (childNeedsRelayout) {
            SetForScope resetChildLogicalHeight(m_shouldResetChildLogicalHeightBeforeLayout, true);
            // We cache the child's intrinsic content logical height to avoid it being
            // reset to the stretched height.
            // FIXME: This is fragile. RenderBoxes should be smart enough to
            // determine their intrinsic content logical height correctly even when
            // there's an overrideHeight.
            LayoutUnit childIntrinsicContentLogicalHeight = cachedChildIntrinsicContentLogicalHeight(child);
            child.setChildNeedsLayout(MarkOnlyThis);
            
            // Don't use layoutChildIfNeeded to avoid setting cross axis cached size twice.
            child.layoutIfNeeded();

            setCachedChildIntrinsicContentLogicalHeight(child, childIntrinsicContentLogicalHeight);
        }
    } else if (!mainAxisIsChildInlineAxis(child) && child.style().logicalWidth().isAuto()) {
        LayoutUnit childWidth = std::max(0_lu, lineCrossAxisExtent - crossAxisMarginExtentForChild(child));
        childWidth = child.constrainLogicalWidthInFragmentByMinMax(childWidth, crossAxisContentExtent(), *this, nullptr);
        
        if (childWidth != child.logicalWidth()) {
            child.setOverridingLogicalWidth(childWidth);
            child.setChildNeedsLayout(MarkOnlyThis);
            child.layoutIfNeeded();
        }
    }
}

void RenderFlexibleBox::flipForRightToLeftColumn(const FlexLineStates& lineStates)
{
    if (style().isLeftToRightDirection() || !isColumnFlow())
        return;
    
    LayoutUnit crossExtent = crossAxisExtent();
    for (size_t lineNumber = 0; lineNumber < lineStates.size(); ++lineNumber) {
        const LineState& lineState = lineStates[lineNumber];
        for (size_t childNumber = 0; childNumber < lineState.flexItems.size(); ++childNumber) {
            const auto& flexItem = lineState.flexItems[childNumber];
            ASSERT(!flexItem.box->isOutOfFlowPositioned());
            
            LayoutPoint location = flowAwareLocationForChild(flexItem.box);
            // For vertical flows, setFlowAwareLocationForChild will transpose x and
            // y, so using the y axis for a column cross axis extent is correct.
            location.setY(crossExtent - crossAxisExtentForChild(flexItem.box) - location.y());
            if (!isHorizontalWritingMode())
                location.move(LayoutSize(0, -horizontalScrollbarHeight()));
            setFlowAwareLocationForChild(flexItem.box, location);
        }
    }
}

void RenderFlexibleBox::flipForWrapReverse(const FlexLineStates& lineStates, LayoutUnit crossAxisStartEdge)
{
    LayoutUnit contentExtent = crossAxisContentExtent();
    for (size_t lineNumber = 0; lineNumber < lineStates.size(); ++lineNumber) {
        const LineState& lineState = lineStates[lineNumber];
        for (size_t childNumber = 0; childNumber < lineState.flexItems.size(); ++childNumber) {
            const auto& flexItem = lineState.flexItems[childNumber];
            LayoutUnit lineCrossAxisExtent = lineStates[lineNumber].crossAxisExtent;
            LayoutUnit originalOffset = lineStates[lineNumber].crossAxisOffset - crossAxisStartEdge;
            LayoutUnit newOffset = contentExtent - originalOffset - lineCrossAxisExtent;
            adjustAlignmentForChild(flexItem.box, newOffset - originalOffset);
        }
    }
}

std::optional<TextDirection> RenderFlexibleBox::leftRightAxisDirectionFromStyle(const RenderStyle& style)
{
    if (!style.isColumnFlexDirection())
        return style.direction();

    if (!style.isHorizontalWritingMode()) {
        return (style.blockFlowDirection() == BlockFlowDirection::LeftToRight)
            ? TextDirection::LTR : TextDirection::RTL;
    }

    return std::nullopt;
}

LayoutOptionalOutsets RenderFlexibleBox::allowedLayoutOverflow() const
{
    LayoutOptionalOutsets allowance = RenderBox::allowedLayoutOverflow();

    auto leftRightAxisDirection = leftRightAxisDirectionFromStyle(style());

    if (isHorizontalFlow()) {
        if (style().justifyContent().overflow() != OverflowAlignment::Safe) {
            if (style().justifyContent().isCentered()) {
                allowance.setLeft(std::nullopt);
                allowance.setRight(std::nullopt);
            } else if (style().justifyContent().isEndward(leftRightAxisDirection, isColumnOrRowReverse()))
                allowance = allowance.xFlippedCopy();
        }

        if (!isMultiline()) // Ignore align-content for single-line flex.
            return allowance;

        if (style().alignContent().overflow() != OverflowAlignment::Safe) {
            if (style().alignContent().isCentered()) {
                allowance.setTop(std::nullopt);
                allowance.setBottom(std::nullopt);
            } else if (style().alignContent().isEndward(std::nullopt, style().flexWrap() == FlexWrap::Reverse))
                allowance = allowance.yFlippedCopy();
        }
    } else {
        if (style().justifyContent().overflow() != OverflowAlignment::Safe) {

            if (style().justifyContent().isCentered()) {
                allowance.setTop(std::nullopt);
                allowance.setBottom(std::nullopt);
            } else if (style().justifyContent().isEndward(leftRightAxisDirection, isColumnOrRowReverse()))
                allowance = allowance.yFlippedCopy();
        }

        if (!isMultiline())
            return allowance;

        if (style().alignContent().overflow() != OverflowAlignment::Safe) {
            if (style().alignContent().isCentered()) {
                allowance.setLeft(std::nullopt);
                allowance.setRight(std::nullopt);
            } else if (style().alignContent().isEndward(std::nullopt, style().flexWrap() == FlexWrap::Reverse))
                allowance = allowance.xFlippedCopy();
        }
    }

    return allowance;
}

LayoutUnit RenderFlexibleBox::computeGap(RenderFlexibleBox::GapType gapType) const
{
    // row-gap is used for gaps between flex items in column flows or for gaps between lines in row flows.
    bool usesRowGap = (gapType == GapType::BetweenItems) == isColumnFlow();
    auto& gapLength = usesRowGap ? style().rowGap() : style().columnGap();
    if (LIKELY(gapLength.isNormal()))
        return { };

    auto availableSize = usesRowGap ? availableLogicalHeightForPercentageComputation().value_or(0_lu) : contentLogicalWidth();
    return minimumValueForLength(gapLength.length(), availableSize);
}

void RenderFlexibleBox::layoutUsingFlexFormattingContext()
{
    if (!m_modernFlexLayout)
        m_modernFlexLayout = makeUnique<LayoutIntegration::FlexLayout>(*this);

    m_modernFlexLayout->updateFormattingRootGeometryAndInvalidate();

    resetHasDefiniteHeight();
    for (auto& flexItem : childrenOfType<RenderBlock>(*this)) {
        // FIXME: This should be moved over to flex integration and run min/max size computation for flex items over there.
        m_modernFlexLayout->updateFlexItemDimensions(flexItem, flexItem.minPreferredLogicalWidth(), flexItem.maxPreferredLogicalWidth());
    }
    m_modernFlexLayout->layout();
    setLogicalHeight(std::max(logicalHeight(), borderBefore() + paddingBefore() + m_modernFlexLayout->contentLogicalHeight() + borderAfter() + paddingAfter()));
    updateLogicalHeight();
}

}
