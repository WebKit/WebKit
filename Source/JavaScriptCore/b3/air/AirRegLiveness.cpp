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

namespace JSC { namespace B3 { namespace Air {

RegLiveness::RegLiveness(Code& code)
    : m_liveAtHead(code.size())
    , m_liveAtTail(code.size())
{
    // The liveAtTail of each block automatically contains the LateUse's of the terminal.
    for (BasicBlock* block : code) {
        RegisterSet& liveAtTail = m_liveAtTail[block];
            
        block->last().forEach<Reg>(
            [&] (Reg& reg, Arg::Role role, Bank, Width) {
                if (Arg::isLateUse(role))
                    liveAtTail.add(reg);
            });
    }
        
    BitVector dirtyBlocks;
    for (size_t blockIndex = code.size(); blockIndex--;)
        dirtyBlocks.set(blockIndex);
        
    bool changed;
    do {
        changed = false;
            
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
                
            RegisterSet& liveAtHead = m_liveAtHead[block];
            if (liveAtHead.subsumes(localCalc.m_workset))
                continue;
                
            liveAtHead.merge(localCalc.m_workset);
                
            for (BasicBlock* predecessor : block->predecessors()) {
                RegisterSet& liveAtTail = m_liveAtTail[predecessor];
                if (liveAtTail.subsumes(localCalc.m_workset))
                    continue;
                    
                liveAtTail.merge(localCalc.m_workset);
                dirtyBlocks.quickSet(predecessor->index());
                changed = true;
            }
        }
    } while (changed);
}

RegLiveness::~RegLiveness()
{
}

void RegLiveness::LocalCalc::execute(unsigned instIndex)
{
    Inst& inst = m_block->at(instIndex);
            
    // First handle the early def's of the next instruction.
    if (instIndex + 1 < m_block->size()) {
        Inst& nextInst = m_block->at(instIndex + 1);
        nextInst.forEach<Reg>(
            [&] (Reg& reg, Arg::Role role, Bank, Width) {
                if (Arg::isEarlyDef(role))
                    m_workset.remove(reg);
            });
    }
            
    // Then handle def's.
    inst.forEach<Reg>(
        [&] (Reg& reg, Arg::Role role, Bank, Width) {
            if (Arg::isLateDef(role))
                m_workset.remove(reg);
        });
            
    // Then handle use's.
    inst.forEach<Reg>(
        [&] (Reg& reg, Arg::Role role, Bank, Width) {
            if (Arg::isEarlyUse(role))
                m_workset.add(reg);
        });
            
    // And finally, handle the late use's of the previous instruction.
    if (instIndex) {
        Inst& prevInst = m_block->at(instIndex - 1);
        prevInst.forEach<Reg>(
            [&] (Reg& reg, Arg::Role role, Bank, Width) {
                if (Arg::isLateUse(role))
                    m_workset.add(reg);
            });
    }
}

} } } // namespace JSC::B3::Air

#endif // ENABLE(B3_JIT)

