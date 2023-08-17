/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#include "InlineFormattingGeometry.h"

#include "FloatingContext.h"
#include "FontCascade.h"
#include "FormattingContext.h"
#include "InlineDisplayContent.h"
#include "InlineFormattingContext.h"
#include "InlineFormattingQuirks.h"
#include "InlineLevelBoxInlines.h"
#include "InlineLineBoxVerticalAligner.h"
#include "LayoutBox.h"
#include "LayoutElementBox.h"
#include "LengthFunctions.h"
#include "RenderStyleInlines.h"

namespace WebCore {
namespace Layout {

InlineFormattingGeometry::InlineFormattingGeometry(const InlineFormattingContext& inlineFormattingContext)
    : FormattingGeometry(inlineFormattingContext)
{
}

InlineLayoutUnit InlineFormattingGeometry::logicalTopForNextLine(const LineLayoutResult& lineLayoutResult, const InlineRect& lineLogicalRect, const FloatingContext& floatingContext) const
{
    auto didManageToPlaceInlineContentOrFloat = !lineLayoutResult.inlineItemRange.isEmpty();
    if (didManageToPlaceInlineContentOrFloat) {
        // Normally the next line's logical top is the previous line's logical bottom, but when the line ends
        // with the clear property set, the next line needs to clear the existing floats.
        if (lineLayoutResult.inlineContent.isEmpty())
            return lineLogicalRect.bottom();
        auto& lastRunLayoutBox = lineLayoutResult.inlineContent.last().layoutBox();
        if (!lastRunLayoutBox.hasFloatClear())
            return lineLogicalRect.bottom();
        auto positionWithClearance = floatingContext.verticalPositionWithClearance(lastRunLayoutBox, formattingContext().geometryForBox(lastRunLayoutBox));
        if (!positionWithClearance)
            return lineLogicalRect.bottom();
        return std::max(lineLogicalRect.bottom(), InlineLayoutUnit(positionWithClearance->position));
    }

    auto intrusiveFloatBottom = [&]() -> std::optional<InlineLayoutUnit> {
        // Floats must have prevented us placing any content on the line.
        // Move next line below the intrusive float(s).
        ASSERT(lineLayoutResult.inlineContent.isEmpty() || lineLayoutResult.inlineContent[0].isLineSpanningInlineBoxStart());
        ASSERT(lineLayoutResult.floatContent.hasIntrusiveFloat);
        auto nextLineLogicalTop = [&]() -> LayoutUnit {
            if (auto nextLineLogicalTopCandidate = lineLayoutResult.hintForNextLineTopToAvoidIntrusiveFloat)
                return LayoutUnit { *nextLineLogicalTopCandidate };
            // We have to have a hit when intrusive floats prevented any inline content placement.
            ASSERT_NOT_REACHED();
            return LayoutUnit { lineLogicalRect.top() + formattingContext().root().style().computedLineHeight() };
        };
        auto floatConstraints = floatingContext.constraints(toLayoutUnit(lineLogicalRect.top()), nextLineLogicalTop(), FloatingContext::MayBeAboveLastFloat::Yes);
        if (floatConstraints.left && floatConstraints.right) {
            // In case of left and right constraints, we need to pick the one that's closer to the current line.
            return std::min(floatConstraints.left->y, floatConstraints.right->y);
        }
        if (floatConstraints.left)
            return floatConstraints.left->y;
        if (floatConstraints.right)
            return floatConstraints.right->y;
        // If we didn't manage to place a content on this vertical position due to intrusive floats, we have to have
        // at least one float here.
        ASSERT_NOT_REACHED();
        return { };
    };
    if (auto firstAvailableVerticalPosition = intrusiveFloatBottom())
        return *firstAvailableVerticalPosition;
    // Do not get stuck on the same vertical position even when we find ourselves in this unexpected state.
    return ceil(nextafter(lineLogicalRect.bottom(), std::numeric_limits<float>::max()));
}

ContentWidthAndMargin InlineFormattingGeometry::inlineBlockContentWidthAndMargin(const Box& formattingContextRoot, const HorizontalConstraints& horizontalConstraints, const OverriddenHorizontalValues& overriddenHorizontalValues) const
{
    ASSERT(formattingContextRoot.isInFlow());

    // 10.3.10 'Inline-block', replaced elements in normal flow

    // Exactly as inline replaced elements.
    if (formattingContextRoot.isReplacedBox())
        return inlineReplacedContentWidthAndMargin(downcast<ElementBox>(formattingContextRoot), horizontalConstraints, { }, overriddenHorizontalValues);

    // 10.3.9 'Inline-block', non-replaced elements in normal flow

    // If 'width' is 'auto', the used value is the shrink-to-fit width as for floating elements.
    // A computed value of 'auto' for 'margin-left' or 'margin-right' becomes a used value of '0'.
    // #1
    auto width = computedValue(formattingContextRoot.style().logicalWidth(), horizontalConstraints.logicalWidth);
    if (!width)
        width = shrinkToFitWidth(formattingContextRoot, horizontalConstraints.logicalWidth);

    // #2
    auto computedHorizontalMargin = FormattingGeometry::computedHorizontalMargin(formattingContextRoot, horizontalConstraints);

    return ContentWidthAndMargin { *width, { computedHorizontalMargin.start.value_or(0_lu), computedHorizontalMargin.end.value_or(0_lu) } };
}

ContentHeightAndMargin InlineFormattingGeometry::inlineBlockContentHeightAndMargin(const Box& layoutBox, const HorizontalConstraints& horizontalConstraints, const OverriddenVerticalValues& overriddenVerticalValues) const
{
    ASSERT(layoutBox.isInFlow());

    // 10.6.2 Inline replaced elements, block-level replaced elements in normal flow, 'inline-block' replaced elements in normal flow and floating replaced elements
    if (layoutBox.isReplacedBox())
        return inlineReplacedContentHeightAndMargin(downcast<ElementBox>(layoutBox), horizontalConstraints, { }, overriddenVerticalValues);

    // 10.6.6 Complicated cases
    // - 'Inline-block', non-replaced elements.
    return complicatedCases(layoutBox, horizontalConstraints, overriddenVerticalValues);
}

bool InlineFormattingGeometry::inlineLevelBoxAffectsLineBox(const InlineLevelBox& inlineLevelBox) const
{
    if (!inlineLevelBox.mayStretchLineBox())
        return false;

    if (inlineLevelBox.isLineBreakBox())
        return false;
    if (inlineLevelBox.isListMarker())
        return true;
    if (inlineLevelBox.isInlineBox())
        return layoutState().inStandardsMode() ? true : formattingContext().formattingQuirks().inlineBoxAffectsLineBox(inlineLevelBox);
    if (inlineLevelBox.isAtomicInlineLevelBox())
        return true;
    return false;
}

InlineRect InlineFormattingGeometry::flipVisualRectToLogicalForWritingMode(const InlineRect& visualRect, WritingMode writingMode)
{
    switch (writingModeToBlockFlowDirection(writingMode)) {
    case BlockFlowDirection::TopToBottom:
        return visualRect;
    case BlockFlowDirection::LeftToRight:
    case BlockFlowDirection::RightToLeft: {
        // FIXME: While vertical-lr and vertical-rl modes do differ in the ordering direction of line boxes
        // in a block container (see: https://drafts.csswg.org/css-writing-modes/#block-flow)
        // we ignore it for now as RenderBlock takes care of it for us.
        return InlineRect { visualRect.left(), visualRect.top(), visualRect.height(), visualRect.width() };
    }
    default:
        ASSERT_NOT_REACHED();
        break;
    }
    return visualRect;
}

InlineLayoutUnit InlineFormattingGeometry::computedTextIndent(IsIntrinsicWidthMode isIntrinsicWidthMode, std::optional<bool> previousLineEndsWithLineBreak, InlineLayoutUnit availableWidth) const
{
    auto& root = formattingContext().root();

    // text-indent property specifies the indentation applied to lines of inline content in a block.
    // The indent is treated as a margin applied to the start edge of the line box.
    // The first formatted line of an element is always indented. For example, the first line of an anonymous block box
    // is only affected if it is the first child of its parent element.
    // If 'each-line' is specified, indentation also applies to all lines where the previous line ends with a hard break.
    // [Integration] root()->parent() would normally produce a valid layout box.
    bool shouldIndent = false;
    if (!previousLineEndsWithLineBreak) {
        shouldIndent = !root.isAnonymous();
        if (root.isAnonymous()) {
            if (!root.isInlineIntegrationRoot())
                shouldIndent = root.parent().firstInFlowChild() == &root;
            else
                shouldIndent = root.isFirstChildForIntegration();
        }
    } else
        shouldIndent = root.style().textIndentLine() == TextIndentLine::EachLine && *previousLineEndsWithLineBreak;

    // Specifying 'hanging' inverts whether the line should be indented or not.
    if (root.style().textIndentType() == TextIndentType::Hanging)
        shouldIndent = !shouldIndent;

    if (!shouldIndent)
        return { };

    auto textIndent = root.style().textIndent();
    if (textIndent == RenderStyle::initialTextIndent())
        return { };
    if (isIntrinsicWidthMode == IsIntrinsicWidthMode::Yes && textIndent.isPercent()) {
        // Percentages must be treated as 0 for the purpose of calculating intrinsic size contributions.
        // https://drafts.csswg.org/css-text/#text-indent-property
        return { };
    }
    return { minimumValueForLength(textIndent, availableWidth) };
}

static std::optional<size_t> firstDisplayBoxIndexForLayoutBox(const Box& layoutBox, const InlineDisplay::Boxes& displayBoxes)
{
    // FIXME: Build a first/last hashmap for these boxes.
    for (size_t index = 0; index < displayBoxes.size(); ++index) {
        if (displayBoxes[index].isRootInlineBox())
            continue;
        if (&displayBoxes[index].layoutBox() == &layoutBox)
            return index;
    }
    return { };
}

static std::optional<size_t> lastDisplayBoxIndexForLayoutBox(const Box& layoutBox, const InlineDisplay::Boxes& displayBoxes)
{
    // FIXME: Build a first/last hashmap for these boxes.
    for (auto index = displayBoxes.size(); index--;) {
        if (displayBoxes[index].isRootInlineBox())
            continue;
        if (&displayBoxes[index].layoutBox() == &layoutBox)
            return index;
    }
    return { };
}

static std::optional<size_t> previousDisplayBoxIndex(const Box& outOfFlowBox, const InlineDisplay::Boxes& displayBoxes)
{
    auto previousDisplayBoxIndexOf = [&] (auto& layoutBox) -> std::optional<size_t> {
        for (auto* box = &layoutBox; box; box = box->previousInFlowSibling()) {
            if (auto displayBoxIndex = lastDisplayBoxIndexForLayoutBox(*box, displayBoxes))
                return displayBoxIndex;
        }
        return { };
    };

    auto* candidateBox = outOfFlowBox.previousInFlowSibling();
    if (!candidateBox)
        candidateBox = outOfFlowBox.parent().isInlineBox() ? &outOfFlowBox.parent() : nullptr;
    while (candidateBox) {
        if (auto displayBoxIndex = previousDisplayBoxIndexOf(*candidateBox))
            return displayBoxIndex;
        candidateBox = candidateBox->parent().isInlineBox() ? &candidateBox->parent() : nullptr;
    }
    return { };
}

static std::optional<size_t> nextDisplayBoxIndex(const Box& outOfFlowBox, const InlineDisplay::Boxes& displayBoxes)
{
    auto nextDisplayBoxIndexOf = [&] (auto& layoutBox) -> std::optional<size_t> {
        for (auto* box = &layoutBox; box; box = box->nextInFlowSibling()) {
            if (auto displayBoxIndex = firstDisplayBoxIndexForLayoutBox(*box, displayBoxes))
                return displayBoxIndex;
        }
        return { };
    };

    auto* candidateBox = outOfFlowBox.nextInFlowSibling();
    if (!candidateBox)
        candidateBox = outOfFlowBox.parent().isInlineBox() ? outOfFlowBox.parent().nextInFlowSibling() : nullptr;
    while (candidateBox) {
        if (auto displayBoxIndex = nextDisplayBoxIndexOf(*candidateBox))
            return displayBoxIndex;
        candidateBox = candidateBox->parent().isInlineBox() ? candidateBox->parent().nextInFlowSibling() : nullptr;
    }
    return { };
}

InlineLayoutUnit InlineFormattingGeometry::contentLeftAfterLastLine(const ConstraintsForInFlowContent& constraints, std::optional<InlineLayoutUnit> lastLineLogicalBottom, const FloatingContext& floatingContext) const
{
    auto contentHasPreviousLine = lastLineLogicalBottom ? std::make_optional(true) : std::nullopt;
    auto textIndent = computedTextIndent(IsIntrinsicWidthMode::No, contentHasPreviousLine, constraints.horizontal().logicalWidth);
    auto floatConstraints = floatConstraintsForLine(lastLineLogicalBottom.value_or(constraints.logicalTop()), 0, floatingContext);
    auto lineBoxLeft = constraints.horizontal().logicalLeft;
    auto lineBoxWidth = constraints.horizontal().logicalWidth;
    // FIXME: Add missing RTL support.
    if (floatConstraints.left) {
        auto floatOffset = std::max(0_lu, floatConstraints.left->x - constraints.horizontal().logicalLeft);
        lineBoxLeft += floatOffset;
        lineBoxWidth -= floatOffset;
    }
    if (floatConstraints.right) {
        auto lineBoxRight = (constraints.horizontal().logicalLeft + constraints.horizontal().logicalWidth);
        auto floatOffset = std::max(0_lu, lineBoxRight - floatConstraints.right->x);
        lineBoxWidth -= floatOffset;
    }
    lineBoxLeft += textIndent;
    auto rootInlineBoxLeft = horizontalAlignmentOffset(formattingContext().root().style(), { }, lineBoxWidth, { }, { }, true);
    return lineBoxLeft + rootInlineBoxLeft;
}

LayoutPoint InlineFormattingGeometry::staticPositionForOutOfFlowInlineLevelBox(const Box& outOfFlowBox, const ConstraintsForInFlowContent& constraints, const InlineDisplay::Content& displayContent, const FloatingContext& floatingContext) const
{
    ASSERT(outOfFlowBox.style().isOriginalDisplayInlineType());
    auto& lines = displayContent.lines;
    auto& boxes = displayContent.boxes;

    if (lines.isEmpty()) {
        ASSERT(boxes.isEmpty());
        return { contentLeftAfterLastLine(constraints, { }, floatingContext), constraints.logicalTop() };
    }

    auto isHorizontalWritingMode = formattingContext().root().style().isHorizontalWritingMode();
    auto leftSideToLogicalTopLeft = [&] (auto& displayBox, auto& line, bool mayNeedMarginAdjustment = true) {
        auto marginStart = LayoutUnit { };
        if (mayNeedMarginAdjustment && displayBox.isNonRootInlineLevelBox())
            marginStart = formattingContext().geometryForBox(displayBox.layoutBox()).marginStart();
        return isHorizontalWritingMode ? LayoutPoint(displayBox.left() - marginStart, line.top()) : LayoutPoint(displayBox.top() - marginStart, line.left());
    };
    auto rightSideToLogicalTopLeft = [&] (auto& displayBox, auto& line) {
        auto marginEnd = LayoutUnit { };
        if (displayBox.isNonRootInlineLevelBox())
            marginEnd = formattingContext().geometryForBox(displayBox.layoutBox()).marginEnd();
        return isHorizontalWritingMode ? LayoutPoint(displayBox.right() + marginEnd, line.top()) : LayoutPoint(displayBox.bottom() + marginEnd, line.left());
    };

    auto previousDisplayBoxIndexBeforeOutOfFlowBox = previousDisplayBoxIndex(outOfFlowBox, boxes);
    if (!previousDisplayBoxIndexBeforeOutOfFlowBox)
        return leftSideToLogicalTopLeft(boxes[0], lines[0]);

    auto& previousDisplayBox = boxes[*previousDisplayBoxIndexBeforeOutOfFlowBox];
    auto& currentLine = lines[previousDisplayBox.lineIndex()];
    if (previousDisplayBox.isInlineBox() && &outOfFlowBox.parent() == &previousDisplayBox.layoutBox()) {
        // Special handling for cases when this is the first box inside an inline box:
        // <div>text<span><img style="position: absolute">content</span></div>
        auto& inlineBox = previousDisplayBox.layoutBox();
        auto inlineBoxDisplayBox = boxes[*firstDisplayBoxIndexForLayoutBox(inlineBox, boxes)];
        auto inlineContentBoxOffset = formattingContext().geometryForBox(inlineBox).borderAndPaddingStart();
        if (isHorizontalWritingMode)
            inlineBoxDisplayBox.moveHorizontally(inlineContentBoxOffset);
        else
            inlineBoxDisplayBox.moveVertically(inlineContentBoxOffset);
        return leftSideToLogicalTopLeft(inlineBoxDisplayBox, lines[inlineBoxDisplayBox.lineIndex()], false);
    }

    auto previousBoxOverflows = (isHorizontalWritingMode ? previousDisplayBox.right() > currentLine.right() : previousDisplayBox.bottom() > currentLine.bottom()) || previousDisplayBox.isLineBreakBox();
    if (!previousBoxOverflows)
        return rightSideToLogicalTopLeft(previousDisplayBox, currentLine);

    auto nextDisplayBoxIndexAfterOutOfFlow = nextDisplayBoxIndex(outOfFlowBox, boxes);
    if (!nextDisplayBoxIndexAfterOutOfFlow) {
        // This is the last content on the block and it does not fit the last line.
        return { contentLeftAfterLastLine(constraints, currentLine.bottom(), floatingContext), isHorizontalWritingMode ? currentLine.bottom() : currentLine.right() };
    }
    auto& nextDisplayBox = boxes[*nextDisplayBoxIndexAfterOutOfFlow];
    return leftSideToLogicalTopLeft(nextDisplayBox, lines[nextDisplayBox.lineIndex()]);
}

LayoutPoint InlineFormattingGeometry::staticPositionForOutOfFlowBlockLevelBox(const Box& outOfFlowBox, const ConstraintsForInFlowContent& constraints, const InlineDisplay::Content& displayContent) const
{
    ASSERT(outOfFlowBox.style().isDisplayBlockLevel());

    auto isHorizontalWritingMode = formattingContext().root().style().isHorizontalWritingMode();
    auto& lines = displayContent.lines;
    auto& boxes = displayContent.boxes;
    auto contentBoxTopLeft = LayoutPoint { constraints.horizontal().logicalLeft, constraints.logicalTop() };

    if (lines.isEmpty()) {
        ASSERT(boxes.isEmpty());
        return contentBoxTopLeft;
    }

    // Block level boxes are placed under the current line as if they were normal inflow block level boxes.
    auto previousDisplayBoxIndexBeforeOutOfFlowBox = previousDisplayBoxIndex(outOfFlowBox, boxes);
    if (!previousDisplayBoxIndexBeforeOutOfFlowBox)
        return { contentBoxTopLeft.x(), isHorizontalWritingMode ? lines[0].top() : lines[0].left() };
    auto& currentLine = lines[boxes[*previousDisplayBoxIndexBeforeOutOfFlowBox].lineIndex()];
    return { contentBoxTopLeft.x(), LayoutUnit { isHorizontalWritingMode ? currentLine.bottom() : currentLine.right() } };
}

InlineLayoutUnit InlineFormattingGeometry::initialLineHeight(bool isFirstLine) const
{
    if (layoutState().inStandardsMode())
        return isFirstLine ? formattingContext().root().firstLineStyle().computedLineHeight() : formattingContext().root().style().computedLineHeight();
    return formattingContext().formattingQuirks().initialLineHeight();
}

FloatingContext::Constraints InlineFormattingGeometry::floatConstraintsForLine(InlineLayoutUnit lineLogicalTop, InlineLayoutUnit contentLogicalHeight, const FloatingContext& floatingContext) const
{
    auto logicalTopCandidate = LayoutUnit { lineLogicalTop };
    auto logicalBottomCandidate = LayoutUnit { lineLogicalTop + contentLogicalHeight };
    if (logicalTopCandidate.mightBeSaturated() || logicalBottomCandidate.mightBeSaturated())
        return { };
    // Check for intruding floats and adjust logical left/available width for this line accordingly.
    return floatingContext.constraints(logicalTopCandidate, logicalBottomCandidate, FloatingContext::MayBeAboveLastFloat::Yes);
}

InlineLayoutUnit InlineFormattingGeometry::horizontalAlignmentOffset(const RenderStyle& rootStyle, InlineLayoutUnit contentLogicalRight, InlineLayoutUnit lineLogicalWidth, InlineLayoutUnit hangingTrailingWidth, const Line::RunList& runs, bool isLastLine, std::optional<TextDirection> inlineBaseDirectionOverride)
{
    // Depending on the line’s alignment/justification, the hanging glyph can be placed outside the line box.
    if (hangingTrailingWidth) {
        // If white-space is set to pre-wrap, the UA must (unconditionally) hang this sequence, unless the sequence is followed
        // by a forced line break, in which case it must conditionally hang the sequence is instead.
        // Note that end of last line in a paragraph is considered a forced break.
        auto isConditionalHanging = runs.last().isLineBreak() || isLastLine;
        // In some cases, a glyph at the end of a line can conditionally hang: it hangs only if it does not otherwise fit in the line prior to justification.
        if (isConditionalHanging) {
            // FIXME: Conditional hanging needs partial overflow trimming at glyph boundary, one by one until they fit.
            contentLogicalRight = std::min(contentLogicalRight, lineLogicalWidth);
        } else
            contentLogicalRight -= hangingTrailingWidth;
    }

    auto isLastLineOrAfterLineBreak = isLastLine || (!runs.isEmpty() && runs.last().isLineBreak());
    auto horizontalAvailableSpace = lineLogicalWidth - contentLogicalRight;

    if (horizontalAvailableSpace <= 0)
        return { };

    auto isLeftToRightDirection = inlineBaseDirectionOverride.value_or(rootStyle.direction()) == TextDirection::LTR;

    auto computedHorizontalAlignment = [&] {
        auto textAlign = rootStyle.textAlign();
        if (!isLastLineOrAfterLineBreak)
            return textAlign;
        // The last line before a forced break or the end of the block is aligned according to text-align-last.
        switch (rootStyle.textAlignLast()) {
        case TextAlignLast::Auto:
            if (textAlign == TextAlignMode::Justify)
                return TextAlignMode::Start;
            return textAlign;
        case TextAlignLast::Start:
            return TextAlignMode::Start;
        case TextAlignLast::End:
            return TextAlignMode::End;
        case TextAlignLast::Left:
            return TextAlignMode::Left;
        case TextAlignLast::Right:
            return TextAlignMode::Right;
        case TextAlignLast::Center:
            return TextAlignMode::Center;
        case TextAlignLast::Justify:
            return TextAlignMode::Justify;
        default:
            ASSERT_NOT_REACHED();
            return TextAlignMode::Start;
        }
    };

    switch (computedHorizontalAlignment()) {
    case TextAlignMode::Left:
    case TextAlignMode::WebKitLeft:
        if (!isLeftToRightDirection)
            return horizontalAvailableSpace;
        FALLTHROUGH;
    case TextAlignMode::Start:
        return { };
    case TextAlignMode::Right:
    case TextAlignMode::WebKitRight:
        if (!isLeftToRightDirection)
            return { };
        FALLTHROUGH;
    case TextAlignMode::End:
        return horizontalAvailableSpace;
    case TextAlignMode::Center:
    case TextAlignMode::WebKitCenter:
        return horizontalAvailableSpace / 2;
    case TextAlignMode::Justify:
        // TextAlignMode::Justify is a run alignment (and we only do inline box alignment here)
        return { };
    default:
        ASSERT_NOT_IMPLEMENTED_YET();
        return { };
    }
    ASSERT_NOT_REACHED();
    return { };
}

InlineItemPosition InlineFormattingGeometry::leadingInlineItemPositionForNextLine(InlineItemPosition lineContentEnd, std::optional<InlineItemPosition> previousLineTrailingInlineItemPosition, InlineItemPosition layoutRangeEnd)
{
    if (!previousLineTrailingInlineItemPosition)
        return lineContentEnd;
    if (previousLineTrailingInlineItemPosition->index < lineContentEnd.index || (previousLineTrailingInlineItemPosition->index == lineContentEnd.index && previousLineTrailingInlineItemPosition->offset < lineContentEnd.offset)) {
        // Either full or partial advancing.
        return lineContentEnd;
    }
    if (previousLineTrailingInlineItemPosition->index == lineContentEnd.index && !previousLineTrailingInlineItemPosition->offset && !lineContentEnd.offset) {
        // Can't mangage to put any content on line (most likely due to floats). Note that this only applies to full content.
        return lineContentEnd;
    }
    // This looks like a partial content and we are stuck. Let's force-move over to the next inline item.
    // We certainly lose some content, but we would be busy looping otherwise.
    ASSERT_NOT_REACHED();
    return { std::min(lineContentEnd.index + 1, layoutRangeEnd.index), { } };
}

InlineLayoutUnit InlineFormattingGeometry::inlineItemWidth(const InlineItem& inlineItem, InlineLayoutUnit contentLogicalLeft, bool useFirstLineStyle) const
{
    ASSERT(inlineItem.layoutBox().isInlineLevelBox());
    if (is<InlineTextItem>(inlineItem)) {
        auto& inlineTextItem = downcast<InlineTextItem>(inlineItem);
        if (auto contentWidth = inlineTextItem.width())
            return *contentWidth;
        auto& fontCascade = useFirstLineStyle ? inlineTextItem.firstLineStyle().fontCascade() : inlineTextItem.style().fontCascade();
        if (!inlineTextItem.isWhitespace() || InlineTextItem::shouldPreserveSpacesAndTabs(inlineTextItem))
            return TextUtil::width(inlineTextItem, fontCascade, contentLogicalLeft);
        return TextUtil::width(inlineTextItem, fontCascade, inlineTextItem.start(), inlineTextItem.start() + 1, contentLogicalLeft);
    }

    if (inlineItem.isLineBreak() || inlineItem.isWordBreakOpportunity())
        return { };

    auto& layoutBox = inlineItem.layoutBox();
    auto& boxGeometry = formattingContext().geometryForBox(layoutBox);

    if (layoutBox.isReplacedBox())
        return boxGeometry.marginBoxWidth();

    if (inlineItem.isInlineBoxStart()) {
        auto logicalWidth = boxGeometry.marginStart() + boxGeometry.borderStart() + boxGeometry.paddingStart().value_or(0);
#if ENABLE(CSS_BOX_DECORATION_BREAK)
        auto& style = useFirstLineStyle ? inlineItem.firstLineStyle() : inlineItem.style();
        if (style.boxDecorationBreak() == BoxDecorationBreak::Clone)
            logicalWidth += boxGeometry.borderEnd() + boxGeometry.paddingEnd().value_or(0_lu);
#endif
        return logicalWidth;
    }

    if (inlineItem.isInlineBoxEnd())
        return boxGeometry.marginEnd() + boxGeometry.borderEnd() + boxGeometry.paddingEnd().value_or(0);

    // FIXME: The overhang should be computed to not overlap the neighboring runs or overflow the line.
    if (auto* rubyAdjustments = layoutBox.rubyAdjustments()) {
        auto& overhang = useFirstLineStyle ? rubyAdjustments->firstLineOverhang : rubyAdjustments->overhang;
        return boxGeometry.marginBoxWidth() - (overhang.start + overhang.end);
    }

    // Non-replaced inline box (e.g. inline-block)
    return boxGeometry.marginBoxWidth();
}

static inline bool endsWithSoftWrapOpportunity(const InlineTextItem& currentTextItem, const InlineTextItem& nextInlineTextItem)
{
    ASSERT(!nextInlineTextItem.isWhitespace());
    // We are at the position after a whitespace.
    if (currentTextItem.isWhitespace())
        return true;
    // When both these non-whitespace runs belong to the same layout box with the same bidi level, it's guaranteed that
    // they are split at a soft breaking opportunity. See InlineItemsBuilder::moveToNextBreakablePosition.
    if (&currentTextItem.inlineTextBox() == &nextInlineTextItem.inlineTextBox()) {
        if (currentTextItem.bidiLevel() == nextInlineTextItem.bidiLevel())
            return true;
        // The bidi boundary may or may not be the reason for splitting the inline text box content.
        // FIXME: We could add a "reason flag" to InlineTextItem to tell why the split happened.
        auto& style = currentTextItem.style();
        auto lineBreakIteratorFactory = CachedLineBreakIteratorFactory { currentTextItem.inlineTextBox().content(), style.computedLocale(), TextUtil::lineBreakIteratorMode(style.lineBreak()), TextUtil::contentAnalysis(style.wordBreak()) };
        auto softWrapOpportunityCandidate = nextInlineTextItem.start();
        return TextUtil::findNextBreakablePosition(lineBreakIteratorFactory, softWrapOpportunityCandidate, style) == softWrapOpportunityCandidate;
    }
    // Now we need to collect at least 3 adjacent characters to be able to make a decision whether the previous text item ends with breaking opportunity.
    // [ex-][ample] <- second to last[x] last[-] current[a]
    // We need at least 1 character in the current inline text item and 2 more from previous inline items.
    auto previousContent = currentTextItem.inlineTextBox().content();
    auto currentContent = nextInlineTextItem.inlineTextBox().content();
    if (!previousContent.is8Bit()) {
        // FIXME: Remove this workaround when we move over to a better way of handling prior-context with Unicode.
        // See the templated CharacterType in nextBreakablePosition for last and lastlast characters.
        currentContent.convertTo16Bit();
    }
    auto& style = nextInlineTextItem.style();
    auto lineBreakIteratorFactory = CachedLineBreakIteratorFactory { currentContent, style.computedLocale(), TextUtil::lineBreakIteratorMode(style.lineBreak()), TextUtil::contentAnalysis(style.wordBreak()) };
    auto previousContentLength = previousContent.length();
    // FIXME: We should look into the entire uncommitted content for more text context.
    UChar lastCharacter = previousContentLength ? previousContent[previousContentLength - 1] : 0;
    if (lastCharacter == softHyphen && currentTextItem.style().hyphens() == Hyphens::None)
        return false;
    UChar secondToLastCharacter = previousContentLength > 1 ? previousContent[previousContentLength - 2] : 0;
    lineBreakIteratorFactory.priorContext().set({ secondToLastCharacter, lastCharacter });
    // Now check if we can break right at the inline item boundary.
    // With the [ex-ample], findNextBreakablePosition should return the startPosition (0).
    // FIXME: Check if there's a more correct way of finding breaking opportunities.
    return !TextUtil::findNextBreakablePosition(lineBreakIteratorFactory, 0, style);
}

static inline bool isAtSoftWrapOpportunity(const InlineItem& current, const InlineItem& next)
{
    // FIXME: Transition no-wrapping logic from InlineContentBreaker to here where we compute the soft wrap opportunity indexes.
    // "is at" simple means that there's a soft wrap opportunity right after the [current].
    // [text][ ][text][inline box start]... (<div>text content<span>..</div>)
    // soft wrap indexes: 0 and 1 definitely, 2 depends on the content after the [inline box start].

    // https://drafts.csswg.org/css-text-3/#line-break-details
    // Figure out if the new incoming content puts the uncommitted content on a soft wrap opportunity.
    // e.g. [inline box start][prior_continuous_content][inline box end] (<span>prior_continuous_content</span>)
    // An incoming <img> box would enable us to commit the "<span>prior_continuous_content</span>" content
    // but an incoming text content would not necessarily.
    ASSERT(current.isText() || current.isBox());
    ASSERT(next.isText() || next.isBox());
    if (current.isText() && next.isText()) {
        auto& currentInlineTextItem = downcast<InlineTextItem>(current);
        auto& nextInlineTextItem = downcast<InlineTextItem>(next);
        if (currentInlineTextItem.isWhitespace() && nextInlineTextItem.isWhitespace()) {
            // <span> </span><span> </span>. Depending on the styles, there may or may not be a soft wrap opportunity between these 2 whitespace content.
            return TextUtil::isWrappingAllowed(currentInlineTextItem.style()) || TextUtil::isWrappingAllowed(nextInlineTextItem.style());
        }
        if (currentInlineTextItem.isWhitespace()) {
            // " <span>text</span>" : after [whitespace] position is a soft wrap opportunity.
            return TextUtil::isWrappingAllowed(currentInlineTextItem.style());
        }
        if (nextInlineTextItem.isWhitespace()) {
            // "<span>text</span> "
            // 'white-space: break-spaces' and '-webkit-line-break: after-white-space': line breaking opportunity exists after every preserved white space character, but not before.
            auto& style = nextInlineTextItem.style();
            return TextUtil::isWrappingAllowed(style) && style.whiteSpaceCollapse() != WhiteSpaceCollapse::BreakSpaces && style.lineBreak() != LineBreak::AfterWhiteSpace;
        }
        if (current.style().lineBreak() == LineBreak::Anywhere || next.style().lineBreak() == LineBreak::Anywhere) {
            // There is a soft wrap opportunity around every typographic character unit, including around any punctuation character
            // or preserved white spaces, or in the middle of words.
            return true;
        }
        // Both current and next items are non-whitespace text.
        // [text][text] : is a continuous content.
        // [text-][text] : after [hyphen] position is a soft wrap opportunity.
        return endsWithSoftWrapOpportunity(currentInlineTextItem, nextInlineTextItem);
    }
    if (current.layoutBox().isListMarkerBox() || next.layoutBox().isListMarkerBox())
        return true;
    if (current.isBox() || next.isBox()) {
        // [text][inline box start][inline box end][inline box] (text<span></span><img>) : there's a soft wrap opportunity between the [text] and [img].
        // The line breaking behavior of a replaced element or other atomic inline is equivalent to an ideographic character.
        return true;
    }
    ASSERT_NOT_REACHED();
    return true;
}

size_t InlineFormattingGeometry::nextWrapOpportunity(size_t startIndex, const InlineItemRange& layoutRange, const InlineItems& inlineItems)
{
    // 1. Find the start candidate by skipping leading non-content items e.g "<span><span>start". Opportunity is after "<span><span>".
    // 2. Find the end candidate by skipping non-content items inbetween e.g. "<span><span>start</span>end". Opportunity is after "</span>".
    // 3. Check if there's a soft wrap opportunity between the 2 candidate inline items and repeat.
    // 4. Any force line break/explicit wrap content inbetween is considered as wrap opportunity.

    // [ex-][inline box start][inline box end][float][ample] (ex-<span></span><div style="float:left"></div>ample). Wrap index is at [ex-].
    // [ex][inline box start][amp-][inline box start][le] (ex<span>amp-<span>ample). Wrap index is at [amp-].
    // [ex-][inline box start][line break][ample] (ex-<span><br>ample). Wrap index is after [br].
    auto previousInlineItemIndex = std::optional<size_t> { };
    for (auto index = startIndex; index < layoutRange.endIndex(); ++index) {
        auto& inlineItem = inlineItems[index];
        if (inlineItem.isLineBreak() || inlineItem.isWordBreakOpportunity()) {
            // We always stop at explicit wrapping opportunities e.g. <br>. However the wrap position may be at later position.
            // e.g. <span><span><br></span></span> <- wrap position is after the second </span>
            // but in case of <span><br><span></span></span> <- wrap position is right after <br>.
            for (++index; index < layoutRange.endIndex() && inlineItems[index].isInlineBoxEnd(); ++index) { }
            return index;
        }
        if (inlineItem.isInlineBoxStart() || inlineItem.isInlineBoxEnd()) {
            // Need to see what comes next to decide.
            continue;
        }
        ASSERT(inlineItem.isText() || inlineItem.isBox() || inlineItem.isFloat());
        if (inlineItem.isFloat()) {
            // While floats are not part of the inline content and they are not supposed to introduce soft wrap opportunities,
            // e.g. [text][float box][float box][text][float box][text] is essentially just [text][text][text]
            // figuring out whether a float (or set of floats) should stay on the line or not (and handle potentially out of order inline items)
            // brings in unnecessary complexity.
            // For now let's always treat a float as a soft wrap opportunity.
            auto wrappingPosition = index == startIndex ? std::min(index + 1, layoutRange.endIndex()) : index;
            return wrappingPosition;
        }
        if (!previousInlineItemIndex) {
            previousInlineItemIndex = index;
            continue;
        }
        // At this point previous and current items are not necessarily adjacent items e.g "previous<span>current</span>"
        auto& previousItem = inlineItems[*previousInlineItemIndex];
        auto& currentItem = inlineItems[index];
        if (isAtSoftWrapOpportunity(previousItem, currentItem)) {
            if (*previousInlineItemIndex + 1 == index && (!previousItem.isText() || !currentItem.isText())) {
                // We only know the exact soft wrap opportunity index when the previous and current items are next to each other.
                return index;
            }
            // There's a soft wrap opportunity between 'previousInlineItemIndex' and 'index'.
            // Now forward-find from the start position to see where we can actually wrap.
            // [ex-][ample] vs. [ex-][inline box start][inline box end][ample]
            // where [ex-] is previousInlineItemIndex and [ample] is index.

            // inline content and their inline boxes form unbreakable content.
            // ex-<span></span>ample               : wrap opportunity is after "ex-<span></span>".
            // ex-<span>ample                      : wrap opportunity is after "ex-".
            // ex-<span><span></span></span>ample  : wrap opportunity is after "ex-<span><span></span></span>".
            // ex-</span></span>ample              : wrap opportunity is after "ex-</span></span>".
            // ex-</span><span>ample               : wrap opportunity is after "ex-</span>".
            // ex-<span><span>ample                : wrap opportunity is after "ex-".
            struct InlineBoxPosition {
                const Box* inlineBox { nullptr };
                size_t index { 0 };
            };
            Vector<InlineBoxPosition> inlineBoxStack;
            auto start = *previousInlineItemIndex;
            auto end = index;
            // Soft wrap opportunity is at the first inline box that encloses the trailing content.
            for (auto candidateIndex = start + 1; candidateIndex < end; ++candidateIndex) {
                auto& inlineItem = inlineItems[candidateIndex];
                ASSERT(inlineItem.isInlineBoxStart() || inlineItem.isInlineBoxEnd());
                if (inlineItem.isInlineBoxStart())
                    inlineBoxStack.append({ &inlineItem.layoutBox(), candidateIndex });
                else if (inlineItem.isInlineBoxEnd() && !inlineBoxStack.isEmpty())
                    inlineBoxStack.removeLast();
            }
            return inlineBoxStack.isEmpty() ? index : inlineBoxStack.first().index;
        }
        previousInlineItemIndex = index;
    }
    return layoutRange.endIndex();
}


}
}

