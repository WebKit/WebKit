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
#import "FETileCoreImageApplier.h"

#if USE(CORE_IMAGE)

#import "AffineTransform.h"
#import "FETile.h"
#import "Filter.h"
#import "Logging.h"
#import <CoreImage/CoreImage.h>
#import <wtf/BlockObjCExceptions.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(FETileCoreImageApplier);

FETileCoreImageApplier::FETileCoreImageApplier(const FETile& effect)
    : Base(effect)
{
}

bool FETileCoreImageApplier::apply(const Filter& filter, std::span<const Ref<FilterImage>> inputs, FilterImage& result) const
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS

    ASSERT(inputs.size() == 1);
    Ref input = inputs[0].get();

    RetainPtr inputImage = input->ciImage();
    if (!inputImage)
        return false;

    auto tileRect = input->maxEffectRect(filter);
    tileRect.scale(filter.filterScale());
    tileRect = filter.flippedRectRelativeToAbsoluteEnclosingFilterRegion(tileRect);

    auto imageExtent = FloatRect { [inputImage extent] };
    RetainPtr<CIImage> tileImage;
    if (imageExtent.contains(tileRect))
        tileImage = [inputImage imageByCroppingToRect:tileRect];
    else {
        // Extend the image by compositing over the clear color.
        RetainPtr clearImage = [[CIImage imageWithColor:[CIColor clearColor]] imageByCroppingToRect:tileRect];
        tileImage = [[inputImage imageByCompositingOverImage:clearImage.get()] imageByCroppingToRect:tileRect];
    }

    RetainPtr tileFilter = [CIFilter filterWithName:@"CIAffineTile"];
    [tileFilter setValue:tileImage.get() forKey:kCIInputImageKey];
    // This identity transform is necessary, otherwise the tiling is half scale when filterScale is not one.
    [tileFilter setValue:[NSValue valueWithBytes:&CGAffineTransformIdentity objCType:@encode(CGAffineTransform)] forKey:kCIInputTransformKey];

    auto cropRect = filter.flippedRectRelativeToAbsoluteEnclosingFilterRegion(result.absoluteImageRect());
    RetainPtr image = [[tileFilter outputImage] imageByCroppingToRect:cropRect];

    result.setCIImage(WTF::move(image));
    return true;

    END_BLOCK_OBJC_EXCEPTIONS
    return false;
}

} // namespace WebCore

#endif // USE(CORE_IMAGE)
