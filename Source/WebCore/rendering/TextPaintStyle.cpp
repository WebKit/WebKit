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

#include "ColorLuminance.h"
#include "FocusController.h"
#include "GraphicsContext.h"
#include "LocalFrame.h"
#include "Page.h"
#include "PaintInfo.h"
#include "RenderObjectInlines.h"
#include "RenderStyle+GettersInlines.h"
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
        && lineCap == other.lineCap && miterLimit == other.miterLimit;
}

bool textColorIsLegibleAgainstBackgroundColor(const Color& textColor, const Color& backgroundColor)
{
    // Uses the WCAG 2.0 definition of legibility: a contrast ratio of 4.5:1 or greater.
    // https://www.w3.org/TR/WCAG20/#visual-audio-contrast-contrast
    return contrastRatio(textColor, backgroundColor) >= 4.5;
}

static Color adjustColorForVisibilityOnBackground(const Color& textColor, const Color& backgroundColor)
{
    if (textColorIsLegibleAgainstBackgroundColor(textColor, backgroundColor))
        return textColor;

    if (textColor.luminance() > 0.5)
        return textColor.darkened();
    return textColor.lightened();
}

TextPaintStyle computeTextPaintStyle(const RenderText& renderer, const RenderStyle& lineStyle, const PaintInfo& paintInfo)
{
    Ref frame = renderer.frame();
    RefPtr frameView = frame->view();
    TextPaintStyle paintStyle;

    auto viewportSize = frameView ? frameView->size() : IntSize();
    paintStyle.strokeWidth = lineStyle.usedStrokeWidth(viewportSize);
    paintStyle.paintOrder = lineStyle.paintOrder();
    paintStyle.lineJoin = lineStyle.joinStyle();
    paintStyle.lineCap = lineStyle.capStyle();
    paintStyle.miterLimit = lineStyle.strokeMiterLimit().value.value;
    
    if (paintInfo.forceTextColor()) {
        paintStyle.fillColor = paintInfo.forcedTextColor();
        paintStyle.strokeColor = paintInfo.forcedTextColor();
        paintStyle.emphasisMarkColor = paintInfo.forcedTextColor();
        return paintStyle;
    }

    if (lineStyle.insideDefaultButton()) {
        RefPtr page = renderer.frame().page();
        if (page && page->focusController().isActive()) {
            OptionSet<StyleColorOptions> options;
            if (page->settings().useSystemAppearance())
                options.add(StyleColorOptions::UseSystemAppearance);
            paintStyle.fillColor = RenderTheme::singleton().defaultButtonTextColor(options);
            return paintStyle;
        }
    }

    if (lineStyle.insideSubmitButton()) {
        RefPtr page = renderer.frame().page();
        auto focusControllerActive = true;
#if PLATFORM(MAC)
        focusControllerActive = page && page->focusController().isActive();
#endif
        if (focusControllerActive) {
            paintStyle.fillColor = RenderTheme::singleton().submitButtonTextColor(renderer);
            return paintStyle;
        }
    }

    paintStyle.fillColor = lineStyle.visitedDependentTextFillColorApplyingColorFilter(paintInfo.paintBehavior);

    bool forceBackgroundToWhite = false;
    if (frame->document() && protect(frame->document())->printing()) {
        if (lineStyle.printColorAdjust() == PrintColorAdjust::Economy)
            forceBackgroundToWhite = true;

        if (frame->settings().shouldPrintBackgrounds())
            forceBackgroundToWhite = false;

        if (forceBackgroundToWhite) {
            if (Style::hasAnyBackgroundClipText(renderer.checkedStyle()->backgroundLayers()))
                paintStyle.fillColor = Color::black;
        }
    }

    // Make the text fill color legible against a white background
    if (forceBackgroundToWhite)
        paintStyle.fillColor = adjustColorForVisibilityOnBackground(paintStyle.fillColor, Color::white);

    paintStyle.strokeColor = lineStyle.usedStrokeColorApplyingColorFilter();

    // Make the text stroke color legible against a white background
    if (forceBackgroundToWhite)
        paintStyle.strokeColor = adjustColorForVisibilityOnBackground(paintStyle.strokeColor, Color::white);

    paintStyle.emphasisMarkColor = lineStyle.visitedDependentTextEmphasisColorApplyingColorFilter();

    // Make the text stroke color legible against a white background
    if (forceBackgroundToWhite)
        paintStyle.emphasisMarkColor = adjustColorForVisibilityOnBackground(paintStyle.emphasisMarkColor, Color::white);

    return paintStyle;
}

TextPaintStyle computeTextSelectionPaintStyle(const TextPaintStyle& textPaintStyle, const RenderText& renderer, const RenderStyle& lineStyle, const PaintInfo& paintInfo, Style::TextShadows& selectionShadow)
{
    TextPaintStyle selectionPaintStyle = textPaintStyle;

#if ENABLE(TEXT_SELECTION)
    Color foreground = paintInfo.forceTextColor() ? paintInfo.forcedTextColor() : renderer.selectionForegroundColor();
    if (foreground.isValid() && foreground != selectionPaintStyle.fillColor)
        selectionPaintStyle.fillColor = foreground;

    Color emphasisMarkForeground = paintInfo.forceTextColor() ? paintInfo.forcedTextColor() : renderer.selectionEmphasisMarkColor();
    if (emphasisMarkForeground.isValid() && emphasisMarkForeground != selectionPaintStyle.emphasisMarkColor)
        selectionPaintStyle.emphasisMarkColor = emphasisMarkForeground;

    RefPtr view = renderer.frame().view();
    if (auto pseudoStyle = renderer.selectionPseudoStyle()) {
        selectionPaintStyle.hasExplicitlySetFillColor = pseudoStyle->hasExplicitlySetColor();
        selectionShadow = paintInfo.forceTextColor() ? Style::TextShadows { CSS::Keyword::None { } } : pseudoStyle->textShadow();
        auto viewportSize = view ? view->size() : IntSize();
        float strokeWidth = pseudoStyle->usedStrokeWidth(viewportSize);
        if (strokeWidth != selectionPaintStyle.strokeWidth)
            selectionPaintStyle.strokeWidth = strokeWidth;

        Color stroke = paintInfo.forceTextColor() ? paintInfo.forcedTextColor() : pseudoStyle->usedStrokeColor();
        if (stroke != selectionPaintStyle.strokeColor)
            selectionPaintStyle.strokeColor = stroke;
    } else
        selectionShadow = paintInfo.forceTextColor() ? Style::TextShadows { CSS::Keyword::None { } } : lineStyle.textShadow();
#else
    UNUSED_PARAM(renderer);
    UNUSED_PARAM(lineStyle);
    UNUSED_PARAM(paintInfo);
    selectionShadow = paintInfo.forceTextColor() ? Style::TextShadows { CSS::Keyword::None { } } : lineStyle.textShadow();
#endif
    return selectionPaintStyle;
}

void updateGraphicsContext(GraphicsContext& context, const TextPaintStyle& paintStyle, FillColorType fillColorType)
{
    TextDrawingModeFlags mode = context.textDrawingMode();
    TextDrawingModeFlags newMode = mode;
    if (paintStyle.strokeWidth > 0 && paintStyle.strokeColor.isVisible())
        newMode.add(TextDrawingMode::Stroke);
    if (mode != newMode) {
        context.setTextDrawingMode(newMode);
        mode = newMode;
    }

    Color fillColor = fillColorType == UseEmphasisMarkColor ? paintStyle.emphasisMarkColor : paintStyle.fillColor;
    if (mode.contains(TextDrawingMode::Fill) && (fillColor != context.fillColor()))
        context.setFillColor(fillColor);

    if (mode & TextDrawingMode::Stroke) {
        if (paintStyle.strokeColor != context.strokeColor())
            context.setStrokeColor(paintStyle.strokeColor);
        if (paintStyle.strokeWidth != context.strokeThickness())
            context.setStrokeThickness(paintStyle.strokeWidth);
        context.setLineJoin(paintStyle.lineJoin);
        context.setLineCap(paintStyle.lineCap);
        if (paintStyle.lineJoin == LineJoin::Miter)
            context.setMiterLimit(paintStyle.miterLimit);
    }
}

}
