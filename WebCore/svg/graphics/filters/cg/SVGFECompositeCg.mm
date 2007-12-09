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
#include "SVGFEComposite.h"
#include "SVGFEHelpersCg.h"

#import "WKArithmeticFilter.h"

namespace WebCore {

CIFilter* SVGFEComposite::getCIFilter(const FloatRect& bbox) const
{
    SVGResourceFilter* svgFilter = filter();
    SVGResourceFilterPlatformDataMac* filterPlatformData = static_cast<SVGResourceFilterPlatformDataMac*>(svgFilter->platformData());
    CIFilter* filter = nil;
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    switch (operation()) {
    case SVG_FECOMPOSITE_OPERATOR_UNKNOWN:
        return nil;
    case SVG_FECOMPOSITE_OPERATOR_OVER:
        filter = [CIFilter filterWithName:@"CISourceOverCompositing"];
        break;
    case SVG_FECOMPOSITE_OPERATOR_IN:
        filter = [CIFilter filterWithName:@"CISourceInCompositing"];
        break;
    case SVG_FECOMPOSITE_OPERATOR_OUT:
        filter = [CIFilter filterWithName:@"CISourceOutCompositing"];
        break;
    case SVG_FECOMPOSITE_OPERATOR_ATOP:
        filter = [CIFilter filterWithName:@"CISourceAtopCompositing"];
        break;
    case SVG_FECOMPOSITE_OPERATOR_XOR:
        //FIXME: I'm not sure this is right...
        filter = [CIFilter filterWithName:@"CIExclusionBlendMode"];
        break;
    case SVG_FECOMPOSITE_OPERATOR_ARITHMETIC:
        [WKArithmeticFilter class];
        filter = [CIFilter filterWithName:@"WKArithmeticFilter"];
        break;
    }

    [filter setDefaults];
    CIImage* inputImage = filterPlatformData->inputImage(this);
    CIImage* backgroundImage = filterPlatformData->imageForName(in2());
    FE_QUARTZ_CHECK_INPUT(inputImage);
    FE_QUARTZ_CHECK_INPUT(backgroundImage);
    [filter setValue:inputImage forKey:@"inputImage"];
    [filter setValue:backgroundImage forKey:@"inputBackgroundImage"];
    //FIXME: this seems ugly
    if (operation() == SVG_FECOMPOSITE_OPERATOR_ARITHMETIC) {
        [filter setValue:[NSNumber numberWithFloat:k1()] forKey:@"inputK1"];
        [filter setValue:[NSNumber numberWithFloat:k2()] forKey:@"inputK2"];
        [filter setValue:[NSNumber numberWithFloat:k3()] forKey:@"inputK3"];
        [filter setValue:[NSNumber numberWithFloat:k4()] forKey:@"inputK4"];
    }

    FE_QUARTZ_MAP_TO_SUBREGION(bbox);
    FE_QUARTZ_OUTPUT_RETURN;
}

}

#endif // ENABLE(SVG) && ENABLE(SVG_FILTERS)
