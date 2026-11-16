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

namespace JSC { namespace B3 { namespace Air {

TmpWidth::TmpWidth() = default;

TmpWidth::TmpWidth(Code& code)
{
    recompute(code);
}

TmpWidth::~TmpWidth() = default;

void TmpWidth::recompute(Code& code)
{
    Analyzer analyzer(*this);
    analyzer.run(code);
}

void TmpWidth::fixpointMoves(const Vector<Inst*>& moves)
{
    bool changed = true;
    while (changed) {
        changed = false;
        for (Inst* move : moves) {
            ASSERT(move->kind.opcode == Move);
            ASSERT(move->args()[0].isTmp());
            ASSERT(move->args()[1].isTmp());

            Widths& srcWidths = widths(move->args()[0].tmp());
            Widths& dstWidths = widths(move->args()[1].tmp());

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

} } } // namespace JSC::B3::Air

#endif // ENABLE(B3_JIT)
