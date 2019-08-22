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

#include "WHLSLDoWhileLoop.h"
#include "WHLSLDotExpression.h"
#include "WHLSLEnumerationDefinition.h"
#include "WHLSLEnumerationMemberLiteral.h"
#include "WHLSLForLoop.h"
#include "WHLSLFunctionDefinition.h"
#include "WHLSLIfStatement.h"
#include "WHLSLNameContext.h"
#include "WHLSLProgram.h"
#include "WHLSLReplaceWith.h"
#include "WHLSLResolveOverloadImpl.h"
#include "WHLSLScopedSetAdder.h"
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

NameResolver::NameResolver(NameResolver& parentResolver, NameContext& nameContext)
    : m_nameContext(nameContext)
    , m_parentNameResolver(&parentResolver)
    , m_currentNameSpace(parentResolver.m_currentNameSpace)
{
}

NameResolver::~NameResolver()
{
    if (hasError() && m_parentNameResolver)
        m_parentNameResolver->setError(result().error());
}

void NameResolver::visit(AST::TypeReference& typeReference)
{
    ScopedSetAdder<AST::TypeReference*> adder(m_typeReferences, &typeReference);
    if (!adder.isNewEntry()) {
        setError(Error("Cannot use recursive type arguments.", typeReference.codeLocation()));
        return;
    }

    Visitor::visit(typeReference);
    if (hasError())
        return;
    if (typeReference.maybeResolvedType()) // FIXME: https://bugs.webkit.org/show_bug.cgi?id=198161 Shouldn't we know by now whether the type has been resolved or not?
        return;

    auto candidates = m_nameContext.getTypes(typeReference.name(), m_currentNameSpace);
    for (auto& candidate : candidates)
        Visitor::visit(candidate);
    if (auto result = resolveTypeOverloadImpl(candidates, typeReference.typeArguments()))
        typeReference.setResolvedType(*result);
    else {
        setError(Error("Cannot resolve type arguments.", typeReference.codeLocation()));
        return;
    }
}

void NameResolver::visit(AST::FunctionDefinition& functionDefinition)
{
    NameContext newNameContext(&m_nameContext);
    NameResolver newNameResolver(*this, newNameContext);
    checkErrorAndVisit(functionDefinition.type());
    if (hasError())
        return;
    for (auto& parameter : functionDefinition.parameters())
        newNameResolver.checkErrorAndVisit(parameter);
    newNameResolver.checkErrorAndVisit(functionDefinition.block());
}

void NameResolver::visit(AST::Block& block)
{
    NameContext nameContext(&m_nameContext);
    NameResolver newNameResolver(*this, nameContext);
    newNameResolver.Visitor::visit(block);
}

void NameResolver::visit(AST::IfStatement& ifStatement)
{
    checkErrorAndVisit(ifStatement.conditional());
    if (hasError())
        return;

    {
        NameContext nameContext(&m_nameContext);
        NameResolver newNameResolver(*this, nameContext);
        newNameResolver.checkErrorAndVisit(ifStatement.body());
    }
    if (hasError())
        return;

    if (ifStatement.elseBody()) {
        NameContext nameContext(&m_nameContext);
        NameResolver newNameResolver(*this, nameContext);
        newNameResolver.checkErrorAndVisit(*ifStatement.elseBody());
    }
}

void NameResolver::visit(AST::WhileLoop& whileLoop)
{
    checkErrorAndVisit(whileLoop.conditional());
    if (hasError())
        return;

    NameContext nameContext(&m_nameContext);
    NameResolver newNameResolver(*this, nameContext);
    newNameResolver.checkErrorAndVisit(whileLoop.body());
}

void NameResolver::visit(AST::DoWhileLoop& whileLoop)
{
    {
        NameContext nameContext(&m_nameContext);
        NameResolver newNameResolver(*this, nameContext);
        newNameResolver.checkErrorAndVisit(whileLoop.body());
    }

    checkErrorAndVisit(whileLoop.conditional());
}

void NameResolver::visit(AST::ForLoop& forLoop)
{
    NameContext nameContext(&m_nameContext);
    NameResolver newNameResolver(*this, nameContext);
    newNameResolver.Visitor::visit(forLoop);
}

void NameResolver::visit(AST::VariableDeclaration& variableDeclaration)
{
    if (!m_nameContext.add(variableDeclaration)) {
        setError(Error("Cannot declare duplicate variables.", variableDeclaration.codeLocation()));
        return;
    }
    Visitor::visit(variableDeclaration);
}

void NameResolver::visit(AST::VariableReference& variableReference)
{
    if (variableReference.variable())
        return;

    if (auto* variable = m_nameContext.getVariable(variableReference.name()))
        variableReference.setVariable(*variable);
    else {
        setError(Error("Cannot find the variable declaration.", variableReference.codeLocation()));
        return;
    }
}

void NameResolver::visit(AST::DotExpression& dotExpression)
{
    if (is<AST::VariableReference>(dotExpression.base())) {
        auto& variableReference = downcast<AST::VariableReference>(dotExpression.base());
        if (!m_nameContext.getVariable(variableReference.name())) {
            auto baseName = variableReference.name();
            auto enumerationTypes = m_nameContext.getTypes(baseName, m_currentNameSpace);
            if (enumerationTypes.size() == 1) {
                AST::NamedType& type = enumerationTypes[0];
                if (is<AST::EnumerationDefinition>(type)) {
                    AST::EnumerationDefinition& enumerationDefinition = downcast<AST::EnumerationDefinition>(type);
                    auto memberName = dotExpression.fieldName();
                    if (auto* member = enumerationDefinition.memberByName(memberName)) {
                        auto enumerationMemberLiteral = AST::EnumerationMemberLiteral::wrap(dotExpression.codeLocation(), WTFMove(baseName), WTFMove(memberName), enumerationDefinition, *member);
                        AST::replaceWith<AST::EnumerationMemberLiteral>(dotExpression, WTFMove(enumerationMemberLiteral));
                        return;
                    }
                    setError(Error("No enum member matches the used name.", dotExpression.codeLocation()));
                    return;
                }
            } else
                ASSERT(enumerationTypes.isEmpty());
        }
    }

    Visitor::visit(dotExpression);
}

void NameResolver::visit(AST::EnumerationMemberLiteral& enumerationMemberLiteral)
{
    if (enumerationMemberLiteral.enumerationMember())
        return;

    auto enumerationTypes = m_nameContext.getTypes(enumerationMemberLiteral.left(), m_currentNameSpace);
    if (enumerationTypes.size() == 1) {
        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=199335 This needs to work with typedef'ed enums.
        AST::NamedType& type = enumerationTypes[0];
        if (is<AST::EnumerationDefinition>(type)) {
            AST::EnumerationDefinition& enumerationDefinition = downcast<AST::EnumerationDefinition>(type);
            if (auto* member = enumerationDefinition.memberByName(enumerationMemberLiteral.right())) {
                enumerationMemberLiteral.setEnumerationMember(enumerationDefinition, *member);
                return;
            }
        }
    }

    setError(Error("Cannot resolve enumeration member literal.", enumerationMemberLiteral.codeLocation()));
}

void NameResolver::visit(AST::NativeFunctionDeclaration& nativeFunctionDeclaration)
{
    NameContext newNameContext(&m_nameContext);
    NameResolver newNameResolver(newNameContext);
    newNameResolver.Visitor::visit(nativeFunctionDeclaration);
}

// FIXME: https://bugs.webkit.org/show_bug.cgi?id=198167 Make sure all the names have been resolved.

Expected<void, Error> resolveNamesInTypes(Program& program, NameResolver& nameResolver)
{
    for (auto& typeDefinition : program.typeDefinitions()) {
        nameResolver.setCurrentNameSpace(typeDefinition.get().nameSpace());
        nameResolver.checkErrorAndVisit(typeDefinition);
        if (nameResolver.hasError())
            return nameResolver.result();
    }
    for (auto& structureDefinition : program.structureDefinitions()) {
        nameResolver.setCurrentNameSpace(structureDefinition.get().nameSpace());
        nameResolver.checkErrorAndVisit(structureDefinition);
        if (nameResolver.hasError())
            return nameResolver.result();
    }
    for (auto& enumerationDefinition : program.enumerationDefinitions()) {
        nameResolver.setCurrentNameSpace(enumerationDefinition.get().nameSpace());
        nameResolver.checkErrorAndVisit(enumerationDefinition);
        if (nameResolver.hasError())
            return nameResolver.result();
    }
    for (auto& nativeTypeDeclaration : program.nativeTypeDeclarations()) {
        nameResolver.setCurrentNameSpace(nativeTypeDeclaration.get().nameSpace());
        nameResolver.checkErrorAndVisit(nativeTypeDeclaration);
        if (nameResolver.hasError())
            return nameResolver.result();
    }
    return { };
}

Expected<void, Error> resolveTypeNamesInFunctions(Program& program, NameResolver& nameResolver)
{
    for (auto& functionDefinition : program.functionDefinitions()) {
        nameResolver.setCurrentNameSpace(functionDefinition.get().nameSpace());
        nameResolver.checkErrorAndVisit(functionDefinition);
        if (nameResolver.hasError())
            return nameResolver.result();
    }
    for (auto& nativeFunctionDeclaration : program.nativeFunctionDeclarations()) {
        nameResolver.setCurrentNameSpace(nativeFunctionDeclaration.get().nameSpace());
        nameResolver.checkErrorAndVisit(nativeFunctionDeclaration);
        if (nameResolver.hasError())
            return nameResolver.result();
    }
    return { };
}

} // namespace WHLSL

} // namespace WebCore

#endif // ENABLE(WEBGPU)
