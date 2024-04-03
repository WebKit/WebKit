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

#include "InlineFormattingContext.h"
#include "TextUtil.h"

namespace WebCore {
namespace Layout {

static inline void shiftDisplayBox(InlineDisplay::Box& displayBox, InlineLayoutUnit offset, InlineFormattingContext& inlineFormattingContext)
{
    if (!offset)
        return;
    auto isHorizontalWritingMode = inlineFormattingContext.root().style().isHorizontalWritingMode();
    isHorizontalWritingMode ? displayBox.moveHorizontally(offset) : displayBox.moveVertically(offset);
    if (!displayBox.isTextOrSoftLineBreak() && !displayBox.isRootInlineBox())
        inlineFormattingContext.geometryForBox(displayBox.layoutBox()).moveHorizontally(LayoutUnit { offset });
}

static inline void expandInlineBox(InlineLayoutUnit expansion, InlineDisplay::Box& displayBox, InlineFormattingContext& inlineFormattingContext)
{
    if (!displayBox.isInlineBox()) {
        ASSERT_NOT_REACHED();
        return;
    }
    if (!expansion)
        return;
    inlineFormattingContext.root().style().isHorizontalWritingMode() ? displayBox.expandHorizontally(expansion) : displayBox.expandVertically(expansion);
    auto& boxGeometry = inlineFormattingContext.geometryForBox(displayBox.layoutBox());
    boxGeometry.setContentBoxWidth(boxGeometry.contentBoxWidth() + LayoutUnit { expansion });
}

static inline InlineLayoutUnit alignmentOffset(auto& latyoutBox, auto& alignmentOffsetList)
{
    auto alignmentOffsetEntry = alignmentOffsetList.find(&latyoutBox);
    return alignmentOffsetEntry != alignmentOffsetList.end() ? alignmentOffsetEntry->value : 0.f;
}

struct InlineBoxIndexAndExpansion {
    size_t index { 0 };
    InlineLayoutUnit expansion { 0.f };
};
static InlineBoxIndexAndExpansion expandInlineBoxWithDescendants(size_t inlineBoxIndex, InlineDisplay::Boxes& displayBoxes, const HashMap<const Box*, InlineLayoutUnit>& alignmentOffsetList,  InlineFormattingContext& inlineFormattingContext)
{
    if (inlineBoxIndex >= displayBoxes.size() || !displayBoxes[inlineBoxIndex].isInlineBox()) {
        ASSERT_NOT_REACHED();
        return { inlineBoxIndex, { } };
    }
    auto& inlineBox = displayBoxes[inlineBoxIndex].layoutBox();
    auto descendantExpansion = InlineLayoutUnit { 0.f };
    size_t index = inlineBoxIndex + 1;
    while (index < displayBoxes.size() && &displayBoxes[index].layoutBox().parent() == &inlineBox) {
        if (displayBoxes[index].isInlineBox()) {
            auto indexAndExpansion = expandInlineBoxWithDescendants(index, displayBoxes, alignmentOffsetList, inlineFormattingContext);
            index = indexAndExpansion.index;
            descendantExpansion += indexAndExpansion.expansion;
            continue;
        }
        ++index;
    }
    auto totalExpansion = 2 * alignmentOffset(inlineBox, alignmentOffsetList) + descendantExpansion;
    if (inlineBoxIndex) {
        // Root inline box has the correct (inflated) logical width.
        expandInlineBox(totalExpansion, displayBoxes[inlineBoxIndex], inlineFormattingContext);
    }
    return { index, totalExpansion };
}

struct BaseIndexAndOffset {
    size_t index { 0 };
    InlineLayoutUnit offset { 0.f };
};
static BaseIndexAndOffset shiftRubyBaseContentByAlignmentOffset(BaseIndexAndOffset baseIndexAndOffset, InlineDisplay::Boxes& displayBoxes, const HashMap<const Box*, InlineLayoutUnit>& alignmentOffsetList, InlineContentAligner::AdjustContentOnlyInsideRubyBase adjustContentOnlyInsideRubyBase, InlineFormattingContext& inlineFormattingContext)
{
    auto baseIndex = baseIndexAndOffset.index;
    if (baseIndex >= displayBoxes.size() || !displayBoxes[baseIndex].layoutBox().isRubyBase()) {
        ASSERT_NOT_REACHED();
        return { baseIndexAndOffset.index, { } };
    }

    // Shift base content within the base (no resize) as part of the alignment process.
    auto& rootBox = inlineFormattingContext.root();
    auto& rubyBaseBox = displayBoxes[baseIndex].layoutBox();
    auto baseOffset = baseIndexAndOffset.offset;
    auto baseContentOffset = alignmentOffset(rubyBaseBox, alignmentOffsetList);
    size_t baseContentIndex = baseIndex + 1;

    while (baseContentIndex < displayBoxes.size()) {
        auto& displayBox = displayBoxes[baseContentIndex];
        auto& layoutBox = displayBox.layoutBox();
        auto isInsideCurrentRubyBase = [&] {
            // Ruby content tends to produce flat structures.
            for (auto* ancestor = &layoutBox.parent(); ancestor; ancestor = &ancestor->parent()) {
                if (ancestor == &rubyBaseBox)
                    return true;
                if (ancestor->isRubyBase() || ancestor->isRuby() || ancestor == &rootBox)
                    return false;
            }
            return false;
        };
        if (!isInsideCurrentRubyBase())
            break;
        if (!layoutBox.isRubyAnnotationBox())
            shiftDisplayBox(displayBox, baseOffset + baseContentOffset, inlineFormattingContext);
        if (layoutBox.isRubyBase()) {
            auto baseEndIndexAndAlignment = shiftRubyBaseContentByAlignmentOffset({ baseContentIndex, baseOffset + baseContentOffset }, displayBoxes, alignmentOffsetList, adjustContentOnlyInsideRubyBase, inlineFormattingContext);
            baseContentIndex = baseEndIndexAndAlignment.index;
            if (adjustContentOnlyInsideRubyBase == InlineContentAligner::AdjustContentOnlyInsideRubyBase::No)
                baseOffset += baseEndIndexAndAlignment.offset;
            continue;
        }
        ++baseContentIndex;
    }
    auto accumulatedOffset = 2 * baseContentOffset;
    if (adjustContentOnlyInsideRubyBase == InlineContentAligner::AdjustContentOnlyInsideRubyBase::No)
        accumulatedOffset += baseOffset;
    return { baseContentIndex, accumulatedOffset };
}

enum class IgnoreRubyRange : bool { No, Yes };
static void computedExpansions(const Line::RunList& runs, WTF::Range<size_t> runRange, size_t hangingTrailingWhitespaceLength, ExpansionInfo& expansionInfo, IgnoreRubyRange ignoreRuby)
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
        auto runIndex = [&] {
            return runRange.begin() + index;
        };
        auto skipRubyContentIfApplicable = [&] {
            auto& rubyBox = runs[runIndex()].layoutBox();
            if (ignoreRuby == IgnoreRubyRange::No || !rubyBox.isRuby())
                return;
            runIsAfterExpansion = false;
            for (; index < rangeSize; ++index) {
                expansionInfo.behaviorList[index] = ExpansionBehavior::defaultBehavior();
                expansionInfo.opportunityList[index] = 0;
                auto& run = runs[runIndex()];
                if (run.isInlineBoxEnd() && &run.layoutBox() == &rubyBox) {
                    ++index;
                    return;
                }
            }
        };
        skipRubyContentIfApplicable();
        if (index >= rangeSize)
            break;
        auto& run = runs[runIndex()];

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
                if (lastTextRunIndexForTrimming && runIndex() == *lastTextRunIndexForTrimming) {
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
    computedExpansions(runs, fullRange, hangingTrailingWhitespaceLength, expansion, IgnoreRubyRange::Yes);
    // Anything to distribute?
    if (!expansion.opportunityCount)
        return { };
    return applyExpansionOnRange(runs, fullRange, expansion, spaceToDistribute);
}

InlineLayoutUnit InlineContentAligner::applyRubyAlignSpaceAround(Line::RunList& runs, WTF::Range<size_t> range, InlineLayoutUnit spaceToDistribute)
{
    if (runs.isEmpty()) {
        ASSERT_NOT_REACHED();
        return { };
    }

    if (spaceToDistribute <= 0)
        return { };

    auto rangeHasInlineContent = [&] {
        if (!range.distance())
            return false;
        for (auto index = range.begin(); index < range.end(); ++index) {
            auto& run = runs[index];
            if (!run.isInlineBox() && !run.isOpaque())
                return true;
        }
        return false;
    };
    if (!rangeHasInlineContent())
        return { };

    auto expansion = ExpansionInfo { };
    computedExpansions(runs, range, { }, expansion, IgnoreRubyRange::No);
    // Anything to distribute?
    if (!expansion.opportunityCount)
        return spaceToDistribute / 2;
    // FIXME: ruby-align: space-around only
    // As for space-between except that there exists an extra justification opportunities whose space is distributed half before and half after the ruby content.
    auto extraExpansionOpportunitySpace = spaceToDistribute / (expansion.opportunityCount + 1);
    applyExpansionOnRange(runs, range, expansion, spaceToDistribute - extraExpansionOpportunitySpace);
    return extraExpansionOpportunitySpace / 2;
}

void InlineContentAligner::applyRubyBaseAlignmentOffset(InlineDisplay::Boxes& displayBoxes, const HashMap<const Box*, InlineLayoutUnit>& alignmentOffsetList, AdjustContentOnlyInsideRubyBase adjustContentOnlyInsideRubyBase, InlineFormattingContext& inlineFormattingContext)
{
    ASSERT(!alignmentOffsetList.isEmpty());

    auto contentOffset = InlineLayoutUnit { 0.f };
    for (size_t index = 0; index < displayBoxes.size();) {
        auto& displayBox = displayBoxes[index];

        if (adjustContentOnlyInsideRubyBase == AdjustContentOnlyInsideRubyBase::No)
            shiftDisplayBox(displayBox, contentOffset, inlineFormattingContext);

        if (displayBox.layoutBox().isRubyBase()) {
            auto baseEndIndexAndAlignment = shiftRubyBaseContentByAlignmentOffset({ index, contentOffset }, displayBoxes, alignmentOffsetList, adjustContentOnlyInsideRubyBase, inlineFormattingContext);
            index = baseEndIndexAndAlignment.index;
            if (adjustContentOnlyInsideRubyBase == AdjustContentOnlyInsideRubyBase::No)
                contentOffset = baseEndIndexAndAlignment.offset;
            continue;
        }
        ++index;
    }

    if (adjustContentOnlyInsideRubyBase == AdjustContentOnlyInsideRubyBase::No)
        expandInlineBoxWithDescendants(0, displayBoxes, alignmentOffsetList, inlineFormattingContext);
}

void InlineContentAligner::applyRubyAnnotationAlignmentOffset(InlineDisplay::Boxes& displayBoxes, InlineLayoutUnit alignmentOffset, InlineFormattingContext& inlineFormattingContext)
{
    for (size_t index = 0; index < displayBoxes.size(); ++index)
        shiftDisplayBox(displayBoxes[index], alignmentOffset, inlineFormattingContext);
}

}
}

