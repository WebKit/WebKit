/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#import <wtf/Platform.h>

#if PLATFORM(IOS_FAMILY)

#import <UIKit/UIKit.h>

#if USE(APPLE_INTERNAL_SDK)

#import <UIKit/UIApplication_Private.h>
#import <UIKit/UIKeyboard.h>
#import <UIKit/UIView_Private.h>
#import <UIKit/UIWindow_Private.h>
#import <UIKit/UIDevice_Private.h>
#import <UIKit/UIScreen_Private.h>
#import <UIKit/_UIApplicationRotationFollowing.h>

#else

#import <pal/spi/cocoa/IOKitSPI.h>

@interface UIApplication ()
- (void)_enqueueHIDEvent:(IOHIDEventRef)event;
- (void)_handleHIDEvent:(IOHIDEventRef)event;
@end

@interface UIWindow ()
- (uint32_t)_contextId;
@end

// FIXME: https://bugs.webkit.org/show_bug.cgi?id=173341
#ifndef _WEBKIT_UIKITSPI_UIKEYBOARD
#define _WEBKIT_UIKITSPI_UIKEYBOARD 1
@interface UIKeyboard : UIView
@end
#endif

@interface UIKeyboard ()
+ (void)removeAllDynamicDictionaries;
+ (BOOL)isInHardwareKeyboardMode;
@end

@interface UIView ()
- (void)_removeAllAnimations:(BOOL)includeSubviews;
@end

@interface UIDevice ()
- (void)setOrientation:(UIDeviceOrientation)orientation animated:(BOOL)animated;
 @end

@interface UIScreen ()
- (void)_setScale:(CGFloat)scale;
@end

@interface UIApplicationRotationFollowingWindow : UIWindow
@end

@interface UIApplicationRotationFollowingController : UIViewController
@end

@interface UIApplicationRotationFollowingControllerNoTouches : UIApplicationRotationFollowingController
@end
 
#endif // USE(APPLE_INTERNAL_SDK)

#if __IPHONE_OS_VERSION_MIN_REQUIRED >= 110000
@interface UIView ()
- (void)_updateSafeAreaInsets;
@end
#endif

#endif // PLATFORM(IOS_FAMILY)

