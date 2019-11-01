/*
 * Copyright (C) 2016-2019 Apple Inc.  All rights reserved.
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
#include "FontPlatformData.h"

#if USE(DIRECT2D)

#include "DirectWriteUtilities.h"
#include "GraphicsContext.h"
#include "HWndDC.h"
#include "SharedGDIObject.h"
#include <d2d1.h>
#include <dwrite_3.h>
#include <wtf/Vector.h>

namespace WebCore {

void FontPlatformData::platformDataInit(HFONT font, float size, HDC hdc, WCHAR* faceName)
{
    LOGFONT logfont;
    HRESULT hr = ::GetObject(font, sizeof(LOGFONT), &logfont);
    if (SUCCEEDED(hr))
        m_dwFont = DirectWrite::createWithPlatformFont(logfont);
    RELEASE_ASSERT(m_dwFont);

    hr = m_dwFont->CreateFontFace(&m_dwFontFace);
    RELEASE_ASSERT(SUCCEEDED(hr));

    if (!m_useGDI)
        m_isSystemFont = !wcscmp(faceName, L"Lucida Grande");
}

FontPlatformData::FontPlatformData(GDIObject<HFONT>&& hfont, COMPtr<IDWriteFont>&& font, float size, bool bold, bool oblique, bool useGDI)
    : m_syntheticBold(bold)
    , m_syntheticOblique(oblique)
    , m_size(size)
    , m_font(SharedGDIObject<HFONT>::create(WTFMove(hfont)))
    , m_dwFont(WTFMove(font))
    , m_useGDI(useGDI)
{
    HRESULT hr = m_dwFont->CreateFontFace(&m_dwFontFace);
    RELEASE_ASSERT(SUCCEEDED(hr));
}

static bool fontsAreEqual(IDWriteFont* a, IDWriteFont* b)
{
    if (!a && !b)
        return true;

    if (a == b)
        return true;

    if ((!a && b) || (a && !b))
        return false;

    if (a->GetWeight() != b->GetWeight())
        return false;

    if (a->GetStyle() != b->GetStyle())
        return false;

    if (a->GetStretch() != b->GetStretch())
        return false;

    DWRITE_FONT_METRICS aMetrics, bMetrics;
    a->GetMetrics(&aMetrics);
    b->GetMetrics(&bMetrics);

    if ((aMetrics.designUnitsPerEm != bMetrics.designUnitsPerEm)
        || (aMetrics.ascent != bMetrics.ascent)
        || (aMetrics.descent != bMetrics.ascent)
        || (aMetrics.lineGap != bMetrics.lineGap)
        || (aMetrics.capHeight != bMetrics.capHeight)
        || (aMetrics.xHeight != bMetrics.xHeight)
        || (aMetrics.underlinePosition != bMetrics.underlinePosition)
        || (aMetrics.underlineThickness != bMetrics.underlineThickness)
        || (aMetrics.strikethroughPosition != bMetrics.strikethroughPosition)
        || (aMetrics.strikethroughThickness != bMetrics.strikethroughThickness))
        return false;

    return true;
}

unsigned FontPlatformData::hash() const
{
    return m_font ? m_font->hash() : 0;
}

bool FontPlatformData::platformIsEqual(const FontPlatformData& other) const
{
    return m_font == other.m_font
        && m_useGDI == other.m_useGDI
        && fontsAreEqual(m_dwFont.get(), other.m_dwFont.get());
}

}

#endif
