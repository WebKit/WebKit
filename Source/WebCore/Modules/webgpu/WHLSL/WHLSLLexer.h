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
#include <wtf/text/StringConcatenate.h>
#include <wtf/text/StringConcatenateNumbers.h>
#include <wtf/text/StringView.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

namespace WHLSL {

struct Token;

namespace AST {

class CodeLocation {
public:
    CodeLocation() = default;
    CodeLocation(unsigned startOffset, unsigned endOffset)
        : m_startOffset(startOffset)
        , m_endOffset(endOffset)
    { }
    inline CodeLocation(const Token&);
    CodeLocation(const CodeLocation& location1, const CodeLocation& location2)
        : m_startOffset(location1.startOffset())
        , m_endOffset(location2.endOffset())
    { }

    unsigned startOffset() const { return m_startOffset; }
    unsigned endOffset() const { return m_endOffset; }

private:
    unsigned m_startOffset { 0 };
    unsigned m_endOffset { 0 };
};

} // namespace AST

class Lexer;

struct Token {
    AST::CodeLocation codeLocation;
    enum class Type : uint8_t {
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
        EndOfFile,
        Invalid
    } type {Type::Invalid};

    static const char* typeName(Type);

    inline StringView stringView(const Lexer&) const;

    unsigned startOffset() const
    {
        return codeLocation.startOffset();
    }
};

AST::CodeLocation::CodeLocation(const Token& token)
    : m_startOffset(token.codeLocation.startOffset())
    , m_endOffset(token.codeLocation.endOffset())
{ }

class Lexer {
public:
    Lexer() = default;

    Lexer(StringView stringView)
        : m_stringView(stringView)
    {
        skipWhitespaceAndComments();
        m_ringBuffer[0] = consumeTokenFromStream();
        m_ringBuffer[1] = consumeTokenFromStream();
    }

    Lexer(const Lexer&) = delete;
    Lexer(Lexer&&) = default;

    Lexer& operator=(const Lexer&) = delete;
    Lexer& operator=(Lexer&&) = default;


    Token consumeToken()
    {
        auto result = m_ringBuffer[m_ringBufferIndex];
        m_ringBuffer[m_ringBufferIndex] = consumeTokenFromStream();
        m_ringBufferIndex = (m_ringBufferIndex + 1) % 2;
        return result;
    }

    Token peek() const
    {
        return m_ringBuffer[m_ringBufferIndex];
    }

    Token peekFurther() const
    {
        return m_ringBuffer[(m_ringBufferIndex + 1) % 2];
    }

    // FIXME: We should not need this
    // https://bugs.webkit.org/show_bug.cgi?id=198357
    struct State {
        Token ringBuffer[2];
        unsigned ringBufferIndex;
        unsigned offset;
    };

    State state() const
    {
        State state;
        state.ringBuffer[0] = m_ringBuffer[0];
        state.ringBuffer[1] = m_ringBuffer[1];
        state.ringBufferIndex = m_ringBufferIndex;
        state.offset = m_offset;
        return state;
    }

    void setState(const State& state)
    {
        m_ringBuffer[0] = state.ringBuffer[0];
        m_ringBuffer[1] = state.ringBuffer[1];
        m_ringBufferIndex = state.ringBufferIndex;
        m_offset = state.offset;

    }

    void setState(State&& state)
    {
        m_ringBuffer[0] = WTFMove(state.ringBuffer[0]);
        m_ringBuffer[1] = WTFMove(state.ringBuffer[1]);
        m_ringBufferIndex = WTFMove(state.ringBufferIndex);
        m_offset = WTFMove(state.offset);
    }

    bool isFullyConsumed() const
    {
        return peek().type == Token::Type::EndOfFile;
    }

    String errorString(const Token& token, const String& message)
    {
        return makeString("Parse error at line ", lineNumberFromOffset(token.startOffset()), ": ", message);
    }

private:
    friend struct Token;
    Token consumeTokenFromStream();

    void skipWhitespaceAndComments();

    unsigned lineNumberFromOffset(unsigned offset);

    Optional<Token::Type> recognizeKeyword(unsigned end);

    Optional<unsigned> coreDecimalIntLiteral(unsigned) const;
    Optional<unsigned> decimalIntLiteral(unsigned) const;
    Optional<unsigned> decimalUintLiteral(unsigned) const;
    Optional<unsigned> coreHexadecimalIntLiteral(unsigned) const;
    Optional<unsigned> hexadecimalIntLiteral(unsigned) const;
    Optional<unsigned> hexadecimalUintLiteral(unsigned) const;
    Optional<unsigned> intLiteral(unsigned) const;
    Optional<unsigned> uintLiteral(unsigned) const;
    Optional<unsigned> digit(unsigned) const;
    unsigned digitStar(unsigned) const;
    Optional<unsigned> character(char, unsigned) const;
    Optional<unsigned> coreFloatLiteralType1(unsigned) const;
    Optional<unsigned> coreFloatLiteral(unsigned) const;
    Optional<unsigned> floatLiteral(unsigned) const;
    template<unsigned length> Optional<unsigned> string(const char (&string)[length], unsigned) const;
    Optional<unsigned> validIdentifier(unsigned) const;
    Optional<unsigned> identifier(unsigned) const;
    Optional<unsigned> completeOperatorName(unsigned) const;

    StringView m_stringView;
    Token m_ringBuffer[2];
    unsigned m_ringBufferIndex { 0 };
    unsigned m_offset { 0 };
};

template<unsigned length> Optional<unsigned> Lexer::string(const char (&string)[length], unsigned offset) const
{
    if (offset + length > m_stringView.length())
        return WTF::nullopt;
    for (unsigned i = 0; i < length - 1; ++i) {
        if (m_stringView[offset + i] != string[i])
            return WTF::nullopt;
    }
    return offset + length - 1;
}

StringView Token::stringView(const Lexer& lexer) const
{
    return lexer.m_stringView.substring(codeLocation.startOffset(), codeLocation.endOffset() - codeLocation.startOffset());
}

} // namespace WHLSL

} // namespace WebCore

#endif // ENABLE(WEBGPU)
