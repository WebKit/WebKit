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

#include "config.h"
#include "AirIteratedRegisterCoalescing.h"

#if ENABLE(B3_JIT)

#include "AirCode.h"
#include "AirInsertionSet.h"
#include "AirInstInlines.h"
#include "AirLiveness.h"
#include "AirPhaseScope.h"
#include "AirRegisterPriority.h"
#include "AirTmpInlines.h"
#include "AirUseCounts.h"
#include <wtf/ListDump.h>
#include <wtf/ListHashSet.h>

namespace JSC { namespace B3 { namespace Air {

namespace {

bool debug = false;
bool traceDebug = false;
bool reportStats = false;

// The AbstractColoringAllocator defines all the code that is independant
// from the type or register and can be shared when allocating registers.
template<typename IndexType>
class AbstractColoringAllocator {
public:
    AbstractColoringAllocator(const Vector<Reg>& regsInPriorityOrder, IndexType lastPrecoloredRegisterIndex, unsigned tmpArraySize)
        : m_regsInPriorityOrder(regsInPriorityOrder)
        , m_lastPrecoloredRegisterIndex(lastPrecoloredRegisterIndex)
    {
        initializeDegrees(tmpArraySize);

        m_adjacencyList.resize(tmpArraySize);
        m_moveList.resize(tmpArraySize);
        m_coalescedTmps.fill(0, tmpArraySize);
        m_isOnSelectStack.ensureSize(tmpArraySize);
    }

protected:
    IndexType getAlias(IndexType tmpIndex) const
    {
        IndexType alias = tmpIndex;
        while (IndexType nextAlias = m_coalescedTmps[alias])
            alias = nextAlias;
        return alias;
    }

    void addEdge(IndexType a, IndexType b)
    {
        if (a == b)
            return;
        addEdgeDistinct(a, b);
    }

    void makeWorkList()
    {
        IndexType firstNonRegIndex = m_lastPrecoloredRegisterIndex + 1;
        for (IndexType i = firstNonRegIndex; i < m_degrees.size(); ++i) {
            unsigned degree = m_degrees[i];
            if (!degree)
                continue;

            if (degree >= m_regsInPriorityOrder.size())
                m_spillWorklist.add(i);
            else if (!m_moveList[i].isEmpty())
                m_freezeWorklist.add(i);
            else
                m_simplifyWorklist.append(i);
        }
    }

    // Low-degree vertex can always be colored: just pick any of the color taken by any
    // other adjacent verices.
    // The "Simplify" phase takes a low-degree out of the interference graph to simplify it.
    void simplify()
    {
        IndexType lastIndex = m_simplifyWorklist.takeLast();

        ASSERT(!m_selectStack.contains(lastIndex));
        ASSERT(!m_isOnSelectStack.get(lastIndex));
        m_selectStack.append(lastIndex);
        m_isOnSelectStack.quickSet(lastIndex);

        forEachAdjacent(lastIndex, [this](IndexType adjacentTmpIndex) {
            decrementDegree(adjacentTmpIndex);
        });
    }

    void freeze()
    {
        IndexType victimIndex = m_freezeWorklist.takeAny();
        ASSERT_WITH_MESSAGE(getAlias(victimIndex) == victimIndex, "coalesce() should not leave aliased Tmp in the worklist.");
        m_simplifyWorklist.append(victimIndex);
        freezeMoves(victimIndex);
    }

    void freezeMoves(IndexType tmpIndex)
    {
        forEachNodeMoves(tmpIndex, [this, tmpIndex] (IndexType moveIndex) {
            if (!m_activeMoves.quickClear(moveIndex))
                m_worklistMoves.takeMove(moveIndex);

            const MoveOperands& moveOperands = m_coalescingCandidates[moveIndex];
            IndexType srcTmpIndex = moveOperands.srcIndex;
            IndexType dstTmpIndex = moveOperands.dstIndex;

            IndexType originalOtherTmp = srcTmpIndex != tmpIndex ? srcTmpIndex : dstTmpIndex;
            IndexType otherTmpIndex = getAlias(originalOtherTmp);
            if (m_degrees[otherTmpIndex] < m_regsInPriorityOrder.size() && !isMoveRelated(otherTmpIndex)) {
                if (m_freezeWorklist.remove(otherTmpIndex))
                    m_simplifyWorklist.append(otherTmpIndex);
            }
        });
    }

    void coalesce()
    {
        unsigned moveIndex = m_worklistMoves.takeLastMove();
        const MoveOperands& moveOperands = m_coalescingCandidates[moveIndex];
        IndexType u = getAlias(moveOperands.srcIndex);
        IndexType v = getAlias(moveOperands.dstIndex);

        if (isPrecolored(v))
            std::swap(u, v);

        if (traceDebug)
            dataLog("Coalescing move at index", moveIndex, " u = ", u, " v = ", v, "\n");

        if (u == v) {
            addWorkList(u);

            if (traceDebug)
                dataLog("    Coalesced\n");
        } else if (isPrecolored(v) || m_interferenceEdges.contains(InterferenceEdge(u, v))) {
            addWorkList(u);
            addWorkList(v);

            if (traceDebug)
                dataLog("    Constrained\n");
        } else if (canBeSafelyCoalesced(u, v)) {
            combine(u, v);
            addWorkList(u);
            m_hasCoalescedNonTrivialMove = true;

            if (traceDebug)
                dataLog("    Safe Coalescing\n");
        } else {
            m_activeMoves.quickSet(moveIndex);

            if (traceDebug)
                dataLog("    Failed coalescing, added to active moves.\n");
        }
    }

    void assignColors()
    {
        ASSERT(m_simplifyWorklist.isEmpty());
        ASSERT(m_worklistMoves.isEmpty());
        ASSERT(m_freezeWorklist.isEmpty());
        ASSERT(m_spillWorklist.isEmpty());

        // Reclaim as much memory as possible.
        m_interferenceEdges.clear();
        m_degrees.clear();
        m_moveList.clear();
        m_worklistMoves.clear();
        m_simplifyWorklist.clear();
        m_spillWorklist.clear();
        m_freezeWorklist.clear();

        // Try to color the Tmp on the stack.
        m_coloredTmp.resize(m_adjacencyList.size());

        while (!m_selectStack.isEmpty()) {
            unsigned tmpIndex = m_selectStack.takeLast();
            ASSERT(!isPrecolored(tmpIndex));
            ASSERT(!m_coloredTmp[tmpIndex]);

            RegisterSet coloredRegisters;
            for (IndexType adjacentTmpIndex : m_adjacencyList[tmpIndex]) {
                IndexType aliasTmpIndex = getAlias(adjacentTmpIndex);
                Reg reg = m_coloredTmp[aliasTmpIndex];

                ASSERT(!isPrecolored(aliasTmpIndex) || (isPrecolored(aliasTmpIndex) && reg));

                if (reg)
                    coloredRegisters.set(reg);
            }

            bool colorAssigned = false;
            for (Reg reg : m_regsInPriorityOrder) {
                if (!coloredRegisters.get(reg)) {
                    m_coloredTmp[tmpIndex] = reg;
                    colorAssigned = true;
                    break;
                }
            }

            if (!colorAssigned)
                m_spilledTmps.append(tmpIndex);
        }
        m_selectStack.clear();

        if (m_spilledTmps.isEmpty())
            m_coalescedTmpsAtSpill.clear();
        else
            m_coloredTmp.clear();
    }

private:
    void initializeDegrees(unsigned tmpArraySize)
    {
        m_degrees.resize(tmpArraySize);

        // All precolored registers have  an "infinite" degree.
        unsigned firstNonRegIndex = m_lastPrecoloredRegisterIndex + 1;
        for (unsigned i = 0; i < firstNonRegIndex; ++i)
            m_degrees[i] = std::numeric_limits<unsigned>::max();

        bzero(m_degrees.data() + firstNonRegIndex, (tmpArraySize - firstNonRegIndex) * sizeof(unsigned));
    }

    void addEdgeDistinct(IndexType a, IndexType b)
    {
        ASSERT(a != b);
        if (m_interferenceEdges.add(InterferenceEdge(a, b)).isNewEntry) {
            if (!isPrecolored(a)) {
                ASSERT(!m_adjacencyList[a].contains(b));
                m_adjacencyList[a].append(b);
                m_degrees[a]++;
            }

            if (!isPrecolored(b)) {
                ASSERT(!m_adjacencyList[b].contains(a));
                m_adjacencyList[b].append(a);
                m_degrees[b]++;
            }
        }
    }

    void decrementDegree(IndexType tmpIndex)
    {
        ASSERT(m_degrees[tmpIndex]);

        unsigned oldDegree = m_degrees[tmpIndex]--;
        if (oldDegree == m_regsInPriorityOrder.size()) {
            enableMovesOnValueAndAdjacents(tmpIndex);
            m_spillWorklist.remove(tmpIndex);
            if (isMoveRelated(tmpIndex))
                m_freezeWorklist.add(tmpIndex);
            else
                m_simplifyWorklist.append(tmpIndex);
        }
    }

    bool isMoveRelated(IndexType tmpIndex)
    {
        for (unsigned moveIndex : m_moveList[tmpIndex]) {
            if (m_activeMoves.quickGet(moveIndex) || m_worklistMoves.contains(moveIndex))
                return true;
        }
        return false;
    }

    template<typename Function>
    void forEachAdjacent(IndexType tmpIndex, Function function)
    {
        for (IndexType adjacentTmpIndex : m_adjacencyList[tmpIndex]) {
            if (!hasBeenSimplified(adjacentTmpIndex))
                function(adjacentTmpIndex);
        }
    }

    bool hasBeenSimplified(IndexType tmpIndex)
    {
        return m_isOnSelectStack.quickGet(tmpIndex) || !!m_coalescedTmps[tmpIndex];
    }

    template<typename Function>
    void forEachNodeMoves(IndexType tmpIndex, Function function)
    {
        for (unsigned moveIndex : m_moveList[tmpIndex]) {
            if (m_activeMoves.quickGet(moveIndex) || m_worklistMoves.contains(moveIndex))
                function(moveIndex);
        }
    }

    void enableMovesOnValue(IndexType tmpIndex)
    {
        for (unsigned moveIndex : m_moveList[tmpIndex]) {
            if (m_activeMoves.quickClear(moveIndex))
                m_worklistMoves.returnMove(moveIndex);
        }
    }

    void enableMovesOnValueAndAdjacents(IndexType tmpIndex)
    {
        enableMovesOnValue(tmpIndex);

        forEachAdjacent(tmpIndex, [this] (IndexType adjacentTmpIndex) {
            enableMovesOnValue(adjacentTmpIndex);
        });
    }

    bool isPrecolored(IndexType tmpIndex)
    {
        return tmpIndex <= m_lastPrecoloredRegisterIndex;
    }

    void addWorkList(IndexType tmpIndex)
    {
        if (!isPrecolored(tmpIndex) && m_degrees[tmpIndex] < m_regsInPriorityOrder.size() && !isMoveRelated(tmpIndex)) {
            m_freezeWorklist.remove(tmpIndex);
            m_simplifyWorklist.append(tmpIndex);
        }
    }

    void combine(IndexType u, IndexType v)
    {
        if (!m_freezeWorklist.remove(v))
            m_spillWorklist.remove(v);

        ASSERT(!m_coalescedTmps[v]);
        m_coalescedTmps[v] = u;

        auto& vMoves = m_moveList[v];
        m_moveList[u].add(vMoves.begin(), vMoves.end());

        forEachAdjacent(v, [this, u] (IndexType adjacentTmpIndex) {
            addEdgeDistinct(adjacentTmpIndex, u);
            decrementDegree(adjacentTmpIndex);
        });

        if (m_degrees[u] >= m_regsInPriorityOrder.size() && m_freezeWorklist.remove(u))
            m_spillWorklist.add(u);
    }

    bool canBeSafelyCoalesced(IndexType u, IndexType v)
    {
        ASSERT(!isPrecolored(v));
        if (isPrecolored(u))
            return precoloredCoalescingHeuristic(u, v);
        return conservativeHeuristic(u, v);
    }

    bool conservativeHeuristic(IndexType u, IndexType v)
    {
        // This is using the Briggs' conservative coalescing rule:
        // If the number of combined adjacent node with a degree >= K is less than K,
        // it is safe to combine the two nodes. The reason is that we know that if the graph
        // is colorable, we have fewer than K adjacents with high order and there is a color
        // for the current node.
        ASSERT(u != v);
        ASSERT(!isPrecolored(u));
        ASSERT(!isPrecolored(v));

        const auto& adjacentsOfU = m_adjacencyList[u];
        const auto& adjacentsOfV = m_adjacencyList[v];

        if (adjacentsOfU.size() + adjacentsOfV.size() < m_regsInPriorityOrder.size()) {
            // Shortcut: if the total number of adjacents is less than the number of register, the condition is always met.
            return true;
        }

        HashSet<IndexType> highOrderAdjacents;

        for (IndexType adjacentTmpIndex : adjacentsOfU) {
            ASSERT(adjacentTmpIndex != v);
            ASSERT(adjacentTmpIndex != u);
            if (!hasBeenSimplified(adjacentTmpIndex) && m_degrees[adjacentTmpIndex] >= m_regsInPriorityOrder.size()) {
                auto addResult = highOrderAdjacents.add(adjacentTmpIndex);
                if (addResult.isNewEntry && highOrderAdjacents.size() >= m_regsInPriorityOrder.size())
                    return false;
            }
        }
        for (IndexType adjacentTmpIndex : adjacentsOfV) {
            ASSERT(adjacentTmpIndex != u);
            ASSERT(adjacentTmpIndex != v);
            if (!hasBeenSimplified(adjacentTmpIndex) && m_degrees[adjacentTmpIndex] >= m_regsInPriorityOrder.size()) {
                auto addResult = highOrderAdjacents.add(adjacentTmpIndex);
                if (addResult.isNewEntry && highOrderAdjacents.size() >= m_regsInPriorityOrder.size())
                    return false;
            }
        }

        ASSERT(highOrderAdjacents.size() < m_regsInPriorityOrder.size());
        return true;
    }

    bool precoloredCoalescingHeuristic(IndexType u, IndexType v)
    {
        ASSERT(isPrecolored(u));
        ASSERT(!isPrecolored(v));

        // If any adjacent of the non-colored node is not an adjacent of the colored node AND has a degree >= K
        // there is a risk that this node needs to have the same color as our precolored node. If we coalesce such
        // move, we may create an uncolorable graph.
        const auto& adjacentsOfV = m_adjacencyList[v];
        for (unsigned adjacentTmpIndex : adjacentsOfV) {
            if (!isPrecolored(adjacentTmpIndex)
                && !hasBeenSimplified(adjacentTmpIndex)
                && m_degrees[adjacentTmpIndex] >= m_regsInPriorityOrder.size()
                && !m_interferenceEdges.contains(InterferenceEdge(u, adjacentTmpIndex)))
                return false;
        }
        return true;
    }

protected:
#if PLATFORM(COCOA)
#pragma mark -
#endif

    // Interference edges are not directed. An edge between any two Tmps is represented
    // by the concatenated values of the smallest Tmp followed by the bigger Tmp.
    class InterferenceEdge {
    public:
        InterferenceEdge()
        {
        }

        InterferenceEdge(IndexType a, IndexType b)
        {
            ASSERT(a);
            ASSERT(b);
            ASSERT_WITH_MESSAGE(a != b, "A Tmp can never interfere with itself. Doing so would force it to be the superposition of two registers.");

            if (b < a)
                std::swap(a, b);
            m_value = static_cast<uint64_t>(a) << 32 | b;
        }

        InterferenceEdge(WTF::HashTableDeletedValueType)
            : m_value(std::numeric_limits<uint64_t>::max())
        {
        }

        IndexType first() const
        {
            return m_value >> 32 & 0xffffffff;
        }

        IndexType second() const
        {
            return m_value & 0xffffffff;
        }

        bool operator==(const InterferenceEdge other) const
        {
            return m_value == other.m_value;
        }

        bool isHashTableDeletedValue() const
        {
            return *this == InterferenceEdge(WTF::HashTableDeletedValue);
        }

        unsigned hash() const
        {
            return WTF::IntHash<uint64_t>::hash(m_value);
        }

        void dump(PrintStream& out) const
        {
            out.print(first(), "<=>", second());
        }

    private:
        uint64_t m_value { 0 };
    };

    struct InterferenceEdgeHash {
        static unsigned hash(const InterferenceEdge& key) { return key.hash(); }
        static bool equal(const InterferenceEdge& a, const InterferenceEdge& b) { return a == b; }
        static const bool safeToCompareToEmptyOrDeleted = true;
    };
    typedef SimpleClassHashTraits<InterferenceEdge> InterferenceEdgeHashTraits;

    const Vector<Reg>& m_regsInPriorityOrder;
    IndexType m_lastPrecoloredRegisterIndex { 0 };

    // The interference graph.
    HashSet<InterferenceEdge, InterferenceEdgeHash, InterferenceEdgeHashTraits> m_interferenceEdges;
    Vector<Vector<IndexType, 0, UnsafeVectorOverflow, 4>, 0, UnsafeVectorOverflow> m_adjacencyList;
    Vector<IndexType, 0, UnsafeVectorOverflow> m_degrees;

    // Instead of keeping track of the move instructions, we just keep their operands around and use the index
    // in the vector as the "identifier" for the move.
    struct MoveOperands {
        IndexType srcIndex;
        IndexType dstIndex;
    };
    Vector<MoveOperands, 0, UnsafeVectorOverflow> m_coalescingCandidates;

    // List of every move instruction associated with a Tmp.
    Vector<HashSet<IndexType, typename DefaultHash<IndexType>::Hash, WTF::UnsignedWithZeroKeyHashTraits<IndexType>>> m_moveList;

    // Colors.
    Vector<Reg, 0, UnsafeVectorOverflow> m_coloredTmp;
    Vector<IndexType> m_spilledTmps;

    // Values that have been coalesced with an other value.
    Vector<IndexType, 0, UnsafeVectorOverflow> m_coalescedTmps;

    // The stack of Tmp removed from the graph and ready for coloring.
    BitVector m_isOnSelectStack;
    Vector<IndexType> m_selectStack;

    struct OrderedMoveSet {
        unsigned addMove()
        {
            unsigned nextIndex = m_moveList.size();
            m_moveList.append(nextIndex);
            m_positionInMoveList.append(nextIndex);
            return nextIndex;
        }

        bool isEmpty() const
        {
            return m_moveList.isEmpty();
        }

        bool contains(unsigned index)
        {
            return m_positionInMoveList[index] != std::numeric_limits<unsigned>::max();
        }

        void takeMove(unsigned moveIndex)
        {
            unsigned positionInMoveList = m_positionInMoveList[moveIndex];
            if (positionInMoveList == std::numeric_limits<unsigned>::max())
                return;

            ASSERT(m_moveList[positionInMoveList] == moveIndex);
            unsigned lastIndex = m_moveList.last();
            m_positionInMoveList[lastIndex] = positionInMoveList;
            m_moveList[positionInMoveList] = lastIndex;
            m_moveList.removeLast();

            m_positionInMoveList[moveIndex] = std::numeric_limits<unsigned>::max();

            ASSERT(!contains(moveIndex));
        }

        unsigned takeLastMove()
        {
            ASSERT(!isEmpty());

            unsigned lastIndex = m_moveList.takeLast();
            ASSERT(m_positionInMoveList[lastIndex] == m_moveList.size());
            m_positionInMoveList[lastIndex] = std::numeric_limits<unsigned>::max();

            ASSERT(!contains(lastIndex));
            return lastIndex;
        }

        void returnMove(unsigned index)
        {
            // This assertion is a bit strict but that is how the move list should be used. The only kind of moves that can
            // return to the list are the ones that we previously failed to coalesce with the conservative heuristics.
            // Values should not be added back if they were never taken out when attempting coalescing.
            ASSERT(!contains(index));

            unsigned position = m_moveList.size();
            m_moveList.append(index);
            m_positionInMoveList[index] = position;

            ASSERT(contains(index));
        }

        void clear()
        {
            m_positionInMoveList.clear();
            m_moveList.clear();
        }

    private:
        Vector<unsigned, 0, UnsafeVectorOverflow> m_positionInMoveList;
        Vector<unsigned, 0, UnsafeVectorOverflow> m_moveList;
    };

    // Work lists.
    // Set of "move" enabled for possible coalescing.
    OrderedMoveSet m_worklistMoves;
    // Set of "move" not yet ready for coalescing.
    BitVector m_activeMoves;
    // Low-degree, non-Move related.
    Vector<IndexType> m_simplifyWorklist;
    // High-degree Tmp.
    HashSet<IndexType> m_spillWorklist;
    // Low-degree, Move related.
    HashSet<IndexType> m_freezeWorklist;

    bool m_hasSelectedSpill { false };
    bool m_hasCoalescedNonTrivialMove { false };

    // The mapping of Tmp to their alias for Moves that are always coalescing regardless of spilling.
    Vector<IndexType, 0, UnsafeVectorOverflow> m_coalescedTmpsAtSpill;
};

// This perform all the tasks that are specific to certain register type.
template<Arg::Type type>
class ColoringAllocator : public AbstractColoringAllocator<unsigned> {
public:
    ColoringAllocator(Code& code, const UseCounts<Tmp>& useCounts, const HashSet<unsigned>& unspillableTmp)
        : AbstractColoringAllocator<unsigned>(regsInPriorityOrder(type), AbsoluteTmpMapper<type>::lastMachineRegisterIndex(), tmpArraySize(code))
        , m_code(code)
        , m_useCounts(useCounts)
        , m_unspillableTmps(unspillableTmp)
    {
        initializePrecoloredTmp();
        build();
        allocate();
    }

    Tmp getAlias(Tmp tmp) const
    {
        return AbsoluteTmpMapper<type>::tmpFromAbsoluteIndex(getAlias(AbsoluteTmpMapper<type>::absoluteIndex(tmp)));
    }

    bool isUselessMove(const Inst& inst) const
    {
        return mayBeCoalescable(inst) && inst.args[0].tmp() == inst.args[1].tmp();
    }

    Tmp getAliasWhenSpilling(Tmp tmp) const
    {
        ASSERT_WITH_MESSAGE(!m_spilledTmps.isEmpty(), "This function is only valid for coalescing during spilling.");

        if (m_coalescedTmpsAtSpill.isEmpty())
            return tmp;

        unsigned aliasIndex = AbsoluteTmpMapper<type>::absoluteIndex(tmp);
        while (unsigned nextAliasIndex = m_coalescedTmpsAtSpill[aliasIndex])
            aliasIndex = nextAliasIndex;

        Tmp alias = AbsoluteTmpMapper<type>::tmpFromAbsoluteIndex(aliasIndex);

        ASSERT_WITH_MESSAGE(!m_spilledTmps.contains(aliasIndex) || alias == tmp, "The aliases at spill should always be colorable. Something went horribly wrong.");

        return alias;
    }

    template<typename IndexIterator>
    class IndexToTmpIteratorAdaptor {
    public:
        IndexToTmpIteratorAdaptor(IndexIterator&& indexIterator)
            : m_indexIterator(WTF::move(indexIterator))
        {
        }

        Tmp operator*() const { return AbsoluteTmpMapper<type>::tmpFromAbsoluteIndex(*m_indexIterator); }
        IndexToTmpIteratorAdaptor& operator++() { ++m_indexIterator; return *this; }

        bool operator==(const IndexToTmpIteratorAdaptor& other) const
        {
            return m_indexIterator == other.m_indexIterator;
        }

        bool operator!=(const IndexToTmpIteratorAdaptor& other) const
        {
            return !(*this == other);
        }

    private:
        IndexIterator m_indexIterator;
    };

    template<typename Collection>
    class IndexToTmpIterableAdaptor {
    public:
        IndexToTmpIterableAdaptor(const Collection& collection)
            : m_collection(collection)
        {
        }

        IndexToTmpIteratorAdaptor<typename Collection::const_iterator> begin() const
        {
            return m_collection.begin();
        }

        IndexToTmpIteratorAdaptor<typename Collection::const_iterator> end() const
        {
            return m_collection.end();
        }

    private:
        const Collection& m_collection;
    };

    IndexToTmpIterableAdaptor<Vector<unsigned>> spilledTmps() const { return m_spilledTmps; }

    bool requiresSpilling() const { return !m_spilledTmps.isEmpty(); }

    Reg allocatedReg(Tmp tmp) const
    {
        ASSERT(!tmp.isReg());
        ASSERT(m_coloredTmp.size());
        ASSERT(tmp.isGP() == (type == Arg::GP));

        Reg reg = m_coloredTmp[AbsoluteTmpMapper<type>::absoluteIndex(tmp)];
        if (!reg) {
            // We only care about Tmps that interfere. A Tmp that never interfere with anything
            // can take any register.
            reg = regsInPriorityOrder(type).first();
        }
        return reg;
    }

private:
    static unsigned tmpArraySize(Code& code)
    {
        unsigned numTmps = code.numTmps(type);
        return AbsoluteTmpMapper<type>::absoluteIndex(numTmps);
    }

    void initializePrecoloredTmp()
    {
        m_coloredTmp.resize(m_lastPrecoloredRegisterIndex + 1);
        for (unsigned i = 1; i <= m_lastPrecoloredRegisterIndex; ++i) {
            Tmp tmp = AbsoluteTmpMapper<type>::tmpFromAbsoluteIndex(i);
            ASSERT(tmp.isReg());
            m_coloredTmp[i] = tmp.reg();
        }
    }

    void build()
    {
        TmpLiveness<type> liveness(m_code);
        for (BasicBlock* block : m_code) {
            typename TmpLiveness<type>::LocalCalc localCalc(liveness, block);
            for (unsigned instIndex = block->size(); instIndex--;) {
                Inst& inst = block->at(instIndex);
                Inst* nextInst = instIndex + 1 < block->size() ? &block->at(instIndex + 1) : nullptr;
                build(inst, nextInst, localCalc);
                localCalc.execute(instIndex);
            }
        }
    }

    void build(Inst& inst, Inst* nextInst, const typename TmpLiveness<type>::LocalCalc& localCalc)
    {
        inst.forEachTmpWithExtraClobberedRegs(
            nextInst,
            [&] (const Tmp& arg, Arg::Role role, Arg::Type argType) {
                if (!Arg::isDef(role) || argType != type)
                    return;
                
                // All the Def()s interfere with each other and with all the extra clobbered Tmps.
                // We should not use forEachDefAndExtraClobberedTmp() here since colored Tmps
                // do not need interference edges in our implementation.
                inst.forEachTmp([&] (Tmp& otherArg, Arg::Role role, Arg::Type argType) {
                    if (!Arg::isDef(role) || argType != type)
                        return;

                    addEdge(arg, otherArg);
                });
            });

        if (mayBeCoalescable(inst)) {
            // We do not want the Use() of this move to interfere with the Def(), even if it is live
            // after the Move. If we were to add the interference edge, it would be impossible to
            // coalesce the Move even if the two Tmp never interfere anywhere.
            Tmp defTmp;
            Tmp useTmp;
            inst.forEachTmp([&defTmp, &useTmp] (Tmp& argTmp, Arg::Role role, Arg::Type) {
                if (Arg::isDef(role))
                    defTmp = argTmp;
                else {
                    ASSERT(Arg::isEarlyUse(role));
                    useTmp = argTmp;
                }
            });
            ASSERT(defTmp);
            ASSERT(useTmp);

            unsigned nextMoveIndex = m_coalescingCandidates.size();
            m_coalescingCandidates.append({ AbsoluteTmpMapper<type>::absoluteIndex(useTmp), AbsoluteTmpMapper<type>::absoluteIndex(defTmp) });

            unsigned newIndexInWorklist = m_worklistMoves.addMove();
            ASSERT_UNUSED(newIndexInWorklist, newIndexInWorklist == nextMoveIndex);

            ASSERT(nextMoveIndex <= m_activeMoves.size());
            m_activeMoves.ensureSize(nextMoveIndex + 1);

            for (const Arg& arg : inst.args) {
                auto& list = m_moveList[AbsoluteTmpMapper<type>::absoluteIndex(arg.tmp())];
                list.add(nextMoveIndex);
            }

            for (const Tmp& liveTmp : localCalc.live()) {
                if (liveTmp != useTmp)
                    addEdge(defTmp, liveTmp);
            }

            // The next instruction could have early clobbers. We need to consider those now.
            if (nextInst && nextInst->hasSpecial()) {
                nextInst->extraEarlyClobberedRegs().forEach([&] (Reg reg) {
                    if (reg.isGPR() == (type == Arg::GP)) {
                        for (const Tmp& liveTmp : localCalc.live())
                            addEdge(Tmp(reg), liveTmp);
                    }
                });
            }
        } else
            addEdges(inst, nextInst, localCalc.live());
    }

    void addEdges(Inst& inst, Inst* nextInst, typename TmpLiveness<type>::LocalCalc::Iterable liveTmps)
    {
        // All the Def()s interfere with everthing live.
        inst.forEachTmpWithExtraClobberedRegs(
            nextInst,
            [&] (const Tmp& arg, Arg::Role role, Arg::Type argType) {
                if (!Arg::isDef(role) || argType != type)
                    return;
                
                for (const Tmp& liveTmp : liveTmps) {
                    if (liveTmp.isGP() == (type == Arg::GP))
                        addEdge(arg, liveTmp);
                }
            });
    }

    void addEdge(Tmp a, Tmp b)
    {
        ASSERT_WITH_MESSAGE(a.isGP() == b.isGP(), "An interference between registers of different types does not make sense, it can lead to non-colorable graphs.");

        addEdge(AbsoluteTmpMapper<type>::absoluteIndex(a), AbsoluteTmpMapper<type>::absoluteIndex(b));
    }

    bool mayBeCoalescable(const Inst& inst) const
    {
        switch (type) {
        case Arg::GP:
            switch (inst.opcode) {
            case Move:
                break;
            default:
                return false;
            }
            break;
        case Arg::FP:
            switch (inst.opcode) {
            case MoveFloat:
            case MoveDouble:
                break;
            default:
                return false;
            }
            break;
        }

        ASSERT_WITH_MESSAGE(inst.args.size() == 2, "We assume coalecable moves only have two arguments in a few places.");

        if (!inst.args[0].isTmp() || !inst.args[1].isTmp())
            return false;

        ASSERT(inst.args[0].type() == type);
        ASSERT(inst.args[1].type() == type);

        return true;
    }

    void selectSpill()
    {
        if (!m_hasSelectedSpill) {
            m_hasSelectedSpill = true;

            if (m_hasCoalescedNonTrivialMove)
                m_coalescedTmpsAtSpill = m_coalescedTmps;
        }

        auto iterator = m_spillWorklist.begin();

        while (iterator != m_spillWorklist.end() && m_unspillableTmps.contains(*iterator))
            ++iterator;

        RELEASE_ASSERT_WITH_MESSAGE(iterator != m_spillWorklist.end(), "It is not possible to color the Air graph with the number of available registers.");

        // Higher score means more desirable to spill. Lower scores maximize the likelihood that a tmp
        // gets a register.
        auto score = [&] (Tmp tmp) -> double {
            // Air exposes the concept of "fast tmps", and we interpret that to mean that the tmp
            // should always be in a register.
            if (m_code.isFastTmp(tmp))
                return 0;
            
            // All else being equal, the score should be directly related to the degree.
            double degree = static_cast<double>(m_degrees[AbsoluteTmpMapper<type>::absoluteIndex(tmp)]);

            // All else being equal, the score should be inversely related to the number of warm uses and
            // defs.
            const UseCounts<Tmp>::Counts& counts = m_useCounts[tmp];
            double uses = counts.numWarmUses + counts.numDefs;

            return degree / uses;
        };

        auto victimIterator = iterator;
        double maxScore = score(AbsoluteTmpMapper<type>::tmpFromAbsoluteIndex(*iterator));

        ++iterator;
        for (;iterator != m_spillWorklist.end(); ++iterator) {
            double tmpScore = score(AbsoluteTmpMapper<type>::tmpFromAbsoluteIndex(*iterator));
            if (tmpScore > maxScore) {
                if (m_unspillableTmps.contains(*iterator))
                    continue;

                victimIterator = iterator;
                maxScore = tmpScore;
            }
        }

        unsigned victimIndex = *victimIterator;
        m_spillWorklist.remove(victimIterator);
        m_simplifyWorklist.append(victimIndex);

        freezeMoves(victimIndex);
    }

    void allocate()
    {
        ASSERT_WITH_MESSAGE(m_activeMoves.size() >= m_coalescingCandidates.size(), "The activeMove set should be big enough for the quick operations of BitVector.");

        makeWorkList();

        if (debug) {
            dataLog("Interference: ", listDump(m_interferenceEdges), "\n");
            dumpInterferenceGraphInDot(WTF::dataFile());
            dataLog("Initial work list\n");
            dumpWorkLists(WTF::dataFile());
        }

        do {
            if (traceDebug) {
                dataLog("Before Graph simplification iteration\n");
                dumpWorkLists(WTF::dataFile());
            }

            if (!m_simplifyWorklist.isEmpty())
                simplify();
            else if (!m_worklistMoves.isEmpty())
                coalesce();
            else if (!m_freezeWorklist.isEmpty())
                freeze();
            else if (!m_spillWorklist.isEmpty())
                selectSpill();

            if (traceDebug) {
                dataLog("After Graph simplification iteration\n");
                dumpWorkLists(WTF::dataFile());
            }
        } while (!m_simplifyWorklist.isEmpty() || !m_worklistMoves.isEmpty() || !m_freezeWorklist.isEmpty() || !m_spillWorklist.isEmpty());

        assignColors();
    }

#if PLATFORM(COCOA)
#pragma mark - Debugging helpers.
#endif

    void dumpInterferenceGraphInDot(PrintStream& out)
    {
        out.print("graph InterferenceGraph { \n");

        HashSet<Tmp> tmpsWithInterferences;
        for (const auto& edge : m_interferenceEdges) {
            tmpsWithInterferences.add(AbsoluteTmpMapper<type>::tmpFromAbsoluteIndex(edge.first()));
            tmpsWithInterferences.add(AbsoluteTmpMapper<type>::tmpFromAbsoluteIndex(edge.second()));
        }

        for (const auto& tmp : tmpsWithInterferences)
            out.print("    ", tmp.internalValue(), " [label=\"", tmp, " (", m_degrees[AbsoluteTmpMapper<type>::absoluteIndex(tmp)], ")\"];\n");

        for (const auto& edge : m_interferenceEdges)
            out.print("    ", edge.first(), " -- ", edge.second(), ";\n");
        out.print("}\n");
    }

    void dumpWorkLists(PrintStream& out)
    {
        out.print("Simplify work list:\n");
        for (unsigned tmpIndex : m_simplifyWorklist)
            out.print("    ", AbsoluteTmpMapper<type>::tmpFromAbsoluteIndex(tmpIndex), "\n");
        out.printf("Moves work list is empty? %d\n", m_worklistMoves.isEmpty());
        out.print("Freeze work list:\n");
        for (unsigned tmpIndex : m_freezeWorklist)
            out.print("    ", AbsoluteTmpMapper<type>::tmpFromAbsoluteIndex(tmpIndex), "\n");
        out.print("Spill work list:\n");
        for (unsigned tmpIndex : m_spillWorklist)
            out.print("    ", AbsoluteTmpMapper<type>::tmpFromAbsoluteIndex(tmpIndex), "\n");
    }

    using AbstractColoringAllocator<unsigned>::addEdge;
    using AbstractColoringAllocator<unsigned>::getAlias;

    Code& m_code;
    // FIXME: spilling should not type specific. It is only a side effect of using UseCounts.
    const UseCounts<Tmp>& m_useCounts;
    const HashSet<unsigned>& m_unspillableTmps;
};

class IteratedRegisterCoalescing {
public:
    IteratedRegisterCoalescing(Code& code)
        : m_code(code)
        , m_useCounts(code)
    {
    }

    void run()
    {
        iteratedRegisterCoalescingOnType<Arg::GP>();
        iteratedRegisterCoalescingOnType<Arg::FP>();

        if (reportStats)
            dataLog("Num iterations = ", m_numIterations, "\n");
    }

private:
    template<Arg::Type type>
    void iteratedRegisterCoalescingOnType()
    {
        HashSet<unsigned> unspillableTmps;
        while (true) {
            ++m_numIterations;
            ColoringAllocator<type> allocator(m_code, m_useCounts, unspillableTmps);
            if (!allocator.requiresSpilling()) {
                assignRegistersToTmp(allocator);
                return;
            }
            addSpillAndFill<type>(allocator, unspillableTmps);
        }
    }

    template<Arg::Type type>
    void assignRegistersToTmp(const ColoringAllocator<type>& allocator)
    {
        for (BasicBlock* block : m_code) {
            // Give Tmp a valid register.
            for (unsigned instIndex = 0; instIndex < block->size(); ++instIndex) {
                Inst& inst = block->at(instIndex);
                inst.forEachTmpFast([&] (Tmp& tmp) {
                    if (tmp.isReg() || tmp.isGP() == (type != Arg::GP))
                        return;

                    Tmp aliasTmp = allocator.getAlias(tmp);
                    Tmp assignedTmp;
                    if (aliasTmp.isReg())
                        assignedTmp = Tmp(aliasTmp.reg());
                    else {
                        auto reg = allocator.allocatedReg(aliasTmp);
                        ASSERT(reg);
                        assignedTmp = Tmp(reg);
                    }
                    ASSERT(assignedTmp.isReg());
                    tmp = assignedTmp;
                });
            }

            // Remove all the useless moves we created in this block.
            block->insts().removeAllMatching([&] (const Inst& inst) {
                return allocator.isUselessMove(inst);
            });
        }
    }

    template<Arg::Type type>
    void addSpillAndFill(const ColoringAllocator<type>& allocator, HashSet<unsigned>& unspillableTmps)
    {
        HashMap<Tmp, StackSlot*> stackSlots;
        for (Tmp tmp : allocator.spilledTmps()) {
            // All the spilled values become unspillable.
            unspillableTmps.add(AbsoluteTmpMapper<type>::absoluteIndex(tmp));

            // Allocate stack slot for each spilled value.
            bool isNewTmp = stackSlots.add(tmp, m_code.addStackSlot(8, StackSlotKind::Anonymous)).isNewEntry;
            ASSERT_UNUSED(isNewTmp, isNewTmp);
        }

        // Rewrite the program to get rid of the spilled Tmp.
        InsertionSet insertionSet(m_code);
        for (BasicBlock* block : m_code) {
            bool hasAliasedTmps = false;

            for (unsigned instIndex = 0; instIndex < block->size(); ++instIndex) {
                Inst& inst = block->at(instIndex);

                // Try to replace the register use by memory use when possible.
                for (unsigned i = 0; i < inst.args.size(); ++i) {
                    Arg& arg = inst.args[i];
                    if (arg.isTmp() && arg.type() == type && !arg.isReg()) {
                        auto stackSlotEntry = stackSlots.find(arg.tmp());
                        if (stackSlotEntry != stackSlots.end() && inst.admitsStack(i))
                            arg = Arg::stack(stackSlotEntry->value);
                    }
                }

                // For every other case, add Load/Store as needed.
                inst.forEachTmp([&] (Tmp& tmp, Arg::Role role, Arg::Type argType) {
                    if (tmp.isReg() || argType != type)
                        return;

                    auto stackSlotEntry = stackSlots.find(tmp);
                    if (stackSlotEntry == stackSlots.end()) {
                        Tmp alias = allocator.getAliasWhenSpilling(tmp);
                        if (alias != tmp) {
                            tmp = alias;
                            hasAliasedTmps = true;
                        }
                        return;
                    }

                    Arg arg = Arg::stack(stackSlotEntry->value);
                    Opcode move = type == Arg::GP ? Move : MoveDouble;

                    if (Arg::isAnyUse(role)) {
                        Tmp newTmp = m_code.newTmp(type);
                        insertionSet.insert(instIndex, move, inst.origin, arg, newTmp);
                        tmp = newTmp;

                        // Any new Fill() should never be spilled.
                        unspillableTmps.add(AbsoluteTmpMapper<type>::absoluteIndex(tmp));
                    }
                    if (Arg::isDef(role))
                        insertionSet.insert(instIndex + 1, move, inst.origin, tmp, arg);
                });
            }
            insertionSet.execute(block);

            if (hasAliasedTmps) {
                block->insts().removeAllMatching([&] (const Inst& inst) {
                    return allocator.isUselessMove(inst);
                });
            }
        }
    }

    Code& m_code;
    UseCounts<Tmp> m_useCounts;
    unsigned m_numIterations { 0 };
};

} // anonymous namespace

void iteratedRegisterCoalescing(Code& code)
{
    PhaseScope phaseScope(code, "iteratedRegisterCoalescing");

    IteratedRegisterCoalescing iteratedRegisterCoalescing(code);
    iteratedRegisterCoalescing.run();
}

} } } // namespace JSC::B3::Air

#endif // ENABLE(B3_JIT)
