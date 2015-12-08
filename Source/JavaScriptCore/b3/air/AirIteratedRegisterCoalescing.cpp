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

template<Arg::Type type>
struct MoveInstHelper;

template<>
struct MoveInstHelper<Arg::GP> {
    static bool mayBeCoalescable(const Inst& inst)
    {
        bool isMove = inst.opcode == Move;
        if (!isMove)
            return false;

        ASSERT_WITH_MESSAGE(inst.args.size() == 2, "We assume coalecable moves only have two arguments in a few places.");
        ASSERT(inst.args[0].isType(Arg::GP));
        ASSERT(inst.args[1].isType(Arg::GP));

        return inst.args[0].isTmp() && inst.args[1].isTmp();
    }
};

template<>
struct MoveInstHelper<Arg::FP> {
    static bool mayBeCoalescable(const Inst& inst)
    {
        if (inst.opcode != MoveDouble)
            return false;

        ASSERT_WITH_MESSAGE(inst.args.size() == 2, "We assume coalecable moves only have two arguments in a few places.");
        ASSERT(inst.args[0].isType(Arg::FP));
        ASSERT(inst.args[1].isType(Arg::FP));

        return inst.args[0].isTmp() && inst.args[1].isTmp();
    }
};

template<Arg::Type type>
class IteratedRegisterCoalescingAllocator {
public:
    IteratedRegisterCoalescingAllocator(
        Code& code, const UseCounts<Tmp>& useCounts, const HashSet<Tmp>& unspillableTmp)
        : m_code(code)
        , m_useCounts(useCounts)
        , m_unspillableTmp(unspillableTmp)
        , m_numberOfRegisters(regsInPriorityOrder(type).size())
    {
        initializeDegrees();

        unsigned tmpArraySize = this->tmpArraySize();
        m_adjacencyList.resize(tmpArraySize);
        m_moveList.resize(tmpArraySize);
        m_coalescedTmps.resize(tmpArraySize);
        m_isOnSelectStack.ensureSize(tmpArraySize);

        build();
        allocate();
    }

    Tmp getAlias(Tmp tmp) const
    {
        Tmp alias = tmp;
        while (Tmp nextAlias = m_coalescedTmps[AbsoluteTmpMapper<type>::absoluteIndex(alias)])
            alias = nextAlias;
        return alias;
    }

    Tmp getAliasWhenSpilling(Tmp tmp) const
    {
        ASSERT_WITH_MESSAGE(!m_spilledTmp.isEmpty(), "This function is only valid for coalescing during spilling.");

        if (m_coalescedTmpsAtSpill.isEmpty())
            return tmp;

        Tmp alias = tmp;
        while (Tmp nextAlias = m_coalescedTmpsAtSpill[AbsoluteTmpMapper<type>::absoluteIndex(alias)])
            alias = nextAlias;

        ASSERT_WITH_MESSAGE(!m_spilledTmp.contains(tmp) || alias == tmp, "The aliases at spill should always be colorable. Something went horribly wrong.");

        return alias;
    }

    const HashSet<Tmp>& spilledTmp() const { return m_spilledTmp; }
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
    unsigned tmpArraySize()
    {
        unsigned numTmps = m_code.numTmps(type);
        return AbsoluteTmpMapper<type>::absoluteIndex(numTmps);
    }

    void initializeDegrees()
    {
        unsigned tmpArraySize = this->tmpArraySize();
        m_degrees.resize(tmpArraySize);

        // All precolored registers have  an "infinite" degree.
        unsigned firstNonRegIndex = AbsoluteTmpMapper<type>::absoluteIndex(0);
        for (unsigned i = 0; i < firstNonRegIndex; ++i)
            m_degrees[i] = std::numeric_limits<unsigned>::max();

        bzero(m_degrees.data() + firstNonRegIndex, (tmpArraySize - firstNonRegIndex) * sizeof(unsigned));
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

        if (MoveInstHelper<type>::mayBeCoalescable(inst)) {
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
            m_coalescingCandidates.append({ useTmp, defTmp });

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
                nextInst->extraEarlyClobberedRegs().forEach(
                    [&] (Reg reg) {
                        if (reg.isGPR() == (type == Arg::GP)) {
                            for (const Tmp& liveTmp : localCalc.live()) {
                                addEdge(Tmp(reg), liveTmp);
                            }
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

    void addEdge(const Tmp& a, const Tmp& b)
    {
        if (a == b)
            return;

        if (m_interferenceEdges.add(InterferenceEdge(a, b)).isNewEntry) {
            if (!a.isReg()) {
                ASSERT(!m_adjacencyList[AbsoluteTmpMapper<type>::absoluteIndex(a)].contains(b));
                m_adjacencyList[AbsoluteTmpMapper<type>::absoluteIndex(a)].append(b);
                m_degrees[AbsoluteTmpMapper<type>::absoluteIndex(a)]++;
            }

            if (!b.isReg()) {
                ASSERT(!m_adjacencyList[AbsoluteTmpMapper<type>::absoluteIndex(b)].contains(a));
                m_adjacencyList[AbsoluteTmpMapper<type>::absoluteIndex(b)].append(a);
                m_degrees[AbsoluteTmpMapper<type>::absoluteIndex(b)]++;
            }
        }
    }

    void makeWorkList()
    {
        unsigned firstNonRegIndex = AbsoluteTmpMapper<type>::absoluteIndex(0);
        for (unsigned i = firstNonRegIndex; i < m_degrees.size(); ++i) {
            unsigned degree = m_degrees[i];
            if (!degree)
                continue;

            Tmp tmp = AbsoluteTmpMapper<type>::tmpFromAbsoluteIndex(i);

            if (degree >= m_numberOfRegisters)
                m_spillWorklist.add(tmp);
            else if (!m_moveList[AbsoluteTmpMapper<type>::absoluteIndex(tmp)].isEmpty())
                m_freezeWorklist.add(tmp);
            else
                m_simplifyWorklist.append(tmp);
        }
    }

    void simplify()
    {
        Tmp last = m_simplifyWorklist.takeLast();

        ASSERT(!m_selectStack.contains(last));
        ASSERT(!m_isOnSelectStack.get(AbsoluteTmpMapper<type>::absoluteIndex(last)));
        m_selectStack.append(last);
        m_isOnSelectStack.quickSet(AbsoluteTmpMapper<type>::absoluteIndex(last));

        forEachAdjacent(last, [this](Tmp adjacentTmp) {
            decrementDegree(adjacentTmp);
        });
    }

    template<typename Function>
    void forEachAdjacent(Tmp tmp, Function function)
    {
        for (Tmp adjacentTmp : m_adjacencyList[AbsoluteTmpMapper<type>::absoluteIndex(tmp)]) {
            if (!hasBeenSimplified(adjacentTmp))
                function(adjacentTmp);
        }
    }

    bool hasBeenSimplified(Tmp tmp)
    {
        return m_isOnSelectStack.quickGet(AbsoluteTmpMapper<type>::absoluteIndex(tmp)) || !!m_coalescedTmps[AbsoluteTmpMapper<type>::absoluteIndex(tmp)];
    }

    void decrementDegree(Tmp tmp)
    {
        ASSERT(m_degrees[AbsoluteTmpMapper<type>::absoluteIndex(tmp)]);

        unsigned oldDegree = m_degrees[AbsoluteTmpMapper<type>::absoluteIndex(tmp)]--;
        if (oldDegree == m_numberOfRegisters) {
            enableMovesOnValueAndAdjacents(tmp);
            m_spillWorklist.remove(tmp);
            if (isMoveRelated(tmp))
                m_freezeWorklist.add(tmp);
            else
                m_simplifyWorklist.append(tmp);
        }
    }

    template<typename Function>
    void forEachNodeMoves(Tmp tmp, Function function)
    {
        for (unsigned moveIndex : m_moveList[AbsoluteTmpMapper<type>::absoluteIndex(tmp)]) {
            if (m_activeMoves.quickGet(moveIndex) || m_worklistMoves.contains(moveIndex))
                function(moveIndex);
        }
    }

    bool isMoveRelated(Tmp tmp)
    {
        for (unsigned moveIndex : m_moveList[AbsoluteTmpMapper<type>::absoluteIndex(tmp)]) {
            if (m_activeMoves.quickGet(moveIndex) || m_worklistMoves.contains(moveIndex))
                return true;
        }
        return false;
    }

    void enableMovesOnValue(Tmp tmp)
    {
        for (unsigned moveIndex : m_moveList[AbsoluteTmpMapper<type>::absoluteIndex(tmp)]) {
            if (m_activeMoves.quickClear(moveIndex))
                m_worklistMoves.returnMove(moveIndex);
        }
    }

    void enableMovesOnValueAndAdjacents(Tmp tmp)
    {
        enableMovesOnValue(tmp);

        forEachAdjacent(tmp, [this] (Tmp adjacentTmp) {
            enableMovesOnValue(adjacentTmp);
        });
    }

    void coalesce()
    {
        unsigned moveIndex = m_worklistMoves.takeLastMove();
        const MoveOperands& moveOperands = m_coalescingCandidates[moveIndex];
        Tmp u = moveOperands.src;
        u = getAlias(u);
        Tmp v = moveOperands.dst;
        v = getAlias(v);

        if (v.isReg())
            std::swap(u, v);

        if (traceDebug)
            dataLog("Coalescing move at index", moveIndex, " u = ", u, " v = ", v, "\n");

        if (u == v) {
            addWorkList(u);

            if (traceDebug)
                dataLog("    Coalesced\n");
        } else if (v.isReg() || m_interferenceEdges.contains(InterferenceEdge(u, v))) {
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

    bool canBeSafelyCoalesced(Tmp u, Tmp v)
    {
        ASSERT(!v.isReg());
        if (u.isReg())
            return precoloredCoalescingHeuristic(u, v);
        return conservativeHeuristic(u, v);
    }

    bool precoloredCoalescingHeuristic(Tmp u, Tmp v)
    {
        ASSERT(u.isReg());
        ASSERT(!v.isReg());

        // If any adjacent of the non-colored node is not an adjacent of the colored node AND has a degree >= K
        // there is a risk that this node needs to have the same color as our precolored node. If we coalesce such
        // move, we may create an uncolorable graph.
        const auto& adjacentsOfV = m_adjacencyList[AbsoluteTmpMapper<type>::absoluteIndex(v)];
        for (Tmp adjacentTmp : adjacentsOfV) {
            if (!adjacentTmp.isReg()
                && !hasBeenSimplified(adjacentTmp)
                && m_degrees[AbsoluteTmpMapper<type>::absoluteIndex(adjacentTmp)] >= m_numberOfRegisters
                && !m_interferenceEdges.contains(InterferenceEdge(u, adjacentTmp)))
                return false;
        }
        return true;
    }

    bool conservativeHeuristic(Tmp u, Tmp v)
    {
        // This is using the Briggs' conservative coalescing rule:
        // If the number of combined adjacent node with a degree >= K is less than K,
        // it is safe to combine the two nodes. The reason is that we know that if the graph
        // is colorable, we have fewer than K adjacents with high order and there is a color
        // for the current node.
        ASSERT(u != v);
        ASSERT(!u.isReg());
        ASSERT(!v.isReg());

        const auto& adjacentsOfU = m_adjacencyList[AbsoluteTmpMapper<type>::absoluteIndex(u)];
        const auto& adjacentsOfV = m_adjacencyList[AbsoluteTmpMapper<type>::absoluteIndex(v)];

        if (adjacentsOfU.size() + adjacentsOfV.size() < m_numberOfRegisters) {
            // Shortcut: if the total number of adjacents is less than the number of register, the condition is always met.
            return true;
        }

        HashSet<Tmp> highOrderAdjacents;

        for (Tmp adjacentTmp : adjacentsOfU) {
            ASSERT(adjacentTmp != v);
            ASSERT(adjacentTmp != u);
            if (!hasBeenSimplified(adjacentTmp) && m_degrees[AbsoluteTmpMapper<type>::absoluteIndex(adjacentTmp)] >= m_numberOfRegisters) {
                auto addResult = highOrderAdjacents.add(adjacentTmp);
                if (addResult.isNewEntry && highOrderAdjacents.size() >= m_numberOfRegisters)
                    return false;
            }
        }
        for (Tmp adjacentTmp : adjacentsOfV) {
            ASSERT(adjacentTmp != u);
            ASSERT(adjacentTmp != v);
            if (!hasBeenSimplified(adjacentTmp) && m_degrees[AbsoluteTmpMapper<type>::absoluteIndex(adjacentTmp)] >= m_numberOfRegisters) {
                auto addResult = highOrderAdjacents.add(adjacentTmp);
                if (addResult.isNewEntry && highOrderAdjacents.size() >= m_numberOfRegisters)
                    return false;
            }
        }

        ASSERT(highOrderAdjacents.size() < m_numberOfRegisters);
        return true;
    }

    void addWorkList(Tmp tmp)
    {
        if (!tmp.isReg() && m_degrees[AbsoluteTmpMapper<type>::absoluteIndex(tmp)] < m_numberOfRegisters && !isMoveRelated(tmp)) {
            m_freezeWorklist.remove(tmp);
            m_simplifyWorklist.append(tmp);
        }
    }

    void combine(Tmp u, Tmp v)
    {
        if (!m_freezeWorklist.remove(v))
            m_spillWorklist.remove(v);

        ASSERT(!m_coalescedTmps[AbsoluteTmpMapper<type>::absoluteIndex(v)]);
        m_coalescedTmps[AbsoluteTmpMapper<type>::absoluteIndex(v)] = u;

        auto& vMoves = m_moveList[AbsoluteTmpMapper<type>::absoluteIndex(v)];
        m_moveList[AbsoluteTmpMapper<type>::absoluteIndex(u)].add(vMoves.begin(), vMoves.end());

        forEachAdjacent(v, [this, u] (Tmp adjacentTmp) {
            addEdge(adjacentTmp, u);
            decrementDegree(adjacentTmp);
        });

        if (m_degrees[AbsoluteTmpMapper<type>::absoluteIndex(u)] >= m_numberOfRegisters && m_freezeWorklist.remove(u))
            m_spillWorklist.add(u);
    }

    void freeze()
    {
        Tmp victim = m_freezeWorklist.takeAny();
        ASSERT_WITH_MESSAGE(getAlias(victim) == victim, "coalesce() should not leave aliased Tmp in the worklist.");
        m_simplifyWorklist.append(victim);
        freezeMoves(victim);
    }

    void freezeMoves(Tmp tmp)
    {
        forEachNodeMoves(tmp, [this, tmp] (unsigned moveIndex) {
            if (!m_activeMoves.quickClear(moveIndex))
                m_worklistMoves.takeMove(moveIndex);

            const MoveOperands& moveOperands = m_coalescingCandidates[moveIndex];
            Tmp originalOtherTmp = moveOperands.src != tmp ? moveOperands.src : moveOperands.dst;
            Tmp otherTmp = getAlias(originalOtherTmp);
            if (m_degrees[AbsoluteTmpMapper<type>::absoluteIndex(otherTmp)] < m_numberOfRegisters && !isMoveRelated(otherTmp)) {
                if (m_freezeWorklist.remove(otherTmp))
                    m_simplifyWorklist.append(otherTmp);
            }
        });
    }

    void selectSpill()
    {
        if (!m_hasSelectedSpill) {
            m_hasSelectedSpill = true;

            if (m_hasCoalescedNonTrivialMove)
                m_coalescedTmpsAtSpill = m_coalescedTmps;
        }

        auto iterator = m_spillWorklist.begin();

        while (iterator != m_spillWorklist.end() && m_unspillableTmp.contains(*iterator))
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
        double maxScore = score(*iterator);

        ++iterator;
        for (;iterator != m_spillWorklist.end(); ++iterator) {
            double tmpScore = score(*iterator);
            if (tmpScore > maxScore) {
                if (m_unspillableTmp.contains(*iterator))
                    continue;

                victimIterator = iterator;
                maxScore = tmpScore;
            }
        }

        Tmp victimTmp = *victimIterator;
        m_spillWorklist.remove(victimIterator);
        m_simplifyWorklist.append(victimTmp);
        freezeMoves(victimTmp);
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
        const auto& registersInPriorityOrder = regsInPriorityOrder(type);

        while (!m_selectStack.isEmpty()) {
            Tmp tmp = m_selectStack.takeLast();
            ASSERT(!tmp.isReg());
            ASSERT(!m_coloredTmp[AbsoluteTmpMapper<type>::absoluteIndex(tmp)]);

            RegisterSet coloredRegisters;
            for (Tmp adjacentTmp : m_adjacencyList[AbsoluteTmpMapper<type>::absoluteIndex(tmp)]) {
                Tmp aliasTmp = getAlias(adjacentTmp);
                if (aliasTmp.isReg()) {
                    coloredRegisters.set(aliasTmp.reg());
                    continue;
                }

                Reg reg = m_coloredTmp[AbsoluteTmpMapper<type>::absoluteIndex(aliasTmp)];
                if (reg)
                    coloredRegisters.set(reg);
            }

            bool colorAssigned = false;
            for (Reg reg : registersInPriorityOrder) {
                if (!coloredRegisters.get(reg)) {
                    m_coloredTmp[AbsoluteTmpMapper<type>::absoluteIndex(tmp)] = reg;
                    colorAssigned = true;
                    break;
                }
            }

            if (!colorAssigned)
                m_spilledTmp.add(tmp);
        }
        m_selectStack.clear();

        if (m_spilledTmp.isEmpty())
            m_coalescedTmpsAtSpill.clear();
        else
            m_coloredTmp.clear();
    }

#if PLATFORM(COCOA)
#pragma mark - Debugging helpers.
#endif

    void dumpInterferenceGraphInDot(PrintStream& out)
    {
        out.print("graph InterferenceGraph { \n");

        HashSet<Tmp> tmpsWithInterferences;
        for (const auto& edge : m_interferenceEdges) {
            tmpsWithInterferences.add(edge.first());
            tmpsWithInterferences.add(edge.second());
        }

        for (const auto& tmp : tmpsWithInterferences)
            out.print("    ", tmp.internalValue(), " [label=\"", tmp, " (", m_degrees[AbsoluteTmpMapper<type>::absoluteIndex(tmp)], ")\"];\n");

        for (const auto& edge : m_interferenceEdges)
            out.print("    ", edge.first().internalValue(), " -- ", edge.second().internalValue(), ";\n");
        out.print("}\n");
    }

    void dumpWorkLists(PrintStream& out)
    {
        out.print("Simplify work list:\n");
        for (Tmp tmp : m_simplifyWorklist)
            out.print("    ", tmp, "\n");
        out.printf("Moves work list is empty? %d\n", m_worklistMoves.isEmpty());
        out.print("Freeze work list:\n");
        for (Tmp tmp : m_freezeWorklist)
            out.print("    ", tmp, "\n");
        out.print("Spill work list:\n");
        for (Tmp tmp : m_spillWorklist)
            out.print("    ", tmp, "\n");
    }

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

        InterferenceEdge(Tmp a, Tmp b)
        {
            ASSERT(a.internalValue());
            ASSERT(b.internalValue());
            ASSERT_WITH_MESSAGE(a != b, "A Tmp can never interfere with itself. Doing so would force it to be the superposition of two registers.");

            unsigned aInternal = a.internalValue();
            unsigned bInternal = b.internalValue();
            if (bInternal < aInternal)
                std::swap(aInternal, bInternal);
            m_value = static_cast<uint64_t>(aInternal) << 32 | bInternal;
        }

        InterferenceEdge(WTF::HashTableDeletedValueType)
            : m_value(std::numeric_limits<uint64_t>::max())
        {
        }

        Tmp first() const
        {
            return Tmp::tmpForInternalValue(m_value >> 32);
        }

        Tmp second() const
        {
            return Tmp::tmpForInternalValue(m_value & 0xffffffff);
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

    Code& m_code;
    const UseCounts<Tmp>& m_useCounts;

    const HashSet<Tmp>& m_unspillableTmp;
    unsigned m_numberOfRegisters { 0 };

    // The interference graph.
    HashSet<InterferenceEdge, InterferenceEdgeHash, InterferenceEdgeHashTraits> m_interferenceEdges;
    Vector<Vector<Tmp, 0, UnsafeVectorOverflow, 4>, 0, UnsafeVectorOverflow> m_adjacencyList;
    Vector<unsigned, 0, UnsafeVectorOverflow> m_degrees;

    // Instead of keeping track of the move instructions, we just keep their operands around and use the index
    // in the vector as the "identifier" for the move.
    struct MoveOperands {
        Tmp src;
        Tmp dst;
    };
    Vector<MoveOperands, 0, UnsafeVectorOverflow> m_coalescingCandidates;

    // List of every move instruction associated with a Tmp.
    Vector<HashSet<unsigned, DefaultHash<unsigned>::Hash, WTF::UnsignedWithZeroKeyHashTraits<unsigned>>> m_moveList;

    // Colors.
    Vector<Reg, 0, UnsafeVectorOverflow> m_coloredTmp;
    HashSet<Tmp> m_spilledTmp;

    // Values that have been coalesced with an other value.
    Vector<Tmp, 0, UnsafeVectorOverflow> m_coalescedTmps;

    // The stack of Tmp removed from the graph and ready for coloring.
    BitVector m_isOnSelectStack;
    Vector<Tmp> m_selectStack;

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
    Vector<Tmp> m_simplifyWorklist;
    // High-degree Tmp.
    HashSet<Tmp> m_spillWorklist;
    // Low-degree, Move related.
    HashSet<Tmp> m_freezeWorklist;

    bool m_hasSelectedSpill { false };
    bool m_hasCoalescedNonTrivialMove { false };

    // The mapping of Tmp to their alias for Moves that are always coalescing regardless of spilling.
    Vector<Tmp, 0, UnsafeVectorOverflow> m_coalescedTmpsAtSpill;
};

template<Arg::Type type>
bool isUselessMoveInst(const Inst& inst)
{
    return MoveInstHelper<type>::mayBeCoalescable(inst) && inst.args[0].tmp() == inst.args[1].tmp();
}

template<Arg::Type type>
void assignRegisterToTmpInProgram(Code& code, const IteratedRegisterCoalescingAllocator<type>& allocator)
{
    for (BasicBlock* block : code) {
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
        block->insts().removeAllMatching(isUselessMoveInst<type>);
    }
}

template<Arg::Type type>
void addSpillAndFillToProgram(Code& code, const IteratedRegisterCoalescingAllocator<type>& allocator, HashSet<Tmp>& unspillableTmp)
{
    const HashSet<Tmp>& spilledTmp = allocator.spilledTmp();

    // All the spilled values become unspillable.
    unspillableTmp.add(spilledTmp.begin(), spilledTmp.end());

    // Allocate stack slot for each spilled value.
    HashMap<Tmp, StackSlot*> stackSlots;
    for (Tmp tmp : spilledTmp) {
        bool isNewTmp = stackSlots.add(tmp, code.addStackSlot(8, StackSlotKind::Anonymous)).isNewEntry;
        ASSERT_UNUSED(isNewTmp, isNewTmp);
    }

    // Rewrite the program to get rid of the spilled Tmp.
    InsertionSet insertionSet(code);
    for (BasicBlock* block : code) {
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
                    Tmp newTmp = code.newTmp(type);
                    insertionSet.insert(instIndex, move, inst.origin, arg, newTmp);
                    tmp = newTmp;

                    // Any new Fill() should never be spilled.
                    unspillableTmp.add(tmp);
                }
                if (Arg::isDef(role))
                    insertionSet.insert(instIndex + 1, move, inst.origin, tmp, arg);
            });
        }
        insertionSet.execute(block);

        if (hasAliasedTmps)
            block->insts().removeAllMatching(isUselessMoveInst<type>);
    }
}

template<Arg::Type type>
void iteratedRegisterCoalescingOnType(
    Code& code, const UseCounts<Tmp>& useCounts, unsigned& numIterations)
{
    HashSet<Tmp> unspillableTmps;
    while (true) {
        numIterations++;
        IteratedRegisterCoalescingAllocator<type> allocator(code, useCounts, unspillableTmps);
        if (allocator.spilledTmp().isEmpty()) {
            assignRegisterToTmpInProgram(code, allocator);
            return;
        }
        addSpillAndFillToProgram<type>(code, allocator, unspillableTmps);
    }
}

} // anonymous namespace

void iteratedRegisterCoalescing(Code& code)
{
    PhaseScope phaseScope(code, "iteratedRegisterCoalescing");

    unsigned numIterations = 0;

    UseCounts<Tmp> useCounts(code);
    iteratedRegisterCoalescingOnType<Arg::GP>(code, useCounts, numIterations);
    iteratedRegisterCoalescingOnType<Arg::FP>(code, useCounts, numIterations);

    if (reportStats)
        dataLog("Num iterations = ", numIterations, "\n");
}

} } } // namespace JSC::B3::Air

#endif // ENABLE(B3_JIT)
