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
#include "Font.h"

#if USE(DIRECT2D)

#include "FloatRect.h"
#include "FontCache.h"
#include "FontDescription.h"
#include "GlyphPage.h"
#include "GraphicsContext.h"
#include "HWndDC.h"
#include "NotImplemented.h"
#include <comutil.h>
#include <dwrite.h>
#include <mlang.h>
#include <pal/spi/win/CoreTextSPIWin.h>
#include <unicode/uchar.h>
#include <unicode/unorm.h>
#include <winsock2.h>
#include <wtf/MathExtras.h>
#include <wtf/RetainPtr.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

IDWriteFactory* Font::systemDWriteFactory()
{
    static IDWriteFactory* directWriteFactory = nullptr;
    if (!directWriteFactory) {
        HRESULT hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(directWriteFactory), reinterpret_cast<IUnknown**>(&directWriteFactory));
        RELEASE_ASSERT(SUCCEEDED(hr));
    }

    return directWriteFactory;
}

IDWriteGdiInterop* Font::systemDWriteGdiInterop()
{
    static IDWriteGdiInterop* directWriteGdiInterop = nullptr;
    if (!directWriteGdiInterop) {
        HRESULT hr = systemDWriteFactory()->GetGdiInterop(&directWriteGdiInterop);
        RELEASE_ASSERT(SUCCEEDED(hr));
    }

    return directWriteGdiInterop;
}

static Vector<WCHAR> getFaceName(IDWriteFont* font)
{
    if (!font)
        return Vector<WCHAR>();

    COMPtr<IDWriteLocalizedStrings> localizedFaceNames;
    HRESULT hr = font->GetFaceNames(&localizedFaceNames);
    RELEASE_ASSERT(SUCCEEDED(hr));

    UINT32 localeIndex = 0;
    BOOL exists = false;

    wchar_t localeName[LOCALE_NAME_MAX_LENGTH];
    int localeLength = GetUserDefaultLocaleName(localeName, LOCALE_NAME_MAX_LENGTH);
    if (localeLength)
        hr = localizedFaceNames->FindLocaleName(localeName, &localeIndex, &exists);

    if (!exists || !SUCCEEDED(hr))
        hr = localizedFaceNames->FindLocaleName(L"en-us", &localeIndex, &exists);

    if (!exists || !SUCCEEDED(hr))
        localeIndex = 0;

    UINT32 faceNameLength = 0;
    hr = localizedFaceNames->GetStringLength(localeIndex, &faceNameLength);
    if (!SUCCEEDED(hr))
        return Vector<WCHAR>();

    Vector<WCHAR> faceName(faceNameLength + 1);
    hr = localizedFaceNames->GetString(localeIndex, faceName.data(), faceName.size());

    return faceName;
}

void Font::platformInit()
{
    m_syntheticBoldOffset = m_platformData.syntheticBold() ? 1.0f : 0.f;
    m_scriptCache = 0;
    m_scriptFontProperties = 0;

    if (m_platformData.useGDI())
        return initGDIFont();

    float pointSize = m_platformData.size();

    auto font = m_platformData.dwFont();
    RELEASE_ASSERT(font);

    auto fontFace = m_platformData.dwFontFace();
    RELEASE_ASSERT(fontFace);

    DWRITE_FONT_METRICS fontMetrics;
    font->GetMetrics(&fontMetrics);

    int iAscent = fontMetrics.ascent;
    int iDescent = fontMetrics.descent;
    int iLineGap = fontMetrics.lineGap;
    int iCapHeight = fontMetrics.capHeight;

    unsigned unitsPerEm = fontMetrics.designUnitsPerEm;
    float fAscent = scaleEmToUnits(iAscent, unitsPerEm) * pointSize;
    float fDescent = scaleEmToUnits(iDescent, unitsPerEm) * pointSize;
    float fCapHeight = scaleEmToUnits(iCapHeight, unitsPerEm) * pointSize;
    float fLineGap = scaleEmToUnits(iLineGap, unitsPerEm) * pointSize;

    if (origin() == Origin::Local) {
        Vector<WCHAR> faceName = getFaceName(font);
        fAscent = ascentConsideringMacAscentHack(faceName.data(), fAscent, fDescent);
    }

    m_fontMetrics.setAscent(fAscent);
    m_fontMetrics.setDescent(fDescent);
    m_fontMetrics.setCapHeight(fCapHeight);
    m_fontMetrics.setLineGap(fLineGap);
    m_fontMetrics.setLineSpacing(lroundf(fAscent) + lroundf(fDescent) + lroundf(fLineGap));

    Glyph xGlyph = glyphDataForCharacter('x').glyph;

    if (xGlyph) {
        // Measure the actual character "x", since it's possible for it to extend below the baseline, and we need the
        // reported x-height to only include the portion of the glyph that is above the baseline.
        Vector<DWRITE_GLYPH_METRICS> glyphMetrics(1);
        HRESULT hr = fontFace->GetDesignGlyphMetrics(&xGlyph, 1, glyphMetrics.data(), m_platformData.orientation() == FontOrientation::Vertical);
        RELEASE_ASSERT(SUCCEEDED(hr));
        m_fontMetrics.setXHeight(scaleEmToUnits(glyphMetrics.first().verticalOriginY, unitsPerEm) * pointSize);
    } else {
        int iXHeight = fontMetrics.xHeight;
        m_fontMetrics.setXHeight(scaleEmToUnits(iXHeight, unitsPerEm) * pointSize);
    }

    m_fontMetrics.setUnitsPerEm(unitsPerEm);
}

FloatRect Font::platformBoundsForGlyph(Glyph glyph) const
{
    if (!platformData().size())
        return FloatRect();

    if (m_platformData.useGDI())
        return boundsForGDIGlyph(glyph);

    auto font = m_platformData.dwFont();
    RELEASE_ASSERT(font);

    auto fontFace = m_platformData.dwFontFace();
    RELEASE_ASSERT(fontFace);

    float pointSize = m_platformData.size();
    bool vertical = m_platformData.orientation() == FontOrientation::Vertical;

    Vector<DWRITE_GLYPH_METRICS> glyphMetrics(1);
    HRESULT hr = fontFace->GetDesignGlyphMetrics(&glyph, 1, glyphMetrics.data(), m_platformData.orientation() == FontOrientation::Vertical);
    RELEASE_ASSERT(SUCCEEDED(hr));

    const auto& metrics = glyphMetrics.first();

    unsigned unitsPerEm = m_fontMetrics.unitsPerEm();

    FloatPoint origin(scaleEmToUnits(metrics.leftSideBearing, unitsPerEm) * pointSize, scaleEmToUnits(metrics.verticalOriginY, unitsPerEm) * pointSize);
    FloatSize size(scaleEmToUnits(metrics.advanceWidth - metrics.leftSideBearing - metrics.rightSideBearing, unitsPerEm) * pointSize,
        scaleEmToUnits(metrics.advanceHeight - metrics.topSideBearing - metrics.bottomSideBearing, unitsPerEm) * pointSize);

    FloatRect boundingBox(origin, size);
    if (m_syntheticBoldOffset)
        boundingBox.setWidth(boundingBox.width() + m_syntheticBoldOffset);

    return boundingBox;
}

float Font::platformWidthForGlyph(Glyph glyph) const
{
    if (!platformData().size())
        return 0;

    if (m_platformData.useGDI())
        return widthForGDIGlyph(glyph);

    auto font = m_platformData.dwFont();
    RELEASE_ASSERT(font);

    auto fontFace = m_platformData.dwFontFace();
    RELEASE_ASSERT(fontFace);

    bool isVertical = m_platformData.orientation() == FontOrientation::Vertical;

    Vector<DWRITE_GLYPH_METRICS> glyphMetrics(1);
    HRESULT hr = fontFace->GetDesignGlyphMetrics(&glyph, 1, glyphMetrics.data(), isVertical);
    RELEASE_ASSERT(SUCCEEDED(hr));

    const auto& metrics = glyphMetrics.first();
    int widthInEm = metrics.advanceWidth;

    float pointSize = m_platformData.size();
    unsigned unitsPerEm = m_fontMetrics.unitsPerEm();
    float width = scaleEmToUnits(widthInEm, unitsPerEm) * pointSize;

    return width + m_syntheticBoldOffset;
}

Path Font::platformPathForGlyph(Glyph) const
{
    notImplemented();
    return Path();
}

}

#endif
