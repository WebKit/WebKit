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

// Shader Module

void Visitor::visit(ShaderModule& shaderModule)
{
    for (auto& directive : shaderModule.directives())
        checkErrorAndVisit(directive);
    for (auto& structure : shaderModule.structures())
        checkErrorAndVisit(structure);
    for (auto& variable : shaderModule.variables())
        checkErrorAndVisit(variable);
    for (auto& function : shaderModule.functions())
        checkErrorAndVisit(function);
}

void Visitor::visit(AST::Directive&)
{
}

// Attribute

void Visitor::visit(Attribute& attribute)
{
    switch (attribute.kind()) {
    case AST::NodeKind::AlignAttribute:
        checkErrorAndVisit(downcast<AST::AlignAttribute>(attribute));
        break;
    case AST::NodeKind::BindingAttribute:
        checkErrorAndVisit(downcast<AST::BindingAttribute>(attribute));
        break;
    case AST::NodeKind::BuiltinAttribute:
        checkErrorAndVisit(downcast<AST::BuiltinAttribute>(attribute));
        break;
    case AST::NodeKind::ConstAttribute:
        checkErrorAndVisit(downcast<AST::ConstAttribute>(attribute));
        break;
    case AST::NodeKind::GroupAttribute:
        checkErrorAndVisit(downcast<AST::GroupAttribute>(attribute));
        break;
    case AST::NodeKind::IdAttribute:
        checkErrorAndVisit(downcast<AST::IdAttribute>(attribute));
        break;
    case AST::NodeKind::InterpolateAttribute:
        checkErrorAndVisit(downcast<AST::InterpolateAttribute>(attribute));
        break;
    case AST::NodeKind::InvariantAttribute:
        checkErrorAndVisit(downcast<AST::InvariantAttribute>(attribute));
        break;
    case AST::NodeKind::LocationAttribute:
        checkErrorAndVisit(downcast<AST::LocationAttribute>(attribute));
        break;
    case AST::NodeKind::SizeAttribute:
        checkErrorAndVisit(downcast<AST::SizeAttribute>(attribute));
        break;
    case AST::NodeKind::StageAttribute:
        checkErrorAndVisit(downcast<AST::StageAttribute>(attribute));
        break;
    case AST::NodeKind::WorkgroupSizeAttribute:
        checkErrorAndVisit(downcast<AST::WorkgroupSizeAttribute>(attribute));
        break;
    default:
        ASSERT_NOT_REACHED("Unhandled Attribute");
    }
}

void Visitor::visit(AST::AlignAttribute&)
{
}

void Visitor::visit(AST::BindingAttribute&)
{
}

void Visitor::visit(AST::ConstAttribute&)
{
}

void Visitor::visit(AST::BuiltinAttribute&)
{
}

void Visitor::visit(GroupAttribute&)
{
}

void Visitor::visit(AST::IdAttribute&)
{
}

void Visitor::visit(AST::InterpolateAttribute&)
{
}

void Visitor::visit(AST::InvariantAttribute&)
{
}

void Visitor::visit(AST::LocationAttribute&)
{
}

void Visitor::visit(AST::SizeAttribute&)
{
}

void Visitor::visit(AST::StageAttribute&)
{
}

void Visitor::visit(AST::WorkgroupSizeAttribute&)
{
}

// Expression

void Visitor::visit(Expression& expression)
{
    switch (expression.kind()) {
    case AST::NodeKind::AbstractFloatLiteral:
        checkErrorAndVisit(downcast<AST::AbstractFloatLiteral>(expression));
        break;
    case AST::NodeKind::AbstractIntegerLiteral:
        checkErrorAndVisit(downcast<AST::AbstractIntegerLiteral>(expression));
        break;
    case AST::NodeKind::BinaryExpression:
        checkErrorAndVisit(downcast<AST::BinaryExpression>(expression));
        break;
    case AST::NodeKind::BitcastExpression:
        checkErrorAndVisit(downcast<AST::BitcastExpression>(expression));
        break;
    case AST::NodeKind::BoolLiteral:
        checkErrorAndVisit(downcast<AST::BoolLiteral>(expression));
        break;
    case AST::NodeKind::CallExpression:
        checkErrorAndVisit(downcast<AST::CallExpression>(expression));
        break;
    case AST::NodeKind::FieldAccessExpression:
        checkErrorAndVisit(downcast<AST::FieldAccessExpression>(expression));
        break;
    case AST::NodeKind::Float32Literal:
        checkErrorAndVisit(downcast<AST::Float32Literal>(expression));
        break;
    case AST::NodeKind::IdentifierExpression:
        checkErrorAndVisit(downcast<AST::IdentifierExpression>(expression));
        break;
    case AST::NodeKind::IdentityExpression:
        checkErrorAndVisit(downcast<IdentityExpression>(expression));
        break;
    case AST::NodeKind::IndexAccessExpression:
        checkErrorAndVisit(downcast<AST::IndexAccessExpression>(expression));
        break;
    case AST::NodeKind::Signed32Literal:
        checkErrorAndVisit(downcast<AST::Signed32Literal>(expression));
        break;
    case AST::NodeKind::UnaryExpression:
        checkErrorAndVisit(downcast<AST::UnaryExpression>(expression));
        break;
    case AST::NodeKind::Unsigned32Literal:
        checkErrorAndVisit(downcast<AST::Unsigned32Literal>(expression));
        break;
    default:
        ASSERT_NOT_REACHED("Unhandled Expression");
    }
}

void Visitor::visit(AbstractFloatLiteral&)
{
}

void Visitor::visit(AST::AbstractIntegerLiteral&)
{
}

void Visitor::visit(AST::BinaryExpression& binaryExpression)
{
    checkErrorAndVisit(binaryExpression.leftExpression());
    checkErrorAndVisit(binaryExpression.rightExpression());
}

void Visitor::visit(AST::BitcastExpression& bitcastExpression)
{
    checkErrorAndVisit(bitcastExpression.expression());
}

void Visitor::visit(BoolLiteral&)
{
}

void Visitor::visit(AST::CallExpression& callExpression)
{
    checkErrorAndVisit(callExpression.target());
    for (auto& argument : callExpression.arguments())
        checkErrorAndVisit(argument);
}

void Visitor::visit(AST::FieldAccessExpression& fieldAccessExpression)
{
    checkErrorAndVisit(fieldAccessExpression.base());
}

void Visitor::visit(AST::Float32Literal&)
{
}

void Visitor::visit(AST::IdentifierExpression& identifierExpression)
{
    checkErrorAndVisit(identifierExpression.identifier());
}

void Visitor::visit(AST::IndexAccessExpression& indexAccessExpression)
{
    checkErrorAndVisit(indexAccessExpression.base());
    checkErrorAndVisit(indexAccessExpression.index());
}

void Visitor::visit(AST::PointerDereferenceExpression&)
{
}

void Visitor::visit(AST::Signed32Literal&)
{
}

void Visitor::visit(UnaryExpression& unaryExpression)
{
    checkErrorAndVisit(unaryExpression.expression());
}

void Visitor::visit(AST::Unsigned32Literal&)
{
}

// Function

void Visitor::visit(AST::Function& function)
{
    for (auto& attribute : function.attributes())
        checkErrorAndVisit(attribute);
    for (auto& parameter : function.parameters())
        checkErrorAndVisit(parameter);
    for (auto& attribute : function.returnAttributes())
        checkErrorAndVisit(attribute);
    maybeCheckErrorAndVisit(function.maybeReturnType());
    checkErrorAndVisit(function.body());
}

// Identifier

void Visitor::visit(AST::Identifier&)
{
}

void Visitor::visit(IdentityExpression& identity)
{
    checkErrorAndVisit(identity.expression());
}

// Statement

void Visitor::visit(Statement& statement)
{
    switch (statement.kind()) {
    case AST::NodeKind::AssignmentStatement:
        checkErrorAndVisit(downcast<AST::AssignmentStatement>(statement));
        break;
    case AST::NodeKind::BreakStatement:
        checkErrorAndVisit(downcast<AST::BreakStatement>(statement));
        break;
    case AST::NodeKind::CompoundAssignmentStatement:
        checkErrorAndVisit(downcast<AST::CompoundAssignmentStatement>(statement));
        break;
    case AST::NodeKind::CompoundStatement:
        checkErrorAndVisit(downcast<AST::CompoundStatement>(statement));
        break;
    case AST::NodeKind::ContinueStatement:
        checkErrorAndVisit(downcast<AST::ContinueStatement>(statement));
        break;
    case AST::NodeKind::DecrementIncrementStatement:
        checkErrorAndVisit(downcast<AST::DecrementIncrementStatement>(statement));
        break;
    case AST::NodeKind::DiscardStatement:
        checkErrorAndVisit(downcast<AST::DiscardStatement>(statement));
        break;
    case AST::NodeKind::ForStatement:
        checkErrorAndVisit(downcast<AST::ForStatement>(statement));
        break;
    case AST::NodeKind::IfStatement:
        checkErrorAndVisit(downcast<AST::IfStatement>(statement));
        break;
    case AST::NodeKind::LoopStatement:
        checkErrorAndVisit(downcast<AST::LoopStatement>(statement));
        break;
    case AST::NodeKind::PhonyAssignmentStatement:
        checkErrorAndVisit(downcast<AST::PhonyAssignmentStatement>(statement));
        break;
    case AST::NodeKind::ReturnStatement:
        checkErrorAndVisit(downcast<AST::ReturnStatement>(statement));
        break;
    case AST::NodeKind::StaticAssertStatement:
        checkErrorAndVisit(downcast<AST::StaticAssertStatement>(statement));
        break;
    case AST::NodeKind::SwitchStatement:
        checkErrorAndVisit(downcast<AST::SwitchStatement>(statement));
        break;
    case AST::NodeKind::VariableStatement:
        checkErrorAndVisit(downcast<AST::VariableStatement>(statement));
        break;
    case AST::NodeKind::WhileStatement:
        checkErrorAndVisit(downcast<AST::WhileStatement>(statement));
        break;
    default:
        ASSERT_NOT_REACHED("Unhandled Statement");
    }
}

void Visitor::visit(AST::AssignmentStatement& assignmentStatement)
{
    checkErrorAndVisit(assignmentStatement.lhs());
    checkErrorAndVisit(assignmentStatement.rhs());
}

void Visitor::visit(AST::BreakStatement&)
{
}

void Visitor::visit(AST::CompoundAssignmentStatement& compoundAssignmentStatement)
{
    checkErrorAndVisit(compoundAssignmentStatement.leftExpression());
    checkErrorAndVisit(compoundAssignmentStatement.rightExpression());
}

void Visitor::visit(CompoundStatement& compoundStatement)
{
    for (auto& statement : compoundStatement.statements())
        checkErrorAndVisit(statement);
}

void Visitor::visit(AST::ContinueStatement&)
{
}

void Visitor::visit(AST::DecrementIncrementStatement& decrementIncrementStatement)
{
    checkErrorAndVisit(decrementIncrementStatement.expression());
}

void Visitor::visit(AST::DiscardStatement&)
{
}

void Visitor::visit(AST::ForStatement& forStatement)
{
    maybeCheckErrorAndVisit(forStatement.maybeInitializer());
    maybeCheckErrorAndVisit(forStatement.maybeTest());
    maybeCheckErrorAndVisit(forStatement.maybeUpdate());
    checkErrorAndVisit(forStatement.body());
}

void Visitor::visit(AST::IfStatement& ifStatement)
{
    checkErrorAndVisit(ifStatement.test());
    checkErrorAndVisit(ifStatement.trueBody());
    checkErrorAndVisit(ifStatement.falseBody());
}

void Visitor::visit(AST::LoopStatement& loopStatement)
{
    checkErrorAndVisit(loopStatement.body());
    checkErrorAndVisit(loopStatement.continuingBody());
}

void Visitor::visit(AST::PhonyAssignmentStatement& phonyAssignmentStatement)
{
    checkErrorAndVisit(phonyAssignmentStatement.rhs());
}

void Visitor::visit(AST::ReturnStatement& returnStatement)
{
    maybeCheckErrorAndVisit(returnStatement.maybeExpression());
}

void Visitor::visit(AST::StaticAssertStatement& staticAssertStatement)
{
    checkErrorAndVisit(staticAssertStatement.expression());
}

void Visitor::visit(AST::SwitchStatement&)
{
}

void Visitor::visit(AST::VariableStatement& varStatement)
{
    checkErrorAndVisit(varStatement.variable());
}

void Visitor::visit(AST::WhileStatement& whileStatement)
{
    checkErrorAndVisit(whileStatement.test());
    checkErrorAndVisit(whileStatement.body());
}

// Structure

void Visitor::visit(AST::Structure& structure)
{
    for (auto& attribute : structure.attributes())
        checkErrorAndVisit(attribute);
    for (auto& member : structure.members())
        checkErrorAndVisit(member);
}

void Visitor::visit(AST::StructureMember& structureMember)
{
    for (auto& attribute : structureMember.attributes())
        checkErrorAndVisit(attribute);
    checkErrorAndVisit(structureMember.type());
}

// Types

void Visitor::visit(AST::TypeName& typeName)
{
    switch (typeName.kind()) {
    case AST::NodeKind::ArrayTypeName:
        checkErrorAndVisit(downcast<AST::ArrayTypeName>(typeName));
        break;
    case AST::NodeKind::NamedTypeName:
        checkErrorAndVisit(downcast<AST::NamedTypeName>(typeName));
        break;
    case AST::NodeKind::ParameterizedTypeName:
        checkErrorAndVisit(downcast<AST::ParameterizedTypeName>(typeName));
        break;
    case AST::NodeKind::ReferenceTypeName:
        checkErrorAndVisit(downcast<AST::ReferenceTypeName>(typeName));
        break;
    case AST::NodeKind::StructTypeName:
        checkErrorAndVisit(downcast<AST::StructTypeName>(typeName));
        break;
    default:
        ASSERT_NOT_REACHED("Unhandled TypeName");
    }
}

void Visitor::visit(AST::ArrayTypeName& arrayTypeName)
{
    maybeCheckErrorAndVisit(arrayTypeName.maybeElementType());
    maybeCheckErrorAndVisit(arrayTypeName.maybeElementCount());
}

void Visitor::visit(AST::NamedTypeName&)
{
}

void Visitor::visit(AST::ParameterizedTypeName& parameterizedTypeName)
{
    checkErrorAndVisit(parameterizedTypeName.elementType());
}

void Visitor::visit(AST::ReferenceTypeName& referenceTypeName)
{
    checkErrorAndVisit(referenceTypeName.type());
}

void Visitor::visit(AST::StructTypeName&)
{
}

// Values

void Visitor::visit(AST::Value& value)
{
    switch (value.kind()) {
    case AST::NodeKind::ConstantValue:
        checkErrorAndVisit(downcast<AST::ConstantValue>(value));
        break;
    case AST::NodeKind::OverrideValue:
        checkErrorAndVisit(downcast<AST::OverrideValue>(value));
        break;
    case AST::NodeKind::LetValue:
        checkErrorAndVisit(downcast<AST::LetValue>(value));
        break;
    case AST::NodeKind::ParameterValue:
        checkErrorAndVisit(downcast<AST::ParameterValue>(value));
        break;
    default:
        ASSERT_NOT_REACHED("Unhandled Value");
    }
}

void Visitor::visit(AST::ConstantValue& constantValue)
{
    checkErrorAndVisit(constantValue.name());
    maybeCheckErrorAndVisit(constantValue.maybeTypeName());
    checkErrorAndVisit(constantValue.initializer());
}

void Visitor::visit(AST::OverrideValue& overrideValue)
{
    for (auto& attribute : overrideValue.attributes())
        checkErrorAndVisit(attribute);
    checkErrorAndVisit(overrideValue.name());
    maybeCheckErrorAndVisit(overrideValue.maybeTypeName());
    checkErrorAndVisit(overrideValue.initializer());
}

void Visitor::visit(AST::LetValue& letValue)
{
    checkErrorAndVisit(letValue.name());
    maybeCheckErrorAndVisit(letValue.maybeTypeName());
    checkErrorAndVisit(letValue.initializer());
}

void Visitor::visit(AST::ParameterValue& parameterValue)
{
    for (auto& attribute : parameterValue.attributes())
        checkErrorAndVisit(attribute);
    checkErrorAndVisit(parameterValue.typeName());
}

// Variable

void Visitor::visit(AST::Variable& variable)
{
    for (auto& attribute : variable.attributes())
        checkErrorAndVisit(attribute);
    maybeCheckErrorAndVisit(variable.maybeQualifier());
    maybeCheckErrorAndVisit(variable.maybeTypeName());
    maybeCheckErrorAndVisit(variable.maybeInitializer());
}

void Visitor::visit(VariableQualifier&)
{
}

} // namespace WGSL::AST
