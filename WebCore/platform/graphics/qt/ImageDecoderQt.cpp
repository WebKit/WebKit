/*
 * Copyright (C) 2006 Friedemann Kleint <fkleint@trolltech.com>
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 *
 * All rights reserved.
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
#include "ImageDecoderQt.h"

#include <QtCore/QByteArray>
#include <QtCore/QBuffer>

#include <QtGui/QImageReader>
#include <qdebug.h>

namespace WebCore {

ImageDecoder* ImageDecoder::create(const SharedBuffer& data)
{
    // We need at least 4 bytes to figure out what kind of image we're dealing with.
    if (data.size() < 4)
        return 0;

    QByteArray bytes = QByteArray::fromRawData(data.data(), data.size());
    QBuffer buffer(&bytes);
    if (!buffer.open(QBuffer::ReadOnly))
        return 0;

    QByteArray imageFormat = QImageReader::imageFormat(&buffer);
    if (imageFormat.isEmpty())
        return 0; // Image format not supported

    return new ImageDecoderQt(imageFormat);
}

// Context, maintains IODevice on a data buffer.
class ImageDecoderQt::ReadContext {
public:

    ReadContext(SharedBuffer* data, Vector<RGBA32Buffer>& target);
    bool read();

    QImageReader *reader() { return &m_reader; }

private:
    enum IncrementalReadResult { IncrementalReadFailed, IncrementalReadPartial, IncrementalReadComplete };
    // Incrementally read an image
    IncrementalReadResult readImageLines(RGBA32Buffer&);

    QByteArray m_data;
    QBuffer m_buffer;
    QImageReader m_reader;

    Vector<RGBA32Buffer> &m_target;
};

ImageDecoderQt::ReadContext::ReadContext(SharedBuffer* data, Vector<RGBA32Buffer> &target)
    : m_data(data->data(), data->size())
    , m_buffer(&m_data)
    , m_reader(&m_buffer)
    , m_target(target)
{
    m_buffer.open(QIODevice::ReadOnly);
}


bool ImageDecoderQt::ReadContext::read()
{
    // Attempt to read out all images
    bool completed = false;
    while (true) {
        if (m_target.isEmpty() || completed) {
            // Start a new image.
            if (!m_reader.canRead())
                return true;

            m_target.append(RGBA32Buffer());
            completed = false;
        }

        // read chunks
        switch (readImageLines(m_target.last())) {
        case IncrementalReadFailed:
            m_target.removeLast();
            return false;
        case IncrementalReadPartial:
            return true;
        case IncrementalReadComplete:
            completed = true;
            //store for next
            const bool supportsAnimation = m_reader.supportsAnimation();

            // No point in readinfg further
            if (!supportsAnimation)
                return true;

            break;
        }
    }
    return true;
}



ImageDecoderQt::ReadContext::IncrementalReadResult
        ImageDecoderQt::ReadContext::readImageLines(RGBA32Buffer& buffer)
{
    // TODO: Implement incremental reading here,
    // set state to reflect complete header, etc.
    // For now, we read the whole image.


    const qint64 startPos = m_buffer.pos();
    QImage img;
    // Oops, failed. Rewind.
    if (!m_reader.read(&img)) {
        m_buffer.seek(startPos);
        return IncrementalReadFailed;
    }

    buffer.setDuration(m_reader.nextImageDelay());
    buffer.setDecodedImage(img);
    buffer.setStatus(RGBA32Buffer::FrameComplete);
    return IncrementalReadComplete;
}

ImageDecoderQt::ImageDecoderQt(const QByteArray& imageFormat)
    : m_imageFormat(imageFormat.constData())
{
}

ImageDecoderQt::~ImageDecoderQt()
{
}

void ImageDecoderQt::setData(SharedBuffer* data, bool allDataReceived)
{
    if (m_failed)
        return;

    // Cache our own new data.
    ImageDecoder::setData(data, allDataReceived);

    // No progressive loading possible
    if (!allDataReceived)
        return;

    ReadContext readContext(data, m_frameBufferCache);

    const bool result =  readContext.read();

    if (!result)
        setFailed();
    else if (!m_frameBufferCache.isEmpty()) {
        QSize imgSize = m_frameBufferCache[0].decodedImage().size();
        setSize(imgSize.width(), imgSize.height());

        if (readContext.reader()->supportsAnimation()) {
            if (readContext.reader()->loopCount() != -1)
                m_loopCount = readContext.reader()->loopCount();
            else
                m_loopCount = 0; //loop forever
        }
    }
}


bool ImageDecoderQt::isSizeAvailable()
{
    return ImageDecoder::isSizeAvailable();
}

size_t ImageDecoderQt::frameCount()
{
    return m_frameBufferCache.size();
}

int ImageDecoderQt::repetitionCount() const
{
    return m_loopCount;
}

String ImageDecoderQt::filenameExtension() const
{
    return m_imageFormat;
};

RGBA32Buffer* ImageDecoderQt::frameBufferAtIndex(size_t index)
{
    if (index >= m_frameBufferCache.size())
        return 0;

    return &m_frameBufferCache[index];
}

void ImageDecoderQt::clearFrameBufferCache(size_t index)
{
    // Currently QImageReader will be asked to read everything. This
    // might change when we read gif images on demand. For now we
    // can have a rather simple implementation.
    if (index > m_frameBufferCache.size())
        return;

    for (size_t i = 0; i < index; ++index)
        m_frameBufferCache[index].setDecodedImage(QImage());
}

}

// vim: ts=4 sw=4 et
