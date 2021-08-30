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

    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(add32);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(add64);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(sub32);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(sub64);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(mul32);
    MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD(mul64);
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
};

} // namespace JSC

#undef MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD
#undef MACRO_ASSEMBLER_RISCV64_TEMPLATED_NOOP_METHOD_WITH_RETURN

#endif // ENABLE(ASSEMBLER) && CPU(RISCV64)
