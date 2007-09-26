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

#if ENABLE(SVG) && ENABLE(SVG_EXPERIMENTAL_FEATURES)

#include "BlockExceptions.h"
#include "SVGFEDisplacementMap.h"
#include "SVGResourceFilter.h"
#include <QuartzCore/CoreImage.h>
#include <wtf/MathExtras.h>

class Color;
class SVGLightSource;

namespace WebCore {

CIVector* getVectorForChannel(SVGChannelSelectorType channel);
CIColor* ciColor(const Color& c);

// Lighting
CIFilter* getPointLightVectors(CIFilter* normals, CIVector* lightPosition, float surfaceScale);
CIFilter* getLightVectors(CIFilter* normals, const SVGLightSource* light, float surfaceScale);
CIFilter* getNormalMap(CIImage* bumpMap, float scale);

};

// Macros used by the SVGFE*Cg classes
#define FE_QUARTZ_SETUP_INPUT(name) \
    CIImage* inputImage = svgFilter->inputImage(this); \
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
    svgFilter->setOutputImage(this, [filter valueForKey:@"outputImage"]); \
    return filter; \
    END_BLOCK_OBJC_EXCEPTIONS; \
    return nil;

#define FE_QUARTZ_CROP_TO_RECT(rect) \
    { \
        CIFilter* crop = [CIFilter filterWithName:@"CICrop"]; \
        [crop setDefaults]; \
        [crop setValue:[filter valueForKey:@"outputImage"] forKey:@"inputImage"]; \
        [crop setValue:[CIVector vectorWithX:rect.origin.x Y:rect.origin.y Z:rect.size.width W:rect.size.height] forKey:@"inputRectangle"]; \
        filter = crop; \
    }

#define deg2rad(d) ((d * (2.0 * piDouble)) / 360.0)

#endif // ENABLE(SVG) && ENABLE(SVG_EXPERIMENTAL_FEATURES)
