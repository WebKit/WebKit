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
#include "AirEnsureDedicatedLoopEntryExitBlocks.h"

#if ENABLE(B3_JIT)

#include "AirBlockInsertionSet.h"
#include "AirCode.h"
#include "AirNaturalLoops.h"

namespace JSC { namespace B3 { namespace Air {

void ensureDedicatedLoopEntryExitBlocks(Code& code)
{
    Dominators dominators(code);
    NaturalLoops loops(code, dominators);

    unsigned originalBlockCount = code.size();

    BlockInsertionSet insertionSet(code);

    // Pass 1: Ensure dedicated exit blocks.
    //
    // For each loop L, each exit successor S must have all its predecessors inside L.
    // When S has mixed predecessors (some in L, some not), we insert a pad block between
    // L's predecessors and S.
    //
    // We use innerMostLoopOf(pred) == L to assign each predecessor to exactly one loop.
    // This means inner loops claim their own predecessors, and outer loops only redirect
    // predecessors that aren't in any inner loop. This works because pad blocks inserted
    // for inner loops become exit successors of outer loops in the allocator's recomputed
    // NaturalLoops, with all their predecessors inside the outer loop by containment.
    for (unsigned loopIndex = loops.numLoops(); loopIndex--;) {
        const NaturalLoop& loop = loops.loop(loopIndex);

        for (unsigned i = 0; i < loop.size(); i++) {
            for (auto& succ : loop.at(i)->successors()) {
                BasicBlock* exitBlock = succ.block();
                // Skip new pad blocks since the exit edges they replaced have already been handled.
                // And we can't query loops on them since they didn't exist when loops was computed.
                if (exitBlock->index() >= originalBlockCount)
                    continue;
                if (loops.belongsTo(exitBlock, loop))
                    continue;

                // Only redirect predecessors whose innermost loop is L. Inner
                // loops handle their own predecessors and create their own pads.
                Vector<BasicBlock*, 8> loopPreds;
                for (BasicBlock* pred : exitBlock->predecessors()) {
                    if (pred->index() < originalBlockCount && loops.innerMostLoopOf(pred) == &loop)
                        loopPreds.append(pred);
                }
                if (loopPreds.isEmpty())
                    continue;
                // All predecessors are already in this loop — exit is already dedicated.
                if (loopPreds.size() == exitBlock->numPredecessors())
                    continue;

                BasicBlock* pad = insertionSet.insertBefore(exitBlock, exitBlock->frequency());
                pad->append(Jump, exitBlock->at(0).origin);
                pad->setSuccessors(FrequentedBlock(exitBlock));
                for (BasicBlock* loopPred : loopPreds) {
                    for (BasicBlock*& target : loopPred->successorBlocks()) {
                        if (target == exitBlock)
                            target = pad;
                    }
                    pad->addPredecessor(loopPred);
                    exitBlock->removePredecessor(loopPred);
                }
                exitBlock->addPredecessor(pad);
            }
        }
    }

    // Pass 2: Ensure non-critical entry edges.
    //
    // If any non-back-edge predecessor of a loop header has multiple successors (critical
    // entry edge), redirect all non-back-edge predecessors through a pre-header block.
    // This runs after the exit pass so entry mutations can't affect exit analysis. Exit
    // pads from pass 1 may be predecessors of a loop header (when an exit successor is
    // another loop's header), but belongsTo correctly identifies them as non-back-edge
    // predecessors since they aren't in the frozen NaturalLoops.
    for (unsigned loopIndex = loops.numLoops(); loopIndex--;) {
        const NaturalLoop& loop = loops.loop(loopIndex);
        BasicBlock* header = loop.header();

        Vector<BasicBlock*, 4> entryPreds;
        bool needsPreHeader = false;
        for (BasicBlock* pred : header->predecessors()) {
            // Skip pad blocks from the exit pass — they may be back-edge predecessors
            // (inner loop exiting to this header) and always have a single successor,
            // so they can't create critical entry edges.
            if (pred->index() >= originalBlockCount)
                continue;
            if (loops.belongsTo(pred, loop))
                continue;
            entryPreds.append(pred);
            if (pred->numSuccessors() > 1)
                needsPreHeader = true;
        }
        if (!needsPreHeader)
            continue;

        BasicBlock* preHeader = insertionSet.insertBefore(header, header->frequency());
        preHeader->append(Jump, header->at(0).origin);
        preHeader->setSuccessors(FrequentedBlock(header));
        for (BasicBlock* pred : entryPreds) {
            for (BasicBlock*& target : pred->successorBlocks()) {
                if (target == header)
                    target = preHeader;
            }
            preHeader->addPredecessor(pred);
            header->removePredecessor(pred);
        }
        header->addPredecessor(preHeader);
    }

    insertionSet.execute();
}

} } } // namespace JSC::B3::Air

#endif // ENABLE(B3_JIT)
