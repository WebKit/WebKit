/*
 * Copyright (C) 2008-2022 Apple Inc. All rights reserved.
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

#if USE(JSVALUE64)
ALWAYS_INLINE bool JIT::isOperandConstantDouble(VirtualRegister src)
{
    if (!src.isConstant())
        return false;
    if (m_unlinkedCodeBlock->constantSourceCodeRepresentation(src) == SourceCodeRepresentation::LinkTimeConstant)
        return false;
    return getConstantOperand(src).isDouble();
}
#endif

ALWAYS_INLINE bool JIT::isOperandConstantInt(VirtualRegister src)
{
    if (!src.isConstant())
        return false;
    if (m_unlinkedCodeBlock->constantSourceCodeRepresentation(src) == SourceCodeRepresentation::LinkTimeConstant)
        return false;
    return getConstantOperand(src).isInt32();
}

ALWAYS_INLINE bool JIT::isKnownCell(VirtualRegister src)
{
    if (!src.isConstant())
        return false;
    if (m_unlinkedCodeBlock->constantSourceCodeRepresentation(src) == SourceCodeRepresentation::LinkTimeConstant) {
        // All link time constants are cells.
        return true;
    }
    return getConstantOperand(src).isCell();
}

ALWAYS_INLINE JSValue JIT::getConstantOperand(VirtualRegister src)
{
    ASSERT(src.isConstant());
    RELEASE_ASSERT(m_unlinkedCodeBlock->constantSourceCodeRepresentation(src) != SourceCodeRepresentation::LinkTimeConstant);
    return m_unlinkedCodeBlock->getConstant(src);
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

ALWAYS_INLINE void JIT::updateTopCallFrame()
{
    uint32_t locationBits = CallSiteIndex(m_bytecodeIndex.offset()).bits();
    store32(TrustedImm32(locationBits), tagFor(CallFrameSlot::argumentCountIncludingThis));
    prepareCallOperation(*m_vm);
}

ALWAYS_INLINE MacroAssembler::Call JIT::appendCallWithExceptionCheck(const CodePtr<CFunctionPtrTag> function)
{
    updateTopCallFrame();
    MacroAssembler::Call call = appendCall(function);
    exceptionCheck();
    return call;
}

ALWAYS_INLINE void JIT::appendCallWithExceptionCheck(Address function)
{
    updateTopCallFrame();
    appendCall(function);
    exceptionCheck();
}

ALWAYS_INLINE MacroAssembler::Call JIT::appendCallWithExceptionCheckSetJSValueResult(const CodePtr<CFunctionPtrTag> function, VirtualRegister dst)
{
    MacroAssembler::Call call = appendCallWithExceptionCheck(function);
    emitPutVirtualRegister(dst, returnValueJSR);
    return call;
}

ALWAYS_INLINE void JIT::appendCallWithExceptionCheckSetJSValueResult(Address function, VirtualRegister dst)
{
    appendCallWithExceptionCheck(function);
    emitPutVirtualRegister(dst, returnValueJSR);
}

template<typename Bytecode>
ALWAYS_INLINE MacroAssembler::Call JIT::appendCallWithExceptionCheckSetJSValueResultWithProfile(const Bytecode& bytecode, const CodePtr<CFunctionPtrTag> function, VirtualRegister dst)
{
    MacroAssembler::Call call = appendCallWithExceptionCheck(function);
    emitValueProfilingSite(bytecode, returnValueJSR);
    emitPutVirtualRegister(dst, returnValueJSR);
    return call;
}

template<typename Bytecode>
ALWAYS_INLINE void JIT::appendCallWithExceptionCheckSetJSValueResultWithProfile(const Bytecode& bytecode, Address function, VirtualRegister dst)
{
    appendCallWithExceptionCheck(function);
    emitValueProfilingSite(bytecode, returnValueJSR);
    emitPutVirtualRegister(dst, returnValueJSR);
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
    ASSERT(m_unlinkedCodeBlock->instructionAt(m_bytecodeIndex)->hasCheckpoints());
    m_bytecodeIndex = BytecodeIndex(m_bytecodeIndex.offset(), m_bytecodeIndex.checkpoint() + 1);

    auto result = m_checkpointLabels.add(m_bytecodeIndex, label());
    ASSERT_UNUSED(result, result.isNewEntry);
}

inline void JIT::emitJumpSlowToHotForCheckpoint(Jump jump)
{
    ASSERT_WITH_MESSAGE(m_bytecodeIndex, "This method should only be called during hot/cold path generation, so that m_bytecodeIndex is set");
    ASSERT(m_unlinkedCodeBlock->instructionAt(m_bytecodeIndex)->hasCheckpoints());
    m_bytecodeIndex = BytecodeIndex(m_bytecodeIndex.offset(), m_bytecodeIndex.checkpoint() + 1);

    auto iter = m_checkpointLabels.find(m_bytecodeIndex);
    ASSERT(iter != m_checkpointLabels.end());
    if (jump.isSet())
        jump.linkTo(iter->value, this);
}

inline void JIT::setFastPathResumePoint()
{
    ASSERT_WITH_MESSAGE(m_bytecodeIndex, "This method should only be called during hot/cold path generation, so that m_bytecodeIndex is set");
    auto result = m_fastPathResumeLabels.add(m_bytecodeIndex, label());
    ASSERT_UNUSED(result, result.isNewEntry);
}

inline MacroAssembler::Label JIT::fastPathResumePoint() const
{
    ASSERT_WITH_MESSAGE(m_bytecodeIndex, "This method should only be called during hot/cold path generation, so that m_bytecodeIndex is set");
    // Location set by setFastPathResumePoint
    auto iter = m_fastPathResumeLabels.find(m_bytecodeIndex);
    if (iter != m_fastPathResumeLabels.end())
        return iter->value;
    // Next instruction in sequence
    const auto* currentInstruction = m_unlinkedCodeBlock->instructions().at(m_bytecodeIndex).ptr();
    return m_labels[m_bytecodeIndex.offset() + currentInstruction->size()];
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

ALWAYS_INLINE bool JIT::isOperandConstantChar(VirtualRegister src)
{
    if (!src.isConstant())
        return false;
    if (m_unlinkedCodeBlock->constantSourceCodeRepresentation(src) == SourceCodeRepresentation::LinkTimeConstant)
        return false;
    return getConstantOperand(src).isString() && asString(getConstantOperand(src).asCell())->length() == 1;
}

template<typename Bytecode>
inline void JIT::emitValueProfilingSite(const Bytecode& bytecode, BytecodeIndex bytecodeIndex, JSValueRegs value)
{
    if (!shouldEmitProfiling())
        return;

    ptrdiff_t offset = -static_cast<ptrdiff_t>(valueProfileOffsetFor<Bytecode>(bytecode, bytecodeIndex.checkpoint())) * sizeof(ValueProfile) + ValueProfile::offsetOfFirstBucket() - sizeof(UnlinkedMetadataTable::LinkingData);
    storeValue(value, Address(s_metadataGPR, offset));
}

template<typename Bytecode>
inline void JIT::emitValueProfilingSite(const Bytecode& bytecode, JSValueRegs value)
{
    emitValueProfilingSite(bytecode, m_bytecodeIndex, value);
}

template <typename Bytecode>
inline void JIT::emitArrayProfilingSiteWithCell(const Bytecode& bytecode, ptrdiff_t offsetOfArrayProfile, RegisterID cellGPR, RegisterID scratchGPR)
{
    if (shouldEmitProfiling()) {
        load32(Address(cellGPR, JSCell::structureIDOffset()), scratchGPR);
        store32ToMetadata(scratchGPR, bytecode, offsetOfArrayProfile);
    }
}

template <typename Bytecode>
inline void JIT::emitArrayProfilingSiteWithCell(const Bytecode& bytecode, RegisterID cellGPR, RegisterID scratchGPR)
{
    emitArrayProfilingSiteWithCell(bytecode, Bytecode::Metadata::offsetOfArrayProfile() + ArrayProfile::offsetOfLastSeenStructureID(), cellGPR, scratchGPR);
}

inline void JIT::emitArrayProfilingSiteWithCellAndProfile(RegisterID cellGPR, RegisterID profileGPR, RegisterID scratchGPR)
{
    if (shouldEmitProfiling()) {
        load32(Address(cellGPR, JSCell::structureIDOffset()), scratchGPR);
        store32(scratchGPR, Address(profileGPR, ArrayProfile::offsetOfLastSeenStructureID()));
    }
}

ALWAYS_INLINE int32_t JIT::getOperandConstantInt(VirtualRegister src)
{
    return getConstantOperand(src).asInt32();
}

#if USE(JSVALUE64)
ALWAYS_INLINE double JIT::getOperandConstantDouble(VirtualRegister src)
{
    return getConstantOperand(src).asDouble();
}
#endif

ALWAYS_INLINE void JIT::emitGetVirtualRegister(VirtualRegister src, JSValueRegs dst)
{
    ASSERT(m_bytecodeIndex); // This method should only be called during hot/cold path generation, so that m_bytecodeIndex is set.
    if (src.isConstant()) {
        if (m_profiledCodeBlock->isConstantOwnedByUnlinkedCodeBlock(src))
            moveValue(m_unlinkedCodeBlock->getConstant(src), dst);
        else
            loadCodeBlockConstant(src, dst);
        return;
    }
    loadValue(addressFor(src), dst);
}

ALWAYS_INLINE void JIT::emitPutVirtualRegister(VirtualRegister dst, JSValueRegs from)
{
    storeValue(from, addressFor(dst));
}

ALWAYS_INLINE void JIT::emitGetVirtualRegisterPayload(VirtualRegister src, RegisterID dst)
{
#if USE(JSVALUE64)
    emitGetVirtualRegister(src, JSValueRegs { dst });
#elif USE(JSVALUE32_64)
    ASSERT(m_bytecodeIndex); // This method should only be called during hot/cold path generation, so that m_bytecodeIndex is set.
    if (src.isConstant()) {
        if (m_profiledCodeBlock->isConstantOwnedByUnlinkedCodeBlock(src))
            move(Imm32(m_unlinkedCodeBlock->getConstant(src).payload()), dst);
        else
            loadCodeBlockConstantPayload(src, dst);
        return;
    }
    load32(payloadFor(src), dst);
#endif
}

#if USE(JSVALUE32_64)
ALWAYS_INLINE void JIT::emitGetVirtualRegisterTag(VirtualRegister src, RegisterID dst)
{
    ASSERT(m_bytecodeIndex); // This method should only be called during hot/cold path generation, so that m_bytecodeIndex is set.
    if (src.isConstant()) {
        if (m_profiledCodeBlock->isConstantOwnedByUnlinkedCodeBlock(src))
            move(Imm32(m_unlinkedCodeBlock->getConstant(src).tag()), dst);
        else
            loadCodeBlockConstantTag(src, dst);
        return;
    }
    load32(tagFor(src), dst);
}
#elif USE(JSVALUE64)
ALWAYS_INLINE void JIT::emitGetVirtualRegister(VirtualRegister src, RegisterID dst)
{
    emitGetVirtualRegister(src, JSValueRegs { dst });
}

ALWAYS_INLINE void JIT::emitPutVirtualRegister(VirtualRegister dst, RegisterID from)
{
    emitPutVirtualRegister(dst, JSValueRegs { from });
}

ALWAYS_INLINE JIT::Jump JIT::emitJumpIfNotInt(RegisterID reg1, RegisterID reg2, RegisterID scratch)
{
    move(reg1, scratch);
    and64(reg2, scratch);
    return branchIfNotInt32(scratch);
}

ALWAYS_INLINE void JIT::emitJumpSlowCaseIfNotInt(RegisterID reg1, RegisterID reg2, RegisterID scratch)
{
    addSlowCase(emitJumpIfNotInt(reg1, reg2, scratch));
}

ALWAYS_INLINE void JIT::emitJumpSlowCaseIfNotInt(RegisterID gpr)
{
    emitJumpSlowCaseIfNotInt(JSValueRegs { gpr });
}
#endif

ALWAYS_INLINE void JIT::emitJumpSlowCaseIfNotInt(JSValueRegs jsr)
{
    addSlowCase(branchIfNotInt32(jsr));
}

ALWAYS_INLINE void JIT::emitJumpSlowCaseIfNotJSCell(JSValueRegs reg)
{
    addSlowCase(branchIfNotCell(reg));
}

ALWAYS_INLINE void JIT::emitJumpSlowCaseIfNotJSCell(JSValueRegs jsReg, VirtualRegister vReg)
{
    if (!isKnownCell(vReg))
        emitJumpSlowCaseIfNotJSCell(jsReg);
}

ALWAYS_INLINE int JIT::jumpTarget(const JSInstruction* instruction, int target)
{
    if (target)
        return target;
    return m_unlinkedCodeBlock->outOfLineJumpOffset(instruction);
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

template <typename Bytecode>
ALWAYS_INLINE void JIT::loadPtrFromMetadata(const Bytecode& bytecode, size_t offset, GPRReg result)
{
    loadPtr(Address(s_metadataGPR, m_profiledCodeBlock->metadataTable()->offsetInMetadataTable(bytecode) + offset), result);
}

template <typename Bytecode>
ALWAYS_INLINE void JIT::load32FromMetadata(const Bytecode& bytecode, size_t offset, GPRReg result)
{
    load32(Address(s_metadataGPR, m_profiledCodeBlock->metadataTable()->offsetInMetadataTable(bytecode) + offset), result);
}

template <typename Bytecode>
ALWAYS_INLINE void JIT::load8FromMetadata(const Bytecode& bytecode, size_t offset, GPRReg result)
{
    load8(Address(s_metadataGPR, m_profiledCodeBlock->metadataTable()->offsetInMetadataTable(bytecode) + offset), result);
}

template <typename ValueType, typename Bytecode>
ALWAYS_INLINE void JIT::store8ToMetadata(ValueType value, const Bytecode& bytecode, size_t offset)
{
    store8(value, Address(s_metadataGPR, m_profiledCodeBlock->metadataTable()->offsetInMetadataTable(bytecode) + offset));
}

template <typename Bytecode>
ALWAYS_INLINE void JIT::store32ToMetadata(GPRReg value, const Bytecode& bytecode, size_t offset)
{
    store32(value, Address(s_metadataGPR, m_profiledCodeBlock->metadataTable()->offsetInMetadataTable(bytecode) + offset));
}

template <typename Bytecode>
ALWAYS_INLINE void JIT::materializePointerIntoMetadata(const Bytecode& bytecode, size_t offset, GPRReg result)
{
    addPtr(TrustedImm32(m_profiledCodeBlock->metadataTable()->offsetInMetadataTable(bytecode) + offset), s_metadataGPR, result);
}

ALWAYS_INLINE void JIT::loadConstant(CCallHelpers& jit, JITConstantPool::Constant constantIndex, GPRReg result)
{
    jit.loadPtr(Address(s_constantsGPR, BaselineJITData::offsetOfTrailingData() + static_cast<uintptr_t>(constantIndex) * sizeof(void*)), result);
}

ALWAYS_INLINE void JIT::loadGlobalObject(CCallHelpers& jit, GPRReg result)
{
    jit.loadPtr(Address(s_constantsGPR, BaselineJITData::offsetOfGlobalObject()), result);
}

ALWAYS_INLINE void JIT::loadConstant(JITConstantPool::Constant constantIndex, GPRReg result)
{
    loadConstant(*this, constantIndex, result);
}

ALWAYS_INLINE void JIT::loadGlobalObject(GPRReg result)
{
    loadGlobalObject(*this, result);
}

ALWAYS_INLINE void JIT::loadStructureStubInfo(CCallHelpers& jit, StructureStubInfoIndex index, GPRReg result)
{
    jit.subPtr(s_constantsGPR, TrustedImm32(static_cast<uintptr_t>(index.m_index + 1) * sizeof(StructureStubInfo)), result);
}

ALWAYS_INLINE void JIT::loadStructureStubInfo(StructureStubInfoIndex index, GPRReg result)
{
    loadStructureStubInfo(*this, index, result);
}

ALWAYS_INLINE static void loadAddrOfCodeBlockConstantBuffer(JIT &jit, GPRReg dst)
{
    jit.loadPtr(jit.addressFor(CallFrameSlot::codeBlock), dst);
    jit.loadPtr(JIT::Address(dst, CodeBlock::offsetOfConstantsVectorBuffer()), dst);
}

ALWAYS_INLINE void JIT::loadCodeBlockConstant(VirtualRegister constant, JSValueRegs dst)
{
    RELEASE_ASSERT(constant.isConstant());
    loadAddrOfCodeBlockConstantBuffer(*this, dst.payloadGPR());
    loadValue(Address(dst.payloadGPR(), constant.toConstantIndex() * sizeof(Register)), dst);
}

ALWAYS_INLINE void JIT::loadCodeBlockConstantPayload(VirtualRegister constant, RegisterID dst)
{
    RELEASE_ASSERT(constant.isConstant());
    loadAddrOfCodeBlockConstantBuffer(*this, dst);
    Address address(dst, constant.toConstantIndex() * sizeof(Register));
#if USE(JSVALUE64)
    load64(address, dst);
#elif USE(JSVALUE32_64)
    load32(address.withOffset(PayloadOffset), dst);
#endif
}

#if USE(JSVALUE32_64)
ALWAYS_INLINE void JIT::loadCodeBlockConstantTag(VirtualRegister constant, RegisterID dst)
{
    RELEASE_ASSERT(constant.isConstant());
    loadAddrOfCodeBlockConstantBuffer(*this, dst);
    load32(Address(dst, constant.toConstantIndex() * sizeof(Register) + TagOffset), dst);
}
#endif

} // namespace JSC

#endif // ENABLE(JIT)
