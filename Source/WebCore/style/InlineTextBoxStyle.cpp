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
#include "InlineIteratorTextBox.h"
#include "LegacyInlineTextBox.h"
#include "LegacyRootInlineBox.h"
#include "RenderInline.h"
#include "TextUnderlineOffset.h"

namespace WebCore {

struct UnderlineOffsetArguments {
    const RenderStyle& lineStyle;
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

static float minLogicalTopForTextDecorationLine(const InlineIterator::LineBoxIterator& lineBox, float textRunLogicalTop, const RenderElement* decorationRenderer)
{
    auto minLogicalTop = textRunLogicalTop;
    for (auto run = lineBox->firstLeafBox(); run; run.traverseNextOnLine()) {
        if (run->renderer().isOutOfFlowPositioned())
            continue; // Positioned placeholders don't affect calculations.

        if (!run->style().textDecorationsInEffect().contains(TextDecorationLine::Underline))
            continue; // If the text decoration isn't in effect on the child, then it must be outside of |decorationRenderer|'s hierarchy.

        if (decorationRenderer && decorationRenderer->isRenderInline() && !isAncestorAndWithinBlock(downcast<RenderInline>(*decorationRenderer), &run->renderer()))
            continue;

        if (run->isText() || run->style().textDecorationSkipInk() == TextDecorationSkipInk::None)
            minLogicalTop = std::min<float>(minLogicalTop, run->logicalTop());
    }
    return minLogicalTop;
}

static float maxLogicalBottomForTextDecorationLine(const InlineIterator::LineBoxIterator& lineBox, float textRunLogicalBottom, const RenderElement* decorationRenderer)
{
    auto maxLogicalBottom = textRunLogicalBottom;
    for (auto run = lineBox->firstLeafBox(); run; run.traverseNextOnLine()) {
        if (run->renderer().isOutOfFlowPositioned())
            continue; // Positioned placeholders don't affect calculations.

        if (!run->style().textDecorationsInEffect().contains(TextDecorationLine::Underline))
            continue; // If the text decoration isn't in effect on the child, then it must be outside of |decorationRenderer|'s hierarchy.

        if (decorationRenderer && decorationRenderer->isRenderInline() && !isAncestorAndWithinBlock(downcast<RenderInline>(*decorationRenderer), &run->renderer()))
            continue;

        if (run->isText() || run->style().textDecorationSkipInk() == TextDecorationSkipInk::None)
            maxLogicalBottom = std::max<float>(maxLogicalBottom, run->logicalBottom());
    }
    return maxLogicalBottom;
}

static const RenderElement* enclosingRendererWithTextDecoration(const RenderText& renderer, bool firstLine)
{
    const RenderElement* current = renderer.parent();
    do {
        if (current->isRenderBlock())
            return current;
        if (!current->isRenderInline() || current->isRubyText())
            return nullptr;

        const RenderStyle& styleToUse = firstLine ? current->firstLineStyle() : current->style();
        if (styleToUse.textDecorationLine().contains(TextDecorationLine::Underline))
            return current;
        current = current->parent();
    } while (current && (!current->element() || (!is<HTMLAnchorElement>(*current->element()) && !current->element()->hasTagName(HTMLNames::fontTag))));

    return current;
}

static float textRunOffsetFromBottomMost(const InlineIterator::LineBoxIterator& lineBox, const RenderText& renderer, float textBoxLogicalTop, float textBoxLogicalBottom)
{
    float offset = 0.f;
    auto* decorationRenderer = enclosingRendererWithTextDecoration(renderer, lineBox->isFirst());
    if (renderer.style().isFlippedLinesWritingMode()) {
        auto minLogicalTop = minLogicalTopForTextDecorationLine(lineBox, textBoxLogicalTop, decorationRenderer);
        offset = textBoxLogicalTop - minLogicalTop;
    } else {
        offset = maxLogicalBottomForTextDecorationLine(lineBox, textBoxLogicalBottom, decorationRenderer);
        offset -= textBoxLogicalBottom;
    }
    return offset;
}

static inline float defaultGap(const RenderStyle& style)
{
    const float textDecorationBaseFontSize = 16.f;
    return style.computedFontSize() / textDecorationBaseFontSize;
}

static float computeUnderlineOffset(const UnderlineOffsetArguments& context)
{
    // This represents the gap between the baseline and the closest edge of the underline.
    float gap = std::max<int>(1, std::ceil(context.defaultGap / 2.0f));
    
    // FIXME: The code for visual overflow detection passes in a null inline text box. This means it is now
    // broken for the case where auto needs to behave like "under".
    
    // According to the specification TextUnderlinePosition::Auto should avoid drawing through glyphs in
    // scripts where it would not be appropriate (e.g., ideographs).
    // Strictly speaking this can occur whenever the line contains ideographs
    // even if it is horizontal, but detecting this has performance implications. For now we only work with
    // vertical text, since we already determined the baseline type to be ideographic in that
    // case.
    auto underlinePosition = context.lineStyle.textUnderlinePosition();
    auto underlineOffset = context.lineStyle.textUnderlineOffset();
    auto& fontMetrics = context.lineStyle.metricsOfPrimaryFont();

    auto resolvedUnderlinePosition = underlinePosition;
    if (resolvedUnderlinePosition == TextUnderlinePosition::Auto && underlineOffset.isAuto()) {
        if (context.textUnderlinePositionUnder)
            resolvedUnderlinePosition = context.textUnderlinePositionUnder->baselineType == IdeographicBaseline ? TextUnderlinePosition::Under : TextUnderlinePosition::Auto;
        else
            resolvedUnderlinePosition = TextUnderlinePosition::Auto;
    }

    switch (resolvedUnderlinePosition) {
    case TextUnderlinePosition::Auto:
        if (underlineOffset.isAuto())
            return fontMetrics.ascent() + gap;
        return fontMetrics.ascent() + underlineOffset.lengthValue();
    case TextUnderlinePosition::FromFont:
        return fontMetrics.ascent() + fontMetrics.underlinePosition() + underlineOffset.lengthOr(0);
    case TextUnderlinePosition::Under: {
        ASSERT(context.textUnderlinePositionUnder);
        // Position underline relative to the bottom edge of the lowest element's content box.
        auto desiredOffset = context.textUnderlinePositionUnder->textRunLogicalHeight + gap + std::max(context.textUnderlinePositionUnder->textRunOffsetFromBottomMost, 0.0f) + underlineOffset.lengthOr(0);
        return std::max<float>(desiredOffset, fontMetrics.ascent());
    }
    }

    ASSERT_NOT_REACHED();
    return fontMetrics.ascent() + gap;
}

WavyStrokeParameters getWavyStrokeParameters(float fontSize)
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
        wavyStrokeParameters = getWavyStrokeParameters(lineStyle.computedFontPixelSize());
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
    auto underlineOffset = style.textDecorationsInEffect().contains(TextDecorationLine::Underline)
        ? std::make_optional(computeUnderlineOffset({ style, defaultGap(style), TextUnderlinePositionUnder { lineBox->baselineType(), textBoxLogicalBottom - textBoxLogicalTop, textRunOffsetFromBottomMost(lineBox, renderer, textBoxLogicalTop, textBoxLogicalBottom) } }))
        : std::nullopt;
    return computedVisualOverflowForDecorations(style, underlineOffset);
}

GlyphOverflow visualOverflowForDecorations(const RenderStyle& style, TextUnderlinePositionUnder textUnderlinePositionUnder)
{
    auto underlineOffset = style.textDecorationsInEffect().contains(TextDecorationLine::Underline)
        ? std::make_optional(computeUnderlineOffset({ style, defaultGap(style), textUnderlinePositionUnder }))
        : std::nullopt;
    return computedVisualOverflowForDecorations(style, underlineOffset);
}

GlyphOverflow visualOverflowForDecorations(const RenderStyle& style)
{
    auto underlineOffset = style.textDecorationsInEffect().contains(TextDecorationLine::Underline)
        ? std::make_optional(computeUnderlineOffset({ style, defaultGap(style), { } }))
        : std::nullopt;
    return computedVisualOverflowForDecorations(style, underlineOffset);
}

float underlineOffsetForTextBoxPainting(const RenderStyle& style, const InlineIterator::TextBoxIterator& textBox)
{
    auto textUnderlinePositionUnder = TextUnderlinePositionUnder {
        textBox->lineBox()->baselineType(),
        textBox->logicalBottom() - textBox->logicalTop(),
        textRunOffsetFromBottomMost(textBox->lineBox(), textBox->renderer(), textBox->logicalTop(), textBox->logicalBottom())
    };
    return computeUnderlineOffset({ style , defaultGap(style), textUnderlinePositionUnder });
}

}
