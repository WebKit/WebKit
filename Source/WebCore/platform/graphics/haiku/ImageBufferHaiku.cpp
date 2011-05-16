/*
 * Copyright (C) 2010 Stephan Aßmus <superstippi@gmx.de>
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
#include "GraphicsContext.h"
#include "ImageData.h"
#include "MIMETypeRegistry.h"
#include "StillImageHaiku.h"
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>
#include <BitmapStream.h>
#include <String.h>
#include <TranslatorRoster.h>

namespace WebCore {

ImageBufferData::ImageBufferData(const IntSize& size)
    : m_bitmap(BRect(0, 0, size.width() - 1, size.height() - 1), B_RGBA32, true)
    , m_view(m_bitmap.Bounds(), "WebKit ImageBufferData", 0, 0)
{
    // Always keep the bitmap locked, we are the only client.
    m_bitmap.Lock();
    m_bitmap.AddChild(&m_view);

    // Fill with completely transparent color.
    memset(m_bitmap.Bits(), 0, m_bitmap.BitsLength());

    // Since ImageBuffer is used mainly for Canvas, explicitly initialize
    // its view's graphics state with the corresponding canvas defaults
    // NOTE: keep in sync with CanvasRenderingContext2D::State
    m_view.SetLineMode(B_BUTT_CAP, B_MITER_JOIN, 10);
    m_view.SetDrawingMode(B_OP_ALPHA);
    m_view.SetBlendingMode(B_PIXEL_ALPHA, B_ALPHA_COMPOSITE);
}

ImageBufferData::~ImageBufferData()
{
    // Remove the view from the bitmap, keeping it from being free'd twice.
    m_view.RemoveSelf();
    m_bitmap.Unlock();
}

ImageBuffer::ImageBuffer(const IntSize& size, ImageColorSpace imageColorSpace, RenderingMode, bool& success)
    : m_data(size)
    , m_size(size)
{
    m_context.set(new GraphicsContext(&m_data.m_view));
    success = true;
}

ImageBuffer::~ImageBuffer()
{
}

size_t ImageBuffer::dataSize() const
{
    return m_size.width() * m_size.height() * 4;
}

GraphicsContext* ImageBuffer::context() const
{
    ASSERT(m_data.m_view.Window());

    return m_context.get();
}

Image* ImageBuffer::image() const
{
    if (!m_image) {
        // It's assumed that if image() is called, the actual rendering to the
        // GraphicsContext must be done.
        ASSERT(context());
        m_data.m_view.Sync();
        m_image = StillImage::create(m_data.m_bitmap);
    }

    return m_image.get();
}

void ImageBuffer::platformTransformColorSpace(const Vector<int>& lookUpTable)
{
    uint8* rowData = reinterpret_cast<uint8*>(m_data.m_bitmap.Bits());
    unsigned bytesPerRow = m_data.m_bitmap.BytesPerRow();
    unsigned rows = m_size.height();
    unsigned columns = m_size.width();
    for (unsigned y = 0; y < rows; y++) {
        uint8* pixel = rowData;
        for (unsigned x = 0; x < columns; x++) {
            // lookUpTable doesn't seem to support a LUT for each color channel
            // separately (judging from the other ports). We don't need to
            // convert from/to pre-multiplied color space since BBitmap storage
            // is not pre-multiplied.
            pixel[0] = lookUpTable[pixel[0]];
            pixel[1] = lookUpTable[pixel[1]];
            pixel[2] = lookUpTable[pixel[2]];
            // alpha stays unmodified.
            pixel += 4;
        }
        rowData += bytesPerRow;
    }
}

static inline void convertFromData(const uint8* sourceRows, unsigned sourceBytesPerRow,
                                   uint8* destRows, unsigned destBytesPerRow,
                                   unsigned rows, unsigned columns)
{
    for (unsigned y = 0; y < rows; y++) {
        const uint8* sourcePixel = sourceRows;
        uint8* destPixel = destRows;
        for (unsigned x = 0; x < columns; x++) {
            // RGBA -> BGRA or BGRA -> RGBA
            destPixel[0] = sourcePixel[2];
            destPixel[1] = sourcePixel[1];
            destPixel[2] = sourcePixel[0];
            destPixel[3] = sourcePixel[3];
            destPixel += 4;
            sourcePixel += 4;
        }
        sourceRows += sourceBytesPerRow;
        destRows += destBytesPerRow;
    }
}

static inline void convertFromInternalData(const uint8* sourceRows, unsigned sourceBytesPerRow,
                                           uint8* destRows, unsigned destBytesPerRow,
                                           unsigned rows, unsigned columns,
                                           bool premultiplied)
{
    if (premultiplied) {
        // Internal storage is not pre-multiplied, pre-multiply on the fly.
        for (unsigned y = 0; y < rows; y++) {
            const uint8* sourcePixel = sourceRows;
            uint8* destPixel = destRows;
            for (unsigned x = 0; x < columns; x++) {
                // RGBA -> BGRA or BGRA -> RGBA
                destPixel[0] = static_cast<uint16>(sourcePixel[2]) * sourcePixel[3] / 255;
                destPixel[1] = static_cast<uint16>(sourcePixel[1]) * sourcePixel[3] / 255;
                destPixel[2] = static_cast<uint16>(sourcePixel[0]) * sourcePixel[3] / 255;
                destPixel[3] = sourcePixel[3];
                destPixel += 4;
                sourcePixel += 4;
            }
            sourceRows += sourceBytesPerRow;
            destRows += destBytesPerRow;
        }
    } else {
        convertFromData(sourceRows, sourceBytesPerRow,
                        destRows, destBytesPerRow,
                        rows, columns);
    }
}

static inline void convertToInternalData(const uint8* sourceRows, unsigned sourceBytesPerRow,
                                         uint8* destRows, unsigned destBytesPerRow,
                                         unsigned rows, unsigned columns,
                                         bool premultiplied)
{
    if (premultiplied) {
        // Internal storage is not pre-multiplied, de-multiply source data.
        for (unsigned y = 0; y < rows; y++) {
            const uint8* sourcePixel = sourceRows;
            uint8* destPixel = destRows;
            for (unsigned x = 0; x < columns; x++) {
                // RGBA -> BGRA or BGRA -> RGBA
                if (sourcePixel[3]) {
                    destPixel[0] = static_cast<uint16>(sourcePixel[2]) * 255 / sourcePixel[3];
                    destPixel[1] = static_cast<uint16>(sourcePixel[1]) * 255 / sourcePixel[3];
                    destPixel[2] = static_cast<uint16>(sourcePixel[0]) * 255 / sourcePixel[3];
                    destPixel[3] = sourcePixel[3];
                } else {
                    destPixel[0] = 0;
                    destPixel[1] = 0;
                    destPixel[2] = 0;
                    destPixel[3] = 0;
                }
                destPixel += 4;
                sourcePixel += 4;
            }
            sourceRows += sourceBytesPerRow;
            destRows += destBytesPerRow;
        }
    } else {
        convertFromData(sourceRows, sourceBytesPerRow,
                        destRows, destBytesPerRow,
                        rows, columns);
    }
}

static PassRefPtr<ImageData> getImageData(const IntRect& rect, const ImageBufferData& imageData, const IntSize& size, bool premultiplied)
{
    RefPtr<ImageData> result = ImageData::create(IntSize(rect.width(), rect.height()));
    unsigned char* data = result->data()->data()->data();

    // If the destination image is larger than the source image, the outside
    // regions need to be transparent. This way is simply, although with a
    // a slight overhead for the inside region.
    if (rect.x() < 0 || rect.y() < 0 || (rect.x() + rect.width()) > size.width() || (rect.y() + rect.height()) > size.height())
        memset(data, 0, result->data()->length());

    // If the requested image is outside the source image, we can return at
    // this point.
    if (rect.x() > size.width() || rect.y() > size.height() || rect.right() < 0 || rect.bottom() < 0)
        return result.release();

    // Now we know there must be an intersection rect which we need to extract.
    BRect sourceRect(0, 0, size.width() - 1, size.height() - 1);
    sourceRect = BRect(rect) & sourceRect;

    unsigned destBytesPerRow = 4 * rect.width();
    unsigned char* destRows = data;
    // Offset the destination pointer to point at the first pixel of the
    // intersection rect.
    destRows += (rect.x() - static_cast<int>(sourceRect.left)) * 4
        + (rect.y() - static_cast<int>(sourceRect.top)) * destBytesPerRow;

    const uint8* sourceRows = reinterpret_cast<const uint8*>(imageData.m_bitmap.Bits());
    uint32 sourceBytesPerRow = imageData.m_bitmap.BytesPerRow();
    // Offset the source pointer to point at the first pixel of the
    // intersection rect.
    sourceRows += static_cast<int>(sourceRect.left) * 4
        + static_cast<int>(sourceRect.top) * sourceBytesPerRow;

    unsigned rows = sourceRect.IntegerHeight() + 1;
    unsigned columns = sourceRect.IntegerWidth() + 1;
    convertFromInternalData(sourceRows, sourceBytesPerRow, destRows, destBytesPerRow,
        rows, columns, premultiplied);

    return result.release();
}


PassRefPtr<ImageData> ImageBuffer::getUnmultipliedImageData(const IntRect& rect) const
{
    // Make sure all asynchronous drawing has finished
    m_data.m_view.Sync();
    return getImageData(rect, m_data, m_size, false);
}

PassRefPtr<ImageData> ImageBuffer::getPremultipliedImageData(const IntRect& rect) const
{
    // Make sure all asynchronous drawing has finished
    m_data.m_view.Sync();
    return getImageData(rect, m_data, m_size, true);
}

static void putImageData(ImageData* source, const IntRect& sourceRect, const IntPoint& destPoint, ImageBufferData& imageData, const IntSize& size, bool premultiplied)
{
    // If the source image is outside the destination image, we can return at
    // this point.
    // FIXME: Check if this isn't already done in WebCore.
    if (destPoint.x() > size.width() || destPoint.y() > size.height()
        || destPoint.x() + sourceRect.width() < 0
        || destPoint.y() + sourceRect.height() < 0) {
        return;
    }

    const unsigned char* sourceRows = source->data()->data()->data();
    unsigned sourceBytesPerRow = 4 * source->width();
    // Offset the source pointer to the first pixel of the source rect.
    sourceRows += sourceRect.x() * 4 + sourceRect.y() * sourceBytesPerRow;

    // We know there must be an intersection rect.
    BRect destRect(destPoint.x(), destPoint.y(), sourceRect.width() - 1, sourceRect.height() - 1);
    destRect = destRect & BRect(0, 0, size.width() - 1, size.height() - 1);

    unsigned char* destRows = reinterpret_cast<unsigned char*>(imageData.m_bitmap.Bits());
    uint32 destBytesPerRow = imageData.m_bitmap.BytesPerRow();
    // Offset the source pointer to point at the first pixel of the
    // intersection rect.
    destRows += static_cast<int>(destRect.left) * 4
        + static_cast<int>(destRect.top) * destBytesPerRow;

    unsigned rows = sourceRect.height();
    unsigned columns = sourceRect.width();
    convertToInternalData(sourceRows, sourceBytesPerRow, destRows, destBytesPerRow,
        rows, columns, premultiplied);
}

void ImageBuffer::putUnmultipliedImageData(ImageData* source, const IntRect& sourceRect, const IntPoint& destPoint)
{
    // Make sure all asynchronous drawing has finished
    m_data.m_view.Sync();
    putImageData(source, sourceRect, destPoint, m_data, m_size, false);
}

void ImageBuffer::putPremultipliedImageData(ImageData* source, const IntRect& sourceRect, const IntPoint& destPoint)
{
    // Make sure all asynchronous drawing has finished
    m_data.m_view.Sync();
    putImageData(source, sourceRect, destPoint, m_data, m_size, true);
}

String ImageBuffer::toDataURL(const String& mimeType, const double*) const
{
    if (!MIMETypeRegistry::isSupportedImageMIMETypeForEncoding(mimeType))
        return "data:,";

    BString mimeTypeString(mimeType);

    uint32 translatorType = 0;

    BTranslatorRoster* roster = BTranslatorRoster::Default();
    translator_id* translators;
    int32 translatorCount;
    roster->GetAllTranslators(&translators, &translatorCount);
    for (int32 i = 0; i < translatorCount; i++) {
        // Skip translators that don't support archived BBitmaps as input data.
        const translation_format* inputFormats;
        int32 formatCount;
        roster->GetInputFormats(translators[i], &inputFormats, &formatCount);
        bool supportsBitmaps = false;
        for (int32 j = 0; j < formatCount; j++) {
            if (inputFormats[j].type == B_TRANSLATOR_BITMAP) {
                supportsBitmaps = true;
                break;
            }
        }
        if (!supportsBitmaps)
            continue;

        const translation_format* outputFormats;
        roster->GetOutputFormats(translators[i], &outputFormats, &formatCount);
        for (int32 j = 0; j < formatCount; j++) {
            if (outputFormats[j].group == B_TRANSLATOR_BITMAP
                && mimeTypeString == outputFormats[j].MIME) {
                translatorType = outputFormats[j].type;
            }
        }
        if (translatorType)
            break;
    }


    BMallocIO translatedStream;
    BBitmap* bitmap = const_cast<BBitmap*>(&m_data.m_bitmap);
        // BBitmapStream doesn't take "const Bitmap*"...
    BBitmapStream bitmapStream(bitmap);
    if (roster->Translate(&bitmapStream, 0, 0, &translatedStream, translatorType,
                          B_TRANSLATOR_BITMAP, mimeType.utf8().data()) != B_OK) {
        bitmapStream.DetachBitmap(&bitmap);
        return "data:,";
    }

    bitmapStream.DetachBitmap(&bitmap);

    Vector<char> encodedBuffer;
    base64Encode(reinterpret_cast<const char*>(translatedStream.Buffer()),
                 translatedStream.BufferLength(), encodedBuffer);

    return "data:" + mimeType + ";base64," + encodedBuffer;
}

} // namespace WebCore

