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

#if !USE(DIRECT2D)

#include "FontCache.h"
#include "FontCascade.h"
#include "HWndDC.h"
#include <usp10.h>

namespace WebCore {

static bool shapeByUniscribe(const UChar* str, int len, SCRIPT_ITEM& item, const Font* fontData,
    Vector<WORD>& glyphs, Vector<WORD>& clusters,
    Vector<SCRIPT_VISATTR>& visualAttributes)
{
    HWndDC hdc;
    HGDIOBJ oldFont = nullptr;
    HRESULT shapeResult = E_PENDING;
    int glyphCount = 0;

    if (!fontData)
        return false;

    do {
        shapeResult = ScriptShape(hdc, fontData->scriptCache(), wcharFrom(str), len, glyphs.size(), &item.a,
            glyphs.data(), clusters.data(), visualAttributes.data(), &glyphCount);
        if (shapeResult == E_PENDING) {
            // The script cache isn't primed with enough info yet. We need to select our HFONT into
            // a DC and pass the DC in to ScriptShape.
            ASSERT(!hdc);
            hdc.setHWnd(nullptr);
            HFONT hfont = fontData->platformData().hfont();
            oldFont = SelectObject(hdc, hfont);
        } else if (shapeResult == E_OUTOFMEMORY) {
            // Need to resize our buffers.
            glyphs.resize(glyphs.size() * 2);
            visualAttributes.resize(glyphs.size());
        } else if (shapeResult == USP_E_SCRIPT_NOT_IN_FONT)
            item.a.eScript = SCRIPT_UNDEFINED;
        else
            break;
    } while (true);

    if (hdc)
        SelectObject(hdc, oldFont);

    if (FAILED(shapeResult))
        return false;

    glyphs.shrink(glyphCount);
    visualAttributes.shrink(glyphCount);

    return true;
}

template<typename T>
class BidiRange {
public:
    BidiRange(T* data, unsigned length, bool isLTR)
        : m_data(data)
        , m_length(length)
        , m_isLTR(isLTR)
    {
    }

    BidiRange(Vector<T>& data, bool isLTR)
        : m_data(data.data())
        , m_length(data.size())
        , m_isLTR(isLTR)
    {
    }

    class Iterator {
    public:
        Iterator(const BidiRange& range, unsigned logicalIndex)
            : m_range(range)
            , m_logicalIndex(logicalIndex)
        {
        }

        void operator++() { ++m_logicalIndex; }
        T& operator*() { return m_range.m_data[index()]; }
        bool operator==(const Iterator& other) { return m_logicalIndex == other.m_logicalIndex; }
        bool operator!=(const Iterator& other) { return m_logicalIndex != other.m_logicalIndex; }
        unsigned index()
        {
            ASSERT(m_logicalIndex < m_range.m_length);
            if (m_range.m_isLTR)
                return m_logicalIndex;
            return m_range.m_length - m_logicalIndex - 1;
        }

    private:
        friend BidiRange;

        const BidiRange& m_range;
        unsigned m_logicalIndex;
    };

    Iterator begin() const { return { *this, 0 }; }
    Iterator end() const { return { *this, m_length }; }

    Iterator fromIndex(unsigned index) const { return { *this, m_isLTR ? index : m_length - index - 1 }; }

private:
    T* m_data;
    unsigned m_length;
    bool m_isLTR;
};

static Vector<unsigned> stringIndicesFromClusters(const Vector<WORD>& clusters, StringView string, unsigned stringPosition, unsigned numberOfGlyphs, bool isLTR)
{
    Vector<unsigned> stringIndices(numberOfGlyphs);

    BidiRange<unsigned> stringIndicesRange(stringIndices, isLTR);
    auto glyphIndex = stringIndicesRange.begin();
    unsigned stringIndex = 0;
    int i = 0;
    for (;;) {
        auto startStringIndex = stringIndex;
        auto startGlyphIndex = clusters[stringIndex];
        while (stringIndex < clusters.size() && clusters[stringIndex] == startGlyphIndex)
            ++stringIndex;
        bool finish = stringIndex == clusters.size();
        auto endStringIndex = stringIndex;
        auto endGlyphIterator = !finish ? stringIndicesRange.fromIndex(clusters[endStringIndex]) : stringIndicesRange.end();

        // ComplexTextController::adjustGlyphsAndAdvances replaces glyphs pointing to space characters with space glyphs.
        // stringIndices should point to non-space characters if available.
        auto targetStringIndex = startStringIndex;
        for (auto s = startStringIndex; s < endStringIndex; s++) {
            auto character = string[s];
            auto isSpace = FontCascade::treatAsSpace(character) || FontCascade::treatAsZeroWidthSpace(character);
            if (!isSpace) {
                targetStringIndex = s;
                break;
            }
        }

        for (; glyphIndex != endGlyphIterator; ++glyphIndex)
            *glyphIndex = stringPosition + targetStringIndex;

        if (finish)
            break;
    }

    return stringIndices;
}

void ComplexTextController::collectComplexTextRunsForCharacters(const UChar* cp, unsigned stringLength, unsigned stringLocation, const Font* font)
{
    if (!font) {
        // Create a run of missing glyphs from the primary font.
        m_complexTextRuns.append(ComplexTextRun::create(m_font.primaryFont(), cp, stringLocation, stringLength, 0, stringLength, m_run.ltr()));
        return;
    }

    Vector<SCRIPT_ITEM> items(6);
    int numItems = 0;
    for (;;) {
        SCRIPT_CONTROL control { };
        SCRIPT_STATE state { };
        // Set up the correct direction for the run.
        state.uBidiLevel = m_run.rtl();
        // Lock the correct directional override.
        state.fOverrideDirection = m_run.directionalOverride();

        // ScriptItemize may write (cMaxItems + 1) SCRIPT_ITEM.
        HRESULT hr = ScriptItemize(wcharFrom(cp), stringLength, items.size() - 1, &control, &state, items.data(), &numItems);
        if (hr != E_OUTOFMEMORY) {
            ASSERT(SUCCEEDED(hr));
            break;
        }
        items.resize(items.size() * 2);
    }
    items.resize(numItems + 1);

    for (int i = 0; i < numItems; i++) {
        // Determine the string for this item.
        const UChar* str = cp + items[i].iCharPos;
        int length = items[i+1].iCharPos - items[i].iCharPos;
        SCRIPT_ITEM& item = items[i];

        // Set up buffers to hold the results of shaping the item.
        Vector<WORD> glyphs;
        Vector<WORD> clusters;
        Vector<SCRIPT_VISATTR> visualAttributes;
        clusters.resize(length);

        // Shape the item.
        // The recommended size for the glyph buffer is 1.5 * the character length + 16 in the uniscribe docs.
        // Apparently this is a good size to avoid having to make repeated calls to ScriptShape.
        glyphs.resize(1.5 * length + 16);
        visualAttributes.resize(glyphs.size());

        if (!shapeByUniscribe(str, length, item, font, glyphs, clusters, visualAttributes))
            continue;

        // We now have a collection of glyphs.
        Vector<GOFFSET> offsets;
        Vector<int> advances;
        offsets.resize(glyphs.size());
        advances.resize(glyphs.size());
        HRESULT placeResult = ScriptPlace(0, font->scriptCache(), glyphs.data(), glyphs.size(), visualAttributes.data(),
            &item.a, advances.data(), offsets.data(), 0);
        if (placeResult == E_PENDING) {
            // The script cache isn't primed with enough info yet. We need to select our HFONT into
            // a DC and pass the DC in to ScriptPlace.
            HWndDC hdc(0);
            HFONT hfont = font->platformData().hfont();
            HGDIOBJ oldFont = SelectObject(hdc, hfont);
            placeResult = ScriptPlace(hdc, font->scriptCache(), glyphs.data(), glyphs.size(), visualAttributes.data(),
                &item.a, advances.data(), offsets.data(), 0);
            SelectObject(hdc, oldFont);
        }

        if (FAILED(placeResult) || glyphs.isEmpty())
            continue;

        if (m_fallbackFonts)
            m_fallbackFonts->add(font);

        Vector<FloatSize> baseAdvances;
        Vector<FloatPoint> origins;
        baseAdvances.reserveCapacity(glyphs.size());
        origins.reserveCapacity(glyphs.size());

        for (unsigned k = 0; k < glyphs.size(); k++) {
            const float cLogicalScale = font->platformData().useGDI() ? 1 : 32;
            float advance = advances[k] / cLogicalScale;
            float offsetX = offsets[k].du / cLogicalScale;
            float offsetY = offsets[k].dv / cLogicalScale;

            // Match AppKit's rules for the integer vs. non-integer rendering modes.
            if (!font->platformData().isSystemFont()) {
                advance = roundf(advance);
                offsetX = roundf(offsetX);
                offsetY = roundf(offsetY);
            }

            baseAdvances.uncheckedAppend({ advance, 0 });
            origins.uncheckedAppend({ offsetX, offsetY });
        }
        bool ltr = !item.a.fRTL;
        auto stringIndices = stringIndicesFromClusters(clusters, StringView(str, length), item.iCharPos, glyphs.size(), ltr);
        FloatSize initialAdvance = toFloatSize(origins[0]);
        m_complexTextRuns.append(ComplexTextRun::create(baseAdvances, origins, glyphs, stringIndices, initialAdvance, *font, cp, stringLocation, stringLength, item.iCharPos, items[i+1].iCharPos, ltr));
    }
}

}

#endif // !USE(DIRECT2D)
