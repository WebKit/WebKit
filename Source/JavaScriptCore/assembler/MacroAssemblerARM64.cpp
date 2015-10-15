/*
 * Copyright (C) 2013-2015 Apple Inc. All rights reserved.
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

#if ENABLE(ASSEMBLER) && CPU(ARM64)
#include "MacroAssemblerARM64.h"

namespace JSC {

#if ENABLE(MASM_PROBE)

extern "C" void ctiMasmProbeTrampoline();

using namespace ARM64Registers;

static void arm64ProbeTrampoline(MacroAssemblerARM64::ProbeContext* context)
{
    void* origSP = context->cpu.sp;
    void* origPC = context->cpu.pc;
    
    context->probeFunction(context);
    
    if (context->cpu.sp != origSP) {
        dataLog("MacroAssembler probe ERROR: ARM64 does not support the probe changing the SP. The change will be ignored\n");
        context->cpu.sp = origSP;
    }
    if (context->cpu.pc != origPC) {
        dataLog("MacroAssembler probe ERROR: ARM64 does not support the probe changing the PC. The change will be ignored\n");
        context->cpu.pc = origPC;
    }
}

// For details on "What code is emitted for the probe?" and "What values are in
// the saved registers?", see comment for MacroAssemblerX86Common::probe() in
// MacroAssemblerX86Common.cpp.
void MacroAssemblerARM64::probe(MacroAssemblerARM64::ProbeFunction function, void* arg1, void* arg2)
{
    sub64(TrustedImm32(8 * 8), sp);

    store64(x27, Address(sp, 4 * 8));
    store64(x28, Address(sp, 5 * 8));
    store64(lr, Address(sp, 6 * 8));

    add64(TrustedImm32(8 * 8), sp, x28);
    store64(x28, Address(sp, 7 * 8)); // Save original sp value.

    move(TrustedImmPtr(reinterpret_cast<void*>(function)), x28);
    store64(x28, Address(sp));
    move(TrustedImmPtr(arg1), x28);
    store64(x28, Address(sp, 1 * 8));
    move(TrustedImmPtr(arg2), x28);
    store64(x28, Address(sp, 2 * 8));
    move(TrustedImmPtr(reinterpret_cast<void*>(arm64ProbeTrampoline)), x28);
    store64(x28, Address(sp, 3 * 8));

    move(TrustedImmPtr(reinterpret_cast<void*>(ctiMasmProbeTrampoline)), x28);
    m_assembler.blr(x28);

    // ctiMasmProbeTrampoline should have restored every register except for
    // lr and the sp.
    load64(Address(sp, 6 * 8), lr);
    add64(TrustedImm32(8 * 8), sp);
}
#endif // ENABLE(MASM_PROBE)

} // namespace JSC

#endif // ENABLE(ASSEMBLER) && CPU(ARM64)

