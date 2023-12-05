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

static bool isInterlinearAnnotation(const Box* annotationBox)
{
    return annotationBox && annotationBox->isInterlinearRubyAnnotationBox();
}

static inline InlineLayoutUnit halfOfAFullWidthCharacter(const Box& annotationBox)
{
    return annotationBox.style().computedFontSize() / 2.f;
}

static inline size_t baseContentIndex(size_t rubyBaseStart, const InlineDisplay::Boxes& boxes)
{
    auto baseContentIndex = rubyBaseStart + 1;
    if (boxes[baseContentIndex].layoutBox().isRubyAnnotationBox())
        ++baseContentIndex;
    return baseContentIndex;
}

static InlineLayoutRect visualRectIncludingBlockDirection(const InlineLayoutRect& visualRectIgnoringBlockDirection, const RenderStyle& rootStyle)
{
    if (!rootStyle.isFlippedLinesWritingMode())
        return visualRectIgnoringBlockDirection;

    auto flippedRect = visualRectIgnoringBlockDirection;
    rootStyle.isVerticalWritingMode() ? flippedRect.setX(flippedRect.x() - flippedRect.width()) : flippedRect.setY(flippedRect.y() - flippedRect.height());
    return flippedRect;
}

static bool annotationOverlapCheck(const InlineDisplay::Box& adjacentDisplayBox, const InlineLayoutRect& overhangingRect, const InlineFormattingContext& inlineFormattingContext)
{
    // We are in the middle of a line, should not see any line breaks or ellipsis boxes here.
    ASSERT(!adjacentDisplayBox.isEllipsis() && !adjacentDisplayBox.isRootInlineBox());
    // Skip empty content like <span></span>
    if (adjacentDisplayBox.visualRectIgnoringBlockDirection().isEmpty())
        return false;

    auto& rootStyle = inlineFormattingContext.root().style();
    if (visualRectIncludingBlockDirection(adjacentDisplayBox.inkOverflow(), rootStyle).intersects(visualRectIncludingBlockDirection(overhangingRect, rootStyle)))
        return true;
    auto& adjacentLayoutBox = adjacentDisplayBox.layoutBox();
    // Adjacent ruby may have overlapping annotation.
    if (adjacentLayoutBox.isRubyBase() && adjacentLayoutBox.associatedRubyAnnotationBox()) {
        auto annotationMarginBoxRect = InlineLayoutRect { BoxGeometry::marginBoxRect(inlineFormattingContext.geometryForBox(*adjacentLayoutBox.associatedRubyAnnotationBox())) };
        if (visualRectIncludingBlockDirection(annotationMarginBoxRect, rootStyle).intersects(visualRectIncludingBlockDirection(overhangingRect, rootStyle)))
            return true;
    }
    return false;
}

static bool canBreakAtCharacter(UChar character)
{
    auto lineBreak = (ULineBreak)u_getIntPropertyValue(character, UCHAR_LINE_BREAK);
    // UNICODE LINE BREAKING ALGORITHM
    // http://www.unicode.org/reports/tr14/
    // And Requirements for Japanese Text Layout, 3.1.7 Characters Not Starting a Line
    // http://www.w3.org/TR/2012/NOTE-jlreq-20120403/#characters_not_starting_a_line
    switch (lineBreak) {
    case U_LB_NONSTARTER:
    case U_LB_CLOSE_PARENTHESIS:
    case U_LB_CLOSE_PUNCTUATION:
    case U_LB_EXCLAMATION:
    case U_LB_BREAK_SYMBOLS:
    case U_LB_INFIX_NUMERIC:
    case U_LB_ZWSPACE:
    case U_LB_WORD_JOINER:
        return false;
    default:
        break;
    }
    // Special care for Requirements for Japanese Text Layout
    switch (character) {
    case 0x2019: // RIGHT SINGLE QUOTATION MARK
    case 0x201D: // RIGHT DOUBLE QUOTATION MARK
    case 0x00BB: // RIGHT-POINTING DOUBLE ANGLE QUOTATION MARK
    case 0x2010: // HYPHEN
    case 0x2013: // EN DASH
    case 0x300C: // LEFT CORNER BRACKET
        return false;
    default:
        break;
    }
    return true;
}

bool RubyFormattingContext::isAtSoftWrapOpportunity(const InlineItem& previous, const InlineItem& current)
{
    auto& previousLayoutBox = previous.layoutBox();
    auto& currentLayoutBox = current.layoutBox();
    ASSERT(previousLayoutBox.isRubyInlineBox() || currentLayoutBox.isRubyInlineBox());

    if (currentLayoutBox.isRuby()) {
        ASSERT((!previous.isInlineBoxStart() && !previous.isInlineBoxEnd()) || previous.layoutBox().isRubyInlineBox());

        if (current.isInlineBoxStart()) {
            // At the beginning of <ruby>.
            if (!previous.isText())
                return true;
            auto& leadingTextItem = downcast<InlineTextItem>(previous);
            if (!leadingTextItem.length()) {
                // FIXME: This needs to know prior context.
                return true;
            }
            auto lastCharacter = leadingTextItem.inlineTextBox().content()[leadingTextItem.end() - 1];
            return canBreakAtCharacter(lastCharacter);
        }
        // Don't break between base end and <ruby> end.
        return false;
    }

    if (currentLayoutBox.isRubyBase() || previousLayoutBox.isRubyBase()) {
        // There's always a soft wrap opportunity between two bases.
        return currentLayoutBox.isRubyBase() && previousLayoutBox.isRubyBase();
    }

    if (previousLayoutBox.isRuby()) {
        ASSERT((!current.isInlineBoxStart() && !current.isInlineBoxEnd()) || current.layoutBox().isRuby());

        if (previous.isInlineBoxEnd()) {
            // At the end of <ruby>
            if (!current.isText())
                return true;
            auto& trailingTextItem = downcast<InlineTextItem>(current);
            if (!trailingTextItem.length()) {
                // FIXME: This should be turned into one of those "can't decide it yet" cases.
                return true;
            }
            auto firstCharacter = trailingTextItem.inlineTextBox().content()[trailingTextItem.start()];
            return canBreakAtCharacter(firstCharacter);
        }
        // We should handled this case already when looking at current: base, previous: ruby.
        ASSERT_NOT_REACHED();
        return false;
    }

    ASSERT_NOT_REACHED();
    return false;
}

std::optional<InlineLayoutUnit> RubyFormattingContext::annotationBoxLogicalWidth(const Box& rubyBaseLayoutBox, const InlineFormattingContext& inlineFormattingContext)
{
    ASSERT(rubyBaseLayoutBox.isRubyBase());

    if (auto* annotationBox = rubyBaseLayoutBox.associatedRubyAnnotationBox())
        return InlineLayoutUnit { inlineFormattingContext.geometryForBox(*annotationBox).marginBoxWidth() };
    return { };
}

InlineLayoutUnit RubyFormattingContext::baseLogicalWidthFromRubyBaseEnd(const InlineItem& rubyBaseEnd, const Line::RunList& lineRuns, const InlineContentBreaker::ContinuousContent::RunList& candidateRuns)
{
    ASSERT(rubyBaseEnd.isInlineBoxEnd() && rubyBaseEnd.layoutBox().isRubyBase());
    // Canidate content is supposed to hold the base content and in case of soft wrap opportunities, line may have some base content too.
    auto& rubyBaseLayoutBox = rubyBaseEnd.layoutBox();
    auto baseLogicalWidth = InlineLayoutUnit { 0.f };
    auto hasSeenRubyBaseStart = false;
    for (auto& candidateRun : makeReversedRange(candidateRuns)) {
        auto& inlineItem = candidateRun.inlineItem;
        if (inlineItem.isInlineBoxStart() && &inlineItem.layoutBox() == &rubyBaseLayoutBox) {
            hasSeenRubyBaseStart = true;
            break;
        }
        baseLogicalWidth += candidateRun.logicalWidth;
    }
    if (hasSeenRubyBaseStart)
        return baseLogicalWidth;
    // Let's check the line for the rest of the base content.
    for (auto& lineRun : makeReversedRange(lineRuns)) {
        if ((lineRun.isInlineBoxStart() || lineRun.isLineSpanningInlineBoxStart()) && &lineRun.layoutBox() == &rubyBaseLayoutBox)
            break;
        baseLogicalWidth += lineRun.logicalWidth();
    }
    return baseLogicalWidth;
}

static size_t applyRubyAlignOnBaseContent(size_t rubyBaseStart, Line& line, const InlineFormattingContext& inlineFormattingContext)
{
    auto& runs = line.runs();
    auto& rubyBaseLayoutBox = runs[rubyBaseStart].layoutBox();
    auto* annotationBox = rubyBaseLayoutBox.associatedRubyAnnotationBox();
    if (!annotationBox)
        return rubyBaseStart;

    auto rubyBaseEnd = [&] {
        auto& rubyBox = rubyBaseLayoutBox.parent();
        for (auto index = rubyBaseStart + 1; index < runs.size(); ++index) {
            if (&runs[index].layoutBox().parent() == &rubyBox)
                return index;
        }
        return runs.size();
    }();
    if (rubyBaseEnd == rubyBaseStart + 1) {
        // Blank base needs no alignment.
        return rubyBaseEnd;
    }

    auto annotationBoxLogicalWidth = InlineLayoutUnit { inlineFormattingContext.geometryForBox(*annotationBox).marginBoxWidth() };
    auto baseContentLogicalWidth = runs[rubyBaseEnd - 1].logicalRight() - runs[rubyBaseStart].logicalLeft();

    if (annotationBoxLogicalWidth <= baseContentLogicalWidth)
        return rubyBaseEnd;

    if (rubyBaseEnd != runs.size()) {
        ASSERT(runs[rubyBaseEnd].isInlineBoxEnd());
        // Remove ruby inline box end extra width (see InlineBuilder where we add it to accomodate for overflowing annotation) before applying expansion.
        auto extraWidth = annotationBoxLogicalWidth - baseContentLogicalWidth;
        line.moveBy({ rubyBaseEnd, runs.size() }, extraWidth);
        line.expandBy(rubyBaseEnd, -extraWidth);
    }

    auto expansion = ExpansionInfo { };
    // ruby-align: space-around
    // As for space-between except that there exists an extra justification opportunities whose space is distributed half before and half after the ruby content.
    auto baseContentRunRange = WTF::Range<size_t> { rubyBaseStart + 1, rubyBaseEnd };
    TextUtil::computedExpansions(line.runs(), baseContentRunRange, { }, expansion);

    auto contentLogicalLeftOffset = InlineLayoutUnit { };
    if (expansion.opportunityCount) {
        auto extraSpace = annotationBoxLogicalWidth - baseContentLogicalWidth;
        contentLogicalLeftOffset = extraSpace / (expansion.opportunityCount + 1) / 2;
        extraSpace -= (2 * contentLogicalLeftOffset);
        line.applyExpansionOnRange(baseContentRunRange, expansion, extraSpace);
    } else
        contentLogicalLeftOffset = (annotationBoxLogicalWidth - baseContentLogicalWidth) / 2;
    line.moveBy(baseContentRunRange, contentLogicalLeftOffset);
    return rubyBaseEnd;
}

void RubyFormattingContext::applyRubyAlign(Line& line, const InlineFormattingContext& inlineFormattingContext)
{
    // https://drafts.csswg.org/css-ruby/#interlinear-inline
    // Within each base and annotation box, how the extra space is distributed when its content is narrower than
    // the measure of the box is specified by its ruby-align property.
    auto& runs = line.runs();
    for (size_t index = 0; index < runs.size(); ++index) {
        auto& lineRun = runs[index];
        if (lineRun.isInlineBoxStart() && lineRun.layoutBox().isRubyBase())
            index = applyRubyAlignOnBaseContent(index, line, inlineFormattingContext);
    }
}

InlineLayoutPoint RubyFormattingContext::placeAnnotationBox(const Box& rubyBaseLayoutBox, const InlineFormattingContext& inlineFormattingContext)
{
    ASSERT(rubyBaseLayoutBox.isRubyBase());
    auto* annotationBox = rubyBaseLayoutBox.associatedRubyAnnotationBox();
    if (!annotationBox) {
        ASSERT_NOT_REACHED();
        return { };
    }
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

InlineLayoutSize RubyFormattingContext::sizeAnnotationBox(const Box& rubyBaseLayoutBox, const InlineFormattingContext& inlineFormattingContext)
{
    // FIXME: This is where we should take advantage of the ruby-column setup.
    ASSERT(rubyBaseLayoutBox.isRubyBase());
    auto* annotationBox = rubyBaseLayoutBox.associatedRubyAnnotationBox();
    if (!annotationBox) {
        ASSERT_NOT_REACHED();
        return { };
    }
    auto isHorizontalWritingMode = rubyBaseLayoutBox.style().isHorizontalWritingMode();
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

void RubyFormattingContext::applyAnnotationContributionToLayoutBounds(InlineLevelBox& rubyBaseInlineBox, const InlineFormattingContext& inlineFormattingContext)
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
    auto annotationBoxLogicalHeight = InlineLayoutUnit { inlineFormattingContext.geometryForBox(*annotationBox).marginBoxHeight() };
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

InlineLayoutUnit RubyFormattingContext::overhangForAnnotationBefore(const Box& rubyBaseLayoutBox, size_t rubyBaseStart, const InlineDisplay::Boxes& boxes, const InlineFormattingContext& inlineFormattingContext)
{
    // [root inline box][ruby container][ruby base][ruby annotation]
    ASSERT(rubyBaseStart >= 2);
    auto* annotationBox = rubyBaseLayoutBox.associatedRubyAnnotationBox();
    if (!isInterlinearAnnotation(annotationBox) || rubyBaseStart <= 2)
        return { };
    if (rubyBaseStart + 1 >= boxes.size()) {
        // We have to have some base content.
        ASSERT_NOT_REACHED();
        return { };
    }

    auto isHorizontalWritingMode = rubyBaseLayoutBox.style().isHorizontalWritingMode();
    auto baseContentStart = baseContentIndex(rubyBaseStart, boxes);
    if (baseContentStart >= boxes.size()) {
        ASSERT_NOT_REACHED();
        return { };
    }
    // FIXME: Usually the first content box is visually the leftmost, but we should really look for content shifted to the left through negative margins on inline boxes.
    auto gapBetweenBaseAndContent = [&] {
        auto contentVisualRect = boxes[baseContentStart].visualRectIgnoringBlockDirection();
        auto baseVisualRect = boxes[rubyBaseStart].visualRectIgnoringBlockDirection();
        if (isHorizontalWritingMode)
            return std::max(0.f, contentVisualRect.x() - baseVisualRect.x());
        return std::max(0.f, contentVisualRect.y() - baseVisualRect.y());
    };
    auto overhangValue = std::min(halfOfAFullWidthCharacter(*annotationBox), gapBetweenBaseAndContent());
    auto wouldAnnotationOrBaseOverlapAdjacentContent = [&] {
        // Check of adjacent (previous) content for overlapping.
        auto overhangingAnnotationRect = InlineLayoutRect { BoxGeometry::marginBoxRect(inlineFormattingContext.geometryForBox(*annotationBox)) };
        auto baseContentBoxRect = boxes[baseContentStart].inkOverflow();
        // This is how much the annotation box/ base content would be closer to content outside of base.
        auto offset = isHorizontalWritingMode ? LayoutSize(-overhangValue, 0.f) : LayoutSize(0.f, -overhangValue);
        overhangingAnnotationRect.move(offset);
        baseContentBoxRect.move(offset);

        for (size_t index = 1; index < rubyBaseStart - 1; ++index) {
            auto& previousDisplayBox = boxes[index];
            if (annotationOverlapCheck(previousDisplayBox, overhangingAnnotationRect, inlineFormattingContext))
                return true;
            if (annotationOverlapCheck(previousDisplayBox, baseContentBoxRect, inlineFormattingContext))
                return true;
        }
        return false;
    };
    return wouldAnnotationOrBaseOverlapAdjacentContent() ? 0.f : overhangValue;
}

InlineLayoutUnit RubyFormattingContext::overhangForAnnotationAfter(const Box& rubyBaseLayoutBox, WTF::Range<size_t> rubyBaseRange, const InlineDisplay::Boxes& boxes, const InlineFormattingContext& inlineFormattingContext)
{
    auto* annotationBox = rubyBaseLayoutBox.associatedRubyAnnotationBox();
    if (!isInterlinearAnnotation(annotationBox))
        return { };

    if (!rubyBaseRange || rubyBaseRange.distance() == 1 || rubyBaseRange.end() == boxes.size())
        return { };

    auto isHorizontalWritingMode = rubyBaseLayoutBox.style().isHorizontalWritingMode();
    // FIXME: Usually the last content box is visually the rightmost, but negative margin may override it.
    // FIXME: Currently justified content always expands producing 0 value for gapBetweenBaseEndAndContent.
    auto rubyBaseContentEnd = rubyBaseRange.end() - 1;
    auto gapBetweenBaseEndAndContent = [&] {
        auto baseStartVisualRect = boxes[rubyBaseRange.begin()].visualRectIgnoringBlockDirection();
        auto baseContentEndVisualRect = boxes[rubyBaseContentEnd].visualRectIgnoringBlockDirection();
        if (isHorizontalWritingMode)
            return std::max(0.f, baseStartVisualRect.maxX() - baseContentEndVisualRect.maxX());
        return std::max(0.f, baseStartVisualRect.maxY() - baseContentEndVisualRect.maxY());
    };
    auto overhangValue = std::min(halfOfAFullWidthCharacter(*annotationBox), gapBetweenBaseEndAndContent());
    auto wouldAnnotationOrBaseOverlapLineContent = [&] {
        // Check of adjacent (next) content for overlapping.
        auto overhangingAnnotationRect = InlineLayoutRect { BoxGeometry::marginBoxRect(inlineFormattingContext.geometryForBox(*annotationBox)) };
        auto baseContentBoxRect = boxes[rubyBaseContentEnd].inkOverflow();
        // This is how much the base content would be closer to content outside of base.
        auto offset = isHorizontalWritingMode ? LayoutSize(overhangValue, 0.f) : LayoutSize(0.f, overhangValue);
        overhangingAnnotationRect.move(offset);
        baseContentBoxRect.move(offset);

        for (size_t index = boxes.size() - 1; index > rubyBaseRange.end(); --index) {
            auto& previousDisplayBox = boxes[index];
            if (annotationOverlapCheck(previousDisplayBox, overhangingAnnotationRect, inlineFormattingContext))
                return true;
            if (annotationOverlapCheck(previousDisplayBox, baseContentBoxRect, inlineFormattingContext))
                return true;
        }
        return false;
    };
    return wouldAnnotationOrBaseOverlapLineContent() ? 0.f : overhangValue;
}

}
}

