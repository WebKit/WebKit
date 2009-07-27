/*
    Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
    Copyright (C) 2008 Holger Hans Peter Freyther
    Copyright (C) 2009 Torch Mobile Inc. http://www.torchmobile.com/

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
#ifndef FontPlatformData_h
#define FontPlatformData_h

#include "FontDescription.h"
#include <QFont>

namespace WebCore {

class String;

class FontPlatformData
{
public:
#if ENABLE(SVG_FONTS)
    FontPlatformData(float size, bool bold, bool oblique);
#endif
    FontPlatformData();
    FontPlatformData(const FontDescription&, int wordSpacing = 0, int letterSpacing = 0);
    FontPlatformData(const QFont&, bool bold);

    static inline QFont::Weight toQFontWeight(FontWeight fontWeight)
    {
        switch (fontWeight) {
        case FontWeight100:
        case FontWeight200:
            return QFont::Light;  // QFont::Light == Weight of 25
        case FontWeight600:
            return QFont::DemiBold;  // QFont::DemiBold == Weight of 63
        case FontWeight700:
        case FontWeight800:
            return QFont::Bold;  // QFont::Bold == Weight of 75
        case FontWeight900:
            return QFont::Black;  // QFont::Black == Weight of 87
        case FontWeight300:
        case FontWeight400:
        case FontWeight500:
        default:
            return QFont::Normal;  // QFont::Normal == Weight of 50
        }
    }

    QFont font() const { return m_font; }
    float size() const { return m_size; }
    QString family() const { return m_font.family(); }
    bool bold() const { return m_bold; }
    bool italic() const { return m_font.italic(); }
    bool smallCaps() const { return m_font.capitalization() == QFont::SmallCaps; }
    int pixelSize() const { return m_font.pixelSize(); }

#ifndef NDEBUG
    String description() const;
#endif

    float m_size;
    bool m_bold;
    bool m_oblique;
    QFont m_font;
};

} // namespace WebCore

#endif // FontPlatformData_h
