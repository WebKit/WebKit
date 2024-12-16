/*
 * Copyright (C) 2011-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2023-2024 Loongson Technology. All rights reserved.
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

#if ENABLE(ASSEMBLER) && CPU(LOONGARCH64)

#include "AbstractMacroAssembler.h"
#include "LOONGARCH64Assembler.h"

#define MACRO_ASSEMBLER_LOONGARCH64_TEMPLATED_NOOP_METHOD(methodName) \
    template<typename... Args> void methodName(Args&&...) { }
#define MACRO_ASSEMBLER_LOONGARCH64_TEMPLATED_NOOP_METHOD_WITH_RETURN(methodName, returnType) \
    template<typename... Args> returnType methodName(Args&&...) { return { }; }

namespace JSC {

using Assembler = TARGET_ASSEMBLER;

class MacroAssemblerLOONGARCH64 : public AbstractMacroAssembler<Assembler> {
public:
    static constexpr unsigned numGPRs = 32;
    static constexpr unsigned numFPRs = 32;

    static constexpr size_t nearJumpRange = 2 * GB;

    static constexpr RegisterID dataTempRegister = LOONGARCH64Registers::r20;
    static constexpr RegisterID dataTempRegister2 = LOONGARCH64Registers::r19;
    static constexpr RegisterID memoryTempRegister = LOONGARCH64Registers::r18;

    static constexpr FPRegisterID fpTempRegister = LOONGARCH64Registers::f22;
    static constexpr FPRegisterID fpTempRegister2 = LOONGARCH64Registers::f23;

    static constexpr RegisterID InvalidGPRReg = LOONGARCH64Registers::InvalidGPRReg;

    RegisterID scratchRegister()
    {
        return dataTempRegister;
    }

    enum TempRegisterType : int8_t {
        Data,
        Data2,
        Memory,
    };

    template<TempRegisterType... RegisterTypes>
    struct TempRegister {
        RegisterID data()
        {
            static_assert(((RegisterTypes == Data) || ...));
            return dataTempRegister;
        }

        RegisterID data2()
        {
            static_assert(((RegisterTypes == Data2) || ...));
            return dataTempRegister2;
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
            static_assert(RegisterType == Data || RegisterType == Data2 || RegisterType == Memory);
        }

        operator RegisterID()
        {
            RELEASE_ASSERT(m_allowScratchRegister);
            if constexpr (RegisterType == Data)
                return dataTempRegister;
            if constexpr (RegisterType == Data2)
                return dataTempRegister2;
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
    static bool supportsFloat16() { return false; }

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
        Carry, // <- not implemented
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

    static constexpr RegisterID stackPointerRegister = LOONGARCH64Registers::sp;
    static constexpr RegisterID framePointerRegister = LOONGARCH64Registers::fp;
    static constexpr RegisterID linkRegister = LOONGARCH64Registers::ra;

    static constexpr CFRegisterID fcc0 = LOONGARCH64Registers::fcc0; 

    void add32(RegisterID src, RegisterID dest)
    {
        add32(src, dest, dest);
    }

    void add32(RegisterID op1, RegisterID op2, RegisterID dest)
    {
        m_assembler.add_wInsn(dest, op1, op2);
        m_assembler.maskRegister<32>(dest);
    }

    void add32(TrustedImm32 imm, RegisterID dest)
    {
        add32(imm, dest, dest);
    }

    void add32(TrustedImm32 imm, RegisterID op2, RegisterID dest)
    {
        if (Imm::I12Type::isSImm<12>(imm.m_value)) {
            m_assembler.addi_wInsn(dest, op2, Imm::I12(imm.m_value));
            m_assembler.maskRegister<32>(dest);
            return;
        }

        auto temp = temps<Data>();
        loadImmediate(imm, temp.data());
        m_assembler.add_wInsn(dest, temp.data(), op2);
        m_assembler.maskRegister<32>(dest);
    }

    void add32(TrustedImm32 imm, AbsoluteAddress address)
    {
        auto temp = temps<Data, Memory>();
        loadImmediate(TrustedImmPtr(address.m_ptr), temp.memory());
        if (Imm::I12Type::isSImm<12>(imm.m_value)) {
            m_assembler.ld_wInsn(temp.data(), temp.memory(), Imm::I12<0>());
            m_assembler.addi_dInsn(temp.data(), temp.data(), Imm::I12(imm.m_value));
            m_assembler.st_wInsn(temp.data(), temp.memory(), Imm::I12<0>());
            return;
        }

        m_assembler.ld_wInsn(temp.memory(), temp.memory(), Imm::I12<0>());
        loadImmediate(imm, temp.data());
        m_assembler.add_dInsn(temp.data(), temp.memory(), temp.data());

        loadImmediate(TrustedImmPtr(address.m_ptr), temp.memory());
        m_assembler.st_wInsn(temp.data(), temp.memory(), Imm::I12<0>());
    }

    void add32(TrustedImm32 imm, Address address)
    {
        auto temp = temps<Data, Memory>();
        auto resolution = resolveAddress(address, temp.memory());
        if (Imm::I12Type::isSImm<12>(imm.m_value)) {
            m_assembler.ld_wInsn(temp.data(), resolution.base, Imm::I12(resolution.offset));
            m_assembler.addi_dInsn(temp.data(), temp.data(), Imm::I12(imm.m_value));
            m_assembler.st_wInsn(temp.data(), resolution.base, Imm::I12(resolution.offset));
            return;
        }

        m_assembler.ld_wInsn(temp.memory(), resolution.base, Imm::I12(resolution.offset));
        loadImmediate(imm, temp.data());
        m_assembler.add_dInsn(temp.data(), temp.memory(), temp.data());

        resolution = resolveAddress(address, temp.memory());
        m_assembler.st_wInsn(temp.data(), resolution.base, Imm::I12(resolution.offset));
    }

    void add32(Address address, RegisterID dest)
    {
        auto temp = temps<Data, Memory>();
        auto resolution = resolveAddress(address, temp.memory());
        m_assembler.ld_wInsn(temp.data(), resolution.base, Imm::I12(resolution.offset));
        m_assembler.add_wInsn(dest, temp.data(), dest);
        m_assembler.maskRegister<32>(dest);
    }

    void add64(RegisterID src, RegisterID dest)
    {
        add64(src, dest, dest);
    }

    void add64(RegisterID op1, RegisterID op2, RegisterID dest)
    {
        m_assembler.add_dInsn(dest, op1, op2);
    }

    void add64(TrustedImm32 imm, RegisterID dest)
    {
        add64(imm, dest, dest);
    }

    void add64(TrustedImm32 imm, RegisterID op2, RegisterID dest)
    {
        if (Imm::I12Type::isSImm<12>(imm.m_value)) {
            m_assembler.addi_dInsn(dest, op2, Imm::I12(imm.m_value));
            return;
        }

        auto temp = temps<Data>();
        loadImmediate(imm, temp.data());
        m_assembler.add_dInsn(dest, temp.data(), op2);
    }

    void add64(TrustedImm64 imm, RegisterID dest)
    {
        add64(imm, dest, dest);
    }

    void add64(TrustedImm64 imm, RegisterID op2, RegisterID dest)
    {
        if (Imm::I12Type::isSImm<12>(imm.m_value)) {
            m_assembler.addi_dInsn(dest, op2, Imm::I12(imm.m_value));
            return;
        }

        auto temp = temps<Data>();
        loadImmediate(imm, temp.data());
        m_assembler.add_dInsn(dest, temp.data(), op2);
    }

    void add64(TrustedImm32 imm, AbsoluteAddress address)
    {
        auto temp = temps<Data, Memory>();
        loadImmediate(TrustedImmPtr(address.m_ptr), temp.memory());

        if (Imm::I12Type::isSImm<12>(imm.m_value)) {
            m_assembler.ld_dInsn(temp.data(), temp.memory(), Imm::I12<0>());
            m_assembler.addi_dInsn(temp.data(), temp.data(), Imm::I12(imm.m_value));
            m_assembler.st_dInsn(temp.data(), temp.memory(), Imm::I12<0>());
            return;
        }

        m_assembler.ld_dInsn(temp.memory(), temp.memory(), Imm::I12<0>());
        loadImmediate(imm, temp.data());
        m_assembler.add_dInsn(temp.data(), temp.data(), temp.memory());

        loadImmediate(TrustedImmPtr(address.m_ptr), temp.memory());
        m_assembler.st_dInsn(temp.data(), temp.memory(), Imm::I12<0>());
    }

    void add64(TrustedImm32 imm, Address address)
    {
        auto temp = temps<Data, Memory>();
        auto resolution = resolveAddress(address, temp.memory());
        m_assembler.ld_dInsn(temp.data(), resolution.base, Imm::I12(resolution.offset));

        if (Imm::I12Type::isSImm<12>(imm.m_value)) {
            m_assembler.addi_dInsn(temp.data(), temp.data(), Imm::I12(imm.m_value));
            m_assembler.st_dInsn(temp.data(), resolution.base, Imm::I12(resolution.offset));
            return;
        }

        loadImmediate(imm, temp.memory());
        m_assembler.add_dInsn(temp.data(), temp.memory(), temp.data());

        resolution = resolveAddress(address, temp.memory());
        m_assembler.st_dInsn(temp.data(), resolution.base, Imm::I12(resolution.offset));
    }

    void add64(AbsoluteAddress address, RegisterID dest)
    {
        auto temp = temps<Memory>();
        loadImmediate(TrustedImmPtr(address.m_ptr), temp.memory());
        m_assembler.ld_dInsn(temp.memory(), temp.memory(), Imm::I12<0>());
        m_assembler.add_dInsn(dest, temp.memory(), dest);
    }

    void add64(Address address, RegisterID dest)
    {
        auto temp = temps<Data, Memory>();
        auto resolution = resolveAddress(address, temp.memory());
        m_assembler.ld_dInsn(temp.data(), resolution.base, Imm::I12(resolution.offset));
        m_assembler.add_dInsn(dest, temp.data(), dest);
    }

    void add8(TrustedImm32 imm, Address address)
    {
        load8(address, dataTempRegister);
        add32(imm, dataTempRegister, dataTempRegister);
        store8(dataTempRegister, address);
    }

    void sub32(RegisterID src, RegisterID dest)
    {
        sub32(dest, src, dest);
    }

    void sub32(RegisterID op1, RegisterID op2, RegisterID dest)
    {
        m_assembler.sub_wInsn(dest, op1, op2);
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

        if (Imm::I12Type::isSImm<12>(-imm.m_value)) {
            m_assembler.ld_wInsn(temp.data(), temp.memory(), Imm::I12<0>());
            m_assembler.addi_wInsn(temp.data(), temp.data(), Imm::I12(-imm.m_value));
            m_assembler.st_wInsn(temp.data(), temp.memory(), Imm::I12<0>());
            return;
        }

        m_assembler.ld_wInsn(temp.memory(), temp.memory(), Imm::I12<0>());
        loadImmediate(imm, temp.data());
        m_assembler.sub_wInsn(temp.data(), temp.memory(), temp.data());

        loadImmediate(TrustedImmPtr(address.m_ptr), temp.memory());
        m_assembler.st_wInsn(temp.data(), temp.memory(), Imm::I12<0>());
    }

    void sub32(TrustedImm32 imm, Address address)
    {
        auto temp = temps<Data, Memory>();
        auto resolution = resolveAddress(address, temp.memory());
        m_assembler.ld_wInsn(temp.data(), resolution.base, Imm::I12(resolution.offset));

        if (Imm::I12Type::isSImm<12>(-imm.m_value)) {
            m_assembler.addi_wInsn(temp.data(), temp.data(), Imm::I12(-imm.m_value));
            m_assembler.st_wInsn(temp.data(), resolution.base, Imm::I12(resolution.offset));
            return;
        }

        loadImmediate(imm, temp.memory());
        m_assembler.sub_wInsn(temp.data(), temp.data(), temp.memory());

        resolution = resolveAddress(address, temp.memory());
        m_assembler.st_wInsn(temp.data(), resolution.base, Imm::I12(resolution.offset));
    }

    void sub32(Address address, RegisterID dest)
    {
        auto temp = temps<Data, Memory>();
        auto resolution = resolveAddress(address, temp.memory());
        m_assembler.ld_wInsn(temp.data(), resolution.base, Imm::I12(resolution.offset));
        m_assembler.sub_wInsn(dest, dest, temp.data());
        m_assembler.maskRegister<32>(dest);
    }

    void sub64(RegisterID src, RegisterID dest)
    {
        sub64(dest, src, dest);
    }

    void sub64(RegisterID op1, RegisterID op2, RegisterID dest)
    {
        m_assembler.sub_dInsn(dest, op1, op2);
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
        m_assembler.mul_wInsn(dest, lhs, rhs);
        m_assembler.maskRegister<32>(dest);
    }

    void mul32(TrustedImm32 imm, RegisterID rhs, RegisterID dest)
    {
        auto temp = temps<Data>();
        loadImmediate(imm, temp.data());
        m_assembler.mul_wInsn(dest, temp.data(), rhs);
        m_assembler.maskRegister<32>(dest);
    }

    void mul64(RegisterID src, RegisterID dest)
    {
        mul64(src, dest, dest);
    }

    void mul64(RegisterID lhs, RegisterID rhs, RegisterID dest)
    {
        m_assembler.mul_dInsn(dest, lhs, rhs);
    }

    void extractUnsignedBitfield32(RegisterID src, TrustedImm32 lsb, TrustedImm32 width, RegisterID dest)
    {
        m_assembler.srli_dInsn(dest, src, std::clamp<int32_t>(lsb.m_value, 0, 31));
        if (!Imm::I12Type::isUImm<12>(width.m_value)) {
            auto temp = temps<Data>();
            loadImmediate(width, temp.data());
            m_assembler.andInsn(dest, dest, temp.data());
        } else
            m_assembler.andiInsn(dest, dest, Imm::I12((uint32_t)width.m_value));
    }

    void extractUnsignedBitfield64(RegisterID src, TrustedImm32 lsb, TrustedImm32 width, RegisterID dest)
    {
        m_assembler.srli_dInsn(dest, src, std::clamp<int32_t>(lsb.m_value, 0, 63));
        if (!Imm::I12Type::isUImm<12>(width.m_value)) {
            auto temp = temps<Data>();
            loadImmediate(width, temp.data());
            m_assembler.andInsn(dest, dest, temp.data());
        } else
            m_assembler.andiInsn(dest, dest, Imm::I12((uint32_t)width.m_value));
    }

    void insertUnsignedBitfieldInZero32(RegisterID src, TrustedImm32 lsb, TrustedImm32 width, RegisterID dest)
    {
        if (!Imm::I12Type::isUImm<12>(width.m_value)) {
            auto temp = temps<Data>();
            loadImmediate(width, temp.data());
            m_assembler.andInsn(dest, src, temp.data());
        } else
            m_assembler.andiInsn(dest, src, Imm::I12((uint32_t)width.m_value));
        m_assembler.slli_dInsn(dest, dest, std::clamp<int32_t>(lsb.m_value, 0, 63));
    }

    void insertUnsignedBitfieldInZero64(RegisterID src, TrustedImm32 lsb, TrustedImm32 width, RegisterID dest)
    {
        if (!Imm::I12Type::isUImm<12>(width.m_value)) {
            auto temp = temps<Data>();
            loadImmediate(width, temp.data());
            m_assembler.andInsn(dest, src, temp.data());
        } else
            m_assembler.andiInsn(dest, src, Imm::I12((uint32_t)width.m_value));
        m_assembler.slli_dInsn(dest, dest, std::clamp<int32_t>(lsb.m_value, 0, 63));
    }

    void countLeadingZeros32(RegisterID src, RegisterID dest)
    {
        auto temp = temps<Data>();
        m_assembler.zeroExtend<32>(temp.data(), src);
        m_assembler.addi_dInsn(dest, LOONGARCH64Registers::zero, Imm::I12<32>());

        JumpList zero(makeBranch(Equal, temp.data(), LOONGARCH64Registers::zero));

        Label loop = label();
        m_assembler.srli_dInsn(temp.data(), temp.data(), 1);
        m_assembler.addi_dInsn(dest, dest, Imm::I12<-1>());
        zero.append(makeBranch(Equal, temp.data(), LOONGARCH64Registers::zero));
        jump().linkTo(loop, this);

        zero.link(this);
    }

    void countLeadingZeros64(RegisterID src, RegisterID dest)
    {
        auto temp = temps<Data>();
        m_assembler.addi_dInsn(temp.data(), src, Imm::I12<0>());
        m_assembler.addi_dInsn(dest, LOONGARCH64Registers::zero, Imm::I12<64>());

        JumpList zero(makeBranch(Equal, temp.data(), LOONGARCH64Registers::zero));

        Label loop = label();
        m_assembler.srli_dInsn(temp.data(), temp.data(), 1);
        m_assembler.addi_dInsn(dest, dest, Imm::I12<-1>());
        zero.append(makeBranch(Equal, temp.data(), LOONGARCH64Registers::zero));
        jump().linkTo(loop, this);

        zero.link(this);
    }

    void countTrailingZeros32(RegisterID src, RegisterID dest)
    {
        auto temp = temps<Data>();
        m_assembler.addi_dInsn(dest, LOONGARCH64Registers::zero, Imm::I12<32>());
        m_assembler.zeroExtend<32>(temp.data(), src);

        JumpList zero(makeBranch(Equal, temp.data(), LOONGARCH64Registers::zero));

        Label loop = label();
        m_assembler.slli_dInsn(temp.data(), temp.data(), 1);
        m_assembler.addi_dInsn(dest, dest, Imm::I12<-1>());
        zero.append(makeBranch(Equal, temp.data(), LOONGARCH64Registers::zero));
        jump().linkTo(loop, this);

        zero.link(this);
    }

    void countTrailingZeros64(RegisterID src, RegisterID dest)
    {
        auto temp = temps<Data>();
        m_assembler.addi_dInsn(dest, LOONGARCH64Registers::zero, Imm::I12<64>());
        m_assembler.addi_dInsn(temp.data(), src, Imm::I12<0>());

        JumpList zero(makeBranch(Equal, temp.data(), LOONGARCH64Registers::zero));

        Label loop = label();
        m_assembler.slli_dInsn(temp.data(), temp.data(), 1);
        m_assembler.addi_dInsn(dest, dest, Imm::I12<-1>());
        zero.append(makeBranch(Equal, temp.data(), LOONGARCH64Registers::zero));
        jump().linkTo(loop, this);

        zero.link(this);
    }

    void byteSwap16(RegisterID reg)
    {
        auto temp = temps<Data>();
        m_assembler.andiInsn(temp.data(), reg, Imm::I12<0xff>());
        m_assembler.slli_dInsn(temp.data(), temp.data(), 8);
        m_assembler.slli_dInsn(reg, reg, 48);
        m_assembler.srli_dInsn(reg, reg, 56);
        m_assembler.orInsn(reg, reg, temp.data());
    }

    void byteSwap32(RegisterID reg)
    {
        auto temp = temps<Data, Memory>();
        m_assembler.andiInsn(temp.data(), reg, Imm::I12<0xff>());
        m_assembler.slli_dInsn(temp.data(), temp.data(), 8);
        m_assembler.srli_dInsn(reg, reg, 8);

        for (unsigned i = 0; i < 2; ++i) {
            m_assembler.andiInsn(temp.memory(), reg, Imm::I12<0xff>());
            m_assembler.orInsn(temp.data(), temp.data(), temp.memory());
            m_assembler.slli_dInsn(temp.data(), temp.data(), 8);
            m_assembler.srli_dInsn(reg, reg, 8);
        }

        m_assembler.andiInsn(temp.memory(), reg, Imm::I12<0xff>());
        m_assembler.orInsn(reg, temp.data(), temp.memory());
    }

    void byteSwap64(RegisterID reg)
    {
        auto temp = temps<Data, Memory>();
        RELEASE_ASSERT(reg != temp.data() && reg != temp.memory());
        m_assembler.andiInsn(temp.data(), reg, Imm::I12<0xff>());
        m_assembler.slli_dInsn(temp.data(), temp.data(), 8);
        m_assembler.srli_dInsn(reg, reg, 8);

        for (unsigned i = 0; i < 6; ++i) {
            m_assembler.andiInsn(temp.memory(), reg, Imm::I12<0xff>());
            m_assembler.orInsn(temp.data(), temp.data(), temp.memory());
            m_assembler.slli_dInsn(temp.data(), temp.data(), 8);
            m_assembler.srli_dInsn(reg, reg, 8);
        }

        m_assembler.andiInsn(temp.memory(), reg, Imm::I12<0xff>());
        m_assembler.orInsn(reg, temp.data(), temp.memory());
    }

    void lshift32(RegisterID shiftAmount, RegisterID dest)
    {
        lshift32(dest, shiftAmount, dest);
    }

    void lshift32(RegisterID src, RegisterID shiftAmount, RegisterID dest)
    {
        m_assembler.sll_wInsn(dest, src, shiftAmount);
        m_assembler.maskRegister<32>(dest);
    }

    void lshift32(TrustedImm32 shiftAmount, RegisterID dest)
    {
        lshift32(dest, shiftAmount, dest);
    }

    void lshift32(RegisterID src, TrustedImm32 imm, RegisterID dest)
    {
        m_assembler.slli_wInsn(dest, src, imm.m_value & 0x1f);
        m_assembler.maskRegister<32>(dest);
    }

    void lshift32(TrustedImm32 imm, RegisterID shiftAmount, RegisterID dest)
    {
        auto temp = temps<Data>();
        loadImmediate(imm, temp.data());
        m_assembler.sll_wInsn(dest, temp.data(), shiftAmount);
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
        m_assembler.sll_dInsn(dest, src, shiftAmount);
    }

    void lshift64(TrustedImm32 shiftAmount, RegisterID dest)
    {
        lshift64(dest, shiftAmount, dest);
    }

    void lshift64(RegisterID src, TrustedImm32 imm, RegisterID dest)
    {
        if (UNLIKELY(!imm.m_value))
            return move(src, dest);
        m_assembler.slli_dInsn(dest, src, imm.m_value & 0x3f);
    }

    void lshift64(TrustedImm32 imm, RegisterID shiftAmount, RegisterID dest)
    {
        auto temp = temps<Data>();
        loadImmediate(imm, temp.data());
        m_assembler.sll_wInsn(dest, temp.data(), shiftAmount);
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
        m_assembler.sra_wInsn(dest, src, shiftAmount);
        m_assembler.maskRegister<32>(dest);
    }

    void rshift32(TrustedImm32 shiftAmount, RegisterID dest)
    {
        rshift32(dest, shiftAmount, dest);
    }

    void rshift32(RegisterID src, TrustedImm32 imm, RegisterID dest)
    {
        m_assembler.srai_wInsn(dest, src, imm.m_value & 0x1f);
        m_assembler.maskRegister<32>(dest);
    }

    void rshift64(RegisterID shiftAmount, RegisterID dest)
    {
        rshift64(dest, shiftAmount, dest);
    }

    void rshift64(RegisterID src, RegisterID shiftAmount, RegisterID dest)
    {
        m_assembler.sra_dInsn(dest, src, shiftAmount);
    }

    void rshift64(TrustedImm32 shiftAmount, RegisterID dest)
    {
        rshift64(dest, shiftAmount, dest);
    }

    void rshift64(RegisterID src, TrustedImm32 imm, RegisterID dest)
    {
        if (UNLIKELY(!imm.m_value))
            return move(src, dest);
        m_assembler.srai_dInsn(dest, src, imm.m_value & 0x3f);
    }

    void urshift32(RegisterID shiftAmount, RegisterID dest)
    {
        urshift32(dest, shiftAmount, dest);
    }

    void urshift32(RegisterID src, RegisterID shiftAmount, RegisterID dest)
    {
        m_assembler.srl_wInsn(dest, src, shiftAmount);
        m_assembler.maskRegister<32>(dest);
    }

    void urshift32(TrustedImm32 shiftAmount, RegisterID dest)
    {
        urshift32(dest, shiftAmount, dest);
    }

    void urshift32(RegisterID src, TrustedImm32 imm, RegisterID dest)
    {
        m_assembler.srli_wInsn(dest, src, imm.m_value & 0x1f);
        m_assembler.maskRegister<32>(dest);
    }

    void addUnsignedRightShift32(RegisterID src1, RegisterID src2, TrustedImm32 amount, RegisterID dest)
    {
        // dest = src1 + (src2 >> amount)
        urshift32(src2, amount, dataTempRegister);
        add32(src1, dataTempRegister, dest);
    }

    void urshift64(RegisterID shiftAmount, RegisterID dest)
    {
        urshift64(dest, shiftAmount, dest);
    }

    void urshift64(RegisterID src, RegisterID shiftAmount, RegisterID dest)
    {
        m_assembler.srl_dInsn(dest, src, shiftAmount);
    }

    void urshift64(TrustedImm32 shiftAmount, RegisterID dest)
    {
        urshift64(dest, shiftAmount, dest);
    }

    void urshift64(RegisterID src, TrustedImm32 imm, RegisterID dest)
    {
        if (UNLIKELY(!imm.m_value))
            return move(src, dest);
        m_assembler.srli_dInsn(dest, src, imm.m_value & 0x3f);
    }

    MACRO_ASSEMBLER_LOONGARCH64_TEMPLATED_NOOP_METHOD(rotateRight32);
    MACRO_ASSEMBLER_LOONGARCH64_TEMPLATED_NOOP_METHOD(rotateRight64);

    void load8(Address address, RegisterID dest)
    {
        auto resolution = resolveAddress(address, lazyTemp<Memory>());
        m_assembler.ld_buInsn(dest, resolution.base, Imm::I12(resolution.offset));
    }

    void load8(BaseIndex address, RegisterID dest)
    {
        auto resolution = resolveAddress(address, lazyTemp<Memory>());
        m_assembler.ld_buInsn(dest, resolution.base, Imm::I12(resolution.offset));
    }

    void load8(const void* address, RegisterID dest)
    {
        auto temp = temps<Memory>();
        loadImmediate(TrustedImmPtr(address), temp.memory());
        m_assembler.ld_buInsn(dest, temp.memory(), Imm::I12<0>());
    }

    void load8SignedExtendTo32(Address address, RegisterID dest)
    {
        auto resolution = resolveAddress(address, lazyTemp<Memory>());
        m_assembler.ld_bInsn(dest, resolution.base, Imm::I12(resolution.offset));
        m_assembler.maskRegister<32>(dest);
    }

    void load8SignedExtendTo32(BaseIndex address, RegisterID dest)
    {
        auto resolution = resolveAddress(address, lazyTemp<Memory>());
        m_assembler.ld_bInsn(dest, resolution.base, Imm::I12(resolution.offset));
        m_assembler.maskRegister<32>(dest);
    }

    void load8SignedExtendTo32(const void* address, RegisterID dest)
    {
        auto temp = temps<Memory>();
        loadImmediate(TrustedImmPtr(address), temp.memory());
        m_assembler.ld_bInsn(dest, temp.memory(), Imm::I12<0>());
        m_assembler.maskRegister<32>(dest);
    }

    void load16(Address address, RegisterID dest)
    {
        auto resolution = resolveAddress(address, lazyTemp<Memory>());
        m_assembler.ld_huInsn(dest, resolution.base, Imm::I12(resolution.offset));
    }

    void load16(BaseIndex address, RegisterID dest)
    {
        auto resolution = resolveAddress(address, lazyTemp<Memory>());
        m_assembler.ld_huInsn(dest, resolution.base, Imm::I12(resolution.offset));
    }

    void load16(const void* address, RegisterID dest)
    {
        auto temp = temps<Memory>();
        loadImmediate(TrustedImmPtr(address), temp.memory());
        m_assembler.ld_huInsn(dest, temp.memory(), Imm::I12<0>());
    }

    void load16(ExtendedAddress address, RegisterID dest)
    {
        auto temp = temps<Data, Memory>();
        loadImmediate(TrustedImm64(int64_t(address.offset)), temp.memory());
        m_assembler.slli_dInsn(temp.data(), address.base, 1);
        m_assembler.add_dInsn(temp.memory(), temp.memory(), temp.data());
        m_assembler.ld_huInsn(dest, temp.memory(), Imm::I12<0>());
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
        m_assembler.ld_hInsn(dest, resolution.base, Imm::I12(resolution.offset));
        m_assembler.maskRegister<32>(dest);
    }

    void load16SignedExtendTo32(BaseIndex address, RegisterID dest)
    {
        auto resolution = resolveAddress(address, lazyTemp<Memory>());
        m_assembler.ld_hInsn(dest, resolution.base, Imm::I12(resolution.offset));
        m_assembler.maskRegister<32>(dest);
    }

    void load16SignedExtendTo32(const void* address, RegisterID dest)
    {
        auto temp = temps<Memory>();
        loadImmediate(TrustedImmPtr(address), temp.memory());
        m_assembler.ld_hInsn(dest, temp.memory(), Imm::I12<0>());
        m_assembler.maskRegister<32>(dest);
    }

    void load32(Address address, RegisterID dest)
    {
        auto resolution = resolveAddress(address, lazyTemp<Memory>());
        m_assembler.ld_wuInsn(dest, resolution.base, Imm::I12(resolution.offset));
    }

    void load32(BaseIndex address, RegisterID dest)
    {
        auto resolution = resolveAddress(address, lazyTemp<Memory>());
        m_assembler.ld_wuInsn(dest, resolution.base, Imm::I12(resolution.offset));
    }

    void load32(const void* address, RegisterID dest)
    {
        auto temp = temps<Memory>();
        loadImmediate(TrustedImmPtr(address), temp.memory());
        m_assembler.ld_wuInsn(dest, temp.memory(), Imm::I12<0>());
    }

    void load32WithUnalignedHalfWords(BaseIndex address, RegisterID dest)
    {
        load32(address, dest);
    }

    void load64(Address address, RegisterID dest)
    {
        auto resolution = resolveAddress(address, lazyTemp<Memory>());
        m_assembler.ld_dInsn(dest, resolution.base, Imm::I12(resolution.offset));
    }

    void load64(BaseIndex address, RegisterID dest)
    {
        auto resolution = resolveAddress(address, lazyTemp<Memory>());
        m_assembler.ld_dInsn(dest, resolution.base, Imm::I12(resolution.offset));
    }

    void load64(const void* address, RegisterID dest)
    {
        auto temp = temps<Memory>();
        loadImmediate(TrustedImmPtr(address), temp.memory());
        m_assembler.ld_dInsn(dest, temp.memory(), Imm::I12<0>());
    }

    void loadPair32(RegisterID src, RegisterID dest1, RegisterID dest2)
    {
        loadPair32(src, TrustedImm32(0), dest1, dest2);
    }

    void loadPair32(RegisterID src, TrustedImm32 offset, RegisterID dest1, RegisterID dest2)
    {
        RELEASE_ASSERT(dest1 != dest2);
        if (src == dest1) {
            load32(Address(src, offset.m_value + 4), dest2);
            load32(Address(src, offset.m_value), dest1);
        } else {
            load32(Address(src, offset.m_value), dest1);
            load32(Address(src, offset.m_value + 4), dest2);
        }
    }

    void loadPair32(Address src, RegisterID dest1, RegisterID dest2)
    {
        loadPair32(src.base, TrustedImm32(src.offset), dest1, dest2);
    }

    void loadPair64(RegisterID src, RegisterID dest1, RegisterID dest2)
    {
        loadPair64(src, TrustedImm32(0), dest1, dest2);
    }

    void loadPair64(RegisterID src, TrustedImm32 offset, RegisterID dest1, RegisterID dest2)
    {
        RELEASE_ASSERT(dest1 != dest2);
        if (src == dest1) {
            load64(Address(src, offset.m_value + 8), dest2);
            load64(Address(src, offset.m_value), dest1);
        } else {
            load64(Address(src, offset.m_value), dest1);
            load64(Address(src, offset.m_value + 8), dest2);
        }
    }

    void loadPair64(Address src, RegisterID dest1, RegisterID dest2)
    {
        loadPair64(src.base, TrustedImm32(src.offset), dest1, dest2);
    }

    void store8(RegisterID src, Address address)
    {
        auto resolution = resolveAddress(address, lazyTemp<Memory>());
        m_assembler.st_bInsn(src, resolution.base, Imm::I12(resolution.offset));
    }

    void store8(RegisterID src, BaseIndex address)
    {
        auto resolution = resolveAddress(address, lazyTemp<Memory>());
        m_assembler.st_bInsn(src, resolution.base, Imm::I12(resolution.offset));
    }

    void store8(RegisterID src, const void* address)
    {
        auto temp = temps<Memory>();
        loadImmediate(TrustedImmPtr(address), temp.memory());
        m_assembler.st_bInsn(src, temp.memory(), Imm::I12<0>());
    }

    void store8(TrustedImm32 imm, Address address)
    {
        auto temp = temps<Data, Memory>();
        RegisterID immRegister = LOONGARCH64Registers::zero;
        TrustedImm32 imm8(int8_t(imm.m_value));
        if (!!imm8.m_value) {
            loadImmediate(imm8, temp.data());
            immRegister = temp.data();
        }

        auto resolution = resolveAddress(address, temp.memory());
        m_assembler.st_bInsn(immRegister, resolution.base, Imm::I12(resolution.offset));
    }

    void store8(TrustedImm32 imm, BaseIndex address)
    {
        auto temp = temps<Data, Memory>();
        RegisterID immRegister = LOONGARCH64Registers::zero;
        TrustedImm32 imm8(int8_t(imm.m_value));
        if (!!imm8.m_value) {
            loadImmediate(imm8, temp.data());
            immRegister = temp.data();
        }

        auto resolution = resolveAddress(address, temp.memory());
        m_assembler.st_bInsn(immRegister, resolution.base, Imm::I12(resolution.offset));
    }

    void store8(TrustedImm32 imm, const void* address)
    {
        auto temp = temps<Memory, Data>();
        RegisterID immRegister = LOONGARCH64Registers::zero;
        TrustedImm32 imm8(int8_t(imm.m_value));
        if (!!imm8.m_value) {
            loadImmediate(imm8, temp.data());
            immRegister = temp.data();
        }

        loadImmediate(TrustedImmPtr(address), temp.memory());
        m_assembler.st_bInsn(immRegister, temp.memory(), Imm::I12<0>());
    }

    void store16(RegisterID src, Address address)
    {
        auto resolution = resolveAddress(address, lazyTemp<Memory>());
        m_assembler.st_hInsn(src, resolution.base, Imm::I12(resolution.offset));
    }

    void store16(RegisterID src, BaseIndex address)
    {
        auto resolution = resolveAddress(address, lazyTemp<Memory>());
        m_assembler.st_hInsn(src, resolution.base, Imm::I12(resolution.offset));
    }

    void store16(RegisterID src, const void* address)
    {
        auto temp = temps<Memory>();
        loadImmediate(TrustedImmPtr(address), temp.memory());
        m_assembler.st_hInsn(src, temp.memory(), Imm::I12<0>());
    }

    void store16(TrustedImm32 imm, Address address)
    {
        auto temp = temps<Data, Memory>();
        RegisterID immRegister = LOONGARCH64Registers::zero;
        TrustedImm32 imm16(int16_t(imm.m_value));
        if (!!imm16.m_value) {
            loadImmediate(imm16, temp.data());
            immRegister = temp.data();
        }

        auto resolution = resolveAddress(address, temp.memory());
        m_assembler.st_hInsn(immRegister, resolution.base, Imm::I12(resolution.offset));
    }

    void store16(TrustedImm32 imm, BaseIndex address)
    {
        auto temp = temps<Data, Memory>();
        RegisterID immRegister = LOONGARCH64Registers::zero;
        TrustedImm32 imm16(int16_t(imm.m_value));
        if (!!imm16.m_value) {
            loadImmediate(imm16, temp.data());
            immRegister = temp.data();
        }

        auto resolution = resolveAddress(address, lazyTemp<Memory>());
        m_assembler.st_hInsn(immRegister, resolution.base, Imm::I12(resolution.offset));
    }

    void store16(TrustedImm32 imm, const void* address)
    {
        auto temp = temps<Data, Memory>();
        RegisterID immRegister = LOONGARCH64Registers::zero;
        TrustedImm32 imm16(int16_t(imm.m_value));
        if (!!imm16.m_value) {
            loadImmediate(imm16, temp.data());
            immRegister = temp.data();
        }

        loadImmediate(TrustedImmPtr(address), temp.memory());
        m_assembler.st_hInsn(immRegister, temp.memory(), Imm::I12<0>());
    }

    void store32(RegisterID src, Address address)
    {
        auto resolution = resolveAddress(address, lazyTemp<Memory>());
        m_assembler.st_wInsn(src, resolution.base, Imm::I12(resolution.offset));
    }

    void store32(RegisterID src, BaseIndex address)
    {
        auto resolution = resolveAddress(address, lazyTemp<Memory>());
        m_assembler.st_wInsn(src, resolution.base, Imm::I12(resolution.offset));
    }

    void store32(RegisterID src, const void* address)
    {
        auto temp = temps<Memory>();
        loadImmediate(TrustedImmPtr(address), temp.memory());
        m_assembler.st_wInsn(src, temp.memory(), Imm::I12<0>());
    }

    void store32(TrustedImm32 imm, Address address)
    {
        auto temp = temps<Data, Memory>();
        RegisterID immRegister = LOONGARCH64Registers::zero;
        if (!!imm.m_value) {
            loadImmediate(imm, temp.data());
            immRegister = temp.data();
        }

        auto resolution = resolveAddress(address, temp.memory());
        m_assembler.st_wInsn(immRegister, resolution.base, Imm::I12(resolution.offset));
    }

    void store32(TrustedImm32 imm, BaseIndex address)
    {
        auto temp = temps<Data, Memory>();
        RegisterID immRegister = LOONGARCH64Registers::zero;
        if (!!imm.m_value) {
            loadImmediate(imm, temp.data());
            immRegister = temp.data();
        }

        auto resolution = resolveAddress(address, lazyTemp<Memory>());
        m_assembler.st_wInsn(immRegister, resolution.base, Imm::I12(resolution.offset));
    }

    void store32(TrustedImm32 imm, const void* address)
    {
        auto temp = temps<Data, Memory>();
        RegisterID immRegister = LOONGARCH64Registers::zero;
        if (!!imm.m_value) {
            loadImmediate(imm, temp.data());
            immRegister = temp.data();
        }

        loadImmediate(TrustedImmPtr(address), temp.memory());
        m_assembler.st_wInsn(immRegister, temp.memory(), Imm::I12<0>());
    }

    void store64(RegisterID src, Address address)
    {
        auto resolution = resolveAddress(address, lazyTemp<Memory>());
        m_assembler.st_dInsn(src, resolution.base, Imm::I12(resolution.offset));
    }

    void store64(RegisterID src, BaseIndex address)
    {
        auto resolution = resolveAddress(address, lazyTemp<Memory>());
        m_assembler.st_dInsn(src, resolution.base, Imm::I12(resolution.offset));
    }

    void store64(RegisterID src, const void* address)
    {
        auto temp = temps<Memory>();
        loadImmediate(TrustedImmPtr(address), temp.memory());
        m_assembler.st_dInsn(src, temp.memory(), Imm::I12<0>());
    }

    void store64(TrustedImm32 imm, Address address)
    {
        auto temp = temps<Data, Memory>();
        RegisterID immRegister = LOONGARCH64Registers::zero;
        if (!!imm.m_value) {
            loadImmediate(imm, temp.data());
            m_assembler.maskRegister<32>(temp.data());
            immRegister = temp.data();
        }

        auto resolution = resolveAddress(address, temp.memory());
        m_assembler.st_dInsn(immRegister, resolution.base, Imm::I12(resolution.offset));
    }

    void store64(TrustedImm64 imm, Address address)
    {
        auto temp = temps<Data, Memory>();
        RegisterID immRegister = LOONGARCH64Registers::zero;
        if (!!imm.m_value) {
            loadImmediate(imm, temp.data());
            immRegister = temp.data();
        }

        auto resolution = resolveAddress(address, temp.memory());
        m_assembler.st_dInsn(immRegister, resolution.base, Imm::I12(resolution.offset));
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
        RegisterID immRegister = LOONGARCH64Registers::zero;
        if (!!imm.m_value) {
            loadImmediate(imm, temp.data());
            immRegister = temp.data();
        }

        auto resolution = resolveAddress(address, temp.memory());
        m_assembler.st_dInsn(immRegister, resolution.base, Imm::I12(resolution.offset));
    }

    void store64(TrustedImm64 imm, const void* address)
    {
        auto temp = temps<Data, Memory>();
        RegisterID immRegister = LOONGARCH64Registers::zero;
        if (!!imm.m_value) {
            loadImmediate(imm, temp.data());
            immRegister = temp.data();
        }

        loadImmediate(TrustedImmPtr(address), temp.memory());
        m_assembler.st_dInsn(immRegister, temp.memory(), Imm::I12<0>());
    }

    void transfer32(Address src, Address dest)
    {
        auto temp = temps<Data>();
        load32(src, temp.data());
        store32(temp.data(), dest);
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

    void transfer32(BaseIndex src, BaseIndex dest)
    {
        auto temp = temps<Data>();
        load32(src, temp.data());
        store32(temp.data(), dest);
    }

    void transfer64(BaseIndex src, BaseIndex dest)
    {
        auto temp = temps<Data>();
        load64(src, temp.data());
        store64(temp.data(), dest);
    }

    void transferPtr(BaseIndex src, BaseIndex dest)
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

    void storePair32(RegisterID src1, RegisterID src2, Address dest)
    {
        storePair32(src1, src2, dest.base, TrustedImm32(dest.offset));
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

    void storePair64(RegisterID src1, RegisterID src2, Address dest)
    {
        storePair64(src1, src2, dest.base, TrustedImm32(dest.offset));
    }

    void zeroExtend8To32(RegisterID src, RegisterID dest)
    {
        m_assembler.slli_dInsn(dest, src, 56);
        m_assembler.srli_dInsn(dest, dest, 56);
    }

    void zeroExtend16To32(RegisterID src, RegisterID dest)
    {
        m_assembler.slli_dInsn(dest, src, 48);
        m_assembler.srli_dInsn(dest, dest, 48);
    }

    void zeroExtend32ToWord(RegisterID src, RegisterID dest)
    {
        m_assembler.slli_dInsn(dest, src, 32);
        m_assembler.srli_dInsn(dest, dest, 32);
    }

    void signExtend8To32(RegisterID src, RegisterID dest)
    {
        m_assembler.slli_dInsn(dest, src, 56);
        m_assembler.srai_dInsn(dest, dest, 24);
        m_assembler.srli_dInsn(dest, dest, 32);
    }

    void signExtend16To32(RegisterID src, RegisterID dest)
    {
        m_assembler.slli_dInsn(dest, src, 48);
        m_assembler.srai_dInsn(dest, dest, 16);
        m_assembler.srli_dInsn(dest, dest, 32);
    }

    void zeroExtend8To64(RegisterID src, RegisterID dest)
    {
        zeroExtend8To32(src, dest);
    }

    void zeroExtend16To64(RegisterID src, RegisterID dest)
    {
        zeroExtend16To32(src, dest);
    }

    void signExtend8To64(RegisterID src, RegisterID dest)
    {
        signExtend8To32(src, dest);
        signExtend32To64(dest, dest);
    }

    void signExtend16To64(RegisterID src, RegisterID dest)
    {
        signExtend16To32(src, dest);
        signExtend32To64(dest, dest);
    }

    void signExtend32ToPtr(RegisterID src, RegisterID dest)
    {
        signExtend32To64(src, dest);
    }

    void signExtend32ToPtr(TrustedImm32 imm, RegisterID dest)
    {
        signExtend32To64(imm, dest);
    }

    void signExtend32To64(RegisterID src, RegisterID dest)
    {
        m_assembler.addi_wInsn(dest, src, Imm::I12<0>());
    }

    void signExtend32To64(TrustedImm32 imm, RegisterID dest)
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
        if (!Imm::I12Type::isUImm<12>(imm.m_value)) {
            auto temp = temps<Data>();
            loadImmediate(imm, temp.data());
            m_assembler.andInsn(dest, temp.data(), op2);
        } else
            m_assembler.andiInsn(dest, op2, Imm::I12((uint32_t)imm.m_value));
        m_assembler.maskRegister<32>(dest);
    }

    void and32(Address address, RegisterID dest)
    {
        auto temp = temps<Data, Memory>();
        auto resolution = resolveAddress(address, temp.memory());
        m_assembler.ld_wInsn(temp.data(), resolution.base, Imm::I12(resolution.offset));
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
        if (Imm::I12Type::isUImm<12>(imm.m_value)) {
            m_assembler.andiInsn(dest, op2, Imm::I12((uint32_t)imm.m_value));
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
        if (Imm::I12Type::isUImm<12>(imm.m_value)) {
            m_assembler.andiInsn(dest, op2, Imm::I12((uint32_t)imm.m_value));
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
        m_assembler.ld_bInsn(temp.data(), temp.memory(), Imm::I12<0>());
        m_assembler.orInsn(temp.data(), src, temp.data());
        m_assembler.st_bInsn(temp.data(), temp.memory(), Imm::I12<0>());
    }

    void or8(TrustedImm32 imm, AbsoluteAddress address)
    {
        auto temp = temps<Data, Memory>();
        loadImmediate(TrustedImmPtr(address.m_ptr), temp.memory());
        m_assembler.ld_bInsn(temp.data(), temp.memory(), Imm::I12<0>());

        if (Imm::I12Type::isUImm<12>(imm.m_value)) {
            m_assembler.oriInsn(temp.data(), temp.data(), Imm::I12((uint32_t)imm.m_value));
            m_assembler.st_bInsn(temp.data(), temp.memory(), Imm::I12<0>());
        } else {
            loadImmediate(imm, temp.memory());
            m_assembler.orInsn(temp.data(), temp.data(), temp.memory());
            loadImmediate(TrustedImmPtr(address.m_ptr), temp.memory());
            m_assembler.st_bInsn(temp.data(), temp.memory(), Imm::I12<0>());
        }
    }

    void or16(RegisterID src, AbsoluteAddress address)
    {
        auto temp = temps<Data, Memory>();
        loadImmediate(TrustedImmPtr(address.m_ptr), temp.memory());
        m_assembler.ld_hInsn(temp.data(), temp.memory(), Imm::I12<0>());
        m_assembler.orInsn(temp.data(), src, temp.data());
        m_assembler.st_hInsn(temp.data(), temp.memory(), Imm::I12<0>());
    }

    void or16(TrustedImm32 imm, AbsoluteAddress address)
    {
        auto temp = temps<Data, Memory>();
        loadImmediate(TrustedImmPtr(address.m_ptr), temp.memory());
        m_assembler.ld_hInsn(temp.data(), temp.memory(), Imm::I12<0>());

        if (Imm::I12Type::isUImm<12>(imm.m_value)) {
            m_assembler.oriInsn(temp.data(), temp.data(), Imm::I12((uint32_t)imm.m_value));
            m_assembler.st_hInsn(temp.data(), temp.memory(), Imm::I12<0>());
        } else {
            loadImmediate(imm, temp.memory());
            m_assembler.orInsn(temp.data(), temp.data(), temp.memory());
            loadImmediate(TrustedImmPtr(address.m_ptr), temp.memory());
            m_assembler.st_hInsn(temp.data(), temp.memory(), Imm::I12<0>());
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
        if (Imm::I12Type::isUImm<12>(imm.m_value)) {
            m_assembler.oriInsn(dest, op2, Imm::I12((uint32_t)imm.m_value));
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
        m_assembler.ld_wInsn(temp.data(), temp.memory(), Imm::I12<0>());
        m_assembler.orInsn(temp.data(), src, temp.data());
        m_assembler.st_wInsn(temp.data(), temp.memory(), Imm::I12<0>());
    }

    void or32(TrustedImm32 imm, AbsoluteAddress address)
    {
        auto temp = temps<Data, Memory>();
        loadImmediate(TrustedImmPtr(address.m_ptr), temp.memory());
        m_assembler.ld_wInsn(temp.data(), temp.memory(), Imm::I12<0>());

        if (Imm::I12Type::isUImm<12>(imm.m_value)) {
            m_assembler.oriInsn(temp.data(), temp.data(), Imm::I12((uint32_t)imm.m_value));
            m_assembler.st_wInsn(temp.data(), temp.memory(), Imm::I12<0>());
        } else {
            loadImmediate(imm, temp.memory());
            m_assembler.orInsn(temp.data(), temp.data(), temp.memory());
            loadImmediate(TrustedImmPtr(address.m_ptr), temp.memory());
            m_assembler.st_wInsn(temp.data(), temp.memory(), Imm::I12<0>());
        }
    }

    void or32(TrustedImm32 imm, Address address)
    {
        auto temp = temps<Data, Memory>();
        auto resolution = resolveAddress(address, temp.memory());
        m_assembler.ld_wInsn(temp.data(), resolution.base, Imm::I12(resolution.offset));

        if (Imm::I12Type::isUImm<12>(imm.m_value)) {
            m_assembler.oriInsn(temp.data(), temp.memory(), Imm::I12((uint32_t)imm.m_value));
            m_assembler.st_wInsn(temp.data(), resolution.base, Imm::I12(resolution.offset));
        } else {
            loadImmediate(imm, temp.memory());
            m_assembler.orInsn(temp.data(), temp.data(), temp.memory());
            resolution = resolveAddress(address, temp.memory());
            m_assembler.st_wInsn(temp.data(), resolution.base, Imm::I12(resolution.offset));
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
        if (Imm::I12Type::isUImm<12>(imm.m_value)) {
            m_assembler.oriInsn(dest, op2, Imm::I12((uint32_t)imm.m_value));
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
        if (Imm::I12Type::isUImm<12>(imm.m_value)) {
            m_assembler.oriInsn(dest, op2, Imm::I12((uint32_t)imm.m_value));
            return;
        }

        auto temp = temps<Data>();
        RELEASE_ASSERT(dest != temp.data() && op2 != temp.data());
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
        if (Imm::I12Type::isUImm<12>(imm.m_value)) {
            m_assembler.xoriInsn(dest, op2, Imm::I12((uint32_t)imm.m_value));
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
        m_assembler.ld_wInsn(temp.data(), resolution.base, Imm::I12(resolution.offset));
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
        if (Imm::I12Type::isUImm<12>(imm.m_value)) {
            m_assembler.xoriInsn(dest, op2, Imm::I12((uint32_t)imm.m_value));
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
        if (Imm::I12Type::isUImm<12>(imm.m_value)) {
            m_assembler.xoriInsn(dest, op2, Imm::I12((uint32_t)imm.m_value));
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
        m_assembler.ld_dInsn(temp.data(), resolution.base, Imm::I12(resolution.offset));
        m_assembler.xorInsn(dest, temp.data(), dest);
    }

    void xor64(RegisterID src, Address address)
    {
        auto temp = temps<Data, Memory>();
        auto resolution = resolveAddress(address, temp.memory());
        m_assembler.ld_dInsn(temp.data(), resolution.base, Imm::I12(resolution.offset));
        m_assembler.xorInsn(temp.data(), src, temp.data());
        m_assembler.st_dInsn(temp.data(), resolution.base, Imm::I12(resolution.offset));
    }

    void not32(RegisterID dest)
    {
        not32(dest, dest);
    }

    void not32(RegisterID src, RegisterID dest)
    {
        auto temp = temps<Data>();
        loadImmediate(TrustedImm32(-1), temp.data());
        m_assembler.xorInsn(dest, src, temp.data());
        m_assembler.maskRegister<32>(dest);
    }

    void not64(RegisterID dest)
    {
        not64(dest, dest);
    }

    void not64(RegisterID src, RegisterID dest)
    {
        auto temp = temps<Data>();
        loadImmediate(TrustedImm32(-1), temp.data());
        m_assembler.xorInsn(dest, src, temp.data());
    }

    void neg32(RegisterID dest)
    {
        neg32(dest, dest);
    }

    void neg32(RegisterID src, RegisterID dest)
    {
        m_assembler.sub_wInsn(dest, LOONGARCH64Registers::zero, src);
        m_assembler.maskRegister<32>(dest);
    }

    void neg64(RegisterID dest)
    {
        neg64(dest, dest);
    }

    void neg64(RegisterID src, RegisterID dest)
    {
        m_assembler.sub_dInsn(dest, LOONGARCH64Registers::zero, src);
    }

    void move(RegisterID src, RegisterID dest)
    {
        if (src != dest)
            m_assembler.orInsn(dest, src, LOONGARCH64Registers::zero);
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

    void swapDouble(FPRegisterID reg1, FPRegisterID reg2)
    {
        if (reg1 == reg2)
            return;
        RELEASE_ASSERT(reg1 != fpTempRegister && reg2 != fpTempRegister);
        moveDouble(reg1, fpTempRegister);
        moveDouble(reg2, reg1);
        moveDouble(fpTempRegister, reg2);
    }

    void moveZeroToFloat(FPRegisterID dest)
    {
        m_assembler.movgr2fr_wInsn(dest, LOONGARCH64Registers::zero);
    }

    void moveZeroToDouble(FPRegisterID dest)
    {
        m_assembler.movgr2fr_dInsn(dest, LOONGARCH64Registers::zero);
    }

    void moveFloat(FPRegisterID src, FPRegisterID dest)
    {
        m_assembler.fcopysignInsn<32>(dest, src, src);
    }

    void moveFloatTo32(FPRegisterID src, RegisterID dest)
    {
        m_assembler.movfr2grInsn<32>(dest, src);
    }

    void move32ToFloat(RegisterID src, FPRegisterID dest)
    {
        m_assembler.movgr2fr_wInsn(dest, src);
    }

    void move32ToFloat(TrustedImm32 imm, FPRegisterID dest)
    {
        if (!imm.m_value) {
            moveZeroToFloat(dest);
            return;
        }
        move(imm, scratchRegister());
        move32ToFloat(scratchRegister(), dest);
    }

    void moveDouble(FPRegisterID src, FPRegisterID dest)
    {
        m_assembler.fcopysignInsn<64>(dest, src, src);
    }

    void moveDoubleTo64(FPRegisterID src, RegisterID dest)
    {
        m_assembler.movfr2grInsn<64>(dest, src);
    }

    void move64ToDouble(RegisterID src, FPRegisterID dest)
    {
        m_assembler.movgr2fr_dInsn(dest, src);
    }

    void move64ToDouble(TrustedImm64 imm, FPRegisterID dest)
    {
        if (!imm.m_value) {
            moveZeroToDouble(dest);
            return;
        }
        move(imm, scratchRegister());
        move64ToDouble(scratchRegister(), dest);
    }

    static bool supportsCountPopulation() { return false; }
    MACRO_ASSEMBLER_LOONGARCH64_TEMPLATED_NOOP_METHOD(countPopulation32);
    MACRO_ASSEMBLER_LOONGARCH64_TEMPLATED_NOOP_METHOD(countPopulation64);

    // The LSX and LASX are not yet implemented
    static bool supportsLSX()  { return false; }
    static bool supportsLASX() { return false; }
    MACRO_ASSEMBLER_LOONGARCH64_TEMPLATED_NOOP_METHOD(moveVector);
    MACRO_ASSEMBLER_LOONGARCH64_TEMPLATED_NOOP_METHOD(loadVector);
    MACRO_ASSEMBLER_LOONGARCH64_TEMPLATED_NOOP_METHOD(storeVector);
    MACRO_ASSEMBLER_LOONGARCH64_TEMPLATED_NOOP_METHOD(vectorReplaceLaneInt64);
    MACRO_ASSEMBLER_LOONGARCH64_TEMPLATED_NOOP_METHOD(vectorReplaceLaneInt32);
    MACRO_ASSEMBLER_LOONGARCH64_TEMPLATED_NOOP_METHOD(vectorReplaceLaneInt16);
    MACRO_ASSEMBLER_LOONGARCH64_TEMPLATED_NOOP_METHOD(vectorReplaceLaneInt8);
    MACRO_ASSEMBLER_LOONGARCH64_TEMPLATED_NOOP_METHOD(vectorReplaceLaneFloat64);
    MACRO_ASSEMBLER_LOONGARCH64_TEMPLATED_NOOP_METHOD(vectorReplaceLaneFloat32);
    MACRO_ASSEMBLER_LOONGARCH64_TEMPLATED_NOOP_METHOD(vectorExtractLaneInt64);
    MACRO_ASSEMBLER_LOONGARCH64_TEMPLATED_NOOP_METHOD(vectorExtractLaneInt32);
    MACRO_ASSEMBLER_LOONGARCH64_TEMPLATED_NOOP_METHOD(vectorExtractLaneSignedInt16);
    MACRO_ASSEMBLER_LOONGARCH64_TEMPLATED_NOOP_METHOD(vectorExtractLaneUnsignedInt16);
    MACRO_ASSEMBLER_LOONGARCH64_TEMPLATED_NOOP_METHOD(vectorExtractLaneSignedInt8);
    MACRO_ASSEMBLER_LOONGARCH64_TEMPLATED_NOOP_METHOD(vectorExtractLaneUnsignedInt8);
    MACRO_ASSEMBLER_LOONGARCH64_TEMPLATED_NOOP_METHOD(vectorExtractLaneFloat64);
    MACRO_ASSEMBLER_LOONGARCH64_TEMPLATED_NOOP_METHOD(vectorExtractLaneFloat32);
    MACRO_ASSEMBLER_LOONGARCH64_TEMPLATED_NOOP_METHOD(vectorDupElementInt8);
    MACRO_ASSEMBLER_LOONGARCH64_TEMPLATED_NOOP_METHOD(vectorDupElementInt16);
    MACRO_ASSEMBLER_LOONGARCH64_TEMPLATED_NOOP_METHOD(vectorDupElementInt32);
    MACRO_ASSEMBLER_LOONGARCH64_TEMPLATED_NOOP_METHOD(vectorDupElementInt64);
    MACRO_ASSEMBLER_LOONGARCH64_TEMPLATED_NOOP_METHOD(vectorDupElementFloat32);
    MACRO_ASSEMBLER_LOONGARCH64_TEMPLATED_NOOP_METHOD(vectorDupElementFloat64);
    MACRO_ASSEMBLER_LOONGARCH64_TEMPLATED_NOOP_METHOD(vectorSplatInt8);
    MACRO_ASSEMBLER_LOONGARCH64_TEMPLATED_NOOP_METHOD(vectorSplatInt16);
    MACRO_ASSEMBLER_LOONGARCH64_TEMPLATED_NOOP_METHOD(vectorSplatInt32);
    MACRO_ASSEMBLER_LOONGARCH64_TEMPLATED_NOOP_METHOD(vectorSplatInt64);
    MACRO_ASSEMBLER_LOONGARCH64_TEMPLATED_NOOP_METHOD(vectorSplatFloat32);
    MACRO_ASSEMBLER_LOONGARCH64_TEMPLATED_NOOP_METHOD(vectorSplatFloat64);
    MACRO_ASSEMBLER_LOONGARCH64_TEMPLATED_NOOP_METHOD(compareFloatingPointVector);
    MACRO_ASSEMBLER_LOONGARCH64_TEMPLATED_NOOP_METHOD(compareIntegerVector);
    MACRO_ASSEMBLER_LOONGARCH64_TEMPLATED_NOOP_METHOD(compareIntegerVectorWithZero);
    MACRO_ASSEMBLER_LOONGARCH64_TEMPLATED_NOOP_METHOD(vectorAdd);
    MACRO_ASSEMBLER_LOONGARCH64_TEMPLATED_NOOP_METHOD(vectorSub);
    MACRO_ASSEMBLER_LOONGARCH64_TEMPLATED_NOOP_METHOD(vectorAddSat);
    MACRO_ASSEMBLER_LOONGARCH64_TEMPLATED_NOOP_METHOD(vectorSubSat);
    MACRO_ASSEMBLER_LOONGARCH64_TEMPLATED_NOOP_METHOD(vectorMul);
    MACRO_ASSEMBLER_LOONGARCH64_TEMPLATED_NOOP_METHOD(vectorDiv);
    MACRO_ASSEMBLER_LOONGARCH64_TEMPLATED_NOOP_METHOD(vectorMin);
    MACRO_ASSEMBLER_LOONGARCH64_TEMPLATED_NOOP_METHOD(vectorMax);
    MACRO_ASSEMBLER_LOONGARCH64_TEMPLATED_NOOP_METHOD(vectorPmin);
    MACRO_ASSEMBLER_LOONGARCH64_TEMPLATED_NOOP_METHOD(vectorPmax);
    MACRO_ASSEMBLER_LOONGARCH64_TEMPLATED_NOOP_METHOD(vectorNarrow);
    MACRO_ASSEMBLER_LOONGARCH64_TEMPLATED_NOOP_METHOD(vectorBitwiseSelect);
    MACRO_ASSEMBLER_LOONGARCH64_TEMPLATED_NOOP_METHOD(vectorNot);
    MACRO_ASSEMBLER_LOONGARCH64_TEMPLATED_NOOP_METHOD(vectorAnd);
    MACRO_ASSEMBLER_LOONGARCH64_TEMPLATED_NOOP_METHOD(vectorAndnot);
    MACRO_ASSEMBLER_LOONGARCH64_TEMPLATED_NOOP_METHOD(vectorOr);
    MACRO_ASSEMBLER_LOONGARCH64_TEMPLATED_NOOP_METHOD(vectorXor);
    MACRO_ASSEMBLER_LOONGARCH64_TEMPLATED_NOOP_METHOD(vectorAbs);
    MACRO_ASSEMBLER_LOONGARCH64_TEMPLATED_NOOP_METHOD(vectorNeg);
    MACRO_ASSEMBLER_LOONGARCH64_TEMPLATED_NOOP_METHOD(vectorPopcnt);
    MACRO_ASSEMBLER_LOONGARCH64_TEMPLATED_NOOP_METHOD(vectorCeil);
    MACRO_ASSEMBLER_LOONGARCH64_TEMPLATED_NOOP_METHOD(vectorFloor);
    MACRO_ASSEMBLER_LOONGARCH64_TEMPLATED_NOOP_METHOD(vectorTrunc);
    MACRO_ASSEMBLER_LOONGARCH64_TEMPLATED_NOOP_METHOD(vectorTruncSat);
    MACRO_ASSEMBLER_LOONGARCH64_TEMPLATED_NOOP_METHOD(vectorConvert);
    MACRO_ASSEMBLER_LOONGARCH64_TEMPLATED_NOOP_METHOD(vectorConvertLow);
    MACRO_ASSEMBLER_LOONGARCH64_TEMPLATED_NOOP_METHOD(vectorNearest);
    MACRO_ASSEMBLER_LOONGARCH64_TEMPLATED_NOOP_METHOD(vectorSqrt);
    MACRO_ASSEMBLER_LOONGARCH64_TEMPLATED_NOOP_METHOD(vectorExtendLow);
    MACRO_ASSEMBLER_LOONGARCH64_TEMPLATED_NOOP_METHOD(vectorExtendHigh);
    MACRO_ASSEMBLER_LOONGARCH64_TEMPLATED_NOOP_METHOD(vectorPromote);
    MACRO_ASSEMBLER_LOONGARCH64_TEMPLATED_NOOP_METHOD(vectorDemote);
    MACRO_ASSEMBLER_LOONGARCH64_TEMPLATED_NOOP_METHOD(vectorAnyTrue);
    MACRO_ASSEMBLER_LOONGARCH64_TEMPLATED_NOOP_METHOD(vectorAllTrue);
    MACRO_ASSEMBLER_LOONGARCH64_TEMPLATED_NOOP_METHOD(vectorBitmask);
    MACRO_ASSEMBLER_LOONGARCH64_TEMPLATED_NOOP_METHOD(vectorExtaddPairwise);
    MACRO_ASSEMBLER_LOONGARCH64_TEMPLATED_NOOP_METHOD(vectorAvgRound);
    MACRO_ASSEMBLER_LOONGARCH64_TEMPLATED_NOOP_METHOD(vectorMulSat);
    MACRO_ASSEMBLER_LOONGARCH64_TEMPLATED_NOOP_METHOD(vectorDotProduct);
    MACRO_ASSEMBLER_LOONGARCH64_TEMPLATED_NOOP_METHOD(vectorSwizzle);

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

    template<PtrTag startTag>
    static void replaceWithNops(CodeLocationLabel<startTag> instructionStart, size_t memoryToFillWithNopsInBytes)
    {
        Assembler::replaceWithNops(instructionStart.dataLocation(), memoryToFillWithNopsInBytes);
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
                m_assembler.bInsn(0);
            });
        return Jump(label);
    }

    void farJump(RegisterID target, PtrTag)
    {
        m_assembler.jirlInsn(LOONGARCH64Registers::zero, target, 0);
    }

    void farJump(AbsoluteAddress address, PtrTag)
    {
        auto temp = temps<Data>();
        loadImmediate(TrustedImmPtr(address.m_ptr), temp.data());
        m_assembler.ld_dInsn(temp.data(), temp.data(), Imm::I12<0>());
        m_assembler.jirlInsn(LOONGARCH64Registers::zero, temp.data(), 0);
    }

    void farJump(Address address, PtrTag)
    {
        auto temp = temps<Data, Memory>();
        auto resolution = resolveAddress(address, temp.memory());
        m_assembler.ld_dInsn(temp.data(), resolution.base, Imm::I12(resolution.offset));
        m_assembler.jirlInsn(LOONGARCH64Registers::zero, temp.data(), 0);
    }

    void farJump(BaseIndex address, PtrTag)
    {
        auto temp = temps<Data, Memory>();
        auto resolution = resolveAddress(address, temp.memory());
        m_assembler.ld_dInsn(temp.data(), resolution.base, Imm::I12(resolution.offset));
        m_assembler.jirlInsn(LOONGARCH64Registers::zero, temp.data(), 0);
    }

    void farJump(TrustedImmPtr imm, PtrTag)
    {
        auto temp = temps<Data>();
        loadImmediate(imm, temp.data());
        m_assembler.jirlInsn(LOONGARCH64Registers::zero, temp.data(), 0);
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
                m_assembler.blInsn(0);
            });
        return Call(label, Call::LinkableNear);
    }

    Call nearTailCall()
    {
        auto label = m_assembler.label();
        m_assembler.nearCallPlaceholder(
            [&] {
                m_assembler.bInsn(0);
            });
        return Call(label, Call::LinkableNearTail);
    }

    Call threadSafePatchableNearCall()
    {
        auto label = m_assembler.label();
        m_assembler.nearCallPlaceholder(
            [&] {
                m_assembler.blInsn(0);
            });
        return Call(label, Call::LinkableNear);
    }

    Call threadSafePatchableNearTailCall()
    {
        auto label = m_assembler.label();
        m_assembler.nearCallPlaceholder(
            [&] {
                m_assembler.bInsn(0);
            });
        return Call(label, Call::LinkableNearTail);
    }

    void ret()
    {
        m_assembler.jirlInsn(LOONGARCH64Registers::zero, LOONGARCH64Registers::ra, 0);
    }

    void compare8(RelationalCondition cond, Address address, TrustedImm32 imm, RegisterID dest)
    {
        auto temp = temps<Data, Memory>();
        auto resolution = resolveAddress(address, temp.memory());
        m_assembler.ld_bInsn(temp.memory(), resolution.base, Imm::I12(resolution.offset));
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
        m_assembler.ld_wInsn(temp.memory(), resolution.base, Imm::I12(resolution.offset));
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

    void compare64(RelationalCondition cond, RegisterID lhs, TrustedImm64 imm, RegisterID dest)
    {
        auto temp = temps<Data>();
        loadImmediate(imm, temp.data());
        compareFinalize(cond, lhs, temp.data(), dest);
    }

    void test8(ResultCondition cond, Address address, TrustedImm32 imm, RegisterID dest)
    {
        auto temp = temps<Data, Memory>();
        auto resolution = resolveAddress(address, temp.memory());
        m_assembler.ld_buInsn(temp.data(), resolution.base, Imm::I12(resolution.offset));
        m_assembler.andiInsn(temp.data(), temp.data(), Imm::I12((uint32_t)(imm.m_value & 0xff)));
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
        if (!Imm::I12Type::isUImm<12>(imm.m_value)) {
            loadImmediate(imm, temp.data());
            m_assembler.andInsn(temp.data(), lhs, temp.data());
        } else
            m_assembler.andiInsn(temp.data(), lhs, Imm::I12((uint32_t)imm.m_value));
        m_assembler.maskRegister<32>(temp.data());
        testFinalize(cond, temp.data(), dest);
    }

    void test32(ResultCondition cond, Address address, TrustedImm32 imm, RegisterID dest)
    {
        auto temp = temps<Data, Memory>();
        auto resolution = resolveAddress(address, temp.memory());
        m_assembler.ld_wuInsn(temp.memory(), resolution.base, Imm::I12(resolution.offset));

        if (!Imm::I12Type::isUImm<12>(imm.m_value)) {
            loadImmediate(imm, temp.data());
            m_assembler.andInsn(temp.data(), temp.memory(), temp.data());
        } else
            m_assembler.andiInsn(temp.data(), temp.memory(), Imm::I12((uint32_t)imm.m_value));
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
        if (!Imm::I12Type::isUImm<12>(imm.m_value)) {
            loadImmediate(imm, temp.data());
            m_assembler.andInsn(dest, lhs, temp.data());
        } else
            m_assembler.andiInsn(dest, lhs, Imm::I12((uint32_t)imm.m_value));
        testFinalize(cond, dest, dest);
    }

    void testAssembler()
    {
        m_assembler.lu32i_dInsn(LOONGARCH64Registers::r20, Imm::I20<int32_t>(524287),  0x16fffff4);
        m_assembler.lu32i_dInsn(LOONGARCH64Registers::r20, Imm::I20<int32_t>(-524288), 0x17000014);
        m_assembler.lu32i_dInsn(LOONGARCH64Registers::r20, Imm::I20<524287>(),  0x16fffff4);
        m_assembler.lu32i_dInsn(LOONGARCH64Registers::r20, Imm::I20<-524288>(), 0x17000014);
        m_assembler.lu32i_dInsn(LOONGARCH64Registers::r20, Imm::I20(524287),  0x16fffff4);
        m_assembler.lu32i_dInsn(LOONGARCH64Registers::r20, Imm::I20(-524288), 0x17000014);
        m_assembler.lu12i_wInsn(LOONGARCH64Registers::r20, Imm::I20<int32_t>(524287),  0x14fffff4);
        m_assembler.lu12i_wInsn(LOONGARCH64Registers::r20, Imm::I20<int32_t>(-524288), 0x15000014);
        m_assembler.lu12i_wInsn(LOONGARCH64Registers::r20, Imm::I20<524287>(),  0x14fffff4);
        m_assembler.lu12i_wInsn(LOONGARCH64Registers::r20, Imm::I20<-524288>(), 0x15000014);
        m_assembler.lu12i_wInsn(LOONGARCH64Registers::r20, Imm::I20(524287),  0x14fffff4);
        m_assembler.lu12i_wInsn(LOONGARCH64Registers::r20, Imm::I20(-524288), 0x15000014);
        m_assembler.pcaddu18iInsn(LOONGARCH64Registers::r20, 524287,  0x1e000034);
        m_assembler.pcaddu18iInsn(LOONGARCH64Registers::r20, -524288, 0x1fffffd4);
        m_assembler.andiInsn(LOONGARCH64Registers::r20, LOONGARCH64Registers::r20, Imm::I12((uint32_t)4095), 0x037ffe94);
        m_assembler.ld_dInsn(LOONGARCH64Registers::r20, LOONGARCH64Registers::r20, Imm::I12<int32_t>(2047),  0x28dffe94);
        m_assembler.ld_dInsn(LOONGARCH64Registers::r20, LOONGARCH64Registers::r20, Imm::I12<int32_t>(-2048), 0x28e00294);
        m_assembler.ld_dInsn(LOONGARCH64Registers::r20, LOONGARCH64Registers::r20, Imm::I12<2047>(),  0x28dffe94);
        m_assembler.ld_dInsn(LOONGARCH64Registers::r20, LOONGARCH64Registers::r20, Imm::I12<-2048>(), 0x28e00294);
        m_assembler.ld_dInsn(LOONGARCH64Registers::r20, LOONGARCH64Registers::r20, Imm::I12(2047),  0x28dffe94);
        m_assembler.ld_dInsn(LOONGARCH64Registers::r20, LOONGARCH64Registers::r20, Imm::I12(-2048), 0x28e00294);
        m_assembler.bcnezInsn(LOONGARCH64Registers::fcc0, 1048560,  0x4bfff103);
        m_assembler.bcnezInsn(LOONGARCH64Registers::fcc0, -1048560, 0x4800111c);
    }

    MACRO_ASSEMBLER_LOONGARCH64_TEMPLATED_NOOP_METHOD(setCarry);

    Jump branch8(RelationalCondition cond, Address address, TrustedImm32 imm)
    {
        auto temp = temps<Data, Memory>();
        auto resolution = resolveAddress(address, temp.memory());
        m_assembler.ld_bInsn(temp.memory(), resolution.base, Imm::I12(resolution.offset));
        loadImmediate(imm, temp.data());
        return makeBranch(cond, temp.memory(), temp.data());
    }

    Jump branch8(RelationalCondition cond, AbsoluteAddress address, TrustedImm32 imm)
    {
        auto temp = temps<Data, Memory>();
        loadImmediate(TrustedImmPtr(address.m_ptr), temp.memory());
        m_assembler.ld_bInsn(temp.memory(), temp.memory(), Imm::I12<0>());
        loadImmediate(imm, temp.data());
        return makeBranch(cond, temp.memory(), temp.data());
    }

    Jump branch16(RelationalCondition cond, Address address, TrustedImm32 imm)
    {
        auto temp = temps<Data, Memory>();
        auto resolution = resolveAddress(address, temp.memory());
        m_assembler.ld_hInsn(temp.memory(), resolution.base, Imm::I12(resolution.offset));
        loadImmediate(imm, temp.data());
        return makeBranch(cond, temp.memory(), temp.data());
    }

    Jump branch16(RelationalCondition cond, AbsoluteAddress address, TrustedImm32 imm)
    {
        auto temp = temps<Data, Memory>();
        loadImmediate(TrustedImmPtr(address.m_ptr), temp.memory());
        m_assembler.ld_hInsn(temp.memory(), temp.memory(), Imm::I12<0>());
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
        m_assembler.ld_wInsn(temp.memory(), resolution.base, Imm::I12(resolution.offset));
        m_assembler.signExtend<32>(temp.data(), lhs);
        return makeBranch(cond, temp.data(), temp.memory());
    }

    Jump branch32(RelationalCondition cond, Address address, RegisterID rhs)
    {
        auto temp = temps<Data, Memory>();
        auto resolution = resolveAddress(address, temp.memory());
        m_assembler.ld_wInsn(temp.memory(), resolution.base, Imm::I12(resolution.offset));
        m_assembler.signExtend<32>(temp.data(), rhs);
        return makeBranch(cond, temp.memory(), temp.data());
    }

    Jump branch32(RelationalCondition cond, Address address, TrustedImm32 imm)
    {
        auto temp = temps<Data, Memory>();
        auto resolution = resolveAddress(address, temp.memory());
        m_assembler.ld_wInsn(temp.memory(), resolution.base, Imm::I12(resolution.offset));
        loadImmediate(imm, temp.data());
        return makeBranch(cond, temp.memory(), temp.data());
    }

    Jump branch32(RelationalCondition cond, AbsoluteAddress address, RegisterID rhs)
    {
        auto temp = temps<Data, Memory>();
        loadImmediate(TrustedImmPtr(address.m_ptr), temp.memory());
        m_assembler.ld_wInsn(temp.memory(), temp.memory(), Imm::I12<0>());
        m_assembler.signExtend<32>(temp.data(), rhs);
        return makeBranch(cond, temp.memory(), temp.data());
    }

    Jump branch32(RelationalCondition cond, BaseIndex address, TrustedImm32 imm)
    {
        auto temp = temps<Data, Memory>();
        auto resolution = resolveAddress(address, temp.memory());
        m_assembler.ld_wInsn(temp.memory(), resolution.base, Imm::I12(resolution.offset));
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
        m_assembler.ld_dInsn(temp.data(), resolution.base, Imm::I12(resolution.offset));
        return makeBranch(cond, lhs, temp.data());
    }

    Jump branch64(RelationalCondition cond, RegisterID lhs, AbsoluteAddress address)
    {
        auto temp = temps<Data, Memory>();
        loadImmediate(TrustedImmPtr(address.m_ptr), temp.memory());
        m_assembler.ld_dInsn(temp.data(), temp.memory(), Imm::I12<0>());
        return makeBranch(cond, lhs, temp.data());
    }

    Jump branch64(RelationalCondition cond, Address address, RegisterID rhs)
    {
        auto temp = temps<Data, Memory>();
        auto resolution = resolveAddress(address, temp.memory());
        m_assembler.ld_dInsn(temp.data(), resolution.base, Imm::I12(resolution.offset));
        return makeBranch(cond, temp.data(), rhs);
    }

    Jump branch64(RelationalCondition cond, Address address, TrustedImm32 imm)
    {
        auto temp = temps<Data, Memory>();
        auto resolution = resolveAddress(address, temp.memory());
        m_assembler.ld_dInsn(temp.memory(), resolution.base, Imm::I12(resolution.offset));
        loadImmediate(imm, temp.data());
        return makeBranch(cond, temp.memory(), temp.data());
    }

    Jump branch64(RelationalCondition cond, Address address, TrustedImm64 imm)
    {
        auto temp = temps<Data, Memory>();
        auto resolution = resolveAddress(address, temp.memory());
        m_assembler.ld_dInsn(temp.memory(), resolution.base, Imm::I12(resolution.offset));
        loadImmediate(imm, temp.data());
        return makeBranch(cond, temp.memory(), temp.data());
    }

    Jump branch64(RelationalCondition cond, AbsoluteAddress address, RegisterID rhs)
    {
        auto temp = temps<Data, Memory>();
        loadImmediate(TrustedImmPtr(address.m_ptr), temp.memory());
        m_assembler.ld_dInsn(temp.data(), temp.memory(), Imm::I12<0>());
        return makeBranch(cond, temp.data(), rhs);
    }

    Jump branch64(RelationalCondition cond, AbsoluteAddress address, TrustedImm32 imm)
    {
        auto temp = temps<Data, Memory>();
        loadImmediate(TrustedImmPtr(address.m_ptr), temp.memory());
        m_assembler.ld_dInsn(temp.memory(), temp.memory(), Imm::I12<0>());
        loadImmediate(imm, temp.data());
        return makeBranch(cond, temp.memory(), temp.data());
    }

    Jump branch64(RelationalCondition cond, AbsoluteAddress address, TrustedImm64 imm)
    {
        auto temp = temps<Data, Memory>();
        loadImmediate(TrustedImmPtr(address.m_ptr), temp.memory());
        m_assembler.ld_dInsn(temp.memory(), temp.memory(), Imm::I12<0>());
        loadImmediate(imm, temp.data());
        return makeBranch(cond, temp.memory(), temp.data());
    }

    Jump branch64(RelationalCondition cond, BaseIndex address, RegisterID rhs)
    {
        auto temp = temps<Data, Memory>();
        auto resolution = resolveAddress(address, temp.memory());
        m_assembler.ld_dInsn(temp.data(), resolution.base, Imm::I12(resolution.offset));
        return makeBranch(cond, temp.data(), rhs);
    }

    Jump branch64(RelationalCondition cond, Address left, Address right)
    {
        auto temp = temps<Data, Memory>();
        auto leftResolution = resolveAddress(left, temp.memory());
        m_assembler.ld_dInsn(temp.data(), leftResolution.base, Imm::I12(leftResolution.offset));
        auto rightResolution = resolveAddress(right, temp.memory());
        m_assembler.ld_dInsn(temp.memory(), rightResolution.base, Imm::I12(rightResolution.offset));
        return makeBranch(cond, temp.data(), temp.memory());
    }

    Jump branch32WithUnalignedHalfWords(RelationalCondition cond, BaseIndex address, TrustedImm32 imm)
    {
        return branch32(cond, address, imm);
    }

    Jump branch32WithMemory16(RelationalCondition cond, Address left, RegisterID right)
    {
        auto temp = temps<Data, Memory>();
        MacroAssemblerHelpers::load16OnCondition(*this, cond, left, temp.data());
        m_assembler.signExtend<32>(temp.data(), temp.data());
        m_assembler.signExtend<32>(temp.memory(), right);
        return makeBranch(cond, temp.data(), temp.memory());
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
        m_assembler.add_wInsn(temp.data(), op1, op2);
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
        if (!Imm::I12Type::isSImm<12>(imm.m_value)) {
            loadImmediate(imm, temp.data());
            m_assembler.add_wInsn(temp.data(), op1, temp.data());
        } else
            m_assembler.addi_wInsn(temp.data(), op1, Imm::I12(imm.m_value));
        m_assembler.maskRegister<32>(dest, temp.data());
        return branchTestFinalize(cond, temp.data());
    }

    Jump branchAdd32(ResultCondition cond, Address address, RegisterID dest)
    {
        auto temp = temps<Data, Memory>();
        auto resolution = resolveAddress(address, temp.memory());
        m_assembler.ld_wInsn(temp.memory(), resolution.base, Imm::I12(resolution.offset));

        if (cond == Overflow)
            return branchForArithmeticOverflow<32, ArithmeticOperation::Addition>(dest, temp.memory(), dest);

        m_assembler.add_wInsn(temp.data(), dest, temp.memory());
        m_assembler.maskRegister<32>(dest, temp.data());
        return branchTestFinalize(cond, temp.data());
    }

    Jump branchAdd32(ResultCondition cond, TrustedImm32 imm, AbsoluteAddress address)
    {
        auto temp = temps<Data, Memory>();
        loadImmediate(TrustedImmPtr(address.m_ptr), temp.memory());
        m_assembler.ld_wInsn(temp.data(), temp.memory(), Imm::I12<0>());

        if (cond == Overflow) {
            auto branch = branchForArithmeticOverflow<32, ArithmeticOperation::Addition>(temp.data(), imm, temp.data());
            loadImmediate(TrustedImmPtr(address.m_ptr), temp.memory());
            m_assembler.st_wInsn(temp.data(), temp.memory(), Imm::I12<0>());
            return branch;
        }

        if (!Imm::I12Type::isSImm<12>(imm.m_value)) {
            loadImmediate(imm, temp.memory());
            m_assembler.add_wInsn(temp.data(), temp.data(), temp.memory());
        } else
            m_assembler.addi_wInsn(temp.data(), temp.data(), Imm::I12(imm.m_value));

        loadImmediate(TrustedImmPtr(address.m_ptr), temp.memory());
        m_assembler.st_wInsn(temp.data(), temp.memory(), Imm::I12<0>());
        return branchTestFinalize(cond, temp.data());
    }

    Jump branchAdd32(ResultCondition cond, TrustedImm32 imm, Address address)
    {
        auto temp = temps<Data, Memory>();
        auto resolution = resolveAddress(address, temp.memory());
        m_assembler.ld_wInsn(temp.data(), resolution.base, Imm::I12(resolution.offset));

        if (cond == Overflow) {
            auto branch = branchForArithmeticOverflow<32, ArithmeticOperation::Addition>(temp.data(), imm, temp.data());
            resolution = resolveAddress(address, temp.memory());
            m_assembler.st_wInsn(temp.data(), resolution.base, Imm::I12(resolution.offset));
            return branch;
        }

        if (!Imm::I12Type::isSImm<12>(imm.m_value)) {
            loadImmediate(imm, temp.memory());
            m_assembler.add_wInsn(temp.data(), temp.data(), temp.memory());
        } else
            m_assembler.addi_wInsn(temp.data(), temp.data(), Imm::I12(imm.m_value));

        resolution = resolveAddress(address, temp.memory());
        m_assembler.st_wInsn(temp.data(), resolution.base, Imm::I12(resolution.offset));
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

        m_assembler.add_dInsn(dest, op1, op2);
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

        if (!Imm::I12Type::isSImm<12>(imm.m_value)) {
            auto temp = temps<Data>();
            loadImmediate(imm, temp.data());
            m_assembler.add_dInsn(dest, op1, temp.data());
        } else
            m_assembler.addi_dInsn(dest, op1, Imm::I12(imm.m_value));
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
        m_assembler.sub_wInsn(temp.data(), op1, op2);
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

        m_assembler.sub_dInsn(dest, op1, op2);
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
        m_assembler.mul_dInsn(temp.data(), temp.memory(), temp.data());
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
        m_assembler.mul_dInsn(temp.data(), temp.memory(), temp.data());
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

        m_assembler.mul_dInsn(dest, op1, op2);
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
        m_assembler.mul_dInsn(dest, op1, temp.data());
        return branchTestFinalize(cond, dest);
    }

    Jump branchNeg32(ResultCondition cond, RegisterID reg)
    {
        return branchSub32(cond, LOONGARCH64Registers::zero, reg, reg);
    }

    Jump branchNeg64(ResultCondition cond, RegisterID reg)
    {
        return branchSub64(cond, LOONGARCH64Registers::zero, reg, reg);
    }

    Jump branchTest8(ResultCondition cond, Address address, TrustedImm32 imm = TrustedImm32(-1))
    {
        auto temp = temps<Data, Memory>();
        auto resolution = resolveAddress(address, temp.memory());
        m_assembler.ld_buInsn(temp.memory(), resolution.base, Imm::I12(resolution.offset));

        if (!Imm::I12Type::isUImm<12>(imm.m_value)) {
            loadImmediate(imm, temp.data());
            m_assembler.andInsn(temp.data(), temp.memory(), temp.data());
        } else
            m_assembler.andiInsn(temp.data(), temp.memory(), Imm::I12((uint32_t)imm.m_value));
        m_assembler.signExtend<8>(temp.data());
        return branchTestFinalize(cond, temp.data());
    }

    Jump branchTest8(ResultCondition cond, AbsoluteAddress address, TrustedImm32 imm = TrustedImm32(-1))
    {
        auto temp = temps<Data, Memory>();
        loadImmediate(TrustedImmPtr(address.m_ptr), temp.memory());
        m_assembler.ld_buInsn(temp.memory(), temp.memory(), Imm::I12<0>());

        if (!Imm::I12Type::isUImm<12>(imm.m_value)) {
            loadImmediate(imm, temp.data());
            m_assembler.andInsn(temp.data(), temp.memory(), temp.data());
        } else
            m_assembler.andiInsn(temp.data(), temp.memory(), Imm::I12((uint32_t)imm.m_value));
        m_assembler.signExtend<8>(temp.data());
        return branchTestFinalize(cond, temp.data());
    }

    Jump branchTest8(ResultCondition cond, BaseIndex address, TrustedImm32 imm = TrustedImm32(-1))
    {
        auto temp = temps<Data, Memory>();
        auto resolution = resolveAddress(address, temp.memory());
        m_assembler.ld_buInsn(temp.memory(), resolution.base, Imm::I12(resolution.offset));

        if (!Imm::I12Type::isUImm<12>(imm.m_value)) {
            loadImmediate(imm, temp.data());
            m_assembler.andInsn(temp.data(), temp.memory(), temp.data());
        } else
            m_assembler.andiInsn(temp.data(), temp.memory(), Imm::I12((uint32_t)imm.m_value));
        m_assembler.signExtend<8>(temp.data());
        return branchTestFinalize(cond, temp.data());
    }

    Jump branchTest8(ResultCondition cond, ExtendedAddress address, TrustedImm32 imm = TrustedImm32(-1))
    {
        auto temp = temps<Data, Memory>();
        auto resolution = resolveAddress(address, temp.memory());
        m_assembler.ld_buInsn(temp.memory(), resolution.base, Imm::I12(resolution.offset));

        if (!Imm::I12Type::isUImm<12>(imm.m_value)) {
            loadImmediate(imm, temp.data());
            m_assembler.andInsn(temp.data(), temp.memory(), temp.data());
        } else
            m_assembler.andiInsn(temp.data(), temp.memory(), Imm::I12((uint32_t)imm.m_value));
        m_assembler.signExtend<8>(temp.data());
        return branchTestFinalize(cond, temp.data());
    }

    Jump branchTest16(ResultCondition cond, Address address, TrustedImm32 imm = TrustedImm32(-1))
    {
        auto temp = temps<Data, Memory>();
        auto resolution = resolveAddress(address, temp.memory());
        m_assembler.ld_huInsn(temp.memory(), resolution.base, Imm::I12(resolution.offset));

        if (!Imm::I12Type::isUImm<12>(imm.m_value)) {
            loadImmediate(imm, temp.data());
            m_assembler.andInsn(temp.data(), temp.memory(), temp.data());
        } else
            m_assembler.andiInsn(temp.data(), temp.memory(), Imm::I12((uint32_t)imm.m_value));
        m_assembler.signExtend<16>(temp.data());
        return branchTestFinalize(cond, temp.data());
    }

    Jump branchTest16(ResultCondition cond, BaseIndex address, TrustedImm32 imm = TrustedImm32(-1))
    {
        auto temp = temps<Data, Memory>();
        auto resolution = resolveAddress(address, temp.memory());
        m_assembler.ld_huInsn(temp.memory(), resolution.base, Imm::I12(resolution.offset));

        if (!Imm::I12Type::isUImm<12>(imm.m_value)) {
            loadImmediate(imm, temp.data());
            m_assembler.andInsn(temp.data(), temp.memory(), temp.data());
        } else
            m_assembler.andiInsn(temp.data(), temp.memory(), Imm::I12((uint32_t)imm.m_value));
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

        if (!Imm::I12Type::isUImm<12>(imm.m_value)) {
            loadImmediate(imm, temp.data());
            m_assembler.andInsn(temp.data(), temp.memory(), temp.data());
        } else
            m_assembler.andiInsn(temp.data(), temp.memory(), Imm::I12((uint32_t)imm.m_value));
        m_assembler.signExtend<32>(temp.data());
        return branchTestFinalize(cond, temp.data());
    }

    Jump branchTest32(ResultCondition cond, Address address, TrustedImm32 imm = TrustedImm32(-1))
    {
        auto temp = temps<Data, Memory>();
        auto resolution = resolveAddress(address, temp.memory());
        m_assembler.ld_wuInsn(temp.memory(), resolution.base, Imm::I12(resolution.offset));

        if (!Imm::I12Type::isUImm<12>(imm.m_value)) {
            loadImmediate(imm, temp.data());
            m_assembler.andInsn(temp.data(), temp.memory(), temp.data());
        } else
            m_assembler.andiInsn(temp.data(), temp.memory(), Imm::I12((uint32_t)imm.m_value));
        m_assembler.signExtend<32>(temp.data());
        return branchTestFinalize(cond, temp.data());
    }

    Jump branchTest32(ResultCondition cond, AbsoluteAddress address, TrustedImm32 imm = TrustedImm32(-1))
    {
        auto temp = temps<Data, Memory>();
        loadImmediate(TrustedImmPtr(address.m_ptr), temp.memory());
        m_assembler.ld_wuInsn(temp.memory(), temp.memory(), Imm::I12<0>());

        if (!Imm::I12Type::isUImm<12>(imm.m_value)) {
            loadImmediate(imm, temp.data());
            m_assembler.andInsn(temp.data(), temp.memory(), temp.data());
        } else
            m_assembler.andiInsn(temp.data(), temp.memory(), Imm::I12((uint32_t)imm.m_value));
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
        if (!Imm::I12Type::isUImm<12>(imm.m_value)) {
            loadImmediate(imm, temp.data());
            m_assembler.andInsn(temp.data(), lhs, temp.data());
        } else
            m_assembler.andiInsn(temp.data(), lhs, Imm::I12((uint32_t)imm.m_value));
        return branchTestFinalize(cond, temp.data());
    }

    Jump branchTest64(ResultCondition cond, RegisterID lhs, TrustedImm64 imm)
    {
        auto temp = temps<Data>();
        if (!Imm::I12Type::isUImm<12>(imm.m_value)) {
            loadImmediate(imm, temp.data());
            m_assembler.andInsn(temp.data(), lhs, temp.data());
        } else
            m_assembler.andiInsn(temp.data(), lhs, Imm::I12((uint32_t)imm.m_value));
        return branchTestFinalize(cond, temp.data());
    }

    Jump branchTest64(ResultCondition cond, Address address, RegisterID rhs)
    {
        auto temp = temps<Data, Memory>();
        auto resolution = resolveAddress(address, temp.memory());
        m_assembler.ld_dInsn(temp.data(), resolution.base, Imm::I12(resolution.offset));
        m_assembler.andInsn(temp.data(), temp.data(), rhs);
        return branchTestFinalize(cond, temp.data());
    }

    Jump branchTest64(ResultCondition cond, Address address, TrustedImm32 imm = TrustedImm32(-1))
    {
        auto temp = temps<Data, Memory>();
        auto resolution = resolveAddress(address, temp.memory());
        m_assembler.ld_dInsn(temp.memory(), resolution.base, Imm::I12(resolution.offset));

        if (!Imm::I12Type::isUImm<12>(imm.m_value)) {
            loadImmediate(imm, temp.data());
            m_assembler.andInsn(temp.data(), temp.memory(), temp.data());
        } else
            m_assembler.andiInsn(temp.data(), temp.memory(), Imm::I12((uint32_t)imm.m_value));
        return branchTestFinalize(cond, temp.data());
    }

    Jump branchTest64(ResultCondition cond, AbsoluteAddress address, TrustedImm32 imm = TrustedImm32(-1))
    {
        auto temp = temps<Data, Memory>();
        loadImmediate(TrustedImmPtr(address.m_ptr), temp.memory());
        m_assembler.ld_dInsn(temp.memory(), temp.memory(), Imm::I12<0>());

        if (!Imm::I12Type::isUImm<12>(imm.m_value)) {
            loadImmediate(imm, temp.data());
            m_assembler.andInsn(temp.data(), temp.memory(), temp.data());
        } else
            m_assembler.andiInsn(temp.data(), temp.memory(), Imm::I12((uint32_t)imm.m_value));
        return branchTestFinalize(cond, temp.data());
    }

    Jump branchTest64(ResultCondition cond, BaseIndex address, TrustedImm32 imm = TrustedImm32(-1))
    {
        auto temp = temps<Data, Memory>();
        auto resolution = resolveAddress(address, temp.memory());
        m_assembler.ld_dInsn(temp.memory(), resolution.base, Imm::I12(resolution.offset));

        if (!Imm::I12Type::isUImm<12>(imm.m_value)) {
            loadImmediate(imm, temp.data());
            m_assembler.andInsn(temp.data(), temp.memory(), temp.data());
        } else
            m_assembler.andiInsn(temp.data(), temp.memory(), Imm::I12((uint32_t)imm.m_value));
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
        LOONGARCH64Assembler::ImmediateLoader imml(LOONGARCH64Assembler::ImmediateLoader::Placeholder, imm.m_value);

        DataLabel32 label(this);
        imml.moveInto(m_assembler, dest);
        return label;
    }

    DataLabelPtr moveWithPatch(TrustedImmPtr imm, RegisterID dest)
    {
        LOONGARCH64Assembler::ImmediateLoader imml(LOONGARCH64Assembler::ImmediateLoader::Placeholder, int64_t(imm.asIntptr()));

        DataLabelPtr label(this);
        imml.moveInto(m_assembler, dest);
        return label;
    }

    DataLabelPtr storePtrWithPatch(TrustedImmPtr initialValue, Address address)
    {
        auto temp = temps<Data, Memory>();
        LOONGARCH64Assembler::ImmediateLoader imml(LOONGARCH64Assembler::ImmediateLoader::Placeholder, int64_t(initialValue.asIntptr()));
        DataLabelPtr label(this);
        imml.moveInto(m_assembler, temp.data());

        auto resolution = resolveAddress(address, temp.memory());
        m_assembler.st_dInsn(temp.data(), resolution.base, Imm::I12(resolution.offset));
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
        m_assembler.ld_wInsn(temp.memory(), resolution.base, Imm::I12(resolution.offset));

        dataLabel = moveWithPatch(initialRightValue, temp.data());
        return makeBranch(cond, temp.memory(), temp.data());
    }

    Jump branchPtrWithPatch(RelationalCondition cond, Address address, DataLabelPtr& dataLabel, TrustedImmPtr initialRightValue = TrustedImmPtr(nullptr))
    {
        auto temp = temps<Data, Memory>();
        auto resolution = resolveAddress(address, temp.memory());
        m_assembler.ld_dInsn(temp.memory(), resolution.base, Imm::I12(resolution.offset));

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

    Jump branchFloatWithZero(DoubleCondition cond, FPRegisterID left)
    {
        UNUSED_PARAM(cond);
        UNUSED_PARAM(left);
        UNREACHABLE_FOR_PLATFORM();
        return { };
    }

    Jump branchDouble(DoubleCondition cond, FPRegisterID lhs, FPRegisterID rhs)
    {
        return branchFP<64>(cond, lhs, rhs);
    }

    Jump branchDoubleWithZero(DoubleCondition cond, FPRegisterID left)
    {
        UNUSED_PARAM(cond);
        UNUSED_PARAM(left);
        UNREACHABLE_FOR_PLATFORM();
        return { };
    }

    Jump branchDoubleNonZero(FPRegisterID reg, FPRegisterID)
    {
        RELEASE_ASSERT(reg != fpTempRegister);
        m_assembler.movgr2fr_wInsn(fpTempRegister, LOONGARCH64Registers::zero);
        m_assembler.ffint_wInsn<64>(fpTempRegister, fpTempRegister);
        return branchFP<64>(DoubleNotEqualAndOrdered, reg, fpTempRegister);
    }

    Jump branchDoubleZeroOrNaN(FPRegisterID reg, FPRegisterID)
    {
        RELEASE_ASSERT(reg != fpTempRegister);
        m_assembler.movgr2fr_wInsn(fpTempRegister, LOONGARCH64Registers::zero);
        m_assembler.ffint_wInsn<64>(fpTempRegister, fpTempRegister);
        return branchFP<64>(DoubleEqualOrUnordered, reg, fpTempRegister);
    }

    enum BranchTruncateType { BranchIfTruncateFailed, BranchIfTruncateSuccessful };
    Jump branchTruncateDoubleToInt32(FPRegisterID src, RegisterID dest, BranchTruncateType branchType = BranchIfTruncateFailed)
    {
        auto temp = temps<Data>();
        RELEASE_ASSERT(src != fpTempRegister2 && dest != temp.data());
        // Truncate to a 64-bit integer in temp register, copy the low 32-bit to dest.
        m_assembler.ftintrz_lInsn<64>(fpTempRegister2, src);
        m_assembler.movfr2grInsn<64>(dest, fpTempRegister2);
        // Check the low 32-bits sign extend to be equal to the full value.
        m_assembler.signExtend<32>(temp.data(), dest);
        m_assembler.xorInsn(temp.data(), dest, temp.data());
        if (branchType == BranchIfTruncateFailed)
            m_assembler.sltuInsn(temp.data(), LOONGARCH64Registers::zero, temp.data());
        else
            m_assembler.sltuiInsn(temp.data(), temp.data(), Imm::I12<1>());

        m_assembler.maskRegister<32>(dest);
        return makeBranch(NotEqual, temp.data(), LOONGARCH64Registers::zero);
    }

    void branchConvertDoubleToInt32(FPRegisterID src, RegisterID dest, JumpList& failureCases, FPRegisterID, bool negZeroCheck = true)
    {
        auto temp = temps<Data>();
        RELEASE_ASSERT(src != fpTempRegister && src != fpTempRegister2 && dest != temp.data());
        m_assembler.ftintrz_wInsn<64>(fpTempRegister2, src);
        m_assembler.movfr2grInsn<32>(temp.data(), fpTempRegister2);
        m_assembler.movgr2fr_wInsn(fpTempRegister, temp.data());
        m_assembler.ffint_wInsn<64>(fpTempRegister, fpTempRegister);
        m_assembler.maskRegister<32>(dest, temp.data());
        failureCases.append(branchFP<64>(DoubleNotEqualOrUnordered, src, fpTempRegister));

        if (negZeroCheck) {
            Jump resultIsNonZero = makeBranch(NotEqual, temp.data(), LOONGARCH64Registers::zero);
            m_assembler.movfr2grInsn<64>(temp.data(), src);
            failureCases.append(makeBranch(LessThan, temp.data(), LOONGARCH64Registers::zero));
            resultIsNonZero.link(this);
        }
    }

    Call call(PtrTag)
    {
        auto label = m_assembler.label();
        m_assembler.pointerCallPlaceholder(
            [&] {
                auto temp = temps<Data>();
                m_assembler.addi_dInsn(temp.data(), LOONGARCH64Registers::zero, Imm::I12<0>());
                m_assembler.jirlInsn(LOONGARCH64Registers::ra, temp.data(), 0);
            });
        return Call(label, Call::Linkable);
    }

    Call call(RegisterID target, PtrTag)
    {
        m_assembler.jirlInsn(LOONGARCH64Registers::ra, target, 0);
        return Call(m_assembler.label(), Call::None);
    }

    Call call(Address address, PtrTag)
    {
        auto temp = temps<Data, Memory>();
        auto resolution = resolveAddress(address, temp.memory());
        m_assembler.ld_dInsn(temp.data(), resolution.base, Imm::I12(resolution.offset));
        m_assembler.jirlInsn(LOONGARCH64Registers::ra, temp.data(), 0);
        return Call(m_assembler.label(), Call::None);
    }

    Call call(RegisterID callTag) { UNUSED_PARAM(callTag); return call(NoPtrTag); }
    Call call(RegisterID target, RegisterID callTag) { UNUSED_PARAM(callTag); return call(target, NoPtrTag); }
    Call call(Address address, RegisterID callTag) { UNUSED_PARAM(callTag); return call(address, NoPtrTag); }

    template<PtrTag tag>
    void callOperation(const CodePtr<tag> operation)
    {
        auto temp = temps<Data>();
        loadImmediate(TrustedImmPtr(operation.taggedPtr()), temp.data());
        m_assembler.jirlInsn(LOONGARCH64Registers::ra, temp.data(), 0);
    }

    void getEffectiveAddress(BaseIndex address, RegisterID dest)
    {
        auto resolution = resolveAddress(address, lazyTemp<Memory>());
        m_assembler.addi_dInsn(dest, resolution.base, Imm::I12(resolution.offset));
    }

    void loadFloat(Address address, FPRegisterID dest)
    {
        auto resolution = resolveAddress(address, lazyTemp<Memory>());
        m_assembler.fld_sInsn(dest, resolution.base, Imm::I12(resolution.offset));
    }

    void loadFloat(BaseIndex address, FPRegisterID dest)
    {
        auto resolution = resolveAddress(address, lazyTemp<Memory>());
        m_assembler.fld_sInsn(dest, resolution.base, Imm::I12(resolution.offset));
    }

    void loadFloat(TrustedImmPtr address, FPRegisterID dest)
    {
        auto temp = temps<Memory>();
        loadImmediate(address, temp.memory());
        m_assembler.fld_sInsn(dest, temp.memory(), Imm::I12<0>());
    }

    void loadDouble(Address address, FPRegisterID dest)
    {
        auto resolution = resolveAddress(address, lazyTemp<Memory>());
        m_assembler.fld_dInsn(dest, resolution.base, Imm::I12(resolution.offset));
    }

    void loadDouble(BaseIndex address, FPRegisterID dest)
    {
        auto resolution = resolveAddress(address, lazyTemp<Memory>());
        m_assembler.fld_dInsn(dest, resolution.base, Imm::I12(resolution.offset));
    }

    void loadDouble(TrustedImmPtr address, FPRegisterID dest)
    {
        auto temp = temps<Memory>();
        loadImmediate(address, temp.memory());
        m_assembler.fld_dInsn(dest, temp.memory(), Imm::I12<0>());
    }

    void storeFloat(FPRegisterID src, Address address)
    {
        auto resolution = resolveAddress(address, lazyTemp<Memory>());
        m_assembler.fst_sInsn(src, resolution.base, Imm::I12(resolution.offset));
    }

    void storeFloat(FPRegisterID src, BaseIndex address)
    {
        auto resolution = resolveAddress(address, lazyTemp<Memory>());
        m_assembler.fst_sInsn(src, resolution.base, Imm::I12(resolution.offset));
    }

    void storeFloat(FPRegisterID src, TrustedImmPtr address)
    {
        auto temp = temps<Memory>();
        loadImmediate(address, temp.memory());
        m_assembler.fst_sInsn(src, temp.memory(), Imm::I12<0>());
    }

    void storeDouble(FPRegisterID src, Address address)
    {
        auto resolution = resolveAddress(address, lazyTemp<Memory>());
        m_assembler.fst_dInsn(src, resolution.base, Imm::I12(resolution.offset));
    }

    void storeDouble(FPRegisterID src, BaseIndex address)
    {
        auto resolution = resolveAddress(address, lazyTemp<Memory>());
        m_assembler.fst_dInsn(src, resolution.base, Imm::I12(resolution.offset));
    }

    void storeDouble(FPRegisterID src, TrustedImmPtr address)
    {
        auto temp = temps<Memory>();
        loadImmediate(address, temp.memory());
        m_assembler.fst_dInsn(src, temp.memory(), Imm::I12<0>());
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
        RELEASE_ASSERT(dest != fpTempRegister);
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
        RELEASE_ASSERT(dest != fpTempRegister);
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
        m_assembler.fabsInsn<32>(dest, src);
    }

    void absDouble(FPRegisterID src, FPRegisterID dest)
    {
        m_assembler.fabsInsn<64>(dest, src);
    }

    void ceilFloat(FPRegisterID src, FPRegisterID dest)
    {
        if (supportsLSX())
            m_assembler.vfrintrpInsn<32>(dest, src);
        else
            roundFP<32, LOONGARCH64Assembler::FPRoundingMode::RP>(src, dest);
    }

    void ceilDouble(FPRegisterID src, FPRegisterID dest)
    {
        if (supportsLSX())
            m_assembler.vfrintrpInsn<64>(dest, src);
        else
            roundFP<64, LOONGARCH64Assembler::FPRoundingMode::RP>(src, dest);
    }

    void floorFloat(FPRegisterID src, FPRegisterID dest)
    {
        if (supportsLSX())
            m_assembler.vfrintrmInsn<32>(dest, src);
        else
            roundFP<32, LOONGARCH64Assembler::FPRoundingMode::RM>(src, dest);
    }

    void floorDouble(FPRegisterID src, FPRegisterID dest)
    {
        if (supportsLSX())
            m_assembler.vfrintrmInsn<64>(dest, src);
        else
            roundFP<64, LOONGARCH64Assembler::FPRoundingMode::RM>(src, dest);
    }

    void roundTowardNearestIntFloat(FPRegisterID src, FPRegisterID dest)
    {
        if (supportsLSX())
            m_assembler.vfrintrneInsn<32>(dest, src);
        else
            roundFP<32, LOONGARCH64Assembler::FPRoundingMode::RNE>(src, dest);
    }

    void roundTowardNearestIntDouble(FPRegisterID src, FPRegisterID dest)
    {
        if (supportsLSX())
            m_assembler.vfrintrneInsn<64>(dest, src);
        else
            roundFP<64, LOONGARCH64Assembler::FPRoundingMode::RNE>(src, dest);
    }

    void roundTowardZeroFloat(FPRegisterID src, FPRegisterID dest)
    {
        if (supportsLSX())
            m_assembler.vfrintrzInsn<32>(dest, src);
        else
            roundFP<32, LOONGARCH64Assembler::FPRoundingMode::RZ>(src, dest);
    }

    void roundTowardZeroDouble(FPRegisterID src, FPRegisterID dest)
    {
        if (supportsLSX())
            m_assembler.vfrintrzInsn<64>(dest, src);
        else
            roundFP<64, LOONGARCH64Assembler::FPRoundingMode::RZ>(src, dest);
    }

    void andFloat(FPRegisterID op1, FPRegisterID op2, FPRegisterID dest)
    {
        if (supportsLSX())
            m_assembler.vand_vInsn(dest, op1, op2);
        else {
            auto temp = temps<Data, Memory>();
            m_assembler.movfr2grInsn<32>(temp.data(), op1);
            m_assembler.movfr2grInsn<32>(temp.memory(), op2);
            m_assembler.andInsn(temp.data(), temp.data(), temp.memory());
            m_assembler.movgr2fr_wInsn(dest, temp.data());
        }
    }

    void andDouble(FPRegisterID op1, FPRegisterID op2, FPRegisterID dest)
    {
        if (supportsLSX())
            m_assembler.vand_vInsn(dest, op1, op2);
        else {
            auto temp = temps<Data, Memory>();
            m_assembler.movfr2grInsn<64>(temp.data(), op1);
            m_assembler.movfr2grInsn<64>(temp.memory(), op2);
            m_assembler.andInsn(temp.data(), temp.data(), temp.memory());
            m_assembler.movgr2fr_dInsn(dest, temp.data());
        }
    }

    void orFloat(FPRegisterID op1, FPRegisterID op2, FPRegisterID dest)
    {
        if (supportsLSX())
            m_assembler.vor_vInsn(dest, op1, op2);
        else {
            auto temp = temps<Data, Memory>();
            m_assembler.movfr2grInsn<32>(temp.data(), op1);
            m_assembler.movfr2grInsn<32>(temp.memory(), op2);
            m_assembler.orInsn(temp.data(), temp.data(), temp.memory());
            m_assembler.movgr2fr_wInsn(dest, temp.data());
        }
    }

    void orDouble(FPRegisterID op1, FPRegisterID op2, FPRegisterID dest)
    {
        if (supportsLSX())
            m_assembler.vor_vInsn(dest, op1, op2);
        else {
            auto temp = temps<Data, Memory>();
            m_assembler.movfr2grInsn<64>(temp.data(), op1);
            m_assembler.movfr2grInsn<64>(temp.memory(), op2);
            m_assembler.orInsn(temp.data(), temp.data(), temp.memory());
            m_assembler.movgr2fr_dInsn(dest, temp.data());
        }
    }

    void negateFloat(FPRegisterID src, FPRegisterID dest)
    {
        m_assembler.fnegInsn<32>(dest, src);
    }

    void negateDouble(FPRegisterID src, FPRegisterID dest)
    {
        m_assembler.fnegInsn<64>(dest, src);
    }

    void compareFloat(DoubleCondition cond, FPRegisterID lhs, FPRegisterID rhs, RegisterID dest)
    {
        compareFP<32>(cond, lhs, rhs, dest);
    }

    void compareDouble(DoubleCondition cond, FPRegisterID lhs, FPRegisterID rhs, RegisterID dest)
    {
        compareFP<64>(cond, lhs, rhs, dest);
    }

    void compareDoubleWithZero(DoubleCondition cond, FPRegisterID left, RegisterID dest)
    {
        UNUSED_PARAM(cond);
        UNUSED_PARAM(left);
        UNUSED_PARAM(dest);
        UNREACHABLE_FOR_PLATFORM();
    }

    void compareFloatWithZero(DoubleCondition cond, FPRegisterID left, RegisterID dest)
    {
        UNUSED_PARAM(cond);
        UNUSED_PARAM(left);
        UNUSED_PARAM(dest);
        UNREACHABLE_FOR_PLATFORM();
    }

    void convertInt32ToFloat(RegisterID src, FPRegisterID dest)
    {
        m_assembler.movgr2fr_wInsn(dest, src);
        m_assembler.ffint_wInsn<32>(dest, dest);
    }

    void convertInt32ToDouble(RegisterID src, FPRegisterID dest)
    {
        m_assembler.movgr2fr_wInsn(dest, src);
        m_assembler.ffint_wInsn<64>(dest, dest);
    }

    void convertInt32ToDouble(TrustedImm32 imm, FPRegisterID dest)
    {
        auto temp = temps<Data>();
        loadImmediate(imm, temp.data());
        convertInt32ToDouble(temp.data(), dest);
    }

    void convertInt64ToFloat(RegisterID src, FPRegisterID dest)
    {
        m_assembler.movgr2fr_dInsn(dest, src);
        m_assembler.ffint_lInsn<32>(dest, dest);
    }

    void convertInt64ToDouble(RegisterID src, FPRegisterID dest)
    {
        m_assembler.movgr2fr_dInsn(dest, src);
        m_assembler.ffint_lInsn<64>(dest, dest);
    }

    void convertUInt64ToFloat(RegisterID src, FPRegisterID dest)
    {
        auto temp = temps<Data, Data2>();
        RELEASE_ASSERT(src != temp.data() && src != temp.data2());
        m_assembler.bltInsn(src, LOONGARCH64Registers::zero, 16);
        m_assembler.movgr2fr_dInsn(dest, src);
        m_assembler.ffint_lInsn<32>(dest, dest);
        m_assembler.bInsn(36);
        m_assembler.orInsn(temp.data(), src, LOONGARCH64Registers::zero);
        m_assembler.andiInsn(temp.data(), temp.data(), Imm::I12<1>());
        m_assembler.orInsn(temp.data2(), src, LOONGARCH64Registers::zero);
        m_assembler.srli_dInsn(temp.data2(), temp.data2(), 1);
        m_assembler.orInsn(temp.data(), temp.data2(), temp.data());
        m_assembler.movgr2fr_dInsn(dest, temp.data());
        m_assembler.ffint_lInsn<32>(dest, dest);
        m_assembler.faddInsn<32>(dest, dest, dest);
    }

    void convertUInt64ToDouble(RegisterID src, FPRegisterID dest)
    {
        auto temp = temps<Data, Data2>();
        RELEASE_ASSERT(src != temp.data() && src != temp.data2());
        m_assembler.bltInsn(src, LOONGARCH64Registers::zero, 16);
        m_assembler.movgr2fr_dInsn(dest, src);
        m_assembler.ffint_lInsn<64>(dest, dest);
        m_assembler.bInsn(36);
        m_assembler.orInsn(temp.data(), src, LOONGARCH64Registers::zero);
        m_assembler.andiInsn(temp.data(), temp.data(), Imm::I12<1>());
        m_assembler.orInsn(temp.data2(), src, LOONGARCH64Registers::zero);
        m_assembler.srli_dInsn(temp.data2(), temp.data2(), 1);
        m_assembler.orInsn(temp.data(), temp.data2(), temp.data());
        m_assembler.movgr2fr_dInsn(dest, temp.data());
        m_assembler.ffint_lInsn<64>(dest, dest);
        m_assembler.faddInsn<64>(dest, dest, dest);
    }

    void convertFloatToDouble(FPRegisterID src, FPRegisterID dest)
    {
        m_assembler.fcvt_d_sInsn(dest, src);
    }

    void convertDoubleToFloat(FPRegisterID src, FPRegisterID dest)
    {
        m_assembler.fcvt_s_dInsn(dest, src);
    }

    void truncateFloatToInt32(FPRegisterID src, RegisterID dest)
    {
        RELEASE_ASSERT(src != fpTempRegister);
        m_assembler.ftintrz_wInsn<32>(fpTempRegister, src);
        m_assembler.movfr2grInsn<32>(dest, fpTempRegister);
        m_assembler.maskRegister<32>(dest);
    }

    void truncateFloatToUint32(FPRegisterID src, RegisterID dest)
    {
        auto temp = temps<Data>();
        RELEASE_ASSERT(src != fpTempRegister && src != fpTempRegister2 && dest != temp.data());
        m_assembler.addi_dInsn(LOONGARCH64Registers::sp, LOONGARCH64Registers::sp, Imm::I12<-4>());
        m_assembler.lu12i_wInsn(temp.data(), Imm::I20<323584>());
        m_assembler.lu32i_dInsn(temp.data(), Imm::I20<203521>());
        m_assembler.lu52i_dInsn(temp.data(), temp.data(), Imm::I12<944>());
        m_assembler.st_dInsn(temp.data(), LOONGARCH64Registers::sp, Imm::I12<0>());
        // fpTempRegister2 = 0x3b031b014f000000 = 2.14748365e+09
        m_assembler.fld_sInsn(fpTempRegister2, LOONGARCH64Registers::sp, Imm::I12<0>());
        m_assembler.fcmp_sleInsn<32>(fcc0, fpTempRegister2, src);
        m_assembler.bcnezInsn(fcc0, 16 /* DON'T forget to modify 16 if changed the context below */);
        {
            m_assembler.ftintrz_wInsn<32>(fpTempRegister, src);
            m_assembler.movfr2grInsn<32>(dest, fpTempRegister);
            m_assembler.bInsn(28 /* DON'T forget to modify 28 if changed the context below */);
        }
        m_assembler.fmovInsn<32>(fpTempRegister, src); // keep src
        m_assembler.fsubInsn<32>(fpTempRegister, fpTempRegister, fpTempRegister2);
        m_assembler.ftintrz_wInsn<32>(fpTempRegister, fpTempRegister);
        m_assembler.movfr2grInsn<32>(dest, fpTempRegister);
        m_assembler.lu12i_wInsn(temp.data(), Imm::I20<-524288>()); // temp.data() = 0xffffffff80000000
        m_assembler.orInsn(dest, dest, temp.data());
        m_assembler.addi_dInsn(LOONGARCH64Registers::sp, LOONGARCH64Registers::sp, Imm::I12<4>());
    }

    void truncateFloatToInt64(FPRegisterID src, RegisterID dest)
    {
        m_assembler.ftintrz_lInsn<32>(fpTempRegister, src);
        m_assembler.movfr2grInsn<64>(dest, fpTempRegister);
    }

    void truncateFloatToUint64(FPRegisterID src, RegisterID dest)
    {
        auto temp = temps<Data>();
        RELEASE_ASSERT(src != fpTempRegister && src != fpTempRegister2 && dest != temp.data());
        m_assembler.addi_dInsn(LOONGARCH64Registers::sp, LOONGARCH64Registers::sp, Imm::I12<-4>());
        m_assembler.lu12i_wInsn(temp.data(), Imm::I20<389120>());
        m_assembler.lu32i_dInsn(temp.data(), Imm::I20<203521>());
        m_assembler.lu52i_dInsn(temp.data(), temp.data(), Imm::I12<944>());
        m_assembler.st_dInsn(temp.data(), LOONGARCH64Registers::sp, Imm::I12<0>());
        // fpTempRegister2 = 0x3b031b015f000000 = 9.22337204e+18
        m_assembler.fld_sInsn(fpTempRegister2, LOONGARCH64Registers::sp, Imm::I12<0>());
        m_assembler.fcmp_sleInsn<32>(fcc0, fpTempRegister2, src);
        m_assembler.bcnezInsn(fcc0, 16 /* DON'T forget to modify 16 if changed the context below */);
        {
            m_assembler.ftintrz_lInsn<32>(fpTempRegister, src);
            m_assembler.movfr2grInsn<64>(dest, fpTempRegister);
            m_assembler.bInsn(36 /* DON'T forget to modify 36 if changed the context below */);
        }
        m_assembler.fmovInsn<32>(fpTempRegister, src); // keep src
        m_assembler.fsubInsn<32>(fpTempRegister, fpTempRegister, fpTempRegister2);
        m_assembler.ftintrz_lInsn<32>(fpTempRegister, fpTempRegister);
        m_assembler.movfr2grInsn<64>(dest, fpTempRegister);
        m_assembler.lu12i_wInsn(temp.data(), Imm::I20<-524288>());
        m_assembler.lu32i_dInsn(temp.data(), Imm::I20<0>());
        m_assembler.slli_dInsn(temp.data(), temp.data(), 32); // temp.data() = 0x8000000000000000
        m_assembler.orInsn(dest, dest, temp.data());
        m_assembler.addi_dInsn(LOONGARCH64Registers::sp, LOONGARCH64Registers::sp, Imm::I12<4>());
    }

    void truncateFloatToUint64(FPRegisterID src, RegisterID dest, FPRegisterID, FPRegisterID)
    {
        truncateFloatToUint64(src, dest);
    }

    void truncateDoubleToInt32(FPRegisterID src, RegisterID dest)
    {
        m_assembler.ftintrz_wInsn<64>(fpTempRegister, src);
        m_assembler.movfr2grInsn<32>(dest, fpTempRegister);
        m_assembler.maskRegister<32>(dest);
    }

    void truncateDoubleToUint32(FPRegisterID src, RegisterID dest)
    {
        auto temp = temps<Data>();
        RELEASE_ASSERT(src != fpTempRegister && src != fpTempRegister2 && dest != temp.data());
        m_assembler.addi_dInsn(LOONGARCH64Registers::sp, LOONGARCH64Registers::sp, Imm::I12<-4>());
        m_assembler.lu52i_dInsn(temp.data(), LOONGARCH64Registers::zero, Imm::I12<1054>());
        m_assembler.st_dInsn(temp.data(), LOONGARCH64Registers::sp, Imm::I12<0>());
        // fpTempRegister2 = 0x41e0000000000000 = 2147483648.0
        m_assembler.fld_dInsn(fpTempRegister2, LOONGARCH64Registers::sp, Imm::I12<0>());
        m_assembler.fcmp_sleInsn<64>(fcc0, fpTempRegister2, src);
        m_assembler.bcnezInsn(fcc0, 16 /* DON'T forget to modify 16 if changed the context below */);
        {
            m_assembler.ftintrz_wInsn<64>(fpTempRegister, src);
            m_assembler.movfr2grInsn<32>(dest, fpTempRegister);
            m_assembler.bInsn(28 /* DON'T forget to modify 28 if changed the context below */);
        }
        m_assembler.fmovInsn<64>(fpTempRegister, src); // keep src
        m_assembler.fsubInsn<64>(fpTempRegister, fpTempRegister, fpTempRegister2);
        m_assembler.ftintrz_wInsn<64>(fpTempRegister, fpTempRegister);
        m_assembler.movfr2grInsn<32>(dest, fpTempRegister);
        m_assembler.lu12i_wInsn(temp.data(), Imm::I20<-524288>()); // temp.data() = 0xffffffff80000000
        m_assembler.orInsn(dest, dest, temp.data());
        m_assembler.addi_dInsn(LOONGARCH64Registers::sp, LOONGARCH64Registers::sp, Imm::I12<4>());
    }

    void truncateDoubleToInt64(FPRegisterID src, RegisterID dest)
    {
        m_assembler.ftintrz_lInsn<64>(fpTempRegister, src);
        m_assembler.movfr2grInsn<64>(dest, fpTempRegister);
    }

    void truncateDoubleToUint64(FPRegisterID src, RegisterID dest)
    {
        auto temp = temps<Data>();
        RELEASE_ASSERT(src != fpTempRegister && src != fpTempRegister2 && dest != temp.data());
        m_assembler.addi_dInsn(LOONGARCH64Registers::sp, LOONGARCH64Registers::sp, Imm::I12<-4>());
        m_assembler.lu52i_dInsn(temp.data(), LOONGARCH64Registers::zero, Imm::I12<1086>());
        m_assembler.st_dInsn(temp.data(), LOONGARCH64Registers::sp, Imm::I12<0>());
        // fpTempRegister2 = 0x43e0000000000000 = 9.2233720368547758e+18
        m_assembler.fld_dInsn(fpTempRegister2, LOONGARCH64Registers::sp, Imm::I12<0>());
        m_assembler.fcmp_sleInsn<64>(fcc0, fpTempRegister2, src);
        m_assembler.bcnezInsn(fcc0, 16 /* DON'T forget to modify 16 if changed the context below */);
        {
            m_assembler.ftintrz_lInsn<64>(fpTempRegister, src);
            m_assembler.movfr2grInsn<64>(dest, fpTempRegister);
            m_assembler.bInsn(36 /* DON'T forget to modify 36 if changed the context below */);
        }
        m_assembler.fmovInsn<64>(fpTempRegister, src); // keep src
        m_assembler.fsubInsn<64>(fpTempRegister, fpTempRegister, fpTempRegister2);
        m_assembler.ftintrz_lInsn<64>(fpTempRegister, fpTempRegister);
        m_assembler.movfr2grInsn<64>(dest, fpTempRegister);
        m_assembler.lu12i_wInsn(temp.data(), Imm::I20<-524288>());
        m_assembler.lu32i_dInsn(temp.data(), Imm::I20<0>());
        m_assembler.slli_dInsn(temp.data(), temp.data(), 32); // temp.data() = 0x8000000000000000
        m_assembler.orInsn(dest, dest, temp.data());
        m_assembler.addi_dInsn(LOONGARCH64Registers::sp, LOONGARCH64Registers::sp, Imm::I12<4>());
    }

    void truncateDoubleToUint64(FPRegisterID src, RegisterID dest, FPRegisterID, FPRegisterID)
    {
        truncateDoubleToUint64(src, dest);
    }

    void push(RegisterID src)
    {
        m_assembler.addi_dInsn(LOONGARCH64Registers::sp, LOONGARCH64Registers::sp, Imm::I12<-8>());
        m_assembler.st_dInsn(src, LOONGARCH64Registers::sp, Imm::I12<0>());
    }

    void push(TrustedImm32 imm)
    {
        auto temp = temps<Data>();
        loadImmediate(imm, temp.data());
        m_assembler.addi_dInsn(LOONGARCH64Registers::sp, LOONGARCH64Registers::sp, Imm::I12<-8>());
        m_assembler.st_dInsn(temp.data(), LOONGARCH64Registers::sp, Imm::I12<0>());
    }

    void pushPair(RegisterID src1, RegisterID src2)
    {
        m_assembler.addi_dInsn(LOONGARCH64Registers::sp, LOONGARCH64Registers::sp, Imm::I12<-16>());
        m_assembler.st_dInsn(src1, LOONGARCH64Registers::sp, Imm::I12<0>());
        m_assembler.st_dInsn(src2, LOONGARCH64Registers::sp, Imm::I12<8>());
    }

    void pop(RegisterID dest)
    {
        m_assembler.ld_dInsn(dest, LOONGARCH64Registers::sp, Imm::I12<0>());
        m_assembler.addi_dInsn(LOONGARCH64Registers::sp, LOONGARCH64Registers::sp, Imm::I12<8>());
    }

    void popPair(RegisterID dest1, RegisterID dest2)
    {
        m_assembler.ld_dInsn(dest1, LOONGARCH64Registers::sp, Imm::I12<0>());
        m_assembler.ld_dInsn(dest2, LOONGARCH64Registers::sp, Imm::I12<8>());
        m_assembler.addi_dInsn(LOONGARCH64Registers::sp, LOONGARCH64Registers::sp, Imm::I12<16>());
    }

    void abortWithReason(AbortReason reason)
    {
        auto temp = temps<Data>();
        loadImmediate(TrustedImm32(reason), temp.data());
        m_assembler.breakInsn(0x5);
    }

    void abortWithReason(AbortReason reason, intptr_t misc)
    {
        auto temp = temps<Data, Memory>();
        loadImmediate(TrustedImm32(reason), temp.data());
        loadImmediate(TrustedImm64(misc), temp.memory());
        m_assembler.breakInsn(0x5);
    }

    // Helpful break 0x5 for gdb.
    void breakpoint(uint16_t imm = 0x5)
    {
        m_assembler.breakInsn(imm);
    }

    void nop()
    {
        m_assembler.andiInsn(LOONGARCH64Registers::zero, LOONGARCH64Registers::zero, Imm::I12<0>());
    }

    void memoryFence()
    {
        m_assembler.fenceInsn();
    }

    void storeFence()
    {
        m_assembler.fenceInsn();
    }

    void loadFence()
    {
        m_assembler.fenceInsn();
    }

    template<unsigned bitSize>
    JumpList branchAtomicWeakCASImpl(StatusCondition cond, RegisterID expectedAndClobbered, RegisterID newValue, BaseIndex address)
    {
        static_assert(bitSize == 8 || bitSize == 16);
        // There's no 8-bit or 16-bit load-reserved and store-conditional instructions in LOONGARCH,
        // so we have to implement the operations through the 32-bit versions, with a limited amount
        // of usable registers.

        auto temp = temps<Data, Data2, Memory>();
        JumpList failure;

        RELEASE_ASSERT(expectedAndClobbered != temp.data() && expectedAndClobbered != temp.data2() && newValue != temp.data());

        // We clobber the expected-value register with the XOR difference between the expected
        // and the new value, also clipping the result to the desired number of bits.
        m_assembler.xorInsn(expectedAndClobbered, expectedAndClobbered, newValue);
        m_assembler.zeroExtend<bitSize>(expectedAndClobbered);

        // The BaseIndex address is resolved into the memory temp. The address is aligned to the 4-byte
        // boundary, and the remainder is used to calculate the shift amount for the exact position
        // in the 32-bit word where the target bit pattern is located.
        auto resolution = resolveAddress(address, temp.memory());
        m_assembler.addi_dInsn(temp.memory(), resolution.base, Imm::I12(resolution.offset));
        m_assembler.andiInsn(temp.data(), temp.memory(), Imm::I12<0b11>());
        loadImmediate(TrustedImm32(~0b11), temp.data2());
        m_assembler.andInsn(temp.memory(), temp.memory(), temp.data2());
        m_assembler.slli_dInsn(temp.data(), temp.data(), 3);
        m_assembler.addi_dInsn(temp.data(), temp.data(), Imm::I12<32>());

        // The XOR value in the expected-value register is shifted into the appropriate position in
        // the upper half of the register. The shift value is OR-ed into the lower half.
        m_assembler.sll_dInsn(expectedAndClobbered, expectedAndClobbered, temp.data());
        m_assembler.orInsn(expectedAndClobbered, expectedAndClobbered, temp.data());

        // The 32-bit value is loaded through the load-reserve instruction, and then shifted into the
        // upper 32 bits of the register. XOR against the expected-value register will, in the upper
        // 32 bits of the register, produce the 32-bit word with the expected value replaced by the new one.
        m_assembler.ll_wInsn(temp.data(), temp.memory(), 0);
        m_assembler.slli_dInsn(temp.data(), temp.data(), 32);
        m_assembler.xorInsn(expectedAndClobbered, temp.data(), expectedAndClobbered);

        // We still have to validate that the expected value, after XOR, matches the new one. The upper
        // 32 bits of the expected-value register are shifted by the pre-prepared shift amount stored
        // in the lower half of that same register. This works becasue the shift amount is read only from
        // the bottom 6 bits of the shift-amount register. XOR-ing against the new-value register and shifting
        // back left should leave is with a zero value, in which case the expected-value bit pattern matched
        // the one that was loaded from memory. If non-zero, the failure branch is taken.
        m_assembler.srl_dInsn(temp.data(), expectedAndClobbered, expectedAndClobbered);
        m_assembler.xorInsn(temp.data(), temp.data(), newValue);
        m_assembler.slli_dInsn(temp.data(), temp.data(), 64 - bitSize);
        failure.append(makeBranch(NotEqual, temp.data(), LOONGARCH64Registers::zero));

        // The corresponding store-conditional remains. The 32-bit word, containing the new value after
        // the XOR, is located in the upper 32 bits of the expected-value register. That can be shifted
        // down and then used in the store-conditional instruction.
        m_assembler.srli_dInsn(expectedAndClobbered, expectedAndClobbered, 32);
        m_assembler.orInsn(temp.data2(), expectedAndClobbered, LOONGARCH64Registers::zero);
        m_assembler.sc_wInsn(temp.data2(), temp.memory(), 0);

        // On successful store, the temp register will have a zero value, and a non-zero value otherwise.
        // Branches are produced accordingly.
        switch (cond) {
        case Success: {
            Jump success = makeBranch(Equal, temp.data(), LOONGARCH64Registers::zero);
            failure.link(this);
            return JumpList(success);
        }
        case Failure:
            failure.append(makeBranch(NotEqual, temp.data(), LOONGARCH64Registers::zero));
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
        auto temp = temps<Data, Data2, Memory>();
        JumpList failure;

        RELEASE_ASSERT(expectedAndClobbered != temp.data() && newValue != temp.data2());

        auto resolution = resolveAddress(address, temp.memory());
        m_assembler.addi_dInsn(temp.memory(), resolution.base, Imm::I12(resolution.offset));
        m_assembler.zeroExtend<32>(expectedAndClobbered, expectedAndClobbered);

        m_assembler.ll_wInsn(temp.data(), temp.memory(), 0);
        m_assembler.lu32i_dInsn(temp.data(), Imm::I20<0>()); // !sign
        m_assembler.xorInsn(temp.data(), temp.data(), expectedAndClobbered);
        failure.append(makeBranch(NotEqual, temp.data(), LOONGARCH64Registers::zero));
        m_assembler.orInsn(temp.data2(), newValue, LOONGARCH64Registers::zero);
        m_assembler.sc_wInsn(temp.data2(), temp.memory(), 0);

        switch (cond) {
        case Success: {
            Jump success = makeBranch(Equal, temp.data(), LOONGARCH64Registers::zero);
            failure.link(this);
            return JumpList(success);
        }
        case Failure:
            failure.append(makeBranch(NotEqual, temp.data(), LOONGARCH64Registers::zero));
            break;
        }

        return failure;
    }

    JumpList branchAtomicWeakCAS64(StatusCondition cond, RegisterID expectedAndClobbered, RegisterID newValue, BaseIndex address)
    {
        auto temp = temps<Data, Data2, Memory>();
        JumpList failure;

        RELEASE_ASSERT(expectedAndClobbered != temp.data() && newValue != temp.data2());

        auto resolution = resolveAddress(address, temp.memory());
        m_assembler.addi_dInsn(temp.memory(), resolution.base, Imm::I12(resolution.offset));

        m_assembler.ll_dInsn(temp.data(), temp.memory(), 0);
        failure.append(makeBranch(NotEqual, temp.data(), expectedAndClobbered));
        m_assembler.orInsn(temp.data2(), newValue, LOONGARCH64Registers::zero);
        m_assembler.sc_dInsn(temp.data2(), temp.memory(), 0);

        switch (cond) {
        case Success: {
            Jump success = makeBranch(Equal, temp.data(), LOONGARCH64Registers::zero);
            failure.link(this);
            return JumpList(success);
        }
        case Failure:
            failure.append(makeBranch(NotEqual, temp.data(), LOONGARCH64Registers::zero));
            break;
        }

        return failure;
    }

    void atomicLoad32(Address address, RegisterID dest)
    {
        auto resolution = resolveAddress(address, lazyTemp<Memory>());
        memoryFence();
        m_assembler.ld_wInsn(dest, resolution.base, Imm::I12(resolution.offset));
        loadFence();
    }

    void atomicLoad32(BaseIndex address, RegisterID dest)
    {
        auto resolution = resolveAddress(address, lazyTemp<Memory>());
        memoryFence();
        m_assembler.ld_wInsn(dest, resolution.base, Imm::I12(resolution.offset));
        loadFence();
    }

    void atomicLoad64(Address address, RegisterID dest)
    {
        auto resolution = resolveAddress(address, lazyTemp<Memory>());
        memoryFence();
        m_assembler.ld_dInsn(dest, resolution.base, Imm::I12(resolution.offset));
        loadFence();
    }

    void atomicLoad64(BaseIndex address, RegisterID dest)
    {
        auto resolution = resolveAddress(address, lazyTemp<Memory>());
        memoryFence();
        m_assembler.ld_dInsn(dest, resolution.base, Imm::I12(resolution.offset));
        loadFence();
    }

    void moveConditionally32(RelationalCondition cond, RegisterID lhs, RegisterID rhs, RegisterID src, RegisterID dest)
    {
        auto temp = temps<Data, Memory>();
        m_assembler.signExtend<32>(temp.data(), lhs);
        m_assembler.signExtend<32>(temp.memory(), rhs);

        branchForMoveConditionally(invert(cond), temp.data(), temp.memory(), 8);
        m_assembler.addi_dInsn(dest, src, Imm::I12<0>());
    }

    void moveConditionally32(RelationalCondition cond, RegisterID lhs, RegisterID rhs, RegisterID trueSrc, RegisterID falseSrc, RegisterID dest)
    {
        auto temp = temps<Data, Memory>();
        m_assembler.signExtend<32>(temp.data(), lhs);
        m_assembler.signExtend<32>(temp.memory(), rhs);

        branchForMoveConditionally(invert(cond), temp.data(), temp.memory(), 12);
        m_assembler.addi_dInsn(dest, trueSrc, Imm::I12<0>());
        m_assembler.bInsn(8);
        m_assembler.addi_dInsn(dest, falseSrc, Imm::I12<0>());
    }

    void moveConditionally32(RelationalCondition cond, RegisterID lhs, TrustedImm32 imm, RegisterID trueSrc, RegisterID falseSrc, RegisterID dest)
    {
        auto temp = temps<Data, Memory>();
        m_assembler.signExtend<32>(temp.data(), lhs);
        loadImmediate(imm, temp.memory());

        branchForMoveConditionally(invert(cond), temp.data(), temp.memory(), 12);
        m_assembler.addi_dInsn(dest, trueSrc, Imm::I12<0>());
        m_assembler.bInsn(8);
        m_assembler.addi_dInsn(dest, falseSrc, Imm::I12<0>());
    }

    void moveConditionally64(RelationalCondition cond, RegisterID lhs, RegisterID rhs, RegisterID src, RegisterID dest)
    {
        branchForMoveConditionally(invert(cond), lhs, rhs, 8);
        m_assembler.addi_dInsn(dest, src, Imm::I12<0>());
    }

    void moveConditionally64(RelationalCondition cond, RegisterID lhs, RegisterID rhs, RegisterID trueSrc, RegisterID falseSrc, RegisterID dest)
    {
        branchForMoveConditionally(invert(cond), lhs, rhs, 12);
        m_assembler.addi_dInsn(dest, trueSrc, Imm::I12<0>());
        m_assembler.bInsn(8);
        m_assembler.addi_dInsn(dest, falseSrc, Imm::I12<0>());
    }

    void moveConditionally64(RelationalCondition cond, RegisterID lhs, TrustedImm32 imm, RegisterID trueSrc, RegisterID falseSrc, RegisterID dest)
    {
        auto temp = temps<Data>();
        loadImmediate(imm, temp.data());
        branchForMoveConditionally(invert(cond), lhs, temp.data(), 12);
        m_assembler.addi_dInsn(dest, trueSrc, Imm::I12<0>());
        m_assembler.bInsn(8);
        m_assembler.addi_dInsn(dest, falseSrc, Imm::I12<0>());
    }

    void moveConditionallyFloat(DoubleCondition cond, FPRegisterID lhs, FPRegisterID rhs, RegisterID src, RegisterID dest)
    {
        Jump invcondBranch = branchFP<32, true>(cond, lhs, rhs);
        m_assembler.addi_dInsn(dest, src, Imm::I12<0>());
        invcondBranch.link(this);
    }

    void moveConditionallyFloat(DoubleCondition cond, FPRegisterID lhs, FPRegisterID rhs, RegisterID trueSrc, RegisterID falseSrc, RegisterID dest)
    {
        Jump invcondBranch = branchFP<32, true>(cond, lhs, rhs);
        m_assembler.addi_dInsn(dest, trueSrc, Imm::I12<0>());
        Jump end = jump();
        invcondBranch.link(this);
        m_assembler.addi_dInsn(dest, falseSrc, Imm::I12<0>());
        end.link(this);
    }

    void moveConditionallyFloatWithZero(DoubleCondition cond, FPRegisterID left, RegisterID src, RegisterID dest)
    {
        UNUSED_PARAM(cond);
        UNUSED_PARAM(left);
        UNUSED_PARAM(src);
        UNUSED_PARAM(dest);
        UNREACHABLE_FOR_PLATFORM();
    }

    void moveConditionallyFloatWithZero(DoubleCondition cond, FPRegisterID left, RegisterID thenCase, RegisterID elseCase, RegisterID dest)
    {
        UNUSED_PARAM(cond);
        UNUSED_PARAM(left);
        UNUSED_PARAM(thenCase);
        UNUSED_PARAM(elseCase);
        UNUSED_PARAM(dest);
        UNREACHABLE_FOR_PLATFORM();
    }

    void moveConditionallyDouble(DoubleCondition cond, FPRegisterID lhs, FPRegisterID rhs, RegisterID src, RegisterID dest)
    {
        Jump invcondBranch = branchFP<64, true>(cond, lhs, rhs);
        m_assembler.addi_dInsn(dest, src, Imm::I12<0>());
        invcondBranch.link(this);
    }

    void moveConditionallyDouble(DoubleCondition cond, FPRegisterID lhs, FPRegisterID rhs, RegisterID trueSrc, RegisterID falseSrc, RegisterID dest)
    {
        Jump invcondBranch = branchFP<64, true>(cond, lhs, rhs);
        m_assembler.addi_dInsn(dest, trueSrc, Imm::I12<0>());
        Jump end = jump();
        invcondBranch.link(this);
        m_assembler.addi_dInsn(dest, falseSrc, Imm::I12<0>());
        end.link(this);
    }

    void moveConditionallyDoubleWithZero(DoubleCondition cond, FPRegisterID left, RegisterID src, RegisterID dest)
    {
        UNUSED_PARAM(cond);
        UNUSED_PARAM(left);
        UNUSED_PARAM(src);
        UNUSED_PARAM(dest);
        UNREACHABLE_FOR_PLATFORM();
    }

    void moveConditionallyDoubleWithZero(DoubleCondition cond, FPRegisterID left, RegisterID thenCase, RegisterID elseCase, RegisterID dest)
    {
        UNUSED_PARAM(cond);
        UNUSED_PARAM(left);
        UNUSED_PARAM(thenCase);
        UNUSED_PARAM(elseCase);
        UNUSED_PARAM(dest);
        UNREACHABLE_FOR_PLATFORM();
    }

    void moveConditionallyTest32(ResultCondition cond, RegisterID value, RegisterID mask, RegisterID src, RegisterID dest)
    {
        auto temp = temps<Data>();
        m_assembler.andInsn(temp.data(), value, mask);
        m_assembler.signExtend<32>(temp.data());
        testFinalize(cond, temp.data(), temp.data());

        m_assembler.beqInsn(temp.data(), LOONGARCH64Registers::zero, 8);
        m_assembler.addi_dInsn(dest, src, Imm::I12<0>());
    }

    void moveConditionallyTest32(ResultCondition cond, RegisterID value, RegisterID mask, RegisterID trueSrc, RegisterID falseSrc, RegisterID dest)
    {
        auto temp = temps<Data>();
        m_assembler.andInsn(temp.data(), value, mask);
        m_assembler.signExtend<32>(temp.data());
        testFinalize(cond, temp.data(), temp.data());

        m_assembler.beqInsn(temp.data(), LOONGARCH64Registers::zero, 12);
        m_assembler.addi_dInsn(dest, trueSrc, Imm::I12<0>());
        m_assembler.bInsn(8);
        m_assembler.addi_dInsn(dest, falseSrc, Imm::I12<0>());
    }

    void moveConditionallyTest32(ResultCondition cond, RegisterID value, TrustedImm32 mask, RegisterID trueSrc, RegisterID falseSrc, RegisterID dest)
    {
        auto temp = temps<Data>();
        loadImmediate(mask, temp.data());
        m_assembler.andInsn(temp.data(), value, temp.data());
        m_assembler.signExtend<32>(temp.data());
        testFinalize(cond, temp.data(), temp.data());

        m_assembler.beqInsn(temp.data(), LOONGARCH64Registers::zero, 12);
        m_assembler.addi_dInsn(dest, trueSrc, Imm::I12<0>());
        m_assembler.bInsn(8);
        m_assembler.addi_dInsn(dest, falseSrc, Imm::I12<0>());
    }

    void moveConditionallyTest64(ResultCondition cond, RegisterID value, RegisterID mask, RegisterID src, RegisterID dest)
    {
        auto temp = temps<Data>();
        m_assembler.andInsn(temp.data(), value, mask);
        testFinalize(cond, temp.data(), temp.data());

        m_assembler.beqInsn(temp.data(), LOONGARCH64Registers::zero, 8);
        m_assembler.addi_dInsn(dest, src, Imm::I12<0>());
    }

    void moveConditionallyTest64(ResultCondition cond, RegisterID value, RegisterID mask, RegisterID trueSrc, RegisterID falseSrc, RegisterID dest)
    {
        auto temp = temps<Data>();
        m_assembler.andInsn(temp.data(), value, mask);
        testFinalize(cond, temp.data(), temp.data());

        m_assembler.beqInsn(temp.data(), LOONGARCH64Registers::zero, 12);
        m_assembler.addi_dInsn(dest, trueSrc, Imm::I12<0>());
        m_assembler.bInsn(8);
        m_assembler.addi_dInsn(dest, falseSrc, Imm::I12<0>());
    }

    void moveConditionallyTest64(ResultCondition cond, RegisterID value, TrustedImm32 mask, RegisterID trueSrc, RegisterID falseSrc, RegisterID dest)
    {
        auto temp = temps<Data>();
        loadImmediate(mask, temp.data());
        m_assembler.andInsn(temp.data(), value, temp.data());
        testFinalize(cond, temp.data(), temp.data());

        m_assembler.beqInsn(temp.data(), LOONGARCH64Registers::zero, 12);
        m_assembler.addi_dInsn(dest, trueSrc, Imm::I12<0>());
        m_assembler.bInsn(8);
        m_assembler.addi_dInsn(dest, falseSrc, Imm::I12<0>());
    }

    void moveDoubleConditionally32(RelationalCondition cond, RegisterID lhs, RegisterID rhs, FPRegisterID trueSrc, FPRegisterID falseSrc, FPRegisterID dest)
    {
        auto temp = temps<Data, Memory>();
        m_assembler.signExtend<32>(temp.data(), lhs);
        m_assembler.signExtend<32>(temp.memory(), rhs);

        branchForMoveConditionally(invert(cond), temp.data(), temp.memory(), 12);
        m_assembler.fcopysignInsn<64>(dest, trueSrc, trueSrc);
        m_assembler.bInsn(8);
        m_assembler.fcopysignInsn<64>(dest, falseSrc, falseSrc);
    }

    void moveDoubleConditionally32(RelationalCondition cond, RegisterID lhs, TrustedImm32 imm, FPRegisterID trueSrc, FPRegisterID falseSrc, FPRegisterID dest)
    {
        auto temp = temps<Data, Memory>();
        m_assembler.signExtend<32>(temp.data(), lhs);
        loadImmediate(imm, temp.memory());

        branchForMoveConditionally(invert(cond), temp.data(), temp.memory(), 12);
        m_assembler.fcopysignInsn<64>(dest, trueSrc, trueSrc);
        m_assembler.bInsn(8);
        m_assembler.fcopysignInsn<64>(dest, falseSrc, falseSrc);
    }

    void moveDoubleConditionally64(RelationalCondition cond, RegisterID lhs, RegisterID rhs, FPRegisterID trueSrc, FPRegisterID falseSrc, FPRegisterID dest)
    {
        branchForMoveConditionally(invert(cond), lhs, rhs, 12);
        m_assembler.fcopysignInsn<64>(dest, trueSrc, trueSrc);
        m_assembler.bInsn(8);
        m_assembler.fcopysignInsn<64>(dest, falseSrc, falseSrc);
    }

    void moveDoubleConditionally64(RelationalCondition cond, RegisterID lhs, TrustedImm32 imm, FPRegisterID trueSrc, FPRegisterID falseSrc, FPRegisterID dest)
    {
        auto temp = temps<Data>();
        loadImmediate(imm, temp.data());
        branchForMoveConditionally(invert(cond), lhs, temp.data(), 12);
        m_assembler.fcopysignInsn<64>(dest, trueSrc, trueSrc);
        m_assembler.bInsn(8);
        m_assembler.fcopysignInsn<64>(dest, falseSrc, falseSrc);
    }

    void moveDoubleConditionallyFloat(DoubleCondition cond, FPRegisterID lhs, FPRegisterID rhs, FPRegisterID trueSrc, FPRegisterID falseSrc, FPRegisterID dest)
    {
        Jump invcondBranch = branchFP<32, true>(cond, lhs, rhs);
        m_assembler.fcopysignInsn<64>(dest, trueSrc, trueSrc);
        Jump end = jump();
        invcondBranch.link(this);
        m_assembler.fcopysignInsn<64>(dest, falseSrc, falseSrc);
        end.link(this);
    }

    void moveDoubleConditionallyDouble(DoubleCondition cond, FPRegisterID lhs, FPRegisterID rhs, FPRegisterID trueSrc, FPRegisterID falseSrc, FPRegisterID dest)
    {
        Jump invcondBranch = branchFP<64, true>(cond, lhs, rhs);
        m_assembler.fcopysignInsn<64>(dest, trueSrc, trueSrc);
        Jump end = jump();
        invcondBranch.link(this);
        m_assembler.dbarInsn(0x0);
        m_assembler.fcopysignInsn<64>(dest, falseSrc, falseSrc);
        end.link(this);
    }

    void moveDoubleConditionallyFloatWithZero(DoubleCondition cond, FPRegisterID left, FPRegisterID thenCase, FPRegisterID elseCase, FPRegisterID dest)
    {
        UNUSED_PARAM(cond);
        UNUSED_PARAM(left);
        UNUSED_PARAM(thenCase);
        UNUSED_PARAM(elseCase);
        UNUSED_PARAM(dest);
        UNREACHABLE_FOR_PLATFORM();
    }

    void moveDoubleConditionallyDoubleWithZero(DoubleCondition cond, FPRegisterID left, FPRegisterID thenCase, FPRegisterID elseCase, FPRegisterID dest)
    {
        UNUSED_PARAM(cond);
        UNUSED_PARAM(left);
        UNUSED_PARAM(thenCase);
        UNUSED_PARAM(elseCase);
        UNUSED_PARAM(dest);
        UNREACHABLE_FOR_PLATFORM();
    }

    void moveDoubleConditionallyTest32(ResultCondition cond, RegisterID value, RegisterID mask, FPRegisterID trueSrc, FPRegisterID falseSrc, FPRegisterID dest)
    {
        auto temp = temps<Data>();
        m_assembler.andInsn(temp.data(), value, mask);
        m_assembler.signExtend<32>(temp.data());
        testFinalize(cond, temp.data(), temp.data());

        m_assembler.beqInsn(temp.data(), LOONGARCH64Registers::zero, 12);
        m_assembler.fcopysignInsn<64>(dest, trueSrc, trueSrc);
        m_assembler.bInsn(8);
        m_assembler.fcopysignInsn<64>(dest, falseSrc, falseSrc);
    }

    void moveDoubleConditionallyTest32(ResultCondition cond, RegisterID value, TrustedImm32 mask, FPRegisterID trueSrc, FPRegisterID falseSrc, FPRegisterID dest)
    {
        auto temp = temps<Data>();
        loadImmediate(mask, temp.data());
        m_assembler.andInsn(temp.data(), value, temp.data());
        m_assembler.signExtend<32>(temp.data());
        testFinalize(cond, temp.data(), temp.data());

        m_assembler.beqInsn(temp.data(), LOONGARCH64Registers::zero, 12);
        m_assembler.fcopysignInsn<64>(dest, trueSrc, trueSrc);
        m_assembler.bInsn(8);
        m_assembler.fcopysignInsn<64>(dest, falseSrc, falseSrc);
    }

    void moveDoubleConditionallyTest64(ResultCondition cond, RegisterID value, RegisterID mask, FPRegisterID trueSrc, FPRegisterID falseSrc, FPRegisterID dest)
    {
        auto temp = temps<Data>();
        m_assembler.andInsn(temp.data(), value, mask);
        testFinalize(cond, temp.data(), temp.data());

        m_assembler.beqInsn(temp.data(), LOONGARCH64Registers::zero, 12);
        m_assembler.fcopysignInsn<64>(dest, trueSrc, trueSrc);
        m_assembler.bInsn(8);
        m_assembler.fcopysignInsn<64>(dest, falseSrc, falseSrc);
    }

    void moveDoubleConditionallyTest64(ResultCondition cond, RegisterID value, TrustedImm32 mask, FPRegisterID trueSrc, FPRegisterID falseSrc, FPRegisterID dest)
    {
        auto temp = temps<Data>();
        loadImmediate(mask, temp.data());
        m_assembler.andInsn(temp.data(), value, temp.data());
        testFinalize(cond, temp.data(), temp.data());

        m_assembler.beqInsn(temp.data(), LOONGARCH64Registers::zero, 12);
        m_assembler.fcopysignInsn<64>(dest, trueSrc, trueSrc);
        m_assembler.bInsn(8);
        m_assembler.fcopysignInsn<64>(dest, falseSrc, falseSrc);
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

private:
    enum class ArithmeticOperation {
        Addition,
        Subtraction,
        Multiplication,
    };

    struct Imm {
        template<typename T>
        using EnableIfInteger = std::enable_if_t<(std::is_same_v<T, int32_t> || std::is_same_v<T, int64_t>)>;

        using I20Type = LOONGARCH64Assembler::I20Immediate;
        template<int32_t value>
        static I20Type I20() { return I20Type::v<I20Type, value>(); }
        template<typename T, typename = EnableIfInteger<T>>
        static I20Type I20(T value) { return I20Type::v<I20Type>(value); }
        static I20Type I20(uint32_t value) { return I20Type(value); }

        using I16Type = LOONGARCH64Assembler::I16Immediate;
        template<int32_t value>
        static I16Type I16() { return I16Type::v<I16Type, value>(); }
        template<typename T, typename = EnableIfInteger<T>>
        static I16Type I16(T value) { return I16Type::v<I16Type>(value); }
        static I16Type I16(uint32_t value) { return I16Type(value); }

        using I12Type = LOONGARCH64Assembler::I12Immediate;
        template<int32_t value>
        static I12Type I12() { return I12Type::v<I12Type, value>(); }
        template<typename T, typename = EnableIfInteger<T>>
        static I12Type I12(T value) { return I12Type::v<I12Type>(value); }
        static I12Type I12(uint32_t value) { return I12Type(value); }
    };

    void loadImmediate(TrustedImm32 imm, RegisterID dest)
    {
        LOONGARCH64Assembler::ImmediateLoader(imm.m_value).moveInto(m_assembler, dest);
    }

    void loadImmediate(TrustedImm64 imm, RegisterID dest)
    {
        LOONGARCH64Assembler::ImmediateLoader(imm.m_value).moveInto(m_assembler, dest);
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
            if (LOONGARCH64Assembler::ImmediateBase<12>::isSImm<12>(address.offset)) {
                if (address.scale != TimesOne) {
                    m_assembler.slli_dInsn(destination, address.index, address.scale);
                    m_assembler.add_dInsn(destination, address.base, destination);
                } else
                    m_assembler.add_dInsn(destination, address.base, address.index);
                return { destination, address.offset };
            }

            if (address.scale != TimesOne) {
                uint32_t scale = address.scale;
                int32_t upperOffset = address.offset >> scale;
                int32_t lowerOffset = address.offset & ((1 << scale) - 1);

                if (!LOONGARCH64Assembler::ImmediateBase<12>::isSImm<12>(upperOffset)) {
                    LOONGARCH64Assembler::ImmediateLoader imml(upperOffset);
                    imml.moveInto(m_assembler, destination);
                    m_assembler.add_dInsn(destination, address.index, destination);
                } else
                    m_assembler.addi_dInsn(destination, address.index, Imm::I12(upperOffset));
                m_assembler.slli_dInsn(destination, destination, scale);
                m_assembler.oriInsn(destination, destination, Imm::I12((uint32_t)lowerOffset));
            } else {
                LOONGARCH64Assembler::ImmediateLoader imml(address.offset);
                imml.moveInto(m_assembler, destination);
                m_assembler.add_dInsn(destination, destination, address.index);
            }
            m_assembler.add_dInsn(destination, address.base, destination);
            return { destination, 0 };
        }

        if (address.scale != TimesOne) {
            m_assembler.slli_dInsn(destination, address.index, address.scale);
            m_assembler.add_dInsn(destination, address.base, destination);
        } else
            m_assembler.add_dInsn(destination, address.base, address.index);
        return { destination, 0 };
    }

    template<typename RegisterType>
    AddressResolution resolveAddress(Address address, RegisterType destination)
    {
        if (LOONGARCH64Assembler::ImmediateBase<12>::isSImm<12>(address.offset))
            return { address.base, address.offset };

        LOONGARCH64Assembler::ImmediateLoader imml(int32_t(address.offset));
        imml.moveInto(m_assembler, destination);
        m_assembler.add_dInsn(destination, address.base, destination);
        return { destination, 0 };
    }

    template<typename RegisterType>
    AddressResolution resolveAddress(ExtendedAddress address, RegisterType destination)
    {
        if (LOONGARCH64Assembler::ImmediateBase<12>::isSImm<12>(address.offset))
            return { address.base, int32_t(address.offset) };

        LOONGARCH64Assembler::ImmediateLoader imml(int64_t(address.offset));
        imml.moveInto(m_assembler, destination);
        m_assembler.add_dInsn(destination, address.base, destination);
        return { destination, 0 };
    }

    Jump makeBranch(RelationalCondition condition, RegisterID lhs, RegisterID rhs)
    {
        auto label = m_assembler.label();
        m_assembler.branchPlaceholder(
            [&] {
                switch (condition) {
                case Equal:
                    return m_assembler.beqInsn(lhs, rhs, 0);
                case NotEqual:
                    return m_assembler.bneInsn(lhs, rhs, 0);
                case Above:
                    return m_assembler.bltuInsn(rhs, lhs, 0);
                case AboveOrEqual:
                    return m_assembler.bgeuInsn(lhs, rhs, 0);
                case Below:
                    return m_assembler.bltuInsn(lhs, rhs, 0);
                case BelowOrEqual:
                    return m_assembler.bgeuInsn(rhs, lhs, 0);
                case GreaterThan:
                    return m_assembler.bltInsn(rhs, lhs, 0);
                case GreaterThanOrEqual:
                    return m_assembler.bgeInsn(lhs, rhs, 0);
                case LessThan:
                    return m_assembler.bltInsn(lhs, rhs, 0);
                case LessThanOrEqual:
                    return m_assembler.bgeInsn(rhs, lhs, 0);
                }
            });
        return Jump(label);
    }

    Jump branchTestFinalize(ResultCondition cond, RegisterID src)
    {
        switch (cond) {
        case Carry:
        case Overflow:
            break;
        case Signed:
            return makeBranch(LessThan, src, LOONGARCH64Registers::zero);
        case PositiveOrZero:
            return makeBranch(GreaterThanOrEqual, src, LOONGARCH64Registers::zero);
        case Zero:
            return makeBranch(Equal, src, LOONGARCH64Registers::zero);
        case NonZero:
            return makeBranch(NotEqual, src, LOONGARCH64Registers::zero);
        }

        RELEASE_ASSERT_NOT_REACHED();
        return { };
    }

    template<unsigned bitSize, ArithmeticOperation arithmeticOperation, typename Op2Type>
    Jump branchForArithmeticOverflow(RegisterID op1, Op2Type op2, RegisterID dest)
    {
        static_assert(bitSize == 32 || bitSize == 64);
        static_assert(std::is_same_v<Op2Type, RegisterID> || std::is_same_v<Op2Type, TrustedImm32>);
        auto temp = temps<Data, Data2, Memory>();

        if constexpr (bitSize == 32) {
            RELEASE_ASSERT(op1 == temp.data() || op1 != temp.memory());
            m_assembler.signExtend<32>(temp.data(), op1);

            if constexpr (!std::is_same_v<Op2Type, TrustedImm32>) {
                RELEASE_ASSERT(op2 == temp.memory() || op1 != temp.data());
                m_assembler.signExtend<32>(temp.memory(), op2);
            } else
                loadImmediate(op2, temp.memory());

            void (LOONGARCH64Assembler::*op)(RegisterID, RegisterID, RegisterID) =
                [] {
                    switch (arithmeticOperation) {
                    case ArithmeticOperation::Addition:
                        return &LOONGARCH64Assembler::add_dInsn;
                    case ArithmeticOperation::Subtraction:
                        return &LOONGARCH64Assembler::sub_dInsn;
                    case ArithmeticOperation::Multiplication:
                        return &LOONGARCH64Assembler::mul_dInsn;
                    }
                }();

            if (dest == temp.data() || dest == temp.memory()) {
                RegisterID otherTemp = (dest == temp.data()) ? temp.memory() : temp.data();
                (m_assembler.*op)(dest, temp.data(), temp.memory());
                m_assembler.signExtend<32>(otherTemp, dest);
                m_assembler.xorInsn(otherTemp, dest, otherTemp);
                m_assembler.maskRegister<32>(dest, dest);
                return makeBranch(NotEqual, otherTemp, LOONGARCH64Registers::zero);
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
                m_assembler.slli_dInsn(temp.memory(), dest, 1);
                m_assembler.xorInsn(temp.data(), temp.memory(), dest);
                move(temp.memory(), dest);
            } else {
                m_assembler.xorInsn(temp.data(), op1, rop2);
                loadImmediate(TrustedImm32(-1), temp.memory());
                m_assembler.xorInsn(temp.data(), temp.data(), temp.memory());

                m_assembler.add_dInsn(dest, op1, rop2);
                m_assembler.xorInsn(temp.memory(), (op1 == dest) ? rop2 : op1, dest);
                m_assembler.andInsn(temp.data(), temp.data(), temp.memory());
            }
            return makeBranch(LessThan, temp.data(), LOONGARCH64Registers::zero);
        }
        case ArithmeticOperation::Subtraction:
        {
            if (op1 == dest && rop2 == dest) {
                move(LOONGARCH64Registers::zero, dest);
                move(LOONGARCH64Registers::zero, temp.data());
            } else {
                m_assembler.xorInsn(temp.data(), op1, rop2);

                m_assembler.sub_dInsn(dest, op1, rop2);
                if (op1 == dest) {
                    m_assembler.xorInsn(temp.memory(), rop2, dest);
                    loadImmediate(TrustedImm32(-1), temp.data2());
                    m_assembler.xorInsn(temp.memory(), temp.memory(), temp.data2());
                } else
                    m_assembler.xorInsn(temp.memory(), op1, dest);
                m_assembler.andInsn(temp.data(), temp.data(), temp.memory());
            }
            return makeBranch(LessThan, temp.data(), LOONGARCH64Registers::zero);
        }
        case ArithmeticOperation::Multiplication:
            m_assembler.mulh_dInsn(temp.data(), op1, rop2);
            m_assembler.mul_dInsn(dest, op1, rop2);
            m_assembler.srai_dInsn(temp.memory(), dest, 0x3f);
            return makeBranch(NotEqual, temp.data(), temp.memory());
        }
    }

    void branchForMoveConditionally(RelationalCondition condition, RegisterID lhs, RegisterID rhs, int16_t offset)
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
            m_assembler.sltuiInsn(dest, dest, Imm::I12<1>());
            break;
        case NotEqual:
            m_assembler.xorInsn(dest, lhs, rhs);
            m_assembler.sltuInsn(dest, LOONGARCH64Registers::zero, dest);
            break;
        case Above:
            m_assembler.sltuInsn(dest, rhs, lhs);
            break;
        case AboveOrEqual:
            m_assembler.sltuInsn(dest, lhs, rhs);
            m_assembler.xoriInsn(dest, dest, Imm::I12<1>());
            break;
        case Below:
            m_assembler.sltuInsn(dest, lhs, rhs);
            break;
        case BelowOrEqual:
            m_assembler.sltuInsn(dest, rhs, lhs);
            m_assembler.xoriInsn(dest, dest, Imm::I12<1>());
            break;
        case GreaterThan:
            m_assembler.sltInsn(dest, rhs, lhs);
            break;
        case GreaterThanOrEqual:
            m_assembler.sltInsn(dest, lhs, rhs);
            m_assembler.xoriInsn(dest, dest, Imm::I12<1>());
            break;
        case LessThan:
            m_assembler.sltInsn(dest, lhs, rhs);
            break;
        case LessThanOrEqual:
            m_assembler.sltInsn(dest, rhs, lhs);
            m_assembler.xoriInsn(dest, dest, Imm::I12<1>());
            break;
        }
    }

    void testFinalize(ResultCondition cond, RegisterID src, RegisterID dest)
    {
        switch (cond) {
        case Carry:
        case Overflow:
        case Signed:
        case PositiveOrZero:
            // None of the above should be used for testing operations.
            RELEASE_ASSERT_NOT_REACHED();
            break;
        case Zero:
            m_assembler.sltuiInsn(dest, src, Imm::I12<1>());
            break;
        case NonZero:
            m_assembler.sltuInsn(dest, LOONGARCH64Registers::zero, src);
            break;
        }
    }

    template<unsigned fpSize, bool invert = false>
    Jump branchFP(DoubleCondition cond, FPRegisterID lhs, FPRegisterID rhs)
    {
        auto temp = temps<Data>();
        JumpList unorderedJump;

        // Detect any NaN values.
        m_assembler.fclassInsn<fpSize>(fpTempRegister2, lhs);
        m_assembler.movfr2grInsn<fpSize>(temp.data(), fpTempRegister2);
        m_assembler.andiInsn(temp.data(), temp.data(), Imm::I12<0b0000000011>());
        unorderedJump.append(makeBranch(NotEqual, temp.data(), LOONGARCH64Registers::zero));

        m_assembler.fclassInsn<fpSize>(fpTempRegister2, rhs);
        m_assembler.movfr2grInsn<fpSize>(temp.data(), fpTempRegister2);
        m_assembler.andiInsn(temp.data(), temp.data(), Imm::I12<0b0000000011>());
        unorderedJump.append(makeBranch(NotEqual, temp.data(), LOONGARCH64Registers::zero));

        switch (cond) {
        case DoubleEqualAndOrdered:
        case DoubleEqualOrUnordered:
            m_assembler.fcmp_ceqInsn<fpSize>(fcc0, lhs, rhs);
            m_assembler.movcf2grInsn(temp.data(), fcc0);
            break;
        case DoubleNotEqualAndOrdered:
        case DoubleNotEqualOrUnordered:
            m_assembler.fcmp_ceqInsn<fpSize>(fcc0, lhs, rhs);
            m_assembler.movcf2grInsn(temp.data(), fcc0);
            m_assembler.xoriInsn(temp.data(), temp.data(), Imm::I12<1>());
            break;
        case DoubleGreaterThanAndOrdered:
        case DoubleGreaterThanOrUnordered:
            m_assembler.fcmp_cltInsn<fpSize>(fcc0, rhs, lhs);
            m_assembler.movcf2grInsn(temp.data(), fcc0);
            break;
        case DoubleGreaterThanOrEqualAndOrdered:
        case DoubleGreaterThanOrEqualOrUnordered:
            m_assembler.fcmp_cleInsn<fpSize>(fcc0, rhs, lhs);
            m_assembler.movcf2grInsn(temp.data(), fcc0);
            break;
        case DoubleLessThanAndOrdered:
        case DoubleLessThanOrUnordered:
            m_assembler.fcmp_cltInsn<fpSize>(fcc0, lhs, rhs);
            m_assembler.movcf2grInsn(temp.data(), fcc0);
            break;
        case DoubleLessThanOrEqualAndOrdered:
        case DoubleLessThanOrEqualOrUnordered:
            m_assembler.fcmp_cleInsn<fpSize>(fcc0, lhs, rhs);
            m_assembler.movcf2grInsn(temp.data(), fcc0);
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
            m_assembler.addi_dInsn(temp.data(), LOONGARCH64Registers::zero, Imm::I12<0>());
            break;
        case DoubleEqualOrUnordered:
        case DoubleNotEqualOrUnordered:
        case DoubleGreaterThanOrUnordered:
        case DoubleGreaterThanOrEqualOrUnordered:
        case DoubleLessThanOrUnordered:
        case DoubleLessThanOrEqualOrUnordered:
            m_assembler.addi_dInsn(temp.data(), LOONGARCH64Registers::zero, Imm::I12<1>());
            break;
        }

        end.link(this);
        return makeBranch(invert ? Equal : NotEqual, temp.data(), LOONGARCH64Registers::zero);
    }

    template<unsigned fpSize, LOONGARCH64Assembler::FPRoundingMode RM>
    void roundFP(FPRegisterID src, FPRegisterID dest)
    {
        static_assert(fpSize == 32 || fpSize == 64);
        auto temp = temps<Data>();

        JumpList end;

        // Test the given source register for NaN condition. If detected, it should be
        // propagated to the destination register.
        m_assembler.fclassInsn<fpSize>(fpTempRegister2, src);
        m_assembler.movfr2grInsn<fpSize>(temp.data(), fpTempRegister2);
        m_assembler.andiInsn(temp.data(), temp.data(), Imm::I12<0b0000000011>());
        Jump notNaN = makeBranch(Equal, temp.data(), LOONGARCH64Registers::zero);

        m_assembler.faddInsn<fpSize>(dest, src, src);
        end.append(jump());

        notNaN.link(this);
        m_assembler.fabsInsn<fpSize>(fpTempRegister, src);

        // Compare the absolute source value with the maximum representable integer value.
        // Rounding is only possible if the absolute source value is smaller.
        if constexpr (fpSize == 32) {
            m_assembler.addi_dInsn(temp.data(), LOONGARCH64Registers::zero, Imm::I12<0b10010111>());
            m_assembler.slli_dInsn(temp.data(), temp.data(), 23);
            m_assembler.movgr2fr_wInsn(fpTempRegister2, temp.data());
        } else {
            m_assembler.addi_dInsn(temp.data(), LOONGARCH64Registers::zero, Imm::I12<0b10000110100>());
            m_assembler.slli_dInsn(temp.data(), temp.data(), 52);
            m_assembler.movgr2fr_dInsn(fpTempRegister2, temp.data());
        }

        m_assembler.fcmp_cltInsn<fpSize>(fcc0, fpTempRegister, fpTempRegister2);
        m_assembler.movcf2grInsn(temp.data(), fcc0);
        Jump notRoundable = makeBranch(Equal, temp.data(), LOONGARCH64Registers::zero);

        FPRegisterID dealiasedSrc = src;
        if (src == dest) {
            m_assembler.fmovInsn<fpSize>(fpTempRegister, src);
            dealiasedSrc = fpTempRegister;
        }

        // Rounding can now be done by roundtripping through a general-purpose register
        // with the desired rounding mode applied.
        if constexpr (fpSize == 32) {
            switch (RM) {
            case LOONGARCH64Assembler::FPRoundingMode::RM:
                m_assembler.ftintrm_wInsn<fpSize>(dest, dealiasedSrc);
                break;
            case LOONGARCH64Assembler::FPRoundingMode::RP:
                m_assembler.ftintrp_wInsn<fpSize>(dest, dealiasedSrc);
                break;
            case LOONGARCH64Assembler::FPRoundingMode::RZ:
                m_assembler.ftintrz_wInsn<fpSize>(dest, dealiasedSrc);
                break;
            case LOONGARCH64Assembler::FPRoundingMode::RNE:
                m_assembler.ftintrne_wInsn<fpSize>(dest, dealiasedSrc);
                break;
            default:
                RELEASE_ASSERT_NOT_REACHED();
                break;
            }
            m_assembler.ffint_wInsn<fpSize>(dest, dest);
        } else {
            switch (RM) {
            case LOONGARCH64Assembler::FPRoundingMode::RM:
                m_assembler.ftintrm_lInsn<fpSize>(dest, dealiasedSrc);
                break;
            case LOONGARCH64Assembler::FPRoundingMode::RP:
                m_assembler.ftintrp_lInsn<fpSize>(dest, dealiasedSrc);
                break;
            case LOONGARCH64Assembler::FPRoundingMode::RZ:
                m_assembler.ftintrz_lInsn<fpSize>(dest, dealiasedSrc);
                break;
            case LOONGARCH64Assembler::FPRoundingMode::RNE:
                m_assembler.ftintrne_lInsn<fpSize>(dest, dealiasedSrc);
                break;
            default:
                RELEASE_ASSERT_NOT_REACHED();
                break;
            }
            m_assembler.ffint_lInsn<fpSize>(dest, dest);
        }
        m_assembler.fcopysignInsn<fpSize>(dest, dest, dealiasedSrc);
        end.append(jump());

        notRoundable.link(this);
        // If not roundable, the value should still be moved over into the destination register.
        if (src != dest)
            m_assembler.fmovInsn<fpSize>(dest, src);

        end.link(this);
    }

    template<unsigned fpSize>
    void compareFP(DoubleCondition cond, FPRegisterID lhs, FPRegisterID rhs, RegisterID dest)
    {
        static_assert(fpSize == 32 || fpSize == 64);
        auto temp = temps<Data>();

        JumpList unorderedJump;

        // Detect any NaN values that could still yield a positive comparison, depending on the condition.
        m_assembler.fclassInsn<fpSize>(fpTempRegister2, lhs);
        m_assembler.movfr2grInsn<fpSize>(temp.data(), fpTempRegister2);
        m_assembler.andiInsn(temp.data(), temp.data(), Imm::I12<0b0000000011>());
        unorderedJump.append(makeBranch(NotEqual, temp.data(), LOONGARCH64Registers::zero));

        m_assembler.fclassInsn<fpSize>(fpTempRegister2, rhs);
        m_assembler.movfr2grInsn<fpSize>(temp.data(), fpTempRegister2);
        m_assembler.andiInsn(temp.data(), temp.data(), Imm::I12<0b0000000011>());
        unorderedJump.append(makeBranch(NotEqual, temp.data(), LOONGARCH64Registers::zero));

        switch (cond) {
        case DoubleEqualAndOrdered:
        case DoubleEqualOrUnordered:
            m_assembler.fcmp_ceqInsn<fpSize>(fcc0, lhs, rhs);
            m_assembler.movcf2grInsn(dest, fcc0);
            break;
        case DoubleNotEqualAndOrdered:
        case DoubleNotEqualOrUnordered:
            m_assembler.fcmp_ceqInsn<fpSize>(fcc0, lhs, rhs);
            m_assembler.movcf2grInsn(dest, fcc0);
            m_assembler.xoriInsn(dest, dest, Imm::I12<1>());
            break;
        case DoubleGreaterThanAndOrdered:
        case DoubleGreaterThanOrUnordered:
            m_assembler.fcmp_cltInsn<fpSize>(fcc0, rhs, lhs);
            m_assembler.movcf2grInsn(dest, fcc0);
            break;
        case DoubleGreaterThanOrEqualAndOrdered:
        case DoubleGreaterThanOrEqualOrUnordered:
            m_assembler.fcmp_cleInsn<fpSize>(fcc0, rhs, lhs);
            m_assembler.movcf2grInsn(dest, fcc0);
            break;
        case DoubleLessThanAndOrdered:
        case DoubleLessThanOrUnordered:
            m_assembler.fcmp_cltInsn<fpSize>(fcc0, lhs, rhs);
            m_assembler.movcf2grInsn(dest, fcc0);
            break;
        case DoubleLessThanOrEqualAndOrdered:
        case DoubleLessThanOrEqualOrUnordered:
            m_assembler.fcmp_cleInsn<fpSize>(fcc0, lhs, rhs);
            m_assembler.movcf2grInsn(dest, fcc0);
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
            m_assembler.addi_dInsn(dest, LOONGARCH64Registers::zero, Imm::I12<0>());
            break;
        case DoubleEqualOrUnordered:
        case DoubleNotEqualOrUnordered:
        case DoubleGreaterThanOrUnordered:
        case DoubleGreaterThanOrEqualOrUnordered:
        case DoubleLessThanOrUnordered:
        case DoubleLessThanOrEqualOrUnordered:
            m_assembler.addi_dInsn(dest, LOONGARCH64Registers::zero, Imm::I12<1>());
            break;
        }

        end.link(this);
    }
};

} // namespace JSC

#undef MACRO_ASSEMBLER_LOONGARCH64_TEMPLATED_NOOP_METHOD
#undef MACRO_ASSEMBLER_LOONGARCH64_TEMPLATED_NOOP_METHOD_WITH_RETURN

#endif // ENABLE(ASSEMBLER) && CPU(LOONGARCH64)
