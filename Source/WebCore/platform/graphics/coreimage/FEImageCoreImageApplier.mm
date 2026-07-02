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
#import "FEImageCoreImageApplier.h"

#if USE(CORE_IMAGE)

#import "FEImage.h"
#import "Filter.h"
#import "IOSurface.h"
#import "Logging.h"
#import <CoreImage/CoreImage.h>
#import <wtf/BlockObjCExceptions.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(FEImageCoreImageApplier);

FEImageCoreImageApplier::FEImageCoreImageApplier(const FEImage& effect)
    : Base(effect)
{
}

bool FEImageCoreImageApplier::apply(const Filter& filter, std::span<const Ref<FilterImage>>, FilterImage& result) const
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS

    auto& sourceImage = m_effect->sourceImage();
    auto primitiveSubregion = result.primitiveSubregion();

    RetainPtr<CIImage> image;
    if (RefPtr nativeImage = sourceImage.nativeImageIfExists()) {
        auto imageRect = primitiveSubregion;
        auto srcRect = m_effect->sourceImageRect();
        m_effect->preserveAspectRatio().transformRect(imageRect, srcRect);
        imageRect.scale(filter.filterScale());

        image = [CIImage imageWithCGImage:nativeImage->platformImage().get()];
        image = [image imageByCroppingToRect:srcRect];

        auto destRect = filter.flippedRectRelativeToAbsoluteEnclosingFilterRegion(imageRect);

        auto transform = makeMapBetweenRects(srcRect, destRect);
        image = [image imageByApplyingTransform:transform];

    } else if (RefPtr imageBuffer = sourceImage.imageBufferIfExists()) {
        if (auto* surface = imageBuffer->surface()) {
            RetainPtr ioSurface = surface->surface();
            image = [CIImage imageWithIOSurface:ioSurface.get()];
        }

        if (!image)
            return false;

        // FIXME: This geometry computation needs fixing per webkit.org/b/304911.
        auto sourceRect = m_effect->sourceImageRect();
        sourceRect.moveBy(primitiveSubregion.location());
        auto scaledSourceRect = filter.scaledByFilterScale(sourceRect);

        auto absoluteSourceRect = filter.flippedRectRelativeToAbsoluteEnclosingFilterRegion(scaledSourceRect);
        image = [image imageByApplyingTransform:CGAffineTransformMakeTranslation(absoluteSourceRect.x(), absoluteSourceRect.y())];

        auto resultRect = filter.flippedRectRelativeToAbsoluteEnclosingFilterRegion(result.absoluteImageRect());
        image = [image imageByCroppingToRect:resultRect];
    }

    if (!image)
        return false;

    result.setCIImage(WTF::move(image));
    return true;

    END_BLOCK_OBJC_EXCEPTIONS
    return false;
}

} // namespace WebCore

#endif // USE(CORE_IMAGE)
