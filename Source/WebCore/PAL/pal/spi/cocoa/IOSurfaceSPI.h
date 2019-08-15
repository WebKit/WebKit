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

#if HAVE(IOSURFACE)

#if PLATFORM(MAC) || USE(APPLE_INTERNAL_SDK)

#include <IOSurface/IOSurface.h>

#else

#include <CoreFoundation/CFBase.h>
#include <mach/mach_port.h>
#include <pal/spi/cocoa/IOReturnSPI.h>
#include <pal/spi/cocoa/IOTypesSPI.h>

typedef struct __IOSurface *IOSurfaceRef;

#if PLATFORM(IOS_FAMILY) && __IPHONE_OS_VERSION_MIN_REQUIRED < 110000
enum {
    kIOSurfaceLockReadOnly  = 0x00000001,
};
#endif

#endif

WTF_EXTERN_C_BEGIN

extern const CFStringRef kIOSurfaceAllocSize;
extern const CFStringRef kIOSurfaceBytesPerElement;
extern const CFStringRef kIOSurfaceBytesPerRow;
extern const CFStringRef kIOSurfaceCacheMode;
extern const CFStringRef kIOSurfaceColorSpace;
extern const CFStringRef kIOSurfaceHeight;
extern const CFStringRef kIOSurfacePixelFormat;
extern const CFStringRef kIOSurfaceWidth;
extern const CFStringRef kIOSurfaceElementWidth;
extern const CFStringRef kIOSurfaceElementHeight;
extern const CFStringRef kIOSurfacePlaneWidth;
extern const CFStringRef kIOSurfacePlaneHeight;
extern const CFStringRef kIOSurfacePlaneBytesPerRow;
extern const CFStringRef kIOSurfacePlaneOffset;
extern const CFStringRef kIOSurfacePlaneSize;
extern const CFStringRef kIOSurfacePlaneInfo;

size_t IOSurfaceAlignProperty(CFStringRef property, size_t value);
IOSurfaceRef IOSurfaceCreate(CFDictionaryRef properties);
mach_port_t IOSurfaceCreateMachPort(IOSurfaceRef buffer);
size_t IOSurfaceGetAllocSize(IOSurfaceRef buffer);
void *IOSurfaceGetBaseAddress(IOSurfaceRef buffer);
size_t IOSurfaceGetBytesPerRow(IOSurfaceRef buffer);
size_t IOSurfaceGetHeight(IOSurfaceRef buffer);
size_t IOSurfaceGetPropertyMaximum(CFStringRef property);
size_t IOSurfaceGetWidth(IOSurfaceRef buffer);
OSType IOSurfaceGetPixelFormat(IOSurfaceRef buffer);
Boolean IOSurfaceIsInUse(IOSurfaceRef buffer);
IOReturn IOSurfaceLock(IOSurfaceRef buffer, uint32_t options, uint32_t *seed);
IOSurfaceRef IOSurfaceLookupFromMachPort(mach_port_t);
IOReturn IOSurfaceUnlock(IOSurfaceRef buffer, uint32_t options, uint32_t *seed);
size_t IOSurfaceGetWidthOfPlane(IOSurfaceRef buffer, size_t planeIndex);
size_t IOSurfaceGetHeightOfPlane(IOSurfaceRef buffer, size_t planeIndex);

WTF_EXTERN_C_END

#if USE(APPLE_INTERNAL_SDK)

#import <IOSurface/IOSurfacePrivate.h>

#else

#if (PLATFORM(IOS_FAMILY) && __IPHONE_OS_VERSION_MIN_REQUIRED >= 110000) || PLATFORM(MAC)

#import <IOSurface/IOSurfaceTypes.h>

#else
enum {
    kIOSurfacePurgeableNonVolatile = 0,
    kIOSurfacePurgeableVolatile = 1,
    kIOSurfacePurgeableEmpty = 2,
    kIOSurfacePurgeableKeepCurrent = 3,
};
#endif

#endif

WTF_EXTERN_C_BEGIN

IOReturn IOSurfaceSetPurgeable(IOSurfaceRef buffer, uint32_t newState, uint32_t *oldState);

WTF_EXTERN_C_END

#if HAVE(IOSURFACE_ACCELERATOR)
#if USE(APPLE_INTERNAL_SDK)

#import <IOSurfaceAccelerator/IOSurfaceAccelerator.h>

#else

#if __IPHONE_OS_VERSION_MIN_REQUIRED < 110000
typedef uint32_t IOSurfaceID;
#endif

typedef struct __IOSurfaceAccelerator *IOSurfaceAcceleratorRef;

WTF_EXTERN_C_BEGIN

extern const CFStringRef kIOSurfaceAcceleratorUnwireSurfaceKey;

IOReturn IOSurfaceAcceleratorCreate(CFAllocatorRef, CFDictionaryRef properties, IOSurfaceAcceleratorRef* acceleratorOut);
CFRunLoopSourceRef IOSurfaceAcceleratorGetRunLoopSource(IOSurfaceAcceleratorRef);

typedef void (*IOSurfaceAcceleratorCompletionCallback)(void* completionRefCon, IOReturn status, void* completionRefCon2);

typedef struct IOSurfaceAcceleratorCompletion {
    IOSurfaceAcceleratorCompletionCallback completionCallback;
    void* completionRefCon;
    void* completionRefCon2;
} IOSurfaceAcceleratorCompletion;

IOReturn IOSurfaceAcceleratorTransformSurface(IOSurfaceAcceleratorRef, IOSurfaceRef sourceBuffer, IOSurfaceRef destinationBuffer, CFDictionaryRef options, void* pCropRectangles, IOSurfaceAcceleratorCompletion* pCompletion, void* pSwap, uint32_t* pCommandID);

WTF_EXTERN_C_END

#endif // USE(APPLE_INTERNAL_SDK)
#endif // HAVE(IOSURFACE_ACCELERATOR)

#endif // HAVE(IOSURFACE)
