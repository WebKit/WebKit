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

#pragma once

#if PLATFORM(GTK) || PLATFORM(WPE)

#include "FontRenderOptions.h"
#include <optional>
#include <wtf/HashMap.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

struct SystemSettingsState {
    std::optional<String> themeName;
    std::optional<bool> darkMode;
    std::optional<String> fontName;
    std::optional<int> xftAntialias;
    std::optional<int> xftHinting;
    std::optional<String> xftHintStyle;
    std::optional<String> xftRGBA;
    std::optional<int> xftDPI;
    std::optional<bool> followFontSystemSettings;
    std::optional<bool> cursorBlink;
    std::optional<int> cursorBlinkTime;
    std::optional<bool> primaryButtonWarpsSlider;
    std::optional<bool> overlayScrolling;
    std::optional<bool> enableAnimations;
};

class SystemSettings {
    WTF_MAKE_NONCOPYABLE(SystemSettings);
    friend NeverDestroyed<SystemSettings>;
public:
    static SystemSettings& singleton();

    using State = SystemSettingsState;

    void updateSettings(const SystemSettings::State&);

    const State& settingsState() const { return m_state; }

    void addObserver(Function<void(const State&)>&&, void* context);
    void removeObserver(void* context);

    std::optional<String> themeName() const { return m_state.themeName; }
    std::optional<bool> darkMode() const { return m_state.darkMode; }
    std::optional<String> fontName() const { return m_state.fontName; }
    std::optional<bool> cursorBlink() const { return m_state.cursorBlink; }
    std::optional<int> cursorBlinkTime() const { return m_state.cursorBlinkTime; }
    std::optional<int> xftDPI() const { return m_state.xftDPI; }
    std::optional<bool> followFontSystemSettings() const { return m_state.followFontSystemSettings; }
    std::optional<bool> overlayScrolling() const { return m_state.overlayScrolling; }
    std::optional<bool> primaryButtonWarpsSlider() const { return m_state.primaryButtonWarpsSlider; }
    std::optional<bool> enableAnimations() const { return m_state.enableAnimations; }

    std::optional<FontRenderOptions::Hinting> hintStyle() const;
    std::optional<FontRenderOptions::SubpixelOrder> subpixelOrder() const;
    std::optional<FontRenderOptions::Antialias> antialiasMode() const;

private:
    SystemSettings();

    State m_state;
    UncheckedKeyHashMap<void*, Function<void(const State&)>> m_observers;
};

} // namespace WebCore

#endif // PLATFORM(GTK) || PLATFORM(WPE)

