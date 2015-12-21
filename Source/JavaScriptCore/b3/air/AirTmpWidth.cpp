/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

namespace JSC { namespace B3 { namespace Air {

TmpWidth::TmpWidth()
{
}

TmpWidth::TmpWidth(Code& code)
{
    recompute(code);
}

TmpWidth::~TmpWidth()
{
}

void TmpWidth::recompute(Code& code)
{
    m_width.clear();
    
    // Assume the worst for registers.
    RegisterSet::allRegisters().forEach(
        [&] (Reg reg) {
            Widths& widths = m_width.add(Tmp(reg), Widths()).iterator->value;
            Arg::Type type = Arg(Tmp(reg)).type();
            widths.use = Arg::conservativeWidth(type);
            widths.def = Arg::conservativeWidth(type);
        });
    
    // Now really analyze everything but Move's over Tmp's, but set aside those Move's so we can find
    // them quickly during the fixpoint below. Note that we can make this analysis stronger by
    // recognizing more kinds of Move's or anything that has Move-like behavior, though it's probably not
    // worth it.
    Vector<Inst*> moves;
    for (BasicBlock* block : code) {
        for (Inst& inst : *block) {
            if (inst.opcode == Move && inst.args[1].isTmp()) {
                if (inst.args[0].isTmp()) {
                    moves.append(&inst);
                    continue;
                }
                if (inst.args[0].isImm()
                    && inst.args[0].value() >= 0) {
                    Tmp tmp = inst.args[1].tmp();
                    Widths& widths = m_width.add(tmp, Widths(Arg::GP)).iterator->value;
                    
                    if (inst.args[0].value() <= std::numeric_limits<int8_t>::max())
                        widths.def = std::max(widths.def, Arg::Width8);
                    else if (inst.args[0].value() <= std::numeric_limits<int16_t>::max())
                        widths.def = std::max(widths.def, Arg::Width16);
                    else if (inst.args[0].value() <= std::numeric_limits<int32_t>::max())
                        widths.def = std::max(widths.def, Arg::Width32);
                    else
                        widths.def = std::max(widths.def, Arg::Width64);

                    continue;
                }
            }
            inst.forEachTmp(
                [&] (Tmp& tmp, Arg::Role role, Arg::Type type, Arg::Width width) {
                    Widths& widths = m_width.add(tmp, Widths(type)).iterator->value;
                    
                    if (Arg::isAnyUse(role))
                        widths.use = std::max(widths.use, width);

                    if (Arg::isZDef(role))
                        widths.def = std::max(widths.def, width);
                    else if (Arg::isDef(role))
                        widths.def = Arg::conservativeWidth(type);
                });
        }
    }

    // Finally, fixpoint over the Move's.
    bool changed = true;
    while (changed) {
        changed = false;
        for (Inst* move : moves) {
            ASSERT(move->opcode == Move);
            ASSERT(move->args[0].isTmp());
            ASSERT(move->args[1].isTmp());
            
            Widths& srcWidths = m_width.add(move->args[0].tmp(), Widths(Arg::GP)).iterator->value;
            Widths& dstWidths = m_width.add(move->args[1].tmp(), Widths(Arg::GP)).iterator->value;

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

} } } // namespace JSC::B3::Air

#endif // ENABLE(B3_JIT)

