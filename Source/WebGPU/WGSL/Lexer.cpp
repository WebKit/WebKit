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

#include "config.h"
#include "Lexer.h"

#include <wtf/unicode/CharacterNames.h>

namespace WGSL {

template <typename T>
Token Lexer<T>::lex()
{
    skipWhitespace();

    m_tokenStartingPosition = m_currentPosition;

    if (isAtEndOfFile())
        return makeToken(TokenType::EndOfFile);

    switch (m_current) {
    case '(':
        shift();
        return makeToken(TokenType::ParenLeft);
    case ')':
        shift();
        return makeToken(TokenType::ParenRight);
    case '{':
        shift();
        return makeToken(TokenType::BraceLeft);
    case '}':
        shift();
        return makeToken(TokenType::BraceRight);
    case '[':
        shift();
        return makeToken(TokenType::BracketLeft);
    case ']':
        shift();
        return makeToken(TokenType::BracketRight);
    case ':':
        shift();
        return makeToken(TokenType::Colon);
    case ',':
        shift();
        return makeToken(TokenType::Comma);
    case ';':
        shift();
        return makeToken(TokenType::Semicolon);
    case '=':
        shift();
        return makeToken(TokenType::Equal);
    case '>':
        shift();
        return makeToken(TokenType::GT);
    case '<':
        shift();
        return makeToken(TokenType::LT);
    case '@':
        shift();
        return makeToken(TokenType::Attribute);
    case '.': {
        shift();
        unsigned offset = currentOffset();
        std::optional<uint64_t> postPeriod = parseDecimalInteger();
        if (!postPeriod)
            return makeToken(TokenType::Period);
        double literalValue = postPeriod.value();
        // FIXME: verify that there is no unnaceptable precision loss
        // It should be tested in the CTS, for now let's get something that works
        // Also the same code appears in a few places below.
        literalValue /= pow(10, currentOffset() - offset);
        std::optional<int64_t> exponent = parseDecimalFloatExponent();
        if (exponent)
            literalValue *= pow(10, exponent.value());
        return makeLiteralToken(TokenType::DecimalFloatLiteral, literalValue);
    }
    case '-':
        shift();
        if (m_current == '>') {
            shift();
            return makeToken(TokenType::Arrow);
        }
        break;
    case '0': {
        shift();
        double literalValue = 0;
        if (m_current == 'x') {
            // FIXME: add support for hexadecimal floating point literals
            shift();
            bool hexNumberIsEmpty = true;
            while (isHexadecimal(m_current)) {
                literalValue *= 16;
                literalValue += readHexadecimal(m_current);
                shift();
                hexNumberIsEmpty = false;
            }
            if (hexNumberIsEmpty)
                break;
            return parseIntegerLiteralSuffix(literalValue);
        }

        bool isFloatingPoint = false;
        if (isDecimal(m_current) || m_current == '.' || m_current == 'e' || m_current == 'E') {
            std::optional<uint64_t> integerPart = parseDecimalInteger();
            if (integerPart)
                literalValue = integerPart.value();
            if (m_current == '.') {
                isFloatingPoint = true;
                shift();
                // FIXME: share this code with the [1-9] case
                unsigned offset = currentOffset();
                std::optional<uint64_t> postPeriod = parseDecimalInteger();
                if (postPeriod) {
                    double fractionalPart = postPeriod.value();
                    fractionalPart /= pow(10, currentOffset() - offset);
                    literalValue += fractionalPart;
                }
                if (m_current == 'f') {
                    shift();
                    return makeLiteralToken(TokenType::DecimalFloatLiteral, literalValue);
                }
            }
            if (std::optional<int64_t> exponent = parseDecimalFloatExponent()) {
                literalValue *= pow(10, exponent.value());
                return makeLiteralToken(TokenType::DecimalFloatLiteral, literalValue);
            }
            // Decimal integers are not allowed to start with 0.
            if (!isFloatingPoint)
                return makeToken(TokenType::Invalid);
        }
        if (m_current == 'f') {
            shift();
            return makeLiteralToken(TokenType::DecimalFloatLiteral, literalValue);
        }
        if (isFloatingPoint)
            return makeLiteralToken(TokenType::DecimalFloatLiteral, literalValue);
        return parseIntegerLiteralSuffix(literalValue);
    }
    default:
        if (isDecimal (m_current)) {
            std::optional<uint64_t> value = parseDecimalInteger();
            if (!value)
                return makeToken(TokenType::Invalid);
            double literalValue = value.value();
            bool isFloatingPoint = false;
            if (m_current == '.') {
                isFloatingPoint = true;
                shift();
                unsigned offset = currentOffset();
                std::optional<uint64_t> postPeriod = parseDecimalInteger();
                if (postPeriod) {
                    double fractionalPart = postPeriod.value();
                    fractionalPart /= pow(10, currentOffset() - offset);
                    literalValue += fractionalPart;
                }
            }
            if (std::optional<int64_t> exponent = parseDecimalFloatExponent()) {
                literalValue *= pow(10, exponent.value());
                return makeLiteralToken(TokenType::DecimalFloatLiteral, literalValue);
            }
            if (m_current == 'f') {
                shift();
                return makeLiteralToken(TokenType::DecimalFloatLiteral, literalValue);
            }
            if (!isFloatingPoint)
                return parseIntegerLiteralSuffix(literalValue);
            return makeLiteralToken(TokenType::DecimalFloatLiteral, literalValue);
        } else if (isIdentifierStart(m_current)) {
            const T* startOfToken = m_code;
            shift();
            while (isValidIdentifierCharacter(m_current))
                shift();
            // FIXME: a trie would be more efficient here, look at JavaScriptCore/KeywordLookupGenerator.py for an example of code autogeneration that produces such a trie.
            StringView view { startOfToken, currentTokenLength() };
            // FIXME: I don't think that true/false/f32/u32/i32/bool need to be their own tokens, they could just be regular identifiers.
            if (view == "true")
                return makeToken(TokenType::LiteralTrue);
            if (view == "false")
                return makeToken(TokenType::LiteralFalse);
            if (view == "bool")
                return makeToken(TokenType::KeywordBool);
            if (view == "i32")
                return makeToken(TokenType::KeywordI32);
            if (view == "u32")
                return makeToken(TokenType::KeywordU32);
            if (view == "f32")
                return makeToken(TokenType::KeywordF32);
            if (view == "fn")
                return makeToken(TokenType::KeywordFn);
            if (view == "function")
                return makeToken(TokenType::KeywordFunction);
            if (view == "private")
                return makeToken(TokenType::KeywordPrivate);
            if (view == "read")
                return makeToken(TokenType::KeywordRead);
            if (view == "read_write")
                return makeToken(TokenType::KeywordReadWrite);
            if (view == "return")
                return makeToken(TokenType::KeywordReturn);
            if (view == "storage")
                return makeToken(TokenType::KeywordStorage);
            if (view == "struct")
                return makeToken(TokenType::KeywordStruct);
            if (view == "uniform")
                return makeToken(TokenType::KeywordUniform);
            if (view == "var")
                return makeToken(TokenType::KeywordVar);
            if (view == "workgroup")
                return makeToken(TokenType::KeywordWorkgroup);
            if (view == "write")
                return makeToken(TokenType::KeywordWrite);
            if (view == "asm" || view == "bf16" || view == "const" || view == "do" || view == "enum"
                || view == "f16" || view == "f64" || view == "handle" || view == "i8" || view == "i16"
                || view == "i64" || view == "mat" || view == "premerge" || view == "regardless"
                || view == "typedef" || view == "u8" || view == "u16" || view == "u64" || view == "unless"
                || view == "using" || view == "vec" || view == "void" || view == "while")
                return makeToken(TokenType::ReservedWord);
            return makeIdentifierToken(view);
        }
        break;
    }
    return makeToken(TokenType::Invalid);
}

template <typename T>
void Lexer<T>::shift()
{
    // At one point timing showed that setting m_current to 0 unconditionally was faster than an if-else sequence.
    m_current = 0;
    ++m_code;
    ++m_currentPosition.m_offset;
    ++m_currentPosition.m_lineOffset;
    if (LIKELY(m_code < m_codeEnd))
        m_current = *m_code;
}

template <typename T>
T Lexer<T>::peek(unsigned i)
{
    if (UNLIKELY(m_code + i >= m_codeEnd))
        return 0;
    return *(m_code + i);
}

template <typename T>
void Lexer<T>::skipWhitespace()
{
    while (isWhiteSpace(m_current)) {
        if (m_current == '\n') {
            shift();
            ++m_currentPosition.m_line;
            m_currentPosition.m_lineOffset = 0;
        } else
            shift();
    }
}

template <typename T>
bool Lexer<T>::isAtEndOfFile() const
{
    if (m_code == m_codeEnd) {
        ASSERT(!m_current);
        return true;
    }
    ASSERT(m_code < m_codeEnd);
    return false;
}

template <typename T>
std::optional<uint64_t> Lexer<T>::parseDecimalInteger()
{
    if (!isDecimal(m_current))
        return std::nullopt;

    CheckedUint64 value = 0;
    while (isDecimal(m_current)) {
        value *= 10ull;
        value += readDecimal(m_current);
        shift();
    }
    if (value.hasOverflowed())
        return std::nullopt;
    return { value.value() };
}

// Parse pattern (e|E)(\+|-)?[0-9]+f? if it is present, and return the exponent
template <typename T>
std::optional<int64_t> Lexer<T>::parseDecimalFloatExponent()
{
    T char1 = peek(1);
    T char2 = peek(2);
    // Check for pattern (e|E)(\+|-)?[0-9]+
    if (m_current != 'e' && m_current != 'E')
        return std::nullopt;
    if (char1 == '+' || char1 == '-') {
        if (!isDecimal(char2))
            return std::nullopt;
    } else if (!isDecimal(char1))
        return std::nullopt;
    shift();

    bool negateExponent = false;
    if (m_current == '-') {
        negateExponent = true;
        shift();
    } else if (m_current == '+')
        shift();

    std::optional<int64_t> exponent = parseDecimalInteger();
    if (!exponent)
        return std::nullopt;
    CheckedInt64 exponentValue = exponent.value();
    if (negateExponent)
        exponentValue = - exponentValue;
    if (exponentValue.hasOverflowed())
        return std::nullopt;
    return { exponentValue.value() };
};

template <typename T>
Token Lexer<T>::parseIntegerLiteralSuffix(double literalValue)
{
    if (m_current == 'i') {
        shift();
        return makeLiteralToken(TokenType::IntegerLiteralSigned, literalValue);
    }
    if (m_current == 'u') {
        shift();
        return makeLiteralToken(TokenType::IntegerLiteralUnsigned, literalValue);
    }
    return makeLiteralToken(TokenType::IntegerLiteral, literalValue);
};

template <typename T>
ALWAYS_INLINE bool Lexer<T>::isWhiteSpace(T ch)
{
    switch (ch) {
    case WTF::Unicode::space:
    case WTF::Unicode::tabCharacter:
    case WTF::Unicode::carriageReturn:
    case WTF::Unicode::newlineCharacter:
    case WTF::Unicode::verticalTabulation:
    case WTF::Unicode::formFeed:
        return true;
    default:
        return false;
    }
}

template <typename T>
ALWAYS_INLINE bool Lexer<T>::isIdentifierStart(T ch)
{
    return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z');
}

template <typename T>
ALWAYS_INLINE bool Lexer<T>::isValidIdentifierCharacter(T ch)
{
    return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '_';
}

template <typename T>
ALWAYS_INLINE bool Lexer<T>::isDecimal(T ch)
{
    return (ch >= '0' && ch <= '9');
}

template <typename T>
ALWAYS_INLINE bool Lexer<T>::isHexadecimal(T ch)
{
    return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F');
}

template <typename T>
ALWAYS_INLINE uint64_t Lexer<T>::readDecimal(T ch)
{
    ASSERT(isDecimal(ch));
    return ch - '0';
}

template <typename T>
ALWAYS_INLINE uint64_t Lexer<T>::readHexadecimal(T ch)
{
    ASSERT(isHexadecimal(ch));
    if (ch >= '0' && ch <= '9')
        return ch - '0';
    if (ch >= 'a' && ch <= 'f')
        return ch - 'a';
    ASSERT(ch >= 'A' && ch <= 'F');
    return ch - 'A';
}

template class Lexer<LChar>;
template class Lexer<UChar>;

}

