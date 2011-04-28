/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#ifndef DFGFPRInfo_h
#define DFGFPRInfo_h

#if ENABLE(DFG_JIT)

#include <assembler/MacroAssembler.h>
#include <dfg/DFGRegisterBank.h>

namespace JSC { namespace DFG {

// Abstracted sequential numbering of available machine registers (as opposed to GPRReg,
// which are non-sequential, and not abstracted from the register numbering used by the underlying processor).
enum FPRReg { fpr0, fpr1, fpr2, fpr3, fpr4, fpr5, numberOfFPRs, InvalidFPRReg = 0xFFFFFFFF };

inline FPRReg next(FPRReg& reg)
{
    ASSERT(reg < numberOfFPRs);
    return reg = static_cast<FPRReg>(reg + 1);
}

class FPRBankInfo {
public:
    typedef FPRReg RegisterType;
    static const unsigned numberOfRegisters = 6;

    static FPRReg toRegister(unsigned index)
    {
        return (FPRReg)index;
    }

    static unsigned toIndex(FPRReg reg)
    {
        return (unsigned)reg;
    }
};

typedef RegisterBank<FPRBankInfo>::iterator fpr_iterator;

} } // namespace JSC::DFG

#endif
#endif
