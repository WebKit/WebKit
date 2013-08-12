/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2008 Holger Hans Peter Freyther
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
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

#include "GraphicsContext.h"
#include "ImageData.h"
#include "MIMETypeRegistry.h"
#include "StillImageQt.h"
#include "TransparencyLayer.h"
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

#include <QBuffer>
#include <QColor>
#include <QImage>
#include <QImageWriter>
#include <QPainter>
#include <QPixmap>
#include <math.h>

namespace WebCore {

ImageBufferData::ImageBufferData(const IntSize& size)
    : m_pixmap(size)
{
    if (m_pixmap.isNull())
        return;

    m_pixmap.fill(QColor(Qt::transparent));

    QPainter* painter = new QPainter;
    m_painter = adoptPtr(painter);

    if (!painter->begin(&m_pixmap))
        return;

    // Since ImageBuffer is used mainly for Canvas, explicitly initialize
    // its painter's pen and brush with the corresponding canvas defaults
    // NOTE: keep in sync with CanvasRenderingContext2D::State
    QPen pen = painter->pen();
    pen.setColor(Qt::black);
    pen.setWidth(1);
    pen.setCapStyle(Qt::FlatCap);
    pen.setJoinStyle(Qt::SvgMiterJoin);
    pen.setMiterLimit(10);
    painter->setPen(pen);
    QBrush brush = painter->brush();
    brush.setColor(Qt::black);
    painter->setBrush(brush);
    painter->setCompositionMode(QPainter::CompositionMode_SourceOver);
    
    m_image = StillImage::createForRendering(&m_pixmap);
}

QImage ImageBufferData::toQImage() const
{
    QPaintEngine* paintEngine = m_pixmap.paintEngine();
    if (!paintEngine || paintEngine->type() != QPaintEngine::Raster)
        return m_pixmap.toImage();

    // QRasterPixmapData::toImage() will deep-copy the backing QImage if there's an active QPainter on it.
    // For performance reasons, we don't want that here, so we temporarily redirect the paint engine.
    QPaintDevice* currentPaintDevice = paintEngine->paintDevice();
    paintEngine->setPaintDevice(0);
    QImage image = m_pixmap.toImage();
    paintEngine->setPaintDevice(currentPaintDevice);
    return image;
}

ImageBuffer::ImageBuffer(const IntSize& size, float /* resolutionScale */, ColorSpace, RenderingMode, bool& success)
    : m_data(size)
    , m_size(size)
    , m_logicalSize(size)
{
    success = m_data.m_painter && m_data.m_painter->isActive();
    if (!success)
        return;

    m_context = adoptPtr(new GraphicsContext(m_data.m_painter.get()));
}

ImageBuffer::~ImageBuffer()
{
}

GraphicsContext* ImageBuffer::context() const
{
    ASSERT(m_data.m_painter->isActive());

    return m_context.get();
}

PassRefPtr<Image> ImageBuffer::copyImage(BackingStoreCopy copyBehavior, ScaleBehavior) const
{
    if (copyBehavior == CopyBackingStore)
        return StillImage::create(m_data.m_pixmap);

    return StillImage::createForRendering(&m_data.m_pixmap);
}

BackingStoreCopy ImageBuffer::fastCopyImageMode()
{
    return DontCopyBackingStore;
}

void ImageBuffer::draw(GraphicsContext* destContext, ColorSpace styleColorSpace, const FloatRect& destRect, const FloatRect& srcRect,
    CompositeOperator op, BlendMode blendMode, bool useLowQualityScale)
{
    if (destContext == context()) {
        // We're drawing into our own buffer.  In order for this to work, we need to copy the source buffer first.
        RefPtr<Image> copy = copyImage(CopyBackingStore);
        destContext->drawImage(copy.get(), ColorSpaceDeviceRGB, destRect, srcRect, op, blendMode, DoNotRespectImageOrientation, useLowQualityScale);
    } else
        destContext->drawImage(m_data.m_image.get(), styleColorSpace, destRect, srcRect, op, blendMode, DoNotRespectImageOrientation, useLowQualityScale);
}

void ImageBuffer::drawPattern(GraphicsContext* destContext, const FloatRect& srcRect, const AffineTransform& patternTransform,
                              const FloatPoint& phase, ColorSpace styleColorSpace, CompositeOperator op, const FloatRect& destRect)
{
    if (destContext == context()) {
        // We're drawing into our own buffer.  In order for this to work, we need to copy the source buffer first.
        RefPtr<Image> copy = copyImage(CopyBackingStore);
        copy->drawPattern(destContext, srcRect, patternTransform, phase, styleColorSpace, op, destRect);
    } else
        m_data.m_image->drawPattern(destContext, srcRect, patternTransform, phase, styleColorSpace, op, destRect);
}

void ImageBuffer::clip(GraphicsContext* context, const FloatRect& floatRect) const
{
    QPixmap* nativeImage = m_data.m_image->nativeImageForCurrentFrame();
    if (!nativeImage)
        return;

    IntRect rect = enclosingIntRect(floatRect);
    QPixmap alphaMask = *nativeImage;

    context->pushTransparencyLayerInternal(rect, 1.0, alphaMask);
}

void ImageBuffer::platformTransformColorSpace(const Vector<int>& lookUpTable)
{
    bool isPainting = m_data.m_painter->isActive();
    if (isPainting)
        m_data.m_painter->end();

    QImage image = m_data.toQImage().convertToFormat(QImage::Format_ARGB32);
    ASSERT(!image.isNull());

    uchar* bits = image.bits();
    const int bytesPerLine = image.bytesPerLine();

    for (int y = 0; y < m_size.height(); ++y) {
        quint32* scanLine = reinterpret_cast_ptr<quint32*>(bits + y * bytesPerLine);
        for (int x = 0; x < m_size.width(); ++x) {
            QRgb& pixel = scanLine[x];
            pixel = qRgba(lookUpTable[qRed(pixel)],
                          lookUpTable[qGreen(pixel)],
                          lookUpTable[qBlue(pixel)],
                          qAlpha(pixel));
        }
    }

    m_data.m_pixmap = QPixmap::fromImage(image);

    if (isPainting)
        m_data.m_painter->begin(&m_data.m_pixmap);
}

static inline void copyColorToRGBA(Color& from, uchar* to)
{
    // Copy from endian dependent 32bit ARGB to endian independent RGBA8888.
    to[0] = from.red();
    to[1] = from.green();
    to[2] = from.blue();
    to[3] = from.alpha();
}

static inline void copyRGBAToColor(const uchar* from, Color& to)
{
    // Copy from endian independent RGBA8888 to endian dependent 32bit ARGB.
    to = Color::createUnchecked(from[0], from[1], from[2], from[3]);
}

template <Multiply multiplied>
PassRefPtr<Uint8ClampedArray> getImageData(const IntRect& rect, const ImageBufferData& imageData, const IntSize& size)
{
    float area = 4.0f * rect.width() * rect.height();
    if (area > static_cast<float>(std::numeric_limits<int>::max()))
        return 0;

    RefPtr<Uint8ClampedArray> result = Uint8ClampedArray::createUninitialized(rect.width() * rect.height() * 4);
    uchar* resultData = result->data();

    if (rect.x() < 0 || rect.y() < 0 || rect.maxX() > size.width() || rect.maxY() > size.height())
        result->zeroFill();

    int originx = rect.x();
    int destx = 0;
    if (originx < 0) {
        destx = -originx;
        originx = 0;
    }
    int endx = rect.maxX();
    if (endx > size.width())
        endx = size.width();
    int numColumns = endx - originx;

    int originy = rect.y();
    int desty = 0;
    if (originy < 0) {
        desty = -originy;
        originy = 0;
    }
    int endy = rect.maxY();
    if (endy > size.height())
        endy = size.height();
    int numRows = endy - originy;

    const unsigned destBytesPerRow = 4 * rect.width();

    // NOTE: For unmultiplied data, we undo the premultiplication below.
    QImage image = imageData.toQImage().convertToFormat(QImage::Format_ARGB32_Premultiplied);

    ASSERT(!image.isNull());

    // The Canvas 2D Context expects RGBA order, while Qt uses 32bit QRgb (ARGB/BGRA).
    for (int y = 0; y < numRows; ++y) {
        // This cast and the calls below relies on both QRgb and WebCore::RGBA32 being 32bit ARGB.
        const unsigned* srcRow = reinterpret_cast<const unsigned*>(image.constScanLine(originy + y)) + originx;
        uchar* destRow = resultData + (desty + y) * destBytesPerRow + destx * 4;
        for (int x = 0; x < numColumns; x++, srcRow++, destRow += 4) {
            Color pixelColor;
            if (multiplied == Unmultiplied)
                pixelColor = colorFromPremultipliedARGB(*srcRow);
            else
                pixelColor = Color(*srcRow);
            copyColorToRGBA(pixelColor, destRow);
        }
    }

    return result.release();
}

PassRefPtr<Uint8ClampedArray> ImageBuffer::getUnmultipliedImageData(const IntRect& rect, CoordinateSystem) const
{
    return getImageData<Unmultiplied>(rect, m_data, m_size);
}

PassRefPtr<Uint8ClampedArray> ImageBuffer::getPremultipliedImageData(const IntRect& rect, CoordinateSystem) const
{
    return getImageData<Premultiplied>(rect, m_data, m_size);
}

void ImageBuffer::putByteArray(Multiply multiplied, Uint8ClampedArray* source, const IntSize& sourceSize, const IntRect& sourceRect, const IntPoint& destPoint, CoordinateSystem)
{
    ASSERT(sourceRect.width() > 0);
    ASSERT(sourceRect.height() > 0);

    int originx = sourceRect.x();
    int destx = destPoint.x() + sourceRect.x();
    ASSERT(destx >= 0);
    ASSERT(destx < m_size.width());
    ASSERT(originx >= 0);
    ASSERT(originx <= sourceRect.maxX());

    int endx = destPoint.x() + sourceRect.maxX();
    ASSERT(endx <= m_size.width());

    int numColumns = endx - destx;

    int originy = sourceRect.y();
    int desty = destPoint.y() + sourceRect.y();
    ASSERT(desty >= 0);
    ASSERT(desty < m_size.height());
    ASSERT(originy >= 0);
    ASSERT(originy <= sourceRect.maxY());

    int endy = destPoint.y() + sourceRect.maxY();
    ASSERT(endy <= m_size.height());
    int numRows = endy - desty;

    const unsigned srcBytesPerRow = 4 * sourceSize.width();

    // NOTE: For unmultiplied input data, we do the premultiplication below.
    QImage image(numColumns, numRows, QImage::Format_ARGB32_Premultiplied);

    unsigned* destData = reinterpret_cast<unsigned*>(image.bits());
    const uchar* srcData = source->data();

    for (int y = 0; y < numRows; ++y) {
        const uchar* srcRow = srcData + (originy + y) * srcBytesPerRow + originx * 4;
        // This cast and the calls below relies on both QRgb and WebCore::RGBA32 being 32bit ARGB.
        unsigned* destRow = destData + y * numColumns;
        for (int x = 0; x < numColumns; x++, srcRow += 4, destRow++) {
            Color pixelColor;
            copyRGBAToColor(srcRow, pixelColor);
            if (multiplied == Unmultiplied)
                *destRow = premultipliedARGBFromColor(pixelColor);
            else
                *destRow = pixelColor.rgb();
        }
    }

    bool isPainting = m_data.m_painter->isActive();
    if (!isPainting)
        m_data.m_painter->begin(&m_data.m_pixmap);
    else {
        m_data.m_painter->save();

        // putImageData() should be unaffected by painter state
        m_data.m_painter->resetTransform();
        m_data.m_painter->setOpacity(1.0);
        m_data.m_painter->setClipping(false);
    }

    m_data.m_painter->setCompositionMode(QPainter::CompositionMode_Source);
    m_data.m_painter->drawImage(destx, desty, image);

    if (!isPainting)
        m_data.m_painter->end();
    else
        m_data.m_painter->restore();
}

static bool encodeImage(const QPixmap& pixmap, const String& format, const double* quality, QByteArray& data)
{
    int compressionQuality = 100;
    if (quality && *quality >= 0.0 && *quality <= 1.0)
        compressionQuality = static_cast<int>(*quality * 100 + 0.5);

    QBuffer buffer(&data);
    buffer.open(QBuffer::WriteOnly);
    bool success = pixmap.save(&buffer, format.utf8().data(), compressionQuality);
    buffer.close();

    return success;
}

String ImageBuffer::toDataURL(const String& mimeType, const double* quality, CoordinateSystem) const
{
    ASSERT(MIMETypeRegistry::isSupportedImageMIMETypeForEncoding(mimeType));

    // QImageWriter does not support mimetypes. It does support Qt image formats (png,
    // gif, jpeg..., xpm) so skip the image/ to get the Qt image format used to encode
    // the m_pixmap image.

    QByteArray data;
    if (!encodeImage(m_data.m_pixmap, mimeType.substring(sizeof "image"), quality, data))
        return "data:,";

    return "data:" + mimeType + ";base64," + data.toBase64().data();
}

}
