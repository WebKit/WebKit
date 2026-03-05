/*
 * Copyright (C) 2024-2026 Apple Inc. All rights reserved.
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
#include "AirAllocateRegistersByGreedy.h"

#if ENABLE(B3_JIT)

#include "AirArgInlines.h"
#include "AirCode.h"
#include "AirFixSpillsAfterTerminals.h"
#include "AirPhaseInsertionSet.h"
#include "AirInstInlines.h"
#include "AirLiveness.h"
#include "AirPadInterference.h"
#include "AirPhaseScope.h"
#include "AirRegLiveness.h"
#include "AirRegisterAllocatorStats.h"
#include "AirTmpMap.h"
#include "AirTmpWidthInlines.h"
#include "AirUseCounts.h"
#include <wtf/GenerationalSet.h>
#include <wtf/IntervalSet.h>
#include <wtf/IterationStatus.h>
#include <wtf/ListDump.h>
#include <wtf/PriorityQueue.h>
#include <wtf/Range.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC { namespace B3 { namespace Air {

namespace Greedy {

// Experiments
static constexpr bool eagerGroupsExhaustiveSearch = false;

static constexpr float unspillableCost = std::numeric_limits<float>::infinity();
static constexpr float fastTmpSpillCost = std::numeric_limits<float>::max();
static constexpr float maxSpillableSpillCost = std::numeric_limits<float>::max();
static_assert(unspillableCost > fastTmpSpillCost);
static_assert(unspillableCost > maxSpillableSpillCost);

// Phase constants used for the PhaseInsertionSet. Ensures that the fixup and spill/fill instructions
// inserted in a particular gap ends up in the correct order.
static constexpr unsigned spillStore = 0;
static constexpr unsigned splitMoveTo = 1;
static constexpr unsigned splitMoveFrom = 2;
static constexpr unsigned spillLoad = 3;

static bool verbose() { return Options::airGreedyRegAllocVerbose(); }

// Terminology / core data-structures:
//
// Point: a position within the IR stream. There are four points associated with each
// instruction, see PointOffsets. The early point for an instruction occurs immediately
// before the instruction and the late occurs immediately following. Early use/defs
// for that instruction are modeled as occurring at the early point whereas late use/defs
// are modeled as occurring at the late point. The late point for an instruction and
// early point for the subsequent instruction are distinct in order to avoid false
// conflicts, e.g. when a late use is followed by an early def. Two "gap" points, Pre & Post,
// exist for each instruction to allow distinguishing e.g. a Tmp being live into a block from
// a Tmp becoming live due to an early def by the first instruction of the block
// (and sinilarly for block exits and late use).
//
// Interval: contiguous set of points, represented by a half open interval [begin, end).
//
// LiveRange: a set of intervals, usually used to model the liveness of a particular Tmp.
// In this implementation, Intervals of a LiveRange must be non-overlapping and
// non-contigous (if contigous, they should have been merged), and stored in sorted order.
//
// RegisterRange: a set of intervals, usually used to model assignments to a particular
// register at each Point. Each Interval of a RegisterRange is associated with a single
// Tmp, which may be a register Tmp (to represent fixed register lifetime and clobbers) or
// a Tmp that is currently assigned to this register. This structure models which Tmp occupies
// the register at each Point. Tmps and their associated Intervals can be assigned and
// evicted to/from the RegisterRange as the register allocation algorithm progresses.
//
// Algorithm:
//
// 1. Initialization:
//   a. Define where Points are located in the IR.
//   b. Run liveness analysis and build a LiveRange for each Tmp (including fixed registers).
//   c. Run analysis to determine the cost to spill each Tmp.
//   d. Build metadata related to Tmp coalescing. Eagerly group Tmps that can be coalesced.
// 2. Register allocation:
//   a. Process each Tmp in order of priority. Priority is mostly related to the "stage" of the
//      Tmp, whether a Tmp has a preferred register, and the size of a Tmp's LiveRange. The idea
//      is to fit the most constrained Tmps first, and let the smaller and less constrained ranges
//      fit into the remaining gaps.
//   b. Each Tmp is driven through a state machine (see Stage enum), where the further a Tmp
//      progresses, the more aggressive we try to fit it somewhere.
//     - First, simply try to find space where the LiveRange can fit in a RegisterRange.
//     - If that's not successful, we may evict LiveRanges in favor of a LiveRange with higher
//       spill cost. The evicted LiveRanges will requeued for further processing.
//     - If that's not successful, we may split up eagerly coalesced Tmps and process each
//       subgroup individually. We also try other forms of splitting LiveRanges, which will
//       produce new Tmps/LiveRanges, some of which may be assignable to registers.
//     - Finally, if all else fails, the Tmp is spilled and new Tmps for the spill/fill fixups
//       are queued for processing.
// 3. Finalization: fixup IR code is inserted to handle Tmps' split ranges and spills.
//
// Note that stack slots are allocated during a subsequent compiler phase.

using Point = uint32_t;
using Interval = WTF::Range<Point>;

enum PointOffsets {
    Pre = 0,
    Early = 1,
    // Inst lives between the Early and Late points.
    Late = 2,
    Post = 3,
    PointsPerInst
};

// Quickly filters out short ranges from live range splitting consideration.
static constexpr size_t splitMinRangeSize = 4 * PointOffsets::PointsPerInst;

static constexpr unsigned spillCostSizeBias = 25000 * PointOffsets::PointsPerInst;

class LiveRange {
public:
    LiveRange() = default;

    inline void validate()
    {
#if ASSERT_ENABLED
        size_t size = 0;
        Interval* prevInterval = nullptr;
        for (auto& interval : m_intervals) {
            ASSERT(interval.begin() < interval.end());
            ASSERT(!prevInterval || prevInterval->end() < interval.begin());
            size += interval.distance();
            prevInterval = &interval;
        }
        ASSERT(size == m_size);
#endif
    }

    // interval must come before all the intervals already in this LiveRange.
    void prepend(Interval interval)
    {
        ASSERT(interval);
        if (m_intervals.isEmpty() || interval.end() < m_intervals.first().begin())
            m_intervals.prepend(interval);
        else {
            ASSERT(interval.end() == m_intervals.first().begin());
            m_intervals.first() |= interval;
        }
        m_size += interval.distance();
        validate();
    }

    // interval must come after all the intervals already in this LiveRange.
    void append(Interval interval)
    {
        ASSERT(interval);
        if (m_intervals.isEmpty() || m_intervals.last().end() < interval.begin())
            m_intervals.append(interval);
        else {
            ASSERT(m_intervals.last().end() == interval.begin());
            m_intervals.last() |= interval;
        }
        m_size += interval.distance();
        validate();
    }

    const Deque<Interval>& intervals() const
    {
        return m_intervals;
    }

    size_t size() const
    {
        return m_size;
    }

    bool overlaps(LiveRange& other)
    {
        auto otherIter = other.intervals().begin();
        auto otherEnd = other.intervals().end();
        for (auto interval : intervals()) {
            while (otherIter != otherEnd && otherIter->end() <= interval.begin())
                ++otherIter; // otherIter was entirely before interval
            if (otherIter == otherEnd)
                return false;
            // Either otherIter overlaps interval or otherIter is entirely after interval.
            if (otherIter->begin() < interval.end())
                return true;
        }
        return false;
    }

    static LiveRange merge(const LiveRange& a, const LiveRange& b)
    {
        auto getEarliestAndAdvance = [](auto& aIter, const auto aEnd, auto& bIter, const auto bEnd) {
            auto postInc = [](auto& iter) {
                Interval interval = *iter;
                ++iter;
                return interval;
            };
            ASSERT(aIter != aEnd || bIter != bEnd);
            if (aIter == aEnd)
                return postInc(bIter);
            if (bIter == bEnd)
                return postInc(aIter);
            if (aIter->begin() < bIter->begin())
                return postInc(aIter);
            return postInc(bIter);
        };

        LiveRange result;
        auto aIter = a.intervals().begin();
        auto aEnd = a.intervals().end();
        auto bIter = b.intervals().begin();
        auto bEnd = b.intervals().end();

        Interval current = { };
        while (aIter != aEnd || bIter != bEnd) {
            Interval next = getEarliestAndAdvance(aIter, aEnd, bIter, bEnd);
            if (current.overlaps(next)) {
                current |= next;
                continue;
            }
            if (current)
                result.append(current);
            current = next;
        }
        if (current)
            result.append(current);
        result.validate();
        return result;
    }

    static LiveRange subtract(const LiveRange& a, const LiveRange& b)
    {
        LiveRange result;
        auto aIter = a.intervals().begin();
        auto bIter = b.intervals().begin();

        if (aIter == a.intervals().end())
            return result;
        Interval interval = *aIter;
        ++aIter;

        while (true) {
            // Skip over intervals in b that come before the current interval of a.
            while (bIter != b.intervals().end() && bIter->end() <= interval.begin())
                ++bIter;

            if (bIter != b.intervals().end() && bIter->overlaps(interval)) {
                // Overlap: Split the interval into 0, 1 or 2 intervals.
                if (interval.begin() < bIter->begin())
                    result.append({ interval.begin(), bIter->begin() });
                if (bIter->end() < interval.end()) {
                    // Process remaining portion of the interval.
                    interval = { bIter->end(), interval.end() };
                    continue;
                }
            } else {
                // No overlap: include entire interval in result.
                result.append(interval);
            }
            // Finished processing interval of a; move on to the next.
            if (aIter == a.intervals().end())
                break;
            interval = *aIter;
            ++aIter;
        }
        result.validate();
        return result;
    }

    void dump(PrintStream& out) const
    {
        WTF::CommaPrinter comma;
        out.print("{ ");
        for (auto& interval : intervals())
            out.print(comma, interval);
        out.print(" }[", m_size, "]");
    }

private:
    Deque<Interval> m_intervals;
    size_t m_size { 0 }; // Sum of the distances over m_intervals
};

enum class Stage : uint8_t {
    New,
    Unspillable,
    TryAllocate,
    TrySplit,
    Spill,
    MaxInQueue = Spill,
    Assigned,
    Coalesced,
    Spilled,
    Replaced,
    SplitGroup,
};

class TmpPriority {
private:
    // Priority is encoded into a 64-bit unsigned integer for fast comparison.
    static constexpr size_t stageBits = 3;
    static_assert(static_cast<uint64_t>(Stage::MaxInQueue) < (1ULL << stageBits));
    static constexpr size_t stageShift = 64 - stageBits;

    static constexpr size_t maybeCoalescableBits = 1;
    static constexpr size_t maybeCoalescableShift = stageShift - maybeCoalescableBits;

    static constexpr size_t isGlobalBits = 1;
    static constexpr size_t isGlobalShift = maybeCoalescableShift - isGlobalBits;

    static constexpr size_t rangeSizeBits = 39;
    static constexpr size_t rangeSizeShift = isGlobalShift - rangeSizeBits;

    static constexpr size_t tmpIndexBits = 20;
    static constexpr size_t tmpIndexShift = rangeSizeShift - tmpIndexBits;
    static_assert(!tmpIndexShift);

    inline void packPriority(uint64_t val, size_t numBits, size_t shift, bool reverse)
    {
        const uint64_t mask = (1ull << numBits) - 1;
        val &= mask;
        if (reverse)
            val = mask - val;
        m_priority |= val << shift;
    }

public:
    TmpPriority(Tmp tmp, Stage stage, size_t rangeSizeOrStart, bool maybeCoalescable, bool isGlobal)
        : m_tmp(tmp)
    {
        ASSERT(!tmp.isReg());
        ASSERT(stage <= Stage::MaxInQueue);
        // Earlier stages are higher priority.
        packPriority(static_cast<uint64_t>(stage), stageBits, stageShift, true);
        packPriority(maybeCoalescable, maybeCoalescableBits, maybeCoalescableShift, false);
        packPriority(isGlobal, isGlobalBits, isGlobalShift, false);
        // If range is global, then rangeSizeOrStart is size, and larger ranges are higher priority.
        // If range is local, then rangeSizeOrStart is start, and earlier ranges are higher priority.
        packPriority(rangeSizeOrStart, rangeSizeBits, rangeSizeShift, !isGlobal);
        // Make a strict total order for determinism.
        packPriority(tmp.tmpIndex(), tmpIndexBits, tmpIndexShift, true);
    }

    Tmp tmp() { return m_tmp; }

    Stage stage() const
    {
        const uint64_t mask = (1 << stageBits) - 1;
        uint64_t stageReversed = m_priority >> stageShift;
        stageReversed &= mask;
        return static_cast<Stage>(mask - stageReversed);
    }

    void dump(PrintStream& out) const
    {
        out.print("<", m_tmp, ", ", WTF::RawHex(m_priority), ">");
    }

    static bool isHigherPriority(const TmpPriority& left, const TmpPriority& right)
    {
        return left.m_priority > right.m_priority;
    }

private:
    Tmp m_tmp;
    uint64_t m_priority { 0 };
};

class RegisterRange {
public:
    // Diminishing returns on perf after 2 cache lines, but 3 is more space efficient than 2.
    // InnerNodes store arrays of Range<Point> & uintptr_t (16 bytes per order)
    // LeafNodes store arrays of Range<Point> & Tmp (12 bytes per order)
    // Both are multiples of 192, which is 3 cache lines.
    static constexpr unsigned cacheLinesPerNode = 3;
    using AllocatedIntervalSet = IntervalSet<Point, Tmp, cacheLinesPerNode>;

    RegisterRange() = default;

    struct AllocatedInterval {
        Interval interval;
        Tmp tmp;

        AllocatedInterval(const std::pair<Interval, Tmp>& pair)
            : interval(pair.first)
            , tmp(pair.second)
        { }

        bool operator<(const AllocatedInterval& other) const
        {
            return this->interval.end() < other.interval.end();
        }

        void dump(PrintStream& out) const
        {
            out.print("{ ", tmp, " ", interval, " }");
        }
    };

    void add(Tmp tmp, LiveRange& range)
    {
        ASSERT(!hasConflict(range, Width64)); // Can't add overlapping LiveRanges
        for (auto& interval : range.intervals()) {
            ASSERT(interval != Interval()); // Strict ordering requires no empty intervals.
            m_allocations.insert(interval, tmp);
        }
    }

    void addClobberHigh64(Reg reg, Point point)
    {
        ASSERT(reg.isFPR());
        m_allocationsHigh64.insert(Interval(point), Tmp(reg));
    }

    void evict(LiveRange& range)
    {
        for (auto& interval : range.intervals())
            m_allocations.erase(interval);
    }

    bool hasConflict(LiveRange& range, Width width)
    {
        for (auto interval : range.intervals()) {
            if (m_allocations.hasOverlap(interval))
                return true;
        }
        if (width <= Width64)
            return false;
        for (auto interval : range.intervals()) {
            if (m_allocationsHigh64.hasOverlap(interval))
                return true;
        }
        return false;
    }

    // func is called with each (Tmp, Interval) pair (i.e. AllocatedInterval) of this
    // RegisterRange that overlaps with the given LiveRange 'range'.
    //
    // func is allowed to modify this RegisterRange, e.g. by calling evict().
    // func must not modify 'range' for the duration of this forEachConflict invocation.
    template<typename Func>
    void forEachConflict(const LiveRange& range, Width width, const Func& func)
    {
        auto status = forEachConflictImpl(m_allocations, range, func);
        if (width > Width64) [[unlikely]] {
            if (status == IterationStatus::Continue)
                forEachConflictImpl(m_allocationsHigh64, range, func);
        }
    }

    bool isEmpty() const
    {
        return m_allocations.isEmpty() && m_allocationsHigh64.isEmpty();
    }

    void dump(PrintStream& out) const
    {
        auto dumpSet = [&out](const AllocatedIntervalSet& allocationSet) {
            CommaPrinter comma;
            out.print("[");
            for (auto alloc : allocationSet) {
                out.print(comma);
                out.print(AllocatedInterval(alloc));
            }
            out.print("]");
        };

        dumpSet(m_allocations);
        if (!m_allocationsHigh64.isEmpty()) {
            out.print(", ↑");
            dumpSet(m_allocationsHigh64);
        }
    }

private:
    template<typename Func>
    static IterationStatus forEachConflictImpl(AllocatedIntervalSet& allocatedSet, const LiveRange& range, const Func& func)
    {
        for (auto interval : range.intervals()) {
            while (true) {
                auto intervalAndTmp = allocatedSet.find(interval);
                if (!intervalAndTmp)
                    break;
                AllocatedInterval conflict = { *intervalAndTmp };
                if (func(conflict) == IterationStatus::Done)
                    return IterationStatus::Done;
                if (interval.end() <= conflict.interval.end())
                    break; // There can't exist other conflicts with interval
                // Search for remaining conflicts with 'interval'
                interval = { conflict.interval.end(), interval.end() };
            }
        }
        return IterationStatus::Continue;
    }

    AllocatedIntervalSet m_allocations;
    AllocatedIntervalSet m_allocationsHigh64; // Tracks clobbers to vector registers that preserve lower 64-bits
};

// Auxiliary register allocator data per Tmp.
struct TmpData {
    // When an unspillable or fastTmp is coalesced with another tmp, we don't want the spillCost of the
    // group to be unspillableCost or fastTmpCost, so this property is tracked independent of useDefCost.
    enum class Spillability : uint8_t {
        Spillable,
        FastTmp,
        Unspillable,
    };

    struct CoalescableWith {
        void dump(PrintStream& out) const
        {
            out.print("(", tmp, ", ", moveCost, ")");
        }

        Tmp tmp;
        float moveCost; // The frequency-adjusted number of moves between TmpData's tmp and CoalescableWith.tmp
    };

    void dump(PrintStream& out) const
    {
        out.print("{stage = ", stage, " liveRange = ", liveRange, ", preferredReg = ", preferredReg,
            ", coalescables = ", listDump(coalescables), ", parentGroup = ", parentGroup, ", subGroup0 = ", subGroup0, ", subGroup1 = ", subGroup1,
            ", useDefCost = ", useDefCost, ", spillability = ", spillability, ", assigned = ", assigned, ", spillSlot = ", pointerDump(spillSlot),
            ", splitMetadataIndex = ", splitMetadataIndex, "}");
    }

    bool isGroup()
    {
        ASSERT(!subGroup0 == !subGroup1);
        return !!subGroup0;
    }

    float spillCost()
    {
        switch (spillability) {
        case Spillability::Unspillable:
            return unspillableCost;
        case Spillability::FastTmp:
            return fastTmpSpillCost;
        case Spillability::Spillable:
            break;
        default:
            ASSERT_NOT_REACHED();
        }
        ASSERT(liveRange.size()); // 0-sized ranges shouldn't be allocated
        // Heuristic that primarily favors not spilling higher use/def frequency-adjusted counts and
        // secondarily favors smaller ranges. The range size is a crude proxy for degree of
        // interference, since the register allocator never directly computes that.
        // The spillCostSizeBias causes the range size penalty to be relatively insignificant
        // for smaller ranges but become significant for very larger ranges.
        float cost = useDefCost / (liveRange.size() + spillCostSizeBias);
        return std::min(cost, maxSpillableSpillCost);
    }

    void validate()
    {
        ASSERT(!(spillSlot && assigned));
        ASSERT(!!assigned == (stage == Stage::Assigned));
        ASSERT(liveRange.intervals().isEmpty() == !liveRange.size());
        ASSERT_IMPLIES(spillSlot, stage == Stage::Spilled || !parentGroup);
        ASSERT_IMPLIES(stage == Stage::Spilled, spillCost() != unspillableCost);
        ASSERT_IMPLIES(stage == Stage::Spilled, !isGroup()); // Should have been split
        ASSERT_IMPLIES(coalescables.size(), !isGroup()); // Only bottom-most should have coalescables
    }

    LiveRange liveRange;
    Vector<CoalescableWith> coalescables;
    StackSlot* spillSlot { nullptr };
    float useDefCost { 0.0f };
    Tmp parentGroup;
    Tmp subGroup0, subGroup1;
    uint32_t splitMetadataIndex : 31 { 0 };
    uint32_t hasColdUse : 1 { 0 };
    Stage stage { Stage::New };
    Spillability spillability { Spillability::Spillable };
    Reg preferredReg;
    Reg assigned;
};

class UseDefList {
public:
    using List = Vector<Point, 4>;

    void add(Point instPoint)
    {
        if (m_instPoints.isEmpty() || m_instPoints.last() != instPoint) {
            ASSERT(m_instPoints.isEmpty() || m_instPoints.last() < instPoint);
            m_instPoints.append(instPoint);
        }
    }

    const List& useDefs() { return m_instPoints; }

private:
    List m_instPoints;
};

// SplitMetadata tracks a Tmp that has been split into multiple Tmps in an effort
// to either allocate the original Tmp or the split Tmps, depending on the split type.
struct SplitMetadata {
    enum class Type : uint8_t {
        Invalid,
        AroundClobbers,
        IntraBlock,
    };

    struct Split {
        Tmp tmp;
        Point lastDefPoint;

        void dump(PrintStream& out) const
        {
            out.print(tmp);
            if (lastDefPoint)
                out.print(" defPoint=", lastDefPoint);
        }
    };

    SplitMetadata()
        : type(Type::Invalid) { }

    SplitMetadata(Type t, Tmp tmp)
        : type(t)
        , originalTmp(tmp) { }

    void dump(PrintStream& out) const
    {
        out.print(originalTmp, " : ", type, " { ", listDump(splits), " } ");
    }

    Type type;
    Tmp originalTmp;
    Vector<Split> splits;
};

class GreedyAllocator {
public:
    GreedyAllocator(Code& code)
        : m_code(code)
        , m_blockToHeadPoint(code.size())
        , m_tailPoints(code.size())
        , m_map(code)
        , m_useDefLists()
        , m_splitMetadata(1) // Sacrifice index 0.
        , m_regRanges(Reg::maxIndex() + 1)
        , m_insertionSets(code.size())
        , m_useCounts(m_code)
        , m_tmpWidth(m_code)
    {
    }

    void run()
    {
        m_stats[GP].numTmpsIn = m_code.numTmps(GP);
        m_stats[FP].numTmpsIn = m_code.numTmps(FP);

        // FIXME: reconsider use of padIntereference, https://bugs.webkit.org/show_bug.cgi?id=288122
        padInterference(m_code);
        buildRegisterSets();
        buildIndices();
        buildLiveRanges();
        initSpillCosts<GP>();
        initSpillCosts<FP>();
        coalesceWithPinnedRegisters();
        finalizeGroups<GP>();
        finalizeGroups<FP>();

        dataLogLnIf(verbose(), "State before greedy register allocation:\n", *this);

        allocateRegisters<GP>();
        allocateRegisters<FP>();

        insertFixupCode();

        validateAssignments<GP>();
        validateAssignments<FP>();

        assignRegisters();
        fixSpillsAfterTerminals(m_code);

        m_stats[GP].didSpill += m_didSpill[GP];
        m_stats[FP].didSpill += m_didSpill[FP];
        m_stats[GP].numTmpsOut = m_code.numTmps(GP);
        m_stats[FP].numTmpsOut = m_code.numTmps(FP);
    }

    void dump(PrintStream& out) const
    {
        out.println("usesSIMD=", m_code.usesSIMD());
        out.println("Block to Point:");
        for (BasicBlock* block : m_code)
            out.println("    BB", pointerDump(block), ": ", positionOfHead(block));
        out.println("RegRanges:");
        dumpRegRanges<GP>(out);
        dumpRegRanges<FP>(out);
        out.println("LiveRanges:");
        auto dumpRegTmpData = [&](Reg r) {
            const TmpData& tmpData = m_map[Tmp(r)];
            if (tmpData.liveRange.size())
                out.println("    ", r, ": ", m_map[Tmp(r)]);
        };
        for (Reg r : m_allowedRegistersInPriorityOrder[GP])
            dumpRegTmpData(r);
        m_code.pinnedRegisters().forEachReg([&](Reg r) {
            if (bankForReg(r) == GP)
                dumpRegTmpData(r);
        });
        for (Reg r : m_allowedRegistersInPriorityOrder[FP])
            dumpRegTmpData(r);
        m_code.pinnedRegisters().forEachReg([&](Reg r) {
            if (bankForReg(r) == FP)
                dumpRegTmpData(r);
        });
        m_code.forEachTmp([&](Tmp tmp) {
            out.println("    ", tmp, ": ", m_map[tmp], " useWidth=", m_tmpWidth.useWidth(tmp));
        });
        out.println("Splits:\n", listDump(m_splitMetadata, "\n"));
        out.println("Stats (GP):", m_stats[GP]);
        out.println("Stats (FP):", m_stats[FP]);
    }

private:
    template<Bank bank>
    void dumpRegRanges(PrintStream& out) const
    {
        for (Reg r : m_allowedRegistersInPriorityOrder[bank]) {
            if (!m_regRanges[r].isEmpty())
                out.println("    ", r, ": ", m_regRanges[r]);
        }
        m_code.pinnedRegisters().forEachReg([&](Reg r) {
            if (bankForReg(r) == bank)
                out.println("    ", r, ": ", m_regRanges[r]);
        });
    }

    bool shouldSpillEverything()
    {
        if (!Options::airGreedyRegAllocSpillsEverything())
            return false;

        // You're meant to hack this so that you selectively spill everything depending on reasons.
        // That's super useful for debugging.
        return true;
    }

    void buildRegisterSets()
    {
        forEachBank([&] (Bank bank) {
            m_allowedRegistersInPriorityOrder[bank] = m_code.regsInPriorityOrder(bank);
            for (Reg r : m_allowedRegistersInPriorityOrder[bank]) {
                m_allAllowedRegisters.add(r, IgnoreVectors);
                ASSERT(m_code.mutableRegs().contains(r, IgnoreVectors));
                ASSERT(!m_code.pinnedRegisters().contains(r, IgnoreVectors));
                ASSERT(!m_code.isPinned(r));
            }
        });
        ASSERT(m_allAllowedRegisters == m_code.mutableRegs().toScalarRegisterSet());
    }

    void buildIndices()
    {
        Point headPosition = 0;
        Point tailPosition = 0;
        for (size_t i = 0; i < m_code.size(); i++) {
            BasicBlock* block = m_code[i];
            if (!block) {
                m_tailPoints[i] = tailPosition;
                continue;
            }
            tailPosition = headPosition + block->size() * PointOffsets::PointsPerInst - 1;
            m_blockToHeadPoint[block] = headPosition;
            m_tailPoints[i] = tailPosition;
            headPosition += block->size() * PointOffsets::PointsPerInst;
        }
        m_stats[GP].numInsts += (tailPosition + 1) / PointOffsets::PointsPerInst;
    }

    BasicBlock* findBlockContainingPoint(Point point)
    {
        auto iter = std::lower_bound(m_tailPoints.begin(), m_tailPoints.end(), point);
        ASSERT(iter != m_tailPoints.end()); // Should ask only about legal instruction boundaries.
        size_t blockIndex = std::distance(m_tailPoints.begin(), iter);
        BasicBlock* block = m_code[blockIndex];
        ASSERT(positionOfHead(block) <= point && point <= positionOfTail(block));
        return block;
    }

    Point positionOfHead(BasicBlock* block) const
    {
        return m_blockToHeadPoint[block];
    }

    Point positionOfTail(BasicBlock* block)
    {
        return positionOfHead(block) + block->size() * PointOffsets::PointsPerInst - 1;
    }

    static size_t instIndex(Point positionOfHead, Point point)
    {
        return (point - positionOfHead) / PointOffsets::PointsPerInst;
    }

    static Point pointAtOffset(Point point, PointOffsets offset)
    {
        static_assert(!(PointOffsets::PointsPerInst & (PointOffsets::PointsPerInst - 1)));
        return (point & ~(PointOffsets::PointsPerInst - 1)) + offset;
    }

    static Point positionOfEarly(Point positionOfHead, unsigned instIndex)
    {
        ASSERT(!(positionOfHead % PointOffsets::PointsPerInst));
        return positionOfHead + instIndex * PointOffsets::PointsPerInst + PointOffsets::Early;
    }

    static Point positionOfLate(Point positionOfHead, unsigned instIndex)
    {
        ASSERT(!(positionOfHead % PointOffsets::PointsPerInst));
        return positionOfHead + instIndex * PointOffsets::PointsPerInst + PointOffsets::Late;
    }

    static Point positionOfEarly(Interval interval)
    {
        return pointAtOffset(interval.begin(), PointOffsets::Early);
    }

    static Interval earlyInterval(Point positionOfEarly)
    {
        ASSERT((positionOfEarly % PointOffsets::PointsPerInst) == PointOffsets::Early);
        return Interval(positionOfEarly);
    }

    static Interval lateInterval(Point positionOfEarly)
    {
        ASSERT((positionOfEarly % PointOffsets::PointsPerInst) == PointOffsets::Early);
        return Interval(positionOfEarly + (PointOffsets::Late - PointOffsets::Early));
    }

    static Interval earlyAndLateInterval(Point positionOfEarly)
    {
        return earlyInterval(positionOfEarly) | lateInterval(positionOfEarly);
    }

    static Interval intervalForTiming(Point positionOfEarly, Arg::Timing timing)
    {
        switch (timing) {
        case Arg::OnlyEarly:
            return earlyInterval(positionOfEarly);
        case Arg::OnlyLate:
            return lateInterval(positionOfEarly);
        case Arg::EarlyAndLate:
            return earlyAndLateInterval(positionOfEarly);
        }
        ASSERT_NOT_REACHED();
        return Interval();
    }

    static Interval intervalForSpill(Point positionOfEarly, Arg::Role role)
    {
        Arg::Timing timing = Arg::timing(role);
        switch (timing) {
        case Arg::OnlyEarly:
            if (Arg::isAnyDef(role))
                return earlyAndLateInterval(positionOfEarly); // We have a spill store after this insn.
            return earlyInterval(positionOfEarly);
        case Arg::OnlyLate:
            if (Arg::isAnyUse(role))
                return earlyAndLateInterval(positionOfEarly); // We had a spill load before this insn.
            return lateInterval(positionOfEarly);
        case Arg::EarlyAndLate:
            return earlyAndLateInterval(positionOfEarly);
        }
        ASSERT_NOT_REACHED();
        return Interval();
    }

    // Returns the root of the spill-group tree. All Tmps in the tree are known to not interfere and
    // will share the same spill slot.
    template<Bank bank>
    Tmp groupForSpill(Tmp tmp)
    {
        ASSERT(m_map.get<bank>(tmp).stage == Stage::Spilled);
        while (Tmp parent = m_map.get<bank>(tmp).parentGroup)
            tmp = parent;
        return tmp;
    }

    // Returns the root of the register-subgroup tree. All Tmps in this subtree are candidates for
    // coalescing into the same register assignment.
    template<Bank bank>
    Tmp groupForReg(Tmp tmp)
    {
        Tmp parent = m_map.get<bank>(tmp).parentGroup;
        while (parent && m_map[parent].stage != Stage::SplitGroup) {
            ASSERT(!m_map[tmp].assigned); // Only the root of the register-group should have an assignment
            tmp = parent;
            parent = m_map.get<bank>(tmp).parentGroup;
        }
        return tmp;
    }

    Tmp groupForReg(Tmp tmp)
    {
        ASSERT(tmp.isGP() || tmp.isFP());
        return tmp.isGP() ? groupForReg<GP>(tmp) : groupForReg<FP>(tmp);
    }

    template<Bank bank>
    Reg assignedReg(Tmp tmp)
    {
        return m_map.get<bank>(groupForReg<bank>(tmp)).assigned;
    }

    Reg assignedReg(Tmp tmp)
    {
        return m_map[groupForReg(tmp)].assigned;
    }

    // Returns the stack slot a Tmp should use if spilled. Otherwise, returns nullptr.
    template<Bank bank>
    StackSlot* spillSlot(Tmp tmp)
    {
        TmpData& tmpData = m_map.get<bank>(tmp);
        if (tmpData.stage == Stage::Spilled) {
            ASSERT(tmpData.spillSlot && tmpData.spillSlot == m_map.get<bank>(groupForSpill<bank>(tmp)).spillSlot);
            return tmpData.spillSlot;
        }
        return nullptr;
    }

    StackSlot* spillSlot(Tmp tmp)
    {
        ASSERT(tmp.isGP() || tmp.isFP());
        return tmp.isGP() ? spillSlot<GP>(tmp) : spillSlot<FP>(tmp);
    }

    float adjustedBlockFrequency(BasicBlock* block)
    {
        float freq = block->frequency();
        if (!m_fastBlocks.saw(block)) [[unlikely]]
            freq *= Options::rareBlockPenalty();
        return freq;
    }

    template<Bank bank>
    Width widthForConflicts(Tmp tmp)
    {
        if constexpr (bank == GP)
            return Width64;
        ASSERT(bank == FP);
        // For FP, the top 64-bits of vector registers are checked for conflicts only when
        // those top 64-bits of the tmp are used. The low 64-bits are always checked.
        return m_tmpWidth.useWidth(tmp);
    }

    // Debug code to verify that the results of register allocation and finalization fixup is valid.
    // That is, no two Tmps simultaneously alive share the same register (unless they were coalesced).
    template<Bank bank>
    void validateAssignments()
    {
        if (!Options::airValidateGreedRegAlloc())
            return;
        bool anyFailures = false;

        auto fail = [&](BasicBlock* block, Tmp a, Tmp b) {
            dataLogLn("AIR GREEDY REGISTER ALLOCATION VALIDATION FAILURE");
            dataLogLn("   In BB", *block);
            dataLogLn("     tmp = ", a, " : ", m_map[a]);
            dataLogLn("     tmp = ", b, " : ", m_map[b]);
            anyFailures = true;
        };

        auto checkConflicts = [&](BasicBlock* block, const typename TmpLiveness<bank>::LocalCalc& localCalc) {
            for (Tmp a : localCalc.live()) {
                Tmp aGrp = groupForReg<bank>(a);
                Reg aReg = assignedReg<bank>(a);
                if (!aReg)
                    continue;
                for (Tmp b : localCalc.live()) {
                    Tmp bGrp = groupForReg<bank>(b);
                    Reg bReg = assignedReg<bank>(b);
                    if (aGrp == bGrp) {
                        // Coalesced a & b so they better have the same register.
                        if (aReg != bReg)
                            fail(block, a, b);
                    } else {
                        // a & b interfere so b must either have been spilled or assigned a different register.
                        if (!bReg)
                            continue;
                        if (aReg == bReg) {
                            if (m_code.isPinned(aReg)) {
                                // It's okay if both Tmps were coalseced to the same pinned register.
                                TmpData& regData = m_map[Tmp(aReg)];
                                if (regData.coalescables.containsIf([a](auto& with) { return with.tmp == a; })
                                    && regData.coalescables.containsIf([b](auto& with) { return with.tmp == b; }))
                                    continue;
                            }
                            fail(block, a, b);
                        }
                    }
                }
            }
        };

        TmpLiveness<bank> liveness(m_code);
        for (BasicBlock* block : m_code) {
            typename TmpLiveness<bank>::LocalCalc localCalc(liveness, block);
            for (unsigned instIndex = block->size(); instIndex--;) {
                checkConflicts(block, localCalc);
                localCalc.execute(instIndex);
            }
            checkConflicts(block, localCalc);
        }
        if (anyFailures) {
            dataLogLn("IR:\n", m_code);
            dataLogLn("State:\n", *this);
            RELEASE_ASSERT_NOT_REACHED();
        }
    }

    void buildLiveRanges()
    {
        CompilerTimingScope timingScope("Air"_s, "GreedyRegAlloc::buildLiveRanges"_s);
        UnifiedTmpLiveness liveness(m_code);
        TmpMap<Point> activeEnds(m_code);
        TmpMap<Point> liveAtTailMarkers(m_code, std::numeric_limits<Point>::max());
#if ASSERT_ENABLED
        UnifiedTmpLiveness::LiveAtHead assertOnlyLiveAtHead = liveness.liveAtHead();
#endif
        // Find non-rare blocks.
        m_fastBlocks.push(m_code[0]);
        while (BasicBlock* block = m_fastBlocks.pop()) {
            for (FrequentedBlock& successor : block->successors()) {
                if (!successor.isRare())
                    m_fastBlocks.push(successor.block());
            }
        }

        // addMaybeCoalescable is used during the first pass to collect all potentially
        // coalescables pairs. i.e. pairs of Tmps 'a' and 'b' such that there exists a
        // 'Move a, b' instruction. These pairs will be pruned after liveness analysis
        // based on conflicting defs. We do this rather than simply requiring that the
        // LiveRanges of coalescable tmps do not overlap so that we can handle Tmp copies, e.g.:
        //
        //   Move a, b
        //   Use a
        //   Use b
        //
        // The LiveRanges of a and b overlap, but the move is still coalescable (unless there
        // is a non-coalescable-move def of 'a' or 'b' during the lifetime of the other).
        auto addMaybeCoalescable = [&](Tmp a, Tmp b, BasicBlock* block) {
            if (a == b)
                return;
            TmpData& tmpData = m_map[a];
            float freq = adjustedBlockFrequency(block);
            for (auto& with : tmpData.coalescables) {
                if (with.tmp == b) {
                    with.moveCost += freq;
                    return;
                }
            }
            tmpData.coalescables.append({ b, freq });
        };

        auto coalescableMoveSrc = [&](Inst& inst) {
            return mayBeCoalescable(inst) ? inst.args[0].tmp() : Tmp();
        };

        auto isLiveAt = [&](Tmp tmp, Point point) {
            if (activeEnds[tmp])
                return true;
            // Tmp may have had a dead def at point (e.g. clobber).
            auto& intervals = m_map[tmp].liveRange.intervals();
            if (intervals.isEmpty())
                return false;
            return intervals.first().contains(point);
        };

        auto assertPinnedRegsAreLive = [&]() {
#if ASSERT_ENABLED
            m_code.pinnedRegisters().forEachReg([&](Reg reg) {
                ASSERT(isLiveAt(Tmp(reg), 0));
            });
#endif
        };

        // Remove def from any coalescable pair of a live tmp. We now know from liveness analysis
        // that these pairs are not coalescable.
        auto pruneCoalescable = [&](Inst& inst, Tmp def, Point point) {
            TmpData& defData = m_map[def];
            if (!defData.coalescables.size())
                return;
            Tmp movSrc = coalescableMoveSrc(inst);
            dataLogLnIf(verbose(), "Checking affinity ", inst, " def=", def, " movSrc=", movSrc);
            defData.coalescables.removeAllMatching([&](TmpData::CoalescableWith& with) {
                ASSERT(with.tmp != def);
                if (with.tmp != movSrc && isLiveAt(with.tmp, point)) {
                    dataLogLnIf(verbose(), "Pruning affinity ", def, " ", with.tmp);
                    m_map[with.tmp].coalescables.removeAllMatching([def](TmpData::CoalescableWith& with) {
                        return with.tmp == def;
                    });
                    return true;
                }
                return false;
            });
        };

        auto markUse = [&](Tmp tmp, Point point) {
            Point& end = activeEnds[tmp];
            ASSERT(!end || point < end);
            if (!end)
                end = point + 1; // +1 since Interval end is not inclusive
        };
        auto markDef = [&](Tmp tmp, Point point)  {
            if (tmp.isReg() && m_code.isPinned(tmp.reg())) [[unlikely]]
                return; // Model pinned registers as never being killed
            Point end = activeEnds[tmp];
            if (!end) [[unlikely]]
                end = point + 1; // Dead def / clobber
            m_map[tmp].liveRange.prepend({ point, end });
            activeEnds[tmp] = 0;
        };

        // First pass: collect all the potential coalescable pairs of Tmps.
        for (BasicBlock* block : m_code) {
            for (Inst& inst : block->insts()) {
                if (mayBeCoalescable(inst)) {
                    ASSERT(inst.args.size() == 2);
                    if (inst.args[0].isReg() || inst.args[1].isReg()) {
                        unsigned regIdx = inst.args[0].isReg() ? 0 : 1;
                        Reg reg = inst.args[regIdx].reg();
                        Tmp other = inst.args[regIdx ^ 1].tmp();
                        if (other.isReg())
                            continue;
                        if (m_allAllowedRegisters.contains(reg, IgnoreVectors)) {
                            if (!m_map[other].preferredReg)
                                m_map[other].preferredReg = inst.args[regIdx].reg();
                            continue;
                        }
                        if (!m_code.isPinned(reg))
                            continue;
                        // Pinned registers fall-through
                    }
                    ASSERT(inst.args[0].isTmp() && inst.args[1].isTmp());
                    addMaybeCoalescable(inst.args[0].tmp(), inst.args[1].tmp(), block);
                    addMaybeCoalescable(inst.args[1].tmp(), inst.args[0].tmp(), block);
                }
            }
        }

        Point funcEndPoint = m_tailPoints[m_code.size() - 1];
        m_code.pinnedRegisters().forEachReg([&](Reg reg) {
            markUse(Tmp(reg), funcEndPoint); // Pinned registers are always live
        });

        // Second pass: Run liveness analysis and build the LiveRange for each Tmp. Also,
        // prune conflicts from the coalescables.
        BasicBlock* blockAfter = nullptr;
        Vector<Tmp, 8> earlyUses, earlyDefs, lateUses, lateDefs;
        Vector<Reg, 8> earlyClobbersHigh64, lateClobbersHigh64;
        for (size_t blockIndex = m_code.size(); blockIndex--;) {
            BasicBlock* block = m_code[blockIndex];
            if (!block)
                continue;

            Point positionOfHead = this->positionOfHead(block);
            Point positionOfTail = this->positionOfTail(block);
            if (verbose()) {
                dataLog("At BB", pointerDump(block), "\n");
                dataLog("  positionOfHead = ", positionOfHead, "\n");
                dataLog("  positionOfTail = ", positionOfTail, "\n");
            }

            for (Tmp tmp : liveness.liveAtTail(block)) {
                markUse(tmp, positionOfTail);
                liveAtTailMarkers[tmp] = positionOfTail;
            }
            if (blockAfter) {
                Point blockAfterPositionOfHead = this->positionOfHead(blockAfter);
                for (Tmp tmp : liveness.liveAtHead(blockAfter)) {
                    ASSERT(activeEnds[tmp]);
                    // If tmp was live at the head of the next block but not live at the
                    // tail of the current block, close the interval.
                    if (liveAtTailMarkers[tmp] > positionOfTail) {
                        if (activeEnds[tmp]) [[likely]]
                            markDef(tmp, blockAfterPositionOfHead);
                    }
                }
            }
            assertPinnedRegsAreLive();

            for (unsigned instIndex = block->size(); instIndex--;) {
                Inst& inst = block->at(instIndex);
                Point positionOfEarly = this->positionOfEarly(positionOfHead, instIndex);
                Point positionOfLate = this->positionOfLate(positionOfHead, instIndex);

                lateUses.shrink(0);
                lateDefs.shrink(0);
                lateClobbersHigh64.shrink(0);
                earlyUses.shrink(0);
                earlyDefs.shrink(0);
                earlyClobbersHigh64.shrink(0);
                inst.forEachTmp([&](Tmp& tmp, Arg::Role role, Bank, Width) {
                    if (Arg::isLateUse(role))
                        lateUses.append(tmp);
                    if (Arg::isLateDef(role))
                        lateDefs.append(tmp);
                    if (Arg::isEarlyUse(role))
                        earlyUses.append(tmp);
                    if (Arg::isEarlyDef(role))
                        earlyDefs.append(tmp);
                    if (Arg::isColdUse(role)) [[unlikely]]
                        m_map[tmp].hasColdUse = true;
                });
                if (inst.kind.opcode == Patch) [[unlikely]] {
                    inst.extraEarlyClobberedRegs().forEachWithWidthAndPreserved(
                        [&](Reg reg, Width, PreservedWidth preservedWidth) {
                            ASSERT(preservedWidth == PreservesNothing || preservedWidth == Preserves64);
                            if (preservedWidth == PreservesNothing)
                                earlyDefs.append(Tmp(reg));
                            else
                                earlyClobbersHigh64.append(reg);
                        });
                    inst.extraClobberedRegs().forEachWithWidthAndPreserved(
                        [&](Reg reg, Width, PreservedWidth preservedWidth) {
                            ASSERT(preservedWidth == PreservesNothing || preservedWidth == Preserves64);
                            if (preservedWidth == PreservesNothing)
                                lateDefs.append(Tmp(reg));
                            else
                                lateClobbersHigh64.append(reg);
                        });
                }

                for (Tmp tmp : lateUses)
                    markUse(tmp, positionOfLate);
                for (Tmp tmp : lateDefs) {
                    markDef(tmp, positionOfLate);
                    pruneCoalescable(inst, tmp, positionOfLate);
                }
                for (Reg reg : lateClobbersHigh64)
                    m_regRanges[reg].addClobberHigh64(reg, positionOfLate);

                for (Tmp tmp : earlyUses)
                    markUse(tmp, positionOfEarly);
                for (Tmp tmp : earlyDefs) {
                    markDef(tmp, positionOfEarly);
                    pruneCoalescable(inst, tmp, positionOfEarly);
                }
                for (Reg reg : earlyClobbersHigh64)
                    m_regRanges[reg].addClobberHigh64(reg, positionOfEarly);
            }
#if ASSERT_ENABLED
            m_code.forEachTmp([&](Tmp tmp) {
                ASSERT(!!activeEnds[tmp] == assertOnlyLiveAtHead.isLiveAtHead(block, tmp));
            });
#endif
            blockAfter = block;
        }
        if (blockAfter) {
            Point firstBlockPositionOfHead = this->positionOfHead(blockAfter);
            for (Tmp tmp : liveness.liveAtHead(blockAfter))
                markDef(tmp, firstBlockPositionOfHead);
        }
        assertPinnedRegsAreLive();
        // Pinned registers are never killed, so markDef never completes their live-range. Do it now.
        m_code.pinnedRegisters().forEachReg([&](Reg reg) {
            Tmp tmp = Tmp(reg);
            ASSERT(activeEnds[tmp] == funcEndPoint + 1 && !m_map[tmp].liveRange.size());
            m_map[tmp].liveRange.prepend({ 0, activeEnds[tmp] });
        });

#if ASSERT_ENABLED
        m_code.forEachTmp([&](Tmp tmp) {
            ASSERT(!activeEnds[tmp]);
        });
#endif
    }

    template<typename Func, size_t inlineCapacity>
    IterationStatus forEachTmpInGroup(Tmp grp, Vector<Tmp, inlineCapacity>& worklist, const Func& func)
    {
        ASSERT(worklist.isEmpty());
        worklist.append(grp);

        while (!worklist.isEmpty()) {
            Tmp tmp = worklist.takeLast();
            TmpData& data = m_map[tmp];

            if (data.isGroup()) {
                worklist.append(data.subGroup1);
                worklist.append(data.subGroup0);
            } else if (func(tmp) == IterationStatus::Done) {
                worklist.shrink(0);
                return IterationStatus::Done;
            }
        }
        return IterationStatus::Continue;
    }

    void coalesceWithPinnedRegisters()
    {
        // If a Tmp is in a pinned register's coalescables set, that means the
        // Tmp's defs are always moves from the pinned register. Since no other
        // Tmps can be assigned to the pinned register, might as well assign
        // the coalescable tmps to the pinned register upfront.
        // Doing this eagerly could be the wrong decision if these Tmps are
        // coalescable with other Tmps, but that doesn't seem to happen in practice.
        m_code.pinnedRegisters().forEachReg([&](Reg reg) {
            Tmp tmp = Tmp(reg);
            TmpData& data = m_map[tmp];
            for (auto& with : data.coalescables) {
                ASSERT(!with.tmp.isReg());
                coalesceWithPinned(with.tmp, m_map[with.tmp], reg);
                m_stats[tmp.bank()].numCoalescedPinned++;
            }
        });
    }

    template <Bank bank>
    void finalizeGroups()
    {
        CompilerTimingScope timingScope("Air"_s, "GreedyRegAlloc::finalizeGroups"_s);

        struct Move {
            Tmp tmp0, tmp1;
            float cost;

            void dump(PrintStream& out) const
            {
                out.print(tmp0, ", ", tmp1, " ", cost);
            }
        };
        Vector<Move> moves;
        Vector<Tmp, 8> worklist0, worklist1;

        m_code.forEachTmp<bank>([&](Tmp tmp) {
            ASSERT(!tmp.isReg());
            TmpData& data = m_map.get<bank>(tmp);
            if (data.stage != Stage::New) {
                ASSERT(assignedReg(tmp) && m_code.isPinned(assignedReg(tmp)));
                return; // Already coalesced with a pinned register
            }
            std::ranges::sort(data.coalescables, [this](const auto& a, const auto& b) {
                    if (a.moveCost != b.moveCost)
                        return a.moveCost > b.moveCost;
                    // Favor coalescing shorter live ranges.
                    auto aSize = m_map.get<bank>(a.tmp).liveRange.size();
                    auto bSize = m_map.get<bank>(b.tmp).liveRange.size();
                    if (aSize != bSize)
                        return aSize < bSize;
                    return a.tmp.tmpIndex(bank) < b.tmp.tmpIndex(bank);
            });
            for (auto& with : m_map[tmp].coalescables) {
                if (tmp.tmpIndex(bank) < with.tmp.tmpIndex(bank))
                    moves.append({ tmp, with.tmp, with.moveCost });
            }
        });

        std::ranges::sort(moves, [](auto& a, auto& b) {
                if (a.cost != b.cost)
                    return a.cost > b.cost;
                if (a.tmp0.tmpIndex(bank) != b.tmp1.tmpIndex(bank))
                    return a.tmp0.tmpIndex(bank) < a.tmp0.tmpIndex(bank);
                ASSERT(a.tmp1.tmpIndex(bank) != b.tmp1.tmpIndex(bank));
                return a.tmp1.tmpIndex(bank) < b.tmp1.tmpIndex(bank);
        });

        auto hasConflict = [this, &worklist0, &worklist1](Tmp group0, Tmp group1) {
            bool conflicts = false;
            forEachTmpInGroup(group0, worklist0, [&](Tmp tmp0) {
                ASSERT(!conflicts);
                TmpData& data0 = m_map.get<bank>(tmp0);
                ASSERT(!data0.subGroup0 && !data0.subGroup1);
                forEachTmpInGroup(group1, worklist1, [&](Tmp tmp1) {
                    ASSERT(!conflicts);
                    ASSERT(tmp0 != tmp1);
                    TmpData& data1 = m_map.get<bank>(tmp1);
                    if (!data0.coalescables.containsIf([tmp1](auto& with) { return with.tmp == tmp1; })
                        && data0.liveRange.overlaps(data1.liveRange)) {
                        conflicts = true;
                        return IterationStatus::Done;
                    }
                    return IterationStatus::Continue;
                });
                return conflicts ? IterationStatus::Done : IterationStatus::Continue;
            });
            return conflicts;
        };

        auto addSubGroup = [this](Tmp group, TmpData& groupData, Tmp& subGroupField, Tmp subGroup) {
            TmpData& subGroupData = m_map.get<bank>(subGroup);
            subGroupField = subGroup;
            subGroupData.parentGroup = group;
            subGroupData.stage = Stage::Coalesced;

            groupData.liveRange = LiveRange::merge(groupData.liveRange, subGroupData.liveRange);
            groupData.useDefCost += subGroupData.useDefCost;
            if (!groupData.preferredReg)
                groupData.preferredReg = subGroupData.preferredReg;

            Width defWidth, useWidth;
            defWidth = std::max(m_tmpWidth.defWidth(group), m_tmpWidth.defWidth(subGroup));
            useWidth = std::max(m_tmpWidth.useWidth(group), m_tmpWidth.useWidth(subGroup));
            m_tmpWidth.setWidths(group, useWidth, defWidth);
        };

        for (Move& move : moves) {
            dataLogLnIf(verbose(), "Processing move: ", move);
            Tmp group0 = groupForReg<bank>(move.tmp0);
            Tmp group1 = groupForReg<bank>(move.tmp1);
            if (group0 == group1) {
                dataLogLnIf(verbose(), "Already grouped transitively into ", group0);
                continue;
            }
            if (!hasConflict(group0, group1)) {
                Tmp newGrp = m_code.newTmp(bank);
                TmpData newGrpData;
                m_tmpWidth.setWidths(newGrp, Width8, Width8);

                addSubGroup(newGrp, newGrpData, newGrpData.subGroup0, group0);
                addSubGroup(newGrp, newGrpData, newGrpData.subGroup1, group1);
                newGrpData.validate();
                m_map.append(newGrp, newGrpData);
                dataLogLnIf(verbose(), "Created group ", newGrp, ": ", m_map.get<bank>(newGrp));
            }
        }
        if (verbose()) {
            m_code.forEachTmp<bank>([&](Tmp tmp) {
                TmpData& data = m_map.get<bank>(tmp);
                if (!data.parentGroup && data.isGroup()) {
                    dataLog("Group: ", tmp, " = { ");
                    CommaPrinter comma;
                    forEachTmpInGroup(tmp, worklist0, [&comma](Tmp member) {
                        dataLog(comma, member);
                        return IterationStatus::Continue;
                    });
                    dataLogLn(" }");
                }
            });
        }
    }

    template<Bank bank>
    void initSpillCosts()
    {
        CompilerTimingScope timingScope("Air"_s, "GreedyRegAlloc::initSpillCosts"_s);

        IndexSet<Tmp::AbsolutelyIndexed<bank>> cannotSpillInPlace;
        for (BasicBlock* block : m_code) {
            for (Inst& inst : block->insts()) {
                inst.forEachArg([&](Arg& arg, Arg::Role, Bank, Width) {
                    if (arg.isTmp() && (arg.tmp().bank() != bank || inst.admitsStack(arg)))
                        return;
                    // Arg cannot be spilled in-place.
                    arg.forEachTmpFast([&](Tmp& tmp) {
                        if (tmp.bank() != bank)
                            return;
                        if (!cannotSpillInPlace.contains(tmp)) {
                            dataLogLnIf(verbose(), tmp, " cannot be spilled in-place: ", inst);
                            cannotSpillInPlace.add(tmp);
                        }
                    });
                });
            }
        }

        for (Reg reg : m_allowedRegistersInPriorityOrder[bank])
            m_map.get<bank>(Tmp(reg)).spillability = TmpData::Spillability::Unspillable;

        m_code.forEachTmp<bank>([&](Tmp tmp) {
            ASSERT(tmp.bank() == bank);
            ASSERT(!tmp.isReg());
            TmpData& tmpData = m_map.get<bank>(tmp);
            ASSERT(tmpData.useDefCost == 0.0f);
            auto index = AbsoluteTmpMapper<bank>::absoluteIndex(tmp);
            float useDefCost = m_useCounts.numWarmUsesAndDefs<bank>(index);
            if (m_useCounts.isConstDef<bank>(index))
                useDefCost /= 2; // Can rematerialize rather than spill in many cases.
            tmpData.useDefCost = useDefCost;

            if (cannotSpillInPlace.contains(tmp)
                && tmpData.liveRange.intervals().size() == 1 && tmpData.liveRange.size() <= PointOffsets::PointsPerInst) {
                tmpData.spillability = TmpData::Spillability::Unspillable;
                m_stats[bank].numUnspillableTmps++;
            }
            tmpData.validate();
        });
        m_code.forEachFastTmp([&](Tmp tmp) {
            if (tmp.bank() != bank)
                return;
            m_map.get<bank>(tmp).spillability = TmpData::Spillability::FastTmp;
            m_stats[bank].numFastTmps++;
            dataLogLnIf(verbose(), "FastTmp: ", tmp);
        });
    }

    // newTmp creates and returns a new tmp that can hold the values of 'from'.
    // Note that all TmpData references invalidated since it may expand/realloc the TmpData map.
    Tmp newTmp(Tmp from, float useDefCost, Interval interval)
    {
        Tmp tmp = m_code.newTmp(from.bank());
        m_tmpWidth.setWidths(tmp, m_tmpWidth.useWidth(from), m_tmpWidth.defWidth(from));

        m_map.append(tmp, TmpData());
        TmpData& tmpData = m_map[tmp];
        tmpData.liveRange.prepend(interval);
        tmpData.useDefCost = useDefCost;
        tmpData.validate();
        return tmp;
    }

    Tmp addSpillTmpWithInterval(Tmp spilledTmp, Interval interval)
    {
        Tmp tmp = newTmp(spilledTmp, 0, interval);
        m_map[tmp].spillability = TmpData::Spillability::Unspillable;
        dataLogLnIf(verbose(), "New spill for ", spilledTmp, " tmp: ", tmp, ": ", m_map[tmp]);
        setStageAndEnqueue(tmp, m_map[tmp], Stage::Unspillable);
        m_stats[tmp.bank()].numSpillTmps++;
        return tmp;
    }

    void setStageAndEnqueue(Tmp tmp, TmpData& tmpData, Stage stage)
    {
        ASSERT(!tmp.isReg());
        ASSERT(&tmpData == &m_map[tmp]);
        ASSERT(m_map[tmp].liveRange.size()); // 0-size ranges don't need a register and spillCost() depends on size() != 0
        ASSERT(stage == Stage::Unspillable || stage == Stage::TryAllocate || stage == Stage::TrySplit || stage == Stage::Spill);
        ASSERT(groupForReg(tmp) == tmp); // Only the roots of register-groups should be enqueued
        tmpData.validate();

        tmpData.stage = stage;

        bool isLocal = false;
        size_t rangeSizeOrStart = tmpData.liveRange.size();
        if (tmpData.liveRange.intervals().size() == 1) {
            const Interval& interval = tmpData.liveRange.intervals().first();
            BasicBlock* block = findBlockContainingPoint(interval.begin());
            Point blockTail = positionOfTail(block);
            if (interval.end() - 1 <= blockTail) {
                isLocal = true;
                rangeSizeOrStart = interval.begin();
            }
        }
        m_queue.enqueue({ tmp, stage, rangeSizeOrStart, tmpData.preferredReg || tmpData.coalescables.size(), !isLocal });
        dataLogLnIf(verbose(), "Enqueued (", stage, ") ", tmp);
    }

    template <Bank bank>
    void allocateRegisters()
    {
        CompilerTimingScope timingScope("Air"_s, "GreedyRegAlloc::allocateRegisters"_s);

        for (Reg reg : m_allowedRegistersInPriorityOrder[bank])
            assign(Tmp(reg), m_map.get<bank>(Tmp(reg)), reg);

        m_code.forEachTmp<bank>([&](Tmp tmp) {
            ASSERT(!tmp.isReg());
            TmpData& tmpData = m_map.get<bank>(tmp);
            if (tmpData.parentGroup)
                return;
            if (tmpData.liveRange.intervals().isEmpty())
                return;
            if (tmpData.stage != Stage::New) {
                ASSERT(assignedReg(tmp) && m_code.isPinned(assignedReg(tmp)));
                return;
            }
            m_stats[bank].maxLiveRangeSize = std::max(m_stats[bank].maxLiveRangeSize, static_cast<unsigned>(tmpData.liveRange.size()));
            m_stats[bank].maxLiveRangeIntervals = std::max(m_stats[bank].maxLiveRangeIntervals, static_cast<unsigned>(tmpData.liveRange.intervals().size()));
            Stage initialStage = shouldSpillEverything() ? Stage::Spill : Stage::TryAllocate;
            setStageAndEnqueue(tmp, tmpData, initialStage);
        });

        do {
            while (!m_queue.isEmpty()) {
                auto entry = m_queue.dequeue();
                Tmp tmp = entry.tmp();
                ASSERT(tmp && tmp.bank() == bank);
                TmpData& tmpData = m_map.get<bank>(tmp);
                if (verbose()) {
                    StringPrintStream out;
                    out.println("Pop: ", entry, " tmp: ", tmpData);
                    dumpRegRanges<bank>(out);
                    dataLog(out.toCString());
                }
                switch (tmpData.stage) {
                case Stage::Unspillable:
                case Stage::TryAllocate:
                    doStageTryAllocate<bank>(tmp, tmpData);
                    continue;
                case Stage::TrySplit:
                    doStageTrySplit<bank>(tmp, tmpData);
                    continue;
                case Stage::Spill:
                    doStageSpill<bank>(tmp, tmpData);
                    continue;
                default:
                    dataLogLn("Invalid stage tmp = ", tmp, " tmpData = ", tmpData);
                    // Tmps in these stages should not have been enqueued.
                    RELEASE_ASSERT_NOT_REACHED();
                }
            }
            if (m_needsEmitSpillCode) {
                emitSpillCodeAndEnqueueNewTmps<bank>();
                m_needsEmitSpillCode = false;
                m_didSpill[bank] = true;
            }
            // Process the spill/fill tmps,
        } while (!m_queue.isEmpty());
    }

    template <Bank bank>
    void doStageTryAllocate(Tmp tmp, TmpData& tmpData)
    {
        ASSERT(tmpData.stage == Stage::TryAllocate || tmpData.stage == Stage::Unspillable);
        if (tryAllocate<bank>(tmp, tmpData))
            return;
        if (tryEvict<bank>(tmp, tmpData))
            return;
        RELEASE_ASSERT(tmpData.stage == Stage::TryAllocate); // Unspillable must have succeeded
        // If we couldn't allocate tmp, allow it to split next time.
        Stage nextStage = Stage::TrySplit;
        // If we already know splitting won't be profitable, skip it.
        if (!tmpData.isGroup() && tmpData.liveRange.size() < splitMinRangeSize)
            nextStage = Stage::Spill;
        setStageAndEnqueue(tmp, tmpData, nextStage);
    }

    template <Bank bank>
    void doStageTrySplit(Tmp tmp, TmpData& tmpData)
    {
        ASSERT(tmpData.stage == Stage::TrySplit);
        if (tryAllocate<bank>(tmp, tmpData))
            return;
        if (trySplit<bank>(tmp, tmpData))
            return;
        setStageAndEnqueue(tmp, tmpData, Stage::Spill);
    }

    template <Bank bank>
    void doStageSpill(Tmp tmp, TmpData& tmpData)
    {
        ASSERT(tmpData.stage == Stage::Spill);
        if (tryAllocate<bank>(tmp, tmpData))
            return;
        if (tryEvict<bank>(tmp, tmpData))
            return;
        ASSERT(queueContainsOnlySpills());
        spill(tmp, tmpData);
    }

    template <Bank bank>
    bool tryAllocate(Tmp tmp, TmpData& tmpData)
    {
        ASSERT(&m_map.get<bank>(tmp) == &tmpData);
        ASSERT(!assignedReg<bank>(tmp));
        ASSERT(groupForReg<bank>(tmp) == tmp);

        Width width = widthForConflicts<bank>(tmp);

        auto tryAllocateToReg = [&](Reg r) {
            LiveRange& liveRange = tmpData.liveRange;
            RegisterRange& regRanges = m_regRanges[r];
            if (!regRanges.hasConflict(liveRange, width)) {
                assign(tmp, tmpData, r);
                return true;
            }
            return false;
        };

        ScalarRegisterSet alreadyAttempted;
        if (eagerGroupsExhaustiveSearch) {
            Vector<Tmp, 8> worklist;
            // FIXME: this will check coalescables within the group, which is wasteful and common.
            // But without doing this, we won't try to coalescables between partially split groups.
            IterationStatus status = forEachTmpInGroup(tmp, worklist, [&](Tmp member) {
                for (auto& with : m_map.get<bank>(member).coalescables) {
                    Reg r = assignedReg<bank>(with.tmp);
                    if (r) {
                        if (tryAllocateToReg(r))
                            return IterationStatus::Done;
                        alreadyAttempted.add(r, IgnoreVectors);
                    }
                }
                return IterationStatus::Continue;
            });
            if (status == IterationStatus::Done) {
                ASSERT(tmpData.assigned);
                return true;
            }
        } else {
            for (auto& with : tmpData.coalescables) {
                Reg r = m_map.get<bank>(with.tmp).assigned;
                if (r) {
                    if (tryAllocateToReg(r))
                        return true;
                    alreadyAttempted.add(r, IgnoreVectors);
                }
            }
        }
        ASSERT(!assignedReg<bank>(tmp));

        if (tmpData.preferredReg) {
            if (tryAllocateToReg(tmpData.preferredReg))
                return true;
            alreadyAttempted.add(tmpData.preferredReg, IgnoreVectors);
        }
        for (Reg r : m_allowedRegistersInPriorityOrder[bank]) {
            if (alreadyAttempted.contains(r, IgnoreVectors))
                continue;
            if (tryAllocateToReg(r))
                return true;
        }
        return false;
    }

    template <Bank bank>
    bool tryEvict(Tmp tmp, TmpData& tmpData)
    {
        ASSERT(&m_map.get<bank>(tmp) == &tmpData);
        ASSERT(tmp.bank() == bank);

        auto failOutOfRegisters = [this](Tmp tmp) {
            insertFixupCode(); // So that the log shows the fixup code too
            StringPrintStream out;
            out.println("FATAL: no register for ", tmp);
            out.println("Unspillable Conflicts:");
            for (Reg r : m_allowedRegistersInPriorityOrder[bank]) {
                out.print("  ", r, ": ");
                m_regRanges[r].forEachConflict(m_map.get<bank>(tmp).liveRange, widthForConflicts<bank>(tmp),
                    [&](auto& conflict) -> IterationStatus {
                        if (m_map.get<bank>(conflict.tmp).spillCost() == unspillableCost)
                            out.print("{", conflict.tmp, ", ", conflict.interval, "}, ");
                        return IterationStatus::Continue;
                    });
                out.println("");
            }
            out.println("Code:", m_code);
            out.println("Register Allocator State:\n", pointerDump(this));
            dataLogLn(out.toCString());
            RELEASE_ASSERT_NOT_REACHED();
        };

        Reg bestEvictReg;
        float minSpillCost = tmpData.spillCost();
        m_visited.resize(m_code.numTmps(bank));
        LiveRange& liveRange = tmpData.liveRange;
        Width width = widthForConflicts<bank>(tmp);
        for (Reg r : m_allowedRegistersInPriorityOrder[bank]) {
            float conflictsSpillCost = 0.0f;
            m_visited.clear();
            m_regRanges[r].forEachConflict(liveRange, width,
                [&](auto& conflict) -> IterationStatus {
                    if (conflict.tmp.isReg()) {
                        // Conflicts with a fixed register use/def, cannot evict.
                        conflictsSpillCost = unspillableCost;
                        return IterationStatus::Done;
                    }
                    unsigned conflictTmpIndex = conflict.tmp.tmpIndex(bank);
                    if (m_visited.contains(conflictTmpIndex))
                        return IterationStatus::Continue;
                    m_visited.add(conflictTmpIndex);
                    auto cost = m_map.get<bank>(conflict.tmp).spillCost();
                    if (cost == unspillableCost) {
                        conflictsSpillCost = unspillableCost;
                        return IterationStatus::Done;
                    }
                    if (cost == std::numeric_limits<float>::max()
                        || conflictsSpillCost == std::numeric_limits<float>::max()) [[unlikely]]
                        conflictsSpillCost = std::numeric_limits<float>::max();
                    else
                        conflictsSpillCost += cost;
                    return conflictsSpillCost >= minSpillCost ? IterationStatus::Done : IterationStatus::Continue;
            });
            if (conflictsSpillCost < minSpillCost) {
                minSpillCost = conflictsSpillCost;
                bestEvictReg = r;
            }
        }
        if (minSpillCost >= tmpData.spillCost()) {
            // If 'tmp' was unspillable, we better have found at least one suitable register.
            if (tmpData.spillCost() == unspillableCost) [[unlikely]]
                failOutOfRegisters(tmp);
            return false;
        }
        // It's cheaper to spill all the already-assigned conflicting tmps, so evict them in favor of assigning 'tmp'.
        m_regRanges[bestEvictReg].forEachConflict(liveRange, widthForConflicts<bank>(tmp),
            [&](auto& conflict) -> IterationStatus {
                TmpData& conflictData = m_map.get<bank>(conflict.tmp);
                evict(conflict.tmp, conflictData, bestEvictReg);
                setStageAndEnqueue(conflict.tmp, conflictData, Stage::TryAllocate);
                return IterationStatus::Continue;
            });
        assign(tmp, tmpData, bestEvictReg);
        return true;
    }

    void assignImpl(Tmp tmp, TmpData& tmpData, Reg reg)
    {
        ASSERT(tmpData.stage != Stage::Assigned && tmpData.stage != Stage::Spilled);
        ASSERT(&m_map[tmp] == &tmpData);
        ASSERT(groupForReg(tmp) == tmp);
        tmpData.stage = Stage::Assigned;
        tmpData.assigned = reg;
        dataLogLnIf(verbose(), "Assigned ", tmp, " to ", reg);
        tmpData.validate();
    }

    void coalesceWithPinned(Tmp tmp, TmpData& tmpData, Reg reg)
    {
        ASSERT(m_code.isPinned(reg));
        assignImpl(tmp, tmpData, reg);
    }

    void assign(Tmp tmp, TmpData& tmpData, Reg reg)
    {
        ASSERT(m_allAllowedRegisters.contains(reg, IgnoreVectors));
        m_regRanges[reg].add(tmp, tmpData.liveRange);
        assignImpl(tmp, tmpData, reg);
    }

    void evict(Tmp tmp, TmpData& tmpData, Reg reg)
    {
        ASSERT(tmpData.stage == Stage::Assigned);
        ASSERT(tmpData.spillCost() != unspillableCost);
        ASSERT(tmpData.assigned == reg);
        ASSERT(groupForReg(tmp) == tmp);
        m_regRanges[reg].evict(tmpData.liveRange);
        tmpData.stage = Stage::New;
        tmpData.assigned = Reg();
        dataLogLnIf(verbose(), "Evicted ", tmp, " from ", reg);
        tmpData.validate();
    }

    template<Bank bank>
    bool trySplit(Tmp tmp, TmpData& tmpData)
    {
        ASSERT(tmpData.spillCost() != unspillableCost); // Should have evicted.
        if (trySplitGroup<bank>(tmp, tmpData))
            return true;
        if (trySplitAroundClobbers<bank>(tmp, tmpData))
            return true;
        return trySplitIntraBlock<bank>(tmp, tmpData);
    }

    template<Bank bank>
    bool trySplitGroup(Tmp tmp, TmpData& tmpData)
    {
        if (!tmpData.isGroup())
            return false;
        tmpData.stage = Stage::SplitGroup;
        setStageAndEnqueue(tmpData.subGroup0, m_map.get<bank>(tmpData.subGroup0), Stage::TryAllocate);
        setStageAndEnqueue(tmpData.subGroup1, m_map.get<bank>(tmpData.subGroup1), Stage::TryAllocate);
        dataLogLnIf(verbose(), "Split (group) ", tmp);
        tmpData.validate();
        return true;
    }

    template<Bank bank>
    bool trySplitAroundClobbers(Tmp tmp, TmpData& tmpData)
    {
        if (tmpData.splitMetadataIndex)
            return false; // Already split around clobbers
        if (tmpData.liveRange.size() < splitMinRangeSize)
            return false; // Not enough instructions to be worthwhile

        auto instUsesOrDefsTmp = [](Inst& inst, Tmp tmp) {
            bool result = false;
            inst.forEachTmpFast([&](Tmp useOrDef) {
                result |= useOrDef == tmp;
            });
            return result;
        };

        Reg bestSplitReg;
        float minSplitCost = unspillableCost;
        Width width = widthForConflicts<bank>(tmp);
        for (Reg r : m_allowedRegistersInPriorityOrder[bank]) {
            float splitCost = 0.0f;
            m_regRanges[r].forEachConflict(tmpData.liveRange, width,
                [&](auto& conflict) -> IterationStatus {
                    if (conflict.tmp.isReg() && conflict.interval.distance() == 1) {
                        // Block freq * rare block penalty
                        BasicBlock* block = findBlockContainingPoint(conflict.interval.begin());
                        unsigned instIndex = this->instIndex(positionOfHead(block), conflict.interval.begin());
                        Inst& inst = block->at(instIndex);
                        if (instUsesOrDefsTmp(inst, tmp)) {
                            // If the inst that clobbers regs also use/def the tmp trying to be split, then
                            // can't split the tmp around this clobber.
                            // FIXME: could allow uses, but then we'd have to make split tmp conflict with any
                            // spill tmps used by this instruction, so unclear if that's better.
                            splitCost = unspillableCost;
                            return IterationStatus::Done;
                        }
                        // Times 2 for 'MOV tmp, gapTmp' and 'MOV gapTmp, tmp'
                        splitCost += adjustedBlockFrequency(block) * 2;
                        return IterationStatus::Continue;
                    }
                    // Conflicts with another Tmp already assigned to this register so splitting around the clobbers won't help.
                    splitCost = unspillableCost;
                    return IterationStatus::Done;
                });
            if (splitCost < minSplitCost) {
                minSplitCost = splitCost;
                bestSplitReg = r;
            }
        }
        ASSERT(tmpData.spillCost() != unspillableCost); // Should have evicted.
        if (minSplitCost >= unspillableCost)
            return false; // Other conflicts exist, so splitting is not productive
        // Multiplier of 0 means split around clobbers at every opportunity. The higher the multiplier,
        // the less often the split will be applied (i.e. treats splitting as more costly).
        if (minSplitCost * Options::airGreedyRegAllocSplitMultiplier() >= tmpData.spillCost())
            return false; // Better to spill than to split

        LiveRange holeRange;
        m_regRanges[bestSplitReg].forEachConflict(tmpData.liveRange, width,
            [&](auto& conflict) -> IterationStatus {
                ASSERT(conflict.tmp.isReg() && conflict.interval.distance() == 1);
                // Punched hole should always include the instructions late interval so the
                // split tmp won't be modeled as conflicting with late defs.
                Interval hole = conflict.interval | lateInterval(positionOfEarly(conflict.interval));
                ASSERT(hole.distance() == 1 || hole.distance() == 2);
                holeRange.append(hole);
                return IterationStatus::Continue;
            });

        tmpData.liveRange = LiveRange::subtract(tmpData.liveRange, holeRange);
        tmpData.splitMetadataIndex = m_splitMetadata.size();
        setStageAndEnqueue(tmp, tmpData, Stage::TryAllocate);

        SplitMetadata metadata(SplitMetadata::Type::AroundClobbers, tmp);
        // Create tmps to carry the value across register clobbering instructions. These tmps
        // might spill or be assigned another register.
        for (Interval hole : holeRange.intervals()) {
            BasicBlock* block = findBlockContainingPoint(hole.begin());
            float freq = 2 * adjustedBlockFrequency(block);
            // padInterference() ensures this.
            // FIXME: reconsider that, see https://bugs.webkit.org/show_bug.cgi?id=288122
            ASSERT(hole.begin() > positionOfHead(block));
            // Model gapTmp interference with any other tmp split at this location by starting
            // the gapTmp's range one position before the hole. Otherwise, the same register
            // may be chosen for the gapTmp and another splitTmp, which wouldn't be valid
            // since there could be a cycle among the set of fixup Move instructions.
            // An alternative would be to use the Shuffle opcode (which can handle that
            // rotation of register assignments) but that would trigger an extra liveness
            // analysis (see lowerAfterRegAlloc()), and that's unlikely to be worth it.
            Interval gapInterval = hole | Interval(hole.begin() - 1);
            Tmp gapTmp = newTmp(tmp, freq, gapInterval);
            metadata.splits.append({ gapTmp, 0 });
            setStageAndEnqueue(gapTmp, m_map.get<bank>(gapTmp), Stage::TryAllocate);
        }
        dataLogLnIf(verbose(), "Split (clobbers): reg = ", bestSplitReg, " spillCost = ", m_map.get<bank>(tmp).spillCost(), " splitCost = ", minSplitCost, " split tmp = ", metadata);
        m_splitMetadata.append(WTF::move(metadata));
        m_stats[bank].numSplitAroundClobbers++;
        return true;
    }

    // Note that the use/def lists are computed only once and not kept up to date.
    // So after a Tmp is split or spilled that Tmp's use/def list may include instructions that now
    // reference the new split tmp or spill slot rather than the Tmp itself.
    void ensureUseDefLists()
    {
        if (m_hasUseDefLists)
            return;

        m_useDefLists.resize(m_code);

        for (BasicBlock* block : m_code) {
            Point instPoint = this->positionOfHead(block);
            for (Inst& inst : block->insts()) {
                inst.forEachArg([&](Arg& arg, Arg::Role, Bank, Width) {
                    arg.forEachTmpFast([&](Tmp& tmp) {
                        m_useDefLists[tmp].add(instPoint);
                    });
                });
                instPoint += PointOffsets::PointsPerInst;
            }
        }
        m_hasUseDefLists = true;
    }

    // forEachUseDefWithin calls `func` for each instruction in the given interval that
    // uses or defs the given tmp, up to the end of the basic block.
    // Returns the unprocessed portion of the interval (if interval spans multiple blocks).
    // `cursor` can be used to perform a "sort-merge join" when the caller is making queries over a sorted set of intervals for the same tmp
    template<typename Func>
    Interval forEachUseDefWithin(Tmp tmp, Interval interval, size_t& cursor, const Func& func)
    {
        auto& useDefs = m_useDefLists[tmp].useDefs();

        Point start = pointAtOffset(interval.begin(), PointOffsets::Pre);
        Point end = interval.end();

        ASSERT(useDefs.isEmpty() || !cursor || useDefs[cursor - 1] < start);

        size_t i = cursor;
        for (; i < useDefs.size(); i++) {
            if (start <= useDefs[i])
                break;
        }
        if (i == useDefs.size() || end <= useDefs[i]) {
            cursor = i;
            return { }; // No more use/defs in `interval`
        }

        BasicBlock* block = findBlockContainingPoint(useDefs[i]);
        Point positionOfHead = this->positionOfHead(block);
        Point positionOfTail = this->positionOfTail(block);
        Point last = std::min(positionOfTail, end - 1);

        do {
            func(useDefs[i], block->at(instIndex(positionOfHead, useDefs[i])));
            i++;
        } while (i < useDefs.size() && useDefs[i] <= last);
        cursor = i - 1; // i-1 since the next interval may include this final instruction
        return { last + 1, end }; // Return the unprocessed portion of interval
    }

    template<Bank bank>
    bool trySplitIntraBlock(Tmp tmp, TmpData& tmpData)
    {
        if (tmpData.splitMetadataIndex)
            return false;

        unsigned tmpIndex = AbsoluteTmpMapper<bank>::absoluteIndex(tmp);
        if (m_useCounts.isConstDef<bank>(tmpIndex))
            return false; // Constant will be rematerialized instead

        // Don't split an already intra-block tmp. Otherwise, we might recursively try to
        // split a cluster tmp that couldn't be allocated.
        BasicBlock* startBlock = findBlockContainingPoint(tmpData.liveRange.intervals().first().begin());
        Point last = tmpData.liveRange.intervals().last().end() - 1;
        if (last <= positionOfTail(startBlock))
            return false; // Tmp's live range is already block local

        ensureUseDefLists();

        SplitMetadata* metadata = nullptr;
        Vector<Tmp*, 8> tmpPtrs;
        size_t cursor = 0;

        size_t numIntervals = tmpData.liveRange.intervals().size();
        for (size_t i = 0; i < numIntervals; i++) {
            // Note: this loop calls newTmp() which invalidates the tmpData reference.
            Interval interval = *(m_map.get<bank>(tmp).liveRange.intervals().begin() + i);
            Interval remaining = interval;

            do {
                Point lastDefPoint = 0;
                Interval cluster = { };
                tmpPtrs.shrink(0);

                remaining = forEachUseDefWithin(tmp, remaining, cursor, [&](Point point, Inst& inst) {
                    inst.forEachTmp([&](Tmp& t, Arg::Role role, Bank, Width) {
                        if (t == tmp && !Arg::isColdUse(role)) {
                            Point early = point + PointOffsets::Early;
                            tmpPtrs.append(&t);
                            if (Arg::isAnyDef(role))
                                lastDefPoint = early; // Remember where the fixup store to spill is needed
                            cluster |= intervalForTiming(early, Arg::timing(role));
                        }
                    });
                });

                if (tmpPtrs.size() > 1) {
                    ASSERT(cluster);
                    if (!metadata) {
                        m_map.get<bank>(tmp).splitMetadataIndex = m_splitMetadata.size();
                        m_splitMetadata.constructAndAppend(SplitMetadata::Type::IntraBlock, tmp);
                        metadata = &m_splitMetadata.last();
                    }
                    Point pre = pointAtOffset(cluster.begin(), PointOffsets::Pre);
                    // If the Tmp is live into the cluster then cluster range is extended to model the fixup load from
                    // the Tmp's spill slot before the first use. This also allows insertSplitIntraBlockFixupCode()
                    // to quickly determine whether the fixup load is needed.
                    if (interval.contains(pre))
                        cluster |= Interval(pre);
                    // Likewise, if the cluster defs Tmp then a store to Tmp's spill slot will be needed after the final def point.
                    if (lastDefPoint)
                        cluster |= Interval(pointAtOffset(lastDefPoint, PointOffsets::Post));

                    BasicBlock* block = findBlockContainingPoint(cluster.begin());
                    Tmp clusterTmp = newTmp(tmp, tmpPtrs.size() * adjustedBlockFrequency(block), cluster);
                    m_stats[bank].numSplitIntraBlockClusterTmps++;
                    for (auto& ptr : tmpPtrs)
                        *ptr = clusterTmp; // Within this cluster, use clusterTmp rather than Tmp
                    metadata->splits.append({ clusterTmp, lastDefPoint });
                    setStageAndEnqueue(clusterTmp, m_map.get<bank>(clusterTmp), Stage::TryAllocate);
                }
            } while (remaining);
        }

        if (!metadata) {
            m_stats[bank].numSplitIntraBlockNoCluster++;
            return false;
        }

        // The original Tmp is spilled, but the cluster Tmps will hopefully carry the value in registers
        // within the cluster intervals.
        spill(tmp, m_map.get<bank>(tmp));
        m_stats[bank].numSplitIntraBlock++;
        dataLogLnIf(verbose(), "Split (intra-block): ", *metadata);
        return true;
    }

    static unsigned stackSlotMinimumWidth(Width width)
    {
        if (width <= Width32)
            return 4;
        if (width <= Width64)
            return 8;
        ASSERT(width == Width128);
        return 16;
    }

    void spill(Tmp tmp, TmpData& tmpData)
    {
        RELEASE_ASSERT(tmpData.spillCost() != unspillableCost);
        ASSERT(tmpData.assigned == Reg());
        ASSERT(!tmpData.isGroup()); // Should have been split
        ASSERT(groupForReg(tmp) == tmp);
        tmpData.stage = Stage::Spilled;

        m_stats[tmp.bank()].numSpilledTmps++;
        dataLogLnIf(verbose(), "Spilled ", tmp);

        if (tmpData.splitMetadataIndex) {
            auto& metadata = m_splitMetadata[tmpData.splitMetadataIndex];
            if (metadata.type == SplitMetadata::Type::AroundClobbers) {
                // Splitting didn't prevent originalTmp from spilling after all, so no point assigning
                // registers or stack slots to the gap tmps for this split.
                dataLogLnIf(verbose(), "   evicting tmps created during split");
                ASSERT(metadata.originalTmp == tmp);
                for (auto& split : metadata.splits) {
                    Tmp gapTmp = split.tmp;
                    Reg reg = m_map[gapTmp].assigned;
                    if (reg)
                        evict(gapTmp, m_map[gapTmp], reg);
                    m_map[gapTmp].stage = Stage::Replaced;
                }
            }
        }
        // Batch the generation of spill/fill tmps so that we can limit traversals of the code while
        // not tracking each tmp's use/defs explicitly.
        m_needsEmitSpillCode = true;
        tmpData.validate();
    }

    bool queueContainsOnlySpills()
    {
        for (auto& elem : m_queue) {
            if (elem.stage() != Stage::Spill)
                return false;
        }
        return true;
    }

    Opcode moveOpcode(Tmp tmp)
    {
        Opcode move = Oops;
        Width width = m_tmpWidth.requiredWidth(tmp);
        switch (stackSlotMinimumWidth(width)) {
        case 4:
            move = tmp.bank() == GP ? Move32 : MoveFloat;
            break;
        case 8:
            move = tmp.bank() == GP ? Move : MoveDouble;
            break;
        case 16:
            ASSERT(tmp.bank() == FP);
            move = MoveVector;
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
        return move;
    }

    template<Bank bank>
    StackSlot* ensureGroupSpillSlot(Tmp tmp)
    {
        Tmp group = groupForSpill<bank>(tmp);
        TmpData& groupData = m_map.get<bank>(group);

        if (!groupData.spillSlot)
            groupData.spillSlot = m_code.addStackSlot(stackSlotMinimumWidth(m_tmpWidth.requiredWidth(group)), StackSlotKind::Spill);
        return groupData.spillSlot;
    }

    template <Bank bank>
    void emitSpillCodeAndEnqueueNewTmps()
    {
        m_code.forEachTmp<bank>([&](Tmp tmp) {
            TmpData& tmpData = m_map.get<bank>(tmp);
            if (tmpData.stage == Stage::Spilled && !tmpData.spillSlot) {
                tmpData.spillSlot = ensureGroupSpillSlot<bank>(tmp);
                ASSERT(spillSlot<bank>(tmp));
                m_stats[bank].numSpillStackSlots++;
            }
            if (tmpData.splitMetadataIndex) {
                auto& metadata = m_splitMetadata[tmpData.splitMetadataIndex];
                if (metadata.type == SplitMetadata::Type::IntraBlock) {
                    // Redirect spilled intra-block cluster tmps to the spill slot of the original tmp.
                    ASSERT(tmpData.stage == Stage::Spilled && spillSlot<bank>(tmp));
                    for (auto& split : metadata.splits) {
                        Tmp clusterTmp = split.tmp;
                        TmpData& clusterData = m_map.get<bank>(clusterTmp);
                        if (clusterData.stage == Stage::Spilled) {
                            if (!clusterData.spillSlot)
                                clusterData.spillSlot = spillSlot<bank>(tmp);
                            ASSERT(spillSlot<bank>(clusterTmp) == spillSlot<bank>(tmp));
                        }
                    }
                }
            }
        });
        for (BasicBlock* block : m_code) {
            Point positionOfHead = this->positionOfHead(block);
            for (unsigned instIndex = 0; instIndex < block->size(); ++instIndex) {
                Inst& inst = block->at(instIndex);
                unsigned indexOfEarly = positionOfEarly(positionOfHead, instIndex);

                bool useMove32IfDidSpill = false;
                bool didSpill = false;
                bool needScratch = false;
                Tmp scratchForTmp;

                if (bank == GP && inst.kind.opcode == Move) {
                    Arg& srcArg = inst.args[0];
                    Arg& dstArg = inst.args[1];
                    if (dstArg.isTmp() && spillSlot(dstArg.tmp())) {
                        // Storing to spill. If storing to a 4-byte slot, use store32, otherwise storePtr.
                        useMove32IfDidSpill = spillSlot(dstArg.tmp())->byteSize() == 4;
                    } else if (srcArg.isTmp() && spillSlot(srcArg.tmp())) {
                        // Loading from a spill slot (but not storing to a spill slot). If loading from a
                        // 4-byte slot, then use load32, otherwise loadPtr.
                        useMove32IfDidSpill = spillSlot(srcArg.tmp())->byteSize() == 4;
                    }
                }

                bool maybeCoalescable = this->mayBeCoalescable(inst);
                // Try to replace the register use by memory use when possible.
                inst.forEachArg(
                    [&] (Arg& arg, Arg::Role role, Bank argBank, Width width) {
                        if (!arg.isTmp())
                            return;
                        if (argBank != bank)
                            return;
                        StackSlot* spilled = spillSlot<bank>(arg.tmp());
                        if (!spilled)
                            return;
                        ASSERT(!arg.isReg());
                        bool needScratchIfSpilledInPlace = false;
                        if (!inst.admitsStack(arg)) {
                            switch (inst.kind.opcode) {
                            case Move:
                            case Move32:
                            case MoveDouble:
                            case MoveFloat:
                            case MoveVector: {
                                unsigned argIndex = &arg - &inst.args[0];
                                unsigned otherArgIndex = argIndex ^ 1;
                                Arg otherArg = inst.args[otherArgIndex];
                                if (inst.args.size() == 2
                                    && otherArg.isStack()
                                    && otherArg.stackSlot()->isSpill()) {
                                    needScratchIfSpilledInPlace = true;
                                    break;
                                }
                                return;
                            }
                            default:
                                return;
                            }
                        }
                        // If the Tmp holds a constant then we want to rematerialize its
                        // value rather than loading it from the stack.
                        unsigned tmpIndex = AbsoluteTmpMapper<bank>::absoluteIndex(arg.tmp());
                        ASSERT_IMPLIES(Arg::isColdUse(role), m_map.get<bank>(arg.tmp()).hasColdUse);
                        if (!Arg::isColdUse(role) && m_useCounts.isConstDef<bank>(tmpIndex)) {
                            // We do not handle FP here as there is almost zero instructions which can take FPImm except for Move.
                            if constexpr (bank == GP) {
                                int64_t value = m_useCounts.constant<bank>(tmpIndex);
                                Arg oldArg = arg;
                                Arg imm;
                                if (Arg::isValidImmForm(value))
                                    imm = Arg::imm(value);
                                else
                                    imm = Arg::bigImm(value);
                                ASSERT(inst.isValidForm());
                                arg = imm;
                                if (inst.isValidForm()) {
                                    m_stats[bank].numRematerializeConst++;
                                    dataLogLnIf(verbose(), "Rematerialized (direct imm), BB", *block, " arg=", oldArg, ", inst=", inst);
                                    return;
                                }
                                // Couldn't insert the immediate into the instruction directly, so undo.
                                arg = oldArg;
                            }
                            // We can still rematerialize it into a register. In order for that optimization to kick in,
                            // we need to avoid placing the Tmp's stack address into the instruction.
                            return;
                        }

                        Arg spilledArg = Arg::stack(spilled);
                        if (Arg::isZDef(role) && bytesForWidth(width) < spilled->byteSize()) {
                            // ZDef32 to 64 slot would require two 32-bit accesses (second one for zero extend), so
                            // usually it will be better to ZDef into a register and then storePtr the register.
                            // Unless, this move can have its live range coalesced.
                            ASSERT_IMPLIES(maybeCoalescable, &arg == &inst.args[1]);
                            if (!maybeCoalescable || spilledArg != inst.args[0]) {
                                m_stats[bank].numInPlaceSpillGiveUpSpillWidth++;
                                return;
                            }
                        }
                        spilled->ensureSize(useMove32IfDidSpill ? 4 : bytesForWidth(width));
                        didSpill = true;
                        if (needScratchIfSpilledInPlace) {
                            needScratch = true;
                            scratchForTmp = arg.tmp();
                        }
                        arg = spilledArg;
                        m_stats[bank].numInPlaceSpill++;
                    });

                if (didSpill && useMove32IfDidSpill)
                    inst.kind.opcode = Move32;

                if (needScratch) {
                    ASSERT(scratchForTmp != Tmp());
                    // This has become a Move spillN, spillM. If N==M, we can remove this instruction. Otherwise,
                    // a scratch register is needed in order to execute the move between spill slots.
                    if (maybeCoalescable && inst.args[0] == inst.args[1]) {
                        m_stats[bank].numCoalescedStackSlotMoves++;
                        inst = Inst(); // Will be removed during assignRegisters final pass
                        continue;
                    }
                    Tmp tmp = addSpillTmpWithInterval(scratchForTmp, intervalForSpill(indexOfEarly, Arg::Scratch));
                    inst.args.append(tmp);
                    RELEASE_ASSERT(inst.args.size() == 3);
                    m_stats[bank].numMoveSpillSpillInsts++;
                    ASSERT(inst.isValidForm());
                    // WTF::Liveness and Air::LivenessAdapter do not handle a late-def/use followed by early-def
                    // correctly. While this register allocator does handle it correctly (since it models distinct
                    // late and early points between instructions (i.e. intervalForSpill() won't overlap for different
                    // scratch Tmps)), insert a Nop so that subsequent liveness analysis correctly compute lifetime interference
                    // when there are back-to-back Move spill-spill-scratch instructions (scratch is early-def, late-use).
                    // See https://bugs.webkit.org/show_bug.cgi?id=163548#c2 for more info.
                    // FIXME: reconsider this, https://bugs.webkit.org/show_bug.cgi?id=288122
                    m_insertionSets[block].insert(instIndex, spillLoad, Nop, inst.origin);
                    continue;
                }

                bool doKillInst = false;
                // For every other case, add Load/Store as needed.
                inst.forEachTmp([&] (Tmp& tmp, Arg::Role role, Bank argBank, Width) {
                    if (tmp.isReg() || argBank != bank)
                        return;
                    StackSlot* spilled = spillSlot<bank>(tmp);
                    if (!spilled)
                        return;

                    Opcode move = moveOpcode(tmp);
                    auto originalTmp = tmp;
                    auto tmpIndex = AbsoluteTmpMapper<bank>::absoluteIndex(tmp);

                    bool canRematerializeConstant = m_useCounts.isConstDef<bank>(tmpIndex);
                    ASSERT_IMPLIES(canRematerializeConstant, !(Arg::isAnyUse(role) && Arg::isAnyDef(role)));
                    ASSERT_IMPLIES(canRematerializeConstant, role != Arg::Scratch);

                    bool canKillDef = canRematerializeConstant && !m_map.get<bank>(originalTmp).hasColdUse;

                    if (Arg::isAnyUse(role) || (!canKillDef && Arg::isAnyDef(role)))
                        tmp = addSpillTmpWithInterval(tmp, intervalForSpill(indexOfEarly, role));

                    if (role == Arg::Scratch)
                        return;

                    Arg arg = Arg::stack(spilled);
                    if (Arg::isAnyUse(role)) {
                        auto tryRematerialize = [&]() {
                            if (!canRematerializeConstant)
                                return false;

                            if constexpr (bank == GP) {
                                int64_t value = m_useCounts.constant<bank>(tmpIndex);
                                if (Arg::isValidImmForm(value) && isValidForm(Move, Arg::Imm, Arg::Tmp)) {
                                    m_insertionSets[block].insert(instIndex, spillLoad, Move, inst.origin, Arg::imm(value), tmp);
                                    m_stats[bank].numRematerializeConst++;
                                    dataLogLnIf(verbose(), "Rematerialized (imm) BB", *block, " ", originalTmp, ": ", tmp, " <- ", WTF::RawHex(value));
                                    return true;
                                }
                                RELEASE_ASSERT(isValidForm(Move, Arg::BigImm, Arg::Tmp));
                                m_insertionSets[block].insert(instIndex, spillLoad, Move, inst.origin, Arg::bigImm(value), tmp);
                                m_stats[bank].numRematerializeConst++;
                                dataLogLnIf(verbose(), "Rematerialized (bigImm) BB", *block, " ", originalTmp, ": ", tmp, " <- ", WTF::RawHex(value));
                                return true;
                            } else {
                                v128_t constant = m_useCounts.constant<bank>(tmpIndex);
                                Width constWidth = m_useCounts.constantWidth<bank>(tmpIndex);
                                Arg imm;
                                Opcode constMove = Oops;
                                switch (constWidth) {
                                case Width32:
                                    if (Arg::isValidFPImm32Form(constant.u64x2[0]))
                                        imm = Arg::fpImm32(constant.u64x2[0]);
                                    constMove = MoveFloat;
                                    break;
                                case Width64:
                                    if (Arg::isValidFPImm64Form(constant.u64x2[0]))
                                        imm = Arg::fpImm64(constant.u64x2[0]);
                                    constMove = MoveDouble;
                                    break;
                                case Width128:
                                    if (Arg::isValidFPImm128Form(constant))
                                        imm = Arg::fpImm128(constant);
                                    constMove = MoveVector;
                                    break;
                                default:
                                    RELEASE_ASSERT_NOT_REACHED();
                                }

                                if (imm && isValidForm(constMove, imm.kind(), Arg::Tmp)) {
                                    m_insertionSets[block].insert(instIndex, spillLoad, constMove, inst.origin, imm, tmp);
                                    m_stats[bank].numRematerializeConst++;
                                    dataLogLnIf(verbose(), "Rematerialized (FP) BB", *block, " ", originalTmp, ": ", tmp);
                                    return true;
                                }

                                return false;
                            }
                        };

                        if (!tryRematerialize()) {
                            m_insertionSets[block].insert(instIndex, spillLoad, move, inst.origin, arg, tmp);
                            m_stats[bank].numLoadSpill++;
                        }
                    }
                    if (Arg::isAnyDef(role)) {
                        if (canKillDef) {
                            ASSERT(inst.kind.opcode == Move || inst.kind.opcode == Move32
                                || inst.kind.opcode == MoveFloat || inst.kind.opcode == MoveDouble
                                || inst.kind.opcode == MoveVector);
                            ASSERT(inst.args[0].isSomeImm() && inst.args[1] == originalTmp);
                            doKillInst = true;
                            dataLogLnIf(verbose(), "Rematerialized BB", *block, " removing def inst: ", inst);
                        } else {
                            ASSERT(!doKillInst);
                            m_insertionSets[block].insert(instIndex + 1, spillStore, move, inst.origin, tmp, arg);
                            m_stats[bank].numStoreSpill++;
                        }
                    }
                });
                ASSERT(inst.isValidForm());
                if (doKillInst)
                    inst = Inst(); // Will be removed by assignRegisters()
            }
        }
    }

    void insertFixupCode()
    {
        for (auto& metadata : m_splitMetadata) {
            switch (metadata.type) {
            case SplitMetadata::Type::Invalid:
                break;
            case SplitMetadata::Type::AroundClobbers:
                insertSplitAroundClobbersFixupCode(metadata);
                break;
            case SplitMetadata::Type::IntraBlock:
                insertSplitIntraBlockFixupCode(metadata);
                break;
            }
        }
        for (BasicBlock* block : m_code)
            m_insertionSets[block].execute(block);
    }

    void insertSplitAroundClobbersFixupCode(SplitMetadata& metadata)
    {
        ASSERT(metadata.type == SplitMetadata::Type::AroundClobbers);

        if (m_map[metadata.originalTmp].stage == Stage::Spilled) {
            m_stats[metadata.originalTmp.bank()].numSplitAroundClobberSpilled++;
            return; // If spilled, better to not split after all. See spill().
        }
        ASSERT(assignedReg(metadata.originalTmp));
        // Emit moves to and from the gapTmps (or stack stot) that fill the split holes.
        for (auto& split : metadata.splits) {
            Tmp gapTmp = split.tmp;
            TmpData& gapData = m_map[gapTmp];
            for (auto& interval : gapData.liveRange.intervals()) {
                ASSERT(interval.distance() == 2);
                Point lastPoint = interval.end() - 1;
                BasicBlock* block = findBlockContainingPoint(lastPoint);
                unsigned instIndex = this->instIndex(positionOfHead(block), lastPoint);
                Inst& inst = block->at(instIndex);

                Arg arg = gapTmp;
                StackSlot* spilled = spillSlot(gapTmp);
                if (spilled)
                    arg = Arg::stack(spilled);
                Opcode move = moveOpcode(gapTmp);
                m_insertionSets[block].insert(instIndex, splitMoveFrom, move, inst.origin, metadata.originalTmp, arg);
                m_insertionSets[block].insert(instIndex + 1, splitMoveTo, move, inst.origin, arg, metadata.originalTmp);
                dataLogLnIf(verbose(), "Inserted Moves around clobber tmp=", metadata.originalTmp, " gapTmp=", gapTmp, " gapReg = ", assignedReg(gapTmp), " block=", *block, " index=", instIndex, " inst = ", inst);
            }
        }
    }

    void insertSplitIntraBlockFixupCode(SplitMetadata& metadata)
    {
        ASSERT(metadata.type == SplitMetadata::Type::IntraBlock);

        Tmp originalTmp = metadata.originalTmp;
        Bank bank = originalTmp.bank();
        Opcode move = moveOpcode(originalTmp);
        StackSlot* spilled = spillSlot(originalTmp);
        ASSERT(spilled);

        for (auto& split : metadata.splits) {
            Tmp clusterTmp = split.tmp;
            if (spillSlot(clusterTmp)) {
                ASSERT(spillSlot(clusterTmp) == spillSlot(originalTmp));
                m_stats[bank].numSplitIntraBlockClusterTmpsSpilled++;
                continue; // No fixup needed since the same spill slot is used
            }

            LiveRange& clusterRange = m_map[clusterTmp].liveRange;
            ASSERT(clusterRange.intervals().size() == 1);
            const Interval& clusterInterval = clusterRange.intervals().first();

            BasicBlock* block = findBlockContainingPoint(clusterInterval.begin());
            Point positionOfHead = this->positionOfHead(block);

            // trySplitIntraBlock includes the Pre point in the cluster interval iff the original Tmp is live into the cluster.
            if (pointAtOffset(clusterInterval.begin(), PointOffsets::Pre) == clusterInterval.begin()) {
                unsigned instIndex = this->instIndex(positionOfHead, clusterInterval.begin());
                m_insertionSets[block].insert(instIndex, splitMoveFrom, move, block->at(instIndex).origin, Arg::stack(spilled), clusterTmp);
                m_stats[bank].numSplitIntraBlockLoad++;
            } else
                ASSERT(split.lastDefPoint);

            // Need to store back into the original Tmp's spill slot only if the cluster def'ed the Tmp
            if (split.lastDefPoint) {
                unsigned instIndex = this->instIndex(positionOfHead, split.lastDefPoint);
                m_insertionSets[block].insert(instIndex + 1, splitMoveTo, move, block->at(instIndex).origin, clusterTmp, Arg::stack(spilled));
                m_stats[bank].numSplitIntraBlockStore++;
            }
        }
    }

    bool mayBeCoalescable(Inst& inst)
    {
        switch (inst.kind.opcode) {
        case Move:
        case Move32:
        case MoveFloat:
        case MoveDouble:
        case MoveVector:
            break;
        default:
            return false;
        }

        // Avoid the three-argument coalescable spill moves.
        if (inst.args.size() != 2)
            return false;

        if (!inst.args[0].isTmp() || !inst.args[1].isTmp())
            return false;

        // We can coalesce a Move32 so long as either of the following holds:
        // - The input is already zero-filled.
        // - The output only cares about the low 32 bits.
        if (inst.kind.opcode == Move32 && !is32Bit() && m_tmpWidth.defWidth(inst.args[0].tmp()) > Width32)
            return false;
        return true;
    }

    void assignRegisters()
    {
        CompilerTimingScope timingScope("Air"_s, "GreedyRegAlloc::assignRegisters"_s);
        dataLogLnIf(verbose(), "Greedy register allocator about to assign registers:\n", *this, "IR:\n", m_code);

        for (BasicBlock* block : m_code) {
            for (Inst& inst : *block) {
                bool mayBeCoalescable = this->mayBeCoalescable(inst);
                dataLogLnIf(verbose(), "At: ", inst, mayBeCoalescable ? " [coalescable]" : "");

                if constexpr (isX86_64()) {
                    // Move32 is cheaper if we know that it's equivalent to a Move in x86_64. It's
                    // equivalent if the destination's high bits are not observable or if the source's high
                    // bits are all zero.
                    if (inst.kind.opcode == Move && inst.args[0].isTmp() && inst.args[1].isTmp()) {
                        if (m_tmpWidth.useWidth(inst.args[1].tmp()) <= Width32 || m_tmpWidth.defWidth(inst.args[0].tmp()) <= Width32)
                            inst.kind.opcode = Move32;
                    }
                }
                if constexpr (isARM64()) {
                    // On the other hand, on ARM64, Move is cheaper than Move32. We would like to use Move instead of Move32.
                    // Move32 on ARM64 is explicitly selected in B3LowerToAir for ZExt32 for example. But using ZDef information
                    // here can optimize it from Move32 to Move.
                    if (inst.kind.opcode == Move32 && inst.args[0].isTmp() && inst.args[1].isTmp()) {
                        if (m_tmpWidth.defWidth(inst.args[0].tmp()) <= Width32)
                            inst.kind.opcode = Move;
                    }
                }

                inst.forEachTmpFast([&](Tmp& tmp) {
                    if (tmp.isReg())
                        return;
                    Reg reg = assignedReg(tmp);
                    if (!reg) {
                        dataLogLn("Failed to allocate register: BB", *block, " inst=", inst, " tmp=", tmp);
                        dataLogLn("Reg Alloc State: ", *this);
                        RELEASE_ASSERT_NOT_REACHED();
                    }
                    tmp = Tmp(reg);
                });
                ASSERT(inst.isValidForm());

                if (mayBeCoalescable && inst.args[0].isTmp() && inst.args[1].isTmp()
                    && inst.args[0].tmp() == inst.args[1].tmp()) {
                    m_stats[inst.args[0].tmp().bank()].numCoalescedRegisterMoves++;
                    inst = Inst();
                }
            }
            // Remove all the useless moves we created in this block.
            block->insts().removeAllMatching([&] (const Inst& inst) {
                return !inst;
            });
        }
    }

    Code& m_code;
    Vector<Reg> m_allowedRegistersInPriorityOrder[numBanks];
    ScalarRegisterSet m_allAllowedRegisters;
    IndexMap<BasicBlock*, Point> m_blockToHeadPoint;
    Vector<Point> m_tailPoints;
    TmpMap<TmpData> m_map;
    TmpMap<UseDefList> m_useDefLists;
    Vector<SplitMetadata> m_splitMetadata;
    IndexMap<Reg, RegisterRange> m_regRanges;
    GenerationalSet<uint8_t, SaVector> m_visited;
    PriorityQueue<TmpPriority, TmpPriority::isHigherPriority> m_queue;
    IndexMap<BasicBlock*, PhaseInsertionSet> m_insertionSets;
    BlockWorklist m_fastBlocks;
    UseCounts m_useCounts;
    TmpWidth m_tmpWidth;
    std::array<AirAllocateRegistersStats, numBanks> m_stats = { GP, FP };
    std::array<bool, numBanks> m_didSpill { };
    bool m_needsEmitSpillCode { false };
    bool m_hasUseDefLists { false };
};

} // namespace JSC::B3::Air::Greedy

void allocateRegistersByGreedy(Code& code)
{
    PhaseScope phaseScope(code, "allocateRegistersByGreedy"_s);
    dataLogIf(Greedy::verbose(), "Air before greedy register allocation:\n", code);
    Greedy::GreedyAllocator allocator(code);
    allocator.run();
    dataLogIf(Greedy::verbose(), "Air after greedy register allocation:\n", code);
}

} } } // namespace JSC::B3::Air

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif // ENABLE(B3_JIT)
