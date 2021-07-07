/*
 * Copyright (C) 2006 Apple Inc.  All rights reserved.
 * Copyright (C) 2006 Michael Emmel mike.emmel@gmail.com
 * Copyright (C) 2007, 2008 Alp Toker <alp@atoker.com>
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) 2010 Holger Hans Peter Freyther
 * Copyright (C) 2014 Igalia S.L.
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
#include "FontCascade.h"

#if USE(CAIRO)

#include "AffineTransform.h"
#include "CairoOperations.h"
#include "CairoUtilities.h"
#include "Font.h"
#include "GlyphBuffer.h"
#include "Gradient.h"
#include "GraphicsContext.h"
#include "ImageBuffer.h"
#include "Pattern.h"
#include "RefPtrCairo.h"
#include "ShadowBlur.h"

namespace WebCore {

void FontCascade::drawGlyphs(GraphicsContext& context, const Font& font, const GlyphBufferGlyph* glyphs,
    const GlyphBufferAdvance* advances, unsigned numGlyphs, const FloatPoint& point,
    FontSmoothingMode fontSmoothingMode)
{
    if (!font.platformData().size())
        return;

    auto xOffset = point.x();
    Vector<cairo_glyph_t> cairoGlyphs(numGlyphs);
    {
        auto yOffset = point.y();
        for (size_t i = 0; i < numGlyphs; ++i) {
            cairoGlyphs[i] = { glyphs[i], xOffset, yOffset };
            xOffset += advances[i].width();
            yOffset += advances[i].height();
        }
    }

    cairo_scaled_font_t* scaledFont = font.platformData().scaledFont();
    double syntheticBoldOffset = font.syntheticBoldOffset();

    ASSERT(context.hasPlatformContext());
    auto& state = context.state();
    Cairo::drawGlyphs(*context.platformContext(), Cairo::FillSource(state), Cairo::StrokeSource(state),
        Cairo::ShadowState(state), point, scaledFont, syntheticBoldOffset, cairoGlyphs, xOffset,
        state.textDrawingMode, state.strokeThickness, state.shadowOffset, state.shadowColor,
        fontSmoothingMode);
}

Path Font::platformPathForGlyph(Glyph glyph) const
{
    RefPtr<cairo_t> cr = adoptRef(cairo_create(adoptRef(cairo_image_surface_create(CAIRO_FORMAT_A8, 1, 1)).get()));

    cairo_glyph_t cairoGlyph = { glyph, 0, 0 };
    cairo_set_scaled_font(cr.get(), platformData().scaledFont());
    cairo_glyph_path(cr.get(), &cairoGlyph, 1);

    float syntheticBoldOffset = this->syntheticBoldOffset();
    if (syntheticBoldOffset) {
        cairo_translate(cr.get(), syntheticBoldOffset, 0);
        cairo_glyph_path(cr.get(), &cairoGlyph, 1);
    }
    return Path(WTFMove(cr));
}

FloatRect Font::platformBoundsForGlyph(Glyph glyph) const
{
    if (!m_platformData.size())
        return FloatRect();

    cairo_glyph_t cglyph = { glyph, 0, 0 };
    cairo_text_extents_t extents;
    cairo_scaled_font_glyph_extents(m_platformData.scaledFont(), &cglyph, 1, &extents);

    if (cairo_scaled_font_status(m_platformData.scaledFont()) == CAIRO_STATUS_SUCCESS)
        return FloatRect(extents.x_bearing, extents.y_bearing, extents.width, extents.height);

    return FloatRect();
}

float Font::platformWidthForGlyph(Glyph glyph) const
{
    if (!m_platformData.size())
        return 0;

    if (cairo_scaled_font_status(m_platformData.scaledFont()) != CAIRO_STATUS_SUCCESS)
        return m_spaceWidth;

    cairo_glyph_t cairoGlyph = { glyph, 0, 0 };
    cairo_text_extents_t extents;
    cairo_scaled_font_glyph_extents(m_platformData.scaledFont(), &cairoGlyph, 1, &extents);
    float width = platformData().orientation() == FontOrientation::Horizontal ? extents.x_advance : -extents.y_advance;
    return width ? width : m_spaceWidth;
}

} // namespace WebCore

#endif // USE(CAIRO)
