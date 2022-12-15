/*
 * Copyright (C) 2006-2022 Apple Inc. All rights reserved.
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

#ifndef WTF_PLATFORM_GUARD_AGAINST_INDIRECT_INCLUSION
#error "Please #include <wtf/Platform.h> instead of this file directly."
#endif


/* USE() - use a particular third-party library or optional OS service */
#define USE(WTF_FEATURE) (defined USE_##WTF_FEATURE && USE_##WTF_FEATURE)


#if PLATFORM(COCOA)
#if defined __has_include && __has_include(<CoreFoundation/CFPriv.h>)
#define USE_APPLE_INTERNAL_SDK 1
#endif
#endif

/* Export macro support. Detects the attributes available for shared library symbol export
   decorations. */
#if OS(WINDOWS) || (COMPILER_HAS_CLANG_DECLSPEC(dllimport) && COMPILER_HAS_CLANG_DECLSPEC(dllexport))
#define USE_DECLSPEC_ATTRIBUTE 1
#elif defined(__GNUC__)
#define USE_VISIBILITY_ATTRIBUTE 1
#endif

#if PLATFORM(COCOA)
#define USE_CG 1
#endif

#if PLATFORM(COCOA)
#define USE_CORE_TEXT 1
#endif

#if PLATFORM(COCOA)
#define USE_CA 1
#endif

#if PLATFORM(COCOA)
#define USE_CORE_IMAGE 1
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

#if PLATFORM(COCOA)
#define USE_CF 1
#endif

#if PLATFORM(COCOA) || (PLATFORM(GTK) || PLATFORM(WPE))
#define USE_FILE_LOCK 1
#endif

#if PLATFORM(COCOA)
#define USE_FOUNDATION 1
#endif

#if PLATFORM(MAC)
#define USE_APPKIT 1
#endif

#if PLATFORM(MAC)
#define USE_PASSKIT 1
#endif

#if PLATFORM(MAC)
#define USE_PLUGIN_HOST_PROCESS 1
#endif

#if PLATFORM(IOS_FAMILY)
#define USE_UIKIT_EDITING 1
#endif

#if PLATFORM(IOS_FAMILY)
#define USE_WEB_THREAD 1
#endif

#if OS(UNIX)
#define USE_PTHREADS 1
#endif

#if OS(DARWIN) && !PLATFORM(GTK)
#define USE_ACCELERATE 1
#endif

#if OS(WINDOWS)
#define USE_SYSTEM_MALLOC 1
#endif

#if CPU(REGISTER64)
#define USE_JSVALUE64 1
#else
#define USE_JSVALUE32_64 1
#endif

// FIXME: this should instead be based on SIZE_MAX == UINT64_MAX
// But this requires including <cstdint> and Platform.h is included in all kind of weird places, including non-cpp files
// And in practice CPU(ADDRESS64) is equivalent on all platforms we support (verified by static_asserts in ArrayBuffer.h)
#if CPU(ADDRESS64)
#define USE_LARGE_TYPED_ARRAYS 1
#else
#define USE_LARGE_TYPED_ARRAYS 0
#endif

#if USE(JSVALUE64)
/* FIXME: Enable BIGINT32 optimization again after we ensure Speedometer2 and JetStream2 regressions are fixed. */
/* https://bugs.webkit.org/show_bug.cgi?id=214777 */
#define USE_BIGINT32 0
#endif

/* FIXME: This name should be more specific if it is only for use with CallFrame* */
/* Use __builtin_frame_address(1) to get CallFrame* */
#if COMPILER(GCC_COMPATIBLE) && (CPU(ARM64) || CPU(X86_64))
#define USE_BUILTIN_FRAME_ADDRESS 1
#endif

#if OS(DARWIN) && CPU(ARM64) && HAVE(REMAP_JIT)
#define USE_EXECUTE_ONLY_JIT_WRITE_FUNCTION 1
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

#if PLATFORM(COCOA)
#define USE_AVFOUNDATION 1
#endif

#if PLATFORM(COCOA)
#define USE_PROTECTION_SPACE_AUTH_CALLBACK 1
#endif

#if PLATFORM(COCOA)
#define USE_METAL 1
#endif

#if PLATFORM(GTK) || PLATFORM(WPE)
#define USE_UNIX_DOMAIN_SOCKETS 1
#endif

#if !defined(USE_IMLANG_FONT_LINK2)
#define USE_IMLANG_FONT_LINK2 1
#endif

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
#define USE_AUDIO_SESSION 1
#endif

#if PLATFORM(COCOA)
#define USE_MEDIATOOLBOX 1
#endif

#if PLATFORM(COCOA)
#define USE_OS_LOG 1
#endif

#if PLATFORM(COCOA)
#define USE_OS_STATE 1
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
#define USE_DICTATION_ALTERNATIVES 1
#endif

#if (PLATFORM(MAC) && USE(APPLE_INTERNAL_SDK)) || PLATFORM(IOS_FAMILY)
#define USE_SOURCE_APPLICATION_AUDIT_DATA 1
#endif

#if PLATFORM(COCOA) && USE(CA)
#define USE_IOSURFACE_CANVAS_BACKING_STORE 1
#endif

/* The override isn't needed on iOS family, as the default behavior is to not sniff. */
#if PLATFORM(MAC)
#define USE_CFNETWORK_CONTENT_ENCODING_SNIFFING_OVERRIDE 1
#endif

#if PLATFORM(MAC) || PLATFORM(WPE) || PLATFORM(GTK) || PLATFORM(WIN_CAIRO)
/* FIXME: This really needs a descriptive name, this "new theme" was added in 2008. */
#define USE_NEW_THEME 1
#endif

#if PLATFORM(IOS) || PLATFORM(MACCATALYST)
#define USE_UICONTEXTMENU 1
#endif

#if PLATFORM(IOS_FAMILY) || (!USE(SYSTEM_MALLOC) && (OS(LINUX) && (PLATFORM(GTK) || PLATFORM(WPE))))
#define USE_BMALLOC_MEMORY_FOOTPRINT_API 1
#endif

#if !defined(USE_PLATFORM_REGISTERS_WITH_PROFILE) && OS(DARWIN) && CPU(ARM64) && defined(__LP64__)
#define USE_PLATFORM_REGISTERS_WITH_PROFILE 1
#endif

#if OS(DARWIN) && !USE(PLATFORM_REGISTERS_WITH_PROFILE) && CPU(ARM64)
#define USE_DARWIN_REGISTER_MACROS 1
#endif

#if PLATFORM(COCOA) && !(PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED < 110000)
#define USE_CTFONTSHAPEGLYPHS 1
#endif

#if PLATFORM(COCOA) && (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED < 110000)
#define USE_CTFONTGETADVANCES_WORKAROUND 1
#endif

#if (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 130000)
#define USE_NSIMAGE_FOR_SVG_SUPPORT 1
#endif

#if (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED < 120000) \
    || ((PLATFORM(IOS) || PLATFORM(MACCATALYST)) && __IPHONE_OS_VERSION_MIN_REQUIRED < 150000) \
    || (PLATFORM(WATCHOS) && __WATCH_OS_VERSION_MIN_REQUIRED < 80000) \
    || (PLATFORM(APPLETV) && __TV_OS_VERSION_MIN_REQUIRED < 150000)
#define USE_CTFONTSHAPEGLYPHS_WORKAROUND 1
#endif

#if (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED < 120000) \
    || ((PLATFORM(IOS) || PLATFORM(MACCATALYST)) && __IPHONE_OS_VERSION_MIN_REQUIRED < 150000) \
    || (PLATFORM(WATCHOS) && __WATCH_OS_VERSION_MIN_REQUIRED < 80000) \
    || (PLATFORM(APPLETV) && __TV_OS_VERSION_MIN_REQUIRED < 150000)
#define USE_NON_VARIABLE_SYSTEM_FONT 1
#endif

#if (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 130000) \
    || ((PLATFORM(IOS) || PLATFORM(MACCATALYST)) && __IPHONE_OS_VERSION_MIN_REQUIRED >= 160000) \
    || (PLATFORM(WATCHOS) && __WATCH_OS_VERSION_MIN_REQUIRED >= 90000) \
    || (PLATFORM(APPLETV) && __TV_OS_VERSION_MIN_REQUIRED >= 160000)
#define USE_CTFONTHASTABLE 1
#endif

#if (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 110000) \
    || (PLATFORM(MACCATALYST) && __IPHONE_OS_VERSION_MIN_REQUIRED >= 140000) \
    || (PLATFORM(IOS) && PLATFORM(IOS_FAMILY_SIMULATOR) && __IPHONE_OS_VERSION_MIN_REQUIRED >= 140000) \
    || (PLATFORM(WATCHOS) && PLATFORM(IOS_FAMILY_SIMULATOR) && __WATCH_OS_VERSION_MIN_REQUIRED >= 70000) \
    || (PLATFORM(APPLETV) && PLATFORM(IOS_FAMILY_SIMULATOR) && __TV_OS_VERSION_MIN_REQUIRED >= 140000)
#if USE(APPLE_INTERNAL_SDK)
/* Always use the macro on internal builds */
#define USE_PTHREAD_JIT_PERMISSIONS_API 0 
#else
#define USE_PTHREAD_JIT_PERMISSIONS_API 1
#endif
#endif

#if PLATFORM(COCOA)
#define USE_OPENXR 0
#define USE_IOSURFACE_FOR_XR_LAYER_DATA 1
#define USE_MTLSHAREDEVENT_FOR_XR_FRAME_COMPLETION 0
#if !defined(HAVE_WEBXR_INTERNALS) && !HAVE(WEBXR_INTERNALS)
#define USE_EMPTYXR 1
#endif
#endif

#if PLATFORM(IOS_FAMILY)
#define USE_SANDBOX_EXTENSIONS_FOR_CACHE_AND_TEMP_DIRECTORY_ACCESS 1
#endif

#if PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED < 120000
#define USE_VORBIS_AUDIOCOMPONENT_WORKAROUND 1
#endif

#if !defined(USE_ISO_MALLOC)
#define USE_ISO_MALLOC 1
#endif

#if !PLATFORM(WATCHOS)
#define USE_GLYPH_DISPLAY_LIST_CACHE 1
#endif

#if PLATFORM(IOS_FAMILY) || (USE(APPLE_INTERNAL_SDK) && PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 130000)
#define USE_RUNNINGBOARD 1
#endif

#if (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 130000)
#define USE_NSVIEW_SEMANTICCONTEXT 1
#endif

#if PLATFORM(IOS) && __IPHONE_OS_VERSION_MIN_REQUIRED >= 170000
#define USE_MEDIAPARSERD 1
#endif

#if (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED < 130000)
#define USE_AVIF 1
#endif

#if PLATFORM(COCOA) && (HAVE(CGSTYLE_CREATE_SHADOW2) || HAVE(CGSTYLE_COLORMATRIX_BLUR))
#define USE_GRAPHICS_CONTEXT_FILTERS 1
#endif
