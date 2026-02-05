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

#include "B3Value.h"
#include "WasmTypeDefinition.h"
#include <wtf/StdLibExtras.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC::B3 {

class WasmStructNewValue final : public Value {
public:
    static bool accepts(Kind kind) { return kind.opcode() == WasmStructNew; }

    ~WasmStructNewValue() final;

    // Child 0 is the instance pointer (Int64)
    // Child 1 is the structureID (Int32)
    Value* instance() const { return child(0); }
    Value* structureID() const { return child(1); }

    Ref<const Wasm::RTT> rtt() const { return m_rtt; }
    const Wasm::StructType* structType() const { return m_structType; }
    uint32_t typeIndex() const { return m_typeIndex; }

    // Offset from instance for allocator lookup (pre-computed from ModuleInformation)
    int32_t allocatorsBaseOffset() const { return m_allocatorsBaseOffset; }

    B3_SPECIALIZE_VALUE_FOR_FIXED_CHILDREN(2)
    B3_SPECIALIZE_VALUE_FOR_FINAL_SIZE_FIXED_CHILDREN

private:
    void dumpMeta(CommaPrinter&, PrintStream&) const final;

    friend class Procedure;
    friend class Value;

    template<typename... Arguments>
    static Opcode opcodeFromConstructor(Arguments...) { return WasmStructNew; }

    WasmStructNewValue(Origin origin, Type resultType, Ref<const Wasm::RTT> rtt, const Wasm::StructType* structType, uint32_t typeIndex, int32_t allocatorsBaseOffset, Value* instance, Value* structureID)
        : Value(CheckedOpcode, WasmStructNew, resultType, Two, origin, instance, structureID)
        , m_rtt(WTF::move(rtt))
        , m_structType(structType)
        , m_typeIndex(typeIndex)
        , m_allocatorsBaseOffset(allocatorsBaseOffset)
    {
    }

    Ref<const Wasm::RTT> m_rtt;
    SUPPRESS_UNCOUNTED_MEMBER const Wasm::StructType* m_structType;
    uint32_t m_typeIndex;
    int32_t m_allocatorsBaseOffset;
};

} // namespace JSC::B3

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif // ENABLE(B3_JIT)
