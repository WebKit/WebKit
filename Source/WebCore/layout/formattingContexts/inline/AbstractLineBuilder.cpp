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
#include "RenderStyle+GettersInlines.h"

namespace WebCore {
namespace Layout {

AbstractLineBuilder::AbstractLineBuilder(InlineFormattingContext& inlineFormattingContext, const ElementBox& rootBox, HorizontalConstraints rootHorizontalConstraints, const InlineItemList& inlineItemList)
    : m_line(inlineFormattingContext)
    , m_inlineItemList(inlineItemList.span())
    , m_inlineFormattingContext(inlineFormattingContext)
    , m_rootBox(rootBox)
    , m_rootHorizontalConstraints(rootHorizontalConstraints)
{
}

void AbstractLineBuilder::reset()
{
    m_wrapOpportunityList.shrink(0);
    m_partialLeadingTextItem = { };
    m_previousLine = { };
    m_isFirstFormattedLineCandidate = false;
}

std::optional<InlineLayoutUnit> AbstractLineBuilder::overflowWidthAsLeadingForNextLine(const InlineContentBreaker::ContinuousContent::RunList& candidateRuns, const InlineContentBreaker::Result& lineBreakingResult) const
{
    auto trailingTextRunIndexAsLeadingCandidate = [&]() -> std::optional<size_t> {
        ASSERT(lineBreakingResult.action == InlineContentBreaker::Result::Action::Wrap || lineBreakingResult.action == InlineContentBreaker::Result::Action::Break);

        auto trailingRunIndex = [&]() -> std::optional<size_t> {
            if (candidateRuns.size() == 1)
                return { 0 };
            if (lineBreakingResult.action == InlineContentBreaker::Result::Action::Break && lineBreakingResult.partialTrailingContent)
                return lineBreakingResult.partialTrailingContent->trailingRunIndex;
            return { };
        };
        auto candidateIndex = trailingRunIndex();
        if (!candidateIndex || !candidateRuns[*candidateIndex].inlineItem.isText())
            return { };
        auto& inlineTextItem = downcast<InlineTextItem>(candidateRuns[*candidateIndex].inlineItem);
        // For whitespace content we can only re-use measured width when no position dependent character(s) is present.
        return !inlineTextItem.isWhitespace() || inlineTextItem.width().has_value() ? candidateIndex : std::nullopt;
    };

    auto index = trailingTextRunIndexAsLeadingCandidate();
    if (!index)
        return { };

    auto& overflowingRun = candidateRuns[*index];
    auto* inlineTextItem = dynamicDowncast<InlineTextItem>(overflowingRun.inlineItem);
    if (!inlineTextItem) {
        ASSERT_NOT_REACHED();
        return { };
    }
    if (isFirstFormattedLineCandidate()) {
        CheckedRef usedStyle = overflowingRun.style;
        CheckedRef style = overflowingRun.inlineItem.style();
        if (usedStyle.ptr() != style.ptr() && !usedStyle->fontCascadeEqual(style)) {
            // We may have the incorrect text width when styles differ. Just re-measure the text content when we place it on the next line.
            return { };
        }
    }
    auto logicalWidthForNextLineAsLeading = overflowingRun.contentWidth();
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

const RenderStyle& AbstractLineBuilder::rootStyle() const
{
    return isFirstFormattedLineCandidate() ? root().firstLineStyle() : root().style();
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
