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

namespace WebCore {
namespace Layout {

static std::optional<InlineLayoutUnit> horizontalAlignmentOffset(TextAlignMode textAlign, TextAlignLast textAlignLast, const LineBuilder::LineContent& lineContent, bool isLeftToRightDirection)
{
    // Depending on the line’s alignment/justification, the hanging glyph can be placed outside the line box.
    auto& runs = lineContent.runs;
    auto contentLogicalRight = lineContent.contentLogicalRight;
    auto lineLogicalRight = lineContent.lineLogicalWidth;
    if (lineContent.hangingContentWidth) {
        ASSERT(!runs.isEmpty());
        // If white-space is set to pre-wrap, the UA must (unconditionally) hang this sequence, unless the sequence is followed
        // by a forced line break, in which case it must conditionally hang the sequence is instead.
        // Note that end of last line in a paragraph is considered a forced break.
        auto isConditionalHanging = runs.last().isLineBreak() || lineContent.isLastLineWithInlineContent;
        // In some cases, a glyph at the end of a line can conditionally hang: it hangs only if it does not otherwise fit in the line prior to justification.
        if (isConditionalHanging) {
            // FIXME: Conditional hanging needs partial overflow trimming at glyph boundary, one by one until they fit.
            contentLogicalRight = std::min(contentLogicalRight, lineLogicalRight);
        } else
            contentLogicalRight -= lineContent.hangingContentWidth;
    }
    auto extraHorizontalSpace = lineLogicalRight - contentLogicalRight;
    if (extraHorizontalSpace <= 0)
        return { };

    auto computedHorizontalAlignment = [&] {
        // The last line before a forced break or the end of the block is aligned according to
        // text-align-last.
        if (lineContent.isLastLineWithInlineContent || (!runs.isEmpty() && runs.last().isLineBreak())) {
            switch (textAlignLast) {
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
        }

        // All other lines are aligned according to text-align.
        return textAlign;
    };

    switch (computedHorizontalAlignment()) {
    case TextAlignMode::Left:
    case TextAlignMode::WebKitLeft:
        if (!isLeftToRightDirection)
            return extraHorizontalSpace;
        FALLTHROUGH;
    case TextAlignMode::Start:
        return { };
    case TextAlignMode::Right:
    case TextAlignMode::WebKitRight:
        if (!isLeftToRightDirection)
            return { };
        FALLTHROUGH;
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

LineBox LineBoxBuilder::build(const LineBuilder::LineContent& lineContent, size_t lineIndex)
{
    auto& rootStyle = lineIndex ? rootBox().firstLineStyle() : rootBox().style();
    auto rootInlineBoxAlignmentOffset = valueOrDefault(Layout::horizontalAlignmentOffset(rootStyle.textAlign(), rootStyle.textAlignLast(), lineContent, lineContent.inlineBaseDirection == TextDirection::LTR));
    // FIXME: The overflowing hanging content should be part of the ink overflow.  
    auto lineBox = LineBox { rootBox(), rootInlineBoxAlignmentOffset, lineContent.contentLogicalWidth - lineContent.hangingContentWidth, lineIndex, lineContent.nonSpanningInlineLevelBoxCount };
    constructInlineLevelBoxes(lineBox, lineContent, lineIndex);
    adjustIdeographicBaselineIfApplicable(lineBox, lineIndex);
    auto lineBoxLogicalHeight = LineBoxVerticalAligner { formattingContext() }.computeLogicalHeightAndAlign(lineBox);
    lineBox.setLogicalRect({ lineContent.lineLogicalTopLeft, lineContent.lineLogicalWidth, lineBoxLogicalHeight });
    return lineBox;
}

void LineBoxBuilder::adjustLayoutBoundsWithFallbackFonts(InlineLevelBox& inlineBox, const TextUtil::FallbackFontList& fallbackFontsForContent, FontBaseline fontBaseline) const
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
        InlineLayoutUnit ascent = fontMetrics.ascent(fontBaseline);
        InlineLayoutUnit descent = fontMetrics.descent(fontBaseline);
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

TextUtil::FallbackFontList LineBoxBuilder::collectFallbackFonts(const InlineLevelBox& parentInlineBox, const Line::Run& run, const RenderStyle& style)
{
    ASSERT(parentInlineBox.isInlineBox());
    auto& inlineTextBox = downcast<InlineTextBox>(run.layoutBox());
    if (inlineTextBox.canUseSimplifiedContentMeasuring()) {
        // Simplified text measuring works with primary font only.
        return { };
    }
    auto text = *run.textContent();
    auto fallbackFonts = TextUtil::fallbackFontsForText(StringView(inlineTextBox.content()).substring(text.start, text.length), style, text.needsHyphen ? TextUtil::IncludeHyphen::Yes : TextUtil::IncludeHyphen::No);
    if (fallbackFonts.isEmpty())
        return { };

    auto fallbackFontsForInlineBoxes = m_fallbackFontsForInlineBoxes.get(&parentInlineBox);
    auto numberOfFallbackFontsForInlineBox = fallbackFontsForInlineBoxes.size();
    for (auto* font : fallbackFonts) {
        fallbackFontsForInlineBoxes.add(font);
        m_fallbackFontRequiresIdeographicBaseline = m_fallbackFontRequiresIdeographicBaseline || font->hasVerticalGlyphs();
    }
    if (fallbackFontsForInlineBoxes.size() != numberOfFallbackFontsForInlineBox)
        m_fallbackFontsForInlineBoxes.set(&parentInlineBox, fallbackFontsForInlineBoxes);
    return fallbackFonts;
}

struct LayoutBoundsMetrics {
    InlineLayoutUnit ascent { 0 };
    InlineLayoutUnit descent { 0 };
    InlineLayoutUnit lineSpacing { 0 };
    std::optional<InlineLayoutUnit> preferredLineHeight { };
};
static LayoutBoundsMetrics layoutBoundsPrimaryMetricsForInlineBox(const InlineLevelBox& inlineBox, FontBaseline fontBaseline = AlphabeticBaseline)
{
    ASSERT(inlineBox.isInlineBox());
    auto& fontMetrics = inlineBox.primarymetricsOfPrimaryFont();
    InlineLayoutUnit ascent = fontMetrics.ascent(fontBaseline);
    InlineLayoutUnit descent = fontMetrics.descent(fontBaseline);
    InlineLayoutUnit lineSpacing = fontMetrics.lineSpacing();
    return { ascent, descent, lineSpacing, inlineBox.isPreferredLineHeightFontMetricsBased() ? std::nullopt : std::make_optional(inlineBox.preferredLineHeight()) };
}

void LineBoxBuilder::setBaselineAndLayoutBounds(InlineLevelBox& inlineLevelBox, const LayoutBoundsMetrics& layoutBoundsMetrics, BehavesAsText behavesAsText) const
{
    if (inlineLevelBox.isInlineBox() || inlineLevelBox.isLineBreakBox() || behavesAsText == BehavesAsText::Yes) {
        auto logicalHeight = layoutBoundsMetrics.ascent + layoutBoundsMetrics.descent;
        auto halfLeading = InlineLayoutUnit { };
        if (layoutBoundsMetrics.preferredLineHeight) {
            // If line-height computes to normal and either text-edge is leading or this is the root inline box,
            // the font’s line gap metric may also be incorporated into A and D by adding half to each side as half-leading.
            // https://www.w3.org/TR/css-inline-3/#inline-height
            // Since text-edge is not supported yet and the initial value is leading, we should just apply it to
            // all inline boxes.
            halfLeading = (*layoutBoundsMetrics.preferredLineHeight - logicalHeight) / 2;
        } else {
            // Preferred line height is purely font metrics based (i.e glyphs stretch the line).
            halfLeading = (layoutBoundsMetrics.lineSpacing - logicalHeight) / 2;
        }
        // We need floor/ceil to match legacy layout integral positioning.
        inlineLevelBox.setAscent(floorf(layoutBoundsMetrics.ascent));
        inlineLevelBox.setDescent(ceilf(layoutBoundsMetrics.descent));
        inlineLevelBox.setLogicalHeight(logicalHeight);
        inlineLevelBox.setLayoutBounds({ floorf(layoutBoundsMetrics.ascent + halfLeading), ceilf(layoutBoundsMetrics.descent + halfLeading) });
        return;
    }
    if (inlineLevelBox.isAtomicInlineLevelBox()) {
        inlineLevelBox.setAscent(layoutBoundsMetrics.ascent);
        inlineLevelBox.setLayoutBounds({ layoutBoundsMetrics.ascent, layoutBoundsMetrics.descent });
        return;
    }
    ASSERT_NOT_REACHED();
}

void LineBoxBuilder::constructInlineLevelBoxes(LineBox& lineBox, const LineBuilder::LineContent& lineContent, size_t lineIndex)
{
    auto& rootInlineBox = lineBox.rootInlineBox();
    setBaselineAndLayoutBounds(rootInlineBox, layoutBoundsPrimaryMetricsForInlineBox(rootInlineBox));

    auto styleToUse = [&] (const auto& layoutBox) -> const RenderStyle& {
        return !lineIndex ? layoutBox.firstLineStyle() : layoutBox.style();
    };

    auto lineHasContent = false;
    for (auto& run : lineContent.runs) {
        auto& layoutBox = run.layoutBox();
        auto& style = styleToUse(layoutBox);
        auto runHasContent = [&] () -> bool {
            ASSERT(!lineHasContent);
            if (run.isContentful())
                return true;
            if (run.isText() || run.isWordBreakOpportunity())
                return false;

            ASSERT(run.isInlineBox());
            if (run.isLineSpanningInlineBoxStart()) {
                // We already handled inline box decoration at the non-spanning start.
                return false;
            }
            auto& inlineBoxGeometry = formattingContext().geometryForBox(layoutBox);
            // Even negative horizontal margin makes the line "contentful".
            if (run.isInlineBoxStart())
                return inlineBoxGeometry.marginStart() || inlineBoxGeometry.borderStart() || inlineBoxGeometry.paddingStart().value_or(0_lu);
            if (run.isInlineBoxEnd())
                return inlineBoxGeometry.marginEnd() || inlineBoxGeometry.borderEnd() || inlineBoxGeometry.paddingEnd().value_or(0_lu);
            ASSERT_NOT_REACHED();
            return true;
        };
        lineHasContent = lineHasContent || runHasContent();
        auto logicalLeft = rootInlineBox.logicalLeft() + run.logicalLeft();
        if (run.isBox()) {
            auto& inlineLevelBoxGeometry = formattingContext().geometryForBox(layoutBox);
            auto marginBoxHeight = inlineLevelBoxGeometry.marginBoxHeight();
            auto ascent = [&]() -> InlineLayoutUnit {
                if (layoutState().shouldNotSynthesizeInlineBlockBaseline())
                    return downcast<ElementBox>(layoutBox).baselineForIntegration().value_or(marginBoxHeight);

                if (layoutBox.isInlineBlockBox()) {
                    // The baseline of an 'inline-block' is the baseline of its last line box in the normal flow, unless it has either no in-flow line boxes or
                    // if its 'overflow' property has a computed value other than 'visible', in which case the baseline is the bottom margin edge.
                    auto synthesizeBaseline = !layoutBox.establishesInlineFormattingContext() || !style.isOverflowVisible();
                    if (synthesizeBaseline)
                        return marginBoxHeight;

                    auto& formattingState = layoutState().formattingStateForInlineFormattingContext(downcast<ElementBox>(layoutBox));
                    auto& lastLine = formattingState.lines().last();
                    auto inlineBlockBaseline = lastLine.top() + lastLine.baseline();
                    return inlineLevelBoxGeometry.marginBefore() + inlineLevelBoxGeometry.borderBefore() + inlineLevelBoxGeometry.paddingBefore().value_or(0) + inlineBlockBaseline;
                }
                return marginBoxHeight;
            }();
            logicalLeft += std::max(0_lu, inlineLevelBoxGeometry.marginStart());
            auto atomicInlineLevelBox = InlineLevelBox::createAtomicInlineLevelBox(layoutBox, style, logicalLeft, { inlineLevelBoxGeometry.borderBoxWidth(), marginBoxHeight });
            setBaselineAndLayoutBounds(atomicInlineLevelBox, { ascent, marginBoxHeight - ascent });
            lineBox.addInlineLevelBox(WTFMove(atomicInlineLevelBox));
            continue;
        }
        if (run.isListMarker()) {
            auto& listMarkerBoxGeometry = formattingContext().geometryForBox(layoutBox);
            auto marginBoxHeight = listMarkerBoxGeometry.marginBoxHeight();
            // Integration codepath constructs ReplacedBoxes for list markers.
            auto baseline = downcast<ElementBox>(layoutBox).baselineForIntegration();
            auto ascent = baseline.value_or(marginBoxHeight);

            logicalLeft += std::max(0_lu, listMarkerBoxGeometry.marginStart());
            auto listMarkerInlineLevelBox = InlineLevelBox::createAtomicInlineLevelBox(layoutBox, style, logicalLeft, { listMarkerBoxGeometry.borderBoxWidth(), marginBoxHeight });
            setBaselineAndLayoutBounds(listMarkerInlineLevelBox, { ascent, marginBoxHeight - ascent }, baseline.has_value() ? BehavesAsText::Yes : BehavesAsText::No);
            lineBox.addInlineLevelBox(WTFMove(listMarkerInlineLevelBox));
            continue;
        }
        if (run.isLineSpanningInlineBoxStart()) {
            auto marginStart = InlineLayoutUnit { };
#if ENABLE(CSS_BOX_DECORATION_BREAK)
            if (style.boxDecorationBreak() == BoxDecorationBreak::Clone)
                marginStart = formattingContext().geometryForBox(layoutBox).marginStart();
#endif
            auto adjustedLogicalStart = logicalLeft + std::max(0.0f, marginStart);
            auto logicalWidth = rootInlineBox.logicalWidth() - adjustedLogicalStart;
            auto inlineBox = InlineLevelBox::createInlineBox(layoutBox, style, adjustedLogicalStart, logicalWidth, InlineLevelBox::LineSpanningInlineBox::Yes);
            setBaselineAndLayoutBounds(inlineBox, layoutBoundsPrimaryMetricsForInlineBox(inlineBox));
            lineBox.addInlineLevelBox(WTFMove(inlineBox));
            continue;
        }
        if (run.isInlineBoxStart()) {
            // At this point we don't know yet how wide this inline box is. Let's assume it's as long as the line is
            // and adjust it later if we come across an inlineBoxEnd run (see below).
            // Inline box run is based on margin box. Let's convert it to border box.
            auto marginStart = formattingContext().geometryForBox(layoutBox).marginStart();
            logicalLeft += std::max(0_lu, marginStart);
            auto initialLogicalWidth = rootInlineBox.logicalWidth() - (logicalLeft - rootInlineBox.logicalLeft());
            ASSERT(initialLogicalWidth >= 0 || lineContent.hangingContentWidth || std::isnan(initialLogicalWidth));
            initialLogicalWidth = std::max(initialLogicalWidth, 0.f);
            auto inlineBox = InlineLevelBox::createInlineBox(layoutBox, style, logicalLeft, initialLogicalWidth);
            inlineBox.setIsFirstBox();
            setBaselineAndLayoutBounds(inlineBox, layoutBoundsPrimaryMetricsForInlineBox(inlineBox));
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
            continue;
        }
        if (run.isText()) {
            auto& parentInlineBox = lineBox.inlineLevelBoxForLayoutBox(layoutBox.parent());
            parentInlineBox.setHasContent();
            if (auto fallbackFonts = collectFallbackFonts(parentInlineBox, run, style); !fallbackFonts.isEmpty()) {
                // Adjust non-empty inline box height when glyphs from the non-primary font stretch the box.
                adjustLayoutBoundsWithFallbackFonts(parentInlineBox, fallbackFonts, AlphabeticBaseline);
            }
            continue;
        }
        if (run.isSoftLineBreak()) {
            lineBox.inlineLevelBoxForLayoutBox(layoutBox.parent()).setHasContent();
            continue;
        }
        if (run.isHardLineBreak()) {
            auto lineBreakBox = InlineLevelBox::createLineBreakBox(layoutBox, style, logicalLeft);
            setBaselineAndLayoutBounds(lineBreakBox, layoutBoundsPrimaryMetricsForInlineBox(lineBox.inlineLevelBoxForLayoutBox(layoutBox.parent())));
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
}

void LineBoxBuilder::adjustIdeographicBaselineIfApplicable(LineBox& lineBox, size_t lineIndex)
{
    // Re-compute the ascent/descent values for the inline boxes on the line (including the root inline box)
    // when the style/content needs ideographic baseline setup in vertical writing mode.
    auto& rootInlineBox = lineBox.rootInlineBox();

    auto lineNeedsIdeographicBaseline = [&] {
        auto styleToUse = [&] (auto& inlineLevelBox) -> const RenderStyle& {
            return !lineIndex ? inlineLevelBox.layoutBox().firstLineStyle() : inlineLevelBox.layoutBox().style();
        };
        auto& rootInlineBoxStyle = styleToUse(rootInlineBox);
        if (rootInlineBoxStyle.isHorizontalWritingMode())
            return false;

        auto primaryFontRequiresIdeographicBaseline = [&] (auto& style) {
            return style.fontDescription().orientation() == FontOrientation::Vertical || style.fontCascade().primaryFont().hasVerticalGlyphs();
        };

        if (m_fallbackFontRequiresIdeographicBaseline || primaryFontRequiresIdeographicBaseline(rootInlineBoxStyle))
            return true;
        for (auto& inlineLevelBox : lineBox.nonRootInlineLevelBoxes()) {
            if (inlineLevelBox.isInlineBox() && primaryFontRequiresIdeographicBaseline(styleToUse(inlineLevelBox)))
                return true;
        }
        return false;
    };

    if (!lineNeedsIdeographicBaseline())
        return;

    lineBox.setBaselineType(IdeographicBaseline);

    auto adjustLayoutBoundsWithIdeographicBaseline = [&] (auto& inlineLevelBox) {
        auto initiatesLayoutBoundsChange = inlineLevelBox.isInlineBox() || inlineLevelBox.isAtomicInlineLevelBox() || inlineLevelBox.isLineBreakBox();
        if (!initiatesLayoutBoundsChange)
            return;

        auto behavesAsText = inlineLevelBox.isLineBreakBox() || (inlineLevelBox.isListMarker() && !downcast<ElementBox>(inlineLevelBox.layoutBox()).isListMarkerImage());
        auto layoutBoundsPrimaryMetrics = LayoutBoundsMetrics { };
        if (behavesAsText) {
            auto& parentInlineBox = lineBox.inlineLevelBoxForLayoutBox(inlineLevelBox.layoutBox().parent());
            layoutBoundsPrimaryMetrics = layoutBoundsPrimaryMetricsForInlineBox(parentInlineBox, IdeographicBaseline);
        } else if (inlineLevelBox.isInlineBox())
            layoutBoundsPrimaryMetrics = layoutBoundsPrimaryMetricsForInlineBox(inlineLevelBox, IdeographicBaseline);
        else if (inlineLevelBox.isAtomicInlineLevelBox()) {
            auto inlineLevelBoxHeight = inlineLevelBox.layoutBounds().height();
            InlineLayoutUnit ideographicBaseline = roundToInt(inlineLevelBoxHeight / 2);
            // Move the baseline position but keep the same logical height.
            layoutBoundsPrimaryMetrics = { ideographicBaseline, inlineLevelBoxHeight - ideographicBaseline };
        }
        setBaselineAndLayoutBounds(inlineLevelBox, layoutBoundsPrimaryMetrics);

        auto needsFontFallbackAdjustment = inlineLevelBox.isInlineBox() || inlineLevelBox.isLineBreakBox();
        if (needsFontFallbackAdjustment) {
            if (auto fallbackFonts = m_fallbackFontsForInlineBoxes.get(&inlineLevelBox); !fallbackFonts.isEmpty())
                adjustLayoutBoundsWithFallbackFonts(inlineLevelBox, fallbackFonts, IdeographicBaseline);
        }
    };

    adjustLayoutBoundsWithIdeographicBaseline(rootInlineBox);
    for (auto& inlineLevelBox : lineBox.nonRootInlineLevelBoxes()) {
        if (inlineLevelBox.isAtomicInlineLevelBox()) {
            auto& layoutBox = inlineLevelBox.layoutBox();
            if (layoutBox.isInlineTableBox()) {
                // This is the integration codepath where inline table boxes are represented as atomic inline boxes.
                // Integration codepath sets ideographic baseline by default for non-horizontal content.
                continue;
            }
            auto isInlineBlockWithNonSyntheticBaseline = layoutBox.isInlineBlockBox() && downcast<ElementBox>(layoutBox).baselineForIntegration().has_value();
            if (isInlineBlockWithNonSyntheticBaseline && !layoutBox.style().isHorizontalWritingMode())
                continue;
        }
        adjustLayoutBoundsWithIdeographicBaseline(inlineLevelBox);
    }
}

}
}

