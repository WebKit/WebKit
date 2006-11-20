/*
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.
 * Copyright (C) 2006 Samuel Weinig <sam.weinig@gmail.com>
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#include <wtf/Platform.h>

#define XSLT_SUPPORT 1
#define MOBILE 0

#if __APPLE__
#define HAVE_FUNC_USLEEP 1

#ifndef CGFLOAT_DEFINED
#ifdef __LP64__
typedef double CGFloat;
#else
typedef float CGFloat;
#endif
#define CGFLOAT_DEFINED 1
#endif

#endif /* __APPLE__ */


#if PLATFORM(WIN_OS)

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0500
#endif

#ifndef WINVER
#define WINVER 0x0500
#endif

// We need to define this in order to use the numeric limit 
// values provided by the C++ standard library.
#define NOMINMAX

// Hack to match configuration of JavaScriptCore.
// Maybe there's a better way to do this.
#define USE_SYSTEM_MALLOC 1

// FIXME: Should probably just dump this eventually, but it's needed for now.
// We get this from some system place on OS X; probably better not to use it
// in WebCore code.
#include <assert.h>

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

#if !COMPILER(MSVC) // can't get this to compile on Visual C++ yet
#define AVOID_STATIC_CONSTRUCTORS 1
#endif

#if PLATFORM(MAC)
#define WTF_USE_JAVASCRIPTCORE_BINDINGS 1
#define WTF_USE_NPOBJECT 1
#endif

#if PLATFORM(SYMBIAN)
#define WTF_USE_JAVASCRIPTCORE_BINDINGS 1
#define WTF_USE_NPOBJECT 1
#undef XSLT_SUPPORT
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
