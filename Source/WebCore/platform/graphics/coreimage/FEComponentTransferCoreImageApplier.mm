/*
 * Copyright (C) 2020-2021 Apple Inc. All rights reserved.
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
#import "FEComponentTransferCoreImageApplier.h"

#if USE(CORE_IMAGE)

#import "FEComponentTransfer.h"
#import <CoreImage/CIContext.h>
#import <CoreImage/CIFilter.h>
#import <CoreImage/CoreImage.h>
#import <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(FEComponentTransferCoreImageApplier);

FEComponentTransferCoreImageApplier::FEComponentTransferCoreImageApplier(const FEComponentTransfer& effect)
    : Base(effect)
{
    // FIXME: Implement the rest of FEComponentTransfer functions
    ASSERT(supportsCoreImageRendering(effect));
}

bool FEComponentTransferCoreImageApplier::supportsCoreImageRendering(const FEComponentTransfer& effect)
{
    auto isNullOrLinear = [] (const ComponentTransferFunction& function) {
        return function.type == ComponentTransferType::FECOMPONENTTRANSFER_TYPE_UNKNOWN
            || function.type == ComponentTransferType::FECOMPONENTTRANSFER_TYPE_LINEAR;
    };

    return isNullOrLinear(effect.redFunction())
        && isNullOrLinear(effect.greenFunction())
        && isNullOrLinear(effect.blueFunction())
        && isNullOrLinear(effect.alphaFunction());
}

bool FEComponentTransferCoreImageApplier::apply(const Filter&, std::span<const Ref<FilterImage>> inputs, FilterImage& result) const
{
    ASSERT(inputs.size() == 1);
    auto& input = inputs[0].get();

    auto inputImage = input.ciImage();
    if (!inputImage)
        return false;

    auto componentTransferFilter = [CIFilter filterWithName:@"CIColorPolynomial"];
    [componentTransferFilter setValue:inputImage.get() forKey:kCIInputImageKey];

    auto setCoefficients = [&] (NSString *key, const ComponentTransferFunction& function) {
        if (function.type == ComponentTransferType::FECOMPONENTTRANSFER_TYPE_LINEAR)
            [componentTransferFilter setValue:[CIVector vectorWithX:function.intercept Y:function.slope Z:0 W:0] forKey:key];
    };

    setCoefficients(@"inputRedCoefficients", m_effect->redFunction());
    setCoefficients(@"inputGreenCoefficients", m_effect->greenFunction());
    setCoefficients(@"inputBlueCoefficients", m_effect->blueFunction());
    setCoefficients(@"inputAlphaCoefficients", m_effect->alphaFunction());

    result.setCIImage(componentTransferFilter.outputImage);
    return true;
}

} // namespace WebCore

#endif // USE(CORE_IMAGE)
