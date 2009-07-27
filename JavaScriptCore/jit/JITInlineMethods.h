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

namespace JSC {

ALWAYS_INLINE void JIT::killLastResultRegister()
{
    m_lastResultBytecodeRegister = std::numeric_limits<int>::max();
}

// get arg puts an arg from the SF register array into a h/w register
ALWAYS_INLINE void JIT::emitGetVirtualRegister(int src, RegisterID dst)
{
    ASSERT(m_bytecodeIndex != (unsigned)-1); // This method should only be called during hot/cold path generation, so that m_bytecodeIndex is set.

    // TODO: we want to reuse values that are already in registers if we can - add a register allocator!
    if (m_codeBlock->isConstantRegisterIndex(src)) {
        JSValue value = m_codeBlock->getConstant(src);
        move(ImmPtr(JSValue::encode(value)), dst);
        killLastResultRegister();
        return;
    }

    if (src == m_lastResultBytecodeRegister && m_codeBlock->isTemporaryRegisterIndex(src)) {
        bool atJumpTarget = false;
        while (m_jumpTargetsPosition < m_codeBlock->numberOfJumpTargets() && m_codeBlock->jumpTarget(m_jumpTargetsPosition) <= m_bytecodeIndex) {
            if (m_codeBlock->jumpTarget(m_jumpTargetsPosition) == m_bytecodeIndex)
                atJumpTarget = true;
            ++m_jumpTargetsPosition;
        }

        if (!atJumpTarget) {
            // The argument we want is already stored in eax
            if (dst != cachedResultRegister)
                move(cachedResultRegister, dst);
            killLastResultRegister();
            return;
        }
    }

    loadPtr(Address(callFrameRegister, src * sizeof(Register)), dst);
    killLastResultRegister();
}

ALWAYS_INLINE void JIT::emitGetVirtualRegisters(int src1, RegisterID dst1, int src2, RegisterID dst2)
{
    if (src2 == m_lastResultBytecodeRegister) {
        emitGetVirtualRegister(src2, dst2);
        emitGetVirtualRegister(src1, dst1);
    } else {
        emitGetVirtualRegister(src1, dst1);
        emitGetVirtualRegister(src2, dst2);
    }
}

// puts an arg onto the stack, as an arg to a context threaded function.
ALWAYS_INLINE void JIT::emitPutJITStubArg(RegisterID src, unsigned argumentNumber)
{
    poke(src, argumentNumber);
}

ALWAYS_INLINE void JIT::emitPutJITStubArgConstant(unsigned value, unsigned argumentNumber)
{
    poke(Imm32(value), argumentNumber);
}

ALWAYS_INLINE void JIT::emitPutJITStubArgConstant(void* value, unsigned argumentNumber)
{
    poke(ImmPtr(value), argumentNumber);
}

ALWAYS_INLINE void JIT::emitGetJITStubArg(unsigned argumentNumber, RegisterID dst)
{
    peek(dst, argumentNumber);
}

ALWAYS_INLINE JSValue JIT::getConstantOperand(unsigned src)
{
    ASSERT(m_codeBlock->isConstantRegisterIndex(src));
    return m_codeBlock->getConstant(src);
}

ALWAYS_INLINE int32_t JIT::getConstantOperandImmediateInt(unsigned src)
{
    return getConstantOperand(src).getInt32Fast();
}

ALWAYS_INLINE bool JIT::isOperandConstantImmediateInt(unsigned src)
{
    return m_codeBlock->isConstantRegisterIndex(src) && getConstantOperand(src).isInt32Fast();
}

// get arg puts an arg from the SF register array onto the stack, as an arg to a context threaded function.
ALWAYS_INLINE void JIT::emitPutJITStubArgFromVirtualRegister(unsigned src, unsigned argumentNumber, RegisterID scratch)
{
    if (m_codeBlock->isConstantRegisterIndex(src)) {
        JSValue value = m_codeBlock->getConstant(src);
        emitPutJITStubArgConstant(JSValue::encode(value), argumentNumber);
    } else {
        loadPtr(Address(callFrameRegister, src * sizeof(Register)), scratch);
        emitPutJITStubArg(scratch, argumentNumber);
    }

    killLastResultRegister();
}

ALWAYS_INLINE void JIT::emitPutToCallFrameHeader(RegisterID from, RegisterFile::CallFrameHeaderEntry entry)
{
    storePtr(from, Address(callFrameRegister, entry * sizeof(Register)));
}

ALWAYS_INLINE void JIT::emitPutImmediateToCallFrameHeader(void* value, RegisterFile::CallFrameHeaderEntry entry)
{
    storePtr(ImmPtr(value), Address(callFrameRegister, entry * sizeof(Register)));
}

ALWAYS_INLINE void JIT::emitGetFromCallFrameHeaderPtr(RegisterFile::CallFrameHeaderEntry entry, RegisterID to, RegisterID from)
{
    loadPtr(Address(from, entry * sizeof(Register)), to);
    killLastResultRegister();
}

ALWAYS_INLINE void JIT::emitGetFromCallFrameHeader32(RegisterFile::CallFrameHeaderEntry entry, RegisterID to, RegisterID from)
{
    load32(Address(from, entry * sizeof(Register)), to);
    killLastResultRegister();
}

ALWAYS_INLINE void JIT::emitPutVirtualRegister(unsigned dst, RegisterID from)
{
    storePtr(from, Address(callFrameRegister, dst * sizeof(Register)));
    m_lastResultBytecodeRegister = (from == cachedResultRegister) ? dst : std::numeric_limits<int>::max();
    // FIXME: #ifndef NDEBUG, Write the correct m_type to the register.
}

ALWAYS_INLINE void JIT::emitInitRegister(unsigned dst)
{
    storePtr(ImmPtr(JSValue::encode(jsUndefined())), Address(callFrameRegister, dst * sizeof(Register)));
    // FIXME: #ifndef NDEBUG, Write the correct m_type to the register.
}

ALWAYS_INLINE JIT::Call JIT::emitNakedCall(CodePtr function)
{
    ASSERT(m_bytecodeIndex != (unsigned)-1); // This method should only be called during hot/cold path generation, so that m_bytecodeIndex is set.

    Call nakedCall = nearCall();
    m_calls.append(CallRecord(nakedCall, m_bytecodeIndex, function.executableAddress()));
    return nakedCall;
}

#if PLATFORM(X86) || PLATFORM(X86_64)

ALWAYS_INLINE void JIT::preserveReturnAddressAfterCall(RegisterID reg)
{
    pop(reg);
}

ALWAYS_INLINE void JIT::restoreReturnAddressBeforeReturn(RegisterID reg)
{
    push(reg);
}

ALWAYS_INLINE void JIT::restoreReturnAddressBeforeReturn(Address address)
{
    push(address);
}

#elif PLATFORM_ARM_ARCH(7)

ALWAYS_INLINE void JIT::preserveReturnAddressAfterCall(RegisterID reg)
{
    move(linkRegister, reg);
}

ALWAYS_INLINE void JIT::restoreReturnAddressBeforeReturn(RegisterID reg)
{
    move(reg, linkRegister);
}

ALWAYS_INLINE void JIT::restoreReturnAddressBeforeReturn(Address address)
{
    loadPtr(address, linkRegister);
}

#endif

#if USE(JIT_STUB_ARGUMENT_VA_LIST)
ALWAYS_INLINE void JIT::restoreArgumentReference()
{
    poke(callFrameRegister, OBJECT_OFFSETOF(struct JITStackFrame, callFrame) / sizeof (void*));
}
ALWAYS_INLINE void JIT::restoreArgumentReferenceForTrampoline() {}
#else
ALWAYS_INLINE void JIT::restoreArgumentReference()
{
    move(stackPointerRegister, firstArgumentRegister);
    poke(callFrameRegister, OBJECT_OFFSETOF(struct JITStackFrame, callFrame) / sizeof (void*));
}
ALWAYS_INLINE void JIT::restoreArgumentReferenceForTrampoline()
{
#if PLATFORM(X86)
    // Within a trampoline the return address will be on the stack at this point.
    addPtr(Imm32(sizeof(void*)), stackPointerRegister, firstArgumentRegister);
#elif PLATFORM_ARM_ARCH(7)
    move(stackPointerRegister, firstArgumentRegister);
#endif
    // In the trampoline on x86-64, the first argument register is not overwritten.
}
#endif

ALWAYS_INLINE JIT::Jump JIT::checkStructure(RegisterID reg, Structure* structure)
{
    return branchPtr(NotEqual, Address(reg, OBJECT_OFFSETOF(JSCell, m_structure)), ImmPtr(structure));
}

ALWAYS_INLINE JIT::Jump JIT::emitJumpIfJSCell(RegisterID reg)
{
#if USE(ALTERNATE_JSIMMEDIATE)
    return branchTestPtr(Zero, reg, tagMaskRegister);
#else
    return branchTest32(Zero, reg, Imm32(JSImmediate::TagMask));
#endif
}

ALWAYS_INLINE JIT::Jump JIT::emitJumpIfBothJSCells(RegisterID reg1, RegisterID reg2, RegisterID scratch)
{
    move(reg1, scratch);
    orPtr(reg2, scratch);
    return emitJumpIfJSCell(scratch);
}

ALWAYS_INLINE void JIT::emitJumpSlowCaseIfJSCell(RegisterID reg)
{
    addSlowCase(emitJumpIfJSCell(reg));
}

ALWAYS_INLINE JIT::Jump JIT::emitJumpIfNotJSCell(RegisterID reg)
{
#if USE(ALTERNATE_JSIMMEDIATE)
    return branchTestPtr(NonZero, reg, tagMaskRegister);
#else
    return branchTest32(NonZero, reg, Imm32(JSImmediate::TagMask));
#endif
}

ALWAYS_INLINE void JIT::emitJumpSlowCaseIfNotJSCell(RegisterID reg)
{
    addSlowCase(emitJumpIfNotJSCell(reg));
}

ALWAYS_INLINE void JIT::emitJumpSlowCaseIfNotJSCell(RegisterID reg, int vReg)
{
    if (!m_codeBlock->isKnownNotImmediate(vReg))
        emitJumpSlowCaseIfNotJSCell(reg);
}

ALWAYS_INLINE void JIT::linkSlowCaseIfNotJSCell(Vector<SlowCaseEntry>::iterator& iter, int vReg)
{
    if (!m_codeBlock->isKnownNotImmediate(vReg))
        linkSlowCase(iter);
}

#if USE(ALTERNATE_JSIMMEDIATE)
ALWAYS_INLINE JIT::Jump JIT::emitJumpIfImmediateNumber(RegisterID reg)
{
    return branchTestPtr(NonZero, reg, tagTypeNumberRegister);
}
ALWAYS_INLINE JIT::Jump JIT::emitJumpIfNotImmediateNumber(RegisterID reg)
{
    return branchTestPtr(Zero, reg, tagTypeNumberRegister);
}
#endif

ALWAYS_INLINE JIT::Jump JIT::emitJumpIfImmediateInteger(RegisterID reg)
{
#if USE(ALTERNATE_JSIMMEDIATE)
    return branchPtr(AboveOrEqual, reg, tagTypeNumberRegister);
#else
    return branchTest32(NonZero, reg, Imm32(JSImmediate::TagTypeNumber));
#endif
}

ALWAYS_INLINE JIT::Jump JIT::emitJumpIfNotImmediateInteger(RegisterID reg)
{
#if USE(ALTERNATE_JSIMMEDIATE)
    return branchPtr(Below, reg, tagTypeNumberRegister);
#else
    return branchTest32(Zero, reg, Imm32(JSImmediate::TagTypeNumber));
#endif
}

ALWAYS_INLINE JIT::Jump JIT::emitJumpIfNotImmediateIntegers(RegisterID reg1, RegisterID reg2, RegisterID scratch)
{
    move(reg1, scratch);
    andPtr(reg2, scratch);
    return emitJumpIfNotImmediateInteger(scratch);
}

ALWAYS_INLINE void JIT::emitJumpSlowCaseIfNotImmediateInteger(RegisterID reg)
{
    addSlowCase(emitJumpIfNotImmediateInteger(reg));
}

ALWAYS_INLINE void JIT::emitJumpSlowCaseIfNotImmediateIntegers(RegisterID reg1, RegisterID reg2, RegisterID scratch)
{
    addSlowCase(emitJumpIfNotImmediateIntegers(reg1, reg2, scratch));
}

#if !USE(ALTERNATE_JSIMMEDIATE)
ALWAYS_INLINE void JIT::emitFastArithDeTagImmediate(RegisterID reg)
{
    subPtr(Imm32(JSImmediate::TagTypeNumber), reg);
}

ALWAYS_INLINE JIT::Jump JIT::emitFastArithDeTagImmediateJumpIfZero(RegisterID reg)
{
    return branchSubPtr(Zero, Imm32(JSImmediate::TagTypeNumber), reg);
}
#endif

ALWAYS_INLINE void JIT::emitFastArithReTagImmediate(RegisterID src, RegisterID dest)
{
#if USE(ALTERNATE_JSIMMEDIATE)
    emitFastArithIntToImmNoCheck(src, dest);
#else
    if (src != dest)
        move(src, dest);
    addPtr(Imm32(JSImmediate::TagTypeNumber), dest);
#endif
}

ALWAYS_INLINE void JIT::emitFastArithImmToInt(RegisterID reg)
{
#if USE(ALTERNATE_JSIMMEDIATE)
    UNUSED_PARAM(reg);
#else
    rshiftPtr(Imm32(JSImmediate::IntegerPayloadShift), reg);
#endif
}

// operand is int32_t, must have been zero-extended if register is 64-bit.
ALWAYS_INLINE void JIT::emitFastArithIntToImmNoCheck(RegisterID src, RegisterID dest)
{
#if USE(ALTERNATE_JSIMMEDIATE)
    if (src != dest)
        move(src, dest);
    orPtr(tagTypeNumberRegister, dest);
#else
    signExtend32ToPtr(src, dest);
    addPtr(dest, dest);
    emitFastArithReTagImmediate(dest, dest);
#endif
}

ALWAYS_INLINE void JIT::emitTagAsBoolImmediate(RegisterID reg)
{
    lshift32(Imm32(JSImmediate::ExtendedPayloadShift), reg);
    or32(Imm32(static_cast<int32_t>(JSImmediate::FullTagTypeBool)), reg);
}

ALWAYS_INLINE void JIT::addSlowCase(Jump jump)
{
    ASSERT(m_bytecodeIndex != (unsigned)-1); // This method should only be called during hot/cold path generation, so that m_bytecodeIndex is set.

    m_slowCases.append(SlowCaseEntry(jump, m_bytecodeIndex));
}

ALWAYS_INLINE void JIT::addJump(Jump jump, int relativeOffset)
{
    ASSERT(m_bytecodeIndex != (unsigned)-1); // This method should only be called during hot/cold path generation, so that m_bytecodeIndex is set.

    m_jmpTable.append(JumpTable(jump, m_bytecodeIndex + relativeOffset));
}

ALWAYS_INLINE void JIT::emitJumpSlowToHot(Jump jump, int relativeOffset)
{
    ASSERT(m_bytecodeIndex != (unsigned)-1); // This method should only be called during hot/cold path generation, so that m_bytecodeIndex is set.

    jump.linkTo(m_labels[m_bytecodeIndex + relativeOffset], this);
}

#if ENABLE(SAMPLING_FLAGS)
ALWAYS_INLINE void JIT::setSamplingFlag(int32_t flag)
{
    ASSERT(flag >= 1);
    ASSERT(flag <= 32);
    or32(Imm32(1u << (flag - 1)), AbsoluteAddress(&SamplingFlags::s_flags));
}

ALWAYS_INLINE void JIT::clearSamplingFlag(int32_t flag)
{
    ASSERT(flag >= 1);
    ASSERT(flag <= 32);
    and32(Imm32(~(1u << (flag - 1))), AbsoluteAddress(&SamplingFlags::s_flags));
}
#endif

#if ENABLE(SAMPLING_COUNTERS)
ALWAYS_INLINE void JIT::emitCount(AbstractSamplingCounter& counter, uint32_t count)
{
#if PLATFORM(X86_64) // Or any other 64-bit plattform.
    addPtr(Imm32(count), AbsoluteAddress(&counter.m_counter));
#elif PLATFORM(X86) // Or any other little-endian 32-bit plattform.
    intptr_t hiWord = reinterpret_cast<intptr_t>(&counter.m_counter) + sizeof(int32_t);
    add32(Imm32(count), AbsoluteAddress(&counter.m_counter));
    addWithCarry32(Imm32(0), AbsoluteAddress(reinterpret_cast<void*>(hiWord)));
#else
#error "SAMPLING_FLAGS not implemented on this platform."
#endif
}
#endif

#if ENABLE(OPCODE_SAMPLING)
#if PLATFORM(X86_64)
ALWAYS_INLINE void JIT::sampleInstruction(Instruction* instruction, bool inHostFunction)
{
    move(ImmPtr(m_interpreter->sampler()->sampleSlot()), X86::ecx);
    storePtr(ImmPtr(m_interpreter->sampler()->encodeSample(instruction, inHostFunction)), X86::ecx);
}
#else
ALWAYS_INLINE void JIT::sampleInstruction(Instruction* instruction, bool inHostFunction)
{
    storePtr(ImmPtr(m_interpreter->sampler()->encodeSample(instruction, inHostFunction)), m_interpreter->sampler()->sampleSlot());
}
#endif
#endif

#if ENABLE(CODEBLOCK_SAMPLING)
#if PLATFORM(X86_64)
ALWAYS_INLINE void JIT::sampleCodeBlock(CodeBlock* codeBlock)
{
    move(ImmPtr(m_interpreter->sampler()->codeBlockSlot()), X86::ecx);
    storePtr(ImmPtr(codeBlock), X86::ecx);
}
#else
ALWAYS_INLINE void JIT::sampleCodeBlock(CodeBlock* codeBlock)
{
    storePtr(ImmPtr(codeBlock), m_interpreter->sampler()->codeBlockSlot());
}
#endif
#endif
}

#endif // ENABLE(JIT)

#endif
