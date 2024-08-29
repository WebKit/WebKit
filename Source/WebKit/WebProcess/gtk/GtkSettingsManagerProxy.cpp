/*
 * Copyright (C) 2021 Purism SPC
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
#include "GtkSettingsManagerProxy.h"

#include "GtkSettingsManagerProxyMessages.h"
#include "WebProcess.h"
#include <WebCore/FontRenderOptions.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/Page.h>
#include <WebCore/RenderTheme.h>

namespace WebKit {
using namespace WebCore;

GtkSettingsManagerProxy& GtkSettingsManagerProxy::singleton()
{
    static NeverDestroyed<GtkSettingsManagerProxy> manager;
    return manager;
}

GtkSettingsManagerProxy::GtkSettingsManagerProxy()
{
    WebProcess::singleton().addMessageReceiver(Messages::GtkSettingsManagerProxy::messageReceiverName(), *this);
}

void GtkSettingsManagerProxy::settingsDidChange(GtkSettingsState&& state)
{
    applySettings(WTFMove(state));
}

void GtkSettingsManagerProxy::applySettings(GtkSettingsState&& state)
{
    bool shouldUpdateStyle = false;
    if (state.themeName) {
        m_settings.themeName = WTFMove(state.themeName);
        RenderTheme::singleton().platformColorsDidChange();
        shouldUpdateStyle = true;
    }

    if (state.fontName)
        m_settings.fontName = WTFMove(state.fontName);

    bool hintingSettingsDidChange = false;
    if (state.xftHinting) {
        m_settings.xftHinting = state.xftHinting;
        hintingSettingsDidChange = true;
    }

    if (state.xftHintStyle) {
        m_settings.xftHintStyle = WTFMove(state.xftHintStyle);
        hintingSettingsDidChange = true;
    }

    if (hintingSettingsDidChange) {
        applyHintingSettings();
        shouldUpdateStyle = true;
    }

    bool antialiasSettingsDidChange = false;
    if (state.xftAntialias) {
        m_settings.xftAntialias = state.xftAntialias;
        antialiasSettingsDidChange = true;
    }

    if (state.xftRGBA) {
        m_settings.xftRGBA = WTFMove(state.xftRGBA);
        antialiasSettingsDidChange = true;
    }

    if (antialiasSettingsDidChange) {
        applyAntialiasSettings();
        shouldUpdateStyle = true;
    }

    if (state.xftDPI)
        m_settings.xftAntialias = state.xftAntialias;

    if (state.cursorBlink)
        m_settings.cursorBlink = state.cursorBlink;

    if (state.cursorBlinkTime)
        m_settings.cursorBlinkTime = state.cursorBlinkTime;

    if (state.primaryButtonWarpsSlider)
        m_settings.primaryButtonWarpsSlider = state.primaryButtonWarpsSlider;

    if (state.overlayScrolling)
        m_settings.overlayScrolling = state.overlayScrolling;

    if (state.enableAnimations)
        m_settings.enableAnimations = state.enableAnimations;

    if (shouldUpdateStyle)
        Page::updateStyleForAllPagesAfterGlobalChangeInEnvironment();
}

void GtkSettingsManagerProxy::applyHintingSettings()
{
    std::optional<FontRenderOptions::Hinting> hintStyle;
    if (m_settings.xftHinting && !m_settings.xftHinting.value())
        hintStyle = FontRenderOptions::Hinting::None;
    else if (m_settings.xftHinting == 1) {
        if (m_settings.xftHintStyle == "hintnone"_s)
            hintStyle = FontRenderOptions::Hinting::None;
        else if (m_settings.xftHintStyle == "hintslight"_s)
            hintStyle = FontRenderOptions::Hinting::Slight;
        else if (m_settings.xftHintStyle == "hintmedium"_s)
            hintStyle = FontRenderOptions::Hinting::Medium;
        else if (m_settings.xftHintStyle == "hintfull"_s)
            hintStyle = FontRenderOptions::Hinting::Full;
    }
    FontRenderOptions::singleton().setHinting(hintStyle);
}

void GtkSettingsManagerProxy::applyAntialiasSettings()
{
    std::optional<FontRenderOptions::SubpixelOrder> subpixelOrder;
    if (m_settings.xftRGBA) {
        if (m_settings.xftRGBA == "rgb"_s)
            subpixelOrder = FontRenderOptions::SubpixelOrder::HorizontalRGB;
        else if (m_settings.xftRGBA == "bgr"_s)
            subpixelOrder = FontRenderOptions::SubpixelOrder::HorizontalBGR;
        else if (m_settings.xftRGBA == "vrgb"_s)
            subpixelOrder = FontRenderOptions::SubpixelOrder::VerticalRGB;
        else if (m_settings.xftRGBA == "vbgr"_s)
            subpixelOrder = FontRenderOptions::SubpixelOrder::VerticalBGR;
    }
    FontRenderOptions::singleton().setSubpixelOrder(subpixelOrder);

    std::optional<FontRenderOptions::Antialias> antialiasMode;
    if (m_settings.xftAntialias && !m_settings.xftAntialias.value())
        antialiasMode = FontRenderOptions::Antialias::None;
    else if (m_settings.xftAntialias == 1)
        antialiasMode = subpixelOrder.has_value() ? FontRenderOptions::Antialias::Subpixel : FontRenderOptions::Antialias::Normal;

    FontRenderOptions::singleton().setAntialias(antialiasMode);
}

} // namespace WebKit
