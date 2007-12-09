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
#include "SVGFEComponentTransfer.h"
#include "SVGFEHelpersCg.h"

#import "WKComponentMergeFilter.h"
#import "WKIdentityTransferFilter.h"
#import "WKTableTransferFilter.h"
#import "WKDiscreteTransferFilter.h"
#import "WKLinearTransferFilter.h"
#import "WKGammaTransferFilter.h"

namespace WebCore {

static CIImage* genImageFromTable(const Vector<float>& table)
{
    int length = table.size();
    int nBytes = length * 4 * sizeof(float);
    float* tableStore = (float *) malloc(nBytes);
    NSData* bitmapData = [NSData dataWithBytesNoCopy:tableStore length:nBytes];
    for (Vector<float>::const_iterator it = table.begin(); it != table.end(); it++) {
        const float value = *it;
        *tableStore++ = value;
        *tableStore++ = value;
        *tableStore++ = value;
        *tableStore++ = value;
    }
    return [CIImage imageWithBitmapData:bitmapData bytesPerRow:nBytes size:CGSizeMake(length, 1) format:kCIFormatRGBAf colorSpace:nil];
}

static void setParametersForComponentFunc(CIFilter* filter, const SVGComponentTransferFunction& func, CIVector* channelSelector)
{
    switch (func.type) {
    case SVG_FECOMPONENTTRANSFER_TYPE_UNKNOWN:
        return;
    case SVG_FECOMPONENTTRANSFER_TYPE_TABLE:
        [filter setValue:genImageFromTable(func.tableValues) forKey:@"inputTable"];
        [filter setValue:channelSelector forKey:@"inputSelector"];
        break;
    case SVG_FECOMPONENTTRANSFER_TYPE_DISCRETE:
        [filter setValue:genImageFromTable(func.tableValues) forKey:@"inputTable"];
        [filter setValue:channelSelector forKey:@"inputSelector"];
        break;
    case SVG_FECOMPONENTTRANSFER_TYPE_LINEAR:
        [filter setValue:[NSNumber numberWithFloat:func.slope] forKey:@"inputSlope"];
        [filter setValue:[NSNumber numberWithFloat:func.intercept] forKey:@"inputIntercept"];
        break;
    case SVG_FECOMPONENTTRANSFER_TYPE_GAMMA:
        [filter setValue:[NSNumber numberWithFloat:func.amplitude] forKey:@"inputAmplitude"];
        [filter setValue:[NSNumber numberWithFloat:func.exponent] forKey:@"inputExponent"];
        [filter setValue:[NSNumber numberWithFloat:func.offset] forKey:@"inputOffset"];
        break;
    default:
        // identity has no args
        break;
    }
}

static CIFilter* filterForComponentFunc(const SVGComponentTransferFunction& func)
{
    CIFilter *filter;
    switch (func.type) {
    case SVG_FECOMPONENTTRANSFER_TYPE_UNKNOWN:
    case SVG_FECOMPONENTTRANSFER_TYPE_IDENTITY:
        filter = [CIFilter filterWithName:@"WKIdentityTransfer"];
        break;
    case SVG_FECOMPONENTTRANSFER_TYPE_TABLE:
        filter = [CIFilter filterWithName:@"WKTableTransferFilter"];
        break;
    case SVG_FECOMPONENTTRANSFER_TYPE_DISCRETE:
        filter = [CIFilter filterWithName:@"WKDiscreteTransferFilter"];
        break;
    case SVG_FECOMPONENTTRANSFER_TYPE_LINEAR:
        filter = [CIFilter filterWithName:@"WKLinearTransfer"];
        break;
    case SVG_FECOMPONENTTRANSFER_TYPE_GAMMA:
        filter = [CIFilter filterWithName:@"WKGammaTransfer"];
        break;
    default:
        NSLog(@"WARNING: Unknown function type for feComponentTransfer");
        //and to prevent the entire svg from failing as a result
        filter = [CIFilter filterWithName:@"WKIdentityTransfer"];
        break;
    }
    return filter;
}

static CIFilter* getFilterForFunc(const SVGComponentTransferFunction& func, CIImage* inputImage, CIVector* channelSelector)
{
    CIFilter* filter = filterForComponentFunc(func);
    [filter setDefaults];

    setParametersForComponentFunc(filter, func, channelSelector);
    [filter setValue:inputImage forKey:@"inputImage"];
    return filter;
}

CIFilter* SVGFEComponentTransfer::getFunctionFilter(SVGChannelSelectorType channel, CIImage* inputImage) const
{
    switch (channel) {
        case SVG_CHANNEL_R:
            return [getFilterForFunc(redFunction(), inputImage, getVectorForChannel(channel)) valueForKey:@"outputImage"];
        case SVG_CHANNEL_G:
            return [getFilterForFunc(greenFunction(), inputImage, getVectorForChannel(channel)) valueForKey:@"outputImage"];
        case SVG_CHANNEL_B:
            return [getFilterForFunc(blueFunction(), inputImage, getVectorForChannel(channel)) valueForKey:@"outputImage"];
        case SVG_CHANNEL_A:
            return [getFilterForFunc(alphaFunction(), inputImage, getVectorForChannel(channel)) valueForKey:@"outputImage"];
        default:
            return nil;
    }
}

CIFilter* SVGFEComponentTransfer::getCIFilter(const FloatRect& bbox) const
{
    [WKComponentMergeFilter class];
    [WKIdentityTransferFilter class];
    [WKTableTransferFilter class];
    [WKDiscreteTransferFilter class];
    [WKLinearTransferFilter class];
    [WKGammaTransferFilter class];

    SVGResourceFilter* svgFilter = filter();
    SVGResourceFilterPlatformDataMac* filterPlatformData = static_cast<SVGResourceFilterPlatformDataMac*>(svgFilter->platformData());
    CIFilter* filter = nil;
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    filter = [CIFilter filterWithName:@"WKComponentMerge"];
    if (!filter)
        return nil;
    [filter setDefaults];
    CIImage* inputImage = filterPlatformData->inputImage(this);
    FE_QUARTZ_CHECK_INPUT(inputImage);

    [filter setValue:getFunctionFilter(SVG_CHANNEL_R, inputImage) forKey:@"inputFuncR"];
    [filter setValue:getFunctionFilter(SVG_CHANNEL_G, inputImage) forKey:@"inputFuncG"];
    [filter setValue:getFunctionFilter(SVG_CHANNEL_B, inputImage) forKey:@"inputFuncB"];
    [filter setValue:getFunctionFilter(SVG_CHANNEL_A, inputImage) forKey:@"inputFuncA"];

    FE_QUARTZ_MAP_TO_SUBREGION(bbox);
    FE_QUARTZ_OUTPUT_RETURN;
}

}

#endif // ENABLE(SVG) && ENABLE(SVG_FILTERS)
