/*
 * Copyright (C) 2024 Igalia S.L.
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
#include "SystemSettingsManager.h"

#if PLATFORM(GTK) || PLATFORM(WPE)
#include "SystemSettingsManagerMessages.h"
#include "WebProcess.h"
#include "WebProcessCreationParameters.h"
#include <WebCore/FontRenderOptions.h>
#include <WebCore/Page.h>
#include <WebCore/RenderTheme.h>
#include <WebCore/ScrollbarTheme.h>

namespace WebKit {
using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(SystemSettingsManager);

SystemSettingsManager::SystemSettingsManager(WebProcess& process)
{
    process.addMessageReceiver(Messages::SystemSettingsManager::messageReceiverName(), *this);

    SystemSettings::singleton().addObserver([](const SystemSettings::State& state) {
        auto& systemSettings = SystemSettings::singleton();
        auto& fontRenderOptions = FontRenderOptions::singleton();
        bool antialiasSettingsDidChange = state.xftAntialias || state.xftRGBA;
        bool hintingSettingsDidChange = state.xftHinting || state.xftHintStyle;
        bool themeDidChange = state.themeName || state.darkMode;

        if (themeDidChange)
            RenderTheme::singleton().platformColorsDidChange();

        if (hintingSettingsDidChange)
            fontRenderOptions.setHinting(systemSettings.hintStyle());

        if (antialiasSettingsDidChange) {
            fontRenderOptions.setSubpixelOrder(systemSettings.subpixelOrder());
            fontRenderOptions.setAntialias(systemSettings.antialiasMode());
        }

        if (state.followFontSystemSettings)
            fontRenderOptions.setFollowSystemSettings(systemSettings.followFontSystemSettings());

        if (state.overlayScrolling || state.themeName)
            ScrollbarTheme::theme().themeChanged();

        if (themeDidChange || antialiasSettingsDidChange || hintingSettingsDidChange || state.followFontSystemSettings)
            Page::updateStyleForAllPagesAfterGlobalChangeInEnvironment();
    }, this);
}

SystemSettingsManager::~SystemSettingsManager()
{
    SystemSettings::singleton().removeObserver(this);
}

ASCIILiteral SystemSettingsManager::supplementName()
{
    return "SystemSettingsManager"_s;
}

void SystemSettingsManager::initialize(const WebProcessCreationParameters& parameters)
{
    didChange(parameters.systemSettings);
}

void SystemSettingsManager::didChange(const SystemSettings::State& state)
{
    SystemSettings::singleton().updateSettings(state);
}

} // namespace WebKit

#endif // PLATFORM(GTK) || PLATFORM(WPE)
