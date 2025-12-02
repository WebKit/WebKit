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
#include "SystemSettingsManagerProxy.h"

#include <WebCore/PlatformScreen.h>
#include <WebCore/SystemSettings.h>
#include <gtk/gtk.h>
#include <wtf/glib/GUniquePtr.h>

namespace WebKit {

String SystemSettingsManagerProxy::themeName() const
{
    if (auto* themeNameEnv = g_getenv("GTK_THEME")) {
        String name = String::fromUTF8(themeNameEnv);
        if (name.endsWithIgnoringASCIICase("-dark"_s) || name.endsWith(":dark"_s))
            return name.left(name.length() - 5);
        return name;
    }

    GUniqueOutPtr<char> themeNameSetting;
    g_object_get(m_settings, "gtk-theme-name", &themeNameSetting.outPtr(), nullptr);
    String name = String::fromUTF8(themeNameSetting.get());
    if (name.endsWithIgnoringASCIICase("-dark"_s))
        return name.left(name.length() - 5);
    return name;
}

bool SystemSettingsManagerProxy::darkMode() const
{
    gboolean preferDarkTheme;
    g_object_get(m_settings, "gtk-application-prefer-dark-theme", &preferDarkTheme, nullptr);
    if (preferDarkTheme)
        return true;

    // FIXME: These are just heuristics, we should get the dark mode from libhandy/libadwaita, falling back to the settings portal.
    // Or maybe just use the settings portal, because we don't want to depend on libhandy, and maybe don't want to depend on libadwaita?
    if (const char* themeNameEnv = g_getenv("GTK_THEME")) {
        String themeNameEnvString = String::fromUTF8(themeNameEnv);
        return themeNameEnvString.endsWith("-dark"_s) || themeNameEnvString.endsWith("-Dark"_s) || themeNameEnvString.endsWith(":dark"_s);
    }

    GUniqueOutPtr<char> themeName;
    g_object_get(m_settings, "gtk-theme-name", &themeName.outPtr(), nullptr);
    if (themeName) {
        String themeNameString = String::fromUTF8(themeName.get());
        if (themeNameString.endsWith("-dark"_s) || themeNameString.endsWith("-Dark"_s))
            return true;
    }

    return false;
}

String SystemSettingsManagerProxy::fontName() const
{
    GUniqueOutPtr<char> fontNameSetting;
    g_object_get(m_settings, "gtk-font-name", &fontNameSetting.outPtr(), nullptr);
    return String::fromUTF8(fontNameSetting.get());
}

int SystemSettingsManagerProxy::xftAntialias() const
{
    int antialiasSetting;
    g_object_get(m_settings, "gtk-xft-antialias", &antialiasSetting, nullptr);
    return antialiasSetting;
}

int SystemSettingsManagerProxy::xftHinting() const
{
    int hintingSetting;
    g_object_get(m_settings, "gtk-xft-hinting", &hintingSetting, nullptr);
    return hintingSetting;
}

String SystemSettingsManagerProxy::xftHintStyle() const
{
    GUniqueOutPtr<char> hintStyleSetting;
    g_object_get(m_settings, "gtk-xft-hintstyle", &hintStyleSetting.outPtr(), nullptr);
    return String::fromUTF8(hintStyleSetting.get());
}

String SystemSettingsManagerProxy::xftRGBA() const
{
    GUniqueOutPtr<char> rgbaSetting;
    g_object_get(m_settings, "gtk-xft-rgba", &rgbaSetting.outPtr(), nullptr);
    return String::fromUTF8(rgbaSetting.get());
}

int SystemSettingsManagerProxy::xftDPI() const
{
#if !USE(GTK4)
    // The code in this conditionally-compiled block is needed in order to
    // respect the GDK_DPI_SCALE setting that was present in GTK3 as an
    // additional font scaling factor.
    if (auto* display = gdk_display_get_default()) {
        if (auto* screen = gdk_display_get_default_screen(display))
            return gdk_screen_get_resolution(screen) * 1024;
    }
#endif
    int dpiSetting;
    g_object_get(m_settings, "gtk-xft-dpi", &dpiSetting, nullptr);
    return dpiSetting;
}

bool SystemSettingsManagerProxy::followFontSystemSettings() const
{
#if USE(GTK4)
#if GTK_CHECK_VERSION(4, 16, 0)
    GtkFontRendering fontRendering;
    g_object_get(m_settings, "gtk-font-rendering", &fontRendering, nullptr);
    return fontRendering == GTK_FONT_RENDERING_MANUAL;
#else
    return false;
#endif
#endif

    return true;
}

bool SystemSettingsManagerProxy::cursorBlink() const
{
    gboolean cursorBlinkSetting;
    g_object_get(m_settings, "gtk-cursor-blink", &cursorBlinkSetting, nullptr);
    return cursorBlinkSetting ? true : false;
}

int SystemSettingsManagerProxy::cursorBlinkTime() const
{
    int cursorBlinkTimeSetting;
    g_object_get(m_settings, "gtk-cursor-blink-time", &cursorBlinkTimeSetting, nullptr);
    return cursorBlinkTimeSetting;
}

bool SystemSettingsManagerProxy::primaryButtonWarpsSlider() const
{
    gboolean buttonSetting;
    g_object_get(m_settings, "gtk-primary-button-warps-slider", &buttonSetting, nullptr);
    return buttonSetting ? true : false;
}

bool SystemSettingsManagerProxy::overlayScrolling() const
{
    gboolean overlayScrollingSetting;
    g_object_get(m_settings, "gtk-overlay-scrolling", &overlayScrollingSetting, nullptr);
    return overlayScrollingSetting ? true : false;
}

bool SystemSettingsManagerProxy::enableAnimations() const
{
    gboolean enableAnimationsSetting;
    g_object_get(m_settings, "gtk-enable-animations", &enableAnimationsSetting, nullptr);
    return enableAnimationsSetting ? true : false;
}

void SystemSettingsManagerProxy::updateFontProperties(const String& fontName, WebCore::SystemSettingsState& changedState)
{
    changedState.fontName = fontName;
    changedState.fontFamily = std::nullopt;
    changedState.fontSize = std::nullopt;
    changedState.fontWeight = std::nullopt;
    if (fontName.isEmpty())
        return;

    PangoFontDescription* pangoDescription = pango_font_description_from_string(fontName.utf8().data());
    if (!pangoDescription)
        return;

    int size = pango_font_description_get_size(pangoDescription) / PANGO_SCALE;
    // If the size of the font is in points, we need to convert it to pixels.
    if (!pango_font_description_get_size_is_absolute(pangoDescription))
        size = size * (WebCore::fontDPI() / 72.0);
    changedState.fontFamily = String { byteCast<char8_t>(unsafeSpan(pango_font_description_get_family(pangoDescription))) };
    changedState.fontSize = static_cast<float>(size);
    changedState.fontWeight = pango_font_description_get_weight(pangoDescription);
    pango_font_description_free(pangoDescription);
}

SystemSettingsManagerProxy::SystemSettingsManagerProxy()
    : m_settings(gtk_settings_get_default())
{
    auto settingsChangedCallback = +[](SystemSettingsManagerProxy* settingsManager) {
        settingsManager->settingsDidChange();
    };

    g_signal_connect_swapped(m_settings, "notify::gtk-theme-name", G_CALLBACK(settingsChangedCallback), this);
    g_signal_connect_swapped(m_settings, "notify::gtk-font-name", G_CALLBACK(settingsChangedCallback), this);
    g_signal_connect_swapped(m_settings, "notify::gtk-xft-antialias", G_CALLBACK(settingsChangedCallback), this);
    g_signal_connect_swapped(m_settings, "notify::gtk-xft-dpi", G_CALLBACK(settingsChangedCallback), this);
    g_signal_connect_swapped(m_settings, "notify::gtk-xft-hinting", G_CALLBACK(settingsChangedCallback), this);
    g_signal_connect_swapped(m_settings, "notify::gtk-xft-hintstyle", G_CALLBACK(settingsChangedCallback), this);
    g_signal_connect_swapped(m_settings, "notify::gtk-xft-rgba", G_CALLBACK(settingsChangedCallback), this);
#if GTK_CHECK_VERSION(4, 16, 0)
    g_signal_connect_swapped(m_settings, "notify::gtk-font-rendering", G_CALLBACK(settingsChangedCallback), this);
#endif
    g_signal_connect_swapped(m_settings, "notify::gtk-cursor-blink", G_CALLBACK(settingsChangedCallback), this);
    g_signal_connect_swapped(m_settings, "notify::gtk-cursor-blink-time", G_CALLBACK(settingsChangedCallback), this);
    g_signal_connect_swapped(m_settings, "notify::gtk-primary-button-warps-slider", G_CALLBACK(settingsChangedCallback), this);
    g_signal_connect_swapped(m_settings, "notify::gtk-overlay-scrolling", G_CALLBACK(settingsChangedCallback), this);
    g_signal_connect_swapped(m_settings, "notify::gtk-enable-animations", G_CALLBACK(settingsChangedCallback), this);
    g_signal_connect_swapped(m_settings, "notify::gtk-application-prefer-dark-theme", G_CALLBACK(settingsChangedCallback), this);

    settingsDidChange();
}

} // namespace WebKit

