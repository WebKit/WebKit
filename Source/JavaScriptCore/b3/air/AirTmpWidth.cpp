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

#include "config.h"
#include "AirTmpWidth.h"

#if ENABLE(B3_JIT)

#include "AirCode.h"
#include "AirInstInlines.h"
#include "AirTmpWidthInlines.h"
#include <optional>

namespace JSC { namespace B3 { namespace Air {

TmpWidth::TmpWidth() = default;

TmpWidth::TmpWidth(Code& code)
{
    recomputeBoth(code);
}

TmpWidth::~TmpWidth() = default;

template <auto bankFilter>
void TmpWidth::recompute(Code& code)
{
    // Set this to true to cause this analysis to always return pessimistic results.
    constexpr bool beCareful = false;
    constexpr bool verbose = false;

    constexpr auto shouldProcess = [](Bank bank) {
        if constexpr (std::is_same_v<decltype(bankFilter), Bank>)
            return bankFilter == bank;
        else
            return true;
    };

    if (verbose) {
        dataLogLn("Code before TmpWidth:");
        dataLog(code);
    }

    forEachBank([&](Bank bank) {
        if (!shouldProcess(bank))
            return;

        auto& bankWidthsVector = widthsVector(bank);
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

    auto assumeTheWorst = [&](Tmp tmp) {
        Bank bank = Arg(tmp).bank();
        if (!shouldProcess(bank))
            return;

        Width conservative = code.usesSIMD() ? conservativeWidth(bank) : conservativeWidthWithoutVectors(bank);
        addWidths(tmp, { conservative, conservative });
    };

    // Assume the worst for registers.
    RegisterSet::allRegisters().forEach([&](Reg reg) {
        assumeTheWorst(Tmp(reg));
    });

    if (beCareful) {
        code.forEachTmp(assumeTheWorst);

        // We fall through because the fixpoint that follows can only make things even more
        // conservative. This mode isn't meant to be fast, just safe.
    }

    // Now really analyze everything but Move's over Tmp's, but set aside those Move's so we can find
    // them quickly during the fixpoint below. Note that we can make this analysis stronger by
    // recognizing more kinds of Move's or anything that has Move-like behavior, though it's probably not
    // worth it. We bucket the Move's per-bank so the fixpoint can run independently for each bank.
    std::array<Vector<Inst*>, numBanks> moves;
    for (BasicBlock* block : code) {
        for (Inst& inst : *block) {
            if (inst.kind.opcode == Move && inst.args[1].isTmp()) {
                Bank dstBank = Arg(inst.args[1]).bank();
                if (!shouldProcess(dstBank))
                    continue;

                if (inst.args[0].isTmp()) {
                    moves[dstBank].append(&inst);
                    continue;
                }

                if (inst.args[0].isImm() && inst.args[0].value() >= 0) {
                    Widths& tmpWidths = widths(inst.args[1].tmp());
                    Width maxWidth = Width64;
                    if (inst.args[0].value() <= std::numeric_limits<int8_t>::max())
                        maxWidth = Width8;
                    else if (inst.args[0].value() <= std::numeric_limits<int16_t>::max())
                        maxWidth = Width16;
                    else if (inst.args[0].value() <= std::numeric_limits<int32_t>::max())
                        maxWidth = Width32;

                    tmpWidths.def = std::max(tmpWidths.def, maxWidth);
                    continue;
                }
            }
            inst.forEachTmp(
                [&] (Tmp& tmp, Arg::Role role, Bank tmpBank, Width width) {
                    if (!shouldProcess(Arg(tmp).bank()))
                        return;

                    Widths& tmpWidths = widths(tmp);
                    if (Arg::isAnyUse(role))
                        tmpWidths.use = std::max(tmpWidths.use, width);

                    if (Arg::isZDef(role))
                        tmpWidths.def = std::max(tmpWidths.def, width);
                    else if (Arg::isAnyDef(role))
                        tmpWidths.def = code.usesSIMD() ? conservativeWidth(tmpBank) : conservativeWidthWithoutVectors(tmpBank);
                });
        }
    }

    // Finally, fixpoint over the Move's.
    auto fixpointMoves = [&](const Vector<Inst*>& moves) {
        bool changed = true;
        while (changed) {
            changed = false;
            for (Inst* move : moves) {
                ASSERT(move->kind.opcode == Move);
                ASSERT(move->args[0].isTmp());
                ASSERT(move->args[1].isTmp());

                Widths& srcWidths = widths(move->args[0].tmp());
                Widths& dstWidths = widths(move->args[1].tmp());

                // Legend:
                //
                //     Move %src, %dst

                // defWidth(%dst) is a promise about how many high bits are zero. The smaller the width, the
                // stronger the promise. This Move may weaken that promise if we know that %src is making a
                // weaker promise. Such forward flow is the only thing that determines defWidth().
                if (dstWidths.def < srcWidths.def) {
                    dstWidths.def = srcWidths.def;
                    changed = true;
                }

                // srcWidth(%src) is a promise about how many high bits are ignored. The smaller the width,
                // the stronger the promise. This Move may weaken that promise if we know that %dst is making
                // a weaker promise. Such backward flow is the only thing that determines srcWidth().
                if (srcWidths.use < dstWidths.use) {
                    srcWidths.use = dstWidths.use;
                    changed = true;
                }
            }
        }
    };
    forEachBank([&](Bank bank) {
        if (!shouldProcess(bank))
            return;

        fixpointMoves(moves[bank]);
    });

    if (verbose) {
        forEachBank([&](Bank bank) {
            if (!shouldProcess(bank))
                return;

            dataLogLn("bank: ", bank, ", widthsVector: ");
            auto& vector = widthsVector(bank);
            for (unsigned i = 0; i < vector.size(); ++i) {
                if (bank == GP)
                    dataLogLn("\t", AbsoluteTmpMapper<GP>::tmpFromAbsoluteIndex(i), " : ", vector[i]);
                else
                    dataLogLn("\t", AbsoluteTmpMapper<FP>::tmpFromAbsoluteIndex(i), " : ", vector[i]);
            }
        });
    }
}

void TmpWidth::recomputeBoth(Code& code)
{
    recompute<std::nullopt>(code);
}

template <Bank bank>
void TmpWidth::ensureSize(Tmp tmp)
{
    ASSERT(tmp.bank() == bank);
    auto index = AbsoluteTmpMapper<bank>::absoluteIndex(tmp);
    auto& bankWidthsVector = widthsVector(bank);
    if (index >= bankWidthsVector.size())
        bankWidthsVector.resize(index + 1);
}

void TmpWidth::setWidths(Tmp tmp, Width useWidth, Width defWidth)
{
    if (tmp.isGP())
        ensureSize<GP>(tmp);
    else
        ensureSize<FP>(tmp);
    addWidths(tmp, { useWidth, defWidth });
}


void TmpWidth::Widths::dump(PrintStream& out) const
{
    out.print("{use = ", use, ", def = ", def, "}");
}

template void TmpWidth::recompute<GP>(Code&);
template void TmpWidth::recompute<FP>(Code&);
template void TmpWidth::recompute<std::nullopt>(Code&);

} } } // namespace JSC::B3::Air

#endif // ENABLE(B3_JIT)
