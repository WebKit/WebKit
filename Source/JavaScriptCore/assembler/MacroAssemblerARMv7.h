/*
 * Copyright (C) 2009-2022 Apple Inc. All rights reserved.
 * Copyright (C) 2010 University of Szeged
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

#if ENABLE(ASSEMBLER)

#include <initializer_list>

#include "ARMv7Assembler.h"
#include "AbstractMacroAssembler.h"

namespace JSC {

using Assembler = TARGET_ASSEMBLER;

class MacroAssemblerARMv7 : public AbstractMacroAssembler<Assembler> {
public:
    static constexpr size_t nearJumpRange = 16 * MB;

    static constexpr RegisterID dataTempRegister = ARMRegisters::ip;
    static constexpr RegisterID addressTempRegister = ARMRegisters::r6;

    // d15 is host/C ABI callee save, but is volatile in the VM/JS ABI. We use
    // this as scratch register so we can use the full range of d0-d7 as
    // temporary, and in particular as Wasm argument/return register.
    static constexpr ARMRegisters::FPDoubleRegisterID fpTempRegister = ARMRegisters::d15;
private:
    inline ARMRegisters::FPSingleRegisterID fpTempRegisterAsSingle() { return ARMRegisters::asSingle(fpTempRegister); }

    // In the Thumb-2 instruction set, instructions operating only on registers r0-r7 can often
    // be encoded using 16-bit encodings, while the use of registers r8 and above often require
    // 32-bit encodings, so prefer to use the addressTemporary (r6) whenever possible.
    inline RegisterID bestTempRegister(RegisterID excluded)
    {
        if (excluded == addressTempRegister)
            return dataTempRegister;
        return addressTempRegister;
    }

public:
    template<typename MacroAssemblerType, typename Condition, typename ...Args>
        friend void JSC::MacroAssemblerHelpers::load8OnCondition(MacroAssemblerType&, Condition, Args...);
    template<typename MacroAssemblerType, typename Condition, typename ...Args>
        friend void JSC::MacroAssemblerHelpers::load16OnCondition(MacroAssemblerType&, Condition, Args...);

    struct BoundsNonDoubleWordOffset {
        static bool within(intptr_t value)
        {
            return (value >= -0xff) && (value <= 0xfff);
        }
    };
    struct BoundsDoubleWordOffset {
        static bool within(intptr_t value)
        {
            if (value < 0)
                value = -value;
            return !(value & ~0x3fc);
        }
    };
#define DUMMY_REGISTER_VALUE(id, name, r, cs) 0,
    static constexpr unsigned numGPRs = std::initializer_list<int>({ FOR_EACH_GP_REGISTER(DUMMY_REGISTER_VALUE) }).size();
    static constexpr unsigned numFPRs = std::initializer_list<int>({ FOR_EACH_FP_REGISTER(DUMMY_REGISTER_VALUE) }).size();
#undef DUMMY_REGISTER_VALUE
    static constexpr RegisterID s_scratchRegister = addressTempRegister;
    RegisterID scratchRegister()
    {
        RELEASE_ASSERT(m_allowScratchRegister);
        return s_scratchRegister;
    }

    MacroAssemblerARMv7()
        : m_cachedDataTempRegister(this, dataTempRegister)
        , m_cachedAddressTempRegister(this, addressTempRegister)
    {
    }

    typedef ARMv7Assembler::LinkRecord LinkRecord;
    typedef ARMv7Assembler::JumpType JumpType;
    typedef ARMv7Assembler::JumpLinkType JumpLinkType;
    typedef ARMv7Assembler::Condition Condition;

    static constexpr ARMv7Assembler::Condition DefaultCondition = ARMv7Assembler::ConditionInvalid;
    static constexpr ARMv7Assembler::JumpType DefaultJump = ARMv7Assembler::JumpNoConditionFixedSize;

    static bool isCompactPtrAlignedAddressOffset(ptrdiff_t value)
    {
        return value >= -255 && value <= 255;
    }

    Vector<LinkRecord, 0, UnsafeVectorOverflow>& jumpsToLink() { return m_assembler.jumpsToLink(); }
    static bool canCompact(JumpType jumpType) { return ARMv7Assembler::canCompact(jumpType); }
    static JumpLinkType computeJumpType(JumpType jumpType, const uint8_t* from, const uint8_t* to) { return ARMv7Assembler::computeJumpType(jumpType, from, to); }
    static JumpLinkType computeJumpType(LinkRecord& record, const uint8_t* from, const uint8_t* to) { return ARMv7Assembler::computeJumpType(record, from, to); }
    static int jumpSizeDelta(JumpType jumpType, JumpLinkType jumpLinkType) { return ARMv7Assembler::jumpSizeDelta(jumpType, jumpLinkType); }

    template <Assembler::CopyFunction copy>
    ALWAYS_INLINE static void link(LinkRecord& record, uint8_t* from, const uint8_t* fromInstruction, uint8_t* to) { return ARMv7Assembler::link<copy>(record, from, fromInstruction, to); }

    struct ArmAddress {
        enum AddressType {
            HasOffset,
            HasIndex,
        } type;
        RegisterID base;
        union {
            int32_t offset;
            struct {
                RegisterID index;
                Scale scale;
            };
        } u;
        
        explicit ArmAddress(RegisterID base, int32_t offset = 0)
            : type(HasOffset)
            , base(base)
        {
            u.offset = offset;
        }
        
        explicit ArmAddress(RegisterID base, RegisterID index, Scale scale = TimesOne)
            : type(HasIndex)
            , base(base)
        {
            u.index = index;
            u.scale = scale;
        }
    };
    
public:
    enum RelationalCondition {
        Equal = ARMv7Assembler::ConditionEQ,
        NotEqual = ARMv7Assembler::ConditionNE,
        Above = ARMv7Assembler::ConditionHI,
        AboveOrEqual = ARMv7Assembler::ConditionHS,
        Below = ARMv7Assembler::ConditionLO,
        BelowOrEqual = ARMv7Assembler::ConditionLS,
        GreaterThan = ARMv7Assembler::ConditionGT,
        GreaterThanOrEqual = ARMv7Assembler::ConditionGE,
        LessThan = ARMv7Assembler::ConditionLT,
        LessThanOrEqual = ARMv7Assembler::ConditionLE
    };

    enum ResultCondition {
        Overflow = ARMv7Assembler::ConditionVS,
        Signed = ARMv7Assembler::ConditionMI,
        PositiveOrZero = ARMv7Assembler::ConditionPL,
        Zero = ARMv7Assembler::ConditionEQ,
        NonZero = ARMv7Assembler::ConditionNE
    };

    enum DoubleCondition {
        // These conditions will only evaluate to true if the comparison is ordered - i.e. neither operand is NaN.
        DoubleEqualAndOrdered = ARMv7Assembler::ConditionEQ,
        DoubleNotEqualAndOrdered = ARMv7Assembler::ConditionVC, // Not the right flag! check for this & handle differently.
        DoubleGreaterThanAndOrdered = ARMv7Assembler::ConditionGT,
        DoubleGreaterThanOrEqualAndOrdered = ARMv7Assembler::ConditionGE,
        DoubleLessThanAndOrdered = ARMv7Assembler::ConditionLO,
        DoubleLessThanOrEqualAndOrdered = ARMv7Assembler::ConditionLS,
        // If either operand is NaN, these conditions always evaluate to true.
        DoubleEqualOrUnordered = ARMv7Assembler::ConditionVS, // Not the right flag! check for this & handle differently.
        DoubleNotEqualOrUnordered = ARMv7Assembler::ConditionNE,
        DoubleGreaterThanOrUnordered = ARMv7Assembler::ConditionHI,
        DoubleGreaterThanOrEqualOrUnordered = ARMv7Assembler::ConditionHS,
        DoubleLessThanOrUnordered = ARMv7Assembler::ConditionLT,
        DoubleLessThanOrEqualOrUnordered = ARMv7Assembler::ConditionLE,
    };

    static constexpr RegisterID stackPointerRegister = ARMRegisters::sp;
    static constexpr RegisterID framePointerRegister = ARMRegisters::fp;
    static constexpr RegisterID linkRegister = ARMRegisters::lr;

    // Integer arithmetic operations:
    //
    // Operations are typically two operand - operation(source, srcDst)
    // For many operations the source may be an TrustedImm32, the srcDst operand
    // may often be a memory location (explictly described using an Address
    // object).

    void add32(RegisterID src, RegisterID dest)
    {
        m_assembler.add(dest, dest, src);
    }

    void add32(RegisterID left, RegisterID right, RegisterID dest)
    {
        m_assembler.add(dest, left, right);
    }

    void add32(TrustedImm32 imm, RegisterID dest)
    {
        add32(imm, dest, dest);
    }
    
    void add32(AbsoluteAddress src, RegisterID dest)
    {
        load32(setupArmAddress(src), dataTempRegister);
        add32(dataTempRegister, dest);
    }

    void add32(TrustedImm32 imm, RegisterID src, RegisterID dest)
    {
        // Avoid unpredictable instruction if the destination is the stack pointer
        if (dest == ARMRegisters::sp && src != dest) {
            RegisterID scratch = getCachedAddressTempRegisterIDAndInvalidate();
            add32(imm, src, scratch);
            move(scratch, dest);
            return;
        }

        ARMThumbImmediate armImm = ARMThumbImmediate::makeUInt12OrEncodedImm(imm.m_value);
        if (armImm.isValid()) {
            m_assembler.add(dest, src, armImm);
            return;
        }

        armImm = ARMThumbImmediate::makeUInt12OrEncodedImm(-imm.m_value);
        if (armImm.isValid()) {
            m_assembler.sub(dest, src, armImm);
            return;
        }

        move(imm, dataTempRegister);
        m_assembler.add(dest, src, dataTempRegister);
    }

    void add32(TrustedImm32 imm, Address address)
    {
        constexpr bool updateFlags = false;
        add32Impl(imm, address, updateFlags);
    }

    void add32(Address src, RegisterID dest)
    {
        // load32 will invalidate the cachedDataTempRegister() for us
        load32(src, dataTempRegister);
        add32(dataTempRegister, dest);
    }

    void add32(TrustedImm32 imm, AbsoluteAddress address)
    {
        constexpr bool updateFlags = false;
        add32Impl(imm, address, updateFlags);
    }

    void getEffectiveAddress(BaseIndex address, RegisterID dest)
    {
        RegisterID scratch = getCachedAddressTempRegisterIDAndInvalidate();
        m_assembler.lsl(scratch, address.index, static_cast<int>(address.scale));
        m_assembler.add(dest, address.base, scratch);
        if (address.offset)
            add32(TrustedImm32(address.offset), dest);
    }

    void addPtrNoFlags(TrustedImm32 imm, RegisterID srcDest)
    {
        add32(imm, srcDest);
    }
    
    void add64(TrustedImm32 imm, AbsoluteAddress address)
    {
        RegisterID scratch = getCachedDataTempRegisterIDAndInvalidate();

        load32(setupArmAddress(address), scratch);
        ARMThumbImmediate armImm = ARMThumbImmediate::makeEncodedImm(imm.m_value);
        if (armImm.isValid())
            m_assembler.add_S(scratch, scratch, armImm);
        else {
            move(imm, addressTempRegister);
            m_assembler.add_S(scratch, scratch, addressTempRegister);
            move(TrustedImmPtr(address.m_ptr), addressTempRegister);
        }
        m_assembler.str(scratch, addressTempRegister, ARMThumbImmediate::makeUInt12(0));

        m_assembler.ldr(scratch, addressTempRegister, ARMThumbImmediate::makeUInt12(4));
        m_assembler.adc(scratch, scratch, ARMThumbImmediate::makeEncodedImm(imm.m_value >> 31));
        m_assembler.str(scratch, addressTempRegister, ARMThumbImmediate::makeUInt12(4));
    }

    void and16(Address src, RegisterID dest)
    {
        load16(src, dataTempRegister);
        and32(dataTempRegister, dest);
    }

    void and32(RegisterID op1, RegisterID op2, RegisterID dest)
    {
        m_assembler.ARM_and(dest, op1, op2);
    }

    void and32(TrustedImm32 imm, RegisterID src, RegisterID dest)
    {
        ARMThumbImmediate armImm = ARMThumbImmediate::makeEncodedImm(imm.m_value);
        if (armImm.isValid()) {
            m_assembler.ARM_and(dest, src, armImm);
            return;
        }

        armImm = ARMThumbImmediate::makeEncodedImm(~imm.m_value);
        if (armImm.isValid()) {
            m_assembler.bic(dest, src, armImm);
            return;
        }

        move(imm, dataTempRegister);
        m_assembler.ARM_and(dest, src, dataTempRegister);
    }

    void and32(RegisterID src, RegisterID dest)
    {
        and32(dest, src, dest);
    }

    void and32(TrustedImm32 imm, RegisterID dest)
    {
        and32(imm, dest, dest);
    }

    void and32(Address src, RegisterID dest)
    {
        load32(src, dataTempRegister);
        and32(dataTempRegister, dest);
    }

    void countLeadingZeros32(RegisterID src, RegisterID dest)
    {
        m_assembler.clz(dest, src);
    }

    void lshift32(RegisterID src, RegisterID shiftAmount, RegisterID dest)
    {
        RegisterID scratch = getCachedDataTempRegisterIDAndInvalidate();
        // Clamp the shift to the range 0..31
        ARMThumbImmediate armImm = ARMThumbImmediate::makeEncodedImm(0x1f);
        ASSERT(armImm.isValid());
        m_assembler.ARM_and(scratch, shiftAmount, armImm);

        m_assembler.lsl(dest, src, scratch);
    }

    void lshift32(RegisterID src, TrustedImm32 imm, RegisterID dest)
    {
        m_assembler.lsl(dest, src, imm.m_value & 0x1f);
    }

    void lshift32(RegisterID shiftAmount, RegisterID dest)
    {
        lshift32(dest, shiftAmount, dest);
    }

    void lshift32(TrustedImm32 imm, RegisterID dest)
    {
        lshift32(dest, imm, dest);
    }

    void mul32(RegisterID src, RegisterID dest)
    {
        RegisterID scratch = getCachedDataTempRegisterIDAndInvalidate();
        m_assembler.smull(dest, scratch, dest, src);
    }

    void mul32(RegisterID left, RegisterID right, RegisterID dest)
    {
        RegisterID scratch = getCachedDataTempRegisterIDAndInvalidate();
        m_assembler.smull(dest, scratch, left, right);
    }

    void mul32(TrustedImm32 imm, RegisterID src, RegisterID dest)
    {
        move(imm, dataTempRegister);
        cachedDataTempRegister().invalidate();
        m_assembler.smull(dest, dataTempRegister, src, dataTempRegister);
    }

    void neg32(RegisterID srcDest)
    {
        m_assembler.neg(srcDest, srcDest);
    }

    void neg32(RegisterID src, RegisterID dest)
    {
        m_assembler.neg(dest, src);
    }

    void or8(TrustedImm32 imm, AbsoluteAddress address)
    {
        ARMThumbImmediate armImm = ARMThumbImmediate::makeEncodedImm(imm.m_value);
        load8(setupArmAddress(address), dataTempRegister);
        if (armImm.isValid()) {
            m_assembler.orr(dataTempRegister, dataTempRegister, armImm);
            store8(dataTempRegister, Address(addressTempRegister));
        } else {
            move(imm, addressTempRegister);
            m_assembler.orr(dataTempRegister, dataTempRegister, addressTempRegister);
            move(TrustedImmPtr(address.m_ptr), addressTempRegister);
            store8(dataTempRegister, Address(addressTempRegister));
        }
    }

    void or16(TrustedImm32 imm, AbsoluteAddress dest)
    {
        ARMThumbImmediate armImm = ARMThumbImmediate::makeEncodedImm(imm.m_value);
        load16(setupArmAddress(dest), dataTempRegister);
        if (armImm.isValid()) {
            m_assembler.orr(dataTempRegister, dataTempRegister, armImm);
            store16(dataTempRegister, Address(addressTempRegister));
        } else {
            move(imm, addressTempRegister);
            m_assembler.orr(dataTempRegister, dataTempRegister, addressTempRegister);
            move(TrustedImmPtr(dest.m_ptr), addressTempRegister);
            store16(dataTempRegister, Address(addressTempRegister));
        }
    }

    void or32(RegisterID src, RegisterID dest)
    {
        m_assembler.orr(dest, dest, src);
    }

    void or32(RegisterID src, AbsoluteAddress dest)
    {
        load32(setupArmAddress(dest), dataTempRegister);
        or32(src, dataTempRegister);
        store32(dataTempRegister, Address(addressTempRegister));
    }

    void or32(TrustedImm32 imm, AbsoluteAddress address)
    {
        ARMThumbImmediate armImm = ARMThumbImmediate::makeEncodedImm(imm.m_value);
        load32(setupArmAddress(address), dataTempRegister);
        if (armImm.isValid()) {
            m_assembler.orr(dataTempRegister, dataTempRegister, armImm);
            store32(dataTempRegister, Address(addressTempRegister));
        } else {
            move(imm, addressTempRegister);
            m_assembler.orr(dataTempRegister, dataTempRegister, addressTempRegister);
            move(TrustedImmPtr(address.m_ptr), addressTempRegister);
            store32(dataTempRegister, Address(addressTempRegister));
        }
    }

    void or32(TrustedImm32 imm, Address address)
    {
        load32(address, dataTempRegister);
        or32(imm, dataTempRegister, dataTempRegister);
        store32(dataTempRegister, address);
    }

    void or32(TrustedImm32 imm, RegisterID dest)
    {
        or32(imm, dest, dest);
    }

    void or32(RegisterID op1, RegisterID op2, RegisterID dest)
    {
        m_assembler.orr(dest, op1, op2);
    }

    void or32(TrustedImm32 imm, RegisterID src, RegisterID dest)
    {
        ARMThumbImmediate armImm = ARMThumbImmediate::makeEncodedImm(imm.m_value);
        if (armImm.isValid())
            m_assembler.orr(dest, src, armImm);
        else {
            ASSERT(src != dataTempRegister);
            move(imm, dataTempRegister);
            m_assembler.orr(dest, src, dataTempRegister);
        }
    }

    void rotateRight32(RegisterID src, TrustedImm32 imm, RegisterID dest)
    {
        if (!imm.m_value)
            move(src, dest);
        else
            m_assembler.ror(dest, src, imm.m_value & 0x1f);
    }

    void rotateRight32(TrustedImm32 imm, RegisterID srcDst)
    {
        rotateRight32(srcDst, imm, srcDst);
    }

    void rshift32(RegisterID src, RegisterID shiftAmount, RegisterID dest)
    {
        RegisterID scratch = getCachedDataTempRegisterIDAndInvalidate();
        // Clamp the shift to the range 0..31
        ARMThumbImmediate armImm = ARMThumbImmediate::makeEncodedImm(0x1f);
        ASSERT(armImm.isValid());
        m_assembler.ARM_and(scratch, shiftAmount, armImm);

        m_assembler.asr(dest, src, scratch);
    }

    void rshift32(RegisterID src, TrustedImm32 imm, RegisterID dest)
    {
        if (!imm.m_value)
            move(src, dest);
        else
            m_assembler.asr(dest, src, imm.m_value & 0x1f);
    }

    void rshift32(RegisterID shiftAmount, RegisterID dest)
    {
        rshift32(dest, shiftAmount, dest);
    }
    
    void rshift32(TrustedImm32 imm, RegisterID dest)
    {
        rshift32(dest, imm, dest);
    }

    void urshift32(RegisterID src, RegisterID shiftAmount, RegisterID dest)
    {
        RegisterID scratch = getCachedDataTempRegisterIDAndInvalidate();
        // Clamp the shift to the range 0..31
        ARMThumbImmediate armImm = ARMThumbImmediate::makeEncodedImm(0x1f);
        ASSERT(armImm.isValid());
        m_assembler.ARM_and(scratch, shiftAmount, armImm);
        
        m_assembler.lsr(dest, src, scratch);
    }
    
    void urshift32(RegisterID src, TrustedImm32 imm, RegisterID dest)
    {
        if (!imm.m_value)
            move(src, dest);
        else
            m_assembler.lsr(dest, src, imm.m_value & 0x1f);
    }

    void urshift32(RegisterID shiftAmount, RegisterID dest)
    {
        urshift32(dest, shiftAmount, dest);
    }
    
    void urshift32(TrustedImm32 imm, RegisterID dest)
    {
        urshift32(dest, imm, dest);
    }

    void sub32(RegisterID src, RegisterID dest)
    {
        m_assembler.sub(dest, dest, src);
    }

    void sub32(RegisterID left, RegisterID right, RegisterID dest)
    {
        m_assembler.sub(dest, left, right);
    }

    void sub32(RegisterID left, TrustedImm32 right, RegisterID dest)
    {
        ARMThumbImmediate armImm = ARMThumbImmediate::makeUInt12OrEncodedImm(right.m_value);
        if (armImm.isValid())
            m_assembler.sub(dest, left, armImm);
        else {
            move(right, dataTempRegister);
            m_assembler.sub(dest, left, dataTempRegister);
        }
    }

    void sub32(TrustedImm32 imm, RegisterID dest)
    {
        ARMThumbImmediate armImm = ARMThumbImmediate::makeUInt12OrEncodedImm(imm.m_value);
        if (armImm.isValid())
            m_assembler.sub(dest, dest, armImm);
        else {
            move(imm, dataTempRegister);
            m_assembler.sub(dest, dest, dataTempRegister);
        }
    }

    void sub32(TrustedImm32 imm, Address address)
    {
        load32(address, dataTempRegister);

        ARMThumbImmediate armImm = ARMThumbImmediate::makeUInt12OrEncodedImm(imm.m_value);
        if (armImm.isValid())
            m_assembler.sub(dataTempRegister, dataTempRegister, armImm);
        else {
            // Hrrrm, since dataTempRegister holds the data loaded,
            // use addressTempRegister to hold the immediate.
            move(imm, addressTempRegister);
            m_assembler.sub(dataTempRegister, dataTempRegister, addressTempRegister);
        }

        store32(dataTempRegister, address);
    }

    void sub32(Address src, RegisterID dest)
    {
        load32(src, dataTempRegister);
        sub32(dataTempRegister, dest);
    }

    void sub32(TrustedImm32 imm, AbsoluteAddress address)
    {
        load32(setupArmAddress(address), dataTempRegister);

        ARMThumbImmediate armImm = ARMThumbImmediate::makeUInt12OrEncodedImm(imm.m_value);
        if (armImm.isValid())
            m_assembler.sub(dataTempRegister, dataTempRegister, armImm);
        else {
            // Hrrrm, since dataTempRegister holds the data loaded,
            // use addressTempRegister to hold the immediate.
            move(imm, addressTempRegister);
            m_assembler.sub(dataTempRegister, dataTempRegister, addressTempRegister);
        }

        store32(dataTempRegister, address.m_ptr);
    }

    void xor32(RegisterID op1, RegisterID op2, RegisterID dest)
    {
        m_assembler.eor(dest, op1, op2);
    }

    void xor32(TrustedImm32 imm, RegisterID src, RegisterID dest)
    {
        if (imm.m_value == -1) {
            m_assembler.mvn(dest, src);
            return;
        }

        ARMThumbImmediate armImm = ARMThumbImmediate::makeEncodedImm(imm.m_value);
        if (armImm.isValid())
            m_assembler.eor(dest, src, armImm);
        else {
            move(imm, dataTempRegister);
            m_assembler.eor(dest, src, dataTempRegister);
        }
    }

    void xor32(RegisterID src, RegisterID dest)
    {
        xor32(dest, src, dest);
    }

    void xor32(Address src, RegisterID dest)
    {
        load32(src, dataTempRegister);
        xor32(dataTempRegister, dest);
    }

    void xor32(TrustedImm32 imm, RegisterID dest)
    {
        if (imm.m_value == -1)
            m_assembler.mvn(dest, dest);
        else
            xor32(imm, dest, dest);
    }
    
    void not32(RegisterID srcDest)
    {
        m_assembler.mvn(srcDest, srcDest);
    }

    // Memory access operations:
    //
    // Loads are of the form load(address, destination) and stores of the form
    // store(source, address).  The source for a store may be an TrustedImm32.  Address
    // operand objects to loads and store will be implicitly constructed if a
    // register is passed.

private:
    void load32(ArmAddress address, RegisterID dest)
    {
        if (dest == addressTempRegister)
            invalidateCachedAddressTempRegister();
        else if (dest == dataTempRegister)
            cachedDataTempRegister().invalidate();

        if (address.type == ArmAddress::HasIndex)
            m_assembler.ldr(dest, address.base, address.u.index, address.u.scale);
        else if (address.u.offset >= 0) {
            ARMThumbImmediate armImm = ARMThumbImmediate::makeUInt12(address.u.offset);
            ASSERT(armImm.isValid());
            m_assembler.ldr(dest, address.base, armImm);
        } else {
            ASSERT(address.u.offset >= -255);
            m_assembler.ldr(dest, address.base, address.u.offset, true, false);
        }
    }

    void load16(ArmAddress address, RegisterID dest)
    {
        if (dest == addressTempRegister)
            invalidateCachedAddressTempRegister();
        else if (dest == dataTempRegister)
            cachedDataTempRegister().invalidate();

        if (address.type == ArmAddress::HasIndex)
            m_assembler.ldrh(dest, address.base, address.u.index, address.u.scale);
        else if (address.u.offset >= 0) {
            ARMThumbImmediate armImm = ARMThumbImmediate::makeUInt12(address.u.offset);
            ASSERT(armImm.isValid());
            m_assembler.ldrh(dest, address.base, armImm);
        } else {
            ASSERT(address.u.offset >= -255);
            m_assembler.ldrh(dest, address.base, address.u.offset, true, false);
        }
    }
    
    void load16SignedExtendTo32(ArmAddress address, RegisterID dest)
    {
        ASSERT(address.type == ArmAddress::HasIndex);
        if (dest == addressTempRegister)
            invalidateCachedAddressTempRegister();
        else if (dest == dataTempRegister)
            cachedDataTempRegister().invalidate();

        m_assembler.ldrsh(dest, address.base, address.u.index, address.u.scale);
    }

    void load8(ArmAddress address, RegisterID dest)
    {
        if (dest == addressTempRegister)
            invalidateCachedAddressTempRegister();
        else if (dest == dataTempRegister)
            cachedDataTempRegister().invalidate();

        if (address.type == ArmAddress::HasIndex)
            m_assembler.ldrb(dest, address.base, address.u.index, address.u.scale);
        else if (address.u.offset >= 0) {
            ARMThumbImmediate armImm = ARMThumbImmediate::makeUInt12(address.u.offset);
            ASSERT(armImm.isValid());
            m_assembler.ldrb(dest, address.base, armImm);
        } else {
            ASSERT(address.u.offset >= -255);
            m_assembler.ldrb(dest, address.base, address.u.offset, true, false);
        }
    }
    
    void load8SignedExtendTo32(ArmAddress address, RegisterID dest)
    {
        ASSERT(address.type == ArmAddress::HasIndex);
        if (dest == addressTempRegister)
            invalidateCachedAddressTempRegister();
        else if (dest == dataTempRegister)
            cachedDataTempRegister().invalidate();

        m_assembler.ldrsb(dest, address.base, address.u.index, address.u.scale);
    }

protected:
    void store32(RegisterID src, ArmAddress address)
    {
        if (address.type == ArmAddress::HasIndex)
            m_assembler.str(src, address.base, address.u.index, address.u.scale);
        else if (address.u.offset >= 0) {
            ARMThumbImmediate armImm = ARMThumbImmediate::makeUInt12(address.u.offset);
            ASSERT(armImm.isValid());
            m_assembler.str(src, address.base, armImm);
        } else {
            ASSERT(address.u.offset >= -255);
            m_assembler.str(src, address.base, address.u.offset, true, false);
        }
    }

private:
    void store8(RegisterID src, ArmAddress address)
    {
        if (address.type == ArmAddress::HasIndex)
            m_assembler.strb(src, address.base, address.u.index, address.u.scale);
        else if (address.u.offset >= 0) {
            ARMThumbImmediate armImm = ARMThumbImmediate::makeUInt12(address.u.offset);
            ASSERT(armImm.isValid());
            m_assembler.strb(src, address.base, armImm);
        } else {
            ASSERT(address.u.offset >= -255);
            m_assembler.strb(src, address.base, address.u.offset, true, false);
        }
    }
    
    void store16(RegisterID src, ArmAddress address)
    {
        if (address.type == ArmAddress::HasIndex)
            m_assembler.strh(src, address.base, address.u.index, address.u.scale);
        else if (address.u.offset >= 0) {
            ARMThumbImmediate armImm = ARMThumbImmediate::makeUInt12(address.u.offset);
            ASSERT(armImm.isValid());
            m_assembler.strh(src, address.base, armImm);
        } else {
            ASSERT(address.u.offset >= -255);
            m_assembler.strh(src, address.base, address.u.offset, true, false);
        }
    }

public:
    void load32(Address address, RegisterID dest)
    {
        load32(setupArmAddress(address), dest);
    }

    void load32(BaseIndex address, RegisterID dest)
    {
        load32(setupArmAddress(address), dest);
    }

    void load32WithUnalignedHalfWords(BaseIndex address, RegisterID dest)
    {
        load32(setupArmAddress(address), dest);
    }

    void load16Unaligned(BaseIndex address, RegisterID dest)
    {
        load16(setupArmAddress(address), dest);
    }

    void load32(const void* address, RegisterID dest)
    {
        load32(setupArmAddress(AbsoluteAddress(address)), dest);
    }

    void abortWithReason(AbortReason reason)
    {
        move(TrustedImm32(reason), dataTempRegister);
        breakpoint();
    }

    void abortWithReason(AbortReason reason, intptr_t misc)
    {
        move(TrustedImm32(misc), addressTempRegister);
        abortWithReason(reason);
    }

    ConvertibleLoadLabel convertibleLoadPtr(Address address, RegisterID dest)
    {
        ConvertibleLoadLabel result(this);
        ASSERT(address.offset >= 0 && address.offset <= 255);
        m_assembler.ldrWide8BitImmediate(dest, address.base, address.offset);
        return result;
    }

    void load8(Address address, RegisterID dest)
    {
        load8(setupArmAddress(address), dest);
    }

    void load8SignedExtendTo32(Address, RegisterID)
    {
        UNREACHABLE_FOR_PLATFORM();
    }

    void load8(BaseIndex address, RegisterID dest)
    {
        load8(setupArmAddress(address), dest);
    }
    
    void load8SignedExtendTo32(BaseIndex address, RegisterID dest)
    {
        load8SignedExtendTo32(setupArmAddress(address), dest);
    }

    void load8(const void* address, RegisterID dest)
    {
        load8(setupArmAddress(AbsoluteAddress(address), dest), dest);
    }

    void load16(const void* address, RegisterID dest)
    {
        load16(setupArmAddress(AbsoluteAddress(address), dest), dest);
    }

    void load16(BaseIndex address, RegisterID dest)
    {
        m_assembler.ldrh(dest, makeBaseIndexBase(address), address.index, address.scale);
    }
    
    void load16SignedExtendTo32(BaseIndex address, RegisterID dest)
    {
        load16SignedExtendTo32(setupArmAddress(address), dest);
    }
    
    void load16(Address address, RegisterID dest)
    {
        ARMThumbImmediate armImm = ARMThumbImmediate::makeUInt12(address.offset);
        if (armImm.isValid())
            m_assembler.ldrh(dest, address.base, armImm);
        else {
            move(TrustedImm32(address.offset), dataTempRegister);
            m_assembler.ldrh(dest, address.base, dataTempRegister);
        }
    }
    
    void load16SignedExtendTo32(Address, RegisterID)
    {
        UNREACHABLE_FOR_PLATFORM();
    }

    void loadPair32(RegisterID src, RegisterID dest1, RegisterID dest2)
    {
        loadPair32(src, TrustedImm32(0), dest1, dest2);
    }

    void loadPair32(RegisterID src, TrustedImm32 offset, RegisterID dest1, RegisterID dest2)
    {
        loadPair32(Address(src, offset.m_value), dest1, dest2);
    }

    void loadPair32(ArmAddress address, RegisterID dest1, RegisterID dest2)
    {
        if (address.type == ArmAddress::HasIndex) {
            // Using r0-r7 can often be encoded with a shorter (16-bit vs 32-bit) instruction, so use
            // whichever destination register is in that range (if any) as the address temp register
            RegisterID scratch = dest1;
            if (dest1 >= ARMRegisters::r8)
                scratch = dest2;
            if (address.u.scale == TimesOne)
                m_assembler.add(scratch, address.base, address.u.index);
            else {
                ShiftTypeAndAmount shift { ARMShiftType::SRType_LSL, static_cast<unsigned>(address.u.scale) };
                m_assembler.add(scratch, address.base, address.u.index, shift);
            }
            loadPair32(Address(scratch), dest1, dest2);
        } else {
            ASSERT(dest1 != dest2); // If it is the same, ldp becomes illegal instruction.
            int32_t absOffset = address.u.offset;
            if (absOffset < 0)
                absOffset = -absOffset;
            if (!(absOffset & ~0x3fc)) {
                if ((dest1 == addressTempRegister) || (dest2 == addressTempRegister))
                    invalidateCachedAddressTempRegister();
                if ((dest1 == dataTempRegister) || (dest2 == dataTempRegister))
                    cachedDataTempRegister().invalidate();
                m_assembler.ldrd(dest1, dest2, address.base, address.u.offset, /* index: */ true, /* wback: */ false);
            } else if (address.base == dest1) {
                ArmAddress highAddress(address.base, address.u.offset + 4);
                load32(highAddress, dest2);
                load32(address, dest1);
            } else {
                load32(address, dest1);
                ArmAddress highAddress(address.base, address.u.offset + 4);
                load32(highAddress, dest2);
            }
        }
    }

    void loadPair32(Address address, RegisterID dest1, RegisterID dest2)
    {
        loadPair32(setupArmAddress(address), dest1, dest2);
    }

    void loadPair32(BaseIndex address, RegisterID dest1, RegisterID dest2)
    {
        // Using r0-r7 can often be encoded with a shorter (16-bit vs 32-bit) instruction, so use
        // whichever destination register is in that range (if any) as the address temp register
        RegisterID scratch = dest1;
        if (dest1 >= ARMRegisters::r8)
            scratch = dest2;
        if (address.scale == TimesOne)
            m_assembler.add(scratch, address.base, address.index);
        else {
            ShiftTypeAndAmount shift { ARMShiftType::SRType_LSL, static_cast<unsigned>(address.scale) };
            m_assembler.add(scratch, address.base, address.index, shift);
        }
        loadPair32(Address(scratch, address.offset), dest1, dest2);
    }

    void loadPair64(RegisterID src, TrustedImm32 offset, FPRegisterID dest1, FPRegisterID dest2)
    {
        ASSERT(dest1 != dest2);
        if ((dest2 == (dest1 + 1)) && !offset.m_value) {
            // Only emit a VLDMIA if the registers happen to be consecutive and
            // in the proper order and the offset happens to be zero. Otherwise,
            // the extra instructions to adjust things mean there are no space
            // savings and the VLDM itself might be a performance loss.
            m_assembler.vldmia(src, dest1, 2);
        } else {
            loadDouble(Address(src, offset.m_value), dest1);
            loadDouble(Address(src, offset.m_value + 8), dest2);
        }
    }

    void storePair64(FPRegisterID src1, FPRegisterID src2, RegisterID dest, TrustedImm32 offset)
    {
        if ((src2 == (src1 + 1)) && !offset.m_value) {
            // Only emit a VSTMIA under a narrow set of conditions. See
            // loadPair64 for the rationale.
            m_assembler.vstmia(dest, src1, 2);
        } else {
            storeDouble(src1, Address(dest, offset.m_value));
            storeDouble(src2, Address(dest, offset.m_value + 8));
        }
    }

    void loadPair32(AbsoluteAddress address, RegisterID dest1, RegisterID dest2)
    {
        loadPair32(setupArmAddress<BoundsDoubleWordOffset>(address), dest1, dest2);
    }

    void store32(RegisterID src, Address address)
    {
        store32(src, setupArmAddress(address));
    }

    void store32(RegisterID src, BaseIndex address)
    {
        store32(src, setupArmAddress(address));
    }

    void store32(TrustedImm32 imm, Address address)
    {
        ArmAddress armAddress = setupArmAddress(address);
        RegisterID scratch = addressTempRegister;
        if (armAddress.type == ArmAddress::HasIndex)
            scratch = dataTempRegister;
        move(imm, scratch);
        store32(scratch, armAddress);
    }

    void store32(TrustedImm32 imm, BaseIndex address)
    {
        move(imm, dataTempRegister);
        store32(dataTempRegister, setupArmAddress(address));
    }

    void store32(RegisterID src, const void* address)
    {
        store32(src, setupArmAddress(AbsoluteAddress(address)));
    }

    void store32(TrustedImm32 imm, const void* address)
    {
        move(imm, dataTempRegister);
        store32(dataTempRegister, address);
    }

    void store8(RegisterID src, Address address)
    {
        store8(src, setupArmAddress(address));
    }
    
    void store8(RegisterID src, BaseIndex address)
    {
        store8(src, setupArmAddress(address));
    }
    
    void store8(RegisterID src, const void* address)
    {
        store8(src, setupArmAddress(AbsoluteAddress(address)));
    }
    
    void store8(TrustedImm32 imm, const void* address)
    {
        TrustedImm32 imm8(static_cast<int8_t>(imm.m_value));
        move(imm8, dataTempRegister);
        store8(dataTempRegister, address);
    }
    
    void store8(TrustedImm32 imm, Address address)
    {
        TrustedImm32 imm8(static_cast<int8_t>(imm.m_value));
        move(imm8, dataTempRegister);
        store8(dataTempRegister, address);
    }

    void store8(RegisterID src, RegisterID addrreg)
    {
        store8(src, ArmAddress(addrreg, 0));
    }
    
    void store16(RegisterID src, Address address)
    {
        store16(src, setupArmAddress(address));
    }

    void store16(RegisterID src, BaseIndex address)
    {
        store16(src, setupArmAddress(address));
    }

    void store16(RegisterID src, const void* address)
    {
        store16(src, setupArmAddress(AbsoluteAddress(address)));
    }

    void store16(TrustedImm32 imm, const void* address)
    {
        move(imm, dataTempRegister);
        store16(dataTempRegister, address);
    }

    void storePair32(RegisterID src1, TrustedImm32 imm, Address address)
    {
        RegisterID scratch = getCachedDataTempRegisterIDAndInvalidate();
        move(imm, scratch);
        storePair32(src1, scratch, address);
    }

    void storePair32(TrustedImmPtr immPtr, TrustedImm32 imm32, Address address)
    {
        storePair32(TrustedImm32(immPtr), imm32, address);
    }

    void storePair32(TrustedImm32 imm1, TrustedImm32 imm2, Address address)
    {
        // We cannot re-use the two register version of `storePair32` defined
        // below here because we can only use the `addressTempRegister` as a
        // scratch register if the `strd` case is taken.
        int32_t absOffset = address.offset;
        if (absOffset < 0)
            absOffset = -absOffset;
        if (!(absOffset & ~0x3fc)) {
            RegisterID src1 = getCachedAddressTempRegisterIDAndInvalidate();
            move(imm1, src1);
            RegisterID src2 = src1;
            if (imm1.m_value != imm2.m_value) {
                src2 = getCachedDataTempRegisterIDAndInvalidate();
                move(imm2, src2);
            }
            ASSERT(src1 != address.base && src2 != address.base);
            m_assembler.strd(src1, src2, address.base, address.offset, /* index: */ true, /* wback: */ false);
        } else {
            store32(imm1, address);
            store32(imm2, address.withOffset(4));
        }
    }

    void storePair32(RegisterID src1, RegisterID src2, RegisterID dest)
    {
        storePair32(src1, src2, dest, TrustedImm32(0));
    }

    void storePair32(RegisterID src1, RegisterID src2, RegisterID dest, TrustedImm32 offset)
    {
        storePair32(src1, src2, Address(dest, offset.m_value));
    }

    void storePair32(RegisterID src1, RegisterID src2, Address address)
    {
        int32_t absOffset = address.offset;
        if (absOffset < 0)
            absOffset = -absOffset;
        if (!(absOffset & ~0x3fc))
            m_assembler.strd(src1, src2, address.base, address.offset, /* index: */ true, /* wback: */ false);
        else {
            store32(src1, address);
            store32(src2, address.withOffset(4));
        }
    }

    void storePair32(RegisterID src1, RegisterID src2, BaseIndex address)
    {
        ASSERT(src1 != dataTempRegister && src2 != dataTempRegister);
        RegisterID scratch = getCachedDataTempRegisterIDAndInvalidate();
        // The 'addressTempRegister' might be used when the offset is wide, so use 'dataTempRegister'
        if (address.scale == TimesOne)
            m_assembler.add(scratch, address.base, address.index);
        else {
            ShiftTypeAndAmount shift { ARMShiftType::SRType_LSL, static_cast<unsigned>(address.scale) };
            m_assembler.add(scratch, address.base, address.index, shift);
        }
        storePair32(src1, src2, Address(scratch, address.offset));
    }

    void storePair32(TrustedImm32 imm1, TrustedImm32 imm2, BaseIndex address)
    {
        // We don't have enough temp registers to move both imm and calculate the address
        store32(imm1, address);
        store32(imm2, address.withOffset(4));
    }

    void storePair32(RegisterID src1, TrustedImm32 imm, const void* address)
    {
        ArmAddress armAddress = setupArmAddress(AbsoluteAddress(address));
        ASSERT(armAddress.type == ArmAddress::HasOffset);
        RegisterID scratch = getCachedDataTempRegisterIDAndInvalidate();
        move(imm, scratch);
        storePair32(src1, scratch, Address(armAddress.base, armAddress.u.offset));
    }

    void storePair32(RegisterID src1, RegisterID src2, const void* address)
    {
        ArmAddress armAddress = setupArmAddress(AbsoluteAddress(address));
        ASSERT(armAddress.type == ArmAddress::HasOffset);
        storePair32(src1, src2, Address(armAddress.base, armAddress.u.offset));
    }

    // Possibly clobbers src, but not on this architecture.
    void moveDoubleToInts(FPRegisterID src, RegisterID dest1, RegisterID dest2)
    {
        m_assembler.vmov(dest1, dest2, src);
    }
    
    void moveIntsToDouble(RegisterID src1, RegisterID src2, FPRegisterID dest)
    {
        m_assembler.vmov(dest, src1, src2);
    }

    static bool shouldBlindForSpecificArch(uint32_t value)
    {
        ARMThumbImmediate immediate = ARMThumbImmediate::makeEncodedImm(value);

        // Couldn't be encoded as an immediate, so assume it's untrusted.
        if (!immediate.isValid())
            return true;
        
        // If we can encode the immediate, we have less than 16 attacker
        // controlled bits.
        if (immediate.isEncodedImm())
            return false;

        // Don't let any more than 12 bits of an instruction word
        // be controlled by an attacker.
        return !immediate.isUInt12();
    }

    // Floating-point operations:

    static bool supportsFloatingPoint() { return true; }
    static bool supportsFloatingPointTruncate() { return true; }
    static bool supportsFloatingPointSqrt() { return true; }
    static bool supportsFloatingPointAbs() { return true; }
    static bool supportsFloatingPointRounding() { return false; }

    void loadDouble(Address address, FPRegisterID dest)
    {
        RegisterID base = address.base;
        int32_t offset = address.offset;

        // Arm vfp addresses can be offset by a 9-bit ones-comp immediate, left shifted by 2.
        if ((offset & 3) || (offset > (255 * 4)) || (offset < -(255 * 4))) {
            RegisterID scratch = getCachedAddressTempRegisterIDAndInvalidate();
            add32(TrustedImm32(offset), base, scratch);
            base = scratch;
            offset = 0;
        }
        
        m_assembler.vldr(dest, base, offset);
    }

    void loadFloat(Address address, FPRegisterID dest)
    {
        RegisterID base = address.base;
        int32_t offset = address.offset;

        // Arm vfp addresses can be offset by a 9-bit ones-comp immediate, left shifted by 2.
        if ((offset & 3) || (offset > (255 * 4)) || (offset < -(255 * 4))) {
            RegisterID scratch = getCachedAddressTempRegisterIDAndInvalidate();
            add32(TrustedImm32(offset), base, scratch);
            base = scratch;
            offset = 0;
        }
        
        m_assembler.flds(ARMRegisters::asSingle(dest), base, offset);
    }

    void loadDouble(BaseIndex address, FPRegisterID dest)
    {
        move(address.index, addressTempRegister);
        lshift32(TrustedImm32(address.scale), addressTempRegister);
        add32(address.base, addressTempRegister);
        cachedAddressTempRegister().invalidate();
        loadDouble(Address(addressTempRegister, address.offset), dest);
    }
    
    void loadFloat(BaseIndex address, FPRegisterID dest)
    {
        move(address.index, addressTempRegister);
        lshift32(TrustedImm32(address.scale), addressTempRegister);
        add32(address.base, addressTempRegister);
        cachedAddressTempRegister().invalidate();
        loadFloat(Address(addressTempRegister, address.offset), dest);
    }

    void moveDouble(FPRegisterID src, FPRegisterID dest)
    {
        if (src != dest)
            m_assembler.vmov(dest, src);
    }

    void moveDouble(FPRegisterID src, RegisterID dest)
    {
        m_assembler.vmov(dest, RegisterID(dest + 1), src);
    }

    void moveZeroToDouble(FPRegisterID reg)
    {
        static double zeroConstant = 0.;
        loadDouble(TrustedImmPtr(&zeroConstant), reg);
    }

    void loadDouble(TrustedImmPtr address, FPRegisterID dest)
    {
        move(address, addressTempRegister);
        m_assembler.vldr(dest, addressTempRegister, 0);
    }

    void storeDouble(FPRegisterID src, Address address)
    {
        RegisterID base = address.base;
        int32_t offset = address.offset;

        // Arm vfp addresses can be offset by a 9-bit ones-comp immediate, left shifted by 2.
        if ((offset & 3) || (offset > (255 * 4)) || (offset < -(255 * 4))) {
            RegisterID scratch = getCachedAddressTempRegisterIDAndInvalidate();
            add32(TrustedImm32(offset), base, scratch);
            base = scratch;
            offset = 0;
        }
        
        m_assembler.vstr(src, base, offset);
    }

    void storeFloat(FPRegisterID src, Address address)
    {
        RegisterID base = address.base;
        int32_t offset = address.offset;

        // Arm vfp addresses can be offset by a 9-bit ones-comp immediate, left shifted by 2.
        if ((offset & 3) || (offset > (255 * 4)) || (offset < -(255 * 4))) {
            RegisterID scratch = getCachedAddressTempRegisterIDAndInvalidate();
            add32(TrustedImm32(offset), base, scratch);
            base = scratch;
            offset = 0;
        }
        
        m_assembler.fsts(ARMRegisters::asSingle(src), base, offset);
    }

    void storeDouble(FPRegisterID src, TrustedImmPtr address)
    {
        move(address, addressTempRegister);
        storeDouble(src, Address(addressTempRegister));
    }

    void storeDouble(FPRegisterID src, BaseIndex address)
    {
        move(address.index, addressTempRegister);
        lshift32(TrustedImm32(address.scale), addressTempRegister);
        add32(address.base, addressTempRegister);
        cachedAddressTempRegister().invalidate();
        storeDouble(src, Address(addressTempRegister, address.offset));
    }
    
    void storeFloat(FPRegisterID src, BaseIndex address)
    {
        move(address.index, addressTempRegister);
        lshift32(TrustedImm32(address.scale), addressTempRegister);
        add32(address.base, addressTempRegister);
        cachedAddressTempRegister().invalidate();
        storeFloat(src, Address(addressTempRegister, address.offset));
    }
    
    void addDouble(FPRegisterID src, FPRegisterID dest)
    {
        m_assembler.vadd(dest, dest, src);
    }

    void addDouble(Address src, FPRegisterID dest)
    {
        loadDouble(src, fpTempRegister);
        addDouble(fpTempRegister, dest);
    }

    void addDouble(FPRegisterID op1, FPRegisterID op2, FPRegisterID dest)
    {
        m_assembler.vadd(dest, op1, op2);
    }

    void addDouble(AbsoluteAddress address, FPRegisterID dest)
    {
        loadDouble(TrustedImmPtr(address.m_ptr), fpTempRegister);
        m_assembler.vadd(dest, dest, fpTempRegister);
    }

    void divDouble(FPRegisterID src, FPRegisterID dest)
    {
        m_assembler.vdiv(dest, dest, src);
    }

    void divDouble(FPRegisterID op1, FPRegisterID op2, FPRegisterID dest)
    {
        m_assembler.vdiv(dest, op1, op2);
    }

    void subDouble(FPRegisterID src, FPRegisterID dest)
    {
        m_assembler.vsub(dest, dest, src);
    }

    void subDouble(Address src, FPRegisterID dest)
    {
        loadDouble(src, fpTempRegister);
        subDouble(fpTempRegister, dest);
    }

    void subDouble(FPRegisterID op1, FPRegisterID op2, FPRegisterID dest)
    {
        m_assembler.vsub(dest, op1, op2);
    }

    void mulDouble(FPRegisterID src, FPRegisterID dest)
    {
        m_assembler.vmul(dest, dest, src);
    }

    void mulDouble(Address src, FPRegisterID dest)
    {
        loadDouble(src, fpTempRegister);
        mulDouble(fpTempRegister, dest);
    }

    void mulDouble(FPRegisterID op1, FPRegisterID op2, FPRegisterID dest)
    {
        m_assembler.vmul(dest, op1, op2);
    }

    void andDouble(FPRegisterID op1, FPRegisterID op2, FPRegisterID dest)
    {
        m_assembler.vand(dest, op1, op2);
    }

    void orDouble(FPRegisterID op1, FPRegisterID op2, FPRegisterID dest)
    {
        m_assembler.vorr(dest, op1, op2);
    }

    void sqrtDouble(FPRegisterID src, FPRegisterID dest)
    {
        m_assembler.vsqrt(dest, src);
    }
    
    void absDouble(FPRegisterID src, FPRegisterID dest)
    {
        m_assembler.vabs(dest, src);
    }

    void negateDouble(FPRegisterID src, FPRegisterID dest)
    {
        m_assembler.vneg(dest, src);
    }

    NO_RETURN_DUE_TO_CRASH void ceilDouble(FPRegisterID, FPRegisterID)
    {
        ASSERT(!supportsFloatingPointRounding());
        CRASH();
    }

    NO_RETURN_DUE_TO_CRASH void floorDouble(FPRegisterID, FPRegisterID)
    {
        ASSERT(!supportsFloatingPointRounding());
        CRASH();
    }

    NO_RETURN_DUE_TO_CRASH void roundTowardZeroDouble(FPRegisterID, FPRegisterID)
    {
        ASSERT(!supportsFloatingPointRounding());
        CRASH();
    }

    void convertInt32ToFloat(RegisterID src, FPRegisterID dest)
    {
        m_assembler.vmov(fpTempRegister, src, src);
        m_assembler.vcvt_signedToFloatingPoint(dest, fpTempRegisterAsSingle(), /* toDouble: */ false);
    }

    void convertInt32ToDouble(RegisterID src, FPRegisterID dest)
    {
        m_assembler.vmov(fpTempRegister, src, src);
        m_assembler.vcvt_signedToFloatingPoint(dest, fpTempRegisterAsSingle());
    }

    void convertInt32ToDouble(Address address, FPRegisterID dest)
    {
        // Fixme: load directly into the fpr!
        load32(address, dataTempRegister);
        m_assembler.vmov(fpTempRegister, dataTempRegister, dataTempRegister);
        m_assembler.vcvt_signedToFloatingPoint(dest, fpTempRegisterAsSingle());
    }

    void convertInt32ToDouble(AbsoluteAddress address, FPRegisterID dest)
    {
        // Fixme: load directly into the fpr!
        load32(address.m_ptr, dataTempRegister);
        m_assembler.vmov(fpTempRegister, dataTempRegister, dataTempRegister);
        m_assembler.vcvt_signedToFloatingPoint(dest, fpTempRegisterAsSingle());
    }
    
    void convertFloatToDouble(FPRegisterID src, FPRegisterID dst)
    {
        m_assembler.vcvtds(dst, ARMRegisters::asSingle(src));
    }
    
    void convertDoubleToFloat(FPRegisterID src, FPRegisterID dst)
    {
        m_assembler.vcvtsd(ARMRegisters::asSingle(dst), src);
    }

    Jump branchDouble(DoubleCondition cond, FPRegisterID left, FPRegisterID right)
    {
        m_assembler.vcmp(left, right);
        m_assembler.vmrs();

        if (cond == DoubleNotEqualAndOrdered) {
            // ConditionNE jumps if NotEqual *or* unordered - force the unordered cases not to jump.
            Jump unordered = makeBranch(ARMv7Assembler::ConditionVS);
            Jump result = makeBranch(ARMv7Assembler::ConditionNE);
            unordered.link(this);
            return result;
        }
        if (cond == DoubleEqualOrUnordered) {
            Jump unordered = makeBranch(ARMv7Assembler::ConditionVS);
            Jump notEqual = makeBranch(ARMv7Assembler::ConditionNE);
            unordered.link(this);
            // We get here if either unordered or equal.
            Jump result = jump();
            notEqual.link(this);
            return result;
        }
        return makeBranch(cond);
    }

    enum BranchTruncateType { BranchIfTruncateFailed, BranchIfTruncateSuccessful };
    Jump branchTruncateDoubleToInt32(FPRegisterID src, RegisterID dest, BranchTruncateType branchType = BranchIfTruncateFailed)
    {
        RegisterID scratch = getCachedDataTempRegisterIDAndInvalidate();
        // Convert into dest.
        m_assembler.vcvt_floatingPointToSigned(fpTempRegisterAsSingle(), src);
        m_assembler.vmov(dest, fpTempRegisterAsSingle());

        // Calculate 2x dest.  If the value potentially underflowed, it will have
        // clamped to 0x80000000, so 2x dest is zero in this case. In the case of
        // overflow the result will be equal to -2.
        Jump underflow = branchAdd32(Zero, dest, dest, scratch);
        Jump noOverflow = branch32(NotEqual, scratch, TrustedImm32(-2));

        // For BranchIfTruncateSuccessful, we branch if 'noOverflow' jumps.
        underflow.link(this);
        if (branchType == BranchIfTruncateSuccessful)
            return noOverflow;

        // We'll reach the current point in the code on failure, so plant a
        // jump here & link the success case.
        Jump failure = jump();
        noOverflow.link(this);
        return failure;
    }

    // Result is undefined if the value is outside of the integer range.
    void truncateDoubleToInt32(FPRegisterID src, RegisterID dest)
    {
        m_assembler.vcvt_floatingPointToSigned(fpTempRegisterAsSingle(), src);
        m_assembler.vmov(dest, fpTempRegisterAsSingle());
    }

    void truncateDoubleToUint32(FPRegisterID src, RegisterID dest)
    {
        m_assembler.vcvt_floatingPointToUnsigned(fpTempRegisterAsSingle(), src);
        m_assembler.vmov(dest, fpTempRegisterAsSingle());
    }
    
    // Convert 'src' to an integer, and places the resulting 'dest'.
    // If the result is not representable as a 32 bit value, branch.
    // May also branch for some values that are representable in 32 bits
    // (specifically, in this case, 0).
    void branchConvertDoubleToInt32(FPRegisterID src, RegisterID dest, JumpList& failureCases, FPRegisterID, bool negZeroCheck = true)
    {
        m_assembler.vcvt_floatingPointToSigned(fpTempRegisterAsSingle(), src);
        m_assembler.vmov(dest, fpTempRegisterAsSingle());

        // Convert the integer result back to float & compare to the original value - if not equal or unordered (NaN) then jump.
        m_assembler.vcvt_signedToFloatingPoint(fpTempRegister, fpTempRegisterAsSingle());
        failureCases.append(branchDouble(DoubleNotEqualOrUnordered, src, fpTempRegister));

        // Test for negative zero.
        if (negZeroCheck) {
            Jump valueIsNonZero = branchTest32(NonZero, dest);
            RegisterID scratch = getCachedDataTempRegisterIDAndInvalidate();
            m_assembler.vmov(scratch, ARMRegisters::asSingleUpper(src));
            failureCases.append(branch32(LessThan, scratch, TrustedImm32(0)));
            valueIsNonZero.link(this);
        }
    }

    Jump branchDoubleNonZero(FPRegisterID reg, FPRegisterID)
    {
        m_assembler.vcmpz(reg);
        m_assembler.vmrs();
        Jump unordered = makeBranch(ARMv7Assembler::ConditionVS);
        Jump result = makeBranch(ARMv7Assembler::ConditionNE);
        unordered.link(this);
        return result;
    }

    Jump branchDoubleZeroOrNaN(FPRegisterID reg, FPRegisterID)
    {
        m_assembler.vcmpz(reg);
        m_assembler.vmrs();
        Jump unordered = makeBranch(ARMv7Assembler::ConditionVS);
        Jump notEqual = makeBranch(ARMv7Assembler::ConditionNE);
        unordered.link(this);
        // We get here if either unordered or equal.
        Jump result = jump();
        notEqual.link(this);
        return result;
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
        m_assembler.pop(dest);
    }

    void push(RegisterID src)
    {
        m_assembler.push(src);
    }

    void push(Address address)
    {
        load32(address, dataTempRegister);
        push(dataTempRegister);
    }

    void push(TrustedImm32 imm)
    {
        move(imm, dataTempRegister);
        push(dataTempRegister);
    }

    void popPair(RegisterID dest1, RegisterID dest2)
    {
        m_assembler.pop(1 << dest1 | 1 << dest2);
    }
    
    void pushPair(RegisterID src1, RegisterID src2)
    {
        m_assembler.push(1 << src1 | 1 << src2);
    }

    ALWAYS_INLINE void long_move(TrustedImm32 imm, RegisterID dest)
    {
        uint32_t value = imm.m_value;
        m_assembler.mov(dest, ARMThumbImmediate::makeUInt16(value));
        if (value & 0xffff0000)
            m_assembler.movt(dest, ARMThumbImmediate::makeUInt16(value >> 16));
    }

    bool short_move(RegisterID dest, CachedTempRegister& cachedRegister, intptr_t valueAsInt)
    {
        intptr_t currentRegisterContents;
        if (cachedRegister.value(currentRegisterContents)) {
            intptr_t valueDelta = valueAsInt - currentRegisterContents;
            intptr_t valueDeltaSave = valueDelta;
            if (valueDelta < 0) {
                valueDelta = -valueDelta;
            } else if (!valueDelta) {
                // If valueDelta is 0, no need to emit or update anything.
                return true;
            }
            ARMThumbImmediate armImm = ARMThumbImmediate::makeEncodedImm(valueDelta);
            // Only use an add/sub if it results in a shorter instruction,
            // otherwise we introduce a data dependency for no gain.
            if (armImm.isValid() && armImm.isUInt8()) {
                if (valueDeltaSave > 0)
                    m_assembler.add(dest, dest, armImm);
                else if (valueDeltaSave < 0)
                    m_assembler.sub(dest, dest, armImm);
                return true;
            }
        }
        return false;
    }

    // Register move operations:
    //
    // Move values in registers.

    void move(TrustedImm32 imm, RegisterID dest)
    {
        uint32_t value = imm.m_value;

        ARMThumbImmediate armImm = ARMThumbImmediate::makeEncodedImm(value);
        intptr_t valueAsInt = reinterpret_cast<intptr_t>(reinterpret_cast<void *>(value));

        if (armImm.isValid())
            m_assembler.mov(dest, armImm);
        else if ((armImm = ARMThumbImmediate::makeEncodedImm(~value)).isValid())
            m_assembler.mvn(dest, armImm);
        else if (dest == addressTempRegister) {
            if (!short_move(dest, cachedAddressTempRegister(), valueAsInt))
                long_move(imm, dest);
        } else if (dest == dataTempRegister) {
            if (!short_move(dest, cachedDataTempRegister(), valueAsInt))
                long_move(imm, dest);
        } else {
            long_move(imm, dest);
        }

        if (dest == addressTempRegister)
            cachedAddressTempRegister().setValue(valueAsInt);
        else if (dest == dataTempRegister)
            cachedDataTempRegister().setValue(valueAsInt);

    }

    void move(RegisterID src, RegisterID dest)
    {
        if (src != dest)
            m_assembler.mov(dest, src);
        if (dest == dataTempRegister)
            cachedDataTempRegister().invalidate();
        else if (dest == addressTempRegister)
            invalidateCachedAddressTempRegister();
    }

    void move(TrustedImmPtr imm, RegisterID dest)
    {
        move(TrustedImm32(imm), dest);
    }

    void swap(RegisterID reg1, RegisterID reg2)
    {
        move(reg1, dataTempRegister);
        move(reg2, reg1);
        move(dataTempRegister, reg2);
    }

    void swap(FPRegisterID fr1, FPRegisterID fr2)
    {
        moveDouble(fr1, fpTempRegister);
        moveDouble(fr2, fr1);
        moveDouble(fpTempRegister, fr2);
    }

    void signExtend32ToPtr(RegisterID src, RegisterID dest)
    {
        move(src, dest);
    }

    void zeroExtend32ToWord(RegisterID src, RegisterID dest)
    {
        move(src, dest);
    }

    // Invert a relational condition, e.g. == becomes !=, < becomes >=, etc.
    static RelationalCondition invert(RelationalCondition cond)
    {
        return static_cast<RelationalCondition>(cond ^ 1);
    }

    void nop()
    {
        m_assembler.nop();
    }
    
    void memoryFence()
    {
        m_assembler.dmbSY();
    }
    
    void storeFence()
    {
        m_assembler.dmbISHST();
    }

    template<PtrTag startTag, PtrTag destTag>
    static void replaceWithJump(CodeLocationLabel<startTag> instructionStart, CodeLocationLabel<destTag> destination)
    {
        ARMv7Assembler::replaceWithJump(instructionStart.dataLocation(), destination.dataLocation());
    }
    
    static ptrdiff_t maxJumpReplacementSize()
    {
        return ARMv7Assembler::maxJumpReplacementSize();
    }

    static ptrdiff_t patchableJumpSize()
    {
        return ARMv7Assembler::patchableJumpSize();
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
    // jle32(reg1, TrustedImm32(5)) will branch if the value held in reg1, when
    // treated as a signed 32bit value, is less than or equal to 5.
    //
    // jz and jnz test whether the first operand is equal to zero, and take
    // an optional second operand of a mask under which to perform the test.
private:

    // Should we be using TEQ for equal/not-equal?
    void compare32AndSetFlags(RegisterID left, TrustedImm32 right)
    {
        int32_t imm = right.m_value;
        ARMThumbImmediate armImm = ARMThumbImmediate::makeEncodedImm(imm);
        if (armImm.isValid()) {
            m_assembler.cmp(left, armImm);
            return;
        }

        armImm = ARMThumbImmediate::makeEncodedImm(-imm);
        if (armImm.isValid()) {
            if (!(left & 8) && armImm.isUInt3() && (left != addressTempRegister)) {
                // This is common enough to warrant a special case to save 2 bytes
                RegisterID scratch = getCachedAddressTempRegisterIDAndInvalidate();
                m_assembler.add_S(scratch, left, armImm);
                return;
            }
            m_assembler.cmn(left, armImm);
            return;
        }

        RegisterID scratch = bestTempRegister(left);
        move(TrustedImm32(imm), scratch);
        m_assembler.cmp(left, scratch);
    }

    void add32Impl(TrustedImm32 imm, Address address, bool updateFlags = false)
    {
        load32(address, dataTempRegister);

        ARMThumbImmediate armImm = ARMThumbImmediate::makeEncodedImm(imm.m_value);
        if (armImm.isValid()) {
            if (updateFlags)
                m_assembler.add_S(dataTempRegister, dataTempRegister, armImm);
            else
                m_assembler.add(dataTempRegister, dataTempRegister, armImm);
        } else {
            // Hrrrm, since dataTempRegister holds the data loaded,
            // use addressTempRegister to hold the immediate.
            move(imm, addressTempRegister);
            if (updateFlags)
                m_assembler.add_S(dataTempRegister, dataTempRegister, addressTempRegister);
            else
                m_assembler.add(dataTempRegister, dataTempRegister, addressTempRegister);
        }

        store32(dataTempRegister, address);
    }

    void add32Impl(TrustedImm32 imm, AbsoluteAddress address, bool updateFlags = false)
    {
        load32(setupArmAddress(address), dataTempRegister);

        ARMThumbImmediate armImm = ARMThumbImmediate::makeEncodedImm(imm.m_value);
        if (armImm.isValid()) {
            if (updateFlags)
                m_assembler.add_S(dataTempRegister, dataTempRegister, armImm);
            else
                m_assembler.add(dataTempRegister, dataTempRegister, armImm);
        } else {
            // Hrrrm, since dataTempRegister holds the data loaded,
            // use addressTempRegister to hold the immediate.
            move(imm, addressTempRegister);
            if (updateFlags)
                m_assembler.add_S(dataTempRegister, dataTempRegister, addressTempRegister);
            else
                m_assembler.add(dataTempRegister, dataTempRegister, addressTempRegister);
        }

        store32(dataTempRegister, address.m_ptr);
    }

public:
    void test32(RegisterID reg, TrustedImm32 mask = TrustedImm32(-1))
    {
        int32_t imm = mask.m_value;

        if (imm == -1)
            m_assembler.tst(reg, reg);
        else {
            ARMThumbImmediate armImm = ARMThumbImmediate::makeEncodedImm(imm);
            if (armImm.isValid()) {
                if (reg == ARMRegisters::sp) {
                    move(reg, addressTempRegister);
                    m_assembler.tst(addressTempRegister, armImm);
                } else
                    m_assembler.tst(reg, armImm);
            } else {
                if (reg == ARMRegisters::sp) {
                    move(reg, dataTempRegister);
                    reg = dataTempRegister;
                }
                RegisterID scratch = bestTempRegister(reg);
                move(mask, scratch);
                m_assembler.tst(reg, scratch);
            }
        }
    }
    
    Jump branch(ResultCondition cond)
    {
        return Jump(makeBranch(cond));
    }

    Jump branch32(RelationalCondition cond, RegisterID left, RegisterID right)
    {
        if (left == ARMRegisters::sp) {
            move(left, addressTempRegister);
            m_assembler.cmp(addressTempRegister, right);
        } else if (right == ARMRegisters::sp) {
            move(right, addressTempRegister);
            m_assembler.cmp(left, addressTempRegister);
        } else
            m_assembler.cmp(left, right);
        return Jump(makeBranch(cond));
    }

    Jump branch32(RelationalCondition cond, RegisterID left, TrustedImm32 right)
    {
        compare32AndSetFlags(left, right);
        return Jump(makeBranch(cond));
    }

    Jump branch32(RelationalCondition cond, RegisterID left, Address right)
    {
        load32(right, addressTempRegister);
        return branch32(cond, left, addressTempRegister);
    }

    Jump branch32(RelationalCondition cond, Address left, RegisterID right)
    {
        load32(left, addressTempRegister);
        return branch32(cond, addressTempRegister, right);
    }

    Jump branch32(RelationalCondition cond, Address left, TrustedImm32 right)
    {
        // use addressTempRegister incase the branch32 we call uses dataTempRegister. :-/
        load32(left, addressTempRegister);
        return branch32(cond, addressTempRegister, right);
    }

    Jump branch32(RelationalCondition cond, BaseIndex left, TrustedImm32 right)
    {
        // use addressTempRegister incase the branch32 we call uses dataTempRegister. :-/
        load32(left, addressTempRegister);
        return branch32(cond, addressTempRegister, right);
    }

    Jump branch32WithUnalignedHalfWords(RelationalCondition cond, BaseIndex left, TrustedImm32 right)
    {
        // use addressTempRegister incase the branch32 we call uses dataTempRegister. :-/
        load32WithUnalignedHalfWords(left, addressTempRegister);
        return branch32(cond, addressTempRegister, right);
    }

    Jump branch32(RelationalCondition cond, AbsoluteAddress left, RegisterID right)
    {
        load32(left.m_ptr, addressTempRegister);
        return branch32(cond, addressTempRegister, right);
    }

    Jump branch32(RelationalCondition cond, AbsoluteAddress left, TrustedImm32 right)
    {
        load32(left.m_ptr, addressTempRegister);
        return branch32(cond, addressTempRegister, right);
    }

    Jump branchPtr(RelationalCondition cond, BaseIndex left, RegisterID right)
    {
        load32(left, dataTempRegister);
        return branch32(cond, dataTempRegister, right);
    }

    Jump branch8(RelationalCondition cond, RegisterID left, TrustedImm32 right)
    {
        TrustedImm32 right8 = MacroAssemblerHelpers::mask8OnCondition(*this, cond, right);
        compare32AndSetFlags(left, right8);
        return Jump(makeBranch(cond));
    }

    Jump branch8(RelationalCondition cond, Address left, TrustedImm32 right)
    {
        // use addressTempRegister incase the branch8 we call uses dataTempRegister. :-/
        RegisterID scratch = getCachedAddressTempRegisterIDAndInvalidate();
        TrustedImm32 right8 = MacroAssemblerHelpers::mask8OnCondition(*this, cond, right);
        MacroAssemblerHelpers::load8OnCondition(*this, cond, left, scratch);
        return branch8(cond, scratch, right8);
    }

    Jump branch8(RelationalCondition cond, BaseIndex left, TrustedImm32 right)
    {
        // use addressTempRegister incase the branch32 we call uses dataTempRegister. :-/
        RegisterID scratch = getCachedAddressTempRegisterIDAndInvalidate();
        TrustedImm32 right8 = MacroAssemblerHelpers::mask8OnCondition(*this, cond, right);
        MacroAssemblerHelpers::load8OnCondition(*this, cond, left, scratch);
        return branch32(cond, scratch, right8);
    }
    
    Jump branch8(RelationalCondition cond, AbsoluteAddress address, TrustedImm32 right)
    {
        // Use addressTempRegister instead of dataTempRegister, since branch32 uses dataTempRegister.
        TrustedImm32 right8 = MacroAssemblerHelpers::mask8OnCondition(*this, cond, right);
        ArmAddress armAddress = setupArmAddress(address);
        MacroAssemblerHelpers::load8OnCondition(*this, cond, armAddress, addressTempRegister);
        return branch32(cond, addressTempRegister, right8);
    }
    
    Jump branchTest32(ResultCondition cond, RegisterID reg, RegisterID mask)
    {
        ASSERT(cond == Zero || cond == NonZero || cond == Signed || cond == PositiveOrZero);
        m_assembler.tst(reg, mask);
        return Jump(makeBranch(cond));
    }

    Jump branchTest32(ResultCondition cond, RegisterID reg, TrustedImm32 mask = TrustedImm32(-1))
    {
        ASSERT(cond == Zero || cond == NonZero || cond == Signed || cond == PositiveOrZero);
        test32(reg, mask);
        return Jump(makeBranch(cond));
    }

    Jump branchTest32(ResultCondition cond, Address address, TrustedImm32 mask = TrustedImm32(-1))
    {
        // use addressTempRegister incase the branchTest32 we call uses dataTempRegister. :-/
        load32(address, addressTempRegister);
        return branchTest32(cond, addressTempRegister, mask);
    }

    Jump branchTest32(ResultCondition cond, BaseIndex address, TrustedImm32 mask = TrustedImm32(-1))
    {
        // use addressTempRegister incase the branchTest32 we call uses dataTempRegister. :-/
        load32(address, addressTempRegister);
        return branchTest32(cond, addressTempRegister, mask);
    }

    Jump branchTest32(ResultCondition cond, AbsoluteAddress address, TrustedImm32 mask = TrustedImm32(-1))
    {
        // use addressTempRegister incase the branchTest32 we call uses dataTempRegister. :-/
        load32(setupArmAddress(address), addressTempRegister);
        return branchTest32(cond, addressTempRegister, mask);
    }

    Jump branchTest8(ResultCondition cond, BaseIndex address, TrustedImm32 mask = TrustedImm32(-1))
    {
        // use addressTempRegister incase the branchTest32 we call uses dataTempRegister. :-/
        RegisterID scratch = getCachedAddressTempRegisterIDAndInvalidate();
        TrustedImm32 mask8 = MacroAssemblerHelpers::mask8OnCondition(*this, cond, mask);
        MacroAssemblerHelpers::load8OnCondition(*this, cond, address, scratch);
        return branchTest32(cond, scratch, mask8);
    }

    Jump branchTest8(ResultCondition cond, Address address, TrustedImm32 mask = TrustedImm32(-1))
    {
        // use addressTempRegister incase the branchTest32 we call uses dataTempRegister. :-/
        RegisterID scratch = getCachedAddressTempRegisterIDAndInvalidate();
        TrustedImm32 mask8 = MacroAssemblerHelpers::mask8OnCondition(*this, cond, mask);
        MacroAssemblerHelpers::load8OnCondition(*this, cond, address, scratch);
        return branchTest32(cond, scratch, mask8);
    }

    Jump branchTest8(ResultCondition cond, AbsoluteAddress address, TrustedImm32 mask = TrustedImm32(-1))
    {
        // use addressTempRegister incase the branchTest32 we call uses dataTempRegister. :-/
        TrustedImm32 mask8 = MacroAssemblerHelpers::mask8OnCondition(*this, cond, mask);
        ArmAddress armAddress = setupArmAddress(address);
        MacroAssemblerHelpers::load8OnCondition(*this, cond, armAddress, addressTempRegister);
        return branchTest32(cond, addressTempRegister, mask8);
    }

    Jump branchTest16(ResultCondition cond, BaseIndex address, TrustedImm32 mask = TrustedImm32(-1))
    {
        // use addressTempRegister incase the branchTest32 we call uses dataTempRegister. :-/
        RegisterID scratch = getCachedAddressTempRegisterIDAndInvalidate();
        TrustedImm32 mask16 = MacroAssemblerHelpers::mask16OnCondition(*this, cond, mask);
        MacroAssemblerHelpers::load16OnCondition(*this, cond, address, scratch);
        return branchTest32(cond, scratch, mask16);
    }

    Jump branchTest16(ResultCondition cond, Address address, TrustedImm32 mask = TrustedImm32(-1))
    {
        // use addressTempRegister incase the branchTest32 we call uses dataTempRegister. :-/
        RegisterID scratch = getCachedAddressTempRegisterIDAndInvalidate();
        TrustedImm32 mask16 = MacroAssemblerHelpers::mask16OnCondition(*this, cond, mask);
        MacroAssemblerHelpers::load16OnCondition(*this, cond, address, scratch);
        return branchTest32(cond, scratch, mask16);
    }

    Jump branchTest16(ResultCondition cond, AbsoluteAddress address, TrustedImm32 mask = TrustedImm32(-1))
    {
        // use addressTempRegister incase the branchTest32 we call uses dataTempRegister. :-/
        TrustedImm32 mask16 = MacroAssemblerHelpers::mask16OnCondition(*this, cond, mask);
        ArmAddress armAddress = setupArmAddress(address);
        MacroAssemblerHelpers::load16OnCondition(*this, cond, armAddress, addressTempRegister);
        return branchTest32(cond, addressTempRegister, mask16);
    }

    void farJump(RegisterID target, PtrTag)
    {
        cachedDataTempRegister().invalidate();
        invalidateCachedAddressTempRegister();
        m_assembler.bx(target);
    }

    void farJump(TrustedImmPtr target, PtrTag)
    {
        move(target, addressTempRegister);
        cachedDataTempRegister().invalidate();
        invalidateCachedAddressTempRegister();
        m_assembler.bx(addressTempRegister);
    }

    // Address is a memory location containing the address to jump to
    void farJump(Address address, PtrTag)
    {
        load32(address, addressTempRegister);
        cachedDataTempRegister().invalidate(); // addressTempRegister already invalidated by the load
        m_assembler.bx(addressTempRegister);
    }
    
    void farJump(AbsoluteAddress address, PtrTag)
    {
        load32(setupArmAddress(address), addressTempRegister);
        cachedDataTempRegister().invalidate();
        m_assembler.bx(addressTempRegister);
    }

    ALWAYS_INLINE void farJump(RegisterID target, RegisterID jumpTag) { UNUSED_PARAM(jumpTag), farJump(target, NoPtrTag); }
    ALWAYS_INLINE void farJump(Address address, RegisterID jumpTag) { UNUSED_PARAM(jumpTag), farJump(address, NoPtrTag); }
    ALWAYS_INLINE void farJump(AbsoluteAddress address, RegisterID jumpTag) { UNUSED_PARAM(jumpTag), farJump(address, NoPtrTag); }

    // Arithmetic control flow operations:
    //
    // This set of conditional branch operations branch based
    // on the result of an arithmetic operation.  The operation
    // is performed as normal, storing the result.
    //
    // * jz operations branch if the result is zero.
    // * jo operations branch if the (signed) arithmetic
    //   operation caused an overflow to occur.
    
    Jump branchAdd32(ResultCondition cond, RegisterID op1, RegisterID op2, RegisterID dest)
    {
        m_assembler.add_S(dest, op1, op2);
        return Jump(makeBranch(cond));
    }

    Jump branchAdd32(ResultCondition cond, RegisterID op1, TrustedImm32 imm, RegisterID dest)
    {
        ARMThumbImmediate armImm = ARMThumbImmediate::makeEncodedImm(imm.m_value);
        if (armImm.isValid())
            m_assembler.add_S(dest, op1, armImm);
        else {
            move(imm, dataTempRegister);
            m_assembler.add_S(dest, op1, dataTempRegister);
        }
        return Jump(makeBranch(cond));
    }

    Jump branchAdd32(ResultCondition cond, RegisterID src, RegisterID dest)
    {
        return branchAdd32(cond, dest, src, dest);
    }

    Jump branchAdd32(ResultCondition cond, Address src, RegisterID dest)
    {
        load32(src, dataTempRegister);
        return branchAdd32(cond, dest, dataTempRegister, dest);
    }

    Jump branchAdd32(ResultCondition cond, TrustedImm32 imm, RegisterID dest)
    {
        return branchAdd32(cond, dest, imm, dest);
    }

    Jump branchAdd32(ResultCondition cond, TrustedImm32 imm, AbsoluteAddress dest)
    {
        constexpr bool updateFlags = true;
        add32Impl(imm, dest, updateFlags);
        return Jump(makeBranch(cond));
    }

    Jump branchAdd32(ResultCondition cond, TrustedImm32 imm, Address dest)
    {
        constexpr bool updateFlags = true;
        add32Impl(imm, dest, updateFlags);
        return Jump(makeBranch(cond));
    }

    Jump branchMul32(ResultCondition cond, RegisterID src1, RegisterID src2, RegisterID dest)
    {
        m_assembler.smull(dest, dataTempRegister, src1, src2);
        // The invalidation of cachedDataTempRegister is handled by the branch.
        if (cond == Overflow) {
            RegisterID scratch = getCachedAddressTempRegisterIDAndInvalidate();
            m_assembler.asr(scratch, dest, 31);
            return branch32(NotEqual, scratch, dataTempRegister);
        }

        return branchTest32(cond, dest);
    }

    Jump branchMul32(ResultCondition cond, RegisterID src, RegisterID dest)
    {
        return branchMul32(cond, src, dest, dest);
    }

    Jump branchMul32(ResultCondition cond, RegisterID src, TrustedImm32 imm, RegisterID dest)
    {
        move(imm, dataTempRegister);
        return branchMul32(cond, dataTempRegister, src, dest);
    }

    Jump branchNeg32(ResultCondition cond, RegisterID srcDest)
    {
        ARMThumbImmediate zero = ARMThumbImmediate::makeUInt12(0);
        m_assembler.sub_S(srcDest, zero, srcDest);
        return Jump(makeBranch(cond));
    }

    Jump branchOr32(ResultCondition cond, RegisterID src, RegisterID dest)
    {
        m_assembler.orr_S(dest, dest, src);
        return Jump(makeBranch(cond));
    }

    Jump branchSub32(ResultCondition cond, RegisterID op1, RegisterID op2, RegisterID dest)
    {
        m_assembler.sub_S(dest, op1, op2);
        return Jump(makeBranch(cond));
    }

    Jump branchSub32(ResultCondition cond, RegisterID op1, TrustedImm32 imm, RegisterID dest)
    {
        ARMThumbImmediate armImm = ARMThumbImmediate::makeEncodedImm(imm.m_value);
        if (armImm.isValid())
            m_assembler.sub_S(dest, op1, armImm);
        else {
            move(imm, dataTempRegister);
            m_assembler.sub_S(dest, op1, dataTempRegister);
        }
        return Jump(makeBranch(cond));
    }
    
    Jump branchSub32(ResultCondition cond, RegisterID src, RegisterID dest)
    {
        return branchSub32(cond, dest, src, dest);
    }

    Jump branchSub32(ResultCondition cond, TrustedImm32 imm, RegisterID dest)
    {
        return branchSub32(cond, dest, imm, dest);
    }
    
    void relativeTableJump(RegisterID index, int scale)
    {
        ASSERT(scale >= 0 && scale <= 31);

        // dataTempRegister will point after the jump if index register contains zero
        move(ARMRegisters::pc, dataTempRegister);
        m_assembler.add(dataTempRegister, dataTempRegister, ARMThumbImmediate::makeEncodedImm(9));

        ShiftTypeAndAmount shift(SRType_LSL, scale);
        m_assembler.add(dataTempRegister, dataTempRegister, index, shift);
        farJump(dataTempRegister, NoPtrTag);
    }

    // Miscellaneous operations:

    void breakpoint(uint8_t imm = 0)
    {
        m_assembler.bkpt(imm);
    }

    static bool isBreakpoint(void* address) { return ARMv7Assembler::isBkpt(address); }

    ALWAYS_INLINE Call nearCall()
    {
        invalidateAllTempRegisters();
        return Call(m_assembler.bl(), Call::LinkableNear);
    }

    ALWAYS_INLINE Call nearTailCall()
    {
        invalidateAllTempRegisters();
        return Call(m_assembler.b(), Call::LinkableNearTail);
    }

    // FIXME: why is this the same than nearCall() in ARM64? is it right?
    ALWAYS_INLINE Call threadSafePatchableNearCall()
    {
        invalidateAllTempRegisters();
        moveFixedWidthEncoding(TrustedImm32(0), dataTempRegister);
        return Call(m_assembler.blx(dataTempRegister), Call::LinkableNear);
    }

    ALWAYS_INLINE Call call(PtrTag)
    {
        moveFixedWidthEncoding(TrustedImm32(0), dataTempRegister);
        invalidateAllTempRegisters();
        return Call(m_assembler.blx(dataTempRegister), Call::Linkable);
    }

    ALWAYS_INLINE Call call(RegisterID target, PtrTag)
    {
        invalidateAllTempRegisters();
        return Call(m_assembler.blx(target), Call::None);
    }

    ALWAYS_INLINE Call call(Address address, PtrTag)
    {
        load32(address, addressTempRegister);
        cachedDataTempRegister().invalidate();
        return Call(m_assembler.blx(addressTempRegister), Call::None);
    }

    ALWAYS_INLINE Call call(RegisterID callTag) { return UNUSED_PARAM(callTag), call(NoPtrTag); }
    ALWAYS_INLINE Call call(RegisterID target, RegisterID callTag) { return UNUSED_PARAM(callTag), call(target, NoPtrTag); }
    ALWAYS_INLINE Call call(Address address, RegisterID callTag) { return UNUSED_PARAM(callTag), call(address, NoPtrTag); }

    ALWAYS_INLINE void callOperation(const CodePtr<OperationPtrTag> operation)
    {
        move(TrustedImmPtr(operation.taggedPtr()), addressTempRegister);
        call(addressTempRegister, OperationPtrTag);
    }

    ALWAYS_INLINE void ret()
    {
        m_assembler.bx(linkRegister);
    }

    void compare32(RelationalCondition cond, RegisterID left, RegisterID right, RegisterID dest)
    {
        m_assembler.cmp(left, right);
        m_assembler.it(armV7Condition(cond), false);
        m_assembler.mov(dest, ARMThumbImmediate::makeUInt16(1));
        m_assembler.mov(dest, ARMThumbImmediate::makeUInt16(0));
    }

    void compare32(RelationalCondition cond, Address left, RegisterID right, RegisterID dest)
    {
        load32(left, addressTempRegister);
        compare32(cond, addressTempRegister, right, dest);
    }

    void compare8(RelationalCondition cond, Address left, TrustedImm32 right, RegisterID dest)
    {
        TrustedImm32 right8 = MacroAssemblerHelpers::mask8OnCondition(*this, cond, right);
        RegisterID scratch = getCachedAddressTempRegisterIDAndInvalidate();
        MacroAssemblerHelpers::load8OnCondition(*this, cond, left, scratch);
        compare32(cond, scratch, right8, dest);
    }

    void compare32(RelationalCondition cond, RegisterID left, TrustedImm32 right, RegisterID dest)
    {
        compare32AndSetFlags(left, right);
        m_assembler.it(armV7Condition(cond), false);
        m_assembler.mov(dest, ARMThumbImmediate::makeUInt16(1));
        m_assembler.mov(dest, ARMThumbImmediate::makeUInt16(0));
    }

    // FIXME:
    // The mask should be optional... paerhaps the argument order should be
    // dest-src, operations always have a dest? ... possibly not true, considering
    // asm ops like test, or pseudo ops like pop().
    void test32(ResultCondition cond, Address address, TrustedImm32 mask, RegisterID dest)
    {
        load32(address, addressTempRegister);
        test32(addressTempRegister, mask);
        m_assembler.it(armV7Condition(cond), false);
        m_assembler.mov(dest, ARMThumbImmediate::makeUInt16(1));
        m_assembler.mov(dest, ARMThumbImmediate::makeUInt16(0));
    }

    void test8(ResultCondition cond, Address address, TrustedImm32 mask, RegisterID dest)
    {
        TrustedImm32 mask8 = MacroAssemblerHelpers::mask8OnCondition(*this, cond, mask);
        RegisterID scratch = getCachedAddressTempRegisterIDAndInvalidate();
        MacroAssemblerHelpers::load8OnCondition(*this, cond, address, scratch);
        test32(scratch, mask8);
        m_assembler.it(armV7Condition(cond), false);
        m_assembler.mov(dest, ARMThumbImmediate::makeUInt16(1));
        m_assembler.mov(dest, ARMThumbImmediate::makeUInt16(0));
    }

    ALWAYS_INLINE DataLabel32 moveWithPatch(TrustedImm32 imm, RegisterID dst)
    {
        padBeforePatch();
        moveFixedWidthEncoding(imm, dst);
        return DataLabel32(this);
    }

    ALWAYS_INLINE DataLabelPtr moveWithPatch(TrustedImmPtr imm, RegisterID dst)
    {
        padBeforePatch();
        moveFixedWidthEncoding(TrustedImm32(imm), dst);
        return DataLabelPtr(this);
    }

    ALWAYS_INLINE Jump branchPtrWithPatch(RelationalCondition cond, RegisterID left, DataLabelPtr& dataLabel, TrustedImmPtr initialRightValue = TrustedImmPtr(nullptr))
    {
        dataLabel = moveWithPatch(initialRightValue, dataTempRegister);
        return branch32(cond, left, dataTempRegister);
    }

    ALWAYS_INLINE Jump branchPtrWithPatch(RelationalCondition cond, Address left, DataLabelPtr& dataLabel, TrustedImmPtr initialRightValue = TrustedImmPtr(nullptr))
    {
        load32(left, addressTempRegister);
        dataLabel = moveWithPatch(initialRightValue, dataTempRegister);
        return branch32(cond, addressTempRegister, dataTempRegister);
    }
    
    ALWAYS_INLINE Jump branch32WithPatch(RelationalCondition cond, Address left, DataLabel32& dataLabel, TrustedImm32 initialRightValue = TrustedImm32(0))
    {
        load32(left, addressTempRegister);
        dataLabel = moveWithPatch(initialRightValue, dataTempRegister);
        return branch32(cond, addressTempRegister, dataTempRegister);
    }
    
    PatchableJump patchableBranchPtr(RelationalCondition cond, Address left, TrustedImmPtr right = TrustedImmPtr(nullptr))
    {
        m_makeJumpPatchable = true;
        Jump result = branch32(cond, left, TrustedImm32(right));
        m_makeJumpPatchable = false;
        return PatchableJump(result);
    }
    
    PatchableJump patchableBranchTest32(ResultCondition cond, RegisterID reg, TrustedImm32 mask = TrustedImm32(-1))
    {
        m_makeJumpPatchable = true;
        Jump result = branchTest32(cond, reg, mask);
        m_makeJumpPatchable = false;
        return PatchableJump(result);
    }

    PatchableJump patchableBranch8(RelationalCondition cond, Address left, TrustedImm32 imm)
    {
        m_makeJumpPatchable = true;
        Jump result = branch8(cond, left, imm);
        m_makeJumpPatchable = false;
        return PatchableJump(result);
    }

    PatchableJump patchableBranch32(RelationalCondition cond, RegisterID reg, TrustedImm32 imm)
    {
        m_makeJumpPatchable = true;
        Jump result = branch32(cond, reg, imm);
        m_makeJumpPatchable = false;
        return PatchableJump(result);
    }

    PatchableJump patchableBranch32(RelationalCondition cond, Address left, TrustedImm32 imm)
    {
        m_makeJumpPatchable = true;
        Jump result = branch32(cond, left, imm);
        m_makeJumpPatchable = false;
        return PatchableJump(result);
    }

    PatchableJump patchableBranchPtrWithPatch(RelationalCondition cond, Address left, DataLabelPtr& dataLabel, TrustedImmPtr initialRightValue = TrustedImmPtr(nullptr))
    {
        m_makeJumpPatchable = true;
        Jump result = branchPtrWithPatch(cond, left, dataLabel, initialRightValue);
        m_makeJumpPatchable = false;
        return PatchableJump(result);
    }

    PatchableJump patchableBranch32WithPatch(RelationalCondition cond, Address left, DataLabel32& dataLabel, TrustedImm32 initialRightValue = TrustedImm32(0))
    {
        m_makeJumpPatchable = true;
        Jump result = branch32WithPatch(cond, left, dataLabel, initialRightValue);
        m_makeJumpPatchable = false;
        return PatchableJump(result);
    }

    PatchableJump patchableJump()
    {
        padBeforePatch();
        m_makeJumpPatchable = true;
        Jump result = jump();
        m_makeJumpPatchable = false;
        return PatchableJump(result);
    }

    ALWAYS_INLINE DataLabelPtr storePtrWithPatch(TrustedImmPtr initialValue, Address address)
    {
        DataLabelPtr label = moveWithPatch(initialValue, dataTempRegister);
        store32(dataTempRegister, address);
        return label;
    }
    ALWAYS_INLINE DataLabelPtr storePtrWithPatch(Address address) { return storePtrWithPatch(TrustedImmPtr(nullptr), address); }

    template<PtrTag resultTag, PtrTag locationTag>
    static CodePtr<resultTag> readCallTarget(CodeLocationCall<locationTag> call)
    {
        return CodePtr<resultTag>(reinterpret_cast<void(*)()>(ARMv7Assembler::readCallTarget(call.dataLocation())));
    }
    
    static bool canJumpReplacePatchableBranchPtrWithPatch() { return false; }
    static bool canJumpReplacePatchableBranch32WithPatch() { return false; }
    
    template<PtrTag tag>
    static CodeLocationLabel<tag> startOfBranchPtrWithPatchOnRegister(CodeLocationDataLabelPtr<tag> label)
    {
        const unsigned twoWordOpSize = 4;
        return label.labelAtOffset(-twoWordOpSize * 2);
    }
    
    template<PtrTag tag>
    static void revertJumpReplacementToBranchPtrWithPatch(CodeLocationLabel<tag> instructionStart, RegisterID rd, void* initialValue)
    {
#if OS(LINUX)
        ARMv7Assembler::revertJumpTo_movT3movtcmpT2(instructionStart.dataLocation(), rd, dataTempRegister, reinterpret_cast<uintptr_t>(initialValue));
#else
        UNUSED_PARAM(rd);
        ARMv7Assembler::revertJumpTo_movT3(instructionStart.dataLocation(), dataTempRegister, ARMThumbImmediate::makeUInt16(reinterpret_cast<uintptr_t>(initialValue) & 0xffff));
#endif
    }

    template<PtrTag tag>
    static CodeLocationLabel<tag> startOfPatchableBranchPtrWithPatchOnAddress(CodeLocationDataLabelPtr<tag>)
    {
        UNREACHABLE_FOR_PLATFORM();
        return CodeLocationLabel<tag>();
    }

    template<PtrTag tag>
    static CodeLocationLabel<tag> startOfPatchableBranch32WithPatchOnAddress(CodeLocationDataLabel32<tag>)
    {
        UNREACHABLE_FOR_PLATFORM();
        return CodeLocationLabel<tag>();
    }

    template<PtrTag tag>
    static void revertJumpReplacementToPatchableBranchPtrWithPatch(CodeLocationLabel<tag>, Address, void*)
    {
        UNREACHABLE_FOR_PLATFORM();
    }

    template<PtrTag tag>
    static void revertJumpReplacementToPatchableBranch32WithPatch(CodeLocationLabel<tag>, Address, int32_t)
    {
        UNREACHABLE_FOR_PLATFORM();
    }

    template<PtrTag callTag, PtrTag destTag>
    static void repatchCall(CodeLocationCall<callTag> call, CodeLocationLabel<destTag> destination)
    {
        ARMv7Assembler::relinkCall(call.dataLocation(), destination.taggedPtr());
    }

    template<PtrTag callTag, PtrTag destTag>
    static void repatchCall(CodeLocationCall<callTag> call, CodePtr<destTag> destination)
    {
        ARMv7Assembler::relinkCall(call.dataLocation(), destination.taggedPtr());
    }

protected:
    ALWAYS_INLINE Jump jump()
    {
        m_assembler.label(); // Force nop-padding if we're in the middle of a watchpoint.
        moveFixedWidthEncoding(TrustedImm32(0), dataTempRegister);
        cachedDataTempRegister().invalidate();
        return Jump(m_assembler.bx(dataTempRegister), m_makeJumpPatchable ? ARMv7Assembler::JumpNoConditionFixedSize : ARMv7Assembler::JumpNoCondition);
    }

    ALWAYS_INLINE Jump makeBranch(ARMv7Assembler::Condition cond)
    {
        m_assembler.label(); // Force nop-padding if we're in the middle of a watchpoint.
        m_assembler.it(cond, true, true);
        moveFixedWidthEncoding(TrustedImm32(0), dataTempRegister);
        cachedDataTempRegister().invalidate();
        return Jump(m_assembler.bx(dataTempRegister), m_makeJumpPatchable ? ARMv7Assembler::JumpConditionFixedSize : ARMv7Assembler::JumpCondition, cond);
    }
    ALWAYS_INLINE Jump makeBranch(RelationalCondition cond) { return makeBranch(armV7Condition(cond)); }
    ALWAYS_INLINE Jump makeBranch(ResultCondition cond) { return makeBranch(armV7Condition(cond)); }
    ALWAYS_INLINE Jump makeBranch(DoubleCondition cond) { return makeBranch(armV7Condition(cond)); }

    ArmAddress setupArmAddress(BaseIndex address)
    {
        if (address.offset) {
            ARMThumbImmediate imm = ARMThumbImmediate::makeUInt12OrEncodedImm(address.offset);
            if (imm.isValid()) {
                RegisterID scratch = getCachedAddressTempRegisterIDAndInvalidate();
                m_assembler.add(scratch, address.base, imm);
            } else {
                move(TrustedImm32(address.offset), addressTempRegister);
                m_assembler.add(addressTempRegister, addressTempRegister, address.base);
                cachedAddressTempRegister().invalidate();
            }

            return ArmAddress(addressTempRegister, address.index, address.scale);
        } else
            return ArmAddress(address.base, address.index, address.scale);
    }

    ArmAddress setupArmAddress(Address address)
    {
        if (BoundsNonDoubleWordOffset::within(address.offset))
            return ArmAddress(address.base, address.offset);

        move(TrustedImm32(address.offset), addressTempRegister);
        return ArmAddress(address.base, addressTempRegister);
    }

    template <class Bounds>
    std::optional<int32_t> absoluteAddressWithinShortOffset(AbsoluteAddress address, CachedTempRegister &cachedRegister)
    {
        intptr_t addressAsInt = reinterpret_cast<uintptr_t>(address.m_ptr);
        intptr_t currentRegisterContents;
        if (cachedRegister.value(currentRegisterContents)) {
            intptr_t addressDelta = addressAsInt - currentRegisterContents;
            if (Bounds::within(addressDelta))
                return reinterpret_cast<int32_t>(addressDelta);
        }
        return { };
    }

    template<class Bounds = BoundsNonDoubleWordOffset>
    ArmAddress setupArmAddress(AbsoluteAddress address, RegisterID scratch = addressTempRegister)
    {
        if (auto offset = absoluteAddressWithinShortOffset<Bounds>(address, cachedAddressTempRegister()))
            return ArmAddress(addressTempRegister, *offset);
        if (auto offset = absoluteAddressWithinShortOffset<Bounds>(address, cachedDataTempRegister()))
            return ArmAddress(dataTempRegister, *offset);
        move(TrustedImmPtr(address.m_ptr), scratch);
        return ArmAddress(scratch);
    }

    RegisterID makeBaseIndexBase(BaseIndex address)
    {
        if (!address.offset)
            return address.base;

        ARMThumbImmediate imm = ARMThumbImmediate::makeUInt12OrEncodedImm(address.offset);
        if (imm.isValid())
            m_assembler.add(addressTempRegister, address.base, imm);
        else {
            move(TrustedImm32(address.offset), addressTempRegister);
            m_assembler.add(addressTempRegister, addressTempRegister, address.base);
        }

        cachedAddressTempRegister().invalidate();
        return addressTempRegister;
    }

    void moveFixedWidthEncoding(TrustedImm32 imm, RegisterID dst)
    {
        uint32_t value = imm.m_value;
        intptr_t valueAsInt = reinterpret_cast<intptr_t>(reinterpret_cast<void *>(value));
        if (dst == dataTempRegister)
            cachedDataTempRegister().setValue(valueAsInt);
        else if (dst == addressTempRegister)
            cachedAddressTempRegister().setValue(valueAsInt);
        m_assembler.movT3(dst, ARMThumbImmediate::makeUInt16(value & 0xffff));
        m_assembler.movt(dst, ARMThumbImmediate::makeUInt16(value >> 16));
    }

    ARMv7Assembler::Condition armV7Condition(RelationalCondition cond)
    {
        return static_cast<ARMv7Assembler::Condition>(cond);
    }

    ARMv7Assembler::Condition armV7Condition(ResultCondition cond)
    {
        return static_cast<ARMv7Assembler::Condition>(cond);
    }

    ARMv7Assembler::Condition armV7Condition(DoubleCondition cond)
    {
        return static_cast<ARMv7Assembler::Condition>(cond);
    }

    ALWAYS_INLINE CachedTempRegister& cachedDataTempRegister()
    {
        return m_cachedDataTempRegister;
    }

    ALWAYS_INLINE void invalidateCachedAddressTempRegister()
    {
        // This function is intended for when we are explicitly using
        // addressTempRegister (because the caller supplied it), so it can
        // ignore m_allowScratchRegister.
        m_cachedAddressTempRegister.invalidate();
    }

    ALWAYS_INLINE CachedTempRegister& cachedAddressTempRegister()
    {
        RELEASE_ASSERT(m_allowScratchRegister);
        return m_cachedAddressTempRegister;
    }

    ALWAYS_INLINE RegisterID getCachedDataTempRegisterIDAndInvalidate()
    {
        return cachedDataTempRegister().registerIDInvalidate();
    }

    ALWAYS_INLINE RegisterID getCachedAddressTempRegisterIDAndInvalidate()
    {
        return cachedAddressTempRegister().registerIDInvalidate();
    }
private:
    friend class LinkBuffer;

    template<PtrTag tag>
    static void linkCall(void* code, Call call, CodePtr<tag> function)
    {
        if (!call.isFlagSet(Call::Near))
            Assembler::linkPointer(code, call.m_label.labelAtOffset(-2), function.taggedPtr());
        else if (call.isFlagSet(Call::Tail))
            Assembler::linkTailCall(code, call.m_label, function.taggedPtr());
        else
            Assembler::linkCall(code, call.m_label, function.taggedPtr());
    }

    bool m_makeJumpPatchable { false };
    CachedTempRegister m_cachedDataTempRegister;
    CachedTempRegister m_cachedAddressTempRegister;
};

} // namespace JSC

#endif // ENABLE(ASSEMBLER)
