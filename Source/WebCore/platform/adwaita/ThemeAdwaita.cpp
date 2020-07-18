/*
 * Copyright (C) 2015, 2020 Igalia S.L.
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
#include "ThemeAdwaita.h"

#include "Color.h"
#include "ControlStates.h"
#include "FloatRoundedRect.h"
#include "GraphicsContext.h"
#include "LengthSize.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {

static const unsigned focusLineWidth = 1;
static constexpr auto focusRingColor = SRGBA<uint8_t> { 46, 52, 54, 150 };
static const unsigned arrowSize = 16;
static constexpr auto arrowColor = SRGBA<uint8_t> { 46, 52, 54 };
static const int buttonFocusOffset = -3;
static const unsigned buttonPadding = 5;
static const int buttonBorderSize = 1; // Keep in sync with menuListButtonBorderSize in RenderThemeAdwaita.
static constexpr auto buttonBorderColor = SRGBA<uint8_t> { 205, 199, 194 };
static constexpr auto buttonBackgroundColor = SRGBA<uint8_t> { 244, 242, 241 };
static constexpr auto buttonBackgroundPressedColor = SRGBA<uint8_t> { 214, 209, 205 };
static constexpr auto buttonBackgroundHoveredColor = SRGBA<uint8_t> { 248, 248, 247 };
static constexpr auto buttonBackgroundDisabledColor = SRGBA<uint8_t> { 246, 246, 244 };
static constexpr auto toggleBackgroundColor = Color::white;
static constexpr auto toggleBackgroundHoveredColor = SRGBA<uint8_t> { 242, 242, 242 };
static constexpr auto toggleBackgroundDisabledColor = SRGBA<uint8_t> { 252, 252, 252 };
static const double toggleSize = 14.;
static const int toggleFocusOffset = 2;
static constexpr auto toggleColor = SRGBA<uint8_t> { 46, 52, 54 };
static constexpr auto toggleDisabledColor = SRGBA<uint8_t> { 160, 160, 160 };
static constexpr auto spinButtonBorderColor = SRGBA<uint8_t> { 220, 223, 227 };
static constexpr auto spinButtonBackgroundColor = SRGBA<uint8_t> { 252, 252, 252 };
static constexpr auto spinButtonBackgroundHoveredColor = SRGBA<uint8_t> { 46, 52, 54, 50 };
static constexpr auto spinButtonBackgroundPressedColor = SRGBA<uint8_t> { 46, 52, 54, 70 };

#if !PLATFORM(GTK) || USE(GTK4)
Theme& Theme::singleton()
{
    static NeverDestroyed<ThemeAdwaita> theme;
    return theme;
}
#endif

Color ThemeAdwaita::activeSelectionForegroundColor() const
{
    return Color::white;
}

Color ThemeAdwaita::activeSelectionBackgroundColor() const
{
    return SRGBA<uint8_t> { 52, 132, 228 };
}

Color ThemeAdwaita::inactiveSelectionForegroundColor() const
{
    return SRGBA<uint8_t> { 252, 252, 252 };
}

Color ThemeAdwaita::inactiveSelectionBackgroundColor() const
{
    return activeSelectionBackgroundColor();
}

Color ThemeAdwaita::focusColor()
{
    return focusRingColor;
}

void ThemeAdwaita::paintFocus(GraphicsContext& graphicsContext, const FloatRect& rect, int offset)
{
    FloatRect focusRect = rect;
    focusRect.inflate(offset);
    Path path;
    path.addRoundedRect(focusRect, { 2, 2 });
    paintFocus(graphicsContext, path, focusRingColor);
}

void ThemeAdwaita::paintFocus(GraphicsContext& graphicsContext, const Path& path, const Color& color)
{
    GraphicsContextStateSaver stateSaver(graphicsContext);

    graphicsContext.beginTransparencyLayer(color.alphaAsFloat());
    graphicsContext.setStrokeThickness(focusLineWidth);
    graphicsContext.setLineDash({ focusLineWidth, 2 * focusLineWidth }, 0);
    graphicsContext.setLineCap(SquareCap);
    graphicsContext.setLineJoin(MiterJoin);
    graphicsContext.setStrokeColor(color.opaqueColor());
    graphicsContext.strokePath(path);
    graphicsContext.setFillRule(WindRule::NonZero);
    graphicsContext.setCompositeOperation(CompositeOperator::Clear);
    graphicsContext.fillPath(path);
    graphicsContext.setCompositeOperation(CompositeOperator::SourceOver);
    graphicsContext.endTransparencyLayer();
}

void ThemeAdwaita::paintFocus(GraphicsContext& graphicsContext, const Vector<FloatRect>& rects, const Color& color)
{
    FloatSize corner(2, 2);
    Path path;
    for (const auto& rect : rects)
        path.addRoundedRect(rect, corner);
    paintFocus(graphicsContext, path, color);
}

void ThemeAdwaita::paintArrow(GraphicsContext& graphicsContext, ArrowDirection direction)
{
    Path path;
    switch (direction) {
    case ArrowDirection::Down:
        path.moveTo({ 3, 6 });
        path.addLineTo({ 13, 6 });
        path.addLineTo({ 8, 11 });
        break;
    case ArrowDirection::Up:
        path.moveTo({ 3, 10 });
        path.addLineTo({ 8, 5 });
        path.addLineTo({ 13, 10});
        break;
    }
    path.closeSubpath();

    graphicsContext.setFillColor(arrowColor);
    graphicsContext.fillPath(path);
}

LengthSize ThemeAdwaita::controlSize(ControlPart part, const FontCascade& fontCascade, const LengthSize& zoomedSize, float zoomFactor) const
{
    if (!zoomedSize.width.isIntrinsicOrAuto() && !zoomedSize.height.isIntrinsicOrAuto())
        return Theme::controlSize(part, fontCascade, zoomedSize, zoomFactor);

    switch (part) {
    case CheckboxPart:
    case RadioPart: {
        LengthSize buttonSize = zoomedSize;
        if (buttonSize.width.isIntrinsicOrAuto())
            buttonSize.width = Length(12, Fixed);
        if (buttonSize.height.isIntrinsicOrAuto())
            buttonSize.height = Length(12, Fixed);
        return buttonSize;
    }
    case InnerSpinButtonPart: {
        LengthSize spinButtonSize = zoomedSize;
        if (spinButtonSize.width.isIntrinsicOrAuto())
            spinButtonSize.width = Length(static_cast<int>(arrowSize), Fixed);
        if (spinButtonSize.height.isIntrinsicOrAuto() || fontCascade.pixelSize() > static_cast<int>(arrowSize))
            spinButtonSize.height = Length(fontCascade.pixelSize(), Fixed);
        return spinButtonSize;
    }
    default:
        break;
    }

    return Theme::controlSize(part, fontCascade, zoomedSize, zoomFactor);
}

LengthSize ThemeAdwaita::minimumControlSize(ControlPart, const FontCascade&, const LengthSize& zoomedSize, float) const
{
    if (!zoomedSize.width.isIntrinsicOrAuto() && !zoomedSize.height.isIntrinsicOrAuto())
        return zoomedSize;

    LengthSize minSize = zoomedSize;
    if (minSize.width.isIntrinsicOrAuto())
        minSize.width = Length(0, Fixed);
    if (minSize.height.isIntrinsicOrAuto())
        minSize.height = Length(0, Fixed);
    return minSize;
}

LengthBox ThemeAdwaita::controlBorder(ControlPart part, const FontCascade& font, const LengthBox& zoomedBox, float zoomFactor) const
{
    switch (part) {
    case PushButtonPart:
    case DefaultButtonPart:
    case ButtonPart:
    case SquareButtonPart:
        return zoomedBox;
    default:
        break;
    }

    return Theme::controlBorder(part, font, zoomedBox, zoomFactor);
}

void ThemeAdwaita::paint(ControlPart part, ControlStates& states, GraphicsContext& context, const FloatRect& zoomedRect, float zoomFactor, ScrollView*, float, float, bool, bool)
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
    case InnerSpinButtonPart:
        paintSpinButton(states, context, zoomedRect, zoomFactor);
        break;
    default:
        break;
    }
}

void ThemeAdwaita::paintCheckbox(ControlStates& states, GraphicsContext& graphicsContext, const FloatRect& zoomedRect, float)
{
    GraphicsContextStateSaver stateSaver(graphicsContext);

    FloatRect fieldRect = zoomedRect;
    if (fieldRect.width() != fieldRect.height()) {
        auto buttonSize = std::min(fieldRect.width(), fieldRect.height());
        fieldRect.setSize({ buttonSize, buttonSize });
        if (fieldRect.width() != zoomedRect.width())
            fieldRect.move((zoomedRect.width() - fieldRect.width()) / 2.0, 0);
        else
            fieldRect.move(0, (zoomedRect.height() - fieldRect.height()) / 2.0);
    }

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

void ThemeAdwaita::paintRadio(ControlStates& states, GraphicsContext& graphicsContext, const FloatRect& zoomedRect, float)
{
    GraphicsContextStateSaver stateSaver(graphicsContext);
    FloatRect fieldRect = zoomedRect;
    if (fieldRect.width() != fieldRect.height()) {
        auto buttonSize = std::min(fieldRect.width(), fieldRect.height());
        fieldRect.setSize({ buttonSize, buttonSize });
        if (fieldRect.width() != zoomedRect.width())
            fieldRect.move((zoomedRect.width() - fieldRect.width()) / 2.0, 0);
        else
            fieldRect.move(0, (zoomedRect.height() - fieldRect.height()) / 2.0);
    }

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

void ThemeAdwaita::paintButton(ControlStates& states, GraphicsContext& graphicsContext, const FloatRect& zoomedRect, float)
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

void ThemeAdwaita::paintSpinButton(ControlStates& states, GraphicsContext& graphicsContext, const FloatRect& zoomedRect, float)
{
    GraphicsContextStateSaver stateSaver(graphicsContext);

    FloatRect fieldRect = zoomedRect;
    FloatSize corner(2, 2);
    Path path;
    path.addRoundedRect(fieldRect, corner);
    fieldRect.inflate(-buttonBorderSize);
    path.addRoundedRect(fieldRect, corner);
    graphicsContext.setFillRule(WindRule::EvenOdd);
    graphicsContext.setFillColor(spinButtonBorderColor);
    graphicsContext.fillPath(path);
    path.clear();

    path.addRoundedRect(fieldRect, corner);
    graphicsContext.setFillRule(WindRule::NonZero);
    graphicsContext.setFillColor(spinButtonBackgroundColor);
    graphicsContext.fillPath(path);
    path.clear();

    FloatRect buttonRect = fieldRect;
    buttonRect.setHeight(fieldRect.height() / 2.0);
    {
        if (states.states() & ControlStates::SpinUpState) {
            path.addRoundedRect(FloatRoundedRect(buttonRect, corner, corner, { }, { }));
            if (states.states() & ControlStates::PressedState)
                graphicsContext.setFillColor(spinButtonBackgroundPressedColor);
            else if (states.states() & ControlStates::HoverState)
                graphicsContext.setFillColor(spinButtonBackgroundHoveredColor);
            graphicsContext.fillPath(path);
            path.clear();
        }

        GraphicsContextStateSaver buttonStateSaver(graphicsContext);
        if (buttonRect.height() > arrowSize)
            graphicsContext.translate(buttonRect.x(), buttonRect.y() + (buttonRect.height() / 2.0) - (arrowSize / 2.));
        else {
            graphicsContext.translate(buttonRect.x(), buttonRect.y());
            graphicsContext.scale(FloatSize::narrowPrecision(buttonRect.width() / arrowSize, buttonRect.height() / arrowSize));
        }
        paintArrow(graphicsContext, ArrowDirection::Up);
    }

    buttonRect.move(0, buttonRect.height());
    {
        if (!(states.states() & ControlStates::SpinUpState)) {
            path.addRoundedRect(FloatRoundedRect(buttonRect, { }, { }, corner, corner));
            if (states.states() & ControlStates::PressedState)
                graphicsContext.setFillColor(spinButtonBackgroundPressedColor);
            else if (states.states() & ControlStates::HoverState)
                graphicsContext.setFillColor(spinButtonBackgroundHoveredColor);
            graphicsContext.fillPath(path);
            path.clear();
        }

        GraphicsContextStateSaver buttonStateSaver(graphicsContext);
        if (buttonRect.height() > arrowSize)
            graphicsContext.translate(buttonRect.x(), buttonRect.y() + (buttonRect.height() / 2.0) - (arrowSize / 2.));
        else {
            graphicsContext.translate(buttonRect.x(), buttonRect.y());
            graphicsContext.scale(FloatSize::narrowPrecision(buttonRect.width() / arrowSize, buttonRect.height() / arrowSize));
        }
        paintArrow(graphicsContext, ArrowDirection::Down);
    }
}

} // namespace WebCore
