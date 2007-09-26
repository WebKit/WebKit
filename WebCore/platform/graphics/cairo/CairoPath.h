/*
    Copyright (C) 2007 Alp Toker <alp.toker@collabora.co.uk>

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

    struct CairoPath {
        cairo_t* m_cr;

        CairoPath()
        {
            cairo_surface_t* pathSurface = cairo_image_surface_create(CAIRO_FORMAT_A8, 1, 1);
            m_cr = cairo_create(pathSurface);
            cairo_surface_destroy(pathSurface);
            //FIXME: hack to work around no current point bug
            //should be fixed in cairo git
            cairo_move_to(m_cr, 0, 0);
        }

        ~CairoPath()
        {
            cairo_destroy(m_cr);
        }
    };

} // namespace WebCore

#endif // CairoPath_h
