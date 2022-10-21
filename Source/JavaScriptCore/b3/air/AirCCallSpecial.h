/*
 * Copyright (C) 2015-2019 Apple Inc. All rights reserved.
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

#include "AirSpecial.h"
#include "RegisterSet.h"

namespace JSC { namespace B3 { namespace Air {

// Use this special for constructing a C call. Arg 0 is of course a Special arg that refers to the
// CCallSpecial object. Arg 1 is the callee, and it can be an ImmPtr, a register, or an address. The
// next three args - arg 2, arg 3, and arg 4 - hold the return value GPRs and FPR. The remaining args
// are just the set of argument registers used by this call. For arguments that go to the stack, you
// have to do the grunt work of doing those stack stores. In fact, the only reason why we specify the
// argument registers as arguments to a call is so that the liveness analysis can see that they get
// used here. It would be wrong to automagically report all argument registers as being used because
// if we had a call that didn't pass them, then they'd appear to be live until some clobber point or
// the prologue, whichever happened sooner.

class CCallSpecial final : public Special {
public:
    CCallSpecial();
    ~CCallSpecial() final;

    // You cannot use this register to pass arguments. It just so happens that this register is not
    // used for arguments in the C calling convention. By the way, this is the only thing that causes
    // this special to be specific to C calls.
    static constexpr GPRReg scratchRegister = GPRInfo::nonPreservedNonArgumentGPR0;

private:
    void forEachArg(Inst&, const ScopedLambda<Inst::EachArgCallback>&) final;
    bool isValid(Inst&) final;
    bool admitsStack(Inst&, unsigned argIndex) final;
    bool admitsExtendedOffsetAddr(Inst&, unsigned) final;
    void reportUsedRegisters(Inst&, const RegisterSetBuilder&) final;
    MacroAssembler::Jump generate(Inst&, CCallHelpers&, GenerationContext&) final;
    RegisterSetBuilder extraEarlyClobberedRegs(Inst&) final;
    RegisterSetBuilder extraClobberedRegs(Inst&) final;

    void dumpImpl(PrintStream&) const final;
    void deepDumpImpl(PrintStream&) const final;

    static constexpr unsigned specialArgOffset = 0;
    static constexpr unsigned numSpecialArgs = 1;
    static constexpr unsigned calleeArgOffset = numSpecialArgs;
    static constexpr unsigned numCalleeArgs = 1;
    static constexpr unsigned returnGPArgOffset = numSpecialArgs + numCalleeArgs;
    static constexpr unsigned numReturnGPArgs = 2;
    static constexpr unsigned returnFPArgOffset = numSpecialArgs + numCalleeArgs + numReturnGPArgs;
    static constexpr unsigned numReturnFPArgs = 1;
    static constexpr unsigned argArgOffset =
        numSpecialArgs + numCalleeArgs + numReturnGPArgs + numReturnFPArgs;
    
    RegisterSetBuilder m_clobberedRegs;
};

} } } // namespace JSC::B3::Air

#endif // ENABLE(B3_JIT)
