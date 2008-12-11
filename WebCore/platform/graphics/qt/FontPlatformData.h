/*
    Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
    Copyright (C) 2008 Holger Hans Peter Freyther

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

class FontPlatformData
{
public:
#if ENABLE(SVG_FONTS)
    FontPlatformData(float size, bool bold, bool oblique);
#endif
    FontPlatformData();
    FontPlatformData(const FontDescription&, int wordSpacing = 0, int letterSpacing = 0);
    FontPlatformData(const QFont&, bool bold);

    QFont font() const { return m_font; }
    float size() const { return m_size; }

    float m_size;
    bool m_bold;
    bool m_oblique;
    QFont m_font;
};

} // namespace WebCore

#endif // FontPlatformData_h
