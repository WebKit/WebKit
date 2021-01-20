/*
 *  Copyright (C) 2010 Igalia S.L.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "RefPtrCairo.h"

#if USE(CAIRO)

#include <cairo.h>

namespace WTF {

void DefaultRefDerefTraits<cairo_t>::refIfNotNull(cairo_t* ptr)
{
    if (LIKELY(ptr))
        cairo_reference(ptr);
}

void DefaultRefDerefTraits<cairo_t>::derefIfNotNull(cairo_t* ptr)
{
    if (LIKELY(ptr))
        cairo_destroy(ptr);
}

void DefaultRefDerefTraits<cairo_surface_t>::refIfNotNull(cairo_surface_t* ptr)
{
    if (LIKELY(ptr))
        cairo_surface_reference(ptr);
}

void DefaultRefDerefTraits<cairo_surface_t>::derefIfNotNull(cairo_surface_t* ptr)
{
    if (LIKELY(ptr))
        cairo_surface_destroy(ptr);
}

void DefaultRefDerefTraits<cairo_font_face_t>::refIfNotNull(cairo_font_face_t* ptr)
{
    if (LIKELY(ptr))
        cairo_font_face_reference(ptr);
}

void DefaultRefDerefTraits<cairo_font_face_t>::derefIfNotNull(cairo_font_face_t* ptr)
{
    if (LIKELY(ptr))
        cairo_font_face_destroy(ptr);
}

void DefaultRefDerefTraits<cairo_scaled_font_t>::refIfNotNull(cairo_scaled_font_t* ptr)
{
    if (LIKELY(ptr))
        cairo_scaled_font_reference(ptr);
}

void DefaultRefDerefTraits<cairo_scaled_font_t>::derefIfNotNull(cairo_scaled_font_t* ptr)
{
    if (LIKELY(ptr))
        cairo_scaled_font_destroy(ptr);
}

void DefaultRefDerefTraits<cairo_pattern_t>::refIfNotNull(cairo_pattern_t* ptr)
{
    if (LIKELY(ptr))
        cairo_pattern_reference(ptr);
}

void DefaultRefDerefTraits<cairo_pattern_t>::derefIfNotNull(cairo_pattern_t* ptr)
{
    if (LIKELY(ptr))
        cairo_pattern_destroy(ptr);
}

void DefaultRefDerefTraits<cairo_region_t>::refIfNotNull(cairo_region_t* ptr)
{
    if (LIKELY(ptr))
        cairo_region_reference(ptr);
}

void DefaultRefDerefTraits<cairo_region_t>::derefIfNotNull(cairo_region_t* ptr)
{
    if (LIKELY(ptr))
        cairo_region_destroy(ptr);
}

} // namespace WTF

#endif // USE(CAIRO)
