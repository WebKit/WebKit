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
#include "SVGPaintServerSolid.h"

#include "GraphicsContext.h"
#include "SVGPaintServer.h"
#include "RenderPath.h"

namespace WebCore {

bool SVGPaintServerSolid::setup(GraphicsContext*& context, const RenderObject* object, SVGPaintTargetType type, bool isPaintingText) const
{
    // TODO: share this code with other PaintServers
    cairo_t* cr = context->platformContext();

    const SVGRenderStyle* svgStyle = object->style()->svgStyle();
    RenderStyle* style = object->style();

    float red, green, blue, alpha;
    color().getRGBA(red, green, blue, alpha);

    if ((type & ApplyToFillTargetType) && svgStyle->hasFill()) {
        alpha = svgStyle->fillOpacity();
        context->setFillRule(svgStyle->fillRule());
    }

    if ((type & ApplyToStrokeTargetType) && svgStyle->hasStroke()) {
        alpha = svgStyle->strokeOpacity();
        applyStrokeStyleToContext(context, style, object);
    }

    cairo_set_source_rgba(cr, red, green, blue, alpha);

    return true;
}

} // namespace WebCore

#endif
