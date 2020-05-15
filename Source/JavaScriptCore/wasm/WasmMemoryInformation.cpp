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

#include "WasmContextInlines.h"
#include <wtf/NeverDestroyed.h>

namespace JSC { namespace Wasm {

const PinnedRegisterInfo& PinnedRegisterInfo::get()
{
    static LazyNeverDestroyed<PinnedRegisterInfo> staticPinnedRegisterInfo;
    static std::once_flag staticPinnedRegisterInfoFlag;
    std::call_once(staticPinnedRegisterInfoFlag, [] () {
        unsigned numberOfPinnedRegisters = 2;
        if (!Context::useFastTLS())
            ++numberOfPinnedRegisters;
        GPRReg baseMemoryPointer = GPRInfo::regCS3;
        GPRReg sizeRegister = GPRInfo::regCS4;
        GPRReg wasmContextInstancePointer = InvalidGPRReg;
        if (!Context::useFastTLS())
            wasmContextInstancePointer = GPRInfo::regCS0;

        staticPinnedRegisterInfo.construct(sizeRegister, baseMemoryPointer, wasmContextInstancePointer);
    });

    return staticPinnedRegisterInfo.get();
}

PinnedRegisterInfo::PinnedRegisterInfo(GPRReg sizeRegister, GPRReg baseMemoryPointer, GPRReg wasmContextInstancePointer)
    : sizeRegister(sizeRegister)
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
