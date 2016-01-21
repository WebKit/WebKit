/*
 * Copyright (C) 2015-2016 Apple Inc. All rights reserved.
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

#ifndef B3StackSlotValue_h
#define B3StackSlotValue_h

#if ENABLE(B3_JIT)

#include "B3StackSlotKind.h"
#include "B3Value.h"

namespace JSC { namespace B3 {

namespace Air {
class StackSlot;
} // namespace Air

class JS_EXPORT_PRIVATE StackSlotValue : public Value {
public:
    static bool accepts(Opcode opcode) { return opcode == StackSlot; }

    ~StackSlotValue();

    unsigned byteSize() const { return m_byteSize; }
    StackSlotKind kind() const { return m_kind; }

    // This gets assigned at the end of compilation. But, you can totally pin stack slots. Use the
    // set method to do that.
    intptr_t offsetFromFP() const { return m_offsetFromFP; }

    // Note that this is meaningless unless the stack slot is Locked.
    void setOffsetFromFP(intptr_t value)
    {
        m_offsetFromFP = value;
    }

protected:
    void dumpMeta(CommaPrinter&, PrintStream&) const override;

    Value* cloneImpl() const override;

private:
    friend class Air::StackSlot;
    friend class Procedure;

    StackSlotValue(unsigned index, Origin origin, unsigned byteSize, StackSlotKind kind)
        : Value(index, CheckedOpcode, StackSlot, pointerType(), origin)
        , m_byteSize(byteSize)
        , m_kind(kind)
        , m_offsetFromFP(0)
    {
    }

    unsigned m_byteSize;
    StackSlotKind m_kind;
    intptr_t m_offsetFromFP;
};

} } // namespace JSC::B3

#endif // ENABLE(B3_JIT)

#endif // B3StackSlotValue_h

