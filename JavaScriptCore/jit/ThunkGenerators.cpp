/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ThunkGenerators.h"

#include "CodeBlock.h"
#include <wtf/text/StringImpl.h>
#include "SpecializedThunkJIT.h"

#if ENABLE(JIT)

namespace JSC {

static void stringCharLoad(SpecializedThunkJIT& jit)
{
    // load string
    jit.loadJSStringArgument(SpecializedThunkJIT::ThisArgument, SpecializedThunkJIT::regT0);
    // regT0 now contains this, and is a non-rope JSString*

    // Load string length to regT2, and start the process of loading the data pointer into regT0
    jit.load32(MacroAssembler::Address(SpecializedThunkJIT::regT0, ThunkHelpers::jsStringLengthOffset()), SpecializedThunkJIT::regT2);
    jit.loadPtr(MacroAssembler::Address(SpecializedThunkJIT::regT0, ThunkHelpers::jsStringValueOffset()), SpecializedThunkJIT::regT0);
    jit.loadPtr(MacroAssembler::Address(SpecializedThunkJIT::regT0, ThunkHelpers::stringImplDataOffset()), SpecializedThunkJIT::regT0);

    // load index
    jit.loadInt32Argument(0, SpecializedThunkJIT::regT1); // regT1 contains the index

    // Do an unsigned compare to simultaneously filter negative indices as well as indices that are too large
    jit.appendFailure(jit.branch32(MacroAssembler::AboveOrEqual, SpecializedThunkJIT::regT1, SpecializedThunkJIT::regT2));

    // Load the character
    jit.load16(MacroAssembler::BaseIndex(SpecializedThunkJIT::regT0, SpecializedThunkJIT::regT1, MacroAssembler::TimesTwo, 0), SpecializedThunkJIT::regT0);
}

static void charToString(SpecializedThunkJIT& jit, JSGlobalData* globalData, MacroAssembler::RegisterID src, MacroAssembler::RegisterID dst, MacroAssembler::RegisterID scratch)
{
    jit.appendFailure(jit.branch32(MacroAssembler::AboveOrEqual, src, MacroAssembler::Imm32(0x100)));
    jit.move(MacroAssembler::ImmPtr(globalData->smallStrings.singleCharacterStrings()), scratch);
    jit.loadPtr(MacroAssembler::BaseIndex(scratch, src, MacroAssembler::ScalePtr, 0), dst);
    jit.appendFailure(jit.branchTestPtr(MacroAssembler::Zero, dst));
}

PassRefPtr<NativeExecutable> charCodeAtThunkGenerator(JSGlobalData* globalData, ExecutablePool* pool)
{
    SpecializedThunkJIT jit(1, globalData, pool);
    stringCharLoad(jit);
    jit.returnInt32(SpecializedThunkJIT::regT0);
    return jit.finalize();
}

PassRefPtr<NativeExecutable> charAtThunkGenerator(JSGlobalData* globalData, ExecutablePool* pool)
{
    SpecializedThunkJIT jit(1, globalData, pool);
    stringCharLoad(jit);
    charToString(jit, globalData, SpecializedThunkJIT::regT0, SpecializedThunkJIT::regT0, SpecializedThunkJIT::regT1);
    jit.returnJSCell(SpecializedThunkJIT::regT0);
    return jit.finalize();
}

PassRefPtr<NativeExecutable> fromCharCodeThunkGenerator(JSGlobalData* globalData, ExecutablePool* pool)
{
    SpecializedThunkJIT jit(1, globalData, pool);
    // load char code
    jit.loadInt32Argument(0, SpecializedThunkJIT::regT0);
    charToString(jit, globalData, SpecializedThunkJIT::regT0, SpecializedThunkJIT::regT0, SpecializedThunkJIT::regT1);
    jit.returnJSCell(SpecializedThunkJIT::regT0);
    return jit.finalize();
}

PassRefPtr<NativeExecutable> sqrtThunkGenerator(JSGlobalData* globalData, ExecutablePool* pool)
{
#if USE(JSVALUE64) || USE(JSVALUE32_64)
    SpecializedThunkJIT jit(1, globalData, pool);
    if (!jit.supportsFloatingPointSqrt())
        return globalData->jitStubs.ctiNativeCallThunk();

    jit.loadDoubleArgument(0, SpecializedThunkJIT::fpRegT0, SpecializedThunkJIT::regT0);
    jit.sqrtDouble(SpecializedThunkJIT::fpRegT0, SpecializedThunkJIT::fpRegT0);
    jit.returnDouble(SpecializedThunkJIT::fpRegT0);
    return jit.finalize();
#else
    UNUSED_PARAM(pool);
    return globalData->jitStubs.ctiNativeCallThunk();
#endif
}

static const double oneConstant = 1.0;
static const double negativeHalfConstant = -0.5;

PassRefPtr<NativeExecutable> powThunkGenerator(JSGlobalData* globalData, ExecutablePool* pool)
{
#if USE(JSVALUE64) || USE(JSVALUE32_64)
    SpecializedThunkJIT jit(2, globalData, pool);
    if (!jit.supportsFloatingPoint())
        return globalData->jitStubs.ctiNativeCallThunk();

    jit.loadDouble(&oneConstant, SpecializedThunkJIT::fpRegT1);
    jit.loadDoubleArgument(0, SpecializedThunkJIT::fpRegT0, SpecializedThunkJIT::regT0);
    MacroAssembler::Jump nonIntExponent;
    jit.loadInt32Argument(1, SpecializedThunkJIT::regT0, nonIntExponent);
    jit.appendFailure(jit.branch32(MacroAssembler::LessThan, SpecializedThunkJIT::regT0, MacroAssembler::Imm32(0)));
    
    MacroAssembler::Jump exponentIsZero = jit.branchTest32(MacroAssembler::Zero, SpecializedThunkJIT::regT0);
    MacroAssembler::Label startLoop(jit.label());

    MacroAssembler::Jump exponentIsEven = jit.branchTest32(MacroAssembler::Zero, SpecializedThunkJIT::regT0, MacroAssembler::Imm32(1));
    jit.mulDouble(SpecializedThunkJIT::fpRegT0, SpecializedThunkJIT::fpRegT1);
    exponentIsEven.link(&jit);
    jit.mulDouble(SpecializedThunkJIT::fpRegT0, SpecializedThunkJIT::fpRegT0);
    jit.rshift32(MacroAssembler::Imm32(1), SpecializedThunkJIT::regT0);
    jit.branchTest32(MacroAssembler::NonZero, SpecializedThunkJIT::regT0).linkTo(startLoop, &jit);

    exponentIsZero.link(&jit);
    jit.returnDouble(SpecializedThunkJIT::fpRegT1);

    if (jit.supportsFloatingPointSqrt()) {
        nonIntExponent.link(&jit);
        jit.loadDouble(&negativeHalfConstant, SpecializedThunkJIT::fpRegT3);
        jit.loadDoubleArgument(1, SpecializedThunkJIT::fpRegT2, SpecializedThunkJIT::regT0);
        jit.appendFailure(jit.branchDouble(MacroAssembler::DoubleLessThanOrEqual, SpecializedThunkJIT::fpRegT0, SpecializedThunkJIT::fpRegT1));
        jit.appendFailure(jit.branchDouble(MacroAssembler::DoubleNotEqualOrUnordered, SpecializedThunkJIT::fpRegT2, SpecializedThunkJIT::fpRegT3));
        jit.sqrtDouble(SpecializedThunkJIT::fpRegT0, SpecializedThunkJIT::fpRegT0);
        jit.divDouble(SpecializedThunkJIT::fpRegT0, SpecializedThunkJIT::fpRegT1);
        jit.returnDouble(SpecializedThunkJIT::fpRegT1);
    } else
        jit.appendFailure(nonIntExponent);

    return jit.finalize();
#else
    UNUSED_PARAM(pool);
    return globalData->jitStubs.ctiNativeCallThunk();
#endif
}

}

#endif // ENABLE(JIT)
