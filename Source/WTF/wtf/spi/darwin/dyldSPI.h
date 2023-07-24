/*
 * Copyright (C) 2016-2017 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#pragma once

#if USE(APPLE_INTERNAL_SDK)

#include <mach-o/dyld_priv.h>

#ifndef DYLD_IOS_VERSION_11_0
#define DYLD_IOS_VERSION_11_0 0x000B0000
#endif

#ifndef DYLD_IOS_VERSION_11_3
#define DYLD_IOS_VERSION_11_3 0x000B0300
#endif

#ifndef DYLD_IOS_VERSION_12_0
#define DYLD_IOS_VERSION_12_0 0x000C0000
#endif

#ifndef DYLD_IOS_VERSION_12_2
#define DYLD_IOS_VERSION_12_2 0x000C0200
#endif

#ifndef DYLD_IOS_VERSION_13_0
#define DYLD_IOS_VERSION_13_0 0x000D0000
#endif

#ifndef DYLD_IOS_VERSION_13_2
#define DYLD_IOS_VERSION_13_2 0x000D0200
#endif

#ifndef DYLD_IOS_VERSION_13_4
#define DYLD_IOS_VERSION_13_4 0x000D0400
#endif

#ifndef DYLD_IOS_VERSION_14_0
#define DYLD_IOS_VERSION_14_0 0x000E0000
#endif

#ifndef DYLD_IOS_VERSION_14_2
#define DYLD_IOS_VERSION_14_2 0x000E0200
#endif

#ifndef DYLD_IOS_VERSION_14_5
#define DYLD_IOS_VERSION_14_5 0x000E0500
#endif

#ifndef DYLD_IOS_VERSION_15_0
#define DYLD_IOS_VERSION_15_0 0x000f0000
#endif

#ifndef DYLD_IOS_VERSION_15_4
#define DYLD_IOS_VERSION_15_4 0x000f0400
#endif

#ifndef DYLD_IOS_VERSION_16_0
#define DYLD_IOS_VERSION_16_0 0x00100000
#endif

#ifndef DYLD_IOS_VERSION_16_4
#define DYLD_IOS_VERSION_16_4 0x00100400
#endif

#ifndef DYLD_IOS_VERSION_17_0
#define DYLD_IOS_VERSION_17_0 0x00200000
#endif

#ifndef DYLD_MACOSX_VERSION_10_13
#define DYLD_MACOSX_VERSION_10_13 0x000A0D00
#endif

#ifndef DYLD_MACOSX_VERSION_10_14
#define DYLD_MACOSX_VERSION_10_14 0x000A0E00
#endif

#ifndef DYLD_MACOSX_VERSION_10_15
#define DYLD_MACOSX_VERSION_10_15 0x000A0F00
#endif

#ifndef DYLD_MACOSX_VERSION_10_15_1
#define DYLD_MACOSX_VERSION_10_15_1 0x000A0F01
#endif

#ifndef DYLD_MACOSX_VERSION_10_15_4
#define DYLD_MACOSX_VERSION_10_15_4 0x000A0F04
#endif

#ifndef DYLD_MACOSX_VERSION_10_16
#define DYLD_MACOSX_VERSION_10_16 0x000A1000
#endif

#ifndef DYLD_MACOSX_VERSION_11_3
#define DYLD_MACOSX_VERSION_11_3 0x000B0300
#endif

#ifndef DYLD_MACOSX_VERSION_12_00
#define DYLD_MACOSX_VERSION_12_00 0x000c0000
#endif

#ifndef DYLD_MACOSX_VERSION_12_3
#define DYLD_MACOSX_VERSION_12_3 0x000c0300
#endif

#ifndef DYLD_MACOSX_VERSION_13_0
#define DYLD_MACOSX_VERSION_13_0 0x000d0000
#endif

#ifndef DYLD_MACOSX_VERSION_13_3
#define DYLD_MACOSX_VERSION_13_3 0x000d0300
#endif

#ifndef DYLD_MACOSX_VERSION_14_0
#define DYLD_MACOSX_VERSION_14_0 0x000e0000
#endif

#else

typedef uint32_t dyld_platform_t;

typedef struct {
    dyld_platform_t platform;
    uint32_t version;
} dyld_build_version_t;

#define DYLD_IOS_VERSION_3_0 0x00030000
#define DYLD_IOS_VERSION_4_2 0x00040200
#define DYLD_IOS_VERSION_5_0 0x00050000
#define DYLD_IOS_VERSION_6_0 0x00060000
#define DYLD_IOS_VERSION_7_0 0x00070000
#define DYLD_IOS_VERSION_9_0 0x00090000
#define DYLD_IOS_VERSION_10_0 0x000A0000
#define DYLD_IOS_VERSION_11_0 0x000B0000
#define DYLD_IOS_VERSION_11_3 0x000B0300
#define DYLD_IOS_VERSION_12_0 0x000C0000
#define DYLD_IOS_VERSION_12_2 0x000C0200
#define DYLD_IOS_VERSION_13_0 0x000D0000
#define DYLD_IOS_VERSION_13_2 0x000D0200
#define DYLD_IOS_VERSION_13_4 0x000D0400
#define DYLD_IOS_VERSION_14_0 0x000E0000
#define DYLD_IOS_VERSION_14_2 0x000E0200
#define DYLD_IOS_VERSION_14_5 0x000E0500
#define DYLD_IOS_VERSION_15_0 0x000f0000
#define DYLD_IOS_VERSION_15_4 0x000f0400
#define DYLD_IOS_VERSION_16_0 0x00100000
#define DYLD_IOS_VERSION_16_4 0x00100400
#define DYLD_IOS_VERSION_17_0 0x00200000

#define DYLD_MACOSX_VERSION_10_10 0x000A0A00
#define DYLD_MACOSX_VERSION_10_11 0x000A0B00
#define DYLD_MACOSX_VERSION_10_12 0x000A0C00
#define DYLD_MACOSX_VERSION_10_13 0x000A0D00
#define DYLD_MACOSX_VERSION_10_13_4 0x000A0D04
#define DYLD_MACOSX_VERSION_10_14 0x000A0E00
#define DYLD_MACOSX_VERSION_10_14_4 0x000A0E04
#define DYLD_MACOSX_VERSION_10_15 0x000A0F00
#define DYLD_MACOSX_VERSION_10_15_1 0x000A0F01
#define DYLD_MACOSX_VERSION_10_15_4 0x000A0F04
#define DYLD_MACOSX_VERSION_10_16 0x000A1000
#define DYLD_MACOSX_VERSION_11_3 0x000B0300
#define DYLD_MACOSX_VERSION_12_00 0x000c0000
#define DYLD_MACOSX_VERSION_12_3 0x000c0300
#define DYLD_MACOSX_VERSION_13_0 0x000d0000
#define DYLD_MACOSX_VERSION_13_3 0x000d0300
#define DYLD_MACOSX_VERSION_14_0 0x000e0000

#endif

WTF_EXTERN_C_BEGIN

// Because it is not possible to forward-declare dyld_build_version_t values,
// we forward-declare placeholders for any missing definitions, and use their empty
// value to indicate that we need to fall back to antique single-platform SDK checks.

#ifndef dyld_fall_2014_os_versions
#define dyld_fall_2014_os_versions ({ (dyld_build_version_t) { 0, 0 }; })
#endif

#ifndef dyld_fall_2015_os_versions
#define dyld_fall_2015_os_versions ({ (dyld_build_version_t) { 0, 0 }; })
#endif

#ifndef dyld_fall_2016_os_versions
#define dyld_fall_2016_os_versions ({ (dyld_build_version_t) { 0, 0 }; })
#endif

#ifndef dyld_fall_2016_os_versions
#define dyld_fall_2016_os_versions ({ (dyld_build_version_t) { 0, 0 }; })
#endif

#ifndef dyld_fall_2017_os_versions
#define dyld_fall_2017_os_versions ({ (dyld_build_version_t) { 0, 0 }; })
#endif

#ifndef dyld_spring_2018_os_versions
#define dyld_spring_2018_os_versions ({ (dyld_build_version_t) { 0, 0 }; })
#endif

#ifndef dyld_fall_2018_os_versions
#define dyld_fall_2018_os_versions ({ (dyld_build_version_t) { 0, 0 }; })
#endif

#ifndef dyld_spring_2019_os_versions
#define dyld_spring_2019_os_versions ({ (dyld_build_version_t) { 0, 0 }; })
#endif

#ifndef dyld_fall_2019_os_versions
#define dyld_fall_2019_os_versions ({ (dyld_build_version_t) { 0, 0 }; })
#endif

#ifndef dyld_late_fall_2019_os_versions
#define dyld_late_fall_2019_os_versions ({ (dyld_build_version_t) { 0, 0 }; })
#endif

#ifndef dyld_spring_2020_os_versions
#define dyld_spring_2020_os_versions ({ (dyld_build_version_t) { 0, 0 }; })
#endif

#ifndef dyld_fall_2020_os_versions
#define dyld_fall_2020_os_versions ({ (dyld_build_version_t) { 0, 0 }; })
#endif

#ifndef dyld_late_fall_2020_os_versions
#define dyld_late_fall_2020_os_versions ({ (dyld_build_version_t) { 0, 0 }; })
#endif

#ifndef dyld_spring_2021_os_versions
#define dyld_spring_2021_os_versions ({ (dyld_build_version_t) { 0, 0 }; })
#endif

#ifndef dyld_fall_2021_os_versions
#define dyld_fall_2021_os_versions ({ (dyld_build_version_t) { 0, 0 }; })
#endif

#ifndef dyld_spring_2022_os_versions
#define dyld_spring_2022_os_versions ({ (dyld_build_version_t) { 0, 0 }; })
#endif

#ifndef dyld_fall_2022_os_versions
#define dyld_fall_2022_os_versions ({ (dyld_build_version_t) { 0, 0 }; })
#endif

#ifndef dyld_spring_2023_os_versions
#define dyld_spring_2023_os_versions ({ (dyld_build_version_t) { 0, 0 }; })
#endif

#ifndef dyld_fall_2023_os_versions
#define dyld_fall_2023_os_versions ({ (dyld_build_version_t) { 0, 0 }; })
#endif

uint32_t dyld_get_program_sdk_version();
bool dyld_program_sdk_at_least(dyld_build_version_t);
extern const char* dyld_shared_cache_file_path(void);
extern const struct mach_header* _dyld_get_dlopen_image_header(void* handle);
extern bool _dyld_get_image_uuid(const struct mach_header* mh, uuid_t);
extern bool _dyld_get_shared_cache_uuid(uuid_t);

WTF_EXTERN_C_END
