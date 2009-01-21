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

using namespace std;

namespace WebCore {

ImageBufferData::ImageBufferData(const IntSize&)
    : m_data(0)
{
}

ImageBuffer::ImageBuffer(const IntSize& size, bool grayScale, bool& success)
    : m_data(size)
    , m_size(size)
{
    success = false;  // Make early return mean failure.
    unsigned bytesPerRow;
    if (size.width() < 0 || size.height() < 0)
        return;
    bytesPerRow = size.width();
    if (!grayScale) {
        // Protect against overflow
        if (bytesPerRow > 0x3FFFFFFF)
            return;
        bytesPerRow *= 4;
    }

    m_data.m_data = tryFastCalloc(size.height(), bytesPerRow);
    ASSERT((reinterpret_cast<size_t>(m_data.m_data) & 2) == 0);

    CGColorSpaceRef colorSpace = grayScale ? CGColorSpaceCreateDeviceGray() : CGColorSpaceCreateDeviceRGB();
    CGContextRef cgContext = CGBitmapContextCreate(m_data.m_data, size.width(), size.height(), 8, bytesPerRow,
        colorSpace, grayScale ? kCGImageAlphaNone : kCGImageAlphaPremultipliedLast);
    CGColorSpaceRelease(colorSpace);
    if (!cgContext)
        return;

    m_context.set(new GraphicsContext(cgContext));
    m_context->scale(FloatSize(1, -1));
    m_context->translate(0, -size.height());
    CGContextRelease(cgContext);
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

PassRefPtr<ImageData> ImageBuffer::getImageData(const IntRect& rect) const
{
    PassRefPtr<ImageData> result = ImageData::create(rect.width(), rect.height());
    unsigned char* data = result->data()->data()->data();

    if (rect.x() < 0 || rect.y() < 0 || (rect.x() + rect.width()) > m_size.width() || (rect.y() + rect.height()) > m_size.height())
        memset(data, 0, result->data()->length());

    int originx = rect.x();
    int destx = 0;
    if (originx < 0) {
        destx = -originx;
        originx = 0;
    }
    int endx = rect.x() + rect.width();
    if (endx > m_size.width())
        endx = m_size.width();
    int numColumns = endx - originx;

    int originy = rect.y();
    int desty = 0;
    if (originy < 0) {
        desty = -originy;
        originy = 0;
    }
    int endy = rect.y() + rect.height();
    if (endy > m_size.height())
        endy = m_size.height();
    int numRows = endy - originy;

    unsigned srcBytesPerRow = 4 * m_size.width();
    unsigned destBytesPerRow = 4 * rect.width();

    // ::create ensures that all ImageBuffers have valid data, so we don't need to check it here.
    unsigned char* srcRows = reinterpret_cast<unsigned char*>(m_data.m_data) + originy * srcBytesPerRow + originx * 4;
    unsigned char* destRows = data + desty * destBytesPerRow + destx * 4;
    for (int y = 0; y < numRows; ++y) {
        for (int x = 0; x < numColumns; x++) {
            int basex = x * 4;
            if (unsigned char alpha = srcRows[basex + 3]) {
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

void ImageBuffer::putImageData(ImageData* source, const IntRect& sourceRect, const IntPoint& destPoint)
{
    ASSERT(sourceRect.width() > 0);
    ASSERT(sourceRect.height() > 0);

    int originx = sourceRect.x();
    int destx = destPoint.x() + sourceRect.x();
    ASSERT(destx >= 0);
    ASSERT(destx < m_size.width());
    ASSERT(originx >= 0);
    ASSERT(originx <= sourceRect.right());

    int endx = destPoint.x() + sourceRect.right();
    ASSERT(endx <= m_size.width());

    int numColumns = endx - destx;

    int originy = sourceRect.y();
    int desty = destPoint.y() + sourceRect.y();
    ASSERT(desty >= 0);
    ASSERT(desty < m_size.height());
    ASSERT(originy >= 0);
    ASSERT(originy <= sourceRect.bottom());

    int endy = destPoint.y() + sourceRect.bottom();
    ASSERT(endy <= m_size.height());
    int numRows = endy - desty;

    unsigned srcBytesPerRow = 4 * source->width();
    unsigned destBytesPerRow = 4 * m_size.width();

    unsigned char* srcRows = source->data()->data()->data() + originy * srcBytesPerRow + originx * 4;
    unsigned char* destRows = reinterpret_cast<unsigned char*>(m_data.m_data) + desty * destBytesPerRow + destx * 4;
    for (int y = 0; y < numRows; ++y) {
        for (int x = 0; x < numColumns; x++) {
            int basex = x * 4;
            unsigned char alpha = srcRows[basex + 3];
            if (alpha != 255) {
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

    size_t width = CGImageGetWidth(image.get());
    size_t height = CGImageGetHeight(image.get());

    OwnArrayPtr<uint32_t> imageData(new uint32_t[width * height]);
    if (!imageData)
        return "data:,";
    
    RetainPtr<CGImageRef> transformedImage(AdoptCF, CGBitmapContextCreateImage(context()->platformContext()));
    if (!transformedImage)
        return "data:,";

    RetainPtr<CFMutableDataRef> transformedImageData(AdoptCF, CFDataCreateMutable(kCFAllocatorDefault, 0));
    if (!transformedImageData)
        return "data:,";

    RetainPtr<CGImageDestinationRef> imageDestination(AdoptCF, CGImageDestinationCreateWithData(transformedImageData.get(),
        utiFromMIMEType(mimeType).get(), 1, 0));
    if (!imageDestination)
        return "data:,";

    CGImageDestinationAddImage(imageDestination.get(), transformedImage.get(), 0);
    CGImageDestinationFinalize(imageDestination.get());

    Vector<char> in;
    in.append(CFDataGetBytePtr(transformedImageData.get()), CFDataGetLength(transformedImageData.get()));

    Vector<char> out;
    base64Encode(in, out);
    out.append('\0');

    return String::format("data:%s;base64,%s", mimeType.utf8().data(), out.data());
}

} // namespace WebCore
