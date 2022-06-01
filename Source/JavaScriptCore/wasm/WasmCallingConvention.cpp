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
        RegisterSet callerSaveRegisters = RegisterSet::allRegisters();
        callerSaveRegisters.exclude(RegisterSet::calleeSaveRegisters());

        staticJSCallingConvention.construct(Vector<Reg>(), Vector<Reg>(), RegisterSet::calleeSaveRegisters(), WTFMove(callerSaveRegisters));
    });

    return staticJSCallingConvention;
}

const WasmCallingConvention& wasmCallingConvention()
{
    static LazyNeverDestroyed<WasmCallingConvention> staticWasmCallingConvention;
    static std::once_flag staticWasmCallingConventionFlag;
    std::call_once(staticWasmCallingConventionFlag, [] () {
        Vector<Reg> gprArgumentRegisters(GPRInfo::numberOfArgumentRegisters);
        for (unsigned i = 0; i < GPRInfo::numberOfArgumentRegisters; ++i)
            gprArgumentRegisters[i] = GPRInfo::toArgumentRegister(i);

        Vector<Reg> fprArgumentRegisters(FPRInfo::numberOfArgumentRegisters);
        for (unsigned i = 0; i < FPRInfo::numberOfArgumentRegisters; ++i)
            fprArgumentRegisters[i] = FPRInfo::toArgumentRegister(i);

        RegisterSet scratch = RegisterSet::allGPRs();
        scratch.exclude(RegisterSet::calleeSaveRegisters());
        scratch.exclude(RegisterSet::macroScratchRegisters());
        scratch.exclude(RegisterSet::reservedHardwareRegisters());
        scratch.exclude(RegisterSet::stackRegisters());
        for (Reg reg : gprArgumentRegisters)
            scratch.clear(reg);

        Vector<GPRReg> scratchGPRs;
        for (Reg reg : scratch)
            scratchGPRs.append(reg.gpr());
        RELEASE_ASSERT(scratchGPRs.size() >= 2);

        RegisterSet callerSaveRegisters = RegisterSet::allRegisters();
        callerSaveRegisters.exclude(RegisterSet::calleeSaveRegisters());

        staticWasmCallingConvention.construct(WTFMove(gprArgumentRegisters), WTFMove(fprArgumentRegisters), WTFMove(scratchGPRs), RegisterSet::calleeSaveRegisters(), WTFMove(callerSaveRegisters));
    });

    return staticWasmCallingConvention;
}

} // namespace JSC::Wasm

#endif // ENABLE(B3_JIT)
