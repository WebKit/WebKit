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

#include "AST/ShaderModule.h"
#include "AST/Statements/ReturnStatement.h"
#include "Lexer.h"

namespace WGSL {

namespace AST {
class ArrayType;
}

template<typename Lexer>
class Parser {
public:
    Parser(Lexer& lexer)
        : m_lexer(lexer)
        , m_current(lexer.lex())
    {
    }

    Expected<AST::ShaderModule, Error> parseShader();

    // UniqueRef whenever it can return multiple types.
    Expected<UniqueRef<AST::Decl>, Error> parseGlobalDecl();
    Expected<AST::Attributes, Error> parseAttributes();
    Expected<UniqueRef<AST::Attribute>, Error> parseAttribute();
    Expected<AST::StructDecl, Error> parseStructDecl(AST::Attributes&&);
    Expected<AST::StructMember, Error> parseStructMember();
    Expected<UniqueRef<AST::TypeDecl>, Error> parseTypeDecl();
    Expected<UniqueRef<AST::TypeDecl>, Error> parseTypeDeclAfterIdentifier(StringView&&, SourcePosition start);
    Expected<UniqueRef<AST::TypeDecl>, Error> parseArrayType();
    Expected<AST::VariableDecl, Error> parseVariableDecl();
    Expected<AST::VariableDecl, Error> parseVariableDeclWithAttributes(AST::Attributes&&);
    Expected<AST::VariableQualifier, Error> parseVariableQualifier();
    Expected<AST::StorageClass, Error> parseStorageClass();
    Expected<AST::AccessMode, Error> parseAccessMode();
    Expected<AST::FunctionDecl, Error> parseFunctionDecl(AST::Attributes&&);
    Expected<AST::Parameter, Error> parseParameter();
    Expected<UniqueRef<AST::Statement>, Error> parseStatement();
    Expected<AST::CompoundStatement, Error> parseCompoundStatement();
    Expected<AST::ReturnStatement, Error> parseReturnStatement();
    Expected<UniqueRef<AST::Expression>, Error> parseShortCircuitOrExpression();
    Expected<UniqueRef<AST::Expression>, Error> parseRelationalExpression();
    Expected<UniqueRef<AST::Expression>, Error> parseShiftExpression();
    Expected<UniqueRef<AST::Expression>, Error> parseAdditiveExpression();
    Expected<UniqueRef<AST::Expression>, Error> parseMultiplicativeExpression();
    Expected<UniqueRef<AST::Expression>, Error> parseUnaryExpression();
    Expected<UniqueRef<AST::Expression>, Error> parseSingularExpression();
    Expected<UniqueRef<AST::Expression>, Error> parsePostfixExpression(UniqueRef<AST::Expression>&& base, SourcePosition startPosition);
    Expected<UniqueRef<AST::Expression>, Error> parsePrimaryExpression();
    Expected<UniqueRef<AST::Expression>, Error> parseExpression();
    Expected<UniqueRef<AST::Expression>, Error> parseLHSExpression();
    Expected<UniqueRef<AST::Expression>, Error> parseCoreLHSExpression();
    Expected<Vector<UniqueRef<AST::Expression>>, Error> parseArgumentExpressionList();

private:
    Expected<Token, TokenType> consumeType(TokenType);
    void consume();

    Token& current() { return m_current; }

    Lexer& m_lexer;
    Token m_current;
};

} // namespace WGSL
