/*
 * Copyright (C) 2007 Ryan Leavengood <leavengood@gmail.com>
 *
 * All rights reserved.
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
#include "Font.h"

#include "FontData.h"
#include "FontDescription.h"
#include "FontSelector.h"
#include "GraphicsContext.h"
#include "NotImplemented.h"
#include <Font.h>
#include <String.h>
#include <View.h>


// FIXME: Temp routine to convert unicode character to UTF8.
int charUnicodeToUTF8HACK(unsigned short glyph, char* out)
{
    int i = 0;

    if (glyph < 0x0080)
        out[i++] = static_cast<char>(glyph);
    else if (glyph < 0x0800) {  // 2 bytes
        out[i++] = 0xc0 | (glyph >> 6);
        out[i++] = 0x80 | (glyph & 0x3F);
    } else if (glyph > 0x0800) {  // 3 bytes
        out[i++] = 0xe0 | (glyph >> 12);
        out[i++] = 0x80 | ((glyph >> 6) & 0x3F);
        out[i++] = 0x80 | (glyph & 0x3F);
    }

    out[i] = '\0';
    return i;
}

namespace WebCore {

bool Font::canReturnFallbackFontsForComplexText()
{
    return false;
}

bool Font::canExpandAroundIdeographsInComplexText()
{
    return false;
}

void Font::drawGlyphs(GraphicsContext* graphicsContext, const SimpleFontData* font,
                      const GlyphBuffer& glyphBuffer, int from, int numGlyphs, const FloatPoint& point) const
{
    Color color = graphicsContext->fillColor();
    BView* view = graphicsContext->platformContext();
    BFont* m_font = font->platformData().font();

    graphicsContext->setCompositeOperation(CompositeSourceOver);
    view->SetHighColor(color);
    view->SetFont(m_font);

    GlyphBufferGlyph* glyphs = const_cast<GlyphBufferGlyph*>(glyphBuffer.glyphs(from));
    float offset = point.x();
    for (int i = 0; i < numGlyphs; i++) {
        char out[4];
        charUnicodeToUTF8HACK(glyphs[i], out);

        view->DrawString(out, sizeof(out), BPoint(offset, point.y()));
        offset += glyphBuffer.advanceAt(from + i);
    }
}

void Font::drawComplexText(GraphicsContext* ctx, const TextRun& run, const FloatPoint& point,
                           int from, int to) const
{
    notImplemented();
}

void Font::drawEmphasisMarksForComplexText(GraphicsContext* /* context */, const TextRun& /* run */, const AtomicString& /* mark */, const FloatPoint& /* point */, int /* from */, int /* to */) const
{
    notImplemented();
}

float Font::floatWidthForComplexText(const TextRun& run, HashSet<const SimpleFontData*>* fallbackFonts, GlyphOverflow* glyphOverflow) const
{
    notImplemented();
    return 0;
}

FloatRect Font::selectionRectForComplexText(const TextRun&, const FloatPoint&, int, int, int) const
{
    notImplemented();
    return FloatRect();
}

int Font::offsetForPositionForComplexText(const TextRun&, float, bool) const
{
    notImplemented();
    return 0;
}

} // namespace WebCore

