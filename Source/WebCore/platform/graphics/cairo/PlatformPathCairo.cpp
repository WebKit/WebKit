/*
 * Copyright (C) 2011 Collabora Ltd.
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

#include "config.h"
#include "PlatformPathCairo.h"

#if USE(CAIRO)

#include <cairo.h>

namespace WebCore {

CairoPath::CairoPath()
{
    // cairo_t takes its own reference of the surface, meaning we don't have to keep one.
    auto surface = adoptRef(cairo_image_surface_create(CAIRO_FORMAT_A8, 1, 1));
    m_context = adoptRef(cairo_create(surface.get()));
}

} // namespace WebCore

#endif // USE(CAIRO)
