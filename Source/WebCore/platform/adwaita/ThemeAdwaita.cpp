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
#include "FontCascade.h"
#include <wtf/NeverDestroyed.h>

#if PLATFORM(GTK) || PLATFORM(WPE)
#include "SystemSettings.h"
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
#if PLATFORM(GTK) || PLATFORM(WPE)
    refreshSettings();

    // Note that Theme is NeverDestroy'd so the destructor will never be called to disconnect this.
    SystemSettings::singleton().addObserver([this](const SystemSettings::State& state) mutable {
        if (state.enableAnimations || state.themeName)
            this->refreshSettings();
    }, this);
#endif // PLATFORM(GTK) || PLATFORM(WPE)
}

#if PLATFORM(GTK) || PLATFORM(WPE)

void ThemeAdwaita::refreshSettings()
{
    if (auto enableAnimations = SystemSettings::singleton().enableAnimations())
        m_prefersReducedMotion = !enableAnimations.value();

    // For high contrast in GTK3 we can rely on the theme name and be accurate most of the time.
    // However whether or not high-contrast is enabled is also stored in GSettings/xdg-desktop-portal.
    // We could rely on libadwaita, dynamically, to re-use its logic.
#if PLATFORM(GTK) && !USE(GTK4)
    if (auto themeName = SystemSettings::singleton().themeName())
        m_prefersContrast = themeName == "HighContrast"_s || themeName == "HighContrastInverse"_s;
#endif // PLATFORM(GTK) && !USE(GTK4)
}

#endif // PLATFORM(GTK) || PLATFORM(WPE)

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
