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

#import "ColorSpaceCG.h"
#import "DestinationColorSpace.h"
#import "Filter.h"
#import "Logging.h"
#import <CoreImage/CIFilterBuiltins.h>
#import <CoreImage/CoreImage.h>
#import <wtf/MathExtras.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/SystemTracing.h>
#import <wtf/cocoa/TypeCastsCocoa.h>

namespace WebCore {

static RetainPtr<CIContext> sharedLinearSRGBCIContext()
{
    static NeverDestroyed<RetainPtr<CIContext>> ciContext = [CIContext contextWithOptions:@{ kCIContextWorkingColorSpace:(__bridge id)linearSRGBColorSpaceSingleton() }];
    return ciContext;
}

static RetainPtr<CIContext> sharedSRGBCIContext()
{
    static NeverDestroyed<RetainPtr<CIContext>> ciContext = [CIContext contextWithOptions:@{ kCIContextWorkingColorSpace: (__bridge id)sRGBColorSpaceSingleton() }];
    return ciContext;
}

void FilterImage::setCIImage(RetainPtr<CIImage>&& ciImage)
{
    ASSERT(ciImage);
    m_ciImage = WTF::move(ciImage);
}

size_t FilterImage::memoryCostOfCIImage() const
{
    ASSERT(m_ciImage);
    return FloatSize([m_ciImage.get() extent].size).area() * 4;
}

ImageBuffer* FilterImage::filterResultImageBuffer(const Filter& filter)
{
    TraceScope traceScope(CoreImageRenderStart, CoreImageRenderEnd);
    BEGIN_BLOCK_OBJC_EXCEPTIONS

    if (!m_ciImage)
        return imageBuffer();

    if (m_imageBuffer)
        return m_imageBuffer.get();

    RefPtr imageBuffer = m_allocator.createImageBuffer(m_absoluteImageRect.size(), m_colorSpace, RenderingMode::Accelerated);
    m_imageBuffer = imageBuffer;
    if (!imageBuffer)
        return nullptr;

    ASSERT(imageBuffer->surface());

    RetainPtr context = colorSpace() == DestinationColorSpace::LinearSRGB() ? sharedLinearSRGBCIContext() : sharedSRGBCIContext();

    // We use -[CIContext:startTaskToRender...] because it lets us specify `fromRect`, which provides the rect against which earlier
    // CIImages extents are computed (in flipped coordinates). -[CIContext render:...] uses the size of the IOSurface, which is only
    // the output size of the last filter operation.
    RetainPtr destination = adoptNS([[CIRenderDestination alloc] initWithIOSurface:(__bridge id)imageBuffer->surface()->surface()]);
    [destination setColorSpace:m_colorSpace.platformColorSpace()];

    RetainPtr image = m_ciImage;

    if (filter.isShowingDebugOverlay()) {
        RetainPtr stripesFilter = [CIFilter stripesGeneratorFilter];
        [stripesFilter setWidth:30];
        [stripesFilter setColor0:[CIColor clearColor]];
        [stripesFilter setColor1:[CIColor colorWithRed:1.f green:0.95f blue:0 alpha:0.35]];
        [stripesFilter setSharpness:0.9];
        RetainPtr rotatedStripes = [[stripesFilter outputImage] imageByApplyingTransform:CGAffineTransformMakeRotation(deg2rad(45.f))];

        image = [rotatedStripes imageByCompositingOverImage:image.get()];
    }

    auto absoluteFilterRegion = filter.absoluteEnclosingFilterRegion();
    auto sourceRect = FloatRect { { }, absoluteFilterRegion.size() };
    auto location = FloatPoint { m_absoluteImageRect.x() - absoluteFilterRegion.x(), absoluteFilterRegion.maxY() - m_absoluteImageRect.maxY() };
    RetainPtr task = [context startTaskToRender:image.get()
                                       fromRect:sourceRect
                                  toDestination:destination.get()
                                        atPoint:-location
                                          error:nil];
    if (task)
        [task waitUntilCompletedAndReturnError:nil];

    LOG_WITH_STREAM(Filters, stream << "FilterImage::filterResultImageBuffer - output rect " << m_absoluteImageRect << " result " << ValueOrNull(m_imageBuffer.get()));
    return m_imageBuffer.get();

    END_BLOCK_OBJC_EXCEPTIONS
    return nullptr;
}

} // namespace WebCore

#endif // USE(CORE_IMAGE)
