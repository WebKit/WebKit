/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#include "HIDGamepadProvider.h"

#if ENABLE(GAMEPAD)

#include "GamepadProviderClient.h"
#include "PlatformGamepad.h"
#include <wtf/MainThread.h>

namespace WebCore {

static RetainPtr<CFDictionaryRef> deviceMatchingDictionary(uint32_t usagePage, uint32_t usage)
{
    ASSERT(usagePage);
    ASSERT(usage);

    RetainPtr<CFNumberRef> pageNumber = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &usagePage));
    RetainPtr<CFNumberRef> usageNumber = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &usage));

    CFStringRef keys[] = { CFSTR(kIOHIDDeviceUsagePageKey), CFSTR(kIOHIDDeviceUsageKey) };
    CFNumberRef values[] = { pageNumber.get(), usageNumber.get() };

    return adoptCF(CFDictionaryCreate(kCFAllocatorDefault, (const void**)keys, (const void**)values, 2, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
}

static void deviceAddedCallback(void* context, IOReturn, void*, IOHIDDeviceRef device)
{
    HIDGamepadProvider* listener = static_cast<HIDGamepadProvider*>(context);
    listener->deviceAdded(device);
}

static void deviceRemovedCallback(void* context, IOReturn, void*, IOHIDDeviceRef device)
{
    HIDGamepadProvider* listener = static_cast<HIDGamepadProvider*>(context);
    listener->deviceRemoved(device);
}

static void deviceValuesChangedCallback(void* context, IOReturn result, void*, IOHIDValueRef value)
{
    // A non-zero result value indicates an error that we can do nothing about for input values.
    if (result)
        return;

    HIDGamepadProvider* listener = static_cast<HIDGamepadProvider*>(context);
    listener->valuesChanged(value);
}

HIDGamepadProvider& HIDGamepadProvider::shared()
{
    static NeverDestroyed<HIDGamepadProvider> sharedListener;
    return sharedListener;
}

HIDGamepadProvider::HIDGamepadProvider()
    : m_shouldDispatchCallbacks(false)
{
    m_manager = adoptCF(IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone));

    RetainPtr<CFDictionaryRef> joystickDictionary = deviceMatchingDictionary(kHIDPage_GenericDesktop, kHIDUsage_GD_Joystick);
    RetainPtr<CFDictionaryRef> gamepadDictionary = deviceMatchingDictionary(kHIDPage_GenericDesktop, kHIDUsage_GD_GamePad);

    CFDictionaryRef devices[] = { joystickDictionary.get(), gamepadDictionary.get() };

    RetainPtr<CFArrayRef> matchingArray = adoptCF(CFArrayCreate(kCFAllocatorDefault, (const void**)devices, 2, &kCFTypeArrayCallBacks));

    IOHIDManagerSetDeviceMatchingMultiple(m_manager.get(), matchingArray.get());
    IOHIDManagerRegisterDeviceMatchingCallback(m_manager.get(), deviceAddedCallback, this);
    IOHIDManagerRegisterDeviceRemovalCallback(m_manager.get(), deviceRemovedCallback, this);
    IOHIDManagerScheduleWithRunLoop(m_manager.get(), CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
    IOHIDManagerOpen(m_manager.get(), kIOHIDOptionsTypeNone);

    IOHIDManagerRegisterInputValueCallback(m_manager.get(), deviceValuesChangedCallback, this);

    // We'll immediately get a series of connection and value callbacks for already-connected gamepads
    // but we don't want to notify WebCore of those events.
    // This callOnMainThread call re-enables those callbacks after the runloop empties out.
    callOnMainThread([]() {
        HIDGamepadProvider::shared().setShouldDispatchCallbacks(true);
    });
}

unsigned HIDGamepadProvider::indexForNewlyConnectedDevice()
{
    unsigned index = 0;
    while (index < m_gamepadVector.size() && m_gamepadVector[index])
        ++index;

    return index;
}

void HIDGamepadProvider::startMonitoringGamepads(GamepadProviderClient* client)
{
    m_clients.add(client);
}
void HIDGamepadProvider::stopMonitoringGamepads(GamepadProviderClient* client)
{
    m_clients.remove(client);
}

void HIDGamepadProvider::deviceAdded(IOHIDDeviceRef device)
{
    ASSERT(!m_gamepadMap.get(device));

    std::unique_ptr<HIDGamepad> gamepad = std::make_unique<HIDGamepad>(device);
    unsigned index = indexForNewlyConnectedDevice();

    if (m_gamepadVector.size() <= index)
        m_gamepadVector.resize(index + 1);

    m_gamepadVector[index] = gamepad.get();
    m_gamepadMap.set(device, std::move(gamepad));

    if (!m_shouldDispatchCallbacks)
        return;

    for (auto& client : m_clients)
        client->platformGamepadConnected(index);
}

void HIDGamepadProvider::deviceRemoved(IOHIDDeviceRef device)
{
    std::pair<std::unique_ptr<HIDGamepad>, unsigned> removedGamepad = removeGamepadForDevice(device);
    ASSERT(removedGamepad.first);

    if (!m_shouldDispatchCallbacks)
        return;

    for (auto& client : m_clients)
        client->platformGamepadDisconnected(removedGamepad.second);
}

void HIDGamepadProvider::valuesChanged(IOHIDValueRef value)
{
    IOHIDDeviceRef device = IOHIDElementGetDevice(IOHIDValueGetElement(value));

    HIDGamepad* gamepad = m_gamepadMap.get(device);

    // When starting monitoring we might get a value changed callback before we even know the device is connected.
    if (!gamepad)
        return;

    gamepad->valueChanged(value);
}

std::pair<std::unique_ptr<HIDGamepad>, unsigned>  HIDGamepadProvider::removeGamepadForDevice(IOHIDDeviceRef device)
{
    std::pair<std::unique_ptr<HIDGamepad>, unsigned> result;
    result.first = m_gamepadMap.take(device);
    ASSERT(result.first);

    for (unsigned i = 0; i < m_gamepadVector.size(); ++i) {
        if (m_gamepadVector[i] == result.first.get()) {
            result.second = i;
            m_gamepadVector[i] = nullptr;
            break;
        }
    }

    return result;
}

} // namespace WebCore

#endif // ENABLE(GAMEPAD)
