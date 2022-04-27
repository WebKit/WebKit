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

#include "WasmOps.h"
#include <wtf/CheckedArithmetic.h>
#include <wtf/HashSet.h>
#include <wtf/HashTraits.h>
#include <wtf/Lock.h>
#include <wtf/StdLibExtras.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/Vector.h>

namespace JSC {

namespace Wasm {

using FunctionArgCount = uint32_t;
using StructFieldCount = uint32_t;

class FunctionSignature {
public:
    FunctionSignature(Type* payload, FunctionArgCount argumentCount, FunctionArgCount returnCount)
        : m_payload(payload)
        , m_argCount(argumentCount)
        , m_retCount(returnCount)
    {
    }

    FunctionArgCount argumentCount() const { return m_argCount; }
    FunctionArgCount returnCount() const { return m_retCount; }
    Type returnType(FunctionArgCount i) const { ASSERT(i < returnCount()); return const_cast<FunctionSignature*>(this)->getReturnType(i); }
    bool returnsVoid() const { return !returnCount(); }
    Type argumentType(FunctionArgCount i) const { return const_cast<FunctionSignature*>(this)->getArgumentType(i); }

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
};

// FIXME auto-generate this. https://bugs.webkit.org/show_bug.cgi?id=165231
enum Mutability : uint8_t {
    Mutable = 1,
    Immutable = 0
};

struct StructField {
    Type type;
    Mutability mutability;

    bool operator==(const StructField& rhs) const { return type == rhs.type && mutability == rhs.mutability; }
    bool operator!=(const StructField& rhs) const { return !(*this == rhs); }
};

class StructType {
public:
    StructType(StructField* payload, StructFieldCount fieldCount)
        : m_payload(payload)
        , m_fieldCount(fieldCount)
    {
    }

    StructFieldCount fieldCount() const { return m_fieldCount; }
    StructField field(StructFieldCount i) const { return const_cast<StructType*>(this)->getField(i); }

    WTF::String toString() const;
    void dump(WTF::PrintStream& out) const;

    StructField& getField(StructFieldCount i) { ASSERT(i < fieldCount()); return *storage(i); }
    StructField* storage(StructFieldCount i) { return i + m_payload; }
    const StructField* storage(StructFieldCount i) const { return const_cast<StructType*>(this)->storage(i); }

private:
    StructField* m_payload;
    StructFieldCount m_fieldCount;
};

class TypeDefinition : public ThreadSafeRefCounted<TypeDefinition> {
    WTF_MAKE_FAST_ALLOCATED;

    TypeDefinition() = delete;
    TypeDefinition(const TypeDefinition&) = delete;

    TypeDefinition(FunctionArgCount retCount, FunctionArgCount argCount)
        : m_typeHeader { FunctionSignature { static_cast<Type*>(payload()), argCount, retCount } }
    {
    }

    TypeDefinition(StructFieldCount fieldCount)
        : m_typeHeader { StructType { static_cast<StructField*>(payload()), fieldCount } }
    {
    }

    // Payload starts past end of this object.
    void* payload() { return this + 1; }

    static size_t allocatedFunctionSize(Checked<FunctionArgCount> retCount, Checked<FunctionArgCount> argCount) { return sizeof(TypeDefinition) + (retCount + argCount) * sizeof(Type); }
    static size_t allocatedStructSize(Checked<StructFieldCount> fieldCount) { return sizeof(TypeDefinition) + fieldCount * sizeof(StructField); }

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

    // Type definitions are uniqued and, for call_indirect, validated at runtime. Tables can create invalid TypeIndex values which cause call_indirect to fail. We use 0 as the invalidIndex so that the codegen can easily test for it and trap, and we add a token invalid entry in TypeInformation.
    static const constexpr TypeIndex invalidIndex = 0;

private:
    friend class TypeInformation;
    friend struct FunctionParameterTypes;
    friend struct StructParameterTypes;

    static RefPtr<TypeDefinition> tryCreateFunctionSignature(FunctionArgCount returnCount, FunctionArgCount argumentCount);
    static RefPtr<TypeDefinition> tryCreateStructType(StructFieldCount);

    std::variant<FunctionSignature, StructType> m_typeHeader;
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
    static RefPtr<TypeDefinition> typeDefinitionForStruct(const Vector<StructField>& fields);
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
