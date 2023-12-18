/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#include "AbstractLineBuilder.h"

#include "FontCascade.h"
#include "InlineContentBreaker.h"
#include "InlineFormattingContext.h"

namespace WebCore {
namespace Layout {

AbstractLineBuilder::AbstractLineBuilder(InlineFormattingContext& inlineFormattingContext, HorizontalConstraints rootHorizontalConstraints, const InlineItemList& inlineItemList)
    : m_line(inlineFormattingContext)
    , m_inlineItemList(inlineItemList)
    , m_inlineFormattingContext(inlineFormattingContext)
    , m_rootHorizontalConstraints(rootHorizontalConstraints)
{
}

void AbstractLineBuilder::reset()
{
    m_wrapOpportunityList = { };
    m_partialLeadingTextItem = { };
    m_previousLine = { };
}

std::optional<InlineLayoutUnit> AbstractLineBuilder::eligibleOverflowWidthAsLeading(const InlineContentBreaker::ContinuousContent::RunList& candidateRuns, const InlineContentBreaker::Result& lineBreakingResult, bool isFirstFormattedLine) const
{
    auto eligibleTrailingRunIndex = [&]() -> std::optional<size_t> {
        ASSERT(lineBreakingResult.action == InlineContentBreaker::Result::Action::Wrap || lineBreakingResult.action == InlineContentBreaker::Result::Action::Break);
        if (candidateRuns.size() == 1 && candidateRuns.first().inlineItem.isText()) {
            // A single text run is always a candidate.
            return { 0 };
        }
        if (lineBreakingResult.action == InlineContentBreaker::Result::Action::Break && lineBreakingResult.partialTrailingContent) {
            auto& trailingRun = candidateRuns[lineBreakingResult.partialTrailingContent->trailingRunIndex];
            if (trailingRun.inlineItem.isText())
                return lineBreakingResult.partialTrailingContent->trailingRunIndex;
        }
        return { };
    }();

    if (!eligibleTrailingRunIndex)
        return { };

    auto& overflowingRun = candidateRuns[*eligibleTrailingRunIndex];
    // FIXME: Add support for other types of continuous content.
    ASSERT(is<InlineTextItem>(overflowingRun.inlineItem));
    auto& inlineTextItem = downcast<InlineTextItem>(overflowingRun.inlineItem);
    if (inlineTextItem.isWhitespace())
        return { };
    if (isFirstFormattedLine) {
        auto& usedStyle = overflowingRun.style;
        auto& style = overflowingRun.inlineItem.style();
        if (&usedStyle != &style && usedStyle.fontCascade() != style.fontCascade()) {
            // We may have the incorrect text width when styles differ. Just re-measure the text content when we place it on the next line.
            return { };
        }
    }
    auto logicalWidthForNextLineAsLeading = overflowingRun.logicalWidth;
    if (lineBreakingResult.action == InlineContentBreaker::Result::Action::Wrap)
        return logicalWidthForNextLineAsLeading;
    if (lineBreakingResult.action == InlineContentBreaker::Result::Action::Break && lineBreakingResult.partialTrailingContent->partialRun)
        return logicalWidthForNextLineAsLeading - lineBreakingResult.partialTrailingContent->partialRun->logicalWidth;
    return { };
}

void AbstractLineBuilder::setIntrinsicWidthMode(IntrinsicWidthMode intrinsicWidthMode)
{
    m_intrinsicWidthMode = intrinsicWidthMode;
    m_inlineContentBreaker.setIsMinimumInIntrinsicWidthMode(m_intrinsicWidthMode == IntrinsicWidthMode::Minimum);
}

const ElementBox& AbstractLineBuilder::root() const
{
    return formattingContext().root();
}

const RenderStyle& AbstractLineBuilder::rootStyle() const
{
    return isFirstFormattedLine() ? root().firstLineStyle() : root().style();
}

const InlineLayoutState& AbstractLineBuilder::layoutState() const
{
    return formattingContext().layoutState();
}

InlineLayoutState& AbstractLineBuilder::layoutState()
{
    return formattingContext().layoutState();
}

}
}
