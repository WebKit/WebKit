/*
 * Copyright (C) 2021-2022 Apple Inc. All rights reserved.
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
#import "FilterImage.h"

#if USE(CORE_IMAGE)

#import "PlatformImageBuffer.h"
#import <CoreImage/CIContext.h>
#import <CoreImage/CoreImage.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/cocoa/TypeCastsCocoa.h>

namespace WebCore {

static RetainPtr<CIContext> sharedCIContext()
{
    static NeverDestroyed<RetainPtr<CIContext>> ciContext = [CIContext contextWithOptions:@{ kCIContextWorkingColorSpace: bridge_id_cast(adoptCF(CGColorSpaceCreateWithName(kCGColorSpaceSRGB))).get() }];
    return ciContext;
}

RetainPtr<CIImage> FilterImage::ciImage() const
{
    if (std::holds_alternative<RetainPtr<CIImage>>(m_buffer))
        return ciImageSlot();
    
    if (auto imageBuffer = this->imageBuffer()) {
        RetainPtr<CIImage> ciImage;
        if (is<IOSurfaceImageBuffer>(*imageBuffer))
            ciImage = [CIImage imageWithIOSurface:downcast<IOSurfaceImageBuffer>(*imageBuffer).surface().surface()];
        else
            ciImage = [CIImage imageWithCGImage:imageBuffer->copyNativeImage()->platformImage().get()];
        if (ciImage)
            m_buffer = WTFMove(ciImage);
    }
    
    return ciImageSlot();
}

RetainPtr<CIImage> FilterImage::ciImageSlot() const
{
    if (!std::holds_alternative<RetainPtr<CIImage>>(m_buffer))
        return nullptr;

    return std::get<RetainPtr<CIImage>>(m_buffer);
}

void FilterImage::setCIImage(RetainPtr<CIImage>&& ciImage)
{
    ASSERT(ciImage);
    m_buffer = WTFMove(ciImage);
}

RefPtr<ImageBuffer> FilterImage::createImageBuffer(RetainPtr<CIImage>& ciImage) const
{
    auto imageBuffer = IOSurfaceImageBuffer::create(m_absoluteImageRect.size(), 1, m_colorSpace, PixelFormat::BGRA8, RenderingPurpose::Unspecified);
    if (!imageBuffer)
        return nullptr;

    auto destRect = FloatRect { FloatPoint(), m_absoluteImageRect.size() };
    [sharedCIContext().get() render: ciImage.get() toIOSurface: imageBuffer->surface().surface() bounds:destRect colorSpace:m_colorSpace.platformColorSpace()];

    return imageBuffer;
}

} // namespace WebCore

#endif // USE(CORE_IMAGE)
