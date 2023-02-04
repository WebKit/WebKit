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
#include "ASTStructure.h"
#include "ASTTypeName.h"
#include "ASTVariable.h"
#include "CompilationMessage.h"
#include "Lexer.h"
#include <wtf/Ref.h>

namespace WGSL {

struct Configuration;

template<typename Lexer>
class Parser {
public:
    Parser(Lexer& lexer)
        : m_lexer(lexer)
        , m_current(lexer.lex())
    {
    }

    Expected<AST::ShaderModule, Error> parseShader(const String& source, const Configuration&);

    // UniqueRef whenever it can return multiple types.
    Expected<AST::Identifier, Error> parseIdentifier();
    Expected<AST::Node::Ref, Error> parseGlobalDecl();
    Expected<AST::Attribute::List, Error> parseAttributes();
    Expected<AST::Attribute::Ref, Error> parseAttribute();
    Expected<AST::Structure::Ref, Error> parseStructure(AST::Attribute::List&&);
    Expected<AST::StructureMember, Error> parseStructureMember();
    Expected<AST::TypeName::Ref, Error> parseTypeName();
    Expected<AST::TypeName::Ref, Error> parseTypeNameAfterIdentifier(AST::Identifier&&, SourcePosition start);
    Expected<AST::TypeName::Ref, Error> parseArrayType();
    Expected<AST::Variable::Ref, Error> parseVariable();
    Expected<AST::Variable::Ref, Error> parseVariableWithAttributes(AST::Attribute::List&&);
    Expected<AST::VariableQualifier, Error> parseVariableQualifier();
    Expected<AST::StorageClass, Error> parseStorageClass();
    Expected<AST::AccessMode, Error> parseAccessMode();
    Expected<AST::Function, Error> parseFunction(AST::Attribute::List&&);
    Expected<AST::ParameterValue, Error> parseParameterValue();
    Expected<AST::Statement::Ref, Error> parseStatement();
    Expected<AST::CompoundStatement, Error> parseCompoundStatement();
    Expected<AST::ReturnStatement, Error> parseReturnStatement();
    Expected<AST::Expression::Ref, Error> parseShortCircuitOrExpression();
    Expected<AST::Expression::Ref, Error> parseRelationalExpression(AST::Expression::Ref&& lhs);
    Expected<AST::Expression::Ref, Error> parseShiftExpression(AST::Expression::Ref&& lhs);
    Expected<AST::Expression::Ref, Error> parseAdditiveExpression(AST::Expression::Ref&& lhs);
    Expected<AST::Expression::Ref, Error> parseMultiplicativeExpression(AST::Expression::Ref&& lhs);
    Expected<AST::Expression::Ref, Error> parseUnaryExpression();
    Expected<AST::Expression::Ref, Error> parseSingularExpression();
    Expected<AST::Expression::Ref, Error> parsePostfixExpression(AST::Expression::Ref&& base, SourcePosition startPosition);
    Expected<AST::Expression::Ref, Error> parsePrimaryExpression();
    Expected<AST::Expression::Ref, Error> parseExpression();
    Expected<AST::Expression::Ref, Error> parseLHSExpression();
    Expected<AST::Expression::Ref, Error> parseCoreLHSExpression();
    Expected<AST::Expression::List, Error> parseArgumentExpressionList();

private:
    Expected<Token, TokenType> consumeType(TokenType);
    void consume();

    Token& current() { return m_current; }

    Lexer& m_lexer;
    Token m_current;
};

} // namespace WGSL
