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

    // Load string length to regT2, and start the process of loading the data pointer into regT0
    jit.load32(MacroAssembler::Address(SpecializedThunkJIT::regT0, ThunkHelpers::jsStringLengthOffset()), SpecializedThunkJIT::regT2);
    jit.loadPtr(MacroAssembler::Address(SpecializedThunkJIT::regT0, ThunkHelpers::jsStringValueOffset()), SpecializedThunkJIT::regT0);
    jit.appendFailure(jit.branchTest32(MacroAssembler::Zero, SpecializedThunkJIT::regT0));

    // load index
    jit.loadInt32Argument(0, SpecializedThunkJIT::regT1); // regT1 contains the index

    // Do an unsigned compare to simultaneously filter negative indices as well as indices that are too large
    jit.appendFailure(jit.branch32(MacroAssembler::AboveOrEqual, SpecializedThunkJIT::regT1, SpecializedThunkJIT::regT2));

    // Load the character
    SpecializedThunkJIT::JumpList is16Bit;
    SpecializedThunkJIT::JumpList cont8Bit;
    // Load the string flags
    jit.loadPtr(MacroAssembler::Address(SpecializedThunkJIT::regT0, ThunkHelpers::stringImplFlagsOffset()), SpecializedThunkJIT::regT2);
    jit.loadPtr(MacroAssembler::Address(SpecializedThunkJIT::regT0, ThunkHelpers::stringImplDataOffset()), SpecializedThunkJIT::regT0);
    is16Bit.append(jit.branchTest32(MacroAssembler::Zero, SpecializedThunkJIT::regT2, MacroAssembler::TrustedImm32(ThunkHelpers::stringImpl8BitFlag())));
    jit.load8(MacroAssembler::BaseIndex(SpecializedThunkJIT::regT0, SpecializedThunkJIT::regT1, MacroAssembler::TimesOne, 0), SpecializedThunkJIT::regT0);
    cont8Bit.append(jit.jump());
    is16Bit.link(&jit);
    jit.load16(MacroAssembler::BaseIndex(SpecializedThunkJIT::regT0, SpecializedThunkJIT::regT1, MacroAssembler::TimesTwo, 0), SpecializedThunkJIT::regT0);
    cont8Bit.link(&jit);
}

static void charToString(SpecializedThunkJIT& jit, JSGlobalData* globalData, MacroAssembler::RegisterID src, MacroAssembler::RegisterID dst, MacroAssembler::RegisterID scratch)
{
    jit.appendFailure(jit.branch32(MacroAssembler::AboveOrEqual, src, MacroAssembler::TrustedImm32(0x100)));
    jit.move(MacroAssembler::TrustedImmPtr(globalData->smallStrings.singleCharacterStrings()), scratch);
    jit.loadPtr(MacroAssembler::BaseIndex(scratch, src, MacroAssembler::ScalePtr, 0), dst);
    jit.appendFailure(jit.branchTestPtr(MacroAssembler::Zero, dst));
}

MacroAssemblerCodeRef charCodeAtThunkGenerator(JSGlobalData* globalData)
{
    SpecializedThunkJIT jit(1, globalData);
    stringCharLoad(jit);
    jit.returnInt32(SpecializedThunkJIT::regT0);
    return jit.finalize(*globalData, globalData->jitStubs->ctiNativeCall());
}

MacroAssemblerCodeRef charAtThunkGenerator(JSGlobalData* globalData)
{
    SpecializedThunkJIT jit(1, globalData);
    stringCharLoad(jit);
    charToString(jit, globalData, SpecializedThunkJIT::regT0, SpecializedThunkJIT::regT0, SpecializedThunkJIT::regT1);
    jit.returnJSCell(SpecializedThunkJIT::regT0);
    return jit.finalize(*globalData, globalData->jitStubs->ctiNativeCall());
}

MacroAssemblerCodeRef fromCharCodeThunkGenerator(JSGlobalData* globalData)
{
    SpecializedThunkJIT jit(1, globalData);
    // load char code
    jit.loadInt32Argument(0, SpecializedThunkJIT::regT0);
    charToString(jit, globalData, SpecializedThunkJIT::regT0, SpecializedThunkJIT::regT0, SpecializedThunkJIT::regT1);
    jit.returnJSCell(SpecializedThunkJIT::regT0);
    return jit.finalize(*globalData, globalData->jitStubs->ctiNativeCall());
}

MacroAssemblerCodeRef sqrtThunkGenerator(JSGlobalData* globalData)
{
    SpecializedThunkJIT jit(1, globalData);
    if (!jit.supportsFloatingPointSqrt())
        return MacroAssemblerCodeRef::createSelfManagedCodeRef(globalData->jitStubs->ctiNativeCall());

    jit.loadDoubleArgument(0, SpecializedThunkJIT::fpRegT0, SpecializedThunkJIT::regT0);
    jit.sqrtDouble(SpecializedThunkJIT::fpRegT0, SpecializedThunkJIT::fpRegT0);
    jit.returnDouble(SpecializedThunkJIT::fpRegT0);
    return jit.finalize(*globalData, globalData->jitStubs->ctiNativeCall());
}

#if OS(DARWIN) || (OS(WINDOWS) && CPU(X86))
#define SYMBOL_STRING(name) "_" #name
#else
#define SYMBOL_STRING(name) #name
#endif
    
#if (OS(LINUX) || OS(FREEBSD)) && CPU(X86_64)
#define SYMBOL_STRING_RELOCATION(name) #name "@plt"
#elif OS(DARWIN) || (CPU(X86_64) && COMPILER(MINGW) && !GCC_VERSION_AT_LEAST(4, 5, 0))
#define SYMBOL_STRING_RELOCATION(name) "_" #name
#elif CPU(X86) && COMPILER(MINGW)
#define SYMBOL_STRING_RELOCATION(name) "@" #name "@4"
#else
#define SYMBOL_STRING_RELOCATION(name) #name
#endif

#define UnaryDoubleOpWrapper(function) function##Wrapper
enum MathThunkCallingConvention { };
typedef MathThunkCallingConvention(*MathThunk)(MathThunkCallingConvention);
extern "C" {

double jsRound(double);
double jsRound(double d)
{
    double integer = ceil(d);
    return integer - (integer - d > 0.5);
}

}
    
#if CPU(X86_64) && COMPILER(GCC) && (PLATFORM(MAC) || OS(LINUX))

#define defineUnaryDoubleOpWrapper(function) \
    asm( \
        ".text\n" \
        ".globl " SYMBOL_STRING(function##Thunk) "\n" \
        SYMBOL_STRING(function##Thunk) ":" "\n" \
        "call " SYMBOL_STRING_RELOCATION(function) "\n" \
        "ret\n" \
    );\
    extern "C" { \
        MathThunkCallingConvention function##Thunk(MathThunkCallingConvention); \
    } \
    static MathThunk UnaryDoubleOpWrapper(function) = &function##Thunk;

#elif CPU(X86) && COMPILER(GCC) && (PLATFORM(MAC) || OS(LINUX))
#define defineUnaryDoubleOpWrapper(function) \
    asm( \
        ".text\n" \
        ".globl " SYMBOL_STRING(function##Thunk) "\n" \
        SYMBOL_STRING(function##Thunk) ":" "\n" \
        "subl $8, %esp\n" \
        "movsd %xmm0, (%esp) \n" \
        "call " SYMBOL_STRING_RELOCATION(function) "\n" \
        "fstpl (%esp) \n" \
        "movsd (%esp), %xmm0 \n" \
        "addl $8, %esp\n" \
        "ret\n" \
    );\
    extern "C" { \
        MathThunkCallingConvention function##Thunk(MathThunkCallingConvention); \
    } \
    static MathThunk UnaryDoubleOpWrapper(function) = &function##Thunk;

#else

#define defineUnaryDoubleOpWrapper(function) \
    static MathThunk UnaryDoubleOpWrapper(function) = 0
#endif

defineUnaryDoubleOpWrapper(jsRound);
defineUnaryDoubleOpWrapper(exp);
defineUnaryDoubleOpWrapper(log);
defineUnaryDoubleOpWrapper(floor);
defineUnaryDoubleOpWrapper(ceil);

MacroAssemblerCodeRef floorThunkGenerator(JSGlobalData* globalData)
{
    SpecializedThunkJIT jit(1, globalData);
    MacroAssembler::Jump nonIntJump;
    if (!UnaryDoubleOpWrapper(floor) || !jit.supportsFloatingPoint())
        return MacroAssemblerCodeRef::createSelfManagedCodeRef(globalData->jitStubs->ctiNativeCall());
    jit.loadInt32Argument(0, SpecializedThunkJIT::regT0, nonIntJump);
    jit.returnInt32(SpecializedThunkJIT::regT0);
    nonIntJump.link(&jit);
    jit.loadDoubleArgument(0, SpecializedThunkJIT::fpRegT0, SpecializedThunkJIT::regT0);
    jit.callDoubleToDouble(UnaryDoubleOpWrapper(floor));
    SpecializedThunkJIT::JumpList doubleResult;
    jit.branchConvertDoubleToInt32(SpecializedThunkJIT::fpRegT0, SpecializedThunkJIT::regT0, doubleResult, SpecializedThunkJIT::fpRegT1);
    jit.returnInt32(SpecializedThunkJIT::regT0);
    doubleResult.link(&jit);
    jit.returnDouble(SpecializedThunkJIT::fpRegT0);
    return jit.finalize(*globalData, globalData->jitStubs->ctiNativeCall());
}

MacroAssemblerCodeRef ceilThunkGenerator(JSGlobalData* globalData)
{
    SpecializedThunkJIT jit(1, globalData);
    if (!UnaryDoubleOpWrapper(ceil) || !jit.supportsFloatingPoint())
        return MacroAssemblerCodeRef::createSelfManagedCodeRef(globalData->jitStubs->ctiNativeCall());
    MacroAssembler::Jump nonIntJump;
    jit.loadInt32Argument(0, SpecializedThunkJIT::regT0, nonIntJump);
    jit.returnInt32(SpecializedThunkJIT::regT0);
    nonIntJump.link(&jit);
    jit.loadDoubleArgument(0, SpecializedThunkJIT::fpRegT0, SpecializedThunkJIT::regT0);
    jit.callDoubleToDouble(UnaryDoubleOpWrapper(ceil));
    SpecializedThunkJIT::JumpList doubleResult;
    jit.branchConvertDoubleToInt32(SpecializedThunkJIT::fpRegT0, SpecializedThunkJIT::regT0, doubleResult, SpecializedThunkJIT::fpRegT1);
    jit.returnInt32(SpecializedThunkJIT::regT0);
    doubleResult.link(&jit);
    jit.returnDouble(SpecializedThunkJIT::fpRegT0);
    return jit.finalize(*globalData, globalData->jitStubs->ctiNativeCall());
}

static const double negativeZeroConstant = -0.0;
static const double oneConstant = 1.0;
static const double negativeHalfConstant = -0.5;
    
MacroAssemblerCodeRef roundThunkGenerator(JSGlobalData* globalData)
{
    SpecializedThunkJIT jit(1, globalData);
    if (!UnaryDoubleOpWrapper(jsRound) || !jit.supportsFloatingPoint())
        return MacroAssemblerCodeRef::createSelfManagedCodeRef(globalData->jitStubs->ctiNativeCall());
    MacroAssembler::Jump nonIntJump;
    jit.loadInt32Argument(0, SpecializedThunkJIT::regT0, nonIntJump);
    jit.returnInt32(SpecializedThunkJIT::regT0);
    nonIntJump.link(&jit);
    jit.loadDoubleArgument(0, SpecializedThunkJIT::fpRegT0, SpecializedThunkJIT::regT0);
    jit.callDoubleToDouble(UnaryDoubleOpWrapper(jsRound));
    SpecializedThunkJIT::JumpList doubleResult;
    jit.branchConvertDoubleToInt32(SpecializedThunkJIT::fpRegT0, SpecializedThunkJIT::regT0, doubleResult, SpecializedThunkJIT::fpRegT1);
    jit.returnInt32(SpecializedThunkJIT::regT0);
    doubleResult.link(&jit);
    jit.returnDouble(SpecializedThunkJIT::fpRegT0);
    return jit.finalize(*globalData, globalData->jitStubs->ctiNativeCall());
}

MacroAssemblerCodeRef expThunkGenerator(JSGlobalData* globalData)
{
    if (!UnaryDoubleOpWrapper(exp))
        return MacroAssemblerCodeRef::createSelfManagedCodeRef(globalData->jitStubs->ctiNativeCall());
    SpecializedThunkJIT jit(1, globalData);
    if (!jit.supportsFloatingPoint())
        return MacroAssemblerCodeRef::createSelfManagedCodeRef(globalData->jitStubs->ctiNativeCall());
    jit.loadDoubleArgument(0, SpecializedThunkJIT::fpRegT0, SpecializedThunkJIT::regT0);
    jit.callDoubleToDouble(UnaryDoubleOpWrapper(exp));
    jit.returnDouble(SpecializedThunkJIT::fpRegT0);
    return jit.finalize(*globalData, globalData->jitStubs->ctiNativeCall());
}

MacroAssemblerCodeRef logThunkGenerator(JSGlobalData* globalData)
{
    if (!UnaryDoubleOpWrapper(log))
        return MacroAssemblerCodeRef::createSelfManagedCodeRef(globalData->jitStubs->ctiNativeCall());
    SpecializedThunkJIT jit(1, globalData);
    if (!jit.supportsFloatingPoint())
        return MacroAssemblerCodeRef::createSelfManagedCodeRef(globalData->jitStubs->ctiNativeCall());
    jit.loadDoubleArgument(0, SpecializedThunkJIT::fpRegT0, SpecializedThunkJIT::regT0);
    jit.callDoubleToDouble(UnaryDoubleOpWrapper(log));
    jit.returnDouble(SpecializedThunkJIT::fpRegT0);
    return jit.finalize(*globalData, globalData->jitStubs->ctiNativeCall());
}

MacroAssemblerCodeRef absThunkGenerator(JSGlobalData* globalData)
{
    SpecializedThunkJIT jit(1, globalData);
    if (!jit.supportsFloatingPointAbs())
        return MacroAssemblerCodeRef::createSelfManagedCodeRef(globalData->jitStubs->ctiNativeCall());
    MacroAssembler::Jump nonIntJump;
    jit.loadInt32Argument(0, SpecializedThunkJIT::regT0, nonIntJump);
    jit.rshift32(SpecializedThunkJIT::regT0, MacroAssembler::TrustedImm32(31), SpecializedThunkJIT::regT1);
    jit.add32(SpecializedThunkJIT::regT1, SpecializedThunkJIT::regT0);
    jit.xor32(SpecializedThunkJIT::regT1, SpecializedThunkJIT::regT0);
    jit.appendFailure(jit.branch32(MacroAssembler::Equal, SpecializedThunkJIT::regT0, MacroAssembler::TrustedImm32(1 << 31)));
    jit.returnInt32(SpecializedThunkJIT::regT0);
    nonIntJump.link(&jit);
    // Shame about the double int conversion here.
    jit.loadDoubleArgument(0, SpecializedThunkJIT::fpRegT0, SpecializedThunkJIT::regT0);
    jit.absDouble(SpecializedThunkJIT::fpRegT0, SpecializedThunkJIT::fpRegT1);
    jit.returnDouble(SpecializedThunkJIT::fpRegT1);
    return jit.finalize(*globalData, globalData->jitStubs->ctiNativeCall());
}

MacroAssemblerCodeRef powThunkGenerator(JSGlobalData* globalData)
{
    SpecializedThunkJIT jit(2, globalData);
    if (!jit.supportsFloatingPoint())
        return MacroAssemblerCodeRef::createSelfManagedCodeRef(globalData->jitStubs->ctiNativeCall());

    jit.loadDouble(&oneConstant, SpecializedThunkJIT::fpRegT1);
    jit.loadDoubleArgument(0, SpecializedThunkJIT::fpRegT0, SpecializedThunkJIT::regT0);
    MacroAssembler::Jump nonIntExponent;
    jit.loadInt32Argument(1, SpecializedThunkJIT::regT0, nonIntExponent);
    jit.appendFailure(jit.branch32(MacroAssembler::LessThan, SpecializedThunkJIT::regT0, MacroAssembler::TrustedImm32(0)));
    
    MacroAssembler::Jump exponentIsZero = jit.branchTest32(MacroAssembler::Zero, SpecializedThunkJIT::regT0);
    MacroAssembler::Label startLoop(jit.label());

    MacroAssembler::Jump exponentIsEven = jit.branchTest32(MacroAssembler::Zero, SpecializedThunkJIT::regT0, MacroAssembler::TrustedImm32(1));
    jit.mulDouble(SpecializedThunkJIT::fpRegT0, SpecializedThunkJIT::fpRegT1);
    exponentIsEven.link(&jit);
    jit.mulDouble(SpecializedThunkJIT::fpRegT0, SpecializedThunkJIT::fpRegT0);
    jit.rshift32(MacroAssembler::TrustedImm32(1), SpecializedThunkJIT::regT0);
    jit.branchTest32(MacroAssembler::NonZero, SpecializedThunkJIT::regT0).linkTo(startLoop, &jit);

    exponentIsZero.link(&jit);

    {
        SpecializedThunkJIT::JumpList doubleResult;
        jit.branchConvertDoubleToInt32(SpecializedThunkJIT::fpRegT1, SpecializedThunkJIT::regT0, doubleResult, SpecializedThunkJIT::fpRegT0);
        jit.returnInt32(SpecializedThunkJIT::regT0);
        doubleResult.link(&jit);
        jit.returnDouble(SpecializedThunkJIT::fpRegT1);
    }

    if (jit.supportsFloatingPointSqrt()) {
        nonIntExponent.link(&jit);
        jit.loadDouble(&negativeHalfConstant, SpecializedThunkJIT::fpRegT3);
        jit.loadDoubleArgument(1, SpecializedThunkJIT::fpRegT2, SpecializedThunkJIT::regT0);
        jit.appendFailure(jit.branchDouble(MacroAssembler::DoubleLessThanOrEqual, SpecializedThunkJIT::fpRegT0, SpecializedThunkJIT::fpRegT1));
        jit.appendFailure(jit.branchDouble(MacroAssembler::DoubleNotEqualOrUnordered, SpecializedThunkJIT::fpRegT2, SpecializedThunkJIT::fpRegT3));
        jit.sqrtDouble(SpecializedThunkJIT::fpRegT0, SpecializedThunkJIT::fpRegT0);
        jit.divDouble(SpecializedThunkJIT::fpRegT0, SpecializedThunkJIT::fpRegT1);

        SpecializedThunkJIT::JumpList doubleResult;
        jit.branchConvertDoubleToInt32(SpecializedThunkJIT::fpRegT1, SpecializedThunkJIT::regT0, doubleResult, SpecializedThunkJIT::fpRegT0);
        jit.returnInt32(SpecializedThunkJIT::regT0);
        doubleResult.link(&jit);
        jit.returnDouble(SpecializedThunkJIT::fpRegT1);
    } else
        jit.appendFailure(nonIntExponent);

    return jit.finalize(*globalData, globalData->jitStubs->ctiNativeCall());
}

}

#endif // ENABLE(JIT)
