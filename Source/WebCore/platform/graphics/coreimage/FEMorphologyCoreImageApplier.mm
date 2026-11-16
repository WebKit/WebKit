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
#import "FEMorphologyCoreImageApplier.h"

#if USE(CORE_IMAGE)

#import "FEMorphology.h"
#import "Filter.h"
#import "FilterImage.h"
#import "Logging.h"
#import <CoreImage/CoreImage.h>
#import <wtf/BlockObjCExceptions.h>
#import <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(FEMorphologyCoreImageApplier);

FEMorphologyCoreImageApplier::FEMorphologyCoreImageApplier(const FEMorphology& effect)
    : Base(effect)
{
}

bool FEMorphologyCoreImageApplier::apply(const Filter& filter, std::span<const Ref<FilterImage>> inputs, FilterImage& result) const
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS

    ASSERT(inputs.size() == 1);
    Ref input = inputs[0].get();

    RetainPtr inputImage = input->ciImage();
    if (!inputImage)
        return false;

    auto radius = filter.resolvedSize({ m_effect->radiusX(), m_effect->radiusY() });
    auto scaledRadius = filter.scaledByFilterScale(radius);

    float radiusX = scaledRadius.width();
    float radiusY = scaledRadius.height();

    if (radiusX <= 0 && radiusY <= 0) {
        result.setCIImage(inputImage.get());
        return true;
    }

    // Core Image expects kernel size (width/height), not radius
    // kernel size = 2 * radius + 1
    int kernelWidth = static_cast<int>(2 * radiusX + 1);
    int kernelHeight = static_cast<int>(2 * radiusY + 1);

    NSString *filterName = (m_effect->morphologyOperator() == MorphologyOperatorType::Erode)
        ? @"CIMorphologyRectangleMinimum"
        : @"CIMorphologyRectangleMaximum";

    RetainPtr morphologyFilter = [CIFilter filterWithName:filterName];
    if (!morphologyFilter) {
        LOG(Filters, "FEMorphologyCoreImageApplier: Failed to create filter %@", filterName);
        return false;
    }

    [morphologyFilter setValue:inputImage.get() forKey:kCIInputImageKey];
    [morphologyFilter setValue:@(kernelWidth) forKey:@"inputWidth"];
    [morphologyFilter setValue:@(kernelHeight) forKey:@"inputHeight"];

    result.setCIImage([morphologyFilter outputImage]);
    return true;

    END_BLOCK_OBJC_EXCEPTIONS
    return false;
}

} // namespace WebCore

#endif // USE(CORE_IMAGE)
