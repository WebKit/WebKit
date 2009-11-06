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

/* Deprecated: Please use JITStubCall instead. */

// puts an arg onto the stack, as an arg to a context threaded function.
ALWAYS_INLINE void JIT::emitPutJITStubArg(RegisterID src, unsigned argumentNumber)
{
    unsigned argumentStackOffset = (argumentNumber * (sizeof(JSValue) / sizeof(void*))) + JITSTACKFRAME_ARGS_INDEX;
    poke(src, argumentStackOffset);
}

/* Deprecated: Please use JITStubCall instead. */

ALWAYS_INLINE void JIT::emitPutJITStubArgConstant(unsigned value, unsigned argumentNumber)
{
    unsigned argumentStackOffset = (argumentNumber * (sizeof(JSValue) / sizeof(void*))) + JITSTACKFRAME_ARGS_INDEX;
    poke(Imm32(value), argumentStackOffset);
}

/* Deprecated: Please use JITStubCall instead. */

ALWAYS_INLINE void JIT::emitPutJITStubArgConstant(void* value, unsigned argumentNumber)
{
    unsigned argumentStackOffset = (argumentNumber * (sizeof(JSValue) / sizeof(void*))) + JITSTACKFRAME_ARGS_INDEX;
    poke(ImmPtr(value), argumentStackOffset);
}

/* Deprecated: Please use JITStubCall instead. */

ALWAYS_INLINE void JIT::emitGetJITStubArg(unsigned argumentNumber, RegisterID dst)
{
    unsigned argumentStackOffset = (argumentNumber * (sizeof(JSValue) / sizeof(void*))) + JITSTACKFRAME_ARGS_INDEX;
    peek(dst, argumentStackOffset);
}

ALWAYS_INLINE bool JIT::isOperandConstantImmediateDouble(unsigned src)
{
    return m_codeBlock->isConstantRegisterIndex(src) && getConstantOperand(src).isDouble();
}

ALWAYS_INLINE JSValue JIT::getConstantOperand(unsigned src)
{
    ASSERT(m_codeBlock->isConstantRegisterIndex(src));
    return m_codeBlock->getConstant(src);
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
#if !USE(JSVALUE32_64)
    killLastResultRegister();
#endif
}

ALWAYS_INLINE void JIT::emitGetFromCallFrameHeader32(RegisterFile::CallFrameHeaderEntry entry, RegisterID to, RegisterID from)
{
    load32(Address(from, entry * sizeof(Register)), to);
#if !USE(JSVALUE32_64)
    killLastResultRegister();
#endif
}

ALWAYS_INLINE JIT::Call JIT::emitNakedCall(CodePtr function)
{
    ASSERT(m_bytecodeIndex != (unsigned)-1); // This method should only be called during hot/cold path generation, so that m_bytecodeIndex is set.

    Call nakedCall = nearCall();
    m_calls.append(CallRecord(nakedCall, m_bytecodeIndex, function.executableAddress()));
    return nakedCall;
}

#if defined(ASSEMBLER_HAS_CONSTANT_POOL) && ASSEMBLER_HAS_CONSTANT_POOL

ALWAYS_INLINE void JIT::beginUninterruptedSequence(int insnSpace, int constSpace)
{
#if PLATFORM(ARM_TRADITIONAL)
#ifndef NDEBUG
    // Ensure the label after the sequence can also fit
    insnSpace += sizeof(ARMWord);
    constSpace += sizeof(uint64_t);
#endif

    ensureSpace(insnSpace, constSpace);

#endif

#if defined(ASSEMBLER_HAS_CONSTANT_POOL) && ASSEMBLER_HAS_CONSTANT_POOL
#ifndef NDEBUG
    m_uninterruptedInstructionSequenceBegin = label();
    m_uninterruptedConstantSequenceBegin = sizeOfConstantPool();
#endif
#endif
}

ALWAYS_INLINE void JIT::endUninterruptedSequence(int insnSpace, int constSpace)
{
#if defined(ASSEMBLER_HAS_CONSTANT_POOL) && ASSEMBLER_HAS_CONSTANT_POOL
    ASSERT(differenceBetween(m_uninterruptedInstructionSequenceBegin, label()) == insnSpace);
    ASSERT(sizeOfConstantPool() - m_uninterruptedConstantSequenceBegin == constSpace);
#endif
}

#endif

#if PLATFORM(ARM)

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

#else // PLATFORM(X86) || PLATFORM(X86_64)

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
#elif PLATFORM(ARM)
    move(stackPointerRegister, firstArgumentRegister);
#endif
    // In the trampoline on x86-64, the first argument register is not overwritten.
}
#endif

ALWAYS_INLINE JIT::Jump JIT::checkStructure(RegisterID reg, Structure* structure)
{
    return branchPtr(NotEqual, Address(reg, OBJECT_OFFSETOF(JSCell, m_structure)), ImmPtr(structure));
}

ALWAYS_INLINE void JIT::linkSlowCaseIfNotJSCell(Vector<SlowCaseEntry>::iterator& iter, int vReg)
{
    if (!m_codeBlock->isKnownNotImmediate(vReg))
        linkSlowCase(iter);
}

ALWAYS_INLINE void JIT::addSlowCase(Jump jump)
{
    ASSERT(m_bytecodeIndex != (unsigned)-1); // This method should only be called during hot/cold path generation, so that m_bytecodeIndex is set.

    m_slowCases.append(SlowCaseEntry(jump, m_bytecodeIndex));
}

ALWAYS_INLINE void JIT::addSlowCase(JumpList jumpList)
{
    ASSERT(m_bytecodeIndex != (unsigned)-1); // This method should only be called during hot/cold path generation, so that m_bytecodeIndex is set.

    const JumpList::JumpVector& jumpVector = jumpList.jumps();
    size_t size = jumpVector.size();
    for (size_t i = 0; i < size; ++i)
        m_slowCases.append(SlowCaseEntry(jumpVector[i], m_bytecodeIndex));
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
    move(ImmPtr(m_interpreter->sampler()->sampleSlot()), X86Registers::ecx);
    storePtr(ImmPtr(m_interpreter->sampler()->encodeSample(instruction, inHostFunction)), X86Registers::ecx);
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
    move(ImmPtr(m_interpreter->sampler()->codeBlockSlot()), X86Registers::ecx);
    storePtr(ImmPtr(codeBlock), X86Registers::ecx);
}
#else
ALWAYS_INLINE void JIT::sampleCodeBlock(CodeBlock* codeBlock)
{
    storePtr(ImmPtr(codeBlock), m_interpreter->sampler()->codeBlockSlot());
}
#endif
#endif

inline JIT::Address JIT::addressFor(unsigned index, RegisterID base)
{
    return Address(base, (index * sizeof(Register)));
}

#if USE(JSVALUE32_64)

inline JIT::Address JIT::tagFor(unsigned index, RegisterID base)
{
    return Address(base, (index * sizeof(Register)) + OBJECT_OFFSETOF(JSValue, u.asBits.tag));
}

inline JIT::Address JIT::payloadFor(unsigned index, RegisterID base)
{
    return Address(base, (index * sizeof(Register)) + OBJECT_OFFSETOF(JSValue, u.asBits.payload));
}

inline void JIT::emitLoadTag(unsigned index, RegisterID tag)
{
    RegisterID mappedTag;
    if (getMappedTag(index, mappedTag)) {
        move(mappedTag, tag);
        unmap(tag);
        return;
    }

    if (m_codeBlock->isConstantRegisterIndex(index)) {
        move(Imm32(getConstantOperand(index).tag()), tag);
        unmap(tag);
        return;
    }

    load32(tagFor(index), tag);
    unmap(tag);
}

inline void JIT::emitLoadPayload(unsigned index, RegisterID payload)
{
    RegisterID mappedPayload;
    if (getMappedPayload(index, mappedPayload)) {
        move(mappedPayload, payload);
        unmap(payload);
        return;
    }

    if (m_codeBlock->isConstantRegisterIndex(index)) {
        move(Imm32(getConstantOperand(index).payload()), payload);
        unmap(payload);
        return;
    }

    load32(payloadFor(index), payload);
    unmap(payload);
}

inline void JIT::emitLoad(const JSValue& v, RegisterID tag, RegisterID payload)
{
    move(Imm32(v.payload()), payload);
    move(Imm32(v.tag()), tag);
}

inline void JIT::emitLoad(unsigned index, RegisterID tag, RegisterID payload, RegisterID base)
{
    ASSERT(tag != payload);

    if (base == callFrameRegister) {
        ASSERT(payload != base);
        emitLoadPayload(index, payload);
        emitLoadTag(index, tag);
        return;
    }

    if (payload == base) { // avoid stomping base
        load32(tagFor(index, base), tag);
        load32(payloadFor(index, base), payload);
        return;
    }

    load32(payloadFor(index, base), payload);
    load32(tagFor(index, base), tag);
}

inline void JIT::emitLoad2(unsigned index1, RegisterID tag1, RegisterID payload1, unsigned index2, RegisterID tag2, RegisterID payload2)
{
    if (isMapped(index1)) {
        emitLoad(index1, tag1, payload1);
        emitLoad(index2, tag2, payload2);
        return;
    }
    emitLoad(index2, tag2, payload2);
    emitLoad(index1, tag1, payload1);
}

inline void JIT::emitLoadDouble(unsigned index, FPRegisterID value)
{
    if (m_codeBlock->isConstantRegisterIndex(index)) {
        Register& inConstantPool = m_codeBlock->constantRegister(index);
        loadDouble(&inConstantPool, value);
    } else
        loadDouble(addressFor(index), value);
}

inline void JIT::emitLoadInt32ToDouble(unsigned index, FPRegisterID value)
{
    if (m_codeBlock->isConstantRegisterIndex(index)) {
        Register& inConstantPool = m_codeBlock->constantRegister(index);
        char* bytePointer = reinterpret_cast<char*>(&inConstantPool);
        convertInt32ToDouble(AbsoluteAddress(bytePointer + OBJECT_OFFSETOF(JSValue, u.asBits.payload)), value);
    } else
        convertInt32ToDouble(payloadFor(index), value);
}

inline void JIT::emitStore(unsigned index, RegisterID tag, RegisterID payload, RegisterID base)
{
    store32(payload, payloadFor(index, base));
    store32(tag, tagFor(index, base));
}

inline void JIT::emitStoreInt32(unsigned index, RegisterID payload, bool indexIsInt32)
{
    store32(payload, payloadFor(index, callFrameRegister));
    if (!indexIsInt32)
        store32(Imm32(JSValue::Int32Tag), tagFor(index, callFrameRegister));
}

inline void JIT::emitStoreInt32(unsigned index, Imm32 payload, bool indexIsInt32)
{
    store32(payload, payloadFor(index, callFrameRegister));
    if (!indexIsInt32)
        store32(Imm32(JSValue::Int32Tag), tagFor(index, callFrameRegister));
}

inline void JIT::emitStoreCell(unsigned index, RegisterID payload, bool indexIsCell)
{
    store32(payload, payloadFor(index, callFrameRegister));
    if (!indexIsCell)
        store32(Imm32(JSValue::CellTag), tagFor(index, callFrameRegister));
}

inline void JIT::emitStoreBool(unsigned index, RegisterID tag, bool indexIsBool)
{
    if (!indexIsBool)
        store32(Imm32(0), payloadFor(index, callFrameRegister));
    store32(tag, tagFor(index, callFrameRegister));
}

inline void JIT::emitStoreDouble(unsigned index, FPRegisterID value)
{
    storeDouble(value, addressFor(index));
}

inline void JIT::emitStore(unsigned index, const JSValue constant, RegisterID base)
{
    store32(Imm32(constant.payload()), payloadFor(index, base));
    store32(Imm32(constant.tag()), tagFor(index, base));
}

ALWAYS_INLINE void JIT::emitInitRegister(unsigned dst)
{
    emitStore(dst, jsUndefined());
}

inline bool JIT::isLabeled(unsigned bytecodeIndex)
{
    for (size_t numberOfJumpTargets = m_codeBlock->numberOfJumpTargets(); m_jumpTargetIndex != numberOfJumpTargets; ++m_jumpTargetIndex) {
        unsigned jumpTarget = m_codeBlock->jumpTarget(m_jumpTargetIndex);
        if (jumpTarget == bytecodeIndex)
            return true;
        if (jumpTarget > bytecodeIndex)
            return false;
    }
    return false;
}

inline void JIT::map(unsigned bytecodeIndex, unsigned virtualRegisterIndex, RegisterID tag, RegisterID payload)
{
    if (isLabeled(bytecodeIndex))
        return;

    m_mappedBytecodeIndex = bytecodeIndex;
    m_mappedVirtualRegisterIndex = virtualRegisterIndex;
    m_mappedTag = tag;
    m_mappedPayload = payload;
}

inline void JIT::unmap(RegisterID registerID)
{
    if (m_mappedTag == registerID)
        m_mappedTag = (RegisterID)-1;
    else if (m_mappedPayload == registerID)
        m_mappedPayload = (RegisterID)-1;
}

inline void JIT::unmap()
{
    m_mappedBytecodeIndex = (unsigned)-1;
    m_mappedVirtualRegisterIndex = (unsigned)-1;
    m_mappedTag = (RegisterID)-1;
    m_mappedPayload = (RegisterID)-1;
}

inline bool JIT::isMapped(unsigned virtualRegisterIndex)
{
    if (m_mappedBytecodeIndex != m_bytecodeIndex)
        return false;
    if (m_mappedVirtualRegisterIndex != virtualRegisterIndex)
        return false;
    return true;
}

inline bool JIT::getMappedPayload(unsigned virtualRegisterIndex, RegisterID& payload)
{
    if (m_mappedBytecodeIndex != m_bytecodeIndex)
        return false;
    if (m_mappedVirtualRegisterIndex != virtualRegisterIndex)
        return false;
    if (m_mappedPayload == (RegisterID)-1)
        return false;
    payload = m_mappedPayload;
    return true;
}

inline bool JIT::getMappedTag(unsigned virtualRegisterIndex, RegisterID& tag)
{
    if (m_mappedBytecodeIndex != m_bytecodeIndex)
        return false;
    if (m_mappedVirtualRegisterIndex != virtualRegisterIndex)
        return false;
    if (m_mappedTag == (RegisterID)-1)
        return false;
    tag = m_mappedTag;
    return true;
}

inline void JIT::emitJumpSlowCaseIfNotJSCell(unsigned virtualRegisterIndex)
{
    if (!m_codeBlock->isKnownNotImmediate(virtualRegisterIndex))
        addSlowCase(branch32(NotEqual, tagFor(virtualRegisterIndex), Imm32(JSValue::CellTag)));
}

inline void JIT::emitJumpSlowCaseIfNotJSCell(unsigned virtualRegisterIndex, RegisterID tag)
{
    if (!m_codeBlock->isKnownNotImmediate(virtualRegisterIndex))
        addSlowCase(branch32(NotEqual, tag, Imm32(JSValue::CellTag)));
}

inline void JIT::linkSlowCaseIfNotJSCell(Vector<SlowCaseEntry>::iterator& iter, unsigned virtualRegisterIndex)
{
    if (!m_codeBlock->isKnownNotImmediate(virtualRegisterIndex))
        linkSlowCase(iter);
}

ALWAYS_INLINE bool JIT::isOperandConstantImmediateInt(unsigned src)
{
    return m_codeBlock->isConstantRegisterIndex(src) && getConstantOperand(src).isInt32();
}

ALWAYS_INLINE bool JIT::getOperandConstantImmediateInt(unsigned op1, unsigned op2, unsigned& op, int32_t& constant)
{
    if (isOperandConstantImmediateInt(op1)) {
        constant = getConstantOperand(op1).asInt32();
        op = op2;
        return true;
    }

    if (isOperandConstantImmediateInt(op2)) {
        constant = getConstantOperand(op2).asInt32();
        op = op1;
        return true;
    }
    
    return false;
}

/* Deprecated: Please use JITStubCall instead. */

ALWAYS_INLINE void JIT::emitPutJITStubArg(RegisterID tag, RegisterID payload, unsigned argumentNumber)
{
    unsigned argumentStackOffset = (argumentNumber * (sizeof(JSValue) / sizeof(void*))) + JITSTACKFRAME_ARGS_INDEX;
    poke(payload, argumentStackOffset);
    poke(tag, argumentStackOffset + 1);
}

/* Deprecated: Please use JITStubCall instead. */

ALWAYS_INLINE void JIT::emitPutJITStubArgFromVirtualRegister(unsigned src, unsigned argumentNumber, RegisterID scratch1, RegisterID scratch2)
{
    unsigned argumentStackOffset = (argumentNumber * (sizeof(JSValue) / sizeof(void*))) + JITSTACKFRAME_ARGS_INDEX;
    if (m_codeBlock->isConstantRegisterIndex(src)) {
        JSValue constant = m_codeBlock->getConstant(src);
        poke(Imm32(constant.payload()), argumentStackOffset);
        poke(Imm32(constant.tag()), argumentStackOffset + 1);
    } else {
        emitLoad(src, scratch1, scratch2);
        poke(scratch2, argumentStackOffset);
        poke(scratch1, argumentStackOffset + 1);
    }
}

#else // USE(JSVALUE32_64)

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

ALWAYS_INLINE int32_t JIT::getConstantOperandImmediateInt(unsigned src)
{
    return getConstantOperand(src).asInt32();
}

ALWAYS_INLINE bool JIT::isOperandConstantImmediateInt(unsigned src)
{
    return m_codeBlock->isConstantRegisterIndex(src) && getConstantOperand(src).isInt32();
}

ALWAYS_INLINE void JIT::emitPutVirtualRegister(unsigned dst, RegisterID from)
{
    storePtr(from, Address(callFrameRegister, dst * sizeof(Register)));
    m_lastResultBytecodeRegister = (from == cachedResultRegister) ? dst : std::numeric_limits<int>::max();
}

ALWAYS_INLINE void JIT::emitInitRegister(unsigned dst)
{
    storePtr(ImmPtr(JSValue::encode(jsUndefined())), Address(callFrameRegister, dst * sizeof(Register)));
}

ALWAYS_INLINE JIT::Jump JIT::emitJumpIfJSCell(RegisterID reg)
{
#if USE(JSVALUE64)
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
#if USE(JSVALUE64)
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

#if USE(JSVALUE64)
ALWAYS_INLINE JIT::Jump JIT::emitJumpIfImmediateNumber(RegisterID reg)
{
    return branchTestPtr(NonZero, reg, tagTypeNumberRegister);
}
ALWAYS_INLINE JIT::Jump JIT::emitJumpIfNotImmediateNumber(RegisterID reg)
{
    return branchTestPtr(Zero, reg, tagTypeNumberRegister);
}

inline void JIT::emitLoadDouble(unsigned index, FPRegisterID value)
{
    if (m_codeBlock->isConstantRegisterIndex(index)) {
        Register& inConstantPool = m_codeBlock->constantRegister(index);
        loadDouble(&inConstantPool, value);
    } else
        loadDouble(addressFor(index), value);
}

inline void JIT::emitLoadInt32ToDouble(unsigned index, FPRegisterID value)
{
    if (m_codeBlock->isConstantRegisterIndex(index)) {
        Register& inConstantPool = m_codeBlock->constantRegister(index);
        convertInt32ToDouble(AbsoluteAddress(&inConstantPool), value);
    } else
        convertInt32ToDouble(addressFor(index), value);
}
#endif

ALWAYS_INLINE JIT::Jump JIT::emitJumpIfImmediateInteger(RegisterID reg)
{
#if USE(JSVALUE64)
    return branchPtr(AboveOrEqual, reg, tagTypeNumberRegister);
#else
    return branchTest32(NonZero, reg, Imm32(JSImmediate::TagTypeNumber));
#endif
}

ALWAYS_INLINE JIT::Jump JIT::emitJumpIfNotImmediateInteger(RegisterID reg)
{
#if USE(JSVALUE64)
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

ALWAYS_INLINE void JIT::emitJumpSlowCaseIfNotImmediateNumber(RegisterID reg)
{
    addSlowCase(emitJumpIfNotImmediateNumber(reg));
}

#if !USE(JSVALUE64)
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
#if USE(JSVALUE64)
    emitFastArithIntToImmNoCheck(src, dest);
#else
    if (src != dest)
        move(src, dest);
    addPtr(Imm32(JSImmediate::TagTypeNumber), dest);
#endif
}

ALWAYS_INLINE void JIT::emitFastArithImmToInt(RegisterID reg)
{
#if USE(JSVALUE64)
    UNUSED_PARAM(reg);
#else
    rshift32(Imm32(JSImmediate::IntegerPayloadShift), reg);
#endif
}

// operand is int32_t, must have been zero-extended if register is 64-bit.
ALWAYS_INLINE void JIT::emitFastArithIntToImmNoCheck(RegisterID src, RegisterID dest)
{
#if USE(JSVALUE64)
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

/* Deprecated: Please use JITStubCall instead. */

// get arg puts an arg from the SF register array onto the stack, as an arg to a context threaded function.
ALWAYS_INLINE void JIT::emitPutJITStubArgFromVirtualRegister(unsigned src, unsigned argumentNumber, RegisterID scratch)
{
    unsigned argumentStackOffset = (argumentNumber * (sizeof(JSValue) / sizeof(void*))) + JITSTACKFRAME_ARGS_INDEX;
    if (m_codeBlock->isConstantRegisterIndex(src)) {
        JSValue value = m_codeBlock->getConstant(src);
        poke(ImmPtr(JSValue::encode(value)), argumentStackOffset);
    } else {
        loadPtr(Address(callFrameRegister, src * sizeof(Register)), scratch);
        poke(scratch, argumentStackOffset);
    }

    killLastResultRegister();
}

#endif // USE(JSVALUE32_64)

} // namespace JSC

#endif // ENABLE(JIT)

#endif
