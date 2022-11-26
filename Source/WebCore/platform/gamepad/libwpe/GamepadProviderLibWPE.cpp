/*
 * Copyright (C) 2020 RDK Management  All rights reserved.
 * Copyright (C) 2022 Igalia S.L.
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
#include "GamepadProviderLibWPE.h"

#if ENABLE(GAMEPAD) && USE(LIBWPE)

#include "GamepadLibWPE.h"
#include "GamepadProviderClient.h"
#include "Logging.h"
#include <inttypes.h>
#include <wpe/wpe.h>
#include <wtf/NeverDestroyed.h>

namespace WebCore {

static const Seconds connectionDelayInterval { 500_ms };
static const Seconds inputNotificationDelay { 50_ms };

GamepadProviderLibWPE& GamepadProviderLibWPE::singleton()
{
    static NeverDestroyed<GamepadProviderLibWPE> sharedProvider;
    return sharedProvider;
}

GamepadProviderLibWPE::GamepadProviderLibWPE()
    : m_provider(wpe_gamepad_provider_create(), wpe_gamepad_provider_destroy)
    , m_initialGamepadsConnectedTimer(RunLoop::current(), this, &GamepadProviderLibWPE::initialGamepadsConnectedTimerFired)
    , m_inputNotificationTimer(RunLoop::current(), this, &GamepadProviderLibWPE::inputNotificationTimerFired)
{
    static const struct wpe_gamepad_provider_client_interface s_client = {
        // connected
        [](void* data, uintptr_t gamepadId) {
            auto& provider = *static_cast<GamepadProviderLibWPE*>(data);
            provider.gamepadConnected(gamepadId);
        },
        // disconnected
        [](void* data, uintptr_t gamepadId) {
            auto& provider = *static_cast<GamepadProviderLibWPE*>(data);
            provider.gamepadDisconnected(gamepadId);
        },
        nullptr, nullptr, nullptr,
    };

    wpe_gamepad_provider_set_client(m_provider.get(), &s_client, this);
}

GamepadProviderLibWPE::~GamepadProviderLibWPE()
{
    wpe_gamepad_provider_set_client(m_provider.get(), nullptr, nullptr);
}

void GamepadProviderLibWPE::startMonitoringGamepads(GamepadProviderClient& client)
{
    if (!m_provider)
        return;

    bool shouldOpenAndScheduleManager = m_clients.isEmpty();

    ASSERT(!m_clients.contains(&client));
    m_clients.add(&client);

    if (!shouldOpenAndScheduleManager)
        return;

    ASSERT(m_gamepadVector.isEmpty());
    ASSERT(m_gamepadMap.isEmpty());

    m_initialGamepadsConnected = false;

    // Any connections we are notified of within the connectionDelayInterval of listening likely represent
    // devices that were already connected, so we suppress notifying clients of these.
    m_initialGamepadsConnectedTimer.startOneShot(connectionDelayInterval);

    wpe_gamepad_provider_start(m_provider.get());
}

void GamepadProviderLibWPE::stopMonitoringGamepads(GamepadProviderClient& client)
{
    if (!m_provider)
        return;

    ASSERT(m_clients.contains(&client));

    bool shouldCloseAndUnscheduleManager = m_clients.remove(&client) && m_clients.isEmpty();
    if (!shouldCloseAndUnscheduleManager)
        return;

    wpe_gamepad_provider_stop(m_provider.get());

    m_gamepadVector.clear();
    m_gamepadMap.clear();
    m_initialGamepadsConnectedTimer.stop();
    m_lastActiveGamepad = nullptr;
}

void GamepadProviderLibWPE::gamepadConnected(uintptr_t id)
{
    ASSERT(!m_gamepadMap.get(id));
    ASSERT(m_provider);

    LOG(Gamepad, "GamepadProviderLibWPE device %" PRIuPTR " added", id);

    unsigned index = indexForNewlyConnectedDevice();
    auto gamepad = makeUnique<GamepadLibWPE>(m_provider.get(), id, index);

    if (m_gamepadVector.size() <= index)
        m_gamepadVector.grow(index + 1);

    m_gamepadVector[index] = gamepad.get();
    m_gamepadMap.set(id, WTFMove(gamepad));

    if (!m_initialGamepadsConnected) {
        // This added device is the result of us starting to monitor gamepads.
        // We'll get notified of all connected devices during this current spin of the runloop
        // and the client should be told they were already connected.
        // The m_connectionDelayTimer fires in a subsequent spin of the runloop after which
        // any connection events are actual new devices and can trigger gamepad visibility.
        if (!m_initialGamepadsConnectedTimer.isActive())
            m_initialGamepadsConnectedTimer.startOneShot(0_s);
    }

    auto eventVisibility = m_initialGamepadsConnected ? EventMakesGamepadsVisible::Yes : EventMakesGamepadsVisible::No;
    for (auto& client : m_clients)
        client->platformGamepadConnected(*m_gamepadVector[index], eventVisibility);
}

void GamepadProviderLibWPE::gamepadDisconnected(uintptr_t id)
{
    LOG(Gamepad, "GamepadProviderLibWPE device %" PRIuPTR " removed", id);

    auto removedGamepad = removeGamepadForId(id);
    ASSERT(removedGamepad);

    if (removedGamepad->wpeGamepad() == m_lastActiveGamepad)
        m_lastActiveGamepad = nullptr;

    for (auto& client : m_clients)
        client->platformGamepadDisconnected(*removedGamepad);
}

unsigned GamepadProviderLibWPE::indexForNewlyConnectedDevice()
{
    unsigned index = 0;
    while (index < m_gamepadVector.size() && m_gamepadVector[index])
        ++index;

    return index;
}

std::unique_ptr<GamepadLibWPE> GamepadProviderLibWPE::removeGamepadForId(uintptr_t id)
{
    auto removedGamepad = m_gamepadMap.take(id);
    ASSERT(removedGamepad);

    auto index = m_gamepadVector.find(removedGamepad.get());
    if (index != notFound)
        m_gamepadVector[index] = nullptr;

    if (removedGamepad->wpeGamepad() == m_lastActiveGamepad)
        m_lastActiveGamepad = nullptr;

    return removedGamepad;
}

void GamepadProviderLibWPE::initialGamepadsConnectedTimerFired()
{
    m_initialGamepadsConnected = true;
}

void GamepadProviderLibWPE::inputNotificationTimerFired()
{
    if (!m_initialGamepadsConnected) {
        if (!m_inputNotificationTimer.isActive())
            m_inputNotificationTimer.startOneShot(0_s);
        return;
    }

    dispatchPlatformGamepadInputActivity();
}

void GamepadProviderLibWPE::scheduleInputNotification(GamepadLibWPE& gamepad, ShouldMakeGamepadsVisible shouldMakeGamepadsVisible)
{
    m_lastActiveGamepad = const_cast<struct wpe_gamepad*>(gamepad.wpeGamepad());

    if (!m_inputNotificationTimer.isActive())
        m_inputNotificationTimer.startOneShot(inputNotificationDelay);

    if (shouldMakeGamepadsVisible == ShouldMakeGamepadsVisible::Yes)
        setShouldMakeGamepadsVisibile();
}

struct wpe_view_backend* GamepadProviderLibWPE::inputView()
{
    if (!m_provider || !m_lastActiveGamepad)
        return nullptr;
    return wpe_gamepad_provider_get_view_backend(m_provider.get(), m_lastActiveGamepad);
}

} // namespace WebCore

#endif // ENABLE(GAMEPAD) && USE(LIBWPE)
