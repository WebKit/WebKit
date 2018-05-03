/*
 * Copyright (C) 2008-2018 Apple Inc.
 * Copyright (C) 2009, 2010 University of Szeged
 * All rights reserved.
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

#if ENABLE(ASSEMBLER) && CPU(ARM_TRADITIONAL)

#include "ARMAssembler.h"
#include "AbstractMacroAssembler.h"

namespace JSC {

using Assembler = TARGET_ASSEMBLER;

class MacroAssemblerARM : public AbstractMacroAssembler<Assembler> {
    static const int DoubleConditionMask = 0x0f;
    static const int DoubleConditionBitSpecial = 0x10;
    COMPILE_ASSERT(!(DoubleConditionBitSpecial & DoubleConditionMask), DoubleConditionBitSpecial_should_not_interfere_with_ARMAssembler_Condition_codes);
public:
    static const unsigned numGPRs = 16;
    static const unsigned numFPRs = 16;
    
    typedef ARMRegisters::FPRegisterID FPRegisterID;

    static const RegisterID dataTempRegister = ARMRegisters::ip;
    static const RegisterID addressTempRegister = ARMRegisters::r6;

    static const ARMRegisters::FPRegisterID fpTempRegister = ARMRegisters::d7;

    enum RelationalCondition {
        Equal = ARMAssembler::EQ,
        NotEqual = ARMAssembler::NE,
        Above = ARMAssembler::HI,
        AboveOrEqual = ARMAssembler::CS,
        Below = ARMAssembler::CC,
        BelowOrEqual = ARMAssembler::LS,
        GreaterThan = ARMAssembler::GT,
        GreaterThanOrEqual = ARMAssembler::GE,
        LessThan = ARMAssembler::LT,
        LessThanOrEqual = ARMAssembler::LE
    };

    enum ResultCondition {
        Overflow = ARMAssembler::VS,
        Signed = ARMAssembler::MI,
        PositiveOrZero = ARMAssembler::PL,
        Zero = ARMAssembler::EQ,
        NonZero = ARMAssembler::NE
    };

    enum DoubleCondition {
        // These conditions will only evaluate to true if the comparison is ordered - i.e. neither operand is NaN.
        DoubleEqual = ARMAssembler::EQ,
        DoubleNotEqual = ARMAssembler::NE | DoubleConditionBitSpecial,
        DoubleGreaterThan = ARMAssembler::GT,
        DoubleGreaterThanOrEqual = ARMAssembler::GE,
        DoubleLessThan = ARMAssembler::CC,
        DoubleLessThanOrEqual = ARMAssembler::LS,
        // If either operand is NaN, these conditions always evaluate to true.
        DoubleEqualOrUnordered = ARMAssembler::EQ | DoubleConditionBitSpecial,
        DoubleNotEqualOrUnordered = ARMAssembler::NE,
        DoubleGreaterThanOrUnordered = ARMAssembler::HI,
        DoubleGreaterThanOrEqualOrUnordered = ARMAssembler::CS,
        DoubleLessThanOrUnordered = ARMAssembler::LT,
        DoubleLessThanOrEqualOrUnordered = ARMAssembler::LE,
    };

    static const RegisterID stackPointerRegister = ARMRegisters::sp;
    static const RegisterID framePointerRegister = ARMRegisters::fp;
    static const RegisterID linkRegister = ARMRegisters::lr;

    static const Scale ScalePtr = TimesFour;

    void add32(RegisterID src, RegisterID dest)
    {
        m_assembler.adds(dest, dest, src);
    }

    void add32(RegisterID op1, RegisterID op2, RegisterID dest)
    {
        m_assembler.adds(dest, op1, op2);
    }

    void add32(TrustedImm32 imm, Address address)
    {
        load32(address, dataTempRegister);
        add32(imm, dataTempRegister);
        store32(dataTempRegister, address);
    }

    void add32(TrustedImm32 imm, RegisterID dest)
    {
        m_assembler.adds(dest, dest, m_assembler.getImm(imm.m_value, addressTempRegister));
    }

    void add32(AbsoluteAddress src, RegisterID dest)
    {
        move(TrustedImmPtr(src.m_ptr), dataTempRegister);
        m_assembler.dtrUp(ARMAssembler::LoadUint32, dataTempRegister, dataTempRegister, 0);
        add32(dataTempRegister, dest);
    }

    void add32(Address src, RegisterID dest)
    {
        load32(src, dataTempRegister);
        add32(dataTempRegister, dest);
    }

    void add32(RegisterID src, TrustedImm32 imm, RegisterID dest)
    {
        m_assembler.adds(dest, src, m_assembler.getImm(imm.m_value, addressTempRegister));
    }

    void getEffectiveAddress(BaseIndex address, RegisterID dest)
    {
        m_assembler.add(dest, address.base, m_assembler.lsl(address.index, static_cast<int>(address.scale)));
        if (address.offset)
            add32(TrustedImm32(address.offset), dest);
    }

    void and32(RegisterID src, RegisterID dest)
    {
        m_assembler.bitAnds(dest, dest, src);
    }

    void and32(RegisterID op1, RegisterID op2, RegisterID dest)
    {
        m_assembler.bitAnds(dest, op1, op2);
    }

    void and32(TrustedImm32 imm, RegisterID dest)
    {
        ARMWord w = m_assembler.getImm(imm.m_value, addressTempRegister, true);
        if (w & ARMAssembler::Op2InvertedImmediate)
            m_assembler.bics(dest, dest, w & ~ARMAssembler::Op2InvertedImmediate);
        else
            m_assembler.bitAnds(dest, dest, w);
    }

    void and32(TrustedImm32 imm, RegisterID src, RegisterID dest)
    {
        ARMWord w = m_assembler.getImm(imm.m_value, addressTempRegister, true);
        if (w & ARMAssembler::Op2InvertedImmediate)
            m_assembler.bics(dest, src, w & ~ARMAssembler::Op2InvertedImmediate);
        else
            m_assembler.bitAnds(dest, src, w);
    }

    void and32(Address src, RegisterID dest)
    {
        load32(src, dataTempRegister);
        and32(dataTempRegister, dest);
    }

    void and16(Address src, RegisterID dest)
    {
        load16(src, dataTempRegister);
        and32(dataTempRegister, dest);
    }

    void lshift32(RegisterID shiftAmount, RegisterID dest)
    {
        lshift32(dest, shiftAmount, dest);
    }

    void lshift32(RegisterID src, RegisterID shiftAmount, RegisterID dest)
    {
        ARMWord w = ARMAssembler::getOp2Byte(0x1f);
        m_assembler.bitAnd(addressTempRegister, shiftAmount, w);

        m_assembler.movs(dest, m_assembler.lslRegister(src, addressTempRegister));
    }

    void lshift32(TrustedImm32 imm, RegisterID dest)
    {
        m_assembler.movs(dest, m_assembler.lsl(dest, imm.m_value & 0x1f));
    }

    void lshift32(RegisterID src, TrustedImm32 imm, RegisterID dest)
    {
        m_assembler.movs(dest, m_assembler.lsl(src, imm.m_value & 0x1f));
    }

    void mul32(RegisterID op1, RegisterID op2, RegisterID dest)
    {
        if (op2 == dest) {
            if (op1 == dest) {
                move(op2, addressTempRegister);
                op2 = addressTempRegister;
            } else {
                // Swap the operands.
                RegisterID tmp = op1;
                op1 = op2;
                op2 = tmp;
            }
        }
        m_assembler.muls(dest, op1, op2);
    }

    void mul32(RegisterID src, RegisterID dest)
    {
        mul32(src, dest, dest);
    }

    void mul32(TrustedImm32 imm, RegisterID src, RegisterID dest)
    {
        move(imm, addressTempRegister);
        m_assembler.muls(dest, src, addressTempRegister);
    }

    void neg32(RegisterID srcDest)
    {
        m_assembler.rsbs(srcDest, srcDest, ARMAssembler::getOp2Byte(0));
    }

    void neg32(RegisterID src, RegisterID dest)
    {
        m_assembler.rsbs(dest, src, ARMAssembler::getOp2Byte(0));
    }

    void or32(RegisterID src, RegisterID dest)
    {
        m_assembler.orrs(dest, dest, src);
    }

    void or32(RegisterID src, AbsoluteAddress dest)
    {
        move(TrustedImmPtr(dest.m_ptr), addressTempRegister);
        load32(Address(addressTempRegister), dataTempRegister);
        or32(src, dataTempRegister);
        store32(dataTempRegister, addressTempRegister);
    }

    void or32(TrustedImm32 imm, AbsoluteAddress dest)
    {
        move(TrustedImmPtr(dest.m_ptr), addressTempRegister);
        load32(Address(addressTempRegister), dataTempRegister);
        or32(imm, dataTempRegister); // It uses S0 as temporary register, we need to reload the address.
        move(TrustedImmPtr(dest.m_ptr), addressTempRegister);
        store32(dataTempRegister, addressTempRegister);
    }

    void or32(TrustedImm32 imm, Address address)
    {
        load32(address, addressTempRegister);
        or32(imm, addressTempRegister, addressTempRegister);
        store32(addressTempRegister, address);
    }

    void or32(TrustedImm32 imm, RegisterID dest)
    {
        ASSERT(dest != addressTempRegister);
        m_assembler.orrs(dest, dest, m_assembler.getImm(imm.m_value, addressTempRegister));
    }

    void or32(TrustedImm32 imm, RegisterID src, RegisterID dest)
    {
        ASSERT(src != addressTempRegister);
        m_assembler.orrs(dest, src, m_assembler.getImm(imm.m_value, addressTempRegister));
    }

    void or32(RegisterID op1, RegisterID op2, RegisterID dest)
    {
        m_assembler.orrs(dest, op1, op2);
    }

    void rshift32(RegisterID shiftAmount, RegisterID dest)
    {
        rshift32(dest, shiftAmount, dest);
    }

    void rshift32(RegisterID src, RegisterID shiftAmount, RegisterID dest)
    {
        ARMWord w = ARMAssembler::getOp2Byte(0x1f);
        m_assembler.bitAnd(addressTempRegister, shiftAmount, w);

        m_assembler.movs(dest, m_assembler.asrRegister(src, addressTempRegister));
    }

    void rshift32(TrustedImm32 imm, RegisterID dest)
    {
        rshift32(dest, imm, dest);
    }

    void rshift32(RegisterID src, TrustedImm32 imm, RegisterID dest)
    {
        if (!imm.m_value)
            move(src, dest);
        else
            m_assembler.movs(dest, m_assembler.asr(src, imm.m_value & 0x1f));
    }

    void urshift32(RegisterID shiftAmount, RegisterID dest)
    {
        urshift32(dest, shiftAmount, dest);
    }

    void urshift32(RegisterID src, RegisterID shiftAmount, RegisterID dest)
    {
        ARMWord w = ARMAssembler::getOp2Byte(0x1f);
        m_assembler.bitAnd(addressTempRegister, shiftAmount, w);

        m_assembler.movs(dest, m_assembler.lsrRegister(src, addressTempRegister));
    }

    void urshift32(TrustedImm32 imm, RegisterID dest)
    {
        m_assembler.movs(dest, m_assembler.lsr(dest, imm.m_value & 0x1f));
    }
    
    void urshift32(RegisterID src, TrustedImm32 imm, RegisterID dest)
    {
        if (!imm.m_value)
            move(src, dest);
        else
            m_assembler.movs(dest, m_assembler.lsr(src, imm.m_value & 0x1f));
    }

    void sub32(RegisterID src, RegisterID dest)
    {
        m_assembler.subs(dest, dest, src);
    }

    void sub32(RegisterID left, RegisterID right, RegisterID dest)
    {
        m_assembler.subs(dest, left, right);
    }

    void sub32(TrustedImm32 imm, RegisterID dest)
    {
        m_assembler.subs(dest, dest, m_assembler.getImm(imm.m_value, addressTempRegister));
    }

    void sub32(TrustedImm32 imm, Address address)
    {
        load32(address, dataTempRegister);
        sub32(imm, dataTempRegister);
        store32(dataTempRegister, address);
    }

    void sub32(Address src, RegisterID dest)
    {
        load32(src, dataTempRegister);
        sub32(dataTempRegister, dest);
    }

    void sub32(RegisterID src, TrustedImm32 imm, RegisterID dest)
    {
        m_assembler.subs(dest, src, m_assembler.getImm(imm.m_value, addressTempRegister));
    }

    void xor32(RegisterID src, RegisterID dest)
    {
        m_assembler.eors(dest, dest, src);
    }

    void xor32(RegisterID op1, RegisterID op2, RegisterID dest)
    {
        m_assembler.eors(dest, op1, op2);
    }

    void xor32(Address src, RegisterID dest)
    {
        load32(src, dataTempRegister);
        xor32(dataTempRegister, dest);
    }

    void xor32(TrustedImm32 imm, RegisterID dest)
    {
        if (imm.m_value == -1)
            m_assembler.mvns(dest, dest);
        else
            m_assembler.eors(dest, dest, m_assembler.getImm(imm.m_value, addressTempRegister));
    }

    void xor32(TrustedImm32 imm, RegisterID src, RegisterID dest)
    {
        if (imm.m_value == -1)
            m_assembler.mvns(dest, src);
        else    
            m_assembler.eors(dest, src, m_assembler.getImm(imm.m_value, addressTempRegister));
    }

    void countLeadingZeros32(RegisterID src, RegisterID dest)
    {
#if WTF_ARM_ARCH_AT_LEAST(5)
        m_assembler.clz(dest, src);
#else
        UNUSED_PARAM(src);
        UNUSED_PARAM(dest);
        RELEASE_ASSERT_NOT_REACHED();
#endif
    }

    void load8(ImplicitAddress address, RegisterID dest)
    {
        m_assembler.dataTransfer32(ARMAssembler::LoadUint8, dest, address.base, address.offset);
    }

    void load8(BaseIndex address, RegisterID dest)
    {
        m_assembler.baseIndexTransfer32(ARMAssembler::LoadUint8, dest, address.base, address.index, static_cast<int>(address.scale), address.offset);
    }

    void load8(const void* address, RegisterID dest)
    {
        move(TrustedImmPtr(address), addressTempRegister);
        m_assembler.dataTransfer32(ARMAssembler::LoadUint8, dest, addressTempRegister, 0);
    }

    void load8SignedExtendTo32(Address address, RegisterID dest)
    {
        m_assembler.dataTransfer16(ARMAssembler::LoadInt8, dest, address.base, address.offset);
    }

    void load8SignedExtendTo32(BaseIndex address, RegisterID dest)
    {
        m_assembler.baseIndexTransfer16(ARMAssembler::LoadInt8, dest, address.base, address.index, static_cast<int>(address.scale), address.offset);
    }

    void load16(ImplicitAddress address, RegisterID dest)
    {
        m_assembler.dataTransfer16(ARMAssembler::LoadUint16, dest, address.base, address.offset);
    }

    void load16(BaseIndex address, RegisterID dest)
    {
        m_assembler.baseIndexTransfer16(ARMAssembler::LoadUint16, dest, address.base, address.index, static_cast<int>(address.scale), address.offset);
    }

    void load16SignedExtendTo32(BaseIndex address, RegisterID dest)
    {
        m_assembler.baseIndexTransfer16(ARMAssembler::LoadInt16, dest, address.base, address.index, static_cast<int>(address.scale), address.offset);
    }

    void load32(ImplicitAddress address, RegisterID dest)
    {
        m_assembler.dataTransfer32(ARMAssembler::LoadUint32, dest, address.base, address.offset);
    }

    void load32(BaseIndex address, RegisterID dest)
    {
        m_assembler.baseIndexTransfer32(ARMAssembler::LoadUint32, dest, address.base, address.index, static_cast<int>(address.scale), address.offset);
    }

#if CPU(ARMV5_OR_LOWER)
    void load32WithUnalignedHalfWords(BaseIndex address, RegisterID dest);
#else
    void load32WithUnalignedHalfWords(BaseIndex address, RegisterID dest)
    {
        load32(address, dest);
    }
#endif

    void load16Unaligned(BaseIndex address, RegisterID dest)
    {
        load16(address, dest);
    }

    void abortWithReason(AbortReason reason)
    {
        move(TrustedImm32(reason), addressTempRegister);
        breakpoint();
    }

    void abortWithReason(AbortReason reason, intptr_t misc)
    {
        move(TrustedImm32(misc), dataTempRegister);
        abortWithReason(reason);
    }

    ConvertibleLoadLabel convertibleLoadPtr(Address address, RegisterID dest)
    {
        ConvertibleLoadLabel result(this);
        ASSERT(address.offset >= 0 && address.offset <= 255);
        m_assembler.dtrUp(ARMAssembler::LoadUint32, dest, address.base, address.offset);
        return result;
    }

    DataLabel32 load32WithAddressOffsetPatch(Address address, RegisterID dest)
    {
        DataLabel32 dataLabel(this);
        m_assembler.ldrUniqueImmediate(addressTempRegister, 0);
        m_assembler.dtrUpRegister(ARMAssembler::LoadUint32, dest, address.base, addressTempRegister);
        return dataLabel;
    }

    static bool isCompactPtrAlignedAddressOffset(ptrdiff_t value)
    {
        return value >= -4095 && value <= 4095;
    }

    DataLabelCompact load32WithCompactAddressOffsetPatch(Address address, RegisterID dest)
    {
        DataLabelCompact dataLabel(this);
        ASSERT(isCompactPtrAlignedAddressOffset(address.offset));
        if (address.offset >= 0)
            m_assembler.dtrUp(ARMAssembler::LoadUint32, dest, address.base, address.offset);
        else
            m_assembler.dtrDown(ARMAssembler::LoadUint32, dest, address.base, address.offset);
        return dataLabel;
    }

    DataLabel32 store32WithAddressOffsetPatch(RegisterID src, Address address)
    {
        DataLabel32 dataLabel(this);
        m_assembler.ldrUniqueImmediate(addressTempRegister, 0);
        m_assembler.dtrUpRegister(ARMAssembler::StoreUint32, src, address.base, addressTempRegister);
        return dataLabel;
    }

    void store8(RegisterID src, BaseIndex address)
    {
        m_assembler.baseIndexTransfer32(ARMAssembler::StoreUint8, src, address.base, address.index, static_cast<int>(address.scale), address.offset);
    }

    void store8(RegisterID src, ImplicitAddress address)
    {
        m_assembler.dtrUp(ARMAssembler::StoreUint8, src, address.base, address.offset);
    }

    void store8(RegisterID src, const void* address)
    {
        move(TrustedImmPtr(address), addressTempRegister);
        m_assembler.dtrUp(ARMAssembler::StoreUint8, src, addressTempRegister, 0);
    }

    void store8(TrustedImm32 imm, ImplicitAddress address)
    {
        TrustedImm32 imm8(static_cast<int8_t>(imm.m_value));
        move(imm8, dataTempRegister);
        store8(dataTempRegister, address);
    }

    void store8(TrustedImm32 imm, const void* address)
    {
        TrustedImm32 imm8(static_cast<int8_t>(imm.m_value));
        move(TrustedImm32(reinterpret_cast<ARMWord>(address)), addressTempRegister);
        move(imm8, dataTempRegister);
        m_assembler.dtrUp(ARMAssembler::StoreUint8, dataTempRegister, addressTempRegister, 0);
    }

    void store16(RegisterID src, BaseIndex address)
    {
        m_assembler.baseIndexTransfer16(ARMAssembler::StoreUint16, src, address.base, address.index, static_cast<int>(address.scale), address.offset);
    }

    void store16(RegisterID src, ImplicitAddress address)
    {
        m_assembler.dataTransfer16(ARMAssembler::StoreUint16, src, address.base, address.offset);
    }

    void store32(RegisterID src, ImplicitAddress address)
    {
        m_assembler.dataTransfer32(ARMAssembler::StoreUint32, src, address.base, address.offset);
    }

    void store32(RegisterID src, BaseIndex address)
    {
        m_assembler.baseIndexTransfer32(ARMAssembler::StoreUint32, src, address.base, address.index, static_cast<int>(address.scale), address.offset);
    }

    void store32(TrustedImm32 imm, ImplicitAddress address)
    {
        move(imm, dataTempRegister);
        store32(dataTempRegister, address);
    }

    void store32(TrustedImm32 imm, BaseIndex address)
    {
        move(imm, dataTempRegister);
        m_assembler.baseIndexTransfer32(ARMAssembler::StoreUint32, dataTempRegister, address.base, address.index, static_cast<int>(address.scale), address.offset);
    }

    void store32(RegisterID src, const void* address)
    {
        m_assembler.ldrUniqueImmediate(addressTempRegister, reinterpret_cast<ARMWord>(address));
        m_assembler.dtrUp(ARMAssembler::StoreUint32, src, addressTempRegister, 0);
    }

    void store32(TrustedImm32 imm, const void* address)
    {
        m_assembler.ldrUniqueImmediate(addressTempRegister, reinterpret_cast<ARMWord>(address));
        m_assembler.moveImm(imm.m_value, dataTempRegister);
        m_assembler.dtrUp(ARMAssembler::StoreUint32, dataTempRegister, addressTempRegister, 0);
    }

    void pop(RegisterID dest)
    {
        m_assembler.pop(dest);
    }

    void popPair(RegisterID dest1, RegisterID dest2)
    {
        m_assembler.pop(dest1);
        m_assembler.pop(dest2);
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
        move(imm, addressTempRegister);
        push(addressTempRegister);
    }

    void pushPair(RegisterID src1, RegisterID src2)
    {
        m_assembler.push(src2);
        m_assembler.push(src1);
    }

    void move(TrustedImm32 imm, RegisterID dest)
    {
        m_assembler.moveImm(imm.m_value, dest);
    }

    void move(RegisterID src, RegisterID dest)
    {
        if (src != dest)
            m_assembler.mov(dest, src);
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
        if (src != dest)
            move(src, dest);
    }

    void zeroExtend32ToPtr(RegisterID src, RegisterID dest)
    {
        if (src != dest)
            move(src, dest);
    }

    Jump branch8(RelationalCondition cond, Address left, TrustedImm32 right)
    {
        TrustedImm32 right8 = MacroAssemblerHelpers::mask8OnCondition(*this, cond, right);
        MacroAssemblerHelpers::load8OnCondition(*this, cond, left, dataTempRegister);
        return branch32(cond, dataTempRegister, right8);
    }

    Jump branch8(RelationalCondition cond, BaseIndex left, TrustedImm32 right)
    {
        TrustedImm32 right8 = MacroAssemblerHelpers::mask8OnCondition(*this, cond, right);
        MacroAssemblerHelpers::load8OnCondition(*this, cond, left, dataTempRegister);
        return branch32(cond, dataTempRegister, right8);
    }

    Jump branch8(RelationalCondition cond, AbsoluteAddress left, TrustedImm32 right)
    {
        TrustedImm32 right8 = MacroAssemblerHelpers::mask8OnCondition(*this, cond, right);
        move(TrustedImmPtr(left.m_ptr), dataTempRegister);
        MacroAssemblerHelpers::load8OnCondition(*this, cond, Address(dataTempRegister), dataTempRegister);
        return branch32(cond, dataTempRegister, right8);
    }

    Jump branchPtr(RelationalCondition cond, BaseIndex left, RegisterID right)
    {
        load32(left, dataTempRegister);
        return branch32(cond, dataTempRegister, right);
    }

    Jump branch32(RelationalCondition cond, RegisterID left, RegisterID right, int useConstantPool = 0)
    {
        m_assembler.cmp(left, right);
        return Jump(m_assembler.jmp(ARMCondition(cond), useConstantPool));
    }

    Jump branch32(RelationalCondition cond, RegisterID left, TrustedImm32 right, int useConstantPool = 0)
    {
        internalCompare32(left, right);
        return Jump(m_assembler.jmp(ARMCondition(cond), useConstantPool));
    }

    Jump branch32(RelationalCondition cond, RegisterID left, Address right)
    {
        load32(right, dataTempRegister);
        return branch32(cond, left, dataTempRegister);
    }

    Jump branch32(RelationalCondition cond, Address left, RegisterID right)
    {
        load32(left, dataTempRegister);
        return branch32(cond, dataTempRegister, right);
    }

    Jump branch32(RelationalCondition cond, Address left, TrustedImm32 right)
    {
        load32(left, dataTempRegister);
        return branch32(cond, dataTempRegister, right);
    }

    Jump branch32(RelationalCondition cond, BaseIndex left, TrustedImm32 right)
    {
        load32(left, dataTempRegister);
        return branch32(cond, dataTempRegister, right);
    }

    Jump branch32WithUnalignedHalfWords(RelationalCondition cond, BaseIndex left, TrustedImm32 right)
    {
        load32WithUnalignedHalfWords(left, dataTempRegister);
        return branch32(cond, dataTempRegister, right);
    }

    Jump branchTest8(ResultCondition cond, Address address, TrustedImm32 mask = TrustedImm32(-1))
    {
        TrustedImm32 mask8 = MacroAssemblerHelpers::mask8OnCondition(*this, cond, mask);
        MacroAssemblerHelpers::load8OnCondition(*this, cond, address, dataTempRegister);
        return branchTest32(cond, dataTempRegister, mask8);
    }

    Jump branchTest8(ResultCondition cond, BaseIndex address, TrustedImm32 mask = TrustedImm32(-1))
    {
        TrustedImm32 mask8 = MacroAssemblerHelpers::mask8OnCondition(*this, cond, mask);
        MacroAssemblerHelpers::load8OnCondition(*this, cond, address, dataTempRegister);
        return branchTest32(cond, dataTempRegister, mask8);
    }

    Jump branchTest8(ResultCondition cond, AbsoluteAddress address, TrustedImm32 mask = TrustedImm32(-1))
    {
        TrustedImm32 mask8 = MacroAssemblerHelpers::mask8OnCondition(*this, cond, mask);
        move(TrustedImmPtr(address.m_ptr), dataTempRegister);
        MacroAssemblerHelpers::load8OnCondition(*this, cond, Address(dataTempRegister), dataTempRegister);
        return branchTest32(cond, dataTempRegister, mask8);
    }

    Jump branchTest32(ResultCondition cond, RegisterID reg, RegisterID mask)
    {
        ASSERT(cond == Zero || cond == NonZero || cond == Signed || cond == PositiveOrZero);
        m_assembler.tst(reg, mask);
        return Jump(m_assembler.jmp(ARMCondition(cond)));
    }

    Jump branchTest32(ResultCondition cond, RegisterID reg, TrustedImm32 mask = TrustedImm32(-1))
    {
        ASSERT(cond == Zero || cond == NonZero || cond == Signed || cond == PositiveOrZero);
        ARMWord w = m_assembler.getImm(mask.m_value, addressTempRegister, true);
        if (w & ARMAssembler::Op2InvertedImmediate)
            m_assembler.bics(addressTempRegister, reg, w & ~ARMAssembler::Op2InvertedImmediate);
        else
            m_assembler.tst(reg, w);
        return Jump(m_assembler.jmp(ARMCondition(cond)));
    }

    Jump branchTest32(ResultCondition cond, Address address, TrustedImm32 mask = TrustedImm32(-1))
    {
        load32(address, dataTempRegister);
        return branchTest32(cond, dataTempRegister, mask);
    }

    Jump branchTest32(ResultCondition cond, BaseIndex address, TrustedImm32 mask = TrustedImm32(-1))
    {
        load32(address, dataTempRegister);
        return branchTest32(cond, dataTempRegister, mask);
    }

    Jump jump()
    {
        return Jump(m_assembler.jmp());
    }

    void jump(RegisterID target, PtrTag)
    {
        m_assembler.bx(target);
    }

    void jump(Address address, PtrTag)
    {
        load32(address, ARMRegisters::pc);
    }

    void jump(AbsoluteAddress address, PtrTag)
    {
        move(TrustedImmPtr(address.m_ptr), addressTempRegister);
        load32(Address(addressTempRegister, 0), ARMRegisters::pc);
    }

    ALWAYS_INLINE void jump(RegisterID target, RegisterID jumpTag) { UNUSED_PARAM(jumpTag), jump(target, NoPtrTag); }
    ALWAYS_INLINE void jump(Address address, RegisterID jumpTag) { UNUSED_PARAM(jumpTag), jump(address, NoPtrTag); }
    ALWAYS_INLINE void jump(AbsoluteAddress address, RegisterID jumpTag) { UNUSED_PARAM(jumpTag), jump(address, NoPtrTag); }

    void moveDoubleToInts(FPRegisterID src, RegisterID dest1, RegisterID dest2)
    {
        m_assembler.vmov(dest1, dest2, src);
    }

    void moveIntsToDouble(RegisterID src1, RegisterID src2, FPRegisterID dest, FPRegisterID)
    {
        m_assembler.vmov(dest, src1, src2);
    }

    Jump branchAdd32(ResultCondition cond, RegisterID src, RegisterID dest)
    {
        ASSERT((cond == Overflow) || (cond == Signed) || (cond == Zero)
            || (cond == NonZero) || (cond == PositiveOrZero));
        add32(src, dest);
        return Jump(m_assembler.jmp(ARMCondition(cond)));
    }

    Jump branchAdd32(ResultCondition cond, RegisterID op1, RegisterID op2, RegisterID dest)
    {
        ASSERT((cond == Overflow) || (cond == Signed) || (cond == Zero)
            || (cond == NonZero) || (cond == PositiveOrZero));
        add32(op1, op2, dest);
        return Jump(m_assembler.jmp(ARMCondition(cond)));
    }

    Jump branchAdd32(ResultCondition cond, TrustedImm32 imm, RegisterID dest)
    {
        ASSERT((cond == Overflow) || (cond == Signed) || (cond == Zero)
            || (cond == NonZero) || (cond == PositiveOrZero));
        add32(imm, dest);
        return Jump(m_assembler.jmp(ARMCondition(cond)));
    }

    Jump branchAdd32(ResultCondition cond, RegisterID src, TrustedImm32 imm, RegisterID dest)
    {
        ASSERT((cond == Overflow) || (cond == Signed) || (cond == Zero)
            || (cond == NonZero) || (cond == PositiveOrZero));
        add32(src, imm, dest);
        return Jump(m_assembler.jmp(ARMCondition(cond)));
    }

    Jump branchAdd32(ResultCondition cond, TrustedImm32 imm, AbsoluteAddress dest)
    {
        ASSERT((cond == Overflow) || (cond == Signed) || (cond == Zero)
            || (cond == NonZero) || (cond == PositiveOrZero));
        add32(imm, dest);
        return Jump(m_assembler.jmp(ARMCondition(cond)));
    }

    Jump branchAdd32(ResultCondition cond, Address src, RegisterID dest)
    {
        load32(src, addressTempRegister);
        return branchAdd32(cond, dest, addressTempRegister, dest);
    }
    void mull32(RegisterID op1, RegisterID op2, RegisterID dest)
    {
        if (op2 == dest) {
            if (op1 == dest) {
                move(op2, addressTempRegister);
                op2 = addressTempRegister;
            } else {
                // Swap the operands.
                RegisterID tmp = op1;
                op1 = op2;
                op2 = tmp;
            }
        }
        m_assembler.mull(dataTempRegister, dest, op1, op2);
        m_assembler.cmp(dataTempRegister, m_assembler.asr(dest, 31));
    }

    Jump branchMul32(ResultCondition cond, RegisterID src1, RegisterID src2, RegisterID dest)
    {
        ASSERT((cond == Overflow) || (cond == Signed) || (cond == Zero) || (cond == NonZero));
        if (cond == Overflow) {
            mull32(src1, src2, dest);
            cond = NonZero;
        }
        else
            mul32(src1, src2, dest);
        return Jump(m_assembler.jmp(ARMCondition(cond)));
    }

    Jump branchMul32(ResultCondition cond, RegisterID src, RegisterID dest)
    {
        return branchMul32(cond, src, dest, dest);
    }

    Jump branchMul32(ResultCondition cond, RegisterID src, TrustedImm32 imm, RegisterID dest)
    {
        ASSERT((cond == Overflow) || (cond == Signed) || (cond == Zero) || (cond == NonZero));
        if (cond == Overflow) {
            move(imm, addressTempRegister);
            mull32(addressTempRegister, src, dest);
            cond = NonZero;
        }
        else
            mul32(imm, src, dest);
        return Jump(m_assembler.jmp(ARMCondition(cond)));
    }

    Jump branchSub32(ResultCondition cond, RegisterID src, RegisterID dest)
    {
        ASSERT((cond == Overflow) || (cond == Signed) || (cond == Zero) || (cond == NonZero));
        sub32(src, dest);
        return Jump(m_assembler.jmp(ARMCondition(cond)));
    }

    Jump branchSub32(ResultCondition cond, TrustedImm32 imm, RegisterID dest)
    {
        ASSERT((cond == Overflow) || (cond == Signed) || (cond == Zero) || (cond == NonZero));
        sub32(imm, dest);
        return Jump(m_assembler.jmp(ARMCondition(cond)));
    }

    Jump branchSub32(ResultCondition cond, RegisterID src, TrustedImm32 imm, RegisterID dest)
    {
        ASSERT((cond == Overflow) || (cond == Signed) || (cond == Zero) || (cond == NonZero));
        sub32(src, imm, dest);
        return Jump(m_assembler.jmp(ARMCondition(cond)));
    }

    Jump branchSub32(ResultCondition cond, RegisterID op1, RegisterID op2, RegisterID dest)
    {
        ASSERT((cond == Overflow) || (cond == Signed) || (cond == Zero) || (cond == NonZero));
        m_assembler.subs(dest, op1, op2);
        return Jump(m_assembler.jmp(ARMCondition(cond)));
    }

    Jump branchNeg32(ResultCondition cond, RegisterID srcDest)
    {
        ASSERT((cond == Overflow) || (cond == Signed) || (cond == Zero) || (cond == NonZero));
        neg32(srcDest);
        return Jump(m_assembler.jmp(ARMCondition(cond)));
    }

    Jump branchOr32(ResultCondition cond, RegisterID src, RegisterID dest)
    {
        ASSERT((cond == Signed) || (cond == Zero) || (cond == NonZero));
        or32(src, dest);
        return Jump(m_assembler.jmp(ARMCondition(cond)));
    }

    PatchableJump patchableJump()
    {
        return PatchableJump(m_assembler.jmp(ARMAssembler::AL, 1));
    }

    PatchableJump patchableBranch32(RelationalCondition cond, RegisterID reg, TrustedImm32 imm)
    {
        internalCompare32(reg, imm);
        Jump jump(m_assembler.loadBranchTarget(dataTempRegister, ARMCondition(cond), true));
        m_assembler.bx(dataTempRegister, ARMCondition(cond));
        return PatchableJump(jump);
    }

    PatchableJump patchableBranch32(RelationalCondition cond, Address address, TrustedImm32 imm)
    {
        internalCompare32(address, imm);
        Jump jump(m_assembler.loadBranchTarget(dataTempRegister, ARMCondition(cond), false));
        m_assembler.bx(dataTempRegister, ARMCondition(cond));
        return PatchableJump(jump);
    }

    void breakpoint()
    {
        m_assembler.bkpt(0);
    }

    static bool isBreakpoint(void* address) { return ARMAssembler::isBkpt(address); }

    Call nearCall()
    {
        m_assembler.loadBranchTarget(dataTempRegister, ARMAssembler::AL, true);
        return Call(m_assembler.blx(dataTempRegister), Call::LinkableNear);
    }

    Call nearTailCall()
    {
        return Call(m_assembler.jmp(), Call::LinkableNearTail);
    }

    Call call(RegisterID target, PtrTag)
    {
        return Call(m_assembler.blx(target), Call::None);
    }

    void call(Address address, PtrTag)
    {
        call32(address.base, address.offset);
    }

    void ret()
    {
        m_assembler.bx(linkRegister);
    }

    void compare32(RelationalCondition cond, RegisterID left, RegisterID right, RegisterID dest)
    {
        m_assembler.cmp(left, right);
        m_assembler.mov(dest, ARMAssembler::getOp2Byte(0));
        m_assembler.mov(dest, ARMAssembler::getOp2Byte(1), ARMCondition(cond));
    }

    void compare32(RelationalCondition cond, RegisterID left, TrustedImm32 right, RegisterID dest)
    {
        m_assembler.cmp(left, m_assembler.getImm(right.m_value, addressTempRegister));
        m_assembler.mov(dest, ARMAssembler::getOp2Byte(0));
        m_assembler.mov(dest, ARMAssembler::getOp2Byte(1), ARMCondition(cond));
    }

    void compare8(RelationalCondition cond, Address left, TrustedImm32 right, RegisterID dest)
    {
        TrustedImm32 right8 = MacroAssemblerHelpers::mask8OnCondition(*this, cond, right);
        MacroAssemblerHelpers::load8OnCondition(*this, cond, left, dataTempRegister);
        compare32(cond, dataTempRegister, right8, dest);
    }

    void test32(ResultCondition cond, RegisterID reg, TrustedImm32 mask, RegisterID dest)
    {
        if (mask.m_value == -1)
            m_assembler.tst(reg, reg);
        else
            m_assembler.tst(reg, m_assembler.getImm(mask.m_value, addressTempRegister));
        m_assembler.mov(dest, ARMAssembler::getOp2Byte(0));
        m_assembler.mov(dest, ARMAssembler::getOp2Byte(1), ARMCondition(cond));
    }

    void test32(ResultCondition cond, Address address, TrustedImm32 mask, RegisterID dest)
    {
        load32(address, dataTempRegister);
        test32(cond, dataTempRegister, mask, dest);
    }

    void test8(ResultCondition cond, Address address, TrustedImm32 mask, RegisterID dest)
    {
        TrustedImm32 mask8 = MacroAssemblerHelpers::mask8OnCondition(*this, cond, mask);
        MacroAssemblerHelpers::load8OnCondition(*this, cond, address, dataTempRegister);
        test32(cond, dataTempRegister, mask8, dest);
    }

    void add32(TrustedImm32 imm, RegisterID src, RegisterID dest)
    {
        m_assembler.add(dest, src, m_assembler.getImm(imm.m_value, addressTempRegister));
    }

    void add32(TrustedImm32 imm, AbsoluteAddress address)
    {
        load32(address.m_ptr, dataTempRegister);
        add32(imm, dataTempRegister);
        store32(dataTempRegister, address.m_ptr);
    }

    void add64(TrustedImm32 imm, AbsoluteAddress address)
    {
        ARMWord tmp;

        move(TrustedImmPtr(address.m_ptr), dataTempRegister);
        m_assembler.dtrUp(ARMAssembler::LoadUint32, addressTempRegister, dataTempRegister, 0);

        if ((tmp = ARMAssembler::getOp2(imm.m_value)) != ARMAssembler::InvalidImmediate)
            m_assembler.adds(addressTempRegister, addressTempRegister, tmp);
        else if ((tmp = ARMAssembler::getOp2(-imm.m_value)) != ARMAssembler::InvalidImmediate)
            m_assembler.subs(addressTempRegister, addressTempRegister, tmp);
        else {
            m_assembler.adds(addressTempRegister, addressTempRegister, m_assembler.getImm(imm.m_value, dataTempRegister));
            move(TrustedImmPtr(address.m_ptr), dataTempRegister);
        }
        m_assembler.dtrUp(ARMAssembler::StoreUint32, addressTempRegister, dataTempRegister, 0);

        m_assembler.dtrUp(ARMAssembler::LoadUint32, addressTempRegister, dataTempRegister, sizeof(ARMWord));
        if (imm.m_value >= 0)
            m_assembler.adc(addressTempRegister, addressTempRegister, ARMAssembler::getOp2Byte(0));
        else
            m_assembler.sbc(addressTempRegister, addressTempRegister, ARMAssembler::getOp2Byte(0));
        m_assembler.dtrUp(ARMAssembler::StoreUint32, addressTempRegister, dataTempRegister, sizeof(ARMWord));
    }

    void sub32(TrustedImm32 imm, AbsoluteAddress address)
    {
        load32(address.m_ptr, dataTempRegister);
        sub32(imm, dataTempRegister);
        store32(dataTempRegister, address.m_ptr);
    }

    void load32(const void* address, RegisterID dest)
    {
        m_assembler.ldrUniqueImmediate(addressTempRegister, reinterpret_cast<ARMWord>(address));
        m_assembler.dtrUp(ARMAssembler::LoadUint32, dest, addressTempRegister, 0);
    }

    Jump branch32(RelationalCondition cond, AbsoluteAddress left, RegisterID right)
    {
        load32(left.m_ptr, dataTempRegister);
        return branch32(cond, dataTempRegister, right);
    }

    Jump branch32(RelationalCondition cond, AbsoluteAddress left, TrustedImm32 right)
    {
        load32(left.m_ptr, dataTempRegister);
        return branch32(cond, dataTempRegister, right);
    }

    void relativeTableJump(RegisterID index, int scale)
    {
        ASSERT(scale >= 0 && scale <= 31);
        m_assembler.add(ARMRegisters::pc, ARMRegisters::pc, m_assembler.lsl(index, scale));

        // NOP the default prefetching
        m_assembler.mov(ARMRegisters::r0, ARMRegisters::r0);
    }

    Call call(PtrTag)
    {
        ensureSpace(2 * sizeof(ARMWord), sizeof(ARMWord));
        m_assembler.loadBranchTarget(dataTempRegister, ARMAssembler::AL, true);
        return Call(m_assembler.blx(dataTempRegister), Call::Linkable);
    }

    ALWAYS_INLINE Call call(RegisterID callTag) { return UNUSED_PARAM(callTag), call(NoPtrTag); }
    ALWAYS_INLINE Call call(RegisterID target, RegisterID callTag) { return UNUSED_PARAM(callTag), call(target, NoPtrTag); }
    ALWAYS_INLINE void call(Address address, RegisterID callTag) { UNUSED_PARAM(callTag), call(address, NoPtrTag); }

    Call tailRecursiveCall()
    {
        return Call::fromTailJump(jump());
    }

    Call makeTailRecursiveCall(Jump oldJump)
    {
        return Call::fromTailJump(oldJump);
    }

    DataLabelPtr moveWithPatch(TrustedImmPtr initialValue, RegisterID dest)
    {
        DataLabelPtr dataLabel(this);
        m_assembler.ldrUniqueImmediate(dest, reinterpret_cast<ARMWord>(initialValue.m_value));
        return dataLabel;
    }

    DataLabel32 moveWithPatch(TrustedImm32 initialValue, RegisterID dest)
    {
        DataLabel32 dataLabel(this);
        m_assembler.ldrUniqueImmediate(dest, static_cast<ARMWord>(initialValue.m_value));
        return dataLabel;
    }

    Jump branchPtrWithPatch(RelationalCondition cond, RegisterID left, DataLabelPtr& dataLabel, TrustedImmPtr initialRightValue = TrustedImmPtr(nullptr))
    {
        ensureSpace(3 * sizeof(ARMWord), 2 * sizeof(ARMWord));
        dataLabel = moveWithPatch(initialRightValue, dataTempRegister);
        Jump jump = branch32(cond, left, dataTempRegister, true);
        return jump;
    }

    Jump branchPtrWithPatch(RelationalCondition cond, Address left, DataLabelPtr& dataLabel, TrustedImmPtr initialRightValue = TrustedImmPtr(nullptr))
    {
        load32(left, dataTempRegister);
        ensureSpace(3 * sizeof(ARMWord), 2 * sizeof(ARMWord));
        dataLabel = moveWithPatch(initialRightValue, addressTempRegister);
        Jump jump = branch32(cond, addressTempRegister, dataTempRegister, true);
        return jump;
    }

    Jump branch32WithPatch(RelationalCondition cond, Address left, DataLabel32& dataLabel, TrustedImm32 initialRightValue = TrustedImm32(0))
    {
        load32(left, dataTempRegister);
        ensureSpace(3 * sizeof(ARMWord), 2 * sizeof(ARMWord));
        dataLabel = moveWithPatch(initialRightValue, addressTempRegister);
        Jump jump = branch32(cond, addressTempRegister, dataTempRegister, true);
        return jump;
    }

    DataLabelPtr storePtrWithPatch(TrustedImmPtr initialValue, ImplicitAddress address)
    {
        DataLabelPtr dataLabel = moveWithPatch(initialValue, dataTempRegister);
        store32(dataTempRegister, address);
        return dataLabel;
    }

    DataLabelPtr storePtrWithPatch(ImplicitAddress address)
    {
        return storePtrWithPatch(TrustedImmPtr(nullptr), address);
    }

    // Floating point operators
    static bool supportsFloatingPoint()
    {
        return s_isVFPPresent;
    }

    static bool supportsFloatingPointTruncate()
    {
        return false;
    }

    static bool supportsFloatingPointSqrt()
    {
        return s_isVFPPresent;
    }
    static bool supportsFloatingPointAbs() { return false; }
    static bool supportsFloatingPointRounding() { return false; }


    void loadFloat(ImplicitAddress address, FPRegisterID dest)
    {
        m_assembler.dataTransferFloat(ARMAssembler::LoadFloat, dest, address.base, address.offset);
    }

    void loadFloat(BaseIndex address, FPRegisterID dest)
    {
        m_assembler.baseIndexTransferFloat(ARMAssembler::LoadFloat, dest, address.base, address.index, static_cast<int>(address.scale), address.offset);
    }

    void loadDouble(ImplicitAddress address, FPRegisterID dest)
    {
        m_assembler.dataTransferFloat(ARMAssembler::LoadDouble, dest, address.base, address.offset);
    }

    void loadDouble(BaseIndex address, FPRegisterID dest)
    {
        m_assembler.baseIndexTransferFloat(ARMAssembler::LoadDouble, dest, address.base, address.index, static_cast<int>(address.scale), address.offset);
    }

    void loadDouble(TrustedImmPtr address, FPRegisterID dest)
    {
        move(TrustedImm32(reinterpret_cast<ARMWord>(address.m_value)), addressTempRegister);
        m_assembler.doubleDtrUp(ARMAssembler::LoadDouble, dest, addressTempRegister, 0);
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

    void storeFloat(FPRegisterID src, ImplicitAddress address)
    {
        m_assembler.dataTransferFloat(ARMAssembler::StoreFloat, src, address.base, address.offset);
    }

    void storeFloat(FPRegisterID src, BaseIndex address)
    {
        m_assembler.baseIndexTransferFloat(ARMAssembler::StoreFloat, src, address.base, address.index, static_cast<int>(address.scale), address.offset);
    }

    void storeDouble(FPRegisterID src, ImplicitAddress address)
    {
        m_assembler.dataTransferFloat(ARMAssembler::StoreDouble, src, address.base, address.offset);
    }

    void storeDouble(FPRegisterID src, BaseIndex address)
    {
        m_assembler.baseIndexTransferFloat(ARMAssembler::StoreDouble, src, address.base, address.index, static_cast<int>(address.scale), address.offset);
    }

    void storeDouble(FPRegisterID src, TrustedImmPtr address)
    {
        move(TrustedImm32(reinterpret_cast<ARMWord>(address.m_value)), addressTempRegister);
        m_assembler.dataTransferFloat(ARMAssembler::StoreDouble, src, addressTempRegister, 0);
    }

    void moveDouble(FPRegisterID src, FPRegisterID dest)
    {
        if (src != dest)
            m_assembler.vmov_f64(dest, src);
    }

    void moveZeroToDouble(FPRegisterID reg)
    {
        static double zeroConstant = 0.;
        loadDouble(TrustedImmPtr(&zeroConstant), reg);
    }

    void addDouble(FPRegisterID src, FPRegisterID dest)
    {
        m_assembler.vadd_f64(dest, dest, src);
    }

    void addDouble(FPRegisterID op1, FPRegisterID op2, FPRegisterID dest)
    {
        m_assembler.vadd_f64(dest, op1, op2);
    }

    void addDouble(Address src, FPRegisterID dest)
    {
        loadDouble(src, fpTempRegister);
        addDouble(fpTempRegister, dest);
    }

    void addDouble(AbsoluteAddress address, FPRegisterID dest)
    {
        loadDouble(TrustedImmPtr(address.m_ptr), fpTempRegister);
        addDouble(fpTempRegister, dest);
    }

    void divDouble(FPRegisterID src, FPRegisterID dest)
    {
        m_assembler.vdiv_f64(dest, dest, src);
    }

    void divDouble(FPRegisterID op1, FPRegisterID op2, FPRegisterID dest)
    {
        m_assembler.vdiv_f64(dest, op1, op2);
    }

    void divDouble(Address src, FPRegisterID dest)
    {
        RELEASE_ASSERT_NOT_REACHED(); // Untested
        loadDouble(src, fpTempRegister);
        divDouble(fpTempRegister, dest);
    }

    void subDouble(FPRegisterID src, FPRegisterID dest)
    {
        m_assembler.vsub_f64(dest, dest, src);
    }

    void subDouble(FPRegisterID op1, FPRegisterID op2, FPRegisterID dest)
    {
        m_assembler.vsub_f64(dest, op1, op2);
    }

    void subDouble(Address src, FPRegisterID dest)
    {
        loadDouble(src, fpTempRegister);
        subDouble(fpTempRegister, dest);
    }

    void mulDouble(FPRegisterID src, FPRegisterID dest)
    {
        m_assembler.vmul_f64(dest, dest, src);
    }

    void mulDouble(Address src, FPRegisterID dest)
    {
        loadDouble(src, fpTempRegister);
        mulDouble(fpTempRegister, dest);
    }

    void mulDouble(FPRegisterID op1, FPRegisterID op2, FPRegisterID dest)
    {
        m_assembler.vmul_f64(dest, op1, op2);
    }

    void sqrtDouble(FPRegisterID src, FPRegisterID dest)
    {
        m_assembler.vsqrt_f64(dest, src);
    }
    
    void absDouble(FPRegisterID src, FPRegisterID dest)
    {
        m_assembler.vabs_f64(dest, src);
    }

    void negateDouble(FPRegisterID src, FPRegisterID dest)
    {
        m_assembler.vneg_f64(dest, src);
    }

    void convertInt32ToDouble(RegisterID src, FPRegisterID dest)
    {
        m_assembler.vmov_vfp32(dest << 1, src);
        m_assembler.vcvt_f64_s32(dest, dest << 1);
    }

    void convertInt32ToDouble(Address src, FPRegisterID dest)
    {
        load32(src, dataTempRegister);
        convertInt32ToDouble(dataTempRegister, dest);
    }

    void convertInt32ToDouble(AbsoluteAddress src, FPRegisterID dest)
    {
        move(TrustedImmPtr(src.m_ptr), dataTempRegister);
        load32(Address(dataTempRegister), dataTempRegister);
        convertInt32ToDouble(dataTempRegister, dest);
    }

    void convertFloatToDouble(FPRegisterID src, FPRegisterID dst)
    {
        m_assembler.vcvt_f64_f32(dst, src);
    }

    void convertDoubleToFloat(FPRegisterID src, FPRegisterID dst)
    {
        m_assembler.vcvt_f32_f64(dst, src);
    }

    Jump branchDouble(DoubleCondition cond, FPRegisterID left, FPRegisterID right)
    {
        m_assembler.vcmp_f64(left, right);
        m_assembler.vmrs_apsr();
        if (cond & DoubleConditionBitSpecial)
            m_assembler.cmp(addressTempRegister, addressTempRegister, ARMAssembler::VS);
        return Jump(m_assembler.jmp(static_cast<ARMAssembler::Condition>(cond & ~DoubleConditionMask)));
    }

    // Truncates 'src' to an integer, and places the resulting 'dest'.
    // If the result is not representable as a 32 bit value, branch.
    // May also branch for some values that are representable in 32 bits
    // (specifically, in this case, INT_MIN).
    enum BranchTruncateType { BranchIfTruncateFailed, BranchIfTruncateSuccessful };
    Jump branchTruncateDoubleToInt32(FPRegisterID src, RegisterID dest, BranchTruncateType branchType = BranchIfTruncateFailed)
    {
        truncateDoubleToInt32(src, dest);

        m_assembler.add(addressTempRegister, dest, ARMAssembler::getOp2Byte(1));
        m_assembler.bic(addressTempRegister, addressTempRegister, ARMAssembler::getOp2Byte(1));

        ARMWord w = ARMAssembler::getOp2(0x80000000);
        ASSERT(w != ARMAssembler::InvalidImmediate);
        m_assembler.cmp(addressTempRegister, w);
        return Jump(m_assembler.jmp(branchType == BranchIfTruncateFailed ? ARMAssembler::EQ : ARMAssembler::NE));
    }

    // Result is undefined if the value is outside of the integer range.
    void truncateDoubleToInt32(FPRegisterID src, RegisterID dest)
    {
        m_assembler.vcvt_s32_f64(fpTempRegister << 1, src);
        m_assembler.vmov_arm32(dest, fpTempRegister << 1);
    }

    void truncateDoubleToUint32(FPRegisterID src, RegisterID dest)
    {
        m_assembler.vcvt_u32_f64(fpTempRegister << 1, src);
        m_assembler.vmov_arm32(dest, fpTempRegister << 1);
    }

    // Convert 'src' to an integer, and places the resulting 'dest'.
    // If the result is not representable as a 32 bit value, branch.
    // May also branch for some values that are representable in 32 bits
    // (specifically, in this case, 0).
    void branchConvertDoubleToInt32(FPRegisterID src, RegisterID dest, JumpList& failureCases, FPRegisterID, bool negZeroCheck = true)
    {
        m_assembler.vcvt_s32_f64(fpTempRegister << 1, src);
        m_assembler.vmov_arm32(dest, fpTempRegister << 1);

        // Convert the integer result back to float & compare to the original value - if not equal or unordered (NaN) then jump.
        m_assembler.vcvt_f64_s32(fpTempRegister, fpTempRegister << 1);
        failureCases.append(branchDouble(DoubleNotEqualOrUnordered, src, fpTempRegister));

        // If the result is zero, it might have been -0.0, and 0.0 equals to -0.0
        if (negZeroCheck)
            failureCases.append(branchTest32(Zero, dest));
    }

    Jump branchDoubleNonZero(FPRegisterID reg, FPRegisterID scratch)
    {
        m_assembler.mov(addressTempRegister, ARMAssembler::getOp2Byte(0));
        convertInt32ToDouble(addressTempRegister, scratch);
        return branchDouble(DoubleNotEqual, reg, scratch);
    }

    Jump branchDoubleZeroOrNaN(FPRegisterID reg, FPRegisterID scratch)
    {
        m_assembler.mov(addressTempRegister, ARMAssembler::getOp2Byte(0));
        convertInt32ToDouble(addressTempRegister, scratch);
        return branchDouble(DoubleEqualOrUnordered, reg, scratch);
    }

    // Invert a relational condition, e.g. == becomes !=, < becomes >=, etc.
    static RelationalCondition invert(RelationalCondition cond)
    {
        ASSERT((static_cast<uint32_t>(cond & 0x0fffffff)) == 0 && static_cast<uint32_t>(cond) < static_cast<uint32_t>(ARMAssembler::AL));
        return static_cast<RelationalCondition>(cond ^ 0x10000000);
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

    template<PtrTag resultTag, PtrTag locationTag>
    static FunctionPtr<resultTag> readCallTarget(CodeLocationCall<locationTag> call)
    {
        return FunctionPtr<resultTag>(reinterpret_cast<void(*)()>(ARMAssembler::readCallTarget(call.dataLocation())));
    }

    template<PtrTag startTag, PtrTag destTag>
    static void replaceWithJump(CodeLocationLabel<startTag> instructionStart, CodeLocationLabel<destTag> destination)
    {
        ARMAssembler::replaceWithJump(instructionStart.dataLocation(), destination.dataLocation());
    }
    
    static ptrdiff_t maxJumpReplacementSize()
    {
        return ARMAssembler::maxJumpReplacementSize();
    }

    static ptrdiff_t patchableJumpSize()
    {
        return ARMAssembler::patchableJumpSize();
    }

    static bool canJumpReplacePatchableBranchPtrWithPatch() { return false; }
    static bool canJumpReplacePatchableBranch32WithPatch() { return false; }

    template<PtrTag tag>
    static CodeLocationLabel<tag> startOfPatchableBranch32WithPatchOnAddress(CodeLocationDataLabel32<tag>)
    {
        UNREACHABLE_FOR_PLATFORM();
        return CodeLocationLabel<tag>();
    }

    template<PtrTag tag>
    static CodeLocationLabel<tag> startOfPatchableBranchPtrWithPatchOnAddress(CodeLocationDataLabelPtr<tag>)
    {
        UNREACHABLE_FOR_PLATFORM();
        return CodeLocationLabel<tag>();
    }

    template<PtrTag tag>
    static CodeLocationLabel<tag> startOfBranchPtrWithPatchOnRegister(CodeLocationDataLabelPtr<tag> label)
    {
        return label.labelAtOffset(0);
    }

    template<PtrTag tag>
    static void revertJumpReplacementToBranchPtrWithPatch(CodeLocationLabel<tag> instructionStart, RegisterID reg, void* initialValue)
    {
        ARMAssembler::revertBranchPtrWithPatch(instructionStart.dataLocation(), reg, reinterpret_cast<uintptr_t>(initialValue) & 0xffff);
    }

    template<PtrTag tag>
    static void revertJumpReplacementToPatchableBranch32WithPatch(CodeLocationLabel<tag>, Address, int32_t)
    {
        UNREACHABLE_FOR_PLATFORM();
    }

    template<PtrTag tag>
    static void revertJumpReplacementToPatchableBranchPtrWithPatch(CodeLocationLabel<tag>, Address, void*)
    {
        UNREACHABLE_FOR_PLATFORM();
    }

    template<PtrTag callTag, PtrTag destTag>
    static void repatchCall(CodeLocationCall<callTag> call, CodeLocationLabel<destTag> destination)
    {
        ARMAssembler::relinkCall(call.dataLocation(), destination.executableAddress());
    }

    template<PtrTag callTag, PtrTag destTag>
    static void repatchCall(CodeLocationCall<callTag> call, FunctionPtr<destTag> destination)
    {
        ARMAssembler::relinkCall(call.dataLocation(), destination.executableAddress());
    }

protected:
    ARMAssembler::Condition ARMCondition(RelationalCondition cond)
    {
        return static_cast<ARMAssembler::Condition>(cond);
    }

    ARMAssembler::Condition ARMCondition(ResultCondition cond)
    {
        return static_cast<ARMAssembler::Condition>(cond);
    }

    void ensureSpace(int insnSpace, int constSpace)
    {
        m_assembler.ensureSpace(insnSpace, constSpace);
    }

    int sizeOfConstantPool()
    {
        return m_assembler.sizeOfConstantPool();
    }

    void call32(RegisterID base, int32_t offset)
    {
        load32(Address(base, offset), dataTempRegister);
        m_assembler.blx(dataTempRegister);
    }

private:
    friend class LinkBuffer;

    void internalCompare32(RegisterID left, TrustedImm32 right)
    {
        ARMWord tmp = (!right.m_value || static_cast<unsigned>(right.m_value) == 0x80000000) ? ARMAssembler::InvalidImmediate : m_assembler.getOp2(-right.m_value);
        if (tmp != ARMAssembler::InvalidImmediate)
            m_assembler.cmn(left, tmp);
        else
            m_assembler.cmp(left, m_assembler.getImm(right.m_value, addressTempRegister));
    }

    void internalCompare32(Address left, TrustedImm32 right)
    {
        ARMWord tmp = (!right.m_value || static_cast<unsigned>(right.m_value) == 0x80000000) ? ARMAssembler::InvalidImmediate : m_assembler.getOp2(-right.m_value);
        load32(left, dataTempRegister);
        if (tmp != ARMAssembler::InvalidImmediate)
            m_assembler.cmn(dataTempRegister, tmp);
        else
            m_assembler.cmp(dataTempRegister, m_assembler.getImm(right.m_value, addressTempRegister));
    }

    template<PtrTag tag>
    static void linkCall(void* code, Call call, FunctionPtr<tag> function)
    {
        if (call.isFlagSet(Call::Tail))
            ARMAssembler::linkJump(code, call.m_label, function.executableAddress());
        else
            ARMAssembler::linkCall(code, call.m_label, function.executableAddress());
    }

    static const bool s_isVFPPresent;
};

} // namespace JSC

#endif // ENABLE(ASSEMBLER) && CPU(ARM_TRADITIONAL)
