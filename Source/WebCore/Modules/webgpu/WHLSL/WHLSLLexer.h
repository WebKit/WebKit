/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#if ENABLE(WEBGPU)

#include <wtf/Optional.h>
#include <wtf/Vector.h>
#include <wtf/text/StringView.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

namespace WHLSL {

class Lexer {
public:
    Lexer() = default;

    Lexer(StringView stringView)
        : m_stringView(stringView)
    {
        skipWhitespaceAndComments();
    }

    Lexer(const Lexer&) = delete;
    Lexer(Lexer&&) = default;

    Lexer& operator=(const Lexer&) = delete;
    Lexer& operator=(Lexer&&) = default;

    struct Token {
        Token() = delete;
        Token(const Token&) = default;
        Token(Token&&) = default;
        Token& operator=(const Token&) = default;
        Token& operator=(Token&&) = default;

        StringView stringView;
        unsigned lineNumber;
        enum class Type {
            IntLiteral,
            UintLiteral,
            FloatLiteral,
            Struct,
            Typedef,
            Enum,
            Operator,
            If,
            Else,
            Continue,
            Break,
            Switch,
            Case,
            Default,
            Fallthrough,
            For,
            While,
            Do,
            Return,
            Trap,
            Null,
            True,
            False,
            Constant,
            Device,
            Threadgroup,
            Thread,
            Space,
            Vertex,
            Fragment,
            Compute,
            NumThreads,
            SVInstanceID,
            SVVertexID,
            PSize,
            SVPosition,
            SVIsFrontFace,
            SVSampleIndex,
            SVInnerCoverage,
            SVTarget,
            SVDepth,
            SVCoverage,
            SVDispatchThreadID,
            SVGroupID,
            SVGroupIndex,
            SVGroupThreadID,
            Attribute,
            Register,
            Specialized,
            Native,
            Restricted,
            Underscore,
            Auto,
            Protocol,
            Const,
            Static,
            Qualifier,
            Identifier,
            OperatorName,
            EqualsSign,
            Semicolon,
            LeftCurlyBracket,
            RightCurlyBracket,
            Colon,
            Comma,
            LeftParenthesis,
            RightParenthesis,
            SquareBracketPair,
            LeftSquareBracket,
            RightSquareBracket,
            Star,
            LessThanSign,
            GreaterThanSign,
            FullStop,
            PlusEquals,
            MinusEquals,
            TimesEquals,
            DivideEquals,
            ModEquals,
            XorEquals,
            AndEquals,
            OrEquals,
            RightShiftEquals,
            LeftShiftEquals,
            PlusPlus,
            MinusMinus,
            Arrow,
            QuestionMark,
            OrOr,
            AndAnd,
            Or,
            Xor,
            And,
            LessThanOrEqualTo,
            GreaterThanOrEqualTo,
            EqualComparison,
            NotEqual,
            RightShift,
            LeftShift,
            Plus,
            Minus,
            Divide,
            Mod,
            Tilde,
            ExclamationPoint,
            At,
        } type;

        static const char* typeName(Type);
    };

    std::optional<Token> consumeToken()
    {
        if (!m_stack.isEmpty())
            return m_stack.takeLast();
        return consumeTokenFromStream();
    }

    void unconsumeToken(Token&& token)
    {
        m_stack.append(WTFMove(token));
    }

    struct State {
        Vector<Token> stack;
        unsigned offset;
        unsigned lineNumber;
    };

    State state() const
    {
        return { m_stack, m_offset, m_lineNumber };
    }

    void setState(const State& state)
    {
        m_stack = state.stack;
        m_offset = state.offset;
        m_lineNumber = state.lineNumber;
    }

    void setState(State&& state)
    {
        m_stack = WTFMove(state.stack);
        m_offset = WTFMove(state.offset);
        m_lineNumber = WTFMove(state.lineNumber);
    }

    bool isFullyConsumed() const
    {
        return m_offset == m_stringView.length();
    }

    String errorString(const Token& token, const String& message)
    {
        return String::format("Parse error at line %u: %s", token.lineNumber, message.utf8().data());
    }

private:
    std::optional<Token> consumeTokenFromStream();

    void skipWhitespaceAndComments();
    void skipWhitespace();
    void skipLineComment();
    void skipLongComment();

    std::optional<Token::Type> recognizeKeyword(unsigned end);

    std::optional<unsigned> coreDecimalIntLiteral(unsigned) const;
    std::optional<unsigned> decimalIntLiteral(unsigned) const;
    std::optional<unsigned> decimalUintLiteral(unsigned) const;
    std::optional<unsigned> coreHexadecimalIntLiteral(unsigned) const;
    std::optional<unsigned> hexadecimalIntLiteral(unsigned) const;
    std::optional<unsigned> hexadecimalUintLiteral(unsigned) const;
    std::optional<unsigned> intLiteral(unsigned) const;
    std::optional<unsigned> uintLiteral(unsigned) const;
    std::optional<unsigned> digit(unsigned) const;
    unsigned digitStar(unsigned) const;
    std::optional<unsigned> character(char, unsigned) const;
    template<unsigned length> std::optional<unsigned> anyCharacter(const char (&string)[length], unsigned) const;
    std::optional<unsigned> coreFloatLiteralType1(unsigned) const;
    std::optional<unsigned> coreFloatLiteral(unsigned) const;
    std::optional<unsigned> floatLiteral(unsigned) const;
    template<unsigned length> std::optional<unsigned> string(const char (&string)[length], unsigned) const;
    std::optional<unsigned> validIdentifier(unsigned) const;
    std::optional<unsigned> identifier(unsigned) const;
    std::optional<unsigned> operatorName(unsigned) const;

    StringView m_stringView;
    Vector<Token> m_stack;
    unsigned m_offset { 0 };
    unsigned m_lineNumber { 0 };
};

template<unsigned length> std::optional<unsigned> Lexer::string(const char (&string)[length], unsigned offset) const
{
    for (unsigned i = 0; i < length - 1; ++i) {
        if (offset + i >= m_stringView.length() || m_stringView[offset + i] != string[i])
            return std::nullopt;
    }
    return offset + length - 1;
}

template<unsigned length> std::optional<unsigned> Lexer::anyCharacter(const char (&string)[length], unsigned offset) const
{
    if (offset >= m_stringView.length())
        return std::nullopt;
    for (unsigned i = 0; i < length - 1; ++i) {
        if (m_stringView[offset] == string[i])
            return offset + 1;
    }
    return std::nullopt;
}

}

}

#endif
