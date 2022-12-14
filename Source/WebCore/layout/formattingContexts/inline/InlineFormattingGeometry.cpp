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
#include "InlineFormattingContext.h"
#include "InlineFormattingQuirks.h"
#include "InlineLineBoxVerticalAligner.h"
#include "LayoutBox.h"
#include "LayoutElementBox.h"
#include "LengthFunctions.h"

namespace WebCore {
namespace Layout {

InlineFormattingGeometry::InlineFormattingGeometry(const InlineFormattingContext& inlineFormattingContext)
    : FormattingGeometry(inlineFormattingContext)
{
}

InlineLayoutUnit InlineFormattingGeometry::logicalTopForNextLine(const LineBuilder::LineContent& lineContent, const InlineRect& lineLogicalRect, const FloatingContext& floatingContext) const
{
    if (!lineContent.inlineItemRange.isEmpty()) {
        // Normally the next line's logical top is the previous line's logical bottom, but when the line ends
        // with the clear property set, the next line needs to clear the existing floats.
        if (lineContent.runs.isEmpty())
            return lineLogicalRect.bottom();
        auto& lastRunLayoutBox = lineContent.runs.last().layoutBox();
        if (!lastRunLayoutBox.hasFloatClear())
            return lineLogicalRect.bottom();
        auto positionWithClearance = floatingContext.verticalPositionWithClearance(lastRunLayoutBox);
        if (!positionWithClearance)
            return lineLogicalRect.bottom();
        return std::max(lineLogicalRect.bottom(), InlineLayoutUnit(positionWithClearance->position));
    }
    // Floats must have prevented us placing any content on the line.
    // Move the next line below the intrusive float(s).
    ASSERT(lineContent.runs.isEmpty() || lineContent.runs[0].isLineSpanningInlineBoxStart());
    ASSERT(lineContent.hasIntrusiveFloat);
    // FIXME: Moving the bottom position by the initial line height may be too much meaning that we miss vertical positions where atomic inline content could very well fit.
    // The spec is unclear of how much we should move down at this point and while 1px should be the most precise it's also rather expensive. 
    auto inflatedLineRectBottom = lineLogicalRect.top() + formattingContext().root().style().computedLineHeight();
    auto floatConstraints = floatingContext.constraints(toLayoutUnit(lineLogicalRect.top()), toLayoutUnit(inflatedLineRectBottom), FloatingContext::MayBeAboveLastFloat::Yes);
    ASSERT(floatConstraints.left || floatConstraints.right);
    if (floatConstraints.left && floatConstraints.right) {
        // In case of left and right constraints, we need to pick the one that's closer to the current line.
        return std::min(floatConstraints.left->y, floatConstraints.right->y);
    }
    if (floatConstraints.left)
        return floatConstraints.left->y;
    if (floatConstraints.right)
        return floatConstraints.right->y;
    ASSERT_NOT_REACHED();
    // Do not get stuck on the same vertical position.
    return lineLogicalRect.bottom() + 1.0f;
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
    if (!inlineLevelBox.lineBoxContain())
        return false;

    if (inlineLevelBox.isLineBreakBox())
        return false;
    if (inlineLevelBox.isListMarker())
        return downcast<ElementBox>(inlineLevelBox.layoutBox()).isListMarkerImage();
    if (inlineLevelBox.isInlineBox())
        return layoutState().inStandardsMode() ? true : formattingContext().formattingQuirks().inlineBoxAffectsLineBox(inlineLevelBox);
    if (inlineLevelBox.isAtomicInlineLevelBox())
        return true;
    return false;
}

InlineRect InlineFormattingGeometry::flipVisualRectToLogicalForWritingMode(const InlineRect& visualRect, WritingMode writingMode)
{
    switch (writingMode) {
    case WritingMode::TopToBottom:
        return visualRect;
    case WritingMode::LeftToRight:
    case WritingMode::RightToLeft: {
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

static std::optional<size_t> firstDisplayBoxIndexForLayoutBox(const Box& layoutBox, const DisplayBoxes& displayBoxes)
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

static std::optional<size_t> lastDisplayBoxIndexForLayoutBox(const Box& layoutBox, const DisplayBoxes& displayBoxes)
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

static std::optional<size_t> previousDisplayBoxIndex(const Box& outOfFlowBox, const DisplayBoxes& displayBoxes)
{
    auto previousDisplayBoxIndexOf = [&] (auto& layoutBox) -> std::optional<size_t> {
        for (auto* box = &layoutBox; box; box = box->previousInFlowSibling()) {
            if (auto displayBoxIndex = lastDisplayBoxIndexForLayoutBox(*box, displayBoxes))
                return displayBoxIndex;
        }
        return { };
    };

    auto* canidateBox = outOfFlowBox.previousInFlowSibling();
    if (!canidateBox)
        canidateBox = outOfFlowBox.parent().isInlineBox() ? &outOfFlowBox.parent() : nullptr;
    while (canidateBox) {
        if (auto displayBoxIndex = previousDisplayBoxIndexOf(*canidateBox))
            return displayBoxIndex;
        canidateBox = canidateBox->parent().isInlineBox() ? &canidateBox->parent() : nullptr;
    }
    return { };
}

static std::optional<size_t> nextDisplayBoxIndex(const Box& outOfFlowBox, const DisplayBoxes& displayBoxes)
{
    auto nextDisplayBoxIndexOf = [&] (auto& layoutBox) -> std::optional<size_t> {
        for (auto* box = &layoutBox; box; box = box->nextInFlowSibling()) {
            if (auto displayBoxIndex = firstDisplayBoxIndexForLayoutBox(*box, displayBoxes))
                return displayBoxIndex;
        }
        return { };
    };

    auto* canidateBox = outOfFlowBox.nextInFlowSibling();
    if (!canidateBox)
        canidateBox = outOfFlowBox.parent().isInlineBox() ? &outOfFlowBox.parent() : nullptr;
    while (canidateBox) {
        if (auto displayBoxIndex = nextDisplayBoxIndexOf(*canidateBox))
            return displayBoxIndex;
        canidateBox = canidateBox->parent().isInlineBox() ? &canidateBox->parent() : nullptr;
    }
    return { };
}

LayoutPoint InlineFormattingGeometry::staticPositionForOutOfFlowInlineLevelBox(const Box& outOfFlowBox, LayoutPoint contentBoxTopLeft) const
{
    ASSERT(outOfFlowBox.style().isOriginalDisplayInlineType());
    auto& formattingState = formattingContext().formattingState();
    auto& lines = formattingState.lines();
    auto& boxes = formattingState.boxes();

    if (lines.isEmpty()) {
        ASSERT(boxes.isEmpty());
        return contentBoxTopLeft;
    }

    auto isHorizontalWritingMode = formattingContext().root().style().isHorizontalWritingMode();
    auto leftSideToLogicalTopLeft = [&] (auto& displayBox, auto& line) {
        return isHorizontalWritingMode ? LayoutPoint(displayBox.left(), line.top()) : LayoutPoint(displayBox.top(), line.left());
    };
    auto rightSideToLogicalTopLeft = [&] (auto& displayBox, auto& line) {
        return isHorizontalWritingMode ? LayoutPoint(displayBox.right(), line.top()) : LayoutPoint(displayBox.bottom(), line.left());
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
        return leftSideToLogicalTopLeft(inlineBoxDisplayBox, lines[inlineBoxDisplayBox.lineIndex()]);
    }

    auto previousBoxOverflows = (isHorizontalWritingMode ? previousDisplayBox.right() > currentLine.right() : previousDisplayBox.bottom() > currentLine.bottom()) || previousDisplayBox.isLineBreakBox();
    if (!previousBoxOverflows)
        return rightSideToLogicalTopLeft(previousDisplayBox, currentLine);

    auto nextDisplayBoxIndexAfterOutOfFlow = nextDisplayBoxIndex(outOfFlowBox, boxes);
    if (!nextDisplayBoxIndexAfterOutOfFlow) {
        // This is the last content on the block and it does not fit the last line.
        // FIXME: This still has line type of constraints like text-align.
        return LayoutPoint { contentBoxTopLeft.x(), isHorizontalWritingMode ? currentLine.bottom() : currentLine.right() };
    }
    auto& nextDisplayBox = boxes[*nextDisplayBoxIndexAfterOutOfFlow];
    return leftSideToLogicalTopLeft(nextDisplayBox, lines[nextDisplayBox.lineIndex()]);
}

LayoutPoint InlineFormattingGeometry::staticPositionForOutOfFlowBlockLevelBox(const Box& outOfFlowBox, LayoutPoint contentBoxTopLeft) const
{
    ASSERT(outOfFlowBox.style().isDisplayBlockLevel());

    auto& formattingState = formattingContext().formattingState();
    auto isHorizontalWritingMode = formattingContext().root().style().isHorizontalWritingMode();
    auto& lines = formattingState.lines();
    auto& boxes = formattingState.boxes();

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
    // Check for intruding floats and adjust logical left/available width for this line accordingly.
    return floatingContext.constraints(toLayoutUnit(lineLogicalTop), toLayoutUnit(lineLogicalTop + contentLogicalHeight), FloatingContext::MayBeAboveLastFloat::Yes);
}

}
}

