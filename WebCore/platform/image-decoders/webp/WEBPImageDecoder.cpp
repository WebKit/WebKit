/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WEBPImageDecoder.h"

#if USE(WEBP)

#include "webp/decode.h"

namespace WebCore {

WEBPImageDecoder::WEBPImageDecoder(bool premultiplyAlpha, bool ignoreGammaAndColorProfile)
    : ImageDecoder(premultiplyAlpha, ignoreGammaAndColorProfile)
{
}

WEBPImageDecoder::~WEBPImageDecoder()
{
}

bool WEBPImageDecoder::isSizeAvailable()
{
    if (!ImageDecoder::isSizeAvailable())
         decode(true);

    return ImageDecoder::isSizeAvailable();
}

RGBA32Buffer* WEBPImageDecoder::frameBufferAtIndex(size_t index)
{
    if (index)
        return 0;

    if (m_frameBufferCache.isEmpty()) {
        m_frameBufferCache.resize(1);
        m_frameBufferCache[0].setPremultiplyAlpha(m_premultiplyAlpha);
    }

    RGBA32Buffer& frame = m_frameBufferCache[0];
    if (frame.status() != RGBA32Buffer::FrameComplete)
        decode(false);
    return &frame;
}


bool WEBPImageDecoder::decode(bool onlySize)
{
    // Minimum number of bytes needed to ensure one can parse size information.
    static const size_t sizeOfHeader = 30;
    // Number of bytes per pixel.
    static const int bytesPerPixel = 3;

    if (failed())
        return false;
    const size_t dataSize = m_data->buffer().size();
    const uint8_t* dataBytes =
        reinterpret_cast<const uint8_t*>(m_data->buffer().data());
    int width, height;
    if (dataSize < sizeOfHeader)
        return true;
    if (!WebPGetInfo(dataBytes, dataSize, &width, &height))
        return setFailed();
    if (!ImageDecoder::isSizeAvailable() && !setSize(width, height))
        return setFailed();
    if (onlySize)
        return true;

    // FIXME: Add support for progressive decoding.
    if (!isAllDataReceived())
        return true;
    ASSERT(!m_frameBufferCache.isEmpty());
    RGBA32Buffer& buffer = m_frameBufferCache[0];
    if (buffer.status() == RGBA32Buffer::FrameEmpty) {
        ASSERT(width == size().width());
        ASSERT(height == size().height());
        if (!buffer.setSize(width, height))
            return setFailed();
    }
    const int stride = width * bytesPerPixel;
    Vector<uint8_t> rgb;
    rgb.resize(height * stride);
    if (!WebPDecodeBGRInto(dataBytes, dataSize, rgb.data(), rgb.size(), stride))
        return setFailed();
    // FIXME: remove this data copy.
    for (int y = 0; y < height; ++y) {
        const uint8_t* const src = &rgb[y * stride];
        for (int x = 0; x < width; ++x)
            buffer.setRGBA(x, y, src[bytesPerPixel * x + 2], src[bytesPerPixel * x + 1], src[bytesPerPixel * x + 0], 0xff);
    }
    buffer.setStatus(RGBA32Buffer::FrameComplete);
    buffer.setHasAlpha(false);
    buffer.setRect(IntRect(IntPoint(), size()));
    return true;
}

}

#endif
