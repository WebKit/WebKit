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
#include "GtkSettingsManager.h"

#include "GtkSettingsManagerProxyMessages.h"
#include "WebProcessPool.h"

namespace WebKit {
using namespace WebCore;

GtkSettingsManager& GtkSettingsManager::singleton()
{
    static NeverDestroyed<GtkSettingsManager> manager;
    return manager;
}

String GtkSettingsManager::themeName() const
{
    if (auto* themeNameEnv = g_getenv("GTK_THEME")) {
        String name = String::fromUTF8(themeNameEnv);
        if (name.endsWith("-dark"_s) || name.endsWith("-Dark"_s) || name.endsWith(":dark"_s))
            return name.left(name.length() - 5);
        return name;
    }

    GUniqueOutPtr<char> themeNameSetting;
    g_object_get(m_settings, "gtk-theme-name", &themeNameSetting.outPtr(), nullptr);
    String name = String::fromUTF8(themeNameSetting.get());
    if (name.endsWith("-dark"_s) || name.endsWith("-Dark"_s))
        return name.left(name.length() - 5);
    return name;
}

String GtkSettingsManager::fontName() const
{
    GUniqueOutPtr<char> fontNameSetting;
    g_object_get(m_settings, "gtk-font-name", &fontNameSetting.outPtr(), nullptr);
    return String::fromUTF8(fontNameSetting.get());
}

int GtkSettingsManager::xftAntialias() const
{
    int antialiasSetting;
    g_object_get(m_settings, "gtk-xft-antialias", &antialiasSetting, nullptr);
    return antialiasSetting;
}

int GtkSettingsManager::xftHinting() const
{
    int hintingSetting;
    g_object_get(m_settings, "gtk-xft-hinting", &hintingSetting, nullptr);
    return hintingSetting;
}

String GtkSettingsManager::xftHintStyle() const
{
    GUniqueOutPtr<char> hintStyleSetting;
    g_object_get(m_settings, "gtk-xft-hintstyle", &hintStyleSetting.outPtr(), nullptr);
    return String::fromUTF8(hintStyleSetting.get());
}

String GtkSettingsManager::xftRGBA() const
{
    GUniqueOutPtr<char> rgbaSetting;
    g_object_get(m_settings, "gtk-xft-rgba", &rgbaSetting.outPtr(), nullptr);
    return String::fromUTF8(rgbaSetting.get());
}

int GtkSettingsManager::xftDPI() const
{
    int dpiSetting;
    g_object_get(m_settings, "gtk-xft-dpi", &dpiSetting, nullptr);
    return dpiSetting;
}

bool GtkSettingsManager::cursorBlink() const
{
    gboolean cursorBlinkSetting;
    g_object_get(m_settings, "gtk-cursor-blink", &cursorBlinkSetting, nullptr);
    return cursorBlinkSetting ? true : false;
}

int GtkSettingsManager::cursorBlinkTime() const
{
    int cursorBlinkTimeSetting;
    g_object_get(m_settings, "gtk-cursor-blink-time", &cursorBlinkTimeSetting, nullptr);
    return cursorBlinkTimeSetting;
}

bool GtkSettingsManager::primaryButtonWarpsSlider() const
{
    gboolean buttonSetting;
    g_object_get(m_settings, "gtk-primary-button-warps-slider", &buttonSetting, nullptr);
    return buttonSetting ? true : false;
}

bool GtkSettingsManager::overlayScrolling() const
{
    gboolean overlayScrollingSetting;
    g_object_get(m_settings, "gtk-overlay-scrolling", &overlayScrollingSetting, nullptr);
    return overlayScrollingSetting ? true : false;
}

bool GtkSettingsManager::enableAnimations() const
{
    gboolean enableAnimationsSetting;
    g_object_get(m_settings, "gtk-enable-animations", &enableAnimationsSetting, nullptr);
    return enableAnimationsSetting ? true : false;
}

void GtkSettingsManager::settingsDidChange()
{
    GtkSettingsState state;

    auto themeName = this->themeName();
    if (m_settingsState.themeName != themeName)
        m_settingsState.themeName = state.themeName = themeName;

    auto fontName = this->fontName();
    if (m_settingsState.fontName != fontName)
        m_settingsState.fontName = state.fontName = fontName;

    auto xftAntialias = this->xftAntialias();
    if (m_settingsState.xftAntialias != xftAntialias)
        m_settingsState.xftAntialias = state.xftAntialias = xftAntialias;

    auto xftHinting = this->xftHinting();
    if (m_settingsState.xftHinting != xftHinting)
        m_settingsState.xftHinting = state.xftHinting = xftHinting;

    auto xftHintStyle = this->xftHintStyle();
    if (m_settingsState.xftHintStyle != xftHintStyle)
        m_settingsState.xftHintStyle = state.xftHintStyle = xftHintStyle;

    auto xftRGBA = this->xftRGBA();
    if (m_settingsState.xftRGBA != xftRGBA)
        m_settingsState.xftRGBA = state.xftRGBA = xftRGBA;

    auto xftDPI = this->xftDPI();
    if (m_settingsState.xftDPI != xftDPI)
        m_settingsState.xftDPI = state.xftDPI = xftDPI;

    auto cursorBlink = this->cursorBlink();
    if (m_settingsState.cursorBlink != cursorBlink)
        m_settingsState.cursorBlink = state.cursorBlink = cursorBlink;

    auto cursorBlinkTime = this->cursorBlinkTime();
    if (m_settingsState.cursorBlinkTime != cursorBlinkTime)
        m_settingsState.cursorBlinkTime = state.cursorBlinkTime = cursorBlinkTime;

    auto primaryButtonWarpsSlider = this->primaryButtonWarpsSlider();
    if (m_settingsState.primaryButtonWarpsSlider != primaryButtonWarpsSlider)
        m_settingsState.primaryButtonWarpsSlider = state.primaryButtonWarpsSlider = primaryButtonWarpsSlider;

    auto overlayScrolling = this->overlayScrolling();
    if (m_settingsState.overlayScrolling != overlayScrolling)
        m_settingsState.overlayScrolling = state.overlayScrolling = overlayScrolling;

    auto enableAnimations = this->enableAnimations();
    if (m_settingsState.enableAnimations != enableAnimations)
        m_settingsState.enableAnimations = state.enableAnimations = enableAnimations;

    for (const auto& observer : m_observers.values())
        observer(state);

    for (auto& processPool : WebProcessPool::allProcessPools())
        processPool->sendToAllProcesses(Messages::GtkSettingsManagerProxy::SettingsDidChange(state));
}

GtkSettingsManager::GtkSettingsManager()
    : m_settings(gtk_settings_get_default())
{
    auto settingsChangedCallback = +[](GtkSettingsManager* settingsManager) {
        settingsManager->settingsDidChange();
    };

    m_settingsState.themeName = themeName();
    m_settingsState.fontName = fontName();
    m_settingsState.xftAntialias = xftAntialias();
    m_settingsState.xftHinting = xftHinting();
    m_settingsState.xftHintStyle = xftHintStyle();
    m_settingsState.xftRGBA = xftRGBA();
    m_settingsState.xftDPI = xftDPI();
    m_settingsState.cursorBlink = cursorBlink();
    m_settingsState.cursorBlinkTime = cursorBlinkTime();
    m_settingsState.primaryButtonWarpsSlider = primaryButtonWarpsSlider();
    m_settingsState.overlayScrolling = overlayScrolling();
    m_settingsState.enableAnimations = enableAnimations();

    g_signal_connect_swapped(m_settings, "notify::gtk-theme-name", G_CALLBACK(settingsChangedCallback), this);
    g_signal_connect_swapped(m_settings, "notify::gtk-font-name", G_CALLBACK(settingsChangedCallback), this);
    g_signal_connect_swapped(m_settings, "notify::gtk-xft-antialias", G_CALLBACK(settingsChangedCallback), this);
    g_signal_connect_swapped(m_settings, "notify::gtk-xft-dpi", G_CALLBACK(settingsChangedCallback), this);
    g_signal_connect_swapped(m_settings, "notify::gtk-xft-hinting", G_CALLBACK(settingsChangedCallback), this);
    g_signal_connect_swapped(m_settings, "notify::gtk-xft-hintstyle", G_CALLBACK(settingsChangedCallback), this);
    g_signal_connect_swapped(m_settings, "notify::gtk-xft-rgba", G_CALLBACK(settingsChangedCallback), this);
    g_signal_connect_swapped(m_settings, "notify::gtk-cursor-blink", G_CALLBACK(settingsChangedCallback), this);
    g_signal_connect_swapped(m_settings, "notify::gtk-cursor-blink-time", G_CALLBACK(settingsChangedCallback), this);
    g_signal_connect_swapped(m_settings, "notify::gtk-primary-button-warps-slider", G_CALLBACK(settingsChangedCallback), this);
    g_signal_connect_swapped(m_settings, "notify::gtk-overlay-scrolling", G_CALLBACK(settingsChangedCallback), this);
    g_signal_connect_swapped(m_settings, "notify::gtk-enable-animations", G_CALLBACK(settingsChangedCallback), this);
}

void GtkSettingsManager::addObserver(Function<void(const GtkSettingsState&)>&& handler, void* context)
{
    m_observers.add(context, WTFMove(handler));
}

void GtkSettingsManager::removeObserver(void* context)
{
    m_observers.remove(context);
}

} // namespace WebKit
