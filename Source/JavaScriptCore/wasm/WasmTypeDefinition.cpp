/*
 * Copyright (C) 2016-2019 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WasmTypeDefinition.h"

#if ENABLE(WEBASSEMBLY)

#include "WasmFormat.h"
#include "WasmTypeDefinitionInlines.h"
#include <wtf/CommaPrinter.h>
#include <wtf/FastMalloc.h>
#include <wtf/StringPrintStream.h>

namespace JSC { namespace Wasm {

String TypeDefinition::toString() const
{
    return WTF::toString(*this);
}

void TypeDefinition::dump(PrintStream& out) const
{
    if (is<FunctionSignature>())
        return as<FunctionSignature>()->dump(out);

    if (is<StructType>())
        return as<StructType>()->dump(out);

    if (is<ArrayType>())
        return as<ArrayType>()->dump(out);

    if (is<RecursionGroup>())
        return as<RecursionGroup>()->dump(out);

    if (is<Projection>())
        return as<Projection>()->dump(out);

    ASSERT(is<Subtype>());
    return as<Subtype>()->dump(out);
}

String FunctionSignature::toString() const
{
    return WTF::toString(*this);
}

void FunctionSignature::dump(PrintStream& out) const
{
    {
        out.print("(");
        CommaPrinter comma;
        for (FunctionArgCount arg = 0; arg < argumentCount(); ++arg)
            out.print(comma, makeString(argumentType(arg).kind));
        out.print(")");
    }

    {
        CommaPrinter comma;
        out.print(" -> [");
        for (FunctionArgCount ret = 0; ret < returnCount(); ++ret)
            out.print(comma, makeString(returnType(ret).kind));
        out.print("]");
    }
}

String StructType::toString() const
{
    return WTF::toString(*this);
}

void StructType::dump(PrintStream& out) const
{
    out.print("(");
    CommaPrinter comma;
    for (StructFieldCount fieldIndex = 0; fieldIndex < fieldCount(); ++fieldIndex) {
        out.print(comma, makeString(field(fieldIndex).type.kind));
        out.print(comma, field(fieldIndex).mutability ? "immutable" : "mutable");
    }
    out.print(")");
}

StructType::StructType(FieldType* payload, StructFieldCount fieldCount, const FieldType* fieldTypes)
    : m_payload(payload)
    , m_fieldCount(fieldCount)
    , m_hasRecursiveReference(false)
{
    bool hasRecursiveReference = false;
    unsigned currentFieldOffset = 0;
    for (unsigned fieldIndex = 0; fieldIndex < m_fieldCount; ++fieldIndex) {
        const auto& fieldType = fieldTypes[fieldIndex];
        hasRecursiveReference |= isRefWithRecursiveReference(fieldType.type);
        getField(fieldIndex) = fieldType;
        *getFieldOffset(fieldIndex) = currentFieldOffset;
        currentFieldOffset += typeKindSizeInBytes(field(fieldIndex).type.kind);
    }

    m_instancePayloadSize = WTF::roundUpToMultipleOf<sizeof(uint64_t)>(currentFieldOffset);
    setHasRecursiveReference(hasRecursiveReference);
}

String ArrayType::toString() const
{
    return WTF::toString(*this);
}

void ArrayType::dump(PrintStream& out) const
{
    out.print("(");
    CommaPrinter comma;
    out.print(comma, makeString(elementType().type.kind));
    out.print(comma, elementType().mutability ? "immutable" : "mutable");
    out.print(")");
}

String RecursionGroup::toString() const
{
    return WTF::toString(*this);
}

void RecursionGroup::dump(PrintStream& out) const
{
    out.print("(");
    CommaPrinter comma;
    for (RecursionGroupCount typeIndex = 0; typeIndex < typeCount(); ++typeIndex) {
        out.print(comma);
        TypeInformation::get(type(typeIndex)).dump(out);
    }
    out.print(")");
}

String Projection::toString() const
{
    return WTF::toString(*this);
}

void Projection::dump(PrintStream& out) const
{
    out.print("(");
    CommaPrinter comma;
    if (isPlaceholder())
        out.print("<current-rec-group>");
    else
        TypeInformation::get(recursionGroup()).dump(out);
    out.print(".", index());
    out.print(")");
}

String Subtype::toString() const
{
    return WTF::toString(*this);
}

void Subtype::dump(PrintStream& out) const
{
    out.print("(");
    CommaPrinter comma;
    TypeInformation::get(superType()).dump(out);
    out.print(comma);
    TypeInformation::get(underlyingType()).dump(out);
    out.print(")");
}

void TypeDefinition::cleanup()
{
    // Only compound type definitions need to be cleaned up, not, e.g., function types.
    if (is<Subtype>())
        return as<Subtype>()->cleanup();
    if (is<Projection>())
        return as<Projection>()->cleanup();
    if (is<RecursionGroup>())
        return as<RecursionGroup>()->cleanup();
}

void Subtype::cleanup()
{
    TypeInformation::get(superType()).deref();
    TypeInformation::get(underlyingType()).deref();
}

void Projection::cleanup()
{
    if (recursionGroup() != TypeDefinition::invalidIndex)
        TypeInformation::get(recursionGroup()).deref();
}

void RecursionGroup::cleanup()
{
    for (RecursionGroupCount i = 0; i < typeCount(); i++)
        TypeInformation::get(type(i)).deref();
}

static unsigned computeSignatureHash(size_t returnCount, const Type* returnTypes, size_t argumentCount, const Type* argumentTypes)
{
    unsigned accumulator = 0xa1bcedd8u;
    for (uint32_t i = 0; i < argumentCount; ++i) {
        accumulator = WTF::pairIntHash(accumulator, WTF::IntHash<uint8_t>::hash(static_cast<uint8_t>(argumentTypes[i].kind)));
        accumulator = WTF::pairIntHash(accumulator, WTF::IntHash<unsigned>::hash(argumentTypes[i].index));
    }
    for (uint32_t i = 0; i < returnCount; ++i) {
        accumulator = WTF::pairIntHash(accumulator, WTF::IntHash<uint8_t>::hash(static_cast<uint8_t>(returnTypes[i].kind)));
        accumulator = WTF::pairIntHash(accumulator, WTF::IntHash<unsigned>::hash(returnTypes[i].index));
    }
    return accumulator;
}

static unsigned computeStructTypeHash(size_t fieldCount, const FieldType* fields)
{
    unsigned accumulator = 0x15d2546;
    for (uint32_t i = 0; i < fieldCount; ++i) {
        accumulator = WTF::pairIntHash(accumulator, WTF::IntHash<uint8_t>::hash(static_cast<uint8_t>(fields[i].type.kind)));
        accumulator = WTF::pairIntHash(accumulator, WTF::IntHash<uint8_t>::hash(static_cast<uint8_t>(fields[i].type.index)));
        accumulator = WTF::pairIntHash(accumulator, WTF::IntHash<uint8_t>::hash(static_cast<uint8_t>(fields[i].mutability)));
    }
    return accumulator;
}

static unsigned computeArrayTypeHash(FieldType elementType)
{
    unsigned accumulator = 0x7835ab;
    accumulator = WTF::pairIntHash(accumulator, WTF::IntHash<uint8_t>::hash(static_cast<uint8_t>(elementType.type.kind)));
    accumulator = WTF::pairIntHash(accumulator, WTF::IntHash<TypeIndex>::hash(elementType.type.index));
    accumulator = WTF::pairIntHash(accumulator, WTF::IntHash<uint8_t>::hash(static_cast<uint8_t>(elementType.mutability)));
    return accumulator;
}

static unsigned computeRecursionGroupHash(size_t typeCount, const TypeIndex* types)
{
    unsigned accumulator = 0x9cfb89bb;
    for (uint32_t i = 0; i < typeCount; ++i)
        accumulator = WTF::pairIntHash(accumulator, WTF::IntHash<TypeIndex>::hash(static_cast<TypeIndex>(types[i])));
    return accumulator;
}

static unsigned computeProjectionHash(TypeIndex recursionGroup, ProjectionIndex index)
{
    unsigned accumulator = 0xbeae6d4e;
    accumulator = WTF::pairIntHash(accumulator, WTF::IntHash<TypeIndex>::hash(static_cast<TypeIndex>(recursionGroup)));
    accumulator = WTF::pairIntHash(accumulator, WTF::IntHash<uint32_t>::hash(static_cast<uint32_t>(index)));
    return accumulator;
}

static unsigned computeSubtypeHash(TypeIndex superType, TypeIndex underlyingType)
{
    unsigned accumulator = 0x3efa01b9;
    accumulator = WTF::pairIntHash(accumulator, WTF::IntHash<TypeIndex>::hash(superType));
    accumulator = WTF::pairIntHash(accumulator, WTF::IntHash<TypeIndex>::hash(underlyingType));
    return accumulator;
}

unsigned TypeDefinition::hash() const
{
    if (is<FunctionSignature>()) {
        const FunctionSignature* signature = as<FunctionSignature>();
        return computeSignatureHash(signature->returnCount(), signature->storage(0), signature->argumentCount(), signature->storage(signature->returnCount()));
    }

    if (is<StructType>()) {
        const StructType* structType = as<StructType>();
        return computeStructTypeHash(structType->fieldCount(), structType->storage(0));
    }

    if (is<ArrayType>()) {
        const ArrayType* arrayType = as<ArrayType>();
        return computeArrayTypeHash(arrayType->elementType());
    }

    if (is<RecursionGroup>()) {
        const RecursionGroup* recursionGroup = as<RecursionGroup>();
        return computeRecursionGroupHash(recursionGroup->typeCount(), recursionGroup->storage(0));
    }

    if (is<Projection>()) {
        const Projection* projection = as<Projection>();
        return computeProjectionHash(projection->recursionGroup(), projection->index());
    }

    ASSERT(is<Subtype>());
    const Subtype* subtype = as<Subtype>();
    return computeSubtypeHash(subtype->superType(), subtype->underlyingType());
}

RefPtr<TypeDefinition> TypeDefinition::tryCreateFunctionSignature(FunctionArgCount returnCount, FunctionArgCount argumentCount)
{
    // We use WTF_MAKE_FAST_ALLOCATED for this class.
    auto result = tryFastMalloc(allocatedFunctionSize(returnCount, argumentCount));
    void* memory = nullptr;
    if (!result.getValue(memory))
        return nullptr;
    TypeDefinition* signature = new (NotNull, memory) TypeDefinition(TypeDefinitionKind::FunctionSignature, returnCount, argumentCount);
    return adoptRef(signature);
}

RefPtr<TypeDefinition> TypeDefinition::tryCreateStructType(StructFieldCount fieldCount, const FieldType* fields)
{
    // We use WTF_MAKE_FAST_ALLOCATED for this class.
    auto result = tryFastMalloc(allocatedStructSize(fieldCount));
    void* memory = nullptr;
    if (!result.getValue(memory))
        return nullptr;
    TypeDefinition* signature = new (NotNull, memory) TypeDefinition(TypeDefinitionKind::StructType, fieldCount, fields);
    return adoptRef(signature);
}

RefPtr<TypeDefinition> TypeDefinition::tryCreateArrayType()
{
    // We use WTF_MAKE_FAST_ALLOCATED for this class.
    auto result = tryFastMalloc(allocatedArraySize());
    void* memory = nullptr;
    if (!result.getValue(memory))
        return nullptr;
    TypeDefinition* signature = new (NotNull, memory) TypeDefinition(TypeDefinitionKind::ArrayType);
    return adoptRef(signature);
}

RefPtr<TypeDefinition> TypeDefinition::tryCreateRecursionGroup(RecursionGroupCount typeCount)
{
    // We use WTF_MAKE_FAST_ALLOCATED for this class.
    auto result = tryFastMalloc(allocatedRecursionGroupSize(typeCount));
    void* memory = nullptr;
    if (!result.getValue(memory))
        return nullptr;
    TypeDefinition* signature = new (NotNull, memory) TypeDefinition(TypeDefinitionKind::RecursionGroup, typeCount);
    return adoptRef(signature);
}

RefPtr<TypeDefinition> TypeDefinition::tryCreateProjection()
{
    // We use WTF_MAKE_FAST_ALLOCATED for this class.
    auto result = tryFastMalloc(allocatedProjectionSize());
    void* memory = nullptr;
    if (!result.getValue(memory))
        return nullptr;
    TypeDefinition* signature = new (NotNull, memory) TypeDefinition(TypeDefinitionKind::Projection);
    return adoptRef(signature);
}

RefPtr<TypeDefinition> TypeDefinition::tryCreateSubtype()
{
    // We use WTF_MAKE_FAST_ALLOCATED for this class.
    auto result = tryFastMalloc(allocatedSubtypeSize());
    void* memory = nullptr;
    if (!result.getValue(memory))
        return nullptr;
    TypeDefinition* signature = new (NotNull, memory) TypeDefinition(TypeDefinitionKind::Subtype);
    return adoptRef(signature);
}

// Recursive types are stored "tied" in the sense that the spec refers to here:
//
//   https://github.com/WebAssembly/gc/blob/main/proposals/gc/MVP.md#equivalence
//
// That is, the recursive "back edges" are stored as a special type index. These
// need to be substituted back out to a Projection eventually so that the type
// can be further expanded if necessary. The substitute and replacePlaceholders
// functions below are used to implement this substitution.
Type TypeDefinition::substitute(Type type, TypeIndex projectee)
{
    if (isRefWithTypeIndex(type) && TypeInformation::get(type.index).is<Projection>()) {
        const Projection* projection = TypeInformation::get(type.index).as<Projection>();
        if (projection->isPlaceholder()) {
            RefPtr<TypeDefinition> newProjection = TypeInformation::typeDefinitionForProjection(projectee, projection->index());
            TypeKind kind = type.isNullable() ? TypeKind::RefNull : TypeKind::Ref;
            return Type { kind, newProjection->index() };
        }
    }

    return type;
}

// Perform a substitution as above but for a Subtype's parent type.
static TypeIndex substituteParent(TypeIndex parent, TypeIndex projectee)
{
    if (TypeInformation::get(parent).is<Projection>()) {
        const Projection* projection = TypeInformation::get(parent).as<Projection>();
        if (projection->isPlaceholder())
            return TypeInformation::typeDefinitionForProjection(projectee, projection->index())->index();
    }

    return parent;
}

// This operation is a helper for expand() that calls substitute() in order
// to replace placeholder recursive references in structural types.
const TypeDefinition& TypeDefinition::replacePlaceholders(TypeIndex projectee) const
{
    if (is<FunctionSignature>()) {
        const FunctionSignature* func = as<FunctionSignature>();
        Vector<Type> newArguments;
        Vector<Type, 1> newReturns;
        newArguments.tryReserveCapacity(func->argumentCount());
        newReturns.tryReserveCapacity(func->returnCount());
        for (unsigned i = 0; i < func->argumentCount(); ++i)
            newArguments.uncheckedAppend(substitute(func->argumentType(i), projectee));
        for (unsigned i = 0; i < func->returnCount(); ++i)
            newReturns.uncheckedAppend(substitute(func->returnType(i), projectee));

        RefPtr<TypeDefinition> def = TypeInformation::typeDefinitionForFunction(newReturns, newArguments);
        return *def;
    }

    if (is<StructType>()) {
        const StructType* structType = as<StructType>();
        Vector<FieldType> newFields;
        newFields.tryReserveCapacity(structType->fieldCount());
        for (unsigned i = 0; i < structType->fieldCount(); i++) {
            FieldType field = structType->field(i);
            newFields.uncheckedAppend(FieldType { substitute(field.type, projectee), field.mutability });
        }

        RefPtr<TypeDefinition> def = TypeInformation::typeDefinitionForStruct(newFields);
        return *def;
    }

    if (is<ArrayType>()) {
        const ArrayType* arrayType = as<ArrayType>();
        FieldType field = arrayType->elementType();
        RefPtr<TypeDefinition> def = TypeInformation::typeDefinitionForArray(FieldType { substitute(field.type, projectee), field.mutability });
        return *def;
    }

    if (is<Subtype>()) {
        const Subtype* subtype = as<Subtype>();
        const TypeDefinition& newUnderlyingType = TypeInformation::get(subtype->underlyingType()).replacePlaceholders(projectee);
        RefPtr<TypeDefinition> def = TypeInformation::typeDefinitionForSubtype(substituteParent(subtype->superType(), projectee), newUnderlyingType.index());
        return *def;
    }

    return *this;
}

// This function corresponds to the unroll metafunction from the spec:
//
//  https://github.com/WebAssembly/gc/blob/main/proposals/gc/MVP.md#auxiliary-definitions
//
// It unrolls a potentially recursive type to a Subtype or structural type.
const TypeDefinition& TypeDefinition::unroll() const
{
    if (is<Projection>()) {
        const Projection& projection = *as<Projection>();
        const TypeDefinition& projectee = TypeInformation::get(projection.recursionGroup());

        const RecursionGroup& recursionGroup = *projectee.as<RecursionGroup>();
        const TypeDefinition& underlyingType = TypeInformation::get(recursionGroup.type(projection.index()));

        if (underlyingType.hasRecursiveReference()) {
            if (std::optional<TypeIndex> cachedUnrolling = TypeInformation::tryGetCachedUnrolling(index()))
                return TypeInformation::get(*cachedUnrolling);

            const TypeDefinition& unrolled = underlyingType.replacePlaceholders(projectee.index());
            TypeInformation::addCachedUnrolling(index(), unrolled.index());
            return unrolled;
        }

        return underlyingType;
    }

    return *this;
}

// This function corresponds to the expand metafunction from the spec:
//
//  https://github.com/WebAssembly/gc/blob/main/proposals/gc/MVP.md#auxiliary-definitions
//
// It expands a potentially recursive context type and returns the concrete structural
// type definition that it corresponds to. It should be called whenever the concrete
// type is needed during validation or other phases.
const TypeDefinition& TypeDefinition::expand() const
{
    const TypeDefinition& unrolled = unroll();
    if (unrolled.is<Subtype>())
        return TypeInformation::get(unrolled.as<Subtype>()->underlyingType());

    return unrolled;
}

// Determine if, for a structural type or subtype, the type contains any references to recursion group members.
bool TypeDefinition::hasRecursiveReference() const
{
    if (is<FunctionSignature>())
        return as<FunctionSignature>()->hasRecursiveReference();
    if (is<StructType>())
        return as<StructType>()->hasRecursiveReference();
    if (is<ArrayType>())
        return as<ArrayType>()->hasRecursiveReference();

    ASSERT(is<Subtype>());
    const TypeDefinition& supertype = TypeInformation::get(as<Subtype>()->superType());
    const bool hasRecGroupSupertype = supertype.is<Projection>() && supertype.as<Projection>()->isPlaceholder();
    return hasRecGroupSupertype || TypeInformation::get(as<Subtype>()->underlyingType()).hasRecursiveReference();
}

RefPtr<RTT> RTT::tryCreateRTT(DisplayCount displaySize)
{
    auto result = tryFastMalloc(allocatedRTTSize(displaySize));
    void* memory = nullptr;
    if (!result.getValue(memory))
        return nullptr;
    return new (NotNull, memory) RTT(displaySize);
}

bool RTT::isSubRTT(const RTT& parent) const
{
    if (displaySize() > 0) {
        if (parent.displaySize() > 0) {
            if (displaySize() <= parent.displaySize())
                return false;
            return &parent == displayEntry(displaySize() - parent.displaySize() - 1);
        }
        // If not a subtype itself, the parent must be at the top of the display.
        return &parent == displayEntry(displaySize() - 1);
    }

    return false;
}

const TypeDefinition& TypeInformation::signatureForLLIntBuiltin(LLIntBuiltin builtin)
{
    switch (builtin) {
    case LLIntBuiltin::CurrentMemory:
        return *singleton().m_I64_Void;
    case LLIntBuiltin::MemoryFill:
    case LLIntBuiltin::MemoryCopy:
        return *singleton().m_Void_I32I32I32;
    case LLIntBuiltin::MemoryInit:
        return *singleton().m_Void_I32I32I32I32;
    case LLIntBuiltin::TableSize:
        return *singleton().m_I32_I32;
    case LLIntBuiltin::TableCopy:
        return *singleton().m_Void_I32I32I32I32I32;
    case LLIntBuiltin::DataDrop:
    case LLIntBuiltin::ElemDrop:
        return *singleton().m_Void_I32;
    }
    RELEASE_ASSERT_NOT_REACHED();
    return *singleton().m_I64_Void;
}

struct FunctionParameterTypes {
    const Vector<Type, 1>& returnTypes;
    const Vector<Type>& argumentTypes;

    static unsigned hash(const FunctionParameterTypes& params)
    {
        return computeSignatureHash(params.returnTypes.size(), params.returnTypes.data(), params.argumentTypes.size(), params.argumentTypes.data());
    }

    static bool equal(const TypeHash& sig, const FunctionParameterTypes& params)
    {
        if (!sig.key->is<FunctionSignature>())
            return false;

        const FunctionSignature* signature = sig.key->as<FunctionSignature>();
        if (signature->argumentCount() != params.argumentTypes.size())
            return false;
        if (signature->returnCount() != params.returnTypes.size())
            return false;

        for (unsigned i = 0; i < signature->argumentCount(); ++i) {
            if (signature->argumentType(i) != params.argumentTypes[i])
                return false;
        }

        for (unsigned i = 0; i < signature->returnCount(); ++i) {
            if (signature->returnType(i) != params.returnTypes[i])
                return false;
        }
        return true;
    }

    // The translate method (here and in structs below) is used as a part of the
    // HashTranslator interface in order to construct a hash set entry when the entry
    // is not already in the set. See HashSet.h for details.
    static void translate(TypeHash& entry, const FunctionParameterTypes& params, unsigned)
    {
        RefPtr<TypeDefinition> signature = TypeDefinition::tryCreateFunctionSignature(params.returnTypes.size(), params.argumentTypes.size());
        RELEASE_ASSERT(signature);
        bool hasRecursiveReference = false;
        bool argumentsOrResultsIncludeV128 = false;

        for (unsigned i = 0; i < params.returnTypes.size(); ++i) {
            signature->as<FunctionSignature>()->getReturnType(i) = params.returnTypes[i];
            hasRecursiveReference |= isRefWithRecursiveReference(params.returnTypes[i]);
            argumentsOrResultsIncludeV128 |= params.returnTypes[i].isV128();
        }

        for (unsigned i = 0; i < params.argumentTypes.size(); ++i) {
            signature->as<FunctionSignature>()->getArgumentType(i) = params.argumentTypes[i];
            hasRecursiveReference |= isRefWithRecursiveReference(params.argumentTypes[i]);
            argumentsOrResultsIncludeV128 |= params.argumentTypes[i].isV128();
        }

        signature->as<FunctionSignature>()->setHasRecursiveReference(hasRecursiveReference);
        signature->as<FunctionSignature>()->setArgumentsOrResultsIncludeV128(argumentsOrResultsIncludeV128);

        entry.key = WTFMove(signature);
    }
};

struct StructParameterTypes {
    const Vector<FieldType>& fields;

    static unsigned hash(const StructParameterTypes& params)
    {
        return computeStructTypeHash(params.fields.size(), params.fields.data());
    }

    static bool equal(const TypeHash& sig, const StructParameterTypes& params)
    {
        if (!sig.key->is<StructType>())
            return false;

        const StructType* structType = sig.key->as<StructType>();
        if (structType->fieldCount() != params.fields.size())
            return false;

        for (unsigned i = 0; i < structType->fieldCount(); ++i) {
            if (structType->field(i) != params.fields[i])
                return false;
        }

        return true;
    }

    static void translate(TypeHash& entry, const StructParameterTypes& params, unsigned)
    {
        RefPtr<TypeDefinition> signature = TypeDefinition::tryCreateStructType(params.fields.size(), params.fields.data());
        RELEASE_ASSERT(signature);
        entry.key = WTFMove(signature);
    }
};

struct ArrayParameterTypes {
    FieldType elementType;

    static unsigned hash(const ArrayParameterTypes& params)
    {
        return computeArrayTypeHash(params.elementType);
    }

    static bool equal(const TypeHash& sig, const ArrayParameterTypes& params)
    {
        if (!sig.key->is<ArrayType>())
            return false;

        const ArrayType* arrayType = sig.key->as<ArrayType>();

        if (arrayType->elementType() != params.elementType)
            return false;

        return true;
    }

    static void translate(TypeHash& entry, const ArrayParameterTypes& params, unsigned)
    {
        RefPtr<TypeDefinition> signature = TypeDefinition::tryCreateArrayType();
        RELEASE_ASSERT(signature);

        ArrayType* arrayType = signature->as<ArrayType>();
        arrayType->getElementType() = params.elementType;
        arrayType->setHasRecursiveReference(isRefWithRecursiveReference(params.elementType.type));

        entry.key = WTFMove(signature);
    }
};

struct RecursionGroupParameterTypes {
    const Vector<TypeIndex>& types;

    static unsigned hash(const RecursionGroupParameterTypes& params)
    {
        return computeRecursionGroupHash(params.types.size(), params.types.data());
    }

    static bool equal(const TypeHash& sig, const RecursionGroupParameterTypes& params)
    {
        if (!sig.key->is<RecursionGroup>())
            return false;

        const RecursionGroup* recursionGroup = sig.key->as<RecursionGroup>();
        if (recursionGroup->typeCount() != params.types.size())
            return false;

        for (unsigned i = 0; i < recursionGroup->typeCount(); ++i) {
            if (recursionGroup->type(i) != params.types[i])
                return false;
        }

        return true;
    }

    static void translate(TypeHash& entry, const RecursionGroupParameterTypes& params, unsigned)
    {
        RefPtr<TypeDefinition> signature = TypeDefinition::tryCreateRecursionGroup(params.types.size());
        RELEASE_ASSERT(signature);

        RecursionGroup* recursionGroup = signature->as<RecursionGroup>();
        for (unsigned i = 0; i < params.types.size(); ++i) {
            TypeInformation::get(params.types[i]).ref();
            recursionGroup->getType(i) = params.types[i];
        }

        entry.key = WTFMove(signature);
    }
};

struct ProjectionParameterTypes {
    const TypeIndex recursionGroup;
    const ProjectionIndex index;

    static unsigned hash(const ProjectionParameterTypes& params)
    {
        return computeProjectionHash(params.recursionGroup, params.index);
    }

    static bool equal(const TypeHash& sig, const ProjectionParameterTypes& params)
    {
        if (!sig.key->is<Projection>())
            return false;

        const Projection* projection = sig.key->as<Projection>();
        if (projection->recursionGroup() != params.recursionGroup || projection->index() != params.index)
            return false;

        return true;
    }

    static void translate(TypeHash& entry, const ProjectionParameterTypes& params, unsigned)
    {
        RefPtr<TypeDefinition> signature = TypeDefinition::tryCreateProjection();
        RELEASE_ASSERT(signature);

        Projection* projection = signature->as<Projection>();
        // An invalid index may show up here for placeholder references, in which
        // case we should avoid trying to resolve the type index.
        if (params.recursionGroup != TypeDefinition::invalidIndex)
            TypeInformation::get(params.recursionGroup).ref();
        projection->getRecursionGroup() = params.recursionGroup;
        projection->getIndex() = params.index;

        entry.key = WTFMove(signature);
    }
};

struct SubtypeParameterTypes {
    TypeIndex superType;
    TypeIndex underlyingType;

    static unsigned hash(const SubtypeParameterTypes& params)
    {
        return computeSubtypeHash(params.superType, params.underlyingType);
    }

    static bool equal(const TypeHash& sig, const SubtypeParameterTypes& params)
    {
        if (!sig.key->is<Subtype>())
            return false;

        const Subtype* subtype = sig.key->as<Subtype>();
        if (subtype->superType() != params.superType)
            return false;

        if (subtype->underlyingType() != params.underlyingType)
            return false;

        return true;
    }

    static void translate(TypeHash& entry, const SubtypeParameterTypes& params, unsigned)
    {
        RefPtr<TypeDefinition> signature = TypeDefinition::tryCreateSubtype();
        RELEASE_ASSERT(signature);

        Subtype* subtype = signature->as<Subtype>();
        subtype->getSuperType() = params.superType;
        subtype->getUnderlyingType() = params.underlyingType;

        TypeInformation::get(params.superType).ref();
        TypeInformation::get(params.underlyingType).ref();

        entry.key = WTFMove(signature);
    }
};

TypeInformation::TypeInformation()
{
#define MAKE_THUNK_SIGNATURE(type, enc, str, val, ...) \
    do { \
        if (TypeKind::type != TypeKind::Void) { \
            RefPtr<TypeDefinition> sig = TypeDefinition::tryCreateFunctionSignature(1, 0); \
            sig->ref();                                                                    \
            sig->as<FunctionSignature>()->getReturnType(0) = Types::type;                  \
            if (Types::type.isV128())                                                      \
                sig->as<FunctionSignature>()->setArgumentsOrResultsIncludeV128(true);      \
            thunkTypes[linearizeType(TypeKind::type)] = sig.get();                         \
            m_typeSet.add(TypeHash { sig.releaseNonNull() });                              \
        }                                                                                  \
    } while (false);

    FOR_EACH_WASM_TYPE(MAKE_THUNK_SIGNATURE);

    // Make Void again because we don't use the one that has void in it.
    {
        RefPtr<TypeDefinition> sig = TypeDefinition::tryCreateFunctionSignature(0, 0);
        sig->ref();
        thunkTypes[linearizeType(TypeKind::Void)] = sig.get();
        m_typeSet.add(TypeHash { sig.releaseNonNull() });
    }
    m_I64_Void = m_typeSet.template add<FunctionParameterTypes>(FunctionParameterTypes { { Wasm::Types::I64 }, { } }).iterator->key;
    m_Void_I32 = m_typeSet.template add<FunctionParameterTypes>(FunctionParameterTypes { { }, { Wasm::Types::I32 } }).iterator->key;
    m_Void_I32I32I32 = m_typeSet.template add<FunctionParameterTypes>(FunctionParameterTypes { { }, { Wasm::Types::I32, Wasm::Types::I32, Wasm::Types::I32 } }).iterator->key;
    m_Void_I32I32I32I32 = m_typeSet.template add<FunctionParameterTypes>(FunctionParameterTypes { { }, { Wasm::Types::I32, Wasm::Types::I32, Wasm::Types::I32, Wasm::Types::I32 } }).iterator->key;
    m_Void_I32I32I32I32I32 = m_typeSet.template add<FunctionParameterTypes>(FunctionParameterTypes { { }, { Wasm::Types::I32, Wasm::Types::I32, Wasm::Types::I32, Wasm::Types::I32, Wasm::Types::I32 } }).iterator->key;
    m_I32_I32 = m_typeSet.template add<FunctionParameterTypes>(FunctionParameterTypes { { Wasm::Types::I32 }, { Wasm::Types::I32 } }).iterator->key;
}

RefPtr<TypeDefinition> TypeInformation::typeDefinitionForFunction(const Vector<Type, 1>& results, const Vector<Type>& args)
{
    if constexpr (ASSERT_ENABLED) {
        ASSERT(!results.contains(Wasm::Types::Void));
        ASSERT(!args.contains(Wasm::Types::Void));
    }
    TypeInformation& info = singleton();
    Locker locker { info.m_lock };

    auto addResult = info.m_typeSet.template add<FunctionParameterTypes>(FunctionParameterTypes { results, args });
    return addResult.iterator->key;
}

RefPtr<TypeDefinition> TypeInformation::typeDefinitionForStruct(const Vector<FieldType>& fields)
{
    TypeInformation& info = singleton();
    Locker locker { info.m_lock };

    auto addResult = info.m_typeSet.template add<StructParameterTypes>(StructParameterTypes { fields });
    return addResult.iterator->key;
}

RefPtr<TypeDefinition> TypeInformation::typeDefinitionForArray(FieldType elementType)
{
    TypeInformation& info = singleton();
    Locker locker { info.m_lock };

    auto addResult = info.m_typeSet.template add<ArrayParameterTypes>(ArrayParameterTypes { elementType });
    return addResult.iterator->key;
}

RefPtr<TypeDefinition> TypeInformation::typeDefinitionForRecursionGroup(const Vector<TypeIndex>& types)
{
    TypeInformation& info = singleton();
    Locker locker { info.m_lock };

    auto addResult = info.m_typeSet.template add<RecursionGroupParameterTypes>(RecursionGroupParameterTypes { types });
    return addResult.iterator->key;
}

RefPtr<TypeDefinition> TypeInformation::typeDefinitionForProjection(TypeIndex recursionGroup, ProjectionIndex index)
{
    TypeInformation& info = singleton();
    Locker locker { info.m_lock };

    auto addResult = info.m_typeSet.template add<ProjectionParameterTypes>(ProjectionParameterTypes { recursionGroup, index });
    return addResult.iterator->key;
}

RefPtr<TypeDefinition> TypeInformation::typeDefinitionForSubtype(TypeIndex superType, TypeIndex underlyingType)
{
    TypeInformation& info = singleton();
    Locker locker { info.m_lock };

    auto addResult = info.m_typeSet.template add<SubtypeParameterTypes>(SubtypeParameterTypes { superType, underlyingType });
    return addResult.iterator->key;
}

void TypeInformation::addCachedUnrolling(TypeIndex type, TypeIndex unrolled)
{
    TypeInformation& info = singleton();
    Locker locker { info.m_lock };

    info.m_unrollingCache.add(type, unrolled);
}

std::optional<TypeIndex> TypeInformation::tryGetCachedUnrolling(TypeIndex type)
{
    TypeInformation& info = singleton();
    Locker locker { info.m_lock };

    const auto iterator = info.m_unrollingCache.find(type);
    if (iterator == info.m_unrollingCache.end())
        return std::nullopt;
    return std::optional<TypeIndex>(iterator->value);
}

void TypeInformation::registerCanonicalRTTForType(TypeIndex type)
{
    TypeInformation& info = singleton();

    auto registered = tryGetCanonicalRTT(type);

    if (!registered.has_value()) {
        RefPtr<RTT> rtt = TypeInformation::canonicalRTTForType(type);
        {
            Locker locker { info.m_lock };
            info.m_rttMap.add(type, rtt.releaseNonNull());
        }
    }
}

RefPtr<RTT> TypeInformation::canonicalRTTForType(TypeIndex type)
{
    const TypeDefinition& signature = TypeInformation::get(type).unroll();
    RefPtr<RTT> protector = nullptr;

    if (signature.is<Subtype>()) {
        auto superRTT = TypeInformation::tryGetCanonicalRTT(signature.as<Subtype>()->superType());
        ASSERT(superRTT.has_value());
        DisplayCount displaySize = superRTT.value()->displaySize() + 1;

        protector = RTT::tryCreateRTT(displaySize);
        RELEASE_ASSERT(protector);

        protector->setDisplayEntry(0, superRTT.value());
        for (DisplayCount i = 1; i < displaySize; i++)
            protector->setDisplayEntry(i, superRTT.value()->displayEntry(i - 1));

        return protector;
    }

    protector = RTT::tryCreateRTT(0);
    RELEASE_ASSERT(protector);
    return protector;
}

std::optional<const RTT*> TypeInformation::tryGetCanonicalRTT(TypeIndex type)
{
    TypeInformation& info = singleton();
    Locker locker { info.m_lock };

    const auto iterator = info.m_rttMap.find(type);
    if (iterator == info.m_rttMap.end())
        return std::nullopt;
    return std::optional<const RTT*>(iterator->value.get());
}

void TypeInformation::tryCleanup()
{
    TypeInformation& info = singleton();
    Locker locker { info.m_lock };

    info.m_unrollingCache.removeIf([&] (auto& keyValuePair) {
        const TypeDefinition& type = TypeInformation::get(keyValuePair.key);
        return type.refCount() == 1;
    });

    info.m_rttMap.removeIf([&] (auto& keyValuePair) {
        const TypeDefinition& type = TypeInformation::get(keyValuePair.key);
        return type.refCount() == 1;
    });

    info.m_typeSet.removeIf([&] (auto& hash) {
        const auto& signature = hash.key;
        const bool shouldRemove = signature->refCount() == 1;
        if (shouldRemove)
            signature->cleanup();
        return shouldRemove;
    });
}

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
