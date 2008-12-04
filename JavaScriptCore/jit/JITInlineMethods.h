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

#ifndef JITInlineMethods_h
#define JITInlineMethods_h

#include <wtf/Platform.h>

#if ENABLE(JIT)

#define __ m_assembler. 

#if PLATFORM(WIN)
#undef FIELD_OFFSET // Fix conflict with winnt.h.
#endif

// FIELD_OFFSET: Like the C++ offsetof macro, but you can use it with classes.
// The magic number 0x4000 is insignificant. We use it to avoid using NULL, since
// NULL can cause compiler problems, especially in cases of multiple inheritance.
#define FIELD_OFFSET(class, field) (reinterpret_cast<ptrdiff_t>(&(reinterpret_cast<class*>(0x4000)->field)) - 0x4000)

namespace JSC {

typedef X86Assembler::JmpSrc JmpSrc;

static ALWAYS_INLINE uintptr_t asInteger(JSValue* value)
{
    return reinterpret_cast<uintptr_t>(value);
}

ALWAYS_INLINE void JIT::killLastResultRegister()
{
    m_lastResultBytecodeRegister = std::numeric_limits<int>::max();
}

// get arg puts an arg from the SF register array into a h/w register
ALWAYS_INLINE void JIT::emitGetVirtualRegister(int src, RegisterID dst, unsigned currentInstructionIndex)
{
    // TODO: we want to reuse values that are already in registers if we can - add a register allocator!
    if (m_codeBlock->isConstantRegisterIndex(src)) {
        JSValue* value = m_codeBlock->getConstant(src);
        __ movl_i32r(asInteger(value), dst);
        killLastResultRegister();
        return;
    }

    if (src == m_lastResultBytecodeRegister && m_codeBlock->isTemporaryRegisterIndex(src)) {
        bool atJumpTarget = false;
        while (m_jumpTargetsPosition < m_codeBlock->jumpTargets.size() && m_codeBlock->jumpTargets[m_jumpTargetsPosition] <= currentInstructionIndex) {
            if (m_codeBlock->jumpTargets[m_jumpTargetsPosition] == currentInstructionIndex)
                atJumpTarget = true;
            ++m_jumpTargetsPosition;
        }

        if (!atJumpTarget) {
            // The argument we want is already stored in eax
            if (dst != X86::eax)
                __ movl_rr(X86::eax, dst);
            killLastResultRegister();
            return;
        }
    }

    __ movl_mr(src * sizeof(Register), X86::edi, dst);
    killLastResultRegister();
}

ALWAYS_INLINE void JIT::emitGetVirtualRegisters(int src1, RegisterID dst1, int src2, RegisterID dst2, unsigned i)
{
    if (src2 == m_lastResultBytecodeRegister) {
        emitGetVirtualRegister(src2, dst2, i);
        emitGetVirtualRegister(src1, dst1, i);
    } else {
        emitGetVirtualRegister(src1, dst1, i);
        emitGetVirtualRegister(src2, dst2, i);
    }
}

// get arg puts an arg from the SF register array onto the stack, as an arg to a context threaded function.
ALWAYS_INLINE void JIT::emitPutCTIArgFromVirtualRegister(unsigned src, unsigned offset, RegisterID scratch)
{
    if (m_codeBlock->isConstantRegisterIndex(src)) {
        JSValue* value = m_codeBlock->getConstant(src);
        __ movl_i32m(asInteger(value), offset + sizeof(void*), X86::esp);
    } else {
        __ movl_mr(src * sizeof(Register), X86::edi, scratch);
        __ movl_rm(scratch, offset + sizeof(void*), X86::esp);
    }

    killLastResultRegister();
}

// puts an arg onto the stack, as an arg to a context threaded function.
ALWAYS_INLINE void JIT::emitPutCTIArg(RegisterID src, unsigned offset)
{
    __ movl_rm(src, offset + sizeof(void*), X86::esp);
}

ALWAYS_INLINE void JIT::emitGetCTIArg(unsigned offset, RegisterID dst)
{
    __ movl_mr(offset + sizeof(void*), X86::esp, dst);
}


ALWAYS_INLINE void JIT::emitPutCTIArgConstant(unsigned value, unsigned offset)
{
    __ movl_i32m(value, offset + sizeof(void*), X86::esp);
}

ALWAYS_INLINE JSValue* JIT::getConstantImmediateNumericArg(unsigned src)
{
    if (m_codeBlock->isConstantRegisterIndex(src)) {
        JSValue* value = m_codeBlock->getConstant(src);
        return JSImmediate::isNumber(value) ? value : noValue();
    }
    return noValue();
}

ALWAYS_INLINE void JIT::emitPutCTIParam(void* value, unsigned name)
{
    __ movl_i32m(reinterpret_cast<intptr_t>(value), name * sizeof(void*), X86::esp);
}

ALWAYS_INLINE void JIT::emitPutCTIParam(RegisterID from, unsigned name)
{
    __ movl_rm(from, name * sizeof(void*), X86::esp);
}

ALWAYS_INLINE void JIT::emitGetCTIParam(unsigned name, RegisterID to)
{
    __ movl_mr(name * sizeof(void*), X86::esp, to);
    killLastResultRegister();
}

ALWAYS_INLINE void JIT::emitPutToCallFrameHeader(RegisterID from, RegisterFile::CallFrameHeaderEntry entry)
{
    __ movl_rm(from, entry * sizeof(Register), X86::edi);
}

ALWAYS_INLINE void JIT::emitGetFromCallFrameHeader(RegisterFile::CallFrameHeaderEntry entry, RegisterID to)
{
    __ movl_mr(entry * sizeof(Register), X86::edi, to);
    killLastResultRegister();
}

ALWAYS_INLINE void JIT::emitPutVirtualRegister(unsigned dst, RegisterID from)
{
    __ movl_rm(from, dst * sizeof(Register), X86::edi);
    m_lastResultBytecodeRegister = (from == X86::eax) ? dst : std::numeric_limits<int>::max();
    // FIXME: #ifndef NDEBUG, Write the correct m_type to the register.
}

ALWAYS_INLINE void JIT::emitInitRegister(unsigned dst)
{
    __ movl_i32m(asInteger(jsUndefined()), dst * sizeof(Register), X86::edi);
    // FIXME: #ifndef NDEBUG, Write the correct m_type to the register.
}

ALWAYS_INLINE JmpSrc JIT::emitNakedCall(unsigned bytecodeIndex, X86::RegisterID r)
{
    JmpSrc call = __ call(r);
    m_calls.append(CallRecord(call, bytecodeIndex));

    return call;
}

ALWAYS_INLINE  JmpSrc JIT::emitNakedCall(unsigned bytecodeIndex, void* function)
{
    JmpSrc call = __ call();
    m_calls.append(CallRecord(call, reinterpret_cast<CTIHelper_v>(function), bytecodeIndex));
    return call;
}

ALWAYS_INLINE JmpSrc JIT::emitCTICall(unsigned bytecodeIndex, CTIHelper_j helper)
{
#if ENABLE(OPCODE_SAMPLING)
    __ movl_i32m(m_interpreter->sampler()->encodeSample(m_codeBlock->instructions.begin() + bytecodeIndex, true), m_interpreter->sampler()->sampleSlot());
#endif
    __ restoreArgumentReference();
    emitPutCTIParam(X86::edi, CTI_ARGS_callFrame);
    JmpSrc call = __ call();
    m_calls.append(CallRecord(call, helper, bytecodeIndex));
#if ENABLE(OPCODE_SAMPLING)
    __ movl_i32m(m_interpreter->sampler()->encodeSample(m_codeBlock->instructions.begin() + bytecodeIndex, false), m_interpreter->sampler()->sampleSlot());
#endif
    killLastResultRegister();

    return call;
}

ALWAYS_INLINE JmpSrc JIT::emitCTICall(unsigned bytecodeIndex, CTIHelper_o helper)
{
#if ENABLE(OPCODE_SAMPLING)
    __ movl_i32m(m_interpreter->sampler()->encodeSample(m_codeBlock->instructions.begin() + bytecodeIndex, true), m_interpreter->sampler()->sampleSlot());
#endif
    __ restoreArgumentReference();
    emitPutCTIParam(X86::edi, CTI_ARGS_callFrame);
    JmpSrc call = __ call();
    m_calls.append(CallRecord(call, helper, bytecodeIndex));
#if ENABLE(OPCODE_SAMPLING)
    __ movl_i32m(m_interpreter->sampler()->encodeSample(m_codeBlock->instructions.begin() + bytecodeIndex, false), m_interpreter->sampler()->sampleSlot());
#endif
    killLastResultRegister();

    return call;
}

ALWAYS_INLINE JmpSrc JIT::emitCTICall(unsigned bytecodeIndex, CTIHelper_p helper)
{
#if ENABLE(OPCODE_SAMPLING)
    __ movl_i32m(m_interpreter->sampler()->encodeSample(m_codeBlock->instructions.begin() + bytecodeIndex, true), m_interpreter->sampler()->sampleSlot());
#endif
    __ restoreArgumentReference();
    emitPutCTIParam(X86::edi, CTI_ARGS_callFrame);
    JmpSrc call = __ call();
    m_calls.append(CallRecord(call, helper, bytecodeIndex));
#if ENABLE(OPCODE_SAMPLING)
    __ movl_i32m(m_interpreter->sampler()->encodeSample(m_codeBlock->instructions.begin() + bytecodeIndex, false), m_interpreter->sampler()->sampleSlot());
#endif
    killLastResultRegister();

    return call;
}

ALWAYS_INLINE JmpSrc JIT::emitCTICall(unsigned bytecodeIndex, CTIHelper_b helper)
{
#if ENABLE(OPCODE_SAMPLING)
    __ movl_i32m(m_interpreter->sampler()->encodeSample(m_codeBlock->instructions.begin() + bytecodeIndex, true), m_interpreter->sampler()->sampleSlot());
#endif
    __ restoreArgumentReference();
    emitPutCTIParam(X86::edi, CTI_ARGS_callFrame);
    JmpSrc call = __ call();
    m_calls.append(CallRecord(call, helper, bytecodeIndex));
#if ENABLE(OPCODE_SAMPLING)
    __ movl_i32m(m_interpreter->sampler()->encodeSample(m_codeBlock->instructions.begin() + bytecodeIndex, false), m_interpreter->sampler()->sampleSlot());
#endif
    killLastResultRegister();

    return call;
}

ALWAYS_INLINE JmpSrc JIT::emitCTICall(unsigned bytecodeIndex, CTIHelper_v helper)
{
#if ENABLE(OPCODE_SAMPLING)
    __ movl_i32m(m_interpreter->sampler()->encodeSample(m_codeBlock->instructions.begin() + bytecodeIndex, true), m_interpreter->sampler()->sampleSlot());
#endif
    __ restoreArgumentReference();
    emitPutCTIParam(X86::edi, CTI_ARGS_callFrame);
    JmpSrc call = __ call();
    m_calls.append(CallRecord(call, helper, bytecodeIndex));
#if ENABLE(OPCODE_SAMPLING)
    __ movl_i32m(m_interpreter->sampler()->encodeSample(m_codeBlock->instructions.begin() + bytecodeIndex, false), m_interpreter->sampler()->sampleSlot());
#endif
    killLastResultRegister();

    return call;
}

ALWAYS_INLINE JmpSrc JIT::emitCTICall(unsigned bytecodeIndex, CTIHelper_s helper)
{
#if ENABLE(OPCODE_SAMPLING)
    __ movl_i32m(m_interpreter->sampler()->encodeSample(m_codeBlock->instructions.begin() + bytecodeIndex, true), m_interpreter->sampler()->sampleSlot());
#endif
    __ restoreArgumentReference();
    emitPutCTIParam(X86::edi, CTI_ARGS_callFrame);
    JmpSrc call = __ call();
    m_calls.append(CallRecord(call, helper, bytecodeIndex));
#if ENABLE(OPCODE_SAMPLING)
    __ movl_i32m(m_interpreter->sampler()->encodeSample(m_codeBlock->instructions.begin() + bytecodeIndex, false), m_interpreter->sampler()->sampleSlot());
#endif
    killLastResultRegister();

    return call;
}

ALWAYS_INLINE JmpSrc JIT::emitCTICall(unsigned bytecodeIndex, CTIHelper_2 helper)
{
#if ENABLE(OPCODE_SAMPLING)
    __ movl_i32m(m_interpreter->sampler()->encodeSample(m_codeBlock->instructions.begin() + bytecodeIndex, true), m_interpreter->sampler()->sampleSlot());
#endif
    __ restoreArgumentReference();
    emitPutCTIParam(X86::edi, CTI_ARGS_callFrame);
    JmpSrc call = __ call();
    m_calls.append(CallRecord(call, helper, bytecodeIndex));
#if ENABLE(OPCODE_SAMPLING)
    __ movl_i32m(m_interpreter->sampler()->encodeSample(m_codeBlock->instructions.begin() + bytecodeIndex, false), m_interpreter->sampler()->sampleSlot());
#endif
    killLastResultRegister();

    return call;
}

ALWAYS_INLINE JmpSrc JIT::checkStructure(RegisterID reg, Structure* structure)
{
    __ cmpl_i32m(reinterpret_cast<uint32_t>(structure), FIELD_OFFSET(JSCell, m_structure), reg);
    return __ jne();
}

ALWAYS_INLINE void JIT::emitJumpSlowCaseIfNotJSCell(RegisterID reg, unsigned bytecodeIndex)
{
    __ testl_i32r(JSImmediate::TagMask, reg);
    m_slowCases.append(SlowCaseEntry(__ jne(), bytecodeIndex));
}

ALWAYS_INLINE void JIT::emitJumpSlowCaseIfNotJSCell(RegisterID reg, unsigned bytecodeIndex, int vReg)
{
    if (m_codeBlock->isKnownNotImmediate(vReg))
        return;

    emitJumpSlowCaseIfNotJSCell(reg, bytecodeIndex);
}

ALWAYS_INLINE bool JIT::linkSlowCaseIfNotJSCell(const Vector<SlowCaseEntry>::iterator& iter, int vReg)
{
    if (m_codeBlock->isKnownNotImmediate(vReg))
        return false;
    
    __ link(iter->from, __ label());
    return true;
}

ALWAYS_INLINE void JIT::emitJumpSlowCaseIfNotImmNum(RegisterID reg, unsigned bytecodeIndex)
{
    __ testl_i32r(JSImmediate::TagBitTypeInteger, reg);
    m_slowCases.append(SlowCaseEntry(__ je(), bytecodeIndex));
}

ALWAYS_INLINE void JIT::emitJumpSlowCaseIfNotImmNums(RegisterID reg1, RegisterID reg2, unsigned bytecodeIndex)
{
    __ movl_rr(reg1, X86::ecx);
    __ andl_rr(reg2, X86::ecx);
    emitJumpSlowCaseIfNotImmNum(X86::ecx, bytecodeIndex);
}

ALWAYS_INLINE unsigned JIT::getDeTaggedConstantImmediate(JSValue* imm)
{
    ASSERT(JSImmediate::isNumber(imm));
    return asInteger(imm) & ~JSImmediate::TagBitTypeInteger;
}

ALWAYS_INLINE void JIT::emitFastArithDeTagImmediate(RegisterID reg)
{
    __ subl_i8r(JSImmediate::TagBitTypeInteger, reg);
}

ALWAYS_INLINE JmpSrc JIT::emitFastArithDeTagImmediateJumpIfZero(RegisterID reg)
{
    __ subl_i8r(JSImmediate::TagBitTypeInteger, reg);
    return __ je();
}

ALWAYS_INLINE void JIT::emitFastArithReTagImmediate(RegisterID reg)
{
    __ addl_i8r(JSImmediate::TagBitTypeInteger, reg);
}

ALWAYS_INLINE void JIT::emitFastArithPotentiallyReTagImmediate(RegisterID reg)
{
    __ orl_i8r(JSImmediate::TagBitTypeInteger, reg);
}

ALWAYS_INLINE void JIT::emitFastArithImmToInt(RegisterID reg)
{
    __ sarl_i8r(1, reg);
}

ALWAYS_INLINE void JIT::emitFastArithIntToImmOrSlowCase(RegisterID reg, unsigned bytecodeIndex)
{
    __ addl_rr(reg, reg);
    m_slowCases.append(SlowCaseEntry(__ jo(), bytecodeIndex));
    emitFastArithReTagImmediate(reg);
}

ALWAYS_INLINE void JIT::emitFastArithIntToImmNoCheck(RegisterID reg)
{
    __ addl_rr(reg, reg);
    emitFastArithReTagImmediate(reg);
}

ALWAYS_INLINE void JIT::emitTagAsBoolImmediate(RegisterID reg)
{
    __ shl_i8r(JSImmediate::ExtendedPayloadShift, reg);
    __ orl_i8r(JSImmediate::FullTagTypeBool, reg);
}

}

#endif // ENABLE(JIT)

#endif
