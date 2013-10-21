/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#ifndef RegisterSet_h
#define RegisterSet_h

#include <wtf/Platform.h>

#if ENABLE(JIT)

#include "FPRInfo.h"
#include "GPRInfo.h"
#include "MacroAssembler.h"
#include "TempRegisterSet.h"
#include <wtf/BitVector.h>

namespace JSC {

class RegisterSet {
public:
    RegisterSet() { }
    
    static RegisterSet specialRegisters();

    void set(GPRReg reg, bool value = true)
    {
        m_vector.set(MacroAssembler::registerIndex(reg), value);
    }
    
    void set(JSValueRegs regs)
    {
        if (regs.tagGPR() != InvalidGPRReg)
            set(regs.tagGPR());
        set(regs.payloadGPR());
    }
    
    void clear(GPRReg reg)
    {
        set(reg, false);
    }
    
    bool get(GPRReg reg) const { return m_vector.get(MacroAssembler::registerIndex(reg)); }
    
    void set(FPRReg reg, bool value = true)
    {
        m_vector.set(MacroAssembler::registerIndex(reg), value);
    }
    
    void clear(FPRReg reg)
    {
        set(reg, false);
    }
    
    bool get(FPRReg reg) const { return m_vector.get(MacroAssembler::registerIndex(reg)); }
    
    void merge(const RegisterSet& other) { m_vector.merge(other.m_vector); }
    
private:
    BitVector m_vector;
};

} // namespace JSC

#endif // ENABLE(JIT)

#endif // RegisterSet_h

