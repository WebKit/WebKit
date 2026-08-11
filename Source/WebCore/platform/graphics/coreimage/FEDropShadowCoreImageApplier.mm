/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "FEDropShadowCoreImageApplier.h"

#if USE(CORE_IMAGE)

#import "ColorSpaceCG.h"
#import "FEDropShadow.h"
#import "Filter.h"
#import "Logging.h"
#import <CoreImage/CIFilterBuiltins.h>
#import <CoreImage/CoreImage.h>
#import <wtf/BlockObjCExceptions.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(FEDropShadowCoreImageApplier);

FEDropShadowCoreImageApplier::FEDropShadowCoreImageApplier(const FEDropShadow& effect)
    : Base(effect)
{
}

bool FEDropShadowCoreImageApplier::apply(const Filter& filter, std::span<const Ref<FilterImage>> inputs, FilterImage& result) const
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS

    ASSERT(inputs.size() == 1);
    Ref input = inputs[0].get();

    RetainPtr inputImage = input->ciImage();
    if (!inputImage)
        return false;

    auto stdDeviation = filter.resolvedSize({ m_effect->stdDeviationX(), m_effect->stdDeviationY() });
    auto blurRadius = filter.scaledByFilterScale(stdDeviation);

    auto offset = filter.resolvedSize({ m_effect->dx(), m_effect->dy() });
    auto absoluteOffset = filter.scaledByFilterScale(offset);

    auto shadowColor = m_effect->shadowColor().colorWithAlphaMultipliedBy(m_effect->shadowOpacity());
    auto [r, g, b, a] = shadowColor.toResolvedColorComponentsInColorSpace(m_effect->operatingColorSpace());

    RetainPtr colorSpace = m_effect->operatingColorSpace() == DestinationColorSpace::SRGB() ? sRGBColorSpaceSingleton() : linearSRGBColorSpaceSingleton();
    RetainPtr shadowCIColor = [CIColor colorWithRed:r green:g blue:b alpha:a colorSpace:colorSpace.get()];

    // Extract alpha.
    RetainPtr alphaFilter = [CIFilter filterWithName:@"CIColorMatrix"];
    [alphaFilter setValue:inputImage.get() forKey:kCIInputImageKey];

    [alphaFilter setValue:[CIVector vectorWithX:0 Y:0 Z:0 W:0] forKey:@"inputRVector"];
    [alphaFilter setValue:[CIVector vectorWithX:0 Y:0 Z:0 W:0] forKey:@"inputGVector"];
    [alphaFilter setValue:[CIVector vectorWithX:0 Y:0 Z:0 W:0] forKey:@"inputBVector"];
    [alphaFilter setValue:[CIVector vectorWithX:0 Y:0 Z:0 W:1] forKey:@"inputAVector"];
    [alphaFilter setValue:[CIVector vectorWithX:0 Y:0 Z:0 W:0] forKey:@"inputBiasVector"];

    auto blurFilter = [CIFilter filterWithName:@"CIGaussianBlurXY"];
    [blurFilter setValue:[alphaFilter outputImage] forKey:kCIInputImageKey];
    [blurFilter setValue:@(blurRadius.width()) forKey:@"inputSigmaX"];
    [blurFilter setValue:@(blurRadius.height()) forKey:@"inputSigmaY"];

    RetainPtr shadowImage = [[blurFilter outputImage] imageByApplyingTransform:CGAffineTransformMakeTranslation(absoluteOffset.width(), -absoluteOffset.height())];

    RetainPtr shadowColorImage = [CIImage imageWithColor:shadowCIColor.get()];
    RetainPtr compositeFilter = [CIFilter filterWithName:@"CISourceInCompositing"];
    [compositeFilter setValue:shadowColorImage.get() forKey:kCIInputImageKey];
    [compositeFilter setValue:shadowImage.get() forKey:kCIInputBackgroundImageKey];

    RetainPtr shadowedImage = [inputImage imageByCompositingOverImage:[compositeFilter outputImage]];
    auto cropRect = filter.flippedRectRelativeToAbsoluteEnclosingFilterRegion(result.absoluteImageRect());
    shadowedImage = [shadowedImage imageByCroppingToRect:cropRect];

    result.setCIImage(shadowedImage.get());
    return true;

    END_BLOCK_OBJC_EXCEPTIONS
    return false;
}

} // namespace WebCore

#endif // USE(CORE_IMAGE)
