/*
 * Copyright (C) 2015 Igalia S.L.
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
#include "ThemeWPE.h"

#include "Color.h"
#include "ControlStates.h"
#include "FloatRoundedRect.h"
#include "GraphicsContext.h"
#include "LengthSize.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {

static const unsigned focusLineWidth = 1;
static const Color focusColor = makeRGBA(46, 52, 54, 150);
static const int buttonFocusOffset = -3;
static const unsigned buttonPadding = 5;
static const int buttonBorderSize = 1; // Keep in sync with menuListButtonBorderSize in RenderThemeWPE.
static const Color buttonBorderColor = makeRGB(205, 199, 194);
static const Color buttonBackgroundColor = makeRGB(244, 242, 241);
static const Color buttonBackgroundPressedColor = makeRGB(214, 209, 205);
static const Color buttonBackgroundHoveredColor = makeRGB(248, 248, 247);
static const Color buttonBackgroundDisabledColor = makeRGB(246, 246, 244);
static const Color toggleBackgroundColor = makeRGB(255, 255, 255);
static const Color toggleBackgroundHoveredColor = makeRGB(242, 242, 242);
static const Color toggleBackgroundDisabledColor = makeRGB(252, 252, 252);
static const double toggleSize = 14.;
static const int toggleFocusOffset = 2;
static const Color toggleColor = makeRGB(46, 52, 54);
static const Color toggleDisabledColor = makeRGB(160, 160, 160);

Theme& Theme::singleton()
{
    static NeverDestroyed<ThemeWPE> theme;
    return theme;
}

void ThemeWPE::paintFocus(GraphicsContext& graphicsContext, const FloatRect& rect, int offset)
{
    FloatRect focusRect = rect;
    focusRect.inflate(offset);
    graphicsContext.setStrokeThickness(focusLineWidth);
    graphicsContext.setLineDash({ focusLineWidth, 2 * focusLineWidth }, 0);
    graphicsContext.setLineCap(SquareCap);
    graphicsContext.setLineJoin(MiterJoin);

    Path path;
    path.addRoundedRect(focusRect, { 2, 2 });
    graphicsContext.setStrokeColor(focusColor);
    graphicsContext.strokePath(path);
}

LengthSize ThemeWPE::controlSize(ControlPart part, const FontCascade& fontCascade, const LengthSize& zoomedSize, float zoomFactor) const
{
    if (!zoomedSize.width.isIntrinsicOrAuto() && !zoomedSize.height.isIntrinsicOrAuto())
        return Theme::controlSize(part, fontCascade, zoomedSize, zoomFactor);

    switch (part) {
    case CheckboxPart:
    case RadioPart:
        return LengthSize { Length(12, Fixed), Length(12, Fixed) };
    default:
        break;
    }

    return Theme::controlSize(part, fontCascade, zoomedSize, zoomFactor);
}

void ThemeWPE::paint(ControlPart part, ControlStates& states, GraphicsContext& context, const FloatRect& zoomedRect, float zoomFactor, ScrollView*, float, float, bool, bool)
{
    switch (part) {
    case CheckboxPart:
        paintCheckbox(states, context, zoomedRect, zoomFactor);
        break;
    case RadioPart:
        paintRadio(states, context, zoomedRect, zoomFactor);
        break;
    case PushButtonPart:
    case DefaultButtonPart:
    case ButtonPart:
    case SquareButtonPart:
        paintButton(states, context, zoomedRect, zoomFactor);
        break;
    default:
        break;
    }
}

void ThemeWPE::paintCheckbox(ControlStates& states, GraphicsContext& graphicsContext, const FloatRect& zoomedRect, float)
{
    GraphicsContextStateSaver stateSaver(graphicsContext);

    FloatRect fieldRect = zoomedRect;
    FloatSize corner(2, 2);
    Path path;
    path.addRoundedRect(fieldRect, corner);
    fieldRect.inflate(-buttonBorderSize);
    path.addRoundedRect(fieldRect, corner);
    graphicsContext.setFillRule(WindRule::EvenOdd);
    graphicsContext.setFillColor(buttonBorderColor);
    graphicsContext.fillPath(path);
    path.clear();

    path.addRoundedRect(fieldRect, corner);
    graphicsContext.setFillRule(WindRule::NonZero);
    if (!(states.states() & ControlStates::EnabledState))
        graphicsContext.setFillColor(toggleBackgroundDisabledColor);
    else if (states.states() & ControlStates::HoverState)
        graphicsContext.setFillColor(toggleBackgroundHoveredColor);
    else
        graphicsContext.setFillColor(toggleBackgroundColor);
    graphicsContext.fillPath(path);
    path.clear();

    if (states.states() & (ControlStates::CheckedState | ControlStates::IndeterminateState)) {
        GraphicsContextStateSaver checkedStateSaver(graphicsContext);
        graphicsContext.translate(fieldRect.x(), fieldRect.y());
        graphicsContext.scale(FloatSize::narrowPrecision(fieldRect.width() / toggleSize, fieldRect.height() / toggleSize));
        if (states.states() & ControlStates::CheckedState) {
            path.moveTo({ 2.43, 6.57 });
            path.addLineTo({ 7.5, 11.63 });
            path.addLineTo({ 14, 5 });
            path.addLineTo({ 14, 1 });
            path.addLineTo({ 7.5, 7.38 });
            path.addLineTo({ 4.56, 4.44 });
            path.closeSubpath();
        } else
            path.addRoundedRect(FloatRect(2, 5, 10, 4), corner);

        if (!(states.states() & ControlStates::EnabledState))
            graphicsContext.setFillColor(toggleDisabledColor);
        else
            graphicsContext.setFillColor(toggleColor);

        graphicsContext.fillPath(path);
        path.clear();
    }

    if (states.states() & ControlStates::FocusState)
        paintFocus(graphicsContext, zoomedRect, toggleFocusOffset);
}

void ThemeWPE::paintRadio(ControlStates& states, GraphicsContext& graphicsContext, const FloatRect& zoomedRect, float)
{
    GraphicsContextStateSaver stateSaver(graphicsContext);
    FloatRect fieldRect = zoomedRect;
    Path path;
    path.addEllipse(fieldRect);
    fieldRect.inflate(-buttonBorderSize);
    path.addEllipse(fieldRect);
    graphicsContext.setFillRule(WindRule::EvenOdd);
    graphicsContext.setFillColor(buttonBorderColor);
    graphicsContext.fillPath(path);
    path.clear();

    path.addEllipse(fieldRect);
    graphicsContext.setFillRule(WindRule::NonZero);
    if (!(states.states() & ControlStates::EnabledState))
        graphicsContext.setFillColor(toggleBackgroundDisabledColor);
    else if (states.states() & ControlStates::HoverState)
        graphicsContext.setFillColor(toggleBackgroundHoveredColor);
    else
        graphicsContext.setFillColor(toggleBackgroundColor);
    graphicsContext.fillPath(path);
    path.clear();

    if (states.states() & ControlStates::CheckedState) {
        fieldRect.inflate(-(fieldRect.width() - fieldRect.width() * 0.70));
        path.addEllipse(fieldRect);
        if (!(states.states() & ControlStates::EnabledState))
            graphicsContext.setFillColor(toggleDisabledColor);
        else
            graphicsContext.setFillColor(toggleColor);
        graphicsContext.fillPath(path);
    }

    if (states.states() & ControlStates::FocusState)
        paintFocus(graphicsContext, zoomedRect, toggleFocusOffset);
}

void ThemeWPE::paintButton(ControlStates& states, GraphicsContext& graphicsContext, const FloatRect& zoomedRect, float)
{
    GraphicsContextStateSaver stateSaver(graphicsContext);

    FloatRect fieldRect = zoomedRect;
    FloatSize corner(5, 5);
    Path path;
    path.addRoundedRect(fieldRect, corner);
    fieldRect.inflate(-buttonBorderSize);
    path.addRoundedRect(fieldRect, corner);
    graphicsContext.setFillRule(WindRule::EvenOdd);
    graphicsContext.setFillColor(buttonBorderColor);
    graphicsContext.fillPath(path);
    path.clear();

    path.addRoundedRect(fieldRect, corner);
    graphicsContext.setFillRule(WindRule::NonZero);
    if (!(states.states() & ControlStates::EnabledState))
        graphicsContext.setFillColor(buttonBackgroundDisabledColor);
    else if (states.states() & ControlStates::PressedState)
        graphicsContext.setFillColor(buttonBackgroundPressedColor);
    else if (states.states() & ControlStates::HoverState)
        graphicsContext.setFillColor(buttonBackgroundHoveredColor);
    else
        graphicsContext.setFillColor(buttonBackgroundColor);
    graphicsContext.fillPath(path);

    if (states.states() & ControlStates::FocusState)
        paintFocus(graphicsContext, zoomedRect, buttonFocusOffset);
}

} // namespace WebCore
