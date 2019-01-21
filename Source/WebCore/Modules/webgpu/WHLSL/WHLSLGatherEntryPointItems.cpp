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
#include "WHLSLGatherEntryPointItems.h"

#if ENABLE(WEBGPU)

#include "WHLSLArrayReferenceType.h"
#include "WHLSLArrayType.h"
#include "WHLSLFunctionDefinition.h"
#include "WHLSLInferTypes.h"
#include "WHLSLIntrinsics.h"
#include "WHLSLPointerType.h"
#include "WHLSLStructureDefinition.h"
#include "WHLSLTypeDefinition.h"
#include "WHLSLTypeReference.h"
#include "WHLSLVariableDeclaration.h"
#include "WHLSLVisitor.h"
#include <wtf/Optional.h>

namespace WebCore {

namespace WHLSL {

class Gatherer : public Visitor {
public:
    Gatherer(const Intrinsics& intrinsics, AST::Semantic* semantic = nullptr)
        : m_intrinsics(intrinsics)
        , m_currentSemantic(semantic)
    {
    }

    virtual ~Gatherer() = default;

    void reset()
    {
        m_currentSemantic = nullptr;
    }

    Vector<EntryPointItem>&& takeEntryPointItems()
    {
        return WTFMove(m_entryPointItems);
    }

    void visit(AST::EnumerationDefinition&)
    {
        if (!m_currentSemantic) {
            setError();
            return;
        }
        m_entryPointItems.append(EntryPointItem(m_typeReferences.last().get(), *m_currentSemantic, m_path));
    }

    void visit(AST::NativeTypeDeclaration& nativeTypeDeclaration)
    {
        if (!m_currentSemantic) {
            setError();
            return;
        }
        if (matches(nativeTypeDeclaration, m_intrinsics.voidType())) {
            setError();
            return;
        }

        m_entryPointItems.append(EntryPointItem(m_typeReferences.last().get(), *m_currentSemantic, m_path));
    }

    void visit(AST::StructureDefinition& structureDefinition)
    {
        if (m_currentSemantic) {
            setError();
            return;
        }

        for (auto& structureElement : structureDefinition.structureElements()) {
            if (structureElement.semantic())
                m_currentSemantic = &*structureElement.semantic();
            m_path.append(structureElement.name());
            checkErrorAndVisit(structureElement);
            m_path.takeLast();
        }
    }

    void visit(AST::TypeDefinition& typeDefinition)
    {
        checkErrorAndVisit(typeDefinition.type());
    }

    void visit(AST::TypeReference& typeReference)
    {
        ASSERT(typeReference.resolvedType());
        m_typeReferences.append(typeReference);
        auto depth = m_typeReferences.size();
        checkErrorAndVisit(*typeReference.resolvedType());
        ASSERT_UNUSED(depth, m_typeReferences.size() == depth);
    }

    void visit(AST::PointerType& pointerType)
    {
        if (!m_currentSemantic) {
            setError();
            return;
        }
        m_entryPointItems.append(EntryPointItem(pointerType, *m_currentSemantic, m_path));
    }

    void visit(AST::ArrayReferenceType& arrayReferenceType)
    {
        if (!m_currentSemantic) {
            setError();
            return;
        }
        m_entryPointItems.append(EntryPointItem(arrayReferenceType, *m_currentSemantic, m_path));
    }

    void visit(AST::ArrayType& arrayType)
    {
        if (!m_currentSemantic) {
            setError();
            return;
        }
        m_entryPointItems.append(EntryPointItem(arrayType, *m_currentSemantic, m_path));
    }

    void visit(AST::VariableDeclaration& variableDeclaration)
    {
        ASSERT(!m_currentSemantic);
        if (variableDeclaration.semantic())
            m_currentSemantic = &*variableDeclaration.semantic();
        ASSERT(variableDeclaration.type());
        m_path.append(variableDeclaration.name());
        checkErrorAndVisit(*variableDeclaration.type());
        m_path.takeLast();
    }

private:
    Vector<String> m_path;
    const Intrinsics& m_intrinsics;
    AST::Semantic* m_currentSemantic { nullptr };
    Vector<std::reference_wrapper<AST::TypeReference>> m_typeReferences;
    Vector<EntryPointItem> m_entryPointItems;
};

Optional<EntryPointItems> gatherEntryPointItems(const Intrinsics& intrinsics, AST::FunctionDefinition& functionDefinition)
{
    ASSERT(functionDefinition.entryPointType());
    Gatherer inputGatherer(intrinsics);
    for (auto& parameter : functionDefinition.parameters()) {
        inputGatherer.reset();
        inputGatherer.checkErrorAndVisit(parameter);
        if (inputGatherer.error())
            return WTF::nullopt;
    }
    Gatherer outputGatherer(intrinsics, functionDefinition.semantic() ? &*functionDefinition.semantic() : nullptr);
    outputGatherer.checkErrorAndVisit(functionDefinition.type());
    if (outputGatherer.error())
        return WTF::nullopt;

    return {{ inputGatherer.takeEntryPointItems(), outputGatherer.takeEntryPointItems() }};
}

} // namespace WHLSL

} // namespace WebCore

#endif // ENABLE(WEBGPU)
