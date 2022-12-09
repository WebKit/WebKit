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
#include "AirInstInlines.h"

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
    
    // The liveAtTail of each block automatically contains the LateUse's of the terminal.
    for (BasicBlock* block : code) {
        auto& liveAtTail = m_liveAtTail[block];
            
        block->last().forEach<Reg>(
            [&] (Reg& reg, Arg::Role role, Bank, Width width) {
                ASSERT(width <= Width64 || Options::useWebAssemblySIMD());
                if (Arg::isLateUse(role))
                    liveAtTail.add(reg, width);
            });
    }
        
    BitVector dirtyBlocks;
    for (size_t blockIndex = code.size(); blockIndex--;)
        dirtyBlocks.set(blockIndex);
        
    bool changed;
    do {
        changed = false;

        if (AirRegLivenessInternal::verbose) {
            dataLogLn("Next iteration");
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
            
        for (size_t blockIndex = code.size(); blockIndex--;) {
            BasicBlock* block = code[blockIndex];
            if (!block)
                continue;
                
            if (!dirtyBlocks.quickClear(blockIndex))
                continue;
                
            LocalCalc localCalc(*this, block);
            for (size_t instIndex = block->size(); instIndex--;)
                localCalc.execute(instIndex);
                
            // Handle the early def's of the first instruction.
            block->at(0).forEach<Reg>(
                [&] (Reg& reg, Arg::Role role, Bank, Width) {
                    if (Arg::isEarlyDef(role))
                        localCalc.m_workset.remove(reg);
                });
                
            auto& liveAtHead = m_liveAtHead[block];
            if (liveAtHead.subsumes(localCalc.m_workset))
                continue;
                
            liveAtHead.merge(localCalc.m_workset);
                
            for (BasicBlock* predecessor : block->predecessors()) {
                auto& liveAtTail = m_liveAtTail[predecessor];
                if (liveAtTail.subsumes(localCalc.m_workset))
                    continue;
                    
                liveAtTail.merge(localCalc.m_workset);
                dirtyBlocks.quickSet(predecessor->index());
                changed = true;
            }
        }
    } while (changed);

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

RegLiveness::~RegLiveness()
{
}

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

