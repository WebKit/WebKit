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
#include "RubyFormattingContext.h"

#include "InlineLine.h"

namespace WebCore {
namespace Layout {

RubyFormattingContext::RubyFormattingContext(const InlineFormattingContext& parentFormattingContext)
    : m_parentFormattingContext(parentFormattingContext)
{
}

void RubyFormattingContext::layoutInlineAxis(const InlineItemRange& rubyRange, const InlineItems& inlineItems, Line& line, InlineLayoutUnit availableWidth)
{
    UNUSED_PARAM(availableWidth);

    ASSERT(!rubyRange.isEmpty());
    // Ruby container inline item list is as follows:
    // [ruby container start][ruby base start][ruby base content][ruby base end][...][ruby container end]

    auto& rubyContainerStart = inlineItems[rubyRange.startIndex()];
    auto rubyContainerEndIndex = rubyRange.endIndex() - 1;
    ASSERT(rubyContainerStart.layoutBox().isRuby());
    line.append(rubyContainerStart, rubyContainerStart.style(), { });

    auto baseContentIndex = rubyRange.startIndex() + 1;
    while (baseContentIndex < rubyContainerEndIndex) {
        auto handleRubyColumn = [&] {
            // ruby column: represented by a single ruby base and one ruby annotation
            // from each interlinear annotation level in its ruby segment.
            auto& rubyBaseStart = inlineItems[baseContentIndex++];
            ASSERT(rubyBaseStart.layoutBox().isRubyBase());
            line.append(rubyBaseStart, rubyBaseStart.style(), { });

            baseContentIndex += layoutRubyBaseInlineAxis(line, rubyBaseStart.layoutBox(), baseContentIndex, inlineItems);

            auto& rubyBaseEnd = inlineItems[baseContentIndex++];
            ASSERT(rubyBaseEnd.layoutBox().isRubyBase());
            line.append(rubyBaseEnd, rubyBaseEnd.layoutBox().style(), { });
        };
        handleRubyColumn();
    }

    auto& rubyContainerEnd = inlineItems[rubyContainerEndIndex];
    ASSERT(rubyContainerEnd.layoutBox().isRuby());
    line.append(rubyContainerEnd, rubyContainerEnd.style(), { });
}

size_t RubyFormattingContext::layoutRubyBaseInlineAxis(Line& line, const Box& rubyBaseLayoutBox, size_t rubyBaseContentStartIndex, const InlineItems& inlineItems)
{
    // Append ruby base content (including start/end inline box) to the line and apply "ruby-align: space-around" on the ruby subrange.
    auto& formattingGeometry = parentFormattingContext().formattingGeometry();
    auto lineLogicalRight = line.contentLogicalRight();
    auto baseContentLogicalWidth = InlineLayoutUnit { };
    auto baseRunStart = line.runs().size();

    for (size_t index = rubyBaseContentStartIndex; index < inlineItems.size(); ++index) {
        auto& rubyBaseInlineItem = inlineItems[index];
        if (&rubyBaseInlineItem.layoutBox() == &rubyBaseLayoutBox) {
            auto baseRunCount = line.runs().size() - baseRunStart;
            if (baseRunCount)
                applyRubyAlign(line, { baseRunStart, baseRunStart + baseRunCount }, rubyBaseLayoutBox, baseContentLogicalWidth);
            return index - rubyBaseContentStartIndex;
        }
        auto logicalWidth = formattingGeometry.inlineItemWidth(rubyBaseInlineItem, lineLogicalRight + baseContentLogicalWidth, { });
        line.append(rubyBaseInlineItem, rubyBaseInlineItem.style(), logicalWidth);
        baseContentLogicalWidth += logicalWidth;
    }
    ASSERT_NOT_REACHED();
    return inlineItems.size() - rubyBaseContentStartIndex;
}

InlineLayoutPoint RubyFormattingContext::annotationPosition(const Box& rubyBaseLayoutBox)
{
    ASSERT(rubyBaseLayoutBox.isRubyBase());
    auto* annotationBox = rubyBaseLayoutBox.associatedRubyAnnotationBox();
    if (!annotationBox) {
        ASSERT_NOT_REACHED();
        return { };
    }
    auto annotationMarginBoxHeight = InlineLayoutUnit { parentFormattingContext().geometryForBox(*annotationBox).marginBoxHeight() };
    if (annotationBox->style().rubyPosition() == RubyPosition::Before)
        return { { }, -annotationMarginBoxHeight };
    return { { }, parentFormattingContext().geometryForBox(rubyBaseLayoutBox).marginBoxHeight() };
}

RubyFormattingContext::OverUnder RubyFormattingContext::annotationExtent(const Box& rubyBaseLayoutBox)
{
    auto* annotationBox = rubyBaseLayoutBox.associatedRubyAnnotationBox();
    if (!annotationBox)
        return { };
    auto annotationBoxLogicalHeight = InlineLayoutUnit { parentFormattingContext().geometryForBox(*annotationBox).marginBoxHeight() };
    if (annotationBox->style().rubyPosition() == RubyPosition::Before)
        return { annotationBoxLogicalHeight, { } };
    return { { }, annotationBoxLogicalHeight };
}

static inline InlineLayoutUnit halfOfAFullWidthCharacter(const Box& annotationBox)
{
    return annotationBox.style().computedFontSize() / 2.f;
}

InlineLayoutUnit RubyFormattingContext::overhangForAnnotationBefore(const Box& rubyBaseLayoutBox, size_t rubyBaseStartIndex, const InlineDisplay::Boxes& boxes)
{
    // [root inline box][ruby container][ruby base][ruby annotation]
    ASSERT(rubyBaseStartIndex >= 2);
    auto* annotationBox = rubyBaseLayoutBox.associatedRubyAnnotationBox();
    if (!annotationBox || rubyBaseStartIndex <= 2)
        return { };
    auto overhangValue = halfOfAFullWidthCharacter(*annotationBox);
    auto wouldAnnotationOverlap = [&] {
        // Check of adjacent (previous) content for overlapping.
        auto annotationMarginBoxRect = BoxGeometry::marginBoxRect(parentFormattingContext().geometryForBox(*annotationBox));
        auto overhangingRect = InlineLayoutRect { annotationMarginBoxRect.left() - overhangValue, annotationMarginBoxRect.top(), annotationMarginBoxRect.width(), annotationMarginBoxRect.height() };

        for (size_t index = rubyBaseStartIndex - 1; index > 0; --index) {
            if (auto wouldOverlap = annotationOverlapCheck(boxes[index], overhangingRect))
                return *wouldOverlap;
        }
        return true;
    };
    return wouldAnnotationOverlap() ? 0.f : overhangValue;
}

InlineLayoutUnit RubyFormattingContext::overhangForAnnotationAfter(const Box& rubyBaseLayoutBox, size_t rubyBaseContentEndIndex, const InlineDisplay::Boxes& boxes)
{
    auto* annotationBox = rubyBaseLayoutBox.associatedRubyAnnotationBox();
    if (!annotationBox || rubyBaseContentEndIndex == boxes.size() - 1)
        return { };
    auto overhangValue = halfOfAFullWidthCharacter(*annotationBox);
    auto wouldAnnotationOverlap = [&] {
        // Check of adjacent (previous) content for overlapping.
        auto annotationMarginBoxRect = BoxGeometry::marginBoxRect(parentFormattingContext().geometryForBox(*annotationBox));
        auto overhangingRect = InlineLayoutRect { annotationMarginBoxRect.left(), annotationMarginBoxRect.top(), annotationMarginBoxRect.width() + overhangValue, annotationMarginBoxRect.height() };

        for (size_t index = rubyBaseContentEndIndex + 1; index < boxes.size(); ++index) {
            if (auto wouldOverlap = annotationOverlapCheck(boxes[index], overhangingRect))
                return *wouldOverlap;
        }
        return true;
    };
    return wouldAnnotationOverlap() ? 0.f : overhangValue;
}

void RubyFormattingContext::applyRubyAlign(Line& line, WTF::Range<size_t> baseRunRange, const Box& rubyBaseLayoutBox, InlineLayoutUnit baseContentLogicalWidth)
{
    // https://drafts.csswg.org/css-ruby/#interlinear-inline
    // Within each base and annotation box, how the extra space is distributed when its content is narrower than
    // the measure of the box is specified by its ruby-align property.
    auto* annotationBox = rubyBaseLayoutBox.associatedRubyAnnotationBox();
    if (!annotationBox)
        return;
    auto annotationBoxLogicalWidth = InlineLayoutUnit { parentFormattingContext().geometryForBox(*annotationBox).marginBoxWidth() };
    if (annotationBoxLogicalWidth <= baseContentLogicalWidth)
        return;
    auto expansion = ExpansionInfo { };
    // ruby-align: space-around
    // As for space-between except that there exists an extra justification opportunities whose space is distributed half before and half after the ruby content.
    TextUtil::computedExpansions(line.runs(), baseRunRange, { }, expansion);
    if (expansion.opportunityCount) {
        auto extraSpace = annotationBoxLogicalWidth - baseContentLogicalWidth;
        auto baseContentOffset = extraSpace / (expansion.opportunityCount + 1) / 2;
        line.expandBy(baseRunRange.begin() - 1, baseContentOffset);
        extraSpace -= (2 * baseContentOffset);
        line.applyExpansionOnRange(baseRunRange, expansion, extraSpace);
        line.expandBy(baseRunRange.end() - 1, baseContentOffset);
    } else {
        auto centerOffset = (annotationBoxLogicalWidth - baseContentLogicalWidth) / 2;
        line.moveRunsBy(baseRunRange.begin(), centerOffset);
        line.expandBy(baseRunRange.begin(), centerOffset);
    }
}

std::optional<bool> RubyFormattingContext::annotationOverlapCheck(const InlineDisplay::Box& adjacentDisplayBox, const InlineLayoutRect& overhangingRect) const
{
    // We are in the middle of a line, should not see any line breaks or ellipsis boxes here.
    ASSERT(adjacentDisplayBox.isText() || adjacentDisplayBox.isAtomicInlineLevelBox() || adjacentDisplayBox.isInlineBox() || adjacentDisplayBox.isGenericInlineLevelBox() || adjacentDisplayBox.isWordSeparator());
    // Skip empty content like <span></span>
    if (adjacentDisplayBox.visualRectIgnoringBlockDirection().isEmpty())
        return { };
    if (adjacentDisplayBox.inkOverflow().intersects(overhangingRect))
        return true;
    auto& adjacentLayoutBox = adjacentDisplayBox.layoutBox();
    // Check if there might be some inline box (end decoration) overlapping as previous content.
    if (&adjacentLayoutBox.parent() == &parentFormattingContext().root())
        return false;
    if (adjacentLayoutBox.isRubyBase() && adjacentLayoutBox.associatedRubyAnnotationBox()) {
        auto annotationMarginBoxRect = InlineLayoutRect { BoxGeometry::marginBoxRect(parentFormattingContext().geometryForBox(*adjacentLayoutBox.associatedRubyAnnotationBox())) };
        if (annotationMarginBoxRect.intersects(overhangingRect))
            return true;
    }
    // FIXME: We should not let the neighboring content overlap with the base content either (currently base is sized to the annotation if shorter as we don't have the
    // inline box equivalent of ruby column).
    return { };
}

}
}

