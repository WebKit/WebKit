/*
 * Copyright (C) 2010, 2011 Apple Inc. All rights reserved.
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
#include "ArgumentDecoder.h"

#include "DataReference.h"
#include <stdio.h>

namespace IPC {

ArgumentDecoder::ArgumentDecoder(const uint8_t* buffer, size_t bufferSize)
{
    initialize(buffer, bufferSize);
}

ArgumentDecoder::ArgumentDecoder(const uint8_t* buffer, size_t bufferSize, Vector<Attachment> attachments)
{
    initialize(buffer, bufferSize);

    m_attachments = WTF::move(attachments);
}

ArgumentDecoder::~ArgumentDecoder()
{
    ASSERT(m_buffer);
    free(m_buffer);
#if !USE(UNIX_DOMAIN_SOCKETS)
    // FIXME: We need to dispose of the mach ports in cases of failure.
#else
    Vector<Attachment>::iterator end = m_attachments.end();
    for (Vector<Attachment>::iterator it = m_attachments.begin(); it != end; ++it)
        it->dispose();
#endif
}

void ArgumentDecoder::initialize(const uint8_t* buffer, size_t bufferSize)
{
    m_buffer = static_cast<uint8_t*>(malloc(bufferSize));

    ASSERT(!(reinterpret_cast<uintptr_t>(m_buffer) % alignof(uint64_t)));

    m_bufferPosition = m_buffer;
    m_bufferEnd = m_buffer + bufferSize;
    memcpy(m_buffer, buffer, bufferSize);
}

bool ArgumentDecoder::bufferIsLargeEnoughToContain(size_t size) const
{
    return m_buffer + size <= m_bufferEnd;
}

bool ArgumentDecoder::decodeFixedLengthData(uint8_t* data, size_t size)
{
    if (!bufferIsLargeEnoughToContain(size))
        return false;

    memcpy(data, m_bufferPosition, size);
    m_bufferPosition += size;

    return true;
}

bool ArgumentDecoder::decodeVariableLengthByteArray(DataReference& dataReference)
{
    uint64_t size;
    if (!decode(size))
        return false;
    
    if (!bufferIsLargeEnoughToContain(size))
        return false;

    uint8_t* data = m_bufferPosition;
    m_bufferPosition += size;

    dataReference = DataReference(data, size);
    return true;
}

template<typename Type>
bool ArgumentDecoder::decodeNumber(Type& value)
{
    if (!bufferIsLargeEnoughToContain(sizeof(Type)))
        return false;

    memcpy(&value, m_bufferPosition, sizeof(Type));
    m_bufferPosition += sizeof(Type);

    return true;
}

bool ArgumentDecoder::decode(bool& result)
{
    return decodeNumber(result);
}

bool ArgumentDecoder::decode(uint8_t& result)
{
    return decodeNumber(result);
}

bool ArgumentDecoder::decode(uint16_t& result)
{
    return decodeNumber(result);
}

bool ArgumentDecoder::decode(uint32_t& result)
{
    return decodeNumber(result);
}

bool ArgumentDecoder::decode(uint64_t& result)
{
    return decodeNumber(result);
}

bool ArgumentDecoder::decode(int32_t& result)
{
    return decodeNumber(result);
}

bool ArgumentDecoder::decode(int64_t& result)
{
    return decodeNumber(result);
}

bool ArgumentDecoder::decode(float& result)
{
    return decodeNumber(result);
}

bool ArgumentDecoder::decode(double& result)
{
    return decodeNumber(result);
}

bool ArgumentDecoder::removeAttachment(Attachment& attachment)
{
    if (m_attachments.isEmpty())
        return false;

    attachment = m_attachments.last();
    m_attachments.removeLast();
    return true;
}

} // namespace IPC
