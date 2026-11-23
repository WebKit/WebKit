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
#include "RenderBoxInlines.h"
#include "RenderElementStyleInlines.h"
#include "RenderObjectInlines.h"
#include "StyleComputedStyle+GettersInlines.h"
#include <ranges>

namespace WebCore {
namespace Layout {

bool BlockMarginCollapse::marginsCollapseThrough(const RenderBox& box, const BoxGeometry& geometry, std::optional<LayoutUnit> containingBlockContentBoxBlockSize, const StretchFitBorderBoxBlockSizes& stretchFitBorderBoxBlockSizes, WritingMode writingMode)
{
    if (geometry.borderAndPaddingAfter())
        return false;
    CheckedRef style = box.style();
    // Like RenderBlock::isSelfCollapsingBlock, a calc() min-height counts as non-zero.
    auto& minimumBlockSize = style->logicalMinHeight(writingMode);
    if (minimumBlockSize.isKnownPositive() || minimumBlockSize.isCalculated())
        return false;
    // A calc-size() has the size it resolves to without content, which the box has when nothing separates its margins (like
    // RenderBlock::isSelfCollapsingBlock, which checks the height the box has). Otherwise it acts like its basis.
    auto& blockSize = style->logicalHeight(writingMode);
    if ((blockSize.isCalcSize() || minimumBlockSize.isCalcSize()) && BlockFormattingUtils::borderBoxBlockSize(box, geometry.verticalBorderAndPadding(), 0_lu, containingBlockContentBoxBlockSize, stretchFitBorderBoxBlockSizes, writingMode) > 0)
        return false;
    // Like RenderBlock::isSelfCollapsingBlock, stretch is neither a zero or auto height nor a zero min-height, also where it resolves to zero
    // or behaves as auto.
    if ((minimumBlockSize.isStretch() && !minimumBlockSize.isCalcSize()) || (blockSize.isStretch() && !blockSize.isCalcSize()))
        return false;
    // Like RenderBlock::isSelfCollapsingBlock, which checks the height the box has: the one a preferred aspect ratio or size containment gives it.
    if (box.shouldApplySizeContainment() && box.explicitIntrinsicInnerLogicalHeight().value_or(0_lu) > 0)
        return false;
    if (auto aspectRatioBlockSize = BlockFormattingUtils::aspectRatioBorderBoxBlockSize(box, geometry.borderBoxWidth(), geometry.horizontalBorderAndPadding(), geometry.verticalBorderAndPadding(), containingBlockContentBoxBlockSize, stretchFitBorderBoxBlockSizes, writingMode); aspectRatioBlockSize && *aspectRatioBlockSize > 0)
        return false;
    // A zero or auto computed height (CSS 2.2 §8.3.1): not the sizing keywords, whose used size is the automatic one (like RenderBlock::isSelfCollapsingBlock).
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
    return box.document().inQuirksMode() && (box.isBody() || box.isRenderTableCell());
}

// The margins a quirk container keeps at its block-start side collapse with the block-start margin of the child of the quirk container around it
// they are in, and are not quirky (RenderBlockFlow::collapseMarginsWithChildInfo).
static void addInnerQuirkContainerMargins(MarginsWithQuirk& childMargins, const AdjoiningMargins& margins)
{
    childMargins.margins.add(margins);
    if (margins.collapsedValue())
        childMargins.nonZeroMarginsAreQuirky = false;
}

LayoutUnit BlockMarginCollapse::collapsedLogicalBottomForCollapsedThroughBox() const
{
    auto blockStartSidesMargins = AdjoiningMargins { };
    for (auto& blockStartSide : m_quirkContainerBlockStartSides | std::views::reverse) {
        auto childMargins = blockStartSide.childMargins;
        addInnerQuirkContainerMargins(childMargins, blockStartSidesMargins);
        blockStartSidesMargins = blockStartSide.childrenMargins;
        if (!blockStartSide.marginsCollapseWithQuirkContainer || !childMargins.areQuirky())
            blockStartSidesMargins.add(childMargins.margins);
    }
    auto margins = m_adjoiningMargins;
    margins.add(blockStartSidesMargins);
    return m_logicalTop + margins.collapsedValue();
}

LayoutUnit BlockMarginCollapse::collapsedLogicalBottomAtContent() const
{
    // Like endQuirkContainerBlockStartSides().
    auto blockStartSidesMargins = AdjoiningMargins { };
    for (auto blockStartSide : m_quirkContainerBlockStartSides | std::views::reverse) {
        addInnerQuirkContainerMargins(blockStartSide.childMargins, blockStartSidesMargins);
        blockStartSidesMargins = blockStartSide.end(QuirkContainerBlockStartSideEnd::AtContent);
    }
    auto margins = m_adjoiningMargins;
    margins.add(blockStartSidesMargins);
    return m_logicalTop + margins.collapsedValue();
}

void BlockMarginCollapse::resetAdjoiningMargins(LayoutUnit logicalTop, const AdjoiningMargins& margins, bool marginsAreQuirky)
{
    m_adjoiningMargins = margins;
    m_logicalTop = logicalTop;
    m_trimsMarginsAtBlockStartEdge = false;
    m_endsWithQuirk = margins.collapsedValue() && marginsAreQuirky;
}

void BlockMarginCollapse::resetAdjoiningMarginsAfterLineBoxes(LayoutUnit logicalTop)
{
    resetAdjoiningMargins(logicalTop);
    m_endsWithQuirk = std::nullopt;
}

void BlockMarginCollapse::separateMarginsAtLineBox(LayoutUnit lineBoxLogicalTop)
{
    endQuirkContainerBlockStartSides();
    resetAdjoiningMargins(lineBoxLogicalTop);
}

void BlockMarginCollapse::addKeptContentMarginsBefore(ContainingBlockState& blockContainerState, const AdjoiningMargins& margins)
{
    m_adjoiningMargins.add(margins);
    blockContainerState.margins.m_collapsedMarginsBefore.margins.add(margins);
}

void BlockMarginCollapse::startBlockContainer(ContainingBlockState& blockContainerState, const ContainingBlockState& containingBlockState, LayoutUnit marginBefore)
{
    if (m_trimsMarginsAtBlockStartEdge)
        marginBefore = { };
    blockContainerState.margins.m_endsWithQuirkBefore = std::exchange(m_endsWithQuirk, false);
    CheckedRef box = blockContainerState.blockContainer;
    auto isQuirky = box->style().marginBefore().hasQuirk();
    // A child of the quirk container starts the margins that collapse with its block-start edge.
    if (isInnermostQuirkContainerAtBlockStartSide(containingBlockState.blockContainer))
        m_quirkContainerBlockStartSides.last().endChild();
    if (!m_quirkContainerBlockStartSides.isEmpty())
        m_quirkContainerBlockStartSides.last().childMargins.add(marginBefore, isQuirky);
    else
        m_adjoiningMargins.add(marginBefore);
    auto& margins = blockContainerState.margins;
    margins.m_marginBeforeIsQuirky = isQuirky;
    margins.m_collapsedMarginsBefore.add(marginBefore, isQuirky);
    margins.m_collapsesWithContainingBlockMarginBefore = !containingBlockState.borderBoxLogicalTop;
    if (isQuirkContainer(box))
        m_quirkContainerBlockStartSides.append({ .quirkContainer = box });
}

void BlockMarginCollapse::endBlockContainer(const ContainingBlockState& blockContainerState, ContainingBlockState& containingBlockState)
{
    if (isInnermostQuirkContainerAtBlockStartSide(blockContainerState.blockContainer))
        endInnermostQuirkContainerBlockStartSide(QuirkContainerBlockStartSideEnd::AtBlockEnd);
    if (marginsBeforeCollapseWithContainingBlock(blockContainerState, containingBlockState))
        containingBlockState.margins.m_collapsedMarginsBefore.add(blockContainerState.margins.m_collapsedMarginsBefore);
}

void BlockMarginCollapse::startBlockFormattingContextRoot(const ContainingBlockState& rootState)
{
    if (isQuirkContainer(rootState.blockContainer))
        m_quirkContainerBlockStartSides.append({ .quirkContainer = rootState.blockContainer, .marginsCollapseWithQuirkContainer = false });
}

void BlockMarginCollapse::endBlockFormattingContextRoot(const ContainingBlockState& rootState)
{
    if (isInnermostQuirkContainerAtBlockStartSide(rootState.blockContainer))
        endInnermostQuirkContainerBlockStartSide(QuirkContainerBlockStartSideEnd::AtBlockEnd);
}

void BlockMarginCollapse::addCollapsedThroughBlockEndMargin(const ContainingBlockState& blockContainerState, ContainingBlockState& containingBlockState, LayoutUnit marginAfter)
{
    if (m_trimsMarginsAtBlockStartEdge)
        marginAfter = { };
    if (!m_quirkContainerBlockStartSides.isEmpty())
        m_quirkContainerBlockStartSides.last().childMargins.add(marginAfter, { });
    else
        m_adjoiningMargins.add(marginAfter);
    m_endsWithQuirk = blockContainerState.margins.m_endsWithQuirkBefore;
    if (marginsBeforeCollapseWithContainingBlock(blockContainerState, containingBlockState))
        containingBlockState.margins.m_collapsedMarginsBefore.add(marginAfter, { });
}

bool BlockMarginCollapse::addBlockEndMargin(const ContainingBlockState& blockContainerState, LayoutUnit margin, bool isQuirky, bool collapsesWithMarginsAfterContent, LayoutUnit borderBoxLogicalBottom)
{
    // A box that does not collapse through separates the margins before it from the ones after it.
    m_trimsMarginsAtBlockStartEdge = false;
    if (!collapsesWithMarginsAfterContent)
        m_adjoiningMargins = { };
    else if (m_endsWithQuirk)
        isQuirky = *m_endsWithQuirk && (!margin || isQuirky);
    m_adjoiningMargins.add(margin);
    if (m_adjoiningMargins.collapsedValue())
        m_endsWithQuirk = isQuirky;
    else
        m_endsWithQuirk = blockContainerState.margins.m_endsWithQuirkBefore;
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
    if (!isInnermostQuirkContainerAtBlockStartSide(blockContainerState.blockContainer)) {
        endQuirkContainerBlockStartSides();
        return;
    }
    // Its own block-start border or padding separates its block-start margin from its children's, and ends the block-start sides of the
    // quirk containers around it.
    auto blockStartSide = m_quirkContainerBlockStartSides.takeLast();
    endQuirkContainerBlockStartSides();
    blockStartSide.marginsCollapseWithQuirkContainer = false;
    m_quirkContainerBlockStartSides.append(WTF::move(blockStartSide));
}

void BlockMarginCollapse::separateFromContainingBlockMarginBefore(ContainingBlockState& blockContainerState)
{
    blockContainerState.margins.m_collapsesWithContainingBlockMarginBefore = false;
}

void BlockMarginCollapse::QuirkContainerBlockStartSide::endChild()
{
    auto margins = std::exchange(childMargins, { });
    if (marginsCollapseWithQuirkContainer) {
        if (!margins.areQuirky())
            childrenMargins.add(margins.margins);
        return;
    }
    // A child with margins decides whether the ones so far are quirky, but only the before ones of a child whose margins collapse through it.
    if (margins.margins.collapsedValue())
        lastMarginsAreQuirky = margins.areQuirky();
    childrenMargins.add(margins.margins);
}

AdjoiningMargins BlockMarginCollapse::QuirkContainerBlockStartSide::end(QuirkContainerBlockStartSideEnd end)
{
    // The margins are kept unless content ends the block-start side and the last child with margins has quirky ones. When every child
    // collapses through, the quirk container keeps them (RenderBlockFlow::handleAfterSideOfBlock).
    endChild();
    if (end == QuirkContainerBlockStartSideEnd::AtBlockEnd || !lastMarginsAreQuirky)
        return childrenMargins;
    return { };
}

bool BlockMarginCollapse::isInnermostQuirkContainerAtBlockStartSide(const RenderBox& box) const
{
    return !m_quirkContainerBlockStartSides.isEmpty() && m_quirkContainerBlockStartSides.last().quirkContainer.ptr() == &box;
}

void BlockMarginCollapse::endInnermostQuirkContainerBlockStartSide(QuirkContainerBlockStartSideEnd end)
{
    auto margins = m_quirkContainerBlockStartSides.takeLast().end(end);
    if (m_quirkContainerBlockStartSides.isEmpty())
        m_adjoiningMargins.add(margins);
    else
        addInnerQuirkContainerMargins(m_quirkContainerBlockStartSides.last().childMargins, margins);
}

// Content ends the block-start side of every quirk container it is in.
void BlockMarginCollapse::endQuirkContainerBlockStartSides()
{
    while (!m_quirkContainerBlockStartSides.isEmpty())
        endInnermostQuirkContainerBlockStartSide(QuirkContainerBlockStartSideEnd::AtContent);
}

}
}
