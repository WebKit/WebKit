/*
 * Copyright (C) 2004, 2005, 2006, 2013, 2014 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#if defined(HAVE_CONFIG_H) && HAVE_CONFIG_H && defined(BUILDING_WITH_CMAKE)
#include "cmakeconfig.h"
#endif

#include <wtf/Platform.h>

#if PLATFORM(COCOA)
#define USE_FILE_LOCK 1
#endif

#if PLATFORM(WIN) && !USE(WINGDI)
#include "WebCoreHeaderDetection.h"
#endif

#include <wtf/ExportMacros.h>
#include "PlatformExportMacros.h"

#include <runtime/JSExportMacros.h>

#ifdef __APPLE__
#define HAVE_FUNC_USLEEP 1
#endif /* __APPLE__ */

#if OS(WINDOWS)

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x601
#endif

#ifndef WINVER
#define WINVER 0x0601
#endif

// CURL needs winsock, so don't prevent inclusion of it
#if !USE(CURL)
#ifndef _WINSOCKAPI_
#define _WINSOCKAPI_ // Prevent inclusion of winsock.h in windows.h
#endif
#endif

#endif /* OS(WINDOWS) */

// Using CMake with Unix makefiles does not use prefix headers.
#if PLATFORM(MAC) && defined(BUILDING_WITH_CMAKE)
#include "WebCorePrefix.h"
#endif

#ifdef __cplusplus

// These undefs match up with defines in WebCorePrefix.h for Mac OS X.
// Helps us catch if anyone uses new or delete by accident in code and doesn't include "config.h".
#undef new
#undef delete
#include <wtf/FastMalloc.h>

#include <ciso646>

#endif

#include <wtf/DisallowCType.h>

#if COMPILER(MSVC)
#define SKIP_STATIC_CONSTRUCTORS_ON_MSVC 1
#else
#define SKIP_STATIC_CONSTRUCTORS_ON_GCC 1
#endif

#if PLATFORM(WIN)
#if PLATFORM(WIN_CAIRO)
#undef USE_CG
#define USE_CAIRO 1
#define USE_CURL 1
#ifndef _WINSOCKAPI_
#define _WINSOCKAPI_ // Prevent inclusion of winsock.h in windows.h
#endif
#elif USE(DIRECT2D)
#undef USE_CA
#undef USE_CG
#elif !USE(WINGDI)
#define USE_CG 1
#undef USE_CAIRO
#undef USE_CURL
#endif
#endif

#if PLATFORM(MAC)
#define USE_NEW_THEME 1
#endif

#if USE(CG)
#ifndef CGFLOAT_DEFINED
#if (defined(__LP64__) && __LP64__) || (defined(__x86_64__) && __x86_64__) || defined(_M_X64) || defined(__amd64__)
typedef double CGFloat;
#else
typedef float CGFloat;
#endif
#define CGFLOAT_DEFINED 1
#endif
#endif /* USE(CG) */

// FIXME: Move this to JavaScriptCore/wtf/Platform.h, which is where we define USE_AVFOUNDATION on the Mac.
// https://bugs.webkit.org/show_bug.cgi?id=67334
#if PLATFORM(WIN) && USE(CG) && HAVE(AVCF)
#define USE_AVFOUNDATION 1

#if HAVE(AVCF_LEGIBLE_OUTPUT)
#define USE_AVFOUNDATION 1
#define HAVE_AVFOUNDATION_MEDIA_SELECTION_GROUP 1
#define HAVE_AVFOUNDATION_LEGIBLE_OUTPUT_SUPPORT 1
#define HAVE_MEDIA_ACCESSIBILITY_FRAMEWORK 1
#endif

#endif
