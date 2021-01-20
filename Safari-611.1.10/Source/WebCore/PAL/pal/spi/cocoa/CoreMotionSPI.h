/*
 * Copyright (C) 2015, 2020 Apple Inc. All rights reserved.
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

#if USE(APPLE_INTERNAL_SDK) || !PLATFORM(APPLETV)
#import <CoreMotion/CoreMotion.h>
#else

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN


typedef struct {
    double x;
    double y;
    double z;
} CMAcceleration;

typedef struct {
    double x;
    double y;
    double z;
} CMRotationRate;


@interface CMLogItem : NSObject <NSSecureCoding, NSCopying>
@end


@interface CMAttitude : NSObject <NSCopying, NSSecureCoding>
@property (readonly, nonatomic) double roll;
@property (readonly, nonatomic) double pitch;
@property (readonly, nonatomic) double yaw;
@end


@interface CMDeviceMotion : CMLogItem
@property (readonly, nonatomic) CMAttitude *attitude;
@property (readonly, nonatomic) CMRotationRate rotationRate;
@property (readonly, nonatomic) CMAcceleration gravity;
@property (readonly, nonatomic) CMAcceleration userAcceleration;
@end


@interface CMAccelerometerData : CMLogItem
@property (readonly, nonatomic) CMAcceleration acceleration;
@end


@interface CMMotionManager : NSObject
@property (assign, nonatomic) NSTimeInterval accelerometerUpdateInterval;
@property (readonly, nullable) CMAccelerometerData *accelerometerData;

- (void)startAccelerometerUpdates;
- (void)stopAccelerometerUpdates;

@property (assign, nonatomic) NSTimeInterval deviceMotionUpdateInterval;
@property (readonly, nonatomic, getter=isDeviceMotionAvailable) BOOL deviceMotionAvailable;
@property (readonly, nullable) CMDeviceMotion *deviceMotion;

- (void)startDeviceMotionUpdates;
- (void)stopDeviceMotionUpdates;
@end

NS_ASSUME_NONNULL_END

#endif // USE(APPLE_INTERNAL_SDK) || !PLATFORM(APPLETV)
