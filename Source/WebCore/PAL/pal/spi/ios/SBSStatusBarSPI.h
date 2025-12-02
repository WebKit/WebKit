/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

DECLARE_SYSTEM_HEADER

#import <wtf/Platform.h>

#if PLATFORM(IOS_FAMILY)

#if USE(APPLE_INTERNAL_SDK)

#import <SpringBoardServices/SBSStatusBarStyleOverridesAssertion.h>
#import <SpringBoardServices/SBSStatusBarStyleOverridesCoordinator.h>

#else

#import <Foundation/Foundation.h>

typedef enum _UIStatusBarStyleOverrides : uint32_t {
    UIStatusBarStyleOverrideWebRTCAudioCapture  = 1 << 24,
    UIStatusBarStyleOverrideWebRTCCapture       = 1 << 25
} UIStatusBarStyleOverrides;

typedef void (^SBSStatusBarStyleOverridesAssertionAcquisitionHandler)(BOOL acquired);

@interface SBSStatusBarStyleOverridesAssertion : NSObject
@property (nonatomic, readonly) UIStatusBarStyleOverrides statusBarStyleOverrides;
@property (nonatomic, readwrite, copy) NSString *statusString;
+ (instancetype)assertionWithStatusBarStyleOverrides:(UIStatusBarStyleOverrides)overrides forPID:(pid_t)pid exclusive:(BOOL)exclusive showsWhenForeground:(BOOL)showsWhenForeground;
- (instancetype)initWithStatusBarStyleOverrides:(UIStatusBarStyleOverrides)overrides forPID:(pid_t)pid exclusive:(BOOL)exclusive showsWhenForeground:(BOOL)showsWhenForeground;
- (void)acquireWithHandler:(SBSStatusBarStyleOverridesAssertionAcquisitionHandler)handler invalidationHandler:(void (^)(void))invalidationHandler;
- (void)invalidate;
@end

@protocol SBSStatusBarStyleOverridesCoordinatorDelegate;
@protocol SBSStatusBarTapContext;

@interface SBSStatusBarStyleOverridesCoordinator : NSObject
@property (nonatomic, weak, readwrite) id <SBSStatusBarStyleOverridesCoordinatorDelegate> delegate;
@property (nonatomic, readonly) UIStatusBarStyleOverrides styleOverrides;
- (void)setRegisteredStyleOverrides:(UIStatusBarStyleOverrides)styleOverrides reply:(void(^)(NSError *error))reply;
@end

@protocol SBSStatusBarStyleOverridesCoordinatorDelegate <NSObject>
@optional
- (BOOL)statusBarCoordinator:(SBSStatusBarStyleOverridesCoordinator *)coordinator receivedTapWithContext:(id<SBSStatusBarTapContext>)tapContext completionBlock:(void (^)(void))completion;
@required
- (void)statusBarCoordinator:(SBSStatusBarStyleOverridesCoordinator *)coordinator invalidatedRegistrationWithError:(NSError *)error;
@end

@protocol SBSStatusBarTapContext
@property (nonatomic, readonly) UIStatusBarStyleOverrides styleOverride;
@end

#endif // USE(APPLE_INTERNAL_SDK)

#endif // PLATFORM(IOS_FAMILY)
