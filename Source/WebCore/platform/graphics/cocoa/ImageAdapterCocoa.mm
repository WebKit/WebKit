/*
 * Copyright (C) 2004-2025 Apple Inc. All rights reserved.
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
#import "ImageAdapter.h"

#import "BitmapImage.h"
#import "FloatRect.h"
#import "GraphicsContext.h"
#import "NativeImageCocoa.h"
#import "SharedBuffer.h"
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#import <wtf/PointerComparison.h>
#import <wtf/cocoa/SpanCocoa.h>
#import <wtf/cocoa/TypeCastsCocoa.h>
#import <wtf/text/WTFString.h>

#if ENABLE(MULTI_REPRESENTATION_HEIC)
#import "Font.h"
#import "ImageBuffer.h"
#import "MultiRepresentationHEICMetrics.h"
#import "PlatformNSAdaptiveImageGlyph.h"
#import <CoreText/CoreText.h>
#endif

#if PLATFORM(IOS_FAMILY)
#import "UIFoundationSoftLink.h"
#import <CoreGraphics/CoreGraphics.h>
#import <ImageIO/ImageIO.h>
#import <MobileCoreServices/MobileCoreServices.h>
#endif

namespace WebCore {

#if ENABLE(MULTI_REPRESENTATION_HEIC)
NSAdaptiveImageGlyph *ImageAdapter::multiRepresentationHEIC()
{
    if (m_multiRepHEIC)
        return m_multiRepHEIC.get();

    RefPtr buffer = image().data();
    if (!buffer)
        return nullptr;

    Vector<uint8_t> data = buffer->copyData();

    RetainPtr nsData = WTF::toNSData(data.span());
    m_multiRepHEIC = adoptNS([[PlatformNSAdaptiveImageGlyph alloc] initWithImageContent:nsData.get()]);

    return m_multiRepHEIC.get();
}

ImageDrawResult ImageAdapter::drawMultiRepresentationHEIC(GraphicsContext& context, const Font& font, const FloatRect& destination, ImageDrawingOptions options)
{
    RetainPtr multiRepresentationHEIC = this->multiRepresentationHEIC();
    if (!multiRepresentationHEIC)
        return ImageDrawResult::DidNothing;

    RefPtr imageBuffer = context.createScaledImageBuffer(destination.size(), context.scaleFactor(), ColorSpace::SRGB(), RenderingMode::Unaccelerated, RenderingMethod::Local);
    if (!imageBuffer)
        return ImageDrawResult::DidNothing;

    RetainPtr cgContext = imageBuffer->context().platformContext();

    CGContextScaleCTM(cgContext, 1, -1);
    CGContextTranslateCTM(cgContext, 0, -destination.height());

    // FIXME (rdar://123044459): This needs to account for vertical writing modes.
    CGContextSetTextMatrix(cgContext, CGAffineTransformIdentity);
    CGContextSetTextPosition(cgContext, 0, font.metricsForMultiRepresentationHEIC().descent);

    CTFontDrawImageFromAdaptiveImageProviderAtPoint(protect(font.ctFont()), multiRepresentationHEIC.get(), CGContextGetTextPosition(cgContext), cgContext);

    auto orientation = options.orientation();
    if (orientation == ImageOrientation::Orientation::FromImage)
        orientation = image().orientation();

    context.drawImageBuffer(*imageBuffer, destination, { options, orientation });

    return ImageDrawResult::DidDraw;
}
#endif

void ImageAdapter::invalidate()
{
#if USE(APPKIT)
    m_nsImage = nullptr;
    m_nsImageKey = std::nullopt;
#endif
    m_tiffRep = nullptr;
    m_tiffRepKey = std::nullopt;
#if ENABLE(MULTI_REPRESENTATION_HEIC)
    m_multiRepHEIC = nullptr;
#endif
}

ImageAdapter::RasterizationKey::RasterizationKey(ConcreteObjectSize concreteObjectSize, const ImageDrawingExtras* extras)
    : concreteObjectSize(concreteObjectSize)
    , extras(extras ? extras->copy() : nullptr)
{
}

bool ImageAdapter::RasterizationKey::matches(ConcreteObjectSize otherConcreteObjectSize, const ImageDrawingExtras* otherExtras) const
{
    return concreteObjectSize == otherConcreteObjectSize
        && arePointingToEqualData<const ImageDrawingExtras*>(extras.get(), otherExtras);
}

CFDataRef ImageAdapter::tiffRepresentation()
{
    return tiffRepresentation(ConcreteObjectSize::fixed(protect(image())->size()));
}

CFDataRef ImageAdapter::tiffRepresentation(ConcreteObjectSize concreteObjectSize, const ImageDrawingExtras* extras)
{
    if (m_tiffRep && m_tiffRepKey && m_tiffRepKey->matches(concreteObjectSize, extras))
        return m_tiffRep.get();

    auto data = WebCore::tiffRepresentation(allNativeImages(concreteObjectSize, extras));
    if (!data)
        return nullptr;

    m_tiffRep = data;
    m_tiffRepKey.emplace(concreteObjectSize, extras);
    return m_tiffRep.get();
}

#if USE(APPKIT)
NSImage* ImageAdapter::nsImage()
{
    return nsImage(ConcreteObjectSize::fixed(protect(image())->size()));
}

NSImage* ImageAdapter::nsImage(ConcreteObjectSize concreteObjectSize, const ImageDrawingExtras* extras)
{
    if (m_nsImage && m_nsImageKey && m_nsImageKey->matches(concreteObjectSize, extras))
        return m_nsImage.get();

    RetainPtr data = tiffRepresentation(concreteObjectSize, extras);
    if (!data)
        return nullptr;

    m_nsImage = adoptNS([[NSImage alloc] initWithData:(__bridge NSData *)data.get()]);
    m_nsImageKey.emplace(concreteObjectSize, extras);
    return m_nsImage.get();
}

RetainPtr<NSImage> ImageAdapter::snapshotNSImage()
{
    return snapshotNSImage(ConcreteObjectSize::fixed(protect(image())->size()));
}

RetainPtr<NSImage> ImageAdapter::snapshotNSImage(ConcreteObjectSize concreteObjectSize, const ImageDrawingExtras* extras)
{
    RefPtr nativeImage = protect(image())->currentNativeImage(concreteObjectSize, extras);
    if (!nativeImage)
        return nullptr;

    auto data = WebCore::tiffRepresentation(*nativeImage);
    if (!data)
        return nullptr;

    return adoptNS([[NSImage alloc] initWithData:(__bridge NSData *)data.get()]);
}
#endif

} // namespace WebCore
