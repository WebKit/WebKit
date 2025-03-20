/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#include "RangeBasedLineBuilder.h"

#include "InlineFormattingContext.h"

namespace WebCore {
namespace Layout {

static inline bool hasInlineBoxesOnly(size_t inlineBoxCount, size_t numberOfInlineItems)
{
    return !(numberOfInlineItems % 2) && inlineBoxCount == numberOfInlineItems / 2;
}

RangeBasedLineBuilder::RangeBasedLineBuilder(InlineFormattingContext& inlineFormattingContext, HorizontalConstraints rootHorizontalConstraints, const InlineContentCache::InlineItems& inlineItems)
    : AbstractLineBuilder(inlineFormattingContext, inlineFormattingContext.root(), rootHorizontalConstraints, inlineItems.content())
    , m_textOnlySimpleLineBuilder(inlineFormattingContext, downcast<ElementBox>(inlineItems.content()[0].layoutBox()), rootHorizontalConstraints, inlineItems.content())
    , m_inlineBoxCount(inlineItems.inlineBoxCount())
{
    ASSERT(m_inlineBoxCount);
}

LineLayoutResult RangeBasedLineBuilder::layoutInlineContent(const LineInput& lineInput, const std::optional<PreviousLine>& previousLine)
{
    auto numberOfInlineItems = m_inlineItemList.size();
    if (hasInlineBoxesOnly(m_inlineBoxCount, numberOfInlineItems)) {
        Line::RunList inlineBoxRuns;
        inlineBoxRuns.reserveCapacity(numberOfInlineItems);
        for (auto& inlineItem : m_inlineItemList) {
            ASSERT(inlineItem.isInlineBoxStartOrEnd());
            inlineBoxRuns.append({ Line::Run(inlineItem, inlineItem.firstLineStyle(), { }) });
        }

        auto lineRect = lineInput.initialLogicalRect;
        auto contentLeft = InlineFormattingUtils::horizontalAlignmentOffset(rootStyle(), { }, lineRect.width(), { }, inlineBoxRuns, true);
        return LineLayoutResult { lineInput.needsLayoutRange
            , WTFMove(inlineBoxRuns)
            , { }
            , { contentLeft, { }, contentLeft, std::max(0.f, contentLeft - lineRect.right()) }
            , { lineRect.topLeft(), lineRect.width(), lineRect.left() }
            , { }
            , { }
            , { }
            , { }
            , { }
            , m_inlineBoxCount
        };
    }

    // 1. Shrink the layout range that we can run text-only builder on (currently it's just the opening/closing inline box)
    // 2. Run text-only line builder
    // 3. Insert/append the missing inline box run
    auto isFirstLine = !lineInput.needsLayoutRange.startIndex();

    auto adjustedNeedsLayoutRange = [&] {
        auto needsLayoutRange = lineInput.needsLayoutRange;
        if (isFirstLine) {
            ASSERT(m_inlineItemList.front().isInlineBoxStart());
            ASSERT(!needsLayoutRange.start.offset);
            // Skip leading InlineItemStart (e.g. <span>)
            ++needsLayoutRange.start.index;
        }
        // SKip trailing InlineItemEnd (e.g. </span>)
        ASSERT(m_inlineItemList.back().isInlineBoxEnd());
        ASSERT(!needsLayoutRange.end.offset);
        --needsLayoutRange.end.index;
        return needsLayoutRange;
    };
    auto needsLayoutRange = adjustedNeedsLayoutRange();
    ASSERT(!needsLayoutRange.isEmpty());

    auto lineLayoutResult = m_textOnlySimpleLineBuilder.layoutInlineContent({ needsLayoutRange, lineInput.initialLogicalRect }, previousLine);

    auto insertLeadingInlineBoxRun = [&] {
        auto& leadingInlineItem = m_inlineItemList.front();
        ASSERT(leadingInlineItem.isInlineBoxStart());

        if (isFirstLine) {
            ASSERT(!previousLine);
            lineLayoutResult.inlineContent.insert(0, { leadingInlineItem, leadingInlineItem.firstLineStyle(), { } });
            lineLayoutResult.inlineItemRange.start = lineInput.needsLayoutRange.start;
            return;
        }
        // Subsequent lines need leading spanning inline box run.
        lineLayoutResult.inlineContent.insert(0, { leadingInlineItem, { }, { } });
    };
    insertLeadingInlineBoxRun();

    auto appendTrailingInlineBoxRunIfNeeded = [&] {
        if (lineLayoutResult.inlineItemRange.end != needsLayoutRange.end)
            return;
        auto& trailingInlineItem = m_inlineItemList.back();
        lineLayoutResult.inlineContent.append({ trailingInlineItem, isFirstLine ? trailingInlineItem.firstLineStyle() : trailingInlineItem.style(), lineLayoutResult.contentGeometry.logicalWidth });
        lineLayoutResult.inlineItemRange.end = lineInput.needsLayoutRange.end;
    };
    appendTrailingInlineBoxRunIfNeeded();

    return lineLayoutResult;
}

bool RangeBasedLineBuilder::isEligibleForRangeInlineLayout(const InlineFormattingContext& inlineFormattingContext, const InlineContentCache::InlineItems& inlineItems, const PlacedFloats& placedFloats)
{
    if (inlineItems.isEmpty())
        return false;

    // Range based line builder only supports the following content <inline box>eligible for text only layout</inline box>
    auto& inlineItemList = inlineItems.content();
    auto numberOfInlineItems = inlineItemList.size();
    auto isEmptyContent = hasInlineBoxesOnly(inlineItems.inlineBoxCount(), numberOfInlineItems);
    auto isFullyNestedContent = inlineItems.inlineBoxCount() == 1 && inlineItemList.first().isInlineBoxStart() && inlineItemList.last().isInlineBoxEnd() && numberOfInlineItems > 2;
    if (!isEmptyContent && !isFullyNestedContent)
        return false;

    auto hasDecorationOrBreak = [&] {
        for (auto& inlineItem : inlineItemList) {
            if (!inlineItem.isInlineBoxStart())
                return false;
            auto& inlineBox = inlineItem.layoutBox();
            if (inlineFormattingContext.geometryForBox(inlineBox).horizontalMarginBorderAndPadding())
                return true;
            if (inlineBox.style().boxDecorationBreak() != RenderStyle::initialBoxDecorationBreak())
                return true;
        }
        ASSERT_NOT_REACHED();
        return true;
    };

    if (hasDecorationOrBreak()) {
        // FIXME: Add start decoration support is just a matter of shrinking the available space for the first line (or on subsequent lines when decoration break is present)
        return false;
    }

    if (inlineFormattingContext.layoutState().parentBlockLayoutState().lineClamp())
        return false;

    if (!isEmptyContent) {
        // Check the nested text content.
        if (!inlineItems.hasTextAndLineBreakOnlyContent() || inlineItems.requiresVisualReordering() || !placedFloats.isEmpty() || inlineItems.hasTextAutospace())
            return false;

        auto& inlineBox = inlineItemList.first().layoutBox();
        if (!TextOnlySimpleLineBuilder::isEligibleForSimplifiedInlineLayoutByStyle(inlineFormattingContext.root().style()) || !TextOnlySimpleLineBuilder::isEligibleForSimplifiedInlineLayoutByStyle(inlineBox.style()))
            return false;
    }

    return true;
}

}
}

