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

#if ENABLE(ASSEMBLER) && CPU(X86)

#include "MacroAssemblerX86Common.h"

namespace JSC {

class MacroAssemblerX86 : public MacroAssemblerX86Common {
public:
    static constexpr unsigned numGPRs = 8;
    static constexpr unsigned numFPRs = 8;
    
    static constexpr Scale ScalePtr = TimesFour;

    using MacroAssemblerX86Common::add32;
    using MacroAssemblerX86Common::and32;
    using MacroAssemblerX86Common::branchAdd32;
    using MacroAssemblerX86Common::branchSub32;
    using MacroAssemblerX86Common::sub32;
    using MacroAssemblerX86Common::or32;
    using MacroAssemblerX86Common::load32;
    using MacroAssemblerX86Common::load8;
    using MacroAssemblerX86Common::store32;
    using MacroAssemblerX86Common::store8;
    using MacroAssemblerX86Common::branch32;
    using MacroAssemblerX86Common::call;
    using MacroAssemblerX86Common::jump;
    using MacroAssemblerX86Common::farJump;
    using MacroAssemblerX86Common::addDouble;
    using MacroAssemblerX86Common::loadDouble;
    using MacroAssemblerX86Common::storeDouble;
    using MacroAssemblerX86Common::convertInt32ToDouble;
    using MacroAssemblerX86Common::branch8;
    using MacroAssemblerX86Common::branchTest8;

    void add32(TrustedImm32 imm, RegisterID src, RegisterID dest)
    {
        m_assembler.leal_mr(imm.m_value, src, dest);
    }

    void add32(TrustedImm32 imm, AbsoluteAddress address)
    {
        m_assembler.addl_im(imm.m_value, address.m_ptr);
    }
    
    void add32(AbsoluteAddress address, RegisterID dest)
    {
        m_assembler.addl_mr(address.m_ptr, dest);
    }
    
    void add64(TrustedImm32 imm, AbsoluteAddress address)
    {
        m_assembler.addl_im(imm.m_value, address.m_ptr);
        m_assembler.adcl_im(imm.m_value >> 31, reinterpret_cast<const char*>(address.m_ptr) + sizeof(int32_t));
    }

    void getEffectiveAddress(BaseIndex address, RegisterID dest)
    {
        return x86Lea32(address, dest);
    }

    void and32(TrustedImm32 imm, AbsoluteAddress address)
    {
        m_assembler.andl_im(imm.m_value, address.m_ptr);
    }
    
    void or32(TrustedImm32 imm, AbsoluteAddress address)
    {
        m_assembler.orl_im(imm.m_value, address.m_ptr);
    }
    
    void or32(RegisterID reg, AbsoluteAddress address)
    {
        m_assembler.orl_rm(reg, address.m_ptr);
    }

    void or16(TrustedImm32 imm, AbsoluteAddress address)
    {
        m_assembler.orw_im(imm.m_value, address.m_ptr);
    }
    
    void sub32(TrustedImm32 imm, AbsoluteAddress address)
    {
        m_assembler.subl_im(imm.m_value, address.m_ptr);
    }

    void load32(const void* address, RegisterID dest)
    {
        m_assembler.movl_mr(address, dest);
    }
    
    void load8(const void* address, RegisterID dest)
    {
        m_assembler.movzbl_mr(address, dest);
    }

    void abortWithReason(AbortReason reason)
    {
        move(TrustedImm32(reason), X86Registers::eax);
        breakpoint();
    }

    void abortWithReason(AbortReason reason, intptr_t misc)
    {
        move(TrustedImm32(misc), X86Registers::edx);
        abortWithReason(reason);
    }

    ConvertibleLoadLabel convertibleLoadPtr(Address address, RegisterID dest)
    {
        ConvertibleLoadLabel result = ConvertibleLoadLabel(this);
        m_assembler.movl_mr(address.offset, address.base, dest);
        return result;
    }

    void addDouble(AbsoluteAddress address, FPRegisterID dest)
    {
        m_assembler.addsd_mr(address.m_ptr, dest);
    }

    void storeDouble(FPRegisterID src, TrustedImmPtr address)
    {
        ASSERT(address.m_value);
        m_assembler.movsd_rm(src, address.asPtr());
    }

    void convertInt32ToDouble(AbsoluteAddress src, FPRegisterID dest)
    {
        m_assembler.cvtsi2sd_mr(src.m_ptr, dest);
    }

    void store32(TrustedImm32 imm, void* address)
    {
        m_assembler.movl_i32m(imm.m_value, address);
    }

    void store32(RegisterID src, void* address)
    {
        m_assembler.movl_rm(src, address);
    }
    
    void store8(RegisterID src, void* address)
    {
        m_assembler.movb_rm(src, address);
    }

    void store8(TrustedImm32 imm, void* address)
    {
        TrustedImm32 imm8(static_cast<int8_t>(imm.m_value));
        m_assembler.movb_i8m(imm8.m_value, address);
    }
    
    void moveDoubleToInts(FPRegisterID src, RegisterID dest1, RegisterID dest2)
    {
        m_assembler.pextrw_irr(3, src, dest1);
        m_assembler.pextrw_irr(2, src, dest2);
        lshift32(TrustedImm32(16), dest1);
        or32(dest1, dest2);
        moveFloatTo32(src, dest1);
    }

    void moveIntsToDouble(RegisterID src1, RegisterID src2, FPRegisterID dest, FPRegisterID scratch)
    {
        move32ToFloat(src1, dest);
        move32ToFloat(src2, scratch);
        lshiftPacked(TrustedImm32(32), scratch);
        orPacked(scratch, dest);
    }

    Jump branchAdd32(ResultCondition cond, TrustedImm32 imm, AbsoluteAddress dest)
    {
        m_assembler.addl_im(imm.m_value, dest.m_ptr);
        return Jump(m_assembler.jCC(x86Condition(cond)));
    }

    Jump branchSub32(ResultCondition cond, TrustedImm32 imm, AbsoluteAddress dest)
    {
        m_assembler.subl_im(imm.m_value, dest.m_ptr);
        return Jump(m_assembler.jCC(x86Condition(cond)));
    }

    Jump branch32(RelationalCondition cond, AbsoluteAddress left, RegisterID right)
    {
        m_assembler.cmpl_rm(right, left.m_ptr);
        return Jump(m_assembler.jCC(x86Condition(cond)));
    }

    Jump branch32(RelationalCondition cond, AbsoluteAddress left, TrustedImm32 right)
    {
        m_assembler.cmpl_im(right.m_value, left.m_ptr);
        return Jump(m_assembler.jCC(x86Condition(cond)));
    }

    Call call(PtrTag)
    {
        return Call(m_assembler.call(), Call::Linkable);
    }

    ALWAYS_INLINE Call call(RegisterID callTag) { return UNUSED_PARAM(callTag), call(NoPtrTag); }

    // Address is a memory location containing the address to jump to
    void farJump(AbsoluteAddress address, PtrTag)
    {
        m_assembler.jmp_m(address.m_ptr);
    }

    ALWAYS_INLINE void farJump(AbsoluteAddress address, RegisterID jumpTag) { UNUSED_PARAM(jumpTag), farJump(address, NoPtrTag); }

    DataLabelPtr moveWithPatch(TrustedImmPtr initialValue, RegisterID dest)
    {
        padBeforePatch();
        m_assembler.movl_i32r(initialValue.asIntptr(), dest);
        return DataLabelPtr(this);
    }
    
    Jump branch8(RelationalCondition cond, AbsoluteAddress left, TrustedImm32 right)
    {
        TrustedImm32 right8(static_cast<int8_t>(right.m_value));
        m_assembler.cmpb_im(right8.m_value, left.m_ptr);
        return Jump(m_assembler.jCC(x86Condition(cond)));
    }

    Jump branchTest8(ResultCondition cond, AbsoluteAddress address, TrustedImm32 mask = TrustedImm32(-1))
    {
        TrustedImm32 mask8(static_cast<int8_t>(mask.m_value));
        if (mask8.m_value == -1)
            m_assembler.cmpb_im(0, address.m_ptr);
        else
            m_assembler.testb_im(mask8.m_value, address.m_ptr);
        return Jump(m_assembler.jCC(x86Condition(cond)));
    }

    Jump branchPtrWithPatch(RelationalCondition cond, RegisterID left, DataLabelPtr& dataLabel, TrustedImmPtr initialRightValue = TrustedImmPtr(nullptr))
    {
        padBeforePatch();
        m_assembler.cmpl_ir_force32(initialRightValue.asIntptr(), left);
        dataLabel = DataLabelPtr(this);
        return Jump(m_assembler.jCC(x86Condition(cond)));
    }

    Jump branchPtrWithPatch(RelationalCondition cond, Address left, DataLabelPtr& dataLabel, TrustedImmPtr initialRightValue = TrustedImmPtr(nullptr))
    {
        padBeforePatch();
        m_assembler.cmpl_im_force32(initialRightValue.asIntptr(), left.offset, left.base);
        dataLabel = DataLabelPtr(this);
        return Jump(m_assembler.jCC(x86Condition(cond)));
    }

    Jump branch32WithPatch(RelationalCondition cond, Address left, DataLabel32& dataLabel, TrustedImm32 initialRightValue = TrustedImm32(0))
    {
        padBeforePatch();
        m_assembler.cmpl_im_force32(initialRightValue.m_value, left.offset, left.base);
        dataLabel = DataLabel32(this);
        return Jump(m_assembler.jCC(x86Condition(cond)));
    }

    DataLabelPtr storePtrWithPatch(TrustedImmPtr initialValue, ImplicitAddress address)
    {
        padBeforePatch();
        m_assembler.movl_i32m(initialValue.asIntptr(), address.offset, address.base);
        return DataLabelPtr(this);
    }

    static bool supportsFloatingPoint() { return true; }
    static bool supportsFloatingPointTruncate() { return true; }
    static bool supportsFloatingPointSqrt() { return true; }
    static bool supportsFloatingPointAbs() { return true; }

    template<PtrTag resultTag, PtrTag locationTag>
    static FunctionPtr<resultTag> readCallTarget(CodeLocationCall<locationTag> call)
    {
        intptr_t offset = WTF::unalignedLoad<int32_t>(bitwise_cast<int32_t*>(call.dataLocation()) - 1);
        return FunctionPtr<resultTag>(reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(call.dataLocation()) + offset));
    }

    static bool canJumpReplacePatchableBranchPtrWithPatch() { return true; }
    static bool canJumpReplacePatchableBranch32WithPatch() { return true; }

    template<PtrTag tag>
    static CodeLocationLabel<tag> startOfBranchPtrWithPatchOnRegister(CodeLocationDataLabelPtr<tag> label)
    {
        const int opcodeBytes = 1;
        const int modRMBytes = 1;
        const int immediateBytes = 4;
        const int totalBytes = opcodeBytes + modRMBytes + immediateBytes;
        ASSERT(totalBytes >= maxJumpReplacementSize());
        return label.labelAtOffset(-totalBytes);
    }

    template<PtrTag tag>
    static CodeLocationLabel<tag> startOfPatchableBranchPtrWithPatchOnAddress(CodeLocationDataLabelPtr<tag> label)
    {
        const int opcodeBytes = 1;
        const int modRMBytes = 1;
        const int offsetBytes = 0;
        const int immediateBytes = 4;
        const int totalBytes = opcodeBytes + modRMBytes + offsetBytes + immediateBytes;
        ASSERT(totalBytes >= maxJumpReplacementSize());
        return label.labelAtOffset(-totalBytes);
    }

    template<PtrTag tag>
    static CodeLocationLabel<tag> startOfPatchableBranch32WithPatchOnAddress(CodeLocationDataLabel32<tag> label)
    {
        const int opcodeBytes = 1;
        const int modRMBytes = 1;
        const int offsetBytes = 0;
        const int immediateBytes = 4;
        const int totalBytes = opcodeBytes + modRMBytes + offsetBytes + immediateBytes;
        ASSERT(totalBytes >= maxJumpReplacementSize());
        return label.labelAtOffset(-totalBytes);
    }

    template<PtrTag tag>
    static void revertJumpReplacementToBranchPtrWithPatch(CodeLocationLabel<tag> instructionStart, RegisterID reg, void* initialValue)
    {
        X86Assembler::revertJumpTo_cmpl_ir_force32(instructionStart.executableAddress(), reinterpret_cast<intptr_t>(initialValue), reg);
    }

    template<PtrTag tag>
    static void revertJumpReplacementToPatchableBranchPtrWithPatch(CodeLocationLabel<tag> instructionStart, Address address, void* initialValue)
    {
        ASSERT(!address.offset);
        X86Assembler::revertJumpTo_cmpl_im_force32(instructionStart.executableAddress(), reinterpret_cast<intptr_t>(initialValue), 0, address.base);
    }

    template<PtrTag tag>
    static void revertJumpReplacementToPatchableBranch32WithPatch(CodeLocationLabel<tag> instructionStart, Address address, int32_t initialValue)
    {
        ASSERT(!address.offset);
        X86Assembler::revertJumpTo_cmpl_im_force32(instructionStart.executableAddress(), initialValue, 0, address.base);
    }

    template<PtrTag callTag, PtrTag destTag>
    static void repatchCall(CodeLocationCall<callTag> call, CodeLocationLabel<destTag> destination)
    {
        X86Assembler::relinkCall(call.dataLocation(), destination.executableAddress());
    }

    template<PtrTag callTag, PtrTag destTag>
    static void repatchCall(CodeLocationCall<callTag> call, FunctionPtr<destTag> destination)
    {
        X86Assembler::relinkCall(call.dataLocation(), destination.executableAddress());
    }

private:
    friend class LinkBuffer;

    template<PtrTag tag>
    static void linkCall(void* code, Call call, FunctionPtr<tag> function)
    {
        if (call.isFlagSet(Call::Tail))
            X86Assembler::linkJump(code, call.m_label, function.executableAddress());
        else
            X86Assembler::linkCall(code, call.m_label, function.executableAddress());
    }
};

} // namespace JSC

#endif // ENABLE(ASSEMBLER)
