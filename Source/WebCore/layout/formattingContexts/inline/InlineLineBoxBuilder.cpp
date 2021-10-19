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

static std::optional<InlineLayoutUnit> horizontalAlignmentOffset(TextAlignMode textAlign, const LineBuilder::LineContent& lineContent)
{
    // Depending on the line’s alignment/justification, the hanging glyph can be placed outside the line box.
    auto& runs = lineContent.runs;
    auto contentLogicalWidth = lineContent.contentLogicalWidth;
    if (lineContent.hangingWhitespaceWidth) {
        ASSERT(!runs.isEmpty());
        // If white-space is set to pre-wrap, the UA must (unconditionally) hang this sequence, unless the sequence is followed
        // by a forced line break, in which case it must conditionally hang the sequence is instead.
        // Note that end of last line in a paragraph is considered a forced break.
        auto isConditionalHanging = runs.last().isLineBreak() || lineContent.isLastLineWithInlineContent;
        // In some cases, a glyph at the end of a line can conditionally hang: it hangs only if it does not otherwise fit in the line prior to justification.
        // FIXME: Only the overflowing glyphs should be considered for hanging.
        if (isConditionalHanging)
            contentLogicalWidth = std::min(contentLogicalWidth, lineContent.lineLogicalWidth);
        else
            contentLogicalWidth -= lineContent.hangingWhitespaceWidth;
    }
    auto extraHorizontalSpace = lineContent.lineLogicalWidth - contentLogicalWidth;
    if (extraHorizontalSpace <= 0)
        return { };

    auto computedHorizontalAlignment = [&] {
        if (textAlign != TextAlignMode::Justify)
            return textAlign;
        // Text is justified according to the method specified by the text-justify property,
        // in order to exactly fill the line box. Unless otherwise specified by text-align-last,
        // the last line before a forced break or the end of the block is start-aligned.
        if (lineContent.isLastLineWithInlineContent || (!runs.isEmpty() && runs.last().isLineBreak()))
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

LineBoxBuilder::LineAndLineBox LineBoxBuilder::build(const LineBuilder::LineContent& lineContent, size_t lineIndex)
{
    auto textAlign = !lineIndex ? rootBox().firstLineStyle().textAlign() : rootBox().style().textAlign();
    auto contentLogicalLeft = Layout::horizontalAlignmentOffset(textAlign, lineContent).value_or(InlineLayoutUnit { });
    auto lineBox = LineBox { rootBox(), contentLogicalLeft, lineContent.contentLogicalWidth, lineIndex, lineContent.nonSpanningInlineLevelBoxCount };

    auto lineBoxLogicalHeight = constructAndAlignInlineLevelBoxes(lineBox, lineContent.runs, lineIndex);

    auto line = [&] {
        auto lineBoxLogicalRect = InlineRect { lineContent.logicalTopLeft, lineContent.lineLogicalWidth, lineBoxLogicalHeight };
        auto scrollableOverflowRect = lineBoxLogicalRect;
        auto& rootInlineBox = lineBox.rootInlineBox();
        auto enclosingTopAndBottom = InlineDisplay::Line::EnclosingTopAndBottom { lineBoxLogicalRect.top() + rootInlineBox.logicalTop(), lineBoxLogicalRect.top() + rootInlineBox.logicalBottom() };

        for (auto& inlineLevelBox : lineBox.nonRootInlineLevelBoxes()) {
            if (!inlineLevelBox.isAtomicInlineLevelBox() && !inlineLevelBox.isInlineBox())
                continue;

            auto& layoutBox = inlineLevelBox.layoutBox();
            auto borderBox = InlineRect { };

            if (inlineLevelBox.isAtomicInlineLevelBox()) {
                borderBox = lineBox.logicalBorderBoxForAtomicInlineLevelBox(layoutBox, formattingContext().geometryForBox(layoutBox));
                borderBox.moveBy(lineBoxLogicalRect.topLeft());
            } else if (inlineLevelBox.isInlineBox()) {
                auto& boxGeometry = formattingContext().geometryForBox(layoutBox);
                borderBox = lineBox.logicalBorderBoxForInlineBox(layoutBox, boxGeometry);
                borderBox.moveBy(lineBoxLogicalRect.topLeft());
                // Collect scrollable overflow from inline boxes. All other inline level boxes (e.g atomic inline level boxes) stretch the line.
                auto hasScrollableContent = [&] {
                    // In standards mode, inline boxes always start with an imaginary strut.
                    return layoutState().inStandardsMode() || inlineLevelBox.hasContent() || boxGeometry.horizontalBorder() || (boxGeometry.horizontalPadding() && boxGeometry.horizontalPadding().value());
                };
                if (lineBox.hasContent() && hasScrollableContent()) {
                    // Empty lines (e.g. continuation pre/post blocks) don't expect scrollbar overflow.
                    scrollableOverflowRect.expandToContain(borderBox);
                }
            } else
                ASSERT_NOT_REACHED();

            enclosingTopAndBottom.top = std::min(enclosingTopAndBottom.top, borderBox.top());
            enclosingTopAndBottom.bottom = std::max(enclosingTopAndBottom.bottom, borderBox.bottom());
        }
        return InlineDisplay::Line { lineBoxLogicalRect, scrollableOverflowRect, enclosingTopAndBottom, rootInlineBox.logicalTop() + rootInlineBox.baseline(), rootInlineBox.logicalLeft(), rootInlineBox.logicalWidth() };
    };
    return { line(), lineBox };
}

void LineBoxBuilder::adjustVerticalGeometryForInlineBoxWithFallbackFonts(InlineLevelBox& inlineBox, const TextUtil::FallbackFontList& fallbackFontsForContent) const
{
    ASSERT(!fallbackFontsForContent.isEmpty());
    ASSERT(inlineBox.isInlineBox());
    if (!inlineBox.isPreferredLineHeightFontMetricsBased())
        return;

    // https://www.w3.org/TR/css-inline-3/#inline-height
    // When the computed line-height is normal, the layout bounds of an inline box encloses all its glyphs, going from the highest A to the deepest D. 
    auto maxAscent = InlineLayoutUnit { };
    auto maxDescent = InlineLayoutUnit { };
    // If line-height computes to normal and either text-edge is leading or this is the root inline box,
    // the font's line gap metric may also be incorporated into A and D by adding half to each side as half-leading.
    // FIXME: We don't support the text-edge property yet, but its initial value is 'leading' which makes the line-gap adjustment always on.
    auto isTextEdgeLeading = true;
    auto shouldUseLineGapToAdjustAscentDescent = inlineBox.isRootInlineBox() || isTextEdgeLeading;
    for (auto* font : fallbackFontsForContent) {
        auto& fontMetrics = font->fontMetrics();
        InlineLayoutUnit ascent = fontMetrics.ascent();
        InlineLayoutUnit descent = fontMetrics.descent();
        if (shouldUseLineGapToAdjustAscentDescent) {
            auto logicalHeight = ascent + descent;
            auto halfLineGap = (fontMetrics.lineSpacing() - logicalHeight) / 2;
            ascent = ascent + halfLineGap;
            descent = descent + halfLineGap;
        }
        maxAscent = std::max(maxAscent, ascent);
        maxDescent = std::max(maxDescent, descent);
    }

    // We need floor/ceil to match legacy layout integral positioning.
    auto layoutBounds = inlineBox.layoutBounds();
    inlineBox.setLayoutBounds({ std::max(layoutBounds.ascent, floorf(maxAscent)), std::max(layoutBounds.descent, ceilf(maxDescent)) });
}

struct HeightAndLayoutBounds {
    InlineLayoutUnit ascent { 0 };
    InlineLayoutUnit descent { 0 };
    InlineLevelBox::LayoutBounds layoutBounds { };
};
static auto computedHeightAndLayoutBounds(const FontMetrics& fontMetrics, std::optional<InlineLayoutUnit> preferredLineHeight)
{
    InlineLayoutUnit ascent = fontMetrics.ascent();
    InlineLayoutUnit descent = fontMetrics.descent();
    auto logicalHeight = ascent + descent;

    if (preferredLineHeight) {
        // If line-height computes to normal and either text-edge is leading or this is the root inline box,
        // the font’s line gap metric may also be incorporated into A and D by adding half to each side as half-leading.
        // https://www.w3.org/TR/css-inline-3/#inline-height
        // Since text-edge is not supported yet and the initial value is leading, we should just apply it to
        // all inline boxes.
        auto halfLeading = (*preferredLineHeight - logicalHeight) / 2;
        return HeightAndLayoutBounds { ascent, descent, { ascent + halfLeading, descent + halfLeading } };
    }
    // Preferred line height is purely font metrics based (i.e glyphs stretch the line).
    auto halfLineGap = (fontMetrics.lineSpacing() - logicalHeight) / 2;
    return HeightAndLayoutBounds { ascent, descent, { ascent + halfLineGap, descent + halfLineGap } };
}

void LineBoxBuilder::setVerticalGeometryForLineBreakBox(InlineLevelBox& lineBreakBox, const InlineLevelBox& parentInlineBox) const
{
    // We need floor/ceil to match legacy layout integral positioning.
    ASSERT(lineBreakBox.isLineBreakBox());
    ASSERT(parentInlineBox.isInlineBox());

    auto& fontMetrics = parentInlineBox.primaryFontMetrics();
    auto preferredLineHeight = parentInlineBox.isPreferredLineHeightFontMetricsBased() ? std::nullopt : std::make_optional(parentInlineBox.preferredLineHeight());
    auto heightAndLayoutBounds = computedHeightAndLayoutBounds(fontMetrics, preferredLineHeight);

    lineBreakBox.setBaseline(floorf(heightAndLayoutBounds.ascent));
    lineBreakBox.setDescent(ceilf(heightAndLayoutBounds.descent));
    lineBreakBox.setLogicalHeight(heightAndLayoutBounds.ascent + heightAndLayoutBounds.descent);
    lineBreakBox.setLayoutBounds({ floorf(heightAndLayoutBounds.layoutBounds.ascent), ceilf(heightAndLayoutBounds.layoutBounds.descent) });
}

void LineBoxBuilder::setInitialVerticalGeometryForInlineBox(InlineLevelBox& inlineBox) const
{
    // We need floor/ceil to match legacy layout integral positioning.
    ASSERT(inlineBox.isInlineBox());

    auto& fontMetrics = inlineBox.primaryFontMetrics();
    auto preferredLineHeight = inlineBox.isPreferredLineHeightFontMetricsBased() ? std::nullopt : std::make_optional(inlineBox.preferredLineHeight());
    auto heightAndLayoutBounds = computedHeightAndLayoutBounds(fontMetrics, preferredLineHeight);

    inlineBox.setBaseline(floorf(heightAndLayoutBounds.ascent));
    inlineBox.setDescent(ceilf(heightAndLayoutBounds.descent));
    inlineBox.setLogicalHeight(heightAndLayoutBounds.ascent + heightAndLayoutBounds.descent);
    inlineBox.setLayoutBounds({ floorf(heightAndLayoutBounds.layoutBounds.ascent), ceilf(heightAndLayoutBounds.layoutBounds.descent) });
}

InlineLayoutUnit LineBoxBuilder::constructAndAlignInlineLevelBoxes(LineBox& lineBox, const Line::RunList& runs, size_t lineIndex)
{
    auto& rootInlineBox = lineBox.rootInlineBox();
    setInitialVerticalGeometryForInlineBox(rootInlineBox);

    // FIXME: Add fast path support for line-height content.
    // FIXME: We should always be able to exercise the fast path when the line has no content at all, even in non-standards mode or with line-height set.
    auto canUseSimplifiedAlignment = layoutState().inStandardsMode() && rootInlineBox.isPreferredLineHeightFontMetricsBased();
    auto updateCanUseSimplifiedAlignment = [&](auto& inlineLevelBox, std::optional<const BoxGeometry> boxGeometry = std::nullopt) {
        if (!canUseSimplifiedAlignment)
            return;
        canUseSimplifiedAlignment = LineBoxVerticalAligner::canUseSimplifiedAlignmentForInlineLevelBox(rootInlineBox, inlineLevelBox, boxGeometry);
    };

    auto styleToUse = [&] (const auto& layoutBox) -> const RenderStyle& {
        return !lineIndex ? layoutBox.firstLineStyle() : layoutBox.style();
    };

    auto lineHasContent = false;
    for (auto& run : runs) {
        auto& layoutBox = run.layoutBox();
        auto& style = styleToUse(layoutBox);
        auto runHasContent = [&] () -> bool {
            ASSERT(!lineHasContent);
            if (run.isText() || run.isBox() || run.isSoftLineBreak() || run.isHardLineBreak())
                return true;
            if (run.isLineSpanningInlineBoxStart())
                return false;
            if (run.isWordBreakOpportunity())
                return false;
            auto& inlineBoxGeometry = formattingContext().geometryForBox(layoutBox);
            // Even negative horizontal margin makes the line "contentful".
            if (run.isInlineBoxStart())
                return inlineBoxGeometry.marginStart() || inlineBoxGeometry.borderLeft() || inlineBoxGeometry.paddingLeft().value_or(0_lu);
            if (run.isInlineBoxEnd())
                return inlineBoxGeometry.marginEnd() || inlineBoxGeometry.borderRight() || inlineBoxGeometry.paddingRight().value_or(0_lu);
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
                auto synthesizeBaseline = !layoutBox.establishesInlineFormattingContext() || !style.isOverflowVisible();
                if (synthesizeBaseline)
                    ascent = marginBoxHeight;
                else {
                    auto& formattingState = layoutState().formattingStateForInlineFormattingContext(downcast<ContainerBox>(layoutBox));
                    auto& lastLine = formattingState.lines().last();
                    auto inlineBlockBaseline = lastLine.lineBoxLogicalRect().top() + lastLine.baseline();
                    ascent = inlineLevelBoxGeometry.marginBefore() + inlineLevelBoxGeometry.borderTop() + inlineLevelBoxGeometry.paddingTop().value_or(0) + inlineBlockBaseline;
                }
            } else if (layoutBox.isReplacedBox())
                ascent = downcast<ReplacedBox>(layoutBox).baseline().value_or(marginBoxHeight);
            else
                ascent = marginBoxHeight;
            logicalLeft += std::max(0_lu, inlineLevelBoxGeometry.marginStart());
            auto atomicInlineLevelBox = InlineLevelBox::createAtomicInlineLevelBox(layoutBox, style, logicalLeft, { inlineLevelBoxGeometry.borderBoxWidth(), marginBoxHeight });
            atomicInlineLevelBox.setBaseline(ascent);
            atomicInlineLevelBox.setLayoutBounds(InlineLevelBox::LayoutBounds { ascent, marginBoxHeight - ascent });
            updateCanUseSimplifiedAlignment(atomicInlineLevelBox, inlineLevelBoxGeometry);
            lineBox.addInlineLevelBox(WTFMove(atomicInlineLevelBox));
            continue;
        }
        if (run.isLineSpanningInlineBoxStart()) {
            auto marginStart = InlineLayoutUnit { };
#if ENABLE(CSS_BOX_DECORATION_BREAK)
            if (style.boxDecorationBreak() == BoxDecorationBreak::Clone)
                marginStart = formattingContext().geometryForBox(layoutBox).marginStart();
#endif
            auto adjustedLogicalStart = logicalLeft + marginStart;
            auto logicalWidth = rootInlineBox.logicalWidth() - adjustedLogicalStart;
            auto inlineBox = InlineLevelBox::createInlineBox(layoutBox, style, adjustedLogicalStart, logicalWidth, InlineLevelBox::LineSpanningInlineBox::Yes);
            setInitialVerticalGeometryForInlineBox(inlineBox);
            updateCanUseSimplifiedAlignment(inlineBox);
            lineBox.addInlineLevelBox(WTFMove(inlineBox));
            continue;
        }
        if (run.isInlineBoxStart()) {
            // At this point we don't know yet how wide this inline box is. Let's assume it's as long as the line is
            // and adjust it later if we come across an inlineBoxEnd run (see below).
            // Inline box run is based on margin box. Let's convert it to border box.
            auto marginStart = formattingContext().geometryForBox(layoutBox).marginStart();
            auto initialLogicalWidth = rootInlineBox.logicalWidth() - (run.logicalLeft() + marginStart);
            ASSERT(initialLogicalWidth >= 0);
            auto inlineBox = InlineLevelBox::createInlineBox(layoutBox, style, logicalLeft + marginStart, initialLogicalWidth);
            inlineBox.setIsFirstBox();
            setInitialVerticalGeometryForInlineBox(inlineBox);
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
            inlineBox.setIsLastBox();
            updateCanUseSimplifiedAlignment(inlineBox);
            continue;
        }
        if (run.isText()) {
            auto& parentInlineBox = lineBox.inlineLevelBoxForLayoutBox(layoutBox.parent());
            parentInlineBox.setHasContent();
            auto fallbackFonts = TextUtil::fallbackFontsForRun(run, style);
            if (!fallbackFonts.isEmpty()) {
                // Adjust non-empty inline box height when glyphs from the non-primary font stretch the box.
                adjustVerticalGeometryForInlineBoxWithFallbackFonts(parentInlineBox, fallbackFonts);
                updateCanUseSimplifiedAlignment(parentInlineBox);
            }
            continue;
        }
        if (run.isSoftLineBreak()) {
            lineBox.inlineLevelBoxForLayoutBox(layoutBox.parent()).setHasContent();
            continue;
        }
        if (run.isHardLineBreak()) {
            auto lineBreakBox = InlineLevelBox::createLineBreakBox(layoutBox, style, logicalLeft);
            auto& parentInlineBox = lineBox.inlineLevelBoxForLayoutBox(layoutBox.parent());
            setVerticalGeometryForLineBreakBox(lineBreakBox, parentInlineBox);
            updateCanUseSimplifiedAlignment(lineBreakBox);
            lineBox.addInlineLevelBox(WTFMove(lineBreakBox));
            continue;
        }
        if (run.isWordBreakOpportunity()) {
            lineBox.addInlineLevelBox(InlineLevelBox::createGenericInlineLevelBox(layoutBox, style, logicalLeft));
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
