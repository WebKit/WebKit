/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include "B3EnsureLoopPreHeaders.h"
#include <wtf/BitVector.h>
#include <wtf/IndexSet.h>

#if ENABLE(B3_JIT)

#include "B3BasicBlockInlines.h"
#include "B3BlockInsertionSet.h"
#include "B3NaturalLoops.h"
#include "B3ValueInlines.h"

namespace JSC { namespace B3 {

bool ensureLoopPreHeaders(Procedure& proc)
{
    NaturalLoops& loops = proc.naturalLoops();
    
    BlockInsertionSet insertionSet(proc);

    // In order to ensure that every loop has a unique preheader, we first need
    // to guarantee that every loop has a unique header. This involves finding
    // headers shared by more than one loop and replacing the headers of outer
    // loops with new blocks. These will generally become the preheaders of
    // enclosed loops in the subsequent pass.

    BitVector visitedLoops(loops.numLoops());
    Vector<BasicBlock*, 4> inBodyPredecessors;

    for (unsigned loopIndex = 0; loopIndex < loops.numLoops(); loopIndex ++) {
        if (visitedLoops.contains(loopIndex))
            continue;
        visitedLoops.set(loopIndex);

        const NaturalLoop& loop = loops.loop(loopIndex);
        auto header = loop.header();

        // If there are no enclosing loops other than the current one, we have
        // no additional work to do.

        auto enclosingLoops = loops.loopsOf(header);
        if (enclosingLoops.size() == 1)
            continue;

        // Otherwise, we must have multiple loops - for each of these loops
        // with the same header as the current one, create a new unique header
        // and fix up its predecessors and successors. We'll use enclosedHeader
        // to track the header block of the next innermost loop - this will be
        // the successor of any new header we insert.

        BasicBlock* enclosedHeader = nullptr;

        // We also need to track the visited backedges because if multiple
        // loops share a header, then predecessors of the header that are
        // within the loop applies to not only the current loop's backedges,
        // but the backedges of any loop contained within it. We use this set
        // to remember which backedges belonged to inner loops, so we don't
        // retarget them to the inserted headers of outer loops.

        IndexSet<BasicBlock*> visitedBackedges;

        for (auto enclosingLoop : enclosingLoops) {
            if (enclosingLoop->header() != header)
                break;
            visitedLoops.set(enclosingLoop->index());

            inBodyPredecessors.clear();
            for (BasicBlock* predecessor : header->predecessors()) {
                if (loops.belongsTo(predecessor, *enclosingLoop) && !visitedBackedges.contains(predecessor)) {
                    inBodyPredecessors.append(predecessor);
                    visitedBackedges.add(predecessor);
                }
            }

            // For the innermost loop, we reuse the existing header, so we
            // don't do anything other than visit our backedges and tell
            // the outer loops what our header is.

            if (enclosingLoop == enclosingLoops[0]) {
                enclosedHeader = header;
                continue;
            }

            BasicBlock* newHeader = insertionSet.insertBefore(header, header->frequency());
            newHeader->appendNew<Value>(proc, Jump, enclosedHeader->at(0)->origin());
            newHeader->setSuccessors(FrequentedBlock(enclosedHeader));

            for (BasicBlock* predecessor : inBodyPredecessors) {
                predecessor->replaceSuccessor(header, newHeader);
                newHeader->addPredecessor(predecessor);
                header->removePredecessor(predecessor);
            }

            enclosedHeader->addPredecessor(newHeader);

            // Since we added a new header, we should tell other new headers to
            // point to this new block, instead of creating more predecessors
            // of the original header block.

            enclosedHeader = newHeader;
        }
    }

    bool uniqueHeadersChangedGraph = false;
    if (insertionSet.execute()) {
        proc.invalidateCFG();
        uniqueHeadersChangedGraph = true;
    }

    // Now, we find or create unique preheaders for each loop.

    for (unsigned loopIndex = loops.numLoops(); loopIndex--;) {
        const NaturalLoop& loop = loops.loop(loopIndex);
        BasicBlock* header = loop.header();

        Vector<BasicBlock*, 4> outOfBodyPredecessors;
        double totalFrequency = 0;
        for (BasicBlock* predecessor : header->predecessors()) {
            if (loops.belongsTo(predecessor, loop))
                continue;
            
            outOfBodyPredecessors.append(predecessor);
            totalFrequency += predecessor->frequency();
        }

        if (outOfBodyPredecessors.size() <= 1)
            continue;
        
        BasicBlock* preHeader = insertionSet.insertBefore(header, totalFrequency);
        preHeader->appendNew<Value>(proc, Jump, header->at(0)->origin());
        preHeader->setSuccessors(FrequentedBlock(header));
        
        for (BasicBlock* predecessor : outOfBodyPredecessors) {
            predecessor->replaceSuccessor(header, preHeader);
            preHeader->addPredecessor(predecessor);
            header->removePredecessor(predecessor);
        }

        header->addPredecessor(preHeader);
    }
    
    if (insertionSet.execute()) {
        proc.invalidateCFG();
        return true;
    }
    
    return uniqueHeadersChangedGraph;
}

} } // namespace JSC::B3

#endif // ENABLE(B3_JIT)

