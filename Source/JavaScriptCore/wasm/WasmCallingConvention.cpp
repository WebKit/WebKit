/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#include "WasmCallingConvention.h"

#if ENABLE(WEBASSEMBLY)

#include <wtf/NeverDestroyed.h>

namespace JSC::Wasm {

const JSCallingConvention& jsCallingConvention()
{
    static LazyNeverDestroyed<JSCallingConvention> staticJSCallingConvention;
    static std::once_flag staticJSCCallingConventionFlag;
    std::call_once(staticJSCCallingConventionFlag, [] () {
        staticJSCallingConvention.construct(Vector<JSValueRegs>(), Vector<FPRReg>(), RegisterSetBuilder::calleeSaveRegisters());
    });

    return staticJSCallingConvention;
}

const WasmCallingConvention& wasmCallingConvention()
{
    static LazyNeverDestroyed<WasmCallingConvention> staticWasmCallingConvention;
    static std::once_flag staticWasmCallingConventionFlag;
    std::call_once(staticWasmCallingConventionFlag, [] () {
#if USE(JSVALUE64) // One value per GPR
        constexpr unsigned numberOfArgumentJSRs = GPRInfo::numberOfArgumentRegisters;
#elif USE(JSVALUE32_64) // One value per consecutive GPR pair
        constexpr unsigned numberOfArgumentJSRs = GPRInfo::numberOfArgumentRegisters / 2;
#endif
        Vector<JSValueRegs> jsrArgumentRegisters(numberOfArgumentJSRs);
        for (unsigned i = 0; i < numberOfArgumentJSRs; ++i) {
#if USE(JSVALUE64)
            jsrArgumentRegisters[i] = JSValueRegs { GPRInfo::toArgumentRegister(i) };
#elif USE(JSVALUE32_64)
            jsrArgumentRegisters[i] = JSValueRegs { GPRInfo::toArgumentRegister(2 * i + 1), GPRInfo::toArgumentRegister(2 * i) };
#endif
        }

        Vector<FPRReg> fprArgumentRegisters(FPRInfo::numberOfArgumentRegisters);
        for (unsigned i = 0; i < FPRInfo::numberOfArgumentRegisters; ++i)
            fprArgumentRegisters[i] = FPRInfo::toArgumentRegister(i);

        RegisterSetBuilder scratch = RegisterSetBuilder::allGPRs();
        scratch.exclude(RegisterSetBuilder::vmCalleeSaveRegisters().includeWholeRegisterWidth());
        scratch.exclude(RegisterSetBuilder::macroClobberedGPRs());
        scratch.exclude(RegisterSetBuilder::reservedHardwareRegisters());
        scratch.exclude(RegisterSetBuilder::stackRegisters());
        for (JSValueRegs jsr : jsrArgumentRegisters) {
            scratch.remove(jsr.payloadGPR());
#if USE(JSVALUE32_64)
            scratch.remove(jsr.tagGPR());
#endif
        }

        Vector<GPRReg> scratchGPRs;
        for (Reg reg : scratch.buildAndValidate())
            scratchGPRs.append(reg.gpr());

        // Need at least one JSValue and an additional GPR
#if USE(JSVALUE64)
        RELEASE_ASSERT(scratchGPRs.size() >= 2);
#elif USE(JSVALUE32_64)
        RELEASE_ASSERT(scratchGPRs.size() >= 3);
#endif

        staticWasmCallingConvention.construct(WTFMove(jsrArgumentRegisters), WTFMove(fprArgumentRegisters), WTFMove(scratchGPRs), RegisterSetBuilder::calleeSaveRegisters());
    });

    return staticWasmCallingConvention;
}

#if CPU(ARM_THUMB2)

const CCallingConventionArmThumb2& cCallingConventionArmThumb2()
{
    static LazyNeverDestroyed<CCallingConventionArmThumb2> staticCCallingConventionArmThumb2;
    static std::once_flag staticCCallingConventionArmThumb2Flag;
    std::call_once(staticCCallingConventionArmThumb2Flag, [] () {
        constexpr unsigned numberOfArgumentGPRs = GPRInfo::numberOfArgumentRegisters;
        Vector<GPRReg> gprArgumentRegisters(numberOfArgumentGPRs);
        for (unsigned i = 0; i < numberOfArgumentGPRs; ++i)
            gprArgumentRegisters[i] = GPRInfo::toArgumentRegister(i);

        Vector<FPRReg> fprArgumentRegisters(FPRInfo::numberOfArgumentRegisters);
        for (unsigned i = 0; i < FPRInfo::numberOfArgumentRegisters; ++i)
            fprArgumentRegisters[i] = FPRInfo::toArgumentRegister(i);

        RegisterSetBuilder scratch = RegisterSetBuilder::allGPRs();
        scratch.exclude(RegisterSetBuilder::vmCalleeSaveRegisters().includeWholeRegisterWidth());
        scratch.exclude(RegisterSetBuilder::macroClobberedGPRs());
        scratch.exclude(RegisterSetBuilder::reservedHardwareRegisters());
        scratch.exclude(RegisterSetBuilder::stackRegisters());
        for (GPRReg gpr : gprArgumentRegisters)
            scratch.remove(gpr);

        Vector<GPRReg> scratchGPRs;
        for (Reg reg : scratch.buildAndValidate())
            scratchGPRs.append(reg.gpr());

        // Need at least one JSValue and an additional GPR
        RELEASE_ASSERT(scratchGPRs.size() >= 3);

        staticCCallingConventionArmThumb2.construct(WTFMove(gprArgumentRegisters), WTFMove(fprArgumentRegisters), WTFMove(scratchGPRs), RegisterSetBuilder::calleeSaveRegisters());
    });

    return staticCCallingConventionArmThumb2;
}

#endif

} // namespace JSC::Wasm

#endif // ENABLE(B3_JIT)
