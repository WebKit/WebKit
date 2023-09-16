/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#include "ScriptBuffer.h"
#include <JavaScriptCore/SourceProvider.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class AbstractScriptBufferHolder : public CanMakeWeakPtr<AbstractScriptBufferHolder> {
public:
    virtual void clearDecodedData() = 0;
    virtual void tryReplaceScriptBuffer(const ScriptBuffer&) = 0;

    virtual ~AbstractScriptBufferHolder() { }
};

class ScriptBufferSourceProvider final : public JSC::SourceProvider, public AbstractScriptBufferHolder {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<ScriptBufferSourceProvider> create(const ScriptBuffer& scriptBuffer, const JSC::SourceOrigin& sourceOrigin, String sourceURL, String preRedirectURL, const TextPosition& startPosition = TextPosition(), JSC::SourceProviderSourceType sourceType = JSC::SourceProviderSourceType::Program)
    {
        return adoptRef(*new ScriptBufferSourceProvider(scriptBuffer, sourceOrigin, WTFMove(sourceURL), WTFMove(preRedirectURL), startPosition, sourceType));
    }

    unsigned hash() const final
    {
        if (!m_scriptHash)
            source();
        return m_scriptHash;
    }

    StringView source() const final
    {
        if (m_scriptBuffer.isEmpty())
            return emptyString();

        if (!m_contiguousBuffer && (!m_containsOnlyASCII || *m_containsOnlyASCII))
            m_contiguousBuffer = m_scriptBuffer.buffer()->makeContiguous();
        if (!m_containsOnlyASCII) {
            m_containsOnlyASCII = charactersAreAllASCII(m_contiguousBuffer->data(), m_contiguousBuffer->size());
            if (*m_containsOnlyASCII)
                m_scriptHash = StringHasher::computeHashAndMaskTop8Bits(m_contiguousBuffer->data(), m_contiguousBuffer->size());
        }
        if (*m_containsOnlyASCII)
            return { m_contiguousBuffer->data(), static_cast<unsigned>(m_contiguousBuffer->size()) };

        if (!m_cachedScriptString) {
            m_cachedScriptString = m_scriptBuffer.toString();
            if (!m_scriptHash)
                m_scriptHash = m_cachedScriptString.impl()->hash();
        }

        return m_cachedScriptString;
    }

    void clearDecodedData() final
    {
        m_cachedScriptString = String();
    }

    void tryReplaceScriptBuffer(const ScriptBuffer& scriptBuffer) final
    {
        // If this new file-mapped script buffer is identical to the one we have, then replace
        // ours to save dirty memory.
        if (m_scriptBuffer != scriptBuffer)
            return;

        m_scriptBuffer = scriptBuffer;
        m_contiguousBuffer = nullptr;
    }

private:
    ScriptBufferSourceProvider(const ScriptBuffer& scriptBuffer, const JSC::SourceOrigin& sourceOrigin, String&& sourceURL, String&& preRedirectURL, const TextPosition& startPosition, JSC::SourceProviderSourceType sourceType)
        : JSC::SourceProvider(sourceOrigin, WTFMove(sourceURL), WTFMove(preRedirectURL), JSC::SourceTaintedOrigin::Untainted, startPosition, sourceType)
        , m_scriptBuffer(scriptBuffer)
    {
    }

    ScriptBuffer m_scriptBuffer;
    mutable RefPtr<SharedBuffer> m_contiguousBuffer;
    mutable unsigned m_scriptHash { 0 };
    mutable String m_cachedScriptString;
    mutable std::optional<bool> m_containsOnlyASCII;
};

} // namespace WebCore
