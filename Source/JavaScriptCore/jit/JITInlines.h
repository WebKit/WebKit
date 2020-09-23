/*
 * Copyright (C) 2008-2019 Apple Inc. All rights reserved.
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
#include "BytecodeOperandsForCheckpoint.h"
#include "CommonSlowPathsInlines.h"
#include "JIT.h"
#include "JSCInlines.h"

namespace JSC {

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

ALWAYS_INLINE bool JIT::isOperandConstantDouble(VirtualRegister src)
{
    return src.isConstant() && getConstantOperand(src).isDouble();
}

ALWAYS_INLINE JSValue JIT::getConstantOperand(VirtualRegister src)
{
    ASSERT(src.isConstant());
    return m_codeBlock->getConstant(src);
}

ALWAYS_INLINE void JIT::emitPutIntToCallFrameHeader(RegisterID from, VirtualRegister entry)
{
    ASSERT(entry.isHeader());
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
    loadPtr(MacroAssembler::Address(src, JSString::offsetOfValue()), dst);
    failures.append(branchIfRopeStringImpl(dst));
    failures.append(branch32(NotEqual, MacroAssembler::Address(dst, StringImpl::lengthMemoryOffset()), TrustedImm32(1)));
    loadPtr(MacroAssembler::Address(dst, StringImpl::dataOffset()), regT1);

    auto is16Bit = branchTest32(Zero, Address(dst, StringImpl::flagsOffset()), TrustedImm32(StringImpl::flagIs8Bit()));
    load8(MacroAssembler::Address(regT1, 0), dst);
    auto done = jump();
    is16Bit.link(this);
    load16(MacroAssembler::Address(regT1, 0), dst);
    done.link(this);
}

ALWAYS_INLINE JIT::Call JIT::emitNakedCall(CodePtr<NoPtrTag> target)
{
    ASSERT(m_bytecodeIndex); // This method should only be called during hot/cold path generation, so that m_bytecodeIndex is set.
    Call nakedCall = nearCall();
    m_calls.append(CallRecord(nakedCall, m_bytecodeIndex, FunctionPtr<OperationPtrTag>(target.retagged<OperationPtrTag>())));
    return nakedCall;
}

ALWAYS_INLINE JIT::Call JIT::emitNakedTailCall(CodePtr<NoPtrTag> target)
{
    ASSERT(m_bytecodeIndex); // This method should only be called during hot/cold path generation, so that m_bytecodeIndex is set.
    Call nakedCall = nearTailCall();
    m_calls.append(CallRecord(nakedCall, m_bytecodeIndex, FunctionPtr<OperationPtrTag>(target.retagged<OperationPtrTag>())));
    return nakedCall;
}

ALWAYS_INLINE void JIT::updateTopCallFrame()
{
    uint32_t locationBits = CallSiteIndex(m_bytecodeIndex.offset()).bits();
    store32(TrustedImm32(locationBits), tagFor(CallFrameSlot::argumentCountIncludingThis));
    
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

ALWAYS_INLINE MacroAssembler::Call JIT::appendCallWithExceptionCheckSetJSValueResult(const FunctionPtr<CFunctionPtrTag> function, VirtualRegister dst)
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
ALWAYS_INLINE MacroAssembler::Call JIT::appendCallWithExceptionCheckSetJSValueResultWithProfile(Metadata& metadata, const FunctionPtr<CFunctionPtrTag> function, VirtualRegister dst)
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

ALWAYS_INLINE void JIT::linkSlowCaseIfNotJSCell(Vector<SlowCaseEntry>::iterator& iter, VirtualRegister reg)
{
    if (!m_codeBlock->isKnownCell(reg))
        linkSlowCase(iter);
}

ALWAYS_INLINE void JIT::linkAllSlowCasesForBytecodeIndex(Vector<SlowCaseEntry>& slowCases, Vector<SlowCaseEntry>::iterator& iter, BytecodeIndex bytecodeIndex)
{
    while (iter != slowCases.end() && iter->to == bytecodeIndex)
        linkSlowCase(iter);
}

ALWAYS_INLINE bool JIT::hasAnySlowCases(Vector<SlowCaseEntry>& slowCases, Vector<SlowCaseEntry>::iterator& iter, BytecodeIndex bytecodeIndex)
{
    if (iter != slowCases.end() && iter->to == bytecodeIndex)
        return true;
    return false;
}

inline void JIT::advanceToNextCheckpoint()
{
    ASSERT_WITH_MESSAGE(m_bytecodeIndex, "This method should only be called during hot/cold path generation, so that m_bytecodeIndex is set");
    ASSERT(m_codeBlock->instructionAt(m_bytecodeIndex)->hasCheckpoints());
    m_bytecodeIndex = BytecodeIndex(m_bytecodeIndex.offset(), m_bytecodeIndex.checkpoint() + 1);

    auto result = m_checkpointLabels.add(m_bytecodeIndex, label());
    ASSERT_UNUSED(result, result.isNewEntry);
}

inline void JIT::emitJumpSlowToHotForCheckpoint(Jump jump)
{
    ASSERT_WITH_MESSAGE(m_bytecodeIndex, "This method should only be called during hot/cold path generation, so that m_bytecodeIndex is set");
    ASSERT(m_codeBlock->instructionAt(m_bytecodeIndex)->hasCheckpoints());
    m_bytecodeIndex = BytecodeIndex(m_bytecodeIndex.offset(), m_bytecodeIndex.checkpoint() + 1);

    auto iter = m_checkpointLabels.find(m_bytecodeIndex);
    ASSERT(iter != m_checkpointLabels.end());
    jump.linkTo(iter->value, this);
}

ALWAYS_INLINE void JIT::addSlowCase(Jump jump)
{
    ASSERT(m_bytecodeIndex); // This method should only be called during hot/cold path generation, so that m_bytecodeIndex is set.

    m_slowCases.append(SlowCaseEntry(jump, m_bytecodeIndex));
}

ALWAYS_INLINE void JIT::addSlowCase(const JumpList& jumpList)
{
    ASSERT(m_bytecodeIndex); // This method should only be called during hot/cold path generation, so that m_bytecodeIndex is set.

    for (const Jump& jump : jumpList.jumps())
        m_slowCases.append(SlowCaseEntry(jump, m_bytecodeIndex));
}

ALWAYS_INLINE void JIT::addSlowCase()
{
    ASSERT(m_bytecodeIndex); // This method should only be called during hot/cold path generation, so that m_bytecodeIndex is set.
    
    Jump emptyJump; // Doing it this way to make Windows happy.
    m_slowCases.append(SlowCaseEntry(emptyJump, m_bytecodeIndex));
}

ALWAYS_INLINE void JIT::addJump(Jump jump, int relativeOffset)
{
    ASSERT(m_bytecodeIndex); // This method should only be called during hot/cold path generation, so that m_bytecodeIndex is set.

    m_jmpTable.append(JumpTable(jump, m_bytecodeIndex.offset() + relativeOffset));
}

ALWAYS_INLINE void JIT::addJump(const JumpList& jumpList, int relativeOffset)
{
    ASSERT(m_bytecodeIndex); // This method should only be called during hot/cold path generation, so that m_bytecodeIndex is set.

    for (auto& jump : jumpList.jumps())
        addJump(jump, relativeOffset);
}

ALWAYS_INLINE void JIT::emitJumpSlowToHot(Jump jump, int relativeOffset)
{
    ASSERT(m_bytecodeIndex); // This method should only be called during hot/cold path generation, so that m_bytecodeIndex is set.

    jump.linkTo(m_labels[m_bytecodeIndex.offset() + relativeOffset], this);
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

ALWAYS_INLINE bool JIT::isOperandConstantChar(VirtualRegister src)
{
    return src.isConstant() && getConstantOperand(src).isString() && asString(getConstantOperand(src).asCell())->length() == 1;
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
inline std::enable_if_t<std::is_same<decltype(Op::Metadata::m_profile), ValueProfile>::value, void> JIT::emitValueProfilingSiteIfProfiledOpcode(Op bytecode)
{
    emitValueProfilingSite(bytecode.metadata(m_codeBlock));
}

inline void JIT::emitValueProfilingSiteIfProfiledOpcode(...) { }

template<typename Metadata>
inline void JIT::emitValueProfilingSite(Metadata& metadata)
{
    if (!shouldEmitProfiling())
        return;
    emitValueProfilingSite(valueProfileFor(metadata, m_bytecodeIndex.checkpoint()));
}

inline void JIT::emitArrayProfilingSiteWithCell(RegisterID cell, RegisterID indexingType, ArrayProfile* arrayProfile)
{
    if (shouldEmitProfiling()) {
        load32(MacroAssembler::Address(cell, JSCell::structureIDOffset()), indexingType);
        store32(indexingType, arrayProfile->addressOfLastSeenStructureID());
    }

    load8(Address(cell, JSCell::indexingTypeAndMiscOffset()), indexingType);
}

inline void JIT::emitArrayProfileStoreToHoleSpecialCase(ArrayProfile* arrayProfile)
{
    store8(TrustedImm32(1), arrayProfile->addressOfMayStoreToHole());
}

inline void JIT::emitArrayProfileOutOfBoundsSpecialCase(ArrayProfile* arrayProfile)
{
    store8(TrustedImm32(1), arrayProfile->addressOfOutOfBounds());
}

inline JITArrayMode JIT::chooseArrayMode(ArrayProfile* profile)
{
    auto arrayProfileSaw = [] (ArrayModes arrayModes, IndexingType capability) {
        return arrayModesIncludeIgnoringTypedArrays(arrayModes, capability);
    };

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

ALWAYS_INLINE int32_t JIT::getOperandConstantInt(VirtualRegister src)
{
    return getConstantOperand(src).asInt32();
}

ALWAYS_INLINE double JIT::getOperandConstantDouble(VirtualRegister src)
{
    return getConstantOperand(src).asDouble();
}

ALWAYS_INLINE void JIT::emitInitRegister(VirtualRegister dst)
{
    storeTrustedValue(jsUndefined(), addressFor(dst));
}

#if USE(JSVALUE32_64)

inline void JIT::emitLoadDouble(VirtualRegister reg, FPRegisterID value)
{
    if (reg.isConstant()) {
        WriteBarrier<Unknown>& inConstantPool = m_codeBlock->constantRegister(reg);
        loadDouble(TrustedImmPtr(&inConstantPool), value);
    } else
        loadDouble(addressFor(reg), value);
}

inline void JIT::emitLoadTag(VirtualRegister reg, RegisterID tag)
{
    if (reg.isConstant()) {
        move(Imm32(getConstantOperand(reg).tag()), tag);
        return;
    }

    load32(tagFor(reg), tag);
}

inline void JIT::emitLoadPayload(VirtualRegister reg, RegisterID payload)
{
    if (reg.isConstant()) {
        move(Imm32(getConstantOperand(reg).payload()), payload);
        return;
    }

    load32(payloadFor(reg), payload);
}

inline void JIT::emitLoad(const JSValue& v, RegisterID tag, RegisterID payload)
{
    move(Imm32(v.payload()), payload);
    move(Imm32(v.tag()), tag);
}

ALWAYS_INLINE void JIT::emitGetVirtualRegister(VirtualRegister src, JSValueRegs dst)
{
    emitLoad(src, dst.tagGPR(), dst.payloadGPR());
}

ALWAYS_INLINE void JIT::emitPutVirtualRegister(VirtualRegister dst, JSValueRegs from)
{
    emitStore(dst, from.tagGPR(), from.payloadGPR());
}

inline void JIT::emitLoad(VirtualRegister reg, RegisterID tag, RegisterID payload, RegisterID base)
{
    RELEASE_ASSERT(tag != payload);

    if (base == callFrameRegister) {
        RELEASE_ASSERT(payload != base);
        emitLoadPayload(reg, payload);
        emitLoadTag(reg, tag);
        return;
    }

    if (payload == base) { // avoid stomping base
        load32(tagFor(reg, base), tag);
        load32(payloadFor(reg, base), payload);
        return;
    }

    load32(payloadFor(reg, base), payload);
    load32(tagFor(reg, base), tag);
}

inline void JIT::emitLoad2(VirtualRegister reg1, RegisterID tag1, RegisterID payload1, VirtualRegister reg2, RegisterID tag2, RegisterID payload2)
{
    emitLoad(reg2, tag2, payload2);
    emitLoad(reg1, tag1, payload1);
}

inline void JIT::emitStore(VirtualRegister reg, RegisterID tag, RegisterID payload, RegisterID base)
{
    store32(payload, payloadFor(reg, base));
    store32(tag, tagFor(reg, base));
}

inline void JIT::emitStoreInt32(VirtualRegister reg, RegisterID payload, bool indexIsInt32)
{
    store32(payload, payloadFor(reg));
    if (!indexIsInt32)
        store32(TrustedImm32(JSValue::Int32Tag), tagFor(reg));
}

inline void JIT::emitStoreInt32(VirtualRegister reg, TrustedImm32 payload, bool indexIsInt32)
{
    store32(payload, payloadFor(reg));
    if (!indexIsInt32)
        store32(TrustedImm32(JSValue::Int32Tag), tagFor(reg));
}

inline void JIT::emitStoreCell(VirtualRegister reg, RegisterID payload, bool indexIsCell)
{
    store32(payload, payloadFor(reg));
    if (!indexIsCell)
        store32(TrustedImm32(JSValue::CellTag), tagFor(reg));
}

inline void JIT::emitStoreBool(VirtualRegister reg, RegisterID payload, bool indexIsBool)
{
    store32(payload, payloadFor(reg));
    if (!indexIsBool)
        store32(TrustedImm32(JSValue::BooleanTag), tagFor(reg));
}

inline void JIT::emitStoreDouble(VirtualRegister reg, FPRegisterID value)
{
    storeDouble(value, addressFor(reg));
}

inline void JIT::emitStore(VirtualRegister reg, const JSValue constant, RegisterID base)
{
    store32(Imm32(constant.payload()), payloadFor(reg, base));
    store32(Imm32(constant.tag()), tagFor(reg, base));
}

inline void JIT::emitJumpSlowCaseIfNotJSCell(VirtualRegister reg)
{
    if (!m_codeBlock->isKnownCell(reg)) {
        if (reg.isConstant())
            addSlowCase(jump());
        else
            addSlowCase(emitJumpIfNotJSCell(reg));
    }
}

inline void JIT::emitJumpSlowCaseIfNotJSCell(VirtualRegister reg, RegisterID tag)
{
    if (!m_codeBlock->isKnownCell(reg)) {
        if (reg.isConstant())
            addSlowCase(jump());
        else
            addSlowCase(branchIfNotCell(tag));
    }
}

ALWAYS_INLINE bool JIT::isOperandConstantInt(VirtualRegister src)
{
    return src.isConstant() && getConstantOperand(src).isInt32();
}

ALWAYS_INLINE bool JIT::getOperandConstantInt(VirtualRegister op1, VirtualRegister op2, VirtualRegister& op, int32_t& constant)
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
ALWAYS_INLINE void JIT::emitGetVirtualRegister(VirtualRegister src, RegisterID dst)
{
    ASSERT(m_bytecodeIndex); // This method should only be called during hot/cold path generation, so that m_bytecodeIndex is set.

    if (src.isConstant()) {
        JSValue value = m_codeBlock->getConstant(src);
        if (!value.isNumber())
            move(TrustedImm64(JSValue::encode(value)), dst);
        else
            move(Imm64(JSValue::encode(value)), dst);
        return;
    }

    load64(addressFor(src), dst);
}

ALWAYS_INLINE void JIT::emitGetVirtualRegister(VirtualRegister src, JSValueRegs dst)
{
    emitGetVirtualRegister(src, dst.payloadGPR());
}

ALWAYS_INLINE void JIT::emitGetVirtualRegisters(VirtualRegister src1, RegisterID dst1, VirtualRegister src2, RegisterID dst2)
{
    emitGetVirtualRegister(src1, dst1);
    emitGetVirtualRegister(src2, dst2);
}

ALWAYS_INLINE bool JIT::isOperandConstantInt(VirtualRegister src)
{
    return src.isConstant() && getConstantOperand(src).isInt32();
}

ALWAYS_INLINE void JIT::emitPutVirtualRegister(VirtualRegister dst, RegisterID from)
{
    store64(from, addressFor(dst));
}

ALWAYS_INLINE void JIT::emitPutVirtualRegister(VirtualRegister dst, JSValueRegs from)
{
    emitPutVirtualRegister(dst, from.payloadGPR());
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

ALWAYS_INLINE void JIT::emitJumpSlowCaseIfNotJSCell(RegisterID reg, VirtualRegister vReg)
{
    if (!m_codeBlock->isKnownCell(vReg))
        emitJumpSlowCaseIfNotJSCell(reg);
}

ALWAYS_INLINE JIT::PatchableJump JIT::emitPatchableJumpIfNotInt(RegisterID reg)
{
    return patchableBranch64(Below, reg, numberTagRegister);
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

#endif // USE(JSVALUE32_64)

ALWAYS_INLINE void JIT::emitJumpSlowCaseIfNotJSCell(RegisterID reg)
{
    addSlowCase(branchIfNotCell(reg));
}

ALWAYS_INLINE int JIT::jumpTarget(const Instruction* instruction, int target)
{
    if (target)
        return target;
    return m_codeBlock->outOfLineJumpOffset(instruction);
}

ALWAYS_INLINE GetPutInfo JIT::copiedGetPutInfo(OpPutToScope bytecode)
{
    unsigned key = bytecode.m_metadataID + 1; // HashMap doesn't like 0 as a key
    auto iterator = m_copiedGetPutInfos.find(key);
    if (iterator != m_copiedGetPutInfos.end())
        return GetPutInfo(iterator->value);
    GetPutInfo getPutInfo = bytecode.metadata(m_codeBlock).m_getPutInfo;
    m_copiedGetPutInfos.add(key, getPutInfo.operand());
    return getPutInfo;
}

template<typename BinaryOp>
ALWAYS_INLINE BinaryArithProfile JIT::copiedArithProfile(BinaryOp bytecode)
{
    uint64_t key = (static_cast<uint64_t>(BinaryOp::opcodeID) + 1) << 32 | static_cast<uint64_t>(bytecode.m_metadataID);
    auto iterator = m_copiedArithProfiles.find(key);
    if (iterator != m_copiedArithProfiles.end())
        return iterator->value;
    BinaryArithProfile arithProfile = bytecode.metadata(m_codeBlock).m_arithProfile;
    m_copiedArithProfiles.add(key, arithProfile);
    return arithProfile;
}

template<typename Op>
ALWAYS_INLINE ECMAMode JIT::ecmaMode(Op op)
{
    return op.m_ecmaMode;
}

template<>
ALWAYS_INLINE ECMAMode JIT::ecmaMode<OpPutById>(OpPutById op)
{
    return op.m_flags.ecmaMode();
}

template<>
ALWAYS_INLINE ECMAMode JIT::ecmaMode<OpPutPrivateName>(OpPutPrivateName)
{
    return ECMAMode::strict();
}

} // namespace JSC

#endif // ENABLE(JIT)
