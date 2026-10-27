/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "AutomationGamepadProvider.h"

#if ENABLE(GAMEPAD) && PLATFORM(GTK)

#include "AutomationGamepad.h"
#include <WebCore/GamepadProviderClient.h>
#include <cmath>
#include <wtf/CompletionHandler.h>
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>

namespace WebKit {

using namespace WebCore;

AutomationGamepadProvider& AutomationGamepadProvider::singleton()
{
    static NeverDestroyed<AutomationGamepadProvider> sharedProvider;
    return sharedProvider;
}

AutomationGamepadProvider::AutomationGamepadProvider() = default;

void AutomationGamepadProvider::startMonitoringGamepads(GamepadProviderClient& client)
{
    ASSERT(!m_clients.contains(client));
    m_clients.add(client);
    // A device may be registered before the page starts monitoring. Replay
    // descriptions without revealing them until a subsequent input frame.
    for (CheckedPtr gamepad : m_connectedGamepadVector) {
        if (gamepad)
            client.platformGamepadConnected(*gamepad, EventMakesGamepadsVisible::No);
    }
}

void AutomationGamepadProvider::stopMonitoringGamepads(GamepadProviderClient& client)
{
    ASSERT(m_clients.contains(client));
    m_clients.remove(client);
}

void AutomationGamepadProvider::setAutomationGamepadDetails(unsigned index, const String& gamepadID, const String& mapping, unsigned axisCount, unsigned buttonCount, bool supportsDualRumble, bool wasConnected)
{
    if (index >= m_mockGamepadVector.size())
        m_mockGamepadVector.grow(index + 1);

    if (CheckedPtr gamepad = m_mockGamepadVector[index].get())
        gamepad->updateDetails(gamepadID, mapping, axisCount, buttonCount, supportsDualRumble, wasConnected);
    else
        m_mockGamepadVector[index] = makeUnique<AutomationGamepad>(index, gamepadID, mapping, axisCount, buttonCount, supportsDualRumble, wasConnected);
}

bool AutomationGamepadProvider::connectAutomationGamepad(unsigned index)
{
    if (index < m_connectedGamepadVector.size() && m_connectedGamepadVector[index]) {
        LOG_ERROR("AutomationGamepadProvider: Attempt to connect a fake gamepad that is already connected (%u)", index);
        return false;
    }

    if (index >= m_mockGamepadVector.size() || !m_mockGamepadVector[index]) {
        LOG_ERROR("AutomationGamepadProvider: Attempt to connect a fake gamepad that doesn't have details set(%u)", index);
        return false;
    }

    if (m_connectedGamepadVector.size() <= index)
        m_connectedGamepadVector.reserveCapacity(index + 1);

    while (m_connectedGamepadVector.size() <= index)
        m_connectedGamepadVector.append(nullptr);

    m_connectedGamepadVector[index] = m_mockGamepadVector[index].get();

    EventMakesGamepadsVisible eventMakesGamepadsVisible = m_mockGamepadVector[index]->wasConnected() ?
        EventMakesGamepadsVisible::No : EventMakesGamepadsVisible::Yes;

    CheckedRef connectedGamepad = *m_connectedGamepadVector[index];
    for (Ref client : m_clients)
        client->platformGamepadConnected(connectedGamepad, eventMakesGamepadsVisible);

    return true;
}

bool AutomationGamepadProvider::disconnectAutomationGamepad(unsigned index)
{
    if (index >= m_connectedGamepadVector.size() || !m_connectedGamepadVector[index]) {
        LOG_ERROR("AutomationGamepadProvider: Attempt to disconnect a fake gamepad that is not connected (%u)", index);
        return false;
    }
    if (m_connectedGamepadVector[index] != m_mockGamepadVector[index].get()) {
        LOG_ERROR("AutomationGamepadProvider: Vectors of fake gamepads and connected gamepads have gotten out of sync");
        return false;
    }

    m_connectedGamepadVector[index] = nullptr;

    CheckedRef gamepadToRemove = *m_mockGamepadVector[index];
    for (Ref client : m_clients)
        client->platformGamepadDisconnected(gamepadToRemove);

    return true;
}

bool AutomationGamepadProvider::applyInputFrame(const Vector<AutomationGamepadInput>& inputs)
{
    // Validate the entire frame before changing any device.
    for (auto& input : inputs) {
        if (input.gamepadIndex >= m_connectedGamepadVector.size() || !m_connectedGamepadVector[input.gamepadIndex])
            return false;
        auto& gamepad = *m_mockGamepadVector[input.gamepadIndex];
        auto controlCount = input.isButton ? gamepad.buttonValues().size() : gamepad.axisValues().size();
        if (input.controlIndex >= controlCount || !std::isfinite(input.value) || input.value > 1 || input.value < (input.isButton ? 0 : -1))
            return false;
    }

    auto timestamp = MonotonicTime::now();
    for (auto& input : inputs) {
        auto& gamepad = *m_mockGamepadVector[input.gamepadIndex];
        if (input.isButton)
            gamepad.setButtonValue(input.controlIndex, input.value);
        else
            gamepad.setAxisValue(input.controlIndex, input.value);
        gamepad.setLastUpdateTime(timestamp);
        if (input.value)
            setShouldMakeGamepadsVisibile();
    }
    if (!inputs.isEmpty())
        dispatchPlatformGamepadInputActivity();
    return true;
}

void AutomationGamepadProvider::clearGamepadsForTesting()
{
    // Disconnect any remaining connected gamepads.
    for (size_t i = 0; i < m_connectedGamepadVector.size(); ++i) {
        if (m_connectedGamepadVector[i])
            disconnectAutomationGamepad(i);
    }
    m_mockGamepadVector.clear();
    m_connectedGamepadVector.clear();
}

void AutomationGamepadProvider::playEffect(unsigned, const String&, GamepadHapticEffectType, const GamepadEffectParameters&, CompletionHandler<void(bool)>&& completionHandler)
{
    completionHandler(false);
}

void AutomationGamepadProvider::stopEffects(unsigned, const String&, CompletionHandler<void()>&& completionHandler)
{
    completionHandler();
}

} // namespace WebCore

#endif // ENABLE(GAMEPAD) && PLATFORM(GTK)
