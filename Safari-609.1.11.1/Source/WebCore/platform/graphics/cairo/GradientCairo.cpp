/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc.  All rights reserved.
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
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

cairo_pattern_t* Gradient::createPlatformGradient(float globalAlpha)
{
    cairo_pattern_t* gradient = WTF::switchOn(m_data,
        [&] (const LinearData& data) -> cairo_pattern_t* {
            return cairo_pattern_create_linear(data.point0.x(), data.point0.y(), data.point1.x(), data.point1.y());
        },
        [&] (const RadialData& data) -> cairo_pattern_t* {
            return cairo_pattern_create_radial(data.point0.x(), data.point0.y(), data.startRadius, data.point1.x(), data.point1.y(), data.endRadius);
        },
        [&] (const ConicData&)  -> cairo_pattern_t* {
            // FIXME: implement conic gradient rendering.
            return nullptr;
        }
    );

    for (const auto& stop : m_stops) {
        if (stop.color.isExtended()) {
            cairo_pattern_add_color_stop_rgba(gradient, stop.offset, stop.color.asExtended().red(), stop.color.asExtended().green(), stop.color.asExtended().blue(),
                stop.color.asExtended().alpha() * globalAlpha);
        } else {
            float r, g, b, a;
            stop.color.getRGBA(r, g, b, a);
            cairo_pattern_add_color_stop_rgba(gradient, stop.offset, r, g, b, a * globalAlpha);
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
