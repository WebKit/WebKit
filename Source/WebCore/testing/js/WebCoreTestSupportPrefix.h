/*
 * Copyright (C) 2015 Apple Inc.
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

/* This prefix file should contain only: 
 *    1) files to precompile for faster builds
 *    2) in one case at least: OS-X-specific performance bug workarounds
 *    3) the special trick to catch us using new or delete without including "config.h"
 * The project should be able to build without this header, although we rarely test that.
 */

/* Things that need to be defined globally should go into "config.h". */

#if defined(HAVE_CONFIG_H) && HAVE_CONFIG_H && defined(BUILDING_WITH_CMAKE)
#include "cmakeconfig.h"
#endif

#include <wtf/Platform.h>

#if defined(__APPLE__)
#ifdef __cplusplus
#define NULL __null
#else
#define NULL ((void *)0)
#endif
#endif

#if OS(WINDOWS)
#undef WEBCORE_EXPORT
#define WEBCORE_EXPORT WTF_IMPORT_DECLARATION
#define WEBCORE_TESTSUPPORT_EXPORT WTF_EXPORT_DECLARATION
#else

#include <pthread.h>

#define WEBCORE_TESTSUPPORT_EXPORT WEBCORE_EXPORT

#endif // OS(WINDOWS)

#include <fcntl.h>
#include <sys/types.h>
#if HAVE(REGEX_H)
#include <regex.h>
#endif

#include <setjmp.h>

#include <signal.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#if defined(__APPLE__)
#include <unistd.h>
#endif

#ifdef __cplusplus
#include <algorithm>
#include <cstddef>
#include <new>
#endif

#if defined(__APPLE__)
#include <sys/param.h>
#endif
#include <sys/stat.h>
#if defined(__APPLE__)
#include <sys/resource.h>
#include <sys/time.h>
#endif

#if USE(CF)
#include <CoreFoundation/CoreFoundation.h>
#endif

#if PLATFORM(WIN_CAIRO)
#include <windows.h>
#else

#if OS(WINDOWS)
#if USE(CG)

// FIXME <rdar://problem/8208868> Remove support for obsolete ColorSync API, CoreServices header in CoreGraphics
// We can remove this once the new ColorSync APIs are available in an internal Safari SDK.
#include <ColorSync/ColorSync.h>
#ifdef __COLORSYNCDEPRECATED__
#define COREGRAPHICS_INCLUDES_CORESERVICES_HEADER
#define OBSOLETE_COLORSYNC_API
#endif
#endif
#if USE(CFURLCONNECTION)
#include <CFNetwork/CFNetwork.h>
// On Windows, dispatch.h needs to be included before certain CFNetwork headers.
#include <dispatch/dispatch.h>
#endif
#include <windows.h>
#else
#if !PLATFORM(IOS_FAMILY)
#include <CoreServices/CoreServices.h>
#endif // !PLATFORM(IOS_FAMILY)
#endif // OS(WINDOWS)

#endif

#ifdef __OBJC__
#if PLATFORM(IOS_FAMILY)
#import <Foundation/Foundation.h>
#else
#import <Cocoa/Cocoa.h>
#endif // PLATFORM(IOS_FAMILY)
#endif

#ifdef __cplusplus
#define new ("if you use new/delete make sure to include config.h at the top of the file"()) 
#define delete ("if you use new/delete make sure to include config.h at the top of the file"()) 
#endif

/* When C++ exceptions are disabled, the C++ library defines |try| and |catch|
 * to allow C++ code that expects exceptions to build. These definitions
 * interfere with Objective-C++ uses of Objective-C exception handlers, which
 * use |@try| and |@catch|. As a workaround, undefine these macros. */
#ifdef __OBJC__
#undef try
#undef catch
#endif

