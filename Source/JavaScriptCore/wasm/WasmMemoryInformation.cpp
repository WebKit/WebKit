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
#include "WasmMemoryInformation.h"

#include "WasmCallingConvention.h"

#if ENABLE(WEBASSEMBLY)

namespace JSC { namespace Wasm {

MemoryInformation::MemoryInformation(PageCount initial, PageCount maximum,  const Vector<unsigned>& pinnedSizeRegisters, bool isImport)
    : m_initial(initial)
    , m_maximum(maximum)
    , m_isImport(isImport)
{
    RELEASE_ASSERT(!!m_initial);
    RELEASE_ASSERT(!m_maximum || m_maximum >= m_initial);
    ASSERT(!!*this);

    unsigned remainingPinnedRegisters = pinnedSizeRegisters.size() + 1;
    jscCallingConvention().m_calleeSaveRegisters.forEach([&] (Reg reg) {
        GPRReg gpr = reg.gpr();
        if (!remainingPinnedRegisters || RegisterSet::stackRegisters().get(reg))
            return;
        if (remainingPinnedRegisters == 1) {
            m_pinnedRegisters.baseMemoryPointer = gpr;
            remainingPinnedRegisters--;
        } else
            m_pinnedRegisters.sizeRegisters.append({ gpr, pinnedSizeRegisters[--remainingPinnedRegisters - 1] });
    });

    ASSERT(!remainingPinnedRegisters);
}

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
