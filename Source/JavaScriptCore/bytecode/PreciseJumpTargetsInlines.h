/*
 * Copyright (C) 2016-2017 Apple Inc. All rights reserved.
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

#include "BytecodeStructs.h"
#include "InterpreterInlines.h"
#include "Opcode.h"
#include "PreciseJumpTargets.h"

namespace JSC {

#define SWITCH_JMP(CASE_OP, SWITCH_CASE, SWITCH_DEFAULT_OFFSET) \
    switch (instruction->opcodeID()) { \
    CASE_OP(OpJmp) \
    \
    CASE_OP(OpJtrue) \
    CASE_OP(OpJfalse) \
    CASE_OP(OpJeqNull) \
    CASE_OP(OpJneqNull) \
    CASE_OP(OpJneqPtr) \
    \
    CASE_OP(OpJless) \
    CASE_OP(OpJlesseq) \
    CASE_OP(OpJgreater) \
    CASE_OP(OpJgreatereq) \
    CASE_OP(OpJnless) \
    CASE_OP(OpJnlesseq) \
    CASE_OP(OpJngreater) \
    CASE_OP(OpJngreatereq) \
    CASE_OP(OpJeq) \
    CASE_OP(OpJneq) \
    CASE_OP(OpJstricteq) \
    CASE_OP(OpJnstricteq) \
    CASE_OP(OpJbelow) \
    CASE_OP(OpJbeloweq) \
    case op_switch_imm: { \
        auto bytecode = instruction->as<OpSwitchImm>(); \
        auto& table = codeBlock->switchJumpTable(bytecode.tableIndex); \
        for (unsigned i = table.branchOffsets.size(); i--;) \
            SWITCH_CASE(table.branchOffsets[i]); \
        SWITCH_DEFAULT_OFFSET(OpSwitchImm); \
        break; \
    } \
    case op_switch_char: { \
        auto bytecode = instruction->as<OpSwitchChar>(); \
        auto& table = codeBlock->switchJumpTable(bytecode.tableIndex); \
        for (unsigned i = table.branchOffsets.size(); i--;) \
            SWITCH_CASE(table.branchOffsets[i]); \
        SWITCH_DEFAULT_OFFSET(OpSwitchChar); \
        break; \
    } \
    case op_switch_string: { \
        auto bytecode = instruction->as<OpSwitchString>(); \
        auto& table = codeBlock->stringSwitchJumpTable(bytecode.tableIndex); \
        auto iter = table.offsetTable.begin(); \
        auto end = table.offsetTable.end(); \
        for (; iter != end; ++iter) \
            SWITCH_CASE(iter->value.branchOffset); \
        SWITCH_DEFAULT_OFFSET(OpSwitchString); \
        break; \
    } \
    default: \
        break; \
    } \


template<typename Block>
inline int jumpTargetForInstruction(Block* codeBlock, const InstructionStream::Ref& instruction, unsigned target)
{
    if (target)
        return target;
    return codeBlock->outOfLineJumpOffset(instruction);
}

template<typename HashMap>
inline int jumpTargetForInstruction(HashMap& outOfLineJumpTargets, const InstructionStream::Ref& instruction, unsigned target)
{
    if (target)
        return target;
    ASSERT(outOfLineJumpTargets.contains(instruction.offset()));
    return outOfLineJumpTargets.get(instruction.offset());
}

template<typename Op, typename Block>
inline int jumpTargetForInstruction(Block&& codeBlock, const InstructionStream::Ref& instruction)
{
    auto bytecode = instruction->as<Op>();
    return jumpTargetForInstruction(codeBlock, instruction, bytecode.target);
}

template<typename Block, typename Function>
inline void extractStoredJumpTargetsForInstruction(Block&& codeBlock, const InstructionStream::Ref& instruction, const Function& function)
{
#define CASE_OP(__op) \
    case __op::opcodeID: \
        function(jumpTargetForInstruction<__op>(codeBlock, instruction)); \
        break;

#define SWITCH_CASE(__target) \
    function(__target)

#define SWITCH_DEFAULT_OFFSET(__op) \
    function(jumpTargetForInstruction(codeBlock, instruction, bytecode.defaultOffset)) \

SWITCH_JMP(CASE_OP, SWITCH_CASE, SWITCH_DEFAULT_OFFSET)

#undef CASE_OP
#undef SWITCH_CASE
#undef SWITCH_DEFAULT_OFFSET
}

template<typename Block, typename Function, typename CodeBlockOrHashMap>
inline void updateStoredJumpTargetsForInstruction(Block&& codeBlock, unsigned finalOffset, InstructionStream::MutableRef instruction, const Function& function, CodeBlockOrHashMap& codeBlockOrHashMap)
{
#define CASE_OP(__op) \
    case __op::opcodeID: { \
        int32_t target = jumpTargetForInstruction<__op>(codeBlockOrHashMap, instruction); \
        int32_t newTarget = function(target); \
        instruction->cast<__op>()->setTarget(BoundLabel(newTarget), [&]() { \
            codeBlock->addOutOfLineJumpTarget(finalOffset + instruction.offset(), newTarget); \
            return BoundLabel(); \
        }); \
        break; \
    }

#define SWITCH_CASE(__target) \
    do { \
        int32_t target = __target; \
        __target = function(target); \
    } while (false)

#define SWITCH_DEFAULT_OFFSET(__op) \
    do { \
        int32_t target = jumpTargetForInstruction(codeBlockOrHashMap, instruction, bytecode.defaultOffset); \
        int32_t newTarget = function(target); \
        instruction->cast<__op>()->setDefaultOffset(BoundLabel(newTarget), [&]() { \
            codeBlock->addOutOfLineJumpTarget(finalOffset + instruction.offset(), newTarget); \
            return BoundLabel(); \
        }); \
    } while (false)

SWITCH_JMP(CASE_OP, SWITCH_CASE, SWITCH_DEFAULT_OFFSET)

#undef CASE_OP
#undef JMP_TARGET
}

template<typename Block, typename Function>
inline void updateStoredJumpTargetsForInstruction(Block* codeBlock, unsigned finalOffset, InstructionStream::MutableRef instruction, Function function)
{
    updateStoredJumpTargetsForInstruction(codeBlock, finalOffset, instruction, function, codeBlock);
}

} // namespace JSC
