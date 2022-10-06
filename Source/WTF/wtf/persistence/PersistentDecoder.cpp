/*
 * Copyright (C) 2010-2021 Apple Inc. All rights reserved.
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

namespace WTF::Persistence {

Decoder::Decoder(Span<const uint8_t> span)
    : m_buffer(span)
    , m_bufferPosition(span.begin())
{
}

Decoder::~Decoder()
{
}

bool Decoder::bufferIsLargeEnoughToContain(size_t size) const
{
    return size <= static_cast<size_t>(m_buffer.end() - m_bufferPosition);
}

const uint8_t* Decoder::bufferPointerForDirectRead(size_t size)
{
    if (!bufferIsLargeEnoughToContain(size))
        return nullptr;

    auto data = m_bufferPosition;
    m_bufferPosition += size;

    Encoder::updateChecksumForData(m_sha1, { data, size });
    return data;
}

bool Decoder::decodeFixedLengthData(Span<uint8_t> span)
{
    auto buffer = bufferPointerForDirectRead(span.size());
    if (!buffer)
        return false;
    memcpy(span.data(), buffer, span.size());
    return true;
}

bool Decoder::rewind(size_t size)
{
    if (size <= static_cast<size_t>(m_bufferPosition - m_buffer.begin())) {
        m_bufferPosition -= size;
        return true;
    }
    return false;
}

template<typename T>
Decoder& Decoder::decodeNumber(std::optional<T>& optional)
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

Decoder& Decoder::operator>>(std::optional<bool>& result)
{
    return decodeNumber(result);
}

Decoder& Decoder::operator>>(std::optional<uint8_t>& result)
{
    return decodeNumber(result);
}

Decoder& Decoder::operator>>(std::optional<uint16_t>& result)
{
    return decodeNumber(result);
}

Decoder& Decoder::operator>>(std::optional<int16_t>& result)
{
    return decodeNumber(result);
}

Decoder& Decoder::operator>>(std::optional<uint32_t>& result)
{
    return decodeNumber(result);
}

Decoder& Decoder::operator>>(std::optional<uint64_t>& result)
{
    return decodeNumber(result);
}

Decoder& Decoder::operator>>(std::optional<int32_t>& result)
{
    return decodeNumber(result);
}

Decoder& Decoder::operator>>(std::optional<int64_t>& result)
{
    return decodeNumber(result);
}

Decoder& Decoder::operator>>(std::optional<float>& result)
{
    return decodeNumber(result);
}

Decoder& Decoder::operator>>(std::optional<double>& result)
{
    return decodeNumber(result);
}

bool Decoder::verifyChecksum()
{
    SHA1::Digest computedHash;
    m_sha1.computeHash(computedHash);

    SHA1::Digest savedHash;
    if (!decodeFixedLengthData({ savedHash.data(), sizeof(savedHash) }))
        return false;

    return computedHash == savedHash;
}

} // namespace WTF::Persistence
