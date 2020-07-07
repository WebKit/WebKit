/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#include "UIGamepadProvider.h"

#if ENABLE(GAMEPAD)

#include "GamepadData.h"
#include "UIGamepad.h"
#include "WebPageProxy.h"
#include "WebProcessPool.h"
#include <WebCore/MockGamepadProvider.h>
#include <WebCore/PlatformGamepad.h>
#include <wtf/NeverDestroyed.h>

namespace WebKit {
using namespace WebCore;

static const Seconds maximumGamepadUpdateInterval { 1_s / 120. };

UIGamepadProvider& UIGamepadProvider::singleton()
{
    static NeverDestroyed<UIGamepadProvider> sharedProvider;
    return sharedProvider;
}

UIGamepadProvider::UIGamepadProvider()
    : m_gamepadSyncTimer(RunLoop::main(), this, &UIGamepadProvider::gamepadSyncTimerFired)
{
    platformSetDefaultGamepadProvider();
}

UIGamepadProvider::~UIGamepadProvider()
{
    if (!m_processPoolsUsingGamepads.isEmpty())
        GamepadProvider::singleton().stopMonitoringGamepads(*this);
}

void UIGamepadProvider::gamepadSyncTimerFired()
{
    auto webPageProxy = platformWebPageProxyForGamepadInput();
    if (!webPageProxy || !m_processPoolsUsingGamepads.contains(&webPageProxy->process().processPool()))
        return;

    webPageProxy->gamepadActivity(snapshotGamepads(), m_shouldMakeGamepadsVisibleOnSync ? EventMakesGamepadsVisible::Yes : EventMakesGamepadsVisible::No);
    m_shouldMakeGamepadsVisibleOnSync = false;
}

void UIGamepadProvider::scheduleGamepadStateSync()
{
    if (!m_isMonitoringGamepads || m_gamepadSyncTimer.isActive())
        return;

    if (m_gamepads.isEmpty() || m_processPoolsUsingGamepads.isEmpty()) {
        m_gamepadSyncTimer.stop();
        return;
    }

    m_gamepadSyncTimer.startOneShot(maximumGamepadUpdateInterval);
}

void UIGamepadProvider::platformGamepadConnected(PlatformGamepad& gamepad, EventMakesGamepadsVisible eventVisibility)
{
    LOG(Gamepad, "UIGamepadProvider::platformGamepadConnected - Index %i attached (visibility: %i)\n", gamepad.index(), (int)eventVisibility);

    if (m_gamepads.size() <= gamepad.index())
        m_gamepads.grow(gamepad.index() + 1);

    ASSERT(!m_gamepads[gamepad.index()]);
    m_gamepads[gamepad.index()] = makeUnique<UIGamepad>(gamepad);

    scheduleGamepadStateSync();

    for (auto& pool : m_processPoolsUsingGamepads)
        pool->gamepadConnected(*m_gamepads[gamepad.index()], eventVisibility);
}

void UIGamepadProvider::platformGamepadDisconnected(PlatformGamepad& gamepad)
{
    ASSERT(gamepad.index() < m_gamepads.size());
    ASSERT(m_gamepads[gamepad.index()]);

    std::unique_ptr<UIGamepad> disconnectedGamepad = WTFMove(m_gamepads[gamepad.index()]);

    scheduleGamepadStateSync();

    for (auto& pool : m_processPoolsUsingGamepads)
        pool->gamepadDisconnected(*disconnectedGamepad);
}

void UIGamepadProvider::platformGamepadInputActivity(EventMakesGamepadsVisible eventVisibility)
{
    auto platformGamepads = GamepadProvider::singleton().platformGamepads();
    ASSERT(platformGamepads.size() == m_gamepads.size());

    for (size_t i = 0; i < platformGamepads.size(); ++i) {
        if (!platformGamepads[i]) {
            ASSERT(!m_gamepads[i]);
            continue;
        }

        ASSERT(m_gamepads[i]);
        m_gamepads[i]->updateFromPlatformGamepad(*platformGamepads[i]);
    }

    if (eventVisibility == EventMakesGamepadsVisible::Yes)
        m_shouldMakeGamepadsVisibleOnSync = true;

    scheduleGamepadStateSync();
}

void UIGamepadProvider::processPoolStartedUsingGamepads(WebProcessPool& pool)
{
    ASSERT(!m_processPoolsUsingGamepads.contains(&pool));
    m_processPoolsUsingGamepads.add(&pool);

    if (!m_isMonitoringGamepads && platformWebPageProxyForGamepadInput())
        startMonitoringGamepads();
}

void UIGamepadProvider::processPoolStoppedUsingGamepads(WebProcessPool& pool)
{
    ASSERT(m_processPoolsUsingGamepads.contains(&pool));
    m_processPoolsUsingGamepads.remove(&pool);

    if (m_isMonitoringGamepads && !platformWebPageProxyForGamepadInput())
        platformStopMonitoringInput();
}

void UIGamepadProvider::viewBecameActive(WebPageProxy& page)
{
    if (!m_processPoolsUsingGamepads.contains(&page.process().processPool()))
        return;

    if (!m_isMonitoringGamepads)
        startMonitoringGamepads();

    if (platformWebPageProxyForGamepadInput())
        platformStartMonitoringInput();
}

void UIGamepadProvider::viewBecameInactive(WebPageProxy& page)
{
    auto pageForGamepadInput = platformWebPageProxyForGamepadInput();
    if (pageForGamepadInput == &page)
        platformStopMonitoringInput();
}

void UIGamepadProvider::startMonitoringGamepads()
{
    if (m_isMonitoringGamepads)
        return;

    m_isMonitoringGamepads = true;
    ASSERT(!m_processPoolsUsingGamepads.isEmpty());
    GamepadProvider::singleton().startMonitoringGamepads(*this);
}

void UIGamepadProvider::stopMonitoringGamepads()
{
    if (!m_isMonitoringGamepads)
        return;

    m_isMonitoringGamepads = false;

    ASSERT(m_processPoolsUsingGamepads.isEmpty());
    GamepadProvider::singleton().stopMonitoringGamepads(*this);

    m_gamepads.clear();
}

Vector<GamepadData> UIGamepadProvider::snapshotGamepads()
{
    Vector<GamepadData> gamepadDatas;
    gamepadDatas.reserveInitialCapacity(m_gamepads.size());

    for (auto& gamepad : m_gamepads) {
        if (gamepad)
            gamepadDatas.uncheckedAppend(gamepad->condensedGamepadData());
        else
            gamepadDatas.uncheckedAppend({ });
    }

    return gamepadDatas;
}

#if !PLATFORM(COCOA) && !(USE(MANETTE) && OS(LINUX))

void UIGamepadProvider::platformSetDefaultGamepadProvider()
{
    // FIXME: Implement for other platforms
}

WebPageProxy* UIGamepadProvider::platformWebPageProxyForGamepadInput()
{
    // FIXME: Implement for other platforms
    return nullptr;
}

void UIGamepadProvider::platformStopMonitoringInput()
{
}

void UIGamepadProvider::platformStartMonitoringInput()
{
}

#endif // !PLATFORM(COCOA) && !(USE(MANETTE) && OS(LINUX))

}

#endif // ENABLE(GAMEPAD)
