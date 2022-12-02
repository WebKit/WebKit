/*
 * Copyright (C) 2015-2021 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(JIT)

#include "CCallHelpers.h"
#include "IntrinsicGetterAccessCase.h"
#include "JSArrayBufferView.h"
#include "JSCJSValueInlines.h"
#include "JSTypedArrays.h"
#include "PolymorphicAccess.h"
#include "StructureStubInfo.h"

namespace JSC {

typedef CCallHelpers::TrustedImm32 TrustedImm32;
typedef CCallHelpers::Imm32 Imm32;
typedef CCallHelpers::TrustedImmPtr TrustedImmPtr;
typedef CCallHelpers::ImmPtr ImmPtr;
typedef CCallHelpers::TrustedImm64 TrustedImm64;
typedef CCallHelpers::Imm64 Imm64;

bool IntrinsicGetterAccessCase::canEmitIntrinsicGetter(StructureStubInfo& stubInfo, JSFunction* getter, Structure* structure)
{
    // We aren't structure checking the this value, so we don't know:
    // - For type array loads, that it's a typed array.
    // - For __proto__ getter, that the incoming value is an object,
    //   and if it overrides getPrototype structure flags.
    // So for these cases, it's simpler to just call the getter directly.
    if (stubInfo.thisValueIsInExtraGPR())
        return false;

    switch (getter->intrinsic()) {
    case TypedArrayByteOffsetIntrinsic:
    case TypedArrayByteLengthIntrinsic:
    case TypedArrayLengthIntrinsic: {
        if (!isTypedView(structure->typeInfo().type()))
            return false;
#if USE(JSVALUE32_64)
        if (isResizableOrGrowableSharedTypedArrayIncludingDataView(structure->classInfoForCells()))
            return false;
#endif
        return true;
    }
    case UnderscoreProtoIntrinsic: {
        TypeInfo info = structure->typeInfo();
        return info.isObject() && !info.overridesGetPrototype();
    }
    default:
        return false;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

bool IntrinsicGetterAccessCase::doesCalls() const
{
    switch (intrinsic()) {
    case TypedArrayByteOffsetIntrinsic:
    case TypedArrayByteLengthIntrinsic:
    case TypedArrayLengthIntrinsic:
        return isResizableOrGrowableSharedTypedArrayIncludingDataView(structure()->classInfoForCells());
    default:
        return false;
    }
    return false;
}

void IntrinsicGetterAccessCase::emitIntrinsicGetter(AccessGenerationState& state)
{
    CCallHelpers& jit = *state.jit;
    StructureStubInfo& stubInfo = *state.stubInfo;
    JSValueRegs valueRegs = stubInfo.valueRegs();
    GPRReg baseGPR = stubInfo.m_baseGPR;
    GPRReg valueGPR = valueRegs.payloadGPR();

    switch (intrinsic()) {
    case TypedArrayLengthIntrinsic: {
#if USE(JSVALUE64)
        if (isResizableOrGrowableSharedTypedArrayIncludingDataView(structure()->classInfoForCells())) {
            auto allocator = state.makeDefaultScratchAllocator(state.scratchGPR);
            GPRReg scratch2GPR = allocator.allocateScratchGPR();

            ScratchRegisterAllocator::PreservedState preservedState = allocator.preserveReusedRegistersByPushing(jit, ScratchRegisterAllocator::ExtraStackSpace::NoExtraSpace);

            jit.loadTypedArrayLength(baseGPR, valueGPR, state.scratchGPR, scratch2GPR, typedArrayType(structure()->typeInfo().type()));
#if USE(LARGE_TYPED_ARRAYS)
            jit.boxInt52(valueGPR, valueGPR, state.scratchGPR, state.scratchFPR);
#else
            jit.boxInt32(valueGPR, valueRegs);
#endif
            allocator.restoreReusedRegistersByPopping(jit, preservedState);
            state.succeed();
            return;
        }
#endif

#if USE(LARGE_TYPED_ARRAYS)
        jit.load64(MacroAssembler::Address(baseGPR, JSArrayBufferView::offsetOfLength()), valueGPR);
        jit.boxInt52(valueGPR, valueGPR, state.scratchGPR, state.scratchFPR);
#else
        jit.load32(MacroAssembler::Address(baseGPR, JSArrayBufferView::offsetOfLength()), valueGPR);
        jit.boxInt32(valueGPR, valueRegs);
#endif
        state.succeed();
        return;
    }

    case TypedArrayByteLengthIntrinsic: {
        TypedArrayType type = typedArrayType(structure()->typeInfo().type());

#if USE(JSVALUE64)
        if (isResizableOrGrowableSharedTypedArrayIncludingDataView(structure()->classInfoForCells())) {
            auto allocator = state.makeDefaultScratchAllocator(state.scratchGPR);
            GPRReg scratch2GPR = allocator.allocateScratchGPR();

            ScratchRegisterAllocator::PreservedState preservedState = allocator.preserveReusedRegistersByPushing(jit, ScratchRegisterAllocator::ExtraStackSpace::NoExtraSpace);

            jit.loadTypedArrayByteLength(baseGPR, valueGPR, state.scratchGPR, scratch2GPR, typedArrayType(structure()->typeInfo().type()));
#if USE(LARGE_TYPED_ARRAYS)
            jit.boxInt52(valueGPR, valueGPR, state.scratchGPR, state.scratchFPR);
#else
            jit.boxInt32(valueGPR, valueRegs);
#endif
            allocator.restoreReusedRegistersByPopping(jit, preservedState);
            state.succeed();
            return;
        }
#endif

#if USE(LARGE_TYPED_ARRAYS)
        jit.load64(MacroAssembler::Address(baseGPR, JSArrayBufferView::offsetOfLength()), valueGPR);
        if (elementSize(type) > 1)
            jit.lshift64(TrustedImm32(logElementSize(type)), valueGPR);
        jit.boxInt52(valueGPR, valueGPR, state.scratchGPR, state.scratchFPR);
#else
        jit.load32(MacroAssembler::Address(baseGPR, JSArrayBufferView::offsetOfLength()), valueGPR);
        if (elementSize(type) > 1) {
            // We can use a bitshift here since on ADDRESS32 platforms TypedArrays cannot have byteLength that overflows an int32.
            jit.lshift32(TrustedImm32(logElementSize(type)), valueGPR);
        }
        jit.boxInt32(valueGPR, valueRegs);
#endif
        state.succeed();
        return;
    }

    case TypedArrayByteOffsetIntrinsic: {
#if USE(JSVALUE64)
        if (isResizableOrGrowableSharedTypedArrayIncludingDataView(structure()->classInfoForCells())) {
            auto allocator = state.makeDefaultScratchAllocator(state.scratchGPR);
            GPRReg scratch2GPR = allocator.allocateScratchGPR();

            ScratchRegisterAllocator::PreservedState preservedState = allocator.preserveReusedRegistersByPushing(jit, ScratchRegisterAllocator::ExtraStackSpace::NoExtraSpace);
            auto outOfBounds = jit.branchIfResizableOrGrowableSharedTypedArrayIsOutOfBounds(baseGPR, state.scratchGPR, scratch2GPR, typedArrayType(structure()->typeInfo().type()));
#if USE(LARGE_TYPED_ARRAYS)
            jit.load64(CCallHelpers::Address(baseGPR, JSArrayBufferView::offsetOfByteOffset()), valueGPR);
#else
            jit.load32(CCallHelpers::Address(baseGPR, JSArrayBufferView::offsetOfByteOffset()), valueGPR);
#endif
            auto done = jit.jump();

            outOfBounds.link(&jit);
            jit.move(CCallHelpers::TrustedImm32(0), valueGPR);

            done.link(&jit);
#if USE(LARGE_TYPED_ARRAYS)
            jit.boxInt52(valueGPR, valueGPR, state.scratchGPR, state.scratchFPR);
#else
            jit.boxInt32(valueGPR, valueRegs);
#endif
            allocator.restoreReusedRegistersByPopping(jit, preservedState);
            state.succeed();
            return;
        }
#endif

#if USE(LARGE_TYPED_ARRAYS)
        jit.load64(MacroAssembler::Address(baseGPR, JSArrayBufferView::offsetOfByteOffset()), valueGPR);
        jit.boxInt52(valueGPR, valueGPR, state.scratchGPR, state.scratchFPR);
#else
        jit.load32(MacroAssembler::Address(baseGPR, JSArrayBufferView::offsetOfByteOffset()), valueGPR);
        jit.boxInt32(valueGPR, valueRegs);
#endif
        state.succeed();
        return;
    }

    case UnderscoreProtoIntrinsic: {
        if (structure()->hasPolyProto())
            jit.loadValue(CCallHelpers::Address(baseGPR, offsetRelativeToBase(knownPolyProtoOffset)), valueRegs);
        else
            jit.moveValue(structure()->storedPrototype(), valueRegs);
        state.succeed();
        return;
    }

    default:
        break;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

} // namespace JSC

#endif // ENABLE(JIT)
