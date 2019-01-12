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
#include "WHLSLNameResolver.h"

#if ENABLE(WEBGPU)

#include "WHLSLCallExpression.h"
#include "WHLSLDoWhileLoop.h"
#include "WHLSLDotExpression.h"
#include "WHLSLEnumerationDefinition.h"
#include "WHLSLEnumerationMemberLiteral.h"
#include "WHLSLForLoop.h"
#include "WHLSLFunctionDefinition.h"
#include "WHLSLIfStatement.h"
#include "WHLSLNameContext.h"
#include "WHLSLProgram.h"
#include "WHLSLPropertyAccessExpression.h"
#include "WHLSLResolveOverloadImpl.h"
#include "WHLSLReturn.h"
#include "WHLSLTypeReference.h"
#include "WHLSLVariableDeclaration.h"
#include "WHLSLVariableReference.h"
#include "WHLSLWhileLoop.h"

namespace WebCore {

namespace WHLSL {

NameResolver::NameResolver(NameContext& nameContext)
    : m_nameContext(nameContext)
{
}

void NameResolver::visit(AST::TypeReference& typeReference)
{
    checkErrorAndVisit(typeReference);
    if (typeReference.resolvedType())
        return;

    auto* candidates = m_nameContext.getTypes(typeReference.name());
    if (candidates == nullptr) {
        setError();
        return;
    }
    if (auto result = resolveTypeOverloadImpl(*candidates, typeReference.typeArguments()))
        typeReference.setResolvedType(*result);
    else {
        setError();
        return;
    }
}

void NameResolver::visit(AST::FunctionDefinition& functionDefinition)
{
    NameContext newNameContext(&m_nameContext);
    NameResolver newNameResolver(newNameContext);
    checkErrorAndVisit(functionDefinition.type());
    for (auto& parameter : functionDefinition.parameters()) {
        newNameResolver.checkErrorAndVisit(parameter);
        auto success = newNameContext.add(parameter);
        if (!success) {
            setError();
            return;
        }
    }
    newNameResolver.checkErrorAndVisit(functionDefinition.block());
}

void NameResolver::visit(AST::Block& block)
{
    NameContext nameContext(&m_nameContext);
    NameResolver(nameContext).checkErrorAndVisit(block);
}

void NameResolver::visit(AST::IfStatement& ifStatement)
{
    checkErrorAndVisit(ifStatement.conditional());
    NameContext nameContext(&m_nameContext);
    NameResolver(nameContext).checkErrorAndVisit(ifStatement.body());
    if (ifStatement.elseBody()) {
        NameContext nameContext(&m_nameContext);
        NameResolver(nameContext).checkErrorAndVisit(static_cast<AST::Statement&>(*ifStatement.elseBody()));
    }
}

void NameResolver::visit(AST::WhileLoop& whileLoop)
{
    checkErrorAndVisit(whileLoop.conditional());
    NameContext nameContext(&m_nameContext);
    NameResolver(nameContext).checkErrorAndVisit(whileLoop.body());
}

void NameResolver::visit(AST::DoWhileLoop& whileLoop)
{
    NameContext nameContext(&m_nameContext);
    NameResolver(nameContext).checkErrorAndVisit(whileLoop.body());
    checkErrorAndVisit(whileLoop.conditional());
}

void NameResolver::visit(AST::ForLoop& forLoop)
{
    NameContext nameContext(&m_nameContext);
    NameResolver(nameContext).checkErrorAndVisit(forLoop);
}

void NameResolver::visit(AST::VariableDeclaration& variableDeclaration)
{
    m_nameContext.add(variableDeclaration);
    checkErrorAndVisit(variableDeclaration);
}

void NameResolver::visit(AST::VariableReference& variableReference)
{
    if (variableReference.variable())
        return;

    if (auto* variable = m_nameContext.getVariable(variableReference.name()))
        variableReference.setVariable(*variable);
    else {
        setError();
        return;
    }
}

void NameResolver::visit(AST::Return& returnStatement)
{
    ASSERT(m_currentFunction);
    returnStatement.setFunction(m_currentFunction);
    checkErrorAndVisit(returnStatement);
}

void NameResolver::visit(AST::PropertyAccessExpression& propertyAccessExpression)
{
    if (auto* getFunctions = m_nameContext.getFunctions(propertyAccessExpression.getFunctionName()))
        propertyAccessExpression.setPossibleGetOverloads(*getFunctions);
    if (auto* setFunctions = m_nameContext.getFunctions(propertyAccessExpression.setFunctionName()))
        propertyAccessExpression.setPossibleSetOverloads(*setFunctions);
    if (auto* andFunctions = m_nameContext.getFunctions(propertyAccessExpression.andFunctionName()))
        propertyAccessExpression.setPossibleAndOverloads(*andFunctions);
    checkErrorAndVisit(propertyAccessExpression);
}

void NameResolver::visit(AST::DotExpression& dotExpression)
{
    if (is<AST::VariableReference>(dotExpression.base())) {
        if (auto enumerationTypes = m_nameContext.getTypes(downcast<AST::VariableReference>(dotExpression.base()).name())) {
            ASSERT(enumerationTypes->size() == 1);
            AST::NamedType& type = (*enumerationTypes)[0];
            if (is<AST::EnumerationDefinition>(type)) {
                AST::EnumerationDefinition& enumerationDefinition = downcast<AST::EnumerationDefinition>(type);
                if (auto* member = enumerationDefinition.memberByName(dotExpression.fieldName())) {
                    static_assert(sizeof(AST::EnumerationMemberLiteral) <= sizeof(AST::DotExpression), "Dot expressions need to be able to become EnumerationMemberLiterals without updating backreferences");
                    Lexer::Token origin = dotExpression.origin();
                    // FIXME: Perhaps do this with variants or a Rewriter instead.
                    dotExpression.~DotExpression();
                    new (&dotExpression) AST::EnumerationMemberLiteral(WTFMove(origin), *member);
                    return;
                }
                setError();
                return;
            }
        }
    }

    checkErrorAndVisit(dotExpression);
}

void NameResolver::visit(AST::CallExpression& callExpression)
{
    if (!callExpression.hasOverloads()) {
        if (auto* functions = m_nameContext.getFunctions(callExpression.name()))
            callExpression.setOverloads(*functions);
        else {
            if (auto* types = m_nameContext.getTypes(callExpression.name())) {
                if (types->size() == 1) {
                    if (auto* functions = m_nameContext.getFunctions("operator cast"_str)) {
                        callExpression.setCastData((*types)[0].get());
                        callExpression.setOverloads(*functions);
                    }
                }
            }
        }
    }
    if (!callExpression.hasOverloads()) {
        setError();
        return;
    }
    checkErrorAndVisit(callExpression);
}

void NameResolver::visit(AST::ConstantExpressionEnumerationMemberReference& constantExpressionEnumerationMemberReference)
{
    if (auto enumerationTypes = m_nameContext.getTypes(constantExpressionEnumerationMemberReference.left())) {
        ASSERT(enumerationTypes->size() == 1);
        AST::NamedType& type = (*enumerationTypes)[0];
        if (is<AST::EnumerationDefinition>(type)) {
            AST::EnumerationDefinition& enumerationDefinition = downcast<AST::EnumerationDefinition>(type);
            if (auto* member = enumerationDefinition.memberByName(constantExpressionEnumerationMemberReference.right())) {
                constantExpressionEnumerationMemberReference.setEnumerationMember(enumerationDefinition, *member);
                return;
            }
        }
    }
    
    setError();
}

// FIXME: Make sure all the names have been resolved.

bool resolveNamesInTypes(Program& program, NameResolver& nameResolver)
{
    for (auto& typeDefinition : program.typeDefinitions()) {
        nameResolver.checkErrorAndVisit(static_cast<AST::TypeDefinition&>(typeDefinition));
        if (nameResolver.error())
            return false;
    }
    for (auto& structureDefinition : program.structureDefinitions()) {
        nameResolver.checkErrorAndVisit(static_cast<AST::StructureDefinition&>(structureDefinition));
        if (nameResolver.error())
            return false;
    }
    for (auto& enumerationDefinition : program.enumerationDefinitions()) {
        nameResolver.checkErrorAndVisit(static_cast<AST::EnumerationDefinition&>(enumerationDefinition));
        if (nameResolver.error())
            return false;
    }
    for (auto& nativeTypeDeclaration : program.nativeTypeDeclarations()) {
        nameResolver.checkErrorAndVisit(static_cast<AST::NativeTypeDeclaration&>(nativeTypeDeclaration));
        if (nameResolver.error())
            return false;
    }
    return true;
}

bool resolveNamesInFunctions(Program& program, NameResolver& nameResolver)
{
    for (auto& functionDefinition : program.functionDefinitions()) {
        nameResolver.setCurrentFunctionDefinition(&static_cast<AST::FunctionDefinition&>(functionDefinition));
        nameResolver.checkErrorAndVisit(static_cast<AST::FunctionDefinition&>(functionDefinition));
        if (nameResolver.error())
            return false;
    }
    nameResolver.setCurrentFunctionDefinition(nullptr);
    for (auto& nativeFunctionDeclaration : program.nativeFunctionDeclarations()) {
        nameResolver.checkErrorAndVisit(static_cast<AST::FunctionDeclaration&>(nativeFunctionDeclaration));
        if (nameResolver.error())
            return false;
    }
    return true;
}

}

}

#endif
