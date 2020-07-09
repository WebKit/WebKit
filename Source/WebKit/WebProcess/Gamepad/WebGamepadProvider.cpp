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
#include "WebGamepadProvider.h"

#if ENABLE(GAMEPAD)

#include "GamepadData.h"
#include "WebGamepad.h"
#include "WebProcess.h"
#include "WebProcessPoolMessages.h"
#include <WebCore/GamepadProviderClient.h>
#include <wtf/NeverDestroyed.h>

namespace WebKit {
using namespace WebCore;

WebGamepadProvider& WebGamepadProvider::singleton()
{
    static NeverDestroyed<WebGamepadProvider> provider;
    return provider;
}

WebGamepadProvider::WebGamepadProvider()
{
}

WebGamepadProvider::~WebGamepadProvider()
{
}

void WebGamepadProvider::setInitialGamepads(const Vector<GamepadData>& gamepadDatas)
{
    ASSERT(m_gamepads.isEmpty());

    m_gamepads.resize(gamepadDatas.size());
    m_rawGamepads.resize(gamepadDatas.size());
    for (size_t i = 0; i < gamepadDatas.size(); ++i) {
        if (gamepadDatas[i].isNull())
            continue;

        m_gamepads[i] = makeUnique<WebGamepad>(gamepadDatas[i]);
        m_rawGamepads[i] = m_gamepads[i].get();
    }
}

void WebGamepadProvider::gamepadConnected(const GamepadData& gamepadData, EventMakesGamepadsVisible eventVisibility)
{
    LOG(Gamepad, "WebGamepadProvider::gamepadConnected - Gamepad index %u attached (visibility: %i)\n", gamepadData.index(), (int)eventVisibility);

    if (m_gamepads.size() <= gamepadData.index()) {
        m_gamepads.resize(gamepadData.index() + 1);
        m_rawGamepads.resize(gamepadData.index() + 1);
    }

    ASSERT(!m_gamepads[gamepadData.index()]);

    m_gamepads[gamepadData.index()] = makeUnique<WebGamepad>(gamepadData);
    m_rawGamepads[gamepadData.index()] = m_gamepads[gamepadData.index()].get();

    for (auto* client : m_clients)
        client->platformGamepadConnected(*m_gamepads[gamepadData.index()], eventVisibility);
}

void WebGamepadProvider::gamepadDisconnected(unsigned index)
{
    ASSERT(m_gamepads.size() > index);

    std::unique_ptr<WebGamepad> disconnectedGamepad = WTFMove(m_gamepads[index]);
    m_rawGamepads[index] = nullptr;

    LOG(Gamepad, "WebGamepadProvider::gamepadDisconnected - Gamepad index %u detached (m_gamepads size %zu, m_rawGamepads size %zu\n", index, m_gamepads.size(), m_rawGamepads.size());

    for (auto* client : m_clients)
        client->platformGamepadDisconnected(*disconnectedGamepad);
}

void WebGamepadProvider::gamepadActivity(const Vector<GamepadData>& gamepadDatas, EventMakesGamepadsVisible eventVisibility)
{
    LOG(Gamepad, "WebGamepadProvider::gamepadActivity - %zu gamepad datas with %zu local web gamepads\n", gamepadDatas.size(), m_gamepads.size());

    ASSERT(m_gamepads.size() == gamepadDatas.size());

    for (size_t i = 0; i < m_gamepads.size(); ++i) {
        if (m_gamepads[i])
            m_gamepads[i]->updateValues(gamepadDatas[i]);
    }

    for (auto* client : m_clients)
        client->platformGamepadInputActivity(eventVisibility);
}

void WebGamepadProvider::startMonitoringGamepads(GamepadProviderClient& client)
{
    bool processHadGamepadClients = !m_clients.isEmpty();

    ASSERT(!m_clients.contains(&client));
    m_clients.add(&client);

    if (!processHadGamepadClients)
        WebProcess::singleton().send(Messages::WebProcessPool::StartedUsingGamepads(), 0);
}

void WebGamepadProvider::stopMonitoringGamepads(GamepadProviderClient& client)
{
    bool processHadGamepadClients = !m_clients.isEmpty();

    ASSERT(m_clients.contains(&client));
    m_clients.remove(&client);

    if (processHadGamepadClients && m_clients.isEmpty())
        WebProcess::singleton().send(Messages::WebProcessPool::StoppedUsingGamepads(), 0);
}

const Vector<PlatformGamepad*>& WebGamepadProvider::platformGamepads()
{
    return m_rawGamepads;
}

}

#endif // ENABLE(GAMEPAD)
