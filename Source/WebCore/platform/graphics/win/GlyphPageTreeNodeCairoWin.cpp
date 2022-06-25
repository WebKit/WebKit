/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
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
#include "HWndDC.h"
#include <wtf/text/win/WCharStringExtras.h>

namespace WebCore {

bool GlyphPage::fill(UChar* buffer, unsigned bufferLength)
{
    ASSERT(bufferLength == GlyphPage::size || bufferLength == 2 * GlyphPage::size);

    const Font& font = this->font();
    bool haveGlyphs = false;

    HWndDC dc(0);
    SaveDC(dc);
    SelectObject(dc, font.platformData().hfont());

    if (bufferLength == GlyphPage::size) {
        WORD localGlyphBuffer[GlyphPage::size * 2];
        DWORD result = GetGlyphIndices(dc, wcharFrom(buffer), bufferLength, localGlyphBuffer, GGI_MARK_NONEXISTING_GLYPHS);
        bool success = result != GDI_ERROR && static_cast<unsigned>(result) == bufferLength;

        if (success) {
            for (unsigned i = 0; i < GlyphPage::size; i++) {
                Glyph glyph = localGlyphBuffer[i];
                if (glyph == 0xffff)
                    setGlyphForIndex(i, 0);
                else {
                    setGlyphForIndex(i, glyph);
                    haveGlyphs = true;
                }
            }
        }
    } else {
        SCRIPT_CACHE sc = { };
        SCRIPT_FONTPROPERTIES fp = { };
        fp.cBytes = sizeof fp;
        ScriptGetFontProperties(dc, &sc, &fp);
        ScriptFreeCache(&sc);

        for (unsigned i = 0; i < GlyphPage::size; i++) {
            wchar_t glyphs[2] = { };
            GCP_RESULTS gcpResults = { };
            gcpResults.lStructSize = sizeof gcpResults;
            gcpResults.nGlyphs = 2;
            gcpResults.lpGlyphs = glyphs;
            GetCharacterPlacement(dc, wcharFrom(buffer) + i * 2, 2, 0, &gcpResults, GCP_GLYPHSHAPE);
            bool success = 1 == gcpResults.nGlyphs;
            if (success) {
                auto glyph = glyphs[0];
                if (glyph == fp.wgBlank || glyph == fp.wgInvalid || glyph == fp.wgDefault)
                    setGlyphForIndex(i, 0);
                else {
                    setGlyphForIndex(i, glyph);
                    haveGlyphs = true;
                }
            }
        }
    }
    RestoreDC(dc, -1);

    return haveGlyphs;
}

}
