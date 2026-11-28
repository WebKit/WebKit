/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include "StructuredCloneTags.h"
#include <span>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/Vector.h>

#if ENABLE(WEBASSEMBLY)
#include "WasmModule.h"
#endif

namespace JSCShell {

class Message : public ThreadSafeRefCounted<Message> {
public:
    static Ref<Message> create(Vector<uint8_t>&& data, JSC::CloneSerializationSideChannels&& sideChannels, int32_t index = 0, JSC::ArrayBufferContentsArray&& transferredBuffers = { })
    {
        return adoptRef(*new Message(WTF::move(data), WTF::move(sideChannels), index, WTF::move(transferredBuffers)));
    }

    std::span<const uint8_t> data() const { return m_data.span(); }
    NODELETE int32_t index() const { return m_index; }

    // Concurrent deserialization is only safe when m_transferredBuffers is empty.
    JSC::CloneDeserializationSideChannels sideChannels()
    {
        return {
            .arrayBufferContents = &m_transferredBuffers,
            .sharedBuffers = &m_sideChannels.sharedBuffers,
#if ENABLE(WEBASSEMBLY)
            .wasmModules = &m_sideChannels.wasmModules,
            .wasmMemoryHandles = &m_sideChannels.wasmMemoryHandles,
#endif
        };
    }

private:
    Message(Vector<uint8_t>&& data, JSC::CloneSerializationSideChannels&& sideChannels, int32_t index, JSC::ArrayBufferContentsArray&& transferredBuffers)
        : m_data(WTF::move(data))
        , m_sideChannels(WTF::move(sideChannels))
        , m_transferredBuffers(WTF::move(transferredBuffers))
        , m_index(index)
    {
    }

    const Vector<uint8_t> m_data;
    JSC::CloneSerializationSideChannels m_sideChannels;
    JSC::ArrayBufferContentsArray m_transferredBuffers;
    int32_t m_index { 0 };
};

} // namespace JSCShell
