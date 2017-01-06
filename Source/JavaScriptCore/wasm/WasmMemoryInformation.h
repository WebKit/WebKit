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

#pragma once

#if ENABLE(WEBASSEMBLY)

#include "GPRInfo.h"
#include "WasmPageCount.h"
#include <wtf/Vector.h>

namespace JSC { namespace Wasm {

struct PinnedSizeRegisterInfo {
    GPRReg sizeRegister;
    unsigned sizeOffset;
};

struct PinnedRegisterInfo {
    Vector<PinnedSizeRegisterInfo> sizeRegisters;
    GPRReg baseMemoryPointer;
    static const PinnedRegisterInfo& get();
    PinnedRegisterInfo(Vector<PinnedSizeRegisterInfo>&&, GPRReg);
};

class MemoryInformation {
public:
    MemoryInformation()
    {
        ASSERT(!*this);
    }

    MemoryInformation(PageCount initial, PageCount maximum, bool isImport);

    PageCount initial() const { return m_initial; }
    PageCount maximum() const { return m_maximum; }
    bool isImport() const { return m_isImport; }

    explicit operator bool() const { return !!m_initial; }

private:
    PageCount m_initial { };
    PageCount m_maximum { };
    bool m_isImport { false };
};

} } // namespace JSC::Wasm

#endif // ENABLE(WASM)
