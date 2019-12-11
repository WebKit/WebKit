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

#if ENABLE(B3_JIT)

#include "B3MemoryValue.h"
#include "B3Width.h"

namespace JSC { namespace B3 {

class JS_EXPORT_PRIVATE AtomicValue : public MemoryValue {
public:
    static bool accepts(Kind kind)
    {
        return isAtom(kind.opcode());
    }
    
    ~AtomicValue();
    
    Type accessType() const { return child(0)->type(); }
    
    Width accessWidth() const { return m_width; }

    B3_SPECIALIZE_VALUE_FOR_FINAL_SIZE_FIXED_CHILDREN
    
protected:
    void dumpMeta(CommaPrinter&, PrintStream&) const override;

private:
    friend class Procedure;
    friend class Value;

    enum AtomicValueRMW { AtomicValueRMWTag };
    enum AtomicValueCAS { AtomicValueCASTag };

    AtomicValue(Kind kind, Origin origin, Width width, Value* operand, Value* pointer)
        : AtomicValue(kind, origin, width, operand, pointer, 0)
    {
    }
    template<typename Int,
        typename = typename std::enable_if<std::is_integral<Int>::value>::type,
        typename = typename std::enable_if<std::is_signed<Int>::value>::type,
        typename = typename std::enable_if<sizeof(Int) <= sizeof(OffsetType)>::type
    >
    AtomicValue(Kind kind, Origin origin, Width width, Value* operand, Value* pointer, Int offset, HeapRange range = HeapRange::top(), HeapRange fenceRange = HeapRange::top())
        : AtomicValue(AtomicValueRMWTag, kind, origin, width, operand, pointer, offset, range, fenceRange)
    {
    }

    AtomicValue(Kind kind, Origin origin, Width width, Value* expectedValue, Value* newValue, Value* pointer)
        : AtomicValue(kind, origin, width, expectedValue, newValue, pointer, 0)
    {
    }
    template<typename Int,
        typename = typename std::enable_if<std::is_integral<Int>::value>::type,
        typename = typename std::enable_if<std::is_signed<Int>::value>::type,
        typename = typename std::enable_if<sizeof(Int) <= sizeof(OffsetType)>::type
    >
    AtomicValue(Kind kind, Origin origin, Width width, Value* expectedValue, Value* newValue, Value* pointer, Int offset, HeapRange range = HeapRange::top(), HeapRange fenceRange = HeapRange::top())
        : AtomicValue(AtomicValueCASTag, kind, origin, width, expectedValue, newValue, pointer, offset, range, fenceRange)
    {
    }

    // The above templates forward to these implementations.
    AtomicValue(AtomicValueRMW, Kind, Origin, Width, Value* operand, Value* pointer, OffsetType, HeapRange = HeapRange::top(), HeapRange fenceRange = HeapRange::top());
    AtomicValue(AtomicValueCAS, Kind, Origin, Width, Value* expectedValue, Value* newValue, Value* pointer, OffsetType, HeapRange = HeapRange::top(), HeapRange fenceRange = HeapRange::top());

    Width m_width;
};

} } // namespace JSC::B3

#endif // ENABLE(B3_JIT)
