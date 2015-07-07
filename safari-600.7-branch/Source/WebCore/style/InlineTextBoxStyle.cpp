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

#include "Font.h"
#include "InlineTextBox.h"
#include "RootInlineBox.h"

namespace WebCore {
    
int computeUnderlineOffset(TextUnderlinePosition underlinePosition, const FontMetrics& fontMetrics, InlineTextBox* inlineTextBox, int textDecorationThickness)
{
    // This represents the gap between the baseline and the closest edge of the underline.
    int gap = std::max<int>(1, ceilf(textDecorationThickness / 2.0));

    // According to the specification TextUnderlinePositionAuto should default to 'alphabetic' for horizontal text
    // and to 'under Left' for vertical text (e.g. japanese). We support only horizontal text for now.
    switch (underlinePosition) {
    case TextUnderlinePositionAlphabetic:
    case TextUnderlinePositionAuto:
        return fontMetrics.ascent() + gap;
    case TextUnderlinePositionUnder: {
        ASSERT(inlineTextBox);
        // Position underline relative to the bottom edge of the lowest element's content box.
        const RootInlineBox& rootBox = inlineTextBox->root();
        const RenderElement* decorationRenderer = inlineTextBox->parent()->renderer().enclosingRendererWithTextDecoration(TextDecorationUnderline, inlineTextBox->isFirstLine());
        
        float offset;
        if (inlineTextBox->renderer().style().isFlippedLinesWritingMode()) {
            offset = inlineTextBox->logicalTop();
            rootBox.minLogicalTopForTextDecorationLine(offset, decorationRenderer, TextDecorationUnderline);
            offset = inlineTextBox->logicalTop() - offset;
        } else {
            offset = inlineTextBox->logicalBottom();
            rootBox.maxLogicalBottomForTextDecorationLine(offset, decorationRenderer, TextDecorationUnderline);
            offset -= inlineTextBox->logicalBottom();
        }
        return inlineTextBox->logicalHeight() + gap + std::max<float>(offset, 0);
    }
    }

    ASSERT_NOT_REACHED();
    return fontMetrics.ascent() + gap;
}
    
void getWavyStrokeParameters(float strokeThickness, float& controlPointDistance, float& step)
{
    // Distance between decoration's axis and Bezier curve's control points.
    // The height of the curve is based on this distance. Use a minimum of 6 pixels distance since
    // the actual curve passes approximately at half of that distance, that is 3 pixels.
    // The minimum height of the curve is also approximately 3 pixels. Increases the curve's height
    // as strokeThickness increases to make the curve look better.
    controlPointDistance = 3 * std::max<float>(2, strokeThickness);

    // Increment used to form the diamond shape between start point (p1), control
    // points and end point (p2) along the axis of the decoration. Makes the
    // curve wider as strokeThickness increases to make the curve look better.
    step = 2 * std::max<float>(2, strokeThickness);
}

static inline void extendIntToFloat(int& extendMe, float extendTo)
{
    extendMe = std::max(extendMe, static_cast<int>(ceilf(extendTo)));
}

GlyphOverflow visualOverflowForDecorations(const RenderStyle& lineStyle, InlineTextBox* inlineTextBox)
{
    ASSERT(!inlineTextBox || inlineTextBox->lineStyle() == lineStyle);
    
    TextDecoration decoration = lineStyle.textDecorationsInEffect();
    if (decoration == TextDecorationNone)
        return GlyphOverflow();
    
    float strokeThickness = textDecorationStrokeThickness(lineStyle.fontSize());
    float controlPointDistance;
    float step;
    float wavyOffset;
        
    TextDecorationStyle decorationStyle = lineStyle.textDecorationStyle();
    float height = lineStyle.font().fontMetrics().floatHeight();
    GlyphOverflow overflowResult;
    
    if (decorationStyle == TextDecorationStyleWavy) {
        getWavyStrokeParameters(strokeThickness, controlPointDistance, step);
        wavyOffset = wavyOffsetFromDecoration();
        overflowResult.left = strokeThickness;
        overflowResult.right = strokeThickness;
    }

    // These metrics must match where underlines get drawn.
    if (decoration & TextDecorationUnderline) {
        float underlineOffset = computeUnderlineOffset(lineStyle.textUnderlinePosition(), lineStyle.fontMetrics(), inlineTextBox, strokeThickness);
        if (decorationStyle == TextDecorationStyleWavy) {
            extendIntToFloat(overflowResult.bottom, underlineOffset + wavyOffset + controlPointDistance + strokeThickness - height);
            extendIntToFloat(overflowResult.top, -(underlineOffset + wavyOffset - controlPointDistance - strokeThickness));
        } else {
            extendIntToFloat(overflowResult.bottom, underlineOffset + strokeThickness - height);
            extendIntToFloat(overflowResult.top, -underlineOffset);
        }
    }
    if (decoration & TextDecorationOverline) {
        if (decorationStyle == TextDecorationStyleWavy) {
            extendIntToFloat(overflowResult.bottom, -wavyOffset + controlPointDistance + strokeThickness - height);
            extendIntToFloat(overflowResult.top, wavyOffset + controlPointDistance + strokeThickness);
        } else {
            extendIntToFloat(overflowResult.bottom, strokeThickness - height);
            // top is untouched
        }
    }
    if (decoration & TextDecorationLineThrough) {
        float baseline = lineStyle.fontMetrics().floatAscent();
        if (decorationStyle == TextDecorationStyleWavy) {
            extendIntToFloat(overflowResult.bottom, 2 * baseline / 3 + controlPointDistance + strokeThickness - height);
            extendIntToFloat(overflowResult.top, -(2 * baseline / 3 - controlPointDistance - strokeThickness));
        } else {
            extendIntToFloat(overflowResult.bottom, 2 * baseline / 3 + strokeThickness - height);
            extendIntToFloat(overflowResult.top, -(2 * baseline / 3));
        }
    }
    return overflowResult;
}
    
}
