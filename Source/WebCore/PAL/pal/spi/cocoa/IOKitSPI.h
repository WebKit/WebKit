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

#pragma once

#if USE(APPLE_INTERNAL_SDK)

#import <IOKit/hid/IOHIDEvent.h>
#import <IOKit/hid/IOHIDEventData.h>
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
    kHIDPage_KeyboardOrKeypad       = 0x07,
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
    kIOHIDDigitizerEventEstimatedAltitude = 1<<28,
    kIOHIDDigitizerEventEstimatedAzimuth = 1<<29,
    kIOHIDDigitizerEventEstimatedPressure = 1<<30
};
typedef uint32_t IOHIDDigitizerEventMask;

enum {
    kIOHIDDigitizerEventUpdateAltitudeMask = 1<<28,
    kIOHIDDigitizerEventUpdateAzimuthMask = 1<<29,
    kIOHIDDigitizerEventUpdatePressureMask = 1<<30
};

enum {
    kIOHIDEventTypeNULL,
    kIOHIDEventTypeVendorDefined,
    kIOHIDEventTypeKeyboard = 3,
    kIOHIDEventTypeRotation = 5,
    kIOHIDEventTypeScroll = 6,
    kIOHIDEventTypeZoom = 8,
    kIOHIDEventTypeDigitizer = 11,
    kIOHIDEventTypeNavigationSwipe = 16,
    kIOHIDEventTypeForce = 32,

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
    kIOHIDTransducerRange               = 0x00010000,
    kIOHIDTransducerTouch               = 0x00020000,
    kIOHIDTransducerInvert              = 0x00040000,
    kIOHIDTransducerDisplayIntegrated   = 0x00080000
};

enum {
    kIOHIDDigitizerTransducerTypeStylus  = 0,
    kIOHIDDigitizerTransducerTypeFinger = 2,
    kIOHIDDigitizerTransducerTypeHand = 3
};
typedef uint32_t IOHIDDigitizerTransducerType;

enum {
    kIOHIDEventFieldDigitizerWillUpdateMask = 720924,
    kIOHIDEventFieldDigitizerDidUpdateMask = 720925
};

IOHIDEventRef IOHIDEventCreateDigitizerEvent(CFAllocatorRef, uint64_t, IOHIDDigitizerTransducerType, uint32_t, uint32_t, IOHIDDigitizerEventMask, uint32_t, IOHIDFloat, IOHIDFloat, IOHIDFloat, IOHIDFloat, IOHIDFloat, boolean_t, boolean_t, IOOptionBits);

IOHIDEventRef IOHIDEventCreateDigitizerFingerEvent(CFAllocatorRef, uint64_t, uint32_t, uint32_t, IOHIDDigitizerEventMask, IOHIDFloat, IOHIDFloat, IOHIDFloat, IOHIDFloat, IOHIDFloat, boolean_t, boolean_t, IOHIDEventOptionBits);

IOHIDEventRef IOHIDEventCreateForceEvent(CFAllocatorRef, uint64_t, uint32_t, IOHIDFloat, uint32_t, IOHIDFloat, IOHIDEventOptionBits);

IOHIDEventRef IOHIDEventCreateKeyboardEvent(CFAllocatorRef, uint64_t, uint32_t, uint32_t, boolean_t, IOOptionBits);

IOHIDEventRef IOHIDEventCreateVendorDefinedEvent(CFAllocatorRef, uint64_t, uint32_t, uint32_t, uint32_t, uint8_t*, CFIndex, IOHIDEventOptionBits);

IOHIDEventRef IOHIDEventCreateDigitizerStylusEventWithPolarOrientation(CFAllocatorRef, uint64_t, uint32_t, uint32_t, IOHIDDigitizerEventMask, uint32_t, IOHIDFloat, IOHIDFloat, IOHIDFloat, IOHIDFloat, IOHIDFloat, IOHIDFloat, IOHIDFloat, IOHIDFloat, boolean_t, boolean_t, IOHIDEventOptionBits);

IOHIDEventType IOHIDEventGetType(IOHIDEventRef);

void IOHIDEventSetFloatValue(IOHIDEventRef, IOHIDEventField, IOHIDFloat);

CFIndex IOHIDEventGetIntegerValue(IOHIDEventRef, IOHIDEventField);
void IOHIDEventSetIntegerValue(IOHIDEventRef, IOHIDEventField, CFIndex);

void IOHIDEventAppendEvent(IOHIDEventRef, IOHIDEventRef, IOOptionBits);

IOHIDEventSystemClientRef IOHIDEventSystemClientCreate(CFAllocatorRef);

#define kGSEventPathInfoInRange (1 << 0)
#define kGSEventPathInfoInTouch (1 << 1)

enum {
    kHIDUsage_KeyboardA = 0x04,
    kHIDUsage_Keyboard1 = 0x1E,
    kHIDUsage_Keyboard2 = 0x1F,
    kHIDUsage_Keyboard3 = 0x20,
    kHIDUsage_Keyboard4 = 0x21,
    kHIDUsage_Keyboard5 = 0x22,
    kHIDUsage_Keyboard6 = 0x23,
    kHIDUsage_Keyboard7 = 0x24,
    kHIDUsage_Keyboard8 = 0x25,
    kHIDUsage_Keyboard9 = 0x26,
    kHIDUsage_Keyboard0 = 0x27,
    kHIDUsage_KeyboardReturnOrEnter = 0x28,
    kHIDUsage_KeyboardEscape = 0x29,
    kHIDUsage_KeyboardDeleteOrBackspace = 0x2A,
    kHIDUsage_KeyboardTab = 0x2B,
    kHIDUsage_KeyboardSpacebar = 0x2C,
    kHIDUsage_KeyboardHyphen = 0x2D,
    kHIDUsage_KeyboardEqualSign = 0x2E,
    kHIDUsage_KeyboardOpenBracket = 0x2F,
    kHIDUsage_KeyboardCloseBracket = 0x30,
    kHIDUsage_KeyboardBackslash = 0x31,
    kHIDUsage_KeyboardSemicolon = 0x33,
    kHIDUsage_KeyboardQuote = 0x34,
    kHIDUsage_KeyboardGraveAccentAndTilde = 0x35,
    kHIDUsage_KeyboardComma = 0x36,
    kHIDUsage_KeyboardPeriod = 0x37,
    kHIDUsage_KeyboardSlash = 0x38,
    kHIDUsage_KeyboardCapsLock = 0x39,
    kHIDUsage_KeyboardF1 = 0x3A,
    kHIDUsage_KeyboardF12 = 0x45,
    kHIDUsage_KeyboardPrintScreen = 0x46,
    kHIDUsage_KeyboardInsert = 0x49,
    kHIDUsage_KeyboardHome = 0x4A,
    kHIDUsage_KeyboardPageUp = 0x4B,
    kHIDUsage_KeyboardDeleteForward = 0x4C,
    kHIDUsage_KeyboardEnd = 0x4D,
    kHIDUsage_KeyboardPageDown = 0x4E,
    kHIDUsage_KeyboardRightArrow = 0x4F,
    kHIDUsage_KeyboardLeftArrow = 0x50,
    kHIDUsage_KeyboardDownArrow = 0x51,
    kHIDUsage_KeyboardUpArrow = 0x52,
    kHIDUsage_KeypadNumLock = 0x53,
    kHIDUsage_KeyboardF13 = 0x68,
    kHIDUsage_KeyboardF24 = 0x73,
    kHIDUsage_KeyboardMenu = 0x76,
    kHIDUsage_KeyboardLeftControl = 0xE0,
    kHIDUsage_KeyboardLeftShift = 0xE1,
    kHIDUsage_KeyboardLeftAlt = 0xE2,
    kHIDUsage_KeyboardLeftGUI = 0xE3,
    kHIDUsage_KeyboardRightControl = 0xE4,
    kHIDUsage_KeyboardRightShift = 0xE5,
    kHIDUsage_KeyboardRightAlt = 0xE6,
    kHIDUsage_KeyboardRightGUI = 0xE7,
};

WTF_EXTERN_C_END

#endif // USE(APPLE_INTERNAL_SDK)
