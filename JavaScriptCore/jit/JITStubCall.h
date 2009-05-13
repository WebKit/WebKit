/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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

#ifndef JITStubCall_h
#define JITStubCall_h

#include <wtf/Platform.h>

#if ENABLE(JIT)

namespace JSC {

    class JITStubCall {
    public:
        JITStubCall(JIT* jit, JSObject* (JIT_STUB *stub)(STUB_ARGS_DECLARATION))
            : m_jit(jit)
            , m_stub(reinterpret_cast<void*>(stub))
            , m_returnType(Value)
            , m_argumentIndex(1) // Index 0 is reserved for restoreArgumentReference();
        {
        }

        JITStubCall(JIT* jit, JSPropertyNameIterator* (JIT_STUB *stub)(STUB_ARGS_DECLARATION))
            : m_jit(jit)
            , m_stub(reinterpret_cast<void*>(stub))
            , m_returnType(Value)
            , m_argumentIndex(1) // Index 0 is reserved for restoreArgumentReference();
        {
        }

        JITStubCall(JIT* jit, void* (JIT_STUB *stub)(STUB_ARGS_DECLARATION))
            : m_jit(jit)
            , m_stub(reinterpret_cast<void*>(stub))
            , m_returnType(Value)
            , m_argumentIndex(1) // Index 0 is reserved for restoreArgumentReference();
        {
        }

        JITStubCall(JIT* jit, int (JIT_STUB *stub)(STUB_ARGS_DECLARATION))
            : m_jit(jit)
            , m_stub(reinterpret_cast<void*>(stub))
            , m_returnType(Value)
            , m_argumentIndex(1) // Index 0 is reserved for restoreArgumentReference();
        {
        }

        JITStubCall(JIT* jit, void (JIT_STUB *stub)(STUB_ARGS_DECLARATION))
            : m_jit(jit)
            , m_stub(reinterpret_cast<void*>(stub))
            , m_returnType(Void)
            , m_argumentIndex(1) // Index 0 is reserved for restoreArgumentReference();
        {
        }

        // Arguments are added first to last.

        void addArgument(JIT::Imm32 argument)
        {
            m_jit->poke(argument, m_argumentIndex);
            ++m_argumentIndex;
        }

        void addArgument(JIT::ImmPtr argument)
        {
            m_jit->poke(argument, m_argumentIndex);
            ++m_argumentIndex;
        }

        void addArgument(JIT::RegisterID argument)
        {
            m_jit->poke(argument, m_argumentIndex);
            ++m_argumentIndex;
        }

        void addArgument(unsigned src, JIT::RegisterID scratchRegister) // src is a virtual register.
        {
            if (m_jit->m_codeBlock->isConstantRegisterIndex(src))
                addArgument(JIT::ImmPtr(JSValue::encode(m_jit->m_codeBlock->getConstant(src))));
            else {
                m_jit->loadPtr(JIT::Address(JIT::callFrameRegister, src * sizeof(Register)), scratchRegister);
                addArgument(scratchRegister);
            }
            m_jit->killLastResultRegister();
        }

        JIT::Call call()
        {
            ASSERT(m_jit->m_bytecodeIndex != (unsigned)-1); // This method should only be called during hot/cold path generation, so that m_bytecodeIndex is set.

#if ENABLE(OPCODE_SAMPLING)
            m_jit->sampleInstruction(m_jit->m_codeBlock->instructions().begin() + m_jit->m_bytecodeIndex, true);
#endif

            m_jit->restoreArgumentReference();
            JIT::Call call = m_jit->call();
            m_jit->m_calls.append(CallRecord(call, m_jit->m_bytecodeIndex, m_stub));

#if ENABLE(OPCODE_SAMPLING)
            m_jit->sampleInstruction(m_jit->m_codeBlock->instructions().begin() + m_jit->m_bytecodeIndex, false);
#endif

            m_jit->killLastResultRegister();
            return call;
        }

        JIT::Call call(unsigned dst) // dst is a virtual register.
        {
            ASSERT(m_returnType == Value);
            JIT::Call call = this->call();
            m_jit->emitPutVirtualRegister(dst);
            return call;
        }

        JIT::Call call(JIT::RegisterID dst)
        {
            ASSERT(m_returnType == Value);
            JIT::Call call = this->call();
            if (dst != JIT::returnValueRegister)
                m_jit->move(JIT::returnValueRegister, dst);
            return call;
        }

    private:
        JIT* m_jit;
        void* m_stub;
        enum { Value, Void } m_returnType;
        size_t m_argumentIndex;
    };

    class CallEvalJITStub : public JITStubCall {
    public:
        CallEvalJITStub(JIT* jit, Instruction* instruction)
            : JITStubCall(jit, JITStubs::cti_op_call_eval)
        {
            int callee = instruction[2].u.operand;
            int argCount = instruction[3].u.operand;
            int registerOffset = instruction[4].u.operand;

            addArgument(callee, JIT::regT2);
            addArgument(JIT::Imm32(registerOffset));
            addArgument(JIT::Imm32(argCount));
        }
    };
}

#endif // ENABLE(JIT)

#endif // JITStubCall_h
