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
#import "FEDisplacementMapCoreImageApplier.h"

#if USE(CORE_IMAGE)

#import "FEDisplacementMap.h"
#import "Filter.h"
#import "FilterImage.h"
#import "Logging.h"
#import <CoreImage/CoreImage.h>
#import <wtf/BlockObjCExceptions.h>
#import <wtf/TZoneMallocInlines.h>

namespace WebCore {

static CIKernel* displacementMapKernel()
{
    static NeverDestroyed<RetainPtr<CIKernel>> kernel;
    static std::once_flag onceFlag;

    std::call_once(onceFlag, [] {
        NSError *error = nil;
        // FIXME: Why not a CIColorKernel?
        NSArray<CIKernel *> *kernels = [CIKernel kernelsWithMetalString:@R"( /* NOLINT */
extern "C" {
namespace coreimage {

[[stitchable]] float4 displacement_map(sampler src, sampler map,
    float2 scale, float2 channelIndex, destination dest)
{
    unsigned xChannelIndex = (unsigned)channelIndex.x;
    unsigned yChannelIndex = (unsigned)channelIndex.y;

    float2 sc = dest.coord();
    float2 mapCoord = map.transform(sc);
    float4 mapPixel = map.sample(mapCoord);
    // According to spec (https://drafts.csswg.org/filter-effects/#feDisplacementMapElement) we should unpremultiply mapPixel,
    // but that doesn't match implementations: https://github.com/w3c/fxtf-drafts/issues/113

    float2 offset = {
        scale.x * (mapPixel[xChannelIndex] - 0.5),
        scale.y * (mapPixel[yChannelIndex] - 0.5)
    };

    float2 positionInInputTexture = sc + offset;
    float2 srcPosition = src.transform(positionInInputTexture);

    return src.sample(srcPosition);
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


WTF_MAKE_TZONE_ALLOCATED_IMPL(FEDisplacementMapCoreImageApplier);

FEDisplacementMapCoreImageApplier::FEDisplacementMapCoreImageApplier(const FEDisplacementMap& effect)
    : Base(effect)
{
}

bool FEDisplacementMapCoreImageApplier::apply(const Filter& filter, std::span<const Ref<FilterImage>> inputs, FilterImage& result) const
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS

    ASSERT(inputs.size() == 2);
    Ref input = inputs[0].get();
    Ref mapInput = inputs[1].get();

    RetainPtr inputImage = input->ciImage();
    if (!inputImage)
        return false;

    RetainPtr mapImage = mapInput->ciImage();
    if (!mapImage)
        return false;

    RetainPtr kernel = displacementMapKernel();
    if (!kernel)
        return false;

    auto extent = filter.flippedRectRelativeToAbsoluteEnclosingFilterRegion(result.absoluteImageRect());
    auto scale = filter.resolvedSize({ m_effect->scale(), m_effect->scale() });
    scale = filter.scaledByFilterScale(scale);

    int channelXIndex = clampTo(static_cast<int>(m_effect->xChannelSelector()) - 1, 0, 3);
    int channelYIndex = clampTo(static_cast<int>(m_effect->yChannelSelector()) - 1, 0, 3);

    RetainPtr<NSArray> arguments = @[
        inputImage.get(),
        mapImage.get(),
        [CIVector vectorWithX:scale.width() Y:-scale.height()], // Flip the y scale because CI coordinates are flipped.
        [CIVector vectorWithX:channelXIndex Y:channelYIndex],
    ];

    RetainPtr<CIImage> outputImage = [kernel applyWithExtent:extent
        roiCallback:^CGRect(int, CGRect destRect) {
            return destRect;
        }
        arguments:arguments.get()];


    outputImage = [outputImage imageByCroppingToRect:extent];
    result.setCIImage(WTF::move(outputImage));
    return true;

    END_BLOCK_OBJC_EXCEPTIONS
    return false;
}

} // namespace WebCore

#endif // USE(CORE_IMAGE)
