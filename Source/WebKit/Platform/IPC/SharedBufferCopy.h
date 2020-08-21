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

// Encodes a SharedBuffer that is received as a copy of the decoded data in a new SharedBuffer.
// To avoid copying from the Decoder, use SharedBufferDataReference to receive a DataReference instead.

#pragma once

#include <WebCore/SharedBuffer.h>

namespace IPC {

class Decoder;
class Encoder;

class SharedBufferCopy {
public:
    SharedBufferCopy() = default;

    SharedBufferCopy(RefPtr<WebCore::SharedBuffer>&& buffer)
        : m_buffer(WTFMove(buffer)) { }
    SharedBufferCopy(Ref<WebCore::SharedBuffer>&& buffer)
        : m_buffer(WTFMove(buffer)) { }
    SharedBufferCopy(const WebCore::SharedBuffer& buffer)
        : m_buffer(WebCore::SharedBuffer::create())
    {
        m_buffer->append(buffer);
    }

    RefPtr<WebCore::SharedBuffer>& buffer() { return m_buffer; }
    const RefPtr<WebCore::SharedBuffer>& buffer() const { return m_buffer; }

    const char* data() const { return m_buffer ? m_buffer->data() : nullptr; }
    size_t size() const { return m_buffer ? m_buffer->size() : 0; }
    bool isEmpty() const { return m_buffer ? m_buffer->isEmpty() : true; }

    void encode(Encoder&) const;
    static WARN_UNUSED_RETURN Optional<SharedBufferCopy> decode(Decoder&);

private:
    RefPtr<WebCore::SharedBuffer> m_buffer;
};

} // namespace IPC
