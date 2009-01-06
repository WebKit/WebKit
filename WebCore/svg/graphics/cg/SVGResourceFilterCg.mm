/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
 *           (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"

#if ENABLE(SVG) && ENABLE(SVG_FILTERS)
#include "SVGResourceFilter.h"

#include "TransformationMatrix.h"
#include "GraphicsContext.h"

#include "SVGResourceFilterPlatformDataMac.h"

#include <QuartzCore/CoreImage.h>

// Setting to a value > 0 allows to dump the output image as JPEG.
#define DEBUG_OUTPUT_IMAGE 0

namespace WebCore {

SVGResourceFilterPlatformData* SVGResourceFilter::createPlatformData()
{
    return new SVGResourceFilterPlatformDataMac(this);
}

void SVGResourceFilter::prepareFilter(GraphicsContext*& context, const FloatRect& bbox)
{
    if (bbox.isEmpty() || m_effects.isEmpty())
        return;

    SVGResourceFilterPlatformDataMac* platform = static_cast<SVGResourceFilterPlatformDataMac*>(platformData());
    
    CGContextRef cgContext = context->platformContext();

    // Use of CGBegin/EndTransparencyLayer around this call causes over release
    // of cgContext due to it being created on an autorelease pool, and released
    // after CGEndTransparencyLayer. Create local pool to fix.
    // <http://bugs.webkit.org/show_bug.cgi?id=8425>
    // <http://bugs.webkit.org/show_bug.cgi?id=6947>
    // <rdar://problem/4647735>
    NSAutoreleasePool* filterContextPool = [[NSAutoreleasePool alloc] init];
    platform->m_filterCIContext = [CIContext contextWithCGContext:cgContext options:nil];
    [filterContextPool drain];

    FloatRect filterRect = filterBBoxForItemBBox(bbox);

    // TODO: Ensure the size is not greater than the nearest <svg> size and/or the window size.
    // This is also needed for masking & gradients-on-stroke-of-text. File a bug on this.
    float width = filterRect.width();
    float height = filterRect.height();

    platform->m_filterCGLayer = [platform->m_filterCIContext.get() createCGLayerWithSize:CGSizeMake(width, height) info:NULL];

    context = new GraphicsContext(CGLayerGetContext(platform->m_filterCGLayer));
    context->save();

    context->translate(-filterRect.x(), -filterRect.y());
}

#ifndef NDEBUG
// Extremly helpful debugging utilities for any paint server / resource that creates
// internal image buffers (ie. gradients on text, masks, filters...)
void dumpCIOutputImage(CIImage* outputImage, NSString* fileName)
{
    CGSize extentSize = [outputImage extent].size;
    NSImage* image = [[[NSImage alloc] initWithSize:NSMakeSize(extentSize.width, extentSize.height)] autorelease];
    [image addRepresentation:[NSCIImageRep imageRepWithCIImage:outputImage]];

    NSData* imageData = [image TIFFRepresentation];
    NSBitmapImageRep* imageRep = [NSBitmapImageRep imageRepWithData:imageData];
    imageData = [imageRep representationUsingType:NSJPEGFileType properties:nil];

    [imageData writeToFile:fileName atomically:YES];
}

void dumpCGOutputImage(CGImage* outputImage, NSString* fileName)
{
    if (CIImage* ciOutputImage = [CIImage imageWithCGImage:outputImage])
        dumpCIOutputImage(ciOutputImage, fileName);
}
#endif

void SVGResourceFilter::applyFilter(GraphicsContext*& context, const FloatRect& bbox)
{
    if (bbox.isEmpty() || m_effects.isEmpty())
        return;

    SVGResourceFilterPlatformDataMac* platform = static_cast<SVGResourceFilterPlatformDataMac*>(platformData());
    
    // actually apply the filter effects
    CIImage* inputImage = [CIImage imageWithCGLayer:platform->m_filterCGLayer];
    NSArray* filterStack = platform->getCIFilterStack(inputImage, bbox);
    if ([filterStack count]) {
        CIImage* outputImage = [[filterStack lastObject] valueForKey:@"outputImage"];

        if (outputImage) {
#if DEBUG_OUTPUT_IMAGE > 0
            dumpOutputImage(outputImage);
#endif

            FloatRect filterRect = filterBBoxForItemBBox(bbox);
            FloatPoint destOrigin = filterRect.location();
            filterRect.setLocation(FloatPoint(0.0f, 0.0f));

            [platform->m_filterCIContext.get() drawImage:outputImage atPoint:CGPoint(destOrigin) fromRect:filterRect];
        }
    }

    CGLayerRelease(platform->m_filterCGLayer);
    platform->m_filterCGLayer = 0;

    platform->m_filterCIContext = 0;

    delete context;
    context = 0;
}

}

#endif // ENABLE(SVG) ENABLE(SVG_FILTERS)
