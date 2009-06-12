/*
 * Copyright (c) 2008, Google Inc. All rights reserved.
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
#include "ImageSourceSkia.h"
#include "SharedBuffer.h"

#include "GIFImageDecoder.h"
#include "ICOImageDecoder.h"
#include "JPEGImageDecoder.h"
#include "PNGImageDecoder.h"
#include "BMPImageDecoder.h"
#include "XBMImageDecoder.h"

#include "SkBitmap.h"

namespace WebCore {

ImageDecoder* createDecoder(const Vector<char>& data, const IntSize& preferredIconSize)
{
    // We need at least 4 bytes to figure out what kind of image we're dealing with.
    int length = data.size();
    if (length < 4)
        return 0;

    const unsigned char* uContents = (const unsigned char*)data.data();
    const char* contents = data.data();

    // GIFs begin with GIF8(7 or 9).
    if (strncmp(contents, "GIF8", 4) == 0)
        return new GIFImageDecoder();

    // Test for PNG.
    if (uContents[0]==0x89 &&
        uContents[1]==0x50 &&
        uContents[2]==0x4E &&
        uContents[3]==0x47)
        return new PNGImageDecoder();

    // JPEG
    if (uContents[0]==0xFF &&
        uContents[1]==0xD8 &&
        uContents[2]==0xFF)
        return new JPEGImageDecoder();

    // BMP
    if (strncmp(contents, "BM", 2) == 0)
        return new BMPImageDecoder();

    // ICOs always begin with a 2-byte 0 followed by a 2-byte 1.
    // CURs begin with 2-byte 0 followed by 2-byte 2.
    if (!memcmp(contents, "\000\000\001\000", 4) ||
        !memcmp(contents, "\000\000\002\000", 4))
        return new ICOImageDecoder(preferredIconSize);
   
    // XBMs require 8 bytes of info.
    if (length >= 8 && strncmp(contents, "#define ", 8) == 0)
        return new XBMImageDecoder();

    // Give up. We don't know what the heck this is.
    return 0;
}

ImageSource::ImageSource()
    : m_decoder(0)
{}

ImageSource::~ImageSource()
{
    clear(true);
}

void ImageSource::clear(bool destroyAll, size_t clearBeforeFrame, SharedBuffer* data, bool allDataReceived)
{
    if (!destroyAll) {
        if (m_decoder)
            m_decoder->clearFrameBufferCache(clearBeforeFrame);
        return;
    }

    delete m_decoder;
    m_decoder = 0;
    if (data)
        setData(data, allDataReceived);
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
        m_decoder = createDecoder(data->buffer(), IntSize());

    // CreateDecoder will return NULL if the decoder could not be created. Plus,
    // we should not send more data to a decoder which has already decided it
    // has failed.
    if (!m_decoder || m_decoder->failed())
        return;
    m_decoder->setData(data, allDataReceived);
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

IntSize ImageSource::frameSizeAtIndex(size_t) const
{
    // TODO(brettw) do we need anything here?
    return size();
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
    return m_decoder->failed() ? 0 : m_decoder->frameCount();
}

NativeImagePtr ImageSource::createFrameAtIndex(size_t index)
{
    if (!m_decoder)
        return 0;

    // Note that the buffer can have NULL bytes even when it is marked as
    // non-empty. It seems "FrameEmpty" is only set before the frame has been
    // initialized. If it is decoded and it happens to be empty, it will be
    // marked as "FrameComplete" but will still have NULL bytes.
    RGBA32Buffer* buffer = m_decoder->frameBufferAtIndex(index);
    if (!buffer || buffer->status() == RGBA32Buffer::FrameEmpty)
        return 0;

    // Copy the bitmap.  The pixel data is refcounted internally by SkBitmap, so
    // this doesn't cost much.  
    return buffer->asNewNativeImage();
}

bool ImageSource::frameIsCompleteAtIndex(size_t index)
{
    if (!m_decoder)
        return false;

    RGBA32Buffer* buffer = m_decoder->frameBufferAtIndex(index);
    return buffer && buffer->status() == RGBA32Buffer::FrameComplete;
}

float ImageSource::frameDurationAtIndex(size_t index)
{
    if (!m_decoder)
        return 0;

    RGBA32Buffer* buffer = m_decoder->frameBufferAtIndex(index);
    if (!buffer || buffer->status() == RGBA32Buffer::FrameEmpty)
        return 0;

    // Many annoying ads specify a 0 duration to make an image flash as quickly
    // as possible.  We follow WinIE's behavior and use a duration of 100 ms
    // for any frames that specify a duration of <= 50 ms.  See
    // <http://bugs.webkit.org/show_bug.cgi?id=14413> or Radar 4051389 for
    // more.
    const float duration = buffer->duration() / 1000.0f;
    return (duration < 0.051f) ? 0.100f : duration;
}

bool ImageSource::frameHasAlphaAtIndex(size_t index)
{
    if (!m_decoder || !m_decoder->supportsAlpha())
        return false;

    RGBA32Buffer* buffer = m_decoder->frameBufferAtIndex(index);
    if (!buffer || buffer->status() == RGBA32Buffer::FrameEmpty)
        return false;

    return buffer->hasAlpha();
}

void ImageSourceSkia::setData(SharedBuffer* data,
                              bool allDataReceived,
                              const IntSize& preferredIconSize)
{
    if (!m_decoder)
        m_decoder = createDecoder(data->buffer(), preferredIconSize);

    ImageSource::setData(data, allDataReceived);
}

String ImageSource::filenameExtension() const
{
    return m_decoder ? m_decoder->filenameExtension() : String();
}

}
