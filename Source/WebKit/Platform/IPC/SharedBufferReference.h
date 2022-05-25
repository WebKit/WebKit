/*
 * Copyright (C) 2018-2022 Apple Inc. All rights reserved.
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

// Encodes a FragmentedSharedBuffer that is to be sent over IPC
// WARNING: a SharedBufferReference should only be accessed on the IPC's receiver side.

#pragma once

#include "DataReference.h"
#include <WebCore/SharedBuffer.h>
#include <optional>

namespace WebKit {
class SharedMemory;
}

namespace IPC {

class Decoder;
class Encoder;

class SharedBufferReference {
public:
    SharedBufferReference() = default;

    explicit SharedBufferReference(RefPtr<WebCore::FragmentedSharedBuffer>&& buffer)
        : m_size(buffer ? buffer->size() : 0)
        , m_buffer(WTFMove(buffer)) { }
    explicit SharedBufferReference(Ref<WebCore::FragmentedSharedBuffer>&& buffer)
        : m_size(buffer->size())
        , m_buffer(WTFMove(buffer)) { }
    explicit SharedBufferReference(RefPtr<WebCore::SharedBuffer>&& buffer)
        : m_size(buffer ? buffer->size() : 0)
        , m_buffer(WTFMove(buffer)) { }
    explicit SharedBufferReference(Ref<WebCore::SharedBuffer>&& buffer)
        : m_size(buffer->size())
        , m_buffer(WTFMove(buffer)) { }
    explicit SharedBufferReference(const WebCore::FragmentedSharedBuffer& buffer)
        : m_size(buffer.size())
        , m_buffer(const_cast<WebCore::FragmentedSharedBuffer*>(&buffer)) { }

    SharedBufferReference(const SharedBufferReference&) = default;
    SharedBufferReference(SharedBufferReference&&) = default;
    SharedBufferReference& operator=(const SharedBufferReference&) = default;
    SharedBufferReference& operator=(SharedBufferReference&&) = default;

    size_t size() const { return m_size; }
    bool isEmpty() const { return !size(); }
    bool isNull() const { return isEmpty() && !m_buffer; }

    // The following method must only be used on the receiver's IPC side.
    // It relies on an implementation detail that makes m_buffer become a contiguous SharedBuffer
    // once it's deserialised over IPC.
    RefPtr<WebCore::SharedBuffer> unsafeBuffer() const;
    const uint8_t* data() const;

    void encode(Encoder&) const;
    static WARN_UNUSED_RETURN std::optional<SharedBufferReference> decode(Decoder&);

private:
    SharedBufferReference(Ref<WebKit::SharedMemory>&& memory, size_t size)
        : m_size(size)
        , m_memory(WTFMove(memory)) { }

    size_t m_size { 0 };
    RefPtr<WebCore::FragmentedSharedBuffer> m_buffer;
    RefPtr<WebKit::SharedMemory> m_memory; // Only set on the receiver side and if m_size isn't 0.
};

} // namespace IPC
