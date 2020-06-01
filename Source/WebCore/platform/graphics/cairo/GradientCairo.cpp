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

#include "CairoOperations.h"
#include "CairoUtilities.h"
#include "GraphicsContext.h"
#include "PlatformContextCairo.h"

namespace WebCore {

void Gradient::platformDestroy()
{
}

static void addColorStopRGBA(cairo_pattern_t *gradient, Gradient::ColorStop stop, float globalAlpha)
{
    auto [r, g, b, a] = stop.color.toSRGBAComponentsLossy();
    cairo_pattern_add_color_stop_rgba(gradient, stop.offset, r, g, b, a * globalAlpha);
}

#if PLATFORM(GTK) || PLATFORM(WPE)

typedef struct point_t {
    double x, y;
} point_t;

static void setCornerColorRGBA(cairo_pattern_t* gradient, int id, Gradient::ColorStop stop, float globalAlpha)
{
    auto [r, g, b, a] = stop.color.toSRGBAComponentsLossy();
    cairo_mesh_pattern_set_corner_color_rgba(gradient, id, r, g, b, a * globalAlpha);
}

static void addConicSector(cairo_pattern_t *gradient, float cx, float cy, float r, float angleRadians,
    Gradient::ColorStop from, Gradient::ColorStop to, float globalAlpha)
{
    const double angOffset = 0.25; // 90 degrees.

    // Substract 90 degrees so angles start from top left.
    // Convert to radians and add angleRadians offset.
    double angleStart = ((from.offset - angOffset) * 2 * M_PI) + angleRadians;
    double angleEnd = ((to.offset - angOffset) * 2 * M_PI) + angleRadians;

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
        .x = cx + (r * cos(angleStart)),
        .y = cy + (r * sin(angleStart)),
    };
    point_t p1 = {
        .x = cx + (r * cos(angleStart)) - f * (r * sin(angleStart)),
        .y = cy + (r * sin(angleStart)) + f * (r * cos(angleStart)),
    };
    point_t p2 = {
        .x = cx + (r * cos(angleEnd)) + f * (r * sin(angleEnd)),
        .y = cy + (r * sin(angleEnd)) - f * (r * cos(angleEnd)),
    };
    point_t p3 = {
        .x = cx + (r * cos(angleEnd)),
        .y = cy + (r * sin(angleEnd)),
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

static Gradient::ColorStop interpolateColorStop(Gradient::ColorStop from, Gradient::ColorStop to)
{
    auto [r1, g1, b1, a1] = from.color.toSRGBAComponentsLossy();
    auto [r2, g2, b2, a2] = to.color.toSRGBAComponentsLossy();

    float offset = from.offset + (to.offset - from.offset) * 0.5f;
    float r = r1 + (r2 - r1) * 0.5f;
    float g = g1 + (g2 - g1) * 0.5f;
    float b = b1 + (b2 - b1) * 0.5f;
    float a = a1 + (a2 - a1) * 0.5f;

    return Gradient::ColorStop(offset, makeSimpleColorFromFloats(r, g, b, a));
}

static cairo_pattern_t* createConic(float xo, float yo, float r, float angleRadians,
    Gradient::ColorStopVector stops, float globalAlpha)
{
    cairo_pattern_t* gradient = cairo_pattern_create_mesh();
    Gradient::ColorStop from, to;

    /* It's not possible to paint an entire circle with a single Bezier curve.
     * To have a good approximation to a circle it's necessary to use at least
     * four Bezier curves. So three additional stops with interpolated colors
     * are added to force painting of four Bezier curves. */
    if (stops.size() == 2) {
        Gradient::ColorStop first = stops.at(0);
        Gradient::ColorStop last = stops.at(1);
        Gradient::ColorStop third = interpolateColorStop(first, last);
        Gradient::ColorStop second = interpolateColorStop(first, third);
        Gradient::ColorStop fourth = interpolateColorStop(third, last);
        stops.insert(1, fourth);
        stops.insert(1, third);
        stops.insert(1, second);
    }

    // Add extra color stop at the beginning if first element offset is not zero.
    if (stops.at(0).offset > 0)
        stops.insert(0, Gradient::ColorStop(0, stops.at(0).color));
    // Add extra color stop at the end if last element offset is not zero.
    if (stops.at(stops.size() - 1).offset < 1)
        stops.append(Gradient::ColorStop(1, stops.at(stops.size() - 1).color));

    for (size_t i = 0; i < stops.size() - 1; i++) {
        from = stops.at(i), to = stops.at(i + 1);
        addConicSector(gradient, xo, yo, r, angleRadians, from, to, globalAlpha);
    }

    return gradient;
}

#endif

cairo_pattern_t* Gradient::createPlatformGradient(float globalAlpha)
{
    cairo_pattern_t* gradient = WTF::switchOn(m_data,
        [&] (const LinearData& data) -> cairo_pattern_t* {
            return cairo_pattern_create_linear(data.point0.x(), data.point0.y(), data.point1.x(), data.point1.y());
        },
        [&] (const RadialData& data) -> cairo_pattern_t* {
            return cairo_pattern_create_radial(data.point0.x(), data.point0.y(), data.startRadius, data.point1.x(), data.point1.y(), data.endRadius);
        },
#if PLATFORM(GTK) || PLATFORM(WPE)
        [&] (const ConicData& data)  -> cairo_pattern_t* {
            // FIXME: data passed for a Conic gradient doesn't contain a radius. That's apparently correct because the W3C spec
            // (https://www.w3.org/TR/css-images-4/#conic-gradients) states a conic gradient is only defined by its position and angle.
            // Thus, here I give the radius an extremely large value. The resulting gradient will be later clipped by fillRect.
            // An alternative solution could be to change the API and pass a rect's width and height to optimize the computation of the radius.
            const float radius = 4096;
            return createConic(data.point0.x(), data.point0.y(), radius, data.angleRadians, stops(), globalAlpha);
#else
        [&] (const ConicData&)  -> cairo_pattern_t* {
            // FIXME: implement conic gradient rendering.
            return nullptr;
#endif
        }
    );

    if (type() != Type::Conic) {
        for (const auto& stop : stops()) {
            addColorStopRGBA(gradient, stop, globalAlpha);
        }
    }

    switch (m_spreadMethod) {
    case SpreadMethodPad:
        cairo_pattern_set_extend(gradient, CAIRO_EXTEND_PAD);
        break;
    case SpreadMethodReflect:
        cairo_pattern_set_extend(gradient, CAIRO_EXTEND_REFLECT);
        break;
    case SpreadMethodRepeat:
        cairo_pattern_set_extend(gradient, CAIRO_EXTEND_REPEAT);
        break;
    }

    cairo_matrix_t matrix = toCairoMatrix(m_gradientSpaceTransformation);
    cairo_matrix_invert(&matrix);
    cairo_pattern_set_matrix(gradient, &matrix);

    return gradient;
}

void Gradient::fill(GraphicsContext& context, const FloatRect& rect)
{
    RefPtr<cairo_pattern_t> platformGradient = adoptRef(createPlatformGradient(1.0));
    if (!platformGradient)
        return;

    ASSERT(context.hasPlatformContext());
    auto& platformContext = *context.platformContext();

    Cairo::save(platformContext);
    Cairo::fillRect(platformContext, rect, platformGradient.get());
    Cairo::restore(platformContext);
}

} // namespace WebCore

#endif // USE(CAIRO)
