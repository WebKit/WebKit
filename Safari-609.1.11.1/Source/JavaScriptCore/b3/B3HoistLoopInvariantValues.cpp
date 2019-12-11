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
#include "B3HoistLoopInvariantValues.h"

#if ENABLE(B3_JIT)

#include "B3BackwardsDominators.h"
#include "B3BasicBlockInlines.h"
#include "B3Dominators.h"
#include "B3EnsureLoopPreHeaders.h"
#include "B3NaturalLoops.h"
#include "B3PhaseScope.h"
#include "B3ProcedureInlines.h"
#include "B3ValueInlines.h"
#include <wtf/RangeSet.h>

namespace JSC { namespace B3 {

bool hoistLoopInvariantValues(Procedure& proc)
{
    PhaseScope phaseScope(proc, "hoistLoopInvariantValues");
    
    ensureLoopPreHeaders(proc);
    
    NaturalLoops& loops = proc.naturalLoops();
    if (!loops.numLoops())
        return false;

    proc.resetValueOwners();
    Dominators& dominators = proc.dominators();
    BackwardsDominators& backwardsDominators = proc.backwardsDominators();
    
    // FIXME: We should have a reusable B3::EffectsSet data structure.
    // https://bugs.webkit.org/show_bug.cgi?id=174762
    struct LoopData {
        RangeSet<HeapRange> writes;
        bool writesLocalState { false };
        bool writesPinned { false };
        bool sideExits { false };
        BasicBlock* preHeader { nullptr };
    };
    
    IndexMap<NaturalLoop, LoopData> data(loops.numLoops());
    
    for (unsigned loopIndex = loops.numLoops(); loopIndex--;) {
        const NaturalLoop& loop = loops.loop(loopIndex);
        for (BasicBlock* predecessor : loop.header()->predecessors()) {
            if (loops.belongsTo(predecessor, loop))
                continue;
            RELEASE_ASSERT(!data[loop].preHeader);
            data[loop].preHeader = predecessor;
        }
    }
    
    for (BasicBlock* block : proc) {
        const NaturalLoop* loop = loops.innerMostLoopOf(block);
        if (!loop)
            continue;
        for (Value* value : *block) {
            Effects effects = value->effects();
            data[*loop].writes.add(effects.writes);
            data[*loop].writesLocalState |= effects.writesLocalState;
            data[*loop].writesPinned |= effects.writesPinned;
            data[*loop].sideExits |= effects.exitsSideways;
        }
    }
    
    for (unsigned loopIndex = loops.numLoops(); loopIndex--;) {
        const NaturalLoop& loop = loops.loop(loopIndex);
        for (const NaturalLoop* current = loops.innerMostOuterLoop(loop); current; current = loops.innerMostOuterLoop(*current)) {
            data[*current].writes.addAll(data[loop].writes);
            data[*current].writesLocalState |= data[loop].writesLocalState;
            data[*current].writesPinned |= data[loop].writesPinned;
            data[*current].sideExits |= data[loop].sideExits;
        }
    }
    
    bool changed = false;
    
    // Pre-order ensures that we visit our dominators before we visit ourselves. Otherwise we'd miss some
    // hoisting opportunities in complex CFGs.
    for (BasicBlock* block : proc.blocksInPreOrder()) {
        Vector<const NaturalLoop*> blockLoops = loops.loopsOf(block);
        if (blockLoops.isEmpty())
            continue;
        
        for (Value*& value : *block) {
            Effects effects = value->effects();
            
            // We never hoist write effects or control constructs.
            if (effects.mustExecute())
                continue;

            // Try outermost loop first.
            for (unsigned i = blockLoops.size(); i--;) {
                const NaturalLoop& loop = *blockLoops[i];
                
                bool ok = true;
                for (Value* child : value->children()) {
                    if (!dominators.dominates(child->owner, data[loop].preHeader)) {
                        ok = false;
                        break;
                    }
                }
                if (!ok)
                    continue;
                
                if (effects.controlDependent) {
                    if (!backwardsDominators.dominates(block, data[loop].preHeader))
                        continue;
                    
                    // FIXME: This is super conservative. In reality, we just need to make sure that there
                    // aren't any side exits between here and the pre-header according to backwards search.
                    // https://bugs.webkit.org/show_bug.cgi?id=174763
                    if (data[loop].sideExits)
                        continue;
                }
                
                if (effects.readsPinned && data[loop].writesPinned)
                    continue;
                
                if (effects.readsLocalState && data[loop].writesLocalState)
                    continue;
                
                if (data[loop].writes.overlaps(effects.reads))
                    continue;
                
                data[loop].preHeader->appendNonTerminal(value);
                value = proc.add<Value>(Nop, Void, value->origin());
                changed = true;
            }
        }
    }
    
    return changed;
}

} } // namespace JSC::B3

#endif // ENABLE(B3_JIT)

