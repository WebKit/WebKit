/*
 * Copyright (C) 2014-2025 Apple Inc. All rights reserved.
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
#include "Navigator.h"
#include "NavigatorGamepad.h"
#include "PlatformGamepad.h"
#include "UserGestureIndicator.h"
#include <wtf/NeverDestroyed.h>

#if PLATFORM(VISION)
#include "Page.h"
#endif

namespace WebCore {

GamepadManager& GamepadManager::singleton()
{
    static NeverDestroyed<GamepadManager> sharedManager;
    return sharedManager;
}

GamepadManager::GamepadManager()
{
}

void GamepadManager::platformGamepadConnected(PlatformGamepad&, EventMakesGamepadsVisible eventVisibility)
{
    m_hasPendingConnections = true;
    if (eventVisibility == EventMakesGamepadsVisible::Yes)
        makeGamepadsVisible();
}

void GamepadManager::platformGamepadDisconnected(PlatformGamepad& platformGamepad)
{
    for (auto& window : copyToVectorOf<WeakPtr<LocalDOMWindow, WeakPtrImplWithEventTargetData>>(m_domWindows)) {
        // Event dispatch might have made this window go away.
        if (!window)
            continue;
        Ref navigator = window->navigator();
        auto& navigatorGamepad = NavigatorGamepad::from(navigator);
        RefPtr gamepad = navigatorGamepad.gamepadIfExists(platformGamepad);
        if (!gamepad)
            continue;
        gamepad->setConnected(false);
        window->dispatchEvent(GamepadEvent::create(eventNames().gamepaddisconnectedEvent, *gamepad), window->protectedDocument().get());
        navigatorGamepad.removeGamepadIfExists(platformGamepad);
    }
    for (Ref navigator : m_navigators) {
        auto& navigatorGamepad = NavigatorGamepad::from(navigator);
        RefPtr gamepad = navigatorGamepad.gamepadIfExists(platformGamepad);
        if (!gamepad)
            continue;
        gamepad->setConnected(false);
        navigatorGamepad.removeGamepadIfExists(platformGamepad);
    }
}

void GamepadManager::platformGamepadInputActivity(EventMakesGamepadsVisible eventVisibility)
{
    if (eventVisibility == EventMakesGamepadsVisible::Yes)
        makeGamepadsVisible();
}

void GamepadManager::makeGamepadsVisible()
{
    if (!m_hasPendingConnections)
        return;
    m_hasPendingConnections = false;

    // Copy the Vector to avoid possible event dispatch changing the list.
    auto platformGamepads = copyToVectorOf<WeakPtr<PlatformGamepad>>(GamepadProvider::singleton().platformGamepads());
    if (platformGamepads.isEmpty())
        return;
    for (auto& window : copyToVectorOf<WeakPtr<LocalDOMWindow, WeakPtrImplWithEventTargetData>>(m_domWindows)) {
        // Event dispatch might have made this window go away.
        if (!window)
            continue;
        Ref navigator = window->navigator();
#if PLATFORM(VISION)
        if (RefPtr page = navigator->page(); page && !page->gamepadAccessGranted())
            continue;
#endif
        NavigatorGamepad& navigatorGamepad = NavigatorGamepad::from(navigator);
        RefPtr document = navigator->document();
        for (auto& platformGamepad : platformGamepads) {
            if (!platformGamepad)
                continue;
            // As per spec, if the gamepad is [[exposed]], do not post events.
            if (navigatorGamepad.gamepadIfExists(*platformGamepad.get()))
                continue;
            // As per spec, set [[exposed]] to true.
            Ref gamepad = navigatorGamepad.ensureGamepad(*platformGamepad.get());
            LOG(Gamepad, "(%u) GamepadManager::makeGamepadVisible - Dispatching gamepadconnected event for gamepad '%s'", (unsigned)getpid(), platformGamepad->id().utf8().data());
            UserGestureIndicator gestureIndicator(IsProcessingUserGesture::Yes, document.get());
            window->dispatchEvent(GamepadEvent::create(eventNames().gamepadconnectedEvent, gamepad.get()), window->protectedDocument().get());
        }
    }
    for (Ref navigator : m_navigators) {
#if PLATFORM(VISION)
        if (RefPtr page = navigator->page(); page && !page->gamepadAccessGranted())
            continue;
#endif
        auto& navigatorGamepad = NavigatorGamepad::from(navigator);
        for (auto& platformGamepad : platformGamepads) {
            if (!platformGamepad)
                continue;
            // As per spec, set [[exposed]] to true.
            navigatorGamepad.ensureGamepad(*platformGamepad.get());
        }
    }
}

void GamepadManager::registerNavigator(Navigator& navigator)
{
    LOG(Gamepad, "(%u) GamepadManager registering Navigator %p", (unsigned)getpid(), &navigator);
    ASSERT(!m_navigators.contains(navigator));
    m_navigators.add(navigator);
    maybeStartMonitoringGamepads();
}

void GamepadManager::unregisterNavigator(Navigator& navigator)
{
    LOG(Gamepad, "(%u) GamepadManager unregistering Navigator %p", (unsigned)getpid(), &navigator);
    ASSERT(m_navigators.contains(navigator));
    m_navigators.remove(navigator);
    maybeStopMonitoringGamepads();
}

void GamepadManager::registerDOMWindow(LocalDOMWindow& window)
{
    LOG(Gamepad, "(%u) GamepadManager registering LocalDOMWindow %p", (unsigned)getpid(), &window);
    // Anytime we register a LocalDOMWindow, we should make sure its NavigatorGamepad is constructed.
    NavigatorGamepad::from(window.protectedNavigator());
    ASSERT(!m_domWindows.contains(window));
    m_domWindows.add(window);
    maybeStartMonitoringGamepads();
}

void GamepadManager::unregisterDOMWindow(LocalDOMWindow& window)
{
    LOG(Gamepad, "(%u) GamepadManager unregistering LocalDOMWindow %p", (unsigned)getpid(), &window);
    ASSERT(m_domWindows.contains(window));
    m_domWindows.remove(window);
    maybeStopMonitoringGamepads();
}

#if PLATFORM(VISION)
void GamepadManager::didUpdateGamepadAccess()
{
    m_hasPendingConnections = true;
    makeGamepadsVisible();
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
