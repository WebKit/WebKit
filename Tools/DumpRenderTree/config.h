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
#include <wtf/ExportMacros.h>
#include <runtime/JSExportMacros.h>

#ifdef __cplusplus
#undef new
#undef delete
#include <wtf/FastMalloc.h>
#endif

#if PLATFORM(COCOA)
#define WTF_USE_CF 1
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
#define _WIN32_WINNT 0x0502

#undef WINVER
#define WINVER 0x0502

#undef _WINSOCKAPI_
#define _WINSOCKAPI_ // Prevent inclusion of winsock.h in windows.h
#endif  // PLATFORM(WIN)
