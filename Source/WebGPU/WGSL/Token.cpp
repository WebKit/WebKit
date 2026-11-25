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
#include "Token.h"

namespace WGSL {

RefPtr<StringImpl> toString(TokenType type)
{
    switch (type) {
    case TokenType::Invalid:
        return "Invalid"_si;
    case TokenType::EndOfFile:
        return "EOF"_si;
    case TokenType::AbstractFloatLiteral:
        return "AbstractFloatLiteral"_si;
    case TokenType::IntegerLiteral:
        return "IntegerLiteral"_si;
    case TokenType::IntegerLiteralSigned:
        return "IntegerLiteralSigned"_si;
    case TokenType::IntegerLiteralUnsigned:
        return "IntegerLiteralUnsigned"_si;
    case TokenType::FloatLiteral:
        return "FloatLiteral"_si;
    case TokenType::HalfLiteral:
        return "HalfLiteral"_si;
    case TokenType::Identifier:
        return "Identifier"_si;
    case TokenType::ReservedWord:
        return "ReservedWord"_si;

#define KEYWORD_TO_STRING(lexeme, name) \
    case TokenType::Keyword##name: \
        return #lexeme##_s;
FOREACH_KEYWORD(KEYWORD_TO_STRING)
#undef KEYWORD_TO_STRING

    case TokenType::And:
        return "&"_si;
    case TokenType::AndAnd:
        return "&&"_si;
    case TokenType::AndEq:
        return "&="_si;
    case TokenType::Arrow:
        return "->"_si;
    case TokenType::Attribute:
        return "@"_si;
    case TokenType::Bang:
        return "!"_si;
    case TokenType::BangEq:
        return "!="_si;
    case TokenType::BracketLeft:
        return "["_si;
    case TokenType::BracketRight:
        return "]"_si;
    case TokenType::BraceLeft:
        return "{"_si;
    case TokenType::BraceRight:
        return "}"_si;
    case TokenType::Colon:
        return ":"_si;
    case TokenType::Comma:
        return ","_si;
    case TokenType::Equal:
        return "="_si;
    case TokenType::EqEq:
        return "=="_si;
    case TokenType::TemplateArgsRight:
    case TokenType::Gt:
        return ">"_si;
    case TokenType::GtEq:
        return ">="_si;
    case TokenType::GtGt:
        return ">>"_si;
    case TokenType::GtGtEq:
        return ">>="_si;
    case TokenType::TemplateArgsLeft:
    case TokenType::Lt:
        return "<"_si;
    case TokenType::LtEq:
        return "<="_si;
    case TokenType::LtLt:
        return "<<"_si;
    case TokenType::LtLtEq:
        return "<<="_si;
    case TokenType::Minus:
        return "-"_si;
    case TokenType::MinusMinus:
        return "--"_si;
    case TokenType::MinusEq:
        return "-="_si;
    case TokenType::Modulo:
        return "%"_si;
    case TokenType::ModuloEq:
        return "%="_si;
    case TokenType::Or:
        return "|"_si;
    case TokenType::OrOr:
        return "||"_si;
    case TokenType::OrEq:
        return "|="_si;
    case TokenType::Plus:
        return "+"_si;
    case TokenType::PlusPlus:
        return "++"_si;
    case TokenType::PlusEq:
        return "+="_si;
    case TokenType::Period:
        return "."_si;
    case TokenType::ParenLeft:
        return "("_si;
    case TokenType::ParenRight:
        return ")"_si;
    case TokenType::Semicolon:
        return ";"_si;
    case TokenType::Slash:
        return "/"_si;
    case TokenType::SlashEq:
        return "/="_si;
    case TokenType::Star:
        return "*"_si;
    case TokenType::StarEq:
        return "*="_si;
    case TokenType::Tilde:
        return "~"_si;
    case TokenType::Underbar:
        return "_"_si;
    case TokenType::Xor:
        return "^"_si;
    case TokenType::XorEq:
        return "^="_si;
    case TokenType::Placeholder:
        return "<placeholder>"_si;
    }
}

}
