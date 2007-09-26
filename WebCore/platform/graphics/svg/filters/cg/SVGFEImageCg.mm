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

#if ENABLE(SVG) && ENABLE(SVG_EXPERIMENTAL_FEATURES)
#include "SVGFEImage.h"

#include "Image.h"
#include "SVGFEHelpersCg.h"
#include "CgSupport.h"

namespace WebCore {

CIFilter* SVGFEImage::getCIFilter(SVGResourceFilter* svgFilter) const
{
    if (!cachedImage())
        return nil;

    CIFilter* filter;
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    CIImage* ciImage = [CIImage imageWithCGImage:cachedImage()->image()->getCGImageRef()];

    // FIXME: There is probably a nicer way to perform both of these transforms.
    filter = [CIFilter filterWithName:@"CIAffineTransform"];
    [filter setDefaults];
    [filter setValue:ciImage forKey:@"inputImage"];

    CGAffineTransform cgTransform = CGAffineTransformMake(1,0,0,-1,0,cachedImage()->image()->rect().bottom());
    NSAffineTransform* nsTransform = [NSAffineTransform transform];
    [nsTransform setTransformStruct:*((NSAffineTransformStruct *)&cgTransform)];
    [filter setValue:nsTransform forKey:@"inputTransform"];

    if (!subRegion().isEmpty()) {
        CIFilter* scaleImage = [CIFilter filterWithName:@"CIAffineTransform"];
        [scaleImage setDefaults];
        [scaleImage setValue:[filter valueForKey:@"outputImage"] forKey:@"inputImage"];

        cgTransform = CGAffineTransformMakeMapBetweenRects(CGRect(cachedImage()->image()->rect()), subRegion());
        [nsTransform setTransformStruct:*((NSAffineTransformStruct *)&cgTransform)];
        [scaleImage setValue:nsTransform forKey:@"inputTransform"];
        filter = scaleImage;
    }

    FE_QUARTZ_OUTPUT_RETURN;
}

}

#endif // ENABLE(SVG) && ENABLE(SVG_EXPERIMENTAL_FEATURES)
