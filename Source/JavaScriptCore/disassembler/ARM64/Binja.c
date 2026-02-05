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

#include "config.h"

#if ENABLE(ARM64_DISASSEMBLER)

#include "Binja.h"
#include <string.h>

IGNORE_WARNINGS_BEGIN("error=undef")
IGNORE_WARNINGS_BEGIN("undef")
IGNORE_WARNINGS_BEGIN("missing-prototypes")
IGNORE_WARNINGS_BEGIN("cast-qual")
IGNORE_WARNINGS_BEGIN("cast-align")
IGNORE_WARNINGS_BEGIN("sign-compare")
IGNORE_WARNINGS_BEGIN("documentation")
IGNORE_WARNINGS_BEGIN("unused-parameter")
IGNORE_WARNINGS_BEGIN("missing-field-initializers")
IGNORE_WARNINGS_BEGIN("implicit-fallthrough")
IGNORE_WARNINGS_BEGIN("incompatible-pointer-types-discards-qualifiers")

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

#include "binja_impl.c.h"

// Helper function to extract register number from Register enum
static uint8_t extractRegNumber(Register reg)
{
    // W registers: REG_W0 (1) to REG_W30 (31), REG_WZR (32), REG_WSP (33)
    if (reg >= REG_W0 && reg <= REG_W30)
        return (uint8_t)(reg - REG_W0);
    if (reg == REG_WZR)
        return 31;
    if (reg == REG_WSP)
        return 31;

    // X registers: REG_X0 (34) to REG_X30 (64), REG_XZR (65), REG_SP (66)
    if (reg >= REG_X0 && reg <= REG_X30)
        return (uint8_t)(reg - REG_X0);
    if (reg == REG_XZR)
        return 31;
    if (reg == REG_SP)
        return 31;

    return 32; // Invalid marker
}

// Helper function to check if a register is 64-bit
static uint8_t isReg64Bit(Register reg)
{
    return (reg >= REG_X0 && reg <= REG_XZR) || reg == REG_SP;
}

bool arm64GetInstructionInfo(uint32_t instruction, uint64_t pc, ARM64InstructionInfo* outInfo)
{
    Instruction instr;
    memset(&instr, 0, sizeof(instr));
    memset(outInfo, 0, sizeof(*outInfo));

    int status = aarch64_decompose(instruction, &instr, pc);
    if (status != DECODE_STATUS_OK)
        return false;

    outInfo->category = ARM64_CATEGORY_OTHER;
    outInfo->destRegister = 32; // Invalid marker
    outInfo->srcRegister = 32;

    switch (instr.operation) {
    // Unconditional branch
    case ARM64_B:
        outInfo->category = ARM64_CATEGORY_BRANCH_UNCONDITIONAL;
        outInfo->isLink = 0;
        // First operand is LABEL with target address
        if (instr.operands[0].operandClass == LABEL)
            outInfo->immediate = (int64_t)instr.operands[0].immediate - (int64_t)pc;
        break;

    case ARM64_BL:
        outInfo->category = ARM64_CATEGORY_BRANCH_UNCONDITIONAL;
        outInfo->isLink = 1;
        if (instr.operands[0].operandClass == LABEL)
            outInfo->immediate = (int64_t)instr.operands[0].immediate - (int64_t)pc;
        break;

    // Conditional branches (B.cond)
    case ARM64_B_EQ:
    case ARM64_B_NE:
    case ARM64_B_CS:
    case ARM64_B_CC:
    case ARM64_B_MI:
    case ARM64_B_PL:
    case ARM64_B_VS:
    case ARM64_B_VC:
    case ARM64_B_HI:
    case ARM64_B_LS:
    case ARM64_B_GE:
    case ARM64_B_LT:
    case ARM64_B_GT:
    case ARM64_B_LE:
    case ARM64_B_AL:
    case ARM64_B_NV:
        outInfo->category = ARM64_CATEGORY_BRANCH_CONDITIONAL;
        // First operand is LABEL
        if (instr.operands[0].operandClass == LABEL)
            outInfo->immediate = (int64_t)instr.operands[0].immediate - (int64_t)pc;
        break;

    // Compare and branch
    case ARM64_CBZ:
    case ARM64_CBNZ:
        outInfo->category = ARM64_CATEGORY_BRANCH_COMPARE;
        // First operand is register, second is LABEL
        if (instr.operands[0].operandClass == REG) {
            outInfo->srcRegister = extractRegNumber(instr.operands[0].reg[0]);
            outInfo->is64Bit = isReg64Bit(instr.operands[0].reg[0]);
        }
        if (instr.operands[1].operandClass == LABEL)
            outInfo->immediate = (int64_t)instr.operands[1].immediate - (int64_t)pc;
        break;

    // Test and branch
    case ARM64_TBZ:
    case ARM64_TBNZ:
        outInfo->category = ARM64_CATEGORY_BRANCH_TEST;
        // First operand is register
        if (instr.operands[0].operandClass == REG) {
            outInfo->srcRegister = extractRegNumber(instr.operands[0].reg[0]);
            outInfo->is64Bit = isReg64Bit(instr.operands[0].reg[0]);
        }
        // Find LABEL operand for target
        for (int i = 0; i < MAX_OPERANDS; i++) {
            if (instr.operands[i].operandClass == LABEL) {
                outInfo->immediate = (int64_t)instr.operands[i].immediate - (int64_t)pc;
                break;
            }
        }
        break;

    // Branch register instructions
    case ARM64_BR:
    case ARM64_BRAA:
    case ARM64_BRAAZ:
    case ARM64_BRAB:
    case ARM64_BRABZ:
        outInfo->category = ARM64_CATEGORY_BRANCH_REGISTER;
        outInfo->isLink = 0;
        if (instr.operands[0].operandClass == REG)
            outInfo->srcRegister = extractRegNumber(instr.operands[0].reg[0]);
        break;

    case ARM64_BLR:
    case ARM64_BLRAA:
    case ARM64_BLRAAZ:
    case ARM64_BLRAB:
    case ARM64_BLRABZ:
        outInfo->category = ARM64_CATEGORY_BRANCH_REGISTER;
        outInfo->isLink = 1;
        if (instr.operands[0].operandClass == REG)
            outInfo->srcRegister = extractRegNumber(instr.operands[0].reg[0]);
        break;

    case ARM64_RET:
    case ARM64_RETAA:
    case ARM64_RETAB:
    case ARM64_RETAASPPC:
    case ARM64_RETAASPPCR:
    case ARM64_RETABSPPC:
    case ARM64_RETABSPPCR:
        outInfo->category = ARM64_CATEGORY_BRANCH_REGISTER;
        outInfo->isLink = 0;
        // RET uses X30 by default, but may specify register
        if (instr.operands[0].operandClass == REG)
            outInfo->srcRegister = extractRegNumber(instr.operands[0].reg[0]);
        else
            outInfo->srcRegister = 30; // LR
        break;

    // Move wide
    case ARM64_MOVZ:
        outInfo->category = ARM64_CATEGORY_MOVZ;
        if (instr.operands[0].operandClass == REG) {
            outInfo->destRegister = extractRegNumber(instr.operands[0].reg[0]);
            outInfo->is64Bit = isReg64Bit(instr.operands[0].reg[0]);
        }
        if (instr.operands[1].operandClass == IMM32 || instr.operands[1].operandClass == IMM64) {
            outInfo->immediate = (int64_t)instr.operands[1].immediate;
            outInfo->shiftAmount = (uint8_t)instr.operands[1].shiftValue;
        }
        break;

    case ARM64_MOVN:
        outInfo->category = ARM64_CATEGORY_MOVN;
        if (instr.operands[0].operandClass == REG) {
            outInfo->destRegister = extractRegNumber(instr.operands[0].reg[0]);
            outInfo->is64Bit = isReg64Bit(instr.operands[0].reg[0]);
        }
        if (instr.operands[1].operandClass == IMM32 || instr.operands[1].operandClass == IMM64) {
            outInfo->immediate = (int64_t)instr.operands[1].immediate;
            outInfo->shiftAmount = (uint8_t)instr.operands[1].shiftValue;
        }
        break;

    case ARM64_MOVK:
        outInfo->category = ARM64_CATEGORY_MOVK;
        if (instr.operands[0].operandClass == REG) {
            outInfo->destRegister = extractRegNumber(instr.operands[0].reg[0]);
            outInfo->is64Bit = isReg64Bit(instr.operands[0].reg[0]);
        }
        if (instr.operands[1].operandClass == IMM32 || instr.operands[1].operandClass == IMM64) {
            outInfo->immediate = (int64_t)instr.operands[1].immediate;
            outInfo->shiftAmount = (uint8_t)instr.operands[1].shiftValue;
        }
        break;

    // MOV alias (when assembler uses MOV instead of MOVZ/MOVN)
    case ARM64_MOV:
        // Check if this is a MOV with immediate (alias for MOVZ/MOVN)
        if (instr.operands[1].operandClass == IMM32 || instr.operands[1].operandClass == IMM64) {
            outInfo->category = ARM64_CATEGORY_MOV;
            if (instr.operands[0].operandClass == REG) {
                outInfo->destRegister = extractRegNumber(instr.operands[0].reg[0]);
                outInfo->is64Bit = isReg64Bit(instr.operands[0].reg[0]);
            }
            outInfo->immediate = (int64_t)instr.operands[1].immediate;
            outInfo->shiftAmount = 0;
        }
        // If MOV with register operand, leave as OTHER category
        break;

    // ADR/ADRP
    case ARM64_ADR:
        outInfo->category = ARM64_CATEGORY_ADR;
        if (instr.operands[0].operandClass == REG)
            outInfo->destRegister = extractRegNumber(instr.operands[0].reg[0]);
        // Second operand is LABEL with target address
        if (instr.operands[1].operandClass == LABEL)
            outInfo->immediate = (int64_t)instr.operands[1].immediate - (int64_t)pc;
        break;

    case ARM64_ADRP:
        outInfo->category = ARM64_CATEGORY_ADRP;
        if (instr.operands[0].operandClass == REG)
            outInfo->destRegister = extractRegNumber(instr.operands[0].reg[0]);
        // Second operand is LABEL with page-aligned target address
        if (instr.operands[1].operandClass == LABEL)
            outInfo->immediate = (int64_t)instr.operands[1].immediate - (int64_t)(pc & ~0xFFFULL);
        break;

    default:
        break;
    }

    return true;
}

const char* arm64Disassemble(uint32_t* pc, char* buffer, size_t size)
{
    Instruction instr;
    int status = aarch64_decompose(*pc, &instr, (uint64_t)pc);
    if (status == DECODE_STATUS_OK)
        aarch64_disassemble(&instr, buffer, size);
    else
        buffer[0] = '\0';
    return buffer;
}

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

IGNORE_WARNINGS_END
IGNORE_WARNINGS_END
IGNORE_WARNINGS_END
IGNORE_WARNINGS_END
IGNORE_WARNINGS_END
IGNORE_WARNINGS_END
IGNORE_WARNINGS_END
IGNORE_WARNINGS_END
IGNORE_WARNINGS_END
IGNORE_WARNINGS_END
IGNORE_WARNINGS_END

#endif // ENABLE(ARM64_DISASSEMBLER)
