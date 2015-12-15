/*
 * Copyright (C) 2011, 2013-2015 Apple Inc. All rights reserved.
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

#ifndef AssemblyHelpers_h
#define AssemblyHelpers_h

#if ENABLE(JIT)

#include "CodeBlock.h"
#include "CopyBarrier.h"
#include "FPRInfo.h"
#include "GPRInfo.h"
#include "InlineCallFrame.h"
#include "JITCode.h"
#include "MacroAssembler.h"
#include "MaxFrameExtentForSlowPathCall.h"
#include "RegisterAtOffsetList.h"
#include "RegisterSet.h"
#include "TypeofType.h"
#include "VM.h"

namespace JSC {

typedef void (*V_DebugOperation_EPP)(ExecState*, void*, void*);

class AssemblyHelpers : public MacroAssembler {
public:
    AssemblyHelpers(VM* vm, CodeBlock* codeBlock)
        : m_vm(vm)
        , m_codeBlock(codeBlock)
        , m_baselineCodeBlock(codeBlock ? codeBlock->baselineAlternative() : 0)
    {
        if (m_codeBlock) {
            ASSERT(m_baselineCodeBlock);
            ASSERT(!m_baselineCodeBlock->alternative());
            ASSERT(m_baselineCodeBlock->jitType() == JITCode::None || JITCode::isBaselineCode(m_baselineCodeBlock->jitType()));
        }
    }
    
    CodeBlock* codeBlock() { return m_codeBlock; }
    VM* vm() { return m_vm; }
    AssemblerType_T& assembler() { return m_assembler; }

    void checkStackPointerAlignment()
    {
        // This check is both unneeded and harder to write correctly for ARM64
#if !defined(NDEBUG) && !CPU(ARM64)
        Jump stackPointerAligned = branchTestPtr(Zero, stackPointerRegister, TrustedImm32(0xf));
        abortWithReason(AHStackPointerMisaligned);
        stackPointerAligned.link(this);
#endif
    }
    
    template<typename T>
    void storeCell(T cell, Address address)
    {
#if USE(JSVALUE64)
        store64(cell, address);
#else
        store32(cell, address.withOffset(PayloadOffset));
        store32(TrustedImm32(JSValue::CellTag), address.withOffset(TagOffset));
#endif
    }
    
    void storeValue(JSValueRegs regs, Address address)
    {
#if USE(JSVALUE64)
        store64(regs.gpr(), address);
#else
        store32(regs.payloadGPR(), address.withOffset(PayloadOffset));
        store32(regs.tagGPR(), address.withOffset(TagOffset));
#endif
    }
    
    void storeValue(JSValueRegs regs, BaseIndex address)
    {
#if USE(JSVALUE64)
        store64(regs.gpr(), address);
#else
        store32(regs.payloadGPR(), address.withOffset(PayloadOffset));
        store32(regs.tagGPR(), address.withOffset(TagOffset));
#endif
    }
    
    void storeValue(JSValueRegs regs, void* address)
    {
#if USE(JSVALUE64)
        store64(regs.gpr(), address);
#else
        store32(regs.payloadGPR(), bitwise_cast<void*>(bitwise_cast<uintptr_t>(address) + PayloadOffset));
        store32(regs.tagGPR(), bitwise_cast<void*>(bitwise_cast<uintptr_t>(address) + TagOffset));
#endif
    }
    
    void loadValue(Address address, JSValueRegs regs)
    {
#if USE(JSVALUE64)
        load64(address, regs.gpr());
#else
        if (address.base == regs.payloadGPR()) {
            load32(address.withOffset(TagOffset), regs.tagGPR());
            load32(address.withOffset(PayloadOffset), regs.payloadGPR());
        } else {
            load32(address.withOffset(PayloadOffset), regs.payloadGPR());
            load32(address.withOffset(TagOffset), regs.tagGPR());
        }
#endif
    }
    
    void loadValue(BaseIndex address, JSValueRegs regs)
    {
#if USE(JSVALUE64)
        load64(address, regs.gpr());
#else
        if (address.base == regs.payloadGPR() || address.index == regs.payloadGPR()) {
            // We actually could handle the case where the registers are aliased to both
            // tag and payload, but we don't for now.
            RELEASE_ASSERT(address.base != regs.tagGPR());
            RELEASE_ASSERT(address.index != regs.tagGPR());
            
            load32(address.withOffset(TagOffset), regs.tagGPR());
            load32(address.withOffset(PayloadOffset), regs.payloadGPR());
        } else {
            load32(address.withOffset(PayloadOffset), regs.payloadGPR());
            load32(address.withOffset(TagOffset), regs.tagGPR());
        }
#endif
    }

    void moveValueRegs(JSValueRegs srcRegs, JSValueRegs destRegs)
    {
#if USE(JSVALUE32_64)
        move(srcRegs.tagGPR(), destRegs.tagGPR());
#endif
        move(srcRegs.payloadGPR(), destRegs.payloadGPR());
    }

    void moveValue(JSValue value, JSValueRegs regs)
    {
#if USE(JSVALUE64)
        move(Imm64(JSValue::encode(value)), regs.gpr());
#else
        move(Imm32(value.tag()), regs.tagGPR());
        move(Imm32(value.payload()), regs.payloadGPR());
#endif
    }

    void moveTrustedValue(JSValue value, JSValueRegs regs)
    {
#if USE(JSVALUE64)
        move(TrustedImm64(JSValue::encode(value)), regs.gpr());
#else
        move(TrustedImm32(value.tag()), regs.tagGPR());
        move(TrustedImm32(value.payload()), regs.payloadGPR());
#endif
    }
    
    void storeTrustedValue(JSValue value, Address address)
    {
#if USE(JSVALUE64)
        store64(TrustedImm64(JSValue::encode(value)), address);
#else
        store32(TrustedImm32(value.tag()), address.withOffset(TagOffset));
        store32(TrustedImm32(value.payload()), address.withOffset(PayloadOffset));
#endif
    }

    void storeTrustedValue(JSValue value, BaseIndex address)
    {
#if USE(JSVALUE64)
        store64(TrustedImm64(JSValue::encode(value)), address);
#else
        store32(TrustedImm32(value.tag()), address.withOffset(TagOffset));
        store32(TrustedImm32(value.payload()), address.withOffset(PayloadOffset));
#endif
    }

    void emitSaveCalleeSavesFor(CodeBlock* codeBlock)
    {
        ASSERT(codeBlock);

        RegisterAtOffsetList* calleeSaves = codeBlock->calleeSaveRegisters();
        RegisterSet dontSaveRegisters = RegisterSet(RegisterSet::stackRegisters(), RegisterSet::allFPRs());
        unsigned registerCount = calleeSaves->size();

        for (unsigned i = 0; i < registerCount; i++) {
            RegisterAtOffset entry = calleeSaves->at(i);
            if (dontSaveRegisters.get(entry.reg()))
                continue;
            storePtr(entry.reg().gpr(), Address(framePointerRegister, entry.offset()));
        }
    }
    
    enum RestoreTagRegisterMode { UseExistingTagRegisterContents, CopySavedTagRegistersFromBaseFrame };

    void emitSaveOrCopyCalleeSavesFor(CodeBlock* codeBlock, VirtualRegister offsetVirtualRegister, RestoreTagRegisterMode tagRegisterMode, GPRReg temp)
    {
        ASSERT(codeBlock);
        
        RegisterAtOffsetList* calleeSaves = codeBlock->calleeSaveRegisters();
        RegisterSet dontSaveRegisters = RegisterSet(RegisterSet::stackRegisters(), RegisterSet::allFPRs());
        unsigned registerCount = calleeSaves->size();
        
        for (unsigned i = 0; i < registerCount; i++) {
            RegisterAtOffset entry = calleeSaves->at(i);
            if (dontSaveRegisters.get(entry.reg()))
                continue;
            
            GPRReg registerToWrite;
            
#if USE(JSVALUE32_64)
            UNUSED_PARAM(tagRegisterMode);
            UNUSED_PARAM(temp);
#else
            if (tagRegisterMode == CopySavedTagRegistersFromBaseFrame
                && (entry.reg() == GPRInfo::tagTypeNumberRegister || entry.reg() == GPRInfo::tagMaskRegister)) {
                registerToWrite = temp;
                loadPtr(AssemblyHelpers::Address(GPRInfo::callFrameRegister, entry.offset()), registerToWrite);
            } else
#endif
                registerToWrite = entry.reg().gpr();

            storePtr(registerToWrite, Address(framePointerRegister, offsetVirtualRegister.offsetInBytes() + entry.offset()));
        }
    }
    
    void emitRestoreCalleeSavesFor(CodeBlock* codeBlock)
    {
        ASSERT(codeBlock);

        RegisterAtOffsetList* calleeSaves = codeBlock->calleeSaveRegisters();
        RegisterSet dontRestoreRegisters = RegisterSet(RegisterSet::stackRegisters(), RegisterSet::allFPRs());
        unsigned registerCount = calleeSaves->size();
        
        for (unsigned i = 0; i < registerCount; i++) {
            RegisterAtOffset entry = calleeSaves->at(i);
            if (dontRestoreRegisters.get(entry.reg()))
                continue;
            loadPtr(Address(framePointerRegister, entry.offset()), entry.reg().gpr());
        }
    }

    void emitSaveCalleeSaves()
    {
        emitSaveCalleeSavesFor(codeBlock());
    }

    void emitRestoreCalleeSaves()
    {
        emitRestoreCalleeSavesFor(codeBlock());
    }

    void copyCalleeSavesToVMCalleeSavesBuffer(const TempRegisterSet& usedRegisters = { RegisterSet::stubUnavailableRegisters() })
    {
#if NUMBER_OF_CALLEE_SAVES_REGISTERS > 0
        GPRReg temp1 = usedRegisters.getFreeGPR(0);

        move(TrustedImmPtr(m_vm->calleeSaveRegistersBuffer), temp1);

        RegisterAtOffsetList* allCalleeSaves = m_vm->getAllCalleeSaveRegisterOffsets();
        RegisterSet dontCopyRegisters = RegisterSet::stackRegisters();
        unsigned registerCount = allCalleeSaves->size();
        
        for (unsigned i = 0; i < registerCount; i++) {
            RegisterAtOffset entry = allCalleeSaves->at(i);
            if (dontCopyRegisters.get(entry.reg()))
                continue;
            if (entry.reg().isGPR())
                storePtr(entry.reg().gpr(), Address(temp1, entry.offset()));
            else
                storeDouble(entry.reg().fpr(), Address(temp1, entry.offset()));
        }
#else
        UNUSED_PARAM(usedRegisters);
#endif
    }

    void restoreCalleeSavesFromVMCalleeSavesBuffer(const TempRegisterSet& usedRegisters = { RegisterSet::stubUnavailableRegisters() })
    {
#if NUMBER_OF_CALLEE_SAVES_REGISTERS > 0
        GPRReg temp1 = usedRegisters.getFreeGPR(0);
        
        move(TrustedImmPtr(m_vm->calleeSaveRegistersBuffer), temp1);

        RegisterAtOffsetList* allCalleeSaves = m_vm->getAllCalleeSaveRegisterOffsets();
        RegisterSet dontRestoreRegisters = RegisterSet::stackRegisters();
        unsigned registerCount = allCalleeSaves->size();
        
        for (unsigned i = 0; i < registerCount; i++) {
            RegisterAtOffset entry = allCalleeSaves->at(i);
            if (dontRestoreRegisters.get(entry.reg()))
                continue;
            if (entry.reg().isGPR())
                loadPtr(Address(temp1, entry.offset()), entry.reg().gpr());
            else
                loadDouble(Address(temp1, entry.offset()), entry.reg().fpr());
        }
#else
        UNUSED_PARAM(usedRegisters);
#endif
    }
    
    void copyCalleeSavesFromFrameOrRegisterToVMCalleeSavesBuffer(const TempRegisterSet& usedRegisters = { RegisterSet::stubUnavailableRegisters() })
    {
#if NUMBER_OF_CALLEE_SAVES_REGISTERS > 0
        GPRReg temp1 = usedRegisters.getFreeGPR(0);
        GPRReg temp2 = usedRegisters.getFreeGPR(1);
        FPRReg fpTemp = usedRegisters.getFreeFPR();
        ASSERT(temp2 != InvalidGPRReg);

        ASSERT(codeBlock());

        // Copy saved calleeSaves on stack or unsaved calleeSaves in register to vm calleeSave buffer
        move(TrustedImmPtr(m_vm->calleeSaveRegistersBuffer), temp1);

        RegisterAtOffsetList* allCalleeSaves = m_vm->getAllCalleeSaveRegisterOffsets();
        RegisterAtOffsetList* currentCalleeSaves = codeBlock()->calleeSaveRegisters();
        RegisterSet dontCopyRegisters = RegisterSet::stackRegisters();
        unsigned registerCount = allCalleeSaves->size();

        for (unsigned i = 0; i < registerCount; i++) {
            RegisterAtOffset vmEntry = allCalleeSaves->at(i);
            if (dontCopyRegisters.get(vmEntry.reg()))
                continue;
            RegisterAtOffset* currentFrameEntry = currentCalleeSaves->find(vmEntry.reg());

            if (vmEntry.reg().isGPR()) {
                GPRReg regToStore;
                if (currentFrameEntry) {
                    // Load calleeSave from stack into temp register
                    regToStore = temp2;
                    loadPtr(Address(framePointerRegister, currentFrameEntry->offset()), regToStore);
                } else
                    // Just store callee save directly
                    regToStore = vmEntry.reg().gpr();

                storePtr(regToStore, Address(temp1, vmEntry.offset()));
            } else {
                FPRReg fpRegToStore;
                if (currentFrameEntry) {
                    // Load calleeSave from stack into temp register
                    fpRegToStore = fpTemp;
                    loadDouble(Address(framePointerRegister, currentFrameEntry->offset()), fpRegToStore);
                } else
                    // Just store callee save directly
                    fpRegToStore = vmEntry.reg().fpr();

                storeDouble(fpRegToStore, Address(temp1, vmEntry.offset()));
            }
        }
#else
        UNUSED_PARAM(usedRegisters);
#endif
    }

    void emitMaterializeTagCheckRegisters()
    {
#if USE(JSVALUE64)
        move(MacroAssembler::TrustedImm64(TagTypeNumber), GPRInfo::tagTypeNumberRegister);
        orPtr(MacroAssembler::TrustedImm32(TagBitTypeOther), GPRInfo::tagTypeNumberRegister, GPRInfo::tagMaskRegister);
#endif
    }

#if CPU(X86_64) || CPU(X86)
    static size_t prologueStackPointerDelta()
    {
        // Prologue only saves the framePointerRegister
        return sizeof(void*);
    }

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
#endif // CPU(X86_64) || CPU(X86)

#if CPU(ARM) || CPU(ARM64)
    static size_t prologueStackPointerDelta()
    {
        // Prologue saves the framePointerRegister and linkRegister
        return 2 * sizeof(void*);
    }

    void emitFunctionPrologue()
    {
        pushPair(framePointerRegister, linkRegister);
        move(stackPointerRegister, framePointerRegister);
    }

    void emitFunctionEpilogue()
    {
        move(framePointerRegister, stackPointerRegister);
        popPair(framePointerRegister, linkRegister);
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

#if CPU(MIPS)
    static size_t prologueStackPointerDelta()
    {
        // Prologue saves the framePointerRegister and returnAddressRegister
        return 2 * sizeof(void*);
    }

    ALWAYS_INLINE void preserveReturnAddressAfterCall(RegisterID reg)
    {
        move(returnAddressRegister, reg);
    }

    ALWAYS_INLINE void restoreReturnAddressBeforeReturn(RegisterID reg)
    {
        move(reg, returnAddressRegister);
    }

    ALWAYS_INLINE void restoreReturnAddressBeforeReturn(Address address)
    {
        loadPtr(address, returnAddressRegister);
    }
#endif

#if CPU(SH4)
    static size_t prologueStackPointerDelta()
    {
        // Prologue saves the framePointerRegister and link register
        return 2 * sizeof(void*);
    }

    void emitFunctionPrologue()
    {
        push(linkRegister);
        push(framePointerRegister);
        move(stackPointerRegister, framePointerRegister);
    }

    void emitFunctionEpilogue()
    {
        move(framePointerRegister, stackPointerRegister);
        pop(framePointerRegister);
        pop(linkRegister);
    }

    ALWAYS_INLINE void preserveReturnAddressAfterCall(RegisterID reg)
    {
        m_assembler.stspr(reg);
    }

    ALWAYS_INLINE void restoreReturnAddressBeforeReturn(RegisterID reg)
    {
        m_assembler.ldspr(reg);
    }

    ALWAYS_INLINE void restoreReturnAddressBeforeReturn(Address address)
    {
        loadPtrLinkReg(address);
    }
#endif

    void emitGetFromCallFrameHeaderPtr(JSStack::CallFrameHeaderEntry entry, GPRReg to, GPRReg from = GPRInfo::callFrameRegister)
    {
        loadPtr(Address(from, entry * sizeof(Register)), to);
    }
    void emitGetFromCallFrameHeader32(JSStack::CallFrameHeaderEntry entry, GPRReg to, GPRReg from = GPRInfo::callFrameRegister)
    {
        load32(Address(from, entry * sizeof(Register)), to);
    }
#if USE(JSVALUE64)
    void emitGetFromCallFrameHeader64(JSStack::CallFrameHeaderEntry entry, GPRReg to, GPRReg from = GPRInfo::callFrameRegister)
    {
        load64(Address(from, entry * sizeof(Register)), to);
    }
#endif // USE(JSVALUE64)
    void emitPutToCallFrameHeader(GPRReg from, JSStack::CallFrameHeaderEntry entry)
    {
        storePtr(from, Address(GPRInfo::callFrameRegister, entry * sizeof(Register)));
    }

    void emitPutToCallFrameHeader(void* value, JSStack::CallFrameHeaderEntry entry)
    {
        storePtr(TrustedImmPtr(value), Address(GPRInfo::callFrameRegister, entry * sizeof(Register)));
    }

    void emitGetCallerFrameFromCallFrameHeaderPtr(RegisterID to)
    {
        loadPtr(Address(GPRInfo::callFrameRegister, CallFrame::callerFrameOffset()), to);
    }
    void emitPutCallerFrameToCallFrameHeader(RegisterID from)
    {
        storePtr(from, Address(GPRInfo::callFrameRegister, CallFrame::callerFrameOffset()));
    }

    void emitPutReturnPCToCallFrameHeader(RegisterID from)
    {
        storePtr(from, Address(GPRInfo::callFrameRegister, CallFrame::returnPCOffset()));
    }
    void emitPutReturnPCToCallFrameHeader(TrustedImmPtr from)
    {
        storePtr(from, Address(GPRInfo::callFrameRegister, CallFrame::returnPCOffset()));
    }

    // emitPutToCallFrameHeaderBeforePrologue() and related are used to access callee frame header
    // fields before the code from emitFunctionPrologue() has executed.
    // First, the access is via the stack pointer. Second, the address calculation must also take
    // into account that the stack pointer may not have been adjusted down for the return PC and/or
    // caller's frame pointer. On some platforms, the callee is responsible for pushing the
    // "link register" containing the return address in the function prologue.
#if USE(JSVALUE64)
    void emitPutToCallFrameHeaderBeforePrologue(GPRReg from, JSStack::CallFrameHeaderEntry entry)
    {
        storePtr(from, Address(stackPointerRegister, entry * static_cast<ptrdiff_t>(sizeof(Register)) - prologueStackPointerDelta()));
    }
#else
    void emitPutPayloadToCallFrameHeaderBeforePrologue(GPRReg from, JSStack::CallFrameHeaderEntry entry)
    {
        storePtr(from, Address(stackPointerRegister, entry * static_cast<ptrdiff_t>(sizeof(Register)) - prologueStackPointerDelta() + OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.payload)));
    }

    void emitPutTagToCallFrameHeaderBeforePrologue(TrustedImm32 tag, JSStack::CallFrameHeaderEntry entry)
    {
        storePtr(tag, Address(stackPointerRegister, entry * static_cast<ptrdiff_t>(sizeof(Register)) - prologueStackPointerDelta() + OBJECT_OFFSETOF(EncodedValueDescriptor, asBits.tag)));
    }
#endif
    
    JumpList branchIfNotEqual(JSValueRegs regs, JSValue value)
    {
#if USE(JSVALUE64)
        return branch64(NotEqual, regs.gpr(), TrustedImm64(JSValue::encode(value)));
#else
        JumpList result;
        result.append(branch32(NotEqual, regs.tagGPR(), TrustedImm32(value.tag())));
        if (value.isEmpty() || value.isUndefinedOrNull())
            return result; // These don't have anything interesting in the payload.
        result.append(branch32(NotEqual, regs.payloadGPR(), TrustedImm32(value.payload())));
        return result;
#endif
    }
    
    Jump branchIfEqual(JSValueRegs regs, JSValue value)
    {
#if USE(JSVALUE64)
        return branch64(Equal, regs.gpr(), TrustedImm64(JSValue::encode(value)));
#else
        Jump notEqual;
        // These don't have anything interesting in the payload.
        if (!value.isEmpty() && !value.isUndefinedOrNull())
            notEqual = branch32(NotEqual, regs.payloadGPR(), TrustedImm32(value.payload()));
        Jump result = branch32(Equal, regs.tagGPR(), TrustedImm32(value.tag()));
        if (notEqual.isSet())
            notEqual.link(this);
        return result;
#endif
    }

    enum TagRegistersMode {
        DoNotHaveTagRegisters,
        HaveTagRegisters
    };

    Jump branchIfNotCell(GPRReg reg, TagRegistersMode mode = HaveTagRegisters)
    {
#if USE(JSVALUE64)
        if (mode == HaveTagRegisters)
            return branchTest64(NonZero, reg, GPRInfo::tagMaskRegister);
        return branchTest64(NonZero, reg, TrustedImm64(TagMask));
#else
        UNUSED_PARAM(mode);
        return branch32(MacroAssembler::NotEqual, reg, TrustedImm32(JSValue::CellTag));
#endif
    }
    Jump branchIfNotCell(JSValueRegs regs, TagRegistersMode mode = HaveTagRegisters)
    {
#if USE(JSVALUE64)
        return branchIfNotCell(regs.gpr(), mode);
#else
        return branchIfNotCell(regs.tagGPR(), mode);
#endif
    }
    
    Jump branchIfCell(GPRReg reg, TagRegistersMode mode = HaveTagRegisters)
    {
#if USE(JSVALUE64)
        if (mode == HaveTagRegisters)
            return branchTest64(Zero, reg, GPRInfo::tagMaskRegister);
        return branchTest64(Zero, reg, TrustedImm64(TagMask));
#else
        UNUSED_PARAM(mode);
        return branch32(MacroAssembler::Equal, reg, TrustedImm32(JSValue::CellTag));
#endif
    }
    Jump branchIfCell(JSValueRegs regs, TagRegistersMode mode = HaveTagRegisters)
    {
#if USE(JSVALUE64)
        return branchIfCell(regs.gpr(), mode);
#else
        return branchIfCell(regs.tagGPR(), mode);
#endif
    }
    
    Jump branchIfOther(JSValueRegs regs, GPRReg tempGPR)
    {
#if USE(JSVALUE64)
        move(regs.gpr(), tempGPR);
        and64(TrustedImm32(~TagBitUndefined), tempGPR);
        return branch64(Equal, tempGPR, TrustedImm64(ValueNull));
#else
        or32(TrustedImm32(1), regs.tagGPR(), tempGPR);
        return branch32(Equal, tempGPR, TrustedImm32(JSValue::NullTag));
#endif
    }
    
    Jump branchIfNotOther(JSValueRegs regs, GPRReg tempGPR)
    {
#if USE(JSVALUE64)
        move(regs.gpr(), tempGPR);
        and64(TrustedImm32(~TagBitUndefined), tempGPR);
        return branch64(NotEqual, tempGPR, TrustedImm64(ValueNull));
#else
        or32(TrustedImm32(1), regs.tagGPR(), tempGPR);
        return branch32(NotEqual, tempGPR, TrustedImm32(JSValue::NullTag));
#endif
    }
    
    Jump branchIfInt32(JSValueRegs regs, TagRegistersMode mode = HaveTagRegisters)
    {
#if USE(JSVALUE64)
        if (mode == HaveTagRegisters)
            return branch64(AboveOrEqual, regs.gpr(), GPRInfo::tagTypeNumberRegister);
        return branch64(AboveOrEqual, regs.gpr(), TrustedImm64(TagTypeNumber));
#else
        UNUSED_PARAM(mode);
        return branch32(Equal, regs.tagGPR(), TrustedImm32(JSValue::Int32Tag));
#endif
    }

#if USE(JSVALUE64)
    Jump branchIfNotInt32(GPRReg gpr, TagRegistersMode mode = HaveTagRegisters)
    {
        if (mode == HaveTagRegisters)
            return branch64(Below, gpr, GPRInfo::tagTypeNumberRegister);
        return branch64(Below, gpr, TrustedImm64(TagTypeNumber));
    }
#endif

    Jump branchIfNotInt32(JSValueRegs regs, TagRegistersMode mode = HaveTagRegisters)
    {
#if USE(JSVALUE64)
        return branchIfNotInt32(regs.gpr(), mode);
#else
        UNUSED_PARAM(mode);
        return branch32(NotEqual, regs.tagGPR(), TrustedImm32(JSValue::Int32Tag));
#endif
    }

    // Note that the tempGPR is not used in 64-bit mode.
    Jump branchIfNumber(JSValueRegs regs, GPRReg tempGPR, TagRegistersMode mode = HaveTagRegisters)
    {
#if USE(JSVALUE64)
        UNUSED_PARAM(tempGPR);
        if (mode == HaveTagRegisters)
            return branchTest64(NonZero, regs.gpr(), GPRInfo::tagTypeNumberRegister);
        return branchTest64(NonZero, regs.gpr(), TrustedImm64(TagTypeNumber));
#else
        UNUSED_PARAM(mode);
        add32(TrustedImm32(1), regs.tagGPR(), tempGPR);
        return branch32(Below, tempGPR, TrustedImm32(JSValue::LowestTag + 1));
#endif
    }
    
    // Note that the tempGPR is not used in 64-bit mode.
    Jump branchIfNotNumber(JSValueRegs regs, GPRReg tempGPR, TagRegistersMode mode = HaveTagRegisters)
    {
#if USE(JSVALUE64)
        UNUSED_PARAM(tempGPR);
        if (mode == HaveTagRegisters)
            return branchTest64(Zero, regs.gpr(), GPRInfo::tagTypeNumberRegister);
        return branchTest64(Zero, regs.gpr(), TrustedImm64(TagTypeNumber));
#else
        UNUSED_PARAM(mode);
        add32(TrustedImm32(1), regs.tagGPR(), tempGPR);
        return branch32(AboveOrEqual, tempGPR, TrustedImm32(JSValue::LowestTag + 1));
#endif
    }

    // Note that the tempGPR is not used in 32-bit mode.
    Jump branchIfBoolean(JSValueRegs regs, GPRReg tempGPR)
    {
#if USE(JSVALUE64)
        move(regs.gpr(), tempGPR);
        xor64(TrustedImm32(static_cast<int32_t>(ValueFalse)), tempGPR);
        return branchTest64(Zero, tempGPR, TrustedImm32(static_cast<int32_t>(~1)));
#else
        UNUSED_PARAM(tempGPR);
        return branch32(Equal, regs.tagGPR(), TrustedImm32(JSValue::BooleanTag));
#endif
    }
    
    // Note that the tempGPR is not used in 32-bit mode.
    Jump branchIfNotBoolean(JSValueRegs regs, GPRReg tempGPR)
    {
#if USE(JSVALUE64)
        move(regs.gpr(), tempGPR);
        xor64(TrustedImm32(static_cast<int32_t>(ValueFalse)), tempGPR);
        return branchTest64(NonZero, tempGPR, TrustedImm32(static_cast<int32_t>(~1)));
#else
        UNUSED_PARAM(tempGPR);
        return branch32(NotEqual, regs.tagGPR(), TrustedImm32(JSValue::BooleanTag));
#endif
    }
    
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
    
    Jump branchIfType(GPRReg cellGPR, JSType type)
    {
        return branch8(Equal, Address(cellGPR, JSCell::typeInfoTypeOffset()), TrustedImm32(type));
    }
    
    Jump branchIfNotType(GPRReg cellGPR, JSType type)
    {
        return branch8(NotEqual, Address(cellGPR, JSCell::typeInfoTypeOffset()), TrustedImm32(type));
    }
    
    Jump branchIfString(GPRReg cellGPR) { return branchIfType(cellGPR, StringType); }
    Jump branchIfNotString(GPRReg cellGPR) { return branchIfNotType(cellGPR, StringType); }
    Jump branchIfSymbol(GPRReg cellGPR) { return branchIfType(cellGPR, SymbolType); }
    Jump branchIfNotSymbol(GPRReg cellGPR) { return branchIfNotType(cellGPR, SymbolType); }
    Jump branchIfFunction(GPRReg cellGPR) { return branchIfType(cellGPR, JSFunctionType); }
    Jump branchIfNotFunction(GPRReg cellGPR) { return branchIfNotType(cellGPR, JSFunctionType); }
    
    Jump branchIfEmpty(JSValueRegs regs)
    {
#if USE(JSVALUE64)
        return branchTest64(Zero, regs.gpr());
#else
        return branch32(Equal, regs.tagGPR(), TrustedImm32(JSValue::EmptyValueTag));
#endif
    }

    JumpList branchIfNotType(
        JSValueRegs, GPRReg tempGPR, const InferredType::Descriptor&, TagRegistersMode);

    template<typename T>
    Jump branchStructure(RelationalCondition condition, T leftHandSide, Structure* structure)
    {
#if USE(JSVALUE64)
        return branch32(condition, leftHandSide, TrustedImm32(structure->id()));
#else
        return branchPtr(condition, leftHandSide, TrustedImmPtr(structure));
#endif
    }

    Jump branchIfToSpace(GPRReg storageGPR)
    {
        return branchTest32(Zero, storageGPR, TrustedImm32(CopyBarrierBase::spaceBits));
    }

    Jump branchIfNotToSpace(GPRReg storageGPR)
    {
        return branchTest32(NonZero, storageGPR, TrustedImm32(CopyBarrierBase::spaceBits));
    }

    void removeSpaceBits(GPRReg storageGPR)
    {
        andPtr(TrustedImmPtr(~static_cast<uintptr_t>(CopyBarrierBase::spaceBits)), storageGPR);
    }

    Jump branchIfFastTypedArray(GPRReg baseGPR);
    Jump branchIfNotFastTypedArray(GPRReg baseGPR);

    // Returns a jump to slow path for when we need to execute the barrier. Note that baseGPR and
    // resultGPR must be different.
    Jump loadTypedArrayVector(GPRReg baseGPR, GPRReg resultGPR);
    
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
    static Address addressFor(int operand)
    {
        return addressFor(static_cast<VirtualRegister>(operand));
    }

    static Address tagFor(VirtualRegister virtualRegister)
    {
        ASSERT(virtualRegister.isValid());
        return Address(GPRInfo::callFrameRegister, virtualRegister.offset() * sizeof(Register) + TagOffset);
    }
    static Address tagFor(int operand)
    {
        return tagFor(static_cast<VirtualRegister>(operand));
    }

    static Address payloadFor(VirtualRegister virtualRegister)
    {
        ASSERT(virtualRegister.isValid());
        return Address(GPRInfo::callFrameRegister, virtualRegister.offset() * sizeof(Register) + PayloadOffset);
    }
    static Address payloadFor(int operand)
    {
        return payloadFor(static_cast<VirtualRegister>(operand));
    }

    // Access to our fixed callee CallFrame.
    static Address calleeFrameSlot(int slot)
    {
        ASSERT(slot >= JSStack::CallerFrameAndPCSize);
        return Address(stackPointerRegister, sizeof(Register) * (slot - JSStack::CallerFrameAndPCSize));
    }

    // Access to our fixed callee CallFrame.
    static Address calleeArgumentSlot(int argument)
    {
        return calleeFrameSlot(virtualRegisterForArgument(argument).offset());
    }

    static Address calleeFrameTagSlot(int slot)
    {
        return calleeFrameSlot(slot).withOffset(TagOffset);
    }

    static Address calleeFramePayloadSlot(int slot)
    {
        return calleeFrameSlot(slot).withOffset(PayloadOffset);
    }

    static Address calleeArgumentTagSlot(int argument)
    {
        return calleeArgumentSlot(argument).withOffset(TagOffset);
    }

    static Address calleeArgumentPayloadSlot(int argument)
    {
        return calleeArgumentSlot(argument).withOffset(PayloadOffset);
    }

    static Address calleeFrameCallerFrame()
    {
        return calleeFrameSlot(0).withOffset(CallFrame::callerFrameOffset());
    }

    static GPRReg selectScratchGPR(GPRReg preserve1 = InvalidGPRReg, GPRReg preserve2 = InvalidGPRReg, GPRReg preserve3 = InvalidGPRReg, GPRReg preserve4 = InvalidGPRReg, GPRReg preserve5 = InvalidGPRReg)
    {
        if (preserve1 != GPRInfo::regT0 && preserve2 != GPRInfo::regT0 && preserve3 != GPRInfo::regT0 && preserve4 != GPRInfo::regT0 && preserve5 != GPRInfo::regT0)
            return GPRInfo::regT0;

        if (preserve1 != GPRInfo::regT1 && preserve2 != GPRInfo::regT1 && preserve3 != GPRInfo::regT1 && preserve4 != GPRInfo::regT1 && preserve5 != GPRInfo::regT1)
            return GPRInfo::regT1;

        if (preserve1 != GPRInfo::regT2 && preserve2 != GPRInfo::regT2 && preserve3 != GPRInfo::regT2 && preserve4 != GPRInfo::regT2 && preserve5 != GPRInfo::regT2)
            return GPRInfo::regT2;

        if (preserve1 != GPRInfo::regT3 && preserve2 != GPRInfo::regT3 && preserve3 != GPRInfo::regT3 && preserve4 != GPRInfo::regT3 && preserve5 != GPRInfo::regT3)
            return GPRInfo::regT3;

        if (preserve1 != GPRInfo::regT4 && preserve2 != GPRInfo::regT4 && preserve3 != GPRInfo::regT4 && preserve4 != GPRInfo::regT4 && preserve5 != GPRInfo::regT4)
            return GPRInfo::regT4;

        return GPRInfo::regT5;
    }

    // Add a debug call. This call has no effect on JIT code execution state.
    void debugCall(V_DebugOperation_EPP function, void* argument)
    {
        size_t scratchSize = sizeof(EncodedJSValue) * (GPRInfo::numberOfRegisters + FPRInfo::numberOfRegisters);
        ScratchBuffer* scratchBuffer = m_vm->scratchBufferForSize(scratchSize);
        EncodedJSValue* buffer = static_cast<EncodedJSValue*>(scratchBuffer->dataBuffer());

        for (unsigned i = 0; i < GPRInfo::numberOfRegisters; ++i) {
#if USE(JSVALUE64)
            store64(GPRInfo::toRegister(i), buffer + i);
#else
            store32(GPRInfo::toRegister(i), buffer + i);
#endif
        }

        for (unsigned i = 0; i < FPRInfo::numberOfRegisters; ++i) {
            move(TrustedImmPtr(buffer + GPRInfo::numberOfRegisters + i), GPRInfo::regT0);
            storeDouble(FPRInfo::toRegister(i), GPRInfo::regT0);
        }

        // Tell GC mark phase how much of the scratch buffer is active during call.
        move(TrustedImmPtr(scratchBuffer->activeLengthPtr()), GPRInfo::regT0);
        storePtr(TrustedImmPtr(scratchSize), GPRInfo::regT0);

#if CPU(X86_64) || CPU(ARM) || CPU(ARM64) || CPU(MIPS) || CPU(SH4)
        move(TrustedImmPtr(buffer), GPRInfo::argumentGPR2);
        move(TrustedImmPtr(argument), GPRInfo::argumentGPR1);
        move(GPRInfo::callFrameRegister, GPRInfo::argumentGPR0);
        GPRReg scratch = selectScratchGPR(GPRInfo::argumentGPR0, GPRInfo::argumentGPR1, GPRInfo::argumentGPR2);
#elif CPU(X86)
        poke(GPRInfo::callFrameRegister, 0);
        poke(TrustedImmPtr(argument), 1);
        poke(TrustedImmPtr(buffer), 2);
        GPRReg scratch = GPRInfo::regT0;
#else
#error "JIT not supported on this platform."
#endif
        move(TrustedImmPtr(reinterpret_cast<void*>(function)), scratch);
        call(scratch);

        move(TrustedImmPtr(scratchBuffer->activeLengthPtr()), GPRInfo::regT0);
        storePtr(TrustedImmPtr(0), GPRInfo::regT0);

        for (unsigned i = 0; i < FPRInfo::numberOfRegisters; ++i) {
            move(TrustedImmPtr(buffer + GPRInfo::numberOfRegisters + i), GPRInfo::regT0);
            loadDouble(GPRInfo::regT0, FPRInfo::toRegister(i));
        }
        for (unsigned i = 0; i < GPRInfo::numberOfRegisters; ++i) {
#if USE(JSVALUE64)
            load64(buffer + i, GPRInfo::toRegister(i));
#else
            load32(buffer + i, GPRInfo::toRegister(i));
#endif
        }
    }

    // These methods JIT generate dynamic, debug-only checks - akin to ASSERTs.
#if !ASSERT_DISABLED
    void jitAssertIsInt32(GPRReg);
    void jitAssertIsJSInt32(GPRReg);
    void jitAssertIsJSNumber(GPRReg);
    void jitAssertIsJSDouble(GPRReg);
    void jitAssertIsCell(GPRReg);
    void jitAssertHasValidCallFrame();
    void jitAssertIsNull(GPRReg);
    void jitAssertTagsInPlace();
    void jitAssertArgumentCountSane();
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
#endif

    void jitReleaseAssertNoException();
    
    void purifyNaN(FPRReg);

    // These methods convert between doubles, and doubles boxed and JSValues.
#if USE(JSVALUE64)
    GPRReg boxDouble(FPRReg fpr, GPRReg gpr)
    {
        moveDoubleTo64(fpr, gpr);
        sub64(GPRInfo::tagTypeNumberRegister, gpr);
        jitAssertIsJSDouble(gpr);
        return gpr;
    }
    FPRReg unboxDoubleWithoutAssertions(GPRReg gpr, FPRReg fpr)
    {
        add64(GPRInfo::tagTypeNumberRegister, gpr);
        move64ToDouble(gpr, fpr);
        return fpr;
    }
    FPRReg unboxDouble(GPRReg gpr, FPRReg fpr)
    {
        jitAssertIsJSDouble(gpr);
        return unboxDoubleWithoutAssertions(gpr, fpr);
    }
    
    void boxDouble(FPRReg fpr, JSValueRegs regs)
    {
        boxDouble(fpr, regs.gpr());
    }

    void unboxDoubleNonDestructive(JSValueRegs regs, FPRReg destFPR, GPRReg scratchGPR, FPRReg)
    {
        move(regs.payloadGPR(), scratchGPR);
        unboxDouble(scratchGPR, destFPR);
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
        zeroExtend32ToPtr(source, target);
        or64(GPRInfo::tagTypeNumberRegister, target);
        
        done.link(this);
    }
#endif

#if USE(JSVALUE32_64)
    void boxDouble(FPRReg fpr, GPRReg tagGPR, GPRReg payloadGPR)
    {
        moveDoubleToInts(fpr, payloadGPR, tagGPR);
    }
    void unboxDouble(GPRReg tagGPR, GPRReg payloadGPR, FPRReg fpr, FPRReg scratchFPR)
    {
        moveIntsToDouble(payloadGPR, tagGPR, fpr, scratchFPR);
    }
    
    void boxDouble(FPRReg fpr, JSValueRegs regs)
    {
        boxDouble(fpr, regs.tagGPR(), regs.payloadGPR());
    }
    void unboxDouble(JSValueRegs regs, FPRReg fpr, FPRReg scratchFPR)
    {
        unboxDouble(regs.tagGPR(), regs.payloadGPR(), fpr, scratchFPR);
    }

    void unboxDoubleNonDestructive(const JSValueRegs regs, FPRReg destFPR, GPRReg, FPRReg scratchFPR)
    {
        unboxDouble(regs, destFPR, scratchFPR);
    }
#endif
    
    void boxBooleanPayload(GPRReg boolGPR, GPRReg payloadGPR)
    {
#if USE(JSVALUE64)
        add32(TrustedImm32(ValueFalse), boolGPR, payloadGPR);
#else
        move(boolGPR, payloadGPR);
#endif
    }

    void boxBooleanPayload(bool value, GPRReg payloadGPR)
    {
#if USE(JSVALUE64)
        move(TrustedImm32(ValueFalse + value), payloadGPR);
#else
        move(TrustedImm32(value), payloadGPR);
#endif
    }

    void boxBoolean(GPRReg boolGPR, JSValueRegs boxedRegs)
    {
        boxBooleanPayload(boolGPR, boxedRegs.payloadGPR());
#if USE(JSVALUE32_64)
        move(TrustedImm32(JSValue::BooleanTag), boxedRegs.tagGPR());
#endif
    }

    void boxInt32(GPRReg intGPR, JSValueRegs boxedRegs, TagRegistersMode mode = HaveTagRegisters)
    {
#if USE(JSVALUE64)
        if (mode == DoNotHaveTagRegisters) {
            move(intGPR, boxedRegs.gpr());
            or64(TrustedImm64(TagTypeNumber), boxedRegs.gpr());
        } else
            or64(GPRInfo::tagTypeNumberRegister, intGPR, boxedRegs.gpr());
#else
        UNUSED_PARAM(mode);
        move(intGPR, boxedRegs.payloadGPR());
        move(TrustedImm32(JSValue::Int32Tag), boxedRegs.tagGPR());
#endif
    }
    
    void callExceptionFuzz();
    
    enum ExceptionCheckKind { NormalExceptionCheck, InvertedExceptionCheck };
    enum ExceptionJumpWidth { NormalJumpWidth, FarJumpWidth };
    Jump emitExceptionCheck(
        ExceptionCheckKind = NormalExceptionCheck, ExceptionJumpWidth = NormalJumpWidth);
    Jump emitNonPatchableExceptionCheck();

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

    JSGlobalObject* globalObjectFor(CodeOrigin codeOrigin)
    {
        return codeBlock()->globalObjectFor(codeOrigin);
    }
    
    bool isStrictModeFor(CodeOrigin codeOrigin)
    {
        if (!codeOrigin.inlineCallFrame)
            return codeBlock()->isStrictMode();
        return codeOrigin.inlineCallFrame->isStrictMode();
    }
    
    ECMAMode ecmaModeFor(CodeOrigin codeOrigin)
    {
        return isStrictModeFor(codeOrigin) ? StrictMode : NotStrictMode;
    }
    
    ExecutableBase* executableFor(const CodeOrigin& codeOrigin);
    
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
        if (inlineCallFrame->arguments.size() <= 1)
            return virtualRegisterForLocal(0);
        ValueRecovery recovery = inlineCallFrame->arguments[1];
        RELEASE_ASSERT(recovery.technique() == DisplacedInJSStack);
        return recovery.virtualRegister();
    }
    
    static VirtualRegister argumentsStart(const CodeOrigin& codeOrigin)
    {
        return argumentsStart(codeOrigin.inlineCallFrame);
    }
    
    void emitLoadStructure(RegisterID source, RegisterID dest, RegisterID scratch)
    {
#if USE(JSVALUE64)
        load32(MacroAssembler::Address(source, JSCell::structureIDOffset()), dest);
        loadPtr(vm()->heap.structureIDTable().base(), scratch);
        loadPtr(MacroAssembler::BaseIndex(scratch, dest, MacroAssembler::TimesEight), dest);
#else
        UNUSED_PARAM(scratch);
        loadPtr(MacroAssembler::Address(source, JSCell::structureIDOffset()), dest);
#endif
    }

    static void emitLoadStructure(AssemblyHelpers& jit, RegisterID base, RegisterID dest, RegisterID scratch)
    {
#if USE(JSVALUE64)
        jit.load32(MacroAssembler::Address(base, JSCell::structureIDOffset()), dest);
        jit.loadPtr(jit.vm()->heap.structureIDTable().base(), scratch);
        jit.loadPtr(MacroAssembler::BaseIndex(scratch, dest, MacroAssembler::TimesEight), dest);
#else
        UNUSED_PARAM(scratch);
        jit.loadPtr(MacroAssembler::Address(base, JSCell::structureIDOffset()), dest);
#endif
    }

    void emitStoreStructureWithTypeInfo(TrustedImmPtr structure, RegisterID dest, RegisterID)
    {
        emitStoreStructureWithTypeInfo(*this, structure, dest);
    }

    void emitStoreStructureWithTypeInfo(RegisterID structure, RegisterID dest, RegisterID scratch)
    {
#if USE(JSVALUE64)
        load64(MacroAssembler::Address(structure, Structure::structureIDOffset()), scratch);
        store64(scratch, MacroAssembler::Address(dest, JSCell::structureIDOffset()));
#else
        // Store all the info flags using a single 32-bit wide load and store.
        load32(MacroAssembler::Address(structure, Structure::indexingTypeOffset()), scratch);
        store32(scratch, MacroAssembler::Address(dest, JSCell::indexingTypeOffset()));

        // Store the StructureID
        storePtr(structure, MacroAssembler::Address(dest, JSCell::structureIDOffset()));
#endif
    }

    static void emitStoreStructureWithTypeInfo(AssemblyHelpers& jit, TrustedImmPtr structure, RegisterID dest);

    Jump jumpIfIsRememberedOrInEden(GPRReg cell)
    {
        return branchTest8(MacroAssembler::NonZero, MacroAssembler::Address(cell, JSCell::cellStateOffset()));
    }

    Jump jumpIfIsRememberedOrInEden(JSCell* cell)
    {
        uint8_t* address = reinterpret_cast<uint8_t*>(cell) + JSCell::cellStateOffset();
        return branchTest8(MacroAssembler::NonZero, MacroAssembler::AbsoluteAddress(address));
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
        //     } else {
        //         return symbol
        //     }
        // } else if (is number) {
        //     return number
        // } else if (is null) {
        //     return object
        // } else if (is boolean) {
        //     return boolean
        // } else {
        //     return undefined
        // }
        
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
                TrustedImm32(MasqueradesAsUndefined | TypeOfShouldCallGetCallData)));
        functor(TypeofType::Object, false);
        
        notObject.link(this);
        
        Jump notString = branchIfNotString(cellGPR);
        functor(TypeofType::String, false);
        notString.link(this);
        functor(TypeofType::Symbol, false);
        
        notCell.link(this);

        Jump notNumber = branchIfNotNumber(regs, tempGPR);
        functor(TypeofType::Number, false);
        notNumber.link(this);
        
        JumpList notNull = branchIfNotEqual(regs, jsNull());
        functor(TypeofType::Object, false);
        notNull.link(this);
        
        Jump notBoolean = branchIfNotBoolean(regs, tempGPR);
        functor(TypeofType::Boolean, false);
        notBoolean.link(this);
        
        functor(TypeofType::Undefined, true);
    }

    Vector<BytecodeAndMachineOffset>& decodedCodeMapFor(CodeBlock*);

    void makeSpaceOnStackForCCall()
    {
        unsigned stackOffset = WTF::roundUpToMultipleOf(stackAlignmentBytes(), maxFrameExtentForSlowPathCall);
        if (stackOffset)
            subPtr(TrustedImm32(stackOffset), stackPointerRegister);
    }

    void reclaimSpaceOnStackForCCall()
    {
        unsigned stackOffset = WTF::roundUpToMultipleOf(stackAlignmentBytes(), maxFrameExtentForSlowPathCall);
        if (stackOffset)
            addPtr(TrustedImm32(stackOffset), stackPointerRegister);
    }

#if USE(JSVALUE64)
    void emitRandomThunk(JSGlobalObject*, GPRReg scratch0, GPRReg scratch1, GPRReg scratch2, FPRReg result);
    void emitRandomThunk(GPRReg scratch0, GPRReg scratch1, GPRReg scratch2, GPRReg scratch3, FPRReg result);
#endif
    
protected:
    VM* m_vm;
    CodeBlock* m_codeBlock;
    CodeBlock* m_baselineCodeBlock;

    HashMap<CodeBlock*, Vector<BytecodeAndMachineOffset>> m_decodedCodeMaps;
};

} // namespace JSC

#endif // ENABLE(JIT)

#endif // AssemblyHelpers_h

