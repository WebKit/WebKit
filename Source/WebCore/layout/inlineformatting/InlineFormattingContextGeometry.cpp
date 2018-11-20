/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#include "InlineFormattingContext.h"

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "FormattingContext.h"
#include "InlineFormattingState.h"
#include "LayoutBox.h"
#include "LayoutContainer.h"
#include "LayoutFormattingState.h"

namespace WebCore {
namespace Layout {

WidthAndMargin InlineFormattingContext::Geometry::inlineBlockWidthAndMargin(LayoutState& layoutState, const Box& formattingContextRoot)
{
    ASSERT(formattingContextRoot.isInFlow());

    // 10.3.10 'Inline-block', replaced elements in normal flow

    // Exactly as inline replaced elements.
    if (formattingContextRoot.replaced())
        return inlineReplacedWidthAndMargin(layoutState, formattingContextRoot);

    // 10.3.9 'Inline-block', non-replaced elements in normal flow

    // If 'width' is 'auto', the used value is the shrink-to-fit width as for floating elements.
    // A computed value of 'auto' for 'margin-left' or 'margin-right' becomes a used value of '0'.
    auto& containingBlock = *formattingContextRoot.containingBlock();
    auto containingBlockWidth = layoutState.displayBoxForLayoutBox(containingBlock).contentBoxWidth();
    // #1
    auto width = computedValueIfNotAuto(formattingContextRoot.style().logicalWidth(), containingBlockWidth);
    if (!width)
        width = shrinkToFitWidth(layoutState, formattingContextRoot);

    // #2
    auto margin = computedNonCollapsedHorizontalMarginValue(layoutState, formattingContextRoot);

    return WidthAndMargin { *width, margin, margin };
}

HeightAndMargin InlineFormattingContext::Geometry::inlineBlockHeightAndMargin(const LayoutState& layoutState, const Box& layoutBox)
{
    ASSERT(layoutBox.isInFlow());

    // 10.6.2 Inline replaced elements, block-level replaced elements in normal flow, 'inline-block' replaced elements in normal flow and floating replaced elements
    if (layoutBox.replaced())
        return inlineReplacedHeightAndMargin(layoutState, layoutBox);

    // 10.6.6 Complicated cases
    // - 'Inline-block', non-replaced elements.
    return complicatedCases(layoutState, layoutBox);
}

static LayoutUnit adjustedLineLogicalLeft(TextAlignMode align, LayoutUnit lineLogicalLeft, LayoutUnit remainingWidth)
{
    switch (align) {
    case TextAlignMode::Left:
    case TextAlignMode::WebKitLeft:
    case TextAlignMode::Start:
        return lineLogicalLeft;
    case TextAlignMode::Right:
    case TextAlignMode::WebKitRight:
    case TextAlignMode::End:
        return lineLogicalLeft + std::max(remainingWidth, LayoutUnit());
    case TextAlignMode::Center:
    case TextAlignMode::WebKitCenter:
        return lineLogicalLeft + std::max(remainingWidth / 2, LayoutUnit());
    case TextAlignMode::Justify:
        ASSERT_NOT_REACHED();
        break;
    }
    ASSERT_NOT_REACHED();
    return lineLogicalLeft;
}

void InlineFormattingContext::Geometry::justifyRuns(Line& line)
{
    auto& inlineRuns = line.runs();
    auto& lastInlineRun = inlineRuns.last();

    // Adjust (forbid) trailing expansion for the last text run on line.
    auto expansionBehavior = lastInlineRun.expansionOpportunity().behavior;
    // Remove allow and add forbid.
    expansionBehavior ^= AllowTrailingExpansion;
    expansionBehavior |= ForbidTrailingExpansion;
    lastInlineRun.expansionOpportunity().behavior = expansionBehavior;

    // Collect expansion opportunities and justify the runs.
    auto widthToDistribute = line.availableWidth();
    if (widthToDistribute <= 0)
        return;

    auto expansionOpportunities = 0;
    for (auto& inlineRun : inlineRuns)
        expansionOpportunities += inlineRun.expansionOpportunity().count;

    if (!expansionOpportunities)
        return;

    float expansion = widthToDistribute.toFloat() / expansionOpportunities;
    LayoutUnit accumulatedExpansion;
    for (auto& inlineRun : inlineRuns) {
        auto expansionForRun = inlineRun.expansionOpportunity().count * expansion;

        inlineRun.expansionOpportunity().expansion = expansionForRun;
        inlineRun.setLogicalLeft(inlineRun.logicalLeft() + accumulatedExpansion);
        inlineRun.setWidth(inlineRun.width() + expansionForRun);
        accumulatedExpansion += expansionForRun;
    }
}

void InlineFormattingContext::Geometry::computeExpansionOpportunities(Line& line, const InlineRunProvider::Run& run, InlineRunProvider::Run::Type lastRunType)
{
    auto isExpansionOpportunity = [](auto currentRunIsWhitespace, auto lastRunIsWhitespace) {
        return currentRunIsWhitespace || (!currentRunIsWhitespace && !lastRunIsWhitespace);
    };

    auto expansionBehavior = [](auto isAtExpansionOpportunity) {
        ExpansionBehavior expansionBehavior = AllowTrailingExpansion;
        expansionBehavior |= isAtExpansionOpportunity ? ForbidLeadingExpansion : AllowLeadingExpansion;
        return expansionBehavior;
    };

    auto isAtExpansionOpportunity = isExpansionOpportunity(run.isWhitespace(), lastRunType == InlineRunProvider::Run::Type::Whitespace);

    auto& currentInlineRun = line.runs().last();
    auto& expansionOpportunity = currentInlineRun.expansionOpportunity();
    if (isAtExpansionOpportunity)
        ++expansionOpportunity.count;

    expansionOpportunity.behavior = expansionBehavior(isAtExpansionOpportunity);
}

void InlineFormattingContext::Geometry::alignRuns(TextAlignMode textAlign, Line& line,  IsLastLine isLastLine)
{
    auto adjutedTextAlignment = textAlign != TextAlignMode::Justify ? textAlign : isLastLine == IsLastLine::No ? TextAlignMode::Justify : TextAlignMode::Left;
    if (adjutedTextAlignment == TextAlignMode::Justify) {
        justifyRuns(line);
        return;
    }

    auto lineLogicalLeft = line.contentLogicalLeft();
    auto adjustedLogicalLeft = adjustedLineLogicalLeft(adjutedTextAlignment, lineLogicalLeft, line.availableWidth());
    if (adjustedLogicalLeft == lineLogicalLeft)
        return;

    auto delta = adjustedLogicalLeft - lineLogicalLeft;
    for (auto& inlineRun : line.runs())
        inlineRun.setLogicalLeft(inlineRun.logicalLeft() + delta);
}

}
}

#endif
