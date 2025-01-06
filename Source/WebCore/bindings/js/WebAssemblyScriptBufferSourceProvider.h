/*
 * Copyright (C) 2022 Igalia S.L. All rights reserved.
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(WEBASSEMBLY)

#include "ScriptBufferSourceProvider.h"
#include <JavaScriptCore/SourceProvider.h>

namespace WebCore {

class WebAssemblyScriptBufferSourceProvider final : public JSC::BaseWebAssemblySourceProvider, public AbstractScriptBufferHolder {
    WTF_MAKE_TZONE_ALLOCATED(WebAssemblyScriptBufferSourceProvider);
public:
    static Ref<WebAssemblyScriptBufferSourceProvider> create(const ScriptBuffer& scriptBuffer, URL&& sourceURL, Ref<JSC::ScriptFetcher>&& scriptFetcher)
    {
        return adoptRef(*new WebAssemblyScriptBufferSourceProvider(scriptBuffer, JSC::SourceOrigin { WTFMove(sourceURL), WTFMove(scriptFetcher) }, sourceURL.string()));
    }

    unsigned hash() const final
    {
        return m_source.impl()->hash();
    }

    StringView source() const final
    {
        return m_source;
    }

    size_t size() const final { return m_buffer ? m_buffer->size() : 0; }

    const uint8_t* data() final
    {
        if (!m_buffer)
            return nullptr;

        ASSERT(m_buffer->isContiguous());
        return downcast<SharedBuffer>(*m_buffer).span().data();
    }

    void lockUnderlyingBuffer() final
    {
        ASSERT(!m_buffer);
        m_buffer = m_scriptBuffer.buffer();

        if (!m_buffer)
            return;

        if (!m_buffer->isContiguous())
            m_buffer = m_buffer->makeContiguous();
    }

    void unlockUnderlyingBuffer() final
    {
        ASSERT(m_buffer);
        m_buffer = nullptr;
    }

    void clearDecodedData() final { }

    void tryReplaceScriptBuffer(const ScriptBuffer& scriptBuffer) final
    {
        if (m_scriptBuffer != scriptBuffer)
            return;

        m_scriptBuffer = scriptBuffer;
    }

private:
    WebAssemblyScriptBufferSourceProvider(const ScriptBuffer& scriptBuffer, const JSC::SourceOrigin& sourceOrigin, String sourceURL)
        : BaseWebAssemblySourceProvider(sourceOrigin, WTFMove(sourceURL))
        , m_scriptBuffer(scriptBuffer)
        , m_buffer(nullptr)
        , m_source("[WebAssembly source]"_s)
    {
    }

    ScriptBuffer m_scriptBuffer;
    RefPtr<const FragmentedSharedBuffer> m_buffer;
    String m_source;
};

} // namespace WebCore

#endif // ENABLE(WEBASSEMBLY)
