/*
 * Copyright (C) 2007 Alp Toker <alp.toker@collabora.co.uk>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
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

namespace WebCore {

bool SVGPaintServerGradient::setup(GraphicsContext*& context, const RenderObject* object, SVGPaintTargetType type, bool isPaintingText) const
{
    m_ownerElement->buildGradient();

    cairo_t* cr = context->platformContext();
    cairo_pattern_t* pattern;

    cairo_matrix_t matrix;
    cairo_matrix_init_identity (&matrix);
    const cairo_matrix_t gradient_matrix = gradientTransform();

    // TODO: revise this code, it is known not to work in many cases
    if (this->type() == LinearGradientPaintServer) {
        const SVGPaintServerLinearGradient* linear = static_cast<const SVGPaintServerLinearGradient*>(this);

        if (boundingBoxMode()) {
            // TODO: use RenderPathCairo's strokeBBox?
            double x1, y1, x2, y2;
            cairo_fill_extents(cr, &x1, &y1, &x2, &y2);
            cairo_matrix_translate(&matrix, x1, y1);
            cairo_matrix_scale(&matrix, x2 - x1, y2 - y1);
            cairo_matrix_multiply(&matrix, &matrix, &gradient_matrix);
            cairo_matrix_invert(&matrix);
        }

        double x0, x1, y0, y1;
        x0 = linear->gradientStart().x();
        y0 = linear->gradientStart().y();
        x1 = linear->gradientEnd().x();
        y1 = linear->gradientEnd().y();
        pattern = cairo_pattern_create_linear(x0, y0, x1, y1);

    } else if (this->type() == RadialGradientPaintServer) {
        const SVGPaintServerRadialGradient* radial = static_cast<const SVGPaintServerRadialGradient*>(this);
        // TODO: pattern = cairo_pattern_create_radial();
        return false;
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

    cairo_pattern_set_matrix(pattern, &matrix);

    const Vector<SVGGradientStop>& stops = gradientStops();

    for (unsigned i = 0; i < stops.size(); ++i) {
        float offset = stops[i].first;
        Color color = stops[i].second;

        cairo_pattern_add_color_stop_rgba(pattern, offset, color.red(), color.green(), color.blue(), color.alpha());
    }

    cairo_set_source(cr, pattern);
    cairo_pattern_destroy(pattern);

    return true;
}

} // namespace WebCore

#endif
