/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#import <Availability.h>
#import <TargetConditionals.h>

#if !defined(WK_API_ENABLED)
#if TARGET_OS_IPHONE
#define WK_API_ENABLED 1
#else
#define WK_API_ENABLED (defined(__clang__) && defined(__APPLE__) && !defined(__i386__))
#endif
#endif

#ifdef __cplusplus
#define WK_EXTERN extern "C" __attribute__((visibility ("default")))
#else
#define WK_EXTERN extern __attribute__((visibility ("default")))
#endif

#ifndef WK_API_AVAILABILITY_ENABLED
#define WK_AVAILABLE(_mac, _ios)
#define WK_CLASS_AVAILABLE(_mac, _ios) __attribute__((visibility ("default")))
#define WK_ENUM_AVAILABLE(_mac, _ios)
#define WK_ENUM_AVAILABLE_IOS(_ios)

#if defined(__MAC_OS_X_VERSION_MIN_REQUIRED) && __MAC_OS_X_VERSION_MIN_REQUIRED <= 1090

#define WK_DESIGNATED_INITIALIZER
#define WK_UNAVAILABLE

#ifdef __OBJC__
#import <Foundation/Foundation.h>
typedef NSUInteger NSEventModifierFlags;

#if __MAC_OS_X_VERSION_MAX_ALLOWED < 1090
typedef NSInteger NSURLSessionAuthChallengeDisposition;
#endif

#endif

#else

#define WK_DESIGNATED_INITIALIZER NS_DESIGNATED_INITIALIZER
#define WK_UNAVAILABLE NS_UNAVAILABLE

#endif

#endif
