/*
 * Copyright (C) 2006-2019 Apple Inc. All rights reserved.
 * Copyright (C) 2007-2009 Torch Mobile, Inc.
 * Copyright (C) 2010, 2011 Research In Motion Limited. All rights reserved.
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

/* Include compiler specific macros */
#include <wtf/Compiler.h>

/* ==== PLATFORM handles OS, operating environment, graphics API, and
   CPU. This macro will be phased out in favor of platform adaptation
   macros, policy decision macros, and top-level port definitions. ==== */
#define PLATFORM(WTF_FEATURE) (defined WTF_PLATFORM_##WTF_FEATURE  && WTF_PLATFORM_##WTF_FEATURE)


/* ==== Platform adaptation macros: these describe properties of the target environment. ==== */

/* CPU() - the target CPU architecture */
#define CPU(WTF_FEATURE) (defined WTF_CPU_##WTF_FEATURE  && WTF_CPU_##WTF_FEATURE)
/* HAVE() - specific system features (headers, functions or similar) that are present or not */
#define HAVE(WTF_FEATURE) (defined HAVE_##WTF_FEATURE  && HAVE_##WTF_FEATURE)
/* OS() - underlying operating system; only to be used for mandated low-level services like 
   virtual memory, not to choose a GUI toolkit */
#define OS(WTF_FEATURE) (defined WTF_OS_##WTF_FEATURE  && WTF_OS_##WTF_FEATURE)


/* ==== Policy decision macros: these define policy choices for a particular port. ==== */

/* USE() - use a particular third-party library or optional OS service */
#define USE(WTF_FEATURE) (defined USE_##WTF_FEATURE  && USE_##WTF_FEATURE)
/* ENABLE() - turn on a specific feature of WebKit */
#define ENABLE(WTF_FEATURE) (defined ENABLE_##WTF_FEATURE  && ENABLE_##WTF_FEATURE)


/* ==== CPU() - the target CPU architecture ==== */
/* CPU(KNOWN) becomes true if we explicitly support a target CPU. */

/* CPU(MIPS) - MIPS 32-bit and 64-bit */
#if (defined(mips) || defined(__mips__) || defined(MIPS) || defined(_MIPS_) || defined(__mips64))
#if defined(_ABI64) && (_MIPS_SIM == _ABI64)
#define WTF_CPU_MIPS64 1
#define WTF_MIPS_ARCH __mips64
#else
#define WTF_CPU_MIPS 1
#define WTF_MIPS_ARCH __mips
#endif
#define WTF_CPU_KNOWN 1
#define WTF_MIPS_PIC (defined __PIC__)
#define WTF_MIPS_ISA(v) (defined WTF_MIPS_ARCH && WTF_MIPS_ARCH == v)
#define WTF_MIPS_ISA_AT_LEAST(v) (defined WTF_MIPS_ARCH && WTF_MIPS_ARCH >= v)
#define WTF_MIPS_ARCH_REV __mips_isa_rev
#define WTF_MIPS_ISA_REV(v) (defined WTF_MIPS_ARCH_REV && WTF_MIPS_ARCH_REV == v)
#define WTF_MIPS_ISA_REV_AT_LEAST(v) (defined WTF_MIPS_ARCH_REV && WTF_MIPS_ARCH_REV >= v)
#define WTF_MIPS_DOUBLE_FLOAT (defined __mips_hard_float && !defined __mips_single_float)
#define WTF_MIPS_FP64 (defined __mips_fpr && __mips_fpr == 64)
/* MIPS requires allocators to use aligned memory */
#define USE_ARENA_ALLOC_ALIGNMENT_INTEGER 1
#endif /* MIPS */

/* CPU(PPC64) - PowerPC 64-bit Big Endian */
#if (  defined(__ppc64__)      \
    || defined(__PPC64__))     \
    && defined(__BYTE_ORDER__) \
    && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
#define WTF_CPU_PPC64 1
#define WTF_CPU_KNOWN 1
#endif

/* CPU(PPC64LE) - PowerPC 64-bit Little Endian */
#if (   defined(__ppc64__)     \
    || defined(__PPC64__)      \
    || defined(__ppc64le__)    \
    || defined(__PPC64LE__))   \
    && defined(__BYTE_ORDER__) \
    && (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
#define WTF_CPU_PPC64LE 1
#define WTF_CPU_KNOWN 1
#endif

/* CPU(PPC) - PowerPC 32-bit */
#if (  defined(__ppc__)        \
    || defined(__PPC__)        \
    || defined(__powerpc__)    \
    || defined(__powerpc)      \
    || defined(__POWERPC__)    \
    || defined(_M_PPC)         \
    || defined(__PPC))         \
    && !CPU(PPC64)             \
    && CPU(BIG_ENDIAN)
#define WTF_CPU_PPC 1
#define WTF_CPU_KNOWN 1
#endif

/* CPU(X86) - i386 / x86 32-bit */
#if   defined(__i386__) \
    || defined(i386)     \
    || defined(_M_IX86)  \
    || defined(_X86_)    \
    || defined(__THW_INTEL)
#define WTF_CPU_X86 1
#define WTF_CPU_KNOWN 1

#if defined(__SSE2__) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
#define WTF_CPU_X86_SSE2 1
#endif

#endif

/* CPU(X86_64) - AMD64 / Intel64 / x86_64 64-bit */
#if   defined(__x86_64__) \
    || defined(_M_X64)
#define WTF_CPU_X86_64 1
#define WTF_CPU_X86_SSE2 1
#define WTF_CPU_KNOWN 1
#endif

/* CPU(ARM64) - Apple */
#if (defined(__arm64__) && defined(__APPLE__)) || defined(__aarch64__)
#define WTF_CPU_ARM64 1
#define WTF_CPU_KNOWN 1

#if defined(__arm64e__)
#define WTF_CPU_ARM64E 1
#endif
#endif

/* CPU(ARM) - ARM, any version*/
#define WTF_ARM_ARCH_AT_LEAST(N) (CPU(ARM) && WTF_ARM_ARCH_VERSION >= N)

#if   defined(arm) \
    || defined(__arm__) \
    || defined(ARM) \
    || defined(_ARM_)
#define WTF_CPU_ARM 1
#define WTF_CPU_KNOWN 1

#if defined(__ARM_PCS_VFP)
#define WTF_CPU_ARM_HARDFP 1
#endif

/* Set WTF_ARM_ARCH_VERSION */
#if   defined(__ARM_ARCH_4__) \
    || defined(__ARM_ARCH_4T__) \
    || defined(__MARM_ARMV4__)
#define WTF_ARM_ARCH_VERSION 4

#elif defined(__ARM_ARCH_5__) \
    || defined(__ARM_ARCH_5T__) \
    || defined(__MARM_ARMV5__)
#define WTF_ARM_ARCH_VERSION 5

#elif defined(__ARM_ARCH_5E__) \
    || defined(__ARM_ARCH_5TE__) \
    || defined(__ARM_ARCH_5TEJ__)
#define WTF_ARM_ARCH_VERSION 5
/*ARMv5TE requires allocators to use aligned memory*/
#define USE_ARENA_ALLOC_ALIGNMENT_INTEGER 1

#elif defined(__ARM_ARCH_6__) \
    || defined(__ARM_ARCH_6J__) \
    || defined(__ARM_ARCH_6K__) \
    || defined(__ARM_ARCH_6Z__) \
    || defined(__ARM_ARCH_6ZK__) \
    || defined(__ARM_ARCH_6T2__) \
    || defined(__ARMV6__)
#define WTF_ARM_ARCH_VERSION 6

#elif defined(__ARM_ARCH_7A__) \
    || defined(__ARM_ARCH_7K__) \
    || defined(__ARM_ARCH_7R__) \
    || defined(__ARM_ARCH_7S__)
#define WTF_ARM_ARCH_VERSION 7

#elif defined(__ARM_ARCH_8__) \
    || defined(__ARM_ARCH_8A__)
#define WTF_ARM_ARCH_VERSION 8

/* MSVC sets _M_ARM */
#elif defined(_M_ARM)
#define WTF_ARM_ARCH_VERSION _M_ARM

/* RVCT sets _TARGET_ARCH_ARM */
#elif defined(__TARGET_ARCH_ARM)
#define WTF_ARM_ARCH_VERSION __TARGET_ARCH_ARM

#if defined(__TARGET_ARCH_5E) \
    || defined(__TARGET_ARCH_5TE) \
    || defined(__TARGET_ARCH_5TEJ)
/*ARMv5TE requires allocators to use aligned memory*/
#define USE_ARENA_ALLOC_ALIGNMENT_INTEGER 1
#endif

#else
#define WTF_ARM_ARCH_VERSION 0

#endif

/* Set WTF_THUMB_ARCH_VERSION */
#if   defined(__ARM_ARCH_4T__)
#define WTF_THUMB_ARCH_VERSION 1

#elif defined(__ARM_ARCH_5T__) \
    || defined(__ARM_ARCH_5TE__) \
    || defined(__ARM_ARCH_5TEJ__)
#define WTF_THUMB_ARCH_VERSION 2

#elif defined(__ARM_ARCH_6J__) \
    || defined(__ARM_ARCH_6K__) \
    || defined(__ARM_ARCH_6Z__) \
    || defined(__ARM_ARCH_6ZK__) \
    || defined(__ARM_ARCH_6M__)
#define WTF_THUMB_ARCH_VERSION 3

#elif defined(__ARM_ARCH_6T2__) \
    || defined(__ARM_ARCH_7__) \
    || defined(__ARM_ARCH_7A__) \
    || defined(__ARM_ARCH_7K__) \
    || defined(__ARM_ARCH_7M__) \
    || defined(__ARM_ARCH_7R__) \
    || defined(__ARM_ARCH_7S__)
#define WTF_THUMB_ARCH_VERSION 4

/* RVCT sets __TARGET_ARCH_THUMB */
#elif defined(__TARGET_ARCH_THUMB)
#define WTF_THUMB_ARCH_VERSION __TARGET_ARCH_THUMB

#else
#define WTF_THUMB_ARCH_VERSION 0
#endif


/* CPU(ARMV5_OR_LOWER) - ARM instruction set v5 or earlier */
/* On ARMv5 and below the natural alignment is required. 
   And there are some other differences for v5 or earlier. */
#if !defined(ARMV5_OR_LOWER) && !WTF_ARM_ARCH_AT_LEAST(6)
#define WTF_CPU_ARMV5_OR_LOWER 1
#endif


/* CPU(ARM_TRADITIONAL) - Thumb2 is not available, only traditional ARM (v4 or greater) */
/* CPU(ARM_THUMB2) - Thumb2 instruction set is available */
/* Only one of these will be defined. */
#if !defined(WTF_CPU_ARM_TRADITIONAL) && !defined(WTF_CPU_ARM_THUMB2)
#  if defined(thumb2) || defined(__thumb2__) \
    || ((defined(__thumb) || defined(__thumb__)) && WTF_THUMB_ARCH_VERSION == 4)
#    define WTF_CPU_ARM_TRADITIONAL 0
#    define WTF_CPU_ARM_THUMB2 1
#  elif WTF_ARM_ARCH_AT_LEAST(4)
#    define WTF_CPU_ARM_TRADITIONAL 1
#    define WTF_CPU_ARM_THUMB2 0
#  else
#    error "Not supported ARM architecture"
#  endif
#elif CPU(ARM_TRADITIONAL) && CPU(ARM_THUMB2) /* Sanity Check */
#  error "Cannot use both of WTF_CPU_ARM_TRADITIONAL and WTF_CPU_ARM_THUMB2 platforms"
#endif /* !defined(WTF_CPU_ARM_TRADITIONAL) && !defined(WTF_CPU_ARM_THUMB2) */

#if defined(__ARM_NEON__) && !defined(WTF_CPU_ARM_NEON)
#define WTF_CPU_ARM_NEON 1
#endif

#if CPU(ARM_NEON)
/* All NEON intrinsics usage can be disabled by this macro. */
#define HAVE_ARM_NEON_INTRINSICS 1
#endif

#if (defined(__VFP_FP__) && !defined(__SOFTFP__))
#define WTF_CPU_ARM_VFP 1
#endif

/* If CPU(ARM_NEON) is not enabled, we'll conservatively assume only VFP2 or VFPv3D16
   support is available. Hence, only the first 16 64-bit floating point registers
   are available. See:
   NEON registers: http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.dui0473c/CJACABEJ.html
   VFP2 and VFP3 registers: http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.dui0473c/CIHDIBDG.html
   NEON to VFP register mapping: http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.dui0473c/CJAIJHFC.html
*/
#if CPU(ARM_NEON)
#define WTF_CPU_ARM_VFP_V3_D32 1
#else
#define WTF_CPU_ARM_VFP_V2 1
#endif

#if defined(__ARM_ARCH_7K__)
#define WTF_CPU_APPLE_ARMV7K 1
#endif

#if defined(__ARM_ARCH_7S__)
#define WTF_CPU_APPLE_ARMV7S 1
#endif

#if defined(__ARM_ARCH_EXT_IDIV__) || CPU(APPLE_ARMV7S)
#define HAVE_ARM_IDIV_INSTRUCTIONS 1
#endif

#endif /* ARM */

#if !CPU(KNOWN)
#define WTF_CPU_UNKNOWN 1
#endif

#if CPU(ARM) || CPU(MIPS) || CPU(UNKNOWN)
#define WTF_CPU_NEEDS_ALIGNED_ACCESS 1
#endif

#if COMPILER(GCC_COMPATIBLE)
/* __LP64__ is not defined on 64bit Windows since it uses LLP64. Using __SIZEOF_POINTER__ is simpler. */
#if __SIZEOF_POINTER__ == 8
#define WTF_CPU_ADDRESS64 1
#elif __SIZEOF_POINTER__ == 4
#define WTF_CPU_ADDRESS32 1
#else
#error "Unsupported pointer width"
#endif
#elif COMPILER(MSVC)
#if defined(_WIN64)
#define WTF_CPU_ADDRESS64 1
#else
#define WTF_CPU_ADDRESS32 1
#endif
#else
/* This is the most generic way. But in OS(DARWIN), Platform.h can be included by sandbox definition file (.sb).
 * At that time, we cannot include "stdint.h" header. So in the case of known compilers, we use predefined constants instead. */
#include <stdint.h>
#if UINTPTR_MAX > UINT32_MAX
#define WTF_CPU_ADDRESS64 1
#else
#define WTF_CPU_ADDRESS32 1
#endif
#endif

/* ==== OS() - underlying operating system; only to be used for mandated low-level services like 
   virtual memory, not to choose a GUI toolkit ==== */

/* OS(AIX) - AIX */
#ifdef _AIX
#define WTF_OS_AIX 1
#endif

/* OS(DARWIN) - Any Darwin-based OS, including Mac OS X and iPhone OS */
#ifdef __APPLE__
#define WTF_OS_DARWIN 1

#include <Availability.h>
#include <AvailabilityMacros.h>
#include <TargetConditionals.h>
#endif

/* OS(IOS_FAMILY) - iOS family, including iOS, iOSMac, tvOS, watchOS */
/* OS(IOS) - iOS only, not including iOSMac */
/* OS(MAC_OS_X) - macOS (not including iOS family) */
#if OS(DARWIN)
#if TARGET_OS_IOS && !(defined(TARGET_OS_IOSMAC) && TARGET_OS_IOSMAC)
#define WTF_OS_IOS 1
#endif
#if TARGET_OS_IPHONE
#define WTF_OS_IOS_FAMILY 1
#elif TARGET_OS_MAC
#define WTF_OS_MAC_OS_X 1
#endif
#endif

/* OS(FREEBSD) - FreeBSD */
#if defined(__FreeBSD__) || defined(__DragonFly__) || defined(__FreeBSD_kernel__)
#define WTF_OS_FREEBSD 1
#endif

/* OS(FUCHSIA) - Fuchsia */
#ifdef __Fuchsia__
#define WTF_OS_FUCHSIA 1
#endif

/* OS(HURD) - GNU/Hurd */
#ifdef __GNU__
#define WTF_OS_HURD 1
#endif

/* OS(LINUX) - Linux */
#ifdef __linux__
#define WTF_OS_LINUX 1
#endif

/* OS(NETBSD) - NetBSD */
#if defined(__NetBSD__)
#define WTF_OS_NETBSD 1
#endif

/* OS(OPENBSD) - OpenBSD */
#ifdef __OpenBSD__
#define WTF_OS_OPENBSD 1
#endif

/* OS(WINDOWS) - Any version of Windows */
#if defined(WIN32) || defined(_WIN32)
#define WTF_OS_WINDOWS 1
#endif

#define WTF_OS_WIN ERROR "USE WINDOWS WITH OS NOT WIN"
#define WTF_OS_MAC ERROR "USE MAC_OS_X WITH OS NOT MAC"

/* OS(UNIX) - Any Unix-like system */
#if    OS(AIX)              \
    || OS(DARWIN)           \
    || OS(FREEBSD)          \
    || OS(FUCHSIA)          \
    || OS(HURD)             \
    || OS(LINUX)            \
    || OS(NETBSD)           \
    || OS(OPENBSD)          \
    || defined(unix)        \
    || defined(__unix)      \
    || defined(__unix__)
#define WTF_OS_UNIX 1
#endif

/* Operating environments */

/* CPU(BIG_ENDIAN) or CPU(MIDDLE_ENDIAN) or neither, as appropriate. */

#if COMPILER(GCC_COMPATIBLE)
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define WTF_CPU_BIG_ENDIAN 1
#elif __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define WTF_CPU_LITTLE_ENDIAN 1
#elif __BYTE_ORDER__ == __ORDER_PDP_ENDIAN__
#define WTF_CPU_MIDDLE_ENDIAN 1
#else
#error "Unknown endian"
#endif
#else
#if OS(WINDOWS)
/* Windows only have little endian architecture. */
#define WTF_CPU_LITTLE_ENDIAN 1
#else
#include <sys/types.h>
#if __has_include(<endian.h>)
#include <endian.h>
#if __BYTE_ORDER == __BIG_ENDIAN
#define WTF_CPU_BIG_ENDIAN 1
#elif __BYTE_ORDER == __LITTLE_ENDIAN
#define WTF_CPU_LITTLE_ENDIAN 1
#elif __BYTE_ORDER == __PDP_ENDIAN
#define WTF_CPU_MIDDLE_ENDIAN 1
#else
#error "Unknown endian"
#endif
#else
#if __has_include(<machine/endian.h>)
#include <machine/endian.h>
#else
#include <sys/endian.h>
#endif
#if BYTE_ORDER == BIG_ENDIAN
#define WTF_CPU_BIG_ENDIAN 1
#elif BYTE_ORDER == LITTLE_ENDIAN
#define WTF_CPU_LITTLE_ENDIAN 1
#elif BYTE_ORDER == PDP_ENDIAN
#define WTF_CPU_MIDDLE_ENDIAN 1
#else
#error "Unknown endian"
#endif
#endif
#endif
#endif

#if !CPU(LITTLE_ENDIAN) && !CPU(BIG_ENDIAN)
#error "Unsupported endian"
#endif

/* Export macro support. Detects the attributes available for shared library symbol export
   decorations. */
#if OS(WINDOWS) || (COMPILER_HAS_CLANG_DECLSPEC(dllimport) && COMPILER_HAS_CLANG_DECLSPEC(dllexport))
#define USE_DECLSPEC_ATTRIBUTE 1
#define USE_VISIBILITY_ATTRIBUTE 0
#elif defined(__GNUC__)
#define USE_DECLSPEC_ATTRIBUTE 0
#define USE_VISIBILITY_ATTRIBUTE 1
#else
#define USE_DECLSPEC_ATTRIBUTE 0
#define USE_VISIBILITY_ATTRIBUTE 0
#endif

/* Standard libraries */
#if defined(HAVE_FEATURES_H) && HAVE_FEATURES_H
/* If the included features.h is glibc's one, __GLIBC__ is defined. */
#include <features.h>
#endif

/* FIXME: these are all mixes of OS, operating environment and policy choices. */
/* PLATFORM(GTK) */
/* PLATFORM(MAC) */
/* PLATFORM(IOS) */
/* PLATFORM(IOS_FAMILY) */
/* PLATFORM(IOS_SIMULATOR) */
/* PLATFORM(IOS_FAMILY_SIMULATOR) */
/* PLATFORM(WIN) */
#if defined(BUILDING_GTK__)
#define WTF_PLATFORM_GTK 1
#elif defined(BUILDING_WPE__)
#define WTF_PLATFORM_WPE 1
#elif defined(BUILDING_JSCONLY__)
/* JSCOnly does not provide PLATFORM() macro */
#elif OS(MAC_OS_X)
#define WTF_PLATFORM_MAC 1
#elif OS(IOS_FAMILY)
#if OS(IOS)
#define WTF_PLATFORM_IOS 1
#endif
#define WTF_PLATFORM_IOS_FAMILY 1
#if TARGET_OS_SIMULATOR
#if OS(IOS)
#define WTF_PLATFORM_IOS_SIMULATOR 1
#endif
#define WTF_PLATFORM_IOS_FAMILY_SIMULATOR 1
#endif
#if defined(TARGET_OS_IOSMAC) && TARGET_OS_IOSMAC
#define WTF_PLATFORM_IOSMAC 1
#endif
#elif OS(WINDOWS)
#define WTF_PLATFORM_WIN 1
#endif

/* PLATFORM(COCOA) */
#if PLATFORM(MAC) || PLATFORM(IOS_FAMILY)
#define WTF_PLATFORM_COCOA 1
#endif

#if PLATFORM(COCOA)
#if defined __has_include && __has_include(<CoreFoundation/CFPriv.h>)
#define USE_APPLE_INTERNAL_SDK 1
#endif
#endif

/* PLATFORM(APPLETV) */
#if defined(TARGET_OS_TV) && TARGET_OS_TV
#define WTF_PLATFORM_APPLETV 1
#endif

/* PLATFORM(WATCHOS) */
#if defined(TARGET_OS_WATCH) && TARGET_OS_WATCH
#define WTF_PLATFORM_WATCHOS 1
#endif

/* Graphics engines */

/* USE(CG) and PLATFORM(CI) */
#if PLATFORM(COCOA)
#define USE_CG 1
#define USE_CA 1
#endif

#if PLATFORM(GTK) || PLATFORM(WPE)
#define USE_GLIB 1
#define USE_FREETYPE 1
#define USE_HARFBUZZ 1
#define USE_SOUP 1
#define USE_WEBP 1
#define USE_FILE_LOCK 1
#endif

#if PLATFORM(GTK)
#define GLIB_VERSION_MIN_REQUIRED GLIB_VERSION_2_36
#endif

#if PLATFORM(WPE)
#define GLIB_VERSION_MIN_REQUIRED GLIB_VERSION_2_40
#endif

#if PLATFORM(GTK) && !defined(GTK_API_VERSION_2)
#define GDK_VERSION_MIN_REQUIRED GDK_VERSION_3_6
#endif

#if USE(SOUP)
#define SOUP_VERSION_MIN_REQUIRED SOUP_VERSION_2_42
#endif

/* On Windows, use QueryPerformanceCounter by default */
#if OS(WINDOWS)
#define USE_QUERY_PERFORMANCE_COUNTER  1
#endif

#if PLATFORM(COCOA)

#define HAVE_OUT_OF_PROCESS_LAYER_HOSTING 1
#define USE_CF 1
#define USE_FILE_LOCK 1
#define USE_FOUNDATION 1
#define USE_NETWORK_CFDATA_ARRAY_CALLBACK 1

/* Cocoa defines a series of platform macros for debugging. */
/* Some of them are really annoying because they use common names (e.g. check()). */
/* Disable those macros so that we are not limited in how we name methods and functions. */
#undef __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORES
#define __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORES 0

#endif

#if PLATFORM(MAC)

#define HAVE_RUNLOOP_TIMER 1
#define HAVE_SEC_KEYCHAIN 1
#define USE_APPKIT 1
#define USE_PASSKIT 1

#if CPU(X86_64)
#define HAVE_NETWORK_EXTENSION 1
#define USE_PLUGIN_HOST_PROCESS 1
#endif
#endif /* PLATFORM(MAC) */

#if PLATFORM(IOS_FAMILY)

#define HAVE_NETWORK_EXTENSION 1
#define HAVE_READLINE 1
#define USE_UIKIT_EDITING 1
#define USE_WEB_THREAD 1

#if CPU(ARM64)
#define ENABLE_JIT_CONSTANT_BLINDING 0
#endif

#if CPU(ARM_NEON)
#undef HAVE_ARM_NEON_INTRINSICS
#define HAVE_ARM_NEON_INTRINSICS 0
#endif

#endif /* PLATFORM(IOS_FAMILY) */

#if !defined(HAVE_ACCESSIBILITY)
#if PLATFORM(COCOA) || PLATFORM(WIN) || PLATFORM(GTK) || PLATFORM(WPE)
#define HAVE_ACCESSIBILITY 1
#endif
#endif /* !defined(HAVE_ACCESSIBILITY) */

/* FIXME: Remove after CMake build enabled on Darwin */
#if OS(DARWIN)
#define HAVE_ERRNO_H 1
#define HAVE_LANGINFO_H 1
#define HAVE_LOCALTIME_R 1
#define HAVE_MMAP 1
#define HAVE_REGEX_H 1
#define HAVE_SIGNAL_H 1
#define HAVE_STAT_BIRTHTIME 1
#define HAVE_STRINGS_H 1
#define HAVE_STRNSTR 1
#define HAVE_SYS_PARAM_H 1
#define HAVE_SYS_TIME_H 1 
#define HAVE_TM_GMTOFF 1
#define HAVE_TM_ZONE 1
#define HAVE_TIMEGM 1
#define HAVE_PTHREAD_MAIN_NP 1

#if CPU(X86_64) || CPU(ARM64)
#define HAVE_INT128_T 1
#endif
#endif /* OS(DARWIN) */

#if OS(UNIX)
#define USE_PTHREADS 1
#endif /* OS(UNIX) */

#if OS(UNIX) && !OS(FUCHSIA)
#define HAVE_RESOURCE_H 1
#define HAVE_PTHREAD_SETSCHEDPARAM 1
#endif

#if OS(DARWIN)
#define HAVE_DISPATCH_H 1
#define HAVE_MADV_FREE 1
#define HAVE_MADV_FREE_REUSE 1
#define HAVE_MADV_DONTNEED 1
#define HAVE_MERGESORT 1
#define HAVE_PTHREAD_SETNAME_NP 1
#define HAVE_READLINE 1
#define HAVE_SYS_TIMEB_H 1

#if __has_include(<mach/mach_exc.defs>) && !PLATFORM(GTK)
#define HAVE_MACH_EXCEPTIONS 1
#endif

#if !PLATFORM(GTK)
#define USE_ACCELERATE 1
#endif
#if !PLATFORM(IOS_FAMILY)
#define HAVE_HOSTED_CORE_ANIMATION 1
#endif

#endif /* OS(DARWIN) */

#if OS(DARWIN) || OS(FUCHSIA) || ((OS(FREEBSD) || defined(__GLIBC__) || defined(__BIONIC__)) && (CPU(X86) || CPU(X86_64) || CPU(ARM) || CPU(ARM64) || CPU(MIPS)))
#define HAVE_MACHINE_CONTEXT 1
#endif

#if OS(DARWIN) || (OS(LINUX) && defined(__GLIBC__) && !defined(__UCLIBC__))
#define HAVE_BACKTRACE 1
#endif

#if OS(DARWIN) || OS(LINUX)
#if PLATFORM(GTK)
#if defined(__GLIBC__) && !defined(__UCLIBC__)
#define HAVE_BACKTRACE_SYMBOLS 1
#endif
#endif /* PLATFORM(GTK) */
#define HAVE_DLADDR 1
#endif /* OS(DARWIN) || OS(LINUX) */


/* ENABLE macro defaults */

/* FIXME: move out all ENABLE() defines from here to FeatureDefines.h */

#if USE(APPLE_INTERNAL_SDK) && __has_include(<WebKitAdditions/AdditionalFeatureDefines.h>)
#include <WebKitAdditions/AdditionalFeatureDefines.h>
#endif

/* Include feature macros */
#include <wtf/FeatureDefines.h>

#if OS(WINDOWS)
#define USE_SYSTEM_MALLOC 1
#endif

#if !defined(USE_JSVALUE64) && !defined(USE_JSVALUE32_64)
#if CPU(ADDRESS64) || CPU(ARM64)
#define USE_JSVALUE64 1
#else
#define USE_JSVALUE32_64 1
#endif
#endif /* !defined(USE_JSVALUE64) && !defined(USE_JSVALUE32_64) */

/* The JIT is enabled by default on all x86-64 & ARM64 platforms. */
#if !defined(ENABLE_JIT) \
    && (CPU(X86_64) || CPU(ARM64)) \
    && !CPU(APPLE_ARMV7K)
#define ENABLE_JIT 1
#endif

#if USE(JSVALUE32_64)
#if CPU(ARM_THUMB2) && OS(LINUX)
/* On ARMv7/Linux the JIT is enabled unless explicitly disabled. */
#if !defined(ENABLE_JIT)
#define ENABLE_JIT 1
#endif
#elif CPU(MIPS) && OS(LINUX)
/* Same on MIPS/Linux, but DFG is disabled for now. */
#if !defined(ENABLE_JIT)
#define ENABLE_JIT 1
#endif
#undef ENABLE_DFG_JIT
#define ENABLE_DFG_JIT 0
#else
/* Disable JIT and force C_LOOP on all other 32bit architectures. */
#undef ENABLE_JIT
#define ENABLE_JIT 0
#undef ENABLE_C_LOOP
#define ENABLE_C_LOOP 1
#endif
#endif

#if !defined(ENABLE_C_LOOP)
#if ENABLE(JIT) \
    || CPU(X86_64) || (CPU(ARM64) && !defined(__ILP32__))
#define ENABLE_C_LOOP 0
#else
#define ENABLE_C_LOOP 1
#endif
#endif

/* The FTL *does not* work on 32-bit platforms. Disable it even if someone asked us to enable it. */
#if USE(JSVALUE32_64)
#undef ENABLE_FTL_JIT
#define ENABLE_FTL_JIT 0
#endif

/* The FTL is disabled on the iOS simulator, mostly for simplicity. */
#if PLATFORM(IOS_FAMILY_SIMULATOR)
#undef ENABLE_FTL_JIT
#define ENABLE_FTL_JIT 0
#endif

/* If possible, try to enable a disassembler. This is optional. We proceed in two
   steps: first we try to find some disassembler that we can use, and then we
   decide if the high-level disassembler API can be enabled. */
#if !defined(USE_UDIS86) && ENABLE(JIT) && (CPU(X86) || CPU(X86_64)) && !USE(CAPSTONE)
#define USE_UDIS86 1
#endif

#if !defined(USE_ARM64_DISASSEMBLER) && ENABLE(JIT) && CPU(ARM64) && !USE(CAPSTONE)
#define USE_ARM64_DISASSEMBLER 1
#endif

#if !defined(ENABLE_DISASSEMBLER) && (USE(UDIS86) || USE(ARM64_DISASSEMBLER) || (ENABLE(JIT) && USE(CAPSTONE)))
#define ENABLE_DISASSEMBLER 1
#endif

#if !defined(ENABLE_DFG_JIT) && ENABLE(JIT)
/* Enable the DFG JIT on X86 and X86_64. */
#if (CPU(X86) || CPU(X86_64)) && (OS(DARWIN) || OS(LINUX) || OS(FREEBSD) || OS(HURD) || OS(WINDOWS))
#define ENABLE_DFG_JIT 1
#endif
/* Enable the DFG JIT on ARMv7.  Only tested on iOS, Linux, and FreeBSD. */
#if (CPU(ARM_THUMB2) || CPU(ARM64)) && (PLATFORM(IOS_FAMILY) || OS(LINUX) || OS(FREEBSD))
#define ENABLE_DFG_JIT 1
#endif
/* Enable the DFG JIT on MIPS. */
#if CPU(MIPS)
#define ENABLE_DFG_JIT 1
#endif
#endif

/* Concurrent JS only works on 64-bit platforms because it requires that
   values get stored to atomically. This is trivially true on 64-bit platforms,
   but not true at all on 32-bit platforms where values are composed of two
   separate sub-values. */
#if ENABLE(DFG_JIT) && USE(JSVALUE64)
#define ENABLE_CONCURRENT_JS 1
#endif

#if __has_include(<System/pthread_machdep.h>)
#define HAVE_FAST_TLS 1
#endif

#if (CPU(X86_64) || CPU(ARM64)) && HAVE(FAST_TLS)
#define ENABLE_FAST_TLS_JIT 1
#endif

#if CPU(X86) || CPU(X86_64) || CPU(ARM_THUMB2) || CPU(ARM64) || CPU(MIPS)
#define ENABLE_MASM_PROBE 1
#else
#define ENABLE_MASM_PROBE 0
#endif

#if !ENABLE(JIT)
#undef ENABLE_MASM_PROBE
#define ENABLE_MASM_PROBE 0
#endif

/* If the baseline jit is not available, then disable upper tiers as well.
   The MacroAssembler::probe() is also required for supporting the upper tiers. */
#if !ENABLE(JIT) || !ENABLE(MASM_PROBE)
#undef ENABLE_DFG_JIT
#undef ENABLE_FTL_JIT
#define ENABLE_DFG_JIT 0
#define ENABLE_FTL_JIT 0
#endif

/* If the DFG jit is not available, then disable upper tiers as well: */
#if !ENABLE(DFG_JIT)
#undef ENABLE_FTL_JIT
#define ENABLE_FTL_JIT 0
#endif

/* This controls whether B3 is built. B3 is needed for FTL JIT and WebAssembly */
#if ENABLE(FTL_JIT)
#define ENABLE_B3_JIT 1
#endif

#if !defined(ENABLE_WEBASSEMBLY)
#if ENABLE(B3_JIT) && PLATFORM(COCOA) && CPU(ADDRESS64)
#define ENABLE_WEBASSEMBLY 1
#else
#define ENABLE_WEBASSEMBLY 0
#endif
#endif

/* The SamplingProfiler is the probabilistic and low-overhead profiler used by
 * JSC to measure where time is spent inside a JavaScript program.
 * In configurations other than Windows and Darwin, because layout of mcontext_t depends on standard libraries (like glibc),
 * sampling profiler is enabled if WebKit uses pthreads and glibc. */
#if !defined(ENABLE_SAMPLING_PROFILER)
#if !ENABLE(C_LOOP) && (OS(WINDOWS) || HAVE(MACHINE_CONTEXT))
#define ENABLE_SAMPLING_PROFILER 1
#else
#define ENABLE_SAMPLING_PROFILER 0
#endif
#endif

#if ENABLE(WEBASSEMBLY) && HAVE(MACHINE_CONTEXT)
#define ENABLE_WEBASSEMBLY_FAST_MEMORY 1
#endif

/* Counts uses of write barriers using sampling counters. Be sure to also
   set ENABLE_SAMPLING_COUNTERS to 1. */
#if !defined(ENABLE_WRITE_BARRIER_PROFILING)
#define ENABLE_WRITE_BARRIER_PROFILING 0
#endif

/* Logs all allocation-related activity that goes through fastMalloc or the
   JSC GC (both cells and butterflies). Also logs marking. Note that this
   isn't a completely accurate view of the heap since it doesn't include all
   butterfly resize operations, doesn't tell you what is going on with weak
   references (other than to tell you when they're marked), and doesn't
   track direct mmap() allocations or things like JIT allocation. */
#if !defined(ENABLE_ALLOCATION_LOGGING)
#define ENABLE_ALLOCATION_LOGGING 0
#endif

/* Enable verification that that register allocations are not made within generated control flow.
   Turned on for debug builds. */
#if !defined(ENABLE_DFG_REGISTER_ALLOCATION_VALIDATION) && ENABLE(DFG_JIT)
#if !defined(NDEBUG)
#define ENABLE_DFG_REGISTER_ALLOCATION_VALIDATION 1
#else
#define ENABLE_DFG_REGISTER_ALLOCATION_VALIDATION 0
#endif
#endif

/* Configure the JIT */
#if CPU(X86) && COMPILER(MSVC)
#define JSC_HOST_CALL __fastcall
#elif CPU(X86) && COMPILER(GCC_COMPATIBLE)
#define JSC_HOST_CALL __attribute__ ((fastcall))
#else
#define JSC_HOST_CALL
#endif

#if CPU(X86) && OS(WINDOWS)
#define CALLING_CONVENTION_IS_STDCALL 1
#ifndef CDECL
#if COMPILER(MSVC)
#define CDECL __cdecl
#else
#define CDECL __attribute__ ((__cdecl))
#endif
#endif
#else
#define CALLING_CONVENTION_IS_STDCALL 0
#endif

#if CPU(X86)
#define WTF_COMPILER_SUPPORTS_FASTCALL_CALLING_CONVENTION 1
#ifndef FASTCALL
#if COMPILER(MSVC)
#define FASTCALL __fastcall
#else
#define FASTCALL  __attribute__ ((fastcall))
#endif
#endif
#else
#define WTF_COMPILER_SUPPORTS_FASTCALL_CALLING_CONVENTION 0
#endif

#if ENABLE(JIT) && CALLING_CONVENTION_IS_STDCALL
#define JIT_OPERATION CDECL
#else
#define JIT_OPERATION
#endif

#if PLATFORM(IOS_FAMILY) && CPU(ARM64) && (!ENABLE(FAST_JIT_PERMISSIONS) || !CPU(ARM64E))
#define ENABLE_SEPARATED_WX_HEAP 1
#else
#define ENABLE_SEPARATED_WX_HEAP 0
#endif

/* Configure the interpreter */
#if COMPILER(GCC_COMPATIBLE)
#define HAVE_COMPUTED_GOTO 1
#endif

/* Determine if we need to enable Computed Goto Opcodes or not: */
#if HAVE(COMPUTED_GOTO) || !ENABLE(C_LOOP)
#define ENABLE_COMPUTED_GOTO_OPCODES 1
#endif

#if !ENABLE(C_LOOP) && !COMPILER(MSVC) && \
    (CPU(X86) || CPU(X86_64) || CPU(ARM64) || (CPU(ARM_THUMB2) && OS(DARWIN)))
/* This feature works by embedding the OpcodeID in the 32 bit just before the generated LLint code
   that executes each opcode. It cannot be supported by the CLoop since there's no way to embed the
   OpcodeID word in the CLoop's switch statement cases. It is also currently not implemented for MSVC.
*/
#define USE_LLINT_EMBEDDED_OPCODE_ID 1
#endif

/* Regular Expression Tracing - Set to 1 to trace RegExp's in jsc.  Results dumped at exit */
#define ENABLE_REGEXP_TRACING 0

/* Yet Another Regex Runtime - turned on by default for JIT enabled ports. */
#if !defined(ENABLE_YARR_JIT) && ENABLE(JIT)
#define ENABLE_YARR_JIT 1

/* Setting this flag compares JIT results with interpreter results. */
#define ENABLE_YARR_JIT_DEBUG 0
#endif

#if ENABLE(YARR_JIT)
#if CPU(ARM64) || (CPU(X86_64) && !OS(WINDOWS))
/* Enable JIT'ing Regular Expressions that have nested parenthesis and back references. */
#define ENABLE_YARR_JIT_ALL_PARENS_EXPRESSIONS 1
#define ENABLE_YARR_JIT_BACKREFERENCES 1
#endif
#endif

/* If either the JIT or the RegExp JIT is enabled, then the Assembler must be
   enabled as well: */
#if ENABLE(JIT) || ENABLE(YARR_JIT) || !ENABLE(C_LOOP)
#if defined(ENABLE_ASSEMBLER) && !ENABLE_ASSEMBLER
#error "Cannot enable the JIT or RegExp JIT without enabling the Assembler"
#else
#undef ENABLE_ASSEMBLER
#define ENABLE_ASSEMBLER 1
#endif
#endif

/* If the Disassembler is enabled, then the Assembler must be enabled as well: */
#if ENABLE(DISASSEMBLER)
#if defined(ENABLE_ASSEMBLER) && !ENABLE_ASSEMBLER
#error "Cannot enable the Disassembler without enabling the Assembler"
#else
#undef ENABLE_ASSEMBLER
#define ENABLE_ASSEMBLER 1
#endif
#endif

#ifndef ENABLE_EXCEPTION_SCOPE_VERIFICATION
#ifdef NDEBUG
#define ENABLE_EXCEPTION_SCOPE_VERIFICATION 0
#else
#define ENABLE_EXCEPTION_SCOPE_VERIFICATION 1
#endif
#endif

#if ENABLE(DFG_JIT) && HAVE(MACHINE_CONTEXT) && (CPU(X86) || CPU(X86_64) || CPU(ARM64))
#define ENABLE_SIGNAL_BASED_VM_TRAPS 1
#endif

#define ENABLE_POISON 0

#if !defined(USE_POINTER_PROFILING) || USE(JSVALUE32_64) || !ENABLE(JIT)
#undef USE_POINTER_PROFILING
#define USE_POINTER_PROFILING 0
#endif

/* CSS Selector JIT Compiler */
#if !defined(ENABLE_CSS_SELECTOR_JIT)
#if (CPU(X86_64) || CPU(ARM64) || (CPU(ARM_THUMB2) && PLATFORM(IOS_FAMILY))) && ENABLE(JIT) && (OS(DARWIN) || PLATFORM(GTK) || PLATFORM(WPE))
#define ENABLE_CSS_SELECTOR_JIT 1
#else
#define ENABLE_CSS_SELECTOR_JIT 0
#endif
#endif

#if CPU(ARM64E) && OS(DARWIN)
#define HAVE_FJCVTZS_INSTRUCTION 1
#endif

#if PLATFORM(IOS)
#define HAVE_APP_LINKS 1
#define USE_PASSKIT 1
#define USE_QUICK_LOOK 1
#define USE_SYSTEM_PREVIEW 1
#endif

#if PLATFORM(IOS_FAMILY) && !PLATFORM(IOSMAC)
#define HAVE_AUDIO_TOOLBOX_AUDIO_SESSION 1
#define HAVE_CELESTIAL 1
#define HAVE_CORE_ANIMATION_RENDER_SERVER 1
#endif

#if PLATFORM(COCOA)

#define USE_AVFOUNDATION 1
#define USE_PROTECTION_SPACE_AUTH_CALLBACK 1

#if !PLATFORM(WATCHOS) && !PLATFORM(APPLETV) && !PLATFORM(IOSMAC)
#define ENABLE_DATA_DETECTION 1
#endif

#if !PLATFORM(APPLETV) && !PLATFORM(IOSMAC)
#define HAVE_PARENTAL_CONTROLS 1
#endif

#if !PLATFORM(APPLETV) && (!PLATFORM(IOSMAC) || __IPHONE_OS_VERSION_MIN_REQUIRED < 130000)
#define HAVE_AVKIT 1
#endif

#if ENABLE(WEBGL)
#if PLATFORM(MAC)
#define USE_OPENGL 1
#define USE_OPENGL_ES 0
#elif PLATFORM(IOSMAC) && __has_include(<OpenGL/OpenGL.h>)
#define USE_OPENGL 1
#define USE_OPENGL_ES 0
#else
#define USE_OPENGL 0
#define USE_OPENGL_ES 1
#endif
#if PLATFORM(COCOA)
#ifndef GL_SILENCE_DEPRECATION
#define GL_SILENCE_DEPRECATION 1
#endif
#endif
#endif

#define USE_METAL 1

#if HAVE(ACCESSIBILITY)
#define USE_ACCESSIBILITY_CONTEXT_MENUS 1
#endif

#endif

#if ENABLE(WEBGL) && PLATFORM(WIN)
#define USE_OPENGL 1
#define USE_OPENGL_ES 1
#define USE_EGL 1
#endif

#if USE(TEXTURE_MAPPER) && ENABLE(GRAPHICS_CONTEXT_3D) && !defined(USE_TEXTURE_MAPPER_GL)
#define USE_TEXTURE_MAPPER_GL 1
#endif

#if CPU(ARM_THUMB2) || CPU(ARM64)
#define ENABLE_BRANCH_COMPACTION 1
#endif

#if !defined(ENABLE_THREADING_LIBDISPATCH) && HAVE(DISPATCH_H)
#define ENABLE_THREADING_LIBDISPATCH 1
#elif !defined(ENABLE_THREADING_OPENMP) && defined(_OPENMP)
#define ENABLE_THREADING_OPENMP 1
#elif !defined(THREADING_GENERIC)
#define ENABLE_THREADING_GENERIC 1
#endif

#if USE(GLIB)
#include <wtf/glib/GTypedefs.h>
#endif

#if !defined(USE_EXPORT_MACROS) && (PLATFORM(COCOA) || OS(WINDOWS))
#define USE_EXPORT_MACROS 1
#endif

#if PLATFORM(GTK) || PLATFORM(WPE)
#define USE_UNIX_DOMAIN_SOCKETS 1
#endif

#if !defined(USE_IMLANG_FONT_LINK2)
#define USE_IMLANG_FONT_LINK2 1
#endif

#if !defined(ENABLE_GC_VALIDATION) && !defined(NDEBUG)
#define ENABLE_GC_VALIDATION 1
#endif

#if !defined(ENABLE_BINDING_INTEGRITY) && !OS(WINDOWS)
#define ENABLE_BINDING_INTEGRITY 1
#endif

#if !defined(ENABLE_TREE_DEBUGGING)
#if !defined(NDEBUG)
#define ENABLE_TREE_DEBUGGING 1
#else
#define ENABLE_TREE_DEBUGGING 0
#endif
#endif

#if PLATFORM(IOS_FAMILY) || PLATFORM(MAC)
#define USE_COREMEDIA 1
#define USE_VIDEOTOOLBOX 1

#if !PLATFORM(IOSMAC)
#define HAVE_AVFOUNDATION_VIDEO_OUTPUT 1
#define HAVE_CORE_VIDEO 1
#define HAVE_MEDIA_PLAYER 1
#endif
#endif

#if PLATFORM(IOS_FAMILY) || PLATFORM(MAC)
#define HAVE_AVFOUNDATION_MEDIA_SELECTION_GROUP 1
#endif

#if PLATFORM(IOS_FAMILY) || PLATFORM(MAC)
#define HAVE_AVFOUNDATION_LEGIBLE_OUTPUT_SUPPORT 1
#define HAVE_MEDIA_ACCESSIBILITY_FRAMEWORK 1
#endif

#if PLATFORM(IOS_FAMILY) || PLATFORM(MAC)
#define HAVE_AVFOUNDATION_LOADER_DELEGATE 1
#endif

#if !PLATFORM(WIN)
#define USE_REQUEST_ANIMATION_FRAME_DISPLAY_MONITOR 1
#endif

#if PLATFORM(MAC)
#define HAVE_APPLE_GRAPHICS_CONTROL 1
#define USE_COREAUDIO 1
#endif

#if !defined(USE_ZLIB)
#define USE_ZLIB 1
#endif

#ifndef HAVE_QOS_CLASSES
#if PLATFORM(COCOA)
#define HAVE_QOS_CLASSES 1
#endif
#endif

#ifndef HAVE_VOUCHERS
#if PLATFORM(COCOA)
#define HAVE_VOUCHERS 1
#endif
#endif

#define USE_GRAMMAR_CHECKING 1

#if PLATFORM(COCOA) || PLATFORM(GTK)
#define USE_UNIFIED_TEXT_CHECKING 1
#endif
#if PLATFORM(MAC)
#define USE_AUTOMATIC_TEXT_REPLACEMENT 1
#endif

#if PLATFORM(MAC)
/* Some platforms provide UI for suggesting autocorrection. */
#define USE_AUTOCORRECTION_PANEL 1
#endif

#if PLATFORM(COCOA)
/* Some platforms use spelling and autocorrection markers to provide visual cue. On such platform, if word with marker is edited, we need to remove the marker. */
#define USE_MARKER_REMOVAL_UPON_EDITING 1
#endif

#if PLATFORM(MAC)
#define USE_INSERTION_UNDO_GROUPING 1
#endif

#if PLATFORM(MAC) || PLATFORM(IOS_FAMILY)
#define HAVE_AVASSETREADER 1
#endif

#if PLATFORM(COCOA)
#define HAVE_TIMINGDATAOPTIONS 1
#endif

#if PLATFORM(COCOA)
#define USE_AUDIO_SESSION 1
#endif

#if PLATFORM(COCOA) && !PLATFORM(IOS_FAMILY_SIMULATOR)
#define HAVE_IOSURFACE 1
#endif

#if PLATFORM(IOS_FAMILY) && !PLATFORM(IOS_FAMILY_SIMULATOR) && !PLATFORM(IOSMAC)
#define HAVE_IOSURFACE_ACCELERATOR 1
#endif

#if PLATFORM(COCOA)
#define ENABLE_RESOURCE_USAGE 1
#endif

#if PLATFORM(GTK) || PLATFORM(WPE)
#undef ENABLE_OPENTYPE_VERTICAL
#define ENABLE_OPENTYPE_VERTICAL 1
#endif

#if COMPILER(MSVC)
#undef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#undef __STDC_LIMIT_MACROS
#define __STDC_LIMIT_MACROS
#endif

#if PLATFORM(MAC)
#define HAVE_NS_ACTIVITY 1
#endif

/* Disable SharedArrayBuffers until Spectre security concerns are mitigated. */
#define ENABLE_SHARED_ARRAY_BUFFER 0

#if (OS(DARWIN) && USE(CG)) || (USE(FREETYPE) && !PLATFORM(GTK)) || (PLATFORM(WIN) && (USE(CG) || USE(CAIRO)))
#undef ENABLE_OPENTYPE_MATH
#define ENABLE_OPENTYPE_MATH 1
#endif

/* Set TARGET_OS_IPHONE to 0 by default to allow using it as a guard 
 * in cross-platform the same way as it is used in OS(DARWIN) code. */ 
#if !defined(TARGET_OS_IPHONE) && !OS(DARWIN)
#define TARGET_OS_IPHONE 0
#endif

#if PLATFORM(COCOA)
#define USE_MEDIATOOLBOX 1
#endif

#if PLATFORM(MAC) || PLATFORM(IOS_FAMILY)
#define USE_OS_LOG 1
#if USE(APPLE_INTERNAL_SDK)
#define USE_OS_STATE 1
#endif
#endif

#if PLATFORM(COCOA)
#define HAVE_SEC_TRUST_SERIALIZATION 1
#endif

#if !defined(WTF_DEFAULT_EVENT_LOOP)
#define WTF_DEFAULT_EVENT_LOOP 1
#endif

#if WTF_DEFAULT_EVENT_LOOP
#if USE(GLIB)
/* Use GLib's event loop abstraction. Primarily GTK port uses it. */
#define USE_GLIB_EVENT_LOOP 1
#elif OS(WINDOWS)
/* Use Windows message pump abstraction.
 * Even if the port is AppleWin, we use the Windows message pump system for the event loop,
 * so that USE(WINDOWS_EVENT_LOOP) && USE(CF) can be true.
 * And PLATFORM(WIN) and PLATFORM(GTK) are exclusive. If the port is GTK,
 * PLATFORM(WIN) should be false. And in that case, GLib's event loop is used.
 */
#define USE_WINDOWS_EVENT_LOOP 1
#elif PLATFORM(COCOA)
/* OS X and IOS. Use CoreFoundation & GCD abstraction. */
#define USE_COCOA_EVENT_LOOP 1
#else
#define USE_GENERIC_EVENT_LOOP 1
#endif
#endif

#if PLATFORM(MAC) || PLATFORM(IOS_FAMILY)
#define USE_MEDIAREMOTE 1
#endif

#if COMPILER(MSVC)
/* Enable strict runtime stack buffer checks. */
#pragma strict_gs_check(on)
#endif

#if PLATFORM(MAC)
#define HAVE_TOUCH_BAR 1
#define USE_DICTATION_ALTERNATIVES 1

#if defined(__LP64__)
#define ENABLE_WEB_PLAYBACK_CONTROLS_MANAGER 1
#endif
#endif /* PLATFORM(MAC) */

#if PLATFORM(COCOA) && ENABLE(WEB_RTC)
#define USE_LIBWEBRTC 1
#endif

#if (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101300) || PLATFORM(IOS) || PLATFORM(IOSMAC) || USE(GCRYPT)
#define HAVE_RSA_PSS 1
#endif

#if (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101500) || PLATFORM(IOS_FAMILY)
#define USE_SOURCE_APPLICATION_AUDIT_DATA 1
#endif

#if (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101400) || PLATFORM(IOS) || PLATFORM(IOSMAC)
#define HAVE_URL_FORMATTING 1
#endif

#if !OS(WINDOWS)
#define HAVE_STACK_BOUNDS_FOR_NEW_THREAD 1
#endif

#if (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101300) || PLATFORM(IOS) || PLATFORM(IOSMAC)
#define HAVE_AVCONTENTKEYSESSION 1
#endif

#if (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101400) || PLATFORM(IOS) || PLATFORM(IOSMAC)
#define ENABLE_ACCESSIBILITY_EVENTS 1
#define HAVE_SEC_KEY_PROXY 1
#endif

#if PLATFORM(COCOA) && USE(CA) && !PLATFORM(IOS_FAMILY_SIMULATOR)
#define USE_IOSURFACE_CANVAS_BACKING_STORE 1
#endif

/* FIXME: Should this be enabled or IOS_FAMILY, not just IOS? */
#if (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101400) || PLATFORM(IOS)
#define HAVE_FOUNDATION_WITH_SAVE_COOKIES_WITH_COMPLETION_HANDLER 1
#endif

/* FIXME: Should this be enabled for IOS_FAMILY, not just IOS? */
#if (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101400) || PLATFORM(IOS)
#define HAVE_FOUNDATION_WITH_SAME_SITE_COOKIE_SUPPORT 1
#endif

#if PLATFORM(MAC) && __MAC_OS_X_VERSION_MAX_ALLOWED < 101400
#define HAVE_NSHTTPCOOKIESTORAGE__INITWITHIDENTIFIER_WITH_INACCURATE_NULLABILITY 1
#endif

#if (PLATFORM(MAC) && __MAC_OS_X_VERSION_MAX_ALLOWED >= 101302 && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101300) || PLATFORM(IOS_FAMILY)
#define HAVE_CFNETWORK_WITH_CONTENT_ENCODING_SNIFFING_OVERRIDE 1
/* The override isn't needed on iOS family, as the default behavior is to not sniff. */
/* FIXME: This should probably be enabled on 10.13.2 and newer, not just 10.14 and newer. */
#if PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101400
#define USE_CFNETWORK_CONTENT_ENCODING_SNIFFING_OVERRIDE 1
#endif
#endif

#if (PLATFORM(MAC) && __MAC_OS_X_VERSION_MAX_ALLOWED >= 101302 && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101300) || PLATFORM(IOS_FAMILY)
#define HAVE_CFNETWORK_WITH_IGNORE_HSTS 1
#endif

#if (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101300 && __MAC_OS_X_VERSION_MAX_ALLOWED >= 101302) || PLATFORM(IOS_FAMILY)
#define HAVE_CFNETWORK_WITH_AUTO_ADDED_HTTP_HEADER_SUPPRESSION_SUPPORT 1
/* FIXME: Does this work, and is this needed on other iOS family platforms? */
#if PLATFORM(MAC) || PLATFORM(IOS)
#define USE_CFNETWORK_AUTO_ADDED_HTTP_HEADER_SUPPRESSION 1
#endif
#endif

#if PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101400
#define HAVE_OS_DARK_MODE_SUPPORT 1
#endif

#if PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101400
#define HAVE_CG_FONT_RENDERING_GET_FONT_SMOOTHING_DISABLED 1
#endif

#ifdef __APPLE__
#define HAVE_FUNC_USLEEP 1
#endif

#if PLATFORM(MAC) || PLATFORM(WPE)
/* FIXME: This really needs a descriptive name, this "new theme" was added in 2008. */
#define USE_NEW_THEME 1
#endif

#if PLATFORM(MAC)
#define HAVE_WINDOW_SERVER_OCCLUSION_NOTIFICATIONS 1
#endif

#if PLATFORM(COCOA)
#define HAVE_SEC_ACCESS_CONTROL 1
#endif

#if PLATFORM(IOS)
/* FIXME: SafariServices.framework exists on macOS. It is only used by WebKit on iOS, so the behavior is correct, but the name is misleading. */
#define HAVE_SAFARI_SERVICES_FRAMEWORK 1
#endif

#if (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101300 && !defined(__i386__)) || PLATFORM(IOS) || PLATFORM(WATCHOS)
#define HAVE_SAFE_BROWSING 1
#endif

#if PLATFORM(IOS)
#define HAVE_LINK_PREVIEW 1
#endif

#if PLATFORM(COCOA)
/* FIXME: This is a USE style macro, as it triggers the use of CFURLConnection framework stubs. */
/* FIXME: Is this still necessary? CFURLConnection isn't used on Cocoa platforms any more. */
#define ENABLE_SEC_ITEM_SHIM 1
#endif

#if (PLATFORM(IOS_FAMILY) || (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101400))
#define HAVE_ACCESSIBILITY_SUPPORT 1
#endif

#if PLATFORM(MAC)
#define ENABLE_FULL_KEYBOARD_ACCESS 1
#endif

#if PLATFORM(IOS_FAMILY) || (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101400)
#define HAVE_AUTHORIZATION_STATUS_FOR_MEDIA_TYPE 1
#endif

#if (PLATFORM(MAC) && (__MAC_OS_X_VERSION_MIN_REQUIRED == 101200 || (__MAC_OS_X_VERSION_MIN_REQUIRED >= 101400 && __MAC_OS_X_VERSION_MAX_ALLOWED >= 101404))) || (PLATFORM(IOS) && __IPHONE_OS_VERSION_MIN_REQUIRED >= 120000 && __IPHONE_OS_VERSION_MAX_ALLOWED >= 120200) || (PLATFORM(WATCHOS) && __WATCH_OS_VERSION_MIN_REQUIRED >= 50000 && __WATCH_OS_VERSION_MAX_ALLOWED >= 50200) || (PLATFORM(APPLETV) && __TV_OS_VERSION_MIN_REQUIRED >= 120000 && __TV_OS_VERSION_MAX_ALLOWED >= 120200)
#define HAVE_CFNETWORK_OVERRIDE_SESSION_COOKIE_ACCEPT_POLICY 1
#endif

#if (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101500) || (PLATFORM(IOS_FAMILY) && __IPHONE_OS_VERSION_MIN_REQUIRED >= 130000)
#define HAVE_CFNETWORK_NSURLSESSION_STRICTRUSTEVALUATE 1
#endif

#if (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101500) || (PLATFORM(IOS_FAMILY) && __IPHONE_OS_VERSION_MIN_REQUIRED >= 130000)
#define HAVE_CFNETWORK_NEGOTIATED_SSL_PROTOCOL_CIPHER 1
#endif

#if PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101500
#define HAVE_CSCHECKFIXDISABLE 1
#endif

#if PLATFORM(MAC)
#define ENABLE_MONOSPACE_FONT_EXCEPTION (__MAC_OS_X_VERSION_MIN_REQUIRED < 101500)
#elif PLATFORM(IOS_FAMILY)
#define ENABLE_MONOSPACE_FONT_EXCEPTION (__IPHONE_OS_VERSION_MIN_REQUIRED < 130000)
#endif
