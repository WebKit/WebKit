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
#include "MockGamepadProvider.h"

#if ENABLE(GAMEPAD)

#include "GamepadProviderClient.h"
#include "MockGamepad.h"
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>

namespace WebCore {

MockGamepadProvider& MockGamepadProvider::singleton()
{
    static NeverDestroyed<MockGamepadProvider> sharedProvider;
    return sharedProvider;
}

MockGamepadProvider::MockGamepadProvider() = default;

void MockGamepadProvider::startMonitoringGamepads(GamepadProviderClient& client)
{
    ASSERT(!m_clients.contains(&client));
    m_clients.add(&client);
}

void MockGamepadProvider::stopMonitoringGamepads(GamepadProviderClient& client)
{
    ASSERT(m_clients.contains(&client));
    m_clients.remove(&client);
}

void MockGamepadProvider::setMockGamepadDetails(unsigned index, const String& gamepadID, unsigned axisCount, unsigned buttonCount)
{
    if (index >= m_mockGamepadVector.size())
        m_mockGamepadVector.resize(index + 1);

    if (m_mockGamepadVector[index])
        m_mockGamepadVector[index]->updateDetails(gamepadID, axisCount, buttonCount);
    else
        m_mockGamepadVector[index] = makeUnique<MockGamepad>(index, gamepadID, axisCount, buttonCount);
}

bool MockGamepadProvider::connectMockGamepad(unsigned index)
{
    if (index < m_connectedGamepadVector.size() && m_connectedGamepadVector[index]) {
        LOG_ERROR("MockGamepadProvider: Attempt to connect a fake gamepad that is already connected (%u)", index);
        return false;
    }

    if (index >= m_mockGamepadVector.size() || !m_mockGamepadVector[index]) {
        LOG_ERROR("MockGamepadProvider: Attempt to connect a fake gamepad that doesn't have details set(%u)", index);
        return false;
    }

    if (m_connectedGamepadVector.size() <= index)
        m_connectedGamepadVector.reserveCapacity(index + 1);

    while (m_connectedGamepadVector.size() <= index)
        m_connectedGamepadVector.uncheckedAppend(nullptr);

    m_connectedGamepadVector[index] = m_mockGamepadVector[index].get();

    for (auto& client : m_clients)
        client->platformGamepadConnected(*m_connectedGamepadVector[index]);

    return true;
}

bool MockGamepadProvider::disconnectMockGamepad(unsigned index)
{
    if (index >= m_connectedGamepadVector.size() || !m_connectedGamepadVector[index]) {
        LOG_ERROR("MockGamepadProvider: Attempt to disconnect a fake gamepad that is not connected (%u)", index);
        return false;
    }
    if (m_connectedGamepadVector[index] != m_mockGamepadVector[index].get()) {
        LOG_ERROR("MockGamepadProvider: Vectors of fake gamepads and connected gamepads have gotten out of sync");
        return false;
    }

    m_connectedGamepadVector[index] = nullptr;

    for (auto& client : m_clients)
        client->platformGamepadDisconnected(*m_mockGamepadVector[index]);

    return true;
}

bool MockGamepadProvider::setMockGamepadAxisValue(unsigned index, unsigned axisIndex, double value)
{
    if (index >= m_mockGamepadVector.size() || !m_mockGamepadVector[index]) {
        LOG_ERROR("MockGamepadProvider: Attempt to set axis value on a fake gamepad that doesn't exist (%u)", index);
        return false;
    }

    m_mockGamepadVector[index]->setAxisValue(axisIndex, value);
    gamepadInputActivity();
    return true;
}

bool MockGamepadProvider::setMockGamepadButtonValue(unsigned index, unsigned buttonIndex, double value)
{
    if (index >= m_mockGamepadVector.size() || !m_mockGamepadVector[index]) {
        LOG_ERROR("MockGamepadProvider: Attempt to set button value on a fake gamepad that doesn't exist (%u)", index);
        return false;
    }

    m_mockGamepadVector[index]->setButtonValue(buttonIndex, value);
    setShouldMakeGamepadsVisibile();
    gamepadInputActivity();
    return true;
}

void MockGamepadProvider::gamepadInputActivity()
{
    if (!m_shouldScheduleActivityCallback)
        return;

    m_shouldScheduleActivityCallback = false;
    callOnMainThread([this]() {
        dispatchPlatformGamepadInputActivity();

        m_shouldScheduleActivityCallback = true;
    });
}

} // namespace WebCore

#endif // ENABLE(GAMEPAD)
