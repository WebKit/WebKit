/*
 * Copyright (C) 2015-2017 Apple Inc. All rights reserved.
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
#include "AirAllocateStackByGraphColoring.h"

#if ENABLE(B3_JIT)

#include "AirArgInlines.h"
#include "AirCode.h"
#include "AirHandleCalleeSaves.h"
#include "AirInstInlines.h"
#include "AirLiveness.h"
#include "AirPhaseScope.h"
#include "AirStackAllocation.h"
#include "StackAlignment.h"
#include <wtf/ListDump.h>

namespace JSC { namespace B3 { namespace Air {

namespace {

namespace AirAllocateStackByGraphColoringInternal {
static const bool verbose = false;
}

struct CoalescableMove {
    CoalescableMove()
    {
    }

    CoalescableMove(StackSlot* src, StackSlot* dst, double frequency)
        : src(src)
        , dst(dst)
        , frequency(frequency)
    {
    }
    
    bool operator==(const CoalescableMove& other) const
    {
        return src == other.src
            && dst == other.dst
            && frequency == other.frequency;
    }
    
    bool operator!=(const CoalescableMove& other) const
    {
        return !(*this == other);
    }
    
    explicit operator bool() const
    {
        return *this != CoalescableMove();
    }
    
    void dump(PrintStream& out) const
    {
        out.print(pointerDump(src), "->", pointerDump(dst), "(", frequency, ")");
    }
    
    StackSlot* src { nullptr };
    StackSlot* dst { nullptr };
    double frequency { PNaN };
};

} // anonymous namespace

void allocateStackByGraphColoring(Code& code)
{
    PhaseScope phaseScope(code, "allocateStackByGraphColoring");
    
    handleCalleeSaves(code);
    
    Vector<StackSlot*> assignedEscapedStackSlots =
        allocateAndGetEscapedStackSlotsWithoutChangingFrameSize(code);

    // Now we handle the spill slots.
    StackSlotLiveness liveness(code);
    IndexMap<StackSlot*, HashSet<StackSlot*>> interference(code.stackSlots().size());
    Vector<StackSlot*> slots;

    // We will perform some spill coalescing. To make that effective, we need to be able to identify
    // coalescable moves and handle them specially in interference analysis.
    auto isCoalescableMove = [&] (Inst& inst) -> bool {
        Width width;
        switch (inst.kind.opcode) {
        case Move:
            width = pointerWidth();
            break;
        case Move32:
        case MoveFloat:
            width = Width32;
            break;
        case MoveDouble:
            width = Width64;
            break;
        default:
            return false;
        }
        
        if (!Options::coalesceSpillSlots())
            return false;
        
        if (inst.args.size() != 3)
            return false;
        
        for (unsigned i = 0; i < 2; ++i) {
            Arg arg = inst.args[i];
            if (!arg.isStack())
                return false;
            StackSlot* slot = arg.stackSlot();
            if (slot->kind() != StackSlotKind::Spill)
                return false;
            if (slot->byteSize() != bytes(width))
                return false;
        }
        
        return true;
    };
    
    auto isUselessMove = [&] (Inst& inst) -> bool {
        return isCoalescableMove(inst) && inst.args[0] == inst.args[1];
    };
    
    auto addEdge = [&] (StackSlot* a, StackSlot* b) {
        interference[a].add(b);
        interference[b].add(a);
    };
    
    Vector<CoalescableMove> coalescableMoves;

    for (BasicBlock* block : code) {
        StackSlotLiveness::LocalCalc localCalc(liveness, block);

        auto interfere = [&] (unsigned instIndex) {
            if (AirAllocateStackByGraphColoringInternal::verbose)
                dataLog("Interfering: ", WTF::pointerListDump(localCalc.live()), "\n");

            Inst* prevInst = block->get(instIndex);
            Inst* nextInst = block->get(instIndex + 1);
            if (prevInst && isCoalescableMove(*prevInst)) {
                CoalescableMove move(prevInst->args[0].stackSlot(), prevInst->args[1].stackSlot(), block->frequency());
                
                coalescableMoves.append(move);
                
                for (StackSlot* otherSlot : localCalc.live()) {
                    if (otherSlot != move.src)
                        addEdge(move.dst, otherSlot);
                }
                
                prevInst = nullptr;
            }
            Inst::forEachDef<Arg>(
                prevInst, nextInst,
                [&] (Arg& arg, Arg::Role, Bank, Width) {
                    if (!arg.isStack())
                        return;
                    StackSlot* slot = arg.stackSlot();
                    if (slot->kind() != StackSlotKind::Spill)
                        return;

                    for (StackSlot* otherSlot : localCalc.live())
                        addEdge(slot, otherSlot);
                });
        };

        for (unsigned instIndex = block->size(); instIndex--;) {
            if (AirAllocateStackByGraphColoringInternal::verbose)
                dataLog("Analyzing: ", block->at(instIndex), "\n");

            // Kill dead stores. For simplicity we say that a store is killable if it has only late
            // defs and those late defs are to things that are dead right now. We only do that
            // because that's the only kind of dead stack store we will see here.
            Inst& inst = block->at(instIndex);
            if (!inst.hasNonArgEffects()) {
                bool ok = true;
                inst.forEachArg(
                    [&] (Arg& arg, Arg::Role role, Bank, Width) {
                        if (Arg::isEarlyDef(role)) {
                            ok = false;
                            return;
                        }
                        if (!Arg::isLateDef(role))
                            return;
                        if (!arg.isStack()) {
                            ok = false;
                            return;
                        }
                        StackSlot* slot = arg.stackSlot();
                        if (slot->kind() != StackSlotKind::Spill) {
                            ok = false;
                            return;
                        }

                        if (localCalc.isLive(slot)) {
                            ok = false;
                            return;
                        }
                    });
                if (ok)
                    inst = Inst();
            }
            
            interfere(instIndex);
            localCalc.execute(instIndex);
        }
        interfere(-1);
        
        block->insts().removeAllMatching(
            [&] (const Inst& inst) -> bool {
                return !inst;
            });
    }

    if (AirAllocateStackByGraphColoringInternal::verbose) {
        for (StackSlot* slot : code.stackSlots())
            dataLog("Interference of ", pointerDump(slot), ": ", pointerListDump(interference[slot]), "\n");
    }
    
    // Now try to coalesce some moves.
    std::sort(
        coalescableMoves.begin(), coalescableMoves.end(),
        [&] (CoalescableMove& a, CoalescableMove& b) -> bool {
            return a.frequency > b.frequency;
        });
    
    IndexMap<StackSlot*, StackSlot*> remappedStackSlots(code.stackSlots().size());
    auto remap = [&] (StackSlot* slot) -> StackSlot* {
        if (!slot)
            return nullptr;
        for (;;) {
            StackSlot* remappedSlot = remappedStackSlots[slot];
            if (!remappedSlot)
                return slot;
            slot = remappedSlot;
        }
    };
    
    for (CoalescableMove& move : coalescableMoves) {
        move.src = remap(move.src);
        move.dst = remap(move.dst);
        if (move.src == move.dst)
            continue;
        if (interference[move.src].contains(move.dst))
            continue;
        
        StackSlot* slotToKill = move.src;
        StackSlot* slotToKeep = move.dst;
        
        remappedStackSlots[slotToKill] = slotToKeep;
        for (StackSlot* interferingSlot : interference[slotToKill]) {
            if (interferingSlot == slotToKill)
                continue;
            interference[interferingSlot].remove(slotToKill);
            interference[interferingSlot].add(slotToKeep);
        }
        interference[slotToKeep].add(interference[slotToKill].begin(), interference[slotToKill].end());
        interference[slotToKill].clear();
    }
    
    for (BasicBlock* block : code) {
        for (Inst& inst : *block) {
            for (Arg& arg : inst.args) {
                if (arg.isStack())
                    arg = Arg::stack(remap(arg.stackSlot()), arg.offset());
            }
            if (isUselessMove(inst))
                inst = Inst();
        }
    }
    
    // Now we assign stack locations. At its heart this algorithm is just first-fit. For each
    // StackSlot we just want to find the offsetFromFP that is closest to zero while ensuring no
    // overlap with other StackSlots that this overlaps with.
    Vector<StackSlot*> otherSlots = assignedEscapedStackSlots;
    for (StackSlot* slot : code.stackSlots()) {
        if (remappedStackSlots[slot])
            continue;
        
        if (slot->offsetFromFP()) {
            // Already assigned an offset.
            continue;
        }

        HashSet<StackSlot*>& interferingSlots = interference[slot];
        otherSlots.resize(assignedEscapedStackSlots.size());
        otherSlots.resize(assignedEscapedStackSlots.size() + interferingSlots.size());
        unsigned nextIndex = assignedEscapedStackSlots.size();
        for (StackSlot* otherSlot : interferingSlots)
            otherSlots[nextIndex++] = otherSlot;

        assign(slot, otherSlots);
    }

    updateFrameSizeBasedOnStackSlots(code);
    code.setStackIsAllocated(true);
}

} } } // namespace JSC::B3::Air

#endif // ENABLE(B3_JIT)

