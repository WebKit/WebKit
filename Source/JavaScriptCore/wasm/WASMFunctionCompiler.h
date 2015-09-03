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

#ifndef WASMFunctionCompiler_h
#define WASMFunctionCompiler_h

#if ENABLE(WEBASSEMBLY)

#include "CCallHelpers.h"
#include "JITOperations.h"
#include "LinkBuffer.h"
#include "MaxFrameExtentForSlowPathCall.h"

namespace JSC {

class WASMFunctionCompiler : private CCallHelpers {
public:
    typedef int Expression;
    typedef int Statement;

    union StackSlot {
        int32_t intValue;
        float floatValue;
        double doubleValue;
    };

    WASMFunctionCompiler(VM& vm, CodeBlock* codeBlock, unsigned stackHeight)
        : CCallHelpers(&vm, codeBlock)
        , m_stackHeight(stackHeight)
    {
    }

    void startFunction(const Vector<WASMType>& arguments, uint32_t numberOfI32LocalVariables, uint32_t numberOfF32LocalVariables, uint32_t numberOfF64LocalVariables)
    {
        emitFunctionPrologue();
        emitPutImmediateToCallFrameHeader(m_codeBlock, JSStack::CodeBlock);

        m_beginLabel = label();

        addPtr(TrustedImm32(-WTF::roundUpToMultipleOf(stackAlignmentRegisters(), m_stackHeight) * sizeof(StackSlot)), GPRInfo::callFrameRegister, GPRInfo::regT1);
        m_stackOverflow = branchPtr(Above, AbsoluteAddress(m_vm->addressOfStackLimit()), GPRInfo::regT1);

        move(GPRInfo::regT1, stackPointerRegister);
        checkStackPointerAlignment();

        m_numberOfLocals = arguments.size() + numberOfI32LocalVariables + numberOfF32LocalVariables + numberOfF64LocalVariables;

        unsigned localIndex = 0;
        for (size_t i = 0; i < arguments.size(); ++i) {
            Address address(GPRInfo::callFrameRegister, CallFrame::argumentOffset(i) * sizeof(Register));
            switch (arguments[i]) {
            case WASMType::I32:
                load32(address, GPRInfo::regT0);
                store32(GPRInfo::regT0, localAddress(localIndex++));
                break;
            case WASMType::F32:
                load64(address, GPRInfo::regT0);
                unboxDoubleWithoutAssertions(GPRInfo::regT0, FPRInfo::fpRegT0);
                convertDoubleToFloat(FPRInfo::fpRegT0, FPRInfo::fpRegT0);
                storeDouble(FPRInfo::fpRegT0, localAddress(localIndex++));
                break;
            case WASMType::F64:
                load64(address, GPRInfo::regT0);
                unboxDoubleWithoutAssertions(GPRInfo::regT0, FPRInfo::fpRegT0);
                storeDouble(FPRInfo::fpRegT0, localAddress(localIndex++));
                break;
            default:
                ASSERT_NOT_REACHED();
            }
        }
        for (uint32_t i = 0; i < numberOfI32LocalVariables; ++i)
            store32(TrustedImm32(0), localAddress(localIndex++));
        for (uint32_t i = 0; i < numberOfF32LocalVariables; ++i)
            store32(TrustedImm32(0), localAddress(localIndex++));
        for (uint32_t i = 0; i < numberOfF64LocalVariables; ++i)
            store64(TrustedImm64(0), localAddress(localIndex++));

        m_codeBlock->setNumParameters(1 + arguments.size());
    }

    void endFunction()
    {
        // FIXME: Remove these if the last statement is a return statement.
        move(TrustedImm64(JSValue::encode(jsUndefined())), GPRInfo::returnValueGPR);
        emitFunctionEpilogue();
        ret();

        m_stackOverflow.link(this);
        if (maxFrameExtentForSlowPathCall)
            addPtr(TrustedImm32(-maxFrameExtentForSlowPathCall), stackPointerRegister);
        throwStackOverflowError();

        // FIXME: Implement arity check.
        Label arityCheck = label();
        emitFunctionPrologue();
        emitPutImmediateToCallFrameHeader(m_codeBlock, JSStack::CodeBlock);
        jump(m_beginLabel);

        LinkBuffer patchBuffer(*m_vm, *this, m_codeBlock, JITCompilationMustSucceed);

        for (auto iterator : m_calls)
            patchBuffer.link(iterator.first, FunctionPtr(iterator.second));

        MacroAssemblerCodePtr withArityCheck = patchBuffer.locationOf(arityCheck);
        CodeRef result = FINALIZE_CODE(patchBuffer, ("Baseline JIT code for WebAssembly"));
        m_codeBlock->setJITCode(adoptRef(new DirectJITCode(result, withArityCheck, JITCode::BaselineJIT)));
        m_codeBlock->capabilityLevel();
    }

private:
    Address localAddress(unsigned localIndex) const
    {
        ASSERT(localIndex < m_numberOfLocals);
        return Address(GPRInfo::callFrameRegister, -(localIndex + 1) * sizeof(StackSlot));
    }

    void throwStackOverflowError()
    {
        setupArgumentsWithExecState(TrustedImmPtr(m_codeBlock));

        m_calls.append(std::make_pair(call(), FunctionPtr(operationThrowStackOverflowError).value()));

        // lookupExceptionHandlerFromCallerFrame is passed two arguments, the VM and the exec (the CallFrame*).
        move(TrustedImmPtr(m_vm), GPRInfo::argumentGPR0);
        move(GPRInfo::callFrameRegister, GPRInfo::argumentGPR1);
#if CPU(X86)
        // FIXME: should use the call abstraction, but this is currently in the SpeculativeJIT layer!
        poke(GPRInfo::argumentGPR0);
        poke(GPRInfo::argumentGPR1, 1);
#endif
        m_calls.append(std::make_pair(call(), FunctionPtr(lookupExceptionHandlerFromCallerFrame).value()));
        jumpToExceptionHandler();
    }

    unsigned m_stackHeight;
    unsigned m_numberOfLocals;

    Label m_beginLabel;
    Jump m_stackOverflow;

    Vector<std::pair<Call, void*>> m_calls;
};

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)

#endif // WASMFunctionCompiler_h
