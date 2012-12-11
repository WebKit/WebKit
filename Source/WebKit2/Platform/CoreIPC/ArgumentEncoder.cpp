/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
#include "ArgumentEncoder.h"

#include "DataReference.h"
#include <algorithm>
#include <stdio.h>

namespace CoreIPC {

PassOwnPtr<ArgumentEncoder> ArgumentEncoder::create()
{
    return adoptPtr(new ArgumentEncoder);
}

ArgumentEncoder::ArgumentEncoder()
    : m_buffer(0)
    , m_bufferSize(0)
    , m_bufferCapacity(inlineBufferSize)
{
}

ArgumentEncoder::~ArgumentEncoder()
{
    if (m_buffer)
        free(m_buffer);
#if !USE(UNIX_DOMAIN_SOCKETS)
    // FIXME: We need to dispose of the attachments in cases of failure.
#else
    for (size_t i = 0; i < m_attachments.size(); ++i)
        m_attachments[i].dispose();
#endif
}

static inline size_t roundUpToAlignment(size_t value, unsigned alignment)
{
    return ((value + alignment - 1) / alignment) * alignment;
}

uint8_t* ArgumentEncoder::grow(unsigned alignment, size_t size)
{
    size_t alignedSize = roundUpToAlignment(m_bufferSize, alignment);
    
    if (alignedSize + size > m_bufferCapacity) {
        size_t newCapacity = std::max(alignedSize + size, std::max(static_cast<size_t>(32), m_bufferCapacity + m_bufferCapacity / 4 + 1));

        if (newCapacity > inlineBufferSize) {
            // Use system malloc / realloc instead of fastMalloc due to
            // fastMalloc using MADV_FREE_REUSABLE which doesn't work with
            // mach messages with OOL message and MACH_MSG_VIRTUAL_COPY.
            // System malloc also calls madvise(MADV_FREE_REUSABLE) but after first
            // checking via madvise(CAN_REUSE) that it will succeed. Should this
            // behavior change we'll need to revisit this.
            if (!m_buffer)
                m_buffer = static_cast<uint8_t*>(malloc(newCapacity));
            else
                m_buffer = static_cast<uint8_t*>(realloc(m_buffer, newCapacity));

            if (!m_buffer)
                CRASH();

            if (usesInlineBuffer())
                memcpy(m_buffer, m_inlineBuffer, m_bufferCapacity);
        }

        m_bufferCapacity = newCapacity;        
    }

    m_bufferSize = alignedSize + size;
    return buffer() + alignedSize;
}

void ArgumentEncoder::encodeFixedLengthData(const uint8_t* data, size_t size, unsigned alignment)
{
    ASSERT(!(reinterpret_cast<uintptr_t>(data) % alignment));

    uint8_t* buffer = grow(alignment, size);
    memcpy(buffer, data, size);
}

void ArgumentEncoder::encodeVariableLengthByteArray(const DataReference& dataReference)
{
    encode(static_cast<uint64_t>(dataReference.size()));
    encodeFixedLengthData(dataReference.data(), dataReference.size(), 1);
}

void ArgumentEncoder::encode(bool n)
{
    uint8_t* buffer = grow(sizeof(n), sizeof(n));
    
    *reinterpret_cast<bool*>(buffer) = n;
}

void ArgumentEncoder::encode(uint16_t n)
{
    uint8_t* buffer = grow(sizeof(n), sizeof(n));

    *reinterpret_cast<uint16_t*>(buffer) = n;
}

void ArgumentEncoder::encode(uint32_t n)
{
    uint8_t* buffer = grow(sizeof(n), sizeof(n));
    
    *reinterpret_cast<uint32_t*>(buffer) = n;
}

void ArgumentEncoder::encode(uint64_t n)
{
    uint8_t* buffer = grow(sizeof(n), sizeof(n));
    
    *reinterpret_cast<uint64_t*>(buffer) = n;
}

void ArgumentEncoder::encode(int32_t n)
{
    uint8_t* buffer = grow(sizeof(n), sizeof(n));
    
    *reinterpret_cast<int32_t*>(buffer) = n;
}

void ArgumentEncoder::encode(int64_t n)
{
    uint8_t* buffer = grow(sizeof(n), sizeof(n));
    
    *reinterpret_cast<int64_t*>(buffer) = n;
}

void ArgumentEncoder::encode(float n)
{
    uint8_t* buffer = grow(sizeof(n), sizeof(n));

    *reinterpret_cast<float*>(buffer) = n;
}

void ArgumentEncoder::encode(double n)
{
    uint8_t* buffer = grow(sizeof(n), sizeof(n));

    *reinterpret_cast<double*>(buffer) = n;
}

void ArgumentEncoder::addAttachment(const Attachment& attachment)
{
    m_attachments.append(attachment);
}

Vector<Attachment> ArgumentEncoder::releaseAttachments()
{
    Vector<Attachment> newList;
    newList.swap(m_attachments);
    return newList;
}

} // namespace CoreIPC
