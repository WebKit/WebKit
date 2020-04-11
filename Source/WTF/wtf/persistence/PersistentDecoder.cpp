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
#include <wtf/persistence/PersistentDecoder.h>

#include <wtf/persistence/PersistentEncoder.h>

namespace WTF {
namespace Persistence {

Decoder::Decoder(const uint8_t* buffer, size_t bufferSize)
    : m_buffer(buffer)
    , m_bufferPosition(buffer)
    , m_bufferEnd(buffer + bufferSize)
{
}

Decoder::~Decoder()
{
}

bool Decoder::bufferIsLargeEnoughToContain(size_t size) const
{
    return size <= static_cast<size_t>(m_bufferEnd - m_bufferPosition);
}

bool Decoder::decodeFixedLengthData(uint8_t* data, size_t size)
{
    if (!bufferIsLargeEnoughToContain(size))
        return false;

    memcpy(data, m_bufferPosition, size);
    m_bufferPosition += size;

    Encoder::updateChecksumForData(m_sha1, data, size);
    return true;
}

template<typename T>
Decoder& Decoder::decodeNumber(Optional<T>& optional)
{
    if (!bufferIsLargeEnoughToContain(sizeof(T)))
        return *this;

    T value;
    memcpy(&value, m_bufferPosition, sizeof(T));
    m_bufferPosition += sizeof(T);

    Encoder::updateChecksumForNumber(m_sha1, value);
    optional = value;
    return *this;
}

Decoder& Decoder::operator>>(Optional<bool>& result)
{
    return decodeNumber(result);
}

Decoder& Decoder::operator>>(Optional<uint8_t>& result)
{
    return decodeNumber(result);
}

Decoder& Decoder::operator>>(Optional<uint16_t>& result)
{
    return decodeNumber(result);
}

Decoder& Decoder::operator>>(Optional<int16_t>& result)
{
    return decodeNumber(result);
}

Decoder& Decoder::operator>>(Optional<uint32_t>& result)
{
    return decodeNumber(result);
}

Decoder& Decoder::operator>>(Optional<uint64_t>& result)
{
    return decodeNumber(result);
}

Decoder& Decoder::operator>>(Optional<int32_t>& result)
{
    return decodeNumber(result);
}

Decoder& Decoder::operator>>(Optional<int64_t>& result)
{
    return decodeNumber(result);
}

Decoder& Decoder::operator>>(Optional<float>& result)
{
    return decodeNumber(result);
}

Decoder& Decoder::operator>>(Optional<double>& result)
{
    return decodeNumber(result);
}

bool Decoder::verifyChecksum()
{
    SHA1::Digest computedHash;
    m_sha1.computeHash(computedHash);

    SHA1::Digest savedHash;
    if (!decodeFixedLengthData(savedHash.data(), sizeof(savedHash)))
        return false;

    return computedHash == savedHash;
}

}
}
