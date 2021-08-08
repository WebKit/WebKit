/*
 * Copyright (C) 2014 Apple Inc.  All rights reserved.
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
#include "LegacyInlineTextBox.h"
#include "LegacyRootInlineBox.h"
#include "TextUnderlineOffset.h"

namespace WebCore {
    
float computeUnderlineOffset(TextUnderlinePosition underlinePosition, TextUnderlineOffset underlineOffset, const FontMetrics& fontMetrics, const LegacyInlineTextBox* inlineTextBox, float defaultGap)
{
    // This represents the gap between the baseline and the closest edge of the underline.
    float gap = std::max<int>(1, std::ceil(defaultGap / 2.0f));
    
    // FIXME: The code for visual overflow detection passes in a null inline text box. This means it is now
    // broken for the case where auto needs to behave like "under".
    
    // According to the specification TextUnderlinePosition::Auto should avoid drawing through glyphs in
    // scripts where it would not be appropriate (e.g., ideographs).
    // Strictly speaking this can occur whenever the line contains ideographs
    // even if it is horizontal, but detecting this has performance implications. For now we only work with
    // vertical text, since we already determined the baseline type to be ideographic in that
    // case.
    
    auto resolvedUnderlinePosition = underlinePosition;
    if (resolvedUnderlinePosition == TextUnderlinePosition::Auto && underlineOffset.isAuto()) {
        if (inlineTextBox)
            resolvedUnderlinePosition = inlineTextBox->root().baselineType() == IdeographicBaseline ? TextUnderlinePosition::Under : TextUnderlinePosition::Auto;
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
        ASSERT(inlineTextBox);
        // Position underline relative to the bottom edge of the lowest element's content box.
        const LegacyRootInlineBox& rootBox = inlineTextBox->root();
        const RenderElement* decorationRenderer = inlineTextBox->parent()->renderer().enclosingRendererWithTextDecoration(TextDecoration::Underline, inlineTextBox->isFirstLine());
        
        float offset;
        if (inlineTextBox->renderer().style().isFlippedLinesWritingMode()) {
            offset = inlineTextBox->logicalTop();
            rootBox.minLogicalTopForTextDecorationLine(offset, decorationRenderer, TextDecoration::Underline);
            offset = inlineTextBox->logicalTop() - offset;
        } else {
            offset = inlineTextBox->logicalBottom();
            rootBox.maxLogicalBottomForTextDecorationLine(offset, decorationRenderer, TextDecoration::Underline);
            offset -= inlineTextBox->logicalBottom();
        }
        auto desiredOffset = inlineTextBox->logicalHeight() + gap + std::max(offset, 0.0f) + underlineOffset.lengthOr(0);
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

GlyphOverflow visualOverflowForDecorations(const RenderStyle& lineStyle, const LegacyInlineTextBox* inlineTextBox)
{
    ASSERT(!inlineTextBox || inlineTextBox->lineStyle() == lineStyle);
    
    auto decoration = lineStyle.textDecorationsInEffect();
    if (decoration.isEmpty())
        return GlyphOverflow();

    float strokeThickness = lineStyle.textDecorationThickness().resolve(lineStyle.computedFontSize(), lineStyle.fontMetrics());
    WavyStrokeParameters wavyStrokeParameters;
    float wavyOffset = 0;
        
    TextDecorationStyle decorationStyle = lineStyle.textDecorationStyle();
    float height = lineStyle.fontCascade().fontMetrics().floatHeight();
    GlyphOverflow overflowResult;
    
    if (decorationStyle == TextDecorationStyle::Wavy) {
        wavyStrokeParameters = getWavyStrokeParameters(lineStyle.computedFontPixelSize());
        wavyOffset = wavyOffsetFromDecoration();
        overflowResult.left = strokeThickness;
        overflowResult.right = strokeThickness;
    }

    // These metrics must match where underlines get drawn.
    // FIXME: Share the code in TextDecorationPainter::paintTextDecoration() so we can just query it for the painted geometry.
    if (decoration & TextDecoration::Underline) {
        // Compensate for the integral ceiling in GraphicsContext::computeLineBoundsAndAntialiasingModeForText()
        int underlineOffset = 1;
        float textDecorationBaseFontSize = 16;
        auto defaultGap = lineStyle.computedFontSize() / textDecorationBaseFontSize;
        underlineOffset += computeUnderlineOffset(lineStyle.textUnderlinePosition(), lineStyle.textUnderlineOffset(), lineStyle.fontMetrics(), inlineTextBox, defaultGap);
        if (decorationStyle == TextDecorationStyle::Wavy) {
            overflowResult.extendBottom(underlineOffset + wavyOffset + wavyStrokeParameters.controlPointDistance + strokeThickness - height);
            overflowResult.extendTop(-(underlineOffset + wavyOffset - wavyStrokeParameters.controlPointDistance - strokeThickness));
        } else {
            overflowResult.extendBottom(underlineOffset + strokeThickness - height);
            overflowResult.extendTop(-underlineOffset);
        }
    }
    if (decoration & TextDecoration::Overline) {
        FloatRect rect(FloatPoint(), FloatSize(1, strokeThickness));
        float autoTextDecorationThickness = TextDecorationThickness::createWithAuto().resolve(lineStyle.computedFontSize(), lineStyle.fontMetrics());
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
    if (decoration & TextDecoration::LineThrough) {
        FloatRect rect(FloatPoint(), FloatSize(1, strokeThickness));
        float autoTextDecorationThickness = TextDecorationThickness::createWithAuto().resolve(lineStyle.computedFontSize(), lineStyle.fontMetrics());
        auto center = 2 * lineStyle.fontMetrics().floatAscent() / 3 + autoTextDecorationThickness / 2;
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
    
}
