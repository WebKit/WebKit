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

#ifndef FontPlatformData_H
#define FontPlatformData_H

#include "StringImpl.h"

typedef struct HFONT__ *HFONT;
typedef struct _cairo_scaled_font cairo_scaled_font_t;
typedef struct _cairo_font_face cairo_font_face_t;

#define WIN32_FONT_LOGICAL_SCALE 32

namespace WebCore {

class FontDescription;

class FontPlatformData
{
public:
    class Deleted {};

    // Used for deleted values in the font cache's hash tables.
    FontPlatformData(Deleted)
    : m_font((HFONT)-1), m_fontFace(0), m_scaledFont(0), m_size(0)
    {}

    FontPlatformData()
    : m_font(0), m_fontFace(0), m_scaledFont(0), m_size(0)
    {}

    FontPlatformData(HFONT, int size);
    ~FontPlatformData();

    HFONT hfont() const { return m_font; }
    cairo_font_face_t* fontFace() const { return m_fontFace; }
    cairo_scaled_font_t* scaledFont() const { return m_scaledFont; }

    int size() const { return m_size; }

    unsigned hash() const
    { 
        return StringImpl::computeHash((UChar*)(&m_font), sizeof(HFONT) / sizeof(UChar));
    }

    bool operator==(const FontPlatformData& other) const
    { 
        return m_font == other.m_font && m_fontFace == other.m_fontFace && 
               m_scaledFont == other.m_scaledFont && m_size == other.m_size;
    }

private:
    HFONT m_font;
    cairo_font_face_t* m_fontFace;
    cairo_scaled_font_t* m_scaledFont;
    int m_size;
};

}

#endif
