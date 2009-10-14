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

    return new ImageDecoderQt;
}

ImageDecoderQt::ImageDecoderQt()
    :  m_buffer(0)
    , m_reader(0)
    , m_repetitionCount(cAnimationNone)
{
}

ImageDecoderQt::~ImageDecoderQt()
{
    delete m_reader;
    delete m_buffer;
}

void ImageDecoderQt::setData(SharedBuffer* data, bool allDataReceived)
{
    if (m_failed)
        return;

    // No progressive loading possible
    if (!allDataReceived)
        return;

    // Cache our own new data.
    ImageDecoder::setData(data, allDataReceived);

    // We expect to be only called once with allDataReceived
    ASSERT(!m_buffer);
    ASSERT(!m_reader);

    // Attempt to load the data
    QByteArray imageData = QByteArray::fromRawData(m_data->data(), m_data->size());
    m_buffer = new QBuffer;
    m_buffer->setData(imageData);
    m_buffer->open(QBuffer::ReadOnly);
    m_reader = new QImageReader(m_buffer, m_format);
}

bool ImageDecoderQt::isSizeAvailable()
{
    if (!ImageDecoder::isSizeAvailable() && m_reader)
        internalDecodeSize();

    return ImageDecoder::isSizeAvailable();
}

size_t ImageDecoderQt::frameCount()
{
    if (m_frameBufferCache.isEmpty() && m_reader) {
        if (m_reader->supportsAnimation()) {
            int imageCount = m_reader->imageCount();

            // Fixup for Qt decoders... imageCount() is wrong
            // and jumpToNextImage does not work either... so
            // we will have to parse everything...
            if (imageCount == 0)
                forceLoadEverything();
            else
                m_frameBufferCache.resize(imageCount);
        } else {
            m_frameBufferCache.resize(1);
        }
    }

    return m_frameBufferCache.size();
}

int ImageDecoderQt::repetitionCount() const
{
    if (m_reader && m_reader->supportsAnimation())
        m_repetitionCount = qMax(0, m_reader->loopCount());

    return m_repetitionCount;
}

String ImageDecoderQt::filenameExtension() const
{
    return String(m_format.constData(), m_format.length());
};

RGBA32Buffer* ImageDecoderQt::frameBufferAtIndex(size_t index)
{
    // In case the ImageDecoderQt got recreated we don't know
    // yet how many images we are going to have and need to
    // find that out now.
    int count = m_frameBufferCache.size();
    if (!m_failed && count == 0) {
        internalDecodeSize();
        count = frameCount();
    }

    if (index >= static_cast<size_t>(count))
        return 0;

    RGBA32Buffer& frame = m_frameBufferCache[index];
    if (frame.status() != RGBA32Buffer::FrameComplete && m_reader)
        internalReadImage(index);
    return &frame;
}

void ImageDecoderQt::clearFrameBufferCache(size_t index)
{
    // Currently QImageReader will be asked to read everything. This
    // might change when we read gif images on demand. For now we
    // can have a rather simple implementation.
    if (index > m_frameBufferCache.size())
        return;

    for (size_t i = 0; i < index; ++index)
        m_frameBufferCache[index].clear();
}

void ImageDecoderQt::internalDecodeSize()
{
    ASSERT(m_reader);

    // If we have a QSize() something failed
    QSize size = m_reader->size();
    if (size.isEmpty())
        return failRead();

    m_format = m_reader->format();
    setSize(size.width(), size.height());
}

void ImageDecoderQt::internalReadImage(size_t frameIndex)
{
    ASSERT(m_reader);

    if (m_reader->supportsAnimation())
        m_reader->jumpToImage(frameIndex);
    else if (frameIndex != 0)
        return failRead();

    internalHandleCurrentImage(frameIndex);

    // Attempt to return some memory
    for (int i = 0; i < m_frameBufferCache.size(); ++i)
        if (m_frameBufferCache[i].status() != RGBA32Buffer::FrameComplete)
            return;

    delete m_reader;
    delete m_buffer;
    m_buffer = 0;
    m_reader = 0;
}

void ImageDecoderQt::internalHandleCurrentImage(size_t frameIndex)
{
    // Now get the QImage from Qt and place it in the RGBA32Buffer
    QImage img;
    if (!m_reader->read(&img))
        return failRead();

    // now into the RGBA32Buffer - even if the image is not
    QSize imageSize = img.size();
    RGBA32Buffer* const buffer = &m_frameBufferCache[frameIndex];
    buffer->setRect(m_reader->currentImageRect());
    buffer->setStatus(RGBA32Buffer::FrameComplete);
    buffer->setDuration(m_reader->nextImageDelay());
    buffer->setDecodedImage(img);
}

// The QImageIOHandler is not able to tell us how many frames
// we have and we need to parse every image. We do this by
// increasing the m_frameBufferCache by one and try to parse
// the image. We stop when QImage::read fails and then need
// to resize the m_frameBufferCache to the final size and update
// the m_failed. In case we failed to decode the first image
// we want to keep m_failed set to true.

// TODO: Do not increment the m_frameBufferCache.size() by one but more than one
void ImageDecoderQt::forceLoadEverything()
{
    int imageCount = 0;

    do {
        m_frameBufferCache.resize(++imageCount);
        internalHandleCurrentImage(imageCount - 1);
    } while(!m_failed);

    // If we failed decoding the first image we actually
    // have no images and need to keep m_failed set to
    // true otherwise we want to reset it and forget about
    // the last attempt to decode a image.
    m_frameBufferCache.resize(imageCount - 1);
    m_failed = imageCount == 1;
}

void ImageDecoderQt::failRead()
{
    setFailed();
    delete m_reader;
    delete m_buffer;
    m_reader = 0;
    m_buffer = 0;
}
}

// vim: ts=4 sw=4 et
