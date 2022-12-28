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

#include "BlockLayoutState.h"
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
    if (lineContent.hangingContent.width) {
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
            contentLogicalRight -= lineContent.hangingContent.width;
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

LineBoxBuilder::LineBoxBuilder(const InlineFormattingContext& inlineFormattingContext, const LineBuilder::LineContent& lineContent, BlockLayoutState::LeadingTrim leadingTrim)
    : m_inlineFormattingContext(inlineFormattingContext)
    , m_lineContent(lineContent)
    , m_leadingTrim(leadingTrim)
{
}

LineBox LineBoxBuilder::build(size_t lineIndex)
{
    auto& lineContent = this->lineContent();
    auto rootInlineBoxAlignmentOffset = valueOrDefault(Layout::horizontalAlignmentOffset(rootStyle().textAlign(), rootStyle().textAlignLast(), lineContent, lineContent.inlineBaseDirection == TextDirection::LTR));
    // FIXME: The overflowing hanging content should be part of the ink overflow.  
    auto lineBox = LineBox { rootBox(), rootInlineBoxAlignmentOffset, lineContent.contentLogicalWidth - lineContent.hangingContent.width, lineIndex, lineContent.nonSpanningInlineLevelBoxCount };
    constructInlineLevelBoxes(lineBox);
    adjustIdeographicBaselineIfApplicable(lineBox);
    adjustInlineBoxHeightsForLineBoxContainIfApplicable(lineBox);
    computeLineBoxGeometry(lineBox);
    return lineBox;
}

InlineLevelBox::LayoutBounds LineBoxBuilder::adjustedLayoutBoundsWithFallbackFonts(InlineLevelBox& inlineBox, const TextUtil::FallbackFontList& fallbackFontsForContent, FontBaseline fontBaseline) const
{
    ASSERT(!fallbackFontsForContent.isEmpty());
    ASSERT(inlineBox.isInlineBox());

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
            auto halfLeading = (fontMetrics.lineSpacing() - logicalHeight) / 2;
            ascent = ascent + halfLeading;
            descent = descent + halfLeading;
        }
        maxAscent = std::max(maxAscent, ascent);
        maxDescent = std::max(maxDescent, descent);
    }

    // We need floor/ceil to match legacy layout integral positioning.
    auto layoutBounds = *inlineBox.layoutBounds();
    return { std::max(layoutBounds.ascent, floorf(maxAscent)), std::max(layoutBounds.descent, ceilf(maxDescent)) };
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

struct AscentAndDescent {
    InlineLayoutUnit ascent { 0 };
    InlineLayoutUnit descent { 0 };

    InlineLayoutUnit height() const { return ascent + descent; }
    // FIXME: Remove this.
    // We need floor/ceil to match legacy layout integral positioning.
    AscentAndDescent round() const { return { floorf(ascent), ceilf(descent) }; }
};
static AscentAndDescent primaryFontMetricsForInlineBox(const InlineLevelBox& inlineBox, FontBaseline fontBaseline = AlphabeticBaseline)
{
    ASSERT(inlineBox.isInlineBox());
    auto& fontMetrics = inlineBox.primarymetricsOfPrimaryFont();
    InlineLayoutUnit ascent = fontMetrics.ascent(fontBaseline);
    InlineLayoutUnit descent = fontMetrics.descent(fontBaseline);
    return { ascent, descent };
}

static bool isTextEdgeLeading(const InlineLevelBox& inlineBox)
{
    ASSERT(inlineBox.isInlineBox());
    auto textEdge = inlineBox.textEdge();
    ASSERT(textEdge.over != TextEdgeType::Leading || textEdge.under == TextEdgeType::Leading);
    return textEdge.over == TextEdgeType::Leading;
}

void LineBoxBuilder::setLayoutBoundsForInlineBox(InlineLevelBox& inlineBox, FontBaseline fontBaseline) const
{
    ASSERT(inlineBox.isInlineBox());
    auto& fontMetrics = inlineBox.primarymetricsOfPrimaryFont();
    auto ascent = [&]() -> InlineLayoutUnit {
        if (inlineBox.isRootInlineBox() || inlineBox.textEdge().over == TextEdgeType::Leading)
            return fontMetrics.ascent(fontBaseline);

        switch (inlineBox.textEdge().over) {
        case TextEdgeType::Text:
            return fontMetrics.ascent(fontBaseline);
        case TextEdgeType::CapHeight:
            return fontMetrics.floatCapHeight();
        case TextEdgeType::ExHeight:
            return fontMetrics.xHeight();
        case TextEdgeType::CJKIdeographic:
            return fontMetrics.ascent(IdeographicBaseline);
        case TextEdgeType::CJKIdeographicInk:
            ASSERT_NOT_IMPLEMENTED_YET();
            return fontMetrics.ascent(IdeographicBaseline);
        default:
            ASSERT_NOT_REACHED();
            return fontMetrics.ascent(fontBaseline);
        }
    }();
    auto descent = [&]() -> InlineLayoutUnit {
        if (inlineBox.isRootInlineBox() || inlineBox.textEdge().over == TextEdgeType::Leading)
            return fontMetrics.descent(fontBaseline);

        switch (inlineBox.textEdge().under) {
        case TextEdgeType::Text:
            return fontMetrics.descent(fontBaseline);
        case TextEdgeType::Alphabetic:
            return 0.f;
        case TextEdgeType::CJKIdeographic:
            return fontMetrics.descent(IdeographicBaseline);
        case TextEdgeType::CJKIdeographicInk:
            ASSERT_NOT_IMPLEMENTED_YET();
            return fontMetrics.descent(IdeographicBaseline);
        default:
            ASSERT_NOT_REACHED();
            return fontMetrics.descent(fontBaseline);
        }
    }();

    if (!inlineBox.isPreferredLineHeightFontMetricsBased()) {
        // https://www.w3.org/TR/css-inline-3/#inline-height
        // When computed line-height is not normal, calculate the leading L as L = line-height - (A + D).
        // Half the leading (its half-leading) is added above A, and the other half below D,
        // giving an effective ascent above the baseline of A′ = A + L/2, and an effective descent of D′ = D + L/2.
        auto halfLeading = (inlineBox.preferredLineHeight() - (ascent + descent)) / 2;
        if (!isTextEdgeLeading(inlineBox) && !inlineBox.isRootInlineBox()) {
            // However, if text-edge is not leading and this is not the root inline box, if the half-leading is positive, treat it as zero.
            halfLeading = std::min(halfLeading, 0.f);
        }
        ascent += halfLeading;
        descent += halfLeading;
    } else {
        // https://www.w3.org/TR/css-inline-3/#inline-height
        // If line-height computes to normal and either text-edge is leading or this is the root inline box,
        // the font’s line gap metric may also be incorporated into A and D by adding half to each side as half-leading.
        auto shouldIncorporateHalfLeading = inlineBox.isRootInlineBox() || isTextEdgeLeading(inlineBox);
        if (shouldIncorporateHalfLeading) {
            InlineLayoutUnit lineGap = inlineBox.primarymetricsOfPrimaryFont().lineSpacing();
            auto halfLeading = (lineGap - (ascent + descent)) / 2;
            ascent += halfLeading;
            descent += halfLeading;
        }
    }
    if (!isTextEdgeLeading(inlineBox) && !inlineBox.isRootInlineBox()) {
        // Additionally, when text-edge is not leading, the layout bounds are inflated by the sum of the margin,
        // border, and padding on each side.
        ASSERT(!inlineBox.isRootInlineBox());
        auto& inlineBoxGeometry = formattingContext().geometryForBox(inlineBox.layoutBox());
        ascent += inlineBoxGeometry.marginBorderAndPaddingBefore();
        descent += inlineBoxGeometry.marginBorderAndPaddingAfter();
    }
    inlineBox.setLayoutBounds({ floorf(ascent), ceilf(descent) });
}

AscentAndDescent LineBoxBuilder::computedAsentAndDescentForInlineBox(const InlineLevelBox& inlineBox, FontBaseline fontBaseline) const
{
    ASSERT(inlineBox.isInlineBox());
    if (inlineBox.isRootInlineBox())
        return primaryFontMetricsForInlineBox(inlineBox, fontBaseline);

    auto& fontMetrics = inlineBox.primarymetricsOfPrimaryFont();
    auto leadingTrim = inlineBox.leadingTrim();
    if (leadingTrim == LeadingTrim::Normal)
        return primaryFontMetricsForInlineBox(inlineBox, fontBaseline);
    auto ascent = [&]() -> InlineLayoutUnit {
        if (leadingTrim == LeadingTrim::End)
            return fontMetrics.ascent(fontBaseline);

        switch (inlineBox.textEdge().over) {
        case TextEdgeType::Leading:
        case TextEdgeType::Text:
            return fontMetrics.ascent(fontBaseline);
        case TextEdgeType::CapHeight:
            return fontMetrics.floatCapHeight();
        case TextEdgeType::ExHeight:
            return fontMetrics.xHeight();
        case TextEdgeType::CJKIdeographic:
        case TextEdgeType::CJKIdeographicInk:
            ASSERT_NOT_IMPLEMENTED_YET();
            return fontMetrics.ascent(fontBaseline);
        default:
            ASSERT_NOT_REACHED();
            return fontMetrics.ascent(fontBaseline);
        }
    };
    auto descent = [&]() -> InlineLayoutUnit {
        if (leadingTrim == LeadingTrim::Start)
            return fontMetrics.descent(fontBaseline);

        switch (inlineBox.textEdge().under) {
        case TextEdgeType::Leading:
        case TextEdgeType::Text:
            return fontMetrics.descent(fontBaseline);
        case TextEdgeType::Alphabetic:
            return 0.f;
        case TextEdgeType::CJKIdeographic:
        case TextEdgeType::CJKIdeographicInk:
            ASSERT_NOT_IMPLEMENTED_YET();
            return fontMetrics.descent(fontBaseline);
        default:
            ASSERT_NOT_REACHED();
            return fontMetrics.descent(fontBaseline);
        }
    };
    return { ascent(), descent() };
}

void LineBoxBuilder::setVerticalPropertiesForInlineLevelBox(const LineBox& lineBox, InlineLevelBox& inlineLevelBox) const
{
    auto setAscentAndDescent = [&] (auto ascentAndDescent, bool applyLegacyRounding = true) {
        auto adjustedAscentAndDescent = applyLegacyRounding ? ascentAndDescent.round() : ascentAndDescent;
        inlineLevelBox.setAscent(adjustedAscentAndDescent.ascent);
        inlineLevelBox.setDescent(adjustedAscentAndDescent.descent);
    };

    if (inlineLevelBox.isInlineBox()) {
        setLayoutBoundsForInlineBox(inlineLevelBox, lineBox.baselineType());
        auto ascentAndDescent = computedAsentAndDescentForInlineBox(inlineLevelBox, lineBox.baselineType());
        setAscentAndDescent(ascentAndDescent);
        inlineLevelBox.setLogicalHeight(ascentAndDescent.height());

        // With leading-trim, the inline box top is not always where the content starts.
        auto fontMetricAscent = primaryFontMetricsForInlineBox(inlineLevelBox, lineBox.baselineType()).ascent;
        inlineLevelBox.setInlineBoxContentOffsetForLeadingTrim(fontMetricAscent - ascentAndDescent.ascent);
        return;
    }
    if (inlineLevelBox.isLineBreakBox()) {
        auto parentAscentAndDescent = primaryFontMetricsForInlineBox(lineBox.inlineLevelBoxForLayoutBox(inlineLevelBox.layoutBox().parent()), lineBox.baselineType());
        setAscentAndDescent(parentAscentAndDescent);
        inlineLevelBox.setLogicalHeight(parentAscentAndDescent.height());
        return;
    }
    if (inlineLevelBox.isListMarker()) {
        // Special list marker handling. Text driven list markers behave as text when it comes to layout bounds.
        auto& layoutBox = inlineLevelBox.layoutBox();
        auto& listMarkerBoxGeometry = formattingContext().geometryForBox(layoutBox);
        auto marginBoxHeight = listMarkerBoxGeometry.marginBoxHeight();

        inlineLevelBox.setLogicalHeight(marginBoxHeight);
        if (lineBox.baselineType() == IdeographicBaseline) {
            // FIXME: We should rely on the integration baseline.
            setAscentAndDescent(primaryFontMetricsForInlineBox(lineBox.inlineLevelBoxForLayoutBox(inlineLevelBox.layoutBox().parent()), lineBox.baselineType()));
            return;
        }
        if (auto ascent = downcast<ElementBox>(layoutBox).baselineForIntegration())
            return setAscentAndDescent(AscentAndDescent { *ascent, marginBoxHeight - *ascent });
        setAscentAndDescent(AscentAndDescent { marginBoxHeight, { } });
        return;
    }
    if (inlineLevelBox.isAtomicInlineLevelBox()) {
        auto& layoutBox = inlineLevelBox.layoutBox();
        auto& inlineLevelBoxGeometry = formattingContext().geometryForBox(layoutBox);
        auto marginBoxHeight = inlineLevelBoxGeometry.marginBoxHeight();
        auto ascent = [&]() -> InlineLayoutUnit {
            if (layoutState().shouldNotSynthesizeInlineBlockBaseline())
                return downcast<ElementBox>(layoutBox).baselineForIntegration().value_or(marginBoxHeight);

            if (layoutBox.isInlineBlockBox()) {
                // The baseline of an 'inline-block' is the baseline of its last line box in the normal flow, unless it has either no in-flow line boxes or
                // if its 'overflow' property has a computed value other than 'visible', in which case the baseline is the bottom margin edge.
                auto synthesizeBaseline = !layoutBox.establishesInlineFormattingContext() || !layoutBox.style().isOverflowVisible();
                if (synthesizeBaseline)
                    return marginBoxHeight;

                auto& formattingState = layoutState().formattingStateForInlineFormattingContext(downcast<ElementBox>(layoutBox));
                auto& lastLine = formattingState.lines().last();
                auto inlineBlockBaseline = lastLine.top() + lastLine.baseline();
                return inlineLevelBoxGeometry.marginBefore() + inlineLevelBoxGeometry.borderBefore() + inlineLevelBoxGeometry.paddingBefore().value_or(0) + inlineBlockBaseline;
            }
            return marginBoxHeight;
        }();
        setAscentAndDescent(AscentAndDescent { ascent, marginBoxHeight - ascent }, false);
        inlineLevelBox.setLogicalHeight(marginBoxHeight);
        return;
    }
    ASSERT_NOT_REACHED();
}

void LineBoxBuilder::constructInlineLevelBoxes(LineBox& lineBox)
{
    auto& rootInlineBox = lineBox.rootInlineBox();
    setVerticalPropertiesForInlineLevelBox(lineBox, rootInlineBox);

    auto styleToUse = [&] (const auto& layoutBox) -> const RenderStyle& {
        return isFirstLine() ? layoutBox.firstLineStyle() : layoutBox.style();
    };

    auto lineHasContent = false;
    for (auto& run : lineContent().runs) {
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
        if (run.isBox() || run.isListMarker()) {
            auto& inlineLevelBoxGeometry = formattingContext().geometryForBox(layoutBox);
            if (run.isListMarker()) {
                auto& listMarkerBox = downcast<ElementBox>(layoutBox);
                auto lineBoxOffset = lineContent().lineLogicalTopLeft.x() - lineContent().lineInitialLogicalLeft;
                auto rootInlineBoxOffsetFromContentBox = LayoutUnit { lineBoxOffset + lineBox.rootInlineBoxAlignmentOffset() };
                formattingContext().formattingGeometry().adjustMarginStartForListMarker(listMarkerBox, rootInlineBoxOffsetFromContentBox);
                logicalLeft -= rootInlineBoxOffsetFromContentBox;
                if (!listMarkerBox.isListMarkerImage()) {
                    // Non-image type of list markers make their parent inline boxes (e.g. root inline box) contentful (and stretch them vertically).
                    lineBox.inlineLevelBoxForLayoutBox(listMarkerBox.parent()).setHasContent();
                }
            } else
                logicalLeft += std::max(0_lu, inlineLevelBoxGeometry.marginStart());
            auto atomicInlineLevelBox = InlineLevelBox::createAtomicInlineLevelBox(layoutBox, style, logicalLeft, inlineLevelBoxGeometry.borderBoxWidth());
            setVerticalPropertiesForInlineLevelBox(lineBox, atomicInlineLevelBox);
            lineBox.addInlineLevelBox(WTFMove(atomicInlineLevelBox));
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
            setVerticalPropertiesForInlineLevelBox(lineBox, inlineBox);
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
            ASSERT(initialLogicalWidth >= 0 || lineContent().hangingContent.width || std::isnan(initialLogicalWidth));
            initialLogicalWidth = std::max(initialLogicalWidth, 0.f);
            auto inlineBox = InlineLevelBox::createInlineBox(layoutBox, style, logicalLeft, initialLogicalWidth);
            inlineBox.setIsFirstBox();
            setVerticalPropertiesForInlineLevelBox(lineBox, inlineBox);
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
                if (parentInlineBox.isPreferredLineHeightFontMetricsBased())
                    parentInlineBox.setLayoutBounds(adjustedLayoutBoundsWithFallbackFonts(parentInlineBox, fallbackFonts, AlphabeticBaseline));
            }
            continue;
        }
        if (run.isSoftLineBreak()) {
            lineBox.inlineLevelBoxForLayoutBox(layoutBox.parent()).setHasContent();
            continue;
        }
        if (run.isHardLineBreak()) {
            auto lineBreakBox = InlineLevelBox::createLineBreakBox(layoutBox, style, logicalLeft);
            setVerticalPropertiesForInlineLevelBox(lineBox, lineBreakBox);
            lineBox.addInlineLevelBox(WTFMove(lineBreakBox));

            if (layoutState().inStandardsMode() || InlineFormattingQuirks::lineBreakBoxAffectsParentInlineBox(lineBox))
                lineBox.inlineLevelBoxForLayoutBox(layoutBox.parent()).setHasContent();
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

void LineBoxBuilder::adjustInlineBoxHeightsForLineBoxContainIfApplicable(LineBox& lineBox)
{
    // While line-box-contain normally tells whether a certain type of content should be included when computing the line box height,
    // font and Glyphs values affect the "size" of the associated inline boxes (which affects the line box height).
    auto lineBoxContain = isFirstLine() ? rootBox().firstLineStyle().lineBoxContain() : rootBox().style().lineBoxContain();

    // Collect layout bounds based on the contain property and set them on the inline boxes when they are applicable.
    HashMap<InlineLevelBox*, TextUtil::EnclosingAscentDescent> inlineBoxBoundsMap;

    if (lineBoxContain.contains(LineBoxContain::InlineBox)) {
        for (auto& inlineLevelBox : lineBox.nonRootInlineLevelBoxes()) {
            if (!inlineLevelBox.isInlineBox())
                continue;
            auto& inlineBoxGeometry = formattingContext().geometryForBox(inlineLevelBox.layoutBox());
            auto layoutBounds = *inlineLevelBox.layoutBounds();
            auto ascent = layoutBounds.ascent + inlineBoxGeometry.marginBorderAndPaddingBefore();
            auto descent = layoutBounds.descent + inlineBoxGeometry.marginBorderAndPaddingAfter();
            inlineBoxBoundsMap.set(&inlineLevelBox, TextUtil::EnclosingAscentDescent { ascent, descent });
        }
    }

    if (lineBoxContain.contains(LineBoxContain::Font)) {
        // Assign font based layout bounds to all inline boxes.
        auto ensureFontMetricsBasedHeight = [&] (auto& inlineBox) {
            ASSERT(inlineBox.isInlineBox());
            if (inlineBox.isPreferredLineHeightFontMetricsBased()) {
                auto fontMetricsBaseLayoutBounds = *inlineBox.layoutBounds();
                inlineBoxBoundsMap.set(&inlineBox, TextUtil::EnclosingAscentDescent { fontMetricsBaseLayoutBounds.ascent, fontMetricsBaseLayoutBounds.descent });
                return;
            }

            auto ascentAndDescent = primaryFontMetricsForInlineBox(inlineBox, lineBox.baselineType());
            InlineLayoutUnit lineGap = inlineBox.primarymetricsOfPrimaryFont().lineSpacing();
            auto halfLeading = (lineGap - ascentAndDescent.height()) / 2;
            auto ascent = ascentAndDescent.ascent + halfLeading;
            auto descent = ascentAndDescent.descent + halfLeading;
            if (auto fallbackFonts = m_fallbackFontsForInlineBoxes.get(&inlineBox); !fallbackFonts.isEmpty()) {
                auto layoutBounds = adjustedLayoutBoundsWithFallbackFonts(inlineBox, fallbackFonts, lineBox.baselineType());
                ascent = std::max(ascent, layoutBounds.ascent);
                descent = std::max(descent, layoutBounds.descent);
            }
            inlineBoxBoundsMap.set(&inlineBox, TextUtil::EnclosingAscentDescent { ascent, descent });
        };

        ensureFontMetricsBasedHeight(lineBox.rootInlineBox());
        for (auto& inlineLevelBox : lineBox.nonRootInlineLevelBoxes()) {
            if (!inlineLevelBox.isInlineBox())
                continue;
            ensureFontMetricsBasedHeight(inlineLevelBox);
        }
    }

    if (lineBoxContain.contains(LineBoxContain::Glyphs)) {
        // Compute text content (glyphs) hugging inline box layout bounds.
        for (auto run : lineContent().runs) {
            if (!run.isText())
                continue;

            auto& textBox = downcast<InlineTextBox>(run.layoutBox());
            auto textContent = run.textContent();
            auto& style = isFirstLine() ? textBox.firstLineStyle() : textBox.style();
            auto enclosingAscentDescentForRun = TextUtil::enclosingGlyphBoundsForText(StringView(textBox.content()).substring(textContent->start, textContent->length), style);

            auto& parentInlineBox = lineBox.inlineLevelBoxForLayoutBox(textBox.parent());
            auto enclosingAscentDescentForInlineBox = inlineBoxBoundsMap.get(&parentInlineBox);
            enclosingAscentDescentForInlineBox.ascent = std::max(enclosingAscentDescentForInlineBox.ascent, -enclosingAscentDescentForRun.ascent);
            enclosingAscentDescentForInlineBox.descent = std::max(enclosingAscentDescentForInlineBox.descent, enclosingAscentDescentForRun.descent);

            inlineBoxBoundsMap.set(&parentInlineBox, enclosingAscentDescentForInlineBox);
        }
    }

    for (auto entry : inlineBoxBoundsMap) {
        auto* inlineBox = entry.key;
        auto enclosingAscentDescentForInlineBox = entry.value;
        auto inlineBoxLayoutBounds = *inlineBox->layoutBounds();

        // "line-box-container: block" The extended block progression dimension of the root inline box must fit within the line box.
        auto mayShrinkLineBox = inlineBox->isRootInlineBox() ? !lineBoxContain.contains(LineBoxContain::Block) : true;
        auto ascent = mayShrinkLineBox ? enclosingAscentDescentForInlineBox.ascent : std::max(enclosingAscentDescentForInlineBox.ascent, inlineBoxLayoutBounds.ascent);
        auto descent = mayShrinkLineBox ? enclosingAscentDescentForInlineBox.descent : std::max(enclosingAscentDescentForInlineBox.descent, inlineBoxLayoutBounds.descent);
        inlineBox->setLayoutBounds({ ceilf(ascent), ceilf(descent) });
    }
}

void LineBoxBuilder::adjustIdeographicBaselineIfApplicable(LineBox& lineBox)
{
    // Re-compute the ascent/descent values for the inline boxes on the line (including the root inline box)
    // when the style/content needs ideographic baseline setup in vertical writing mode.
    auto& rootInlineBox = lineBox.rootInlineBox();

    auto lineNeedsIdeographicBaseline = [&] {
        auto styleToUse = [&] (auto& inlineLevelBox) -> const RenderStyle& {
            return isFirstLine() ? inlineLevelBox.layoutBox().firstLineStyle() : inlineLevelBox.layoutBox().style();
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

        if (inlineLevelBox.isInlineBox() || inlineLevelBox.isLineBreakBox() || (inlineLevelBox.isListMarker() && !downcast<ElementBox>(inlineLevelBox.layoutBox()).isListMarkerImage()))
            setVerticalPropertiesForInlineLevelBox(lineBox, inlineLevelBox);
        else if (inlineLevelBox.isAtomicInlineLevelBox()) {
            auto inlineLevelBoxHeight = inlineLevelBox.logicalHeight();
            InlineLayoutUnit ideographicBaseline = roundToInt(inlineLevelBoxHeight / 2);
            // Move the baseline position but keep the same logical height.
            inlineLevelBox.setAscent(ideographicBaseline);
        }

        auto needsFontFallbackAdjustment = inlineLevelBox.isInlineBox();
        if (needsFontFallbackAdjustment) {
            if (auto fallbackFonts = m_fallbackFontsForInlineBoxes.get(&inlineLevelBox); !fallbackFonts.isEmpty() && inlineLevelBox.isPreferredLineHeightFontMetricsBased())
                inlineLevelBox.setLayoutBounds(adjustedLayoutBoundsWithFallbackFonts(inlineLevelBox, fallbackFonts, IdeographicBaseline));
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

void LineBoxBuilder::computeLineBoxGeometry(LineBox& lineBox) const
{
    auto lineBoxLogicalHeight = LineBoxVerticalAligner { formattingContext() }.computeLogicalHeightAndAlign(lineBox);

    auto& rootStyle = this->rootStyle();
    auto shouldTrimBlockStartOfLineBox = isFirstLine() && m_leadingTrim.contains(BlockLayoutState::LeadingTrimSide::Start) && rootStyle.textEdge().over != TextEdgeType::Leading;
    auto shouldTrimBlockEndOfLineBox = isLastLine() && m_leadingTrim.contains(BlockLayoutState::LeadingTrimSide::End) && rootStyle.textEdge().under != TextEdgeType::Leading;

    if (shouldTrimBlockEndOfLineBox) {
        auto textEdgeUnderHeight = [&] {
            auto& rootInlineBox = lineBox.rootInlineBox();
            switch (rootStyle.textEdge().under) {
            case TextEdgeType::Text:
                return rootInlineBox.layoutBounds()->descent - *rootInlineBox.descent();
            case TextEdgeType::Alphabetic:
                return rootInlineBox.layoutBounds()->descent;
            case TextEdgeType::CJKIdeographic:
            case TextEdgeType::CJKIdeographicInk:
                ASSERT_NOT_IMPLEMENTED_YET();
                return 0.f;
            case TextEdgeType::Leading:
            default:
                ASSERT_NOT_REACHED();
                return 0.f;
            }
        }();
        lineBoxLogicalHeight -= std::max(0.f, textEdgeUnderHeight);
    }
    if (shouldTrimBlockStartOfLineBox) {
        auto& rootInlineBox = lineBox.rootInlineBox();
        auto textEdgeOverHeight = [&] {
            switch (rootStyle.textEdge().over) {
            case TextEdgeType::Text:
                return rootInlineBox.layoutBounds()->ascent - rootInlineBox.ascent();
            case TextEdgeType::CapHeight:
                return rootInlineBox.layoutBounds()->ascent - rootInlineBox.primarymetricsOfPrimaryFont().floatCapHeight();
            case TextEdgeType::ExHeight:
                return rootInlineBox.layoutBounds()->ascent - rootInlineBox.primarymetricsOfPrimaryFont().xHeight();
            case TextEdgeType::CJKIdeographic:
            case TextEdgeType::CJKIdeographicInk:
                ASSERT_NOT_IMPLEMENTED_YET();
                return 0.f;
            case TextEdgeType::Leading:
            default:
                ASSERT_NOT_REACHED();
                return 0.f;
            }
        }();
        lineBoxLogicalHeight -= std::max(0.f, textEdgeOverHeight);

        rootInlineBox.setLogicalTop(rootInlineBox.logicalTop() - textEdgeOverHeight);
        for (auto& nonRootInlineLevelBox : lineBox.nonRootInlineLevelBoxes())
            nonRootInlineLevelBox.setLogicalTop(nonRootInlineLevelBox.logicalTop() - textEdgeOverHeight);
    }
    return lineBox.setLogicalRect({ lineContent().lineLogicalTopLeft, lineContent().lineLogicalWidth, lineBoxLogicalHeight });
}

}
}

