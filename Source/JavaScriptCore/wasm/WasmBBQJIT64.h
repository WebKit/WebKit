/*
 * Copyright (C) 2019-2023 Apple Inc. All rights reserved.
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

#if ENABLE(WEBASSEMBLY_BBQJIT)
#if USE(JSVALUE64)

#include "WasmBBQJIT.h"
#include "WasmCallingConvention.h"
#include "WasmCompilationContext.h"
#include "WasmFunctionParser.h"
#include "WasmLimits.h"

namespace JSC { namespace Wasm { namespace BBQJITImpl {

template<typename Functor>
auto BBQJIT::emitCheckAndPrepareAndMaterializePointerApply(Value pointer, uint32_t uoffset, uint32_t sizeOfOperation, Functor&& functor) -> decltype(auto)
{
    uint64_t boundary = static_cast<uint64_t>(sizeOfOperation) + uoffset - 1;

    ScratchScope<1, 0> scratches(*this);
    Location pointerLocation;

    if (pointer.isConst()) {
        uint64_t constantPointer = static_cast<uint64_t>(static_cast<uint32_t>(pointer.asI32()));
        uint64_t finalOffset = constantPointer + uoffset;
        if (!(finalOffset > static_cast<uint64_t>(std::numeric_limits<int32_t>::max()) || !B3::Air::Arg::isValidAddrForm(B3::Air::Move, finalOffset, Width::Width128))) {
            switch (m_mode) {
            case MemoryMode::BoundsChecking: {
                m_jit.move(TrustedImmPtr(constantPointer + boundary), wasmScratchGPR);
                throwExceptionIf(ExceptionType::OutOfBoundsMemoryAccess, m_jit.branchPtr(RelationalCondition::AboveOrEqual, wasmScratchGPR, wasmBoundsCheckingSizeRegister));
                break;
            }
            case MemoryMode::Signaling: {
                if (uoffset >= Memory::fastMappedRedzoneBytes()) {
                    uint64_t maximum = m_info.memory.maximum() ? m_info.memory.maximum().bytes() : std::numeric_limits<uint32_t>::max();
                    if ((constantPointer + boundary) >= maximum)
                        throwExceptionIf(ExceptionType::OutOfBoundsMemoryAccess, m_jit.jump());
                }
                break;
            }
            }
            return functor(CCallHelpers::Address(wasmBaseMemoryPointer, static_cast<int32_t>(finalOffset)));
        }
        pointerLocation = Location::fromGPR(scratches.gpr(0));
        emitMoveConst(pointer, pointerLocation);
    } else
        pointerLocation = loadIfNecessary(pointer);
    ASSERT(pointerLocation.isGPR());

    switch (m_mode) {
    case MemoryMode::BoundsChecking: {
        // We're not using signal handling only when the memory is not shared.
        // Regardless of signaling, we must check that no memory access exceeds the current memory size.
        m_jit.zeroExtend32ToWord(pointerLocation.asGPR(), wasmScratchGPR);
        if (boundary)
            m_jit.addPtr(TrustedImmPtr(boundary), wasmScratchGPR);
        throwExceptionIf(ExceptionType::OutOfBoundsMemoryAccess, m_jit.branchPtr(RelationalCondition::AboveOrEqual, wasmScratchGPR, wasmBoundsCheckingSizeRegister));
        break;
    }

    case MemoryMode::Signaling: {
        // We've virtually mapped 4GiB+redzone for this memory. Only the user-allocated pages are addressable, contiguously in range [0, current],
        // and everything above is mapped PROT_NONE. We don't need to perform any explicit bounds check in the 4GiB range because WebAssembly register
        // memory accesses are 32-bit. However WebAssembly register + offset accesses perform the addition in 64-bit which can push an access above
        // the 32-bit limit (the offset is unsigned 32-bit). The redzone will catch most small offsets, and we'll explicitly bounds check any
        // register + large offset access. We don't think this will be generated frequently.
        //
        // We could check that register + large offset doesn't exceed 4GiB+redzone since that's technically the limit we need to avoid overflowing the
        // PROT_NONE region, but it's better if we use a smaller immediate because it can codegens better. We know that anything equal to or greater
        // than the declared 'maximum' will trap, so we can compare against that number. If there was no declared 'maximum' then we still know that
        // any access equal to or greater than 4GiB will trap, no need to add the redzone.
        if (uoffset >= Memory::fastMappedRedzoneBytes()) {
            uint64_t maximum = m_info.memory.maximum() ? m_info.memory.maximum().bytes() : std::numeric_limits<uint32_t>::max();
            m_jit.zeroExtend32ToWord(pointerLocation.asGPR(), wasmScratchGPR);
            if (boundary)
                m_jit.addPtr(TrustedImmPtr(boundary), wasmScratchGPR);
            throwExceptionIf(ExceptionType::OutOfBoundsMemoryAccess, m_jit.branchPtr(RelationalCondition::AboveOrEqual, wasmScratchGPR, TrustedImmPtr(static_cast<int64_t>(maximum))));
        }
        break;
    }
    }

#if CPU(ARM64)
    if (!(static_cast<uint64_t>(uoffset) > static_cast<uint64_t>(std::numeric_limits<int32_t>::max()) || !B3::Air::Arg::isValidAddrForm(B3::Air::Move, uoffset, Width::Width128)))
        return functor(CCallHelpers::BaseIndex(wasmBaseMemoryPointer, pointerLocation.asGPR(), CCallHelpers::TimesOne, static_cast<int32_t>(uoffset), CCallHelpers::Extend::ZExt32));

    m_jit.addZeroExtend64(wasmBaseMemoryPointer, pointerLocation.asGPR(), wasmScratchGPR);
#else
    m_jit.zeroExtend32ToWord(pointerLocation.asGPR(), wasmScratchGPR);
    m_jit.addPtr(wasmBaseMemoryPointer, wasmScratchGPR);
#endif

    if (static_cast<uint64_t>(uoffset) > static_cast<uint64_t>(std::numeric_limits<int32_t>::max()) || !B3::Air::Arg::isValidAddrForm(B3::Air::Move, uoffset, Width::Width128)) {
        m_jit.addPtr(TrustedImmPtr(static_cast<int64_t>(uoffset)), wasmScratchGPR);
        return functor(Address(wasmScratchGPR));
    }
    return functor(Address(wasmScratchGPR, static_cast<int32_t>(uoffset)));
}

#if CPU(X86_64)
static inline RegisterSet clobbersForDivX86()
{
    static RegisterSet x86DivClobbers;
    static std::once_flag flag;
    std::call_once(
        flag,
        []() {
            RegisterSetBuilder builder;
            builder.add(X86Registers::eax, IgnoreVectors);
            builder.add(X86Registers::edx, IgnoreVectors);
            x86DivClobbers = builder.buildAndValidate();
        });
    return x86DivClobbers;
}

#define PREPARE_FOR_MOD_OR_DIV \
do { \
    for (JSC::Reg reg : clobbersForDivX86()) \
        clobber(reg); \
} while (false); \
ScratchScope<0, 0> scratches(*this, clobbersForDivX86())
#else
#define PREPARE_FOR_MOD_OR_DIV
#endif

#if CPU(X86_64)
template<typename IntType, bool IsMod>
void BBQJIT::emitModOrDiv(Value& lhs, Location lhsLocation, Value& rhs, Location rhsLocation, Value&, Location resultLocation)
{
    // FIXME: We currently don't do nearly as sophisticated instruction selection on Intel as we do on other platforms,
    // but there's no good reason we can't. We should probably port over the isel in the future if it seems to yield
    // dividends.

    constexpr bool isSigned = std::is_signed<IntType>();
    constexpr bool is32 = sizeof(IntType) == 4;

    ASSERT(lhsLocation.isRegister() || rhsLocation.isRegister());
    if (lhs.isConst())
        emitMoveConst(lhs, lhsLocation = Location::fromGPR(wasmScratchGPR));
    else if (rhs.isConst())
        emitMoveConst(rhs, rhsLocation = Location::fromGPR(wasmScratchGPR));
    ASSERT(lhsLocation.isRegister() && rhsLocation.isRegister());

    ASSERT(resultLocation.isRegister());
    ASSERT(lhsLocation.asGPR() != X86Registers::eax && lhsLocation.asGPR() != X86Registers::edx);
    ASSERT(rhsLocation.asGPR() != X86Registers::eax && lhsLocation.asGPR() != X86Registers::edx);

    ScratchScope<2, 0> scratches(*this, lhsLocation, rhsLocation, resultLocation);

    Jump toDiv, toEnd;

    Jump isZero = is32
        ? m_jit.branchTest32(ResultCondition::Zero, rhsLocation.asGPR())
        : m_jit.branchTest64(ResultCondition::Zero, rhsLocation.asGPR());
    throwExceptionIf(ExceptionType::DivisionByZero, isZero);
    if constexpr (isSigned) {
        if constexpr (is32)
            m_jit.compare32(RelationalCondition::Equal, rhsLocation.asGPR(), TrustedImm32(-1), scratches.gpr(0));
        else
            m_jit.compare64(RelationalCondition::Equal, rhsLocation.asGPR(), TrustedImm32(-1), scratches.gpr(0));
        if constexpr (is32)
            m_jit.compare32(RelationalCondition::Equal, lhsLocation.asGPR(), TrustedImm32(std::numeric_limits<int32_t>::min()), scratches.gpr(1));
        else {
            m_jit.move(TrustedImm64(std::numeric_limits<int64_t>::min()), scratches.gpr(1));
            m_jit.compare64(RelationalCondition::Equal, lhsLocation.asGPR(), scratches.gpr(1), scratches.gpr(1));
        }
        m_jit.and64(scratches.gpr(0), scratches.gpr(1));
        if constexpr (IsMod) {
            toDiv = m_jit.branchTest64(ResultCondition::Zero, scratches.gpr(1));
            // In this case, WASM doesn't want us to fault, but x86 will. So we set the result ourselves.
            if constexpr (is32)
                m_jit.xor32(resultLocation.asGPR(), resultLocation.asGPR());
            else
                m_jit.xor64(resultLocation.asGPR(), resultLocation.asGPR());
            toEnd = m_jit.jump();
        } else {
            Jump isNegativeOne = m_jit.branchTest64(ResultCondition::NonZero, scratches.gpr(1));
            throwExceptionIf(ExceptionType::IntegerOverflow, isNegativeOne);
        }
    }

    if (toDiv.isSet())
        toDiv.link(&m_jit);

    m_jit.move(lhsLocation.asGPR(), X86Registers::eax);

    if constexpr (is32 && isSigned) {
        m_jit.x86ConvertToDoubleWord32();
        m_jit.x86Div32(rhsLocation.asGPR());
    } else if constexpr (is32) {
        m_jit.xor32(X86Registers::edx, X86Registers::edx);
        m_jit.x86UDiv32(rhsLocation.asGPR());
    } else if constexpr (isSigned) {
        m_jit.x86ConvertToQuadWord64();
        m_jit.x86Div64(rhsLocation.asGPR());
    } else {
        m_jit.xor64(X86Registers::edx, X86Registers::edx);
        m_jit.x86UDiv64(rhsLocation.asGPR());
    }

    if constexpr (IsMod)
        m_jit.move(X86Registers::edx, resultLocation.asGPR());
    else
        m_jit.move(X86Registers::eax, resultLocation.asGPR());

    if (toEnd.isSet())
        toEnd.link(&m_jit);
}
#else
template<typename IntType, bool IsMod>
void BBQJIT::emitModOrDiv(Value& lhs, Location lhsLocation, Value& rhs, Location rhsLocation, Value&, Location resultLocation)
{
    constexpr bool isSigned = std::is_signed<IntType>();
    constexpr bool is32 = sizeof(IntType) == 4;

    ASSERT(lhsLocation.isRegister() || rhsLocation.isRegister());
    ASSERT(resultLocation.isRegister());

    bool checkedForZero = false, checkedForNegativeOne = false;
    if (rhs.isConst()) {
        int64_t divisor = is32 ? rhs.asI32() : rhs.asI64();
        if (!divisor) {
            emitThrowException(ExceptionType::DivisionByZero);
            return;
        }
        if (divisor == 1) {
            if constexpr (IsMod) {
                // N % 1 == 0
                if constexpr (is32)
                    m_jit.xor32(resultLocation.asGPR(), resultLocation.asGPR());
                else
                    m_jit.xor64(resultLocation.asGPR(), resultLocation.asGPR());
            } else
                m_jit.move(lhsLocation.asGPR(), resultLocation.asGPR());
            return;
        }
        if (divisor == -1) {
            // Check for INT_MIN / -1 case, and throw an IntegerOverflow exception if it occurs
            if (!IsMod && isSigned) {
                Jump jump = is32
                    ? m_jit.branch32(RelationalCondition::Equal, lhsLocation.asGPR(), TrustedImm32(std::numeric_limits<int32_t>::min()))
                    : m_jit.branch64(RelationalCondition::Equal, lhsLocation.asGPR(), TrustedImm64(std::numeric_limits<int64_t>::min()));
                throwExceptionIf(ExceptionType::IntegerOverflow, jump);
            }

            if constexpr (IsMod) {
                // N % 1 == 0
                if constexpr (is32)
                    m_jit.xor32(resultLocation.asGPR(), resultLocation.asGPR());
                else
                    m_jit.xor64(resultLocation.asGPR(), resultLocation.asGPR());
                return;
            }
            if constexpr (isSigned) {
                if constexpr (is32)
                    m_jit.neg32(lhsLocation.asGPR(), resultLocation.asGPR());
                else
                    m_jit.neg64(lhsLocation.asGPR(), resultLocation.asGPR());
                return;
            }

            // Fall through to general case.
        } else if (isPowerOfTwo(divisor)) {
            if constexpr (IsMod) {
                if constexpr (isSigned) {
                    // This constructs an extra operand with log2(divisor) bits equal to the sign bit of the dividend. If the dividend
                    // is positive, this is zero and adding it achieves nothing; but if the dividend is negative, this is equal to the
                    // divisor minus one, which is the exact amount of bias we need to get the correct result. Computing this for both
                    // positive and negative dividends lets us elide branching, but more importantly allows us to save a register by
                    // not needing an extra multiplySub at the end.
                    if constexpr (is32) {
                        m_jit.rshift32(lhsLocation.asGPR(), TrustedImm32(31), wasmScratchGPR);
                        m_jit.urshift32(wasmScratchGPR, TrustedImm32(32 - WTF::fastLog2(static_cast<unsigned>(divisor))), wasmScratchGPR);
                        m_jit.add32(wasmScratchGPR, lhsLocation.asGPR(), resultLocation.asGPR());
                    } else {
                        m_jit.rshift64(lhsLocation.asGPR(), TrustedImm32(63), wasmScratchGPR);
                        m_jit.urshift64(wasmScratchGPR, TrustedImm32(64 - WTF::fastLog2(static_cast<uint64_t>(divisor))), wasmScratchGPR);
                        m_jit.add64(wasmScratchGPR, lhsLocation.asGPR(), resultLocation.asGPR());
                    }

                    lhsLocation = resultLocation;
                }

                if constexpr (is32)
                    m_jit.and32(Imm32(static_cast<uint32_t>(divisor) - 1), lhsLocation.asGPR(), resultLocation.asGPR());
                else
                    m_jit.and64(TrustedImm64(static_cast<uint64_t>(divisor) - 1), lhsLocation.asGPR(), resultLocation.asGPR());

                if constexpr (isSigned) {
                    // The extra operand we computed is still in wasmScratchGPR - now we can subtract it from the result to get the
                    // correct answer.
                    if constexpr (is32)
                        m_jit.sub32(resultLocation.asGPR(), wasmScratchGPR, resultLocation.asGPR());
                    else
                        m_jit.sub64(resultLocation.asGPR(), wasmScratchGPR, resultLocation.asGPR());
                }
                return;
            }

            if constexpr (isSigned) {
                // If we are doing signed division, we need to bias the dividend for negative numbers.
                if constexpr (is32)
                    m_jit.add32(TrustedImm32(static_cast<int32_t>(divisor) - 1), lhsLocation.asGPR(), wasmScratchGPR);
                else
                    m_jit.add64(TrustedImm64(divisor - 1), lhsLocation.asGPR(), wasmScratchGPR);

                // moveConditionally seems to be faster than a branch here, even if it's well predicted.
                if (is32)
                    m_jit.moveConditionally32(RelationalCondition::GreaterThanOrEqual, lhsLocation.asGPR(), TrustedImm32(0), lhsLocation.asGPR(), wasmScratchGPR, wasmScratchGPR);
                else
                    m_jit.moveConditionally64(RelationalCondition::GreaterThanOrEqual, lhsLocation.asGPR(), TrustedImm32(0), lhsLocation.asGPR(), wasmScratchGPR, wasmScratchGPR);
                lhsLocation = Location::fromGPR(wasmScratchGPR);
            }

            // Emit the actual division instruction: arithmetic shift if signed,
            // logical shift if unsigned
            if constexpr (isSigned) {
                if constexpr (is32)
                    m_jit.rshift32(lhsLocation.asGPR(), m_jit.trustedImm32ForShift(Imm32(WTF::fastLog2(static_cast<unsigned>(divisor)))), resultLocation.asGPR());
                else
                    m_jit.rshift64(lhsLocation.asGPR(), TrustedImm32(WTF::fastLog2(static_cast<uint64_t>(divisor))), resultLocation.asGPR());
            } else {
                if constexpr (is32)
                    m_jit.urshift32(lhsLocation.asGPR(), m_jit.trustedImm32ForShift(Imm32(WTF::fastLog2(static_cast<unsigned>(divisor)))), resultLocation.asGPR());
                else
                    m_jit.urshift64(lhsLocation.asGPR(), TrustedImm32(WTF::fastLog2(static_cast<uint64_t>(divisor))), resultLocation.asGPR());
            }

            return;
        }
        // TODO: try generating integer reciprocal instead.
        checkedForNegativeOne = true;
        checkedForZero = true;
        rhsLocation = Location::fromGPR(wasmScratchGPR);
        emitMoveConst(rhs, rhsLocation);
        // Fall through to register/register div.
    } else if (lhs.isConst()) {
        int64_t dividend = is32 ? lhs.asI32() : lhs.asI64();

        Jump isZero = is32
            ? m_jit.branchTest32(ResultCondition::Zero, rhsLocation.asGPR())
            : m_jit.branchTest64(ResultCondition::Zero, rhsLocation.asGPR());
        throwExceptionIf(ExceptionType::DivisionByZero, isZero);
        checkedForZero = true;

        if (!dividend) {
            if constexpr (is32)
                m_jit.xor32(resultLocation.asGPR(), resultLocation.asGPR());
            else
                m_jit.xor64(resultLocation.asGPR(), resultLocation.asGPR());
            return;
        }
        if (isSigned && !IsMod && dividend == std::numeric_limits<IntType>::min()) {
            Jump isNegativeOne = is32
                ? m_jit.branch32(RelationalCondition::Equal, rhsLocation.asGPR(), TrustedImm32(-1))
                : m_jit.branch64(RelationalCondition::Equal, rhsLocation.asGPR(), TrustedImm64(-1));
            throwExceptionIf(ExceptionType::IntegerOverflow, isNegativeOne);
        }
        checkedForNegativeOne = true;

        lhsLocation = Location::fromGPR(wasmScratchGPR);
        emitMoveConst(lhs, lhsLocation);
        // Fall through to register/register div.
    }

    if (!checkedForZero) {
        Jump isZero = is32
            ? m_jit.branchTest32(ResultCondition::Zero, rhsLocation.asGPR())
            : m_jit.branchTest64(ResultCondition::Zero, rhsLocation.asGPR());
        throwExceptionIf(ExceptionType::DivisionByZero, isZero);
    }

    ScratchScope<1, 0> scratches(*this, lhsLocation, rhsLocation, resultLocation);
    if (isSigned && !IsMod && !checkedForNegativeOne) {
        // The following code freely clobbers wasmScratchGPR. This would be a bug if either of our operands were
        // stored in wasmScratchGPR, which is the case if one of our operands is a constant - but in that case,
        // we should be able to rule out this check based on the value of that constant above.
        ASSERT(!lhs.isConst());
        ASSERT(!rhs.isConst());
        ASSERT(lhsLocation.asGPR() != wasmScratchGPR);
        ASSERT(rhsLocation.asGPR() != wasmScratchGPR);

        if constexpr (is32)
            m_jit.compare32(RelationalCondition::Equal, rhsLocation.asGPR(), TrustedImm32(-1), wasmScratchGPR);
        else
            m_jit.compare64(RelationalCondition::Equal, rhsLocation.asGPR(), TrustedImm32(-1), wasmScratchGPR);
        if constexpr (is32)
            m_jit.compare32(RelationalCondition::Equal, lhsLocation.asGPR(), TrustedImm32(std::numeric_limits<int32_t>::min()), scratches.gpr(0));
        else {
            m_jit.move(TrustedImm64(std::numeric_limits<int64_t>::min()), scratches.gpr(0));
            m_jit.compare64(RelationalCondition::Equal, lhsLocation.asGPR(), scratches.gpr(0), scratches.gpr(0));
        }
        m_jit.and64(wasmScratchGPR, scratches.gpr(0), wasmScratchGPR);
        Jump isNegativeOne = m_jit.branchTest64(ResultCondition::NonZero, wasmScratchGPR);
        throwExceptionIf(ExceptionType::IntegerOverflow, isNegativeOne);
    }

    GPRReg divResult = IsMod ? scratches.gpr(0) : resultLocation.asGPR();
    if (is32 && isSigned)
        m_jit.div32(lhsLocation.asGPR(), rhsLocation.asGPR(), divResult);
    else if (is32)
        m_jit.uDiv32(lhsLocation.asGPR(), rhsLocation.asGPR(), divResult);
    else if (isSigned)
        m_jit.div64(lhsLocation.asGPR(), rhsLocation.asGPR(), divResult);
    else
        m_jit.uDiv64(lhsLocation.asGPR(), rhsLocation.asGPR(), divResult);

    if (IsMod) {
        if (is32)
            m_jit.multiplySub32(divResult, rhsLocation.asGPR(), lhsLocation.asGPR(), resultLocation.asGPR());
        else
            m_jit.multiplySub64(divResult, rhsLocation.asGPR(), lhsLocation.asGPR(), resultLocation.asGPR());
    }
}
#endif

#if CPU(X86_64)
#define PREPARE_FOR_SHIFT \
    do { \
        clobber(shiftRCX); \
    } while (false); \
    ScratchScope<0, 0> scratches(*this, Location::fromGPR(shiftRCX))
#else
#define PREPARE_FOR_SHIFT
#endif

template<size_t N, typename OverflowHandler>
void BBQJIT::emitShuffleMove(Vector<Value, N, OverflowHandler>& srcVector, Vector<Location, N, OverflowHandler>& dstVector, Vector<ShuffleStatus, N, OverflowHandler>& statusVector, unsigned index)
{
    Location srcLocation = locationOf(srcVector[index]);
    Location dst = dstVector[index];
    if (srcLocation == dst)
        return; // Easily eliminate redundant moves here.

    statusVector[index] = ShuffleStatus::BeingMoved;
    for (unsigned i = 0; i < srcVector.size(); i ++) {
        // This check should handle constants too - constants always have location None, and no
        // dst should ever be a constant. But we assume that's asserted in the caller.
        if (locationOf(srcVector[i]) == dst) {
            switch (statusVector[i]) {
            case ShuffleStatus::ToMove:
                emitShuffleMove(srcVector, dstVector, statusVector, i);
                break;
            case ShuffleStatus::BeingMoved: {
                Location temp = srcVector[i].isFloat() ? Location::fromFPR(wasmScratchFPR) : Location::fromGPR(wasmScratchGPR);
                emitMove(srcVector[i], temp);
                srcVector[i] = Value::pinned(srcVector[i].type(), temp);
                break;
            }
            case ShuffleStatus::Moved:
                break;
            }
        }
    }
    emitMove(srcVector[index], dst);
    statusVector[index] = ShuffleStatus::Moved;
}

template<typename Func, size_t N>
void BBQJIT::emitCCall(Func function, const Vector<Value, N>& arguments)
{
    // Currently, we assume the Wasm calling convention is the same as the C calling convention
    Vector<Type, 16> resultTypes;
    auto argumentTypes = WTF::map<16>(arguments, [](auto& value) {
        return Type { value.type(), 0u };
    });
    RefPtr<TypeDefinition> functionType = TypeInformation::typeDefinitionForFunction(resultTypes, argumentTypes);
    CallInformation callInfo = wasmCallingConvention().callInformationFor(*functionType, CallRole::Caller);
    Checked<int32_t> calleeStackSize = WTF::roundUpToMultipleOf(stackAlignmentBytes(), callInfo.headerAndArgumentStackSizeInBytes);
    m_maxCalleeStackSize = std::max<int>(calleeStackSize, m_maxCalleeStackSize);

    // Prepare wasm operation calls.
    m_jit.prepareWasmCallOperation(GPRInfo::wasmContextInstancePointer);

    // Preserve caller-saved registers and other info
    prepareForExceptions();
    saveValuesAcrossCallAndPassArguments(arguments, callInfo, *functionType);

    // Materialize address of native function and call register
    void* taggedFunctionPtr = tagCFunctionPtr<void*, OperationPtrTag>(function);
    m_jit.move(TrustedImmPtr(bitwise_cast<uintptr_t>(taggedFunctionPtr)), wasmScratchGPR);
    m_jit.call(wasmScratchGPR, OperationPtrTag);
}

template<typename Func, size_t N>
void BBQJIT::emitCCall(Func function, const Vector<Value, N>& arguments, Value& result)
{
    ASSERT(result.isTemp());

    // Currently, we assume the Wasm calling convention is the same as the C calling convention
    Vector<Type, 16> resultTypes = { Type { result.type(), 0u } };
    auto argumentTypes = WTF::map<16>(arguments, [](auto& value) {
        return Type { value.type(), 0u };
    });

    RefPtr<TypeDefinition> functionType = TypeInformation::typeDefinitionForFunction(resultTypes, argumentTypes);
    CallInformation callInfo = wasmCallingConvention().callInformationFor(*functionType, CallRole::Caller);
    Checked<int32_t> calleeStackSize = WTF::roundUpToMultipleOf(stackAlignmentBytes(), callInfo.headerAndArgumentStackSizeInBytes);
    m_maxCalleeStackSize = std::max<int>(calleeStackSize, m_maxCalleeStackSize);

    // Prepare wasm operation calls.
    m_jit.prepareWasmCallOperation(GPRInfo::wasmContextInstancePointer);

    // Preserve caller-saved registers and other info
    prepareForExceptions();
    saveValuesAcrossCallAndPassArguments(arguments, callInfo, *functionType);

    // Materialize address of native function and call register
    void* taggedFunctionPtr = tagCFunctionPtr<void*, OperationPtrTag>(function);
    m_jit.move(TrustedImmPtr(bitwise_cast<uintptr_t>(taggedFunctionPtr)), wasmScratchGPR);
    m_jit.call(wasmScratchGPR, OperationPtrTag);

    Location resultLocation;
    switch (result.type()) {
    case TypeKind::I32:
    case TypeKind::I31ref:
    case TypeKind::I64:
    case TypeKind::Ref:
    case TypeKind::RefNull:
    case TypeKind::Arrayref:
    case TypeKind::Structref:
    case TypeKind::Funcref:
    case TypeKind::Externref:
    case TypeKind::Eqref:
    case TypeKind::Anyref:
    case TypeKind::Nullref:
    case TypeKind::Nullfuncref:
    case TypeKind::Nullexternref:
    case TypeKind::Rec:
    case TypeKind::Sub:
    case TypeKind::Subfinal:
    case TypeKind::Array:
    case TypeKind::Struct:
    case TypeKind::Func: {
        resultLocation = Location::fromGPR(GPRInfo::returnValueGPR);
        ASSERT(m_validGPRs.contains(GPRInfo::returnValueGPR, IgnoreVectors));
        break;
    }
    case TypeKind::F32:
    case TypeKind::F64: {
        resultLocation = Location::fromFPR(FPRInfo::returnValueFPR);
        ASSERT(m_validFPRs.contains(FPRInfo::returnValueFPR, Width::Width128));
        break;
    }
    case TypeKind::V128: {
        resultLocation = Location::fromFPR(FPRInfo::returnValueFPR);
        ASSERT(m_validFPRs.contains(FPRInfo::returnValueFPR, Width::Width128));
        break;
    }
    case TypeKind::Void:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }

    RegisterBinding currentBinding;
    if (resultLocation.isGPR())
        currentBinding = m_gprBindings[resultLocation.asGPR()];
    else if (resultLocation.isFPR())
        currentBinding = m_fprBindings[resultLocation.asFPR()];
    RELEASE_ASSERT(!currentBinding.isScratch());

    bind(result, resultLocation);
}

} } } // namespace JSC::Wasm::BBQJITImpl

#endif // USE(JSVALUE64)
#endif // ENABLE(WEBASSEMBLY_OMGJIT)
