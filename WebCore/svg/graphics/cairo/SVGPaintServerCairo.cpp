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
#include "SVGPaintServer.h"

#include "GraphicsContext.h"
#include "SVGPaintServer.h"
#include "RenderPath.h"

#include <cairo.h>

namespace WebCore {

void SVGPaintServer::draw(GraphicsContext*& context, const RenderObject* path, SVGPaintTargetType type) const
{
    if (!setup(context, path, type))
        return;

    renderPath(context, path, type);
    teardown(context, path, type);
}

void SVGPaintServer::teardown(GraphicsContext*&, const RenderObject*, SVGPaintTargetType, bool isPaintingText) const
{
    // no-op
}

void SVGPaintServer::renderPath(GraphicsContext*& context, const RenderObject* path, SVGPaintTargetType type) const
{
    cairo_t* cr = context->platformContext();
    const SVGRenderStyle* style = path->style()->svgStyle();

    cairo_set_fill_rule(cr, style->fillRule() == RULE_EVENODD ? CAIRO_FILL_RULE_EVEN_ODD : CAIRO_FILL_RULE_WINDING);

    if ((type & ApplyToFillTargetType) && style->hasFill())
        cairo_fill_preserve(cr);

    if ((type & ApplyToStrokeTargetType) && style->hasStroke())
        cairo_stroke_preserve(cr);

    cairo_new_path(cr);
}

} // namespace WebCore

#endif
