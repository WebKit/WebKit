/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#include "Token.h"
#include <wtf/ASCIICType.h>
#include <wtf/text/WTFString.h>

namespace WGSL {

template<typename T>
class Lexer {
public:
    Lexer(const String& wgsl)
    {
        if constexpr (std::is_same<T, LChar>::value) {
            m_code = wgsl.characters8();
            m_codeEnd = m_code + wgsl.sizeInBytes();
        } else {
            static_assert(std::is_same<T, UChar>::value, "The lexer expects its template parameter to be either LChar or UChar");
            m_code = wgsl.characters16();
            ASSERT(!(wgsl.sizeInBytes() % 2));
            m_codeEnd = m_code + wgsl.sizeInBytes() / 2;
        }

        m_current = (m_code != m_codeEnd) ? *m_code : 0;
        m_currentPosition = { 1, 0, 0 };
    }

    Vector<Token> lex();
    bool isAtEndOfFile() const;

private:
    Token nextToken();
    Token lexNumber();
    unsigned currentOffset() const { return m_currentPosition.offset; }
    unsigned currentTokenLength() const { return currentOffset() - m_tokenStartingPosition.offset; }

    Token makeToken(TokenType type)
    {
        return { type, m_tokenStartingPosition, currentTokenLength() };
    }
    Token makeLiteralToken(TokenType type, double literalValue)
    {
        return { type, m_tokenStartingPosition, currentTokenLength(), literalValue };
    }
    Token makeIdentifierToken(String&& identifier)
    {
        return { WGSL::TokenType::Identifier, m_tokenStartingPosition, currentTokenLength(), WTFMove(identifier) };
    }

    T shift(unsigned = 1);
    T peek(unsigned = 0);
    void newLine();
    bool skipBlockComments();
    void skipLineComment();
    bool skipWhitespaceAndComments();

    static bool isIdentifierStart(T character) { return isASCIIAlpha(character) || character == '_'; }
    static bool isIdentifierContinue(T character) { return isASCIIAlphanumeric(character) || character == '_'; }

    T m_current;
    const T* m_code;
    const T* m_codeEnd;
    SourcePosition m_currentPosition { 0, 0, 0 };
    SourcePosition m_tokenStartingPosition { 0, 0, 0 };
};

}
