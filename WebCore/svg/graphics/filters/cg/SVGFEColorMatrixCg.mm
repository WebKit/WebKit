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
#include "SVGFEColorMatrix.h"
#include "SVGFEHelpersCg.h"

#include <wtf/MathExtras.h>

namespace WebCore {

#define CMValuesCheck(expected, type) \
    if (values().size() != expected) { \
        NSLog(@"Error, incorrect number of values in ColorMatrix for type \"%s\", expected: %i actual: %i, ignoring filter.  Values:", type, expected, values().size()); \
        for (unsigned x=0; x < values().size(); x++) fprintf(stderr, " %f", values()[x]); \
        fprintf(stderr, "\n"); \
        return nil; \
    }

CIFilter* SVGFEColorMatrix::getCIFilter(const FloatRect& bbox) const
{
    SVGResourceFilter* svgFilter = filter();
    SVGResourceFilterPlatformDataMac* filterPlatformData = static_cast<SVGResourceFilterPlatformDataMac*>(svgFilter->platformData());
    CIFilter* filter = nil;
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    switch (type()) {
    case SVG_FECOLORMATRIX_TYPE_UNKNOWN:
        return nil;     
    case SVG_FECOLORMATRIX_TYPE_MATRIX:
    {
        CMValuesCheck(20, "matrix");
        filter = [CIFilter filterWithName:@"CIColorMatrix"];
        [filter setDefaults];
        const Vector<float>& v = values();
        [filter setValue:[CIVector vectorWithX:v[0] Y:v[1] Z:v[2] W:v[3]] forKey:@"inputRVector"];
        [filter setValue:[CIVector vectorWithX:v[5] Y:v[6] Z:v[7] W:v[8]] forKey:@"inputGVector"];
        [filter setValue:[CIVector vectorWithX:v[10] Y:v[11] Z:v[12] W:v[13]] forKey:@"inputBVector"];
        [filter setValue:[CIVector vectorWithX:v[15] Y:v[16] Z:v[17] W:v[18]] forKey:@"inputAVector"];
        [filter setValue:[CIVector vectorWithX:v[4] Y:v[9] Z:v[14] W:v[19]] forKey:@"inputBiasVector"];
        break;
    }
    case SVG_FECOLORMATRIX_TYPE_SATURATE:
    {
        CMValuesCheck(1, "saturate");
        filter = [CIFilter filterWithName:@"CIColorControls"];
        [filter setDefaults];
        float saturation = values()[0];
        if ((saturation < 0.0) || (saturation > 3.0))
            NSLog(@"WARNING: Saturation adjustment: %f outside supported range.");
        [filter setValue:[NSNumber numberWithFloat:saturation] forKey:@"inputSaturation"];
        break;
    }
    case SVG_FECOLORMATRIX_TYPE_HUEROTATE:
    {
        CMValuesCheck(1, "hueRotate");
        filter = [CIFilter filterWithName:@"CIHueAdjust"];
        [filter setDefaults];
        float radians = deg2rad(values()[0]);
        [filter setValue:[NSNumber numberWithFloat:radians] forKey:@"inputAngle"];
        break;
    }
    case SVG_FECOLORMATRIX_TYPE_LUMINANCETOALPHA:
    {
        CMValuesCheck(0, "luminanceToAlpha");
        // FIXME: I bet there is an easy filter to do this.
        filter = [CIFilter filterWithName:@"CIColorMatrix"];
        [filter setDefaults];
        CGFloat zero[4] = {0, 0, 0, 0};
        CGFloat alpha[4] = {0.2125f, 0.7154f, 0.0721f, 0};
        [filter setValue:[CIVector vectorWithValues:zero count:4] forKey:@"inputRVector"];
        [filter setValue:[CIVector vectorWithValues:zero count:4] forKey:@"inputGVector"];
        [filter setValue:[CIVector vectorWithValues:zero count:4] forKey:@"inputBVector"];
        [filter setValue:[CIVector vectorWithValues:alpha count:4] forKey:@"inputAVector"];
        [filter setValue:[CIVector vectorWithValues:zero count:4] forKey:@"inputBiasVector"];
        break;
    }
    default:
    LOG_ERROR("Unhandled ColorMatrix type: %i", type());
    return nil;
    }
    CIImage *inputImage = filterPlatformData->inputImage(this);
    FE_QUARTZ_CHECK_INPUT(inputImage);
    [filter setValue:inputImage forKey:@"inputImage"];

    FE_QUARTZ_MAP_TO_SUBREGION(bbox);
    FE_QUARTZ_OUTPUT_RETURN;
}

}

#endif // ENABLE(SVG) && ENABLE(SVG_FILTERS)
