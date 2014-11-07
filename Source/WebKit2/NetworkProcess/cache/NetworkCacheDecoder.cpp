/*
 * Copyright (C) 2010, 2011, 2014 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "NetworkCacheDecoder.h"

#if ENABLE(NETWORK_CACHE)

#include "NetworkCacheEncoder.h"

namespace WebKit {

NetworkCacheDecoder::NetworkCacheDecoder(const uint8_t* buffer, size_t bufferSize)
    : m_buffer(buffer)
    , m_bufferPos(buffer)
    , m_bufferEnd(buffer + bufferSize)
    , m_checksum(0)
{
}

NetworkCacheDecoder::~NetworkCacheDecoder()
{
}

static inline uint8_t* roundUpToAlignment(const uint8_t* ptr, unsigned alignment)
{
    // Assert that the alignment is a power of 2.
    ASSERT(alignment && !(alignment & (alignment - 1)));

    uintptr_t alignmentMask = alignment - 1;
    return reinterpret_cast<uint8_t*>((reinterpret_cast<uintptr_t>(ptr) + alignmentMask) & ~alignmentMask);
}

static inline bool alignedBufferIsLargeEnoughToContain(const uint8_t* alignedPosition, const uint8_t* bufferEnd, size_t size)
{
    return bufferEnd >= alignedPosition && static_cast<size_t>(bufferEnd - alignedPosition) >= size;
}

bool NetworkCacheDecoder::alignBufferPosition(unsigned alignment, size_t size)
{
    uint8_t* alignedPosition = roundUpToAlignment(m_bufferPos, alignment);
    if (!alignedBufferIsLargeEnoughToContain(alignedPosition, m_bufferEnd, size)) {
        // We've walked off the end of this buffer.
        markInvalid();
        return false;
    }
    
    m_bufferPos = alignedPosition;
    return true;
}

bool NetworkCacheDecoder::bufferIsLargeEnoughToContain(unsigned alignment, size_t size) const
{
    return alignedBufferIsLargeEnoughToContain(roundUpToAlignment(m_bufferPos, alignment), m_bufferEnd, size);
}

bool NetworkCacheDecoder::decodeFixedLengthData(uint8_t* data, size_t size, unsigned alignment)
{
    if (!alignBufferPosition(alignment, size))
        return false;

    memcpy(data, m_bufferPos, size);
    m_bufferPos += size;

    NetworkCacheEncoder::updateChecksumForData(m_checksum, data, size);
    return true;
}

template<typename Type>
bool NetworkCacheDecoder::decodeNumber(Type& value)
{
    if (!alignBufferPosition(sizeof(value), sizeof(value)))
        return false;

    memcpy(&value, m_bufferPos, sizeof(value));
    m_bufferPos += sizeof(Type);

    NetworkCacheEncoder::updateChecksumForNumber(m_checksum, value);
    return true;
}

bool NetworkCacheDecoder::decode(bool& result)
{
    return decodeNumber(result);
}

bool NetworkCacheDecoder::decode(uint8_t& result)
{
    return decodeNumber(result);
}

bool NetworkCacheDecoder::decode(uint16_t& result)
{
    return decodeNumber(result);
}

bool NetworkCacheDecoder::decode(uint32_t& result)
{
    return decodeNumber(result);
}

bool NetworkCacheDecoder::decode(uint64_t& result)
{
    return decodeNumber(result);
}

bool NetworkCacheDecoder::decode(int32_t& result)
{
    return decodeNumber(result);
}

bool NetworkCacheDecoder::decode(int64_t& result)
{
    return decodeNumber(result);
}

bool NetworkCacheDecoder::decode(float& result)
{
    return decodeNumber(result);
}

bool NetworkCacheDecoder::decode(double& result)
{
    return decodeNumber(result);
}

bool NetworkCacheDecoder::verifyChecksum()
{
    unsigned computedChecksum = m_checksum;
    unsigned decodedChecksum;
    if (!decodeNumber(decodedChecksum))
        return false;
    return computedChecksum == decodedChecksum;
}

}

#endif
