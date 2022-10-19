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
#include "ASTVisitor.h"

#include "AST.h"

namespace WGSL::AST {

bool Visitor::hasError() const
{
    return !m_expectedError;
}

Expected<void, Error> Visitor::result()
{
    return m_expectedError;
}

template<typename T> void Visitor::checkErrorAndVisit(T& x)
{
    if (!hasError())
        visit(x);
}

template<typename T> void Visitor::maybeCheckErrorAndVisit(T* x)
{
    if (!hasError() && x)
        visit(*x);
}

// Shader Module

void Visitor::visit(AST::ShaderModule& shaderModule)
{
    for (auto& directive : shaderModule.directives())
        checkErrorAndVisit(directive);
    for (auto& structDecl : shaderModule.structs())
        checkErrorAndVisit(structDecl);
    for (auto& variableDecl : shaderModule.globalVars())
        checkErrorAndVisit(variableDecl);
    for (auto& functionDecl : shaderModule.functions())
        checkErrorAndVisit(functionDecl);
}

void Visitor::visit(AST::GlobalDirective&)
{
}

// Attribute

void Visitor::visit(AST::Attribute& attribute)
{
    switch (attribute.kind()) {
    case AST::Attribute::Kind::Binding:
        checkErrorAndVisit(downcast<AST::BindingAttribute>(attribute));
        break;
    case AST::Attribute::Kind::Builtin:
        checkErrorAndVisit(downcast<AST::BuiltinAttribute>(attribute));
        break;
    case AST::Attribute::Kind::Group:
        checkErrorAndVisit(downcast<AST::GroupAttribute>(attribute));
        break;
    case AST::Attribute::Kind::Location:
        checkErrorAndVisit(downcast<AST::LocationAttribute>(attribute));
        break;
    case AST::Attribute::Kind::Stage:
        checkErrorAndVisit(downcast<AST::StageAttribute>(attribute));
        break;
    }
}

void Visitor::visit(AST::BindingAttribute&)
{
}

void Visitor::visit(AST::BuiltinAttribute&)
{
}

void Visitor::visit(AST::GroupAttribute&)
{
}

void Visitor::visit(AST::LocationAttribute&)
{
}

void Visitor::visit(AST::StageAttribute&)
{
}

// Declaration

void Visitor::visit(AST::Decl& declaration)
{
    switch (declaration.kind()) {
    case AST::Decl::Kind::Function:
        checkErrorAndVisit(downcast<AST::FunctionDecl>(declaration));
        break;
    case AST::Decl::Kind::Struct:
        checkErrorAndVisit(downcast<AST::StructDecl>(declaration));
        break;
    case AST::Decl::Kind::Variable:
        checkErrorAndVisit(downcast<AST::VariableDecl>(declaration));
        break;
    }
}

void Visitor::visit(AST::FunctionDecl& functionDeclaration)
{
    for (auto& attribute : functionDeclaration.attributes())
        checkErrorAndVisit(attribute);
    for (auto& parameter : functionDeclaration.parameters())
        checkErrorAndVisit(parameter);
    for (auto& attribute : functionDeclaration.returnAttributes())
        checkErrorAndVisit(attribute);
    maybeCheckErrorAndVisit(functionDeclaration.maybeReturnType());
    checkErrorAndVisit(functionDeclaration.body());
}

void Visitor::visit(AST::StructDecl& structDeclaration)
{
    for (auto& attribute : structDeclaration.attributes())
        checkErrorAndVisit(attribute);
    for (auto& member : structDeclaration.members())
        checkErrorAndVisit(member);
}

void Visitor::visit(AST::VariableDecl& varDeclaration)
{
    for (auto& attribute : varDeclaration.attributes())
        checkErrorAndVisit(attribute);
    maybeCheckErrorAndVisit(varDeclaration.maybeQualifier());
    maybeCheckErrorAndVisit(varDeclaration.maybeTypeDecl());
    maybeCheckErrorAndVisit(varDeclaration.maybeInitializer());
}

// Expression

void Visitor::visit(AST::Expression& expression)
{
    switch (expression.kind()) {
    case AST::Expression::Kind::AbstractFloatLiteral:
        checkErrorAndVisit(downcast<AST::AbstractFloatLiteral>(expression));
        break;
    case AST::Expression::Kind::AbstractIntLiteral:
        checkErrorAndVisit(downcast<AST::AbstractIntLiteral>(expression));
        break;
    case AST::Expression::Kind::ArrayAccess:
        checkErrorAndVisit(downcast<AST::ArrayAccess>(expression));
        break;
    case AST::Expression::Kind::BoolLiteral:
        checkErrorAndVisit(downcast<AST::BoolLiteral>(expression));
        break;
    case AST::Expression::Kind::CallableExpression:
        checkErrorAndVisit(downcast<AST::CallableExpression>(expression));
        break;
    case AST::Expression::Kind::Float32Literal:
        checkErrorAndVisit(downcast<AST::Float32Literal>(expression));
        break;
    case AST::Expression::Kind::Identifier:
        checkErrorAndVisit(downcast<AST::IdentifierExpression>(expression));
        break;
    case AST::Expression::Kind::Int32Literal:
        checkErrorAndVisit(downcast<AST::Int32Literal>(expression));
        break;
    case AST::Expression::Kind::StructureAccess:
        checkErrorAndVisit(downcast<AST::StructureAccess>(expression));
        break;
    case AST::Expression::Kind::Uint32Literal:
        checkErrorAndVisit(downcast<AST::Uint32Literal>(expression));
        break;
    case AST::Expression::Kind::UnaryExpression:
        checkErrorAndVisit(downcast<AST::UnaryExpression>(expression));
        break;
    }
}

void Visitor::visit(AST::AbstractFloatLiteral&)
{
}

void Visitor::visit(AST::AbstractIntLiteral&)
{
}

void Visitor::visit(AST::ArrayAccess& arrayAccess)
{
    checkErrorAndVisit(arrayAccess.base());
    checkErrorAndVisit(arrayAccess.index());
}

void Visitor::visit(AST::BoolLiteral&)
{
}

void Visitor::visit(AST::CallableExpression& callableExpression)
{
    checkErrorAndVisit(callableExpression.target());
    for (auto& argument : callableExpression.arguments())
        checkErrorAndVisit(argument);
}

void Visitor::visit(AST::Float32Literal&)
{
}

void Visitor::visit(AST::IdentifierExpression&)
{
}

void Visitor::visit(AST::Int32Literal&)
{
}

void Visitor::visit(AST::StructureAccess& structureAccess)
{
    checkErrorAndVisit(structureAccess.base());
}

void Visitor::visit(AST::Uint32Literal&)
{
}

void Visitor::visit(AST::UnaryExpression& unaryExpression)
{
    checkErrorAndVisit(unaryExpression.expression());
}

// Statement

void Visitor::visit(AST::Statement& statement)
{
    switch (statement.kind()) {
    case AST::Statement::Kind::Assignment:
        checkErrorAndVisit(downcast<AST::AssignmentStatement>(statement));
        break;
    case AST::Statement::Kind::Compound:
        checkErrorAndVisit(downcast<AST::CompoundStatement>(statement));
        break;
    case AST::Statement::Kind::Return:
        checkErrorAndVisit(downcast<AST::ReturnStatement>(statement));
        break;
    case AST::Statement::Kind::Variable:
        checkErrorAndVisit(downcast<AST::VariableStatement>(statement));
        break;
    }
}

void Visitor::visit(AST::AssignmentStatement& assignStatement)
{
    maybeCheckErrorAndVisit(assignStatement.maybeLhs());
    checkErrorAndVisit(assignStatement.rhs());
}

void Visitor::visit(AST::CompoundStatement& compoundStatement)
{
    for (auto& statement : compoundStatement.statements())
        checkErrorAndVisit(statement);
}

void Visitor::visit(AST::ReturnStatement& returnStatement)
{
    maybeCheckErrorAndVisit(returnStatement.maybeExpression());
}

void Visitor::visit(AST::VariableStatement& varStatement)
{
    checkErrorAndVisit(varStatement.declaration());
}

// Types

void Visitor::visit(AST::TypeDecl& typeDecl)
{
    switch (typeDecl.kind()) {
    case AST::TypeDecl::Kind::Array:
        checkErrorAndVisit(downcast<AST::ArrayType>(typeDecl));
        break;
    case AST::TypeDecl::Kind::Named:
        checkErrorAndVisit(downcast<AST::NamedType>(typeDecl));
        break;
    case AST::TypeDecl::Kind::Parameterized:
        checkErrorAndVisit(downcast<AST::ParameterizedType>(typeDecl));
        break;
    }
}

void Visitor::visit(AST::ArrayType& arrayType)
{
    maybeCheckErrorAndVisit(arrayType.maybeElementType());
    maybeCheckErrorAndVisit(arrayType.maybeElementCount());
}

void Visitor::visit(AST::NamedType&)
{
}

void Visitor::visit(AST::ParameterizedType& parameterizedType)
{
    checkErrorAndVisit(parameterizedType.elementType());
}

//

void Visitor::visit(AST::Parameter& parameter)
{
    for (auto& attribute : parameter.attributes())
        checkErrorAndVisit(attribute);
    checkErrorAndVisit(parameter.type());
}

void Visitor::visit(AST::StructMember& structMember)
{
    for (auto& attribute : structMember.attributes())
        checkErrorAndVisit(attribute);
    checkErrorAndVisit(structMember.type());
}

void Visitor::visit(AST::VariableQualifier&)
{
}

} // namespace WGSL::AST
