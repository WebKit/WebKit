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

#include "RenderBlockFlow.h"
#include "RenderBoxInlines.h"
#include "RenderFlexibleBox.h"
#include "RenderObjectInlines.h"
#include "RenderTable.h"
#include "StylePreferredSize.h"
#include "StylePrimitiveNumericTypes+Evaluation.h"

namespace WebCore {
namespace LayoutIntegration {

FlexIntegrationUtils::FlexIntegrationUtils(RenderFlexibleBox& flexBox)
    : m_flexBox(flexBox)
{
}

FlexLayoutState& FlexIntegrationUtils::flexLayoutState() const
{
    return m_flexBox->flexLayoutState();
}

void FlexIntegrationUtils::applyStretchedLogicalHeightToFlexItem(const FlexLayoutItem& flexLayoutItem, LayoutUnit blockSize)
{
    auto& flexItem = flexLayoutItem.renderer.get();
    // We cache the child's content logical height to avoid it being reset to the stretched height.
    // FIXME: This is fragile. RenderBoxes should be smart enough to determine their content logical height
    // correctly even when there's an overrideHeight.
    auto contentLogicalHeight = flexBox().flexItemContentLogicalHeight(flexItem);
    flexBox().dirtyPercentHeightDescendantsWithinFlexItem(flexItem);
    // Don't use layoutChildIfNeeded to avoid setting cross axis cached size twice.
    layoutFlexItemForStretchedCrossSize(flexLayoutItem, blockSize, LogicalBoxAxis::Block);
    flexBox().cacheFlexItemContentLogicalHeightIfAllowed(flexItem, contentLogicalHeight);
}

void FlexIntegrationUtils::layoutFlexItemForStretchedCrossSize(const FlexLayoutItem& flexLayoutItem, LayoutUnit crossSize, LogicalBoxAxis crossAxis)
{
    auto& flexItem = flexLayoutItem.renderer.get();
    if (crossAxis == LogicalBoxAxis::Block)
        flexItem.setOverridingBorderBoxLogicalHeight(crossSize);
    else
        flexItem.setOverridingBorderBoxLogicalWidth(crossSize);
    flexItem.setChildNeedsLayout(MarkingBehavior::MarkOnlyThis);
    flexItem.layoutIfNeeded();
}

void FlexIntegrationUtils::layoutFlexItemWithMainSize(FlexLayoutItem& flexLayoutItem, LayoutUnit mainSize)
{
    auto& flexItem = flexLayoutItem.renderer.get();

    FlexFormattingUtils::mainAxisIsFlexItemInlineAxis(flexItem) ? flexItem.setOverridingBorderBoxLogicalWidth(mainSize + flexItem.borderAndPaddingLogicalWidth())
        : flexItem.setOverridingBorderBoxLogicalHeight(mainSize + flexItem.borderAndPaddingLogicalHeight());
    auto mainAxisContentExtentIncludingScrollbar = FlexFormattingUtils::isHorizontalFlow(flexBox()) ? flexItem.contentBoxWidth() + flexItem.verticalScrollbarWidth() : flexItem.contentBoxHeight() + flexItem.horizontalScrollbarHeight();
    auto mainSizeIsUnchanged = mainSize == mainAxisContentExtentIncludingScrollbar;

    if (mainSizeIsUnchanged) {
        // To avoid double applying margin changes in updateAutoMarginsInCrossAxis, we reset the margins here.
        flexBox().resetAutoMarginsAndLogicalTopInCrossAxis(flexItem);
    }

    // We may have already forced relayout for orthogonal flowing children in computeInnerFlexBaseSizeForFlexItem.
    auto shouldMarkFlexItemForLayout = [&] {
        // FIXME: Technically percentage height objects only need a relayout if their percentage isn't going to be turned into
        // an auto value. Add a method to determine this, so that we can avoid the relayout.
        if (flexItem.hasRelativeLogicalHeight())
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
        return flexBox().flexItemHasPercentHeightDescendants(flexItem);
    };

    if (shouldMarkFlexItemForLayout())
        flexItem.setChildNeedsLayout(MarkingBehavior::MarkOnlyThis);
    else
        flexItem.markForPaginationRelayoutIfNeeded();

    if (flexItem.needsLayout()) {
        flexItem.layoutIfNeeded();
        flexLayoutState().setFlexItemHasCompletedLayout(flexItem);
    }

    if (!flexLayoutItem.everHadLayout && flexItem.checkForRepaintDuringLayout()) {
        flexItem.repaint();
        flexItem.repaintOverhangingFloats(true);
    }
}

FlexContainerUsedExtents FlexIntegrationUtils::updateFlexContainerLogicalHeight(LayoutUnit flexContentBlockExtent)
{
    return flexBox().updateFlexContainerLogicalHeight(flexContentBlockExtent);
}

void FlexIntegrationUtils::setFlexItemGeometry(const FlexLayoutItem& flexLayoutItem, const LayoutPoint& location, bool isHorizontalFlow)
{
    // For vertical flows the flex algorithm works in flow-relative coordinates, so transpose back to physical here.
    flexLayoutItem.renderer->setLocation(isHorizontalFlow ? location : location.transposedPoint());
}

void FlexIntegrationUtils::setFlexItemOverridingBorderBoxLogicalHeight(const FlexLayoutItem& flexLayoutItem, LayoutUnit blockSize)
{
    flexLayoutItem.renderer->setOverridingBorderBoxLogicalHeight(blockSize);
}

void FlexIntegrationUtils::invalidateFlexItemContentLogicalWidthsIfNeeded(const FlexLayoutItem& flexLayoutItem)
{
    auto& flexItem = flexLayoutItem.renderer.get();
    if (flexItem.shouldInvalidateContentWidths())
        flexItem.invalidateContentLogicalWidths(MarkingBehavior::MarkOnlyThis);
}

void FlexIntegrationUtils::setTrimmedMarginForChild(const FlexLayoutItem& flexLayoutItem, Style::MarginTrimSide side)
{
    flexBox().setTrimmedMarginForChild(flexLayoutItem.renderer.get(), side);
}

LayoutUnit FlexIntegrationUtils::adjustBorderBoxLogicalWidthForBoxSizing(LayoutUnit computedLogicalWidth) const
{
    return flexBox().adjustBorderBoxLogicalWidthForBoxSizing(computedLogicalWidth);
}

void FlexIntegrationUtils::addItemAtFlexLineStart(const FlexLayoutItem& flexLayoutItem)
{
    flexBox().addItemAtFlexLineStart(flexLayoutItem.renderer.get());
}

void FlexIntegrationUtils::addItemAtFlexLineEnd(const FlexLayoutItem& flexLayoutItem)
{
    flexBox().addItemAtFlexLineEnd(flexLayoutItem.renderer.get());
}

void FlexIntegrationUtils::addItemOnFirstFlexLine(const FlexLayoutItem& flexLayoutItem)
{
    flexBox().addItemOnFirstFlexLine(flexLayoutItem.renderer.get());
}

void FlexIntegrationUtils::addItemOnLastFlexLine(const FlexLayoutItem& flexLayoutItem)
{
    flexBox().addItemOnLastFlexLine(flexLayoutItem.renderer.get());
}

bool FlexIntegrationUtils::flexItemHasPercentHeightDescendants(const FlexLayoutItem& flexLayoutItem) const
{
    return flexBox().flexItemHasPercentHeightDescendants(flexLayoutItem.renderer.get());
}

LayoutUnit FlexIntegrationUtils::flexItemContentLogicalHeight(const FlexLayoutItem& flexLayoutItem) const
{
    return flexBox().flexItemContentLogicalHeight(flexLayoutItem.renderer.get());
}

LayoutUnit FlexIntegrationUtils::computeBlockAxisContentSizeForFlexItem(const FlexLayoutItem& flexLayoutItem)
{
    return flexBox().computeBlockAxisContentSizeForFlexItem(flexLayoutItem.renderer.get());
}

template<typename SizeType> bool FlexIntegrationUtils::flexItemMainSizeIsDefinite(const FlexLayoutItem& flexLayoutItem, const SizeType& size)
{
    return flexBox().flexItemMainSizeIsDefinite(flexLayoutItem.renderer.get(), size);
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

// The item's min/max-content main-axis contribution, read with the container's cross size applied as an override so
// aspect-ratio and percentage resolution see the definite cross size. Unlike the paths that go through
// computeMainAxisExtentForFlexItem (which invalidates the item's content widths itself), these raw contribution reads
// rely on the scope to invalidate them (InvalidateContentWidths::Yes).
LayoutUnit FlexIntegrationUtils::maxContentMainAxisContributionForFlexItem(const FlexLayoutItem& flexLayoutItem)
{
    auto definiteCrossSizeScope = FlexItemDefiniteCrossSizeScope { flexLayoutItem.renderer.get(), FlexItemDefiniteCrossSizeScope::InvalidateContentWidths::Yes };
    auto intrinsicWidthComputationScope = FlexItemIntrinsicWidthComputationScope { flexLayoutItem.renderer.get() };
    return flexLayoutItem.renderer->maxContentLogicalWidthContribution();
}

LayoutUnit FlexIntegrationUtils::minContentMainAxisContributionForFlexItem(const FlexLayoutItem& flexLayoutItem)
{
    auto definiteCrossSizeScope = FlexItemDefiniteCrossSizeScope { flexLayoutItem.renderer.get(), FlexItemDefiniteCrossSizeScope::InvalidateContentWidths::Yes };
    auto intrinsicWidthComputationScope = FlexItemIntrinsicWidthComputationScope { flexLayoutItem.renderer.get() };
    return flexLayoutItem.renderer->minContentLogicalWidthContribution();
}

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
