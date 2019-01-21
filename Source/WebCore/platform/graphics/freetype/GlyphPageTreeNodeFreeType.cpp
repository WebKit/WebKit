/*
 * Copyright (C) 2006, 2007 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Michael Emmel mike.emmel@gmail.com
 * Copyright (C) 2007 Alp Toker <alp.toker@collabora.co.uk>
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
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
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
#include "GlyphPage.h"

#include "CairoUtilities.h"
#include "CharacterProperties.h"
#include "Font.h"
#include "FontCascade.h"
#include "UTF16UChar32Iterator.h"
#include <cairo-ft.h>
#include <cairo.h>
#include <fontconfig/fcfreetype.h>
#include <wtf/Optional.h>

namespace WebCore {

bool GlyphPage::fill(UChar* buffer, unsigned bufferLength)
{
    const Font& font = this->font();
    cairo_scaled_font_t* scaledFont = font.platformData().scaledFont();
    ASSERT(scaledFont);

    CairoFtFaceLocker cairoFtFaceLocker(scaledFont);
    FT_Face face = cairoFtFaceLocker.ftFace();
    if (!face)
        return false;

    WTF::Optional<Glyph> zeroWidthSpaceGlyphValue;
    auto zeroWidthSpaceGlyph =
        [&] {
            if (!zeroWidthSpaceGlyphValue)
                zeroWidthSpaceGlyphValue = FcFreeTypeCharIndex(face, zeroWidthSpace);
            return *zeroWidthSpaceGlyphValue;
        };

    bool haveGlyphs = false;
    UTF16UChar32Iterator iterator(buffer, bufferLength);
    for (unsigned i = 0; i < GlyphPage::size; i++) {
        UChar32 character = iterator.next();
        if (character == iterator.end())
            break;

        Glyph glyph = FcFreeTypeCharIndex(face, FontCascade::treatAsSpace(character) ? space : character);
        // If the font doesn't support a Default_Ignorable character, replace it with zero with space.
        if (!glyph && isDefaultIgnorableCodePoint(character))
            glyph = zeroWidthSpaceGlyph();

        if (!glyph)
            setGlyphForIndex(i, 0);
        else {
            setGlyphForIndex(i, glyph);
            haveGlyphs = true;
        }
    }

    return haveGlyphs;
}

}
