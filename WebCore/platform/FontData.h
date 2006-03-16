/*
 * This file is part of the internal font implementation.  It should not be included by anyone other than
 * FontMac.cpp, FontWin.cpp and Font.cpp.
 *
 * Copyright (C) 2006 Apple Computer, Inc.
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#include "FontPlatformData.h"

namespace WebCore {

class FontDescription;

class FontData
{
public:
#if WIN32
    FontData(HFONT font, const FontDescription& fontDescription)
        :m_platformData(font, fontDescription) {}
#endif

    const FontPlatformData& platformData() const { return m_platformData; }

    void setMetrics(int ascent, int descent, int xHeight, int lineSpacing)
    {
        m_ascent = ascent;
        m_descent = descent;
        m_xHeight = xHeight;
        m_lineSpacing = lineSpacing;
    }

    int ascent() const { return m_ascent; }
    int descent() const { return m_descent; }
    int xHeight() const { return m_xHeight; }
    int lineSpacing() const { return m_lineSpacing; }

private:
    FontPlatformData m_platformData;
    int m_ascent;
    int m_descent;
    int m_xHeight;
    int m_lineSpacing;
};

}
