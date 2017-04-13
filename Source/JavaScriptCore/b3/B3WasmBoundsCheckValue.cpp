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
#include "B3WasmBoundsCheckValue.h"
#include "WasmMemory.h"

#if ENABLE(B3_JIT)

namespace JSC { namespace B3 {

WasmBoundsCheckValue::~WasmBoundsCheckValue()
{
}

WasmBoundsCheckValue::WasmBoundsCheckValue(Origin origin, Value* ptr, GPRReg pinnedGPR, unsigned offset, PageCount maximum)
    : Value(CheckedOpcode, WasmBoundsCheck, origin, ptr)
    , m_pinnedGPR(pinnedGPR)
    , m_offset(offset)
    , m_maximum(maximum)
{
}

Value* WasmBoundsCheckValue::cloneImpl() const
{
    return new WasmBoundsCheckValue(*this);
}

size_t WasmBoundsCheckValue::redzoneLimit() const
{
    ASSERT(m_pinnedGPR == InvalidGPRReg);
#if ENABLE(WEBASSEMBLY)
    return static_cast<uint64_t>(std::numeric_limits<uint32_t>::max()) + Wasm::Memory::fastMappedRedzoneBytes();
#else
    RELEASE_ASSERT_NOT_REACHED();
#endif
}

void WasmBoundsCheckValue::dumpMeta(CommaPrinter& comma, PrintStream& out) const
{
    if (m_pinnedGPR == InvalidGPRReg)
        out.print(comma, "redzoneLimit = ", redzoneLimit(), ", offset = ", m_offset);
    else
        out.print(comma, "sizeRegister = ", m_pinnedGPR, ", offset = ", m_offset);
}

} } // namespace JSC::B3

#endif // ENABLE(B3_JIT)
