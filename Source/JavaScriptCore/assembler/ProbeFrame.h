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

#if ENABLE(MASM_PROBE)

#include "CallFrame.h"
#include "ProbeStack.h"

namespace JSC {
namespace Probe {

class Frame {
public:
    Frame(void* frameBase, Stack& stack)
        : m_frameBase { reinterpret_cast<uint8_t*>(frameBase) }
        , m_stack { stack }
    { }

    template<typename T = JSValue>
    T argument(int argument)
    {
        return get<T>(CallFrame::argumentOffset(argument) * sizeof(Register));
    }
    template<typename T = JSValue>
    T operand(VirtualRegister operand)
    {
        return get<T>(operand.offset() * sizeof(Register));
    }
    template<typename T = JSValue>
    T operand(VirtualRegister operand, ptrdiff_t offset)
    {
        return get<T>(operand.offset() * sizeof(Register) + offset);
    }

    template<typename T>
    void setArgument(int argument, T value)
    {
        return set<T>(CallFrame::argumentOffset(argument) * sizeof(Register), value);
    }
    template<typename T>
    void setOperand(VirtualRegister operand, T value)
    {
        set<T>(operand.offset() * sizeof(Register), value);
    }
    template<typename T>
    void setOperand(VirtualRegister operand, ptrdiff_t offset, T value)
    {
        set<T>(operand.offset() * sizeof(Register) + offset, value);
    }

    template<typename T = JSValue>
    T get(ptrdiff_t offset)
    {
        return m_stack.get<T>(m_frameBase + offset);
    }
    template<typename T>
    void set(ptrdiff_t offset, T value)
    {
        m_stack.set<T>(m_frameBase + offset, value);
    }

private:
    uint8_t* m_frameBase;
    Stack& m_stack;
};

} // namespace Probe
} // namespace JSC

#endif // ENABLE(MASM_PROBE)
