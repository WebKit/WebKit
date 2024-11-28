/*
 * This file is part of the internal font implementation.  It should not be included by anyone other than
 * FontMac.cpp, FontWin.cpp and Font.cpp.
 *
 * Copyright (C) 2006-2008, 2016 Apple Inc.
 * Copyright (C) 2007 Alp Toker
 * Copyright (C) 2008, 2010, 2011 Brent Fulgham
 * Copyright (C) 2020 Sony Interactive Entertainment Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "FontPlatformData.h"

#if PLATFORM(WIN) && USE(CAIRO)

#include "HWndDC.h"
#include "SharedBuffer.h"
#include <cairo-dwrite.h>
#include <wtf/HashMap.h>
#include <wtf/RetainPtr.h>
#include <wtf/Vector.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

static IDWriteGdiInterop* getDWriteGdiInterop()
{
    static COMPtr<IDWriteGdiInterop> gdiInterop;
    if (gdiInterop)
        return gdiInterop.get();
    COMPtr<IDWriteFactory> factory;
    HRESULT hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(&factory));
    RELEASE_ASSERT(SUCCEEDED(hr));
    hr = factory->GetGdiInterop(&gdiInterop);
    RELEASE_ASSERT(SUCCEEDED(hr));
    return gdiInterop.get();
}

cairo_font_face_t*
createCairoDWriteFontFace(HFONT font)
{
    IDWriteGdiInterop* gdiInterop = getDWriteGdiInterop();

    bool retry = false;
    GDIObject<HFONT> retryFont;
    COMPtr<IDWriteFontFace> dwFace;
    HWndDC hdc(nullptr);
    while (font) {
        HGDIOBJ oldFont = SelectObject(hdc, font);
        HRESULT hr = gdiInterop->CreateFontFaceFromHdc(hdc, &dwFace);
        SelectObject(hdc, oldFont);
        if (SUCCEEDED(hr))
            break;
        if (retry)
            break;
        // CreateFontFaceFromHdc may fail if the font size is too large. Retry it by creating a smaller font.
        LOGFONT logfont;
        GetObject(font, sizeof(logfont), &logfont);
        logfont.lfHeight = -32;
        retryFont = adoptGDIObject(CreateFontIndirect(&logfont));
        font = retryFont.get();
        retry = true;
    }
    RELEASE_ASSERT(dwFace);
    return cairo_dwrite_font_face_create_for_dwrite_fontface(dwFace.get());
}

void FontPlatformData::platformDataInit(HFONT font, float size)
{
    cairo_font_face_t* fontFace = createCairoDWriteFontFace(font);

    cairo_matrix_t sizeMatrix, ctm;
    cairo_matrix_init_identity(&ctm);
    cairo_matrix_init_scale(&sizeMatrix, size, size);

    static cairo_font_options_t* fontOptions = nullptr;
    if (!fontOptions) {
        fontOptions = cairo_font_options_create();
        cairo_font_options_set_antialias(fontOptions, CAIRO_ANTIALIAS_SUBPIXEL);
    }

    m_scaledFont = adoptRef(cairo_scaled_font_create(fontFace, &sizeMatrix, &ctm, fontOptions));
    cairo_font_face_destroy(fontFace);
}

FontPlatformData::FontPlatformData(GDIObject<HFONT> font, cairo_font_face_t* fontFace, float size, bool bold, bool oblique, const FontCustomPlatformData* customPlatformData)
    : FontPlatformData(size, bold, oblique, FontOrientation::Horizontal, FontWidthVariant::RegularWidth, TextRenderingMode::AutoTextRendering, customPlatformData)
{
    m_hfont = SharedGDIObject<HFONT>::create(WTFMove(font));

    cairo_matrix_t fontMatrix;
    cairo_matrix_init_scale(&fontMatrix, size, size);
    cairo_matrix_t ctm;
    cairo_matrix_init_identity(&ctm);
    cairo_font_options_t* options = cairo_font_options_create();

    // We force antialiasing and disable hinting to provide consistent
    // typographic qualities for custom fonts on all platforms.
    cairo_font_options_set_hint_style(options, CAIRO_HINT_STYLE_NONE);
    cairo_font_options_set_antialias(options, CAIRO_ANTIALIAS_BEST);

    if (syntheticOblique()) {
        static const float syntheticObliqueSkew = -tanf(14 * acosf(0) / 90);
        cairo_matrix_t skew = { 1, 0, syntheticObliqueSkew, 1, 0, 0 };
        cairo_matrix_multiply(&fontMatrix, &skew, &fontMatrix);
    }

    m_scaledFont = adoptRef(cairo_scaled_font_create(fontFace, &fontMatrix, &ctm, options));
    cairo_font_options_destroy(options);
}

unsigned FontPlatformData::hash() const
{
    return PtrHash<cairo_scaled_font_t*>::hash(m_scaledFont.get());
}

bool FontPlatformData::platformIsEqual(const FontPlatformData& other) const
{
    return m_hfont == other.m_hfont
        && m_scaledFont == other.m_scaledFont;
}

RefPtr<SharedBuffer> FontPlatformData::openTypeTable(uint32_t table) const
{
    return platformOpenTypeTable(table);
}

#if !LOG_DISABLED
String FontPlatformData::description() const
{
    return String();
}
#endif

String FontPlatformData::familyName() const
{
    HWndDC hdc(0);
    HGDIOBJ oldFont = SelectObject(hdc, m_hfont.get());
    wchar_t faceName[LF_FACESIZE];
    GetTextFace(hdc, LF_FACESIZE, faceName);
    SelectObject(hdc, oldFont);
    return faceName;
}

} // namespace WebCore

#endif // PLATFORM(WIN) && USE(CAIRO)
