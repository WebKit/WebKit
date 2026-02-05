/*
 * Copyright (C) 2016-2023 Apple Inc. All rights reserved.
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

#include "JSCJSValueInlines.h"
#include "JSWebAssemblyArray.h"
#include "JSWebAssemblyException.h"
#include "JSWebAssemblyStruct.h"
#include "WasmCallee.h"
#include "WasmFormat.h"
#include "WasmTypeDefinitionInlines.h"
#include "WebAssemblyFunctionBase.h"
#include <wtf/CommaPrinter.h>
#include <wtf/FastMalloc.h>
#include <wtf/StringPrintStream.h>
#include <wtf/TZoneMallocInlines.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC { namespace Wasm {

WTF_MAKE_TZONE_ALLOCATED_IMPL(TypeInformation);

String TypeDefinition::toString() const
{
    return WTF::toString(*this);
}

void TypeDefinition::dump(PrintStream& out) const
{
    switch (m_kind) {
    case FunctionSignature::kind:
        return as<FunctionSignature>()->dump(out);
    case StructType::kind:
        return as<StructType>()->dump(out);
    case ArrayType::kind:
        return as<ArrayType>()->dump(out);
    case RecursionGroup::kind:
        return as<RecursionGroup>()->dump(out);
    case Projection::kind:
        return as<Projection>()->dump(out);
    case Subtype::kind:
        return as<Subtype>()->dump(out);
    }
    RELEASE_ASSERT_NOT_REACHED();
    return;
}

String FunctionSignature::toString() const
{
    return WTF::toString(*this);
}

void FunctionSignature::dump(PrintStream& out) const
{
    {
        out.print("("_s);
        CommaPrinter comma;
        for (FunctionArgCount arg = 0; arg < argumentCount(); ++arg)
            out.print(comma, makeString(argumentType(arg).kind));
        out.print(")"_s);
    }

    {
        CommaPrinter comma;
        out.print(" -> ["_s);
        for (FunctionArgCount ret = 0; ret < returnCount(); ++ret)
            out.print(comma, makeString(returnType(ret).kind));
        out.print("]"_s);
    }
}

FunctionSignature::FunctionSignature(FunctionArgCount argumentCount, FunctionArgCount returnCount)
    : TypeDefinition(kind)
    , m_argCount(argumentCount)
    , m_retCount(returnCount)
{ }

FunctionSignature::~FunctionSignature()
{
}

String StructType::toString() const
{
    return WTF::toString(*this);
}

void StructType::dump(PrintStream& out) const
{
    out.print("("_s);
    CommaPrinter comma;
    for (StructFieldCount fieldIndex = 0; fieldIndex < fieldCount(); ++fieldIndex)
        out.print(comma, field(fieldIndex).mutability ? "immutable "_s : "mutable "_s, makeString(field(fieldIndex).type));
    out.print(")"_s);
}

StructType::StructType(std::span<const FieldType> fieldTypes)
    : TypeDefinition(kind)
    , m_fieldCount(fieldTypes.size())
{
    unsigned currentFieldOffset = 0;
    auto fields = mutableFields();
    for (unsigned fieldIndex = 0; fieldIndex < fieldTypes.size(); ++fieldIndex) {
        const auto& fieldType = fieldTypes[fieldIndex];
        m_hasRefFieldTypes |= isRefType(fieldType.type);
        m_hasRecursiveReference |= isRefWithRecursiveReference(fieldType.type);
        new (&fields[fieldIndex]) FieldType(fieldType);
        const auto& fieldStorageType = fields[fieldIndex].type;
        currentFieldOffset = WTF::roundUpToMultipleOf(typeAlignmentInBytes(fieldStorageType), currentFieldOffset);
        fieldOffsetFromInstancePayload(fieldIndex) = currentFieldOffset;
        currentFieldOffset += typeSizeInBytes(fieldStorageType);
    }

    m_instancePayloadSize = WTF::roundUpToMultipleOf<sizeof(uint64_t)>(currentFieldOffset);
}

ArrayType::ArrayType(const FieldType& elementType)
    : TypeDefinition(kind)
    , m_hasRecursiveReference(isRefWithRecursiveReference(elementType.type))
    , m_elementType(elementType)
{
}

String ArrayType::toString() const
{
    return WTF::toString(*this);
}

void ArrayType::dump(PrintStream& out) const
{
    out.print("("_s);
    CommaPrinter comma;
    out.print(comma, elementType().mutability ? "immutable "_s : "mutable "_s, makeString(elementType().type));
    out.print(")"_s);
}

RecursionGroup::RecursionGroup(std::span<const TypeIndex> types)
    : TypeDefinition(kind)
    , m_typeCount(types.size())
{
    for (unsigned i = 0; i < types.size(); ++i) {
        TypeInformation::get(types[i]).ref();
        mutableTypes()[i] = types[i];
    }
}


String RecursionGroup::toString() const
{
    return WTF::toString(*this);
}

void RecursionGroup::dump(PrintStream& out) const
{
    out.print("("_s);
    CommaPrinter comma;
    for (RecursionGroupCount typeIndex = 0; typeIndex < typeCount(); ++typeIndex) {
        out.print(comma);
        TypeInformation::get(type(typeIndex)).dump(out);
    }
    out.print(")"_s);
}

Projection::Projection(TypeIndex recursionGroup, ProjectionIndex projectionIndex)
    : TypeDefinition(kind)
    , m_recursionGroup(recursionGroup)
    , m_projectionIndex(projectionIndex)
{
    // An invalid index may show up here for placeholder references, in which
    // case we should avoid trying to resolve the type index.
    if (recursionGroup != TypeDefinition::invalidIndex)
        TypeInformation::get(recursionGroup).ref();
}


String Projection::toString() const
{
    return WTF::toString(*this);
}

void Projection::dump(PrintStream& out) const
{
    out.print("("_s);
    CommaPrinter comma;
    if (isPlaceholder())
        out.print("<current-rec-group>"_s);
    else
        TypeInformation::get(recursionGroup()).dump(out);
    out.print("."_s, projectionIndex());
    out.print(")"_s);
}

Subtype::Subtype(std::span<const TypeIndex> superTypes, TypeIndex underlyingType, bool isFinal)
    : TypeDefinition(kind)
    , m_final(isFinal)
    , m_underlyingType(underlyingType)
    , m_supertypeCount(superTypes.size())
{
    for (SupertypeCount i = 0; i < superTypes.size(); i++) {
        mutableSuperTypes()[i] = superTypes[i];
        TypeInformation::get(superTypes[i]).ref();
    }
    TypeInformation::get(underlyingType).ref();
}

String Subtype::toString() const
{
    return WTF::toString(*this);
}

void Subtype::dump(PrintStream& out) const
{
    out.print("("_s);
    CommaPrinter comma;
    if (supertypeCount() > 0) {
        TypeInformation::get(firstSuperType()).dump(out);
        out.print(comma);
    }
    TypeInformation::get(underlyingType()).dump(out);
    out.print(")"_s);
}

void StorageType::dump(PrintStream& out) const
{
    if (is<Type>())
        out.print(makeString(as<Type>().kind));
    else {
        ASSERT(is<PackedType>());
        out.print(makeString(as<PackedType>()));
    }
}

bool TypeDefinition::cleanup()
{
    // Only compound type definitions need to be cleaned up, not, e.g., function types.
    if (is<Subtype>())
        return as<Subtype>()->cleanup();
    if (is<Projection>())
        return as<Projection>()->cleanup();
    if (is<RecursionGroup>())
        return as<RecursionGroup>()->cleanup();
    return false;
}

bool Subtype::cleanup()
{
    for (auto& type : superTypes())
        TypeInformation::get(type).deref();
    TypeInformation::get(m_underlyingType).deref();
    return true;
}

bool Projection::cleanup()
{
    if (recursionGroup() != TypeDefinition::invalidIndex) {
        TypeInformation::get(recursionGroup()).deref();
        return true;
    }
    return false;
}

bool RecursionGroup::cleanup()
{
    for (auto& type : types())
        TypeInformation::get(type).deref();
    return true;
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

static unsigned computeStructTypeHash(std::span<const FieldType> fields)
{
    unsigned accumulator = 0x15d2546;
    for (auto& field : fields) {
        accumulator = WTF::pairIntHash(accumulator, WTF::IntHash<int8_t>::hash(static_cast<int8_t>(field.type.typeCode())));
        accumulator = WTF::pairIntHash(accumulator, WTF::IntHash<uint8_t>::hash(static_cast<uint8_t>(field.type.index())));
        accumulator = WTF::pairIntHash(accumulator, WTF::IntHash<uint8_t>::hash(static_cast<uint8_t>(field.mutability)));
    }
    return accumulator;
}

static unsigned computeArrayTypeHash(FieldType elementType)
{
    unsigned accumulator = 0x7835ab;
    accumulator = WTF::pairIntHash(accumulator, WTF::IntHash<int8_t>::hash(static_cast<int8_t>(elementType.type.typeCode())));
    accumulator = WTF::pairIntHash(accumulator, WTF::IntHash<TypeIndex>::hash(elementType.type.index()));
    accumulator = WTF::pairIntHash(accumulator, WTF::IntHash<uint8_t>::hash(static_cast<uint8_t>(elementType.mutability)));
    return accumulator;
}

static unsigned computeRecursionGroupHash(std::span<const TypeIndex> types)
{
    unsigned accumulator = 0x9cfb89bb;
    for (auto& type : types)
        accumulator = WTF::pairIntHash(accumulator, WTF::IntHash<TypeIndex>::hash(type));
    return accumulator;
}

static unsigned computeProjectionHash(TypeIndex recursionGroup, ProjectionIndex projectionIndex)
{
    unsigned accumulator = 0xbeae6d4e;
    accumulator = WTF::pairIntHash(accumulator, WTF::IntHash<TypeIndex>::hash(recursionGroup));
    accumulator = WTF::pairIntHash(accumulator, WTF::IntHash<ProjectionIndex>::hash(projectionIndex));
    return accumulator;
}

static unsigned computeSubtypeHash(std::span<const TypeIndex> superTypes, TypeIndex underlyingType, bool isFinal)
{
    unsigned accumulator = 0x3efa01b9;
    for (auto& type : superTypes)
        accumulator = WTF::pairIntHash(accumulator, WTF::IntHash<TypeIndex>::hash(type));
    accumulator = WTF::pairIntHash(accumulator, WTF::IntHash<TypeIndex>::hash(underlyingType));
    accumulator = WTF::pairIntHash(accumulator, WTF::IntHash<bool>::hash(isFinal));
    return accumulator;
}

unsigned TypeDefinition::hash() const
{
    switch (m_kind) {
    case FunctionSignature::kind: {
        const FunctionSignature* signature = as<FunctionSignature>();
        return computeSignatureHash(signature->returnCount(), signature->storage(0), signature->argumentCount(), signature->storage(signature->returnCount()));
    }
    case StructType::kind: {
        const StructType* structType = as<StructType>();
        return computeStructTypeHash(structType->fields());
    }
    case ArrayType::kind: {
        const ArrayType* arrayType = as<ArrayType>();
        return computeArrayTypeHash(arrayType->elementType());
    }
    case RecursionGroup::kind: {
        const RecursionGroup* recursionGroup = as<RecursionGroup>();
        return computeRecursionGroupHash(recursionGroup->types());
    }
    case Projection::kind: {
        const Projection* projection = as<Projection>();
        return computeProjectionHash(projection->recursionGroup(), projection->projectionIndex());
    }
    case Subtype::kind: {
        const Subtype* subtype = as<Subtype>();
        return computeSubtypeHash(subtype->superTypes(), subtype->underlyingType(), subtype->isFinal());
    }
    }
    RELEASE_ASSERT_NOT_REACHED();
    return 0;
}

RefPtr<FunctionSignature> FunctionSignature::tryCreate(FunctionArgCount returnCount, FunctionArgCount argumentCount)
{
    // We use WTF_MAKE_TZONE_ALLOCATED for this class.
    auto result = tryFastMalloc(allocationSize(returnCount, argumentCount));
    void* memory = nullptr;
    if (!result.getValue(memory))
        return nullptr;
    return adoptRef(*new (NotNull, memory) FunctionSignature(argumentCount, returnCount));
}

RefPtr<StructType> StructType::tryCreate(std::span<const FieldType> fields)
{
    // We use WTF_MAKE_TZONE_ALLOCATED for this class.
    auto result = tryFastMalloc(allocationSize(fields.size()));
    void* memory = nullptr;
    if (!result.getValue(memory))
        return nullptr;
    return adoptRef(*new (NotNull, memory) StructType(fields));
}

RefPtr<ArrayType> ArrayType::tryCreate(const FieldType& elementType)
{
    // We use WTF_MAKE_TZONE_ALLOCATED for this class.
    auto result = tryFastMalloc(allocationSize());
    void* memory = nullptr;
    if (!result.getValue(memory))
        return nullptr;
    return adoptRef(*new (NotNull, memory) ArrayType(elementType));
}

RefPtr<RecursionGroup> RecursionGroup::tryCreate(std::span<const TypeIndex> types)
{
    // We use WTF_MAKE_TZONE_ALLOCATED for this class.
    auto result = tryFastMalloc(allocationSize(types.size()));
    void* memory = nullptr;
    if (!result.getValue(memory))
        return nullptr;
    return adoptRef(*new (NotNull, memory) RecursionGroup(types));
}

RefPtr<Projection> Projection::tryCreate(TypeIndex recursionGroup, ProjectionIndex index)
{
    // We use WTF_MAKE_TZONE_ALLOCATED for this class.
    auto result = tryFastMalloc(allocationSize());
    void* memory = nullptr;
    if (!result.getValue(memory))
        return nullptr;
    return adoptRef(*new (NotNull, memory) Projection(recursionGroup, index));
}

RefPtr<Subtype> Subtype::tryCreate(std::span<const TypeIndex> superTypes, TypeIndex underlyingType, bool isFinal)
{
    // We use WTF_MAKE_TZONE_ALLOCATED for this class.
    auto result = tryFastMalloc(allocationSize(superTypes.size()));
    void* memory = nullptr;
    if (!result.getValue(memory))
        return nullptr;
    return adoptRef(*new (NotNull, memory) Subtype(superTypes, underlyingType, isFinal));
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
    if (isRefWithTypeIndex(type)) {
        auto& candidate = TypeInformation::get(type.index);
        if (candidate.is<Projection>()) {
            const Projection* projection = candidate.as<Projection>();
            if (projection->isPlaceholder()) {
                auto newProjection = TypeInformation::typeDefinitionForProjection(projectee, projection->projectionIndex());
                TypeKind kind = type.isNullable() ? TypeKind::RefNull : TypeKind::Ref;
                // Calling module must have already taken ownership of all projections.
                RELEASE_ASSERT(newProjection->refCount() > 2); // TypeInformation registry + RefPtr + owning module(s)
                return Type { kind, newProjection->index() };
            }
        }
    }

    return type;
}

// Perform a substitution as above but for a Subtype's parent type.
static TypeIndex substituteParent(TypeIndex parent, TypeIndex projectee)
{
    auto& candidate = TypeInformation::get(parent);
    if (candidate.is<Projection>()) {
        const Projection* projection = candidate.as<Projection>();
        if (projection->isPlaceholder()) {
            auto newProjection = TypeInformation::typeDefinitionForProjection(projectee, projection->projectionIndex());
            // Caller module must have already taken ownership of all its projections.
            RELEASE_ASSERT(newProjection->refCount() > 2); // tbl + RefPtr + owning module(s)
            return newProjection->index();
        }
    }

    return parent;
}

// This operation is a helper for expand() that calls substitute() in order
// to replace placeholder recursive references in structural types.
Ref<const TypeDefinition> TypeDefinition::replacePlaceholders(TypeIndex projectee) const
{
    if (is<FunctionSignature>()) {
        const FunctionSignature* func = as<FunctionSignature>();
        Vector<Type, 16> newArguments(func->argumentCount(), [&](size_t i) {
            return substitute(func->argumentType(i), projectee);
        });
        Vector<Type, 16> newReturns(func->returnCount(), [&](size_t i) {
            return substitute(func->returnType(i), projectee);
        });

        auto def = TypeInformation::typeDefinitionForFunction(newReturns, newArguments);
        return def.releaseNonNull();
    }

    if (is<StructType>()) {
        const StructType* structType = as<StructType>();
        Vector<FieldType> newFields(structType->fieldCount(), [&](size_t i) {
            FieldType field = structType->field(i);
            StorageType substituted = field.type.is<PackedType>() ? field.type : StorageType(substitute(field.type.as<Type>(), projectee));
            return FieldType { substituted, field.mutability };
        });

        auto def = TypeInformation::typeDefinitionForStruct(newFields);
        return def.releaseNonNull();
    }

    if (is<ArrayType>()) {
        const ArrayType* arrayType = as<ArrayType>();
        FieldType field = arrayType->elementType();
        StorageType substituted = field.type.is<PackedType>() ? field.type : StorageType(substitute(field.type.as<Type>(), projectee));
        auto def = TypeInformation::typeDefinitionForArray(FieldType { substituted, field.mutability });
        return def.releaseNonNull();
    }

    if (is<Subtype>()) {
        const Subtype* subtype = as<Subtype>();
        Ref newUnderlyingType = TypeInformation::get(subtype->underlyingType()).replacePlaceholders(projectee);
        Vector<TypeIndex> supertypes(subtype->supertypeCount(), [&](size_t i) {
            return substituteParent(subtype->superType(i), projectee);
        });
        // Subtype takes ownership of newUnderlyingType.
        auto def = TypeInformation::typeDefinitionForSubtype(supertypes, newUnderlyingType->index(), subtype->isFinal());
        return def.releaseNonNull();
    }

    return Ref { *this };
}

// This function corresponds to the unroll metafunction from the spec:
//
//  https://github.com/WebAssembly/gc/blob/main/proposals/gc/MVP.md#auxiliary-definitions
//
// It unrolls a potentially recursive type to a Subtype or structural type.
const TypeDefinition& TypeDefinition::unrollSlow() const
{
    ASSERT(is<Projection>());
    const Projection& projection = *as<Projection>();
    const TypeDefinition& projectee = TypeInformation::get(projection.recursionGroup());

    const RecursionGroup& recursionGroup = *projectee.as<RecursionGroup>();
    const TypeDefinition& underlyingType = TypeInformation::get(recursionGroup.type(projection.projectionIndex()));

    if (underlyingType.hasRecursiveReference()) {
        if (std::optional<TypeIndex> cachedUnrolling = TypeInformation::tryGetCachedUnrolling(index()))
            return TypeInformation::get(*cachedUnrolling);

        Ref unrolled = underlyingType.replacePlaceholders(projectee.index());
        TypeInformation::addCachedUnrolling(index(), unrolled);
        RELEASE_ASSERT(unrolled->refCount() > 2); // TypeInformation registry + Ref + owner (unrolling cache).
        return unrolled.unsafeGet(); // TypeInformation unrolling cache now owns, with lifetime tied to 'this'.
    }
    RELEASE_ASSERT(underlyingType.refCount() > 1); // TypeInformation registry + owner(s).
    return underlyingType;
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
    if (as<Subtype>()->supertypeCount() > 0) {
        const TypeDefinition& supertype = TypeInformation::get(as<Subtype>()->firstSuperType());
        const bool hasRecGroupSupertype = supertype.is<Projection>() && supertype.as<Projection>()->isPlaceholder();
        return hasRecGroupSupertype || TypeInformation::get(as<Subtype>()->underlyingType()).hasRecursiveReference();
    }

    return TypeInformation::get(as<Subtype>()->underlyingType()).hasRecursiveReference();
}

bool TypeDefinition::isFinalType() const
{
    const auto& unrolled = unroll();
    if (unrolled.is<Subtype>())
        return unrolled.as<Subtype>()->isFinal();

    return true;
}

RTT::RTT(RTTKind kind, bool isFinalType, StructFieldCount fieldCount)
    : TrailingArrayType(std::max(1u, inlinedDisplaySize))
    , m_kind(kind)
    , m_isFinalType(isFinalType)
    , m_displaySizeExcludingThis(0)
    , m_fieldCount(fieldCount)
{
    at(0) = this;
}

RTT::RTT(RTTKind kind, const RTT& supertype, bool isFinalType, StructFieldCount fieldCount)
    : TrailingArrayType(std::max(supertype.displaySizeExcludingThis() + 2, inlinedDisplaySize))
    , m_kind(kind)
    , m_isFinalType(isFinalType)
    , m_displaySizeExcludingThis(supertype.displaySizeExcludingThis() + 1)
    , m_fieldCount(fieldCount)
{
    unsigned actualDisplaySize = supertype.displaySizeExcludingThis() + 2;
    ASSERT(actualDisplaySize == (m_displaySizeExcludingThis + 1));
    for (size_t i = 0; i < actualDisplaySize - 1; ++i)
        span()[i] = supertype.span()[i];
    at(m_displaySizeExcludingThis) = this;
}

RefPtr<RTT> RTT::tryCreate(RTTKind kind, bool isFinalType, StructFieldCount fieldCount)
{
    auto result = tryFastMalloc(allocationSize(std::max(/* itself */ 1u, inlinedDisplaySize)));
    void* memory = nullptr;
    if (!result.getValue(memory))
        return nullptr;
    return adoptRef(new (NotNull, memory) RTT(kind, isFinalType, fieldCount));
}

RefPtr<RTT> RTT::tryCreate(RTTKind kind, const RTT& supertype, bool isFinalType, StructFieldCount fieldCount)
{
    unsigned allocationCount = std::max(supertype.displaySizeExcludingThis() + 2, inlinedDisplaySize);
    auto result = tryFastMalloc(allocationSize(allocationCount));
    void* memory = nullptr;
    if (!result.getValue(memory))
        return nullptr;
    return adoptRef(new (NotNull, memory) RTT(kind, supertype, isFinalType, fieldCount));
}

bool RTT::isSubRTT(const RTT& parent) const
{
    if (this == &parent)
        return true;
    if (displaySizeExcludingThis() < parent.displaySizeExcludingThis())
        return false;
    return &parent == displayEntry(parent.displaySizeExcludingThis());
}

bool RTT::isStrictSubRTT(const RTT& parent) const
{
    if (displaySizeExcludingThis() <= parent.displaySizeExcludingThis())
        return false;
    return &parent == displayEntry(parent.displaySizeExcludingThis());
}

const FunctionSignature& TypeInformation::signatureForJSException()
{
    return *singleton().m_Void_Externref;
}

struct FunctionParameterTypes {
    const Vector<Type, 16>& returnTypes;
    const Vector<Type, 16>& argumentTypes;

    static unsigned hash(const FunctionParameterTypes& params)
    {
        return computeSignatureHash(params.returnTypes.size(), params.returnTypes.span().data(), params.argumentTypes.size(), params.argumentTypes.span().data());
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
        auto signature = FunctionSignature::tryCreate(params.returnTypes.size(), params.argumentTypes.size());
        RELEASE_ASSERT(signature);
        bool hasRecursiveReference = false;
        bool argumentsOrResultsIncludeI64 = false;
        bool argumentsOrResultsIncludeV128 = false;
        bool argumentsOrResultsIncludeExnref = false;

        for (unsigned i = 0; i < params.returnTypes.size(); ++i) {
            signature->getReturnType(i) = params.returnTypes[i];
            hasRecursiveReference |= isRefWithRecursiveReference(params.returnTypes[i]);
            argumentsOrResultsIncludeI64 |= params.returnTypes[i].isI64();
            argumentsOrResultsIncludeV128 |= params.returnTypes[i].isV128();
            argumentsOrResultsIncludeExnref |= isExnref(params.returnTypes[i]);
        }

        for (unsigned i = 0; i < params.argumentTypes.size(); ++i) {
            signature->getArgumentType(i) = params.argumentTypes[i];
            hasRecursiveReference |= isRefWithRecursiveReference(params.argumentTypes[i]);
            argumentsOrResultsIncludeI64 |= params.argumentTypes[i].isI64();
            argumentsOrResultsIncludeV128 |= params.argumentTypes[i].isV128();
            argumentsOrResultsIncludeExnref |= isExnref(params.argumentTypes[i]);
        }

        signature->setHasRecursiveReference(hasRecursiveReference);
        signature->setArgumentsOrResultsIncludeI64(argumentsOrResultsIncludeI64);
        signature->setArgumentsOrResultsIncludeV128(argumentsOrResultsIncludeV128);
        signature->setArgumentsOrResultsIncludeExnref(argumentsOrResultsIncludeExnref);

        entry.key = WTF::move(signature);
    }
};

struct StructParameterTypes {
    const Vector<FieldType>& fields;

    static unsigned hash(const StructParameterTypes& params)
    {
        return computeStructTypeHash(params.fields.span());
    }

    static bool equal(const TypeHash& sig, const StructParameterTypes& params)
    {
        if (!sig.key->is<StructType>())
            return false;

        auto structType = sig.key->as<StructType>();
        auto fields = structType->fields();

        if (fields.size() != params.fields.size())
            return false;

        for (size_t i = 0; i < fields.size(); ++i) {
            if (fields[i] != params.fields[i])
                return false;
        }

        return true;
    }

    static void translate(TypeHash& entry, const StructParameterTypes& params, unsigned)
    {
        auto signature = StructType::tryCreate(params.fields.span());
        RELEASE_ASSERT(signature);
        entry.key = WTF::move(signature);
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
        auto signature = ArrayType::tryCreate(params.elementType);
        RELEASE_ASSERT(signature);
        entry.key = WTF::move(signature);
    }
};

struct RecursionGroupParameterTypes {
    const Vector<TypeIndex>& types;

    static unsigned hash(const RecursionGroupParameterTypes& params)
    {
        return computeRecursionGroupHash(params.types.span());
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
        auto signature = RecursionGroup::tryCreate(params.types.span());
        RELEASE_ASSERT(signature);
        entry.key = WTF::move(signature);
    }
};

struct ProjectionParameterTypes {
    const TypeIndex recursionGroup;
    const ProjectionIndex projectionIndex;

    static unsigned hash(const ProjectionParameterTypes& params)
    {
        return computeProjectionHash(params.recursionGroup, params.projectionIndex);
    }

    static bool equal(const TypeHash& sig, const ProjectionParameterTypes& params)
    {
        if (!sig.key->is<Projection>())
            return false;

        const Projection* projection = sig.key->as<Projection>();
        if (projection->recursionGroup() != params.recursionGroup || projection->projectionIndex() != params.projectionIndex)
            return false;

        return true;
    }

    static void translate(TypeHash& entry, const ProjectionParameterTypes& params, unsigned)
    {
        auto projection = Projection::tryCreate(params.recursionGroup, params.projectionIndex);
        RELEASE_ASSERT(projection);
        entry.key = WTF::move(projection);
    }
};

struct SubtypeParameterTypes {
    const Vector<TypeIndex>& superTypes;
    TypeIndex underlyingType;
    bool isFinal;

    static unsigned hash(const SubtypeParameterTypes& params)
    {
        return computeSubtypeHash(params.superTypes.span(), params.underlyingType, params.isFinal);
    }

    static bool equal(const TypeHash& sig, const SubtypeParameterTypes& params)
    {
        if (!sig.key->is<Subtype>())
            return false;

        const Subtype* subtype = sig.key->as<Subtype>();
        if (subtype->supertypeCount() != params.superTypes.size())
            return false;

        for (SupertypeCount i = 0; i < params.superTypes.size(); i++) {
            if (subtype->superType(i) != params.superTypes[i])
                return false;
        }

        if (subtype->underlyingType() != params.underlyingType)
            return false;

        if (subtype->isFinal() != params.isFinal)
            return false;

        return true;
    }

    static void translate(TypeHash& entry, const SubtypeParameterTypes& params, unsigned)
    {
        auto signature = Subtype::tryCreate(params.superTypes.span(), params.underlyingType, params.isFinal);
        RELEASE_ASSERT(signature);
        entry.key = WTF::move(signature);
    }
};

TypeInformation::TypeInformation()
{
#define MAKE_THUNK_SIGNATURE(type, enc, str, val, ...) \
    do { \
        if (TypeKind::type != TypeKind::Void) { \
            auto sig = FunctionSignature::tryCreate(1, 0);                                 \
            RELEASE_ASSERT(sig);                                                           \
            sig->ref();                                                                    \
            sig->getReturnType(0) = Types::type;                                           \
            if (Types::type.isI64())                                                       \
                sig->setArgumentsOrResultsIncludeI64(true);                                \
            if (Types::type.isV128())                                                      \
                sig->setArgumentsOrResultsIncludeV128(true);                               \
            if (isExnref(Types::type))                                                     \
                sig->setArgumentsOrResultsIncludeExnref(true);                             \
            thunkTypes[linearizeType(TypeKind::type)] = sig.get();                         \
            m_typeSet.add(TypeHash { sig.releaseNonNull() });                              \
        }                                                                                  \
    } while (false);

    FOR_EACH_WASM_TYPE(MAKE_THUNK_SIGNATURE);

    // Make Void again because we don't use the one that has void in it.
    {
        auto sig = FunctionSignature::tryCreate(0, 0);
        sig->ref();
        thunkTypes[linearizeType(TypeKind::Void)] = sig->as<FunctionSignature>();
        m_typeSet.add(TypeHash { sig.releaseNonNull() });
    }
    m_I64_Void = m_typeSet.template add<FunctionParameterTypes>(FunctionParameterTypes { { Wasm::Types::I64 }, { } }).iterator->key->as<FunctionSignature>();
    m_Void_I32 = m_typeSet.template add<FunctionParameterTypes>(FunctionParameterTypes { { }, { Wasm::Types::I32 } }).iterator->key->as<FunctionSignature>();
    m_Void_I32I32I32 = m_typeSet.template add<FunctionParameterTypes>(FunctionParameterTypes { { }, { Wasm::Types::I32, Wasm::Types::I32, Wasm::Types::I32 } }).iterator->key->as<FunctionSignature>();
    m_Void_I32I32I32I32 = m_typeSet.template add<FunctionParameterTypes>(FunctionParameterTypes { { }, { Wasm::Types::I32, Wasm::Types::I32, Wasm::Types::I32, Wasm::Types::I32 } }).iterator->key->as<FunctionSignature>();
    m_Void_I32I32I32I32I32 = m_typeSet.template add<FunctionParameterTypes>(FunctionParameterTypes { { }, { Wasm::Types::I32, Wasm::Types::I32, Wasm::Types::I32, Wasm::Types::I32, Wasm::Types::I32 } }).iterator->key->as<FunctionSignature>();
    m_I32_I32 = m_typeSet.template add<FunctionParameterTypes>(FunctionParameterTypes { { Wasm::Types::I32 }, { Wasm::Types::I32 } }).iterator->key->as<FunctionSignature>();
    m_I32_RefI32I32I32 = m_typeSet.template add<FunctionParameterTypes>(FunctionParameterTypes { { Wasm::Types::I32 }, { anyrefType(), Wasm::Types::I32, Wasm::Types::I32, Wasm::Types::I32 } }).iterator->key->as<FunctionSignature>();
    m_Ref_RefI32I32 = m_typeSet.template add<FunctionParameterTypes>(FunctionParameterTypes { { anyrefType() }, { anyrefType(), Wasm::Types::I32, Wasm::Types::I32 } }).iterator->key->as<FunctionSignature>();
    m_Arrayref_I32I32I32I32 = m_typeSet.template add<FunctionParameterTypes>(FunctionParameterTypes { { arrayrefType(false) }, { Wasm::Types::I32, Wasm::Types::I32, Wasm::Types::I32, Wasm::Types::I32 } }).iterator->key->as<FunctionSignature>();
    m_Anyref_Externref = m_typeSet.template add<FunctionParameterTypes>(FunctionParameterTypes { { anyrefType() }, { externrefType() } }).iterator->key->as<FunctionSignature>();
    m_Void_Externref = m_typeSet.template add<FunctionParameterTypes>(FunctionParameterTypes { { }, { externrefType() } }).iterator->key->as<FunctionSignature>();
    m_Void_I32AnyrefI32I32AnyrefI32I32 = m_typeSet.template add<FunctionParameterTypes>(FunctionParameterTypes { { }, { Wasm::Types::I32, anyrefType(), Wasm::Types::I32, Wasm::Types::I32, anyrefType(), Wasm::Types::I32, Wasm::Types::I32 } }).iterator->key->as<FunctionSignature>();
    m_Void_I32AnyrefI32I32I32I32 = m_typeSet.template add<FunctionParameterTypes>(FunctionParameterTypes { { }, { Wasm::Types::I32, anyrefType(), Wasm::Types::I32, Wasm::Types::I32, Wasm::Types::I32, Wasm::Types::I32 } }).iterator->key->as<FunctionSignature>();
}

RefPtr<FunctionSignature> TypeInformation::typeDefinitionForFunction(const Vector<Type, 16>& results, const Vector<Type, 16>& args)
{
    if constexpr (ASSERT_ENABLED) {
        ASSERT(!results.contains(Wasm::Types::Void));
        ASSERT(!args.contains(Wasm::Types::Void));
    }
    TypeInformation& info = singleton();
    Locker locker { info.m_lock };

    auto addResult = info.m_typeSet.template add<FunctionParameterTypes>(FunctionParameterTypes { results, args });
    return addResult.iterator->key->as<FunctionSignature>();
}

RefPtr<StructType> TypeInformation::typeDefinitionForStruct(const Vector<FieldType>& fields)
{
    TypeInformation& info = singleton();
    Locker locker { info.m_lock };

    auto addResult = info.m_typeSet.template add<StructParameterTypes>(StructParameterTypes { fields });
    return addResult.iterator->key->as<StructType>();
}

RefPtr<ArrayType> TypeInformation::typeDefinitionForArray(FieldType elementType)
{
    TypeInformation& info = singleton();
    Locker locker { info.m_lock };

    auto addResult = info.m_typeSet.template add<ArrayParameterTypes>(ArrayParameterTypes { elementType });
    return addResult.iterator->key->as<ArrayType>();
}

RefPtr<RecursionGroup> TypeInformation::typeDefinitionForRecursionGroup(const Vector<TypeIndex>& types)
{
    TypeInformation& info = singleton();
    Locker locker { info.m_lock };

    auto addResult = info.m_typeSet.template add<RecursionGroupParameterTypes>(RecursionGroupParameterTypes { types });
    return addResult.iterator->key->as<RecursionGroup>();
}

RefPtr<Projection> TypeInformation::typeDefinitionForProjection(TypeIndex recursionGroup, ProjectionIndex projectionIndex)
{
    TypeInformation& info = singleton();
    Locker locker { info.m_lock };

    auto addResult = info.m_typeSet.template add<ProjectionParameterTypes>(ProjectionParameterTypes { recursionGroup, projectionIndex });
    return addResult.iterator->key->as<Projection>();
}

RefPtr<Subtype> TypeInformation::typeDefinitionForSubtype(const Vector<TypeIndex>& superTypes, TypeIndex underlyingType, bool isFinal)
{
    TypeInformation& info = singleton();
    Locker locker { info.m_lock };

    auto addResult = info.m_typeSet.template add<SubtypeParameterTypes>(SubtypeParameterTypes { superTypes, underlyingType, isFinal });
    return addResult.iterator->key->as<Subtype>();
}

RefPtr<Projection> TypeInformation::getPlaceholderProjection(ProjectionIndex projectionIndex)
{
    TypeInformation& info = singleton();
    auto projection = typeDefinitionForProjection(Projection::PlaceholderGroup, projectionIndex);

    {
        Locker locker { info.m_lock };
        info.m_placeholders.add(projection);
    }

    return projection;
}

void TypeInformation::addCachedUnrolling(TypeIndex type, const TypeDefinition& unrolled)
{
    TypeInformation& info = singleton();
    Locker locker { info.m_lock };

    info.m_unrollingCache.add(type, RefPtr { &unrolled });
}

std::optional<TypeIndex> TypeInformation::tryGetCachedUnrolling(TypeIndex type)
{
    TypeInformation& info = singleton();
    Locker locker { info.m_lock };

    const auto iterator = info.m_unrollingCache.find(type);
    if (iterator == info.m_unrollingCache.end())
        return std::nullopt;
    return std::optional<TypeIndex>(iterator->value->index());
}

void TypeInformation::registerCanonicalRTTForType(TypeIndex type)
{
    Ref def = get(type);
    if (def->m_rtt)
        return;

    {
        Locker locker { def->m_rttLock };
        if (def->m_rtt)
            return;
        auto rtt = TypeInformation::createCanonicalRTTForType(locker, def);
        WTF::storeStoreFence(); // Make double-checked locking work.
        def->m_rtt = WTF::move(rtt);
    }
}

Ref<RTT> TypeInformation::createCanonicalRTTForType(const AbstractLocker&, const TypeDefinition& def)
{
    const TypeDefinition& signature = def.unroll();
    const TypeDefinition& expanded = signature.expand();
    bool isFinalType = signature.isFinalType();
    RTTKind kind;
    StructFieldCount fieldCount = 0;
    if (expanded.is<FunctionSignature>())
        kind = RTTKind::Function;
    else if (expanded.is<ArrayType>())
        kind = RTTKind::Array;
    else {
        kind = RTTKind::Struct;
        fieldCount = expanded.as<StructType>()->fieldCount();
    }

    if (signature.is<Subtype>() && signature.as<Subtype>()->supertypeCount() > 0) {
        Ref superTypeDef = TypeInformation::get(signature.as<Subtype>()->firstSuperType());
        auto superRTT = superTypeDef->m_rtt;
        ASSERT(superRTT);
        auto protector = RTT::tryCreate(kind, *superRTT, isFinalType, fieldCount);
        RELEASE_ASSERT(protector);
        return protector.releaseNonNull();
    }

    auto protector = RTT::tryCreate(kind, isFinalType, fieldCount);
    RELEASE_ASSERT(protector);
    return protector.releaseNonNull();
}

Ref<const RTT> TypeInformation::getCanonicalRTT(TypeIndex type)
{
    Ref def = get(type);
    auto result = def->m_rtt;
    RELEASE_ASSERT(result);
    return result.releaseNonNull();
}

bool TypeInformation::isReferenceValueAssignable(JSValue refValue, bool allowNull, TypeIndex typeIndex, const RTT* rtt)
{
    if (refValue.isNull())
        return allowNull;

    if (typeIndexIsType(typeIndex)) {
        switch (static_cast<TypeKind>(typeIndex)) {
        case TypeKind::Externref:
        case TypeKind::Anyref:
            // Casts to these types cannot fail as any value can be an externref/hostref.
            return true;
        case TypeKind::Funcref:
            return jsDynamicCast<WebAssemblyFunctionBase*>(refValue);
        case TypeKind::Eqref:
            return (refValue.isInt32() && refValue.asInt32() <= maxI31ref && refValue.asInt32() >= minI31ref) || jsDynamicCast<JSWebAssemblyArray*>(refValue) || jsDynamicCast<JSWebAssemblyStruct*>(refValue);
        case TypeKind::Exnref:
            // Exnref and Noexnref are in a different heap hierarchy
            return jsDynamicCast<JSWebAssemblyException*>(refValue);
        case TypeKind::Noexnref:
        case TypeKind::Noneref:
        case TypeKind::Nofuncref:
        case TypeKind::Noexternref:
            return false;
        case TypeKind::I31ref:
            return refValue.isInt32() && refValue.asInt32() <= maxI31ref && refValue.asInt32() >= minI31ref;
        case TypeKind::Arrayref:
            return jsDynamicCast<JSWebAssemblyArray*>(refValue);
        case TypeKind::Structref:
            return jsDynamicCast<JSWebAssemblyStruct*>(refValue);
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
        return false;
    }

    RefPtr<const RTT> signatureRTT;
    if (!rtt) {
        signatureRTT = TypeInformation::getCanonicalRTT(typeIndex);
        rtt = signatureRTT.get();
    }

    switch (rtt->kind()) {
    case RTTKind::Function: {
        WebAssemblyFunctionBase* funcRef = jsDynamicCast<WebAssemblyFunctionBase*>(refValue);
        if (!funcRef)
            return false;
        return funcRef->rtt()->isSubRTT(*rtt);
    }
    case RTTKind::Array:
    case RTTKind::Struct: {
        auto* object = jsDynamicCast<WebAssemblyGCObjectBase*>(refValue);
        if (!object)
            return false;
        return object->rtt()->isSubRTT(*rtt);
    }
    }

    RELEASE_ASSERT_NOT_REACHED();
    return false;
}

void TypeInformation::tryCleanup()
{
    TypeInformation& info = singleton();
    Locker locker { info.m_lock };

    bool changed;
    do {
        changed = false;
        info.m_typeSet.removeIf([&] (auto& hash) {
            const auto& signature = hash.key;
            if (signature->refCount() == 1) {
                TypeIndex index = signature->unownedIndex();
                info.m_unrollingCache.remove(index);
                changed |= signature->cleanup();
                return true;
            }
            return false;
        });
    } while (changed);
}

bool Type::definitelyIsCellOrNull() const
{
    if (!isRefType(*this))
        return false;

    if (typeIndexIsType(index)) {
        switch (static_cast<TypeKind>(index)) {
        case TypeKind::Funcref:
        case TypeKind::Arrayref:
        case TypeKind::Structref:
        case TypeKind::Exnref:
            return true;
        default:
            return false;
        }
    }
    return true;
}

bool Type::definitelyIsWasmGCObjectOrNull() const
{
    if (!isRefType(*this))
        return false;

    if (typeIndexIsType(index)) {
        switch (static_cast<TypeKind>(index)) {
        case TypeKind::Arrayref:
        case TypeKind::Structref:
            return true;
        default:
            return false;
        }
    }

    const TypeDefinition& def = TypeInformation::get(index).expand();
    if (def.is<Wasm::StructType>())
        return true;
    if (def.is<Wasm::ArrayType>())
        return true;
    return false;
}

} } // namespace JSC::Wasm

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif // ENABLE(WEBASSEMBLY)
