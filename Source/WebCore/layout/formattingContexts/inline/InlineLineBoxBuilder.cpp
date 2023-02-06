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

LineBoxBuilder::LineBoxBuilder(const InlineFormattingContext& inlineFormattingContext, const LineBuilder::LineContent& lineContent, const BlockLayoutState& blockLayoutState)
    : m_inlineFormattingContext(inlineFormattingContext)
    , m_lineContent(lineContent)
    , m_blockLayoutState(blockLayoutState)
{
}

LineBox LineBoxBuilder::build(size_t lineIndex)
{
    auto& lineContent = this->lineContent();
    // FIXME: The overflowing hanging content should be part of the ink overflow.
    auto contentLogicalWidth = [&] {
        if (lineContent.inlineBaseDirection == TextDirection::LTR)
            return lineContent.contentLogicalWidth - lineContent.hangingContent.width;
        // FIXME: Currently clients of the inline iterator interface (editing, selection, DOM etc) can't deal with
        // hanging content offsets when they affect the rest of the content.
        // In left-to-right inline direction, hanging content is always trailing hence the width does not impose offset on the rest of the content
        // while with right-to-left, the hanging content is visually leading (left side of the content) and it does offset the rest of the line.
        // What's missing is a way to tell that while the content starts at the left side of the (visually leading) hanging content
        // the root inline box has an offset, the width of the hanging content (essentially decoupling the content and the root inline box visual left).
        // For now just include the hanging content in the root inline box as if it was not hanging (this is how legacy line layout works).
        return lineContent.contentLogicalWidth;
    };
    auto lineBox = LineBox { rootBox(), lineContent.contentLogicalLeft, contentLogicalWidth(), lineIndex, lineContent.nonSpanningInlineLevelBoxCount };
    constructInlineLevelBoxes(lineBox);
    adjustIdeographicBaselineIfApplicable(lineBox);
    adjustInlineBoxHeightsForLineBoxContainIfApplicable(lineBox);
    computeLineBoxGeometry(lineBox);
    adjustOutsideListMarkersPosition(lineBox);
    return lineBox;
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

static AscentAndDescent ascentAndDescentWithTextEdgeForInlineBox(const InlineLevelBox& inlineBox, const FontMetrics& fontMetrics, FontBaseline fontBaseline)
{
    ASSERT(inlineBox.isInlineBox());

    if (inlineBox.isRootInlineBox())
        return { InlineLayoutUnit(fontMetrics.ascent(fontBaseline)), InlineLayoutUnit(fontMetrics.descent(fontBaseline)) };

    auto ascent = [&]() -> InlineLayoutUnit {
        switch (inlineBox.textEdge().over) {
        case TextEdgeType::Leading:
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
    };

    auto descent = [&]() -> InlineLayoutUnit {
        switch (inlineBox.textEdge().under) {
        case TextEdgeType::Leading:
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
    };
    return { ascent(), descent() };
}

AscentAndDescent LineBoxBuilder::enclosingAscentDescentWithFallbackFonts(const InlineLevelBox& inlineBox, const TextUtil::FallbackFontList& fallbackFontsForContent, FontBaseline fontBaseline) const
{
    ASSERT(!fallbackFontsForContent.isEmpty());
    ASSERT(inlineBox.isInlineBox());

    // https://www.w3.org/TR/css-inline-3/#inline-height
    // When the computed line-height is normal, the layout bounds of an inline box encloses all its glyphs, going from the highest A to the deepest D. 
    auto maxAscent = InlineLayoutUnit { };
    auto maxDescent = InlineLayoutUnit { };
    // If line-height computes to normal and either text-edge is leading or this is the root inline box,
    // the font's line gap metric may also be incorporated into A and D by adding half to each side as half-leading.
    auto shouldUseLineGapToAdjustAscentDescent = inlineBox.isRootInlineBox() || isTextEdgeLeading(inlineBox);
    for (auto* font : fallbackFontsForContent) {
        auto& fontMetrics = font->fontMetrics();
        auto ascentAndDescent = ascentAndDescentWithTextEdgeForInlineBox(inlineBox, fontMetrics, fontBaseline);
        if (shouldUseLineGapToAdjustAscentDescent) {
            auto halfLeading = (fontMetrics.lineSpacing() - ascentAndDescent.height()) / 2;
            ascentAndDescent.ascent += halfLeading;
            ascentAndDescent.descent += halfLeading;
        }
        maxAscent = std::max(maxAscent, ascentAndDescent.ascent);
        maxDescent = std::max(maxDescent, ascentAndDescent.descent);
    }
    // We need floor/ceil to match legacy layout integral positioning.
    return { floorf(maxAscent), ceilf(maxDescent) };
}

void LineBoxBuilder::setLayoutBoundsForInlineBox(InlineLevelBox& inlineBox, FontBaseline fontBaseline) const
{
    ASSERT(inlineBox.isInlineBox());

    auto ascentAndDescent = ascentAndDescentWithTextEdgeForInlineBox(inlineBox, inlineBox.primarymetricsOfPrimaryFont(), fontBaseline);
    auto ascent = ascentAndDescent.ascent;
    auto descent = ascentAndDescent.descent;

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

void LineBoxBuilder::setVerticalPropertiesForInlineLevelBox(const LineBox& lineBox, InlineLevelBox& inlineLevelBox) const
{
    auto setAscentAndDescent = [&] (auto ascentAndDescent, bool applyLegacyRounding = true) {
        auto adjustedAscentAndDescent = applyLegacyRounding ? ascentAndDescent.round() : ascentAndDescent;
        inlineLevelBox.setAscent(adjustedAscentAndDescent.ascent);
        inlineLevelBox.setDescent(adjustedAscentAndDescent.descent);
    };

    if (inlineLevelBox.isInlineBox()) {
        setLayoutBoundsForInlineBox(inlineLevelBox, lineBox.baselineType());
        auto ascentAndDescent = [&]() -> AscentAndDescent {
            auto leadingTrim = inlineLevelBox.leadingTrim();
            auto fontBaseline = lineBox.baselineType();
            if (inlineLevelBox.isRootInlineBox() || leadingTrim == LeadingTrim::Normal)
                return primaryFontMetricsForInlineBox(inlineLevelBox, fontBaseline);

            auto& fontMetrics = inlineLevelBox.primarymetricsOfPrimaryFont();
            auto ascentAndDescent = ascentAndDescentWithTextEdgeForInlineBox(inlineLevelBox, fontMetrics, fontBaseline);
            auto ascent = leadingTrim == LeadingTrim::End ? fontMetrics.ascent(fontBaseline) : ascentAndDescent.ascent;
            auto descent = leadingTrim == LeadingTrim::Start ? fontMetrics.descent(fontBaseline) : ascentAndDescent.descent;
            return { ascent, descent };
        }();

        setAscentAndDescent(ascentAndDescent);
        inlineLevelBox.setLogicalHeight(ascentAndDescent.height());

        // With leading-trim, the inline box top is not always where the content starts.
        auto fontMetricBasedAscent = primaryFontMetricsForInlineBox(inlineLevelBox, lineBox.baselineType()).ascent;
        inlineLevelBox.setInlineBoxContentOffsetForLeadingTrim(fontMetricBasedAscent - ascentAndDescent.ascent);
        return;
    }
    if (inlineLevelBox.isLineBreakBox()) {
        auto parentAscentAndDescent = primaryFontMetricsForInlineBox(lineBox.parentInlineBox(inlineLevelBox), lineBox.baselineType());
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
            setAscentAndDescent(primaryFontMetricsForInlineBox(lineBox.parentInlineBox(inlineLevelBox), lineBox.baselineType()));
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
    auto& runs = lineContent().runs;
    for (size_t index = 0; index < runs.size(); ++index) {
        auto& run = runs[index];
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
            logicalLeft += std::max(0_lu, inlineLevelBoxGeometry.marginStart());
            auto atomicInlineLevelBox = InlineLevelBox::createAtomicInlineLevelBox(layoutBox, style, logicalLeft, inlineLevelBoxGeometry.borderBoxWidth());
            setVerticalPropertiesForInlineLevelBox(lineBox, atomicInlineLevelBox);
            lineBox.addInlineLevelBox(WTFMove(atomicInlineLevelBox));
            continue;
        }
        if (run.isLineSpanningInlineBoxStart()) {
            auto marginStart = LayoutUnit { };
#if ENABLE(CSS_BOX_DECORATION_BREAK)
            if (style.boxDecorationBreak() == BoxDecorationBreak::Clone)
                marginStart = formattingContext().geometryForBox(layoutBox).marginStart();
#endif
            logicalLeft += std::max(0_lu, marginStart);
            auto logicalWidth = rootInlineBox.logicalRight() - logicalLeft;
            auto inlineBox = InlineLevelBox::createInlineBox(layoutBox, style, logicalLeft, logicalWidth, InlineLevelBox::LineSpanningInlineBox::Yes);
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
            auto initialLogicalWidth = rootInlineBox.logicalRight() - logicalLeft;
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
            auto& inlineBox = lineBox.inlineLevelBoxFor(run);
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
            auto& parentInlineBox = lineBox.parentInlineBox(run);
            parentInlineBox.setHasContent();
            if (auto fallbackFonts = collectFallbackFonts(parentInlineBox, run, style); !fallbackFonts.isEmpty()) {
                // Adjust non-empty inline box height when glyphs from the non-primary font stretch the box.
                if (parentInlineBox.isPreferredLineHeightFontMetricsBased()) {
                    auto enclosingAscentAndDescent = enclosingAscentDescentWithFallbackFonts(parentInlineBox, fallbackFonts, AlphabeticBaseline);
                    auto layoutBounds = *parentInlineBox.layoutBounds();
                    parentInlineBox.setLayoutBounds({ std::max(layoutBounds.ascent, enclosingAscentAndDescent.ascent), std::max(layoutBounds.descent, enclosingAscentAndDescent.descent) });
                }
            }
            continue;
        }
        if (run.isSoftLineBreak()) {
            lineBox.parentInlineBox(run).setHasContent();
            continue;
        }
        if (run.isHardLineBreak()) {
            auto lineBreakBox = InlineLevelBox::createLineBreakBox(layoutBox, style, logicalLeft);
            setVerticalPropertiesForInlineLevelBox(lineBox, lineBreakBox);
            lineBox.addInlineLevelBox(WTFMove(lineBreakBox));

            if (layoutState().inStandardsMode() || InlineFormattingQuirks::lineBreakBoxAffectsParentInlineBox(lineBox))
                lineBox.parentInlineBox(run).setHasContent();
            continue;
        }
        if (run.isListMarker()) {
            auto& listMarkerBox = downcast<ElementBox>(layoutBox);
            if (!listMarkerBox.isListMarkerImage()) {
                // Non-image type of list markers make their parent inline boxes (e.g. root inline box) contentful (and stretch them vertically).
                lineBox.parentInlineBox(run).setHasContent();
            }

            if (listMarkerBox.isListMarkerOutside())
                m_outsideListMarkers.append(index);

            auto atomicInlineLevelBox = InlineLevelBox::createAtomicInlineLevelBox(listMarkerBox, style, logicalLeft, formattingContext().geometryForBox(listMarkerBox).borderBoxWidth());
            setVerticalPropertiesForInlineLevelBox(lineBox, atomicInlineLevelBox);
            lineBox.addInlineLevelBox(WTFMove(atomicInlineLevelBox));
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
                auto enclosingAscentAndDescent = enclosingAscentDescentWithFallbackFonts(inlineBox, fallbackFonts, lineBox.baselineType());
                ascent = std::max(ascent, enclosingAscentAndDescent.ascent);
                descent = std::max(descent, enclosingAscentAndDescent.descent);
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

            auto& parentInlineBox = lineBox.parentInlineBox(run);
            auto enclosingAscentDescentForInlineBox = inlineBoxBoundsMap.get(&parentInlineBox);
            enclosingAscentDescentForInlineBox.ascent = std::max(enclosingAscentDescentForInlineBox.ascent, -enclosingAscentDescentForRun.ascent);
            enclosingAscentDescentForInlineBox.descent = std::max(enclosingAscentDescentForInlineBox.descent, enclosingAscentDescentForRun.descent);

            inlineBoxBoundsMap.set(&parentInlineBox, enclosingAscentDescentForInlineBox);
        }
    }

    if (lineBoxContain.contains(LineBoxContain::InitialLetter)) {
        // Initial letter contain is based on the font metrics cap geometry and we hug descent.
        auto& rootInlineBox = lineBox.rootInlineBox();
        auto& fontMetrics = rootInlineBox.primarymetricsOfPrimaryFont();
        InlineLayoutUnit initialLetterAscent = fontMetrics.capHeight();
        auto initialLetterDescent = InlineLayoutUnit { };

        for (auto run : lineContent().runs) {
            // We really should only have one text run for initial letter.
            if (!run.isText())
                continue;

            auto& textBox = downcast<InlineTextBox>(run.layoutBox());
            auto textContent = run.textContent();
            auto& style = isFirstLine() ? textBox.firstLineStyle() : textBox.style();
            auto ascentAndDescent = TextUtil::enclosingGlyphBoundsForText(StringView(textBox.content()).substring(textContent->start, textContent->length), style);

            initialLetterDescent = ascentAndDescent.descent;
            if (lineBox.baselineType() != AlphabeticBaseline)
                initialLetterAscent = -ascentAndDescent.ascent;
            break;
        }
        inlineBoxBoundsMap.set(&rootInlineBox, TextUtil::EnclosingAscentDescent { initialLetterAscent, initialLetterDescent });
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
            if (auto fallbackFonts = m_fallbackFontsForInlineBoxes.get(&inlineLevelBox); !fallbackFonts.isEmpty() && inlineLevelBox.isPreferredLineHeightFontMetricsBased()) {
                auto enclosingAscentAndDescent = enclosingAscentDescentWithFallbackFonts(inlineLevelBox, fallbackFonts, IdeographicBaseline);
                auto layoutBounds = *inlineLevelBox.layoutBounds();
                inlineLevelBox.setLayoutBounds({ std::max(layoutBounds.ascent, enclosingAscentAndDescent.ascent), std::max(layoutBounds.descent, enclosingAscentAndDescent.descent) });
            }
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
    auto leadingTrim = blockLayoutState().leadingTrim();
    auto shouldTrimBlockStartOfLineBox = isFirstLine() && leadingTrim.contains(BlockLayoutState::LeadingTrimSide::Start) && rootStyle.textEdge().over != TextEdgeType::Leading;
    auto shouldTrimBlockEndOfLineBox = isLastLine() && leadingTrim.contains(BlockLayoutState::LeadingTrimSide::End) && rootStyle.textEdge().under != TextEdgeType::Leading;

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
    lineBox.setLogicalRect({ lineContent().lineLogicalTopLeft, lineContent().lineLogicalWidth, lineBoxLogicalHeight });
}

void LineBoxBuilder::adjustOutsideListMarkersPosition(LineBox& lineBox)
{
    auto& formattingContext = this->formattingContext();
    auto& formattingState = formattingContext.formattingState();
    auto& formattingGeometry = formattingContext.formattingGeometry();

    auto floatingContext = FloatingContext { formattingContext, blockLayoutState().floatingState() };
    auto lineBoxRect = lineBox.logicalRect();
    auto floatConstraints = floatingContext.constraints(LayoutUnit { lineBoxRect.top() }, LayoutUnit { lineBoxRect.bottom() }, FloatingContext::MayBeAboveLastFloat::No);

    auto lineBoxOffset = lineBoxRect.left() - lineContent().lineInitialLogicalLeftIncludingIntrusiveFloats;
    auto rootInlineBoxLogicalLeft = lineBox.logicalRectForRootInlineBox().left();
    auto rootInlineBoxOffsetFromContentBoxOrIntrusiveFloat = lineBoxOffset + rootInlineBoxLogicalLeft;
    for (auto listMarkerBoxIndex : m_outsideListMarkers) {
        auto& listMarkerRun = lineContent().runs[listMarkerBoxIndex];
        ASSERT(listMarkerRun.isListMarker());
        auto& listMarkerBox = downcast<ElementBox>(listMarkerRun.layoutBox());
        ASSERT(listMarkerBox.isListMarkerOutside());
        auto& listMarkerInlineLevelBox = lineBox.inlineLevelBoxFor(listMarkerRun);
        // Move it to the logical left of the line box (from the logical left of the root inline box).
        auto listMarkerInitialOffsetFromRootInlineBox = listMarkerInlineLevelBox.logicalLeft() - rootInlineBoxOffsetFromContentBoxOrIntrusiveFloat;
        auto logicalLeft = listMarkerInitialOffsetFromRootInlineBox;
        auto nestedListMarkerMarginStart = [&] {
            auto nestedOffset = formattingState.nestedListMarkerOffset(listMarkerBox);
            if (nestedOffset == LayoutUnit::min())
                return 0_lu;
            // Nested list markers (in standards mode) share the same line and have offsets as if they had dedicated lines.
            // <!DOCTYPE html>
            // <ul><li><ul><li>markers on the same line in standards mode
            // vs.
            // <ul><li><ul><li>markers with dedicated lines in quirks mode
            // or
            // <!DOCTYPE html>
            // <ul><li>markers<ul><li>with dedicated lines
            // While a float may not constrain the line, it could constrain the nested list marker (being it outside of the line box to the logical left).  
            // FIXME: We may need to do this in a post-process task after the line box geometry is computed.
            return floatConstraints.left ? std::min(0_lu, std::max(floatConstraints.left->x, nestedOffset)) : nestedOffset;
        }();
        formattingGeometry.adjustMarginStartForListMarker(listMarkerBox, nestedListMarkerMarginStart, rootInlineBoxOffsetFromContentBoxOrIntrusiveFloat);
        logicalLeft += nestedListMarkerMarginStart;
        listMarkerInlineLevelBox.setLogicalLeft(logicalLeft);
    }
}

}
}

