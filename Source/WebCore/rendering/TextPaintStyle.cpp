/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
#include "TextPaintStyle.h"

#include "FocusController.h"
#include "Frame.h"
#include "GraphicsContext.h"
#include "Page.h"
#include "PaintInfo.h"
#include "RenderStyle.h"
#include "RenderText.h"
#include "RenderTheme.h"
#include "RenderView.h"
#include "Settings.h"

namespace WebCore {

TextPaintStyle::TextPaintStyle(const Color& color)
    : fillColor(color)
    , strokeColor(color)
{
}

bool TextPaintStyle::operator==(const TextPaintStyle& other) const
{
    return fillColor == other.fillColor && strokeColor == other.strokeColor && emphasisMarkColor == other.emphasisMarkColor
        && strokeWidth == other.strokeWidth && paintOrder == other.paintOrder && lineJoin == other.lineJoin
#if ENABLE(LETTERPRESS)
        && useLetterpressEffect == other.useLetterpressEffect
#endif
        && lineCap == other.lineCap && miterLimit == other.miterLimit;
}

bool textColorIsLegibleAgainstBackgroundColor(const Color& textColor, const Color& backgroundColor)
{
    // Semi-arbitrarily chose 65025 (255^2) value here after a few tests.
    return differenceSquared(textColor, backgroundColor) > 65025;
}

static Color adjustColorForVisibilityOnBackground(const Color& textColor, const Color& backgroundColor)
{
    if (textColorIsLegibleAgainstBackgroundColor(textColor, backgroundColor))
        return textColor;

    int distanceFromWhite = differenceSquared(textColor, Color::white);
    int distanceFromBlack = differenceSquared(textColor, Color::black);
    if (distanceFromWhite < distanceFromBlack)
        return textColor.dark();

    return textColor.light();
}

TextPaintStyle computeTextPaintStyle(const Frame& frame, const RenderStyle& lineStyle, const PaintInfo& paintInfo)
{
    TextPaintStyle paintStyle;

#if ENABLE(LETTERPRESS)
    paintStyle.useLetterpressEffect = lineStyle.textDecorationsInEffect().contains(TextDecoration::Letterpress);
#endif
    auto viewportSize = frame.view() ? frame.view()->size() : IntSize();
    paintStyle.strokeWidth = lineStyle.computedStrokeWidth(viewportSize);
    paintStyle.paintOrder = lineStyle.paintOrder();
    paintStyle.lineJoin = lineStyle.joinStyle();
    paintStyle.lineCap = lineStyle.capStyle();
    paintStyle.miterLimit = lineStyle.strokeMiterLimit();
    
    if (paintInfo.forceTextColor()) {
        paintStyle.fillColor = paintInfo.forcedTextColor();
        paintStyle.strokeColor = paintInfo.forcedTextColor();
        paintStyle.emphasisMarkColor = paintInfo.forcedTextColor();
        return paintStyle;
    }

    if (lineStyle.insideDefaultButton()) {
        Page* page = frame.page();
        if (page && page->focusController().isActive()) {
            OptionSet<StyleColor::Options> options;
            if (page->useSystemAppearance())
                options.add(StyleColor::Options::UseSystemAppearance);
            paintStyle.fillColor = RenderTheme::singleton().systemColor(CSSValueActivebuttontext, options);
            return paintStyle;
        }
    }

    paintStyle.fillColor = lineStyle.visitedDependentColorWithColorFilter(CSSPropertyWebkitTextFillColor);

    bool forceBackgroundToWhite = false;
    if (frame.document() && frame.document()->printing()) {
        if (lineStyle.printColorAdjust() == PrintColorAdjust::Economy)
            forceBackgroundToWhite = true;
        if (frame.settings().shouldPrintBackgrounds())
            forceBackgroundToWhite = false;
    }

    // Make the text fill color legible against a white background
    if (forceBackgroundToWhite)
        paintStyle.fillColor = adjustColorForVisibilityOnBackground(paintStyle.fillColor, Color::white);

    paintStyle.strokeColor = lineStyle.colorByApplyingColorFilter(lineStyle.computedStrokeColor());

    // Make the text stroke color legible against a white background
    if (forceBackgroundToWhite)
        paintStyle.strokeColor = adjustColorForVisibilityOnBackground(paintStyle.strokeColor, Color::white);

    paintStyle.emphasisMarkColor = lineStyle.visitedDependentColorWithColorFilter(CSSPropertyWebkitTextEmphasisColor);

    // Make the text stroke color legible against a white background
    if (forceBackgroundToWhite)
        paintStyle.emphasisMarkColor = adjustColorForVisibilityOnBackground(paintStyle.emphasisMarkColor, Color::white);

    return paintStyle;
}

TextPaintStyle computeTextSelectionPaintStyle(const TextPaintStyle& textPaintStyle, const RenderText& renderer, const RenderStyle& lineStyle, const PaintInfo& paintInfo, Optional<ShadowData>& selectionShadow)
{
    TextPaintStyle selectionPaintStyle = textPaintStyle;

#if ENABLE(TEXT_SELECTION)
    Color foreground = paintInfo.forceTextColor() ? paintInfo.forcedTextColor() : renderer.selectionForegroundColor();
    if (foreground.isValid() && foreground != selectionPaintStyle.fillColor)
        selectionPaintStyle.fillColor = foreground;

    Color emphasisMarkForeground = paintInfo.forceTextColor() ? paintInfo.forcedTextColor() : renderer.selectionEmphasisMarkColor();
    if (emphasisMarkForeground.isValid() && emphasisMarkForeground != selectionPaintStyle.emphasisMarkColor)
        selectionPaintStyle.emphasisMarkColor = emphasisMarkForeground;

    if (auto pseudoStyle = renderer.selectionPseudoStyle()) {
        selectionShadow = ShadowData::clone(paintInfo.forceTextColor() ? nullptr : pseudoStyle->textShadow());
        auto viewportSize = renderer.frame().view() ? renderer.frame().view()->size() : IntSize();
        float strokeWidth = pseudoStyle->computedStrokeWidth(viewportSize);
        if (strokeWidth != selectionPaintStyle.strokeWidth)
            selectionPaintStyle.strokeWidth = strokeWidth;

        Color stroke = paintInfo.forceTextColor() ? paintInfo.forcedTextColor() : pseudoStyle->computedStrokeColor();
        if (stroke != selectionPaintStyle.strokeColor)
            selectionPaintStyle.strokeColor = stroke;
    } else
        selectionShadow = ShadowData::clone(paintInfo.forceTextColor() ? nullptr : lineStyle.textShadow());
#else
    UNUSED_PARAM(renderer);
    UNUSED_PARAM(lineStyle);
    UNUSED_PARAM(paintInfo);
    selectionShadow = ShadowData::clone(paintInfo.forceTextColor() ? nullptr : lineStyle.textShadow());
#endif
    return selectionPaintStyle;
}

void updateGraphicsContext(GraphicsContext& context, const TextPaintStyle& paintStyle, FillColorType fillColorType)
{
    TextDrawingModeFlags mode = context.textDrawingMode();
    TextDrawingModeFlags newMode = mode;
#if ENABLE(LETTERPRESS)
    if (paintStyle.useLetterpressEffect)
        newMode |= TextModeLetterpress;
    else
        newMode &= ~TextModeLetterpress;
#endif
    if (paintStyle.strokeWidth > 0 && paintStyle.strokeColor.isVisible())
        newMode |= TextModeStroke;
    if (mode != newMode) {
        context.setTextDrawingMode(newMode);
        mode = newMode;
    }

    Color fillColor = fillColorType == UseEmphasisMarkColor ? paintStyle.emphasisMarkColor : paintStyle.fillColor;
    if (mode & TextModeFill && (fillColor != context.fillColor()))
        context.setFillColor(fillColor);

    if (mode & TextModeStroke) {
        if (paintStyle.strokeColor != context.strokeColor())
            context.setStrokeColor(paintStyle.strokeColor);
        if (paintStyle.strokeWidth != context.strokeThickness())
            context.setStrokeThickness(paintStyle.strokeWidth);
        context.setLineJoin(paintStyle.lineJoin);
        context.setLineCap(paintStyle.lineCap);
        if (paintStyle.lineJoin == MiterJoin)
            context.setMiterLimit(paintStyle.miterLimit);
    }
}

}
