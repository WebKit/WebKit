/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#include "AirSimplifyCFG.h"

#if ENABLE(B3_JIT)

#include "AirCode.h"
#include "AirPhaseScope.h"

namespace JSC { namespace B3 { namespace Air {

bool simplifyCFG(Code& code)
{
    constexpr bool verbose = false;

    PhaseScope phaseScope(code, "simplifyCFG"_s);

    // We have three easy simplification rules:
    //
    // 1) If a successor is a block that just jumps to another block, then jump directly to
    //    that block.
    //
    // 2) If all successors are the same and the operation has no effects, then use a jump
    //    instead.
    //
    // 3) If you jump to a block that is not you and has one predecessor, then merge.
    //
    // Note that because of the first rule, this phase may introduce critical edges. That's fine.
    // If you need broken critical edges, then you have to break them yourself.

    if (verbose) {
        dataLog("Air before simplifyCFG:\n");
        dataLog(code);
    }

    bool changed = false;
    bool validateAtEachPhase = shouldValidateIRAtEachPhase();
    for (BasicBlock* block : code) {
        // We rely on predecessors being conservatively correct. Verify this here.
        if (validateAtEachPhase) [[unlikely]] {
            for (BasicBlock* block : code) {
                for (BasicBlock* successor : block->successorBlocks())
                    RELEASE_ASSERT(successor->containsPredecessor(block));
            }
        }

        // We don't care about blocks that don't have successors.
        if (!block->numSuccessors())
            continue;

        // First check if any of the successors of this block can be forwarded over.
        // We compress multiple basic blocks into one jump so long as isForwardable is met.
        for (BasicBlock*& successor : block->successorBlocks()) {
            auto isForwardable = [&](BasicBlock* candidate) {
                return candidate != block
                    && candidate->size() == 1
                    && candidate->last().kind.opcode == Jump
                    && candidate->successorBlock(0) != candidate;
            };

            // Trace the chain of jump-only blocks with a Floyd tortoise/hare walk. The hare
            // (fast) moves two hops at a time, so it reaches the end of the chain first. When it
            // lands on a non-forwardable block, that block is the chain end and there is no cycle.
            // Orphan basic blocks can instead form a cycle with no end, in which case the hare
            // laps the tortoise (slow) and we bail. Cycles are extremely rare.
            bool sawCycle = false;
            BasicBlock* slow = successor;
            BasicBlock* fast = successor;
            while (true) {
                if (!isForwardable(fast))
                    break;
                fast = fast->successorBlock(0);
                if (!isForwardable(fast))
                    break;
                fast = fast->successorBlock(0);

                slow = slow->successorBlock(0);
                if (slow == fast) {
                    sawCycle = true;
                    break;
                }
            }

            // When there is no cycle, fast is the end of the chain.
            if (!sawCycle && fast != successor) {
                BasicBlock* newSuccessor = fast;
                dataLogLnIf(verbose, "Replacing ", pointerDump(block), "->", pointerDump(successor), " with ", pointerDump(block), "->", pointerDump(newSuccessor));
                for (BasicBlock* current = successor; current != newSuccessor;) {
                    BasicBlock* next = current->successorBlock(0);
                    current->successorBlock(0) = newSuccessor;
                    newSuccessor->addPredecessor(current);
                    current = next;
                }
                newSuccessor->addPredecessor(block);
                successor = newSuccessor;
                changed = true;
            }
        }

        // Now check if the block's terminal can be replaced with a jump. The terminal must not
        // have weird effects.
        if (block->numSuccessors() > 1
            && !block->last().hasNonControlEffects()) {
            // All of the successors must be the same.
            bool allSame = true;
            BasicBlock* firstSuccessor = block->successorBlock(0);
            for (unsigned i = 1; i < block->numSuccessors(); ++i) {
                if (block->successorBlock(i) != firstSuccessor) {
                    allSame = false;
                    break;
                }
            }
            if (allSame) {
                dataLogLnIf(verbose, "Changing ", pointerDump(block), "'s terminal to a Jump.");
                block->last() = Inst(Jump, block->last().origin);
                block->successors().resize(1);
                block->successors()[0].frequency() = FrequencyClass::Normal;
                changed = true;
            }
        }

        // Finally handle jumps to a block with one predecessor.
        if (block->numSuccessors() == 1
            && !block->last().hasNonControlEffects()) {
            BasicBlock* successor = block->successorBlock(0);
            if (successor != block && successor->numPredecessors() == 1) {
                RELEASE_ASSERT(successor->predecessor(0) == block);

                // We can merge the two blocks because the predecessor only jumps to the successor
                // and the successor is only reachable from the predecessor.

                // Remove the terminal.
                Value* origin = block->insts().takeLast().origin;

                // Append the full contents of the successor to the predecessor.
                block->insts().reserveCapacity(block->size() + successor->size());
                for (Inst& inst : *successor)
                    block->appendInst(WTF::move(inst));

                // Make sure that our successors are the successor's successors.
                block->successors() = WTF::move(successor->successors());

                // Make sure that the successor has nothing left in it except an oops.
                successor->resize(1);
                successor->last() = Inst(Oops, origin);
                successor->successors().clear();

                // Ensure that the predecessors of block's new successors know what's up.
                for (BasicBlock* newSuccessor : block->successorBlocks())
                    newSuccessor->replacePredecessor(successor, block);

                dataLogLnIf(verbose, "Merged ", pointerDump(block), "->", pointerDump(successor));
                changed = true;
            }
        }
    }

    if (changed)
        code.resetReachability();

    return changed;
}

} } } // namespace JSC::B3::Air

#endif // ENABLE(B3_JIT)


