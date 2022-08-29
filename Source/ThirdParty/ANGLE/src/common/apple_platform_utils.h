//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// apple_platform_utils.h: Common utilities for Apple platforms.

#ifndef COMMON_APPLE_PLATFORM_UTILS_H_
#define COMMON_APPLE_PLATFORM_UTILS_H_

#include "common/platform.h"

// These are macros for substitution of Apple specific directive @available:

// TARGET_OS_MACCATALYST only available in MacSDK 10.15

#if TARGET_OS_MACCATALYST
// ANGLE_APPLE_AVAILABLE_XCI: check if either of the 3 platforms (OSX/Catalyst/iOS) min verions is
// available:
#    define ANGLE_APPLE_AVAILABLE_XCI(macVer, macCatalystVer, iOSVer) \
        @available(macOS macVer, macCatalyst macCatalystVer, iOS iOSVer, *)
// ANGLE_APPLE_AVAILABLE_XC: check if either of the 2 platforms (OSX/Catalyst) min verions is
// available:
#    define ANGLE_APPLE_AVAILABLE_XC(macVer, macCatalystVer) \
        @available(macOS macVer, macCatalyst macCatalystVer, *)
// ANGLE_APPLE_AVAILABLE_CI: check if either of the 2 platforms (Catalyst/iOS) min verions is
// available:
#    define ANGLE_APPLE_AVAILABLE_CI(macCatalystVer, iOSVer) \
        @available(macCatalyst macCatalystVer, iOS iOSVer, *)
#else
#    define ANGLE_APPLE_AVAILABLE_XCI(macVer, macCatalystVer, iOSVer) \
        ANGLE_APPLE_AVAILABLE_XI(macVer, iOSVer)

#    define ANGLE_APPLE_AVAILABLE_XC(macVer, macCatalystVer) @available(macOS macVer, *)
#    define ANGLE_APPLE_AVAILABLE_CI(macCatalystVer, iOSVer) @available(iOS iOSVer, tvOS iOSVer, *)
#endif

// ANGLE_APPLE_AVAILABLE_XI: check if either of the 2 platforms (OSX/iOS) min verions is available:
#define ANGLE_APPLE_AVAILABLE_XI(macVer, iOSVer) \
    @available(macOS macVer, iOS iOSVer, tvOS iOSVer, *)

// ANGLE_APPLE_AVAILABLE_I: check if a particular iOS version is available
#define ANGLE_APPLE_AVAILABLE_I(iOSVer) @available(iOS iOSVer, tvOS iOSVer, *)

#if TARGET_OS_IPHONE
#    if !defined(__IPHONE_11_0)
#        define __IPHONE_11_0 110000
#    endif
#    if !defined(ANGLE_IOS_DEPLOY_TARGET)
#        define ANGLE_IOS_DEPLOY_TARGET __IPHONE_11_0
#    endif
#    if !defined(__IPHONE_OS_VERSION_MAX_ALLOWED)
#        define __IPHONE_OS_VERSION_MAX_ALLOWED __IPHONE_11_0
#    endif
#    if !defined(__TV_OS_VERSION_MAX_ALLOWED)
#        define __TV_OS_VERSION_MAX_ALLOWED __IPHONE_11_0
#    endif
#endif

#if !defined(TARGET_OS_MACCATALYST)
#    define TARGET_OS_MACCATALYST 0
#endif

#if defined(__ARM_ARCH)
#    define ANGLE_APPLE_IS_ARM (__ARM_ARCH != 0)
#else
#    define ANGLE_APPLE_IS_ARM 0
#endif

#define ANGLE_APPLE_OBJC_SCOPE @autoreleasepool

#if !__has_feature(objc_arc)
#    define ANGLE_APPLE_AUTORELEASE autorelease
#    define ANGLE_APPLE_RETAIN retain
#    define ANGLE_APPLE_RELEASE release
#else
#    define ANGLE_APPLE_AUTORELEASE self
#    define ANGLE_APPLE_RETAIN self
#    define ANGLE_APPLE_RELEASE self
#endif

#define ANGLE_APPLE_UNUSED __attribute__((unused))

namespace angle
{
bool IsMetalRendererAvailable();
}

#endif
