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

#ifndef AirLiveness_h
#define AirLiveness_h

#if ENABLE(B3_JIT)

#include "AirBasicBlock.h"
#include "B3IndexMap.h"
#include "B3IndexSet.h"

namespace JSC { namespace B3 { namespace Air {

// You can compute liveness over Tmp's or over Arg's. If you compute over Arg's, you get both
// stack liveness and tmp liveness.
template<typename Thing>
class Liveness {
public:
    template<typename T>
    static bool isAlive(const T& thing) { return thing.isAlive(); }

    static bool isAlive(StackSlot* slot) { return slot->kind() == StackSlotKind::Anonymous; }
    
    Liveness(Code& code)
    {
        m_liveAtHead.resize(code.size());
        m_liveAtTail.resize(code.size());

        // The liveAtTail of each block automatically contains the LateUse's of the terminal.
        for (BasicBlock* block : code) {
            HashSet<Thing>& live = m_liveAtTail[block];
            block->last().forEach<Thing>(
                [&] (Thing& thing, Arg::Role role, Arg::Type) {
                    if (Arg::isLateUse(role))
                        live.add(thing);
                });
        }

        IndexSet<BasicBlock> seen;

        bool changed = true;
        while (changed) {
            changed = false;

            for (size_t blockIndex = code.size(); blockIndex--;) {
                BasicBlock* block = code.at(blockIndex);
                if (!block)
                    continue;
                LocalCalc localCalc(*this, block);
                for (size_t instIndex = block->size(); instIndex--;)
                    localCalc.execute(instIndex);
                bool firstTime = seen.add(block);
                if (!firstTime && localCalc.live() == m_liveAtHead[block])
                    continue;
                changed = true;
                for (BasicBlock* predecessor : block->predecessors()) {
                    m_liveAtTail[predecessor].add(
                        localCalc.live().begin(), localCalc.live().end());
                }
                m_liveAtHead[block] = localCalc.takeLive();
            }
        }
    }

    const HashSet<Thing>& liveAtHead(BasicBlock* block) const
    {
        return m_liveAtHead[block];
    }

    const HashSet<Thing>& liveAtTail(BasicBlock* block) const
    {
        return m_liveAtTail[block];
    }

    // This calculator has to be run in reverse.
    class LocalCalc {
    public:
        LocalCalc(Liveness& liveness, BasicBlock* block)
            : m_live(liveness.liveAtTail(block))
            , m_block(block)
        {
        }

        const HashSet<Thing>& live() const { return m_live; }
        HashSet<Thing>&& takeLive() { return WTF::move(m_live); }

        void execute(unsigned instIndex)
        {
            Inst& inst = m_block->at(instIndex);
            
            // First handle def's.
            inst.forEach<Thing>(
                [this] (Thing& arg, Arg::Role role, Arg::Type) {
                    if (!isAlive(arg))
                        return;
                    if (Arg::isDef(role))
                        m_live.remove(arg);
                });

            // Then handle use's.
            inst.forEach<Thing>(
                [this] (Thing& arg, Arg::Role role, Arg::Type) {
                    if (!isAlive(arg))
                        return;
                    if (Arg::isEarlyUse(role))
                        m_live.add(arg);
                });

            // And finally, handle the late use's of the previous instruction.
            if (instIndex) {
                Inst& prevInst = m_block->at(instIndex - 1);

                prevInst.forEach<Thing>(
                    [this] (Thing& arg, Arg::Role role, Arg::Type) {
                        if (!Arg::isLateUse(role))
                            return;
                        if (isAlive(arg))
                            m_live.add(arg);
                    });
            }
        }

    private:
        HashSet<Thing> m_live;
        BasicBlock* m_block;
    };

private:
    IndexMap<BasicBlock, HashSet<Thing>> m_liveAtHead;
    IndexMap<BasicBlock, HashSet<Thing>> m_liveAtTail;
};

} } } // namespace JSC::B3::Air

#endif // ENABLE(B3_JIT)

#endif // AirLiveness_h

