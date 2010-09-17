/*
 *  Copyright (C) 2010 Igalia S.L.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#ifndef PlatformRefPtrCairo_h
#define PlatformRefPtrCairo_h

#include "PlatformRefPtr.h"

typedef struct _cairo cairo_t;
typedef struct _cairo_surface cairo_surface_t;
typedef struct _cairo_scaled_font cairo_scaled_font_t;

#if defined(USE_FREETYPE)
typedef struct _FcPattern FcPattern;
#endif

namespace WTF {

template <> cairo_t* refPlatformPtr(cairo_t* ptr);
template <> void derefPlatformPtr(cairo_t* ptr);

template <> cairo_surface_t* refPlatformPtr(cairo_surface_t* ptr);
template <> void derefPlatformPtr(cairo_surface_t* ptr);

template <> cairo_scaled_font_t* refPlatformPtr(cairo_scaled_font_t*);
template <> void derefPlatformPtr(cairo_scaled_font_t*);

#if defined(USE_FREETYPE)
template <> FcPattern* refPlatformPtr(FcPattern*);
template <> void derefPlatformPtr(FcPattern*);
#endif

}

#endif
