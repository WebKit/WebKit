/*
 * Copyright (C) 2025 Igalia S.L.
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
#include "GamepadProviderWPE.h"

#if ENABLE(GAMEPAD) && ENABLE(WPE_PLATFORM)
#include "PlatformGamepadWPE.h"
#include <WebCore/GamepadProviderClient.h>
#include <WebCore/NotImplemented.h>
#include <wpe/wpe-platform.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/glib/GUniquePtr.h>

namespace WebKit {
using namespace WebCore;

static constexpr Seconds s_inputNotificationDelay { 1_ms };

GamepadProviderWPE& GamepadProviderWPE::singleton()
{
    static NeverDestroyed<GamepadProviderWPE> provider;
    return provider;
}

GamepadProviderWPE::GamepadProviderWPE()
    : m_inputNotificationTimer(RunLoop::currentSingleton(), "GamepadProviderWPE::InputNotificationTimer"_s, this, &GamepadProviderWPE::inputNotificationTimerFired)
{
}

GamepadProviderWPE::~GamepadProviderWPE() = default;

void GamepadProviderWPE::gamepadConnected(WPEGamepad* gamepad, IsInitialDevice isInitialDevice)
{
    unsigned index = 0;
    while (index < m_gamepadVector.size() && m_gamepadVector[index])
        ++index;

    auto device = makeUnique<PlatformGamepadWPE>(gamepad, index);
    if (m_gamepadVector.size() <= index)
        m_gamepadVector.grow(index + 1);

    m_gamepadVector[index] = device.get();
    m_gamepadMap.add(gamepad, WTFMove(device));

    if (m_isMonitoringInput)
        wpe_gamepad_start_input_monitor(gamepad);

    auto eventVisibility = isInitialDevice == IsInitialDevice::No ? EventMakesGamepadsVisible::Yes : EventMakesGamepadsVisible::No;
    for (auto& client : m_clients)
        client.platformGamepadConnected(*m_gamepadVector[index], eventVisibility);
}

void GamepadProviderWPE::gamepadDisconnected(WPEGamepad* gamepad)
{
    auto device = m_gamepadMap.take(gamepad);
    if (!device)
        return;

    auto index = m_gamepadVector.find(device.get());
    if (index != notFound)
        m_gamepadVector[index] = nullptr;

    for (auto& client : m_clients)
        client.platformGamepadDisconnected(*device);
}

void GamepadProviderWPE::startMonitoringGamepads(GamepadProviderClient& client)
{
    bool shouldCreateManager = m_clients.isEmptyIgnoringNullReferences();

    ASSERT(!m_clients.contains(client));
    m_clients.add(client);

    if (!shouldCreateManager)
        return;

    ASSERT(m_gamepadVector.isEmpty());

    m_manager = adoptGRef(wpe_display_create_gamepad_manager(wpe_display_get_primary()));
    if (!m_manager)
        return;

    g_signal_connect_swapped(m_manager.get(), "device-added", G_CALLBACK(+[](GamepadProviderWPE* provider, WPEGamepad* gamepad) {
        provider->gamepadConnected(gamepad, IsInitialDevice::No);
    }), this);
    g_signal_connect_swapped(m_manager.get(), "device-removed", G_CALLBACK(+[](GamepadProviderWPE* provider, WPEGamepad* gamepad) {
        provider->gamepadDisconnected(gamepad);
    }), this);

    m_isMonitoringInput = true;
    gsize deviceCount;
    GUniquePtr<WPEGamepad*> gamepads(wpe_gamepad_manager_list_devices(m_manager.get(), &deviceCount));
    IGNORE_CLANG_WARNINGS_BEGIN("unsafe-buffer-usage")
    for (size_t i = 0; i < deviceCount; ++i)
        gamepadConnected(gamepads.get()[i], IsInitialDevice::Yes);
    IGNORE_CLANG_WARNINGS_END
}

void GamepadProviderWPE::stopMonitoringGamepads(GamepadProviderClient& client)
{
    ASSERT(m_clients.contains(client));

    bool shouldDestroyManager = m_clients.remove(client) && m_clients.isEmptyIgnoringNullReferences();
    if (shouldDestroyManager) {
        if (m_manager) {
            g_signal_handlers_disconnect_by_data(m_manager.get(), this);
            m_manager = nullptr;
        }
        m_gamepadVector.clear();
        m_gamepadMap.clear();
        m_isMonitoringInput = false;
    }
}

void GamepadProviderWPE::playEffect(unsigned, const String&, GamepadHapticEffectType, const GamepadEffectParameters&, CompletionHandler<void(bool)>&& completionHandler)
{
    // Not supported by this provider.
    notImplemented();
    completionHandler(false);
}

void GamepadProviderWPE::stopEffects(unsigned, const String&, CompletionHandler<void()>&& completionHandler)
{
    // Not supported by this provider.
    notImplemented();
    completionHandler();
}

void GamepadProviderWPE::startMonitoringInput()
{
    if (m_isMonitoringInput)
        return;

    m_isMonitoringInput = true;
    for (auto* gamepad : copyToVector(m_gamepadMap.keys()))
        wpe_gamepad_start_input_monitor(gamepad);
}

void GamepadProviderWPE::stopMonitoringInput()
{
    if (!m_isMonitoringInput)
        return;

    m_isMonitoringInput = false;
    for (auto* gamepad : copyToVector(m_gamepadMap.keys()))
        wpe_gamepad_stop_input_monitor(gamepad);
}

void GamepadProviderWPE::inputNotificationTimerFired()
{
    dispatchPlatformGamepadInputActivity();
}

void GamepadProviderWPE::notifyInput(PlatformGamepadWPE& gamepad, ShouldMakeGamepadsVisible shouldMakeGamepadsVisible)
{
    if (shouldMakeGamepadsVisible == ShouldMakeGamepadsVisible::Yes)
        setShouldMakeGamepadsVisibile();

    if (!m_inputNotificationTimer.isActive())
        m_inputNotificationTimer.startOneShot(s_inputNotificationDelay);
}

} // namespace WebKit

#endif // ENABLE(GAMEPAD) && ENABLE(WPE_PLATFORM)
