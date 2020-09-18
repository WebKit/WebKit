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
#include "WHLSLTypeNamer.h"

#if ENABLE(WHLSL_COMPILER)

#include "WHLSLAddressSpace.h"
#include "WHLSLArrayReferenceType.h"
#include "WHLSLArrayType.h"
#include "WHLSLCallExpression.h"
#include "WHLSLEnumerationDefinition.h"
#include "WHLSLEnumerationMember.h"
#include "WHLSLNativeTypeDeclaration.h"
#include "WHLSLNativeTypeWriter.h"
#include "WHLSLPointerType.h"
#include "WHLSLStructureDefinition.h"
#include "WHLSLTypeDefinition.h"
#include "WHLSLTypeReference.h"
#include "WHLSLVisitor.h"
#include <algorithm>
#include <functional>
#include <wtf/FastMalloc.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/Optional.h>
#include <wtf/UniqueRef.h>
#include <wtf/Vector.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringConcatenateNumbers.h>

namespace WebCore {

namespace WHLSL {

namespace Metal {

TypeNamer::TypeNamer(Program& program)
    : m_program(program)
{
}

TypeNamer::~TypeNamer() = default;

void TypeNamer::visit(AST::UnnamedType& unnamedType)
{
    generateUniquedTypeName(unnamedType);
}

void TypeNamer::visit(AST::EnumerationDefinition& enumerationDefinition)
{
    {
        auto addResult = m_namedTypeMapping.add(&enumerationDefinition, generateNextTypeName());
        ASSERT_UNUSED(addResult, addResult.isNewEntry);
    }

    for (auto& enumerationMember : enumerationDefinition.enumerationMembers()) {
        auto addResult = m_enumerationMemberMapping.add(&static_cast<AST::EnumerationMember&>(enumerationMember), generateNextEnumerationMemberName());
        ASSERT_UNUSED(addResult, addResult.isNewEntry);
    }

    Visitor::visit(enumerationDefinition);

    {
        auto addResult = m_dependencyGraph.add(&enumerationDefinition, Vector<std::reference_wrapper<AST::UnnamedType>> { enumerationDefinition.type() });
        ASSERT_UNUSED(addResult, addResult.isNewEntry);
    }
}

void TypeNamer::visit(AST::NativeTypeDeclaration& nativeTypeDeclaration)
{
    // Native type declarations already have names, and are already declared in Metal.
    auto addResult = m_dependencyGraph.add(&nativeTypeDeclaration, Vector<std::reference_wrapper<AST::UnnamedType>> { });
    ASSERT_UNUSED(addResult, addResult.isNewEntry);
}

void TypeNamer::visit(AST::StructureDefinition& structureDefinition)
{
    {
        auto addResult = m_namedTypeMapping.add(&structureDefinition, generateNextTypeName());
        ASSERT_UNUSED(addResult, addResult.isNewEntry);
    }
    Visitor::visit(structureDefinition);
    {
        Vector<std::reference_wrapper<AST::UnnamedType>> neighbors;
        for (auto& structureElement : structureDefinition.structureElements()) {
            auto addResult = m_structureElementMapping.add(&structureElement, generateNextStructureElementName());
            ASSERT_UNUSED(addResult, addResult.isNewEntry);
            neighbors.append(structureElement.type());
        }
        auto addResult = m_dependencyGraph.add(&structureDefinition, WTFMove(neighbors));
        ASSERT_UNUSED(addResult, addResult.isNewEntry);
    }
}

void TypeNamer::visit(AST::TypeDefinition& typeDefinition)
{
    {
        auto addResult = m_namedTypeMapping.add(&typeDefinition, generateNextTypeName());
        ASSERT_UNUSED(addResult, addResult.isNewEntry);
    }
    Visitor::visit(typeDefinition);
    {
        auto addResult = m_dependencyGraph.add(&typeDefinition, Vector<std::reference_wrapper<AST::UnnamedType>> { typeDefinition.type() });
        ASSERT_UNUSED(addResult, addResult.isNewEntry);
    }
}

void TypeNamer::visit(AST::Expression& expression)
{
    generateUniquedTypeName(expression.resolvedType());
    Visitor::visit(expression);
}

void TypeNamer::visit(AST::CallExpression& callExpression)
{
    for (auto& argument : callExpression.arguments())
        checkErrorAndVisit(argument);
}

String TypeNamer::mangledNameForType(AST::NativeTypeDeclaration& nativeTypeDeclaration)
{
    return writeNativeType(nativeTypeDeclaration);
}

static AST::UnnamedType* parent(AST::UnnamedType& unnamedType)
{
    switch (unnamedType.kind()) {
    case AST::UnnamedType::Kind::TypeReference:
        return nullptr;
    case AST::UnnamedType::Kind::Pointer:
        return &downcast<AST::PointerType>(unnamedType).elementType();
    case AST::UnnamedType::Kind::ArrayReference:
        return &downcast<AST::ArrayReferenceType>(unnamedType).elementType();
    case AST::UnnamedType::Kind::Array:
        return &downcast<AST::ArrayType>(unnamedType).type();
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

void TypeNamer::generateUniquedTypeName(AST::UnnamedType& unnamedType)
{
    auto* parentUnnamedType = parent(unnamedType);
    if (parentUnnamedType)
        generateUniquedTypeName(*parentUnnamedType);

    m_unnamedTypeMapping.ensure(UnnamedTypeKey { unnamedType }, [&] {
        return generateNextTypeName();
    });
}

class MetalTypeDeclarationWriter final : public Visitor {
    WTF_MAKE_FAST_ALLOCATED;
public:
    MetalTypeDeclarationWriter(StringBuilder& stringBuilder, std::function<MangledOrNativeTypeName(AST::NamedType&)>&& mangledNameForNamedType)
        : m_mangledNameForNamedType(WTFMove(mangledNameForNamedType))
        , m_stringBuilder(stringBuilder)
    {
    }

private:
    void visit(AST::StructureDefinition& structureDefinition) override
    {
        m_stringBuilder.append("struct ", m_mangledNameForNamedType(structureDefinition), ";\n");
    }

    std::function<MangledOrNativeTypeName(AST::NamedType&)> m_mangledNameForNamedType;
    StringBuilder& m_stringBuilder;
};

void TypeNamer::emitMetalTypeDeclarations(StringBuilder& stringBuilder)
{
    MetalTypeDeclarationWriter metalTypeDeclarationWriter(stringBuilder, [&](AST::NamedType& namedType) -> MangledOrNativeTypeName {
        return mangledNameForType(namedType);
    });
    metalTypeDeclarationWriter.Visitor::visit(m_program);
}

void TypeNamer::emitUnnamedTypeDefinition(StringBuilder& stringBuilder, AST::UnnamedType& unnamedType, MangledTypeName mangledName, HashSet<AST::NamedType*>& emittedNamedTypes, HashSet<UnnamedTypeKey>& emittedUnnamedTypes)
{
    if (emittedUnnamedTypes.contains(UnnamedTypeKey { unnamedType }))
        return;

    switch (unnamedType.kind()) {
    case AST::UnnamedType::Kind::TypeReference: {
        auto& typeReference = downcast<AST::TypeReference>(unnamedType);

        auto& parent = typeReference.resolvedType();
        auto parentMangledName = mangledNameForType(typeReference.resolvedType());
        auto iterator = m_dependencyGraph.find(&parent);
        ASSERT(iterator != m_dependencyGraph.end());
        emitNamedTypeDefinition(stringBuilder, parent, iterator->value, emittedNamedTypes, emittedUnnamedTypes);

        stringBuilder.append("typedef ", parentMangledName, ' ', mangledName, ";\n");
        break;
    }
    case AST::UnnamedType::Kind::Pointer: {
        auto& pointerType = downcast<AST::PointerType>(unnamedType);

        auto& parent = pointerType.elementType();
        auto parentMangledName = mangledNameForType(parent);
        emitUnnamedTypeDefinition(stringBuilder, parent, parentMangledName, emittedNamedTypes, emittedUnnamedTypes);

        stringBuilder.append("typedef ", toString(pointerType.addressSpace()), ' ', parentMangledName, "* ", mangledName, ";\n");
        break;
    }
    case AST::UnnamedType::Kind::ArrayReference: {
        auto& arrayReferenceType = downcast<AST::ArrayReferenceType>(unnamedType);

        auto& parent = arrayReferenceType.elementType();
        auto parentMangledName = mangledNameForType(parent);
        emitUnnamedTypeDefinition(stringBuilder, parent, parentMangledName, emittedNamedTypes, emittedUnnamedTypes);

        stringBuilder.append(
            "struct ", mangledName, " {\n"
            "    ", toString(arrayReferenceType.addressSpace()), ' ', parentMangledName, "* pointer;\n"
            "    uint32_t length;\n"
            "};\n"
        );
        break;
    }
    case AST::UnnamedType::Kind::Array: {
        auto& arrayType = downcast<AST::ArrayType>(unnamedType);

        auto& parent = arrayType.type();
        auto parentMangledName = mangledNameForType(parent);
        emitUnnamedTypeDefinition(stringBuilder, parent, parentMangledName, emittedNamedTypes, emittedUnnamedTypes);

        stringBuilder.append("typedef array<", parentMangledName, ", ", arrayType.numElements(), "> ", mangledName, ";\n");
        break;
    }
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }

    emittedUnnamedTypes.add(UnnamedTypeKey { unnamedType });
}

void TypeNamer::emitNamedTypeDefinition(StringBuilder& stringBuilder, AST::NamedType& namedType, Vector<std::reference_wrapper<AST::UnnamedType>>& neighbors, HashSet<AST::NamedType*>& emittedNamedTypes, HashSet<UnnamedTypeKey>& emittedUnnamedTypes)
{
    if (emittedNamedTypes.contains(&namedType))
        return;

    for (auto& unnameType : neighbors)
        emitUnnamedTypeDefinition(stringBuilder, unnameType, mangledNameForType(unnameType), emittedNamedTypes, emittedUnnamedTypes);

    switch (namedType.kind()) {
    case AST::NamedType::Kind::EnumerationDefinition: {
        auto& enumerationDefinition = downcast<AST::EnumerationDefinition>(namedType);
        auto& baseType = enumerationDefinition.type().unifyNode();

        stringBuilder.append("enum class ", mangledNameForType(enumerationDefinition), " : ", mangledNameForType(downcast<AST::NamedType>(baseType)), " {\n");
        for (auto& enumerationMember : enumerationDefinition.enumerationMembers())
            stringBuilder.append("    ", mangledNameForEnumerationMember(enumerationMember), " = ", enumerationMember.get().value(), ",\n");
        stringBuilder.append("};\n");
        break;
    }
    case AST::NamedType::Kind::NativeTypeDeclaration: {
        // Native types already have definitions. There's nothing to do.
        break;
    }
    case AST::NamedType::Kind::StructureDefinition: {
        auto& structureDefinition = downcast<AST::StructureDefinition>(namedType);

        stringBuilder.append("struct ", mangledNameForType(structureDefinition), " {\n");
        for (auto& structureElement : structureDefinition.structureElements())
            stringBuilder.append("    ", mangledNameForType(structureElement.type()), ' ', mangledNameForStructureElement(structureElement), ";\n");
        stringBuilder.append("};\n");
        break;
    }
    case AST::NamedType::Kind::TypeDefinition: {
        auto& typeDefinition = downcast<AST::TypeDefinition>(namedType);

        stringBuilder.append("typedef ", mangledNameForType(typeDefinition.type()), ' ', mangledNameForType(typeDefinition), ";\n");
        break;
    }
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }

    emittedNamedTypes.add(&namedType);
}

void TypeNamer::emitMetalTypeDefinitions(StringBuilder& stringBuilder)
{
    HashSet<AST::NamedType*> emittedNamedTypes;
    HashSet<UnnamedTypeKey> emittedUnnamedTypes;
    for (auto& [namedType, neighbors] : m_dependencyGraph)
        emitNamedTypeDefinition(stringBuilder, *namedType, neighbors, emittedNamedTypes, emittedUnnamedTypes);
    for (auto& [unnamedTypeKey, mangledName] : m_unnamedTypeMapping)
        emitUnnamedTypeDefinition(stringBuilder, unnamedTypeKey.unnamedType(), mangledName, emittedNamedTypes, emittedUnnamedTypes);
}

MangledTypeName TypeNamer::mangledNameForType(AST::UnnamedType& unnamedType)
{
    auto iterator = m_unnamedTypeMapping.find(UnnamedTypeKey { unnamedType });
    ASSERT(iterator != m_unnamedTypeMapping.end());
    return iterator->value;
}

MangledOrNativeTypeName TypeNamer::mangledNameForType(AST::NamedType& namedType)
{
    if (is<AST::NativeTypeDeclaration>(namedType))
        return mangledNameForType(downcast<AST::NativeTypeDeclaration>(namedType));
    auto iterator = m_namedTypeMapping.find(&namedType);
    ASSERT(iterator != m_namedTypeMapping.end());
    return iterator->value;
}

MangledEnumerationMemberName TypeNamer::mangledNameForEnumerationMember(AST::EnumerationMember& enumerationMember)
{
    auto iterator = m_enumerationMemberMapping.find(&enumerationMember);
    ASSERT(iterator != m_enumerationMemberMapping.end());
    return iterator->value;
}

MangledStructureElementName TypeNamer::mangledNameForStructureElement(AST::StructureElement& structureElement)
{
    auto iterator = m_structureElementMapping.find(&structureElement);
    ASSERT(iterator != m_structureElementMapping.end());
    return iterator->value;
}

void TypeNamer::emitMetalTypes(StringBuilder& stringBuilder)
{
    Visitor::visit(m_program);

    emitMetalTypeDeclarations(stringBuilder);
    stringBuilder.append('\n');
    emitMetalTypeDefinitions(stringBuilder);
}

} // namespace Metal

} // namespace WHLSL

} // namespace WebCore

#endif // ENABLE(WHLSL_COMPILER)
