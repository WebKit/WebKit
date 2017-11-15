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
    cairo_pattern_t* gradient;
    if (m_radial)
        gradient = cairo_pattern_create_radial(m_p0.x(), m_p0.y(), m_r0, m_p1.x(), m_p1.y(), m_r1);
    else
        gradient = cairo_pattern_create_linear(m_p0.x(), m_p0.y(), m_p1.x(), m_p1.y());

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

void Gradient::fill(GraphicsContext* context, const FloatRect& rect)
{
    RefPtr<cairo_pattern_t> platformGradient = adoptRef(createPlatformGradient(1.0));

    context->save();
    ASSERT(context->hasPlatformContext());
    Cairo::fillRect(*context->platformContext(), rect, platformGradient.get());
    context->restore();
}

} // namespace WebCore

#endif // USE(CAIRO)
