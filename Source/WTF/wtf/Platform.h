/*
 * Copyright (C) 2006-2020 Apple Inc. All rights reserved.
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

/* This ensures that users #include <wtf/Platform.h> rather than one of the helper files files directly. */
#define WTF_PLATFORM_GUARD_AGAINST_INDIRECT_INCLUSION
/* IWYU pragma: begin_exports */

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
#include <wtf/PlatformHave.h>


/* ==== Policy decision macros: these define policy choices for a particular port. ==== */

/* USE() - use a particular third-party library or optional OS service */
#include <wtf/PlatformUse.h>

/* ENABLE() - turn on a specific feature of WebKit */
#include <wtf/PlatformEnable.h>


/* ==== Helper macros ==== */

/* Macros for specifing specific calling conventions. */
#include <wtf/PlatformCallingConventions.h>


/* ==== Platform additions: additions to Platform.h from outside the main repository ==== */

#if USE(APPLE_INTERNAL_SDK) && __has_include(<WebKitAdditions/AdditionalPlatform.h>)
#include <WebKitAdditions/AdditionalPlatform.h>
#endif

/* IWYU pragma: end_exports */
#undef WTF_PLATFORM_GUARD_AGAINST_INDIRECT_INCLUSION


/* FIXME: The following are currenly positioned at the bottom of this file as they either
   are currently dependent on macros they should not be and need to be refined or do not
   belong as part of Platform.h at all. */


#if PLATFORM(GTK)
#define GLIB_VERSION_MIN_REQUIRED GLIB_VERSION_2_44
#if USE(GTK4)
#define GDK_VERSION_MIN_REQUIRED GDK_VERSION_4_0
#else
#define GDK_VERSION_MIN_REQUIRED GDK_VERSION_3_22
#endif
#endif

#if PLATFORM(WPE)
#define GLIB_VERSION_MIN_REQUIRED GLIB_VERSION_2_44
#endif

#if USE(SOUP)
#if USE(SOUP2)
#define SOUP_VERSION_MIN_REQUIRED SOUP_VERSION_2_54
#else
#define SOUP_VERSION_MIN_REQUIRED SOUP_VERSION_3_0
#endif
#endif

#if PLATFORM(COCOA)
/* Cocoa defines a series of platform macros for debugging. */
/* Some of them are really annoying because they use common names (e.g. check()). */
/* Disable those macros so that we are not limited in how we name methods and functions. */
#undef __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORES
#define __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORES 0
#endif

/* FIXME: This does not belong in Platform.h and should instead be included in another mechanism (compiler option, prefix header, config.h, etc) */
/* ICU configuration. Some of these match ICU defaults on some platforms, but we would like them consistently set everywhere we build WebKit. */
#define U_HIDE_DEPRECATED_API 1
#define U_SHOW_CPLUSPLUS_API 0
#ifdef __cplusplus
#define UCHAR_TYPE char16_t
#endif
#if PLATFORM(COCOA)
#define U_DISABLE_RENAMING 1
#endif

#if COMPILER(MSVC)
#undef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#undef __STDC_LIMIT_MACROS
#define __STDC_LIMIT_MACROS
#endif

/* FIXME: Any remaining use of TARGET_OS_IPHONE should be removed outside of Apple only files and replaced with OS() checks. */
/* Set TARGET_OS_IPHONE to 0 by default to allow using it as a guard
 * in cross-platform the same way as it is used in OS(DARWIN) code. */ 
#if !defined(TARGET_OS_IPHONE) && !OS(DARWIN)
#define TARGET_OS_IPHONE 0
#endif

/* FIXME: This does not belong in Platform.h and should instead be included in another mechanism (compiler option, prefix header, config.h, etc) */
#if COMPILER(MSVC)
/* Enable strict runtime stack buffer checks. */
#pragma strict_gs_check(on)
#endif

/* FIXME: This does not belong in Platform.h and should instead be included in another mechanism (prefix header, config.h, etc) */
#if USE(GLIB)
#include <wtf/glib/GTypedefs.h>
#endif

/* FIXME: The availability of RSA_PSS should not depend on the policy decision to USE(GCRYPT). */
#if PLATFORM(MAC) || PLATFORM(IOS) || PLATFORM(MACCATALYST) || USE(GCRYPT) || USE(OPENSSL)
#define HAVE_RSA_PSS 1
#endif

/* FIXME: Remove dependence on ENABLE(WEB_RTC). */
#if PLATFORM(COCOA) && ENABLE(WEB_RTC)
#define USE_LIBWEBRTC 1
#endif

#if PLATFORM(COCOA) && ENABLE(WEBGL)
#define USE_ANGLE 1
#ifndef GL_SILENCE_DEPRECATION
#define GL_SILENCE_DEPRECATION 1
#endif
#endif

/* FIXME: This is used to "turn on a specific feature of WebKit", so should be converted to an ENABLE macro. */
#if PLATFORM(COCOA) && ENABLE(ACCESSIBILITY)
#define USE_ACCESSIBILITY_CONTEXT_MENUS 1
#endif
