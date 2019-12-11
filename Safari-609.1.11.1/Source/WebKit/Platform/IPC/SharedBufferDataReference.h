/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#pragma once

#include "Encoder.h"
#include <WebCore/SharedBuffer.h>
#include <wtf/Variant.h>

namespace IPC {

class SharedBufferDataReference {
public:
    SharedBufferDataReference() = default;
    SharedBufferDataReference(const WebCore::SharedBuffer& buffer)
        : m_data(buffer) { }
    SharedBufferDataReference(const uint8_t* data, size_t size)
        : m_data(std::make_pair(data, size)) { }

    void encode(Encoder& encoder) const
    {
        switchOn(m_data,
            [encoder = &encoder] (const Ref<const WebCore::SharedBuffer>& buffer) mutable {
                uint64_t bufferSize = buffer->size();
                encoder->reserve(bufferSize + sizeof(uint64_t));
                *encoder << bufferSize;
                for (const auto& element : buffer.get())
                    encoder->encodeFixedLengthData(reinterpret_cast<const uint8_t*>(element.segment->data()), element.segment->size(), 1);
            }, [encoder = &encoder] (const std::pair<const uint8_t*, size_t>& pair) mutable {
                uint64_t bufferSize = pair.second;
                encoder->reserve(bufferSize + sizeof(uint64_t));
                *encoder << bufferSize;
                encoder->encodeFixedLengthData(pair.first, pair.second, 1);
            }
        );
    }

private:
    Variant<std::pair<const uint8_t*, size_t>, Ref<const WebCore::SharedBuffer>> m_data;
};

}
