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

#if USE(APPLE_INTERNAL_SDK)

#import <Metal/MTLTexture_Private.h>
#import <Metal/MetalPrivate.h>

#else

#import <Foundation/NSObject.h>

typedef struct __IOSurface *IOSurfaceRef;

@protocol MTLDeviceSPI <MTLDevice>
- (NSString*)vendorName;
- (NSString*)familyName;
- (NSString*)productName;
- (id <MTLSharedEvent>)newSharedEventWithMachPort:(mach_port_t)machPort;
@end

@interface _MTLDevice : NSObject
- (void)_purgeDevice;
@end

@interface MTLSharedEventHandle(Private)
- (mach_port_t)eventPort;
@end

#if !PLATFORM(IOS_FAMILY_SIMULATOR)
@interface MTLSharedTextureHandle(Private)
- (instancetype)initWithIOSurface:(IOSurfaceRef)ioSurface label:(NSString*)label;
@end
#endif

#endif
