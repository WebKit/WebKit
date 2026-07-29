/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include "FontBaseline.h"
#include "HitTestResult.h"
#include "InspectorInstrumentation.h"
#include "LayoutIntegrationFlexLayout.h"
#include "LayoutRepainter.h"
#include "LayoutUnit.h"
#include "LineClampUpdater.h"
#include "RenderBlockFlow.h"
#include "RenderBlockInlines.h"
#include "RenderBoxInlines.h"
#include "RenderChildIterator.h"
#include "RenderElementStyleInlines.h"
#include "RenderLayer.h"
#include "RenderLayoutState.h"
#include "RenderObjectEnums.h"
#include "RenderObjectInlines.h"
#include "RenderReplaced.h"
#include "RenderSVGRoot.h"
#include "StyleComputedStyle+GettersInlines.h"
#include "StyleComputedStyle+InitialInlines.h"
#include "StylePrimitiveNumericTypes+Evaluation.h"
#include "WritingMode.h"
#include <wtf/MathExtras.h>
#include <wtf/SetForScope.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/TypeCasts.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RenderFlexibleBox);

RenderFlexibleBox::RenderFlexibleBox(Type type, Element& element, Style::ComputedStyle&& style)
    : RenderBlock(type, element, WTF::move(style), TypeFlag::IsFlexibleBox)
{
    ASSERT(isRenderFlexibleBox());
    setChildrenInline(false); // All of our children must be block-level.
}

RenderFlexibleBox::RenderFlexibleBox(Type type, Document& document, Style::ComputedStyle&& style)
    : RenderBlock(type, document, WTF::move(style), TypeFlag::IsFlexibleBox)
{
    ASSERT(isRenderFlexibleBox());
    setChildrenInline(false); // All of our children must be block-level.
}

RenderFlexibleBox::~RenderFlexibleBox() = default;

ASCIILiteral RenderFlexibleBox::renderName() const
{
    return "RenderFlexibleBox"_s;
}

void RenderFlexibleBox::flexItemWillBeRemoved(const RenderBox& flexItem)
{
    m_flexLayout.flexItemWillBeRemoved(flexItem);
}

void RenderFlexibleBox::layoutBlock(RelayoutChildren relayoutChildren, LayoutUnit)
{
    ASSERT(needsLayout());

    if (relayoutChildren == RelayoutChildren::No) {
        auto simplifiedLayoutScope = SetForScope(m_inSimplifiedLayout, true);
        if (simplifiedLayout())
            return;
    }

    LayoutRepainter repainter(*this);

    resetLogicalHeightBeforeLayoutIfNeeded();

    clearFlexItemOverridingSizes();

    if (recomputeLogicalWidth())
        relayoutChildren = RelayoutChildren::Yes;

    LayoutUnit previousHeight = logicalHeight();
    setLogicalHeight(borderAndPaddingLogicalHeight() + scrollbarLogicalHeight());
    {
        auto lineClampUpdater = LineClampUpdater { *this };
        LayoutStateMaintainer statePusher(*this, locationOffset(), isTransformed() || hasReflection() || writingMode().isBlockFlipped());

        preparePaginationBeforeBlockLayout(relayoutChildren);

        beginUpdateScrollInfoAfterLayoutTransaction();

        // Fieldsets need to find their legend and position it inside the border of the object.
        // The legend then gets skipped during normal layout. The same is true for ruby text.
        // It doesn't get included in the normal layout process but is instead skipped.
        // This must run before the flex layout below so the legend's isExcludedFromNormalLayout
        // bit is set before it collects the flex items (the excluded legend is not a flex item).
        layoutExcludedChildren(relayoutChildren);

        auto oldFlexItemRects = flexItemBorderBoxRects();

        m_flexLayout.layout(relayoutChildren);

        endAndCommitUpdateScrollInfoAfterLayoutTransaction();

        // After the scrollbar reconciliation above, which may have moved the items again.
        repaintFlexItemsDuringLayoutIfMoved(oldFlexItemRects);
        // FIXME: css3/flexbox/repaint-rtl-column.html seems to repaint more overflow than it needs to.
        updateInFlowDescendantTransformsAfterLayout();
        computeInFlowOverflow(flippedContentBoxRect(),  { ComputeOverflowOptions::MarginsExtendContentAreaX, ComputeOverflowOptions::MarginsExtendContentAreaY });
        // FIXME: Only the items at the edges should contribute to the content area. But this distinction only matters in some weird cases with extreme negative margins.

        if (isDocumentElementRenderer() || logicalHeight() != previousHeight)
            layoutOutOfFlowBoxes(RelayoutChildren::Yes);
        else
            layoutOutOfFlowBoxes(relayoutChildren);
        updateOutOfFlowDescendantTransformsAfterLayout();
        addOverflowFromOutOfFlowBoxes();
    }

    updateLayerTransform();

    repainter.repaintAfterLayout();
}

std::optional<LayoutUnit> RenderFlexibleBox::firstLineBaseline() const
{
    return m_flexLayout.firstLineBaseline();
}

std::optional<LayoutUnit> RenderFlexibleBox::lastLineBaseline() const
{
    return m_flexLayout.lastLineBaseline();
}

bool RenderFlexibleBox::hitTestChildren(const HitTestRequest& request, HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& adjustedLocation, HitTestAction hitTestAction)
{
    return m_flexLayout.hitTest(request, result, locationInContainer, adjustedLocation, hitTestAction);
}

void RenderFlexibleBox::paintChildren(PaintInfo& paintInfo, const LayoutPoint& paintOffset, PaintInfo& paintInfoForFlexItem, bool usePrintRect)
{
    m_flexLayout.paint(paintInfo, paintOffset, paintInfoForFlexItem, usePrintRect);
}

void RenderFlexibleBox::styleDidChange(Style::Difference diff, const Style::ComputedStyle* oldStyle)
{
    RenderBlock::styleDidChange(diff, oldStyle);
    if (!oldStyle || diff != Style::DifferenceResult::Layout)
        return;

    auto oldAlignItems = oldStyle->alignItems().resolve().position();
    auto newAlignItems = style().alignItems().resolve().position();
    auto alignItemsStretchChanged = (oldAlignItems == ItemPosition::Normal || oldAlignItems == ItemPosition::Stretch) != (newAlignItems == ItemPosition::Normal || newAlignItems == ItemPosition::Stretch);
    for (auto& flexItem : childrenOfType<RenderBox>(*this)) {
        // Flex items that were previously stretching need to be relayed out so we
        // can compute new available cross axis space. This is only necessary for
        // stretching since other alignment values don't change the size of the
        // box.
        if (alignItemsStretchChanged && flexItem.style().alignSelf().isAuto())
            flexItem.setChildNeedsLayout(MarkingBehavior::MarkOnlyThis);
    }
}

bool RenderFlexibleBox::willStretchItem(const RenderBox& item, LogicalBoxAxis containingAxis, StretchingMode mode) const
{
    return FlexFormattingUtils::willStretchFlexItem(*this, item, containingAxis, mode);
}

RenderFlexibleBox::FlexItemBorderBoxRects RenderFlexibleBox::flexItemBorderBoxRects() const
{
    FlexItemBorderBoxRects flexItemBorderBoxRects;
    for (auto& flexItem : childrenOfType<RenderBox>(*this)) {
        if (!flexItem.isOutOfFlowPositioned() && !flexItem.isExcludedFromNormalLayout())
            flexItemBorderBoxRects.append(flexItem.borderBoxRectInContainer());
    }
    return flexItemBorderBoxRects;
}

void RenderFlexibleBox::repaintFlexItemsDuringLayoutIfMoved(const FlexItemBorderBoxRects& oldFlexItemRects)
{
    size_t index = 0;
    for (auto& flexItem : childrenOfType<RenderBox>(*this)) {
        if (flexItem.isOutOfFlowPositioned() || flexItem.isExcludedFromNormalLayout())
            continue;

        // If the child moved, we have to repaint it as well as any floating/positioned
        // descendants. An exception is if we need a layout. In this case, we know we're going to
        // repaint ourselves (and the child) anyway.
        if (!selfNeedsLayout() && flexItem.checkForRepaintDuringLayout())
            flexItem.repaintDuringLayoutIfMoved(oldFlexItemRects[index]);
        ++index;
    }
    ASSERT(index == oldFlexItemRects.size());
}

LayoutOptionalOutsets RenderFlexibleBox::allowedLayoutOverflow() const
{
    return m_flexLayout.adjustAllowedLayoutOverflow(RenderBox::allowedLayoutOverflow());
}

std::pair<LayoutUnit, LayoutUnit> RenderFlexibleBox::computeIntrinsicLogicalWidths() const
{
    auto scrollbarWidth = scrollbarLogicalWidth();

    if (shouldApplySizeOrInlineSizeContainment()) {
        if (auto width = explicitIntrinsicInnerLogicalWidth())
            return { width.value() + scrollbarWidth, width.value() + scrollbarWidth };
        return { scrollbarWidth, scrollbarWidth };
    }

    // FIXME: We're ignoring flex-basis here and we shouldn't. We can't start
    // honoring it though until the flex shorthand stops setting it to 0. See
    // https://bugs.webkit.org/show_bug.cgi?id=116117 and
    // https://crbug.com/240765.
    auto [legendMinWidth, legendMaxWidth] = computeIntrinsicLogicalWidthsForFieldsetLegend();

    auto minLogicalWidth = LayoutUnit { };
    auto maxLogicalWidth = LayoutUnit { };

    size_t numItemsWithNormalLayout = 0;
    for (RenderBox* flexItem = firstChildBox(); flexItem; flexItem = flexItem->nextSiblingBox()) {
        if (flexItem->isOutOfFlowPositioned() || flexItem->isExcludedFromNormalLayout())
            continue;
        ++numItemsWithNormalLayout;

        // Pre-layout orthogonal children in order to get a valid value for the preferred width.
        if (writingMode().isOrthogonal(flexItem->writingMode()))
            flexItem->layoutIfNeeded();

        auto [marginStart, marginEnd] = intrinsicLogicalMarginStartAndEnd(*flexItem);
        LayoutUnit margin = marginStart + marginEnd;
        // The min-content width of a wrapping container puts a break between every item below, so each one is alone
        // on its line and margin-trim takes both of its inline margins -- not just those of the two items that
        // happen to start and end the single line the max-content width is measured over.
        LayoutUnit marginForMinContent = margin;
        if (!FlexFormattingUtils::isColumnFlow(*this) && FlexFormattingUtils::isMultiline(*this)) {
            auto marginTrim = style().marginTrim();
            if (marginTrim.contains(Style::MarginTrimSide::InlineStart))
                marginForMinContent -= marginStart;
            if (marginTrim.contains(Style::MarginTrimSide::InlineEnd))
                marginForMinContent -= marginEnd;
        }

        auto [minContentInParentInlineAxis, maxContentInParentInlineAxis] = [&]() -> std::pair<LayoutUnit, LayoutUnit> {
            if (writingMode().isOrthogonal(flexItem->writingMode())) {
                auto intrinsicBlockSize = flexItem->computeIntrinsicLogicalHeight();
                return { intrinsicBlockSize, intrinsicBlockSize };
            }
            return computeChildIntrinsicLogicalWidths(*flexItem);
        }();

        minContentInParentInlineAxis += marginForMinContent;
        maxContentInParentInlineAxis += margin;

        if (!FlexFormattingUtils::isColumnFlow(*this)) {
            maxLogicalWidth += maxContentInParentInlineAxis;
            if (FlexFormattingUtils::isMultiline(*this)) {
                // For multiline, the min preferred width is if you put a break between
                // each item.
                minLogicalWidth = std::max(minLogicalWidth, minContentInParentInlineAxis);
            } else
                minLogicalWidth += minContentInParentInlineAxis;
        } else {
            minLogicalWidth = std::max(minContentInParentInlineAxis, minLogicalWidth);
            maxLogicalWidth = std::max(maxContentInParentInlineAxis, maxLogicalWidth);
        }
    }

    if (!FlexFormattingUtils::isColumnFlow(*this) && numItemsWithNormalLayout > 1) {
        LayoutUnit inlineGapSize = (numItemsWithNormalLayout - 1) * FlexFormattingUtils::computeGap(*this, FlexFormattingUtils::GapType::BetweenItems);
        maxLogicalWidth += inlineGapSize;
        if (!FlexFormattingUtils::isMultiline(*this))
            minLogicalWidth += inlineGapSize;
    }

    maxLogicalWidth = std::max(minLogicalWidth, maxLogicalWidth);

    // Due to negative margins, it is possible that we calculated a negative
    // intrinsic width. Make sure that we never return a negative width.
    minLogicalWidth = std::max(0_lu, minLogicalWidth);
    maxLogicalWidth = std::max(0_lu, maxLogicalWidth);

    minLogicalWidth = std::max(minLogicalWidth, legendMinWidth);
    maxLogicalWidth = std::max(maxLogicalWidth, legendMaxWidth);

    return { minLogicalWidth + scrollbarWidth, maxLogicalWidth + scrollbarWidth };
}

// This method is only called whenever a descendant of a flex item wants to resolve a percentage in its
// block axis (logical height). The key here is that percentages should be generally resolved before the
// flex item is flexed, meaning that they shouldn't be recomputed once the flex item has been flexed. There
// are some exceptions though that are implemented here, like the case of fully inflexible items with
// definite flex-basis, or whenever the flex container has a definite main size. See
// https://drafts.csswg.org/css-flexbox/#definite-sizes for additional details.
std::optional<LayoutUnit> RenderFlexibleBox::usedFlexItemOverridingLogicalHeightForPercentageResolution(const RenderBox& flexItem)
{
    return canUseFlexItemForPercentageResolution(flexItem) ? flexItem.overridingBorderBoxLogicalHeight() : std::nullopt;
}

bool RenderFlexibleBox::canUseFlexItemForPercentageResolution(const RenderBox& flexItem)
{
    ASSERT(flexItem.isFlexItem());

    // Whether the item's height is definite enough to resolve a percentage against. The flex algorithm answers for
    // itself while it runs; outside of it, only this container knows which of its own layout passes is in play.
    auto hasDefiniteHeight = [&]() -> bool {
        if (m_inFlexItemIntrinsicWidthComputation)
            return FlexFormattingUtils::hasDefiniteCrossSizeForFlexItem(flexItem);

        if (auto isDefiniteInFlexLayoutPhase = m_flexLayout.isFlexItemHeightDefiniteInLayoutPhase(flexItem))
            return *isDefiniteInFlexLayoutPhase;

        if (m_inSimplifiedLayout) {
            // While in simplified layout, we should only re-compute overflow and/or re-position out-of-flow boxes, some renderers (e.g. RenderReplaced and subclasses)
            // currently ignore this optimization and run regular layout. The flex items are at their final sizes in both
            // directions here, computed during previous layout(s).
            return true;
        }

        if (&flexItem == view().frameView().layoutContext().subtreeLayoutRoot())
            return !FlexFormattingUtils::mainAxisIsFlexItemInlineAxis(flexItem);

        // Outside of layout (i.e. when using relative percentage positioning), base the decision on style alone.
        return true;
    };
    return hasDefiniteHeight() && m_flexLayout.hasDefiniteSizeForPercentResolution(flexItem);
}

void RenderFlexibleBox::invalidateBlockAxisSizeForFlexItem(const RenderBox& flexItem)
{
    m_flexLayout.invalidateBlockAxisSizeForFlexItem(flexItem);
}

bool RenderFlexibleBox::isComputingFlexBaseSizes() const
{
    return m_flexLayout.layoutPhase() == LayoutPhase::ComputingFlexBaseSizes;
}

bool RenderFlexibleBox::isInCrossAxisStretchLayout() const
{
    return m_flexLayout.layoutPhase() == LayoutPhase::CrossAxisItemSizing;
}

void RenderFlexibleBox::setFlexItemContentLogicalHeightFromLayout(const RenderBox& flexItem, LayoutUnit height)
{
    m_flexLayout.setFlexItemContentLogicalHeightFromLayout(flexItem, height);
}

bool RenderFlexibleBox::setStaticPositionForPositionedLayout(const RenderBox& flexItem)
{
    return m_flexLayout.setStaticPositionForPositionedLayout(flexItem);
}

LayoutUnit RenderFlexibleBox::computeGap(FlexFormattingUtils::GapType gapType) const
{
    return FlexFormattingUtils::computeGap(*this, gapType);
}

// FIXME: consider adding this check to RenderBox::hasIntrinsicAspectRatio(). We could even make it
// virtual returning false by default. RenderReplaced will overwrite it with the current implementation
// plus this extra check. See wkb.ug/231955.
// Explicit instantiations for the SizeTypes FlexFormattingContext resolves through RenderFlexibleBox from a separate translation unit.
bool RenderFlexibleBox::isChildEligibleForMarginTrim(Style::MarginTrimSide marginTrimSide, const RenderBox& flexItem) const
{
    return m_flexLayout.isFlexItemEligibleForMarginTrim(marginTrimSide, flexItem);
}

void RenderFlexibleBox::clearFlexItemOverridingSizes()
{
    for (auto* flexItem = firstChildBox(); flexItem; flexItem = flexItem->nextSiblingBox()) {
        if (!flexItem->isOutOfFlowPositioned())
            flexItem->clearOverridingSize();
    }
}

}
