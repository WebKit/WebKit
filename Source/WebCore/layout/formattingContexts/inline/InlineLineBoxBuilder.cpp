/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include "InlineLineBoxBuilder.h"

#include "InlineLineBoxVerticalAligner.h"
#include "InlineLineBuilder.h"
#include "LayoutBoxGeometry.h"
#include "LayoutReplacedBox.h"

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

namespace WebCore {
namespace Layout {

static InlineLayoutUnit hangingGlyphWidth(InlineLayoutUnit extraHorizontalSpace, const Line::RunList& runs, bool isLastLineWithInlineContent)
{
    // When a glyph at the start or end edge of a line hangs, it is not considered when measuring the line’s contents for fit, alignment, or justification.
    // Depending on the line’s alignment/justification, this can result in the mark being placed outside the line box.
    // https://drafts.csswg.org/css-text-3/#hanging
    auto isConditional = isLastLineWithInlineContent;
    auto hangingWidth = InlineLayoutUnit { };
    for (auto& run : WTF::makeReversedRange(runs)) {
        if (run.isInlineBoxStart() || run.isInlineBoxEnd())
            continue;
        if (run.isLineBreak()) {
            isConditional = true;
            continue;
        }
        if (!run.hasTrailingWhitespace())
            break;
        // Check if we have a preserved or hung whitespace.
        if (run.style().whiteSpace() != WhiteSpace::PreWrap)
            break;
        // This is either a normal or conditionally hanging trailing whitespace.
        hangingWidth += run.trailingWhitespaceWidth();
    }
    // In some cases, a glyph at the end of a line can conditionally hang: it hangs only if it does not otherwise fit in the line prior to justification.
    return !isConditional || extraHorizontalSpace < 0 ? hangingWidth : InlineLayoutUnit { };
}

static std::optional<InlineLayoutUnit> horizontalAlignmentOffset(const Line::RunList& runs, TextAlignMode textAlign, InlineLayoutUnit lineLogicalWidth, InlineLayoutUnit contentLogicalWidth, bool isLastLine)
{
    auto extraHorizontalSpace = lineLogicalWidth - contentLogicalWidth;
    // Depending on the line’s alignment/justification, the hanging glyph can be placed outside the line box.
    extraHorizontalSpace += hangingGlyphWidth(extraHorizontalSpace, runs, isLastLine);
    if (extraHorizontalSpace <= 0)
        return { };

    auto computedHorizontalAlignment = [&] {
        if (textAlign != TextAlignMode::Justify)
            return textAlign;
        // Text is justified according to the method specified by the text-justify property,
        // in order to exactly fill the line box. Unless otherwise specified by text-align-last,
        // the last line before a forced break or the end of the block is start-aligned.
        if (isLastLine || (!runs.isEmpty() && runs.last().isLineBreak()))
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
        return extraHorizontalSpace;
    case TextAlignMode::Center:
    case TextAlignMode::WebKitCenter:
        return extraHorizontalSpace / 2;
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

LineBoxBuilder::LineBoxAndGeometry LineBoxBuilder::build(const LineBuilder::LineContent& lineContent)
{
    auto& runs = lineContent.runs;
    auto contentLogicalWidth = lineContent.contentLogicalWidth;
    auto contentLogicalLeft = Layout::horizontalAlignmentOffset(runs, rootBox().style().textAlign(), lineContent.lineLogicalWidth, contentLogicalWidth, lineContent.isLastLineWithInlineContent).value_or(InlineLayoutUnit { });
    auto lineBox = LineBox { rootBox(), contentLogicalLeft, contentLogicalWidth, lineContent.nonSpanningInlineLevelBoxCount };

    auto lineBoxLogicalHeight = constructAndAlignInlineLevelBoxes(lineBox, runs);

    auto lineGeometry = [&] {
        auto lineBoxLogicalRect = InlineRect { lineContent.logicalTopLeft, lineContent.lineLogicalWidth, lineBoxLogicalHeight };
        auto& rootInlineBox = lineBox.rootInlineBox();
        auto enclosingTopAndBottom = LineGeometry::EnclosingTopAndBottom { rootInlineBox.logicalTop(), rootInlineBox.logicalBottom() };

        for (auto& inlineLevelBox : lineBox.nonRootInlineLevelBoxes()) {
            if (!inlineLevelBox.isAtomicInlineLevelBox() || !inlineLevelBox.isInlineBox())
                continue;

            auto& layoutBox = inlineLevelBox.layoutBox();
            auto borderBox = InlineRect { };

            if (inlineLevelBox.isAtomicInlineLevelBox())
                borderBox = lineBox.logicalBorderBoxForAtomicInlineLevelBox(layoutBox, formattingContext().geometryForBox(layoutBox));
            else if (inlineLevelBox.isInlineBox())
                borderBox = lineBox.logicalBorderBoxForInlineBox(layoutBox, formattingContext().geometryForBox(layoutBox));
            else
                ASSERT_NOT_REACHED();

            borderBox.moveBy(lineBoxLogicalRect.topLeft());
            enclosingTopAndBottom.top = std::min(enclosingTopAndBottom.top, borderBox.top());
            enclosingTopAndBottom.bottom = std::max(enclosingTopAndBottom.bottom, borderBox.bottom());
        }
        return LineGeometry { lineBoxLogicalRect, enclosingTopAndBottom, rootInlineBox.logicalTop() + rootInlineBox.baseline(), rootInlineBox.logicalLeft(), rootInlineBox.logicalWidth() };
    };
    return { lineBox, lineGeometry() };
}

void LineBoxBuilder::setVerticalGeometryForInlineBox(InlineLevelBox& inlineLevelBox) const
{
    ASSERT(inlineLevelBox.isInlineBox() || inlineLevelBox.isLineBreakBox());
    auto& fontMetrics = inlineLevelBox.style().fontMetrics();
    InlineLayoutUnit ascent = fontMetrics.ascent();
    InlineLayoutUnit descent = fontMetrics.descent();
    auto logicalHeight = ascent + descent;
    // We need floor/ceil to match legacy layout integral positioning.
    inlineLevelBox.setBaseline(floorf(ascent));
    inlineLevelBox.setDescent(ceil(descent));
    inlineLevelBox.setLogicalHeight(logicalHeight);

    // FIXME: Adjust layout bounds with fallback font when applicable.
    auto& style = inlineLevelBox.layoutBox().style();
    auto lineHeight = style.lineHeight();
    if (lineHeight.isNegative()) {
        // If line-height computes to normal and either text-edge is leading or this is the root inline box,
        // the font’s line gap metric may also be incorporated into A and D by adding half to each side as half-leading.
        // https://www.w3.org/TR/css-inline-3/#inline-height
        // Since text-edge is not supported yet and the initial value is leading, we should just apply it to
        // all inline boxes.
        auto halfLineGap = (fontMetrics.lineSpacing() - logicalHeight) / 2;
        ascent += halfLineGap;
        descent += halfLineGap;
    } else {
        InlineLayoutUnit lineHeight = style.computedLineHeight();
        InlineLayoutUnit halfLeading = (lineHeight - (ascent + descent)) / 2;
        ascent += halfLeading;
        descent += halfLeading;
    }
    // We need floor/ceil to match legacy layout integral positioning.
    inlineLevelBox.setLayoutBounds(InlineLevelBox::LayoutBounds { floorf(ascent), ceil(descent) });
}

InlineLayoutUnit LineBoxBuilder::constructAndAlignInlineLevelBoxes(LineBox& lineBox, const Line::RunList& runs)
{
    auto& rootInlineBox = lineBox.rootInlineBox();
    setVerticalGeometryForInlineBox(rootInlineBox);

    // FIXME: Add fast path support for line-height content.
    // FIXME: We should always be able to exercise the fast path when the line has no content at all, even in non-standards mode or with line-height set.
    auto canUseSimplifiedAlignment = layoutState().inStandardsMode() && rootBox().style().lineHeight().isNegative();
    auto updateCanUseSimplifiedAlignment = [&](auto& inlineLevelBox, std::optional<const BoxGeometry> boxGeometry = std::nullopt) {
        if (!canUseSimplifiedAlignment)
            return;
        canUseSimplifiedAlignment = LineBoxVerticalAligner::canUseSimplifiedAlignmentForInlineLevelBox(rootInlineBox, inlineLevelBox, boxGeometry);
    };

    auto createLineSpanningInlineBoxes = [&] {
        if (runs.isEmpty())
            return;
        // An inline box may not necessarily start on the current line:
        // <span id=outer>line break<br>this content's parent inline box('outer') <span id=inner>starts on the previous line</span></span>
        // We need to make sure that there's an InlineLevelBox for every inline box that's present on the current line.
        // In nesting case we need to create InlineLevelBoxes for the inline box ancestors.
        // We only have to do it on the first run as any subsequent inline content is either at the same/higher nesting level or
        // nested with a [inline box start] run.
        auto& firstRun = runs[0];
        auto& firstRunParentLayoutBox = firstRun.layoutBox().parent();
        // If the parent is the formatting root, we can stop here. This is root inline box content, there's no nesting inline box from the previous line(s)
        // unless the inline box closing is forced over to the current line.
        // e.g.
        // <span>normally the inline box closing forms a continuous content</span>
        // <span>unless it's forced to the next line<br></span>
        auto firstRunNeedsInlineBox = firstRun.isInlineBoxEnd();
        if (!firstRunNeedsInlineBox && isRootLayoutBox(firstRunParentLayoutBox))
            return;
        Vector<const Box*> layoutBoxesWithoutInlineBoxes;
        if (firstRunNeedsInlineBox)
            layoutBoxesWithoutInlineBoxes.append(&firstRun.layoutBox());
        auto* ancestor = &firstRunParentLayoutBox;
        while (!isRootLayoutBox(*ancestor)) {
            layoutBoxesWithoutInlineBoxes.append(ancestor);
            ancestor = &ancestor->parent();
        }
        // Construct the missing LineBox::InlineBoxes starting with the topmost layout box.
        for (auto* layoutBox : WTF::makeReversedRange(layoutBoxesWithoutInlineBoxes)) {
            auto inlineBox = InlineLevelBox::createInlineBox(*layoutBox, rootInlineBox.logicalLeft(), rootInlineBox.logicalWidth(), InlineLevelBox::LineSpanningInlineBox::Yes);
            setVerticalGeometryForInlineBox(inlineBox);
            updateCanUseSimplifiedAlignment(inlineBox);
            lineBox.addInlineLevelBox(WTFMove(inlineBox));
        }
    };
    createLineSpanningInlineBoxes();

    auto lineHasContent = false;
    for (auto& run : runs) {
        auto& layoutBox = run.layoutBox();
        auto runHasContent = [&] () -> bool {
            ASSERT(!lineHasContent);
            if (run.isText() || run.isBox() || run.isSoftLineBreak() || run.isHardLineBreak())
                return true;
            auto& inlineBoxGeometry = formattingContext().geometryForBox(layoutBox);
            // Even negative horizontal margin makes the line "contentful".
            if (run.isInlineBoxStart())
                return inlineBoxGeometry.marginStart() || inlineBoxGeometry.borderLeft() || inlineBoxGeometry.paddingLeft().value_or(0_lu);
            if (run.isInlineBoxEnd())
                return inlineBoxGeometry.marginEnd() || inlineBoxGeometry.borderRight() || inlineBoxGeometry.paddingRight().value_or(0_lu);
            if (run.isWordBreakOpportunity())
                return false;
            ASSERT_NOT_REACHED();
            return true;
        };
        lineHasContent = lineHasContent || runHasContent();
        auto logicalLeft = rootInlineBox.logicalLeft() + run.logicalLeft();
        if (run.isBox()) {
            auto& inlineLevelBoxGeometry = formattingContext().geometryForBox(layoutBox);
            auto marginBoxHeight = inlineLevelBoxGeometry.marginBoxHeight();
            auto ascent = InlineLayoutUnit { };
            if (layoutState().shouldNotSynthesizeInlineBlockBaseline()) {
                // Integration codepath constructs replaced boxes for inline-block content.
                ASSERT(layoutBox.isReplacedBox());
                ascent = *downcast<ReplacedBox>(layoutBox).baseline();
            } else if (layoutBox.isInlineBlockBox()) {
                // The baseline of an 'inline-block' is the baseline of its last line box in the normal flow, unless it has either no in-flow line boxes or
                // if its 'overflow' property has a computed value other than 'visible', in which case the baseline is the bottom margin edge.
                auto synthesizeBaseline = !layoutBox.establishesInlineFormattingContext() || !layoutBox.style().isOverflowVisible();
                if (synthesizeBaseline)
                    ascent = marginBoxHeight;
                else {
                    auto& formattingState = layoutState().establishedInlineFormattingState(downcast<ContainerBox>(layoutBox));
                    auto& lastLine = formattingState.lines().last();
                    auto inlineBlockBaseline = lastLine.lineBoxLogicalRect().top() + lastLine.baseline();
                    ascent = inlineLevelBoxGeometry.marginBefore() + inlineLevelBoxGeometry.borderTop() + inlineLevelBoxGeometry.paddingTop().value_or(0) + inlineBlockBaseline;
                }
            } else if (layoutBox.isReplacedBox())
                ascent = downcast<ReplacedBox>(layoutBox).baseline().value_or(marginBoxHeight);
            else
                ascent = marginBoxHeight;
            logicalLeft += std::max(0_lu, inlineLevelBoxGeometry.marginStart());
            auto atomicInlineLevelBox = InlineLevelBox::createAtomicInlineLevelBox(layoutBox, logicalLeft, { inlineLevelBoxGeometry.borderBoxWidth(), marginBoxHeight });
            atomicInlineLevelBox.setBaseline(ascent);
            atomicInlineLevelBox.setLayoutBounds(InlineLevelBox::LayoutBounds { ascent, marginBoxHeight - ascent });
            updateCanUseSimplifiedAlignment(atomicInlineLevelBox, inlineLevelBoxGeometry);
            lineBox.addInlineLevelBox(WTFMove(atomicInlineLevelBox));
            continue;
        }
        if (run.isInlineBoxStart()) {
            // At this point we don't know yet how wide this inline box is. Let's assume it's as long as the line is
            // and adjust it later if we come across an inlineBoxEnd run (see below).
            // Inline box run is based on margin box. Let's convert it to border box.
            auto marginStart = formattingContext().geometryForBox(layoutBox).marginStart();
            auto initialLogicalWidth = rootInlineBox.logicalWidth() - (run.logicalLeft() + marginStart);
            ASSERT(initialLogicalWidth >= 0);
            auto inlineBox = InlineLevelBox::createInlineBox(layoutBox, logicalLeft + marginStart, initialLogicalWidth);
            setVerticalGeometryForInlineBox(inlineBox);
            updateCanUseSimplifiedAlignment(inlineBox);
            lineBox.addInlineLevelBox(WTFMove(inlineBox));
            continue;
        }
        if (run.isInlineBoxEnd()) {
            // Adjust the logical width when the inline box closes on this line.
            // Note that margin end does not affect the logical width (e.g. positive margin right does not make the run wider).
            auto& inlineBox = lineBox.inlineLevelBoxForLayoutBox(layoutBox);
            ASSERT(inlineBox.isInlineBox());
            // Inline box run is based on margin box. Let's convert it to border box.
            // Negative margin end makes the run have negative width.
            auto marginEndAdjustemnt = -formattingContext().geometryForBox(layoutBox).marginEnd();
            auto logicalWidth = run.logicalWidth() + marginEndAdjustemnt;
            auto inlineBoxLogicalRight = logicalLeft + logicalWidth;
            // When the content pulls the </span> to the logical left direction (e.g. negative letter space)
            // make sure we don't end up with negative logical width on the inline box.
            inlineBox.setLogicalWidth(std::max(0.f, inlineBoxLogicalRight - inlineBox.logicalLeft()));
            updateCanUseSimplifiedAlignment(inlineBox);
            continue;
        }
        if (run.isText() || run.isSoftLineBreak()) {
            // FIXME: Adjust non-empty inline box height when glyphs from the non-primary font stretch the box.
            lineBox.inlineLevelBoxForLayoutBox(layoutBox.parent()).setHasContent();
            continue;
        }
        if (run.isHardLineBreak()) {
            auto lineBreakBox = InlineLevelBox::createLineBreakBox(layoutBox, logicalLeft);
            setVerticalGeometryForInlineBox(lineBreakBox);
            updateCanUseSimplifiedAlignment(lineBreakBox);
            lineBox.addInlineLevelBox(WTFMove(lineBreakBox));
            continue;
        }
        if (run.isWordBreakOpportunity()) {
            lineBox.addInlineLevelBox(InlineLevelBox::createGenericInlineLevelBox(layoutBox, logicalLeft));
            continue;
        }
        ASSERT_NOT_REACHED();
    }

    lineBox.setHasContent(lineHasContent);

    auto verticalAligner = LineBoxVerticalAligner { formattingContext() };
    canUseSimplifiedAlignment = canUseSimplifiedAlignment || !lineHasContent;
    return verticalAligner.computeLogicalHeightAndAlign(lineBox, canUseSimplifiedAlignment);
}

}
}

#endif
