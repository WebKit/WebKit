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

#include "AirInstAnalyzer.h"
#include "AirInstInlines.h"
#include "AirTmpInlines.h"
#include "AirTmpWidth.h"
#include "RegisterSet.h"

namespace JSC { namespace B3 { namespace Air {

class TmpWidth::Analyzer final : public InstAnalyzer<TmpWidth::Analyzer> {
public:
    Analyzer(TmpWidth& widths)
        : m_widths(widths)
    {
    }

    void prepare(Code& code)
    {
        if (verbose)
            dataLogLn("Code before TmpWidth:\n", code);

        m_conservativeWidths[GP] = code.usesSIMD() ? conservativeWidth(GP) : conservativeWidthWithoutVectors(GP);
        m_conservativeWidths[FP] = code.usesSIMD() ? conservativeWidth(FP) : conservativeWidthWithoutVectors(FP);

        forEachBank([&](Bank bank) {
            auto& bankWidthsVector = m_widths.widthsVector(bank);
            switch (bank) {
            case GP:
                bankWidthsVector.resize(AbsoluteTmpMapper<GP>::absoluteIndex(code.numTmps(GP)));
                break;
            case FP:
                bankWidthsVector.resize(AbsoluteTmpMapper<FP>::absoluteIndex(code.numTmps(FP)));
                break;
            }
            bankWidthsVector.fill(Widths(bank));
        });

        // Assume the worst for registers.
        RegisterSet::allRegisters().forEach([&](Reg reg) {
            assumeTheWorst(Tmp(reg));
        });

        if (beCareful) {
            code.forEachTmp([&](Tmp tmp) {
                assumeTheWorst(tmp);
            });

            // We fall through because the fixpoint that follows can only make things even more
            // conservative. This mode isn't meant to be fast, just safe.
        }
    }

    // Analyzes everything but Move's over Tmp's, and sets those Move's aside so that finish() can
    // find them quickly. Note that we can make this analysis stronger by recognizing more kinds of
    // Move's or anything that has Move-like behavior, though it's probably not worth it.
    bool visitInst(Inst& inst)
    {
        if (inst.kind.opcode != Move || !inst.args()[1].isTmp())
            return false;

        if (inst.args()[0].isTmp()) {
            // We bucket the Move's per-bank so that the fixpoint can run independently for each bank.
            m_moves[Arg(inst.args()[1]).bank()].append(&inst);
            return true;
        }

        if (inst.args()[0].isImm() && inst.args()[0].value() >= 0) {
            Widths& tmpWidths = m_widths.widths(inst.args()[1].tmp());
            Width maxWidth = Width64;
            if (inst.args()[0].value() <= std::numeric_limits<int8_t>::max())
                maxWidth = Width8;
            else if (inst.args()[0].value() <= std::numeric_limits<int16_t>::max())
                maxWidth = Width16;
            else if (inst.args()[0].value() <= std::numeric_limits<int32_t>::max())
                maxWidth = Width32;

            tmpWidths.def = std::max(tmpWidths.def, maxWidth);
            return true;
        }

        return false;
    }

    void visitTmp(Tmp& tmp, Arg::Role role, Bank bank, Width width)
    {
        Widths& tmpWidths = m_widths.widths(tmp);
        if (Arg::isAnyUse(role))
            tmpWidths.use = std::max(tmpWidths.use, width);

        if (Arg::isZDef(role))
            tmpWidths.def = std::max(tmpWidths.def, width);
        else if (Arg::isAnyDef(role))
            tmpWidths.def = m_conservativeWidths[bank];
    }

    void finish()
    {
        forEachBank([&](Bank bank) {
            m_widths.fixpointMoves(m_moves[bank]);
        });

        if (verbose) {
            forEachBank([&](Bank bank) {
                dataLogLn("bank: ", bank, ", widthsVector: ");
                auto& vector = m_widths.widthsVector(bank);
                for (unsigned i = 0; i < vector.size(); ++i) {
                    if (bank == GP)
                        dataLogLn("\t", AbsoluteTmpMapper<GP>::tmpFromAbsoluteIndex(i), " : ", vector[i]);
                    else
                        dataLogLn("\t", AbsoluteTmpMapper<FP>::tmpFromAbsoluteIndex(i), " : ", vector[i]);
                }
            });
        }
    }

private:
    // Set this to true to cause this analysis to always return pessimistic results.
    static constexpr bool beCareful = false;
    static constexpr bool verbose = false;

    void assumeTheWorst(Tmp tmp)
    {
        Width conservative = m_conservativeWidths[Arg(tmp).bank()];
        m_widths.addWidths(tmp, { conservative, conservative });
    }

    TmpWidth& m_widths;
    std::array<Vector<Inst*>, numBanks> m_moves;
    std::array<Width, numBanks> m_conservativeWidths;
};

inline TmpWidth::Widths& TmpWidth::widths(Tmp tmp)
{
    if (tmp.isGP()) {
        unsigned index = AbsoluteTmpMapper<GP>::absoluteIndex(tmp);
        ASSERT(index < m_widthGP.size());
        return m_widthGP[index];
    }
    unsigned index = AbsoluteTmpMapper<FP>::absoluteIndex(tmp);
    ASSERT(index < m_widthFP.size());
    return m_widthFP[index];
}

const TmpWidth::Widths& TmpWidth::widths(Tmp tmp) const
{
    return const_cast<TmpWidth*>(this)->widths(tmp);
}

Width TmpWidth::width(Tmp tmp) const
{
    Widths tmpWidths = widths(tmp);
    return std::min(tmpWidths.use, tmpWidths.def);
}

void TmpWidth::addWidths(Tmp tmp, Widths tmpWidths)
{
    widths(tmp) = tmpWidths;
}

Width TmpWidth::requiredWidth(Tmp tmp)
{
    Widths tmpWidths = widths(tmp);
    return std::max(tmpWidths.use, tmpWidths.def);
}

Width TmpWidth::defWidth(Tmp tmp) const
{
    return widths(tmp).def;
}

Width TmpWidth::useWidth(Tmp tmp) const
{
    return widths(tmp).use;
}

} } } // namespace JSC::B3::Air

#endif // ENABLE(B3_JIT)
