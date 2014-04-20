/*
 * Copyright (C) 2013, 2014 Apple Inc. All rights reserved.
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
#include <StackAllocator.h>
#include <wtf/HashSet.h>
#include <wtf/Vector.h>

namespace WebCore {

#if CPU(ARM64)
static const JSC::MacroAssembler::RegisterID callerSavedRegisters[] = {
    JSC::ARM64Registers::x0,
    JSC::ARM64Registers::x1,
    JSC::ARM64Registers::x2,
    JSC::ARM64Registers::x3,
    JSC::ARM64Registers::x4,
    JSC::ARM64Registers::x5,
    JSC::ARM64Registers::x6,
    JSC::ARM64Registers::x7,
    JSC::ARM64Registers::x8,
    JSC::ARM64Registers::x9,
    JSC::ARM64Registers::x10,
    JSC::ARM64Registers::x11,
    JSC::ARM64Registers::x12,
    JSC::ARM64Registers::x13,
    JSC::ARM64Registers::x14,
    JSC::ARM64Registers::x15,
};
static const JSC::MacroAssembler::RegisterID calleeSavedRegisters[] = {
    JSC::ARM64Registers::x19
};
#elif CPU(X86_64)
static const JSC::MacroAssembler::RegisterID callerSavedRegisters[] = {
    JSC::X86Registers::eax,
    JSC::X86Registers::ecx,
    JSC::X86Registers::edx,
    JSC::X86Registers::esi,
    JSC::X86Registers::edi,
    JSC::X86Registers::r8,
    JSC::X86Registers::r9,
    JSC::X86Registers::r10,
    JSC::X86Registers::r11
};
static const JSC::MacroAssembler::RegisterID calleeSavedRegisters[] = {
    JSC::X86Registers::r12,
    JSC::X86Registers::r13,
    JSC::X86Registers::r14,
    JSC::X86Registers::r15
};
#else
#error RegisterAllocator has no defined registers for the architecture.
#endif
static const unsigned registerCount = WTF_ARRAY_LENGTH(callerSavedRegisters) + WTF_ARRAY_LENGTH(calleeSavedRegisters);

class RegisterAllocator {
public:
    RegisterAllocator();
    ~RegisterAllocator();

    unsigned availableRegisterCount() const { return m_registers.size(); }

    JSC::MacroAssembler::RegisterID allocateRegister()
    {
        auto first = m_registers.begin();
        JSC::MacroAssembler::RegisterID registerID = static_cast<JSC::MacroAssembler::RegisterID>(*first);
        RELEASE_ASSERT(m_registers.remove(first));
        ASSERT(!m_allocatedRegisters.contains(registerID));
        m_allocatedRegisters.append(registerID);
        return registerID;
    }

    void allocateRegister(JSC::MacroAssembler::RegisterID registerID)
    {
        RELEASE_ASSERT(m_registers.remove(registerID));
        ASSERT(!m_allocatedRegisters.contains(registerID));
        m_allocatedRegisters.append(registerID);
    }

    void deallocateRegister(JSC::MacroAssembler::RegisterID registerID)
    {
        ASSERT(m_allocatedRegisters.contains(registerID));
        // Most allocation/deallocation happen in stack-like order. In the common case, this
        // just removes the last item.
        m_allocatedRegisters.remove(m_allocatedRegisters.reverseFind(registerID));
        RELEASE_ASSERT(m_registers.add(registerID).isNewEntry);
    }

    void reserveCalleeSavedRegisters(StackAllocator& stack, unsigned count)
    {
        unsigned finalSize = m_calleeSavedRegisters.size() + count;
        RELEASE_ASSERT(finalSize <= WTF_ARRAY_LENGTH(calleeSavedRegisters));
        for (unsigned i = m_calleeSavedRegisters.size(); i < finalSize; ++i) {
            JSC::MacroAssembler::RegisterID registerId = calleeSavedRegisters[i];
            m_calleeSavedRegisters.append(stack.push(registerId));
            m_registers.add(registerId);
        }
    }

    void restoreCalleeSavedRegisters(StackAllocator& stack)
    {
        for (unsigned i = m_calleeSavedRegisters.size(); i > 0 ; --i) {
            JSC::MacroAssembler::RegisterID registerId = calleeSavedRegisters[i - 1];
            stack.pop(m_calleeSavedRegisters[i - 1], registerId);
            RELEASE_ASSERT(m_registers.remove(registerId));
        }
        m_calleeSavedRegisters.clear();
    }

    const Vector<JSC::MacroAssembler::RegisterID, registerCount>& allocatedRegisters() const { return m_allocatedRegisters; }

    static bool isValidRegister(JSC::MacroAssembler::RegisterID registerID)
    {
#if CPU(ARM64)
        return registerID >= JSC::ARM64Registers::x0 && registerID <= JSC::ARM64Registers::x15;
#elif CPU(X86_64)
        return registerID >= JSC::X86Registers::eax && registerID <= JSC::X86Registers::r15;
#else
#error RegisterAllocator does not define the valid register range for the current architecture.
#endif
    }

private:
    HashSet<unsigned, DefaultHash<unsigned>::Hash, WTF::UnsignedWithZeroKeyHashTraits<unsigned>> m_registers;
    Vector<JSC::MacroAssembler::RegisterID, registerCount> m_allocatedRegisters;
    Vector<StackAllocator::StackReference, WTF_ARRAY_LENGTH(calleeSavedRegisters)> m_calleeSavedRegisters;
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
        m_registers.add(callerSavedRegisters[i]);
}

inline RegisterAllocator::~RegisterAllocator()
{
    RELEASE_ASSERT(m_calleeSavedRegisters.isEmpty());
}

} // namespace WebCore

#endif // ENABLE(CSS_SELECTOR_JIT)

#endif // RegisterAllocator_h

