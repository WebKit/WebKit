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

class BaseTypeNameNode {
public:
    BaseTypeNameNode(BaseTypeNameNode* parent, String&& mangledName)
        : m_parent(parent)
        , m_mangledName(mangledName)
    {
    }
    virtual ~BaseTypeNameNode() = default;
    virtual bool isArrayTypeNameNode() const { return false; }
    virtual bool isArrayReferenceTypeNameNode() const { return false; }
    virtual bool isPointerTypeNameNode() const { return false; }
    virtual bool isReferenceTypeNameNode() const { return false; }
    Vector<UniqueRef<BaseTypeNameNode>>& children() { return m_children; }
    void append(UniqueRef<BaseTypeNameNode>&& child)
    {
        m_children.append(WTFMove(child));
    }
    BaseTypeNameNode* parent() { return m_parent; }
    const String& mangledName() const { return m_mangledName; }

private:
    Vector<UniqueRef<BaseTypeNameNode>> m_children;
    BaseTypeNameNode* m_parent;
    String m_mangledName;
};

class ArrayTypeNameNode : public BaseTypeNameNode {
public:
    ArrayTypeNameNode(BaseTypeNameNode* parent, String&& mangledName, unsigned numElements)
        : BaseTypeNameNode(parent, WTFMove(mangledName))
        , m_numElements(numElements)
    {
    }
    virtual ~ArrayTypeNameNode() = default;
    bool isArrayTypeNameNode() const override { return true; }
    unsigned numElements() const { return m_numElements; }

private:
    unsigned m_numElements;
};

class ArrayReferenceTypeNameNode : public BaseTypeNameNode {
public:
    ArrayReferenceTypeNameNode(BaseTypeNameNode* parent, String&& mangledName, AST::AddressSpace addressSpace)
        : BaseTypeNameNode(parent, WTFMove(mangledName))
        , m_addressSpace(addressSpace)
    {
    }
    virtual ~ArrayReferenceTypeNameNode() = default;
    bool isArrayReferenceTypeNameNode() const override { return true; }
    AST::AddressSpace addressSpace() const { return m_addressSpace; }

private:
    AST::AddressSpace m_addressSpace;
};

class PointerTypeNameNode : public BaseTypeNameNode {
public:
    PointerTypeNameNode(BaseTypeNameNode* parent, String&& mangledName, AST::AddressSpace addressSpace)
        : BaseTypeNameNode(parent, WTFMove(mangledName))
        , m_addressSpace(addressSpace)
    {
    }
    virtual ~PointerTypeNameNode() = default;
    bool isPointerTypeNameNode() const override { return true; }
    AST::AddressSpace addressSpace() const { return m_addressSpace; }

private:
    AST::AddressSpace m_addressSpace;
};

class ReferenceTypeNameNode : public BaseTypeNameNode {
public:
    ReferenceTypeNameNode(BaseTypeNameNode* parent, String&& mangledName, AST::NamedType& namedType)
        : BaseTypeNameNode(parent, WTFMove(mangledName))
        , m_namedType(namedType)
    {
    }
    virtual ~ReferenceTypeNameNode() = default;
    bool isReferenceTypeNameNode() const override { return true; }
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
    insert(unnamedType, m_trie);
}

void TypeNamer::visit(AST::EnumerationDefinition& enumerationDefinition)
{
    auto addResult = m_namedTypeMapping.add(&enumerationDefinition, generateNextTypeName());
    ASSERT_UNUSED(addResult, addResult.isNewEntry);
    for (auto& enumerationMember : enumerationDefinition.enumerationMembers()) {
        auto addResult = m_enumerationMemberMapping.add(&static_cast<AST::EnumerationMember&>(enumerationMember), generateNextEnumerationMemberName());
        ASSERT_UNUSED(addResult, addResult.isNewEntry);
    }
    Visitor::visit(enumerationDefinition);
}

void TypeNamer::visit(AST::NativeTypeDeclaration&)
{
    // Native type declarations already have names, and are already declared in Metal.
}

static Vector<UniqueRef<BaseTypeNameNode>>::iterator findInVector(AST::UnnamedType& unnamedType, Vector<UniqueRef<BaseTypeNameNode>>& types)
{
    return std::find_if(types.begin(), types.end(), [&](BaseTypeNameNode& baseTypeNameNode) -> bool {
        if (is<AST::TypeReference>(unnamedType) && is<ReferenceTypeNameNode>(baseTypeNameNode)) {
            auto* resolvedType = downcast<AST::TypeReference>(unnamedType).resolvedType();
            ASSERT(resolvedType);
            return resolvedType == &downcast<ReferenceTypeNameNode>(baseTypeNameNode).namedType();
        }
        if (is<AST::PointerType>(unnamedType) && is<PointerTypeNameNode>(baseTypeNameNode))
            return downcast<AST::PointerType>(unnamedType).addressSpace() == downcast<PointerTypeNameNode>(baseTypeNameNode).addressSpace();
        if (is<AST::ArrayReferenceType>(unnamedType) && is<ArrayReferenceTypeNameNode>(baseTypeNameNode))
            return downcast<AST::ArrayReferenceType>(unnamedType).addressSpace() == downcast<ArrayReferenceTypeNameNode>(baseTypeNameNode).addressSpace();
        if (is<AST::ArrayType>(unnamedType) && is<ArrayTypeNameNode>(baseTypeNameNode))
            return downcast<AST::ArrayType>(unnamedType).numElements() == downcast<ArrayTypeNameNode>(baseTypeNameNode).numElements();
        return false;
    });
}

static BaseTypeNameNode& find(AST::UnnamedType& unnamedType, Vector<UniqueRef<BaseTypeNameNode>>& types)
{
    auto& vectorToSearch = ([&]() -> Vector<UniqueRef<BaseTypeNameNode>>& {
        if (is<AST::TypeReference>(unnamedType))
            return types;
        if (is<AST::PointerType>(unnamedType))
            return find(downcast<AST::PointerType>(unnamedType).elementType(), types).children();
        if (is<AST::ArrayReferenceType>(unnamedType))
            return find(downcast<AST::ArrayReferenceType>(unnamedType).elementType(), types).children();
        ASSERT(is<AST::ArrayType>(unnamedType));
        return find(downcast<AST::ArrayType>(unnamedType).type(), types).children();
    })();
    auto iterator = findInVector(unnamedType, vectorToSearch);
    ASSERT(iterator != types.end());
    return *iterator;
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
            neighbors.append(find(structureElement.type(), m_trie));
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
        Vector<std::reference_wrapper<BaseTypeNameNode>> neighbors = { find(typeDefinition.type(), m_trie) };
        auto addResult = m_dependencyGraph.add(&typeDefinition, WTFMove(neighbors));
        ASSERT_UNUSED(addResult, addResult.isNewEntry);
    }
}

String TypeNamer::mangledNameForType(AST::NativeTypeDeclaration& nativeTypeDeclaration)
{
    return writeNativeType(nativeTypeDeclaration);
}

UniqueRef<BaseTypeNameNode> TypeNamer::createNameNode(AST::UnnamedType& unnamedType, BaseTypeNameNode* parent)
{
    if (is<AST::TypeReference>(unnamedType)) {
        auto& typeReference = downcast<AST::TypeReference>(unnamedType);
        ASSERT(typeReference.resolvedType());
        return makeUniqueRef<ReferenceTypeNameNode>(parent, generateNextTypeName(), *typeReference.resolvedType());
    }
    if (is<AST::PointerType>(unnamedType)) {
        auto& pointerType = downcast<AST::PointerType>(unnamedType);
        return makeUniqueRef<PointerTypeNameNode>(parent, generateNextTypeName(), pointerType.addressSpace());
    }
    if (is<AST::ArrayReferenceType>(unnamedType)) {
        auto& arrayReferenceType = downcast<AST::ArrayReferenceType>(unnamedType);
        return makeUniqueRef<ArrayReferenceTypeNameNode>(parent, generateNextTypeName(), arrayReferenceType.addressSpace());
    }
    ASSERT(is<AST::ArrayType>(unnamedType));
    auto& arrayType = downcast<AST::ArrayType>(unnamedType);
    return makeUniqueRef<ArrayTypeNameNode>(parent, generateNextTypeName(), arrayType.numElements());
}

size_t TypeNamer::insert(AST::UnnamedType& unnamedType, Vector<UniqueRef<BaseTypeNameNode>>& types)
{
    Vector<UniqueRef<BaseTypeNameNode>>* vectorToInsertInto { nullptr };
    BaseTypeNameNode* parent { nullptr };
    if (is<AST::TypeReference>(unnamedType)) {
        vectorToInsertInto = &types;
        parent = nullptr;
    } else if (is<AST::PointerType>(unnamedType)) {
        auto& item = types[insert(downcast<AST::PointerType>(unnamedType).elementType(), types)];
        vectorToInsertInto = &item->children();
        parent = &item;
    } else if (is<AST::ArrayReferenceType>(unnamedType)) {
        auto& item = types[insert(downcast<AST::ArrayReferenceType>(unnamedType).elementType(), types)];
        vectorToInsertInto = &item->children();
        parent = &item;
    } else {
        ASSERT(is<AST::ArrayType>(unnamedType));
        auto& item = types[insert(downcast<AST::ArrayType>(unnamedType).type(), types)];
        vectorToInsertInto = &item->children();
        parent = &item;
    }
    ASSERT(vectorToInsertInto);

    auto iterator = findInVector(unnamedType, *vectorToInsertInto);
    if (iterator == vectorToInsertInto->end()) {
        auto result = createNameNode(unnamedType, parent);
        {
            auto addResult = m_unnamedTypeMapping.add(&unnamedType, &result);
            ASSERT_UNUSED(addResult, addResult.isNewEntry);
        }
        vectorToInsertInto->append(WTFMove(result));
        return vectorToInsertInto->size() - 1;
    }
    auto addResult = m_unnamedTypeMapping.add(&unnamedType, &*iterator);
    ASSERT_UNUSED(addResult, addResult.isNewEntry);
    return iterator - vectorToInsertInto->begin();
}

class MetalTypeDeclarationWriter : public Visitor {
public:
    MetalTypeDeclarationWriter(std::function<String(AST::NamedType&)>&& mangledNameForNamedType, std::function<String(AST::UnnamedType&)>&& mangledNameForUnnamedType, std::function<String(AST::EnumerationMember&)>&& mangledNameForEnumerationMember)
        : m_mangledNameForNamedType(WTFMove(mangledNameForNamedType))
        , m_mangledNameForUnnamedType(WTFMove(mangledNameForUnnamedType))
        , m_mangledNameForEnumerationMember(WTFMove(mangledNameForEnumerationMember))
    {
    }

    String toString() { return m_stringBuilder.toString(); }

private:
    void visit(AST::EnumerationDefinition& enumerationDefinition)
    {
        auto& baseType = enumerationDefinition.type().unifyNode();
        ASSERT(is<AST::NamedType>(baseType));
        m_stringBuilder.append(makeString("enum class ", m_mangledNameForNamedType(enumerationDefinition), " : ", m_mangledNameForNamedType(downcast<AST::NamedType>(baseType)), " {\n"));
        for (auto& enumerationMember : enumerationDefinition.enumerationMembers())
            m_stringBuilder.append(makeString("    ", m_mangledNameForEnumerationMember(enumerationMember), ",\n"));
        m_stringBuilder.append("};\n");
    }

    void visit(AST::StructureDefinition& structureDefinition)
    {
        m_stringBuilder.append(makeString("struct ", m_mangledNameForNamedType(structureDefinition), ";\n"));
    }

    std::function<String(AST::NamedType&)> m_mangledNameForNamedType;
    std::function<String(AST::UnnamedType&)> m_mangledNameForUnnamedType;
    std::function<String(AST::EnumerationMember&)>&& m_mangledNameForEnumerationMember;
    StringBuilder m_stringBuilder;
};

String TypeNamer::metalTypeDeclarations()
{
    MetalTypeDeclarationWriter metalTypeDeclarationWriter([&](AST::NamedType& namedType) -> String {
        return mangledNameForType(namedType);
    }, [&](AST::UnnamedType& unnamedType) -> String {
        return mangledNameForType(unnamedType);
    }, [&](AST::EnumerationMember& enumerationMember) -> String {
        return mangledNameForEnumerationMember(enumerationMember);
    });
    metalTypeDeclarationWriter.Visitor::visit(m_program);
    return metalTypeDeclarationWriter.toString();
}

static String toString(AST::AddressSpace addressSpace)
{
    switch (addressSpace) {
    case AST::AddressSpace::Constant:
        return "constant"_str;
    case AST::AddressSpace::Device:
        return "device"_str;
    case AST::AddressSpace::Threadgroup:
        return "threadgroup"_str;
    default:
        ASSERT(addressSpace == AST::AddressSpace::Thread);
        return "thread"_str;
    }
}

void TypeNamer::emitUnnamedTypeDefinition(BaseTypeNameNode& baseTypeNameNode, HashSet<AST::NamedType*>& emittedNamedTypes, HashSet<BaseTypeNameNode*>& emittedUnnamedTypes, StringBuilder& stringBuilder)
{
    if (emittedUnnamedTypes.contains(&baseTypeNameNode))
        return;
    if (baseTypeNameNode.parent())
        emitUnnamedTypeDefinition(*baseTypeNameNode.parent(), emittedNamedTypes, emittedUnnamedTypes, stringBuilder);
    if (is<ReferenceTypeNameNode>(baseTypeNameNode)) {
        auto& namedType = downcast<ReferenceTypeNameNode>(baseTypeNameNode).namedType();
        emitNamedTypeDefinition(namedType, emittedNamedTypes, emittedUnnamedTypes, stringBuilder);
        stringBuilder.append(makeString("typedef ", mangledNameForType(namedType), ' ', baseTypeNameNode.mangledName(), ";\n"));
    } else if (is<PointerTypeNameNode>(baseTypeNameNode)) {
        auto& pointerType = downcast<PointerTypeNameNode>(baseTypeNameNode);
        ASSERT(baseTypeNameNode.parent());
        stringBuilder.append(makeString("typedef ", toString(pointerType.addressSpace()), " ", pointerType.parent()->mangledName(), "* ", pointerType.mangledName(), ";\n"));
    } else if (is<ArrayReferenceTypeNameNode>(baseTypeNameNode)) {
        auto& arrayReferenceType = downcast<ArrayReferenceTypeNameNode>(baseTypeNameNode);
        ASSERT(baseTypeNameNode.parent());
        stringBuilder.append(makeString("struct ", arrayReferenceType.mangledName(), "{ \n"));
        stringBuilder.append(makeString("    ", toString(arrayReferenceType.addressSpace()), " ", arrayReferenceType.parent()->mangledName(), "* pointer;\n"));
        stringBuilder.append("    unsigned length;\n");
        stringBuilder.append("};\n");
    } else {
        ASSERT(is<ArrayTypeNameNode>(baseTypeNameNode));
        auto& arrayType = downcast<ArrayTypeNameNode>(baseTypeNameNode);
        ASSERT(baseTypeNameNode.parent());
        stringBuilder.append(makeString("typedef Array<", arrayType.parent()->mangledName(), ", ", arrayType.numElements(), "> ", arrayType.mangledName(), ";\n"));
    }
    emittedUnnamedTypes.add(&baseTypeNameNode);
}

void TypeNamer::emitNamedTypeDefinition(AST::NamedType& namedType, HashSet<AST::NamedType*>& emittedNamedTypes, HashSet<BaseTypeNameNode*>& emittedUnnamedTypes, StringBuilder& stringBuilder)
{
    if (emittedNamedTypes.contains(&namedType))
        return;
    auto iterator = m_dependencyGraph.find(&namedType);
    ASSERT(iterator != m_dependencyGraph.end());
    for (auto& baseTypeNameNode : iterator->value)
        emitUnnamedTypeDefinition(baseTypeNameNode, emittedNamedTypes, emittedUnnamedTypes, stringBuilder);
    if (is<AST::EnumerationDefinition>(namedType)) {
        // We already emitted this in the type declaration section. There's nothing to do.
    } else if (is<AST::NativeTypeDeclaration>(namedType)) {
        // Native types already have definitions. There's nothing to do.
    } else if (is<AST::StructureDefinition>(namedType)) {
        auto& structureDefinition = downcast<AST::StructureDefinition>(namedType);
        stringBuilder.append(makeString("struct ", mangledNameForType(structureDefinition), " {\n"));
        for (auto& structureElement : structureDefinition.structureElements())
            stringBuilder.append(makeString("    ", mangledNameForType(structureElement.type()), ' ', mangledNameForStructureElement(structureElement), ";\n"));
        stringBuilder.append("};\n");
    } else {
        ASSERT(is<AST::TypeDefinition>(namedType));
        auto& typeDefinition = downcast<AST::TypeDefinition>(namedType);
        stringBuilder.append(makeString("typedef ", mangledNameForType(typeDefinition.type()), ' ', mangledNameForType(typeDefinition), ";\n"));
    }
    emittedNamedTypes.add(&namedType);
}

String TypeNamer::metalTypeDefinitions()
{
    HashSet<AST::NamedType*> emittedNamedTypes;
    HashSet<BaseTypeNameNode*> emittedUnnamedTypes;
    StringBuilder stringBuilder;
    for (auto& keyValuePair : m_dependencyGraph)
        emitNamedTypeDefinition(*keyValuePair.key, emittedNamedTypes, emittedUnnamedTypes, stringBuilder);
    for (auto& baseTypeNameNode : m_trie)
        emitUnnamedTypeDefinition(baseTypeNameNode, emittedNamedTypes, emittedUnnamedTypes, stringBuilder);
    return stringBuilder.toString();
}

String TypeNamer::mangledNameForType(AST::UnnamedType& unnamedType)
{
    return find(unnamedType, m_trie).mangledName();
}

String TypeNamer::mangledNameForType(AST::NamedType& namedType)
{
    if (is<AST::NativeTypeDeclaration>(namedType))
        return mangledNameForType(downcast<AST::NativeTypeDeclaration>(namedType));
    auto iterator = m_namedTypeMapping.find(&namedType);
    ASSERT(iterator != m_namedTypeMapping.end());
    return iterator->value;
}


String TypeNamer::mangledNameForEnumerationMember(AST::EnumerationMember& enumerationMember)
{
    auto iterator = m_enumerationMemberMapping.find(&enumerationMember);
    ASSERT(iterator != m_enumerationMemberMapping.end());
    return iterator->value;
}

String TypeNamer::mangledNameForStructureElement(AST::StructureElement& structureElement)
{
    auto iterator = m_structureElementMapping.find(&structureElement);
    ASSERT(iterator != m_structureElementMapping.end());
    return iterator->value;
}

String TypeNamer::metalTypes()
{
    Visitor::visit(m_program);
    StringBuilder stringBuilder;
    stringBuilder.append(metalTypeDeclarations());
    stringBuilder.append('\n');
    stringBuilder.append(metalTypeDefinitions());
    return stringBuilder.toString();
}

} // namespace Metal

} // namespace WHLSL

} // namespace WebCore

#endif
