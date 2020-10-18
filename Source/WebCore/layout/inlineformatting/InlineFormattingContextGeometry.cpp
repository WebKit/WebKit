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

#include "FloatingContext.h"
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
    void setVerticalGeometryForInlineBox(LineBox::InlineLevelBox&) const;
    void constructInlineLevelBoxes(LineBox&, const Line::RunList&);
    void alignInlineLevelBoxesVerticallyAndComputeLineBoxHeight(LineBox&);

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
    constructInlineLevelBoxes(lineBox, runs);
    alignInlineLevelBoxesVerticallyAndComputeLineBoxHeight(lineBox);
    return lineBox;
}

void LineBoxBuilder::setVerticalGeometryForInlineBox(LineBox::InlineLevelBox& inlineLevelBox) const
{
    ASSERT(inlineLevelBox.isInlineBox() || inlineLevelBox.isLineBreakBox());
    auto& fontMetrics = inlineLevelBox.fontMetrics();
    InlineLayoutUnit ascent = fontMetrics.ascent();
    InlineLayoutUnit descent = fontMetrics.descent();
    auto logicalHeight = ascent + descent;

    // FIXME: Adjust layout bounds with fallback font when applicable.
    auto& style = inlineLevelBox.layoutBox().style();
    auto lineHeight = style.lineHeight();
    if (lineHeight.isNegative()) {
        // If line-height computes to normal and either text-edge is leading or this is the root inline box,
        // the font’s line gap metric may also be incorporated into A and D by adding half to each side as half-leading.
        auto lineSpacing = &inlineLevelBox.layoutBox() == &rootBox() ? fontMetrics.lineSpacing() - logicalHeight : InlineLayoutUnit();
        ascent += lineSpacing / 2;
        descent += lineSpacing / 2;
    } else {
        InlineLayoutUnit lineHeight = style.computedLineHeight();
        InlineLayoutUnit halfLeading = (lineHeight - (ascent + descent)) / 2;
        ascent += halfLeading;
        descent += halfLeading;
    }
    // We need this to match legacy layout integral positioning.
    ascent = floorf(ascent);
    descent = ceil(descent);

    inlineLevelBox.setLayoutBounds(LineBox::InlineLevelBox::LayoutBounds { ascent, descent });
    inlineLevelBox.setBaseline(ascent);
    inlineLevelBox.setDescent(descent);
    inlineLevelBox.setLogicalHeight(logicalHeight);
}

void LineBoxBuilder::constructInlineLevelBoxes(LineBox& lineBox, const Line::RunList& runs)
{
    auto horizontalAligmentOffset = lineBox.horizontalAlignmentOffset().valueOr(InlineLayoutUnit { });

    auto createRootInlineBox = [&] {
        auto rootInlineBox = LineBox::InlineLevelBox::createRootInlineBox(rootBox(), horizontalAligmentOffset, lineBox.logicalWidth());

        auto lineHasImaginaryStrut = layoutState().inNoQuirksMode();
        auto isInitiallyConsideredNonEmpty = !lineBox.isLineVisuallyEmpty() && lineHasImaginaryStrut;
        if (isInitiallyConsideredNonEmpty)
            rootInlineBox->setIsNonEmpty();
        if (lineHasImaginaryStrut)
            setVerticalGeometryForInlineBox(*rootInlineBox);
        lineBox.addRootInlineBox(WTFMove(rootInlineBox));
    };
    createRootInlineBox();

    auto createWrappedInlineBoxes = [&] {
        if (runs.isEmpty())
            return;
        // An inline box may not necessarily start on the current line:
        // <span id=outer>line break<br>this content's parent inline box('outer') <span id=inner>starts on the previous line</span></span>
        // We need to make sure that there's an LineBox::InlineLevelBox for every inline box that's present on the current line.
        // In nesting case we need to create LineBox::InlineLevelBoxes for the inline box ancestors.
        // We only have to do it on the first run as any subsequent inline content is either at the same/higher nesting level or
        // nested with a [container start] run.
        auto& firstRun = runs[0];
        auto& firstRunParentInlineBox = firstRun.layoutBox().parent();
        // If the parent is the formatting root, we can stop here. This is root inline box content, there's no nesting inline box from the previous line(s)
        // unless the inline box closing (container end run) is forced over to the current line.
        // e.g.
        // <span>normally the inline box closing forms a continuous content</span>
        // <span>unless it's forced to the next line<br></span>
        if (&firstRunParentInlineBox == &rootBox() && !firstRun.isContainerEnd())
            return;
        auto* ancestor = &firstRunParentInlineBox;
        Vector<const Box*> ancestorsWithoutInlineBoxes;
        while (ancestor != &rootBox()) {
            ancestorsWithoutInlineBoxes.append(ancestor);
            ancestor = &ancestor->parent();
        }
        // Construct the missing LineBox::InlineBoxes starting with the topmost ancestor.
        for (auto* ancestor : WTF::makeReversedRange(ancestorsWithoutInlineBoxes)) {
            auto inlineBox = LineBox::InlineLevelBox::createInlineBox(*ancestor, horizontalAligmentOffset, lineBox.logicalWidth());
            setVerticalGeometryForInlineBox(*inlineBox);
            lineBox.addInlineLevelBox(WTFMove(inlineBox));
        }
    };
    createWrappedInlineBoxes();

    auto stretchRootInlineBoxIfNeededQuirk = [&] (const auto& layoutBox) {
        auto& parentInlineBox = lineBox.inlineLevelBoxForLayoutBox(layoutBox.parent());
        if (&parentInlineBox.layoutBox() != &rootBox() || !parentInlineBox.isEmpty())
            return;
        setVerticalGeometryForInlineBox(parentInlineBox);
        parentInlineBox.setIsNonEmpty();
    };

    for (auto& run : runs) {
        auto& layoutBox = run.layoutBox();
        auto logicalLeft = horizontalAligmentOffset + run.logicalLeft();
        if (run.isBox()) {
            auto& inlineLevelBoxGeometry = formattingContext().geometryForBox(layoutBox);
            auto logicalHeight = inlineLevelBoxGeometry.marginBoxHeight();
            auto baseline = logicalHeight;
            if (layoutBox.isInlineBlockBox() && layoutBox.establishesInlineFormattingContext()) {
                auto& formattingState = layoutState().establishedInlineFormattingState(downcast<ContainerBox>(layoutBox));
                auto& lastLine = formattingState.lines().last();
                auto inlineBlockBaseline = lastLine.logicalTop() + lastLine.baseline();
                baseline = inlineLevelBoxGeometry.marginBefore() + inlineLevelBoxGeometry.borderTop() + inlineLevelBoxGeometry.paddingTop().valueOr(0) + inlineBlockBaseline;
            }
            auto atomicInlineLevelBox = LineBox::InlineLevelBox::createAtomicInlineLevelBox(layoutBox, logicalLeft, { run.logicalWidth(), logicalHeight });
            atomicInlineLevelBox->setBaseline(baseline);
            atomicInlineLevelBox->setLayoutBounds(LineBox::InlineLevelBox::LayoutBounds { baseline, { } });
            if (logicalHeight)
                atomicInlineLevelBox->setIsNonEmpty();
            lineBox.addInlineLevelBox(WTFMove(atomicInlineLevelBox));
        } else if (run.isContainerStart()) {
            auto initialLogicalWidth = lineBox.logicalWidth() - run.logicalLeft();
            ASSERT(initialLogicalWidth >= 0);
            auto inlineBox = LineBox::InlineLevelBox::createInlineBox(layoutBox, logicalLeft, initialLogicalWidth);
            setVerticalGeometryForInlineBox(*inlineBox);
            lineBox.addInlineLevelBox(WTFMove(inlineBox));
        } else if (run.isContainerEnd()) {
            // Adjust the logical width when the inline level container closes on this line.
            auto& inlineBox = lineBox.inlineLevelBoxForLayoutBox(layoutBox);
            ASSERT(inlineBox.isInlineBox());
            inlineBox.setLogicalWidth(run.logicalRight() - inlineBox.logicalLeft());
        } else if (run.isText() || run.isSoftLineBreak()) {
            // FIXME: Adjust non-empty inline box height when glyphs from the non-primary font stretch the box.
            stretchRootInlineBoxIfNeededQuirk(layoutBox);
        } else if (run.isHardLineBreak()) {
            auto lineBreakBox = LineBox::InlineLevelBox::createLineBreakBox(layoutBox, logicalLeft);
            setVerticalGeometryForInlineBox(*lineBreakBox);
            lineBox.addInlineLevelBox(WTFMove(lineBreakBox));
            stretchRootInlineBoxIfNeededQuirk(layoutBox);
        } else if (run.isWordBreakOpportunity())
            lineBox.addInlineLevelBox(LineBox::InlineLevelBox::createGenericInlineLevelBox(layoutBox, logicalLeft));
        else
            ASSERT_NOT_REACHED();
    }
}

void LineBoxBuilder::alignInlineLevelBoxesVerticallyAndComputeLineBoxHeight(LineBox& lineBox)
{
    // This function (partially) implements:
    // 2.2. Layout Within Line Boxes
    // https://www.w3.org/TR/css-inline-3/#line-layout
    Vector<LineBox::InlineLevelBox*> lineBoxRelativeInlineLevelBoxes;
    struct AbsoluteTopAndBottom {
        InlineLayoutUnit top { 0 };
        InlineLayoutUnit bottom { 0 };
    };
    HashMap<LineBox::InlineLevelBox*, AbsoluteTopAndBottom> absoluteLogicalTopAndBottomMap;
    auto& rootInlineBox = lineBox.rootInlineBox();
    absoluteLogicalTopAndBottomMap.add(&rootInlineBox, AbsoluteTopAndBottom { { }, rootInlineBox.layoutBounds().height() });

    auto alignInlineBoxRelativeInlineLevelBoxes = [&] {
        // FIXME: Add proper support for cases when the inline box with line box relative alignment has a child inline box
        // with non-line box relative alignment.
        for (auto& inlineLevelBox : lineBox.nonRootInlineLevelBoxes()) {
            auto& layoutBox = inlineLevelBox->layoutBox();
            auto verticalAlignment = layoutBox.style().verticalAlign();
            if (inlineLevelBox->hasLineBoxRelativeAlignment()) {
                lineBoxRelativeInlineLevelBoxes.append(inlineLevelBox.get());
                continue;
            }
            auto& parentInlineBox = lineBox.inlineLevelBoxForLayoutBox(layoutBox.parent());
            auto logicalTop = InlineLayoutUnit { };
            switch (verticalAlignment) {
            case VerticalAlign::Baseline:
                logicalTop = parentInlineBox.baseline() - inlineLevelBox->baseline();
                break;
            case VerticalAlign::TextTop:
                logicalTop = { };
                break;
            case VerticalAlign::TextBottom:
                logicalTop = parentInlineBox.layoutBounds().height() - inlineLevelBox->layoutBounds().height();
                break;
            case VerticalAlign::Middle:
                logicalTop = parentInlineBox.baseline() - (inlineLevelBox->layoutBounds().height() / 2 + parentInlineBox.fontMetrics().xHeight() / 2);
                break;
            default:
                ASSERT_NOT_IMPLEMENTED_YET();
                break;
            }
            inlineLevelBox->setLogicalTop(logicalTop);
            auto parentAbsoluteLogicalTop = absoluteLogicalTopAndBottomMap.get(&parentInlineBox).top;
            auto absoluteLogicalTop = parentAbsoluteLogicalTop + logicalTop;
            absoluteLogicalTopAndBottomMap.add(inlineLevelBox.get(), AbsoluteTopAndBottom { absoluteLogicalTop, absoluteLogicalTop + inlineLevelBox->layoutBounds().height() });
        }
    };
    alignInlineBoxRelativeInlineLevelBoxes();

    auto lineBoxLogicalHeight = InlineLayoutUnit { };
    auto minimumInlineBoxRelativeLogicalTop = InlineLayoutUnit { };
    auto inlineBoxRelativeLogicalHeight = InlineLayoutUnit { };
    auto computeLineBoxLogicalHeight = [&] {
        // FIXME: Add support for layout bounds based line box height.
        auto minimumLogicalTop = InlineLayoutUnit { };
        auto maximumlogicalBottom = InlineLayoutUnit { };
        for (auto absoluteLogicalTopAndBottom : absoluteLogicalTopAndBottomMap.values()) {
            minimumLogicalTop = std::min(minimumLogicalTop, absoluteLogicalTopAndBottom.top);
            maximumlogicalBottom = std::max(maximumlogicalBottom, absoluteLogicalTopAndBottom.bottom);
        }
        minimumInlineBoxRelativeLogicalTop = minimumLogicalTop;
        inlineBoxRelativeLogicalHeight = maximumlogicalBottom - minimumLogicalTop;
        lineBoxLogicalHeight = inlineBoxRelativeLogicalHeight;
        // Now stretch the line box with the line box relative inline level boxes.
        for (auto* lineBoxRelativeInlineLevelBox : lineBoxRelativeInlineLevelBoxes)
            lineBoxLogicalHeight = std::max(lineBoxLogicalHeight, lineBoxRelativeInlineLevelBox->layoutBounds().height());
    };
    computeLineBoxLogicalHeight();

    auto adjustRootInlineBoxVerticalPosition = [&] {
        if (minimumInlineBoxRelativeLogicalTop >= 0)
            return;
        rootInlineBox.setLogicalTop(-minimumInlineBoxRelativeLogicalTop);
    };
    adjustRootInlineBoxVerticalPosition();

    auto alignLineBoxRelativeInlineLevelBoxes = [&] {
        for (auto* inlineLevelBox : lineBoxRelativeInlineLevelBoxes) {
            auto& layoutBox = inlineLevelBox->layoutBox();
            auto verticalAlignment = layoutBox.style().verticalAlign();
            auto logicalTop = InlineLayoutUnit { };
            auto rootInlineBoxOffset = InlineLayoutUnit { };
            switch (verticalAlignment) {
            case VerticalAlign::Top:
                logicalTop = { };
                break;
            case VerticalAlign::Bottom:
                logicalTop = lineBoxLogicalHeight - inlineLevelBox->layoutBounds().height();
                rootInlineBoxOffset = inlineLevelBox->layoutBounds().height() - inlineBoxRelativeLogicalHeight;
                break;
            default:
                ASSERT_NOT_IMPLEMENTED_YET();
                break;
            }
            inlineLevelBox->setLogicalTop(logicalTop);
            if (rootInlineBoxOffset > 0)
                rootInlineBox.setLogicalTop(rootInlineBox.logicalTop() + rootInlineBoxOffset);
        }
    };
    alignLineBoxRelativeInlineLevelBoxes();
    lineBox.setLogicalHeight(lineBoxLogicalHeight);
}

LineBox InlineFormattingContext::Geometry::lineBoxForLineContent(const LineBuilder::LineContent& lineContent)
{
    return LineBoxBuilder(formattingContext()).build(lineContent);
}

InlineRect InlineFormattingContext::Geometry::computedLineLogicalRect(const LineBox& lineBox, const LineBuilder::LineContent& lineContent) const
{
    auto isConsideredEmpty = lineContent.runs.isEmpty() || lineBox.isLineVisuallyEmpty();
    return { lineContent.logicalTopLeft, lineContent.lineLogicalWidth, isConsideredEmpty ? InlineLayoutUnit() : lineBox.logicalHeight()};
}

InlineLayoutUnit InlineFormattingContext::Geometry::logicalTopForNextLine(const LineBuilder::LineContent& lineContent, InlineLayoutUnit previousLineLogicalBottom, const FloatingContext& floatingContext) const
{
    // Normally the next line's logical top is the previous line's logical bottom, but when the line ends
    // with the clear property set, the next line needs to clear the existing floats.
    if (lineContent.runs.isEmpty())
        return previousLineLogicalBottom;
    auto& lastRunLayoutBox = lineContent.runs.last().layoutBox(); 
    if (lastRunLayoutBox.style().clear() == Clear::None)
        return previousLineLogicalBottom;
    auto positionWithClearance = floatingContext.verticalPositionWithClearance(lastRunLayoutBox);
    if (!positionWithClearance)
        return previousLineLogicalBottom;
    return std::max(previousLineLogicalBottom, InlineLayoutUnit(positionWithClearance->position));
}

ContentWidthAndMargin InlineFormattingContext::Geometry::inlineBlockWidthAndMargin(const Box& formattingContextRoot, const HorizontalConstraints& horizontalConstraints, const OverriddenHorizontalValues& overriddenHorizontalValues)
{
    ASSERT(formattingContextRoot.isInFlow());

    // 10.3.10 'Inline-block', replaced elements in normal flow

    // Exactly as inline replaced elements.
    if (formattingContextRoot.isReplacedBox())
        return inlineReplacedWidthAndMargin(downcast<ReplacedBox>(formattingContextRoot), horizontalConstraints, { }, overriddenHorizontalValues);

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

ContentHeightAndMargin InlineFormattingContext::Geometry::inlineBlockHeightAndMargin(const Box& layoutBox, const HorizontalConstraints& horizontalConstraints, const OverriddenVerticalValues& overriddenVerticalValues) const
{
    ASSERT(layoutBox.isInFlow());

    // 10.6.2 Inline replaced elements, block-level replaced elements in normal flow, 'inline-block' replaced elements in normal flow and floating replaced elements
    if (layoutBox.isReplacedBox())
        return inlineReplacedHeightAndMargin(downcast<ReplacedBox>(layoutBox), horizontalConstraints, { }, overriddenVerticalValues);

    // 10.6.6 Complicated cases
    // - 'Inline-block', non-replaced elements.
    return complicatedCases(layoutBox, horizontalConstraints, overriddenVerticalValues);
}

}
}

#endif
