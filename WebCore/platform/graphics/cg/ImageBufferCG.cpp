/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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
#include "CString.h"
#include "GraphicsContext.h"
#include "ImageData.h"
#include "MIMETypeRegistry.h"
#include "PlatformString.h"
#include <ApplicationServices/ApplicationServices.h>
#include <wtf/Assertions.h>
#include <wtf/OwnArrayPtr.h>
#include <wtf/RetainPtr.h>
#include <math.h>

using namespace std;

namespace WebCore {

ImageBufferData::ImageBufferData(const IntSize&)
    : m_data(0)
{
}

ImageBuffer::ImageBuffer(const IntSize& size, ImageColorSpace imageColorSpace, bool& success)
    : m_data(size)
    , m_size(size)
{
    success = false;  // Make early return mean failure.
    unsigned bytesPerRow;
    if (size.width() < 0 || size.height() < 0)
        return;
    bytesPerRow = size.width();
    if (imageColorSpace != GrayScale) {
        // Protect against overflow
        if (bytesPerRow > 0x3FFFFFFF)
            return;
        bytesPerRow *= 4;
    }

    if (!tryFastCalloc(size.height(), bytesPerRow).getValue(m_data.m_data))
        return;

    ASSERT((reinterpret_cast<size_t>(m_data.m_data) & 2) == 0);

    RetainPtr<CGColorSpaceRef> colorSpace;
    switch(imageColorSpace) {
        case DeviceRGB:
            colorSpace.adoptCF(CGColorSpaceCreateDeviceRGB());
            break;
        case GrayScale:
            colorSpace.adoptCF(CGColorSpaceCreateDeviceGray());
            break;
#if PLATFORM(MAC) && !defined(BUILDING_ON_TIGER)
        case LinearRGB:
            colorSpace.adoptCF(CGColorSpaceCreateWithName(kCGColorSpaceGenericRGBLinear));
            break;
#endif
        default:
            colorSpace.adoptCF(CGColorSpaceCreateDeviceRGB());
            break;
    }

    RetainPtr<CGContextRef> cgContext(AdoptCF, CGBitmapContextCreate(m_data.m_data, size.width(), size.height(), 8, bytesPerRow,
        colorSpace.get(), (imageColorSpace == GrayScale) ? kCGImageAlphaNone : kCGImageAlphaPremultipliedLast));
    if (!cgContext)
        return;

    m_context.set(new GraphicsContext(cgContext.get()));
    m_context->scale(FloatSize(1, -1));
    m_context->translate(0, -size.height());
    success = true;
}

ImageBuffer::~ImageBuffer()
{
    fastFree(m_data.m_data);
}

GraphicsContext* ImageBuffer::context() const
{
    return m_context.get();
}

Image* ImageBuffer::image() const
{
    if (!m_image) {
        // It's assumed that if image() is called, the actual rendering to the
        // GraphicsContext must be done.
        ASSERT(context());
        CGImageRef cgImage = CGBitmapContextCreateImage(context()->platformContext());
        // BitmapImage will release the passed in CGImage on destruction
        m_image = BitmapImage::create(cgImage);
    }
    return m_image.get();
}

template <Multiply multiplied>
PassRefPtr<ImageData> getImageData(const IntRect& rect, const ImageBufferData& imageData, const IntSize& size)
{
    PassRefPtr<ImageData> result = ImageData::create(rect.width(), rect.height());
    unsigned char* data = result->data()->data()->data();

    if (rect.x() < 0 || rect.y() < 0 || (rect.x() + rect.width()) > size.width() || (rect.y() + rect.height()) > size.height())
        memset(data, 0, result->data()->length());

    int originx = rect.x();
    int destx = 0;
    if (originx < 0) {
        destx = -originx;
        originx = 0;
    }
    int endx = rect.x() + rect.width();
    if (endx > size.width())
        endx = size.width();
    int numColumns = endx - originx;

    int originy = rect.y();
    int desty = 0;
    if (originy < 0) {
        desty = -originy;
        originy = 0;
    }
    int endy = rect.y() + rect.height();
    if (endy > size.height())
        endy = size.height();
    int numRows = endy - originy;

    unsigned srcBytesPerRow = 4 * size.width();
    unsigned destBytesPerRow = 4 * rect.width();

    // ::create ensures that all ImageBuffers have valid data, so we don't need to check it here.
    unsigned char* srcRows = reinterpret_cast<unsigned char*>(imageData.m_data) + originy * srcBytesPerRow + originx * 4;
    unsigned char* destRows = data + desty * destBytesPerRow + destx * 4;
    for (int y = 0; y < numRows; ++y) {
        for (int x = 0; x < numColumns; x++) {
            int basex = x * 4;
            unsigned char alpha = srcRows[basex + 3];
            if (multiplied == Unmultiplied && alpha) {
                destRows[basex] = (srcRows[basex] * 255) / alpha;
                destRows[basex + 1] = (srcRows[basex + 1] * 255) / alpha;
                destRows[basex + 2] = (srcRows[basex + 2] * 255) / alpha;
                destRows[basex + 3] = alpha;
            } else
                reinterpret_cast<uint32_t*>(destRows + basex)[0] = reinterpret_cast<uint32_t*>(srcRows + basex)[0];
        }
        srcRows += srcBytesPerRow;
        destRows += destBytesPerRow;
    }
    return result;
}

PassRefPtr<ImageData> ImageBuffer::getUnmultipliedImageData(const IntRect& rect) const
{
    return getImageData<Unmultiplied>(rect, m_data, m_size);
}

PassRefPtr<ImageData> ImageBuffer::getPremultipliedImageData(const IntRect& rect) const
{
    return getImageData<Premultiplied>(rect, m_data, m_size);
}

template <Multiply multiplied>
void putImageData(ImageData*& source, const IntRect& sourceRect, const IntPoint& destPoint, ImageBufferData& imageData, const IntSize& size)
{
    ASSERT(sourceRect.width() > 0);
    ASSERT(sourceRect.height() > 0);

    int originx = sourceRect.x();
    int destx = destPoint.x() + sourceRect.x();
    ASSERT(destx >= 0);
    ASSERT(destx < size.width());
    ASSERT(originx >= 0);
    ASSERT(originx <= sourceRect.right());

    int endx = destPoint.x() + sourceRect.right();
    ASSERT(endx <= size.width());

    int numColumns = endx - destx;

    int originy = sourceRect.y();
    int desty = destPoint.y() + sourceRect.y();
    ASSERT(desty >= 0);
    ASSERT(desty < size.height());
    ASSERT(originy >= 0);
    ASSERT(originy <= sourceRect.bottom());

    int endy = destPoint.y() + sourceRect.bottom();
    ASSERT(endy <= size.height());
    int numRows = endy - desty;

    unsigned srcBytesPerRow = 4 * source->width();
    unsigned destBytesPerRow = 4 * size.width();

    unsigned char* srcRows = source->data()->data()->data() + originy * srcBytesPerRow + originx * 4;
    unsigned char* destRows = reinterpret_cast<unsigned char*>(imageData.m_data) + desty * destBytesPerRow + destx * 4;
    for (int y = 0; y < numRows; ++y) {
        for (int x = 0; x < numColumns; x++) {
            int basex = x * 4;
            unsigned char alpha = srcRows[basex + 3];
            if (multiplied == Unmultiplied && alpha != 255) {
                destRows[basex] = (srcRows[basex] * alpha + 254) / 255;
                destRows[basex + 1] = (srcRows[basex + 1] * alpha + 254) / 255;
                destRows[basex + 2] = (srcRows[basex + 2] * alpha + 254) / 255;
                destRows[basex + 3] = alpha;
            } else
                reinterpret_cast<uint32_t*>(destRows + basex)[0] = reinterpret_cast<uint32_t*>(srcRows + basex)[0];
        }
        destRows += destBytesPerRow;
        srcRows += srcBytesPerRow;
    }
}

void ImageBuffer::putUnmultipliedImageData(ImageData* source, const IntRect& sourceRect, const IntPoint& destPoint)
{
    putImageData<Unmultiplied>(source, sourceRect, destPoint, m_data, m_size);
}

void ImageBuffer::putPremultipliedImageData(ImageData* source, const IntRect& sourceRect, const IntPoint& destPoint)
{
    putImageData<Premultiplied>(source, sourceRect, destPoint, m_data, m_size);
}

static RetainPtr<CFStringRef> utiFromMIMEType(const String& mimeType)
{
#if PLATFORM(MAC)
    RetainPtr<CFStringRef> mimeTypeCFString(AdoptCF, mimeType.createCFString());
    return RetainPtr<CFStringRef>(AdoptCF, UTTypeCreatePreferredIdentifierForTag(kUTTagClassMIMEType, mimeTypeCFString.get(), 0));
#else
    // FIXME: Add Windows support for all the supported UTIs when a way to convert from MIMEType to UTI reliably is found.
    // For now, only support PNG, JPEG, and GIF. See <rdar://problem/6095286>.
    static const CFStringRef kUTTypePNG = CFSTR("public.png");
    static const CFStringRef kUTTypeJPEG = CFSTR("public.jpeg");
    static const CFStringRef kUTTypeGIF = CFSTR("com.compuserve.gif");

    if (equalIgnoringCase(mimeType, "image/png"))
        return kUTTypePNG;
    if (equalIgnoringCase(mimeType, "image/jpeg"))
        return kUTTypeJPEG;
    if (equalIgnoringCase(mimeType, "image/gif"))
        return kUTTypeGIF;

    ASSERT_NOT_REACHED();
    return kUTTypePNG;
#endif
}

String ImageBuffer::toDataURL(const String& mimeType) const
{
    ASSERT(MIMETypeRegistry::isSupportedImageMIMETypeForEncoding(mimeType));

    RetainPtr<CGImageRef> image(AdoptCF, CGBitmapContextCreateImage(context()->platformContext()));
    if (!image)
        return "data:,";

    RetainPtr<CFMutableDataRef> data(AdoptCF, CFDataCreateMutable(kCFAllocatorDefault, 0));
    if (!data)
        return "data:,";

    RetainPtr<CGImageDestinationRef> destination(AdoptCF, CGImageDestinationCreateWithData(data.get(), utiFromMIMEType(mimeType).get(), 1, 0));
    if (!destination)
        return "data:,";

    CGImageDestinationAddImage(destination.get(), image.get(), 0);
    CGImageDestinationFinalize(destination.get());

    Vector<char> out;
    base64Encode(reinterpret_cast<const char*>(CFDataGetBytePtr(data.get())), CFDataGetLength(data.get()), out);
    out.append('\0');

    return String::format("data:%s;base64,%s", mimeType.utf8().data(), out.data());
}

} // namespace WebCore
