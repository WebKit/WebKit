/*
 * Copyright (C) 2005, 2006 Alexander Kellett <lypanov@kde.org>
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
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

#if ENABLE(SVG)
#import "SVGResourceMasker.h"

#import "BlockExceptions.h"
#import "CgSupport.h"
#import "GraphicsContext.h"
#import "ImageBuffer.h"
#import "SVGMaskElement.h"
#import "SVGRenderSupport.h"
#import "SVGRenderStyle.h"
#import "SVGResourceFilter.h"
#import <QuartzCore/CIFilter.h>
#import <QuartzCore/CoreImage.h>

using namespace std;

namespace WebCore {

static CIImage* applyLuminanceToAlphaFilter(CIImage* inputImage)
{
    CIFilter* luminanceToAlpha = [CIFilter filterWithName:@"CIColorMatrix"];
    [luminanceToAlpha setDefaults];
    CGFloat alpha[4] = {0.2125f, 0.7154f, 0.0721f, 0.0f};
    CGFloat zero[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    [luminanceToAlpha setValue:inputImage forKey:@"inputImage"];  
    [luminanceToAlpha setValue:[CIVector vectorWithValues:zero count:4] forKey:@"inputRVector"];
    [luminanceToAlpha setValue:[CIVector vectorWithValues:zero count:4] forKey:@"inputGVector"];
    [luminanceToAlpha setValue:[CIVector vectorWithValues:zero count:4] forKey:@"inputBVector"];
    [luminanceToAlpha setValue:[CIVector vectorWithValues:alpha count:4] forKey:@"inputAVector"];
    [luminanceToAlpha setValue:[CIVector vectorWithValues:zero count:4] forKey:@"inputBiasVector"];
    return [luminanceToAlpha valueForKey:@"outputImage"];
}

static CIImage* applyExpandAlphatoGrayscaleFilter(CIImage* inputImage)
{
    CIFilter* alphaToGrayscale = [CIFilter filterWithName:@"CIColorMatrix"];
    CGFloat zero[4] = {0, 0, 0, 0};
    [alphaToGrayscale setDefaults];
    [alphaToGrayscale setValue:inputImage forKey:@"inputImage"];
    [alphaToGrayscale setValue:[CIVector vectorWithValues:zero count:4] forKey:@"inputRVector"];
    [alphaToGrayscale setValue:[CIVector vectorWithValues:zero count:4] forKey:@"inputGVector"];
    [alphaToGrayscale setValue:[CIVector vectorWithValues:zero count:4] forKey:@"inputBVector"];
    [alphaToGrayscale setValue:[CIVector vectorWithX:0.0f Y:0.0f Z:0.0f W:1.0f] forKey:@"inputAVector"];
    [alphaToGrayscale setValue:[CIVector vectorWithX:1.0f Y:1.0f Z:1.0f W:0.0f] forKey:@"inputBiasVector"];
    return [alphaToGrayscale valueForKey:@"outputImage"];
}

static CIImage* transformImageIntoGrayscaleMask(CIImage* inputImage)
{
    CIFilter* blackBackground = [CIFilter filterWithName:@"CIConstantColorGenerator"];
    [blackBackground setValue:[CIColor colorWithRed:0.0f green:0.0f blue:0.0f alpha:1.0f] forKey:@"inputColor"];

    CIFilter* layerOverBlack = [CIFilter filterWithName:@"CISourceOverCompositing"];
    [layerOverBlack setValue:[blackBackground valueForKey:@"outputImage"] forKey:@"inputBackgroundImage"];  
    [layerOverBlack setValue:inputImage forKey:@"inputImage"];  

    CIImage* luminanceAlpha = applyLuminanceToAlphaFilter([layerOverBlack valueForKey:@"outputImage"]);
    CIImage* luminanceAsGrayscale = applyExpandAlphatoGrayscaleFilter(luminanceAlpha);
    CIImage* alphaAsGrayscale = applyExpandAlphatoGrayscaleFilter(inputImage);

    CIFilter* multipliedGrayscale = [CIFilter filterWithName:@"CIMultiplyCompositing"];
    [multipliedGrayscale setValue:luminanceAsGrayscale forKey:@"inputBackgroundImage"];  
    [multipliedGrayscale setValue:alphaAsGrayscale forKey:@"inputImage"];  
    return [multipliedGrayscale valueForKey:@"outputImage"];
}

void SVGResourceMasker::applyMask(GraphicsContext* context, const FloatRect& boundingBox)
{
    if (!m_mask)
        m_mask.set(m_ownerElement->drawMaskerContent(boundingBox, m_maskRect).release());
    if (!m_mask)
        return;
    
    IntSize maskSize(static_cast<int>(m_maskRect.width()), static_cast<int>(m_maskRect.height()));
    clampImageBufferSizeToViewport(m_ownerElement->document()->renderer(), maskSize);

    // Create new graphics context in gray scale mode for image rendering
    auto_ptr<ImageBuffer> grayScaleImage(ImageBuffer::create(maskSize, true));
    if (!grayScaleImage.get())
        return;
    
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    CGContextRef grayScaleContext = grayScaleImage->context()->platformContext();
    CIContext* ciGrayscaleContext = [CIContext contextWithCGContext:grayScaleContext options:nil];

    // Transform colorized mask to gray scale
    CIImage* colorMask = [CIImage imageWithCGImage:m_mask->cgImage()];
    if (!colorMask)
        return;
    CIImage* grayScaleMask = transformImageIntoGrayscaleMask(colorMask);
    [ciGrayscaleContext drawImage:grayScaleMask atPoint:CGPointZero fromRect:CGRectMake(0, 0, maskSize.width(), maskSize.height())];

    CGContextClipToMask(context->platformContext(), m_maskRect, grayScaleImage->cgImage());
    END_BLOCK_OBJC_EXCEPTIONS
}

} // namespace WebCore

#endif // ENABLE(SVG)
