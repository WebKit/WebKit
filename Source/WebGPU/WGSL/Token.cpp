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
        return "Invalid";
    case TokenType::EndOfFile:
        return "EOF";
    case TokenType::IntegerLiteral:
        return "IntegerLiteral";
    case TokenType::IntegerLiteralSigned:
        return "IntegerLiteralSigned";
    case TokenType::IntegerLiteralUnsigned:
        return "IntegerLiteralUnsigned";
    case TokenType::DecimalFloatLiteral:
        return "DecimalFloatLiteral";
    case TokenType::HexFloatLiteral:
        return "HexFloatLiteral";
    case TokenType::Identifier:
        return "Identifier";
    case TokenType::ReservedWord:
        return "ReservedWord";
    case TokenType::KeywordStruct:
        return "struct";
    case TokenType::KeywordFn:
        return "fn";
    case TokenType::KeywordFunction:
        return "function";
    case TokenType::KeywordPrivate:
        return "private";
    case TokenType::KeywordRead:
        return "read";
    case TokenType::KeywordReadWrite:
        return "read_write";
    case TokenType::KeywordReturn:
        return "return";
    case TokenType::KeywordStorage:
        return "storage";
    case TokenType::KeywordUniform:
        return "uniform";
    case TokenType::KeywordVar:
        return "var";
    case TokenType::KeywordWorkgroup:
        return "workgroup";
    case TokenType::KeywordWrite:
        return "write";
    case TokenType::KeywordI32:
        return "i32";
    case TokenType::KeywordU32:
        return "u32";
    case TokenType::KeywordF32:
        return "f32";
    case TokenType::KeywordBool:
        return "bool";
    case TokenType::LiteralTrue:
        return "true";
    case TokenType::LiteralFalse:
        return "false";
    case TokenType::Arrow:
        return "->";
    case TokenType::Attribute:
        return "@";
    case TokenType::BracketLeft:
        return "[";
    case TokenType::BracketRight:
        return "]";
    case TokenType::BraceLeft:
        return "{";
    case TokenType::BraceRight:
        return "}";
    case TokenType::Colon:
        return ":";
    case TokenType::Comma:
        return ",";
    case TokenType::Equal:
        return "=";
    case TokenType::GT:
        return ">";
    case TokenType::LT:
        return "<";
    case TokenType::Period:
        return ".";
    case TokenType::ParenLeft:
        return "(";
    case TokenType::ParenRight:
        return ")";
    case TokenType::Semicolon:
        return ";";
    }
}

}
