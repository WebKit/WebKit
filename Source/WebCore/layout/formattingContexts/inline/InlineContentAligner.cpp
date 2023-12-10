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
#include "InlineContentAligner.h"

#include "TextUtil.h"

namespace WebCore {
namespace Layout {

static void computedExpansions(const Line::RunList& runs, WTF::Range<size_t> runRange, size_t hangingTrailingWhitespaceLength, ExpansionInfo& expansionInfo)
{
    // Collect and distribute the expansion opportunities.
    expansionInfo.opportunityCount = 0;
    auto rangeSize = runRange.end() - runRange.begin();
    if (rangeSize > runs.size()) {
        ASSERT_NOT_REACHED();
        return;
    }
    expansionInfo.opportunityList.resizeToFit(rangeSize);
    expansionInfo.behaviorList.resizeToFit(rangeSize);
    auto lastExpansionIndexWithContent = std::optional<size_t> { };

    // Line start behaves as if we had an expansion here (i.e. first runs should not start with allowing left expansion).
    auto runIsAfterExpansion = true;
    auto lastTextRunIndexForTrimming = [&]() -> std::optional<size_t> {
        if (!hangingTrailingWhitespaceLength)
            return { };
        for (auto index = runs.size(); index--;) {
            if (runs[index].isText())
                return index;
        }
        return { };
    }();
    for (size_t index = 0; index < rangeSize; ++index) {
        auto runIndex = runRange.begin() + index;
        auto& run = runs[runIndex];
        auto expansionBehavior = ExpansionBehavior::defaultBehavior();
        size_t expansionOpportunitiesInRun = 0;

        // According to the CSS3 spec, a UA can determine whether or not
        // it wishes to apply text-align: justify to text with collapsible spaces (and this behavior matches Blink).
        auto mayAlterSpacingWithinText = !TextUtil::shouldPreserveSpacesAndTabs(run.layoutBox()) || hangingTrailingWhitespaceLength;
        if (run.isText() && mayAlterSpacingWithinText) {
            if (run.hasTextCombine())
                expansionBehavior = ExpansionBehavior::forbidAll();
            else {
                expansionBehavior.left = runIsAfterExpansion ? ExpansionBehavior::Behavior::Forbid : ExpansionBehavior::Behavior::Allow;
                expansionBehavior.right = ExpansionBehavior::Behavior::Allow;
                auto& textContent = *run.textContent();
                auto length = textContent.length;
                if (lastTextRunIndexForTrimming && runIndex == *lastTextRunIndexForTrimming) {
                    // Trailing hanging whitespace sequence is ignored when computing the expansion opportunities.
                    length -= hangingTrailingWhitespaceLength;
                }
                std::tie(expansionOpportunitiesInRun, runIsAfterExpansion) = FontCascade::expansionOpportunityCount(StringView(downcast<InlineTextBox>(run.layoutBox()).content()).substring(textContent.start, length), run.inlineDirection(), expansionBehavior);
            }
        } else if (run.isBox())
            runIsAfterExpansion = false;

        expansionInfo.behaviorList[index] = expansionBehavior;
        expansionInfo.opportunityList[index] = expansionOpportunitiesInRun;
        expansionInfo.opportunityCount += expansionOpportunitiesInRun;

        if (run.isText() || run.isBox())
            lastExpansionIndexWithContent = index;
    }
    // Forbid right expansion in the last run to prevent trailing expansion at the end of the line.
    if (lastExpansionIndexWithContent && expansionInfo.opportunityList[*lastExpansionIndexWithContent]) {
        expansionInfo.behaviorList[*lastExpansionIndexWithContent].right = ExpansionBehavior::Behavior::Forbid;
        if (runIsAfterExpansion) {
            // When the last run has an after expansion (e.g. CJK ideograph) we need to remove this trailing expansion opportunity.
            // Note that this is not about trailing collapsible whitespace as at this point we trimmed them all.
            ASSERT(expansionInfo.opportunityCount && expansionInfo.opportunityList[*lastExpansionIndexWithContent]);
            --expansionInfo.opportunityCount;
            --expansionInfo.opportunityList[*lastExpansionIndexWithContent];
        }
    }
}

InlineLayoutUnit InlineContentAligner::applyExpansionOnRange(Line::RunList& runs, WTF::Range<size_t> range, const ExpansionInfo& expansion, InlineLayoutUnit spaceToDistribute)
{
    ASSERT(spaceToDistribute > 0);
    ASSERT(expansion.opportunityCount);
    // Distribute the extra space.
    auto expansionToDistribute = spaceToDistribute / expansion.opportunityCount;
    auto accumulatedExpansion = InlineLayoutUnit { };
    auto rangeSize = range.distance();
    if (range.end() > runs.size()) {
        ASSERT_NOT_REACHED();
        return { };
    }
    for (size_t index = 0; index < rangeSize; ++index) {
        auto& run = runs[range.begin() + index];
        // Move runs by the accumulated expansion first
        run.moveHorizontally(accumulatedExpansion);
        // and expand.
        auto computedExpansion = expansionToDistribute * expansion.opportunityList[index];
        run.setExpansion({ expansion.behaviorList[index], computedExpansion });
        run.shrinkHorizontally(-computedExpansion);
        accumulatedExpansion += computedExpansion;
    }
    // Content grows as runs expand.
    return accumulatedExpansion;
}

InlineLayoutUnit InlineContentAligner::applyTextAlignJustify(Line::RunList& runs, InlineLayoutUnit spaceToDistribute, size_t hangingTrailingWhitespaceLength)
{
    if (runs.isEmpty()) {
        ASSERT_NOT_REACHED();
        return { };
    }

    if (spaceToDistribute <= 0)
        return { };

    auto expansion = ExpansionInfo { };
    auto fullRange = WTF::Range<size_t> { 0, runs.size() };
    computedExpansions(runs, fullRange, hangingTrailingWhitespaceLength, expansion);
    // Anything to distribute?
    if (!expansion.opportunityCount)
        return { };
    return applyExpansionOnRange(runs, fullRange, expansion, spaceToDistribute);
}

void InlineContentAligner::applyRubyAlign(Line::RunList& runs, WTF::Range<size_t> rubyBaseRange, InlineLayoutUnit spaceToDistribute, size_t hangingTrailingWhitespaceLength)
{
    if (rubyBaseRange.distance() <= 1)
        return;
    // FIXME: ruby-align: space-around only
    // As for space-between except that there exists an extra justification opportunities whose space is distributed half before and half after the ruby content.
    auto baseContentRunRange = WTF::Range<size_t> { rubyBaseRange.begin() + 1, rubyBaseRange.end() - 1 };
    auto expansion = ExpansionInfo { };
    computedExpansions(runs, baseContentRunRange, hangingTrailingWhitespaceLength, expansion);

    if (expansion.opportunityCount) {
        auto contentLogicalLeftOffset = spaceToDistribute / (expansion.opportunityCount + 1) / 2;
        spaceToDistribute -= (2 * contentLogicalLeftOffset);
        applyExpansionOnRange(runs, baseContentRunRange, expansion, spaceToDistribute);
    }
}

}
}

