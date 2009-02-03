/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2008 Holger Hans Peter Freyther
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
#include "NotImplemented.h"
#include "StillImageQt.h"

#include <QBuffer>
#include <QColor>
#include <QImage>
#include <QImageWriter>
#include <QPainter>
#include <QPixmap>

namespace WebCore {

ImageBufferData::ImageBufferData(const IntSize& size)
    : m_pixmap(size)
{
    m_pixmap.fill(QColor(Qt::transparent));
    m_painter.set(new QPainter(&m_pixmap));
}

ImageBuffer::ImageBuffer(const IntSize& size, bool grayScale, bool& success)
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

    QImage image;
    if (m_data.m_pixmap.toImage().format() != QImage::Format_ARGB32)
        image = m_data.m_pixmap.toImage().convertToFormat(QImage::Format_ARGB32);
    else
        image = m_data.m_pixmap.toImage();
    ASSERT(image);

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

void ImageBuffer::putImageData(ImageData*, const IntRect&, const IntPoint&)
{
    notImplemented();
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
