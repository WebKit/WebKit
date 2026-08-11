/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#include <wtf/PrintStream.h>
#include <wtf/TaggedPtr.h>
#include <wtf/ValidatedReinterpretCast.h>

namespace JSC::Wasm {
class WasmOrigin;
class OpcodeOrigin;
}

namespace JSC::DFG {
struct Node;
}

namespace JSC { namespace B3 {

// Whoever generates B3IR can choose to put origins on values. When you do this, B3 will be able to
// account, down to the machine code, which instruction corresponds to which origin. B3
// transformations must preserve Origins carefully. It's an error to write a transformation that
// either drops Origins or lies about them.
class Origin {
public:
    enum OriginType : uint8_t {
        InvalidTag,
        DFGOriginPtr,
        WasmOriginPtr,
        PackedWasmOrigin,
    };

    explicit Origin(const Wasm::WasmOrigin* data)
        : m_data(data, WasmOriginPtr)
    {
    }

    explicit Origin(const DFG::Node* data)
        : m_data(data, DFGOriginPtr)
    {
    }

    explicit Origin()
        : m_data(nullptr)
    { }

    explicit operator bool() const { return !!m_data.bits(); }

    bool isDFGOrigin() const { return !m_data.bits() || m_data.tag() == DFGOriginPtr; }
    bool isWasmOrigin() const { return !m_data.bits() || m_data.tag() == WasmOriginPtr; }
    bool isPackedWasmOrigin() const { return !m_data.bits() || m_data.tag() == PackedWasmOrigin; }

    Wasm::WasmOrigin* wasmOrigin() const
    {
        ASSERT(isWasmOrigin());
        return VALIDATED_REINTERPRET_CAST("WasmOrigin", Wasm::WasmOrigin, m_data.ptr());
    }

    DFG::Node* dfgOrigin() const
    {
        ASSERT(isDFGOrigin());
        return std::bit_cast<DFG::Node*>(m_data.ptr());
    }

    const Wasm::WasmOrigin* maybeWasmOrigin() const { return isWasmOrigin() ? wasmOrigin() : nullptr; }

    // You should avoid using this. Use OriginDump instead.
    void dump(PrintStream&) const;

private:

    // See WasmOpcodeOrigin for packing details.
    explicit Origin(uint64_t data)
        : m_data(data, PackedWasmOrigin)
    {
    }

    WTF::TaggedBits60 m_data;
    friend class Wasm::OpcodeOrigin;
};

} } // namespace JSC::B3

#endif // ENABLE(B3_JIT)
