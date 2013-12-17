/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef RegisterAllocator_h
#define RegisterAllocator_h

#if ENABLE(CSS_SELECTOR_JIT)

#include <JavaScriptCore/MacroAssembler.h>
#include <wtf/Deque.h>
#include <wtf/Vector.h>

namespace WebCore {

#if CPU(X86_64)
static const JSC::MacroAssembler::RegisterID callerSavedRegisters[] = {
    JSC::X86Registers::eax,
    JSC::X86Registers::ecx,
    JSC::X86Registers::esi,
    JSC::X86Registers::edi,
    JSC::X86Registers::r8,
    JSC::X86Registers::r9,
    JSC::X86Registers::r10,
    JSC::X86Registers::r11
};
#else
#error RegisterAllocator has no defined registers for the architecture.
#endif

class RegisterAllocator {
public:
    RegisterAllocator();

    JSC::MacroAssembler::RegisterID allocateRegister()
    {
        RELEASE_ASSERT(!m_registers.isEmpty());

        JSC::MacroAssembler::RegisterID registerID = m_registers.takeFirst();
        ASSERT(!m_allocatedRegisters.contains(registerID));
        m_allocatedRegisters.append(registerID);
        return registerID;
    }

    void allocateRegister(JSC::MacroAssembler::RegisterID registerID)
    {
        ASSERT(!m_allocatedRegisters.contains(registerID));
        for (auto it = m_registers.begin(), end = m_registers.end(); it != end; ++it) {
            if (*it == registerID) {
                m_registers.remove(it);
                break;
            }
        }
        m_allocatedRegisters.append(registerID);
    }

    void deallocateRegister(JSC::MacroAssembler::RegisterID registerID)
    {
        ASSERT(m_allocatedRegisters.contains(registerID));
        // Most allocation/deallocation happen in stack-like order. In the common case, this
        // just removes the last item.
        m_allocatedRegisters.remove(m_allocatedRegisters.reverseFind(registerID));
        m_registers.append(registerID);
    }

    const Vector<JSC::MacroAssembler::RegisterID, 8>& allocatedRegisters() const { return m_allocatedRegisters; }

private:
    Deque<JSC::MacroAssembler::RegisterID, WTF_ARRAY_LENGTH(callerSavedRegisters)> m_registers;
    Vector<JSC::MacroAssembler::RegisterID, WTF_ARRAY_LENGTH(callerSavedRegisters)> m_allocatedRegisters;
};

class LocalRegister {
public:
    explicit LocalRegister(RegisterAllocator& allocator)
        : m_allocator(allocator)
        , m_register(allocator.allocateRegister())
    {
    }

    ~LocalRegister()
    {
        m_allocator.deallocateRegister(m_register);
    }

    operator JSC::MacroAssembler::RegisterID() const
    {
        return m_register;
    }

private:
    RegisterAllocator& m_allocator;
    JSC::MacroAssembler::RegisterID m_register;
};

inline RegisterAllocator::RegisterAllocator()
{
    for (unsigned i = 0; i < WTF_ARRAY_LENGTH(callerSavedRegisters); ++i)
        m_registers.append(callerSavedRegisters[i]);
}

} // namespace WebCore

#endif // ENABLE(CSS_SELECTOR_JIT)

#endif // RegisterAllocator_h

