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

#if ENABLE(WHLSL_COMPILER)

#include "WHLSLArrayType.h"
#include "WHLSLArrayReferenceType.h"
#include "WHLSLEnumerationDefinition.h"
#include "WHLSLInferTypes.h"
#include "WHLSLNativeFunctionDeclaration.h"
#include "WHLSLNativeTypeDeclaration.h"
#include "WHLSLPointerType.h"
#include "WHLSLProgram.h"
#include "WHLSLStructureDefinition.h"
#include "WHLSLTypeReference.h"
#include "WHLSLUnnamedTypeHash.h"
#include "WHLSLVariableDeclaration.h"
#include "WHLSLVisitor.h"

namespace WebCore {

namespace WHLSL {

class FindAllTypes : public Visitor {
public:
    virtual ~FindAllTypes() = default;

    void visit(AST::PointerType& pointerType) override
    {
        m_unnamedTypes.add(UnnamedTypeKey { pointerType });
        Visitor::visit(pointerType);
    }

    void visit(AST::ArrayReferenceType& arrayReferenceType) override
    {
        m_unnamedTypes.add(UnnamedTypeKey { arrayReferenceType });
        Visitor::visit(arrayReferenceType);
    }

    void visit(AST::ArrayType& arrayType) override
    {
        m_unnamedTypes.add(UnnamedTypeKey { arrayType });
        Visitor::visit(arrayType);
    }

    void visit(AST::EnumerationDefinition& enumerationDefinition) override
    {
        appendNamedType(enumerationDefinition);
        Visitor::visit(enumerationDefinition);
    }

    void visit(AST::StructureDefinition& structureDefinition) override
    {
        appendNamedType(structureDefinition);
        Visitor::visit(structureDefinition);
    }

    void visit(AST::NativeTypeDeclaration& nativeTypeDeclaration) override
    {
        appendNamedType(nativeTypeDeclaration);
        Visitor::visit(nativeTypeDeclaration);
    }

    HashSet<UnnamedTypeKey> takeUnnamedTypes()
    {
        return WTFMove(m_unnamedTypes);
    }

    Vector<std::reference_wrapper<AST::NamedType>> takeNamedTypes()
    {
        return WTFMove(m_namedTypes);
    }

private:
    void appendNamedType(AST::NamedType& type)
    {
        // The way we walk the AST ensures we should never visit a named type twice.
#if ASSERT_ENABLED
        for (auto& entry : m_namedTypes)
            ASSERT(&entry.get().unifyNode() != &type.unifyNode());
#endif
        m_namedTypes.append(type);
    }

    HashSet<UnnamedTypeKey> m_unnamedTypes;
    Vector<std::reference_wrapper<AST::NamedType>> m_namedTypes;
};

Expected<void, Error> synthesizeConstructors(Program& program)
{
    FindAllTypes findAllTypes;
    findAllTypes.checkErrorAndVisit(program);
    auto unnamedTypes = findAllTypes.takeUnnamedTypes();
    auto namedTypes = findAllTypes.takeNamedTypes();

    bool isOperator = true;

    for (auto& unnamedTypeKey : unnamedTypes) {
        auto& unnamedType = unnamedTypeKey.unnamedType();
        auto location = unnamedType.codeLocation();

        {
            auto variableDeclaration = makeUniqueRef<AST::VariableDeclaration>(location, AST::Qualifiers(), &unnamedType, String(), nullptr, nullptr);
            AST::VariableDeclarations parameters;
            parameters.append(WTFMove(variableDeclaration));
            AST::NativeFunctionDeclaration copyConstructor(AST::FunctionDeclaration(location, AST::AttributeBlock(), WTF::nullopt, unnamedType, "operator cast"_str, WTFMove(parameters), nullptr, isOperator, ParsingMode::StandardLibrary));
            program.append(WTFMove(copyConstructor));
        }

        AST::NativeFunctionDeclaration defaultConstructor(AST::FunctionDeclaration(location, AST::AttributeBlock(), WTF::nullopt, unnamedType, "operator cast"_str, AST::VariableDeclarations(), nullptr, isOperator, ParsingMode::StandardLibrary));
        if (!program.append(WTFMove(defaultConstructor)))
            return makeUnexpected(Error("Could not synthesize default constructor"));
    }

    for (auto& namedType : namedTypes) {
        if (matches(namedType, program.intrinsics().voidType()))
            continue;
        if (is<AST::NativeTypeDeclaration>(static_cast<AST::NamedType&>(namedType)) && downcast<AST::NativeTypeDeclaration>(static_cast<AST::NamedType&>(namedType)).isAtomic())
            continue;

        auto location = namedType.get().codeLocation();

        auto variableDeclaration = makeUniqueRef<AST::VariableDeclaration>(location, AST::Qualifiers(), AST::TypeReference::wrap(location, namedType.get()), String(), nullptr, nullptr);
        AST::VariableDeclarations parameters;
        parameters.append(WTFMove(variableDeclaration));
        AST::NativeFunctionDeclaration copyConstructor(AST::FunctionDeclaration(location, AST::AttributeBlock(), WTF::nullopt, AST::TypeReference::wrap(location, namedType.get()), "operator cast"_str, WTFMove(parameters), nullptr, isOperator, ParsingMode::StandardLibrary));
        program.append(WTFMove(copyConstructor));

        if (is<AST::NativeTypeDeclaration>(static_cast<AST::NamedType&>(namedType))) {
            auto& nativeTypeDeclaration = downcast<AST::NativeTypeDeclaration>(static_cast<AST::NamedType&>(namedType));
            if (nativeTypeDeclaration.isOpaqueType())
                continue;
        }
        AST::NativeFunctionDeclaration defaultConstructor(AST::FunctionDeclaration(location, AST::AttributeBlock(), WTF::nullopt, AST::TypeReference::wrap(location, namedType.get()), "operator cast"_str, AST::VariableDeclarations(), nullptr, isOperator, ParsingMode::StandardLibrary));
        if (!program.append(WTFMove(defaultConstructor)))
            return makeUnexpected(Error("Could not synthesize default constructor"));
    }
    return { };
}

} // namespace WHLSL

} // namespace WebCore

#endif // ENABLE(WHLSL_COMPILER)
