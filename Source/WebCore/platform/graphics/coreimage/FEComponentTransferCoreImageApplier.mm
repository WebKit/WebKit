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
#import "Logging.h"
#import <CoreImage/CIContext.h>
#import <CoreImage/CIFilter.h>
#import <CoreImage/CoreImage.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(FEComponentTransferCoreImageApplier);

FEComponentTransferCoreImageApplier::FEComponentTransferCoreImageApplier(const FEComponentTransfer& effect)
    : Base(effect)
{
    // FIXME: Implement the rest of FEComponentTransfer functions
    ASSERT(supportsCoreImageRendering(effect));
}

template<ComponentTransferType... Types>
static bool isNullOr(const ComponentTransferFunction& function)
{
    if (function.type == ComponentTransferType::FECOMPONENTTRANSFER_TYPE_UNKNOWN)
        return true;
    return ((function.type == Types) || ...);
}

template<typename Predicate>
static bool allChannelsMatch(const FEComponentTransfer& effect, Predicate predicate)
{
    return predicate(effect.redFunction())
        && predicate(effect.greenFunction())
        && predicate(effect.blueFunction())
        && predicate(effect.alphaFunction());
}

#if HAVE(CI_KERNEL)
static CIKernel* gammaKernel()
{
    static NeverDestroyed<RetainPtr<CIKernel>> kernel;
    static std::once_flag onceFlag;

    std::call_once(onceFlag, [] {
        NSError *error = nil;
        NSArray<CIKernel *> *kernels = [CIKernel kernelsWithMetalString:@R"( /* NOLINT */
            [[stitchable]] float4 gammaFilter(coreimage::sampler src, float ampR, float expR, float offR,
                float ampG, float expG, float offG,
                float ampB, float expB, float offB) {
                    float4 pixel = src.sample(src.coord());

                    float3 color = pixel.a > 0.0 ? pixel.rgb / pixel.a : pixel.rgb;

                    pixel.r = ampR * pow(pixel.r, expR) + offR;
                    pixel.g = ampG * pow(pixel.g, expG) + offG;
                    pixel.b = ampB * pow(pixel.b, expB) + offB;

                    return pixel;
            }
        )" error:&error]; /* NOLINT */

        if (error || !kernels || !kernels.count) {
            LOG(Filters, "Gamma kernel compilation failed: %@", error);
            return;
        }

        kernel.get() = kernels[0];
    });

    return kernel.get().get();
}
#endif

bool FEComponentTransferCoreImageApplier::supportsCoreImageRendering(const FEComponentTransfer& effect)
{
#if HAVE(CI_KERNEL)
    return allChannelsMatch(effect, isNullOr<ComponentTransferType::FECOMPONENTTRANSFER_TYPE_LINEAR>)
        || allChannelsMatch(effect, isNullOr<ComponentTransferType::FECOMPONENTTRANSFER_TYPE_GAMMA>);
#else
    return allChannelsMatch(effect, isNullOr<ComponentTransferType::FECOMPONENTTRANSFER_TYPE_LINEAR>);
#endif
}

bool FEComponentTransferCoreImageApplier::apply(const Filter&, std::span<const Ref<FilterImage>> inputs, FilterImage& result) const
{
    ASSERT(inputs.size() == 1);
    auto& input = inputs[0].get();

    auto inputImage = input.ciImage();
    if (!inputImage)
        return false;

#if HAVE(CI_KERNEL)
    if (allChannelsMatch(m_effect, isNullOr<ComponentTransferType::FECOMPONENTTRANSFER_TYPE_GAMMA>))
        return applyGamma(inputImage, result);
#endif
    return applyLinear(inputImage, result);
}

bool FEComponentTransferCoreImageApplier::applyLinear(RetainPtr<CIImage> inputImage, FilterImage& result) const
{
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

#if HAVE(CI_KERNEL)
bool FEComponentTransferCoreImageApplier::applyGamma(RetainPtr<CIImage> inputImage, FilterImage& result) const
{
    RetainPtr kernel = gammaKernel();
    if (!kernel)
        return false;

    RetainPtr<NSArray> arguments = @[
        inputImage.get(),
        @(m_effect->redFunction().amplitude),   @(m_effect->redFunction().exponent),   @(m_effect->redFunction().offset),
        @(m_effect->greenFunction().amplitude), @(m_effect->greenFunction().exponent), @(m_effect->greenFunction().offset),
        @(m_effect->blueFunction().amplitude),  @(m_effect->blueFunction().exponent),  @(m_effect->blueFunction().offset)
    ];

    CIImage *outputImage = [kernel applyWithExtent:inputImage.get().extent
        roiCallback:^CGRect(int, CGRect destRect) {
            return destRect;
        }
        arguments:arguments.get()];

    result.setCIImage(outputImage);
    return true;
}
#endif

} // namespace WebCore

#endif // USE(CORE_IMAGE)
