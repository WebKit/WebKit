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

#include "CSSLineBoxContainValue.h"
#include "InlineLevelBoxInlines.h"
#include "InlineLineBoxVerticalAligner.h"
#include "InlineLineBuilder.h"
#include "LayoutBoxGeometry.h"
#include "RenderStyleInlines.h"

namespace WebCore {
namespace Layout {

LineBoxBuilder::LineBoxBuilder(const InlineFormattingContext& inlineFormattingContext, const InlineLayoutState& inlineLayoutState, const LineBuilder::LayoutResult& lineLayoutResult)
    : m_inlineFormattingContext(inlineFormattingContext)
    , m_inlineLayoutState(inlineLayoutState)
    , m_lineLayoutResult(lineLayoutResult)
{
}

LineBox LineBoxBuilder::build(size_t lineIndex)
{
    auto& lineLayoutResult = this->lineLayoutResult();
    // FIXME: The overflowing hanging content should be part of the ink overflow.
    auto contentLogicalWidth = [&] {
        if (lineLayoutResult.directionality.inlineBaseDirection == TextDirection::LTR)
            return lineLayoutResult.contentGeometry.logicalWidth - lineLayoutResult.hangingContent.logicalWidth;
        // FIXME: Currently clients of the inline iterator interface (editing, selection, DOM etc) can't deal with
        // hanging content offsets when they affect the rest of the content.
        // In left-to-right inline direction, hanging content is always trailing hence the width does not impose offset on the rest of the content
        // while with right-to-left, the hanging content is visually leading (left side of the content) and it does offset the rest of the line.
        // What's missing is a way to tell that while the content starts at the left side of the (visually leading) hanging content
        // the root inline box has an offset, the width of the hanging content (essentially decoupling the content and the root inline box visual left).
        // For now just include the hanging content in the root inline box as if it was not hanging (this is how legacy line layout works).
        return lineLayoutResult.contentGeometry.logicalWidth;
    };
    auto lineBox = LineBox { rootBox(), lineLayoutResult.contentGeometry.logicalLeft, contentLogicalWidth(), lineIndex, lineLayoutResult.nonSpanningInlineLevelBoxCount };
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

static InlineLevelBox::AscentAndDescent primaryFontMetricsForInlineBox(const InlineLevelBox& inlineBox, FontBaseline fontBaseline = AlphabeticBaseline)
{
    ASSERT(inlineBox.isInlineBox());
    auto& fontMetrics = inlineBox.primarymetricsOfPrimaryFont();
    InlineLayoutUnit ascent = fontMetrics.ascent(fontBaseline);
    InlineLayoutUnit descent = fontMetrics.descent(fontBaseline);
    return { ascent, descent };
}

static bool isTextBoxEdgeLeading(const InlineLevelBox& inlineBox)
{
    ASSERT(inlineBox.isInlineBox());
    auto textBoxEdge = inlineBox.textBoxEdge();
    ASSERT(textBoxEdge.over != TextBoxEdgeType::Leading || textBoxEdge.under == TextBoxEdgeType::Leading);
    return textBoxEdge.over == TextBoxEdgeType::Leading;
}

static InlineLevelBox::AscentAndDescent ascentAndDescentWithTextBoxEdgeForInlineBox(const InlineLevelBox& inlineBox, const FontMetrics& fontMetrics, FontBaseline fontBaseline)
{
    ASSERT(inlineBox.isInlineBox());

    if (inlineBox.isRootInlineBox())
        return { InlineLayoutUnit(fontMetrics.ascent(fontBaseline)), InlineLayoutUnit(fontMetrics.descent(fontBaseline)) };

    auto ascent = [&]() -> InlineLayoutUnit {
        switch (inlineBox.textBoxEdge().over) {
        case TextBoxEdgeType::Leading:
        case TextBoxEdgeType::Text:
            return fontMetrics.ascent(fontBaseline);
        case TextBoxEdgeType::CapHeight:
            return fontMetrics.floatCapHeight();
        case TextBoxEdgeType::ExHeight:
            return fontMetrics.xHeight();
        case TextBoxEdgeType::CJKIdeographic:
            return fontMetrics.ascent(IdeographicBaseline);
        case TextBoxEdgeType::CJKIdeographicInk:
            ASSERT_NOT_IMPLEMENTED_YET();
            return fontMetrics.ascent(IdeographicBaseline);
        default:
            ASSERT_NOT_REACHED();
            return fontMetrics.ascent(fontBaseline);
        }
    };

    auto descent = [&]() -> InlineLayoutUnit {
        switch (inlineBox.textBoxEdge().under) {
        case TextBoxEdgeType::Leading:
        case TextBoxEdgeType::Text:
            return fontMetrics.descent(fontBaseline);
        case TextBoxEdgeType::Alphabetic:
            return 0.f;
        case TextBoxEdgeType::CJKIdeographic:
            return fontMetrics.descent(IdeographicBaseline);
        case TextBoxEdgeType::CJKIdeographicInk:
            ASSERT_NOT_IMPLEMENTED_YET();
            return fontMetrics.descent(IdeographicBaseline);
        default:
            ASSERT_NOT_REACHED();
            return fontMetrics.descent(fontBaseline);
        }
    };
    return { ascent(), descent() };
}

InlineLevelBox::AscentAndDescent LineBoxBuilder::enclosingAscentDescentWithFallbackFonts(const InlineLevelBox& inlineBox, const TextUtil::FallbackFontList& fallbackFontsForContent, FontBaseline fontBaseline) const
{
    ASSERT(!fallbackFontsForContent.isEmpty());
    ASSERT(inlineBox.isInlineBox());

    // https://www.w3.org/TR/css-inline-3/#inline-height
    // When the computed line-height is normal, the layout bounds of an inline box encloses all its glyphs, going from the highest A to the deepest D. 
    auto maxAscent = InlineLayoutUnit { };
    auto maxDescent = InlineLayoutUnit { };
    // If line-height computes to normal and either text-box-edge is leading or this is the root inline box,
    // the font's line gap metric may also be incorporated into A and D by adding half to each side as half-leading.
    auto shouldUseLineGapToAdjustAscentDescent = inlineBox.isRootInlineBox() || isTextBoxEdgeLeading(inlineBox);
    for (auto* font : fallbackFontsForContent) {
        auto& fontMetrics = font->fontMetrics();
        auto ascentAndDescent = ascentAndDescentWithTextBoxEdgeForInlineBox(inlineBox, fontMetrics, fontBaseline);
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

    auto ascentAndDescent = ascentAndDescentWithTextBoxEdgeForInlineBox(inlineBox, inlineBox.primarymetricsOfPrimaryFont(), fontBaseline);
    auto ascent = ascentAndDescent.ascent;
    auto descent = ascentAndDescent.descent;

    if (!inlineBox.isPreferredLineHeightFontMetricsBased()) {
        // https://www.w3.org/TR/css-inline-3/#inline-height
        // When computed line-height is not normal, calculate the leading L as L = line-height - (A + D).
        // Half the leading (its half-leading) is added above A, and the other half below D,
        // giving an effective ascent above the baseline of A′ = A + L/2, and an effective descent of D′ = D + L/2.
        auto halfLeading = (inlineBox.preferredLineHeight() - (ascent + descent)) / 2;
        if (!isTextBoxEdgeLeading(inlineBox) && !inlineBox.isRootInlineBox()) {
            // However, if text-box-edge is not leading and this is not the root inline box, if the half-leading is positive, treat it as zero.
            halfLeading = std::min(halfLeading, 0.f);
        }
        ascent += halfLeading;
        descent += halfLeading;
    } else {
        // https://www.w3.org/TR/css-inline-3/#inline-height
        // If line-height computes to normal and either text-box-edge is leading or this is the root inline box,
        // the font’s line gap metric may also be incorporated into A and D by adding half to each side as half-leading.
        auto shouldIncorporateHalfLeading = inlineBox.isRootInlineBox() || isTextBoxEdgeLeading(inlineBox);
        if (shouldIncorporateHalfLeading) {
            InlineLayoutUnit lineGap = inlineBox.primarymetricsOfPrimaryFont().lineSpacing();
            auto halfLeading = (lineGap - (ascent + descent)) / 2;
            ascent += halfLeading;
            descent += halfLeading;
        }
    }
    if (!isTextBoxEdgeLeading(inlineBox) && !inlineBox.isRootInlineBox()) {
        // Additionally, when text-box-edge is not leading, the layout bounds are inflated by the sum of the margin,
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
    auto setVerticalProperties = [&] (InlineLevelBox::AscentAndDescent ascentAndDescent, bool applyLegacyRounding = true) {
        if (applyLegacyRounding)
            ascentAndDescent.round();
        inlineLevelBox.setAscentAndDescent(ascentAndDescent);
        inlineLevelBox.setLayoutBounds(ascentAndDescent);
        inlineLevelBox.setLogicalHeight(ascentAndDescent.height());
    };

    if (inlineLevelBox.isInlineBox()) {
        auto ascentAndDescent = [&]() -> InlineLevelBox::AscentAndDescent {
            auto textBoxTrim = inlineLevelBox.textBoxTrim();
            auto fontBaseline = lineBox.baselineType();
            if (inlineLevelBox.isRootInlineBox() || textBoxTrim == TextBoxTrim::None)
                return primaryFontMetricsForInlineBox(inlineLevelBox, fontBaseline);

            auto& fontMetrics = inlineLevelBox.primarymetricsOfPrimaryFont();
            auto ascentAndDescent = ascentAndDescentWithTextBoxEdgeForInlineBox(inlineLevelBox, fontMetrics, fontBaseline);
            auto ascent = textBoxTrim == TextBoxTrim::End ? fontMetrics.ascent(fontBaseline) : ascentAndDescent.ascent;
            auto descent = textBoxTrim == TextBoxTrim::Start ? fontMetrics.descent(fontBaseline) : ascentAndDescent.descent;
            return { ascent, descent };
        }();

        setVerticalProperties(ascentAndDescent);
        // Override default layout bounds.
        setLayoutBoundsForInlineBox(inlineLevelBox, lineBox.baselineType());

        // With text-box-trim, the inline box top is not always where the content starts.
        auto fontMetricBasedAscent = primaryFontMetricsForInlineBox(inlineLevelBox, lineBox.baselineType()).ascent;
        inlineLevelBox.setInlineBoxContentOffsetForTextBoxTrim(fontMetricBasedAscent - ascentAndDescent.ascent);
        return;
    }
    if (inlineLevelBox.isLineBreakBox()) {
        auto parentAscentAndDescent = primaryFontMetricsForInlineBox(lineBox.parentInlineBox(inlineLevelBox), lineBox.baselineType());
        setVerticalProperties(parentAscentAndDescent);
        return;
    }
    if (inlineLevelBox.isListMarker()) {
        auto& layoutBox = downcast<ElementBox>(inlineLevelBox.layoutBox());
        auto& listMarkerBoxGeometry = formattingContext().geometryForBox(layoutBox);
        auto marginBoxHeight = listMarkerBoxGeometry.marginBoxHeight();

        if (lineBox.baselineType() == IdeographicBaseline) {
            // FIXME: We should rely on the integration baseline.
            setVerticalProperties(primaryFontMetricsForInlineBox(lineBox.parentInlineBox(inlineLevelBox), lineBox.baselineType()));
            inlineLevelBox.setLogicalHeight(marginBoxHeight);
            return;
        }
        if (auto ascent = layoutBox.baselineForIntegration()) {
            if (layoutBox.isListMarkerImage())
                return setVerticalProperties({ *ascent, marginBoxHeight - *ascent });
            // Special list marker handling. Text driven list markers behave as text when it comes to layout bounds/ascent descent.
            // This needs to consult the list marker's style (and not the root) because we don't follow the DOM insertion point in case like this:
            // <li><div>content</div></li>
            // where the list marker ends up inside the <div> and the <div>'s style != <li>'s style.
            inlineLevelBox.setLayoutBounds({ *ascent, layoutBox.style().computedLineHeight() - *ascent });

            auto& fontMetrics = inlineLevelBox.primarymetricsOfPrimaryFont();
            auto fontBaseline = lineBox.baselineType();
            inlineLevelBox.setAscentAndDescent({ InlineLayoutUnit(fontMetrics.ascent(fontBaseline)), InlineLayoutUnit(fontMetrics.descent(fontBaseline)) });

            inlineLevelBox.setLogicalHeight(marginBoxHeight);
            return;
        }
        setVerticalProperties({ marginBoxHeight, { } });
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

                // FIXME: Grab the first/last baseline off of the inline formatting context (display content).
                ASSERT_NOT_IMPLEMENTED_YET();
            }
            return marginBoxHeight;
        }();
        setVerticalProperties({ ascent, marginBoxHeight - ascent }, false);
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
    auto& inlineContent = lineLayoutResult().inlineContent;
    for (size_t index = 0; index < inlineContent.size(); ++index) {
        auto& run = inlineContent[index];
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
            ASSERT(initialLogicalWidth >= 0 || lineLayoutResult().hangingContent.logicalWidth || std::isnan(initialLogicalWidth));
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
                    auto layoutBounds = parentInlineBox.layoutBounds();
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

            if (run.isListMarkerOutside())
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
    // font and Glyphs values affect the "size" of the associated inline boxes (which then affect the line box height).
    auto lineBoxContain = rootBox().style().lineBoxContain();
    // Collect layout bounds based on the contain property and set them on the inline boxes when they are applicable.
    HashMap<InlineLevelBox*, TextUtil::EnclosingAscentDescent> inlineBoxBoundsMap;

    if (lineBoxContain.contains(LineBoxContain::InlineBox)) {
        for (auto& inlineLevelBox : lineBox.nonRootInlineLevelBoxes()) {
            if (!inlineLevelBox.isInlineBox())
                continue;
            auto& inlineBoxGeometry = formattingContext().geometryForBox(inlineLevelBox.layoutBox());
            auto ascent = inlineLevelBox.ascent() + inlineBoxGeometry.marginBorderAndPaddingBefore();
            auto descent = inlineLevelBox.descent() + inlineBoxGeometry.marginBorderAndPaddingAfter();
            inlineBoxBoundsMap.set(&inlineLevelBox, TextUtil::EnclosingAscentDescent { ascent, descent });
        }
    }

    if (lineBoxContain.contains(LineBoxContain::Font)) {
        // Assign font based layout bounds to all inline boxes.
        auto ensureFontMetricsBasedHeight = [&] (auto& inlineBox) {
            ASSERT(inlineBox.isInlineBox());
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
        for (auto run : lineLayoutResult().inlineContent) {
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

        for (auto run : lineLayoutResult().inlineContent) {
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
        auto inlineBoxLayoutBounds = inlineBox->layoutBounds();

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
            inlineLevelBox.setAscentAndDescent({ ideographicBaseline, inlineLevelBoxHeight - ideographicBaseline });
            inlineLevelBox.setLayoutBounds({ ideographicBaseline, inlineLevelBoxHeight - ideographicBaseline });
        }

        auto needsFontFallbackAdjustment = inlineLevelBox.isInlineBox();
        if (needsFontFallbackAdjustment) {
            if (auto fallbackFonts = m_fallbackFontsForInlineBoxes.get(&inlineLevelBox); !fallbackFonts.isEmpty() && inlineLevelBox.isPreferredLineHeightFontMetricsBased()) {
                auto enclosingAscentAndDescent = enclosingAscentDescentWithFallbackFonts(inlineLevelBox, fallbackFonts, IdeographicBaseline);
                auto layoutBounds = inlineLevelBox.layoutBounds();
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
    auto textBoxTrim = blockLayoutState().textBoxTrim();
    auto shouldTrimBlockStartOfLineBox = isFirstLine() && textBoxTrim.contains(BlockLayoutState::TextBoxTrimSide::Start) && rootStyle.textBoxEdge().over != TextBoxEdgeType::Leading;
    auto shouldTrimBlockEndOfLineBox = isLastLine() && textBoxTrim.contains(BlockLayoutState::TextBoxTrimSide::End) && rootStyle.textBoxEdge().under != TextBoxEdgeType::Leading;

    if (shouldTrimBlockEndOfLineBox) {
        auto textBoxEdgeUnderHeight = [&] {
            auto& rootInlineBox = lineBox.rootInlineBox();
            switch (rootStyle.textBoxEdge().under) {
            case TextBoxEdgeType::Text:
                return rootInlineBox.layoutBounds().descent - rootInlineBox.descent();
            case TextBoxEdgeType::Alphabetic:
                return rootInlineBox.layoutBounds().descent;
            case TextBoxEdgeType::CJKIdeographic:
            case TextBoxEdgeType::CJKIdeographicInk:
                ASSERT_NOT_IMPLEMENTED_YET();
                return 0.f;
            case TextBoxEdgeType::Leading:
            default:
                ASSERT_NOT_REACHED();
                return 0.f;
            }
        }();
        lineBoxLogicalHeight -= std::max(0.f, textBoxEdgeUnderHeight);
    }
    if (shouldTrimBlockStartOfLineBox) {
        auto& rootInlineBox = lineBox.rootInlineBox();
        auto textBoxEdgeOverHeight = [&] {
            switch (rootStyle.textBoxEdge().over) {
            case TextBoxEdgeType::Text:
                return rootInlineBox.layoutBounds().ascent - rootInlineBox.ascent();
            case TextBoxEdgeType::CapHeight:
                return rootInlineBox.layoutBounds().ascent - rootInlineBox.primarymetricsOfPrimaryFont().floatCapHeight();
            case TextBoxEdgeType::ExHeight:
                return rootInlineBox.layoutBounds().ascent - rootInlineBox.primarymetricsOfPrimaryFont().xHeight();
            case TextBoxEdgeType::CJKIdeographic:
            case TextBoxEdgeType::CJKIdeographicInk:
                ASSERT_NOT_IMPLEMENTED_YET();
                return 0.f;
            case TextBoxEdgeType::Leading:
            default:
                ASSERT_NOT_REACHED();
                return 0.f;
            }
        }();
        lineBoxLogicalHeight -= std::max(0.f, textBoxEdgeOverHeight);

        rootInlineBox.setLogicalTop(rootInlineBox.logicalTop() - textBoxEdgeOverHeight);
        for (auto& nonRootInlineLevelBox : lineBox.nonRootInlineLevelBoxes())
            nonRootInlineLevelBox.setLogicalTop(nonRootInlineLevelBox.logicalTop() - textBoxEdgeOverHeight);
    }
    lineBox.setLogicalRect({ lineLayoutResult().lineGeometry.logicalTopLeft, lineLayoutResult().lineGeometry.logicalWidth, lineBoxLogicalHeight });
}

void LineBoxBuilder::adjustOutsideListMarkersPosition(LineBox& lineBox)
{
    auto floatingContext = FloatingContext { formattingContext(), blockLayoutState().floatingState() };
    auto lineBoxRect = lineBox.logicalRect();
    auto floatConstraints = floatingContext.constraints(LayoutUnit { lineBoxRect.top() }, LayoutUnit { lineBoxRect.bottom() }, FloatingContext::MayBeAboveLastFloat::No);

    auto lineBoxOffset = lineBoxRect.left() - lineLayoutResult().lineGeometry.initialLogicalLeftIncludingIntrusiveFloats;
    auto rootInlineBoxLogicalLeft = lineBox.logicalRectForRootInlineBox().left();
    auto rootInlineBoxOffsetFromContentBoxOrIntrusiveFloat = lineBoxOffset + rootInlineBoxLogicalLeft;
    for (auto listMarkerBoxIndex : m_outsideListMarkers) {
        auto& listMarkerRun = lineLayoutResult().inlineContent[listMarkerBoxIndex];
        ASSERT(listMarkerRun.isListMarkerOutside());
        auto& listMarkerBox = downcast<ElementBox>(listMarkerRun.layoutBox());
        auto& listMarkerInlineLevelBox = lineBox.inlineLevelBoxFor(listMarkerRun);
        // Move it to the logical left of the line box (from the logical left of the root inline box).
        auto listMarkerInitialOffsetFromRootInlineBox = listMarkerInlineLevelBox.logicalLeft() - rootInlineBoxOffsetFromContentBoxOrIntrusiveFloat;
        auto logicalLeft = listMarkerInitialOffsetFromRootInlineBox;
        auto nestedListMarkerMarginStart = [&] {
            auto nestedOffset = inlineLayoutState().nestedListMarkerOffset(listMarkerBox);
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
        adjustMarginStartForListMarker(listMarkerBox, nestedListMarkerMarginStart, rootInlineBoxOffsetFromContentBoxOrIntrusiveFloat);
        logicalLeft += nestedListMarkerMarginStart;
        listMarkerInlineLevelBox.setLogicalLeft(logicalLeft);
    }
}

void LineBoxBuilder::adjustMarginStartForListMarker(const ElementBox& listMarkerBox, LayoutUnit nestedListMarkerMarginStart, InlineLayoutUnit rootInlineBoxOffset) const
{
    if (!nestedListMarkerMarginStart && !rootInlineBoxOffset)
        return;
    auto& listMarkerGeometry = const_cast<InlineFormattingState&>(formattingContext().formattingState()).boxGeometry(listMarkerBox);
    // Make sure that the line content does not get pulled in to logical left direction due to
    // the large negative margin (i.e. this ensures that logical left of the list content stays at the line start)
    listMarkerGeometry.setHorizontalMargin({ listMarkerGeometry.marginStart() + nestedListMarkerMarginStart - LayoutUnit { rootInlineBoxOffset }, listMarkerGeometry.marginEnd() - nestedListMarkerMarginStart + LayoutUnit { rootInlineBoxOffset } });
}

}
}

