/*
 * Copyright (C) 2016-2024 Apple Inc. All rights reserved.
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

#include <wtf/Platform.h>

#if ENABLE(WEBASSEMBLY)

#include <JavaScriptCore/JITCompilation.h>
#include <JavaScriptCore/SIMDInfo.h>
#include <JavaScriptCore/WasmLimits.h>
#include <JavaScriptCore/WasmOps.h>
#include <JavaScriptCore/WasmSIMDOpcodes.h>
#include <JavaScriptCore/Width.h>
#include <JavaScriptCore/WriteBarrier.h>
#include <wtf/CheckedArithmetic.h>
#include <wtf/EmbeddedFixedVector.h>
#include <wtf/FixedVector.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/HashTraits.h>
#include <wtf/Lock.h>
#include <wtf/ReferenceWrapperVector.h>
#include <wtf/StdLibExtras.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/ThreadSafeLazyUniquePtr.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/Vector.h>

#if ENABLE(WEBASSEMBLY_OMGJIT) || ENABLE(WEBASSEMBLY_BBQJIT)
#include <JavaScriptCore/B3Type.h>
#endif

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC {

class LLIntOffsetsExtractor;

namespace Wasm {

class JSToWasmICCallee;
class RTT;
class RTTGroup;
class SectionParser;
class TypeSectionState;
struct CallInformation;

// Shared aINT/uINT bytecode, canonicalized per signature. Aliased so
// offlineasm can reference it without template syntax.
using IPIntSharedBytecode = EmbeddedFixedVector<uint8_t>;

#define CREATE_ENUM_VALUE(name, id, ...) name = id,
enum class ExtSIMDOpType : uint32_t {
    FOR_EACH_WASM_EXT_SIMD_OP(CREATE_ENUM_VALUE)
};
#undef CREATE_ENUM_VALUE

#define CREATE_CASE(name, ...) case ExtSIMDOpType::name: return #name ## _s;
inline ASCIILiteral makeString(ExtSIMDOpType op)
{
    switch (op) {
        FOR_EACH_WASM_EXT_SIMD_OP(CREATE_CASE)
    }
    RELEASE_ASSERT_NOT_REACHED();
    return { };
}
#undef CREATE_CASE

constexpr std::pair<size_t, size_t> countNumberOfWasmExtendedSIMDOpcodes()
{
    uint8_t numberOfOpcodes = 0;
    uint8_t mapSize = 0;
#define COUNT_EXT_SIMD_OPERATION(name, id, ...) \
    numberOfOpcodes++; \
    mapSize = std::max<size_t>(mapSize, (size_t)id);
    FOR_EACH_WASM_EXT_SIMD_OP(COUNT_EXT_SIMD_OPERATION)
#undef COUNT_EXT_SIMD_OPERATION
    return { numberOfOpcodes, mapSize + 1 };
}

constexpr bool isRegisteredWasmExtendedSIMDOpcode(ExtSIMDOpType op)
{
    switch (op) {
#define CREATE_CASE(name, id, ...) case ExtSIMDOpType::name:
    FOR_EACH_WASM_EXT_SIMD_OP(CREATE_CASE)
#undef CREATE_CASE
        return true;
    default:
        return false;
    }
}

constexpr void dumpExtSIMDOpType(PrintStream& out, ExtSIMDOpType op)
{
    switch (op) {
#define CREATE_CASE(name, id, ...) case ExtSIMDOpType::name: out.print(#name); break;
    FOR_EACH_WASM_EXT_SIMD_OP(CREATE_CASE)
#undef CREATE_CASE
    default:
        return;
    }
}

MAKE_PRINT_ADAPTOR(ExtSIMDOpTypeDump, ExtSIMDOpType, dumpExtSIMDOpType);

constexpr std::pair<size_t, size_t> countNumberOfWasmExtendedAtomicOpcodes()
{
    uint8_t numberOfOpcodes = 0;
    uint8_t mapSize = 0;
#define COUNT_WASM_EXT_ATOMIC_OP(name, id, ...) \
    numberOfOpcodes++;                      \
    mapSize = std::max<size_t>(mapSize, (size_t)id);
    FOR_EACH_WASM_EXT_ATOMIC_LOAD_OP(COUNT_WASM_EXT_ATOMIC_OP);
    FOR_EACH_WASM_EXT_ATOMIC_STORE_OP(COUNT_WASM_EXT_ATOMIC_OP);
    FOR_EACH_WASM_EXT_ATOMIC_BINARY_RMW_OP(COUNT_WASM_EXT_ATOMIC_OP);
    FOR_EACH_WASM_EXT_ATOMIC_OTHER_OP(COUNT_WASM_EXT_ATOMIC_OP);
#undef COUNT_WASM_EXT_ATOMIC_OP
    return { numberOfOpcodes, mapSize + 1 };
}

constexpr bool isRegisteredExtenedAtomicOpcode(ExtAtomicOpType op)
{
    switch (op) {
#define CREATE_CASE(name, id, ...) case ExtAtomicOpType::name:
    FOR_EACH_WASM_EXT_ATOMIC_LOAD_OP(CREATE_CASE)
    FOR_EACH_WASM_EXT_ATOMIC_STORE_OP(CREATE_CASE)
    FOR_EACH_WASM_EXT_ATOMIC_BINARY_RMW_OP(CREATE_CASE)
    FOR_EACH_WASM_EXT_ATOMIC_OTHER_OP(CREATE_CASE)
#undef CREATE_CASE
        return true;
    default:
        return false;
    }
}

constexpr void dumpExtAtomicOpType(PrintStream& out, ExtAtomicOpType op)
{
    switch (op) {
#define CREATE_CASE(name, id, ...) case ExtAtomicOpType::name: out.print(#name); break;
    FOR_EACH_WASM_EXT_ATOMIC_LOAD_OP(CREATE_CASE)
    FOR_EACH_WASM_EXT_ATOMIC_STORE_OP(CREATE_CASE)
    FOR_EACH_WASM_EXT_ATOMIC_BINARY_RMW_OP(CREATE_CASE)
    FOR_EACH_WASM_EXT_ATOMIC_OTHER_OP(CREATE_CASE)
#undef CREATE_CASE
    default:
        return;
    }
}

MAKE_PRINT_ADAPTOR(ExtAtomicOpTypeDump, ExtAtomicOpType, dumpExtAtomicOpType);

constexpr std::pair<size_t, size_t> countNumberOfWasmGCOpcodes()
{
    uint8_t numberOfOpcodes = 0;
    uint8_t mapSize = 0;
#define COUNT_WASM_GC_OP(name, id, ...) \
    numberOfOpcodes++;                  \
    mapSize = std::max<size_t>(mapSize, (size_t)id);
    FOR_EACH_WASM_GC_OP(COUNT_WASM_GC_OP);
#undef COUNT_WASM_GC_OP
    return { numberOfOpcodes, mapSize + 1 };
}

constexpr bool isRegisteredGCOpcode(ExtGCOpType op)
{
    switch (op) {
#define CREATE_CASE(name, id, ...) case ExtGCOpType::name:
    FOR_EACH_WASM_GC_OP(CREATE_CASE)
#undef CREATE_CASE
        return true;
    default:
        return false;
    }
}

constexpr void dumpExtGCOpType(PrintStream& out, ExtGCOpType op)
{
    switch (op) {
#define CREATE_CASE(name, id, ...) case ExtGCOpType::name: out.print(#name); break;
    FOR_EACH_WASM_GC_OP(CREATE_CASE)
#undef CREATE_CASE
    default:
        return;
    }
}

MAKE_PRINT_ADAPTOR(ExtGCOpTypeDump, ExtGCOpType, dumpExtGCOpType);

constexpr std::pair<size_t, size_t> countNumberOfWasmBaseOpcodes()
{
    uint8_t numberOfOpcodes = 0;
    uint8_t mapSize = 0;
#define COUNT_WASM_OP(name, id, ...) \
    numberOfOpcodes++;               \
    mapSize = std::max<size_t>(mapSize, (size_t)id);
    FOR_EACH_WASM_OP(COUNT_WASM_OP);
#undef COUNT_WASM_OP
    return { numberOfOpcodes, mapSize + 1 };
}

constexpr bool isRegisteredBaseOpcode(OpType op)
{
    switch (op) {
#define CREATE_CASE(name, id, ...) case OpType::name:
    FOR_EACH_WASM_OP(CREATE_CASE)
#undef CREATE_CASE
        return true;
    default:
        return false;
    }
}

constexpr void dumpOpType(PrintStream& out, OpType op)
{
    switch (op) {
#define CREATE_CASE(name, id, ...) case OpType::name: out.print(#name); break;
    FOR_EACH_WASM_OP(CREATE_CASE)
#undef CREATE_CASE
    default:
        return;
    }
}

MAKE_PRINT_ADAPTOR(OpTypeDump, OpType, dumpOpType);

inline bool isCompareOpType(OpType op)
{
    switch (op) {
#define CREATE_CASE(name, ...) case name: return true;
    FOR_EACH_WASM_COMPARE_UNARY_OP(CREATE_CASE)
    FOR_EACH_WASM_COMPARE_BINARY_OP(CREATE_CASE)
#undef CREATE_CASE
    default:
        return false;
    }
}

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

using FunctionArgCount = uint32_t;
using StructFieldCount = uint32_t;
using RecursionGroupCount = uint32_t;
using ProjectionIndex = uint32_t;
using DisplayCount = uint32_t;
using SupertypeCount = uint32_t;

ALWAYS_INLINE Width Type::width() const
{
    switch (kind) {
#define CREATE_CASE(name, id, b3type, inc, wasmName, width, ...) case TypeKind::name: return widthForBytes(width / 8);
    FOR_EACH_WASM_TYPE(CREATE_CASE)
#undef CREATE_CASE
    }
    RELEASE_ASSERT_NOT_REACHED();
}

#if ENABLE(WEBASSEMBLY_OMGJIT) || ENABLE(WEBASSEMBLY_BBQJIT)
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
    case TypeKind::V128:
        return 16;

    case TypeKind::Arrayref:
    case TypeKind::Structref:
    case TypeKind::Funcref:
    case TypeKind::Exnref:
    case TypeKind::Externref:
    case TypeKind::Ref:
    case TypeKind::RefNull: {
        return sizeof(WriteBarrierBase<Unknown>);
    }
    case TypeKind::Array:
    case TypeKind::Func:
    case TypeKind::Struct:
    case TypeKind::Void:
    case TypeKind::Sub:
    case TypeKind::Subfinal:
    case TypeKind::Rec:
    case TypeKind::Eqref:
    case TypeKind::Anyref:
    case TypeKind::Noexnref:
    case TypeKind::Noneref:
    case TypeKind::Nofuncref:
    case TypeKind::Noexternref:
    case TypeKind::I31ref: {
        break;
    }
    }

    ASSERT_NOT_REACHED();
    return 0;
}

class RecursionGroup;

// Sentinel TypeIndex for "no type" / uninitialized slots (e.g. empty entries).
static constexpr TypeIndex invalidTypeIndex = 0;

// FIXME auto-generate this. https://bugs.webkit.org/show_bug.cgi?id=165231
enum Mutability : uint8_t {
    Mutable = 1,
    Immutable = 0
};

struct StorageType {
public:
    template <typename T>
    bool is() const { return std::holds_alternative<T>(m_storageType); }

    template <typename T>
    const T as() const { ASSERT(is<T>()); return *std::get_if<T>(&m_storageType); }

    StorageType() = default;

    explicit StorageType(Type t)
    {
        m_storageType = Variant<Type, PackedType>(t);
    }

    explicit StorageType(PackedType t)
    {
        m_storageType = Variant<Type, PackedType>(t);
    }

    // Return a value type suitable for validating instruction arguments. Packed types cannot show up as value types and need to be unpacked to I32.
    Type unpacked() const
    {
        if (is<Type>())
            return as<Type>();
        return Types::I32;
    }

    size_t elementSize() const
    {
        if (is<Type>()) {
            switch (as<Type>().kind) {
            case Wasm::TypeKind::I32:
            case Wasm::TypeKind::F32:
                return sizeof(uint32_t);
            case Wasm::TypeKind::I64:
            case Wasm::TypeKind::F64:
            case Wasm::TypeKind::Ref:
            case Wasm::TypeKind::RefNull:
                return sizeof(uint64_t);
            case Wasm::TypeKind::V128:
                return sizeof(v128_t);
            default:
                RELEASE_ASSERT_NOT_REACHED();
            }
        }
        switch (as<PackedType>()) {
        case PackedType::I8:
            return sizeof(uint8_t);
        case PackedType::I16:
            return sizeof(uint16_t);
        }
        RELEASE_ASSERT_NOT_REACHED();
    }

    bool operator==(const StorageType& rhs) const
    {
        if (rhs.is<PackedType>())
            return (is<PackedType>() && as<PackedType>() == rhs.as<PackedType>());
        if (!is<Type>())
            return false;
        return(as<Type>() == rhs.as<Type>());
    }

    int8_t typeCode() const
    {
        if (is<Type>())
            return static_cast<int8_t>(as<Type>().kind);
        return static_cast<int8_t>(as<PackedType>());
    }

    TypeIndex index() const
    {
        if (is<Type>())
            return as<Type>().index;
        return 0;
    }
    void dump(WTF::PrintStream& out) const;

private:
    Variant<Type, PackedType> m_storageType;
};

inline ASCIILiteral makeString(const StorageType& storageType)
{
    return(storageType.is<Type>() ? makeString(storageType.as<Type>().kind) :
        makeString(storageType.as<PackedType>()));
}

inline size_t typeSizeInBytes(const StorageType& storageType)
{
    if (storageType.is<PackedType>()) {
        switch (storageType.as<PackedType>()) {
        case PackedType::I8: {
            return 1;
        }
        case PackedType::I16: {
            return 2;
        }
        }
    }
    return typeKindSizeInBytes(storageType.as<Type>().kind);
}

inline size_t typeAlignmentInBytes(const StorageType& storageType)
{
    return typeSizeInBytes(storageType);
}

class FieldType {
public:
    StorageType type;
    Mutability mutability;

    friend bool operator==(const FieldType&, const FieldType&) = default;
};

// Co-locates a Type value with the RefPtr that anchors the canonical RTT it
// references (if any). Used inside payload storage so anchors travel with
// their slots under copy/move -- no separate vector to keep in sync.
// rttAnchor is null unless `type` is a ref type whose TypeIndex is a bare
// canonical RTT pointer that needs to be kept alive (intra- or inter-recgroup).
struct TypeSlot {
    Type type;
    RefPtr<const RTT> rttAnchor;

    TypeSlot() = default;
    TypeSlot(Type t)
        : type(t)
    {
    }
    TypeSlot(Type t, RefPtr<const RTT> anchor)
        : type(t)
        , rttAnchor(WTF::move(anchor))
    {
    }
};

// Paired field + offset entry. Collapses what used to be two parallel
// FixedVectors in RTTStructPayload (fields + field offsets) into a single
// allocation. Costs one alignment-induced padding word per field but saves
// a heap allocation per struct RTT -- a measurable win during type-section
// parse given how many struct RTTs are built. rttAnchor anchors the
// canonical RTT referenced by `type.type` when it's a ref Type.
struct StructFieldEntry {
    FieldType type;
    unsigned offset;
    RefPtr<const RTT> rttAnchor;

    StructFieldEntry() = default;
    StructFieldEntry(FieldType type, unsigned offset, RefPtr<const RTT> anchor = nullptr)
        : type(type)
        , offset(offset)
        , rttAnchor(WTF::move(anchor))
    {
    }
};

// An RTT encodes subtyping information in a way that is suitable for executing
// runtime subtyping checks, e.g., for ref.cast and related operations. RTTs are also
// used to facilitate static subtyping checks for references.
//
// It contains a display data structure that allows subtyping of references to be checked in constant time.
//
// See https://github.com/WebAssembly/gc/blob/main/proposals/gc/MVP.md#runtime-types for an explanation of displays.
enum class RTTKind : uint8_t {
    Function,
    Array,
    Struct
};

// RTT*Payload holds the structural data for a concrete type (function,
// struct, or array). One of the three is stored inline in RTT via Variant,
// chosen by RTTKind.
class RTTFunctionPayload {
    WTF_MAKE_NONCOPYABLE(RTTFunctionPayload);
    // TypeSectionState::createCanonicalRTT(const Subtype&) reads
    // signatureSpan() to bulk-copy the slot vector. No other consumer.
    friend class TypeSectionState;
public:
    RTTFunctionPayload() = default;
    RTTFunctionPayload(RTTFunctionPayload&&) = default;
    RTTFunctionPayload& operator=(RTTFunctionPayload&&) = default;

    RTTFunctionPayload(FunctionArgCount argCount, FunctionArgCount retCount, std::span<const TypeSlot> signatureReturnsThenArgs, bool includesI64, bool includesV128, bool includesExnref, bool hasRecursiveReference)
        : m_signature(signatureReturnsThenArgs)
        , m_argCount(argCount)
        , m_retCount(retCount)
        , m_argumentsOrResultsIncludeI64(includesI64)
        , m_argumentsOrResultsIncludeV128(includesV128)
        , m_argumentsOrResultsIncludeExnref(includesExnref)
        , m_hasRecursiveReference(hasRecursiveReference)
    {
        ASSERT(m_signature.size() == static_cast<size_t>(m_argCount) + static_cast<size_t>(m_retCount));
    }

    // Move-in constructor: caller has already built a FixedVector<TypeSlot>
    // for the signature (returns then args). Avoids the span-copy performed
    // by the span-based constructor. Used by provider-based typeDefinitionFor*
    // paths that populate the FixedVector in place.
    RTTFunctionPayload(FunctionArgCount argCount, FunctionArgCount retCount, FixedVector<TypeSlot>&& signatureReturnsThenArgs, bool includesI64, bool includesV128, bool includesExnref, bool hasRecursiveReference)
        : m_signature(WTF::move(signatureReturnsThenArgs))
        , m_argCount(argCount)
        , m_retCount(retCount)
        , m_argumentsOrResultsIncludeI64(includesI64)
        , m_argumentsOrResultsIncludeV128(includesV128)
        , m_argumentsOrResultsIncludeExnref(includesExnref)
        , m_hasRecursiveReference(hasRecursiveReference)
    {
        ASSERT(m_signature.size() == static_cast<size_t>(m_argCount) + static_cast<size_t>(m_retCount));
    }

    FunctionArgCount argumentCount() const { return m_argCount; }
    FunctionArgCount returnCount() const { return m_retCount; }
    Type returnType(FunctionArgCount i) const { ASSERT(i < m_retCount); return m_signature[i].type; }
    Type argumentType(FunctionArgCount i) const { ASSERT(i < m_argCount); return m_signature[m_retCount + i].type; }
    bool returnsVoid() const { return !m_retCount; }
    bool argumentsOrResultsIncludeI64() const { return m_argumentsOrResultsIncludeI64; }
    bool argumentsOrResultsIncludeV128() const { return m_argumentsOrResultsIncludeV128; }
    bool argumentsOrResultsIncludeExnref() const { return m_argumentsOrResultsIncludeExnref; }
    bool hasRecursiveReference() const { return m_hasRecursiveReference; }

    // Iterate every ref-bearing slot, invoking `cb(TypeSlot&)`. Used by both
    // the rewrite path (RTT::rewriteInternalRefs) and the cycle-break path
    // (RTT::clearReferencedRTTs).
    template<typename Callback>
    void visitChildrenRTT(Callback&& cb)
    {
        for (TypeSlot& slot : m_signature)
            cb(slot);
    }

    template<typename Callback>
    void forEachPayloadRTTRef(Callback&& cb) const
    {
        for (const TypeSlot& slot : m_signature) {
            if (slot.rttAnchor)
                cb(slot.rttAnchor.get());
        }
    }

private:
    std::span<const TypeSlot> signatureSpan() const LIFETIME_BOUND { return m_signature.span(); }

    FixedVector<TypeSlot> m_signature;
    FunctionArgCount m_argCount { 0 };
    FunctionArgCount m_retCount { 0 };
    bool m_argumentsOrResultsIncludeI64 : 1 { false };
    bool m_argumentsOrResultsIncludeV128 : 1 { false };
    bool m_argumentsOrResultsIncludeExnref : 1 { false };
    bool m_hasRecursiveReference : 1 { false };
};

class RTTStructPayload {
    WTF_MAKE_NONCOPYABLE(RTTStructPayload);
    // TypeSectionState::createCanonicalRTT(const Subtype&) reads
    // fieldsSpan() to bulk-copy the entry vector. No other consumer.
    friend class TypeSectionState;
public:
    RTTStructPayload() = default;
    RTTStructPayload(RTTStructPayload&&) = default;
    RTTStructPayload& operator=(RTTStructPayload&&) = default;

    // Move-in constructor: caller has prebuilt the FixedVector<StructFieldEntry>
    // (fields + offsets colocated). One heap allocation per struct RTT.
    RTTStructPayload(FixedVector<StructFieldEntry>&& fields, size_t instancePayloadSize, bool hasRefFieldTypes, bool hasRecursiveReference)
        : m_fields(WTF::move(fields))
        , m_instancePayloadSize(instancePayloadSize)
        , m_hasRefFieldTypes(hasRefFieldTypes)
        , m_hasRecursiveReference(hasRecursiveReference)
    {
    }

    StructFieldCount fieldCount() const { return m_fields.size(); }
    const FieldType& field(StructFieldCount i) const LIFETIME_BOUND { return m_fields[i].type; }
    unsigned offsetOfFieldInPayload(StructFieldCount i) const { return m_fields[i].offset; }
    size_t instancePayloadSize() const { return m_instancePayloadSize; }
    bool hasRefFieldTypes() const { return m_hasRefFieldTypes; }
    bool hasRecursiveReference() const { return m_hasRecursiveReference; }

    // Iterate every ref-bearing field (PackedType fields are skipped --
    // they cannot reference an RTT). The callback receives a TypeSlot& view
    // built from the entry's FieldType + rttAnchor; mutations are written
    // back into the entry on return.
    template<typename Callback>
    void visitChildrenRTT(Callback&& cb)
    {
        for (StructFieldEntry& entry : m_fields) {
            if (!entry.type.type.is<Type>())
                continue;
            TypeSlot slot { entry.type.type.as<Type>(), WTF::move(entry.rttAnchor) };
            cb(slot);
            entry.type.type = StorageType(slot.type);
            entry.rttAnchor = WTF::move(slot.rttAnchor);
        }
    }

    template<typename Callback>
    void forEachPayloadRTTRef(Callback&& cb) const
    {
        for (const StructFieldEntry& entry : m_fields) {
            if (entry.rttAnchor)
                cb(entry.rttAnchor.get());
        }
    }

private:
    std::span<const StructFieldEntry> fieldsSpan() const LIFETIME_BOUND { return m_fields.span(); }

    FixedVector<StructFieldEntry> m_fields;
    size_t m_instancePayloadSize { 0 };
    bool m_hasRefFieldTypes : 1 { false };
    bool m_hasRecursiveReference : 1 { false };
};

class RTTArrayPayload {
    WTF_MAKE_NONCOPYABLE(RTTArrayPayload);
    // TypeSectionState::createCanonicalRTT(const Subtype&) reads
    // elementTypeAnchor() to bulk-copy the anchor. No other consumer.
    friend class TypeSectionState;
public:
    RTTArrayPayload() = default;
    RTTArrayPayload(RTTArrayPayload&&) = default;
    RTTArrayPayload& operator=(RTTArrayPayload&&) = default;

    RTTArrayPayload(FieldType elementType, RefPtr<const RTT> elementTypeAnchor, bool hasRecursiveReference)
        : m_elementType(elementType)
        , m_elementTypeAnchor(WTF::move(elementTypeAnchor))
        , m_hasRecursiveReference(hasRecursiveReference)
    {
    }

    const FieldType& elementType() const LIFETIME_BOUND { return m_elementType; }
    bool hasRecursiveReference() const { return m_hasRecursiveReference; }

    // Iterate the (single) ref-bearing element, if any. PackedType element
    // types are skipped. Same view-pattern as RTTStructPayload::visitChildrenRTT.
    template<typename Callback>
    void visitChildrenRTT(Callback&& cb)
    {
        if (!m_elementType.type.is<Type>())
            return;
        TypeSlot slot { m_elementType.type.as<Type>(), WTF::move(m_elementTypeAnchor) };
        cb(slot);
        m_elementType.type = StorageType(slot.type);
        m_elementTypeAnchor = WTF::move(slot.rttAnchor);
    }

    template<typename Callback>
    void forEachPayloadRTTRef(Callback&& cb) const
    {
        if (m_elementTypeAnchor)
            cb(m_elementTypeAnchor.get());
    }

private:
    const RefPtr<const RTT>& elementTypeAnchor() const LIFETIME_BOUND { return m_elementTypeAnchor; }

    FieldType m_elementType { };
    RefPtr<const RTT> m_elementTypeAnchor;
    bool m_hasRecursiveReference { false };
};

class alignas(16) RTT final : public ThreadSafeRefCounted<RTT>, private TrailingArray<RTT, RefPtr<const RTT>> {
    WTF_DEPRECATED_MAKE_FAST_COMPACT_ALLOCATED(RTT);
    WTF_MAKE_NONMOVABLE(RTT);
    using TrailingArrayType = TrailingArray<RTT, RefPtr<const RTT>>;
    friend TrailingArrayType;
    friend class JSC::LLIntOffsetsExtractor;
public:
    static_assert(sizeof(const RTT*) == sizeof(RefPtr<const RTT>));
    static constexpr unsigned inlinedDisplaySize = 6;
    RTT() = delete;
    JS_EXPORT_PRIVATE ~RTT();

    static RefPtr<RTT> tryCreateFunction(bool isFinalType, RTTFunctionPayload&&);
    static RefPtr<RTT> tryCreateFunction(const RTT& supertype, bool isFinalType, RTTFunctionPayload&&);
    static RefPtr<RTT> tryCreateStruct(bool isFinalType, RTTStructPayload&&);
    static RefPtr<RTT> tryCreateStruct(const RTT& supertype, bool isFinalType, RTTStructPayload&&);
    static RefPtr<RTT> tryCreateArray(bool isFinalType, RTTArrayPayload&&);
    static RefPtr<RTT> tryCreateArray(const RTT& supertype, bool isFinalType, RTTArrayPayload&&);

    RTTKind kind() const { return m_kind; }
    DisplayCount displaySizeExcludingThis() const { return m_displaySizeExcludingThis; }
    const RTT* displayEntry(DisplayCount i) const { return at(i).get(); }
    StructFieldCount fieldCount() const { return m_fieldCount; }

    // View this RTT pointer as a TypeIndex (concrete encoding: bit-cast of the RTT pointer).
    // Use when a Category-C API takes `TypeIndex`; every concrete TypeIndex is bit-identical
    // to `const RTT*`.
    TypeIndex asTypeIndex() const { return std::bit_cast<TypeIndex>(this); }

    const RTTFunctionPayload& functionPayload() const LIFETIME_BOUND { return std::get<RTTFunctionPayload>(m_payload); }
    const RTTStructPayload& structPayload() const LIFETIME_BOUND { return std::get<RTTStructPayload>(m_payload); }
    const RTTArrayPayload& arrayPayload() const LIFETIME_BOUND { return std::get<RTTArrayPayload>(m_payload); }

    // Function payload accessors.
    FunctionArgCount argumentCount() const { return functionPayload().argumentCount(); }
    FunctionArgCount returnCount() const { return functionPayload().returnCount(); }
    Type argumentType(FunctionArgCount i) const { return functionPayload().argumentType(i); }
    Type returnType(FunctionArgCount i) const { return functionPayload().returnType(i); }
    bool returnsVoid() const { return functionPayload().returnsVoid(); }
    bool argumentsOrResultsIncludeI64() const { return functionPayload().argumentsOrResultsIncludeI64(); }
    bool argumentsOrResultsIncludeV128() const { return functionPayload().argumentsOrResultsIncludeV128(); }
    bool argumentsOrResultsIncludeExnref() const { return functionPayload().argumentsOrResultsIncludeExnref(); }

    size_t numberOfV128() const
    {
        size_t n = 0;
        for (FunctionArgCount i = 0; i < argumentCount(); ++i) {
            if (argumentType(i).isV128())
                ++n;
        }
        return n;
    }

    size_t numberOfReturnedV128() const
    {
        size_t n = 0;
        for (FunctionArgCount i = 0; i < returnCount(); ++i) {
            if (returnType(i).isV128())
                ++n;
        }
        return n;
    }

    bool hasReturnedV128() const
    {
        for (FunctionArgCount i = 0; i < returnCount(); ++i) {
            if (returnType(i).isV128())
                return true;
        }
        return false;
    }

    // Struct payload accessors.
    const FieldType& field(StructFieldCount i) const LIFETIME_BOUND
    {
        ASSERT(m_kind == RTTKind::Struct);
        return structPayload().field(i);
    }
    unsigned offsetOfFieldInPayload(StructFieldCount i) const { return structPayload().offsetOfFieldInPayload(i); }
    size_t instancePayloadSize() const { return structPayload().instancePayloadSize(); }
    bool hasRefFieldTypes() const { return structPayload().hasRefFieldTypes(); }

    // Array payload accessors.
    const FieldType& elementType() const LIFETIME_BOUND { return arrayPayload().elementType(); }

    // Returns whether the payload references a type within its own recursion group.
    bool hasRecursiveReference() const
    {
        switch (m_kind) {
        case RTTKind::Function:
            return functionPayload().hasRecursiveReference();
        case RTTKind::Struct:
            return structPayload().hasRecursiveReference();
        case RTTKind::Array:
            return arrayPayload().hasRecursiveReference();
        }
        RELEASE_ASSERT_NOT_REACHED();
        return false;
    }

    const RTT& definingRTTForField(StructFieldCount fieldIndex) const
    {
        ASSERT(m_kind == RTTKind::Struct);
        ASSERT(fieldIndex < m_fieldCount);
        for (DisplayCount i = 0; i < m_displaySizeExcludingThis; ++i) {
            const RTT* ancestor = displayEntry(i);
            if (ancestor->kind() == RTTKind::Struct && fieldIndex < ancestor->fieldCount())
                return *ancestor;
        }
        return *this;
    }

    // Generate a key for each field, which can be usable for B3 TBAA.
    uint64_t fieldHeapKey(StructFieldCount fieldIndex) const
    {
        uint64_t ptr = std::bit_cast<uintptr_t>(this);
#if CPU(ADDRESS64)
        static_assert(maxStructFieldCount <= (1U << 20));
        constexpr uint32_t fieldIndexMask = (1 << 20) - 1;
        uint32_t maskedFieldIndex = fieldIndex & fieldIndexMask; // mod 20-bits.
        return static_cast<uint64_t>(ptr | (maskedFieldIndex & 0b1111) | (static_cast<uint64_t>(maskedFieldIndex >> 4) << 48));
#else
        return static_cast<uint64_t>(ptr | (static_cast<uint64_t>(fieldIndex) << 32));
#endif
    }

    bool NODELETE isSubRTT(const RTT& other) const;
    bool NODELETE isStrictSubRTT(const RTT& other) const;
    bool isFinalType() const { return m_isFinalType; }

    // Print this RTT for debugging.
    String toString() const;
    void dump(WTF::PrintStream& out) const;

#if ENABLE(JIT)
    // For function-kind RTTs: returns the JS->Wasm IC entrypoint for this signature.
    // The IC cache + lock live on the RTT itself.
    CodePtr<JSEntryPtrTag> jsToWasmICEntrypoint() const;
#endif

    // Function-kind only. Lazy-installed via a CAS in ThreadSafeLazyUniquePtr;
    // buffer is immutable once published.
    void ensureArgumINTBytecode(const CallInformation&) const;
    void ensureUINTBytecode(const CallInformation&) const;

    // Unified mINT call bytecode: [arg bytecode][Call][CallReturnMetadata][result bytecode][End].
    // MC walks through this buffer during the entire call -- arg dispatch leaves MC at
    // CallReturnMetadata start (callee-preserves MC), ret dispatch continues from there.
    void ensureCallBytecode() const;

    // Unified mINT tail-call bytecode: [arg bytecode][TailCall]. Tail calls don't return.
    void ensureTailCallBytecode() const;

    // Walk every ref-bearing TypeSlot in this RTT's payload, dispatching to
    // the per-payload visitor based on m_kind. Used by both
    // rewriteInternalRefs (sets each slot's anchor inline as it rewrites)
    // and clearReferencedRTTs (nulls each anchor).
    template<typename Callback>
    void visitChildrenRTT(Callback&& cb)
    {
        switch (m_kind) {
        case RTTKind::Function:
            std::get<RTTFunctionPayload>(m_payload).visitChildrenRTT(std::forward<Callback>(cb));
            return;
        case RTTKind::Struct:
            std::get<RTTStructPayload>(m_payload).visitChildrenRTT(std::forward<Callback>(cb));
            return;
        case RTTKind::Array:
            std::get<RTTArrayPayload>(m_payload).visitChildrenRTT(std::forward<Callback>(cb));
            return;
        }
        RELEASE_ASSERT_NOT_REACHED();
    }

    template<typename Callback>
    void forEachPayloadRTTRef(Callback&& cb) const
    {
        switch (m_kind) {
        case RTTKind::Function:
            std::get<RTTFunctionPayload>(m_payload).forEachPayloadRTTRef(std::forward<Callback>(cb));
            return;
        case RTTKind::Struct:
            std::get<RTTStructPayload>(m_payload).forEachPayloadRTTRef(std::forward<Callback>(cb));
            return;
        case RTTKind::Array:
            std::get<RTTArrayPayload>(m_payload).forEachPayloadRTTRef(std::forward<Callback>(cb));
            return;
        }
        RELEASE_ASSERT_NOT_REACHED();
    }

    // Rewrite internal recgroup refs in this RTT's payload to canonical RTT
    // pointers. Called during canonicalizeRecursionGroup before the candidate
    // RTTs are exposed: any Type ref whose index is a Projection of
    // recursionGroup is replaced with groupMembers[projectionIndex] (i.e.,
    // a self-reference in the post-canonicalization group). After this all
    // refs in payload are uniformly RTT* form.
    void rewriteInternalRefs(TypeSectionState*, std::span<const Ref<const RTT>> groupMembers, const RecursionGroup*);

    // Drop all RTT anchors. Called on the dupe-on-insert candidate and on
    // sweep-eligible canonical RTTs. Never call on a still-reachable
    // canonical RTT -- re-introduces the UAF rewriteInternalRefs guards.
    void clearReferencedRTTs();

    // Null every display slot. Sweep-time helper; breaks self- and
    // sibling-display cycles before the destructor cascade runs.
    void clearAllDisplayRefs();

    // Write `this` into span()[m_displaySizeExcludingThis]. Called exactly
    // once per RTT after successful canonicalization; deferring this write
    // until canonicalization means parse failures and dupe losers never
    // establish a self-reference cycle and can destruct cleanly.
    void setSelfDisplaySlot() const;

    // Set during canonicalization (under TypeInformation::m_lock) to identify
    // membership in a canonical recursion group without scanning the group's
    // members vector. Group identity is the RTTGroup pointer. nullptr = not
    // yet canonicalized. indexInGroup is the relative projection index
    // (0 for singletons, 0..size-1 for multi-member).
    // Mirrors V8's CanonicalTypeIndex + RecursionGroupRange arithmetic.
    void setCanonicalGroup(const RTTGroup* group, uint32_t indexInGroup) const
    {
        m_group = group;
        m_canonicalIndexInGroup = indexInGroup;
    }
    const RTTGroup* canonicalGroup() const { return m_group.get(); }
    uint32_t canonicalIndexInGroup() const { return m_canonicalIndexInGroup; }

    uint32_t virtualRefCount() const { return m_virtualRefCount; }
    void setVirtualRefCount(uint32_t value) const { m_virtualRefCount = value; }

    static constexpr ptrdiff_t offsetOfKind() { return OBJECT_OFFSETOF(RTT, m_kind); }
    static constexpr ptrdiff_t offsetOfDisplaySizeExcludingThis() { return OBJECT_OFFSETOF(RTT, m_displaySizeExcludingThis); }
    static constexpr ptrdiff_t offsetOfArgumINTBytecode() { return OBJECT_OFFSETOF(RTT, m_argumINTBytecode); }
    static constexpr ptrdiff_t offsetOfUINTBytecode() { return OBJECT_OFFSETOF(RTT, m_uINTBytecode); }
    static constexpr ptrdiff_t offsetOfCallBytecode() { return OBJECT_OFFSETOF(RTT, m_callBytecode); }
    static constexpr ptrdiff_t offsetOfTailCallBytecode() { return OBJECT_OFFSETOF(RTT, m_tailCallBytecode); }
    using TrailingArrayType::offsetOfData;

    unsigned hashMayBeEmpty() const
    {
        return m_hash;
    }

    void setHash(unsigned hash) const
    {
        if (!hash)
            m_hash = 1;
        else
            m_hash = hash;
    }

private:
    // Templated payload-aware constructors. Initialize m_payload in the
    // initializer list so the Variant is never observed in an unset state
    // (no std::monostate alternative). Defined inline so each tryCreate*
    // call site instantiates the matching specialization.
    // Display layout: span()[0..m_displaySizeExcludingThis-1] is the
    // ancestor chain; span()[m_displaySizeExcludingThis] is the self-slot,
    // left nullptr by the constructor and written only by
    // setSelfDisplaySlot() after canonicalization. Subtype constructor
    // copies supertype's ancestor slots and explicitly writes &supertype
    // as its own supertype-identity slot, because the supertype may still
    // be a non-canonical sibling at construction time.
    template<typename Payload>
    RTT(RTTKind kind, bool isFinalType, StructFieldCount fieldCount, Payload&& payload)
        : TrailingArrayType(std::max(1u, inlinedDisplaySize))
        , m_kind(kind)
        , m_isFinalType(isFinalType)
        , m_displaySizeExcludingThis(0)
        , m_fieldCount(fieldCount)
        , m_payload(std::forward<Payload>(payload))
    {
    }
    template<typename Payload>
    RTT(RTTKind kind, const RTT& supertype, bool isFinalType, StructFieldCount fieldCount, Payload&& payload)
        : TrailingArrayType(std::max(supertype.displaySizeExcludingThis() + 2, inlinedDisplaySize))
        , m_kind(kind)
        , m_isFinalType(isFinalType)
        , m_displaySizeExcludingThis(supertype.displaySizeExcludingThis() + 1)
        , m_fieldCount(fieldCount)
        , m_payload(std::forward<Payload>(payload))
    {
        for (size_t i = 0; i < supertype.displaySizeExcludingThis(); ++i)
            span()[i] = supertype.span()[i];
        at(supertype.displaySizeExcludingThis()) = &supertype;
    }

    const RTTKind m_kind;
    const bool m_isFinalType { false };
    const unsigned m_displaySizeExcludingThis { };
    const StructFieldCount m_fieldCount { 0 };
    mutable unsigned m_hash { 0 };
    mutable RefPtr<const RTTGroup> m_group { nullptr };
    mutable uint32_t m_canonicalIndexInGroup { 0 };
    mutable uint32_t m_virtualRefCount { 0 };
#if ENABLE(JIT)
    // Cache for the JS->Wasm IC entrypoint. Function-kind only; null for
    // struct/array. Lazy-initialized under m_jitCodeLock; once set, the IC
    // pointer is read without locking via the storeStoreFence in the writer.
    mutable RefPtr<JSToWasmICCallee> m_jsToWasmICCallee;
    mutable Lock m_jitCodeLock;
#endif
    mutable ThreadSafeLazyUniquePtr<const IPIntSharedBytecode> m_argumINTBytecode;
    mutable ThreadSafeLazyUniquePtr<const IPIntSharedBytecode> m_uINTBytecode;
    mutable ThreadSafeLazyUniquePtr<const IPIntSharedBytecode> m_callBytecode;
    mutable ThreadSafeLazyUniquePtr<const IPIntSharedBytecode> m_tailCallBytecode;
    Variant<RTTFunctionPayload, RTTStructPayload, RTTArrayPayload> m_payload;
};

// A canonical isorecursive recursion group: owns Refs to its N member RTTs
// and acts as the group's identity token. Each member RTT back-references
// this group via RTT::canonicalGroup() (raw pointer, set under the
// TypeInformation lock during canonicalization). This lets hash/equal detect
// intra-group refs in payload Types via pointer identity instead of needing
// a per-canonicalization id counter.
//
// Lifetime: owned by m_canonicalRecursionGroups; a member RTT's raw back-
// pointer is valid as long as this group is in the set (and, today, forever,
// because the set never evicts multi-member groups -- see tryCleanup). The
// destructor clears each member's back-pointer defensively so that a member
// surviving the group via an external Ref is correctly reported as
// "ungrouped" by canonicalGroup() == nullptr.
class RTTGroup final : public ThreadSafeRefCounted<RTTGroup> {
public:
    static Ref<const RTTGroup> create(Vector<Ref<const RTT>>&& rtts) { return adoptRef(*new RTTGroup(WTF::move(rtts))); }

    ~RTTGroup();

    const Vector<Ref<const RTT>>& rtts() const LIFETIME_BOUND { return m_rtts; }
    size_t size() const { return m_rtts.size(); }
    const RTT& at(size_t i) const LIFETIME_BOUND { return m_rtts[i]; }

    uint32_t virtualRefCount() const { return m_virtualRefCount; }
    void setVirtualRefCount(uint32_t value) const { m_virtualRefCount = value; }

    unsigned hashMayBeEmpty() const
    {
        return m_hash;
    }

    void setHash(unsigned hash) const
    {
        if (!hash)
            m_hash = 1;
        else
            m_hash = hash;
    }

private:
    explicit RTTGroup(Vector<Ref<const RTT>>&& rtts)
        : m_rtts(WTF::move(rtts))
    { }

    Vector<Ref<const RTT>> m_rtts;
    mutable uint32_t m_virtualRefCount { 0 };
    mutable unsigned m_hash { 0 };
};

// Isorecursive canonical recursion-group entry. Wraps an owning Ref to the
// RTTGroup so m_canonicalRecursionGroups keeps it alive; intra-group
// membership is tested by pointer equality against the group. The
// HashTableEmpty-constructed Ref is zero-bit-pattern (m_ptr == nullptr),
// so emptyValueIsZero still holds.
struct CanonicalRecursionGroupEntry {
    Ref<const RTTGroup> group;

    CanonicalRecursionGroupEntry()
        : group(WTF::HashTableEmptyValue)
    { }
    explicit CanonicalRecursionGroupEntry(Ref<const RTTGroup>&& g)
        : group(WTF::move(g))
    { }
    explicit CanonicalRecursionGroupEntry(WTF::HashTableDeletedValueType)
        : group(WTF::HashTableDeletedValue)
    { }
    bool isHashTableDeletedValue() const { return group.isHashTableDeletedValue(); }
    bool operator==(const CanonicalRecursionGroupEntry& other) const;
};

struct CanonicalRecursionGroupEntryHash {
    static unsigned hash(const CanonicalRecursionGroupEntry&);
    static bool equal(const CanonicalRecursionGroupEntry&, const CanonicalRecursionGroupEntry&);
    static constexpr bool safeToCompareToEmptyOrDeleted = false;
};

// Canonical entry for a size-1 recursion group. Fast-path analogue of
// CanonicalRecursionGroupEntry: hashed/compared by the single RTT's
// structural content, with self-refs detected by pointer equality against
// the entry's own RTT. Mirrors V8's canonical_singleton_groups_.
struct CanonicalSingletonEntry {
    Ref<const RTT> rtt;

    CanonicalSingletonEntry()
        : rtt(WTF::HashTableEmptyValue)
    { }
    explicit CanonicalSingletonEntry(Ref<const RTT>&& r)
        : rtt(WTF::move(r))
    { }
    explicit CanonicalSingletonEntry(WTF::HashTableDeletedValueType)
        : rtt(WTF::HashTableDeletedValue)
    { }
    bool isHashTableDeletedValue() const { return rtt.isHashTableDeletedValue(); }
    bool operator==(const CanonicalSingletonEntry& other) const;
};

struct CanonicalSingletonEntryHash {
    static unsigned hash(const CanonicalSingletonEntry&);
    static bool equal(const CanonicalSingletonEntry&, const CanonicalSingletonEntry&);
    static constexpr bool safeToCompareToEmptyOrDeleted = false;
};

} } // namespace JSC::Wasm


namespace WTF {

template<typename T> struct DefaultHash;
template<> struct DefaultHash<JSC::Wasm::CanonicalRecursionGroupEntry> : JSC::Wasm::CanonicalRecursionGroupEntryHash { };
template<> struct DefaultHash<JSC::Wasm::CanonicalSingletonEntry> : JSC::Wasm::CanonicalSingletonEntryHash { };

template<typename T> struct HashTraits;
template<> struct HashTraits<JSC::Wasm::CanonicalRecursionGroupEntry> : SimpleClassHashTraits<JSC::Wasm::CanonicalRecursionGroupEntry> {
    static constexpr bool emptyValueIsZero = true;
    // The entry's operator== dereferences group, so the HashSet's empty-slot
    // check can't fall back to operator==(entry, emptyValue()). Check the
    // Ref's HashTableEmptyValue sentinel directly.
    static constexpr bool hasIsEmptyValueFunction = true;
    static bool isEmptyValue(const JSC::Wasm::CanonicalRecursionGroupEntry& value) { return value.group.isHashTableEmptyValue(); }
};
template<> struct HashTraits<JSC::Wasm::CanonicalSingletonEntry> : SimpleClassHashTraits<JSC::Wasm::CanonicalSingletonEntry> {
    static constexpr bool emptyValueIsZero = true;
    static constexpr bool hasIsEmptyValueFunction = true;
    static bool isEmptyValue(const JSC::Wasm::CanonicalSingletonEntry& value) { return value.rtt.isHashTableEmptyValue(); }
};

} // namespace WTF


namespace JSC { namespace Wasm {

// Type information is held globally and shared by the entire process to allow all type definitions to be unique.
// This is required when wasm calls another wasm instance, and must work when modules are shared between multiple VMs.
class TypeInformation {
    WTF_MAKE_NONCOPYABLE(TypeInformation);
    WTF_MAKE_TZONE_ALLOCATED(TypeInformation);

    TypeInformation();

public:
    friend class ParserBase;
    friend class ParsedDef;
    friend class RTT;
    friend class SectionParser;
    friend class TypeSectionState;

    static TypeInformation& singleton();

    static const RTT& signatureForJSException();

    static Ref<const RTT> rttForFunction(const Vector<Type, 16>& returnTypes, const Vector<Type, 16>& argumentTypes);

    static bool isReferenceValueAssignable(JSValue, bool, TypeIndex);

    // External TypeIndex values for concrete types are RTT pointers (set up
    // by ModuleInformation::typeIndexFromTypeSignatureIndex / Tag::typeIndex /
    // WebAssemblyFunctionBase). This bit-casts directly, no lookup.
    static Ref<const RTT> getCanonicalRTT(TypeIndex);

    // The index passed to tryGetRTTRef may be an abstract type or
    // invalid, in which case nullptr is returned. The non-null result
    // assumes the index is a bare RTT pointer (the external/canonical
    // convention).
    static RefPtr<const RTT> tryGetRTT(TypeIndex);

    static void tryCleanup();

    // Total canonical entries currently retained. Used by tests.
    static size_t canonicalTypeCount();

private:
    static Ref<const RTT> typeDefinitionForFunction(const Vector<Type, 16>& returnTypes, const Vector<Type, 16>& argumentTypes);
    static Ref<const RTT> typeDefinitionForStruct(const Vector<FieldType>& fields);
    static Ref<const RTT> typeDefinitionForArray(FieldType);

    // Provider-based overloads: caller supplies a callable instead of a
    // Vector. The FixedVector inside the resulting RTT payload is built in
    // place via the provider, eliminating one heap allocation + one copy
    // compared to the Vector-based overloads. Used by
    // TypeSectionState::createCanonicalRTT's rebuild-with-substitution path
    // where the intermediate Vector was purely scaffolding.
    template<typename FieldProvider>
    static Ref<const RTT> typeDefinitionForStructFromProvider(StructFieldCount, FieldProvider&&);
    template<typename ReturnProvider, typename ArgProvider>
    static Ref<const RTT> typeDefinitionForFunctionFromProviders(FunctionArgCount retCount, ReturnProvider&&, FunctionArgCount argCount, ArgProvider&&);

    // Isorecursive RTT canonicalization at recursion-group granularity.
    // Given a freshly-parsed recursion group identified by recursionGroup
    // and the per-member candidate RTTs auto-registered for it, return the
    // canonical RTT vector. If a structurally-identical recursion group is
    // already canonical, the candidates are discarded and the existing canonical
    // RTTs are returned (so cross-module identical recgroups share RTT identity).
    // Otherwise the candidates become canonical for this signature.
    //
    // Equality treats refs to projections of recursionGroup by their
    // relative projection index; refs outside the group are compared by
    // canonical RTT pointer. Mirrors V8's CanonicalEquality.
    static Vector<Ref<const RTT>> canonicalizeRecursionGroup(TypeSectionState*, const RecursionGroup*, Vector<Ref<const RTT>>&& candidateRTTs);

    // Fast path for the common case of a single-member recursion group
    // (standalone struct/array/function, shorthand types, self-recursive
    // singletons). Bypasses the Vector<Ref<const RTT>> allocation and goes
    // directly to the singleton canonical table. recursionGroup is the
    // original recursion-group TypeIndex used by placeholder projections in
    // the candidate's payload; pass 0 for non-recursive standalone types.
    static Ref<const RTT> canonicalizeSingleton(TypeSectionState*, Ref<const RTT>&& candidate);
    static Ref<const RTT> canonicalizeSingleton(TypeSectionState*, const RecursionGroup*, Ref<const RTT>&& candidate);

    // Canonicalize a single non-recursive RTT (function/struct/array). If a
    // structurally-identical RTT is already canonical, returns it; otherwise the
    // input becomes canonical. Used by TypeInformation initialization for
    // built-in signatures (m_Void_Externref, thunks) so they share identity with
    // RTTs minted later by parseType.
    static Ref<const RTT> canonicalizeStandaloneRTT(Ref<const RTT>&&);

    // Non-static impls (avoid singleton() recursion when called from
    // TypeInformation's own constructor).
    Vector<Ref<const RTT>> canonicalizeRecursionGroupImpl(TypeSectionState*, const RecursionGroup*, Vector<Ref<const RTT>>&&);
    Ref<const RTT> canonicalizeSingletonImpl(TypeSectionState*, const RecursionGroup*, Ref<const RTT>&&);
    Ref<const RTT> canonicalizeStandaloneRTTImpl(Ref<const RTT>&&);
    static RefPtr<const RTT> extractExternalRTT(Type);

    // Null every cycle-forming ref on `rtt` (display, payload, m_group) so
    // its refcount can cascade to 0. Used by tryCleanup's sweep and the
    // dupe-on-insert fallbacks.
    static void breakCyclesForReclamation(const RTT&);

    // Returns true iff `type` is a ref whose index encodes a parser-time
    // placeholder Projection (i.e. an unresolved intra-recgroup reference).
    // Canonical (post-parse) Type::index values are RTT pointers and return
    // false. Centralised here so consumers don't need to know about the
    // placeholder-tag-bit encoding.
    static bool isRefWithRecursiveReference(Type);
    static bool isRefWithRecursiveReference(StorageType);

    UncheckedKeyHashSet<CanonicalRecursionGroupEntry, CanonicalRecursionGroupEntryHash> m_canonicalRecursionGroups;
    UncheckedKeyHashSet<CanonicalSingletonEntry> m_canonicalSingletonGroups;
    RefPtr<const RTT> m_Void_Externref;
    Lock m_lock;
};

} } // namespace JSC::Wasm

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif // ENABLE(WEBASSEMBLY)
