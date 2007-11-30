/*
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
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

#if ENABLE(SVG)
#include "SVGResourceMasker.h"
#include "ImageBuffer.h"
#include "GraphicsContext.h"

#include <cairo.h>

namespace WebCore {

void SVGResourceMasker::applyMask(GraphicsContext* context, const FloatRect& boundingBox)
{
    cairo_t* cr = context->platformContext();
    cairo_surface_t* surface = m_mask->surface();
    if (!surface)
        return;
    cairo_pattern_t* mask = cairo_pattern_create_for_surface(surface);
    cairo_mask(cr, mask);
    cairo_pattern_destroy(mask);
}

} // namespace WebCore

#endif
