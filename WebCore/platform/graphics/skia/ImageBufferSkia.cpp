/*
 * Copyright (c) 2008, Google Inc. All rights reserved.
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ImageBuffer.h"

#include "Base64.h"
#include "BitmapImage.h"
#include "BitmapImageSingleFrameSkia.h"
#include "GraphicsContext.h"
#include "ImageData.h"
#include "PlatformContextSkia.h"
#include "PNGImageEncoder.h"
#include "SkColorPriv.h"
#include "SkiaUtils.h"

using namespace std;

namespace WebCore {

// We pass a technically-uninitialized canvas to the platform context here since
// the canvas initialization completes in ImageBuffer::ImageBuffer. But
// PlatformContext doesn't actually need to use the object, and this makes all
// the ownership easier to manage.
ImageBufferData::ImageBufferData(const IntSize& size)
    : m_platformContext(0)  // Canvas is set in ImageBuffer constructor.
{
}

ImageBuffer::ImageBuffer(const IntSize& size, ImageColorSpace imageColorSpace, bool& success)
    : m_data(size)
    , m_size(size)
{
    if (!m_data.m_canvas.initialize(size.width(), size.height(), false)) {
        success = false;
        return;
    }

    m_data.m_platformContext.setCanvas(&m_data.m_canvas);
    m_context.set(new GraphicsContext(&m_data.m_platformContext));
#if PLATFORM(WIN_OS)
    m_context->platformContext()->setDrawingToImageBuffer(true);
#endif

    // Make the background transparent. It would be nice if this wasn't
    // required, but the canvas is currently filled with the magic transparency
    // color. Can we have another way to manage this?
    m_data.m_canvas.drawARGB(0, 0, 0, 0, SkXfermode::kClear_Mode);
    success = true;
}

ImageBuffer::~ImageBuffer()
{
}

GraphicsContext* ImageBuffer::context() const
{
    return m_context.get();
}

Image* ImageBuffer::image() const
{
    if (!m_image) {
        // This creates a COPY of the image and will cache that copy. This means
        // that if subsequent operations take place on the context, neither the
        // currently-returned image, nor the results of future image() calls,
        // will contain that operation.
        //
        // This seems silly, but is the way the CG port works: image() is
        // intended to be used only when rendering is "complete."
        m_image = BitmapImageSingleFrameSkia::create(
            *m_data.m_platformContext.bitmap());
    }
    return m_image.get();
}

void ImageBuffer::platformTransformColorSpace(const Vector<int>& lookUpTable)
{
    const SkBitmap& bitmap = *context()->platformContext()->bitmap();
    ASSERT(bitmap.config() == SkBitmap::kARGB_8888_Config);
    SkAutoLockPixels bitmapLock(bitmap);
    for (int y = 0; y < m_size.height(); ++y) {
        uint32_t* srcRow = bitmap.getAddr32(0, y);
        for (int x = 0; x < m_size.width(); ++x) {
            SkColor color = SkPMColorToColor(srcRow[x]);
            srcRow[x] = SkPreMultiplyARGB(lookUpTable[SkColorGetA(color)],
                                          lookUpTable[SkColorGetR(color)],
                                          lookUpTable[SkColorGetG(color)],
                                          lookUpTable[SkColorGetB(color)]);
        }
    }
}

template <Multiply multiplied>
PassRefPtr<ImageData> getImageData(const IntRect& rect, const SkBitmap& bitmap, 
                                   const IntSize& size)
{
    RefPtr<ImageData> result = ImageData::create(rect.width(), rect.height());
    unsigned char* data = result->data()->data()->data();

    if (rect.x() < 0 || rect.y() < 0 ||
        (rect.x() + rect.width()) > size.width() ||
        (rect.y() + rect.height()) > size.height())
        memset(data, 0, result->data()->length());

    int originX = rect.x();
    int destX = 0;
    if (originX < 0) {
        destX = -originX;
        originX = 0;
    }
    int endX = rect.x() + rect.width();
    if (endX > size.width())
        endX = size.width();
    int numColumns = endX - originX;

    int originY = rect.y();
    int destY = 0;
    if (originY < 0) {
        destY = -originY;
        originY = 0;
    }
    int endY = rect.y() + rect.height();
    if (endY > size.height())
        endY = size.height();
    int numRows = endY - originY;

    ASSERT(bitmap.config() == SkBitmap::kARGB_8888_Config);
    SkAutoLockPixels bitmapLock(bitmap);

    unsigned destBytesPerRow = 4 * rect.width();
    unsigned char* destRow = data + destY * destBytesPerRow + destX * 4;

    for (int y = 0; y < numRows; ++y) {
        uint32_t* srcRow = bitmap.getAddr32(originX, originY + y);
        for (int x = 0; x < numColumns; ++x) {
            unsigned char* destPixel = &destRow[x * 4];
            if (multiplied == Unmultiplied) {
                SkColor color = srcRow[x];
                unsigned a = SkColorGetA(color);
                destPixel[0] = a ? SkColorGetR(color) * 255 / a : 0;
                destPixel[1] = a ? SkColorGetG(color) * 255 / a : 0;
                destPixel[2] = a ? SkColorGetB(color) * 255 / a : 0;
                destPixel[3] = a;
            } else {
                // Input and output are both pre-multiplied, we just need to re-arrange the
                // bytes from the bitmap format to RGBA.
                destPixel[0] = SkGetPackedR32(srcRow[x]);
                destPixel[1] = SkGetPackedG32(srcRow[x]);
                destPixel[2] = SkGetPackedB32(srcRow[x]);
                destPixel[3] = SkGetPackedA32(srcRow[x]);
            }
        }
        destRow += destBytesPerRow;
    }

    return result;
}

PassRefPtr<ImageData> ImageBuffer::getUnmultipliedImageData(const IntRect& rect) const
{
    return getImageData<Unmultiplied>(rect, *context()->platformContext()->bitmap(), m_size);
}

PassRefPtr<ImageData> ImageBuffer::getPremultipliedImageData(const IntRect& rect) const
{
    return getImageData<Premultiplied>(rect, *context()->platformContext()->bitmap(), m_size);
}

template <Multiply multiplied>
void putImageData(ImageData*& source, const IntRect& sourceRect, const IntPoint& destPoint, 
                  const SkBitmap& bitmap, const IntSize& size)
{
    ASSERT(sourceRect.width() > 0);
    ASSERT(sourceRect.height() > 0);

    int originX = sourceRect.x();
    int destX = destPoint.x() + sourceRect.x();
    ASSERT(destX >= 0);
    ASSERT(destX < size.width());
    ASSERT(originX >= 0);
    ASSERT(originX < sourceRect.right());

    int endX = destPoint.x() + sourceRect.right();
    ASSERT(endX <= size.width());

    int numColumns = endX - destX;

    int originY = sourceRect.y();
    int destY = destPoint.y() + sourceRect.y();
    ASSERT(destY >= 0);
    ASSERT(destY < size.height());
    ASSERT(originY >= 0);
    ASSERT(originY < sourceRect.bottom());

    int endY = destPoint.y() + sourceRect.bottom();
    ASSERT(endY <= size.height());
    int numRows = endY - destY;

    ASSERT(bitmap.config() == SkBitmap::kARGB_8888_Config);
    SkAutoLockPixels bitmapLock(bitmap);

    unsigned srcBytesPerRow = 4 * source->width();

    const unsigned char* srcRow = source->data()->data()->data() + originY * srcBytesPerRow + originX * 4;

    for (int y = 0; y < numRows; ++y) {
        uint32_t* destRow = bitmap.getAddr32(destX, destY + y);
        for (int x = 0; x < numColumns; ++x) {
            const unsigned char* srcPixel = &srcRow[x * 4];
            if (multiplied == Unmultiplied)
                destRow[x] = SkPreMultiplyARGB(srcPixel[3], srcPixel[0],
                                               srcPixel[1], srcPixel[2]);
            else
                destRow[x] = SkPackARGB32(srcPixel[3], srcPixel[0],
                                          srcPixel[1], srcPixel[2]);
        }
        srcRow += srcBytesPerRow;
    }
}

void ImageBuffer::putUnmultipliedImageData(ImageData* source, const IntRect& sourceRect, const IntPoint& destPoint)
{
    putImageData<Unmultiplied>(source, sourceRect, destPoint, *context()->platformContext()->bitmap(), m_size);
}

void ImageBuffer::putPremultipliedImageData(ImageData* source, const IntRect& sourceRect, const IntPoint& destPoint)
{
    putImageData<Premultiplied>(source, sourceRect, destPoint, *context()->platformContext()->bitmap(), m_size);
}

String ImageBuffer::toDataURL(const String&) const
{
    // Encode the image into a vector.
    Vector<unsigned char> pngEncodedData;
    PNGImageEncoder::encode(*context()->platformContext()->bitmap(), &pngEncodedData);

    // Convert it into base64.
    Vector<char> base64EncodedData;
    base64Encode(*reinterpret_cast<Vector<char>*>(&pngEncodedData), base64EncodedData);
    // Append with a \0 so that it's a valid string.
    base64EncodedData.append('\0');

    // And the resulting string.
    return String::format("data:image/png;base64,%s", base64EncodedData.data());
}

} // namespace WebCore
