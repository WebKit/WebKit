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
    : FidoService(observer)
{
    m_manager = adoptCF(IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone));
    NSDictionary *matchingDictionary = @{
        @kIOHIDPrimaryUsagePageKey: adoptNS([NSNumber numberWithInt:kCtapHidUsagePage]).get(),
        @kIOHIDPrimaryUsageKey: adoptNS([NSNumber numberWithInt:kCtapHidUsage]).get()
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
    getInfo(WTF::makeUnique<CtapHidDriver>(createHidConnection(device)));
}

} // namespace WebKit

#endif // ENABLE(WEB_AUTHN)
