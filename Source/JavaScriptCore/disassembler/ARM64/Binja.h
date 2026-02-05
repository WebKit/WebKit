/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#if ENABLE(ARM64_DISASSEMBLER)

#ifdef __cplusplus
extern "C" {
#endif

const char* arm64Disassemble(uint32_t*, char*, size_t);

// Instruction category for JSC metadata analysis
typedef enum {
    ARM64_CATEGORY_OTHER = 0,
    ARM64_CATEGORY_BRANCH_UNCONDITIONAL,  // B, BL
    ARM64_CATEGORY_BRANCH_CONDITIONAL,    // B.cond
    ARM64_CATEGORY_BRANCH_COMPARE,        // CBZ, CBNZ
    ARM64_CATEGORY_BRANCH_TEST,           // TBZ, TBNZ
    ARM64_CATEGORY_BRANCH_REGISTER,       // BR, BLR, RET, etc.
    ARM64_CATEGORY_MOVZ,
    ARM64_CATEGORY_MOVN,
    ARM64_CATEGORY_MOVK,
    ARM64_CATEGORY_MOV,                   // MOV alias (immediate)
    ARM64_CATEGORY_ADR,
    ARM64_CATEGORY_ADRP,
} ARM64InstructionCategory;

typedef struct {
    ARM64InstructionCategory category;
    int64_t immediate;      // PC-relative offset or immediate value
    uint8_t destRegister;   // Destination register (0-30, 31=SP, 32+=invalid)
    uint8_t srcRegister;    // Source register for CBZ/CBNZ/BR
    uint8_t shiftAmount;    // For MOVZ/MOVN/MOVK shift (0,16,32,48)
    uint8_t is64Bit;        // Uses 64-bit registers
    uint8_t isLink;         // BL, BLR variants
} ARM64InstructionInfo;

// Decode instruction and extract info needed for JSC metadata analysis
// Returns true on success, false on decode failure
bool arm64GetInstructionInfo(uint32_t instruction, uint64_t pc, ARM64InstructionInfo* outInfo);

#ifdef __cplusplus
}
#endif

#endif // ENABLE(ARM64_DISASSEMBLER)
