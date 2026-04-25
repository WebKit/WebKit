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
#include "LayoutBoxInlines.h"
#include "TextUtil.h"

namespace WebCore {
namespace Layout {

static inline void shiftDisplayBox(InlineDisplay::Box& displayBox, InlineLayoutUnit offset, InlineFormattingContext& inlineFormattingContext)
{
    if (!offset)
        return;
    auto writingMode = inlineFormattingContext.root().style().writingMode();
    if (writingMode.isLineOverLeft())
        displayBox.moveVertically(-offset);
    else
        writingMode.isHorizontal() ? displayBox.moveHorizontally(offset) : displayBox.moveVertically(offset);
    if (!displayBox.isTextOrSoftLineBreak() && !displayBox.isRootInlineBox())
        inlineFormattingContext.geometryForBox(displayBox.layoutBox()).moveHorizontally(LayoutUnit { offset });
}

static inline InlineLayoutUnit alignmentOffset(auto& latyoutBox, auto& alignmentOffsetList)
{
    auto alignmentOffsetEntry = alignmentOffsetList.find(latyoutBox.ptr());
    return alignmentOffsetEntry != alignmentOffsetList.end() ? alignmentOffsetEntry->value : 0.f;
}

struct InlineBoxIndexAndExpansion {
    size_t index { 0 };
    InlineLayoutUnit expansion { 0.f };
};
static InlineBoxIndexAndExpansion expandInlineBoxToEncloseContent(size_t inlineBoxIndex, std::span<InlineDisplay::Box> displayBoxes, const HashMap<const Box*, InlineLayoutUnit>& alignmentOffsetList, InlineFormattingContext& inlineFormattingContext)
{
    if (inlineBoxIndex >= displayBoxes.size() || !displayBoxes[inlineBoxIndex].isInlineBox()) {
        ASSERT_NOT_REACHED();
        return { inlineBoxIndex, { } };
    }

    auto& inlineBoxDisplayBox = displayBoxes[inlineBoxIndex];
    CheckedRef inlineBox = inlineBoxDisplayBox.layoutBox();
    auto descendantExpansion = InlineLayoutUnit { 0.f };
    size_t index = inlineBoxIndex + 1;
    while (index < displayBoxes.size() && &displayBoxes[index].layoutBox().parent() == inlineBox.ptr()) {
        if (displayBoxes[index].isInlineBox()) {
            auto indexAndExpansion = expandInlineBoxToEncloseContent(index, displayBoxes, alignmentOffsetList, inlineFormattingContext);
            index = indexAndExpansion.index;
            descendantExpansion += indexAndExpansion.expansion;
            continue;
        }
        ++index;
    }
    auto totalExpansion = 2 * alignmentOffset(inlineBox, alignmentOffsetList) + descendantExpansion;
    // Root inline box always has the correct size.
    if (!inlineBoxIndex || !totalExpansion)
        return { index, totalExpansion };

    // This could either be an ruby inline box (<ruby> or base) or an inline box enclosing <ruby> e.g. <span><ruby>.
    ASSERT(!inlineBoxDisplayBox.isRubyBase() || (inlineBoxDisplayBox.style().rubyAlign() == RubyAlign::Center || inlineBoxDisplayBox.style().rubyAlign() == RubyAlign::SpaceAround));
    auto expand = [&] {
        auto writingMode = inlineFormattingContext.root().writingMode();
        writingMode.isHorizontal() ? inlineBoxDisplayBox.expandHorizontally(totalExpansion) : inlineBoxDisplayBox.expandVertically(totalExpansion);
        auto& boxGeometry = inlineFormattingContext.geometryForBox(inlineBoxDisplayBox.layoutBox());
        if (writingMode.isLineOverLeft()) {
            inlineBoxDisplayBox.setTop(inlineBoxDisplayBox.top() - totalExpansion);
            boxGeometry.setLeft(BoxGeometry::borderBoxLeft(boxGeometry) - LayoutUnit { totalExpansion });
        }
        boxGeometry.setContentBoxWidth(boxGeometry.contentBoxWidth() + LayoutUnit { totalExpansion });
    };
    expand();
    return { index, totalExpansion };
}

struct BaseIndexAndOffset {
    size_t index { 0 };
    InlineLayoutUnit offset { 0.f };
};
static BaseIndexAndOffset shiftRubyBaseContentByAlignmentOffset(BaseIndexAndOffset baseIndexAndContentOffset, std::span<InlineDisplay::Box> displayBoxes, const HashMap<const Box*, InlineLayoutUnit>& alignmentOffsetList, InlineFormattingContext& inlineFormattingContext)
{
    auto baseIndex = baseIndexAndContentOffset.index;
    if (baseIndex >= displayBoxes.size() || !displayBoxes[baseIndex].isRubyBase()) {
        ASSERT_NOT_REACHED();
        return { baseIndexAndContentOffset.index, { } };
    }

    // Shift base content within the base (no resize) as part of the alignment process.
    CheckedRef rootBox = inlineFormattingContext.root();
    CheckedRef rubyBaseBox = displayBoxes[baseIndex].layoutBox();
    auto baseOffset = baseIndexAndContentOffset.offset;
    auto baseContentOffset = alignmentOffset(rubyBaseBox, alignmentOffsetList);
    size_t baseContentIndex = baseIndex + 1;

    while (baseContentIndex < displayBoxes.size()) {
        auto& displayBox = displayBoxes[baseContentIndex];
        CheckedRef layoutBox = displayBox.layoutBox();
        auto isInsideCurrentRubyBase = [&] {
            // Ruby content tends to produce flat structures.
            for (auto* ancestor = &layoutBox->parent(); ancestor; ancestor = &ancestor->parent()) {
                if (ancestor == rubyBaseBox.ptr())
                    return true;
                if (ancestor->isRubyBase() || ancestor->isRuby() || ancestor == rootBox.ptr())
                    return false;
            }
            return false;
        };
        if (!isInsideCurrentRubyBase())
            break;
        if (!layoutBox->isRubyAnnotationBox())
            shiftDisplayBox(displayBox, baseOffset + baseContentOffset, inlineFormattingContext);
        if (layoutBox->isRubyBase()) {
            auto baseContentEndIndexAndOffset = shiftRubyBaseContentByAlignmentOffset({ baseContentIndex, baseOffset + baseContentOffset }, displayBoxes, alignmentOffsetList, inlineFormattingContext);
            baseContentIndex = baseContentEndIndexAndOffset.index;
            baseOffset += baseContentEndIndexAndOffset.offset;
            continue;
        }
        ++baseContentIndex;
    }
    auto accumulatedOffset = 2 * baseContentOffset;
    accumulatedOffset += baseOffset;
    return { baseContentIndex, accumulatedOffset };
}

enum class IgnoreRubyRange : bool { No, Yes };
static void computedExpansions(std::span<Line::Run> runs, size_t hangingTrailingWhitespaceLength, ExpansionInfo& expansionInfo, IgnoreRubyRange ignoreRuby)
{
    // Collect and distribute the expansion opportunities.
    expansionInfo.opportunityCount = 0;
    expansionInfo.opportunityList.resizeToFit(runs.size());
    expansionInfo.behaviorList.resizeToFit(runs.size());
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
    for (size_t index = 0; index < runs.size(); ++index) {
        auto skipRubyContentIfApplicable = [&] {
            auto& rubyBox = runs[index].layoutBox();
            if (ignoreRuby == IgnoreRubyRange::No || !rubyBox.isRuby())
                return;
            runIsAfterExpansion = false;
            for (; index < runs.size(); ++index) {
                expansionInfo.behaviorList[index] = ExpansionBehavior::defaultBehavior();
                expansionInfo.opportunityList[index] = 0;
                auto& run = runs[index];
                if (run.isInlineBoxEnd() && &run.layoutBox() == &rubyBox) {
                    ++index;
                    return;
                }
            }
        };
        skipRubyContentIfApplicable();
        if (index >= runs.size())
            break;
        auto& run = runs[index];

        auto expansionBehavior = ExpansionBehavior::defaultBehavior();
        size_t expansionOpportunitiesInRun = 0;

        if (run.isText()) {
            if (run.hasTextCombine())
                expansionBehavior = ExpansionBehavior::forbidAll();
            else {
                expansionBehavior.left = runIsAfterExpansion ? ExpansionBehavior::Behavior::Forbid : ExpansionBehavior::Behavior::Allow;
                expansionBehavior.right = ExpansionBehavior::Behavior::Allow;
                auto& textContent = run.textContent();
                auto length = textContent.length;
                if (lastTextRunIndexForTrimming && index == *lastTextRunIndexForTrimming) {
                    // Trailing hanging whitespace sequence is ignored when computing the expansion opportunities.
                    length -= hangingTrailingWhitespaceLength;
                }
                std::tie(expansionOpportunitiesInRun, runIsAfterExpansion) = FontCascade::expansionOpportunityCount(StringView(downcast<InlineTextBox>(run.layoutBox()).content()).substring(textContent.start, length), run.inlineDirection(), expansionBehavior);
            }
        } else if (run.isAtomicInlineBox())
            runIsAfterExpansion = false;

        expansionInfo.behaviorList[index] = expansionBehavior;
        expansionInfo.opportunityList[index] = expansionOpportunitiesInRun;
        expansionInfo.opportunityCount += expansionOpportunitiesInRun;

        if (run.isText() || run.isAtomicInlineBox())
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

InlineLayoutUnit InlineContentAligner::applyExpansionOnRange(std::span<Line::Run> runs, const ExpansionInfo& expansion, InlineLayoutUnit spaceToDistribute)
{
    ASSERT(spaceToDistribute > 0);
    ASSERT(expansion.opportunityCount);
    // Distribute the extra space.
    auto expansionToDistribute = spaceToDistribute / expansion.opportunityCount;
    auto accumulatedExpansion = InlineLayoutUnit { };
    for (size_t index = 0; index < runs.size(); ++index) {
        auto& run = runs[index];
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
    computedExpansions(runs.mutableSpan(), hangingTrailingWhitespaceLength, expansion, IgnoreRubyRange::Yes);
    // Anything to distribute?
    if (!expansion.opportunityCount)
        return { };
    return applyExpansionOnRange(runs.mutableSpan(), expansion, spaceToDistribute);
}

InlineLayoutUnit InlineContentAligner::applyRubyAlign(RubyAlign rubyAlign, std::span<Line::Run> runs, InlineLayoutUnit spaceToDistribute)
{
    if (runs.empty()) {
        ASSERT_NOT_REACHED();
        return { };
    }

    if (spaceToDistribute <= 0)
        return { };

    auto rangeHasInlineContent = [&] {
        for (auto& run : runs) {
            if (!run.isInlineBox() && !run.isOutOfFlow())
                return true;
        }
        return false;
    };
    if (!rangeHasInlineContent())
        return { };

    switch (rubyAlign) {
    case RubyAlign::Start:
        return { };
    case RubyAlign::Center:
        return spaceToDistribute / 2;
    case RubyAlign::SpaceBetween: {
        // The ruby content expands as defined for normal text justification (as defined by text-justify), except that if there are no
        // justification opportunities the content is centered.
        auto expansion = ExpansionInfo { };
        computedExpansions(runs, { }, expansion, IgnoreRubyRange::No);
        // Anything to distribute?
        if (!expansion.opportunityCount)
            return spaceToDistribute / 2;
        applyExpansionOnRange(runs, expansion, spaceToDistribute);
        return { };
    }
    case RubyAlign::SpaceAround: {
        auto expansion = ExpansionInfo { };
        computedExpansions(runs, { }, expansion, IgnoreRubyRange::No);
        // Anything to distribute?
        if (!expansion.opportunityCount)
            return spaceToDistribute / 2;
        // As for space-between except that there exists an extra justification opportunities whose space is distributed half before and half after the ruby content.
        auto extraExpansionOpportunitySpace = spaceToDistribute / (expansion.opportunityCount + 1);
        applyExpansionOnRange(runs, expansion, spaceToDistribute - extraExpansionOpportunitySpace);
        return extraExpansionOpportunitySpace / 2;
    }
    default:
        return { };
    }
}

void InlineContentAligner::adjustRubyBaseContentWithAlignmentOffset(std::span<InlineDisplay::Box> displayBoxes, const HashMap<const Box*, InlineLayoutUnit>& alignmentOffsetList, InlineFormattingContext& inlineFormattingContext)
{
    ASSERT(!alignmentOffsetList.isEmpty());

    auto baseIndexAndOffset = BaseIndexAndOffset { };
    while (baseIndexAndOffset.index < displayBoxes.size()) {
        auto& displayBox = displayBoxes[baseIndexAndOffset.index];
        shiftDisplayBox(displayBox, baseIndexAndOffset.offset, inlineFormattingContext);

        if (!displayBox.isRubyBase()) {
            ++baseIndexAndOffset.index;
            continue;
        }
        baseIndexAndOffset = shiftRubyBaseContentByAlignmentOffset(baseIndexAndOffset, displayBoxes, alignmentOffsetList, inlineFormattingContext);
    }

    expandInlineBoxToEncloseContent(0, displayBoxes, alignmentOffsetList, inlineFormattingContext);
}

void InlineContentAligner::adjustAnnotationContentWithAlignmentOffset(std::span<InlineDisplay::Box> displayBoxes, InlineLayoutUnit alignmentOffset, InlineFormattingContext& inlineFormattingContext)
{
    for (auto& displayBox : displayBoxes)
        shiftDisplayBox(displayBox, alignmentOffset, inlineFormattingContext);
}

}
}

