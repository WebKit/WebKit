/*
 * Copyright (C) 2014-2023 Apple Inc.  All rights reserved.
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
#include "InlineIteratorBoxInlines.h"
#include "InlineIteratorInlineBox.h"
#include "InlineIteratorTextBox.h"
#include "RenderBoxModelObjectInlines.h"
#include "RenderInline.h"

namespace WebCore {

struct UnderlineOffsetArguments {
    const RenderStyle& lineStyle;
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

        if (auto* renderInline = dynamicDowncast<RenderInline>(decoratingBoxRendererForUnderline); renderInline && !isAncestorAndWithinBlock(*renderInline, &run->renderer()))
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

        if (auto* renderInline = dynamicDowncast<RenderInline>(decoratingBoxRendererForUnderline); renderInline && !isAncestorAndWithinBlock(*renderInline, &run->renderer()))
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
    // This represents the gap between the baseline and the closest edge of the underline.
    const float textDecorationBaseFontSize = 16.f;
    return std::max(1.f, ceilf(style.computedFontSize() / textDecorationBaseFontSize / 2.f));
}

static float computedUnderlineOffset(const UnderlineOffsetArguments& context)
{
    // FIXME: The code for visual overflow detection passes in a null inline text box. This means it is now
    // broken for the case where auto needs to behave like "under".
    
    // According to the specification `text-underline-position: auto` should avoid drawing through glyphs in
    // scripts where it would not be appropriate (e.g., ideographs).
    // Strictly speaking this can occur whenever the line contains ideographs
    // even if it is horizontal, but detecting this has performance implications. For now we only work with
    // vertical text, since we already determined the baseline type to be ideographic in that case.
    auto& styleToUse = context.lineStyle;
    auto& fontMetrics = styleToUse.metricsOfPrimaryFont();
    auto underlineOffset = 0.f;
    auto textUnderlinePosition = styleToUse.textUnderlinePosition();

    if (isAlignedForUnder(styleToUse)) {
        ASSERT(context.textUnderlinePositionUnder);
        // FIXME: This needs to be flipped for sideways-lr.
        if (textUnderlinePosition.contains(TextUnderlinePosition::Right)) {
            ASSERT(!textUnderlinePosition.contains(TextUnderlinePosition::Left));
            // In vertical typographic modes, the underline is aligned as for under, except it is always aligned to the right edge of the text.
            underlineOffset = 0.f - (styleToUse.textUnderlineOffset().lengthOr(0.f) + defaultGap(styleToUse));
        } else {
            // Position underline relative to the bottom edge of the lowest element's content box.
            auto desiredOffset = context.textUnderlinePositionUnder->textRunLogicalHeight + std::max(context.textUnderlinePositionUnder->textRunOffsetFromBottomMost, 0.f);
            desiredOffset += styleToUse.textUnderlineOffset().lengthOr(0.f) + defaultGap(styleToUse);
            underlineOffset = std::max<float>(desiredOffset, fontMetrics.intAscent());
        }
    } else if (textUnderlinePosition.contains(TextUnderlinePosition::FromFont)) {
        ASSERT(!textUnderlinePosition.contains(TextUnderlinePosition::Under));
        underlineOffset = fontMetrics.intAscent() + fontMetrics.underlinePosition().value_or(0) + styleToUse.textUnderlineOffset().lengthOr(0.f);
    } else
        underlineOffset = fontMetrics.intAscent() + styleToUse.textUnderlineOffset().lengthOr(defaultGap(styleToUse));
    return underlineOffset;
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
        *underlineOffset += *underlineOffset >= 0 ? 1 : -1;

    auto decoration = lineStyle.textDecorationsInEffect();
    if (decoration.isEmpty())
        return GlyphOverflow();

    float strokeThickness = lineStyle.textDecorationThickness().resolve(lineStyle.computedFontSize(), lineStyle.metricsOfPrimaryFont());
    WavyStrokeParameters wavyStrokeParameters;
    float wavyOffset = 0;

    TextDecorationStyle decorationStyle = lineStyle.textDecorationStyle();
    float height = lineStyle.fontCascade().metricsOfPrimaryFont().height();
    GlyphOverflow overflowResult;

    if (decorationStyle == TextDecorationStyle::Wavy) {
        wavyStrokeParameters = WebCore::wavyStrokeParameters(lineStyle.computedFontSize());
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
        auto center = 2 * lineStyle.metricsOfPrimaryFont().ascent() / 3 + autoTextDecorationThickness / 2;
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

bool isAlignedForUnder(const RenderStyle& decoratingBoxStyle)
{
    auto underlinePosition = decoratingBoxStyle.textUnderlinePosition();
    if (underlinePosition.contains(TextUnderlinePosition::Under))
        return true;
    if (decoratingBoxStyle.typographicMode() == TypographicMode::Horizontal || decoratingBoxStyle.textOrientation() == TextOrientation::Sideways)
        return false;
    if (underlinePosition.contains(TextUnderlinePosition::Left) || underlinePosition.contains(TextUnderlinePosition::Right)) {
        // In vertical typographic modes, the underline is aligned as for under for 'left' and 'right'.
        return true;
    }
    // When left/right support is not enabled.
    // FIXME: The offset check is mostly about visual overflow, consider splitting out.
    return underlinePosition.isEmpty() && decoratingBoxStyle.textUnderlineOffset().isAuto();
}

GlyphOverflow visualOverflowForDecorations(const InlineIterator::LineBoxIterator& lineBox, const RenderText& renderer, float textBoxLogicalTop, float textBoxLogicalBottom)
{
    auto& style = lineBox->isFirst() ? renderer.firstLineStyle() : renderer.style();
    auto textUnderlinePositionUnder = std::optional<TextUnderlinePositionUnder> { };

    if (isAlignedForUnder(style)) {
        auto textRunOffset = textRunOffsetFromBottomMost(lineBox, renderer, textBoxLogicalTop, textBoxLogicalBottom);
        textUnderlinePositionUnder = TextUnderlinePositionUnder { textBoxLogicalBottom - textBoxLogicalTop, textRunOffset };
    }

    auto underlineOffset = style.textDecorationsInEffect().contains(TextDecorationLine::Underline)
        ? std::make_optional(computedUnderlineOffset({ style, textUnderlinePositionUnder }))
        : std::nullopt;
    return computedVisualOverflowForDecorations(style, underlineOffset);
}

GlyphOverflow visualOverflowForDecorations(const RenderStyle& style, TextUnderlinePositionUnder textUnderlinePositionUnder)
{
    auto underlineOffset = style.textDecorationsInEffect().contains(TextDecorationLine::Underline)
        ? std::make_optional(computedUnderlineOffset({ style, textUnderlinePositionUnder }))
        : std::nullopt;
    return computedVisualOverflowForDecorations(style, underlineOffset);
}

GlyphOverflow visualOverflowForDecorations(const RenderStyle& style)
{
    auto underlineOffset = style.textDecorationsInEffect().contains(TextDecorationLine::Underline)
        ? std::make_optional(computedUnderlineOffset({ style, { } }))
        : std::nullopt;
    return computedVisualOverflowForDecorations(style, underlineOffset);
}

static inline float inlineBoxContentBoxHeight(const InlineIterator::InlineBox& inlineBox)
{
    auto contentBoxHeight = inlineBox.logicalHeight();
    if (!inlineBox.isRootInlineBox())
        contentBoxHeight -= (inlineBox.renderer().borderAndPaddingBefore() + inlineBox.renderer().borderAndPaddingAfter());
    return contentBoxHeight;
}

float underlineOffsetForTextBoxPainting(const InlineIterator::InlineBox& inlineBox, const RenderStyle& style)
{
    if (!isAlignedForUnder(style))
        return computedUnderlineOffset({ style, { } });

    auto textRunOffset = boxOffsetFromBottomMost(inlineBox.lineBox(), inlineBox.renderer(), inlineBox.logicalTop(), inlineBox.logicalBottom());
    return computedUnderlineOffset({ style, TextUnderlinePositionUnder { inlineBoxContentBoxHeight(inlineBox), textRunOffset } });
}

float overlineOffsetForTextBoxPainting(const InlineIterator::InlineBox& inlineBox, const RenderStyle& style)
{
    if (style.typographicMode() == TypographicMode::Horizontal || style.textOrientation() == TextOrientation::Sideways)
        return { };

    auto underlinePosition = style.textUnderlinePosition();
    auto overBecomesUnder = [&] {
        // If 'right' causes the underline to be drawn on the "over" side of the text, then an overline also switches sides and is drawn on the "under" side.
        if (underlinePosition.contains(TextUnderlinePosition::Right))
            return style.typographicMode() == TypographicMode::Vertical || style.blockFlowDirection() == BlockFlowDirection::RightToLeft;
        return false;
    };
    return overBecomesUnder() ? inlineBoxContentBoxHeight(inlineBox) + defaultGap(style) : (0.f - defaultGap(style));
}

}
