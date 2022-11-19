/*
 * Copyright (c) 2021 Apple Inc. All rights reserved.
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

#ifndef PAS_PLATFORM_H
#define PAS_PLATFORM_H

#ifdef __APPLE__
#include <Availability.h>
#include <AvailabilityMacros.h>
#include <TargetConditionals.h>
#endif

/* PAS_CPU() - the target CPU architecture */
#define PAS_CPU(FEATURE) (defined PAS_CPU_##FEATURE  && PAS_CPU_##FEATURE)

/* PAS_HAVE() - specific system features (headers, functions or similar) that are present or not */
#define PAS_HAVE(FEATURE) (defined PAS_HAVE_##FEATURE && PAS_HAVE_##FEATURE)

/* PAS_COMPILER() - the target compiler */
#define PAS_COMPILER(FEATURE) (defined PAS_COMPILER_##FEATURE  && PAS_COMPILER_##FEATURE)

#if defined(__clang__)
#define PAS_COMPILER_CLANG 1
#endif

#if defined(__GNUC__)
#define PAS_COMPILER_GCC_COMPATIBLE 1
#endif

#if PAS_COMPILER(GCC_COMPATIBLE) && !PAS_COMPILER(CLANG)
#define PAS_COMPILER_GCC 1
#endif

#if defined(_MSC_VER)
#define PAS_COMPILER_MSVC 1
#endif

/* PAS_IGNORE_WARNINGS */

/* Can't use WTF_CONCAT() and STRINGIZE() because they are defined in
 * StdLibExtras.h, which includes this file. */
#define PAS_COMPILER_CONCAT_I(a, b) a ## b
#define PAS_COMPILER_CONCAT(a, b) PAS_COMPILER_CONCAT_I(a, b)

#define PAS_COMPILER_STRINGIZE(exp) #exp

#define PAS_COMPILER_WARNING_NAME(warning) "-W" warning

#if PAS_COMPILER(GCC) || PAS_COMPILER(CLANG)
#define PAS_IGNORE_WARNINGS_BEGIN_COND(cond, compiler, warning) \
    _Pragma(PAS_COMPILER_STRINGIZE(compiler diagnostic push)) \
    PAS_COMPILER_CONCAT(PAS_IGNORE_WARNINGS_BEGIN_IMPL_, cond)(compiler, warning)

#define PAS_IGNORE_WARNINGS_BEGIN_IMPL_1(compiler, warning) \
    _Pragma(PAS_COMPILER_STRINGIZE(compiler diagnostic ignored warning))
#define PAS_IGNORE_WARNINGS_BEGIN_IMPL_0(compiler, warning)
#define PAS_IGNORE_WARNINGS_BEGIN_IMPL_(compiler, warning)


#define PAS_IGNORE_WARNINGS_END_IMPL(compiler) _Pragma(PAS_COMPILER_STRINGIZE(compiler diagnostic pop))

#if defined(__has_warning)
#define PAS__IGNORE_WARNINGS_BEGIN_IMPL(compiler, warning) \
    PAS_IGNORE_WARNINGS_BEGIN_COND(__has_warning(warning), compiler, warning)
#else
#define PAS__IGNORE_WARNINGS_BEGIN_IMPL(compiler, warning) PAS_IGNORE_WARNINGS_BEGIN_COND(1, compiler, warning)
#endif

#define PAS_IGNORE_WARNINGS_BEGIN_IMPL(compiler, warning) \
    PAS__IGNORE_WARNINGS_BEGIN_IMPL(compiler, PAS_COMPILER_WARNING_NAME(warning))

#endif // PAS_COMPILER(GCC) || PAS_COMPILER(CLANG)

#if PAS_COMPILER(GCC)
#define PAS_IGNORE_GCC_WARNINGS_BEGIN(warning) PAS_IGNORE_WARNINGS_BEGIN_IMPL(GCC, warning)
#define PAS_IGNORE_GCC_WARNINGS_END PAS_IGNORE_WARNINGS_END_IMPL(GCC)
#else
#define PAS_IGNORE_GCC_WARNINGS_BEGIN(warning)
#define PAS_IGNORE_GCC_WARNINGS_END
#endif

#if PAS_COMPILER(CLANG)
#define PAS_IGNORE_CLANG_WARNINGS_BEGIN(warning) PAS_IGNORE_WARNINGS_BEGIN_IMPL(clang, warning)
#define PAS_IGNORE_CLANG_WARNINGS_END PAS_IGNORE_WARNINGS_END_IMPL(clang)
#else
#define PAS_IGNORE_CLANG_WARNINGS_BEGIN(warning)
#define PAS_IGNORE_CLANG_WARNINGS_END
#endif

#if PAS_COMPILER(GCC) || PAS_COMPILER(CLANG)
#define PAS_IGNORE_WARNINGS_BEGIN(warning) PAS_IGNORE_WARNINGS_BEGIN_IMPL(GCC, warning)
#define PAS_IGNORE_WARNINGS_END PAS_IGNORE_WARNINGS_END_IMPL(GCC)
#else
#define PAS_IGNORE_WARNINGS_BEGIN(warning)
#define PAS_IGNORE_WARNINGS_END
#endif

#define PAS_PLATFORM(PLATFORM) (defined PAS_PLATFORM_##PLATFORM && PAS_PLATFORM_##PLATFORM)
#define PAS_OS(OS) (defined PAS_OS_##OS && PAS_OS_##OS)

#ifdef __APPLE__
#define PAS_OS_DARWIN 1
#endif

#if defined(__unix) || defined(__unix__)
#define PAS_OS_UNIX 1
#endif

#ifdef __linux__
#define PAS_OS_LINUX 1
#endif

#if defined(__FreeBSD__) || defined(__DragonFly__) || defined(__FreeBSD_kernel__)
#define PAS_OS_FREEBSD 1
#endif

#if defined(WIN32) || defined(_WIN32)
#define PAS_OS_WINDOWS 1
#endif

#if PAS_OS(DARWIN) && !defined(BUILDING_WITH_CMAKE)
#if TARGET_OS_IOS
#define PAS_OS_IOS 1
#define PAS_PLATFORM_IOS 1
#if TARGET_OS_SIMULATOR
#define PAS_PLATFORM_IOS_SIMULATOR 1
#endif
#if defined(TARGET_OS_MACCATALYST) && TARGET_OS_MACCATALYST
#define PAS_PLATFORM_MACCATALYST 1
#endif
#endif
#if TARGET_OS_IPHONE
#define PAS_PLATFORM_IOS_FAMILY 1
#if TARGET_OS_SIMULATOR
#define PAS_PLATFORM_IOS_FAMILY_SIMULATOR 1
#endif
#elif TARGET_OS_MAC
#define PAS_OS_MAC 1
#define PAS_PLATFORM_MAC 1
#endif
#endif

#if PAS_PLATFORM(MAC) || PAS_PLATFORM(IOS_FAMILY)
#define PAS_PLATFORM_COCOA 1
#endif

#if defined(TARGET_OS_WATCH) && TARGET_OS_WATCH
#define PAS_OS_WATCHOS 1
#define PAS_PLATFORM_WATCHOS 1
#endif

#if defined(TARGET_OS_TV) && TARGET_OS_TV
#define PAS_OS_APPLETV 1
#define PAS_PLATFORM_APPLETV 1
#endif

#if defined(__SCE__)
#define PAS_PLATFORM_PLAYSTATION 1
#endif

#if PAS_COMPILER(GCC_COMPATIBLE)
/* __LP64__ is not defined on 64bit Windows since it uses LLP64. Using __SIZEOF_POINTER__ is simpler. */
#if __SIZEOF_POINTER__ == 8
#define PAS_CPU_ADDRESS64 1
#elif __SIZEOF_POINTER__ == 4
#define PAS_CPU_ADDRESS32 1
#else
#error "Unsupported pointer width"
#endif
#else
#error "Unsupported compiler for libpas"
#endif

#endif /* PAS_PLATFORM_H */
