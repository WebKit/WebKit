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
#include "AirAllocateRegistersAndStackByLinearScan.h"

#if ENABLE(B3_JIT)

#include "AirArgInlines.h"
#include "AirCode.h"
#include "AirFixSpillsAfterTerminals.h"
#include "AirHandleCalleeSaves.h"
#include "AirPhaseInsertionSet.h"
#include "AirInstInlines.h"
#include "AirLiveness.h"
#include "AirPadInterference.h"
#include "AirPhaseScope.h"
#include "AirRegLiveness.h"
#include "AirStackAllocation.h"
#include "AirTmpInlines.h"
#include "AirTmpMap.h"
#include <wtf/ListDump.h>
#include <wtf/Range.h>

namespace JSC { namespace B3 { namespace Air {

namespace {

NO_RETURN_DUE_TO_CRASH NEVER_INLINE void crash()
{
    CRASH();
}

#undef RELEASE_ASSERT
#define RELEASE_ASSERT(assertion) do { \
    if (!(assertion)) { \
        WTFReportAssertionFailure(__FILE__, __LINE__, WTF_PRETTY_FUNCTION, #assertion); \
        crash(); \
    } \
} while (0)

bool verbose() { return Options::airLinearScanVerbose(); }

// Phase constants we use for the PhaseInsertionSet.
const unsigned firstPhase = 0;
const unsigned secondPhase = 1;

typedef Range<size_t> Interval;

struct TmpData {
    void dump(PrintStream& out) const
    {
        out.print("{interval = ", interval, ", spilled = ", pointerDump(spilled), ", assigned = ", assigned, ", isUnspillable = ", isUnspillable, ", possibleRegs = ", possibleRegs, ", didBuildPossibleRegs = ", didBuildPossibleRegs, "}");
    }
    
    void validate()
    {
        RELEASE_ASSERT(!(spilled && assigned));
    }
    
    Interval interval;
    StackSlot* spilled { nullptr };
    RegisterSet possibleRegs;
    Reg assigned;
    bool isUnspillable { false };
    bool didBuildPossibleRegs { false };
    unsigned spillIndex { 0 };
};

struct Clobber {
    Clobber()
    {
    }
    
    Clobber(size_t index, RegisterSet regs)
        : index(index)
        , regs(regs)
    {
    }
    
    void dump(PrintStream& out) const
    {
        out.print(index, ":", regs);
    }
    
    size_t index { 0 };
    RegisterSet regs;
};

class LinearScan {
public:
    LinearScan(Code& code)
        : m_code(code)
        , m_startIndex(code.size())
        , m_map(code)
        , m_insertionSets(code.size())
    {
    }
    
    void run()
    {
        padInterference(m_code);
        buildRegisterSet();
        buildIndices();
        buildIntervals();
        if (shouldSpillEverything()) {
            spillEverything();
            emitSpillCode();
        }
        for (;;) {
            prepareIntervalsForScanForRegisters();
            m_didSpill = false;
            forEachBank(
                [&] (Bank bank) {
                    attemptScanForRegisters(bank);
                });
            if (!m_didSpill)
                break;
            emitSpillCode();
        }
        insertSpillCode();
        assignRegisters();
        fixSpillsAfterTerminals(m_code);

        handleCalleeSaves(m_code);
        allocateEscapedStackSlots(m_code);
        prepareIntervalsForScanForStack();
        scanForStack();
        updateFrameSizeBasedOnStackSlots(m_code);
        m_code.setStackIsAllocated(true);
    }
    
private:
    void buildRegisterSet()
    {
        forEachBank(
            [&] (Bank bank) {
                m_registers[bank] = m_code.regsInPriorityOrder(bank);
                m_registerSet[bank].setAll(m_registers[bank]);
                m_unifiedRegisterSet.merge(m_registerSet[bank]);
            });
    }

    void buildIndices()
    {
        size_t index = 0;
        for (BasicBlock* block : m_code) {
            m_startIndex[block] = index;
            index += block->size() * 2;
        }
    }
    
    size_t indexOfHead(BasicBlock* block)
    {
        return m_startIndex[block];
    }
    
    size_t indexOfTail(BasicBlock* block)
    {
        return indexOfHead(block) + block->size() * 2;
    }
    
    Interval interval(size_t indexOfEarly, Arg::Timing timing)
    {
        switch (timing) {
        case Arg::OnlyEarly:
            return Interval(indexOfEarly);
        case Arg::OnlyLate:
            return Interval(indexOfEarly + 1);
        case Arg::EarlyAndLate:
            return Interval(indexOfEarly, indexOfEarly + 2);
        }
        ASSERT_NOT_REACHED();
        return Interval();
    }
    
    void buildIntervals()
    {
        TimingScope timingScope("LinearScan::buildIntervals");
        UnifiedTmpLiveness liveness(m_code);
        
        for (BasicBlock* block : m_code) {
            size_t indexOfHead = this->indexOfHead(block);
            size_t indexOfTail = this->indexOfTail(block);
            if (verbose()) {
                dataLog("At block ", pointerDump(block), "\n");
                dataLog("  indexOfHead = ", indexOfHead, "\n");
                dataLog("  idnexOfTail = ", indexOfTail, "\n");
            }
            for (Tmp tmp : liveness.liveAtHead(block)) {
                if (!tmp.isReg())
                    m_map[tmp].interval |= Interval(indexOfHead);
            }
            for (Tmp tmp : liveness.liveAtTail(block)) {
                if (!tmp.isReg())
                    m_map[tmp].interval |= Interval(indexOfTail);
            }
            
            for (unsigned instIndex = 0; instIndex < block->size(); ++instIndex) {
                Inst& inst = block->at(instIndex);
                size_t indexOfEarly = indexOfHead + instIndex * 2;
                // FIXME: We can get this information from the liveness constraints. Except of
                // course we want to separate the earlies of one instruction from the lates of
                // the next.
                // https://bugs.webkit.org/show_bug.cgi?id=170850
                inst.forEachTmp(
                    [&] (Tmp& tmp, Arg::Role role, Bank, Width) {
                        if (tmp.isReg())
                            return;
                        m_map[tmp].interval |= interval(indexOfEarly, Arg::timing(role));
                    });
            }

            RegLiveness::LocalCalcForUnifiedTmpLiveness localCalc(liveness, block);
            
            auto record = [&] (unsigned instIndex) {
                // FIXME: This could get the register sets from somewhere else, like the
                // liveness constraints. Except we want those constraints to separate the late
                // actions of one instruction from the early actions of the next.
                // https://bugs.webkit.org/show_bug.cgi?id=170850
                const RegisterSet& regs = localCalc.live();
                if (Inst* prev = block->get(instIndex - 1)) {
                    RegisterSet prevRegs = regs;
                    prev->forEach<Reg>(
                        [&] (Reg& reg, Arg::Role role, Bank, Width) {
                            if (Arg::isLateDef(role))
                                prevRegs.add(reg);
                        });
                    if (prev->kind.opcode == Patch)
                        prevRegs.merge(prev->extraClobberedRegs());
                    prevRegs.filter(m_unifiedRegisterSet);
                    if (!prevRegs.isEmpty())
                        m_clobbers.append(Clobber(indexOfHead + instIndex * 2 - 1, prevRegs));
                }
                if (Inst* next = block->get(instIndex)) {
                    RegisterSet nextRegs = regs;
                    next->forEach<Reg>(
                        [&] (Reg& reg, Arg::Role role, Bank, Width) {
                            if (Arg::isEarlyDef(role))
                                nextRegs.add(reg);
                        });
                    if (next->kind.opcode == Patch)
                        nextRegs.merge(next->extraEarlyClobberedRegs());
                    if (!nextRegs.isEmpty())
                        m_clobbers.append(Clobber(indexOfHead + instIndex * 2, nextRegs));
                }
            };
            
            record(block->size());
            for (unsigned instIndex = block->size(); instIndex--;) {
                localCalc.execute(instIndex);
                record(instIndex);
            }
        }
        
        std::sort(
            m_clobbers.begin(), m_clobbers.end(),
            [] (Clobber& a, Clobber& b) -> bool {
                return a.index < b.index;
            });
        
        if (verbose()) {
            dataLog("Intervals:\n");
            m_code.forEachTmp(
                [&] (Tmp tmp) {
                    dataLog("    ", tmp, ": ", m_map[tmp], "\n");
                });
            dataLog("Clobbers: ", listDump(m_clobbers), "\n");
        }
    }
    
    bool shouldSpillEverything()
    {
        if (!Options::airLinearScanSpillsEverything())
            return false;
        
        // You're meant to hack this so that you selectively spill everything depending on reasons.
        // That's super useful for debugging.

        return true;
    }
    
    void spillEverything()
    {
        m_code.forEachTmp(
            [&] (Tmp tmp) {
                spill(tmp);
            });
    }
    
    void prepareIntervalsForScanForRegisters()
    {
        prepareIntervals(
            [&] (TmpData& data) -> bool {
                if (data.spilled)
                    return false;
                
                data.assigned = Reg();
                return true;
            });
    }
    
    void prepareIntervalsForScanForStack()
    {
        prepareIntervals(
            [&] (TmpData& data) -> bool {
                return data.spilled;
            });
    }
    
    template<typename SelectFunc>
    void prepareIntervals(const SelectFunc& selectFunc)
    {
        m_tmps.shrink(0);
        
        m_code.forEachTmp(
            [&] (Tmp tmp) {
                TmpData& data = m_map[tmp];
                if (!selectFunc(data))
                    return;
                
                m_tmps.append(tmp);
            });
        
        std::sort(
            m_tmps.begin(), m_tmps.end(),
            [&] (Tmp& a, Tmp& b) {
                return m_map[a].interval.begin() < m_map[b].interval.begin();
            });
        
        if (verbose())
            dataLog("Tmps: ", listDump(m_tmps), "\n");
    }
    
    Tmp addSpillTmpWithInterval(Bank bank, Interval interval)
    {
        TmpData data;
        data.interval = interval;
        data.isUnspillable = true;
        
        Tmp tmp = m_code.newTmp(bank);
        m_map.append(tmp, data);
        return tmp;
    }
    
    void attemptScanForRegisters(Bank bank)
    {
        // This is modeled after LinearScanRegisterAllocation in Fig. 1 in
        // http://dl.acm.org/citation.cfm?id=330250.

        m_active.clear();
        m_activeRegs = { };
        
        size_t clobberIndex = 0;
        for (Tmp& tmp : m_tmps) {
            if (tmp.bank() != bank)
                continue;
            
            TmpData& entry = m_map[tmp];
            size_t index = entry.interval.begin();
            
            if (verbose()) {
                dataLog("Index #", index, ": ", tmp, "\n");
                dataLog("  ", tmp, ": ", entry, "\n");
                dataLog("  clobberIndex = ", clobberIndex, "\n");
                // This could be so much faster.
                BasicBlock* block = m_code[0];
                for (BasicBlock* candidateBlock : m_code) {
                    if (m_startIndex[candidateBlock] > index)
                        break;
                    block = candidateBlock;
                }
                unsigned instIndex = (index - m_startIndex[block] + 1) / 2;
                dataLog("  At: ", pointerDump(block), ", instIndex = ", instIndex, "\n");
                dataLog("    Prev: ", pointerDump(block->get(instIndex - 1)), "\n");
                dataLog("    Next: ", pointerDump(block->get(instIndex)), "\n");
                dataLog("  Active:\n");
                for (Tmp tmp : m_active)
                    dataLog("    ", tmp, ": ", m_map[tmp], "\n");
            }
            
            // This is ExpireOldIntervals in Fig. 1.
            while (!m_active.isEmpty()) {
                Tmp tmp = m_active.first();
                TmpData& entry = m_map[tmp];
                
                bool expired = entry.interval.end() <= index;
                
                if (!expired)
                    break;
                
                m_active.removeFirst();
                m_activeRegs.remove(entry.assigned);
            }

            // If necessary, compute the set of registers that this tmp could even use. This is not
            // part of Fig. 1, but it's a technique that the authors claim to have implemented in one of
            // their two implementations. There may be other more efficient ways to do this, but this
            // implementation gets some perf wins from piggy-backing this calculation in the scan.
            //
            // Note that the didBuild flag sticks through spilling. Spilling doesn't change the
            // interference situation.
            //
            // Note that we could short-circuit this if we're dealing with a spillable tmp, there are no
            // free registers, and this register's interval ends after the one on the top of the active
            // stack.
            if (!entry.didBuildPossibleRegs) {
                // Advance the clobber index until it's at a clobber that is relevant to us.
                while (clobberIndex < m_clobbers.size() && m_clobbers[clobberIndex].index < index)
                    clobberIndex++;
                
                RegisterSet possibleRegs = m_registerSet[bank];
                for (size_t i = clobberIndex; i < m_clobbers.size() && m_clobbers[i].index < entry.interval.end(); ++i)
                    possibleRegs.exclude(m_clobbers[i].regs);
                
                entry.possibleRegs = possibleRegs;
                entry.didBuildPossibleRegs = true;
            }
            
            if (verbose())
                dataLog("  Possible regs: ", entry.possibleRegs, "\n");
            
            // Find a free register that we are allowed to use.
            if (m_active.size() != m_registers[bank].size()) {
                bool didAssign = false;
                for (Reg reg : m_registers[bank]) {
                    // FIXME: Could do priority coloring here.
                    // https://bugs.webkit.org/show_bug.cgi?id=170304
                    if (!m_activeRegs.contains(reg) && entry.possibleRegs.contains(reg)) {
                        assign(tmp, reg);
                        didAssign = true;
                        break;
                    }
                }
                if (didAssign)
                    continue;
            }
            
            // This is SpillAtInterval in Fig. 1, but modified to handle clobbers.
            Tmp spillTmp = m_active.takeLast(
                [&] (Tmp spillCandidate) -> bool {
                    return entry.possibleRegs.contains(m_map[spillCandidate].assigned);
                });
            if (!spillTmp) {
                spill(tmp);
                continue;
            }
            TmpData& spillEntry = m_map[spillTmp];
            RELEASE_ASSERT(spillEntry.assigned);
            if (spillEntry.isUnspillable ||
                (!entry.isUnspillable && spillEntry.interval.end() <= entry.interval.end())) {
                spill(tmp);
                addToActive(spillTmp);
                continue;
            }
            
            assign(tmp, spillEntry.assigned);
            spill(spillTmp);
        }
    }
    
    void addToActive(Tmp tmp)
    {
        if (m_map[tmp].isUnspillable) {
            m_active.prepend(tmp);
            return;
        }
        
        m_active.appendAndBubble(
            tmp,
            [&] (Tmp otherTmp) -> bool {
                TmpData& otherEntry = m_map[otherTmp];
                if (otherEntry.isUnspillable)
                    return false;
                return m_map[otherTmp].interval.end() > m_map[tmp].interval.end();
            });
    }
    
    void assign(Tmp tmp, Reg reg)
    {
        TmpData& entry = m_map[tmp];
        RELEASE_ASSERT(!entry.spilled);
        entry.assigned = reg;
        m_activeRegs.add(reg);
        addToActive(tmp);
    }
    
    void spill(Tmp tmp)
    {
        TmpData& entry = m_map[tmp];
        RELEASE_ASSERT(!entry.isUnspillable);
        entry.spilled = m_code.addStackSlot(8, StackSlotKind::Spill);
        entry.assigned = Reg();
        m_didSpill = true;
    }
    
    void emitSpillCode()
    {
        for (BasicBlock* block : m_code) {
            size_t indexOfHead = this->indexOfHead(block);
            for (unsigned instIndex = 0; instIndex < block->size(); ++instIndex) {
                Inst& inst = block->at(instIndex);
                unsigned indexOfEarly = indexOfHead + instIndex * 2;
                
                // First try to spill directly.
                for (unsigned i = 0; i < inst.args.size(); ++i) {
                    Arg& arg = inst.args[i];
                    if (!arg.isTmp())
                        continue;
                    if (arg.isReg())
                        continue;
                    StackSlot* spilled = m_map[arg.tmp()].spilled;
                    if (!spilled)
                        continue;
                    if (!inst.admitsStack(i))
                        continue;
                    arg = Arg::stack(spilled);
                }
                
                // Fall back on the hard way.
                inst.forEachTmp(
                    [&] (Tmp& tmp, Arg::Role role, Bank bank, Width) {
                        if (tmp.isReg())
                            return;
                        StackSlot* spilled = m_map[tmp].spilled;
                        if (!spilled)
                            return;
                        Opcode move = bank == GP ? Move : MoveDouble;
                        tmp = addSpillTmpWithInterval(bank, interval(indexOfEarly, Arg::timing(role)));
                        if (role == Arg::Scratch)
                            return;
                        if (Arg::isAnyUse(role))
                            m_insertionSets[block].insert(instIndex, secondPhase, move, inst.origin, Arg::stack(spilled), tmp);
                        if (Arg::isAnyDef(role))
                            m_insertionSets[block].insert(instIndex + 1, firstPhase, move, inst.origin, tmp, Arg::stack(spilled));
                    });
            }
        }
    }
    
    void scanForStack()
    {
        // This is loosely modeled after LinearScanRegisterAllocation in Fig. 1 in
        // http://dl.acm.org/citation.cfm?id=330250.

        m_active.clear();
        m_usedSpillSlots.clearAll();
        
        for (Tmp& tmp : m_tmps) {
            TmpData& entry = m_map[tmp];
            if (!entry.spilled)
                continue;
            
            size_t index = entry.interval.begin();
            
            // This is ExpireOldIntervals in Fig. 1.
            while (!m_active.isEmpty()) {
                Tmp tmp = m_active.first();
                TmpData& entry = m_map[tmp];
                
                bool expired = entry.interval.end() <= index;
                
                if (!expired)
                    break;
                
                m_active.removeFirst();
                m_usedSpillSlots.clear(entry.spillIndex);
            }
            
            entry.spillIndex = m_usedSpillSlots.findBit(0, false);
            ptrdiff_t offset = -static_cast<ptrdiff_t>(m_code.frameSize()) - static_cast<ptrdiff_t>(entry.spillIndex) * 8 - 8;
            if (verbose())
                dataLog("  Assigning offset = ", offset, " to spill ", pointerDump(entry.spilled), " for ", tmp, "\n");
            entry.spilled->setOffsetFromFP(offset);
            m_usedSpillSlots.set(entry.spillIndex);
            m_active.append(tmp);
        }
    }
    
    void insertSpillCode()
    {
        for (BasicBlock* block : m_code)
            m_insertionSets[block].execute(block);
    }
    
    void assignRegisters()
    {
        if (verbose()) {
            dataLog("About to allocate registers. State of all tmps:\n");
            m_code.forEachTmp(
                [&] (Tmp tmp) {
                    dataLog("    ", tmp, ": ", m_map[tmp], "\n");
                });
            dataLog("IR:\n");
            dataLog(m_code);
        }
        
        for (BasicBlock* block : m_code) {
            for (Inst& inst : *block) {
                if (verbose())
                    dataLog("At: ", inst, "\n");
                inst.forEachTmpFast(
                    [&] (Tmp& tmp) {
                        if (tmp.isReg())
                            return;
                        
                        Reg reg = m_map[tmp].assigned;
                        if (!reg) {
                            dataLog("Failed to allocate reg for: ", tmp, "\n");
                            RELEASE_ASSERT_NOT_REACHED();
                        }
                        tmp = Tmp(reg);
                    });
            }
        }
    }
    
    Code& m_code;
    Vector<Reg> m_registers[numBanks];
    RegisterSet m_registerSet[numBanks];
    RegisterSet m_unifiedRegisterSet;
    IndexMap<BasicBlock*, size_t> m_startIndex;
    TmpMap<TmpData> m_map;
    IndexMap<BasicBlock*, PhaseInsertionSet> m_insertionSets;
    Vector<Clobber> m_clobbers; // After we allocate this, we happily point pointers into it.
    Vector<Tmp> m_tmps;
    Deque<Tmp> m_active;
    RegisterSet m_activeRegs;
    BitVector m_usedSpillSlots;
    bool m_didSpill { false };
};

} // anonymous namespace

void allocateRegistersAndStackByLinearScan(Code& code)
{
    PhaseScope phaseScope(code, "allocateRegistersAndStackByLinearScan");
    if (verbose())
        dataLog("Air before linear scan:\n", code);
    LinearScan linearScan(code);
    linearScan.run();
    if (verbose())
        dataLog("Air after linear scan:\n", code);
}

} } } // namespace JSC::B3::Air

#endif // ENABLE(B3_JIT)
