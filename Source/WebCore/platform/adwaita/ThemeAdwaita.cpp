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

#include "Adwaita.h"
#include "Color.h"
#include "GraphicsContext.h"
#include "LengthSize.h"
#include <wtf/NeverDestroyed.h>

#if PLATFORM(GTK)
#include <gtk/gtk.h>
#include <wtf/glib/GUniquePtr.h>
#endif

namespace WebCore {
using namespace WebCore::Adwaita;

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
