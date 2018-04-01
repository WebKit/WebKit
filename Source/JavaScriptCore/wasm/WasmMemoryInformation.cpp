/*
 * Copyright (C) 2016-2017 Apple Inc. All rights reserved.
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
#include "WasmMemoryInformation.h"

#if ENABLE(WEBASSEMBLY)

#include "WasmCallingConvention.h"
#include "WasmContext.h"
#include "WasmMemory.h"
#include <wtf/NeverDestroyed.h>

namespace JSC { namespace Wasm {

static Vector<GPRReg> getPinnedRegisters(unsigned remainingPinnedRegisters)
{
    Vector<GPRReg> registers;
    jscCallingConvention().m_calleeSaveRegisters.forEach([&] (Reg reg) {
        if (!reg.isGPR())
            return;
        GPRReg gpr = reg.gpr();
        if (!remainingPinnedRegisters || RegisterSet::stackRegisters().get(reg))
            return;
        --remainingPinnedRegisters;
        registers.append(gpr);
    });
    return registers;
}

const PinnedRegisterInfo& PinnedRegisterInfo::get()
{
    static LazyNeverDestroyed<PinnedRegisterInfo> staticPinnedRegisterInfo;
    static std::once_flag staticPinnedRegisterInfoFlag;
    std::call_once(staticPinnedRegisterInfoFlag, [] () {
        Vector<PinnedSizeRegisterInfo> sizeRegisters;
        GPRReg baseMemoryPointer = InvalidGPRReg;
        GPRReg wasmContextInstancePointer = InvalidGPRReg;

        // FIXME: We should support more than one memory size register, and we should allow different
        //        WebAssembly.Instance to have different pins. Right now we take a vector with only one entry.
        //        If we have more than one size register, we can have one for each load size class.
        //        see: https://bugs.webkit.org/show_bug.cgi?id=162952
        Vector<unsigned> pinnedSizes = { 0 };
        unsigned numberOfPinnedRegisters = pinnedSizes.size() + 1;
        if (!Context::useFastTLS())
            ++numberOfPinnedRegisters;
        Vector<GPRReg> pinnedRegs = getPinnedRegisters(numberOfPinnedRegisters);

        baseMemoryPointer = pinnedRegs.takeLast();
        if (!Context::useFastTLS())
            wasmContextInstancePointer = pinnedRegs.takeLast();

        ASSERT(pinnedSizes.size() == pinnedRegs.size());
        for (unsigned i = 0; i < pinnedSizes.size(); ++i)
            sizeRegisters.append({ pinnedRegs[i], pinnedSizes[i] });
        staticPinnedRegisterInfo.construct(WTFMove(sizeRegisters), baseMemoryPointer, wasmContextInstancePointer);
    });

    return staticPinnedRegisterInfo.get();
}

PinnedRegisterInfo::PinnedRegisterInfo(Vector<PinnedSizeRegisterInfo>&& sizeRegisters, GPRReg baseMemoryPointer, GPRReg wasmContextInstancePointer)
    : sizeRegisters(WTFMove(sizeRegisters))
    , baseMemoryPointer(baseMemoryPointer)
    , wasmContextInstancePointer(wasmContextInstancePointer)
{
}

MemoryInformation::MemoryInformation(PageCount initial, PageCount maximum, bool isImport)
    : m_initial(initial)
    , m_maximum(maximum)
    , m_isImport(isImport)
{
    RELEASE_ASSERT(!!m_initial);
    RELEASE_ASSERT(!m_maximum || m_maximum >= m_initial);
    ASSERT(!!*this);
}

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
