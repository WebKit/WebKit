#!/usr/bin/env python3

# Copyright (C) 2016-2017 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# This tool has a couple of helpful macros to process Wasm files from the wasm.json.

from generateWasm import *
import optparse
import sys

parser = optparse.OptionParser(usage="usage: %prog <wasm.json> <WasmOps.h>")
(options, args) = parser.parse_args(sys.argv[0:])
if len(args) != 3:
    parser.error(parser.usage)

wasm = Wasm(args[0], args[1])
types = wasm.types
opcodes = wasm.opcodes
wasmOpsHFile = open(args[2], "w")


def cppType(type):
    if type == "bool":
        return "I32"
    return type.capitalize()


def cppMacro(wasmOpcode, value, b3, inc, *extraArgs):
    extraArgsStr = ", " + ", ".join(extraArgs) if len(extraArgs) else ""
    return " \\\n    macro(" + wasm.toCpp(wasmOpcode) + ", " + hex(int(value)) + ", " + b3 + ", " + str(inc) + extraArgsStr + ")"


def cppMacroPacked(wasmOpcode, value):
    return " \\\n    macro(" + wasm.toCpp(wasmOpcode) + ", " + hex(int(value)) + ")"


def typeMacroizer():
    inc = 0
    for ty in wasm.types:
        yield cppMacro(ty, wasm.types[ty]["value"], wasm.types[ty]["b3type"], inc, ty, str(wasm.types[ty]["width"]))
        inc += 1


def packedTypeMacroizer():
    for ty in wasm.packed_types:
        yield cppMacroPacked(ty, wasm.packed_types[ty]["value"])


def typeMacroizerFiltered(filter):
    for t in typeMacroizer():
        if not filter(t):
            yield t

type_definitions = ["#define FOR_EACH_WASM_TYPE(macro)"]
type_definitions.extend([t for t in typeMacroizer()])
type_definitions.extend(["\n\n#define FOR_EACH_WASM_PACKED_TYPE(macro)"])
type_definitions.extend([t for t in packedTypeMacroizer()])
type_definitions = "".join(type_definitions)

type_definitions_except_funcref_externref = ["#define FOR_EACH_WASM_TYPE_EXCEPT_FUNCREF_AND_EXTERNREF(macro)"]
type_definitions_except_funcref_externref.extend([t for t in typeMacroizerFiltered(lambda x: x == "funcref" or x == "externref")])
type_definitions_except_funcref_externref = "".join(type_definitions_except_funcref_externref)

min_type_value = min(wasm.types.items(), key=lambda pair: pair[1]['value'])[1]['value']

def opcodeMacroizer(filter, opcodeField="value", modifier=None):
    inc = 0
    for op in wasm.opcodeIterator(filter):
        b3op = "Oops"
        if isSimple(op["opcode"]):
            b3op = op["opcode"]["b3op"]
        extraArgs = []
        if modifier:
            extraArgs = modifier(op["opcode"])
        yield cppMacro(op["name"], op["opcode"][opcodeField], b3op, inc, *extraArgs)
        inc += 1


def opcodeWithTypesMacroizer(filter):
    def modifier(op):
        return [cppType(type) for type in op["parameter"] + op["return"]]
    return opcodeMacroizer(filter, modifier=modifier)

def memoryLoadMacroizer():
    def modifier(op):
        return [cppType(op["return"][0])]
    return opcodeMacroizer(lambda op: (op["category"] == "memory" and len(op["return"]) == 1), modifier=modifier)


def memoryStoreMacroizer():
    def modifier(op):
        return [cppType(op["parameter"][1])]
    return opcodeMacroizer(lambda op: (op["category"] == "memory" and len(op["return"]) == 0), modifier=modifier)


def saturatedTruncMacroizer():
    def modifier(op):
        return [cppType(type) for type in op["parameter"] + op["return"]]
    return opcodeMacroizer(lambda op: (op["category"] == "conversion" and op["value"] == 0xfc), modifier=modifier, opcodeField="extendedOp")


def wideArithmeticMacroizer():
    def modifier(op):
        return [cppType(type) for type in op["parameter"] + op["return"]]
    return opcodeMacroizer(lambda op: (op["category"] == "widearithmetic"), modifier=modifier, opcodeField="extendedOp")


def atomicMemoryLoadMacroizer():
    def modifier(op):
        return [cppType(op["return"][0])]
    return opcodeMacroizer(lambda op: isAtomicLoad(op), modifier=modifier, opcodeField="extendedOp")


def atomicMemoryStoreMacroizer():
    def modifier(op):
        return [cppType(op["parameter"][1])]
    return opcodeMacroizer(lambda op: isAtomicStore(op), modifier=modifier, opcodeField="extendedOp")


def atomicBinaryRMWMacroizer():
    def modifier(op):
        return [cppType(op["parameter"][1])]
    return opcodeMacroizer(lambda op: isAtomicBinaryRMW(op), modifier=modifier, opcodeField="extendedOp")


defines = ["#define FOR_EACH_WASM_SPECIAL_OP(macro)"]
defines.extend([op for op in opcodeMacroizer(lambda op: not (isUnary(op) or isBinary(op) or op["category"] == "control" or op["category"] == "memory" or op["value"] == 0xfc or op["category"] == "gc" or isAtomic(op)))])
defines.append("\n\n#define FOR_EACH_WASM_CONTROL_FLOW_OP(macro)")
defines.extend([op for op in opcodeMacroizer(lambda op: op["category"] == "control")])
defines.append("\n\n#define FOR_EACH_WASM_NON_COMPARE_UNARY_OP(macro)")
defines.extend([op for op in opcodeWithTypesMacroizer(lambda op: isUnary(op) and not isCompare(op))])
defines.append("\n\n#define FOR_EACH_WASM_COMPARE_UNARY_OP(macro)")
defines.extend([op for op in opcodeWithTypesMacroizer(lambda op: isUnary(op) and isCompare(op))])
defines.append("\n\n#define FOR_EACH_WASM_UNARY_OP(macro) \\\n    FOR_EACH_WASM_NON_COMPARE_UNARY_OP(macro) \\\n    FOR_EACH_WASM_COMPARE_UNARY_OP(macro)")
defines.append("\n\n#define FOR_EACH_WASM_NON_COMPARE_BINARY_OP(macro)")
defines.extend([op for op in opcodeWithTypesMacroizer(lambda op: isBinary(op) and not isCompare(op))])
defines.append("\n\n#define FOR_EACH_WASM_COMPARE_BINARY_OP(macro)")
defines.extend([op for op in opcodeWithTypesMacroizer(lambda op: isBinary(op) and isCompare(op))])
defines.append("\n\n#define FOR_EACH_WASM_BINARY_OP(macro) \\\n    FOR_EACH_WASM_NON_COMPARE_BINARY_OP(macro) \\\n    FOR_EACH_WASM_COMPARE_BINARY_OP(macro)")
defines.append("\n\n#define FOR_EACH_WASM_MEMORY_LOAD_OP(macro)")
defines.extend([op for op in memoryLoadMacroizer()])
defines.append("\n\n#define FOR_EACH_WASM_MEMORY_STORE_OP(macro)")
defines.extend([op for op in memoryStoreMacroizer()])
defines.append("\n\n#define FOR_EACH_WASM_TABLE_OP(macro)")
defines.extend([op for op in opcodeMacroizer(lambda op: (op["category"] == "exttable"), opcodeField="extendedOp")])
defines.append("\n\n#define FOR_EACH_WASM_TRUNC_SATURATED_OP(macro)")
defines.extend([op for op in saturatedTruncMacroizer()])
defines.append("\n\n#define FOR_EACH_WASM_WIDE_ARITHMETIC_OP(macro)")
defines.extend([op for op in wideArithmeticMacroizer()])
defines.append("\n\n#define FOR_EACH_WASM_EXT_ATOMIC_LOAD_OP(macro)")
defines.extend([op for op in atomicMemoryLoadMacroizer()])
defines.append("\n\n#define FOR_EACH_WASM_EXT_ATOMIC_STORE_OP(macro)")
defines.extend([op for op in atomicMemoryStoreMacroizer()])
defines.append("\n\n#define FOR_EACH_WASM_EXT_ATOMIC_BINARY_RMW_OP(macro)")
defines.extend([op for op in atomicBinaryRMWMacroizer()])
defines.append("\n\n#define FOR_EACH_WASM_EXT_ATOMIC_OTHER_OP(macro)")
defines.extend([op for op in opcodeMacroizer(lambda op: isAtomic(op) and (not isAtomicLoad(op) and not isAtomicStore(op) and not isAtomicBinaryRMW(op)), opcodeField="extendedOp")])
defines.append("\n\n#define FOR_EACH_WASM_GC_OP(macro)")
defines.extend([op for op in opcodeMacroizer(lambda op: (op["category"] == "gc"), opcodeField="extendedOp")])
defines.append("\n\n")

defines = "".join(defines)

opValueSet = set([op for op in wasm.opcodeIterator(lambda op: True, lambda op: opcodes[op]["value"])])
opValueSet.add(0xFD)  # ExtSIMD
maxOpValue = max(opValueSet)


# Luckily, python does floor division rather than trunc division so this works.
def ceilDiv(a, b):
    return -(-a // b)


def bitSet():
    v = ""
    for i in range(ceilDiv(maxOpValue + 1, 8)):
        entry = 0
        for j in range(8):
            if i * 8 + j in opValueSet:
                entry |= 1 << j
        v += (", " if i else "") + hex(entry)
    return v

validOps = bitSet()


def memoryLog2AlignmentGenerator(filter):
    result = []
    for op in wasm.opcodeIterator(filter):
        result.append("    case " + wasm.toCpp(op["name"]) + ": return " + memoryLog2Alignment(op) + ";")
    return "\n".join(result)


def atomicMemoryLog2AlignmentGenerator(filter):
    result = []
    for op in wasm.opcodeIterator(filter):
        result.append("    case ExtAtomicOpType::" + wasm.toCpp(op["name"]) + ": return " + memoryLog2Alignment(op) + ";")
    return "\n".join(result)


memoryLog2AlignmentLoads = memoryLog2AlignmentGenerator(lambda op: (op["category"] == "memory" and len(op["return"]) == 1))
memoryLog2AlignmentStores = memoryLog2AlignmentGenerator(lambda op: (op["category"] == "memory" and len(op["return"]) == 0))
memoryLog2AlignmentAtomic = atomicMemoryLog2AlignmentGenerator(lambda op: (isAtomic(op)))

contents = wasm.header + """

#pragma once

#include <wtf/Compiler.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

#include <wtf/Platform.h>

#if ENABLE(WEBASSEMBLY)

#include <cstdint>
#include <wtf/CompactPointerTuple.h>
#include <wtf/PrintStream.h>
#include <wtf/TaggedPtr.h>
#include <wtf/text/ASCIILiteral.h>

namespace JSC {

enum class Width : uint8_t;

namespace Wasm {

static constexpr unsigned expectedVersionNumber = """ + wasm.expectedVersionNumber + """;

static constexpr unsigned numTypes = """ + str(len(types)) + """;

static constexpr int minTypeValue = """ + str(min_type_value) + """;
""" + type_definitions + "\n" + """
""" + type_definitions_except_funcref_externref + """
#define CREATE_ENUM_VALUE(name, id, ...) name = id,
enum class TypeKind : int8_t {
    FOR_EACH_WASM_TYPE(CREATE_ENUM_VALUE)
};
#undef CREATE_ENUM_VALUE

#define CREATE_ENUM_VALUE(name, id) name = id,
enum class PackedType: int8_t {
    FOR_EACH_WASM_PACKED_TYPE(CREATE_ENUM_VALUE)
};
#undef CREATE_ENUM_VALUE

using TypeIndex = uintptr_t;

#define FOR_EACH_WASM_ABSTRACT_HEAP_TYPE_INDEX_TAG(macro) \\
    macro(Noexnref) \\
    macro(Nofuncref) \\
    macro(Noexternref) \\
    macro(Noneref) \\
    macro(Funcref) \\
    macro(Externref) \\
    macro(Anyref) \\
    macro(Eqref) \\
    macro(I31ref) \\
    macro(Structref) \\
    macro(Arrayref) \\
    macro(Exnref)

enum class TypeIndexTag : uint8_t {
    Concrete = 0,
#define CREATE_TYPE_INDEX_TAG(name) name,
    FOR_EACH_WASM_ABSTRACT_HEAP_TYPE_INDEX_TAG(CREATE_TYPE_INDEX_TAG)
#undef CREATE_TYPE_INDEX_TAG
    NumberOfTags,
};

// EnumTaggingTraits uses 4 high bits on ADDRESS64.
static_assert(static_cast<unsigned>(TypeIndexTag::NumberOfTags) <= 16);

// Stand-in for EnumTaggingTraits (needs complete type + alignof). alignas(16) matches RTT.
// Also used as CompactPointerTuple's pointer type for Type's payload (needs allowCompactPointers).
struct alignas(16) TypeIndexTagStorage {
    WTF_ALLOW_STRUCT_COMPACT_POINTERS;
};
using TypeIndexTaggingTraits = EnumTaggingTraits<TypeIndexTagStorage, TypeIndexTag, TypeIndexTag::Concrete>;

inline TypeIndexTag typeKindToTypeIndexTag(TypeKind kind)
{
    switch (kind) {
#define CREATE_CASE(name) case TypeKind::name: return TypeIndexTag::name;
    FOR_EACH_WASM_ABSTRACT_HEAP_TYPE_INDEX_TAG(CREATE_CASE)
#undef CREATE_CASE
    default:
        RELEASE_ASSERT_NOT_REACHED();
        return TypeIndexTag::Concrete;
    }
}

inline TypeKind typeIndexTagToTypeKind(TypeIndexTag tag)
{
    switch (tag) {
#define CREATE_CASE(name) case TypeIndexTag::name: return TypeKind::name;
    FOR_EACH_WASM_ABSTRACT_HEAP_TYPE_INDEX_TAG(CREATE_CASE)
#undef CREATE_CASE
    case TypeIndexTag::Concrete:
    case TypeIndexTag::NumberOfTags:
        RELEASE_ASSERT_NOT_REACHED();
        return TypeKind::Void;
    }
    RELEASE_ASSERT_NOT_REACHED();
    return TypeKind::Void;
}

inline bool isAbstractHeapTypeKind(TypeKind kind)
{
    switch (kind) {
#define CREATE_CASE(name) case TypeKind::name: return true;
    FOR_EACH_WASM_ABSTRACT_HEAP_TYPE_INDEX_TAG(CREATE_CASE)
#undef CREATE_CASE
    default:
        return false;
    }
}

inline TypeIndex typeIndexFromTypeKind(TypeKind kind)
{
    RELEASE_ASSERT(isAbstractHeapTypeKind(kind));
#if CPU(ADDRESS64)
    return TypeIndexTaggingTraits::wrap(nullptr, typeKindToTypeIndexTag(kind));
#else
    return static_cast<TypeIndex>(kind);
#endif
}

inline bool isAbstractTypeIndex(TypeIndex index)
{
#if CPU(ADDRESS64)
    return TypeIndexTaggingTraits::unwrapTag(index) != TypeIndexTag::Concrete;
#else
    auto signedIndex = static_cast<std::make_signed_t<TypeIndex>>(index);
    return (signedIndex < 0) && (signedIndex > minTypeValue);
#endif
}

inline TypeKind typeIndexAsTypeKind(TypeIndex index)
{
    ASSERT(isAbstractTypeIndex(index));
#if CPU(ADDRESS64)
    return typeIndexTagToTypeKind(TypeIndexTaggingTraits::unwrapTag(index));
#else
    return static_cast<TypeKind>(index);
#endif
}

// CompactPointerTuple packs pointer + uint8 kind bits on ADDRESS64 (one word);
// on 32-bit it is a real { pointer, type } pair (same API).
// TypeKind is a signed int8_t enum, so the tag is the uint8_t bit pattern.
//   Bare kinds — pointer null, tag = TypeKind bits
//   Concrete ref/ref_null — pointer = TypeIndex (RTT* / parse-time tagged ptr), tag = Ref/RefNull
//   Abstract ref/ref_null — pointer holds TypeIndexTag (1..NumberOfTags-1), tag = Ref/RefNull
// Abstract TypeIndex may use EnumTaggingTraits high tags on ADDRESS64 and cannot live in the
// pointer half as a full tagged index, so abstract heaps store only TypeIndexTag and rebuild
// in index().
struct Type {
    using Payload = CompactPointerTuple<TypeIndexTagStorage*, uint8_t>;

    constexpr Type()
        : m_data(nullptr, encodeKind(TypeKind::Void))
    {
    }

    Type(TypeKind typeKind, TypeIndex typeIndex = 0)
    {
        set(typeKind, typeIndex);
    }

    static constexpr Type fromBareKind(TypeKind typeKind)
    {
        return Type { Payload { nullptr, encodeKind(typeKind) } };
    }

    void set(TypeKind typeKind, TypeIndex typeIndex)
    {
        if (typeKind == TypeKind::Ref || typeKind == TypeKind::RefNull) {
            if (isAbstractTypeIndex(typeIndex)) {
                auto tag = typeKindToTypeIndexTag(typeIndexAsTypeKind(typeIndex));
                ASSERT(static_cast<unsigned>(tag) > static_cast<unsigned>(TypeIndexTag::Concrete));
                ASSERT(static_cast<unsigned>(tag) < static_cast<unsigned>(TypeIndexTag::NumberOfTags));
                m_data = Payload { std::bit_cast<TypeIndexTagStorage*>(static_cast<uintptr_t>(tag)), encodeKind(typeKind) };
                return;
            }
            m_data = Payload { std::bit_cast<TypeIndexTagStorage*>(typeIndex), encodeKind(typeKind) };
            return;
        }
        ASSERT(!typeIndex);
        m_data = Payload { nullptr, encodeKind(typeKind) };
    }

    constexpr TypeKind kind() const { return decodeKind(m_data.type()); }

    constexpr TypeIndex index() const
    {
        TypeKind typeKind = kind();
        if (typeKind != TypeKind::Ref && typeKind != TypeKind::RefNull)
            return 0;

        // ADDRESS64: data()+mask avoids bit_cast to/from pointers in constexpr.
        // 32-bit CompactPointerTuple has no packed data(); pointer() is fine.
#if CPU(ADDRESS64)
        uintptr_t payload = static_cast<uintptr_t>(m_data.data() & Payload::pointerMask);
#else
        uintptr_t payload = std::bit_cast<uintptr_t>(m_data.pointer());
#endif
        if (payload && payload < static_cast<uintptr_t>(TypeIndexTag::NumberOfTags))
            return typeIndexFromTypeKind(typeIndexTagToTypeKind(static_cast<TypeIndexTag>(payload)));
        return static_cast<TypeIndex>(payload);
    }

    constexpr bool operator==(const Type& other) const { return m_data == other.m_data; }

private:
    explicit constexpr Type(Payload data)
        : m_data(data)
    {
    }

    static constexpr uint8_t encodeKind(TypeKind typeKind)
    {
        return static_cast<uint8_t>(static_cast<int8_t>(typeKind));
    }

    static constexpr TypeKind decodeKind(uint8_t bits)
    {
        return static_cast<TypeKind>(static_cast<int8_t>(bits));
    }

    Payload m_data;
#if CPU(ADDRESS64)
    static_assert(sizeof(Payload) == sizeof(void*));
#endif
public:

    bool isNullable() const
    {
        TypeKind typeKind = kind();
        return typeKind == TypeKind::RefNull || typeKind == TypeKind::Externref || typeKind == TypeKind::Funcref;
    }

    // Saying conservatively.
    bool definitelyIsCellOrNull() const;
    bool definitelyIsWasmGCObjectOrNull() const;

    void dump(PrintStream& out) const;
    Width width() const;

    // Use Wasm::isFuncref and Wasm::isExternref instead because they check against all kinds of representations of function references and external references.

#define CREATE_PREDICATE(name, ...) bool is ## name() const { return kind() == TypeKind::name; }
    FOR_EACH_WASM_TYPE_EXCEPT_FUNCREF_AND_EXTERNREF(CREATE_PREDICATE)
#undef CREATE_PREDICATE

    bool isGP64() const
    {
        switch (kind()) {
        case TypeKind::I64:
        case TypeKind::Funcref:
        case TypeKind::Exnref:
        case TypeKind::Externref:
        case TypeKind::RefNull:
        case TypeKind::Ref:
            return true;
        default:
            return false;
        }
    }
};

#if CPU(ADDRESS64)
static_assert(sizeof(Type) == sizeof(void*));
#endif

namespace Types
{
#define CREATE_CONSTANT(name, id, ...) constexpr Type name = Type::fromBareKind(TypeKind::name);
FOR_EACH_WASM_TYPE(CREATE_CONSTANT)
#undef CREATE_CONSTANT
#if USE(JSVALUE64)
constexpr Type IPtr = I64;
#elif USE(JSVALUE32_64)
constexpr Type IPtr = I32;
#endif
} // namespace Types

static_assert(Types::I32.kind() == TypeKind::I32);
static_assert(Types::I32.index() == 0);
static_assert(Types::Funcref.kind() == TypeKind::Funcref);
static_assert(Types::Funcref.index() == 0);
static_assert(Type::fromBareKind(TypeKind::Ref).kind() == TypeKind::Ref);
static_assert(Type::fromBareKind(TypeKind::Ref).index() == 0);
static_assert(Type::fromBareKind(TypeKind::RefNull).kind() == TypeKind::RefNull);
static_assert(Types::Funcref != Type::fromBareKind(TypeKind::RefNull));

#define CREATE_CASE(name, id, ...) case id: return true;
template <typename Int>
inline bool isValidTypeKind(Int i)
{
    switch (i) {
    default: return false;
    FOR_EACH_WASM_TYPE(CREATE_CASE)
    }
    RELEASE_ASSERT_NOT_REACHED();
    return false;
}
#undef CREATE_CASE

#define CREATE_CASE(name, id, ...) case id: return true;
template <typename Int>
inline bool isValidPackedType(Int i)
{
    switch (i) {
    default: return false;
    FOR_EACH_WASM_PACKED_TYPE(CREATE_CASE)
    }
    RELEASE_ASSERT_NOT_REACHED();
    return false;
}
#undef CREATE_CASE

#define CREATE_CASE(name, ...) case TypeKind::name: return #name ## _s;
inline ASCIILiteral makeString(TypeKind kind)
{
    switch (kind) {
    FOR_EACH_WASM_TYPE(CREATE_CASE)
    }
    RELEASE_ASSERT_NOT_REACHED();
    return { };
}
#undef CREATE_CASE

#define CREATE_CASE(name, ...) case PackedType::name: return #name ## _s;
inline ASCIILiteral makeString(PackedType packedType)
{
    switch (packedType) {
    FOR_EACH_WASM_PACKED_TYPE(CREATE_CASE)
    }
    RELEASE_ASSERT_NOT_REACHED();
    return { };
}
#undef CREATE_CASE

#define CREATE_CASE(name, id, b3type, inc, ...) case TypeKind::name: return inc;
inline int linearizeType(TypeKind kind)
{
    switch (kind) {
    FOR_EACH_WASM_TYPE(CREATE_CASE)
    }
    RELEASE_ASSERT_NOT_REACHED();
    return 0;
}
#undef CREATE_CASE

#define CREATE_CASE(name, id, b3type, inc, ...) case inc: return TypeKind::name;
inline TypeKind linearizedToType(int i)
{
    switch (i) {
    FOR_EACH_WASM_TYPE(CREATE_CASE)
    }
    RELEASE_ASSERT_NOT_REACHED();
    return TypeKind::Void;
}
#undef CREATE_CASE


""" + defines + """
#define FOR_EACH_WASM_EXT_PREFIX_OP_WITH_ENUM(macro) \\
    macro(ExtGC, 0xFB, Oops, 0, ExtGCOpType) \\
    macro(Ext1, 0xFC, Oops, 0, Ext1OpType) \\
    macro(ExtSIMD, 0xFD, Oops, 0, ExtSIMDOpType) \\
    macro(ExtAtomic, 0xFE, Oops, 0, ExtAtomicOpType)

#define FOR_EACH_WASM_OP(macro) \\
    FOR_EACH_WASM_SPECIAL_OP(macro) \\
    FOR_EACH_WASM_CONTROL_FLOW_OP(macro) \\
    FOR_EACH_WASM_UNARY_OP(macro) \\
    FOR_EACH_WASM_BINARY_OP(macro) \\
    FOR_EACH_WASM_MEMORY_LOAD_OP(macro) \\
    FOR_EACH_WASM_MEMORY_STORE_OP(macro) \\
    FOR_EACH_WASM_EXT_PREFIX_OP_WITH_ENUM(macro)

#define CREATE_ENUM_VALUE(name, id, ...) name = id,

enum OpType : uint8_t {
    FOR_EACH_WASM_OP(CREATE_ENUM_VALUE)
};

template<typename Int>
inline bool isValidOpType(Int i)
{
    // Bitset of valid ops.
    static const uint8_t valid[] = { """ + validOps + """ };
    return 0 <= i && i <= """ + str(maxOpValue) + """ && (valid[i / 8] & (1 << (i % 8)));
}

enum class BinaryOpType : uint8_t {
    FOR_EACH_WASM_BINARY_OP(CREATE_ENUM_VALUE)
};

enum class UnaryOpType : uint8_t {
    FOR_EACH_WASM_UNARY_OP(CREATE_ENUM_VALUE)
};

enum class LoadOpType : uint8_t {
    FOR_EACH_WASM_MEMORY_LOAD_OP(CREATE_ENUM_VALUE)
};

enum class StoreOpType : uint8_t {
    FOR_EACH_WASM_MEMORY_STORE_OP(CREATE_ENUM_VALUE)
};

enum class Ext1OpType : uint32_t {
    FOR_EACH_WASM_TABLE_OP(CREATE_ENUM_VALUE)
    FOR_EACH_WASM_TRUNC_SATURATED_OP(CREATE_ENUM_VALUE)
    FOR_EACH_WASM_WIDE_ARITHMETIC_OP(CREATE_ENUM_VALUE)
};

enum class ExtSIMDOpType : uint32_t;

enum class ExtGCOpType : uint32_t {
    FOR_EACH_WASM_GC_OP(CREATE_ENUM_VALUE)
};

enum class ExtAtomicOpType : uint32_t {
    FOR_EACH_WASM_EXT_ATOMIC_LOAD_OP(CREATE_ENUM_VALUE)
    FOR_EACH_WASM_EXT_ATOMIC_STORE_OP(CREATE_ENUM_VALUE)
    FOR_EACH_WASM_EXT_ATOMIC_BINARY_RMW_OP(CREATE_ENUM_VALUE)
    FOR_EACH_WASM_EXT_ATOMIC_OTHER_OP(CREATE_ENUM_VALUE)
};

#undef CREATE_ENUM_VALUE

inline bool isControlOp(OpType op)
{
    switch (op) {
#define CREATE_CASE(name, ...) case OpType::name:
    FOR_EACH_WASM_CONTROL_FLOW_OP(CREATE_CASE)
        return true;
#undef CREATE_CASE
    default:
        break;
    }
    return false;
}

inline bool isControlFlowInstruction(OpType op)
{
    switch (op) {
#define CREATE_CASE(name, ...) case OpType::name:
    FOR_EACH_WASM_CONTROL_FLOW_OP(CREATE_CASE)
        return true;
#undef CREATE_CASE
    default:
        break;
    }
    return false;
}

// Enhanced version that includes ExtGC branch operations
// This function requires access to the current extended opcode for ExtGC operations
template<typename ExtendedOpcodeProvider>
inline bool isControlFlowInstructionWithExtGC(OpType op, ExtendedOpcodeProvider&& getExtendedOpcode)
{
    if (isControlFlowInstruction(op))
        return true;

    if (op == OpType::ExtGC) {
        uint32_t extOp = getExtendedOpcode();
        return extOp == static_cast<uint32_t>(ExtGCOpType::BrOnCast) || extOp == static_cast<uint32_t>(ExtGCOpType::BrOnCastFail);
    }

    return false;
}

inline uint32_t memoryLog2Alignment(OpType op)
{
    switch (op) {
""" + memoryLog2AlignmentLoads + """
""" + memoryLog2AlignmentStores + """
    default:
        break;
    }
    RELEASE_ASSERT_NOT_REACHED();
    return 0;
}

inline uint32_t memoryLog2Alignment(ExtAtomicOpType op)
{
    switch (op) {
""" + memoryLog2AlignmentAtomic + """
    default:
        break;
    }
    RELEASE_ASSERT_NOT_REACHED();
    return 0;
}

#define CREATE_CASE(name, ...) case name: return #name ## _s;
inline ASCIILiteral makeString(OpType op)
{
    switch (op) {
    FOR_EACH_WASM_OP(CREATE_CASE)
    }
    RELEASE_ASSERT_NOT_REACHED();
    return { };
}
#undef CREATE_CASE

#define CREATE_CASE(name, ...) case Ext1OpType::name: return #name ## _s;
inline ASCIILiteral makeString(Ext1OpType op)
{
    switch (op) {
    FOR_EACH_WASM_TABLE_OP(CREATE_CASE)
    FOR_EACH_WASM_TRUNC_SATURATED_OP(CREATE_CASE)
    FOR_EACH_WASM_WIDE_ARITHMETIC_OP(CREATE_CASE)
    }
    RELEASE_ASSERT_NOT_REACHED();
    return { };
}
#undef CREATE_CASE

#define CREATE_CASE(name, ...) case ExtGCOpType::name: return #name ## _s;
inline ASCIILiteral makeString(ExtGCOpType op)
{
    switch (op) {
    FOR_EACH_WASM_GC_OP(CREATE_CASE)
    }
    RELEASE_ASSERT_NOT_REACHED();
    return { };
}
#undef CREATE_CASE

#define CREATE_CASE(name, ...) case ExtAtomicOpType::name: return #name ## _s;
inline ASCIILiteral makeString(ExtAtomicOpType op)
{
    switch (op) {
    FOR_EACH_WASM_EXT_ATOMIC_LOAD_OP(CREATE_CASE)
    FOR_EACH_WASM_EXT_ATOMIC_STORE_OP(CREATE_CASE)
    FOR_EACH_WASM_EXT_ATOMIC_BINARY_RMW_OP(CREATE_CASE)
    FOR_EACH_WASM_EXT_ATOMIC_OTHER_OP(CREATE_CASE)
    }
    RELEASE_ASSERT_NOT_REACHED();
    return { };
}
#undef CREATE_CASE

} } // namespace JSC::Wasm

namespace WTF {

inline void printInternal(PrintStream& out, JSC::Wasm::TypeKind kind)
{
    out.print(JSC::Wasm::makeString(kind));
}

inline void printInternal(PrintStream& out, JSC::Wasm::OpType op)
{
    out.print(JSC::Wasm::makeString(op));
}

inline void printInternal(PrintStream& out, JSC::Wasm::ExtAtomicOpType op)
{
    out.print(JSC::Wasm::makeString(op));
}

} // namespace WTF

#endif // ENABLE(WEBASSEMBLY)

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

"""

wasmOpsHFile.write(contents)
wasmOpsHFile.close()
