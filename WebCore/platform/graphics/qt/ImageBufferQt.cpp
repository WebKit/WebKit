/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2008 Holger Hans Peter Freyther
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
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

#include "CString.h"
#include "GraphicsContext.h"
#include "ImageData.h"
#include "MIMETypeRegistry.h"
#include "StillImageQt.h"

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
    m_pixmap.fill(QColor(Qt::transparent));

    QPainter* painter = new QPainter(&m_pixmap);
    m_painter.set(painter);

    // Since ImageBuffer is used mainly for Canvas, explicitly initialize
    // its painter's pen and brush with the corresponding canvas defaults
    // NOTE: keep in sync with CanvasRenderingContext2D::State
    QPen pen = painter->pen();
    pen.setColor(Qt::black);
    pen.setWidth(1);
    pen.setCapStyle(Qt::FlatCap);
    pen.setJoinStyle(Qt::MiterJoin);
    pen.setMiterLimit(10);
    painter->setPen(pen);
    QBrush brush = painter->brush();
    brush.setColor(Qt::black);
    painter->setBrush(brush);
    painter->setCompositionMode(QPainter::CompositionMode_SourceOver);
}

ImageBuffer::ImageBuffer(const IntSize& size, ImageColorSpace, bool& success)
    : m_data(size)
    , m_size(size)
{
    m_context.set(new GraphicsContext(m_data.m_painter.get()));
    success = true;
}

ImageBuffer::~ImageBuffer()
{
}

GraphicsContext* ImageBuffer::context() const
{
    ASSERT(m_data.m_painter->isActive());

    return m_context.get();
}

Image* ImageBuffer::image() const
{
    if (!m_image) {
        // It's assumed that if image() is called, the actual rendering to the
        // GraphicsContext must be done.
        ASSERT(context());
        m_image = StillImage::create(m_data.m_pixmap);
    }

    return m_image.get();
}

void ImageBuffer::platformTransformColorSpace(const Vector<int>& lookUpTable)
{
    bool isPainting = m_data.m_painter->isActive();
    if (isPainting)
        m_data.m_painter->end();

    QImage image = m_data.m_pixmap.toImage().convertToFormat(QImage::Format_ARGB32);
    ASSERT(!image.isNull());

    for (int y = 0; y < m_size.height(); ++y) {
        for (int x = 0; x < m_size.width(); x++) {
            QRgb value = image.pixel(x, y);
            value = qRgba(lookUpTable[qRed(value)],
                          lookUpTable[qGreen(value)],
                          lookUpTable[qBlue(value)],
                          qAlpha(value));
            image.setPixel(x, y, value);
        }
    }

    m_data.m_pixmap = QPixmap::fromImage(image);

    if (isPainting)
        m_data.m_painter->begin(&m_data.m_pixmap);
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

    QImage image = imageData.m_pixmap.toImage();
    if (multiplied == Unmultiplied)
        image = image.convertToFormat(QImage::Format_ARGB32);
    else
        image = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);

    ASSERT(!image.isNull());

    unsigned destBytesPerRow = 4 * rect.width();
    unsigned char* destRows = data + desty * destBytesPerRow + destx * 4;
    for (int y = 0; y < numRows; ++y) {
        for (int x = 0; x < numColumns; x++) {
            QRgb value = image.pixel(x + originx, y + originy);
            int basex = x * 4;

            destRows[basex] = qRed(value);
            destRows[basex + 1] = qGreen(value);
            destRows[basex + 2] = qBlue(value);
            destRows[basex + 3] = qAlpha(value);
        }
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
void putImageData(ImageData*& source, const IntRect& sourceRect, const IntPoint& destPoint, ImageBufferData& data, const IntSize& size)
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

    bool isPainting = data.m_painter->isActive();
    if (isPainting)
        data.m_painter->end();

    QImage image = data.m_pixmap.toImage();
    if (multiplied == Unmultiplied)
        image = image.convertToFormat(QImage::Format_ARGB32);
    else
        image = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);

    unsigned char* srcRows = source->data()->data()->data() + originy * srcBytesPerRow + originx * 4;
    for (int y = 0; y < numRows; ++y) {
        quint32* scanLine = reinterpret_cast<quint32*>(image.scanLine(y + desty));
        for (int x = 0; x < numColumns; x++) {
            // ImageData stores the pixels in RGBA while QImage is ARGB
            quint32 pixel = reinterpret_cast<quint32*>(srcRows + 4 * x)[0];
            pixel = ((pixel << 16) & 0xff0000) | ((pixel >> 16) & 0xff) | (pixel & 0xff00ff00);
            scanLine[x + destx] = pixel;
        }

        srcRows += srcBytesPerRow;
    }

    data.m_pixmap = QPixmap::fromImage(image);

    if (isPainting)
        data.m_painter->begin(&data.m_pixmap);
}

void ImageBuffer::putUnmultipliedImageData(ImageData* source, const IntRect& sourceRect, const IntPoint& destPoint)
{
    putImageData<Unmultiplied>(source, sourceRect, destPoint, m_data, m_size);
}

void ImageBuffer::putPremultipliedImageData(ImageData* source, const IntRect& sourceRect, const IntPoint& destPoint)
{
    putImageData<Premultiplied>(source, sourceRect, destPoint, m_data, m_size);
}

// We get a mimeType here but QImageWriter does not support mimetypes but
// only formats (png, gif, jpeg..., xpm). So assume we get image/ as image
// mimetypes and then remove the image/ to get the Qt format.
String ImageBuffer::toDataURL(const String& mimeType) const
{
    ASSERT(MIMETypeRegistry::isSupportedImageMIMETypeForEncoding(mimeType));

    if (!mimeType.startsWith("image/"))
        return "data:,";

    // prepare our target
    QByteArray data;
    QBuffer buffer(&data);
    buffer.open(QBuffer::WriteOnly);

    if (!m_data.m_pixmap.save(&buffer, mimeType.substring(sizeof "image").utf8().data()))
        return "data:,";

    buffer.close();
    return String::format("data:%s;base64,%s", mimeType.utf8().data(), data.toBase64().data());
}

}
