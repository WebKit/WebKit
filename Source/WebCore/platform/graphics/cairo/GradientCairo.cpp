/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc.  All rights reserved.
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2019 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "Gradient.h"

#if USE(CAIRO)

#include "AnimationUtilities.h"
#include "CairoOperations.h"
#include "CairoUtilities.h"
#include "ColorBlending.h"
#include "GraphicsContext.h"
#include "PlatformContextCairo.h"
#include <wtf/MathExtras.h>

namespace WebCore {

void Gradient::stopsChanged()
{
}

static void addColorStopRGBA(cairo_pattern_t *gradient, Gradient::ColorStop stop, float globalAlpha)
{
    auto [r, g, b, a] = stop.color.toSRGBALossy<float>();
    cairo_pattern_add_color_stop_rgba(gradient, stop.offset, r, g, b, a * globalAlpha);
}

typedef struct point_t {
    double x, y;
} point_t;

static void setCornerColorRGBA(cairo_pattern_t* gradient, int id, Gradient::ColorStop stop, float globalAlpha)
{
    auto [r, g, b, a] = stop.color.toSRGBALossy<float>();
    cairo_mesh_pattern_set_corner_color_rgba(gradient, id, r, g, b, a * globalAlpha);
}

static void addConicSector(cairo_pattern_t *gradient, float cx, float cy, float r, float angleRadians,
    Gradient::ColorStop from, Gradient::ColorStop to, float globalAlpha)
{
    const double angOffset = 0.25; // 90 degrees.

    // Substract 90 degrees so angles start from top left.
    // Convert to radians and add angleRadians offset.
    double angleStart = ((from.offset - angOffset) * 2 * piDouble) + angleRadians;
    double angleEnd = ((to.offset - angOffset) * 2 * piDouble) + angleRadians;

    // Calculate center offset depending on quadrant.
    //
    // All sections belonging to the same quadrant share a common center. As we move
    // along the circle, sections belonging to a new quadrant will have a different
    // center. If all sections had the same center, the center will get overridden as
    // the sections get painted.
    double cxOffset, cyOffset;
    if (from.offset >= 0 && from.offset < 0.25) {
        cxOffset = 0;
        cyOffset = -1;
    } else if (from.offset >= 0.25 && from.offset < 0.50) {
        cxOffset = 0;
        cyOffset = 0;
    } else if (from.offset >= 0.50 && from.offset < 0.75) {
        cxOffset = -1;
        cyOffset = 0;
    } else if (from.offset >= 0.75 && from.offset < 1) {
        cxOffset = -1;
        cyOffset = -1;
    } else {
        cxOffset = 0;
        cyOffset = -1;
    }
    // The center offset for each of the sections is 1 pixel, since in theory nothing
    // can be smaller than 1 pixel. However, in high-resolution displays 1 pixel is
    // too wide, and that makes the separation between sections clearly visible by a
    // straight white line. To fix this issue, I set the size of the offset not to
    // 1 pixel but 0.10. This has proved to work OK both in low-resolution displays
    // as well as high-resolution displays.
    const double offsetWidth = 0.1;
    cx = cx + cxOffset * offsetWidth;
    cy = cy + cyOffset * offsetWidth;

    // Calculate starting point, ending point and control points of Bezier curve.
    double f = 4 * tan((angleEnd - angleStart) / 4) / 3;
    point_t p0 = {
        cx + (r * cos(angleStart)),
        cy + (r * sin(angleStart)),
    };
    point_t p1 = {
        cx + (r * cos(angleStart)) - f * (r * sin(angleStart)),
        cy + (r * sin(angleStart)) + f * (r * cos(angleStart)),
    };
    point_t p2 = {
        cx + (r * cos(angleEnd)) + f * (r * sin(angleEnd)),
        cy + (r * sin(angleEnd)) - f * (r * cos(angleEnd)),
    };
    point_t p3 = {
        cx + (r * cos(angleEnd)),
        cy + (r * sin(angleEnd)),
    };

    // Add patch with shape of the sector and gradient colors.
    cairo_mesh_pattern_begin_patch(gradient);
    cairo_mesh_pattern_move_to(gradient, cx, cy);
    cairo_mesh_pattern_line_to(gradient, p0.x, p0.y);
    cairo_mesh_pattern_curve_to(gradient, p1.x, p1.y, p2.x, p2.y, p3.x, p3.y);
    setCornerColorRGBA(gradient, 0, from, globalAlpha);
    setCornerColorRGBA(gradient, 1, from, globalAlpha);
    setCornerColorRGBA(gradient, 2, to, globalAlpha);
    setCornerColorRGBA(gradient, 3, to, globalAlpha);
    cairo_mesh_pattern_end_patch(gradient);
}

static RefPtr<cairo_pattern_t> createConic(float xo, float yo, float r, float angleRadians,
    Gradient::ColorStopVector stops, float globalAlpha)
{
    // It's not possible to paint an entire circle with a single Bezier curve.
    // To have a good approximation to a circle it's necessary to use at least four Bezier curves.
    // So add three additional interpolated stops, allowing for four Bezier curves.
    if (stops.size() == 2) {
        auto interpolatedStop = [&] (double fraction) -> Gradient::ColorStop {
            return { blend(stops.first().offset, stops.last().offset, fraction), blend(stops.first().color, stops.last().color, fraction) };
        };
        stops = { stops.first(), interpolatedStop(0.25), interpolatedStop(0.5), interpolatedStop(0.75), stops.last() };
    }

    if (stops.first().offset > 0.0f)
        stops.insert(0, { 0.0f, stops.first().color });
    if (stops.last().offset < 1.0f)
        stops.append({ 1.0f, stops.last().color });

    auto gradient = adoptRef(cairo_pattern_create_mesh());
    for (size_t i = 0; i < stops.size() - 1; i++)
        addConicSector(gradient.get(), xo, yo, r, angleRadians, stops[i], stops[i + 1], globalAlpha);
    return gradient;
}

RefPtr<cairo_pattern_t> Gradient::createPattern(float globalAlpha)
{
    auto gradient = WTF::switchOn(m_data,
        [&] (const LinearData& data) {
            auto gradient = adoptRef(cairo_pattern_create_linear(data.point0.x(), data.point0.y(), data.point1.x(), data.point1.y()));
            for (auto& stop : stops())
                addColorStopRGBA(gradient.get(), stop, globalAlpha);
            return gradient;
        },
        [&] (const RadialData& data) {
            auto gradient = adoptRef(cairo_pattern_create_radial(data.point0.x(), data.point0.y(), data.startRadius, data.point1.x(), data.point1.y(), data.endRadius));
            for (auto& stop : stops())
                addColorStopRGBA(gradient.get(), stop, globalAlpha);
            return gradient;
        },
        [&] (const ConicData& data) {
            // FIXME: data passed for a Conic gradient doesn't contain a radius. That's apparently correct because the W3C spec
            // (https://www.w3.org/TR/css-images-4/#conic-gradients) states a conic gradient is only defined by its position and angle.
            // Thus, here I give the radius an extremely large value. The resulting gradient will be later clipped by fillRect.
            // An alternative solution could be to change the API and pass a rect's width and height to optimize the computation of the radius.
            const float radius = 4096;
            return createConic(data.point0.x(), data.point0.y(), radius, data.angleRadians, stops(), globalAlpha);
        }
    );

    switch (m_spreadMethod) {
    case GradientSpreadMethod::Pad:
        cairo_pattern_set_extend(gradient.get(), CAIRO_EXTEND_PAD);
        break;
    case GradientSpreadMethod::Reflect:
        cairo_pattern_set_extend(gradient.get(), CAIRO_EXTEND_REFLECT);
        break;
    case GradientSpreadMethod::Repeat:
        cairo_pattern_set_extend(gradient.get(), CAIRO_EXTEND_REPEAT);
        break;
    }

    cairo_matrix_t matrix = toCairoMatrix(m_gradientSpaceTransformation);
    cairo_matrix_invert(&matrix);
    cairo_pattern_set_matrix(gradient.get(), &matrix);

    return gradient;
}

void Gradient::fill(GraphicsContext& context, const FloatRect& rect)
{
    auto pattern = createPattern(1.0);
    if (!pattern)
        return;

    ASSERT(context.hasPlatformContext());
    auto& platformContext = *context.platformContext();

    Cairo::save(platformContext);
    Cairo::fillRect(platformContext, rect, pattern.get());
    Cairo::restore(platformContext);
}

} // namespace WebCore

#endif // USE(CAIRO)
