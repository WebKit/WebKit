/*
 * Copyright (C) 2024 Igalia S.L.
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
#include "SystemSettings.h"

#if PLATFORM(GTK) || PLATFORM(WPE)

namespace WebCore {

SystemSettings& SystemSettings::singleton()
{
    static NeverDestroyed<SystemSettings> settings;
    return settings;
}

SystemSettings::SystemSettings() = default;

void SystemSettings::updateSettings(const SystemSettings::State& state)
{
    if (state.themeName)
        m_state.themeName = state.themeName;

    if (state.darkMode)
        m_state.darkMode = state.darkMode;

    if (state.fontName)
        m_state.fontName = state.fontName;

    if (state.xftHinting)
        m_state.xftHinting = state.xftHinting;

    if (state.xftHintStyle)
        m_state.xftHintStyle = state.xftHintStyle;

    if (state.xftAntialias)
        m_state.xftAntialias = state.xftAntialias;

    if (state.xftRGBA)
        m_state.xftRGBA = state.xftRGBA;

    if (state.xftDPI)
        m_state.xftDPI = state.xftDPI;

    if (state.followFontSystemSettings)
        m_state.followFontSystemSettings = state.followFontSystemSettings;

    if (state.cursorBlink)
        m_state.cursorBlink = state.cursorBlink;

    if (state.cursorBlinkTime)
        m_state.cursorBlinkTime = state.cursorBlinkTime;

    if (state.primaryButtonWarpsSlider)
        m_state.primaryButtonWarpsSlider = state.primaryButtonWarpsSlider;

    if (state.overlayScrolling)
        m_state.overlayScrolling = state.overlayScrolling;

    if (state.enableAnimations)
        m_state.enableAnimations = state.enableAnimations;

    for (auto* context : copyToVector(m_observers.keys())) {
        const auto it = m_observers.find(context);
        if (it == m_observers.end())
            continue;
        it->value(state);
    }
}

std::optional<FontRenderOptions::Hinting> SystemSettings::hintStyle() const
{
    std::optional<FontRenderOptions::Hinting> hintStyle;
    if (m_state.xftHinting && !m_state.xftHinting.value())
        hintStyle = FontRenderOptions::Hinting::None;
    else if (m_state.xftHinting == 1) {
        if (m_state.xftHintStyle == "hintnone"_s)
            hintStyle = FontRenderOptions::Hinting::None;
        else if (m_state.xftHintStyle == "hintslight"_s)
            hintStyle = FontRenderOptions::Hinting::Slight;
        else if (m_state.xftHintStyle == "hintmedium"_s)
            hintStyle = FontRenderOptions::Hinting::Medium;
        else if (m_state.xftHintStyle == "hintfull"_s)
            hintStyle = FontRenderOptions::Hinting::Full;
    }

    return hintStyle;
}

std::optional<FontRenderOptions::SubpixelOrder> SystemSettings::subpixelOrder() const
{
    std::optional<FontRenderOptions::SubpixelOrder> subpixelOrder;
    if (m_state.xftRGBA) {
        if (m_state.xftRGBA == "rgb"_s)
            subpixelOrder = FontRenderOptions::SubpixelOrder::HorizontalRGB;
        else if (m_state.xftRGBA == "bgr"_s)
            subpixelOrder = FontRenderOptions::SubpixelOrder::HorizontalBGR;
        else if (m_state.xftRGBA == "vrgb"_s)
            subpixelOrder = FontRenderOptions::SubpixelOrder::VerticalRGB;
        else if (m_state.xftRGBA == "vbgr"_s)
            subpixelOrder = FontRenderOptions::SubpixelOrder::VerticalBGR;
    }
    return subpixelOrder;
}

std::optional<FontRenderOptions::Antialias> SystemSettings::antialiasMode() const
{
    std::optional<FontRenderOptions::Antialias> antialiasMode;
    if (m_state.xftAntialias && !m_state.xftAntialias.value())
        antialiasMode = FontRenderOptions::Antialias::None;
    else if (m_state.xftAntialias == 1)
        antialiasMode = subpixelOrder().has_value() ? FontRenderOptions::Antialias::Subpixel : FontRenderOptions::Antialias::Normal;

    return antialiasMode;
}

String SystemSettings::defaultSystemFont() const
{
    auto fontDescription = fontName();
    if (!fontDescription || fontDescription->isEmpty())
        return "Sans"_s;

    // We need to remove the size from the value of the property,
    // which is separated from the font family using a space.
    if (auto index = fontDescription->reverseFind(' '); index != notFound)
        fontDescription = fontDescription->left(index);
    return *fontDescription;
}

void SystemSettings::addObserver(Function<void(const SystemSettings::State&)>&& handler, void* context)
{
    m_observers.add(context, WTFMove(handler));
}

void SystemSettings::removeObserver(void* context)
{
    m_observers.remove(context);
}

} // namespace WebCore

#endif // PLATFORM(GTK) || PLATFORM(WPE)
