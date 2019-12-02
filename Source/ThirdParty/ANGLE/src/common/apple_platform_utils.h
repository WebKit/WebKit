//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// apple_platform_utils.h: Common utilities for Apple platforms.

#ifndef COMMON_APPLE_PLATFORM_UTILS_H_
#define COMMON_APPLE_PLATFORM_UTILS_H_

#include <TargetConditionals.h>

// These are macros for substitution of Apple specific directive @available:

// TARGET_OS_MACCATALYST only available in MacSDK 10.15

// ANGLE_APPLE_AVAILABLE_XCI: check if either of the 3 platforms (OSX/Catalyst/iOS) min verions is
// available:
#if TARGET_OS_MACCATALYST
#    define ANGLE_APPLE_AVAILABLE_XCI(macVer, macCatalystVer, iOSVer) \
        @available(macOS macVer, macCatalyst macCatalystVer, iOS iOSVer, *)
// ANGLE_APPLE_AVAILABLE_XC: check if either of the 2 platforms (OSX/Catalyst) min verions is
// available:
#    define ANGLE_APPLE_AVAILABLE_XC(macVer, macCatalystVer) \
        @available(macOS macVer, macCatalyst macCatalystVer, *)
#else
#    define ANGLE_APPLE_AVAILABLE_XCI(macVer, macCatalystVer, iOSVer) \
        ANGLE_APPLE_AVAILABLE_XI(macVer, iOSVer)
// ANGLE_APPLE_AVAILABLE_XC: check if either of the 2 platforms (OSX/Catalyst) min verions is
// available:
#    define ANGLE_APPLE_AVAILABLE_XC(macVer, macCatalystVer) @available(macOS macVer, *)
#endif

// ANGLE_APPLE_AVAILABLE_XI: check if either of the 2 platforms (OSX/iOS) min verions is available:
#define ANGLE_APPLE_AVAILABLE_XI(macVer, iOSVer) @available(macOS macVer, iOS iOSVer, *)

#endif
