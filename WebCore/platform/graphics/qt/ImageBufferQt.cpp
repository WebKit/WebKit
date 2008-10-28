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
#include <QImageWriter>
#include <QPainter>
#include <QPixmap>

namespace WebCore {

ImageBufferData::ImageBufferData(const IntSize& size)
    : m_pixmap(size)
{
    px.fill(QColor(Qt::transparent));
    m_painter.set(new QPainter(&m_pixmap));
}

ImageBuffer::ImageBuffer(const IntSize& size, bool grayScale, bool& success)
    : m_platform(size)
    , m_size(size)
{
    m_context.set(new GraphicsContext(platform.m_painter));
    success = true;
}

ImageBuffer::~ImageBuffer()
{
}

GraphicsContext* ImageBuffer::context() const
{
    if (!m_painter->isActive())
        m_painter->begin(&m_pixmap);

    return m_context.get();
}

Image* ImageBuffer::image() const
{
    if (!m_image) {
        // It's assumed that if image() is called, the actual rendering to the
        // GraphicsContext must be done.
        ASSERT(context());
        m_image = StillImage::create(m_pixmap);
    }

    return m_image.get();
}

PassRefPtr<ImageData> ImageBuffer::getImageData(const IntRect&) const
{
    notImplemented();
    return 0;
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

    if (!m_pixmap.save(&buffer, mimeType.substring(sizeof "image").utf8().data()))
        return "data:,";

    buffer.close();
    return String::format("data:%s;base64,%s", mimeType.utf8().data(), data.toBase64().data());
}

}
