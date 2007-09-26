/*
 * This file is part of the internal font implementation.  It should not be included by anyone other than
 * FontMac.cpp, FontWin.cpp and Font.cpp.
 *
 * Copyright (C) 2006, 2007 Apple Inc.
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

#ifndef FontPlatformData_H
#define FontPlatformData_H

#include "StringImpl.h"

typedef struct HFONT__ *HFONT;
typedef struct CGFont *CGFontRef;

namespace WebCore {

class FontDescription;

class FontPlatformData
{
public:
    class Deleted {};

    // Used for deleted values in the font cache's hash tables.
    FontPlatformData(Deleted)
    : m_font((HFONT)-1)
    , m_cgFont(NULL)
    , m_size(0)
    {}

    FontPlatformData()
    : m_font(0)
    , m_cgFont(NULL)
    , m_size(0)
    {}

    FontPlatformData(HFONT, int size, bool bold, bool oblique);
    ~FontPlatformData();

    HFONT hfont() const { return m_font; }
    CGFontRef cgFont() const { return m_cgFont; }

    int size() const { return m_size; }
    bool syntheticBold() const { return m_syntheticBold; }
    bool syntheticOblique() const { return m_syntheticOblique; }

    unsigned hash() const
    { 
        return StringImpl::computeHash((UChar*)(&m_font), sizeof(HFONT) / sizeof(UChar));
    }

    bool operator==(const FontPlatformData& other) const
    { 
        return m_font == other.m_font
              && m_cgFont ==other.m_cgFont
              && m_size == other.m_size;
    }

private:
    HFONT m_font;
    CGFontRef m_cgFont;

    int m_size;
    bool m_syntheticBold;
    bool m_syntheticOblique;
};

}

#endif
