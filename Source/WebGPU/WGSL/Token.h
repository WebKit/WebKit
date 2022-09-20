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

#include "SourceSpan.h"
#include <wtf/text/StringView.h>
#include <wtf/text/WTFString.h>

namespace WGSL {

enum class TokenType: uint32_t {
    // Instead of having this type, we could have a std::optional<Token> everywhere that we currently have a Token.
    // I made this choice for two reasons:
    // - space efficiency: we don't use an extra word of memory for the variant's tag
    //     (although this part could be solved by using https://github.com/akrzemi1/markable)
    // - ease of use and time efficiency: everywhere that we check for a given TokenType, we would have to first check that the Token is not nullopt, and then check the TokenType.
    Invalid,

    EndOfFile,

    IntegerLiteral,
    IntegerLiteralSigned,
    IntegerLiteralUnsigned,
    DecimalFloatLiteral,
    HexFloatLiteral,

    Identifier,

    ReservedWord,
    KeywordArray,
    KeywordFn,
    KeywordFunction,
    KeywordPrivate,
    KeywordRead,
    KeywordReadWrite,
    KeywordReturn,
    KeywordStorage,
    KeywordStruct,
    KeywordUniform,
    KeywordVar,
    KeywordWorkgroup,
    KeywordWrite,
    KeywordI32,
    KeywordU32,
    KeywordF32,
    KeywordBool,
    LiteralTrue,
    LiteralFalse,
    // FIXME: add all the other keywords: see #keyword-summary in the WGSL spec

    Arrow,
    Attribute,
    BraceLeft,
    BraceRight,
    BracketLeft,
    BracketRight,
    Colon,
    Comma,
    Equal,
    GT,
    LT,
    Minus,
    MinusMinus,
    Period,
    ParenLeft,
    ParenRight,
    Semicolon,
    // FIXME: add all the other special tokens
};

String toString(TokenType);

struct Token {
    TokenType m_type;
    SourceSpan m_span;
    union {
        double m_literalValue;
        StringView m_ident;
    };

    Token(TokenType type, SourcePosition position, unsigned length)
        : m_type(type)
        , m_span(position.m_line, position.m_lineOffset, position.m_offset, length)
    {
        ASSERT(type != TokenType::Identifier);
        ASSERT(type != TokenType::IntegerLiteral);
        ASSERT(type != TokenType::IntegerLiteralSigned);
        ASSERT(type != TokenType::IntegerLiteralUnsigned);
        ASSERT(type != TokenType::DecimalFloatLiteral);
        ASSERT(type != TokenType::HexFloatLiteral);
    }

    Token(TokenType type, SourcePosition position, unsigned length, double literalValue)
        : m_type(type)
        , m_span(position.m_line, position.m_lineOffset, position.m_offset, length)
        , m_literalValue(literalValue)
    {
        ASSERT(type == TokenType::IntegerLiteral
            || type == TokenType::IntegerLiteralSigned
            || type == TokenType::IntegerLiteralUnsigned
            || type == TokenType::DecimalFloatLiteral
            || type == TokenType::HexFloatLiteral);
    }

    Token(TokenType type, SourcePosition position, unsigned length, StringView ident)
        : m_type(type)
        , m_span(position.m_line, position.m_lineOffset, position.m_offset, length)
        , m_ident(ident)
    {
        ASSERT(type == TokenType::Identifier);
    }

    Token& operator=(Token&& other)
    {
        if (m_type == TokenType::Identifier)
            m_ident.~StringView();

        m_type = other.m_type;
        m_span = other.m_span;

        switch (other.m_type) {
        case TokenType::Identifier:
            new (NotNull, &m_ident) StringView();
            m_ident = other.m_ident;
            break;
        case TokenType::IntegerLiteral:
        case TokenType::IntegerLiteralSigned:
        case TokenType::IntegerLiteralUnsigned:
        case TokenType::DecimalFloatLiteral:
        case TokenType::HexFloatLiteral:
            m_literalValue = other.m_literalValue;
            break;
        default:
            break;
        }

        return *this;
    }

    Token(const Token& other)
        : m_type(other.m_type)
        , m_span(other.m_span)
    {
        switch (other.m_type) {
        case TokenType::Identifier:
            new (NotNull, &m_ident) StringView();
            m_ident = other.m_ident;
            break;
        case TokenType::IntegerLiteral:
        case TokenType::IntegerLiteralSigned:
        case TokenType::IntegerLiteralUnsigned:
        case TokenType::DecimalFloatLiteral:
        case TokenType::HexFloatLiteral:
            m_literalValue = other.m_literalValue;
            break;
        default:
            break;
        }
    }

    ~Token()
    {
        if (m_type == TokenType::Identifier)
            (&m_ident)->~StringView();
    }
};

} // namespace WGSL
