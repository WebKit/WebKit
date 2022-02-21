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

// Encodes a FragmentedSharedBuffer that is to be sent over IPC
// WARNING: a SharedBufferCopy should only be accessed on the IPC's receiver side.

#pragma once

#include "DataReference.h"
#include <WebCore/SharedBuffer.h>

namespace IPC {

class Decoder;
class Encoder;

class SharedBufferCopy {
public:
    SharedBufferCopy() = default;

    explicit SharedBufferCopy(RefPtr<WebCore::FragmentedSharedBuffer>&& buffer)
        : m_buffer(WTFMove(buffer)) { }
    explicit SharedBufferCopy(Ref<WebCore::FragmentedSharedBuffer>&& buffer)
        : m_buffer(WTFMove(buffer)) { }
    explicit SharedBufferCopy(RefPtr<WebCore::SharedBuffer>&& buffer)
        : m_buffer(WTFMove(buffer)) { }
    explicit SharedBufferCopy(Ref<WebCore::SharedBuffer>&& buffer)
        : m_buffer(WTFMove(buffer)) { }
    explicit SharedBufferCopy(const WebCore::FragmentedSharedBuffer& buffer)
        : m_buffer(const_cast<WebCore::FragmentedSharedBuffer*>(&buffer)) { }

    size_t size() const { return m_buffer ? m_buffer->size() : 0; }
    bool isEmpty() const { return m_buffer ? m_buffer->isEmpty() : true; }

    // The following methods must only be used on the receiver's IPC side.
    // It relies on an implementation detail that makes m_buffer becomes a contiguous SharedBuffer
    // once it's deserialised over IPC.
    RefPtr<WebCore::SharedBuffer> buffer() const { return downcast<WebCore::SharedBuffer>(m_buffer.get()); }
    Ref<WebCore::SharedBuffer> safeBuffer() const { return m_buffer ? buffer().releaseNonNull() : WebCore::SharedBuffer::create(); }
    const uint8_t* data() const;

    void encode(Encoder&) const;
    static WARN_UNUSED_RETURN std::optional<SharedBufferCopy> decode(Decoder&);

private:
    RefPtr<WebCore::FragmentedSharedBuffer> m_buffer;
};

} // namespace IPC
