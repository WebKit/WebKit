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
#import "SourceGraphicCoreImageApplier.h"

#if USE(CORE_IMAGE)

#import "FilterImage.h"
#import "PlatformImageBuffer.h"
#import <CoreImage/CIContext.h>
#import <CoreImage/CIFilter.h>
#import <CoreImage/CoreImage.h>

namespace WebCore {

bool SourceGraphicCoreImageApplier::apply(const Filter&, const FilterImageVector& inputs, FilterImage& result) const
{
    auto& input = inputs[0].get();

    auto sourceImage = input.imageBuffer();
    if (!sourceImage)
        return false;

    RetainPtr<CIImage> image;
    if (is<IOSurfaceImageBuffer>(*sourceImage))
        image = [CIImage imageWithIOSurface:downcast<IOSurfaceImageBuffer>(*sourceImage).surface().surface()];
    else
        image = [CIImage imageWithCGImage:sourceImage->copyNativeImage()->platformImage().get()];

    if (!image)
        return false;

    result.setCIImage(WTFMove(image));
    return true;
}

} // namespace WebCore

#endif // USE(CORE_IMAGE)
