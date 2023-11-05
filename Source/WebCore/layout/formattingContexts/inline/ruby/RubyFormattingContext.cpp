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
    return annotationBox && annotationBox->isInterlinearRubyAnnotationBox();
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
        ASSERT_NOT_REACHED();
        ++index;
    }
}

RubyFormattingContext::BaseLayoutResult RubyFormattingContext::layoutRubyBaseInlineAxis(Line& line, const Box& rubyBaseLayoutBox, size_t rubyBaseContentStartIndex, const InlineItemList& inlineItemList)
{
    // Append ruby base content (including start/end inline box) to the line and apply "ruby-align: space-around" on the ruby subrange.
    auto& formattingUtils = parentFormattingContext().formattingUtils();
    auto baseRunStart = line.runs().size();
    auto baseContentLogicalLeft = line.contentLogicalRight();

    auto index = rubyBaseContentStartIndex;
    while (index < inlineItemList.size() && &inlineItemList[index].layoutBox() != &rubyBaseLayoutBox) {
        auto& rubyBaseContentItem = inlineItemList[index++];
        line.append(rubyBaseContentItem, rubyBaseContentItem.style(), formattingUtils.inlineItemWidth(rubyBaseContentItem, line.contentLogicalRight(), { }));
    }
    ASSERT(index < inlineItemList.size());
    auto logicalRightSpacing = InlineLayoutUnit { };
    auto baseRunCount = line.runs().size() - baseRunStart;
    if (baseRunCount) {
        auto baseContentLogicalWidth = line.contentLogicalRight() - baseContentLogicalLeft;
        logicalRightSpacing = applyRubyAlign(line, { baseRunStart, baseRunStart + baseRunCount }, rubyBaseLayoutBox, baseContentLogicalWidth);
    }
    return { index - rubyBaseContentStartIndex, logicalRightSpacing };
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
        // Move it over/under the base and make it border box position.
        auto leftOffset = annotationBoxGeometry.marginStart();
        auto topOffset = annotationBox->style().rubyPosition() == RubyPosition::Before ? -annotationBoxGeometry.marginBoxHeight() : rubyBaseGeometry.marginBoxHeight();
        switch (writingModeToBlockFlowDirection(rubyBaseLayoutBox.style().writingMode())) {
        case BlockFlowDirection::TopToBottom:
            break;
        case BlockFlowDirection::BottomToTop:
            topOffset = annotationBox->style().rubyPosition() == RubyPosition::Before ?  rubyBaseGeometry.marginBoxHeight() : -annotationBoxGeometry.marginBoxHeight();
            break;
        case BlockFlowDirection::RightToLeft:
            topOffset = leftOffset;
            leftOffset = annotationBox->style().rubyPosition() == RubyPosition::Before ? -annotationBoxGeometry.marginBoxHeight() : rubyBaseGeometry.marginBoxWidth();
            break;
        case BlockFlowDirection::LeftToRight:
            topOffset = leftOffset;
            leftOffset = annotationBox->style().rubyPosition() == RubyPosition::Before ? rubyBaseGeometry.marginBoxWidth() : -annotationBoxGeometry.marginBoxHeight();
            break;
        default:
            ASSERT_NOT_REACHED();
            break;
        }
        auto topLeft = BoxGeometry::marginBoxRect(rubyBaseGeometry).topLeft();
        topLeft.move(LayoutSize { leftOffset, topOffset });
        return topLeft;
    }
    // Inter-character annotation box is stretched to the size of the base content box and vertically centered.
    auto isHorizontalWritingMode = rubyBaseLayoutBox.style().isHorizontalWritingMode();
    auto annotationVisualContentBoxHeight = isHorizontalWritingMode ? annotationBoxGeometry.contentBoxHeight() : annotationBoxGeometry.contentBoxWidth();
    auto annotationBorderTop = isHorizontalWritingMode ? annotationBoxGeometry.borderBefore() : annotationBoxGeometry.borderStart();
    auto rubyBaseMarginBox = BoxGeometry::marginBoxRect(rubyBaseGeometry);
    auto borderBoxRight = rubyBaseMarginBox.right() + annotationBoxGeometry.marginStart();
    return { borderBoxRight, rubyBaseMarginBox.top() + ((rubyBaseGeometry.marginBoxHeight() - annotationVisualContentBoxHeight) / 2) - annotationBorderTop };
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
    auto isHorizontalWritingMode = rubyBaseLayoutBox.style().isHorizontalWritingMode();
    auto& inlineFormattingContext = parentFormattingContext();
    auto& visualRubyBaseGeometry = inlineFormattingContext.geometryForBox(rubyBaseLayoutBox);
    auto& logicalAnnotationBoxGeometry = inlineFormattingContext.geometryForBox(*annotationBox);

    if (isInterlinearAnnotation(annotationBox)) {
        if (isHorizontalWritingMode)
            return { std::max(visualRubyBaseGeometry.marginBoxWidth(), logicalAnnotationBoxGeometry.marginBoxWidth()) - logicalAnnotationBoxGeometry.horizontalMarginBorderAndPadding(), logicalAnnotationBoxGeometry.contentBoxHeight() };
        return { logicalAnnotationBoxGeometry.contentBoxHeight(), std::max(logicalAnnotationBoxGeometry.marginBoxWidth(), visualRubyBaseGeometry.marginBoxHeight()) - logicalAnnotationBoxGeometry.horizontalMarginBorderAndPadding() };
    }

    // Note that inter-character geometry follows the ruby base's writing direction even though it's flipped in horizontal mode.
    if (isHorizontalWritingMode)
        return logicalAnnotationBoxGeometry.contentBoxSize();
    return { logicalAnnotationBoxGeometry.contentBoxHeight(), logicalAnnotationBoxGeometry.marginBoxWidth() };
}

void RubyFormattingContext::applyAnnotationContributionToLayoutBounds(InlineLevelBox& rubyBaseInlineBox) const
{
    // In order to ensure consistent spacing of lines, documents with ruby typically ensure that the line-height is
    // large enough to accommodate ruby between lines of text. Therefore, ordinarily, ruby annotation containers and ruby annotation
    // boxes do not contribute to the measured height of a line’s inline contents;
    // line-height calculations are performed using only the ruby base container, exactly as if it were a normal inline.
    // However, if the line-height specified on the ruby container is less than the distance between the top of the top ruby annotation
    // container and the bottom of the bottom ruby annotation container, then additional leading is added on the appropriate side(s).

    auto& rubyBaseLayoutBox = rubyBaseInlineBox.layoutBox();
    ASSERT(rubyBaseLayoutBox.isRubyBase());
    auto* annotationBox = rubyBaseLayoutBox.associatedRubyAnnotationBox();
    if (!isInterlinearAnnotation(annotationBox))
        return;

    auto over = InlineLayoutUnit { };
    auto under = InlineLayoutUnit { };
    auto annotationBoxLogicalHeight = InlineLayoutUnit { parentFormattingContext().geometryForBox(*annotationBox).marginBoxHeight() };
    if (annotationBox->style().rubyPosition() == RubyPosition::Before)
        over = annotationBoxLogicalHeight;
    else
        under = annotationBoxLogicalHeight;

    auto layoutBounds = rubyBaseInlineBox.layoutBounds();
    if (rubyBaseInlineBox.isPreferredLineHeightFontMetricsBased()) {
        layoutBounds.ascent += over;
        layoutBounds.descent += under;
    } else {
        auto& fontMetrics = rubyBaseLayoutBox.style().metricsOfPrimaryFont();
        auto ascent = fontMetrics.floatAscent() + over;
        auto descent = fontMetrics.floatDescent() + under;
        if (layoutBounds.height() < ascent + descent)
            layoutBounds = { ascent , descent };
    }
    rubyBaseInlineBox.setLayoutBounds(layoutBounds);
}

static inline InlineLayoutUnit halfOfAFullWidthCharacter(const Box& annotationBox)
{
    return annotationBox.style().computedFontSize() / 2.f;
}

static inline size_t baseContentIndex(size_t rubyBaseStartIndex, const InlineDisplay::Boxes& boxes)
{
    auto baseContentIndex = rubyBaseStartIndex + 1;
    if (boxes[baseContentIndex].layoutBox().isRubyAnnotationBox())
        ++baseContentIndex;
    return baseContentIndex;
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

    auto isHorizontalWritingMode = rubyBaseLayoutBox.style().isHorizontalWritingMode();
    auto baseContentStartIndex = baseContentIndex(rubyBaseStartIndex, boxes);
    if (baseContentStartIndex >= boxes.size()) {
        ASSERT_NOT_REACHED();
        return { };
    }
    // FIXME: Usually the first content box is visually the leftmost, but we should really look for content shifted to the left through negative margins on inline boxes.
    auto gapBetweenBaseAndContent = [&] {
        auto contentVisualRect = boxes[baseContentStartIndex].visualRectIgnoringBlockDirection();
        auto baseVisualRect = boxes[rubyBaseStartIndex].visualRectIgnoringBlockDirection();
        if (isHorizontalWritingMode)
            return std::max(0.f, contentVisualRect.x() - baseVisualRect.x());
        return std::max(0.f, contentVisualRect.y() - baseVisualRect.y());
    };
    auto overhangValue = std::min(halfOfAFullWidthCharacter(*annotationBox), gapBetweenBaseAndContent());
    auto wouldAnnotationOrBaseOverlapLineContent = [&] {
        // Check of adjacent (previous) content for overlapping.
        auto overhangingAnnotationRect = InlineLayoutRect { BoxGeometry::marginBoxRect(parentFormattingContext().geometryForBox(*annotationBox)) };
        auto baseContentBoxRect = boxes[baseContentStartIndex].inkOverflow();
        // This is how much the annotation box/ base content would be closer to content outside of base.
        auto offset = isHorizontalWritingMode ? LayoutSize(-overhangValue, 0.f) : LayoutSize(0.f, -overhangValue);
        overhangingAnnotationRect.move(offset);
        baseContentBoxRect.move(offset);

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

    auto isHorizontalWritingMode = rubyBaseLayoutBox.style().isHorizontalWritingMode();
    // FIXME: Usually the last content box is visually the rightmost, but negative margin may override it.
    // FIXME: Currently justified content always expands producing 0 value for gapBetweenBaseEndAndContent.
    auto gapBetweenBaseEndAndContent = [&] {
        auto baseStartVisualRect = boxes[rubyBaseStartIndex].visualRectIgnoringBlockDirection();
        auto baseContentEndVisualRect = boxes[rubyBaseContentEndIndex].visualRectIgnoringBlockDirection();
        if (isHorizontalWritingMode)
            return std::max(0.f, baseStartVisualRect.maxX() - baseContentEndVisualRect.maxX());
        return std::max(0.f, baseStartVisualRect.maxY() - baseContentEndVisualRect.maxY());
    };
    auto overhangValue = std::min(halfOfAFullWidthCharacter(*annotationBox), gapBetweenBaseEndAndContent());
    auto wouldAnnotationOrBaseOverlapLineContent = [&] {
        // Check of adjacent (next) content for overlapping.
        auto overhangingAnnotationRect = InlineLayoutRect { BoxGeometry::marginBoxRect(parentFormattingContext().geometryForBox(*annotationBox)) };
        auto baseContentBoxRect = boxes[rubyBaseContentEndIndex].inkOverflow();
        // This is how much the base content would be closer to content outside of base.
        auto offset = isHorizontalWritingMode ? LayoutSize(overhangValue, 0.f) : LayoutSize(0.f, overhangValue);
        overhangingAnnotationRect.move(offset);
        baseContentBoxRect.move(offset);

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

InlineLayoutRect RubyFormattingContext::visualRectIncludingBlockDirection(const InlineLayoutRect& visualRectIgnoringBlockDirection) const
{
    auto& rootStyle = parentFormattingContext().root().style();
    if (!rootStyle.isFlippedLinesWritingMode())
        return visualRectIgnoringBlockDirection;

    auto flippedRect = visualRectIgnoringBlockDirection;
    rootStyle.isVerticalWritingMode() ? flippedRect.setX(flippedRect.x() - flippedRect.width()) : flippedRect.setY(flippedRect.y() - flippedRect.height());
    return flippedRect;
}

std::optional<bool> RubyFormattingContext::annotationOverlapCheck(const InlineDisplay::Box& adjacentDisplayBox, const InlineLayoutRect& overhangingRect) const
{
    // We are in the middle of a line, should not see any line breaks or ellipsis boxes here.
    ASSERT(adjacentDisplayBox.isText() || adjacentDisplayBox.isAtomicInlineLevelBox() || adjacentDisplayBox.isInlineBox() || adjacentDisplayBox.isGenericInlineLevelBox() || adjacentDisplayBox.isWordSeparator());
    // Skip empty content like <span></span>
    if (adjacentDisplayBox.visualRectIgnoringBlockDirection().isEmpty())
        return { };
    if (visualRectIncludingBlockDirection(adjacentDisplayBox.inkOverflow()).intersects(visualRectIncludingBlockDirection(overhangingRect)))
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
        if (visualRectIncludingBlockDirection(annotationMarginBoxRect).intersects(visualRectIncludingBlockDirection(overhangingRect)))
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
            if (auto* annotationBox = rubyLayoutBox.associatedRubyAnnotationBox()) {
                auto annotationMarginBoxWidth = InlineLayoutUnit { parentFormattingContext().geometryForBox(*annotationBox).marginBoxWidth() };
                candidateContentLogicalWidth += isInterlinearAnnotation(annotationBox) ? std::max(baseLogicalWidth(), annotationMarginBoxWidth) : (baseLogicalWidth() + annotationMarginBoxWidth);
            } else
                candidateContentLogicalWidth += baseLogicalWidth();
            continue;
        }
        ASSERT_NOT_REACHED();
        ++index;
    }
    return candidateContentLogicalWidth;
}

}
}

