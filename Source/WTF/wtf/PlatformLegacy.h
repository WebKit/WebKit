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

#ifndef WTF_PLATFORM_GUARD_AGAINST_INDIRECT_INCLUSION
#error "Please #include <wtf/Platform.h> instead of this file directly."
#endif


/* PLATFORM() - handles OS, operating environment, graphics API, and
   CPU. This macro will be phased out in favor of platform adaptation
   macros, policy decision macros, and top-level port definitions. */
#define PLATFORM(WTF_FEATURE) (defined WTF_PLATFORM_##WTF_FEATURE && WTF_PLATFORM_##WTF_FEATURE)


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
#elif OS(MACOS)
#define WTF_PLATFORM_MAC 1
#elif OS(IOS_FAMILY)
#if OS(IOS)
/* PLATFORM(IOS) - iOS and iPadOS only (iPhone and iPad), not including macCatalyst, not including watchOS, not including tvOS */
#define WTF_PLATFORM_IOS 1
#endif
/* PLATFORM(IOS_FAMILY) - iOS family, including iOS, iPadOS, macCatalyst, tvOS, watchOS */
#define WTF_PLATFORM_IOS_FAMILY 1
#if TARGET_OS_SIMULATOR
#if OS(IOS)
#define WTF_PLATFORM_IOS_SIMULATOR 1
#endif
#define WTF_PLATFORM_IOS_FAMILY_SIMULATOR 1
#endif
#if defined(TARGET_OS_MACCATALYST) && TARGET_OS_MACCATALYST
#define WTF_PLATFORM_MACCATALYST 1
#endif
#elif OS(WINDOWS)
#define WTF_PLATFORM_WIN 1
#endif

/* PLATFORM(COCOA) */
#if PLATFORM(MAC) || PLATFORM(IOS_FAMILY)
#define WTF_PLATFORM_COCOA 1
#endif

/* PLATFORM(APPLETV) */
#if defined(TARGET_OS_TV) && TARGET_OS_TV
#define WTF_PLATFORM_APPLETV 1
#endif

/* PLATFORM(WATCHOS) */
#if defined(TARGET_OS_WATCH) && TARGET_OS_WATCH
#define WTF_PLATFORM_WATCHOS 1
#endif
