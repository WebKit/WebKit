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
#include <wtf/Liveness.h>

namespace JSC { namespace B3 { namespace Air {

template<typename Adapter>
struct LivenessAdapter {
    typedef Air::CFG CFG;
    
    LivenessAdapter(Code& code)
        : code(code)
    {
    }
    
    unsigned blockSize(BasicBlock* block)
    {
        return block->size();
    }
    
    template<typename Func>
    void forEachUse(BasicBlock* block, size_t instBoundaryIndex, const Func& func)
    {
        if (Inst* prevInst = block->get(instBoundaryIndex - 1)) {
            prevInst->forEach<typename Adapter::Thing>(
                [&] (typename Adapter::Thing& thing, Arg::Role role, Bank bank, Width) {
                    if (Arg::isLateUse(role)
                        && Adapter::acceptsBank(bank)
                        && Adapter::acceptsRole(role))
                        func(Adapter::valueToIndex(thing));
                });
        }
        
        if (Inst* nextInst = block->get(instBoundaryIndex)) {
            nextInst->forEach<typename Adapter::Thing>(
                [&] (typename Adapter::Thing& thing, Arg::Role role, Bank bank, Width) {
                    if (Arg::isEarlyUse(role)
                        && Adapter::acceptsBank(bank)
                        && Adapter::acceptsRole(role))
                        func(Adapter::valueToIndex(thing));
                });
        }
    }
    
    template<typename Func>
    void forEachDef(BasicBlock* block, size_t instBoundaryIndex, const Func& func)
    {
        if (Inst* prevInst = block->get(instBoundaryIndex - 1)) {
            prevInst->forEach<typename Adapter::Thing>(
                [&] (typename Adapter::Thing& thing, Arg::Role role, Bank bank, Width) {
                    if (Arg::isLateDef(role)
                        && Adapter::acceptsBank(bank)
                        && Adapter::acceptsRole(role))
                        func(Adapter::valueToIndex(thing));
                });
        }
        
        if (Inst* nextInst = block->get(instBoundaryIndex)) {
            nextInst->forEach<typename Adapter::Thing>(
                [&] (typename Adapter::Thing& thing, Arg::Role role, Bank bank, Width) {
                    if (Arg::isEarlyDef(role)
                        && Adapter::acceptsBank(bank)
                        && Adapter::acceptsRole(role))
                        func(Adapter::valueToIndex(thing));
                });
        }
    }

    Code& code;
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
        unsigned numTmps = Base::code.numTmps(adapterBank);
        return AbsoluteTmpMapper<adapterBank>::absoluteIndex(numTmps);
    }
    static bool acceptsBank(Bank bank) { return bank == adapterBank; }
    static bool acceptsRole(Arg::Role role) { return Arg::temperature(role) >= minimumTemperature; }
    static unsigned valueToIndex(Tmp tmp) { return AbsoluteTmpMapper<adapterBank>::absoluteIndex(tmp); }
    static Tmp indexToValue(unsigned index) { return AbsoluteTmpMapper<adapterBank>::tmpFromAbsoluteIndex(index); }
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

template<typename Adapter>
class Liveness : public WTF::Liveness<Adapter> {
public:
    Liveness(Code& code)
        : WTF::Liveness<Adapter>(code.cfg(), code)
    {
    }
};

template<Bank bank, Arg::Temperature minimumTemperature = Arg::Cold>
using TmpLiveness = Liveness<TmpLivenessAdapter<bank, minimumTemperature>>;

typedef Liveness<TmpLivenessAdapter<GP>> GPLiveness;
typedef Liveness<TmpLivenessAdapter<FP>> FPLiveness;
typedef Liveness<StackSlotLivenessAdapter> StackSlotLiveness;

} } } // namespace JSC::B3::Air

#endif // ENABLE(B3_JIT)
