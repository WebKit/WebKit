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
#include <wtf/text/StringView.h>
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
    }

    Token lex();
    bool isAtEndOfFile() const;
    SourcePosition currentPosition() const { return m_currentPosition; }

private:
    unsigned currentOffset() const { return m_currentPosition.m_offset; }
    unsigned currentTokenLength() const { return currentOffset() - m_tokenStartingPosition.m_offset; }

    Token makeToken(TokenType type)
    {
        return { type, m_tokenStartingPosition, currentTokenLength() };
    }
    Token makeLiteralToken(TokenType type, double literalValue)
    {
        return { type, m_tokenStartingPosition, currentTokenLength(), literalValue };
    }
    Token makeIdentifierToken(StringView view)
    {
        return { WGSL::TokenType::Identifier, m_tokenStartingPosition, currentTokenLength(), view };
    }

    void shift();
    T peek(unsigned);
    void skipWhitespace();

    // Reads [0-9]+
    std::optional<uint64_t> parseDecimalInteger();
    // Parse pattern (e|E)(\+|-)?[0-9]+f? if it is present, and return the exponent
    std::optional<int64_t> parseDecimalFloatExponent();
    // Checks whether there is an "i" or "u" coming, and return the right kind of literal token
    Token parseIntegerLiteralSuffix(double literalValue);

    static bool isIdentifierStart(T character) { return isASCIIAlpha(character); }
    static bool isValidIdentifierCharacter(T character) { return isASCIIAlphanumeric(character) || character == '_'; }
    static unsigned readDecimal(T character)
    {
        ASSERT(isASCIIDigit(character));
        return character - '0';
    }

    T m_current;
    const T* m_code;
    const T* m_codeEnd;
    SourcePosition m_currentPosition { 0, 0, 0 };
    SourcePosition m_tokenStartingPosition { 0, 0, 0 };
};

}
