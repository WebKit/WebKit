/*
 * Copyright (C) 2007 Apple Computer, Inc.
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

#ifndef FontCustomPlatformDataCairo_h
#define FontCustomPlatformDataCairo_h

#include <wtf/Noncopyable.h>

#include <cairo.h>

namespace WebCore {

class FontPlatformData;
class SharedBuffer;

struct FontCustomPlatformDataCairo : Noncopyable {
    FontCustomPlatformDataCairo(cairo_font_face_t* fontFace)
        : m_fontFace(fontFace)
    {
    }
    ~FontCustomPlatformDataCairo();

    FontPlatformData fontPlatformData(int size, bool bold, bool italic);

    cairo_font_face_t* m_fontFace;
};

FontCustomPlatformDataCairo* createFontCustomPlatformData(SharedBuffer*);

}

#endif
