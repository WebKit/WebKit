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
#include "SVGFEHelpersCg.h"

#include "Color.h"
#include "SVGDistantLightSource.h"
#include "SVGLightSource.h"
#include "SVGPointLightSource.h"
#include "SVGSpotLightSource.h"

#import "WKDistantLightFilter.h"
#import "WKNormalMapFilter.h"
#import "WKPointLightFilter.h"
#import "WKSpotLightFilter.h"

#include <wtf/MathExtras.h>

namespace WebCore {

CIVector* getVectorForChannel(ChannelSelectorType channel)
{
    switch (channel) {
    case CHANNEL_UNKNOWN:
        return nil;    
    case CHANNEL_R:
        return [CIVector vectorWithX:1.0f Y:0.0f Z:0.0f W:0.0f];
    case CHANNEL_G:
        return [CIVector vectorWithX:0.0f Y:1.0f Z:0.0f W:0.0f];
    case CHANNEL_B:
        return [CIVector vectorWithX:0.0f Y:0.0f Z:1.0f W:0.0f];
    case CHANNEL_A:
        return [CIVector vectorWithX:0.0f Y:0.0f Z:0.0f W:1.0f];
    default:
        return [CIVector vectorWithX:0.0f Y:0.0f Z:0.0f W:0.0f];
    }
}

CIColor* ciColor(const Color& c)
{
    CGColorRef colorCG = createCGColor(c);
    CIColor* colorCI = [CIColor colorWithCGColor:colorCG];
    CGColorRelease(colorCG);
    return colorCI;
}

// Lighting
CIFilter* getPointLightVectors(CIFilter* normals, CIVector* lightPosition, float surfaceScale)
{
    CIFilter* filter;
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    filter = [CIFilter filterWithName:@"WKPointLight"];
    if (!filter)
        return nil;
    [filter setDefaults];
    [filter setValue:[normals valueForKey:@"outputImage"] forKey:@"inputNormalMap"];
    [filter setValue:lightPosition forKey:@"inputLightPosition"];
    [filter setValue:[NSNumber numberWithFloat:surfaceScale] forKey:@"inputSurfaceScale"];
    return filter;
    END_BLOCK_OBJC_EXCEPTIONS;
    return nil;
}

CIFilter* getLightVectors(CIFilter* normals, const LightSource* light, float surfaceScale)
{
    [WKDistantLightFilter class];
    [WKPointLightFilter class];
    [WKSpotLightFilter class];

    CIFilter* filter = nil;
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    switch (light->type()) {
    case LS_DISTANT:
    {
        const DistantLightSource* dlight = static_cast<const DistantLightSource*>(light);

        filter = [CIFilter filterWithName:@"WKDistantLight"];
        if (!filter)
            return nil;
        [filter setDefaults];

        float azimuth = dlight->azimuth();
        float elevation = dlight->elevation();
        azimuth = deg2rad(azimuth);
        elevation = deg2rad(elevation);
        float Lx = cosf(azimuth)*cosf(elevation);
        float Ly = sinf(azimuth)*cosf(elevation);
        float Lz = sinf(elevation);

        [filter setValue:[normals valueForKey:@"outputImage"] forKey:@"inputNormalMap"];
        [filter setValue:[CIVector vectorWithX:Lx Y:Ly Z:Lz] forKey:@"inputLightDirection"];
        return filter;
    }
    case LS_POINT:
    {
        const PointLightSource* plight = static_cast<const PointLightSource*>(light);
        return getPointLightVectors(normals, [CIVector vectorWithX:plight->position().x() Y:plight->position().y() Z:plight->position().z()], surfaceScale);
    }
    case LS_SPOT:
    {
        const SpotLightSource* slight = static_cast<const SpotLightSource*>(light);
        filter = [CIFilter filterWithName:@"WKSpotLight"];
        if (!filter)
            return nil;

        CIFilter* pointLightFilter = getPointLightVectors(normals, [CIVector vectorWithX:slight->position().x() Y:slight->position().y() Z:slight->position().z()], surfaceScale);
        if (!pointLightFilter)
            return nil;
        [filter setDefaults];

        [filter setValue:[pointLightFilter valueForKey:@"outputImage"] forKey:@"inputLightVectors"];
        [filter setValue:[CIVector vectorWithX:slight->direction().x() Y:slight->direction().y() Z:slight->direction().z()] forKey:@"inputLightDirection"];
        [filter setValue:[NSNumber numberWithFloat:slight->specularExponent()] forKey:@"inputSpecularExponent"];
        [filter setValue:[NSNumber numberWithFloat:deg2rad(slight->limitingConeAngle())] forKey:@"inputLimitingConeAngle"];
        return filter;
    }
    }

    END_BLOCK_OBJC_EXCEPTIONS;
    return nil;
}

CIFilter* getNormalMap(CIImage* bumpMap, float scale)
{
    [WKNormalMapFilter class];
    CIFilter* filter;
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    filter = [CIFilter filterWithName:@"WKNormalMap"];
    [filter setDefaults];

    [filter setValue:bumpMap forKey:@"inputImage"];
    [filter setValue:[NSNumber numberWithFloat:scale] forKey:@"inputSurfaceScale"];
    return filter;
    END_BLOCK_OBJC_EXCEPTIONS;
    return nil;
}

}

#endif // ENABLE(SVG) && ENABLE(SVG_FILTERS)
