/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#ifndef IOKitSPI_h
#define IOKitSPI_h

#import <wtf/Platform.h>

#if PLATFORM(IOS)

#if USE(APPLE_INTERNAL_SDK)

#import <IOKit/hid/IOHIDEvent.h>
#import <IOKit/hid/IOHIDEventSystemClient.h>
#import <IOKit/hid/IOHIDUsageTables.h>

#else

WTF_EXTERN_C_BEGIN

#ifdef __LP64__
typedef double IOHIDFloat;
#else
typedef float IOHIDFloat;
#endif

enum {
    kIOHIDEventOptionNone = 0,
};

typedef UInt32 IOOptionBits;
typedef uint32_t IOHIDEventOptionBits;
typedef uint32_t IOHIDEventField;

typedef struct __IOHIDEventSystemClient * IOHIDEventSystemClientRef;
typedef struct __IOHIDEvent * IOHIDEventRef;

#define IOHIDEventFieldBase(type) (type << 16)

enum {
    kHIDPage_VendorDefinedStart     = 0xFF00
};

enum {
    kIOHIDDigitizerEventRange       = 1<<0,
    kIOHIDDigitizerEventTouch       = 1<<1,
    kIOHIDDigitizerEventPosition    = 1<<2,
    kIOHIDDigitizerEventIdentity    = 1<<5,
    kIOHIDDigitizerEventAttribute   = 1<<6,
    kIOHIDDigitizerEventCancel      = 1<<7,
    kIOHIDDigitizerEventStart       = 1<<8,
};
typedef uint32_t IOHIDDigitizerEventMask;

enum {
    kIOHIDEventTypeNULL,
    kIOHIDEventTypeVendorDefined,
    kIOHIDEventTypeKeyboard = 3,
    kIOHIDEventTypeDigitizer = 11,
};
typedef uint32_t IOHIDEventType;

enum {
    kIOHIDEventFieldVendorDefinedUsagePage = IOHIDEventFieldBase(kIOHIDEventTypeVendorDefined),
    kIOHIDEventFieldVendorDefinedReserved,
    kIOHIDEventFieldVendorDefinedReserved1,
    kIOHIDEventFieldVendorDefinedDataLength,
    kIOHIDEventFieldVendorDefinedData
};

enum {
    kIOHIDEventFieldDigitizerX = IOHIDEventFieldBase(kIOHIDEventTypeDigitizer),
    kIOHIDEventFieldDigitizerY,
    kIOHIDEventFieldDigitizerMajorRadius = kIOHIDEventFieldDigitizerX + 20,
    kIOHIDEventFieldDigitizerMinorRadius,
    kIOHIDEventFieldDigitizerIsDisplayIntegrated = kIOHIDEventFieldDigitizerMajorRadius + 5,
};

enum {
    kIOHIDDigitizerTransducerTypeHand = 3
};
typedef uint32_t IOHIDDigitizerTransducerType;

IOHIDEventRef IOHIDEventCreateDigitizerEvent(CFAllocatorRef, uint64_t, IOHIDDigitizerTransducerType, uint32_t, uint32_t, IOHIDDigitizerEventMask, uint32_t, IOHIDFloat, IOHIDFloat, IOHIDFloat, IOHIDFloat, IOHIDFloat, boolean_t, boolean_t, IOOptionBits);

IOHIDEventRef IOHIDEventCreateDigitizerFingerEvent(CFAllocatorRef, uint64_t, uint32_t, uint32_t, IOHIDDigitizerEventMask, IOHIDFloat, IOHIDFloat, IOHIDFloat, IOHIDFloat, IOHIDFloat, boolean_t, boolean_t, IOHIDEventOptionBits);

IOHIDEventRef IOHIDEventCreateForceEvent(CFAllocatorRef, uint64_t, uint32_t, IOHIDFloat, uint32_t, IOHIDFloat, IOHIDEventOptionBits);

IOHIDEventRef IOHIDEventCreateKeyboardEvent(CFAllocatorRef, uint64_t, uint32_t, uint32_t, boolean_t, IOOptionBits);

IOHIDEventRef IOHIDEventCreateVendorDefinedEvent(CFAllocatorRef, uint64_t, uint32_t, uint32_t, uint32_t, uint8_t*, CFIndex, IOHIDEventOptionBits);

IOHIDEventType IOHIDEventGetType(IOHIDEventRef);

void IOHIDEventSetFloatValue(IOHIDEventRef, IOHIDEventField, IOHIDFloat);

CFIndex IOHIDEventGetIntegerValue(IOHIDEventRef, IOHIDEventField);
void IOHIDEventSetIntegerValue(IOHIDEventRef, IOHIDEventField, CFIndex);

void IOHIDEventAppendEvent(IOHIDEventRef, IOHIDEventRef, IOOptionBits);

IOHIDEventSystemClientRef IOHIDEventSystemClientCreate(CFAllocatorRef);

#define kGSEventPathInfoInRange (1 << 0)
#define kGSEventPathInfoInTouch (1 << 1)

WTF_EXTERN_C_END

#endif // USE(APPLE_INTERNAL_SDK)

#endif // PLATFORM(IOS)

#endif // IOKitSPI_h
