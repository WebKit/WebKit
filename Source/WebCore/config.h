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

#include "PlatformExportMacros.h"
#include <JavaScriptCore/JSExportMacros.h>
#include <pal/ExportMacros.h>

// Using CMake with Unix makefiles does not use prefix headers.
#if PLATFORM(MAC) && defined(BUILDING_WITH_CMAKE)
#include "WebCorePrefix.h"
#ifndef JSC_API_AVAILABLE
#define JSC_API_AVAILABLE(...)
#endif
#ifndef JSC_CLASS_AVAILABLE
#define JSC_CLASS_AVAILABLE(...) JS_EXPORT
#endif
#ifndef JSC_API_DEPRECATED
#define JSC_API_DEPRECATED(...)
#endif
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
