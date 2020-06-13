/*
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2011 Igalia S.L.
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
#include "Color.h"

#include "ColorUtilities.h"
#include <gdk/gdk.h>

namespace WebCore {

Color::Color(const GdkRGBA& c)
{
    setSimpleColor(makeSimpleColor(
        static_cast<int>(c.red * 255),
        static_cast<int>(c.green * 255),
        static_cast<int>(c.blue * 255),
        static_cast<int>(c.alpha * 255)
    ));
}

Color::operator GdkRGBA() const
{
    auto [r, g, b, a] = toSRGBALossy();
    return { r, g, b, a };
}

}

// vim: ts=4 sw=4 et
