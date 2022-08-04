/*
 * Copyright (C) 2015-2021 Apple Inc. All rights reserved.
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
#include <wtf/InterferenceGraph.h>
#include <wtf/ListDump.h>

namespace JSC { namespace B3 { namespace Air {

namespace {

namespace AirAllocateStackByGraphColoringInternal {
static constexpr bool verbose = false;
static constexpr bool reportLargeMemoryUses = false;
}

class StackAllocatorBase {
protected:
    StackAllocatorBase(Code& code)
        : m_code(code)
        , m_remappedStackSlotIndices(code.stackSlots().size())
    {
        for (unsigned i = 0; i < m_remappedStackSlotIndices.size(); ++i)
            m_remappedStackSlotIndices[i] = i;
    }

    // We will perform some spill coalescing. To make that effective, we need to be able to identify
    // coalescable moves and handle them specially in interference analysis.
    bool isCoalescableMove(Inst& inst) const
    {
        if (!Options::coalesceSpillSlots())
            return false;

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

        if (inst.args.size() != 3)
            return false;

        for (unsigned i = 0; i < 2; ++i) {
            Arg arg = inst.args[i];
            if (!arg.isStack())
                return false;
            StackSlot* slot = arg.stackSlot();
            if (slot->kind() != StackSlotKind::Spill)
                return false;
            if (slot->byteSize() != bytesForWidth(width))
                return false;
        }

        return true;
    }

    bool isUselessMove(Inst& inst) const
    {
        return isCoalescableMove(inst) && inst.args[0] == inst.args[1];
    }

    unsigned remap(unsigned slotIndex) const
    {
        for (;;) {
            unsigned remappedSlotIndex = m_remappedStackSlotIndices[slotIndex];
            if (remappedSlotIndex == slotIndex)
                return slotIndex;
            slotIndex = remappedSlotIndex;
        }
    }

    StackSlot* remapStackSlot(StackSlot* slot) const
    {
        return m_code.stackSlots()[remap(slot->index())];
    }

    bool isRemappedSlotIndex(unsigned slotIndex) const
    {
        return m_remappedStackSlotIndices[slotIndex] != slotIndex;
    };

    Code& m_code;
    Vector<unsigned> m_remappedStackSlotIndices;
};

template<typename InterferenceGraph>
class GraphColoringStackAllocator final : public StackAllocatorBase {
    using IndexType = typename InterferenceGraph::IndexType;
public:
    GraphColoringStackAllocator(Code& code)
        : StackAllocatorBase(code)
        , m_interference()
        , m_coalescableMoves()
    {
        m_interference.setMaxIndex(m_code.stackSlots().size());
    }

    void run(const Vector<StackSlot*>& assignedEscapedStackSlots)
    {
        StackSlotLiveness liveness(m_code);

        for (BasicBlock* block : m_code) {
            StackSlotLiveness::LocalCalc localCalc(liveness, block);

            auto interfere = [&] (unsigned instIndex) {
                if (AirAllocateStackByGraphColoringInternal::verbose)
                    dataLog("Interfering: ", WTF::pointerListDump(localCalc.live()), "\n");

                Inst* prevInst = block->get(instIndex);
                Inst* nextInst = block->get(instIndex + 1);
                if (prevInst && isCoalescableMove(*prevInst)) {
                    CoalescableMove move(prevInst->args[0].stackSlot()->index(), prevInst->args[1].stackSlot()->index(), block->frequency());

                    m_coalescableMoves.append(move);

                    for (StackSlot* otherSlot : localCalc.live()) {
                        unsigned otherSlotIndex = otherSlot->index();
                        if (otherSlotIndex != move.src)
                            addEdge(move.dst, otherSlotIndex);
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

        if (AirAllocateStackByGraphColoringInternal::reportLargeMemoryUses && m_interference.memoryUse() > 1024) {
            dataLog("GraphColoringStackAllocator stackSlots, interferenceGraph(kB), coalescable moves(kB): ", m_code.stackSlots().size(), " : ");
            m_interference.dumpMemoryUseInKB();
            dataLog(", ", (m_coalescableMoves.size() * sizeof(CoalescableMove)) / 1024);
            dataLogLn();
        }

        if (AirAllocateStackByGraphColoringInternal::verbose) {
            for (StackSlot* slot : m_code.stackSlots()) {
                dataLog("Interference of ", pointerDump(slot), ": ");
                for (unsigned slotIndex : m_interference[slot->index()])
                    dataLog(pointerDump(m_code.stackSlots()[slotIndex]));
                dataLog("\n");
            }
        }

        // Now try to coalesce some moves.
        std::sort(
            m_coalescableMoves.begin(), m_coalescableMoves.end(),
            [&] (CoalescableMove& a, CoalescableMove& b) -> bool {
                return a.frequency > b.frequency;
            });

        for (const CoalescableMove& move : m_coalescableMoves) {
            IndexType slotToKill = remap(move.src);
            IndexType slotToKeep = remap(move.dst);
            if (slotToKill == slotToKeep)
                continue;
            if (m_interference.contains(slotToKill, slotToKeep))
                continue;

            m_remappedStackSlotIndices[slotToKill] = slotToKeep;

            for (IndexType interferingSlot : m_interference[slotToKill])
                addEdge(interferingSlot, slotToKeep);
            m_interference.mayClear(slotToKill);
        }

        for (BasicBlock* block : m_code) {
            for (Inst& inst : *block) {
                for (Arg& arg : inst.args) {
                    if (arg.isStack())
                        arg = Arg::stack(remapStackSlot(arg.stackSlot()), arg.offset());
                }
                if (isUselessMove(inst))
                    inst = Inst();
            }
        }

        // Now we assign stack locations. At its heart this algorithm is just first-fit. For each
        // StackSlot we just want to find the offsetFromFP that is closest to zero while ensuring no
        // overlap with other StackSlots that this overlaps with.
        Vector<StackSlot*> otherSlots = assignedEscapedStackSlots;
        for (StackSlot* slot : m_code.stackSlots()) {
            if (isRemappedSlotIndex(slot->index()))
                continue;

            if (slot->offsetFromFP()) {
                // Already assigned an offset.
                continue;
            }

            otherSlots.resize(assignedEscapedStackSlots.size());
            for (unsigned otherSlotIndex : m_interference[slot->index()]) {
                if (isRemappedSlotIndex(otherSlotIndex))
                    continue;
                StackSlot* otherSlot = m_code.stackSlots()[otherSlotIndex];
                otherSlots.append(otherSlot);
            }

            assign(slot, otherSlots);
        }
    }

private:
    struct CoalescableMove {
        CoalescableMove()
        {
        }

        CoalescableMove(IndexType src, IndexType dst, double frequency)
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
            out.print(src, "->", dst, "(", frequency, ")");
        }

        IndexType src { std::numeric_limits<IndexType>::max() };
        IndexType dst { std::numeric_limits<IndexType>::max() };
        float frequency { 0.0 };
    };

    void addEdge(IndexType u, IndexType v)
    {
        if (u == v)
            return;
        m_interference.add(u, v);
    }

    void addEdge(StackSlot* u, StackSlot* v)
    {
        addEdge(u->index(), v->index());
    }

    InterferenceGraph m_interference;
    Vector<CoalescableMove> m_coalescableMoves;
};

// We try to avoid computing the liveness information if there is no spill slot to allocate
bool tryTrivialStackAllocation(Code& code)
{
    for (StackSlot* slot : code.stackSlots()) {
        if (slot->offsetFromFP())
            continue;
        return false;
    }

    return true;
}

} // anonymous namespace

void allocateStackByGraphColoring(Code& code)
{
    RELEASE_ASSERT(!Options::useWebAssemblySIMD());
    PhaseScope phaseScope(code, "allocateStackByGraphColoring");

    handleCalleeSaves(code);

    Vector<StackSlot*> assignedEscapedStackSlots =
        allocateAndGetEscapedStackSlotsWithoutChangingFrameSize(code);

    if (!tryTrivialStackAllocation(code)) {
        if (code.stackSlots().size() < WTF::maxSizeForSmallInterferenceGraph) {
            GraphColoringStackAllocator<SmallIterableInterferenceGraph> allocator(code);
            allocator.run(assignedEscapedStackSlots);
        } else if (code.stackSlots().size() < std::numeric_limits<uint16_t>::max()) {
            GraphColoringStackAllocator<LargeIterableInterferenceGraph> allocator(code);
            allocator.run(assignedEscapedStackSlots);
        } else {
            GraphColoringStackAllocator<HugeIterableInterferenceGraph> allocator(code);
            allocator.run(assignedEscapedStackSlots);
        }
    }

    updateFrameSizeBasedOnStackSlots(code);
    code.setStackIsAllocated(true);
}

} } } // namespace JSC::B3::Air

#endif // ENABLE(B3_JIT)

