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
#include "StylePreferredSize.h"

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

} // namespace LayoutIntegration
} // namespace WebCore
