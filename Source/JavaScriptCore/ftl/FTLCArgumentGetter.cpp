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

#include "config.h"
#include "FTLCArgumentGetter.h"

#if ENABLE(FTL_JIT)

namespace JSC { namespace FTL {

using namespace DFG;

void CArgumentGetter::loadNextAndBox(ValueFormat format, GPRReg destination, GPRReg scratch)
{
    if (scratch == InvalidGPRReg) {
        if (destination == GPRInfo::nonArgGPR0)
            scratch = GPRInfo::nonArgGPR1;
        else
            scratch = GPRInfo::nonArgGPR0;
    }
    
    switch (format) {
    case ValueFormatInt32: {
        loadNext32(destination);
        m_jit.or64(GPRInfo::tagTypeNumberRegister, destination);
        break;
    }
            
    case ValueFormatUInt32: {
        loadNext32(destination);
        MacroAssembler::Jump isInt = m_jit.branch32(
            MacroAssembler::GreaterThanOrEqual,
            destination, MacroAssembler::TrustedImm32(0));
            
        m_jit.moveDoubleTo64(FPRInfo::fpRegT0, scratch);
        m_jit.convertInt32ToDouble(destination, FPRInfo::fpRegT0);
        m_jit.boxDouble(FPRInfo::fpRegT0, destination);
        m_jit.move64ToDouble(scratch, FPRInfo::fpRegT0);
            
        MacroAssembler::Jump done = m_jit.jump();
            
        isInt.link(&m_jit);
        m_jit.or64(GPRInfo::tagTypeNumberRegister, destination);
            
        done.link(&m_jit);
        break;
    }
            
    case ValueFormatBoolean: {
        loadNext8(destination);
        m_jit.or32(MacroAssembler::TrustedImm32(ValueFalse), destination);
        break;
    }
            
    case ValueFormatJSValue: {
        loadNext64(destination);
        break;
    }
            
    case ValueFormatDouble: {
        m_jit.moveDoubleTo64(FPRInfo::fpRegT0, scratch);
        loadNextDouble(FPRInfo::fpRegT0);
        m_jit.boxDouble(FPRInfo::fpRegT0, destination);
        m_jit.move64ToDouble(scratch, FPRInfo::fpRegT0);
        break;
    }
            
    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }
}

} } // namespace JSC::FTL

#endif // ENABLE(FTL_JIT)

