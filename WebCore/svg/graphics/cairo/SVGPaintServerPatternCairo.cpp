/*
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2008 Dirk Schulze <vbs85@gmx.de>
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
#include "SVGPaintServerPattern.h"

#include "GraphicsContext.h"
#include "Image.h"
#include "ImageBuffer.h"
#include "RenderObject.h"
#include "SVGPatternElement.h"

#include <wtf/OwnArrayPtr.h>

namespace WebCore {

// TODO: Share this code with all SVGPaintServers
static void applyStrokeStyleToContext(GraphicsContext* context, const SVGRenderStyle* style, const RenderObject* object)
{
    cairo_t* cr = context->platformContext();

    context->setStrokeThickness(SVGRenderStyle::cssPrimitiveToLength(object, style->strokeWidth(), 1.0));
    context->setLineCap(style->capStyle());
    context->setLineJoin(style->joinStyle());
    if (style->joinStyle() == MiterJoin)
        context->setMiterLimit(style->strokeMiterLimit());

    const DashArray& dashes = dashArrayFromRenderingStyle(object->style());
    OwnArrayPtr<double> dsh(new double[dashes.size()]);
    for (size_t i = 0; i < dashes.size() ; i++)
        dsh[i] = dashes[i];
    double dashOffset = SVGRenderStyle::cssPrimitiveToLength(object, style->strokeDashOffset(), 0.0);
    cairo_set_dash(cr, dsh.get(), dashes.size(), dashOffset);
}

bool SVGPaintServerPattern::setup(GraphicsContext*& context, const RenderObject* object, SVGPaintTargetType type, bool isPaintingText) const
{
    FloatRect targetRect = object->relativeBBox(false);

    const SVGRenderStyle* style = object->style()->svgStyle();

    float strokeWidth = SVGRenderStyle::cssPrimitiveToLength(object, style->strokeWidth(), 1.0);

    if (targetRect.width() == 0)
        targetRect = FloatRect(targetRect.x(), targetRect.y(), strokeWidth, targetRect.height());
    if (targetRect.height() == 0)
        targetRect = FloatRect(targetRect.x(), targetRect.y(), targetRect.width(), strokeWidth);

    m_ownerElement->buildPattern(targetRect);

    cairo_surface_t* image = tile()->image()->nativeImageForCurrentFrame();
    if (!image)
        return false;

    cairo_t* cr = context->platformContext();

    cairo_pattern_t* pattern = cairo_pattern_create_for_surface(image);
    cairo_pattern_set_extend(pattern, CAIRO_EXTEND_REPEAT);

    cairo_pattern_set_filter(pattern, CAIRO_FILTER_BEST);
    cairo_matrix_t pattern_matrix = patternTransform();
    cairo_matrix_t matrix = {1, 0, 0, 1, patternBoundaries().x(), patternBoundaries().y()};
    cairo_matrix_multiply(&matrix, &matrix, &pattern_matrix);
    cairo_matrix_invert(&matrix);
    cairo_pattern_set_matrix(pattern, &matrix);

    if ((type & ApplyToFillTargetType) && style->hasFill())
        cairo_set_fill_rule(cr, style->fillRule() == RULE_EVENODD ? CAIRO_FILL_RULE_EVEN_ODD : CAIRO_FILL_RULE_WINDING);

    if ((type & ApplyToStrokeTargetType) && style->hasStroke())
        applyStrokeStyleToContext(context, style, object);

    cairo_set_source(cr, pattern);
    cairo_pattern_destroy(pattern);

    return true;
}

} // namespace WebCore

#endif
