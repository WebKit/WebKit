/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include "WHLSLVisitor.h"

#if ENABLE(WEBGPU)

#include "WHLSLArrayReferenceType.h"
#include "WHLSLArrayType.h"
#include "WHLSLAssignmentExpression.h"
#include "WHLSLBaseFunctionAttribute.h"
#include "WHLSLBaseSemantic.h"
#include "WHLSLBlock.h"
#include "WHLSLBooleanLiteral.h"
#include "WHLSLBreak.h"
#include "WHLSLBuiltInSemantic.h"
#include "WHLSLCallExpression.h"
#include "WHLSLCommaExpression.h"
#include "WHLSLConstantExpression.h"
#include "WHLSLContinue.h"
#include "WHLSLDereferenceExpression.h"
#include "WHLSLDoWhileLoop.h"
#include "WHLSLDotExpression.h"
#include "WHLSLEffectfulExpressionStatement.h"
#include "WHLSLEnumerationDefinition.h"
#include "WHLSLEnumerationMember.h"
#include "WHLSLEnumerationMemberLiteral.h"
#include "WHLSLExpression.h"
#include "WHLSLFallthrough.h"
#include "WHLSLFloatLiteral.h"
#include "WHLSLForLoop.h"
#include "WHLSLFunctionAttribute.h"
#include "WHLSLFunctionDeclaration.h"
#include "WHLSLFunctionDefinition.h"
#include "WHLSLIfStatement.h"
#include "WHLSLIndexExpression.h"
#include "WHLSLIntegerLiteral.h"
#include "WHLSLLogicalExpression.h"
#include "WHLSLLogicalNotExpression.h"
#include "WHLSLMakeArrayReferenceExpression.h"
#include "WHLSLMakePointerExpression.h"
#include "WHLSLNativeFunctionDeclaration.h"
#include "WHLSLNativeTypeDeclaration.h"
#include "WHLSLNode.h"
#include "WHLSLNullLiteral.h"
#include "WHLSLNumThreadsFunctionAttribute.h"
#include "WHLSLPointerType.h"
#include "WHLSLProgram.h"
#include "WHLSLPropertyAccessExpression.h"
#include "WHLSLQualifier.h"
#include "WHLSLReadModifyWriteExpression.h"
#include "WHLSLReferenceType.h"
#include "WHLSLResourceSemantic.h"
#include "WHLSLReturn.h"
#include "WHLSLSemantic.h"
#include "WHLSLSpecializationConstantSemantic.h"
#include "WHLSLStageInOutSemantic.h"
#include "WHLSLStatement.h"
#include "WHLSLStructureDefinition.h"
#include "WHLSLStructureElement.h"
#include "WHLSLSwitchCase.h"
#include "WHLSLSwitchStatement.h"
#include "WHLSLTernaryExpression.h"
#include "WHLSLTrap.h"
#include "WHLSLType.h"
#include "WHLSLTypeArgument.h"
#include "WHLSLTypeDefinition.h"
#include "WHLSLTypeReference.h"
#include "WHLSLUnsignedIntegerLiteral.h"
#include "WHLSLValue.h"
#include "WHLSLVariableDeclaration.h"
#include "WHLSLVariableDeclarationsStatement.h"
#include "WHLSLVariableReference.h"
#include "WHLSLWhileLoop.h"

namespace WebCore {

namespace WHLSL {

void Visitor::visit(Program& program)
{
    // These visiting functions might add new global statements, so don't use foreach syntax.
    for (size_t i = 0; i < program.typeDefinitions().size(); ++i)
        checkErrorAndVisit(program.typeDefinitions()[i]);
    for (size_t i = 0; i < program.structureDefinitions().size(); ++i)
        checkErrorAndVisit(program.structureDefinitions()[i]);
    for (size_t i = 0; i < program.enumerationDefinitions().size(); ++i)
        checkErrorAndVisit(program.enumerationDefinitions()[i]);
    for (size_t i = 0; i < program.functionDefinitions().size(); ++i)
        checkErrorAndVisit(program.functionDefinitions()[i]);
    for (size_t i = 0; i < program.nativeFunctionDeclarations().size(); ++i)
        checkErrorAndVisit(program.nativeFunctionDeclarations()[i]);
    for (size_t i = 0; i < program.nativeTypeDeclarations().size(); ++i)
        checkErrorAndVisit(program.nativeTypeDeclarations()[i]);
}

void Visitor::visit(AST::UnnamedType& unnamedType)
{
    if (is<AST::TypeReference>(unnamedType))
        checkErrorAndVisit(downcast<AST::TypeReference>(unnamedType));
    else if (is<AST::PointerType>(unnamedType))
        checkErrorAndVisit(downcast<AST::PointerType>(unnamedType));
    else if (is<AST::ArrayReferenceType>(unnamedType))
        checkErrorAndVisit(downcast<AST::ArrayReferenceType>(unnamedType));
    else {
        ASSERT(is<AST::ArrayType>(unnamedType));
        checkErrorAndVisit(downcast<AST::ArrayType>(unnamedType));
    }
}

void Visitor::visit(AST::NamedType& namedType)
{
    if (is<AST::TypeDefinition>(namedType))
        checkErrorAndVisit(downcast<AST::TypeDefinition>(namedType));
    else if (is<AST::StructureDefinition>(namedType))
        checkErrorAndVisit(downcast<AST::StructureDefinition>(namedType));
    else if (is<AST::EnumerationDefinition>(namedType))
        checkErrorAndVisit(downcast<AST::EnumerationDefinition>(namedType));
    else {
        ASSERT(is<AST::NativeTypeDeclaration>(namedType));
        checkErrorAndVisit(downcast<AST::NativeTypeDeclaration>(namedType));
    }
}

void Visitor::visit(AST::TypeDefinition& typeDefinition)
{
    checkErrorAndVisit(typeDefinition.type());
}

void Visitor::visit(AST::StructureDefinition& structureDefinition)
{
    for (auto& structureElement : structureDefinition.structureElements())
        checkErrorAndVisit(structureElement);
}

void Visitor::visit(AST::EnumerationDefinition& enumerationDefinition)
{
    checkErrorAndVisit(enumerationDefinition.type());
    for (auto& enumerationMember : enumerationDefinition.enumerationMembers())
        checkErrorAndVisit(enumerationMember);
}

void Visitor::visit(AST::FunctionDefinition& functionDefinition)
{
    checkErrorAndVisit(static_cast<AST::FunctionDeclaration&>(functionDefinition));
    checkErrorAndVisit(functionDefinition.block());
}

void Visitor::visit(AST::NativeFunctionDeclaration& nativeFunctionDeclaration)
{
    checkErrorAndVisit(static_cast<AST::FunctionDeclaration&>(nativeFunctionDeclaration));
}

void Visitor::visit(AST::NativeTypeDeclaration& nativeTypeDeclaration)
{
    for (auto& typeArgument : nativeTypeDeclaration.typeArguments())
        checkErrorAndVisit(typeArgument);
}

void Visitor::visit(AST::TypeReference& typeReference)
{
    for (auto& typeArgument : typeReference.typeArguments())
        checkErrorAndVisit(typeArgument);
    if (typeReference.resolvedType() && is<AST::TypeDefinition>(*typeReference.resolvedType())) {
        auto& typeDefinition = downcast<AST::TypeDefinition>(*typeReference.resolvedType());
        checkErrorAndVisit(typeDefinition.type());
    }
}

void Visitor::visit(AST::PointerType& pointerType)
{
    checkErrorAndVisit(static_cast<AST::ReferenceType&>(pointerType));
}

void Visitor::visit(AST::ArrayReferenceType& arrayReferenceType)
{
    checkErrorAndVisit(static_cast<AST::ReferenceType&>(arrayReferenceType));
}

void Visitor::visit(AST::ArrayType& arrayType)
{
    checkErrorAndVisit(arrayType.type());
}

void Visitor::visit(AST::StructureElement& structureElement)
{
    checkErrorAndVisit(structureElement.type());
    if (structureElement.semantic())
        checkErrorAndVisit(*structureElement.semantic());
}

void Visitor::visit(AST::EnumerationMember& enumerationMember)
{
    if (enumerationMember.value())
        checkErrorAndVisit(*enumerationMember.value());
}

void Visitor::visit(AST::FunctionDeclaration& functionDeclaration)
{
    checkErrorAndVisit(functionDeclaration.attributeBlock());
    checkErrorAndVisit(functionDeclaration.type());
    for (auto& parameter : functionDeclaration.parameters())
        checkErrorAndVisit(parameter);
    if (functionDeclaration.semantic())
        checkErrorAndVisit(*functionDeclaration.semantic());
}

void Visitor::visit(AST::TypeArgument& typeArgument)
{
    WTF::visit(WTF::makeVisitor([&](AST::ConstantExpression& constantExpression) {
        checkErrorAndVisit(constantExpression);
    }, [&](UniqueRef<AST::TypeReference>& typeReference) {
        checkErrorAndVisit(typeReference);
    }), typeArgument);
}

void Visitor::visit(AST::ReferenceType& referenceType)
{
    checkErrorAndVisit(referenceType.elementType());
}

void Visitor::visit(AST::Semantic& semantic)
{
    WTF::visit(WTF::makeVisitor([&](AST::BuiltInSemantic& builtInSemantic) {
        checkErrorAndVisit(builtInSemantic);
    }, [&](AST::ResourceSemantic& resourceSemantic) {
        checkErrorAndVisit(resourceSemantic);
    }, [&](AST::SpecializationConstantSemantic& specializationConstantSemantic) {
        checkErrorAndVisit(specializationConstantSemantic);
    }, [&](AST::StageInOutSemantic& stageInOutSemantic) {
        checkErrorAndVisit(stageInOutSemantic);
    }), semantic);
}

void Visitor::visit(AST::ConstantExpression& constantExpression)
{
    constantExpression.visit(WTF::makeVisitor([&](AST::IntegerLiteral& integerLiteral) {
        checkErrorAndVisit(integerLiteral);
    }, [&](AST::UnsignedIntegerLiteral& unsignedIntegerLiteral) {
        checkErrorAndVisit(unsignedIntegerLiteral);
    }, [&](AST::FloatLiteral& floatLiteral) {
        checkErrorAndVisit(floatLiteral);
    }, [&](AST::NullLiteral& nullLiteral) {
        checkErrorAndVisit(nullLiteral);
    }, [&](AST::BooleanLiteral& booleanLiteral) {
        checkErrorAndVisit(booleanLiteral);
    }, [&](AST::EnumerationMemberLiteral& enumerationMemberLiteral) {
        checkErrorAndVisit(enumerationMemberLiteral);
    }));
}

void Visitor::visit(AST::AttributeBlock& attributeBlock)
{
    for (auto& functionAttribute : attributeBlock)
        checkErrorAndVisit(functionAttribute);
}

void Visitor::visit(AST::BuiltInSemantic&)
{
}

void Visitor::visit(AST::ResourceSemantic&)
{
}

void Visitor::visit(AST::SpecializationConstantSemantic&)
{
}

void Visitor::visit(AST::StageInOutSemantic&)
{
}

void Visitor::visit(AST::IntegerLiteral& integerLiteral)
{
    checkErrorAndVisit(integerLiteral.type());
}

void Visitor::visit(AST::UnsignedIntegerLiteral& unsignedIntegerLiteral)
{
    checkErrorAndVisit(unsignedIntegerLiteral.type());
}

void Visitor::visit(AST::FloatLiteral& floatLiteral)
{
    checkErrorAndVisit(floatLiteral.type());
}

void Visitor::visit(AST::NullLiteral& nullLiteral)
{
    checkErrorAndVisit(nullLiteral.type());
}

void Visitor::visit(AST::BooleanLiteral&)
{
}

void Visitor::visit(AST::IntegerLiteralType& integerLiteralType)
{
    if (integerLiteralType.resolvedType())
        checkErrorAndVisit(*integerLiteralType.resolvedType());
    checkErrorAndVisit(integerLiteralType.preferredType());
}

void Visitor::visit(AST::UnsignedIntegerLiteralType& unsignedIntegerLiteralType)
{
    if (unsignedIntegerLiteralType.resolvedType())
        checkErrorAndVisit(*unsignedIntegerLiteralType.resolvedType());
    checkErrorAndVisit(unsignedIntegerLiteralType.preferredType());
}

void Visitor::visit(AST::FloatLiteralType& floatLiteralType)
{
    if (floatLiteralType.resolvedType())
        checkErrorAndVisit(*floatLiteralType.resolvedType());
    checkErrorAndVisit(floatLiteralType.preferredType());
}

void Visitor::visit(AST::NullLiteralType& nullLiteralType)
{
    if (nullLiteralType.resolvedType())
        checkErrorAndVisit(*nullLiteralType.resolvedType());
}

void Visitor::visit(AST::EnumerationMemberLiteral&)
{
}

void Visitor::visit(AST::FunctionAttribute& functionAttribute)
{
    WTF::visit(WTF::makeVisitor([&](AST::NumThreadsFunctionAttribute& numThreadsFunctionAttribute) {
        checkErrorAndVisit(numThreadsFunctionAttribute);
    }), functionAttribute);
}

void Visitor::visit(AST::NumThreadsFunctionAttribute&)
{
}

void Visitor::visit(AST::Block& block)
{
    for (auto& statement : block.statements())
        checkErrorAndVisit(statement);
}

void Visitor::visit(AST::Statement& statement)
{
    if (is<AST::Block>(statement))
        checkErrorAndVisit(downcast<AST::Block>(statement));
    else if (is<AST::Break>(statement))
        checkErrorAndVisit(downcast<AST::Break>(statement));
    else if (is<AST::Continue>(statement))
        checkErrorAndVisit(downcast<AST::Continue>(statement));
    else if (is<AST::DoWhileLoop>(statement))
        checkErrorAndVisit(downcast<AST::DoWhileLoop>(statement));
    else if (is<AST::EffectfulExpressionStatement>(statement))
        checkErrorAndVisit(downcast<AST::EffectfulExpressionStatement>(statement));
    else if (is<AST::Fallthrough>(statement))
        checkErrorAndVisit(downcast<AST::Fallthrough>(statement));
    else if (is<AST::ForLoop>(statement))
        checkErrorAndVisit(downcast<AST::ForLoop>(statement));
    else if (is<AST::IfStatement>(statement))
        checkErrorAndVisit(downcast<AST::IfStatement>(statement));
    else if (is<AST::Return>(statement))
        checkErrorAndVisit(downcast<AST::Return>(statement));
    else if (is<AST::SwitchCase>(statement))
        checkErrorAndVisit(downcast<AST::SwitchCase>(statement));
    else if (is<AST::SwitchStatement>(statement))
        checkErrorAndVisit(downcast<AST::SwitchStatement>(statement));
    else if (is<AST::Trap>(statement))
        checkErrorAndVisit(downcast<AST::Trap>(statement));
    else if (is<AST::VariableDeclarationsStatement>(statement))
        checkErrorAndVisit(downcast<AST::VariableDeclarationsStatement>(statement));
    else {
        ASSERT(is<AST::WhileLoop>(statement));
        checkErrorAndVisit(downcast<AST::WhileLoop>(statement));
    }
}

void Visitor::visit(AST::Break&)
{
}

void Visitor::visit(AST::Continue&)
{
}

void Visitor::visit(AST::DoWhileLoop& doWhileLoop)
{
    checkErrorAndVisit(doWhileLoop.body());
    checkErrorAndVisit(doWhileLoop.conditional());
}

void Visitor::visit(AST::Expression& expression)
{
    if (is<AST::AssignmentExpression>(expression))
        checkErrorAndVisit(downcast<AST::AssignmentExpression>(expression));
    else if (is<AST::BooleanLiteral>(expression))
        checkErrorAndVisit(downcast<AST::BooleanLiteral>(expression));
    else if (is<AST::CallExpression>(expression))
        checkErrorAndVisit(downcast<AST::CallExpression>(expression));
    else if (is<AST::CommaExpression>(expression))
        checkErrorAndVisit(downcast<AST::CommaExpression>(expression));
    else if (is<AST::DereferenceExpression>(expression))
        checkErrorAndVisit(downcast<AST::DereferenceExpression>(expression));
    else if (is<AST::FloatLiteral>(expression))
        checkErrorAndVisit(downcast<AST::FloatLiteral>(expression));
    else if (is<AST::IntegerLiteral>(expression))
        checkErrorAndVisit(downcast<AST::IntegerLiteral>(expression));
    else if (is<AST::LogicalExpression>(expression))
        checkErrorAndVisit(downcast<AST::LogicalExpression>(expression));
    else if (is<AST::LogicalNotExpression>(expression))
        checkErrorAndVisit(downcast<AST::LogicalNotExpression>(expression));
    else if (is<AST::MakeArrayReferenceExpression>(expression))
        checkErrorAndVisit(downcast<AST::MakeArrayReferenceExpression>(expression));
    else if (is<AST::MakePointerExpression>(expression))
        checkErrorAndVisit(downcast<AST::MakePointerExpression>(expression));
    else if (is<AST::NullLiteral>(expression))
        checkErrorAndVisit(downcast<AST::NullLiteral>(expression));
    else if (is<AST::DotExpression>(expression))
        checkErrorAndVisit(downcast<AST::DotExpression>(expression));
    else if (is<AST::IndexExpression>(expression))
        checkErrorAndVisit(downcast<AST::IndexExpression>(expression));
    else if (is<AST::ReadModifyWriteExpression>(expression))
        checkErrorAndVisit(downcast<AST::ReadModifyWriteExpression>(expression));
    else if (is<AST::TernaryExpression>(expression))
        checkErrorAndVisit(downcast<AST::TernaryExpression>(expression));
    else if (is<AST::UnsignedIntegerLiteral>(expression))
        checkErrorAndVisit(downcast<AST::UnsignedIntegerLiteral>(expression));
    else if (is<AST::EnumerationMemberLiteral>(expression))
        checkErrorAndVisit(downcast<AST::EnumerationMemberLiteral>(expression));
    else {
        ASSERT(is<AST::VariableReference>(expression));
        checkErrorAndVisit(downcast<AST::VariableReference>(expression));
    }
}

void Visitor::visit(AST::DotExpression& dotExpression)
{
    checkErrorAndVisit(static_cast<AST::PropertyAccessExpression&>(dotExpression));
}

void Visitor::visit(AST::IndexExpression& indexExpression)
{
    checkErrorAndVisit(indexExpression.indexExpression());
    checkErrorAndVisit(static_cast<AST::PropertyAccessExpression&>(indexExpression));
}

void Visitor::visit(AST::PropertyAccessExpression& expression)
{
    checkErrorAndVisit(expression.base());
}

void Visitor::visit(AST::EffectfulExpressionStatement& effectfulExpressionStatement)
{
    checkErrorAndVisit(effectfulExpressionStatement.effectfulExpression());
}

void Visitor::visit(AST::Fallthrough&)
{
}

void Visitor::visit(AST::ForLoop& forLoop)
{
    WTF::visit(WTF::makeVisitor([&](AST::VariableDeclarationsStatement& variableDeclarationsStatement) {
        checkErrorAndVisit(variableDeclarationsStatement);
    }, [&](UniqueRef<AST::Expression>& expression) {
        checkErrorAndVisit(expression);
    }), forLoop.initialization());
    if (forLoop.condition())
        checkErrorAndVisit(*forLoop.condition());
    if (forLoop.increment())
        checkErrorAndVisit(*forLoop.increment());
    checkErrorAndVisit(forLoop.body());
}

void Visitor::visit(AST::IfStatement& ifStatement)
{
    checkErrorAndVisit(ifStatement.conditional());
    checkErrorAndVisit(ifStatement.body());
    if (ifStatement.elseBody())
        checkErrorAndVisit(*ifStatement.elseBody());
}

void Visitor::visit(AST::Return& returnStatement)
{
    if (returnStatement.value())
        checkErrorAndVisit(*returnStatement.value());
}

void Visitor::visit(AST::SwitchCase& switchCase)
{
    if (switchCase.value())
        checkErrorAndVisit(*switchCase.value());
    checkErrorAndVisit(switchCase.block());
}

void Visitor::visit(AST::SwitchStatement& switchStatement)
{
    checkErrorAndVisit(switchStatement.value());
    for (auto& switchCase : switchStatement.switchCases())
        checkErrorAndVisit(switchCase);
}

void Visitor::visit(AST::Trap&)
{
}

void Visitor::visit(AST::VariableDeclarationsStatement& variableDeclarationsStatement)
{
    for (auto& variableDeclaration : variableDeclarationsStatement.variableDeclarations())
        checkErrorAndVisit(variableDeclaration);
}

void Visitor::visit(AST::WhileLoop& whileLoop)
{
    checkErrorAndVisit(whileLoop.conditional());
    checkErrorAndVisit(whileLoop.body());
}

void Visitor::visit(AST::VariableDeclaration& variableDeclaration)
{
    if (variableDeclaration.type())
        checkErrorAndVisit(*variableDeclaration.type());
    if (variableDeclaration.semantic())
        checkErrorAndVisit(*variableDeclaration.semantic());
    if (variableDeclaration.initializer())
        checkErrorAndVisit(*variableDeclaration.initializer());
}

void Visitor::visit(AST::AssignmentExpression& assignmentExpression)
{
    checkErrorAndVisit(assignmentExpression.left());
    checkErrorAndVisit(assignmentExpression.right());
}

void Visitor::visit(AST::CallExpression& callExpression)
{
    for (auto& argument : callExpression.arguments())
        checkErrorAndVisit(argument);
    if (callExpression.castReturnType())
        checkErrorAndVisit(callExpression.castReturnType()->get());
}

void Visitor::visit(AST::CommaExpression& commaExpression)
{
    for (auto& expression : commaExpression.list())
        checkErrorAndVisit(expression);
}

void Visitor::visit(AST::DereferenceExpression& dereferenceExpression)
{
    checkErrorAndVisit(dereferenceExpression.pointer());
}

void Visitor::visit(AST::LogicalExpression& logicalExpression)
{
    checkErrorAndVisit(logicalExpression.left());
    checkErrorAndVisit(logicalExpression.right());
}

void Visitor::visit(AST::LogicalNotExpression& logicalNotExpression)
{
    checkErrorAndVisit(logicalNotExpression.operand());
}

void Visitor::visit(AST::MakeArrayReferenceExpression& makeArrayReferenceExpression)
{
    checkErrorAndVisit(makeArrayReferenceExpression.lValue());
}

void Visitor::visit(AST::MakePointerExpression& makePointerExpression)
{
    checkErrorAndVisit(makePointerExpression.lValue());
}

void Visitor::visit(AST::ReadModifyWriteExpression& readModifyWriteExpression)
{
    checkErrorAndVisit(readModifyWriteExpression.lValue());
    checkErrorAndVisit(readModifyWriteExpression.oldValue());
    checkErrorAndVisit(readModifyWriteExpression.newValue());
    if (readModifyWriteExpression.newValueExpression())
        checkErrorAndVisit(*readModifyWriteExpression.newValueExpression());
    if (readModifyWriteExpression.resultExpression())
        checkErrorAndVisit(*readModifyWriteExpression.resultExpression());
}

void Visitor::visit(AST::TernaryExpression& ternaryExpression)
{
    checkErrorAndVisit(ternaryExpression.predicate());
    checkErrorAndVisit(ternaryExpression.bodyExpression());
    checkErrorAndVisit(ternaryExpression.elseExpression());
}

void Visitor::visit(AST::VariableReference&)
{
}

} // namespace WHLSL

} // namespace WebCore

#endif // ENABLE(WEBGPU)
