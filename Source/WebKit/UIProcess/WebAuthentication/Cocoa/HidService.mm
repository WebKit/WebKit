/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#import "HidService.h"

#if ENABLE(WEB_AUTHN) && PLATFORM(MAC)

#import "CtapHidAuthenticator.h"
#import "CtapHidDriver.h"
#import "HidConnection.h"
#import <WebCore/DeviceRequestConverter.h>
#import <WebCore/DeviceResponseConverter.h>
#import <WebCore/FidoConstants.h>
#import <WebCore/FidoHidMessage.h>
#import <wtf/RunLoop.h>

namespace WebKit {
using namespace fido;

// FIXME(191518)
static void deviceAddedCallback(void* context, IOReturn, void*, IOHIDDeviceRef device)
{
    ASSERT(device);
    auto* listener = static_cast<HidService*>(context);
    listener->deviceAdded(device);
}

// FIXME(191518)
static void deviceRemovedCallback(void* context, IOReturn, void*, IOHIDDeviceRef device)
{
    // FIXME(191525)
}

HidService::HidService(Observer& observer)
    : AuthenticatorTransportService(observer)
{
    m_manager = adoptCF(IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone));
    NSDictionary *matchingDictionary = @{
        @kIOHIDPrimaryUsagePageKey: adoptNS([NSNumber numberWithInt:kCTAPHIDUsagePage]).get(),
        @kIOHIDPrimaryUsageKey: adoptNS([NSNumber numberWithInt:kCTAPHIDUsage]).get()
    };
    IOHIDManagerSetDeviceMatching(m_manager.get(), (__bridge CFDictionaryRef)matchingDictionary);
    IOHIDManagerRegisterDeviceMatchingCallback(m_manager.get(), deviceAddedCallback, this);
    IOHIDManagerRegisterDeviceRemovalCallback(m_manager.get(), deviceRemovedCallback, this);
}

HidService::~HidService()
{
    IOHIDManagerUnscheduleFromRunLoop(m_manager.get(), CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
    IOHIDManagerClose(m_manager.get(), kIOHIDOptionsTypeNone);
}

void HidService::startDiscoveryInternal()
{
    platformStartDiscovery();
}

void HidService::platformStartDiscovery()
{
    IOHIDManagerScheduleWithRunLoop(m_manager.get(), CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
    IOHIDManagerOpen(m_manager.get(), kIOHIDOptionsTypeNone);
}

UniqueRef<HidConnection> HidService::createHidConnection(IOHIDDeviceRef device) const
{
    return makeUniqueRef<HidConnection>(device);
}

void HidService::deviceAdded(IOHIDDeviceRef device)
{
    auto driver = std::make_unique<CtapHidDriver>(createHidConnection(device));
    // Get authenticator info from the device.
    driver->transact(encodeEmptyAuthenticatorRequest(CtapRequestCommand::kAuthenticatorGetInfo), [weakThis = makeWeakPtr(*this), ptr = driver.get()](Vector<uint8_t>&& response) {
        ASSERT(RunLoop::isMain());
        if (!weakThis)
            return;
        weakThis->continueAddDeviceAfterGetInfo(ptr, WTFMove(response));
    });
    auto addResult = m_drivers.add(WTFMove(driver));
    ASSERT_UNUSED(addResult, addResult.isNewEntry);
}

void HidService::continueAddDeviceAfterGetInfo(CtapHidDriver* ptr, Vector<uint8_t>&& response)
{
    std::unique_ptr<CtapHidDriver> driver = m_drivers.take(ptr);
    if (!driver || !observer())
        return;

    auto info = readCTAPGetInfoResponse(response);
    if (info && info->versions().find(ProtocolVersion::kCtap) != info->versions().end()) {
        observer()->authenticatorAdded(CtapHidAuthenticator::create(WTFMove(driver), WTFMove(*info)));
        return;
    }
    // FIXME(191535): Support U2F authenticators.
    LOG_ERROR("Couldn't parse a ctap get info response.");
}

} // namespace WebKit

#endif // ENABLE(WEB_AUTHN) && PLATFORM(MAC)
