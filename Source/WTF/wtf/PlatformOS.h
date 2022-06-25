/*
 * Copyright (C) 2006-2021 Apple Inc. All rights reserved.
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


#if defined(__APPLE__)
#include <Availability.h>
#include <AvailabilityMacros.h>
#include <TargetConditionals.h>
#endif

/* OS() - underlying operating system; only to be used for mandated low-level services like
   virtual memory, not to choose a GUI toolkit */
#define OS(WTF_FEATURE) (defined WTF_OS_##WTF_FEATURE && WTF_OS_##WTF_FEATURE)
#define OS_CONSTANT(WTF_FEATURE) (WTF_OS_CONSTANT_##WTF_FEATURE)


/* ==== OS() - underlying operating system; only to be used for mandated low-level services like
   virtual memory, not to choose a GUI toolkit ==== */

/* OS(AIX) - AIX */
#if defined(_AIX)
#define WTF_OS_AIX 1
#endif

/* OS(DARWIN) - Any Darwin-based OS, including macOS, iOS, macCatalyst, tvOS, and watchOS */
#if defined(__APPLE__)
#define WTF_OS_DARWIN 1
#endif

/* OS(IOS_FAMILY) - iOS family, including iOS, iPadOS, macCatalyst, tvOS, watchOS */
#if OS(DARWIN) && TARGET_OS_IPHONE
#define WTF_OS_IOS_FAMILY 1
#endif

/* OS(IOS) - iOS and iPadOS only (iPhone and iPad), not including macCatalyst, not including watchOS, not including tvOS */
#if OS(DARWIN) && (TARGET_OS_IOS && !(defined(TARGET_OS_MACCATALYST) && TARGET_OS_MACCATALYST))
#define WTF_OS_IOS 1
#endif

/* OS(TVOS) - tvOS */
#if OS(DARWIN) && TARGET_OS_TV
#define WTF_OS_TVOS 1
#endif

/* OS(WATCHOS) - watchOS */
#if OS(DARWIN) && TARGET_OS_WATCH
#define WTF_OS_WATCHOS 1
#endif

/* FIXME: Rename this to drop the X, as that is no longer the name of the operating system. */
/* OS(MAC_OS_X) - macOS (not including iOS family) */
#if OS(DARWIN) && TARGET_OS_OSX
#define WTF_OS_MAC_OS_X 1
#endif

/* OS(FREEBSD) - FreeBSD */
#if defined(__FreeBSD__) || defined(__DragonFly__) || defined(__FreeBSD_kernel__)
#define WTF_OS_FREEBSD 1
#endif

/* OS(FUCHSIA) - Fuchsia */
#if defined(__Fuchsia__)
#define WTF_OS_FUCHSIA 1
#endif

/* OS(HURD) - GNU/Hurd */
#if defined(__GNU__)
#define WTF_OS_HURD 1
#endif

/* OS(LINUX) - Linux */
#if defined(__linux__)
#define WTF_OS_LINUX 1
#endif

/* OS(NETBSD) - NetBSD */
#if defined(__NetBSD__)
#define WTF_OS_NETBSD 1
#endif

/* OS(OPENBSD) - OpenBSD */
#if defined(__OpenBSD__)
#define WTF_OS_OPENBSD 1
#endif

/* OS(WINDOWS) - Any version of Windows */
#if defined(WIN32) || defined(_WIN32)
#define WTF_OS_WINDOWS 1
#endif


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


#if CPU(ADDRESS64)
#if OS(DARWIN)
#define WTF_OS_CONSTANT_EFFECTIVE_ADDRESS_WIDTH (WTF::getMSBSetConstexpr(MACH_VM_MAX_ADDRESS) + 1)
#else
/* We strongly assume that effective address width is <= 48 in 64bit architectures (e.g. NaN boxing). */
#define WTF_OS_CONSTANT_EFFECTIVE_ADDRESS_WIDTH 48
#endif
#else
#define WTF_OS_CONSTANT_EFFECTIVE_ADDRESS_WIDTH 32
#endif


/* Asserts, invariants for macro definitions */

#define WTF_OS_WIN ERROR "USE WINDOWS WITH OS NOT WIN"
#define WTF_OS_MAC ERROR "USE MAC_OS_X WITH OS NOT MAC"
