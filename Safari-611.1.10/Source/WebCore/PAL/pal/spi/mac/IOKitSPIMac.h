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

#pragma once

#if PLATFORM(MAC)
#if USE(APPLE_INTERNAL_SDK)

#import <IOKit/hid/IOHIDEventSystemClient.h>

#else

#define kIOHIDVendorIDKey "VendorID"
#define kIOHIDProductIDKey "ProductID"

WTF_EXTERN_C_BEGIN
typedef struct CF_BRIDGED_TYPE(id) __IOHIDServiceClient * IOHIDServiceClientRef;
typedef struct CF_BRIDGED_TYPE(id) __IOHIDEventSystemClient * IOHIDEventSystemClientRef;
typedef void (^IOHIDServiceClientBlock)(void *, void *, IOHIDServiceClientRef);

typedef CF_ENUM(int, IOHIDEventSystemClientType)
{
    kIOHIDEventSystemClientTypeAdmin,
    kIOHIDEventSystemClientTypeMonitor,
    kIOHIDEventSystemClientTypePassive,
    kIOHIDEventSystemClientTypeRateControlled,
    kIOHIDEventSystemClientTypeSimple
};

IOHIDEventSystemClientRef IOHIDEventSystemClientCreateWithType(CFAllocatorRef, IOHIDEventSystemClientType, CFDictionaryRef);
IOHIDEventSystemClientRef IOHIDEventSystemClientCreate(CFAllocatorRef);
void IOHIDEventSystemClientSetMatching(IOHIDEventSystemClientRef, CFDictionaryRef);
void IOHIDEventSystemClientSetMatchingMultiple(IOHIDEventSystemClientRef, CFArrayRef);
IOHIDServiceClientRef IOHIDEventSystemClientCopyServiceForRegistryID(IOHIDEventSystemClientRef, uint64_t registryID);
void IOHIDEventSystemClientRegisterDeviceMatchingBlock(IOHIDEventSystemClientRef, IOHIDServiceClientBlock, void *, void *);
void IOHIDEventSystemClientUnregisterDeviceMatchingBlock(IOHIDEventSystemClientRef);
void IOHIDEventSystemClientScheduleWithDispatchQueue(IOHIDEventSystemClientRef, dispatch_queue_t);

CFTypeRef IOHIDServiceClientCopyProperty(IOHIDServiceClientRef service, CFStringRef key);

WTF_EXTERN_C_END

#endif // USE(APPLE_INTERNAL_SDK)
#endif // PLATFORM(MAC)
