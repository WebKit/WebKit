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
#include <WebCore/CairoUtilities.h>
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
    : m_settings(gtk_settings_get_default())
{
    WebProcess::singleton().addMessageReceiver(Messages::GtkSettingsManagerProxy::messageReceiverName(), *this);
}

void GtkSettingsManagerProxy::settingsDidChange(GtkSettingsState&& state)
{
    applySettings(WTFMove(state));
}

void GtkSettingsManagerProxy::applySettings(GtkSettingsState&& state)
{
    if (state.themeName) {
        g_object_set(m_settings, "gtk-theme-name", state.themeName->utf8().data(), nullptr);
        RenderTheme::singleton().platformColorsDidChange();
        Page::updateStyleForAllPagesAfterGlobalChangeInEnvironment();
    }

    if (state.fontName)
        g_object_set(m_settings, "gtk-font-name", state.fontName->utf8().data(), nullptr);

    bool hintingSettingsDidChange = false;

    if (state.xftHinting) {
        g_object_set(m_settings, "gtk-xft-hinting", *state.xftHinting, nullptr);
        hintingSettingsDidChange = true;
    }

    if (state.xftHintStyle) {
        g_object_set(m_settings, "gtk-xft-hintstyle", state.xftHintStyle->utf8().data(), nullptr);
        hintingSettingsDidChange = true;
    }

    if (hintingSettingsDidChange)
        applyHintingSettings();

    bool antialiasSettingsDidChagne = false;

    if (state.xftAntialias) {
        g_object_set(m_settings, "gtk-xft-antialias", *state.xftAntialias, nullptr);
        antialiasSettingsDidChagne = true;
    }

    if (state.xftRGBA) {
        g_object_set(m_settings, "gtk-xft-rgba", state.xftRGBA->utf8().data(), nullptr);
        antialiasSettingsDidChagne = true;
    }

    if (antialiasSettingsDidChagne)
        applyAntialiasSettings();

    if (state.xftDPI)
        g_object_set(m_settings, "gtk-xft-dpi", *state.xftDPI, nullptr);

    if (state.cursorBlink)
        g_object_set(m_settings, "gtk-cursor-blink", *state.cursorBlink, nullptr);

    if (state.cursorBlinkTime)
        g_object_set(m_settings, "gtk-cursor-blink-time", *state.cursorBlinkTime, nullptr);

    if (state.primaryButtonWarpsSlider)
        g_object_set(m_settings, "gtk-primary-button-warps-slider", *state.primaryButtonWarpsSlider, nullptr);

    if (state.overlayScrolling)
        g_object_set(m_settings, "gtk-overlay-scrolling", *state.overlayScrolling, nullptr);

    if (state.enableAnimations)
        g_object_set(m_settings, "gtk-enable-animations", *state.enableAnimations, nullptr);
}

void GtkSettingsManagerProxy::applyHintingSettings()
{
    gint hinting;
    GUniqueOutPtr<char> hintStyleString;
    
    g_object_get(m_settings,
        "gtk-xft-hinting", &hinting,
        "gtk-xft-hintstyle", &hintStyleString.outPtr(),
        nullptr);

    cairo_hint_metrics_t hintMetrics = CAIRO_HINT_METRICS_ON;

    cairo_hint_style_t hintStyle = CAIRO_HINT_STYLE_DEFAULT;
    switch (hinting) {
    case 0:
        hintStyle = CAIRO_HINT_STYLE_NONE;
        break;
    case 1:
        if (hintStyleString) {
            if (!strcmp(hintStyleString.get(), "hintnone"))
                hintStyle = CAIRO_HINT_STYLE_NONE;
            else if (!strcmp(hintStyleString.get(), "hintslight"))
                hintStyle = CAIRO_HINT_STYLE_SLIGHT;
            else if (!strcmp(hintStyleString.get(), "hintmedium"))
                hintStyle = CAIRO_HINT_STYLE_MEDIUM;
            else if (!strcmp(hintStyleString.get(), "hintfull"))
                hintStyle = CAIRO_HINT_STYLE_FULL;
        }
        break;
    }

    setDefaultCairoHintOptions(hintMetrics, hintStyle);
}

void GtkSettingsManagerProxy::applyAntialiasSettings()
{
    gint antialias;
    GUniqueOutPtr<char> rgbaString;

    g_object_get(m_settings,
        "gtk-xft-antialias", &antialias,
        "gtk-xft-rgba", &rgbaString.outPtr(),
        nullptr);

    cairo_subpixel_order_t subpixelOrder = CAIRO_SUBPIXEL_ORDER_DEFAULT;
    if (rgbaString) {
        if (!strcmp(rgbaString.get(), "rgb"))
            subpixelOrder = CAIRO_SUBPIXEL_ORDER_RGB;
        else if (!strcmp(rgbaString.get(), "bgr"))
            subpixelOrder = CAIRO_SUBPIXEL_ORDER_BGR;
        else if (!strcmp(rgbaString.get(), "vrgb"))
            subpixelOrder = CAIRO_SUBPIXEL_ORDER_VRGB;
        else if (!strcmp(rgbaString.get(), "vbgr"))
            subpixelOrder = CAIRO_SUBPIXEL_ORDER_VBGR;
    }

    cairo_antialias_t antialiasMode = CAIRO_ANTIALIAS_DEFAULT;
    switch (antialias) {
    case 0:
        antialiasMode = CAIRO_ANTIALIAS_NONE;
        break;
    case 1:
        if (subpixelOrder != CAIRO_SUBPIXEL_ORDER_DEFAULT)
            antialiasMode = CAIRO_ANTIALIAS_SUBPIXEL;
        else
            antialiasMode = CAIRO_ANTIALIAS_GRAY;
        break;
    }

    setDefaultCairoAntialiasOptions(antialiasMode, subpixelOrder);
}

} // namespace WebKit
