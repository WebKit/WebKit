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

#include "ASTAttribute.h"
#include "ASTExpression.h"
#include "ASTForward.h"
#include "ASTStatement.h"
#include "ASTStructure.h"
#include "ASTTypeName.h"
#include "ASTVariable.h"
#include "CompilationMessage.h"
#include "Lexer.h"
#include <wtf/Ref.h>

namespace WGSL {

class ShaderModule;

template<typename Lexer>
class Parser {
public:
    Parser(ShaderModule& shaderModule, Lexer& lexer)
        : m_shaderModule(shaderModule)
        , m_lexer(lexer)
        , m_current(lexer.lex())
    {
    }

    Result<void> parseShader();

    // AST::<type>::Ref whenever it can return multiple types.
    Result<AST::Identifier> parseIdentifier();
    Result<void> parseGlobalDecl();
    Result<AST::Attribute::List> parseAttributes();
    Result<AST::Attribute::Ref> parseAttribute();
    Result<AST::Structure::Ref> parseStructure(AST::Attribute::List&&);
    Result<AST::StructureMember> parseStructureMember();
    Result<AST::TypeName::Ref> parseTypeName();
    Result<AST::TypeName::Ref> parseTypeNameAfterIdentifier(AST::Identifier&&, SourcePosition start);
    Result<AST::TypeName::Ref> parseArrayType();
    Result<AST::Variable::Ref> parseVariable();
    Result<AST::Variable::Ref> parseVariableWithAttributes(AST::Attribute::List&&);
    Result<AST::VariableQualifier> parseVariableQualifier();
    Result<AST::StorageClass> parseStorageClass();
    Result<AST::AccessMode> parseAccessMode();
    Result<AST::Function> parseFunction(AST::Attribute::List&&);
    Result<AST::Parameter> parseParameter();
    Result<AST::Statement::Ref> parseStatement();
    Result<AST::CompoundStatement> parseCompoundStatement();
    Result<AST::ReturnStatement> parseReturnStatement();
    Result<AST::Expression::Ref> parseShortCircuitExpression(AST::Expression::Ref&&, TokenType, AST::BinaryOperation);
    Result<AST::Expression::Ref> parseRelationalExpression();
    Result<AST::Expression::Ref> parseRelationalExpressionPostUnary(AST::Expression::Ref&& lhs);
    Result<AST::Expression::Ref> parseShiftExpression();
    Result<AST::Expression::Ref> parseShiftExpressionPostUnary(AST::Expression::Ref&& lhs);
    Result<AST::Expression::Ref> parseAdditiveExpressionPostUnary(AST::Expression::Ref&& lhs);
    Result<AST::Expression::Ref> parseBitwiseExpressionPostUnary(AST::Expression::Ref&& lhs);
    Result<AST::Expression::Ref> parseMultiplicativeExpressionPostUnary(AST::Expression::Ref&& lhs);
    Result<AST::Expression::Ref> parseUnaryExpression();
    Result<AST::Expression::Ref> parseSingularExpression();
    Result<AST::Expression::Ref> parsePostfixExpression(AST::Expression::Ref&& base, SourcePosition startPosition);
    Result<AST::Expression::Ref> parsePrimaryExpression();
    Result<AST::Expression::Ref> parseExpression();
    Result<AST::Expression::Ref> parseLHSExpression();
    Result<AST::Expression::Ref> parseCoreLHSExpression();
    Result<AST::Expression::List> parseArgumentExpressionList();

private:
    Expected<Token, TokenType> consumeType(TokenType);
    template<TokenType... TTs> Expected<Token, TokenType> consumeTypes();

    void consume();

    Token& current() { return m_current; }

    ShaderModule& m_shaderModule;
    Lexer& m_lexer;
    Token m_current;
};

Result<AST::Expression::Ref> parseExpression(const String& source);

} // namespace WGSL
