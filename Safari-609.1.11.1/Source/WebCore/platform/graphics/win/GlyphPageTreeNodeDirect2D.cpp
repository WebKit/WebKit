/*
 * Copyright (C) 2016 Apple Inc.  All rights reserved.
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
#include "GlyphPage.h"

#if USE(DIRECT2D)

#include "DirectWriteUtilities.h"
#include "Font.h"
#include "TextAnalyzerHelper.h"
#include <dwrite_3.h>

namespace WebCore {

bool GlyphPage::fill(UChar* buffer, unsigned bufferLength)
{
    if (bufferLength > GlyphPage::size)
        return false;

    const Font& font = this->font();
    bool haveGlyphs = false;

    COMPtr<IDWriteTextAnalyzer> analyzer;
    HRESULT hr = DirectWrite::factory()->CreateTextAnalyzer(&analyzer);
    RELEASE_ASSERT(SUCCEEDED(hr));

    auto& fontPlatformData = font.platformData();

    UChar localeName[LOCALE_NAME_MAX_LENGTH + 1] = { };
    int localeLength = GetUserDefaultLocaleName(reinterpret_cast<LPWSTR>(&localeName), LOCALE_NAME_MAX_LENGTH);
    RELEASE_ASSERT(localeLength <= LOCALE_NAME_MAX_LENGTH);

    TextAnalyzerHelper helper(reinterpret_cast<LPWSTR>(&localeName), reinterpret_cast<LPWSTR>(buffer), bufferLength);

    hr = analyzer->AnalyzeScript(&helper, 0, bufferLength, &helper);
    RELEASE_ASSERT(SUCCEEDED(hr));

    Vector<Glyph> glyphs(GlyphPage::size, 0);
    Vector<Glyph> clusterMap(GlyphPage::size, 0);
    Vector<DWRITE_SHAPING_TEXT_PROPERTIES> textProperties(GlyphPage::size);
    Vector<DWRITE_SHAPING_GLYPH_PROPERTIES> glyphProperties(GlyphPage::size);

    const WCHAR* textStart = reinterpret_cast<LPCWSTR>(buffer);
    Glyph* glyphData = glyphs.data();
    Glyph* clusterMapData = clusterMap.data();
    DWRITE_SHAPING_TEXT_PROPERTIES* textPropertiesData = textProperties.data();
    DWRITE_SHAPING_GLYPH_PROPERTIES* glyphPropertiesData = glyphProperties.data();

    unsigned total = 0;
    unsigned maxGlyphCount = GlyphPage::size;
    for (const auto& run : helper.m_analyzedRuns) {
        RELEASE_ASSERT(total + run.length <= bufferLength);

        unsigned returnedCount = 0;
        hr = analyzer->GetGlyphs(textStart + run.startPosition, run.length, fontPlatformData.dwFontFace(), fontPlatformData.orientation() == FontOrientation::Vertical, false,
            &run.analysis, nullptr, nullptr, nullptr, nullptr, 0, maxGlyphCount, clusterMapData, textPropertiesData,
            glyphData, glyphPropertiesData, &returnedCount);

        if (!SUCCEEDED(hr))
            return false;

        glyphData += returnedCount;
        clusterMapData += returnedCount;
        textPropertiesData += returnedCount;
        glyphPropertiesData += returnedCount;
        maxGlyphCount -= returnedCount;
    }

    for (unsigned i = 0; i < GlyphPage::size; ++i) {
        Glyph glyph = glyphs[i];
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

#endif
