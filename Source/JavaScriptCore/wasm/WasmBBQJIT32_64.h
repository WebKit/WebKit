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
#if USE(JSVALUE32_64)

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

    ScratchScope<2, 0> globals(*this);
    GPRReg wasmBaseMemoryPointer = globals.gpr(0);
    GPRReg wasmBoundsCheckingSizeRegister = globals.gpr(1);
    loadWebAssemblyGlobalState(wasmBaseMemoryPointer, wasmBoundsCheckingSizeRegister);

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
            // NB: On 32-bit we have to check the addition for overflow
            throwExceptionIf(ExceptionType::OutOfBoundsMemoryAccess, m_jit.branchAdd32(ResultCondition::Carry, wasmScratchGPR, TrustedImm32(boundary), wasmScratchGPR));
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

    m_jit.zeroExtend32ToWord(pointerLocation.asGPR(), wasmScratchGPR);
    m_jit.addPtr(wasmBaseMemoryPointer, wasmScratchGPR);

    if (static_cast<uint64_t>(uoffset) > static_cast<uint64_t>(std::numeric_limits<int32_t>::max()) || !B3::Air::Arg::isValidAddrForm(B3::Air::Move, uoffset, Width::Width128)) {
        m_jit.addPtr(TrustedImmPtr(static_cast<int64_t>(uoffset)), wasmScratchGPR);
        return functor(Address(wasmScratchGPR));
    }
    return functor(Address(wasmScratchGPR, static_cast<int32_t>(uoffset)));
}

#define PREPARE_FOR_MOD_OR_DIV

template<typename IntType, bool IsMod>
void BBQJIT::emitModOrDiv(Value& lhs, Location lhsLocation, Value& rhs, Location rhsLocation, Value& result, Location)
{
    static_assert(sizeof(IntType) == 4 || sizeof(IntType) == 8);

    constexpr bool isSigned = std::is_signed<IntType>();
    constexpr bool is32 = sizeof(IntType) == 4;

    TypeKind argType, returnType;
    if constexpr (is32)
        argType = returnType = TypeKind::I32;
    else
        argType = returnType = TypeKind::I64;

    IntType (*modOrDiv)(IntType, IntType);
    if constexpr (IsMod) {
        if constexpr (is32) {
            if constexpr (isSigned)
                modOrDiv = Math::i32_rem_s;
            else
                modOrDiv = Math::i32_rem_u;
        } else {
            if constexpr (isSigned)
                modOrDiv = Math::i64_rem_s;
            else
                modOrDiv = Math::i64_rem_u;
        }
    } else {
        if constexpr (is32) {
            if constexpr (isSigned)
                modOrDiv = Math::i32_div_s;
            else
                modOrDiv = Math::i32_div_u;
        } else {
            if constexpr (isSigned)
                modOrDiv = Math::i64_div_s;
            else
                modOrDiv = Math::i64_div_u;
        }
    }

    if (lhs.isConst() || rhs.isConst()) {
        auto& constantLocation = ImmHelpers::immLocation(lhsLocation, rhsLocation);
        if constexpr (is32)
            constantLocation = Location::fromGPR(wasmScratchGPR);
        else
            constantLocation = Location::fromGPR2(wasmScratchGPR2, wasmScratchGPR);
        emitMoveConst(ImmHelpers::imm(lhs, rhs), constantLocation);
    }

    bool needsZeroCheck = [&] {
        if constexpr (is32)
            return !(rhs.isConst() && rhs.asI32());
        else
            return !(rhs.isConst() && rhs.asI64());
    }();

    if (needsZeroCheck) {
        if constexpr (is32)
            throwExceptionIf(ExceptionType::DivisionByZero, m_jit.branchTest32(ResultCondition::Zero, rhsLocation.asGPR()));
        else {
            auto loNotZero = m_jit.branchTest32(ResultCondition::NonZero, rhsLocation.asGPRlo());
            throwExceptionIf(ExceptionType::DivisionByZero, m_jit.branchTest32(ResultCondition::Zero, rhsLocation.asGPRhi()));
            loNotZero.link(&m_jit);
        }
    }

    if constexpr (!IsMod) {
        if constexpr (isSigned) {
            bool needsOverflowCheck = [&] {
                if constexpr (is32) {
                    if (rhs.isConst() && rhs.asI32() != -1)
                        return false;
                    if (lhs.isConst() && lhs.asI32() != std::numeric_limits<int32_t>::min())
                        return false;
                } else {
                    if (rhs.isConst() && rhs.asI64() != -1)
                        return false;
                    if (lhs.isConst() && lhs.asI64() != std::numeric_limits<int64_t>::min())
                        return false;
                }

                return true;
            }();

            if (needsOverflowCheck) {
                if constexpr (is32) {
                    auto rhsIsOk = m_jit.branch32(RelationalCondition::NotEqual, rhsLocation.asGPR(), TrustedImm32(-1));
                    throwExceptionIf(ExceptionType::IntegerOverflow, m_jit.branch32(RelationalCondition::Equal, lhsLocation.asGPR(), TrustedImm32(std::numeric_limits<int32_t>::min())));
                    rhsIsOk.link(&m_jit);
                } else {
                    auto rhsLoIsOk = m_jit.branch32(RelationalCondition::NotEqual, rhsLocation.asGPRlo(), TrustedImm32(-1));
                    auto rhsHiIsOk = m_jit.branch32(RelationalCondition::NotEqual, rhsLocation.asGPRhi(), TrustedImm32(-1));

                    auto lhsLoIsOk = m_jit.branchTest32(ResultCondition::NonZero, lhsLocation.asGPRlo());
                    throwExceptionIf(ExceptionType::IntegerOverflow, m_jit.branch32(RelationalCondition::Equal, lhsLocation.asGPRhi(), TrustedImm32(std::numeric_limits<int32_t>::min())));

                    rhsLoIsOk.link(&m_jit);
                    rhsHiIsOk.link(&m_jit);
                    lhsLoIsOk.link(&m_jit);
                }
            }
        }
    }

    auto lhsArg = Value::pinned(argType, lhsLocation);
    auto rhsArg = Value::pinned(argType, rhsLocation);
    consume(result);
    emitCCall(modOrDiv, Vector<Value> { lhsArg, rhsArg }, result);
}

#define PREPARE_FOR_SHIFT

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
        auto aliasesDst = [&] (Location loc) -> bool {
            if (loc.isGPR()) {
                if (dst.isGPR2())
                    return loc.asGPR() == dst.asGPRhi() || loc.asGPR() == dst.asGPRlo();
            } else if (loc.isGPR2()) {
                if (dst.isGPR())
                    return loc.asGPRhi() == dst.asGPR() || loc.asGPRlo() == dst.asGPR();
                if (dst.isGPR2()) {
                    return loc.asGPRhi() == dst.asGPRhi() || loc.asGPRhi() == dst.asGPRlo()
                        || loc.asGPRlo() == dst.asGPRhi() || loc.asGPRlo() == dst.asGPRlo();
                }
            }
            return loc == dst;
        };
        if (aliasesDst(locationOf(srcVector[i]))) {
            switch (statusVector[i]) {
            case ShuffleStatus::ToMove:
                emitShuffleMove(srcVector, dstVector, statusVector, i);
                break;
            case ShuffleStatus::BeingMoved: {
                Location temp;
                if (srcVector[i].isFloat())
                    temp = Location::fromFPR(wasmScratchFPR);
                else if (typeNeedsGPR2(srcVector[i].type()))
                    temp = Location::fromGPR2(wasmScratchGPR2, wasmScratchGPR);
                else
                    temp = Location::fromGPR(wasmScratchGPR);
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
    CallInformation callInfo = cCallingConventionArmThumb2().callInformationFor(*functionType, CallRole::Caller);
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
    CallInformation callInfo = cCallingConventionArmThumb2().callInformationFor(*functionType, CallRole::Caller);
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
        resultLocation = Location::fromGPR(GPRInfo::returnValueGPR);
        break;
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
        resultLocation = Location::fromGPR2(GPRInfo::returnValueGPR2, GPRInfo::returnValueGPR);
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
    else if (resultLocation.isGPR2())
        currentBinding = m_gprBindings[resultLocation.asGPRhi()];
    RELEASE_ASSERT(!currentBinding.isScratch());

    bind(result, resultLocation);
}

} } } // namespace JSC::Wasm::BBQJITImpl

#endif // USE(JSVALUE32_64)
#endif // ENABLE(WEBASSEMBLY_OMGJIT)
