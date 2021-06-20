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

class ScriptBufferSourceProvider final : public JSC::SourceProvider, public CanMakeWeakPtr<ScriptBufferSourceProvider> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<ScriptBufferSourceProvider> create(const ScriptBuffer& scriptBuffer, const JSC::SourceOrigin& sourceOrigin, String sourceURL, const TextPosition& startPosition = TextPosition(), JSC::SourceProviderSourceType sourceType = JSC::SourceProviderSourceType::Program)
    {
        return adoptRef(*new ScriptBufferSourceProvider(scriptBuffer, sourceOrigin, WTFMove(sourceURL), startPosition, sourceType));
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

        if (!m_containsOnlyASCII) {
            m_containsOnlyASCII = charactersAreAllASCII(m_scriptBuffer.buffer()->data(), m_scriptBuffer.buffer()->size());
            if (*m_containsOnlyASCII)
                m_scriptHash = StringHasher::computeHashAndMaskTop8Bits(m_scriptBuffer.buffer()->data(), m_scriptBuffer.buffer()->size());
        }
        if (*m_containsOnlyASCII)
            return { m_scriptBuffer.buffer()->data(), static_cast<unsigned>(m_scriptBuffer.buffer()->size()) };

        if (!m_cachedScriptString) {
            m_cachedScriptString = m_scriptBuffer.toString();
            if (!m_scriptHash)
                m_scriptHash = m_cachedScriptString.impl()->hash();
        }

        return m_cachedScriptString;
    }

    void clearDecodedData()
    {
        m_cachedScriptString = String();
    }

    void tryReplaceScriptBuffer(const ScriptBuffer& scriptBuffer)
    {
        // If this new file-mapped script buffer is identical to the one we have, then replace
        // ours to save dirty memory.
        if (m_scriptBuffer != scriptBuffer)
            return;

        m_scriptBuffer = scriptBuffer;
    }

private:
    ScriptBufferSourceProvider(const ScriptBuffer& scriptBuffer, const JSC::SourceOrigin& sourceOrigin, String&& sourceURL, const TextPosition& startPosition, JSC::SourceProviderSourceType sourceType)
        : JSC::SourceProvider(sourceOrigin, WTFMove(sourceURL), startPosition, sourceType)
        , m_scriptBuffer(scriptBuffer)
    {
    }

    ScriptBuffer m_scriptBuffer;
    mutable unsigned m_scriptHash { 0 };
    mutable String m_cachedScriptString;
    mutable std::optional<bool> m_containsOnlyASCII;
};

} // namespace WebCore
