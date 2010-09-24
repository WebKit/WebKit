/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#include <wtf/Platform.h>
#include <wtf/DisallowCType.h>
#ifdef __cplusplus
#include <wtf/FastMalloc.h>
#endif

#if defined(BUILDING_QT__)

#define WTF_USE_JSC 1
#define WTF_USE_V8 0

#define JS_EXPORTDATA
#define JS_EXPORTCLASS

#elif defined(__APPLE__)

#if !defined(BUILDING_ON_LEOPARD) && !defined(BUILDING_ON_SNOW_LEOPARD)
#define ENABLE_WEB_PROCESS_SANDBOX 1
#endif

// FIXME: Enable once this works well enough.
#define ENABLE_PLUGIN_PROCESS 0

#import <CoreGraphics/CoreGraphics.h>

#ifdef __OBJC__
#import <Cocoa/Cocoa.h>
#endif

/* WebKit has no way to pull settings from WebCore/config.h for now */
/* so we assume WebKit is always being compiled on top of JavaScriptCore */
#define WTF_USE_JSC 1
#define WTF_USE_V8 0

#define JS_EXPORTDATA
#define JS_EXPORTCLASS
#define WEBKIT_EXPORTDATA

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

#if PLATFORM(CG)
#include <CoreGraphics/CoreGraphics.h>
#endif

#endif /* defined(WIN32) || defined(_WIN32) */
