/*
 * Copyright (c) 2008, Google Inc. All rights reserved.
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) 2010 Torch Mobile (Beijing) Co. Ltd. All rights reserved.
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
#include "Canvas2DLayerChromium.h"
#include "GrContext.h"
#include "GraphicsContext.h"
#include "ImageData.h"
#include "JPEGImageEncoder.h"
#include "MIMETypeRegistry.h"
#include "PNGImageEncoder.h"
#include "PlatformContextSkia.h"
#include "SharedGraphicsContext3D.h"
#include "SkColorPriv.h"
#include "SkGpuDevice.h"
#include "SkiaUtils.h"
#include "WEBPImageEncoder.h"

#include <wtf/text/WTFString.h>

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

static SkCanvas* createAcceleratedCanvas(const IntSize& size, ImageBufferData* data)
{
    GraphicsContext3D* context3D = SharedGraphicsContext3D::get();
    if (!context3D)
        return 0;
    GrContext* gr = context3D->grContext();
    if (!gr)
        return 0;
    gr->resetContext();
    GrTextureDesc desc;
    desc.fFlags = kRenderTarget_GrTextureFlagBit;
    desc.fAALevel = kNone_GrAALevel;
    desc.fWidth = size.width();
    desc.fHeight = size.height();
    desc.fConfig = kRGBA_8888_GrPixelConfig;
    SkAutoTUnref<GrTexture> texture(gr->createUncachedTexture(desc, 0, 0));
    if (!texture.get())
        return 0;
    SkCanvas* canvas = new SkCanvas();
    canvas->setDevice(new SkGpuDevice(gr, texture.get()))->unref();
    data->m_platformContext.setGraphicsContext3D(context3D);
#if USE(ACCELERATED_COMPOSITING)
    data->m_platformLayer = Canvas2DLayerChromium::create(context3D);
    data->m_platformLayer->setTextureId(texture.get()->getTextureHandle());
#endif
    return canvas;
}

ImageBuffer::ImageBuffer(const IntSize& size, ColorSpace, RenderingMode renderingMode, bool& success)
    : m_data(size)
    , m_size(size)
{
    OwnPtr<SkCanvas> canvas;

    if (renderingMode == Accelerated)
        canvas = adoptPtr(createAcceleratedCanvas(size, &m_data));

    if (!canvas)
        canvas = adoptPtr(skia::TryCreateBitmapCanvas(size.width(), size.height(), false));

    if (!canvas) {
        success = false;
        return;
    }

    m_data.m_canvas = canvas.release();
    m_data.m_platformContext.setCanvas(m_data.m_canvas.get());
    m_context = adoptPtr(new GraphicsContext(&m_data.m_platformContext));
    m_context->platformContext()->setDrawingToImageBuffer(true);

    // Make the background transparent. It would be nice if this wasn't
    // required, but the canvas is currently filled with the magic transparency
    // color. Can we have another way to manage this?
    m_data.m_canvas->drawARGB(0, 0, 0, 0, SkXfermode::kClear_Mode);

    success = true;
}

ImageBuffer::~ImageBuffer()
{
#if USE(ACCELERATED_COMPOSITING)
    if (m_data.m_platformLayer)
        m_data.m_platformLayer->setTextureId(0);
#endif
}

GraphicsContext* ImageBuffer::context() const
{
    return m_context.get();
}

size_t ImageBuffer::dataSize() const
{
    return m_size.width() * m_size.height() * 4;
}

PassRefPtr<Image> ImageBuffer::copyImage(BackingStoreCopy copyBehavior) const
{
    return BitmapImageSingleFrameSkia::create(*m_data.m_platformContext.bitmap(), copyBehavior == CopyBackingStore);
}

PlatformLayer* ImageBuffer::platformLayer() const
{
    return m_data.m_platformLayer.get();
}

void ImageBuffer::clip(GraphicsContext* context, const FloatRect& rect) const
{
    context->platformContext()->beginLayerClippedToImage(rect, this);
}

static bool drawNeedsCopy(GraphicsContext* src, GraphicsContext* dst)
{
    return dst->platformContext()->isDeferred() || src == dst;
}

void ImageBuffer::draw(GraphicsContext* context, ColorSpace styleColorSpace, const FloatRect& destRect, const FloatRect& srcRect,
                       CompositeOperator op, bool useLowQualityScale)
{
    RefPtr<Image> image = BitmapImageSingleFrameSkia::create(*m_data.m_platformContext.bitmap(), drawNeedsCopy(m_context.get(), context));
    context->drawImage(image.get(), styleColorSpace, destRect, srcRect, op, useLowQualityScale);
}

void ImageBuffer::drawPattern(GraphicsContext* context, const FloatRect& srcRect, const AffineTransform& patternTransform,
                              const FloatPoint& phase, ColorSpace styleColorSpace, CompositeOperator op, const FloatRect& destRect)
{
    RefPtr<Image> image = BitmapImageSingleFrameSkia::create(*m_data.m_platformContext.bitmap(), drawNeedsCopy(m_context.get(), context));
    image->drawPattern(context, srcRect, patternTransform, phase, styleColorSpace, op, destRect);
}

void ImageBuffer::platformTransformColorSpace(const Vector<int>& lookUpTable)
{
    // FIXME: Disable color space conversions on accelerated canvases (for now).
    if (m_data.m_platformContext.isAccelerated()) 
        return;

    const SkBitmap& bitmap = *context()->platformContext()->bitmap();
    if (bitmap.isNull())
        return;

    ASSERT(bitmap.config() == SkBitmap::kARGB_8888_Config);
    SkAutoLockPixels bitmapLock(bitmap);
    for (int y = 0; y < m_size.height(); ++y) {
        uint32_t* srcRow = bitmap.getAddr32(0, y);
        for (int x = 0; x < m_size.width(); ++x) {
            SkColor color = SkPMColorToColor(srcRow[x]);
            srcRow[x] = SkPreMultiplyARGB(SkColorGetA(color),
                                          lookUpTable[SkColorGetR(color)],
                                          lookUpTable[SkColorGetG(color)],
                                          lookUpTable[SkColorGetB(color)]);
        }
    }
}

template <Multiply multiplied>
PassRefPtr<ByteArray> getImageData(const IntRect& rect, SkCanvas* canvas,
                                   const IntSize& size)
{
    float area = 4.0f * rect.width() * rect.height();
    if (area > static_cast<float>(std::numeric_limits<int>::max()))
        return 0;

    RefPtr<ByteArray> result = ByteArray::create(rect.width() * rect.height() * 4);

    unsigned char* data = result->data();

    if (rect.x() < 0
        || rect.y() < 0
        || rect.maxX() > size.width()
        || rect.maxY() > size.height())
        memset(data, 0, result->length());

    int originX = rect.x();
    int destX = 0;
    if (originX < 0) {
        destX = -originX;
        originX = 0;
    }
    int endX = rect.maxX();
    if (endX > size.width())
        endX = size.width();
    int numColumns = endX - originX;

    if (numColumns <= 0)
        return result.release();

    int originY = rect.y();
    int destY = 0;
    if (originY < 0) {
        destY = -originY;
        originY = 0;
    }
    int endY = rect.maxY();
    if (endY > size.height())
        endY = size.height();
    int numRows = endY - originY;

    if (numRows <= 0)
        return result.release();

    SkBitmap srcBitmap;
    if (!canvas->readPixels(SkIRect::MakeXYWH(originX, originY, numColumns, numRows), &srcBitmap))
        return result.release();

    unsigned destBytesPerRow = 4 * rect.width();
    unsigned char* destRow = data + destY * destBytesPerRow + destX * 4;

    // Do conversion of byte order and alpha divide (if necessary)
    for (int y = 0; y < numRows; ++y) {
        SkPMColor* srcBitmapRow = srcBitmap.getAddr32(0, y);
        for (int x = 0; x < numColumns; ++x) {
            SkPMColor srcPMColor = srcBitmapRow[x];
            unsigned char* destPixel = &destRow[x * 4];
            if (multiplied == Unmultiplied) {
                unsigned char a = SkGetPackedA32(srcPMColor);
                destPixel[0] = a ? SkGetPackedR32(srcPMColor) * 255 / a : 0;
                destPixel[1] = a ? SkGetPackedG32(srcPMColor) * 255 / a : 0;
                destPixel[2] = a ? SkGetPackedB32(srcPMColor) * 255 / a : 0;
                destPixel[3] = a;
            } else {
                // Input and output are both pre-multiplied, we just need to re-arrange the
                // bytes from the bitmap format to RGBA.
                destPixel[0] = SkGetPackedR32(srcPMColor);
                destPixel[1] = SkGetPackedG32(srcPMColor);
                destPixel[2] = SkGetPackedB32(srcPMColor);
                destPixel[3] = SkGetPackedA32(srcPMColor);
            }
        }
        destRow += destBytesPerRow;
    }

    return result.release();
}

PassRefPtr<ByteArray> ImageBuffer::getUnmultipliedImageData(const IntRect& rect) const
{
    return getImageData<Unmultiplied>(rect, context()->platformContext()->canvas(), m_size);
}

PassRefPtr<ByteArray> ImageBuffer::getPremultipliedImageData(const IntRect& rect) const
{
    return getImageData<Premultiplied>(rect, context()->platformContext()->canvas(), m_size);
}

template <Multiply multiplied>
void putImageData(ByteArray*& source, const IntSize& sourceSize, const IntRect& sourceRect, const IntPoint& destPoint,
                  SkCanvas* canvas, const IntSize& size)
{
    ASSERT(sourceRect.width() > 0);
    ASSERT(sourceRect.height() > 0);

    int originX = sourceRect.x();
    int destX = destPoint.x() + sourceRect.x();
    ASSERT(destX >= 0);
    ASSERT(destX < size.width());
    ASSERT(originX >= 0);
    ASSERT(originX < sourceRect.maxX());

    int endX = destPoint.x() + sourceRect.maxX();
    ASSERT(endX <= size.width());

    int numColumns = endX - destX;

    int originY = sourceRect.y();
    int destY = destPoint.y() + sourceRect.y();
    ASSERT(destY >= 0);
    ASSERT(destY < size.height());
    ASSERT(originY >= 0);
    ASSERT(originY < sourceRect.maxY());

    int endY = destPoint.y() + sourceRect.maxY();
    ASSERT(endY <= size.height());
    int numRows = endY - destY;

    unsigned srcBytesPerRow = 4 * sourceSize.width();
    SkBitmap srcBitmap;
    srcBitmap.setConfig(SkBitmap::kARGB_8888_Config, numColumns, numRows, srcBytesPerRow);
    srcBitmap.setPixels(source->data() + originY * srcBytesPerRow + originX * 4);

    SkCanvas::Config8888 config8888;
    if (multiplied == Premultiplied)
        config8888 = SkCanvas::kRGBA_Premul_Config8888;
    else
        config8888 = SkCanvas::kRGBA_Unpremul_Config8888;

    canvas->writePixels(srcBitmap, destX, destY, config8888);
}

void ImageBuffer::putUnmultipliedImageData(ByteArray* source, const IntSize& sourceSize, const IntRect& sourceRect, const IntPoint& destPoint)
{
    putImageData<Unmultiplied>(source, sourceSize, sourceRect, destPoint, context()->platformContext()->canvas(), m_size);
}

void ImageBuffer::putPremultipliedImageData(ByteArray* source, const IntSize& sourceSize, const IntRect& sourceRect, const IntPoint& destPoint)
{
    putImageData<Premultiplied>(source, sourceSize, sourceRect, destPoint, context()->platformContext()->canvas(), m_size);
}

template <typename T>
static bool encodeImage(T& source, const String& mimeType, const double* quality, Vector<char>* output)
{
    Vector<unsigned char>* encodedImage = reinterpret_cast<Vector<unsigned char>*>(output);

    if (mimeType == "image/jpeg") {
        int compressionQuality = JPEGImageEncoder::DefaultCompressionQuality;
        if (quality && *quality >= 0.0 && *quality <= 1.0)
            compressionQuality = static_cast<int>(*quality * 100 + 0.5);
        if (!JPEGImageEncoder::encode(source, compressionQuality, encodedImage))
            return false;
#if USE(WEBP)
    } else if (mimeType == "image/webp") {
        int compressionQuality = WEBPImageEncoder::DefaultCompressionQuality;
        if (quality && *quality >= 0.0 && *quality <= 1.0)
            compressionQuality = static_cast<int>(*quality * 100 + 0.5);
        if (!WEBPImageEncoder::encode(source, compressionQuality, encodedImage))
            return false;
#endif
    } else {
        if (!PNGImageEncoder::encode(source, encodedImage))
            return false;
        ASSERT(mimeType == "image/png");
    }

    return true;
}

String ImageBuffer::toDataURL(const String& mimeType, const double* quality) const
{
    ASSERT(MIMETypeRegistry::isSupportedImageMIMETypeForEncoding(mimeType));

    Vector<char> encodedImage, base64Data;
    SkDevice* device = context()->platformContext()->canvas()->getDevice();
    if (!encodeImage(device->accessBitmap(false), mimeType, quality, &encodedImage))
        return "data:,";

    base64Encode(encodedImage, base64Data);
    return "data:" + mimeType + ";base64," + base64Data;
}

String ImageDataToDataURL(const ImageData& imageData, const String& mimeType, const double* quality)
{
    ASSERT(MIMETypeRegistry::isSupportedImageMIMETypeForEncoding(mimeType));

    Vector<char> encodedImage, base64Data;
    if (!encodeImage(imageData, mimeType, quality, &encodedImage))
        return "data:,";

    base64Encode(encodedImage, base64Data);
    return "data:" + mimeType + ";base64," + base64Data;
}

} // namespace WebCore
