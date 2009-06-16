/*
 * Copyright (C) 2004, 2005, 2006 Apple Inc.
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#if defined(HAVE_CONFIG_H) && HAVE_CONFIG_H
#include "autotoolsconfig.h"
#endif

#include <wtf/Platform.h>

#if PLATFORM(WIN_OS) && !defined(BUILDING_WX__) && !COMPILER(GCC)
#if defined(BUILDING_JavaScriptCore) || defined(BUILDING_WTF)
#define JS_EXPORTDATA __declspec(dllexport)
#else
#define JS_EXPORTDATA __declspec(dllimport)
#endif
#if defined(BUILDING_WebCore) || defined(BUILDING_WebKit)
#define WEBKIT_EXPORTDATA __declspec(dllexport)
#else
#define WEBKIT_EXPORTDATA __declspec(dllimport)
#endif
#else
#define JS_EXPORTDATA
#define WEBKIT_EXPORTDATA
#endif

#define MOBILE 0

#ifdef __APPLE__
#define HAVE_FUNC_USLEEP 1
#endif /* __APPLE__ */

#if PLATFORM(WIN_OS)

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0500
#endif

#ifndef WINVER
#define WINVER 0x0500
#endif

// If we don't define these, they get defined in windef.h.
// We want to use std::min and std::max.
#ifndef max
#define max max
#endif
#ifndef min
#define min min
#endif

// CURL needs winsock, so don't prevent inclusion of it
#if !USE(CURL)
#ifndef _WINSOCKAPI_
#define _WINSOCKAPI_ // Prevent inclusion of winsock.h in windows.h
#endif
#endif

#endif /* PLATFORM(WIN_OS) */

// On MSW, wx headers need to be included before windows.h is.
// The only way we can always ensure this is if we include wx here.
#if PLATFORM(WX)
// The defines in KeyboardCodes.h conflict with Windows as well, and the only way I've found
// to address the problem is include KeyboarddCodes.h before windows.h, so do it here.
#include "KeyboardCodes.h"
#include <wx/defs.h>
#endif

#ifdef __cplusplus

// These undefs match up with defines in WebCorePrefix.h for Mac OS X.
// Helps us catch if anyone uses new or delete by accident in code and doesn't include "config.h".
#undef new
#undef delete
#include <wtf/FastMalloc.h>

#endif

// this breaks compilation of <QFontDatabase>, at least, so turn it off for now
// Also generates errors on wx on Windows, presumably because these functions
// are used from wx headers.
#if !PLATFORM(QT) && !PLATFORM(WX)
#include <wtf/DisallowCType.h>
#endif

#if COMPILER(MSVC)
#define SKIP_STATIC_CONSTRUCTORS_ON_MSVC 1
#else
#define SKIP_STATIC_CONSTRUCTORS_ON_GCC 1
#endif

#if PLATFORM(WIN)
#define WTF_PLATFORM_CG 1
#undef WTF_PLATFORM_CAIRO
#define WTF_USE_CFNETWORK 1
#undef WTF_USE_WININET
#define WTF_PLATFORM_CF 1
#define WTF_USE_PTHREADS 0
#endif

#if PLATFORM(MAC)
// ATSUI vs. CoreText
#if !defined(BUILDING_ON_TIGER) && !defined(BUILDING_ON_LEOPARD)
#define WTF_USE_ATSUI 0
#define WTF_USE_CORE_TEXT 1
#else
#define WTF_USE_ATSUI 1
#define WTF_USE_CORE_TEXT 0
#endif

// New theme
#define WTF_USE_NEW_THEME 1
#endif // PLATFORM(MAC)

#if PLATFORM(SYMBIAN)
#undef WIN32
#undef _WIN32
#undef SKIP_STATIC_CONSTRUCTORS_ON_GCC
#define USE_SYSTEM_MALLOC 1
#define U_HAVE_INT8_T 0
#define U_HAVE_INT16_T 0
#define U_HAVE_INT32_T 0
#define U_HAVE_INT64_T 0
#define U_HAVE_INTTYPES_H 0

#include <stdio.h>
#include <limits.h>
#include <wtf/MathExtras.h>
#endif

#if !defined(WTF_USE_V8)
/* Currently Chromium is the only platform which uses V8 by default */
#if PLATFORM(CHROMIUM)
#define WTF_USE_V8 1
#else
#define WTF_USE_V8 0
#endif /* PLATFORM(CHROMIUM) */
#endif /* !defined(WTF_USE_V8) */

/* Using V8 implies not using JSC and vice versa */
#define WTF_USE_JSC !WTF_USE_V8

#if PLATFORM(CG)
#ifndef CGFLOAT_DEFINED
#ifdef __LP64__
typedef double CGFloat;
#else
typedef float CGFloat;
#endif
#define CGFLOAT_DEFINED 1
#endif
#endif /* PLATFORM(CG) */

#ifdef BUILDING_ON_TIGER
#undef ENABLE_FTPDIR
#define ENABLE_FTPDIR 0
#endif

#if PLATFORM(WIN) && PLATFORM(CG)
#define WTF_USE_SAFARI_THEME 1
#endif
