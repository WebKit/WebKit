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
    KeywordConst,
    KeywordFn,
    KeywordFunction,
    KeywordLet,
    KeywordOverride,
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

    And,
    AndAnd,
    Arrow,
    Attribute,
    Bang,
    BangEq,
    BraceLeft,
    BraceRight,
    BracketLeft,
    BracketRight,
    Colon,
    Comma,
    Equal,
    EqEq,
    Gt,
    GtEq,
    GtGt,
    Lt,
    LtEq,
    LtLt,
    Minus,
    MinusMinus,
    Modulo,
    Or,
    OrOr,
    ParenLeft,
    ParenRight,
    Period,
    Plus,
    PlusPlus,
    Semicolon,
    Slash,
    Star,
    Tilde,
    Xor,
    // FIXME: add all the other special tokens
};

String toString(TokenType);

struct Token {
    TokenType type;
    SourceSpan span;
    union {
        double literalValue;
        String ident;
    };

    Token(TokenType type, SourcePosition position, unsigned length)
        : type(type)
        , span(position.line, position.lineOffset, position.offset, length)
    {
        ASSERT(type != TokenType::Identifier);
        ASSERT(type != TokenType::IntegerLiteral);
        ASSERT(type != TokenType::IntegerLiteralSigned);
        ASSERT(type != TokenType::IntegerLiteralUnsigned);
        ASSERT(type != TokenType::DecimalFloatLiteral);
        ASSERT(type != TokenType::HexFloatLiteral);
    }

    Token(TokenType type, SourcePosition position, unsigned length, double literalValue)
        : type(type)
        , span(position.line, position.lineOffset, position.offset, length)
        , literalValue(literalValue)
    {
        ASSERT(type == TokenType::IntegerLiteral
            || type == TokenType::IntegerLiteralSigned
            || type == TokenType::IntegerLiteralUnsigned
            || type == TokenType::DecimalFloatLiteral
            || type == TokenType::HexFloatLiteral);
    }

    Token(TokenType type, SourcePosition position, unsigned length, String&& ident)
        : type(type)
        , span(position.line, position.lineOffset, position.offset, length)
        , ident(WTFMove(ident))
    {
        ASSERT(this->ident.impl() && this->ident.impl()->bufferOwnership() == StringImpl::BufferInternal);
        ASSERT(type == TokenType::Identifier);
    }

    Token& operator=(Token&& other)
    {
        if (type == TokenType::Identifier)
            ident.~String();

        type = other.type;
        span = other.span;

        switch (other.type) {
        case TokenType::Identifier:
            new (NotNull, &ident) String();
            ident = other.ident;
            break;
        case TokenType::IntegerLiteral:
        case TokenType::IntegerLiteralSigned:
        case TokenType::IntegerLiteralUnsigned:
        case TokenType::DecimalFloatLiteral:
        case TokenType::HexFloatLiteral:
            literalValue = other.literalValue;
            break;
        default:
            break;
        }

        return *this;
    }

    Token(const Token& other)
        : type(other.type)
        , span(other.span)
    {
        switch (other.type) {
        case TokenType::Identifier:
            new (NotNull, &ident) String();
            ident = other.ident;
            break;
        case TokenType::IntegerLiteral:
        case TokenType::IntegerLiteralSigned:
        case TokenType::IntegerLiteralUnsigned:
        case TokenType::DecimalFloatLiteral:
        case TokenType::HexFloatLiteral:
            literalValue = other.literalValue;
            break;
        default:
            break;
        }
    }

    ~Token()
    {
        if (type == TokenType::Identifier)
            (&ident)->~String();
    }
};

} // namespace WGSL
