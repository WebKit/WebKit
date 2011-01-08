/*
    Copyright (C) 2007 Alp Toker <alp.toker@collabora.co.uk>
    Copyright (C) 2010 Igalia S.L.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    aint with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef CairoPath_h
#define CairoPath_h

#include <cairo.h>

namespace WebCore {

// This is necessary since cairo_path_fixed_t isn't exposed in Cairo's public API.
class CairoPath {
public:
    CairoPath()
    {
        static cairo_surface_t* pathSurface = cairo_image_surface_create(CAIRO_FORMAT_A8, 1, 1);
        m_cr = cairo_create(pathSurface);
    }

    ~CairoPath()
    {
        cairo_destroy(m_cr);
    }

    cairo_t* context() { return m_cr; }

private:
    cairo_t* m_cr;
};

} // namespace WebCore

#endif // CairoPath_h
