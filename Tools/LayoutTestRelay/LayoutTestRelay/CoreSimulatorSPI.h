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

#if __has_include(<CoreSimulator/CoreSimulator.h>)

#define __coresim_unavailable_msg(msg)
#import <CoreSimulator/CoreSimulator.h>

#else

#import <Foundation/Foundation.h>

#define kSimDeviceLaunchApplicationArguments @"arguments"
#define kSimDeviceLaunchApplicationEnvironment @"environment"

typedef NS_ENUM(NSUInteger, SimDeviceState) {
    SimDeviceStateCreating = 0,
};

@interface SimDeviceType : NSObject
+ (NSDictionary *)supportedDeviceTypesByIdentifier;
@property (readonly, copy) NSString *identifier;
@end

@interface SimRuntime : NSObject
+ (NSDictionary *)supportedRuntimesByIdentifier;
@end

@interface SimDevice : NSObject
- (BOOL)installApplication:(NSURL *)installURL withOptions:(NSDictionary *)options error:(NSError **)error;
- (pid_t)launchApplicationWithID:(NSString *)bundleID options:(NSDictionary *)options error:(NSError **)error;
@property (readonly, retain) SimDeviceType *deviceType;
@property (readonly, retain) SimRuntime *runtime;
@property (readonly, assign) SimDeviceState state;
@property (readonly, copy) NSString *name;
@end

@interface SimDeviceSet : NSObject
+ (SimDeviceSet *)defaultSet;
- (SimDevice *)createDeviceWithType:(SimDeviceType *)deviceType runtime:(SimRuntime *)runtime name:(NSString *)name error:(NSError **)error;
@property (readonly, copy) NSArray *devices;

@end

#endif
