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
#include "Logging.h"
#include "PlatformGamepad.h"

namespace WebCore {

static const double ConnectionDelayInterval = 0.5;
static const double InputNotificationDelay = 0.05;

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
    , m_connectionDelayTimer(this, &HIDGamepadProvider::connectionDelayTimerFired)
    , m_inputNotificationTimer(this, &HIDGamepadProvider::inputNotificationTimerFired)
{
    m_manager = adoptCF(IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone));

    RetainPtr<CFDictionaryRef> joystickDictionary = deviceMatchingDictionary(kHIDPage_GenericDesktop, kHIDUsage_GD_Joystick);
    RetainPtr<CFDictionaryRef> gamepadDictionary = deviceMatchingDictionary(kHIDPage_GenericDesktop, kHIDUsage_GD_GamePad);

    CFDictionaryRef devices[] = { joystickDictionary.get(), gamepadDictionary.get() };

    RetainPtr<CFArrayRef> matchingArray = adoptCF(CFArrayCreate(kCFAllocatorDefault, (const void**)devices, 2, &kCFTypeArrayCallBacks));

    IOHIDManagerSetDeviceMatchingMultiple(m_manager.get(), matchingArray.get());
    IOHIDManagerRegisterDeviceMatchingCallback(m_manager.get(), deviceAddedCallback, this);
    IOHIDManagerRegisterDeviceRemovalCallback(m_manager.get(), deviceRemovedCallback, this);
    IOHIDManagerRegisterInputValueCallback(m_manager.get(), deviceValuesChangedCallback, this);
}

unsigned HIDGamepadProvider::indexForNewlyConnectedDevice()
{
    unsigned index = 0;
    while (index < m_gamepadVector.size() && m_gamepadVector[index])
        ++index;

    return index;
}

void HIDGamepadProvider::connectionDelayTimerFired(Timer&)
{
    m_shouldDispatchCallbacks = true;
}

void HIDGamepadProvider::openAndScheduleManager()
{
    LOG(Gamepad, "HIDGamepadProvider opening/scheduling HID manager");

    ASSERT(m_gamepadVector.isEmpty());
    ASSERT(m_gamepadMap.isEmpty());

    m_shouldDispatchCallbacks = false;

    IOHIDManagerScheduleWithRunLoop(m_manager.get(), CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
    IOHIDManagerOpen(m_manager.get(), kIOHIDOptionsTypeNone);

    // Any connections we are notified of within the ConnectionDelayInterval of listening likely represent
    // devices that were already connected, so we suppress notifying clients of these.
    m_connectionDelayTimer.startOneShot(ConnectionDelayInterval);
}

void HIDGamepadProvider::closeAndUnscheduleManager()
{
    LOG(Gamepad, "HIDGamepadProvider closing/unscheduling HID manager");

    IOHIDManagerUnscheduleFromRunLoop(m_manager.get(), CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
    IOHIDManagerClose(m_manager.get(), kIOHIDOptionsTypeNone);

    m_gamepadVector.clear();
    m_gamepadMap.clear();

    m_connectionDelayTimer.stop();
}

void HIDGamepadProvider::startMonitoringGamepads(GamepadProviderClient* client)
{
    bool shouldOpenAndScheduleManager = m_clients.isEmpty();

    ASSERT(!m_clients.contains(client));
    m_clients.add(client);

    if (shouldOpenAndScheduleManager)
        openAndScheduleManager();
}
void HIDGamepadProvider::stopMonitoringGamepads(GamepadProviderClient* client)
{
    ASSERT(m_clients.contains(client));

    bool shouldCloseAndUnscheduleManager = m_clients.remove(client) && m_clients.isEmpty();

    if (shouldCloseAndUnscheduleManager)
        closeAndUnscheduleManager();
}

void HIDGamepadProvider::deviceAdded(IOHIDDeviceRef device)
{
    ASSERT(!m_gamepadMap.get(device));

    LOG(Gamepad, "HIDGamepadProvider device %p added", device);

    unsigned index = indexForNewlyConnectedDevice();
    std::unique_ptr<HIDGamepad> gamepad = std::make_unique<HIDGamepad>(device, index);

    if (m_gamepadVector.size() <= index)
        m_gamepadVector.resize(index + 1);

    m_gamepadVector[index] = gamepad.get();
    m_gamepadMap.set(device, WTF::move(gamepad));

    if (!m_shouldDispatchCallbacks) {
        // This added device is the result of us starting to monitor gamepads.
        // We'll get notified of all connected devices during this current spin of the runloop
        // and we don't want to tell the client about any of them.
        // The m_connectionDelayTimer fires in a subsequent spin of the runloop after which
        // any connection events are actual new devices.
        m_connectionDelayTimer.startOneShot(0);

        LOG(Gamepad, "Device %p was added while suppressing callbacks, so this should be an 'already connected' event", device);

        return;
    }

    for (auto& client : m_clients)
        client->platformGamepadConnected(*m_gamepadVector[index]);
}

void HIDGamepadProvider::deviceRemoved(IOHIDDeviceRef device)
{
    LOG(Gamepad, "HIDGamepadProvider device %p removed", device);

    std::unique_ptr<HIDGamepad> removedGamepad = removeGamepadForDevice(device);
    ASSERT(removedGamepad);

    // Any time we get a device removed callback we know it's a real event and not an 'already connected' event.
    // We should always stop supressing callbacks when we receive such an event.
    m_shouldDispatchCallbacks = true;

    for (auto& client : m_clients)
        client->platformGamepadDisconnected(*removedGamepad);
}

void HIDGamepadProvider::valuesChanged(IOHIDValueRef value)
{
    IOHIDDeviceRef device = IOHIDElementGetDevice(IOHIDValueGetElement(value));

    HIDGamepad* gamepad = m_gamepadMap.get(device);

    // When starting monitoring we might get a value changed callback before we even know the device is connected.
    if (!gamepad)
        return;

    gamepad->valueChanged(value);

    // This isActive check is necessary as we want to delay input notifications from the time of the first input,
    // and not push the notification out on every subsequent input.
    if (!m_inputNotificationTimer.isActive())
        m_inputNotificationTimer.startOneShot(InputNotificationDelay);
}

void HIDGamepadProvider::inputNotificationTimerFired(Timer&)
{
    if (!m_shouldDispatchCallbacks)
        return;

    for (auto& client : m_clients)
        client->platformGamepadInputActivity();
}

std::unique_ptr<HIDGamepad> HIDGamepadProvider::removeGamepadForDevice(IOHIDDeviceRef device)
{
    std::unique_ptr<HIDGamepad> result = m_gamepadMap.take(device);
    ASSERT(result);

    auto i = m_gamepadVector.find(result.get());
    if (i != notFound)
        m_gamepadVector[i] = nullptr;

    return result;
}

} // namespace WebCore

#endif // ENABLE(GAMEPAD)
