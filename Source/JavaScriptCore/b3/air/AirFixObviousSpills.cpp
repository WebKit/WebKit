/*
 * Copyright (C) 2016-2019 Apple Inc. All rights reserved.
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
#include "AirFixObviousSpills.h"

#if ENABLE(B3_JIT)

#include "AirArgInlines.h"
#include "AirCode.h"
#include "AirInstInlines.h"
#include "AirPhaseScope.h"
#include <wtf/IndexMap.h>
#include <wtf/ListDump.h>

namespace JSC { namespace B3 { namespace Air {

namespace {

namespace AirFixObviousSpillsInternal {
static constexpr bool verbose = false;
}

class FixObviousSpills {
public:
    FixObviousSpills(Code& code)
        : m_code(code)
        , m_atHead(code.size())
        , m_notBottom(code.size())
        , m_shouldVisit(code.size())
    {
    }

    void run()
    {
        if (AirFixObviousSpillsInternal::verbose)
            dataLog("Code before fixObviousSpills:\n", m_code);
        
        computeAliases();
        fixCode();
    }

private:
    void computeAliases()
    {
        m_notBottom.quickSet(0);
        m_shouldVisit.quickSet(0);
        
        bool changed = true;
        while (changed) {
            changed = false;
            
            for (unsigned blockIndex : m_shouldVisit) {
                m_shouldVisit.quickClear(blockIndex);
                BasicBlock* block = m_code[blockIndex];
                ASSERT(m_notBottom.quickGet(blockIndex));
                m_block = block;
                m_state = m_atHead[block];

                if (AirFixObviousSpillsInternal::verbose)
                    dataLog("Executing block ", *m_block, ": ", m_state, "\n");
                
                for (m_instIndex = 0; m_instIndex < block->size(); ++m_instIndex)
                    executeInst();

                // Before we call merge we must make sure that the two states are sorted.
                m_state.sort();

                for (BasicBlock* successor : block->successorBlocks()) {
                    unsigned successorIndex = successor->index();
                    State& toState = m_atHead[successor];
                    if (m_notBottom.quickGet(successorIndex)) {
                        bool changedAtSuccessorHead = toState.merge(m_state);
                        if (changedAtSuccessorHead) {
                            changed = true;
                            m_shouldVisit.quickSet(successorIndex);
                        }
                    } else { // The state at head of successor is bottom
                        toState = m_state;
                        changed = true;
                        m_notBottom.quickSet(successorIndex);
                        m_shouldVisit.quickSet(successorIndex);
                    }
                }
            }
        }
    }

    void fixCode()
    {
        for (BasicBlock* block : m_code) {
            m_block = block;
            m_state = m_atHead[block];
            RELEASE_ASSERT(m_notBottom.quickGet(block->index()));

            for (m_instIndex = 0; m_instIndex < block->size(); ++m_instIndex) {
                fixInst();
                executeInst();
            }
        }
    }
    
    template<typename Func>
    void forAllAliases(const Func& func)
    {
        Inst& inst = m_block->at(m_instIndex);

        switch (inst.kind.opcode) {
        case Move:
            if (inst.args[0].isSomeImm()) {
                if (inst.args[1].isReg())
                    func(RegConst(inst.args[1].reg(), inst.args[0].value()));
                else if (isSpillSlot(inst.args[1]))
                    func(SlotConst(inst.args[1].stackSlot(), inst.args[0].value()));
            } else if (isSpillSlot(inst.args[0]) && inst.args[1].isReg()) {
                if (std::optional<int64_t> constant = m_state.constantFor(inst.args[0]))
                    func(RegConst(inst.args[1].reg(), *constant));
                func(RegSlot(inst.args[1].reg(), inst.args[0].stackSlot(), RegSlot::AllBits));
            } else if (inst.args[0].isReg() && isSpillSlot(inst.args[1])) {
                if (std::optional<int64_t> constant = m_state.constantFor(inst.args[0]))
                    func(SlotConst(inst.args[1].stackSlot(), *constant));
                func(RegSlot(inst.args[0].reg(), inst.args[1].stackSlot(), RegSlot::AllBits));
            }
            break;

        case Move32:
            if (inst.args[0].isSomeImm()) {
                if (inst.args[1].isReg())
                    func(RegConst(inst.args[1].reg(), static_cast<uint32_t>(inst.args[0].value())));
                else if (isSpillSlot(inst.args[1]))
                    func(SlotConst(inst.args[1].stackSlot(), static_cast<uint32_t>(inst.args[0].value())));
            } else if (isSpillSlot(inst.args[0]) && inst.args[1].isReg()) {
                if (std::optional<int64_t> constant = m_state.constantFor(inst.args[0]))
                    func(RegConst(inst.args[1].reg(), static_cast<uint32_t>(*constant)));
                func(RegSlot(inst.args[1].reg(), inst.args[0].stackSlot(), RegSlot::ZExt32));
            } else if (inst.args[0].isReg() && isSpillSlot(inst.args[1])) {
                if (std::optional<int64_t> constant = m_state.constantFor(inst.args[0]))
                    func(SlotConst(inst.args[1].stackSlot(), static_cast<int32_t>(*constant)));
                func(RegSlot(inst.args[0].reg(), inst.args[1].stackSlot(), RegSlot::Match32));
            }
            break;

        case MoveFloat:
            if (isSpillSlot(inst.args[0]) && inst.args[1].isReg())
                func(RegSlot(inst.args[1].reg(), inst.args[0].stackSlot(), RegSlot::Match32));
            else if (inst.args[0].isReg() && isSpillSlot(inst.args[1]))
                func(RegSlot(inst.args[0].reg(), inst.args[1].stackSlot(), RegSlot::Match32));
            break;

        case MoveDouble:
            if (isSpillSlot(inst.args[0]) && inst.args[1].isReg())
                func(RegSlot(inst.args[1].reg(), inst.args[0].stackSlot(), RegSlot::AllBits));
            else if (inst.args[0].isReg() && isSpillSlot(inst.args[1]))
                func(RegSlot(inst.args[0].reg(), inst.args[1].stackSlot(), RegSlot::AllBits));
            break;
            
        default:
            break;
        }
    }

    void executeInst()
    {
        Inst& inst = m_block->at(m_instIndex);

        if (AirFixObviousSpillsInternal::verbose)
            dataLog("    Executing ", inst, ": ", m_state, "\n");

        Inst::forEachDefWithExtraClobberedRegs<Reg>(&inst, &inst,
            [&] (const Reg& reg, Arg::Role, Bank, Width, PreservedWidth) {
                if (AirFixObviousSpillsInternal::verbose)
                    dataLog("        Clobbering ", reg, "\n");
                m_state.clobber(reg);
            });

        Inst::forEachDef<StackSlot*>(&inst, &inst,
            [&] (StackSlot* slot, Arg::Role, Bank, Width) {
                if (AirFixObviousSpillsInternal::verbose)
                    dataLog("        Clobbering ", *slot, "\n");
                m_state.clobber(slot);
            });

        forAllAliases(
            [&] (const auto& alias) {
                m_state.addAlias(alias);
            });
    }

    void fixInst()
    {
        Inst& inst = m_block->at(m_instIndex);

        if (AirFixObviousSpillsInternal::verbose)
            dataLog("Fixing inst ", inst, ": ", m_state, "\n");
        
        // Check if alias analysis says that this is unnecessary.
        bool shouldLive = true;
        forAllAliases(
            [&] (const auto& alias) {
                shouldLive &= !m_state.contains(alias);
            });
        if (!shouldLive) {
            inst = Inst();
            return;
        }
        
        // First handle some special instructions.
        switch (inst.kind.opcode) {
        case Move: {
            if (inst.args[0].isBigImm() && inst.args[1].isReg()
                && isValidForm(Add64, Arg::Imm, Arg::Tmp, Arg::Tmp)) {
                // BigImm materializations are super expensive on both x86 and ARM. Let's try to
                // materialize this bad boy using math instead. Note that we use unsigned math here
                // since it's more deterministic.
                uint64_t myValue = inst.args[0].value();
                Reg myDest = inst.args[1].reg();
                for (const RegConst& regConst : m_state.regConst) {
                    uint64_t otherValue = regConst.constant;
                    
                    // Let's try add. That's the only thing that works on all platforms, since it's
                    // the only cheap arithmetic op that x86 does in three operands. Long term, we
                    // should add fancier materializations here for ARM if the BigImm is yuge.
                    uint64_t delta = myValue - otherValue;
                    
                    if (Arg::isValidImmForm(delta)) {
                        inst.kind = Add64;
                        inst.args.resize(3);
                        inst.args[0] = Arg::imm(delta);
                        inst.args[1] = Tmp(regConst.reg);
                        inst.args[2] = Tmp(myDest);
                        return;
                    }
                }
                return;
            }
            break;
        }
            
        default:
            break;
        }
        
        // FIXME: This code should be taught how to simplify the spill-to-spill move
        // instruction. Basically it needs to know to remove the scratch arg.
        // https://bugs.webkit.org/show_bug.cgi?id=171133

        // Create a copy in case we invalidate the instruction. That doesn't happen often.
        Inst instCopy = inst;

        // The goal is to replace references to stack slots. We only care about early uses. We can't
        // handle UseDefs. We could teach this to handle UseDefs if we inserted a store instruction
        // after and we proved that the register aliased to the stack slot dies here. We can get that
        // information from the liveness analysis. We also can't handle late uses, because we don't
        // look at late clobbers when doing this.
        bool didThings = false;
        auto handleArg = [&] (Arg& arg, Arg::Role role, Bank, Width width) {
            if (!isSpillSlot(arg))
                return;
            if (!Arg::isEarlyUse(role))
                return;
            if (Arg::isAnyDef(role))
                return;
            
            // Try to get a register if at all possible.
            if (const RegSlot* alias = m_state.getRegSlot(arg.stackSlot())) {
                switch (width) {
                case Width64:
                    if (alias->mode != RegSlot::AllBits)
                        return;
                    if (AirFixObviousSpillsInternal::verbose)
                        dataLog("    Replacing ", arg, " with ", alias->reg, "\n");
                    arg = Tmp(alias->reg);
                    didThings = true;
                    return;
                case Width32:
                    if (AirFixObviousSpillsInternal::verbose)
                        dataLog("    Replacing ", arg, " with ", alias->reg, " (subwidth case)\n");
                    arg = Tmp(alias->reg);
                    didThings = true;
                    return;
                default:
                    return;
                }
            }

            // Revert to immediate if that didn't work.
            if (const SlotConst* alias = m_state.getSlotConst(arg.stackSlot())) {
                if (AirFixObviousSpillsInternal::verbose)
                    dataLog("    Replacing ", arg, " with constant ", alias->constant, "\n");
                if (Arg::isValidImmForm(alias->constant))
                    arg = Arg::imm(alias->constant);
                else
                    arg = Arg::bigImm(alias->constant);
                didThings = true;
                return;
            }
        };
        
        inst.forEachArg(handleArg);
        if (!didThings || inst.isValidForm())
            return;
        
        // We introduced something invalid along the way. Back up and carefully handle each argument.
        inst = instCopy;
        ASSERT(inst.isValidForm());
        inst.forEachArg(
            [&] (Arg& arg, Arg::Role role, Bank bank, Width width) {
                Arg argCopy = arg;
                handleArg(arg, role, bank, width);
                if (!inst.isValidForm())
                    arg = argCopy;
            });
    }
    
    static bool isSpillSlot(const Arg& arg)
    {
        return arg.isStack() && arg.stackSlot()->isSpill();
    }
    
    struct RegConst {
        RegConst()
        {
        }
        
        RegConst(Reg reg, int64_t constant)
            : reg(reg)
            , constant(constant)
        {
        }

        explicit operator bool() const
        {
            return !!reg;
        }
        
        bool operator==(const RegConst& other) const
        {
            return reg == other.reg
                && constant == other.constant;
        }

        bool operator<(const RegConst& other) const
        {
            return reg < other.reg || (reg == other.reg && constant < other.constant);
        }

        void dump(PrintStream& out) const
        {
            out.print(reg, "->", constant);
        }
        
        Reg reg;
        int64_t constant { 0 };
    };

    struct RegSlot {
        enum Mode : int8_t {
            AllBits,
            ZExt32, // Register contains zero-extended contents of stack slot.
            Match32 // Low 32 bits of register match low 32 bits of stack slot.
        };
        
        RegSlot()
        {
        }

        RegSlot(Reg reg, StackSlot* slot, Mode mode)
            : slot(slot)
            , reg(reg)
            , mode(mode)
        {
        }

        explicit operator bool() const
        {
            return slot && reg;
        }
        
        bool operator==(const RegSlot& other) const
        {
            return slot == other.slot
                && reg == other.reg
                && mode == other.mode;
        }

        bool operator<(const RegSlot& other) const
        {
            // We ignore `mode` on purpose, see merge() for how we deal with it.
            return slot < other.slot || (slot == other.slot && reg < other.reg);
        }

        void dump(PrintStream& out) const
        {
            out.print(pointerDump(slot), "->", reg);
            switch (mode) {
            case AllBits:
                out.print("(AllBits)");
                break;
            case ZExt32:
                out.print("(ZExt32)");
                break;
            case Match32:
                out.print("(Match32)");
                break;
            }
        }
        
        StackSlot* slot { nullptr };
        Reg reg;
        Mode mode { AllBits };
    };

    struct SlotConst {
        SlotConst()
        {
        }

        SlotConst(StackSlot* slot, int64_t constant)
            : slot(slot)
            , constant(constant)
        {
        }

        explicit operator bool() const
        {
            return slot;
        }
        
        bool operator==(const SlotConst& other) const
        {
            return slot == other.slot
                && constant == other.constant;
        }

        bool operator<(const SlotConst& other) const
        {
            return slot < other.slot || (slot == other.slot && constant < other.constant);
        }

        void dump(PrintStream& out) const
        {
            out.print(pointerDump(slot), "->", constant);
        }
        
        StackSlot* slot { nullptr };
        int64_t constant { 0 };
    };

    struct State {
        void addAlias(const RegConst& newAlias)
        {
            regConst.append(newAlias);
#if ASSERT_ENABLED
            m_isSorted = false;
#endif
        }
        void addAlias(const RegSlot& newAlias)
        {
            regSlot.append(newAlias);
#if ASSERT_ENABLED
            m_isSorted = false;
#endif
        }
        void addAlias(const SlotConst& newAlias)
        {
            slotConst.append(newAlias);
#if ASSERT_ENABLED
            m_isSorted = false;
#endif
        }
        
        bool contains(const RegConst& alias)
        {
            return regConst.contains(alias);
        }
        bool contains(const RegSlot& alias)
        {
            return regSlot.contains(alias);
        }
        bool contains(const SlotConst& alias)
        {
            return slotConst.contains(alias);
        }

        const RegConst* getRegConst(Reg reg) const
        {
            for (const RegConst& alias : regConst) {
                if (alias.reg == reg)
                    return &alias;
            }
            return nullptr;
        }

        const RegSlot* getRegSlot(Reg reg) const
        {
            for (const RegSlot& alias : regSlot) {
                if (alias.reg == reg)
                    return &alias;
            }
            return nullptr;
        }

        const RegSlot* getRegSlot(StackSlot* slot) const
        {
            for (const RegSlot& alias : regSlot) {
                if (alias.slot == slot)
                    return &alias;
            }
            return nullptr;
        }

        const RegSlot* getRegSlot(Reg reg, StackSlot* slot) const
        {
            for (const RegSlot& alias : regSlot) {
                if (alias.reg == reg && alias.slot == slot)
                    return &alias;
            }
            return nullptr;
        }

        const SlotConst* getSlotConst(StackSlot* slot) const
        {
            for (const SlotConst& alias : slotConst) {
                if (alias.slot == slot)
                    return &alias;
            }
            return nullptr;
        }

        std::optional<int64_t> constantFor(const Arg& arg)
        {
            if (arg.isReg()) {
                if (const RegConst* alias = getRegConst(arg.reg()))
                    return alias->constant;
                return std::nullopt;
            }
            if (arg.isStack()) {
                if (const SlotConst* alias = getSlotConst(arg.stackSlot()))
                    return alias->constant;
                return std::nullopt;
            }
            return std::nullopt;
        }

        void clobber(const Reg& reg)
        {
            regConst.removeAllMatching(
                [&] (const RegConst& alias) -> bool {
                    return alias.reg == reg;
                });
            regSlot.removeAllMatching(
                [&] (const RegSlot& alias) -> bool {
                    return alias.reg == reg;
                });
        }

        void clobber(StackSlot* slot)
        {
            slotConst.removeAllMatching(
                [&] (const SlotConst& alias) -> bool {
                    return alias.slot == slot;
                });
            regSlot.removeAllMatching(
                [&] (const RegSlot& alias) -> bool {
                    return alias.slot == slot;
                });
        }

        void sort()
        {
            std::sort(regConst.begin(), regConst.end(), [] (const RegConst& a, const RegConst& b) {
                return a < b;
            });
            std::sort(slotConst.begin(), slotConst.end(), [] (const SlotConst& a, const SlotConst& b) {
                return a < b;
            });
            std::sort(regSlot.begin(), regSlot.end(), [] (const RegSlot& a, const RegSlot& b) {
                return a < b;
            });
#if ASSERT_ENABLED
            m_isSorted = true;
#endif
        }

        // Takes two sorted vectors, for each element in the first, it looks for the first element in the second which is not smaller.
        // If such an element exist, call f on both the element from the first vector and this element.
        // Remove the element from the first vector unless f returned true (so f says whether to keep the element)
        // Returns true if any element has been removed.
        template<typename T, typename Func>
        static bool filterVectorAgainst(Vector<T>& own, const Vector<T>& other, Func f)
        {
            const T* it = other.begin();
            const T* end = other.end();
            return !!own.removeAllMatching(
                [&] (T& alias) {
                    it = std::find_if_not(it, end, [&] (const T& otherAlias) {
                        return otherAlias < alias;
                    });
                    if (it == end)
                        return true;
                    return !f(alias, *it);
            });
        }

        bool merge(const State& other)
        {
            ASSERT(m_isSorted);
            ASSERT(other.m_isSorted);
            bool changed = false;

            changed |= filterVectorAgainst(regConst, other.regConst, [](RegConst& a, const RegConst& b) { return a == b; });
            changed |= filterVectorAgainst(slotConst, other.slotConst, [](SlotConst& a, const SlotConst& b) { return a == b; });
            changed |= filterVectorAgainst(regSlot, other.regSlot, [&](RegSlot& alias, const RegSlot& otherAlias) {
                if (alias.reg != otherAlias.reg || alias.slot != otherAlias.slot)
                    return false;
                if (alias.mode != RegSlot::Match32 && alias.mode != otherAlias.mode) {
                    alias.mode = RegSlot::Match32;
                    changed = true;
                }
                return true;
            });

            return changed;
        }

        void dump(PrintStream& out) const
        {
            out.print(
                "{regConst = [", listDump(regConst), "], slotConst = [", listDump(slotConst),
                "], regSlot = [", listDump(regSlot), "]}");
        }

        Vector<RegConst> regConst;
        Vector<SlotConst> slotConst;
        Vector<RegSlot> regSlot;
#if ASSERT_ENABLED
        bool m_isSorted { true };
#endif
    };

    Code& m_code;
    IndexMap<BasicBlock*, State> m_atHead;
    State m_state;
    BitVector m_notBottom;
    BitVector m_shouldVisit;
    BasicBlock* m_block { nullptr };
    unsigned m_instIndex { 0 };
};

} // anonymous namespace

void fixObviousSpills(Code& code)
{
    PhaseScope phaseScope(code, "fixObviousSpills");

    FixObviousSpills fixObviousSpills(code);
    fixObviousSpills.run();
}

} } } // namespace JSC::B3::Air

#endif // ENABLE(B3_JIT)

