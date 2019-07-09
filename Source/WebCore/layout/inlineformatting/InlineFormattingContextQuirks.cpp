/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#include "InlineItem.h"
#include "InlineLine.h"
#include "InlineTextItem.h"
#include "LayoutState.h"

namespace WebCore {
namespace Layout {

bool InlineFormattingContext::Quirks::lineDescentNeedsCollapsing(const LayoutState& layoutState, const Line::Content& lineContent)
{
    // Collapse line descent in limited and full quirk mode when there's no baseline aligned content or
    // the baseline aligned content has no descent.
    if (!layoutState.inQuirksMode() && !layoutState.inLimitedQuirksMode())
        return false;

    for (auto& run : lineContent.runs()) {
        auto& layoutBox = run->layoutBox();
        if (layoutBox.style().verticalAlign() != VerticalAlign::Baseline)
            continue;

        switch (run->type()) {
        case InlineItem::Type::Text:
            if (!run->textContext() || !run->textContext()->isCollapsed)
                return false;
            break;
        case InlineItem::Type::HardLineBreak:
            return false;
        case InlineItem::Type::ContainerStart: {
            auto& displayBox = layoutState.displayBoxForLayoutBox(layoutBox);
            if (displayBox.horizontalBorder() || (displayBox.horizontalPadding() && displayBox.horizontalPadding().value()))
                return false;
            break;
        }
        case InlineItem::Type::ContainerEnd:
            break;
        case InlineItem::Type::Box: {
            if (layoutBox.isInlineBlockBox() && layoutBox.establishesInlineFormattingContext()) {
                auto& formattingState = downcast<InlineFormattingState>(layoutState.establishedFormattingState(layoutBox));
                ASSERT(!formattingState.lineBoxes().isEmpty());
                auto inlineBlockBaseline = formattingState.lineBoxes().last().baseline();
                if (inlineBlockBaseline.descent)
                    return false;
            }
            break;
        }
        default:
            ASSERT_NOT_REACHED();
            break;
        }
    }
    return true;
}

Line::InitialConstraints::HeightAndBaseline InlineFormattingContext::Quirks::lineHeightConstraints(const LayoutState& layoutState, const Box& formattingRoot)
{
    // computedLineHeight takes font-size into account when line-height is not set.
    // Strut is the imaginary box that we put on every line. It sets the initial vertical constraints for each new line.
    auto strutHeight = formattingRoot.style().computedLineHeight();
    auto strutBaselineOffset = Line::halfLeadingMetrics(formattingRoot.style().fontMetrics(), strutHeight).ascent;
    if (layoutState.inNoQuirksMode())
        return { strutHeight, strutBaselineOffset, { } };

    auto lineHeight = formattingRoot.style().lineHeight();
    if (lineHeight.isPercentOrCalculated()) {
        auto initialBaselineOffset = Line::halfLeadingMetrics(formattingRoot.style().fontMetrics(), { }).ascent;
        return { initialBaselineOffset, initialBaselineOffset, LineBox::Baseline { strutBaselineOffset, strutHeight - strutBaselineOffset } };
    }
    // FIXME: The only reason why we use intValue() here is to match current inline tree (integral)behavior.
    auto initialLineHeight = LayoutUnit { lineHeight.intValue() };
    auto initialBaselineOffset = Line::halfLeadingMetrics(formattingRoot.style().fontMetrics(), initialLineHeight).ascent;
    return { initialLineHeight, initialBaselineOffset, LineBox::Baseline { strutBaselineOffset, strutHeight - strutBaselineOffset } };
}

}
}

#endif
