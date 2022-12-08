/*
 * Copyright (C) 2021 Igalia S.L.
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

#if ENABLE(ASSEMBLER) && CPU(RISCV64)

#include "AbstractMacroAssembler.h"
#include "RISCV64Assembler.h"

#define MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(methodName) \
    template<typename... Args> void methodName(Args&&...) { }
#define MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD_WITH_RETURN(methodName, returnType) \
    template<typename... Args> returnType methodName(Args&&...) { return { }; }

namespace JSC {

using Assembler = TARGET_ASSEMBLER;

class MacroAssemblerRISCV64 : public AbstractMacroAssembler<Assembler> {
public:
    static constexpr unsigned numGPRs = 32;
    static constexpr unsigned numFPRs = 32;

    static constexpr size_t nearJumpRange = 2 * GB;

    static constexpr RegisterID dataTempRegister = RISCV64Registers::x30;
    static constexpr RegisterID memoryTempRegister = RISCV64Registers::x31;

    static constexpr FPRegisterID fpTempRegister = RISCV64Registers::f30;
    static constexpr FPRegisterID fpTempRegister2 = RISCV64Registers::f31;

    static constexpr RegisterID InvalidGPRReg = RISCV64Registers::InvalidGPRReg;

    RegisterID scratchRegister()
    {
        return dataTempRegister;
    }

    enum TempRegisterType : int8_t {
        Data,
        Memory,
    };

    template<TempRegisterType... RegisterTypes>
    struct TempRegister {
        RegisterID data()
        {
            static_assert(((RegisterTypes == Data) || ...));
            return dataTempRegister;
        }

        RegisterID memory()
        {
            static_assert(((RegisterTypes == Memory) || ...));
            return memoryTempRegister;
        }
    };

    template<TempRegisterType RegisterType>
    struct LazyTempRegister {
        LazyTempRegister(bool allowScratchRegister)
            : m_allowScratchRegister(allowScratchRegister)
        {
            static_assert(RegisterType == Data || RegisterType == Memory);
        }

        operator RegisterID()
        {
            RELEASE_ASSERT(m_allowScratchRegister);
            if constexpr (RegisterType == Data)
                return dataTempRegister;
            if constexpr (RegisterType == Memory)
                return memoryTempRegister;
            return InvalidGPRReg;
        }

        bool m_allowScratchRegister;
    };

    template<TempRegisterType... RegisterTypes>
    auto temps() -> TempRegister<RegisterTypes...>
    {
        RELEASE_ASSERT(m_allowScratchRegister);
        return { };
    }

    template<TempRegisterType RegisterType>
    auto lazyTemp() -> LazyTempRegister<RegisterType>
    {
        return { m_allowScratchRegister };
    }

    static bool supportsFloatingPoint() { return true; }
    static bool supportsFloatingPointTruncate() { return true; }
    static bool supportsFloatingPointSqrt() { return true; }
    static bool supportsFloatingPointAbs() { return true; }
    static bool supportsFloatingPointRounding() { return true; }

    enum RelationalCondition {
        Equal = Assembler::ConditionEQ,
        NotEqual = Assembler::ConditionNE,
        Above = Assembler::ConditionGTU,
        AboveOrEqual = Assembler::ConditionGEU,
        Below = Assembler::ConditionLTU,
        BelowOrEqual = Assembler::ConditionLEU,
        GreaterThan = Assembler::ConditionGT,
        GreaterThanOrEqual = Assembler::ConditionGE,
        LessThan = Assembler::ConditionLT,
        LessThanOrEqual = Assembler::ConditionLE,
    };

    static constexpr RelationalCondition invert(RelationalCondition cond)
    {
        return static_cast<RelationalCondition>(Assembler::invert(static_cast<Assembler::Condition>(cond)));
    }

    enum ResultCondition {
        Overflow,
        Signed,
        PositiveOrZero,
        Zero,
        NonZero,
    };

    enum ZeroCondition {
        IsZero,
        IsNonZero,
    };

    enum DoubleCondition {
        DoubleEqualAndOrdered,
        DoubleNotEqualAndOrdered,
        DoubleGreaterThanAndOrdered,
        DoubleGreaterThanOrEqualAndOrdered,
        DoubleLessThanAndOrdered,
        DoubleLessThanOrEqualAndOrdered,
        DoubleEqualOrUnordered,
        DoubleNotEqualOrUnordered,
        DoubleGreaterThanOrUnordered,
        DoubleGreaterThanOrEqualOrUnordered,
        DoubleLessThanOrUnordered,
        DoubleLessThanOrEqualOrUnordered,
    };

    static constexpr RegisterID stackPointerRegister = RISCV64Registers::sp;
    static constexpr RegisterID framePointerRegister = RISCV64Registers::fp;
    static constexpr RegisterID linkRegister = RISCV64Registers::ra;

    void add32(RegisterID src, RegisterID dest)
    {
        add32(src, dest, dest);
    }

    void add32(RegisterID op1, RegisterID op2, RegisterID dest)
    {
        m_assembler.addwInsn(dest, op1, op2);
        m_assembler.maskRegister<32>(dest);
    }

    void add32(TrustedImm32 imm, RegisterID dest)
    {
        add32(imm, dest, dest);
    }

    void add32(TrustedImm32 imm, RegisterID op2, RegisterID dest)
    {
        if (Imm::isValid<Imm::IType>(imm.m_value)) {
            m_assembler.addiwInsn(dest, op2, Imm::I(imm.m_value));
            m_assembler.maskRegister<32>(dest);
            return;
        }

        auto temp = temps<Data>();
        loadImmediate(imm, temp.data());
        m_assembler.addwInsn(dest, temp.data(), op2);
        m_assembler.maskRegister<32>(dest);
    }

    void add32(TrustedImm32 imm, AbsoluteAddress address)
    {
        auto temp = temps<Data, Memory>();
        loadImmediate(TrustedImmPtr(address.m_ptr), temp.memory());
        if (Imm::isValid<Imm::IType>(imm.m_value)) {
            m_assembler.lwInsn(temp.data(), temp.memory(), Imm::I<0>());
            m_assembler.addiInsn(temp.data(), temp.data(), Imm::I(imm.m_value));
            m_assembler.swInsn(temp.memory(), temp.data(), Imm::S<0>());
            return;
        }

        m_assembler.lwInsn(temp.memory(), temp.memory(), Imm::I<0>());
        loadImmediate(imm, temp.data());
        m_assembler.addInsn(temp.data(), temp.memory(), temp.data());

        loadImmediate(TrustedImmPtr(address.m_ptr), temp.memory());
        m_assembler.swInsn(temp.memory(), temp.data(), Imm::S<0>());
    }

    void add32(TrustedImm32 imm, Address address)
    {
        auto temp = temps<Data, Memory>();
        auto resolution = resolveAddress(address, temp.memory());
        if (Imm::isValid<Imm::IType>(imm.m_value)) {
            m_assembler.lwInsn(temp.data(), resolution.base, Imm::I(resolution.offset));
            m_assembler.addiInsn(temp.data(), temp.data(), Imm::I(imm.m_value));
            m_assembler.swInsn(resolution.base, temp.data(), Imm::S(resolution.offset));
            return;
        }

        m_assembler.lwInsn(temp.memory(), resolution.base, Imm::I(resolution.offset));
        loadImmediate(imm, temp.data());
        m_assembler.addInsn(temp.data(), temp.memory(), temp.data());

        resolution = resolveAddress(address, temp.memory());
        m_assembler.swInsn(resolution.base, temp.data(), Imm::S(resolution.offset));
    }

    void add32(Address address, RegisterID dest)
    {
        auto temp = temps<Data, Memory>();
        auto resolution = resolveAddress(address, temp.memory());
        m_assembler.lwInsn(temp.data(), resolution.base, Imm::I(resolution.offset));
        m_assembler.addwInsn(dest, temp.data(), dest);
        m_assembler.maskRegister<32>(dest);
    }

    void add64(RegisterID src, RegisterID dest)
    {
        add64(src, dest, dest);
    }

    void add64(RegisterID op1, RegisterID op2, RegisterID dest)
    {
        m_assembler.addInsn(dest, op1, op2);
    }

    void add64(TrustedImm32 imm, RegisterID dest)
    {
        add64(imm, dest, dest);
    }

    void add64(TrustedImm32 imm, RegisterID op2, RegisterID dest)
    {
        if (Imm::isValid<Imm::IType>(imm.m_value)) {
            m_assembler.addiInsn(dest, op2, Imm::I(imm.m_value));
            return;
        }

        auto temp = temps<Data>();
        loadImmediate(imm, temp.data());
        m_assembler.addInsn(dest, temp.data(), op2);
    }

    void add64(TrustedImm64 imm, RegisterID dest)
    {
        add64(imm, dest, dest);
    }

    void add64(TrustedImm64 imm, RegisterID op2, RegisterID dest)
    {
        if (Imm::isValid<Imm::IType>(imm.m_value)) {
            m_assembler.addiInsn(dest, op2, Imm::I(imm.m_value));
            return;
        }

        auto temp = temps<Data>();
        loadImmediate(imm, temp.data());
        m_assembler.addInsn(dest, temp.data(), op2);
    }

    void add64(TrustedImm32 imm, AbsoluteAddress address)
    {
        auto temp = temps<Data, Memory>();
        loadImmediate(TrustedImmPtr(address.m_ptr), temp.memory());

        if (Imm::isValid<Imm::IType>(imm.m_value)) {
            m_assembler.ldInsn(temp.data(), temp.memory(), Imm::I<0>());
            m_assembler.addiInsn(temp.data(), temp.data(), Imm::I(imm.m_value));
            m_assembler.sdInsn(temp.memory(), temp.data(), Imm::S<0>());
            return;
        }

        m_assembler.ldInsn(temp.memory(), temp.memory(), Imm::I<0>());
        loadImmediate(imm, temp.data());
        m_assembler.addInsn(temp.data(), temp.data(), temp.memory());

        loadImmediate(TrustedImmPtr(address.m_ptr), temp.memory());
        m_assembler.sdInsn(temp.memory(), temp.data(), Imm::S<0>());
    }

    void add64(TrustedImm32 imm, Address address)
    {
        auto temp = temps<Data, Memory>();
        auto resolution = resolveAddress(address, temp.memory());
        m_assembler.ldInsn(temp.data(), resolution.base, Imm::I(resolution.offset));

        if (Imm::isValid<Imm::IType>(imm.m_value)) {
            m_assembler.addiInsn(temp.data(), temp.data(), Imm::I(imm.m_value));
            m_assembler.sdInsn(resolution.base, temp.data(), Imm::S(resolution.offset));
            return;
        }

        loadImmediate(imm, temp.memory());
        m_assembler.addInsn(temp.data(), temp.memory(), temp.data());

        resolution = resolveAddress(address, temp.memory());
        m_assembler.sdInsn(resolution.base, temp.data(), Imm::S(resolution.offset));
    }

    void add64(AbsoluteAddress address, RegisterID dest)
    {
        auto temp = temps<Memory>();
        loadImmediate(TrustedImmPtr(address.m_ptr), temp.memory());
        m_assembler.ldInsn(temp.memory(), temp.memory(), Imm::I<0>());
        m_assembler.addInsn(dest, temp.memory(), dest);
    }

    void add64(Address address, RegisterID dest)
    {
        auto temp = temps<Data, Memory>();
        auto resolution = resolveAddress(address, temp.memory());
        m_assembler.ldInsn(temp.data(), resolution.base, Imm::I(resolution.offset));
        m_assembler.addInsn(dest, temp.data(), dest);
    }

    void sub32(RegisterID src, RegisterID dest)
    {
        sub32(dest, src, dest);
    }

    void sub32(RegisterID op1, RegisterID op2, RegisterID dest)
    {
        m_assembler.subwInsn(dest, op1, op2);
        m_assembler.maskRegister<32>(dest);
    }

    void sub32(TrustedImm32 imm, RegisterID dest)
    {
        sub32(dest, imm, dest);
    }

    void sub32(RegisterID op1, TrustedImm32 imm, RegisterID dest)
    {
        add32(TrustedImm32(-imm.m_value), op1, dest);
    }

    void sub32(TrustedImm32 imm, AbsoluteAddress address)
    {
        auto temp = temps<Data, Memory>();
        loadImmediate(TrustedImmPtr(address.m_ptr), temp.memory());

        if (Imm::isValid<Imm::IType>(-imm.m_value)) {
            m_assembler.lwInsn(temp.data(), temp.memory(), Imm::I<0>());
            m_assembler.addiwInsn(temp.data(), temp.data(), Imm::I(-imm.m_value));
            m_assembler.swInsn(temp.memory(), temp.data(), Imm::S<0>());
            return;
        }

        m_assembler.lwInsn(temp.memory(), temp.memory(), Imm::I<0>());
        loadImmediate(imm, temp.data());
        m_assembler.subwInsn(temp.data(), temp.memory(), temp.data());

        loadImmediate(TrustedImmPtr(address.m_ptr), temp.memory());
        m_assembler.swInsn(temp.memory(), temp.data(), Imm::S<0>());
    }

    void sub32(TrustedImm32 imm, Address address)
    {
        auto temp = temps<Data, Memory>();
        auto resolution = resolveAddress(address, temp.memory());
        m_assembler.lwInsn(temp.data(), resolution.base, Imm::I(resolution.offset));

        if (Imm::isValid<Imm::IType>(-imm.m_value)) {
            m_assembler.addiwInsn(temp.data(), temp.data(), Imm::I(-imm.m_value));
            m_assembler.swInsn(resolution.base, temp.data(), Imm::S(resolution.offset));
            return;
        }

        loadImmediate(imm, temp.memory());
        m_assembler.subwInsn(temp.data(), temp.data(), temp.memory());

        resolution = resolveAddress(address, temp.memory());
        m_assembler.swInsn(resolution.base, temp.data(), Imm::S(resolution.offset));
    }

    void sub32(Address address, RegisterID dest)
    {
        auto temp = temps<Data, Memory>();
        auto resolution = resolveAddress(address, temp.memory());
        m_assembler.lwInsn(temp.data(), resolution.base, Imm::I(resolution.offset));
        m_assembler.subwInsn(dest, dest, temp.data());
        m_assembler.maskRegister<32>(dest);
    }

    void sub64(RegisterID src, RegisterID dest)
    {
        sub64(dest, src, dest);
    }

    void sub64(RegisterID op1, RegisterID op2, RegisterID dest)
    {
        m_assembler.subInsn(dest, op1, op2);
    }

    void sub64(TrustedImm32 imm, RegisterID dest)
    {
        sub64(dest, imm, dest);
    }

    void sub64(RegisterID op1, TrustedImm32 imm, RegisterID dest)
    {
        add64(TrustedImm32(-imm.m_value), op1, dest);
    }

    void sub64(TrustedImm64 imm, RegisterID dest)
    {
        sub64(dest, imm, dest);
    }

    void sub64(RegisterID op1, TrustedImm64 imm, RegisterID dest)
    {
        add64(TrustedImm64(-imm.m_value), op1, dest);
    }

    void mul32(RegisterID src, RegisterID dest)
    {
        mul32(src, dest, dest);
    }

    void mul32(RegisterID lhs, RegisterID rhs, RegisterID dest)
    {
        m_assembler.mulwInsn(dest, lhs, rhs);
        m_assembler.maskRegister<32>(dest);
    }

    void mul32(TrustedImm32 imm, RegisterID rhs, RegisterID dest)
    {
        auto temp = temps<Data>();
        loadImmediate(imm, temp.data());
        m_assembler.mulwInsn(dest, temp.data(), rhs);
        m_assembler.maskRegister<32>(dest);
    }

    void mul64(RegisterID src, RegisterID dest)
    {
        mul64(src, dest, dest);
    }

    void mul64(RegisterID lhs, RegisterID rhs, RegisterID dest)
    {
        m_assembler.mulInsn(dest, lhs, rhs);
    }

    void extractUnsignedBitfield32(RegisterID src, TrustedImm32 lsb, TrustedImm32 width, RegisterID dest)
    {
        m_assembler.srliInsn(dest, src, std::clamp<int32_t>(lsb.m_value, 0, 31));
        if (!Imm::isValid<Imm::IType>(width.m_value)) {
            auto temp = temps<Data>();
            loadImmediate(width, temp.data());
            m_assembler.andInsn(dest, dest, temp.data());
        } else
            m_assembler.andiInsn(dest, dest, Imm::I(width.m_value));
    }

    void extractUnsignedBitfield64(RegisterID src, TrustedImm32 lsb, TrustedImm32 width, RegisterID dest)
    {
        m_assembler.srliInsn(dest, src, std::clamp<int32_t>(lsb.m_value, 0, 63));
        if (!Imm::isValid<Imm::IType>(width.m_value)) {
            auto temp = temps<Data>();
            loadImmediate(width, temp.data());
            m_assembler.andInsn(dest, dest, temp.data());
        } else
            m_assembler.andiInsn(dest, dest, Imm::I(width.m_value));
    }

    void insertUnsignedBitfieldInZero32(RegisterID src, TrustedImm32 lsb, TrustedImm32 width, RegisterID dest)
    {
        if (!Imm::isValid<Imm::IType>(width.m_value)) {
            auto temp = temps<Data>();
            loadImmediate(width, temp.data());
            m_assembler.andInsn(dest, src, temp.data());
        } else
            m_assembler.andiInsn(dest, src, Imm::I(width.m_value));
        m_assembler.slliInsn(dest, dest, std::clamp<int32_t>(lsb.m_value, 0, 63));
    }

    void insertUnsignedBitfieldInZero64(RegisterID src, TrustedImm32 lsb, TrustedImm32 width, RegisterID dest)
    {
        if (!Imm::isValid<Imm::IType>(width.m_value)) {
            auto temp = temps<Data>();
            loadImmediate(width, temp.data());
            m_assembler.andInsn(dest, src, temp.data());
        } else
            m_assembler.andiInsn(dest, src, Imm::I(width.m_value));
        m_assembler.slliInsn(dest, dest, std::clamp<int32_t>(lsb.m_value, 0, 63));
    }

    void countLeadingZeros32(RegisterID src, RegisterID dest)
    {
        auto temp = temps<Data>();
        m_assembler.zeroExtend<32>(temp.data(), src);
        m_assembler.addiInsn(dest, RISCV64Registers::zero, Imm::I<32>());

        JumpList zero(makeBranch(Equal, temp.data(), RISCV64Registers::zero));

        Label loop = label();
        m_assembler.srliInsn<1>(temp.data(), temp.data());
        m_assembler.addiInsn(dest, dest, Imm::I<-1>());
        zero.append(makeBranch(Equal, temp.data(), RISCV64Registers::zero));
        jump().linkTo(loop, this);

        zero.link(this);
    }

    void countLeadingZeros64(RegisterID src, RegisterID dest)
    {
        auto temp = temps<Data>();
        m_assembler.addiInsn(temp.data(), src, Imm::I<0>());
        m_assembler.addiInsn(dest, RISCV64Registers::zero, Imm::I<64>());

        JumpList zero(makeBranch(Equal, temp.data(), RISCV64Registers::zero));

        Label loop = label();
        m_assembler.srliInsn<1>(temp.data(), temp.data());
        m_assembler.addiInsn(dest, dest, Imm::I<-1>());
        zero.append(makeBranch(Equal, temp.data(), RISCV64Registers::zero));
        jump().linkTo(loop, this);

        zero.link(this);
    }

    void countTrailingZeros32(RegisterID src, RegisterID dest)
    {
        auto temp = temps<Data>();
        m_assembler.addiInsn(dest, RISCV64Registers::zero, Imm::I<32>());
        m_assembler.zeroExtend<32>(temp.data(), src);

        JumpList zero(makeBranch(Equal, temp.data(), RISCV64Registers::zero));

        Label loop = label();
        m_assembler.slliInsn<1>(temp.data(), temp.data());
        m_assembler.addiInsn(dest, dest, Imm::I<-1>());
        zero.append(makeBranch(Equal, temp.data(), RISCV64Registers::zero));
        jump().linkTo(loop, this);

        zero.link(this);
    }

    void countTrailingZeros64(RegisterID src, RegisterID dest)
    {
        auto temp = temps<Data>();
        m_assembler.addiInsn(dest, RISCV64Registers::zero, Imm::I<64>());
        m_assembler.addiInsn(temp.data(), src, Imm::I<0>());

        JumpList zero(makeBranch(Equal, temp.data(), RISCV64Registers::zero));

        Label loop = label();
        m_assembler.slliInsn<1>(temp.data(), temp.data());
        m_assembler.addiInsn(dest, dest, Imm::I<-1>());
        zero.append(makeBranch(Equal, temp.data(), RISCV64Registers::zero));
        jump().linkTo(loop, this);

        zero.link(this);
    }

    void byteSwap16(RegisterID reg)
    {
        auto temp = temps<Data>();
        m_assembler.andiInsn(temp.data(), reg, Imm::I<0xff>());
        m_assembler.slliInsn<8>(temp.data(), temp.data());
        m_assembler.slliInsn<48>(reg, reg);
        m_assembler.srliInsn<56>(reg, reg);
        m_assembler.orInsn(reg, reg, temp.data());
    }

    void byteSwap32(RegisterID reg)
    {
        auto temp = temps<Data, Memory>();
        m_assembler.andiInsn(temp.data(), reg, Imm::I<0xff>());
        m_assembler.slliInsn<8>(temp.data(), temp.data());
        m_assembler.srliInsn<8>(reg, reg);

        for (unsigned i = 0; i < 2; ++i) {
            m_assembler.andiInsn(temp.memory(), reg, Imm::I<0xff>());
            m_assembler.orInsn(temp.data(), temp.data(), temp.memory());
            m_assembler.slliInsn<8>(temp.data(), temp.data());
            m_assembler.srliInsn<8>(reg, reg);
        }

        m_assembler.andiInsn(temp.memory(), reg, Imm::I<0xff>());
        m_assembler.orInsn(reg, temp.data(), temp.memory());
    }

    void byteSwap64(RegisterID reg)
    {
        auto temp = temps<Data, Memory>();
        m_assembler.andiInsn(temp.data(), reg, Imm::I<0xff>());
        m_assembler.slliInsn<8>(temp.data(), temp.data());
        m_assembler.srliInsn<8>(reg, reg);

        for (unsigned i = 0; i < 6; ++i) {
            m_assembler.andiInsn(temp.memory(), reg, Imm::I<0xff>());
            m_assembler.orInsn(temp.data(), temp.data(), temp.memory());
            m_assembler.slliInsn<8>(temp.data(), temp.data());
            m_assembler.srliInsn<8>(reg, reg);
        }

        m_assembler.andiInsn(temp.memory(), reg, Imm::I<0xff>());
        m_assembler.orInsn(reg, temp.data(), temp.memory());
    }

    void lshift32(RegisterID shiftAmount, RegisterID dest)
    {
        lshift32(dest, shiftAmount, dest);
    }

    void lshift32(RegisterID src, RegisterID shiftAmount, RegisterID dest)
    {
        m_assembler.sllwInsn(dest, src, shiftAmount);
        m_assembler.maskRegister<32>(dest);
    }

    void lshift32(TrustedImm32 shiftAmount, RegisterID dest)
    {
        lshift32(dest, shiftAmount, dest);
    }

    void lshift32(RegisterID src, TrustedImm32 imm, RegisterID dest)
    {
        m_assembler.slliwInsn(dest, src, uint32_t(imm.m_value & ((1 << 5) - 1)));
        m_assembler.maskRegister<32>(dest);
    }

    void lshift32(Address src, RegisterID shiftAmount, RegisterID dest)
    {
        auto temp = temps<Data>();
        load32(src, temp.data());
        lshift32(temp.data(), shiftAmount, dest);
    }

    void lshift64(RegisterID shiftAmount, RegisterID dest)
    {
        lshift64(dest, shiftAmount, dest);
    }

    void lshift64(RegisterID src, RegisterID shiftAmount, RegisterID dest)
    {
        m_assembler.sllInsn(dest, src, shiftAmount);
    }

    void lshift64(TrustedImm32 shiftAmount, RegisterID dest)
    {
        lshift64(dest, shiftAmount, dest);
    }

    void lshift64(RegisterID src, TrustedImm32 imm, RegisterID dest)
    {
        m_assembler.slliInsn(dest, src, uint32_t(imm.m_value & ((1 << 6) - 1)));
    }

    void lshift64(Address src, RegisterID shiftAmount, RegisterID dest)
    {
        auto temp = temps<Data>();
        load64(src, temp.data());
        lshift64(temp.data(), shiftAmount, dest);
    }

    void rshift32(RegisterID shiftAmount, RegisterID dest)
    {
        rshift32(dest, shiftAmount, dest);
    }

    void rshift32(RegisterID src, RegisterID shiftAmount, RegisterID dest)
    {
        m_assembler.srawInsn(dest, src, shiftAmount);
        m_assembler.maskRegister<32>(dest);
    }

    void rshift32(TrustedImm32 shiftAmount, RegisterID dest)
    {
        rshift32(dest, shiftAmount, dest);
    }

    void rshift32(RegisterID src, TrustedImm32 imm, RegisterID dest)
    {
        m_assembler.sraiwInsn(dest, src, uint32_t(imm.m_value & ((1 << 5) - 1)));
        m_assembler.maskRegister<32>(dest);
    }

    void rshift64(RegisterID shiftAmount, RegisterID dest)
    {
        rshift64(dest, shiftAmount, dest);
    }

    void rshift64(RegisterID src, RegisterID shiftAmount, RegisterID dest)
    {
        m_assembler.sraInsn(dest, src, shiftAmount);
    }

    void rshift64(TrustedImm32 shiftAmount, RegisterID dest)
    {
        rshift64(dest, shiftAmount, dest);
    }

    void rshift64(RegisterID src, TrustedImm32 imm, RegisterID dest)
    {
        m_assembler.sraiInsn(dest, src, uint32_t(imm.m_value & ((1 << 6) - 1)));
    }

    void urshift32(RegisterID shiftAmount, RegisterID dest)
    {
        urshift32(dest, shiftAmount, dest);
    }

    void urshift32(RegisterID src, RegisterID shiftAmount, RegisterID dest)
    {
        m_assembler.srlwInsn(dest, src, shiftAmount);
        m_assembler.maskRegister<32>(dest);
    }

    void urshift32(TrustedImm32 shiftAmount, RegisterID dest)
    {
        urshift32(dest, shiftAmount, dest);
    }

    void urshift32(RegisterID src, TrustedImm32 imm, RegisterID dest)
    {
        m_assembler.srliwInsn(dest, src, uint32_t(imm.m_value & ((1 << 5) - 1)));
        m_assembler.maskRegister<32>(dest);
    }

    void urshift64(RegisterID shiftAmount, RegisterID dest)
    {
        urshift64(dest, shiftAmount, dest);
    }

    void urshift64(RegisterID src, RegisterID shiftAmount, RegisterID dest)
    {
        m_assembler.srlInsn(dest, src, shiftAmount);
    }

    void urshift64(TrustedImm32 shiftAmount, RegisterID dest)
    {
        urshift64(dest, shiftAmount, dest);
    }

    void urshift64(RegisterID src, TrustedImm32 imm, RegisterID dest)
    {
        m_assembler.srliInsn(dest, src, uint32_t(imm.m_value & ((1 << 6) - 1)));
    }

    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(rotateRight32);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(rotateRight64);

    void load8(Address address, RegisterID dest)
    {
        auto resolution = resolveAddress(address, lazyTemp<Memory>());
        m_assembler.lbuInsn(dest, resolution.base, Imm::I(resolution.offset));
    }

    void load8(BaseIndex address, RegisterID dest)
    {
        auto resolution = resolveAddress(address, lazyTemp<Memory>());
        m_assembler.lbuInsn(dest, resolution.base, Imm::I(resolution.offset));
    }

    void load8(const void* address, RegisterID dest)
    {
        auto temp = temps<Memory>();
        loadImmediate(TrustedImmPtr(address), temp.memory());
        m_assembler.lbuInsn(dest, temp.memory(), Imm::I<0>());
    }

    void load8SignedExtendTo32(Address address, RegisterID dest)
    {
        auto resolution = resolveAddress(address, lazyTemp<Memory>());
        m_assembler.lbInsn(dest, resolution.base, Imm::I(resolution.offset));
        m_assembler.maskRegister<32>(dest);
    }

    void load8SignedExtendTo32(BaseIndex address, RegisterID dest)
    {
        auto resolution = resolveAddress(address, lazyTemp<Memory>());
        m_assembler.lbInsn(dest, resolution.base, Imm::I(resolution.offset));
        m_assembler.maskRegister<32>(dest);
    }

    void load8SignedExtendTo32(const void* address, RegisterID dest)
    {
        auto temp = temps<Memory>();
        loadImmediate(TrustedImmPtr(address), temp.memory());
        m_assembler.lbInsn(dest, temp.memory(), Imm::I<0>());
        m_assembler.maskRegister<32>(dest);
    }

    void load16(Address address, RegisterID dest)
    {
        auto resolution = resolveAddress(address, lazyTemp<Memory>());
        m_assembler.lhuInsn(dest, resolution.base, Imm::I(resolution.offset));
    }

    void load16(BaseIndex address, RegisterID dest)
    {
        auto resolution = resolveAddress(address, lazyTemp<Memory>());
        m_assembler.lhuInsn(dest, resolution.base, Imm::I(resolution.offset));
    }

    void load16(const void* address, RegisterID dest)
    {
        auto temp = temps<Memory>();
        loadImmediate(TrustedImmPtr(address), temp.memory());
        m_assembler.lhuInsn(dest, temp.memory(), Imm::I<0>());
    }

    void load16(ExtendedAddress address, RegisterID dest)
    {
        auto temp = temps<Data, Memory>();
        loadImmediate(TrustedImm64(int64_t(address.offset)), temp.memory());
        m_assembler.slliInsn<1>(temp.data(), address.base);
        m_assembler.addInsn(temp.memory(), temp.memory(), temp.data());
        m_assembler.lhuInsn(dest, temp.memory(), Imm::I<0>());
    }

    void load16Unaligned(Address address, RegisterID dest)
    {
        load16(address, dest);
    }

    void load16Unaligned(BaseIndex address, RegisterID dest)
    {
        load16(address, dest);
    }

    void load16SignedExtendTo32(Address address, RegisterID dest)
    {
        auto resolution = resolveAddress(address, lazyTemp<Memory>());
        m_assembler.lhInsn(dest, resolution.base, Imm::I(resolution.offset));
        m_assembler.maskRegister<32>(dest);
    }

    void load16SignedExtendTo32(BaseIndex address, RegisterID dest)
    {
        auto resolution = resolveAddress(address, lazyTemp<Memory>());
        m_assembler.lhInsn(dest, resolution.base, Imm::I(resolution.offset));
        m_assembler.maskRegister<32>(dest);
    }

    void load16SignedExtendTo32(const void* address, RegisterID dest)
    {
        auto temp = temps<Memory>();
        loadImmediate(TrustedImmPtr(address), temp.memory());
        m_assembler.lhInsn(dest, temp.memory(), Imm::I<0>());
        m_assembler.maskRegister<32>(dest);
    }

    void load32(Address address, RegisterID dest)
    {
        auto resolution = resolveAddress(address, lazyTemp<Memory>());
        m_assembler.lwuInsn(dest, resolution.base, Imm::I(resolution.offset));
    }

    void load32(BaseIndex address, RegisterID dest)
    {
        auto resolution = resolveAddress(address, lazyTemp<Memory>());
        m_assembler.lwuInsn(dest, resolution.base, Imm::I(resolution.offset));
    }

    void load32(const void* address, RegisterID dest)
    {
        auto temp = temps<Memory>();
        loadImmediate(TrustedImmPtr(address), temp.memory());
        m_assembler.lwuInsn(dest, temp.memory(), Imm::I<0>());
    }

    void load32WithUnalignedHalfWords(BaseIndex address, RegisterID dest)
    {
        load32(address, dest);
    }

    void load64(Address address, RegisterID dest)
    {
        auto resolution = resolveAddress(address, lazyTemp<Memory>());
        m_assembler.ldInsn(dest, resolution.base, Imm::I(resolution.offset));
    }

    void load64(BaseIndex address, RegisterID dest)
    {
        auto resolution = resolveAddress(address, lazyTemp<Memory>());
        m_assembler.ldInsn(dest, resolution.base, Imm::I(resolution.offset));
    }

    void load64(const void* address, RegisterID dest)
    {
        auto temp = temps<Memory>();
        loadImmediate(TrustedImmPtr(address), temp.memory());
        m_assembler.ldInsn(dest, temp.memory(), Imm::I<0>());
    }

    void loadPair32(RegisterID src, RegisterID dest1, RegisterID dest2)
    {
        loadPair32(src, TrustedImm32(0), dest1, dest2);
    }

    void loadPair32(RegisterID src, TrustedImm32 offset, RegisterID dest1, RegisterID dest2)
    {
        ASSERT(dest1 != dest2);
        if (src == dest1) {
            load32(Address(src, offset.m_value + 4), dest2);
            load32(Address(src, offset.m_value), dest1);
        } else {
            load32(Address(src, offset.m_value), dest1);
            load32(Address(src, offset.m_value + 4), dest2);
        }
    }

    void loadPair64(RegisterID src, RegisterID dest1, RegisterID dest2)
    {
        loadPair64(src, TrustedImm32(0), dest1, dest2);
    }

    void loadPair64(RegisterID src, TrustedImm32 offset, RegisterID dest1, RegisterID dest2)
    {
        ASSERT(dest1 != dest2);
        if (src == dest1) {
            load64(Address(src, offset.m_value + 8), dest2);
            load64(Address(src, offset.m_value), dest1);
        } else {
            load64(Address(src, offset.m_value), dest1);
            load64(Address(src, offset.m_value + 8), dest2);
        }
    }

    void store8(RegisterID src, Address address)
    {
        auto resolution = resolveAddress(address, lazyTemp<Memory>());
        m_assembler.sbInsn(resolution.base, src, Imm::S(resolution.offset));
    }

    void store8(RegisterID src, BaseIndex address)
    {
        auto resolution = resolveAddress(address, lazyTemp<Memory>());
        m_assembler.sbInsn(resolution.base, src, Imm::S(resolution.offset));
    }

    void store8(RegisterID src, const void* address)
    {
        auto temp = temps<Memory>();
        loadImmediate(TrustedImmPtr(address), temp.memory());
        m_assembler.sbInsn(temp.memory(), src, Imm::S<0>());
    }

    void store8(TrustedImm32 imm, Address address)
    {
        auto temp = temps<Data, Memory>();
        RegisterID immRegister = RISCV64Registers::zero;
        TrustedImm32 imm8(int8_t(imm.m_value));
        if (!!imm8.m_value) {
            loadImmediate(imm8, temp.data());
            immRegister = temp.data();
        }

        auto resolution = resolveAddress(address, temp.memory());
        m_assembler.sbInsn(resolution.base, immRegister, Imm::S(resolution.offset));
    }

    void store8(TrustedImm32 imm, BaseIndex address)
    {
        auto temp = temps<Data, Memory>();
        RegisterID immRegister = RISCV64Registers::zero;
        TrustedImm32 imm8(int8_t(imm.m_value));
        if (!!imm8.m_value) {
            loadImmediate(imm8, temp.data());
            immRegister = temp.data();
        }

        auto resolution = resolveAddress(address, temp.memory());
        m_assembler.sbInsn(resolution.base, immRegister, Imm::S(resolution.offset));
    }

    void store8(TrustedImm32 imm, const void* address)
    {
        auto temp = temps<Memory, Data>();
        RegisterID immRegister = RISCV64Registers::zero;
        TrustedImm32 imm8(int8_t(imm.m_value));
        if (!!imm8.m_value) {
            loadImmediate(imm8, temp.data());
            immRegister = temp.data();
        }

        loadImmediate(TrustedImmPtr(address), temp.memory());
        m_assembler.sbInsn(temp.memory(), immRegister, Imm::S<0>());
    }

    void store16(RegisterID src, Address address)
    {
        auto resolution = resolveAddress(address, lazyTemp<Memory>());
        m_assembler.shInsn(resolution.base, src, Imm::S(resolution.offset));
    }

    void store16(RegisterID src, BaseIndex address)
    {
        auto resolution = resolveAddress(address, lazyTemp<Memory>());
        m_assembler.shInsn(resolution.base, src, Imm::S(resolution.offset));
    }

    void store16(RegisterID src, const void* address)
    {
        auto temp = temps<Memory>();
        loadImmediate(TrustedImmPtr(address), temp.memory());
        m_assembler.shInsn(temp.memory(), src, Imm::S<0>());
    }

    void store16(TrustedImm32 imm, Address address)
    {
        auto temp = temps<Data, Memory>();
        RegisterID immRegister = RISCV64Registers::zero;
        TrustedImm32 imm16(int16_t(imm.m_value));
        if (!!imm16.m_value) {
            loadImmediate(imm16, temp.data());
            immRegister = temp.data();
        }

        auto resolution = resolveAddress(address, temp.memory());
        m_assembler.shInsn(resolution.base, immRegister, Imm::S(resolution.offset));
    }

    void store16(TrustedImm32 imm, BaseIndex address)
    {
        auto temp = temps<Data, Memory>();
        RegisterID immRegister = RISCV64Registers::zero;
        TrustedImm32 imm16(int16_t(imm.m_value));
        if (!!imm16.m_value) {
            loadImmediate(imm16, temp.data());
            immRegister = temp.data();
        }

        auto resolution = resolveAddress(address, lazyTemp<Memory>());
        m_assembler.shInsn(resolution.base, immRegister, Imm::S(resolution.offset));
    }

    void store16(TrustedImm32 imm, const void* address)
    {
        auto temp = temps<Data, Memory>();
        RegisterID immRegister = RISCV64Registers::zero;
        TrustedImm32 imm16(int16_t(imm.m_value));
        if (!!imm16.m_value) {
            loadImmediate(imm16, temp.data());
            immRegister = temp.data();
        }

        loadImmediate(TrustedImmPtr(address), temp.memory());
        m_assembler.shInsn(temp.memory(), immRegister, Imm::S<0>());
    }

    void store32(RegisterID src, Address address)
    {
        auto resolution = resolveAddress(address, lazyTemp<Memory>());
        m_assembler.swInsn(resolution.base, src, Imm::S(resolution.offset));
    }

    void store32(RegisterID src, BaseIndex address)
    {
        auto resolution = resolveAddress(address, lazyTemp<Memory>());
        m_assembler.swInsn(resolution.base, src, Imm::S(resolution.offset));
    }

    void store32(RegisterID src, const void* address)
    {
        auto temp = temps<Memory>();
        loadImmediate(TrustedImmPtr(address), temp.memory());
        m_assembler.swInsn(temp.memory(), src, Imm::S<0>());
    }

    void store32(TrustedImm32 imm, Address address)
    {
        auto temp = temps<Data, Memory>();
        RegisterID immRegister = RISCV64Registers::zero;
        if (!!imm.m_value) {
            loadImmediate(imm, temp.data());
            immRegister = temp.data();
        }

        auto resolution = resolveAddress(address, temp.memory());
        m_assembler.swInsn(resolution.base, immRegister, Imm::S(resolution.offset));
    }

    void store32(TrustedImm32 imm, BaseIndex address)
    {
        auto temp = temps<Data, Memory>();
        RegisterID immRegister = RISCV64Registers::zero;
        if (!!imm.m_value) {
            loadImmediate(imm, temp.data());
            immRegister = temp.data();
        }

        auto resolution = resolveAddress(address, lazyTemp<Memory>());
        m_assembler.swInsn(resolution.base, immRegister, Imm::S(resolution.offset));
    }

    void store32(TrustedImm32 imm, const void* address)
    {
        auto temp = temps<Data, Memory>();
        RegisterID immRegister = RISCV64Registers::zero;
        if (!!imm.m_value) {
            loadImmediate(imm, temp.data());
            immRegister = temp.data();
        }

        loadImmediate(TrustedImmPtr(address), temp.memory());
        m_assembler.swInsn(temp.memory(), immRegister, Imm::S<0>());
    }

    void store64(RegisterID src, Address address)
    {
        auto resolution = resolveAddress(address, lazyTemp<Memory>());
        m_assembler.sdInsn(resolution.base, src, Imm::S(resolution.offset));
    }

    void store64(RegisterID src, BaseIndex address)
    {
        auto resolution = resolveAddress(address, lazyTemp<Memory>());
        m_assembler.sdInsn(resolution.base, src, Imm::S(resolution.offset));
    }

    void store64(RegisterID src, const void* address)
    {
        auto temp = temps<Memory>();
        loadImmediate(TrustedImmPtr(address), temp.memory());
        m_assembler.sdInsn(temp.memory(), src, Imm::S<0>());
    }

    void store64(TrustedImm32 imm, Address address)
    {
        auto temp = temps<Data, Memory>();
        RegisterID immRegister = RISCV64Registers::zero;
        if (!!imm.m_value) {
            loadImmediate(imm, temp.data());
            m_assembler.maskRegister<32>(temp.data());
            immRegister = temp.data();
        }

        auto resolution = resolveAddress(address, temp.memory());
        m_assembler.sdInsn(resolution.base, immRegister, Imm::S(resolution.offset));
    }

    void store64(TrustedImm64 imm, Address address)
    {
        auto temp = temps<Data, Memory>();
        RegisterID immRegister = RISCV64Registers::zero;
        if (!!imm.m_value) {
            loadImmediate(imm, temp.data());
            immRegister = temp.data();
        }

        auto resolution = resolveAddress(address, temp.memory());
        m_assembler.sdInsn(resolution.base, immRegister, Imm::S(resolution.offset));
    }

    void store64(TrustedImmPtr imm, Address address)
    {
        intptr_t value = imm.asIntptr();
        if constexpr (sizeof(intptr_t) == sizeof(int64_t))
            store64(TrustedImm64(int64_t(value)), address);
        else
            store64(TrustedImm32(int32_t(value)), address);
    }

    void store64(TrustedImm64 imm, BaseIndex address)
    {
        auto temp = temps<Data, Memory>();
        RegisterID immRegister = RISCV64Registers::zero;
        if (!!imm.m_value) {
            loadImmediate(imm, temp.data());
            immRegister = temp.data();
        }

        auto resolution = resolveAddress(address, temp.memory());
        m_assembler.sdInsn(resolution.base, immRegister, Imm::S(resolution.offset));
    }

    void store64(TrustedImm64 imm, const void* address)
    {
        auto temp = temps<Data, Memory>();
        RegisterID immRegister = RISCV64Registers::zero;
        if (!!imm.m_value) {
            loadImmediate(imm, temp.data());
            immRegister = temp.data();
        }

        loadImmediate(TrustedImmPtr(address), temp.memory());
        m_assembler.sdInsn(temp.memory(), immRegister, Imm::S<0>());
    }

    void transfer64(Address src, Address dest)
    {
        auto temp = temps<Data>();
        load64(src, temp.data());
        store64(temp.data(), dest);
    }

    void transferPtr(Address src, Address dest)
    {
        transfer64(src, dest);
    }

    void storePair32(RegisterID src1, RegisterID src2, RegisterID dest)
    {
        storePair32(src1, src2, dest, TrustedImm32(0));
    }

    void storePair32(RegisterID src1, RegisterID src2, RegisterID dest, TrustedImm32 offset)
    {
        store32(src1, Address(dest, offset.m_value));
        store32(src2, Address(dest, offset.m_value + 4));
    }

    void storePair64(RegisterID src1, RegisterID src2, RegisterID dest)
    {
        storePair64(src1, src2, dest, TrustedImm32(0));
    }

    void storePair64(RegisterID src1, RegisterID src2, RegisterID dest, TrustedImm32 offset)
    {
        store64(src1, Address(dest, offset.m_value));
        store64(src2, Address(dest, offset.m_value + 8));
    }

    void zeroExtend8To32(RegisterID src, RegisterID dest)
    {
        m_assembler.slliInsn<56>(dest, src);
        m_assembler.srliInsn<56>(dest, dest);
    }

    void zeroExtend16To32(RegisterID src, RegisterID dest)
    {
        m_assembler.slliInsn<48>(dest, src);
        m_assembler.srliInsn<48>(dest, dest);
    }

    void zeroExtend32ToWord(RegisterID src, RegisterID dest)
    {
        m_assembler.slliInsn<32>(dest, src);
        m_assembler.srliInsn<32>(dest, dest);
    }

    void signExtend8To32(RegisterID src, RegisterID dest)
    {
        m_assembler.slliInsn<56>(dest, src);
        m_assembler.sraiInsn<24>(dest, dest);
        m_assembler.srliInsn<32>(dest, dest);
    }

    void signExtend16To32(RegisterID src, RegisterID dest)
    {
        m_assembler.slliInsn<48>(dest, src);
        m_assembler.sraiInsn<16>(dest, dest);
        m_assembler.srliInsn<32>(dest, dest);
    }

    void signExtend32ToPtr(RegisterID src, RegisterID dest)
    {
        m_assembler.addiwInsn(dest, src, Imm::I<0>());
    }

    void signExtend32ToPtr(TrustedImm32 imm, RegisterID dest)
    {
        loadImmediate(imm, dest);
    }

    void and32(RegisterID src, RegisterID dest)
    {
        and32(src, dest, dest);
    }

    void and32(RegisterID op1, RegisterID op2, RegisterID dest)
    {
        m_assembler.andInsn(dest, op1, op2);
        m_assembler.maskRegister<32>(dest);
    }

    void and32(TrustedImm32 imm, RegisterID dest)
    {
        and32(imm, dest, dest);
    }

    void and32(TrustedImm32 imm, RegisterID op2, RegisterID dest)
    {
        if (!Imm::isValid<Imm::IType>(imm.m_value)) {
            auto temp = temps<Data>();
            loadImmediate(imm, temp.data());
            m_assembler.andInsn(dest, temp.data(), op2);
        } else
            m_assembler.andiInsn(dest, op2, Imm::I(imm.m_value));
        m_assembler.maskRegister<32>(dest);
    }

    void and32(Address address, RegisterID dest)
    {
        auto temp = temps<Data, Memory>();
        auto resolution = resolveAddress(address, temp.memory());
        m_assembler.lwInsn(temp.data(), resolution.base, Imm::I(resolution.offset));
        m_assembler.andInsn(dest, temp.data(), dest);
        m_assembler.maskRegister<32>(dest);
    }

    void and64(RegisterID src, RegisterID dest)
    {
        and64(src, dest, dest);
    }

    void and64(RegisterID op1, RegisterID op2, RegisterID dest)
    {
        m_assembler.andInsn(dest, op1, op2);
    }

    void and64(TrustedImm32 imm, RegisterID dest)
    {
        and64(imm, dest, dest);
    }

    void and64(TrustedImm32 imm, RegisterID op2, RegisterID dest)
    {
        if (Imm::isValid<Imm::IType>(imm.m_value)) {
            m_assembler.andiInsn(dest, op2, Imm::I(imm.m_value));
            return;
        }

        auto temp = temps<Data>();
        loadImmediate(imm, temp.data());
        m_assembler.andInsn(dest, temp.data(), op2);
    }

    void and64(TrustedImm64 imm, RegisterID dest)
    {
        and64(imm, dest, dest);
    }

    void and64(TrustedImm64 imm, RegisterID op2, RegisterID dest)
    {
        if (Imm::isValid<Imm::IType>(imm.m_value)) {
            m_assembler.andiInsn(dest, op2, Imm::I(imm.m_value));
            return;
        }

        auto temp = temps<Data>();
        loadImmediate(imm, temp.data());
        m_assembler.andInsn(dest, temp.data(), op2);
    }

    void and64(TrustedImmPtr imm, RegisterID dest)
    {
        intptr_t value = imm.asIntptr();
        if constexpr (sizeof(intptr_t) == sizeof(int64_t))
            and64(TrustedImm64(int64_t(value)), dest);
        else
            and64(TrustedImm32(int32_t(value)), dest);
    }

    void or8(RegisterID src, AbsoluteAddress address)
    {
        auto temp = temps<Data, Memory>();
        loadImmediate(TrustedImmPtr(address.m_ptr), temp.memory());
        m_assembler.lbInsn(temp.data(), temp.memory(), Imm::I<0>());
        m_assembler.orInsn(temp.data(), src, temp.data());
        m_assembler.sbInsn(temp.memory(), temp.data(), Imm::S<0>());
    }

    void or8(TrustedImm32 imm, AbsoluteAddress address)
    {
        auto temp = temps<Data, Memory>();
        loadImmediate(TrustedImmPtr(address.m_ptr), temp.memory());
        m_assembler.lbInsn(temp.data(), temp.memory(), Imm::I<0>());

        if (Imm::isValid<Imm::IType>(imm.m_value)) {
            m_assembler.oriInsn(temp.data(), temp.data(), Imm::I(imm.m_value));
            m_assembler.sbInsn(temp.memory(), temp.data(), Imm::S<0>());
        } else {
            loadImmediate(imm, temp.memory());
            m_assembler.orInsn(temp.data(), temp.data(), temp.memory());
            loadImmediate(TrustedImmPtr(address.m_ptr), temp.memory());
            m_assembler.sbInsn(temp.memory(), temp.data(), Imm::S<0>());
        }
    }

    void or16(RegisterID src, AbsoluteAddress address)
    {
        auto temp = temps<Data, Memory>();
        loadImmediate(TrustedImmPtr(address.m_ptr), temp.memory());
        m_assembler.lhInsn(temp.data(), temp.memory(), Imm::I<0>());
        m_assembler.orInsn(temp.data(), src, temp.data());
        m_assembler.shInsn(temp.memory(), temp.data(), Imm::S<0>());
    }

    void or16(TrustedImm32 imm, AbsoluteAddress address)
    {
        auto temp = temps<Data, Memory>();
        loadImmediate(TrustedImmPtr(address.m_ptr), temp.memory());
        m_assembler.lhInsn(temp.data(), temp.memory(), Imm::I<0>());

        if (Imm::isValid<Imm::IType>(imm.m_value)) {
            m_assembler.oriInsn(temp.data(), temp.data(), Imm::I(imm.m_value));
            m_assembler.shInsn(temp.memory(), temp.data(), Imm::S<0>());
        } else {
            loadImmediate(imm, temp.memory());
            m_assembler.orInsn(temp.data(), temp.data(), temp.memory());
            loadImmediate(TrustedImmPtr(address.m_ptr), temp.memory());
            m_assembler.shInsn(temp.memory(), temp.data(), Imm::S<0>());
        }
    }

    void or32(RegisterID src, RegisterID dest)
    {
        or32(src, dest, dest);
    }

    void or32(RegisterID op1, RegisterID op2, RegisterID dest)
    {
        m_assembler.orInsn(dest, op1, op2);
        m_assembler.maskRegister<32>(dest);
    }

    void or32(TrustedImm32 imm, RegisterID dest)
    {
        or32(imm, dest, dest);
    }

    void or32(TrustedImm32 imm, RegisterID op2, RegisterID dest)
    {
        if (Imm::isValid<Imm::IType>(imm.m_value)) {
            m_assembler.oriInsn(dest, op2, Imm::I(imm.m_value));
            m_assembler.maskRegister<32>(dest);
            return;
        }

        auto temp = temps<Data>();
        loadImmediate(imm, temp.data());
        m_assembler.orInsn(dest, temp.data(), op2);
        m_assembler.maskRegister<32>(dest);
    }

    void or32(RegisterID src, AbsoluteAddress address)
    {
        auto temp = temps<Data, Memory>();
        loadImmediate(TrustedImmPtr(address.m_ptr), temp.memory());
        m_assembler.lwInsn(temp.data(), temp.memory(), Imm::I<0>());
        m_assembler.orInsn(temp.data(), src, temp.data());
        m_assembler.swInsn(temp.memory(), temp.data(), Imm::S<0>());
    }

    void or32(TrustedImm32 imm, AbsoluteAddress address)
    {
        auto temp = temps<Data, Memory>();
        loadImmediate(TrustedImmPtr(address.m_ptr), temp.memory());
        m_assembler.lwInsn(temp.data(), temp.memory(), Imm::I<0>());

        if (Imm::isValid<Imm::IType>(imm.m_value)) {
            m_assembler.oriInsn(temp.data(), temp.data(), Imm::I(imm.m_value));
            m_assembler.swInsn(temp.memory(), temp.data(), Imm::S<0>());
        } else {
            loadImmediate(imm, temp.memory());
            m_assembler.orInsn(temp.data(), temp.data(), temp.memory());
            loadImmediate(TrustedImmPtr(address.m_ptr), temp.memory());
            m_assembler.swInsn(temp.memory(), temp.data(), Imm::S<0>());
        }
    }

    void or32(TrustedImm32 imm, Address address)
    {
        auto temp = temps<Data, Memory>();
        auto resolution = resolveAddress(address, temp.memory());
        m_assembler.lwInsn(temp.data(), resolution.base, Imm::I(resolution.offset));

        if (Imm::isValid<Imm::IType>(imm.m_value)) {
            m_assembler.oriInsn(temp.data(), temp.memory(), Imm::I(imm.m_value));
            m_assembler.swInsn(resolution.base, temp.data(), Imm::S(resolution.offset));
        } else {
            loadImmediate(imm, temp.memory());
            m_assembler.orInsn(temp.data(), temp.data(), temp.memory());
            resolution = resolveAddress(address, temp.memory());
            m_assembler.swInsn(resolution.base, temp.data(), Imm::S(resolution.offset));
        }
    }

    void or64(RegisterID src, RegisterID dest)
    {
        or64(src, dest, dest);
    }

    void or64(RegisterID op1, RegisterID op2, RegisterID dest)
    {
        m_assembler.orInsn(dest, op1, op2);
    }

    void or64(TrustedImm32 imm, RegisterID dest)
    {
        or64(imm, dest, dest);
    }

    void or64(TrustedImm32 imm, RegisterID op2, RegisterID dest)
    {
        if (Imm::isValid<Imm::IType>(imm.m_value)) {
            m_assembler.oriInsn(dest, op2, Imm::I(imm.m_value));
            return;
        }

        auto temp = temps<Data>();
        loadImmediate(imm, temp.data());
        m_assembler.orInsn(dest, temp.data(), op2);
    }

    void or64(TrustedImm64 imm, RegisterID dest)
    {
        or64(imm, dest, dest);
    }

    void or64(TrustedImm64 imm, RegisterID op2, RegisterID dest)
    {
        if (Imm::isValid<Imm::IType>(imm.m_value)) {
            m_assembler.oriInsn(dest, op2, Imm::I(imm.m_value));
            return;
        }

        auto temp = temps<Data>();
        loadImmediate(imm, temp.data());
        m_assembler.orInsn(dest, temp.data(), op2);
    }


    void xor32(RegisterID src, RegisterID dest)
    {
        xor32(src, dest, dest);
    }

    void xor32(RegisterID op1, RegisterID op2, RegisterID dest)
    {
        m_assembler.xorInsn(dest, op1, op2);
        m_assembler.maskRegister<32>(dest);
    }

    void xor32(TrustedImm32 imm, RegisterID dest)
    {
        xor32(imm, dest, dest);
    }

    void xor32(TrustedImm32 imm, RegisterID op2, RegisterID dest)
    {
        if (Imm::isValid<Imm::IType>(imm.m_value)) {
            m_assembler.xoriInsn(dest, op2, Imm::I(imm.m_value));
            m_assembler.maskRegister<32>(dest);
            return;
        }

        auto temp = temps<Data>();
        loadImmediate(imm, temp.data());
        m_assembler.xorInsn(dest, temp.data(), op2);
        m_assembler.maskRegister<32>(dest);
    }

    void xor32(Address address, RegisterID dest)
    {
        auto temp = temps<Data, Memory>();
        auto resolution = resolveAddress(address, temp.memory());
        m_assembler.lwInsn(temp.data(), resolution.base, Imm::I(resolution.offset));
        m_assembler.xorInsn(dest, temp.data(), dest);
        m_assembler.maskRegister<32>(dest);
    }

    void xor64(RegisterID src, RegisterID dest)
    {
        xor64(src, dest, dest);
    }

    void xor64(RegisterID op1, RegisterID op2, RegisterID dest)
    {
        m_assembler.xorInsn(dest, op1, op2);
    }

    void xor64(TrustedImm32 imm, RegisterID dest)
    {
        xor64(imm, dest, dest);
    }

    void xor64(TrustedImm32 imm, RegisterID op2, RegisterID dest)
    {
        if (Imm::isValid<Imm::IType>(imm.m_value)) {
            m_assembler.xoriInsn(dest, op2, Imm::I(imm.m_value));
            return;
        }

        auto temp = temps<Data>();
        loadImmediate(imm, temp.data());
        m_assembler.xorInsn(dest, temp.data(), op2);
    }

    void xor64(TrustedImm64 imm, RegisterID dest)
    {
        xor64(imm, dest, dest);
    }

    void xor64(TrustedImm64 imm, RegisterID op2, RegisterID dest)
    {
        if (Imm::isValid<Imm::IType>(imm.m_value)) {
            m_assembler.xoriInsn(dest, op2, Imm::I(imm.m_value));
            return;
        }

        auto temp = temps<Data>();
        loadImmediate(imm, temp.data());
        m_assembler.xorInsn(dest, temp.data(), op2);
    }

    void xor64(Address address, RegisterID dest)
    {
        auto temp = temps<Data, Memory>();
        auto resolution = resolveAddress(address, temp.memory());
        m_assembler.ldInsn(temp.data(), resolution.base, Imm::I(resolution.offset));
        m_assembler.xorInsn(dest, temp.data(), dest);
    }

    void xor64(RegisterID src, Address address)
    {
        auto temp = temps<Data, Memory>();
        auto resolution = resolveAddress(address, temp.memory());
        m_assembler.ldInsn(temp.data(), resolution.base, Imm::I(resolution.offset));
        m_assembler.xorInsn(temp.data(), src, temp.data());
        m_assembler.sdInsn(resolution.base, temp.data(), Imm::S(resolution.offset));
    }

    void not32(RegisterID dest)
    {
        not32(dest, dest);
    }

    void not32(RegisterID src, RegisterID dest)
    {
        m_assembler.xoriInsn(dest, src, Imm::I<-1>());
        m_assembler.maskRegister<32>(dest);
    }

    void not64(RegisterID dest)
    {
        not64(dest, dest);
    }

    void not64(RegisterID src, RegisterID dest)
    {
        m_assembler.xoriInsn(dest, src, Imm::I<-1>());
    }

    void neg32(RegisterID dest)
    {
        neg32(dest, dest);
    }

    void neg32(RegisterID src, RegisterID dest)
    {
        m_assembler.subwInsn(dest, RISCV64Registers::zero, src);
        m_assembler.maskRegister<32>(dest);
    }

    void neg64(RegisterID dest)
    {
        neg64(dest, dest);
    }

    void neg64(RegisterID src, RegisterID dest)
    {
        m_assembler.subInsn(dest, RISCV64Registers::zero, src);
    }

    void move(RegisterID src, RegisterID dest)
    {
        m_assembler.addiInsn(dest, src, Imm::I<0>());
    }

    void move(TrustedImm32 imm, RegisterID dest)
    {
        loadImmediate(imm, dest);
        m_assembler.maskRegister<32>(dest);
    }

    void move(TrustedImm64 imm, RegisterID dest)
    {
        loadImmediate(imm, dest);
    }

    void move(TrustedImmPtr imm, RegisterID dest)
    {
        loadImmediate(imm, dest);
    }

    void swap(RegisterID reg1, RegisterID reg2)
    {
        auto temp = temps<Data>();
        move(reg1, temp.data());
        move(reg2, reg1);
        move(temp.data(), reg2);
    }

    void swap(FPRegisterID reg1, FPRegisterID reg2)
    {
        moveDouble(reg1, fpTempRegister);
        moveDouble(reg2, reg1);
        moveDouble(fpTempRegister, reg2);
    }

    void moveZeroToFloat(FPRegisterID dest)
    {
        m_assembler.fcvtInsn<RISCV64Assembler::FCVTType::S, RISCV64Assembler::FCVTType::W>(dest, RISCV64Registers::zero);
    }

    void moveZeroToDouble(FPRegisterID dest)
    {
        m_assembler.fcvtInsn<RISCV64Assembler::FCVTType::D, RISCV64Assembler::FCVTType::L>(dest, RISCV64Registers::zero);
    }

    void moveFloat(FPRegisterID src, FPRegisterID dest)
    {
        m_assembler.fsgnjInsn<32>(dest, src, src);
    }

    void moveFloatTo32(FPRegisterID src, RegisterID dest)
    {
        m_assembler.fmvInsn<RISCV64Assembler::FMVType::X, RISCV64Assembler::FMVType::W>(dest, src);
    }

    void move32ToFloat(RegisterID src, FPRegisterID dest)
    {
        m_assembler.fmvInsn<RISCV64Assembler::FMVType::W, RISCV64Assembler::FMVType::X>(dest, src);
    }

    void moveDouble(FPRegisterID src, FPRegisterID dest)
    {
        m_assembler.fsgnjInsn<64>(dest, src, src);
    }

    void moveDoubleTo64(FPRegisterID src, RegisterID dest)
    {
        m_assembler.fmvInsn<RISCV64Assembler::FMVType::X, RISCV64Assembler::FMVType::D>(dest, src);
    }

    void move64ToDouble(RegisterID src, FPRegisterID dest)
    {
        m_assembler.fmvInsn<RISCV64Assembler::FMVType::D, RISCV64Assembler::FMVType::X>(dest, src);
    }

    // The RISC-V V vector extension is not yet standardized
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(moveVector);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(loadVector);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(storeVector);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(vectorReplaceLaneInt64);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(vectorReplaceLaneInt32);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(vectorReplaceLaneInt16);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(vectorReplaceLaneInt8);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(vectorReplaceLaneFloat64);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(vectorReplaceLaneFloat32);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(vectorExtractLaneInt64);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(vectorExtractLaneInt32);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(vectorExtractLaneSignedInt16);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(vectorExtractLaneUnsignedInt16);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(vectorExtractLaneSignedInt8);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(vectorExtractLaneUnsignedInt8);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(vectorExtractLaneFloat64);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(vectorExtractLaneFloat32);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(vectorSplat8);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(vectorSplat16);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(vectorSplat32);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(vectorSplat64);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(vectorSplatFloat32);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(vectorSplatFloat64);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(compareFloatingPointVector);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(compareIntegerVector);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(compareIntegerVectorWithZero);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(vectorAdd);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(vectorSub);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(vectorAddSat);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(vectorSubSat);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(vectorMul);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(vectorDiv);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(vectorMin);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(vectorMax);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(vectorPmin);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(vectorPmax);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(vectorNarrow);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(vectorBitwiseSelect);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(vectorNot);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(vectorAnd);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(vectorAndnot);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(vectorOr);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(vectorXor);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(vectorAbs);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(vectorNeg);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(vectorPopcnt);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(vectorCeil);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(vectorFloor);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(vectorTrunc);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(vectorTruncSat);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(vectorConvert);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(vectorConvertLow);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(vectorNearest);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(vectorSqrt);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(vectorExtendLow);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(vectorExtendHigh);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(vectorPromote);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(vectorDemote);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(vectorAnyTrue);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(vectorAllTrue);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(vectorBitmask);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(vectorExtaddPairwise);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(vectorAvgRound);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(vectorMulSat);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(vectorDotProductInt32);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(vectorSwizzle);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(vectorShuffle);

    template<PtrTag resultTag, PtrTag locationTag>
    static CodePtr<resultTag> readCallTarget(CodeLocationCall<locationTag> call)
    {
        return CodePtr<resultTag>(Assembler::readCallTarget(call.dataLocation()));
    }

    template<PtrTag tag>
    static void replaceWithVMHalt(CodeLocationLabel<tag> instructionStart)
    {
        Assembler::replaceWithVMHalt(instructionStart.dataLocation());
    }

    template<PtrTag startTag, PtrTag destTag>
    static void replaceWithJump(CodeLocationLabel<startTag> instructionStart, CodeLocationLabel<destTag> destination)
    {
        Assembler::replaceWithJump(instructionStart.dataLocation(), destination.dataLocation());
    }

    static ptrdiff_t maxJumpReplacementSize()
    {
        return Assembler::maxJumpReplacementSize();
    }

    static ptrdiff_t patchableJumpSize()
    {
        return Assembler::patchableJumpSize();
    }

    template<PtrTag tag>
    static CodeLocationLabel<tag> startOfBranchPtrWithPatchOnRegister(CodeLocationDataLabelPtr<tag> label)
    {
        return label.labelAtOffset(0);
    }

    template<PtrTag tag>
    static void revertJumpReplacementToBranchPtrWithPatch(CodeLocationLabel<tag> jump, RegisterID, void* initialValue)
    {
        Assembler::revertJumpReplacementToPatch(jump.dataLocation(), initialValue);
    }

    template<PtrTag tag>
    static void linkCall(void* code, Call call, CodePtr<tag> function)
    {
        if (!call.isFlagSet(Call::Near))
            Assembler::linkPointer(code, call.m_label, function.taggedPtr());
        else
            Assembler::linkCall(code, call.m_label, function.untaggedPtr());
    }

    template<PtrTag callTag, PtrTag destTag>
    static void repatchCall(CodeLocationCall<callTag> call, CodeLocationLabel<destTag> destination)
    {
        Assembler::repatchPointer(call.dataLocation(), destination.taggedPtr());
    }

    template<PtrTag callTag, PtrTag destTag>
    static void repatchCall(CodeLocationCall<callTag> call, CodePtr<destTag> destination)
    {
        Assembler::repatchPointer(call.dataLocation(), destination.taggedPtr());
    }

    Jump jump()
    {
        auto label = m_assembler.label();
        m_assembler.jumpPlaceholder(
            [&] {
                m_assembler.jalInsn(RISCV64Registers::zero, Imm::J<0>());
            });
        return Jump(label);
    }

    void farJump(RegisterID target, PtrTag)
    {
        m_assembler.jalrInsn(RISCV64Registers::zero, target, Imm::I<0>());
    }

    void farJump(AbsoluteAddress address, PtrTag)
    {
        auto temp = temps<Data>();
        loadImmediate(TrustedImmPtr(address.m_ptr), temp.data());
        m_assembler.ldInsn(temp.data(), temp.data(), Imm::I<0>());
        m_assembler.jalrInsn(RISCV64Registers::zero, temp.data(), Imm::I<0>());
    }

    void farJump(Address address, PtrTag)
    {
        auto temp = temps<Data, Memory>();
        auto resolution = resolveAddress(address, temp.memory());
        m_assembler.ldInsn(temp.data(), resolution.base, Imm::I(resolution.offset));
        m_assembler.jalrInsn(RISCV64Registers::zero, temp.data(), Imm::I<0>());
    }

    void farJump(BaseIndex address, PtrTag)
    {
        auto temp = temps<Data, Memory>();
        auto resolution = resolveAddress(address, temp.memory());
        m_assembler.ldInsn(temp.data(), resolution.base, Imm::I(resolution.offset));
        m_assembler.jalrInsn(RISCV64Registers::zero, temp.data(), Imm::I<0>());
    }

    void farJump(TrustedImmPtr imm, PtrTag)
    {
        auto temp = temps<Data>();
        loadImmediate(imm, temp.data());
        m_assembler.jalrInsn(RISCV64Registers::zero, temp.data(), Imm::I<0>());
    }

    void farJump(RegisterID target, RegisterID jumpTag)
    {
        UNUSED_PARAM(jumpTag);
        farJump(target, NoPtrTag);
    }

    void farJump(AbsoluteAddress address, RegisterID jumpTag)
    {
        UNUSED_PARAM(jumpTag);
        farJump(address, NoPtrTag);
    }

    void farJump(Address address, RegisterID jumpTag)
    {
        UNUSED_PARAM(jumpTag);
        farJump(address, NoPtrTag);
    }

    void farJump(BaseIndex address, RegisterID jumpTag)
    {
        UNUSED_PARAM(jumpTag);
        farJump(address, NoPtrTag);
    }

    Call nearCall()
    {
        auto label = m_assembler.label();
        m_assembler.nearCallPlaceholder(
            [&] {
                m_assembler.jalInsn(RISCV64Registers::x1, Imm::J<0>());
            });
        return Call(label, Call::LinkableNear);
    }

    Call nearTailCall()
    {
        auto label = m_assembler.label();
        m_assembler.nearCallPlaceholder(
            [&] {
                m_assembler.jalInsn(RISCV64Registers::zero, Imm::J<0>());
            });
        return Call(label, Call::LinkableNearTail);
    }

    Call threadSafePatchableNearCall()
    {
        auto label = m_assembler.label();
        m_assembler.nearCallPlaceholder(
            [&] {
                m_assembler.jalInsn(RISCV64Registers::x1, Imm::J<0>());
            });
        return Call(label, Call::LinkableNear);
    }

    void ret()
    {
        m_assembler.jalrInsn(RISCV64Registers::zero, RISCV64Registers::x1, Imm::I<0>());
    }

    void compare8(RelationalCondition cond, Address address, TrustedImm32 imm, RegisterID dest)
    {
        auto temp = temps<Data, Memory>();
        auto resolution = resolveAddress(address, temp.memory());
        m_assembler.lbInsn(temp.memory(), resolution.base, Imm::I(resolution.offset));
        loadImmediate(imm, temp.data());
        compareFinalize(cond, temp.memory(), temp.data(), dest);
    }

    void compare32(RelationalCondition cond, RegisterID lhs, RegisterID rhs, RegisterID dest)
    {
        auto temp = temps<Data, Memory>();
        m_assembler.signExtend<32>(temp.memory(), lhs);
        m_assembler.signExtend<32>(temp.data(), rhs);
        compareFinalize(cond, temp.memory(), temp.data(), dest);
    }

    void compare32(RelationalCondition cond, RegisterID lhs, TrustedImm32 imm, RegisterID dest)
    {
        auto temp = temps<Data, Memory>();
        m_assembler.signExtend<32>(temp.memory(), lhs);
        loadImmediate(imm, temp.data());
        compareFinalize(cond, temp.memory(), temp.data(), dest);
    }

    void compare32(RelationalCondition cond, Address address, RegisterID rhs, RegisterID dest)
    {
        auto temp = temps<Data, Memory>();
        auto resolution = resolveAddress(address, temp.memory());
        m_assembler.lwInsn(temp.memory(), resolution.base, Imm::I(resolution.offset));
        m_assembler.signExtend<32>(temp.data(), rhs);
        compareFinalize(cond, temp.memory(), temp.data(), dest);
    }

    void compare64(RelationalCondition cond, RegisterID lhs, RegisterID rhs, RegisterID dest)
    {
        compareFinalize(cond, lhs, rhs, dest);
    }

    void compare64(RelationalCondition cond, RegisterID lhs, TrustedImm32 imm, RegisterID dest)
    {
        auto temp = temps<Data>();
        loadImmediate(imm, temp.data());
        compareFinalize(cond, lhs, temp.data(), dest);
    }

    void test8(ResultCondition cond, Address address, TrustedImm32 imm, RegisterID dest)
    {
        auto temp = temps<Data, Memory>();
        auto resolution = resolveAddress(address, temp.memory());
        m_assembler.lbuInsn(temp.data(), resolution.base, Imm::I(resolution.offset));
        m_assembler.andiInsn(temp.data(), temp.data(), Imm::I(imm.m_value & 0xff));
        testFinalize(cond, temp.data(), dest);
    }

    void test32(ResultCondition cond, RegisterID lhs, RegisterID rhs, RegisterID dest)
    {
        auto temp = temps<Data>();
        m_assembler.andInsn(temp.data(), lhs, rhs);
        m_assembler.maskRegister<32>(temp.data());
        testFinalize(cond, temp.data(), dest);
    }

    void test32(ResultCondition cond, RegisterID lhs, TrustedImm32 imm, RegisterID dest)
    {
        auto temp = temps<Data>();
        if (!Imm::isValid<Imm::IType>(imm.m_value)) {
            loadImmediate(imm, temp.data());
            m_assembler.andInsn(temp.data(), lhs, temp.data());
        } else
            m_assembler.andiInsn(temp.data(), lhs, Imm::I(imm.m_value));
        m_assembler.maskRegister<32>(temp.data());
        testFinalize(cond, temp.data(), dest);
    }

    void test32(ResultCondition cond, Address address, TrustedImm32 imm, RegisterID dest)
    {
        auto temp = temps<Data, Memory>();
        auto resolution = resolveAddress(address, temp.memory());
        m_assembler.lwuInsn(temp.memory(), resolution.base, Imm::I(resolution.offset));

        if (!Imm::isValid<Imm::IType>(imm.m_value)) {
            loadImmediate(imm, temp.data());
            m_assembler.andInsn(temp.data(), temp.memory(), temp.data());
        } else
            m_assembler.andiInsn(temp.data(), temp.memory(), Imm::I(imm.m_value));
        testFinalize(cond, temp.data(), dest);
    }

    void test64(ResultCondition cond, RegisterID lhs, RegisterID rhs, RegisterID dest)
    {
        m_assembler.andInsn(dest, lhs, rhs);
        testFinalize(cond, dest, dest);
    }

    void test64(ResultCondition cond, RegisterID lhs, TrustedImm32 imm, RegisterID dest)
    {
        auto temp = temps<Data>();
        if (!Imm::isValid<Imm::IType>(imm.m_value)) {
            loadImmediate(imm, temp.data());
            m_assembler.andInsn(dest, lhs, temp.data());
        } else
            m_assembler.andiInsn(dest, lhs, Imm::I(imm.m_value));
        testFinalize(cond, dest, dest);
    }

    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(setCarry);

    Jump branch8(RelationalCondition cond, Address address, TrustedImm32 imm)
    {
        auto temp = temps<Data, Memory>();
        auto resolution = resolveAddress(address, temp.memory());
        m_assembler.lbInsn(temp.memory(), resolution.base, Imm::I(resolution.offset));
        loadImmediate(imm, temp.data());
        return makeBranch(cond, temp.memory(), temp.data());
    }

    Jump branch8(RelationalCondition cond, AbsoluteAddress address, TrustedImm32 imm)
    {
        auto temp = temps<Data, Memory>();
        loadImmediate(TrustedImmPtr(address.m_ptr), temp.memory());
        m_assembler.lbInsn(temp.memory(), temp.memory(), Imm::I<0>());
        loadImmediate(imm, temp.data());
        return makeBranch(cond, temp.memory(), temp.data());
    }

    Jump branch32(RelationalCondition cond, RegisterID lhs, RegisterID rhs)
    {
        auto temp = temps<Data, Memory>();
        m_assembler.signExtend<32>(temp.data(), lhs);
        m_assembler.signExtend<32>(temp.memory(), rhs);
        return makeBranch(cond, temp.data(), temp.memory());
    }

    Jump branch32(RelationalCondition cond, RegisterID lhs, TrustedImm32 imm)
    {
        auto temp = temps<Data, Memory>();
        m_assembler.signExtend<32>(temp.data(), lhs);
        loadImmediate(imm, temp.memory());
        return makeBranch(cond, temp.data(), temp.memory());
    }

    Jump branch32(RelationalCondition cond, RegisterID lhs, Address address)
    {
        auto temp = temps<Data, Memory>();
        auto resolution = resolveAddress(address, temp.memory());
        m_assembler.lwInsn(temp.memory(), resolution.base, Imm::I(resolution.offset));
        m_assembler.signExtend<32>(temp.data(), lhs);
        return makeBranch(cond, temp.data(), temp.memory());
    }

    Jump branch32(RelationalCondition cond, Address address, RegisterID rhs)
    {
        auto temp = temps<Data, Memory>();
        auto resolution = resolveAddress(address, temp.memory());
        m_assembler.lwInsn(temp.memory(), resolution.base, Imm::I(resolution.offset));
        m_assembler.signExtend<32>(temp.data(), rhs);
        return makeBranch(cond, temp.memory(), temp.data());
    }

    Jump branch32(RelationalCondition cond, Address address, TrustedImm32 imm)
    {
        auto temp = temps<Data, Memory>();
        auto resolution = resolveAddress(address, temp.memory());
        m_assembler.lwInsn(temp.memory(), resolution.base, Imm::I(resolution.offset));
        loadImmediate(imm, temp.data());
        return makeBranch(cond, temp.memory(), temp.data());
    }

    Jump branch32(RelationalCondition cond, AbsoluteAddress address, RegisterID rhs)
    {
        auto temp = temps<Data, Memory>();
        loadImmediate(TrustedImmPtr(address.m_ptr), temp.memory());
        m_assembler.lwInsn(temp.memory(), temp.memory(), Imm::I<0>());
        m_assembler.signExtend<32>(temp.data(), rhs);
        return makeBranch(cond, temp.memory(), temp.data());
    }

    Jump branch32(RelationalCondition cond, BaseIndex address, TrustedImm32 imm)
    {
        auto temp = temps<Data, Memory>();
        auto resolution = resolveAddress(address, temp.memory());
        m_assembler.lwInsn(temp.memory(), resolution.base, Imm::I(resolution.offset));
        loadImmediate(imm, temp.data());
        return makeBranch(cond, temp.memory(), temp.data());
    }

    Jump branch64(RelationalCondition cond, RegisterID lhs, RegisterID rhs)
    {
        return makeBranch(cond, lhs, rhs);
    }

    Jump branch64(RelationalCondition cond, RegisterID lhs, TrustedImm32 imm)
    {
        auto temp = temps<Data>();
        loadImmediate(imm, temp.data());
        return makeBranch(cond, lhs, temp.data());
    }

    Jump branch64(RelationalCondition cond, RegisterID lhs, TrustedImm64 imm)
    {
        auto temp = temps<Data>();
        loadImmediate(imm, temp.data());
        return makeBranch(cond, lhs, temp.data());
    }

    Jump branch64(RelationalCondition cond, RegisterID left, Imm64 right)
    {
        return branch64(cond, left, right.asTrustedImm64());
    }

    Jump branch64(RelationalCondition cond, RegisterID lhs, Address address)
    {
        auto temp = temps<Data, Memory>();
        auto resolution = resolveAddress(address, temp.memory());
        m_assembler.ldInsn(temp.data(), resolution.base, Imm::I(resolution.offset));
        return makeBranch(cond, lhs, temp.data());
    }

    Jump branch64(RelationalCondition cond, RegisterID lhs, AbsoluteAddress address)
    {
        auto temp = temps<Data, Memory>();
        loadImmediate(TrustedImmPtr(address.m_ptr), temp.memory());
        m_assembler.ldInsn(temp.data(), temp.memory(), Imm::I<0>());
        return makeBranch(cond, lhs, temp.data());
    }

    Jump branch64(RelationalCondition cond, Address address, RegisterID rhs)
    {
        auto temp = temps<Data, Memory>();
        auto resolution = resolveAddress(address, temp.memory());
        m_assembler.ldInsn(temp.data(), resolution.base, Imm::I(resolution.offset));
        return makeBranch(cond, temp.data(), rhs);
    }

    Jump branch64(RelationalCondition cond, Address address, TrustedImm32 imm)
    {
        auto temp = temps<Data, Memory>();
        auto resolution = resolveAddress(address, temp.memory());
        m_assembler.ldInsn(temp.memory(), resolution.base, Imm::I(resolution.offset));
        loadImmediate(imm, temp.data());
        return makeBranch(cond, temp.memory(), temp.data());
    }

    Jump branch64(RelationalCondition cond, Address address, TrustedImm64 imm)
    {
        auto temp = temps<Data, Memory>();
        auto resolution = resolveAddress(address, temp.memory());
        m_assembler.ldInsn(temp.memory(), resolution.base, Imm::I(resolution.offset));
        loadImmediate(imm, temp.data());
        return makeBranch(cond, temp.memory(), temp.data());
    }

    Jump branch64(RelationalCondition cond, AbsoluteAddress address, RegisterID rhs)
    {
        auto temp = temps<Data, Memory>();
        loadImmediate(TrustedImmPtr(address.m_ptr), temp.memory());
        m_assembler.ldInsn(temp.data(), temp.memory(), Imm::I<0>());
        return makeBranch(cond, temp.data(), rhs);
    }

    Jump branch64(RelationalCondition cond, AbsoluteAddress address, TrustedImm32 imm)
    {
        auto temp = temps<Data, Memory>();
        loadImmediate(TrustedImmPtr(address.m_ptr), temp.memory());
        m_assembler.ldInsn(temp.memory(), temp.memory(), Imm::I<0>());
        loadImmediate(imm, temp.data());
        return makeBranch(cond, temp.memory(), temp.data());
    }

    Jump branch64(RelationalCondition cond, AbsoluteAddress address, TrustedImm64 imm)
    {
        auto temp = temps<Data, Memory>();
        loadImmediate(TrustedImmPtr(address.m_ptr), temp.memory());
        m_assembler.ldInsn(temp.memory(), temp.memory(), Imm::I<0>());
        loadImmediate(imm, temp.data());
        return makeBranch(cond, temp.memory(), temp.data());
    }

    Jump branch64(RelationalCondition cond, BaseIndex address, RegisterID rhs)
    {
        auto temp = temps<Data, Memory>();
        auto resolution = resolveAddress(address, temp.memory());
        m_assembler.ldInsn(temp.data(), resolution.base, Imm::I(resolution.offset));
        return makeBranch(cond, temp.data(), rhs);
    }

    Jump branch64(RelationalCondition cond, Address left, Address right)
    {
        auto temp = temps<Data, Memory>();
        auto leftResolution = resolveAddress(left, temp.memory());
        m_assembler.ldInsn(temp.data(), leftResolution.base, Imm::I(leftResolution.offset));
        auto rightResolution = resolveAddress(right, temp.memory());
        m_assembler.ldInsn(temp.memory(), rightResolution.base, Imm::I(rightResolution.offset));
        return makeBranch(cond, temp.data(), temp.memory());
    }

    Jump branch32WithUnalignedHalfWords(RelationalCondition cond, BaseIndex address, TrustedImm32 imm)
    {
        return branch32(cond, address, imm);
    }

    Jump branchAdd32(ResultCondition cond, RegisterID src, RegisterID dest)
    {
        return branchAdd32(cond, dest, src, dest);
    }

    Jump branchAdd32(ResultCondition cond, RegisterID op1, RegisterID op2, RegisterID dest)
    {
        if (cond == Overflow)
            return branchForArithmeticOverflow<32, ArithmeticOperation::Addition>(op1, op2, dest);

        auto temp = temps<Data>();
        m_assembler.addwInsn(temp.data(), op1, op2);
        m_assembler.maskRegister<32>(dest, temp.data());
        return branchTestFinalize(cond, temp.data());
    }

    Jump branchAdd32(ResultCondition cond, TrustedImm32 imm, RegisterID dest)
    {
        return branchAdd32(cond, dest, imm, dest);
    }

    Jump branchAdd32(ResultCondition cond, RegisterID op1, TrustedImm32 imm, RegisterID dest)
    {
        if (cond == Overflow)
            return branchForArithmeticOverflow<32, ArithmeticOperation::Addition>(op1, imm, dest);

        auto temp = temps<Data>();
        if (!Imm::isValid<Imm::IType>(imm.m_value)) {
            loadImmediate(imm, temp.data());
            m_assembler.addwInsn(temp.data(), op1, temp.data());
        } else
            m_assembler.addiwInsn(temp.data(), op1, Imm::I(imm.m_value));
        m_assembler.maskRegister<32>(dest, temp.data());
        return branchTestFinalize(cond, temp.data());
    }

    Jump branchAdd32(ResultCondition cond, Address address, RegisterID dest)
    {
        auto temp = temps<Data, Memory>();
        auto resolution = resolveAddress(address, temp.memory());
        m_assembler.lwInsn(temp.memory(), resolution.base, Imm::I(resolution.offset));

        if (cond == Overflow)
            return branchForArithmeticOverflow<32, ArithmeticOperation::Addition>(dest, temp.memory(), dest);

        m_assembler.addwInsn(temp.data(), dest, temp.memory());
        m_assembler.maskRegister<32>(dest, temp.data());
        return branchTestFinalize(cond, temp.data());
    }

    Jump branchAdd32(ResultCondition cond, TrustedImm32 imm, AbsoluteAddress address)
    {
        auto temp = temps<Data, Memory>();
        loadImmediate(TrustedImmPtr(address.m_ptr), temp.memory());
        m_assembler.lwInsn(temp.data(), temp.memory(), Imm::I<0>());

        if (cond == Overflow) {
            auto branch = branchForArithmeticOverflow<32, ArithmeticOperation::Addition>(temp.data(), imm, temp.data());
            loadImmediate(TrustedImmPtr(address.m_ptr), temp.memory());
            m_assembler.swInsn(temp.memory(), temp.data(), Imm::S<0>());
            return branch;
        }

        if (!Imm::isValid<Imm::IType>(imm.m_value)) {
            loadImmediate(imm, temp.memory());
            m_assembler.addwInsn(temp.data(), temp.data(), temp.memory());
        } else
            m_assembler.addiwInsn(temp.data(), temp.data(), Imm::I(imm.m_value));

        loadImmediate(TrustedImmPtr(address.m_ptr), temp.memory());
        m_assembler.swInsn(temp.memory(), temp.data(), Imm::S<0>());
        return branchTestFinalize(cond, temp.data());
    }

    Jump branchAdd32(ResultCondition cond, TrustedImm32 imm, Address address)
    {
        auto temp = temps<Data, Memory>();
        auto resolution = resolveAddress(address, temp.memory());
        m_assembler.lwInsn(temp.data(), resolution.base, Imm::I(resolution.offset));

        if (cond == Overflow) {
            auto branch = branchForArithmeticOverflow<32, ArithmeticOperation::Addition>(temp.data(), imm, temp.data());
            resolution = resolveAddress(address, temp.memory());
            m_assembler.swInsn(resolution.base, temp.data(), Imm::S(resolution.offset));
            return branch;
        }

        if (!Imm::isValid<Imm::IType>(imm.m_value)) {
            loadImmediate(imm, temp.memory());
            m_assembler.addwInsn(temp.data(), temp.data(), temp.memory());
        } else
            m_assembler.addiwInsn(temp.data(), temp.data(), Imm::I(imm.m_value));

        resolution = resolveAddress(address, temp.memory());
        m_assembler.swInsn(resolution.base, temp.data(), Imm::S(resolution.offset));
        return branchTestFinalize(cond, temp.data());
    }

    Jump branchAdd64(ResultCondition cond, RegisterID src, RegisterID dest)
    {
        return branchAdd64(cond, dest, src, dest);
    }

    Jump branchAdd64(ResultCondition cond, RegisterID op1, RegisterID op2, RegisterID dest)
    {
        if (cond == Overflow)
            return branchForArithmeticOverflow<64, ArithmeticOperation::Addition>(op1, op2, dest);

        m_assembler.addInsn(dest, op1, op2);
        return branchTestFinalize(cond, dest);
    }

    Jump branchAdd64(ResultCondition cond, TrustedImm32 imm, RegisterID dest)
    {
        return branchAdd64(cond, dest, imm, dest);
    }

    Jump branchAdd64(ResultCondition cond, RegisterID op1, TrustedImm32 imm, RegisterID dest)
    {
        if (cond == Overflow)
            return branchForArithmeticOverflow<64, ArithmeticOperation::Addition>(op1, imm, dest);

        if (!Imm::isValid<Imm::IType>(imm.m_value)) {
            auto temp = temps<Data>();
            loadImmediate(imm, temp.data());
            m_assembler.addInsn(dest, op1, temp.data());
        } else
            m_assembler.addiInsn(dest, op1, Imm::I(imm.m_value));
        return branchTestFinalize(cond, dest);
    }

    Jump branchSub32(ResultCondition cond, RegisterID src, RegisterID dest)
    {
        return branchSub32(cond, dest, src, dest);
    }

    Jump branchSub32(ResultCondition cond, RegisterID op1, RegisterID op2, RegisterID dest)
    {
        if (cond == Overflow)
            return branchForArithmeticOverflow<32, ArithmeticOperation::Subtraction>(op1, op2, dest);

        auto temp = temps<Data>();
        m_assembler.subwInsn(temp.data(), op1, op2);
        m_assembler.maskRegister<32>(dest, temp.data());
        return branchTestFinalize(cond, temp.data());
    }

    Jump branchSub32(ResultCondition cond, TrustedImm32 imm, RegisterID dest)
    {
        return branchSub32(cond, dest, imm, dest);
    }

    Jump branchSub32(ResultCondition cond, RegisterID op1, TrustedImm32 imm, RegisterID dest)
    {
        return branchAdd32(cond, op1, TrustedImm32(-imm.m_value), dest);
    }

    Jump branchSub64(ResultCondition cond, RegisterID src, RegisterID dest)
    {
        return branchSub64(cond, dest, src, dest);
    }

    Jump branchSub64(ResultCondition cond, RegisterID op1, RegisterID op2, RegisterID dest)
    {
        if (cond == Overflow)
            return branchForArithmeticOverflow<64, ArithmeticOperation::Subtraction>(op1, op2, dest);

        m_assembler.subInsn(dest, op1, op2);
        return branchTestFinalize(cond, dest);
    }

    Jump branchSub64(ResultCondition cond, TrustedImm32 imm, RegisterID dest)
    {
        return branchSub64(cond, dest, imm, dest);
    }

    Jump branchSub64(ResultCondition cond, RegisterID op1, TrustedImm32 imm, RegisterID dest)
    {
        return branchAdd64(cond, op1, TrustedImm32(-imm.m_value), dest);
    }

    Jump branchMul32(ResultCondition cond, RegisterID src, RegisterID dest)
    {
        return branchMul32(cond, dest, src, dest);
    }

    Jump branchMul32(ResultCondition cond, RegisterID op1, RegisterID op2, RegisterID dest)
    {
        if (cond == Overflow)
            return branchForArithmeticOverflow<32, ArithmeticOperation::Multiplication>(op1, op2, dest);

        auto temp = temps<Data, Memory>();
        m_assembler.signExtend<32>(temp.memory(), op1);
        m_assembler.signExtend<32>(temp.data(), op2);
        m_assembler.mulInsn(temp.data(), temp.memory(), temp.data());
        m_assembler.maskRegister<32>(dest, temp.data());
        return branchTestFinalize(cond, temp.data());
    }

    Jump branchMul32(ResultCondition cond, TrustedImm32 imm, RegisterID dest)
    {
        return branchMul32(cond, dest, imm, dest);
    }

    Jump branchMul32(ResultCondition cond, RegisterID op1, TrustedImm32 imm, RegisterID dest)
    {
        if (cond == Overflow)
            return branchForArithmeticOverflow<32, ArithmeticOperation::Multiplication>(op1, imm, dest);

        auto temp = temps<Data, Memory>();
        m_assembler.signExtend<32>(temp.memory(), op1);
        loadImmediate(imm, temp.data());
        m_assembler.mulInsn(temp.data(), temp.memory(), temp.data());
        m_assembler.maskRegister<32>(dest, temp.data());
        return branchTestFinalize(cond, temp.data());
    }

    Jump branchMul64(ResultCondition cond, RegisterID src, RegisterID dest)
    {
        return branchMul64(cond, dest, src, dest);
    }

    Jump branchMul64(ResultCondition cond, RegisterID op1, RegisterID op2, RegisterID dest)
    {
        if (cond == Overflow)
            return branchForArithmeticOverflow<64, ArithmeticOperation::Multiplication>(op1, op2, dest);

        m_assembler.mulInsn(dest, op1, op2);
        return branchTestFinalize(cond, dest);
    }

    Jump branchMul64(ResultCondition cond, TrustedImm32 imm, RegisterID dest)
    {
        return branchMul64(cond, dest, imm, dest);
    }

    Jump branchMul64(ResultCondition cond, RegisterID op1, TrustedImm32 imm, RegisterID dest)
    {
        if (cond == Overflow)
            return branchForArithmeticOverflow<64, ArithmeticOperation::Multiplication>(op1, imm, dest);

        auto temp = temps<Data>();
        loadImmediate(imm, temp.data());
        m_assembler.mulInsn(dest, op1, temp.data());
        return branchTestFinalize(cond, dest);
    }

    Jump branchNeg32(ResultCondition cond, RegisterID reg)
    {
        return branchSub32(cond, RISCV64Registers::zero, reg, reg);
    }

    Jump branchNeg64(ResultCondition cond, RegisterID reg)
    {
        return branchSub64(cond, RISCV64Registers::zero, reg, reg);
    }

    Jump branchTest8(ResultCondition cond, Address address, TrustedImm32 imm = TrustedImm32(-1))
    {
        auto temp = temps<Data, Memory>();
        auto resolution = resolveAddress(address, temp.memory());
        m_assembler.lbuInsn(temp.memory(), resolution.base, Imm::I(resolution.offset));

        if (!Imm::isValid<Imm::IType>(imm.m_value)) {
            loadImmediate(imm, temp.data());
            m_assembler.andInsn(temp.data(), temp.memory(), temp.data());
        } else
            m_assembler.andiInsn(temp.data(), temp.memory(), Imm::I(imm.m_value));
        m_assembler.signExtend<8>(temp.data());
        return branchTestFinalize(cond, temp.data());
    }

    Jump branchTest8(ResultCondition cond, AbsoluteAddress address, TrustedImm32 imm = TrustedImm32(-1))
    {
        auto temp = temps<Data, Memory>();
        loadImmediate(TrustedImmPtr(address.m_ptr), temp.memory());
        m_assembler.lbuInsn(temp.memory(), temp.memory(), Imm::I<0>());

        if (!Imm::isValid<Imm::IType>(imm.m_value)) {
            loadImmediate(imm, temp.data());
            m_assembler.andInsn(temp.data(), temp.memory(), temp.data());
        } else
            m_assembler.andiInsn(temp.data(), temp.memory(), Imm::I(imm.m_value));
        m_assembler.signExtend<8>(temp.data());
        return branchTestFinalize(cond, temp.data());
    }

    Jump branchTest8(ResultCondition cond, BaseIndex address, TrustedImm32 imm = TrustedImm32(-1))
    {
        auto temp = temps<Data, Memory>();
        auto resolution = resolveAddress(address, temp.memory());
        m_assembler.lbuInsn(temp.memory(), resolution.base, Imm::I(resolution.offset));

        if (!Imm::isValid<Imm::IType>(imm.m_value)) {
            loadImmediate(imm, temp.data());
            m_assembler.andInsn(temp.data(), temp.memory(), temp.data());
        } else
            m_assembler.andiInsn(temp.data(), temp.memory(), Imm::I(imm.m_value));
        m_assembler.signExtend<8>(temp.data());
        return branchTestFinalize(cond, temp.data());
    }

    Jump branchTest8(ResultCondition cond, ExtendedAddress address, TrustedImm32 imm = TrustedImm32(-1))
    {
        auto temp = temps<Data, Memory>();
        auto resolution = resolveAddress(address, temp.memory());
        m_assembler.lbuInsn(temp.memory(), resolution.base, Imm::I(resolution.offset));

        if (!Imm::isValid<Imm::IType>(imm.m_value)) {
            loadImmediate(imm, temp.data());
            m_assembler.andInsn(temp.data(), temp.memory(), temp.data());
        } else
            m_assembler.andiInsn(temp.data(), temp.memory(), Imm::I(imm.m_value));
        m_assembler.signExtend<8>(temp.data());
        return branchTestFinalize(cond, temp.data());
    }

    Jump branchTest16(ResultCondition cond, Address address, TrustedImm32 imm = TrustedImm32(-1))
    {
        auto temp = temps<Data, Memory>();
        auto resolution = resolveAddress(address, temp.memory());
        m_assembler.lhuInsn(temp.memory(), resolution.base, Imm::I(resolution.offset));

        if (!Imm::isValid<Imm::IType>(imm.m_value)) {
            loadImmediate(imm, temp.data());
            m_assembler.andInsn(temp.data(), temp.memory(), temp.data());
        } else
            m_assembler.andiInsn(temp.data(), temp.memory(), Imm::I(imm.m_value));
        m_assembler.signExtend<16>(temp.data());
        return branchTestFinalize(cond, temp.data());
    }

    Jump branchTest16(ResultCondition cond, BaseIndex address, TrustedImm32 imm = TrustedImm32(-1))
    {
        auto temp = temps<Data, Memory>();
        auto resolution = resolveAddress(address, temp.memory());
        m_assembler.lhuInsn(temp.memory(), resolution.base, Imm::I(resolution.offset));

        if (!Imm::isValid<Imm::IType>(imm.m_value)) {
            loadImmediate(imm, temp.data());
            m_assembler.andInsn(temp.data(), temp.memory(), temp.data());
        } else
            m_assembler.andiInsn(temp.data(), temp.memory(), Imm::I(imm.m_value));
        m_assembler.signExtend<16>(temp.data());
        return branchTestFinalize(cond, temp.data());
    }

    Jump branchTest32(ResultCondition cond, RegisterID lhs, RegisterID rhs)
    {
        auto temp = temps<Data>();
        m_assembler.zeroExtend<32>(temp.data(), lhs);
        m_assembler.andInsn(temp.data(), temp.data(), rhs);
        m_assembler.signExtend<32>(temp.data());
        return branchTestFinalize(cond, temp.data());
    }

    Jump branchTest32(ResultCondition cond, RegisterID lhs, TrustedImm32 imm = TrustedImm32(-1))
    {
        auto temp = temps<Data, Memory>();
        m_assembler.zeroExtend<32>(temp.memory(), lhs);

        if (!Imm::isValid<Imm::IType>(imm.m_value)) {
            loadImmediate(imm, temp.data());
            m_assembler.andInsn(temp.data(), temp.memory(), temp.data());
        } else
            m_assembler.andiInsn(temp.data(), temp.memory(), Imm::I(imm.m_value));
        m_assembler.signExtend<32>(temp.data());
        return branchTestFinalize(cond, temp.data());
    }

    Jump branchTest32(ResultCondition cond, Address address, TrustedImm32 imm = TrustedImm32(-1))
    {
        auto temp = temps<Data, Memory>();
        auto resolution = resolveAddress(address, temp.memory());
        m_assembler.lwuInsn(temp.memory(), resolution.base, Imm::I(resolution.offset));

        if (!Imm::isValid<Imm::IType>(imm.m_value)) {
            loadImmediate(imm, temp.data());
            m_assembler.andInsn(temp.data(), temp.memory(), temp.data());
        } else
            m_assembler.andiInsn(temp.data(), temp.memory(), Imm::I(imm.m_value));
        m_assembler.signExtend<32>(temp.data());
        return branchTestFinalize(cond, temp.data());
    }

    Jump branchTest32(ResultCondition cond, AbsoluteAddress address, TrustedImm32 imm = TrustedImm32(-1))
    {
        auto temp = temps<Data, Memory>();
        loadImmediate(TrustedImmPtr(address.m_ptr), temp.memory());
        m_assembler.lwuInsn(temp.memory(), temp.memory(), Imm::I<0>());

        if (!Imm::isValid<Imm::IType>(imm.m_value)) {
            loadImmediate(imm, temp.data());
            m_assembler.andInsn(temp.data(), temp.memory(), temp.data());
        } else
            m_assembler.andiInsn(temp.data(), temp.memory(), Imm::I(imm.m_value));
        m_assembler.signExtend<32>(temp.data());
        return branchTestFinalize(cond, temp.data());
    }


    Jump branchTest64(ResultCondition cond, RegisterID lhs, RegisterID rhs)
    {
        auto temp = temps<Data>();
        m_assembler.andInsn(temp.data(), lhs, rhs);
        return branchTestFinalize(cond, temp.data());
    }

    Jump branchTest64(ResultCondition cond, RegisterID lhs, TrustedImm32 imm = TrustedImm32(-1))
    {
        auto temp = temps<Data>();
        if (!Imm::isValid<Imm::IType>(imm.m_value)) {
            loadImmediate(imm, temp.data());
            m_assembler.andInsn(temp.data(), lhs, temp.data());
        } else
            m_assembler.andiInsn(temp.data(), lhs, Imm::I(imm.m_value));
        return branchTestFinalize(cond, temp.data());
    }

    Jump branchTest64(ResultCondition cond, RegisterID lhs, TrustedImm64 imm)
    {
        auto temp = temps<Data>();
        if (!Imm::isValid<Imm::IType>(imm.m_value)) {
            loadImmediate(imm, temp.data());
            m_assembler.andInsn(temp.data(), lhs, temp.data());
        } else
            m_assembler.andiInsn(temp.data(), lhs, Imm::I(imm.m_value));
        return branchTestFinalize(cond, temp.data());
    }

    Jump branchTest64(ResultCondition cond, Address address, RegisterID rhs)
    {
        auto temp = temps<Data, Memory>();
        auto resolution = resolveAddress(address, temp.memory());
        m_assembler.ldInsn(temp.data(), resolution.base, Imm::I(resolution.offset));
        m_assembler.andInsn(temp.data(), temp.data(), rhs);
        return branchTestFinalize(cond, temp.data());
    }

    Jump branchTest64(ResultCondition cond, Address address, TrustedImm32 imm = TrustedImm32(-1))
    {
        auto temp = temps<Data, Memory>();
        auto resolution = resolveAddress(address, temp.memory());
        m_assembler.ldInsn(temp.memory(), resolution.base, Imm::I(resolution.offset));

        if (!Imm::isValid<Imm::IType>(imm.m_value)) {
            loadImmediate(imm, temp.data());
            m_assembler.andInsn(temp.data(), temp.memory(), temp.data());
        } else
            m_assembler.andiInsn(temp.data(), temp.memory(), Imm::I(imm.m_value));
        return branchTestFinalize(cond, temp.data());
    }

    Jump branchTest64(ResultCondition cond, AbsoluteAddress address, TrustedImm32 imm = TrustedImm32(-1))
    {
        auto temp = temps<Data, Memory>();
        loadImmediate(TrustedImmPtr(address.m_ptr), temp.memory());
        m_assembler.ldInsn(temp.memory(), temp.memory(), Imm::I<0>());

        if (!Imm::isValid<Imm::IType>(imm.m_value)) {
            loadImmediate(imm, temp.data());
            m_assembler.andInsn(temp.data(), temp.memory(), temp.data());
        } else
            m_assembler.andiInsn(temp.data(), temp.memory(), Imm::I(imm.m_value));
        return branchTestFinalize(cond, temp.data());
    }

    Jump branchTest64(ResultCondition cond, BaseIndex address, TrustedImm32 imm = TrustedImm32(-1))
    {
        auto temp = temps<Data, Memory>();
        auto resolution = resolveAddress(address, temp.memory());
        m_assembler.ldInsn(temp.memory(), resolution.base, Imm::I(resolution.offset));

        if (!Imm::isValid<Imm::IType>(imm.m_value)) {
            loadImmediate(imm, temp.data());
            m_assembler.andInsn(temp.data(), temp.memory(), temp.data());
        } else
            m_assembler.andiInsn(temp.data(), temp.memory(), Imm::I(imm.m_value));
        return branchTestFinalize(cond, temp.data());
    }

    Jump branchPtr(RelationalCondition cond, BaseIndex address, RegisterID rhs)
    {
        return branch64(cond, address, rhs);
    }

    Jump branchPtr(RelationalCondition cond, Address left, Address right)
    {
        return branch64(cond, left, right);
    }

    DataLabel32 moveWithPatch(TrustedImm32 imm, RegisterID dest)
    {
        RISCV64Assembler::ImmediateLoader imml(RISCV64Assembler::ImmediateLoader::Placeholder, imm.m_value);

        DataLabel32 label(this);
        imml.moveInto(m_assembler, dest);
        return label;
    }

    DataLabelPtr moveWithPatch(TrustedImmPtr imm, RegisterID dest)
    {
        RISCV64Assembler::ImmediateLoader imml(RISCV64Assembler::ImmediateLoader::Placeholder, int64_t(imm.asIntptr()));

        DataLabelPtr label(this);
        imml.moveInto(m_assembler, dest);
        return label;
    }

    DataLabelPtr storePtrWithPatch(TrustedImmPtr initialValue, Address address)
    {
        auto temp = temps<Data, Memory>();
        RISCV64Assembler::ImmediateLoader imml(RISCV64Assembler::ImmediateLoader::Placeholder, int64_t(initialValue.asIntptr()));
        DataLabelPtr label(this);
        imml.moveInto(m_assembler, temp.data());

        auto resolution = resolveAddress(address, temp.memory());
        m_assembler.sdInsn(resolution.base, temp.data(), Imm::S(resolution.offset));
        return label;
    }

    DataLabelPtr storePtrWithPatch(Address address)
    {
        return storePtrWithPatch(TrustedImmPtr(nullptr), address);
    }

    Jump branch32WithPatch(RelationalCondition cond, Address address, DataLabel32& dataLabel, TrustedImm32 initialRightValue = TrustedImm32(0))
    {
        auto temp = temps<Data, Memory>();
        auto resolution = resolveAddress(address, temp.memory());
        m_assembler.lwInsn(temp.memory(), resolution.base, Imm::I(resolution.offset));

        dataLabel = moveWithPatch(initialRightValue, temp.data());
        return makeBranch(cond, temp.memory(), temp.data());
    }

    Jump branchPtrWithPatch(RelationalCondition cond, Address address, DataLabelPtr& dataLabel, TrustedImmPtr initialRightValue = TrustedImmPtr(nullptr))
    {
        auto temp = temps<Data, Memory>();
        auto resolution = resolveAddress(address, temp.memory());
        m_assembler.ldInsn(temp.memory(), resolution.base, Imm::I(resolution.offset));

        dataLabel = moveWithPatch(initialRightValue, temp.data());
        return makeBranch(cond, temp.memory(), temp.data());
    }

    Jump branchPtrWithPatch(RelationalCondition cond, RegisterID lhs, DataLabelPtr& dataLabel, TrustedImmPtr initialRightValue = TrustedImmPtr(nullptr))
    {
        auto temp = temps<Data>();
        dataLabel = moveWithPatch(initialRightValue, temp.data());
        return makeBranch(cond, lhs, temp.data());
    }

    PatchableJump patchableBranch64(RelationalCondition cond, RegisterID left, RegisterID right)
    {
        return PatchableJump(branch64(cond, left, right));
    }

    Jump branchFloat(DoubleCondition cond, FPRegisterID lhs, FPRegisterID rhs)
    {
        return branchFP<32>(cond, lhs, rhs);
    }

    Jump branchDouble(DoubleCondition cond, FPRegisterID lhs, FPRegisterID rhs)
    {
        return branchFP<64>(cond, lhs, rhs);
    }

    Jump branchDoubleNonZero(FPRegisterID reg, FPRegisterID)
    {
        m_assembler.fcvtInsn<RISCV64Assembler::FCVTType::D, RISCV64Assembler::FCVTType::L>(fpTempRegister, RISCV64Registers::zero);
        return branchFP<64>(DoubleNotEqualAndOrdered, reg, fpTempRegister);
    }

    Jump branchDoubleZeroOrNaN(FPRegisterID reg, FPRegisterID)
    {
        m_assembler.fcvtInsn<RISCV64Assembler::FCVTType::D, RISCV64Assembler::FCVTType::L>(fpTempRegister, RISCV64Registers::zero);
        return branchFP<64>(DoubleEqualOrUnordered, reg, fpTempRegister);
    }

    enum BranchTruncateType { BranchIfTruncateFailed, BranchIfTruncateSuccessful };
    Jump branchTruncateDoubleToInt32(FPRegisterID src, RegisterID dest, BranchTruncateType branchType = BranchIfTruncateFailed)
    {
        auto temp = temps<Data>();
        m_assembler.fcvtInsn<RISCV64Assembler::FPRoundingMode::RTZ, RISCV64Assembler::FCVTType::L, RISCV64Assembler::FCVTType::D>(dest, src);
        m_assembler.signExtend<32>(temp.data(), dest);
        m_assembler.xorInsn(temp.data(), dest, temp.data());
        if (branchType == BranchIfTruncateFailed)
            m_assembler.sltuInsn(temp.data(), RISCV64Registers::zero, temp.data());
        else
            m_assembler.sltiuInsn(temp.data(), temp.data(), Imm::I<1>());

        m_assembler.maskRegister<32>(dest);
        return makeBranch(NotEqual, temp.data(), RISCV64Registers::zero);
    }

    void branchConvertDoubleToInt32(FPRegisterID src, RegisterID dest, JumpList& failureCases, FPRegisterID, bool negZeroCheck = true)
    {
        auto temp = temps<Data>();
        m_assembler.fcvtInsn<RISCV64Assembler::FCVTType::W, RISCV64Assembler::FCVTType::D>(temp.data(),  src);
        m_assembler.fcvtInsn<RISCV64Assembler::FCVTType::D, RISCV64Assembler::FCVTType::W>(fpTempRegister, temp.data());
        m_assembler.maskRegister<32>(dest, temp.data());
        failureCases.append(branchFP<64>(DoubleNotEqualOrUnordered, src, fpTempRegister));

        if (negZeroCheck) {
            Jump resultIsNonZero = makeBranch(NotEqual, temp.data(), RISCV64Registers::zero);
            m_assembler.fmvInsn<RISCV64Assembler::FMVType::X, RISCV64Assembler::FMVType::D>(temp.data(), src);
            failureCases.append(makeBranch(LessThan, temp.data(), RISCV64Registers::zero));
            resultIsNonZero.link(this);
        }
    }

    Call call(PtrTag)
    {
        auto label = m_assembler.label();
        m_assembler.pointerCallPlaceholder(
            [&] {
                auto temp = temps<Data>();
                m_assembler.addiInsn(temp.data(), RISCV64Registers::zero, Imm::I<0>());
                m_assembler.jalrInsn(RISCV64Registers::x1, temp.data(), Imm::I<0>());
            });
        return Call(label, Call::Linkable);
    }

    Call call(RegisterID target, PtrTag)
    {
        m_assembler.jalrInsn(RISCV64Registers::x1, target, Imm::I<0>());
        return Call(m_assembler.label(), Call::None);
    }

    Call call(Address address, PtrTag)
    {
        auto temp = temps<Data, Memory>();
        auto resolution = resolveAddress(address, temp.memory());
        m_assembler.ldInsn(temp.data(), resolution.base, Imm::I(resolution.offset));
        m_assembler.jalrInsn(RISCV64Registers::x1, temp.data(), Imm::I<0>());
        return Call(m_assembler.label(), Call::None);
    }

    Call call(RegisterID callTag) { UNUSED_PARAM(callTag); return call(NoPtrTag); }
    Call call(RegisterID target, RegisterID callTag) { UNUSED_PARAM(callTag); return call(target, NoPtrTag); }
    Call call(Address address, RegisterID callTag) { UNUSED_PARAM(callTag); return call(address, NoPtrTag); }

    void callOperation(const CodePtr<OperationPtrTag> operation)
    {
        auto temp = temps<Data>();
        loadImmediate(TrustedImmPtr(operation.taggedPtr()), temp.data());
        m_assembler.jalrInsn(RISCV64Registers::x1, temp.data(), Imm::I<0>());
    }

    void getEffectiveAddress(BaseIndex address, RegisterID dest)
    {
        auto resolution = resolveAddress(address, lazyTemp<Memory>());
        m_assembler.addiInsn(dest, resolution.base, Imm::I(resolution.offset));
    }

    void loadFloat(Address address, FPRegisterID dest)
    {
        auto resolution = resolveAddress(address, lazyTemp<Memory>());
        m_assembler.flwInsn(dest, resolution.base, Imm::I(resolution.offset));
    }

    void loadFloat(BaseIndex address, FPRegisterID dest)
    {
        auto resolution = resolveAddress(address, lazyTemp<Memory>());
        m_assembler.flwInsn(dest, resolution.base, Imm::I(resolution.offset));
    }

    void loadFloat(TrustedImmPtr address, FPRegisterID dest)
    {
        auto temp = temps<Memory>();
        loadImmediate(address, temp.memory());
        m_assembler.flwInsn(dest, temp.memory(), Imm::I<0>());
    }

    void loadDouble(Address address, FPRegisterID dest)
    {
        auto resolution = resolveAddress(address, lazyTemp<Memory>());
        m_assembler.fldInsn(dest, resolution.base, Imm::I(resolution.offset));
    }

    void loadDouble(BaseIndex address, FPRegisterID dest)
    {
        auto resolution = resolveAddress(address, lazyTemp<Memory>());
        m_assembler.fldInsn(dest, resolution.base, Imm::I(resolution.offset));
    }

    void loadDouble(TrustedImmPtr address, FPRegisterID dest)
    {
        auto temp = temps<Memory>();
        loadImmediate(address, temp.memory());
        m_assembler.fldInsn(dest, temp.memory(), Imm::I<0>());
    }

    void storeFloat(FPRegisterID src, Address address)
    {
        auto resolution = resolveAddress(address, lazyTemp<Memory>());
        m_assembler.fswInsn(resolution.base, src, Imm::S(resolution.offset));
    }

    void storeFloat(FPRegisterID src, BaseIndex address)
    {
        auto resolution = resolveAddress(address, lazyTemp<Memory>());
        m_assembler.fswInsn(resolution.base, src, Imm::S(resolution.offset));
    }

    void storeFloat(FPRegisterID src, TrustedImmPtr address)
    {
        auto temp = temps<Memory>();
        loadImmediate(address, temp.memory());
        m_assembler.fswInsn(temp.memory(), src, Imm::S<0>());
    }

    void storeDouble(FPRegisterID src, Address address)
    {
        auto resolution = resolveAddress(address, lazyTemp<Memory>());
        m_assembler.fsdInsn(resolution.base, src, Imm::S(resolution.offset));
    }

    void storeDouble(FPRegisterID src, BaseIndex address)
    {
        auto resolution = resolveAddress(address, lazyTemp<Memory>());
        m_assembler.fsdInsn(resolution.base, src, Imm::S(resolution.offset));
    }

    void storeDouble(FPRegisterID src, TrustedImmPtr address)
    {
        auto temp = temps<Memory>();
        loadImmediate(address, temp.memory());
        m_assembler.fsdInsn(temp.memory(), src, Imm::S<0>());
    }

    void addFloat(FPRegisterID op1, FPRegisterID op2, FPRegisterID dest)
    {
        m_assembler.faddInsn<32>(dest, op1, op2);
    }

    void addDouble(FPRegisterID src, FPRegisterID dest)
    {
        addDouble(src, dest, dest);
    }

    void addDouble(FPRegisterID op1, FPRegisterID op2, FPRegisterID dest)
    {
        m_assembler.faddInsn<64>(dest, op1, op2);
    }

    void addDouble(AbsoluteAddress address, FPRegisterID dest)
    {
        loadDouble(TrustedImmPtr(address.m_ptr), fpTempRegister);
        m_assembler.faddInsn<64>(dest, fpTempRegister, dest);
    }

    void subFloat(FPRegisterID op1, FPRegisterID op2, FPRegisterID dest)
    {
        m_assembler.fsubInsn<32>(dest, op1, op2);
    }

    void subDouble(FPRegisterID src, FPRegisterID dest)
    {
        subDouble(dest, src, dest);
    }

    void subDouble(FPRegisterID op1, FPRegisterID op2, FPRegisterID dest)
    {
        m_assembler.fsubInsn<64>(dest, op1, op2);
    }

    void mulFloat(FPRegisterID src, FPRegisterID dest)
    {
        mulFloat(src, dest, dest);
    }

    void mulFloat(FPRegisterID op1, FPRegisterID op2, FPRegisterID dest)
    {
        m_assembler.fmulInsn<32>(dest, op1, op2);
    }

    void mulDouble(FPRegisterID src, FPRegisterID dest)
    {
        mulDouble(src, dest, dest);
    }

    void mulDouble(FPRegisterID op1, FPRegisterID op2, FPRegisterID dest)
    {
        m_assembler.fmulInsn<64>(dest, op1, op2);
    }

    void mulDouble(Address address, FPRegisterID dest)
    {
        loadDouble(address, fpTempRegister);
        mulDouble(fpTempRegister, dest, dest);
    }

    void divFloat(FPRegisterID src, FPRegisterID dest)
    {
        divFloat(dest, src, dest);
    }

    void divFloat(FPRegisterID op1, FPRegisterID op2, FPRegisterID dest)
    {
        m_assembler.fdivInsn<32>(dest, op1, op2);
    }

    void divDouble(FPRegisterID src, FPRegisterID dest)
    {
        divDouble(dest, src, dest);
    }

    void divDouble(FPRegisterID op1, FPRegisterID op2, FPRegisterID dest)
    {
        m_assembler.fdivInsn<64>(dest, op1, op2);
    }

    void sqrtFloat(FPRegisterID src, FPRegisterID dest)
    {
        m_assembler.fsqrtInsn<32>(dest, src);
    }

    void sqrtDouble(FPRegisterID src, FPRegisterID dest)
    {
        m_assembler.fsqrtInsn<64>(dest, src);
    }

    void absFloat(FPRegisterID src, FPRegisterID dest)
    {
        m_assembler.fsgnjxInsn<32>(dest, src, src);
    }

    void absDouble(FPRegisterID src, FPRegisterID dest)
    {
        m_assembler.fsgnjxInsn<64>(dest, src, src);
    }

    void ceilFloat(FPRegisterID src, FPRegisterID dest)
    {
        roundFP<32, RISCV64Assembler::FPRoundingMode::RUP>(src, dest);
    }

    void ceilDouble(FPRegisterID src, FPRegisterID dest)
    {
        roundFP<64, RISCV64Assembler::FPRoundingMode::RUP>(src, dest);
    }

    void floorFloat(FPRegisterID src, FPRegisterID dest)
    {
        roundFP<32, RISCV64Assembler::FPRoundingMode::RDN>(src, dest);
    }

    void floorDouble(FPRegisterID src, FPRegisterID dest)
    {
        roundFP<64, RISCV64Assembler::FPRoundingMode::RDN>(src, dest);
    }

    void roundTowardNearestIntFloat(FPRegisterID src, FPRegisterID dest)
    {
        roundFP<32, RISCV64Assembler::FPRoundingMode::RNE>(src, dest);
    }

    void roundTowardNearestIntDouble(FPRegisterID src, FPRegisterID dest)
    {
        roundFP<64, RISCV64Assembler::FPRoundingMode::RNE>(src, dest);
    }

    void roundTowardZeroFloat(FPRegisterID src, FPRegisterID dest)
    {
        roundFP<32, RISCV64Assembler::FPRoundingMode::RTZ>(src, dest);
    }

    void roundTowardZeroDouble(FPRegisterID src, FPRegisterID dest)
    {
        roundFP<64, RISCV64Assembler::FPRoundingMode::RTZ>(src, dest);
    }

    void andFloat(FPRegisterID op1, FPRegisterID op2, FPRegisterID dest)
    {
        auto temp = temps<Data, Memory>();
        m_assembler.fmvInsn<RISCV64Assembler::FMVType::X, RISCV64Assembler::FMVType::W>(temp.data(), op1);
        m_assembler.fmvInsn<RISCV64Assembler::FMVType::X, RISCV64Assembler::FMVType::W>(temp.memory(), op2);
        m_assembler.andInsn(temp.data(), temp.data(), temp.memory());
        m_assembler.fmvInsn<RISCV64Assembler::FMVType::W, RISCV64Assembler::FMVType::X>(dest, temp.data());
    }

    void andDouble(FPRegisterID op1, FPRegisterID op2, FPRegisterID dest)
    {
        auto temp = temps<Data, Memory>();
        m_assembler.fmvInsn<RISCV64Assembler::FMVType::X, RISCV64Assembler::FMVType::D>(temp.data(), op1);
        m_assembler.fmvInsn<RISCV64Assembler::FMVType::X, RISCV64Assembler::FMVType::D>(temp.memory(), op2);
        m_assembler.andInsn(temp.data(), temp.data(), temp.memory());
        m_assembler.fmvInsn<RISCV64Assembler::FMVType::D, RISCV64Assembler::FMVType::X>(dest, temp.data());
    }

    void orFloat(FPRegisterID op1, FPRegisterID op2, FPRegisterID dest)
    {
        auto temp = temps<Data, Memory>();
        m_assembler.fmvInsn<RISCV64Assembler::FMVType::X, RISCV64Assembler::FMVType::W>(temp.data(), op1);
        m_assembler.fmvInsn<RISCV64Assembler::FMVType::X, RISCV64Assembler::FMVType::W>(temp.memory(), op2);
        m_assembler.orInsn(temp.data(), temp.data(), temp.memory());
        m_assembler.fmvInsn<RISCV64Assembler::FMVType::W, RISCV64Assembler::FMVType::X>(dest, temp.data());
    }

    void orDouble(FPRegisterID op1, FPRegisterID op2, FPRegisterID dest)
    {
        auto temp = temps<Data, Memory>();
        m_assembler.fmvInsn<RISCV64Assembler::FMVType::X, RISCV64Assembler::FMVType::D>(temp.data(), op1);
        m_assembler.fmvInsn<RISCV64Assembler::FMVType::X, RISCV64Assembler::FMVType::D>(temp.memory(), op2);
        m_assembler.orInsn(temp.data(), temp.data(), temp.memory());
        m_assembler.fmvInsn<RISCV64Assembler::FMVType::D, RISCV64Assembler::FMVType::X>(dest, temp.data());
    }

    void negateFloat(FPRegisterID src, FPRegisterID dest)
    {
        m_assembler.fsgnjnInsn<32>(dest, src, src);
    }

    void negateDouble(FPRegisterID src, FPRegisterID dest)
    {
        m_assembler.fsgnjnInsn<64>(dest, src, src);
    }

    void compareFloat(DoubleCondition cond, FPRegisterID lhs, FPRegisterID rhs, RegisterID dest)
    {
        compareFP<32>(cond, lhs, rhs, dest);
    }

    void compareDouble(DoubleCondition cond, FPRegisterID lhs, FPRegisterID rhs, RegisterID dest)
    {
        compareFP<64>(cond, lhs, rhs, dest);
    }

    void convertInt32ToFloat(RegisterID src, FPRegisterID dest)
    {
        m_assembler.fcvtInsn<RISCV64Assembler::FCVTType::S, RISCV64Assembler::FCVTType::W>(dest, src);
    }

    void convertInt32ToDouble(RegisterID src, FPRegisterID dest)
    {
        m_assembler.fcvtInsn<RISCV64Assembler::FCVTType::D, RISCV64Assembler::FCVTType::W>(dest, src);
    }

    void convertInt32ToDouble(TrustedImm32 imm, FPRegisterID dest)
    {
        auto temp = temps<Data>();
        loadImmediate(imm, temp.data());
        convertInt32ToDouble(temp.data(), dest);
    }

    void convertInt64ToFloat(RegisterID src, FPRegisterID dest)
    {
        m_assembler.fcvtInsn<RISCV64Assembler::FCVTType::S, RISCV64Assembler::FCVTType::L>(dest, src);
    }

    void convertInt64ToDouble(RegisterID src, FPRegisterID dest)
    {
        m_assembler.fcvtInsn<RISCV64Assembler::FCVTType::D, RISCV64Assembler::FCVTType::L>(dest, src);
    }

    void convertUInt64ToFloat(RegisterID src, FPRegisterID dest)
    {
        m_assembler.fcvtInsn<RISCV64Assembler::FCVTType::S, RISCV64Assembler::FCVTType::LU>(dest, src);
    }

    void convertUInt64ToDouble(RegisterID src, FPRegisterID dest)
    {
        m_assembler.fcvtInsn<RISCV64Assembler::FCVTType::D, RISCV64Assembler::FCVTType::LU>(dest, src);
    }

    void convertFloatToDouble(FPRegisterID src, FPRegisterID dest)
    {
        auto temp = temps<Data>();
        m_assembler.fmvInsn<RISCV64Assembler::FMVType::X, RISCV64Assembler::FMVType::W>(temp.data(), src);
        m_assembler.fmvInsn<RISCV64Assembler::FMVType::W, RISCV64Assembler::FMVType::X>(dest, temp.data());
        m_assembler.fcvtInsn<RISCV64Assembler::FCVTType::D, RISCV64Assembler::FCVTType::S>(dest, dest);
    }

    void convertDoubleToFloat(FPRegisterID src, FPRegisterID dest)
    {
        m_assembler.fcvtInsn<RISCV64Assembler::FCVTType::S, RISCV64Assembler::FCVTType::D>(dest, src);
    }

    void truncateFloatToInt32(FPRegisterID src, RegisterID dest)
    {
        m_assembler.fcvtInsn<RISCV64Assembler::FPRoundingMode::RTZ, RISCV64Assembler::FCVTType::W, RISCV64Assembler::FCVTType::S>(dest, src);
        m_assembler.maskRegister<32>(dest);
    }

    void truncateFloatToUint32(FPRegisterID src, RegisterID dest)
    {
        m_assembler.fcvtInsn<RISCV64Assembler::FPRoundingMode::RTZ, RISCV64Assembler::FCVTType::WU, RISCV64Assembler::FCVTType::S>(dest, src);
        m_assembler.maskRegister<32>(dest);
    }

    void truncateFloatToInt64(FPRegisterID src, RegisterID dest)
    {
        m_assembler.fcvtInsn<RISCV64Assembler::FPRoundingMode::RTZ, RISCV64Assembler::FCVTType::L, RISCV64Assembler::FCVTType::S>(dest, src);
    }

    void truncateFloatToUint64(FPRegisterID src, RegisterID dest)
    {
        m_assembler.fcvtInsn<RISCV64Assembler::FPRoundingMode::RTZ, RISCV64Assembler::FCVTType::LU, RISCV64Assembler::FCVTType::S>(dest, src);
    }

    void truncateFloatToUint64(FPRegisterID src, RegisterID dest, FPRegisterID, FPRegisterID)
    {
        truncateFloatToUint64(src, dest);
    }

    void truncateDoubleToInt32(FPRegisterID src, RegisterID dest)
    {
        m_assembler.fcvtInsn<RISCV64Assembler::FPRoundingMode::RTZ, RISCV64Assembler::FCVTType::W, RISCV64Assembler::FCVTType::D>(dest, src);
        m_assembler.maskRegister<32>(dest);
    }

    void truncateDoubleToUint32(FPRegisterID src, RegisterID dest)
    {
        m_assembler.fcvtInsn<RISCV64Assembler::FPRoundingMode::RTZ, RISCV64Assembler::FCVTType::WU, RISCV64Assembler::FCVTType::D>(dest, src);
        m_assembler.maskRegister<32>(dest);
    }

    void truncateDoubleToInt64(FPRegisterID src, RegisterID dest)
    {
        m_assembler.fcvtInsn<RISCV64Assembler::FPRoundingMode::RTZ, RISCV64Assembler::FCVTType::L, RISCV64Assembler::FCVTType::D>(dest, src);
    }

    void truncateDoubleToUint64(FPRegisterID src, RegisterID dest)
    {
        m_assembler.fcvtInsn<RISCV64Assembler::FPRoundingMode::RTZ, RISCV64Assembler::FCVTType::LU, RISCV64Assembler::FCVTType::D>(dest, src);
    }

    void truncateDoubleToUint64(FPRegisterID src, RegisterID dest, FPRegisterID, FPRegisterID)
    {
        truncateDoubleToUint64(src, dest);
    }

    void push(RegisterID src)
    {
        m_assembler.addiInsn(RISCV64Registers::sp, RISCV64Registers::sp, Imm::I<-8>());
        m_assembler.sdInsn(RISCV64Registers::sp, src, Imm::S<0>());
    }

    void push(TrustedImm32 imm)
    {
        auto temp = temps<Data>();
        loadImmediate(imm, temp.data());
        m_assembler.addiInsn(RISCV64Registers::sp, RISCV64Registers::sp, Imm::I<-8>());
        m_assembler.sdInsn(RISCV64Registers::sp, temp.data(), Imm::S<0>());
    }

    void pushPair(RegisterID src1, RegisterID src2)
    {
        m_assembler.addiInsn(RISCV64Registers::sp, RISCV64Registers::sp, Imm::I<-16>());
        m_assembler.sdInsn(RISCV64Registers::sp, src1, Imm::S<0>());
        m_assembler.sdInsn(RISCV64Registers::sp, src2, Imm::S<8>());
    }

    void pop(RegisterID dest)
    {
        m_assembler.ldInsn(dest, RISCV64Registers::sp, Imm::I<0>());
        m_assembler.addiInsn(RISCV64Registers::sp, RISCV64Registers::sp, Imm::I<8>());
    }

    void popPair(RegisterID dest1, RegisterID dest2)
    {
        m_assembler.ldInsn(dest1, RISCV64Registers::sp, Imm::I<0>());
        m_assembler.ldInsn(dest2, RISCV64Registers::sp, Imm::I<8>());
        m_assembler.addiInsn(RISCV64Registers::sp, RISCV64Registers::sp, Imm::I<16>());
    }

    void abortWithReason(AbortReason reason)
    {
        auto temp = temps<Data>();
        loadImmediate(TrustedImm32(reason), temp.data());
        m_assembler.ebreakInsn();
    }

    void abortWithReason(AbortReason reason, intptr_t misc)
    {
        auto temp = temps<Data, Memory>();
        loadImmediate(TrustedImm32(reason), temp.data());
        loadImmediate(TrustedImm64(misc), temp.memory());
        m_assembler.ebreakInsn();
    }

    void breakpoint(uint16_t = 0xc471)
    {
        m_assembler.ebreakInsn();
    }

    void nop()
    {
        m_assembler.addiInsn(RISCV64Registers::zero, RISCV64Registers::zero, Imm::I<0>());
    }

    void memoryFence()
    {
        m_assembler.fenceInsn({ RISCV64Assembler::MemoryOperation::RW }, { RISCV64Assembler::MemoryOperation::RW });
    }

    void storeFence()
    {
        m_assembler.fenceInsn({ RISCV64Assembler::MemoryOperation::W }, { RISCV64Assembler::MemoryOperation::RW });
    }

    void loadFence()
    {
        m_assembler.fenceInsn({ RISCV64Assembler::MemoryOperation::R }, { RISCV64Assembler::MemoryOperation::RW });
    }

    template<unsigned bitSize>
    JumpList branchAtomicWeakCASImpl(StatusCondition cond, RegisterID expectedAndClobbered, RegisterID newValue, BaseIndex address)
    {
        static_assert(bitSize == 8 || bitSize == 16);
        // There's no 8-bit or 16-bit load-reserved and store-conditional instructions in RISC-V,
        // so we have to implement the operations through the 32-bit versions, with a limited amount
        // of usable registers.

        auto temp = temps<Data, Memory>();
        JumpList failure;

        // We clobber the expected-value register with the XOR difference between the expected
        // and the new value, also clipping the result to the desired number of bits.
        m_assembler.xorInsn(expectedAndClobbered, expectedAndClobbered, newValue);
        m_assembler.zeroExtend<bitSize>(expectedAndClobbered);

        // The BaseIndex address is resolved into the memory temp. The address is aligned to the 4-byte
        // boundary, and the remainder is used to calculate the shift amount for the exact position
        // in the 32-bit word where the target bit pattern is located.
        auto resolution = resolveAddress(address, temp.memory());
        m_assembler.addiInsn(temp.memory(), resolution.base, Imm::I(resolution.offset));
        m_assembler.andiInsn(temp.data(), temp.memory(), Imm::I<0b11>());
        m_assembler.andiInsn(temp.memory(), temp.memory(), Imm::I<~0b11>());
        m_assembler.slliInsn<3>(temp.data(), temp.data());
        m_assembler.addiInsn(temp.data(), temp.data(), Imm::I<32>());

        // The XOR value in the expected-value register is shifted into the appropriate position in
        // the upper half of the register. The shift value is OR-ed into the lower half.
        m_assembler.sllInsn(expectedAndClobbered, expectedAndClobbered, temp.data());
        m_assembler.orInsn(expectedAndClobbered, expectedAndClobbered, temp.data());

        // The 32-bit value is loaded through the load-reserve instruction, and then shifted into the
        // upper 32 bits of the register. XOR against the expected-value register will, in the upper
        // 32 bits of the register, produce the 32-bit word with the expected value replaced by the new one.
        m_assembler.lrwInsn(temp.data(), temp.memory(), { RISCV64Assembler::MemoryAccess::Acquire });
        m_assembler.slliInsn<32>(temp.data(), temp.data());
        m_assembler.xorInsn(expectedAndClobbered, temp.data(), expectedAndClobbered);

        // We still have to validate that the expected value, after XOR, matches the new one. The upper
        // 32 bits of the expected-value register are shifted by the pre-prepared shift amount stored
        // in the lower half of that same register. This works becasue the shift amount is read only from
        // the bottom 6 bits of the shift-amount register. XOR-ing against the new-value register and shifting
        // back left should leave is with a zero value, in which case the expected-value bit pattern matched
        // the one that was loaded from memory. If non-zero, the failure branch is taken.
        m_assembler.srlInsn(temp.data(), expectedAndClobbered, expectedAndClobbered);
        m_assembler.xorInsn(temp.data(), temp.data(), newValue);
        m_assembler.slliInsn<64 - bitSize>(temp.data(), temp.data());
        failure.append(makeBranch(NotEqual, temp.data(), RISCV64Registers::zero));

        // The corresponding store-conditional remains. The 32-bit word, containing the new value after
        // the XOR, is located in the upper 32 bits of the expected-value register. That can be shifted
        // down and then used in the store-conditional instruction.
        m_assembler.srliInsn<32>(expectedAndClobbered, expectedAndClobbered);
        m_assembler.scwInsn(temp.data(), temp.memory(), expectedAndClobbered, { RISCV64Assembler::MemoryAccess::AcquireRelease });

        // On successful store, the temp register will have a zero value, and a non-zero value otherwise.
        // Branches are produced accordingly.
        switch (cond) {
        case Success: {
            Jump success = makeBranch(Equal, temp.data(), RISCV64Registers::zero);
            failure.link(this);
            return JumpList(success);
        }
        case Failure:
            failure.append(makeBranch(NotEqual, temp.data(), RISCV64Registers::zero));
            break;
        }

        return failure;
    }

    JumpList branchAtomicWeakCAS8(StatusCondition cond, RegisterID expectedAndClobbered, RegisterID newValue, BaseIndex address)
    {
        return branchAtomicWeakCASImpl<8>(cond, expectedAndClobbered, newValue, address);
    }

    JumpList branchAtomicWeakCAS16(StatusCondition cond, RegisterID expectedAndClobbered, RegisterID newValue, BaseIndex address)
    {
        return branchAtomicWeakCASImpl<16>(cond, expectedAndClobbered, newValue, address);
    }

    JumpList branchAtomicWeakCAS32(StatusCondition cond, RegisterID expectedAndClobbered, RegisterID newValue, BaseIndex address)
    {
        auto temp = temps<Data, Memory>();
        JumpList failure;

        auto resolution = resolveAddress(address, temp.memory());
        m_assembler.addiInsn(temp.memory(), resolution.base, Imm::I(resolution.offset));
        m_assembler.zeroExtend<32>(expectedAndClobbered, expectedAndClobbered);

        m_assembler.lrwInsn(temp.data(), temp.memory(), { RISCV64Assembler::MemoryAccess::Acquire });
        m_assembler.xorInsn(temp.data(), temp.data(), expectedAndClobbered);
        failure.append(makeBranch(NotEqual, temp.data(), RISCV64Registers::zero));
        m_assembler.scwInsn(temp.data(), temp.memory(), newValue, { RISCV64Assembler::MemoryAccess::AcquireRelease });

        switch (cond) {
        case Success: {
            Jump success = makeBranch(Equal, temp.data(), RISCV64Registers::zero);
            failure.link(this);
            return JumpList(success);
        }
        case Failure:
            failure.append(makeBranch(NotEqual, temp.data(), RISCV64Registers::zero));
            break;
        }

        return failure;
    }

    JumpList branchAtomicWeakCAS64(StatusCondition cond, RegisterID expectedAndClobbered, RegisterID newValue, BaseIndex address)
    {
        auto temp = temps<Data, Memory>();
        JumpList failure;

        auto resolution = resolveAddress(address, temp.memory());
        m_assembler.addiInsn(temp.memory(), resolution.base, Imm::I(resolution.offset));

        m_assembler.lrdInsn(temp.data(), temp.memory(), { RISCV64Assembler::MemoryAccess::Acquire });
        failure.append(makeBranch(NotEqual, temp.data(), expectedAndClobbered));
        m_assembler.scdInsn(temp.data(), temp.memory(), newValue, { RISCV64Assembler::MemoryAccess::AcquireRelease });

        switch (cond) {
        case Success: {
            Jump success = makeBranch(Equal, temp.data(), RISCV64Registers::zero);
            failure.link(this);
            return JumpList(success);
        }
        case Failure:
            failure.append(makeBranch(NotEqual, temp.data(), RISCV64Registers::zero));
            break;
        }

        return failure;
    }

    void atomicLoad32(Address address, RegisterID dest)
    {
        auto resolution = resolveAddress(address, lazyTemp<Memory>());
        memoryFence();
        m_assembler.lwInsn(dest, resolution.base, Imm::I(resolution.offset));
        loadFence();
    }

    void atomicLoad32(BaseIndex address, RegisterID dest)
    {
        auto resolution = resolveAddress(address, lazyTemp<Memory>());
        memoryFence();
        m_assembler.lwInsn(dest, resolution.base, Imm::I(resolution.offset));
        loadFence();
    }

    void atomicLoad64(Address address, RegisterID dest)
    {
        auto resolution = resolveAddress(address, lazyTemp<Memory>());
        memoryFence();
        m_assembler.ldInsn(dest, resolution.base, Imm::I(resolution.offset));
        loadFence();
    }

    void atomicLoad64(BaseIndex address, RegisterID dest)
    {
        auto resolution = resolveAddress(address, lazyTemp<Memory>());
        memoryFence();
        m_assembler.ldInsn(dest, resolution.base, Imm::I(resolution.offset));
        loadFence();
    }

    void moveConditionally32(RelationalCondition cond, RegisterID lhs, RegisterID rhs, RegisterID src, RegisterID dest)
    {
        auto temp = temps<Data, Memory>();
        m_assembler.signExtend<32>(temp.data(), lhs);
        m_assembler.signExtend<32>(temp.memory(), rhs);

        branchForMoveConditionally(invert(cond), temp.data(), temp.memory(), Imm::B<8>());
        m_assembler.addiInsn(dest, src, Imm::I<0>());
    }

    void moveConditionally32(RelationalCondition cond, RegisterID lhs, RegisterID rhs, RegisterID trueSrc, RegisterID falseSrc, RegisterID dest)
    {
        auto temp = temps<Data, Memory>();
        m_assembler.signExtend<32>(temp.data(), lhs);
        m_assembler.signExtend<32>(temp.memory(), rhs);

        branchForMoveConditionally(invert(cond), temp.data(), temp.memory(), Imm::B<12>());
        m_assembler.addiInsn(dest, trueSrc, Imm::I<0>());
        m_assembler.jalInsn(RISCV64Registers::zero, Imm::J<8>());
        m_assembler.addiInsn(dest, falseSrc, Imm::I<0>());
    }

    void moveConditionally32(RelationalCondition cond, RegisterID lhs, TrustedImm32 imm, RegisterID trueSrc, RegisterID falseSrc, RegisterID dest)
    {
        auto temp = temps<Data, Memory>();
        m_assembler.signExtend<32>(temp.data(), lhs);
        loadImmediate(imm, temp.memory());

        branchForMoveConditionally(invert(cond), temp.data(), temp.memory(), Imm::B<12>());
        m_assembler.addiInsn(dest, trueSrc, Imm::I<0>());
        m_assembler.jalInsn(RISCV64Registers::zero, Imm::J<8>());
        m_assembler.addiInsn(dest, falseSrc, Imm::I<0>());
    }

    void moveConditionally64(RelationalCondition cond, RegisterID lhs, RegisterID rhs, RegisterID src, RegisterID dest)
    {
        branchForMoveConditionally(invert(cond), lhs, rhs, Imm::B<8>());
        m_assembler.addiInsn(dest, src, Imm::I<0>());
    }

    void moveConditionally64(RelationalCondition cond, RegisterID lhs, RegisterID rhs, RegisterID trueSrc, RegisterID falseSrc, RegisterID dest)
    {
        branchForMoveConditionally(invert(cond), lhs, rhs, Imm::B<12>());
        m_assembler.addiInsn(dest, trueSrc, Imm::I<0>());
        m_assembler.jalInsn(RISCV64Registers::zero, Imm::J<8>());
        m_assembler.addiInsn(dest, falseSrc, Imm::I<0>());
    }

    void moveConditionally64(RelationalCondition cond, RegisterID lhs, TrustedImm32 imm, RegisterID trueSrc, RegisterID falseSrc, RegisterID dest)
    {
        auto temp = temps<Data>();
        loadImmediate(imm, temp.data());
        branchForMoveConditionally(invert(cond), lhs, temp.data(), Imm::B<12>());
        m_assembler.addiInsn(dest, trueSrc, Imm::I<0>());
        m_assembler.jalInsn(RISCV64Registers::zero, Imm::J<8>());
        m_assembler.addiInsn(dest, falseSrc, Imm::I<0>());
    }

    void moveConditionallyFloat(DoubleCondition cond, FPRegisterID lhs, FPRegisterID rhs, RegisterID src, RegisterID dest)
    {
        Jump invcondBranch = branchFP<32, true>(cond, lhs, rhs);
        m_assembler.addiInsn(dest, src, Imm::I<0>());
        invcondBranch.link(this);
    }

    void moveConditionallyFloat(DoubleCondition cond, FPRegisterID lhs, FPRegisterID rhs, RegisterID trueSrc, RegisterID falseSrc, RegisterID dest)
    {
        Jump invcondBranch = branchFP<32, true>(cond, lhs, rhs);
        m_assembler.addiInsn(dest, trueSrc, Imm::I<0>());
        Jump end = jump();
        invcondBranch.link(this);
        m_assembler.addiInsn(dest, falseSrc, Imm::I<0>());
        end.link(this);
    }

    void moveConditionallyDouble(DoubleCondition cond, FPRegisterID lhs, FPRegisterID rhs, RegisterID src, RegisterID dest)
    {
        Jump invcondBranch = branchFP<64, true>(cond, lhs, rhs);
        m_assembler.addiInsn(dest, src, Imm::I<0>());
        invcondBranch.link(this);
    }

    void moveConditionallyDouble(DoubleCondition cond, FPRegisterID lhs, FPRegisterID rhs, RegisterID trueSrc, RegisterID falseSrc, RegisterID dest)
    {
        Jump invcondBranch = branchFP<64, true>(cond, lhs, rhs);
        m_assembler.addiInsn(dest, trueSrc, Imm::I<0>());
        Jump end = jump();
        invcondBranch.link(this);
        m_assembler.addiInsn(dest, falseSrc, Imm::I<0>());
        end.link(this);
    }

    void moveConditionallyTest32(ResultCondition cond, RegisterID value, RegisterID mask, RegisterID src, RegisterID dest)
    {
        auto temp = temps<Data>();
        m_assembler.andInsn(temp.data(), value, mask);
        m_assembler.signExtend<32>(temp.data());
        testFinalize(cond, temp.data(), temp.data());

        m_assembler.beqInsn(temp.data(), RISCV64Registers::zero, Imm::B<8>());
        m_assembler.addiInsn(dest, src, Imm::I<0>());
    }

    void moveConditionallyTest32(ResultCondition cond, RegisterID value, RegisterID mask, RegisterID trueSrc, RegisterID falseSrc, RegisterID dest)
    {
        auto temp = temps<Data>();
        m_assembler.andInsn(temp.data(), value, mask);
        m_assembler.signExtend<32>(temp.data());
        testFinalize(cond, temp.data(), temp.data());

        m_assembler.beqInsn(temp.data(), RISCV64Registers::zero, Imm::B<12>());
        m_assembler.addiInsn(dest, trueSrc, Imm::I<0>());
        m_assembler.jalInsn(RISCV64Registers::zero, Imm::J<8>());
        m_assembler.addiInsn(dest, falseSrc, Imm::I<0>());
    }

    void moveConditionallyTest32(ResultCondition cond, RegisterID value, TrustedImm32 mask, RegisterID trueSrc, RegisterID falseSrc, RegisterID dest)
    {
        auto temp = temps<Data>();
        loadImmediate(mask, temp.data());
        m_assembler.andInsn(temp.data(), value, temp.data());
        m_assembler.signExtend<32>(temp.data());
        testFinalize(cond, temp.data(), temp.data());

        m_assembler.beqInsn(temp.data(), RISCV64Registers::zero, Imm::B<12>());
        m_assembler.addiInsn(dest, trueSrc, Imm::I<0>());
        m_assembler.jalInsn(RISCV64Registers::zero, Imm::J<8>());
        m_assembler.addiInsn(dest, falseSrc, Imm::I<0>());
    }

    void moveConditionallyTest64(ResultCondition cond, RegisterID value, RegisterID mask, RegisterID src, RegisterID dest)
    {
        auto temp = temps<Data>();
        m_assembler.andInsn(temp.data(), value, mask);
        testFinalize(cond, temp.data(), temp.data());

        m_assembler.beqInsn(temp.data(), RISCV64Registers::zero, Imm::B<8>());
        m_assembler.addiInsn(dest, src, Imm::I<0>());
    }

    void moveConditionallyTest64(ResultCondition cond, RegisterID value, RegisterID mask, RegisterID trueSrc, RegisterID falseSrc, RegisterID dest)
    {
        auto temp = temps<Data>();
        m_assembler.andInsn(temp.data(), value, mask);
        testFinalize(cond, temp.data(), temp.data());

        m_assembler.beqInsn(temp.data(), RISCV64Registers::zero, Imm::B<12>());
        m_assembler.addiInsn(dest, trueSrc, Imm::I<0>());
        m_assembler.jalInsn(RISCV64Registers::zero, Imm::J<8>());
        m_assembler.addiInsn(dest, falseSrc, Imm::I<0>());
    }

    void moveConditionallyTest64(ResultCondition cond, RegisterID value, TrustedImm32 mask, RegisterID trueSrc, RegisterID falseSrc, RegisterID dest)
    {
        auto temp = temps<Data>();
        loadImmediate(mask, temp.data());
        m_assembler.andInsn(temp.data(), value, temp.data());
        testFinalize(cond, temp.data(), temp.data());

        m_assembler.beqInsn(temp.data(), RISCV64Registers::zero, Imm::B<12>());
        m_assembler.addiInsn(dest, trueSrc, Imm::I<0>());
        m_assembler.jalInsn(RISCV64Registers::zero, Imm::J<8>());
        m_assembler.addiInsn(dest, falseSrc, Imm::I<0>());
    }

    void moveDoubleConditionally32(RelationalCondition cond, RegisterID lhs, RegisterID rhs, FPRegisterID trueSrc, FPRegisterID falseSrc, FPRegisterID dest)
    {
        auto temp = temps<Data, Memory>();
        m_assembler.signExtend<32>(temp.data(), lhs);
        m_assembler.signExtend<32>(temp.memory(), rhs);

        branchForMoveConditionally(invert(cond), temp.data(), temp.memory(), Imm::B<12>());
        m_assembler.fsgnjInsn<64>(dest, trueSrc, trueSrc);
        m_assembler.jalInsn(RISCV64Registers::zero, Imm::J<8>());
        m_assembler.fsgnjInsn<64>(dest, falseSrc, falseSrc);
    }

    void moveDoubleConditionally32(RelationalCondition cond, RegisterID lhs, TrustedImm32 imm, FPRegisterID trueSrc, FPRegisterID falseSrc, FPRegisterID dest)
    {
        auto temp = temps<Data, Memory>();
        m_assembler.signExtend<32>(temp.data(), lhs);
        loadImmediate(imm, temp.memory());

        branchForMoveConditionally(invert(cond), temp.data(), temp.memory(), Imm::B<12>());
        m_assembler.fsgnjInsn<64>(dest, trueSrc, trueSrc);
        m_assembler.jalInsn(RISCV64Registers::zero, Imm::J<8>());
        m_assembler.fsgnjInsn<64>(dest, falseSrc, falseSrc);
    }

    void moveDoubleConditionally64(RelationalCondition cond, RegisterID lhs, RegisterID rhs, FPRegisterID trueSrc, FPRegisterID falseSrc, FPRegisterID dest)
    {
        branchForMoveConditionally(invert(cond), lhs, rhs, Imm::B<12>());
        m_assembler.fsgnjInsn<64>(dest, trueSrc, trueSrc);
        m_assembler.jalInsn(RISCV64Registers::zero, Imm::J<8>());
        m_assembler.fsgnjInsn<64>(dest, falseSrc, falseSrc);
    }

    void moveDoubleConditionally64(RelationalCondition cond, RegisterID lhs, TrustedImm32 imm, FPRegisterID trueSrc, FPRegisterID falseSrc, FPRegisterID dest)
    {
        auto temp = temps<Data>();
        loadImmediate(imm, temp.data());
        branchForMoveConditionally(invert(cond), lhs, temp.data(), Imm::B<12>());
        m_assembler.fsgnjInsn<64>(dest, trueSrc, trueSrc);
        m_assembler.jalInsn(RISCV64Registers::zero, Imm::J<8>());
        m_assembler.fsgnjInsn<64>(dest, falseSrc, falseSrc);
    }

    void moveDoubleConditionallyFloat(DoubleCondition cond, FPRegisterID lhs, FPRegisterID rhs, FPRegisterID trueSrc, FPRegisterID falseSrc, FPRegisterID dest)
    {
        Jump invcondBranch = branchFP<32, true>(cond, lhs, rhs);
        m_assembler.fsgnjInsn<64>(dest, trueSrc, trueSrc);
        Jump end = jump();
        invcondBranch.link(this);
        m_assembler.fsgnjInsn<64>(dest, falseSrc, falseSrc);
        end.link(this);
    }

    void moveDoubleConditionallyDouble(DoubleCondition cond, FPRegisterID lhs, FPRegisterID rhs, FPRegisterID trueSrc, FPRegisterID falseSrc, FPRegisterID dest)
    {
        Jump invcondBranch = branchFP<64, true>(cond, lhs, rhs);
        m_assembler.fsgnjInsn<64>(dest, trueSrc, trueSrc);
        Jump end = jump();
        invcondBranch.link(this);
        m_assembler.fsgnjInsn<64>(dest, falseSrc, falseSrc);
        end.link(this);
    }

    void moveDoubleConditionallyTest32(ResultCondition cond, RegisterID value, RegisterID mask, FPRegisterID trueSrc, FPRegisterID falseSrc, FPRegisterID dest)
    {
        auto temp = temps<Data>();
        m_assembler.andInsn(temp.data(), value, mask);
        m_assembler.signExtend<32>(temp.data());
        testFinalize(cond, temp.data(), temp.data());

        m_assembler.beqInsn(temp.data(), RISCV64Registers::zero, Imm::B<12>());
        m_assembler.fsgnjInsn<64>(dest, trueSrc, trueSrc);
        m_assembler.jalInsn(RISCV64Registers::zero, Imm::J<8>());
        m_assembler.fsgnjInsn<64>(dest, falseSrc, falseSrc);
    }

    void moveDoubleConditionallyTest32(ResultCondition cond, RegisterID value, TrustedImm32 mask, FPRegisterID trueSrc, FPRegisterID falseSrc, FPRegisterID dest)
    {
        auto temp = temps<Data>();
        loadImmediate(mask, temp.data());
        m_assembler.andInsn(temp.data(), value, temp.data());
        m_assembler.signExtend<32>(temp.data());
        testFinalize(cond, temp.data(), temp.data());

        m_assembler.beqInsn(temp.data(), RISCV64Registers::zero, Imm::B<12>());
        m_assembler.fsgnjInsn<64>(dest, trueSrc, trueSrc);
        m_assembler.jalInsn(RISCV64Registers::zero, Imm::J<8>());
        m_assembler.fsgnjInsn<64>(dest, falseSrc, falseSrc);
    }

    void moveDoubleConditionallyTest64(ResultCondition cond, RegisterID value, RegisterID mask, FPRegisterID trueSrc, FPRegisterID falseSrc, FPRegisterID dest)
    {
        auto temp = temps<Data>();
        m_assembler.andInsn(temp.data(), value, mask);
        testFinalize(cond, temp.data(), temp.data());

        m_assembler.beqInsn(temp.data(), RISCV64Registers::zero, Imm::B<12>());
        m_assembler.fsgnjInsn<64>(dest, trueSrc, trueSrc);
        m_assembler.jalInsn(RISCV64Registers::zero, Imm::J<8>());
        m_assembler.fsgnjInsn<64>(dest, falseSrc, falseSrc);
    }

    void moveDoubleConditionallyTest64(ResultCondition cond, RegisterID value, TrustedImm32 mask, FPRegisterID trueSrc, FPRegisterID falseSrc, FPRegisterID dest)
    {
        auto temp = temps<Data>();
        loadImmediate(mask, temp.data());
        m_assembler.andInsn(temp.data(), value, temp.data());
        testFinalize(cond, temp.data(), temp.data());

        m_assembler.beqInsn(temp.data(), RISCV64Registers::zero, Imm::B<12>());
        m_assembler.fsgnjInsn<64>(dest, trueSrc, trueSrc);
        m_assembler.jalInsn(RISCV64Registers::zero, Imm::J<8>());
        m_assembler.fsgnjInsn<64>(dest, falseSrc, falseSrc);
    }

private:
    enum class ArithmeticOperation {
        Addition,
        Subtraction,
        Multiplication,
    };

    struct Imm {
        template<typename T>
        using EnableIfInteger = std::enable_if_t<(std::is_same_v<T, int32_t> || std::is_same_v<T, int64_t>)>;

        template<typename ImmediateType, typename T, typename = EnableIfInteger<T>>
        static bool isValid(T value) { return ImmediateType::isValid(value); }

        using IType = RISCV64Assembler::IImmediate;
        template<int32_t value>
        static IType I() { return IType::v<IType, value>(); }
        template<typename T, typename = EnableIfInteger<T>>
        static IType I(T value) { return IType::v<IType>(value); }
        static IType I(uint32_t value) { return IType(value); }

        using SType = RISCV64Assembler::SImmediate;
        template<int32_t value>
        static SType S() { return SType::v<SType, value>(); }
        template<typename T, typename = EnableIfInteger<T>>
        static SType S(T value) { return SType::v<SType>(value); }

        using BType = RISCV64Assembler::BImmediate;
        template<int32_t value>
        static BType B() { return BType::v<BType, value>(); }
        template<typename T, typename = EnableIfInteger<T>>
        static BType B(T value) { return BType::v<BType>(value); }
        static BType B(uint32_t value) { return BType(value); }

        using UType = RISCV64Assembler::UImmediate;
        static UType U(uint32_t value) { return UType(value); }

        using JType = RISCV64Assembler::JImmediate;
        template<int32_t value>
        static JType J() { return JType::v<JType, value>(); }
    };

    void loadImmediate(TrustedImm32 imm, RegisterID dest)
    {
        RISCV64Assembler::ImmediateLoader(imm.m_value).moveInto(m_assembler, dest);
    }

    void loadImmediate(TrustedImm64 imm, RegisterID dest)
    {
        RISCV64Assembler::ImmediateLoader(imm.m_value).moveInto(m_assembler, dest);
    }

    void loadImmediate(TrustedImmPtr imm, RegisterID dest)
    {
        intptr_t value = imm.asIntptr();
        if constexpr (sizeof(intptr_t) == sizeof(int64_t))
            loadImmediate(TrustedImm64(int64_t(value)), dest);
        else
            loadImmediate(TrustedImm32(int32_t(value)), dest);
    }

    struct AddressResolution {
        RegisterID base;
        int32_t offset;
    };

    template<typename RegisterType>
    AddressResolution resolveAddress(BaseIndex address, RegisterType destination)
    {
        if (!!address.offset) {
            if (RISCV64Assembler::ImmediateBase<12>::isValid(address.offset)) {
                if (address.scale != TimesOne) {
                    m_assembler.slliInsn(destination, address.index, uint32_t(address.scale));
                    m_assembler.addInsn(destination, address.base, destination);
                } else
                    m_assembler.addInsn(destination, address.base, address.index);
                return { destination, address.offset };
            }

            if (address.scale != TimesOne) {
                uint32_t scale = address.scale;
                int32_t upperOffset = address.offset >> scale;
                int32_t lowerOffset = address.offset & ((1 << scale) - 1);

                if (!RISCV64Assembler::ImmediateBase<12>::isValid(upperOffset)) {
                    RISCV64Assembler::ImmediateLoader imml(upperOffset);
                    imml.moveInto(m_assembler, destination);
                    m_assembler.addInsn(destination, address.index, destination);
                } else
                    m_assembler.addiInsn(destination, address.index, Imm::I(upperOffset));
                m_assembler.slliInsn(destination, destination, scale);
                m_assembler.oriInsn(destination, destination, Imm::I(lowerOffset));
            } else {
                RISCV64Assembler::ImmediateLoader imml(address.offset);
                imml.moveInto(m_assembler, destination);
                m_assembler.addInsn(destination, destination, address.index);
            }
            m_assembler.addInsn(destination, address.base, destination);
            return { destination, 0 };
        }

        if (address.scale != TimesOne) {
            m_assembler.slliInsn(destination, address.index, address.scale);
            m_assembler.addInsn(destination, address.base, destination);
        } else
            m_assembler.addInsn(destination, address.base, address.index);
        return { destination, 0 };
    }

    template<typename RegisterType>
    AddressResolution resolveAddress(Address address, RegisterType destination)
    {
        if (RISCV64Assembler::ImmediateBase<12>::isValid(address.offset))
            return { address.base, address.offset };

        uint32_t value = *reinterpret_cast<uint32_t*>(&address.offset);
        if (value & (1 << 11))
            value += (1 << 12);

        m_assembler.luiInsn(destination, Imm::U(value));
        m_assembler.addiInsn(destination, destination, Imm::I(value & ((1 << 12) - 1)));
        m_assembler.addInsn(destination, address.base, destination);
        return { destination, 0 };
    }

    template<typename RegisterType>
    AddressResolution resolveAddress(ExtendedAddress address, RegisterType destination)
    {
        if (RISCV64Assembler::ImmediateBase<12>::isValid(address.offset))
            return { address.base, int32_t(address.offset) };

        RISCV64Assembler::ImmediateLoader imml(int64_t(address.offset));
        imml.moveInto(m_assembler, destination);
        m_assembler.addInsn(destination, address.base, destination);
        return { destination, 0 };
    }

    Jump makeBranch(RelationalCondition condition, RegisterID lhs, RegisterID rhs)
    {
        auto label = m_assembler.label();
        m_assembler.branchPlaceholder(
            [&] {
                switch (condition) {
                case Equal:
                    return m_assembler.beqInsn(lhs, rhs, Imm::B<0>());
                case NotEqual:
                    return m_assembler.bneInsn(lhs, rhs, Imm::B<0>());
                case Above:
                    return m_assembler.bltuInsn(rhs, lhs, Imm::B<0>());
                case AboveOrEqual:
                    return m_assembler.bgeuInsn(lhs, rhs, Imm::B<0>());
                case Below:
                    return m_assembler.bltuInsn(lhs, rhs, Imm::B<0>());
                case BelowOrEqual:
                    return m_assembler.bgeuInsn(rhs, lhs, Imm::B<0>());
                case GreaterThan:
                    return m_assembler.bltInsn(rhs, lhs, Imm::B<0>());
                case GreaterThanOrEqual:
                    return m_assembler.bgeInsn(lhs, rhs, Imm::B<0>());
                case LessThan:
                    return m_assembler.bltInsn(lhs, rhs, Imm::B<0>());
                case LessThanOrEqual:
                    return m_assembler.bgeInsn(rhs, lhs, Imm::B<0>());
                }
            });
        return Jump(label);
    }

    Jump branchTestFinalize(ResultCondition cond, RegisterID src)
    {
        switch (cond) {
        case Overflow:
            break;
        case Signed:
            return makeBranch(LessThan, src, RISCV64Registers::zero);
        case PositiveOrZero:
            return makeBranch(GreaterThanOrEqual, src, RISCV64Registers::zero);
        case Zero:
            return makeBranch(Equal, src, RISCV64Registers::zero);
        case NonZero:
            return makeBranch(NotEqual, src, RISCV64Registers::zero);
        }

        RELEASE_ASSERT_NOT_REACHED();
        return { };
    }

    template<unsigned bitSize, ArithmeticOperation arithmeticOperation, typename Op2Type>
    Jump branchForArithmeticOverflow(RegisterID op1, Op2Type op2, RegisterID dest)
    {
        static_assert(bitSize == 32 || bitSize == 64);
        static_assert(std::is_same_v<Op2Type, RegisterID> || std::is_same_v<Op2Type, TrustedImm32>);
        auto temp = temps<Data, Memory>();

        if constexpr (bitSize == 32) {
            RELEASE_ASSERT(op1 == temp.data() || op1 != temp.memory());
            m_assembler.signExtend<32>(temp.data(), op1);

            if constexpr (!std::is_same_v<Op2Type, TrustedImm32>) {
                RELEASE_ASSERT(op2 == temp.memory() || op1 != temp.data());
                m_assembler.signExtend<32>(temp.memory(), op2);
            } else
                loadImmediate(op2, temp.memory());

            void (RISCV64Assembler::*op)(RegisterID, RegisterID, RegisterID) =
                [] {
                    switch (arithmeticOperation) {
                    case ArithmeticOperation::Addition:
                        return &RISCV64Assembler::addInsn;
                    case ArithmeticOperation::Subtraction:
                        return &RISCV64Assembler::subInsn;
                    case ArithmeticOperation::Multiplication:
                        return &RISCV64Assembler::mulInsn;
                    }
                }();

            if (dest == temp.data() || dest == temp.memory()) {
                RegisterID otherTemp = (dest == temp.data()) ? temp.memory() : temp.data();
                (m_assembler.*op)(dest, temp.data(), temp.memory());
                m_assembler.signExtend<32>(otherTemp, dest);
                m_assembler.xorInsn(otherTemp, dest, otherTemp);
                m_assembler.maskRegister<32>(dest, dest);
                return makeBranch(NotEqual, otherTemp, RISCV64Registers::zero);
            }

            (m_assembler.*op)(temp.data(), temp.data(), temp.memory());
            m_assembler.maskRegister<32>(dest, temp.data()),
            m_assembler.signExtend<32>(temp.memory(), temp.data());
            return makeBranch(NotEqual, temp.data(), temp.memory());
        }

        RELEASE_ASSERT(op1 != temp.data() && op1 != temp.memory());
        RELEASE_ASSERT(dest != temp.data() && dest != temp.memory());

        RegisterID rop2;
        if constexpr (std::is_same_v<Op2Type, TrustedImm32>) {
            loadImmediate(op2, temp.memory());
            rop2 = temp.memory();
        } else {
            RELEASE_ASSERT(op2 != temp.data() && op2 != temp.memory());
            rop2 = op2;
        }

        switch (arithmeticOperation) {
        case ArithmeticOperation::Addition:
        {
            if (op1 == dest && rop2 == dest) {
                m_assembler.slliInsn<1>(temp.memory(), dest);
                m_assembler.xorInsn(temp.data(), temp.memory(), dest);
                move(temp.memory(), dest);
            } else {
                m_assembler.xorInsn(temp.data(), op1, rop2);
                m_assembler.xoriInsn(temp.data(), temp.data(), Imm::I<-1>());

                m_assembler.addInsn(dest, op1, rop2);
                m_assembler.xorInsn(temp.memory(), (op1 == dest) ? rop2 : op1, dest);
                m_assembler.andInsn(temp.data(), temp.data(), temp.memory());
            }
            return makeBranch(LessThan, temp.data(), RISCV64Registers::zero);
        }
        case ArithmeticOperation::Subtraction:
        {
            if (op1 == dest && rop2 == dest) {
                move(RISCV64Registers::zero, dest);
                move(RISCV64Registers::zero, temp.data());
            } else {
                m_assembler.xorInsn(temp.data(), op1, rop2);

                m_assembler.subInsn(dest, op1, rop2);
                if (op1 == dest) {
                    m_assembler.xorInsn(temp.memory(), rop2, dest);
                    m_assembler.xoriInsn(temp.memory(), temp.memory(), Imm::I<-1>());
                } else
                    m_assembler.xorInsn(temp.memory(), op1, dest);
                m_assembler.andInsn(temp.data(), temp.data(), temp.memory());
            }
            return makeBranch(LessThan, temp.data(), RISCV64Registers::zero);
        }
        case ArithmeticOperation::Multiplication:
            m_assembler.mulhInsn(temp.data(), op1, rop2);
            m_assembler.mulInsn(dest, op1, rop2);
            m_assembler.sraiInsn<0x3f>(temp.memory(), dest);
            return makeBranch(NotEqual, temp.data(), temp.memory());
        }
    }

    void branchForMoveConditionally(RelationalCondition condition, RegisterID lhs, RegisterID rhs, RISCV64Assembler::BImmediate offset)
    {
        switch (condition) {
        case Equal:
            return m_assembler.beqInsn(lhs, rhs, offset);
        case NotEqual:
            return m_assembler.bneInsn(lhs, rhs, offset);
        case Above:
            return m_assembler.bltuInsn(rhs, lhs, offset);
        case AboveOrEqual:
            return m_assembler.bgeuInsn(lhs, rhs, offset);
        case Below:
            return m_assembler.bltuInsn(lhs, rhs, offset);
        case BelowOrEqual:
            return m_assembler.bgeuInsn(rhs, lhs, offset);
        case GreaterThan:
            return m_assembler.bltInsn(rhs, lhs, offset);
        case GreaterThanOrEqual:
            return m_assembler.bgeInsn(lhs, rhs, offset);
        case LessThan:
            return m_assembler.bltInsn(lhs, rhs, offset);
        case LessThanOrEqual:
            return m_assembler.bgeInsn(rhs, lhs, offset);
        }

        RELEASE_ASSERT_NOT_REACHED();
    }

    void compareFinalize(RelationalCondition cond, RegisterID lhs, RegisterID rhs, RegisterID dest)
    {
        switch (cond) {
        case Equal:
            m_assembler.xorInsn(dest, lhs, rhs);
            m_assembler.sltiuInsn(dest, dest, Imm::I<1>());
            break;
        case NotEqual:
            m_assembler.xorInsn(dest, lhs, rhs);
            m_assembler.sltuInsn(dest, RISCV64Registers::zero, dest);
            break;
        case Above:
            m_assembler.sltuInsn(dest, rhs, lhs);
            break;
        case AboveOrEqual:
            m_assembler.sltuInsn(dest, lhs, rhs);
            m_assembler.xoriInsn(dest, dest, Imm::I<1>());
            break;
        case Below:
            m_assembler.sltuInsn(dest, lhs, rhs);
            break;
        case BelowOrEqual:
            m_assembler.sltuInsn(dest, rhs, lhs);
            m_assembler.xoriInsn(dest, dest, Imm::I<1>());
            break;
        case GreaterThan:
            m_assembler.sltInsn(dest, rhs, lhs);
            break;
        case GreaterThanOrEqual:
            m_assembler.sltInsn(dest, lhs, rhs);
            m_assembler.xoriInsn(dest, dest, Imm::I<1>());
            break;
        case LessThan:
            m_assembler.sltInsn(dest, lhs, rhs);
            break;
        case LessThanOrEqual:
            m_assembler.sltInsn(dest, rhs, lhs);
            m_assembler.xoriInsn(dest, dest, Imm::I<1>());
            break;
        }
    }

    void testFinalize(ResultCondition cond, RegisterID src, RegisterID dest)
    {
        switch (cond) {
        case Overflow:
        case Signed:
        case PositiveOrZero:
            // None of the above should be used for testing operations.
            RELEASE_ASSERT_NOT_REACHED();
            break;
        case Zero:
            m_assembler.sltiuInsn(dest, src, Imm::I<1>());
            break;
        case NonZero:
            m_assembler.sltuInsn(dest, RISCV64Registers::zero, src);
            break;
        }
    }

    template<unsigned fpSize, bool invert = false>
    Jump branchFP(DoubleCondition cond, FPRegisterID lhs, FPRegisterID rhs)
    {
        auto temp = temps<Data>();
        JumpList unorderedJump;

        m_assembler.fclassInsn<fpSize>(temp.data(), lhs);
        m_assembler.andiInsn(temp.data(), temp.data(), Imm::I<0b1100000000>());
        unorderedJump.append(makeBranch(NotEqual, temp.data(), RISCV64Registers::zero));

        m_assembler.fclassInsn<fpSize>(temp.data(), rhs);
        m_assembler.andiInsn(temp.data(), temp.data(), Imm::I<0b1100000000>());
        unorderedJump.append(makeBranch(NotEqual, temp.data(), RISCV64Registers::zero));

        switch (cond) {
        case DoubleEqualAndOrdered:
        case DoubleEqualOrUnordered:
            m_assembler.feqInsn<fpSize>(temp.data(), lhs, rhs);
            break;
        case DoubleNotEqualAndOrdered:
        case DoubleNotEqualOrUnordered:
            m_assembler.feqInsn<fpSize>(temp.data(), lhs, rhs);
            m_assembler.xoriInsn(temp.data(), temp.data(), Imm::I<1>());
            break;
        case DoubleGreaterThanAndOrdered:
        case DoubleGreaterThanOrUnordered:
            m_assembler.fltInsn<fpSize>(temp.data(), rhs, lhs);
            break;
        case DoubleGreaterThanOrEqualAndOrdered:
        case DoubleGreaterThanOrEqualOrUnordered:
            m_assembler.fleInsn<fpSize>(temp.data(), rhs, lhs);
            break;
        case DoubleLessThanAndOrdered:
        case DoubleLessThanOrUnordered:
            m_assembler.fltInsn<fpSize>(temp.data(), lhs, rhs);
            break;
        case DoubleLessThanOrEqualAndOrdered:
        case DoubleLessThanOrEqualOrUnordered:
            m_assembler.fleInsn<fpSize>(temp.data(), lhs, rhs);
            break;
        }

        Jump end = jump();
        unorderedJump.link(this);

        switch (cond) {
        case DoubleEqualAndOrdered:
        case DoubleNotEqualAndOrdered:
        case DoubleGreaterThanAndOrdered:
        case DoubleGreaterThanOrEqualAndOrdered:
        case DoubleLessThanAndOrdered:
        case DoubleLessThanOrEqualAndOrdered:
            m_assembler.addiInsn(temp.data(), RISCV64Registers::zero, Imm::I<0>());
            break;
        case DoubleEqualOrUnordered:
        case DoubleNotEqualOrUnordered:
        case DoubleGreaterThanOrUnordered:
        case DoubleGreaterThanOrEqualOrUnordered:
        case DoubleLessThanOrUnordered:
        case DoubleLessThanOrEqualOrUnordered:
            m_assembler.addiInsn(temp.data(), RISCV64Registers::zero, Imm::I<1>());
            break;
        }

        end.link(this);
        return makeBranch(invert ? Equal : NotEqual, temp.data(), RISCV64Registers::zero);
    }

    template<unsigned fpSize, RISCV64Assembler::FPRoundingMode RM>
    void roundFP(FPRegisterID src, FPRegisterID dest)
    {
        static_assert(fpSize == 32 || fpSize == 64);
        auto temp = temps<Data>();

        JumpList end;

        // Test the given source register for NaN condition. If detected, it should be
        // propagated to the destination register.
        m_assembler.fclassInsn<fpSize>(temp.data(), src);
        m_assembler.andiInsn(temp.data(), temp.data(), Imm::I<0b1100000000>());
        Jump notNaN = makeBranch(Equal, temp.data(), RISCV64Registers::zero);

        m_assembler.faddInsn<fpSize>(dest, src, src);
        end.append(jump());

        notNaN.link(this);
        m_assembler.fsgnjxInsn<fpSize>(fpTempRegister, src, src);

        // Compare the absolute source value with the maximum representable integer value.
        // Rounding is only possible if the absolute source value is smaller.
        if constexpr (fpSize == 32) {
            m_assembler.addiInsn(temp.data(), RISCV64Registers::zero, Imm::I<0b10010111>());
            m_assembler.slliInsn<23>(temp.data(), temp.data());
            m_assembler.fmvInsn<RISCV64Assembler::FMVType::W, RISCV64Assembler::FMVType::X>(fpTempRegister2, temp.data());
        } else {
            m_assembler.addiInsn(temp.data(), RISCV64Registers::zero, Imm::I<0b10000110100>());
            m_assembler.slliInsn<52>(temp.data(), temp.data());
            m_assembler.fmvInsn<RISCV64Assembler::FMVType::D, RISCV64Assembler::FMVType::X>(fpTempRegister2, temp.data());
        }

        m_assembler.fltInsn<fpSize>(temp.data(), fpTempRegister, fpTempRegister2);
        Jump notRoundable = makeBranch(Equal, temp.data(), RISCV64Registers::zero);

        FPRegisterID dealiasedSrc = src;
        if (src == dest) {
            m_assembler.fsgnjInsn<fpSize>(fpTempRegister, src, src);
            dealiasedSrc = fpTempRegister;
        }

        // Rounding can now be done by roundtripping through a general-purpose register
        // with the desired rounding mode applied.
        if constexpr (fpSize == 32) {
            m_assembler.fcvtInsn<RM, RISCV64Assembler::FCVTType::W, RISCV64Assembler::FCVTType::S>(temp.data(), dealiasedSrc);
            m_assembler.fcvtInsn<RM, RISCV64Assembler::FCVTType::S, RISCV64Assembler::FCVTType::W>(dest, temp.data());
        } else {
            m_assembler.fcvtInsn<RM, RISCV64Assembler::FCVTType::L, RISCV64Assembler::FCVTType::D>(temp.data(), dealiasedSrc);
            m_assembler.fcvtInsn<RM, RISCV64Assembler::FCVTType::D, RISCV64Assembler::FCVTType::L>(dest, temp.data());
        }
        m_assembler.fsgnjInsn<fpSize>(dest, dest, dealiasedSrc);
        end.append(jump());

        notRoundable.link(this);
        // If not roundable, the value should still be moved over into the destination register.
        if (src != dest)
            m_assembler.fsgnjInsn<fpSize>(dest, src, src);

        end.link(this);
    }

    template<unsigned fpSize>
    void compareFP(DoubleCondition cond, FPRegisterID lhs, FPRegisterID rhs, RegisterID dest)
    {
        static_assert(fpSize == 32 || fpSize == 64);
        auto temp = temps<Data>();

        JumpList unorderedJump;

        // Detect any NaN values that could still yield a positive comparison, depending on the condition.
        m_assembler.fclassInsn<fpSize>(temp.data(), lhs);
        m_assembler.andiInsn(temp.data(), temp.data(), Imm::I<0b1100000000>());
        unorderedJump.append(makeBranch(NotEqual, temp.data(), RISCV64Registers::zero));

        m_assembler.fclassInsn<fpSize>(temp.data(), rhs);
        m_assembler.andiInsn(temp.data(), temp.data(), Imm::I<0b1100000000>());
        unorderedJump.append(makeBranch(NotEqual, temp.data(), RISCV64Registers::zero));

        switch (cond) {
        case DoubleEqualAndOrdered:
        case DoubleEqualOrUnordered:
            m_assembler.feqInsn<fpSize>(dest, lhs, rhs);
            break;
        case DoubleNotEqualAndOrdered:
        case DoubleNotEqualOrUnordered:
            m_assembler.feqInsn<fpSize>(dest, lhs, rhs);
            m_assembler.xoriInsn(dest, dest, Imm::I<1>());
            break;
        case DoubleGreaterThanAndOrdered:
        case DoubleGreaterThanOrUnordered:
            m_assembler.fltInsn<fpSize>(dest, rhs, lhs);
            break;
        case DoubleGreaterThanOrEqualAndOrdered:
        case DoubleGreaterThanOrEqualOrUnordered:
            m_assembler.fleInsn<fpSize>(dest, rhs, lhs);
            break;
        case DoubleLessThanAndOrdered:
        case DoubleLessThanOrUnordered:
            m_assembler.fltInsn<fpSize>(dest, lhs, rhs);
            break;
        case DoubleLessThanOrEqualAndOrdered:
        case DoubleLessThanOrEqualOrUnordered:
            m_assembler.fleInsn<fpSize>(dest, lhs, rhs);
            break;
        }

        Jump end = jump();
        unorderedJump.link(this);

        switch (cond) {
        case DoubleEqualAndOrdered:
        case DoubleNotEqualAndOrdered:
        case DoubleGreaterThanAndOrdered:
        case DoubleGreaterThanOrEqualAndOrdered:
        case DoubleLessThanAndOrdered:
        case DoubleLessThanOrEqualAndOrdered:
            m_assembler.addiInsn(dest, RISCV64Registers::zero, Imm::I<0>());
            break;
        case DoubleEqualOrUnordered:
        case DoubleNotEqualOrUnordered:
        case DoubleGreaterThanOrUnordered:
        case DoubleGreaterThanOrEqualOrUnordered:
        case DoubleLessThanOrUnordered:
        case DoubleLessThanOrEqualOrUnordered:
            m_assembler.addiInsn(dest, RISCV64Registers::zero, Imm::I<1>());
            break;
        }

        end.link(this);
    }
};

} // namespace JSC

#undef MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD
#undef MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD_WITH_RETURN

#endif // ENABLE(ASSEMBLER) && CPU(RISCV64)
