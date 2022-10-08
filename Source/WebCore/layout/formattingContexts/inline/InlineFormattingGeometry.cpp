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

InlineLayoutUnit InlineFormattingGeometry::logicalTopForNextLine(const LineBuilder::LineContent& lineContent, const InlineRect& lineInitialRect, const InlineRect& lineLogicalRect, const FloatingContext& floatingContext) const
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
    // Move the next line below the intrusive float(s).
    ASSERT(lineContent.runs.isEmpty() || lineContent.runs[0].isLineSpanningInlineBoxStart());
    ASSERT(lineContent.hasIntrusiveFloat);
    auto lineBottomWithNoInlineContent = std::max(lineLogicalRect.bottom(), lineInitialRect.bottom());
    auto floatConstraints = floatingContext.constraints(toLayoutUnit(lineInitialRect.top()), toLayoutUnit(lineBottomWithNoInlineContent), FloatingContext::MayBeAboveLastFloat::Yes);
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
    return lineBottomWithNoInlineContent;
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

bool InlineFormattingGeometry::inlineLevelBoxAffectsLineBox(const InlineLevelBox& inlineLevelBox, const LineBox& lineBox) const
{
    if (inlineLevelBox.isInlineBox() || inlineLevelBox.isLineBreakBox())
        return layoutState().inStandardsMode() ? true : formattingContext().formattingQuirks().inlineLevelBoxAffectsLineBox(inlineLevelBox, lineBox);
    if (inlineLevelBox.isListMarker())
        return inlineLevelBox.layoutBounds().height();
    if (inlineLevelBox.isAtomicInlineLevelBox()) {
        if (inlineLevelBox.layoutBounds().height())
            return true;
        // While in practice when the negative vertical margin makes the layout bounds empty (e.g: height: 100px; margin-top: -100px;), and this inline
        // level box contributes 0px to the line box height, it still needs to be taken into account while computing line box geometries.
        auto& boxGeometry = formattingContext().geometryForBox(inlineLevelBox.layoutBox());
        return boxGeometry.marginBefore() || boxGeometry.marginAfter();
    }
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

static std::tuple<const InlineDisplay::Box*, const InlineDisplay::Box*> previousAndNextDisplayBoxForStaticPosition(const Box& outOfFlowBox, const DisplayBoxes& displayBoxes)
{
    // Both previous float and out-of-flow boxes are skipped here. A series of adjoining out-of-flow boxes should all be placed
    // at the same static position (they don't affect next-sibling positions) and while floats do participate in the inline layout
    // their positions have already been taken into account during the inline layout.
    auto previousInFlowBox = [&] () -> const Box* {
        if (auto* previousInFlowSibling = outOfFlowBox.previousInFlowSibling())
            return previousInFlowSibling;
        // Parent is either the root here or another inline box (e.g. <span><img style="position: absolute"></span>)
        auto& parent = outOfFlowBox.parent();
        return parent.isInlineBox() ? &parent : nullptr;
    }();

    if (!previousInFlowBox) {
        // This is the first content. It sits on the edge of the first root inline box. We don't need to provide the next display box.
        return { nullptr, nullptr };
    }

    // This is supposed to find the last display box of the "inflowBox" and return it with the next display box (first box of the next inflow content).
    auto foundFirstDisplayBox = false;
    for (size_t index = 0; index < displayBoxes.size(); ++index) {
        if (displayBoxes[index].isRootInlineBox())
            continue;
        if (foundFirstDisplayBox && &displayBoxes[index].layoutBox() != previousInFlowBox) {
            ASSERT(index);
            return { &displayBoxes[index - 1], &displayBoxes[index] };
        }
        foundFirstDisplayBox = foundFirstDisplayBox || &displayBoxes[index].layoutBox() == previousInFlowBox;
    }
    if (foundFirstDisplayBox)
        return { &displayBoxes[displayBoxes.size() - 1], nullptr };
    return { nullptr, nullptr };
}

LayoutPoint InlineFormattingGeometry::staticPositionForOutOfFlowInlineLevelBox(const Box& outOfFlowBox) const
{
    ASSERT(outOfFlowBox.style().isOriginalDisplayInlineType());

    auto& formattingState = formattingContext().formattingState();
    auto& lines = formattingState.lines();
    auto& boxes = formattingState.boxes();

    auto [previousDisplayBox, nextDisplayBox] = previousAndNextDisplayBoxForStaticPosition(outOfFlowBox, boxes);

    if (!previousDisplayBox && !nextDisplayBox)
        return { boxes[0].left(), lines[0].top() };

    if (!nextDisplayBox) {
        auto& currentLine = lines[previousDisplayBox->lineIndex()];
        auto shouldFitLine = previousDisplayBox->right() <= currentLine.right() && !previousDisplayBox->isLineBreakBox();
        return shouldFitLine ? LayoutPoint { previousDisplayBox->right(), currentLine.top() } : LayoutPoint { currentLine.left(), currentLine.bottom() };
    }

    if (previousDisplayBox->isInlineBox()) {
        // Special handling for cases when the previous content is an inline box:
        // <div>text<span><img style="position: absolute">content</span></div>
        // or
        // <div>text<span>content</span><img style="position: absolute"></div>
        auto isFirstContentInsideInlineBox = &outOfFlowBox.parent() == &previousDisplayBox->layoutBox();
        auto& inlineBoxBoxGeometry = formattingContext().geometryForBox(previousDisplayBox->layoutBox());

        return {
            isFirstContentInsideInlineBox ? BoxGeometry::borderBoxLeft(inlineBoxBoxGeometry) + inlineBoxBoxGeometry.contentBoxLeft() : BoxGeometry::marginBoxRect(inlineBoxBoxGeometry).right(),
            lines[previousDisplayBox->lineIndex()].top()
        };
    }
    auto& currentLine = lines[previousDisplayBox->lineIndex()];
    auto shouldFitLine = previousDisplayBox->lineIndex() == nextDisplayBox->lineIndex() || (previousDisplayBox->right() <= currentLine.left() && !previousDisplayBox->isLineBreakBox());
    return shouldFitLine ? LayoutPoint { previousDisplayBox->right(), currentLine.top() } : LayoutPoint { nextDisplayBox->left(), lines[nextDisplayBox->lineIndex()].top() };
}

LayoutPoint InlineFormattingGeometry::staticPositionForOutOfFlowBlockLevelBox(const Box& outOfFlowBox) const
{
    ASSERT(outOfFlowBox.style().isOriginalDisplayBlockType());

    auto& formattingState = formattingContext().formattingState();
    auto& lines = formattingState.lines();
    auto& boxes = formattingState.boxes();

    // Block level boxes are placed under the current line as if they were normal inflow block level boxes.
    const InlineDisplay::Box* previousDisplayBox = nullptr;
    std::tie(previousDisplayBox, std::ignore) = previousAndNextDisplayBoxForStaticPosition(outOfFlowBox, boxes);

    if (!previousDisplayBox)
        return { boxes[0].left(), lines[0].top() };

    auto& currentLine = lines[previousDisplayBox->lineIndex()];
    return { currentLine.left(), currentLine.bottom() };
}

InlineLayoutUnit InlineFormattingGeometry::initialLineHeight(bool isFirstLine) const
{
    if (layoutState().inStandardsMode())
        return isFirstLine ? formattingContext().root().firstLineStyle().computedLineHeight() : formattingContext().root().style().computedLineHeight();
    return formattingContext().formattingQuirks().initialLineHeight(isFirstLine);
}

std::optional<HorizontalConstraints> InlineFormattingGeometry::floatConstraintsForLine(const InlineRect& lineLogicalRect, const FloatingContext& floatingContext) const
{
    // Check for intruding floats and adjust logical left/available width for this line accordingly.
    auto toLogicalFloatPosition = [&] (const auto& constraints) {
        // FIXME: Move inline direction flip to FloatingContext.
        if (floatingContext.floatingState().isLeftToRightDirection() == formattingContext().root().style().isLeftToRightDirection())
            return constraints;
        auto logicalConstraints = FloatingContext::Constraints { };
        auto borderBoxWidth = layoutState().geometryForBox(formattingContext().root()).borderBoxWidth();
        if (constraints.left)
            logicalConstraints.right = PointInContextRoot { borderBoxWidth - constraints.left->x, constraints.left->y };
        if (constraints.right)
            logicalConstraints.left = PointInContextRoot { borderBoxWidth - constraints.right->x, constraints.right->y };
        return logicalConstraints;
    };
    auto constraints = toLogicalFloatPosition(floatingContext.constraints(toLayoutUnit(lineLogicalRect.top()), toLayoutUnit(lineLogicalRect.bottom()), FloatingContext::MayBeAboveLastFloat::Yes));

    // Check if these values actually constrain the line.
    if (constraints.left && constraints.left->x <= lineLogicalRect.left())
        constraints.left = { };

    if (constraints.right && constraints.right->x >= lineLogicalRect.right())
        constraints.right = { };

    if (!constraints.left && !constraints.right)
        return { };

    auto lineLogicalLeft = lineLogicalRect.left();
    auto lineLogicalRight = lineLogicalRect.right();
    if (constraints.left && constraints.right) {
        lineLogicalRight = constraints.right->x;
        lineLogicalLeft = constraints.left->x;
    } else if (constraints.left) {
        ASSERT(constraints.left->x >= lineLogicalLeft);
        lineLogicalLeft = constraints.left->x;
    } else if (constraints.right) {
        // Right float boxes may overflow the containing block on the left.
        lineLogicalRight = std::max<InlineLayoutUnit>(lineLogicalLeft, constraints.right->x);
    }
    return HorizontalConstraints { toLayoutUnit(lineLogicalLeft), toLayoutUnit(lineLogicalRight - lineLogicalLeft) };
}

}
}

