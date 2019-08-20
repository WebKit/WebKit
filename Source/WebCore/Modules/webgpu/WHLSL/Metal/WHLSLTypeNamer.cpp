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

#if ENABLE(WEBGPU)

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

// FIXME: Look into replacing BaseTypeNameNode with a simple struct { RefPtr<UnnamedType> parent; MangledTypeName; } that UnnamedTypeKeys map to.
class BaseTypeNameNode {
    WTF_MAKE_FAST_ALLOCATED;
public:
    BaseTypeNameNode(BaseTypeNameNode* parent, MangledTypeName&& mangledName, AST::UnnamedType::Kind kind)
        : m_parent(parent)
        , m_mangledName(mangledName)
        , m_kind(kind)
    {
    }
    virtual ~BaseTypeNameNode() = default;
    
    AST::UnnamedType::Kind kind() { return m_kind; }
    bool isReferenceTypeNameNode() const { return m_kind == AST::UnnamedType::Kind::TypeReference; }
    bool isPointerTypeNameNode() const { return m_kind == AST::UnnamedType::Kind::Pointer; }
    bool isArrayReferenceTypeNameNode() const { return m_kind == AST::UnnamedType::Kind::ArrayReference; }
    bool isArrayTypeNameNode() const { return m_kind == AST::UnnamedType::Kind::Array; }

    BaseTypeNameNode* parent() { return m_parent; }
    MangledTypeName mangledName() const { return m_mangledName; }

private:
    BaseTypeNameNode* m_parent;
    MangledTypeName m_mangledName;
    AST::UnnamedType::Kind m_kind;
};

class ArrayTypeNameNode final : public BaseTypeNameNode {
    WTF_MAKE_FAST_ALLOCATED;
public:
    ArrayTypeNameNode(BaseTypeNameNode* parent, MangledTypeName&& mangledName, unsigned numElements)
        : BaseTypeNameNode(parent, WTFMove(mangledName), AST::UnnamedType::Kind::Array)
        , m_numElements(numElements)
    {
    }
    virtual ~ArrayTypeNameNode() = default;
    unsigned numElements() const { return m_numElements; }

private:
    unsigned m_numElements;
};

class ArrayReferenceTypeNameNode final : public BaseTypeNameNode {
    WTF_MAKE_FAST_ALLOCATED;
public:
    ArrayReferenceTypeNameNode(BaseTypeNameNode* parent, MangledTypeName&& mangledName, AST::AddressSpace addressSpace)
        : BaseTypeNameNode(parent, WTFMove(mangledName), AST::UnnamedType::Kind::ArrayReference)
        , m_addressSpace(addressSpace)
    {
    }
    virtual ~ArrayReferenceTypeNameNode() = default;
    AST::AddressSpace addressSpace() const { return m_addressSpace; }

private:
    AST::AddressSpace m_addressSpace;
};

class PointerTypeNameNode final : public BaseTypeNameNode {
    WTF_MAKE_FAST_ALLOCATED;
public:
    PointerTypeNameNode(BaseTypeNameNode* parent, MangledTypeName&& mangledName, AST::AddressSpace addressSpace)
        : BaseTypeNameNode(parent, WTFMove(mangledName), AST::UnnamedType::Kind::Pointer)
        , m_addressSpace(addressSpace)
    {
    }
    virtual ~PointerTypeNameNode() = default;
    AST::AddressSpace addressSpace() const { return m_addressSpace; }

private:
    AST::AddressSpace m_addressSpace;
};

class ReferenceTypeNameNode final : public BaseTypeNameNode {
    WTF_MAKE_FAST_ALLOCATED;
public:
    ReferenceTypeNameNode(BaseTypeNameNode* parent, MangledTypeName&& mangledName, AST::NamedType& namedType)
        : BaseTypeNameNode(parent, WTFMove(mangledName), AST::UnnamedType::Kind::TypeReference)
        , m_namedType(namedType)
    {
    }
    virtual ~ReferenceTypeNameNode() = default;
    AST::NamedType& namedType() { return m_namedType; }

private:
    AST::NamedType& m_namedType;
};

}

}

}

#define SPECIALIZE_TYPE_TRAITS_WHLSL_BASE_TYPE_NAMED_NODE(ToValueTypeName, predicate) \
SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::WHLSL::Metal::ToValueTypeName) \
    static bool isType(const WebCore::WHLSL::Metal::BaseTypeNameNode& type) { return type.predicate; } \
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_WHLSL_BASE_TYPE_NAMED_NODE(ArrayTypeNameNode, isArrayTypeNameNode())

SPECIALIZE_TYPE_TRAITS_WHLSL_BASE_TYPE_NAMED_NODE(ArrayReferenceTypeNameNode, isArrayReferenceTypeNameNode())

SPECIALIZE_TYPE_TRAITS_WHLSL_BASE_TYPE_NAMED_NODE(PointerTypeNameNode, isPointerTypeNameNode())

SPECIALIZE_TYPE_TRAITS_WHLSL_BASE_TYPE_NAMED_NODE(ReferenceTypeNameNode, isReferenceTypeNameNode())

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
    insert(unnamedType);
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
        Vector<std::reference_wrapper<BaseTypeNameNode>> neighbors = { find(enumerationDefinition.type()) };
        auto addResult = m_dependencyGraph.add(&enumerationDefinition, WTFMove(neighbors));
        ASSERT_UNUSED(addResult, addResult.isNewEntry);
    }
}

void TypeNamer::visit(AST::NativeTypeDeclaration& nativeTypeDeclaration)
{
    // Native type declarations already have names, and are already declared in Metal.
    auto addResult = m_dependencyGraph.add(&nativeTypeDeclaration, Vector<std::reference_wrapper<BaseTypeNameNode>>());
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
        Vector<std::reference_wrapper<BaseTypeNameNode>> neighbors;
        for (auto& structureElement : structureDefinition.structureElements()) {
            auto addResult = m_structureElementMapping.add(&structureElement, generateNextStructureElementName());
            ASSERT_UNUSED(addResult, addResult.isNewEntry);
            neighbors.append(find(structureElement.type()));
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
        Vector<std::reference_wrapper<BaseTypeNameNode>> neighbors = { find(typeDefinition.type()) };
        auto addResult = m_dependencyGraph.add(&typeDefinition, WTFMove(neighbors));
        ASSERT_UNUSED(addResult, addResult.isNewEntry);
    }
}

void TypeNamer::visit(AST::Expression& expression)
{
    insert(expression.resolvedType());
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

BaseTypeNameNode& TypeNamer::find(AST::UnnamedType& unnamedType)
{
    auto iterator = m_unnamedTypesUniquingMap.find(unnamedType);
    ASSERT(iterator != m_unnamedTypesUniquingMap.end());
    return *iterator->value;
}

std::unique_ptr<BaseTypeNameNode> TypeNamer::createNameNode(AST::UnnamedType& unnamedType, BaseTypeNameNode* parent)
{
    switch (unnamedType.kind()) {
    case AST::UnnamedType::Kind::TypeReference: {
        auto& typeReference = downcast<AST::TypeReference>(unnamedType);
        return makeUnique<ReferenceTypeNameNode>(parent, generateNextTypeName(), typeReference.resolvedType());
    }
    case AST::UnnamedType::Kind::Pointer: {
        auto& pointerType = downcast<AST::PointerType>(unnamedType);
        return makeUnique<PointerTypeNameNode>(parent, generateNextTypeName(), pointerType.addressSpace());
    }
    case AST::UnnamedType::Kind::ArrayReference: {
        auto& arrayReferenceType = downcast<AST::ArrayReferenceType>(unnamedType);
        return makeUnique<ArrayReferenceTypeNameNode>(parent, generateNextTypeName(), arrayReferenceType.addressSpace());
    }
    case AST::UnnamedType::Kind::Array: {
        auto& arrayType = downcast<AST::ArrayType>(unnamedType);
        return makeUnique<ArrayTypeNameNode>(parent, generateNextTypeName(), arrayType.numElements());
    }
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
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

BaseTypeNameNode* TypeNamer::insert(AST::UnnamedType& unnamedType)
{
    if (auto* result = m_unnamedTypeMapping.get(&unnamedType))
        return result;

    auto* parentUnnamedType = parent(unnamedType);
    BaseTypeNameNode* parentNode = parentUnnamedType ? insert(*parentUnnamedType) : nullptr;

    auto addResult = m_unnamedTypesUniquingMap.ensure(UnnamedTypeKey { unnamedType }, [&] {
        return createNameNode(unnamedType, parentNode);
    });

    m_unnamedTypeMapping.add(&unnamedType, addResult.iterator->value.get());
    return addResult.iterator->value.get();
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

void TypeNamer::emitUnnamedTypeDefinition(StringBuilder& stringBuilder, BaseTypeNameNode& baseTypeNameNode, HashSet<AST::NamedType*>& emittedNamedTypes, HashSet<BaseTypeNameNode*>& emittedUnnamedTypes)
{
    if (emittedUnnamedTypes.contains(&baseTypeNameNode))
        return;

    if (baseTypeNameNode.parent())
        emitUnnamedTypeDefinition(stringBuilder, *baseTypeNameNode.parent(), emittedNamedTypes, emittedUnnamedTypes);
    
    switch (baseTypeNameNode.kind()) {
    case AST::UnnamedType::Kind::TypeReference: {
        auto& namedType = downcast<ReferenceTypeNameNode>(baseTypeNameNode).namedType();
        emitNamedTypeDefinition(stringBuilder, namedType, emittedNamedTypes, emittedUnnamedTypes);
        stringBuilder.append("typedef ", mangledNameForType(namedType), ' ', baseTypeNameNode.mangledName(), ";\n");
        break;
    }
    case AST::UnnamedType::Kind::Pointer: {
        auto& pointerType = downcast<PointerTypeNameNode>(baseTypeNameNode);
        ASSERT(baseTypeNameNode.parent());
        stringBuilder.append("typedef ", toString(pointerType.addressSpace()), ' ', pointerType.parent()->mangledName(), "* ", pointerType.mangledName(), ";\n");
        break;
    }
    case AST::UnnamedType::Kind::ArrayReference: {
        auto& arrayReferenceType = downcast<ArrayReferenceTypeNameNode>(baseTypeNameNode);
        ASSERT(baseTypeNameNode.parent());
        stringBuilder.append(
            "struct ", arrayReferenceType.mangledName(), " {\n"
            "    ", toString(arrayReferenceType.addressSpace()), ' ', arrayReferenceType.parent()->mangledName(), "* pointer;\n"
            "    uint32_t length;\n"
            "};\n"
        );
        break;
    }
    case AST::UnnamedType::Kind::Array: {
        auto& arrayType = downcast<ArrayTypeNameNode>(baseTypeNameNode);
        ASSERT(baseTypeNameNode.parent());
        stringBuilder.append("typedef array<", arrayType.parent()->mangledName(), ", ", arrayType.numElements(), "> ", arrayType.mangledName(), ";\n");
        break;
    }
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }

    emittedUnnamedTypes.add(&baseTypeNameNode);
}

void TypeNamer::emitNamedTypeDefinition(StringBuilder& stringBuilder, AST::NamedType& namedType, HashSet<AST::NamedType*>& emittedNamedTypes, HashSet<BaseTypeNameNode*>& emittedUnnamedTypes)
{
    if (emittedNamedTypes.contains(&namedType))
        return;
    auto iterator = m_dependencyGraph.find(&namedType);
    ASSERT(iterator != m_dependencyGraph.end());
    for (auto& baseTypeNameNode : iterator->value)
        emitUnnamedTypeDefinition(stringBuilder, baseTypeNameNode, emittedNamedTypes, emittedUnnamedTypes);
    if (is<AST::EnumerationDefinition>(namedType)) {
        auto& enumerationDefinition = downcast<AST::EnumerationDefinition>(namedType);
        auto& baseType = enumerationDefinition.type().unifyNode();
        stringBuilder.append("enum class ", mangledNameForType(enumerationDefinition), " : ", mangledNameForType(downcast<AST::NamedType>(baseType)), " {\n");
        for (auto& enumerationMember : enumerationDefinition.enumerationMembers())
            stringBuilder.append("    ", mangledNameForEnumerationMember(enumerationMember), " = ", enumerationMember.get().value(), ",\n");
        stringBuilder.append("};\n");
    } else if (is<AST::NativeTypeDeclaration>(namedType)) {
        // Native types already have definitions. There's nothing to do.
    } else if (is<AST::StructureDefinition>(namedType)) {
        auto& structureDefinition = downcast<AST::StructureDefinition>(namedType);
        stringBuilder.append("struct ", mangledNameForType(structureDefinition), " {\n");
        for (auto& structureElement : structureDefinition.structureElements())
            stringBuilder.append("    ", mangledNameForType(structureElement.type()), ' ', mangledNameForStructureElement(structureElement), ";\n");
        stringBuilder.append("};\n");
    } else {
        auto& typeDefinition = downcast<AST::TypeDefinition>(namedType);
        stringBuilder.append("typedef ", mangledNameForType(typeDefinition.type()), ' ', mangledNameForType(typeDefinition), ";\n");
    }
    emittedNamedTypes.add(&namedType);
}

void TypeNamer::emitMetalTypeDefinitions(StringBuilder& stringBuilder)
{
    HashSet<AST::NamedType*> emittedNamedTypes;
    HashSet<BaseTypeNameNode*> emittedUnnamedTypes;
    for (auto& namedType : m_dependencyGraph.keys())
        emitNamedTypeDefinition(stringBuilder, *namedType, emittedNamedTypes, emittedUnnamedTypes);
    for (auto& node : m_unnamedTypesUniquingMap.values())
        emitUnnamedTypeDefinition(stringBuilder, *node, emittedNamedTypes, emittedUnnamedTypes);
}

MangledTypeName TypeNamer::mangledNameForType(AST::UnnamedType& unnamedType)
{
    return find(unnamedType).mangledName();
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

#endif
