/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2006 Michael Emmel mike.emmel@gmail.com 
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
#include "FontData.h"

#include "FloatRect.h"
#include "Font.h"
#include "FontCache.h"
#include "FontDescription.h"
#include "GlyphBuffer.h"
#include <cairo.h>
#include <unicode/uchar.h>
#include <unicode/unorm.h>
#include <wtf/MathExtras.h>

namespace WebCore {

void FontData::platformInit()
{
    cairo_font_extents_t font_extents;
    cairo_text_extents_t text_extents;
    cairo_scaled_font_extents(m_font.m_scaledFont, &font_extents);
    m_ascent = static_cast<int>(font_extents.ascent);
    m_descent = static_cast<int>(font_extents.descent);
    m_lineSpacing = static_cast<int>(font_extents.height);
    cairo_scaled_font_text_extents(m_font.m_scaledFont, "x", &text_extents);
    m_xHeight = text_extents.height;
    cairo_scaled_font_text_extents(m_font.m_scaledFont, " ", &text_extents);
    m_spaceWidth =  static_cast<int>(text_extents.x_advance);
    assert(m_spaceWidth != 0);
    m_lineGap = m_lineSpacing - m_ascent - m_descent;
}

void FontData::platformDestroy()
{
    delete m_smallCapsFontData;
}

FontData* FontData::smallCapsFontData(const FontDescription& fontDescription) const
{
    if (!m_smallCapsFontData) {
        FontDescription desc = FontDescription(fontDescription);
        desc.setSpecifiedSize(0.70f*fontDescription.computedSize());
        const FontPlatformData* pdata = new FontPlatformData(desc, desc.family().family());
        m_smallCapsFontData = new FontData(*pdata);
    }
    return m_smallCapsFontData;
}

bool FontData::containsCharacters(const UChar* characters, int length) const
{
    for (unsigned i = 0; i < length; i++)
        if (getGlyphIndex(characters[i]) == 0)
            return false;
    return true;
}

void FontData::determinePitch()
{
    m_treatAsFixedPitch = m_font.isFixedPitch();
}

float FontData::platformWidthForGlyph(Glyph glyph) const
{
    cairo_glyph_t cglyph;
    cglyph.index = (int)glyph;
    cairo_text_extents_t extents;
    cairo_scaled_font_glyph_extents(m_font.m_scaledFont, &cglyph, 1, &extents);
    float w = extents.x_advance;
    if (w == 0)
        w = m_spaceWidth;
    return w;
}

void FontData::setFont(cairo_t* cr) const
{
    m_font.setFont(cr);
}

}
