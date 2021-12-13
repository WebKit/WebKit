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
        Equal,
        NotEqual,
        Above,
        AboveOrEqual,
        Below,
        BelowOrEqual,
        GreaterThan,
        GreaterThanOrEqual,
        LessThan,
        LessThanOrEqual,
    };

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

    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(countLeadingZeros32);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(countLeadingZeros64);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(countTrailingZeros32);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(countTrailingZeros64);

    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(byteSwap16);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(byteSwap32);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(byteSwap64);

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

    void storePair32(RegisterID src1, RegisterID src2, RegisterID dest)
    {
        storePair32(src1, src2, dest, TrustedImm32(0));
    }

    void storePair32(RegisterID src1, RegisterID src2, RegisterID dest, TrustedImm32 offset)
    {
        store32(src1, Address(dest, offset.m_value));
        store32(src2, Address(dest, offset.m_value + 4));
    }

    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD_WITH_RETURN(load64WithAddressOffsetPatch, DataLabel32);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD_WITH_RETURN(load64WithCompactAddressOffsetPatch, DataLabelCompact);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD_WITH_RETURN(store64WithAddressOffsetPatch, DataLabel32);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD_WITH_RETURN(storePtrWithPatch, DataLabelPtr);

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

    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(compare8);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(compare32);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(compare64);

    static RelationalCondition invert(RelationalCondition cond)
    {
        return static_cast<RelationalCondition>(Assembler::invert(static_cast<Assembler::Condition>(cond)));
    }

    template<PtrTag resultTag, PtrTag locationTag>
    static FunctionPtr<resultTag> readCallTarget(CodeLocationCall<locationTag>) { return { }; }

    template<PtrTag tag>
    static void replaceWithVMHalt(CodeLocationLabel<tag>) { }

    template<PtrTag startTag, PtrTag destTag>
    static void replaceWithJump(CodeLocationLabel<startTag>, CodeLocationLabel<destTag>) { }

    static ptrdiff_t maxJumpReplacementSize()
    {
        return Assembler::maxJumpReplacementSize();
    }

    static ptrdiff_t patchableJumpSize()
    {
        return Assembler::patchableJumpSize();
    }

    template<PtrTag tag>
    static CodeLocationLabel<tag> startOfBranchPtrWithPatchOnRegister(CodeLocationDataLabelPtr<tag>) { return { }; }

    template<PtrTag tag>
    static CodeLocationLabel<tag> startOfPatchableBranchPtrWithPatchOnAddress(CodeLocationDataLabelPtr<tag>) { return { }; }

    template<PtrTag tag>
    static CodeLocationLabel<tag> startOfPatchableBranch32WithPatchOnAddress(CodeLocationDataLabel32<tag>) { return { }; }

    template<PtrTag tag>
    static void revertJumpReplacementToBranchPtrWithPatch(CodeLocationLabel<tag>, RegisterID, void*) { }

    template<PtrTag tag>
    static void revertJumpReplacementToPatchableBranchPtrWithPatch(CodeLocationLabel<tag>, Address, void*) { }

    template<PtrTag tag>
    static void revertJumpReplacementToPatchableBranch32WithPatch(CodeLocationLabel<tag>, Address, int32_t) { }

    template<PtrTag callTag, PtrTag destTag>
    static void repatchCall(CodeLocationCall<callTag>, CodeLocationLabel<destTag>) { }

    template<PtrTag callTag, PtrTag destTag>
    static void repatchCall(CodeLocationCall<callTag>, FunctionPtr<destTag>) { }

    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD_WITH_RETURN(jump, Jump);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(farJump);

    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD_WITH_RETURN(nearCall, Call);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD_WITH_RETURN(nearTailCall, Call);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD_WITH_RETURN(threadSafePatchableNearCall, Call);

    void ret() { }

    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(test8);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(test32);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(test64);

    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD_WITH_RETURN(branch8, Jump);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD_WITH_RETURN(branch32, Jump);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD_WITH_RETURN(branch64, Jump);

    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD_WITH_RETURN(branch32WithPatch, Jump);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD_WITH_RETURN(branch32WithUnalignedHalfWords, Jump);

    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD_WITH_RETURN(branchAdd32, Jump);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD_WITH_RETURN(branchAdd64, Jump);

    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD_WITH_RETURN(branchSub32, Jump);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD_WITH_RETURN(branchSub64, Jump);

    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD_WITH_RETURN(branchMul32, Jump);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD_WITH_RETURN(branchMul64, Jump);

    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD_WITH_RETURN(branchNeg32, Jump);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD_WITH_RETURN(branchNeg64, Jump);

    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD_WITH_RETURN(branchTest8, Jump);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD_WITH_RETURN(branchTest32, Jump);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD_WITH_RETURN(branchTest64, Jump);

    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD_WITH_RETURN(branchPtr, Jump);

    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD_WITH_RETURN(moveWithPatch, DataLabel32);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD_WITH_RETURN(moveWithPatch, DataLabelPtr);

    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD_WITH_RETURN(branchPtrWithPatch, Jump);

    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD_WITH_RETURN(patchableBranch8, PatchableJump);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD_WITH_RETURN(patchableBranch32, PatchableJump);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD_WITH_RETURN(patchableBranch64, PatchableJump);

    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD_WITH_RETURN(branchFloat, Jump);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD_WITH_RETURN(branchDouble, Jump);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD_WITH_RETURN(branchDoubleNonZero, Jump);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD_WITH_RETURN(branchDoubleZeroOrNaN, Jump);

    enum BranchTruncateType { BranchIfTruncateFailed, BranchIfTruncateSuccessful };
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD_WITH_RETURN(branchTruncateDoubleToInt32, Jump);

    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(branchConvertDoubleToInt32);

    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD_WITH_RETURN(call, Call);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(callOperation);

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

    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(ceilFloat);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(ceilDouble);

    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(floorFloat);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(floorDouble);

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

    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(roundTowardNearestIntFloat);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(roundTowardNearestIntDouble);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(roundTowardZeroFloat);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(roundTowardZeroDouble);

    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD_WITH_RETURN(compareFloat, Jump);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD_WITH_RETURN(compareDouble, Jump);

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

    void memoryFence() { }
    void storeFence() { }
    void loadFence() { }

    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD_WITH_RETURN(branchAtomicWeakCAS8, JumpList);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD_WITH_RETURN(branchAtomicWeakCAS16, JumpList);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD_WITH_RETURN(branchAtomicWeakCAS32, JumpList);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD_WITH_RETURN(branchAtomicWeakCAS64, JumpList);

    template<PtrTag tag>
    static void linkCall(void*, Call, FunctionPtr<tag>) { }

private:
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
};

} // namespace JSC

#undef MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD
#undef MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD_WITH_RETURN

#endif // ENABLE(ASSEMBLER) && CPU(RISCV64)
