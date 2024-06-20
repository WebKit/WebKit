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

#include "FontCustomPlatformData.h"
#include "HWndDC.h"
#include "SharedBuffer.h"
#include <wtf/HashMap.h>
#include <wtf/RetainPtr.h>
#include <wtf/Vector.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

FontPlatformData::FontPlatformData(GDIObject<HFONT> font, float size, bool bold, bool oblique, const FontCustomPlatformData* customPlatformData)
    : FontPlatformData(size, bold, oblique, FontOrientation::Horizontal, FontWidthVariant::RegularWidth, TextRenderingMode::AutoTextRendering, customPlatformData)
{
    m_font = SharedGDIObject<HFONT>::create(WTFMove(font));
    platformDataInit(m_font->get(), size);
}

RefPtr<SharedBuffer> FontPlatformData::platformOpenTypeTable(uint32_t table) const
{
    HWndDC hdc(0);
    HGDIOBJ oldFont = SelectObject(hdc, hfont());

    DWORD size = GetFontData(hdc, table, 0, 0, 0);
    RefPtr<SharedBuffer> buffer;
    if (size != GDI_ERROR) {
        Vector<uint8_t> data(size);
        DWORD result = GetFontData(hdc, table, 0, (PVOID)data.data(), size);
        ASSERT_UNUSED(result, result == size);
        buffer = SharedBuffer::create(WTFMove(data));
    }

    SelectObject(hdc, oldFont);
    return buffer;
}

FontPlatformData FontPlatformData::create(const Attributes& data, const FontCustomPlatformData* custom)
{
    LOGFONT logFont = data.m_font;
    if (custom)
        wcscpy_s(logFont.lfFaceName, LF_FACESIZE, custom->name.wideCharacters().data());

    auto gdiFont = adoptGDIObject(CreateFontIndirect(&logFont));
    return FontPlatformData(WTFMove(gdiFont), data.m_size, data.m_syntheticBold, data.m_syntheticOblique, custom);
}

FontPlatformData::Attributes FontPlatformData::attributes() const
{
    Attributes result(m_size, m_orientation, m_widthVariant, m_textRenderingMode, m_syntheticBold, m_syntheticOblique);

    GetObject(hfont(), sizeof(LOGFONT), &result.m_font);
    return result;
}

}
