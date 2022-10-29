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

#pragma once

#include <variant>

#if ENABLE(WEBASSEMBLY)

#include "SIMDInfo.h"
#include "WasmOps.h"
#include "WasmSIMDOpcodes.h"
#include "Width.h"
#include "WriteBarrier.h"
#include <wtf/CheckedArithmetic.h>
#include <wtf/HashSet.h>
#include <wtf/HashTraits.h>
#include <wtf/Lock.h>
#include <wtf/StdLibExtras.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/Vector.h>

#if ENABLE(WEBASSEMBLY_B3JIT)
#include "B3Type.h"
#endif

namespace JSC {

namespace Wasm {

#if ENABLE(B3_JIT)
#define CREATE_ENUM_VALUE(name, id, ...) name = id,
enum class ExtSIMDOpType : uint8_t {
    FOR_EACH_WASM_EXT_SIMD_OP(CREATE_ENUM_VALUE)
};
#undef CREATE_ENUM_VALUE

constexpr Type simdScalarType(SIMDLane lane)
{
    switch (lane) {
    case SIMDLane::v128:
        RELEASE_ASSERT_NOT_REACHED();
        return Types::Void;
    case SIMDLane::i64x2:
        return Types::I64;
    case SIMDLane::f64x2:
        return Types::F64;
    case SIMDLane::i8x16:
    case SIMDLane::i16x8:
    case SIMDLane::i32x4:
        return Types::I32;
    case SIMDLane::f32x4:
        return Types::F32;
    }
    RELEASE_ASSERT_NOT_REACHED();
}
#endif

using FunctionArgCount = uint32_t;
using StructFieldCount = uint32_t;
using RecursionGroupCount = uint32_t;
using ProjectionIndex = uint32_t;

inline Width Type::width() const
{
    switch (kind) {
#define CREATE_CASE(name, id, b3type, inc, wasmName, width, ...) case TypeKind::name: return widthForBytes(width / 8);
    FOR_EACH_WASM_TYPE(CREATE_CASE)
#undef CREATE_CASE
    }
    RELEASE_ASSERT_NOT_REACHED();
}

#if ENABLE(WEBASSEMBLY_B3JIT)
#define CREATE_CASE(name, id, b3type, ...) case TypeKind::name: return b3type;
inline B3::Type toB3Type(Type type)
{
    switch (type.kind) {
    FOR_EACH_WASM_TYPE(CREATE_CASE)
    }
    RELEASE_ASSERT_NOT_REACHED();
    return B3::Void;
}
#undef CREATE_CASE
#endif

constexpr size_t typeKindSizeInBytes(TypeKind kind)
{
    switch (kind) {
    case TypeKind::I32:
    case TypeKind::F32: {
        return 4;
    }
    case TypeKind::I64:
    case TypeKind::F64: {
        return 8;
    }

    case TypeKind::Arrayref:
    case TypeKind::Funcref:
    case TypeKind::Externref:
    case TypeKind::Ref:
    case TypeKind::RefNull: {
        return sizeof(WriteBarrierBase<Unknown>);
    }
    case TypeKind::V128:
    case TypeKind::Array:
    case TypeKind::Func:
    case TypeKind::Struct:
    case TypeKind::Void:
    case TypeKind::Rec:
    case TypeKind::I31ref: {
        break;
    }
    }

    ASSERT_NOT_REACHED();
    return 0;
}

class FunctionSignature {
public:
    FunctionSignature(Type* payload, FunctionArgCount argumentCount, FunctionArgCount returnCount)
        : m_payload(payload)
        , m_argCount(argumentCount)
        , m_retCount(returnCount)
        , m_hasRecursiveReference(false)
    {
    }

    FunctionArgCount argumentCount() const { return m_argCount; }
    FunctionArgCount returnCount() const { return m_retCount; }
    bool hasRecursiveReference() const { return m_hasRecursiveReference; }
    void setHasRecursiveReference(bool value) { m_hasRecursiveReference = value; }
    Type returnType(FunctionArgCount i) const { ASSERT(i < returnCount()); return const_cast<FunctionSignature*>(this)->getReturnType(i); }
    bool returnsVoid() const { return !returnCount(); }
    Type argumentType(FunctionArgCount i) const { return const_cast<FunctionSignature*>(this)->getArgumentType(i); }

    size_t numVectors() const
    {
        size_t n = 0;
        for (size_t i = 0; i < argumentCount(); ++i) {
            if (argumentType(i).isV128())
                ++n;
        }
        return n;
    }

    bool operator==(const FunctionSignature& other) const
    {
        // Function signatures are unique because it is just an view class over TypeDefinition and
        // so, we can compare two signatures with just payload pointers comparision.
        // Other checks probably aren't necessary but it's good to be paranoid.
        return m_payload == other.m_payload && m_argCount == other.m_argCount && m_retCount == other.m_retCount;
    }
    bool operator!=(const FunctionSignature& other) const { return !(*this == other); }

    WTF::String toString() const;
    void dump(WTF::PrintStream& out) const;

    Type& getReturnType(FunctionArgCount i) { ASSERT(i < returnCount()); return *storage(i); }
    Type& getArgumentType(FunctionArgCount i) { ASSERT(i < argumentCount()); return *storage(returnCount() + i); }

    Type* storage(FunctionArgCount i) { return i + m_payload; }
    const Type* storage(FunctionArgCount i) const { return const_cast<FunctionSignature*>(this)->storage(i); }

private:
    friend class TypeInformation;

    Type* m_payload;
    FunctionArgCount m_argCount;
    FunctionArgCount m_retCount;
    bool m_hasRecursiveReference;
};

// FIXME auto-generate this. https://bugs.webkit.org/show_bug.cgi?id=165231
enum Mutability : uint8_t {
    Mutable = 1,
    Immutable = 0
};

struct FieldType {
    Type type;
    Mutability mutability;

    bool operator==(const FieldType& rhs) const { return type == rhs.type && mutability == rhs.mutability; }
    bool operator!=(const FieldType& rhs) const { return !(*this == rhs); }
};

class StructType {
public:
    StructType(FieldType*, StructFieldCount, const FieldType*);

    StructFieldCount fieldCount() const { return m_fieldCount; }
    bool hasRecursiveReference() const { return m_hasRecursiveReference; }
    void setHasRecursiveReference(bool value) { m_hasRecursiveReference = value; }
    FieldType field(StructFieldCount i) const { return const_cast<StructType*>(this)->getField(i); }

    WTF::String toString() const;
    void dump(WTF::PrintStream& out) const;

    FieldType& getField(StructFieldCount i) { ASSERT(i < fieldCount()); return *storage(i); }
    FieldType* storage(StructFieldCount i) { return i + m_payload; }
    const FieldType* storage(StructFieldCount i) const { return const_cast<StructType*>(this)->storage(i); }

    const unsigned* getFieldOffset(StructFieldCount i) const { ASSERT(i < fieldCount()); return bitwise_cast<const unsigned*>(m_payload + m_fieldCount) + i; }
    unsigned* getFieldOffset(StructFieldCount i) { return const_cast<unsigned*>(const_cast<const StructType*>(this)->getFieldOffset(i)); }

    size_t instancePayloadSize() const { return m_instancePayloadSize; }

private:
    // Payload is structured this way = | field types | precalculated field offsets |.
    FieldType* m_payload;
    StructFieldCount m_fieldCount;
    bool m_hasRecursiveReference;
    size_t m_instancePayloadSize;
};

class ArrayType {
public:
    ArrayType(FieldType* payload)
        : m_payload(payload)
        , m_hasRecursiveReference(false)
    {
    }

    FieldType elementType() const { return const_cast<ArrayType*>(this)->getElementType(); }
    bool hasRecursiveReference() const { return m_hasRecursiveReference; }
    void setHasRecursiveReference(bool value) { m_hasRecursiveReference = value; }

    WTF::String toString() const;
    void dump(WTF::PrintStream& out) const;

    FieldType& getElementType() { return *storage(); }
    FieldType* storage() { return m_payload; }
    const FieldType* storage() const { return const_cast<ArrayType*>(this)->storage(); }

private:
    FieldType* m_payload;
    bool m_hasRecursiveReference;
};

class RecursionGroup {
public:
    RecursionGroup(TypeIndex* payload, RecursionGroupCount typeCount)
        : m_payload(payload)
        , m_typeCount(typeCount)
    {
    }

    RecursionGroupCount typeCount() const { return m_typeCount; }
    TypeIndex type(RecursionGroupCount i) const { return const_cast<RecursionGroup*>(this)->getType(i); }

    WTF::String toString() const;
    void dump(WTF::PrintStream& out) const;

    TypeIndex& getType(RecursionGroupCount i) { ASSERT(i < typeCount());; return *storage(i); }
    TypeIndex* storage(RecursionGroupCount i) { return i + m_payload; }
    const TypeIndex* storage(RecursionGroupCount i) const { return const_cast<RecursionGroup*>(this)->storage(i); }

private:
    TypeIndex* m_payload;
    RecursionGroupCount m_typeCount;
};

// This class represents a projection into a recursion group. That is, if a recursion
// group is defined as $r = (rec (type $s ...) (type $t ...)), then a projection accesses
// the inner types. For example $r.$s or $r.$t, or $r.0 or $r.1 with numeric indices.
//
// See https://github.com/WebAssembly/gc/blob/main/proposals/gc/MVP.md#type-contexts
//
// We store projections rather than the implied unfolding because the actual type being
// represented may be recursive and infinite. Projections are unfolded into a concrete type
// when operations on the type require a specific concrete type.
//
// A projection with an invalid PlaceholderGroup index represents a recursive reference
// that has not yet been resolved. The expand() function on type definitions resolves it.
class Projection {
public:
    Projection(TypeIndex* payload)
        : m_payload(payload)
    {
    }

    TypeIndex recursionGroup() const { return const_cast<Projection*>(this)->getRecursionGroup(); }
    ProjectionIndex index() const { return const_cast<Projection*>(this)->getIndex(); }

    WTF::String toString() const;
    void dump(WTF::PrintStream& out) const;

    TypeIndex& getRecursionGroup() { return *storage(0); }
    ProjectionIndex& getIndex() { return *reinterpret_cast<ProjectionIndex*>(storage(1)); }
    TypeIndex* storage(uint32_t i) { ASSERT(i <= 1); return i + m_payload; }
    const TypeIndex* storage(uint32_t i) const { return const_cast<Projection*>(this)->storage(i); }

    static constexpr TypeIndex PlaceholderGroup = 0;
    bool isPlaceholder() const { return recursionGroup() == PlaceholderGroup; }

private:
    TypeIndex* m_payload;
};
static_assert(sizeof(ProjectionIndex) <= sizeof(TypeIndex));

enum class TypeDefinitionKind : uint8_t {
    FunctionSignature,
    StructType,
    ArrayType,
    RecursionGroup,
    Projection
};

class TypeDefinition : public ThreadSafeRefCounted<TypeDefinition> {
    WTF_MAKE_FAST_ALLOCATED;

    TypeDefinition() = delete;
    TypeDefinition(const TypeDefinition&) = delete;

    TypeDefinition(TypeDefinitionKind kind, FunctionArgCount retCount, FunctionArgCount argCount)
        : m_typeHeader { FunctionSignature { static_cast<Type*>(payload()), argCount, retCount } }
    {
        RELEASE_ASSERT(kind == TypeDefinitionKind::FunctionSignature);
    }

    TypeDefinition(TypeDefinitionKind kind, uint32_t fieldCount, const FieldType* fields)
        : m_typeHeader { StructType { static_cast<FieldType*>(payload()), static_cast<StructFieldCount>(fieldCount), fields } }
    {
        RELEASE_ASSERT(kind == TypeDefinitionKind::StructType);
    }

    TypeDefinition(TypeDefinitionKind kind, uint32_t fieldCount)
        : m_typeHeader { RecursionGroup { static_cast<TypeIndex*>(payload()), static_cast<RecursionGroupCount>(fieldCount) } }
    {
        RELEASE_ASSERT(kind == TypeDefinitionKind::RecursionGroup);
    }

    TypeDefinition(TypeDefinitionKind kind)
        : m_typeHeader { ArrayType { static_cast<FieldType*>(payload()) } }
    {
        if (kind == TypeDefinitionKind::Projection)
            m_typeHeader = { Projection { static_cast<TypeIndex*>(payload()) } };
        else
            RELEASE_ASSERT(kind == TypeDefinitionKind::ArrayType);
    }

    // Payload starts past end of this object.
    void* payload() { return this + 1; }

    static size_t allocatedFunctionSize(Checked<FunctionArgCount> retCount, Checked<FunctionArgCount> argCount) { return sizeof(TypeDefinition) + (retCount + argCount) * sizeof(Type); }
    static size_t allocatedStructSize(Checked<StructFieldCount> fieldCount) { return sizeof(TypeDefinition) + fieldCount * (sizeof(FieldType) + sizeof(unsigned)); }
    static size_t allocatedArraySize() { return sizeof(TypeDefinition) + sizeof(FieldType); }
    static size_t allocatedRecursionGroupSize(Checked<RecursionGroupCount> typeCount) { return sizeof(TypeDefinition) + typeCount * sizeof(TypeIndex); }
    static size_t allocatedProjectionSize() { return sizeof(TypeDefinition) + 2 * sizeof(TypeIndex); }

public:
    template <typename T>
    bool is() const { return std::holds_alternative<T>(m_typeHeader); }

    template <typename T>
    T* as() { ASSERT(is<T>()); return std::get_if<T>(&m_typeHeader); }

    template <typename T>
    const T* as() const { return const_cast<TypeDefinition*>(this)->as<T>(); }

    TypeIndex index() const { return bitwise_cast<TypeIndex>(this); }

    WTF::String toString() const;
    void dump(WTF::PrintStream& out) const;
    bool operator==(const TypeDefinition& rhs) const { return this == &rhs; }
    unsigned hash() const;

    const TypeDefinition& replacePlaceholders(TypeIndex) const;
    const TypeDefinition& expand() const;

    // Type definitions are uniqued and, for call_indirect, validated at runtime. Tables can create invalid TypeIndex values which cause call_indirect to fail. We use 0 as the invalidIndex so that the codegen can easily test for it and trap, and we add a token invalid entry in TypeInformation.
    static const constexpr TypeIndex invalidIndex = 0;

private:
    friend class TypeInformation;
    friend struct FunctionParameterTypes;
    friend struct StructParameterTypes;
    friend struct ArrayParameterTypes;
    friend struct RecursionGroupParameterTypes;
    friend struct ProjectionParameterTypes;

    static RefPtr<TypeDefinition> tryCreateFunctionSignature(FunctionArgCount returnCount, FunctionArgCount argumentCount);
    static RefPtr<TypeDefinition> tryCreateStructType(StructFieldCount, const FieldType*);
    static RefPtr<TypeDefinition> tryCreateArrayType();
    static RefPtr<TypeDefinition> tryCreateRecursionGroup(RecursionGroupCount);
    static RefPtr<TypeDefinition> tryCreateProjection();

    static Type substitute(Type, TypeIndex);

    std::variant<FunctionSignature, StructType, ArrayType, RecursionGroup, Projection> m_typeHeader;
    // Payload is stored here.
};

struct TypeHash {
    RefPtr<TypeDefinition> key { nullptr };
    TypeHash() = default;
    explicit TypeHash(Ref<TypeDefinition>&& key)
        : key(WTFMove(key))
    { }
    explicit TypeHash(WTF::HashTableDeletedValueType)
        : key(WTF::HashTableDeletedValue)
    { }
    bool operator==(const TypeHash& rhs) const { return equal(*this, rhs); }
    static bool equal(const TypeHash& lhs, const TypeHash& rhs) { return lhs.key == rhs.key; }
    static unsigned hash(const TypeHash& typeHash) { return typeHash.key ? typeHash.key->hash() : 0; }
    static constexpr bool safeToCompareToEmptyOrDeleted = false;
    bool isHashTableDeletedValue() const { return key.isHashTableDeletedValue(); }
};

} } // namespace JSC::Wasm


namespace WTF {

template<typename T> struct DefaultHash;
template<> struct DefaultHash<JSC::Wasm::TypeHash> : JSC::Wasm::TypeHash { };

template<typename T> struct HashTraits;
template<> struct HashTraits<JSC::Wasm::TypeHash> : SimpleClassHashTraits<JSC::Wasm::TypeHash> {
    static constexpr bool emptyValueIsZero = true;
};

} // namespace WTF


namespace JSC { namespace Wasm {

// Type information is held globally and shared by the entire process to allow all type definitions to be unique. This is required when wasm calls another wasm instance, and must work when modules are shared between multiple VMs.
class TypeInformation {
    WTF_MAKE_NONCOPYABLE(TypeInformation);
    WTF_MAKE_FAST_ALLOCATED;

    TypeInformation();

public:
    static TypeInformation& singleton();

    static RefPtr<TypeDefinition> typeDefinitionForFunction(const Vector<Type, 1>& returnTypes, const Vector<Type>& argumentTypes);
    static RefPtr<TypeDefinition> typeDefinitionForStruct(const Vector<FieldType>& fields);
    static RefPtr<TypeDefinition> typeDefinitionForArray(FieldType);
    static RefPtr<TypeDefinition> typeDefinitionForRecursionGroup(const Vector<TypeIndex>& types);
    static RefPtr<TypeDefinition> typeDefinitionForProjection(TypeIndex, ProjectionIndex);
    ALWAYS_INLINE const TypeDefinition* thunkFor(Type type) const { return thunkTypes[linearizeType(type.kind)]; }

    static const TypeDefinition& get(TypeIndex);
    static TypeIndex get(const TypeDefinition&);

    static const FunctionSignature& getFunctionSignature(TypeIndex);

    static void tryCleanup();
private:
    HashSet<Wasm::TypeHash> m_typeSet;
    const TypeDefinition* thunkTypes[numTypes];
    Lock m_lock;
};

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
