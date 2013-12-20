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

#ifndef FunctionCall_h
#define FunctionCall_h

#if ENABLE(CSS_SELECTOR_JIT)

#include "RegisterAllocator.h"
#include "StackAllocator.h"
#include <JavaScriptCore/GPRInfo.h>
#include <JavaScriptCore/MacroAssembler.h>

namespace WebCore {

class FunctionCall {
public:
    FunctionCall(JSC::MacroAssembler& assembler, const RegisterAllocator& registerAllocator, StackAllocator& stackAllocator, Vector<std::pair<JSC::MacroAssembler::Call, JSC::FunctionPtr>>& callRegistry)
        : m_assembler(assembler)
        , m_registerAllocator(registerAllocator)
        , m_stackAllocator(stackAllocator)
        , m_callRegistry(callRegistry)
        , m_firstArgument(0)
    {
    }

    void setFunctionAddress(JSC::FunctionPtr functionAddress)
    {
        m_functionAddress = functionAddress;
    }

    void setFirstArgument(const JSC::MacroAssembler::RegisterID& registerID)
    {
        m_firstArgument = &registerID;
    }

    void call()
    {
        prepareAndCall();
        cleanupPostCall();
    }

    JSC::MacroAssembler::Jump callAndBranchOnCondition(JSC::MacroAssembler::ResultCondition condition)
    {
        prepareAndCall();
        m_assembler.test32(JSC::GPRInfo::returnValueGPR, JSC::MacroAssembler::TrustedImm32(0xff));
        cleanupPostCall();
        return m_assembler.branch(condition);
    }

private:
    void prepareAndCall()
    {
        ASSERT(m_functionAddress.executableAddress());

        saveAllocatedRegisters();
        m_stackAllocator.alignStackPreFunctionCall();

        if (m_firstArgument && *m_firstArgument != JSC::GPRInfo::argumentGPR0)
            m_assembler.move(*m_firstArgument, JSC::GPRInfo::argumentGPR0);

        JSC::MacroAssembler::Call call = m_assembler.call();
        m_callRegistry.append(std::make_pair(call, m_functionAddress));
    }

    void cleanupPostCall()
    {
        m_stackAllocator.unalignStackPostFunctionCall();
        restoreAllocatedRegisters();
    }

    void saveAllocatedRegisters()
    {
        ASSERT(m_savedRegisterStackReferences.isEmpty());

        unsigned allocatedRegistersCount = m_registerAllocator.allocatedRegisters().size();
        m_savedRegisterStackReferences.reserveCapacity(allocatedRegistersCount);

        for (unsigned i = 0; i < allocatedRegistersCount; ++i) {
            JSC::MacroAssembler::RegisterID registerID = m_registerAllocator.allocatedRegisters()[i];
            m_savedRegisterStackReferences.append(m_stackAllocator.push(registerID));
        }
    }

    void restoreAllocatedRegisters()
    {
        for (unsigned i = m_registerAllocator.allocatedRegisters().size(); i > 0; --i)
            m_stackAllocator.pop(m_savedRegisterStackReferences[i - 1], m_registerAllocator.allocatedRegisters()[i - 1]);
        m_savedRegisterStackReferences.clear();
    }

    JSC::MacroAssembler& m_assembler;
    const RegisterAllocator& m_registerAllocator;
    StackAllocator& m_stackAllocator;
    Vector<std::pair<JSC::MacroAssembler::Call, JSC::FunctionPtr>>& m_callRegistry;

    Vector<StackAllocator::StackReference> m_savedRegisterStackReferences;

    JSC::FunctionPtr m_functionAddress;
    const JSC::MacroAssembler::RegisterID* m_firstArgument;
};

} // namespace WebCore

#endif // ENABLE(CSS_SELECTOR_JIT)

#endif // FunctionCall_h
