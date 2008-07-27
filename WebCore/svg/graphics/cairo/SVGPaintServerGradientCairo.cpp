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
#include "SVGPaintServerGradient.h"
#include "SVGPaintServerLinearGradient.h"
#include "SVGPaintServerRadialGradient.h"

#include "GraphicsContext.h"
#include "RenderObject.h"
#include "RenderPath.h"
#include "RenderStyle.h"
#include "SVGGradientElement.h"
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
    for (size_t i = 0 ; i < dashes.size() ; i++)
        dsh[i] = dashes[i];
    double dashOffset = SVGRenderStyle::cssPrimitiveToLength(object, style->strokeDashOffset(), 0.0);
    cairo_set_dash(cr, dsh.get(), dashes.size(), dashOffset);
}

bool SVGPaintServerGradient::setup(GraphicsContext*& context, const RenderObject* object, SVGPaintTargetType type, bool isPaintingText) const
{
    m_ownerElement->buildGradient();

    cairo_t* cr = context->platformContext();
    cairo_pattern_t* pattern;

    cairo_matrix_t matrix;
    cairo_matrix_init_identity (&matrix);
    const cairo_matrix_t gradient_matrix = gradientTransform();
    const SVGRenderStyle* style = object->style()->svgStyle();

    if (this->type() == LinearGradientPaintServer) {
        const SVGPaintServerLinearGradient* linear = static_cast<const SVGPaintServerLinearGradient*>(this);

        if (boundingBoxMode()) {
            FloatRect bbox = object->relativeBBox(false);
            if (bbox.width() == 0 || bbox.height() == 0) {
                applyStrokeStyleToContext(context, style, object);
                cairo_set_source_rgb(cr, 0, 0, 0);
                return true;
            }
            cairo_matrix_translate(&matrix, bbox.x(), bbox.y());
            cairo_matrix_scale(&matrix, bbox.width(), bbox.height());
        }

        double x0 = linear->gradientStart().x();
        double y0 = linear->gradientStart().y();
        double x1 = linear->gradientEnd().x();
        double y1 = linear->gradientEnd().y();

        pattern = cairo_pattern_create_linear(x0, y0, x1, y1);

    } else if (this->type() == RadialGradientPaintServer) {
        const SVGPaintServerRadialGradient* radial = static_cast<const SVGPaintServerRadialGradient*>(this);

        if (boundingBoxMode()) {
            FloatRect bbox = object->relativeBBox(false);
            if (bbox.width() == 0 || bbox.height() == 0) {
                applyStrokeStyleToContext(context, style, object);
                cairo_set_source_rgb(cr, 0, 0, 0);
                return true;
            }
            cairo_matrix_translate(&matrix, bbox.x(), bbox.y());
            cairo_matrix_scale(&matrix, bbox.width(), bbox.height());
        }

        double cx = radial->gradientCenter().x();
        double cy = radial->gradientCenter().y();
        double radius = radial->gradientRadius();
        double fx = radial->gradientFocal().x();
        double fy = radial->gradientFocal().y();

        fx -= cx;
        fy -= cy;

        double fradius = 0.0;

        if (sqrt(fx * fx + fy * fy) >= radius) {
            double angle = atan2(fy, fx);
            if ((fx + cx) < cx)
                fx = cos(angle) * radius + 0.002;
            else
                fx = cos(angle) * radius - 0.002;
            if ((fy + cy) < cy)
                fy = sin(angle) * radius + 0.002;
            else
                fy = sin(angle) * radius - 0.002;
        }

        pattern = cairo_pattern_create_radial(fx + cx, fy + cy, fradius, cx, cy, radius);

    } else {
        return false;
    }

    cairo_pattern_set_filter(pattern, CAIRO_FILTER_BILINEAR);

    switch (spreadMethod()) {
        case SPREADMETHOD_PAD:
            cairo_pattern_set_extend(pattern, CAIRO_EXTEND_PAD);
            break;
        case SPREADMETHOD_REFLECT:
            cairo_pattern_set_extend(pattern, CAIRO_EXTEND_REFLECT);
            break;
        case SPREADMETHOD_REPEAT:
            cairo_pattern_set_extend(pattern, CAIRO_EXTEND_REPEAT);
            break;
        default:
            cairo_pattern_set_extend(pattern, CAIRO_EXTEND_NONE);
            break;
    }

    cairo_matrix_multiply(&matrix, &matrix, &gradient_matrix);
    cairo_matrix_invert(&matrix);
    cairo_pattern_set_matrix(pattern, &matrix);

    const Vector<SVGGradientStop>& stops = gradientStops();

    for (unsigned i = 0; i < stops.size(); ++i) {
        float offset = stops[i].first;
        Color color = stops[i].second;
        if (i > 0 && offset < stops[i - 1].first)
            offset = stops[i - 1].first;

        cairo_pattern_add_color_stop_rgba(pattern, offset, color.red() / 255.0, color.green() / 255.0, color.blue() / 255.0, color.alpha() / 255.0);
    }

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
