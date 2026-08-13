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
#include "AirInstAnalyzer.h"
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
    class Analyzer;

    UseCounts() = default;
    inline UseCounts(Code&);

    template<Bank bank>
    bool isConstDef(unsigned absoluteIndex) const
    {
        if constexpr (bank == GP)
            return m_gpConstDefs.get(absoluteIndex);
        else
            return m_fpConstDefs.get(absoluteIndex);
    }

    template<Bank bank>
    decltype(auto) constant(unsigned absoluteIndex) const
    {
        if constexpr (bank == GP)
            return m_gpConstants[absoluteIndex];
        else
            return m_fpConstants[absoluteIndex];
    }

    template<Bank bank>
    float numWarmUsesAndDefs(unsigned absoluteIndex) const
    {
        if constexpr (bank == GP)
            return m_gpNumWarmUsesAndDefs[absoluteIndex];
        else
            return m_fpNumWarmUsesAndDefs[absoluteIndex];
    }

    template<Bank bank>
    Width constantWidth(unsigned absoluteIndex) const
    {
        static_assert(bank == FP, "constantWidth only valid for FP bank");
        return m_fpConstantWidths[absoluteIndex];
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
    FixedVector<v128_t> m_fpConstants;
    FixedVector<Width> m_fpConstantWidths;
};

class UseCounts::Analyzer final : public InstAnalyzer<UseCounts::Analyzer> {
public:
    Analyzer(UseCounts& useCounts)
        : m_useCounts(useCounts)
    {
    }

    void prepare(Code& code)
    {
        // Find non-rare blocks.
        m_fastBlocks.push(code[0]);
        while (BasicBlock* block = m_fastBlocks.pop()) {
            for (FrequentedBlock& successor : block->successors()) {
                if (!successor.isRare())
                    m_fastBlocks.push(successor.block());
            }
        }

        unsigned gpArraySize = AbsoluteTmpMapper<GP>::absoluteIndex(code.numTmps(GP));
        m_useCounts.m_gpNumWarmUsesAndDefs = FixedVector<float>(FillWith { }, gpArraySize, 0);
        m_useCounts.m_gpConstDefs.ensureSize(gpArraySize);
        m_gpNonConstDefs.ensureSize(gpArraySize);
        m_useCounts.m_gpConstants = FixedVector<int64_t>(FillWith { }, gpArraySize, 0);

        unsigned fpArraySize = AbsoluteTmpMapper<FP>::absoluteIndex(code.numTmps(FP));
        m_useCounts.m_fpNumWarmUsesAndDefs = FixedVector<float>(FillWith { }, fpArraySize, 0);
        m_useCounts.m_fpConstDefs.ensureSize(fpArraySize);
        m_fpNonConstDefs.ensureSize(fpArraySize);
        m_useCounts.m_fpConstants = FixedVector<v128_t>(FillWith { }, fpArraySize, v128_t { });
        m_useCounts.m_fpConstantWidths = FixedVector<Width>(FillWith { }, fpArraySize, Width8);
    }

    void startBlock(BasicBlock* block)
    {
        m_frequency = block->frequency();
        if (!m_fastBlocks.saw(block))
            m_frequency *= Options::rareBlockPenalty();
    }

    // A Tmp that a constant is moved into is counted here rather than by visitTmp, because we also
    // want to remember the constant itself.
    bool visitInst(Inst& inst)
    {
        switch (inst.kind.opcode) {
        case Move:
        case Move32: {
            if (inst.args()[0].isSomeImm() && inst.args()[1].is<Tmp>()) {
                Tmp tmp = inst.args()[1].as<Tmp>();
                if (tmp.bank() == GP) {
                    auto index = AbsoluteTmpMapper<GP>::absoluteIndex(tmp);
                    if (!m_useCounts.m_gpConstDefs.quickGet(index)) {
                        m_useCounts.m_gpConstDefs.quickSet(index);
                        m_useCounts.m_gpConstants[index] = extractGPConstant(inst.kind.opcode, inst.args()[0]);
                    } else
                        m_gpNonConstDefs.quickSet(index);
                    m_useCounts.m_gpNumWarmUsesAndDefs[index] += m_frequency;
                    return true;
                }
            }
            break;
        }
        case MoveFloat:
        case MoveDouble:
        case MoveVector: {
            if (inst.args()[0].isSomeImm() && inst.args()[1].is<Tmp>()) {
                Tmp tmp = inst.args()[1].as<Tmp>();
                if (tmp.bank() == FP) {
                    auto index = AbsoluteTmpMapper<FP>::absoluteIndex(tmp);
                    if (!m_useCounts.m_fpConstDefs.quickGet(index)) {
                        m_useCounts.m_fpConstDefs.quickSet(index);
                        m_useCounts.m_fpConstants[index] = extractFPConstant(inst.args()[0]);
                        switch (inst.kind.opcode) {
                        case MoveFloat:
                            m_useCounts.m_fpConstantWidths[index] = Width32;
                            break;
                        case MoveDouble:
                            m_useCounts.m_fpConstantWidths[index] = Width64;
                            break;
                        case MoveVector:
                            m_useCounts.m_fpConstantWidths[index] = Width128;
                            break;
                        default:
                            RELEASE_ASSERT_NOT_REACHED();
                        }
                    } else
                        m_fpNonConstDefs.quickSet(index);
                    m_useCounts.m_fpNumWarmUsesAndDefs[index] += m_frequency;
                    return true;
                }
            }
            break;
        }
        default:
            break;
        }

        return false;
    }

    void visitTmp(Tmp& tmp, Arg::Role role, Bank bank, Width)
    {
        if (!Arg::isWarmUse(role) && !Arg::isAnyDef(role))
            return;

        if (bank == GP) {
            auto index = AbsoluteTmpMapper<GP>::absoluteIndex(tmp);
            m_useCounts.m_gpNumWarmUsesAndDefs[index] += m_frequency;
            if (Arg::isAnyDef(role))
                m_gpNonConstDefs.quickSet(index);
        } else {
            auto index = AbsoluteTmpMapper<FP>::absoluteIndex(tmp);
            m_useCounts.m_fpNumWarmUsesAndDefs[index] += m_frequency;
            if (Arg::isAnyDef(role))
                m_fpNonConstDefs.quickSet(index);
        }
    }

    void finish()
    {
        m_useCounts.m_gpConstDefs.exclude(m_gpNonConstDefs);
        m_useCounts.m_fpConstDefs.exclude(m_fpNonConstDefs);
    }

private:
    static int64_t extractGPConstant(Air::Opcode opcode, const Air::Arg& arg)
    {
        return opcode == Move32 ? static_cast<int64_t>(static_cast<uint64_t>(static_cast<uint32_t>(static_cast<uint64_t>(arg.value())))) : arg.value();
    }

    static v128_t extractFPConstant(const Air::Arg& arg)
    {
        if (arg.isFPImm128())
            return arg.asV128();

        v128_t v { };
        v.u64x2[0] = static_cast<uint64_t>(arg.value());
        return v;
    }

    UseCounts& m_useCounts;
    BlockWorklist m_fastBlocks;
    BitVector m_gpNonConstDefs;
    BitVector m_fpNonConstDefs;
    double m_frequency { 0 };
};

inline UseCounts::UseCounts(Code& code)
{
    Analyzer analyzer(*this);
    analyzer.run(code);
}

} } } // namespace JSC::B3::Air

#endif // ENABLE(B3_JIT)
