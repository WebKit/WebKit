/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved. 
 * Copyright (C) 2006 Trolltech ASA
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
#include "ImageSource.h"
#include "ImageDecoderQt.h"
#include "SharedBuffer.h"

#include <QImage>
#include <qdebug.h>


namespace WebCore {
    enum ImageFormat { ImageFormat_None, ImageFormat_GIF, ImageFormat_PNG, ImageFormat_JPEG,
                       ImageFormat_BMP,  ImageFormat_ICO,  ImageFormat_XBM };

ImageFormat detectImageFormat(const SharedBuffer& data)
{
    // We need at least 4 bytes to figure out what kind of image we're dealing with.
    int length = data.size();
    if (length < 4)
        return ImageFormat_None;

    const unsigned char* uContents = (const unsigned char*) data.data();
    const char* contents = data.data();

    // GIFs begin with GIF8(7 or 9).
    if (strncmp(contents, "GIF8", 4) == 0)
        return ImageFormat_GIF;

    // Test for PNG.
    if (uContents[0] == 0x89 &&
        uContents[1] == 0x50 &&
        uContents[2] == 0x4E &&
        uContents[3] == 0x47)
        return ImageFormat_PNG;

    // JPEG
    if (uContents[0] == 0xFF &&
        uContents[1] == 0xD8 &&
        uContents[2] == 0xFF)
        return ImageFormat_JPEG;

    // BMP
    if (strncmp(contents, "BM", 2) == 0)
        return ImageFormat_BMP;

    // ICOs always begin with a 2-byte 0 followed by a 2-byte 1.
    // CURs begin with 2-byte 0 followed by 2-byte 2.
    if (!memcmp(contents, "\000\000\001\000", 4) ||
        !memcmp(contents, "\000\000\002\000", 4))
        return ImageFormat_ICO;

    // XBMs require 8 bytes of info.
    if (length >= 8 && strncmp(contents, "#define ", 8) == 0)
        return ImageFormat_XBM;

    // Give up. We don't know what the heck this is.
    return ImageFormat_None;
}
    
ImageDecoderQt* createDecoder(const SharedBuffer& data) {
    if (detectImageFormat(data) != ImageFormat_None) 
        return new ImageDecoderQt();
    return 0;
}

ImageSource::ImageSource()
    : m_decoder(0)
{
}

ImageSource::~ImageSource()
{
    delete m_decoder;
}

bool ImageSource::initialized() const
{
    return m_decoder;
}

void ImageSource::setData(SharedBuffer* data, bool allDataReceived)
{
    // Make the decoder by sniffing the bytes.
    // This method will examine the data and instantiate an instance of the appropriate decoder plugin.
    // If insufficient bytes are available to determine the image type, no decoder plugin will be
    // made.
    if (!m_decoder)
        m_decoder = createDecoder(*data);

    if (!m_decoder)
        return;

    m_decoder->setData(data->buffer(), allDataReceived);
}

bool ImageSource::isSizeAvailable()
{
    if (!m_decoder)
        return false;

    return m_decoder->isSizeAvailable();
}

IntSize ImageSource::size() const
{
    if (!m_decoder)
        return IntSize();

    return m_decoder->size();
}

int ImageSource::repetitionCount()
{
    if (!m_decoder)
        return cAnimationNone;

    return m_decoder->repetitionCount();
}

size_t ImageSource::frameCount() const
{
    if (!m_decoder)
        return 0;

    return m_decoder->frameCount();
}

NativeImagePtr ImageSource::createFrameAtIndex(size_t index)
{
    if (!m_decoder)
        return 0;

    return m_decoder->imageAtIndex(index);
}

float ImageSource::frameDurationAtIndex(size_t index)
{
    if (!m_decoder)
        return 0;
    
    // Many annoying ads specify a 0 duration to make an image flash as quickly
    // as possible.  We follow WinIE's behavior and use a duration of 100 ms
    // for any frames that specify a duration of <= 50 ms.  See
    // <http://bugs.webkit.org/show_bug.cgi?id=14413> or Radar 4051389 for
    // more.
    const float duration = m_decoder->duration(index) / 1000.0f;
    return (duration < 0.051f) ? 0.100f : duration;
}

bool ImageSource::frameHasAlphaAtIndex(size_t index)
{
    if (!m_decoder || !m_decoder->supportsAlpha())
        return false;
    
    const QPixmap* source = m_decoder->imageAtIndex( index);
    if (!source)
        return false;
    
    return source->hasAlphaChannel();
}

bool ImageSource::frameIsCompleteAtIndex(size_t index)
{
    return (m_decoder && m_decoder->imageAtIndex(index) != 0);
}

void ImageSource::clear()
{
    delete  m_decoder;
    m_decoder = 0;
}


}

// vim: ts=4 sw=4 et
