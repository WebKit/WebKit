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

#ifndef MacroAssemblerX86_64_h
#define MacroAssemblerX86_64_h

#include <wtf/Platform.h>

#if ENABLE(ASSEMBLER) && PLATFORM(X86_64)

#include "MacroAssemblerX86Common.h"

namespace JSC {

class MacroAssemblerX86_64 : public MacroAssemblerX86Common {
protected:
    static const X86::RegisterID scratchRegister = X86::r11;

public:
    static const Scale ScalePtr = TimesEight;

    using MacroAssemblerX86Common::add32;
    using MacroAssemblerX86Common::sub32;
    using MacroAssemblerX86Common::load32;
    using MacroAssemblerX86Common::store32;

    void add32(Imm32 imm, AbsoluteAddress address)
    {
        move(ImmPtr(address.m_ptr), scratchRegister);
        add32(imm, Address(scratchRegister));
    }
    
    void sub32(Imm32 imm, AbsoluteAddress address)
    {
        move(ImmPtr(address.m_ptr), scratchRegister);
        sub32(imm, Address(scratchRegister));
    }

    void load32(void* address, RegisterID dest)
    {
        if (dest == X86::eax)
            m_assembler.movl_mEAX(address);
        else {
            move(X86::eax, dest);
            m_assembler.movl_mEAX(address);
            swap(X86::eax, dest);
        }
    }

    void store32(Imm32 imm, void* address)
    {
        move(X86::eax, scratchRegister);
        move(imm, X86::eax);
        m_assembler.movl_EAXm(address);
        move(scratchRegister, X86::eax);
    }



    void addPtr(RegisterID src, RegisterID dest)
    {
        m_assembler.addq_rr(src, dest);
    }

    void addPtr(Imm32 imm, RegisterID srcDest)
    {
        m_assembler.addq_ir(imm.m_value, srcDest);
    }

    void addPtr(ImmPtr imm, RegisterID dest)
    {
        move(imm, scratchRegister);
        m_assembler.addq_rr(scratchRegister, dest);
    }

    void addPtr(Imm32 imm, RegisterID src, RegisterID dest)
    {
        m_assembler.leal_mr(imm.m_value, src, dest);
    }

    void andPtr(RegisterID src, RegisterID dest)
    {
        m_assembler.andq_rr(src, dest);
    }

    void andPtr(Imm32 imm, RegisterID srcDest)
    {
        m_assembler.andq_ir(imm.m_value, srcDest);
    }

    void orPtr(RegisterID src, RegisterID dest)
    {
        m_assembler.orq_rr(src, dest);
    }

    void orPtr(ImmPtr imm, RegisterID dest)
    {
        move(imm, scratchRegister);
        m_assembler.orq_rr(scratchRegister, dest);
    }

    void orPtr(Imm32 imm, RegisterID dest)
    {
        m_assembler.orq_ir(imm.m_value, dest);
    }

    void rshiftPtr(RegisterID shift_amount, RegisterID dest)
    {
        // On x86 we can only shift by ecx; if asked to shift by another register we'll
        // need rejig the shift amount into ecx first, and restore the registers afterwards.
        if (shift_amount != X86::ecx) {
            swap(shift_amount, X86::ecx);

            // E.g. transform "shll %eax, %eax" -> "xchgl %eax, %ecx; shll %ecx, %ecx; xchgl %eax, %ecx"
            if (dest == shift_amount)
                m_assembler.sarq_CLr(X86::ecx);
            // E.g. transform "shll %eax, %ecx" -> "xchgl %eax, %ecx; shll %ecx, %eax; xchgl %eax, %ecx"
            else if (dest == X86::ecx)
                m_assembler.sarq_CLr(shift_amount);
            // E.g. transform "shll %eax, %ebx" -> "xchgl %eax, %ecx; shll %ecx, %ebx; xchgl %eax, %ecx"
            else
                m_assembler.sarq_CLr(dest);
        
            swap(shift_amount, X86::ecx);
        } else
            m_assembler.sarq_CLr(dest);
    }

    void rshiftPtr(Imm32 imm, RegisterID dest)
    {
        m_assembler.sarq_i8r(imm.m_value, dest);
    }

    void subPtr(RegisterID src, RegisterID dest)
    {
        m_assembler.subq_rr(src, dest);
    }
    
    void subPtr(Imm32 imm, RegisterID dest)
    {
        m_assembler.subq_ir(imm.m_value, dest);
    }
    
    void subPtr(ImmPtr imm, RegisterID dest)
    {
        move(imm, scratchRegister);
        m_assembler.subq_rr(scratchRegister, dest);
    }

    void xorPtr(RegisterID src, RegisterID dest)
    {
        m_assembler.xorq_rr(src, dest);
    }

    void xorPtr(Imm32 imm, RegisterID srcDest)
    {
        m_assembler.xorq_ir(imm.m_value, srcDest);
    }


    void loadPtr(ImplicitAddress address, RegisterID dest)
    {
        m_assembler.movq_mr(address.offset, address.base, dest);
    }

    void loadPtr(BaseIndex address, RegisterID dest)
    {
        m_assembler.movq_mr(address.offset, address.base, address.index, address.scale, dest);
    }

    void loadPtr(void* address, RegisterID dest)
    {
        if (dest == X86::eax)
            m_assembler.movq_mEAX(address);
        else {
            move(X86::eax, dest);
            m_assembler.movq_mEAX(address);
            swap(X86::eax, dest);
        }
    }

    DataLabel32 loadPtrWithAddressOffsetPatch(Address address, RegisterID dest)
    {
        m_assembler.movq_mr_disp32(address.offset, address.base, dest);
        return DataLabel32(this);
    }

    void storePtr(RegisterID src, ImplicitAddress address)
    {
        m_assembler.movq_rm(src, address.offset, address.base);
    }

    void storePtr(RegisterID src, BaseIndex address)
    {
        m_assembler.movq_rm(src, address.offset, address.base, address.index, address.scale);
    }

    void storePtr(ImmPtr imm, ImplicitAddress address)
    {
        move(imm, scratchRegister);
        storePtr(scratchRegister, address);
    }

    DataLabel32 storePtrWithAddressOffsetPatch(RegisterID src, Address address)
    {
        m_assembler.movq_rm_disp32(src, address.offset, address.base);
        return DataLabel32(this);
    }


    Jump branchPtr(Condition cond, RegisterID left, RegisterID right)
    {
        m_assembler.cmpq_rr(right, left);
        return Jump(m_assembler.jCC(cond));
    }

    Jump branchPtr(Condition cond, RegisterID left, ImmPtr right)
    {
        intptr_t imm = right.asIntptr();
        if (CAN_SIGN_EXTEND_32_64(imm)) {
            if (!imm)
                m_assembler.testq_rr(left, left);
            else
                m_assembler.cmpq_ir(imm, left);
            return Jump(m_assembler.jCC(cond));
        } else {
            move(right, scratchRegister);
            return branchPtr(cond, left, scratchRegister);
        }
    }

    Jump branchPtr(Condition cond, RegisterID left, Address right)
    {
        m_assembler.cmpq_mr(right.offset, right.base, left);
        return Jump(m_assembler.jCC(cond));
    }

    Jump branchPtr(Condition cond, AbsoluteAddress left, RegisterID right)
    {
        move(ImmPtr(left.m_ptr), scratchRegister);
        return branchPtr(cond, Address(scratchRegister), right);
    }

    Jump branchPtr(Condition cond, Address left, RegisterID right)
    {
        m_assembler.cmpq_rm(right, left.offset, left.base);
        return Jump(m_assembler.jCC(cond));
    }

    Jump branchPtr(Condition cond, Address left, ImmPtr right)
    {
        move(right, scratchRegister);
        return branchPtr(cond, left, scratchRegister);
    }

    Jump branchTestPtr(Condition cond, RegisterID reg, RegisterID mask)
    {
        m_assembler.testq_rr(reg, mask);
        return Jump(m_assembler.jCC(cond));
    }

    Jump branchTestPtr(Condition cond, RegisterID reg, Imm32 mask = Imm32(-1))
    {
        // if we are only interested in the low seven bits, this can be tested with a testb
        if (mask.m_value == -1)
            m_assembler.testq_rr(reg, reg);
        else if ((mask.m_value & ~0x7f) == 0)
            m_assembler.testb_i8r(mask.m_value, reg);
        else
            m_assembler.testq_i32r(mask.m_value, reg);
        return Jump(m_assembler.jCC(cond));
    }

    Jump branchTestPtr(Condition cond, Address address, Imm32 mask = Imm32(-1))
    {
        if (mask.m_value == -1)
            m_assembler.cmpq_im(0, address.offset, address.base);
        else
            m_assembler.testq_i32m(mask.m_value, address.offset, address.base);
        return Jump(m_assembler.jCC(cond));
    }

    Jump branchTestPtr(Condition cond, BaseIndex address, Imm32 mask = Imm32(-1))
    {
        if (mask.m_value == -1)
            m_assembler.cmpq_im(0, address.offset, address.base, address.index, address.scale);
        else
            m_assembler.testq_i32m(mask.m_value, address.offset, address.base, address.index, address.scale);
        return Jump(m_assembler.jCC(cond));
    }


    Jump branchAddPtr(Condition cond, RegisterID src, RegisterID dest)
    {
        ASSERT((cond == Overflow) || (cond == Zero) || (cond == NonZero));
        addPtr(src, dest);
        return Jump(m_assembler.jCC(cond));
    }

    Jump branchSubPtr(Condition cond, Imm32 imm, RegisterID dest)
    {
        ASSERT((cond == Overflow) || (cond == Zero) || (cond == NonZero));
        subPtr(imm, dest);
        return Jump(m_assembler.jCC(cond));
    }

    Jump branchPtrWithPatch(Condition cond, RegisterID left, DataLabelPtr& dataLabel, ImmPtr initialRightValue = ImmPtr(0))
    {
        m_assembler.movq_i64r(initialRightValue.asIntptr(), scratchRegister);
        dataLabel = DataLabelPtr(this);
        return branchPtr(cond, left, scratchRegister);
    }

    Jump branchPtrWithPatch(Condition cond, Address left, DataLabelPtr& dataLabel, ImmPtr initialRightValue = ImmPtr(0))
    {
        m_assembler.movq_i64r(initialRightValue.asIntptr(), scratchRegister);
        dataLabel = DataLabelPtr(this);
        return branchPtr(cond, left, scratchRegister);
    }

    DataLabelPtr storePtrWithPatch(Address address)
    {
        m_assembler.movq_i64r(0, scratchRegister);
        DataLabelPtr label(this);
        storePtr(scratchRegister, address);
        return label;
    }
};

} // namespace JSC

#endif // ENABLE(ASSEMBLER)

#endif // MacroAssemblerX86_64_h
