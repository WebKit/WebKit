/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

Expected<std::unique_ptr<FunctionIPIntMetadataGenerator>, String> parseAndCompileMetadata(std::span<const uint8_t>, const TypeDefinition&, ModuleInformation&, uint32_t functionIndex);

} // namespace JSC::Wasm

namespace IPInt {

#pragma pack(1)

// mINT: mini-interpreter / minimalist interpreter (whichever floats your boat)

// Metadata structure for control flow instructions

struct InstructionLengthMetadata {
    uint8_t length; // 1B for length of current instruction
};

struct BlockMetadata {
    uint32_t newPC; // 2B for new PC
    uint32_t newMC; // 2B for new MC
};

struct IfMetadata {
    uint32_t elsePC; // 4B PC for new else PC
    uint32_t elseMC; // 4B MC of new else MC
    InstructionLengthMetadata instructionLength;
};

struct throwMetadata {
    uint32_t exceptionIndex; // 4B for exception index
};

struct rethrowMetadata {
    uint32_t tryDepth; // 4B for try depth
};

struct catchMetadata {
    uint32_t stackSizeInV128; // 4B for stack size
};

struct branchTargetMetadata {
    BlockMetadata block; // 2B for stack values to pop
    uint16_t toPop; // 2B for stack values to keep
    uint16_t toKeep;
};

struct branchMetadata {
    branchTargetMetadata target;
    InstructionLengthMetadata instructionLength;
};

struct switchMetadata {
    uint32_t size; // 4B for number of jump targets
    branchTargetMetadata target[0];
};

// Global get/set metadata structure

struct globalMetadata {
    uint32_t index; // 4B for index of global
    InstructionLengthMetadata instructionLength;
    uint8_t bindingMode; // 1B for bindingMode
    uint8_t isRef; // 1B for ref flag
};

// Constant metadat structures

struct const32Metadata {
    InstructionLengthMetadata instructionLength;
    uint32_t value;
};

struct const64Metadata {
    InstructionLengthMetadata instructionLength;
    uint64_t value;
};

struct const128Metadata {
    InstructionLengthMetadata instructionLength;
    v128_t value;
};

struct tableInitMetadata {
    uint32_t elementIndex; // 4B for index of element
    uint32_t tableIndex; // 4B for index of table
    uint32_t dst; // 4B for destination (set by interpreter)
    uint32_t src; // 4B for source (set by interpreter)
    uint32_t length; // 4B for length (set by interpreter)
    InstructionLengthMetadata instructionLength;
};

struct tableFillMetadata {
    uint32_t tableIndex; // 4B for index of table
    EncodedJSValue fill; // 8B for fill value
    uint32_t offset; // 4B for offset (set by interpreter)
    uint32_t length; // 4B for length (set by interpreter)
    InstructionLengthMetadata instructionLength;
};

struct tableGrowMetadata {
    uint32_t tableIndex; // 4B for index of table
    EncodedJSValue fill; // 8B for fill value
    uint32_t length; // 4B for length (set by interpreter)
    InstructionLengthMetadata instructionLength;
};

struct tableCopyMetadata {
    uint32_t dstTableIndex; // 4B for index of destination table
    uint32_t srcTableIndex; // 4B for index of source table
    uint32_t dstOffset; // 4B for destination offset (set by interpreter)
    uint32_t srcOffset; // 4B for source offset (set by interpreter)
    uint32_t length; // 4B for length (set by interpreter)
    InstructionLengthMetadata instructionLength;
};

// Metadata structure for calls:

struct calleeMetadata {
    JSWebAssemblyInstance* instance; // Ptr for callee instance (set by operation)
    void* entrypoint; // Ptr for callee entrypoint (set by operation)
    EncodedJSValue boxedCallee; // 8B for callee (set by operation)
};

enum class CallArgumentBytecode : uint8_t { // (mINT)
    ArgumentGPR = 0x0, // 0x00 - 0x07: push into a0, a1, ...
    ArgumentFPR = 0x8, // 0x08 - 0x0b: push into fa0, fa1, ...
    ArgumentStackAligned = 0xc, // 0x0c: pop stack value, push onto stack[0]
    ArgumentStackUnaligned = 0xd, // 0x0d: pop stack value, add another 16B for params, push onto stack[8]
    StackAlign = 0xe, // 0x0e: add another 16B for params
    End = 0xf // 0x0f: stop
};

struct callMetadata {
    uint8_t length; // 1B for instruction length
    uint32_t functionIndex; // 4B for decoded index
    calleeMetadata callee;
    CallArgumentBytecode argumentBytecode[0];
};

struct callIndirectMetadata {
    uint8_t length; // 1B for length
    uint32_t tableIndex; // 4B for table index
    uint32_t typeIndex; // 4B for type index
    uint32_t functionRef; // 4B for function ref (set by interpreter)
    CallFrame* callFrame; // Ptr for call frame (set by interpreter)
    calleeMetadata callee;
    CallArgumentBytecode argumentBytecode[0];
};

// Metadata structure for returns:

enum class CallResultBytecode : uint8_t { // (mINT)
    ResultGPR = 0x0, // 0x00 - 0x07: r0 - r7
    ResultFPR = 0x8, // 0x08 - 0x0b: fr0 - fr3
    ResultStack = 0xc, // 0x0c: stack
    End = 0xd // 0x0d: end
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
