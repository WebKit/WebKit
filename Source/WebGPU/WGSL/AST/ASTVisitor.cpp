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

void Visitor::visit(ShaderModule& shaderModule)
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

void Visitor::visit(GlobalDirective&)
{
}

// Attribute

void Visitor::visit(Attribute& attribute)
{
    switch (attribute.kind()) {
    case Node::Kind::BindingAttribute:
        checkErrorAndVisit(downcast<BindingAttribute>(attribute));
        break;
    case Node::Kind::BuiltinAttribute:
        checkErrorAndVisit(downcast<BuiltinAttribute>(attribute));
        break;
    case Node::Kind::GroupAttribute:
        checkErrorAndVisit(downcast<GroupAttribute>(attribute));
        break;
    case Node::Kind::LocationAttribute:
        checkErrorAndVisit(downcast<LocationAttribute>(attribute));
        break;
    case Node::Kind::StageAttribute:
        checkErrorAndVisit(downcast<StageAttribute>(attribute));
        break;
    default:
        ASSERT_NOT_REACHED("Unhandled attribute kind");
    }
}

void Visitor::visit(BindingAttribute&)
{
}

void Visitor::visit(BuiltinAttribute&)
{
}

void Visitor::visit(GroupAttribute&)
{
}

void Visitor::visit(LocationAttribute&)
{
}

void Visitor::visit(StageAttribute&)
{
}

// Declaration

void Visitor::visit(Decl& declaration)
{
    switch (declaration.kind()) {
    case Node::Kind::FunctionDecl:
        checkErrorAndVisit(downcast<FunctionDecl>(declaration));
        break;
    case Node::Kind::StructDecl:
        checkErrorAndVisit(downcast<StructDecl>(declaration));
        break;
    case Node::Kind::VariableDecl:
        checkErrorAndVisit(downcast<VariableDecl>(declaration));
        break;
    default:
        ASSERT_NOT_REACHED("Unhandled declaration kind");
    }
}

void Visitor::visit(FunctionDecl& functionDeclaration)
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

void Visitor::visit(StructDecl& structDeclaration)
{
    for (auto& attribute : structDeclaration.attributes())
        checkErrorAndVisit(attribute);
    for (auto& member : structDeclaration.members())
        checkErrorAndVisit(member);
}

void Visitor::visit(VariableDecl& varDeclaration)
{
    for (auto& attribute : varDeclaration.attributes())
        checkErrorAndVisit(attribute);
    maybeCheckErrorAndVisit(varDeclaration.maybeQualifier());
    maybeCheckErrorAndVisit(varDeclaration.maybeTypeDecl());
    maybeCheckErrorAndVisit(varDeclaration.maybeInitializer());
}

// Expression

void Visitor::visit(Expression& expression)
{
    switch (expression.kind()) {
    case Expression::Kind::AbstractFloatLiteral:
        checkErrorAndVisit(downcast<AbstractFloatLiteral>(expression));
        break;
    case Expression::Kind::AbstractIntLiteral:
        checkErrorAndVisit(downcast<AbstractIntLiteral>(expression));
        break;
    case Expression::Kind::ArrayAccess:
        checkErrorAndVisit(downcast<ArrayAccess>(expression));
        break;
    case Expression::Kind::BoolLiteral:
        checkErrorAndVisit(downcast<BoolLiteral>(expression));
        break;
    case Expression::Kind::CallableExpression:
        checkErrorAndVisit(downcast<CallableExpression>(expression));
        break;
    case Expression::Kind::Float32Literal:
        checkErrorAndVisit(downcast<Float32Literal>(expression));
        break;
    case Expression::Kind::IdentifierExpression:
        checkErrorAndVisit(downcast<IdentifierExpression>(expression));
        break;
    case Expression::Kind::Int32Literal:
        checkErrorAndVisit(downcast<Int32Literal>(expression));
        break;
    case Expression::Kind::StructureAccess:
        checkErrorAndVisit(downcast<StructureAccess>(expression));
        break;
    case Expression::Kind::Uint32Literal:
        checkErrorAndVisit(downcast<Uint32Literal>(expression));
        break;
    case Expression::Kind::UnaryExpression:
        checkErrorAndVisit(downcast<UnaryExpression>(expression));
        break;
    default:
        ASSERT_NOT_REACHED("Unhandled expression kind");
    }
}

void Visitor::visit(AbstractFloatLiteral&)
{
}

void Visitor::visit(AbstractIntLiteral&)
{
}

void Visitor::visit(ArrayAccess& arrayAccess)
{
    checkErrorAndVisit(arrayAccess.base());
    checkErrorAndVisit(arrayAccess.index());
}

void Visitor::visit(BoolLiteral&)
{
}

void Visitor::visit(CallableExpression& callableExpression)
{
    checkErrorAndVisit(callableExpression.target());
    for (auto& argument : callableExpression.arguments())
        checkErrorAndVisit(argument);
}

void Visitor::visit(Float32Literal&)
{
}

void Visitor::visit(IdentifierExpression&)
{
}

void Visitor::visit(Int32Literal&)
{
}

void Visitor::visit(StructureAccess& structureAccess)
{
    checkErrorAndVisit(structureAccess.base());
}

void Visitor::visit(Uint32Literal&)
{
}

void Visitor::visit(UnaryExpression& unaryExpression)
{
    checkErrorAndVisit(unaryExpression.expression());
}

// Statement

void Visitor::visit(Statement& statement)
{
    switch (statement.kind()) {
    case Node::Kind::AssignmentStatement:
        checkErrorAndVisit(downcast<AssignmentStatement>(statement));
        break;
    case Node::Kind::CompoundStatement:
        checkErrorAndVisit(downcast<CompoundStatement>(statement));
        break;
    case Node::Kind::ReturnStatement:
        checkErrorAndVisit(downcast<ReturnStatement>(statement));
        break;
    case Node::Kind::VariableStatement:
        checkErrorAndVisit(downcast<VariableStatement>(statement));
        break;
    default:
        ASSERT_NOT_REACHED("Unhandled statement kind");
    }
}

void Visitor::visit(AssignmentStatement& assignStatement)
{
    maybeCheckErrorAndVisit(assignStatement.maybeLhs());
    checkErrorAndVisit(assignStatement.rhs());
}

void Visitor::visit(CompoundStatement& compoundStatement)
{
    for (auto& statement : compoundStatement.statements())
        checkErrorAndVisit(statement);
}

void Visitor::visit(ReturnStatement& returnStatement)
{
    maybeCheckErrorAndVisit(returnStatement.maybeExpression());
}

void Visitor::visit(VariableStatement& varStatement)
{
    checkErrorAndVisit(varStatement.declaration());
}

// Types

void Visitor::visit(TypeDecl& typeDecl)
{
    switch (typeDecl.kind()) {
    case Node::Kind::ArrayType:
        checkErrorAndVisit(downcast<ArrayType>(typeDecl));
        break;
    case Node::Kind::NamedType:
        checkErrorAndVisit(downcast<NamedType>(typeDecl));
        break;
    case Node::Kind::ParameterizedType:
        checkErrorAndVisit(downcast<ParameterizedType>(typeDecl));
        break;
    default:
        ASSERT_NOT_REACHED("Unhandled type declaration kind");
    }
}

void Visitor::visit(ArrayType& arrayType)
{
    maybeCheckErrorAndVisit(arrayType.maybeElementType());
    maybeCheckErrorAndVisit(arrayType.maybeElementCount());
}

void Visitor::visit(NamedType&)
{
}

void Visitor::visit(ParameterizedType& parameterizedType)
{
    checkErrorAndVisit(parameterizedType.elementType());
}

//

void Visitor::visit(Parameter& parameter)
{
    for (auto& attribute : parameter.attributes())
        checkErrorAndVisit(attribute);
    checkErrorAndVisit(parameter.type());
}

void Visitor::visit(StructMember& structMember)
{
    for (auto& attribute : structMember.attributes())
        checkErrorAndVisit(attribute);
    checkErrorAndVisit(structMember.type());
}

void Visitor::visit(VariableQualifier&)
{
}

} // namespace WGSL::AST
