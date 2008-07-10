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

#if ENABLE(SVG) && ENABLE(SVG_FILTERS)

#include "BlockExceptions.h"
#include "SVGFEDisplacementMap.h"
#include "SVGResourceFilter.h"
#include "SVGResourceFilterPlatformDataMac.h"
#include <QuartzCore/CoreImage.h>
#include <wtf/MathExtras.h>

class Color;
class LightSource;

namespace WebCore {

CIVector* getVectorForChannel(ChannelSelectorType channel);
CIColor* ciColor(const Color& c);

// Lighting
CIFilter* getPointLightVectors(CIFilter* normals, CIVector* lightPosition, float surfaceScale);
CIFilter* getLightVectors(CIFilter* normals, const LightSource* light, float surfaceScale);
CIFilter* getNormalMap(CIImage* bumpMap, float scale);

};

// Macros used by the SVGFE*Cg classes
#define FE_QUARTZ_SETUP_INPUT(name) \
    SVGResourceFilterPlatformDataMac* filterPlatformData = static_cast<SVGResourceFilterPlatformDataMac*>(svgFilter->platformData()); \
    CIImage* inputImage = filterPlatformData->inputImage(this); \
    FE_QUARTZ_CHECK_INPUT(inputImage) \
    CIFilter* filter; \
    BEGIN_BLOCK_OBJC_EXCEPTIONS; \
    filter = [CIFilter filterWithName:name]; \
    [filter setDefaults]; \
    [filter setValue:inputImage forKey:@"inputImage"];

#define FE_QUARTZ_CHECK_INPUT(input) \
    if (!input) \
        return nil;

#define FE_QUARTZ_OUTPUT_RETURN \
    filterPlatformData->setOutputImage(this, [filter valueForKey:@"outputImage"]); \
    return filter; \
    END_BLOCK_OBJC_EXCEPTIONS; \
    return nil;

#define FE_QUARTZ_MAP_TO_SUBREGION_PREPARE(bbox) \
    FloatRect filterRect = svgFilter->filterBBoxForItemBBox(bbox); \
    FloatRect cropRect = primitiveBBoxForFilterBBox(filterRect, bbox); \
    cropRect.intersect(filterRect); \
    cropRect.move(-filterRect.x(), -filterRect.y());

#define FE_QUARTZ_MAP_TO_SUBREGION_APPLY(cropRect) \
    { \
        CIFilter* crop = [CIFilter filterWithName:@"CICrop"]; \
        [crop setDefaults]; \
        if (CIImage* currentFilterOutputImage = [filter valueForKey:@"outputImage"]) { \
            [crop setValue:currentFilterOutputImage forKey:@"inputImage"]; \
            [crop setValue:[CIVector vectorWithX:cropRect.x() Y:cropRect.y() Z:cropRect.width() W:cropRect.height()] forKey:@"inputRectangle"]; \
            filter = crop; \
        } \
    }

#define FE_QUARTZ_MAP_TO_SUBREGION(bbox) \
    FE_QUARTZ_MAP_TO_SUBREGION_PREPARE(bbox); \
    FE_QUARTZ_MAP_TO_SUBREGION_APPLY(cropRect);

#endif // ENABLE(SVG) && ENABLE(SVG_FILTERS)
