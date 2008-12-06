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
        move(value, dst);
        killLastResultRegister();
        return;
    }

    if (src == m_lastResultBytecodeRegister && m_codeBlock->isTemporaryRegisterIndex(src)) {
        bool atJumpTarget = false;
        while (m_jumpTargetsPosition < m_codeBlock->numberOfJumpTargets() && m_codeBlock->jumpTarget(m_jumpTargetsPosition) <= currentInstructionIndex) {
            if (m_codeBlock->jumpTarget(m_jumpTargetsPosition) == currentInstructionIndex)
                atJumpTarget = true;
            ++m_jumpTargetsPosition;
        }

        if (!atJumpTarget) {
            // The argument we want is already stored in eax
            if (dst != X86::eax)
                move(X86::eax, dst);
            killLastResultRegister();
            return;
        }
    }

    loadPtr(Address(callFrameRegister, src * sizeof(Register)), dst);
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

// puts an arg onto the stack, as an arg to a context threaded function.
ALWAYS_INLINE void JIT::emitPutCTIArg(RegisterID src, unsigned offset)
{
    poke(src, (offset / sizeof(void*)) + 1);
}

ALWAYS_INLINE void JIT::emitPutCTIArgConstant(unsigned value, unsigned offset)
{
    poke(Imm32(value), (offset / sizeof(void*)) + 1);
}

ALWAYS_INLINE void JIT::emitPutCTIArgConstant(void* value, unsigned offset)
{
    poke(value, (offset / sizeof(void*)) + 1);
}

ALWAYS_INLINE void JIT::emitGetCTIArg(unsigned offset, RegisterID dst)
{
    peek(dst, (offset / sizeof(void*)) + 1);
}

ALWAYS_INLINE JSValue* JIT::getConstantImmediateNumericArg(unsigned src)
{
    if (m_codeBlock->isConstantRegisterIndex(src)) {
        JSValue* value = m_codeBlock->getConstant(src);
        return JSImmediate::isNumber(value) ? value : noValue();
    }
    return noValue();
}

// get arg puts an arg from the SF register array onto the stack, as an arg to a context threaded function.
ALWAYS_INLINE void JIT::emitPutCTIArgFromVirtualRegister(unsigned src, unsigned offset, RegisterID scratch)
{
    if (m_codeBlock->isConstantRegisterIndex(src)) {
        JSValue* value = m_codeBlock->getConstant(src);
        emitPutCTIArgConstant(value, offset);
    } else {
        loadPtr(Address(callFrameRegister, src * sizeof(Register)), scratch);
        emitPutCTIArg(scratch, offset);
    }

    killLastResultRegister();
}

ALWAYS_INLINE void JIT::emitPutCTIParam(void* value, unsigned name)
{
    poke(value, name);
}

ALWAYS_INLINE void JIT::emitPutCTIParam(RegisterID from, unsigned name)
{
    poke(from, name);
}

ALWAYS_INLINE void JIT::emitGetCTIParam(unsigned name, RegisterID to)
{
    peek(to, name);
    killLastResultRegister();
}

ALWAYS_INLINE void JIT::emitPutToCallFrameHeader(RegisterID from, RegisterFile::CallFrameHeaderEntry entry)
{
    storePtr(from, Address(callFrameRegister, entry * sizeof(Register)));
}

ALWAYS_INLINE void JIT::emitPutImmediateToCallFrameHeader(void* value, RegisterFile::CallFrameHeaderEntry entry)
{
    storePtr(value, Address(callFrameRegister, entry * sizeof(Register)));
}

ALWAYS_INLINE void JIT::emitGetFromCallFrameHeader(RegisterFile::CallFrameHeaderEntry entry, RegisterID to)
{
    loadPtr(Address(callFrameRegister, entry * sizeof(Register)), to);
    killLastResultRegister();
}

ALWAYS_INLINE void JIT::emitPutVirtualRegister(unsigned dst, RegisterID from)
{
    storePtr(from, Address(callFrameRegister, dst * sizeof(Register)));
    m_lastResultBytecodeRegister = (from == X86::eax) ? dst : std::numeric_limits<int>::max();
    // FIXME: #ifndef NDEBUG, Write the correct m_type to the register.
}

ALWAYS_INLINE void JIT::emitInitRegister(unsigned dst)
{
    storePtr(jsUndefined(), Address(callFrameRegister, dst * sizeof(Register)));
    // FIXME: #ifndef NDEBUG, Write the correct m_type to the register.
}

ALWAYS_INLINE JmpSrc JIT::emitNakedCall(unsigned bytecodeIndex, X86::RegisterID r)
{
    JmpSrc nakedCall = call(r);
    m_calls.append(CallRecord(nakedCall, bytecodeIndex));
    return nakedCall;
}

ALWAYS_INLINE JmpSrc JIT::emitNakedCall(unsigned bytecodeIndex, void* function)
{
    JmpSrc nakedCall = call();
    m_calls.append(CallRecord(nakedCall, reinterpret_cast<CTIHelper_v>(function), bytecodeIndex));
    return nakedCall;
}

ALWAYS_INLINE void JIT::restoreArgumentReference()
{
#if USE(CTI_ARGUMENT)
#if USE(FAST_CALL_CTI_ARGUMENT)
    __ movl_rr(X86::esp, X86::ecx);
#else
    __ movl_rm(X86::esp, 0, X86::esp);
#endif
#endif
}

ALWAYS_INLINE void JIT::restoreArgumentReferenceForTrampoline()
{
#if USE(CTI_ARGUMENT) && USE(FAST_CALL_CTI_ARGUMENT)
    __ movl_rr(X86::esp, X86::ecx);
    __ addl_i32r(4, X86::ecx);
#endif
}


ALWAYS_INLINE JmpSrc JIT::emitCTICall(unsigned bytecodeIndex, CTIHelper_j helper)
{
#if ENABLE(OPCODE_SAMPLING)
    store32(Imm32(m_interpreter->sampler()->encodeSample(m_codeBlock->instructions().begin() + bytecodeIndex, true)), m_interpreter->sampler()->sampleSlot());
#endif
    restoreArgumentReference();
    emitPutCTIParam(callFrameRegister, CTI_ARGS_callFrame);
    JmpSrc ctiCall = call();
    m_calls.append(CallRecord(ctiCall, helper, bytecodeIndex));
#if ENABLE(OPCODE_SAMPLING)
    store32(Imm32(m_interpreter->sampler()->encodeSample(m_codeBlock->instructions().begin() + bytecodeIndex, false)), m_interpreter->sampler()->sampleSlot());
#endif
    killLastResultRegister();

    return ctiCall;
}

ALWAYS_INLINE JmpSrc JIT::emitCTICall(unsigned bytecodeIndex, CTIHelper_o helper)
{
#if ENABLE(OPCODE_SAMPLING)
    store32(Imm32(m_interpreter->sampler()->encodeSample(m_codeBlock->instructions().begin() + bytecodeIndex, true)), m_interpreter->sampler()->sampleSlot());
#endif
    restoreArgumentReference();
    emitPutCTIParam(callFrameRegister, CTI_ARGS_callFrame);
    JmpSrc ctiCall = call();
    m_calls.append(CallRecord(ctiCall, helper, bytecodeIndex));
#if ENABLE(OPCODE_SAMPLING)
    store32(Imm32(m_interpreter->sampler()->encodeSample(m_codeBlock->instructions().begin() + bytecodeIndex, false)), m_interpreter->sampler()->sampleSlot());
#endif
    killLastResultRegister();

    return ctiCall;
}

ALWAYS_INLINE JmpSrc JIT::emitCTICall(unsigned bytecodeIndex, CTIHelper_p helper)
{
#if ENABLE(OPCODE_SAMPLING)
    store32(Imm32(m_interpreter->sampler()->encodeSample(m_codeBlock->instructions().begin() + bytecodeIndex, true)), m_interpreter->sampler()->sampleSlot());
#endif
    restoreArgumentReference();
    emitPutCTIParam(callFrameRegister, CTI_ARGS_callFrame);
    JmpSrc ctiCall = call();
    m_calls.append(CallRecord(ctiCall, helper, bytecodeIndex));
#if ENABLE(OPCODE_SAMPLING)
    store32(Imm32(m_interpreter->sampler()->encodeSample(m_codeBlock->instructions().begin() + bytecodeIndex, false)), m_interpreter->sampler()->sampleSlot());
#endif
    killLastResultRegister();

    return ctiCall;
}

ALWAYS_INLINE JmpSrc JIT::emitCTICall(unsigned bytecodeIndex, CTIHelper_b helper)
{
#if ENABLE(OPCODE_SAMPLING)
    store32(Imm32(m_interpreter->sampler()->encodeSample(m_codeBlock->instructions().begin() + bytecodeIndex, true)), m_interpreter->sampler()->sampleSlot());
#endif
    restoreArgumentReference();
    emitPutCTIParam(callFrameRegister, CTI_ARGS_callFrame);
    JmpSrc ctiCall = call();
    m_calls.append(CallRecord(ctiCall, helper, bytecodeIndex));
#if ENABLE(OPCODE_SAMPLING)
    store32(Imm32(m_interpreter->sampler()->encodeSample(m_codeBlock->instructions().begin() + bytecodeIndex, false)), m_interpreter->sampler()->sampleSlot());
#endif
    killLastResultRegister();

    return ctiCall;
}

ALWAYS_INLINE JmpSrc JIT::emitCTICall(unsigned bytecodeIndex, CTIHelper_v helper)
{
#if ENABLE(OPCODE_SAMPLING)
    store32(Imm32(m_interpreter->sampler()->encodeSample(m_codeBlock->instructions().begin() + bytecodeIndex, true)), m_interpreter->sampler()->sampleSlot());
#endif
    restoreArgumentReference();
    emitPutCTIParam(callFrameRegister, CTI_ARGS_callFrame);
    JmpSrc ctiCall = call();
    m_calls.append(CallRecord(ctiCall, helper, bytecodeIndex));
#if ENABLE(OPCODE_SAMPLING)
    store32(Imm32(m_interpreter->sampler()->encodeSample(m_codeBlock->instructions().begin() + bytecodeIndex, false)), m_interpreter->sampler()->sampleSlot());
#endif
    killLastResultRegister();

    return ctiCall;
}

ALWAYS_INLINE JmpSrc JIT::emitCTICall(unsigned bytecodeIndex, CTIHelper_s helper)
{
#if ENABLE(OPCODE_SAMPLING)
    store32(Imm32(m_interpreter->sampler()->encodeSample(m_codeBlock->instructions().begin() + bytecodeIndex, true)), m_interpreter->sampler()->sampleSlot());
#endif
    restoreArgumentReference();
    emitPutCTIParam(callFrameRegister, CTI_ARGS_callFrame);
    JmpSrc ctiCall = call();
    m_calls.append(CallRecord(ctiCall, helper, bytecodeIndex));
#if ENABLE(OPCODE_SAMPLING)
    store32(Imm32(m_interpreter->sampler()->encodeSample(m_codeBlock->instructions().begin() + bytecodeIndex, false)), m_interpreter->sampler()->sampleSlot());
#endif
    killLastResultRegister();

    return ctiCall;
}

ALWAYS_INLINE JmpSrc JIT::emitCTICall(unsigned bytecodeIndex, CTIHelper_2 helper)
{
#if ENABLE(OPCODE_SAMPLING)
    store32(Imm32(m_interpreter->sampler()->encodeSample(m_codeBlock->instructions().begin() + bytecodeIndex, true)), m_interpreter->sampler()->sampleSlot());
#endif
    restoreArgumentReference();
    emitPutCTIParam(callFrameRegister, CTI_ARGS_callFrame);
    JmpSrc ctiCall = call();
    m_calls.append(CallRecord(ctiCall, helper, bytecodeIndex));
#if ENABLE(OPCODE_SAMPLING)
    store32(Imm32(m_interpreter->sampler()->encodeSample(m_codeBlock->instructions().begin() + bytecodeIndex, false)), m_interpreter->sampler()->sampleSlot());
#endif
    killLastResultRegister();

    return ctiCall;
}

ALWAYS_INLINE JmpSrc JIT::checkStructure(RegisterID reg, Structure* structure)
{
    return jnePtr(structure, Address(reg, FIELD_OFFSET(JSCell, m_structure)));
}

ALWAYS_INLINE JIT::Jump JIT::emitJumpIfJSCell(RegisterID reg)
{
    return jnset32(Imm32(JSImmediate::TagMask), reg);
}

ALWAYS_INLINE void JIT::emitJumpSlowCaseIfJSCell(RegisterID reg, unsigned bytecodeIndex)
{
    m_slowCases.append(SlowCaseEntry(emitJumpIfJSCell(reg), bytecodeIndex));
}

ALWAYS_INLINE JIT::Jump JIT::emitJumpIfNotJSCell(RegisterID reg)
{
    return jset32(Imm32(JSImmediate::TagMask), reg);
}

ALWAYS_INLINE void JIT::emitJumpSlowCaseIfNotJSCell(RegisterID reg, unsigned bytecodeIndex)
{
    m_slowCases.append(SlowCaseEntry(emitJumpIfNotJSCell(reg), bytecodeIndex));
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
    m_slowCases.append(SlowCaseEntry(jnset32(Imm32(JSImmediate::TagBitTypeInteger), reg), bytecodeIndex));
}

ALWAYS_INLINE void JIT::emitJumpSlowCaseIfNotImmNums(RegisterID reg1, RegisterID reg2, RegisterID scratch, unsigned bytecodeIndex)
{
    move(reg1, scratch);
    and32(reg2, scratch);
    emitJumpSlowCaseIfNotImmNum(scratch, bytecodeIndex);
}

ALWAYS_INLINE unsigned JIT::getDeTaggedConstantImmediate(JSValue* imm)
{
    ASSERT(JSImmediate::isNumber(imm));
    return asInteger(imm) & ~JSImmediate::TagBitTypeInteger;
}

ALWAYS_INLINE void JIT::emitFastArithDeTagImmediate(RegisterID reg)
{
    sub32(Imm32(JSImmediate::TagBitTypeInteger), reg);
}

ALWAYS_INLINE JmpSrc JIT::emitFastArithDeTagImmediateJumpIfZero(RegisterID reg)
{
    return jzSub32(Imm32(JSImmediate::TagBitTypeInteger), reg);
}

ALWAYS_INLINE void JIT::emitFastArithReTagImmediate(RegisterID reg)
{
    add32(Imm32(JSImmediate::TagBitTypeInteger), reg);
}

ALWAYS_INLINE void JIT::emitFastArithPotentiallyReTagImmediate(RegisterID reg)
{
    or32(Imm32(JSImmediate::TagBitTypeInteger), reg);
}

ALWAYS_INLINE void JIT::emitFastArithImmToInt(RegisterID reg)
{
    rshift32(Imm32(1), reg);
}

ALWAYS_INLINE void JIT::emitFastArithIntToImmOrSlowCase(RegisterID reg, unsigned bytecodeIndex)
{
    m_slowCases.append(SlowCaseEntry(joAdd32(reg, reg), bytecodeIndex));
    emitFastArithReTagImmediate(reg);
}

ALWAYS_INLINE void JIT::emitFastArithIntToImmNoCheck(RegisterID reg)
{
    add32(reg, reg);
    emitFastArithReTagImmediate(reg);
}

ALWAYS_INLINE void JIT::emitTagAsBoolImmediate(RegisterID reg)
{
    lshift32(Imm32(JSImmediate::ExtendedPayloadShift), reg);
    or32(Imm32(JSImmediate::FullTagTypeBool), reg);
}

}

#endif // ENABLE(JIT)

#endif
