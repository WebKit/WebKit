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

#include <wtf/SortedArrayMap.h>
#include <wtf/text/StringHash.h>
#include <wtf/unicode/CharacterNames.h>

namespace WGSL {

template <typename T>
Token Lexer<T>::lex()
{
    if (!skipWhitespaceAndComments())
        return makeToken(TokenType::Invalid);

    m_tokenStartingPosition = m_currentPosition;

    if (isAtEndOfFile())
        return makeToken(TokenType::EndOfFile);

    switch (m_current) {
    case '!':
        shift();
        if (m_current == '=') {
            shift();
            return makeToken(TokenType::BangEq);
        }
        return makeToken(TokenType::Bang);
    case '%':
        shift();
        switch (m_current) {
        case '=':
            shift();
            return makeToken(TokenType::ModuloEq);
        default:
            return makeToken(TokenType::Modulo);
        }
    case '&':
        shift();
        switch (m_current) {
        case '&':
            shift();
            return makeToken(TokenType::AndAnd);
        case '=':
            shift();
            return makeToken(TokenType::AndEq);
        default:
            return makeToken(TokenType::And);
        }
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
        if (m_current == '=') {
            shift();
            return makeToken(TokenType::EqEq);
        }
        return makeToken(TokenType::Equal);
    case '>':
        shift();
        switch (m_current) {
        case '=':
            shift();
            return makeToken(TokenType::GtEq);
        case '>':
            shift();
            switch (m_current) {
            case '=':
                shift();
                return makeToken(TokenType::GtGtEq);
            default:
                return makeToken(TokenType::GtGt);
            }
        default:
            return makeToken(TokenType::Gt);
        }
    case '<':
        shift();
        switch (m_current) {
        case '=':
            shift();
            return makeToken(TokenType::LtEq);
        case '<':
            shift();
            switch (m_current) {
            case '=':
                shift();
                return makeToken(TokenType::LtLtEq);
            default:
                return makeToken(TokenType::LtLt);
            }
        default:
            return makeToken(TokenType::Lt);
        }
    case '@':
        shift();
        return makeToken(TokenType::Attribute);
    case '*':
        shift();
        switch (m_current) {
        case '=':
            shift();
            return makeToken(TokenType::StarEq);
        default:
            // FIXME: Report unbalanced block comments, such as "this is an unbalanced comment. */"
            return makeToken(TokenType::Star);
        }
    case '/':
        shift();
        switch (m_current) {
        case '=':
            shift();
            return makeToken(TokenType::SlashEq);
        default:
            return makeToken(TokenType::Slash);
        }
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
        if (m_current == 'f') {
            shift();
            return makeLiteralToken(TokenType::FloatLiteral, literalValue);
        }
        return makeLiteralToken(TokenType::AbstractFloatLiteral, literalValue);
    }
    case '-':
        shift();
        switch (m_current) {
        case '>':
            shift();
            return makeToken(TokenType::Arrow);
        case '-':
            shift();
            return makeToken(TokenType::MinusMinus);
        case '=':
            shift();
            return makeToken(TokenType::MinusEq);
        default:
            return makeToken(TokenType::Minus);
        }
    case '+':
        shift();
        switch (m_current) {
        case '+':
            shift();
            return makeToken(TokenType::PlusPlus);
        case '=':
            shift();
            return makeToken(TokenType::PlusEq);
        default:
            return makeToken(TokenType::Plus);
        }
    case '^':
        shift();
        switch (m_current) {
        case '=':
            shift();
            return makeToken(TokenType::XorEq);
        default:
            return makeToken(TokenType::Xor);
        }
    case '|':
        shift();
        switch (m_current) {
        case '|':
            shift();
            return makeToken(TokenType::OrOr);
        case '=':
            shift();
            return makeToken(TokenType::OrEq);
        default:
            return makeToken(TokenType::Or);
        }
    case '0': {
        shift();
        double literalValue = 0;
        if (m_current == 'x') {
            // FIXME: add support for hexadecimal floating point literals
            shift();
            bool hexNumberIsEmpty = true;
            while (isASCIIHexDigit(m_current)) {
                literalValue *= 16;
                literalValue += toASCIIHexValue(m_current);
                shift();
                hexNumberIsEmpty = false;
            }
            if (hexNumberIsEmpty)
                break;
            return parseIntegerLiteralSuffix(literalValue);
        }

        bool isFloatingPoint = false;
        if (isASCIIDigit(m_current) || m_current == '.' || m_current == 'e' || m_current == 'E') {
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
                    return makeLiteralToken(TokenType::FloatLiteral, literalValue);
                }
            }
            if (std::optional<int64_t> exponent = parseDecimalFloatExponent()) {
                isFloatingPoint = true;
                literalValue *= pow(10, exponent.value());
            }
            // Decimal integers are not allowed to start with 0.
            if (!isFloatingPoint)
                return makeToken(TokenType::Invalid);
        }
        if (m_current == 'f') {
            shift();
            return makeLiteralToken(TokenType::FloatLiteral, literalValue);
        }
        if (isFloatingPoint)
            return makeLiteralToken(TokenType::AbstractFloatLiteral, literalValue);
        return parseIntegerLiteralSuffix(literalValue);
    }
    case '~':
        shift();
        return makeToken(TokenType::Tilde);
    default:
        if (isASCIIDigit(m_current)) {
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
                isFloatingPoint = true;
                literalValue *= pow(10, exponent.value());
            }
            if (m_current == 'f') {
                shift();
                return makeLiteralToken(TokenType::FloatLiteral, literalValue);
            }
            if (!isFloatingPoint)
                return parseIntegerLiteralSuffix(literalValue);
            return makeLiteralToken(TokenType::AbstractFloatLiteral, literalValue);
        } else if (isIdentifierStart(m_current)) {
            const T* startOfToken = m_code;
            shift();
            while (isIdentifierContinue(m_current))
                shift();
            // FIXME: a trie would be more efficient here, look at JavaScriptCore/KeywordLookupGenerator.py for an example of code autogeneration that produces such a trie.
            String view(StringImpl::createWithoutCopying(startOfToken, currentTokenLength()));

            static constexpr std::pair<ComparableASCIILiteral, TokenType> keywordMappings[] {
                { "_", TokenType::Underbar },

#define MAPPING_ENTRY(lexeme, name)\
                { #lexeme, TokenType::Keyword##name },
FOREACH_KEYWORD(MAPPING_ENTRY)
#undef MAPPING_ENTRY

            };
            static constexpr SortedArrayMap keywords { keywordMappings };

            // https://www.w3.org/TR/WGSL/#reserved-words
            static constexpr ComparableASCIILiteral reservedWords[] {
                "NULL",
                "Self",
                "abstract",
                "active",
                "alignas",
                "alignof",
                "as",
                "asm",
                "asm_fragment",
                "async",
                "attribute",
                "auto",
                "await",
                "become",
                "binding_array",
                "cast",
                "catch",
                "class",
                "co_await",
                "co_return",
                "co_yield",
                "coherent",
                "column_major",
                "common",
                "compile",
                "compile_fragment",
                "concept",
                "const_cast",
                "consteval",
                "constexpr",
                "constinit",
                "crate",
                "debugger",
                "decltype",
                "delete",
                "demote",
                "demote_to_helper",
                "do",
                "dynamic_cast",
                "enum",
                "explicit",
                "export",
                "extends",
                "extern",
                "external",
                "fallthrough",
                "filter",
                "final",
                "finally",
                "friend",
                "from",
                "fxgroup",
                "get",
                "goto",
                "groupshared",
                "highp",
                "impl",
                "implements",
                "import",
                "inline",
                "instanceof",
                "interface",
                "layout",
                "lowp",
                "macro",
                "macro_rules",
                "match",
                "mediump",
                "meta",
                "mod",
                "module",
                "move",
                "mut",
                "mutable",
                "namespace",
                "new",
                "nil",
                "noexcept",
                "noinline",
                "nointerpolation",
                "noperspective",
                "null",
                "nullptr",
                "of",
                "operator",
                "package",
                "packoffset",
                "partition",
                "pass",
                "patch",
                "pixelfragment",
                "precise",
                "precision",
                "premerge",
                "priv",
                "protected",
                "pub",
                "public",
                "readonly",
                "ref",
                "regardless",
                "register",
                "reinterpret_cast",
                "require",
                "resource",
                "restrict",
                "self",
                "set",
                "shared",
                "sizeof",
                "smooth",
                "snorm",
                "static",
                "static_assert",
                "static_cast",
                "std",
                "subroutine",
                "super",
                "target",
                "template",
                "this",
                "thread_local",
                "throw",
                "trait",
                "try",
                "type",
                "typedef",
                "typeid",
                "typename",
                "typeof",
                "union",
                "unless",
                "unorm",
                "unsafe",
                "unsized",
                "use",
                "using",
                "varying",
                "virtual",
                "volatile",
                "wgsl",
                "where",
                "with",
                "writeonly",
                "yield",
            };
            static constexpr SortedArraySet reservedWordSet { reservedWords };

            auto tokenType = keywords.get(view);
            if (tokenType != TokenType::Invalid)
                return makeToken(tokenType);

            if (UNLIKELY(reservedWordSet.contains(view)))
                return makeToken(TokenType::ReservedWord);

            return makeIdentifierToken(WTFMove(view));
        }
        break;
    }
    return makeToken(TokenType::Invalid);
}

template <typename T>
T Lexer<T>::shift(unsigned i)
{
    ASSERT(m_code + i <= m_codeEnd);

    T last = m_current;
    // At one point timing showed that setting m_current to 0 unconditionally was faster than an if-else sequence.
    m_current = 0;
    m_code += i;
    m_currentPosition.offset += i;
    m_currentPosition.lineOffset += i;
    if (LIKELY(m_code < m_codeEnd))
        m_current = *m_code;
    return last;
}

template <typename T>
T Lexer<T>::peek(unsigned i)
{
    if (UNLIKELY(m_code + i >= m_codeEnd))
        return 0;
    return *(m_code + i);
}

template <typename T>
void Lexer<T>::newLine()
{
    m_currentPosition.line += 1;
    m_currentPosition.lineOffset = 0;
}

template <typename T>
bool Lexer<T>::skipBlockComments()
{
    ASSERT(peek(0) == '/' && peek(1) == '*');
    shift(2);

    T ch = 0;
    unsigned depth = 1u;

    while (!isAtEndOfFile() && (ch = shift())) {
        if (ch == '/' && peek() == '*') {
            shift();
            depth += 1;
        } else if (ch == '*' && peek() == '/') {
            shift();
            depth -= 1;
            if (!depth) {
                // This block comment is closed, so for a construction like "/* */ */"
                // there will be a successfully parsed block comment "/* */"
                // and " */" will be processed separately.
                return true;
            }
        } else if (ch == '\n')
            newLine();
    }

    // FIXME: Report unbalanced block comments, such as "/* this is an unbalanced comment."
    return false;
}

template <typename T>
void Lexer<T>::skipLineComment()
{
    ASSERT(peek(0) == '/' && peek(1) == '/');
    // Note that in the case of \r\n this makes the comment end on the \r. It is
    // fine, as the \n after that is simple whitespace.
    while (!isAtEndOfFile() && peek() != '\n')
        shift();
}

template <typename T>
bool Lexer<T>::skipWhitespaceAndComments()
{
    while (!isAtEndOfFile()) {
        if (isUnicodeCompatibleASCIIWhitespace(m_current)) {
            if (shift() == '\n')
                newLine();
        } else if (peek(0) == '/') {
            if (peek(1) == '/')
                skipLineComment();
            else if (peek(1) == '*') {
                if (!skipBlockComments())
                    return false;
            } else
                break;
        } else
            break;
    }
    return true;
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
    if (!isASCIIDigit(m_current))
        return std::nullopt;

    CheckedUint64 value = 0;
    while (isASCIIDigit(m_current)) {
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
        if (!isASCIIDigit(char2))
            return std::nullopt;
    } else if (!isASCIIDigit(char1))
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

template class Lexer<LChar>;
template class Lexer<UChar>;

}
