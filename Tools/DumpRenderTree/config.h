/*
 * Copyright (C) 2008 Nuanti Ltd.
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

#define Config_H

#if defined(HAVE_CONFIG_H) && HAVE_CONFIG_H
#if defined(BUILDING_WITH_CMAKE)
#include "cmakeconfig.h"
#else
#include "autotoolsconfig.h"
#endif
#endif

#include <wtf/Platform.h>

/* See note in wtf/Platform.h for more info on EXPORT_MACROS. */
#if USE(EXPORT_MACROS)

#include <wtf/ExportMacros.h>

#define WTF_EXPORT_PRIVATE WTF_IMPORT
#define JS_EXPORT_PRIVATE WTF_IMPORT
#define WEBKIT_EXPORTDATA WTF_IMPORT

#define JS_EXPORTDATA JS_EXPORT_PRIVATE
#define JS_EXPORTCLASS JS_EXPORT_PRIVATE

#else /* !USE(EXPORT_MACROS) */

#if OS(WINDOWS) && !COMPILER(GCC) && !defined(BUILDING_WX__)
#define JS_EXPORTDATA __declspec(dllimport)
#define WEBKIT_EXPORTDATA __declspec(dllimport)
#else
#define JS_EXPORTDATA
#define WEBKIT_EXPORTDATA
#endif

#define WTF_EXPORT_PRIVATE
#define JS_EXPORT_PRIVATE

#endif /* USE(EXPORT_MACROS) */

// On MSW, wx headers need to be included before windows.h is.
// The only way we can always ensure this is if we include wx here.
#if PLATFORM(WX)
#include <wx/defs.h>
#endif


#ifdef __cplusplus
#undef new
#undef delete
#include <wtf/FastMalloc.h>
#endif

#if PLATFORM(MAC)
#define WTF_USE_CF 1

#if !defined(MAC_OS_X_VERSION_10_6) || MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_6
#define BUILDING_ON_LEOPARD 1
#elif !defined(MAC_OS_X_VERSION_10_7) || MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_7
#define BUILDING_ON_SNOW_LEOPARD 1
#endif
#endif // PLATFORM(MAC)

#if OS(WINDOWS)
// If we don't define these, they get defined in windef.h. 
// We want to use std::min and std::max
#undef max
#define max max
#undef min
#define min min
#endif

#if PLATFORM(WIN)
#define WTF_USE_CF 1 
#if PLATFORM(WIN_CAIRO)
#define WTF_USE_CAIRO 1
#define WTF_USE_CURL 1
#else
#define WTF_USE_CG 1
#define WTF_USE_CFNETWORK 1
#endif

#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0500

#undef WINVER
#define WINVER 0x0500

#undef _WINSOCKAPI_
#define _WINSOCKAPI_ // Prevent inclusion of winsock.h in windows.h
#endif  // PLATFORM(WIN)
