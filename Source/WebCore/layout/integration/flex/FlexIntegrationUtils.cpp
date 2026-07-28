/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "FlexIntegrationUtils.h"

#include "LocalFrameView.h"
#include "RenderBlockFlow.h"
#include "RenderBoxInlines.h"
#include "RenderFlexibleBox.h"
#include "RenderLayoutState.h"
#include "RenderObjectInlines.h"
#include "RenderReplaced.h"
#include "RenderTable.h"
#include "RenderView.h"
#include "StyleMarginTrim.h"
#include "StyleMaximumSize.h"
#include "StyleMinimumSize.h"
#include "StylePreferredSize.h"
#include "StylePrimitiveNumericTypes+Evaluation.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(FlexItemContentCache);

namespace LayoutIntegration {

FlexIntegrationUtils::FlexIntegrationUtils(RenderFlexibleBox& flexBox, FlexLayoutState& flexLayoutState, FlexItemContentCache& flexItemContentCache)
    : m_flexBox(flexBox)
    , m_flexLayoutState(flexLayoutState)
    , m_flexItemContentCache(flexItemContentCache)
{
}

FlexLayoutState& FlexIntegrationUtils::flexLayoutState() const
{
    return m_flexLayoutState;
}

void FlexIntegrationUtils::applyStretchedLogicalHeightToFlexItem(const FlexLayoutItem& flexLayoutItem, LayoutUnit blockSize)
{
    CheckedRef renderer = flexLayoutItem.renderer;
    // We cache the child's content logical height to avoid it being reset to the stretched height.
    // FIXME: This is fragile. RenderBoxes should be smart enough to determine their content logical height
    // correctly even when there's an overrideHeight.
    auto canSetFlexItemContentLogicalHeight = !is<RenderReplaced>(renderer) && !renderer->shouldComputeLogicalHeightFromAspectRatio();
    if (!canSetFlexItemContentLogicalHeight) {
        dirtyPercentHeightDescendantsWithinFlexItem(renderer);
        layoutFlexItemForStretchedCrossSize(flexLayoutItem, blockSize, LogicalBoxAxis::Block);
        return;
    }

    auto contentLogicalHeight = flexItemContentLogicalHeight(flexLayoutItem);
    dirtyPercentHeightDescendantsWithinFlexItem(renderer);
    layoutFlexItemForStretchedCrossSize(flexLayoutItem, blockSize, LogicalBoxAxis::Block);
    m_flexItemContentCache.setContentLogicalHeight(renderer, contentLogicalHeight);
}

void FlexIntegrationUtils::layoutFlexItemForStretchedCrossSize(const FlexLayoutItem& flexLayoutItem, LayoutUnit crossSize, LogicalBoxAxis crossAxis)
{
    CheckedRef renderer = flexLayoutItem.renderer;
    if (crossAxis == LogicalBoxAxis::Block)
        renderer->setOverridingBorderBoxLogicalHeight(crossSize);
    else
        renderer->setOverridingBorderBoxLogicalWidth(crossSize);
    renderer->setChildNeedsLayout(MarkingBehavior::MarkOnlyThis);
    renderer->layoutIfNeeded();
}

void FlexIntegrationUtils::resetAutoMarginsAndLogicalTopInCrossAxis(RenderBox& flexItem)
{
    if (!FlexFormattingUtils::hasAutoMarginsInCrossAxis(flexItem))
        return;

    flexItem.updateLogicalHeight();
    if (FlexFormattingUtils::isHorizontalFlow(flexBox())) {
        if (flexItem.style().marginTop().isAuto())
            flexItem.setMarginTop(0_lu);
        if (flexItem.style().marginBottom().isAuto())
            flexItem.setMarginBottom(0_lu);
        return;
    }

    if (flexItem.style().marginLeft().isAuto())
        flexItem.setMarginLeft(0_lu);
    if (flexItem.style().marginRight().isAuto())
        flexItem.setMarginRight(0_lu);
}

void FlexIntegrationUtils::layoutFlexItemWithMainSize(FlexLayoutItem& flexLayoutItem, LayoutUnit mainSize)
{
    CheckedRef flexItem = flexLayoutItem.renderer;

    FlexFormattingUtils::mainAxisIsFlexItemInlineAxis(flexItem) ? flexItem->setOverridingBorderBoxLogicalWidth(mainSize + flexItem->borderAndPaddingLogicalWidth())
        : flexItem->setOverridingBorderBoxLogicalHeight(mainSize + flexItem->borderAndPaddingLogicalHeight());
    auto mainAxisContentExtentIncludingScrollbar = FlexFormattingUtils::isHorizontalFlow(flexBox()) ? flexItem->contentBoxWidth() + flexItem->verticalScrollbarWidth() : flexItem->contentBoxHeight() + flexItem->horizontalScrollbarHeight();
    auto mainSizeIsUnchanged = mainSize == mainAxisContentExtentIncludingScrollbar;

    if (mainSizeIsUnchanged) {
        // To avoid double applying margin changes in updateAutoMarginsInCrossAxis, we reset the margins here.
        resetAutoMarginsAndLogicalTopInCrossAxis(flexItem);
    }

    // We may have already forced relayout for orthogonal flowing children in computeInnerFlexBaseSizeForFlexItem.
    auto shouldMarkFlexItemForLayout = [&] {
        // FIXME: Technically percentage height objects only need a relayout if their percentage isn't going to be turned into
        // an auto value. Add a method to determine this, so that we can avoid the relayout.
        if (flexItem->hasRelativeLogicalHeight())
            return true;
        if (flexLayoutItem.shouldInvalidateChildContent && !flexLayoutState().hasFlexItemCompletedLayout(flexItem))
            return true;
        // The flexed content size and the override size include the scrollbar
        // width, so we need to compare to the size including the scrollbar.
        // FIXME: Should it include the scrollbar?
        if (!mainSizeIsUnchanged)
            return true;
        // Have to force another relayout even though the child is sized
        // correctly, because its descendants are not sized correctly yet. Our
        // previous layout of the child was done without an override height set.
        // So, redo it here.
        return flexItemHasPercentHeightDescendants(flexItem);
    };

    if (shouldMarkFlexItemForLayout())
        flexItem->setChildNeedsLayout(MarkingBehavior::MarkOnlyThis);
    else
        flexItem->markForPaginationRelayoutIfNeeded();

    if (flexItem->needsLayout()) {
        flexItem->layoutIfNeeded();
        flexLayoutState().setFlexItemHasCompletedLayout(flexItem);
    }

    if (!flexLayoutItem.everHadLayout && flexItem->checkForRepaintDuringLayout()) {
        flexItem->repaint();
        flexItem->repaintOverhangingFloats(true);
    }
}

FlexContainerUsedExtents FlexIntegrationUtils::updateFlexContainerLogicalHeight(LayoutUnit flexContentBlockExtent)
{
    // Resolve the container's logical height to the largest of: what is already set, the block-axis extent FlexFormattingContext
    // built from its line sizes (row flow) or its column lines' main content extent (column flow), and the empty-line
    // minimum for a container that establishes a line with no in-flow items (e.g. all children are out of flow). The
    // empty-line minimum is a block-axis floor, so it is folded into the block-axis max here rather than compared
    // against the physical borderBoxHeight() (which is the inline extent in a vertical writing mode). Then resolve
    // against the container's own specified/min/max height and box-sizing, and return the used cross extents (line
    // positioning / item cross sizing / rtl-column flip) and block extents (column re-resolve / column-reverse
    // placement) so FlexFormattingContext takes them as values rather than reading them back off the container.
    auto& flexBox = this->flexBox();
    auto minimumHeightForEmptyLine = flexBox.hasLineIfEmpty() ? flexBox.borderAndPaddingLogicalHeight() + flexBox.lineHeight() + flexBox.scrollbarLogicalHeight() : 0_lu;
    flexBox.setLogicalHeight(std::max(minimumHeightForEmptyLine, std::max(flexBox.logicalHeight(), flexBox.borderAndPaddingLogicalHeight() + flexContentBlockExtent)));
    flexBox.updateLogicalHeight();
    auto crossAxisExtent = FlexFormattingUtils::isHorizontalFlow(flexBox) ? flexBox.borderBoxSize().height() : flexBox.borderBoxSize().width();
    return { FlexFormattingUtils::crossAxisContentExtent(flexBox), crossAxisExtent, flexBox.contentBoxLogicalHeight(), flexBox.logicalHeight() };
}

void FlexIntegrationUtils::setFlexItemGeometry(const FlexLayoutItem& flexLayoutItem, const LayoutPoint& location, bool isHorizontalFlow)
{
    // For vertical flows the flex algorithm works in flow-relative coordinates, so transpose back to physical here.
    flexLayoutItem.renderer->setLocation(isHorizontalFlow ? location : location.transposedPoint());
}

void FlexIntegrationUtils::updateAutoMarginsInMainAxis(const FlexLayoutItem& flexLayoutItem, LayoutUnit autoMarginOffset)
{
    CheckedRef flexItem = flexLayoutItem.renderer;
    ASSERT(autoMarginOffset >= 0_lu);

    if (FlexFormattingUtils::isHorizontalFlow(flexBox())) {
        if (flexItem->style().marginLeft().isAuto())
            flexItem->setMarginLeft(autoMarginOffset);
        if (flexItem->style().marginRight().isAuto())
            flexItem->setMarginRight(autoMarginOffset);
    } else {
        if (flexItem->style().marginTop().isAuto())
            flexItem->setMarginTop(autoMarginOffset);
        if (flexItem->style().marginBottom().isAuto())
            flexItem->setMarginBottom(autoMarginOffset);
    }
}

bool FlexIntegrationUtils::updateAutoMarginsInCrossAxis(const FlexLayoutItem& flexLayoutItem, LayoutUnit& crossOffset, LayoutUnit availableAlignmentSpace)
{
    // 9.6. (#13) Resolve cross-axis auto margins: if both cross-axis margins are auto, split the free space
    // between them; if only one is auto, give it all the free space so the item's outer cross size fills the line.
    CheckedRef flexItem = flexLayoutItem.renderer;
    ASSERT(!flexItem->isOutOfFlowPositioned());
    ASSERT(availableAlignmentSpace >= 0_lu);

    bool isHorizontal = FlexFormattingUtils::isHorizontalFlow(flexBox());
    CheckedRef style = flexLayoutItem.style();
    auto& topOrLeft = isHorizontal ? style->marginTop() : style->marginLeft();
    auto& bottomOrRight = isHorizontal ? style->marginBottom() : style->marginRight();
    if (topOrLeft.isAuto() && bottomOrRight.isAuto()) {
        crossOffset += availableAlignmentSpace / 2;
        if (isHorizontal) {
            flexItem->setMarginTop(availableAlignmentSpace / 2);
            flexItem->setMarginBottom(availableAlignmentSpace / 2);
        } else {
            flexItem->setMarginLeft(availableAlignmentSpace / 2);
            flexItem->setMarginRight(availableAlignmentSpace / 2);
        }
        return true;
    }
    bool shouldAdjustTopOrLeft = true;
    if (FlexFormattingUtils::isColumnFlow(flexBox()) && flexItem->writingMode().isInlineFlipped()) {
        // For column flows, only make this adjustment if topOrLeft corresponds to
        // the "before" margin, so that the rtl-column flip in computeFlexItemRects
        // will do the right thing.
        shouldAdjustTopOrLeft = false;
    }
    if (!FlexFormattingUtils::isColumnFlow(flexBox()) && flexItem->writingMode().isBlockFlipped()) {
        // If we are a flipped writing mode, we need to adjust the opposite side.
        // This is only needed for row flows because this only affects the
        // block-direction axis.
        shouldAdjustTopOrLeft = false;
    }

    if (topOrLeft.isAuto()) {
        if (shouldAdjustTopOrLeft)
            crossOffset += availableAlignmentSpace;

        if (isHorizontal)
            flexItem->setMarginTop(availableAlignmentSpace);
        else
            flexItem->setMarginLeft(availableAlignmentSpace);
        return true;
    }

    if (bottomOrRight.isAuto()) {
        if (!shouldAdjustTopOrLeft)
            crossOffset += availableAlignmentSpace;

        if (isHorizontal)
            flexItem->setMarginBottom(availableAlignmentSpace);
        else
            flexItem->setMarginRight(availableAlignmentSpace);
        return true;
    }
    return false;
}

void FlexIntegrationUtils::setFlexItemOverridingBorderBoxLogicalHeight(const FlexLayoutItem& flexLayoutItem, LayoutUnit blockSize)
{
    CheckedRef flexItem = flexLayoutItem.renderer;
    flexItem->setOverridingBorderBoxLogicalHeight(blockSize);
}

void FlexIntegrationUtils::invalidateFlexItemContentLogicalWidthsIfNeeded(const FlexLayoutItem& flexLayoutItem)
{
    CheckedRef flexItem = flexLayoutItem.renderer;
    if (flexItem->shouldInvalidateContentWidths())
        flexItem->invalidateContentLogicalWidths(MarkingBehavior::MarkOnlyThis);
}

void FlexIntegrationUtils::setTrimmedMarginForChild(const FlexLayoutItem& flexLayoutItem, Style::MarginTrimSide side)
{
    flexBox().setTrimmedMarginForChild(flexLayoutItem.renderer.get(), side);
}

void FlexIntegrationUtils::trimMainAxisMarginStart(FlexLayoutItem& flexLayoutItem)
{
    CheckedRef renderer = flexLayoutItem.renderer;
    auto horizontalFlow = FlexFormattingUtils::isHorizontalFlow(flexBox());
    auto containerWritingMode = flexBox().style().writingMode();
    flexLayoutItem.mainAxisMargin -= horizontalFlow ? renderer->marginStart(containerWritingMode) : renderer->marginBefore(containerWritingMode);
    if (horizontalFlow)
        setTrimmedMarginForChild(flexLayoutItem, Style::MarginTrimSide::InlineStart);
    else
        setTrimmedMarginForChild(flexLayoutItem, Style::MarginTrimSide::BlockStart);
    flexLayoutState().addItemAtFlexLineStart(flexLayoutItem.renderer.get());
}

void FlexIntegrationUtils::trimMainAxisMarginEnd(FlexLayoutItem& flexLayoutItem)
{
    flexLayoutItem.mainAxisMargin -= FlexFormattingUtils::mainAxisMarginEndForFlexItem(flexBox(), flexLayoutItem);
    if (FlexFormattingUtils::isHorizontalFlow(flexBox()))
        setTrimmedMarginForChild(flexLayoutItem, Style::MarginTrimSide::InlineEnd);
    else
        setTrimmedMarginForChild(flexLayoutItem, Style::MarginTrimSide::BlockEnd);
    flexLayoutState().addItemAtFlexLineEnd(flexLayoutItem.renderer.get());
}

void FlexIntegrationUtils::trimCrossAxisMarginStart(const FlexLayoutItem& flexLayoutItem)
{
    if (FlexFormattingUtils::isHorizontalFlow(flexBox()))
        setTrimmedMarginForChild(flexLayoutItem, Style::MarginTrimSide::BlockStart);
    else
        setTrimmedMarginForChild(flexLayoutItem, Style::MarginTrimSide::InlineStart);
    flexLayoutState().addItemOnFirstFlexLine(flexLayoutItem.renderer.get());
}

void FlexIntegrationUtils::trimCrossAxisMarginEnd(const FlexLayoutItem& flexLayoutItem)
{
    if (FlexFormattingUtils::isHorizontalFlow(flexBox()))
        setTrimmedMarginForChild(flexLayoutItem, Style::MarginTrimSide::BlockEnd);
    else
        setTrimmedMarginForChild(flexLayoutItem, Style::MarginTrimSide::InlineEnd);
    flexLayoutState().addItemOnLastFlexLine(flexLayoutItem.renderer.get());
}

LayoutUnit FlexIntegrationUtils::adjustBorderBoxLogicalWidthForBoxSizing(LayoutUnit computedLogicalWidth) const
{
    return flexBox().adjustBorderBoxLogicalWidthForBoxSizing(computedLogicalWidth);
}

void FlexIntegrationUtils::dirtyPercentHeightDescendantsWithinFlexItem(RenderBox& flexItem)
{
    // In quirks mode, the percentage height walk may register descendants on the
    // flex container instead of the flex item. This method uses
    // dirtyForLayoutFromPercentageHeightDescendant to propagate layout through
    // intermediate auto-height ancestors down to those descendants.
    if (!flexBox().hasPercentHeightDescendants())
        return;
    CheckedPtr flexItemBlockFlow = dynamicDowncast<RenderBlockFlow>(flexItem);
    if (!flexItemBlockFlow)
        return;
    for (CheckedRef descendant : *flexBox().percentHeightDescendants()) {
        if (descendant->parent() == &flexBox())
            continue;
        if (flexItemBlockFlow->isContainingBlockAncestorFor(descendant))
            flexItemBlockFlow->dirtyForLayoutFromPercentageHeightDescendant(descendant);
    }
}

bool FlexIntegrationUtils::flexItemHasPercentHeightDescendants(const RenderBox& renderer) const
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
    if (flexBox().hasPercentHeightDescendants()) {
        for (CheckedRef descendant : *flexBox().percentHeightDescendants()) {
            if (renderBlock->isContainingBlockAncestorFor(descendant))
                return true;
        }
    }

    if (!renderBlock->hasPercentHeightDescendants())
        return false;

    auto* percentHeightDescendants = renderBlock->percentHeightDescendants();
    if (!percentHeightDescendants)
        return false;

    for (CheckedRef descendant : *percentHeightDescendants) {
        bool hasOutOfFlowAncestor = false;
        for (CheckedPtr ancestor = descendant->containingBlock(); ancestor && ancestor != renderBlock.get(); ancestor = ancestor->containingBlock()) {
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

bool FlexIntegrationUtils::flexItemHasPercentHeightDescendants(const FlexLayoutItem& flexLayoutItem) const
{
    return flexItemHasPercentHeightDescendants(flexLayoutItem.renderer.get());
}

LayoutUnit FlexIntegrationUtils::flexItemContentLogicalHeight(const FlexLayoutItem& flexLayoutItem) const
{
    CheckedRef renderer = flexLayoutItem.renderer;
    if (CheckedPtr renderReplaced = dynamicDowncast<RenderReplaced>(renderer))
        return renderReplaced->intrinsicLogicalHeight();

    if (auto logicalHeight = m_flexItemContentCache.contentLogicalHeight(renderer))
        return *logicalHeight;

    return renderer->contentBoxLogicalHeight();
}

LayoutUnit FlexIntegrationUtils::computeBlockAxisContentSizeForFlexItem(const FlexLayoutItem& flexLayoutItem)
{
    CheckedRef renderer = flexLayoutItem.renderer;
    // Reuse the size cached in a previous layout while the item stays clean.
    if (!renderer->needsLayout()) {
        if (auto cachedBlockAxisContentSize = m_flexItemContentCache.blockAxisSize(renderer))
            return *cachedBlockAxisContentSize;
    }

    // Don't resolve percentages in children. This is especially important for the min-height calculation,
    // where we want percentages to be treated as auto. For flex-basis itself, this is not a problem because
    // by definition we have an indefinite flex basis here and thus percentages should not resolve.
    auto percentResolveDisableScope = FlexPercentResolveDisabler { flexBox().view().frameView().layoutContext(), renderer };
    renderer->setChildNeedsLayout(MarkingBehavior::MarkOnlyThis);
    renderer->layoutIfNeeded();
    flexLayoutState().setFlexItemHasCompletedLayout(renderer);

    auto blockAxisContentSize = [&] {
        auto flexBasis = FlexFormattingUtils::flexBasisForFlexItem(renderer);
        if (flexBasis.isPercentOrCalculated() && !flexBox().flexLayout().flexItemMainSizeIsDefinite(renderer, flexBasis))
            return flexItemContentLogicalHeight(flexLayoutItem) + renderer->scrollbarLogicalHeight();
        return renderer->logicalHeight() - renderer->borderAndPaddingLogicalHeight();
    }();

    // Cache it so a later layout can skip re-laying-out this item while it stays clean, and record that we laid it out this iteration.
    m_flexItemContentCache.setBlockAxisSize(renderer, blockAxisContentSize);
    return blockAxisContentSize;
}

template<typename SizeType> bool FlexIntegrationUtils::flexItemMainSizeIsDefinite(const FlexLayoutItem& flexLayoutItem, const SizeType& size)
{
    return flexBox().flexLayout().flexItemMainSizeIsDefinite(flexLayoutItem.renderer.get(), size);
}

// Explicit instantiations for the SizeTypes FlexFormattingContext resolves through the integration from a separate translation unit.
template bool FlexIntegrationUtils::flexItemMainSizeIsDefinite<Style::FlexBasis>(const FlexLayoutItem&, const Style::FlexBasis&);
template bool FlexIntegrationUtils::flexItemMainSizeIsDefinite<Style::MinimumSize>(const FlexLayoutItem&, const Style::MinimumSize&);
template bool FlexIntegrationUtils::flexItemMainSizeIsDefinite<Style::MaximumSize>(const FlexLayoutItem&, const Style::MaximumSize&);
template bool FlexIntegrationUtils::flexItemMainSizeIsDefinite<Style::PreferredSize>(const FlexLayoutItem&, const Style::PreferredSize&);

template<typename SizeType>
std::optional<LayoutUnit> FlexIntegrationUtils::computeMainAxisExtentForFlexItem(const FlexLayoutItem& flexLayoutItem, const SizeType& size, LayoutUnit mainAxisSizeForLengthResolution)
{
    CheckedRef flexItem = flexLayoutItem.renderer;
    // If we have a horizontal flow, that means the main size is the width.
    // That's the logical width for horizontal writing modes, and the logical
    // height in vertical writing modes. For a vertical flow, main size is the
    // height, so it's the inverse. So we need the logical width if we have a
    // horizontal flow and horizontal writing mode, or vertical flow and vertical
    // writing mode. Otherwise we need the logical height.
    auto blockAxisExtent = [&]() -> std::optional<LayoutUnit> {
        // No "auto" check needed: computeContentLogicalHeight returns nullopt for
        // auto and we propagate that below.
        auto height = flexItem->computeContentLogicalHeight(size, flexItemContentLogicalHeight(flexLayoutItem));
        if (!height)
            return height;

        // Tables interpret overriding sizes as the size of captions + rows. However the specified height of a table
        // only includes the size of the rows. That's why we need to add the size of the captions here so that the table
        // layout algorithm behaves appropriately.
        LayoutUnit captionsHeight;
        if (CheckedPtr table = dynamicDowncast<RenderTable>(flexItem); table && flexItemMainSizeIsDefinite(flexLayoutItem, size))
            captionsHeight = table->sumCaptionsLogicalHeight();

        // scrollbarLogicalHeight depends on layout having run. flexBaseSizeForFlexItem
        // calls ensureBlockAxisContentSizeForFlexItemIfNeeded before reaching here,
        // which forces layout when flexBaseSizeNeedsBlockAxisContentSize is true. On
        // the false path (definite flex-basis + non-auto min-size + non-visible/clip
        // overflow) layout has not run and this returns 0; that coincides with the
        // spec, which does not attribute a scrollbar contribution to the flex base
        // size on that path.
        return *height + flexItem->scrollbarLogicalHeight() + captionsHeight;
    };

    auto inlineAxisExtent = [&] {
        // computeLogicalWidthUsing re-computes the item's intrinsic widths for a content size; when the item's logical
        // width is auto we can use its cached contribution instead.
        // (Compare code in RenderBlock::computeIntrinsicLogicalWidthContributions)
        if (flexLayoutItem.style().logicalWidth().isAuto() && !FlexFormattingUtils::flexItemHasAspectRatio(flexItem)) {
            if (size.isMinContent()) {
                invalidateFlexItemContentLogicalWidthsIfNeeded(flexLayoutItem);
                return flexItem->minContentLogicalWidthContribution() - flexItem->borderAndPaddingLogicalWidth();
            }
            if (size.isMaxContent()) {
                invalidateFlexItemContentLogicalWidthsIfNeeded(flexLayoutItem);
                return flexItem->maxContentLogicalWidthContribution() - flexItem->borderAndPaddingLogicalWidth();
            }
        }
        return flexItem->computeLogicalWidthUsing(size, mainAxisSizeForLengthResolution, flexBox()) - flexItem->borderAndPaddingLogicalWidth();
    };

    // A definite main size resolves to a length without measuring the item's content, so it needs no scope.
    if (!size.isIntrinsicOrStretch())
        return flexLayoutItem.mainAxisIsInlineAxis ? inlineAxisExtent() : blockAxisExtent();

    // An intrinsic main size is measured from the item's content, so give the item its definite cross size for the
    // measurement: the inline (width) branch lays the item's content out, and the block (height) branch resolves a
    // height that -- for a replaced item, or any item with a preferred aspect ratio -- is computed from the item's used
    // cross size.
    // computeMainAxisExtentForFlexItem invalidates the item's content widths itself, hence InvalidateContentWidths::No.
    auto definiteCrossSizeScope = FlexItemDefiniteCrossSizeScope { flexItem.get(), FlexItemDefiniteCrossSizeScope::InvalidateContentWidths::No };
    if (!flexLayoutItem.mainAxisIsInlineAxis)
        return blockAxisExtent();

    // The item's width is measured by laying its content out, so mark the container to make that content's percentage
    // heights resolve against the definite cross size set above.
    auto intrinsicWidthComputationScope = FlexItemIntrinsicWidthComputationScope { flexItem.get() };
    return inlineAxisExtent();
}

// Explicit instantiations for the SizeTypes FlexFormattingContext resolves through the integration from a separate translation unit.
template std::optional<LayoutUnit> FlexIntegrationUtils::computeMainAxisExtentForFlexItem<Style::FlexBasis>(const FlexLayoutItem&, const Style::FlexBasis&, LayoutUnit);
template std::optional<LayoutUnit> FlexIntegrationUtils::computeMainAxisExtentForFlexItem<Style::MinimumSize>(const FlexLayoutItem&, const Style::MinimumSize&, LayoutUnit);
template std::optional<LayoutUnit> FlexIntegrationUtils::computeMainAxisExtentForFlexItem<Style::MaximumSize>(const FlexLayoutItem&, const Style::MaximumSize&, LayoutUnit);
template std::optional<LayoutUnit> FlexIntegrationUtils::computeMainAxisExtentForFlexItem<Style::PreferredSize>(const FlexLayoutItem&, const Style::PreferredSize&, LayoutUnit);

// The item's max-content main-axis extent with its own main-axis border/padding removed — the flex base size for the
// case that sizes the item under a content/max-content used flex basis. Measured with the content laid out under the
// cross-size override (the scope invalidates the item's preferred widths so they recompute with it in place).
LayoutUnit FlexIntegrationUtils::maxContentMainAxisExtentForFlexItem(const FlexLayoutItem& flexLayoutItem)
{
    CheckedRef flexItem = flexLayoutItem.renderer;
    auto definiteCrossSizeScope = FlexItemDefiniteCrossSizeScope { flexItem, FlexItemDefiniteCrossSizeScope::InvalidateContentWidths::Yes };
    auto intrinsicWidthComputationScope = FlexItemIntrinsicWidthComputationScope { flexItem };
    auto mainAxisBorderAndPadding = FlexFormattingUtils::isHorizontalFlow(flexBox()) ? flexItem->horizontalBorderAndPaddingExtent() : flexItem->verticalBorderAndPaddingExtent();
    return flexItem->maxContentLogicalWidthContribution() - mainAxisBorderAndPadding;
}

// The item's raw min-content main-axis contribution (border/padding included), measured under the cross-size override
// — used to floor a table flex item's min main size.
LayoutUnit FlexIntegrationUtils::minContentMainAxisContributionForFlexItem(const FlexLayoutItem& flexLayoutItem)
{
    auto definiteCrossSizeScope = FlexItemDefiniteCrossSizeScope { flexLayoutItem.renderer.get(), FlexItemDefiniteCrossSizeScope::InvalidateContentWidths::Yes };
    auto intrinsicWidthComputationScope = FlexItemIntrinsicWidthComputationScope { flexLayoutItem.renderer.get() };
    CheckedRef flexItem = flexLayoutItem.renderer;
    return flexItem->minContentLogicalWidthContribution();
}

LayoutUnit FlexIntegrationUtils::flexItemIntrinsicLogicalHeight(const FlexLayoutItem& flexLayoutItem, bool needToStretchLogicalHeight) const
{
    CheckedRef flexItem = flexLayoutItem.renderer;
    // This should only be called if the logical height is the cross size.
    ASSERT(flexLayoutItem.mainAxisIsInlineAxis);
    if (needToStretchLogicalHeight) {
        auto flexItemContentHeight = flexItemContentLogicalHeight(flexLayoutItem);
        auto flexItemLogicalHeight = flexItemContentHeight + flexItem->scrollbarLogicalHeight() + flexItem->borderAndPaddingLogicalHeight();
        return flexItem->constrainLogicalHeightByMinMax(flexItemLogicalHeight, flexItemContentHeight);
    }
    return flexItem->logicalHeight();
}

LayoutUnit FlexIntegrationUtils::flexItemIntrinsicLogicalWidth(const FlexLayoutItem& flexLayoutItem, bool crossSizeIsDefinite)
{
    CheckedRef flexItem = flexLayoutItem.renderer;
    // This should only be called if the logical width is the cross size.
    ASSERT(!flexLayoutItem.mainAxisIsInlineAxis);
    if (crossSizeIsDefinite)
        return flexItem->logicalWidth();

    // computeLogicalWidth returns the overriding width as-is for a flex item, so clear it to get the width the
    // item computes from its own style.
    // FIXME: Check whether an overriding inline size can actually be set on an orthogonal flex item at this point
    // (nothing in this layout pass appears to set one) and remove this if it cannot.
    auto previousOverridingBorderBoxLogicalWidth = flexItem->overridingBorderBoxLogicalWidth();
    flexItem->clearOverridingBorderBoxLogicalWidth();

    RenderBox::LogicalExtentComputedValues values;
    {
        // This is an intrinsic width measurement like the ones in maxContentMainAxisExtentForFlexItem and
        // minContentMainAxisContributionForFlexItem: a percentage resolved against this item while measuring it
        // answers from the item's cross-size definiteness, not from how far the flex algorithm has got.
        auto intrinsicWidthComputationScope = FlexItemIntrinsicWidthComputationScope { flexItem };
        flexItem->computeLogicalWidth(values);
    }

    if (previousOverridingBorderBoxLogicalWidth)
        flexItem->setOverridingBorderBoxLogicalWidth(*previousOverridingBorderBoxLogicalWidth);
    return values.extent;
}

LayoutUnit FlexIntegrationUtils::constrainFlexItemLogicalHeightByMinMax(const FlexLayoutItem& flexLayoutItem, LayoutUnit logicalHeight, std::optional<LayoutUnit> intrinsicContentHeight) const
{
    CheckedRef flexItem = flexLayoutItem.renderer;
    return flexItem->constrainLogicalHeightByMinMax(logicalHeight, intrinsicContentHeight);
}

LayoutUnit FlexIntegrationUtils::constrainFlexItemLogicalWidthByMinMax(const FlexLayoutItem& flexLayoutItem, LayoutUnit logicalWidth, LayoutUnit availableWidth) const
{
    CheckedRef flexItem = flexLayoutItem.renderer;
    return flexItem->constrainLogicalWidthByMinMax(logicalWidth, availableWidth, flexBox());
}

template<typename SizeType> std::optional<LayoutUnit> FlexIntegrationUtils::computePercentageLogicalHeightForFlexItem(const FlexLayoutItem& flexLayoutItem, const SizeType& size) const
{
    CheckedRef flexItem = flexLayoutItem.renderer;
    return flexItem->computePercentageLogicalHeight(size);
}

// Explicit instantiations for the SizeTypes FlexFormattingContext resolves through the integration from a separate translation unit.
template std::optional<LayoutUnit> FlexIntegrationUtils::computePercentageLogicalHeightForFlexItem<Style::PreferredSize>(const FlexLayoutItem&, const Style::PreferredSize&) const;
template std::optional<LayoutUnit> FlexIntegrationUtils::computePercentageLogicalHeightForFlexItem<Style::MinimumSize>(const FlexLayoutItem&, const Style::MinimumSize&) const;
template std::optional<LayoutUnit> FlexIntegrationUtils::computePercentageLogicalHeightForFlexItem<Style::MaximumSize>(const FlexLayoutItem&, const Style::MaximumSize&) const;
template std::optional<LayoutUnit> FlexIntegrationUtils::computePercentageLogicalHeightForFlexItem<Style::PreferredSize::Calc>(const FlexLayoutItem&, const Style::PreferredSize::Calc&) const;

template<typename SizeType> std::optional<LayoutUnit> FlexIntegrationUtils::computeLogicalHeightUsingForFlexItem(const FlexLayoutItem& flexLayoutItem, const SizeType& size) const
{
    CheckedRef flexItem = flexLayoutItem.renderer;
    return flexItem->computeLogicalHeightUsing(size, std::nullopt);
}

template std::optional<LayoutUnit> FlexIntegrationUtils::computeLogicalHeightUsingForFlexItem<Style::PreferredSize>(const FlexLayoutItem&, const Style::PreferredSize&) const;
template std::optional<LayoutUnit> FlexIntegrationUtils::computeLogicalHeightUsingForFlexItem<Style::MinimumSize>(const FlexLayoutItem&, const Style::MinimumSize&) const;
template std::optional<LayoutUnit> FlexIntegrationUtils::computeLogicalHeightUsingForFlexItem<Style::MaximumSize>(const FlexLayoutItem&, const Style::MaximumSize&) const;

template<typename SizeType> LayoutUnit FlexIntegrationUtils::computeLogicalWidthUsingForFlexItem(const FlexLayoutItem& flexLayoutItem, const SizeType& size, LayoutUnit availableWidth) const
{
    CheckedRef flexItem = flexLayoutItem.renderer;
    return flexItem->computeLogicalWidthUsing(size, availableWidth, flexBox());
}

template LayoutUnit FlexIntegrationUtils::computeLogicalWidthUsingForFlexItem<Style::PreferredSize>(const FlexLayoutItem&, const Style::PreferredSize&, LayoutUnit) const;
template LayoutUnit FlexIntegrationUtils::computeLogicalWidthUsingForFlexItem<Style::MinimumSize>(const FlexLayoutItem&, const Style::MinimumSize&, LayoutUnit) const;
template LayoutUnit FlexIntegrationUtils::computeLogicalWidthUsingForFlexItem<Style::MaximumSize>(const FlexLayoutItem&, const Style::MaximumSize&, LayoutUnit) const;

ScopedFlexBasisAsFlexItemMainSize::ScopedFlexBasisAsFlexItemMainSize(const FlexLayoutItem& flexLayoutItem, Style::PreferredSize&& flexBasis)
    : m_flexItem(flexLayoutItem.renderer)
    , m_mainAxisIsInlineAxis(flexLayoutItem.mainAxisIsInlineAxis)
{
    if (flexBasis.isAuto())
        return;

    if (m_mainAxisIsInlineAxis)
        m_flexItem->setOverridingBorderBoxLogicalWidthForFlexBasisComputation(WTF::move(flexBasis));
    else
        m_flexItem->setOverridingBorderBoxLogicalHeightForFlexBasisComputation(WTF::move(flexBasis));
    m_didOverride = true;
}

ScopedFlexBasisAsFlexItemMainSize::~ScopedFlexBasisAsFlexItemMainSize()
{
    if (!m_didOverride)
        return;

    if (m_mainAxisIsInlineAxis)
        m_flexItem->clearOverridingLogicalWidthForFlexBasisComputation();
    else
        m_flexItem->clearOverridingLogicalHeightForFlexBasisComputation();
}

static void setOrClearOverridingBorderBoxLogicalWidth(RenderBox& box, std::optional<LayoutUnit> size)
{
    if (size)
        box.setOverridingBorderBoxLogicalWidth(*size);
    else
        box.clearOverridingBorderBoxLogicalWidth();
}

static void setOrClearOverridingBorderBoxLogicalHeight(RenderBox& box, std::optional<LayoutUnit> size)
{
    if (size)
        box.setOverridingBorderBoxLogicalHeight(*size);
    else
        box.clearOverridingBorderBoxLogicalHeight();
}

FlexItemDefiniteCrossSizeScope::FlexItemDefiniteCrossSizeScope(RenderBox& flexItem, InvalidateContentWidths invalidateContentWidths)
    : m_flexItem(flexItem)
{
    auto saveAndSetInlineSize = [&](std::optional<LayoutUnit> size) {
        m_previousOverridingBorderBoxLogicalWidth = flexItem.overridingBorderBoxLogicalWidth();
        m_shouldRestoreInlineSize = true;
        setOrClearOverridingBorderBoxLogicalWidth(flexItem, size);
    };
    auto saveAndSetBlockSize = [&](std::optional<LayoutUnit> size) {
        m_previousOverridingBorderBoxLogicalHeight = flexItem.overridingBorderBoxLogicalHeight();
        m_shouldRestoreBlockSize = true;
        setOrClearOverridingBorderBoxLogicalHeight(flexItem, size);
    };

    if (!FlexFormattingUtils::hasDefiniteCrossSizeForFlexItem(flexItem)) {
        // No definite cross size to measure against, so make sure no stale overriding size is left in either axis.
        saveAndSetInlineSize({ });
        saveAndSetBlockSize({ });
        return;
    }

    auto crossSize = FlexFormattingUtils::innerCrossSizeForFlexItem(flexItem);
    FlexFormattingUtils::mainAxisIsFlexItemInlineAxis(flexItem) ? saveAndSetBlockSize(crossSize) : saveAndSetInlineSize(crossSize);

    if (invalidateContentWidths == InvalidateContentWidths::Yes) {
        flexItem.invalidateContentLogicalWidths(MarkingBehavior::MarkOnlyThis);
#if ASSERT_ENABLED
        m_didInvalidateContentLogicalWidths = true;
#endif
    }
}

FlexItemDefiniteCrossSizeScope::~FlexItemDefiniteCrossSizeScope()
{
#if ASSERT_ENABLED
    if (m_didInvalidateContentLogicalWidths)
        ASSERT(!m_flexItem->hasInvalidContentLogicalWidths());
#endif
    if (m_shouldRestoreInlineSize)
        setOrClearOverridingBorderBoxLogicalWidth(m_flexItem.get(), m_previousOverridingBorderBoxLogicalWidth);
    if (m_shouldRestoreBlockSize)
        setOrClearOverridingBorderBoxLogicalHeight(m_flexItem.get(), m_previousOverridingBorderBoxLogicalHeight);
}

FlexItemIntrinsicWidthComputationScope::FlexItemIntrinsicWidthComputationScope(RenderBox& flexItem)
    : m_intrinsicWidthComputation(downcast<RenderFlexibleBox>(*flexItem.parent()).m_inFlexItemIntrinsicWidthComputation, true)
{
}

} // namespace LayoutIntegration
} // namespace WebCore
