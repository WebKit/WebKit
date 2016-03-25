/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2008, 2015 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Torch Mobile (Beijing) Co. Ltd. All rights reserved.
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

#include "config.h"
#include "ImageBuffer.h"

#include "BitmapImage.h"
#include "GraphicsContext.h"
#include "GraphicsContextCG.h"
#include "ImageData.h"
#include "IntRect.h"
#include "MIMETypeRegistry.h"
#include <math.h>
#include <CoreGraphics/CoreGraphics.h>
#include <ImageIO/ImageIO.h>
#include <wtf/Assertions.h>
#include <wtf/CheckedArithmetic.h>
#include <wtf/MainThread.h>
#include <wtf/RetainPtr.h>
#include <wtf/text/Base64.h>
#include <wtf/text/WTFString.h>

#if PLATFORM(COCOA)
#include "WebCoreSystemInterface.h"
#endif

#if USE(IOSURFACE_CANVAS_BACKING_STORE)
#include "IOSurface.h"
#include "IOSurfaceSPI.h"
#endif

// CA uses ARGB32 for textures and ARGB32 -> ARGB32 resampling is optimized.
#define USE_ARGB32 PLATFORM(IOS)

namespace WebCore {

static void releaseImageData(void*, const void* data, size_t)
{
    fastFree(const_cast<void*>(data));
}

static FloatSize scaleSizeToUserSpace(const FloatSize& logicalSize, const IntSize& backingStoreSize, const IntSize& internalSize)
{
    float xMagnification = static_cast<float>(backingStoreSize.width()) / internalSize.width();
    float yMagnification = static_cast<float>(backingStoreSize.height()) / internalSize.height();
    return FloatSize(logicalSize.width() * xMagnification, logicalSize.height() * yMagnification);
}

ImageBuffer::ImageBuffer(const FloatSize& size, float resolutionScale, ColorSpace imageColorSpace, RenderingMode renderingMode, bool& success)
    : m_logicalSize(size)
    , m_resolutionScale(resolutionScale)
{
    float scaledWidth = ceilf(resolutionScale * size.width());
    float scaledHeight = ceilf(resolutionScale * size.height());

    // FIXME: Should we automatically use a lower resolution?
    if (!FloatSize(scaledWidth, scaledHeight).isExpressibleAsIntSize())
        return;

    m_size = IntSize(scaledWidth, scaledHeight);
    m_data.backingStoreSize = m_size;

    success = false;  // Make early return mean failure.
    bool accelerateRendering = renderingMode == Accelerated;
    if (m_size.width() <= 0 || m_size.height() <= 0)
        return;

#if USE(IOSURFACE_CANVAS_BACKING_STORE)
    Checked<int, RecordOverflow> width = m_size.width();
    Checked<int, RecordOverflow> height = m_size.height();
#endif

    // Prevent integer overflows
    m_data.bytesPerRow = 4 * Checked<unsigned, RecordOverflow>(m_data.backingStoreSize.width());
    Checked<size_t, RecordOverflow> numBytes = Checked<unsigned, RecordOverflow>(m_data.backingStoreSize.height()) * m_data.bytesPerRow;
    if (numBytes.hasOverflowed())
        return;

#if USE(IOSURFACE_CANVAS_BACKING_STORE)
    IntSize maxSize = IOSurface::maximumSize();
    if (width.unsafeGet() > maxSize.width() || height.unsafeGet() > maxSize.height())
        accelerateRendering = false;
#else
    ASSERT(renderingMode == Unaccelerated);
#endif

    m_data.colorSpace = cachedCGColorSpace(imageColorSpace);

    RetainPtr<CGContextRef> cgContext;
    if (accelerateRendering) {
#if USE(IOSURFACE_CANVAS_BACKING_STORE)
        FloatSize userBounds = sizeForDestinationSize(FloatSize(width.unsafeGet(), height.unsafeGet()));
        m_data.surface = IOSurface::create(m_data.backingStoreSize, IntSize(userBounds), imageColorSpace);
        cgContext = m_data.surface->ensurePlatformContext();
        if (cgContext)
            CGContextClearRect(cgContext.get(), FloatRect(FloatPoint(), userBounds));
#endif

        if (!cgContext)
            accelerateRendering = false; // If allocation fails, fall back to non-accelerated path.
    }

    if (!accelerateRendering) {
        if (!tryFastCalloc(m_data.backingStoreSize.height(), m_data.bytesPerRow.unsafeGet()).getValue(m_data.data))
            return;
        ASSERT(!(reinterpret_cast<intptr_t>(m_data.data) & 3));

#if USE_ARGB32
        m_data.bitmapInfo = kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host;
#else
        m_data.bitmapInfo = kCGImageAlphaPremultipliedLast;
#endif
        cgContext = adoptCF(CGBitmapContextCreate(m_data.data, m_data.backingStoreSize.width(), m_data.backingStoreSize.height(), 8, m_data.bytesPerRow.unsafeGet(), m_data.colorSpace, m_data.bitmapInfo));
        // Create a live image that wraps the data.
        m_data.dataProvider = adoptCF(CGDataProviderCreateWithData(0, m_data.data, numBytes.unsafeGet(), releaseImageData));

        if (!cgContext)
            return;

        m_data.context = std::make_unique<GraphicsContext>(cgContext.get());
    }

    context().scale(FloatSize(1, -1));
    context().translate(0, -m_data.backingStoreSize.height());
    context().applyDeviceScaleFactor(m_resolutionScale);

    success = true;
}

ImageBuffer::~ImageBuffer()
{
}

FloatSize ImageBuffer::sizeForDestinationSize(FloatSize destinationSize) const
{
    return scaleSizeToUserSpace(destinationSize, m_data.backingStoreSize, internalSize());
}

GraphicsContext& ImageBuffer::context() const
{
#if USE(IOSURFACE_CANVAS_BACKING_STORE)
    if (m_data.surface)
        return m_data.surface->ensureGraphicsContext();
#endif
    return *m_data.context;
}

void ImageBuffer::flushContext() const
{
    CGContextFlush(context().platformContext());
}

static RetainPtr<CGImageRef> createCroppedImageIfNecessary(CGImageRef image, const IntSize& bounds)
{
    if (image && (CGImageGetWidth(image) != static_cast<size_t>(bounds.width())
        || CGImageGetHeight(image) != static_cast<size_t>(bounds.height()))) {
        return adoptCF(CGImageCreateWithImageInRect(image, CGRectMake(0, 0, bounds.width(), bounds.height())));
    }
    return image;
}

static RefPtr<Image> createBitmapImageAfterScalingIfNeeded(RetainPtr<CGImageRef>&& image, IntSize internalSize, IntSize logicalSize, IntSize backingStoreSize, float resolutionScale, ScaleBehavior scaleBehavior)
{
    if (resolutionScale == 1 || scaleBehavior == Unscaled)
        image = createCroppedImageIfNecessary(image.get(), internalSize);
    else {
        RetainPtr<CGContextRef> context = adoptCF(CGBitmapContextCreate(0, logicalSize.width(), logicalSize.height(), 8, 4 * logicalSize.width(), sRGBColorSpaceRef(), kCGImageAlphaPremultipliedLast));
        CGContextSetBlendMode(context.get(), kCGBlendModeCopy);
        CGContextClipToRect(context.get(), FloatRect(FloatPoint::zero(), logicalSize));
        FloatSize imageSizeInUserSpace = scaleSizeToUserSpace(logicalSize, backingStoreSize, internalSize);
        CGContextDrawImage(context.get(), FloatRect(FloatPoint::zero(), imageSizeInUserSpace), image.get());
        image = adoptCF(CGBitmapContextCreateImage(context.get()));
    }

    if (!image)
        return nullptr;

    return BitmapImage::create(WTFMove(image));
}

RefPtr<Image> ImageBuffer::copyImage(BackingStoreCopy copyBehavior, ScaleBehavior scaleBehavior) const
{
    RetainPtr<CGImageRef> image;
    if (m_resolutionScale == 1 || scaleBehavior == Unscaled)
        image = copyNativeImage(copyBehavior);
    else
        image = copyNativeImage(DontCopyBackingStore);

    return createBitmapImageAfterScalingIfNeeded(WTFMove(image), internalSize(), logicalSize(), m_data.backingStoreSize, m_resolutionScale, scaleBehavior);
}

RefPtr<Image> ImageBuffer::sinkIntoImage(std::unique_ptr<ImageBuffer> imageBuffer, ScaleBehavior scaleBehavior)
{
    IntSize internalSize = imageBuffer->internalSize();
    IntSize logicalSize = imageBuffer->logicalSize();
    IntSize backingStoreSize = imageBuffer->m_data.backingStoreSize;
    float resolutionScale = imageBuffer->m_resolutionScale;

    return createBitmapImageAfterScalingIfNeeded(sinkIntoNativeImage(WTFMove(imageBuffer)), internalSize, logicalSize, backingStoreSize, resolutionScale, scaleBehavior);
}

BackingStoreCopy ImageBuffer::fastCopyImageMode()
{
    return DontCopyBackingStore;
}

RetainPtr<CGImageRef> ImageBuffer::sinkIntoNativeImage(std::unique_ptr<ImageBuffer> imageBuffer)
{
#if USE(IOSURFACE_CANVAS_BACKING_STORE)
    if (!imageBuffer->m_data.surface)
        return imageBuffer->copyNativeImage(DontCopyBackingStore);

    return IOSurface::sinkIntoImage(IOSurface::createFromImageBuffer(WTFMove(imageBuffer)));
#else
    return imageBuffer->copyNativeImage(DontCopyBackingStore);
#endif
}

RetainPtr<CGImageRef> ImageBuffer::copyNativeImage(BackingStoreCopy copyBehavior) const
{
    RetainPtr<CGImageRef> image;
    if (!context().isAcceleratedContext()) {
        switch (copyBehavior) {
        case DontCopyBackingStore:
            image = adoptCF(CGImageCreate(m_data.backingStoreSize.width(), m_data.backingStoreSize.height(), 8, 32, m_data.bytesPerRow.unsafeGet(), m_data.colorSpace, m_data.bitmapInfo, m_data.dataProvider.get(), 0, true, kCGRenderingIntentDefault));
            break;
        case CopyBackingStore:
            image = adoptCF(CGBitmapContextCreateImage(context().platformContext()));
            break;
        default:
            ASSERT_NOT_REACHED();
            break;
        }
    }
#if USE(IOSURFACE_CANVAS_BACKING_STORE)
    else
        image = m_data.surface->createImage();
#endif

    return image;
}

void ImageBuffer::drawConsuming(std::unique_ptr<ImageBuffer> imageBuffer, GraphicsContext& destContext, const FloatRect& destRect, const FloatRect& srcRect, CompositeOperator op, BlendMode blendMode)
{
#if USE(IOSURFACE_CANVAS_BACKING_STORE)
    if (!imageBuffer->m_data.surface) {
        imageBuffer->draw(destContext, destRect, srcRect, op, blendMode);
        return;
    }
    
    ASSERT(destContext.isAcceleratedContext());
    
    float resolutionScale = imageBuffer->m_resolutionScale;
    IntSize backingStoreSize = imageBuffer->m_data.backingStoreSize;

    RetainPtr<CGImageRef> image = IOSurface::sinkIntoImage(IOSurface::createFromImageBuffer(WTFMove(imageBuffer)));
    
    FloatRect adjustedSrcRect = srcRect;
    adjustedSrcRect.scale(resolutionScale, resolutionScale);
    destContext.drawNativeImage(image.get(), backingStoreSize, destRect, adjustedSrcRect, op, blendMode);
#else
    imageBuffer->draw(destContext, destRect, srcRect, op, blendMode);
#endif
}

void ImageBuffer::draw(GraphicsContext& destContext, const FloatRect& destRect, const FloatRect& srcRect, CompositeOperator op, BlendMode blendMode)
{
    RetainPtr<CGImageRef> image;
    if (&destContext == &context() || destContext.isAcceleratedContext())
        image = copyNativeImage(CopyBackingStore); // Drawing into our own buffer, need to deep copy.
    else
        image = copyNativeImage(DontCopyBackingStore);

    FloatRect adjustedSrcRect = srcRect;
    adjustedSrcRect.scale(m_resolutionScale, m_resolutionScale);
    destContext.drawNativeImage(image.get(), m_data.backingStoreSize, destRect, adjustedSrcRect, op, blendMode);
}

void ImageBuffer::drawPattern(GraphicsContext& destContext, const FloatRect& srcRect, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing, CompositeOperator op, const FloatRect& destRect, BlendMode blendMode)
{
    FloatRect adjustedSrcRect = srcRect;
    adjustedSrcRect.scale(m_resolutionScale, m_resolutionScale);

    if (!context().isAcceleratedContext()) {
        if (&destContext == &context() || destContext.isAcceleratedContext()) {
            if (RefPtr<Image> copy = copyImage(CopyBackingStore)) // Drawing into our own buffer, need to deep copy.
                copy->drawPattern(destContext, adjustedSrcRect, patternTransform, phase, spacing, op, destRect, blendMode);
        } else {
            if (RefPtr<Image> imageForRendering = copyImage(DontCopyBackingStore))
                imageForRendering->drawPattern(destContext, adjustedSrcRect, patternTransform, phase, spacing, op, destRect, blendMode);
        }
    } else {
        if (RefPtr<Image> copy = copyImage(CopyBackingStore))
            copy->drawPattern(destContext, adjustedSrcRect, patternTransform, phase, spacing, op, destRect, blendMode);
    }
}

RefPtr<Uint8ClampedArray> ImageBuffer::getUnmultipliedImageData(const IntRect& rect, CoordinateSystem coordinateSystem) const
{
    if (context().isAcceleratedContext())
        flushContext();

    IntRect srcRect = rect;
    if (coordinateSystem == LogicalCoordinateSystem)
        srcRect.scale(m_resolutionScale);

    return m_data.getData(srcRect, internalSize(), context().isAcceleratedContext(), true, 1);
}

RefPtr<Uint8ClampedArray> ImageBuffer::getPremultipliedImageData(const IntRect& rect, CoordinateSystem coordinateSystem) const
{
    if (context().isAcceleratedContext())
        flushContext();

    IntRect srcRect = rect;
    if (coordinateSystem == LogicalCoordinateSystem)
        srcRect.scale(m_resolutionScale);

    return m_data.getData(srcRect, internalSize(), context().isAcceleratedContext(), false, 1);
}

void ImageBuffer::putByteArray(Multiply multiplied, Uint8ClampedArray* source, const IntSize& sourceSize, const IntRect& sourceRect, const IntPoint& destPoint, CoordinateSystem coordinateSystem)
{
    if (!context().isAcceleratedContext()) {
        IntRect scaledSourceRect = sourceRect;
        IntSize scaledSourceSize = sourceSize;
        if (coordinateSystem == LogicalCoordinateSystem) {
            scaledSourceRect.scale(m_resolutionScale);
            scaledSourceSize.scale(m_resolutionScale);
        }

        m_data.putData(source, scaledSourceSize, scaledSourceRect, destPoint, internalSize(), false, multiplied == Unmultiplied, 1);
        return;
    }

#if USE(IOSURFACE_CANVAS_BACKING_STORE)
    // Make a copy of the source to ensure the bits don't change before being drawn
    IntSize sourceCopySize(sourceRect.width(), sourceRect.height());
    // FIXME (149431): Should this ImageBuffer be unconditionally unaccelerated? Making it match the context seems to break putData().
    std::unique_ptr<ImageBuffer> sourceCopy = ImageBuffer::create(sourceCopySize, Unaccelerated, 1, ColorSpaceSRGB);
    if (!sourceCopy)
        return;

    sourceCopy->m_data.putData(source, sourceSize, sourceRect, IntPoint(-sourceRect.x(), -sourceRect.y()), sourceCopy->internalSize(), sourceCopy->context().isAcceleratedContext(), multiplied == Unmultiplied, 1);

    // Set up context for using drawImage as a direct bit copy
    CGContextRef destContext = context().platformContext();
    CGContextSaveGState(destContext);
    
    if (coordinateSystem == LogicalCoordinateSystem) {
        if (auto inverse = AffineTransform(getUserToBaseCTM(destContext)).inverse())
            CGContextConcatCTM(destContext, inverse.value());
    } else {
        if (auto inverse = AffineTransform(CGContextGetCTM(destContext)).inverse())
            CGContextConcatCTM(destContext, inverse.value());
    }
    CGContextResetClip(destContext);
    CGContextSetInterpolationQuality(destContext, kCGInterpolationNone);
    CGContextSetAlpha(destContext, 1.0);
    CGContextSetBlendMode(destContext, kCGBlendModeCopy);
    CGContextSetShadowWithColor(destContext, CGSizeZero, 0, 0);

    // Draw the image in CG coordinate space
    FloatSize scaledDestSize = sizeForDestinationSize(coordinateSystem == LogicalCoordinateSystem ? logicalSize() : internalSize());
    IntPoint destPointInCGCoords(destPoint.x() + sourceRect.x(), scaledDestSize.height() - (destPoint.y() + sourceRect.y()) - sourceRect.height());
    IntRect destRectInCGCoords(destPointInCGCoords, sourceCopySize);
    CGContextClipToRect(destContext, destRectInCGCoords);

    RetainPtr<CGImageRef> sourceCopyImage = sourceCopy->copyNativeImage();
    FloatRect backingStoreInDestRect = FloatRect(FloatPoint(destPointInCGCoords.x(), destPointInCGCoords.y() + sourceCopySize.height() - (int)CGImageGetHeight(sourceCopyImage.get())), FloatSize(CGImageGetWidth(sourceCopyImage.get()), CGImageGetHeight(sourceCopyImage.get())));
    CGContextDrawImage(destContext, backingStoreInDestRect, sourceCopyImage.get());
    CGContextRestoreGState(destContext);
#endif
}

static inline CFStringRef jpegUTI()
{
#if PLATFORM(IOS) || PLATFORM(WIN)
    static const CFStringRef kUTTypeJPEG = CFSTR("public.jpeg");
#endif
    return kUTTypeJPEG;
}
    
static RetainPtr<CFStringRef> utiFromMIMEType(const String& mimeType)
{
#if PLATFORM(MAC)
    return adoptCF(UTTypeCreatePreferredIdentifierForTag(kUTTagClassMIMEType, mimeType.createCFString().get(), 0));
#else
    ASSERT(isMainThread()); // It is unclear if CFSTR is threadsafe.

    // FIXME: Add Windows support for all the supported UTIs when a way to convert from MIMEType to UTI reliably is found.
    // For now, only support PNG, JPEG, and GIF. See <rdar://problem/6095286>.
    static const CFStringRef kUTTypePNG = CFSTR("public.png");
    static const CFStringRef kUTTypeGIF = CFSTR("com.compuserve.gif");

    if (equalLettersIgnoringASCIICase(mimeType, "image/png"))
        return kUTTypePNG;
    if (equalLettersIgnoringASCIICase(mimeType, "image/jpeg"))
        return jpegUTI();
    if (equalLettersIgnoringASCIICase(mimeType, "image/gif"))
        return kUTTypeGIF;

    ASSERT_NOT_REACHED();
    return kUTTypePNG;
#endif
}

static bool CGImageEncodeToData(CGImageRef image, CFStringRef uti, const double* quality, CFMutableDataRef data)
{
    if (!image || !uti || !data)
        return false;

    RetainPtr<CGImageDestinationRef> destination = adoptCF(CGImageDestinationCreateWithData(data, uti, 1, 0));
    if (!destination)
        return false;

    RetainPtr<CFDictionaryRef> imageProperties = 0;
    if (CFEqual(uti, jpegUTI()) && quality && *quality >= 0.0 && *quality <= 1.0) {
        // Apply the compression quality to the JPEG image destination.
        RetainPtr<CFNumberRef> compressionQuality = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberDoubleType, quality));
        const void* key = kCGImageDestinationLossyCompressionQuality;
        const void* value = compressionQuality.get();
        imageProperties = adoptCF(CFDictionaryCreate(0, &key, &value, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    }

    // Setting kCGImageDestinationBackgroundColor to black for JPEG images in imageProperties would save some math
    // in the calling functions, but it doesn't seem to work.

    CGImageDestinationAddImage(destination.get(), image, imageProperties.get());
    return CGImageDestinationFinalize(destination.get());
}

static String CGImageToDataURL(CGImageRef image, const String& mimeType, const double* quality)
{
    RetainPtr<CFStringRef> uti = utiFromMIMEType(mimeType);
    ASSERT(uti);

    RetainPtr<CFMutableDataRef> data = adoptCF(CFDataCreateMutable(kCFAllocatorDefault, 0));
    if (!CGImageEncodeToData(image, uti.get(), quality, data.get()))
        return "data:,";

    Vector<char> base64Data;
    base64Encode(CFDataGetBytePtr(data.get()), CFDataGetLength(data.get()), base64Data);

    return "data:" + mimeType + ";base64," + base64Data;
}

String ImageBuffer::toDataURL(const String& mimeType, const double* quality, CoordinateSystem) const
{
    ASSERT(MIMETypeRegistry::isSupportedImageMIMETypeForEncoding(mimeType));

    if (context().isAcceleratedContext())
        flushContext();

    RetainPtr<CFStringRef> uti = utiFromMIMEType(mimeType);
    ASSERT(uti);

    RefPtr<Uint8ClampedArray> premultipliedData;
    RetainPtr<CGImageRef> image;

    if (CFEqual(uti.get(), jpegUTI())) {
        // JPEGs don't have an alpha channel, so we have to manually composite on top of black.
        premultipliedData = getPremultipliedImageData(IntRect(IntPoint(0, 0), logicalSize()));
        if (!premultipliedData)
            return "data:,";

        RetainPtr<CGDataProviderRef> dataProvider;
        dataProvider = adoptCF(CGDataProviderCreateWithData(0, premultipliedData->data(), 4 * logicalSize().width() * logicalSize().height(), 0));
        if (!dataProvider)
            return "data:,";

        image = adoptCF(CGImageCreate(logicalSize().width(), logicalSize().height(), 8, 32, 4 * logicalSize().width(),
                                    sRGBColorSpaceRef(), kCGBitmapByteOrderDefault | kCGImageAlphaNoneSkipLast,
                                    dataProvider.get(), 0, false, kCGRenderingIntentDefault));
    } else if (m_resolutionScale == 1) {
        image = copyNativeImage(CopyBackingStore);
        image = createCroppedImageIfNecessary(image.get(), internalSize());
    } else {
        image = copyNativeImage(DontCopyBackingStore);
        RetainPtr<CGContextRef> context = adoptCF(CGBitmapContextCreate(0, logicalSize().width(), logicalSize().height(), 8, 4 * logicalSize().width(), sRGBColorSpaceRef(), kCGImageAlphaPremultipliedLast));
        CGContextSetBlendMode(context.get(), kCGBlendModeCopy);
        CGContextClipToRect(context.get(), CGRectMake(0, 0, logicalSize().width(), logicalSize().height()));
        FloatSize imageSizeInUserSpace = sizeForDestinationSize(logicalSize());
        CGContextDrawImage(context.get(), CGRectMake(0, 0, imageSizeInUserSpace.width(), imageSizeInUserSpace.height()), image.get());
        image = adoptCF(CGBitmapContextCreateImage(context.get()));
    }

    return CGImageToDataURL(image.get(), mimeType, quality);
}

String ImageDataToDataURL(const ImageData& source, const String& mimeType, const double* quality)
{
    ASSERT(MIMETypeRegistry::isSupportedImageMIMETypeForEncoding(mimeType));

    RetainPtr<CFStringRef> uti = utiFromMIMEType(mimeType);
    ASSERT(uti);

    CGImageAlphaInfo dataAlphaInfo = kCGImageAlphaLast;
    unsigned char* data = source.data()->data();
    Vector<uint8_t> premultipliedData;

    if (CFEqual(uti.get(), jpegUTI())) {
        // JPEGs don't have an alpha channel, so we have to manually composite on top of black.
        size_t size = 4 * source.width() * source.height();
        if (!premultipliedData.tryReserveCapacity(size))
            return "data:,";

        premultipliedData.resize(size);
        unsigned char *buffer = premultipliedData.data();
        for (size_t i = 0; i < size; i += 4) {
            unsigned alpha = data[i + 3];
            if (alpha != 255) {
                buffer[i + 0] = data[i + 0] * alpha / 255;
                buffer[i + 1] = data[i + 1] * alpha / 255;
                buffer[i + 2] = data[i + 2] * alpha / 255;
            } else {
                buffer[i + 0] = data[i + 0];
                buffer[i + 1] = data[i + 1];
                buffer[i + 2] = data[i + 2];
            }
        }

        dataAlphaInfo = kCGImageAlphaNoneSkipLast; // Ignore the alpha channel.
        data = premultipliedData.data();
    }

    RetainPtr<CGDataProviderRef> dataProvider;
    dataProvider = adoptCF(CGDataProviderCreateWithData(0, data, 4 * source.width() * source.height(), 0));
    if (!dataProvider)
        return "data:,";

    RetainPtr<CGImageRef> image;
    image = adoptCF(CGImageCreate(source.width(), source.height(), 8, 32, 4 * source.width(),
                                sRGBColorSpaceRef(), kCGBitmapByteOrderDefault | dataAlphaInfo,
                                dataProvider.get(), 0, false, kCGRenderingIntentDefault));

    return CGImageToDataURL(image.get(), mimeType, quality);
}

} // namespace WebCore
