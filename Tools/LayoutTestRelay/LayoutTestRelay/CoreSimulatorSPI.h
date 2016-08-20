/*
 * Copyright (C) 2015, 2016 Apple Inc. All rights reserved.
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

/* FIXME: Remove the below #define once we require Xcode 7.3 with iOS 9.3 SDK or newer. */
#define __coresim_unavailable_msg(msg)
#import <CoreSimulator/CoreSimulator.h>

/* FIXME: Always use SimServiceContext once we require Xcode 7.3 with iOS 9.3 SDK or newer. */
#define USE_SIM_SERVICE_CONTEXT defined(CORESIM_API_MAX_ALLOWED)

#else

#import <Foundation/Foundation.h>

#define USE_SIM_SERVICE_CONTEXT 1

#define kSimDeviceLaunchApplicationArguments @"arguments"
#define kSimDeviceLaunchApplicationEnvironment @"environment"

@interface SimDevice : NSObject
- (BOOL)installApplication:(NSURL *)installURL withOptions:(NSDictionary *)options error:(NSError * __autoreleasing *)error;
- (pid_t)launchApplicationWithID:(NSString *)bundleID options:(NSDictionary *)options error:(NSError * __autoreleasing *)error;
@end

@interface SimDeviceSet : NSObject
@property (readonly, copy) NSDictionary *devicesByUDID;
@end

@interface SimServiceContext : NSObject
+(SimServiceContext *)sharedServiceContextForDeveloperDir:(NSString *)developerDir error:(NSError * __autoreleasing *)error;
-(SimDeviceSet *)defaultDeviceSetWithError:(NSError * __autoreleasing *)error;
@end

#endif
