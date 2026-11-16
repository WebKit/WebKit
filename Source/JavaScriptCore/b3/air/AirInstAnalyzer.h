/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "AirCode.h"
#include "AirInst.h"
#include <array>

namespace JSC { namespace B3 { namespace Air {

template<typename... Analyzers>
void analyzeCode(Code&, Analyzers&...);

template<typename Derived>
class InstAnalyzer {
public:
    // Runs this analysis on its own. Equivalent to analyzeCode(code, *this).
    void run(Code& code)
    {
        analyzeCode(code, static_cast<Derived&>(*this));
    }

    // Called once before the walk, then once per block in code order before the block's
    // instructions.
    void prepare(Code&) { }
    void startBlock(BasicBlock*) { }

    // Return true if the analysis is done with this instruction and does not want to see its Tmps.
    bool visitInst(Inst&) { return false; }

    void visitTmp(Tmp&, Arg::Role, Bank, Width) { }

    // Called once after the walk, for whatever the analysis cannot finish until it has seen all of
    // the code, such as a fixpoint.
    void finish() { }
};

template<typename... Analyzers>
void analyzeCode(Code& code, Analyzers&... analyzers)
{
    (analyzers.prepare(code), ...);

    for (BasicBlock* block : code) {
        (analyzers.startBlock(block), ...);

        for (Inst& inst : *block) {
            // An analysis that handles an instruction by itself does not want its Tmps, so the
            // enumeration below runs only for those that still do, and not at all when that is none
            // of them.
            std::array<bool, sizeof...(Analyzers)> needsTmps { };
            bool anyNeedsTmps = false;
            unsigned instIndex = 0;
            auto reportInst = [&](auto& analyzer) {
                bool needs = !analyzer.visitInst(inst);
                needsTmps[instIndex++] = needs;
                anyNeedsTmps |= needs;
            };
            (reportInst(analyzers), ...);

            if (!anyNeedsTmps)
                continue;

            inst.forEachTmp(
                [&] (Tmp& tmp, Arg::Role role, Bank bank, Width width) {
                    unsigned tmpIndex = 0;
                    auto reportTmp = [&](auto& analyzer) {
                        if (needsTmps[tmpIndex++])
                            analyzer.visitTmp(tmp, role, bank, width);
                    };
                    (reportTmp(analyzers), ...);
                });
        }
    }

    (analyzers.finish(), ...);
}

} } } // namespace JSC::B3::Air

#endif // ENABLE(B3_JIT)
