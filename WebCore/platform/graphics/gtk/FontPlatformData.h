/*
 * This file is part of the internal font implementation.  It should not be included by anyone other than
 * FontMac.cpp, FontWin.cpp and Font.cpp.
 *
 * Copyright (C) 2006 Apple Computer, Inc.
 * Copyright (C) 2006 Michael Emmel mike.emmel@gmail.com
 * Copyright (C) 2007 Holger Hans Peter Freyther
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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef FontPlatformData_h
#define FontPlatformData_h

#include "GlyphBuffer.h"
#include "FontDescription.h"
#include <cairo.h>
#include <cairo-ft.h>
#include <fontconfig/fcfreetype.h>

namespace WebCore {

class FontPlatformData {
public:
    class Deleted {};
    FontPlatformData(Deleted)
        : m_pattern(reinterpret_cast<FcPattern*>(-1))
        , m_scaledFont(0)
        { }

    FontPlatformData()
        : m_pattern(0)
        , m_scaledFont(0)
        { }

    FontPlatformData(const FontDescription&, const AtomicString& family);

    FontPlatformData(float size, bool bold, bool italic);
    FontPlatformData(cairo_font_face_t* fontFace, int size, bool bold, bool italic);

    ~FontPlatformData();

    static bool init();

    bool isFixedPitch();
    float size() const { return m_fontDescription.specifiedSize(); }

    void setFont(cairo_t*) const;

    unsigned hash() const
    {
        uintptr_t hashCodes[1] = { reinterpret_cast<uintptr_t>(m_scaledFont) };
        return StringImpl::computeHash( reinterpret_cast<UChar*>(hashCodes), sizeof(hashCodes) / sizeof(UChar));
    }

    bool operator==(const FontPlatformData&) const;

    FcPattern* m_pattern;
    FontDescription m_fontDescription;
    cairo_scaled_font_t* m_scaledFont;
};

}

#endif
