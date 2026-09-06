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
#include "AirCFG.h"
#include "AirCode.h"
#include "AirHandleCalleeSaves.h"
#include "AirInstInlines.h"
#include "AirPhaseScope.h"
#include "AirStackAllocation.h"
#include "AirStackAllocatorStats.h"
#include "CompilerTimingScope.h"
#include <wtf/ForbidHeapAllocation.h>
#include <wtf/InterferenceGraph.h>
#include <wtf/Liveness.h>

namespace JSC { namespace B3 { namespace Air {

namespace {

namespace AirAllocateStackByGraphColoringInternal {
static constexpr bool verbose = false;
static constexpr bool reportLargeMemoryUses = false;
}

// We will perform some spill coalescing. To make that effective, we need to be able to identify
// coalescable moves and handle them specially in interference analysis.
bool isCoalescableMove(Inst& inst, bool coalesceSpillSlots)
{
    if (!coalesceSpillSlots)
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
    case MoveVector:
        width = Width128;
        break;
    default:
        return false;
    }

    if (inst.args().size() != 3)
        return false;

    for (unsigned i = 0; i < 2; ++i) {
        Arg arg = inst.args()[i];
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

bool isUselessMove(Inst& inst, bool coalesceSpillSlots)
{
    return isCoalescableMove(inst, coalesceSpillSlots) && inst.args()[0] == inst.args()[1];
}

// The stack slot mentions, dead store candidates and coalescable move sites of a whole function,
// collected in one forward walk over the instructions.
//
// A mention carries the instruction boundary it happens at, and they land in one flat array sorted by
// boundary: an early action of instruction i belongs to boundary i and a late action to boundary
// i + 1, so staging the late actions of i and appending them once i is decoded keeps it sorted.
//
// The kill candidates cannot be derived from the mentions: an instruction with no arguments at all
// satisfies the predicate vacuously. The coalescable moves have to reach coalesceSlots in the order
// the interference build reports them, because it sorts them with an unstable sort and a different
// order would silently change which slots get coalesced.
class StackSlotActions {
    WTF_MAKE_NONCOPYABLE(StackSlotActions);
    WTF_FORBID_HEAP_ALLOCATION;
public:
    enum ActionKind : uint8_t { UseAction, EarlyDefAction, LateDefAction };

    struct Action {
        uint32_t boundary { 0 };
        uint32_t slotIndex { 0 };
        ActionKind kind { UseAction };
        bool isSpill { false };
    };

    struct MoveSite {
        uint32_t instIndex { 0 };
        uint32_t src { 0 };
        uint32_t dst { 0 };
    };

    StackSlotActions(Code& code, bool coalesceSpillSlots)
        : m_code(code)
    {
        CompilerTimingScope timingScope("Air"_s, "StackAllocator::actions"_s);

        m_blockData.grow(code.size());

        Vector<Action, 8> lateActions;
        for (BasicBlock* block : code) {
            BlockData& data = m_blockData[block->index()];
            data.actionStart = m_actions.size();
            data.killStart = m_killCandidates.size();
            data.moveStart = m_moveSites.size();

            unsigned blockSize = block->size();
            for (unsigned instIndex = 0; instIndex < blockSize; ++instIndex) {
                Inst& inst = block->at(instIndex);

                // A dead store candidate has no effects beyond its arguments and defines only spill
                // slots, late. That is the only kind of dead stack store this phase will see.
                bool isKillCandidate = !inst.hasNonArgEffects();
                lateActions.shrink(0);
                inst.forEachArg(
                    [&] (Arg& arg, Arg::Role role, Bank, Width) {
                        bool isSpill = false;
                        if (arg.isStack()) {
                            StackSlot* slot = arg.stackSlot();
                            uint32_t index = slot->index();
                            isSpill = slot->kind() == StackSlotKind::Spill;
                            if (Arg::isEarlyUse(role))
                                m_actions.append({ instIndex, index, UseAction, isSpill });
                            if (Arg::isEarlyDef(role))
                                m_actions.append({ instIndex, index, EarlyDefAction, isSpill });
                            if (Arg::isLateUse(role))
                                lateActions.append({ instIndex + 1, index, UseAction, isSpill });
                            if (Arg::isLateDef(role))
                                lateActions.append({ instIndex + 1, index, LateDefAction, isSpill });
                        }
                        if (Arg::isEarlyDef(role))
                            isKillCandidate = false;
                        else if (Arg::isLateDef(role) && !isSpill)
                            isKillCandidate = false;
                    });
                m_actions.appendVector(lateActions);

                if (isKillCandidate)
                    m_killCandidates.append(instIndex);

                if (isCoalescableMove(inst, coalesceSpillSlots)) {
                    m_moveSites.append({ instIndex, inst.args()[0].stackSlot()->index(), inst.args()[1].stackSlot()->index() });
                    if (inst.args()[0] == inst.args()[1])
                        m_sawUselessMove = true;
                }
            }

            data.actionEnd = m_actions.size();
            data.killEnd = m_killCandidates.size();
            data.moveEnd = m_moveSites.size();
        }
    }

    Code& code() const { return m_code; }
    unsigned numIndices() const { return m_code.stackSlots().size(); }
    bool sawUselessMove() const { return m_sawUselessMove; }

    std::span<const Action> actionsFor(BasicBlock* block) const
    {
        const BlockData& data = m_blockData[block->index()];
        return m_actions.subspan(data.actionStart, data.actionEnd - data.actionStart);
    }

    std::span<const uint32_t> killCandidatesFor(BasicBlock* block) const
    {
        const BlockData& data = m_blockData[block->index()];
        return m_killCandidates.subspan(data.killStart, data.killEnd - data.killStart);
    }

    std::span<const MoveSite> moveSitesFor(BasicBlock* block) const
    {
        const BlockData& data = m_blockData[block->index()];
        return m_moveSites.subspan(data.moveStart, data.moveEnd - data.moveStart);
    }

    // Boundary numbers restart at zero in every block, so `actions` has to be exactly one block's
    // actions. The actions of one boundary are contiguous, so a boundary is a subspan and finding the
    // next one going backwards is a scan over equal boundaries.
    static std::span<const Action> groupEndingAt(std::span<const Action> actions, size_t end)
    {
        ASSERT(end);
        uint32_t boundary = actions[end - 1].boundary;
        size_t start = end;
        while (start && actions[start - 1].boundary == boundary)
            --start;
        return actions.subspan(start, end - start);
    }

private:
    struct BlockData {
        uint32_t actionStart { 0 };
        uint32_t actionEnd { 0 };
        uint32_t killStart { 0 };
        uint32_t killEnd { 0 };
        uint32_t moveStart { 0 };
        uint32_t moveEnd { 0 };
    };

    Code& m_code;
    bool m_sawUselessMove { false };
    Vector<Action> m_actions;
    Vector<uint32_t> m_killCandidates;
    Vector<MoveSite> m_moveSites;
    Vector<BlockData> m_blockData;
};

// Boundaries with no mentions are simply absent, which is what keeps the fixpoint proportional to the
// mentions rather than to the instructions.
struct StackSlotLivenessAdapter {
    WTF_FORBID_HEAP_ALLOCATION;
public:
    typedef Air::CFG CFG;
    typedef StackSlot* Thing;
    using ActionGroup = std::span<const StackSlotActions::Action>;

    StackSlotLivenessAdapter(const StackSlotActions& actions)
        : actions(actions)
    {
    }

    void prepareToCompute() { }

    unsigned numIndices() const { return actions.numIndices(); }
    unsigned blockSize(BasicBlock* block) const { return block->size(); }
    static unsigned valueToIndex(StackSlot* slot) { return slot->index(); }
    StackSlot* indexToValue(unsigned index) const { return actions.code().stackSlots()[index]; }

    template<typename Func>
    void forEachActionGroupDescending(BasicBlock* block, const Func& func) const
    {
        auto blockActions = actions.actionsFor(block);
        size_t end = blockActions.size();
        while (end) {
            ActionGroup group = StackSlotActions::groupEndingAt(blockActions, end);
            func(group.front().boundary, group);
            end -= group.size();
        }
    }

    template<typename Func>
    static void forEachUseInGroup(ActionGroup group, const Func& func)
    {
        for (const auto& action : group) {
            if (action.kind == StackSlotActions::UseAction)
                func(action.slotIndex);
        }
    }

    template<typename Func>
    static void forEachDefInGroup(ActionGroup group, const Func& func)
    {
        for (const auto& action : group) {
            if (action.kind != StackSlotActions::UseAction)
                func(action.slotIndex);
        }
    }

    template<typename Func>
    void forEachUseAtTail(BasicBlock* block, const Func& func) const
    {
        auto blockActions = actions.actionsFor(block);
        if (blockActions.empty() || blockActions.back().boundary != block->size())
            return;
        forEachUseInGroup(StackSlotActions::groupEndingAt(blockActions, blockActions.size()), func);
    }

    const StackSlotActions& actions;
};

class StackSlotLiveness final : public WTF::Liveness<StackSlotLivenessAdapter> {
    WTF_FORBID_HEAP_ALLOCATION;
public:
    StackSlotLiveness(Code& code, const StackSlotActions& actions)
        : WTF::Liveness<StackSlotLivenessAdapter>(code.cfg(), actions)
    {
        CompilerTimingScope timingScope("Air"_s, "StackAllocator::liveness"_s);
        compute();
    }
};

class StackAllocatorBase {
protected:
    StackAllocatorBase(Code& code)
        : m_code(code)
        , m_remappedStackSlotIndices(code.stackSlots().size())
    {
        for (unsigned i = 0; i < m_remappedStackSlotIndices.size(); ++i)
            m_remappedStackSlotIndices[i] = i;
    }

    unsigned NODELETE remap(unsigned slotIndex) const
    {
        for (;;) {
            unsigned remappedSlotIndex = m_remappedStackSlotIndices[slotIndex];
            if (remappedSlotIndex == slotIndex)
                return slotIndex;
            slotIndex = remappedSlotIndex;
        }
    }

    StackSlot* NODELETE remapStackSlot(StackSlot* slot) const
    {
        return m_code.stackSlots()[remap(slot->index())];
    }

    bool NODELETE isRemappedSlotIndex(unsigned slotIndex) const
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
        , m_stats(code.proc().name())
    {
        m_interference.setMaxIndex(m_code.stackSlots().size());
    }

    void run(const Vector<StackSlot*>& assignedEscapedStackSlots)
    {
        bool coalesceSpillSlots = Options::coalesceSpillSlots();

        StackSlotActions actions(m_code, coalesceSpillSlots);
        StackSlotLiveness liveness(m_code, actions);
        buildInterferenceGraph(liveness);

        coalesceSlots(coalesceSpillSlots, actions.sawUselessMove());
        assignStackLocations(assignedEscapedStackSlots);

        updateFrameSizeBasedOnStackSlots(m_code);
        m_stats.frameSize = m_code.frameSize();
    }

private:
    void buildInterferenceGraph(StackSlotLiveness& liveness)
    {
        CompilerTimingScope timingScope("Air"_s, "StackAllocator::build"_s);

        const StackSlotActions& actions = liveness.actions;
        auto workset = liveness.makeWorkset();

        for (BasicBlock* block : m_code) {
            auto blockActions = actions.actionsFor(block);
            auto killCandidates = actions.killCandidatesFor(block);
            auto moveSites = actions.moveSitesFor(block);

            if (!blockActions.empty() || !killCandidates.empty() || !moveSites.empty()) {
                liveness.copyLiveAtTailInto(workset, block);

                size_t actionEnd = blockActions.size();
                size_t killIndex = killCandidates.size();
                size_t moveIndex = moveSites.size();

                for (;;) {
                    // Visit boundaries from the tail down, skipping any that mentions no stack slot
                    // and holds neither a kill candidate nor a coalescable move: those cannot change
                    // the live set or add an edge.
                    int64_t actionBoundary = actionEnd ? static_cast<int64_t>(blockActions[actionEnd - 1].boundary) : -1;
                    int64_t killBoundary = killIndex ? static_cast<int64_t>(killCandidates[killIndex - 1]) + 1 : -1;
                    int64_t moveBoundary = moveIndex ? static_cast<int64_t>(moveSites[moveIndex - 1].instIndex) + 1 : -1;
                    int64_t boundary = std::max({ actionBoundary, killBoundary, moveBoundary });
                    if (boundary < 0)
                        break;

                    std::span<const StackSlotActions::Action> group;
                    if (actionBoundary == boundary) {
                        group = StackSlotActions::groupEndingAt(blockActions, actionEnd);
                        actionEnd -= group.size();
                    }

                    // Completes the previous boundary's transfer. The uses at the tail boundary are
                    // already part of liveAtTail, so re-adding them is a harmless no-op.
                    for (const auto& action : group) {
                        if (action.kind == StackSlotActions::UseAction)
                            workset.add(action.slotIndex);
                    }

                    if (AirAllocateStackByGraphColoringInternal::verbose) {
                        dataLog("Boundary ", boundary, " of block ", block->index(), ", live:");
                        workset.forEachSetBit(
                            [&] (unsigned slotIndex) {
                                dataLog(" ", pointerDump(m_code.stackSlots()[slotIndex]));
                            });
                        if (boundary)
                            dataLog(", after: ", block->at(boundary - 1));
                        dataLogLn();
                    }

                    bool killedPreviousInst = false;
                    if (killBoundary == boundary) {
                        --killIndex;
                        uint32_t candidateInstIndex = killCandidates[killIndex];
                        // All late defs at this boundary belong to the candidate, so the store is dead
                        // exactly when none of them is live here.
                        bool isDeadStore = true;
                        for (const auto& action : group) {
                            if (action.kind == StackSlotActions::LateDefAction && workset.contains(action.slotIndex)) {
                                isDeadStore = false;
                                break;
                            }
                        }
                        if (isDeadStore) {
                            dataLogLnIf(AirAllocateStackByGraphColoringInternal::verbose, "Killing dead store: ", block->at(candidateInstIndex));
                            block->at(candidateInstIndex) = Inst();
                            killedPreviousInst = true;
                        }
                    }

                    // A coalescable move's destination does not interfere with its source, so it is
                    // handled separately and its late def is left out of the loop below. A killed
                    // store is gone, so it can no longer be coalesced.
                    bool previousInstIsCoalescableMove = false;
                    if (moveBoundary == boundary) {
                        --moveIndex;
                        if (!killedPreviousInst) {
                            const auto& site = moveSites[moveIndex];
                            previousInstIsCoalescableMove = true;
                            m_coalescableMoves.append(CoalescableMove(site.src, site.dst, block->frequency()));
                            workset.forEachSetBit(
                                [&] (unsigned otherSlotIndex) {
                                    if (otherSlotIndex != site.src)
                                        addEdge(site.dst, otherSlotIndex);
                                });
                        }
                    }

                    for (const auto& action : group) {
                        if (action.kind == StackSlotActions::UseAction || !action.isSpill)
                            continue;
                        // All late defs at this boundary belong to the previous instruction, so they
                        // are exactly what a kill or a coalescable move suppresses.
                        if (action.kind == StackSlotActions::LateDefAction && (killedPreviousInst || previousInstIsCoalescableMove))
                            continue;
                        uint32_t slotIndex = action.slotIndex;
                        workset.forEachSetBit(
                            [&] (unsigned otherSlotIndex) {
                                addEdge(slotIndex, otherSlotIndex);
                            });
                    }

                    for (const auto& action : group) {
                        if (action.kind != StackSlotActions::UseAction)
                            workset.remove(action.slotIndex);
                    }
                }

                ASSERT(!actionEnd && !killIndex && !moveIndex);
            }

            // Unconditional because fixObviousSpills, which runs immediately before this phase, leaves
            // the instructions it deletes in the block for a later pass to drop.
            block->insts().removeAllMatching(
                [&] (const Inst& inst) -> bool {
                    return !inst;
                });
        }

        reportInterferenceGraph();
    }

    void reportInterferenceGraph()
    {
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
    }

    void coalesceSlots(bool coalesceSpillSlots, bool sawUselessMove)
    {
        CompilerTimingScope timingScope("Air"_s, "StackAllocator::coalesce"_s);

        if (m_stats.collectingStats()) {
            m_stats.numBlocks = m_code.size();
            m_stats.numStackSlots = m_code.stackSlots().size();
            m_stats.stackSlotInterferenceSizeBytes = m_interference.memoryUse();
            m_stats.numStackSlotsCoalesceableMoves = m_coalescableMoves.size();
        }

        // Now try to coalesce some moves.
        if (m_coalescableMoves.isEmpty() && !sawUselessMove)
            return;

        std::ranges::sort(m_coalescableMoves, std::ranges::greater { }, &CoalescableMove::frequency);

        bool anyRemapHappened = false;
        for (const CoalescableMove& move : m_coalescableMoves) {
            IndexType slotToKill = remap(move.src);
            IndexType slotToKeep = remap(move.dst);
            if (slotToKill == slotToKeep)
                continue;
            if (m_interference.contains(slotToKill, slotToKeep))
                continue;

            m_stats.numStackSlotsCoalesced++;
            m_remappedStackSlotIndices[slotToKill] = slotToKeep;
            anyRemapHappened = true;

            for (IndexType interferingSlot : m_interference[slotToKill])
                addEdge(interferingSlot, slotToKeep);
            m_interference.mayClear(slotToKill);
        }

        if (!anyRemapHappened && !sawUselessMove)
            return;

        for (BasicBlock* block : m_code) {
            for (Inst& inst : *block) {
                for (Arg& arg : inst.args()) {
                    if (arg.isStack())
                        arg = Arg::stack(remapStackSlot(arg.stackSlot()), arg.offset());
                }
                if (isUselessMove(inst, coalesceSpillSlots))
                    inst = Inst();
            }
        }
    }

    void assignStackLocations(const Vector<StackSlot*>& assignedEscapedStackSlots)
    {
        CompilerTimingScope timingScope("Air"_s, "StackAllocator::assign"_s);

        // Now we assign stack locations. At its heart this algorithm is just first-fit. For each
        // StackSlot we just want to find the offsetFromFP that is least negative while ensuring no
        // overlap with other StackSlots that this overlaps with.
        Vector<StackSlot*> otherSlots;
        for (StackSlot* slot : m_code.stackSlots()) {
            if (isRemappedSlotIndex(slot->index()))
                continue;

            if (slot->offsetFromFP()) {
                // Already assigned an offset.
                continue;
            }

            otherSlots.shrink(0);
            otherSlots.appendVector(assignedEscapedStackSlots);
            for (unsigned otherSlotIndex : m_interference[slot->index()]) {
                if (isRemappedSlotIndex(otherSlotIndex))
                    continue;
                StackSlot* otherSlot = m_code.stackSlots()[otherSlotIndex];
                if (otherSlot->offsetFromFP())
                    otherSlots.append(otherSlot);
            }

            std::ranges::sort(otherSlots, std::ranges::greater { }, &StackSlot::offsetFromFP);
            assign(slot, otherSlots);
        }
    }

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

        friend bool operator==(const CoalescableMove&, const CoalescableMove&) = default;

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

    InterferenceGraph m_interference;
    Vector<CoalescableMove> m_coalescableMoves;
    AirStackAllocatorStats m_stats;
};

// Avoid computing the liveness information if there is no spill slot to allocate
bool doTrivialStackAllocation(Code& code)
{
    for (StackSlot* slot : code.stackSlots()) {
        if (slot->offsetFromFP())
            continue;
        return false;
    }
    updateFrameSizeBasedOnStackSlots(code);
    return true;
}

} // anonymous namespace

void allocateStackByGraphColoring(Code& code)
{
    PhaseScope phaseScope(code, "allocateStackByGraphColoring"_s);

    {
        CompilerTimingScope timingScope("Air"_s, "StackAllocator::calleeSaves"_s);
        handleCalleeSaves(code);
    }

    Vector<StackSlot*> assignedEscapedStackSlots = [&] {
        CompilerTimingScope timingScope("Air"_s, "StackAllocator::escapedSlots"_s);
        return allocateAndGetEscapedStackSlotsWithoutChangingFrameSize(code);
    }();

    if (!doTrivialStackAllocation(code)) {
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

    code.setStackIsAllocated(true);
}

} } } // namespace JSC::B3::Air

#endif // ENABLE(B3_JIT)
