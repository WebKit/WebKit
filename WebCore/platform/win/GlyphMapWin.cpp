/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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
#include "GlyphMap.h"
#include "FontData.h"
#include <windows.h>

namespace WebCore
{

bool GlyphMap::fillPage(GlyphPage* page, UChar* buffer, unsigned bufferLength, const FontData* fontData)
{
    HDC dc = GetDC((HWND)0);
    SaveDC(dc);
    SelectObject(dc, fontData->m_font.hfont());

    TEXTMETRIC tm;
    GetTextMetrics(dc, &tm);
    if ((tm.tmPitchAndFamily & TMPF_VECTOR) == 0) {
        // The "glyph" for a bitmap font is just the character itself.
        // FIXME: Need to check character ranges and fill in a glyph of 0 for
        // any characters not in range.  Otherwise bitmap fonts will never fall
        // back.
        // IMLangFontLink2::GetFontUnicodeRanges does what we want. 
        for (unsigned i = 0; i < cGlyphPageSize; i++)
            page->setGlyphDataForIndex(i, buffer[i], fontData);
    } else {
        WORD localGlyphBuffer[cGlyphPageSize];
        GetGlyphIndices(dc, buffer, bufferLength, localGlyphBuffer, 0);

        for (unsigned i = 0; i < cGlyphPageSize; i++)
            page->setGlyphDataForIndex(i, localGlyphBuffer[i], fontData);
    }

    RestoreDC(dc, -1);
    ReleaseDC(0, dc);
    return true;
}

}