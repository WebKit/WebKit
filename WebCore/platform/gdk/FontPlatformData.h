/*
 * This file is part of the internal font implementation.  It should not be included by anyone other than
 * FontMac.cpp, FontWin.cpp and Font.cpp.
 *
 * Copyright (C) 2006 Apple Computer, Inc.
 * Copyright (C) 2006 Michael Emmel mike.emmel@gmail.com 
 * All rights reserved.
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

#include "GlyphBuffer.h"
#include "FontDescription.h"
#include <cairo.h>
#include <cairo-ft.h>
#include <fontconfig/fcfreetype.h>

namespace WebCore {

class FontPlatformData {
public:
    class Deleted {};
    FontPlatformData(Deleted) : m_pattern((FcPattern*)-1) { }

    FontPlatformData() : m_pattern(0) { }

    FontPlatformData(const FontDescription&, const AtomicString& family);
    ~FontPlatformData();

    static bool init();

    static cairo_font_face_t** list(FontDescription&, const AtomicString& familyName, int* length);
    Glyph index(unsigned ucs4) const;

    bool isFixedPitch();

    void setFont(cairo_t*) const;

    unsigned hash() const
    {
        return StringImpl::computeHash((UChar*)&m_fontDescription, sizeof(FontDescription) / sizeof(UChar));
    }

    bool operator==(const FontPlatformData&) const;

    FcPattern* m_pattern;
    FontDescription m_fontDescription;
    cairo_matrix_t* m_fontMatrix;
    cairo_font_face_t* m_fontFace;
    cairo_font_options_t* m_options;
    cairo_scaled_font_t* m_scaledFont;

};

}

#endif
