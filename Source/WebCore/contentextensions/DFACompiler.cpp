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

#include "config.h"
#include "DFACompiler.h"

#if ENABLE(CONTENT_EXTENSIONS)

#include "DFA.h"
#include "DFANode.h"
#include "RegisterAllocator.h"
#include "StackAllocator.h"
#include <JavaScriptCore/GPRInfo.h>
#include <JavaScriptCore/LinkBuffer.h>
#include <JavaScriptCore/MacroAssembler.h>
#include <JavaScriptCore/VM.h>

namespace WebCore {

namespace ContentExtensions {

typedef JSC::MacroAssembler Assembler;

class DFACodeGenerator {
public:
    explicit DFACodeGenerator(const DFA&);
    JSC::MacroAssemblerCodeRef compile(JSC::VM&);

private:
    void lowerStateMachine();
    void lowerNode(Assembler::JumpList& failureCases, const DFANode&);

    void tryLoadingNextCharacter(Assembler::JumpList& failureCases, LocalRegister& output);
    void callAddActionFunction(unsigned argument);

    const DFA& m_dfa;

    Assembler m_assembler;
    RegisterAllocator m_registerAllocator;
    StackAllocator m_stackAllocator;

    Assembler::RegisterID m_urlBufferOffsetRegister = InvalidGPRReg;
    Vector<Assembler::Label> m_stateEntryLabels;
    Vector<Assembler::JumpList> m_jumpsPerState;
};

JSC::MacroAssemblerCodeRef compileDFA(const DFA& dfa, JSC::VM& vm)
{
    DFACodeGenerator codeGenerator(dfa);
    return codeGenerator.compile(vm);
}

DFACodeGenerator::DFACodeGenerator(const DFA& dfa)
    : m_dfa(dfa)
    , m_stackAllocator(m_assembler)
{
}

JSC::MacroAssemblerCodeRef DFACodeGenerator::compile(JSC::VM& vm)
{
    m_registerAllocator.reserveCallerSavedRegisters(WTF_ARRAY_LENGTH(callerSavedRegisters));

    m_registerAllocator.allocateRegister(JSC::GPRInfo::argumentGPR0);
    m_registerAllocator.allocateRegister(JSC::GPRInfo::argumentGPR1);
    m_registerAllocator.allocateRegister(JSC::GPRInfo::argumentGPR2);

    m_urlBufferOffsetRegister = m_registerAllocator.allocateRegister();
    m_assembler.move(Assembler::TrustedImm32(0), m_urlBufferOffsetRegister);

    lowerStateMachine();

    m_registerAllocator.deallocateRegister(m_urlBufferOffsetRegister);
    m_urlBufferOffsetRegister = InvalidGPRReg;

    m_assembler.ret();

    m_registerAllocator.deallocateRegister(JSC::GPRInfo::argumentGPR0);
    m_registerAllocator.deallocateRegister(JSC::GPRInfo::argumentGPR1);
    m_registerAllocator.deallocateRegister(JSC::GPRInfo::argumentGPR2);

    JSC::LinkBuffer linkBuffer(vm, m_assembler, GLOBAL_THUNK_ID);
#if CONTENT_EXTENSIONS_STATE_MACHINE_DEBUGGING
    return linkBuffer.finalizeCodeWithDisassembly("Compiled Extension");
#else
    return FINALIZE_CODE(linkBuffer, ("Compiled Extension"));
#endif
}

void DFACodeGenerator::lowerStateMachine()
{
    Assembler::JumpList failureCases;

    unsigned dfaSize = m_dfa.size();

    m_stateEntryLabels.resize(dfaSize);
    m_jumpsPerState.resize(dfaSize);

    unsigned root = m_dfa.root();
    m_stateEntryLabels[root] = m_assembler.label();
    lowerNode(failureCases, m_dfa.nodeAt(root));

    for (unsigned i = 0; i < dfaSize; ++i) {
        if (i == root)
            continue;
        m_stateEntryLabels[i] = m_assembler.label();
        lowerNode(failureCases, m_dfa.nodeAt(i));
    }

    for (unsigned i = 0; i < dfaSize; ++i)
        m_jumpsPerState[i].linkTo(m_stateEntryLabels[i], &m_assembler);

    failureCases.link(&m_assembler);
}

void DFACodeGenerator::lowerNode(Assembler::JumpList& failureCases, const DFANode& dfaNode)
{
    for (uint64_t actionId : dfaNode.actions)
        callAddActionFunction(static_cast<unsigned>(actionId));

    LocalRegister characterRegister(m_registerAllocator);
    tryLoadingNextCharacter(failureCases, characterRegister);

    for (const auto& transition : dfaNode.transitions) {
        Assembler::Jump jumpOnCharacter = m_assembler.branch32(Assembler::Equal, characterRegister, Assembler::TrustedImm32(transition.key));
        m_jumpsPerState[transition.value].append(jumpOnCharacter);
    }

    if (dfaNode.hasFallbackTransition)
        m_jumpsPerState[dfaNode.fallbackTransition].append(m_assembler.jump());
    else
        failureCases.append(m_assembler.jump());
}

void DFACodeGenerator::tryLoadingNextCharacter(Assembler::JumpList& failureCases, LocalRegister& character)
{
    failureCases.append(m_assembler.branch32(Assembler::Equal, Assembler::Address(JSC::GPRInfo::argumentGPR0, OBJECT_OFFSETOF(PotentialPageLoadDescriptor, urlBufferSize)), m_urlBufferOffsetRegister));

    LocalRegister bufferPointer(m_registerAllocator);
    m_assembler.loadPtr(Assembler::Address(JSC::GPRInfo::argumentGPR0, OBJECT_OFFSETOF(PotentialPageLoadDescriptor, urlBuffer)), bufferPointer);
    m_assembler.load8(Assembler::BaseIndex(bufferPointer, m_urlBufferOffsetRegister, Assembler::TimesOne), character);

    m_assembler.add32(Assembler::TrustedImm32(1), m_urlBufferOffsetRegister);
}

void DFACodeGenerator::callAddActionFunction(unsigned argument)
{
    const RegisterVector& allocatedRegisters = m_registerAllocator.allocatedRegisters();
    StackAllocator::StackReferenceVector savedRegisterStackReferences = m_stackAllocator.push(allocatedRegisters);

    m_stackAllocator.alignStackPreFunctionCall();

    {
        LocalRegister functionAddress(m_registerAllocator);
        m_assembler.move(JSC::GPRInfo::argumentGPR1, functionAddress);
        m_assembler.move(JSC::GPRInfo::argumentGPR2, JSC::GPRInfo::argumentGPR0);
        m_assembler.move(Assembler::TrustedImm32(argument), JSC::GPRInfo::argumentGPR1);

        m_assembler.call(functionAddress);
    }

    m_stackAllocator.unalignStackPostFunctionCall();

    m_stackAllocator.pop(savedRegisterStackReferences, allocatedRegisters);
}

} // namespace ContentExtensions

} // namespace WebCore

#endif // ENABLE(CONTENT_EXTENSIONS)
