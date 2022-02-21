/*
 * Copyright (C) 2006-2021 Apple Inc. All rights reserved.
 * Copyright (C) 2007-2009 Torch Mobile, Inc.
 * Copyright (C) 2010, 2011 Research In Motion Limited. All rights reserved.
 * Copyright (C) 2013 Samsung Electronics. All rights reserved.
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


#define ENABLE(WTF_FEATURE) (defined ENABLE_##WTF_FEATURE && ENABLE_##WTF_FEATURE)

/* Use this file to list _all_ ENABLE() macros. Define the macros to be one of the following values:
 *  - "0" disables the feature by default. The feature can still be enabled for a specific port or environment.
 *  - "1" enables the feature by default. The feature can still be disabled for a specific port or environment.
 *
 * The feature defaults in this file are only taken into account if the (port specific) build system
 * has not enabled or disabled a particular feature.
 *
 * Use this file to define ENABLE() macros only. Do not use this file to define USE() or macros !
 *
 * Only define a macro if it was not defined before - always check for !defined first.
 *
 * Keep the file sorted by the name of the defines. As an exception you can change the order
 * to allow interdependencies between the default values.
 *
 * Below are a few potential commands to take advantage of this file running from the Source/WTF directory
 *
 * Get the list of feature defines: grep -o "ENABLE_\(\w\+\)" wtf/PlatformEnable.h | sort | uniq
 * Get the list of features enabled by default for a PLATFORM(XXX): gcc -E -dM -I. -DWTF_PLATFORM_XXX "wtf/Platform.h" | grep "ENABLE_\w\+ 1" | cut -d' ' -f2 | sort
 */


/* FIXME: This should be renamed to ENABLE_ASSERTS for consistency and so it can be used as ENABLE(ASSERTS). */
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


/* ==== Platform additions: additions to PlatformEnable.h from outside the main repository ==== */

#if USE(APPLE_INTERNAL_SDK) && __has_include(<WebKitAdditions/AdditionalFeatureDefines.h>)
#include <WebKitAdditions/AdditionalFeatureDefines.h>
#endif



/* ==== Platform specific defaults ==== */

/* --------- Apple Cocoa platforms --------- */
#if PLATFORM(COCOA)
#include <wtf/PlatformEnableCocoa.h>
#endif

/* --------- Apple Windows port --------- */
#if PLATFORM(WIN) && !PLATFORM(WIN_CAIRO)
#include <wtf/PlatformEnableWinApple.h>
#endif

/* --------- Windows CAIRO port --------- */
#if PLATFORM(WIN_CAIRO)
#include <wtf/PlatformEnableWinCairo.h>
#endif

/* --------- PlayStation port --------- */
#if PLATFORM(PLAYSTATION)
#include <wtf/PlatformEnablePlayStation.h>
#endif

/* ---------  ENABLE macro defaults --------- */

/* Do not use PLATFORM() tests in this section ! */

#if !defined(ENABLE_WEBPROCESS_NSRUNLOOP)
#define ENABLE_WEBPROCESS_NSRUNLOOP 0
#endif

#if !defined(ENABLE_MAC_GESTURE_EVENTS)
#define ENABLE_MAC_GESTURE_EVENTS 0
#endif

#if !defined(ENABLE_CURSOR_VISIBILITY)
#define ENABLE_CURSOR_VISIBILITY 0
#endif

#if !defined(ENABLE_AIRPLAY_PICKER)
#define ENABLE_AIRPLAY_PICKER 0
#endif

#if !defined(ENABLE_APPLE_PAY_REMOTE_UI)
#define ENABLE_APPLE_PAY_REMOTE_UI 0
#endif

#if !defined(ENABLE_AUTOCORRECT)
#define ENABLE_AUTOCORRECT 0
#endif

#if !defined(ENABLE_AUTOCAPITALIZE)
#define ENABLE_AUTOCAPITALIZE 0
#endif

#if !defined(ENABLE_TEXT_AUTOSIZING)
#define ENABLE_TEXT_AUTOSIZING 0
#endif

#if !defined(ENABLE_IOS_GESTURE_EVENTS)
#define ENABLE_IOS_GESTURE_EVENTS 0
#endif

#if !defined(ENABLE_IOS_TOUCH_EVENTS)
#define ENABLE_IOS_TOUCH_EVENTS 0
#endif

#if !defined(ENABLE_PRIMARY_SNAPSHOTTED_PLUGIN_HEURISTIC)
#define ENABLE_PRIMARY_SNAPSHOTTED_PLUGIN_HEURISTIC 0
#endif

#if !defined(ENABLE_WKPDFVIEW)
#define ENABLE_WKPDFVIEW 0
#endif

#if !defined(ENABLE_PREVIEW_CONVERTER)
#define ENABLE_PREVIEW_CONVERTER 0
#endif

#if !defined(ENABLE_META_VIEWPORT)
#define ENABLE_META_VIEWPORT 0
#endif

#if !defined(ENABLE_FILE_REPLACEMENT)
#define ENABLE_FILE_REPLACEMENT 0
#endif

#if !defined(ENABLE_UI_SIDE_COMPOSITING)
#define ENABLE_UI_SIDE_COMPOSITING 0
#endif

#if !defined(ENABLE_3D_TRANSFORMS)
#define ENABLE_3D_TRANSFORMS 0
#endif

#if !defined(ENABLE_ACCESSIBILITY)
#define ENABLE_ACCESSIBILITY 1
#endif

#if !defined(ENABLE_OVERFLOW_SCROLLING_TOUCH)
#define ENABLE_OVERFLOW_SCROLLING_TOUCH 0
#endif

#if !defined(ENABLE_APNG)
#define ENABLE_APNG 1
#endif

#if !defined(ENABLE_CHANNEL_MESSAGING)
#define ENABLE_CHANNEL_MESSAGING 1
#endif

#if !defined(ENABLE_CONTENT_CHANGE_OBSERVER)
#define ENABLE_CONTENT_CHANGE_OBSERVER 0
#endif

#if !defined(ENABLE_CONTENT_EXTENSIONS)
#define ENABLE_CONTENT_EXTENSIONS 0
#endif

#if !defined(ENABLE_CONTEXT_MENUS)
#define ENABLE_CONTEXT_MENUS 1
#endif

#if !defined(ENABLE_CONTEXT_MENU_EVENT)
#define ENABLE_CONTEXT_MENU_EVENT 1
#endif

#if !defined(ENABLE_CSS3_TEXT)
#define ENABLE_CSS3_TEXT 0
#endif

#if !defined(ENABLE_CSS_BOX_DECORATION_BREAK)
#define ENABLE_CSS_BOX_DECORATION_BREAK 1
#endif

#if !defined(ENABLE_CSS_COMPOSITING)
#define ENABLE_CSS_COMPOSITING 0
#endif

#if !defined(ENABLE_CSS_IMAGE_RESOLUTION)
#define ENABLE_CSS_IMAGE_RESOLUTION 0
#endif

#if !defined(ENABLE_CSS_CONIC_GRADIENTS)
#define ENABLE_CSS_CONIC_GRADIENTS 0
#endif

#if !defined(ENABLE_CSS_TRANSFORM_STYLE_OPTIMIZED_3D)
#define ENABLE_CSS_TRANSFORM_STYLE_OPTIMIZED_3D 0
#endif

#if !defined(ENABLE_CUSTOM_CURSOR_SUPPORT)
#define ENABLE_CUSTOM_CURSOR_SUPPORT 1
#endif

#if !defined(ENABLE_DARK_MODE_CSS)
#define ENABLE_DARK_MODE_CSS 0
#endif

#if !defined(ENABLE_DATALIST_ELEMENT)
#define ENABLE_DATALIST_ELEMENT 0
#endif

#if !defined(ENABLE_DEVICE_ORIENTATION)
#define ENABLE_DEVICE_ORIENTATION 0
#endif

#if !defined(ENABLE_DESTINATION_COLOR_SPACE_DISPLAY_P3)
#define ENABLE_DESTINATION_COLOR_SPACE_DISPLAY_P3 0
#endif

#if !defined(ENABLE_DESTINATION_COLOR_SPACE_LINEAR_SRGB)
#define ENABLE_DESTINATION_COLOR_SPACE_LINEAR_SRGB 1
#endif

#if !defined(ENABLE_DOWNLOAD_ATTRIBUTE)
#define ENABLE_DOWNLOAD_ATTRIBUTE 1
#endif

#if !defined(ENABLE_DRAG_SUPPORT)
#define ENABLE_DRAG_SUPPORT 1
#endif

#if !defined(ENABLE_ENCRYPTED_MEDIA)
#define ENABLE_ENCRYPTED_MEDIA 0
#endif

#if !defined(ENABLE_FILTERS_LEVEL_2)
#define ENABLE_FILTERS_LEVEL_2 0
#endif

#if !defined(ENABLE_FTPDIR)
#define ENABLE_FTPDIR 1
#endif

#if !defined(ENABLE_FULLSCREEN_API)
#define ENABLE_FULLSCREEN_API 0
#endif

#if ((PLATFORM(IOS) || PLATFORM(WATCHOS) || PLATFORM(MACCATALYST)) && HAVE(AVKIT)) || PLATFORM(MAC)
#if !defined(ENABLE_VIDEO_PRESENTATION_MODE)
#define ENABLE_VIDEO_PRESENTATION_MODE 1
#endif
#endif

#if !defined(ENABLE_GAMEPAD)
#define ENABLE_GAMEPAD 0
#endif

#if !defined(ENABLE_GEOLOCATION)
#define ENABLE_GEOLOCATION 0
#endif

#if !defined(ENABLE_INPUT_TYPE_COLOR)
#define ENABLE_INPUT_TYPE_COLOR 1
#endif

#if !defined(ENABLE_INPUT_TYPE_DATE)
#define ENABLE_INPUT_TYPE_DATE 0
#endif

#if !defined(ENABLE_INPUT_TYPE_DATETIMELOCAL)
#define ENABLE_INPUT_TYPE_DATETIMELOCAL 0
#endif

#if !defined(ENABLE_INPUT_TYPE_MONTH)
#define ENABLE_INPUT_TYPE_MONTH 0
#endif

#if !defined(ENABLE_INPUT_TYPE_TIME)
#define ENABLE_INPUT_TYPE_TIME 0
#endif

#if !defined(ENABLE_INPUT_TYPE_WEEK)
#define ENABLE_INPUT_TYPE_WEEK 0
#endif

#if !defined(ENABLE_IOS_FORM_CONTROL_REFRESH)
#define ENABLE_IOS_FORM_CONTROL_REFRESH 0
#endif

#if !defined(ENABLE_IPC_TESTING_API)
/* Enable IPC testing on all ASAN builds and debug builds. */
#if (ASAN_ENABLED || !defined(NDEBUG)) && PLATFORM(COCOA)
#define ENABLE_IPC_TESTING_API 1
#endif
#endif

#if ENABLE(INPUT_TYPE_DATE) || ENABLE(INPUT_TYPE_DATETIMELOCAL) || ENABLE(INPUT_TYPE_MONTH) || ENABLE(INPUT_TYPE_TIME) || ENABLE(INPUT_TYPE_WEEK)
#if !defined(ENABLE_DATE_AND_TIME_INPUT_TYPES)
#define ENABLE_DATE_AND_TIME_INPUT_TYPES 1
#endif
#endif

#if !defined(ENABLE_INSPECTOR_ALTERNATE_DISPATCHERS)
#define ENABLE_INSPECTOR_ALTERNATE_DISPATCHERS 0
#endif

#if !defined(ENABLE_INSPECTOR_EXTENSIONS)
#define ENABLE_INSPECTOR_EXTENSIONS 0
#endif

#if !defined(ENABLE_INSPECTOR_TELEMETRY)
#define ENABLE_INSPECTOR_TELEMETRY 0
#endif

#if !defined(ENABLE_LAYER_BASED_SVG_ENGINE)
#define ENABLE_LAYER_BASED_SVG_ENGINE 0
#endif

#if !defined(ENABLE_LAYOUT_FORMATTING_CONTEXT)
#define ENABLE_LAYOUT_FORMATTING_CONTEXT 0
#endif

#if !defined(ENABLE_MATHML)
#define ENABLE_MATHML 1
#endif

#if !defined(ENABLE_MEDIA_CAPTURE)
#define ENABLE_MEDIA_CAPTURE 0
#endif

#if !defined(ENABLE_MEDIA_CONTROLS_SCRIPT)
#define ENABLE_MEDIA_CONTROLS_SCRIPT 0
#endif

#if !defined(ENABLE_MEDIA_RECORDER)
#define ENABLE_MEDIA_RECORDER 0
#endif

#if !defined(ENABLE_MEDIA_SOURCE)
#define ENABLE_MEDIA_SOURCE 0
#endif

#if !defined(ENABLE_MEDIA_STATISTICS)
#define ENABLE_MEDIA_STATISTICS 0
#endif

#if !defined(ENABLE_MEDIA_STREAM)
#define ENABLE_MEDIA_STREAM 0
#endif

#if !defined(ENABLE_MHTML)
#define ENABLE_MHTML 0
#endif

#if !defined(ENABLE_MODERN_MEDIA_CONTROLS)
#define ENABLE_MODERN_MEDIA_CONTROLS 0
#endif

#if !defined(ENABLE_MOUSE_CURSOR_SCALE)
#define ENABLE_MOUSE_CURSOR_SCALE 0
#endif

#if !defined(ENABLE_MOUSE_FORCE_EVENTS)
#define ENABLE_MOUSE_FORCE_EVENTS 1
#endif

#if !defined(ENABLE_NETSCAPE_PLUGIN_METADATA_CACHE)
#define ENABLE_NETSCAPE_PLUGIN_METADATA_CACHE 0
#endif

#if !defined(ENABLE_NOTIFICATIONS)
#define ENABLE_NOTIFICATIONS 0
#endif

#if !defined(ENABLE_OFFSCREEN_CANVAS)
#define ENABLE_OFFSCREEN_CANVAS 0
#endif

#if !defined(ENABLE_OFFSCREEN_CANVAS_IN_WORKERS)
#define ENABLE_OFFSCREEN_CANVAS_IN_WORKERS 0
#endif

#if !defined(ENABLE_THUNDER)
#define ENABLE_THUNDER 0
#endif

#if !defined(ENABLE_ORIENTATION_EVENTS)
#define ENABLE_ORIENTATION_EVENTS 0
#endif

#if OS(WINDOWS)
#if !defined(ENABLE_PAN_SCROLLING)
#define ENABLE_PAN_SCROLLING 1
#endif
#endif

#if !defined(ENABLE_PAYMENT_REQUEST)
#define ENABLE_PAYMENT_REQUEST 0
#endif

#if !defined(ENABLE_PERIODIC_MEMORY_MONITOR)
#define ENABLE_PERIODIC_MEMORY_MONITOR 0
#endif

#if !defined(ENABLE_POINTER_LOCK)
#define ENABLE_POINTER_LOCK 1
#endif

#if !defined(ENABLE_REMOTE_INSPECTOR)
#define ENABLE_REMOTE_INSPECTOR 0
#endif

#if !defined(ENABLE_RUBBER_BANDING)
#define ENABLE_RUBBER_BANDING 0
#endif

#if !defined(ENABLE_SECURITY_ASSERTIONS)
/* Enable security assertions on all ASAN builds and debug builds. */
#if ASAN_ENABLED || !defined(NDEBUG)
#define ENABLE_SECURITY_ASSERTIONS 1
#endif
#endif

#if !defined(ENABLE_SEPARATED_WX_HEAP)
#define ENABLE_SEPARATED_WX_HEAP 0
#endif

#if !defined(ENABLE_SPEECH_SYNTHESIS)
#define ENABLE_SPEECH_SYNTHESIS 0
#endif

#if !defined(ENABLE_SPELLCHECK)
#define ENABLE_SPELLCHECK 0
#endif

#if !defined(ENABLE_TEXT_CARET)
#define ENABLE_TEXT_CARET 1
#endif

#if !defined(ENABLE_TEXT_SELECTION)
#define ENABLE_TEXT_SELECTION 1
#endif

#if !defined(ENABLE_ASYNC_SCROLLING)
#define ENABLE_ASYNC_SCROLLING 0
#endif

#if !defined(ENABLE_TOUCH_EVENTS)
#define ENABLE_TOUCH_EVENTS 0
#endif

#if !defined(ENABLE_TOUCH_ACTION_REGIONS)
#define ENABLE_TOUCH_ACTION_REGIONS 0
#endif

#if !defined(ENABLE_WHEEL_EVENT_REGIONS)
#define ENABLE_WHEEL_EVENT_REGIONS 0
#endif

#if !defined(ENABLE_VIDEO)
#define ENABLE_VIDEO 0
#endif

#if !defined(ENABLE_DATACUE_VALUE)
#define ENABLE_DATACUE_VALUE 0
#endif

#if !defined(ENABLE_WEBGL)
#define ENABLE_WEBGL 0
#endif

#if !defined(ENABLE_WEB_ARCHIVE)
#define ENABLE_WEB_ARCHIVE 0
#endif

#if !defined(ENABLE_WEB_AUDIO)
#define ENABLE_WEB_AUDIO 0
#endif

#if !defined(ENABLE_XSLT)
#define ENABLE_XSLT 1
#endif

#if !defined(ENABLE_SERVICE_WORKER)
#define ENABLE_SERVICE_WORKER 1
#endif

#if !defined(ENABLE_MONOSPACE_FONT_EXCEPTION)
#define ENABLE_MONOSPACE_FONT_EXCEPTION 0
#endif

#if !defined(ENABLE_FULL_KEYBOARD_ACCESS)
#define ENABLE_FULL_KEYBOARD_ACCESS 0
#endif

#if !defined(ENABLE_PLATFORM_DRIVEN_TEXT_CHECKING)
#define ENABLE_PLATFORM_DRIVEN_TEXT_CHECKING 0
#endif

#if !defined(ENABLE_WEB_PLAYBACK_CONTROLS_MANAGER)
#define ENABLE_WEB_PLAYBACK_CONTROLS_MANAGER 0
#endif

#if !defined(ENABLE_RESOURCE_USAGE)
#define ENABLE_RESOURCE_USAGE 0
#endif

#if !defined(ENABLE_SEC_ITEM_SHIM)
#define ENABLE_SEC_ITEM_SHIM 0
#endif

#if !defined(ENABLE_DATA_DETECTION)
#define ENABLE_DATA_DETECTION 0
#endif

#if !defined(ENABLE_FILE_SHARE)
#define ENABLE_FILE_SHARE 1
#endif

#if !defined(ENABLE_WEBXR)
#define ENABLE_WEBXR 0
#endif

#if !defined(ENABLE_WEBXR_HANDS)
#define ENABLE_WEBXR_HANDS 0
#endif

/*
 * Enable this to put each IsoHeap and other allocation categories into their own malloc heaps, so that tools like vmmap can show how big each heap is.
 * Turn BENABLE_MALLOC_HEAP_BREAKDOWN on in bmalloc together when using this.
 */
#if !defined(ENABLE_MALLOC_HEAP_BREAKDOWN)
#define ENABLE_MALLOC_HEAP_BREAKDOWN 0
#endif

#if !defined(ENABLE_CFPREFS_DIRECT_MODE)
#define ENABLE_CFPREFS_DIRECT_MODE 0
#endif



/* FIXME: This section of the file has not been cleaned up yet and needs major work. */

/* FIXME: JSC_OBJC_API_ENABLED does not match the normal ENABLE naming convention. */
#if !PLATFORM(COCOA)
#if !defined(JSC_OBJC_API_ENABLED)
#define JSC_OBJC_API_ENABLED 0
#endif
#endif

/* The JIT is enabled by default on all x86-64 & ARM64 platforms. */
#if !defined(ENABLE_JIT) && (CPU(X86_64) || (CPU(ARM64) && CPU(ADDRESS64)))
#define ENABLE_JIT 1
#endif

#if USE(JSVALUE32_64)
/* Disable WebAssembly on all 32bit platforms. Its LLInt tier could
 * work on them, but still needs some final touches. */
#undef ENABLE_WEBASSEMBLY
#define ENABLE_WEBASSEMBLY 0
#undef ENABLE_WEBASSEMBLY_B3JIT
#define ENABLE_WEBASSEMBLY_B3JIT 0
#if (CPU(ARM_THUMB2) || CPU(MIPS)) && OS(LINUX)
/* On ARMv7 and MIPS on Linux the JIT is enabled unless explicitly disabled. */
#if !defined(ENABLE_JIT)
#define ENABLE_JIT 1
#endif
#else
/* Disable JIT on all other 32bit architectures. */
#undef ENABLE_JIT
#define ENABLE_JIT 0
#endif
#endif

#if !defined(ENABLE_C_LOOP)
#if ENABLE(JIT) || CPU(X86_64) || CPU(ARM64)
#define ENABLE_C_LOOP 0
#else
#define ENABLE_C_LOOP 1
#endif
#endif

#if !defined(ENABLE_JUMP_ISLANDS) && CPU(ARM64) && CPU(ADDRESS64) && ENABLE(JIT)
#define ENABLE_JUMP_ISLANDS 1
#endif

/* FIXME: This should be turned into an #error invariant */
/* The FTL *does not* work on 32-bit platforms. Disable it even if someone asked us to enable it. */
#if USE(JSVALUE32_64)
#undef ENABLE_FTL_JIT
#define ENABLE_FTL_JIT 0
#endif

/* If possible, try to enable a disassembler. This is optional. We proceed in two
   steps: first we try to find some disassembler that we can use, and then we
   decide if the high-level disassembler API can be enabled. */
#if !defined(ENABLE_ZYDIS) && ENABLE(JIT) && CPU(X86_64) && !USE(CAPSTONE)
#define ENABLE_ZYDIS 1
#endif

#if !defined(ENABLE_ARM64_DISASSEMBLER) && ENABLE(JIT) && CPU(ARM64) && !USE(CAPSTONE)
#define ENABLE_ARM64_DISASSEMBLER 1
#endif

#if !defined(ENABLE_RISCV64_DISASSEMBLER) && ENABLE(JIT) && CPU(RISCV64) && !USE(CAPSTONE)
#define ENABLE_RISCV64_DISASSEMBLER 1
#endif

#if !defined(ENABLE_DISASSEMBLER) && (ENABLE(ZYDIS) || ENABLE(ARM64_DISASSEMBLER) || ENABLE(RISCV64_DISASSEMBLER) || (ENABLE(JIT) && USE(CAPSTONE)))
#define ENABLE_DISASSEMBLER 1
#endif

#if !defined(ENABLE_DFG_JIT) && ENABLE(JIT)

/* Enable the DFG JIT on X86 and X86_64. */
#if CPU(X86_64) && (OS(DARWIN) || OS(LINUX) || OS(FREEBSD) || OS(HURD) || OS(WINDOWS))
#define ENABLE_DFG_JIT 1
#endif

/* Enable the DFG JIT on ARMv7.  Only tested on iOS, Linux, and FreeBSD. */
#if (CPU(ARM_THUMB2) || CPU(ARM64)) && (OS(DARWIN) || OS(LINUX) || OS(FREEBSD))
#define ENABLE_DFG_JIT 1
#endif

/* Enable the DFG JIT on MIPS. */
#if CPU(MIPS)
#define ENABLE_DFG_JIT 1
#endif

#endif /* !defined(ENABLE_DFG_JIT) && ENABLE(JIT) */

/* Concurrent JS only works on 64-bit platforms because it requires that
   values get stored to atomically. This is trivially true on 64-bit platforms,
   but not true at all on 32-bit platforms where values are composed of two
   separate sub-values. */
#if ENABLE(JIT) && USE(JSVALUE64)
#define ENABLE_CONCURRENT_JS 1
#endif

#if (CPU(X86_64) || CPU(ARM64)) && HAVE(FAST_TLS)
#define ENABLE_FAST_TLS_JIT 1
#endif

/* FIXME: This should be turned into an #error invariant */
/* If the baseline jit is not available, then disable upper tiers as well. */
#if !ENABLE(JIT)
#undef ENABLE_DFG_JIT
#undef ENABLE_FTL_JIT
#define ENABLE_DFG_JIT 0
#define ENABLE_FTL_JIT 0
#endif

/* FIXME: This should be turned into an #error invariant */
/* If the DFG jit is not available, then disable upper tiers as well: */
#if !ENABLE(DFG_JIT)
#undef ENABLE_FTL_JIT
#define ENABLE_FTL_JIT 0
#endif

/* This controls whether B3 is built. B3 is needed for FTL JIT and WebAssembly */
#if ENABLE(FTL_JIT)
#define ENABLE_B3_JIT 1
#endif

#if !defined(ENABLE_WEBASSEMBLY) && (ENABLE(B3_JIT) && PLATFORM(COCOA) && CPU(ADDRESS64))
#define ENABLE_WEBASSEMBLY 1
#define ENABLE_WEBASSEMBLY_B3JIT 1
#endif

/* The SamplingProfiler is the probabilistic and low-overhead profiler used by
 * JSC to measure where time is spent inside a JavaScript program.
 * In configurations other than Windows and Darwin, because layout of mcontext_t depends on standard libraries (like glibc),
 * sampling profiler is enabled if WebKit uses pthreads and glibc. */
#if !defined(ENABLE_SAMPLING_PROFILER) && (!ENABLE(C_LOOP) && (OS(WINDOWS) || HAVE(MACHINE_CONTEXT)))
#define ENABLE_SAMPLING_PROFILER 1
#endif

#if ENABLE(WEBASSEMBLY) && HAVE(MACHINE_CONTEXT)
#define ENABLE_WEBASSEMBLY_SIGNALING_MEMORY 1
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
#if !defined(ENABLE_DFG_REGISTER_ALLOCATION_VALIDATION) && ENABLE(DFG_JIT) && !defined(NDEBUG)
#define ENABLE_DFG_REGISTER_ALLOCATION_VALIDATION 1
#endif

/* Determine if we need to enable Computed Goto Opcodes or not: */
#if HAVE(COMPUTED_GOTO) || !ENABLE(C_LOOP)
#define ENABLE_COMPUTED_GOTO_OPCODES 1
#endif

/* Regular Expression Tracing - Set to 1 to trace RegExp's in jsc.  Results dumped at exit */
#if !defined(ENABLE_REGEXP_TRACING)
#define ENABLE_REGEXP_TRACING 0
#endif

/* Yet Another Regex Runtime - turned on by default for JIT enabled ports. */
#if !defined(ENABLE_YARR_JIT) && ENABLE(JIT)
#define ENABLE_YARR_JIT 1
#endif

/* Setting this flag compares JIT results with interpreter results. */
#if !defined(ENABLE_YARR_JIT) && ENABLE(JIT)
#define ENABLE_YARR_JIT_DEBUG 0
#endif

/* Enable JIT'ing Regular Expressions that have nested parenthesis . */
#if ENABLE(YARR_JIT) && (CPU(ARM64) || (CPU(X86_64) && !OS(WINDOWS)) || CPU(RISCV64))
#define ENABLE_YARR_JIT_ALL_PARENS_EXPRESSIONS 1
#define ENABLE_YARR_JIT_REGEXP_TEST_INLINE 1
#endif

/* Enable JIT'ing Regular Expressions that have nested back references. */
#if ENABLE(YARR_JIT) && (CPU(ARM64) || (CPU(X86_64) && !OS(WINDOWS)) || CPU(RISCV64))
#define ENABLE_YARR_JIT_BACKREFERENCES 1
#endif

#if ENABLE(YARR_JIT) && (CPU(ARM64) || CPU(X86_64) || CPU(RISCV64))
#define ENABLE_YARR_JIT_UNICODE_EXPRESSIONS 1
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

#if !defined(ENABLE_EXCEPTION_SCOPE_VERIFICATION)
#define ENABLE_EXCEPTION_SCOPE_VERIFICATION ASSERT_ENABLED
#endif

#if ENABLE(DFG_JIT) && HAVE(MACHINE_CONTEXT) && (CPU(X86_64) || CPU(ARM64) || CPU(RISCV64))
#define ENABLE_SIGNAL_BASED_VM_TRAPS 1
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
#if OS(WINDOWS)
#define ENABLE_UNIFIED_AND_FREEZABLE_CONFIG_RECORD 0
#else
#define ENABLE_UNIFIED_AND_FREEZABLE_CONFIG_RECORD 1
#endif

/* CSS Selector JIT Compiler */
#if !defined(ENABLE_CSS_SELECTOR_JIT) && ((CPU(X86_64) || CPU(ARM64) || (CPU(ARM_THUMB2) && OS(DARWIN))) && ENABLE(JIT) && (OS(DARWIN) || PLATFORM(GTK) || PLATFORM(WPE)))
#define ENABLE_CSS_SELECTOR_JIT 1
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

#if !defined(ENABLE_GC_VALIDATION) && !defined(NDEBUG)
#define ENABLE_GC_VALIDATION 1
#endif

#if OS(DARWIN) && ENABLE(JIT) && USE(APPLE_INTERNAL_SDK) && CPU(ARM64E) && HAVE(JIT_CAGE) && !PLATFORM(MAC)
#define ENABLE_JIT_CAGE 1
#endif

#if OS(DARWIN) && CPU(ADDRESS64) && ENABLE(JIT) && (ENABLE(JIT_CAGE) || ASSERT_ENABLED)
#define ENABLE_JIT_OPERATION_VALIDATION 1
#endif

#if CPU(ARM64) || (CPU(X86_64) && !OS(WINDOWS))
/* The implementation of these thunks can use up to 6 argument registers, and
   make use of ARM64 like features. For now, we'll only support them on platforms
   that have 6 or more argument registers to use.
*/
#define ENABLE_EXTRA_CTI_THUNKS 1
#endif

#if !defined(ENABLE_BINDING_INTEGRITY) && !OS(WINDOWS)
#define ENABLE_BINDING_INTEGRITY 1
#endif

#if !defined(ENABLE_TREE_DEBUGGING) && !defined(NDEBUG)
#define ENABLE_TREE_DEBUGGING 1
#endif

#if !defined(ENABLE_OPENTYPE_VERTICAL) && PLATFORM(GTK) || PLATFORM(WPE)
#define ENABLE_OPENTYPE_VERTICAL 1
#endif

#if !defined(ENABLE_OPENTYPE_MATH) && (OS(DARWIN) && USE(CG)) || (USE(FREETYPE) && !PLATFORM(GTK)) || (PLATFORM(WIN) && (USE(CG) || USE(CAIRO)))
#define ENABLE_OPENTYPE_MATH 1
#endif

#if !defined(ENABLE_INLINE_PATH_DATA) && USE(CG)
#define ENABLE_INLINE_PATH_DATA 1
#endif

#if ((PLATFORM(COCOA) || PLATFORM(PLAYSTATION) || PLATFORM(WPE)) && ENABLE(ASYNC_SCROLLING)) || PLATFORM(GTK)
#define ENABLE_KINETIC_SCROLLING 1
#endif

#if PLATFORM(MAC)
// FIXME: Maybe this can be combined with ENABLE_KINETIC_SCROLLING.
#define ENABLE_WHEEL_EVENT_LATCHING 1
#endif

#if PLATFORM(MAC)
#define ENABLE_MOMENTUM_EVENT_DISPATCHER 1
#endif

#if !defined(ENABLE_SCROLLING_THREAD)
#if USE(NICOSIA)
#define ENABLE_SCROLLING_THREAD 1
#else
#define ENABLE_SCROLLING_THREAD 0
#endif
#endif

/* This feature works by embedding the OpcodeID in the 32 bit just before the generated LLint code
   that executes each opcode. It cannot be supported by the CLoop since there's no way to embed the
   OpcodeID word in the CLoop's switch statement cases. It is also currently not implemented for MSVC.
*/
#if !defined(ENABLE_LLINT_EMBEDDED_OPCODE_ID) && !ENABLE(C_LOOP) && !COMPILER(MSVC) && (CPU(X86) || CPU(X86_64) || CPU(ARM64) || (CPU(ARM_THUMB2) && OS(DARWIN)) || CPU(RISCV64))
#define ENABLE_LLINT_EMBEDDED_OPCODE_ID 1
#endif

#if !defined(ENABLE_PREDEFINED_COLOR_SPACE_DISPLAY_P3)
#define ENABLE_PREDEFINED_COLOR_SPACE_DISPLAY_P3 0
#endif





/* Asserts, invariants for macro definitions */

#if ENABLE(MEDIA_CONTROLS_SCRIPT) && !ENABLE(VIDEO)
#error "ENABLE(MEDIA_CONTROLS_SCRIPT) requires ENABLE(VIDEO)"
#endif

#if ENABLE(INSPECTOR_ALTERNATE_DISPATCHERS) && !ENABLE(REMOTE_INSPECTOR)
#error "ENABLE(INSPECTOR_ALTERNATE_DISPATCHERS) requires ENABLE(REMOTE_INSPECTOR)"
#endif

#if ENABLE(IOS_TOUCH_EVENTS) && !ENABLE(TOUCH_EVENTS)
#error "ENABLE(IOS_TOUCH_EVENTS) requires ENABLE(TOUCH_EVENTS)"
#endif

#if ENABLE(WEBGL2) && !ENABLE(WEBGL)
#error "ENABLE(WEBGL2) requires ENABLE(WEBGL)"
#endif

#if ENABLE(OFFSCREEN_CANVAS_IN_WORKERS) && !ENABLE(OFFSCREEN_CANVAS)
#error "ENABLE(OFFSCREEN_CANVAS_IN_WORKERS) requires ENABLE(OFFSCREEN_CANVAS)"
#endif

#if ENABLE(MEDIA_RECORDER) && !ENABLE(MEDIA_STREAM)
#error "ENABLE(MEDIA_RECORDER) requires ENABLE(MEDIA_STREAM)"
#endif

#if USE(CG)

#if ENABLE(DESTINATION_COLOR_SPACE_DISPLAY_P3) && !HAVE(CORE_GRAPHICS_DISPLAY_P3_COLOR_SPACE)
#error "ENABLE(DESTINATION_COLOR_SPACE_DISPLAY_P3) requires HAVE(CORE_GRAPHICS_DISPLAY_P3_COLOR_SPACE) on platforms using CoreGraphics"
#endif

#if ENABLE(DESTINATION_COLOR_SPACE_LINEAR_SRGB) && !HAVE(CORE_GRAPHICS_LINEAR_SRGB_COLOR_SPACE)
#error "ENABLE(DESTINATION_COLOR_SPACE_LINEAR_SRGB) requires HAVE(CORE_GRAPHICS_LINEAR_SRGB_COLOR_SPACE) on platforms using CoreGraphics"
#endif

#endif

#if ENABLE(PREDEFINED_COLOR_SPACE_DISPLAY_P3) && !ENABLE(DESTINATION_COLOR_SPACE_DISPLAY_P3)
#error "ENABLE(PREDEFINED_COLOR_SPACE_DISPLAY_P3) requires ENABLE(DESTINATION_COLOR_SPACE_DISPLAY_P3)"
#endif

#if ENABLE(WEBXR_HANDS) && !ENABLE(WEBXR)
#error "ENABLE(WEBXR_HANDS) requires ENABLE(WEBXR)"
#endif

#if ENABLE(SERVICE_WORKER) && ENABLE(NOTIFICATIONS)
#if !defined(ENABLE_NOTIFICATION_EVENT)
#define ENABLE_NOTIFICATION_EVENT 1
#endif
#endif
