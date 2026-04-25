/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#import "FEBlendCoreImageApplier.h"

#if USE(CORE_IMAGE)

#import "ColorSpaceCG.h"
#import "FEBlend.h"
#import "Logging.h"
#import <CoreImage/CIFilterBuiltins.h>
#import <CoreImage/CoreImage.h>
#import <wtf/BlockObjCExceptions.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(FEBlendCoreImageApplier);

FEBlendCoreImageApplier::FEBlendCoreImageApplier(const FEBlend& effect)
    : Base(effect)
{
}

bool FEBlendCoreImageApplier::apply(const Filter&, std::span<const Ref<FilterImage>> inputs, FilterImage& result) const
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    ASSERT(inputs.size() == 2);
    Ref input1 = inputs[0];
    Ref input2 = inputs[1];

    auto inputImage1 = input1->ciImage();
    if (!inputImage1)
        return false;

    auto inputImage2 = input2->ciImage();
    if (!inputImage2)
        return false;

    RetainPtr<CIFilter> filter;
    switch (m_effect->blendMode()) {
    case BlendMode::Normal:
        filter = [CIFilter sourceOverCompositingFilter];
        break;
    case BlendMode::Multiply:
        filter = [CIFilter multiplyBlendModeFilter];
        break;
    case BlendMode::Screen:
        filter = [CIFilter screenBlendModeFilter];
        break;
    case BlendMode::Darken:
        filter = [CIFilter darkenBlendModeFilter];
        break;
    case BlendMode::Lighten:
        filter = [CIFilter lightenBlendModeFilter];
        break;
    case BlendMode::Overlay:
        filter = [CIFilter overlayBlendModeFilter];
        break;
    case BlendMode::ColorDodge:
        filter = [CIFilter colorDodgeBlendModeFilter];
        break;
    case BlendMode::ColorBurn:
        filter = [CIFilter colorBurnBlendModeFilter];
        break;
    case BlendMode::HardLight:
        filter = [CIFilter hardLightBlendModeFilter];
        break;
    case BlendMode::SoftLight:
        filter = [CIFilter softLightBlendModeFilter];
        break;
    case BlendMode::Difference:
        filter = [CIFilter differenceBlendModeFilter];
        break;
    case BlendMode::Exclusion:
        filter = [CIFilter exclusionBlendModeFilter];
        break;
    case BlendMode::Hue:
        filter = [CIFilter hueBlendModeFilter];
        break;
    case BlendMode::Saturation:
        filter = [CIFilter saturationBlendModeFilter];
        break;
    case BlendMode::Color:
        filter = [CIFilter colorBlendModeFilter];
        break;
    case BlendMode::Luminosity:
        filter = [CIFilter luminosityBlendModeFilter];
        break;
    case BlendMode::PlusDarker:
    case BlendMode::PlusLighter:
        // These are not supported by SVG.
        ASSERT_NOT_REACHED();
        break;
    }

    [filter setValue:inputImage1.get() forKey:kCIInputImageKey];
    [filter setValue:inputImage2.get() forKey:kCIInputBackgroundImageKey];

    result.setCIImage([filter outputImage]);
    return true;

    END_BLOCK_OBJC_EXCEPTIONS
    return false;
}

} // namespace WebCore

#endif // USE(CORE_IMAGE)
