// Copyright 2014 The Chromium Authors. All rights reserved.
// Copyright (C) 2016-2024 Apple Inc. All rights reserved.
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

#include "CSSParserToken.h"
#include "CSSTokenizerInputStream.h"
#include <climits>
#include <wtf/text/StringView.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class CSSTokenizerInputStream;
class CSSParserObserverWrapper;
class CSSParserTokenRange;

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(CSSTokenizer);
class CSSTokenizer {
    WTF_MAKE_NONCOPYABLE(CSSTokenizer);
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(CSSTokenizer, CSSTokenizer);
public:
    static std::unique_ptr<CSSTokenizer> tryCreate(const String&);
    static std::unique_ptr<CSSTokenizer> tryCreate(const String&, CSSParserObserverWrapper&); // For the inspector

    WEBCORE_EXPORT explicit CSSTokenizer(const String&);
    CSSTokenizer(const String&, CSSParserObserverWrapper&); // For the inspector

    WEBCORE_EXPORT CSSParserTokenRange tokenRange() const LIFETIME_BOUND;
    unsigned tokenCount();

    static bool isWhitespace(CSSParserTokenType);
    static bool isNewline(char16_t);

    Vector<String>&& escapedStringsForAdoption() { return WTFMove(m_stringPool); }

private:
    CSSTokenizer(const String&, CSSParserObserverWrapper*, bool* constructionSuccess);

    CSSParserToken nextToken();

    char16_t consume();
    void reconsume(char16_t);

    String preprocessString(const String&);

    CSSParserToken consumeNumericToken();
    CSSParserToken consumeIdentLikeToken();
    CSSParserToken consumeNumber();
    CSSParserToken consumeStringTokenUntil(char16_t);
    CSSParserToken consumeURLToken();

    void consumeBadUrlRemnants();
    void consumeSingleWhitespaceIfNext();
    void consumeUntilCommentEndFound();

    bool consumeIfNext(char16_t);
    StringView consumeName();
    char32_t consumeEscape();

    bool nextTwoCharsAreValidEscape();
    bool nextCharsAreNumber(char16_t);
    bool nextCharsAreNumber();
    bool nextCharsAreIdentifier(char16_t);
    bool nextCharsAreIdentifier();

    CSSParserToken blockStart(CSSParserTokenType);
    CSSParserToken blockStart(CSSParserTokenType blockType, CSSParserTokenType, StringView);
    CSSParserToken blockEnd(CSSParserTokenType, CSSParserTokenType startType);

    CSSParserToken newline(char16_t);
    CSSParserToken whitespace(char16_t);
    CSSParserToken leftParenthesis(char16_t);
    CSSParserToken rightParenthesis(char16_t);
    CSSParserToken leftBracket(char16_t);
    CSSParserToken rightBracket(char16_t);
    CSSParserToken leftBrace(char16_t);
    CSSParserToken rightBrace(char16_t);
    CSSParserToken plusOrFullStop(char16_t);
    CSSParserToken comma(char16_t);
    CSSParserToken hyphenMinus(char16_t);
    CSSParserToken asterisk(char16_t);
    CSSParserToken lessThan(char16_t);
    CSSParserToken solidus(char16_t);
    CSSParserToken colon(char16_t);
    CSSParserToken semiColon(char16_t);
    CSSParserToken hash(char16_t);
    CSSParserToken circumflexAccent(char16_t);
    CSSParserToken dollarSign(char16_t);
    CSSParserToken verticalLine(char16_t);
    CSSParserToken tilde(char16_t);
    CSSParserToken commercialAt(char16_t);
    CSSParserToken reverseSolidus(char16_t);
    CSSParserToken asciiDigit(char16_t);
    CSSParserToken letterU(char16_t);
    CSSParserToken nameStart(char16_t);
    CSSParserToken stringStart(char16_t);
    CSSParserToken endOfFile(char16_t);

    StringView registerString(const String&);

    using CodePoint = CSSParserToken (CSSTokenizer::*)(char16_t);
    static const std::array<CodePoint, 128> codePoints;

    Vector<CSSParserTokenType, 8> m_blockStack;
    Vector<CSSParserToken, 32> m_tokens;
    // We only allocate strings when escapes are used.
    Vector<String> m_stringPool;
    CSSTokenizerInputStream m_input;
};

} // namespace WebCore
