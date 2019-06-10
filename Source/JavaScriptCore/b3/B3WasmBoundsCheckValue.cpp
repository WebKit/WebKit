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

#include "B3ValueInlines.h"

namespace JSC { namespace B3 {

WasmBoundsCheckValue::~WasmBoundsCheckValue()
{
}

WasmBoundsCheckValue::WasmBoundsCheckValue(Origin origin, GPRReg pinnedSize, Value* ptr, unsigned offset)
    : Value(CheckedOpcode, WasmBoundsCheck, One, origin, ptr)
    , m_offset(offset)
    , m_boundsType(Type::Pinned)
{
    m_bounds.pinnedSize = pinnedSize;
}

WasmBoundsCheckValue::WasmBoundsCheckValue(Origin origin, Value* ptr, unsigned offset, size_t maximum)
    : Value(CheckedOpcode, WasmBoundsCheck, One, origin, ptr)
    , m_offset(offset)
    , m_boundsType(Type::Maximum)
{
#if ENABLE(WEBASSEMBLY)
    size_t redzoneLimit = static_cast<uint64_t>(std::numeric_limits<uint32_t>::max()) + Wasm::Memory::fastMappedRedzoneBytes();
    ASSERT_UNUSED(redzoneLimit, maximum <= redzoneLimit);
#endif
    m_bounds.maximum = maximum;
}

void WasmBoundsCheckValue::dumpMeta(CommaPrinter& comma, PrintStream& out) const
{
    switch (m_boundsType) {
    case Type::Pinned:
        out.print(comma, "offset = ", m_offset, comma, "pinnedSize = ", m_bounds.pinnedSize);
        break;
    case Type::Maximum:
        out.print(comma, "offset = ", m_offset, comma, "maximum = ", m_bounds.maximum);
        break;
    }
}

} } // namespace JSC::B3

#endif // ENABLE(B3_JIT)
