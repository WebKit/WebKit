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

#include "InlineFormattingContext.h"
#include "InlineLine.h"
#include "RenderStyleInlines.h"

namespace WebCore {
namespace Layout {

static inline size_t nextWrapOpportunityWithinRubyContainer(size_t startIndex, const InlineItemRange& rubyRange, const InlineItemList& inlineItemList)
{
    // We only allow wrap opportunity between ruby bases.
    for (size_t index = startIndex; index < rubyRange.endIndex(); ++index) {
        auto& rubyItem = inlineItemList[index];
        if (rubyItem.isInlineBoxEnd() && rubyItem.layoutBox().isRubyBase()) {
            // We are at the end of a ruby base, need to check if we are between bases.
            if (index + 1 == rubyRange.endIndex()) {
                ASSERT_NOT_REACHED();
                continue;
            }
            auto& nextRubyItem = inlineItemList[index + 1];
            if (nextRubyItem.isInlineBoxStart()) {
                ASSERT(nextRubyItem.layoutBox().isRubyBase());
                return index + 1;
            }
        }
    }
    return rubyRange.endIndex();
}

static bool isInterlinearAnnotation(const Box* annotationBox)
{
    return annotationBox && annotationBox->style().rubyPosition() != RubyPosition::InterCharacter;
}

RubyFormattingContext::RubyFormattingContext(const InlineFormattingContext& parentFormattingContext)
    : m_parentFormattingContext(parentFormattingContext)
{
}

RubyFormattingContext::InlineLayoutResult RubyFormattingContext::layoutInlineAxis(const InlineItemRange& rubyRange, const InlineItemList& inlineItemList, Line& line, InlineLayoutUnit availableWidth)
{
    ASSERT(!rubyRange.isEmpty());
    // Ruby container inline item list is as follows:
    // [ruby container start][ruby base start][ruby base content][ruby base end][...][ruby container end]
    auto currentIndex = rubyRange.startIndex();
    while (currentIndex < rubyRange.endIndex()) {
        auto candidateContentEnd = nextWrapOpportunityWithinRubyContainer(currentIndex, rubyRange, inlineItemList);
        auto contentLogicalWidth = logicaWidthForRubyRange({ currentIndex, candidateContentEnd }, inlineItemList, line.contentLogicalRight());
        auto shouldPlaceRubyRange = contentLogicalWidth <= availableWidth || !line.hasContent();
        if (!shouldPlaceRubyRange)
            return { InlineContentBreaker::IsEndOfLine::Yes, currentIndex - rubyRange.startIndex() };
        placeRubyContent({ currentIndex, candidateContentEnd }, inlineItemList, line);
        availableWidth -= contentLogicalWidth;
        currentIndex = candidateContentEnd;
    }
    return { availableWidth >= 0 ? InlineContentBreaker::IsEndOfLine::No : InlineContentBreaker::IsEndOfLine::Yes, currentIndex - rubyRange.startIndex() };
}

void RubyFormattingContext::placeRubyContent(WTF::Range<size_t> candidateRange, const InlineItemList& inlineItemList, Line& line)
{
    ASSERT(candidateRange.end() <= inlineItemList.size());
    ASSERT(inlineItemList[candidateRange.begin()].layoutBox().isRuby() || inlineItemList[candidateRange.begin()].layoutBox().isRubyBase());
    auto& formattingUtils = parentFormattingContext().formattingUtils();

    auto index = candidateRange.begin();
    auto logicalRightSpacingForBase = InlineLayoutUnit { };
    while (index < candidateRange.end()) {
        auto appendInlineLevelItem = [&](auto& inlineBoxItem, InlineLayoutUnit extraSpacingEnd = { }) {
            ASSERT(inlineBoxItem.layoutBox().isRuby() || inlineBoxItem.layoutBox().isRubyBase() || inlineBoxItem.layoutBox().isRubyAnnotationBox());

            auto logicalWidth = formattingUtils.inlineItemWidth(inlineBoxItem, line.contentLogicalRight(), { });
            line.append(inlineBoxItem, inlineBoxItem.style(), logicalWidth + extraSpacingEnd);
            ++index;
        };

        auto& rubyItem = inlineItemList[index];
        auto& rubyLayoutBox = rubyItem.layoutBox();
        ASSERT(rubyItem.isInlineBoxStart() || rubyItem.isInlineBoxEnd() || rubyLayoutBox.isRubyAnnotationBox());

        if (rubyLayoutBox.isRuby()) {
            appendInlineLevelItem(rubyItem);
            ASSERT(rubyItem.isInlineBoxStart() || (rubyItem.isInlineBoxEnd() && index == candidateRange.end()));
            continue;
        }
        if (rubyLayoutBox.isRubyBase()) {
            // ruby column: represented by a single ruby base and one ruby annotation
            // from each interlinear annotation level in its ruby segment.
            if (rubyItem.isInlineBoxStart()) {
                appendInlineLevelItem(rubyItem);
                auto baseLayoutResult = layoutRubyBaseInlineAxis(line, rubyLayoutBox, index, inlineItemList);
                index += baseLayoutResult.committedCount;
                logicalRightSpacingForBase = baseLayoutResult.logicalRightSpacing;
                continue;
            }
            ASSERT(rubyItem.isInlineBoxEnd());
            appendInlineLevelItem(rubyItem, logicalRightSpacingForBase);
            continue;
        }
        if (rubyLayoutBox.isRubyAnnotationBox()) {
            ASSERT(!rubyLayoutBox.isInterlinearRubyAnnotationBox());
            appendInlineLevelItem(rubyItem);
            ++index;
            continue;
        }
        ASSERT_NOT_REACHED();
    }
}

RubyFormattingContext::BaseLayoutResult RubyFormattingContext::layoutRubyBaseInlineAxis(Line& line, const Box& rubyBaseLayoutBox, size_t rubyBaseContentStartIndex, const InlineItemList& inlineItemList)
{
    // Append ruby base content (including start/end inline box) to the line and apply "ruby-align: space-around" on the ruby subrange.
    auto& formattingUtils = parentFormattingContext().formattingUtils();
    auto lineLogicalRight = line.contentLogicalRight();
    auto baseContentLogicalWidth = InlineLayoutUnit { };
    auto baseRunStart = line.runs().size();

    for (size_t index = rubyBaseContentStartIndex; index < inlineItemList.size(); ++index) {
        auto& rubyBaseInlineItem = inlineItemList[index];
        if (&rubyBaseInlineItem.layoutBox() == &rubyBaseLayoutBox) {
            auto baseRunCount = line.runs().size() - baseRunStart;
            auto logicalRightSpacing = InlineLayoutUnit { };
            if (baseRunCount)
                logicalRightSpacing = applyRubyAlign(line, { baseRunStart, baseRunStart + baseRunCount }, rubyBaseLayoutBox, baseContentLogicalWidth);
            return { index - rubyBaseContentStartIndex, logicalRightSpacing };
        }
        auto logicalWidth = formattingUtils.inlineItemWidth(rubyBaseInlineItem, lineLogicalRight + baseContentLogicalWidth, { });
        line.append(rubyBaseInlineItem, rubyBaseInlineItem.style(), logicalWidth);
        baseContentLogicalWidth += logicalWidth;
    }
    ASSERT_NOT_REACHED();
    return { inlineItemList.size() - rubyBaseContentStartIndex, { } };
}

InlineLayoutPoint RubyFormattingContext::placeAnnotationBox(const Box& rubyBaseLayoutBox)
{
    ASSERT(rubyBaseLayoutBox.isRubyBase());
    auto* annotationBox = rubyBaseLayoutBox.associatedRubyAnnotationBox();
    if (!annotationBox) {
        ASSERT_NOT_REACHED();
        return { };
    }
    auto& inlineFormattingContext = parentFormattingContext();
    auto& rubyBaseGeometry = inlineFormattingContext.geometryForBox(rubyBaseLayoutBox);
    auto& annotationBoxGeometry = inlineFormattingContext.geometryForBox(*annotationBox);

    if (isInterlinearAnnotation(annotationBox)) {
        auto topLeft = BoxGeometry::marginBoxRect(rubyBaseGeometry).topLeft();
        // Move it over/under the base and make it border box position.
        auto borderBoxTop = annotationBox->style().rubyPosition() == RubyPosition::Before ? -annotationBoxGeometry.marginBoxHeight() : rubyBaseGeometry.marginBoxHeight();
        borderBoxTop += annotationBoxGeometry.marginBefore();
        topLeft.move(LayoutSize { annotationBoxGeometry.marginStart(), borderBoxTop });
        return topLeft;
    }
    // Inter-character annotation box is stretched to the size of the base content box and vertically centered.
    auto rubyBaseRight = BoxGeometry::marginBoxRect(rubyBaseGeometry).right();
    auto borderBoxRight = rubyBaseRight + annotationBoxGeometry.marginStart();
    return { borderBoxRight, ((rubyBaseGeometry.contentBoxHeight() - annotationBoxGeometry.contentBoxHeight()) / 2) - annotationBoxGeometry.borderBefore() };
}

InlineLayoutSize RubyFormattingContext::sizeAnnotationBox(const Box& rubyBaseLayoutBox)
{
    // FIXME: This is where we should take advantage of the ruby-column setup.
    ASSERT(rubyBaseLayoutBox.isRubyBase());
    auto* annotationBox = rubyBaseLayoutBox.associatedRubyAnnotationBox();
    if (!annotationBox) {
        ASSERT_NOT_REACHED();
        return { };
    }
    auto& inlineFormattingContext = parentFormattingContext();
    auto& rubyBaseGeometry = inlineFormattingContext.geometryForBox(rubyBaseLayoutBox);
    auto& annotationBoxGeometry = inlineFormattingContext.geometryForBox(*annotationBox);
    if (isInterlinearAnnotation(annotationBox))
        return { std::max(rubyBaseGeometry.marginBoxWidth(), annotationBoxGeometry.marginBoxWidth()) - annotationBoxGeometry.horizontalMarginBorderAndPadding(), annotationBoxGeometry.contentBoxHeight() };
    return { annotationBoxGeometry.contentBoxWidth(), std::max(rubyBaseGeometry.marginBoxHeight(), annotationBoxGeometry.marginBoxHeight()) - annotationBoxGeometry.verticalMarginBorderAndPadding() };
}

RubyFormattingContext::OverUnder RubyFormattingContext::annotationContributionToLayoutBounds(const Box& rubyBaseLayoutBox)
{
    auto* annotationBox = rubyBaseLayoutBox.associatedRubyAnnotationBox();
    if (!isInterlinearAnnotation(annotationBox))
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
    if (!isInterlinearAnnotation(annotationBox) || rubyBaseStartIndex <= 2)
        return { };
    if (rubyBaseStartIndex + 1 >= boxes.size()) {
        // We have to have some base content.
        ASSERT_NOT_REACHED();
        return { };
    }
    // FIXME: Usually the first content box is visually the leftmost, but we should really look for content shifted to the left through negative margins on inline boxes.
    auto gapBetweenBaseAndContent = std::max(0.f, boxes[rubyBaseStartIndex + 2].visualRectIgnoringBlockDirection().x() - boxes[rubyBaseStartIndex].visualRectIgnoringBlockDirection().x());
    auto overhangValue = std::min(halfOfAFullWidthCharacter(*annotationBox), gapBetweenBaseAndContent);
    auto wouldAnnotationOrBaseOverlapLineContent = [&] {
        // Check of adjacent (previous) content for overlapping.
        auto annotationMarginBoxRect = BoxGeometry::marginBoxRect(parentFormattingContext().geometryForBox(*annotationBox));
        auto overhangingAnnotationRect = InlineLayoutRect { annotationMarginBoxRect.left() - overhangValue, annotationMarginBoxRect.top(), annotationMarginBoxRect.width(), annotationMarginBoxRect.height() };
        auto baseContentBoxRect = boxes[rubyBaseStartIndex + 1].inkOverflow();
        // This is how much the base content would be closer to content outside of base.
        baseContentBoxRect.move(LayoutSize { -overhangValue, 0.f });

        for (size_t index = rubyBaseStartIndex - 1; index > 0; --index) {
            if (boxes[index].layoutBox().isRuby()) {
                // We can certainly overlap with our own ruby container.
                continue;
            }
            auto wouldAnnotationOverlap = annotationOverlapCheck(boxes[index], overhangingAnnotationRect);
            auto wouldBaseContentOverlap = annotationOverlapCheck(boxes[index], baseContentBoxRect);
            if (!wouldAnnotationOverlap && !wouldBaseContentOverlap) {
                // Can't decide it yet.
                continue;
            }
            return *wouldAnnotationOverlap || *wouldBaseContentOverlap;
        }
        return true;
    };
    return wouldAnnotationOrBaseOverlapLineContent() ? 0.f : overhangValue;
}

InlineLayoutUnit RubyFormattingContext::overhangForAnnotationAfter(const Box& rubyBaseLayoutBox, size_t rubyBaseStartIndex, size_t rubyBaseContentEndIndex, const InlineDisplay::Boxes& boxes)
{
    auto* annotationBox = rubyBaseLayoutBox.associatedRubyAnnotationBox();
    if (!isInterlinearAnnotation(annotationBox) || rubyBaseContentEndIndex == boxes.size() -1)
        return { };
    // FIXME: Usually the last content box is visually the rightmost, but negative margin may override it.
    // FIXME: Currently justified content always expands producing 0 value for gapBetweenBaseEndAndContent.
    auto gapBetweenBaseEndAndContent = std::max(0.f, boxes[rubyBaseStartIndex].visualRectIgnoringBlockDirection().maxX() - boxes[rubyBaseContentEndIndex].visualRectIgnoringBlockDirection().maxX());
    auto overhangValue = std::min(halfOfAFullWidthCharacter(*annotationBox), gapBetweenBaseEndAndContent);
    auto wouldAnnotationOrBaseOverlapLineContent = [&] {
        // Check of adjacent (next) content for overlapping.
        auto annotationMarginBoxRect = BoxGeometry::marginBoxRect(parentFormattingContext().geometryForBox(*annotationBox));
        auto overhangingAnnotationRect = InlineLayoutRect { annotationMarginBoxRect.left(), annotationMarginBoxRect.top(), annotationMarginBoxRect.width() + overhangValue, annotationMarginBoxRect.height() };
        auto baseContentBoxRect = boxes[rubyBaseContentEndIndex].inkOverflow();
        // This is how much the base content would be closer to content outside of base.
        baseContentBoxRect.move(LayoutSize { overhangValue, 0.f });

        for (size_t index = rubyBaseContentEndIndex + 1; index < boxes.size(); ++index) {
            auto wouldAnnotationOverlap = annotationOverlapCheck(boxes[index], overhangingAnnotationRect);
            auto wouldBaseContentOverlap = annotationOverlapCheck(boxes[index], baseContentBoxRect);
            if (!wouldAnnotationOverlap && !wouldBaseContentOverlap) {
                // Can't decide it yet.
                continue;
            }
            return *wouldAnnotationOverlap || *wouldBaseContentOverlap;
        }
        return true;
    };
    return wouldAnnotationOrBaseOverlapLineContent() ? 0.f : overhangValue;
}

std::optional<size_t> RubyFormattingContext::nextWrapOpportunity(size_t inlineItemIndex, std::optional<size_t> previousInlineItemIndex, const InlineItemRange& layoutRange, const InlineItemList& inlineItemList)
{
    auto& inlineItem = inlineItemList[inlineItemIndex];
    ASSERT(inlineItem.isInlineBoxStart() || inlineItem.isInlineBoxEnd());

    auto& layoutBox = inlineItem.layoutBox();
    auto startsWithRubyInlineBox = layoutBox.isRuby() || layoutBox.isRubyBase();
    if (!startsWithRubyInlineBox) {
        // This is not ruby content.
        return { };
    }
    if (previousInlineItemIndex && startsWithRubyInlineBox) {
        // There's always a soft wrap opportunitiy before <ruby>/ruby base.
        return inlineItemIndex;
    }
    // Skip to the end of the ruby container.
    for (; inlineItemIndex < layoutRange.endIndex() && (!inlineItemList[inlineItemIndex].isInlineBoxEnd() || !inlineItemList[inlineItemIndex].layoutBox().isRuby()); ++inlineItemIndex) { }
    ASSERT(inlineItemIndex < layoutRange.endIndex());
    return inlineItemIndex + 1;
}

InlineLayoutUnit RubyFormattingContext::applyRubyAlign(Line& line, WTF::Range<size_t> baseRunRange, const Box& rubyBaseLayoutBox, InlineLayoutUnit baseContentLogicalWidth)
{
    // https://drafts.csswg.org/css-ruby/#interlinear-inline
    // Within each base and annotation box, how the extra space is distributed when its content is narrower than
    // the measure of the box is specified by its ruby-align property.
    auto* annotationBox = rubyBaseLayoutBox.associatedRubyAnnotationBox();
    if (!annotationBox)
        return { };
    auto annotationBoxLogicalWidth = InlineLayoutUnit { parentFormattingContext().geometryForBox(*annotationBox).marginBoxWidth() };
    if (annotationBoxLogicalWidth <= baseContentLogicalWidth)
        return { };
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
        return baseContentOffset;
    }
    auto centerOffset = (annotationBoxLogicalWidth - baseContentLogicalWidth) / 2;
    // Move the first and exand the last run (most cases there's only one run here though.)
    line.moveRunsBy(baseRunRange.begin(), centerOffset);
    return centerOffset;
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
    if (adjacentLayoutBox.isRuby()) {
        // Need to look inside the ruby container to find overlapping content.
        return { };
    }
    // Check if there might be some inline box (end decoration) overlapping as previous content.
    if (&adjacentLayoutBox.parent() == &parentFormattingContext().root())
        return false;
    if (adjacentLayoutBox.isRubyBase() && adjacentLayoutBox.associatedRubyAnnotationBox()) {
        auto annotationMarginBoxRect = InlineLayoutRect { BoxGeometry::marginBoxRect(parentFormattingContext().geometryForBox(*adjacentLayoutBox.associatedRubyAnnotationBox())) };
        if (annotationMarginBoxRect.intersects(overhangingRect))
            return true;
    }
    return { };
}

InlineLayoutUnit RubyFormattingContext::logicaWidthForRubyRange(WTF::Range<size_t> candidateRange, const InlineItemList& inlineItemList, InlineLayoutUnit lineContentLogicalRight) const
{
    ASSERT(candidateRange.end() <= inlineItemList.size());
    ASSERT(inlineItemList[candidateRange.begin()].layoutBox().isRuby() || inlineItemList[candidateRange.begin()].layoutBox().isRubyBase());

    auto& formattingUtils = parentFormattingContext().formattingUtils();
    auto candidateContentLogicalWidth = InlineLayoutUnit { };
    auto index = candidateRange.begin();

    while (index < candidateRange.end()) {
        auto& rubyItem = inlineItemList[index];
        auto& rubyLayoutBox = rubyItem.layoutBox();

        if (rubyLayoutBox.isRuby()) {
            ASSERT(rubyItem.isInlineBoxStart() || rubyItem.isInlineBoxEnd());
            candidateContentLogicalWidth += formattingUtils.inlineItemWidth(rubyItem, lineContentLogicalRight + candidateContentLogicalWidth, { });
            ++index;
            continue;
        }

        if (rubyLayoutBox.isRubyBase()) {
            ASSERT(rubyItem.isInlineBoxStart());

            auto interlinearAnnotationMarginBoxWidth = [&]() -> InlineLayoutUnit {
                if (auto* annotationBox = rubyLayoutBox.associatedRubyAnnotationBox(); isInterlinearAnnotation(annotationBox))
                    return InlineLayoutUnit { parentFormattingContext().geometryForBox(*annotationBox).marginBoxWidth() };
                return { };
            };

            auto baseLogicalWidth = [&] {
                // Base content needs special handling with taking annotation box into account.
                auto logicalWidth = InlineLayoutUnit { };
                for (; index < candidateRange.end(); ++index) {
                    auto& baseInlineItem = inlineItemList[index];
                    logicalWidth += formattingUtils.inlineItemWidth(baseInlineItem, lineContentLogicalRight + logicalWidth, { });
                    if (&baseInlineItem.layoutBox() == &rubyLayoutBox && baseInlineItem.isInlineBoxEnd()) {
                        // End of base.
                        ++index;
                        return logicalWidth;
                    }
                }
                ASSERT_NOT_REACHED();
                return InlineLayoutUnit { };
            };
            candidateContentLogicalWidth += std::max(baseLogicalWidth(), interlinearAnnotationMarginBoxWidth());
            continue;
        }

        if (rubyLayoutBox.isRubyAnnotationBox()) {
            ASSERT(!rubyLayoutBox.isInterlinearRubyAnnotationBox());
            candidateContentLogicalWidth += InlineLayoutUnit { parentFormattingContext().geometryForBox(rubyLayoutBox).marginBoxWidth() };
            ++index;
            continue;
        }

        ASSERT_NOT_REACHED();
    }
    return candidateContentLogicalWidth;
}

}
}

