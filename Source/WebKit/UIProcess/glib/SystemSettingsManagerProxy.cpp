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
#include "SystemSettingsManagerProxy.h"

#include "SystemSettingsManagerMessages.h"
#include "WebProcessPool.h"
#include <WebCore/SystemSettings.h>

namespace WebKit {
using namespace WebCore;

#if !PLATFORM(GTK) && (!PLATFORM(WPE) || !ENABLE(WPE_PLATFORM))

SystemSettingsManagerProxy::SystemSettingsManagerProxy() = default;

String SystemSettingsManagerProxy::themeName() const
{
    return emptyString();
}

bool SystemSettingsManagerProxy::darkMode() const
{
    return false;
}

String SystemSettingsManagerProxy::fontName() const
{
    return "Sans 10"_s;
}

int SystemSettingsManagerProxy::xftAntialias() const
{
    return -1;
}

int SystemSettingsManagerProxy::xftHinting() const
{
    return -1;
}

String SystemSettingsManagerProxy::xftHintStyle() const
{
    return emptyString();
}

String SystemSettingsManagerProxy::xftRGBA() const
{
    return "rgb"_s;
}

int SystemSettingsManagerProxy::xftDPI() const
{
    return -1;
}

bool SystemSettingsManagerProxy::followFontSystemSettings() const
{
    return false;
}

bool SystemSettingsManagerProxy::cursorBlink() const
{
    return true;
}

int SystemSettingsManagerProxy::cursorBlinkTime() const
{
    return 1200;
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
    return true;
}

#endif // !PLATFORM(GTK) && (!PLATFORM(WPE) || !ENABLE(WPE_PLATFORM))

void SystemSettingsManagerProxy::initialize()
{
    static NeverDestroyed<SystemSettingsManagerProxy> manager;
}

void SystemSettingsManagerProxy::settingsDidChange()
{
    const auto& oldState = SystemSettings::singleton().settingsState();
    SystemSettings::State changedState;

    auto themeName = this->themeName();
    if (oldState.themeName != themeName)
        changedState.themeName = themeName;

    auto darkMode = this->darkMode();
    if (oldState.darkMode != darkMode)
        changedState.darkMode = darkMode;

    auto fontName = this->fontName();
    if (oldState.fontName != fontName)
        changedState.fontName = fontName;

    auto xftAntialias = this->xftAntialias();
    if (xftAntialias != -1 && oldState.xftAntialias != xftAntialias)
        changedState.xftAntialias = xftAntialias;

    auto xftHinting = this->xftHinting();
    if (xftHinting != -1 && oldState.xftHinting != xftHinting)
        changedState.xftHinting = xftHinting;

    auto xftDPI = this->xftDPI();
    if (xftDPI != -1 && oldState.xftDPI != xftDPI)
        changedState.xftDPI = xftDPI;

    auto xftHintStyle = this->xftHintStyle();
    if (oldState.xftHintStyle != xftHintStyle)
        changedState.xftHintStyle = xftHintStyle;

    auto xftRGBA = this->xftRGBA();
    if (oldState.xftRGBA != xftRGBA)
        changedState.xftRGBA = xftRGBA;

    auto followFontSystemSettings = this->followFontSystemSettings();
    if (oldState.followFontSystemSettings != followFontSystemSettings)
        changedState.followFontSystemSettings = followFontSystemSettings;

    auto cursorBlink = this->cursorBlink();
    if (oldState.cursorBlink != cursorBlink)
        changedState.cursorBlink = cursorBlink;

    auto cursorBlinkTime = this->cursorBlinkTime();
    if (oldState.cursorBlinkTime != cursorBlinkTime)
        changedState.cursorBlinkTime = cursorBlinkTime;

    auto primaryButtonWarpsSlider = this->primaryButtonWarpsSlider();
    if (oldState.primaryButtonWarpsSlider != primaryButtonWarpsSlider)
        changedState.primaryButtonWarpsSlider = primaryButtonWarpsSlider;

    auto overlayScrolling = this->overlayScrolling();
    if (oldState.overlayScrolling != overlayScrolling)
        changedState.overlayScrolling = overlayScrolling;

    auto enableAnimations = this->enableAnimations();
    if (oldState.enableAnimations != enableAnimations)
        changedState.enableAnimations = enableAnimations;

    for (auto& processPool : WebProcessPool::allProcessPools())
        processPool->sendToAllProcesses(Messages::SystemSettingsManager::DidChange(changedState));

    SystemSettings::singleton().updateSettings(changedState);
}

} // namespace WebKit
