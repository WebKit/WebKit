/*
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2011 Collabora Ltd.
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
 */

#include "config.h"
#include "IntRect.h"

#if USE(CAIRO)

#include <cairo.h>

namespace WebCore {

IntRect::IntRect(const cairo_rectangle_int_t& r)
    : m_location(IntPoint(r.x, r.y))
    , m_size(r.width, r.height)
{
}

IntRect::operator cairo_rectangle_int_t() const
{
    cairo_rectangle_int_t r = { x(), y(), width(), height() };
    return r;
}

} // namespace WebCore

#endif // USE(CAIRO)
