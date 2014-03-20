/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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
#include <IOSurface/IOSurface.h>
#endif

// CA uses ARGB32 for textures and ARGB32 -> ARGB32 resampling is optimized.
#define USE_ARGB32 PLATFORM(IOS)

namespace WebCore {

#if USE(IOSURFACE_CANVAS_BACKING_STORE)
#if PLATFORM(IOS)
static const int maxIOSurfaceDimension = 2048;
#else
static const int maxIOSurfaceDimension = 4096;
#endif

// FIXME: Adopt WebCore::IOSurface.
static RetainPtr<IOSurfaceRef> createIOSurface(const IntSize& size)
{
    unsigned pixelFormat = 'BGRA';
    unsigned bytesPerElement = 4;
    int width = size.width();
    int height = size.height();

    unsigned long bytesPerRow = IOSurfaceAlignProperty(kIOSurfaceBytesPerRow, size.width() * bytesPerElement);
    if (!bytesPerRow)
        return 0;

    unsigned long allocSize = IOSurfaceAlignProperty(kIOSurfaceAllocSize, size.height() * bytesPerRow);
    if (!allocSize)
        return 0;

#if !PLATFORM(IOS)
    const void* keys[6];
    const void* values[6];
#else
    const void* keys[7];
    const void* values[7];
#endif
    keys[0] = kIOSurfaceWidth;
    values[0] = CFNumberCreate(0, kCFNumberIntType, &width);
    keys[1] = kIOSurfaceHeight;
    values[1] = CFNumberCreate(0, kCFNumberIntType, &height);
    keys[2] = kIOSurfacePixelFormat;
    values[2] = CFNumberCreate(0, kCFNumberIntType, &pixelFormat);
    keys[3] = kIOSurfaceBytesPerElement;
    values[3] = CFNumberCreate(0, kCFNumberIntType, &bytesPerElement);
    keys[4] = kIOSurfaceBytesPerRow;
    values[4] = CFNumberCreate(0, kCFNumberLongType, &bytesPerRow);
    keys[5] = kIOSurfaceAllocSize;
    values[5] = CFNumberCreate(0, kCFNumberLongType, &allocSize);
#if PLATFORM(IOS)
    keys[6] = kIOSurfaceCacheMode;
    int cacheMode = kIOMapWriteCombineCache;
    values[6] = CFNumberCreate(0, kCFNumberIntType, &cacheMode);
#endif

    RetainPtr<CFDictionaryRef> dict = adoptCF(CFDictionaryCreate(0, keys, values, WTF_ARRAY_LENGTH(values), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    for (unsigned i = 0; i < WTF_ARRAY_LENGTH(values); ++i)
        CFRelease(values[i]);

    return adoptCF(IOSurfaceCreate(dict.get()));
}
#endif

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

ImageBuffer::ImageBuffer(const IntSize& size, float resolutionScale, ColorSpace imageColorSpace, RenderingMode renderingMode, bool& success)
    : m_data(size) // NOTE: The input here isn't important as ImageBufferDataCG's constructor just ignores it.
    , m_logicalSize(size)
    , m_resolutionScale(resolutionScale)
{
    float scaledWidth = ceilf(resolutionScale * size.width());
    float scaledHeight = ceilf(resolutionScale * size.height());

    // FIXME: Should we automatically use a lower resolution?
    if (!FloatSize(scaledWidth, scaledHeight).isExpressibleAsIntSize())
        return;

    m_size = IntSize(scaledWidth, scaledHeight);
    m_data.m_backingStoreSize = m_size;

    success = false;  // Make early return mean failure.
    bool accelerateRendering = renderingMode == Accelerated;
    if (m_size.width() <= 0 || m_size.height() <= 0)
        return;

#if USE(IOSURFACE_CANVAS_BACKING_STORE)
    Checked<int, RecordOverflow> width = m_size.width();
    Checked<int, RecordOverflow> height = m_size.height();
#endif

    // Prevent integer overflows
    m_data.m_bytesPerRow = 4 * Checked<unsigned, RecordOverflow>(m_data.m_backingStoreSize.width());
    Checked<size_t, RecordOverflow> numBytes = Checked<unsigned, RecordOverflow>(m_data.m_backingStoreSize.height()) * m_data.m_bytesPerRow;
    if (numBytes.hasOverflowed())
        return;

#if USE(IOSURFACE_CANVAS_BACKING_STORE)
    if (width.unsafeGet() > maxIOSurfaceDimension || height.unsafeGet() > maxIOSurfaceDimension)
        accelerateRendering = false;
#else
    ASSERT(renderingMode == Unaccelerated);
#endif

    switch (imageColorSpace) {
    case ColorSpaceDeviceRGB:
        m_data.m_colorSpace = deviceRGBColorSpaceRef();
        break;
    case ColorSpaceSRGB:
        m_data.m_colorSpace = sRGBColorSpaceRef();
        break;
    case ColorSpaceLinearRGB:
        m_data.m_colorSpace = linearRGBColorSpaceRef();
        break;
    }

    RetainPtr<CGContextRef> cgContext;
    if (accelerateRendering) {
#if USE(IOSURFACE_CANVAS_BACKING_STORE)
        m_data.m_surface = createIOSurface(m_data.m_backingStoreSize);
        FloatSize userBounds = scaleSizeToUserSpace(FloatSize(width.unsafeGet(), height.unsafeGet()), m_data.m_backingStoreSize, m_size);
        cgContext = adoptCF(wkIOSurfaceContextCreate(m_data.m_surface.get(), userBounds.width(), userBounds.height(), m_data.m_colorSpace));
#endif
        if (!cgContext)
            accelerateRendering = false; // If allocation fails, fall back to non-accelerated path.
    }

    if (!accelerateRendering) {
        if (!tryFastCalloc(m_data.m_backingStoreSize.height(), m_data.m_bytesPerRow.unsafeGet()).getValue(m_data.m_data))
            return;
        ASSERT(!(reinterpret_cast<intptr_t>(m_data.m_data) & 3));

#if USE_ARGB32
        m_data.m_bitmapInfo = kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host;
#else
        m_data.m_bitmapInfo = kCGImageAlphaPremultipliedLast;
#endif
        cgContext = adoptCF(CGBitmapContextCreate(m_data.m_data, m_data.m_backingStoreSize.width(), m_data.m_backingStoreSize.height(), 8, m_data.m_bytesPerRow.unsafeGet(), m_data.m_colorSpace, m_data.m_bitmapInfo));
        // Create a live image that wraps the data.
        m_data.m_dataProvider = adoptCF(CGDataProviderCreateWithData(0, m_data.m_data, numBytes.unsafeGet(), releaseImageData));
    }

    if (!cgContext)
        return;

    m_context = adoptPtr(new GraphicsContext(cgContext.get()));
    m_context->scale(FloatSize(1, -1));
    m_context->translate(0, -m_data.m_backingStoreSize.height());
    m_context->applyDeviceScaleFactor(m_resolutionScale);
    m_context->setIsAcceleratedContext(accelerateRendering);
    success = true;
}

ImageBuffer::~ImageBuffer()
{
}

GraphicsContext* ImageBuffer::context() const
{
    return m_context.get();
}

void ImageBuffer::flushContext() const
{
    CGContextFlush(m_context->platformContext());
}

static RetainPtr<CGImageRef> createCroppedImageIfNecessary(CGImageRef image, const IntSize& bounds)
{
    if (image && (CGImageGetWidth(image) != static_cast<size_t>(bounds.width())
        || CGImageGetHeight(image) != static_cast<size_t>(bounds.height()))) {
        return adoptCF(CGImageCreateWithImageInRect(image, CGRectMake(0, 0, bounds.width(), bounds.height())));
    }
    return image;
}

PassRefPtr<Image> ImageBuffer::copyImage(BackingStoreCopy copyBehavior, ScaleBehavior scaleBehavior) const
{
    RetainPtr<CGImageRef> image;
    if (m_resolutionScale == 1 || scaleBehavior == Unscaled) {
        image = copyNativeImage(copyBehavior);
        image = createCroppedImageIfNecessary(image.get(), internalSize());
    } else {
        image = copyNativeImage(DontCopyBackingStore);
        RetainPtr<CGContextRef> context = adoptCF(CGBitmapContextCreate(0, logicalSize().width(), logicalSize().height(), 8, 4 * logicalSize().width(), deviceRGBColorSpaceRef(), kCGImageAlphaPremultipliedLast));
        CGContextSetBlendMode(context.get(), kCGBlendModeCopy);
        CGContextClipToRect(context.get(), FloatRect(FloatPoint::zero(), logicalSize()));
        FloatSize imageSizeInUserSpace = scaleSizeToUserSpace(logicalSize(), m_data.m_backingStoreSize, internalSize());
        CGContextDrawImage(context.get(), FloatRect(FloatPoint::zero(), imageSizeInUserSpace), image.get());
        image = adoptCF(CGBitmapContextCreateImage(context.get()));
    }

    if (!image)
        return nullptr;

    RefPtr<BitmapImage> bitmapImage = BitmapImage::create(image.get());
    bitmapImage->setSpaceSize(spaceSize());

    return bitmapImage.release();
}

BackingStoreCopy ImageBuffer::fastCopyImageMode()
{
    return DontCopyBackingStore;
}

RetainPtr<CGImageRef> ImageBuffer::copyNativeImage(BackingStoreCopy copyBehavior) const
{
    CGImageRef image = 0;
    if (!m_context->isAcceleratedContext()) {
        switch (copyBehavior) {
        case DontCopyBackingStore:
            image = CGImageCreate(m_data.m_backingStoreSize.width(), m_data.m_backingStoreSize.height(), 8, 32, m_data.m_bytesPerRow.unsafeGet(), m_data.m_colorSpace, m_data.m_bitmapInfo, m_data.m_dataProvider.get(), 0, true, kCGRenderingIntentDefault);
            break;
        case CopyBackingStore:
            image = CGBitmapContextCreateImage(context()->platformContext());
            break;
        default:
            ASSERT_NOT_REACHED();
            break;
        }
    }
#if USE(IOSURFACE_CANVAS_BACKING_STORE)
    else
        image = wkIOSurfaceContextCreateImage(context()->platformContext());
#endif

    return adoptCF(image);
}

void ImageBuffer::draw(GraphicsContext* destContext, ColorSpace styleColorSpace, const FloatRect& destRect, const FloatRect& srcRect, CompositeOperator op, BlendMode blendMode, bool useLowQualityScale)
{
    UNUSED_PARAM(useLowQualityScale);
    ColorSpace colorSpace = (destContext == m_context) ? ColorSpaceDeviceRGB : styleColorSpace;

    RetainPtr<CGImageRef> image;
    if (destContext == m_context || destContext->isAcceleratedContext())
        image = copyNativeImage(CopyBackingStore); // Drawing into our own buffer, need to deep copy.
    else
        image = copyNativeImage(DontCopyBackingStore);

    FloatRect adjustedSrcRect = srcRect;
    adjustedSrcRect.scale(m_resolutionScale, m_resolutionScale);
    destContext->drawNativeImage(image.get(), m_data.m_backingStoreSize, colorSpace, destRect, adjustedSrcRect, 1, op, blendMode);
}

void ImageBuffer::drawPattern(GraphicsContext* destContext, const FloatRect& srcRect, const AffineTransform& patternTransform, const FloatPoint& phase, ColorSpace styleColorSpace, CompositeOperator op, const FloatRect& destRect, BlendMode blendMode)
{
    FloatRect adjustedSrcRect = srcRect;
    adjustedSrcRect.scale(m_resolutionScale, m_resolutionScale);

    if (!m_context->isAcceleratedContext()) {
        if (destContext == m_context || destContext->isAcceleratedContext()) {
            RefPtr<Image> copy = copyImage(CopyBackingStore); // Drawing into our own buffer, need to deep copy.
            copy->drawPattern(destContext, adjustedSrcRect, patternTransform, phase, styleColorSpace, op, destRect, blendMode);
        } else {
            RefPtr<Image> imageForRendering = copyImage(DontCopyBackingStore);
            imageForRendering->drawPattern(destContext, adjustedSrcRect, patternTransform, phase, styleColorSpace, op, destRect, blendMode);
        }
    } else {
        RefPtr<Image> copy = copyImage(CopyBackingStore);
        copy->drawPattern(destContext, adjustedSrcRect, patternTransform, phase, styleColorSpace, op, destRect, blendMode);
    }
}

void ImageBuffer::clip(GraphicsContext* contextToClip, const FloatRect& rect) const
{
    FloatSize backingStoreSizeInUserSpace = scaleSizeToUserSpace(rect.size(), m_data.m_backingStoreSize, internalSize());

    CGContextRef platformContextToClip = contextToClip->platformContext();
    // FIXME: This image needs to be grayscale to be used as an alpha mask here.
    RetainPtr<CGImageRef> image = copyNativeImage(DontCopyBackingStore);
    CGContextTranslateCTM(platformContextToClip, rect.x(), rect.y() + backingStoreSizeInUserSpace.height());
    CGContextScaleCTM(platformContextToClip, 1, -1);
    CGContextClipToRect(platformContextToClip, FloatRect(FloatPoint(0, backingStoreSizeInUserSpace.height() - rect.height()), rect.size()));
    CGContextClipToMask(platformContextToClip, FloatRect(FloatPoint(), backingStoreSizeInUserSpace), image.get());
    CGContextScaleCTM(platformContextToClip, 1, -1);
    CGContextTranslateCTM(platformContextToClip, -rect.x(), -rect.y() - rect.height());
}

PassRefPtr<Uint8ClampedArray> ImageBuffer::getUnmultipliedImageData(const IntRect& rect, CoordinateSystem coordinateSystem) const
{
    if (m_context->isAcceleratedContext())
        flushContext();

    return m_data.getData(rect, internalSize(), m_context->isAcceleratedContext(), true, coordinateSystem == LogicalCoordinateSystem ? m_resolutionScale : 1);
}

PassRefPtr<Uint8ClampedArray> ImageBuffer::getPremultipliedImageData(const IntRect& rect, CoordinateSystem coordinateSystem) const
{
    if (m_context->isAcceleratedContext())
        flushContext();

    return m_data.getData(rect, internalSize(), m_context->isAcceleratedContext(), false, coordinateSystem == LogicalCoordinateSystem ? m_resolutionScale : 1);
}

void ImageBuffer::putByteArray(Multiply multiplied, Uint8ClampedArray* source, const IntSize& sourceSize, const IntRect& sourceRect, const IntPoint& destPoint, CoordinateSystem coordinateSystem)
{
    if (!m_context->isAcceleratedContext()) {
        m_data.putData(source, sourceSize, sourceRect, destPoint, internalSize(), m_context->isAcceleratedContext(), multiplied == Unmultiplied, coordinateSystem == LogicalCoordinateSystem ? m_resolutionScale : 1);
        return;
    }

#if USE(IOSURFACE_CANVAS_BACKING_STORE)
    // Make a copy of the source to ensure the bits don't change before being drawn
    IntSize sourceCopySize(sourceRect.width(), sourceRect.height());
    std::unique_ptr<ImageBuffer> sourceCopy = ImageBuffer::create(sourceCopySize, 1, ColorSpaceDeviceRGB, Unaccelerated);
    if (!sourceCopy)
        return;

    sourceCopy->m_data.putData(source, sourceSize, sourceRect, IntPoint(-sourceRect.x(), -sourceRect.y()), sourceCopy->internalSize(), sourceCopy->context()->isAcceleratedContext(), multiplied == Unmultiplied, 1);

    // Set up context for using drawImage as a direct bit copy
    CGContextRef destContext = context()->platformContext();
    CGContextSaveGState(destContext);
    if (coordinateSystem == LogicalCoordinateSystem)
        CGContextConcatCTM(destContext, AffineTransform(wkGetUserToBaseCTM(destContext)).inverse());
    else
        CGContextConcatCTM(destContext, AffineTransform(CGContextGetCTM(destContext)).inverse());
    wkCGContextResetClip(destContext);
    CGContextSetInterpolationQuality(destContext, kCGInterpolationNone);
    CGContextSetAlpha(destContext, 1.0);
    CGContextSetBlendMode(destContext, kCGBlendModeCopy);
    CGContextSetShadowWithColor(destContext, CGSizeZero, 0, 0);

    // Draw the image in CG coordinate space
    FloatSize scaledDestSize = scaleSizeToUserSpace(coordinateSystem == LogicalCoordinateSystem ? logicalSize() : internalSize(), m_data.m_backingStoreSize, internalSize());
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

    if (equalIgnoringCase(mimeType, "image/png"))
        return kUTTypePNG;
    if (equalIgnoringCase(mimeType, "image/jpeg"))
        return jpegUTI();
    if (equalIgnoringCase(mimeType, "image/gif"))
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

    if (m_context->isAcceleratedContext())
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
                                    deviceRGBColorSpaceRef(), kCGBitmapByteOrderDefault | kCGImageAlphaNoneSkipLast,
                                    dataProvider.get(), 0, false, kCGRenderingIntentDefault));
    } else if (m_resolutionScale == 1) {
        image = copyNativeImage(CopyBackingStore);
        image = createCroppedImageIfNecessary(image.get(), internalSize());
    } else {
        image = copyNativeImage(DontCopyBackingStore);
        RetainPtr<CGContextRef> context = adoptCF(CGBitmapContextCreate(0, logicalSize().width(), logicalSize().height(), 8, 4 * logicalSize().width(), deviceRGBColorSpaceRef(), kCGImageAlphaPremultipliedLast));
        CGContextSetBlendMode(context.get(), kCGBlendModeCopy);
        CGContextClipToRect(context.get(), CGRectMake(0, 0, logicalSize().width(), logicalSize().height()));
        FloatSize imageSizeInUserSpace = scaleSizeToUserSpace(logicalSize(), m_data.m_backingStoreSize, internalSize());
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
                                deviceRGBColorSpaceRef(), kCGBitmapByteOrderDefault | dataAlphaInfo,
                                dataProvider.get(), 0, false, kCGRenderingIntentDefault));

    return CGImageToDataURL(image.get(), mimeType, quality);
}

} // namespace WebCore
