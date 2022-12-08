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

#pragma once

#if ENABLE(B3_JIT)

#include "AirBasicBlock.h"
#include "AirCFG.h"
#include "AirCode.h"
#include "AirInstInlines.h"
#include "AirStackSlot.h"
#include "AirTmpInlines.h"
#include <wtf/IndexMap.h>

namespace JSC { namespace B3 { namespace Air {

namespace AirLivenessAdapterInternal {
static constexpr bool verbose = false;
}

template<typename Adapter>
struct LivenessAdapter {
    typedef Air::CFG CFG;

    typedef Vector<unsigned, 4> ActionsList;

    struct Actions {
        Actions() { }

        ActionsList use;
        ActionsList def;
    };

    typedef Vector<Actions, 0, UnsafeVectorOverflow> ActionsForBoundary;

    LivenessAdapter(Code& code)
        : code(code)
        , actions(code.size())
    {
    }

    Adapter& adapter()
    {
        return *static_cast<Adapter*>(this);
    }

    void prepareToCompute()
    {
        dataLogLnIf(AirLivenessAdapterInternal::verbose, "Prepare to compute tmp or stack slot liveness for code: ", code);
        for (BasicBlock* block : code) {
            ActionsForBoundary& actionsForBoundary = actions[block];
            actionsForBoundary.resize(block->size() + 1);

            for (size_t instIndex = block->size(); instIndex--;) {
                Inst& inst = block->at(instIndex);
                inst.forEach<typename Adapter::Thing>(
                    [&] (typename Adapter::Thing& thing, Arg::Role role, Bank bank, Width) {
                        if (!Adapter::acceptsBank(bank) || !Adapter::acceptsRole(role))
                            return;

                        unsigned index = adapter().valueToIndex(thing);

                        if (Arg::isEarlyUse(role))
                            actionsForBoundary[instIndex].use.appendIfNotContains(index);
                        if (Arg::isEarlyDef(role))
                            actionsForBoundary[instIndex].def.appendIfNotContains(index);
                        if (Arg::isLateUse(role))
                            actionsForBoundary[instIndex + 1].use.appendIfNotContains(index);
                        if (Arg::isLateDef(role))
                            actionsForBoundary[instIndex + 1].def.appendIfNotContains(index);
                    });
            }
        }

        if (AirLivenessAdapterInternal::verbose) {
            for (size_t blockIndex = code.size(); blockIndex--;) {
                BasicBlock* block = code[blockIndex];
                if (!block)
                    continue;
                ActionsForBoundary& actionsForBoundary = actions[block];
                dataLogLn("Block ", blockIndex);

                dataLogLn("(null) | use: ", listDump(actionsForBoundary[block->size()].use),
                        " def: ", listDump(actionsForBoundary[block->size()].def));
                for (size_t instIndex = block->size(); instIndex--;) {
                    dataLogLn(block->at(instIndex), " | use: ", listDump(actionsForBoundary[instIndex].use),
                        " def: ", listDump(actionsForBoundary[instIndex].def));
                }
                dataLogLn(block->at(0), " | use: ", listDump(actionsForBoundary[0].use),
                        " def: ", listDump(actionsForBoundary[0].def));
            }
        }
    }

    Actions& actionsAt(BasicBlock* block, unsigned instBoundaryIndex)
    {
        return actions[block][instBoundaryIndex];
    }

    unsigned blockSize(BasicBlock* block)
    {
        return block->size();
    }

    template<typename Func>
    void forEachUse(BasicBlock* block, size_t instBoundaryIndex, const Func& func)
    {
        for (unsigned index : actionsAt(block, instBoundaryIndex).use)
            func(index);
    }

    template<typename Func>
    void forEachDef(BasicBlock* block, size_t instBoundaryIndex, const Func& func)
    {
        for (unsigned index : actionsAt(block, instBoundaryIndex).def)
            func(index);
    }

    Code& code;
    IndexMap<BasicBlock*, ActionsForBoundary> actions;
};

template<Bank adapterBank, Arg::Temperature minimumTemperature = Arg::Cold>
struct TmpLivenessAdapter : LivenessAdapter<TmpLivenessAdapter<adapterBank, minimumTemperature>> {
    typedef LivenessAdapter<TmpLivenessAdapter<adapterBank, minimumTemperature>> Base;

    static constexpr const char* name = "TmpLiveness";
    typedef Tmp Thing;

    TmpLivenessAdapter(Code& code)
        : Base(code)
    {
    }

    unsigned numIndices()
    {
        return Tmp::absoluteIndexEnd(Base::code, adapterBank);
    }
    static bool acceptsBank(Bank bank) { return bank == adapterBank; }
    static bool acceptsRole(Arg::Role role) { return Arg::temperature(role) >= minimumTemperature; }
    static unsigned valueToIndex(Tmp tmp) { return AbsoluteTmpMapper<adapterBank>::absoluteIndex(tmp); }
    static Tmp indexToValue(unsigned index) { return AbsoluteTmpMapper<adapterBank>::tmpFromAbsoluteIndex(index); }
};

struct UnifiedTmpLivenessAdapter : LivenessAdapter<UnifiedTmpLivenessAdapter> {
    typedef LivenessAdapter<UnifiedTmpLivenessAdapter> Base;

    static constexpr const char* name = "UnifiedTmpLiveness";

    typedef Tmp Thing;

    UnifiedTmpLivenessAdapter(Code& code)
        : Base(code)
    {
    }

    unsigned numIndices()
    {
        return Tmp::linearIndexEnd(code);
    }

    static bool acceptsBank(Bank) { return true; }
    static bool acceptsRole(Arg::Role) { return true; }
    unsigned valueToIndex(Tmp tmp) { return tmp.linearlyIndexed(code).index(); }
    Tmp indexToValue(unsigned index) { return Tmp::tmpForLinearIndex(code, index); }
};

struct StackSlotLivenessAdapter : LivenessAdapter<StackSlotLivenessAdapter> {
    static constexpr const char* name = "StackSlotLiveness";
    typedef StackSlot* Thing;

    StackSlotLivenessAdapter(Code& code)
        : LivenessAdapter(code)
    {
    }

    unsigned numIndices()
    {
        return code.stackSlots().size();
    }
    static bool acceptsBank(Bank) { return true; }
    static bool acceptsRole(Arg::Role) { return true; }
    static unsigned valueToIndex(StackSlot* stackSlot) { return stackSlot->index(); }
    StackSlot* indexToValue(unsigned index) { return code.stackSlots()[index]; }
};

} } } // namespace JSC::B3::Air

#endif // ENABLE(B3_JIT)

