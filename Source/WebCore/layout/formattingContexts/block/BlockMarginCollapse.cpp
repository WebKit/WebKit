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
#include "LayoutBoxGeometry.h"
#include "RenderBlockFlow.h"
#include "StyleComputedStyle+GettersInlines.h"

namespace WebCore {
namespace Layout {

bool BlockMarginCollapse::marginsCollapseThrough(const RenderBox& box, const BoxGeometry& geometry, std::optional<LayoutUnit> containingBlockContentBoxBlockSize, WritingMode writingMode)
{
    if (geometry.borderAndPaddingAfter())
        return false;
    auto& style = box.style();
    // Like RenderBlock::isSelfCollapsingBlock, a calc() min-height counts as non-zero.
    auto& minimumBlockSize = style.logicalMinHeight(writingMode);
    if (minimumBlockSize.isKnownPositive() || minimumBlockSize.isCalculated())
        return false;
    auto& blockSize = style.logicalHeight(writingMode);
    return blockSize.isKnownZero() || BlockFormattingUtils::blockSizeBehavesAsAuto(blockSize, containingBlockContentBoxBlockSize);
}

void BlockMarginCollapse::resetAdjoiningMargins(LayoutUnit logicalTop)
{
    m_adjoiningMargins = { };
    m_logicalTop = logicalTop;
}

void BlockMarginCollapse::startBlockContainer(ContainingBlockState& blockContainerState, const ContainingBlockState& containingBlockState, LayoutUnit marginBefore)
{
    m_adjoiningMargins.add(marginBefore);
    auto& margins = blockContainerState.margins;
    margins.m_collapsedMarginsBefore.add(marginBefore);
    margins.m_collapsesWithContainingBlockMarginBefore = !containingBlockState.borderBoxLogicalTop;
}

void BlockMarginCollapse::endBlockContainer(const ContainingBlockState& blockContainerState, ContainingBlockState& containingBlockState)
{
    auto& margins = blockContainerState.margins;
    if (margins.m_collapsesWithContainingBlockMarginBefore)
        containingBlockState.margins.m_collapsedMarginsBefore.add(margins.m_collapsedMarginsBefore);
}

void BlockMarginCollapse::addCollapsedThroughBlockEndMargin(const ContainingBlockState& blockContainerState, ContainingBlockState& containingBlockState, LayoutUnit marginAfter)
{
    m_adjoiningMargins.add(marginAfter);
    if (blockContainerState.margins.m_collapsesWithContainingBlockMarginBefore)
        containingBlockState.margins.m_collapsedMarginsBefore.add(marginAfter);
}

void BlockMarginCollapse::addBlockEndMargin(LayoutUnit margin, bool collapsesWithMarginsAfterContent, LayoutUnit borderBoxLogicalBottom)
{
    if (!collapsesWithMarginsAfterContent)
        m_adjoiningMargins = { };
    m_adjoiningMargins.add(margin);
    m_logicalTop = borderBoxLogicalBottom;
}

}
}
