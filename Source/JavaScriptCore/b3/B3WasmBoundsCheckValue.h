/*
 * Copyright (C) 2015-2017 Apple Inc. All rights reserved.
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
#include "CCallHelpers.h"

namespace JSC { namespace B3 {

class WasmBoundsCheckValue : public Value {
public:
    static bool accepts(Kind kind) { return kind == WasmBoundsCheck; }
    
    ~WasmBoundsCheckValue();

    enum class Type {
        Pinned,
        Maximum,
    };

    union Bounds {
        GPRReg pinnedSize;
        size_t maximum;
    };

    unsigned offset() const { return m_offset; }
    Type boundsType() const { return m_boundsType; }
    Bounds bounds() const { return m_bounds; }

    B3_SPECIALIZE_VALUE_FOR_FIXED_CHILDREN(1)
    B3_SPECIALIZE_VALUE_FOR_FINAL_SIZE_FIXED_CHILDREN

protected:
    void dumpMeta(CommaPrinter&, PrintStream&) const override;

private:
    friend class Procedure;
    friend class Value;

    static Opcode opcodeFromConstructor(Origin, GPRReg, Value*, unsigned) { return WasmBoundsCheck; }
    JS_EXPORT_PRIVATE WasmBoundsCheckValue(Origin, GPRReg pinnedGPR, Value* ptr, unsigned offset);

    static Opcode opcodeFromConstructor(Origin, Value*, unsigned, size_t) { return WasmBoundsCheck; }
    JS_EXPORT_PRIVATE WasmBoundsCheckValue(Origin, Value* ptr, unsigned offset, size_t maximum);

    unsigned m_offset;
    Type m_boundsType;
    Bounds m_bounds;

};

} } // namespace JSC::B3

#endif // ENABLE(B3_JIT)
