/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#if ENABLE(B3_JIT)

#include "B3Effects.h"
#include "B3HeapRange.h"
#include "B3Value.h"
#include "WasmTypeDefinition.h"

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC::B3 {

class WasmStructFieldValue : public Value {
public:
    static bool accepts(Kind kind) { return kind.opcode() == WasmStructGet || kind.opcode() == WasmStructSet; }

    ~WasmStructFieldValue() override;

    Ref<const Wasm::RTT> rtt() const { return m_rtt; }
    const Wasm::StructType* structType() const { return m_structType; }
    Wasm::StructFieldCount fieldIndex() const { return m_fieldIndex; }
    uint64_t fieldHeapKey() const { return m_fieldHeapKey; }

    const HeapRange& range() const { return m_range; }
    void setRange(HeapRange range) { m_range = range; }
    Mutability mutability() const { return m_mutability; }

protected:
    template<typename... Arguments>
    WasmStructFieldValue(CheckedOpcodeTag tag, Kind kind, Type type, NumChildren numChildren, Origin origin, Ref<const Wasm::RTT> rtt, const Wasm::StructType* structType, Wasm::StructFieldCount fieldIndex, uint64_t fieldHeapKey, Mutability mutability, Arguments... arguments)
        : Value(tag, kind, type, numChildren, origin, arguments...)
        , m_rtt(WTF::move(rtt))
        , m_structType(structType)
        , m_fieldIndex(fieldIndex)
        , m_fieldHeapKey(fieldHeapKey)
        , m_mutability(mutability)
    {
    }

    const Ref<const Wasm::RTT> m_rtt;
    SUPPRESS_UNCOUNTED_MEMBER const Wasm::StructType* m_structType;
    Wasm::StructFieldCount m_fieldIndex;
    uint64_t m_fieldHeapKey;
    HeapRange m_range { HeapRange::top() };
    Mutability m_mutability;
};

} // namespace JSC::B3

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif // ENABLE(B3_JIT)
