/*
 * Copyright (C) 2014-2021 Apple Inc. All rights reserved.
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

#include "BCompiler.h"

#ifdef __APPLE__
#include <Availability.h>
#include <AvailabilityMacros.h>
#include <TargetConditionals.h>
#endif

#define BPLATFORM(PLATFORM) (defined BPLATFORM_##PLATFORM && BPLATFORM_##PLATFORM)
#define BOS(OS) (defined BOS_##OS && BOS_##OS)

#ifdef __APPLE__
#define BOS_DARWIN 1
#endif

#if defined(__unix) || defined(__unix__)
#define BOS_UNIX 1
#endif

#ifdef __linux__
#define BOS_LINUX 1
#endif

#if defined(__FreeBSD__) || defined(__DragonFly__) || defined(__FreeBSD_kernel__)
#define BOS_FREEBSD 1
#endif

#if defined(WIN32) || defined(_WIN32)
#define BOS_WINDOWS 1
#endif

#if BOS(DARWIN) && !defined(BUILDING_WITH_CMAKE)
#if TARGET_OS_IOS
#define BOS_IOS 1
#define BPLATFORM_IOS 1
#if TARGET_OS_SIMULATOR
#define BPLATFORM_IOS_SIMULATOR 1
#endif
#if defined(TARGET_OS_MACCATALYST) && TARGET_OS_MACCATALYST
#define BPLATFORM_MACCATALYST 1
#endif
#endif
#if TARGET_OS_IPHONE
#define BPLATFORM_IOS_FAMILY 1
#if TARGET_OS_SIMULATOR
#define BPLATFORM_IOS_FAMILY_SIMULATOR 1
#endif
#elif TARGET_OS_MAC
#define BOS_MAC 1
#define BPLATFORM_MAC 1
#endif
#endif

#if BPLATFORM(MAC) || BPLATFORM(IOS_FAMILY)
#define BPLATFORM_COCOA 1
#endif

#if defined(TARGET_OS_WATCH) && TARGET_OS_WATCH
#define BOS_WATCHOS 1
#define BPLATFORM_WATCHOS 1
#endif

#if defined(TARGET_OS_TV) && TARGET_OS_TV
#define BOS_APPLETV 1
#define BPLATFORM_APPLETV 1
#endif

#if defined(__SCE__)
#define BPLATFORM_PLAYSTATION 1
#endif

#if defined(BUILDING_GTK__)
#define BPLATFORM_GTK 1
#elif defined(BUILDING_WPE__)
#define BPLATFORM_WPE 1
#elif defined(BUILDING_JSCONLY__)
/* JSCOnly does not provide BPLATFORM() macro */
#elif BOS(WINDOWS)
#define BPLATFORM_WIN 1
#endif

/* ==== Feature decision macros: these define feature choices for a particular port. ==== */

#define BENABLE(WTF_FEATURE) (defined BENABLE_##WTF_FEATURE && BENABLE_##WTF_FEATURE)

/* ==== Policy decision macros: these define policy choices for a particular port. ==== */

/* BUSE() - use a particular third-party library or optional OS service */
#define BUSE(FEATURE) (defined BUSE_##FEATURE && BUSE_##FEATURE)

/* ==== Compiler adaptation macros: these describe the capabilities of the compiler. ==== */

/* BCOMPILER_SUPPORTS() - check for a compiler feature */
#define BCOMPILER_SUPPORTS(FEATURE) (defined BCOMPILER_SUPPORTS_##FEATURE && BCOMPILER_SUPPORTS_##FEATURE)

/* ==== Platform adaptation macros: these describe properties of the target environment. ==== */

/* BCPU() - the target CPU architecture */
#define BCPU(_FEATURE) (defined BCPU_##_FEATURE  && BCPU_##_FEATURE)

/* BCPU(X86) - i386 / x86 32-bit */
#if defined(__i386__) \
|| defined(i386)     \
|| defined(_M_IX86)  \
|| defined(_X86_)    \
|| defined(__THW_INTEL)
#define BCPU_X86 1
#endif

/* BCPU(X86_64) - AMD64 / Intel64 / x86_64 64-bit */
#if defined(__x86_64__) \
|| defined(_M_X64)
#define BCPU_X86_64 1
#endif

/* BCPU(ARM64) */
#if defined(__arm64__) || defined(__aarch64__)
#define BCPU_ARM64 1
#if defined(__arm64e__)
#define BCPU_ARM64E 1
#endif
#endif

/* BCPU(ARM) - ARM, any version*/
#define BARM_ARCH_AT_LEAST(N) (BCPU(ARM) && BARM_ARCH_VERSION >= N)

#if   defined(arm) \
|| defined(__arm__) \
|| defined(ARM) \
|| defined(_ARM_)
#define BCPU_ARM 1

/* Set BARM_ARCH_VERSION */
#if   defined(__ARM_ARCH_4__) \
|| defined(__ARM_ARCH_4T__) \
|| defined(__MARM_ARMV4__)
#define BARM_ARCH_VERSION 4

#elif defined(__ARM_ARCH_5__) \
|| defined(__ARM_ARCH_5T__) \
|| defined(__MARM_ARMV5__)
#define BARM_ARCH_VERSION 5

#elif defined(__ARM_ARCH_5E__) \
|| defined(__ARM_ARCH_5TE__) \
|| defined(__ARM_ARCH_5TEJ__)
#define BARM_ARCH_VERSION 5

#elif defined(__ARM_ARCH_6__) \
|| defined(__ARM_ARCH_6J__) \
|| defined(__ARM_ARCH_6K__) \
|| defined(__ARM_ARCH_6Z__) \
|| defined(__ARM_ARCH_6ZK__) \
|| defined(__ARM_ARCH_6T2__) \
|| defined(__ARMV6__)
#define BARM_ARCH_VERSION 6

#elif defined(__ARM_ARCH_7A__) \
|| defined(__ARM_ARCH_7K__) \
|| defined(__ARM_ARCH_7R__) \
|| defined(__ARM_ARCH_7S__)
#define BARM_ARCH_VERSION 7

#elif defined(__ARM_ARCH_8__) \
|| defined(__ARM_ARCH_8A__)
#define BARM_ARCH_VERSION 8

/* MSVC sets _M_ARM */
#elif defined(_M_ARM)
#define BARM_ARCH_VERSION _M_ARM

/* RVCT sets _TARGET_ARCH_ARM */
#elif defined(__TARGET_ARCH_ARM)
#define BARM_ARCH_VERSION __TARGET_ARCH_ARM

#else
#define WTF_ARM_ARCH_VERSION 0

#endif

/* Set BTHUMB_ARCH_VERSION */
#if   defined(__ARM_ARCH_4T__)
#define BTHUMB_ARCH_VERSION 1

#elif defined(__ARM_ARCH_5T__) \
|| defined(__ARM_ARCH_5TE__) \
|| defined(__ARM_ARCH_5TEJ__)
#define BTHUMB_ARCH_VERSION 2

#elif defined(__ARM_ARCH_6J__) \
|| defined(__ARM_ARCH_6K__) \
|| defined(__ARM_ARCH_6Z__) \
|| defined(__ARM_ARCH_6ZK__) \
|| defined(__ARM_ARCH_6M__)
#define BTHUMB_ARCH_VERSION 3

#elif defined(__ARM_ARCH_6T2__) \
|| defined(__ARM_ARCH_7__) \
|| defined(__ARM_ARCH_7A__) \
|| defined(__ARM_ARCH_7K__) \
|| defined(__ARM_ARCH_7M__) \
|| defined(__ARM_ARCH_7R__) \
|| defined(__ARM_ARCH_7S__)
#define BTHUMB_ARCH_VERSION 4

/* RVCT sets __TARGET_ARCH_THUMB */
#elif defined(__TARGET_ARCH_THUMB)
#define BTHUMB_ARCH_VERSION __TARGET_ARCH_THUMB

#else
#define BTHUMB_ARCH_VERSION 0
#endif

/* BCPU(ARM_TRADITIONAL) - Thumb2 is not available, only traditional ARM (v4 or greater) */
/* BCPU(ARM_THUMB2) - Thumb2 instruction set is available */
/* Only one of these will be defined. */
#if !defined(BCPU_ARM_TRADITIONAL) && !defined(BCPU_ARM_THUMB2)
#  if defined(thumb2) || defined(__thumb2__) \
|| ((defined(__thumb) || defined(__thumb__)) && BTHUMB_ARCH_VERSION == 4)
#    define BCPU_ARM_TRADITIONAL 0
#    define BCPU_ARM_THUMB2 1
#  elif BARM_ARCH_AT_LEAST(4)
#    define BCPU_ARM_TRADITIONAL 1
#    define BCPU_ARM_THUMB2 0
#  else
#    error "Not supported ARM architecture"
#  endif
#elif BCPU(ARM_TRADITIONAL) && BCPU(ARM_THUMB2) /* Sanity Check */
#  error "Cannot use both of BCPU_ARM_TRADITIONAL and BCPU_ARM_THUMB2 platforms"
#endif /* !defined(BCPU_ARM_TRADITIONAL) && !defined(BCPU_ARM_THUMB2) */

#endif /* ARM */


#if BCOMPILER(GCC_COMPATIBLE)
/* __LP64__ is not defined on 64bit Windows since it uses LLP64. Using __SIZEOF_POINTER__ is simpler. */
#if __SIZEOF_POINTER__ == 8
#define BCPU_ADDRESS64 1
#elif __SIZEOF_POINTER__ == 4
#define BCPU_ADDRESS32 1
#else
#error "Unsupported pointer width"
#endif
#else
#error "Unsupported compiler for bmalloc"
#endif

#if BCOMPILER(GCC_COMPATIBLE)
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define BCPU_BIG_ENDIAN 1
#elif __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define BCPU_LITTLE_ENDIAN 1
#elif __BYTE_ORDER__ == __ORDER_PDP_ENDIAN__
#define BCPU_MIDDLE_ENDIAN 1
#else
#error "Unknown endian"
#endif
#else
#error "Unsupported compiler for bmalloc"
#endif

#if BCPU(ADDRESS64)
#if BOS(DARWIN)
#define BOS_EFFECTIVE_ADDRESS_WIDTH (bmalloc::getMSBSetConstexpr(MACH_VM_MAX_ADDRESS) + 1)
#else
/* We strongly assume that effective address width is <= 48 in 64bit architectures (e.g. NaN boxing). */
#define BOS_EFFECTIVE_ADDRESS_WIDTH 48
#endif
#else
#define BOS_EFFECTIVE_ADDRESS_WIDTH 32
#endif

#define BATTRIBUTE_PRINTF(formatStringArgument, extraArguments) __attribute__((__format__(printf, formatStringArgument, extraArguments)))

/* Export macro support. Detects the attributes available for shared library symbol export
   decorations. */
#if BOS(WINDOWS) || (BCOMPILER_HAS_CLANG_DECLSPEC(dllimport) && BCOMPILER_HAS_CLANG_DECLSPEC(dllexport))
#define BUSE_DECLSPEC_ATTRIBUTE 1
#elif BCOMPILER(GCC_COMPATIBLE)
#define BUSE_VISIBILITY_ATTRIBUTE 1
#endif

#if BPLATFORM(MAC) || BPLATFORM(IOS_FAMILY)
#define BUSE_OS_LOG 1
#endif

/* BUNUSED_PARAM */
#if !defined(BUNUSED_PARAM)
#define BUNUSED_PARAM(variable) (void)variable
#endif

/* Enable this to put each IsoHeap and other allocation categories into their own malloc heaps, so that tools like vmmap can show how big each heap is. */
#define BENABLE_MALLOC_HEAP_BREAKDOWN 0

/* This is used for debugging when hacking on how bmalloc calculates its physical footprint. */
#define ENABLE_PHYSICAL_PAGE_MAP 0

/* BENABLE(LIBPAS) is enabling libpas build. But this does not mean we use libpas for bmalloc replacement. */
#if !defined(BENABLE_LIBPAS)
#if BCPU(ADDRESS64) && (BOS(DARWIN) || (BOS(LINUX) && !BPLATFORM(GTK) && !BPLATFORM(WPE))) || BPLATFORM(PLAYSTATION)
#define BENABLE_LIBPAS 1
#ifndef PAS_BMALLOC
#define PAS_BMALLOC 1
#endif
#else
#define BENABLE_LIBPAS 0
#endif
#endif

/* BUSE(LIBPAS) is using libpas for bmalloc replacement. */
#if !defined(BUSE_LIBPAS)
#if defined(BENABLE_LIBPAS) && BENABLE_LIBPAS
#define BUSE_LIBPAS 1
#else
#define BUSE_LIBPAS 0
#endif
#endif

#if !defined(BUSE_PRECOMPUTED_CONSTANTS_VMPAGE4K)
#define BUSE_PRECOMPUTED_CONSTANTS_VMPAGE4K 1
#endif

#if !defined(BUSE_PRECOMPUTED_CONSTANTS_VMPAGE16K)
#define BUSE_PRECOMPUTED_CONSTANTS_VMPAGE16K 1
#endif

/* The unified Config record feature is not available for Windows because the
   Windows port puts WTF in a separate DLL, and the offlineasm code accessing
   the config record expects the config record to be directly accessible like
   a global variable (and not have to go thru DLL shenanigans). C++ code would
   resolve these DLL bindings automatically, but offlineasm does not.

   The permanently freezing feature also currently relies on the Config records
   being unified, and the Windows port also does not currently have an
   implementation for the freezing mechanism anyway. For simplicity, we just
   disable both the use of unified Config record and config freezing for the
   Windows port.
*/
#if BOS(WINDOWS)
#define BENABLE_UNIFIED_AND_FREEZABLE_CONFIG_RECORD 0
#else
#define BENABLE_UNIFIED_AND_FREEZABLE_CONFIG_RECORD 1
#endif
