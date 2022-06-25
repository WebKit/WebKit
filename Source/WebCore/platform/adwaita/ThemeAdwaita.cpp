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
#include "ColorBlending.h"
#include "ControlStates.h"
#include "FloatRoundedRect.h"
#include "GraphicsContext.h"
#include "LengthSize.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {

static const unsigned focusLineWidth = 1;
static constexpr auto focusRingColorLight = SRGBA<uint8_t> { 46, 52, 54, 150 };
static constexpr auto focusRingColorDark = SRGBA<uint8_t> { 238, 238, 236, 150 };
static const unsigned arrowSize = 16;
static constexpr auto arrowColorLight = SRGBA<uint8_t> { 46, 52, 54 };
static constexpr auto arrowColorDark = SRGBA<uint8_t> { 238, 238, 236 };
static const int buttonFocusOffset = -3;
static const unsigned buttonPadding = 5;
static const int buttonBorderSize = 1; // Keep in sync with menuListButtonBorderSize in RenderThemeAdwaita.
static const double disabledOpacity = 0.5;

static constexpr auto buttonBorderColorLight = SRGBA<uint8_t> { 0, 0, 0, 50 };
static constexpr auto buttonBackgroundColorLight = SRGBA<uint8_t> { 244, 244, 244 };
static constexpr auto buttonBackgroundPressedColorLight = SRGBA<uint8_t> { 214, 214, 214 };
static constexpr auto buttonBackgroundHoveredColorLight = SRGBA<uint8_t> { 248, 248, 248 };
static constexpr auto toggleBorderColorLight = SRGBA<uint8_t> { 0, 0, 0, 50 };
static constexpr auto toggleBorderHoveredColorLight = SRGBA<uint8_t> { 0, 0, 0, 80 };

static constexpr auto buttonBorderColorDark = SRGBA<uint8_t> { 255, 255, 255, 50 };
static constexpr auto buttonBackgroundColorDark = SRGBA<uint8_t> { 52, 52, 52 };
static constexpr auto buttonBackgroundPressedColorDark = SRGBA<uint8_t> { 30, 30, 30 };
static constexpr auto buttonBackgroundHoveredColorDark = SRGBA<uint8_t> { 60, 60, 60 };
static constexpr auto toggleBorderColorDark = SRGBA<uint8_t> { 255, 255, 255, 50 };
static constexpr auto toggleBorderHoveredColorDark = SRGBA<uint8_t> { 255, 255, 255, 80 };

static const double toggleSize = 14.;
static const int toggleBorderSize = 2;
static const int toggleFocusOffset = 2;

static constexpr auto spinButtonBorderColorLight = SRGBA<uint8_t> { 0, 0, 0, 25 };
static constexpr auto spinButtonBackgroundColorLight = Color::white;
static constexpr auto spinButtonBackgroundHoveredColorLight = SRGBA<uint8_t> { 0, 0, 0, 50 };
static constexpr auto spinButtonBackgroundPressedColorLight = SRGBA<uint8_t> { 0, 0, 0, 70 };

static constexpr auto spinButtonBorderColorDark = SRGBA<uint8_t> { 255, 255, 255, 25 };
static constexpr auto spinButtonBackgroundColorDark = SRGBA<uint8_t> { 45, 45, 45 };
static constexpr auto spinButtonBackgroundHoveredColorDark = SRGBA<uint8_t> { 255, 255, 255, 50 };
static constexpr auto spinButtonBackgroundPressedColorDark = SRGBA<uint8_t> { 255, 255, 255, 70 };

Theme& Theme::singleton()
{
    static NeverDestroyed<ThemeAdwaita> theme;
    return theme;
}

Color ThemeAdwaita::focusColor(bool useDarkAppearance)
{
    return useDarkAppearance ? focusRingColorDark : focusRingColorLight;
}

void ThemeAdwaita::paintFocus(GraphicsContext& graphicsContext, const FloatRect& rect, int offset, bool useDarkAppearance)
{
    FloatRect focusRect = rect;
    focusRect.inflate(offset);
    Path path;
    path.addRoundedRect(focusRect, { 2, 2 });
    paintFocus(graphicsContext, path, focusColor(useDarkAppearance));
}

void ThemeAdwaita::paintFocus(GraphicsContext& graphicsContext, const Path& path, const Color& color)
{
    GraphicsContextStateSaver stateSaver(graphicsContext);

    graphicsContext.beginTransparencyLayer(color.alphaAsFloat());
    graphicsContext.setStrokeThickness(focusLineWidth);
    graphicsContext.setLineDash({ focusLineWidth, 2 * focusLineWidth }, 0);
    graphicsContext.setLineCap(LineCap::Square);
    graphicsContext.setLineJoin(LineJoin::Miter);
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

void ThemeAdwaita::paintArrow(GraphicsContext& graphicsContext, ArrowDirection direction, bool useDarkAppearance)
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

    graphicsContext.setFillColor(useDarkAppearance ? arrowColorDark : arrowColorLight);
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
            buttonSize.width = Length(12, LengthType::Fixed);
        if (buttonSize.height.isIntrinsicOrAuto())
            buttonSize.height = Length(12, LengthType::Fixed);
        return buttonSize;
    }
    case InnerSpinButtonPart: {
        LengthSize spinButtonSize = zoomedSize;
        if (spinButtonSize.width.isIntrinsicOrAuto())
            spinButtonSize.width = Length(static_cast<int>(arrowSize), LengthType::Fixed);
        if (spinButtonSize.height.isIntrinsicOrAuto() || fontCascade.pixelSize() > static_cast<int>(arrowSize))
            spinButtonSize.height = Length(fontCascade.pixelSize(), LengthType::Fixed);
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
        minSize.width = Length(0, LengthType::Fixed);
    if (minSize.height.isIntrinsicOrAuto())
        minSize.height = Length(0, LengthType::Fixed);
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

void ThemeAdwaita::paint(ControlPart part, ControlStates& states, GraphicsContext& context, const FloatRect& zoomedRect, float, ScrollView*, float, float, bool, bool useDarkAppearance, const Color& effectiveAccentColor)
{
    switch (part) {
    case CheckboxPart:
        paintCheckbox(states, context, zoomedRect, useDarkAppearance, effectiveAccentColor);
        break;
    case RadioPart:
        paintRadio(states, context, zoomedRect, useDarkAppearance, effectiveAccentColor);
        break;
    case PushButtonPart:
    case DefaultButtonPart:
    case ButtonPart:
    case SquareButtonPart:
        paintButton(states, context, zoomedRect, useDarkAppearance);
        break;
    case InnerSpinButtonPart:
        paintSpinButton(states, context, zoomedRect, useDarkAppearance);
        break;
    default:
        break;
    }
}

void ThemeAdwaita::paintCheckbox(ControlStates& states, GraphicsContext& graphicsContext, const FloatRect& zoomedRect, bool useDarkAppearance, const Color& effectiveAccentColor)
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

    SRGBA<uint8_t> toggleBorderColor;
    SRGBA<uint8_t> toggleBorderHoverColor;

    if (useDarkAppearance) {
        toggleBorderColor = toggleBorderColorDark;
        toggleBorderHoverColor = toggleBorderHoveredColorDark;
    } else {
        toggleBorderColor = toggleBorderColorLight;
        toggleBorderHoverColor = toggleBorderHoveredColorLight;
    }

    Color accentColor = effectiveAccentColor.isValid() ? effectiveAccentColor : m_accentColor;
    Color foregroundColor = accentColor.luminance() > 0.5 ? Color(SRGBA<uint8_t> { 0, 0, 0, 204 }) : Color::white;
    Color accentHoverColor = blendSourceOver(accentColor, foregroundColor.colorWithAlphaMultipliedBy(0.1));

    if (!states.states().contains(ControlStates::States::Enabled))
        graphicsContext.beginTransparencyLayer(disabledOpacity);

    FloatSize corner(2, 2);
    Path path;

    if (states.states().containsAny({ ControlStates::States::Checked, ControlStates::States::Indeterminate })) {
        path.addRoundedRect(fieldRect, corner);
        graphicsContext.setFillRule(WindRule::NonZero);
        if (states.states().contains(ControlStates::States::Hovered) && states.states().contains(ControlStates::States::Enabled))
            graphicsContext.setFillColor(accentHoverColor);
        else
            graphicsContext.setFillColor(accentColor);
        graphicsContext.fillPath(path);
        path.clear();

        GraphicsContextStateSaver checkedStateSaver(graphicsContext);
        graphicsContext.translate(fieldRect.x(), fieldRect.y());
        graphicsContext.scale(FloatSize::narrowPrecision(fieldRect.width() / toggleSize, fieldRect.height() / toggleSize));
        if (states.states().contains(ControlStates::States::Indeterminate))
            path.addRoundedRect(FloatRect(2, 5, 10, 4), corner);
        else {
            path.moveTo({ 2.43, 6.57 });
            path.addLineTo({ 7.5, 11.63 });
            path.addLineTo({ 14, 5 });
            path.addLineTo({ 14, 1 });
            path.addLineTo({ 7.5, 7.38 });
            path.addLineTo({ 4.56, 4.44 });
            path.closeSubpath();
        }

        graphicsContext.setFillColor(foregroundColor);

        graphicsContext.fillPath(path);
        path.clear();
    } else {
        path.addRoundedRect(fieldRect, corner);
        fieldRect.inflate(-toggleBorderSize);
        corner.expand(-buttonBorderSize, -buttonBorderSize);
        path.addRoundedRect(fieldRect, corner);
        graphicsContext.setFillRule(WindRule::EvenOdd);
        if (states.states().contains(ControlStates::States::Hovered) && states.states().contains(ControlStates::States::Enabled))
            graphicsContext.setFillColor(toggleBorderHoverColor);
        else
            graphicsContext.setFillColor(toggleBorderColor);
        graphicsContext.fillPath(path);
        path.clear();
    }

    if (states.states().contains(ControlStates::States::Focused))
        paintFocus(graphicsContext, zoomedRect, toggleFocusOffset, useDarkAppearance);

    if (!states.states().contains(ControlStates::States::Enabled))
        graphicsContext.endTransparencyLayer();
}

void ThemeAdwaita::paintRadio(ControlStates& states, GraphicsContext& graphicsContext, const FloatRect& zoomedRect, bool useDarkAppearance, const Color& effectiveAccentColor)
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

    SRGBA<uint8_t> toggleBorderColor;
    SRGBA<uint8_t> toggleBorderHoverColor;

    if (useDarkAppearance) {
        toggleBorderColor = toggleBorderColorDark;
        toggleBorderHoverColor = toggleBorderHoveredColorDark;
    } else {
        toggleBorderColor = toggleBorderColorLight;
        toggleBorderHoverColor = toggleBorderHoveredColorLight;
    }

    Color accentColor = effectiveAccentColor.isValid() ? effectiveAccentColor : m_accentColor;
    Color foregroundColor = accentColor.luminance() > 0.5 ? Color(SRGBA<uint8_t> { 0, 0, 0, 204 }) : Color::white;
    Color accentHoverColor = blendSourceOver(accentColor, foregroundColor.colorWithAlphaMultipliedBy(0.1));

    if (!states.states().contains(ControlStates::States::Enabled))
        graphicsContext.beginTransparencyLayer(disabledOpacity);

    Path path;

    if (states.states().containsAny({ ControlStates::States::Checked, ControlStates::States::Indeterminate })) {
        path.addEllipse(fieldRect);
        graphicsContext.setFillRule(WindRule::NonZero);
        if (states.states().contains(ControlStates::States::Hovered) && states.states().contains(ControlStates::States::Enabled))
            graphicsContext.setFillColor(accentHoverColor);
        else
            graphicsContext.setFillColor(accentColor);
        graphicsContext.fillPath(path);
        path.clear();

        fieldRect.inflate(-(fieldRect.width() - fieldRect.width() * 0.70));
        path.addEllipse(fieldRect);
        graphicsContext.setFillColor(foregroundColor);
        graphicsContext.fillPath(path);
    } else {
        path.addEllipse(fieldRect);
        fieldRect.inflate(-toggleBorderSize);
        path.addEllipse(fieldRect);
        graphicsContext.setFillRule(WindRule::EvenOdd);
        if (states.states().contains(ControlStates::States::Hovered) && states.states().contains(ControlStates::States::Enabled))
            graphicsContext.setFillColor(toggleBorderHoverColor);
        else
            graphicsContext.setFillColor(toggleBorderColor);
        graphicsContext.fillPath(path);
        path.clear();
    }

    if (states.states().contains(ControlStates::States::Focused))
        paintFocus(graphicsContext, zoomedRect, toggleFocusOffset, useDarkAppearance);

    if (!states.states().contains(ControlStates::States::Enabled))
        graphicsContext.endTransparencyLayer();
}

void ThemeAdwaita::paintButton(ControlStates& states, GraphicsContext& graphicsContext, const FloatRect& zoomedRect, bool useDarkAppearance)
{
    GraphicsContextStateSaver stateSaver(graphicsContext);

    SRGBA<uint8_t> buttonBorderColor;
    SRGBA<uint8_t> buttonBackgroundColor;
    SRGBA<uint8_t> buttonBackgroundHoveredColor;
    SRGBA<uint8_t> buttonBackgroundPressedColor;

    if (useDarkAppearance) {
        buttonBorderColor = buttonBorderColorDark;
        buttonBackgroundColor = buttonBackgroundColorDark;
        buttonBackgroundHoveredColor = buttonBackgroundHoveredColorDark;
        buttonBackgroundPressedColor = buttonBackgroundPressedColorDark;
    } else {
        buttonBorderColor = buttonBorderColorLight;
        buttonBackgroundColor = buttonBackgroundColorLight;
        buttonBackgroundHoveredColor = buttonBackgroundHoveredColorLight;
        buttonBackgroundPressedColor = buttonBackgroundPressedColorLight;
    }

    if (!states.states().contains(ControlStates::States::Enabled))
        graphicsContext.beginTransparencyLayer(disabledOpacity);

    FloatRect fieldRect = zoomedRect;
    FloatSize corner(5, 5);
    Path path;
    path.addRoundedRect(fieldRect, corner);
    fieldRect.inflate(-buttonBorderSize);
    corner.expand(-buttonBorderSize, -buttonBorderSize);
    path.addRoundedRect(fieldRect, corner);
    graphicsContext.setFillRule(WindRule::EvenOdd);
    graphicsContext.setFillColor(buttonBorderColor);
    graphicsContext.fillPath(path);
    path.clear();

    path.addRoundedRect(fieldRect, corner);
    graphicsContext.setFillRule(WindRule::NonZero);
    if (states.states().contains(ControlStates::States::Pressed))
        graphicsContext.setFillColor(buttonBackgroundPressedColor);
    else if (states.states().contains(ControlStates::States::Enabled)
        && states.states().contains(ControlStates::States::Hovered))
        graphicsContext.setFillColor(buttonBackgroundHoveredColor);
    else
        graphicsContext.setFillColor(buttonBackgroundColor);
    graphicsContext.fillPath(path);

    if (states.states().contains(ControlStates::States::Focused))
        paintFocus(graphicsContext, zoomedRect, buttonFocusOffset, useDarkAppearance);

    if (!states.states().contains(ControlStates::States::Enabled))
        graphicsContext.endTransparencyLayer();
}

void ThemeAdwaita::paintSpinButton(ControlStates& states, GraphicsContext& graphicsContext, const FloatRect& zoomedRect, bool useDarkAppearance)
{
    GraphicsContextStateSaver stateSaver(graphicsContext);

    SRGBA<uint8_t> spinButtonBorderColor;
    SRGBA<uint8_t> spinButtonBackgroundColor;
    SRGBA<uint8_t> spinButtonBackgroundHoveredColor;
    SRGBA<uint8_t> spinButtonBackgroundPressedColor;

    if (useDarkAppearance) {
        spinButtonBorderColor = spinButtonBorderColorDark;
        spinButtonBackgroundColor = spinButtonBackgroundColorDark;
        spinButtonBackgroundHoveredColor = spinButtonBackgroundHoveredColorDark;
        spinButtonBackgroundPressedColor = spinButtonBackgroundPressedColorDark;
    } else {
        spinButtonBorderColor = spinButtonBorderColorLight;
        spinButtonBackgroundColor = spinButtonBackgroundColorLight;
        spinButtonBackgroundHoveredColor = spinButtonBackgroundHoveredColorLight;
        spinButtonBackgroundPressedColor = spinButtonBackgroundPressedColorLight;
    }

    FloatRect fieldRect = zoomedRect;
    FloatSize corner(2, 2);
    Path path;
    path.addRoundedRect(fieldRect, corner);
    fieldRect.inflate(-buttonBorderSize);
    corner.expand(-buttonBorderSize, -buttonBorderSize);
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
        if (states.states().contains(ControlStates::States::SpinUp)) {
            path.addRoundedRect(FloatRoundedRect(buttonRect, corner, corner, { }, { }));
            if (states.states().contains(ControlStates::States::Pressed))
                graphicsContext.setFillColor(spinButtonBackgroundPressedColor);
            else if (states.states().contains(ControlStates::States::Hovered))
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
        paintArrow(graphicsContext, ArrowDirection::Up, useDarkAppearance);
    }

    buttonRect.move(0, buttonRect.height());
    {
        if (!states.states().contains(ControlStates::States::SpinUp)) {
            path.addRoundedRect(FloatRoundedRect(buttonRect, { }, { }, corner, corner));
            if (states.states().contains(ControlStates::States::Pressed))
                graphicsContext.setFillColor(spinButtonBackgroundPressedColor);
            else if (states.states().contains(ControlStates::States::Hovered))
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
        paintArrow(graphicsContext, ArrowDirection::Down, useDarkAppearance);
    }
}

void ThemeAdwaita::setAccentColor(const Color& color)
{
    if (m_accentColor == color)
        return;

    m_accentColor = color;

    platformColorsDidChange();
}

Color ThemeAdwaita::accentColor()
{
    return m_accentColor.isValid() ? m_accentColor : SRGBA<uint8_t> { 52, 132, 228 };
}

} // namespace WebCore
