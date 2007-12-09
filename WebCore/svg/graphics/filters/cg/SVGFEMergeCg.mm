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
#include "SVGFEMerge.h"
#include "SVGFEHelpersCg.h"

namespace WebCore {

CIFilter* SVGFEMerge::getCIFilter(const FloatRect& bbox) const
{
    SVGResourceFilter* svgFilter = filter();
    SVGResourceFilterPlatformDataMac* filterPlatformData = static_cast<SVGResourceFilterPlatformDataMac*>(svgFilter->platformData());

    CIFilter* filter = nil;
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    const Vector<String>& inputs = mergeInputs();

    CIImage* previousOutput = filterPlatformData->inputImage(this);
    for (unsigned x = 0; x < inputs.size(); x++) {
        CIImage* inputImage = filterPlatformData->imageForName(inputs[x]);
        FE_QUARTZ_CHECK_INPUT(inputImage);
        FE_QUARTZ_CHECK_INPUT(previousOutput);
        filter = [CIFilter filterWithName:@"CISourceOverCompositing"];
        [filter setDefaults];
        [filter setValue:inputImage forKey:@"inputImage"];
        [filter setValue:previousOutput forKey:@"inputBackgroundImage"];
        previousOutput = [filter valueForKey:@"outputImage"];
    }

    FE_QUARTZ_MAP_TO_SUBREGION(bbox);
    FE_QUARTZ_OUTPUT_RETURN;
}

}

#endif // ENABLE(SVG) && ENABLE(SVG_FILTERS)
