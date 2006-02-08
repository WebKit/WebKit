/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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

#include "PNGImageDecoder.h"

namespace WebCore {

class PNGImageDecoderPrivate
{
public:
    PNGImageDecoderPrivate()
        : m_readOffset(0)
    {
    }

    ~PNGImageDecoderPrivate()
    {
    }

    bool decode(const ByteArray& data, bool sizeOnly)
    {
        // FIXME: Implement
        return false;
    }

private:
    unsigned m_readOffset;
};

PNGImageDecoder::PNGImageDecoder()
: m_sizeAvailable(false)
, m_failed(false)
, m_impl(0)
{}

PNGImageDecoder::~PNGImageDecoder()
{
    delete m_impl;
}

// Take the data and store it.
void PNGImageDecoder::setData(const ByteArray& data, bool allDataReceived)
{
    if (m_failed)
        return;

    // Cache our new data.
    ImageDecoder::setData(data, allDataReceived);

    // Create the PNG reader.
    if (!m_impl && !m_failed)
        m_impl = new PNGImageDecoderPrivate();
}

// Whether or not the size information has been decoded yet.
bool PNGImageDecoder::isSizeAvailable() const
{
    // If we have pending data to decode, send it to the PNG reader now.
    if (!m_sizeAvailable && m_impl) {
        if (m_failed)
            return false;

        // The decoder will go ahead and aggressively consume everything up until the
        // size is encountered.
        decode(true);
    }

    return m_sizeAvailable;
}

// Requests the size.
IntSize PNGImageDecoder::size() const
{
    return m_size;
}


RGBA32Buffer PNGImageDecoder::frameBufferAtIndex(size_t index)
{
    if (index != 0)
        return RGBA32Buffer();

    if (m_frameBufferCache.isEmpty())
        m_frameBufferCache.resize(1);

    const RGBA32Buffer& frame = m_frameBufferCache[0];
    if (frame.status() != RGBA32Buffer::FrameComplete && m_impl)
        // Decode this frame.
        decode();

    return frame;
}

// Feed data to the PNG reader.
void PNGImageDecoder::decode(bool sizeOnly) const
{
    if (m_failed)
        return;

    m_failed = !m_impl->decode(m_data, sizeOnly);
    
    if (m_failed) {
        delete m_impl;
        m_impl = 0;
    }
}

}
