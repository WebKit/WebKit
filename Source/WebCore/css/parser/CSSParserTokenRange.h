// Copyright 2014 The Chromium Authors. All rights reserved.
// Copyright (C) 2016 Apple Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#pragma once

#include <WebCore/CSSParserToken.h>
#include <WebCore/CSSTokenizer.h>
#include <wtf/Forward.h>
#include <wtf/text/ParsingUtilities.h>

namespace WebCore {

class StyleSheetContents;

// A CSSParserTokenRange is an iterator over a subrange of a vector of CSSParserTokens.
// Accessing outside of the range will return an endless stream of EOF tokens.
// This class refers to half-open intervals [first, last).
class CSSParserTokenRange {
public:
    CSSParserTokenRange() = default;

    CSSParserTokenRange(std::span<const CSSParserToken> span)
        : m_tokens(span)
    { }

    template<size_t inlineBuffer>
    CSSParserTokenRange(const Vector<CSSParserToken, inlineBuffer>& vector LIFETIME_BOUND)
        : CSSParserTokenRange(vector.span())
    { }

    CSSParserTokenRange rangeUntil(const CSSParserTokenRange& end) const { return span().first(end.begin() - begin()); }

    bool atEnd() const { return m_tokens.empty(); }

    const CSSParserToken* begin() const { return std::to_address(m_tokens.begin()); }
    const CSSParserToken* end() const { return std::to_address(m_tokens.end()); }

    size_t size() const { return m_tokens.size(); }

    const CSSParserToken& peek(size_t offset = 0) const
    {
        if (offset < m_tokens.size())
            return m_tokens[offset];
        return eofToken();
    }

    const CSSParserToken& consume()
    {
        if (m_tokens.empty())
            return eofToken();
        return WTF::consume(m_tokens);
    }

    const CSSParserToken& consumeIncludingWhitespace()
    {
        auto& result = consume();
        consumeWhitespace();
        return result;
    }

    // The returned range doesn't include the brackets
    CSSParserTokenRange consumeBlock();
    CSSParserTokenRange consumeBlockCheckingForEditability(StyleSheetContents*);

    void consumeComponentValue();

    void consumeWhitespace()
    {
        size_t i = 0;
        for (; i < m_tokens.size() && CSSTokenizer::isWhitespace(m_tokens[i].type()); ++i) { }
        skip(m_tokens, i);
    }

    void trimTrailingWhitespace();
    const CSSParserToken& consumeLast();

    CSSParserTokenRange consumeAll() { return { std::exchange(m_tokens, std::span<const CSSParserToken> { }) }; }

    String serialize(CSSParserToken::SerializationMode = CSSParserToken::SerializationMode::Normal) const;

    std::span<const CSSParserToken> span() const { return m_tokens; }

    static CSSParserToken& eofToken();

private:
    std::span<const CSSParserToken> m_tokens;
};

} // namespace WebCore
