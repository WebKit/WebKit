/*
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.
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

#define KHTML_NO_XBL 1
#define KHTML_XSLT 1

#if __APPLE__
#define HAVE_FUNC_USLEEP 1

#ifndef CGFLOAT_DEFINED
#if __LP64__
typedef double CGFloat;
#else
typedef float CGFloat;
#endif
#define CGFLOAT_DEFINED 1
#endif

#endif

#if WIN32

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0500
#endif

#ifndef WINVER
#define WINVER 0x0500
#endif

#ifndef _HAS_EXCEPTIONS
#define _HAS_EXCEPTIONS 0
#endif

#ifndef _SECURE_SCL_DEPRECATE
#define _SECURE_SCL_DEPRECATE 0
#endif

// Hack to match configuration of JavaScriptCore.
// Maybe there's a better way to do this.
#define USE_SYSTEM_MALLOC 1

// FIXME: Should probably just dump this eventually, but it's needed for now.
// We get this from some system place on OS X; probably better not to use it
// in WebCore code.

#include <assert.h>

#endif

#ifdef __cplusplus

// These undefs match up with defines in WebCorePrefix.h for Mac OS X.
// Helps us catch if anyone uses new or delete by accident in code and doesn't include "config.h".
#undef new
#undef delete
#include <wtf/FastMalloc.h>

#endif

#if !defined(WIN32) // can't get this to compile on Visual C++ yet
#define AVOID_STATIC_CONSTRUCTORS 1
#endif
