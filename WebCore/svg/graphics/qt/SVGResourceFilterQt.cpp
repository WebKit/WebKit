/*
    Copyright (C) 2006 Nikolas Zimmermann <wildfox@kde.org>

    This file is part of the KDE project

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

#include "config.h"

#if ENABLE(SVG) && ENABLE(SVG_EXPERIMENTAL_FEATURES)
#include "SVGResourceFilter.h"

namespace WebCore {

SVGResourceFilterPlatformData* SVGResourceFilter::createPlatformData()
{
    return 0;
}

void SVGResourceFilter::prepareFilter(GraphicsContext*&, const FloatRect&)
{
    // FIXME: implement me :-)
}

void SVGResourceFilter::applyFilter(GraphicsContext*&, const FloatRect&)
{
    // FIXME: implement me :-)
}

} // namespace WebCore

#endif

// vim:ts=4:noet
