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

#if ENABLE(GAMEPAD) && PLATFORM(MAC)

#import "GamepadProviderClient.h"
#import "Logging.h"
#import "PlatformGamepad.h"
#import <pal/spi/mac/IOKitSPIMac.h>
#import <wtf/NeverDestroyed.h>

#if HAVE(GCCONTROLLER_HID_DEVICE_CHECK)
#import <GameController/GCController.h>
#endif

#import "GameControllerSoftLink.h"

namespace WebCore {

static const Seconds connectionDelayInterval { 500_ms };
static const Seconds hidInputNotificationDelay { 50_ms };

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
    listener->deviceAdded(device, HIDGamepadProvider::AllowManagementDecisionDelay::Yes);
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

HIDGamepadProvider& HIDGamepadProvider::singleton()
{
    static NeverDestroyed<HIDGamepadProvider> sharedListener;
    return sharedListener;
}

HIDGamepadProvider::HIDGamepadProvider()
    : m_initialGamepadsConnectedTimer(*this, &HIDGamepadProvider::initialGamepadsConnectedTimerFired)
    , m_inputNotificationTimer(*this, &HIDGamepadProvider::inputNotificationTimerFired)
{
    m_manager = adoptCF(IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone));

    RetainPtr<CFDictionaryRef> joystickDictionary = deviceMatchingDictionary(kHIDPage_GenericDesktop, kHIDUsage_GD_Joystick);
    RetainPtr<CFDictionaryRef> gamepadDictionary = deviceMatchingDictionary(kHIDPage_GenericDesktop, kHIDUsage_GD_GamePad);

    CFDictionaryRef devices[] = { joystickDictionary.get(), gamepadDictionary.get() };

    RetainPtr<CFArrayRef> matchingArray = adoptCF(CFArrayCreate(kCFAllocatorDefault, (const void**)devices, 2, &kCFTypeArrayCallBacks));

    IOHIDManagerSetDeviceMatchingMultiple(m_manager.get(), matchingArray.get());
    IOHIDManagerRegisterDeviceMatchingCallback(m_manager.get(), deviceAddedCallback, this);
    IOHIDManagerRegisterDeviceRemovalCallback(m_manager.get(), deviceRemovedCallback, this);

    startMonitoringInput();
}

void HIDGamepadProvider::stopMonitoringInput()
{
    IGNORE_NULL_CHECK_WARNINGS_BEGIN
    IOHIDManagerRegisterInputValueCallback(m_manager.get(), nullptr, nullptr);
    IGNORE_NULL_CHECK_WARNINGS_END
}

void HIDGamepadProvider::startMonitoringInput()
{
    IOHIDManagerRegisterInputValueCallback(m_manager.get(), deviceValuesChangedCallback, this);
}

unsigned HIDGamepadProvider::indexForNewlyConnectedDevice()
{
    unsigned index = 0;
    while (index < m_gamepadVector.size() && m_gamepadVector[index])
        ++index;

    return index;
}

void HIDGamepadProvider::initialGamepadsConnectedTimerFired()
{
    m_initialGamepadsConnected = true;
}

void HIDGamepadProvider::openAndScheduleManager()
{
    LOG(Gamepad, "HIDGamepadProvider opening/scheduling HID manager");

    ASSERT(m_gamepadVector.isEmpty());
    ASSERT(m_gamepadMap.isEmpty());

    m_initialGamepadsConnected = false;

    IOHIDManagerScheduleWithRunLoop(m_manager.get(), CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
    IOHIDManagerOpen(m_manager.get(), kIOHIDOptionsTypeNone);

    // Any connections we are notified of within the connectionDelayInterval of listening likely represent
    // devices that were already connected, so we suppress notifying clients of these.
    m_initialGamepadsConnectedTimer.startOneShot(connectionDelayInterval);
}

void HIDGamepadProvider::closeAndUnscheduleManager()
{
    LOG(Gamepad, "HIDGamepadProvider closing/unscheduling HID manager");

    IOHIDManagerUnscheduleFromRunLoop(m_manager.get(), CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
    IOHIDManagerClose(m_manager.get(), kIOHIDOptionsTypeNone);

    m_gamepadVector.clear();
    m_gamepadMap.clear();

    m_initialGamepadsConnectedTimer.stop();
}

void HIDGamepadProvider::startMonitoringGamepads(GamepadProviderClient& client)
{
    bool shouldOpenAndScheduleManager = m_clients.isEmpty();

    ASSERT(!m_clients.contains(&client));
    m_clients.add(&client);

    if (shouldOpenAndScheduleManager)
        openAndScheduleManager();
}
void HIDGamepadProvider::stopMonitoringGamepads(GamepadProviderClient& client)
{
    ASSERT(m_clients.contains(&client));

    bool shouldCloseAndUnscheduleManager = m_clients.remove(&client) && m_clients.isEmpty();

    if (shouldCloseAndUnscheduleManager)
        closeAndUnscheduleManager();
}

#if HAVE(MULTIGAMEPADPROVIDER_SUPPORT)

enum class GameControllerFrameworkHandlesDevice : uint8_t {
    No,
    Yes,
    Maybe,
};

static GameControllerFrameworkHandlesDevice gameControllerFrameworkWillHandleHIDDevice(IOHIDDeviceRef device)
{
    if (!isGameControllerFrameworkAvailable())
        return GameControllerFrameworkHandlesDevice::No;

#if HAVE(GCCONTROLLER_HID_DEVICE_CHECK)
    return [getGCControllerClass() supportsHIDDevice:device] ? GameControllerFrameworkHandlesDevice::Yes : GameControllerFrameworkHandlesDevice::No;
#endif

    auto deviceService = IOHIDDeviceGetService(device);
    if (!deviceService)
        return GameControllerFrameworkHandlesDevice::Maybe;

    // Check the service directly backing this device
    uint64_t registryID;
    if (IORegistryEntryGetRegistryEntryID(deviceService, &registryID) != KERN_SUCCESS)
        return GameControllerFrameworkHandlesDevice::Maybe;

    auto eventSystemClient = adoptCF(IOHIDEventSystemClientCreate(nullptr));
    IOHIDEventSystemClientSetMatching(eventSystemClient.get(), nullptr);
    auto serviceClient = adoptCF(IOHIDEventSystemClientCopyServiceForRegistryID(eventSystemClient.get(), registryID));
    if (serviceClient && ControllerClassForService(serviceClient.get()))
        return GameControllerFrameworkHandlesDevice::Yes;

    // Otherwise, check its grandchild service
    io_registry_entry_t child;
    if (IORegistryEntryGetChildEntry(deviceService, kIOServicePlane, &child) != KERN_SUCCESS)
        return GameControllerFrameworkHandlesDevice::Maybe;
    if (!child)
        return GameControllerFrameworkHandlesDevice::Maybe;

    if (IORegistryEntryGetChildEntry(child, kIOServicePlane, &child) != KERN_SUCCESS)
        return GameControllerFrameworkHandlesDevice::Maybe;
    if (!child)
        return GameControllerFrameworkHandlesDevice::Maybe;

    if (IORegistryEntryGetRegistryEntryID(child, &registryID) != KERN_SUCCESS)
        return GameControllerFrameworkHandlesDevice::Maybe;

    serviceClient = adoptCF(IOHIDEventSystemClientCopyServiceForRegistryID(eventSystemClient.get(), registryID));
    if (!serviceClient)
        return GameControllerFrameworkHandlesDevice::Maybe;

    return ControllerClassForService(serviceClient.get()) ? GameControllerFrameworkHandlesDevice::Yes : GameControllerFrameworkHandlesDevice::No;
}

#if !HAVE(GCCONTROLLER_HID_DEVICE_CHECK)

static const Seconds managementDecisionDelay { 1000_ms };

void HIDGamepadProvider::waitForManagementDecisionForDevice(IOHIDDeviceRef device)
{
    LOG(Gamepad, "Unable to determine if GameController framework will handle newly attached device %p - Delaying management decision", device);
    m_devicesWaitingManagementDecision.add(device);

    if (!m_delayManagementDecisionQueue)
        m_delayManagementDecisionQueue = WorkQueue::create("com.apple.WebKit.GamepadManagementDecision", WorkQueue::Type::Serial, WorkQueue::QOS::Utility);

    auto delayedDevice = RetainPtr<IOHIDDeviceRef>(device);
    m_delayManagementDecisionQueue->dispatchAfter(managementDecisionDelay, [this, delayedDevice = WTFMove(delayedDevice)] {
        // If this device was still waiting for its management decision, then the HID provider will handle it.
        if (removeDeviceWaitingForManagementDecision(delayedDevice.get()))
            deviceAdded(delayedDevice.get(), AllowManagementDecisionDelay::No);
        else
            LOG(Gamepad, "Management decision delay for device %p has expired, but device has already been handled", delayedDevice.get());
    });

    if (m_devicesWaitingManagementDecision.size() > 1)
        return;

    m_eventSystemClient = adoptCF(IOHIDEventSystemClientCreateWithType(kCFAllocatorDefault, kIOHIDEventSystemClientTypeMonitor, 0));

    NSMutableArray *matchingAttributes = [NSMutableArray array];
    [matchingAttributes addObject:@{@(kIOHIDDeviceUsagePageKey) : @(kHIDPage_GenericDesktop), @(kIOHIDDeviceUsageKey) : @(kHIDUsage_GD_GamePad)}];
    [matchingAttributes addObject:@{@(kIOHIDDeviceUsagePageKey) : @(kHIDPage_GenericDesktop), @(kIOHIDDeviceUsageKey) : @(kHIDUsage_GD_Joystick)}];

    IOHIDEventSystemClientSetMatchingMultiple(m_eventSystemClient.get(), (__bridge CFArrayRef)matchingAttributes);
    IOHIDEventSystemClientScheduleWithDispatchQueue(m_eventSystemClient.get(), dispatch_get_main_queue());
    IOHIDEventSystemClientRegisterDeviceMatchingBlock(m_eventSystemClient.get(), ^(void *, void *, IOHIDServiceClientRef) {
        // We don't actually care about the actual published service.
        // Any new GamePad service is worth re-checking every pending IOHIDDevice against GameController framework.
        HIDGamepadProvider::singleton().gamePadServiceWasPublished();
    }, nullptr, nullptr);
}

bool HIDGamepadProvider::removeDeviceWaitingForManagementDecision(IOHIDDeviceRef device)
{
    if (!m_devicesWaitingManagementDecision.remove(device))
        return false;

    if (m_devicesWaitingManagementDecision.isEmpty()) {
        // That was the last device, so we can stop monitoring for IOHIDServiceClients
        ASSERT(m_eventSystemClient);
        IOHIDEventSystemClientUnregisterDeviceMatchingBlock(m_eventSystemClient.get());
        m_eventSystemClient = nullptr;
        m_delayManagementDecisionQueue = nullptr;
    }

    return true;
}

void HIDGamepadProvider::gamePadServiceWasPublished()
{
    LOG(Gamepad, "HIDGamepadProvider::gamePadServiceWasPublished()");
    IOHIDDeviceRef matchedDevice = nullptr;
    for (auto device : m_devicesWaitingManagementDecision) {
        auto result = gameControllerFrameworkWillHandleHIDDevice(device);
        if (result == GameControllerFrameworkHandlesDevice::Yes || result == GameControllerFrameworkHandlesDevice::No) {
            matchedDevice = device;
            break;
        }
    }

    if (matchedDevice) {
        LOG(Gamepad, "Device %p pending management decision can now be managed", matchedDevice);
        removeDeviceWaitingForManagementDecision(matchedDevice);
        deviceAdded(matchedDevice, AllowManagementDecisionDelay::No);
    }
}

#endif // !HAVE(GCCONTROLLER_HID_DEVICE_CHECK)
#endif // HAVE(MULTIGAMEPADPROVIDER_SUPPORT)

void HIDGamepadProvider::deviceAdded(IOHIDDeviceRef device, AllowManagementDecisionDelay allowManagementDecisionDelay)
{
#if HAVE(MULTIGAMEPADPROVIDER_SUPPORT)
    if (m_ignoresGameControllerFrameworkDevices) {
        auto result = gameControllerFrameworkWillHandleHIDDevice(device);
        switch (result) {
        case GameControllerFrameworkHandlesDevice::Yes:
            LOG(Gamepad, "GameController framework will handle newly attached device %p - HIDGamepadProvider ignoring it", device);
            m_gameControllerManagedGamepads.add(device);
            return;

        case GameControllerFrameworkHandlesDevice::Maybe:
#if HAVE(GCCONTROLLER_HID_DEVICE_CHECK)
            UNUSED_PARAM(allowManagementDecisionDelay);
            RELEASE_ASSERT_NOT_REACHED();
#else
            if (allowManagementDecisionDelay == AllowManagementDecisionDelay::No) {
                LOG(Gamepad, "Unable to determine if GameController framework will handle attached device %p - HIDGamepadProvider will handle it", device);
                break;
            }

            // Give IOHIDServiceClient callbacks a chance to confirm whether or not
            // this device will be handled by GameController framework
            waitForManagementDecisionForDevice(device);
            return;
#endif // HAVE(GCCONTROLLER_HID_DEVICE_CHECK)
        case GameControllerFrameworkHandlesDevice::No:
            LOG(Gamepad, "GameController framework does not handle attached device %p - HIDGamepadProvider will handle it", device);
            break;
        }
    }
#else
    UNUSED_PARAM(allowManagementDecisionDelay);
#endif // HAVE(MULTIGAMEPADPROVIDER_SUPPORT)

    ASSERT(!m_gamepadMap.get(device));

    LOG(Gamepad, "HIDGamepadProvider device %p added", device);

    unsigned index = indexForNewlyConnectedDevice();
    std::unique_ptr<HIDGamepad> gamepad = makeUnique<HIDGamepad>(device, index);

    if (m_gamepadVector.size() <= index)
        m_gamepadVector.grow(index + 1);

    m_gamepadVector[index] = gamepad.get();
    m_gamepadMap.set(device, WTFMove(gamepad));

    if (!m_initialGamepadsConnected) {
        // This added device is the result of us starting to monitor gamepads.
        // We'll get notified of all connected devices during this current spin of the runloop
        // and the client should be told they were already connected.
        // The m_initialGamepadsConnectedTimer fires in a subsequent spin of the runloop after which
        // any connection events are actual new devices and can trigger gamepad visibility.
        if (!m_initialGamepadsConnectedTimer.isActive())
            m_initialGamepadsConnectedTimer.startOneShot(0_s);
    }

    auto eventVisibility = m_initialGamepadsConnected ? EventMakesGamepadsVisible::Yes : EventMakesGamepadsVisible::No;
    for (auto& client : m_clients)
        client->platformGamepadConnected(*m_gamepadVector[index], eventVisibility);

    // If we are working together with the GameController provider, let it know
    // that gamepads should now be visible.
    if (m_ignoresGameControllerFrameworkDevices && eventVisibility == EventMakesGamepadsVisible::Yes)
        GameControllerGamepadProvider::singleton().makeInvisibleGamepadsVisible();
}

void HIDGamepadProvider::deviceRemoved(IOHIDDeviceRef device)
{
#if HAVE(MULTIGAMEPADPROVIDER_SUPPORT) && !HAVE(GCCONTROLLER_HID_DEVICE_CHECK)
    bool wasRemoved = removeDeviceWaitingForManagementDecision(device);
#endif

    std::unique_ptr<HIDGamepad> removedGamepad = removeGamepadForDevice(device);

    if (!removedGamepad) {
#if HAVE(MULTIGAMEPADPROVIDER_SUPPORT)
        auto taken = m_gameControllerManagedGamepads.take(device);

        // If this device didn't show up in the gamepad map, then it was either managed by GameController framework
        // or we were still waiting to decide which provider would manage it.
#if HAVE(GCCONTROLLER_HID_DEVICE_CHECK)
        ASSERT(taken);
#else
        ASSERT(taken || wasRemoved);
#endif

        if (taken)
            LOG(Gamepad, "HIDGamepadProvider informed of removal of device %p, which is managed by GameController framework. Ignoring.", device);
        else
            LOG(Gamepad, "HIDGamepadProvider informed of removal of device %p, which was not yet managed. Ignoring.", device);
#endif
        return;
    }

    LOG(Gamepad, "HIDGamepadProvider device %p removed", device);

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

    if (gamepad->valueChanged(value) == HIDInputType::ButtonPress)
        setShouldMakeGamepadsVisibile();

    // This isActive check is necessary as we want to delay input notifications from the time of the first input,
    // and not push the notification out on every subsequent input.
    if (!m_inputNotificationTimer.isActive())
        m_inputNotificationTimer.startOneShot(hidInputNotificationDelay);
}

void HIDGamepadProvider::inputNotificationTimerFired()
{
    if (!m_initialGamepadsConnected) {
        if (!m_inputNotificationTimer.isActive())
            m_inputNotificationTimer.startOneShot(0_s);
        return;
    }

    dispatchPlatformGamepadInputActivity();
}

std::unique_ptr<HIDGamepad> HIDGamepadProvider::removeGamepadForDevice(IOHIDDeviceRef device)
{
    std::unique_ptr<HIDGamepad> result = m_gamepadMap.take(device);
    if (!result)
        return nullptr;

    auto i = m_gamepadVector.find(result.get());
    if (i != notFound)
        m_gamepadVector[i] = nullptr;

    return result;
}

} // namespace WebCore

#endif // ENABLE(GAMEPAD) && PLATFORM(MAC)
