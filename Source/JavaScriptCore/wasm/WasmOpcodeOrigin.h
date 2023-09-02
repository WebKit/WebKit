/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#if ENABLE(WEBASSEMBLY_OMGJIT)

#include "B3Origin.h"
#include "WasmFormat.h"

#include <wtf/ForbidHeapAllocation.h>

namespace JSC { namespace Wasm {

class OpcodeOrigin {
    WTF_FORBID_HEAP_ALLOCATION;

    OpcodeOrigin() = default;

public:
    void dump(PrintStream&) const;

#if USE(JSVALUE64)
    OpcodeOrigin(OpType opcode, size_t offset)
    {
        ASSERT(static_cast<uint32_t>(offset) == offset);
        packedData = (static_cast<uint64_t>(opcode) << 32) | offset;
    }
    OpcodeOrigin(B3::Origin origin)
        : packedData(bitwise_cast<uint64_t>(origin))
    {
    }

    OpType opcode() const { return static_cast<OpType>(packedData >> 32); }
    size_t location() const { return static_cast<uint32_t>(packedData); }

private:
    static_assert(sizeof(void*) == sizeof(uint64_t), "this packing doesn't work if this isn't the case");
    uint64_t packedData { 0 };

#elif USE(JSVALUE32_64)
    OpcodeOrigin(B3::Origin) { UNREACHABLE_FOR_PLATFORM(); }

    OpType opcode() const { UNREACHABLE_FOR_PLATFORM(); }
    size_t location() const { UNREACHABLE_FOR_PLATFORM(); }
#endif
};

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY_OMGJIT)
