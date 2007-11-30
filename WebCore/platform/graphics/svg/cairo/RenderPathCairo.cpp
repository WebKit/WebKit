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
#include "RenderPath.h"

#include "CairoPath.h"
#include "SVGPaintServer.h"

namespace WebCore {

bool RenderPath::strokeContains(const FloatPoint& point, bool requiresStroke) const
{
    if (requiresStroke && !SVGPaintServer::strokePaintServer(style(), this))
        return false;

    cairo_t* cr = path().platformPath()->m_cr;

    // TODO: set stroke properties
    return cairo_in_stroke(cr, point.x(), point.y());
}

FloatRect RenderPath::strokeBBox() const
{
    // TODO: this implementation is naive

    cairo_t* cr = path().platformPath()->m_cr;

    double x0, x1, y0, y1;
    cairo_stroke_extents(cr, &x0, &y0, &x1, &y1);
    FloatRect bbox = FloatRect(x0, y0, x1 - x0, y1 - y0);

    return bbox;
}

}
