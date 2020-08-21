/*
 * Copyright (C) 2018-2020 Apple Inc. All rights reserved.
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
#include "SharedBufferCopy.h"

#include "DataReference.h"
#include "Decoder.h"
#include "Encoder.h"

namespace IPC {

using namespace WebCore;

void SharedBufferCopy::encode(Encoder& encoder) const
{
    uint64_t bufferSize = m_buffer ? m_buffer->size() : 0;
    encoder.reserve(sizeof(bufferSize) + bufferSize);
    encoder << bufferSize;
    if (!bufferSize)
        return;
    for (auto& segment : *m_buffer)
        encoder.encodeFixedLengthData(reinterpret_cast<const uint8_t*>(segment.segment->data()), segment.segment->size(), 1);
}

Optional<SharedBufferCopy> SharedBufferCopy::decode(Decoder& decoder)
{
    IPC::DataReference data;
    if (!decoder.decodeVariableLengthByteArray(data))
        return WTF::nullopt;
    RefPtr<SharedBuffer> buffer;
    if (data.size())
        buffer = SharedBuffer::create(data.data(), data.size());
    return { WTFMove(buffer) };
}

} // namespace IPC
