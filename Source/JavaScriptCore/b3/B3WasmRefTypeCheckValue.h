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
#include <wtf/OptionSet.h>
#include <wtf/RefCounted.h>
#include <wtf/StdLibExtras.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC::B3 {

enum class WasmRefTypeCheckFlag : uint8_t {
    AllowNull = 1 << 0,
    ShouldNegate = 1 << 1, // WasmRefTest only
    ReferenceIsNullable = 1 << 2,
    DefinitelyIsCellOrNull = 1 << 3,
    DefinitelyIsWasmGCObjectOrNull = 1 << 4,
    HasRTT = 1 << 5, // Whether m_targetRTT is non-null (vs using targetHeapType)
};

class WasmRefTypeCheckValue final : public Value {
public:
    static bool accepts(Kind kind) { return kind.opcode() == WasmRefCast || kind.opcode() == WasmRefTest; }

    ~WasmRefTypeCheckValue();

    int32_t targetHeapType() const { return m_targetHeapType; }
    bool allowNull() const { return m_flags.contains(WasmRefTypeCheckFlag::AllowNull); }
    bool referenceIsNullable() const { return m_flags.contains(WasmRefTypeCheckFlag::ReferenceIsNullable); }
    bool definitelyIsCellOrNull() const { return m_flags.contains(WasmRefTypeCheckFlag::DefinitelyIsCellOrNull); }
    bool definitelyIsWasmGCObjectOrNull() const { return m_flags.contains(WasmRefTypeCheckFlag::DefinitelyIsWasmGCObjectOrNull); }
    bool shouldNegate() const { return m_flags.contains(WasmRefTypeCheckFlag::ShouldNegate); }
    const Wasm::RTT* targetRTT() const { return m_targetRTT.get(); }
    OptionSet<WasmRefTypeCheckFlag> flags() const { return m_flags; }

    B3_SPECIALIZE_VALUE_FOR_FIXED_CHILDREN(1)
    B3_SPECIALIZE_VALUE_FOR_FINAL_SIZE_FIXED_CHILDREN

private:
    void dumpMeta(CommaPrinter&, PrintStream&) const final;

protected:
    friend class Procedure;
    friend class Value;

    template<typename... Arguments>
    WasmRefTypeCheckValue(Kind kind, Type type, Origin origin, int32_t targetHeapType, OptionSet<WasmRefTypeCheckFlag> flags, RefPtr<const Wasm::RTT> targetRTT, Arguments... arguments)
        : Value(CheckedOpcode, kind, type, One, origin, arguments...)
        , m_targetHeapType(targetHeapType)
        , m_flags(flags)
        , m_targetRTT(WTF::move(targetRTT))
    {
    }

    int32_t m_targetHeapType;
    OptionSet<WasmRefTypeCheckFlag> m_flags;
    RefPtr<const Wasm::RTT> m_targetRTT;
};

} // namespace JSC::B3

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif // ENABLE(B3_JIT)
