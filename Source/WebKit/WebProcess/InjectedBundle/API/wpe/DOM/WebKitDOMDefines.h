/*
 * Copyright (C) 2013 Igalia S.L.
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
 */

#if !defined(__WEBKITDOM_H_INSIDE__) && !defined(WEBKIT2_COMPILATION)
#error "Only <wpe/webkitdom.h> can be included directly."
#endif

#ifndef WebKitDOMDefines_h
#define WebKitDOMDefines_h

#include <glib.h>

#ifdef G_OS_WIN32
    #ifdef BUILDING_WEBKIT
        #define WEBKIT_API __declspec(dllexport)
    #else
        #define WEBKIT_API __declspec(dllimport)
    #endif
#else
    #define WEBKIT_API __attribute__((visibility("default")))
#endif

#define WEBKIT_DEPRECATED WEBKIT_API G_DEPRECATED
#define WEBKIT_DEPRECATED_FOR(f) WEBKIT_API G_DEPRECATED_FOR(f)

#ifndef WEBKIT_API
    #define WEBKIT_API
#endif

#endif /* WebKitDOMDefines_h */
