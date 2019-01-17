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
#include "WHLSLSynthesizeConstructors.h"

#if ENABLE(WEBGPU)

#include "WHLSLNativeFunctionDeclaration.h"
#include "WHLSLProgram.h"
#include "WHLSLTypeReference.h"
#include "WHLSLVariableDeclaration.h"
#include "WHLSLVisitor.h"

namespace WebCore {

namespace WHLSL {

class FindAllTypes : public Visitor {
public:
    ~FindAllTypes() = default;

    void visit(AST::PointerType& pointerType) override
    {
        m_unnamedTypes.append(pointerType);
        checkErrorAndVisit(pointerType);
    }

    void visit(AST::ArrayReferenceType& arrayReferenceType) override
    {
        m_unnamedTypes.append(arrayReferenceType);
        checkErrorAndVisit(arrayReferenceType);
    }

    void visit(AST::ArrayType& arrayType) override
    {
        m_unnamedTypes.append(arrayType);
        checkErrorAndVisit(arrayType);
    }

    void visit(AST::EnumerationDefinition& enumerationDefinition) override
    {
        m_namedTypes.append(enumerationDefinition);
        checkErrorAndVisit(enumerationDefinition);
    }

    void visit(AST::StructureDefinition& structureDefinition) override
    {
        m_namedTypes.append(structureDefinition);
        checkErrorAndVisit(structureDefinition);
    }

    void visit(AST::NativeTypeDeclaration& nativeTypeDeclaration) override
    {
        m_namedTypes.append(nativeTypeDeclaration);
        checkErrorAndVisit(nativeTypeDeclaration);
    }

    Vector<std::reference_wrapper<AST::UnnamedType>>&& takeUnnamedTypes()
    {
        return WTFMove(m_unnamedTypes);
    }

    Vector<std::reference_wrapper<AST::NamedType>>&& takeNamedTypes()
    {
        return WTFMove(m_namedTypes);
    }

private:
    Vector<std::reference_wrapper<AST::UnnamedType>> m_unnamedTypes;
    Vector<std::reference_wrapper<AST::NamedType>> m_namedTypes;
};

void synthesizeConstructors(Program& program)
{
    FindAllTypes findAllTypes;
    findAllTypes.checkErrorAndVisit(program);
    auto m_unnamedTypes = findAllTypes.takeUnnamedTypes();
    auto m_namedTypes = findAllTypes.takeNamedTypes();

    bool isOperator = true;

    for (auto& unnamedType : m_unnamedTypes) {
        AST::VariableDeclaration variableDeclaration(Lexer::Token(unnamedType.get().origin()), AST::Qualifiers(), { unnamedType.get().clone() }, String(), WTF::nullopt, WTF::nullopt);
        AST::VariableDeclarations parameters;
        parameters.append(WTFMove(variableDeclaration));
        AST::NativeFunctionDeclaration copyConstructor(AST::FunctionDeclaration(Lexer::Token(unnamedType.get().origin()), AST::AttributeBlock(), WTF::nullopt, unnamedType.get().clone(), "operator cast"_str, WTFMove(parameters), WTF::nullopt, isOperator));
        program.append(WTFMove(copyConstructor));

        AST::NativeFunctionDeclaration defaultConstructor(AST::FunctionDeclaration(Lexer::Token(unnamedType.get().origin()), AST::AttributeBlock(), WTF::nullopt, unnamedType.get().clone(), "operator cast"_str, AST::VariableDeclarations(), WTF::nullopt, isOperator));
        program.append(WTFMove(defaultConstructor));
    }

    for (auto& namedType : m_namedTypes) {
        AST::VariableDeclaration variableDeclaration(Lexer::Token(namedType.get().origin()), AST::Qualifiers(), { AST::TypeReference::wrap(Lexer::Token(namedType.get().origin()), namedType.get()) }, String(), WTF::nullopt, WTF::nullopt);
        AST::VariableDeclarations parameters;
        parameters.append(WTFMove(variableDeclaration));
        AST::NativeFunctionDeclaration copyConstructor(AST::FunctionDeclaration(Lexer::Token(namedType.get().origin()), AST::AttributeBlock(), WTF::nullopt, AST::TypeReference::wrap(Lexer::Token(namedType.get().origin()), namedType.get()), "operator cast"_str, WTFMove(parameters), WTF::nullopt, isOperator));
        program.append(WTFMove(copyConstructor));

        AST::NativeFunctionDeclaration defaultConstructor(AST::FunctionDeclaration(Lexer::Token(namedType.get().origin()), AST::AttributeBlock(), WTF::nullopt, AST::TypeReference::wrap(Lexer::Token(namedType.get().origin()), namedType.get()), "operator cast"_str, AST::VariableDeclarations(), WTF::nullopt, isOperator));
        program.append(WTFMove(defaultConstructor));
    }
}

} // namespace WHLSL

} // namespace WebCore

#endif // ENABLE(WEBGPU)
