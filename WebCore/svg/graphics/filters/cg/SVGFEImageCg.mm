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
#include "SVGFEImage.h"

#include "Image.h"
#include "SVGFEHelpersCg.h"
#include "CgSupport.h"

namespace WebCore {

CIFilter* SVGFEImage::getCIFilter(const FloatRect& bbox) const
{
    if (!cachedImage())
        return nil;

    SVGResourceFilter* svgFilter = filter();
    SVGResourceFilterPlatformDataMac* filterPlatformData = static_cast<SVGResourceFilterPlatformDataMac*>(svgFilter->platformData());

    CIFilter* filter;
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    CIImage* ciImage = [CIImage imageWithCGImage:cachedImage()->image()->getCGImageRef()];

    filter = [CIFilter filterWithName:@"CIAffineTransform"];
    [filter setDefaults];
    [filter setValue:ciImage forKey:@"inputImage"];

    FloatRect imageRect = cachedImage()->image()->rect();

    // Flip image into right origin
    CGAffineTransform cgTransform = CGAffineTransformMake(1.0f, 0.0f, 0.0f, -1.0f, 0.0f, imageRect.bottom());
    NSAffineTransform* nsTransform = [NSAffineTransform transform];
    [nsTransform setTransformStruct:*((NSAffineTransformStruct *)&cgTransform)];
    [filter setValue:nsTransform forKey:@"inputTransform"];

    // Calculate crop rect
    FE_QUARTZ_MAP_TO_SUBREGION_PREPARE(bbox);

    // Map between the image rectangle and the crop rect
    if (!cropRect.isEmpty()) {
        CIFilter* scaleImage = [CIFilter filterWithName:@"CIAffineTransform"];
        [scaleImage setDefaults];
        [scaleImage setValue:[filter valueForKey:@"outputImage"] forKey:@"inputImage"];

        cgTransform = CGAffineTransformMakeMapBetweenRects(CGRect(imageRect), CGRect(cropRect));
        [nsTransform setTransformStruct:*((NSAffineTransformStruct *)&cgTransform)];
        [scaleImage setValue:nsTransform forKey:@"inputTransform"];

        filter = scaleImage;
    }

    // Actually apply cropping
    FE_QUARTZ_MAP_TO_SUBREGION_APPLY(cropRect);
    FE_QUARTZ_OUTPUT_RETURN;
}

}

#endif // ENABLE(SVG) && ENABLE(SVG_FILTERS)
