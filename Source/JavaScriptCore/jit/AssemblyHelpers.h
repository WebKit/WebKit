/*
 * Copyright (C) 2011-2025 Apple Inc. All rights reserved.
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

#include <wtf/Platform.h>

#if ENABLE(JIT)

#include <JavaScriptCore/CodeBlock.h>
#include <JavaScriptCore/EntryFrame.h>
#include <JavaScriptCore/FPRInfo.h>
#include <JavaScriptCore/GPRInfo.h>
#include <JavaScriptCore/Heap.h>
#include <JavaScriptCore/InlineCallFrame.h>
#include <JavaScriptCore/JITAllocator.h>
#include <JavaScriptCore/JITCode.h>
#include <JavaScriptCore/JSBigInt.h>
#include <JavaScriptCore/JSCell.h>
#include <JavaScriptCore/JSString.h>
#include <JavaScriptCore/MacroAssembler.h>
#include <JavaScriptCore/MarkedSpace.h>
#include <JavaScriptCore/RegisterAtOffsetList.h>
#include <JavaScriptCore/RegisterSet.h>
#include <JavaScriptCore/ScratchRegisterAllocator.h>
#include <JavaScriptCore/StackAlignment.h>
#include <JavaScriptCore/TagRegistersMode.h>
#include <JavaScriptCore/TypeofType.h>
#include <JavaScriptCore/VM.h>
#include <wtf/TZoneMalloc.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC {

typedef void (*V_DebugOperation_EPP)(CallFrame*, void*, void*);

class AssemblyHelpers : public MacroAssembler {
    WTF_MAKE_TZONE_NON_HEAP_ALLOCATABLE(AssemblyHelpers);
public:
    AssemblyHelpers(CodeBlock* codeBlock)
        : m_codeBlock(codeBlock)
        , m_baselineCodeBlock(codeBlock ? codeBlock->baselineAlternative() : nullptr)
    {
        if (m_codeBlock) {
            ASSERT(m_baselineCodeBlock);
            ASSERT(!m_baselineCodeBlock->alternative());
            ASSERT(m_baselineCodeBlock->jitType() == JITType::None || JITCode::isBaselineCode(m_baselineCodeBlock->jitType()));
        }
    }

    CodeBlock* codeBlock() { return m_codeBlock; }
    VM& vm() { return m_codeBlock->vm(); }
    AssemblerType_T& assembler() LIFETIME_BOUND { return m_assembler; }

    void prepareCallOperation(VM& vm)
    {
        UNUSED_PARAM(vm);
#if ASSERT_ENABLED
        storePtr(GPRInfo::callFrameRegister, &vm.topCallFrame);
#endif
    }

#if ENABLE(WEBASSEMBLY)
    void NODELETE prepareWasmCallOperation(GPRReg instanceGPR);
#endif

    void checkStackPointerAlignment()
    {
        // This check is both unneeded and harder to write correctly for ARM64
#if !defined(NDEBUG) && !CPU(ARM64)
        Jump stackPointerAligned = branchTestPtr(Zero, stackPointerRegister, TrustedImm32(0xf));
        abortWithReason(AHStackPointerMisaligned);
        stackPointerAligned.link(this);
#endif
    }

    void store64FromReg(Reg src, Address dst)
    {
        if (src.isFPR())
            storeDouble(src.fpr(), dst);
        else
            store64(src.gpr(), dst);
    }

    void store64FromReg(Reg src, BaseIndex dst)
    {
        if (src.isFPR())
            storeDouble(src.fpr(), dst);
        else
            store64(src.gpr(), dst);
    }
    
    void store32FromReg(Reg src, Address dst)
    {
        if (src.isFPR())
            storeFloat(src.fpr(), dst);
        else
            store32(src.gpr(), dst);
    }

    void store32FromReg(Reg src, BaseIndex dst)
    {
        if (src.isFPR())
            storeFloat(src.fpr(), dst);
        else
            store32(src.gpr(), dst);
    }

    void storeReg(Reg src, Address dst)
    {
        store64FromReg(src, dst);
    }

    void load64ToReg(Address src, Reg dst)
    {
        if (dst.isFPR())
            loadDouble(src, dst.fpr());
        else
            load64(src, dst.gpr());
    }
    
    void load32ToReg(Address src, Reg dst)
    {
        if (dst.isFPR())
            loadFloat(src, dst.fpr());
        else
            load32(src, dst.gpr());
    }

    void loadReg(Address src, Reg dst)
    {
        load64ToReg(src, dst);
    }

    template<typename T, typename U>
    void storeCell(T cell, U address)
    {
        store64(cell, address);
    }

    template<typename U>
    void storeCell(GPRReg cell, U address)
    {
        store64(cell, address);
    }

    void storeCell(JSValueRegs regs, void* address)
    {
        store64(regs.gpr(), address);
    }

    void loadCell(Address address, GPRReg gpr)
    {
        load64(address, gpr);
    }

    void storeValue(JSValueRegs regs, Address address)
    {
        store64(regs.gpr(), address);
    }

    void storeValue(JSValueRegs regs, BaseIndex address)
    {
        store64(regs.gpr(), address);
    }

    void storeValue(JSValueRegs regs, void* address)
    {
        store64(regs.gpr(), address);
    }

    void loadValue(Address address, JSValueRegs regs)
    {
        load64(address, regs.gpr());
    }

    void loadValue(BaseIndex address, JSValueRegs regs)
    {
        load64(address, regs.gpr());
    }

    void loadValue(void* address, JSValueRegs regs)
    {
        load64(address, regs.gpr());
    }
    
    // Note that these clobber offset.
    void loadProperty(GPRReg object, GPRReg offset, JSValueRegs result);
    void storeProperty(JSValueRegs value, GPRReg object, GPRReg offset, GPRReg scratch);

    JumpList loadMegamorphicProperty(VM&, GPRReg baseGPR, GPRReg uidGPR, UniquedStringImpl*, GPRReg resultGPR, GPRReg scratch1GPR, GPRReg scratch2GPR, GPRReg scratch3GPR);
    JumpList loadMegamorphicGetterSetter(VM&, GPRReg baseGPR, GPRReg uidGPR, UniquedStringImpl*, GPRReg resultGPR, GPRReg scratch1GPR, GPRReg scratch2GPR, GPRReg scratch3GPR);
    template<uint32_t primaryMask, ptrdiff_t primaryEntriesOffset, uint32_t secondaryMask, ptrdiff_t secondaryEntriesOffset>
    JumpList findMegamorphicCacheEntry(VM&, GPRReg baseGPR, GPRReg uidGPR, UniquedStringImpl*, GPRReg scratch1GPR, GPRReg scratch2GPR, GPRReg scratch3GPR);
    std::tuple<JumpList, JumpList> storeMegamorphicProperty(VM&, GPRReg baseGPR, GPRReg uidGPR, UniquedStringImpl*, GPRReg valueGPR, GPRReg scratch1GPR, GPRReg scratch2GPR, GPRReg scratch3GPR);
    JumpList hasMegamorphicProperty(VM&, GPRReg baseGPR, GPRReg uidGPR, UniquedStringImpl*, GPRReg resultGPR, GPRReg scratch1GPR, GPRReg scratch2GPR, GPRReg scratch3GPR);
    JumpList loadCacheableIdentifierImpl(GPRReg propertyGPR, GPRReg destGPR, bool propertyIsString, bool propertyIsSymbol, bool canBeRope = true);

    void moveValueRegs(JSValueRegs srcRegs, JSValueRegs destRegs)
    {
        move(srcRegs.gpr(), destRegs.gpr());
    }

    void moveValue(JSValue value, JSValueRegs regs)
    {
        move(Imm64(JSValue::encode(value)), regs.gpr());
    }

    void moveTrustedValue(JSValue value, JSValueRegs regs)
    {
        move(TrustedImm64(JSValue::encode(value)), regs.gpr());
    }

    void storeValue(JSValue value, Address address)
    {
        store64(Imm64(JSValue::encode(value)), address);
    }

    void storeTrustedValue(JSValue value, Address address)
    {
        store64(TrustedImm64(JSValue::encode(value)), address);
    }

    void storeTrustedValue(JSValue value, BaseIndex address)
    {
        store64(TrustedImm64(JSValue::encode(value)), address);
    }

    template<typename Op> class Spooler;
    class LoadRegSpooler;
    class StoreRegSpooler;
    class CopySpooler;

    Address addressFor(const RegisterAtOffset& entry)
    {
        return Address(GPRInfo::callFrameRegister, entry.offset());
    }

    void emitSave(const RegisterAtOffsetList&);
    void emitRestore(const RegisterAtOffsetList&, GPRReg = GPRInfo::callFrameRegister);

    void emitSaveCalleeSavesFor(const RegisterAtOffsetList* calleeSaves);
    
    enum RestoreTagRegisterMode { UseExistingTagRegisterContents, CopyBaselineCalleeSavedRegistersFromBaseFrame };

    void emitSaveOrCopyLLIntBaselineCalleeSavesFor(CodeBlock*, VirtualRegister offsetVirtualRegister, RestoreTagRegisterMode, GPRReg temp1, GPRReg temp2, GPRReg temp3);

    void emitRestoreCalleeSavesFor(const RegisterAtOffsetList* calleeSaves);

    void emitSaveThenMaterializeTagRegisters()
    {
#if CPU(ARM64) || CPU(RISCV64)
        pushPair(GPRInfo::numberTagRegister, GPRInfo::notCellMaskRegister);
#else
        push(GPRInfo::numberTagRegister);
        push(GPRInfo::notCellMaskRegister);
#endif
        emitMaterializeTagCheckRegisters();
    }

    void emitRestoreSavedTagRegisters()
    {
#if CPU(ARM64) || CPU(RISCV64)
        popPair(GPRInfo::numberTagRegister, GPRInfo::notCellMaskRegister);
#else
        pop(GPRInfo::notCellMaskRegister);
        pop(GPRInfo::numberTagRegister);
#endif
    }

    // If you use this, be aware that vmGPR will get trashed.
    void copyCalleeSavesToVMEntryFrameCalleeSavesBuffer(GPRReg vmGPR)
    {
#if NUMBER_OF_CALLEE_SAVES_REGISTERS > 0
        loadPtr(Address(vmGPR, VM::topEntryFrameOffset()), vmGPR);
        copyCalleeSavesToEntryFrameCalleeSavesBufferImpl(vmGPR);
#else
        UNUSED_PARAM(vmGPR);
#endif
    }

    void copyCalleeSavesToEntryFrameCalleeSavesBuffer(EntryFrame*& topEntryFrame, GPRReg scratch)
    {
#if NUMBER_OF_CALLEE_SAVES_REGISTERS > 0
        loadPtr(&topEntryFrame, scratch);
        copyCalleeSavesToEntryFrameCalleeSavesBufferImpl(scratch);
#else
        UNUSED_PARAM(topEntryFrame);
        UNUSED_PARAM(scratch);
#endif
    }

    void copyCalleeSavesToEntryFrameCalleeSavesBuffer(GPRReg topEntryFrame)
    {
#if NUMBER_OF_CALLEE_SAVES_REGISTERS > 0
        copyCalleeSavesToEntryFrameCalleeSavesBufferImpl(topEntryFrame);
#else
        UNUSED_PARAM(topEntryFrame);
#endif
    }

    void restoreCalleeSavesFromEntryFrameCalleeSavesBuffer(EntryFrame*&);
    void restoreCalleeSavesFromVMEntryFrameCalleeSavesBuffer(GPRReg vmGPR, GPRReg scratchGPR);
    void restoreCalleeSavesFromVMEntryFrameCalleeSavesBufferImpl(GPRReg entryFrame, const RegisterSet& skipList);

    void copyLLIntBaselineCalleeSavesFromFrameOrRegisterToEntryFrameCalleeSavesBuffer(EntryFrame*&, const RegisterSet& usedRegisters = RegisterSet::stubUnavailableRegisters());

    void emitMaterializeTagCheckRegisters()
    {
        move(MacroAssembler::TrustedImm64(JSValue::NumberTag), GPRInfo::numberTagRegister);
        or64(MacroAssembler::TrustedImm32(JSValue::OtherTag), GPRInfo::numberTagRegister, GPRInfo::notCellMaskRegister);
    }

#if CPU(X86_64)
    void emitFunctionPrologue()
    {
        push(framePointerRegister);
        move(stackPointerRegister, framePointerRegister);
    }

    void emitFunctionEpilogueWithEmptyFrame()
    {
        pop(framePointerRegister);
    }

    void emitFunctionEpilogue()
    {
        move(framePointerRegister, stackPointerRegister);
        pop(framePointerRegister);
    }

    void preserveReturnAddressAfterCall(GPRReg reg)
    {
        pop(reg);
    }

    void restoreReturnAddressBeforeReturn(GPRReg reg)
    {
        push(reg);
    }

    void restoreReturnAddressBeforeReturn(Address address)
    {
        push(address);
    }

    // dest = base + index << shift.
    void shiftAndAdd(RegisterID base, RegisterID index, uint8_t shift, RegisterID dest, std::optional<RegisterID> optionalScratch = { })
    {
        ASSERT(shift < 32);
        if (shift <= 3) {
            x86Lea64(BaseIndex(base, index, static_cast<Scale>(shift)), dest);
            return;
        }

        RegisterID scratch = dest;
        bool needToPreserveIndexRegister = false;
        if (base == dest) {
            scratch = optionalScratch ? optionalScratch.value() : scratchRegister();
            if (base == scratch) {
                scratch = index;
                needToPreserveIndexRegister = true;
            } else if (index == scratch)
                needToPreserveIndexRegister = true;
            if (needToPreserveIndexRegister)
                push(index);
        }

        move(index, scratch);
        lshift64(TrustedImm32(shift), scratch);
        m_assembler.leaq_mr(0, base, scratch, 0, dest);

        if (needToPreserveIndexRegister)
            pop(index);
    }

#endif // CPU(X86_64)

#if CPU(ARM64)
    void emitFunctionPrologue()
    {
        tagReturnAddress();
        pushPair(framePointerRegister, linkRegister);
        move(stackPointerRegister, framePointerRegister);
    }

    void emitFunctionEpilogueWithEmptyFrame()
    {
        popPair(framePointerRegister, linkRegister);
    }

    void emitFunctionEpilogue()
    {
        move(framePointerRegister, stackPointerRegister);
        emitFunctionEpilogueWithEmptyFrame();
    }

    ALWAYS_INLINE void preserveReturnAddressAfterCall(RegisterID reg)
    {
        move(linkRegister, reg);
    }

    ALWAYS_INLINE void restoreReturnAddressBeforeReturn(RegisterID reg)
    {
        move(reg, linkRegister);
    }

    ALWAYS_INLINE void restoreReturnAddressBeforeReturn(Address address)
    {
        loadPtr(address, linkRegister);
    }

#if CPU(ARM64)
    // dest = base + index << shift.
    void shiftAndAdd(RegisterID base, RegisterID index, uint8_t shift, RegisterID dest, std::optional<RegisterID> = { })
    {
        ASSERT(shift < 32);
        ASSERT(base != index);
        getEffectiveAddress(BaseIndex(base, index, static_cast<Scale>(shift)), dest);
    }
#endif // CPU(ARM64)
#endif

#if CPU(RISCV64)
    void emitFunctionPrologue()
    {
        pushPair(framePointerRegister, linkRegister);
        move(stackPointerRegister, framePointerRegister);
    }

    void emitFunctionEpilogueWithEmptyFrame()
    {
        popPair(framePointerRegister, linkRegister);
    }

    void emitFunctionEpilogue()
    {
        move(framePointerRegister, stackPointerRegister);
        emitFunctionEpilogueWithEmptyFrame();
    }

    ALWAYS_INLINE void preserveReturnAddressAfterCall(RegisterID reg)
    {
        move(linkRegister, reg);
    }

    ALWAYS_INLINE void restoreReturnAddressBeforeReturn(RegisterID reg)
    {
        move(reg, linkRegister);
    }

    ALWAYS_INLINE void restoreReturnAddressBeforeReturn(Address address)
    {
        loadPtr(address, linkRegister);
    }
#endif

    void getArityPadding(VM&, unsigned numberOfParameters, GPRReg argumentCountIncludingThisGPR, GPRReg paddingOutputGPR, GPRReg scratchGPR0, GPRReg scratchGPR1, JumpList& stackOverflow);

    void emitGetFromCallFrameHeaderPtr(VirtualRegister entry, GPRReg to, GPRReg from = GPRInfo::callFrameRegister)
    {
        ASSERT(entry.isHeader());
        loadPtr(Address(from, entry.offset() * sizeof(Register)), to);
    }

    void emitPutToCallFrameHeader(GPRReg from, VirtualRegister entry)
    {
        ASSERT(entry.isHeader());
        storePtr(from, Address(GPRInfo::callFrameRegister, entry.offset() * sizeof(Register)));
    }

    void emitPutToCallFrameHeader(void* value, VirtualRegister entry)
    {
        ASSERT(entry.isHeader());
        storePtr(TrustedImmPtr(value), Address(GPRInfo::callFrameRegister, entry.offset() * sizeof(Register)));
    }

    void emitPutCellToCallFrameHeader(GPRReg from, VirtualRegister entry)
    {
        ASSERT(entry.isHeader());
        storeCell(from, Address(GPRInfo::callFrameRegister, entry.offset() * sizeof(Register)));
    }

    void emitZeroToCallFrameHeader(VirtualRegister entry)
    {
        ASSERT(entry.isHeader());
        storePtr(TrustedImmPtr(nullptr), Address(GPRInfo::callFrameRegister, entry.offset() * sizeof(Register)));
    }

    JumpList branchIfNotEqual(JSValueRegs regs, JSValue value)
    {
        return branch64(NotEqual, regs.gpr(), TrustedImm64(JSValue::encode(value)));
    }
    
    Jump branchIfEqual(JSValueRegs regs, JSValue value)
    {
        return branch64(Equal, regs.gpr(), TrustedImm64(JSValue::encode(value)));
    }

    template<typename T>
    Jump branchIfNotCell(T maybeCell, TagRegistersMode mode = HaveTagRegisters)
    {
        if (mode == HaveTagRegisters)
            return branchTest64(NonZero, maybeCell, GPRInfo::notCellMaskRegister);
        return branchTest64(NonZero, maybeCell, TrustedImm64(JSValue::NotCellMask));
    }

    Jump branchIfNotCell(JSValueRegs regs, TagRegistersMode mode = HaveTagRegisters)
    {
        return branchIfNotCell(regs.gpr(), mode);
    }

    template<typename T>
    Jump branchIfCell(T maybeCell, TagRegistersMode mode = HaveTagRegisters)
    {
        if (mode == HaveTagRegisters)
            return branchTest64(Zero, maybeCell, GPRInfo::notCellMaskRegister);
        return branchTest64(Zero, maybeCell, TrustedImm64(JSValue::NotCellMask));
    }

    Jump branchIfCell(JSValueRegs regs, TagRegistersMode mode = HaveTagRegisters)
    {
        return branchIfCell(regs.gpr(), mode);
    }
    
    Jump branchIfOther(JSValueRegs regs, GPRReg tempGPR)
    {
        and64(TrustedImm32(~JSValue::UndefinedTag), regs.gpr(), tempGPR);
        return branch64(Equal, tempGPR, TrustedImm64(JSValue::ValueNull));
    }
    
    Jump branchIfNotOther(JSValueRegs regs, GPRReg tempGPR)
    {
        and64(TrustedImm32(~JSValue::UndefinedTag), regs.gpr(), tempGPR);
        return branch64(NotEqual, tempGPR, TrustedImm64(JSValue::ValueNull));
    }
    
    Jump branchIfInt32(GPRReg gpr, TagRegistersMode mode = HaveTagRegisters)
    {
        if (mode == HaveTagRegisters)
            return branch64(AboveOrEqual, gpr, GPRInfo::numberTagRegister);
        return branch64(AboveOrEqual, gpr, TrustedImm64(JSValue::NumberTag));
    }

    Jump branchIfInt32(JSValueRegs regs, TagRegistersMode mode = HaveTagRegisters)
    {
        return branchIfInt32(regs.gpr(), mode);
    }

    Jump branchIfNotInt32(GPRReg gpr, TagRegistersMode mode = HaveTagRegisters)
    {
        if (mode == HaveTagRegisters)
            return branch64(Below, gpr, GPRInfo::numberTagRegister);
        return branch64(Below, gpr, TrustedImm64(JSValue::NumberTag));
    }

    Jump branchIfNotInt32(JSValueRegs regs, TagRegistersMode mode = HaveTagRegisters)
    {
        return branchIfNotInt32(regs.gpr(), mode);
    }

    Jump branchIfNumber(GPRReg gpr, TagRegistersMode mode = HaveTagRegisters)
    {
        if (mode == HaveTagRegisters)
            return branchTest64(NonZero, gpr, GPRInfo::numberTagRegister);
        return branchTest64(NonZero, gpr, TrustedImm64(JSValue::NumberTag));
    }

    Jump branchIfNumber(JSValueRegs regs, TagRegistersMode mode = HaveTagRegisters)
    {
        return branchIfNumber(regs.gpr(), mode);
    }

    Jump branchIfNotNumber(GPRReg gpr, TagRegistersMode mode = HaveTagRegisters)
    {
        if (mode == HaveTagRegisters)
            return branchTest64(Zero, gpr, GPRInfo::numberTagRegister);
        return branchTest64(Zero, gpr, TrustedImm64(JSValue::NumberTag));
    }

    Jump branchIfNotNumber(JSValueRegs regs, TagRegistersMode mode = HaveTagRegisters)
    {
        return branchIfNotNumber(regs.gpr(), mode);
    }

    Jump branchIfNotDoubleKnownNotInt32(JSValueRegs regs, TagRegistersMode mode = HaveTagRegisters)
    {
        if (mode == HaveTagRegisters)
            return branchTest64(Zero, regs.gpr(), GPRInfo::numberTagRegister);
        return branchTest64(Zero, regs.gpr(), TrustedImm64(JSValue::NumberTag));
    }

    Jump branchIfBoolean(GPRReg gpr, GPRReg tempGPR)
    {
        ASSERT(tempGPR != InvalidGPRReg);
        xor64(TrustedImm32(JSValue::ValueFalse), gpr, tempGPR);
        return branchTest64(Zero, tempGPR, TrustedImm32(static_cast<int32_t>(~1)));
    }

    Jump branchIfBoolean(JSValueRegs regs, GPRReg tempGPR)
    {
        return branchIfBoolean(regs.gpr(), tempGPR);
    }

    Jump branchIfNotBoolean(GPRReg gpr, GPRReg tempGPR)
    {
        ASSERT(tempGPR != InvalidGPRReg);
        xor64(TrustedImm32(JSValue::ValueFalse), gpr, tempGPR);
        return branchTest64(NonZero, tempGPR, TrustedImm32(static_cast<int32_t>(~1)));
    }

    Jump branchIfNotBoolean(JSValueRegs regs, GPRReg tempGPR)
    {
        return branchIfNotBoolean(regs.gpr(), tempGPR);
    }

#if USE(BIGINT32)
    Jump branchIfBigInt32(GPRReg gpr, GPRReg tempGPR, TagRegistersMode mode = HaveTagRegisters)
    {
        ASSERT(tempGPR != InvalidGPRReg);
        if (mode == HaveTagRegisters && gpr != tempGPR) {
            static_assert(JSValue::BigInt32Mask == JSValue::NumberTag + JSValue::BigInt32Tag);
            add64(TrustedImm32(JSValue::BigInt32Tag), GPRInfo::numberTagRegister, tempGPR);
            and64(gpr, tempGPR);
            return branch64(Equal, tempGPR, TrustedImm32(JSValue::BigInt32Tag));
        }

        and64(TrustedImm64(JSValue::BigInt32Mask), gpr, tempGPR);
        return branch64(Equal, tempGPR, TrustedImm32(JSValue::BigInt32Tag));
    }
    Jump branchIfNotBigInt32(GPRReg gpr, GPRReg tempGPR, TagRegistersMode mode = HaveTagRegisters)
    {
        ASSERT(tempGPR != InvalidGPRReg);
        if (mode == HaveTagRegisters && gpr != tempGPR) {
            static_assert(JSValue::BigInt32Mask == JSValue::NumberTag + JSValue::BigInt32Tag);
            add64(TrustedImm32(JSValue::BigInt32Tag), GPRInfo::numberTagRegister, tempGPR);
            and64(gpr, tempGPR);
            return branch64(NotEqual, tempGPR, TrustedImm32(JSValue::BigInt32Tag));
        }
        and64(TrustedImm64(JSValue::BigInt32Mask), gpr, tempGPR);
        return branch64(NotEqual, tempGPR, TrustedImm32(JSValue::BigInt32Tag));
    }
    Jump branchIfBigInt32(JSValueRegs regs, GPRReg tempGPR, TagRegistersMode mode = HaveTagRegisters)
    {
        return branchIfBigInt32(regs.gpr(), tempGPR, mode);
    }
    Jump branchIfNotBigInt32(JSValueRegs regs, GPRReg tempGPR, TagRegistersMode mode = HaveTagRegisters)
    {
        return branchIfNotBigInt32(regs.gpr(), tempGPR, mode);
    }
#endif // USE(BIGINT32)

    // FIXME: rename these to make it clear that they require their input to be a cell.
    Jump branchIfObject(GPRReg cellGPR)
    {
        return branch8(
            AboveOrEqual, Address(cellGPR, JSCell::typeInfoTypeOffset()), TrustedImm32(ObjectType));
    }
    
    Jump branchIfNotObject(GPRReg cellGPR)
    {
        return branch8(
            Below, Address(cellGPR, JSCell::typeInfoTypeOffset()), TrustedImm32(ObjectType));
    }

    // Note that first and last are inclusive.
    Jump branchIfType(GPRReg cellGPR, JSTypeRange range)
    {
        if (range.last == range.first)
            return branch8(Equal, Address(cellGPR, JSCell::typeInfoTypeOffset()), TrustedImm32(range.first));

        ASSERT(range.last > range.first);
        GPRReg scratch = scratchRegister();
        load8(Address(cellGPR, JSCell::typeInfoTypeOffset()), scratch);
        sub32(TrustedImm32(range.first), scratch);
        return branch32(BelowOrEqual, scratch, TrustedImm32(range.last - range.first));
    }

    Jump branchIfType(GPRReg cellGPR, JSType type)
    {
        return branchIfType(cellGPR, JSTypeRange { type, type });
    }

    Jump branchIfNotType(GPRReg cellGPR, JSTypeRange range)
    {
        if (range.last == range.first)
            return branch8(NotEqual, Address(cellGPR, JSCell::typeInfoTypeOffset()), TrustedImm32(range.first));

        ASSERT(range.last > range.first);
        GPRReg scratch = scratchRegister();
        load8(Address(cellGPR, JSCell::typeInfoTypeOffset()), scratch);
        sub32(TrustedImm32(range.first), scratch);
        return branch32(Above, scratch, TrustedImm32(range.last - range.first));
    }

    Jump branchIfNotType(GPRReg cellGPR, JSType type)
    {
        return branchIfNotType(cellGPR, JSTypeRange { type, type });
    }

    // FIXME: rename these to make it clear that they require their input to be a cell.
    Jump branchIfString(GPRReg cellGPR) { return branchIfType(cellGPR, StringType); }
    Jump branchIfNotString(GPRReg cellGPR) { return branchIfNotType(cellGPR, StringType); }
    Jump branchIfSymbol(GPRReg cellGPR) { return branchIfType(cellGPR, SymbolType); }
    Jump branchIfNotSymbol(GPRReg cellGPR) { return branchIfNotType(cellGPR, SymbolType); }
    Jump branchIfHeapBigInt(GPRReg cellGPR) { return branchIfType(cellGPR, HeapBigIntType); }
    Jump branchIfNotHeapBigInt(GPRReg cellGPR) { return branchIfNotType(cellGPR, HeapBigIntType); }
    Jump branchIfFunction(GPRReg cellGPR) { return branchIfType(cellGPR, JSFunctionType); }
    Jump branchIfNotFunction(GPRReg cellGPR) { return branchIfNotType(cellGPR, JSFunctionType); }
    Jump branchIfStructure(GPRReg cellGPR) { return branchIfType(cellGPR, StructureType); }
    Jump branchIfNotStructure(GPRReg cellGPR) { return branchIfNotType(cellGPR, StructureType); }
    
    void isEmpty(GPRReg gpr, GPRReg dst)
    {
        test64(Zero, gpr, gpr, dst);
    }

    void toBigInt64(GPRReg cellGPR, GPRReg destGPR)
    {
        ASSERT(noOverlap(cellGPR, destGPR));
        load32(Address(cellGPR, JSBigInt::offsetOfLength()), destGPR);
        JumpList doneCases;
        doneCases.append(branchTest32(Zero, destGPR));
        load64(Address(cellGPR, JSBigInt::offsetOfData()), destGPR);
        doneCases.append(branchTest8(Zero, Address(cellGPR, JSCell::typeInfoFlagsOffset()), TrustedImm32(TypeInfoPerCellBit)));
        neg64(destGPR);
        doneCases.link(this);
    }

    void isNotEmpty(GPRReg gpr, GPRReg dst)
    {
        test64(NonZero, gpr, gpr, dst);
    }

    Jump branchIfEmpty(BaseIndex address)
    {
        return branchTest64(Zero, address);
    }

    Jump branchIfEmpty(GPRReg gpr)
    {
        return branchTest64(Zero, gpr);
    }

    Jump branchIfEmpty(JSValueRegs regs)
    {
        return branchIfEmpty(regs.gpr());
    }

    Jump branchIfNotEmpty(BaseIndex address)
    {
        return branchTest64(NonZero, address);
    }

    Jump branchIfNotEmpty(GPRReg gpr)
    {
        return branchTest64(NonZero, gpr);
    }

    Jump branchIfNotEmpty(JSValueRegs regs)
    {
        return branchIfNotEmpty(regs.gpr());
    }

    void isUndefined(JSValueRegs regs, GPRReg dst)
    {
        compare64(Equal, regs.payloadGPR(), TrustedImm32(JSValue::ValueUndefined), dst);
    }

    // Note that this function does not respect MasqueradesAsUndefined.
    Jump branchIfUndefined(GPRReg gpr)
    {
        return branch64(Equal, gpr, TrustedImm64(JSValue::encode(jsUndefined())));
    }

    // Note that this function does not respect MasqueradesAsUndefined.
    Jump branchIfUndefined(JSValueRegs regs)
    {
        return branchIfUndefined(regs.gpr());
    }

    // Note that this function does not respect MasqueradesAsUndefined.
    Jump branchIfNotUndefined(GPRReg gpr)
    {
        return branch64(NotEqual, gpr, TrustedImm64(JSValue::encode(jsUndefined())));
    }

    // Note that this function does not respect MasqueradesAsUndefined.
    Jump branchIfNotUndefined(JSValueRegs regs)
    {
        return branchIfNotUndefined(regs.gpr());
    }

    void isNull(JSValueRegs regs, GPRReg dst)
    {
        compare64(Equal, regs.payloadGPR(), TrustedImm32(JSValue::ValueNull), dst);
    }

    void isNotNull(JSValueRegs regs, GPRReg dst)
    {
        compare64(NotEqual, regs.payloadGPR(), TrustedImm32(JSValue::ValueNull), dst);
    }

    Jump branchIfNull(GPRReg gpr)
    {
        return branch64(Equal, gpr, TrustedImm64(JSValue::encode(jsNull())));
    }

    Jump branchIfNull(JSValueRegs regs)
    {
        return branchIfNull(regs.gpr());
    }

    Jump branchIfNotNull(GPRReg gpr)
    {
        return branch64(NotEqual, gpr, TrustedImm64(JSValue::encode(jsNull())));
    }

    Jump branchIfNotNull(JSValueRegs regs)
    {
        return branchIfNotNull(regs.gpr());
    }

    Jump branchIfTrue(GPRReg gpr)
    {
        return branch64(Equal, gpr, TrustedImm64(JSValue::encode(jsBoolean(true))));
    }

    Jump branchIfNotTrue(GPRReg gpr)
    {
        return branch64(NotEqual, gpr, TrustedImm64(JSValue::encode(jsBoolean(true))));
    }

    Jump branchIfFalse(GPRReg gpr)
    {
        return branch64(Equal, gpr, TrustedImm64(JSValue::encode(jsBoolean(false))));
    }

    Jump branchIfNotFalse(GPRReg gpr)
    {
        return branch64(NotEqual, gpr, TrustedImm64(JSValue::encode(jsBoolean(false))));
    }

    template<typename T>
    Jump branchStructure(RelationalCondition condition, T leftHandSide, Structure* structure)
    {
        return branch32(condition, leftHandSide, TrustedImm32(structure->id().bits()));
    }

    Jump branchIfFastTypedArray(GPRReg baseGPR);
    Jump branchIfNotFastTypedArray(GPRReg baseGPR);

    Jump branchIfNaN(FPRReg fpr)
    {
        return branchDouble(DoubleNotEqualOrUnordered, fpr, fpr);
    }

    Jump branchIfNotNaN(FPRReg fpr)
    {
        return branchDouble(DoubleEqualAndOrdered, fpr, fpr);
    }

    Jump branchIfRopeStringImpl(GPRReg stringImplGPR)
    {
        return branchTestPtr(NonZero, stringImplGPR, TrustedImm32(JSString::isRopeInPointer));
    }

    Jump branchIfNotRopeStringImpl(GPRReg stringImplGPR)
    {
        return branchTestPtr(Zero, stringImplGPR, TrustedImm32(JSString::isRopeInPointer));
    }

    JumpList branchIfResizableOrGrowableSharedTypedArrayIsOutOfBounds(GPRReg baseGPR, GPRReg scratchGPR, GPRReg scratch2GPR, std::optional<TypedArrayType>);
    void loadTypedArrayByteLength(GPRReg baseGPR, GPRReg valueGPR, GPRReg scratchGPR, GPRReg scratch2GPR, TypedArrayType);
    std::tuple<Jump, JumpList> loadDataViewByteLength(GPRReg baseGPR, GPRReg valueGPR, GPRReg scratchGPR, GPRReg scratch2GPR, TypedArrayType);
    void loadTypedArrayLength(GPRReg baseGPR, GPRReg valueGPR, GPRReg scratchGPR, GPRReg scratch2GPR, std::optional<TypedArrayType>);

    void emitTurnUndefinedIntoNull(JSValueRegs regs)
    {
        static_assert((JSValue::ValueUndefined & ~JSValue::UndefinedTag) == JSValue::ValueNull);
        and64(TrustedImm32(~JSValue::UndefinedTag), regs.payloadGPR());
    }

    static Address addressForByteOffset(ptrdiff_t byteOffset)
    {
        return Address(GPRInfo::callFrameRegister, byteOffset);
    }
    static Address addressFor(VirtualRegister virtualRegister, GPRReg baseReg)
    {
        ASSERT(virtualRegister.isValid());
        return Address(baseReg, virtualRegister.offset() * sizeof(Register));
    }
    static Address addressFor(VirtualRegister virtualRegister)
    {
        // NB. It's tempting on some architectures to sometimes use an offset from the stack
        // register because for some offsets that will encode to a smaller instruction. But we
        // cannot do this. We use this in places where the stack pointer has been moved to some
        // unpredictable location.
        ASSERT(virtualRegister.isValid());
        return Address(GPRInfo::callFrameRegister, virtualRegister.offset() * sizeof(Register));
    }
    static Address addressFor(Operand operand)
    {
        ASSERT(!operand.isTmp());
        return addressFor(operand.virtualRegister());
    }

    static Address highWordFor(VirtualRegister virtualRegister, GPRReg baseGPR)
    {
        ASSERT(virtualRegister.isValid());
        return Address(baseGPR, virtualRegister.offset() * sizeof(Register) + HighWordOffset);
    }

    static Address highWordFor(VirtualRegister virtualRegister)
    {
        ASSERT(virtualRegister.isValid());
        return Address(GPRInfo::callFrameRegister, virtualRegister.offset() * sizeof(Register) + HighWordOffset);
    }

    static Address highWordFor(Operand operand)
    {
        ASSERT(!operand.isTmp());
        return highWordFor(operand.virtualRegister());
    }

    static Address lowWordFor(VirtualRegister virtualRegister, GPRReg baseGPR)
    {
        ASSERT(virtualRegister.isValid());
        return Address(baseGPR, virtualRegister.offset() * sizeof(Register) + LowWordOffset);
    }

    static Address lowWordFor(VirtualRegister virtualRegister)
    {
        ASSERT(virtualRegister.isValid());
        return Address(GPRInfo::callFrameRegister, virtualRegister.offset() * sizeof(Register) + LowWordOffset);
    }

    static Address lowWordFor(Operand operand)
    {
        ASSERT(!operand.isTmp());
        return lowWordFor(operand.virtualRegister());
    }

    // Access to our fixed callee CallFrame.
    static Address calleeFrameSlot(VirtualRegister slot)
    {
        ASSERT(slot.offset() >= CallerFrameAndPC::sizeInRegisters);
        return Address(stackPointerRegister, sizeof(Register) * (slot - CallerFrameAndPC::sizeInRegisters).offset());
    }

    // Access to our fixed callee CallFrame.
    static Address calleeArgumentSlot(int argument)
    {
        return calleeFrameSlot(virtualRegisterForArgumentIncludingThis(argument));
    }

    static Address calleeFrameHighWordSlot(VirtualRegister slot)
    {
        return calleeFrameSlot(slot).withOffset(HighWordOffset);
    }

    static Address calleeFrameLowWordSlot(VirtualRegister slot)
    {
        return calleeFrameSlot(slot).withOffset(LowWordOffset);
    }

    static Address calleeArgumentHighWordSlot(int argument)
    {
        return calleeArgumentSlot(argument).withOffset(HighWordOffset);
    }

    static Address calleeArgumentLowWordSlot(int argument)
    {
        return calleeArgumentSlot(argument).withOffset(LowWordOffset);
    }

    static Address calleeFrameCallerFrame()
    {
        return calleeFrameSlot(VirtualRegister(0)).withOffset(CallFrame::callerFrameOffset());
    }

    static Address calleeFrameCodeBlockBeforeCall()
    {
        return calleeFrameSlot(CallFrameSlot::codeBlock);
    }

    static Address calleeFrameCodeBlockBeforeTailCall()
    {
        // The stackPointerRegister state is "after the call, but before the function prologue".
        return calleeFrameSlot(CallFrameSlot::codeBlock).withOffset(sizeof(CallerFrameAndPC) - prologueStackPointerDelta());
    }

    static GPRReg selectScratchGPR(RegisterSet preserved)
    {
        GPRReg registers[] = {
            GPRInfo::regT0,
            GPRInfo::regT1,
            GPRInfo::regT2,
            GPRInfo::regT3,
            GPRInfo::regT4,
            GPRInfo::regT5,
#if CPU(ARM64)
            GPRInfo::regT6,
            GPRInfo::regT7,
            GPRInfo::regT8,
            GPRInfo::regT9,
            GPRInfo::regT10,
            GPRInfo::regT11,
            GPRInfo::regT12,
            GPRInfo::regT13,
            GPRInfo::regT14,
            GPRInfo::regT15,
#elif CPU(X86_64)
            GPRInfo::regT6,
            GPRInfo::regT7,
#elif CPU(RISCV64)
            GPRInfo::regT6,
            GPRInfo::regT7,
            GPRInfo::regT8,
            GPRInfo::regT9,
            GPRInfo::regT10,
            GPRInfo::regT11,
            GPRInfo::regT12,
#endif
        };

        for (GPRReg reg : registers) {
            if (!preserved.contains(reg, IgnoreVectors))
                return reg;
        }
        RELEASE_ASSERT_NOT_REACHED();
        return InvalidGPRReg;
    }

    template<typename... Regs>
    static GPRReg selectScratchGPR(Regs... args)
    {
        RegisterSet set;
        constructRegisterSet(set, args...);
        return selectScratchGPR(set);
    }

    static void constructRegisterSet(RegisterSet&)
    {
    }

    template<typename... Regs>
    static void constructRegisterSet(RegisterSet& set, JSValueRegs regs, Regs... args)
    {
        if (regs.payloadGPR() != InvalidGPRReg)
            set.add(regs.payloadGPR(), IgnoreVectors);
        constructRegisterSet(set, args...);
    }

    template<typename... Regs>
    static void constructRegisterSet(RegisterSet& set, GPRReg reg, Regs... args)
    {
        if (reg != InvalidGPRReg) {
            ASSERT(!Reg(reg).isFPR());
            set.add(reg, IgnoreVectors);
        }
        constructRegisterSet(set, args...);
    }

    // These methods JIT generate dynamic, debug-only checks - akin to ASSERTs.
#if ASSERT_ENABLED
    void jitAssertIsInt32(GPRReg);
    void jitAssertIsJSInt32(GPRReg);
    void jitAssertIsJSNumber(GPRReg);
    void jitAssertIsJSDouble(GPRReg);
    void jitAssertIsCell(GPRReg);
    void jitAssertHasValidCallFrame();
    void jitAssertIsNull(GPRReg);
    void jitAssertTagsInPlace();
    void jitAssertArgumentCountSane();
    inline void jitAssertNoException(VM& vm) { jitReleaseAssertNoException(vm); }
    void jitAssertCodeBlockOnCallFrameWithType(GPRReg scratchGPR, JITType);
    void jitAssertCodeBlockMatchesCurrentCalleeCodeBlockOnCallFrame(GPRReg scratchGPR, GPRReg scratchGPR2, UnlinkedCodeBlock&);
    void jitAssertCodeBlockOnCallFrameIsOptimizingJIT(GPRReg scratchGPR);
#else
    void jitAssertIsInt32(GPRReg) { }
    void jitAssertIsJSInt32(GPRReg) { }
    void jitAssertIsJSNumber(GPRReg) { }
    void jitAssertIsJSDouble(GPRReg) { }
    void jitAssertIsCell(GPRReg) { }
    void jitAssertHasValidCallFrame() { }
    void jitAssertIsNull(GPRReg) { }
    void jitAssertTagsInPlace() { }
    void jitAssertArgumentCountSane() { }
    void jitAssertNoException(VM&) { }
    void jitAssertCodeBlockOnCallFrameWithType(GPRReg, JITType) { }
    void jitAssertCodeBlockOnCallFrameIsOptimizingJIT(GPRReg) { }
    void jitAssertCodeBlockMatchesCurrentCalleeCodeBlockOnCallFrame(GPRReg, GPRReg, UnlinkedCodeBlock&) { }
#endif

    void jitReleaseAssertNoException(VM&);

    void incrementSuperSamplerCount();
    void decrementSuperSamplerCount();
    
    void purifyNaN(FPRReg, FPRReg);

    // These methods convert between doubles, and doubles boxed and JSValues.
    GPRReg boxDouble(FPRReg fpr, GPRReg gpr, TagRegistersMode mode = HaveTagRegisters)
    {
        moveDoubleTo64(fpr, gpr);
        if (mode == DoNotHaveTagRegisters)
            sub64(TrustedImm64(JSValue::NumberTag), gpr);
        else {
            sub64(GPRInfo::numberTagRegister, gpr);
            jitAssertIsJSDouble(gpr);
        }
        return gpr;
    }
    FPRReg unboxDoubleWithoutAssertions(GPRReg gpr, GPRReg resultGPR, FPRReg fpr, TagRegistersMode mode = HaveTagRegisters)
    {
        if (mode == DoNotHaveTagRegisters) {
            move(TrustedImm64(JSValue::NumberTag), resultGPR);
            add64(gpr, resultGPR);
        } else
            add64(GPRInfo::numberTagRegister, gpr, resultGPR);
        move64ToDouble(resultGPR, fpr);
        return fpr;
    }
    FPRReg unboxDouble(GPRReg gpr, GPRReg resultGPR, FPRReg fpr, TagRegistersMode mode = HaveTagRegisters)
    {
        jitAssertIsJSDouble(gpr);
        return unboxDoubleWithoutAssertions(gpr, resultGPR, fpr, mode);
    }
    void unboxDouble(JSValueRegs regs, GPRReg resultGPR, FPRReg fpr)
    {
        unboxDouble(regs.payloadGPR(), resultGPR, fpr);
    }
    void unboxDouble(JSValueRegs regs, FPRReg fpr)
    {
        unboxDouble(regs.payloadGPR(), regs.payloadGPR(), fpr);
    }
    void boxDouble(FPRReg fpr, JSValueRegs regs, TagRegistersMode mode = HaveTagRegisters)
    {
        boxDouble(fpr, regs.gpr(), mode);
    }

    void unboxDoubleNonDestructive(JSValueRegs regs, FPRReg destFPR, GPRReg resultGPR)
    {
        unboxDouble(regs.payloadGPR(), resultGPR, destFPR);
    }

    Jump isStrictInt52(GPRReg valueGPR, GPRReg scratchGPR)
    {
        // This moves the checking range (fail if N >= (1 << (52 - 1)) or N < -(1 << (52 - 1))) by subtracting a value.
        // So, valid value region starts with -1 and lower. In unsigned form, which means,
        // 0x00000000000000000 to 0x000fffffffffffff. So, by ignoring 52 bits, we can extract 0x000 part, and we can check whether it is zero.
        add64(TrustedImm64(0x0008000000000000ULL), valueGPR, scratchGPR);
        return branchTest64(Zero, scratchGPR, TrustedImm64(0xFFF0000000000000ULL));
    }

    Jump isNotStrictInt52(GPRReg valueGPR, GPRReg scratchGPR)
    {
        add64(TrustedImm64(0x0008000000000000ULL), valueGPR, scratchGPR);
        return branchTest64(NonZero, scratchGPR, TrustedImm64(0xFFF0000000000000ULL));
    }

    // Here are possible arrangements of source, target, scratch:
    // - source, target, scratch can all be separate registers.
    // - source and target can be the same but scratch is separate.
    // - target and scratch can be the same but source is separate.
    void boxInt52(GPRReg source, GPRReg target, GPRReg scratch, FPRReg fpScratch)
    {
        // Is it an int32?
        signExtend32ToPtr(source, scratch);
        Jump isInt32 = branch64(Equal, source, scratch);
        
        // Nope, it's not, but regT0 contains the int64 value.
        convertInt64ToDouble(source, fpScratch);
        boxDouble(fpScratch, target);
        Jump done = jump();
        
        isInt32.link(this);
        zeroExtend32ToWord(source, target);
        or64(GPRInfo::numberTagRegister, target);
        
        done.link(this);
    }

    void branchConvertDoubleToInt52(FPRegisterID srcFPR, RegisterID destGPR, JumpList& failureCases, RegisterID scratch1GPR, FPRegisterID scratch2FPR, bool canIgnoreNegativeZero)
    {
        JumpList doneCases;

        truncateDoubleToInt64(srcFPR, destGPR);

        bool convertedBack = false;
#if CPU(ARM64)
        if (supportsRoundFloatToIntegerFloat()) {
            convertedBack = true;
            roundTowardZeroInt64Double(srcFPR, scratch2FPR);
        }
#endif
        if (!convertedBack)
            convertInt64ToDouble(destGPR, scratch2FPR);

        failureCases.append(branchDouble(DoubleNotEqualOrUnordered, srcFPR, scratch2FPR));

        Jump isZero;
        if (!canIgnoreNegativeZero)
            isZero = branchTest64(Zero, destGPR);

        failureCases.append(isNotStrictInt52(destGPR, scratch1GPR));

        if (isZero.isSet()) {
            doneCases.append(jump());
            isZero.link(this);
            moveDoubleTo64(srcFPR, scratch1GPR);
            failureCases.append(branchTest64(NonZero, scratch1GPR, TrustedImm64(1ULL << 63)));
        }

        doneCases.link(this);
    }

#if USE(BIGINT32)
    void unboxBigInt32(GPRReg src, GPRReg dest)
    {
#if CPU(ARM64)
        urshift64(src, trustedImm32ForShift(Imm32(16)), dest);
#else
        move(src, dest);
        urshift64(trustedImm32ForShift(Imm32(16)), dest);
#endif
    }

    void boxBigInt32(GPRReg gpr)
    {
        lshift64(trustedImm32ForShift(Imm32(16)), gpr);
        or64(TrustedImm32(JSValue::BigInt32Tag), gpr);
    }
#endif

    void unboxNativeCallee(GPRReg boxedGPR, GPRReg calleeGPR)
    {
        and64(TrustedImm64(~static_cast<uint64_t>(JSValue::NativeCalleeTag)), boxedGPR, calleeGPR);
        add64(TrustedImm64(lowestAccessibleAddress()), calleeGPR);
    }

    void boxBooleanPayload(GPRReg boolGPR, GPRReg payloadGPR)
    {
        add32(TrustedImm32(JSValue::ValueFalse), boolGPR, payloadGPR);
    }

    void boxBooleanPayload(bool value, GPRReg payloadGPR)
    {
        move(TrustedImm32(JSValue::ValueFalse + value), payloadGPR);
    }

    void boxBoolean(GPRReg boolGPR, JSValueRegs boxedRegs)
    {
        boxBooleanPayload(boolGPR, boxedRegs.payloadGPR());
    }

    void boxBoolean(bool value, JSValueRegs boxedRegs)
    {
        boxBooleanPayload(value, boxedRegs.payloadGPR());
    }

    void boxInt32(GPRReg intGPR, JSValueRegs boxedRegs, TagRegistersMode mode = HaveTagRegisters)
    {
        if (mode == DoNotHaveTagRegisters)
            or64(TrustedImm64(JSValue::NumberTag), intGPR, boxedRegs.gpr());
        else
            or64(GPRInfo::numberTagRegister, intGPR, boxedRegs.gpr());
    }

    void boxCell(GPRReg cellGPR, JSValueRegs boxedRegs)
    {
        move(cellGPR, boxedRegs.gpr());
    }

    void boxNativeCallee(GPRReg calleeGPR, GPRReg boxedGPR)
    {
        sub64(calleeGPR, TrustedImm64(lowestAccessibleAddress()), boxedGPR);
        or64(TrustedImm64(JSValue::NativeCalleeTag), boxedGPR);
    }

    void callExceptionFuzz(VM&, GPRReg exceptionReg);

    enum ExceptionCheckKind { NormalExceptionCheck, InvertedExceptionCheck };
    enum ExceptionJumpWidth { NormalJumpWidth, FarJumpWidth };
    JS_EXPORT_PRIVATE Jump emitExceptionCheck(VM&, ExceptionCheckKind = NormalExceptionCheck, ExceptionJumpWidth = NormalJumpWidth, GPRReg exceptionReg = InvalidGPRReg);
    JS_EXPORT_PRIVATE Jump emitNonPatchableExceptionCheck(VM&, GPRReg exceptionReg = InvalidGPRReg);
    Jump emitJumpIfException(VM&);

#if ENABLE(SAMPLING_COUNTERS)
    static void emitCount(MacroAssembler& jit, AbstractSamplingCounter& counter, int32_t increment = 1)
    {
        jit.add64(TrustedImm32(increment), AbsoluteAddress(counter.addressOfCounter()));
    }
    void emitCount(AbstractSamplingCounter& counter, int32_t increment = 1)
    {
        add64(TrustedImm32(increment), AbsoluteAddress(counter.addressOfCounter()));
    }
#endif

#if ENABLE(SAMPLING_FLAGS)
    void setSamplingFlag(int32_t);
    void clearSamplingFlag(int32_t flag);
#endif

    CodeBlock* baselineCodeBlockFor(const CodeOrigin& codeOrigin)
    {
        return baselineCodeBlockForOriginAndBaselineCodeBlock(codeOrigin, baselineCodeBlock());
    }
    
    CodeBlock* baselineCodeBlockFor(InlineCallFrame* inlineCallFrame)
    {
        if (!inlineCallFrame)
            return baselineCodeBlock();
        return baselineCodeBlockForInlineCallFrame(inlineCallFrame);
    }
    
    CodeBlock* baselineCodeBlock()
    {
        return m_baselineCodeBlock;
    }
    
    static VirtualRegister argumentsStart(InlineCallFrame* inlineCallFrame)
    {
        if (!inlineCallFrame)
            return VirtualRegister(CallFrame::argumentOffset(0));
        if (inlineCallFrame->m_argumentsWithFixup.size() <= 1)
            return virtualRegisterForLocal(0);
        ValueRecovery recovery = inlineCallFrame->m_argumentsWithFixup[1];
        RELEASE_ASSERT(recovery.technique() == DisplacedInJSStack);
        return recovery.virtualRegister();
    }
    
    static VirtualRegister argumentsStart(const CodeOrigin& codeOrigin)
    {
        return argumentsStart(codeOrigin.inlineCallFrame());
    }

    static VirtualRegister argumentCount(InlineCallFrame* inlineCallFrame)
    {
        ASSERT(!inlineCallFrame || inlineCallFrame->isVarargs());
        if (!inlineCallFrame)
            return CallFrameSlot::argumentCountIncludingThis;
        return inlineCallFrame->argumentCountRegister;
    }

    static VirtualRegister argumentCount(const CodeOrigin& codeOrigin)
    {
        return argumentCount(codeOrigin.inlineCallFrame());
    }
    
    void emitLoadStructure(RegisterID cell, RegisterID dest);
    void emitNonNullDecodeZeroExtendedStructureID(RegisterID source, RegisterID dest);
    void emitLoadStructure(VM&, RegisterID source, RegisterID dest);
    void emitLoadPrototype(VM&, GPRReg objectGPR, JSValueRegs resultRegs, JumpList& slowPath);
    void emitEncodeStructureID(RegisterID source, RegisterID dest);

    void emitStoreStructureWithTypeInfo(TrustedImmPtr structure, RegisterID dest, RegisterID)
    {
        emitStoreStructureWithTypeInfo(*this, structure, dest);
    }

    void emitStoreStructureWithTypeInfo(RegisterID structure, RegisterID dest, RegisterID scratch)
    {
        // Store the StructureID
        emitEncodeStructureID(structure, scratch);
        store32(scratch, MacroAssembler::Address(dest, JSCell::structureIDOffset()));
        // Store all the info flags using a single 32-bit wide load and store.
        load32(MacroAssembler::Address(structure, Structure::indexingModeIncludingHistoryOffset()), scratch);
        store32(scratch, MacroAssembler::Address(dest, JSCell::indexingTypeAndMiscOffset()));
    }

    static void emitStoreStructureWithTypeInfo(AssemblyHelpers& jit, TrustedImmPtr structure, RegisterID dest);

    // Branch taken if the cell does not need a store barrier.
    // When reverse is true, branch taken when the store barrier is needed.
    Jump barrierBranchWithoutFence(GPRReg cell, bool reverse = false)
    {
        auto cond = Above;
        if (reverse)
            cond = BelowOrEqual;
        return branch8(cond, Address(cell, JSCell::cellStateOffset()), TrustedImm32(blackThreshold));
    }

    Jump barrierBranchWithoutFence(JSCell* cell)
    {
        uint8_t* address = reinterpret_cast<uint8_t*>(cell) + JSCell::cellStateOffset();
        return branch8(Above, AbsoluteAddress(address), TrustedImm32(blackThreshold));
    }
    
    // FIXME: We should name this something more obvious like branchIfCellIsRememberedOrEden. barrierBranch could mean many things.
    // Branch taken if the cell does not need a memory fence or store barrier.
    // When reverse is true, branch taken when the memory barrier or store barrier is needed.
    Jump barrierBranch(VM& vm, GPRReg cell, GPRReg scratchGPR, bool reverse = false)
    {
        auto cond = Above;
        if (reverse)
            cond = BelowOrEqual;
        load8(Address(cell, JSCell::cellStateOffset()), scratchGPR);
        return branch32(cond, scratchGPR, AbsoluteAddress(vm.heap.addressOfBarrierThreshold()));
    }

    Jump barrierBranch(VM& vm, JSCell* cell, GPRReg scratchGPR)
    {
        uint8_t* address = reinterpret_cast<uint8_t*>(cell) + JSCell::cellStateOffset();
        load8(address, scratchGPR);
        return branch32(Above, scratchGPR, AbsoluteAddress(vm.heap.addressOfBarrierThreshold()));
    }

    Jump branchIfBarriered(GPRReg vmGPR, GPRReg cellGPR, GPRReg scratchGPR)
    {
        load8(Address(cellGPR, JSCell::cellStateOffset()), scratchGPR);
        return branch32(BelowOrEqual, scratchGPR, Address(vmGPR, VM::offsetOfHeapBarrierThreshold()));
    }
    
    void barrierStoreLoadFence(VM& vm)
    {
        Jump ok = jumpIfMutatorFenceNotNeeded(vm);
        memoryFence();
        ok.link(this);
    }
    
    void mutatorFence(VM& vm)
    {
        if (isX86())
            return;
        Jump ok = jumpIfMutatorFenceNotNeeded(vm);
        storeFence();
        ok.link(this);
    }

    JS_EXPORT_PRIVATE void cage(Gigacage::Kind, GPRReg storage);
    // length may be the same register as scratch.
    JS_EXPORT_PRIVATE void cageConditionally(Gigacage::Kind, GPRReg storage, GPRReg length, GPRReg scratch);

    void emitComputeButterflyIndexingMask(GPRReg vectorLengthGPR, GPRReg scratchGPR, GPRReg resultGPR)
    {
        ASSERT(scratchGPR != resultGPR);
        Jump done;
        // If vectorLength == 0 then clz will return 32 on both ARM and x86. We can then do a 64-bit right shift on a 32-bit -1 to get a 0 mask for zero vectorLength.
        countLeadingZeros32(vectorLengthGPR, scratchGPR);
        move(TrustedImm32(-1), resultGPR);
        urshiftPtr(scratchGPR, resultGPR);
        if (done.isSet())
            done.link(this);
    }

    // If for whatever reason the butterfly is going to change vector length this function does NOT
    // update the indexing mask.
    void nukeStructureAndStoreButterfly(VM& vm, GPRReg butterfly, GPRReg object)
    {
        if (isX86()) {
            or32(TrustedImm32(std::bit_cast<int32_t>(StructureID::nukedStructureIDBit)), Address(object, JSCell::structureIDOffset()));
            storePtr(butterfly, Address(object, JSObject::butterflyOffset()));
            return;
        }

        Jump ok = jumpIfMutatorFenceNotNeeded(vm);
        or32(TrustedImm32(std::bit_cast<int32_t>(StructureID::nukedStructureIDBit)), Address(object, JSCell::structureIDOffset()));
        storeFence();
        storePtr(butterfly, Address(object, JSObject::butterflyOffset()));
        storeFence();
        Jump done = jump();
        ok.link(this);
        storePtr(butterfly, Address(object, JSObject::butterflyOffset()));
        done.link(this);
    }
    
    Jump jumpIfMutatorFenceNotNeeded(VM& vm)
    {
        return branchTest8(Zero, AbsoluteAddress(vm.heap.addressOfMutatorShouldBeFenced()));
    }
    
    // Emits the branch structure for typeof. The code emitted by this doesn't fall through. The
    // functor is called at those points where we have pinpointed a type. One way to use this is to
    // have the functor emit the code to put the type string into an appropriate register and then
    // jump out. A secondary functor is used for the call trap and masquerades-as-undefined slow
    // case. It is passed the unlinked jump to the slow case.
    template<typename Functor, typename SlowPathFunctor>
    void emitTypeOf(
        JSValueRegs regs, GPRReg tempGPR, const Functor& functor,
        const SlowPathFunctor& slowPathFunctor)
    {
        // Implements the following branching structure:
        //
        // if (is cell) {
        //     if (is object) {
        //         if (is function) {
        //             return function;
        //         } else if (doesn't have call trap and doesn't masquerade as undefined) {
        //             return object
        //         } else {
        //             return slowPath();
        //         }
        //     } else if (is string) {
        //         return string
        //     } else if (is heapbigint) {
        //         return bigint
        //     } else {
        //         return symbol
        //     }
        // } else if (is number) {
        //     return number
        // } else if (is null) {
        //     return object
        // } else if (is boolean) {
        //     return boolean
        // } else if (is bigint32) {
        //     return bigint
        // } else {
        //     return undefined
        // }
        //
        // FIXME: typeof Symbol should be more frequently seen than BigInt.
        // We should change the order of type detection based on this frequency.
        // https://bugs.webkit.org/show_bug.cgi?id=192650
        
        Jump notCell = branchIfNotCell(regs);
        
        GPRReg cellGPR = regs.payloadGPR();
        Jump notObject = branchIfNotObject(cellGPR);
        
        Jump notFunction = branchIfNotFunction(cellGPR);
        functor(TypeofType::Function, false);
        
        notFunction.link(this);
        slowPathFunctor(
            branchTest8(
                NonZero,
                Address(cellGPR, JSCell::typeInfoFlagsOffset()),
                TrustedImm32(MasqueradesAsUndefined | OverridesGetCallData)));
        functor(TypeofType::Object, false);
        
        notObject.link(this);
        
        Jump notString = branchIfNotString(cellGPR);
        functor(TypeofType::String, false);

        notString.link(this);

        Jump notHeapBigInt = branchIfNotHeapBigInt(cellGPR);
        functor(TypeofType::BigInt, false);

        notHeapBigInt.link(this);
        functor(TypeofType::Symbol, false);
        
        notCell.link(this);

        Jump notNumber = branchIfNotNumber(regs);
        functor(TypeofType::Number, false);
        notNumber.link(this);
        
        JumpList notNull = branchIfNotEqual(regs, jsNull());
        functor(TypeofType::Object, false);
        notNull.link(this);
        
        Jump notBoolean = branchIfNotBoolean(regs, tempGPR);
        functor(TypeofType::Boolean, false);
        notBoolean.link(this);

#if USE(BIGINT32)
        Jump notBigInt32 = branchIfNotBigInt32(regs, tempGPR);
        functor(TypeofType::BigInt, false);
        notBigInt32.link(this);
#endif
        
        functor(TypeofType::Undefined, true);
    }
    
    void emitVirtualCall(VM&, CallLinkInfo*);
    void emitVirtualCallWithoutMovingGlobalObject(VM&, GPRReg callLinkInfoGPR, CallMode);
    
    void makeSpaceOnStackForCCall();
    void reclaimSpaceOnStackForCCall();

    void emitRandomThunk(JSGlobalObject*, GPRReg scratch0, GPRReg scratch1, GPRReg scratch2, FPRReg result);
    void emitRandomThunk(VM&, GPRReg scratch0, GPRReg scratch1, GPRReg scratch2, GPRReg scratch3, FPRReg result);

    // Call this if you know that the value held in allocatorGPR is non-null. This DOES NOT mean
    // that allocator is non-null; allocator can be null as a signal that we don't know what the
    // value of allocatorGPR is. Additionally, if the allocator is not null, then there is no need
    // to populate allocatorGPR - this code will ignore the contents of allocatorGPR.
    enum class SlowAllocationResult : uint8_t {
        ClearToNull,
        UndefinedBehavior,
    };
    void emitAllocateWithNonNullAllocator(GPRReg resultGPR, const JITAllocator&, GPRReg allocatorGPR, GPRReg scratchGPR, JumpList& slowPath, SlowAllocationResult = SlowAllocationResult::ClearToNull);
    
    void emitAllocate(GPRReg resultGPR, const JITAllocator&, GPRReg allocatorGPR, GPRReg scratchGPR, JumpList& slowPath, SlowAllocationResult = SlowAllocationResult::ClearToNull);
    
    template<typename StructureType>
    void emitAllocateJSCell(GPRReg resultGPR, const JITAllocator& allocator, GPRReg allocatorGPR, StructureType structure, GPRReg scratchGPR, JumpList& slowPath, SlowAllocationResult slowAllocationResult = SlowAllocationResult::ClearToNull)
    {
        emitAllocate(resultGPR, allocator, allocatorGPR, scratchGPR, slowPath, slowAllocationResult);
        emitStoreStructureWithTypeInfo(structure, resultGPR, scratchGPR);
    }
    
    template<typename StructureType, typename StorageType>
    void emitAllocateJSObject(GPRReg resultGPR, const JITAllocator& allocator, GPRReg allocatorGPR, StructureType structure, StorageType storage, GPRReg scratchGPR, JumpList& slowPath, SlowAllocationResult slowAllocationResult = SlowAllocationResult::ClearToNull)
    {
        emitAllocateJSCell(resultGPR, allocator, allocatorGPR, structure, scratchGPR, slowPath, slowAllocationResult);
        storePtr(storage, Address(resultGPR, JSObject::butterflyOffset()));
    }
    
    template<typename ClassType, typename StructureType, typename StorageType>
    void emitAllocateJSObjectWithKnownSize(
        VM& vm, GPRReg resultGPR, StructureType structure, StorageType storage, GPRReg scratchGPR1,
        GPRReg scratchGPR2, JumpList& slowPath, size_t size, SlowAllocationResult slowAllocationResult = SlowAllocationResult::ClearToNull)
    {
        Allocator allocator = allocatorForConcurrently<ClassType>(vm, size, AllocatorForMode::AllocatorIfExists);
        emitAllocateJSObject(resultGPR, JITAllocator::constant(allocator), scratchGPR1, structure, storage, scratchGPR2, slowPath, slowAllocationResult);
    }
    
    template<typename ClassType, typename StructureType, typename StorageType>
    void emitAllocateJSObject(VM& vm, GPRReg resultGPR, StructureType structure, StorageType storage, GPRReg scratchGPR1, GPRReg scratchGPR2, JumpList& slowPath, SlowAllocationResult slowAllocationResult = SlowAllocationResult::ClearToNull)
    {
        emitAllocateJSObjectWithKnownSize<ClassType>(vm, resultGPR, structure, storage, scratchGPR1, scratchGPR2, slowPath, ClassType::allocationSize(0), slowAllocationResult);
    }
    
    // allocationSize can be aliased with any of the other input GPRs. If it's not aliased then it
    // won't be clobbered.
    void emitAllocateVariableSized(GPRReg resultGPR, const JITAllocator& allocator, Address subspaceAllocatorsBase, GPRReg allocationSize, GPRReg scratchGPR1, GPRReg scratchGPR2, JumpList& slowPath, SlowAllocationResult = SlowAllocationResult::ClearToNull);
    void emitAllocateVariableSized(GPRReg resultGPR, CompleteSubspace&, GPRReg allocationSize, GPRReg scratchGPR1, GPRReg scratchGPR2, JumpList& slowPath, SlowAllocationResult = SlowAllocationResult::ClearToNull);
    
    template<typename ClassType, typename StructureType>
    void emitAllocateVariableSizedCell(VM& vm, GPRReg resultGPR, StructureType structure, GPRReg allocationSize, GPRReg scratchGPR1, GPRReg scratchGPR2, JumpList& slowPath, SlowAllocationResult slowAllocationResult = SlowAllocationResult::ClearToNull)
    {
        CompleteSubspace* subspace = subspaceForConcurrently<ClassType>(vm);
        RELEASE_ASSERT_WITH_MESSAGE(subspace, "CompleteSubspace is always allocated");
        emitAllocateVariableSized(resultGPR, *subspace, allocationSize, scratchGPR1, scratchGPR2, slowPath, slowAllocationResult);
        emitStoreStructureWithTypeInfo(structure, resultGPR, scratchGPR2);
    }

    template<typename ClassType, typename StructureType>
    void emitAllocateVariableSizedJSObject(VM& vm, GPRReg resultGPR, StructureType structure, GPRReg allocationSize, GPRReg scratchGPR1, GPRReg scratchGPR2, JumpList& slowPath, SlowAllocationResult slowAllocationResult = SlowAllocationResult::ClearToNull)
    {
        emitAllocateVariableSizedCell<ClassType>(vm, resultGPR, structure, allocationSize, scratchGPR1, scratchGPR2, slowPath, slowAllocationResult);
        storePtr(TrustedImmPtr(nullptr), Address(resultGPR, JSObject::butterflyOffset()));
    }

    template<typename StructureType>
    void emitAllocateJSBigInt64(VM& vm, GPRReg resultGPR, GPRReg valueGPR, GPRReg scratchGPR1, GPRReg scratchGPR2, StructureType structure, bool isSigned, JumpList& slowCases)
    {
        // A zero value maps to the shared, immortal heapBigIntConstantZero held by the VM, so we
        // can avoid allocating (and taking the slow path) entirely for it.
        auto isZero = branchTest64(Zero, valueGPR);

        Allocator allocator = allocatorForConcurrently<JSBigInt>(vm, JSBigInt::allocationSize(1), AllocatorForMode::AllocatorIfExists);
        emitAllocateJSCell(resultGPR, JITAllocator::constant(allocator), scratchGPR1, structure, scratchGPR2, slowCases, SlowAllocationResult::UndefinedBehavior);

        store64(TrustedImm64(1), Address(resultGPR, JSBigInt::offsetOfLength()));

        if (isSigned) {
            neg64(valueGPR, scratchGPR1);
            moveConditionally64(LessThan, valueGPR, TrustedImm32(0), scratchGPR1, valueGPR, scratchGPR1);
            store64(scratchGPR1, Address(resultGPR, JSBigInt::offsetOfData()));

            load8(Address(resultGPR, JSCell::typeInfoFlagsOffset()), scratchGPR1);
            or32(TrustedImm32(TypeInfoPerCellBit), scratchGPR1, scratchGPR2);
            moveConditionally64(LessThan, valueGPR, TrustedImm32(0), scratchGPR2, scratchGPR1, scratchGPR1);
            store8(scratchGPR1, Address(resultGPR, JSCell::typeInfoFlagsOffset()));
        } else
            store64(valueGPR, Address(resultGPR, JSBigInt::offsetOfData()));

        mutatorFence(vm);
        auto done = jump();

        isZero.link(this);
        move(TrustedImmPtr(vm.heapBigIntConstantZero.get()), resultGPR);

        done.link(this);
    }

    enum LazyGlobalObjectLoadTag { LazyBaselineGlobalObject };
    JumpList branchIfValue(VM&, JSValueRegs, GPRReg scratch, GPRReg scratchIfShouldCheckMasqueradesAsUndefined, FPRReg, FPRReg, bool shouldCheckMasqueradesAsUndefined, Variant<JSGlobalObject*, GPRReg, LazyGlobalObjectLoadTag>, bool negateResult);
    JumpList branchIfTruthy(VM& vm, JSValueRegs value, GPRReg scratch, GPRReg scratchIfShouldCheckMasqueradesAsUndefined, FPRReg scratchFPR0, FPRReg scratchFPR1, bool shouldCheckMasqueradesAsUndefined, Variant<JSGlobalObject*, GPRReg, LazyGlobalObjectLoadTag> globalObject)
    {
        return branchIfValue(vm, value, scratch, scratchIfShouldCheckMasqueradesAsUndefined, scratchFPR0, scratchFPR1, shouldCheckMasqueradesAsUndefined, globalObject, false);
    }
    JumpList branchIfFalsey(VM& vm, JSValueRegs value, GPRReg scratch, GPRReg scratchIfShouldCheckMasqueradesAsUndefined, FPRReg scratchFPR0, FPRReg scratchFPR1, bool shouldCheckMasqueradesAsUndefined, Variant<JSGlobalObject*, GPRReg, LazyGlobalObjectLoadTag> globalObject)
    {
        return branchIfValue(vm, value, scratch, scratchIfShouldCheckMasqueradesAsUndefined, scratchFPR0, scratchFPR1, shouldCheckMasqueradesAsUndefined, globalObject, true);
    }
    void emitConvertValueToBoolean(VM&, JSValueRegs, GPRReg result, GPRReg scratchIfShouldCheckMasqueradesAsUndefined, FPRReg, FPRReg, bool shouldCheckMasqueradesAsUndefined, JSGlobalObject*, bool negateResult = false);
    
    void emitInitializeInlineStorage(GPRReg baseGPR, unsigned inlineCapacity, GPRReg scratchGPR)
    {
        ptrdiff_t initialOffset = JSObject::offsetOfInlineStorage();
        emitFillStorageWithJSEmpty(baseGPR, initialOffset, inlineCapacity, scratchGPR);
    }

    void emitInitializeInlineStorage(GPRReg baseGPR, GPRReg inlineCapacity)
    {
        Jump empty = branchTest32(Zero, inlineCapacity);
        Label loop = label();
        sub32(TrustedImm32(1), inlineCapacity);
        storeTrustedValue(JSValue(), BaseIndex(baseGPR, inlineCapacity, TimesEight, JSObject::offsetOfInlineStorage()));
        branchTest32(NonZero, inlineCapacity).linkTo(loop, this);
        empty.link(this);
    }

    void emitInitializeOutOfLineStorage(GPRReg butterflyGPR, unsigned outOfLineCapacity, GPRReg scratchGPR)
    {
        ptrdiff_t initialOffset = -sizeof(IndexingHeader) - outOfLineCapacity * sizeof(EncodedJSValue);
        emitFillStorageWithJSEmpty(butterflyGPR, initialOffset, outOfLineCapacity, scratchGPR);
    }

    void rapidHashMix64(GPRReg inputAndResult, GPRReg scratch1, GPRReg scratch2);

#if ENABLE(WEBASSEMBLY)
    void storeWasmContextInstance(GPRReg src);
#endif

    void emitFillStorageWithJSEmpty(GPRReg baseGPR, ptrdiff_t initialOffset, unsigned count, GPRReg scratchGPR)
    {
        if (!count)
            return;
        unsigned pairCount = count >> 1;
        unsigned pairIndex = 0;
        ASSERT(JSValue::encode(JSValue()) == 0);
#if CPU(ARM64)
        UNUSED_PARAM(scratchGPR);
        GPRReg emptyValueGPR = ARM64Registers::zr;
#else
        GPRReg emptyValueGPR = scratchGPR;
        move(TrustedImm32(0), scratchGPR);
#endif
        for (; pairIndex < pairCount; ++pairIndex)
            storePair64(emptyValueGPR, emptyValueGPR, baseGPR, TrustedImm32(initialOffset + pairIndex * 2 * sizeof(EncodedJSValue)));
        if (count & 1)
            store64(emptyValueGPR, Address(baseGPR, initialOffset + pairIndex * 2 * sizeof(EncodedJSValue)));
    }

    void emitFillStorageWithDoubleEmpty(GPRReg baseGPR, ptrdiff_t initialOffset, unsigned count, GPRReg scratchGPR)
    {
        unsigned pairCount = count >> 1;
        unsigned pairIndex = 0;
        move(TrustedImm64(std::bit_cast<int64_t>(PNaN)), scratchGPR);
        for (; pairIndex < pairCount; ++pairIndex)
            storePair64(scratchGPR, scratchGPR, baseGPR, TrustedImm32(initialOffset + pairIndex * 2 * sizeof(double)));
        if (count & 1)
            store64(scratchGPR, Address(baseGPR, initialOffset + pairIndex * 2 * sizeof(double)));
    }

#if ENABLE(WEBASSEMBLY)
#if CPU(ARM64) || CPU(X86_64) || CPU(RISCV64)
    JumpList checkWasmStackOverflow(GPRReg instanceGPR, TrustedImm32, GPRReg framePointerGPR);
#endif
#endif

protected:
    void copyCalleeSavesToEntryFrameCalleeSavesBufferImpl(GPRReg calleeSavesBuffer);

    enum class TypedArrayField { Length, ByteLength };
    std::tuple<Jump, JumpList> loadTypedArrayByteLengthImpl(GPRReg baseGPR, GPRReg valueGPR, GPRReg scratchGPR, GPRReg scratch2GPR, std::optional<TypedArrayType>, TypedArrayField);
    void loadTypedArrayByteLengthCommonImpl(GPRReg baseGPR, GPRReg valueGPR, GPRReg scratchGPR, GPRReg scratch2GPR, std::optional<TypedArrayType>, TypedArrayField);

    CodeBlock* const m_codeBlock;
    CodeBlock* const m_baselineCodeBlock;
};

} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif // ENABLE(JIT)
