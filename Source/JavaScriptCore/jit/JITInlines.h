/*
 * Copyright (C) 2008-2018 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(JIT)

#include "JSCInlines.h"

namespace JSC {

inline MacroAssembler::JumpList JIT::emitDoubleGetByVal(const Instruction* instruction, PatchableJump& badType)
{
#if USE(JSVALUE64)
    JSValueRegs result = JSValueRegs(regT0);
#else
    JSValueRegs result = JSValueRegs(regT1, regT0);
#endif
    JumpList slowCases = emitDoubleLoad(instruction, badType);
    boxDouble(fpRegT0, result);
    return slowCases;
}

ALWAYS_INLINE MacroAssembler::JumpList JIT::emitLoadForArrayMode(const Instruction* currentInstruction, JITArrayMode arrayMode, PatchableJump& badType)
{
    switch (arrayMode) {
    case JITInt32:
        return emitInt32Load(currentInstruction, badType);
    case JITDouble:
        return emitDoubleLoad(currentInstruction, badType);
    case JITContiguous:
        return emitContiguousLoad(currentInstruction, badType);
    case JITArrayStorage:
        return emitArrayStorageLoad(currentInstruction, badType);
    default:
        break;
    }
    RELEASE_ASSERT_NOT_REACHED();
    return MacroAssembler::JumpList();
}

inline MacroAssembler::JumpList JIT::emitContiguousGetByVal(const Instruction* instruction, PatchableJump& badType, IndexingType expectedShape)
{
    return emitContiguousLoad(instruction, badType, expectedShape);
}

inline MacroAssembler::JumpList JIT::emitArrayStorageGetByVal(const Instruction* instruction, PatchableJump& badType)
{
    return emitArrayStorageLoad(instruction, badType);
}

ALWAYS_INLINE bool JIT::isOperandConstantDouble(int src)
{
    return m_codeBlock->isConstantRegisterIndex(src) && getConstantOperand(src).isDouble();
}

ALWAYS_INLINE JSValue JIT::getConstantOperand(int src)
{
    ASSERT(m_codeBlock->isConstantRegisterIndex(src));
    return m_codeBlock->getConstant(src);
}

ALWAYS_INLINE void JIT::emitPutIntToCallFrameHeader(RegisterID from, int entry)
{
#if USE(JSVALUE32_64)
    store32(TrustedImm32(JSValue::Int32Tag), tagFor(entry));
    store32(from, payloadFor(entry));
#else
    store64(from, addressFor(entry));
#endif
}

ALWAYS_INLINE void JIT::emitLoadCharacterString(RegisterID src, RegisterID dst, JumpList& failures)
{
    failures.append(branchIfNotString(src));
    failures.append(branch32(NotEqual, MacroAssembler::Address(src, JSString::offsetOfLength()), TrustedImm32(1)));
    loadPtr(MacroAssembler::Address(src, JSString::offsetOfValue()), dst);
    failures.append(branchTest32(Zero, dst));
    loadPtr(MacroAssembler::Address(dst, StringImpl::flagsOffset()), regT1);
    loadPtr(MacroAssembler::Address(dst, StringImpl::dataOffset()), dst);

    JumpList is16Bit;
    JumpList cont8Bit;
    is16Bit.append(branchTest32(Zero, regT1, TrustedImm32(StringImpl::flagIs8Bit())));
    load8(MacroAssembler::Address(dst, 0), dst);
    cont8Bit.append(jump());
    is16Bit.link(this);
    load16(MacroAssembler::Address(dst, 0), dst);
    cont8Bit.link(this);
}

ALWAYS_INLINE JIT::Call JIT::emitNakedCall(CodePtr<NoPtrTag> target)
{
    ASSERT(m_bytecodeOffset != std::numeric_limits<unsigned>::max()); // This method should only be called during hot/cold path generation, so that m_bytecodeOffset is set.
    Call nakedCall = nearCall();
    m_calls.append(CallRecord(nakedCall, m_bytecodeOffset, FunctionPtr<OperationPtrTag>(target.retagged<OperationPtrTag>())));
    return nakedCall;
}

ALWAYS_INLINE JIT::Call JIT::emitNakedTailCall(CodePtr<NoPtrTag> target)
{
    ASSERT(m_bytecodeOffset != std::numeric_limits<unsigned>::max()); // This method should only be called during hot/cold path generation, so that m_bytecodeOffset is set.
    Call nakedCall = nearTailCall();
    m_calls.append(CallRecord(nakedCall, m_bytecodeOffset, FunctionPtr<OperationPtrTag>(target.retagged<OperationPtrTag>())));
    return nakedCall;
}

ALWAYS_INLINE void JIT::updateTopCallFrame()
{
    ASSERT(static_cast<int>(m_bytecodeOffset) >= 0);
#if USE(JSVALUE32_64)
    Instruction* instruction = &m_codeBlock->instructions()[m_bytecodeOffset]; 
    uint32_t locationBits = CallSiteIndex(instruction).bits();
#else
    uint32_t locationBits = CallSiteIndex(m_bytecodeOffset).bits();
#endif
    store32(TrustedImm32(locationBits), tagFor(CallFrameSlot::argumentCount));
    
    // FIXME: It's not clear that this is needed. JITOperations tend to update the top call frame on
    // the C++ side.
    // https://bugs.webkit.org/show_bug.cgi?id=155693
    storePtr(callFrameRegister, &m_vm->topCallFrame);
}

ALWAYS_INLINE MacroAssembler::Call JIT::appendCallWithExceptionCheck(const FunctionPtr<CFunctionPtrTag> function)
{
    updateTopCallFrame();
    MacroAssembler::Call call = appendCall(function);
    exceptionCheck();
    return call;
}

#if OS(WINDOWS) && CPU(X86_64)
ALWAYS_INLINE MacroAssembler::Call JIT::appendCallWithExceptionCheckAndSlowPathReturnType(const FunctionPtr<CFunctionPtrTag> function)
{
    updateTopCallFrame();
    MacroAssembler::Call call = appendCallWithSlowPathReturnType(function);
    exceptionCheck();
    return call;
}
#endif

ALWAYS_INLINE MacroAssembler::Call JIT::appendCallWithCallFrameRollbackOnException(const FunctionPtr<CFunctionPtrTag> function)
{
    updateTopCallFrame(); // The callee is responsible for setting topCallFrame to their caller
    MacroAssembler::Call call = appendCall(function);
    exceptionCheckWithCallFrameRollback();
    return call;
}

ALWAYS_INLINE MacroAssembler::Call JIT::appendCallWithExceptionCheckSetJSValueResult(const FunctionPtr<CFunctionPtrTag> function, int dst)
{
    MacroAssembler::Call call = appendCallWithExceptionCheck(function);
#if USE(JSVALUE64)
    emitPutVirtualRegister(dst, returnValueGPR);
#else
    emitStore(dst, returnValueGPR2, returnValueGPR);
#endif
    return call;
}

template<typename Metadata>
ALWAYS_INLINE MacroAssembler::Call JIT::appendCallWithExceptionCheckSetJSValueResultWithProfile(Metadata& metadata, const FunctionPtr<CFunctionPtrTag> function, int dst)
{
    MacroAssembler::Call call = appendCallWithExceptionCheck(function);
    emitValueProfilingSite(metadata);
#if USE(JSVALUE64)
    emitPutVirtualRegister(dst, returnValueGPR);
#else
    emitStore(dst, returnValueGPR2, returnValueGPR);
#endif
    return call;
}

ALWAYS_INLINE void JIT::linkSlowCaseIfNotJSCell(Vector<SlowCaseEntry>::iterator& iter, int vReg)
{
    if (!m_codeBlock->isKnownNotImmediate(vReg))
        linkSlowCase(iter);
}

ALWAYS_INLINE void JIT::linkAllSlowCasesForBytecodeOffset(Vector<SlowCaseEntry>& slowCases, Vector<SlowCaseEntry>::iterator& iter, unsigned bytecodeOffset)
{
    while (iter != slowCases.end() && iter->to == bytecodeOffset)
        linkSlowCase(iter);
}

ALWAYS_INLINE void JIT::addSlowCase(Jump jump)
{
    ASSERT(m_bytecodeOffset != std::numeric_limits<unsigned>::max()); // This method should only be called during hot/cold path generation, so that m_bytecodeOffset is set.

    m_slowCases.append(SlowCaseEntry(jump, m_bytecodeOffset));
}

ALWAYS_INLINE void JIT::addSlowCase(const JumpList& jumpList)
{
    ASSERT(m_bytecodeOffset != std::numeric_limits<unsigned>::max()); // This method should only be called during hot/cold path generation, so that m_bytecodeOffset is set.

    for (const Jump& jump : jumpList.jumps())
        m_slowCases.append(SlowCaseEntry(jump, m_bytecodeOffset));
}

ALWAYS_INLINE void JIT::addSlowCase()
{
    ASSERT(m_bytecodeOffset != std::numeric_limits<unsigned>::max()); // This method should only be called during hot/cold path generation, so that m_bytecodeOffset is set.
    
    Jump emptyJump; // Doing it this way to make Windows happy.
    m_slowCases.append(SlowCaseEntry(emptyJump, m_bytecodeOffset));
}

ALWAYS_INLINE void JIT::addJump(Jump jump, int relativeOffset)
{
    ASSERT(m_bytecodeOffset != std::numeric_limits<unsigned>::max()); // This method should only be called during hot/cold path generation, so that m_bytecodeOffset is set.

    m_jmpTable.append(JumpTable(jump, m_bytecodeOffset + relativeOffset));
}

ALWAYS_INLINE void JIT::addJump(const JumpList& jumpList, int relativeOffset)
{
    ASSERT(m_bytecodeOffset != std::numeric_limits<unsigned>::max()); // This method should only be called during hot/cold path generation, so that m_bytecodeOffset is set.

    for (auto& jump : jumpList.jumps())
        addJump(jump, relativeOffset);
}

ALWAYS_INLINE void JIT::emitJumpSlowToHot(Jump jump, int relativeOffset)
{
    ASSERT(m_bytecodeOffset != std::numeric_limits<unsigned>::max()); // This method should only be called during hot/cold path generation, so that m_bytecodeOffset is set.

    jump.linkTo(m_labels[m_bytecodeOffset + relativeOffset], this);
}

#if ENABLE(SAMPLING_FLAGS)
ALWAYS_INLINE void JIT::setSamplingFlag(int32_t flag)
{
    ASSERT(flag >= 1);
    ASSERT(flag <= 32);
    or32(TrustedImm32(1u << (flag - 1)), AbsoluteAddress(SamplingFlags::addressOfFlags()));
}

ALWAYS_INLINE void JIT::clearSamplingFlag(int32_t flag)
{
    ASSERT(flag >= 1);
    ASSERT(flag <= 32);
    and32(TrustedImm32(~(1u << (flag - 1))), AbsoluteAddress(SamplingFlags::addressOfFlags()));
}
#endif

#if ENABLE(SAMPLING_COUNTERS)
ALWAYS_INLINE void JIT::emitCount(AbstractSamplingCounter& counter, int32_t count)
{
    add64(TrustedImm32(count), AbsoluteAddress(counter.addressOfCounter()));
}
#endif

#if ENABLE(OPCODE_SAMPLING)
#if CPU(X86_64)
ALWAYS_INLINE void JIT::sampleInstruction(const Instruction* instruction, bool inHostFunction)
{
    move(TrustedImmPtr(m_interpreter->sampler()->sampleSlot()), X86Registers::ecx);
    storePtr(TrustedImmPtr(m_interpreter->sampler()->encodeSample(instruction, inHostFunction)), X86Registers::ecx);
}
#else
ALWAYS_INLINE void JIT::sampleInstruction(const Instruction* instruction, bool inHostFunction)
{
    storePtr(TrustedImmPtr(m_interpreter->sampler()->encodeSample(instruction, inHostFunction)), m_interpreter->sampler()->sampleSlot());
}
#endif
#endif

#if ENABLE(CODEBLOCK_SAMPLING)
#if CPU(X86_64)
ALWAYS_INLINE void JIT::sampleCodeBlock(CodeBlock* codeBlock)
{
    move(TrustedImmPtr(m_interpreter->sampler()->codeBlockSlot()), X86Registers::ecx);
    storePtr(TrustedImmPtr(codeBlock), X86Registers::ecx);
}
#else
ALWAYS_INLINE void JIT::sampleCodeBlock(CodeBlock* codeBlock)
{
    storePtr(TrustedImmPtr(codeBlock), m_interpreter->sampler()->codeBlockSlot());
}
#endif
#endif

ALWAYS_INLINE bool JIT::isOperandConstantChar(int src)
{
    return m_codeBlock->isConstantRegisterIndex(src) && getConstantOperand(src).isString() && asString(getConstantOperand(src).asCell())->length() == 1;
}

inline void JIT::emitValueProfilingSite(ValueProfile& valueProfile)
{
    ASSERT(shouldEmitProfiling());

    const RegisterID value = regT0;
#if USE(JSVALUE32_64)
    const RegisterID valueTag = regT1;
#endif
    
    // We're in a simple configuration: only one bucket, so we can just do a direct
    // store.
#if USE(JSVALUE64)
    store64(value, valueProfile.m_buckets);
#else
    EncodedValueDescriptor* descriptor = bitwise_cast<EncodedValueDescriptor*>(valueProfile.m_buckets);
    store32(value, &descriptor->asBits.payload);
    store32(valueTag, &descriptor->asBits.tag);
#endif
}

template<typename Op>
inline std::enable_if_t<std::is_same<decltype(Op::Metadata::profile), ValueProfile>::value, void> JIT::emitValueProfilingSiteIfProfiledOpcode(Op bytecode)
{
    emitValueProfilingSite(bytecode.metadata(m_codeBlock));
}

inline void JIT::emitValueProfilingSiteIfProfiledOpcode(...) { }

template<typename Metadata>
inline void JIT::emitValueProfilingSite(Metadata& metadata)
{
    if (!shouldEmitProfiling())
        return;
    emitValueProfilingSite(metadata.profile);
}

inline void JIT::emitArrayProfilingSiteWithCell(RegisterID cell, RegisterID indexingType, ArrayProfile* arrayProfile)
{
    if (shouldEmitProfiling()) {
        load32(MacroAssembler::Address(cell, JSCell::structureIDOffset()), indexingType);
        store32(indexingType, arrayProfile->addressOfLastSeenStructureID());
    }

    load8(Address(cell, JSCell::indexingTypeAndMiscOffset()), indexingType);
}

inline void JIT::emitArrayProfilingSiteForBytecodeIndexWithCell(RegisterID cell, RegisterID indexingType, unsigned bytecodeIndex)
{
    emitArrayProfilingSiteWithCell(cell, indexingType, m_codeBlock->getOrAddArrayProfile(bytecodeIndex));
}

inline void JIT::emitArrayProfileStoreToHoleSpecialCase(ArrayProfile* arrayProfile)
{
    store8(TrustedImm32(1), arrayProfile->addressOfMayStoreToHole());
}

inline void JIT::emitArrayProfileOutOfBoundsSpecialCase(ArrayProfile* arrayProfile)
{
    store8(TrustedImm32(1), arrayProfile->addressOfOutOfBounds());
}

static inline bool arrayProfileSaw(ArrayModes arrayModes, IndexingType capability)
{
    return arrayModesInclude(arrayModes, capability);
}

inline JITArrayMode JIT::chooseArrayMode(ArrayProfile* profile)
{
    ConcurrentJSLocker locker(m_codeBlock->m_lock);
    profile->computeUpdatedPrediction(locker, m_codeBlock);
    ArrayModes arrayModes = profile->observedArrayModes(locker);
    if (arrayProfileSaw(arrayModes, DoubleShape))
        return JITDouble;
    if (arrayProfileSaw(arrayModes, Int32Shape))
        return JITInt32;
    if (arrayProfileSaw(arrayModes, ArrayStorageShape))
        return JITArrayStorage;
    return JITContiguous;
}

ALWAYS_INLINE int32_t JIT::getOperandConstantInt(int src)
{
    return getConstantOperand(src).asInt32();
}

ALWAYS_INLINE double JIT::getOperandConstantDouble(int src)
{
    return getConstantOperand(src).asDouble();
}

ALWAYS_INLINE void JIT::emitInitRegister(int dst)
{
    storeTrustedValue(jsUndefined(), addressFor(dst));
}

#if USE(JSVALUE32_64)

inline void JIT::emitLoadTag(int index, RegisterID tag)
{
    if (m_codeBlock->isConstantRegisterIndex(index)) {
        move(Imm32(getConstantOperand(index).tag()), tag);
        return;
    }

    load32(tagFor(index), tag);
}

inline void JIT::emitLoadPayload(int index, RegisterID payload)
{
    if (m_codeBlock->isConstantRegisterIndex(index)) {
        move(Imm32(getConstantOperand(index).payload()), payload);
        return;
    }

    load32(payloadFor(index), payload);
}

inline void JIT::emitLoad(const JSValue& v, RegisterID tag, RegisterID payload)
{
    move(Imm32(v.payload()), payload);
    move(Imm32(v.tag()), tag);
}

ALWAYS_INLINE void JIT::emitGetVirtualRegister(int src, JSValueRegs dst)
{
    emitLoad(src, dst.tagGPR(), dst.payloadGPR());
}

ALWAYS_INLINE void JIT::emitPutVirtualRegister(int dst, JSValueRegs from)
{
    emitStore(dst, from.tagGPR(), from.payloadGPR());
}

inline void JIT::emitLoad(int index, RegisterID tag, RegisterID payload, RegisterID base)
{
    RELEASE_ASSERT(tag != payload);

    if (base == callFrameRegister) {
        RELEASE_ASSERT(payload != base);
        emitLoadPayload(index, payload);
        emitLoadTag(index, tag);
        return;
    }

    VirtualRegister target { index };
    if (payload == base) { // avoid stomping base
        load32(tagFor(target, base), tag);
        load32(payloadFor(target, base), payload);
        return;
    }

    load32(payloadFor(target, base), payload);
    load32(tagFor(target, base), tag);
}

inline void JIT::emitLoad2(int index1, RegisterID tag1, RegisterID payload1, int index2, RegisterID tag2, RegisterID payload2)
{
    emitLoad(index2, tag2, payload2);
    emitLoad(index1, tag1, payload1);
}

inline void JIT::emitLoadDouble(int index, FPRegisterID value)
{
    if (m_codeBlock->isConstantRegisterIndex(index)) {
        WriteBarrier<Unknown>& inConstantPool = m_codeBlock->constantRegister(index);
        loadDouble(TrustedImmPtr(&inConstantPool), value);
    } else
        loadDouble(addressFor(index), value);
}

inline void JIT::emitLoadInt32ToDouble(int index, FPRegisterID value)
{
    if (m_codeBlock->isConstantRegisterIndex(index)) {
        WriteBarrier<Unknown>& inConstantPool = m_codeBlock->constantRegister(index);
        char* bytePointer = reinterpret_cast<char*>(&inConstantPool);
        convertInt32ToDouble(AbsoluteAddress(bytePointer + OBJECT_OFFSETOF(JSValue, u.asBits.payload)), value);
    } else
        convertInt32ToDouble(payloadFor(index), value);
}

inline void JIT::emitStore(int index, RegisterID tag, RegisterID payload, RegisterID base)
{
    VirtualRegister target { index };
    store32(payload, payloadFor(target, base));
    store32(tag, tagFor(target, base));
}

inline void JIT::emitStoreInt32(int index, RegisterID payload, bool indexIsInt32)
{
    store32(payload, payloadFor(index));
    if (!indexIsInt32)
        store32(TrustedImm32(JSValue::Int32Tag), tagFor(index));
}

inline void JIT::emitStoreInt32(int index, TrustedImm32 payload, bool indexIsInt32)
{
    store32(payload, payloadFor(index));
    if (!indexIsInt32)
        store32(TrustedImm32(JSValue::Int32Tag), tagFor(index));
}

inline void JIT::emitStoreCell(int index, RegisterID payload, bool indexIsCell)
{
    store32(payload, payloadFor(index));
    if (!indexIsCell)
        store32(TrustedImm32(JSValue::CellTag), tagFor(index));
}

inline void JIT::emitStoreBool(int index, RegisterID payload, bool indexIsBool)
{
    store32(payload, payloadFor(index));
    if (!indexIsBool)
        store32(TrustedImm32(JSValue::BooleanTag), tagFor(index));
}

inline void JIT::emitStoreDouble(int index, FPRegisterID value)
{
    storeDouble(value, addressFor(index));
}

inline void JIT::emitStore(int index, const JSValue constant, RegisterID base)
{
    VirtualRegister target { index };
    store32(Imm32(constant.payload()), payloadFor(target, base));
    store32(Imm32(constant.tag()), tagFor(target, base));
}

inline void JIT::emitJumpSlowCaseIfNotJSCell(int virtualRegisterIndex)
{
    if (!m_codeBlock->isKnownNotImmediate(virtualRegisterIndex)) {
        if (m_codeBlock->isConstantRegisterIndex(virtualRegisterIndex))
            addSlowCase(jump());
        else
            addSlowCase(emitJumpIfNotJSCell(virtualRegisterIndex));
    }
}

inline void JIT::emitJumpSlowCaseIfNotJSCell(int virtualRegisterIndex, RegisterID tag)
{
    if (!m_codeBlock->isKnownNotImmediate(virtualRegisterIndex)) {
        if (m_codeBlock->isConstantRegisterIndex(virtualRegisterIndex))
            addSlowCase(jump());
        else
            addSlowCase(branchIfNotCell(tag));
    }
}

ALWAYS_INLINE bool JIT::isOperandConstantInt(int src)
{
    return m_codeBlock->isConstantRegisterIndex(src) && getConstantOperand(src).isInt32();
}

ALWAYS_INLINE bool JIT::getOperandConstantInt(int op1, int op2, int& op, int32_t& constant)
{
    if (isOperandConstantInt(op1)) {
        constant = getConstantOperand(op1).asInt32();
        op = op2;
        return true;
    }

    if (isOperandConstantInt(op2)) {
        constant = getConstantOperand(op2).asInt32();
        op = op1;
        return true;
    }
    
    return false;
}

#else // USE(JSVALUE32_64)

// get arg puts an arg from the SF register array into a h/w register
ALWAYS_INLINE void JIT::emitGetVirtualRegister(int src, RegisterID dst)
{
    ASSERT(m_bytecodeOffset != std::numeric_limits<unsigned>::max()); // This method should only be called during hot/cold path generation, so that m_bytecodeOffset is set.

    if (m_codeBlock->isConstantRegisterIndex(src)) {
        JSValue value = m_codeBlock->getConstant(src);
        if (!value.isNumber())
            move(TrustedImm64(JSValue::encode(value)), dst);
        else
            move(Imm64(JSValue::encode(value)), dst);
        return;
    }

    load64(addressFor(src), dst);
}

ALWAYS_INLINE void JIT::emitGetVirtualRegister(int src, JSValueRegs dst)
{
    emitGetVirtualRegister(src, dst.payloadGPR());
}

ALWAYS_INLINE void JIT::emitGetVirtualRegister(VirtualRegister src, RegisterID dst)
{
    emitGetVirtualRegister(src.offset(), dst);
}

ALWAYS_INLINE void JIT::emitGetVirtualRegisters(int src1, RegisterID dst1, int src2, RegisterID dst2)
{
    emitGetVirtualRegister(src1, dst1);
    emitGetVirtualRegister(src2, dst2);
}

ALWAYS_INLINE void JIT::emitGetVirtualRegisters(VirtualRegister src1, RegisterID dst1, VirtualRegister src2, RegisterID dst2)
{
    emitGetVirtualRegisters(src1.offset(), dst1, src2.offset(), dst2);
}

ALWAYS_INLINE bool JIT::isOperandConstantInt(int src)
{
    return m_codeBlock->isConstantRegisterIndex(src) && getConstantOperand(src).isInt32();
}

ALWAYS_INLINE void JIT::emitPutVirtualRegister(int dst, RegisterID from)
{
    store64(from, addressFor(dst));
}

ALWAYS_INLINE void JIT::emitPutVirtualRegister(int dst, JSValueRegs from)
{
    emitPutVirtualRegister(dst, from.payloadGPR());
}

ALWAYS_INLINE void JIT::emitPutVirtualRegister(VirtualRegister dst, RegisterID from)
{
    emitPutVirtualRegister(dst.offset(), from);
}

ALWAYS_INLINE JIT::Jump JIT::emitJumpIfBothJSCells(RegisterID reg1, RegisterID reg2, RegisterID scratch)
{
    move(reg1, scratch);
    or64(reg2, scratch);
    return branchIfCell(scratch);
}

ALWAYS_INLINE void JIT::emitJumpSlowCaseIfJSCell(RegisterID reg)
{
    addSlowCase(branchIfCell(reg));
}

ALWAYS_INLINE void JIT::emitJumpSlowCaseIfNotJSCell(RegisterID reg)
{
    addSlowCase(branchIfNotCell(reg));
}

ALWAYS_INLINE void JIT::emitJumpSlowCaseIfNotJSCell(RegisterID reg, int vReg)
{
    if (!m_codeBlock->isKnownNotImmediate(vReg))
        emitJumpSlowCaseIfNotJSCell(reg);
}

inline void JIT::emitLoadDouble(int index, FPRegisterID value)
{
    if (m_codeBlock->isConstantRegisterIndex(index)) {
        WriteBarrier<Unknown>& inConstantPool = m_codeBlock->constantRegister(index);
        loadDouble(TrustedImmPtr(&inConstantPool), value);
    } else
        loadDouble(addressFor(index), value);
}

inline void JIT::emitLoadInt32ToDouble(int index, FPRegisterID value)
{
    if (m_codeBlock->isConstantRegisterIndex(index)) {
        ASSERT(isOperandConstantInt(index));
        convertInt32ToDouble(Imm32(getConstantOperand(index).asInt32()), value);
    } else
        convertInt32ToDouble(addressFor(index), value);
}

ALWAYS_INLINE JIT::PatchableJump JIT::emitPatchableJumpIfNotInt(RegisterID reg)
{
    return patchableBranch64(Below, reg, tagTypeNumberRegister);
}

ALWAYS_INLINE JIT::Jump JIT::emitJumpIfNotInt(RegisterID reg1, RegisterID reg2, RegisterID scratch)
{
    move(reg1, scratch);
    and64(reg2, scratch);
    return branchIfNotInt32(scratch);
}

ALWAYS_INLINE void JIT::emitJumpSlowCaseIfNotInt(RegisterID reg)
{
    addSlowCase(branchIfNotInt32(reg));
}

ALWAYS_INLINE void JIT::emitJumpSlowCaseIfNotInt(RegisterID reg1, RegisterID reg2, RegisterID scratch)
{
    addSlowCase(emitJumpIfNotInt(reg1, reg2, scratch));
}

ALWAYS_INLINE void JIT::emitJumpSlowCaseIfNotNumber(RegisterID reg)
{
    addSlowCase(branchIfNotNumber(reg));
}

ALWAYS_INLINE int JIT::jumpTarget(const Instruction* instruction, int target)
{
    if (target)
        return target;
    return m_codeBlock->outOfLineJumpOffset(instruction);
}

ALWAYS_INLINE GetPutInfo JIT::copiedGetPutInfo(OpPutToScope bytecode)
{
    unsigned key = bytecode.metadataID + 1; // HashMap doesn't like 0 as a key
    auto iterator = m_copiedGetPutInfos.find(key);
    if (iterator != m_copiedGetPutInfos.end())
        return GetPutInfo(iterator->value);
    GetPutInfo getPutInfo = bytecode.metadata(m_codeBlock).getPutInfo;
    m_copiedGetPutInfos.add(key, getPutInfo.operand());
    return getPutInfo;
}

template<typename BinaryOp>
ALWAYS_INLINE ArithProfile JIT::copiedArithProfile(BinaryOp bytecode)
{
    uint64_t key = static_cast<uint64_t>(BinaryOp::opcodeID) << 32 | static_cast<uint64_t>(bytecode.metadataID);
    auto iterator = m_copiedArithProfiles.find(key);
    if (iterator != m_copiedArithProfiles.end())
        return iterator->value;
    ArithProfile arithProfile = bytecode.metadata(m_codeBlock).arithProfile;
    m_copiedArithProfiles.add(key, arithProfile);
    return arithProfile;
}

#endif // USE(JSVALUE32_64)

} // namespace JSC

#endif // ENABLE(JIT)
