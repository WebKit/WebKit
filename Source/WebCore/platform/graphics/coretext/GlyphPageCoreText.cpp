/*
 * Copyright (C) 2006-2020 Apple Inc. All rights reserved.
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

#include "Font.h"
#include "FontCascade.h"
#include <pal/spi/cg/CoreGraphicsSPI.h>

#if PLATFORM(COCOA)
#include <pal/spi/cocoa/CoreTextSPI.h>
#else
#include <pal/spi/win/CoreTextSPIWin.h>
#endif

namespace WebCore {

#if !PLATFORM(WIN)

static bool shouldFillWithVerticalGlyphs(const UChar* buffer, unsigned bufferLength, const Font& font)
{
    if (!font.hasVerticalGlyphs())
        return false;
    for (unsigned i = 0; i < bufferLength; ++i) {
        if (!FontCascade::isCJKIdeograph(buffer[i]))
            return true;
    }
    return false;
}

static const constexpr CGGlyph deletedGlyph = 0xFFFF;

bool GlyphPage::fill(UChar* buffer, unsigned bufferLength)
{
    ASSERT(bufferLength == GlyphPage::size || bufferLength == 2 * GlyphPage::size);

    const Font& font = this->font();
    Vector<CGGlyph, 512> glyphs(bufferLength);
    unsigned glyphStep = bufferLength / GlyphPage::size;

    if (shouldFillWithVerticalGlyphs(buffer, bufferLength, font))
        CTFontGetVerticalGlyphsForCharacters(font.platformData().ctFont(), reinterpret_cast<UniChar*>(buffer), glyphs.data(), bufferLength);
    else
        CTFontGetGlyphsForCharacters(font.platformData().ctFont(), reinterpret_cast<UniChar*>(buffer), glyphs.data(), bufferLength);

    bool haveGlyphs = false;
    for (unsigned i = 0; i < GlyphPage::size; ++i) {
        auto theGlyph = glyphs[i * glyphStep];
        if (theGlyph && theGlyph != deletedGlyph) {
            setGlyphForIndex(i, theGlyph);
            haveGlyphs = true;
        }
    }
    return haveGlyphs;
}

#endif

} // namespace WebCore
