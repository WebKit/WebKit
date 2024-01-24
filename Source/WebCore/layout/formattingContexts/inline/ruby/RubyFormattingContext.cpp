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

#include "InlineContentAligner.h"
#include "InlineFormattingContext.h"
#include "InlineLine.h"
#include "RenderStyleInlines.h"

namespace WebCore {
namespace Layout {

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

static RubyPosition rubyPosition(const Box& rubyBaseLayoutBox)
{
    ASSERT(rubyBaseLayoutBox.isRubyBase());
    auto computedRubyPosition = rubyBaseLayoutBox.style().rubyPosition();
    if (rubyBaseLayoutBox.style().isHorizontalWritingMode())
        return computedRubyPosition;
    // inter-character: If the writing mode of the enclosing ruby container is vertical, this value has the same effect as over.
    return computedRubyPosition == RubyPosition::InterCharacter ? RubyPosition::Before : computedRubyPosition;
}

static inline InlineRect annotationMarginBoxVisualRect(const Box& annotationBox, InlineLayoutUnit lineHeight, const InlineFormattingContext& inlineFormattingContext)
{
    auto marginBoxLogicalRect = InlineRect { BoxGeometry::marginBoxRect(inlineFormattingContext.geometryForBox(annotationBox)) };
    auto& rootStyle = inlineFormattingContext.root().style();
    if (rootStyle.isHorizontalWritingMode())
        return marginBoxLogicalRect;
    auto visualTopLeft = marginBoxLogicalRect.topLeft().transposedPoint();
    if (rootStyle.isFlippedBlocksWritingMode())
        return InlineRect { visualTopLeft, marginBoxLogicalRect.size().transposedSize() };
    visualTopLeft.move(lineHeight - marginBoxLogicalRect.height(), { });
    return InlineRect { visualTopLeft, marginBoxLogicalRect.size().transposedSize() };
}

static InlineLayoutUnit baseLogicalWidthFromRubyBaseEnd(const Box& rubyBaseLayoutBox, const Line::RunList& lineRuns, const InlineContentBreaker::ContinuousContent::RunList& candidateRuns)
{
    ASSERT(rubyBaseLayoutBox.isRubyBase());
    // Canidate content is supposed to hold the base content and in case of soft wrap opportunities, line may have some base content too.
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

static bool annotationOverlapCheck(const InlineDisplay::Box& adjacentDisplayBox, const InlineLayoutRect& overhangingRect, InlineLayoutUnit lineLogicalHeight, const InlineFormattingContext& inlineFormattingContext)
{
    // We are in the middle of a line, should not see any line breaks or ellipsis boxes here.
    ASSERT(!adjacentDisplayBox.isEllipsis() && !adjacentDisplayBox.isRootInlineBox());
    // Skip empty content like <span></span>
    if (adjacentDisplayBox.visualRectIgnoringBlockDirection().isEmpty())
        return false;

    if (adjacentDisplayBox.inkOverflow().intersects(overhangingRect))
        return true;
    auto& adjacentLayoutBox = adjacentDisplayBox.layoutBox();
    // Adjacent ruby may have overlapping annotation.
    if (adjacentLayoutBox.isRubyBase() && adjacentLayoutBox.associatedRubyAnnotationBox())
        return annotationMarginBoxVisualRect(*adjacentLayoutBox.associatedRubyAnnotationBox(), lineLogicalHeight, inlineFormattingContext).intersects(overhangingRect);
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

InlineLayoutUnit RubyFormattingContext::annotationBoxLogicalWidth(const Box& rubyBaseLayoutBox, const InlineFormattingContext& inlineFormattingContext)
{
    ASSERT(rubyBaseLayoutBox.isRubyBase());
    auto* annotationBox = rubyBaseLayoutBox.associatedRubyAnnotationBox();
    if (!annotationBox)
        return { };

    return inlineFormattingContext.geometryForBox(*annotationBox).marginBoxWidth();
}

InlineLayoutUnit RubyFormattingContext::baseEndAdditionalLogicalWidth(const Box& rubyBaseLayoutBox, const Line::RunList& lineRuns, const InlineContentBreaker::ContinuousContent::RunList& candidateRuns, const InlineFormattingContext& inlineFormattingContext)
{
    ASSERT(rubyBaseLayoutBox.isRubyBase());
    if (hasInterlinearAnnotation(rubyBaseLayoutBox)) {
        // Base is supposed be at least as wide as the annotation is.
        // Let's adjust the inline box end width to accomodate such overflowing interlinear annotations.
        auto rubyBaseContentWidth = baseLogicalWidthFromRubyBaseEnd(rubyBaseLayoutBox, lineRuns, candidateRuns);
        ASSERT(rubyBaseContentWidth >= 0);
        return std::max(0.f, annotationBoxLogicalWidth(rubyBaseLayoutBox, inlineFormattingContext) - rubyBaseContentWidth);
    }
    // While inter-character annotations don't participate in inline layout, they take up space.
    return annotationBoxLogicalWidth(rubyBaseLayoutBox, inlineFormattingContext);
}

size_t RubyFormattingContext::applyRubyAlignOnBaseContent(size_t rubyBaseStart, Line& line, HashMap<const Box*, InlineLayoutUnit>& alignmentOffsetList, const InlineFormattingContext& inlineFormattingContext)
{
    auto& runs = line.runs();
    if (runs.isEmpty()) {
        ASSERT_NOT_REACHED();
        return rubyBaseStart;
    }
    auto& rubyBaseLayoutBox = runs[rubyBaseStart].layoutBox();
    auto rubyBaseEnd = [&] {
        auto& rubyBox = rubyBaseLayoutBox.parent();
        for (auto index = rubyBaseStart + 1; index < runs.size(); ++index) {
            if (&runs[index].layoutBox().parent() == &rubyBox)
                return index;
        }
        return runs.size() - 1;
    }();
    if (rubyBaseEnd - rubyBaseStart == 1) {
        // Blank base needs no alignment.
        return rubyBaseEnd;
    }
    auto* annotationBox = rubyBaseLayoutBox.associatedRubyAnnotationBox();
    if (!annotationBox)
        return rubyBaseStart + 1;

    auto annotationBoxLogicalWidth = InlineLayoutUnit { inlineFormattingContext.geometryForBox(*annotationBox).marginBoxWidth() };
    auto baseContentLogicalWidth = runs[rubyBaseEnd].logicalLeft() - runs[rubyBaseStart].logicalRight();
    if (annotationBoxLogicalWidth <= baseContentLogicalWidth)
        return rubyBaseStart + 1;

    // FIXME: ruby-align: space-around only
    auto spaceToDistribute = annotationBoxLogicalWidth - baseContentLogicalWidth;
    auto alignmentOffset = InlineContentAligner::applyRubyAlignSpaceAround(line.runs(), { rubyBaseStart, rubyBaseEnd + 1 }, spaceToDistribute);
    // Reset the spacing we added at LineBuilder.
    auto& rubyBaseEndRun = runs[rubyBaseEnd];
    rubyBaseEndRun.shrinkHorizontally(spaceToDistribute);
    rubyBaseEndRun.moveHorizontally(2 * alignmentOffset);

    ASSERT(!alignmentOffsetList.contains(&rubyBaseLayoutBox));
    alignmentOffsetList.add(&rubyBaseLayoutBox, alignmentOffset);
    return rubyBaseEnd;
}

HashMap<const Box*, InlineLayoutUnit> RubyFormattingContext::applyRubyAlign(Line& line, const InlineFormattingContext& inlineFormattingContext)
{
    HashMap<const Box*, InlineLayoutUnit> alignmentOffsetList;
    // https://drafts.csswg.org/css-ruby/#interlinear-inline
    // Within each base and annotation box, how the extra space is distributed when its content is narrower than
    // the measure of the box is specified by its ruby-align property.
    auto& runs = line.runs();
    for (size_t index = 0; index < runs.size(); ++index) {
        auto& lineRun = runs[index];
        if (lineRun.isInlineBoxStart() && lineRun.layoutBox().isRubyBase())
            index = applyRubyAlignOnBaseContent(index, line, alignmentOffsetList, inlineFormattingContext);
    }
    return alignmentOffsetList;
}

InlineLayoutUnit RubyFormattingContext::applyRubyAlignOnAnnotationBox(Line& line, InlineLayoutUnit spaceToDistribute, const InlineFormattingContext&)
{
    return InlineContentAligner::applyRubyAlignSpaceAround(line.runs(), { 0, line.runs().size() }, spaceToDistribute);
}

void RubyFormattingContext::applyAlignmentOffsetList(InlineDisplay::Boxes& displayBoxes, const HashMap<const Box*, InlineLayoutUnit>& alignmentOffsetList, RubyBasesMayNeedResizing rubyBasesMayNeedResizing, InlineFormattingContext& inlineFormattingContext)
{
    if (alignmentOffsetList.isEmpty())
        return;
    InlineContentAligner::applyRubyBaseAlignmentOffset(displayBoxes, alignmentOffsetList, rubyBasesMayNeedResizing == RubyBasesMayNeedResizing::No ? InlineContentAligner::AdjustContentOnlyInsideRubyBase::Yes : InlineContentAligner::AdjustContentOnlyInsideRubyBase::No, inlineFormattingContext);
}

void RubyFormattingContext::applyAnnotationAlignmentOffset(InlineDisplay::Boxes& displayBoxes, InlineLayoutUnit alignmentOffset, InlineFormattingContext& inlineFormattingContext)
{
    if (!alignmentOffset)
        return;
    InlineContentAligner::applyRubyAnnotationAlignmentOffset(displayBoxes, alignmentOffset, inlineFormattingContext);
}

InlineLayoutUnit RubyFormattingContext::baseEndAdditionalLogicalWidth(const Box& rubyBaseLayoutBox, const InlineDisplay::Box&, InlineLayoutUnit baseContentWidth, const InlineFormattingContext& inlineFormattingContext)
{
    if (!hasInterCharacterAnnotation(rubyBaseLayoutBox)) {
        // FIXME: We may want to include interlinear annotations here too so that applyAlignmentOffsetList would not need to initiate resizing (only moving base content).
        if (baseContentWidth)
            return { };
        auto* annotationBox = rubyBaseLayoutBox.associatedRubyAnnotationBox();
        if (!annotationBox)
            return { };
        auto& annotationBoxLogicalGeometry = inlineFormattingContext.geometryForBox(*annotationBox);
        return annotationBoxLogicalGeometry.marginBoxWidth();
    }
    // Note that intercharacter annotation stays vertical even when the ruby itself is vertical (which makes it look like interlinear).
    return annotationBoxLogicalWidth(rubyBaseLayoutBox, inlineFormattingContext);
}

InlineLayoutPoint RubyFormattingContext::placeAnnotationBox(const Box& rubyBaseLayoutBox, const Rect& rubyBaseMarginBoxLogicalRect, const InlineFormattingContext& inlineFormattingContext)
{
    ASSERT(rubyBaseLayoutBox.isRubyBase());
    auto* annotationBox = rubyBaseLayoutBox.associatedRubyAnnotationBox();
    if (!annotationBox) {
        ASSERT_NOT_REACHED();
        return { };
    }
    auto& annotationBoxLogicalGeometry = inlineFormattingContext.geometryForBox(*annotationBox);

    if (hasInterlinearAnnotation(rubyBaseLayoutBox)) {
        // Move it over/under the base and make it border box positioned.
        auto leftOffset = annotationBoxLogicalGeometry.marginStart();
        auto topOffset = rubyPosition(rubyBaseLayoutBox) == RubyPosition::Before ? -annotationBoxLogicalGeometry.marginBoxHeight() : rubyBaseMarginBoxLogicalRect.height();
        auto logicalTopLeft = rubyBaseMarginBoxLogicalRect.topLeft();
        logicalTopLeft.move(LayoutSize { leftOffset, topOffset });
        return logicalTopLeft;
    }
    // Inter-character annotation box is stretched to the size of the base content box and vertically centered.
    auto annotationContentBoxLogicalHeight = annotationBoxLogicalGeometry.contentBoxHeight();
    auto annotationBorderTop = annotationBoxLogicalGeometry.borderBefore();
    auto borderBoxRight = rubyBaseMarginBoxLogicalRect.right() - annotationBoxLogicalGeometry.marginBoxWidth() + annotationBoxLogicalGeometry.marginStart();
    return { borderBoxRight, rubyBaseMarginBoxLogicalRect.top() + ((rubyBaseMarginBoxLogicalRect.height() - annotationContentBoxLogicalHeight) / 2) - annotationBorderTop };
}

InlineLayoutSize RubyFormattingContext::sizeAnnotationBox(const Box& rubyBaseLayoutBox, const Rect& rubyBaseMarginBoxLogicalRect, const InlineFormattingContext& inlineFormattingContext)
{
    // FIXME: This is where we should take advantage of the ruby-column setup.
    ASSERT(rubyBaseLayoutBox.isRubyBase());
    auto* annotationBox = rubyBaseLayoutBox.associatedRubyAnnotationBox();
    if (!annotationBox) {
        ASSERT_NOT_REACHED();
        return { };
    }
    auto& annotationBoxLogicalGeometry = inlineFormattingContext.geometryForBox(*annotationBox);
    if (hasInterlinearAnnotation(rubyBaseLayoutBox))
        return { std::max(rubyBaseMarginBoxLogicalRect.width(), annotationBoxLogicalGeometry.marginBoxWidth()) - annotationBoxLogicalGeometry.horizontalMarginBorderAndPadding(), annotationBoxLogicalGeometry.contentBoxHeight() };
    return annotationBoxLogicalGeometry.contentBoxSize();
}

void RubyFormattingContext::adjustLayoutBoundsAndStretchAncestorRubyBase(LineBox& lineBox, InlineLevelBox& rubyBaseInlineBox, MaximumLayoutBoundsStretchMap& descendantRubySet, const InlineFormattingContext& inlineFormattingContext)
{
    auto& rubyBaseLayoutBox = rubyBaseInlineBox.layoutBox();
    ASSERT(rubyBaseLayoutBox.isRubyBase());

    auto stretchAncestorRubyBaseIfApplicable = [&](auto layoutBounds) {
        auto& rootBox = inlineFormattingContext.root();
        for (auto* ancestor = &rubyBaseLayoutBox.parent(); ancestor != &rootBox; ancestor = &ancestor->parent()) {
            if (ancestor->isRubyBase()) {
                auto* ancestorInlineBox = lineBox.inlineLevelBoxFor(*ancestor);
                if (!ancestorInlineBox) {
                    ASSERT_NOT_REACHED();
                    break;
                }
                auto previousDescendantLayoutBounds = descendantRubySet.get(ancestorInlineBox);
                descendantRubySet.set(ancestorInlineBox, InlineLevelBox::AscentAndDescent { std::max(previousDescendantLayoutBounds.ascent, layoutBounds.ascent), std::max(previousDescendantLayoutBounds.descent, layoutBounds.descent) });
                break;
            }
        }
    };

    auto layoutBounds = rubyBaseInlineBox.layoutBounds();
    auto* annotationBox = rubyBaseLayoutBox.associatedRubyAnnotationBox();
    if (!annotationBox || !hasInterlinearAnnotation(rubyBaseLayoutBox)) {
        // Make sure descendant rubies with annotations are propagated.
        stretchAncestorRubyBaseIfApplicable(layoutBounds);
        return;
    }

    auto over = InlineLayoutUnit { };
    auto under = InlineLayoutUnit { };
    auto annotationBoxLogicalHeight = InlineLayoutUnit { inlineFormattingContext.geometryForBox(*annotationBox).marginBoxHeight() };
    if (rubyPosition(rubyBaseLayoutBox) == RubyPosition::Before)
        over = annotationBoxLogicalHeight;
    else
        under = annotationBoxLogicalHeight;

    // FIXME: The spec says annotation should not stretch the line unless line-height is not normal and annotation does not fit (i.e. line is sized too small for the annotation)
    // Legacy ruby behaves slightly differently by stretching the line box as needed.
    auto isFirstFormattedLine = !lineBox.lineIndex();
    auto descendantLayoutBounds = descendantRubySet.get(&rubyBaseInlineBox);
    auto ascent = std::max(rubyBaseInlineBox.ascent(), descendantLayoutBounds.ascent);
    auto descent = std::max(rubyBaseInlineBox.descent(), descendantLayoutBounds.descent);

    if (rubyBaseInlineBox.isPreferredLineHeightFontMetricsBased()) {
        auto extraSpaceForAnnotation = InlineLayoutUnit { };
        if (!isFirstFormattedLine) {
            // Note that annotation may leak into the half leading space (gap between lines).
            auto lineGap = rubyBaseLayoutBox.style().metricsOfPrimaryFont().lineSpacing();
            extraSpaceForAnnotation = std::max(0.f, (lineGap - (ascent + descent)) / 2);
        }
        auto ascentWithAnnotation = (ascent + over) - extraSpaceForAnnotation;
        auto descentWithAnnotation = (descent + under) - extraSpaceForAnnotation;

        layoutBounds.ascent = std::max(ascentWithAnnotation, layoutBounds.ascent);
        layoutBounds.descent = std::max(descentWithAnnotation, layoutBounds.descent);
    } else {
        auto ascentWithAnnotation = ascent + over;
        auto descentWithAnnotation = descent + under;
        // line-height may produce enough space for annotation.
        if (layoutBounds.height() < ascentWithAnnotation + descentWithAnnotation) {
            layoutBounds.ascent = std::max(ascentWithAnnotation, layoutBounds.ascent);
            layoutBounds.descent = std::max(descentWithAnnotation, layoutBounds.descent);
        }
    }

    rubyBaseInlineBox.setLayoutBounds(layoutBounds);
    stretchAncestorRubyBaseIfApplicable(layoutBounds);
}

void RubyFormattingContext::applyAnnotationContributionToLayoutBounds(LineBox& lineBox, const InlineFormattingContext& inlineFormattingContext)
{
    // In order to ensure consistent spacing of lines, documents with ruby typically ensure that the line-height is
    // large enough to accommodate ruby between lines of text. Therefore, ordinarily, ruby annotation containers and ruby annotation
    // boxes do not contribute to the measured height of a line's inline contents;
    // line-height calculations are performed using only the ruby base container, exactly as if it were a normal inline.
    // However, if the line-height specified on the ruby container is less than the distance between the top of the top ruby annotation
    // container and the bottom of the bottom ruby annotation container, then additional leading is added on the appropriate side(s).
    MaximumLayoutBoundsStretchMap descentRubySet;
    for (auto& inlineLevelBox : makeReversedRange(lineBox.nonRootInlineLevelBoxes())) {
        if (!inlineLevelBox.isInlineBox() || !inlineLevelBox.layoutBox().isRubyBase())
            continue;
        adjustLayoutBoundsAndStretchAncestorRubyBase(lineBox, inlineLevelBox, descentRubySet, inlineFormattingContext);
    }
}

InlineLayoutUnit RubyFormattingContext::overhangForAnnotationBefore(const Box& rubyBaseLayoutBox, size_t rubyBaseStart, const InlineDisplay::Boxes& boxes, InlineLayoutUnit lineLogicalHeight, const InlineFormattingContext& inlineFormattingContext)
{
    // [root inline box][ruby container][ruby base][ruby annotation]
    ASSERT(rubyBaseStart >= 2);
    auto* annotationBox = rubyBaseLayoutBox.associatedRubyAnnotationBox();
    if (!annotationBox || !hasInterlinearAnnotation(rubyBaseLayoutBox) || rubyBaseStart <= 2)
        return { };
    if (rubyBaseStart + 1 >= boxes.size()) {
        // We have to have some base content.
        ASSERT_NOT_REACHED();
        return { };
    }
    auto isHorizontalWritingMode = inlineFormattingContext.root().style().isHorizontalWritingMode();
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
        auto overhangingAnnotationVisualRect = annotationMarginBoxVisualRect(*annotationBox, lineLogicalHeight, inlineFormattingContext);
        auto baseContentBoxRect = boxes[baseContentStart].inkOverflow();
        // This is how much the annotation box/base content would be closer to content outside of base.
        auto offset = isHorizontalWritingMode ? InlineLayoutPoint(-overhangValue, 0.f) : InlineLayoutPoint(0.f, -overhangValue);
        overhangingAnnotationVisualRect.moveBy(offset);
        baseContentBoxRect.moveBy(offset);

        for (size_t index = 1; index < rubyBaseStart - 1; ++index) {
            auto& previousDisplayBox = boxes[index];
            if (annotationOverlapCheck(previousDisplayBox, overhangingAnnotationVisualRect, lineLogicalHeight, inlineFormattingContext))
                return true;
            if (annotationOverlapCheck(previousDisplayBox, baseContentBoxRect, lineLogicalHeight, inlineFormattingContext))
                return true;
        }
        return false;
    };
    return wouldAnnotationOrBaseOverlapAdjacentContent() ? 0.f : overhangValue;
}

InlineLayoutUnit RubyFormattingContext::overhangForAnnotationAfter(const Box& rubyBaseLayoutBox, WTF::Range<size_t> rubyBaseRange, const InlineDisplay::Boxes& boxes, InlineLayoutUnit lineLogicalHeight, const InlineFormattingContext& inlineFormattingContext)
{
    auto* annotationBox = rubyBaseLayoutBox.associatedRubyAnnotationBox();
    if (!annotationBox || !hasInterlinearAnnotation(rubyBaseLayoutBox))
        return { };

    if (!rubyBaseRange || rubyBaseRange.distance() == 1 || rubyBaseRange.end() == boxes.size())
        return { };

    auto isHorizontalWritingMode = inlineFormattingContext.root().style().isHorizontalWritingMode();
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
        auto overhangingAnnotationVisualRect = annotationMarginBoxVisualRect(*annotationBox, lineLogicalHeight, inlineFormattingContext);
        auto baseContentBoxRect = boxes[rubyBaseContentEnd].inkOverflow();
        // This is how much the base content would be closer to content outside of base.
        auto offset = isHorizontalWritingMode ? InlineLayoutPoint(overhangValue, 0.f) : InlineLayoutPoint(0.f, overhangValue);
        overhangingAnnotationVisualRect.moveBy(offset);
        baseContentBoxRect.moveBy(offset);

        for (size_t index = boxes.size() - 1; index >= rubyBaseRange.end(); --index) {
            auto& previousDisplayBox = boxes[index];
            if (annotationOverlapCheck(previousDisplayBox, overhangingAnnotationVisualRect, lineLogicalHeight, inlineFormattingContext))
                return true;
            if (annotationOverlapCheck(previousDisplayBox, baseContentBoxRect, lineLogicalHeight, inlineFormattingContext))
                return true;
        }
        return false;
    };
    return wouldAnnotationOrBaseOverlapLineContent() ? 0.f : overhangValue;
}

bool RubyFormattingContext::hasInterlinearAnnotation(const Box& rubyBaseLayoutBox)
{
    ASSERT(rubyBaseLayoutBox.isRubyBase());
    return rubyBaseLayoutBox.associatedRubyAnnotationBox() && !hasInterCharacterAnnotation(rubyBaseLayoutBox);
}

bool RubyFormattingContext::hasInterCharacterAnnotation(const Box& rubyBaseLayoutBox)
{
    ASSERT(rubyBaseLayoutBox.isRubyBase());
    if (!rubyBaseLayoutBox.style().isHorizontalWritingMode()) {
        // If the writing mode of the enclosing ruby container is vertical, this value has the same effect as over.
        return false;
    }

    if (auto* annotationBox = rubyBaseLayoutBox.associatedRubyAnnotationBox())
        return annotationBox->style().rubyPosition() == RubyPosition::InterCharacter;
    return false;
}

}
}

