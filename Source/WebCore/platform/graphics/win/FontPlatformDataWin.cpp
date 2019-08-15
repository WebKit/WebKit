/*
 * This file is part of the internal font implementation.  It should not be included by anyone other than
 * FontMac.cpp, FontWin.cpp and Font.cpp.
 *
 * Copyright (C) 2006-2008, 2016 Apple Inc.
 * Copyright (C) 2008 Brent Fulgham
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

#include "HWndDC.h"
#include "SharedBuffer.h"
#include <wtf/HashMap.h>
#include <wtf/RetainPtr.h>
#include <wtf/Vector.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

#if USE(DIRECT2D)
#include <dwrite_3.h>
#endif

using std::min;

namespace WebCore {

FontPlatformData::FontPlatformData(GDIObject<HFONT> font, float size, bool bold, bool oblique, bool useGDI)
    : m_font(SharedGDIObject<HFONT>::create(WTFMove(font)))
    , m_size(size)
    , m_syntheticBold(bold)
    , m_syntheticOblique(oblique)
    , m_useGDI(useGDI)
{
    HWndDC hdc(0);
    SaveDC(hdc);
    
    ::SelectObject(hdc, m_font->get());

    wchar_t faceName[LF_FACESIZE];
    GetTextFace(hdc, LF_FACESIZE, faceName);
    platformDataInit(m_font->get(), size, hdc, faceName);

    RestoreDC(hdc, -1);
}

RefPtr<SharedBuffer> FontPlatformData::openTypeTable(uint32_t table) const
{
    HWndDC hdc(0);
    HGDIOBJ oldFont = SelectObject(hdc, hfont());

    DWORD size = GetFontData(hdc, table, 0, 0, 0);
    RefPtr<SharedBuffer> buffer;
    if (size != GDI_ERROR) {
        Vector<char> data(size);
        DWORD result = GetFontData(hdc, table, 0, (PVOID)data.data(), size);
        ASSERT_UNUSED(result, result == size);
        buffer = SharedBuffer::create(WTFMove(data));
    }

    SelectObject(hdc, oldFont);
    return buffer;
}

#if !LOG_DISABLED
String FontPlatformData::description() const
{
    return String();
}
#endif

}
