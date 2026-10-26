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
#include "BlockMarginCollapse.h"

#include "BlockFormattingUtils.h"
#include "BlockLayoutState.h"
#include "Document.h"
#include "LayoutBoxGeometry.h"
#include "RenderBlockFlow.h"
#include "RenderObjectInlines.h"
#include "StyleComputedStyle+GettersInlines.h"

namespace WebCore {
namespace Layout {

bool BlockMarginCollapse::marginsCollapseThrough(const RenderBox& box, const BoxGeometry& geometry, std::optional<LayoutUnit> containingBlockContentBoxBlockSize, std::optional<LayoutUnit> stretchFitBorderBoxBlockSize, WritingMode writingMode)
{
    if (geometry.borderAndPaddingAfter())
        return false;
    CheckedRef style = box.style();
    // Like RenderBlock::isSelfCollapsingBlock, a calc() min-height counts as non-zero.
    auto& minimumBlockSize = style->logicalMinHeight(writingMode);
    if (minimumBlockSize.isKnownPositive() || minimumBlockSize.isCalculated())
        return false;
    // Stretch is the stretch-fit block size, when there is one (css-sizing-4 §6.1): margins collapse through a box whose used block size is zero.
    auto stretchFitSizeIsPositive = stretchFitBorderBoxBlockSize && *stretchFitBorderBoxBlockSize > 0;
    if (minimumBlockSize.isStretch() && stretchFitSizeIsPositive)
        return false;
    if (style->logicalHeight(writingMode).isStretch() && stretchFitBorderBoxBlockSize)
        return !stretchFitSizeIsPositive;
    // Like RenderBlock::isSelfCollapsingBlock, which checks the height the box has.
    if (auto aspectRatioBlockSize = BlockFormattingUtils::aspectRatioBorderBoxBlockSize(box, geometry.borderBoxWidth(), geometry.horizontalBorderAndPadding(), geometry.verticalBorderAndPadding(), containingBlockContentBoxBlockSize, stretchFitBorderBoxBlockSize, writingMode); aspectRatioBlockSize && *aspectRatioBlockSize > 0)
        return false;
    // A zero or auto computed height (CSS 2.2 §8.3.1): not the sizing keywords, whose used size is the automatic one (like RenderBlock::isSelfCollapsingBlock).
    auto& blockSize = style->logicalHeight(writingMode);
    return blockSize.isKnownZero() || (!blockSize.isIntrinsic() && BlockFormattingUtils::blockSizeBehavesAsAuto(blockSize, containingBlockContentBoxBlockSize));
}

void MarginsWithQuirk::add(LayoutUnit margin, std::optional<bool> isQuirky)
{
    margins.add(margin);
    if (isQuirky && margin)
        nonZeroMarginsAreQuirky = nonZeroMarginsAreQuirky.value_or(true) && *isQuirky;
}

void MarginsWithQuirk::add(const MarginsWithQuirk& other)
{
    margins.add(other.margins);
    if (other.nonZeroMarginsAreQuirky)
        nonZeroMarginsAreQuirky = nonZeroMarginsAreQuirky.value_or(true) && *other.nonZeroMarginsAreQuirky;
}

bool BlockMarginCollapse::isQuirkContainer(const RenderBox& box)
{
    return box.document().inQuirksMode() && box.isBody();
}

LayoutUnit BlockMarginCollapse::collapsedLogicalBottomForCollapsedThroughBox() const
{
    auto margins = m_adjoiningMargins;
    if (m_quirkContainerChildMargins && !m_quirkContainerChildMargins->areQuirky())
        margins.add(m_quirkContainerChildMargins->margins);
    return m_logicalTop + margins.collapsedValue();
}

void BlockMarginCollapse::resetAdjoiningMargins(LayoutUnit logicalTop)
{
    m_adjoiningMargins = { };
    m_logicalTop = logicalTop;
    m_endsWithQuirk = false;
}

void BlockMarginCollapse::resetAdjoiningMarginsAfterLineBoxes(LayoutUnit logicalTop)
{
    resetAdjoiningMargins(logicalTop);
    m_endsWithQuirk = std::nullopt;
}

void BlockMarginCollapse::startBlockContainer(ContainingBlockState& blockContainerState, const ContainingBlockState& containingBlockState, LayoutUnit marginBefore)
{
    m_endsWithQuirk = false;
    CheckedRef box = blockContainerState.blockContainer;
    auto isQuirky = box->style().marginBefore().hasQuirk();
    // A child of the quirk container starts the margins that collapse with its block-start edge.
    if (m_quirkContainerChildMargins && isQuirkContainer(containingBlockState.blockContainer)) {
        resolveQuirkContainerChildMargins();
        m_quirkContainerChildMargins = MarginsWithQuirk { };
    }
    if (m_quirkContainerChildMargins)
        m_quirkContainerChildMargins->add(marginBefore, isQuirky);
    else
        m_adjoiningMargins.add(marginBefore);
    auto& margins = blockContainerState.margins;
    margins.m_marginBeforeIsQuirky = isQuirky;
    margins.m_collapsedMarginsBefore.add(marginBefore, isQuirky);
    margins.m_collapsesWithContainingBlockMarginBefore = !containingBlockState.borderBoxLogicalTop;
    if (isQuirkContainer(box)) {
        ASSERT(!m_quirkContainerChildMargins);
        m_quirkContainerChildMargins = MarginsWithQuirk { };
    }
}

void BlockMarginCollapse::endBlockContainer(const ContainingBlockState& blockContainerState, ContainingBlockState& containingBlockState)
{
    if (isQuirkContainer(blockContainerState.blockContainer))
        resolveQuirkContainerChildMargins();
    if (marginsBeforeCollapseWithContainingBlock(blockContainerState, containingBlockState))
        containingBlockState.margins.m_collapsedMarginsBefore.add(blockContainerState.margins.m_collapsedMarginsBefore);
}

void BlockMarginCollapse::startBlockFormattingContextRoot(const ContainingBlockState& rootState)
{
    if (isQuirkContainer(rootState.blockContainer))
        m_quirkContainerChildMargins = MarginsWithQuirk { };
}

void BlockMarginCollapse::endBlockFormattingContextRoot(const ContainingBlockState& rootState)
{
    if (isQuirkContainer(rootState.blockContainer))
        resolveQuirkContainerChildMargins();
}

void BlockMarginCollapse::addCollapsedThroughBlockEndMargin(const ContainingBlockState& blockContainerState, ContainingBlockState& containingBlockState, LayoutUnit marginAfter)
{
    if (m_quirkContainerChildMargins)
        m_quirkContainerChildMargins->add(marginAfter, { });
    else
        m_adjoiningMargins.add(marginAfter);
    if (marginsBeforeCollapseWithContainingBlock(blockContainerState, containingBlockState))
        containingBlockState.margins.m_collapsedMarginsBefore.add(marginAfter, { });
}

bool BlockMarginCollapse::addBlockEndMargin(LayoutUnit margin, bool isQuirky, bool collapsesWithMarginsAfterContent, LayoutUnit borderBoxLogicalBottom)
{
    if (!collapsesWithMarginsAfterContent)
        m_adjoiningMargins = { };
    else if (m_endsWithQuirk)
        isQuirky = *m_endsWithQuirk && (!margin || isQuirky);
    m_adjoiningMargins.add(margin);
    if (m_adjoiningMargins.collapsedValue())
        m_endsWithQuirk = isQuirky;
    else if (!m_endsWithQuirk)
        m_endsWithQuirk = false;
    m_logicalTop = borderBoxLogicalBottom;
    return isQuirky;
}

bool BlockMarginCollapse::marginsBeforeCollapseWithContainingBlock(const ContainingBlockState& blockContainerState, const ContainingBlockState& containingBlockState)
{
    auto& margins = blockContainerState.margins;
    if (!margins.m_collapsesWithContainingBlockMarginBefore)
        return false;
    // A quirk container drops the margins of a child when they are all quirky (RenderBlockFlow::collapseMarginsWithChildInfo).
    return !isQuirkContainer(containingBlockState.blockContainer) || !margins.m_collapsedMarginsBefore.areQuirky();
}

void BlockMarginCollapse::endQuirkContainerBlockStartSide(const ContainingBlockState& blockContainerState)
{
    if (!isQuirkContainer(blockContainerState.blockContainer))
        resolveQuirkContainerChildMargins();
}

void BlockMarginCollapse::resolveQuirkContainerChildMargins()
{
    // The child's margins join the adjoining margins unless they are all quirky. Without a next child to start new ones, the block-start side
    // ends here.
    auto childMargins = std::exchange(m_quirkContainerChildMargins, std::nullopt);
    if (childMargins && !childMargins->areQuirky())
        m_adjoiningMargins.add(childMargins->margins);
}

}
}
