/*
    Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.

    This class provides all functionality needed for loading images, style sheets and html
    pages from the web. It has a memory cache for these objects.
*/
#include "config.h"
#include "FontCustomPlatformData.h"

#include "FontPlatformData.h"
#include "SharedBuffer.h"
#if !HAVE(QRAWFONT)
#include <QFontDatabase>
#endif
#include <QStringList>

namespace WebCore {

FontCustomPlatformData::~FontCustomPlatformData()
{
#if !HAVE(QRAWFONT)
    QFontDatabase::removeApplicationFont(m_handle);
#endif
}

FontPlatformData FontCustomPlatformData::fontPlatformData(int size, bool bold, bool italic, FontOrientation, TextOrientation, FontWidthVariant, FontRenderingMode)
{
#if !HAVE(QRAWFONT)
    QFont font;
    font.setFamily(QFontDatabase::applicationFontFamilies(m_handle)[0]);
    font.setPixelSize(size);
    if (bold)
        font.setWeight(QFont::Bold);
    font.setItalic(italic);
    return FontPlatformData(font);
#else
    Q_ASSERT(m_rawFont.isValid());
    m_rawFont.setPixelSize(qreal(size));
    return FontPlatformData(m_rawFont);
#endif
}

FontCustomPlatformData* createFontCustomPlatformData(SharedBuffer* buffer)
{
    ASSERT_ARG(buffer, buffer);

    const QByteArray fontData(buffer->data(), buffer->size());
#if !HAVE(QRAWFONT)
    int id = QFontDatabase::addApplicationFontFromData(fontData);
    if (id == -1)
        return 0;
    Q_ASSERT(QFontDatabase::applicationFontFamilies(id).size() > 0);
#else
    // Pixel size doesn't matter at this point, it is set in FontCustomPlatformData::fontPlatformData.
    QRawFont rawFont(fontData, /*pixelSize = */0, QFont::PreferDefaultHinting);
    if (!rawFont.isValid())
        return 0;
#endif

    FontCustomPlatformData *data = new FontCustomPlatformData;
#if !HAVE(QRAWFONT)
    data->m_handle = id;
#else
    data->m_rawFont = rawFont;
#endif
    return data;
}

bool FontCustomPlatformData::supportsFormat(const String& format)
{
    return equalIgnoringCase(format, "truetype") || equalIgnoringCase(format, "opentype");
}

}
