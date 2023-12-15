/*
 * Copyright (C) 2015, 2020 Igalia S.L.
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#if USE(THEME_ADWAITA)

#include "Color.h"
#include "ColorBlending.h"
#include "FloatRoundedRect.h"
#include "GraphicsContext.h"
#include "LengthSize.h"
#include <wtf/NeverDestroyed.h>

#if PLATFORM(GTK)
#include <gtk/gtk.h>
#include <wtf/glib/GUniquePtr.h>
#endif

namespace WebCore {

static const double focusRingOpacity = 0.8; // Keep in sync with focusRingOpacity in RenderThemeAdwaita.
static const unsigned focusLineWidth = 2;
static const unsigned arrowSize = 16;
static constexpr auto arrowColorLight = SRGBA<uint8_t> { 46, 52, 54 };
static constexpr auto arrowColorDark = SRGBA<uint8_t> { 238, 238, 236 };
static const int buttonFocusOffset = -2;
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
static const int toggleFocusOffset = 1;

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

ThemeAdwaita::ThemeAdwaita()
{
#if PLATFORM(GTK)
    if (auto* settings = gtk_settings_get_default()) {
        refreshGtkSettings();

        // Note that Theme is NeverDestroy'd so the destructor will never be called to disconnect this.
        g_signal_connect_swapped(G_OBJECT(settings), "notify::gtk-enable-animations", G_CALLBACK(+[](ThemeAdwaita* theme, GParamSpec*, GObject*) {
            theme->refreshGtkSettings();
        }), this);
#if !USE(GTK4)
        g_signal_connect_swapped(G_OBJECT(settings), "notify::gtk-theme-name", G_CALLBACK(+[](ThemeAdwaita* theme, GParamSpec*, GObject*) {
            theme->refreshGtkSettings();
        }), this);
#endif // !USE(GTK4)
    }

#endif // PLATFORM(GTK)
}

#if PLATFORM(GTK)

void ThemeAdwaita::refreshGtkSettings()
{
    if (auto* settings = gtk_settings_get_default()) {
        gboolean enableAnimations;
        g_object_get(settings, "gtk-enable-animations", &enableAnimations, nullptr);
        m_prefersReducedMotion = !enableAnimations;

        // For high contrast in GTK3 we can rely on the theme name and be accurate most of the time.
        // However whether or not high-contrast is enabled is also stored in GSettings/xdg-desktop-portal.
        // We could rely on libadwaita, dynamically, to re-use its logic but, no matter how we do it, the setting
        // has to be proxied over from the UI process different than how GtkSettings is.
#if !USE(GTK4)
        GUniqueOutPtr<char> gtkThemeName;
        g_object_get(settings, "gtk-theme-name", &gtkThemeName.outPtr(), nullptr);
        m_prefersContrast = !g_strcmp0(gtkThemeName.get(), "HighContrast") || !g_strcmp0(gtkThemeName.get(), "HighContrastInverse");
#endif // !USE(GTK4)
    }
}

#endif // PLATFORM(GTK)

Color ThemeAdwaita::focusColor(const Color& accentColor)
{
    return accentColor.colorWithAlphaMultipliedBy(focusRingOpacity);
}

static inline float getRectRadius(const FloatRect& rect, int offset)
{
    return (std::min(rect.width(), rect.height()) + offset) / 2;
}

void ThemeAdwaita::paintFocus(GraphicsContext& graphicsContext, const FloatRect& rect, int offset, const Color& color, PaintRounded rounded)
{
    FloatRect focusRect = rect;
    focusRect.inflate(offset);

    float radius = (rounded == PaintRounded::Yes) ? getRectRadius(rect, offset) : 2;

    Path path;
    path.addRoundedRect(focusRect, { radius, radius });
    paintFocus(graphicsContext, path, color);
}

void ThemeAdwaita::paintFocus(GraphicsContext& graphicsContext, const Path& path, const Color& color)
{
    GraphicsContextStateSaver stateSaver(graphicsContext);

    graphicsContext.beginTransparencyLayer(color.alphaAsFloat());
    // Since we cut off a half of it by erasing the rect contents, and half
    // of the stroke ends up inside that area, it needs to be twice as thick.
    graphicsContext.setStrokeThickness(focusLineWidth * 2);
    graphicsContext.setLineCap(LineCap::Round);
    graphicsContext.setLineJoin(LineJoin::Round);
    graphicsContext.setStrokeColor(color.opaqueColor());
    graphicsContext.strokePath(path);
    graphicsContext.setFillRule(WindRule::NonZero);
    graphicsContext.setCompositeOperation(CompositeOperator::Clear);
    graphicsContext.fillPath(path);
    graphicsContext.setCompositeOperation(CompositeOperator::SourceOver);
    graphicsContext.endTransparencyLayer();
}

void ThemeAdwaita::paintFocus(GraphicsContext& graphicsContext, const Vector<FloatRect>& rects, const Color& color, PaintRounded rounded)
{
    Path path;
    for (const auto& rect : rects) {
        float radius = (rounded == PaintRounded::Yes) ? getRectRadius(rect, 0) : 2;

        path.addRoundedRect(rect, { radius, radius });
    }
    paintFocus(graphicsContext, path, color);
}

void ThemeAdwaita::paintArrow(GraphicsContext& graphicsContext, const FloatRect& rect, ArrowDirection direction, bool useDarkAppearance)
{
    auto offset = rect.location();
    float size;
    if (rect.width() > rect.height()) {
        size = rect.height();
        offset.move((rect.width() - size) / 2, 0);
    } else {
        size = rect.width();
        offset.move(0, (rect.height() - size) / 2);
    }
    float zoomFactor = size / arrowSize;
    auto transform = [&](FloatPoint point) {
        point.scale(zoomFactor);
        point.moveBy(offset);
        return point;
    };
    Path path;
    switch (direction) {
    case ArrowDirection::Down:
        path.moveTo(transform({ 3, 6 }));
        path.addLineTo(transform({ 13, 6 }));
        path.addLineTo(transform({ 8, 11 }));
        break;
    case ArrowDirection::Up:
        path.moveTo(transform({ 3, 10 }));
        path.addLineTo(transform({ 8, 5 }));
        path.addLineTo(transform({ 13, 10 }));
        break;
    }
    path.closeSubpath();

    graphicsContext.setFillColor(useDarkAppearance ? arrowColorDark : arrowColorLight);
    graphicsContext.fillPath(path);
}

LengthSize ThemeAdwaita::controlSize(StyleAppearance appearance, const FontCascade& fontCascade, const LengthSize& zoomedSize, float zoomFactor) const
{
    if (!zoomedSize.width.isIntrinsicOrAuto() && !zoomedSize.height.isIntrinsicOrAuto())
        return Theme::controlSize(appearance, fontCascade, zoomedSize, zoomFactor);

    switch (appearance) {
    case StyleAppearance::Checkbox:
    case StyleAppearance::Radio: {
        LengthSize buttonSize = zoomedSize;
        if (buttonSize.width.isIntrinsicOrAuto())
            buttonSize.width = Length(12 * zoomFactor, LengthType::Fixed);
        if (buttonSize.height.isIntrinsicOrAuto())
            buttonSize.height = Length(12 * zoomFactor, LengthType::Fixed);
        return buttonSize;
    }
    case StyleAppearance::InnerSpinButton: {
        LengthSize spinButtonSize = zoomedSize;
        if (spinButtonSize.width.isIntrinsicOrAuto())
            spinButtonSize.width = Length(static_cast<int>(arrowSize * zoomFactor), LengthType::Fixed);
        if (spinButtonSize.height.isIntrinsicOrAuto() || fontCascade.size() > arrowSize)
            spinButtonSize.height = Length(fontCascade.size(), LengthType::Fixed);
        return spinButtonSize;
    }
    default:
        break;
    }

    return Theme::controlSize(appearance, fontCascade, zoomedSize, zoomFactor);
}

LengthSize ThemeAdwaita::minimumControlSize(StyleAppearance, const FontCascade&, const LengthSize& zoomedSize, float) const
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

LengthBox ThemeAdwaita::controlBorder(StyleAppearance appearance, const FontCascade& font, const LengthBox& zoomedBox, float zoomFactor) const
{
    switch (appearance) {
    case StyleAppearance::PushButton:
    case StyleAppearance::DefaultButton:
    case StyleAppearance::Button:
    case StyleAppearance::SquareButton:
        return zoomedBox;
    default:
        break;
    }

    return Theme::controlBorder(appearance, font, zoomedBox, zoomFactor);
}

void ThemeAdwaita::paint(StyleAppearance appearance, OptionSet<ControlStyle::State> states, GraphicsContext& context, const FloatRect& zoomedRect, bool useDarkAppearance, const Color& effectiveAccentColor)
{
    switch (appearance) {
    case StyleAppearance::Checkbox:
        paintCheckbox(states, context, zoomedRect, useDarkAppearance, effectiveAccentColor);
        break;
    case StyleAppearance::Radio:
        paintRadio(states, context, zoomedRect, useDarkAppearance, effectiveAccentColor);
        break;
    case StyleAppearance::PushButton:
    case StyleAppearance::DefaultButton:
    case StyleAppearance::Button:
    case StyleAppearance::SquareButton:
        paintButton(states, context, zoomedRect, useDarkAppearance);
        break;
    case StyleAppearance::InnerSpinButton:
        paintSpinButton(states, context, zoomedRect, useDarkAppearance);
        break;
    default:
        break;
    }
}

void ThemeAdwaita::paintCheckbox(OptionSet<ControlStyle::State> states, GraphicsContext& graphicsContext, const FloatRect& zoomedRect, bool useDarkAppearance, const Color& effectiveAccentColor)
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

    if (!states.contains(ControlStyle::State::Enabled))
        graphicsContext.beginTransparencyLayer(disabledOpacity);

    FloatSize corner(2, 2);
    Path path;

    if (states.containsAny({ ControlStyle::State::Checked, ControlStyle::State::Indeterminate })) {
        path.addRoundedRect(fieldRect, corner);
        graphicsContext.setFillRule(WindRule::NonZero);
        if (states.contains(ControlStyle::State::Hovered) && states.contains(ControlStyle::State::Enabled))
            graphicsContext.setFillColor(accentHoverColor);
        else
            graphicsContext.setFillColor(accentColor);
        graphicsContext.fillPath(path);
        path.clear();

        GraphicsContextStateSaver checkedStateSaver(graphicsContext);
        graphicsContext.translate(fieldRect.x(), fieldRect.y());
        graphicsContext.scale(FloatSize::narrowPrecision(fieldRect.width() / toggleSize, fieldRect.height() / toggleSize));
        if (states.contains(ControlStyle::State::Indeterminate))
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
    } else {
        path.addRoundedRect(fieldRect, corner);
        if (states.contains(ControlStyle::State::Hovered) && states.contains(ControlStyle::State::Enabled))
            graphicsContext.setFillColor(toggleBorderHoverColor);
        else
            graphicsContext.setFillColor(toggleBorderColor);
        graphicsContext.fillPath(path);
        path.clear();

        fieldRect.inflate(-toggleBorderSize);
        corner.expand(-buttonBorderSize, -buttonBorderSize);
        path.addRoundedRect(fieldRect, corner);
        graphicsContext.setFillColor(foregroundColor);
        graphicsContext.fillPath(path);
    }

    if (states.contains(ControlStyle::State::Focused))
        paintFocus(graphicsContext, zoomedRect, toggleFocusOffset, focusColor(accentColor));

    if (!states.contains(ControlStyle::State::Enabled))
        graphicsContext.endTransparencyLayer();
}

void ThemeAdwaita::paintRadio(OptionSet<ControlStyle::State> states, GraphicsContext& graphicsContext, const FloatRect& zoomedRect, bool useDarkAppearance, const Color& effectiveAccentColor)
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

    if (!states.contains(ControlStyle::State::Enabled))
        graphicsContext.beginTransparencyLayer(disabledOpacity);

    Path path;

    if (states.containsAny({ ControlStyle::State::Checked, ControlStyle::State::Indeterminate })) {
        path.addEllipseInRect(fieldRect);
        graphicsContext.setFillRule(WindRule::NonZero);
        if (states.contains(ControlStyle::State::Hovered) && states.contains(ControlStyle::State::Enabled))
            graphicsContext.setFillColor(accentHoverColor);
        else
            graphicsContext.setFillColor(accentColor);
        graphicsContext.fillPath(path);
        path.clear();

        fieldRect.inflate(-(fieldRect.width() - fieldRect.width() * 0.70));
        path.addEllipseInRect(fieldRect);
        graphicsContext.setFillColor(foregroundColor);
        graphicsContext.fillPath(path);
    } else {
        path.addEllipseInRect(fieldRect);
        if (states.contains(ControlStyle::State::Hovered) && states.contains(ControlStyle::State::Enabled))
            graphicsContext.setFillColor(toggleBorderHoverColor);
        else
            graphicsContext.setFillColor(toggleBorderColor);
        graphicsContext.fillPath(path);
        path.clear();

        fieldRect.inflate(-toggleBorderSize);
        path.addEllipseInRect(fieldRect);
        graphicsContext.setFillColor(foregroundColor);
        graphicsContext.fillPath(path);
    }

    if (states.contains(ControlStyle::State::Focused))
        paintFocus(graphicsContext, zoomedRect, toggleFocusOffset, focusColor(accentColor), PaintRounded::Yes);

    if (!states.contains(ControlStyle::State::Enabled))
        graphicsContext.endTransparencyLayer();
}

void ThemeAdwaita::paintButton(OptionSet<ControlStyle::State> states, GraphicsContext& graphicsContext, const FloatRect& zoomedRect, bool useDarkAppearance)
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

    if (!states.contains(ControlStyle::State::Enabled))
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
    if (states.contains(ControlStyle::State::Pressed))
        graphicsContext.setFillColor(buttonBackgroundPressedColor);
    else if (states.contains(ControlStyle::State::Enabled)
        && states.contains(ControlStyle::State::Hovered))
        graphicsContext.setFillColor(buttonBackgroundHoveredColor);
    else
        graphicsContext.setFillColor(buttonBackgroundColor);
    graphicsContext.fillPath(path);

    if (states.contains(ControlStyle::State::Focused))
        paintFocus(graphicsContext, zoomedRect, buttonFocusOffset, focusColor(m_accentColor));

    if (!states.contains(ControlStyle::State::Enabled))
        graphicsContext.endTransparencyLayer();
}

void ThemeAdwaita::paintSpinButton(OptionSet<ControlStyle::State> states, GraphicsContext& graphicsContext, const FloatRect& zoomedRect, bool useDarkAppearance)
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
        if (states.contains(ControlStyle::State::SpinUp)) {
            path.addRoundedRect(FloatRoundedRect(buttonRect, corner, corner, { }, { }));
            if (states.contains(ControlStyle::State::Pressed))
                graphicsContext.setFillColor(spinButtonBackgroundPressedColor);
            else if (states.contains(ControlStyle::State::Hovered))
                graphicsContext.setFillColor(spinButtonBackgroundHoveredColor);
            graphicsContext.fillPath(path);
            path.clear();
        }

        paintArrow(graphicsContext, buttonRect, ArrowDirection::Up, useDarkAppearance);
    }

    buttonRect.move(0, buttonRect.height());
    {
        if (!states.contains(ControlStyle::State::SpinUp)) {
            path.addRoundedRect(FloatRoundedRect(buttonRect, { }, { }, corner, corner));
            if (states.contains(ControlStyle::State::Pressed))
                graphicsContext.setFillColor(spinButtonBackgroundPressedColor);
            else if (states.contains(ControlStyle::State::Hovered))
                graphicsContext.setFillColor(spinButtonBackgroundHoveredColor);
            else
                graphicsContext.setFillColor(spinButtonBackgroundColor);
            graphicsContext.fillPath(path);
            path.clear();
        }

        paintArrow(graphicsContext, buttonRect, ArrowDirection::Down, useDarkAppearance);
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
    return m_accentColor;
}

bool ThemeAdwaita::userPrefersContrast() const
{
#if !USE(GTK4)
    return m_prefersContrast;
#else
    return false;
#endif
}

bool ThemeAdwaita::userPrefersReducedMotion() const
{
    return m_prefersReducedMotion;
}

} // namespace WebCore

#endif // USE(THEME_ADWAITA)
