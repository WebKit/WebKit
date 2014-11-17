/*
 * Copyright (C) 2013, 2014 Apple Inc. All rights reserved.
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

#if ENABLE(ASSEMBLER) && (CPU(X86) || CPU(X86_64))
#include "MacroAssemblerX86Common.h"

namespace JSC {

#if ENABLE(MASM_PROBE)

#define INDENT printIndent(indentation)

void MacroAssemblerX86Common::printCPURegisters(MacroAssemblerX86Common::CPUState& cpu, int indentation)
{
#if CPU(X86)
    #define PRINT_GPREGISTER(_type, _regName) { \
        int32_t value = reinterpret_cast<int32_t>(cpu._regName); \
        INDENT, dataLogF("%6s: 0x%08x  %d\n", #_regName, value, value) ; \
    }
#elif CPU(X86_64)
    #define PRINT_GPREGISTER(_type, _regName) { \
        int64_t value = reinterpret_cast<int64_t>(cpu._regName); \
        INDENT, dataLogF("%6s: 0x%016llx  %lld\n", #_regName, value, value) ; \
    }
#endif
    FOR_EACH_CPU_GPREGISTER(PRINT_GPREGISTER)
    FOR_EACH_CPU_SPECIAL_REGISTER(PRINT_GPREGISTER)
    #undef PRINT_GPREGISTER

    #define PRINT_FPREGISTER(_type, _regName) { \
        uint64_t* u = reinterpret_cast<uint64_t*>(&cpu._regName); \
        double* d = reinterpret_cast<double*>(&cpu._regName); \
        INDENT, dataLogF("%6s: 0x%016llx  %.13g\n", #_regName, *u, *d); \
    }
    FOR_EACH_CPU_FPREGISTER(PRINT_FPREGISTER)
    #undef PRINT_FPREGISTER
}

#undef INDENT

void MacroAssemblerX86Common::printRegister(MacroAssemblerX86Common::CPUState& cpu, RegisterID regID)
{
    const char* name = CPUState::registerName(regID);
    union {
        void* voidPtr;
        intptr_t intptrValue;
    } u;
    u.voidPtr = cpu.registerValue(regID);
    dataLogF("%s:<%p %ld>", name, u.voidPtr, u.intptrValue);
}

void MacroAssemblerX86Common::printRegister(MacroAssemblerX86Common::CPUState& cpu, FPRegisterID regID)
{
    const char* name = CPUState::registerName(regID);
    union {
        double doubleValue;
        uint64_t uint64Value;
    } u;
    u.doubleValue = cpu.registerValue(regID);
    dataLogF("%s:<0x%016llx %.13g>", name, u.uint64Value, u.doubleValue);
}

extern "C" void ctiMasmProbeTrampoline();

// What code is emitted for the probe?
// ==================================
// We want to keep the size of the emitted probe invocation code as compact as
// possible to minimize the perturbation to the JIT generated code. However,
// we also need to preserve the CPU registers and set up the ProbeContext to be
// passed to the user probe function.
//
// Hence, we do only the minimum here to preserve a scratch register (i.e. rax
// in this case) and the stack pointer (i.e. rsp), and pass the probe arguments.
// We'll let the ctiMasmProbeTrampoline handle the rest of the probe invocation
// work i.e. saving the CPUState (and setting up the ProbeContext), calling the
// user probe function, and restoring the CPUState before returning to JIT
// generated code.
//
// What registers need to be saved?
// ===============================
// The registers are saved for 2 reasons:
// 1. To preserve their state in the JITted code. This means that all registers
//    that are not callee saved needs to be saved. We also need to save the
//    condition code registers because the probe can be inserted between a test
//    and a branch.
// 2. To allow the probe to inspect the values of the registers for debugging
//    purposes. This means all registers need to be saved.
//
// In summary, save everything. But for reasons stated above, we should do the
// minimum here and let ctiMasmProbeTrampoline do the heavy lifting to save the
// full set.
//
// What values are in the saved registers?
// ======================================
// Conceptually, the saved registers should contain values as if the probe
// is not present in the JIT generated code. Hence, they should contain values
// that are expected at the start of the instruction immediately following the
// probe.
//
// Specifically, the saved stack pointer register will point to the stack
// position before we push the ProbeContext frame. The saved rip will point to
// the address of the instruction immediately following the probe. 

void MacroAssemblerX86Common::probe(MacroAssemblerX86Common::ProbeFunction function, void* arg1, void* arg2)
{
    push(RegisterID::esp);
    push(RegisterID::eax);
    move(TrustedImmPtr(arg2), RegisterID::eax);
    push(RegisterID::eax);
    move(TrustedImmPtr(arg1), RegisterID::eax);
    push(RegisterID::eax);
    move(TrustedImmPtr(reinterpret_cast<void*>(function)), RegisterID::eax);
    push(RegisterID::eax);
    move(TrustedImmPtr(reinterpret_cast<void*>(ctiMasmProbeTrampoline)), RegisterID::eax);
    call(RegisterID::eax);
}

#endif // ENABLE(MASM_PROBE)

#if CPU(X86) && !OS(MAC_OS_X)
MacroAssemblerX86Common::SSE2CheckState MacroAssemblerX86Common::s_sse2CheckState = NotCheckedSSE2;
#endif

} // namespace JSC

#endif // ENABLE(ASSEMBLER) && (CPU(X86) || CPU(X86_64))
