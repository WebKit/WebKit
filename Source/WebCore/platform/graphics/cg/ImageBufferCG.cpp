/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2008-2017 Apple Inc. All rights reserved.
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

#if USE(CG)

#include "BitmapImage.h"
#include "GraphicsContext.h"
#include "GraphicsContextCG.h"
#include "ImageBufferUtilitiesCG.h"
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
#include "UTIUtilities.h"
#endif

#if USE(IOSURFACE_CANVAS_BACKING_STORE)
#include "IOSurface.h"
#include <pal/spi/cocoa/IOSurfaceSPI.h>
#endif

// CA uses ARGB32 for textures and ARGB32 -> ARGB32 resampling is optimized.
#define USE_ARGB32 PLATFORM(IOS_FAMILY)

namespace WebCore {

static FloatSize scaleSizeToUserSpace(const FloatSize& logicalSize, const IntSize& backingStoreSize, const IntSize& internalSize)
{
    float xMagnification = static_cast<float>(backingStoreSize.width()) / internalSize.width();
    float yMagnification = static_cast<float>(backingStoreSize.height()) / internalSize.height();
    return FloatSize(logicalSize.width() * xMagnification, logicalSize.height() * yMagnification);
}

std::unique_ptr<ImageBuffer> ImageBuffer::createCompatibleBuffer(const FloatSize& size, const GraphicsContext& context)
{
    if (size.isEmpty())
        return nullptr;

    RetainPtr<CGColorSpaceRef> colorSpace;
#if PLATFORM(COCOA)
    CGContextRef cgContext = context.platformContext();
    switch (CGContextGetType(cgContext)) {
    case kCGContextTypeBitmap:
        colorSpace = CGBitmapContextGetColorSpace(cgContext);
        break;
#if HAVE(IOSURFACE)
    case kCGContextTypeIOSurface:
        colorSpace = CGIOSurfaceContextGetColorSpace(cgContext);
        break;
#endif
    default:
        colorSpace = adoptCF(CGContextCopyDeviceColorSpace(cgContext));
    }

    if (!colorSpace)
        colorSpace = sRGBColorSpaceRef();
#else
    colorSpace = sRGBColorSpaceRef();
#endif
    RenderingMode renderingMode = context.renderingMode();
    IntSize scaledSize = ImageBuffer::compatibleBufferSize(size, context);
    bool success = false;
    std::unique_ptr<ImageBuffer> buffer(new ImageBuffer(scaledSize, 1, colorSpace.get(), renderingMode, nullptr, success));

    if (!success)
        return nullptr;

    // Set up a corresponding scale factor on the graphics context.
    buffer->context().scale(scaledSize / size);
    return buffer;
}

ImageBuffer::ImageBuffer(const FloatSize& size, float resolutionScale, CGColorSpaceRef colorSpace, RenderingMode renderingMode, const HostWindow* hostWindow, bool& success)
    : m_logicalSize(size)
    , m_resolutionScale(resolutionScale)
{
    success = false; // Make early return mean failure.
    float scaledWidth = ceilf(resolutionScale * size.width());
    float scaledHeight = ceilf(resolutionScale * size.height());

    // FIXME: Should we automatically use a lower resolution?
    if (!FloatSize(scaledWidth, scaledHeight).isExpressibleAsIntSize())
        return;

    m_size = IntSize(scaledWidth, scaledHeight);
    m_data.backingStoreSize = m_size;

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

    m_data.colorSpace = colorSpace;

    RetainPtr<CGContextRef> cgContext;
    if (accelerateRendering) {
#if USE(IOSURFACE_CANVAS_BACKING_STORE)
        FloatSize userBounds = sizeForDestinationSize(FloatSize(width.unsafeGet(), height.unsafeGet()));
        m_data.surface = IOSurface::create(m_data.backingStoreSize, IntSize(userBounds), colorSpace);
        if (m_data.surface) {
            cgContext = m_data.surface->ensurePlatformContext(hostWindow);
            if (cgContext)
                CGContextClearRect(cgContext.get(), FloatRect(FloatPoint(), userBounds));
            else
                m_data.surface = nullptr;
        }
#else
        UNUSED_PARAM(hostWindow);
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
        const auto releaseImageData = [] (void*, const void* data, size_t) {
            fastFree(const_cast<void*>(data));
        };
        // Create a live image that wraps the data.
        verifyImageBufferIsBigEnough(m_data.data, numBytes.unsafeGet());
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

ImageBuffer::ImageBuffer(const FloatSize& size, float resolutionScale, ColorSpace imageColorSpace, RenderingMode renderingMode, const HostWindow* hostWindow, bool& success)
    : ImageBuffer(size, resolutionScale, cachedCGColorSpace(imageColorSpace), renderingMode, hostWindow, success)
{
}

ImageBuffer::~ImageBuffer() = default;

FloatSize ImageBuffer::sizeForDestinationSize(FloatSize destinationSize) const
{
    return scaleSizeToUserSpace(destinationSize, m_data.backingStoreSize, internalSize());
}

#if USE(IOSURFACE_CANVAS_BACKING_STORE)
size_t ImageBuffer::memoryCost() const
{
    // memoryCost() may be invoked concurrently from a GC thread, and we need to be careful about what data we access here and how.
    // It's safe to access internalSize() because it doesn't do any pointer chasing.
    // It's safe to access m_data.surface because the surface can only be assigned during construction of this ImageBuffer.
    // It's safe to access m_data.surface->totalBytes() because totalBytes() doesn't chase pointers.
    if (m_data.surface)
        return m_data.surface->totalBytes();
    return 4 * internalSize().width() * internalSize().height();
}

size_t ImageBuffer::externalMemoryCost() const
{
    // externalMemoryCost() may be invoked concurrently from a GC thread, and we need to be careful about what data we access here and how.
    // It's safe to access m_data.surface because the surface can only be assigned during construction of this ImageBuffer.
    // It's safe to access m_data.surface->totalBytes() because totalBytes() doesn't chase pointers.
    if (m_data.surface)
        return m_data.surface->totalBytes();
    return 0;
}
#endif

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
    if (image && (CGImageGetWidth(image) != static_cast<size_t>(bounds.width()) || CGImageGetHeight(image) != static_cast<size_t>(bounds.height())))
        return adoptCF(CGImageCreateWithImageInRect(image, CGRectMake(0, 0, bounds.width(), bounds.height())));
    return image;
}

static RefPtr<Image> createBitmapImageAfterScalingIfNeeded(RetainPtr<CGImageRef>&& image, IntSize internalSize, IntSize logicalSize, IntSize backingStoreSize, float resolutionScale, PreserveResolution preserveResolution)
{
    if (resolutionScale == 1 || preserveResolution == PreserveResolution::Yes)
        image = createCroppedImageIfNecessary(image.get(), internalSize);
    else {
        auto context = adoptCF(CGBitmapContextCreate(0, logicalSize.width(), logicalSize.height(), 8, 4 * logicalSize.width(), sRGBColorSpaceRef(), kCGImageAlphaPremultipliedLast));
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

RefPtr<Image> ImageBuffer::copyImage(BackingStoreCopy copyBehavior, PreserveResolution preserveResolution) const
{
    RetainPtr<CGImageRef> image;
    if (m_resolutionScale == 1 || preserveResolution == PreserveResolution::Yes)
        image = copyNativeImage(copyBehavior);
    else
        image = copyNativeImage(DontCopyBackingStore);

    return createBitmapImageAfterScalingIfNeeded(WTFMove(image), internalSize(), logicalSize(), m_data.backingStoreSize, m_resolutionScale, preserveResolution);
}

RefPtr<Image> ImageBuffer::sinkIntoImage(std::unique_ptr<ImageBuffer> imageBuffer, PreserveResolution preserveResolution)
{
    IntSize internalSize = imageBuffer->internalSize();
    IntSize logicalSize = imageBuffer->logicalSize();
    IntSize backingStoreSize = imageBuffer->m_data.backingStoreSize;
    float resolutionScale = imageBuffer->m_resolutionScale;

    return createBitmapImageAfterScalingIfNeeded(sinkIntoNativeImage(WTFMove(imageBuffer)), internalSize, logicalSize, backingStoreSize, resolutionScale, preserveResolution);
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
    adjustedSrcRect.scale(resolutionScale);
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
    adjustedSrcRect.scale(m_resolutionScale);
    destContext.drawNativeImage(image.get(), m_data.backingStoreSize, destRect, adjustedSrcRect, op, blendMode);
}

void ImageBuffer::drawPattern(GraphicsContext& destContext, const FloatRect& destRect, const FloatRect& srcRect, const AffineTransform& patternTransform, const FloatPoint& phase, const FloatSize& spacing, CompositeOperator op, BlendMode blendMode)
{
    FloatRect adjustedSrcRect = srcRect;
    adjustedSrcRect.scale(m_resolutionScale);

    if (!context().isAcceleratedContext()) {
        if (&destContext == &context() || destContext.isAcceleratedContext()) {
            if (RefPtr<Image> copy = copyImage(CopyBackingStore)) // Drawing into our own buffer, need to deep copy.
                copy->drawPattern(destContext, destRect, adjustedSrcRect, patternTransform, phase, spacing, op, blendMode);
        } else {
            if (RefPtr<Image> imageForRendering = copyImage(DontCopyBackingStore))
                imageForRendering->drawPattern(destContext, destRect, adjustedSrcRect, patternTransform, phase, spacing, op, blendMode);
        }
    } else {
        if (RefPtr<Image> copy = copyImage(CopyBackingStore))
            copy->drawPattern(destContext, destRect, adjustedSrcRect, patternTransform, phase, spacing, op, blendMode);
    }
}

RefPtr<Uint8ClampedArray> ImageBuffer::getUnmultipliedImageData(const IntRect& rect, IntSize* pixelArrayDimensions, CoordinateSystem coordinateSystem) const
{
    if (context().isAcceleratedContext())
        flushContext();

    IntRect srcRect = rect;
    if (coordinateSystem == LogicalCoordinateSystem)
        srcRect.scale(m_resolutionScale);

    if (pixelArrayDimensions)
        *pixelArrayDimensions = srcRect.size();

    return m_data.getData(AlphaPremultiplication::Unpremultiplied, srcRect, internalSize(), context().isAcceleratedContext());
}

RefPtr<Uint8ClampedArray> ImageBuffer::getPremultipliedImageData(const IntRect& rect, IntSize* pixelArrayDimensions, CoordinateSystem coordinateSystem) const
{
    if (context().isAcceleratedContext())
        flushContext();

    IntRect srcRect = rect;
    if (coordinateSystem == LogicalCoordinateSystem)
        srcRect.scale(m_resolutionScale);

    if (pixelArrayDimensions)
        *pixelArrayDimensions = srcRect.size();

    return m_data.getData(AlphaPremultiplication::Premultiplied, srcRect, internalSize(), context().isAcceleratedContext());
}

void ImageBuffer::putByteArray(const Uint8ClampedArray& source, AlphaPremultiplication sourceFormat, const IntSize& sourceSize, const IntRect& sourceRect, const IntPoint& destPoint, CoordinateSystem coordinateSystem)
{
    if (context().isAcceleratedContext())
        flushContext();

    IntRect scaledSourceRect = sourceRect;
    IntSize scaledSourceSize = sourceSize;
    if (coordinateSystem == LogicalCoordinateSystem) {
        scaledSourceRect.scale(m_resolutionScale);
        scaledSourceSize.scale(m_resolutionScale);
    }

    m_data.putData(source, sourceFormat, scaledSourceSize, scaledSourceRect, destPoint, internalSize(), context().isAcceleratedContext());
    
    // Force recreating the IOSurface cached image if it is requested through CGIOSurfaceContextCreateImage().
    // See https://bugs.webkit.org/show_bug.cgi?id=157966 for explaining why this is necessary.
    if (context().isAcceleratedContext())
        context().fillRect(FloatRect(1, 1, 0, 0));
}

static inline CFStringRef jpegUTI()
{
#if PLATFORM(IOS_FAMILY) || PLATFORM(WIN)
    static const CFStringRef kUTTypeJPEG = CFSTR("public.jpeg");
#endif
    return kUTTypeJPEG;
}
    
static RetainPtr<CFStringRef> utiFromImageBufferMIMEType(const String& mimeType)
{
    // FIXME: Why doesn't iOS use the CoreServices version?
#if PLATFORM(MAC)
    return UTIFromMIMEType(mimeType).createCFString();
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

static bool encodeImage(CGImageRef image, CFStringRef uti, std::optional<double> quality, CFMutableDataRef data)
{
    if (!image || !uti || !data)
        return false;

    auto destination = adoptCF(CGImageDestinationCreateWithData(data, uti, 1, 0));
    if (!destination)
        return false;

    RetainPtr<CFDictionaryRef> imageProperties;
    if (CFEqual(uti, jpegUTI()) && quality && *quality >= 0.0 && *quality <= 1.0) {
        // Apply the compression quality to the JPEG image destination.
        auto compressionQuality = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberDoubleType, &*quality));
        const void* key = kCGImageDestinationLossyCompressionQuality;
        const void* value = compressionQuality.get();
        imageProperties = adoptCF(CFDictionaryCreate(0, &key, &value, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    }

    // Setting kCGImageDestinationBackgroundColor to black for JPEG images in imageProperties would save some math
    // in the calling functions, but it doesn't seem to work.

    CGImageDestinationAddImage(destination.get(), image, imageProperties.get());
    return CGImageDestinationFinalize(destination.get());
}

static String dataURL(CFDataRef data, const String& mimeType)
{
    Vector<char> base64Data;
    base64Encode(CFDataGetBytePtr(data), CFDataGetLength(data), base64Data);

    return "data:" + mimeType + ";base64," + base64Data;
}

static Vector<uint8_t> dataVector(CFDataRef cfData)
{
    Vector<uint8_t> data;
    data.append(CFDataGetBytePtr(cfData), CFDataGetLength(cfData));
    return data;
}

String ImageBuffer::toDataURL(const String& mimeType, std::optional<double> quality, PreserveResolution preserveResolution) const
{
    if (auto data = toCFData(mimeType, quality, preserveResolution))
        return dataURL(data.get(), mimeType);
    return "data:,"_s;
}

Vector<uint8_t> ImageBuffer::toData(const String& mimeType, std::optional<double> quality) const
{
    if (auto data = toCFData(mimeType, quality, PreserveResolution::No))
        return dataVector(data.get());
    return { };
}

RetainPtr<CFDataRef> ImageBuffer::toCFData(const String& mimeType, std::optional<double> quality, PreserveResolution preserveResolution) const
{
    ASSERT(MIMETypeRegistry::isSupportedImageMIMETypeForEncoding(mimeType));

    if (context().isAcceleratedContext())
        flushContext();

    auto uti = utiFromImageBufferMIMEType(mimeType);
    ASSERT(uti);

    RetainPtr<CGImageRef> image;
    RefPtr<Uint8ClampedArray> premultipliedData;

    if (CFEqual(uti.get(), jpegUTI())) {
        // JPEGs don't have an alpha channel, so we have to manually composite on top of black.
        premultipliedData = getPremultipliedImageData(IntRect(IntPoint(), logicalSize()));
        if (!premultipliedData)
            return nullptr;

        size_t dataSize = 4 * logicalSize().width() * logicalSize().height();
        verifyImageBufferIsBigEnough(premultipliedData->data(), dataSize);
        auto dataProvider = adoptCF(CGDataProviderCreateWithData(nullptr, premultipliedData->data(), dataSize, nullptr));
        if (!dataProvider)
            return nullptr;

        image = adoptCF(CGImageCreate(logicalSize().width(), logicalSize().height(), 8, 32, 4 * logicalSize().width(), sRGBColorSpaceRef(), kCGBitmapByteOrderDefault | kCGImageAlphaNoneSkipLast, dataProvider.get(), 0, false, kCGRenderingIntentDefault));
    } else if (m_resolutionScale == 1 || preserveResolution == PreserveResolution::Yes) {
        image = copyNativeImage(CopyBackingStore);
        image = createCroppedImageIfNecessary(image.get(), internalSize());
    } else {
        image = copyNativeImage(DontCopyBackingStore);
        auto context = adoptCF(CGBitmapContextCreate(0, logicalSize().width(), logicalSize().height(), 8, 4 * logicalSize().width(), sRGBColorSpaceRef(), kCGImageAlphaPremultipliedLast));
        CGContextSetBlendMode(context.get(), kCGBlendModeCopy);
        CGContextClipToRect(context.get(), CGRectMake(0, 0, logicalSize().width(), logicalSize().height()));
        FloatSize imageSizeInUserSpace = sizeForDestinationSize(logicalSize());
        CGContextDrawImage(context.get(), CGRectMake(0, 0, imageSizeInUserSpace.width(), imageSizeInUserSpace.height()), image.get());
        image = adoptCF(CGBitmapContextCreateImage(context.get()));
    }

    auto cfData = adoptCF(CFDataCreateMutable(kCFAllocatorDefault, 0));
    if (!encodeImage(image.get(), uti.get(), quality, cfData.get()))
        return nullptr;

    return WTFMove(cfData);
}

static RetainPtr<CFDataRef> cfData(const ImageData& source, const String& mimeType, std::optional<double> quality)
{
    ASSERT(MIMETypeRegistry::isSupportedImageMIMETypeForEncoding(mimeType));

    auto uti = utiFromImageBufferMIMEType(mimeType);
    ASSERT(uti);

    CGImageAlphaInfo dataAlphaInfo = kCGImageAlphaLast;
    unsigned char* data = source.data()->data();
    Vector<uint8_t> premultipliedData;

    if (CFEqual(uti.get(), jpegUTI())) {
        // JPEGs don't have an alpha channel, so we have to manually composite on top of black.
        size_t size = 4 * source.width() * source.height();
        if (!premultipliedData.tryReserveCapacity(size))
            return nullptr;

        premultipliedData.grow(size);
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

    verifyImageBufferIsBigEnough(data, 4 * source.width() * source.height());
    auto dataProvider = adoptCF(CGDataProviderCreateWithData(0, data, 4 * source.width() * source.height(), 0));
    if (!dataProvider)
        return nullptr;

    auto image = adoptCF(CGImageCreate(source.width(), source.height(), 8, 32, 4 * source.width(), sRGBColorSpaceRef(), kCGBitmapByteOrderDefault | dataAlphaInfo, dataProvider.get(), 0, false, kCGRenderingIntentDefault));

    auto cfData = adoptCF(CFDataCreateMutable(kCFAllocatorDefault, 0));
    if (!encodeImage(image.get(), uti.get(), quality, cfData.get()))
        return nullptr;

    return WTFMove(cfData);
}

String dataURL(const ImageData& source, const String& mimeType, std::optional<double> quality)
{
    if (auto data = cfData(source, mimeType, quality))
        return dataURL(data.get(), mimeType);
    return "data:,"_s;
}

Vector<uint8_t> data(const ImageData& source, const String& mimeType, std::optional<double> quality)
{
    if (auto data = cfData(source, mimeType, quality))
        return dataVector(data.get());
    return { };
}

Vector<uint8_t> ImageBuffer::toBGRAData() const
{
    if (context().isAcceleratedContext())
        flushContext();
    return m_data.toBGRAData(context().isAcceleratedContext(), m_size.width(), m_size.height());
}

} // namespace WebCore

#endif
