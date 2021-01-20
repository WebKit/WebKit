/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#include "MultiGamepadProvider.h"

#if ENABLE(GAMEPAD) && HAVE(MULTIGAMEPADPROVIDER_SUPPORT)

#import "GameControllerGamepadProvider.h"
#import "HIDGamepadProvider.h"
#import "Logging.h"
#import "PlatformGamepad.h"
#import <wtf/NeverDestroyed.h>

namespace WebCore {

static size_t numberOfGamepadProviders = 2;

MultiGamepadProvider& MultiGamepadProvider::singleton()
{
    static NeverDestroyed<MultiGamepadProvider> sharedProvider;
    return sharedProvider;
}

void MultiGamepadProvider::startMonitoringGamepads(GamepadProviderClient& client)
{
    bool monitorOtherProviders = m_clients.isEmpty();

    ASSERT(!m_clients.contains(&client));
    m_clients.add(&client);

    if (!m_usesOnlyHIDProvider) {
        HIDGamepadProvider::singleton().ignoreGameControllerFrameworkDevices();
        GameControllerGamepadProvider::singleton().prewarmGameControllerDevicesIfNecessary();
    }

    if (monitorOtherProviders) {
        HIDGamepadProvider::singleton().startMonitoringGamepads(*this);
        if (!m_usesOnlyHIDProvider)
            GameControllerGamepadProvider::singleton().startMonitoringGamepads(*this);
    }
}

void MultiGamepadProvider::stopMonitoringGamepads(GamepadProviderClient& client)
{
    ASSERT(m_clients.contains(&client));

    bool shouldStopMonitoringOtherProviders = m_clients.remove(&client) && m_clients.isEmpty();

    if (shouldStopMonitoringOtherProviders) {
        HIDGamepadProvider::singleton().stopMonitoringGamepads(*this);
        if (!m_usesOnlyHIDProvider)
            GameControllerGamepadProvider::singleton().stopMonitoringGamepads(*this);
    }
}

unsigned MultiGamepadProvider::indexForNewlyConnectedDevice()
{
    unsigned index = 0;
    while (index < m_gamepadVector.size() && m_gamepadVector[index])
        ++index;

    ASSERT(index <= m_gamepadVector.size());

    if (index == m_gamepadVector.size())
        m_gamepadVector.resize(index + 1);

    return index;
}

void MultiGamepadProvider::platformGamepadConnected(PlatformGamepad& gamepad, EventMakesGamepadsVisible eventVisibility)
{
    auto index = indexForNewlyConnectedDevice();

    LOG(Gamepad, "MultiGamepadProvider adding new platform gamepad to index %i from a %s source", index, gamepad.source());

    ASSERT(m_gamepadVector.size() > index);

    auto addResult = m_gamepadMap.set(&gamepad, WTF::makeUnique<PlatformGamepadWrapper>(index, &gamepad));
    ASSERT(addResult.isNewEntry);
    m_gamepadVector[index] = addResult.iterator->value.get();

    for (auto& client : m_clients)
        client->platformGamepadConnected(*m_gamepadVector[index], eventVisibility);
}

void MultiGamepadProvider::platformGamepadDisconnected(PlatformGamepad& gamepad)
{
    LOG(Gamepad, "MultiGamepadProvider disconnecting gamepad from a %s source", gamepad.source());

    auto gamepadWrapper = m_gamepadMap.take(&gamepad);

    ASSERT(gamepadWrapper);
    ASSERT(gamepadWrapper->index() < m_gamepadVector.size());
    ASSERT(m_gamepadVector[gamepadWrapper->index()] == gamepadWrapper.get());

    m_gamepadVector[gamepadWrapper->index()] = nullptr;

    for (auto& client : m_clients)
        client->platformGamepadDisconnected(*gamepadWrapper);
}

void MultiGamepadProvider::platformGamepadInputActivity(EventMakesGamepadsVisible eventVisibility)
{
    if (eventVisibility == EventMakesGamepadsVisible::Yes)
        GameControllerGamepadProvider::singleton().makeInvisibleGamepadsVisible();

    for (auto& client : m_clients)
        client->platformGamepadInputActivity(eventVisibility);
}

} // namespace WebCore

#endif // ENABLE(GAMEPAD) && HAVE(MULTIGAMEPADPROVIDER_SUPPORT)
