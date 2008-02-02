/*
 * Copyright (C) 2007 Kevin Ollivier.  All rights reserved.
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

#include "FontFallbackList.h"
#include "GlyphBuffer.h"
#include "GraphicsContext.h"
#include "IntRect.h"
#include "NotImplemented.h"
#include "SimpleFontData.h"

#include "fontprops.h"
#include <wx/defs.h>
#include <wx/dcclient.h>

namespace WebCore {

void Font::drawGlyphs(GraphicsContext* graphicsContext, const SimpleFontData* font, const GlyphBuffer& glyphBuffer, 
                      int from, int numGlyphs, const FloatPoint& point) const
{
    // prepare DC
    Color color = graphicsContext->fillColor();

#if USE(WXGC)
    wxGCDC* dc = (wxGCDC*)graphicsContext->platformContext();
    wxFont wxfont = font->getWxFont();
    if (wxfont.IsOk())
        dc->SetFont(wxfont);
    dc->SetTextForeground(color);
#else
    wxDC* dc = graphicsContext->platformContext();
    dc->SetTextBackground(color);
    dc->SetTextForeground(color);
    dc->SetFont(font->getWxFont());
#endif

    // convert glyphs to wxString
    GlyphBufferGlyph* glyphs = const_cast<GlyphBufferGlyph*>(glyphBuffer.glyphs(from));
    int offset = point.x();
    wxString text = wxEmptyString;
    for (unsigned i = 0; i < numGlyphs; i++) {
        text = text.Append((wxChar)glyphs[i]);
        offset += glyphBuffer.advanceAt(from + i);
    }
    
    // the y point is actually the bottom point of the text, turn it into the top
    float height = font->ascent() - font->descent();
    wxCoord ypoint = (wxCoord) (point.y() - height);
     
    dc->DrawText(text, (wxCoord)point.x(), ypoint);
}

FloatRect Font::selectionRectForComplexText(const TextRun& run, const IntPoint& point, int h, int from, int to) const
{
    notImplemented();
    return FloatRect();
}

void Font::drawComplexText(GraphicsContext* graphicsContext, const TextRun& run, const FloatPoint& point, int from, int to) const
{
    notImplemented();
}

float Font::floatWidthForComplexText(const TextRun& run) const
{
    notImplemented();
    return 0;
}

int Font::offsetForPositionForComplexText(const TextRun& run, int x, bool includePartialGlyphs) const
{
    notImplemented();
    return 0;
}

}
