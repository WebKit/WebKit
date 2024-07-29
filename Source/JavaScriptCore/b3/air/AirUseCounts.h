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
        m_gpNumWarmUsesAndDefs = FixedVector<float>(gpArraySize, 0);
        m_gpConstDefs.ensureSize(gpArraySize);
        BitVector gpNonConstDefs = m_gpConstDefs;
        m_gpConstants = FixedVector<int64_t>(gpArraySize, 0);

        unsigned fpArraySize = AbsoluteTmpMapper<FP>::absoluteIndex(code.numTmps(FP));
        m_fpNumWarmUsesAndDefs = FixedVector<float>(fpArraySize, 0);
        m_fpConstDefs.ensureSize(fpArraySize);
        BitVector fpNonConstDefs = m_fpConstDefs;

        for (BasicBlock* block : code) {
            double frequency = block->frequency();
            if (!fastWorklist.saw(block))
                frequency *= Options::rareBlockPenalty();
            for (Inst& inst : *block) {
                if ((inst.kind.opcode == Move || inst.kind.opcode == Move32) && inst.args[0].isSomeImm() && inst.args[1].is<Tmp>()) {
                    Tmp tmp = inst.args[1].as<Tmp>();
                    if (tmp.bank() == GP) {
                        auto index = AbsoluteTmpMapper<GP>::absoluteIndex(tmp);
                        if (!m_gpConstDefs.quickGet(index)) {
                            m_gpConstDefs.quickSet(index);
                            m_gpConstants[index] = inst.kind.opcode == Move32 ? static_cast<int64_t>(static_cast<uint64_t>(static_cast<uint32_t>(static_cast<uint64_t>(inst.args[0].value())))) : inst.args[0].value();
                        } else
                            gpNonConstDefs.quickSet(index);
                        m_gpNumWarmUsesAndDefs[index] += frequency;
                    } else {
                        auto index = AbsoluteTmpMapper<FP>::absoluteIndex(tmp);
                        if (!m_fpConstDefs.quickGet(index))
                            m_fpConstDefs.quickSet(index);
                        else
                            fpNonConstDefs.quickSet(index);
                        m_fpNumWarmUsesAndDefs[index] += frequency;
                    }
                    continue;
                }

                inst.forEach<Tmp>(
                    [&] (Tmp& tmp, Arg::Role role, Bank bank, Width) {
                        if (Arg::isWarmUse(role) || Arg::isAnyDef(role)) {
                            if (bank == GP) {
                                auto index = AbsoluteTmpMapper<GP>::absoluteIndex(tmp);
                                m_gpNumWarmUsesAndDefs[index] += frequency;
                                if (Arg::isAnyDef(role))
                                    gpNonConstDefs.quickSet(index);
                            } else {
                                auto index = AbsoluteTmpMapper<FP>::absoluteIndex(tmp);
                                m_fpNumWarmUsesAndDefs[index] += frequency;
                                if (Arg::isAnyDef(role))
                                    fpNonConstDefs.quickSet(index);
                            }
                        }
                    });
            }
        }

        m_gpConstDefs.exclude(gpNonConstDefs);
        m_fpConstDefs.exclude(fpNonConstDefs);
    }

    template<Bank bank>
    bool isConstDef(unsigned absoluteIndex) const
    {
        if constexpr (bank == GP)
            return m_gpConstDefs.quickGet(absoluteIndex);
        else
            return m_fpConstDefs.quickGet(absoluteIndex);
    }

    template<Bank bank>
    decltype(auto) constant(unsigned absoluteIndex) const
    {
        if constexpr (bank == GP)
            return m_gpConstants[absoluteIndex];
        else {
            RELEASE_ASSERT_NOT_REACHED();
            return 0.0;
        }
    }

    template<Bank bank>
    float numWarmUsesAndDefs(unsigned absoluteIndex) const
    {
        if constexpr (bank == GP)
            return m_gpNumWarmUsesAndDefs[absoluteIndex];
        else
            return m_fpNumWarmUsesAndDefs[absoluteIndex];
    }

    void dump(PrintStream& out) const
    {
        CommaPrinter comma(", "_s);
        for (unsigned i = 0; i < m_gpNumWarmUsesAndDefs.size(); ++i)
            out.print(comma, AbsoluteTmpMapper<GP>::tmpFromAbsoluteIndex(i), "=> {numWarmUsesAndDefs="_s, m_gpNumWarmUsesAndDefs[i], ", isConstDef="_s, m_gpConstDefs.quickGet(i), "}"_s);
        for (unsigned i = 0; i < m_fpNumWarmUsesAndDefs.size(); ++i)
            out.print(comma, AbsoluteTmpMapper<FP>::tmpFromAbsoluteIndex(i), "=> {numWarmUsesAndDefs="_s, m_fpNumWarmUsesAndDefs[i], ", isConstDef="_s, m_fpConstDefs.quickGet(i), "}"_s);
    }

private:
    FixedVector<float> m_gpNumWarmUsesAndDefs;
    FixedVector<float> m_fpNumWarmUsesAndDefs;
    BitVector m_gpConstDefs;
    BitVector m_fpConstDefs;
    FixedVector<int64_t> m_gpConstants;
};

} } } // namespace JSC::B3::Air

#endif // ENABLE(B3_JIT)
