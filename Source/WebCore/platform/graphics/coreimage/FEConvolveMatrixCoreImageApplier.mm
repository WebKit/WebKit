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
#import "FEConvolveMatrixCoreImageApplier.h"

#if USE(CORE_IMAGE)

#import "FEConvolveMatrix.h"
#import "Filter.h"
#import "FilterImage.h"
#import <CoreImage/CIFilterBuiltins.h>
#import <CoreImage/CoreImage.h>
#import <simd/simd.h>
#import <wtf/MathExtras.h>
#import <wtf/TZoneMallocInlines.h>

namespace WebCore {

static CIKernel* convolveMatrixKernel()
{
    static NeverDestroyed<RetainPtr<CIKernel>> kernel;
    static std::once_flag onceFlag;

    std::call_once(onceFlag, [] {
        NSError *error = nil;
        NSArray<CIKernel *> *kernels = [CIKernel kernelsWithMetalString:@R"( /* NOLINT */
extern "C" {
namespace coreimage {

enum EdgeMode : uint8_t {
    EdgeModeDuplicate,
    EdgeModeWrap,
    EdgeModeNone,
};

struct ConvolveMatrixConstants {
    vector_int2 kernelSize;
    vector_int2 target;
    float divisor;
    float bias;
    EdgeMode edgeMode;
    bool preserveAlpha;
};

[[stitchable]] float4 convolve(sampler src,
    constant ConvolveMatrixConstants* constants,
    constant float* convolveKernel,
    destination dest)
{
    float2 srcPosition = src.transform(dest.coord());
    float4 srcPixel = src.sample(srcPosition);

    int2 kernelSize = constants->kernelSize;
    int2 target = constants->target;

    float4 pixelSum = 0;

    // FIXME: Edge mode not supported yet.
    for (int row = 0; row < kernelSize.y; ++row) {
        for (int col = 0; col < kernelSize.x; ++col) {

            int flippedRow = kernelSize.y - row - 1;
            int flippedCol = kernelSize.x - col - 1;

            float2 sampleOffset = { (float)flippedCol - target.x, (float)flippedRow - target.y };
            float2 samplePosition = srcPosition + sampleOffset;

            float4 nearbyPixel = src.sample(samplePosition);
            nearbyPixel *= convolveKernel[kernelSize.x * flippedRow + col];
            pixelSum += nearbyPixel;
        }
    }

    float alpha = constants->preserveAlpha ? srcPixel.a : clamp(pixelSum.a / constants->divisor + constants->bias, 0.f, 1.f);
    float4 resultPixel = pixelSum / constants->divisor + constants->bias * alpha;

    resultPixel.a = alpha;

    if (constants->preserveAlpha)
        resultPixel = premultiply(clamp(resultPixel, 0, 1));
    else
        resultPixel = premultiply(clamp(resultPixel, 0, alpha));

    return resultPixel;
}

} // namespace coreimage
} // extern "C"
        )" error:&error]; /* NOLINT */

        if (error || !kernels || !kernels.count) {
            LOG(Filters, "DisplacementMap kernel compilation failed: %@", error);
            return;
        }

        kernel.get() = kernels[0];
    });

    return kernel.get().get();
}

WTF_MAKE_TZONE_ALLOCATED_IMPL(FEConvolveMatrixCoreImageApplier);

FEConvolveMatrixCoreImageApplier::FEConvolveMatrixCoreImageApplier(const FEConvolveMatrix& effect)
    : Base(effect)
{
}

bool FEConvolveMatrixCoreImageApplier::supportsCoreImageRendering(const FEConvolveMatrix& effect)
{
    return effect.edgeMode() == EdgeModeType::None;
}

bool FEConvolveMatrixCoreImageApplier::apply(const Filter& filter, std::span<const Ref<FilterImage>> inputs, FilterImage& result) const
{
    ASSERT(inputs.size() == 1);
    Ref input = inputs[0].get();

    RetainPtr inputImage = input->ciImage();
    if (!inputImage)
        return false;

    RetainPtr kernel = convolveMatrixKernel();
    if (!kernel)
        return false;

    auto extent = filter.flippedRectRelativeToAbsoluteEnclosingFilterRegion(result.absoluteImageRect());

    enum class EdgeMode : uint8_t {
        Duplicate,
        Wrap,
        None,
    };
    struct ConvolveMatrixConstants {
        vector_int2 kernelSize;
        vector_int2 target;
        float divisor;
        float bias;
        EdgeMode edgeMode;
        bool preserveAlpha;
    };

    auto edgeMode = [](EdgeModeType type) {
        switch (type) {
        case EdgeModeType::Unknown:
            ASSERT_NOT_REACHED();
            return EdgeMode::None;
        case EdgeModeType::Duplicate: return EdgeMode::Duplicate;
        case EdgeModeType::Wrap: return EdgeMode::Wrap;
        case EdgeModeType::None: return EdgeMode::None;
        }
        return EdgeMode::None;
    };

    auto constants = ConvolveMatrixConstants {
        .kernelSize = { m_effect->kernelSize().width(), m_effect->kernelSize().height() },
        .target = { m_effect->targetOffset().x(), m_effect->kernelSize().height() - m_effect->targetOffset().y() - 1 }, // Flipped y coordinates.
        .divisor = m_effect->divisor(),
        .bias = m_effect->bias(),
        .edgeMode = edgeMode(m_effect->edgeMode()),
        .preserveAlpha = m_effect->preserveAlpha()
    };

    auto kernelValues = m_effect->kernel().map([](const auto& value) {
        return normalizedFloat(value);
    });

    // https://drafts.csswg.org/filter-effects/#element-attrdef-feconvolvematrix-preservealpha
    if (constants.preserveAlpha)
        inputImage = [inputImage imageByUnpremultiplyingAlpha];

    RetainPtr<NSArray> arguments = @[
        inputImage.get(),
        [NSData dataWithBytes:&constants length:sizeof(ConvolveMatrixConstants)],
        [NSData dataWithBytes:kernelValues.span().data() length:kernelValues.sizeInBytes()],
    ];

    RetainPtr<CIImage> outputImage = [kernel applyWithExtent:extent
        roiCallback:^CGRect(int, CGRect destRect) {
            return destRect;
        }
        arguments:arguments.get()];

    if (!outputImage)
        return false;

    outputImage = [outputImage imageByCroppingToRect:extent];
    result.setCIImage(WTF::move(outputImage));
    return true;
}

} // namespace WebCore

#endif // USE(CORE_IMAGE)
