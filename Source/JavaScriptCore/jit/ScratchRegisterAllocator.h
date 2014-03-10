/*
 * Copyright (C) 2012, 2014 Apple Inc. All rights reserved.
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

#ifndef ScratchRegisterAllocator_h
#define ScratchRegisterAllocator_h

#if ENABLE(JIT)

#include "MacroAssembler.h"
#include "TempRegisterSet.h"

namespace JSC {

struct ScratchBuffer;

// This class provides a low-level register allocator for use in stubs.

class ScratchRegisterAllocator {
public:
    ScratchRegisterAllocator(const TempRegisterSet& usedRegisters);
    ~ScratchRegisterAllocator();

    void lock(GPRReg reg);
    void lock(FPRReg reg);
    
    template<typename BankInfo>
    typename BankInfo::RegisterType allocateScratch();
    
    GPRReg allocateScratchGPR();
    FPRReg allocateScratchFPR();
    
    bool didReuseRegisters() const
    {
        return !!m_numberOfReusedRegisters;
    }
    
    unsigned numberOfReusedRegisters() const
    {
        return m_numberOfReusedRegisters;
    }
    
    void preserveReusedRegistersByPushing(MacroAssembler& jit);
    void restoreReusedRegistersByPopping(MacroAssembler& jit);
    
    unsigned desiredScratchBufferSize() const;
    
    void preserveUsedRegistersToScratchBuffer(MacroAssembler& jit, ScratchBuffer* scratchBuffer, GPRReg scratchGPR = InvalidGPRReg);
    
    void restoreUsedRegistersFromScratchBuffer(MacroAssembler& jit, ScratchBuffer* scratchBuffer, GPRReg scratchGPR = InvalidGPRReg);
    
private:
    TempRegisterSet m_usedRegisters;
    TempRegisterSet m_lockedRegisters;
    TempRegisterSet m_scratchRegisters;
    unsigned m_numberOfReusedRegisters;
};

} // namespace JSC

#endif // ENABLE(JIT)

#endif // ScratchRegisterAllocator_h
