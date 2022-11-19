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

#pragma once

#if ENABLE(B3_JIT)

#include "AirArgInlines.h"
#include "AirBlockWorklist.h"
#include "AirCode.h"
#include "AirInstInlines.h"
#include "AirTmpInlines.h"
#include <wtf/CommaPrinter.h>
#include <wtf/Vector.h>

namespace JSC { namespace B3 { namespace Air {

class Code;

// Computes the number of uses of a tmp based on frequency of execution. The frequency of blocks
// that are only reachable by rare edges is scaled by Options::rareBlockPenalty().
class UseCounts {
public:
    UseCounts(Code& code)
    {
        // Find non-rare blocks.
        BlockWorklist fastWorklist;
        fastWorklist.push(code[0]);
        while (BasicBlock* block = fastWorklist.pop()) {
            for (FrequentedBlock& successor : block->successors()) {
                if (!successor.isRare())
                    fastWorklist.push(successor.block());
            }
        }

        unsigned gpArraySize = AbsoluteTmpMapper<GP>::absoluteIndex(code.numTmps(GP));
        m_gpNumWarmUsesAndDefs = FixedVector<float>(gpArraySize);
        m_gpNumWarmUsesAndDefs.fill(0);
        m_gpConstDefs.ensureSize(gpArraySize);
        unsigned fpArraySize = AbsoluteTmpMapper<FP>::absoluteIndex(code.numTmps(FP));
        m_fpNumWarmUsesAndDefs = FixedVector<float>(fpArraySize);
        m_fpNumWarmUsesAndDefs.fill(0);
        m_fpConstDefs.ensureSize(fpArraySize);

        for (BasicBlock* block : code) {
            double frequency = block->frequency();
            if (!fastWorklist.saw(block))
                frequency *= Options::rareBlockPenalty();
            for (Inst& inst : *block) {
                inst.forEach<Tmp>(
                    [&] (Tmp& tmp, Arg::Role role, Bank bank, Width) {

                        if (Arg::isWarmUse(role) || Arg::isAnyDef(role)) {
                            if (bank == GP)
                                m_gpNumWarmUsesAndDefs[AbsoluteTmpMapper<GP>::absoluteIndex(tmp)] += frequency;
                            else
                                m_fpNumWarmUsesAndDefs[AbsoluteTmpMapper<FP>::absoluteIndex(tmp)] += frequency;
                        }
                    });

                if ((inst.kind.opcode == Move || inst.kind.opcode == Move32)
                    && inst.args[0].isSomeImm()
                    && inst.args[1].is<Tmp>()) {
                    Tmp tmp = inst.args[1].as<Tmp>();
                    if (tmp.bank() == GP)
                        m_gpConstDefs.quickSet(AbsoluteTmpMapper<GP>::absoluteIndex(tmp));
                    else
                        m_fpConstDefs.quickSet(AbsoluteTmpMapper<FP>::absoluteIndex(tmp));
                }
            }
        }
    }

    template<Bank bank>
    bool isConstDef(unsigned absoluteIndex) const
    {
        if (bank == GP)
            return m_gpConstDefs.quickGet(absoluteIndex);
        return m_fpConstDefs.quickGet(absoluteIndex);
    }

    template<Bank bank>
    float numWarmUsesAndDefs(unsigned absoluteIndex) const
    {
        if (bank == GP)
            return m_gpNumWarmUsesAndDefs[absoluteIndex];
        return m_fpNumWarmUsesAndDefs[absoluteIndex];
    }

    void dump(PrintStream& out) const
    {
        CommaPrinter comma(", ");
        for (unsigned i = 0; i < m_gpNumWarmUsesAndDefs.size(); ++i)
            out.print(comma, AbsoluteTmpMapper<GP>::tmpFromAbsoluteIndex(i), "=> {numWarmUsesAndDefs=", m_gpNumWarmUsesAndDefs[i], ", isConstDef=", m_gpConstDefs.quickGet(i), "}");
        for (unsigned i = 0; i < m_fpNumWarmUsesAndDefs.size(); ++i)
            out.print(comma, AbsoluteTmpMapper<FP>::tmpFromAbsoluteIndex(i), "=> {numWarmUsesAndDefs=", m_fpNumWarmUsesAndDefs[i], ", isConstDef=", m_fpConstDefs.quickGet(i), "}");
    }

private:
    FixedVector<float> m_gpNumWarmUsesAndDefs;
    FixedVector<float> m_fpNumWarmUsesAndDefs;
    BitVector m_gpConstDefs;
    BitVector m_fpConstDefs;
};

} } } // namespace JSC::B3::Air

#endif // ENABLE(B3_JIT)
