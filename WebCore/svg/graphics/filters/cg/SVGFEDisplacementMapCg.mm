/*
    Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>

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
#include "SVGFEDisplacementMap.h"
#include "SVGFEHelpersCg.h"

#import "WKDisplacementMapFilter.h"

namespace WebCore {

CIFilter* SVGFEDisplacementMap::getCIFilter(const FloatRect& bbox) const
{
    SVGResourceFilter* svgFilter = filter();
    SVGResourceFilterPlatformDataMac* filterPlatformData = static_cast<SVGResourceFilterPlatformDataMac*>(svgFilter->platformData());

    CIFilter* filter = nil;
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [WKDisplacementMapFilter class];
    filter = [CIFilter filterWithName:@"WKDisplacementMapFilter"];
    [filter setDefaults];
    CIImage* inputImage = filterPlatformData->inputImage(this);
    CIImage* displacementMap = filterPlatformData->imageForName(in2());
    FE_QUARTZ_CHECK_INPUT(inputImage);
    FE_QUARTZ_CHECK_INPUT(displacementMap);
    [filter setValue:inputImage forKey:@"inputImage"];
    [filter setValue:displacementMap forKey:@"inputDisplacementMap"];
    [filter setValue:getVectorForChannel(xChannelSelector()) forKey:@"inputXChannelSelector"];
    [filter setValue:getVectorForChannel(yChannelSelector()) forKey:@"inputYChannelSelector"];
    [filter setValue:[NSNumber numberWithFloat:scale()] forKey:@"inputScale"];

    FE_QUARTZ_MAP_TO_SUBREGION(bbox);
    FE_QUARTZ_OUTPUT_RETURN;
}

}

#endif // ENABLE(SVG) && ENABLE(SVG_FILTERS)
