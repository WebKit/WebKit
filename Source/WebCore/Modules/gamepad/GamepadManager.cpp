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

#include "Document.h"
#include "EventNames.h"
#include "Gamepad.h"
#include "GamepadEvent.h"
#include "GamepadProvider.h"
#include "LocalDOMWindow.h"
#include "Logging.h"
#include "NavigatorGamepad.h"
#include "PlatformGamepad.h"
#include <wtf/NeverDestroyed.h>

#if PLATFORM(VISION)
#include "Page.h"
#endif

namespace WebCore {

static NavigatorGamepad* navigatorGamepadFromDOMWindow(LocalDOMWindow& window)
{
    return NavigatorGamepad::from(window.navigator());
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

#if PLATFORM(VISION)
void GamepadManager::findUnquarantinedNavigatorsAndWindows(WeakHashSet<NavigatorGamepad>& navigators, WeakHashSet<LocalDOMWindow, WeakPtrImplWithEventTargetData>& windows)
{
    for (auto& navigator : m_navigators) {
        if (!m_gamepadQuarantinedNavigators.contains(navigator))
            navigators.add(navigator);
    }
    for (auto& window : m_domWindows) {
        if (!m_gamepadQuarantinedDOMWindows.contains(window))
            windows.add(window);
    }
}
#endif

void GamepadManager::platformGamepadConnected(PlatformGamepad& platformGamepad, EventMakesGamepadsVisible eventVisibility)
{
    if (eventVisibility == EventMakesGamepadsVisible::No)
        return;

    // Notify blind Navigators and Windows about all gamepads except for this one.
    for (auto& gamepad : GamepadProvider::singleton().platformGamepads()) {
        if (!gamepad || gamepad == &platformGamepad)
            continue;

        makeGamepadVisible(*gamepad, m_gamepadBlindNavigators, m_gamepadBlindDOMWindows);
    }

    m_gamepadBlindNavigators.clear();
    m_gamepadBlindDOMWindows.clear();

#if PLATFORM(VISION)
    // Notify everyone not in the quarantined list of this new gamepad.
    WeakHashSet<NavigatorGamepad> navigators;
    WeakHashSet<LocalDOMWindow, WeakPtrImplWithEventTargetData> windows;
    findUnquarantinedNavigatorsAndWindows(navigators, windows);
    makeGamepadVisible(platformGamepad, navigators, windows);
#else
    // Notify everyone of this new gamepad.
    makeGamepadVisible(platformGamepad, m_navigators, m_domWindows);
#endif
}

void GamepadManager::platformGamepadDisconnected(PlatformGamepad& platformGamepad)
{
    WeakHashSet<NavigatorGamepad> notifiedNavigators;

    // Handle the disconnect for all DOMWindows with event listeners and their Navigators.
    for (auto& window : copyToVectorOf<WeakPtr<LocalDOMWindow, WeakPtrImplWithEventTargetData>>(m_domWindows)) {
        // Event dispatch might have made this window go away.
        if (!window)
            continue;

        // This LocalDOMWindow's Navigator might not be accessible. e.g. The LocalDOMWindow might be in the back/forward cache.
        // If this happens the LocalDOMWindow will not get this gamepaddisconnected event.
        NavigatorGamepad* navigator = navigatorGamepadFromDOMWindow(*window);
        if (!navigator)
            continue;

        // If this Navigator hasn't seen gamepads yet then its Window should not get the disconnect event.
        if (m_gamepadBlindNavigators.contains(*navigator))
            continue;
#if PLATFORM(VISION)
        if (m_gamepadQuarantinedNavigators.contains(*navigator))
            continue;
#endif

        Ref<Gamepad> gamepad(navigator->gamepadFromPlatformGamepad(platformGamepad));

        navigator->gamepadDisconnected(platformGamepad);
        notifiedNavigators.add(*navigator);

        window->dispatchEvent(GamepadEvent::create(eventNames().gamepaddisconnectedEvent, gamepad.get()), window->document());
    }

    // Notify all the Navigators that haven't already been notified.
    for (auto& navigator : m_navigators) {
        if (!notifiedNavigators.contains(navigator))
            navigator.gamepadDisconnected(platformGamepad);
    }
}

void GamepadManager::platformGamepadInputActivity(EventMakesGamepadsVisible eventVisibility)
{
    if (eventVisibility == EventMakesGamepadsVisible::No)
        return;

    if (m_gamepadBlindNavigators.isEmptyIgnoringNullReferences() && m_gamepadBlindDOMWindows.isEmptyIgnoringNullReferences())
        return;

    for (auto& gamepad : GamepadProvider::singleton().platformGamepads()) {
        if (gamepad)
            makeGamepadVisible(*gamepad, m_gamepadBlindNavigators, m_gamepadBlindDOMWindows);
    }

    m_gamepadBlindNavigators.clear();
    m_gamepadBlindDOMWindows.clear();
}

void GamepadManager::makeGamepadVisible(PlatformGamepad& platformGamepad, WeakHashSet<NavigatorGamepad>& navigatorSet, WeakHashSet<LocalDOMWindow, WeakPtrImplWithEventTargetData>& domWindowSet)
{
    LOG(Gamepad, "(%u) GamepadManager::makeGamepadVisible - New gamepad '%s' is visible", (unsigned)getpid(), platformGamepad.id().utf8().data());

    if (navigatorSet.isEmptyIgnoringNullReferences() && domWindowSet.isEmptyIgnoringNullReferences())
        return;

    for (auto& navigator : navigatorSet)
        navigator.gamepadConnected(platformGamepad);

    for (auto& window : copyToVectorOf<WeakPtr<LocalDOMWindow, WeakPtrImplWithEventTargetData>>(m_domWindows)) {
        // Event dispatch might have made this window go away.
        if (!window)
            continue;

        // This LocalDOMWindow's Navigator might not be accessible. e.g. The LocalDOMWindow might be in the back/forward cache.
        // If this happens the LocalDOMWindow will not get this gamepadconnected event.
        // The new gamepad will still be visibile to it once it is restored from the back/forward cache.
        NavigatorGamepad* navigator = navigatorGamepadFromDOMWindow(*window);
        if (!navigator)
            continue;

        Ref<Gamepad> gamepad(navigator->gamepadFromPlatformGamepad(platformGamepad));

        LOG(Gamepad, "(%u) GamepadManager::makeGamepadVisible - Dispatching gamepadconnected event for gamepad '%s'", (unsigned)getpid(), platformGamepad.id().utf8().data());
        window->dispatchEvent(GamepadEvent::create(eventNames().gamepadconnectedEvent, gamepad.get()), window->document());
    }
}

void GamepadManager::registerNavigator(NavigatorGamepad& navigator)
{
    LOG(Gamepad, "(%u) GamepadManager registering NavigatorGamepad %p", (unsigned)getpid(), &navigator);

    ASSERT(!m_navigators.contains(navigator));
    m_navigators.add(navigator);

#if PLATFORM(VISION)
    auto page = navigator.protectedPage();
    if (page && page->gamepadAccessGranted())
        m_gamepadBlindNavigators.add(navigator);
    else
        m_gamepadQuarantinedNavigators.add(navigator);
#else
    m_gamepadBlindNavigators.add(navigator);
#endif

    maybeStartMonitoringGamepads();
}

void GamepadManager::unregisterNavigator(NavigatorGamepad& navigator)
{
    LOG(Gamepad, "(%u) GamepadManager unregistering NavigatorGamepad %p", (unsigned)getpid(), &navigator);

    ASSERT(m_navigators.contains(navigator));
    m_navigators.remove(navigator);
    m_gamepadBlindNavigators.remove(navigator);

#if PLATFORM(VISION)
    m_gamepadQuarantinedNavigators.remove(navigator);
#endif

    maybeStopMonitoringGamepads();
}

void GamepadManager::registerDOMWindow(LocalDOMWindow& window)
{
    LOG(Gamepad, "(%u) GamepadManager registering LocalDOMWindow %p", (unsigned)getpid(), &window);

    ASSERT(!m_domWindows.contains(window));
    m_domWindows.add(window);

    // Anytime we register a LocalDOMWindow, we should also double check that its NavigatorGamepad is registered.
    NavigatorGamepad* navigator = navigatorGamepadFromDOMWindow(window);
    ASSERT(navigator);

    if (m_navigators.add(*navigator).isNewEntry) {
#if PLATFORM(VISION)
        auto page = navigator->protectedPage();
        if (page && page->gamepadAccessGranted())
            m_gamepadBlindNavigators.add(*navigator);
        else
            m_gamepadQuarantinedNavigators.add(*navigator);
#else
        m_gamepadBlindNavigators.add(*navigator);
#endif
    }

    // If this LocalDOMWindow's NavigatorGamepad was already registered but was still blind,
    // then this LocalDOMWindow should be blind.
    if (m_gamepadBlindNavigators.contains(*navigator))
        m_gamepadBlindDOMWindows.add(window);
#if PLATFORM(VISION)
    else if (m_gamepadQuarantinedNavigators.contains(*navigator))
        m_gamepadQuarantinedDOMWindows.add(window);
#endif

    maybeStartMonitoringGamepads();
}

void GamepadManager::unregisterDOMWindow(LocalDOMWindow& window)
{
    LOG(Gamepad, "(%u) GamepadManager unregistering LocalDOMWindow %p", (unsigned)getpid(), &window);

    ASSERT(m_domWindows.contains(window));
    m_domWindows.remove(window);
    m_gamepadBlindDOMWindows.remove(window);

#if PLATFORM(VISION)
    m_gamepadQuarantinedDOMWindows.remove(window);
#endif

    maybeStopMonitoringGamepads();
}

#if PLATFORM(VISION)
void GamepadManager::updateQuarantineStatus()
{
    if (m_gamepadQuarantinedNavigators.isEmptyIgnoringNullReferences() && m_gamepadQuarantinedDOMWindows.isEmptyIgnoringNullReferences())
        return;

    WeakHashSet<NavigatorGamepad> navigators;
    WeakHashSet<LocalDOMWindow, WeakPtrImplWithEventTargetData> windows;
    for (auto& navigator : m_gamepadQuarantinedNavigators) {
        auto page = navigator.protectedPage();
        if (page && page->gamepadAccessGranted()) {
            LOG(Gamepad, "(%u) GamepadManager found navigator %p to release from quarantine", (unsigned)getpid(), &navigator);
            navigators.add(navigator);
        }
    }
    for (auto& window : m_gamepadQuarantinedDOMWindows) {
        auto page = window.protectedPage();
        if (page && page->gamepadAccessGranted()) {
            LOG(Gamepad, "(%u) GamepadManager found window %p to release from quarantine", (unsigned)getpid(), &window);
            windows.add(window);
        }
    }

    if (navigators.isEmptyIgnoringNullReferences() && windows.isEmptyIgnoringNullReferences())
        return;

    for (auto& navigator : navigators) {
        m_gamepadBlindNavigators.add(navigator);
        m_gamepadQuarantinedNavigators.remove(navigator);
    }
    for (auto& window : windows) {
        m_gamepadBlindDOMWindows.add(window);
        m_gamepadQuarantinedDOMWindows.remove(window);
    }
}
#endif // PLATFORM(VISION)

void GamepadManager::maybeStartMonitoringGamepads()
{
    if (m_isMonitoringGamepads)
        return;

    if (!m_navigators.isEmptyIgnoringNullReferences() || !m_domWindows.isEmptyIgnoringNullReferences()) {
        LOG(Gamepad, "(%u) GamepadManager has %i NavigatorGamepads and %i DOMWindows registered, is starting gamepad monitoring", (unsigned)getpid(), m_navigators.computeSize(), m_domWindows.computeSize());
        m_isMonitoringGamepads = true;
        GamepadProvider::singleton().startMonitoringGamepads(*this);
    }
}

void GamepadManager::maybeStopMonitoringGamepads()
{
    if (!m_isMonitoringGamepads)
        return;

    if (m_navigators.isEmptyIgnoringNullReferences() && m_domWindows.isEmptyIgnoringNullReferences()) {
        LOG(Gamepad, "(%u) GamepadManager has no NavigatorGamepads or DOMWindows registered, is stopping gamepad monitoring", (unsigned)getpid());
        m_isMonitoringGamepads = false;
        GamepadProvider::singleton().stopMonitoringGamepads(*this);
    }
}

} // namespace WebCore

#endif // ENABLE(GAMEPAD)
