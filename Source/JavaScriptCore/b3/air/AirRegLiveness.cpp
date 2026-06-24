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
#include "AirRegLiveness.h"

#if ENABLE(B3_JIT)

#include "AirArgInlines.h"
#include "AirCFG.h"
#include "AirInstInlines.h"
#include <wtf/GraphOrdering.h>

namespace AirRegLivenessInternal {
    static constexpr bool verbose = false;
};

namespace JSC { namespace B3 { namespace Air {

RegLiveness::RegLiveness(Code& code)
    : m_liveAtHead(code.size())
    , m_liveAtTail(code.size())
    , m_actions(code.size())
{
    dataLogLnIf(AirRegLivenessInternal::verbose, "Compute reg liveness for code: ", code);
    // Compute constraints.
    for (BasicBlock* block : code) {
        ActionsForBoundary& actionsForBoundary = m_actions[block];
        actionsForBoundary.resize(block->size() + 1);

        for (size_t instIndex = block->size(); instIndex--;) {
            Inst& inst = block->at(instIndex);
            inst.forEach<Reg>(
                [&] (Reg& reg, Arg::Role role, Bank, Width width) {
                    if (Arg::isEarlyUse(role))
                        actionsForBoundary[instIndex].use.add(reg, width);
                    if (Arg::isEarlyDef(role))
                        actionsForBoundary[instIndex].def.add(reg, width);
                    if (Arg::isLateUse(role))
                        actionsForBoundary[instIndex + 1].use.add(reg, width);
                    if (Arg::isLateDef(role))
                        actionsForBoundary[instIndex + 1].def.add(reg, width);
                });
        }
    }

    IndexMap<BasicBlock*, RegisterSet> gen(code.size());
    IndexMap<BasicBlock*, RegisterSet> kill(code.size());

    for (BasicBlock* block : code) {
        ActionsForBoundary& actionsForBoundary = m_actions[block];

        // kill: every register defined anywhere in the block. The whole register width is included so
        // that excluding it later clears a register entirely, matching LocalCalc's width-agnostic remove.
        RegisterSet& killSet = kill[block];
        for (size_t boundary = 0; boundary <= block->size(); ++boundary)
            killSet.merge(actionsForBoundary[boundary].def);
        killSet.includeWholeRegisterWidth();

        // gen: the block's transfer function applied to an empty liveAtTail, i.e. the uses exposed at the head.
        RegisterSet workset;
        for (size_t instIndex = block->size(); instIndex--;) {
            actionsForBoundary[instIndex + 1].def.forEach([&](Reg r) {
                workset.remove(r);
            });
            workset.merge(actionsForBoundary[instIndex].use);
        }
        // Handle the early def's of the first instruction.
        actionsForBoundary[0].def.forEach([&](Reg r) {
            workset.remove(r);
        });
        gen[block] = workset;

        // The liveAtTail of each block automatically contains the LateUse's of the terminal, which are
        // exactly the uses at the final boundary.
        m_liveAtTail[block].merge(actionsForBoundary[block->size()].use);
    }

    if (AirRegLivenessInternal::verbose) {
        dataLogLn("Initial state");
        for (size_t blockIndex = code.size(); blockIndex--;) {
            BasicBlock* block = code[blockIndex];
            if (!block)
                continue;
            dataLogLn("Block ", blockIndex, " gen: ", gen[block], " kill: ", kill[block], " live at tail: ", m_liveAtTail[block]);
        }
    }

    CFG cfg(code);
    Vector<unsigned, 64> worklist;
    BitVector dirtyBlocks(code.size());
    worklist.reserveInitialCapacity(code.size());
    appendNodeIndicesInOrder(cfg, GraphOrder::PostOrder, dirtyBlocks, worklist);
    worklist.reverse();
    // Queue any active block the DFS did not reach (e.g. unreachable but not yet pruned).
    for (unsigned blockIndex = 0; blockIndex < code.size(); ++blockIndex) {
        if (code[blockIndex] && !dirtyBlocks.quickSet(blockIndex))
            worklist.append(blockIndex);
    }

    while (!worklist.isEmpty()) {
        unsigned blockIndex = worklist.takeLast();
        bool cleared = dirtyBlocks.quickClear(blockIndex);
        ASSERT_UNUSED(cleared, cleared);

        BasicBlock* block = code[blockIndex];
        ASSERT(block);

        // liveAtHead = gen | (liveAtTail & ~kill)
        RegisterSet liveAtHead = gen[block];
        RegisterSet throughLiveness = m_liveAtTail[block];
        throughLiveness.exclude(kill[block]);
        liveAtHead.merge(throughLiveness);

        if (m_liveAtHead[block].subsumes(liveAtHead))
            continue;

        m_liveAtHead[block] = liveAtHead;

        for (BasicBlock* predecessor : block->predecessors()) {
            auto& liveAtTail = m_liveAtTail[predecessor];
            if (liveAtTail.subsumes(liveAtHead))
                continue;

            liveAtTail.merge(liveAtHead);
            if (!dirtyBlocks.quickSet(predecessor->index()))
                worklist.append(predecessor->index());
        }
    }

    if (AirRegLivenessInternal::verbose) {
        dataLogLn("Reg liveness result:");
        for (size_t blockIndex = code.size(); blockIndex--;) {
            BasicBlock* block = code[blockIndex];
            if (!block)
                continue;
            ActionsForBoundary& actionsForBoundary = m_actions[block];
            dataLogLn("Block ", blockIndex);
            dataLogLn("Live at head: ", m_liveAtHead[block]);
            dataLogLn("Live at tail: ", m_liveAtTail[block]);

            for (size_t instIndex = block->size(); instIndex--;) {
                dataLogLn(block->at(instIndex), " | use: ", actionsForBoundary[instIndex].use, " def: ", actionsForBoundary[instIndex].def);
            }
        }
    }
}

RegLiveness::~RegLiveness() = default;

RegLiveness::LocalCalcForUnifiedTmpLiveness::LocalCalcForUnifiedTmpLiveness(UnifiedTmpLiveness& liveness, BasicBlock* block)
    : LocalCalcBase(block)
    , m_code(liveness.code)
    , m_actions(liveness.actions[block])
{
    for (Tmp tmp : liveness.liveAtTail(block)) {
        if (tmp.isReg())
            m_workset.add(tmp.reg(), m_code.usesSIMD() ? conservativeWidth(tmp.reg()) : conservativeWidthWithoutVectors(tmp.reg()));
    }
}

void RegLiveness::LocalCalcForUnifiedTmpLiveness::execute(unsigned instIndex)
{
    for (unsigned index : m_actions[instIndex + 1].def) {
        Tmp tmp = Tmp::tmpForLinearIndex(m_code, index);
        if (tmp.isReg())
            m_workset.remove(tmp.reg());
    }
    for (unsigned index : m_actions[instIndex].use) {
        Tmp tmp = Tmp::tmpForLinearIndex(m_code, index);
        if (tmp.isReg())
            m_workset.add(tmp.reg(), m_code.usesSIMD() ? conservativeWidth(tmp.reg()) : conservativeWidthWithoutVectors(tmp.reg()));
    }
}

} } } // namespace JSC::B3::Air

#endif // ENABLE(B3_JIT)

