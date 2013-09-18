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

#ifndef FTLCArgumentGetter_h
#define FTLCArgumentGetter_h

#include <wtf/Platform.h>

#if ENABLE(FTL_JIT)

#include "DFGAssemblyHelpers.h"
#include "FTLValueFormat.h"

namespace JSC { namespace FTL {

// This currently only understands how to get arguments on X86-64 cdecl. This is also
// not particularly efficient. It will generate some redundant moves.

template<typename RegisterKind>
bool isArgumentRegister(typename RegisterKind::RegisterType reg)
{
    for (unsigned i = 0; i < RegisterKind::numberOfArgumentRegisters; ++i) {
        if (RegisterKind::toArgumentRegister(i) == reg)
            return true;
    }
    return false;
}

class CArgumentGetter {
public:
    // Peek offset is the number of things on the stack below the first argument.
    // It will be 1 if you haven't pushed or popped after the call; i.e. the only
    // thing is the return address.
    CArgumentGetter(DFG::AssemblyHelpers& jit, int peekOffset = 1)
        : m_jit(jit)
        , m_peekOffset(peekOffset)
        , m_gprArgumentIndex(0)
        , m_fprArgumentIndex(0)
        , m_stackArgumentIndex(0)
    {
    }
    
    void loadNext8(DFG::GPRReg destination)
    {
        ASSERT(!isArgumentRegister<DFG::GPRInfo>(destination));
        if (m_gprArgumentIndex < DFG::GPRInfo::numberOfArgumentRegisters) {
            m_jit.move(DFG::GPRInfo::toArgumentRegister(m_gprArgumentIndex++), destination);
            return;
        }
        
        m_jit.load8(nextAddress(), destination);
    }
    
    void loadNext32(DFG::GPRReg destination)
    {
        ASSERT(!isArgumentRegister<DFG::GPRInfo>(destination));
        if (m_gprArgumentIndex < DFG::GPRInfo::numberOfArgumentRegisters) {
            m_jit.move(DFG::GPRInfo::toArgumentRegister(m_gprArgumentIndex++), destination);
            return;
        }
        
        m_jit.load32(nextAddress(), destination);
    }
    
    void loadNext64(DFG::GPRReg destination)
    {
        ASSERT(!isArgumentRegister<DFG::GPRInfo>(destination));
        if (m_gprArgumentIndex < DFG::GPRInfo::numberOfArgumentRegisters) {
            m_jit.move(DFG::GPRInfo::toArgumentRegister(m_gprArgumentIndex++), destination);
            return;
        }
        
        m_jit.load64(nextAddress(), destination);
    }
    
    void loadNextPtr(DFG::GPRReg destination)
    {
        loadNext64(destination);
    }
    
    void loadNextDouble(DFG::FPRReg destination)
    {
        ASSERT(
            !isArgumentRegister<DFG::FPRInfo>(destination)
            || destination == DFG::FPRInfo::argumentFPR0);
        
        if (m_fprArgumentIndex < DFG::FPRInfo::numberOfArgumentRegisters) {
            m_jit.moveDouble(DFG::FPRInfo::toArgumentRegister(m_fprArgumentIndex++), destination);
            return;
        }
        
        m_jit.loadDouble(nextAddress(), destination);
    }
    
    void loadNextAndBox(
        ValueFormat, DFG::GPRReg destination,
        DFG::GPRReg scratch1 = InvalidGPRReg, DFG::GPRReg scratch2 = InvalidGPRReg);

private:
    MacroAssembler::Address nextAddress()
    {
        return MacroAssembler::Address(
            MacroAssembler::stackPointerRegister,
            (m_peekOffset + m_stackArgumentIndex++) * sizeof(void*));
    }
            
    DFG::AssemblyHelpers& m_jit;
    unsigned m_peekOffset;
    unsigned m_gprArgumentIndex;
    unsigned m_fprArgumentIndex;
    unsigned m_stackArgumentIndex;
};

} } // namespace JSC::FTL

#endif // ENABLE(FTL_JIT)

#endif // FTLCArgumentGetter_h

