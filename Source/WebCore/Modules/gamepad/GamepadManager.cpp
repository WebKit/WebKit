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

#include "DOMWindow.h"
#include "Document.h"
#include "EventNames.h"
#include "Gamepad.h"
#include "GamepadEvent.h"
#include "GamepadProvider.h"
#include "Logging.h"
#include "NavigatorGamepad.h"
#include "PlatformGamepad.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {

static NavigatorGamepad* navigatorGamepadFromDOMWindow(DOMWindow* window)
{
    return NavigatorGamepad::from(&window->navigator());
}

GamepadManager& GamepadManager::singleton()
{
    static NeverDestroyed<GamepadManager> sharedManager;
    return sharedManager;
}

GamepadManager::GamepadManager()
    : m_isMonitoringGamepads(false)
{
}

void GamepadManager::platformGamepadConnected(PlatformGamepad& platformGamepad, EventMakesGamepadsVisible eventVisibility)
{
    if (eventVisibility == EventMakesGamepadsVisible::No)
        return;

    // Notify blind Navigators and Windows about all gamepads except for this one.
    for (auto* gamepad : GamepadProvider::singleton().platformGamepads()) {
        if (!gamepad || gamepad == &platformGamepad)
            continue;

        makeGamepadVisible(*gamepad, m_gamepadBlindNavigators, m_gamepadBlindDOMWindows);
    }

    m_gamepadBlindNavigators.clear();
    m_gamepadBlindDOMWindows.clear();

    // Notify everyone of this new gamepad.
    makeGamepadVisible(platformGamepad, m_navigators, m_domWindows);
}

void GamepadManager::platformGamepadDisconnected(PlatformGamepad& platformGamepad)
{
    Vector<WeakPtr<DOMWindow>> weakWindows;
    for (auto* domWindow : m_domWindows)
        weakWindows.append(makeWeakPtr(*domWindow));

    HashSet<NavigatorGamepad*> notifiedNavigators;

    // Handle the disconnect for all DOMWindows with event listeners and their Navigators.
    for (auto& window : weakWindows) {
        // Event dispatch might have made this window go away.
        if (!window)
            continue;

        // This DOMWindow's Navigator might not be accessible. e.g. The DOMWindow might be in the back/forward cache.
        // If this happens the DOMWindow will not get this gamepaddisconnected event.
        NavigatorGamepad* navigator = navigatorGamepadFromDOMWindow(window.get());
        if (!navigator)
            continue;

        // If this Navigator hasn't seen gamepads yet then its Window should not get the disconnect event.
        if (m_gamepadBlindNavigators.contains(navigator))
            continue;

        Ref<Gamepad> gamepad(navigator->gamepadFromPlatformGamepad(platformGamepad));

        navigator->gamepadDisconnected(platformGamepad);
        notifiedNavigators.add(navigator);

        window->dispatchEvent(GamepadEvent::create(eventNames().gamepaddisconnectedEvent, gamepad.get()), window->document());
    }

    // Notify all the Navigators that haven't already been notified.
    for (auto* navigator : m_navigators) {
        if (!notifiedNavigators.contains(navigator))
            navigator->gamepadDisconnected(platformGamepad);
    }
}

void GamepadManager::platformGamepadInputActivity(EventMakesGamepadsVisible eventVisibility)
{
    if (eventVisibility == EventMakesGamepadsVisible::No)
        return;

    if (m_gamepadBlindNavigators.isEmpty() && m_gamepadBlindDOMWindows.isEmpty())
        return;

    for (auto* gamepad : GamepadProvider::singleton().platformGamepads()) {
        if (gamepad)
            makeGamepadVisible(*gamepad, m_gamepadBlindNavigators, m_gamepadBlindDOMWindows);
    }

    m_gamepadBlindNavigators.clear();
    m_gamepadBlindDOMWindows.clear();
}

void GamepadManager::makeGamepadVisible(PlatformGamepad& platformGamepad, HashSet<NavigatorGamepad*>& navigatorSet, HashSet<DOMWindow*>& domWindowSet)
{
    if (navigatorSet.isEmpty() && domWindowSet.isEmpty())
        return;

    for (auto* navigator : navigatorSet)
        navigator->gamepadConnected(platformGamepad);

    Vector<WeakPtr<DOMWindow>> weakWindows;
    for (auto* domWindow : m_domWindows)
        weakWindows.append(makeWeakPtr(*domWindow));

    for (auto& window : weakWindows) {
        // Event dispatch might have made this window go away.
        if (!window)
            continue;

        // This DOMWindow's Navigator might not be accessible. e.g. The DOMWindow might be in the back/forward cache.
        // If this happens the DOMWindow will not get this gamepadconnected event.
        // The new gamepad will still be visibile to it once it is restored from the back/forward cache.
        NavigatorGamepad* navigator = navigatorGamepadFromDOMWindow(window.get());
        if (!navigator)
            continue;

        Ref<Gamepad> gamepad(navigator->gamepadFromPlatformGamepad(platformGamepad));
        window->dispatchEvent(GamepadEvent::create(eventNames().gamepadconnectedEvent, gamepad.get()), window->document());
    }
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

    // Anytime we register a DOMWindow, we should also double check that its NavigatorGamepad is registered.
    NavigatorGamepad* navigator = navigatorGamepadFromDOMWindow(window);
    ASSERT(navigator);

    if (m_navigators.add(navigator).isNewEntry)
        m_gamepadBlindNavigators.add(navigator);

    // If this DOMWindow's NavigatorGamepad was already registered but was still blind,
    // then this DOMWindow should be blind.
    if (m_gamepadBlindNavigators.contains(navigator))
        m_gamepadBlindDOMWindows.add(window);

    maybeStartMonitoringGamepads();
}

void GamepadManager::unregisterDOMWindow(DOMWindow* window)
{
    LOG(Gamepad, "GamepadManager unregistering DOMWindow %p", window);

    ASSERT(m_domWindows.contains(window));
    m_domWindows.remove(window);
    m_gamepadBlindDOMWindows.remove(window);

    maybeStopMonitoringGamepads();
}

void GamepadManager::maybeStartMonitoringGamepads()
{
    if (m_isMonitoringGamepads)
        return;

    if (!m_navigators.isEmpty() || !m_domWindows.isEmpty()) {
        LOG(Gamepad, "GamepadManager has %i NavigatorGamepads and %i DOMWindows registered, is starting gamepad monitoring", m_navigators.size(), m_domWindows.size());
        m_isMonitoringGamepads = true;
        GamepadProvider::singleton().startMonitoringGamepads(*this);
    }
}

void GamepadManager::maybeStopMonitoringGamepads()
{
    if (!m_isMonitoringGamepads)
        return;

    if (m_navigators.isEmpty() && m_domWindows.isEmpty()) {
        LOG(Gamepad, "GamepadManager has no NavigatorGamepads or DOMWindows registered, is stopping gamepad monitoring");
        m_isMonitoringGamepads = false;
        GamepadProvider::singleton().stopMonitoringGamepads(*this);
    }
}

} // namespace WebCore

#endif // ENABLE(GAMEPAD)
