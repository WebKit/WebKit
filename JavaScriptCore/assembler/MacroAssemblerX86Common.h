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

#ifndef MacroAssemblerX86Common_h
#define MacroAssemblerX86Common_h

#include <wtf/Platform.h>

#if ENABLE(ASSEMBLER)

#include "X86Assembler.h"
#include "AbstractMacroAssembler.h"

namespace JSC {

class MacroAssemblerX86Common : public AbstractMacroAssembler<X86Assembler> {
public:

    typedef X86Assembler::Condition Condition;
    static const Condition Equal = X86Assembler::ConditionE;
    static const Condition NotEqual = X86Assembler::ConditionNE;
    static const Condition Above = X86Assembler::ConditionA;
    static const Condition AboveOrEqual = X86Assembler::ConditionAE;
    static const Condition Below = X86Assembler::ConditionB;
    static const Condition BelowOrEqual = X86Assembler::ConditionBE;
    static const Condition GreaterThan = X86Assembler::ConditionG;
    static const Condition GreaterThanOrEqual = X86Assembler::ConditionGE;
    static const Condition LessThan = X86Assembler::ConditionL;
    static const Condition LessThanOrEqual = X86Assembler::ConditionLE;
    static const Condition Overflow = X86Assembler::ConditionO;
    static const Condition Zero = X86Assembler::ConditionE;
    static const Condition NonZero = X86Assembler::ConditionNE;

    static const RegisterID stackPointerRegister = X86::esp;

    // Integer arithmetic operations:
    //
    // Operations are typically two operand - operation(source, srcDst)
    // For many operations the source may be an Imm32, the srcDst operand
    // may often be a memory location (explictly described using an Address
    // object).

    void add32(RegisterID src, RegisterID dest)
    {
        m_assembler.addl_rr(src, dest);
    }

    void add32(Imm32 imm, Address address)
    {
        m_assembler.addl_im(imm.m_value, address.offset, address.base);
    }

    void add32(Imm32 imm, RegisterID dest)
    {
        m_assembler.addl_ir(imm.m_value, dest);
    }
    
    void add32(Address src, RegisterID dest)
    {
        m_assembler.addl_mr(src.offset, src.base, dest);
    }
    
    void and32(RegisterID src, RegisterID dest)
    {
        m_assembler.andl_rr(src, dest);
    }

    void and32(Imm32 imm, RegisterID dest)
    {
        m_assembler.andl_ir(imm.m_value, dest);
    }

    void lshift32(Imm32 imm, RegisterID dest)
    {
        m_assembler.shll_i8r(imm.m_value, dest);
    }
    
    void lshift32(RegisterID shift_amount, RegisterID dest)
    {
        // On x86 we can only shift by ecx; if asked to shift by another register we'll
        // need rejig the shift amount into ecx first, and restore the registers afterwards.
        if (shift_amount != X86::ecx) {
            swap(shift_amount, X86::ecx);

            // E.g. transform "shll %eax, %eax" -> "xchgl %eax, %ecx; shll %ecx, %ecx; xchgl %eax, %ecx"
            if (dest == shift_amount)
                m_assembler.shll_CLr(X86::ecx);
            // E.g. transform "shll %eax, %ecx" -> "xchgl %eax, %ecx; shll %ecx, %eax; xchgl %eax, %ecx"
            else if (dest == X86::ecx)
                m_assembler.shll_CLr(shift_amount);
            // E.g. transform "shll %eax, %ebx" -> "xchgl %eax, %ecx; shll %ecx, %ebx; xchgl %eax, %ecx"
            else
                m_assembler.shll_CLr(dest);
        
            swap(shift_amount, X86::ecx);
        } else
            m_assembler.shll_CLr(dest);
    }
    
    // Take the value from dividend, divide it by divisor, and put the remainder in remainder.
    // For now, this operation has specific register requirements, and the three register must
    // be unique.  It is unfortunate to expose this in the MacroAssembler interface, however
    // given the complexity to fix, the fact that it is not uncommmon  for processors to have
    // specific register requirements on this operation (e.g. Mips result in 'hi'), or to not
    // support a hardware divide at all, it may not be 
    void mod32(RegisterID divisor, RegisterID dividend, RegisterID remainder)
    {
#ifdef NDEBUG
#pragma unused(dividend,remainder)
#else
        ASSERT((dividend == X86::eax) && (remainder == X86::edx));
        ASSERT((dividend != divisor) && (remainder != divisor));
#endif

        m_assembler.cdq();
        m_assembler.idivl_r(divisor);
    }

    void mul32(RegisterID src, RegisterID dest)
    {
        m_assembler.imull_rr(src, dest);
    }
    
    void mul32(Imm32 imm, RegisterID src, RegisterID dest)
    {
        m_assembler.imull_i32r(src, imm.m_value, dest);
    }
    
    void not32(RegisterID srcDest)
    {
        m_assembler.notl_r(srcDest);
    }
    
    void or32(RegisterID src, RegisterID dest)
    {
        m_assembler.orl_rr(src, dest);
    }

    void or32(Imm32 imm, RegisterID dest)
    {
        m_assembler.orl_ir(imm.m_value, dest);
    }

    void rshift32(RegisterID shift_amount, RegisterID dest)
    {
        // On x86 we can only shift by ecx; if asked to shift by another register we'll
        // need rejig the shift amount into ecx first, and restore the registers afterwards.
        if (shift_amount != X86::ecx) {
            swap(shift_amount, X86::ecx);

            // E.g. transform "shll %eax, %eax" -> "xchgl %eax, %ecx; shll %ecx, %ecx; xchgl %eax, %ecx"
            if (dest == shift_amount)
                m_assembler.sarl_CLr(X86::ecx);
            // E.g. transform "shll %eax, %ecx" -> "xchgl %eax, %ecx; shll %ecx, %eax; xchgl %eax, %ecx"
            else if (dest == X86::ecx)
                m_assembler.sarl_CLr(shift_amount);
            // E.g. transform "shll %eax, %ebx" -> "xchgl %eax, %ecx; shll %ecx, %ebx; xchgl %eax, %ecx"
            else
                m_assembler.sarl_CLr(dest);
        
            swap(shift_amount, X86::ecx);
        } else
            m_assembler.sarl_CLr(dest);
    }

    void rshift32(Imm32 imm, RegisterID dest)
    {
        m_assembler.sarl_i8r(imm.m_value, dest);
    }

    void sub32(RegisterID src, RegisterID dest)
    {
        m_assembler.subl_rr(src, dest);
    }
    
    void sub32(Imm32 imm, RegisterID dest)
    {
        m_assembler.subl_ir(imm.m_value, dest);
    }
    
    void sub32(Imm32 imm, Address address)
    {
        m_assembler.subl_im(imm.m_value, address.offset, address.base);
    }

    void sub32(Address src, RegisterID dest)
    {
        m_assembler.subl_mr(src.offset, src.base, dest);
    }

    void xor32(RegisterID src, RegisterID dest)
    {
        m_assembler.xorl_rr(src, dest);
    }

    void xor32(Imm32 imm, RegisterID srcDest)
    {
        m_assembler.xorl_ir(imm.m_value, srcDest);
    }
    

    // Memory access operations:
    //
    // Loads are of the form load(address, destination) and stores of the form
    // store(source, address).  The source for a store may be an Imm32.  Address
    // operand objects to loads and store will be implicitly constructed if a
    // register is passed.

    void load32(ImplicitAddress address, RegisterID dest)
    {
        m_assembler.movl_mr(address.offset, address.base, dest);
    }

    void load32(BaseIndex address, RegisterID dest)
    {
        m_assembler.movl_mr(address.offset, address.base, address.index, address.scale, dest);
    }

    DataLabel32 load32WithAddressOffsetPatch(Address address, RegisterID dest)
    {
        m_assembler.movl_mr_disp32(address.offset, address.base, dest);
        return DataLabel32(this);
    }

    void load16(BaseIndex address, RegisterID dest)
    {
        m_assembler.movzwl_mr(address.offset, address.base, address.index, address.scale, dest);
    }

    DataLabel32 store32WithAddressOffsetPatch(RegisterID src, Address address)
    {
        m_assembler.movl_rm_disp32(src, address.offset, address.base);
        return DataLabel32(this);
    }

    void store32(RegisterID src, ImplicitAddress address)
    {
        m_assembler.movl_rm(src, address.offset, address.base);
    }

    void store32(RegisterID src, BaseIndex address)
    {
        m_assembler.movl_rm(src, address.offset, address.base, address.index, address.scale);
    }

    void store32(Imm32 imm, ImplicitAddress address)
    {
        m_assembler.movl_i32m(imm.m_value, address.offset, address.base);
    }
    

    // Stack manipulation operations:
    //
    // The ABI is assumed to provide a stack abstraction to memory,
    // containing machine word sized units of data.  Push and pop
    // operations add and remove a single register sized unit of data
    // to or from the stack.  Peek and poke operations read or write
    // values on the stack, without moving the current stack position.
    
    void pop(RegisterID dest)
    {
        m_assembler.pop_r(dest);
    }

    void push(RegisterID src)
    {
        m_assembler.push_r(src);
    }

    void push(Address address)
    {
        m_assembler.push_m(address.offset, address.base);
    }

    void push(Imm32 imm)
    {
        m_assembler.push_i32(imm.m_value);
    }

    // Register move operations:
    //
    // Move values in registers.

    void move(Imm32 imm, RegisterID dest)
    {
        // Note: on 64-bit the Imm32 value is zero extended into the register, it
        // may be useful to have a separate version that sign extends the value?
        if (!imm.m_value)
            m_assembler.xorl_rr(dest, dest);
        else
            m_assembler.movl_i32r(imm.m_value, dest);
    }

#if PLATFORM(X86_64)
    void move(RegisterID src, RegisterID dest)
    {
        // Note: on 64-bit this is is a full register move; perhaps it would be
        // useful to have separate move32 & movePtr, with move32 zero extending?
        m_assembler.movq_rr(src, dest);
    }

    void move(ImmPtr imm, RegisterID dest)
    {
        if (CAN_SIGN_EXTEND_U32_64(imm.asIntptr()))
            m_assembler.movl_i32r(static_cast<int32_t>(imm.asIntptr()), dest);
        else
            m_assembler.movq_i64r(imm.asIntptr(), dest);
    }

    void swap(RegisterID reg1, RegisterID reg2)
    {
        m_assembler.xchgq_rr(reg1, reg2);
    }

    void signExtend32ToPtr(RegisterID src, RegisterID dest)
    {
        m_assembler.movsxd_rr(src, dest);
    }

    void zeroExtend32ToPtr(RegisterID src, RegisterID dest)
    {
        m_assembler.movl_rr(src, dest);
    }
#else
    void move(RegisterID src, RegisterID dest)
    {
        m_assembler.movl_rr(src, dest);
    }

    void move(ImmPtr imm, RegisterID dest)
    {
        m_assembler.movl_i32r(imm.asIntptr(), dest);
    }

    void swap(RegisterID reg1, RegisterID reg2)
    {
        m_assembler.xchgl_rr(reg1, reg2);
    }

    void signExtend32ToPtr(RegisterID src, RegisterID dest)
    {
        if (src != dest)
            move(src, dest);
    }

    void zeroExtend32ToPtr(RegisterID src, RegisterID dest)
    {
        if (src != dest)
            move(src, dest);
    }
#endif


    // Forwards / external control flow operations:
    //
    // This set of jump and conditional branch operations return a Jump
    // object which may linked at a later point, allow forwards jump,
    // or jumps that will require external linkage (after the code has been
    // relocated).
    //
    // For branches, signed <, >, <= and >= are denoted as l, g, le, and ge
    // respecitvely, for unsigned comparisons the names b, a, be, and ae are
    // used (representing the names 'below' and 'above').
    //
    // Operands to the comparision are provided in the expected order, e.g.
    // jle32(reg1, Imm32(5)) will branch if the value held in reg1, when
    // treated as a signed 32bit value, is less than or equal to 5.
    //
    // jz and jnz test whether the first operand is equal to zero, and take
    // an optional second operand of a mask under which to perform the test.

public:
    Jump branch32(Condition cond, RegisterID left, RegisterID right)
    {
        m_assembler.cmpl_rr(right, left);
        return Jump(m_assembler.jCC(cond));
    }

    Jump branch32(Condition cond, RegisterID left, Imm32 right)
    {
        if (((cond == Equal) || (cond == NotEqual)) && !right.m_value)
            m_assembler.testl_rr(left, left);
        else
            m_assembler.cmpl_ir(right.m_value, left);
        return Jump(m_assembler.jCC(cond));
    }
    
    Jump branch32(Condition cond, RegisterID left, Address right)
    {
        m_assembler.cmpl_mr(right.offset, right.base, left);
        return Jump(m_assembler.jCC(cond));
    }
    
    Jump branch32(Condition cond, Address left, RegisterID right)
    {
        m_assembler.cmpl_rm(right, left.offset, left.base);
        return Jump(m_assembler.jCC(cond));
    }

    Jump branch32(Condition cond, Address left, Imm32 right)
    {
        m_assembler.cmpl_im(right.m_value, left.offset, left.base);
        return Jump(m_assembler.jCC(cond));
    }

    Jump branch16(Condition cond, BaseIndex left, RegisterID right)
    {
        m_assembler.cmpw_rm(right, left.offset, left.base, left.index, left.scale);
        return Jump(m_assembler.jCC(cond));
    }

    Jump branchTest32(Condition cond, RegisterID reg, RegisterID mask)
    {
        ASSERT((cond == Zero) || (cond == NonZero));
        m_assembler.testl_rr(reg, mask);
        return Jump(m_assembler.jCC(cond));
    }

    Jump branchTest32(Condition cond, RegisterID reg, Imm32 mask = Imm32(-1))
    {
        ASSERT((cond == Zero) || (cond == NonZero));
        // if we are only interested in the low seven bits, this can be tested with a testb
        if (mask.m_value == -1)
            m_assembler.testl_rr(reg, reg);
        else if ((mask.m_value & ~0x7f) == 0)
            m_assembler.testb_i8r(mask.m_value, reg);
        else
            m_assembler.testl_i32r(mask.m_value, reg);
        return Jump(m_assembler.jCC(cond));
    }

    Jump branchTest32(Condition cond, Address address, Imm32 mask = Imm32(-1))
    {
        ASSERT((cond == Zero) || (cond == NonZero));
        if (mask.m_value == -1)
            m_assembler.cmpl_im(0, address.offset, address.base);
        else
            m_assembler.testl_i32m(mask.m_value, address.offset, address.base);
        return Jump(m_assembler.jCC(cond));
    }

    Jump branchTest32(Condition cond, BaseIndex address, Imm32 mask = Imm32(-1))
    {
        ASSERT((cond == Zero) || (cond == NonZero));
        if (mask.m_value == -1)
            m_assembler.cmpl_im(0, address.offset, address.base, address.index, address.scale);
        else
            m_assembler.testl_i32m(mask.m_value, address.offset, address.base, address.index, address.scale);
        return Jump(m_assembler.jCC(cond));
    }

    Jump jump()
    {
        return Jump(m_assembler.jmp());
    }

    void jump(RegisterID target)
    {
        m_assembler.jmp_r(target);
    }

    // Address is a memory location containing the address to jump to
    void jump(Address address)
    {
        m_assembler.jmp_m(address.offset, address.base);
    }


    // Arithmetic control flow operations:
    //
    // This set of conditional branch operations branch based
    // on the result of an arithmetic operation.  The operation
    // is performed as normal, storing the result.
    //
    // * jz operations branch if the result is zero.
    // * jo operations branch if the (signed) arithmetic
    //   operation caused an overflow to occur.
    
    Jump branchAdd32(Condition cond, RegisterID src, RegisterID dest)
    {
        ASSERT((cond == Overflow) || (cond == Zero) || (cond == NonZero));
        add32(src, dest);
        return Jump(m_assembler.jCC(cond));
    }
    
    Jump branchAdd32(Condition cond, Imm32 imm, RegisterID dest)
    {
        ASSERT((cond == Overflow) || (cond == Zero) || (cond == NonZero));
        add32(imm, dest);
        return Jump(m_assembler.jCC(cond));
    }
    
    Jump branchMul32(Condition cond, RegisterID src, RegisterID dest)
    {
        ASSERT((cond == Overflow) || (cond == Zero) || (cond == NonZero));
        mul32(src, dest);
        return Jump(m_assembler.jCC(cond));
    }
    
    Jump branchMul32(Condition cond, Imm32 imm, RegisterID src, RegisterID dest)
    {
        ASSERT((cond == Overflow) || (cond == Zero) || (cond == NonZero));
        mul32(imm, src, dest);
        return Jump(m_assembler.jCC(cond));
    }
    
    Jump branchSub32(Condition cond, RegisterID src, RegisterID dest)
    {
        ASSERT((cond == Overflow) || (cond == Zero) || (cond == NonZero));
        sub32(src, dest);
        return Jump(m_assembler.jCC(cond));
    }
    
    Jump branchSub32(Condition cond, Imm32 imm, RegisterID dest)
    {
        ASSERT((cond == Overflow) || (cond == Zero) || (cond == NonZero));
        sub32(imm, dest);
        return Jump(m_assembler.jCC(cond));
    }
    

    // Miscellaneous operations:

    void breakpoint()
    {
        m_assembler.int3();
    }

    Call call()
    {
        return Call(m_assembler.call(), true);
    }

    Call call(RegisterID target)
    {
        return Call(m_assembler.call(target), false);
    }

    void ret()
    {
        m_assembler.ret();
    }

    void set32(Condition cond, RegisterID left, RegisterID right, RegisterID dest)
    {
        m_assembler.cmpl_rr(right, left);
        m_assembler.setCC_r(cond, dest);
        m_assembler.movzbl_rr(dest, dest);
    }

    void set32(Condition cond, RegisterID left, Imm32 right, RegisterID dest)
    {
        if (((cond == Equal) || (cond == NotEqual)) && !right.m_value)
            m_assembler.testl_rr(left, left);
        else
            m_assembler.cmpl_ir(right.m_value, left);
        m_assembler.setCC_r(cond, dest);
        m_assembler.movzbl_rr(dest, dest);
    }

    // FIXME:
    // The mask should be optional... paerhaps the argument order should be
    // dest-src, operations always have a dest? ... possibly not true, considering
    // asm ops like test, or pseudo ops like pop().
    void setTest32(Condition cond, Address address, Imm32 mask, RegisterID dest)
    {
        if (mask.m_value == -1)
            m_assembler.cmpl_im(0, address.offset, address.base);
        else
            m_assembler.testl_i32m(mask.m_value, address.offset, address.base);
        m_assembler.setCC_r(cond, dest);
        m_assembler.movzbl_rr(dest, dest);
    }
};

} // namespace JSC

#endif // ENABLE(ASSEMBLER)

#endif // MacroAssemblerX86Common_h
