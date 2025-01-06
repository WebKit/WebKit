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

#if ENABLE(WEB_AUTHN)

#import "CtapHidDriver.h"
#import "HidConnection.h"

namespace WebKit {
using namespace fido;

#if HAVE(SECURITY_KEY_API)
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
#endif // HAVE(SECURITY_KEY_API)

Ref<HidService> HidService::create(AuthenticatorTransportServiceObserver& observer)
{
    return adoptRef(*new HidService(observer));
}

HidService::HidService(AuthenticatorTransportServiceObserver& observer)
    : FidoService(observer)
{
#if HAVE(SECURITY_KEY_API)
    m_manager = adoptCF(IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone));
    NSDictionary *matchingDictionary = @{
        @kIOHIDPrimaryUsagePageKey: @(kCtapHidUsagePage),
        @kIOHIDPrimaryUsageKey: @(kCtapHidUsage),
    };
    IOHIDManagerSetDeviceMatching(m_manager.get(), (__bridge CFDictionaryRef)matchingDictionary);
    IOHIDManagerRegisterDeviceMatchingCallback(m_manager.get(), deviceAddedCallback, this);
    IOHIDManagerRegisterDeviceRemovalCallback(m_manager.get(), deviceRemovedCallback, this);
#endif
}

HidService::~HidService()
{
#if HAVE(SECURITY_KEY_API)
    IOHIDManagerUnscheduleFromRunLoop(m_manager.get(), CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
    IOHIDManagerClose(m_manager.get(), kIOHIDOptionsTypeNone);
#endif
}

void HidService::startDiscoveryInternal()
{
    platformStartDiscovery();
}

void HidService::platformStartDiscovery()
{
#if HAVE(SECURITY_KEY_API)
    IOHIDManagerScheduleWithRunLoop(m_manager.get(), CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
    IOHIDManagerOpen(m_manager.get(), kIOHIDOptionsTypeNone);
#endif
}

Ref<HidConnection> HidService::createHidConnection(IOHIDDeviceRef device) const
{
    return HidConnection::create(device);
}

void HidService::deviceAdded(IOHIDDeviceRef device)
{
#if HAVE(SECURITY_KEY_API)
    getInfo(CtapHidDriver::create(createHidConnection(device)));
#endif
}

} // namespace WebKit

#endif // ENABLE(WEB_AUTHN)
