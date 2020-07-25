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

#import "config.h"
#import "VirtualGamepad.h"

#if HAVE(HID_FRAMEWORK) && USE(APPLE_INTERNAL_SDK)

#import <Foundation/Foundation.h>
#import <HID/HIDBase.h>
#import <HID/HIDDevice.h>
#import <HID/HIDElement.h>
#import <HID/HIDEvent.h>
#import <HID/HIDManager.h>
#import <HID/HIDUserDevice.h>
#import <IOKit/hid/IOHIDDeviceKeys.h>

namespace TestWebKitAPI {

VirtualGamepad::VirtualGamepad(const GamepadMapping& gamepadMapping)
    : m_gamepadMapping(gamepadMapping)
{
    m_dispatchQueue = adoptOSObject(dispatch_queue_create(0, DISPATCH_QUEUE_SERIAL));
    m_uniqueID = NSUUID.UUID.UUIDString;

    m_buttonValues = Vector<float>(m_gamepadMapping.buttonCount, 0.0);
    m_axisValues = Vector<float>(m_gamepadMapping.axisCount, 0.0);

    auto descriptor = adoptNS([[NSData alloc] initWithBytes:m_gamepadMapping.descriptorData length:m_gamepadMapping.descriptorDataSize]);
    auto properties = adoptNS([[NSMutableDictionary alloc] init]);

    properties.get()[@kIOHIDReportDescriptorKey] = descriptor.get();
    properties.get()[@kIOHIDVendorIDKey] = @((uint16_t)m_gamepadMapping.vendorID);
    properties.get()[@kIOHIDProductIDKey] = @((uint16_t)m_gamepadMapping.productID);
    properties.get()[@kIOHIDPhysicalDeviceUniqueIDKey] = m_uniqueID.get();
    properties.get()[@kIOHIDTransportKey] = @"Bluetooth";
    properties.get()[@"isVirtual"] = @(YES);
    properties.get()[@(kIOHIDProductKey)] = @(m_gamepadMapping.name);

    m_userDevice = adoptNS([[HIDUserDevice alloc] initWithProperties:properties.get()]);
    [m_userDevice setDispatchQueue:m_dispatchQueue.get()];
    [m_userDevice activate];

    m_device = adoptNS([[HIDDevice alloc] initWithService:m_userDevice.get().service]);
    [m_device setDispatchQueue:dispatch_get_main_queue()];
    [m_device open];
    [m_device activate];
}

VirtualGamepad::~VirtualGamepad()
{
    [m_device close];
    [m_device cancel];
    [m_userDevice cancel];
}

void VirtualGamepad::setButtonValue(size_t index, float value)
{
    RELEASE_ASSERT(index < m_buttonValues.size());
    RELEASE_ASSERT(value >= 0.0 && value <= 1.0);
    m_buttonValues[index] = value;
}

void VirtualGamepad::setAxisValue(size_t index, float value)
{
    RELEASE_ASSERT(index < m_axisValues.size());
    RELEASE_ASSERT(value >= -1.0 && value <= 1.0);
    m_axisValues[index] = value;
}

void VirtualGamepad::publishReport()
{
    m_gamepadMapping.publishReportCallback(m_buttonValues, m_axisValues, m_userDevice.get());
}

} // namespace TestWebKitAPI

#endif // HAVE(HID_FRAMEWORK) && USE(APPLE_INTERNAL_SDK)
