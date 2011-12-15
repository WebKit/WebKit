/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2006 Michael Emmel mike.emmel@gmail.com
 * Copyright (C) 2007, 2008 Alp Toker <alp@atoker.com>
 * Copyright (C) 2007 Holger Hans Peter Freyther
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "SimpleFontData.h"

#include "FloatConversion.h"
#include "FloatRect.h"
#include "Font.h"
#include "FontCache.h"
#include "FontDescription.h"
#include "GlyphBuffer.h"
#include <cairo-ft.h>
#include <cairo.h>
#include <fontconfig/fcfreetype.h>
#include <wtf/MathExtras.h>

namespace WebCore {

void SimpleFontData::platformInit()
{
    if (!m_platformData.m_size)
        return;

    ASSERT(m_platformData.scaledFont());
    cairo_font_extents_t font_extents;
    cairo_text_extents_t text_extents;
    cairo_scaled_font_extents(m_platformData.scaledFont(), &font_extents);

    float ascent = narrowPrecisionToFloat(font_extents.ascent);
    float descent = narrowPrecisionToFloat(font_extents.descent);
    float lineGap = narrowPrecisionToFloat(font_extents.height - font_extents.ascent - font_extents.descent);

    m_fontMetrics.setAscent(ascent);
    m_fontMetrics.setDescent(descent);

    // Match CoreGraphics metrics.
    m_fontMetrics.setLineSpacing(lroundf(ascent) + lroundf(descent) + lroundf(lineGap));
    m_fontMetrics.setLineGap(lineGap);

    cairo_scaled_font_text_extents(m_platformData.scaledFont(), "x", &text_extents);
    m_fontMetrics.setXHeight(narrowPrecisionToFloat(text_extents.height));

    cairo_scaled_font_text_extents(m_platformData.scaledFont(), " ", &text_extents);
    m_spaceWidth = narrowPrecisionToFloat(text_extents.x_advance);
    
    m_syntheticBoldOffset = m_platformData.syntheticBold() ? 1.0f : 0.f;
}

void SimpleFontData::platformCharWidthInit()
{
    m_avgCharWidth = 0.f;
    m_maxCharWidth = 0.f;
    initCharWidths();
}

void SimpleFontData::platformDestroy()
{
}

PassOwnPtr<SimpleFontData> SimpleFontData::createScaledFontData(const FontDescription& fontDescription, float scaleFactor) const
{
    ASSERT(m_platformData.scaledFont());
    return adoptPtr(new SimpleFontData(FontPlatformData(cairo_scaled_font_get_font_face(m_platformData.scaledFont()),
                                                        scaleFactor * fontDescription.computedSize(),
                                                        m_platformData.syntheticBold(),
                                                        m_platformData.syntheticOblique()),
                                       isCustomFont(), false));
}

SimpleFontData* SimpleFontData::smallCapsFontData(const FontDescription& fontDescription) const
{
    if (!m_derivedFontData)
        m_derivedFontData = DerivedFontData::create(isCustomFont());
    // FIXME: I think we want to ask FontConfig for the right font again.
    if (!m_derivedFontData->smallCaps)
        m_derivedFontData->smallCaps = createScaledFontData(fontDescription, .7);

    return m_derivedFontData->smallCaps.get();
}

SimpleFontData* SimpleFontData::emphasisMarkFontData(const FontDescription& fontDescription) const
{
    if (!m_derivedFontData)
        m_derivedFontData = DerivedFontData::create(isCustomFont());
    if (!m_derivedFontData->emphasisMark)
        m_derivedFontData->emphasisMark = createScaledFontData(fontDescription, .5);

    return m_derivedFontData->emphasisMark.get();
}

bool SimpleFontData::containsCharacters(const UChar* characters, int length) const
{
    ASSERT(m_platformData.scaledFont());
    FT_Face face = cairo_ft_scaled_font_lock_face(m_platformData.scaledFont());
    if (!face)
        return false;

    for (int i = 0; i < length; i++) {
        if (FcFreeTypeCharIndex(face, characters[i]) == 0) {
            cairo_ft_scaled_font_unlock_face(m_platformData.scaledFont());
            return false;
        }
    }

    cairo_ft_scaled_font_unlock_face(m_platformData.scaledFont());
    return true;
}

void SimpleFontData::determinePitch()
{
    m_treatAsFixedPitch = m_platformData.isFixedPitch();
}

FloatRect SimpleFontData::platformBoundsForGlyph(Glyph) const
{
    return FloatRect();
}

float SimpleFontData::platformWidthForGlyph(Glyph glyph) const
{
    if (!m_platformData.size())
        return 0;

    cairo_glyph_t cglyph = { glyph, 0, 0 };
    cairo_text_extents_t extents;
    cairo_scaled_font_glyph_extents(m_platformData.scaledFont(), &cglyph, 1, &extents);

    float w = (float)m_spaceWidth;
    if (cairo_scaled_font_status(m_platformData.scaledFont()) == CAIRO_STATUS_SUCCESS && extents.x_advance)
        w = (float)extents.x_advance;

    return w;    
}

void SimpleFontData::updateGlyphWithVariationSelector(UChar32 character, UChar32 selector, Glyph& glyph) const
{
    // FIXME: Implement.
}

}
