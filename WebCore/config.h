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

#include <wtf/Platform.h>

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

#if !PLATFORM(SYMBIAN)
#define IMPORT_C
#define EXPORT_C
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

#if PLATFORM(GTK)
#define WTF_USE_NPOBJECT 1
#define WTF_USE_JAVASCRIPTCORE_BINDINGS 1
#endif

#if !COMPILER(MSVC) // can't get this to compile on Visual C++ yet
#define AVOID_STATIC_CONSTRUCTORS 1
#endif

#if PLATFORM(WIN)
#define WTF_USE_JAVASCRIPTCORE_BINDINGS 1
#define WTF_USE_NPOBJECT 1
#define WTF_PLATFORM_CG 1
#undef WTF_PLATFORM_CAIRO
#define WTF_USE_CFNETWORK 1
#undef WTF_USE_WININET
#define WTF_PLATFORM_CF 1
#define WTF_USE_PTHREADS 0
#endif

#if PLATFORM(MAC)
#define WTF_USE_JAVASCRIPTCORE_BINDINGS 1
#ifdef __LP64__
#define WTF_USE_NPOBJECT 0
#else
#define WTF_USE_NPOBJECT 1
#endif
#endif

#if PLATFORM(SYMBIAN)
#define WTF_USE_JAVASCRIPTCORE_BINDINGS 1
#define WTF_USE_NPOBJECT 1
#undef WIN32
#undef _WIN32
#undef AVOID_STATIC_CONSTRUCTORS
#define USE_SYSTEM_MALLOC 1
#define U_HAVE_INT8_T 0
#define U_HAVE_INT16_T 0
#define U_HAVE_INT32_T 0
#define U_HAVE_INT64_T 0
#define U_HAVE_INTTYPES_H 0

#include <stdio.h>
#include <snprintf.h>
#include <limits.h>
#include <wtf/MathExtras.h>
#endif

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
