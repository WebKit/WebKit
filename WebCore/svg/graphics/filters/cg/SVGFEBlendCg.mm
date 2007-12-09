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
#include "SVGFEBlend.h"
#include "SVGFEHelpersCg.h"

namespace WebCore {

CIFilter* SVGFEBlend::getCIFilter(const FloatRect& bbox) const
{
    SVGResourceFilter* svgFilter = filter();
    SVGResourceFilterPlatformDataMac* filterPlatformData = static_cast<SVGResourceFilterPlatformDataMac*>(svgFilter->platformData());
    CIFilter* filter = nil;
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    switch (blendMode()) {
    case SVG_FEBLEND_MODE_UNKNOWN:
        return nil;
    case SVG_FEBLEND_MODE_NORMAL:
        // FIXME: I think this is correct....
        filter = [CIFilter filterWithName:@"CISourceOverCompositing"];
        break;
    case SVG_FEBLEND_MODE_MULTIPLY:
        filter = [CIFilter filterWithName:@"CIMultiplyBlendMode"];
        break;
    case SVG_FEBLEND_MODE_SCREEN:
        filter = [CIFilter filterWithName:@"CIScreenBlendMode"];
        break;
    case SVG_FEBLEND_MODE_DARKEN:
        filter = [CIFilter filterWithName:@"CIDarkenBlendMode"];
        break;
    case SVG_FEBLEND_MODE_LIGHTEN:
        filter = [CIFilter filterWithName:@"CILightenBlendMode"];
        break;
    default:
        LOG_ERROR("Unhandled blend mode: %i", blendMode());
        return nil;
    }

    [filter setDefaults];

    CIImage* inputImage = filterPlatformData->inputImage(this);
    FE_QUARTZ_CHECK_INPUT(inputImage);
    [filter setValue:inputImage forKey:@"inputImage"];

    CIImage* backgroundImage = filterPlatformData->imageForName(in2());
    FE_QUARTZ_CHECK_INPUT(backgroundImage);
    [filter setValue:backgroundImage forKey:@"inputBackgroundImage"];

    FE_QUARTZ_MAP_TO_SUBREGION(bbox);
    FE_QUARTZ_OUTPUT_RETURN;
}

}

#endif // ENABLE(SVG) && ENABLE(SVG_FILTERS)
