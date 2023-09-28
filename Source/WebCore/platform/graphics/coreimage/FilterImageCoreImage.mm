/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#import "IOSurfaceImageBuffer.h"
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

void FilterImage::setCIImage(RetainPtr<CIImage>&& ciImage)
{
    ASSERT(ciImage);
    m_ciImage = WTFMove(ciImage);
}

size_t FilterImage::memoryCostOfCIImage() const
{
    ASSERT(m_ciImage);
    return FloatSize([m_ciImage.get() extent].size).area() * 4;
}

ImageBuffer* FilterImage::imageBufferFromCIImage()
{
    ASSERT(m_ciImage);

    if (m_imageBuffer)
        return m_imageBuffer.get();

    m_imageBuffer = IOSurfaceImageBuffer::create(m_absoluteImageRect.size(), 1, m_colorSpace, PixelFormat::BGRA8, RenderingPurpose::Unspecified);
    if (!m_imageBuffer)
        return nullptr;

    auto destRect = FloatRect { FloatPoint(), m_absoluteImageRect.size() };
    [sharedCIContext().get() render: m_ciImage.get() toIOSurface: downcast<IOSurfaceImageBuffer>(*m_imageBuffer).surface().surface() bounds:destRect colorSpace:m_colorSpace.platformColorSpace()];

    return m_imageBuffer.get();
}

} // namespace WebCore

#endif // USE(CORE_IMAGE)
