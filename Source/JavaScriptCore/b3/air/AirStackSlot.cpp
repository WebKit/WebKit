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

#include "config.h"
#include "AirStackSlot.h"

#if ENABLE(B3_JIT)

#include "B3StackSlotValue.h"

namespace JSC { namespace B3 { namespace Air {

void StackSlot::setOffsetFromFP(intptr_t value)
{
    m_offsetFromFP = value;
    if (m_value)
        m_value->m_offsetFromFP = value;
}

void StackSlot::dump(PrintStream& out) const
{
    out.print("stack", m_index);
}

void StackSlot::deepDump(PrintStream& out) const
{
    out.print("byteSize = ", m_byteSize, ", offsetFromFP = ", m_offsetFromFP, ", kind = ", m_kind);
    if (m_value)
        out.print(", value = ", *m_value);
}

StackSlot::StackSlot(unsigned byteSize, unsigned index, StackSlotKind kind, StackSlotValue* value)
    : m_byteSize(byteSize)
    , m_index(index)
    , m_offsetFromFP(value ? value->offsetFromFP() : 0)
    , m_kind(kind)
    , m_value(value)
{
    ASSERT(byteSize);
}

} } } // namespace JSC::B3::Air

#endif // ENABLE(B3_JIT)
