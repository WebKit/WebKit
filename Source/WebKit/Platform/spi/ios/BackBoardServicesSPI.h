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

#import <BackBoardServices/BKSAnimationFence.h>
#import <BackBoardServices/BKSAnimationFence_Private.h>
#import <BackBoardServices/BKSMousePointerService.h>

#else

#import "BaseBoardSPI.h"

@interface BKSAnimationFenceHandle : NSObject
- (mach_port_t)CAPort;
@end

@class BKSMousePointerDevice;

@protocol BKSMousePointerDeviceObserver <NSObject>
@optional
- (void)mousePointerDevicesDidConnect:(NSSet<BKSMousePointerDevice *> *)mousePointerDevices;
- (void)mousePointerDevicesDidDisconnect:(NSSet<BKSMousePointerDevice *> *)mousePointerDevices;
@end

@interface BKSMousePointerService : NSObject
+ (BKSMousePointerService *)sharedInstance;
- (id<BSInvalidatable>)addPointerDeviceObserver:(id<BKSMousePointerDeviceObserver>)observer;
@end

#endif // USE(APPLE_INTERNAL_SDK)
