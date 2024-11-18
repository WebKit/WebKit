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
#include "SystemSettingsManagerProxy.h"

#if ENABLE(WPE_PLATFORM)

#include <wpe/WPEDisplay.h>
#include <wpe/WPESettings.h>
#include <wtf/glib/GUniquePtr.h>

namespace WebKit {

static String getString(WPESettings* settings, const char* key, const String& defaultValue)
{
    GUniqueOutPtr<GError> error;
    const char* value;
    if (!(value = wpe_settings_get_string(settings, key, &error.outPtr()))) {
        g_warning("Failed to access %s setting: %s", key, error->message);
        return defaultValue;
    }

    return String::fromUTF8(value);
}

static bool getBool(WPESettings* settings, const char* key, bool defaultValue)
{
    GUniqueOutPtr<GError> error;
    bool value = wpe_settings_get_boolean(settings, key, &error.outPtr());
    if (error) {
        g_warning("Failed to access %s setting: %s", key, error->message);
        return defaultValue;
    }

    return value;
}

static uint32_t getUint32(WPESettings* settings, const char* key, uint32_t defaultValue)
{
    GUniqueOutPtr<GError> error;
    auto value = wpe_settings_get_uint32(settings, key, &error.outPtr());
    if (error) {
        g_warning("Failed to access %s setting: %s", key, error->message);
        return defaultValue;
    }

    return value;
}

static uint8_t getUint8(WPESettings* settings, const char* key, uint8_t defaultValue)
{
    GUniqueOutPtr<GError> error;
    auto value = wpe_settings_get_byte(settings, key, &error.outPtr());
    if (error) {
        g_warning("Failed to access %s setting: %s", key, error->message);
        return defaultValue;
    }

    return value;
}

static double getDouble(WPESettings* settings, const char* key, double defaultValue)
{
    GUniqueOutPtr<GError> error;
    auto value = wpe_settings_get_double(settings, key, &error.outPtr());
    if (error) {
        g_warning("Failed to access %s setting: %s", key, error->message);
        return defaultValue;
    }

    return value;
}

String SystemSettingsManagerProxy::themeName() const
{
    return emptyString();
}

bool SystemSettingsManagerProxy::darkMode() const
{
    return getBool(m_settings, WPE_SETTING_DARK_MODE, false);
}

String SystemSettingsManagerProxy::fontName() const
{
    return getString(m_settings, WPE_SETTING_FONT_NAME, "Sans 10"_s);
}

int SystemSettingsManagerProxy::xftAntialias() const
{
    return getBool(m_settings, WPE_SETTING_FONT_ANTIALIAS, true);
}

int SystemSettingsManagerProxy::xftHinting() const
{
    return getUint8(m_settings, WPE_SETTING_FONT_HINTING_STYLE, WPE_SETTINGS_HINTING_STYLE_SLIGHT) != WPE_SETTINGS_HINTING_STYLE_NONE;
}

String SystemSettingsManagerProxy::xftHintStyle() const
{
    switch (getUint8(m_settings, WPE_SETTING_FONT_HINTING_STYLE, WPE_SETTINGS_HINTING_STYLE_SLIGHT)) {
    case WPE_SETTINGS_HINTING_STYLE_NONE:
        return "hintnone"_s;
    case WPE_SETTINGS_HINTING_STYLE_SLIGHT:
        return "hintslight"_s;
    case WPE_SETTINGS_HINTING_STYLE_MEDIUM:
        return "hintmedium"_s;
    case WPE_SETTINGS_HINTING_STYLE_FULL:
        return "hintfull"_s;
    }
    ASSERT_NOT_REACHED();
    return "hintslight"_s;
}

String SystemSettingsManagerProxy::xftRGBA() const
{
    switch (getUint8(m_settings, WPE_SETTING_FONT_SUBPIXEL_LAYOUT, WPE_SETTINGS_SUBPIXEL_LAYOUT_RGB)) {
    case WPE_SETTINGS_SUBPIXEL_LAYOUT_RGB:
        return "rgb"_s;
    case WPE_SETTINGS_SUBPIXEL_LAYOUT_BGR:
        return "bgr"_s;
    case WPE_SETTINGS_SUBPIXEL_LAYOUT_VRGB:
        return "vrgb"_s;
    case WPE_SETTINGS_SUBPIXEL_LAYOUT_VBGR:
        return "vbgr"_s;
    }
    ASSERT_NOT_REACHED();
    return "rgb"_s;
}

int SystemSettingsManagerProxy::xftDPI() const
{
    return getDouble(m_settings, WPE_SETTING_FONT_DPI, 96.0) * 1024;
}

bool SystemSettingsManagerProxy::followFontSystemSettings() const
{
    return true;
}

bool SystemSettingsManagerProxy::cursorBlink() const
{
    return !!getUint32(m_settings, WPE_SETTING_CURSOR_BLINK_TIME, 1);
}

int SystemSettingsManagerProxy::cursorBlinkTime() const
{
    return getUint32(m_settings, WPE_SETTING_CURSOR_BLINK_TIME, 1200);
}

bool SystemSettingsManagerProxy::primaryButtonWarpsSlider() const
{
    return true;
}

bool SystemSettingsManagerProxy::overlayScrolling() const
{
    return true;
}

bool SystemSettingsManagerProxy::enableAnimations() const
{
    return !getBool(m_settings, WPE_SETTING_DISABLE_ANIMATIONS, false);
}

SystemSettingsManagerProxy::SystemSettingsManagerProxy()
    : m_settings(wpe_display_get_settings(wpe_display_get_primary()))
{
    ASSERT(m_settings);

    g_signal_connect_swapped(m_settings, "changed", G_CALLBACK(+[](SystemSettingsManagerProxy* settingsManager, const char*, GVariant*, WPESettings*) {
        settingsManager->settingsDidChange();
    }), this);

    settingsDidChange();
}

} // namespace WebKit

#endif // ENABLE(WPE_PLATFORM)
