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

#include "config.h"
#include "FontPlatformData.h"

#include "FontDescription.h"
#include <cairo-win32.h>

namespace WebCore {

FontPlatformData::FontPlatformData(HFONT font, const FontDescription& fontDescription)
{
    m_font = font;  
    m_fontFace = cairo_win32_font_face_create_for_hfont(font);
    cairo_matrix_t sizeMatrix, ctm;
    cairo_matrix_init_identity(&ctm);
    cairo_matrix_init_scale(&sizeMatrix, fontDescription.computedPixelSize(), fontDescription.computedPixelSize());
    cairo_font_options_t* fontOptions = cairo_font_options_create();
    m_scaledFont = cairo_scaled_font_create(m_fontFace, &sizeMatrix, &ctm, fontOptions);
    cairo_font_options_destroy(fontOptions);
}
    
FontPlatformData::~FontPlatformData() {
    cairo_font_face_destroy(m_fontFace);
    cairo_scaled_font_destroy(m_scaledFont);
    DeleteObject(m_font);
}

}
