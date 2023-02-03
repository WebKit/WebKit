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

String toString(TokenType type)
{
    switch (type) {
    case TokenType::Invalid:
        return "Invalid"_s;
    case TokenType::EndOfFile:
        return "EOF"_s;
    case TokenType::IntegerLiteral:
        return "IntegerLiteral"_s;
    case TokenType::IntegerLiteralSigned:
        return "IntegerLiteralSigned"_s;
    case TokenType::IntegerLiteralUnsigned:
        return "IntegerLiteralUnsigned"_s;
    case TokenType::DecimalFloatLiteral:
        return "DecimalFloatLiteral"_s;
    case TokenType::HexFloatLiteral:
        return "HexFloatLiteral"_s;
    case TokenType::Identifier:
        return "Identifier"_s;
    case TokenType::ReservedWord:
        return "ReservedWord"_s;
    case TokenType::KeywordArray:
        return "array"_s;
    case TokenType::KeywordStruct:
        return "struct"_s;
    case TokenType::KeywordFn:
        return "fn"_s;
    case TokenType::KeywordFunction:
        return "function"_s;
    case TokenType::KeywordPrivate:
        return "private"_s;
    case TokenType::KeywordRead:
        return "read"_s;
    case TokenType::KeywordReadWrite:
        return "read_write"_s;
    case TokenType::KeywordReturn:
        return "return"_s;
    case TokenType::KeywordStorage:
        return "storage"_s;
    case TokenType::KeywordUniform:
        return "uniform"_s;
    case TokenType::KeywordVar:
        return "var"_s;
    case TokenType::KeywordWorkgroup:
        return "workgroup"_s;
    case TokenType::KeywordWrite:
        return "write"_s;
    case TokenType::KeywordI32:
        return "i32"_s;
    case TokenType::KeywordU32:
        return "u32"_s;
    case TokenType::KeywordF32:
        return "f32"_s;
    case TokenType::KeywordBool:
        return "bool"_s;
    case TokenType::LiteralTrue:
        return "true"_s;
    case TokenType::LiteralFalse:
        return "false"_s;
    case TokenType::Arrow:
        return "->"_s;
    case TokenType::Attribute:
        return "@"_s;
    case TokenType::BracketLeft:
        return "["_s;
    case TokenType::BracketRight:
        return "]"_s;
    case TokenType::BraceLeft:
        return "{"_s;
    case TokenType::BraceRight:
        return "}"_s;
    case TokenType::Colon:
        return ":"_s;
    case TokenType::Comma:
        return ","_s;
    case TokenType::Equal:
        return "="_s;
    case TokenType::GT:
        return ">"_s;
    case TokenType::LT:
        return "<"_s;
    case TokenType::Minus:
        return "-"_s;
    case TokenType::MinusMinus:
        return "--"_s;
    case TokenType::Modulo:
        return "%"_s;
    case TokenType::Plus:
        return "+"_s;
    case TokenType::PlusPlus:
        return "++"_s;
    case TokenType::Period:
        return "."_s;
    case TokenType::ParenLeft:
        return "("_s;
    case TokenType::ParenRight:
        return ")"_s;
    case TokenType::Semicolon:
        return ";"_s;
    case TokenType::Slash:
        return "/"_s;
    case TokenType::Star:
        return "*"_s;
    }
}

}
