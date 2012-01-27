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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
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

#include "Base64.h"
#include "BitmapImage.h"
#include "GraphicsContext.h"
#include "GraphicsContextCG.h"
#include "ImageData.h"
#include "MIMETypeRegistry.h"
#include <ApplicationServices/ApplicationServices.h>
#include <math.h>
#include <wtf/Assertions.h>
#include <wtf/CheckedArithmetic.h>
#include <wtf/CurrentTime.h>
#include <wtf/MainThread.h>
#include <wtf/OwnArrayPtr.h>
#include <wtf/RetainPtr.h>
#include <wtf/UnusedParam.h>
#include <wtf/text/WTFString.h>

#if PLATFORM(MAC) || PLATFORM(CHROMIUM)
#include "WebCoreSystemInterface.h"
#endif

#if USE(IOSURFACE_CANVAS_BACKING_STORE)
#include <IOSurface/IOSurface.h>
#endif

using namespace std;

namespace WebCore {

#if USE(IOSURFACE_CANVAS_BACKING_STORE)
static const int maxIOSurfaceDimension = 4096;
static const int minIOSurfaceArea = 50 * 100;

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

    const void *keys[6];
    const void *values[6];
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

    RetainPtr<CFDictionaryRef> dict(AdoptCF, CFDictionaryCreate(0, keys, values, 6, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    for (unsigned i = 0; i < 6; i++)
        CFRelease(values[i]);

    return RetainPtr<IOSurfaceRef>(AdoptCF, IOSurfaceCreate(dict.get()));
}
#endif

static void releaseImageData(void*, const void* data, size_t)
{
    fastFree(const_cast<void*>(data));
}

ImageBuffer::ImageBuffer(const IntSize& size, ColorSpace imageColorSpace, RenderingMode renderingMode, bool& success)
    : m_data(size)
    , m_size(size)
{
    success = false;  // Make early return mean failure.
    bool accelerateRendering = renderingMode == Accelerated;
    if (size.width() <= 0 || size.height() <= 0)
        return;

    Checked<int, RecordOverflow> width = size.width();
    Checked<int, RecordOverflow> height = size.height();

    // Prevent integer overflows
    m_data.m_bytesPerRow = 4 * width;
    Checked<size_t, RecordOverflow> dataSize = height * m_data.m_bytesPerRow;
    if (dataSize.hasOverflowed())
        return;

#if USE(IOSURFACE_CANVAS_BACKING_STORE)
    if (width.unsafeGet() >= maxIOSurfaceDimension || height.unsafeGet() >= maxIOSurfaceDimension || (width * height).unsafeGet() < minIOSurfaceArea)
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
        m_data.m_surface = createIOSurface(size);
        cgContext.adoptCF(wkIOSurfaceContextCreate(m_data.m_surface.get(), width.unsafeGet(), height.unsafeGet(), m_data.m_colorSpace));
#endif
        if (!cgContext)
            accelerateRendering = false; // If allocation fails, fall back to non-accelerated path.
    }

    if (!accelerateRendering) {
        if (!tryFastCalloc(height.unsafeGet(), m_data.m_bytesPerRow.unsafeGet()).getValue(m_data.m_data))
            return;
        ASSERT(!(reinterpret_cast<size_t>(m_data.m_data) & 2));

        m_data.m_bitmapInfo = kCGImageAlphaPremultipliedLast;
        cgContext.adoptCF(CGBitmapContextCreate(m_data.m_data, width.unsafeGet(), height.unsafeGet(), 8, m_data.m_bytesPerRow.unsafeGet(), m_data.m_colorSpace, m_data.m_bitmapInfo));
        // Create a live image that wraps the data.
        m_data.m_dataProvider.adoptCF(CGDataProviderCreateWithData(0, m_data.m_data, dataSize.unsafeGet(), releaseImageData));
    }

    if (!cgContext)
        return;

    m_context = adoptPtr(new GraphicsContext(cgContext.get()));
    m_context->scale(FloatSize(1, -1));
    m_context->translate(0, -height.unsafeGet());
    m_context->setIsAcceleratedContext(accelerateRendering);
    m_data.m_lastFlushTime = currentTimeMS();
    success = true;
}

ImageBuffer::~ImageBuffer()
{
}

size_t ImageBuffer::dataSize() const
{
    return m_size.height() * m_data.m_bytesPerRow.unsafeGet();
}

GraphicsContext* ImageBuffer::context() const
{
    // Force a flush if last flush was more than 20ms ago
    if (m_context->isAcceleratedContext()) {
        double elapsedTime = currentTimeMS() - m_data.m_lastFlushTime;
        double maxFlushInterval = 20; // in ms

        if (elapsedTime > maxFlushInterval) {
            CGContextRef context = m_context->platformContext();
            CGContextFlush(context);
            m_data.m_lastFlushTime = currentTimeMS();
        }
    }

    return m_context.get();
}

PassRefPtr<Image> ImageBuffer::copyImage(BackingStoreCopy copyBehavior) const
{
    RetainPtr<CGImageRef> image = copyNativeImage(copyBehavior);

    if (!image)
        return 0;

    return BitmapImage::create(image.get());
}

NativeImagePtr ImageBuffer::copyNativeImage(BackingStoreCopy copyBehavior) const
{
    CGImageRef image = 0;
    if (!m_context->isAcceleratedContext()) {
        switch (copyBehavior) {
        case DontCopyBackingStore:
            image = CGImageCreate(m_size.width(), m_size.height(), 8, 32, m_data.m_bytesPerRow.unsafeGet(), m_data.m_colorSpace, m_data.m_bitmapInfo, m_data.m_dataProvider.get(), 0, true, kCGRenderingIntentDefault);
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
    else {
        image = wkIOSurfaceContextCreateImage(context()->platformContext());
        m_data.m_lastFlushTime = currentTimeMS();
    }
#endif

    return image;
}

void ImageBuffer::draw(GraphicsContext* destContext, ColorSpace styleColorSpace, const FloatRect& destRect, const FloatRect& srcRect, CompositeOperator op, bool useLowQualityScale)
{
    UNUSED_PARAM(useLowQualityScale);
    ColorSpace colorSpace = (destContext == m_context) ? ColorSpaceDeviceRGB : styleColorSpace;

    RetainPtr<CGImageRef> image;
    if (destContext == m_context || destContext->isAcceleratedContext())
        image.adoptCF(copyNativeImage(CopyBackingStore)); // Drawing into our own buffer, need to deep copy.
    else
        image.adoptCF(copyNativeImage(DontCopyBackingStore));

    destContext->drawNativeImage(image.get(), m_size, colorSpace, destRect, srcRect, op);
}

void ImageBuffer::drawPattern(GraphicsContext* destContext, const FloatRect& srcRect, const AffineTransform& patternTransform, const FloatPoint& phase, ColorSpace styleColorSpace, CompositeOperator op, const FloatRect& destRect)
{
    if (!m_context->isAcceleratedContext()) {
        if (destContext == m_context || destContext->isAcceleratedContext()) {
            RefPtr<Image> copy = copyImage(CopyBackingStore); // Drawing into our own buffer, need to deep copy.
            copy->drawPattern(destContext, srcRect, patternTransform, phase, styleColorSpace, op, destRect);
        } else {
            RefPtr<Image> imageForRendering = copyImage(DontCopyBackingStore);
            imageForRendering->drawPattern(destContext, srcRect, patternTransform, phase, styleColorSpace, op, destRect);
        }
    } else {
        RefPtr<Image> copy = copyImage(CopyBackingStore);
        copy->drawPattern(destContext, srcRect, patternTransform, phase, styleColorSpace, op, destRect);
    }
}

void ImageBuffer::clip(GraphicsContext* contextToClip, const FloatRect& rect) const
{
    CGContextRef platformContextToClip = contextToClip->platformContext();
    RetainPtr<CGImageRef> image(AdoptCF, copyNativeImage(DontCopyBackingStore));
    CGContextTranslateCTM(platformContextToClip, rect.x(), rect.y() + rect.height());
    CGContextScaleCTM(platformContextToClip, 1, -1);
    CGContextClipToMask(platformContextToClip, FloatRect(FloatPoint(), rect.size()), image.get());
    CGContextScaleCTM(platformContextToClip, 1, -1);
    CGContextTranslateCTM(platformContextToClip, -rect.x(), -rect.y() - rect.height());
}

PassRefPtr<ByteArray> ImageBuffer::getUnmultipliedImageData(const IntRect& rect) const
{
    if (m_context->isAcceleratedContext()) {
        CGContextFlush(context()->platformContext());
        m_data.m_lastFlushTime = currentTimeMS();
    }
    return m_data.getData(rect, m_size, m_context->isAcceleratedContext(), true);
}

PassRefPtr<ByteArray> ImageBuffer::getPremultipliedImageData(const IntRect& rect) const
{
    if (m_context->isAcceleratedContext()) {
        CGContextFlush(context()->platformContext());
        m_data.m_lastFlushTime = currentTimeMS();
    }
    return m_data.getData(rect, m_size, m_context->isAcceleratedContext(), false);
}

void ImageBuffer::putUnmultipliedImageData(ByteArray* source, const IntSize& sourceSize, const IntRect& sourceRect, const IntPoint& destPoint)
{
    if (m_context->isAcceleratedContext()) {
        CGContextFlush(context()->platformContext());
        m_data.m_lastFlushTime = currentTimeMS();
    }
    m_data.putData(source, sourceSize, sourceRect, destPoint, m_size, m_context->isAcceleratedContext(), true);
}

void ImageBuffer::putPremultipliedImageData(ByteArray* source, const IntSize& sourceSize, const IntRect& sourceRect, const IntPoint& destPoint)
{
    if (m_context->isAcceleratedContext()) {
        CGContextFlush(context()->platformContext());
        m_data.m_lastFlushTime = currentTimeMS();
    }
    m_data.putData(source, sourceSize, sourceRect, destPoint, m_size, m_context->isAcceleratedContext(), false);
}

static inline CFStringRef jpegUTI()
{
#if PLATFORM(WIN)
    static const CFStringRef kUTTypeJPEG = CFSTR("public.jpeg");
#endif
    return kUTTypeJPEG;
}
    
static RetainPtr<CFStringRef> utiFromMIMEType(const String& mimeType)
{
#if PLATFORM(MAC)
    RetainPtr<CFStringRef> mimeTypeCFString(AdoptCF, mimeType.createCFString());
    return RetainPtr<CFStringRef>(AdoptCF, UTTypeCreatePreferredIdentifierForTag(kUTTagClassMIMEType, mimeTypeCFString.get(), 0));
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

static String CGImageToDataURL(CGImageRef image, const String& mimeType, const double* quality)
{
    RetainPtr<CFMutableDataRef> data(AdoptCF, CFDataCreateMutable(kCFAllocatorDefault, 0));
    if (!data)
        return "data:,";

    RetainPtr<CFStringRef> uti = utiFromMIMEType(mimeType);
    ASSERT(uti);

    RetainPtr<CGImageDestinationRef> destination(AdoptCF, CGImageDestinationCreateWithData(data.get(), uti.get(), 1, 0));
    if (!destination)
        return "data:,";

    RetainPtr<CFDictionaryRef> imageProperties = 0;
    if (CFEqual(uti.get(), jpegUTI()) && quality && *quality >= 0.0 && *quality <= 1.0) {
        // Apply the compression quality to the image destination.
        RetainPtr<CFNumberRef> compressionQuality(AdoptCF, CFNumberCreate(kCFAllocatorDefault, kCFNumberDoubleType, quality));
        const void* key = kCGImageDestinationLossyCompressionQuality;
        const void* value = compressionQuality.get();
        imageProperties.adoptCF(CFDictionaryCreate(0, &key, &value, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    }

    // Setting kCGImageDestinationBackgroundColor to black in imageProperties would allow saving some math in the
    // calling functions, but it doesn't seem to work.

    CGImageDestinationAddImage(destination.get(), image, imageProperties.get());
    CGImageDestinationFinalize(destination.get());

    Vector<char> out;
    base64Encode(reinterpret_cast<const char*>(CFDataGetBytePtr(data.get())), CFDataGetLength(data.get()), out);

    return "data:" + mimeType + ";base64," + out;
}

String ImageBuffer::toDataURL(const String& mimeType, const double* quality) const
{
    ASSERT(MIMETypeRegistry::isSupportedImageMIMETypeForEncoding(mimeType));
    RetainPtr<CGImageRef> image;
    RetainPtr<CFStringRef> uti = utiFromMIMEType(mimeType);
    ASSERT(uti);
    RefPtr<ByteArray> arr;

    if (CFEqual(uti.get(), jpegUTI())) {
        // JPEGs don't have an alpha channel, so we have to manually composite on top of black.
        arr = getPremultipliedImageData(IntRect(IntPoint(0, 0), m_size));

        unsigned char *data = arr->data();
        for (int i = 0; i < width() * height(); i++)
            data[i * 4 + 3] = 255; // The image is already premultiplied, so we just need to make it opaque.

        RetainPtr<CGDataProviderRef> dataProvider;
    
        dataProvider.adoptCF(CGDataProviderCreateWithData(0, data,
                                                          4 * width() * height(), 0));
    
        if (!dataProvider)
            return "data:,";

        image.adoptCF(CGImageCreate(width(), height(), 8, 32, 4 * width(),
                                    CGColorSpaceCreateDeviceRGB(), kCGBitmapByteOrderDefault | kCGImageAlphaLast,
                                    dataProvider.get(), 0, false, kCGRenderingIntentDefault));
    } else
        image.adoptCF(copyNativeImage(CopyBackingStore));

    if (!image)
        return "data:,";

    return CGImageToDataURL(image.get(), mimeType, quality);
}

String ImageDataToDataURL(const ImageData& source, const String& mimeType, const double* quality)
{
    ASSERT(MIMETypeRegistry::isSupportedImageMIMETypeForEncoding(mimeType));
        
    RetainPtr<CGImageRef> image;
    RetainPtr<CGDataProviderRef> dataProvider;

    unsigned char* data = source.data()->data()->data();
    RetainPtr<CFStringRef> uti = utiFromMIMEType(mimeType);
    ASSERT(uti);
    Vector<uint8_t> dataVector;
    if (CFEqual(uti.get(), jpegUTI())) {
        // JPEGs don't have an alpha channel, so we have to manually composite on top of black.
        dataVector.resize(4 * source.width() * source.height());
        unsigned char *out = dataVector.data();
        
        for (int i = 0; i < source.width() * source.height(); i++) {
            // Multiply color data by alpha, and set alpha to 255.
            int alpha = data[4 * i + 3];
            if (alpha != 255) {
                out[4 * i + 0] = data[4 * i + 0] * alpha / 255;
                out[4 * i + 1] = data[4 * i + 1] * alpha / 255;
                out[4 * i + 2] = data[4 * i + 2] * alpha / 255;
            } else {
                out[4 * i + 0] = data[4 * i + 0];
                out[4 * i + 1] = data[4 * i + 1];
                out[4 * i + 2] = data[4 * i + 2];
            }
            out[4 * i + 3] = 255;
        }

        data = out;
    }
    
    dataProvider.adoptCF(CGDataProviderCreateWithData(0, data,
                                                      4 * source.width() * source.height(), 0));
    
    if (!dataProvider)
        return "data:,";

    image.adoptCF(CGImageCreate(source.width(), source.height(), 8, 32, 4 * source.width(),
                                CGColorSpaceCreateDeviceRGB(), kCGBitmapByteOrderDefault | kCGImageAlphaLast,
                                dataProvider.get(), 0, false, kCGRenderingIntentDefault));
                                
        
    if (!image)
        return "data:,";

    return CGImageToDataURL(image.get(), mimeType, quality);
}
} // namespace WebCore
