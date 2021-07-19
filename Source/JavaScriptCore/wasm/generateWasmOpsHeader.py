#! /usr/bin/env python

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

def typeMacroizer():
    inc = 0
    for ty in wasm.types:
        yield cppMacro(ty, wasm.types[ty]["value"], wasm.types[ty]["b3type"], inc)
        inc += 1

type_definitions = ["#define FOR_EACH_WASM_TYPE(macro)"]
type_definitions.extend([t for t in typeMacroizer()])
type_definitions = "".join(type_definitions)


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
defines.extend([op for op in opcodeMacroizer(lambda op: not (isUnary(op) or isBinary(op) or op["category"] == "control" or op["category"] == "memory" or op["value"] == 0xfc or isAtomic(op)))])
defines.append("\n\n#define FOR_EACH_WASM_CONTROL_FLOW_OP(macro)")
defines.extend([op for op in opcodeMacroizer(lambda op: op["category"] == "control")])
defines.append("\n\n#define FOR_EACH_WASM_SIMPLE_UNARY_OP(macro)")
defines.extend([op for op in opcodeWithTypesMacroizer(lambda op: isUnary(op) and isSimple(op))])
defines.append("\n\n#define FOR_EACH_WASM_UNARY_OP(macro) \\\n    FOR_EACH_WASM_SIMPLE_UNARY_OP(macro)")
defines.extend([op for op in opcodeWithTypesMacroizer(lambda op: isUnary(op) and not (isSimple(op)))])
defines.append("\n\n#define FOR_EACH_WASM_SIMPLE_BINARY_OP(macro)")
defines.extend([op for op in opcodeWithTypesMacroizer(lambda op: isBinary(op) and isSimple(op))])
defines.append("\n\n#define FOR_EACH_WASM_BINARY_OP(macro) \\\n    FOR_EACH_WASM_SIMPLE_BINARY_OP(macro)")
defines.extend([op for op in opcodeWithTypesMacroizer(lambda op: isBinary(op) and not (isSimple(op)))])
defines.append("\n\n#define FOR_EACH_WASM_MEMORY_LOAD_OP(macro)")
defines.extend([op for op in memoryLoadMacroizer()])
defines.append("\n\n#define FOR_EACH_WASM_MEMORY_STORE_OP(macro)")
defines.extend([op for op in memoryStoreMacroizer()])
defines.append("\n\n#define FOR_EACH_WASM_TABLE_OP(macro)")
defines.extend([op for op in opcodeMacroizer(lambda op: (op["category"] == "exttable"), opcodeField="extendedOp")])
defines.append("\n\n#define FOR_EACH_WASM_TRUNC_SATURATED_OP(macro)")
defines.extend([op for op in saturatedTruncMacroizer()])
defines.append("\n\n#define FOR_EACH_WASM_EXT_ATOMIC_LOAD_OP(macro)")
defines.extend([op for op in atomicMemoryLoadMacroizer()])
defines.append("\n\n#define FOR_EACH_WASM_EXT_ATOMIC_STORE_OP(macro)")
defines.extend([op for op in atomicMemoryStoreMacroizer()])
defines.append("\n\n#define FOR_EACH_WASM_EXT_ATOMIC_BINARY_RMW_OP(macro)")
defines.extend([op for op in atomicBinaryRMWMacroizer()])
defines.append("\n\n#define FOR_EACH_WASM_EXT_ATOMIC_OTHER_OP(macro)")
defines.extend([op for op in opcodeMacroizer(lambda op: isAtomic(op) and (not isAtomicLoad(op) and not isAtomicStore(op) and not isAtomicBinaryRMW(op)), opcodeField="extendedOp")])
defines.append("\n\n")

defines = "".join(defines)

opValueSet = set([op for op in wasm.opcodeIterator(lambda op: True, lambda op: opcodes[op]["value"])])
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

#if ENABLE(WEBASSEMBLY)

#include <cstdint>

#if ENABLE(WEBASSEMBLY_B3JIT)
#include "B3Type.h"
#endif

namespace JSC { namespace Wasm {

static constexpr unsigned expectedVersionNumber = """ + wasm.expectedVersionNumber + """;

static constexpr unsigned numTypes = """ + str(len(types)) + """;

""" + type_definitions + """
#define CREATE_ENUM_VALUE(name, id, ...) name = id,
enum class TypeKind : int8_t {
    FOR_EACH_WASM_TYPE(CREATE_ENUM_VALUE)
};
#undef CREATE_ENUM_VALUE

enum class Nullable : bool {
  No = false,
  Yes = true,
};

using SignatureIndex = uintptr_t;

struct Type {
    TypeKind kind;
    Nullable nullable;
    SignatureIndex index;

    bool operator==(const Type& other) const
    {
        return other.kind == kind && other.nullable == nullable && other.index == index;
    }

    bool operator!=(const Type& other) const
    {
        return !(other == *this);
    }

    bool isNullable() const
    {
        return static_cast<bool>(nullable);
    }

    #define CREATE_PREDICATE(name, ...) bool is ## name() const { return kind == TypeKind::name; }
    FOR_EACH_WASM_TYPE(CREATE_PREDICATE)
    #undef CREATE_PREDICATE
};

namespace Types
{
#define CREATE_CONSTANT(name, id, ...) constexpr Type name = Type{TypeKind::name, Nullable::Yes, 0u};
FOR_EACH_WASM_TYPE(CREATE_CONSTANT)
#undef CREATE_CONSTANT
} // namespace Types

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

#define CREATE_CASE(name, ...) case TypeKind::name: return #name;
inline const char* makeString(TypeKind kind)
{
    switch (kind) {
    FOR_EACH_WASM_TYPE(CREATE_CASE)
    }
    RELEASE_ASSERT_NOT_REACHED();
    return nullptr;
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
#define FOR_EACH_WASM_OP(macro) \\
    FOR_EACH_WASM_SPECIAL_OP(macro) \\
    FOR_EACH_WASM_CONTROL_FLOW_OP(macro) \\
    FOR_EACH_WASM_UNARY_OP(macro) \\
    FOR_EACH_WASM_BINARY_OP(macro) \\
    FOR_EACH_WASM_MEMORY_LOAD_OP(macro) \\
    FOR_EACH_WASM_MEMORY_STORE_OP(macro) \\
    macro(Ext1,  0xFC, Oops, 0) \\
    macro(ExtAtomic, 0xFE, Oops, 0)

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

enum class Ext1OpType : uint8_t {
    FOR_EACH_WASM_TABLE_OP(CREATE_ENUM_VALUE)
    FOR_EACH_WASM_TRUNC_SATURATED_OP(CREATE_ENUM_VALUE)
};

enum class ExtAtomicOpType : uint8_t {
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

inline bool isSimple(UnaryOpType op)
{
    switch (op) {
#define CREATE_CASE(name, ...) case UnaryOpType::name:
    FOR_EACH_WASM_SIMPLE_UNARY_OP(CREATE_CASE)
        return true;
#undef CREATE_CASE
    default:
        break;
    }
    return false;
}

inline bool isSimple(BinaryOpType op)
{
    switch (op) {
#define CREATE_CASE(name, ...) case BinaryOpType::name:
    FOR_EACH_WASM_SIMPLE_BINARY_OP(CREATE_CASE)
        return true;
#undef CREATE_CASE
    default:
        break;
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

#define CREATE_CASE(name, ...) case name: return #name;
inline const char* makeString(OpType op)
{
    switch (op) {
    FOR_EACH_WASM_OP(CREATE_CASE)
    }
    RELEASE_ASSERT_NOT_REACHED();
    return nullptr;
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

} // namespace WTF

#endif // ENABLE(WEBASSEMBLY)

"""

wasmOpsHFile.write(contents)
wasmOpsHFile.close()
