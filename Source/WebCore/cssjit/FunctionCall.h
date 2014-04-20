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
    FunctionCall(JSC::MacroAssembler& assembler, RegisterAllocator& registerAllocator, StackAllocator& stackAllocator, Vector<std::pair<JSC::MacroAssembler::Call, JSC::FunctionPtr>>& callRegistry)
        : m_assembler(assembler)
        , m_registerAllocator(registerAllocator)
        , m_stackAllocator(stackAllocator)
        , m_callRegistry(callRegistry)
        , m_argumentCount(0)
        , m_firstArgument(InvalidGPRReg)
        , m_secondArgument(InvalidGPRReg)
    {
    }

    void setFunctionAddress(JSC::FunctionPtr functionAddress)
    {
        m_functionAddress = functionAddress;
    }

    void setOneArgument(const JSC::MacroAssembler::RegisterID& registerID)
    {
        m_argumentCount = 1;
        m_firstArgument = registerID;
    }

    void setTwoArguments(const JSC::MacroAssembler::RegisterID& firstRegisterID, const JSC::MacroAssembler::RegisterID& secondRegisterID)
    {
        m_argumentCount = 2;
        m_firstArgument = firstRegisterID;
        m_secondArgument = secondRegisterID;
    }

    void call()
    {
        prepareAndCall();
        cleanupPostCall();
    }

    JSC::MacroAssembler::Jump callAndBranchOnCondition(JSC::MacroAssembler::ResultCondition condition)
    {
        prepareAndCall();
        m_assembler.test32(condition, JSC::GPRInfo::returnValueGPR, JSC::MacroAssembler::TrustedImm32(0xff));
        cleanupPostCall();
        return m_assembler.branch(condition);
    }

private:
    void swapArguments()
    {
        JSC::MacroAssembler::RegisterID a = m_firstArgument;
        JSC::MacroAssembler::RegisterID b = m_secondArgument;
        // x86 can swap without a temporary register. On other architectures, we need allocate a temporary register to switch the values.
#if CPU(X86) || CPU(X86_64)
        m_assembler.swap(a, b);
#else
        if (m_registerAllocator.availableRegisterCount()) {
            // Usually we can just use a free register.
            LocalRegister tempValue(m_registerAllocator);
            m_assembler.move(a, tempValue);
            m_assembler.move(b, a);
            m_assembler.move(tempValue, b);
        } else {
            // If there is no free register, everything should be on the stack at this point. We can take
            // the first of those saved registers and use it as a temporary.
            JSC::MacroAssembler::RegisterID pushedRegister;
            for (unsigned i = 0; i < m_registerAllocator.allocatedRegisters().size(); ++i) {
                pushedRegister = m_registerAllocator.allocatedRegisters()[i];
                if (pushedRegister != a && pushedRegister != b)
                    break;
            }
            ASSERT(pushedRegister != a && pushedRegister != b);
            m_assembler.move(a, pushedRegister);
            m_assembler.move(b, a);
            m_assembler.move(pushedRegister, b);
        }
#endif
    }

    void prepareAndCall()
    {
        ASSERT(m_functionAddress.executableAddress());
        ASSERT(!m_firstArgument || (m_firstArgument && !m_secondArgument) || (m_firstArgument && m_secondArgument));

        saveAllocatedRegisters();
        m_stackAllocator.alignStackPreFunctionCall();

        if (m_argumentCount == 2) {
            RELEASE_ASSERT(RegisterAllocator::isValidRegister(m_firstArgument));
            RELEASE_ASSERT(RegisterAllocator::isValidRegister(m_secondArgument));

            if (m_firstArgument != JSC::GPRInfo::argumentGPR0) {
                // If firstArgument is not in argumentGPR0, we need to handle potential conflicts:
                // -if secondArgument and firstArgument are in inversted registers, just swap the values.
                // -if secondArgument is in argumentGPR0 but firstArgument is not taking argumentGPR1, we can move in order secondArgument->argumentGPR1, firstArgument->argumentGPR0
                // -if secondArgument does not take argumentGPR0, firstArgument and secondArgument can be moved safely to destination.
                if (m_secondArgument == JSC::GPRInfo::argumentGPR0) {
                    if (m_firstArgument == JSC::GPRInfo::argumentGPR1)
                        swapArguments();
                    else {
                        m_assembler.move(JSC::GPRInfo::argumentGPR0, JSC::GPRInfo::argumentGPR1);
                        m_assembler.move(m_firstArgument, JSC::GPRInfo::argumentGPR0);
                    }
                } else {
                    m_assembler.move(m_firstArgument, JSC::GPRInfo::argumentGPR0);
                    if (m_secondArgument != JSC::GPRInfo::argumentGPR1)
                        m_assembler.move(m_secondArgument, JSC::GPRInfo::argumentGPR1);
                }
            } else {
                // We know firstArgument is already in place, we can safely move secondArgument.
                if (m_secondArgument != JSC::GPRInfo::argumentGPR1)
                    m_assembler.move(m_secondArgument, JSC::GPRInfo::argumentGPR1);
            }
        } else if (m_argumentCount == 1) {
            RELEASE_ASSERT(RegisterAllocator::isValidRegister(m_firstArgument));
            if (m_firstArgument != JSC::GPRInfo::argumentGPR0)
                m_assembler.move(m_firstArgument, JSC::GPRInfo::argumentGPR0);
        }

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
    RegisterAllocator& m_registerAllocator;
    StackAllocator& m_stackAllocator;
    Vector<std::pair<JSC::MacroAssembler::Call, JSC::FunctionPtr>>& m_callRegistry;

    Vector<StackAllocator::StackReference> m_savedRegisterStackReferences;

    JSC::FunctionPtr m_functionAddress;
    unsigned m_argumentCount;
    JSC::MacroAssembler::RegisterID m_firstArgument;
    JSC::MacroAssembler::RegisterID m_secondArgument;
};

} // namespace WebCore

#endif // ENABLE(CSS_SELECTOR_JIT)

#endif // FunctionCall_h
