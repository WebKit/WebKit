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

#ifndef MacroAssembler_h
#define MacroAssembler_h

#include <wtf/Platform.h>

#if ENABLE(ASSEMBLER)

#include "X86Assembler.h"

namespace JSC {

class MacroAssembler {
protected:
    X86Assembler m_assembler;

public:
    typedef X86::RegisterID RegisterID;

    // Note: do not rely on values in this enum, these will change (to 0..3).
    enum Scale {
        TimesOne = 1,
        TimesTwo = 2,
        TimesFour = 4,
        TimesEight = 8
    };

    MacroAssembler()
    {
    }
    
    void* copyCode()
    {
        return m_assembler.executableCopy();
    }

    // Address:
    //
    // Describes a simple base-offset address.
    struct Address {
        explicit Address(RegisterID base, int32_t offset = 0)
            : base(base)
            , offset(offset)
        {
        }

        RegisterID base;
        int32_t offset;
    };

    // ImplicitAddress:
    //
    // This class is used for explicit 'load' and 'store' operations
    // (as opposed to situations in which a memory operand is provided
    // to a generic operation, such as an integer arithmetic instruction).
    //
    // In the case of a load (or store) operation we want to permit
    // addresses to be implicitly constructed, e.g. the two calls:
    //
    //     load32(Address(addrReg), destReg);
    //     load32(addrReg, destReg);
    //
    // Are equivalent, and the explicit wrapping of the Address in the former
    // is unnecessary.
    struct ImplicitAddress {
        ImplicitAddress(RegisterID base)
            : base(base)
            , offset(0)
        {
        }

        ImplicitAddress(Address address)
            : base(address.base)
            , offset(address.offset)
        {
        }

        RegisterID base;
        int32_t offset;
    };

    // BaseIndex:
    //
    // Describes a complex addressing mode.
    struct BaseIndex {
        BaseIndex(RegisterID base, RegisterID index, int32_t offset = 0)
            : base(base)
            , index(index)
            , scale(TimesOne)
            , offset(offset)
        {
        }

        BaseIndex(RegisterID base, RegisterID index, Scale scale, int32_t offset = 0)
            : base(base)
            , index(index)
            , scale(scale)
            , offset(offset)
        {
        }

        RegisterID base;
        RegisterID index;
        Scale scale;
        int32_t offset;
    };

    class Jump;

    // Label:
    //
    // A Label records a point in the generated instruction stream, typically such that
    // it may be used as a destination for a jump.
    class Label {
        friend class Jump;
        friend class MacroAssembler;

    public:
        Label(MacroAssembler* masm)
            : m_label(masm->m_assembler.label())
        {
        }

    private:
        X86Assembler::JmpDst m_label;
    };
    
    // Jump:
    //
    // A jump object is a reference to a jump instruction that has been planted
    // into the code buffer - it is typically used to link the jump, setting the
    // relative offset such that when executed it will jump to the desired
    // destination.
    //
    // Jump objects retain a pointer to the assembler for syntactic purposes -
    // to allow the jump object to be able to link itself, e.g.:
    //
    //     Jump forwardsBranch = jne32(Imm32(0), reg1);
    //     // ...
    //     forwardsBranch.link();
    //
    // Jumps may also be linked to a Label.
    class Jump {
    public:
        Jump()
        {
        }
        
        Jump(X86Assembler::JmpSrc jmp)
            : m_jmp(jmp)
        {
        }
        
        void link(MacroAssembler* masm)
        {
            masm->m_assembler.link(m_jmp, masm->m_assembler.label());
        }
        
        void linkTo(Label label, MacroAssembler* masm)
        {
            masm->m_assembler.link(m_jmp, label.m_label);
        }
        
        // FIXME: transitionary method, while we replace JmpSrces with Jumps.
        operator X86Assembler::JmpSrc()
        {
            return m_jmp;
        }

    private:
        X86Assembler::JmpSrc m_jmp;
    };

    // JumpList:
    //
    // A JumpList is a set of Jump objects.
    // All jumps in the set will be linked to the same destination.
    class JumpList {
    public:
        void link(MacroAssembler* masm)
        {
            size_t size = m_jumps.size();
            for (size_t i = 0; i < size; ++i)
                m_jumps[i].link(masm);
            m_jumps.clear();
        }
        
        void linkTo(Label label, MacroAssembler* masm)
        {
            size_t size = m_jumps.size();
            for (size_t i = 0; i < size; ++i)
                m_jumps[i].linkTo(label, masm);
            m_jumps.clear();
        }
        
        void append(Jump jump)
        {
            m_jumps.append(jump);
        }
        
        void append(JumpList& other)
        {
            m_jumps.append(other.m_jumps.begin(), other.m_jumps.size());
        }

    private:
        Vector<Jump> m_jumps;
    };

    // Imm32:
    //
    // A 32bit immediate operand to an instruction - this is wrapped in a
    // class requiring explicit construction in order to prevent RegisterIDs
    // (which are implemented as an enum) from accidentally being passed as
    // immediate values.
    struct Imm32 {
        explicit Imm32(int32_t value)
            : m_value(value)
        {
        }

        int32_t m_value;
    };


    // Integer arithmetic operations:
    //
    // Operations are typically two operand - operation(source, srcDst)
    // For many operations the source may be an Imm32, the srcDst operand
    // may often be a memory location (explictly described using an Address
    // object).

    void addPtr(Imm32 imm, RegisterID dest)
    {
#if PLATFORM(X86_64)
        if (CAN_SIGN_EXTEND_8_32(imm.m_value))
            m_assembler.addq_i8r(imm.m_value, dest);
        else
            m_assembler.addq_i32r(imm.m_value, dest);
#else
        if (CAN_SIGN_EXTEND_8_32(imm.m_value))
            m_assembler.addl_i8r(imm.m_value, dest);
        else
            m_assembler.addl_i32r(imm.m_value, dest);
#endif
    }

    void add32(RegisterID src, RegisterID dest)
    {
        m_assembler.addl_rr(src, dest);
    }

    void add32(Imm32 imm, RegisterID dest)
    {
        if (CAN_SIGN_EXTEND_8_32(imm.m_value))
            m_assembler.addl_i8r(imm.m_value, dest);
        else
            m_assembler.addl_i32r(imm.m_value, dest);
    }
    
    void add32(Address src, RegisterID dest)
    {
        m_assembler.addl_mr(src.offset, src.base, dest);
    }
    
    void and32(RegisterID src, RegisterID dest)
    {
        m_assembler.andl_rr(src, dest);
    }

    void lshift32(Imm32 imm, RegisterID dest)
    {
        m_assembler.shll_i8r(imm.m_value, dest);
    }
    
    void mul32(Imm32 imm, RegisterID src, RegisterID dest)
    {
        m_assembler.imull_i32r(src, imm.m_value, dest);
    }
    
    void or32(Imm32 imm, RegisterID dest)
    {
        if (CAN_SIGN_EXTEND_8_32(imm.m_value))
            m_assembler.orl_i8r(imm.m_value, dest);
        else
            m_assembler.orl_i32r(imm.m_value, dest);
    }

    void rshift32(Imm32 imm, RegisterID dest)
    {
        m_assembler.sarl_i8r(imm.m_value, dest);
    }

    void sub32(Imm32 imm, RegisterID dest)
    {
        if (CAN_SIGN_EXTEND_8_32(imm.m_value))
            m_assembler.subl_i8r(imm.m_value, dest);
        else
            m_assembler.subl_i32r(imm.m_value, dest);
    }
    
    void sub32(Address src, RegisterID dest)
    {
        m_assembler.subl_mr(src.offset, src.base, dest);
    }


    // Memory access operations:
    //
    // Loads are of the form load(address, destination) and stores of the form
    // store(source, address).  The source for a store may be an Imm32.  Address
    // operand objects to loads and store will be implicitly constructed if a
    // register is passed.

    void loadPtr(ImplicitAddress address, RegisterID dest)
    {
#if PLATFORM(X86_64)
        if (address.offset)
            m_assembler.movq_mr(address.offset, address.base, dest);
        else
            m_assembler.movq_mr(address.base, dest);
#else
        if (address.offset)
            m_assembler.movl_mr(address.offset, address.base, dest);
        else
            m_assembler.movl_mr(address.base, dest);
#endif
    }

    void load32(ImplicitAddress address, RegisterID dest)
    {
        if (address.offset)
            m_assembler.movl_mr(address.offset, address.base, dest);
        else
            m_assembler.movl_mr(address.base, dest);
    }

    void load32(BaseIndex address, RegisterID dest)
    {
        if (address.offset)
            m_assembler.movl_mr(address.offset, address.base, address.index, address.scale, dest);
        else
            m_assembler.movl_mr(address.base, address.index, address.scale, dest);
    }

    void load16(BaseIndex address, RegisterID dest)
    {
        if (address.offset)
            m_assembler.movzwl_mr(address.offset, address.base, address.index, address.scale, dest);
        else
            m_assembler.movzwl_mr(address.base, address.index, address.scale, dest);
    }

    void storePtr(RegisterID src, ImplicitAddress address)
    {
#if PLATFORM(X86_64)
        if (address.offset)
            m_assembler.movq_rm(src, address.offset, address.base);
        else
            m_assembler.movq_rm(src, address.base);
#else
        if (address.offset)
            m_assembler.movl_rm(src, address.offset, address.base);
        else
            m_assembler.movl_rm(src, address.base);
#endif
    }
    
#if !PLATFORM(X86_64)
    void storePtr(void* value, ImplicitAddress address)
    {
        if (address.offset)
            m_assembler.movl_i32m(reinterpret_cast<unsigned>(value), address.offset, address.base);
        else
            m_assembler.movl_i32m(reinterpret_cast<unsigned>(value), address.base);
    }
#endif
    
    void store32(RegisterID src, ImplicitAddress address)
    {
        if (address.offset)
            m_assembler.movl_rm(src, address.offset, address.base);
        else
            m_assembler.movl_rm(src, address.base);
    }
    
    void store32(Imm32 imm, ImplicitAddress address)
    {
        if (address.offset)
            m_assembler.movl_i32m(imm.m_value, address.offset, address.base);
        else
            m_assembler.movl_i32m(imm.m_value, address.base);
    }
    
#if !PLATFORM(X86_64)
    void store32(Imm32 imm, void* address)
    {
        m_assembler.movl_i32m(imm.m_value, address);
    }
#endif


    // Stack manipulation operations:
    //
    // The ABI is assumed to provide a stack abstraction to memory,
    // containing machine word sized units of data.  Push and pop
    // operations add and remove a single register sized unit of data
    // to or from the stack.  Peek and poke operations read or write
    // values on the stack, without moving the current stack position.
    
    void pop(RegisterID dest)
    {
#if PLATFORM(X86_64)
        m_assembler.popq_r(dest);
#else
        m_assembler.popl_r(dest);
#endif
    }

    void push(RegisterID src)
    {
#if PLATFORM(X86_64)
        m_assembler.pushq_r(src);
#else
        m_assembler.pushl_r(src);
#endif
    }

    void pop()
    {
        addPtr(Imm32(sizeof(void*)), X86::esp);
    }
    
    void peek(RegisterID dest, int index = 0)
    {
        loadPtr(Address(X86::esp, (index * sizeof(void *))), dest);
    }

    void poke(RegisterID src, int index = 0)
    {
        storePtr(src, Address(X86::esp, (index * sizeof(void *))));
    }

    void poke(Imm32 value, int index = 0)
    {
        store32(value, Address(X86::esp, (index * sizeof(void *))));
    }

#if !PLATFORM(X86_64)
    void poke(void* value, int index = 0)
    {
        storePtr(value, Address(X86::esp, (index * sizeof(void *))));
    }
#endif

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

    void move(RegisterID src, RegisterID dest)
    {
        // Note: on 64-bit this is is a full register move; perhaps it would be
        // useful to have separate move32 & movePtr, with move32 zero extending?
#if PLATFORM(X86_64)
        m_assembler.movq_rr(src, dest);
#else
        m_assembler.movl_rr(src, dest);
#endif
    }

#if !PLATFORM(X86_64)
    void move(void* value, RegisterID dest)
    {
        m_assembler.movl_i32r(reinterpret_cast<int32_t>(value), dest);
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

private:
    void compareImm32ForBranch(RegisterID left, int32_t right)
    {
        if (CAN_SIGN_EXTEND_8_32(right))
            m_assembler.cmpl_i8r(right, left);
        else
            m_assembler.cmpl_i32r(right, left);
    }

    void compareImm32ForBranchEquality(RegisterID reg, int32_t imm)
    {
        if (!imm)
            m_assembler.testl_rr(reg, reg);
        else if (CAN_SIGN_EXTEND_8_32(imm))
            m_assembler.cmpl_i8r(imm, reg);
        else
            m_assembler.cmpl_i32r(imm, reg);
    }

    void compareImm32ForBranchEquality(Address address, int32_t imm)
    {
        if (CAN_SIGN_EXTEND_8_32(imm)) {
            if (address.offset)
                m_assembler.cmpl_i8m(imm, address.offset, address.base);
            else
                m_assembler.cmpl_i8m(imm, address.base);
        } else {
            if (address.offset)
                m_assembler.cmpl_i32m(imm, address.offset, address.base);
            else
                m_assembler.cmpl_i32m(imm, address.base);
        }
    }

public:
    Jump jae32(RegisterID left, Imm32 right)
    {
        compareImm32ForBranch(left, right.m_value);
        return Jump(m_assembler.jae());
    }
    
    Jump je32(RegisterID op1, RegisterID op2)
    {
        m_assembler.cmpl_rr(op1, op2);
        return Jump(m_assembler.je());
    }
    
    Jump je32(RegisterID op1, Address op2)
    {
        m_assembler.cmpl_rm(op1, op2.offset, op2.base);
        return Jump(m_assembler.je());
    }
    
    Jump je32(Imm32 imm, RegisterID reg)
    {
        compareImm32ForBranchEquality(reg, imm.m_value);
        return Jump(m_assembler.je());
    }
    
    Jump je16(RegisterID op1, BaseIndex op2)
    {
        if (op2.offset)
            m_assembler.cmpw_rm(op1, op2.base, op2.index, op2.scale);
        else
            m_assembler.cmpw_rm(op1, op2.offset, op2.base, op2.index, op2.scale);

        return Jump(m_assembler.je());
    }
    
    Jump jg32(RegisterID left, RegisterID right)
    {
        m_assembler.cmpl_rr(right, left);
        return Jump(m_assembler.jg());
    }
    
    Jump jge32(RegisterID left, RegisterID right)
    {
        m_assembler.cmpl_rr(right, left);
        return Jump(m_assembler.jge());
    }

    Jump jge32(RegisterID left, Imm32 right)
    {
        compareImm32ForBranch(left, right.m_value);
        return Jump(m_assembler.jge());
    }

    Jump jl32(RegisterID left, RegisterID right)
    {
        m_assembler.cmpl_rr(right, left);
        return Jump(m_assembler.jl());
    }
    
    Jump jl32(RegisterID left, Imm32 right)
    {
        compareImm32ForBranch(left, right.m_value);
        return Jump(m_assembler.jl());
    }

    Jump jle32(RegisterID left, RegisterID right)
    {
        m_assembler.cmpl_rr(right, left);
        return Jump(m_assembler.jle());
    }
    
    Jump jle32(RegisterID left, Imm32 right)
    {
        compareImm32ForBranch(left, right.m_value);
        return Jump(m_assembler.jle());
    }

#if !PLATFORM(X86_64)
    Jump jnePtr(void* ptr, Address address)
    {
        compareImm32ForBranchEquality(address, reinterpret_cast<uint32_t>(ptr));
        return Jump(m_assembler.jne());
    }
#endif

    Jump jne32(RegisterID op1, RegisterID op2)
    {
        m_assembler.cmpl_rr(op1, op2);
        return Jump(m_assembler.jne());
    }

    Jump jne32(Imm32 imm, RegisterID reg)
    {
        compareImm32ForBranchEquality(reg, imm.m_value);
        return Jump(m_assembler.jne());
    }

    Jump jnset32(Imm32 imm, RegisterID reg)
    {
        // if we are only interested in the low seven bits, this can be tested with a testb
        if ((imm.m_value & ~0x7f) == 0)
            m_assembler.testb_i8r(imm.m_value, reg);
        else
            m_assembler.testl_i32r(imm.m_value, reg);
        return Jump(m_assembler.je());
    }

    Jump jset32(Imm32 imm, RegisterID reg)
    {
        // if we are only interested in the low seven bits, this can be tested with a testb
        if ((imm.m_value & ~0x7f) == 0)
            m_assembler.testb_i8r(imm.m_value, reg);
        else
            m_assembler.testl_i32r(imm.m_value, reg);
        return Jump(m_assembler.jne());
    }

    Jump jump()
    {
        return Jump(m_assembler.jmp());
    }


    // Backwards, local control flow operations:
    //
    // These operations provide a shorter notation for local
    // backwards branches, which may be both more convenient
    // for the user, and for the programmer, and for the
    // assembler (allowing shorter values to be used in
    // relative offsets).
    //
    // The code sequence:
    //
    //     Label topOfLoop(this);
    //     // ...
    //     jne32(reg1, reg2, topOfLoop);
    //
    // Is equivalent to the longer, potentially less efficient form:
    //
    //     Label topOfLoop(this);
    //     // ...
    //     jne32(reg1, reg2).linkTo(topOfLoop);

    void je32(Imm32 imm, RegisterID op2, Label target)
    {
        je32(imm, op2).linkTo(target, this);
    }

    void je16(RegisterID op1, BaseIndex op2, Label target)
    {
        je16(op1, op2).linkTo(target, this);
    }
    
    void jl32(RegisterID left, Imm32 right, Label target)
    {
        jl32(left, right).linkTo(target, this);
    }
    
    void jle32(RegisterID left, RegisterID right, Label target)
    {
        jle32(left, right).linkTo(target, this);
    }
    
    void jne32(RegisterID op1, RegisterID op2, Label target)
    {
        jne32(op1, op2).linkTo(target, this);
    }

    void jne32(Imm32 imm, RegisterID op2, Label target)
    {
        jne32(imm, op2).linkTo(target, this);
    }

    void jump(Label target)
    {
        m_assembler.link(m_assembler.jmp(), target.m_label);
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

    Jump jnzSub32(Imm32 imm, RegisterID dest)
    {
        sub32(imm, dest);
        return Jump(m_assembler.jne());
    }
    
    Jump joAdd32(RegisterID src, RegisterID dest)
    {
        add32(src, dest);
        return Jump(m_assembler.jo());
    }
    
    Jump joAdd32(Imm32 imm, RegisterID dest)
    {
        add32(imm, dest);
        return Jump(m_assembler.jo());
    }
    
    Jump joMul32(Imm32 imm, RegisterID src, RegisterID dest)
    {
        mul32(imm, src, dest);
        return Jump(m_assembler.jo());
    }
    
    Jump jzSub32(Imm32 imm, RegisterID dest)
    {
        sub32(imm, dest);
        return Jump(m_assembler.je());
    }
    

    // Miscellaneous operations:

    void breakpoint()
    {
        m_assembler.int3();
    }

    Jump call()
    {
        return Jump(m_assembler.call());
    }

    Jump call(RegisterID target)
    {
        return Jump(m_assembler.call(target));
    }

    void ret()
    {
        m_assembler.ret();
    }
};

} // namespace JSC

#endif // ENABLE(ASSEMBLER)

#endif // MacroAssembler_h
