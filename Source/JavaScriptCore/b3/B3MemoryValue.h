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

#ifndef B3MemoryValue_h
#define B3MemoryValue_h

#if ENABLE(B3_JIT)

#include "B3HeapRange.h"
#include "B3Value.h"

namespace JSC { namespace B3 {

class JS_EXPORT_PRIVATE MemoryValue : public Value {
public:
    static bool accepts(Opcode opcode)
    {
        switch (opcode) {
        case Load8Z:
        case Load8S:
        case Load16Z:
        case Load16S:
        case Load:
        case Store8:
        case Store16:
        case Store:
            return true;
        default:
            return false;
        }
    }

    ~MemoryValue();

    int32_t offset() const { return m_offset; }
    void setOffset(int32_t offset) { m_offset = offset; }

    const HeapRange& range() const { return m_range; }
    void setRange(const HeapRange& range) { m_range = range; }

    size_t accessByteSize() const;

protected:
    void dumpMeta(CommaPrinter& comma, PrintStream&) const override;

private:
    friend class Procedure;

    // Use this form for Load (but not Load8Z, Load8S, or any of the Loads that have a suffix that
    // describes the returned type).
    MemoryValue(
        unsigned index, Opcode opcode, Type type, Origin origin, Value* pointer,
        int32_t offset = 0)
        : Value(index, CheckedOpcode, opcode, type, origin, pointer)
        , m_offset(offset)
        , m_range(HeapRange::top())
    {
        if (!ASSERT_DISABLED) {
            switch (opcode) {
            case Load:
                break;
            case Load8Z:
            case Load8S:
            case Load16Z:
            case Load16S:
                ASSERT(type == Int32);
                break;
            case Store8:
            case Store16:
            case Store:
                ASSERT(type == Void);
                break;
            default:
                ASSERT_NOT_REACHED();
            }
        }
    }

    // Use this form for loads where the return type is implied.
    MemoryValue(unsigned index, Opcode opcode, Origin origin, Value* pointer, int32_t offset = 0)
        : MemoryValue(index, opcode, Int32, origin, pointer, offset)
    {
    }

    // Use this form for stores.
    MemoryValue(
        unsigned index, Opcode opcode, Origin origin, Value* value, Value* pointer,
        int32_t offset = 0)
        : Value(index, CheckedOpcode, opcode, Void, origin, value, pointer)
        , m_offset(offset)
        , m_range(HeapRange::top())
    {
        if (!ASSERT_DISABLED) {
            switch (opcode) {
            case Store8:
            case Store16:
            case Store:
                break;
            default:
                ASSERT_NOT_REACHED();
                break;
            }
        }
    }

    int32_t m_offset { 0 };
    HeapRange m_range;
};

} } // namespace JSC::B3

#endif // ENABLE(B3_JIT)

#endif // B3MemoryValue_h

