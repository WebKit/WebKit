/*
 * Copyright (C) 2008, 2013 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <JavaScriptCore/UnlinkedSourceCode.h>

namespace JSC {

class SourceCode : public UnlinkedSourceCode {
    friend class CachedSourceCode;
    friend class CachedSourceCodeWithoutProvider;

public:
    SourceCode()
        : UnlinkedSourceCode()
    {
    }

    SourceCode(Ref<SourceProvider>&& provider)
        : UnlinkedSourceCode(WTF::move(provider))
    {
    }

    SourceCode(RefPtr<SourceProvider>&& provider, int startOffset, int endOffset)
        : UnlinkedSourceCode(WTF::move(provider), startOffset, endOffset)
    {
    }

    // Prefer derivedStartPosition() when both are wanted: each of these derives the whole pair.
    OrdinalNumber firstLine() const { return derivedStartPosition().first; }
    OrdinalNumber startColumn() const { return derivedStartPosition().second; }

    // Builds the provider's line-start table, so this belongs on a reporting path, not a parse path.
    std::pair<OrdinalNumber, OrdinalNumber> derivedStartPosition() const
    {
        SUPPRESS_UNCOUNTED_ARG // m_provider is the owning ref
        auto lineColumn = m_provider->documentLineColumnForOffset(startOffset());
        return { OrdinalNumber::fromOneBasedInt(lineColumn.line), OrdinalNumber::fromOneBasedInt(lineColumn.column) };
    }

    SourceID providerID() const
    {
        if (!m_provider)
            return SourceProvider::nullID;
        return m_provider->asID();
    }

    SourceProvider* provider() const { return m_provider.get(); }

    SourceCode subExpression(unsigned openBrace, unsigned closeBrace) const;

    friend bool operator==(const SourceCode&, const SourceCode&) = default;
};

inline SourceCode makeSource(const String& source, const SourceOrigin& sourceOrigin, SourceTaintedOrigin sourceTaintedOrigin, String filename = String(), const TextPosition& startPosition = TextPosition(), SourceProviderSourceType sourceType = SourceProviderSourceType::Program)
{
    // The provider is the only place an inline <script>'s start is recorded; SourceCode reads it
    // back from there so the two cannot disagree.
    return SourceCode(StringSourceProvider::create(source, sourceOrigin, WTF::move(filename), sourceTaintedOrigin, startPosition, sourceType));
}

inline SourceCode SourceCode::subExpression(unsigned openBrace, unsigned closeBrace) const
{
    return SourceCode(RefPtr<SourceProvider> { provider() }, openBrace, closeBrace + 1);
}

} // namespace JSC
