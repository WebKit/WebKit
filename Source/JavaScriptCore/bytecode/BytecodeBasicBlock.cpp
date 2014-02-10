/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "BytecodeBasicBlock.h"

#include "CodeBlock.h"
#include "Operations.h"
#include "PreciseJumpTargets.h"

namespace JSC {

static bool isBranch(OpcodeID opcodeID)
{
    switch (opcodeID) {
    case op_jmp:
    case op_jtrue:
    case op_jfalse:
    case op_jeq_null:
    case op_jneq_null:
    case op_jneq_ptr:
    case op_jless:
    case op_jlesseq:
    case op_jgreater:
    case op_jgreatereq:
    case op_jnless:
    case op_jnlesseq:
    case op_jngreater:
    case op_jngreatereq:
    case op_switch_imm:
    case op_switch_char:
    case op_switch_string:
    case op_get_pnames:
    case op_next_pname:
    case op_check_has_instance:
        return true;
    default:
        return false;
    }
}

static bool isUnconditionalBranch(OpcodeID opcodeID)
{
    switch (opcodeID) {
    case op_jmp:
        return true;
    default:
        return false;
    }
}

static bool isTerminal(OpcodeID opcodeID)
{
    switch (opcodeID) {
    case op_ret:
    case op_ret_object_or_this:
    case op_end:
        return true;
    default:
        return false;
    }
}

static bool isThrow(OpcodeID opcodeID)
{
    switch (opcodeID) {
    case op_throw:
    case op_throw_static_error:
        return true;
    default:
        return false;
    }
}

static bool isJumpTarget(OpcodeID opcodeID, Vector<unsigned, 32>& jumpTargets, unsigned bytecodeOffset)
{
    if (opcodeID == op_catch)
        return true;

    for (unsigned i = 0; i < jumpTargets.size(); i++) {
        if (bytecodeOffset == jumpTargets[i])
            return true;
    }
    return false;
}

static void linkBlocks(BytecodeBasicBlock* predecessor, BytecodeBasicBlock* successor)
{
    predecessor->addSuccessor(successor);
    successor->addPredecessor(predecessor);
}

void computeBytecodeBasicBlocks(CodeBlock* codeBlock, Vector<RefPtr<BytecodeBasicBlock> >& basicBlocks)
{
    Vector<unsigned, 32> jumpTargets;
    computePreciseJumpTargets(codeBlock, jumpTargets);

    // Create the entry and exit basic blocks.
    BytecodeBasicBlock* entry = new BytecodeBasicBlock(BytecodeBasicBlock::EntryBlock);
    basicBlocks.append(adoptRef(entry));
    BytecodeBasicBlock* exit = new BytecodeBasicBlock(BytecodeBasicBlock::ExitBlock);

    // Find basic block boundaries.
    BytecodeBasicBlock* current = new BytecodeBasicBlock(0, 0);
    linkBlocks(entry, current);
    basicBlocks.append(adoptRef(current));

    bool nextInstructionIsLeader = false;

    Interpreter* interpreter = codeBlock->vm()->interpreter;
    Instruction* instructionsBegin = codeBlock->instructions().begin();
    unsigned instructionCount = codeBlock->instructions().size();
    for (unsigned bytecodeOffset = 0; bytecodeOffset < instructionCount;) {
        OpcodeID opcodeID = interpreter->getOpcodeID(instructionsBegin[bytecodeOffset].u.opcode);
        unsigned opcodeLength = opcodeLengths[opcodeID];

        bool createdBlock = false;
        // If the current bytecode is a jump target, then it's the leader of its own basic block.
        if (isJumpTarget(opcodeID, jumpTargets, bytecodeOffset) || nextInstructionIsLeader) {
            BytecodeBasicBlock* block = new BytecodeBasicBlock(bytecodeOffset, opcodeLength);
            basicBlocks.append(adoptRef(block));
            current = block;
            createdBlock = true;
            nextInstructionIsLeader = false;
            bytecodeOffset += opcodeLength;
        }

        // If the current bytecode is a branch or a return, then the next instruction is the leader of its own basic block.
        if (isBranch(opcodeID) || isTerminal(opcodeID) || isThrow(opcodeID))
            nextInstructionIsLeader = true;

        if (createdBlock)
            continue;

        // Otherwise, just add to the length of the current block.
        current->addBytecodeLength(opcodeLength);
        bytecodeOffset += opcodeLength;
    }

    // Link basic blocks together.
    for (unsigned i = 0; i < basicBlocks.size(); i++) {
        BytecodeBasicBlock* block = basicBlocks[i].get();

        if (block->isEntryBlock() || block->isExitBlock())
            continue;

        bool fallsThrough = true; 
        for (unsigned bytecodeOffset = block->leaderBytecodeOffset(); bytecodeOffset < block->leaderBytecodeOffset() + block->totalBytecodeLength();) {
            const Instruction& currentInstruction = instructionsBegin[bytecodeOffset];
            OpcodeID opcodeID = interpreter->getOpcodeID(currentInstruction.u.opcode);
            unsigned opcodeLength = opcodeLengths[opcodeID];
            // If we found a terminal bytecode, link to the exit block.
            if (isTerminal(opcodeID)) {
                ASSERT(bytecodeOffset + opcodeLength == block->leaderBytecodeOffset() + block->totalBytecodeLength());
                linkBlocks(block, exit);
                fallsThrough = false;
                break;
            }

            // If we found a throw, get the HandlerInfo for this instruction to see where we will jump. 
            // If there isn't one, treat this throw as a terminal. This is true even if we have a finally
            // block because the finally block will create its own catch, which will generate a HandlerInfo.
            if (isThrow(opcodeID)) {
                ASSERT(bytecodeOffset + opcodeLength == block->leaderBytecodeOffset() + block->totalBytecodeLength());
                HandlerInfo* handler = codeBlock->handlerForBytecodeOffset(bytecodeOffset);
                fallsThrough = false;
                if (!handler) {
                    linkBlocks(block, exit);
                    break;
                }
                for (unsigned i = 0; i < basicBlocks.size(); i++) {
                    BytecodeBasicBlock* otherBlock = basicBlocks[i].get();
                    if (handler->target == otherBlock->leaderBytecodeOffset()) {
                        linkBlocks(block, otherBlock);
                        break;
                    }
                }
                break;
            }

            // If we found a branch, link to the block(s) that we jump to.
            if (isBranch(opcodeID)) {
                ASSERT(bytecodeOffset + opcodeLength == block->leaderBytecodeOffset() + block->totalBytecodeLength());
                Vector<unsigned, 1> bytecodeOffsetsJumpedTo;
                findJumpTargetsForBytecodeOffset(codeBlock, bytecodeOffset, bytecodeOffsetsJumpedTo);

                for (unsigned i = 0; i < basicBlocks.size(); i++) {
                    BytecodeBasicBlock* otherBlock = basicBlocks[i].get();
                    if (bytecodeOffsetsJumpedTo.contains(otherBlock->leaderBytecodeOffset()))
                        linkBlocks(block, otherBlock);
                }

                if (isUnconditionalBranch(opcodeID))
                    fallsThrough = false;

                break;
            }
            bytecodeOffset += opcodeLength;
        }

        // If we fall through then link to the next block in program order.
        if (fallsThrough) {
            ASSERT(i + 1 < basicBlocks.size());
            BytecodeBasicBlock* nextBlock = basicBlocks[i + 1].get();
            linkBlocks(block, nextBlock);
        }
    }

    basicBlocks.append(adoptRef(exit));
}

} // namespace JSC
