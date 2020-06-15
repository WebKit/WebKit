/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
 * Copyright (C) 2020 Igalia S.L. All rights reserved.
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
#include "ManetteGamepadProvider.h"

#if ENABLE(GAMEPAD) && OS(LINUX)

#include "GUniquePtrManette.h"
#include "GamepadProviderClient.h"
#include "Logging.h"
#include "ManetteGamepad.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {

static const Seconds connectionDelayInterval { 500_ms };
static const Seconds inputNotificationDelay { 50_ms };

ManetteGamepadProvider& ManetteGamepadProvider::singleton()
{
    static NeverDestroyed<ManetteGamepadProvider> sharedProvider;
    return sharedProvider;
}

static void onDeviceConnected(ManetteMonitor*, ManetteDevice* device, ManetteGamepadProvider* provider)
{
    provider->deviceConnected(device);
}

static void onDeviceDisconnected(ManetteMonitor*, ManetteDevice* device, ManetteGamepadProvider* provider)
{
    provider->deviceDisconnected(device);
}

ManetteGamepadProvider::ManetteGamepadProvider()
    : m_monitor(adoptGRef(manette_monitor_new()))
    , m_connectionDelayTimer(RunLoop::current(), this, &ManetteGamepadProvider::connectionDelayTimerFired)
    , m_inputNotificationTimer(RunLoop::current(), this, &ManetteGamepadProvider::inputNotificationTimerFired)
{
    g_signal_connect(m_monitor.get(), "device-connected", G_CALLBACK(onDeviceConnected), this);
    g_signal_connect(m_monitor.get(), "device-disconnected", G_CALLBACK(onDeviceDisconnected), this);

    ManetteDevice* device;
    GUniquePtr<ManetteMonitorIter> iter(manette_monitor_iterate(m_monitor.get()));
    while (manette_monitor_iter_next(iter.get(), &device))
        deviceConnected(device);
}

ManetteGamepadProvider::~ManetteGamepadProvider()
{
    g_signal_handlers_disconnect_by_data(m_monitor.get(), this);
}

void ManetteGamepadProvider::startMonitoringGamepads(GamepadProviderClient& client)
{
    bool shouldOpenAndScheduleManager = m_clients.isEmpty();

    ASSERT(!m_clients.contains(&client));
    m_clients.add(&client);

    if (!shouldOpenAndScheduleManager)
        return;

    ASSERT(m_gamepadVector.isEmpty());
    ASSERT(m_gamepadMap.isEmpty());

    m_shouldDispatchCallbacks = false;
    m_connectionDelayTimer.startOneShot(connectionDelayInterval);
}

void ManetteGamepadProvider::stopMonitoringGamepads(GamepadProviderClient& client)
{
    ASSERT(m_clients.contains(&client));

    bool shouldCloseAndUnscheduleManager = m_clients.remove(&client) && m_clients.isEmpty();
    if (shouldCloseAndUnscheduleManager) {
        m_gamepadVector.clear();
        m_gamepadMap.clear();
        m_connectionDelayTimer.stop();
    }
}

void ManetteGamepadProvider::gamepadHadInput(ManetteGamepad&, ShouldMakeGamepadsVisible shouldMakeGamepadsVisible)
{
    if (!m_inputNotificationTimer.isActive())
        m_inputNotificationTimer.startOneShot(inputNotificationDelay);

    if (shouldMakeGamepadsVisible == ShouldMakeGamepadsVisible::Yes)
        setShouldMakeGamepadsVisibile();
}

void ManetteGamepadProvider::deviceConnected(ManetteDevice* device)
{
    ASSERT(!m_gamepadMap.get(device));

    LOG(Gamepad, "ManetteGamepadProvider device %p added", device);

    unsigned index = indexForNewlyConnectedDevice();
    auto gamepad = makeUnique<ManetteGamepad>(device, index);

    if (m_gamepadVector.size() <= index)
        m_gamepadVector.grow(index + 1);

    m_gamepadVector[index] = gamepad.get();
    m_gamepadMap.set(device, WTFMove(gamepad));

    if (!m_shouldDispatchCallbacks) {
        // This added device is the result of us starting to monitor gamepads.
        // We'll get notified of all connected devices during this current spin of the runloop
        // and we don't want to tell the client about any of them.
        // The m_connectionDelayTimer fires in a subsequent spin of the runloop after which
        // any connection events are actual new devices.
        m_connectionDelayTimer.startOneShot(0_s);

        LOG(Gamepad, "Device %p was added while suppressing callbacks, so this should be an 'already connected' event", device);

        return;
    }

    for (auto& client : m_clients)
        client->platformGamepadConnected(*m_gamepadVector[index]);
}

void ManetteGamepadProvider::deviceDisconnected(ManetteDevice* device)
{
    LOG(Gamepad, "ManetteGamepadProvider device %p removed", device);

    std::unique_ptr<ManetteGamepad> removedGamepad = removeGamepadForDevice(device);
    ASSERT(removedGamepad);

    // Any time we get a device removed callback we know it's a real event and not an 'already connected' event.
    // We should always stop suppressing callbacks when we receive such an event.
    m_shouldDispatchCallbacks = true;

    for (auto& client : m_clients)
        client->platformGamepadDisconnected(*removedGamepad);
}

unsigned ManetteGamepadProvider::indexForNewlyConnectedDevice()
{
    unsigned index = 0;
    while (index < m_gamepadVector.size() && m_gamepadVector[index])
        ++index;

    return index;
}

void ManetteGamepadProvider::connectionDelayTimerFired()
{
    m_shouldDispatchCallbacks = true;

    for (auto* client : m_clients)
        client->setInitialConnectedGamepads(m_gamepadVector);
}

void ManetteGamepadProvider::inputNotificationTimerFired()
{
    if (!m_shouldDispatchCallbacks)
        return;

    dispatchPlatformGamepadInputActivity();
}

std::unique_ptr<ManetteGamepad> ManetteGamepadProvider::removeGamepadForDevice(ManetteDevice* device)
{
    std::unique_ptr<ManetteGamepad> result = m_gamepadMap.take(device);
    ASSERT(result);

    auto index = m_gamepadVector.find(result.get());
    if (index != notFound)
        m_gamepadVector[index] = nullptr;

    return result;
}

} // namespace WebCore

#endif // ENABLE(GAMEPAD) && OS(LINUX)
