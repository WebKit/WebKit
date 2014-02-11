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
#include "RegisterSet.h"

#if ENABLE(JIT)

#include "GPRInfo.h"
#include "MacroAssembler.h"
#include "JSCInlines.h"

namespace JSC {

RegisterSet RegisterSet::stackRegisters()
{
    RegisterSet result;
    result.set(MacroAssembler::stackPointerRegister);
    result.set(MacroAssembler::framePointerRegister);
    return result;
}

RegisterSet RegisterSet::specialRegisters()
{
    RegisterSet result;
    result.merge(stackRegisters());
    result.set(GPRInfo::callFrameRegister);
#if USE(JSVALUE64)
    result.set(GPRInfo::tagTypeNumberRegister);
    result.set(GPRInfo::tagMaskRegister);
#endif
    return result;
}

RegisterSet RegisterSet::calleeSaveRegisters()
{
    RegisterSet result;
#if CPU(X86_64)
    result.set(X86Registers::ebx);
    result.set(X86Registers::ebp);
    result.set(X86Registers::r12);
    result.set(X86Registers::r13);
    result.set(X86Registers::r14);
    result.set(X86Registers::r15);
#else
    UNREACHABLE_FOR_PLATFORM();
#endif
    return result;
}

RegisterSet RegisterSet::allGPRs()
{
    RegisterSet result;
    for (MacroAssembler::RegisterID reg = MacroAssembler::firstRegister(); reg <= MacroAssembler::lastRegister(); reg = static_cast<MacroAssembler::RegisterID>(reg + 1))
        result.set(reg);
    return result;
}

RegisterSet RegisterSet::allFPRs()
{
    RegisterSet result;
    for (MacroAssembler::FPRegisterID reg = MacroAssembler::firstFPRegister(); reg <= MacroAssembler::lastFPRegister(); reg = static_cast<MacroAssembler::FPRegisterID>(reg + 1))
        result.set(reg);
    return result;
}

RegisterSet RegisterSet::allRegisters()
{
    RegisterSet result;
    result.merge(allGPRs());
    result.merge(allFPRs());
    return result;
}

void RegisterSet::dump(PrintStream& out) const
{
    m_vector.dump(out);
}

} // namespace JSC

#endif // ENABLE(JIT)

