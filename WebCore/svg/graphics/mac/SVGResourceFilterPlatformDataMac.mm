/*
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
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

#include "SVGResourceFilterPlatformDataMac.h"
#include <QuartzCore/CoreImage.h>

namespace WebCore {

static const char* const SVGPreviousFilterOutputName = "__previousOutput__";

SVGResourceFilterPlatformDataMac::SVGResourceFilterPlatformDataMac(SVGResourceFilter* filter)
    : m_filterCIContext(0)
    , m_filterCGLayer(0)
    , m_imagesByName(AdoptNS, [[NSMutableDictionary alloc] init])
    , m_filter(filter)
{
}

SVGResourceFilterPlatformDataMac::~SVGResourceFilterPlatformDataMac()
{
    ASSERT(!m_filterCGLayer);
    ASSERT(!m_filterCIContext);
}

    
NSArray* SVGResourceFilterPlatformDataMac::getCIFilterStack(CIImage* inputImage, const FloatRect& bbox)
{
    NSMutableArray* filterEffects = [NSMutableArray array];
    
    setImageForName(inputImage, "SourceGraphic"); // input
    
    for (unsigned int i = 0; i < m_filter->effects().size(); i++) {
        CIFilter* filter = m_filter->effects()[i]->getCIFilter(bbox);
        if (filter)
            [filterEffects addObject:filter];
    }
    
    [m_imagesByName.get() removeAllObjects]; // clean up before next time.
    
    return filterEffects;
}
    
static inline CIImage* alphaImageForImage(CIImage* image)
{
    CIFilter* onlyAlpha = [CIFilter filterWithName:@"CIColorMatrix"];
    CGFloat zero[4] = {0, 0, 0, 0};
    [onlyAlpha setDefaults];
    [onlyAlpha setValue:image forKey:@"inputImage"];
    [onlyAlpha setValue:[CIVector vectorWithValues:zero count:4] forKey:@"inputRVector"];
    [onlyAlpha setValue:[CIVector vectorWithValues:zero count:4] forKey:@"inputGVector"];
    [onlyAlpha setValue:[CIVector vectorWithValues:zero count:4] forKey:@"inputBVector"];
    return [onlyAlpha valueForKey:@"outputImage"];
}

CIImage* SVGResourceFilterPlatformDataMac::imageForName(const String& name) const
{
    return [m_imagesByName.get() objectForKey:name];
}

void SVGResourceFilterPlatformDataMac::setImageForName(CIImage* image, const String& name)
{
    [m_imagesByName.get() setValue:image forKey:name];
}

void SVGResourceFilterPlatformDataMac::setOutputImage(const SVGFilterEffect* filterEffect, CIImage* output)
{
    if (!filterEffect->result().isEmpty())
        setImageForName(output, filterEffect->result());
    
    setImageForName(output, SVGPreviousFilterOutputName);
}

CIImage* SVGResourceFilterPlatformDataMac::inputImage(const SVGFilterEffect* filterEffect)
{
    if (filterEffect->in().isEmpty()) {
        CIImage* inImage = imageForName(SVGPreviousFilterOutputName);
        
        if (!inImage)
            inImage = imageForName("SourceGraphic");
        
        return inImage;
    } else if (filterEffect->in() == "SourceAlpha") {
        CIImage* sourceAlpha = imageForName(filterEffect->in());
        
        if (!sourceAlpha) {
            CIImage* sourceGraphic = imageForName("SourceGraphic");
            
            if (!sourceGraphic)
                return nil;
            
            sourceAlpha = alphaImageForImage(sourceGraphic);
            setImageForName(sourceAlpha, "SourceAlpha");
        }
        
        return sourceAlpha;
    }
    
    return imageForName(filterEffect->in());
}
    

}

#endif // #if ENABLE(SVG) && ENABLE(SVG_FILTERS)
