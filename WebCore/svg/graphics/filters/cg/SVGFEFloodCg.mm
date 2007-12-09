/*
    Copyright (C) 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>

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

#if ENABLE(SVG) && ENABLE(SVG_FILTERS)
#include "SVGFEFlood.h"

#include "AffineTransform.h"
#include "SVGFEHelpersCg.h"
#include "CgSupport.h"

namespace WebCore {

CIFilter* SVGFEFlood::getCIFilter(const FloatRect& bbox) const
{
    SVGResourceFilter* svgFilter = filter();
    SVGResourceFilterPlatformDataMac* filterPlatformData = static_cast<SVGResourceFilterPlatformDataMac*>(svgFilter->platformData());
    CIFilter* filter;
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    filter = [CIFilter filterWithName:@"CIConstantColorGenerator"];
    [filter setDefaults];
    CGColorRef color = cgColor(floodColor());
    CGColorRef withAlpha = CGColorCreateCopyWithAlpha(color, CGColorGetAlpha(color) * floodOpacity());
    CIColor* inputColor = [CIColor colorWithCGColor:withAlpha];
    CGColorRelease(color);
    CGColorRelease(withAlpha);
    [filter setValue:inputColor forKey:@"inputColor"];

    FE_QUARTZ_MAP_TO_SUBREGION(bbox);
    FE_QUARTZ_OUTPUT_RETURN;
}

}

#endif // ENABLE(SVG) && ENABLE(SVG_FILTERS)
