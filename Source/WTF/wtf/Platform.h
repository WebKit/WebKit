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

/* ==== Platform adaptation macros: these describe properties of the target environment. ==== */

/* CPU() - the target CPU architecture */
#include <wtf/PlatformCPU.h>

/* OS() - underlying operating system; only to be used for mandated low-level services like
   virtual memory, not to choose a GUI toolkit */
#include <wtf/PlatformOS.h>

/* PLATFORM() - handles OS, operating environment, graphics API, and
   CPU. This macro will be phased out in favor of platform adaptation
   macros, policy decision macros, and top-level port definitions. */
#include <wtf/PlatformLegacy.h>

/* HAVE() - specific system features (headers, functions or similar) that are present or not */
#define HAVE(WTF_FEATURE) (defined HAVE_##WTF_FEATURE  && HAVE_##WTF_FEATURE)


/* ==== Policy decision macros: these define policy choices for a particular port. ==== */

/* USE() - use a particular third-party library or optional OS service */
#define USE(WTF_FEATURE) (defined USE_##WTF_FEATURE  && USE_##WTF_FEATURE)
/* ENABLE() - turn on a specific feature of WebKit */
#define ENABLE(WTF_FEATURE) (defined ENABLE_##WTF_FEATURE  && ENABLE_##WTF_FEATURE)


#if PLATFORM(COCOA)
#if defined __has_include && __has_include(<CoreFoundation/CFPriv.h>)
#define USE_APPLE_INTERNAL_SDK 1
#endif
#endif

/* MIPS requires allocators to use aligned memory */
#if CPU(MIPS) || CPU(MIPS64)
#define USE_ARENA_ALLOC_ALIGNMENT_INTEGER 1
#endif

/*ARMv5TE requires allocators to use aligned memory*/
#if defined(__ARM_ARCH_5E__) || defined(__ARM_ARCH_5TE__) || defined(__ARM_ARCH_5TEJ__)
#define USE_ARENA_ALLOC_ALIGNMENT_INTEGER 1
#endif

/*ARMv5TE requires allocators to use aligned memory*/
#if defined(__TARGET_ARCH_5E) || defined(__TARGET_ARCH_5TE) || defined(__TARGET_ARCH_5TEJ)
#define USE_ARENA_ALLOC_ALIGNMENT_INTEGER 1
#endif

#if CPU(ARM_NEON)
/* All NEON intrinsics usage can be disabled by this macro. */
#define HAVE_ARM_NEON_INTRINSICS 1
#endif

/* FIXME: This should be renamed to WTF_CPU_ARM_IDIV_INSTRUCTIONS and moved to CPU.h */
#if defined(__ARM_ARCH_EXT_IDIV__) || CPU(APPLE_ARMV7S)
#define HAVE_ARM_IDIV_INSTRUCTIONS 1
#endif



/* Operating environments */


/* FIXME: Rename WTF_CPU_EFFECTIVE_ADDRESS_WIDTH to WTF_OS_EFFECTIVE_ADDRESS_WIDTH, as it is an OS feature, not a CPU feature. */
#if CPU(ADDRESS64)
#if OS(DARWIN) && CPU(ARM64)
#define WTF_CPU_EFFECTIVE_ADDRESS_WIDTH 36
#else
/* We strongly assume that effective address width is <= 48 in 64bit architectures (e.g. NaN boxing). */
#define WTF_CPU_EFFECTIVE_ADDRESS_WIDTH 48
#endif
#else
#define WTF_CPU_EFFECTIVE_ADDRESS_WIDTH 32
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

/* FIXME: ASSERT_ENABLED should defined in different, perhaps its own, file. */
/* ASSERT_ENABLED should be true if we want the current compilation unit to
   do debug assertion checks unconditionally (e.g. treat a debug ASSERT
   like a RELEASE_ASSERT.
*/
#ifndef ASSERT_ENABLED
#ifdef NDEBUG
#define ASSERT_ENABLED 0
#else
#define ASSERT_ENABLED 1
#endif
#endif



#if PLATFORM(COCOA)
#define USE_CG 1
#endif

#if PLATFORM(COCOA)
#define USE_CA 1
#endif

#if PLATFORM(GTK) || PLATFORM(WPE)
#define USE_GLIB 1
#endif

#if PLATFORM(GTK) || PLATFORM(WPE)
#define USE_FREETYPE 1
#endif

#if PLATFORM(GTK) || PLATFORM(WPE)
#define USE_HARFBUZZ 1
#endif

#if PLATFORM(GTK) || PLATFORM(WPE)
#define USE_SOUP 1
#endif

#if PLATFORM(GTK) || PLATFORM(WPE)
#define USE_WEBP 1
#endif

#if PLATFORM(GTK)
#define GLIB_VERSION_MIN_REQUIRED GLIB_VERSION_2_36
#define GDK_VERSION_MIN_REQUIRED GDK_VERSION_3_6
#endif

#if PLATFORM(WPE)
#define GLIB_VERSION_MIN_REQUIRED GLIB_VERSION_2_40
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
#endif

#if PLATFORM(COCOA)
#define USE_CF 1
#endif

#if PLATFORM(COCOA) || (PLATFORM(GTK) || PLATFORM(WPE))
#define USE_FILE_LOCK 1
#endif

#if PLATFORM(COCOA)
#define USE_FOUNDATION 1
#endif

#if PLATFORM(COCOA)
#define USE_NETWORK_CFDATA_ARRAY_CALLBACK 1
#endif


#if PLATFORM(COCOA)
/* Cocoa defines a series of platform macros for debugging. */
/* Some of them are really annoying because they use common names (e.g. check()). */
/* Disable those macros so that we are not limited in how we name methods and functions. */
#undef __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORES
#define __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORES 0
#endif

#if PLATFORM(MAC)
#define HAVE_RUNLOOP_TIMER 1
#endif

#if PLATFORM(MAC)
#define HAVE_SEC_KEYCHAIN 1
#endif

#if PLATFORM(MAC)
#define HAVE_HISERVICES 1
#endif

#if PLATFORM(MAC)
#define USE_APPKIT 1
#endif

#if PLATFORM(MAC)
#define USE_PASSKIT 1
#endif

#if PLATFORM(MAC)
#define HAVE_NETWORK_EXTENSION 1
#endif

#if PLATFORM(MAC)
#define USE_PLUGIN_HOST_PROCESS 1
#endif

#if PLATFORM(IOS_FAMILY)
#define HAVE_NETWORK_EXTENSION 1
#endif

#if PLATFORM(IOS_FAMILY)
#define HAVE_READLINE 1
#endif

#if PLATFORM(IOS_FAMILY)
#define USE_UIKIT_EDITING 1
#endif

#if PLATFORM(IOS_FAMILY)
#define USE_WEB_THREAD 1
#endif

#if PLATFORM(IOS_FAMILY) && CPU(ARM_NEON)
#undef HAVE_ARM_NEON_INTRINSICS
#define HAVE_ARM_NEON_INTRINSICS 0
#endif

#if !defined(HAVE_PDFHOSTVIEWCONTROLLER_SNAPSHOTTING) && PLATFORM(IOS)
#define HAVE_PDFHOSTVIEWCONTROLLER_SNAPSHOTTING 1
#endif

#if !defined(HAVE_VISIBILITY_PROPAGATION_VIEW) && PLATFORM(IOS_FAMILY)
#define HAVE_VISIBILITY_PROPAGATION_VIEW 1
#endif

#if !defined(HAVE_UISCENE) && PLATFORM(IOS_FAMILY)
#define HAVE_UISCENE 1
#endif

#if !defined(HAVE_AVSTREAMSESSION) && PLATFORM(MAC)
#define HAVE_AVSTREAMSESSION 1
#endif

#if !defined(HAVE_PASSKIT_API_TYPE) && (PLATFORM(IOS_FAMILY) || PLATFORM(MAC))
#define HAVE_PASSKIT_API_TYPE 1
#endif

#if !defined(HAVE_PASSKIT_BOUND_INTERFACE_IDENTIFIER) && (PLATFORM(IOS) || (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101400))
#define HAVE_PASSKIT_BOUND_INTERFACE_IDENTIFIER 1
#endif

#if !defined(USE_UIKIT_KEYBOARD_ADDITIONS) && (PLATFORM(IOS) || PLATFORM(MACCATALYST))
#define USE_UIKIT_KEYBOARD_ADDITIONS 1
#endif


#if !defined(HAVE_ACCESSIBILITY) && (PLATFORM(COCOA) || PLATFORM(WIN) || PLATFORM(GTK) || PLATFORM(WPE))
#define HAVE_ACCESSIBILITY 1
#endif

/* FIXME: Remove after CMake build enabled on Darwin */
#if OS(DARWIN)
#define HAVE_ERRNO_H 1
#endif

#if OS(DARWIN)
#define HAVE_LANGINFO_H 1
#endif

#if OS(DARWIN)
#define HAVE_LOCALTIME_R 1
#endif

#if OS(DARWIN)
#define HAVE_MMAP 1
#endif

#if OS(DARWIN)
#define HAVE_REGEX_H 1
#endif

#if OS(DARWIN)
#define HAVE_SIGNAL_H 1
#endif

#if OS(DARWIN)
#define HAVE_STAT_BIRTHTIME 1
#endif

#if OS(DARWIN)
#define HAVE_STRINGS_H 1
#endif

#if OS(DARWIN)
#define HAVE_STRNSTR 1
#endif

#if OS(DARWIN)
#define HAVE_SYS_PARAM_H 1
#endif

#if OS(DARWIN)
#define HAVE_SYS_TIME_H 1
#endif

#if OS(DARWIN)
#define HAVE_TM_GMTOFF 1
#endif

#if OS(DARWIN)
#define HAVE_TM_ZONE 1
#endif

#if OS(DARWIN)
#define HAVE_TIMEGM 1
#endif

#if OS(DARWIN)
#define HAVE_PTHREAD_MAIN_NP 1
#endif

#if OS(DARWIN) && (CPU(X86_64) || CPU(ARM64))
#define HAVE_INT128_T 1
#endif

#if OS(UNIX)
#define USE_PTHREADS 1
#endif

#if OS(UNIX) && !OS(FUCHSIA)
#define HAVE_RESOURCE_H 1
#endif

#if OS(UNIX) && !OS(FUCHSIA)
#define HAVE_PTHREAD_SETSCHEDPARAM 1
#endif

#if OS(DARWIN)
#define HAVE_DISPATCH_H 1
#endif

#if OS(DARWIN)
#define HAVE_MADV_FREE 1
#endif

#if OS(DARWIN)
#define HAVE_MADV_FREE_REUSE 1
#endif

#if OS(DARWIN)
#define HAVE_MADV_DONTNEED 1
#endif

#if OS(DARWIN)
#define HAVE_MERGESORT 1
#endif

#if OS(DARWIN)
#define HAVE_PTHREAD_SETNAME_NP 1
#endif

#if OS(DARWIN)
#define HAVE_READLINE 1
#endif

#if OS(DARWIN)
#define HAVE_SYS_TIMEB_H 1
#endif

#if OS(DARWIN)
#define HAVE_AUDIT_TOKEN 1
#endif

#if OS(DARWIN) && __has_include(<mach/mach_exc.defs>) && !PLATFORM(GTK)
#define HAVE_MACH_EXCEPTIONS 1
#endif

#if OS(DARWIN) && !PLATFORM(GTK)
#define USE_ACCELERATE 1
#endif

#if OS(DARWIN) && !PLATFORM(IOS_FAMILY)
#define HAVE_HOSTED_CORE_ANIMATION 1
#endif


#if OS(DARWIN) || OS(FUCHSIA) || ((OS(FREEBSD) || defined(__GLIBC__) || defined(__BIONIC__)) && (CPU(X86) || CPU(X86_64) || CPU(ARM) || CPU(ARM64) || CPU(MIPS)))
#define HAVE_MACHINE_CONTEXT 1
#endif

#if OS(DARWIN) || (OS(LINUX) && defined(__GLIBC__) && !defined(__UCLIBC__) && !CPU(MIPS))
#define HAVE_BACKTRACE 1
#endif

#if OS(DARWIN) || OS(LINUX)
#if PLATFORM(GTK)
#if defined(__GLIBC__) && !defined(__UCLIBC__) && !CPU(MIPS)
#define HAVE_BACKTRACE_SYMBOLS 1
#endif
#endif /* PLATFORM(GTK) */
#define HAVE_DLADDR 1
#endif /* OS(DARWIN) || OS(LINUX) */


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

#if __has_include(<System/pthread_machdep.h>)
#define HAVE_FAST_TLS 1
#endif

#if COMPILER(GCC_COMPATIBLE)
#define HAVE_COMPUTED_GOTO 1
#endif

/* FIXME: This name should be more specific if it is only for use with CallFrame* */
/* Use __builtin_frame_address(1) to get CallFrame* */
#if COMPILER(GCC_COMPATIBLE) && (CPU(ARM64) || CPU(X86_64))
#define USE_BUILTIN_FRAME_ADDRESS 1
#endif

#if CPU(ARM64E) && OS(DARWIN)
#define HAVE_FJCVTZS_INSTRUCTION 1
#endif

#if PLATFORM(IOS)
#define HAVE_APP_LINKS 1
#endif

#if PLATFORM(IOS)
#define USE_PASSKIT 1
#endif

#if PLATFORM(IOS)
#define USE_QUICK_LOOK 1
#endif

#if PLATFORM(IOS)
#define USE_SYSTEM_PREVIEW 1
#endif

#if PLATFORM(IOS_FAMILY) && !PLATFORM(MACCATALYST)
#define HAVE_CELESTIAL 1
#endif

#if PLATFORM(IOS_FAMILY) && !PLATFORM(MACCATALYST)
#define HAVE_CORE_ANIMATION_RENDER_SERVER 1
#endif

#if PLATFORM(IOS_FAMILY) && !PLATFORM(MACCATALYST) && !PLATFORM(APPLETV)
#define HAVE_PARENTAL_CONTROLS_WITH_UNBLOCK_HANDLER 1
#endif

#if PLATFORM(COCOA)
#define USE_AVFOUNDATION 1
#endif

#if PLATFORM(COCOA)
#define USE_PROTECTION_SPACE_AUTH_CALLBACK 1
#endif

/* FIXME: Enable HAVE_PARENTAL_CONTROLS for watchOS Simulator once rdar://problem/54608386 is resolved */
#if PLATFORM(COCOA) && (!PLATFORM(APPLETV) && (!PLATFORM(WATCHOS) || !PLATFORM(IOS_FAMILY_SIMULATOR)))
#define HAVE_PARENTAL_CONTROLS 1
#endif

#if PLATFORM(COCOA) && !PLATFORM(APPLETV)
#define HAVE_AVKIT 1
#endif

#if PLATFORM(COCOA)
#define USE_METAL 1
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

#if PLATFORM(COCOA)
#define USE_COREMEDIA 1
#endif

#if PLATFORM(COCOA)
#define USE_VIDEOTOOLBOX 1
#endif

#if PLATFORM(COCOA)
#define HAVE_AVFOUNDATION_VIDEO_OUTPUT 1
#endif

#if PLATFORM(COCOA)
#define HAVE_CORE_VIDEO 1
#endif

#if PLATFORM(COCOA)
#define HAVE_MEDIA_PLAYER 1
#endif

#if PLATFORM(COCOA)
#define HAVE_AVFOUNDATION_MEDIA_SELECTION_GROUP 1
#endif

#if PLATFORM(COCOA)
#define HAVE_AVFOUNDATION_LEGIBLE_OUTPUT_SUPPORT 1
#endif

#if PLATFORM(COCOA)
#define HAVE_MEDIA_ACCESSIBILITY_FRAMEWORK 1
#endif

#if PLATFORM(COCOA)
#define HAVE_AVFOUNDATION_LOADER_DELEGATE 1
#endif

#if !PLATFORM(WIN)
#define USE_REQUEST_ANIMATION_FRAME_DISPLAY_MONITOR 1
#endif

#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
#define HAVE_APPLE_GRAPHICS_CONTROL 1
#endif

#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
#define HAVE_NSCURSOR 1
#endif

#if PLATFORM(MAC)
#define USE_COREAUDIO 1
#endif

#if !defined(USE_ZLIB)
#define USE_ZLIB 1
#endif

#if !defined(HAVE_QOS_CLASSES) && PLATFORM(COCOA)
#define HAVE_QOS_CLASSES 1
#endif

#if !defined(HAVE_VOUCHERS) && PLATFORM(COCOA)
#define HAVE_VOUCHERS 1
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

#if PLATFORM(COCOA)
#define HAVE_AVASSETREADER 1
#endif

#if PLATFORM(COCOA)
#define USE_AUDIO_SESSION 1
#endif

#if PLATFORM(COCOA)
#define HAVE_IOSURFACE 1
#endif

#if PLATFORM(IOS_FAMILY) && !PLATFORM(IOS_FAMILY_SIMULATOR)
#define HAVE_IOSURFACE_COREIMAGE_SUPPORT 1
#endif

#if PLATFORM(IOS_FAMILY) && !PLATFORM(IOS_FAMILY_SIMULATOR) && !PLATFORM(MACCATALYST)
#define HAVE_IOSURFACE_ACCELERATOR 1
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


/* Set TARGET_OS_IPHONE to 0 by default to allow using it as a guard 
 * in cross-platform the same way as it is used in OS(DARWIN) code. */ 
#if !defined(TARGET_OS_IPHONE) && !OS(DARWIN)
#define TARGET_OS_IPHONE 0
#endif

#if PLATFORM(COCOA)
#define USE_MEDIATOOLBOX 1
#endif

#if PLATFORM(COCOA)
#define USE_OS_LOG 1
#endif

#if PLATFORM(COCOA) && USE(APPLE_INTERNAL_SDK)
#define USE_OS_STATE 1
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

#if PLATFORM(COCOA)
#define USE_MEDIAREMOTE 1
#endif

#if COMPILER(MSVC)
/* Enable strict runtime stack buffer checks. */
#pragma strict_gs_check(on)
#endif

#if PLATFORM(MAC)
#define HAVE_TOUCH_BAR 1
#endif

#if PLATFORM(MAC)
#define USE_DICTATION_ALTERNATIVES 1
#endif


#if PLATFORM(MAC) || PLATFORM(IOS) || PLATFORM(MACCATALYST) || USE(GCRYPT)
#define HAVE_RSA_PSS 1
#endif

#if (PLATFORM(MAC) && USE(APPLE_INTERNAL_SDK) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101500) || PLATFORM(IOS_FAMILY)
#define USE_SOURCE_APPLICATION_AUDIT_DATA 1
#endif

#if (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101500) || (PLATFORM(IOS_FAMILY) && __IPHONE_OS_VERSION_MIN_REQUIRED >= 130000)
#define HAVE_HSTS_STORAGE_PATH 1
#endif

#if (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101400) || PLATFORM(IOS) || PLATFORM(MACCATALYST)
#define HAVE_URL_FORMATTING 1
#endif

#if !OS(WINDOWS)
#define HAVE_STACK_BOUNDS_FOR_NEW_THREAD 1
#endif

#if PLATFORM(MAC) || PLATFORM(IOS) || PLATFORM(MACCATALYST)
#define HAVE_AVCONTENTKEYSESSION 1
#endif

#if (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101400) || PLATFORM(IOS) || PLATFORM(MACCATALYST)
#define HAVE_SEC_KEY_PROXY 1
#endif

#if PLATFORM(COCOA) && USE(CA)
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

#if PLATFORM(COCOA)
#define HAVE_CFNETWORK_WITH_CONTENT_ENCODING_SNIFFING_OVERRIDE 1
/* The override isn't needed on iOS family, as the default behavior is to not sniff. */
/* FIXME: This should probably be enabled on 10.13.2 and newer, not just 10.14 and newer. */
#if PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101400
#define USE_CFNETWORK_CONTENT_ENCODING_SNIFFING_OVERRIDE 1
#endif
#endif

#if (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101400) || (PLATFORM(IOS_FAMILY) && __IPHONE_OS_VERSION_MIN_REQUIRED >= 130000) || PLATFORM(GTK)
#define HAVE_OS_DARK_MODE_SUPPORT 1
#endif

#if PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101400
#define HAVE_CG_FONT_RENDERING_GET_FONT_SMOOTHING_DISABLED 1
#endif

#if (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101500) || PLATFORM(IOS_FAMILY)
#define HAVE_CA_WHERE_ADDITIVE_TRANSFORMS_ARE_REVERSED 1
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

#if PLATFORM(MAC) || PLATFORM(IOS) || PLATFORM(WATCHOS)
#define HAVE_SAFE_BROWSING 1
#endif

#if PLATFORM(IOS)
#define HAVE_LINK_PREVIEW 1
#endif

#if (PLATFORM(IOS_FAMILY) || (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101400))
#define HAVE_ACCESSIBILITY_SUPPORT 1
#endif

#if PLATFORM(IOS_FAMILY) && __IPHONE_OS_VERSION_MIN_REQUIRED >= 130000 && __IPHONE_OS_VERSION_MAX_ALLOWED >= 130100
#define HAVE_ACCESSIBILITY_BUNDLES_PATH 1
#endif

#if PLATFORM(IOS_FAMILY) || (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101400)
#define HAVE_AUTHORIZATION_STATUS_FOR_MEDIA_TYPE 1
#endif

#if (PLATFORM(MAC) && (__MAC_OS_X_VERSION_MIN_REQUIRED >= 101400 && __MAC_OS_X_VERSION_MAX_ALLOWED >= 101404)) || (PLATFORM(IOS) && __IPHONE_OS_VERSION_MIN_REQUIRED >= 120000 && __IPHONE_OS_VERSION_MAX_ALLOWED >= 120200) || PLATFORM(WATCHOS) || PLATFORM(APPLETV)
#define HAVE_CFNETWORK_OVERRIDE_SESSION_COOKIE_ACCEPT_POLICY 1
#endif

#if (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101500) || (PLATFORM(IOS_FAMILY) && __IPHONE_OS_VERSION_MIN_REQUIRED >= 130000)
#define HAVE_CFNETWORK_NSURLSESSION_STRICTRUSTEVALUATE 1
#endif

#if (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101500) || PLATFORM(IOS_FAMILY)
#define HAVE_CFNETWORK_NEGOTIATED_SSL_PROTOCOL_CIPHER 1
#endif

#if (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101600) || (PLATFORM(IOS_FAMILY) && __IPHONE_OS_VERSION_MIN_REQUIRED >= 140000)
#define HAVE_CFNETWORK_METRICS_APIS_V4 1
#endif

#if PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101500
#define HAVE_CSCHECKFIXDISABLE 1
#endif

#if (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101400) || PLATFORM(IOS_FAMILY)
#define HAVE_SANDBOX_ISSUE_MACH_EXTENSION_TO_PROCESS_BY_AUDIT_TOKEN 1
#endif

#if (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101400) || PLATFORM(IOS_FAMILY)
#define HAVE_SANDBOX_ISSUE_READ_EXTENSION_TO_PROCESS_BY_AUDIT_TOKEN 1
#endif

#if (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101500) || (PLATFORM(IOS_FAMILY) && __IPHONE_OS_VERSION_MIN_REQUIRED >= 130000)
#define HAVE_MDNS_FAST_REGISTRATION 1
#endif

#if (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101400) || PLATFORM(IOS_FAMILY)
#define HAVE_DISALLOWABLE_USER_INSTALLED_FONTS 1
#endif

#if (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101500) || (PLATFORM(IOS_FAMILY) && __IPHONE_OS_VERSION_MIN_REQUIRED >= 130000)
#define HAVE_CTFONTCREATEFORCHARACTERSWITHLANGUAGEANDOPTION 1
#endif

#if (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101500) || (PLATFORM(IOS_FAMILY) && __IPHONE_OS_VERSION_MIN_REQUIRED >= 130000)
#define HAVE_CTFONTTRANSFORMGLYPHSWITHLANGUAGE 1
#endif

#if PLATFORM(IOS_FAMILY) && __IPHONE_OS_VERSION_MIN_REQUIRED >= 130000
#define HAVE_ARKIT_QUICK_LOOK_PREVIEW_ITEM 1
#endif

#if PLATFORM(IOS_FAMILY) && __IPHONE_OS_VERSION_MIN_REQUIRED >= 130000
#define HAVE_UI_WK_DOCUMENT_CONTEXT 1
#endif

#if PLATFORM(MACCATALYST)
#define HAVE_HOVER_GESTURE_RECOGNIZER 1
#endif

#if PLATFORM(MACCATALYST)
#define HAVE_UI_PARALLAX_TRANSITION_GESTURE_RECOGNIZER 1
#endif

#if (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101500) || PLATFORM(IOS_FAMILY)
#define HAVE_ALLOWS_SENSITIVE_LOGGING 1
#endif

#if (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101500) || PLATFORM(IOS_FAMILY)
#define HAVE_FAIRPLAYSTREAMING_CENC_INITDATA 1
#endif

#if (PLATFORM(IOS) && __IPHONE_OS_VERSION_MIN_REQUIRED >= 130000) || PLATFORM(WATCHOS) || PLATFORM(APPLETV)
#define HAVE_UI_SCROLL_VIEW_INDICATOR_FLASHING_SPI 1
#endif

#if (PLATFORM(IOS) && __IPHONE_OS_VERSION_MIN_REQUIRED >= 130000) || PLATFORM(WATCHOS) || PLATFORM(APPLETV)
#define HAVE_APP_LINKS_WITH_ISENABLED 1
#endif

#if (PLATFORM(IOS) && __IPHONE_OS_VERSION_MIN_REQUIRED >= 130000)
#define HAVE_ROUTE_SHARING_POLICY_LONG_FORM_VIDEO 1
#endif

#if (PLATFORM(IOS) && __IPHONE_OS_VERSION_MIN_REQUIRED >= 120000 && !PLATFORM(IOS_SIMULATOR))
#define HAVE_DEVICE_MANAGEMENT 1
#endif

#if PLATFORM(MAC) && __MAC_OS_X_VERSION_MAX_ALLOWED < 101500
#define USE_REALPATH_FOR_DLOPEN_PREFLIGHT 1
#endif

#if (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101500) || (PLATFORM(IOS_FAMILY) && __IPHONE_OS_VERSION_MIN_REQUIRED >= 130000) || PLATFORM(WATCHOS) || PLATFORM(APPLETV)
#define HAVE_NSURLSESSION_WEBSOCKET 1
#endif

#if (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101500) || (PLATFORM(IOS) && __IPHONE_OS_VERSION_MIN_REQUIRED >= 130000) || PLATFORM(WATCHOS) || PLATFORM(APPLETV)
#define HAVE_AVPLAYER_RESOURCE_CONSERVATION_LEVEL 1
#endif

#if (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101400) || (PLATFORM(IOS) && __IPHONE_OS_VERSION_MIN_REQUIRED >= 120000) || PLATFORM(WATCHOS) || PLATFORM(APPLETV)
#define HAVE_CORETEXT_AUTO_OPTICAL_SIZING 1
#endif

#if (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED < 101500) || (PLATFORM(IOS) && __IPHONE_OS_VERSION_MIN_REQUIRED < 130000)
#define HAVE_NSFONT_WITH_OPTICAL_SIZING_BUG 1
#endif

#if (PLATFORM(IOS) || (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101500))
#define HAVE_APP_SSO 1
#endif

#if (PLATFORM(IOS) && __IPHONE_OS_VERSION_MIN_REQUIRED >= 130000 || PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101500) || PLATFORM(WATCHOS) || PLATFORM(APPLETV) || PLATFORM(MACCATALYST)
#define HAVE_TLS_PROTOCOL_VERSION_T 1
#endif

#if PLATFORM(IOS) || (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101400) || PLATFORM(WATCHOS) || PLATFORM(APPLETV) || PLATFORM(MACCATALYST)
#define HAVE_SEC_TRUST_EVALUATE_WITH_ERROR 1
#endif

#if PLATFORM(IOS) || PLATFORM(MACCATALYST)
#define USE_UICONTEXTMENU 1
#endif

#if PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101500
#define HAVE_SUBVIEWS_IVAR_SPI 1
#endif

#if PLATFORM(MAC) && __MAC_OS_X_VERSION_MAX_ALLOWED < 101500
#define HAVE_SUBVIEWS_IVAR_DECLARED_BY_SDK 1
#endif

#if PLATFORM(MAC) || (PLATFORM(IOS_FAMILY) && __IPHONE_OS_VERSION_MIN_REQUIRED >= 110000) || PLATFORM(WATCHOS) || PLATFORM(APPLETV)
#define USE_PLATFORM_SYSTEM_FALLBACK_LIST 1
#endif

#if PLATFORM(MAC) && __MAC_OS_X_VERSION_MAX_ALLOWED >= 101500
#define HAVE_AX_CLIENT_TYPE 1
#endif

#if (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101500) || (PLATFORM(IOS) && __IPHONE_OS_VERSION_MIN_REQUIRED >= 130000) || PLATFORM(WATCHOS) || PLATFORM(APPLETV)
#define HAVE_DESIGN_SYSTEM_UI_FONTS 1
#endif

#if (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101400) || (PLATFORM(IOS) && !PLATFORM(IOS_SIMULATOR))
#define HAVE_DEVICE_IDENTITY 1
#endif

#if (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101500) || PLATFORM(IOS_FAMILY)
#define HAVE_DATA_PROTECTION_KEYCHAIN 1
#endif

#if (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101500) || (PLATFORM(IOS_FAMILY) && !PLATFORM(IOS_FAMILY_SIMULATOR))
#define HAVE_NEAR_FIELD 1
#endif

#if PLATFORM(IOS_FAMILY) || (!(defined(USE_SYSTEM_MALLOC) && USE_SYSTEM_MALLOC) && (OS(LINUX) && (PLATFORM(GTK) || PLATFORM(WPE))))
#define USE_BMALLOC_MEMORY_FOOTPRINT_API 1
#endif

#if (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101400) || PLATFORM(IOS_FAMILY)
#define HAVE_OS_SIGNPOST 1
#endif



#if USE(APPLE_INTERNAL_SDK) && __has_include(<WebKitAdditions/AdditionalPlatform.h>)
#include <WebKitAdditions/AdditionalPlatform.h>
#endif

#if USE(APPLE_INTERNAL_SDK) && __has_include(<WebKitAdditions/AdditionalFeatureDefines.h>)
#include <WebKitAdditions/AdditionalFeatureDefines.h>
#endif

/* __PLATFORM_INDIRECT__ ensures that users #include <wtf/Platform.h> rather than wtf/FeatureDefines.h directly. */
#define __PLATFORM_INDIRECT__

/* Include feature macros */
#include <wtf/PlatformEnable.h>

#undef __PLATFORM_INDIRECT__



/* FIXME: The following are currenly positioned at the bottom of this file as they are currently dependent on ENABLE macros. */

/* FIXME: Remove dependence on ENABLE(WEB_RTC). */
#if PLATFORM(COCOA) && ENABLE(WEB_RTC)
#define USE_LIBWEBRTC 1
#endif

/* FIXME: This is used to "turn on a specific feature of WebKit", so should be converted to an ENABLE macro. */
/* This feature works by embedding the OpcodeID in the 32 bit just before the generated LLint code
   that executes each opcode. It cannot be supported by the CLoop since there's no way to embed the
   OpcodeID word in the CLoop's switch statement cases. It is also currently not implemented for MSVC.
*/
#if !defined(USE_LLINT_EMBEDDED_OPCODE_ID) && !ENABLE(C_LOOP) && !COMPILER(MSVC) && \
    (CPU(X86) || CPU(X86_64) || CPU(ARM64) || (CPU(ARM_THUMB2) && OS(DARWIN)))
#define USE_LLINT_EMBEDDED_OPCODE_ID 1
#endif


#if PLATFORM(COCOA)
#if ENABLE(WEBGL)

/* USE_ANGLE=1 uses ANGLE for the WebGL backend.
   It replaces USE_OPENGL, USE_OPENGL_ES and USE_EGL. */
#if PLATFORM(MAC) || (PLATFORM(MACCATALYST) && __has_include(<OpenGL/OpenGL.h>))
#define USE_OPENGL 1
#define USE_OPENGL_ES 0
#define USE_ANGLE 0
#else
#define USE_OPENGL 0
#define USE_OPENGL_ES 1
#define USE_ANGLE 0
#endif

#ifndef GL_SILENCE_DEPRECATION
#define GL_SILENCE_DEPRECATION 1
#endif

#if USE(OPENGL) && !defined(HAVE_OPENGL_4)
#define HAVE_OPENGL_4 1
#endif

#if USE(OPENGL_ES) && !defined(HAVE_OPENGL_ES_3)
#define HAVE_OPENGL_ES_3 1
#endif

#if USE_ANGLE && (USE_OPENGL || USE_OPENGL_ES)
#error USE_ANGLE is incompatible with USE_OPENGL and USE_OPENGL_ES
#endif

#endif /* ENABLE(WEBGL) */
#endif /* PLATFORM(COCOA) */

#if ENABLE(WEBGL)
#if !defined(USE_ANGLE)
#define USE_ANGLE 0
#endif

#if (USE_ANGLE && (USE_OPENGL || USE_OPENGL_ES || (defined(USE_EGL) && USE_EGL))) && !USE(TEXTURE_MAPPER)
#error USE_ANGLE is incompatible with USE_OPENGL, USE_OPENGL_ES and USE_EGL
#endif
#endif

#if USE(TEXTURE_MAPPER) && ENABLE(GRAPHICS_CONTEXT_GL) && !defined(USE_TEXTURE_MAPPER_GL)
#define USE_TEXTURE_MAPPER_GL 1
#endif

/* FIXME: This is used to "turn on a specific feature of WebKit", so should be converted to an ENABLE macro. */
#if PLATFORM(COCOA) && ENABLE(ACCESSIBILITY)
#define USE_ACCESSIBILITY_CONTEXT_MENUS 1
#endif


/* FIXME: These calling convention macros should move to their own file. They are at the bottom currently because they depend on FeatureDefines.h */

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
