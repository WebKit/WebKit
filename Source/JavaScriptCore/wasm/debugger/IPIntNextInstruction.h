/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#include <cstdint>

#if ENABLE(WEBASSEMBLY)

#include "WasmIPIntGenerator.h"

namespace JSC {
namespace IPInt {

// Structure to hold the result of next instruction calculation
// Only PC fields are needed for breakpoint placement. The actual MC is provided
// directly by the unreachable handler when breakpoints are hit.
struct NextInstructionResult {
    // For conditional branches, we also provide the alternate path
    bool isConditionalBranch { false };
    const uint8_t* nextPC { nullptr }; // Primary path (when condition is true)
    const uint8_t* elsePC { nullptr }; // Alternate path (when condition is false)
};

// Main function to calculate next instruction PC and MC
NextInstructionResult calculateNextInstruction(uint8_t opcode, const uint8_t* pc, const uint8_t* mc);

// Helper functions for different instruction categories
namespace InstructionAdvance {

// Fixed-length instructions (most arithmetic, comparison, conversion ops)
NextInstructionResult fixedLength(const uint8_t* pc, const uint8_t* mc, uint32_t pcAdvance);

// Variable-length instructions using InstructionLengthMetadata
NextInstructionResult variableLength(const uint8_t* pc, const uint8_t* mc);

// Instructions using specific metadata structures
NextInstructionResult withConst32Metadata(const uint8_t* pc, const uint8_t* mc);
NextInstructionResult withConst64Metadata(const uint8_t* pc, const uint8_t* mc);
NextInstructionResult withGlobalMetadata(const uint8_t* pc, const uint8_t* mc);
NextInstructionResult withBlockMetadata(const uint8_t* pc, const uint8_t* mc);

// Control flow instructions
NextInstructionResult handleIf(const uint8_t* pc, const uint8_t* mc);
NextInstructionResult handleBr(const uint8_t* pc, const uint8_t* mc);
NextInstructionResult handleBrIf(const uint8_t* pc, const uint8_t* mc);
NextInstructionResult handleBrTable(const uint8_t* pc, const uint8_t* mc);

// Local variable instructions (with ULEB128 decoding)
NextInstructionResult handleLocalGet(const uint8_t* pc, const uint8_t* mc);
NextInstructionResult handleLocalSet(const uint8_t* pc, const uint8_t* mc);
NextInstructionResult handleLocalTee(const uint8_t* pc, const uint8_t* mc);

// Call instructions
NextInstructionResult handleCall(const uint8_t* pc, const uint8_t* mc);
NextInstructionResult handleCallIndirect(const uint8_t* pc, const uint8_t* mc);

// Simplified prefix instruction handlers (uniform handling for entire prefix families)
NextInstructionResult handleGCPrefix(const uint8_t* pc, const uint8_t* mc);
NextInstructionResult handleConversionPrefix(const uint8_t* pc, const uint8_t* mc);
NextInstructionResult handleSIMDPrefix(const uint8_t* pc, const uint8_t* mc);
NextInstructionResult handleAtomicPrefix(const uint8_t* pc, const uint8_t* mc);

// Note: Detailed prefix instructions are handled directly in the main implementation
// with proper sub-opcode dispatch using FOR_EACH_IPINT_*_OPCODE macros

} // namespace InstructionAdvance

}
} // namespace JSC::IPInt

#endif // ENABLE(WEBASSEMBLY)
