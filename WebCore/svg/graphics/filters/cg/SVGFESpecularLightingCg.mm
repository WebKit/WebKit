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
#include "SVGFESpecularLighting.h"
#include "SVGFEHelpersCg.h"

#import "WKSpecularLightingFilter.h"

namespace WebCore {

CIFilter* SVGFESpecularLighting::getCIFilter(const FloatRect& bbox) const
{
    const SVGLightSource* light = lightSource();
    if (!light)
        return nil;

    [WKSpecularLightingFilter class];

    SVGResourceFilter* svgFilter = filter();
    SVGResourceFilterPlatformDataMac* filterPlatformData = static_cast<SVGResourceFilterPlatformDataMac*>(svgFilter->platformData());
    CIFilter* filter;
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    filter = [CIFilter filterWithName:@"WKSpecularLighting"];
    [filter setDefaults];
    CIImage* inputImage = filterPlatformData->inputImage(this);
    FE_QUARTZ_CHECK_INPUT(inputImage);
    CIFilter* normals = getNormalMap(inputImage, surfaceScale());
    if (!normals)
        return nil;
    CIFilter* lightVectors = getLightVectors(normals, light, surfaceScale());
    if (!lightVectors)
        return nil;
    [filter setValue:[normals valueForKey:@"outputImage"] forKey:@"inputNormalMap"];
    [filter setValue:[lightVectors valueForKey:@"outputImage"] forKey:@"inputLightVectors"];
    [filter setValue:ciColor(lightingColor()) forKey:@"inputLightingColor"];
    [filter setValue:[NSNumber numberWithFloat:surfaceScale()] forKey:@"inputSurfaceScale"];
    [filter setValue:[NSNumber numberWithFloat:specularConstant()] forKey:@"inputSpecularConstant"];
    [filter setValue:[NSNumber numberWithFloat:specularExponent()] forKey:@"inputSpecularExponent"];
    [filter setValue:[NSNumber numberWithFloat:kernelUnitLengthX()] forKey:@"inputKernelUnitLengthX"];
    [filter setValue:[NSNumber numberWithFloat:kernelUnitLengthY()] forKey:@"inputKernelUnitLengthY"];

    FE_QUARTZ_MAP_TO_SUBREGION(bbox);
    FE_QUARTZ_OUTPUT_RETURN;
}

}

#endif // ENABLE(SVG) && ENABLE(SVG_FILTERS)
