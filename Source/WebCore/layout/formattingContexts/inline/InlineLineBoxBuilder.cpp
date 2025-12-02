/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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

#include "InlineLevelBoxInlines.h"
#include "InlineLineBoxVerticalAligner.h"
#include "InlineLineBuilder.h"
#include "LayoutBoxGeometry.h"
#include "RenderStyleInlines.h"
#include "RubyFormattingContext.h"
#include "StyleWebKitLineBoxContain.h"

namespace WebCore {
namespace Layout {

LineBoxBuilder::LineBoxBuilder(const InlineFormattingContext& inlineFormattingContext, LineLayoutResult& lineLayoutResult)
    : m_inlineFormattingContext(inlineFormattingContext)
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
    auto lineBox = LineBox { rootBox(), lineLayoutResult.contentGeometry.logicalLeft, contentLogicalWidth(), lineIndex, isFirstFormattedLine(), lineLayoutResult.nonSpanningInlineLevelBoxCount };
    if (lineLayoutResult.hasBlockContent())
        constructBlockContent(lineBox);
    else {
        constructInlineLevelBoxes(lineBox);
        if (lineBox.hasContent()) {
            adjustIdeographicBaselineIfApplicable(lineBox);
            adjustInlineBoxHeightsForLineBoxContainIfApplicable(lineBox);
        } else {
            // Collapse all inline boxes (they are supposed to be empty as well).
            for (auto& inlineBox : lineBox.nonRootInlineLevelBoxes()) {
                if (!inlineBox.isInlineBox()) {
                    ASSERT(inlineBox.layoutBox().isWordBreakOpportunity());
                    ASSERT(!inlineBox.logicalHeight());
                    continue;
                }
                ASSERT(!inlineBox.hasContent());
                inlineBox.setLogicalHeight({ });
            }
        }
        if (m_lineHasNonLineSpanningRubyContent)
            RubyFormattingContext::applyAnnotationContributionToLayoutBounds(lineBox, formattingContext());
        computeLineBoxGeometry(lineBox);
        adjustOutsideListMarkersPosition(lineBox);

        if (auto adjustment = formattingContext().quirks().adjustmentForLineGridLineSnap(lineBox))
            expandAboveRootInlineBox(lineBox, *adjustment);
    }

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
    if (fallbackFonts.isEmptyIgnoringNullReferences())
        return { };

    auto fallbackFontsForInlineBoxes = m_fallbackFontsForInlineBoxes.get(&parentInlineBox);
    auto numberOfFallbackFontsForInlineBox = fallbackFontsForInlineBoxes.computeSize();
    for (auto& font : fallbackFonts) {
        fallbackFontsForInlineBoxes.add(font);
        m_fallbackFontRequiresIdeographicBaseline = m_fallbackFontRequiresIdeographicBaseline || font.hasVerticalGlyphs();
    }
    if (fallbackFontsForInlineBoxes.computeSize() != numberOfFallbackFontsForInlineBox)
        m_fallbackFontsForInlineBoxes.set(&parentInlineBox, fallbackFontsForInlineBoxes);
    return fallbackFonts;
}

static InlineLevelBox::AscentAndDescent primaryFontMetricsForInlineBox(const InlineLevelBox& inlineBox, FontBaseline fontBaseline = FontBaseline::Alphabetic)
{
    ASSERT(inlineBox.isInlineBox());
    auto& fontMetrics = inlineBox.primarymetricsOfPrimaryFont();
    InlineLayoutUnit ascent = fontMetrics.intAscent(fontBaseline);
    InlineLayoutUnit descent = fontMetrics.intDescent(fontBaseline);
    return { ascent, descent };
}

static bool isLineFitEdgeLeading(const InlineLevelBox& inlineBox)
{
    ASSERT(inlineBox.isInlineBox());
    return inlineBox.lineFitEdge().isLeading();
}

static InlineLevelBox::AscentAndDescent layoutBoundstWithEdgeAdjustmentForInlineBox(const InlineLevelBox& inlineBox, const FontMetrics& fontMetrics, FontBaseline fontBaseline)
{
    ASSERT(inlineBox.isInlineBox());

    if (inlineBox.isRootInlineBox())
        return { InlineLayoutUnit(fontMetrics.intAscent(fontBaseline)), InlineLayoutUnit(fontMetrics.intDescent(fontBaseline)) };

    auto ascent = [&] -> InlineLayoutUnit {
        return WTF::switchOn(inlineBox.lineFitEdge(),
            [&](CSS::Keyword::Leading) -> InlineLayoutUnit {
                return fontMetrics.intAscent(fontBaseline);
            },
            [&](Style::TextEdgePair edgePair) -> InlineLayoutUnit {
                switch (edgePair.over) {
                case TextEdgeOver::Text:
                    return fontMetrics.intAscent(fontBaseline);
                case TextEdgeOver::Cap:
                    return fontMetrics.intCapHeight();
                case TextEdgeOver::Ex:
                    return roundf(fontMetrics.xHeight().value_or(0.f));
                case TextEdgeOver::Ideographic:
                    return fontMetrics.intAscent(FontBaseline::Ideographic);
                case TextEdgeOver::IdeographicInk:
                    ASSERT_NOT_IMPLEMENTED_YET();
                    return fontMetrics.intAscent(FontBaseline::Ideographic);
                }
                RELEASE_ASSERT_NOT_REACHED();
            }
        );
    };

    auto descent = [&] -> InlineLayoutUnit {
        return WTF::switchOn(inlineBox.lineFitEdge(),
            [&](CSS::Keyword::Leading) -> InlineLayoutUnit {
                return fontMetrics.intDescent(fontBaseline);
            },
            [&](Style::TextEdgePair edgePair) -> InlineLayoutUnit {
                switch (edgePair.under) {
                case TextEdgeUnder::Text:
                    return fontMetrics.intDescent(fontBaseline);
                case TextEdgeUnder::Alphabetic:
                    return 0.f;
                case TextEdgeUnder::Ideographic:
                    return fontMetrics.intDescent(FontBaseline::Ideographic);
                case TextEdgeUnder::IdeographicInk:
                    ASSERT_NOT_IMPLEMENTED_YET();
                    return fontMetrics.intDescent(FontBaseline::Ideographic);
                }
                RELEASE_ASSERT_NOT_REACHED();
            }
        );
    };
    return { ascent(), descent() };
}

static InlineLevelBox::AscentAndDescent textBoxAdjustedInlineBoxHeight(const InlineLevelBox& inlineBox, const FontMetrics& fontMetrics, FontBaseline fontBaseline)
{
    ASSERT(inlineBox.isInlineBox());

    if (inlineBox.isRootInlineBox())
        return { InlineLayoutUnit(fontMetrics.intAscent(fontBaseline)), InlineLayoutUnit(fontMetrics.intDescent(fontBaseline)) };

    auto inlineBoxTextBoxTrim = inlineBox.textBoxTrim();
    auto ascent = [&] -> InlineLayoutUnit {
        auto textBoxEdge = inlineBoxTextBoxTrim == TextBoxTrim::TrimStart || inlineBoxTextBoxTrim == TextBoxTrim::TrimBoth ? inlineBox.textBoxEdge().tryTextEdgePair() : std::nullopt;
        if (!textBoxEdge)
            return fontMetrics.intAscent(fontBaseline);

        switch (textBoxEdge->over) {
        case TextEdgeOver::Text:
            return fontMetrics.intAscent(fontBaseline);
        case TextEdgeOver::Cap:
            return fontMetrics.intCapHeight();
        case TextEdgeOver::Ex:
            return roundf(fontMetrics.xHeight().value_or(0.f));
        case TextEdgeOver::Ideographic:
            return fontMetrics.intAscent(FontBaseline::Ideographic);
        case TextEdgeOver::IdeographicInk:
            ASSERT_NOT_IMPLEMENTED_YET();
            return fontMetrics.intAscent(FontBaseline::Ideographic);
        }
        RELEASE_ASSERT_NOT_REACHED();
    };

    auto descent = [&] -> InlineLayoutUnit {
        auto textBoxEdge = inlineBoxTextBoxTrim == TextBoxTrim::TrimEnd || inlineBoxTextBoxTrim == TextBoxTrim::TrimBoth ? inlineBox.textBoxEdge().tryTextEdgePair() : std::nullopt;
        if (!textBoxEdge)
            return fontMetrics.intDescent(fontBaseline);

        switch (textBoxEdge->under) {
        case TextEdgeUnder::Text:
            return fontMetrics.intDescent(fontBaseline);
        case TextEdgeUnder::Alphabetic:
            return 0.f;
        case TextEdgeUnder::Ideographic:
            return fontMetrics.intDescent(FontBaseline::Ideographic);
        case TextEdgeUnder::IdeographicInk:
            ASSERT_NOT_IMPLEMENTED_YET();
            return fontMetrics.intDescent(FontBaseline::Ideographic);
        }
        RELEASE_ASSERT_NOT_REACHED();
    };
    return { ascent(), descent() };
}

InlineLevelBox::AscentAndDescent LineBoxBuilder::enclosingAscentDescentWithFallbackFonts(const InlineLevelBox& inlineBox, const TextUtil::FallbackFontList& fallbackFontsForContent, FontBaseline fontBaseline) const
{
    ASSERT(!fallbackFontsForContent.isEmptyIgnoringNullReferences());
    ASSERT(inlineBox.isInlineBox());

    // https://www.w3.org/TR/css-inline-3/#inline-height
    // When the computed line-height is normal, the layout bounds of an inline box encloses all its glyphs, going from the highest A to the deepest D. 
    auto maxAscent = InlineLayoutUnit { };
    auto maxDescent = InlineLayoutUnit { };
    // If line-height computes to normal and either line-fit-edge is leading or this is the root inline box,
    // the font's line gap metric may also be incorporated into A and D by adding half to each side as half-leading.
    auto shouldUseLineGapToAdjustAscentDescent = (inlineBox.isRootInlineBox() || isLineFitEdgeLeading(inlineBox)) && !rootBox().isRubyAnnotationBox();
    for (auto& font : fallbackFontsForContent) {
        auto& fontMetrics = font.fontMetrics();
        auto [ascent, descent] = textBoxAdjustedInlineBoxHeight(inlineBox, fontMetrics, fontBaseline);
        if (shouldUseLineGapToAdjustAscentDescent) {
            auto halfLeading = (fontMetrics.intLineSpacing() - (ascent + descent)) / 2;
            ascent += halfLeading;
            descent += halfLeading;
        }
        maxAscent = std::max(maxAscent, ascent);
        maxDescent = std::max(maxDescent, descent);
    }
    // We need floor/ceil to match legacy layout integral positioning.
    return { floorf(maxAscent), ceilf(maxDescent) };
}

void LineBoxBuilder::setLayoutBoundsForInlineBox(InlineLevelBox& inlineBox, FontBaseline fontBaseline) const
{
    ASSERT(inlineBox.isInlineBox());

    auto layoutBounds = [&]() -> InlineLevelBox::AscentAndDescent {
        auto [ascent, descent] = layoutBoundstWithEdgeAdjustmentForInlineBox(inlineBox, inlineBox.primarymetricsOfPrimaryFont(), fontBaseline);

        // FIXME: Annotation root should not have any impact here with the proper annotation box handling (dedicated IFC line for annotations and line-height is 1).
        if (rootBox().isRubyAnnotationBox())
            return { ascent, descent };

        if (!inlineBox.isPreferredLineHeightFontMetricsBased()) {
            // https://www.w3.org/TR/css-inline-3/#inline-height
            // When computed line-height is not normal, calculate the leading L as L = line-height - (A + D).
            // Half the leading (its half-leading) is added above A, and the other half below D,
            // giving an effective ascent above the baseline of A′ = A + L/2, and an effective descent of D′ = D + L/2.
            auto halfLeading = (floorf(inlineBox.preferredLineHeight()) - (ascent + descent)) / 2;
            if (!isLineFitEdgeLeading(inlineBox) && !inlineBox.isRootInlineBox()) {
                // However, if text-box-edge is not leading and this is not the root inline box, if the half-leading is positive, treat it as zero.
                halfLeading = std::min(halfLeading, 0.f);
            }
            ascent += halfLeading;
            descent += halfLeading;
        } else {
            // https://www.w3.org/TR/css-inline-3/#inline-height
            // If line-height computes to normal and either line-fit-edge is leading or this is the root inline box,
            // the font’s line gap metric may also be incorporated into A and D by adding half to each side as half-leading.
            auto shouldIncorporateHalfLeading = inlineBox.isRootInlineBox() || isLineFitEdgeLeading(inlineBox);
            if (shouldIncorporateHalfLeading) {
                InlineLayoutUnit lineGap = inlineBox.primarymetricsOfPrimaryFont().intLineSpacing();
                auto halfLeading = (lineGap - (ascent + descent)) / 2;
                ascent += halfLeading;
                descent += halfLeading;
            }
        }
        return { ascent, descent };
    }();

    auto inflateWithMarginBorderAndPaddingIfApplicable = [&] {
        if (isLineFitEdgeLeading(inlineBox) || inlineBox.isRootInlineBox())
            return;
        // Additionally, when line-fit-edge is not leading, the layout bounds are inflated by the sum of the margin,
        // border, and padding on each side.
        ASSERT(!inlineBox.isRootInlineBox());
        auto& inlineBoxGeometry = formattingContext().geometryForBox(inlineBox.layoutBox());
        layoutBounds.ascent += inlineBoxGeometry.marginBorderAndPaddingBefore();
        layoutBounds.descent += inlineBoxGeometry.marginBorderAndPaddingAfter();
    };
    inflateWithMarginBorderAndPaddingIfApplicable();

    layoutBounds.round();
    inlineBox.setLayoutBounds(layoutBounds);
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
            auto fontBaseline = lineBox.baselineType();
            if (inlineLevelBox.isRootInlineBox())
                return primaryFontMetricsForInlineBox(inlineLevelBox, fontBaseline);

            auto& fontMetrics = inlineLevelBox.primarymetricsOfPrimaryFont();
            auto [ascent, descent] = textBoxAdjustedInlineBoxHeight(inlineLevelBox, fontMetrics, fontBaseline);
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

        if (lineBox.baselineType() == FontBaseline::Ideographic) {
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
            auto listMarkerLayoutBounds = layoutBox.layoutBoundsForListMarker();
            inlineLevelBox.setLayoutBounds({ InlineLayoutUnit(listMarkerLayoutBounds.first), InlineLayoutUnit(listMarkerLayoutBounds.second) });

            auto& fontMetrics = inlineLevelBox.primarymetricsOfPrimaryFont();
            auto fontBaseline = lineBox.baselineType();
            inlineLevelBox.setAscentAndDescent({ InlineLayoutUnit(fontMetrics.intAscent(fontBaseline)), InlineLayoutUnit(fontMetrics.intDescent(fontBaseline)) });

            inlineLevelBox.setLogicalHeight(marginBoxHeight);
            return;
        }
        setVerticalProperties({ marginBoxHeight, { } });
        return;
    }
    if (inlineLevelBox.isAtomicInlineBox()) {
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
    auto& formattingContext = this->formattingContext();
    auto& rootInlineBox = lineBox.rootInlineBox();
    setVerticalPropertiesForInlineLevelBox(lineBox, rootInlineBox);

    auto styleToUse = [&] (const auto& layoutBox) -> const RenderStyle& {
        return isFirstFormattedLine() ? layoutBox.firstLineStyle() : layoutBox.style();
    };

    auto lineHasContent = false;
    auto& inlineContent = lineLayoutResult().runs;
    for (size_t index = 0; index < inlineContent.size(); ++index) {
        auto& run = inlineContent[index];
        auto& layoutBox = run.layoutBox();
        auto& style = styleToUse(layoutBox);
        lineHasContent = lineHasContent || Line::Run::isContentfulOrHasDecoration(run, formattingContext);
        auto logicalLeft = rootInlineBox.logicalLeft() + run.logicalLeft();

        if (run.isText()) {
            auto& parentInlineBox = lineBox.parentInlineBox(run);
            parentInlineBox.setHasContent();
            if (auto fallbackFonts = collectFallbackFonts(parentInlineBox, run, style); !fallbackFonts.isEmptyIgnoringNullReferences()) {
                // Adjust non-empty inline box height when glyphs from the non-primary font stretch the box.
                if (parentInlineBox.isPreferredLineHeightFontMetricsBased()) {
                    auto enclosingAscentAndDescent = enclosingAscentDescentWithFallbackFonts(parentInlineBox, fallbackFonts, FontBaseline::Alphabetic);
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

            if (layoutState().inStandardsMode() || InlineQuirks::lineBreakBoxAffectsParentInlineBox(lineBox))
                lineBox.parentInlineBox(run).setHasContent();
            continue;
        }
        if (run.isAtomicInlineBox()) {
            auto& inlineLevelBoxGeometry = formattingContext.geometryForBox(layoutBox);
            logicalLeft += std::max(0_lu, inlineLevelBoxGeometry.marginStart());
            auto atomicInlineBox = InlineLevelBox::createAtomicInlineBox(layoutBox, style, logicalLeft, inlineLevelBoxGeometry.borderBoxWidth());
            setVerticalPropertiesForInlineLevelBox(lineBox, atomicInlineBox);
            lineBox.addInlineLevelBox(WTFMove(atomicInlineBox));
            continue;
        }
        if (run.isInlineBoxStart() || run.isLineSpanningInlineBoxStart()) {
            auto marginStart = run.isInlineBoxStart() || style.boxDecorationBreak() == BoxDecorationBreak::Clone ? formattingContext.geometryForBox(layoutBox).marginStart() : LayoutUnit();
            // At this point we don't know yet how wide this inline box is. Let's assume it's as long as the line is
            // and adjust it later if we come across an inlineBoxEnd run (see below).
            // Inline box run is based on margin box. Let's convert it to border box.
            logicalLeft += std::max(0_lu, marginStart);
            auto initialLogicalWidth = [&] {
                auto logicalWidth = rootInlineBox.logicalRight() - logicalLeft;
                // We (editing, DOM etc) can't yet handle RTL hanging content (see contentLogicalWidth in LineBoxBuilder::build).
                if (lineLayoutResult().directionality.inlineBaseDirection == TextDirection::LTR)
                    logicalWidth += lineLayoutResult().hangingContent.logicalWidth;
                return logicalWidth;
            }();
            initialLogicalWidth = std::max(initialLogicalWidth, 0.f);
            auto inlineBox = InlineLevelBox::createInlineBox(layoutBox, style, logicalLeft, initialLogicalWidth, run.isInlineBoxStart() ? InlineLevelBox::LineSpanningInlineBox::No : InlineLevelBox::LineSpanningInlineBox::Yes);
            inlineBox.setTextEmphasis(InlineFormattingUtils::textEmphasisForInlineBox(layoutBox, rootBox()));
            setVerticalPropertiesForInlineLevelBox(lineBox, inlineBox);
            if (run.isInlineBoxStart()) {
                inlineBox.setIsFirstBox();
                m_lineHasNonLineSpanningRubyContent = m_lineHasNonLineSpanningRubyContent || layoutBox.isRubyBase();
            }
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
            auto marginEndAdjustemnt = -formattingContext.geometryForBox(layoutBox).marginEnd();
            auto logicalWidth = run.logicalWidth() + marginEndAdjustemnt;
            auto inlineBoxLogicalRight = logicalLeft + logicalWidth;
            // When the content pulls the </span> to the logical left direction (e.g. negative letter space)
            // make sure we don't end up with negative logical width on the inline box.
            inlineBox.setLogicalWidth(std::max(0.f, inlineBoxLogicalRight - inlineBox.logicalLeft()));
            inlineBox.setIsLastBox();
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

            auto atomicInlineBox = InlineLevelBox::createAtomicInlineBox(listMarkerBox, style, logicalLeft, formattingContext.geometryForBox(listMarkerBox).borderBoxWidth());
            setVerticalPropertiesForInlineLevelBox(lineBox, atomicInlineBox);
            lineBox.addInlineLevelBox(WTFMove(atomicInlineBox));
            continue;
        }
        if (run.isWordBreakOpportunity()) {
            lineBox.addInlineLevelBox(InlineLevelBox::createGenericInlineLevelBox(layoutBox, style, logicalLeft));
            continue;
        }
        ASSERT(run.isOpaque());
    }
    lineBox.setHasContent(lineHasContent);
}

void LineBoxBuilder::constructBlockContent(LineBox& lineBox)
{
    auto& lineLayoutResult = this->lineLayoutResult();
    auto& runs = lineLayoutResult.runs;
    if (runs.isEmpty() || !runs.last().isBlock()) {
        ASSERT_NOT_REACHED();
        return;
    }

    // Since we don't need to position and align block content inside the line, we don't need to create any boxes for this block content.
    auto& blockGeometry = formattingContext().geometryForBox(runs.last().layoutBox());
    for (size_t index = 0;  index < runs.size() - 1; ++index) {
        auto& run = runs[index];
        if (run.isLineSpanningInlineBoxStart()) {
            auto lineSpanningInlineBox = InlineLevelBox::createInlineBox(run.layoutBox(), run.layoutBox().style(), lineLayoutResult.contentGeometry.logicalLeft, lineLayoutResult.contentGeometry.logicalWidth, InlineLevelBox::LineSpanningInlineBox::Yes);
            setVerticalPropertiesForInlineLevelBox(lineBox, lineSpanningInlineBox);
            lineSpanningInlineBox.setLogicalTop(blockGeometry.marginBefore());
            lineSpanningInlineBox.setLogicalHeight(InlineLayoutUnit(blockGeometry.borderBoxHeight()));
            lineBox.addInlineLevelBox(WTFMove(lineSpanningInlineBox));
            continue;
        }
        ASSERT_NOT_REACHED();
    }

    auto blockLineLogicalTopLeft = InlineLayoutPoint { lineLayoutResult.lineGeometry.initialLogicalLeft, lineLayoutResult.lineGeometry.logicalTopLeft.y() };
    lineBox.setLogicalRect({ blockLineLogicalTopLeft, lineLayoutResult.lineGeometry.logicalWidth, blockGeometry.marginBoxHeight() });
    setVerticalPropertiesForInlineLevelBox(lineBox, lineBox.rootInlineBox());
    // FIXME: Let's considered collapsed block boxes contentful for now (webkit.org/b/302804).
    lineBox.setHasContent(true);
}

void LineBoxBuilder::adjustInlineBoxHeightsForLineBoxContainIfApplicable(LineBox& lineBox)
{
    // While line-box-contain normally tells whether a certain type of content should be included when computing the line box height,
    // font and Glyphs values affect the "size" of the associated inline boxes (which then affect the line box height).
    auto lineBoxContain = rootBox().style().lineBoxContain();
    // Collect layout bounds based on the contain property and set them on the inline boxes when they are applicable.
    HashMap<InlineLevelBox*, TextUtil::EnclosingAscentDescent> inlineBoxBoundsMap;

    if (lineBoxContain.contains(Style::WebkitLineBoxContainValue::InlineBox)) {
        for (auto& inlineLevelBox : lineBox.nonRootInlineLevelBoxes()) {
            if (!inlineLevelBox.isInlineBox())
                continue;
            auto& inlineBoxGeometry = formattingContext().geometryForBox(inlineLevelBox.layoutBox());
            auto ascent = inlineLevelBox.ascent() + inlineBoxGeometry.marginBorderAndPaddingBefore();
            auto descent = inlineLevelBox.descent() + inlineBoxGeometry.marginBorderAndPaddingAfter();
            inlineBoxBoundsMap.set(&inlineLevelBox, TextUtil::EnclosingAscentDescent { ascent, descent });
        }
    }

    if (lineBoxContain.contains(Style::WebkitLineBoxContainValue::Font)) {
        // Assign font based layout bounds to all inline boxes.
        auto ensureFontMetricsBasedHeight = [&] (auto& inlineBox) {
            ASSERT(inlineBox.isInlineBox());
            auto [ascent, descent] = primaryFontMetricsForInlineBox(inlineBox, lineBox.baselineType());
            InlineLayoutUnit lineGap = inlineBox.primarymetricsOfPrimaryFont().intLineSpacing();
            auto halfLeading = !rootBox().isRubyAnnotationBox() ? (lineGap - (ascent + descent)) / 2 : 0.f;
            ascent += halfLeading;
            descent += halfLeading;
            if (auto fallbackFonts = m_fallbackFontsForInlineBoxes.get(&inlineBox); !fallbackFonts.isEmptyIgnoringNullReferences()) {
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

    if (lineBoxContain.contains(Style::WebkitLineBoxContainValue::Glyphs)) {
        // Compute text content (glyphs) hugging inline box layout bounds.
        for (auto run : lineLayoutResult().runs) {
            if (!run.isText())
                continue;

            auto& textBox = downcast<InlineTextBox>(run.layoutBox());
            auto textContent = run.textContent();
            auto& style = isFirstFormattedLine() ? textBox.firstLineStyle() : textBox.style();
            auto enclosingAscentDescentForRun = TextUtil::enclosingGlyphBoundsForText(StringView(textBox.content()).substring(textContent->start, textContent->length), style, textBox.shouldUseSimpleGlyphOverflowCodePath() ? TextUtil::ShouldUseSimpleGlyphOverflowCodePath::Yes : TextUtil::ShouldUseSimpleGlyphOverflowCodePath::No);

            auto& parentInlineBox = lineBox.parentInlineBox(run);
            auto enclosingAscentDescentForInlineBox = inlineBoxBoundsMap.get(&parentInlineBox);
            enclosingAscentDescentForInlineBox.ascent = std::max(enclosingAscentDescentForInlineBox.ascent, -enclosingAscentDescentForRun.ascent);
            enclosingAscentDescentForInlineBox.descent = std::max(enclosingAscentDescentForInlineBox.descent, enclosingAscentDescentForRun.descent);

            inlineBoxBoundsMap.set(&parentInlineBox, enclosingAscentDescentForInlineBox);
        }
    }

    if (lineBoxContain.contains(Style::WebkitLineBoxContainValue::InitialLetter)) {
        // Initial letter contain is based on the font metrics cap geometry and we hug descent.
        auto& rootInlineBox = lineBox.rootInlineBox();
        auto& fontMetrics = rootInlineBox.primarymetricsOfPrimaryFont();
        InlineLayoutUnit initialLetterAscent = fontMetrics.intCapHeight();
        auto initialLetterDescent = InlineLayoutUnit { };

        for (auto run : lineLayoutResult().runs) {
            // We really should only have one text run for initial letter.
            if (!run.isText())
                continue;

            auto& textBox = downcast<InlineTextBox>(run.layoutBox());
            auto textContent = run.textContent();
            auto& style = isFirstFormattedLine() ? textBox.firstLineStyle() : textBox.style();
            auto ascentAndDescent = TextUtil::enclosingGlyphBoundsForText(StringView(textBox.content()).substring(textContent->start, textContent->length), style, textBox.shouldUseSimpleGlyphOverflowCodePath() ? TextUtil::ShouldUseSimpleGlyphOverflowCodePath::Yes : TextUtil::ShouldUseSimpleGlyphOverflowCodePath::No);

            initialLetterDescent = ascentAndDescent.descent;
            if (lineBox.baselineType() != FontBaseline::Alphabetic)
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
        auto mayShrinkLineBox = inlineBox->isRootInlineBox() ? !lineBoxContain.contains(Style::WebkitLineBoxContainValue::Block) : true;
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
            return isFirstFormattedLine() ? inlineLevelBox.layoutBox().firstLineStyle() : inlineLevelBox.layoutBox().style();
        };
        auto& rootInlineBoxStyle = styleToUse(rootInlineBox);
        if (rootInlineBoxStyle.writingMode().isHorizontal())
            return false;

        auto primaryFontRequiresIdeographicBaseline = [&] (auto& style) {
            return style.fontDescription().orientation() == FontOrientation::Vertical || style.fontCascade().primaryFont()->hasVerticalGlyphs();
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

    lineBox.setBaselineType(FontBaseline::Ideographic);

    auto adjustLayoutBoundsWithIdeographicBaseline = [&] (auto& inlineLevelBox) {
        auto initiatesLayoutBoundsChange = inlineLevelBox.isInlineBox() || inlineLevelBox.isAtomicInlineBox() || inlineLevelBox.isLineBreakBox();
        if (!initiatesLayoutBoundsChange)
            return;

        if (inlineLevelBox.isInlineBox() || inlineLevelBox.isLineBreakBox() || (inlineLevelBox.isListMarker() && !downcast<ElementBox>(inlineLevelBox.layoutBox()).isListMarkerImage()))
            setVerticalPropertiesForInlineLevelBox(lineBox, inlineLevelBox);
        else if (inlineLevelBox.isAtomicInlineBox()) {
            auto inlineLevelBoxHeight = inlineLevelBox.logicalHeight();
            InlineLayoutUnit ideographicBaseline = roundToInt(inlineLevelBoxHeight / 2);
            // Move the baseline position but keep the same logical height.
            inlineLevelBox.setAscentAndDescent({ ideographicBaseline, inlineLevelBoxHeight - ideographicBaseline });
            inlineLevelBox.setLayoutBounds({ ideographicBaseline, inlineLevelBoxHeight - ideographicBaseline });
        }

        auto needsFontFallbackAdjustment = inlineLevelBox.isInlineBox();
        if (needsFontFallbackAdjustment) {
            if (auto fallbackFonts = m_fallbackFontsForInlineBoxes.get(&inlineLevelBox); !fallbackFonts.isEmptyIgnoringNullReferences() && inlineLevelBox.isPreferredLineHeightFontMetricsBased()) {
                auto enclosingAscentAndDescent = enclosingAscentDescentWithFallbackFonts(inlineLevelBox, fallbackFonts, FontBaseline::Ideographic);
                auto layoutBounds = inlineLevelBox.layoutBounds();
                inlineLevelBox.setLayoutBounds({ std::max(layoutBounds.ascent, enclosingAscentAndDescent.ascent), std::max(layoutBounds.descent, enclosingAscentAndDescent.descent) });
            }
        }
    };

    adjustLayoutBoundsWithIdeographicBaseline(rootInlineBox);
    for (auto& inlineLevelBox : lineBox.nonRootInlineLevelBoxes()) {
        if (inlineLevelBox.isAtomicInlineBox()) {
            auto& layoutBox = inlineLevelBox.layoutBox();
            if (layoutBox.isInlineTableBox()) {
                // This is the integration codepath where inline table boxes are represented as atomic inline boxes.
                // Integration codepath sets ideographic baseline by default for non-horizontal content.
                continue;
            }
            auto isInlineBlockWithNonSyntheticBaseline = layoutBox.isInlineBlockBox() && downcast<ElementBox>(layoutBox).baselineForIntegration().has_value();
            if (isInlineBlockWithNonSyntheticBaseline && !layoutBox.writingMode().isHorizontal())
                continue;
        }
        adjustLayoutBoundsWithIdeographicBaseline(inlineLevelBox);
    }
}

static Style::TextEdgePair effectiveTextBoxEdge(const InlineLevelBox& rootInlineBox, const BlockLayoutState& blockLayoutState)
{
    // TextBoxEdge property specifies the metrics to use for text-box-trim effects. Values have the same meanings as for line-fit-edge;
    // the auto keyword uses the value of line-fit-edge on the root inline of the the affected line box,
    // interpreting leading (the initial value) as text.
    // https://drafts.csswg.org/css-inline-3/#text-box-edge
    return WTF::switchOn(blockLayoutState.textBoxEdge(),
        [&](Style::TextEdgePair edgePair) {
            return edgePair;
        },
        [&](CSS::Keyword::Auto) {
            return WTF::switchOn(rootInlineBox.lineFitEdge(),
                [&](Style::TextEdgePair edgePair) {
                    return edgePair;
                },
                [&](CSS::Keyword::Leading) {
                    return Style::TextEdgePair { TextEdgeOver::Text, TextEdgeUnder::Text };
                }
            );
        }
    );
}

InlineLayoutUnit LineBoxBuilder::applyTextBoxTrimOnLineBoxIfNeeded(InlineLayoutUnit lineBoxLogicalHeight, LineBox& lineBox) const
{
    auto& rootInlineBox = lineBox.rootInlineBox();
    auto textBoxTrim = blockLayoutState().textBoxTrim();
    auto textBoxEdge = effectiveTextBoxEdge(rootInlineBox, blockLayoutState());
    auto shouldTrimBlockStartOfLineBox = isFirstFormattedLine() && textBoxTrim.contains(BlockLayoutState::TextBoxTrimSide::Start);
    auto shouldTrimBlockEndOfLineBox = isLastLine() && textBoxTrim.contains(BlockLayoutState::TextBoxTrimSide::End);
    if (!shouldTrimBlockStartOfLineBox && !shouldTrimBlockEndOfLineBox)
        return lineBoxLogicalHeight;

    auto& primaryFontMetrics = rootInlineBox.primarymetricsOfPrimaryFont();
    if (shouldTrimBlockEndOfLineBox) {
        auto textBoxEdgeUnderForRootInlineBox = [&] -> InlineLayoutUnit {
            switch (textBoxEdge.under) {
            case TextEdgeUnder::Text:
                return 0.f;
            case TextEdgeUnder::Alphabetic:
                return primaryFontMetrics.intDescent();
            case TextEdgeUnder::Ideographic:
            case TextEdgeUnder::IdeographicInk:
                ASSERT_NOT_IMPLEMENTED_YET();
                return 0.f;
            }
            RELEASE_ASSERT_NOT_REACHED();
        }();
        auto needToTrimThisMuch = std::max(0.f, (lineBoxLogicalHeight - rootInlineBox.logicalBottom()) + textBoxEdgeUnderForRootInlineBox);
        lineBoxLogicalHeight -= needToTrimThisMuch;
    }
    if (shouldTrimBlockStartOfLineBox) {
        auto textBoxEdgeOverForRootInlineBox = [&] -> InlineLayoutUnit {
            switch (textBoxEdge.over) {
            case TextEdgeOver::Text:
                return 0.f;
            case TextEdgeOver::Cap:
                return primaryFontMetrics.intAscent() - primaryFontMetrics.intCapHeight();
            case TextEdgeOver::Ex:
                return primaryFontMetrics.intAscent() - roundf(primaryFontMetrics.xHeight().value_or(0.f));
            case TextEdgeOver::Ideographic:
            case TextEdgeOver::IdeographicInk:
                ASSERT_NOT_IMPLEMENTED_YET();
                return 0.f;
            }
            RELEASE_ASSERT_NOT_REACHED();
        };
        auto needToTrimThisMuch = std::max(0.f, lineLayoutResult().lineGeometry.initialLetterClearGap.value_or(0_lu) + rootInlineBox.logicalTop() + textBoxEdgeOverForRootInlineBox());
        lineBoxLogicalHeight -= needToTrimThisMuch;

        auto adjustRootInlineAndBottomAlignedBoxes = [&] {
            // When trimming makes the line box move up, bottom aligned boxes has to follow the root inline box.
            // All other boxes can keep their vertical positions relative to the line box top.
            for (auto& inlineLevelBox : lineBox.nonRootInlineLevelBoxes()) {
                if (!WTF::holdsAlternative<CSS::Keyword::Bottom>(inlineLevelBox.verticalAlign()))
                    continue;
                inlineLevelBox.setLogicalTop(inlineLevelBox.logicalTop() - needToTrimThisMuch);
            }
            rootInlineBox.setLogicalTop(rootInlineBox.logicalTop() - needToTrimThisMuch);
        };
        adjustRootInlineAndBottomAlignedBoxes();
        m_lineLayoutResult.firstLineStartTrim = needToTrimThisMuch;
    }
    return lineBoxLogicalHeight;
}

void LineBoxBuilder::computeLineBoxGeometry(LineBox& lineBox) const
{
    auto lineBoxLogicalHeight = applyTextBoxTrimOnLineBoxIfNeeded(LineBoxVerticalAligner { formattingContext() }.computeLogicalHeightAndAlign(lineBox), lineBox);
    if (formattingContext().quirks().shouldCollapseLineBoxHeight(lineLayoutResult().runs, m_outsideListMarkers.size()))
        lineBoxLogicalHeight = { };
    lineBox.setLogicalRect({ lineLayoutResult().lineGeometry.logicalTopLeft, lineLayoutResult().lineGeometry.logicalWidth, lineBoxLogicalHeight });
}

void LineBoxBuilder::adjustOutsideListMarkersPosition(LineBox& lineBox)
{
    auto lineBoxRect = lineBox.logicalRect();
    auto floatConstraints = formattingContext().floatingContext().constraints(LayoutUnit { lineBoxRect.top() }, LayoutUnit { lineBoxRect.bottom() }, FloatingContext::MayBeAboveLastFloat::No);

    auto lineBoxOffset = lineBoxRect.left() - (lineLayoutResult().lineGeometry.initialLogicalLeft + lineLayoutResult().lineGeometry.intrusiveFloatsOffset);
    auto rootInlineBoxLogicalLeft = lineBox.logicalRectForRootInlineBox().left();
    auto rootInlineBoxOffsetFromContentBoxOrIntrusiveFloat = lineBoxOffset + rootInlineBoxLogicalLeft;
    for (auto listMarkerBoxIndex : m_outsideListMarkers) {
        auto& listMarkerRun = lineLayoutResult().runs[listMarkerBoxIndex];
        ASSERT(listMarkerRun.isListMarkerOutside());
        auto& listMarkerBox = downcast<ElementBox>(listMarkerRun.layoutBox());
        auto& listMarkerInlineLevelBox = lineBox.inlineLevelBoxFor(listMarkerRun);
        // Move it to the logical left of the line box (from the logical left of the root inline box).
        auto listMarkerInitialOffsetFromRootInlineBox = listMarkerInlineLevelBox.logicalLeft() - rootInlineBoxOffsetFromContentBoxOrIntrusiveFloat;
        auto logicalLeft = listMarkerInitialOffsetFromRootInlineBox;
        auto nestedListMarkerMarginStart = [&] {
            auto nestedOffset = layoutState().nestedListMarkerOffset(listMarkerBox);
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
            return floatConstraints.start ? std::min(0_lu, std::max(floatConstraints.start->x, nestedOffset)) : nestedOffset;
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
    auto& listMarkerGeometry = const_cast<InlineFormattingContext&>(formattingContext()).geometryForBox(listMarkerBox);
    // Make sure that the line content does not get pulled in to logical left direction due to
    // the large negative margin (i.e. this ensures that logical left of the list content stays at the line start)
    listMarkerGeometry.setHorizontalMargin({ listMarkerGeometry.marginStart() + nestedListMarkerMarginStart - LayoutUnit { rootInlineBoxOffset }, listMarkerGeometry.marginEnd() - nestedListMarkerMarginStart + LayoutUnit { rootInlineBoxOffset } });
}

void LineBoxBuilder::expandAboveRootInlineBox(LineBox& lineBox, InlineLayoutUnit expansion) const
{
    lineBox.rootInlineBox().setLogicalTop(lineBox.rootInlineBox().logicalTop() + expansion);
    auto lineBoxRect = lineBox.logicalRect();
    lineBoxRect.expandVertically(expansion);
    lineBox.setLogicalRect(lineBoxRect);
}

}
}

