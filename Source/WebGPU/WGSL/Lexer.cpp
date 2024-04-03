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

#include "ConstantValue.h"
#include <charconv>
#include <wtf/SortedArrayMap.h>
#include <wtf/dtoa.h>
#include <wtf/text/StringHash.h>
#include <wtf/unicode/CharacterNames.h>

namespace WGSL {

template <typename T>
Vector<Token> Lexer<T>::lex()
{
    Vector<Token> tokens;

    while (true) {
        auto token = nextToken();
        tokens.append(token);
        switch (token.type) {
        case TokenType::GtGtEq:
            tokens.append(makeToken(TokenType::Placeholder));
            FALLTHROUGH;
        case TokenType::GtGt:
        case TokenType::GtEq:
            tokens.append(makeToken(TokenType::Placeholder));
            break;
        default:
            break;
        }

        if (token.type == TokenType::EndOfFile || token.type == TokenType::Invalid)
            break;
    }

    return tokens;
}

template <typename T>
Token Lexer<T>::nextToken()
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
    case '~':
        shift();
        return makeToken(TokenType::Tilde);
    default:
        if (isASCIIDigit(m_current) || m_current == '.')
            return lexNumber();
        if (isIdentifierStart(m_current)) {
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
Token Lexer<T>::lexNumber()
{
    /* Grammar:
    decimal_int_literal:
    | /0[iu]?/
    | /[1-9][0-9]*[iu]?/

    hex_int_literal :
    | /0[xX][0-9a-fA-F]+[iu]?/

    decimal_float_literal:
    | /0[fh]/`
    | /[0-9]*\.[0-9]+([eE][+-]?[0-9]+)?[fh]?/
    | /[0-9]+\.[0-9]*([eE][+-]?[0-9]+)?[fh]?/
    | /[0-9]+[eE][+-]?[0-9]+[fh]?/
    | /[1-9][0-9]*[fh]/

    hex_float_literal:
    | /0[xX][0-9a-fA-F]*\.[0-9a-fA-F]+([pP][+-]?[0-9]+[fh]?)?/
    | /0[xX][0-9a-fA-F]+\.[0-9a-fA-F]*([pP][+-]?[0-9]+[fh]?)?/
    | /0[xX][0-9a-fA-F]+[pP][+-]?[0-9]+[fh]?/
    */

    /* State machine:
    Start -> InitZero (0)
          -> Decimal (1-9)
          -> FloatFractNoIntegral(.)

    InitZero -> End (i, u, f, h, ∅)
             -> Hex (x, X)
             -> Float (0-9)
             -> FloatFract(.)
             -> FloatExponent(e, E)

    Decimal -> End (i, u, f, h, ∅)
            -> Decimal (0-9)
            -> FloatFract(.)
            -> FloatExponent(e, E)

    Float -> Float (0-9)
          -> FloatFract(.)
          -> FloatExponent(e, E)

    FloatFractNoIntegral -> FloatFract (0-9)
                         -> End(∅)

    FloatFract -> FloatFract (0-9)
               -> FloatExponent(e, E)
               -> End(f, h, ∅)

    FloatExponent -> FloatExponentPostSign(+, -)
                  -> FloatExponentNonEmpty(0-9)

    FloatExponentPostSign -> FloatExponentNonEmpty(0-9)

    FloatExponentNonEmpty -> FloatExponentNonEmpty(0-9)
                          -> End(f, h, ∅)

    Hex -> HexNonEmpty(0-9, a-f, A-F)
        -> HexFloatFractNoIntegral(.)

    HexNonEmpty -> HexNonEmpty(0-9, a-f, A-F)
                -> End(i, u, ∅)
                -> HexFloatFract(.)
                -> HexFloatExponentRequireSuffix(p, P)

    HexFloatFractNoIntegral -> HexFloatFract(0-9, a-f, A-F)

    HexFloatFract -> HexFloatFract(0-9, a-f, A-F)
                  -> HexFloatExponent(p, P)
                  -> End(∅)

    HexFloatExponent -> HexFloatExponentNonEmpty(0-9)
                     -> HexFloatExponentPostSign(+, -)

    HexFloatExponentPostSign -> HexFloatExponentNonEmpty(0-9)

    HexFloatExponentNonEmpty -> HexFloatExponentNonEmpty(0-9)
                             -> End(f, h, ∅)

    HexFloatExponentRequireSuffix -> HexFloatExponentRequireSuffixNonEmpty(0-9)
                                  -> HexFloatExponentRequireSuffixPostSign(+, -)

    HexFloatExponentRequireSuffixPostSign -> HexFloatExponentRequireSuffixNonEmpty(0-9)

    HexFloatExponentRequireSuffixNonEmpty -> HexFloatExponentRequireSuffixNonEmpty(0-9)
                                          -> End(f, h)
    */

    enum State : uint8_t {
        Start,

        InitZero,
        Decimal,

        Float,
        FloatFractNoIntegral,
        FloatFract,
        FloatExponent,
        FloatExponentPostSign,
        FloatExponentNonEmpty,

        Hex,
        HexNonEmpty,
        HexFloatFractNoIntegral,
        HexFloatFract,
        HexFloatExponent,
        HexFloatExponentPostSign,
        HexFloatExponentNonEmpty,
        HexFloatExponentRequireSuffix,
        HexFloatExponentRequireSuffixPostSign,
        HexFloatExponentRequireSuffixNonEmpty,

        End,
        EndNoShift,
    };

    auto state = Start;
    char suffix = '\0';
    char exponentSign = '\0';
    bool isHex = false;
    const T* integral = m_code;
    const T* fract = nullptr;
    const T* exponent = nullptr;

    while (true) {
        switch (state) {
        case Start:
            switch (m_current) {
            case '0':
                state = InitZero;
                break;
            case '.':
                state = FloatFractNoIntegral;
                break;
            default:
                ASSERT(isASCIIDigit(m_current));
                state = Decimal;
                break;
            }
            break;

        case InitZero:
            switch (m_current) {
            case 'i':
            case 'u':
            case 'f':
            case 'h':
                state = End;
                suffix = m_current;
                break;

            case 'x':
            case 'X':
                state = Hex;
                break;

            case '.':
                state = FloatFract;
                break;

            case 'e':
            case 'E':
                state = FloatExponent;
                break;

            default:
                if (isASCIIDigit(m_current))
                    state = Float;
                else
                    state = EndNoShift;
            }
            break;

        case Decimal:
            switch (m_current) {
            case 'i':
            case 'u':
            case 'f':
            case 'h':
                state = End;
                suffix = m_current;
                break;

            case '.':
                state = FloatFract;
                break;

            case 'e':
            case 'E':
                state = FloatExponent;
                break;

            default:
                if (!isASCIIDigit(m_current))
                    state = EndNoShift;
            }
            break;

        case Float:
            switch (m_current) {
            case '.':
                state = FloatFract;
                break;

            case 'e':
            case 'E':
                state = FloatExponent;
                break;

            default:
                if (!isASCIIDigit(m_current))
                    return makeToken(TokenType::Invalid);
            }
            break;
        case FloatFractNoIntegral:
            fract = m_code;
            if (!isASCIIDigit(m_current))
                return makeToken(TokenType::Period);
            state = FloatFract;
            break;
        case FloatFract:
            if (!fract)
                fract = m_code;
            switch (m_current) {
            case 'f':
            case 'h':
                state = End;
                suffix = m_current;
                break;

            case 'e':
            case 'E':
                state = FloatExponent;
                break;

            default:
                if (!isASCIIDigit(m_current))
                    state = EndNoShift;
            }
            break;
        case FloatExponent:
            exponent = m_code;
            switch (m_current) {
            case '+':
            case '-':
                exponentSign = m_current;
                state = FloatExponentPostSign;
                break;
            default:
                if (!isASCIIDigit(m_current))
                    return makeToken(TokenType::Invalid);
                state = FloatExponentNonEmpty;
            }
            break;
        case FloatExponentPostSign:
            if (exponentSign == '+')
                exponent = m_code;
            if (!isASCIIDigit(m_current))
                return makeToken(TokenType::Invalid);
            state = FloatExponentNonEmpty;
            break;
        case FloatExponentNonEmpty:
            switch (m_current) {
            case 'f':
            case 'h':
                state = End;
                suffix = m_current;
                break;
            default:
                if (!isASCIIDigit(m_current))
                    state = EndNoShift;
            }
            break;
        case Hex:
            isHex = true;
            integral = m_code;
            if (m_current == '.')
                state = HexFloatFractNoIntegral;
            else if (isASCIIHexDigit(m_current))
                state = HexNonEmpty;
            else
                return makeToken(TokenType::Invalid);
            break;
        case HexNonEmpty:
            switch (m_current) {
            case 'i':
            case 'u':
                state = End;
                suffix = m_current;
                break;

            case 'p':
            case 'P':
                state = HexFloatExponentRequireSuffix;
                break;

            case '.':
                state = HexFloatFract;
                break;

            default:
                if (!isASCIIHexDigit(m_current))
                    state = EndNoShift;
            }
            break;
        case HexFloatFractNoIntegral:
            fract = m_code;
            if (!isASCIIHexDigit(m_current))
                return makeToken(TokenType::Invalid);
            state = HexFloatFract;
            break;
        case HexFloatFract:
            if (!fract)
                fract = m_code;
            if (isASCIIHexDigit(m_current))
                break;
            if (m_current == 'p' || m_current == 'P')
                state = HexFloatExponent;
            else
                state = EndNoShift;
            break;
        case HexFloatExponent:
            exponent = m_code;
            if (isASCIIDigit(m_current))
                state = HexFloatExponentNonEmpty;
            else if (m_current == '+' || m_current == '-') {
                exponentSign = m_current;
                state = HexFloatExponentPostSign;
            } else
                return makeToken(TokenType::Invalid);
            break;
        case HexFloatExponentPostSign:
            if (exponentSign == '+')
                exponent = m_code;
            if (isASCIIDigit(m_current)) {
                state = HexFloatExponentNonEmpty;
                break;
            }
            return makeToken(TokenType::Invalid);
        case HexFloatExponentNonEmpty:
            if (isASCIIDigit(m_current))
                state = HexFloatExponentNonEmpty;
            else if (m_current == 'f' || m_current == 'h') {
                state = End;
                suffix = m_current;
            } else
                state = EndNoShift;
            break;
        case HexFloatExponentRequireSuffix:
            exponent = m_code;
            if (isASCIIDigit(m_current))
                state = HexFloatExponentRequireSuffixNonEmpty;
            else if (m_current == '+' || m_current == '-') {
                exponentSign = m_current;
                state = HexFloatExponentRequireSuffixPostSign;
            } else
                return makeToken(TokenType::Invalid);
            break;
        case HexFloatExponentRequireSuffixPostSign:
            if (exponentSign == '+')
                exponent = m_code;
            if (isASCIIDigit(m_current)) {
                state = HexFloatExponentRequireSuffixNonEmpty;
                break;
            }
            return makeToken(TokenType::Invalid);
        case HexFloatExponentRequireSuffixNonEmpty:
            if (isASCIIDigit(m_current))
                state = HexFloatExponentNonEmpty;
            else if (m_current == 'f' || m_current == 'h') {
                state = End;
                suffix = m_current;
            } else
                return makeToken(TokenType::Invalid);
            break;
        case End:
        case EndNoShift:
            RELEASE_ASSERT_NOT_REACHED();
        }

        if (state == EndNoShift)
            break;
        shift();
        if (state == End)
            break;
    }

    const auto& convert = [&](auto value) -> Token {
        switch (suffix) {
        case 'i': {
            if constexpr (std::is_integral_v<decltype(value)>) {
                if (auto result = convertInteger<int>(value))
                    return makeLiteralToken(TokenType::IntegerLiteralSigned, *result);
            }
            return makeToken(TokenType::Invalid);
        }
        case 'u': {
            if constexpr (std::is_integral_v<decltype(value)>) {
                if (auto result = convertInteger<unsigned>(value))
                    return makeLiteralToken(TokenType::IntegerLiteralUnsigned, *result);
            }
            break;
        }
        case 'f': {
            if (auto result = convertFloat<float>(value))
                return makeLiteralToken(TokenType::FloatLiteral, *result);
            break;
        }
        case 'h':
            if (auto result = convertFloat<half>(value))
                return makeLiteralToken(TokenType::HalfLiteral, *result);
            break;
        default:
            if constexpr (std::is_floating_point_v<decltype(value)>) {
                if (auto result = convertFloat<double>(value))
                    return makeLiteralToken(TokenType::AbstractFloatLiteral, *result);
            } else {
                if (auto result = convertInteger<int64_t>(value))
                    return makeLiteralToken(TokenType::IntegerLiteral, *result);
            }
        }
        return makeToken(TokenType::Invalid);
    };

    auto* end = m_code - (suffix ? 1 : 0);
    if (!fract && !exponent) {
        auto length = static_cast<size_t>(end - integral);
        if (length > 19)
            return makeToken(TokenType::Invalid);

        char ascii[20];
        const char* asciiStart;
        const char* asciiEnd;
        if constexpr (sizeof(T) == 1) {
            asciiStart = bitwise_cast<const char*>(integral);
            asciiEnd = bitwise_cast<const char*>(end);
        } else {
            for (unsigned i = 0; i < length; ++i) {
                auto digit = integral[i];
                RELEASE_ASSERT(isASCIIHexDigit(digit));
                ascii[i] = digit;
            }
            ascii[length] = '\0';
            asciiStart = ascii;
            asciiEnd = ascii + length;
        }

        int64_t result;
        auto base = isHex ? 16 : 10;
        auto remaining = std::from_chars(asciiStart, asciiEnd, result, base);
        RELEASE_ASSERT(remaining.ptr == asciiEnd);
        if (remaining.ec == std::errc::result_out_of_range)
            return makeToken(TokenType::Invalid);
        return convert(result);
    }

    if (!isHex) {
        size_t parsedLength;
        double result = parseDouble(integral, m_code - integral, parsedLength);
        ASSERT(integral + parsedLength == end);
        return convert(result);
    }

    char* parseEnd;
    double result = std::strtod(bitwise_cast<const char*>(integral) - 2, &parseEnd);
    ASSERT(parseEnd == bitwise_cast<const char*>(end));
    return convert(result);
}

template class Lexer<LChar>;
template class Lexer<UChar>;

}
