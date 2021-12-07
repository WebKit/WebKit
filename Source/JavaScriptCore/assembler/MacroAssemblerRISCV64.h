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
        move(imm, temp.data());
        m_assembler.addwInsn(dest, temp.data(), op2);
        m_assembler.maskRegister<32>(dest);
    }

    void add32(TrustedImm32 imm, AbsoluteAddress address)
    {
        auto temp = temps<Data, Memory>();
        move(TrustedImmPtr(address.m_ptr), temp.memory());
        if (Imm::isValid<Imm::IType>(imm.m_value)) {
            m_assembler.lwInsn(temp.data(), temp.memory(), Imm::I<0>());
            m_assembler.addiInsn(temp.data(), temp.data(), Imm::I(imm.m_value));
            m_assembler.swInsn(temp.memory(), temp.data(), Imm::S<0>());
            return;
        }

        m_assembler.lwInsn(temp.memory(), temp.memory(), Imm::I<0>());
        move(imm, temp.data());
        m_assembler.addInsn(temp.data(), temp.memory(), temp.data());

        move(TrustedImmPtr(address.m_ptr), temp.memory());
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
        move(imm, temp.data());
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
        move(imm, temp.data());
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
        move(imm, temp.data());
        m_assembler.addInsn(dest, temp.data(), op2);
    }

    void add64(TrustedImm32 imm, AbsoluteAddress address)
    {
        auto temp = temps<Data, Memory>();
        move(TrustedImmPtr(address.m_ptr), temp.memory());

        if (Imm::isValid<Imm::IType>(imm.m_value)) {
            m_assembler.ldInsn(temp.data(), temp.memory(), Imm::I<0>());
            m_assembler.addiInsn(temp.data(), temp.data(), Imm::I(imm.m_value));
            m_assembler.sdInsn(temp.memory(), temp.data(), Imm::S<0>());
            return;
        }

        m_assembler.ldInsn(temp.memory(), temp.memory(), Imm::I<0>());
        move(imm, temp.data());
        m_assembler.addInsn(temp.data(), temp.data(), temp.memory());

        move(TrustedImmPtr(address.m_ptr), temp.memory());
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

        move(imm, temp.memory());
        m_assembler.addInsn(temp.data(), temp.memory(), temp.data());

        resolution = resolveAddress(address, temp.memory());
        m_assembler.sdInsn(resolution.base, temp.data(), Imm::S(resolution.offset));
    }

    void add64(AbsoluteAddress address, RegisterID dest)
    {
        auto temp = temps<Memory>();
        move(TrustedImmPtr(address.m_ptr), temp.memory());
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
        move(TrustedImmPtr(address.m_ptr), temp.memory());

        if (Imm::isValid<Imm::IType>(-imm.m_value)) {
            m_assembler.lwInsn(temp.data(), temp.memory(), Imm::I<0>());
            m_assembler.addiwInsn(temp.data(), temp.data(), Imm::I(-imm.m_value));
            m_assembler.swInsn(temp.memory(), temp.data(), Imm::S<0>());
            return;
        }

        m_assembler.lwInsn(temp.memory(), temp.memory(), Imm::I<0>());
        move(imm, temp.data());
        m_assembler.subwInsn(temp.data(), temp.memory(), temp.data());

        move(TrustedImmPtr(address.m_ptr), temp.memory());
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

        move(imm, temp.memory());
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
        move(imm, temp.data());
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

    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(and32);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(and64);

    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(countLeadingZeros32);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(countLeadingZeros64);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(countTrailingZeros32);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(countTrailingZeros64);

    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(byteSwap16);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(byteSwap32);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(byteSwap64);

    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(lshift32);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(lshift64);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(rshift32);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(rshift64);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(urshift32);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(urshift64);

    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(rotateRight32);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(rotateRight64);

    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(load8);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(load8SignedExtendTo32);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(load16);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(load16Unaligned);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(load16SignedExtendTo32);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(load32);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(load32WithUnalignedHalfWords);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(load64);

    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(store8);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(store16);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(store32);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(store64);

    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(zeroExtend8To32);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(zeroExtend16To32);

    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(signExtend8To32);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(signExtend16To32);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(signExtend32ToPtr);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(zeroExtend32ToWord);

    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD_WITH_RETURN(load64WithAddressOffsetPatch, DataLabel32);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD_WITH_RETURN(load64WithCompactAddressOffsetPatch, DataLabelCompact);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD_WITH_RETURN(store64WithAddressOffsetPatch, DataLabel32);

    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(or8);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(or16);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(or32);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(or64);

    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(xor32);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(xor64);

    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(not32);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(not64);

    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(neg32);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(neg64);

    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(move);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(swap);

    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(moveZeroToDouble);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(moveFloatTo32);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(move32ToFloat);

    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(moveDouble);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(moveDoubleTo64);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(move64ToDouble);

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

    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(getEffectiveAddress);

    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(loadFloat);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(loadDouble);

    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(storeFloat);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(storeDouble);

    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(addFloat);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(addDouble);

    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(subFloat);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(subDouble);

    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(mulFloat);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(mulDouble);

    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(divFloat);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(divDouble);

    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(sqrtFloat);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(sqrtDouble);

    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(absFloat);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(absDouble);

    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(ceilFloat);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(ceilDouble);

    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(floorFloat);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(floorDouble);

    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(andFloat);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(andDouble);

    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(orFloat);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(orDouble);

    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(negateFloat);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(negateDouble);

    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(roundTowardNearestIntFloat);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(roundTowardNearestIntDouble);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(roundTowardZeroFloat);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(roundTowardZeroDouble);

    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD_WITH_RETURN(compareFloat, Jump);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD_WITH_RETURN(compareDouble, Jump);

    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(convertInt32ToFloat);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(convertInt32ToDouble);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(convertInt64ToFloat);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(convertInt64ToDouble);

    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(convertFloatToDouble);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(convertDoubleToFloat);

    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(truncateFloatToInt32);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(truncateFloatToUint32);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(truncateFloatToInt64);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(truncateFloatToUint64);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(truncateDoubleToInt32);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(truncateDoubleToUint32);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(truncateDoubleToInt64);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(truncateDoubleToUint64);

    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(push);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(pushPair);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(pushToSave);

    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(pop);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(popPair);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(popToRestore);

    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(abortWithReason);

    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD_WITH_RETURN(storePtrWithPatch, DataLabelPtr);

    void breakpoint(uint16_t = 0xc471) { }
    void nop() { }

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
