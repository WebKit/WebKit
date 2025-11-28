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

#include <JavaScriptCore/AssemblerCommon.h>
#include <wtf/Platform.h>

#if ENABLE(ASSEMBLER) && CPU(ARM_THUMB2)

#include <JavaScriptCore/ARMv7Assembler.h>
#include <JavaScriptCore/AbstractMacroAssembler.h>
#include <JavaScriptCore/SIMDInfo.h>
#include <initializer_list>
#include <optional>

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
        : m_makeJumpPatchable(false)
        , m_cachedDataTempRegister(this, dataTempRegister)
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
    static JumpLinkType computeJumpType(LinkRecord& record, const uint8_t* from, const uint8_t* to) { return ARMv7Assembler::computeJumpType(record, from, to); }
    static int jumpSizeDelta(JumpType jumpType, JumpLinkType jumpLinkType) { return ARMv7Assembler::jumpSizeDelta(jumpType, jumpLinkType); }

    template<RepatchingInfo repatch>
    ALWAYS_INLINE static void link(LinkRecord& record, uint8_t* from, const uint8_t* fromInstruction, uint8_t* to) { return ARMv7Assembler::link<repatch>(record, from, fromInstruction, to); }

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

    static constexpr ARMv7Assembler::Condition armV7ConditionForHigh32(RelationalCondition cond)
    {
        switch (cond) {
        case GreaterThan:
        case GreaterThanOrEqual:
            return ARMv7Assembler::ConditionGT;
        case LessThan:
        case LessThanOrEqual:
            return ARMv7Assembler::ConditionLT;
        case Above:
        case AboveOrEqual:
            return ARMv7Assembler::ConditionHI;
        case Below:
        case BelowOrEqual:
            return ARMv7Assembler::ConditionLO;
        case NotEqual:
            return ARMv7Assembler::ConditionNE;
        case Equal:
            // Equal can never be determined from high alone (needs both parts to match)
            return ARMv7Assembler::ConditionInvalid;
        default:
            RELEASE_ASSERT_NOT_REACHED();
            return ARMv7Assembler::ConditionInvalid;
        }
    }

    static constexpr ARMv7Assembler::Condition armV7ConditionForLow32(RelationalCondition cond)
    {
        switch (cond) {
        case GreaterThan:
        case Above:
            return ARMv7Assembler::ConditionHI;
        case GreaterThanOrEqual:
        case AboveOrEqual:
            return ARMv7Assembler::ConditionHS;
        case LessThan:
        case Below:
            return ARMv7Assembler::ConditionLO;
        case LessThanOrEqual:
        case BelowOrEqual:
            return ARMv7Assembler::ConditionLS;
        case NotEqual:
            return ARMv7Assembler::ConditionNE;
        case Equal:
            return ARMv7Assembler::ConditionEQ;
        default:
            RELEASE_ASSERT_NOT_REACHED();
            return ARMv7Assembler::ConditionInvalid;
        }
    }

    enum ResultCondition {
        Carry = ARMv7Assembler::ConditionCS,
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
    // may often be a memory location (explicitly described using an Address
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

    void add8(TrustedImm32 imm, Address address)
    {
        load8(address, dataTempRegister);
        add32(imm, dataTempRegister, dataTempRegister);
        store8(dataTempRegister, address);
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
        if (imm.m_value >= 0)
            m_assembler.adc(scratch, scratch, ARMThumbImmediate::makeEncodedImm(0));
        else
            m_assembler.sbc(scratch, scratch, ARMThumbImmediate::makeEncodedImm(0));
        m_assembler.str(scratch, addressTempRegister, ARMThumbImmediate::makeUInt12(4));
    }

    void add64(RegisterID op1Hi, RegisterID op1Lo, RegisterID op2Hi, RegisterID op2Lo, RegisterID destHi, RegisterID destLo)
    {
        if (destLo != op1Hi && destLo != op2Hi) {
            m_assembler.add_S(destLo, op1Lo, op2Lo);
            m_assembler.adc(destHi, op1Hi, op2Hi);
        } else {
            RegisterID scratch = getCachedDataTempRegisterIDAndInvalidate();
            m_assembler.add_S(scratch, op1Lo, op2Lo);
            m_assembler.adc(destHi, op1Hi, op2Hi);
            move(scratch, destLo);
        }
    }

    void and64(RegisterID op1Hi, RegisterID op1Lo, RegisterID op2Hi, RegisterID op2Lo, RegisterID destHi, RegisterID destLo)
    {
        if (destHi != op1Lo && destHi != op2Lo) {
            m_assembler.ARM_and(destHi, op1Hi, op2Hi);
            m_assembler.ARM_and(destLo, op1Lo, op2Lo);
        } else {
            RegisterID scratch = getCachedDataTempRegisterIDAndInvalidate();
            m_assembler.ARM_and(scratch, op1Hi, op2Hi);
            m_assembler.ARM_and(destLo, op1Lo, op2Lo);
            move(scratch, destHi);
        }
    }

    void mul64(RegisterID op1Hi, RegisterID op1Lo, RegisterID op2Hi, RegisterID op2Lo, RegisterID destHi, RegisterID destLo)
    {
        // Check if dest registers will clobber the high parts we need later
        if (destLo != op1Hi && destLo != op2Hi && destHi != op1Hi && destHi != op2Hi) {
            // No overlap - direct computation
            m_assembler.umull(destLo, destHi, op1Lo, op2Lo);
            m_assembler.mla(destHi, op1Hi, op2Lo, destHi);
            m_assembler.mla(destHi, op1Lo, op2Hi, destHi);
        } else {
            // Overlap exists - compute middle terms first into scratch, then add
            RegisterID scratch = getCachedDataTempRegisterIDAndInvalidate();
            m_assembler.mul(scratch, op1Hi, op2Lo);
            m_assembler.mla(scratch, op1Lo, op2Hi, scratch);
            m_assembler.umull(destLo, destHi, op1Lo, op2Lo);
            add32(scratch, destHi);
        }
    }

    void rotateLeft64(RegisterID srcHi, RegisterID srcLo, RegisterID shiftAmount, RegisterID destHi, RegisterID destLo, RegisterID scratch0, RegisterID scratch1)
    {
        // Test/swap bit 5
        // leftShift = amount & 31
        // rightShift = 32 - leftShift
        // resultLo = (srcLo << leftShift) | (srcHi >> rightShift)
        // resultHi = (srcHi << leftShift) | (srcLo >> rightShift)

        // Test if bit 5 is set
        m_assembler.tst(shiftAmount, ARMThumbImmediate::makeEncodedImm(32));
        m_assembler.mov(scratch0, srcLo);
        m_assembler.mov(scratch1, srcHi);

        // If bit 5 is set, swap them
        m_assembler.it(ARMv7Assembler::ConditionNE, true);
        m_assembler.mov(scratch0, srcHi);
        m_assembler.mov(scratch1, srcLo);

        // leftShift = shiftAmount & 31
        RegisterID dataTemp = getCachedDataTempRegisterIDAndInvalidate();
        m_assembler.ARM_and(dataTemp, shiftAmount, ARMThumbImmediate::makeEncodedImm(31));

        // rightShift = 32 - leftShift
        sub32(TrustedImm32(32), dataTemp, destLo);

        m_assembler.lsl(destHi, scratch0, dataTemp); // A = scratch0 << leftShift
        m_assembler.lsl(dataTemp, scratch1, dataTemp); // C = scratch1 << leftShift
        m_assembler.lsr(scratch1, scratch1, destLo); // B = scratch1 >> rightShift
        m_assembler.lsr(scratch0, scratch0, destLo); // D = scratch0 >> rightShift

        m_assembler.orr(destLo, destHi, scratch1); // resultLo = A | B
        m_assembler.orr(destHi, dataTemp, scratch0); // resultHi = C | D
    }

    void rotateLeft64(RegisterID srcHi, RegisterID srcLo, TrustedImm32 shiftAmount, RegisterID destHi, RegisterID destLo, RegisterID scratch0, RegisterID scratch1)
    {
        ASSERT(shiftAmount.m_value > 0 && shiftAmount.m_value <= 63);

        // Special case: rotation by 32 is just a swap
        if (shiftAmount.m_value == 32) {
            m_assembler.mov(destLo, srcHi);
            m_assembler.mov(destHi, srcLo);
            return;
        }

        int32_t leftShift = shiftAmount.m_value & 31;
        int32_t rightShift = 32 - leftShift;

        bool needSwap = shiftAmount.m_value & 32;
        if (needSwap) {
            m_assembler.mov(scratch0, srcHi);
            m_assembler.mov(scratch1, srcLo);
        } else {
            m_assembler.mov(scratch0, srcLo);
            m_assembler.mov(scratch1, srcHi);
        }

        RegisterID dataTemp = getCachedDataTempRegisterIDAndInvalidate();

        m_assembler.lsl(destHi, scratch0, leftShift); // A = scratch0 << leftShift
        m_assembler.lsl(dataTemp, scratch1, leftShift); // C = scratch1 << leftShift
        m_assembler.lsr(scratch1, scratch1, rightShift); // B = scratch1 >> rightShift
        m_assembler.lsr(scratch0, scratch0, rightShift); // D = scratch0 >> rightShift

        m_assembler.orr(destLo, destHi, scratch1); // resultLo = A | B
        m_assembler.orr(destHi, dataTemp, scratch0); // resultHi = C | D
    }

    void rotateRight64(RegisterID srcHi, RegisterID srcLo, RegisterID shiftAmount, RegisterID destHi, RegisterID destLo, RegisterID scratch0, RegisterID scratch1)
    {
        // Test/swap bit 5
        // rightShift = amount & 31
        // leftShift = 32 - rightShift
        // resultLo = (srcLo >> rightShift) | (srcHi << leftShift)
        // resultHi = (srcHi >> rightShift) | (srcLo << leftShift)

        // Test if bit 5 is set
        m_assembler.tst(shiftAmount, ARMThumbImmediate::makeEncodedImm(32));
        m_assembler.mov(scratch0, srcLo);
        m_assembler.mov(scratch1, srcHi);

        // If bit 5 is set, swap them
        m_assembler.it(ARMv7Assembler::ConditionNE, true);
        m_assembler.mov(scratch0, srcHi);
        m_assembler.mov(scratch1, srcLo);

        // rightShift = shiftAmount & 31
        RegisterID dataTemp = getCachedDataTempRegisterIDAndInvalidate();
        m_assembler.ARM_and(dataTemp, shiftAmount, ARMThumbImmediate::makeEncodedImm(31));

        // leftShift = 32 - rightShift
        sub32(TrustedImm32(32), dataTemp, destLo);

        m_assembler.lsr(destHi, scratch0, dataTemp); // A = scratch0 >> rightShift
        m_assembler.lsr(dataTemp, scratch1, dataTemp); // C = scratch1 >> rightShift
        m_assembler.lsl(scratch1, scratch1, destLo); // B = scratch1 << leftShift
        m_assembler.lsl(scratch0, scratch0, destLo); // D = scratch0 << leftShift

        m_assembler.orr(destLo, destHi, scratch1); // resultLo = A | B
        m_assembler.orr(destHi, dataTemp, scratch0); // resultHi = C | D
    }

    void rotateRight64(RegisterID srcHi, RegisterID srcLo, TrustedImm32 shiftAmount, RegisterID destHi, RegisterID destLo, RegisterID scratch0, RegisterID scratch1)
    {
        ASSERT(shiftAmount.m_value > 0 && shiftAmount.m_value <= 63);

        // Special case: rotation by 32 is just a swap
        if (shiftAmount.m_value == 32) {
            m_assembler.mov(destLo, srcHi);
            m_assembler.mov(destHi, srcLo);
            return;
        }

        int32_t rightShift = shiftAmount.m_value & 31;
        int32_t leftShift = 32 - rightShift;

        bool needSwap = shiftAmount.m_value & 32;
        if (needSwap) {
            m_assembler.mov(scratch0, srcHi);
            m_assembler.mov(scratch1, srcLo);
        } else {
            m_assembler.mov(scratch0, srcLo);
            m_assembler.mov(scratch1, srcHi);
        }

        RegisterID dataTemp = getCachedDataTempRegisterIDAndInvalidate();

        m_assembler.lsr(destHi, scratch0, rightShift); // A = scratch0 >> rightShift
        m_assembler.lsr(dataTemp, scratch1, rightShift); // C = scratch1 >> rightShift
        m_assembler.lsl(scratch1, scratch1, leftShift); // B = scratch1 << leftShift
        m_assembler.lsl(scratch0, scratch0, leftShift); // D = scratch0 << leftShift

        m_assembler.orr(destLo, destHi, scratch1); // resultLo = A | B
        m_assembler.orr(destHi, dataTemp, scratch0); // resultHi = C | D
    }

    void lshift64(RegisterID srcHi, RegisterID srcLo, RegisterID shiftAmount, RegisterID destHi, RegisterID destLo, RegisterID scratch0, RegisterID scratch1)
    {
        // shift = amount & 63
        // resultHi = (srcHi << shift) | (srcLo >> (32 - shift)) | (srcLo << (shift - 32))
        // resultLo = srcLo << shift

        RegisterID dataTemp = getCachedDataTempRegisterIDAndInvalidate();

        // shift = shiftAmount & 63
        m_assembler.ARM_and(dataTemp, shiftAmount, ARMThumbImmediate::makeEncodedImm(63));

        // 32 - shift
        sub32(TrustedImm32(32), dataTemp, scratch0);

        // resultHi = (srcHi << shift) | (srcLo >> (32 - shift))
        m_assembler.lsl(destHi, srcHi, dataTemp);
        m_assembler.lsr(scratch1, srcLo, scratch0);
        m_assembler.orr(destHi, destHi, scratch1);

        // shift - 32
        m_assembler.sub(scratch0, dataTemp, ARMThumbImmediate::makeEncodedImm(32));

        // resultHi |= (srcLo << (shift - 32))
        m_assembler.lsl(scratch1, srcLo, scratch0);
        m_assembler.orr(destHi, destHi, scratch1);

        // resultLo = srcLo << shift
        m_assembler.lsl(destLo, srcLo, dataTemp);
    }

    void lshift64(RegisterID srcHi, RegisterID srcLo, TrustedImm32 shiftAmount, RegisterID destHi, RegisterID destLo)
    {
        ASSERT(shiftAmount.m_value >= 0 && shiftAmount.m_value <= 63);

        int32_t shift = shiftAmount.m_value;
        if (!shift) {
            m_assembler.mov(destLo, srcLo);
            m_assembler.mov(destHi, srcHi);
            return;
        }

        if (shift < 32) {
            // resultHi = (srcHi << shift) | (srcLo >> (32 - shift))
            // resultLo = srcLo << shift
            RegisterID scratch = getCachedDataTempRegisterIDAndInvalidate();
            m_assembler.lsl(destHi, srcHi, shift);
            m_assembler.lsr(scratch, srcLo, 32 - shift);
            m_assembler.orr(destHi, destHi, scratch);
            m_assembler.lsl(destLo, srcLo, shift);
            return;
        }

        if (shift == 32) {
            // resultHi = srcLo
            // resultLo = 0
            m_assembler.mov(destHi, srcLo);
            m_assembler.mov(destLo, ARMThumbImmediate::makeEncodedImm(0));
            return;
        }

        // resultHi = srcLo << (shift - 32)
        // resultLo = 0
        m_assembler.lsl(destHi, srcLo, shift - 32);
        m_assembler.mov(destLo, ARMThumbImmediate::makeEncodedImm(0));
    }

    void urshift64(RegisterID srcHi, RegisterID srcLo, RegisterID shiftAmount, RegisterID destHi, RegisterID destLo, RegisterID scratch0, RegisterID scratch1)
    {
        // shift = amount & 63
        // resultLo = (srcLo >> shift) | (srcHi << (32 - shift)) | (srcHi >> (shift - 32))
        // resultHi = srcHi >> shift

        RegisterID dataTemp = getCachedDataTempRegisterIDAndInvalidate();

        // shift = shiftAmount & 63
        m_assembler.ARM_and(dataTemp, shiftAmount, ARMThumbImmediate::makeEncodedImm(63));

        // 32 - shift
        sub32(TrustedImm32(32), dataTemp, scratch0);

        // resultLo = (srcLo >> shift) | (srcHi << (32 - shift))
        m_assembler.lsr(destLo, srcLo, dataTemp);
        m_assembler.lsl(scratch1, srcHi, scratch0);
        m_assembler.orr(destLo, destLo, scratch1);

        // shift - 32
        m_assembler.sub(scratch0, dataTemp, ARMThumbImmediate::makeEncodedImm(32));

        // resultLo |= (srcHi >> (shift - 32))
        m_assembler.lsr(scratch1, srcHi, scratch0);
        m_assembler.orr(destLo, destLo, scratch1);

        // resultHi = srcHi >> shift
        m_assembler.lsr(destHi, srcHi, dataTemp);
    }

    void urshift64(RegisterID srcHi, RegisterID srcLo, TrustedImm32 shiftAmount, RegisterID destHi, RegisterID destLo)
    {
        ASSERT(shiftAmount.m_value >= 0 && shiftAmount.m_value <= 63);

        int32_t shift = shiftAmount.m_value;
        if (!shift) {
            m_assembler.mov(destLo, srcLo);
            m_assembler.mov(destHi, srcHi);
            return;
        }

        if (shift < 32) {
            // resultLo = (srcLo >> shift) | (srcHi << (32 - shift))
            // resultHi = srcHi >> shift
            RegisterID scratch = getCachedDataTempRegisterIDAndInvalidate();
            m_assembler.lsr(destLo, srcLo, shift);
            m_assembler.lsl(scratch, srcHi, 32 - shift);
            m_assembler.orr(destLo, destLo, scratch);
            m_assembler.lsr(destHi, srcHi, shift);
            return;
        }

        if (shift == 32) {
            // resultLo = srcHi
            // resultHi = 0
            m_assembler.mov(destLo, srcHi);
            m_assembler.mov(destHi, ARMThumbImmediate::makeEncodedImm(0));
            return;
        }

        // resultLo = srcHi >> (shift - 32)
        // resultHi = 0
        m_assembler.lsr(destLo, srcHi, shift - 32);
        m_assembler.mov(destHi, ARMThumbImmediate::makeEncodedImm(0));
    }

    void rshift64(RegisterID srcHi, RegisterID srcLo, RegisterID shiftAmount, RegisterID destHi, RegisterID destLo, RegisterID scratch0, RegisterID scratch1)
    {
        // shift = amount & 63
        // resultLo = (srcLo >> shift) | (srcHi << (32 - shift)) | (srcHi >> (shift - 32))
        // resultHi = srcHi >> shift (arithmetic)

        RegisterID dataTemp = getCachedDataTempRegisterIDAndInvalidate();

        // shift = shiftAmount & 63
        m_assembler.ARM_and(dataTemp, shiftAmount, ARMThumbImmediate::makeEncodedImm(63));

        // 32 - shift
        sub32(TrustedImm32(32), dataTemp, scratch0);

        // resultLo = (srcLo >> shift) | (srcHi << (32 - shift))
        m_assembler.lsr(destLo, srcLo, dataTemp);
        m_assembler.lsl(scratch1, srcHi, scratch0);
        m_assembler.orr(destLo, destLo, scratch1);

        // shift - 32
        m_assembler.sub(scratch0, dataTemp, ARMThumbImmediate::makeEncodedImm(32));

        // (srcHi >> (shift - 32)) for the shift >= 32 case
        m_assembler.asr(scratch1, srcHi, scratch0);

        // if (shift >= 32) use scratch1, else keep destLo
        m_assembler.orr(scratch1, destLo, scratch1);
        moveConditionally32(RelationalCondition::AboveOrEqual, dataTemp, TrustedImm32(32), scratch1, destLo, destLo);

        // resultHi = srcHi >> shift (arithmetic)
        m_assembler.asr(destHi, srcHi, dataTemp);
    }

    void rshift64(RegisterID srcHi, RegisterID srcLo, TrustedImm32 shiftAmount, RegisterID destHi, RegisterID destLo)
    {
        ASSERT(shiftAmount.m_value >= 0 && shiftAmount.m_value <= 63);

        int32_t shift = shiftAmount.m_value;
        if (!shift) {
            m_assembler.mov(destLo, srcLo);
            m_assembler.mov(destHi, srcHi);
            return;
        }

        if (shift < 32) {
            // resultLo = (srcLo >> shift) | (srcHi << (32 - shift))
            // resultHi = srcHi >> shift (arithmetic)
            RegisterID scratch = getCachedDataTempRegisterIDAndInvalidate();
            m_assembler.lsr(destLo, srcLo, shift);
            m_assembler.lsl(scratch, srcHi, 32 - shift);
            m_assembler.orr(destLo, destLo, scratch);
            m_assembler.asr(destHi, srcHi, shift);
            return;
        }

        if (shift == 32) {
            // resultLo = srcHi
            // resultHi = srcHi >> 31 (sign extend)
            m_assembler.mov(destLo, srcHi);
            m_assembler.asr(destHi, srcHi, 31);
            return;
        }

        // resultLo = srcHi >> (shift - 32) (arithmetic)
        // resultHi = srcHi >> 31 (sign extend)
        m_assembler.asr(destLo, srcHi, shift - 32);
        m_assembler.asr(destHi, srcHi, 31);
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
        if (imm.m_value == -1)
            return move(src, dest);

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

    void countTrailingZeros32(RegisterID src, RegisterID dest)
    {
        m_assembler.rbit(dest, src);
        m_assembler.clz(dest, dest);
    }

    void countLeadingZeros64(RegisterID srcHi, RegisterID srcLo, RegisterID destHi, RegisterID destLo)
    {
        if (destLo != srcLo) {
            m_assembler.clz(destLo, srcHi);
            m_assembler.cmp(destLo, ARMThumbImmediate::makeEncodedImm(32));
            Jump done = makeBranch(ARMv7Assembler::ConditionNE);
            m_assembler.clz(destLo, srcLo);
            m_assembler.add(destLo, destLo, ARMThumbImmediate::makeEncodedImm(32));
            done.link(this);
            xor32(destHi, destHi);
        } else {
            RegisterID scratch = getCachedDataTempRegisterIDAndInvalidate();
            move(srcLo, scratch);
            m_assembler.clz(destLo, srcHi);
            m_assembler.cmp(destLo, ARMThumbImmediate::makeEncodedImm(32));
            Jump done = makeBranch(ARMv7Assembler::ConditionNE);
            m_assembler.clz(destLo, scratch);
            m_assembler.add(destLo, destLo, ARMThumbImmediate::makeEncodedImm(32));
            done.link(this);
            xor32(destHi, destHi);
        }
    }

    void countTrailingZeros64(RegisterID srcHi, RegisterID srcLo, RegisterID destHi, RegisterID destLo)
    {
        if (destLo != srcHi) {
            countTrailingZeros32(srcLo, destLo);
            m_assembler.cmp(destLo, ARMThumbImmediate::makeEncodedImm(32));
            Jump done = makeBranch(ARMv7Assembler::ConditionNE);
            countTrailingZeros32(srcHi, destLo);
            m_assembler.add(destLo, destLo, ARMThumbImmediate::makeEncodedImm(32));
            done.link(this);
            xor32(destHi, destHi);
        } else {
            RegisterID scratch = getCachedDataTempRegisterIDAndInvalidate();
            move(srcHi, scratch);
            countTrailingZeros32(srcLo, destLo);
            m_assembler.cmp(destLo, ARMThumbImmediate::makeEncodedImm(32));
            Jump done = makeBranch(ARMv7Assembler::ConditionNE);
            countTrailingZeros32(scratch, destLo);
            m_assembler.add(destLo, destLo, ARMThumbImmediate::makeEncodedImm(32));
            done.link(this);
            xor32(destHi, destHi);
        }
    }

    void compare64(RelationalCondition cond, RegisterID lhsHi, RegisterID lhsLo, RegisterID rhsHi, RegisterID rhsLo, RegisterID dest)
    {
        if (cond == RelationalCondition::Equal || cond == RelationalCondition::NotEqual) {
            // For Equal/NotEqual, we can optimize to only set one value conditionally
            // NotEqual: default to 1, change to 0 only if both parts equal
            // Equal: default to 0, change to 1 only if both parts equal
            if (dest != lhsHi && dest != rhsHi && dest != lhsLo && dest != rhsLo) {
                m_assembler.mov(dest, ARMThumbImmediate::makeEncodedImm(cond == RelationalCondition::NotEqual ? 1 : 0));
                m_assembler.cmp(lhsHi, rhsHi);
                Jump done = makeBranch(ARMv7Assembler::ConditionNE);
                m_assembler.cmp(lhsLo, rhsLo);
                // Only need to set the "opposite" value when both parts match
                m_assembler.it(ARMv7Assembler::ConditionEQ);
                m_assembler.mov(dest, ARMThumbImmediate::makeEncodedImm(cond == RelationalCondition::NotEqual ? 0 : 1));
                done.link(this);
            } else {
                RegisterID scratch = getCachedDataTempRegisterIDAndInvalidate();
                m_assembler.mov(scratch, ARMThumbImmediate::makeEncodedImm(cond == RelationalCondition::NotEqual ? 1 : 0));
                m_assembler.cmp(lhsHi, rhsHi);
                Jump done = makeBranch(ARMv7Assembler::ConditionNE);
                m_assembler.cmp(lhsLo, rhsLo);
                m_assembler.it(ARMv7Assembler::ConditionEQ);
                m_assembler.mov(scratch, ARMThumbImmediate::makeEncodedImm(cond == RelationalCondition::NotEqual ? 0 : 1));
                done.link(this);
                move(scratch, dest);
            }
            return;
        }

        ARMv7Assembler::Condition hiCondition = armV7ConditionForHigh32(cond);
        ARMv7Assembler::Condition loCondition = armV7ConditionForLow32(cond);

        if (dest != lhsLo && dest != rhsLo && dest != lhsHi && dest != rhsHi) {
            // No aliasing - use ITE blocks with 1 branch
            m_assembler.cmp(lhsHi, rhsHi);
            m_assembler.it(hiCondition, false);
            m_assembler.mov(dest, ARMThumbImmediate::makeEncodedImm(1));
            m_assembler.mov(dest, ARMThumbImmediate::makeEncodedImm(0));

            Jump done = makeBranch(ARMv7Assembler::ConditionNE);

            m_assembler.cmp(lhsLo, rhsLo);
            m_assembler.it(loCondition, false);
            m_assembler.mov(dest, ARMThumbImmediate::makeEncodedImm(1));
            m_assembler.mov(dest, ARMThumbImmediate::makeEncodedImm(0));

            done.link(this);
        } else {
            // dest aliases with source - use scratch
            RegisterID scratch = getCachedDataTempRegisterIDAndInvalidate();

            m_assembler.cmp(lhsHi, rhsHi);
            m_assembler.it(hiCondition, false);
            m_assembler.mov(scratch, ARMThumbImmediate::makeEncodedImm(1));
            m_assembler.mov(scratch, ARMThumbImmediate::makeEncodedImm(0));

            Jump done = makeBranch(ARMv7Assembler::ConditionNE);

            m_assembler.cmp(lhsLo, rhsLo);
            m_assembler.it(loCondition, false);
            m_assembler.mov(scratch, ARMThumbImmediate::makeEncodedImm(1));
            m_assembler.mov(scratch, ARMThumbImmediate::makeEncodedImm(0));

            done.link(this);
            move(scratch, dest);
        }
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
        if (!(imm.m_value & 0x1f))
            move(src, dest);
        else
            m_assembler.lsl(dest, src, imm.m_value & 0x1f);
    }

    void lshift32(TrustedImm32 imm, RegisterID shiftAmount, RegisterID dest)
    {
        // Clamp the shift to the range 0..31
        m_assembler.ARM_and(dest, shiftAmount, ARMThumbImmediate::makeEncodedImm(0x1f));
        move(imm, getCachedDataTempRegisterIDAndInvalidate());
        m_assembler.lsl(dest, dataTempRegister, dest);
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

    void uMull32(RegisterID left, RegisterID right, RegisterID destHi, RegisterID destLo)
    {
        m_assembler.umull(destLo, destHi, left, right);
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

    void or16(RegisterID mask, AbsoluteAddress dest)
    {
        load16(setupArmAddress(dest), dataTempRegister);
        m_assembler.orr(dataTempRegister, dataTempRegister, mask);
        store16(dataTempRegister, Address(addressTempRegister));
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

    void or32(RegisterID src, Address dest)
    {
        load32(dest, dataTempRegister);
        or32(src, dataTempRegister);
        store32(dataTempRegister, dest);
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

    void or64(RegisterID op1Hi, RegisterID op1Lo, RegisterID op2Hi, RegisterID op2Lo, RegisterID destHi, RegisterID destLo)
    {
        if (destLo != op1Hi && destLo != op2Hi) {
            m_assembler.orr(destLo, op1Lo, op2Lo);
            m_assembler.orr(destHi, op1Hi, op2Hi);
        } else {
            RegisterID scratch = getCachedDataTempRegisterIDAndInvalidate();
            m_assembler.orr(scratch, op1Lo, op2Lo);
            m_assembler.orr(destHi, op1Hi, op2Hi);
            move(scratch, destLo);
        }
    }

    void rotateRight32(RegisterID op1, RegisterID op2, RegisterID dest)
    {
        m_assembler.ror(dest, op1, op2);
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

    void rotateLeft32(RegisterID src, RegisterID shift, RegisterID dest)
    {
        RegisterID scratch = getCachedDataTempRegisterIDAndInvalidate();
        m_assembler.ARM_and(scratch, shift, ARMThumbImmediate::makeEncodedImm(0x1f));
        m_assembler.sub(scratch, ARMThumbImmediate::makeUInt12(32), scratch);
        m_assembler.ror(dest, src, scratch);
    }

    void rotateLeft32(RegisterID src, TrustedImm32 shift, RegisterID dest)
    {
        RegisterID scratch = getCachedDataTempRegisterIDAndInvalidate();
        move(shift, scratch);
        m_assembler.ARM_and(scratch, scratch, ARMThumbImmediate::makeEncodedImm(0x1f));
        m_assembler.sub(scratch, ARMThumbImmediate::makeUInt12(32), scratch);
        m_assembler.ror(dest, src, scratch);
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
        if (!(imm.m_value & 0x1f))
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

    void rshift32(TrustedImm32 imm, RegisterID shiftAmount, RegisterID dest)
    {
        // Clamp the shift to the range 0..31
        m_assembler.ARM_and(dest, shiftAmount, ARMThumbImmediate::makeEncodedImm(0x1f));
        move(imm, getCachedDataTempRegisterIDAndInvalidate());
        m_assembler.asr(dest, dataTempRegister, dest);
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
        if (!(imm.m_value & 0x1f))
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

    void urshift32(TrustedImm32 imm, RegisterID shiftAmount, RegisterID dest)
    {
        // Clamp the shift to the range 0..31
        m_assembler.ARM_and(dest, shiftAmount, ARMThumbImmediate::makeEncodedImm(0x1f));
        move(imm, getCachedDataTempRegisterIDAndInvalidate());
        m_assembler.lsr(dest, dataTempRegister, dest);
    }

    void addUnsignedRightShift32(RegisterID src1, RegisterID src2, TrustedImm32 amount, RegisterID dest)
    {
        // dest = src1 + (src2 >> amount)
        urshift32(src2, amount, dataTempRegister);
        add32(src1, dataTempRegister, dest);
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

    void sub32(TrustedImm32 imm, RegisterID src, RegisterID dest)
    {
        ARMThumbImmediate armImm = ARMThumbImmediate::makeUInt12OrEncodedImm(imm.m_value);
        if (armImm.isValid())
            m_assembler.sub(dest, armImm, src);
        else {
            move(imm, dataTempRegister);
            m_assembler.sub(dest, dataTempRegister, src);
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

    void sub64(RegisterID leftHi, RegisterID leftLo, RegisterID rightHi, RegisterID rightLo, RegisterID destHi, RegisterID destLo)
    {
        if (destLo != leftHi && destLo != rightHi) {
            m_assembler.sub_S(destLo, leftLo, rightLo);
            m_assembler.sbc(destHi, leftHi, rightHi);
        } else {
            RegisterID scratch = getCachedDataTempRegisterIDAndInvalidate();
            m_assembler.sub_S(scratch, leftLo, rightLo);
            m_assembler.sbc(destHi, leftHi, rightHi);
            move(scratch, destLo);
        }
    }

    void xor64(RegisterID op1Hi, RegisterID op1Lo, RegisterID op2Hi, RegisterID op2Lo, RegisterID destHi, RegisterID destLo)
    {
        if (destHi != op1Lo && destHi != op2Lo) {
            m_assembler.eor(destHi, op1Hi, op2Hi);
            m_assembler.eor(destLo, op1Lo, op2Lo);
        } else {
            RegisterID scratch = getCachedDataTempRegisterIDAndInvalidate();
            m_assembler.eor(scratch, op1Hi, op2Hi);
            m_assembler.eor(destLo, op1Lo, op2Lo);
            move(scratch, destHi);
        }
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

    void depend32(RegisterID src, RegisterID dest)
    {
        xor32(src, src, dest);
    }

    void not32(RegisterID srcDest)
    {
        m_assembler.mvn(srcDest, srcDest);
    }

    void not32(RegisterID src, RegisterID dest)
    {
        m_assembler.mvn(dest, src);
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
        if (dest == addressTempRegister)
            invalidateCachedAddressTempRegister();
        else if (dest == dataTempRegister)
            cachedDataTempRegister().invalidate();

        if (address.type == ArmAddress::HasIndex)
            m_assembler.ldrsh(dest, address.base, address.u.index, address.u.scale);
        else if (address.u.offset >= 0) {
            ARMThumbImmediate armImm = ARMThumbImmediate::makeUInt12(address.u.offset);
            ASSERT(armImm.isValid());
            m_assembler.ldrsh(dest, address.base, armImm);
        } else {
            ASSERT(address.u.offset >= -255);
            m_assembler.ldrsh(dest, address.base, address.u.offset, true, false);
        }
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
        if (dest == addressTempRegister)
            invalidateCachedAddressTempRegister();
        else if (dest == dataTempRegister)
            cachedDataTempRegister().invalidate();

        if (address.type == ArmAddress::HasIndex)
            m_assembler.ldrsb(dest, address.base, address.u.index, address.u.scale);
        else if (address.u.offset >= 0) {
            ARMThumbImmediate armImm = ARMThumbImmediate::makeUInt12(address.u.offset);
            ASSERT(armImm.isValid());
            m_assembler.ldrsb(dest, address.base, armImm);
        } else {
            ASSERT(address.u.offset >= -255);
            m_assembler.ldrsb(dest, address.base, address.u.offset, true, false);
        }
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

    void loadAcq32(Address address, RegisterID dest)
    {
        load32(address, dest);
        loadFence();
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

    void loadAcq8(Address address, RegisterID dest)
    {
        load8(address, dest);
        loadFence();
    }

    void load8SignedExtendTo32(Address address, RegisterID dest)
    {
        load8SignedExtendTo32(setupArmAddress(address), dest);
    }

    void loadAcq8SignedExtendTo32(Address address, RegisterID dest)
    {
        load8SignedExtendTo32(address, dest);
        loadFence();
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

    void loadAcq16(Address address, RegisterID dest)
    {
        load16(address, dest);
        loadFence();
    }
    
    void load16SignedExtendTo32(Address address, RegisterID dest)
    {
        load16SignedExtendTo32(setupArmAddress(address), dest);
    }

    void loadAcq16SignedExtendTo32(Address address, RegisterID dest)
    {
        load16SignedExtendTo32(address, dest);
        loadFence();
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
            // Check if dest1 or dest2 aliases the base register to avoid UNPREDICTABLE ldrd behavior
            if (address.base == dest1) {
                // Load high word first to avoid clobbering base register
                ArmAddress highAddress(address.base, address.u.offset + 4);
                load32(highAddress, dest2);
                load32(address, dest1);
            } else if (address.base == dest2) {
                // Load low word first to avoid clobbering base register
                load32(address, dest1);
                ArmAddress highAddress(address.base, address.u.offset + 4);
                load32(highAddress, dest2);
            } else {
                int32_t absOffset = address.u.offset;
                if (absOffset < 0)
                    absOffset = -absOffset;
                if (!(absOffset & ~0x3fc)) {
                    if ((dest1 == addressTempRegister) || (dest2 == addressTempRegister))
                        invalidateCachedAddressTempRegister();
                    if ((dest1 == dataTempRegister) || (dest2 == dataTempRegister))
                        cachedDataTempRegister().invalidate();
                    m_assembler.ldrd(dest1, dest2, address.base, address.u.offset, /* index: */ true, /* wback: */ false);
                } else {
                    load32(address, dest1);
                    ArmAddress highAddress(address.base, address.u.offset + 4);
                    load32(highAddress, dest2);
                }
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

    void loadLink8(Address addr, RegisterID dest)
    {
        ASSERT(!addr.offset);
        m_assembler.ldrexb(dest, addr.base);
    }

    void loadLink16(Address addr, RegisterID dest)
    {
        ASSERT(!addr.offset);
        m_assembler.ldrexh(dest, addr.base);
    }

    void loadLink32(Address addr, RegisterID dest)
    {
        ASSERT(!addr.offset);
        m_assembler.ldrex(dest, addr.base, 0);
    }

    void loadLinkPair32(Address addr, RegisterID destLo, RegisterID destHi)
    {
        ASSERT(!addr.offset);
        m_assembler.ldrexd(destLo, destHi, addr.base);
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
        RELEASE_ASSERT(m_allowScratchRegister);
        move(imm, dataTempRegister);
        store32(dataTempRegister, address);
    }

    void storeRel32(RegisterID src, Address address)
    {
        storeFence();
        store32(src, address);
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

    void storeRel8(RegisterID src, Address address)
    {
        storeFence();
        store8(src, address);
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

    void store16(TrustedImm32 imm, Address address)
    {
        move(imm, dataTempRegister);
        store16(dataTempRegister, address);
    }

    void storeRel16(RegisterID src, Address address)
    {
        storeFence();
        store16(src, address);
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
        if (imm1.m_value == imm2.m_value) {
            RegisterID scratch = getCachedDataTempRegisterIDAndInvalidate();
            move(imm1, scratch);
            store32(scratch, address);
            store32(scratch, address.withOffset(4));
            return;
        }

        int32_t absOffset = address.offset;
        if (absOffset < 0)
            absOffset = -absOffset;
        store32(imm1, address);
        store32(imm2, address.withOffset(4));
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
        // strd does not support unaligned accesses on some chips, so we avoid it.
        store32(src1, address);
        store32(src2, address.withOffset(4));
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

    void transfer32(Address src, Address dest)
    {
        if (src == dest)
            return;
        load32(src, dataTempRegister);
        store32(dataTempRegister, dest);
    }

    // Warning: not atomic.
    void transfer64(Address src, Address dest)
    {
        if (src == dest)
            return;
        load32(src, dataTempRegister);
        store32(dataTempRegister, dest);
        load32(src.withOffset(sizeof(int)), dataTempRegister);
        store32(dataTempRegister, dest.withOffset(sizeof(int)));
    }

    void transferPtr(Address src, Address dest)
    {
        transfer32(src, dest);
    }

    void transfer32(BaseIndex src, BaseIndex dest)
    {
        load32(src, dataTempRegister);
        store32(dataTempRegister, dest);
    }

    void transferPtr(BaseIndex src, BaseIndex dest)
    {
        transfer32(src, dest);
    }

    void transferFloat(Address src, Address dest)
    {
        transfer32(src, dest);
    }

    void transferDouble(Address src, Address dest)
    {
        if (src == dest)
            return;
        loadDouble(src, fpTempRegister);
        storeDouble(fpTempRegister, dest);
    }

    template<typename SrcType, typename DestType>
    void transferVector(SrcType src, DestType dest)
    {
        if constexpr (std::equality_comparable_with<SrcType, DestType>) {
            if (src == dest)
                return;
        }

        loadVector(src, fpTempRegister);
        storeVector(fpTempRegister, dest);
    }

    void transferFloat(BaseIndex src, BaseIndex dest)
    {
        transfer32(src, dest);
    }

    void transferDouble(BaseIndex src, BaseIndex dest)
    {
        if (src == dest)
            return;
        loadDouble(src, fpTempRegister);
        storeDouble(fpTempRegister, dest);
    }

    void storeCond8(RegisterID src, Address addr, RegisterID result)
    {
        ASSERT(!addr.offset);
        m_assembler.strexb(result, src, addr.base);
    }

    void storeCond16(RegisterID src, Address addr, RegisterID result)
    {
        ASSERT(!addr.offset);
        m_assembler.strexh(result, src, addr.base);
    }

    void storeCond32(RegisterID src, Address addr, RegisterID result)
    {
        ASSERT(!addr.offset);
        m_assembler.strex(result, src, addr.base, 0);
    }

    void storeCondPair32(RegisterID srcLo, RegisterID srcHi, Address addr, RegisterID result)
    {
        ASSERT(!addr.offset);
        m_assembler.strexd(result, srcLo, srcHi, addr.base);
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

    void move32ToFloat(RegisterID src, FPRegisterID dest)
    {
        m_assembler.vmov(asSingle(dest), src);
    }

    void move32ToFloat(RegisterID src, ARMRegisters::FPSingleRegisterID dest)
    {
        m_assembler.vmov(dest, src);
    }

    void moveFloat(ARMRegisters::FPSingleRegisterID src, ARMRegisters::FPSingleRegisterID dest)
    {
        if (src != dest)
            m_assembler.vmov(dest, src);
    }

    void moveFloatTo32(FPRegisterID src, RegisterID dest)
    {
        m_assembler.vmov(dest, asSingle(src));
    }

    void move64ToDouble(RegisterID srcHi, RegisterID srcLo, FPRegisterID dest)
    {
        m_assembler.vmov(dest, srcLo, srcHi);
    }

    void moveDoubleTo64(FPRegisterID src, RegisterID destHi, RegisterID destLo)
    {
        m_assembler.vmov(destLo, destHi, src);
    }

    void move32ToDoubleHi(RegisterID src, FPRegisterID dest)
    {
        m_assembler.vmov(asSingleUpper(dest), src);
    }

    void moveDoubleHiTo32(FPRegisterID src, RegisterID dest)
    {
        m_assembler.vmov(dest, asSingleUpper(src));
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

    // Popcount (could be implemented via VCNT?)

    static bool supportsCountPopulation() { return false; }

    NO_RETURN_DUE_TO_CRASH void countPopulation32(RegisterID, RegisterID)
    {
        ASSERT(!supportsCountPopulation());
        CRASH();
    }

    NO_RETURN_DUE_TO_CRASH void countPopulation32(RegisterID, RegisterID, FPRegisterID)
    {
        ASSERT(!supportsCountPopulation());
        CRASH();
    }

    NO_RETURN_DUE_TO_CRASH void countPopulation64(RegisterID, RegisterID)
    {
        ASSERT(!supportsCountPopulation());
        CRASH();
    }

    NO_RETURN_DUE_TO_CRASH void countPopulation64(RegisterID, RegisterID, FPRegisterID)
    {
        ASSERT(!supportsCountPopulation());
        CRASH();
    }

    // Floating-point operations:

    static bool supportsFloatingPoint() { return true; }
    static bool supportsFloatingPointTruncate() { return true; }
    static bool supportsFloatingPointSqrt() { return true; }
    static bool supportsFloatingPointAbs() { return true; }
    static bool supportsFloatingPointRounding() { return true; }
    static bool supportsFloat16() { return false; }

    void loadDouble(Address address, FPRegisterID dest)
    {
        // Use two GPR loads for unaligned access
        // We only have dataTempRegister safe to use since load32 may use addressTempRegister
        RegisterID scratch = getCachedDataTempRegisterIDAndInvalidate();
        auto sd0 = ARMRegisters::asSingle(dest);
        auto sd1 = ARMRegisters::asSingleUpper(dest);
        load32(address, scratch);
        m_assembler.vmov(sd0, scratch);
        load32(Address(address.base, address.offset + 4), scratch);
        m_assembler.vmov(sd1, scratch);
    }

    void loadFloat(Address address, FPRegisterID dest)
    {
        // Use GPR load for unaligned access (flds requires 4-byte alignment)
        RegisterID scratch = getCachedDataTempRegisterIDAndInvalidate();
        load32(address, scratch);
        move32ToFloat(scratch, dest);
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

    void move32ToFloat(TrustedImm32 imm, FPRegisterID dest)
    {
        if (!imm.m_value) {
            moveZeroToFloat(dest);
            return;
        }
        RegisterID scratch = getCachedDataTempRegisterIDAndInvalidate();
        move(imm, scratch);
        move32ToFloat(scratch, dest);
    }

    void move32ToFloat(TrustedImm32 imm, ARMRegisters::FPSingleRegisterID dest)
    {
        if (!imm.m_value) {
            moveZeroToFloat(dest);
            return;
        }
        RegisterID scratch = getCachedDataTempRegisterIDAndInvalidate();
        move(imm, scratch);
        move32ToFloat(scratch, dest);
    }

    void move64ToDouble(TrustedImm64 imm, FPRegisterID dest)
    {
        if (!imm.m_value) {
            moveZeroToDouble(dest);
            return;
        }
        RegisterID scratch1 = getCachedDataTempRegisterIDAndInvalidate();
        RegisterID scratch2 = getCachedAddressTempRegisterIDAndInvalidate();
        move(TrustedImm32(static_cast<uint32_t>(static_cast<uint64_t>(imm.m_value))), scratch1);
        move(TrustedImm32(static_cast<uint32_t>(static_cast<uint64_t>(imm.m_value) >> 32)), scratch2);
        move64ToDouble(scratch2, scratch1, dest);
    }

    void moveZeroToFloat(FPRegisterID reg)
    {
        static double zeroConstant = 0.;
        loadFloat(TrustedImmPtr(&zeroConstant), reg);
    }

    void moveZeroToFloat(ARMRegisters::FPSingleRegisterID reg)
    {
        RegisterID scratch = getCachedDataTempRegisterIDAndInvalidate();
        move(TrustedImm32(0), scratch);
        m_assembler.vmov(reg, scratch);
    }

    void loadFloat(TrustedImmPtr address, FPRegisterID dest)
    {
        move(address, addressTempRegister);
        m_assembler.flds(ARMRegisters::asSingle(dest), addressTempRegister, 0);
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
        // Use two GPR stores for unaligned access (vstr requires 4-byte alignment)
        // We only have dataTempRegister safe to use since store32 may use addressTempRegister
        RegisterID scratch = getCachedDataTempRegisterIDAndInvalidate();
        auto sd0 = ARMRegisters::asSingle(src);
        auto sd1 = ARMRegisters::asSingleUpper(src);
        m_assembler.vmov(scratch, sd0);
        store32(scratch, address);
        m_assembler.vmov(scratch, sd1);
        store32(scratch, Address(address.base, address.offset + 4));
    }

    void storeFloat(FPRegisterID src, Address address)
    {
        // Use GPR store for unaligned access (fsts requires 4-byte alignment)
        RegisterID scratch = getCachedDataTempRegisterIDAndInvalidate();
        moveFloatTo32(src, scratch);
        store32(scratch, address);
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

    void addFloat(FPRegisterID op1, FPRegisterID op2, FPRegisterID dest)
    {
        m_assembler.vadd(asSingle(dest), asSingle(op1), asSingle(op2));
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

    void divFloat(FPRegisterID op1, FPRegisterID op2, FPRegisterID dest)
    {
        m_assembler.vdiv(asSingle(dest), asSingle(op1), asSingle(op2));
    }

    void divDouble(FPRegisterID src, FPRegisterID dest)
    {
        m_assembler.vdiv(dest, dest, src);
    }

    void divDouble(FPRegisterID op1, FPRegisterID op2, FPRegisterID dest)
    {
        m_assembler.vdiv(dest, op1, op2);
    }

    void subFloat(FPRegisterID op1, FPRegisterID op2, FPRegisterID dest)
    {
        m_assembler.vsub(asSingle(dest), asSingle(op1), asSingle(op2));
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

    void mulFloat(FPRegisterID op1, FPRegisterID op2, FPRegisterID dest)
    {
        m_assembler.vmul(asSingle(dest), asSingle(op1), asSingle(op2));
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

    void andFloat(FPRegisterID op1, FPRegisterID op2, FPRegisterID dest)
    {
        m_assembler.vand(dest, op1, op2);
    }

    void andFloat(ARMRegisters::FPSingleRegisterID op1, ARMRegisters::FPSingleRegisterID op2, ARMRegisters::FPSingleRegisterID dest)
    {
        RegisterID temp1 = getCachedDataTempRegisterIDAndInvalidate();
        RegisterID temp2 = getCachedAddressTempRegisterIDAndInvalidate();
        m_assembler.vmov(temp1, op1);
        m_assembler.vmov(temp2, op2);
        m_assembler.ARM_and(temp1, temp1, temp2);
        m_assembler.vmov(dest, temp1);
    }

    void andDouble(FPRegisterID op1, FPRegisterID op2, FPRegisterID dest)
    {
        m_assembler.vand(dest, op1, op2);
    }

    void orFloat(FPRegisterID op1, FPRegisterID op2, FPRegisterID dest)
    {
        m_assembler.vorr(dest, op1, op2);
    }

    void orFloat(ARMRegisters::FPSingleRegisterID op1, ARMRegisters::FPSingleRegisterID op2, ARMRegisters::FPSingleRegisterID dest)
    {
        RegisterID temp1 = getCachedDataTempRegisterIDAndInvalidate();
        RegisterID temp2 = getCachedAddressTempRegisterIDAndInvalidate();
        m_assembler.vmov(temp1, op1);
        m_assembler.vmov(temp2, op2);
        m_assembler.orr(temp1, temp1, temp2);
        m_assembler.vmov(dest, temp1);
    }

    void orDouble(FPRegisterID op1, FPRegisterID op2, FPRegisterID dest)
    {
        m_assembler.vorr(dest, op1, op2);
    }

    void sqrtFloat(FPRegisterID src, FPRegisterID dest)
    {
        m_assembler.vsqrt(asSingle(dest), asSingle(src));
    }

    void sqrtDouble(FPRegisterID src, FPRegisterID dest)
    {
        m_assembler.vsqrt(dest, src);
    }

    void absFloat(FPRegisterID src, FPRegisterID dest)
    {
        m_assembler.vabs(asSingle(dest), asSingle(src));
    }

    void absDouble(FPRegisterID src, FPRegisterID dest)
    {
        m_assembler.vabs(dest, src);
    }

    void negateFloat(FPRegisterID src, FPRegisterID dest)
    {
        m_assembler.vneg(asSingle(dest), asSingle(src));
    }

    void negateDouble(FPRegisterID src, FPRegisterID dest)
    {
        m_assembler.vneg(dest, src);
    }

    void ceilFloat(ARMRegisters::FPSingleRegisterID src, ARMRegisters::FPSingleRegisterID dest)
    {
        // ARMv7 doesn't have vrintp, use trunc then add 1 if x > trunc(x)
        auto scratchS0 = fpTempRegisterAsSingle();
        auto scratchS1 = static_cast<ARMRegisters::FPSingleRegisterID>(fpTempRegisterAsSingle() + 1);

        // Check NaN: x != x
        Jump isNaN = branchFloat(DoubleNotEqualOrUnordered, src, src);

        // Check for large positive: src >= 2^31
        move32ToFloat(TrustedImm32(0x4F000000), scratchS0);
        Jump isLargePos = branchFloat(DoubleGreaterThanOrEqualOrUnordered, src, scratchS0);

        // Check for large negative: src <= -2^31
        move32ToFloat(TrustedImm32(0xCF000000), scratchS0);
        Jump isLargeNeg = branchFloat(DoubleLessThanOrEqualOrUnordered, src, scratchS0);

        // Save original for later comparison
        moveFloat(src, scratchS1);

        // Compute trunc(x)
        m_assembler.vcvt_floatingPointToSigned(scratchS0, src);
        m_assembler.vcvt_signedToFloatingPoint(dest, scratchS0);

        // If x > trunc(x), add 1.0
        Jump notGreater = branchFloat(DoubleLessThanOrEqualOrUnordered, scratchS1, dest);
        move32ToFloat(TrustedImm32(0x3F800000), scratchS0); // 1.0f
        m_assembler.vadd(dest, dest, scratchS0);
        notGreater.link(this);

        // Preserve sign from input (handle -0.0 case)
        {
            RegisterID gprScratch1 = getCachedDataTempRegisterIDAndInvalidate();
            RegisterID gprScratch2 = getCachedAddressTempRegisterIDAndInvalidate();
            m_assembler.vmov(gprScratch1, dest);
            m_assembler.vmov(gprScratch2, scratchS1);
            and32(TrustedImm32(0x80000000), gprScratch2);
            and32(TrustedImm32(0x7FFFFFFF), gprScratch1);
            or32(gprScratch2, gprScratch1);
            m_assembler.vmov(dest, gprScratch1);
        }
        Jump done = jump();

        // NaN: quieten signaling NaNs
        isNaN.link(this);
        m_assembler.vadd(dest, src, src);
        Jump doneFromNaN = jump();

        // Large values: preserve original
        isLargePos.link(this);
        isLargeNeg.link(this);
        moveFloat(src, dest);

        doneFromNaN.link(this);
        done.link(this);
    }

    void ceilFloat(FPRegisterID src, FPRegisterID dest)
    {
        ceilFloat(asSingle(src), asSingle(dest));
    }

    void floorFloat(ARMRegisters::FPSingleRegisterID src, ARMRegisters::FPSingleRegisterID dest)
    {
        // ARMv7 doesn't have vrintm, use trunc then subtract 1 if x < trunc(x)
        auto scratchS0 = fpTempRegisterAsSingle();
        auto scratchS1 = static_cast<ARMRegisters::FPSingleRegisterID>(fpTempRegisterAsSingle() + 1);

        // Check NaN: x != x
        Jump isNaN = branchFloat(DoubleNotEqualOrUnordered, src, src);

        // Check for large positive: src >= 2^31
        move32ToFloat(TrustedImm32(0x4F000000), scratchS0);
        Jump isLargePos = branchFloat(DoubleGreaterThanOrEqualOrUnordered, src, scratchS0);

        // Check for large negative: src <= -2^31
        move32ToFloat(TrustedImm32(0xCF000000), scratchS0);
        Jump isLargeNeg = branchFloat(DoubleLessThanOrEqualOrUnordered, src, scratchS0);

        // Save original to scratchS1 for later comparison and sign preservation
        moveFloat(src, scratchS1);

        // Compute trunc(x)
        m_assembler.vcvt_floatingPointToSigned(scratchS0, src);
        m_assembler.vcvt_signedToFloatingPoint(dest, scratchS0);

        // If x < trunc(x), subtract 1.0
        Jump notLess = branchFloat(DoubleGreaterThanOrEqualOrUnordered, scratchS1, dest);
        move32ToFloat(TrustedImm32(0x3F800000), scratchS0); // 1.0f
        m_assembler.vsub(dest, dest, scratchS0);
        notLess.link(this);

        // Preserve sign from input
        {
            RegisterID gprScratch1 = getCachedDataTempRegisterIDAndInvalidate();
            RegisterID gprScratch2 = getCachedAddressTempRegisterIDAndInvalidate();
            m_assembler.vmov(gprScratch1, dest);
            m_assembler.vmov(gprScratch2, scratchS1);
            and32(TrustedImm32(0x80000000), gprScratch2);
            and32(TrustedImm32(0x7FFFFFFF), gprScratch1);
            or32(gprScratch2, gprScratch1);
            m_assembler.vmov(dest, gprScratch1);
        }
        Jump done = jump();

        // NaN: quieten signaling NaNs
        isNaN.link(this);
        m_assembler.vadd(dest, src, src);
        Jump doneFromNaN = jump();

        // Large values: preserve original
        isLargePos.link(this);
        isLargeNeg.link(this);
        moveFloat(src, dest);

        doneFromNaN.link(this);
        done.link(this);
    }

    void floorFloat(FPRegisterID src, FPRegisterID dest)
    {
        floorFloat(asSingle(src), asSingle(dest));
    }

    void truncFloat(ARMRegisters::FPSingleRegisterID src, ARMRegisters::FPSingleRegisterID dest)
    {
        // ARMv7 doesn't have vrintz, use vcvt to int (truncates) and back
        auto scratchS0 = fpTempRegisterAsSingle();
        auto scratchS1 = static_cast<ARMRegisters::FPSingleRegisterID>(fpTempRegisterAsSingle() + 1);

        // Check NaN: x != x
        Jump isNaN = branchFloat(DoubleNotEqualOrUnordered, src, src);

        // Check for large positive: src >= 2^31
        move32ToFloat(TrustedImm32(0x4F000000), scratchS0);
        Jump isLargePos = branchFloat(DoubleGreaterThanOrEqualOrUnordered, src, scratchS0);

        // Check for large negative: src <= -2^31
        move32ToFloat(TrustedImm32(0xCF000000), scratchS0);
        Jump isLargeNeg = branchFloat(DoubleLessThanOrEqualOrUnordered, src, scratchS0);

        // Save original sign
        moveFloat(src, scratchS1);

        // Compute trunc(x)
        m_assembler.vcvt_floatingPointToSigned(scratchS0, src);
        m_assembler.vcvt_signedToFloatingPoint(dest, scratchS0);

        // Preserve sign from input (fixes trunc(-0) = -0)
        {
            RegisterID gprScratch1 = getCachedDataTempRegisterIDAndInvalidate();
            RegisterID gprScratch2 = getCachedAddressTempRegisterIDAndInvalidate();
            m_assembler.vmov(gprScratch1, dest);
            m_assembler.vmov(gprScratch2, scratchS1);
            and32(TrustedImm32(0x80000000), gprScratch2);
            and32(TrustedImm32(0x7FFFFFFF), gprScratch1);
            or32(gprScratch2, gprScratch1);
            m_assembler.vmov(dest, gprScratch1);
        }
        Jump done = jump();

        // NaN: quieten signaling NaNs
        isNaN.link(this);
        m_assembler.vadd(dest, src, src);
        Jump doneFromNaN = jump();

        // Large values: preserve original
        isLargePos.link(this);
        isLargeNeg.link(this);
        moveFloat(src, dest);

        doneFromNaN.link(this);
        done.link(this);
    }

    void truncFloat(FPRegisterID src, FPRegisterID dest)
    {
        truncFloat(asSingle(src), asSingle(dest));
    }

    void roundTowardNearestIntFloat(ARMRegisters::FPSingleRegisterID src, ARMRegisters::FPSingleRegisterID dest)
    {
        auto scratchS0 = fpTempRegisterAsSingle();
        auto scratchS1 = static_cast<ARMRegisters::FPSingleRegisterID>(fpTempRegisterAsSingle() + 1);

        // Check NaN: x != x
        Jump isNaN = branchFloat(DoubleNotEqualOrUnordered, src, src);

        // Check for large positive: src >= 2^31
        move32ToFloat(TrustedImm32(0x4F000000), scratchS0);
        Jump isLargePos = branchFloat(DoubleGreaterThanOrEqualOrUnordered, src, scratchS0);

        // Check for large negative: src <= -2^31
        move32ToFloat(TrustedImm32(0xCF000000), scratchS0);
        Jump isLargeNeg = branchFloat(DoubleLessThanOrEqualOrUnordered, src, scratchS0);

        // Save original sign for later
        moveFloat(src, scratchS1);

        // Use vcvt with round-to-nearest
        m_assembler.vcvt_floatingPointToSignedNearest(scratchS0, src);
        m_assembler.vcvt_signedToFloatingPoint(dest, scratchS0);

        // Preserve sign from input (fixes nearest(-0.4) = -0 case)
        {
            RegisterID gprScratch1 = getCachedDataTempRegisterIDAndInvalidate();
            RegisterID gprScratch2 = getCachedAddressTempRegisterIDAndInvalidate();
            m_assembler.vmov(gprScratch1, dest);
            m_assembler.vmov(gprScratch2, scratchS1);
            and32(TrustedImm32(0x80000000), gprScratch2);
            and32(TrustedImm32(0x7FFFFFFF), gprScratch1);
            or32(gprScratch2, gprScratch1);
            m_assembler.vmov(dest, gprScratch1);
        }
        Jump done = jump();

        // NaN: quieten signaling NaNs
        isNaN.link(this);
        m_assembler.vadd(dest, src, src);
        Jump doneFromNaN = jump();

        // Large values: preserve original
        isLargePos.link(this);
        isLargeNeg.link(this);
        moveFloat(src, dest);

        doneFromNaN.link(this);
        done.link(this);
    }

    void roundTowardNearestIntFloat(FPRegisterID src, FPRegisterID dest)
    {
        roundTowardNearestIntFloat(asSingle(src), asSingle(dest));
    }

    void roundTowardZeroFloat(FPRegisterID src, FPRegisterID dest)
    {
        truncFloat(src, dest);
    }

    void ceilDouble(FPRegisterID src, FPRegisterID dest)
    {
        // ARMv7 doesn't have vrintp, use trunc then add 1 if x > trunc(x)
        // Use fpScratchRegister (d14) to save original, fpTempRegister (d15) for constants
        auto fpScratchRegister = static_cast<FPRegisterID>(fpTempRegister - 1);
        auto fpScratchRegisterHi = static_cast<ARMRegisters::FPSingleRegisterID>(fpScratchRegister * 2 + 1);

        // Check for NaN
        Jump isNaN = branchDouble(DoubleNotEqualOrUnordered, src, src);

        // Check for large positive: src >= 2^31
        move64ToDouble(TrustedImm64(0x41E0000000000000), fpTempRegister);
        Jump isLargePos = branchDouble(DoubleGreaterThanOrEqualOrUnordered, src, fpTempRegister);

        // Check for large negative: src <= -2^31
        move64ToDouble(TrustedImm64(0xC1E0000000000000), fpTempRegister);
        Jump isLargeNeg = branchDouble(DoubleLessThanOrEqualOrUnordered, src, fpTempRegister);

        // Save original to fpScratchRegister for later comparison and sign preservation
        moveDouble(src, fpScratchRegister);

        // Compute trunc(x)
        m_assembler.vcvt_floatingPointToSigned(fpTempRegisterAsSingle(), src);
        m_assembler.vcvt_signedToFloatingPoint(dest, fpTempRegisterAsSingle());

        // If x > trunc(x), add 1.0
        Jump notGreater = branchDouble(DoubleLessThanOrEqualOrUnordered, fpScratchRegister, dest);
        move64ToDouble(TrustedImm64(0x3FF0000000000000), fpTempRegister);
        m_assembler.vadd(dest, dest, fpTempRegister);
        notGreater.link(this);

        // Preserve sign from input (read from fpScratchRegister which holds original value)
        {
            RegisterID signBit = getCachedDataTempRegisterIDAndInvalidate();
            RegisterID resultHi = getCachedAddressTempRegisterIDAndInvalidate();
            m_assembler.vmov(resultHi, static_cast<ARMRegisters::FPSingleRegisterID>(dest * 2 + 1));
            m_assembler.vmov(signBit, fpScratchRegisterHi);
            and32(TrustedImm32(0x80000000), signBit);
            and32(TrustedImm32(0x7FFFFFFF), resultHi);
            or32(signBit, resultHi);
            m_assembler.vmov(static_cast<ARMRegisters::FPSingleRegisterID>(dest * 2 + 1), resultHi);
        }
        Jump done = jump();

        // NaN: quieten signaling NaNs
        isNaN.link(this);
        m_assembler.vadd(dest, src, src);
        Jump doneFromNaN = jump();

        // Large values: preserve original
        isLargePos.link(this);
        isLargeNeg.link(this);
        moveDouble(src, dest);

        doneFromNaN.link(this);
        done.link(this);
    }

    void floorDouble(FPRegisterID src, FPRegisterID dest)
    {
        // ARMv7 doesn't have vrintm, use trunc then subtract 1 if x < trunc(x)
        // Use fpScratchRegister (d14) to save original, fpTempRegister (d15) for constants
        auto fpScratchRegister = static_cast<FPRegisterID>(fpTempRegister - 1);
        auto fpScratchRegisterHi = static_cast<ARMRegisters::FPSingleRegisterID>(fpScratchRegister * 2 + 1);

        // Check for NaN
        Jump isNaN = branchDouble(DoubleNotEqualOrUnordered, src, src);

        // Check for large positive: src >= 2^31
        move64ToDouble(TrustedImm64(0x41E0000000000000), fpTempRegister);
        Jump isLargePos = branchDouble(DoubleGreaterThanOrEqualOrUnordered, src, fpTempRegister);

        // Check for large negative: src <= -2^31
        move64ToDouble(TrustedImm64(0xC1E0000000000000), fpTempRegister);
        Jump isLargeNeg = branchDouble(DoubleLessThanOrEqualOrUnordered, src, fpTempRegister);

        // Save original to fpScratchRegister for later comparison and sign preservation
        moveDouble(src, fpScratchRegister);

        // Compute trunc(x)
        m_assembler.vcvt_floatingPointToSigned(fpTempRegisterAsSingle(), src);
        m_assembler.vcvt_signedToFloatingPoint(dest, fpTempRegisterAsSingle());

        // If x < trunc(x), subtract 1.0
        Jump notLess = branchDouble(DoubleGreaterThanOrEqualOrUnordered, fpScratchRegister, dest);
        move64ToDouble(TrustedImm64(0x3FF0000000000000), fpTempRegister);
        m_assembler.vsub(dest, dest, fpTempRegister);
        notLess.link(this);

        // Preserve sign from input (read from fpScratchRegister which holds original value)
        {
            RegisterID signBit = getCachedDataTempRegisterIDAndInvalidate();
            RegisterID resultHi = getCachedAddressTempRegisterIDAndInvalidate();
            m_assembler.vmov(resultHi, static_cast<ARMRegisters::FPSingleRegisterID>(dest * 2 + 1));
            m_assembler.vmov(signBit, fpScratchRegisterHi);
            and32(TrustedImm32(0x80000000), signBit);
            and32(TrustedImm32(0x7FFFFFFF), resultHi);
            or32(signBit, resultHi);
            m_assembler.vmov(static_cast<ARMRegisters::FPSingleRegisterID>(dest * 2 + 1), resultHi);
        }
        Jump done = jump();

        // NaN: quieten signaling NaNs
        isNaN.link(this);
        m_assembler.vadd(dest, src, src);
        Jump doneFromNaN = jump();

        // Large values: preserve original
        isLargePos.link(this);
        isLargeNeg.link(this);
        moveDouble(src, dest);

        doneFromNaN.link(this);
        done.link(this);
    }

    void truncDouble(FPRegisterID src, FPRegisterID dest)
    {
        // ARMv7 doesn't have vrintz, use vcvt to int (truncates) and back
        // Check for NaN
        Jump isNaN = branchDouble(DoubleNotEqualOrUnordered, src, src);

        // Check for large positive: src >= 2^31
        move64ToDouble(TrustedImm64(0x41E0000000000000), fpTempRegister);
        Jump isLargePos = branchDouble(DoubleGreaterThanOrEqualOrUnordered, src, fpTempRegister);

        // Check for large negative: src <= -2^31
        move64ToDouble(TrustedImm64(0xC1E0000000000000), fpTempRegister);
        Jump isLargeNeg = branchDouble(DoubleLessThanOrEqualOrUnordered, src, fpTempRegister);

        // Save original sign
        RegisterID gprScratchHi = getCachedDataTempRegisterIDAndInvalidate();
        m_assembler.vmov(gprScratchHi, static_cast<ARMRegisters::FPSingleRegisterID>(src * 2 + 1));

        // Compute trunc(x)
        m_assembler.vcvt_floatingPointToSigned(fpTempRegisterAsSingle(), src);
        m_assembler.vcvt_signedToFloatingPoint(dest, fpTempRegisterAsSingle());

        // Preserve sign from input
        {
            RegisterID resultHi = getCachedAddressTempRegisterIDAndInvalidate();
            m_assembler.vmov(resultHi, static_cast<ARMRegisters::FPSingleRegisterID>(dest * 2 + 1));
            and32(TrustedImm32(0x80000000), gprScratchHi);
            and32(TrustedImm32(0x7FFFFFFF), resultHi);
            or32(gprScratchHi, resultHi);
            m_assembler.vmov(static_cast<ARMRegisters::FPSingleRegisterID>(dest * 2 + 1), resultHi);
        }
        Jump done = jump();

        // NaN: quieten signaling NaNs
        isNaN.link(this);
        m_assembler.vadd(dest, src, src);
        Jump doneFromNaN = jump();

        // Large values: preserve original
        isLargePos.link(this);
        isLargeNeg.link(this);
        moveDouble(src, dest);

        doneFromNaN.link(this);
        done.link(this);
    }

    void roundTowardZeroDouble(FPRegisterID src, FPRegisterID dest)
    {
        truncDouble(src, dest);
    }

    void roundTowardNearestIntDouble(FPRegisterID src, FPRegisterID dest)
    {
        // Check for NaN
        Jump isNaN = branchDouble(DoubleNotEqualOrUnordered, src, src);

        // Check for large positive: src >= 2^31
        move64ToDouble(TrustedImm64(0x41E0000000000000), fpTempRegister);
        Jump isLargePos = branchDouble(DoubleGreaterThanOrEqualOrUnordered, src, fpTempRegister);

        // Check for large negative: src <= -2^31
        move64ToDouble(TrustedImm64(0xC1E0000000000000), fpTempRegister);
        Jump isLargeNeg = branchDouble(DoubleLessThanOrEqualOrUnordered, src, fpTempRegister);

        // Save original sign
        RegisterID gprScratchHi = getCachedDataTempRegisterIDAndInvalidate();
        m_assembler.vmov(gprScratchHi, static_cast<ARMRegisters::FPSingleRegisterID>(src * 2 + 1));

        // Use vcvt with round-to-nearest
        m_assembler.vcvt_floatingPointToSignedNearest(fpTempRegisterAsSingle(), src);
        m_assembler.vcvt_signedToFloatingPoint(dest, fpTempRegisterAsSingle());

        // Preserve sign from input (fixes nearest(-0.4) = -0 case)
        {
            RegisterID resultHi = getCachedAddressTempRegisterIDAndInvalidate();
            m_assembler.vmov(resultHi, static_cast<ARMRegisters::FPSingleRegisterID>(dest * 2 + 1));
            and32(TrustedImm32(0x80000000), gprScratchHi);
            and32(TrustedImm32(0x7FFFFFFF), resultHi);
            or32(gprScratchHi, resultHi);
            m_assembler.vmov(static_cast<ARMRegisters::FPSingleRegisterID>(dest * 2 + 1), resultHi);
        }
        Jump done = jump();

        // NaN: quieten signaling NaNs
        isNaN.link(this);
        m_assembler.vadd(dest, src, src);
        Jump doneFromNaN = jump();

        // Large values: preserve original
        isLargePos.link(this);
        isLargeNeg.link(this);
        moveDouble(src, dest);

        doneFromNaN.link(this);
        done.link(this);
    }

    void convertInt32ToFloat(RegisterID src, FPRegisterID dest)
    {
        m_assembler.vmov(fpTempRegister, src, src);
        m_assembler.vcvt_signedToFloatingPoint(dest, fpTempRegisterAsSingle(), /* toDouble: */ false);
    }

    void convertInt32ToDouble(TrustedImm32 imm, FPRegisterID dest)
    {
        move(imm, dataTempRegister);
        convertInt32ToDouble(dataTempRegister, dest);
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

    void convertUInt32ToFloat(RegisterID src, FPRegisterID dest)
    {
        m_assembler.vmov(fpTempRegister, src, src);
        m_assembler.vcvt_unsignedToFloatingPoint(dest, fpTempRegisterAsSingle(), /* toDouble: */ false);
    }

    void convertUInt32ToDouble(RegisterID src, FPRegisterID dest)
    {
        m_assembler.vmov(fpTempRegister, src, src);
        m_assembler.vcvt_unsignedToFloatingPoint(dest, fpTempRegisterAsSingle());
    }

    void convertFloatToDouble(FPRegisterID src, FPRegisterID dst)
    {
        m_assembler.vcvtds(dst, ARMRegisters::asSingle(src));
    }

    void convertDoubleToFloat(FPRegisterID src, FPRegisterID dst)
    {
        m_assembler.vcvtsd(ARMRegisters::asSingle(dst), src);
    }

    /* Wide SIMD operations
     *
     * These _are_ available, as an extension to armv7, but are currently
     * unimplemented. These stubs are provided instead (since they are
     * referenced directly by the JIT; these are not available via Air)
     */

private:
    template<typename Operation>
    void forEachDRegister(FPRegisterID reg, Operation op)
    {
        op(reg);
        op(static_cast<FPRegisterID>(reg + 1));
    }

    template<typename Operation>
    void forEachDRegister(FPRegisterID dest, FPRegisterID src, Operation op)
    {
        op(dest, src);
        op(static_cast<FPRegisterID>(dest + 1), static_cast<FPRegisterID>(src + 1));
    }

    template<typename Operation>
    void forEachDRegister(FPRegisterID dest, FPRegisterID left, FPRegisterID right, Operation op)
    {
        op(dest, left, right);
        op(static_cast<FPRegisterID>(dest + 1), static_cast<FPRegisterID>(left + 1), static_cast<FPRegisterID>(right + 1));
    }

public:
    void storeVector(FPRegisterID src, Address address)
    {
        RegisterID scratch = getCachedDataTempRegisterIDAndInvalidate();
        add32(TrustedImm32(address.offset), address.base, scratch);
        m_assembler.vst1_128(src, scratch);
    }

    void storeVector(FPRegisterID src, TrustedImmPtr address)
    {
        RegisterID scratch = getCachedDataTempRegisterIDAndInvalidate();
        move(TrustedImmPtr(address), scratch);
        m_assembler.vst1_128(src, scratch);
    }

    void storeVector(FPRegisterID src, BaseIndex address)
    {
        RegisterID scratch = getCachedDataTempRegisterIDAndInvalidate();
        m_assembler.add(scratch, address.base, address.index, ShiftTypeAndAmount(SRType_LSL, address.scale));
        if (address.offset)
            add32(TrustedImm32(address.offset), scratch);
        m_assembler.vst1_128(src, scratch);
    }

    void loadVector(Address address, FPRegisterID dest)
    {
        RegisterID scratch = getCachedDataTempRegisterIDAndInvalidate();
        add32(TrustedImm32(address.offset), address.base, scratch);
        m_assembler.vld1_128(dest, scratch);
    }

    void loadVector(BaseIndex address, FPRegisterID dest)
    {
        RegisterID scratch = getCachedDataTempRegisterIDAndInvalidate();
        m_assembler.add(scratch, address.base, address.index, ShiftTypeAndAmount(SRType_LSL, address.scale));
        if (address.offset)
            add32(TrustedImm32(address.offset), scratch);
        m_assembler.vld1_128(dest, scratch);
    }

    void loadVector(TrustedImmPtr address, FPRegisterID dest)
    {
        RegisterID scratch = getCachedDataTempRegisterIDAndInvalidate();
        move(TrustedImmPtr(address), scratch);
        m_assembler.vld1_128(dest, scratch);
    }

    void moveVector(FPRegisterID src, FPRegisterID dest)
    {
        if (src != dest)
            m_assembler.vmovVector(dest, src);
    }

    void moveZeroToVector(FPRegisterID dest)
    {
        forEachDRegister(dest, [&](FPRegisterID d) { m_assembler.veor(d, d, d); });
    }

    void vectorBitwiseSelect(FPRegisterID left, FPRegisterID right, FPRegisterID inputBitsAndDest)
    {
        forEachDRegister(inputBitsAndDest, left, right, [&](FPRegisterID d, FPRegisterID l, FPRegisterID r) {
            m_assembler.vbsl(d, l, r);
        });
    }

    void vectorUshl(SIMDInfo simdInfo, FPRegisterID input, FPRegisterID shift, FPRegisterID dest)
    {
        ASSERT(scalarTypeIsIntegral(simdInfo.lane));
        switch (simdInfo.lane) {
        case SIMDLane::i8x16:
            forEachDRegister(dest, input, shift, [&](FPRegisterID d, FPRegisterID i, FPRegisterID s) { m_assembler.vshl_u8(d, i, s); });
            break;
        case SIMDLane::i16x8:
            forEachDRegister(dest, input, shift, [&](FPRegisterID d, FPRegisterID i, FPRegisterID s) { m_assembler.vshl_u16(d, i, s); });
            break;
        case SIMDLane::i32x4:
            forEachDRegister(dest, input, shift, [&](FPRegisterID d, FPRegisterID i, FPRegisterID s) { m_assembler.vshl_u32(d, i, s); });
            break;
        case SIMDLane::i64x2:
            forEachDRegister(dest, input, shift, [&](FPRegisterID d, FPRegisterID i, FPRegisterID s) { m_assembler.vshl_u64(d, i, s); });
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
    }

    void vectorSshl(SIMDInfo simdInfo, FPRegisterID input, FPRegisterID shift, FPRegisterID dest)
    {
        ASSERT(scalarTypeIsIntegral(simdInfo.lane));
        switch (simdInfo.lane) {
        case SIMDLane::i8x16:
            forEachDRegister(dest, input, shift, [&](FPRegisterID d, FPRegisterID i, FPRegisterID s) { m_assembler.vshl_s8(d, i, s); });
            break;
        case SIMDLane::i16x8:
            forEachDRegister(dest, input, shift, [&](FPRegisterID d, FPRegisterID i, FPRegisterID s) { m_assembler.vshl_s16(d, i, s); });
            break;
        case SIMDLane::i32x4:
            forEachDRegister(dest, input, shift, [&](FPRegisterID d, FPRegisterID i, FPRegisterID s) { m_assembler.vshl_s32(d, i, s); });
            break;
        case SIMDLane::i64x2:
            forEachDRegister(dest, input, shift, [&](FPRegisterID d, FPRegisterID i, FPRegisterID s) { m_assembler.vshl_s64(d, i, s); });
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
    }

    void vectorMulLow(SIMDInfo simdInfo, FPRegisterID left, FPRegisterID right, FPRegisterID dest, FPRegisterID)
    {
        ASSERT(!scalarTypeIsFloatingPoint(simdInfo.lane));
        ASSERT(simdInfo.signMode != SIMDSignMode::None);

        SIMDLane inputLane = narrowedLane(simdInfo.lane);
        switch (inputLane) {
        case SIMDLane::i8x16:
            if (simdInfo.signMode == SIMDSignMode::Signed)
                m_assembler.vmull_s8(dest, left, right);
            else
                m_assembler.vmull_u8(dest, left, right);
            break;
        case SIMDLane::i16x8:
            if (simdInfo.signMode == SIMDSignMode::Signed)
                m_assembler.vmull_s16(dest, left, right);
            else
                m_assembler.vmull_u16(dest, left, right);
            break;
        case SIMDLane::i32x4:
            if (simdInfo.signMode == SIMDSignMode::Signed)
                m_assembler.vmull_s32(dest, left, right);
            else
                m_assembler.vmull_u32(dest, left, right);
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
    }

    void vectorMulHigh(SIMDInfo simdInfo, FPRegisterID left, FPRegisterID right, FPRegisterID dest, FPRegisterID)
    {
        ASSERT(!scalarTypeIsFloatingPoint(simdInfo.lane));
        ASSERT(simdInfo.signMode != SIMDSignMode::None);

        SIMDLane inputLane = narrowedLane(simdInfo.lane);
        FPRegisterID leftHigh = static_cast<FPRegisterID>(left + 1);
        FPRegisterID rightHigh = static_cast<FPRegisterID>(right + 1);

        switch (inputLane) {
        case SIMDLane::i8x16:
            if (simdInfo.signMode == SIMDSignMode::Signed)
                m_assembler.vmull_s8(dest, leftHigh, rightHigh);
            else
                m_assembler.vmull_u8(dest, leftHigh, rightHigh);
            break;
        case SIMDLane::i16x8:
            if (simdInfo.signMode == SIMDSignMode::Signed)
                m_assembler.vmull_s16(dest, leftHigh, rightHigh);
            else
                m_assembler.vmull_u16(dest, leftHigh, rightHigh);
            break;
        case SIMDLane::i32x4:
            if (simdInfo.signMode == SIMDSignMode::Signed)
                m_assembler.vmull_s32(dest, leftHigh, rightHigh);
            else
                m_assembler.vmull_u32(dest, leftHigh, rightHigh);
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
    }

    void vectorLoad8Splat(Address address, FPRegisterID dest, FPRegisterID)
    {
        RegisterID scratch = getCachedDataTempRegisterIDAndInvalidate();
        load8(address, scratch);
        vectorSplatInt8(scratch, dest);
    }

    void vectorLoad16Splat(Address address, FPRegisterID dest)
    {
        RegisterID scratch = getCachedDataTempRegisterIDAndInvalidate();
        load16(address, scratch);
        vectorSplatInt16(scratch, dest);
    }

    void vectorLoad32Splat(Address address, FPRegisterID dest)
    {
        RegisterID scratch = getCachedDataTempRegisterIDAndInvalidate();
        load32(address, scratch);
        vectorSplatInt32(scratch, dest);
    }

    void vectorLoad64Splat(Address address, FPRegisterID dest)
    {
        loadDouble(address, dest);
        // Copy dest to dest+1 to splat the 64-bit value across the full V128
        m_assembler.vorr(static_cast<FPRegisterID>(dest + 1), dest, dest);
    }

    void vectorLoad8Lane(Address address, TrustedImm32 laneIndex, FPRegisterID dest)
    {
        uint8_t index = laneIndex.m_value;
        RegisterID scratch = getCachedDataTempRegisterIDAndInvalidate();
        load8(address, scratch);
        FPRegisterID dReg = static_cast<FPRegisterID>(dest + (index >> 3));
        m_assembler.vmovToVectorElement8(dReg, scratch, index & 0x7);
    }

    void vectorLoad16Lane(Address address, TrustedImm32 laneIndex, FPRegisterID dest)
    {
        uint8_t index = laneIndex.m_value;
        RegisterID scratch = getCachedDataTempRegisterIDAndInvalidate();
        load16(address, scratch);
        FPRegisterID dReg = static_cast<FPRegisterID>(dest + (index >> 2));
        m_assembler.vmovToVectorElement16(dReg, scratch, index & 0x3);
    }

    void vectorLoad32Lane(Address address, TrustedImm32 laneIndex, FPRegisterID dest)
    {
        uint8_t index = laneIndex.m_value;
        RegisterID scratch = getCachedDataTempRegisterIDAndInvalidate();
        load32(address, scratch);
        FPRegisterID dReg = static_cast<FPRegisterID>(dest + (index >> 1));
        m_assembler.vmovToVectorElement32(dReg, scratch, index & 0x1);
    }

    void vectorLoad64Lane(Address address, TrustedImm32 laneIndex, FPRegisterID dest)
    {
        uint8_t index = laneIndex.m_value;
        FPRegisterID dReg = static_cast<FPRegisterID>(dest + index);
        loadDouble(address, dReg);
    }

    void vectorStore8Lane(FPRegisterID src, Address address, TrustedImm32 laneIndex)
    {
        uint8_t index = laneIndex.m_value;
        RegisterID scratch = getCachedDataTempRegisterIDAndInvalidate();
        FPRegisterID dReg = static_cast<FPRegisterID>(src + (index >> 3));
        m_assembler.vmovFromVectorElement8(scratch, dReg, index & 0x7);
        store8(scratch, address);
    }

    void vectorStore16Lane(FPRegisterID src, Address address, TrustedImm32 laneIndex)
    {
        uint8_t index = laneIndex.m_value;
        RegisterID scratch = getCachedDataTempRegisterIDAndInvalidate();
        FPRegisterID dReg = static_cast<FPRegisterID>(src + (index >> 2));
        m_assembler.vmovFromVectorElement16(scratch, dReg, index & 0x3);
        store16(scratch, address);
    }

    void vectorStore32Lane(FPRegisterID src, Address address, TrustedImm32 laneIndex)
    {
        uint8_t index = laneIndex.m_value;
        RegisterID scratch = getCachedDataTempRegisterIDAndInvalidate();
        FPRegisterID dReg = static_cast<FPRegisterID>(src + (index >> 1));
        m_assembler.vmovFromVectorElement32(scratch, dReg, index & 0x1);
        store32(scratch, address);
    }

    void vectorStore64Lane(FPRegisterID src, Address address, TrustedImm32 laneIndex)
    {
        uint8_t index = laneIndex.m_value;
        FPRegisterID dReg = static_cast<FPRegisterID>(src + index);
        storeDouble(dReg, address);
    }

    void vectorExtendLow(SIMDInfo simdInfo, FPRegisterID input, FPRegisterID dest)
    {
        ASSERT(simdInfo.signMode != SIMDSignMode::None);
        ASSERT(elementByteSize(simdInfo.lane) <= 8 && elementByteSize(simdInfo.lane) >= 2);

        SIMDLane narrowLane = narrowedLane(simdInfo.lane);

        switch (narrowLane) {
        case SIMDLane::i8x16:
            if (simdInfo.signMode == SIMDSignMode::Signed)
                m_assembler.vmovl_s8(dest, input);
            else
                m_assembler.vmovl_u8(dest, input);
            break;
        case SIMDLane::i16x8:
            if (simdInfo.signMode == SIMDSignMode::Signed)
                m_assembler.vmovl_s16(dest, input);
            else
                m_assembler.vmovl_u16(dest, input);
            break;
        case SIMDLane::i32x4:
            if (simdInfo.signMode == SIMDSignMode::Signed)
                m_assembler.vmovl_s32(dest, input);
            else
                m_assembler.vmovl_u32(dest, input);
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
    }

    void vectorExtendHigh(SIMDInfo simdInfo, FPRegisterID input, FPRegisterID dest)
    {
        ASSERT(simdInfo.signMode != SIMDSignMode::None);
        ASSERT(elementByteSize(simdInfo.lane) <= 8 && elementByteSize(simdInfo.lane) >= 2);

        FPRegisterID inputHigh = static_cast<FPRegisterID>(input + 1);
        SIMDLane narrowLane = narrowedLane(simdInfo.lane);

        switch (narrowLane) {
        case SIMDLane::i8x16:
            if (simdInfo.signMode == SIMDSignMode::Signed)
                m_assembler.vmovl_s8(dest, inputHigh);
            else
                m_assembler.vmovl_u8(dest, inputHigh);
            break;
        case SIMDLane::i16x8:
            if (simdInfo.signMode == SIMDSignMode::Signed)
                m_assembler.vmovl_s16(dest, inputHigh);
            else
                m_assembler.vmovl_u16(dest, inputHigh);
            break;
        case SIMDLane::i32x4:
            if (simdInfo.signMode == SIMDSignMode::Signed)
                m_assembler.vmovl_s32(dest, inputHigh);
            else
                m_assembler.vmovl_u32(dest, inputHigh);
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
    }

    void vectorUnsignedMax(SIMDInfo simdInfo, FPRegisterID vec, FPRegisterID dest)
    {
        switch (simdInfo.lane) {
        case SIMDLane::i8x16:
            // Reduce 16 lanes (in 2 D regs) to 1 using vpmax
            m_assembler.vpmax_u8(dest, vec, static_cast<FPRegisterID>(vec + 1)); // 16 -> 8 lanes in dest
            m_assembler.vpmax_u8(dest, dest, dest); // 8 -> 4 lanes
            m_assembler.vpmax_u8(dest, dest, dest); // 4 -> 2 lanes
            m_assembler.vpmax_u8(dest, dest, dest); // 2 -> 1 lane (result in first byte)
            break;
        case SIMDLane::i16x8:
            // Reduce 8 lanes (in 2 D regs) to 1
            m_assembler.vpmax_u16(dest, vec, static_cast<FPRegisterID>(vec + 1)); // 8 -> 4 lanes
            m_assembler.vpmax_u16(dest, dest, dest); // 4 -> 2 lanes
            m_assembler.vpmax_u16(dest, dest, dest); // 2 -> 1 lane (result in first 16 bits)
            break;
        case SIMDLane::i32x4:
            // Reduce 4 lanes (in 2 D regs) to 1
            m_assembler.vpmax_u32(dest, vec, static_cast<FPRegisterID>(vec + 1)); // 4 -> 2 lanes
            m_assembler.vpmax_u32(dest, dest, dest); // 2 -> 1 lane (result in first 32 bits)
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
    }

    void vectorUnsignedMin(SIMDInfo simdInfo, FPRegisterID vec, FPRegisterID dest)
    {
        switch (simdInfo.lane) {
        case SIMDLane::i8x16:
            // Reduce 16 lanes (in 2 D regs) to 1 using vpmin
            m_assembler.vpmin_u8(dest, vec, static_cast<FPRegisterID>(vec + 1)); // 16 -> 8 lanes in dest
            m_assembler.vpmin_u8(dest, dest, dest); // 8 -> 4 lanes
            m_assembler.vpmin_u8(dest, dest, dest); // 4 -> 2 lanes
            m_assembler.vpmin_u8(dest, dest, dest); // 2 -> 1 lane (result in first byte)
            break;
        case SIMDLane::i16x8:
            // Reduce 8 lanes (in 2 D regs) to 1
            m_assembler.vpmin_u16(dest, vec, static_cast<FPRegisterID>(vec + 1)); // 8 -> 4 lanes
            m_assembler.vpmin_u16(dest, dest, dest); // 4 -> 2 lanes
            m_assembler.vpmin_u16(dest, dest, dest); // 2 -> 1 lane (result in first 16 bits)
            break;
        case SIMDLane::i32x4:
            // Reduce 4 lanes (in 2 D regs) to 1
            m_assembler.vpmin_u32(dest, vec, static_cast<FPRegisterID>(vec + 1)); // 4 -> 2 lanes
            m_assembler.vpmin_u32(dest, dest, dest); // 2 -> 1 lane (result in first 32 bits)
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
    }

    void compareIntegerVectorWithZero(RelationalCondition cond, SIMDInfo simdInfo, FPRegisterID vector, FPRegisterID dest)
    {
        RELEASE_ASSERT(scalarTypeIsIntegral(simdInfo.lane));
        FPRegisterID zeroVec = ARMRegisters::d12;
        moveZeroToVector(zeroVec);
        compareIntegerVector(cond, simdInfo, vector, zeroVec, dest, zeroVec);
    }

    void vectorBitmask(SIMDInfo simdInfo, FPRegisterID vec, RegisterID dest, FPRegisterID)
    {
        // FIXME: We should do something like:
        // https://developer.arm.com/community/arm-community-blogs/b/servers-and-cloud-computing-blog/posts/porting-x86-vector-bitmask-optimizations-to-arm-neon

        move(TrustedImm32(0), dest);
        uint32_t numLanes = elementCount(simdInfo.lane);
        uint32_t elementSize = elementByteSize(simdInfo.lane);
        uint32_t signBitPosition = elementSize * 8 - 1;
        uint32_t signBitMask = 1u << signBitPosition;

        RegisterID temp1 = getCachedDataTempRegisterIDAndInvalidate();
        RegisterID temp2 = getCachedAddressTempRegisterIDAndInvalidate();

        for (uint32_t i = 0; i < numLanes; i++) {
            FPRegisterID dReg;
            uint8_t laneInDReg;

            switch (simdInfo.lane) {
            case SIMDLane::i8x16:
                dReg = static_cast<FPRegisterID>(vec + (i >> 3));
                laneInDReg = i & 0x7;
                m_assembler.vmovFromVectorElementS8(temp1, dReg, laneInDReg);
                break;
            case SIMDLane::i16x8:
                dReg = static_cast<FPRegisterID>(vec + (i >> 2));
                laneInDReg = i & 0x3;
                m_assembler.vmovFromVectorElementS16(temp1, dReg, laneInDReg);
                break;
            case SIMDLane::i32x4:
                dReg = static_cast<FPRegisterID>(vec + (i >> 1));
                laneInDReg = i & 0x1;
                m_assembler.vmovFromVectorElement32(temp1, dReg, laneInDReg);
                break;
            case SIMDLane::i64x2:
                dReg = static_cast<FPRegisterID>(vec + i);
                m_assembler.vmovFromVectorElement32(temp1, dReg, 1);
                break;
            default:
                RELEASE_ASSERT_NOT_REACHED();
            }

            // vmovFromVectorElementS8/S16 sign-extends, so mask to get just the sign bit
            // For i8x16 and i16x8, we need to mask before shifting
            // For i32x4 and i64x2 (which extract the high 32 bits), we can shift directly
            if (elementSize < 4) {
                and32(TrustedImm32(signBitMask), temp1, temp2);
                urshift32(temp2, TrustedImm32(signBitPosition), temp2);
            } else {
                // For i32x4 and i64x2, just shift right to get bit 31 at position 0
                urshift32(temp1, TrustedImm32(31), temp2);
            }

            lshift32(TrustedImm32(i), temp2);
            or32(temp2, dest);
        }
    }

    void vectorPromote(SIMDInfo simdInfo, FPRegisterID input, FPRegisterID dest)
    {
        ASSERT_UNUSED(simdInfo, simdInfo.lane == SIMDLane::f32x4);
        auto inputS0 = ARMRegisters::asSingle(input);
        auto inputS1 = ARMRegisters::asSingleUpper(input);
        auto destD0 = dest;
        auto destD1 = static_cast<FPRegisterID>(dest + 1);
        m_assembler.vcvt_f64_f32(destD1, inputS1);
        m_assembler.vcvt_f64_f32(destD0, inputS0);
    }

    void vectorDemote(SIMDInfo simdInfo, FPRegisterID input, FPRegisterID dest)
    {
        ASSERT_UNUSED(simdInfo, simdInfo.lane == SIMDLane::f64x2);
        auto destS0 = ARMRegisters::asSingle(dest);
        auto destS1 = ARMRegisters::asSingleUpper(dest);
        auto destD1 = static_cast<FPRegisterID>(dest + 1);
        m_assembler.vcvt_f32_f64(destS0, input);
        m_assembler.vcvt_f32_f64(destS1, static_cast<FPRegisterID>(input + 1));
        // Zero the upper 64 bits
        m_assembler.veor(destD1, destD1, destD1);
    }

    void vectorAbs(SIMDInfo simdInfo, FPRegisterID input, FPRegisterID dest)
    {
        auto emitI64Abs = [&](FPRegisterID d, FPRegisterID src) {
            RegisterID scratchLow = getCachedDataTempRegisterIDAndInvalidate();
            RegisterID scratchHigh = getCachedAddressTempRegisterIDAndInvalidate();

            moveDoubleToInts(src, scratchLow, scratchHigh);
            auto positive = branch32(GreaterThanOrEqual, scratchHigh, TrustedImm32(0));
            m_assembler.sub_S(scratchLow, ARMThumbImmediate::makeUInt12(0), scratchLow);
            m_assembler.mvn(scratchHigh, scratchHigh);
            m_assembler.adc(scratchHigh, scratchHigh, ARMThumbImmediate::makeEncodedImm(0));
            positive.link(this);
            moveIntsToDouble(scratchLow, scratchHigh, d);
        };

        switch (simdInfo.lane) {
        case SIMDLane::i8x16:
            forEachDRegister(dest, input, [&](FPRegisterID d, FPRegisterID s) { m_assembler.vabs_i8(d, s); });
            break;
        case SIMDLane::i16x8:
            forEachDRegister(dest, input, [&](FPRegisterID d, FPRegisterID s) { m_assembler.vabs_i16(d, s); });
            break;
        case SIMDLane::i32x4:
            forEachDRegister(dest, input, [&](FPRegisterID d, FPRegisterID s) { m_assembler.vabs_i32(d, s); });
            break;
        case SIMDLane::i64x2:
            forEachDRegister(dest, input, emitI64Abs);
            break;
        case SIMDLane::f32x4:
            forEachDRegister(dest, input, [&](FPRegisterID d, FPRegisterID s) { m_assembler.vabs_f32(d, s); });
            break;
        case SIMDLane::f64x2:
            forEachDRegister(dest, input, [&](FPRegisterID d, FPRegisterID s) { m_assembler.vabs(d, s); });
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
    }

    void vectorNeg(SIMDInfo simdInfo, FPRegisterID input, FPRegisterID dest)
    {
        auto emitI64Neg = [&](FPRegisterID d, FPRegisterID src) {
            RegisterID scratchLow = getCachedDataTempRegisterIDAndInvalidate();
            RegisterID scratchHigh = getCachedAddressTempRegisterIDAndInvalidate();

            moveDoubleToInts(src, scratchLow, scratchHigh);
            m_assembler.sub_S(scratchLow, ARMThumbImmediate::makeUInt12(0), scratchLow);
            m_assembler.mvn(scratchHigh, scratchHigh);
            m_assembler.adc(scratchHigh, scratchHigh, ARMThumbImmediate::makeEncodedImm(0));
            moveIntsToDouble(scratchLow, scratchHigh, d);
        };

        switch (simdInfo.lane) {
        case SIMDLane::i8x16:
            forEachDRegister(dest, input, [&](FPRegisterID d, FPRegisterID s) { m_assembler.vneg_i8(d, s); });
            break;
        case SIMDLane::i16x8:
            forEachDRegister(dest, input, [&](FPRegisterID d, FPRegisterID s) { m_assembler.vneg_i16(d, s); });
            break;
        case SIMDLane::i32x4:
            forEachDRegister(dest, input, [&](FPRegisterID d, FPRegisterID s) { m_assembler.vneg_i32(d, s); });
            break;
        case SIMDLane::i64x2:
            forEachDRegister(dest, input, emitI64Neg);
            break;
        case SIMDLane::f32x4:
            forEachDRegister(dest, input, [&](FPRegisterID d, FPRegisterID s) { m_assembler.vneg_f32(d, s); });
            break;
        case SIMDLane::f64x2:
            forEachDRegister(dest, input, [&](FPRegisterID d, FPRegisterID s) { m_assembler.vneg(d, s); });
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
    }

    void vectorPopcnt(SIMDInfo simdInfo, FPRegisterID input, FPRegisterID dest)
    {
        ASSERT_UNUSED(simdInfo, simdInfo.lane == SIMDLane::i8x16);
        forEachDRegister(dest, input, [&](FPRegisterID d, FPRegisterID s) { m_assembler.vcnt(d, s); });
    }

    void vectorCeil(SIMDInfo simdInfo, FPRegisterID input, FPRegisterID dest, FPRegisterID)
    {
        ASSERT(scalarTypeIsFloatingPoint(simdInfo.lane));
        switch (simdInfo.lane) {
        case SIMDLane::f32x4: {
            for (int i = 0; i < 4; i++) {
                auto sd = static_cast<ARMRegisters::FPSingleRegisterID>((dest * 2) + i);
                auto ss = static_cast<ARMRegisters::FPSingleRegisterID>((input * 2) + i);
                ceilFloat(ss, sd);
            }
            break;
        }
        case SIMDLane::f64x2:
            forEachDRegister(dest, input, [&](FPRegisterID d, FPRegisterID s) { ceilDouble(s, d); });
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
    }

    void vectorFloor(SIMDInfo simdInfo, FPRegisterID input, FPRegisterID dest, FPRegisterID)
    {
        ASSERT(scalarTypeIsFloatingPoint(simdInfo.lane));
        switch (simdInfo.lane) {
        case SIMDLane::f32x4: {
            for (int i = 0; i < 4; i++) {
                auto sd = static_cast<ARMRegisters::FPSingleRegisterID>((dest * 2) + i);
                auto ss = static_cast<ARMRegisters::FPSingleRegisterID>((input * 2) + i);
                floorFloat(ss, sd);
            }
            break;
        }
        case SIMDLane::f64x2:
            forEachDRegister(dest, input, [&](FPRegisterID d, FPRegisterID s) { floorDouble(s, d); });
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
    }

    void vectorTrunc(SIMDInfo simdInfo, FPRegisterID input, FPRegisterID dest, FPRegisterID)
    {
        ASSERT(scalarTypeIsFloatingPoint(simdInfo.lane));
        switch (simdInfo.lane) {
        case SIMDLane::f32x4: {
            for (int i = 0; i < 4; i++) {
                auto sd = static_cast<ARMRegisters::FPSingleRegisterID>((dest * 2) + i);
                auto ss = static_cast<ARMRegisters::FPSingleRegisterID>((input * 2) + i);
                truncFloat(ss, sd);
            }
            break;
        }
        case SIMDLane::f64x2:
            forEachDRegister(dest, input, [&](FPRegisterID d, FPRegisterID s) { truncDouble(s, d); });
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
    }

    void vectorNearest(SIMDInfo simdInfo, FPRegisterID input, FPRegisterID dest, FPRegisterID)
    {
        ASSERT(scalarTypeIsFloatingPoint(simdInfo.lane));
        switch (simdInfo.lane) {
        case SIMDLane::f32x4: {
            for (int i = 0; i < 4; i++) {
                auto sd = static_cast<ARMRegisters::FPSingleRegisterID>((dest * 2) + i);
                auto ss = static_cast<ARMRegisters::FPSingleRegisterID>((input * 2) + i);
                roundTowardNearestIntFloat(ss, sd);
            }
            break;
        }
        case SIMDLane::f64x2:
            forEachDRegister(dest, input, [&](FPRegisterID d, FPRegisterID s) { roundTowardNearestIntDouble(s, d); });
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
    }

    void vectorSqrt(SIMDInfo simdInfo, FPRegisterID input, FPRegisterID dest)
    {
        ASSERT(scalarTypeIsFloatingPoint(simdInfo.lane));
        switch (simdInfo.lane) {
        case SIMDLane::f32x4:
            for (int i = 0; i < 4; i++) {
                auto sd = static_cast<ARMRegisters::FPSingleRegisterID>((dest * 2) + i);
                auto sm = static_cast<ARMRegisters::FPSingleRegisterID>((input * 2) + i);
                m_assembler.vsqrt(sd, sm);
            }
            break;
        case SIMDLane::f64x2:
            forEachDRegister(dest, input, [&](FPRegisterID d, FPRegisterID s) { m_assembler.vsqrt(d, s); });
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
    }

    void vectorExtaddPairwise(SIMDInfo simdInfo, FPRegisterID a, FPRegisterID dst)
    {
        switch (simdInfo.lane) {
        case SIMDLane::i8x16:
            if (simdInfo.signMode == SIMDSignMode::Signed)
                forEachDRegister(dst, a, [&](FPRegisterID d, FPRegisterID s) { m_assembler.vpaddl_s8(d, s); });
            else
                forEachDRegister(dst, a, [&](FPRegisterID d, FPRegisterID s) { m_assembler.vpaddl_u8(d, s); });
            break;
        case SIMDLane::i16x8:
            if (simdInfo.signMode == SIMDSignMode::Signed)
                forEachDRegister(dst, a, [&](FPRegisterID d, FPRegisterID s) { m_assembler.vpaddl_s16(d, s); });
            else
                forEachDRegister(dst, a, [&](FPRegisterID d, FPRegisterID s) { m_assembler.vpaddl_u16(d, s); });
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
    }

    void vectorConvert(SIMDInfo simdInfo, FPRegisterID input, FPRegisterID dest)
    {
        ASSERT(scalarTypeIsIntegral(simdInfo.lane));
        ASSERT(elementByteSize(simdInfo.lane) == 4);
        ASSERT(simdInfo.signMode != SIMDSignMode::None);
        if (simdInfo.signMode == SIMDSignMode::Signed)
            forEachDRegister(dest, input, [&](FPRegisterID d, FPRegisterID s) { m_assembler.vcvt_f32_s32(d, s); });
        else
            forEachDRegister(dest, input, [&](FPRegisterID d, FPRegisterID s) { m_assembler.vcvt_f32_u32(d, s); });
    }

    void vectorConvertLow(SIMDInfo simdInfo, FPRegisterID input, FPRegisterID dest)
    {
        ASSERT(scalarTypeIsIntegral(simdInfo.lane));
        ASSERT(elementByteSize(simdInfo.lane) == 4);
        auto inputS0 = ARMRegisters::asSingle(input);
        auto inputS1 = ARMRegisters::asSingleUpper(input);
        auto destD0 = dest;
        auto destD1 = static_cast<FPRegisterID>(dest + 1);

        if (simdInfo.signMode == SIMDSignMode::Signed) {
            m_assembler.vcvt_f64_s32(destD1, inputS1);
            m_assembler.vcvt_f64_s32(destD0, inputS0);
        } else {
            m_assembler.vcvt_f64_u32(destD1, inputS1);
            m_assembler.vcvt_f64_u32(destD0, inputS0);
        }
    }

    void vectorTruncSat(SIMDInfo simdInfo, FPRegisterID input, FPRegisterID dest)
    {
        ASSERT(scalarTypeIsFloatingPoint(simdInfo.lane));
        ASSERT(simdInfo.signMode != SIMDSignMode::None);
        if (simdInfo.lane == SIMDLane::f32x4) {
            if (simdInfo.signMode == SIMDSignMode::Signed)
                forEachDRegister(dest, input, [&](FPRegisterID d, FPRegisterID s) { m_assembler.vcvt_s32_f32(d, s); });
            else
                forEachDRegister(dest, input, [&](FPRegisterID d, FPRegisterID s) { m_assembler.vcvt_u32_f32(d, s); });
        } else {
            auto destS0 = ARMRegisters::asSingle(dest);
            auto destS1 = ARMRegisters::asSingleUpper(dest);
            auto destD1 = static_cast<FPRegisterID>(dest + 1);

            if (simdInfo.signMode == SIMDSignMode::Signed) {
                m_assembler.vcvt_s32_f64(destS0, input);
                m_assembler.vcvt_s32_f64(destS1, static_cast<FPRegisterID>(input + 1));
            } else {
                m_assembler.vcvt_u32_f64(destS0, input);
                m_assembler.vcvt_u32_f64(destS1, static_cast<FPRegisterID>(input + 1));
            }
            // Zero the upper 64 bits
            m_assembler.veor(destD1, destD1, destD1);
        }
    }

    void vectorNot(SIMDInfo, FPRegisterID input, FPRegisterID dest)
    {
        forEachDRegister(dest, input, [&](FPRegisterID d, FPRegisterID s) { m_assembler.vmvn(d, s); });
    }

    void vectorAnd(SIMDInfo, FPRegisterID left, FPRegisterID right, FPRegisterID dest)
    {
        forEachDRegister(dest, left, right, [&](FPRegisterID d, FPRegisterID l, FPRegisterID r) { m_assembler.vand(d, l, r); });
    }

    void vectorAndnot(SIMDInfo, FPRegisterID left, FPRegisterID right, FPRegisterID dest)
    {
        forEachDRegister(dest, left, right, [&](FPRegisterID d, FPRegisterID l, FPRegisterID r) { m_assembler.vbic(d, l, r); });
    }

    void vectorOr(SIMDInfo, FPRegisterID left, FPRegisterID right, FPRegisterID dest)
    {
        forEachDRegister(dest, left, right, [&](FPRegisterID d, FPRegisterID l, FPRegisterID r) { m_assembler.vorr(d, l, r); });
    }

    void vectorXor(SIMDInfo, FPRegisterID left, FPRegisterID right, FPRegisterID dest)
    {
        forEachDRegister(dest, left, right, [&](FPRegisterID d, FPRegisterID l, FPRegisterID r) { m_assembler.veor(d, l, r); });
    }

    void vectorAdd(SIMDInfo simdInfo, FPRegisterID left, FPRegisterID right, FPRegisterID dest)
    {
        switch (simdInfo.lane) {
        case SIMDLane::i8x16:
            forEachDRegister(dest, left, right, [&](FPRegisterID d, FPRegisterID l, FPRegisterID r) { m_assembler.vadd_i8(d, l, r); });
            break;
        case SIMDLane::i16x8:
            forEachDRegister(dest, left, right, [&](FPRegisterID d, FPRegisterID l, FPRegisterID r) { m_assembler.vadd_i16(d, l, r); });
            break;
        case SIMDLane::i32x4:
            forEachDRegister(dest, left, right, [&](FPRegisterID d, FPRegisterID l, FPRegisterID r) { m_assembler.vadd_i32(d, l, r); });
            break;
        case SIMDLane::i64x2:
            forEachDRegister(dest, left, right, [&](FPRegisterID d, FPRegisterID l, FPRegisterID r) { m_assembler.vadd_i64(d, l, r); });
            break;
        case SIMDLane::f32x4:
            // NEON vadd.f32 flushes denormals, use VFP scalar vadd for IEEE compliance
            for (int i = 0; i < 4; i++) {
                auto sd = static_cast<ARMRegisters::FPSingleRegisterID>((dest * 2) + i);
                auto sl = static_cast<ARMRegisters::FPSingleRegisterID>((left * 2) + i);
                auto sr = static_cast<ARMRegisters::FPSingleRegisterID>((right * 2) + i);
                m_assembler.vadd(sd, sl, sr);
            }
            break;
        case SIMDLane::f64x2:
            forEachDRegister(dest, left, right, [&](FPRegisterID d, FPRegisterID l, FPRegisterID r) { m_assembler.vadd(d, l, r); });
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
    }

    void vectorSub(SIMDInfo simdInfo, FPRegisterID left, FPRegisterID right, FPRegisterID dest)
    {
        switch (simdInfo.lane) {
        case SIMDLane::i8x16:
            forEachDRegister(dest, left, right, [&](FPRegisterID d, FPRegisterID l, FPRegisterID r) { m_assembler.vsub_i8(d, l, r); });
            break;
        case SIMDLane::i16x8:
            forEachDRegister(dest, left, right, [&](FPRegisterID d, FPRegisterID l, FPRegisterID r) { m_assembler.vsub_i16(d, l, r); });
            break;
        case SIMDLane::i32x4:
            forEachDRegister(dest, left, right, [&](FPRegisterID d, FPRegisterID l, FPRegisterID r) { m_assembler.vsub_i32(d, l, r); });
            break;
        case SIMDLane::i64x2:
            forEachDRegister(dest, left, right, [&](FPRegisterID d, FPRegisterID l, FPRegisterID r) { m_assembler.vsub_i64(d, l, r); });
            break;
        case SIMDLane::f32x4:
            // NEON vsub.f32 flushes denormals, use VFP scalar vsub for IEEE compliance
            for (int i = 0; i < 4; i++) {
                auto sd = static_cast<ARMRegisters::FPSingleRegisterID>((dest * 2) + i);
                auto sl = static_cast<ARMRegisters::FPSingleRegisterID>((left * 2) + i);
                auto sr = static_cast<ARMRegisters::FPSingleRegisterID>((right * 2) + i);
                m_assembler.vsub(sd, sl, sr);
            }
            break;
        case SIMDLane::f64x2:
            forEachDRegister(dest, left, right, [&](FPRegisterID d, FPRegisterID l, FPRegisterID r) { m_assembler.vsub(d, l, r); });
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
    }

    void vectorMul(SIMDInfo simdInfo, FPRegisterID left, FPRegisterID right, FPRegisterID dest)
    {
        switch (simdInfo.lane) {
        case SIMDLane::i8x16:
            forEachDRegister(dest, left, right, [&](FPRegisterID d, FPRegisterID l, FPRegisterID r) { m_assembler.vmul_i8(d, l, r); });
            break;
        case SIMDLane::i16x8:
            forEachDRegister(dest, left, right, [&](FPRegisterID d, FPRegisterID l, FPRegisterID r) { m_assembler.vmul_i16(d, l, r); });
            break;
        case SIMDLane::i32x4:
            forEachDRegister(dest, left, right, [&](FPRegisterID d, FPRegisterID l, FPRegisterID r) { m_assembler.vmul_i32(d, l, r); });
            break;
        case SIMDLane::f32x4:
            // NEON vmul.f32 flushes denormals to zero, use VFP scalar vmul for IEEE compliance
            for (int i = 0; i < 4; i++) {
                auto sd = static_cast<ARMRegisters::FPSingleRegisterID>((dest * 2) + i);
                auto sl = static_cast<ARMRegisters::FPSingleRegisterID>((left * 2) + i);
                auto sr = static_cast<ARMRegisters::FPSingleRegisterID>((right * 2) + i);
                m_assembler.vmul(sd, sl, sr);
            }
            break;
        case SIMDLane::f64x2:
            forEachDRegister(dest, left, right, [&](FPRegisterID d, FPRegisterID l, FPRegisterID r) { m_assembler.vmul(d, l, r); });
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
    }

    void vectorDiv(SIMDInfo simdInfo, FPRegisterID left, FPRegisterID right, FPRegisterID dest)
    {
        ASSERT(scalarTypeIsFloatingPoint(simdInfo.lane));
        switch (simdInfo.lane) {
        case SIMDLane::f32x4:
            for (int i = 0; i < 4; i++) {
                auto sd = static_cast<ARMRegisters::FPSingleRegisterID>((dest * 2) + i);
                auto sl = static_cast<ARMRegisters::FPSingleRegisterID>((left * 2) + i);
                auto sr = static_cast<ARMRegisters::FPSingleRegisterID>((right * 2) + i);
                m_assembler.vdiv(sd, sl, sr);
            }
            break;
        case SIMDLane::f64x2:
            forEachDRegister(dest, left, right, [&](FPRegisterID d, FPRegisterID l, FPRegisterID r) { m_assembler.vdiv(d, l, r); });
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
    }

    // IEEE 754-2019 compliant float min: propagates NaN, -0.0 < +0.0
    void floatMin(ARMRegisters::FPSingleRegisterID left, ARMRegisters::FPSingleRegisterID right, ARMRegisters::FPSingleRegisterID dest)
    {
        Jump isEqual = branchFloat(DoubleEqualAndOrdered, left, right);
        Jump isLessThan = branchFloat(DoubleLessThanAndOrdered, left, right);
        Jump isGreaterThan = branchFloat(DoubleGreaterThanAndOrdered, left, right);

        // NaN case: propagate via addition
        m_assembler.vadd(dest, left, right);
        Jump afterNaN = jump();

        // Left > right: min is right
        isGreaterThan.link(this);
        moveFloat(right, dest);
        Jump afterGreaterThan = jump();

        // Left < right: min is left
        isLessThan.link(this);
        moveFloat(left, dest);
        Jump afterLessThan = jump();

        // Equal case: OR to handle -0.0 vs +0.0
        isEqual.link(this);
        orFloat(left, right, dest);

        afterNaN.link(this);
        afterGreaterThan.link(this);
        afterLessThan.link(this);
    }

    // IEEE 754-2019 compliant float max: propagates NaN, +0.0 > -0.0
    void floatMax(ARMRegisters::FPSingleRegisterID left, ARMRegisters::FPSingleRegisterID right, ARMRegisters::FPSingleRegisterID dest)
    {
        Jump isEqual = branchFloat(DoubleEqualAndOrdered, left, right);
        Jump isLessThan = branchFloat(DoubleLessThanAndOrdered, left, right);
        Jump isGreaterThan = branchFloat(DoubleGreaterThanAndOrdered, left, right);

        // NaN case: propagate via addition
        m_assembler.vadd(dest, left, right);
        Jump afterNaN = jump();

        // Left > right: max is left
        isGreaterThan.link(this);
        moveFloat(left, dest);
        Jump afterGreaterThan = jump();

        // Left < right: max is right
        isLessThan.link(this);
        moveFloat(right, dest);
        Jump afterLessThan = jump();

        // Equal case: AND to handle -0.0 vs +0.0
        isEqual.link(this);
        andFloat(left, right, dest);

        afterNaN.link(this);
        afterGreaterThan.link(this);
        afterLessThan.link(this);
    }

    void doubleMin(FPRegisterID left, FPRegisterID right, FPRegisterID dest)
    {
        Jump isEqual = branchDouble(DoubleEqualAndOrdered, left, right);
        Jump isLessThan = branchDouble(DoubleLessThanAndOrdered, left, right);
        Jump isGreaterThan = branchDouble(DoubleGreaterThanAndOrdered, left, right);

        addDouble(left, right, dest);
        Jump afterNaN = jump();

        isGreaterThan.link(this);
        moveDouble(right, dest);
        Jump afterGreaterThan = jump();

        isLessThan.link(this);
        moveDouble(left, dest);
        Jump afterLessThan = jump();

        isEqual.link(this);
        orDouble(left, right, dest);

        afterNaN.link(this);
        afterGreaterThan.link(this);
        afterLessThan.link(this);
    }

    void doubleMax(FPRegisterID left, FPRegisterID right, FPRegisterID dest)
    {
        Jump isEqual = branchDouble(DoubleEqualAndOrdered, left, right);
        Jump isLessThan = branchDouble(DoubleLessThanAndOrdered, left, right);
        Jump isGreaterThan = branchDouble(DoubleGreaterThanAndOrdered, left, right);

        addDouble(left, right, dest);
        Jump afterNaN = jump();

        isGreaterThan.link(this);
        moveDouble(left, dest);
        Jump afterGreaterThan = jump();

        isLessThan.link(this);
        moveDouble(right, dest);
        Jump afterLessThan = jump();

        isEqual.link(this);
        andDouble(left, right, dest);

        afterNaN.link(this);
        afterGreaterThan.link(this);
        afterLessThan.link(this);
    }

    void vectorMin(SIMDInfo simdInfo, FPRegisterID left, FPRegisterID right, FPRegisterID dest)
    {
        switch (simdInfo.lane) {
        case SIMDLane::i8x16:
            if (simdInfo.signMode == SIMDSignMode::Signed)
                forEachDRegister(dest, left, right, [&](FPRegisterID d, FPRegisterID l, FPRegisterID r) { m_assembler.vmin_s8(d, l, r); });
            else
                forEachDRegister(dest, left, right, [&](FPRegisterID d, FPRegisterID l, FPRegisterID r) { m_assembler.vmin_u8(d, l, r); });
            break;
        case SIMDLane::i16x8:
            if (simdInfo.signMode == SIMDSignMode::Signed)
                forEachDRegister(dest, left, right, [&](FPRegisterID d, FPRegisterID l, FPRegisterID r) { m_assembler.vmin_s16(d, l, r); });
            else
                forEachDRegister(dest, left, right, [&](FPRegisterID d, FPRegisterID l, FPRegisterID r) { m_assembler.vmin_u16(d, l, r); });
            break;
        case SIMDLane::i32x4:
            if (simdInfo.signMode == SIMDSignMode::Signed)
                forEachDRegister(dest, left, right, [&](FPRegisterID d, FPRegisterID l, FPRegisterID r) { m_assembler.vmin_s32(d, l, r); });
            else
                forEachDRegister(dest, left, right, [&](FPRegisterID d, FPRegisterID l, FPRegisterID r) { m_assembler.vmin_u32(d, l, r); });
            break;
        case SIMDLane::f32x4:
            for (int i = 0; i < 4; i++) {
                auto sd = static_cast<ARMRegisters::FPSingleRegisterID>((dest * 2) + i);
                auto sl = static_cast<ARMRegisters::FPSingleRegisterID>((left * 2) + i);
                auto sr = static_cast<ARMRegisters::FPSingleRegisterID>((right * 2) + i);
                floatMin(sl, sr, sd);
            }
            break;
        case SIMDLane::f64x2:
            forEachDRegister(dest, left, right, [&](FPRegisterID d, FPRegisterID l, FPRegisterID r) { doubleMin(l, r, d); });
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
    }

    void vectorMax(SIMDInfo simdInfo, FPRegisterID left, FPRegisterID right, FPRegisterID dest)
    {
        switch (simdInfo.lane) {
        case SIMDLane::i8x16:
            if (simdInfo.signMode == SIMDSignMode::Signed)
                forEachDRegister(dest, left, right, [&](FPRegisterID d, FPRegisterID l, FPRegisterID r) { m_assembler.vmax_s8(d, l, r); });
            else
                forEachDRegister(dest, left, right, [&](FPRegisterID d, FPRegisterID l, FPRegisterID r) { m_assembler.vmax_u8(d, l, r); });
            break;
        case SIMDLane::i16x8:
            if (simdInfo.signMode == SIMDSignMode::Signed)
                forEachDRegister(dest, left, right, [&](FPRegisterID d, FPRegisterID l, FPRegisterID r) { m_assembler.vmax_s16(d, l, r); });
            else
                forEachDRegister(dest, left, right, [&](FPRegisterID d, FPRegisterID l, FPRegisterID r) { m_assembler.vmax_u16(d, l, r); });
            break;
        case SIMDLane::i32x4:
            if (simdInfo.signMode == SIMDSignMode::Signed)
                forEachDRegister(dest, left, right, [&](FPRegisterID d, FPRegisterID l, FPRegisterID r) { m_assembler.vmax_s32(d, l, r); });
            else
                forEachDRegister(dest, left, right, [&](FPRegisterID d, FPRegisterID l, FPRegisterID r) { m_assembler.vmax_u32(d, l, r); });
            break;
        case SIMDLane::f32x4:
            for (int i = 0; i < 4; i++) {
                auto sd = static_cast<ARMRegisters::FPSingleRegisterID>((dest * 2) + i);
                auto sl = static_cast<ARMRegisters::FPSingleRegisterID>((left * 2) + i);
                auto sr = static_cast<ARMRegisters::FPSingleRegisterID>((right * 2) + i);
                floatMax(sl, sr, sd);
            }
            break;
        case SIMDLane::f64x2:
            forEachDRegister(dest, left, right, [&](FPRegisterID d, FPRegisterID l, FPRegisterID r) { doubleMax(l, r, d); });
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
    }

    void vectorAddSat(SIMDInfo simdInfo, FPRegisterID left, FPRegisterID right, FPRegisterID dest)
    {
        ASSERT(scalarTypeIsIntegral(simdInfo.lane));
        switch (simdInfo.lane) {
        case SIMDLane::i8x16:
            if (simdInfo.signMode == SIMDSignMode::Signed)
                forEachDRegister(dest, left, right, [&](FPRegisterID d, FPRegisterID l, FPRegisterID r) { m_assembler.vqadd_s8(d, l, r); });
            else
                forEachDRegister(dest, left, right, [&](FPRegisterID d, FPRegisterID l, FPRegisterID r) { m_assembler.vqadd_u8(d, l, r); });
            break;
        case SIMDLane::i16x8:
            if (simdInfo.signMode == SIMDSignMode::Signed)
                forEachDRegister(dest, left, right, [&](FPRegisterID d, FPRegisterID l, FPRegisterID r) { m_assembler.vqadd_s16(d, l, r); });
            else
                forEachDRegister(dest, left, right, [&](FPRegisterID d, FPRegisterID l, FPRegisterID r) { m_assembler.vqadd_u16(d, l, r); });
            break;
        case SIMDLane::i32x4:
            if (simdInfo.signMode == SIMDSignMode::Signed)
                forEachDRegister(dest, left, right, [&](FPRegisterID d, FPRegisterID l, FPRegisterID r) { m_assembler.vqadd_s32(d, l, r); });
            else
                forEachDRegister(dest, left, right, [&](FPRegisterID d, FPRegisterID l, FPRegisterID r) { m_assembler.vqadd_u32(d, l, r); });
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
    }

    void vectorSubSat(SIMDInfo simdInfo, FPRegisterID left, FPRegisterID right, FPRegisterID dest)
    {
        ASSERT(scalarTypeIsIntegral(simdInfo.lane));
        switch (simdInfo.lane) {
        case SIMDLane::i8x16:
            if (simdInfo.signMode == SIMDSignMode::Signed)
                forEachDRegister(dest, left, right, [&](FPRegisterID d, FPRegisterID l, FPRegisterID r) { m_assembler.vqsub_s8(d, l, r); });
            else
                forEachDRegister(dest, left, right, [&](FPRegisterID d, FPRegisterID l, FPRegisterID r) { m_assembler.vqsub_u8(d, l, r); });
            break;
        case SIMDLane::i16x8:
            if (simdInfo.signMode == SIMDSignMode::Signed)
                forEachDRegister(dest, left, right, [&](FPRegisterID d, FPRegisterID l, FPRegisterID r) { m_assembler.vqsub_s16(d, l, r); });
            else
                forEachDRegister(dest, left, right, [&](FPRegisterID d, FPRegisterID l, FPRegisterID r) { m_assembler.vqsub_u16(d, l, r); });
            break;
        case SIMDLane::i32x4:
            if (simdInfo.signMode == SIMDSignMode::Signed)
                forEachDRegister(dest, left, right, [&](FPRegisterID d, FPRegisterID l, FPRegisterID r) { m_assembler.vqsub_s32(d, l, r); });
            else
                forEachDRegister(dest, left, right, [&](FPRegisterID d, FPRegisterID l, FPRegisterID r) { m_assembler.vqsub_u32(d, l, r); });
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
    }

    void vectorAvgRound(SIMDInfo simdInfo, FPRegisterID left, FPRegisterID right, FPRegisterID dest)
    {
        ASSERT(scalarTypeIsIntegral(simdInfo.lane));
        switch (simdInfo.lane) {
        case SIMDLane::i8x16:
            if (simdInfo.signMode == SIMDSignMode::Signed)
                forEachDRegister(dest, left, right, [&](FPRegisterID d, FPRegisterID l, FPRegisterID r) { m_assembler.vrhadd_s8(d, l, r); });
            else
                forEachDRegister(dest, left, right, [&](FPRegisterID d, FPRegisterID l, FPRegisterID r) { m_assembler.vrhadd_u8(d, l, r); });
            break;
        case SIMDLane::i16x8:
            if (simdInfo.signMode == SIMDSignMode::Signed)
                forEachDRegister(dest, left, right, [&](FPRegisterID d, FPRegisterID l, FPRegisterID r) { m_assembler.vrhadd_s16(d, l, r); });
            else
                forEachDRegister(dest, left, right, [&](FPRegisterID d, FPRegisterID l, FPRegisterID r) { m_assembler.vrhadd_u16(d, l, r); });
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
    }

    void vectorMulSat(FPRegisterID left, FPRegisterID right, FPRegisterID dest, RegisterID, FPRegisterID)
    {
        forEachDRegister(dest, left, right, [&](FPRegisterID d, FPRegisterID l, FPRegisterID r) { m_assembler.vqrdmulh_i16(d, l, r); });
    }

    void vectorPmin(SIMDInfo simdInfo, FPRegisterID left, FPRegisterID right, FPRegisterID dest, FPRegisterID)
    {
        ASSERT(scalarTypeIsFloatingPoint(simdInfo.lane));

        // Use scalar VFP comparisons which handle denormals correctly.
        if (simdInfo.lane == SIMDLane::f32x4) {
            auto emitF32Pmin = [&](ARMRegisters::FPSingleRegisterID d, ARMRegisters::FPSingleRegisterID l, ARMRegisters::FPSingleRegisterID r) {
                m_assembler.vcmp(l, r);
                m_assembler.vmrs();
                if (d == l) {
                    m_assembler.it(ARMv7Assembler::ConditionGT);
                    moveFloat(r, d);
                } else if (d == r) {
                    m_assembler.it(ARMv7Assembler::ConditionLE);
                    moveFloat(l, d);
                } else {
                    moveFloat(l, d);
                    m_assembler.it(ARMv7Assembler::ConditionGT);
                    moveFloat(r, d);
                }
            };

            for (int i = 0; i < 4; i++) {
                auto sd = static_cast<ARMRegisters::FPSingleRegisterID>((dest * 2) + i);
                auto sl = static_cast<ARMRegisters::FPSingleRegisterID>((left * 2) + i);
                auto sr = static_cast<ARMRegisters::FPSingleRegisterID>((right * 2) + i);
                emitF32Pmin(sd, sl, sr);
            }
        } else {
            ASSERT(simdInfo.lane == SIMDLane::f64x2);
            auto emitF64Pmin = [&](FPRegisterID d, FPRegisterID l, FPRegisterID r) {
                m_assembler.vcmp(l, r);
                m_assembler.vmrs();
                if (d == l) {
                    m_assembler.it(ARMv7Assembler::ConditionGT);
                    moveDouble(r, d);
                } else if (d == r) {
                    m_assembler.it(ARMv7Assembler::ConditionLE);
                    moveDouble(l, d);
                } else {
                    moveDouble(l, d);
                    m_assembler.it(ARMv7Assembler::ConditionGT);
                    moveDouble(r, d);
                }
            };

            emitF64Pmin(dest, left, right);
            emitF64Pmin(static_cast<FPRegisterID>(dest + 1), static_cast<FPRegisterID>(left + 1), static_cast<FPRegisterID>(right + 1));
        }
    }

    void vectorPmax(SIMDInfo simdInfo, FPRegisterID left, FPRegisterID right, FPRegisterID dest, FPRegisterID)
    {
        ASSERT(scalarTypeIsFloatingPoint(simdInfo.lane));

        // Use scalar VFP comparisons which handle denormals correctly.
        if (simdInfo.lane == SIMDLane::f32x4) {
            auto emitF32Pmax = [&](ARMRegisters::FPSingleRegisterID d, ARMRegisters::FPSingleRegisterID l, ARMRegisters::FPSingleRegisterID r) {
                m_assembler.vcmp(l, r);
                m_assembler.vmrs();
                if (d == l) {
                    m_assembler.it(ARMv7Assembler::ConditionMI);
                    moveFloat(r, d);
                } else if (d == r) {
                    m_assembler.it(ARMv7Assembler::ConditionPL);
                    moveFloat(l, d);
                } else {
                    moveFloat(l, d);
                    m_assembler.it(ARMv7Assembler::ConditionMI);
                    moveFloat(r, d);
                }
            };

            for (int i = 0; i < 4; i++) {
                auto sd = static_cast<ARMRegisters::FPSingleRegisterID>((dest * 2) + i);
                auto sl = static_cast<ARMRegisters::FPSingleRegisterID>((left * 2) + i);
                auto sr = static_cast<ARMRegisters::FPSingleRegisterID>((right * 2) + i);
                emitF32Pmax(sd, sl, sr);
            }
        } else {
            ASSERT(simdInfo.lane == SIMDLane::f64x2);
            auto emitF64Pmax = [&](FPRegisterID d, FPRegisterID l, FPRegisterID r) {
                m_assembler.vcmp(l, r);
                m_assembler.vmrs();
                if (d == l) {
                    m_assembler.it(ARMv7Assembler::ConditionMI);
                    moveDouble(r, d);
                } else if (d == r) {
                    m_assembler.it(ARMv7Assembler::ConditionPL);
                    moveDouble(l, d);
                } else {
                    moveDouble(l, d);
                    m_assembler.it(ARMv7Assembler::ConditionMI);
                    moveDouble(r, d);
                }
            };

            emitF64Pmax(dest, left, right);
            emitF64Pmax(static_cast<FPRegisterID>(dest + 1), static_cast<FPRegisterID>(left + 1), static_cast<FPRegisterID>(right + 1));
        }
    }

    void vectorSwizzle(FPRegisterID table, FPRegisterID indices, FPRegisterID dest)
    {
        if (dest == table || dest == static_cast<FPRegisterID>(table + 1)) {
            FPRegisterID scratch = fpTempRegister;
            moveDouble(table, scratch);
            moveDouble(static_cast<FPRegisterID>(table + 1), static_cast<FPRegisterID>(scratch + 1));
            m_assembler.vtbl2(dest, scratch, indices);
            m_assembler.vtbl2(static_cast<FPRegisterID>(dest + 1), scratch, static_cast<FPRegisterID>(indices + 1));
        } else {
            m_assembler.vtbl2(dest, table, indices);
            m_assembler.vtbl2(static_cast<FPRegisterID>(dest + 1), table, static_cast<FPRegisterID>(indices + 1));
        }
    }

    void vectorNarrow(SIMDInfo simdInfo, FPRegisterID lower, FPRegisterID upper, FPRegisterID dest, FPRegisterID scratch)
    {
        ASSERT(simdInfo.signMode != SIMDSignMode::None);
        ASSERT(scalarTypeIsIntegral(simdInfo.lane));
        ASSERT(scratch != upper);

        FPRegisterID scratchHi = static_cast<FPRegisterID>(scratch + 1);

        SIMDLane narrowLane = narrowedLane(simdInfo.lane);
        switch (narrowLane) {
        case SIMDLane::i8x16:
            if (simdInfo.signMode == SIMDSignMode::Signed) {
                m_assembler.vqmovn_s16(scratch, lower);
                m_assembler.vqmovn_s16(scratchHi, upper);
            } else {
                m_assembler.vqmovun_s16(scratch, lower);
                m_assembler.vqmovun_s16(scratchHi, upper);
            }
            break;
        case SIMDLane::i16x8:
            if (simdInfo.signMode == SIMDSignMode::Signed) {
                m_assembler.vqmovn_s32(scratch, lower);
                m_assembler.vqmovn_s32(scratchHi, upper);
            } else {
                m_assembler.vqmovun_s32(scratch, lower);
                m_assembler.vqmovun_s32(scratchHi, upper);
            }
            break;
        case SIMDLane::i32x4:
            if (simdInfo.signMode == SIMDSignMode::Signed) {
                m_assembler.vqmovn_s64(scratch, lower);
                m_assembler.vqmovn_s64(scratchHi, upper);
            } else {
                m_assembler.vqmovun_s64(scratch, lower);
                m_assembler.vqmovun_s64(scratchHi, upper);
            }
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
        moveVector(scratch, dest);
    }

    void vectorDotProduct(FPRegisterID a, FPRegisterID b, FPRegisterID dest, FPRegisterID scratch)
    {
        FPRegisterID aHigh = static_cast<FPRegisterID>(a + 1);
        FPRegisterID bHigh = static_cast<FPRegisterID>(b + 1);
        FPRegisterID scratchHigh = static_cast<FPRegisterID>(scratch + 1);
        FPRegisterID destHigh = static_cast<FPRegisterID>(dest + 1);

        m_assembler.vmull_s16(scratch, aHigh, bHigh);
        m_assembler.vpadd_i32(destHigh, scratch, scratchHigh);

        m_assembler.vmull_s16(scratch, a, b);
        m_assembler.vpadd_i32(dest, scratch, scratchHigh);
    }

    void vectorExtractLane(SIMDLane lane, SIMDSignMode signMode, TrustedImm32 laneIndex, FPRegisterID src, RegisterID dest)
    {
        uint8_t index = laneIndex.m_value;

        switch (lane) {
        case SIMDLane::i8x16: {
            FPRegisterID dReg = static_cast<FPRegisterID>(src + (index >> 3));
            if (signMode == SIMDSignMode::Signed)
                m_assembler.vmovFromVectorElementS8(dest, dReg, index & 0x7);
            else
                m_assembler.vmovFromVectorElement8(dest, dReg, index & 0x7);
            break;
        }
        case SIMDLane::i16x8: {
            FPRegisterID dReg = static_cast<FPRegisterID>(src + (index >> 2));
            if (signMode == SIMDSignMode::Signed)
                m_assembler.vmovFromVectorElementS16(dest, dReg, index & 0x3);
            else
                m_assembler.vmovFromVectorElement16(dest, dReg, index & 0x3);
            break;
        }
        case SIMDLane::i32x4: {
            FPRegisterID dReg = static_cast<FPRegisterID>(src + (index >> 1));
            m_assembler.vmovFromVectorElement32(dest, dReg, index & 0x1);
            break;
        }
        default:
            // i64x2 extraction on 32-bit needs the two-register overload
            RELEASE_ASSERT_NOT_REACHED();
        }
    }

    void vectorExtractLaneInt64(TrustedImm32 laneIndex, FPRegisterID src, RegisterID destLo, RegisterID destHi)
    {
        uint8_t index = laneIndex.m_value;
        FPRegisterID dReg = static_cast<FPRegisterID>(src + index);
        m_assembler.vmovFromVectorElement32(destLo, dReg, 0);
        m_assembler.vmovFromVectorElement32(destHi, dReg, 1);
    }

    void vectorExtractLane(SIMDLane lane, TrustedImm32 laneIndex, FPRegisterID src, FPRegisterID dest)
    {
        uint8_t index = laneIndex.m_value;
        if (!index) {
            moveDouble(src, dest);
            return;
        }

        switch (lane) {
        case SIMDLane::f32x4: {
            FPRegisterID dReg = static_cast<FPRegisterID>(src + (index >> 1));
            auto srcSingle = static_cast<ARMRegisters::FPSingleRegisterID>((dReg << 1) + (index & 1));
            auto destSingle = static_cast<ARMRegisters::FPSingleRegisterID>(dest << 1);
            moveFloat(srcSingle, destSingle);
            break;
        }
        case SIMDLane::f64x2:
            ASSERT(index == 1);
            moveDouble(static_cast<FPRegisterID>(src + 1), dest);
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
    }

    DEFINE_SIGNED_SIMD_FUNCS(vectorExtractLane);

    void vectorReplaceLane(SIMDLane lane, TrustedImm32 laneIndex, RegisterID src, FPRegisterID dest)
    {
        uint8_t index = laneIndex.m_value;
        switch (lane) {
        case SIMDLane::i8x16: {
            FPRegisterID dReg = static_cast<FPRegisterID>(dest + (index >> 3));
            m_assembler.vmovToVectorElement8(dReg, src, index & 0x7);
            break;
        }
        case SIMDLane::i16x8: {
            FPRegisterID dReg = static_cast<FPRegisterID>(dest + (index >> 2));
            m_assembler.vmovToVectorElement16(dReg, src, index & 0x3);
            break;
        }
        case SIMDLane::i32x4: {
            FPRegisterID dReg = static_cast<FPRegisterID>(dest + (index >> 1));
            m_assembler.vmovToVectorElement32(dReg, src, index & 0x1);
            break;
        }
        default:
            // i64x2 replacement on 32-bit needs the two-register overload
            RELEASE_ASSERT_NOT_REACHED();
        }
    }

    void vectorReplaceLaneInt64(TrustedImm32 laneIndex, RegisterID srcLo, RegisterID srcHi, FPRegisterID dest)
    {
        uint8_t index = laneIndex.m_value;
        FPRegisterID dReg = static_cast<FPRegisterID>(dest + index);
        m_assembler.vmovToVectorElement32(dReg, srcLo, 0);
        m_assembler.vmovToVectorElement32(dReg, srcHi, 1);
    }

    void vectorReplaceLane(SIMDLane lane, TrustedImm32 laneIndex, FPRegisterID src, FPRegisterID dest)
    {
        uint8_t index = laneIndex.m_value;
        switch (lane) {
        case SIMDLane::f32x4: {
            FPRegisterID dReg = static_cast<FPRegisterID>(dest + (index >> 1));
            m_assembler.vmovToVectorElementFloat32(dReg, asSingle(src), index & 0x1);
            break;
        }
        case SIMDLane::f64x2: {
            FPRegisterID dReg = static_cast<FPRegisterID>(dest + index);
            m_assembler.vmovToVectorElementFloat64(dReg, src);
            break;
        }
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
    }

    DEFINE_SIMD_FUNCS(vectorReplaceLane);

    void vectorSplat(SIMDLane lane, RegisterID src, FPRegisterID dest)
    {
        ASSERT(scalarTypeIsIntegral(lane));
        switch (lane) {
        case SIMDLane::i8x16:
            forEachDRegister(dest, [&](FPRegisterID d) { m_assembler.vdup8(d, src); });
            break;
        case SIMDLane::i16x8:
            forEachDRegister(dest, [&](FPRegisterID d) { m_assembler.vdup16(d, src); });
            break;
        case SIMDLane::i32x4:
            forEachDRegister(dest, [&](FPRegisterID d) { m_assembler.vdup32(d, src); });
            break;
        default:
            // i64x2 splat on 32-bit needs the two-register overload
            RELEASE_ASSERT_NOT_REACHED();
        }
    }

    void vectorSplat(SIMDLane lane, FPRegisterID src, FPRegisterID dest)
    {
        ASSERT(scalarTypeIsFloatingPoint(lane));
        switch (lane) {
        case SIMDLane::f32x4: {
            RegisterID scratch = getCachedDataTempRegisterIDAndInvalidate();
            m_assembler.vmovFromVectorElement32(scratch, src, 0);
            forEachDRegister(dest, [&](FPRegisterID d) { m_assembler.vdup32(d, scratch); });
            break;
        }
        case SIMDLane::f64x2:
            forEachDRegister(dest, [&](FPRegisterID d) { m_assembler.vorr(d, src, src); });
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
    }

    void vectorSplatInt8(RegisterID src, FPRegisterID dest) { vectorSplat(SIMDLane::i8x16, src, dest); }
    void vectorSplatInt16(RegisterID src, FPRegisterID dest) { vectorSplat(SIMDLane::i16x8, src, dest); }
    void vectorSplatInt32(RegisterID src, FPRegisterID dest) { vectorSplat(SIMDLane::i32x4, src, dest); }
    void vectorSplatInt64(RegisterID lo, RegisterID hi, FPRegisterID dest)
    {
        m_assembler.vmov(dest, lo, hi);
        moveDouble(dest, static_cast<FPRegisterID>(dest + 1));
    }
    void vectorSplatFloat32(FPRegisterID src, FPRegisterID dest) { vectorSplat(SIMDLane::f32x4, src, dest); }
    void vectorSplatFloat64(FPRegisterID src, FPRegisterID dest) { vectorSplat(SIMDLane::f64x2, src, dest); }

    void compareIntegerVector(RelationalCondition cond, SIMDInfo simdInfo, FPRegisterID left, FPRegisterID right, FPRegisterID dest, FPRegisterID scratch)
    {
        RELEASE_ASSERT(scalarTypeIsIntegral(simdInfo.lane));

        auto emitI64ScalarComparison = [&](RelationalCondition condition) {
            auto compareOneLane = [&](FPRegisterID d, FPRegisterID l, FPRegisterID r) {
                RegisterID temp1 = getCachedDataTempRegisterIDAndInvalidate();
                RegisterID temp2 = getCachedAddressTempRegisterIDAndInvalidate();

                ARMRegisters::FPSingleRegisterID s_left_low = ARMRegisters::asSingle(l);
                ARMRegisters::FPSingleRegisterID s_left_high = ARMRegisters::asSingleUpper(l);
                ARMRegisters::FPSingleRegisterID s_right_low = ARMRegisters::asSingle(r);
                ARMRegisters::FPSingleRegisterID s_right_high = ARMRegisters::asSingleUpper(r);

                m_assembler.vmov(temp1, s_left_high);
                m_assembler.vmov(temp2, s_right_high);
                ARMv7Assembler::Condition hiCondition = armV7ConditionForHigh32(condition);
                compare32(hiCondition, temp1, temp2, temp1);
                Jump done = makeBranch(ARMv7Assembler::ConditionNE);

                m_assembler.vmov(temp1, s_left_low);
                m_assembler.vmov(temp2, s_right_low);
                ARMv7Assembler::Condition loCondition = armV7ConditionForLow32(condition);
                compare32(loCondition, temp1, temp2, temp1);

                done.link(this);
                m_assembler.neg(temp1, temp1);
                m_assembler.vmov(d, temp1, temp1);
            };

            compareOneLane(dest, left, right);
            compareOneLane(static_cast<FPRegisterID>(dest + 1), static_cast<FPRegisterID>(left + 1), static_cast<FPRegisterID>(right + 1));
        };

        auto emitComparison = [&](auto compareFn) {
            FPRegisterID leftHi = static_cast<FPRegisterID>(left + 1);
            FPRegisterID rightHi = static_cast<FPRegisterID>(right + 1);

            if (dest == leftHi || dest == rightHi) {
                compareFn(m_assembler, scratch, left, right);
                compareFn(m_assembler, static_cast<FPRegisterID>(dest + 1), leftHi, rightHi);
                moveDouble(scratch, dest);
            } else {
                compareFn(m_assembler, dest, left, right);
                compareFn(m_assembler, static_cast<FPRegisterID>(dest + 1), leftHi, rightHi);
            }
        };

        switch (cond) {
        case Equal:
            switch (simdInfo.lane) {
            case SIMDLane::i8x16:
                emitComparison([](auto& assembler, auto d, auto l, auto r) { assembler.vceq_i8(d, l, r); });
                break;
            case SIMDLane::i16x8:
                emitComparison([](auto& assembler, auto d, auto l, auto r) { assembler.vceq_i16(d, l, r); });
                break;
            case SIMDLane::i32x4:
                emitComparison([](auto& assembler, auto d, auto l, auto r) { assembler.vceq_i32(d, l, r); });
                break;
            case SIMDLane::i64x2:
                emitI64ScalarComparison(Equal);
                break;
            default:
                RELEASE_ASSERT_NOT_REACHED();
            }
            break;
        case NotEqual:
            switch (simdInfo.lane) {
            case SIMDLane::i8x16:
                emitComparison([](auto& assembler, auto d, auto l, auto r) { assembler.vceq_i8(d, l, r); });
                break;
            case SIMDLane::i16x8:
                emitComparison([](auto& assembler, auto d, auto l, auto r) { assembler.vceq_i16(d, l, r); });
                break;
            case SIMDLane::i32x4:
                emitComparison([](auto& assembler, auto d, auto l, auto r) { assembler.vceq_i32(d, l, r); });
                break;
            case SIMDLane::i64x2:
                emitI64ScalarComparison(Equal);
                break;
            default:
                RELEASE_ASSERT_NOT_REACHED();
            }
            vectorNot(simdInfo, dest, dest);
            break;
        case GreaterThan:
            switch (simdInfo.lane) {
            case SIMDLane::i8x16:
                emitComparison([](auto& assembler, auto d, auto l, auto r) { assembler.vcgt_s8(d, l, r); });
                break;
            case SIMDLane::i16x8:
                emitComparison([](auto& assembler, auto d, auto l, auto r) { assembler.vcgt_s16(d, l, r); });
                break;
            case SIMDLane::i32x4:
                emitComparison([](auto& assembler, auto d, auto l, auto r) { assembler.vcgt_s32(d, l, r); });
                break;
            case SIMDLane::i64x2:
                emitI64ScalarComparison(GreaterThan);
                break;
            default:
                RELEASE_ASSERT_NOT_REACHED();
            }
            break;
        case GreaterThanOrEqual:
            switch (simdInfo.lane) {
            case SIMDLane::i8x16:
                emitComparison([](auto& assembler, auto d, auto l, auto r) { assembler.vcge_s8(d, l, r); });
                break;
            case SIMDLane::i16x8:
                emitComparison([](auto& assembler, auto d, auto l, auto r) { assembler.vcge_s16(d, l, r); });
                break;
            case SIMDLane::i32x4:
                emitComparison([](auto& assembler, auto d, auto l, auto r) { assembler.vcge_s32(d, l, r); });
                break;
            case SIMDLane::i64x2:
                emitI64ScalarComparison(GreaterThanOrEqual);
                break;
            default:
                RELEASE_ASSERT_NOT_REACHED();
            }
            break;
        case LessThan:
            switch (simdInfo.lane) {
            case SIMDLane::i8x16:
                emitComparison([](auto& assembler, auto d, auto l, auto r) { assembler.vcgt_s8(d, r, l); });
                break;
            case SIMDLane::i16x8:
                emitComparison([](auto& assembler, auto d, auto l, auto r) { assembler.vcgt_s16(d, r, l); });
                break;
            case SIMDLane::i32x4:
                emitComparison([](auto& assembler, auto d, auto l, auto r) { assembler.vcgt_s32(d, r, l); });
                break;
            case SIMDLane::i64x2:
                emitI64ScalarComparison(LessThan);
                break;
            default:
                RELEASE_ASSERT_NOT_REACHED();
            }
            break;
        case LessThanOrEqual:
            switch (simdInfo.lane) {
            case SIMDLane::i8x16:
                emitComparison([](auto& assembler, auto d, auto l, auto r) { assembler.vcge_s8(d, r, l); });
                break;
            case SIMDLane::i16x8:
                emitComparison([](auto& assembler, auto d, auto l, auto r) { assembler.vcge_s16(d, r, l); });
                break;
            case SIMDLane::i32x4:
                emitComparison([](auto& assembler, auto d, auto l, auto r) { assembler.vcge_s32(d, r, l); });
                break;
            case SIMDLane::i64x2:
                emitI64ScalarComparison(LessThanOrEqual);
                break;
            default:
                RELEASE_ASSERT_NOT_REACHED();
            }
            break;
        case Above:
            switch (simdInfo.lane) {
            case SIMDLane::i8x16:
                emitComparison([](auto& assembler, auto d, auto l, auto r) { assembler.vcgt_u8(d, l, r); });
                break;
            case SIMDLane::i16x8:
                emitComparison([](auto& assembler, auto d, auto l, auto r) { assembler.vcgt_u16(d, l, r); });
                break;
            case SIMDLane::i32x4:
                emitComparison([](auto& assembler, auto d, auto l, auto r) { assembler.vcgt_u32(d, l, r); });
                break;
            case SIMDLane::i64x2:
                emitI64ScalarComparison(Above);
                break;
            default:
                RELEASE_ASSERT_NOT_REACHED();
            }
            break;
        case AboveOrEqual:
            switch (simdInfo.lane) {
            case SIMDLane::i8x16:
                emitComparison([](auto& assembler, auto d, auto l, auto r) { assembler.vcge_u8(d, l, r); });
                break;
            case SIMDLane::i16x8:
                emitComparison([](auto& assembler, auto d, auto l, auto r) { assembler.vcge_u16(d, l, r); });
                break;
            case SIMDLane::i32x4:
                emitComparison([](auto& assembler, auto d, auto l, auto r) { assembler.vcge_u32(d, l, r); });
                break;
            case SIMDLane::i64x2:
                emitI64ScalarComparison(AboveOrEqual);
                break;
            default:
                RELEASE_ASSERT_NOT_REACHED();
            }
            break;
        case Below:
            switch (simdInfo.lane) {
            case SIMDLane::i8x16:
                emitComparison([](auto& assembler, auto d, auto l, auto r) { assembler.vcgt_u8(d, r, l); });
                break;
            case SIMDLane::i16x8:
                emitComparison([](auto& assembler, auto d, auto l, auto r) { assembler.vcgt_u16(d, r, l); });
                break;
            case SIMDLane::i32x4:
                emitComparison([](auto& assembler, auto d, auto l, auto r) { assembler.vcgt_u32(d, r, l); });
                break;
            case SIMDLane::i64x2:
                emitI64ScalarComparison(Below);
                break;
            default:
                RELEASE_ASSERT_NOT_REACHED();
            }
            break;
        case BelowOrEqual:
            switch (simdInfo.lane) {
            case SIMDLane::i8x16:
                emitComparison([](auto& assembler, auto d, auto l, auto r) { assembler.vcge_u8(d, r, l); });
                break;
            case SIMDLane::i16x8:
                emitComparison([](auto& assembler, auto d, auto l, auto r) { assembler.vcge_u16(d, r, l); });
                break;
            case SIMDLane::i32x4:
                emitComparison([](auto& assembler, auto d, auto l, auto r) { assembler.vcge_u32(d, r, l); });
                break;
            case SIMDLane::i64x2:
                emitI64ScalarComparison(BelowOrEqual);
                break;
            default:
                RELEASE_ASSERT_NOT_REACHED();
            }
            break;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
    }

    void compareFloatingPointVector(DoubleCondition cond, SIMDInfo simdInfo, FPRegisterID left, FPRegisterID right, FPRegisterID dest)
    {
        RELEASE_ASSERT(scalarTypeIsFloatingPoint(simdInfo.lane));

        // No scratch needed: each lane reads its inputs before writing output,
        // so aliasing between dest and left/right is safe.

        auto emitF32ScalarComparison = [&](ARMv7Assembler::Condition condition) {
            RegisterID allOnes = getCachedDataTempRegisterIDAndInvalidate();
            RegisterID zero = getCachedAddressTempRegisterIDAndInvalidate();
            m_assembler.mvn(allOnes, ARMThumbImmediate::makeEncodedImm(0));
            m_assembler.mov(zero, ARMThumbImmediate::makeEncodedImm(0));

            for (int i = 0; i < 4; i++) {
                auto sd = static_cast<ARMRegisters::FPSingleRegisterID>((dest * 2) + i);
                auto sl = static_cast<ARMRegisters::FPSingleRegisterID>((left * 2) + i);
                auto sr = static_cast<ARMRegisters::FPSingleRegisterID>((right * 2) + i);
                m_assembler.vcmp(sl, sr);
                m_assembler.vmrs();
                m_assembler.it(condition, false);
                m_assembler.vmov(sd, allOnes);
                m_assembler.vmov(sd, zero);
            }
        };

        auto emitF64ScalarComparison = [&](ARMv7Assembler::Condition condition) {
            RegisterID allOnes = getCachedDataTempRegisterIDAndInvalidate();
            m_assembler.mvn(allOnes, ARMThumbImmediate::makeEncodedImm(0));

            auto compareOneLane = [&](FPRegisterID d, FPRegisterID l, FPRegisterID r) {
                m_assembler.vcmp(l, r);
                m_assembler.vmrs();
                m_assembler.it(condition, false);
                m_assembler.vmov(d, allOnes, allOnes);
                m_assembler.veor(d, d, d);
            };

            compareOneLane(dest, left, right);
            compareOneLane(static_cast<FPRegisterID>(dest + 1), static_cast<FPRegisterID>(left + 1), static_cast<FPRegisterID>(right + 1));
        };

        ARMv7Assembler::Condition condition = static_cast<ARMv7Assembler::Condition>(cond);
        if (simdInfo.lane == SIMDLane::f32x4)
            emitF32ScalarComparison(condition);
        else
            emitF64ScalarComparison(condition);
    }

    void vectorFusedMulAdd(SIMDInfo simdInfo, FPRegisterID mul1, FPRegisterID mul2, FPRegisterID addend, FPRegisterID dest, FPRegisterID scratch)
    {
        ASSERT(scalarTypeIsFloatingPoint(simdInfo.lane));
        if (mul1 == dest) {
            moveVector(mul1, scratch);
            mul1 = scratch;
        }
        moveVector(addend, dest);
        if (simdInfo.lane == SIMDLane::f32x4) {
            // NEON vfma.f32 flushes denormals, use VFP scalar vfma
            for (int i = 0; i < 4; i++) {
                auto sd = static_cast<ARMRegisters::FPSingleRegisterID>((dest * 2) + i);
                auto s1 = static_cast<ARMRegisters::FPSingleRegisterID>((mul1 * 2) + i);
                auto s2 = static_cast<ARMRegisters::FPSingleRegisterID>((mul2 * 2) + i);
                m_assembler.vfma(sd, s1, s2);
            }
        } else if (simdInfo.lane == SIMDLane::f64x2) {
            m_assembler.vfma(dest, mul1, mul2);
            m_assembler.vfma(static_cast<FPRegisterID>(dest + 1), static_cast<FPRegisterID>(mul1 + 1), static_cast<FPRegisterID>(mul2 + 1));
        } else
            UNREACHABLE_FOR_PLATFORM();
    }

    void vectorFusedNegMulAdd(SIMDInfo simdInfo, FPRegisterID mul1, FPRegisterID mul2, FPRegisterID addend, FPRegisterID dest, FPRegisterID scratch)
    {
        ASSERT(scalarTypeIsFloatingPoint(simdInfo.lane));
        if (mul1 == dest) {
            moveVector(mul1, scratch);
            mul1 = scratch;
        }
        moveVector(addend, dest);
        if (simdInfo.lane == SIMDLane::f32x4) {
            // NEON vfms.f32 flushes denormals, use VFP scalar vfms
            for (int i = 0; i < 4; i++) {
                auto sd = static_cast<ARMRegisters::FPSingleRegisterID>((dest * 2) + i);
                auto s1 = static_cast<ARMRegisters::FPSingleRegisterID>((mul1 * 2) + i);
                auto s2 = static_cast<ARMRegisters::FPSingleRegisterID>((mul2 * 2) + i);
                m_assembler.vfms(sd, s1, s2);
            }
        } else if (simdInfo.lane == SIMDLane::f64x2) {
            m_assembler.vfms(dest, mul1, mul2);
            m_assembler.vfms(static_cast<FPRegisterID>(dest + 1), static_cast<FPRegisterID>(mul1 + 1), static_cast<FPRegisterID>(mul2 + 1));
        } else
            UNREACHABLE_FOR_PLATFORM();
    }

    void materializeVector(v128_t value, FPRegisterID dest)
    {
        FPRegisterID loDReg = dest;
        FPRegisterID hiDReg = static_cast<FPRegisterID>(dest + 1);

        RegisterID scratch1 = getCachedDataTempRegisterIDAndInvalidate();
        RegisterID scratch2 = getCachedAddressTempRegisterIDAndInvalidate();

        move(TrustedImm32(value.u32x4[0]), scratch1);
        move(TrustedImm32(value.u32x4[1]), scratch2);
        move64ToDouble(scratch2, scratch1, loDReg);

        move(TrustedImm32(value.u32x4[2]), scratch1);
        move(TrustedImm32(value.u32x4[3]), scratch2);
        move64ToDouble(scratch2, scratch1, hiDReg);
    }

private:
    Jump makeFPBranch(DoubleCondition cond)
    {
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

public:
    Jump branchFloat(DoubleCondition cond, FPRegisterID left, FPRegisterID right)
    {
        m_assembler.vcmp(asSingle(left), asSingle(right));
        return makeFPBranch(cond);
    }

    Jump branchFloat(DoubleCondition cond, ARMRegisters::FPSingleRegisterID left, ARMRegisters::FPSingleRegisterID right)
    {
        m_assembler.vcmp(left, right);
        return makeFPBranch(cond);
    }

    Jump branchFloatWithZero(DoubleCondition cond, FPRegisterID left)
    {
        m_assembler.vcmpz(asSingle(left));
        return makeFPBranch(cond);
    }

    Jump branchDouble(DoubleCondition cond, FPRegisterID left, FPRegisterID right)
    {
        m_assembler.vcmp(left, right);
        return makeFPBranch(cond);
    }

    Jump branchDoubleWithZero(DoubleCondition cond, FPRegisterID left)
    {
        m_assembler.vcmpz(left);
        return makeFPBranch(cond);
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

    void truncateFloatToInt32(FPRegisterID src, RegisterID dest)
    {
        m_assembler.vcvt_floatingPointToSigned(fpTempRegisterAsSingle(), asSingle(src));
        m_assembler.vmov(dest, fpTempRegisterAsSingle());
    }

    void truncateFloatToUint32(FPRegisterID src, RegisterID dest)
    {
        m_assembler.vcvt_floatingPointToUnsigned(fpTempRegisterAsSingle(), asSingle(src));
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
private:
    void convertDoubleToUint64(FPRegisterID src, RegisterID destLo, RegisterID destHi, FPRegisterID scratch1, FPRegisterID scratch2)
    {
        // We override src on the vmla, so we require a temp fp register here
        ASSERT(src == fpTempRegister);

        // Constant materialization
        move(TrustedImm32(0x00000000), destLo);
        move(TrustedImm32(0x3DF00000), destHi);
        move64ToDouble(destHi, destLo, scratch1);
        move(TrustedImm32(0x00000000), destLo);
        move(TrustedImm32(0xC1F00000), destHi);
        move64ToDouble(destHi, destLo, scratch2);

        m_assembler.vmul(scratch1, src, scratch1);

        m_assembler.vcvt_floatingPointToUnsigned(ARMRegisters::asSingle(scratch1), scratch1);
        m_assembler.vmov(destHi, ARMRegisters::asSingle(scratch1));

        m_assembler.vcvt_unsignedToFloatingPoint(scratch1, ARMRegisters::asSingle(scratch1));

        m_assembler.vmla(src, scratch1, scratch2);

        m_assembler.vcvt_floatingPointToUnsigned(ARMRegisters::asSingle(src), src);
        m_assembler.vmov(destLo, ARMRegisters::asSingle(src));
    }

public:
    void truncateDoubleToUint64(FPRegisterID src, RegisterID destLo, RegisterID destHi, FPRegisterID scratch1, FPRegisterID scratch2)
    {
        Jump notPositive = branchDoubleWithZero(DoubleLessThanOrEqualOrUnordered, src);

        moveDouble(src, fpTempRegister);
        convertDoubleToUint64(fpTempRegister, destLo, destHi, scratch1, scratch2);

        Jump done = jump();

        notPositive.link(this);
        move(TrustedImm32(0), destLo);
        move(TrustedImm32(0), destHi);

        done.link(this);
    }

    void truncateDoubleToInt64(FPRegisterID src, RegisterID destLo, RegisterID destHi, FPRegisterID scratch1, FPRegisterID scratch2)
    {
        RegisterID signFlag = getCachedAddressTempRegisterIDAndInvalidate();
        moveDouble(src, fpTempRegister);

        Jump isNegative = branchDoubleWithZero(DoubleLessThanAndOrdered, fpTempRegister);
        move(TrustedImm32(0), signFlag);
        Jump join = jump();

        isNegative.link(this);
        m_assembler.vneg(fpTempRegister, fpTempRegister);
        move(TrustedImm32(1), signFlag);

        join.link(this);
        convertDoubleToUint64(fpTempRegister, destLo, destHi, scratch1, scratch2);

        Jump wasNonNegative = branch32(Equal, signFlag, TrustedImm32(0));
        m_assembler.sub_S(destLo, ARMThumbImmediate::makeUInt12OrEncodedImm(0), destLo);
        m_assembler.mvn(destHi, destHi);
        m_assembler.adc(destHi, destHi, ARMThumbImmediate::makeEncodedImm(0));
        wasNonNegative.link(this);
    }

    void truncateFloatToUint64(FPRegisterID src, RegisterID destLo, RegisterID destHi, FPRegisterID scratch1, FPRegisterID scratch2)
    {
        convertFloatToDouble(src, fpTempRegister);
        truncateDoubleToUint64(fpTempRegister, destLo, destHi, scratch1, scratch2);
    }

    void truncateFloatToInt64(FPRegisterID src, RegisterID destLo, RegisterID destHi, FPRegisterID scratch1, FPRegisterID scratch2)
    {
        convertFloatToDouble(src, fpTempRegister);
        truncateDoubleToInt64(fpTempRegister, destLo, destHi, scratch1, scratch2);
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


    bool cachedRegisterGetValue(CachedTempRegister& cachedRegister, intptr_t &currentRegisterContents)
    {
        if (!m_allowScratchRegister)
            return false;
        return cachedRegister.value(currentRegisterContents);
    }

    void cachedRegisterSetValue(CachedTempRegister& cachedRegister, intptr_t value)
    {
        if (!m_allowScratchRegister)
            return;
        cachedRegister.setValue(value);
    }

    bool short_move(RegisterID dest, CachedTempRegister& cachedRegister, intptr_t valueAsInt)
    {
        intptr_t currentRegisterContents;
        if (cachedRegisterGetValue(cachedRegister, currentRegisterContents)) {
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
        else if (m_allowScratchRegister && (dest == addressTempRegister)) {
            if (!short_move(dest, cachedAddressTempRegister(), valueAsInt))
                long_move(imm, dest);
        } else if (m_allowScratchRegister && (dest == dataTempRegister)) {
            if (!short_move(dest, cachedDataTempRegister(), valueAsInt))
                long_move(imm, dest);
        } else {
            long_move(imm, dest);
        }

        if (dest == addressTempRegister)
            cachedRegisterSetValue(m_cachedAddressTempRegister, valueAsInt);
        else if (dest == dataTempRegister)
            cachedRegisterSetValue(m_cachedDataTempRegister, valueAsInt);
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

    void swapDouble(FPRegisterID fr1, FPRegisterID fr2)
    {
        if (fr1 == fr2)
            return;
        moveDouble(fr1, fpTempRegister);
        moveDouble(fr2, fr1);
        moveDouble(fpTempRegister, fr2);
    }

    void zeroExtend8To32(RegisterID src, RegisterID dest)
    {
        m_assembler.uxtb(dest, src);
    }

    void zeroExtend16To32(RegisterID src, RegisterID dest)
    {
        m_assembler.uxth(dest, src);
    }

    void signExtend8To32(RegisterID src, RegisterID dest)
    {
        m_assembler.sxtb(dest, src);
    }

    void signExtend16To32(RegisterID src, RegisterID dest)
    {
        m_assembler.sxth(dest, src);
    }

    void signExtend32ToPtr(RegisterID src, RegisterID dest)
    {
        move(src, dest);
    }

    void signExtend32ToPtr(TrustedImm32 imm, RegisterID dest)
    {
        move(imm, dest);
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

    static ResultCondition invert(ResultCondition cond)
    {
        return static_cast<ResultCondition>(cond ^ 1);
    }

    static std::optional<ResultCondition> commuteCompareToZeroIntoTest(RelationalCondition cond)
    {
        switch (cond) {
        case Equal:
            return Zero;
        case NotEqual:
            return NonZero;
        case LessThan:
            return Signed;
        case GreaterThanOrEqual:
            return PositiveOrZero;
        default:
            return std::nullopt;
        }
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

    void loadFence()
    {
        m_assembler.dmbISH();
    }

    template<PtrTag startTag, PtrTag destTag>
    static void replaceWithJump(CodeLocationLabel<startTag> instructionStart, CodeLocationLabel<destTag> destination)
    {
        ARMv7Assembler::replaceWithJump(instructionStart.dataLocation(), destination.dataLocation());
    }

    template<PtrTag startTag>
    static void replaceWithNops(CodeLocationLabel<startTag> instructionStart, size_t memoryToFillWithNopsInBytes)
    {
        ARMv7Assembler::replaceWithNops(instructionStart.dataLocation(), memoryToFillWithNopsInBytes);
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

    void compare32AndSetFlags(RegisterID left, RegisterID right)
    {
        m_assembler.cmp(left, right);
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
        if (left == ARMRegisters::sp && right == addressTempRegister && cond == Equal) {
            m_assembler.sub_S(addressTempRegister, left, addressTempRegister);
            return Jump(makeBranch(Zero));
        } else if (left == ARMRegisters::sp && right == addressTempRegister && cond == NotEqual) {
            m_assembler.sub_S(addressTempRegister, left, addressTempRegister);
            return Jump(makeBranch(NonZero));
        } else if (right == ARMRegisters::sp && left == addressTempRegister && cond == Equal) {
            m_assembler.sub_S(addressTempRegister, right, addressTempRegister);
            return Jump(makeBranch(Zero));
        } else if (right == ARMRegisters::sp && left == addressTempRegister && cond == NotEqual) {
            m_assembler.sub_S(addressTempRegister, right, addressTempRegister);
            return Jump(makeBranch(NonZero));
        } else if (left == ARMRegisters::sp) {
            ASSERT(right != addressTempRegister);
            move(left, addressTempRegister);
            m_assembler.cmp(addressTempRegister, right);
        } else if (right == ARMRegisters::sp) {
            ASSERT(left != addressTempRegister);
            move(right, addressTempRegister);
            m_assembler.cmp(left, addressTempRegister);
        } else
            m_assembler.cmp(left, right);
        return Jump(makeBranch(cond));
    }

    Jump branch32(RelationalCondition cond, RegisterID left, TrustedImm32 right)
    {
        if (!right.m_value) {
            if (auto resultCondition = commuteCompareToZeroIntoTest(cond))
                return branchTest32(*resultCondition, left, left);
        }

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

    Jump branch32WithMemory16(RelationalCondition cond, Address left, RegisterID right)
    {
        MacroAssemblerHelpers::load16OnCondition(*this, cond, left, addressTempRegister);
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

    Jump branch16(RelationalCondition cond, RegisterID left, TrustedImm32 right)
    {
        TrustedImm32 right16 = MacroAssemblerHelpers::mask16OnCondition(*this, cond, right);
        compare32AndSetFlags(left, right16);
        return Jump(makeBranch(cond));
    }

    Jump branch16(RelationalCondition cond, Address left, TrustedImm32 right)
    {
        // use addressTempRegister incase the branch16 we call uses dataTempRegister. :-/
        RegisterID scratch = getCachedAddressTempRegisterIDAndInvalidate();
        TrustedImm32 right16 = MacroAssemblerHelpers::mask16OnCondition(*this, cond, right);
        MacroAssemblerHelpers::load16OnCondition(*this, cond, left, scratch);
        return branch16(cond, scratch, right16);
    }

    Jump branch16(RelationalCondition cond, BaseIndex left, TrustedImm32 right)
    {
        // use addressTempRegister incase the branch32 we call uses dataTempRegister. :-/
        RegisterID scratch = getCachedAddressTempRegisterIDAndInvalidate();
        TrustedImm32 right16 = MacroAssemblerHelpers::mask16OnCondition(*this, cond, right);
        MacroAssemblerHelpers::load16OnCondition(*this, cond, left, scratch);
        return branch32(cond, scratch, right16);
    }

    Jump branch16(RelationalCondition cond, AbsoluteAddress address, TrustedImm32 right)
    {
        // Use addressTempRegister instead of dataTempRegister, since branch32 uses dataTempRegister.
        TrustedImm32 right16 = MacroAssemblerHelpers::mask16OnCondition(*this, cond, right);
        ArmAddress armAddress = setupArmAddress(address);
        MacroAssemblerHelpers::load16OnCondition(*this, cond, armAddress, addressTempRegister);
        return branch32(cond, addressTempRegister, right16);
    }

private:
    template<typename T>
    Jump branch64Impl(RelationalCondition cond, RegisterID leftHi, RegisterID leftLo, T rightHi, T rightLo)
    {
        if (cond == Equal) {
            // Equal: bne done; cmp lo; beq target; done:
            compare32AndSetFlags(leftHi, rightHi);
            Jump done = makeBranch(ARMv7Assembler::ConditionNE);
            compare32AndSetFlags(leftLo, rightLo);
            Jump result = makeBranch(ARMv7Assembler::ConditionEQ);
            done.link(this);
            return result;
        }

        if (cond == NotEqual) {
            // NotEqual: branch taken if ANY part differs
            compare32AndSetFlags(leftHi, rightHi);
            Jump fromHi = makeBranch(ARMv7Assembler::ConditionNE);
            compare32AndSetFlags(leftLo, rightLo);
            Jump fromLo = makeBranch(ARMv7Assembler::ConditionNE);
            Jump notTaken = jump();
            fromHi.link(this);
            fromLo.link(this);
            Jump result = jump();
            notTaken.link(this);
            return result;
        }

        ARMv7Assembler::Condition hiCond = armV7ConditionForHigh32(cond);
        ARMv7Assembler::Condition loCond = armV7ConditionForLow32(cond);
        ARMv7Assembler::Condition inverseLoCond = ARMv7Assembler::invert(loCond);
        compare32AndSetFlags(leftHi, rightHi);
        Jump fromHi = makeBranch(hiCond);
        Jump notTakenHi = makeBranch(ARMv7Assembler::ConditionNE);
        compare32AndSetFlags(leftLo, rightLo);
        Jump notTakenLo = makeBranch(inverseLoCond);
        fromHi.link(this);
        Jump result = jump();
        notTakenHi.link(this);
        notTakenLo.link(this);
        return result;
    }

public:
    Jump branch64(RelationalCondition cond, RegisterID leftHi, RegisterID leftLo, RegisterID rightHi, RegisterID rightLo)
    {
        return branch64Impl(cond, leftHi, leftLo, rightHi, rightLo);
    }

    Jump branch64(RelationalCondition cond, RegisterID leftHi, RegisterID leftLo, TrustedImm64 right)
    {
        TrustedImm32 rightHi(static_cast<int32_t>(right.m_value >> 32));
        TrustedImm32 rightLo(static_cast<int32_t>(right.m_value));

        // Optimize for comparing with zero (unsigned comparisons only)
        if (!rightHi.m_value && !rightLo.m_value) {
            if (cond == Below)
                return Jump(); // no unsigned value is < 0
            if (cond == AboveOrEqual)
                return jump(); // all unsigned values are >= 0

            if (cond == Equal || cond == BelowOrEqual || cond == NotEqual || cond == Above) {
                RegisterID scratch = getCachedDataTempRegisterIDAndInvalidate();
                m_assembler.orr_S(scratch, leftHi, leftLo);
                return Jump(makeBranch((cond == Equal || cond == BelowOrEqual) ? ARMv7Assembler::ConditionEQ : ARMv7Assembler::ConditionNE));
            }
        }

        return branch64Impl(cond, leftHi, leftLo, rightHi, rightLo);
    }

    Jump branchTest64(ResultCondition cond, RegisterID regHi, RegisterID regLo)
    {
        if (cond == Signed || cond == PositiveOrZero) {
            // For sign tests, only check the sign bit of the high 32 bits
            m_assembler.tst(regHi, regHi);
            return Jump(makeBranch((cond == Signed) ? ARMv7Assembler::ConditionMI : ARMv7Assembler::ConditionPL));
        }

        ASSERT(cond == Zero || cond == NonZero);
        RegisterID scratch = getCachedDataTempRegisterIDAndInvalidate();
        m_assembler.orr_S(scratch, regHi, regLo);
        return Jump(makeBranch(cond == Zero ? ARMv7Assembler::ConditionEQ : ARMv7Assembler::ConditionNE));
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
        m_assembler.udf(imm);
    }

    void setCarry(RegisterID dest)
    {
        m_assembler.it(ARMv7Assembler::ConditionCS, false);
        move(TrustedImm32(1), dest);
        move(TrustedImm32(0), dest);
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

    ALWAYS_INLINE void padBeforePatch()
    {
        (void)label();
        m_assembler.alignWithNop(sizeof(uint64_t));
    }

    ALWAYS_INLINE Call threadSafePatchableNearCall()
    {
        invalidateAllTempRegisters();
        padBeforePatch();
        m_assembler.bl();
        return Call(m_assembler.labelIgnoringWatchpoints(), Call::LinkableNear);
    }

    ALWAYS_INLINE Call threadSafePatchableNearTailCall()
    {
        invalidateAllTempRegisters();
        padBeforePatch();
        return Call(m_assembler.b(), Call::LinkableNearTail);
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

    template<PtrTag tag>
    ALWAYS_INLINE void callOperation(const CodePtr<tag> operation)
    {
        move(TrustedImmPtr(operation.taggedPtr()), addressTempRegister);
        call(addressTempRegister, tag);
    }

    ALWAYS_INLINE void ret()
    {
        m_assembler.bx(linkRegister);
    }

    void compare32(ARMv7Assembler::Condition cond, RegisterID left, RegisterID right, RegisterID dest)
    {
        m_assembler.cmp(left, right);
        m_assembler.it(cond, false);
        m_assembler.mov(dest, ARMThumbImmediate::makeUInt16(1));
        m_assembler.mov(dest, ARMThumbImmediate::makeUInt16(0));
    }

    void compare32(RelationalCondition cond, RegisterID left, RegisterID right, RegisterID dest)
    {
        compare32(armV7Condition(cond), left, right, dest);
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
        if (!right.m_value) {
            if (auto resultCondition = commuteCompareToZeroIntoTest(cond)) {
                test32(*resultCondition, left, left, dest);
                return;
            }
        }

        compare32AndSetFlags(left, right);
        m_assembler.it(armV7Condition(cond), false);
        m_assembler.mov(dest, ARMThumbImmediate::makeUInt16(1));
        m_assembler.mov(dest, ARMThumbImmediate::makeUInt16(0));
    }

    void compareFloat(DoubleCondition cond, FPRegisterID left, FPRegisterID right, RegisterID dest)
    {
        if ((cond == DoubleNotEqualAndOrdered) || (cond == DoubleEqualOrUnordered)) {
            move(TrustedImm32(1), dest);
            Jump trueCase = branchFloat(cond, left, right);
            move(TrustedImm32(0), dest);
            trueCase.link(this);
            return;
        }
        m_assembler.vcmp(asSingle(left), asSingle(right));
        m_assembler.vmrs();
        m_assembler.it(armV7Condition(cond), false);
        m_assembler.mov(dest, ARMThumbImmediate::makeUInt16(1));
        m_assembler.mov(dest, ARMThumbImmediate::makeUInt16(0));
    }

    void compareFloatWithZero(DoubleCondition cond, FPRegisterID left, RegisterID dest)
    {
        UNUSED_PARAM(cond);
        UNUSED_PARAM(left);
        UNUSED_PARAM(dest);
        UNREACHABLE_FOR_PLATFORM();
    }

    void compareDouble(DoubleCondition cond, FPRegisterID left, FPRegisterID right, RegisterID dest)
    {
        if ((cond == DoubleNotEqualAndOrdered) || (cond == DoubleEqualOrUnordered)) {
            move(TrustedImm32(1), dest);
            Jump trueCase = branchDouble(cond, left, right);
            move(TrustedImm32(0), dest);
            trueCase.link(this);
            return;
        }
        m_assembler.vcmp(left, right);
        m_assembler.vmrs();
        m_assembler.it(armV7Condition(cond), false);
        m_assembler.mov(dest, ARMThumbImmediate::makeUInt16(1));
        m_assembler.mov(dest, ARMThumbImmediate::makeUInt16(0));
    }

    void test32(ResultCondition cond, RegisterID op1, RegisterID op2, RegisterID dest)
    {
        switch (cond) {
        case Zero:
            if (op1 == op2) {
                m_assembler.clz(dest, op1);
                m_assembler.lsr(dest, dest, 5);
            } else {
                m_assembler.eor(dest, op1, op2);
                m_assembler.clz(dest, dest);
                m_assembler.lsr(dest, dest, 5);
            }
            return;

        case NonZero:
            if (op1 == op2) {
                m_assembler.clz(dest, op1);
                m_assembler.lsr(dest, dest, 5);
                m_assembler.eor(dest, dest, ARMThumbImmediate::makeEncodedImm(1));
            } else {
                m_assembler.eor(dest, op1, op2);
                m_assembler.clz(dest, dest);
                m_assembler.lsr(dest, dest, 5);
                m_assembler.eor(dest, dest, ARMThumbImmediate::makeEncodedImm(1));
            }
            return;

        case Signed:
            if (op1 == op2)
                m_assembler.lsr(dest, op1, 31);
            else {
                m_assembler.ARM_and(dest, op1, op2);
                m_assembler.lsr(dest, dest, 31);
            }
            return;

        case PositiveOrZero:
            if (op1 == op2) {
                m_assembler.lsr(dest, op1, 31);
                m_assembler.eor(dest, dest, ARMThumbImmediate::makeEncodedImm(1));
            } else {
                m_assembler.ARM_and(dest, op1, op2);
                m_assembler.lsr(dest, dest, 31);
                m_assembler.eor(dest, dest, ARMThumbImmediate::makeEncodedImm(1));
            }
            return;

        default:
            // tst instruction doesn't set Carry or Overflow flags
            RELEASE_ASSERT_NOT_REACHED();
            break;
        }
    }

    void test32(ResultCondition cond, RegisterID op1, TrustedImm32 mask, RegisterID dest)
    {
        // Special case: mask == -1 is equivalent to testing register against itself
        if (mask.m_value == -1) {
            test32(cond, op1, op1, dest);
            return;
        }

        // Special case: mask == 0x80000000 is sign bit testing
        if (mask.m_value == static_cast<int32_t>(0x80000000)) {
            switch (cond) {
            case NonZero:
            case Signed:
                m_assembler.lsr(dest, op1, 31);
                return;
            case Zero:
            case PositiveOrZero:
                m_assembler.lsr(dest, op1, 31);
                m_assembler.eor(dest, dest, ARMThumbImmediate::makeEncodedImm(1));
                return;
            default:
                // tst instruction doesn't set Carry or Overflow flag
                RELEASE_ASSERT_NOT_REACHED();
            }
        }

        switch (cond) {
        case Zero: {
            ARMThumbImmediate armImm = ARMThumbImmediate::makeEncodedImm(mask.m_value);
            if (armImm.isValid())
                m_assembler.ARM_and(dest, op1, armImm);
            else {
                move(mask, dest);
                m_assembler.ARM_and(dest, op1, dest);
            }
            m_assembler.clz(dest, dest);
            m_assembler.lsr(dest, dest, 5);
            return;
        }
        case NonZero: {
            ARMThumbImmediate armImm = ARMThumbImmediate::makeEncodedImm(mask.m_value);
            if (armImm.isValid())
                m_assembler.ARM_and(dest, op1, armImm);
            else {
                move(mask, dest);
                m_assembler.ARM_and(dest, op1, dest);
            }
            m_assembler.clz(dest, dest);
            m_assembler.lsr(dest, dest, 5);
            m_assembler.eor(dest, dest, ARMThumbImmediate::makeEncodedImm(1));
            return;
        }
        case Signed: {
            ARMThumbImmediate armImm = ARMThumbImmediate::makeEncodedImm(mask.m_value);
            if (armImm.isValid())
                m_assembler.ARM_and(dest, op1, armImm);
            else {
                move(mask, dest);
                m_assembler.ARM_and(dest, op1, dest);
            }
            m_assembler.lsr(dest, dest, 31);
            return;
        }
        case PositiveOrZero: {
            ARMThumbImmediate armImm = ARMThumbImmediate::makeEncodedImm(mask.m_value);
            if (armImm.isValid())
                m_assembler.ARM_and(dest, op1, armImm);
            else {
                move(mask, dest);
                m_assembler.ARM_and(dest, op1, dest);
            }
            m_assembler.lsr(dest, dest, 31);
            m_assembler.eor(dest, dest, ARMThumbImmediate::makeEncodedImm(1));
            return;
        }
        default:
            // tst instruction doesn't set Carry or Overflow flag
            RELEASE_ASSERT_NOT_REACHED();
        }
    }

    void test32(ResultCondition cond, Address address, TrustedImm32 mask, RegisterID dest)
    {
        load32(address, addressTempRegister);
        test32(cond, addressTempRegister, mask, dest);
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

    void moveConditionally32(RelationalCondition cond, RegisterID left, RegisterID right, RegisterID src, RegisterID dest)
    {
        if (src == dest)
            return;

        m_assembler.cmp(left, right);
        m_assembler.it(armV7Condition(cond));
        move(src, dest);
    }

    void moveConditionally32(RelationalCondition cond, RegisterID left, RegisterID right, RegisterID thenCase, RegisterID elseCase, RegisterID dest)
    {
        if (thenCase == elseCase) {
            move(thenCase, dest);
            return;
        }

        m_assembler.cmp(left, right);
        if (thenCase == dest) {
            m_assembler.it(armV7Condition(invert(cond)));
            move(elseCase, dest);
        } else if (elseCase == dest) {
            m_assembler.it(armV7Condition(cond));
            move(thenCase, dest);
        } else {
            m_assembler.it(armV7Condition(cond), false);
            move(thenCase, dest);
            move(elseCase, dest);
        }
    }

    void moveConditionally32(RelationalCondition cond, RegisterID left, TrustedImm32 right, RegisterID thenCase, RegisterID elseCase, RegisterID dest)
    {
        if (thenCase == elseCase) {
            move(thenCase, dest);
            return;
        }

        if (!right.m_value) {
            if (auto resultCondition = commuteCompareToZeroIntoTest(cond)) {
                moveConditionallyTest32(*resultCondition, left, left, thenCase, elseCase, dest);
                return;
            }
        }

        compare32AndSetFlags(left, right);
        if (thenCase == dest) {
            m_assembler.it(armV7Condition(invert(cond)));
            move(elseCase, dest);
        } else if (elseCase == dest) {
            m_assembler.it(armV7Condition(cond));
            move(thenCase, dest);
        } else {
            m_assembler.it(armV7Condition(cond), false);
            move(thenCase, dest);
            move(elseCase, dest);
        }
    }

    void moveConditionallyTest32(ResultCondition cond, RegisterID testReg, RegisterID mask, RegisterID src, RegisterID dest)
    {
        if (src == dest)
            return;
        m_assembler.tst(testReg, mask);
        m_assembler.it(armV7Condition(cond));
        move(src, dest);
    }

    void moveConditionallyTest32(ResultCondition cond, RegisterID left, RegisterID right, RegisterID thenCase, RegisterID elseCase, RegisterID dest)
    {
        if (thenCase == elseCase) {
            move(thenCase, dest);
            return;
        }

        m_assembler.tst(left, right);
        if (thenCase == dest) {
            m_assembler.it(armV7Condition(invert(cond)));
            move(elseCase, dest);
        } else if (elseCase == dest) {
            m_assembler.it(armV7Condition(cond));
            move(thenCase, dest);
        } else {
            m_assembler.it(armV7Condition(cond), false);
            move(thenCase, dest);
            move(elseCase, dest);
        }
    }

    void moveConditionallyTest32(ResultCondition cond, RegisterID left, TrustedImm32 right, RegisterID thenCase, RegisterID elseCase, RegisterID dest)
    {
        // easy case: both assignments are the same, so we just ignore the condition
        if (thenCase == elseCase) {
            move(thenCase, dest);
            return;
        }

        test32(left, right);
        if (thenCase == dest) {
            m_assembler.it(armV7Condition(invert(cond)));
            move(elseCase, dest);
        } else if (elseCase == dest) {
            m_assembler.it(armV7Condition(cond));
            move(thenCase, dest);
        } else {
            m_assembler.it(armV7Condition(cond), false);
            move(thenCase, dest);
            move(elseCase, dest);
        }
    }

    void moveDoubleConditionally32(RelationalCondition cond, RegisterID left, RegisterID right, FPRegisterID thenCase, FPRegisterID elseCase, FPRegisterID dest)
    {
        if (thenCase == elseCase) {
            moveDouble(thenCase, dest);
            return;
        }

        m_assembler.cmp(left, right);
        if (thenCase == dest) {
            m_assembler.it(armV7Condition(invert(cond)));
            moveDouble(elseCase, dest);
        } else if (elseCase == dest) {
            m_assembler.it(armV7Condition(cond));
            moveDouble(thenCase, dest);
        } else {
            m_assembler.it(armV7Condition(cond), false);
            moveDouble(thenCase, dest);
            moveDouble(elseCase, dest);
        }
    }

    void moveDoubleConditionally32(RelationalCondition cond, RegisterID left, TrustedImm32 right, FPRegisterID thenCase, FPRegisterID elseCase, FPRegisterID dest)
    {
        if (thenCase == elseCase) {
            moveDouble(thenCase, dest);
            return;
        }

        if (!right.m_value) {
            if (auto resultCondition = commuteCompareToZeroIntoTest(cond)) {
                moveDoubleConditionallyTest32(*resultCondition, left, left, thenCase, elseCase, dest);
                return;
            }
        }

        compare32AndSetFlags(left, right);
        if (thenCase == dest) {
            m_assembler.it(armV7Condition(invert(cond)));
            moveDouble(elseCase, dest);
        } else if (elseCase == dest) {
            m_assembler.it(armV7Condition(cond));
            moveDouble(thenCase, dest);
        } else {
            m_assembler.it(armV7Condition(cond), false);
            moveDouble(thenCase, dest);
            moveDouble(elseCase, dest);
        }
    }

    void moveDoubleConditionallyTest32(ResultCondition cond, RegisterID left, RegisterID right, FPRegisterID thenCase, FPRegisterID elseCase, FPRegisterID dest)
    {
        if (thenCase == elseCase) {
            moveDouble(thenCase, dest);
            return;
        }

        m_assembler.tst(left, right);
        if (thenCase == dest) {
            m_assembler.it(armV7Condition(invert(cond)));
            moveDouble(elseCase, dest);
        } else if (elseCase == dest) {
            m_assembler.it(armV7Condition(cond));
            moveDouble(thenCase, dest);
        } else {
            m_assembler.it(armV7Condition(cond), false);
            moveDouble(thenCase, dest);
            moveDouble(elseCase, dest);
        }
    }

    void moveDoubleConditionallyTest32(ResultCondition cond, RegisterID left, TrustedImm32 right, FPRegisterID thenCase, FPRegisterID elseCase, FPRegisterID dest)
    {
        if (thenCase == elseCase) {
            moveDouble(thenCase, dest);
            return;
        }

        test32(left, right);
        if (thenCase == dest) {
            m_assembler.it(armV7Condition(invert(cond)));
            moveDouble(elseCase, dest);
        } else if (elseCase == dest) {
            m_assembler.it(armV7Condition(cond));
            moveDouble(thenCase, dest);
        } else {
            m_assembler.it(armV7Condition(cond), false);
            moveDouble(thenCase, dest);
            moveDouble(elseCase, dest);
        }
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

    PatchableJump patchableBranch16(RelationalCondition cond, Address left, TrustedImm32 imm)
    {
        m_makeJumpPatchable = true;
        Jump result = branch16(cond, left, imm);
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

    void convertDoubleToFloat16(FPRegisterID src, FPRegisterID dest)
    {
        UNUSED_PARAM(src);
        UNUSED_PARAM(dest);
        UNREACHABLE_FOR_PLATFORM();
    }

    void convertFloat16ToDouble(FPRegisterID src, FPRegisterID dest)
    {
        UNUSED_PARAM(src);
        UNUSED_PARAM(dest);
        UNREACHABLE_FOR_PLATFORM();
    }

    void loadFloat16(Address address, FPRegisterID dest)
    {
        UNUSED_PARAM(address);
        UNUSED_PARAM(dest);
        UNREACHABLE_FOR_PLATFORM();
    }

    void loadFloat16(BaseIndex address, FPRegisterID dest)
    {
        UNUSED_PARAM(address);
        UNUSED_PARAM(dest);
        UNREACHABLE_FOR_PLATFORM();
    }

    void loadFloat16(TrustedImmPtr address, FPRegisterID dest)
    {
        UNUSED_PARAM(address);
        UNUSED_PARAM(dest);
        UNREACHABLE_FOR_PLATFORM();
    }

    void moveZeroToFloat16(FPRegisterID reg)
    {
        UNUSED_PARAM(reg);
        UNREACHABLE_FOR_PLATFORM();
    }

    void move16ToFloat16(RegisterID src, FPRegisterID dest)
    {
        UNUSED_PARAM(src);
        UNUSED_PARAM(dest);
        UNREACHABLE_FOR_PLATFORM();
    }

    void move16ToFloat16(TrustedImm32 imm, FPRegisterID dest)
    {
        UNUSED_PARAM(imm);
        UNUSED_PARAM(dest);
        UNREACHABLE_FOR_PLATFORM();
    }

    void moveFloat16To16(FPRegisterID src, RegisterID dest)
    {
        UNUSED_PARAM(src);
        UNUSED_PARAM(dest);
        UNREACHABLE_FOR_PLATFORM();
    }

    void storeFloat16(FPRegisterID src, Address address)
    {
        UNUSED_PARAM(src);
        UNUSED_PARAM(address);
        UNREACHABLE_FOR_PLATFORM();
    }

    void storeFloat16(FPRegisterID src, BaseIndex address)
    {
        UNUSED_PARAM(src);
        UNUSED_PARAM(address);
        UNREACHABLE_FOR_PLATFORM();
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
                RELEASE_ASSERT(m_allowScratchRegister);
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

        RELEASE_ASSERT(m_allowScratchRegister);
        move(TrustedImm32(address.offset), addressTempRegister);
        return ArmAddress(address.base, addressTempRegister);
    }

    template <class Bounds>
    std::optional<int32_t> absoluteAddressWithinShortOffset(AbsoluteAddress address, CachedTempRegister &cachedRegister)
    {
        intptr_t addressAsInt = reinterpret_cast<uintptr_t>(address.m_ptr);
        intptr_t currentRegisterContents;
        if (cachedRegisterGetValue(cachedRegister, currentRegisterContents)) {
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
            cachedRegisterSetValue(cachedDataTempRegister(), valueAsInt);
        else if (dst == addressTempRegister)
            cachedRegisterSetValue(cachedAddressTempRegister(), valueAsInt);
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

    bool m_makeJumpPatchable;
    CachedTempRegister m_cachedDataTempRegister;
    CachedTempRegister m_cachedAddressTempRegister;
};

} // namespace JSC

#endif // ENABLE(ASSEMBLER) && CPU(ARM_THUMB2)
