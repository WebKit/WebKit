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
#import "FECompositeCoreImageApplier.h"

#if USE(CORE_IMAGE)

#import "FEComposite.h"
#import "FilterImage.h"
#import <CoreImage/CoreImage.h>
#import <wtf/TZoneMallocInlines.h>

namespace WebCore {

static CIKernel* compositeArithmeticKernel()
{
    static NeverDestroyed<RetainPtr<CIKernel>> kernel;
    static std::once_flag onceFlag;

    std::call_once(onceFlag, [] {
        NSError *error = nil;
        // FIXME: Why not a CIColorKernel?
        NSArray<CIKernel *> *kernels = [CIKernel kernelsWithMetalString:@R"( /* NOLINT */
extern "C" {
namespace coreimage {

[[stitchable]] float4 composite_arithmetic(sampler front, sampler back,
    vector_float4 constants,
    destination dest)
{
    float2 destPosition = dest.coord();

    float2 frontPosition = front.transform(destPosition);
    float4 frontPixel = front.sample(frontPosition);

    float2 backPosition = back.transform(destPosition);
    float4 backPixel = back.sample(backPosition);

    float4 resultPixel = clamp(constants.x * frontPixel * backPixel + constants.y * frontPixel + constants.z * backPixel + constants.w, 0, 1);
    resultPixel = clamp(resultPixel, 0, resultPixel.a);
    return resultPixel;
}

} // namespace coreimage
} // extern "C"

        )" error:&error]; /* NOLINT */

        if (error || !kernels || !kernels.count) {
            LOG(Filters, "Composite Arithmetic kernel compilation failed: %@", error);
            return;
        }

        kernel.get() = kernels[0];
    });

    return kernel.get().get();
}

WTF_MAKE_TZONE_ALLOCATED_IMPL(FECompositeCoreImageApplier);

FECompositeCoreImageApplier::FECompositeCoreImageApplier(const FEComposite& effect)
    : Base(effect)
{
}

bool FECompositeCoreImageApplier::supportsCoreImageRendering(const FEComposite&)
{
    return true;
}

RetainPtr<CIImage> FECompositeCoreImageApplier::applyBuiltIn(RetainPtr<CIImage>&& frontImage, RetainPtr<CIImage>&& backImage, const FloatRect&) const
{
    RetainPtr<CIBlendKernel> kernel;
    switch (m_effect->operation()) {
    case CompositeOperationType::FECOMPOSITE_OPERATOR_UNKNOWN:
        return nil;
    case CompositeOperationType::FECOMPOSITE_OPERATOR_OVER:
        kernel = [CIBlendKernel sourceOver];
        break;
    case CompositeOperationType::FECOMPOSITE_OPERATOR_IN:
        kernel = [CIBlendKernel sourceIn];
        break;
    case CompositeOperationType::FECOMPOSITE_OPERATOR_OUT:
        kernel = [CIBlendKernel sourceOut];
        break;
    case CompositeOperationType::FECOMPOSITE_OPERATOR_ATOP:
        kernel = [CIBlendKernel sourceAtop];
        break;
    case CompositeOperationType::FECOMPOSITE_OPERATOR_XOR:
        kernel = [CIBlendKernel exclusiveOr];
        break;
    case CompositeOperationType::FECOMPOSITE_OPERATOR_ARITHMETIC:
        ASSERT_NOT_REACHED();
        break;
    case CompositeOperationType::FECOMPOSITE_OPERATOR_LIGHTER:
        kernel = [CIBlendKernel componentAdd];
        break;
    }

    return [kernel applyWithForeground:frontImage.get() background:backImage.get()];
}

RetainPtr<CIImage> FECompositeCoreImageApplier::applyArithmetic(RetainPtr<CIImage>&& frontImage, RetainPtr<CIImage>&& backImage, const FloatRect& extent) const
{
    RetainPtr kernel = compositeArithmeticKernel();
    if (!kernel)
        return nil;

    RetainPtr<NSArray> arguments = @[
        frontImage.get(),
        backImage.get(),
        [CIVector vectorWithX:m_effect->k1() Y:m_effect->k2() Z:m_effect->k3() W:m_effect->k4()]
    ];

    RetainPtr<CIImage> outputImage = [kernel applyWithExtent:extent
        roiCallback:^CGRect(int, CGRect destRect) {
            return destRect;
        }
        arguments:arguments.get()];

    return outputImage;
}

bool FECompositeCoreImageApplier::apply(const Filter& filter, std::span<const Ref<FilterImage>> inputs, FilterImage& result) const
{
    ASSERT(inputs.size() == 2);
    Ref input1 = inputs[0].get();
    Ref input2 = inputs[1].get();

    RetainPtr inputImage1 = input1->ciImage();
    if (!inputImage1)
        return false;

    RetainPtr inputImage2 = input2->ciImage();
    if (!inputImage2)
        return false;

    auto extent = filter.flippedRectRelativeToAbsoluteEnclosingFilterRegion(result.absoluteImageRect());

    RetainPtr<CIImage> resultImage;
    if (m_effect->operation() == CompositeOperationType::FECOMPOSITE_OPERATOR_ARITHMETIC)
        resultImage = applyArithmetic(WTF::move(inputImage1), WTF::move(inputImage2), extent);
    else
        resultImage = applyBuiltIn(WTF::move(inputImage1), WTF::move(inputImage2), extent);

    resultImage = [resultImage imageByCroppingToRect:extent];
    if (!resultImage)
        return false;

    result.setCIImage(WTF::move(resultImage));
    return true;
}

} // namespace WebCore

#endif // USE(CORE_IMAGE)
