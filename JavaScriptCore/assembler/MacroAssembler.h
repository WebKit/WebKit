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

#if PLATFORM(X86_64)
    static const X86::RegisterID scratchRegister = X86::r11;
#endif

public:
    typedef X86::RegisterID RegisterID;

    // Note: do not rely on values in this enum, these will change (to 0..3).
    enum Scale {
        TimesOne,
        TimesTwo,
        TimesFour,
        TimesEight,
#if PLATFORM(X86)
        ScalePtr = TimesFour
#endif
#if PLATFORM(X86_64)
        ScalePtr = TimesEight
#endif
    };

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

    MacroAssembler()
    {
    }
    
    size_t size() { return m_assembler.size(); }
    void* copyCode(ExecutablePool* allocator)
    {
        return m_assembler.executableCopy(allocator);
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

    // AbsoluteAddress:
    //
    // Describes an memory operand given by a pointer.  For regular load & store
    // operations an unwrapped void* will be used, rather than using this.
    struct AbsoluteAddress {
        explicit AbsoluteAddress(void* ptr)
            : m_ptr(ptr)
        {
        }

        void* m_ptr;
    };


    class Jump;
    class PatchBuffer;

    // DataLabelPtr:
    //
    // A DataLabelPtr is used to refer to a location in the code containing a pointer to be
    // patched after the code has been generated.
    class DataLabelPtr {
        friend class MacroAssembler;
        friend class PatchBuffer;

    public:
        DataLabelPtr()
        {
        }

        DataLabelPtr(MacroAssembler* masm)
            : m_label(masm->m_assembler.label())
        {
        }

        static void patch(void* address, void* value)
        {
            X86Assembler::patchPointer(reinterpret_cast<intptr_t>(address), reinterpret_cast<intptr_t>(value));
        }
        
    private:
        X86Assembler::JmpDst m_label;
    };

    // DataLabel32:
    //
    // A DataLabelPtr is used to refer to a location in the code containing a pointer to be
    // patched after the code has been generated.
    class DataLabel32 {
        friend class MacroAssembler;
        friend class PatchBuffer;

    public:
        DataLabel32()
        {
        }

        DataLabel32(MacroAssembler* masm)
            : m_label(masm->m_assembler.label())
        {
        }

        static void patch(void* address, int32_t value)
        {
            X86Assembler::patchImmediate(reinterpret_cast<intptr_t>(address), value);
        }

    private:
        X86Assembler::JmpDst m_label;
    };

    // Label:
    //
    // A Label records a point in the generated instruction stream, typically such that
    // it may be used as a destination for a jump.
    class Label {
        friend class Jump;
        friend class MacroAssembler;
        friend class PatchBuffer;

    public:
        Label()
        {
        }

        Label(MacroAssembler* masm)
            : m_label(masm->m_assembler.label())
        {
        }
        
        // FIXME: transitionary method, while we replace JmpSrces with Jumps.
        operator X86Assembler::JmpDst()
        {
            return m_label;
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
        friend class PatchBuffer;
        friend class MacroAssembler;

    public:
        Jump()
        {
        }
        
        // FIXME: transitionary method, while we replace JmpSrces with Jumps.
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

        static void patch(void* address, void* destination)
        {
            X86Assembler::patchBranchOffset(reinterpret_cast<intptr_t>(address), destination);
        }

    private:
        X86Assembler::JmpSrc m_jmp;
    };

    // JumpList:
    //
    // A JumpList is a set of Jump objects.
    // All jumps in the set will be linked to the same destination.
    class JumpList {
        friend class PatchBuffer;

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

        bool empty()
        {
            return !m_jumps.size();
        }

    private:
        Vector<Jump, 16> m_jumps;
    };


    // PatchBuffer:
    //
    // This class assists in linking code generated by the macro assembler, once code generation
    // has been completed, and the code has been copied to is final location in memory.  At this
    // time pointers to labels within the code may be resolved, and relative offsets to external
    // addresses may be fixed.
    //
    // Specifically:
    //   * Jump objects may be linked to external targets,
    //   * The address of Jump objects may taken, such that it can later be relinked.
    //   * The return address of a Jump object representing a call may be acquired.
    //   * The address of a Label pointing into the code may be resolved.
    //   * The value referenced by a DataLabel may be fixed.
    //
    // FIXME: distinguish between Calls & Jumps (make a specific call to obtain the return
    // address of calls, as opposed to a point that can be used to later relink a Jump -
    // possibly wrap the later up in an object that can do just that).
    class PatchBuffer {
    public:
        PatchBuffer(void* code)
            : m_code(code)
        {
        }

        void link(Jump jump, void* target)
        {
            X86Assembler::link(m_code, jump.m_jmp, target);
        }

        void link(JumpList list, void* target)
        {
            for (unsigned i = 0; i < list.m_jumps.size(); ++i)
                X86Assembler::link(m_code, list.m_jumps[i], target);
        }

        void* addressOf(Jump jump)
        {
            return X86Assembler::getRelocatedAddress(m_code, jump.m_jmp);
        }

        void* addressOf(Label label)
        {
            return X86Assembler::getRelocatedAddress(m_code, label.m_label);
        }

        void* addressOf(DataLabelPtr label)
        {
            return X86Assembler::getRelocatedAddress(m_code, label.m_label);
        }

        void* addressOf(DataLabel32 label)
        {
            return X86Assembler::getRelocatedAddress(m_code, label.m_label);
        }

        void setPtr(DataLabelPtr label, void* value)
        {
            X86Assembler::patchAddress(m_code, label.m_label, value);
        }

    private:
        void* m_code;
    };
 

    // ImmPtr:
    //
    // A pointer sized immediate operand to an instruction - this is wrapped
    // in a class requiring explicit construction in order to differentiate
    // from pointers used as absolute addresses to memory operations
    struct ImmPtr {
        explicit ImmPtr(void* value)
            : m_value(value)
        {
        }

        intptr_t asIntptr()
        {
            return reinterpret_cast<intptr_t>(m_value);
        }

        void* m_value;
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

#if PLATFORM(X86)
        explicit Imm32(ImmPtr ptr)
            : m_value(ptr.asIntptr())
        {
        }
#endif

        int32_t m_value;
    };

    // Integer arithmetic operations:
    //
    // Operations are typically two operand - operation(source, srcDst)
    // For many operations the source may be an Imm32, the srcDst operand
    // may often be a memory location (explictly described using an Address
    // object).

    void addPtr(RegisterID src, RegisterID dest)
    {
#if PLATFORM(X86_64)
        m_assembler.addq_rr(src, dest);
#else
        add32(src, dest);
#endif
    }

    void addPtr(Imm32 imm, RegisterID srcDest)
    {
#if PLATFORM(X86_64)
        m_assembler.addq_ir(imm.m_value, srcDest);
#else
        add32(imm, srcDest);
#endif
    }

    void addPtr(ImmPtr imm, RegisterID dest)
    {
#if PLATFORM(X86_64)
        move(imm, scratchRegister);
        m_assembler.addq_rr(scratchRegister, dest);
#else
        add32(Imm32(imm), dest);
#endif
    }

    void addPtr(Imm32 imm, RegisterID src, RegisterID dest)
    {
        m_assembler.leal_mr(imm.m_value, src, dest);
    }

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
    
    void add32(Imm32 imm, AbsoluteAddress address)
    {
#if PLATFORM(X86_64)
        move(ImmPtr(address.m_ptr), scratchRegister);
        add32(imm, Address(scratchRegister));
#else
        m_assembler.addl_im(imm.m_value, address.m_ptr);
#endif
    }
    
    void add32(Address src, RegisterID dest)
    {
        m_assembler.addl_mr(src.offset, src.base, dest);
    }
    
    void andPtr(RegisterID src, RegisterID dest)
    {
#if PLATFORM(X86_64)
        m_assembler.andq_rr(src, dest);
#else
        and32(src, dest);
#endif
    }

    void andPtr(Imm32 imm, RegisterID srcDest)
    {
#if PLATFORM(X86_64)
        m_assembler.andq_ir(imm.m_value, srcDest);
#else
        and32(imm, srcDest);
#endif
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
    
    void orPtr(RegisterID src, RegisterID dest)
    {
#if PLATFORM(X86_64)
        m_assembler.orq_rr(src, dest);
#else
        or32(src, dest);
#endif
    }

    void orPtr(ImmPtr imm, RegisterID dest)
    {
#if PLATFORM(X86_64)
        move(imm, scratchRegister);
        m_assembler.orq_rr(scratchRegister, dest);
#else
        or32(Imm32(imm), dest);
#endif
    }

    void orPtr(Imm32 imm, RegisterID dest)
    {
#if PLATFORM(X86_64)
        m_assembler.orq_ir(imm.m_value, dest);
#else
        or32(imm, dest);
#endif
    }

    void or32(RegisterID src, RegisterID dest)
    {
        m_assembler.orl_rr(src, dest);
    }

    void or32(Imm32 imm, RegisterID dest)
    {
        m_assembler.orl_ir(imm.m_value, dest);
    }

    void rshiftPtr(RegisterID shift_amount, RegisterID dest)
    {
#if PLATFORM(X86_64)
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
#else
        rshift32(shift_amount, dest);
#endif
    }

    void rshiftPtr(Imm32 imm, RegisterID dest)
    {
#if PLATFORM(X86_64)
        m_assembler.sarq_i8r(imm.m_value, dest);
#else
        rshift32(imm, dest);
#endif
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

    void subPtr(RegisterID src, RegisterID dest)
    {
#if PLATFORM(X86_64)
        m_assembler.subq_rr(src, dest);
#else
        sub32(src, dest);
#endif
    }
    
    void subPtr(Imm32 imm, RegisterID dest)
    {
#if PLATFORM(X86_64)
        m_assembler.subq_ir(imm.m_value, dest);
#else
        sub32(imm, dest);
#endif
    }
    
    void subPtr(ImmPtr imm, RegisterID dest)
    {
#if PLATFORM(X86_64)
        move(imm, scratchRegister);
        m_assembler.subq_rr(scratchRegister, dest);
#else
        sub32(Imm32(imm), dest);
#endif
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

    void sub32(Imm32 imm, AbsoluteAddress address)
    {
#if PLATFORM(X86_64)
        move(ImmPtr(address.m_ptr), scratchRegister);
        sub32(imm, Address(scratchRegister));
#else
        m_assembler.subl_im(imm.m_value, address.m_ptr);
#endif
    }

    void sub32(Address src, RegisterID dest)
    {
        m_assembler.subl_mr(src.offset, src.base, dest);
    }

    void xorPtr(RegisterID src, RegisterID dest)
    {
#if PLATFORM(X86_64)
        m_assembler.xorq_rr(src, dest);
#else
        xor32(src, dest);
#endif
    }

    void xorPtr(Imm32 imm, RegisterID srcDest)
    {
#if PLATFORM(X86_64)
        m_assembler.xorq_ir(imm.m_value, srcDest);
#else
        xor32(imm, srcDest);
#endif
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

    void loadPtr(ImplicitAddress address, RegisterID dest)
    {
#if PLATFORM(X86_64)
        m_assembler.movq_mr(address.offset, address.base, dest);
#else
        load32(address, dest);
#endif
    }

    DataLabel32 loadPtrWithAddressOffsetPatch(Address address, RegisterID dest)
    {
#if PLATFORM(X86_64)
        m_assembler.movq_mr_disp32(address.offset, address.base, dest);
        return DataLabel32(this);
#else
        m_assembler.movl_mr_disp32(address.offset, address.base, dest);
        return DataLabel32(this);
#endif
    }

    void loadPtr(BaseIndex address, RegisterID dest)
    {
#if PLATFORM(X86_64)
        m_assembler.movq_mr(address.offset, address.base, address.index, address.scale, dest);
#else
        load32(address, dest);
#endif
    }

    void loadPtr(void* address, RegisterID dest)
    {
#if PLATFORM(X86_64)
        if (dest == X86::eax)
            m_assembler.movq_mEAX(address);
        else {
            move(X86::eax, dest);
            m_assembler.movq_mEAX(address);
            swap(X86::eax, dest);
        }
#else
        load32(address, dest);
#endif
    }

    void load32(ImplicitAddress address, RegisterID dest)
    {
        m_assembler.movl_mr(address.offset, address.base, dest);
    }

    void load32(BaseIndex address, RegisterID dest)
    {
        m_assembler.movl_mr(address.offset, address.base, address.index, address.scale, dest);
    }

    void load32(void* address, RegisterID dest)
    {
#if PLATFORM(X86_64)
        if (dest == X86::eax)
            m_assembler.movl_mEAX(address);
        else {
            move(X86::eax, dest);
            m_assembler.movl_mEAX(address);
            swap(X86::eax, dest);
        }
#else
        m_assembler.movl_mr(address, dest);
#endif
    }

    void load16(BaseIndex address, RegisterID dest)
    {
        m_assembler.movzwl_mr(address.offset, address.base, address.index, address.scale, dest);
    }

    void storePtr(RegisterID src, ImplicitAddress address)
    {
#if PLATFORM(X86_64)
        m_assembler.movq_rm(src, address.offset, address.base);
#else
        store32(src, address);
#endif
    }

    DataLabel32 storePtrWithAddressOffsetPatch(RegisterID src, Address address)
    {
#if PLATFORM(X86_64)
        m_assembler.movq_rm_disp32(src, address.offset, address.base);
        return DataLabel32(this);
#else
        m_assembler.movl_rm_disp32(src, address.offset, address.base);
        return DataLabel32(this);
#endif
    }

    void storePtr(RegisterID src, BaseIndex address)
    {
#if PLATFORM(X86_64)
        m_assembler.movq_rm(src, address.offset, address.base, address.index, address.scale);
#else
        store32(src, address);
#endif
    }

    void storePtr(ImmPtr imm, ImplicitAddress address)
    {
#if PLATFORM(X86_64)
        move(imm, scratchRegister);
        storePtr(scratchRegister, address);
#else
        m_assembler.movl_i32m(imm.asIntptr(), address.offset, address.base);
#endif
    }

#if !PLATFORM(X86_64)
    void storePtr(ImmPtr imm, void* address)
    {
        store32(Imm32(imm), address);
    }
#endif

    DataLabelPtr storePtrWithPatch(Address address)
    {
#if PLATFORM(X86_64)
        m_assembler.movq_i64r(0, scratchRegister);
        DataLabelPtr label(this);
        storePtr(scratchRegister, address);
        return label;
#else
        m_assembler.movl_i32m(0, address.offset, address.base);
        return DataLabelPtr(this);
#endif
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
    
    void store32(Imm32 imm, void* address)
    {
#if PLATFORM(X86_64)
        move(X86::eax, scratchRegister);
        move(imm, X86::eax);
        m_assembler.movl_EAXm(address);
        move(scratchRegister, X86::eax);
#else
        m_assembler.movl_i32m(imm.m_value, address);
#endif
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

    void poke(ImmPtr imm, int index = 0)
    {
        storePtr(imm, Address(X86::esp, (index * sizeof(void *))));
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

    void move(ImmPtr imm, RegisterID dest)
    {
#if PLATFORM(X86_64)
        if (CAN_SIGN_EXTEND_U32_64(imm.asIntptr()))
            m_assembler.movl_i32r(static_cast<int32_t>(imm.asIntptr()), dest);
        else
            m_assembler.movq_i64r(imm.asIntptr(), dest);
#else
        m_assembler.movl_i32r(imm.asIntptr(), dest);
#endif
    }

    void swap(RegisterID reg1, RegisterID reg2)
    {
#if PLATFORM(X86_64)
        m_assembler.xchgq_rr(reg1, reg2);
#else
        m_assembler.xchgl_rr(reg1, reg2);
#endif
    }

    void signExtend32ToPtr(RegisterID src, RegisterID dest)
    {
#if PLATFORM(X86_64)
        m_assembler.movsxd_rr(src, dest);
#else
        if (src != dest)
            move(src, dest);
#endif
    }

    void zeroExtend32ToPtr(RegisterID src, RegisterID dest)
    {
#if PLATFORM(X86_64)
        m_assembler.movl_rr(src, dest);
#else
        if (src != dest)
            move(src, dest);
#endif
    }


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
    Jump branchPtr(Condition cond, RegisterID left, RegisterID right)
    {
#if PLATFORM(X86_64)
        m_assembler.cmpq_rr(right, left);
        return Jump(m_assembler.jCC(cond));
#else
        return branch32(cond, left, right);
#endif
    }

    Jump branchPtr(Condition cond, RegisterID left, ImmPtr right)
    {
#if PLATFORM(X86_64)
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
#else
        return branch32(cond, left, Imm32(right));
#endif
    }

    Jump branchPtr(Condition cond, RegisterID left, Address right)
    {
#if PLATFORM(X86_64)
        m_assembler.cmpq_mr(right.offset, right.base, left);
        return Jump(m_assembler.jCC(cond));
#else
        return branch32(cond, left, right);
#endif
    }

    Jump branchPtr(Condition cond, AbsoluteAddress left, RegisterID right)
    {
#if PLATFORM(X86_64)
        move(ImmPtr(left.m_ptr), scratchRegister);
        return branchPtr(cond, Address(scratchRegister), right);
#else
        m_assembler.cmpl_rm(right, left.m_ptr);
        return Jump(m_assembler.jCC(cond));
#endif
    }

    Jump branchPtr(Condition cond, Address left, RegisterID right)
    {
#if PLATFORM(X86_64)
        m_assembler.cmpq_rm(right, left.offset, left.base);
        return Jump(m_assembler.jCC(cond));
#else
        return branch32(cond, left, right);
#endif
    }

    Jump branchPtr(Condition cond, Address left, ImmPtr right)
    {
#if PLATFORM(X86_64)
        move(right, scratchRegister);
        return branchPtr(cond, left, scratchRegister);
#else
        return branch32(cond, left, Imm32(right));
#endif
    }

#if !PLATFORM(X86_64)
    Jump branchPtr(Condition cond, AbsoluteAddress left, ImmPtr right)
    {
        m_assembler.cmpl_im(right.asIntptr(), left.m_ptr);
        return Jump(m_assembler.jCC(cond));
    }
#endif

    Jump branchPtrWithPatch(Condition cond, RegisterID left, DataLabelPtr& dataLabel, ImmPtr initialRightValue = ImmPtr(0))
    {
#if PLATFORM(X86_64)
        m_assembler.movq_i64r(initialRightValue.asIntptr(), scratchRegister);
        dataLabel = DataLabelPtr(this);
        return branchPtr(cond, left, scratchRegister);
#else
        m_assembler.cmpl_ir_force32(initialRightValue.asIntptr(), left);
        dataLabel = DataLabelPtr(this);
        return Jump(m_assembler.jCC(cond));
#endif
    }

    Jump branchPtrWithPatch(Condition cond, Address left, DataLabelPtr& dataLabel, ImmPtr initialRightValue = ImmPtr(0))
    {
#if PLATFORM(X86_64)
        m_assembler.movq_i64r(initialRightValue.asIntptr(), scratchRegister);
        dataLabel = DataLabelPtr(this);
        return branchPtr(cond, left, scratchRegister);
#else
        m_assembler.cmpl_im_force32(initialRightValue.asIntptr(), left.offset, left.base);
        dataLabel = DataLabelPtr(this);
        return Jump(m_assembler.jCC(cond));
#endif
    }

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

    Jump branchTestPtr(Condition cond, RegisterID reg, RegisterID mask)
    {
#if PLATFORM(X86_64)
        m_assembler.testq_rr(reg, mask);
        return Jump(m_assembler.jCC(cond));
#else
        return branchTest32(cond, reg, mask);
#endif
    }

    Jump branchTestPtr(Condition cond, RegisterID reg, Imm32 mask = Imm32(-1))
    {
#if PLATFORM(X86_64)
        // if we are only interested in the low seven bits, this can be tested with a testb
        if (mask.m_value == -1)
            m_assembler.testq_rr(reg, reg);
        else if ((mask.m_value & ~0x7f) == 0)
            m_assembler.testb_i8r(mask.m_value, reg);
        else
            m_assembler.testq_i32r(mask.m_value, reg);
        return Jump(m_assembler.jCC(cond));
#else
        return branchTest32(cond, reg, mask);
#endif
    }

    Jump branchTestPtr(Condition cond, Address address, Imm32 mask = Imm32(-1))
    {
#if PLATFORM(X86_64)
        if (mask.m_value == -1)
            m_assembler.cmpq_im(0, address.offset, address.base);
        else
            m_assembler.testq_i32m(mask.m_value, address.offset, address.base);
        return Jump(m_assembler.jCC(cond));
#else
        return branchTest32(cond, address, mask);
#endif
    }

    Jump branchTestPtr(Condition cond, BaseIndex address, Imm32 mask = Imm32(-1))
    {
#if PLATFORM(X86_64)
        if (mask.m_value == -1)
            m_assembler.cmpq_im(0, address.offset, address.base, address.index, address.scale);
        else
            m_assembler.testq_i32m(mask.m_value, address.offset, address.base, address.index, address.scale);
        return Jump(m_assembler.jCC(cond));
#else
        return branchTest32(cond, address, mask);
#endif
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

    void branchPtr(Condition cond, RegisterID op1, ImmPtr imm, Label target)
    {
        branchPtr(cond, op1, imm).linkTo(target, this);
    }

    void branch32(Condition cond, RegisterID op1, RegisterID op2, Label target)
    {
        branch32(cond, op1, op2).linkTo(target, this);
    }

    void branch32(Condition cond, RegisterID op1, Imm32 imm, Label target)
    {
        branch32(cond, op1, imm).linkTo(target, this);
    }

    void branch32(Condition cond, RegisterID left, Address right, Label target)
    {
        branch32(cond, left, right).linkTo(target, this);
    }

    void branch16(Condition cond, BaseIndex left, RegisterID right, Label target)
    {
        branch16(cond, left, right).linkTo(target, this);
    }
    
    void branchTestPtr(Condition cond, RegisterID reg, Label target)
    {
        branchTestPtr(cond, reg).linkTo(target, this);
    }

    void jump(Label target)
    {
        m_assembler.link(m_assembler.jmp(), target.m_label);
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

    Jump branchAddPtr(Condition cond, RegisterID src, RegisterID dest)
    {
        ASSERT((cond == Overflow) || (cond == Zero) || (cond == NonZero));
        addPtr(src, dest);
        return Jump(m_assembler.jCC(cond));
    }
    
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
    
    Jump branchSubPtr(Condition cond, Imm32 imm, RegisterID dest)
    {
        ASSERT((cond == Overflow) || (cond == Zero) || (cond == NonZero));
        subPtr(imm, dest);
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

    Jump call()
    {
        return Jump(m_assembler.call());
    }

    // FIXME: why does this return a Jump object? - it can't be linked.
    // This may be to get a reference to the return address of the call.
    //
    // This should probably be handled by a separate label type to a regular
    // jump.  Todo: add a CallLabel type, for the regular call - can be linked
    // like a jump (possibly a subclass of jump?, or possibly casts to a Jump).
    // Also add a CallReturnLabel type for this to return (just a more JmpDsty
    // form of label, can get the void* after the code has been linked, but can't
    // try to link it like a Jump object), and let the CallLabel be cast into a
    // CallReturnLabel.
    Jump call(RegisterID target)
    {
        return Jump(m_assembler.call(target));
    }

    Label label()
    {
        return Label(this);
    }
    
    Label align()
    {
        m_assembler.align(16);
        return Label(this);
    }

    ptrdiff_t differenceBetween(Label from, Jump to)
    {
        return X86Assembler::getDifferenceBetweenLabels(from.m_label, to.m_jmp);
    }

    ptrdiff_t differenceBetween(Label from, Label to)
    {
        return X86Assembler::getDifferenceBetweenLabels(from.m_label, to.m_label);
    }

    ptrdiff_t differenceBetween(Label from, DataLabelPtr to)
    {
        return X86Assembler::getDifferenceBetweenLabels(from.m_label, to.m_label);
    }

    ptrdiff_t differenceBetween(Label from, DataLabel32 to)
    {
        return X86Assembler::getDifferenceBetweenLabels(from.m_label, to.m_label);
    }

    ptrdiff_t differenceBetween(DataLabelPtr from, Jump to)
    {
        return X86Assembler::getDifferenceBetweenLabels(from.m_label, to.m_jmp);
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

#endif // MacroAssembler_h
