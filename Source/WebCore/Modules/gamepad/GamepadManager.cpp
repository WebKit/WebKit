/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#include "GamepadManager.h"

#if ENABLE(GAMEPAD)

#include "Gamepad.h"
#include "GamepadProvider.h"
#include "Logging.h"
#include "NavigatorGamepad.h"
#include "PlatformGamepad.h"

namespace WebCore {

GamepadManager& GamepadManager::shared()
{
    static NeverDestroyed<GamepadManager> sharedManager;
    return sharedManager;
}

GamepadManager::GamepadManager()
    : m_isMonitoringGamepads(false)
{
}

void GamepadManager::platformGamepadConnected(PlatformGamepad& platformGamepad)
{
    for (auto& navigator : m_navigators) {
        if (!m_gamepadBlindNavigators.contains(navigator))
            navigator->gamepadConnected(platformGamepad);

        // FIXME: Fire connected event to all pages with listeners.
    }

    makeGamepadsVisibileToBlindNavigators();
}

void GamepadManager::platformGamepadDisconnected(PlatformGamepad& platformGamepad)
{
    for (auto& navigator : m_navigators) {
        if (!m_gamepadBlindNavigators.contains(navigator))
            navigator->gamepadDisconnected(platformGamepad);

        // FIXME: Fire disconnected event to all pages with listeners.
    }
}

void GamepadManager::platformGamepadInputActivity()
{
    makeGamepadsVisibileToBlindNavigators();
}

void GamepadManager::makeGamepadsVisibileToBlindNavigators()
{
    for (auto& navigator : m_gamepadBlindNavigators) {
        // FIXME: Here we notify a blind Navigator of each existing gamepad.
        // But we also need to fire the connected event to its corresponding DOMWindow objects.
        for (auto& platformGamepad : GamepadProvider::shared().platformGamepads()) {
            if (platformGamepad)
                navigator->gamepadConnected(*platformGamepad);
        }
    }

    m_gamepadBlindNavigators.clear();
}

void GamepadManager::registerNavigator(NavigatorGamepad* navigator)
{
    LOG(Gamepad, "GamepadManager registering NavigatorGamepad %p", navigator);

    ASSERT(!m_navigators.contains(navigator));
    m_navigators.add(navigator);
    m_gamepadBlindNavigators.add(navigator);

    maybeStartMonitoringGamepads();
}

void GamepadManager::unregisterNavigator(NavigatorGamepad* navigator)
{
    LOG(Gamepad, "GamepadManager unregistering NavigatorGamepad %p", navigator);

    ASSERT(m_navigators.contains(navigator));
    m_navigators.remove(navigator);
    m_gamepadBlindNavigators.remove(navigator);

    maybeStopMonitoringGamepads();
}

void GamepadManager::registerDOMWindow(DOMWindow* window)
{
    LOG(Gamepad, "GamepadManager registering DOMWindow %p", window);

    ASSERT(!m_domWindows.contains(window));
    m_domWindows.add(window);

    maybeStartMonitoringGamepads();
}

void GamepadManager::unregisterDOMWindow(DOMWindow* window)
{
    LOG(Gamepad, "GamepadManager unregistering DOMWindow %p", window);

    ASSERT(m_domWindows.contains(window));
    m_domWindows.remove(window);

    maybeStopMonitoringGamepads();
}

void GamepadManager::maybeStartMonitoringGamepads()
{
    if (m_isMonitoringGamepads)
        return;

    if (!m_navigators.isEmpty() || !m_domWindows.isEmpty()) {
        LOG(Gamepad, "GamepadManager has %i NavigatorGamepads and %i DOMWindows registered, is starting gamepad monitoring", m_navigators.size(), m_domWindows.size());
        m_isMonitoringGamepads = true;
        GamepadProvider::shared().startMonitoringGamepads(this);
    }
}

void GamepadManager::maybeStopMonitoringGamepads()
{
    if (!m_isMonitoringGamepads)
        return;

    if (m_navigators.isEmpty() && m_domWindows.isEmpty()) {
        LOG(Gamepad, "GamepadManager has no NavigatorGamepads or DOMWindows registered, is stopping gamepad monitoring");
        m_isMonitoringGamepads = false;
        GamepadProvider::shared().stopMonitoringGamepads(this);
    }
}

} // namespace WebCore

#endif // ENABLE(GAMEPAD)
