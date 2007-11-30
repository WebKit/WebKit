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
    const SVGRenderStyle* style = object->style()->svgStyle();

    float red, green, blue, alpha;
    color().getRGBA(red, green, blue, alpha);

    if ((type & ApplyToFillTargetType) && style->hasFill()) {
        alpha = style->fillOpacity();

        cairo_set_fill_rule(cr, style->fillRule() == RULE_EVENODD ? CAIRO_FILL_RULE_EVEN_ODD : CAIRO_FILL_RULE_WINDING);
    }

    if ((type & ApplyToStrokeTargetType) && style->hasStroke()) {
        alpha = style->strokeOpacity();

        cairo_set_line_width(cr, SVGRenderStyle::cssPrimitiveToLength(object, style->strokeWidth(), 1.0));
        context->setLineCap(style->capStyle());
        context->setLineJoin(style->joinStyle());
        if (style->joinStyle() == MiterJoin)
            context->setMiterLimit(style->strokeMiterLimit());

        const DashArray& dashes = dashArrayFromRenderingStyle(object->style());
        double* dsh = new double[dashes.size()];
        for (unsigned i = 0 ; i < dashes.size() ; i++)
            dsh[i] = dashes[i];
        double dashOffset = SVGRenderStyle::cssPrimitiveToLength(object, style->strokeDashOffset(), 0.0);
        cairo_set_dash(cr, dsh, dashes.size(), dashOffset);
        delete[] dsh;
    }

    cairo_set_source_rgba(cr, red, green, blue, alpha);

    return true;
}

} // namespace WebCore

#endif
