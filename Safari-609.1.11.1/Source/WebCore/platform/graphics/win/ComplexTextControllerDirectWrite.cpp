/*
* Copyright (C) 2017-2019 Apple Inc. All rights reserved.
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
* THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
* EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
* DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
* LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
* ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
* SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "config.h"
#include "ComplexTextController.h"

#if USE(DIRECT2D)

#include "DirectWriteUtilities.h"
#include "FontCache.h"
#include "FontCascade.h"
#include "TextAnalyzerHelper.h"
#include <dwrite_3.h>

namespace WebCore {

static bool shape(IDWriteTextAnalyzer* analyzer, const DWRITE_SCRIPT_ANALYSIS& analysis, const FontPlatformData& fontPlatformData, const WCHAR* text, unsigned length, unsigned suggestedCount, Vector<Glyph>& glyphs, Vector<Glyph>& clusterMap, Vector<DWRITE_SHAPING_TEXT_PROPERTIES>& textProperties, Vector<DWRITE_SHAPING_GLYPH_PROPERTIES>& glyphProperties, unsigned& glyphCount)
{
    HRESULT shapeResult = E_PENDING;

    do {
        shapeResult = analyzer->GetGlyphs(text, length, fontPlatformData.dwFontFace(), fontPlatformData.orientation() == FontOrientation::Vertical, false,
            &analysis, nullptr, nullptr, nullptr, nullptr, 0, suggestedCount, clusterMap.data(), textProperties.data(),
            glyphs.data(), glyphProperties.data(), &glyphCount);

        if (shapeResult == E_OUTOFMEMORY) {
            // Need to resize our buffers (except for clusterMap. It is always the length of the string).
            glyphs.resize(glyphs.size() * 2);
            textProperties.resize(glyphs.size());
            glyphProperties.resize(glyphs.size());
        }
    } while (shapeResult == E_OUTOFMEMORY);

    if (FAILED(shapeResult))
        return false;

    return true;
}

void ComplexTextController::collectComplexTextRunsForCharacters(const UChar* cp, unsigned length, unsigned stringLocation, const Font* font)
{
    if (!font) {
        // Create a run of missing glyphs from the primary font.
        m_complexTextRuns.append(ComplexTextRun::create(m_font.primaryFont(), cp, stringLocation, length, 0, length, m_run.ltr()));
        return;
    }

    auto& fontPlatformData = font->platformData();

    wchar_t localeName[LOCALE_NAME_MAX_LENGTH] = { };
    int localeLength = GetUserDefaultLocaleName(reinterpret_cast<LPWSTR>(&localeName), LOCALE_NAME_MAX_LENGTH);
    RELEASE_ASSERT(localeLength < LOCALE_NAME_MAX_LENGTH);

    Vector<DWRITE_FONT_FEATURE> fontFeatures(1);
    fontFeatures[0].nameTag = DWRITE_FONT_FEATURE_TAG_KERNING;
    fontFeatures[0].parameter = m_font.enableKerning() ? 1 : 0;

    if (fontPlatformData.orientation() == FontOrientation::Vertical)
        fontFeatures.append({DWRITE_FONT_FEATURE_TAG_VERTICAL_WRITING, 1});

    DWRITE_TYPOGRAPHIC_FEATURES usedFeatures;
    usedFeatures.features = fontFeatures.data();
    usedFeatures.featureCount = fontFeatures.size();

    Vector<const DWRITE_TYPOGRAPHIC_FEATURES*> features;
    features.append(&usedFeatures);

    COMPtr<IDWriteTextAnalyzer> analyzer;
    HRESULT hr = DirectWrite::factory()->CreateTextAnalyzer(&analyzer);
    RELEASE_ASSERT(SUCCEEDED(hr));

    TextAnalyzerHelper helper(reinterpret_cast<LPWSTR>(&localeName), reinterpret_cast<LPCWSTR>(cp), length);

    hr = analyzer->AnalyzeScript(&helper, 0, length, &helper);
    RELEASE_ASSERT(SUCCEEDED(hr));

    unsigned totalSuggestedCount = (3 * length / 2 + 16);

    Vector<Glyph> glyphs;
    Vector<Glyph> clusterMap;
    Vector<DWRITE_SHAPING_TEXT_PROPERTIES> textProperties;
    Vector<DWRITE_SHAPING_GLYPH_PROPERTIES> glyphProperties;
    Vector<FLOAT> glyphAdvances;
    Vector<DWRITE_GLYPH_OFFSET> glyphOffsets;
    Vector<FloatPoint> origins;
    Vector<FloatSize> advances;
    Vector<unsigned> stringIndices;

    glyphs.reserveCapacity(totalSuggestedCount);
    clusterMap.reserveCapacity(length);
    textProperties.reserveCapacity(totalSuggestedCount);
    glyphProperties.reserveCapacity(totalSuggestedCount);
    glyphAdvances.reserveCapacity(totalSuggestedCount);
    glyphOffsets.reserveCapacity(totalSuggestedCount);
    origins.reserveCapacity(totalSuggestedCount);
    advances.reserveCapacity(totalSuggestedCount);
    stringIndices.reserveCapacity(totalSuggestedCount);

    for (const auto& run : helper.m_analyzedRuns) {
        const UChar* currentCP = cp + run.startPosition;
        LPCWSTR textPosition = reinterpret_cast<LPCWSTR>(currentCP);

        unsigned suggestedCount = (3 * run.length / 2 + 16);
        glyphs.resize(suggestedCount);
        clusterMap.resize(run.length);
        textProperties.resize(suggestedCount);
        glyphProperties.resize(suggestedCount);

        unsigned glyphCount = 0;
        if (!shape(analyzer.get(), run.analysis, fontPlatformData, textPosition, run.length, suggestedCount, glyphs, clusterMap, textProperties, glyphProperties, glyphCount))
            return;

        glyphs.shrink(glyphCount);
        textProperties.shrink(glyphCount);
        glyphProperties.shrink(glyphCount);
        glyphAdvances.resize(glyphCount);
        glyphOffsets.resize(glyphCount);

        HRESULT placementResult = analyzer->GetGlyphPlacements(textPosition, clusterMap.data(), textProperties.data(), run.length,
            glyphs.data(), glyphProperties.data(), glyphCount, fontPlatformData.dwFontFace(),
            fontPlatformData.size(), fontPlatformData.orientation() == FontOrientation::Vertical, m_run.ltr(),
            &run.analysis, reinterpret_cast<LPCWSTR>(localeName), 
            features.data(), &run.length, 1,
            glyphAdvances.data(), glyphOffsets.data());
        if (FAILED(placementResult))
            return;

        // Convert all chars that should be treated as spaces to use the space glyph.
        // We also create a map that allows us to quickly go from space glyphs back to their corresponding characters.
        float spaceWidth = font->spaceWidth() - font->syntheticBoldOffset();

        for (int k = 0; k < run.length; ++k) {
            UChar ch = *(currentCP + k);
            bool treatAsSpace = FontCascade::treatAsSpace(ch);
            bool treatAsZeroWidthSpace = FontCascade::treatAsZeroWidthSpace(ch);
            if (treatAsSpace || treatAsZeroWidthSpace) {
                // Substitute in the space glyph at the appropriate place in the glyphs
                // array.
                glyphs[clusterMap[k]] = font->spaceGlyph();
                glyphAdvances[clusterMap[k]] = treatAsSpace ? spaceWidth : 0;
            }
        }

        origins.resize(glyphCount);
        advances.resize(glyphCount);
        stringIndices.resize(glyphCount);

        for (size_t i = 0; i < glyphCount; ++i) {
            stringIndices[i] = i;
            origins[i] = FloatPoint(glyphOffsets[i].advanceOffset, glyphOffsets[i].ascenderOffset);
            advances[i] = FloatSize(glyphAdvances[i], 0);
        }

        m_complexTextRuns.append(ComplexTextRun::create(advances, origins, glyphs, stringIndices, FloatSize(), *font, currentCP, 0, run.length, 0, run.length, m_run.ltr()));
    }
}

}

#endif
