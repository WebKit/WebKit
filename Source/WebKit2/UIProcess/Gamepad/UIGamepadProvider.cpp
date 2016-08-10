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
#include "WebProcessPool.h"
#include <WebCore/HIDGamepadProvider.h>
#include <wtf/NeverDestroyed.h>

using namespace WebCore;

namespace WebKit {

static const double gamepadUpdateInterval = 1 / 60.0;

UIGamepadProvider& UIGamepadProvider::singleton()
{
    static NeverDestroyed<UIGamepadProvider> sharedProvider;
    return sharedProvider;
}

UIGamepadProvider::UIGamepadProvider()
    : m_timer(*this, &UIGamepadProvider::updateTimerFired)
{
}

UIGamepadProvider::~UIGamepadProvider()
{
    if (!m_processPoolsUsingGamepads.isEmpty())
        platformStopMonitoringGamepads();
}

void UIGamepadProvider::updateTimerFired()
{
    if (!m_hadActivitySinceLastSynch)
        return;

    Vector<GamepadData> gamepadDatas;
    gamepadDatas.reserveInitialCapacity(m_gamepads.size());

    for (auto& gamepad : m_gamepads) {
        if (gamepad)
            gamepadDatas.uncheckedAppend(gamepad->gamepadData());
        else
            gamepadDatas.uncheckedAppend({ });
    }

    auto webPageProxy = platformWebPageProxyForGamepadInput();
    if (webPageProxy && m_processPoolsUsingGamepads.contains(&webPageProxy->process().processPool()))
        webPageProxy->gamepadActivity(gamepadDatas);

    m_hadActivitySinceLastSynch = false;
}

void UIGamepadProvider::startOrStopSynchingGamepadState()
{
    // FIXME (https://bugs.webkit.org/show_bug.cgi?id=160699)
    // Only start synching updates if the currently focused WKWebView is also listening for gamepads.

    // FIXME (https://bugs.webkit.org/show_bug.cgi?id=160673)
    // Instead of refreshing gamepad data on a 60hz timer, actually sync with the display.
    if (m_gamepads.isEmpty() || m_processPoolsUsingGamepads.isEmpty())
        m_timer.stop();
    else
        m_timer.startRepeating(gamepadUpdateInterval);
}

void UIGamepadProvider::platformGamepadConnected(PlatformGamepad& gamepad)
{
    if (m_gamepads.size() <= gamepad.index())
        m_gamepads.resize(gamepad.index() + 1);

    ASSERT(!m_gamepads[gamepad.index()]);
    m_gamepads[gamepad.index()] = std::make_unique<UIGamepad>(gamepad);

    m_hadActivitySinceLastSynch = true;
    startOrStopSynchingGamepadState();

    for (auto& pool : m_processPoolsUsingGamepads)
        pool->gamepadConnected(*m_gamepads[gamepad.index()]);
}

void UIGamepadProvider::platformGamepadDisconnected(PlatformGamepad& gamepad)
{
    ASSERT(gamepad.index() < m_gamepads.size());
    ASSERT(m_gamepads[gamepad.index()]);

    std::unique_ptr<UIGamepad> disconnectedGamepad = WTFMove(m_gamepads[gamepad.index()]);

    startOrStopSynchingGamepadState();

    for (auto& pool : m_processPoolsUsingGamepads)
        pool->gamepadDisconnected(*disconnectedGamepad);
}

void UIGamepadProvider::platformGamepadInputActivity()
{
    auto platformGamepads = this->platformGamepads();
    ASSERT(platformGamepads.size() == m_gamepads.size());

    for (size_t i = 0; i < platformGamepads.size(); ++i) {
        if (!platformGamepads[i]) {
            ASSERT(!m_gamepads[i]);
            continue;
        }

        ASSERT(m_gamepads[i]);
        m_gamepads[i]->updateFromPlatformGamepad(*platformGamepads[i]);
    }

    m_hadActivitySinceLastSynch = true;
}

void UIGamepadProvider::processPoolStartedUsingGamepads(WebProcessPool& pool)
{
    bool wereAnyProcessPoolsUsingGamepads = !m_processPoolsUsingGamepads.isEmpty();

    ASSERT(!m_processPoolsUsingGamepads.contains(&pool));
    m_processPoolsUsingGamepads.add(&pool);

    if (!wereAnyProcessPoolsUsingGamepads)
        platformStartMonitoringGamepads();

    startOrStopSynchingGamepadState();
}

void UIGamepadProvider::processPoolStoppedUsingGamepads(WebProcessPool& pool)
{
    bool wereAnyProcessPoolsUsingGamepads = !m_processPoolsUsingGamepads.isEmpty();

    ASSERT(m_processPoolsUsingGamepads.contains(&pool));
    m_processPoolsUsingGamepads.remove(&pool);

    if (wereAnyProcessPoolsUsingGamepads && m_processPoolsUsingGamepads.isEmpty())
        platformStopMonitoringGamepads();

    startOrStopSynchingGamepadState();
}

#if !PLATFORM(MAC)

void UIGamepadProvider::platformStartMonitoringGamepads()
{
    // FIXME: Implement for other platforms
}

void UIGamepadProvider::platformStopMonitoringGamepads()
{
    // FIXME: Implement for other platforms
}

const Vector<PlatformGamepad*>& UIGamepadProvider::platformGamepads()
{
    static NeverDestroyed<Vector<PlatformGamepad*>> emptyGamepads;
    return emptyGamepads;

    // FIXME: Implement for other platforms
}

WebProcessProxy* UIGamepadProvider::platformWebProcessProxyForGamepadInput()
{
    // FIXME: Implement for other platforms
}

#endif // !PLATFORM(MAC)

}

#endif // ENABLE(GAMEPAD)
