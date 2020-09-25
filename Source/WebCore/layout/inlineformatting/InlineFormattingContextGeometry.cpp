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
#include "InlineFormattingContext.h"

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "FormattingContext.h"
#include "InlineLineBox.h"
#include "LayoutBox.h"
#include "LayoutContainerBox.h"
#include "LayoutReplacedBox.h"
#include "LengthFunctions.h"

namespace WebCore {
namespace Layout {

class LineBoxBuilder {
public:
    LineBoxBuilder(const InlineFormattingContext&);
    LineBox build(const LineBuilder::LineContent&);

private:
    void constructInlineBoxes(LineBox&, const Line::RunList&);
    void computeInlineBoxesLogicalHeight(LineBox&);
    void alignInlineBoxesVerticallyAndComputeLineBoxHeight(LineBox&);

    const InlineFormattingContext& formattingContext() const { return m_inlineFormattingContext; }
    const Box& rootBox() const { return formattingContext().root(); }
    LayoutState& layoutState() const { return formattingContext().layoutState(); }

private:
    const InlineFormattingContext& m_inlineFormattingContext;
};

struct HangingTrailingWhitespaceContent {
public:
    void reset();

    InlineLayoutUnit width() const { return m_width; }
    bool isConditional() const { return m_isConditional; }

    void setIsConditional() { m_isConditional = true; }
    void expand(InlineLayoutUnit width) { m_width += width; }

private:
    bool m_isConditional { false };
    InlineLayoutUnit m_width { 0 };
};

void HangingTrailingWhitespaceContent::reset()
{
    m_isConditional = false;
    m_width =  0;
}

static HangingTrailingWhitespaceContent collectHangingTrailingWhitespaceContent(const Line::RunList& runs, bool isLastLineWithInlineContent)
{
    auto hangingContent = HangingTrailingWhitespaceContent { };
    if (isLastLineWithInlineContent)
        hangingContent.setIsConditional();
    for (auto& run : WTF::makeReversedRange(runs)) {
        if (run.isContainerStart() || run.isContainerEnd())
            continue;
        if (run.isLineBreak()) {
            hangingContent.setIsConditional();
            continue;
        }
        if (!run.hasTrailingWhitespace())
            break;
        // Check if we have a preserved or hung whitespace.
        if (run.style().whiteSpace() != WhiteSpace::PreWrap)
            break;
        // This is either a normal or conditionally hanging trailing whitespace.
        hangingContent.expand(run.trailingWhitespaceWidth());
    }
    return hangingContent;
}

static Optional<InlineLayoutUnit> horizontalAlignmentOffset(const Line::RunList& runs, TextAlignMode textAlign, InlineLayoutUnit lineLogicalWidth, InlineLayoutUnit contentLogicalWidth, bool isLastLine)
{
    auto availableWidth = lineLogicalWidth - contentLogicalWidth;
    auto hangingTrailingWhitespaceContent = collectHangingTrailingWhitespaceContent(runs, isLastLine);
    availableWidth += hangingTrailingWhitespaceContent.width();
    if (availableWidth <= 0)
        return { };

    auto computedHorizontalAlignment = [&] {
        if (textAlign != TextAlignMode::Justify)
            return textAlign;
        // Text is justified according to the method specified by the text-justify property,
        // in order to exactly fill the line box. Unless otherwise specified by text-align-last,
        // the last line before a forced break or the end of the block is start-aligned.
        if (runs.last().isLineBreak() || isLastLine)
            return TextAlignMode::Start;
        return TextAlignMode::Justify;
    };

    switch (computedHorizontalAlignment()) {
    case TextAlignMode::Left:
    case TextAlignMode::WebKitLeft:
    case TextAlignMode::Start:
        return { };
    case TextAlignMode::Right:
    case TextAlignMode::WebKitRight:
    case TextAlignMode::End:
        return availableWidth;
    case TextAlignMode::Center:
    case TextAlignMode::WebKitCenter:
        return availableWidth / 2;
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

LineBoxBuilder::LineBoxBuilder(const InlineFormattingContext& inlineFormattingContext)
    : m_inlineFormattingContext(inlineFormattingContext)
{
}

LineBox LineBoxBuilder::build(const LineBuilder::LineContent& lineContent)
{
    auto& runs = lineContent.runs;
    auto lineLogicalWidth = lineContent.lineLogicalWidth;
    auto contentLogicalWidth = lineContent.lineContentLogicalWidth;
    auto isLineVisuallyEmpty = lineContent.isLineVisuallyEmpty ? LineBox::IsLineVisuallyEmpty::Yes : LineBox::IsLineVisuallyEmpty::No;
    auto lineBox = LineBox { contentLogicalWidth, isLineVisuallyEmpty };

    if (auto horizontalAlignmentOffset = Layout::horizontalAlignmentOffset(runs, rootBox().style().textAlign(), lineLogicalWidth, contentLogicalWidth, lineContent.isLastLineWithInlineContent))
        lineBox.setHorizontalAlignmentOffset(*horizontalAlignmentOffset);
    constructInlineBoxes(lineBox, runs);
    computeInlineBoxesLogicalHeight(lineBox);
    alignInlineBoxesVerticallyAndComputeLineBoxHeight(lineBox);
    return lineBox;
}

void LineBoxBuilder::constructInlineBoxes(LineBox& lineBox, const Line::RunList& runs)
{
    auto adjustVerticalGeometryForNonEmptyInlineBox = [](auto& inlineBox) {
        // Inline box vertical geometry is driven by either child inline boxes (see computeInlineBoxesLogicalHeight)
        // or the text runs' font metrics (text runs don't generate inline boxes).
        ASSERT(!inlineBox.isEmpty());
        auto& fontMetrics = inlineBox.fontMetrics();
        InlineLayoutUnit logicalHeight = fontMetrics.height();
        InlineLayoutUnit baseline = fontMetrics.ascent();

        inlineBox.setLogicalHeight(logicalHeight);
        inlineBox.setBaseline(baseline);
        inlineBox.setDescent(logicalHeight - baseline);
        if (auto lineSpacing = fontMetrics.lineSpacing() - logicalHeight)
            inlineBox.setLineSpacing(lineSpacing);
    };
    auto horizontalAligmentOffset = lineBox.horizontalAlignmentOffset().valueOr(InlineLayoutUnit { });
    auto constructRootInlineBox = [&] {
        auto rootInlineBox = LineBox::InlineBox::createBoxForRootInlineBox(rootBox(), horizontalAligmentOffset, lineBox.logicalWidth());

        auto lineHasImaginaryStrut = !layoutState().inQuirksMode();
        auto isInitiallyConsideredNonEmpty = !lineBox.isLineVisuallyEmpty() && lineHasImaginaryStrut;
        if (isInitiallyConsideredNonEmpty) {
            rootInlineBox->setIsNonEmpty();
            adjustVerticalGeometryForNonEmptyInlineBox(*rootInlineBox);
        }
        lineBox.addRootInlineBox(WTFMove(rootInlineBox));
    };
    constructRootInlineBox();
    if (!runs.isEmpty()) {
        // An inline box may not necessarily start on the current line:
        // <span id=outer>line break<br>this content's parent inline box('outer') <span id=inner>starts on the previous line</span></span>
        // We need to make sure that there's an LineBox::InlineBox for every inline box that's present on the current line.
        // In nesting case we need to create LineBox::InlineBoxes for the inline box ancestors.
        // We only have to do it on the first run as any subsequent inline content is either at the same/higher nesting level or
        // nested with a [container start] run.
        auto& firstRun = runs[0];
        auto& firstRunParentInlineBox = firstRun.layoutBox().parent();
        // If the parent is the formatting root, we can stop here. This is root inline box content, there's no nesting inline box from the previous line(s)
        // unless the inline box closing (container end run) is forced over to the current line.
        // e.g.
        // <span>normally the inline box closing forms a continuous content</span>
        // <span>unless it's forced to the next line<br></span>
        if (firstRun.isContainerEnd() || &firstRunParentInlineBox != &rootBox()) {
            auto* ancestor = &firstRunParentInlineBox;
            Vector<const Box*> ancestorsWithoutInlineBoxes;
            while (ancestor != &rootBox()) {
                ancestorsWithoutInlineBoxes.append(ancestor);
                ancestor = &ancestor->parent();
            }
            // Construct the missing LineBox::InlineBoxes starting with the topmost ancestor.
            for (auto* ancestor : WTF::makeReversedRange(ancestorsWithoutInlineBoxes)) {
                auto inlineBox = LineBox::InlineBox::createBoxForInlineBox(*ancestor, horizontalAligmentOffset, lineBox.logicalWidth());
                inlineBox->setIsNonEmpty();
                adjustVerticalGeometryForNonEmptyInlineBox(*inlineBox);
                lineBox.addInlineBox(WTFMove(inlineBox));
            }
        }
    }

    for (auto& run : runs) {
        auto& inlineLevelBox = run.layoutBox();
        if (run.isBox()) {
            auto logicalLeft = horizontalAligmentOffset + run.logicalLeft();
            auto& inlineLevelBoxGeometry = formattingContext().geometryForBox(inlineLevelBox);
            auto logicalHeight = inlineLevelBoxGeometry.marginBoxHeight();
            auto baseline = logicalHeight;
            if (inlineLevelBox.isInlineBlockBox() && inlineLevelBox.establishesInlineFormattingContext()) {
                // The inline-block's baseline offset is relative to its content box. Let's convert it relative to the margin box.
                //           _______________ <- margin box
                //          |
                //          |  ____________  <- border box
                //          | |
                //          | |  _________  <- content box
                //          | | |   ^
                //          | | |   |  <- baseline offset
                //          | | |   |
                //     text | | |   v text
                //     -----|-|-|---------- <- baseline
                //
                auto& formattingState = layoutState().establishedInlineFormattingState(downcast<ContainerBox>(inlineLevelBox));
                auto& lastLine = formattingState.lines().last();
                auto inlineBlockBaseline = lastLine.logicalTop() + lastLine.baseline();
                baseline = inlineLevelBoxGeometry.marginBefore() + inlineLevelBoxGeometry.borderTop() + inlineLevelBoxGeometry.paddingTop().valueOr(0) + inlineBlockBaseline;
            }
            auto inlineBox = LineBox::InlineBox::createBoxForAtomicInlineLevelBox(inlineLevelBox, logicalLeft, { run.logicalWidth(), logicalHeight }, baseline);
            if (logicalHeight)
                inlineBox->setIsNonEmpty();
            lineBox.addInlineBox(WTFMove(inlineBox));
        } else if (run.isContainerStart()) {
            auto logicalLeft = horizontalAligmentOffset + run.logicalLeft();
            auto initialLogicalWidth = lineBox.logicalWidth() - run.logicalLeft();
            ASSERT(initialLogicalWidth >= 0);
            lineBox.addInlineBox(LineBox::InlineBox::createBoxForInlineBox(inlineLevelBox, logicalLeft, initialLogicalWidth));
        } else if (run.isContainerEnd()) {
            // Adjust the logical width when the inline level container closes on this line.
            auto& inlineBox = lineBox.inlineBoxForLayoutBox(inlineLevelBox);
            inlineBox.setLogicalWidth(run.logicalRight() - inlineBox.logicalLeft());
        } else if (run.isText() || run.isLineBreak()) {
            auto& parentBox = inlineLevelBox.parent();
            auto& parentInlineBox = &parentBox == &rootBox() ? lineBox.rootInlineBox() : lineBox.inlineBoxForLayoutBox(parentBox);
            if (parentInlineBox.isEmpty()) {
                // FIXME: Adjust non-empty inline box height when glyphs from the non-primary font stretch the box.
                parentInlineBox.setIsNonEmpty();
                adjustVerticalGeometryForNonEmptyInlineBox(parentInlineBox);
            }
        }
    }
}

void LineBoxBuilder::computeInlineBoxesLogicalHeight(LineBox& lineBox)
{
    // By traversing the inline box list backwards, it's guaranteed that descendant inline boxes are sized first.
    for (auto& inlineBox : WTF::makeReversedRange(lineBox.nonRootInlineBoxes())) {
        if (inlineBox->isEmpty())
            continue;

        auto& layoutBox = inlineBox->layoutBox();
        switch (layoutBox.style().verticalAlign()) {
        case VerticalAlign::Top:
        case VerticalAlign::Bottom:
            // top and bottom alignments only stretch the line box. They don't stretch any of the inline boxes, not even the root inline box.
            break;
        case VerticalAlign::TextTop: {
            auto& parentInlineBox = lineBox.inlineBoxForLayoutBox(layoutBox.parent());
            auto parentTextLogicalTop = parentInlineBox.baseline() - parentInlineBox.fontMetrics().ascent();
            parentInlineBox.setLogicalHeight(std::max(parentInlineBox.logicalHeight(), parentTextLogicalTop + inlineBox->logicalHeight()));
            break;
        }
        case VerticalAlign::Baseline: {
            auto& parentInlineBox = lineBox.inlineBoxForLayoutBox(layoutBox.parent());
            auto baselineOverflow = std::max(0.0f, inlineBox->baseline() - parentInlineBox.baseline());
            if (baselineOverflow) {
                parentInlineBox.setBaseline(parentInlineBox.baseline() + baselineOverflow);
                parentInlineBox.setLogicalHeight(parentInlineBox.logicalHeight() + baselineOverflow);
            }
            auto parentLineBoxBelowBaseline = parentInlineBox.logicalHeight() - parentInlineBox.baseline();
            auto inlineBoxBelowBaseline = inlineBox->logicalHeight() - inlineBox->baseline();
            auto belowBaselineOverflow = std::max(0.0f, inlineBoxBelowBaseline - parentLineBoxBelowBaseline);
            if (belowBaselineOverflow)
                parentInlineBox.setLogicalHeight(parentInlineBox.logicalHeight() + belowBaselineOverflow);
            break;
        }
        case VerticalAlign::TextBottom: {
            auto& parentInlineBox = lineBox.inlineBoxForLayoutBox(layoutBox.parent());
            auto parentTextLogicalBottom = parentInlineBox.baseline() + parentInlineBox.descent().valueOr(InlineLayoutUnit { });
            auto overflow = std::max(0.0f, inlineBox->logicalHeight() - parentTextLogicalBottom);
            if (overflow) {
                // TextBottom pushes the baseline downward the same way 'bottom' does.
                parentInlineBox.setLogicalHeight(parentInlineBox.logicalHeight() + overflow);
                parentInlineBox.setBaseline(parentInlineBox.baseline() + overflow);
            }
            break;
        }
        case VerticalAlign::Middle: {
            auto& parentLayoutBox = layoutBox.parent();
            auto& parentInlineBox = lineBox.inlineBoxForLayoutBox(parentLayoutBox);
            auto logicalTop = parentInlineBox.baseline() - (inlineBox->logicalHeight() / 2 + parentInlineBox.fontMetrics().xHeight() / 2);
            if (logicalTop < 0) {
                auto overflow = -logicalTop;
                // Child inline box with middle alignment pushes the baseline down when overflows.
                parentInlineBox.setBaseline(parentInlineBox.baseline() + overflow);
                parentInlineBox.setLogicalHeight(parentInlineBox.logicalHeight() + overflow);
                logicalTop = { };
            }
            auto logicalBottom = logicalTop + inlineBox->logicalHeight();
            parentInlineBox.setLogicalHeight(std::max(parentInlineBox.logicalHeight(), logicalBottom));
            break;
        }
        default:
            ASSERT_NOT_IMPLEMENTED_YET();
            break;
        }
    }
}

void LineBoxBuilder::alignInlineBoxesVerticallyAndComputeLineBoxHeight(LineBox& lineBox)
{
    // Inline boxes are in the coordinate system of the line box (and not in the coordinate system of their parents).
    // Starting with the root inline box, position the ancestors first so that the descendant line boxes see absolute vertical positions.
    auto& rootInlineBox = lineBox.rootInlineBox();
    auto contentLogicalHeight = InlineLayoutUnit { };
    auto alignRootInlineBox = [&] {
        contentLogicalHeight = rootInlineBox.logicalHeight();
        for (auto& inlineBox : lineBox.nonRootInlineBoxes()) {
            auto verticalAlign = inlineBox->layoutBox().style().verticalAlign();
            if (verticalAlign == VerticalAlign::Bottom) {
                // bottom align always pushes the root inline box downwards.
                auto overflow = std::max(0.0f, inlineBox->logicalBottom() - rootInlineBox.logicalBottom());
                rootInlineBox.setLogicalTop(rootInlineBox.logicalTop() + overflow);
                contentLogicalHeight += overflow;
            } else if (verticalAlign == VerticalAlign::Top)
                contentLogicalHeight = std::max(contentLogicalHeight, inlineBox->logicalHeight());
        }
    };
    alignRootInlineBox();

    for (auto& inlineBox : lineBox.nonRootInlineBoxes()) {
        auto inlineBoxLogicalTop = InlineLayoutUnit { };
        auto& layoutBox = inlineBox->layoutBox();
        switch (layoutBox.style().verticalAlign()) {
        case VerticalAlign::Baseline: {
            auto& parentInlineBox = lineBox.inlineBoxForLayoutBox(layoutBox.parent());
            inlineBoxLogicalTop = parentInlineBox.logicalTop() + parentInlineBox.baseline() - inlineBox->baseline();
            break;
        }
        case VerticalAlign::TextTop: {
            auto& parentLayoutBox = layoutBox.parent();
            auto& parentInlineBox = lineBox.inlineBoxForLayoutBox(parentLayoutBox);
            inlineBoxLogicalTop = parentInlineBox.logicalTop() + parentInlineBox.baseline() - parentInlineBox.fontMetrics().ascent();
            break;
        }
        case VerticalAlign::TextBottom: {
            auto& parentLayoutBox = layoutBox.parent();
            auto& parentInlineBox = lineBox.inlineBoxForLayoutBox(parentLayoutBox);
            auto parentTextLogicalBottom = parentInlineBox.logicalTop() + parentInlineBox.baseline() + parentInlineBox.fontMetrics().descent();
            inlineBoxLogicalTop = parentTextLogicalBottom - inlineBox->logicalHeight();
            break;
        }
        case VerticalAlign::Top:
            inlineBoxLogicalTop = InlineLayoutUnit { };
            break;
        case VerticalAlign::Bottom:
            inlineBoxLogicalTop = contentLogicalHeight - inlineBox->logicalHeight();
            break;
        case VerticalAlign::Middle: {
            auto& parentLayoutBox = layoutBox.parent();
            auto& parentInlineBox = lineBox.inlineBoxForLayoutBox(parentLayoutBox);
            inlineBoxLogicalTop = parentInlineBox.logicalTop() + parentInlineBox.baseline() - (inlineBox->logicalHeight() / 2 + parentInlineBox.fontMetrics().xHeight() / 2);
            break;
        }
        default:
            ASSERT_NOT_IMPLEMENTED_YET();
            break;
        }
        inlineBox->setLogicalTop(inlineBoxLogicalTop);
    }
    if (!lineBox.isLineVisuallyEmpty())
        lineBox.setLogicalHeight(contentLogicalHeight);
}

LineBox InlineFormattingContext::Geometry::lineBoxForLineContent(const LineBuilder::LineContent& lineContent)
{
    return LineBoxBuilder(formattingContext()).build(lineContent);
}

InlineFormattingContext::Geometry::LineRectAndLineBoxOffset InlineFormattingContext::Geometry::computedLineLogicalRect(const LineBox& lineBox, const RenderStyle& rootStyle, const LineBuilder::LineContent& lineContent) const
{
    // Compute the line height and the line box vertical offset.
    // The line height is either the line-height value (negative value means line height is not set) or the font metrics's line spacing/line box height.
    // The line box is then positioned using the half leading centering.
    //   ___________________________________________  line
    // |                    ^                       |
    // |                    | line spacing          |
    // |                    v                       |
    // | -------------------------------------------|---------  LineBox
    // ||    ^                                      |         |
    // ||    | line box height                      |         |
    // ||----v--------------------------------------|-------- | alignment baseline
    // | -------------------------------------------|---------
    // |                    ^                       |   ^
    // |                    | line spacing          |   |
    // |____________________v_______________________|  scrollable overflow
    //
    if (lineContent.runs.isEmpty() || lineBox.isLineVisuallyEmpty())
        return { { }, { lineContent.logicalTopLeft, lineContent.lineLogicalWidth, { } } };

    auto lineBoxLogicalHeight = lineBox.logicalHeight();
    auto lineLogicalHeight = InlineLayoutUnit { };
    if (rootStyle.lineHeight().isNegative()) {
        // Negative line height value means the line height is driven by the content.
        auto usedLineSpacing = [&] {
            auto logicalTopWithLineSpacing = InlineLayoutUnit { };
            auto logicalBottomWithLineSpacing = lineBoxLogicalHeight;
            for (auto& inlineBox : lineBox.inlineBoxList()) {
                if (auto lineSpacing = inlineBox->lineSpacing()) {
                    // FIXME: check if line spacing is distributed evenly.
                    logicalTopWithLineSpacing = std::min(logicalTopWithLineSpacing, inlineBox->logicalTop() - *lineSpacing / 2);
                    logicalBottomWithLineSpacing = std::max(logicalBottomWithLineSpacing, inlineBox->logicalBottom() + *lineSpacing / 2);
                }
            }
            return -logicalTopWithLineSpacing + (logicalBottomWithLineSpacing - lineBoxLogicalHeight);
        };
        lineLogicalHeight = lineBox.logicalHeight() + usedLineSpacing();
    } else
        lineLogicalHeight = rootStyle.computedLineHeight();

    auto logicalRect = InlineRect { lineContent.logicalTopLeft, lineContent.lineLogicalWidth, lineLogicalHeight};
    // Inline tree height is all integer based.
    auto lineBoxOffset = floorf((lineLogicalHeight - lineBoxLogicalHeight) / 2);
    return { lineBoxOffset, logicalRect };
}

ContentWidthAndMargin InlineFormattingContext::Geometry::inlineBlockWidthAndMargin(const Box& formattingContextRoot, const HorizontalConstraints& horizontalConstraints, const OverrideHorizontalValues& overrideHorizontalValues)
{
    ASSERT(formattingContextRoot.isInFlow());

    // 10.3.10 'Inline-block', replaced elements in normal flow

    // Exactly as inline replaced elements.
    if (formattingContextRoot.isReplacedBox())
        return inlineReplacedWidthAndMargin(downcast<ReplacedBox>(formattingContextRoot), horizontalConstraints, { }, overrideHorizontalValues);

    // 10.3.9 'Inline-block', non-replaced elements in normal flow

    // If 'width' is 'auto', the used value is the shrink-to-fit width as for floating elements.
    // A computed value of 'auto' for 'margin-left' or 'margin-right' becomes a used value of '0'.
    // #1
    auto width = computedValue(formattingContextRoot.style().logicalWidth(), horizontalConstraints.logicalWidth);
    if (!width)
        width = shrinkToFitWidth(formattingContextRoot, horizontalConstraints.logicalWidth);

    // #2
    auto computedHorizontalMargin = Geometry::computedHorizontalMargin(formattingContextRoot, horizontalConstraints);

    return ContentWidthAndMargin { *width, { computedHorizontalMargin.start.valueOr(0_lu), computedHorizontalMargin.end.valueOr(0_lu) } };
}

ContentHeightAndMargin InlineFormattingContext::Geometry::inlineBlockHeightAndMargin(const Box& layoutBox, const HorizontalConstraints& horizontalConstraints, const OverrideVerticalValues& overrideVerticalValues) const
{
    ASSERT(layoutBox.isInFlow());

    // 10.6.2 Inline replaced elements, block-level replaced elements in normal flow, 'inline-block' replaced elements in normal flow and floating replaced elements
    if (layoutBox.isReplacedBox())
        return inlineReplacedHeightAndMargin(downcast<ReplacedBox>(layoutBox), horizontalConstraints, { }, overrideVerticalValues);

    // 10.6.6 Complicated cases
    // - 'Inline-block', non-replaced elements.
    return complicatedCases(layoutBox, horizontalConstraints, overrideVerticalValues);
}

}
}

#endif
