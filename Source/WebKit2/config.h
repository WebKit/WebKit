/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Portions Copyright (c) 2010 Motorola Mobility, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */ 

#if defined (BUILDING_GTK__)
#include "autotoolsconfig.h"
#endif /* defined (BUILDING_GTK__) */

#if defined (BUILDING_WITH_CMAKE)
#include "cmakeconfig.h"
#endif

#include <wtf/DisallowCType.h>
#include <wtf/Platform.h>
#include <wtf/ExportMacros.h>
#if USE(JSC)
#include <runtime/JSExportMacros.h>
#endif

#ifdef __cplusplus
#ifndef EXTERN_C_BEGIN
#define EXTERN_C_BEGIN extern "C" {
#endif
#ifndef EXTERN_C_END
#define EXTERN_C_END }
#endif
#else
#define EXTERN_C_BEGIN
#define EXTERN_C_END
#endif

// For defining getters to a static value, where the getters have internal linkage
#define DEFINE_STATIC_GETTER(type, name, arguments) \
static const type& name() \
{ \
    DEFINE_STATIC_LOCAL(type, name##Value, arguments); \
    return name##Value; \
}

#if defined(__APPLE__)

#ifdef __OBJC__
#define OBJC_CLASS @class
#else
#define OBJC_CLASS class
#endif

#if !defined(BUILDING_ON_LEOPARD) && !defined(BUILDING_ON_SNOW_LEOPARD)
#define ENABLE_WEB_PROCESS_SANDBOX 1
#endif

#define ENABLE_PLUGIN_PROCESS 1

#if PLATFORM(MAC)
#define ENABLE_MEMORY_SAMPLER 1
#endif

#import <CoreGraphics/CoreGraphics.h>

#ifdef __OBJC__
#import <Cocoa/Cocoa.h>
#endif


#include <WebCore/EmptyProtocolDefinitions.h>

#elif defined(WIN32) || defined(_WIN32)

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0500
#endif

#ifndef WINVER
#define WINVER 0x0500
#endif

/* If we don't define these, they get defined in windef.h. */
/* We want to use std::min and std::max. */
#ifndef max
#define max max
#endif
#ifndef min
#define min min
#endif

#ifndef _WINSOCKAPI_
#define _WINSOCKAPI_ /* Prevent inclusion of winsock.h in windows.h */
#endif

#include <WebCore/config.h>
#include <windows.h>

#if USE(CG)
#include <CoreGraphics/CoreGraphics.h>
#endif

#endif /* defined(WIN32) || defined(_WIN32) */

#ifdef __cplusplus

// These undefs match up with defines in WebKit2Prefix.h for Mac OS X.
// Helps us catch if anyone uses new or delete by accident in code and doesn't include "config.h".
#undef new
#undef delete
#include <wtf/FastMalloc.h>

#endif

#ifndef PLUGIN_ARCHITECTURE_UNSUPPORTED
#if PLATFORM(MAC)
#define PLUGIN_ARCHITECTURE_MAC 1
#elif PLATFORM(WIN)
#define PLUGIN_ARCHITECTURE_WIN 1
#elif PLATFORM(GTK) && (OS(UNIX) && !OS(MAC_OS_X))
#define PLUGIN_ARCHITECTURE_X11 1
#else
#define PLUGIN_ARCHITECTURE_UNSUPPORTED 1
#endif
#endif

#define PLUGIN_ARCHITECTURE(ARCH) (defined PLUGIN_ARCHITECTURE_##ARCH && PLUGIN_ARCHITECTURE_##ARCH)
