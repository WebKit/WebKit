/*
 * Copyright (C) 2023-2024 Apple Inc. All rights reserved.
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

#if ENABLE(WEBASSEMBLY)

#include "WasmCallingConvention.h"
#include <wtf/Expected.h>
#include <wtf/text/WTFString.h>

namespace JSC { namespace Wasm {

class FunctionIPIntMetadataGenerator;
class TypeDefinition;
struct ModuleInformation;

Expected<std::unique_ptr<FunctionIPIntMetadataGenerator>, String> parseAndCompileMetadata(std::span<const uint8_t>, const TypeDefinition&, ModuleInformation&, FunctionCodeIndex functionIndex);

} // namespace JSC::Wasm

namespace IPInt {

constexpr static unsigned STACK_ENTRY_SIZE = 16; // bytes
struct IPIntStackEntry {
    union {
        int32_t i32;
        float f32;
        int64_t i64;
        double f64;
        v128_t v128;
        EncodedJSValue ref;
    };
};

constexpr static unsigned LOCAL_SIZE = 16; // bytes
struct IPIntLocal {
    union {
        int32_t i32;
        float f32;
        int64_t i64;
        double f64;
        v128_t v128;
        EncodedJSValue ref;
    };
};

static_assert(sizeof(IPIntStackEntry) == STACK_ENTRY_SIZE);
static_assert(sizeof(IPIntLocal) == LOCAL_SIZE);

#pragma pack(1)

// Metadata structure for control flow instructions

struct InstructionLengthMetadata {
    uint8_t length; // 1B for length of current instruction
};

struct BlockMetadata {
    uint32_t deltaPC; // 4B for new PC
    uint32_t deltaMC; // 4B for new MC
};

struct IfMetadata {
    uint32_t elseDeltaPC; // 4B PC for new else PC
    uint32_t elseDeltaMC; // 4B MC of new else MC
    InstructionLengthMetadata instructionLength;
};

struct ThrowMetadata {
    uint32_t exceptionIndex; // 4B for exception index
};

struct RethrowMetadata {
    uint32_t tryDepth; // 4B for try depth
};

struct CatchMetadata {
    uint32_t stackSizeInV128; // 4B for stack size
};

struct BranchTargetMetadata {
    BlockMetadata block; // 8B for target
    uint16_t toPop; // 2B for stack values to pop
    uint16_t toKeep; // 2B for stack values to keep
};

struct BranchMetadata {
    BranchTargetMetadata target;
    InstructionLengthMetadata instructionLength;
};

struct SwitchMetadata {
    uint32_t size; // 4B for number of jump targets
    BranchTargetMetadata target[0];
};

// Global get/set metadata structure

struct GlobalMetadata {
    uint32_t index; // 4B for index of global
    InstructionLengthMetadata instructionLength;
    uint8_t bindingMode; // 1B for bindingMode
    uint8_t isRef; // 1B for ref flag
};

// Constant metadata structures

struct Const32Metadata {
    InstructionLengthMetadata instructionLength;
    uint32_t value;
};

struct Const64Metadata {
    InstructionLengthMetadata instructionLength;
    uint64_t value;
};

struct Const128Metadata {
    InstructionLengthMetadata instructionLength;
    v128_t value;
};

struct TableInitMetadata {
    uint32_t elementIndex; // 4B for index of element
    uint32_t tableIndex; // 4B for index of table
    InstructionLengthMetadata instructionLength;
};

struct TableFillMetadata {
    uint32_t tableIndex; // 4B for index of table
    InstructionLengthMetadata instructionLength;
};

struct TableGrowMetadata {
    uint32_t tableIndex; // 4B for index of table
    InstructionLengthMetadata instructionLength;
};

struct TableCopyMetadata {
    uint32_t dstTableIndex; // 4B for index of destination table
    uint32_t srcTableIndex; // 4B for index of source table
    InstructionLengthMetadata instructionLength;
};

// Metadata structure for calls:

enum class CallArgumentBytecode : uint8_t { // (mINT)
    ArgumentGPR = 0x0, // 0x00 - 0x07: push into a0, a1, ...
    ArgumentFPR = 0x8, // 0x08 - 0x0f: push into fa0, fa1, ...
    ArgumentStackAligned = 0x10, // 0x10: pop stack value, push onto stack[0]
    ArgumentStackUnaligned = 0x11, // 0x11: pop stack value, add another 16B for params, push onto stack[8]
    StackAlign = 0x12, // 0x12: add another 16B for params
    End = 0x13 // 0x13: stop
};

struct CallMetadata {
    uint8_t length; // 1B for instruction length
    Wasm::FunctionSpaceIndex functionIndex; // 4B for decoded index
    CallArgumentBytecode argumentBytecode[0];
};

struct CallIndirectMetadata {
    uint8_t length; // 1B for length
    uint32_t tableIndex; // 4B for table index
    uint32_t typeIndex; // 4B for type index
    CallArgumentBytecode argumentBytecode[0];
};

// Metadata structure for returns:

enum class CallResultBytecode : uint8_t { // (mINT)
    ResultGPR = 0x0, // 0x00 - 0x07: r0 - r7
    ResultFPR = 0x8, // 0x08 - 0x0f: fr0 - fr7
    ResultStack = 0x10, // 0x0c: stack
    End = 0x11 // 0x0d: end
};

struct callReturnMetadata {
    uint16_t stackSlots; // 2B for number of arguments on stack (to clean up current call frame)
    uint16_t argumentCount; // 2B for number of arguments (to take off arguments)
    CallResultBytecode resultBytecode[0];
};

#pragma pack()

} // namespace JSC::IPInt

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
