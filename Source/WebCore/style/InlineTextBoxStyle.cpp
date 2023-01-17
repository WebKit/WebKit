/*
 * Copyright (C) 2014-2021 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "InlineTextBoxStyle.h"

#include "FontCascade.h"
#include "HTMLAnchorElement.h"
#include "HTMLNames.h"
#include "InlineIteratorInlineBox.h"
#include "InlineIteratorTextBox.h"
#include "RenderInline.h"
#include "TextUnderlineOffset.h"

namespace WebCore {

struct UnderlineOffsetArguments {
    const RenderStyle& lineStyle;
    TextUnderlinePosition resolvedUnderlinePosition { TextUnderlinePosition::Auto };
    float defaultGap { 0 };
    std::optional<TextUnderlinePositionUnder> textUnderlinePositionUnder { };
};

static bool isAncestorAndWithinBlock(const RenderInline& ancestor, const RenderObject* child)
{
    const RenderObject* object = child;
    while (object && (!object->isRenderBlock() || object->isInline())) {
        if (object == &ancestor)
            return true;
        object = object->parent();
    }
    return false;
}

static float minLogicalTopForTextDecorationLineUnder(const InlineIterator::LineBoxIterator& lineBox, float textRunLogicalTop, const RenderElement& decoratingBoxRendererForUnderline)
{
    auto minLogicalTop = textRunLogicalTop;
    for (auto run = lineBox->firstLeafBox(); run; run.traverseNextOnLine()) {
        if (run->renderer().isOutOfFlowPositioned())
            continue; // Positioned placeholders don't affect calculations.

        if (!run->style().textDecorationsInEffect().contains(TextDecorationLine::Underline))
            continue; // If the text decoration isn't in effect on the child, then it must be outside of |decoratingBoxRendererForUnderline|'s hierarchy.

        if (decoratingBoxRendererForUnderline.isRenderInline() && !isAncestorAndWithinBlock(downcast<RenderInline>(decoratingBoxRendererForUnderline), &run->renderer()))
            continue;

        if (run->isText() || run->style().textDecorationSkipInk() == TextDecorationSkipInk::None)
            minLogicalTop = std::min<float>(minLogicalTop, run->logicalTop());
    }
    return minLogicalTop;
}

static float maxLogicalBottomForTextDecorationLineUnder(const InlineIterator::LineBoxIterator& lineBox, float textRunLogicalBottom, const RenderElement& decoratingBoxRendererForUnderline)
{
    auto maxLogicalBottom = textRunLogicalBottom;
    for (auto run = lineBox->firstLeafBox(); run; run.traverseNextOnLine()) {
        if (run->renderer().isOutOfFlowPositioned())
            continue; // Positioned placeholders don't affect calculations.

        if (!run->style().textDecorationsInEffect().contains(TextDecorationLine::Underline))
            continue; // If the text decoration isn't in effect on the child, then it must be outside of |decoratingBoxRendererForUnderline|'s hierarchy.

        if (decoratingBoxRendererForUnderline.isRenderInline() && !isAncestorAndWithinBlock(downcast<RenderInline>(decoratingBoxRendererForUnderline), &run->renderer()))
            continue;

        if (run->isText() || run->style().textDecorationSkipInk() == TextDecorationSkipInk::None)
            maxLogicalBottom = std::max<float>(maxLogicalBottom, run->logicalBottom());
    }
    return maxLogicalBottom;
}

static const RenderElement* enclosingRendererWithTextDecoration(const RenderText& renderer)
{
    for (auto* ancestor = renderer.parent(); ancestor; ancestor = ancestor->parent()) {
        if (ancestor->isRenderBlock())
            return ancestor;

        if (!ancestor->isRenderInline()) {
            // We should always find either the block container or an inline box ancestor inbetween.
            return nullptr;
        }

        auto isDecoratingInlineBox = [&] {
            if (ancestor->element() && (is<HTMLAnchorElement>(*ancestor->element()) || ancestor->element()->hasTagName(HTMLNames::fontTag))) {
                // <font> and <a> are always considered decorating boxes.
                return true;
            }
            return ancestor->style().textDecorationLine().contains(TextDecorationLine::Underline);
        };
        if (isDecoratingInlineBox())
            return ancestor;
    }

    return nullptr;
}

static float boxOffsetFromBottomMost(const InlineIterator::LineBoxIterator& lineBox, const RenderElement& decoratingInlineBoxRenderer, float boxLogicalTop, float boxLogicalBottom)
{
    if (decoratingInlineBoxRenderer.style().isFlippedLinesWritingMode())
        return boxLogicalTop - minLogicalTopForTextDecorationLineUnder(lineBox, boxLogicalTop, decoratingInlineBoxRenderer);
    return maxLogicalBottomForTextDecorationLineUnder(lineBox, boxLogicalBottom, decoratingInlineBoxRenderer) - boxLogicalBottom;
}

static float textRunOffsetFromBottomMost(const InlineIterator::LineBoxIterator& lineBox, const RenderText& renderer, float textBoxLogicalTop, float textBoxLogicalBottom)
{
    auto* decoratingBoxRendererForUnderline = enclosingRendererWithTextDecoration(renderer);
    if (!decoratingBoxRendererForUnderline)
        return 0.f;

    return boxOffsetFromBottomMost(lineBox, *decoratingBoxRendererForUnderline, textBoxLogicalTop, textBoxLogicalBottom);
}

static inline float defaultGap(const RenderStyle& style)
{
    const float textDecorationBaseFontSize = 16.f;
    return style.computedFontSize() / textDecorationBaseFontSize;
}

static inline TextUnderlinePosition resolvedUnderlinePosition(const RenderStyle& decoratingBoxStyle, std::optional<FontBaseline> baselineType)
{
    auto underlinePosition = decoratingBoxStyle.textUnderlinePosition();
    if (underlinePosition == TextUnderlinePosition::Auto && decoratingBoxStyle.textUnderlineOffset().isAuto()) {
        if (baselineType)
            return *baselineType == IdeographicBaseline ? TextUnderlinePosition::Under : TextUnderlinePosition::Auto;
        return TextUnderlinePosition::Auto;
    }
    return underlinePosition;
}

static float computedUnderlineOffset(const UnderlineOffsetArguments& context)
{
    // This represents the gap between the baseline and the closest edge of the underline.
    auto gap = std::max(1.f, std::ceil(context.defaultGap / 2.f));

    // FIXME: The code for visual overflow detection passes in a null inline text box. This means it is now
    // broken for the case where auto needs to behave like "under".
    
    // According to the specification TextUnderlinePosition::Auto should avoid drawing through glyphs in
    // scripts where it would not be appropriate (e.g., ideographs).
    // Strictly speaking this can occur whenever the line contains ideographs
    // even if it is horizontal, but detecting this has performance implications. For now we only work with
    // vertical text, since we already determined the baseline type to be ideographic in that case.
    auto underlineOffset = context.lineStyle.textUnderlineOffset();
    auto& fontMetrics = context.lineStyle.metricsOfPrimaryFont();

    float computedUnderlineOffset = fontMetrics.ascent() + gap;
    switch (context.resolvedUnderlinePosition) {
    case TextUnderlinePosition::Auto:
        computedUnderlineOffset = fontMetrics.ascent() + underlineOffset.lengthOr(gap);
        break;
    case TextUnderlinePosition::FromFont:
        computedUnderlineOffset = fontMetrics.ascent() + fontMetrics.underlinePosition() + underlineOffset.lengthOr(0.f);
        break;
    case TextUnderlinePosition::Under: {
        ASSERT(context.textUnderlinePositionUnder);
        // Position underline relative to the bottom edge of the lowest element's content box.
        auto desiredOffset = context.textUnderlinePositionUnder->textRunLogicalHeight + gap + std::max(context.textUnderlinePositionUnder->textRunOffsetFromBottomMost, 0.f) + underlineOffset.lengthOr(0.f);
        computedUnderlineOffset = std::max<float>(desiredOffset, fontMetrics.ascent());
        break;
    }
    default:
        ASSERT_NOT_REACHED();
        break;
    }

    return computedUnderlineOffset;
}

WavyStrokeParameters wavyStrokeParameters(float fontSize)
{
    WavyStrokeParameters result;
    // More information is in the WavyStrokeParameters definition.
    result.controlPointDistance = fontSize * 1.5 / 16;
    result.step = fontSize / 4.5;
    return result;
}

static GlyphOverflow computedVisualOverflowForDecorations(const RenderStyle& lineStyle, std::optional<float> underlineOffset)
{
    // Compensate for the integral ceiling in GraphicsContext::computeLineBoundsAndAntialiasingModeForText()
    if (underlineOffset)
        *underlineOffset += 1;

    auto decoration = lineStyle.textDecorationsInEffect();
    if (decoration.isEmpty())
        return GlyphOverflow();

    float strokeThickness = lineStyle.textDecorationThickness().resolve(lineStyle.computedFontSize(), lineStyle.metricsOfPrimaryFont());
    WavyStrokeParameters wavyStrokeParameters;
    float wavyOffset = 0;

    TextDecorationStyle decorationStyle = lineStyle.textDecorationStyle();
    float height = lineStyle.fontCascade().metricsOfPrimaryFont().floatHeight();
    GlyphOverflow overflowResult;

    if (decorationStyle == TextDecorationStyle::Wavy) {
        wavyStrokeParameters = WebCore::wavyStrokeParameters(lineStyle.computedFontPixelSize());
        wavyOffset = wavyOffsetFromDecoration();
        overflowResult.left = strokeThickness;
        overflowResult.right = strokeThickness;
    }

    // These metrics must match where underlines get drawn.
    // FIXME: Share the code in TextDecorationPainter::paintBackgroundDecorations() so we can just query it for the painted geometry.
    if (decoration & TextDecorationLine::Underline) {
        ASSERT(underlineOffset);
        if (decorationStyle == TextDecorationStyle::Wavy) {
            overflowResult.extendBottom(*underlineOffset + wavyOffset + wavyStrokeParameters.controlPointDistance + strokeThickness - height);
            overflowResult.extendTop(-(*underlineOffset + wavyOffset - wavyStrokeParameters.controlPointDistance - strokeThickness));
        } else {
            overflowResult.extendBottom(*underlineOffset + strokeThickness - height);
            overflowResult.extendTop(-*underlineOffset);
        }
    }
    if (decoration & TextDecorationLine::Overline) {
        FloatRect rect(FloatPoint(), FloatSize(1, strokeThickness));
        float autoTextDecorationThickness = TextDecorationThickness::createWithAuto().resolve(lineStyle.computedFontSize(), lineStyle.metricsOfPrimaryFont());
        rect.move(0, autoTextDecorationThickness - strokeThickness - wavyOffset);
        if (decorationStyle == TextDecorationStyle::Wavy) {
            FloatBoxExtent wavyExpansion;
            wavyExpansion.setTop(wavyStrokeParameters.controlPointDistance);
            wavyExpansion.setBottom(wavyStrokeParameters.controlPointDistance);
            rect.expand(wavyExpansion);
        }
        overflowResult.extendTop(-rect.y());
        overflowResult.extendBottom(rect.maxY() - height);
    }
    if (decoration & TextDecorationLine::LineThrough) {
        FloatRect rect(FloatPoint(), FloatSize(1, strokeThickness));
        float autoTextDecorationThickness = TextDecorationThickness::createWithAuto().resolve(lineStyle.computedFontSize(), lineStyle.metricsOfPrimaryFont());
        auto center = 2 * lineStyle.metricsOfPrimaryFont().floatAscent() / 3 + autoTextDecorationThickness / 2;
        rect.move(0, center - strokeThickness / 2);
        if (decorationStyle == TextDecorationStyle::Wavy) {
            FloatBoxExtent wavyExpansion;
            wavyExpansion.setTop(wavyStrokeParameters.controlPointDistance);
            wavyExpansion.setBottom(wavyStrokeParameters.controlPointDistance);
            rect.expand(wavyExpansion);
        }
        overflowResult.extendTop(-rect.y());
        overflowResult.extendBottom(rect.maxY() - height);
    }
    return overflowResult;
}

GlyphOverflow visualOverflowForDecorations(const InlineIterator::LineBoxIterator& lineBox, const RenderText& renderer, float textBoxLogicalTop, float textBoxLogicalBottom)
{
    auto& style = lineBox->isFirst() ? renderer.firstLineStyle() : renderer.style();
    auto underlinePositionValue = resolvedUnderlinePosition(style, lineBox->baselineType());
    auto textUnderlinePositionUnder = std::optional<TextUnderlinePositionUnder> { };

    if (underlinePositionValue == TextUnderlinePosition::Under) {
        auto textRunOffset = textRunOffsetFromBottomMost(lineBox, renderer, textBoxLogicalTop, textBoxLogicalBottom);
        textUnderlinePositionUnder = TextUnderlinePositionUnder { textBoxLogicalBottom - textBoxLogicalTop, textRunOffset };
    }

    auto underlineOffset = style.textDecorationsInEffect().contains(TextDecorationLine::Underline)
        ? std::make_optional(computedUnderlineOffset({ style, underlinePositionValue, defaultGap(style), textUnderlinePositionUnder }))
        : std::nullopt;
    return computedVisualOverflowForDecorations(style, underlineOffset);
}

GlyphOverflow visualOverflowForDecorations(const RenderStyle& style, FontBaseline baselineType, TextUnderlinePositionUnder textUnderlinePositionUnder)
{
    auto underlineOffset = style.textDecorationsInEffect().contains(TextDecorationLine::Underline)
        ? std::make_optional(computedUnderlineOffset({ style, resolvedUnderlinePosition(style, baselineType), defaultGap(style), textUnderlinePositionUnder }))
        : std::nullopt;
    return computedVisualOverflowForDecorations(style, underlineOffset);
}

GlyphOverflow visualOverflowForDecorations(const RenderStyle& style)
{
    auto underlineOffset = style.textDecorationsInEffect().contains(TextDecorationLine::Underline)
        ? std::make_optional(computedUnderlineOffset({ style, resolvedUnderlinePosition(style, { }), defaultGap(style), { } }))
        : std::nullopt;
    return computedVisualOverflowForDecorations(style, underlineOffset);
}

float underlineOffsetForTextBoxPainting(const InlineIterator::InlineBox& inlineBox, const RenderStyle& style)
{
    auto textUnderlinePositionUnder = std::optional<TextUnderlinePositionUnder> { };
    auto underlinePositionValue = resolvedUnderlinePosition(style, inlineBox.lineBox()->baselineType());

    if (underlinePositionValue == TextUnderlinePosition::Under) {
        auto textRunOffset = boxOffsetFromBottomMost(inlineBox.lineBox(), inlineBox.renderer(), inlineBox.logicalTop(), inlineBox.logicalBottom());
        auto inlineBoxContentBoxHeight = inlineBox.logicalHeight();
        if (!inlineBox.isRootInlineBox())
            inlineBoxContentBoxHeight -= (inlineBox.renderer().borderAndPaddingBefore() + inlineBox.renderer().borderAndPaddingAfter());
        textUnderlinePositionUnder = TextUnderlinePositionUnder { inlineBoxContentBoxHeight, textRunOffset };
    }

    return computedUnderlineOffset({ style, underlinePositionValue, defaultGap(style), textUnderlinePositionUnder });
}

}
