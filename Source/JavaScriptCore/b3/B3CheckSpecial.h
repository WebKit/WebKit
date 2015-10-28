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

#ifndef B3CheckSpecial_h
#define B3CheckSpecial_h

#if ENABLE(B3_JIT)

#include "AirOpcode.h"
#include "B3StackmapSpecial.h"

namespace JSC { namespace B3 {

// We want to lower Check instructions to a branch, but then we want to route that branch to our
// out-of-line code instead of doing anything else. For this reason, a CheckSpecial will remember
// which branch opcode we have selected along with the number of args in the overload we want. It
// will create an Inst with that opcode plus the appropriate args from the owning Inst whenever you
// call any of the callbacks.
//
// Note that for CheckAdd, CheckSub, and CheckMul we expect that the B3 arguments are the reverse
// of the Air arguments (Add(a, b) => Add32 b, a). Except:
// - CheckSub(0, x), which turns into BranchNeg32 x.
// - CheckMul(a, b), which turns into Mul32 b, a but we pass Any for a's ValueRep.

class CheckSpecial : public StackmapSpecial {
public:
    CheckSpecial(Air::Opcode, unsigned numArgs);
    ~CheckSpecial();

protected:
    // Constructs and returns the Inst representing the branch that this will use.
    Air::Inst hiddenBranch(const Air::Inst&) const;

    void forEachArg(Air::Inst&, const ScopedLambda<Air::Inst::EachArgCallback>&) override;
    bool isValid(Air::Inst&) override;
    bool admitsStack(Air::Inst&, unsigned argIndex) override;

    // NOTE: the generate method will generate the hidden branch and then register a LatePath that
    // generates the stackmap. Super crazy dude!

    CCallHelpers::Jump generate(Air::Inst&, CCallHelpers&, Air::GenerationContext&) override;

    void dumpImpl(PrintStream&) const override;
    void deepDumpImpl(PrintStream&) const override;

private:
    Air::Opcode m_checkOpcode;
    unsigned m_numCheckArgs;
};

} } // namespace JSC::B3

#endif // ENABLE(B3_JIT)

#endif // B3CheckSpecial_h

