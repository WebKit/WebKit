/*
 * Copyright (C) 2015-2019 Apple Inc. All rights reserved.
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

#include "config.h"
#include "testb3.h"

#if ENABLE(B3_JIT)

Lock crashLock;

// Make sure the compiler does not try to optimize anything out.
static NEVER_INLINE double zero()
{
    return 0.;
}

static double negativeZero()
{
    return -zero();
}

bool shouldRun(const char* filter, const char* testName)
{
    // FIXME: These tests fail <https://bugs.webkit.org/show_bug.cgi?id=199330>.
    if (!filter && isARM64()) {
        for (auto& failingTest : {
            "testReportUsedRegistersLateUseFollowedByEarlyDefDoesNotMarkUseAsDead",
            "testNegFloatWithUselessDoubleConversion",
            "testPinRegisters",
        }) {
            if (WTF::findIgnoringASCIICaseWithoutLength(testName, failingTest) != WTF::notFound) {
                dataLogLn("*** Warning: Skipping known-bad test: ", testName);
                return false;
            }
        }
    }
    if (!filter && isX86()) {
        for (auto& failingTest : {
            "testReportUsedRegistersLateUseFollowedByEarlyDefDoesNotMarkUseAsDead",
        }) {
            if (WTF::findIgnoringASCIICaseWithoutLength(testName, failingTest) != WTF::notFound) {
                dataLogLn("*** Warning: Skipping known-bad test: ", testName);
                return false;
            }
        }
    }
    return !filter || WTF::findIgnoringASCIICaseWithoutLength(testName, filter) != WTF::notFound;
}

template<typename T>
void testRotR(T valueInt, int32_t shift)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    
    Value* value = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    if (sizeof(T) == 4)
        value = root->appendNew<Value>(proc, Trunc, Origin(), value);
    
    Value* ammount = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1));
    root->appendNewControlValue(proc, Return, Origin(),
        root->appendNew<Value>(proc, RotR, Origin(), value, ammount));
    
    CHECK_EQ(compileAndRun<T>(proc, valueInt, shift), rotateRight(valueInt, shift));
}

template<typename T>
void testRotL(T valueInt, int32_t shift)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    
    Value* value = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    if (sizeof(T) == 4)
        value = root->appendNew<Value>(proc, Trunc, Origin(), value);
    
    Value* ammount = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1));
    root->appendNewControlValue(proc, Return, Origin(),
        root->appendNew<Value>(proc, RotL, Origin(), value, ammount));
    
    CHECK_EQ(compileAndRun<T>(proc, valueInt, shift), rotateLeft(valueInt, shift));
}


template<typename T>
void testRotRWithImmShift(T valueInt, int32_t shift)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    
    Value* value = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    if (sizeof(T) == 4)
        value = root->appendNew<Value>(proc, Trunc, Origin(), value);
    
    Value* ammount = root->appendIntConstant(proc, Origin(), Int32, shift);
    root->appendNewControlValue(proc, Return, Origin(),
        root->appendNew<Value>(proc, RotR, Origin(), value, ammount));
    
    CHECK_EQ(compileAndRun<T>(proc, valueInt, shift), rotateRight(valueInt, shift));
}

template<typename T>
void testRotLWithImmShift(T valueInt, int32_t shift)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    
    Value* value = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    if (sizeof(T) == 4)
        value = root->appendNew<Value>(proc, Trunc, Origin(), value);
    
    Value* ammount = root->appendIntConstant(proc, Origin(), Int32, shift);
    root->appendNewControlValue(proc, Return, Origin(),
        root->appendNew<Value>(proc, RotL, Origin(), value, ammount));
    
    CHECK_EQ(compileAndRun<T>(proc, valueInt, shift), rotateLeft(valueInt, shift));
}

template<typename T>
void testComputeDivisionMagic(T value, T magicMultiplier, unsigned shift)
{
    DivisionMagic<T> magic = computeDivisionMagic(value);
    CHECK(magic.magicMultiplier == magicMultiplier);
    CHECK(magic.shift == shift);
}

void run(const char* filter)
{
    Deque<RefPtr<SharedTask<void()>>> tasks;

    RUN_NOW(testTerminalPatchpointThatNeedsToBeSpilled2());
    RUN(test42());
    RUN(testLoad42());
    RUN(testLoadAcq42());
    RUN(testLoadOffsetImm9Max());
    RUN(testLoadOffsetImm9MaxPlusOne());
    RUN(testLoadOffsetImm9MaxPlusTwo());
    RUN(testLoadOffsetImm9Min());
    RUN(testLoadOffsetImm9MinMinusOne());
    RUN(testLoadOffsetScaledUnsignedImm12Max());
    RUN(testLoadOffsetScaledUnsignedOverImm12Max());
    RUN(testArg(43));
    RUN(testReturnConst64(5));
    RUN(testReturnConst64(-42));
    RUN(testReturnVoid());

    RUN_BINARY(testBitXorTreeArgs, int64Operands(), int64Operands());
    RUN_BINARY(testBitXorTreeArgsEven, int64Operands(), int64Operands());
    RUN_BINARY(testBitXorTreeArgImm, int64Operands(), int64Operands());
    RUN_UNARY(testAddTreeArg32, int32Operands());
    RUN_UNARY(testMulTreeArg32, int32Operands());
    RUN_UNARY(testBitAndTreeArg32, int32Operands());
    RUN_UNARY(testBitOrTreeArg32, int32Operands());

    RUN(testAddArg(111));
    RUN(testAddArgs(1, 1));
    RUN(testAddArgs(1, 2));
    RUN(testAddArgImm(1, 2));
    RUN(testAddArgImm(0, 2));
    RUN(testAddArgImm(1, 0));
    RUN(testAddImmArg(1, 2));
    RUN(testAddImmArg(0, 2));
    RUN(testAddImmArg(1, 0));
    RUN_BINARY(testAddArgMem, int64Operands(), int64Operands());
    RUN_BINARY(testAddMemArg, int64Operands(), int64Operands());
    RUN_BINARY(testAddImmMem, int64Operands(), int64Operands());
    RUN_UNARY(testAddArg32, int32Operands());
    RUN(testAddArgs32(1, 1));
    RUN(testAddArgs32(1, 2));
    RUN_BINARY(testAddArgMem32, int32Operands(), int32Operands());
    RUN_BINARY(testAddMemArg32, int32Operands(), int32Operands());
    RUN_BINARY(testAddImmMem32, int32Operands(), int32Operands());
    RUN_BINARY(testAddNeg1, int32Operands(), int32Operands());
    RUN_BINARY(testAddNeg2, int32Operands(), int32Operands());
    RUN(testAddArgZeroImmZDef());
    RUN(testAddLoadTwice());
    RUN_TERNARY(testAddMulMulArgs, int64Operands(), int64Operands(), int64Operands());

    RUN(testAddArgDouble(M_PI));
    RUN(testAddArgsDouble(M_PI, 1));
    RUN(testAddArgsDouble(M_PI, -M_PI));
    RUN(testAddArgImmDouble(M_PI, 1));
    RUN(testAddArgImmDouble(M_PI, 0));
    RUN(testAddArgImmDouble(M_PI, negativeZero()));
    RUN(testAddArgImmDouble(0, 0));
    RUN(testAddArgImmDouble(0, negativeZero()));
    RUN(testAddArgImmDouble(negativeZero(), 0));
    RUN(testAddArgImmDouble(negativeZero(), negativeZero()));
    RUN(testAddImmArgDouble(M_PI, 1));
    RUN(testAddImmArgDouble(M_PI, 0));
    RUN(testAddImmArgDouble(M_PI, negativeZero()));
    RUN(testAddImmArgDouble(0, 0));
    RUN(testAddImmArgDouble(0, negativeZero()));
    RUN(testAddImmArgDouble(negativeZero(), 0));
    RUN(testAddImmArgDouble(negativeZero(), negativeZero()));
    RUN(testAddImmsDouble(M_PI, 1));
    RUN(testAddImmsDouble(M_PI, 0));
    RUN(testAddImmsDouble(M_PI, negativeZero()));
    RUN(testAddImmsDouble(0, 0));
    RUN(testAddImmsDouble(0, negativeZero()));
    RUN(testAddImmsDouble(negativeZero(), negativeZero()));
    RUN_UNARY(testAddArgFloat, floatingPointOperands<float>());
    RUN_BINARY(testAddArgsFloat, floatingPointOperands<float>(), floatingPointOperands<float>());
    RUN_BINARY(testAddFPRArgsFloat, floatingPointOperands<float>(), floatingPointOperands<float>());
    RUN_BINARY(testAddArgImmFloat, floatingPointOperands<float>(), floatingPointOperands<float>());
    RUN_BINARY(testAddImmArgFloat, floatingPointOperands<float>(), floatingPointOperands<float>());
    RUN_BINARY(testAddImmsFloat, floatingPointOperands<float>(), floatingPointOperands<float>());
    RUN_UNARY(testAddArgFloatWithUselessDoubleConversion, floatingPointOperands<float>());
    RUN_BINARY(testAddArgsFloatWithUselessDoubleConversion, floatingPointOperands<float>(), floatingPointOperands<float>());
    RUN_BINARY(testAddArgsFloatWithEffectfulDoubleConversion, floatingPointOperands<float>(), floatingPointOperands<float>());

    RUN(testMulArg(5));
    RUN(testMulAddArg(5));
    RUN(testMulAddArg(85));
    RUN(testMulArgStore(5));
    RUN(testMulArgStore(85));
    RUN(testMulArgs(1, 1));
    RUN(testMulArgs(1, 2));
    RUN(testMulArgs(3, 3));
    RUN(testMulArgImm(1, 2));
    RUN(testMulArgImm(1, 4));
    RUN(testMulArgImm(1, 8));
    RUN(testMulArgImm(1, 16));
    RUN(testMulArgImm(1, 0x80000000llu));
    RUN(testMulArgImm(1, 0x800000000000llu));
    RUN(testMulArgImm(7, 2));
    RUN(testMulArgImm(7, 4));
    RUN(testMulArgImm(7, 8));
    RUN(testMulArgImm(7, 16));
    RUN(testMulArgImm(7, 0x80000000llu));
    RUN(testMulArgImm(7, 0x800000000000llu));
    RUN(testMulArgImm(-42, 2));
    RUN(testMulArgImm(-42, 4));
    RUN(testMulArgImm(-42, 8));
    RUN(testMulArgImm(-42, 16));
    RUN(testMulArgImm(-42, 0x80000000llu));
    RUN(testMulArgImm(-42, 0x800000000000llu));
    RUN(testMulArgImm(0, 2));
    RUN(testMulArgImm(1, 0));
    RUN(testMulArgImm(3, 3));
    RUN(testMulArgImm(3, -1));
    RUN(testMulArgImm(-3, -1));
    RUN(testMulArgImm(0, -1));
    RUN(testMulImmArg(1, 2));
    RUN(testMulImmArg(0, 2));
    RUN(testMulImmArg(1, 0));
    RUN(testMulImmArg(3, 3));
    RUN(testMulImm32SignExtend(1, 2));
    RUN(testMulImm32SignExtend(0, 2));
    RUN(testMulImm32SignExtend(1, 0));
    RUN(testMulImm32SignExtend(3, 3));
    RUN(testMulImm32SignExtend(0xFFFFFFFF, 0xFFFFFFFF));
    RUN(testMulImm32SignExtend(0xFFFFFFFE, 0xFFFFFFFF));
    RUN(testMulImm32SignExtend(0xFFFFFFFF, 0xFFFFFFFE));
    RUN(testMulArgs32(1, 1));
    RUN(testMulArgs32(1, 2));
    RUN(testMulArgs32(0xFFFFFFFF, 0xFFFFFFFF));
    RUN(testMulArgs32(0xFFFFFFFE, 0xFFFFFFFF));
    RUN(testMulArgs32SignExtend(1, 1));
    RUN(testMulArgs32SignExtend(1, 2));
    RUN(testMulArgs32SignExtend(0xFFFFFFFF, 0xFFFFFFFF));
    RUN(testMulArgs32SignExtend(0xFFFFFFFE, 0xFFFFFFFF));
    RUN(testMulLoadTwice());
    RUN(testMulAddArgsLeft());
    RUN(testMulAddArgsRight());
    RUN(testMulAddArgsLeft32());
    RUN(testMulAddArgsRight32());
    RUN(testMulSubArgsLeft());
    RUN(testMulSubArgsRight());
    RUN(testMulSubArgsLeft32());
    RUN(testMulSubArgsRight32());
    RUN(testMulNegArgs());
    RUN(testMulNegArgs32());

    RUN_BINARY(testMulArgNegArg, int64Operands(), int64Operands())
    RUN_BINARY(testMulNegArgArg, int64Operands(), int64Operands())
    RUN_UNARY(testMulArgDouble, floatingPointOperands<double>());
    RUN_BINARY(testMulArgsDouble, floatingPointOperands<double>(), floatingPointOperands<double>());
    RUN_BINARY(testMulArgImmDouble, floatingPointOperands<double>(), floatingPointOperands<double>());
    RUN_BINARY(testMulImmArgDouble, floatingPointOperands<double>(), floatingPointOperands<double>());
    RUN_BINARY(testMulImmsDouble, floatingPointOperands<double>(), floatingPointOperands<double>());
    RUN_UNARY(testMulArgFloat, floatingPointOperands<float>());
    RUN_BINARY(testMulArgsFloat, floatingPointOperands<float>(), floatingPointOperands<float>());
    RUN_BINARY(testMulArgImmFloat, floatingPointOperands<float>(), floatingPointOperands<float>());
    RUN_BINARY(testMulImmArgFloat, floatingPointOperands<float>(), floatingPointOperands<float>());
    RUN_BINARY(testMulImmsFloat, floatingPointOperands<float>(), floatingPointOperands<float>());
    RUN_UNARY(testMulArgFloatWithUselessDoubleConversion, floatingPointOperands<float>());
    RUN_BINARY(testMulArgsFloatWithUselessDoubleConversion, floatingPointOperands<float>(), floatingPointOperands<float>());
    RUN_BINARY(testMulArgsFloatWithEffectfulDoubleConversion, floatingPointOperands<float>(), floatingPointOperands<float>());

    RUN(testDivArgDouble(M_PI));
    RUN(testDivArgsDouble(M_PI, 1));
    RUN(testDivArgsDouble(M_PI, -M_PI));
    RUN(testDivArgImmDouble(M_PI, 1));
    RUN(testDivArgImmDouble(M_PI, 0));
    RUN(testDivArgImmDouble(M_PI, negativeZero()));
    RUN(testDivArgImmDouble(0, 0));
    RUN(testDivArgImmDouble(0, negativeZero()));
    RUN(testDivArgImmDouble(negativeZero(), 0));
    RUN(testDivArgImmDouble(negativeZero(), negativeZero()));
    RUN(testDivImmArgDouble(M_PI, 1));
    RUN(testDivImmArgDouble(M_PI, 0));
    RUN(testDivImmArgDouble(M_PI, negativeZero()));
    RUN(testDivImmArgDouble(0, 0));
    RUN(testDivImmArgDouble(0, negativeZero()));
    RUN(testDivImmArgDouble(negativeZero(), 0));
    RUN(testDivImmArgDouble(negativeZero(), negativeZero()));
    RUN(testDivImmsDouble(M_PI, 1));
    RUN(testDivImmsDouble(M_PI, 0));
    RUN(testDivImmsDouble(M_PI, negativeZero()));
    RUN(testDivImmsDouble(0, 0));
    RUN(testDivImmsDouble(0, negativeZero()));
    RUN(testDivImmsDouble(negativeZero(), negativeZero()));
    RUN_UNARY(testDivArgFloat, floatingPointOperands<float>());
    RUN_BINARY(testDivArgsFloat, floatingPointOperands<float>(), floatingPointOperands<float>());
    RUN_BINARY(testDivArgImmFloat, floatingPointOperands<float>(), floatingPointOperands<float>());
    RUN_BINARY(testDivImmArgFloat, floatingPointOperands<float>(), floatingPointOperands<float>());
    RUN_BINARY(testDivImmsFloat, floatingPointOperands<float>(), floatingPointOperands<float>());
    RUN_UNARY(testDivArgFloatWithUselessDoubleConversion, floatingPointOperands<float>());
    RUN_BINARY(testDivArgsFloatWithUselessDoubleConversion, floatingPointOperands<float>(), floatingPointOperands<float>());
    RUN_BINARY(testDivArgsFloatWithEffectfulDoubleConversion, floatingPointOperands<float>(), floatingPointOperands<float>());

    RUN_BINARY(testUDivArgsInt32, int32Operands(), int32Operands());
    RUN_BINARY(testUDivArgsInt64, int64Operands(), int64Operands());

    RUN_UNARY(testModArgDouble, floatingPointOperands<double>());
    RUN_BINARY(testModArgsDouble, floatingPointOperands<double>(), floatingPointOperands<double>());
    RUN_BINARY(testModArgImmDouble, floatingPointOperands<double>(), floatingPointOperands<double>());
    RUN_BINARY(testModImmArgDouble, floatingPointOperands<double>(), floatingPointOperands<double>());
    RUN_BINARY(testModImmsDouble, floatingPointOperands<double>(), floatingPointOperands<double>());
    RUN_UNARY(testModArgFloat, floatingPointOperands<float>());
    RUN_BINARY(testModArgsFloat, floatingPointOperands<float>(), floatingPointOperands<float>());
    RUN_BINARY(testModArgImmFloat, floatingPointOperands<float>(), floatingPointOperands<float>());
    RUN_BINARY(testModImmArgFloat, floatingPointOperands<float>(), floatingPointOperands<float>());
    RUN_BINARY(testModImmsFloat, floatingPointOperands<float>(), floatingPointOperands<float>());

    RUN_BINARY(testUModArgsInt32, int32Operands(), int32Operands());
    RUN_BINARY(testUModArgsInt64, int64Operands(), int64Operands());

    RUN(testSubArg(24));
    RUN(testSubArgs(1, 1));
    RUN(testSubArgs(1, 2));
    RUN(testSubArgs(13, -42));
    RUN(testSubArgs(-13, 42));
    RUN(testSubArgImm(1, 1));
    RUN(testSubArgImm(1, 2));
    RUN(testSubArgImm(13, -42));
    RUN(testSubArgImm(-13, 42));
    RUN(testSubArgImm(42, 0));
    RUN(testSubImmArg(1, 1));
    RUN(testSubImmArg(1, 2));
    RUN(testSubImmArg(13, -42));
    RUN(testSubImmArg(-13, 42));
    RUN_BINARY(testSubArgMem, int64Operands(), int64Operands());
    RUN_BINARY(testSubMemArg, int64Operands(), int64Operands());
    RUN_BINARY(testSubImmMem, int32Operands(), int32Operands());
    RUN_BINARY(testSubMemImm, int32Operands(), int32Operands());
    RUN_BINARY(testSubNeg, int32Operands(), int32Operands());
    RUN_BINARY(testNegSub, int32Operands(), int32Operands());
    RUN_UNARY(testNegValueSubOne, int32Operands());
    RUN_BINARY(testNegMulArgImm, int64Operands(), int64Operands());
    RUN_TERNARY(testSubMulMulArgs, int64Operands(), int64Operands(), int64Operands());

    RUN_TERNARY(testSubSub, int32Operands(), int32Operands(), int32Operands());
    RUN_TERNARY(testSubSub2, int32Operands(), int32Operands(), int32Operands());
    RUN_TERNARY(testSubAdd, int32Operands(), int32Operands(), int32Operands());
    RUN_BINARY(testSubFirstNeg, int32Operands(), int32Operands());

    RUN(testSubArgs32(1, 1));
    RUN(testSubArgs32(1, 2));
    RUN(testSubArgs32(13, -42));
    RUN(testSubArgs32(-13, 42));
    RUN(testSubArgImm32(1, 1));
    RUN(testSubArgImm32(1, 2));
    RUN(testSubArgImm32(13, -42));
    RUN(testSubArgImm32(-13, 42));
    RUN(testSubImmArg32(1, 1));
    RUN(testSubImmArg32(1, 2));
    RUN(testSubImmArg32(13, -42));
    RUN(testSubImmArg32(-13, 42));
    RUN_BINARY(testSubArgMem32, int32Operands(), int32Operands());
    RUN_BINARY(testSubMemArg32, int32Operands(), int32Operands());
    RUN_BINARY(testSubImmMem32, int32Operands(), int32Operands());
    RUN_BINARY(testSubMemImm32, int32Operands(), int32Operands());
    RUN_UNARY(testNegValueSubOne32, int64Operands());

    RUN_UNARY(testSubArgDouble, floatingPointOperands<double>());
    RUN_BINARY(testSubArgsDouble, floatingPointOperands<double>(), floatingPointOperands<double>());
    RUN_BINARY(testSubArgImmDouble, floatingPointOperands<double>(), floatingPointOperands<double>());
    RUN_BINARY(testSubImmArgDouble, floatingPointOperands<double>(), floatingPointOperands<double>());
    RUN_BINARY(testSubImmsDouble, floatingPointOperands<double>(), floatingPointOperands<double>());
    RUN_UNARY(testSubArgFloat, floatingPointOperands<float>());
    RUN_BINARY(testSubArgsFloat, floatingPointOperands<float>(), floatingPointOperands<float>());
    RUN_BINARY(testSubArgImmFloat, floatingPointOperands<float>(), floatingPointOperands<float>());
    RUN_BINARY(testSubImmArgFloat, floatingPointOperands<float>(), floatingPointOperands<float>());
    RUN_BINARY(testSubImmsFloat, floatingPointOperands<float>(), floatingPointOperands<float>());
    RUN_UNARY(testSubArgFloatWithUselessDoubleConversion, floatingPointOperands<float>());
    RUN_BINARY(testSubArgsFloatWithUselessDoubleConversion, floatingPointOperands<float>(), floatingPointOperands<float>());
    RUN_BINARY(testSubArgsFloatWithEffectfulDoubleConversion, floatingPointOperands<float>(), floatingPointOperands<float>());

    RUN_UNARY(testNegDouble, floatingPointOperands<double>());
    RUN_UNARY(testNegFloat, floatingPointOperands<float>());
    RUN_UNARY(testNegFloatWithUselessDoubleConversion, floatingPointOperands<float>());

    RUN(testBitAndArgs(43, 43));
    RUN(testBitAndArgs(43, 0));
    RUN(testBitAndArgs(10, 3));
    RUN(testBitAndArgs(42, 0xffffffffffffffff));
    RUN(testBitAndSameArg(43));
    RUN(testBitAndSameArg(0));
    RUN(testBitAndSameArg(3));
    RUN(testBitAndSameArg(0xffffffffffffffff));
    RUN(testBitAndImms(43, 43));
    RUN(testBitAndImms(43, 0));
    RUN(testBitAndImms(10, 3));
    RUN(testBitAndImms(42, 0xffffffffffffffff));
    RUN(testBitAndArgImm(43, 43));
    RUN(testBitAndArgImm(43, 0));
    RUN(testBitAndArgImm(10, 3));
    RUN(testBitAndArgImm(42, 0xffffffffffffffff));
    RUN(testBitAndArgImm(42, 0xff));
    RUN(testBitAndArgImm(300, 0xff));
    RUN(testBitAndArgImm(-300, 0xff));
    RUN(testBitAndArgImm(42, 0xffff));
    RUN(testBitAndArgImm(40000, 0xffff));
    RUN(testBitAndArgImm(-40000, 0xffff));
    RUN(testBitAndImmArg(43, 43));
    RUN(testBitAndImmArg(43, 0));
    RUN(testBitAndImmArg(10, 3));
    RUN(testBitAndImmArg(42, 0xffffffffffffffff));
    RUN(testBitAndBitAndArgImmImm(2, 7, 3));
    RUN(testBitAndBitAndArgImmImm(1, 6, 6));
    RUN(testBitAndBitAndArgImmImm(0xffff, 24, 7));
    RUN(testBitAndImmBitAndArgImm(7, 2, 3));
    RUN(testBitAndImmBitAndArgImm(6, 1, 6));
    RUN(testBitAndImmBitAndArgImm(24, 0xffff, 7));
    RUN(testBitAndArgs32(43, 43));
    RUN(testBitAndArgs32(43, 0));
    RUN(testBitAndArgs32(10, 3));
    RUN(testBitAndArgs32(42, 0xffffffff));
    RUN(testBitAndSameArg32(43));
    RUN(testBitAndSameArg32(0));
    RUN(testBitAndSameArg32(3));
    RUN(testBitAndSameArg32(0xffffffff));
    RUN(testBitAndImms32(43, 43));
    RUN(testBitAndImms32(43, 0));
    RUN(testBitAndImms32(10, 3));
    RUN(testBitAndImms32(42, 0xffffffff));
    RUN(testBitAndArgImm32(43, 43));
    RUN(testBitAndArgImm32(43, 0));
    RUN(testBitAndArgImm32(10, 3));
    RUN(testBitAndArgImm32(42, 0xffffffff));
    RUN(testBitAndImmArg32(43, 43));
    RUN(testBitAndImmArg32(43, 0));
    RUN(testBitAndImmArg32(10, 3));
    RUN(testBitAndImmArg32(42, 0xffffffff));
    RUN(testBitAndImmArg32(42, 0xff));
    RUN(testBitAndImmArg32(300, 0xff));
    RUN(testBitAndImmArg32(-300, 0xff));
    RUN(testBitAndImmArg32(42, 0xffff));
    RUN(testBitAndImmArg32(40000, 0xffff));
    RUN(testBitAndImmArg32(-40000, 0xffff));
    RUN(testBitAndBitAndArgImmImm32(2, 7, 3));
    RUN(testBitAndBitAndArgImmImm32(1, 6, 6));
    RUN(testBitAndBitAndArgImmImm32(0xffff, 24, 7));
    RUN(testBitAndImmBitAndArgImm32(7, 2, 3));
    RUN(testBitAndImmBitAndArgImm32(6, 1, 6));
    RUN(testBitAndImmBitAndArgImm32(24, 0xffff, 7));
    RUN_BINARY(testBitAndWithMaskReturnsBooleans, int64Operands(), int64Operands());
    RUN_UNARY(testBitAndArgDouble, floatingPointOperands<double>());
    RUN_BINARY(testBitAndArgsDouble, floatingPointOperands<double>(), floatingPointOperands<double>());
    RUN_BINARY(testBitAndArgImmDouble, floatingPointOperands<double>(), floatingPointOperands<double>());
    RUN_BINARY(testBitAndImmsDouble, floatingPointOperands<double>(), floatingPointOperands<double>());
    RUN_UNARY(testBitAndArgFloat, floatingPointOperands<float>());
    RUN_BINARY(testBitAndArgsFloat, floatingPointOperands<float>(), floatingPointOperands<float>());
    RUN_BINARY(testBitAndArgImmFloat, floatingPointOperands<float>(), floatingPointOperands<float>());
    RUN_BINARY(testBitAndImmsFloat, floatingPointOperands<float>(), floatingPointOperands<float>());
    RUN_BINARY(testBitAndArgsFloatWithUselessDoubleConversion, floatingPointOperands<float>(), floatingPointOperands<float>());
    RUN_BINARY(testBitAndNotNot, int64Operands(), int64Operands());
    RUN_BINARY(testBitAndNotImm, int64Operands(), int64Operands());

    RUN(testBitOrArgs(43, 43));
    RUN(testBitOrArgs(43, 0));
    RUN(testBitOrArgs(10, 3));
    RUN(testBitOrArgs(42, 0xffffffffffffffff));
    RUN(testBitOrSameArg(43));
    RUN(testBitOrSameArg(0));
    RUN(testBitOrSameArg(3));
    RUN(testBitOrSameArg(0xffffffffffffffff));
    RUN(testBitOrImms(43, 43));
    RUN(testBitOrImms(43, 0));
    RUN(testBitOrImms(10, 3));
    RUN(testBitOrImms(42, 0xffffffffffffffff));
    RUN(testBitOrArgImm(43, 43));
    RUN(testBitOrArgImm(43, 0));
    RUN(testBitOrArgImm(10, 3));
    RUN(testBitOrArgImm(42, 0xffffffffffffffff));
    RUN(testBitOrImmArg(43, 43));
    RUN(testBitOrImmArg(43, 0));
    RUN(testBitOrImmArg(10, 3));
    RUN(testBitOrImmArg(42, 0xffffffffffffffff));
    RUN(testBitOrBitOrArgImmImm(2, 7, 3));
    RUN(testBitOrBitOrArgImmImm(1, 6, 6));
    RUN(testBitOrBitOrArgImmImm(0xffff, 24, 7));
    RUN(testBitOrImmBitOrArgImm(7, 2, 3));
    RUN(testBitOrImmBitOrArgImm(6, 1, 6));
    RUN(testBitOrImmBitOrArgImm(24, 0xffff, 7));
    RUN(testBitOrArgs32(43, 43));
    RUN(testBitOrArgs32(43, 0));
    RUN(testBitOrArgs32(10, 3));
    RUN(testBitOrArgs32(42, 0xffffffff));
    RUN(testBitOrSameArg32(43));
    RUN(testBitOrSameArg32(0));
    RUN(testBitOrSameArg32(3));
    RUN(testBitOrSameArg32(0xffffffff));
    RUN(testBitOrImms32(43, 43));
    RUN(testBitOrImms32(43, 0));
    RUN(testBitOrImms32(10, 3));
    RUN(testBitOrImms32(42, 0xffffffff));
    RUN(testBitOrArgImm32(43, 43));
    RUN(testBitOrArgImm32(43, 0));
    RUN(testBitOrArgImm32(10, 3));
    RUN(testBitOrArgImm32(42, 0xffffffff));
    RUN(testBitOrImmArg32(43, 43));
    RUN(testBitOrImmArg32(43, 0));
    RUN(testBitOrImmArg32(10, 3));
    RUN(testBitOrImmArg32(42, 0xffffffff));
    RUN(testBitOrBitOrArgImmImm32(2, 7, 3));
    RUN(testBitOrBitOrArgImmImm32(1, 6, 6));
    RUN(testBitOrBitOrArgImmImm32(0xffff, 24, 7));
    RUN(testBitOrImmBitOrArgImm32(7, 2, 3));
    RUN(testBitOrImmBitOrArgImm32(6, 1, 6));
    RUN(testBitOrImmBitOrArgImm32(24, 0xffff, 7));
    RUN_UNARY(testBitOrArgDouble, floatingPointOperands<double>());
    RUN_BINARY(testBitOrArgsDouble, floatingPointOperands<double>(), floatingPointOperands<double>());
    RUN_BINARY(testBitOrArgImmDouble, floatingPointOperands<double>(), floatingPointOperands<double>());
    RUN_BINARY(testBitOrImmsDouble, floatingPointOperands<double>(), floatingPointOperands<double>());
    RUN_UNARY(testBitOrArgFloat, floatingPointOperands<float>());
    RUN_BINARY(testBitOrArgsFloat, floatingPointOperands<float>(), floatingPointOperands<float>());
    RUN_BINARY(testBitOrArgImmFloat, floatingPointOperands<float>(), floatingPointOperands<float>());
    RUN_BINARY(testBitOrImmsFloat, floatingPointOperands<float>(), floatingPointOperands<float>());
    RUN_BINARY(testBitOrArgsFloatWithUselessDoubleConversion, floatingPointOperands<float>(), floatingPointOperands<float>());
    RUN_TERNARY(testBitOrAndAndArgs, int64Operands(), int64Operands(), int64Operands());
    RUN_BINARY(testBitOrAndSameArgs, int64Operands(), int64Operands());
    RUN_BINARY(testBitOrNotNot, int64Operands(), int64Operands());
    RUN_BINARY(testBitOrNotImm, int64Operands(), int64Operands());

    RUN_BINARY(testBitXorArgs, int64Operands(), int64Operands());
    RUN_UNARY(testBitXorSameArg, int64Operands());
    RUN_BINARY(testBitXorImms, int64Operands(), int64Operands());
    RUN_BINARY(testBitXorArgImm, int64Operands(), int64Operands());
    RUN_BINARY(testBitXorImmArg, int64Operands(), int64Operands());
    RUN(testBitXorBitXorArgImmImm(2, 7, 3));
    RUN(testBitXorBitXorArgImmImm(1, 6, 6));
    RUN(testBitXorBitXorArgImmImm(0xffff, 24, 7));
    RUN(testBitXorImmBitXorArgImm(7, 2, 3));
    RUN(testBitXorImmBitXorArgImm(6, 1, 6));
    RUN(testBitXorImmBitXorArgImm(24, 0xffff, 7));
    RUN(testBitXorArgs32(43, 43));
    RUN(testBitXorArgs32(43, 0));
    RUN(testBitXorArgs32(10, 3));
    RUN(testBitXorArgs32(42, 0xffffffff));
    RUN(testBitXorSameArg32(43));
    RUN(testBitXorSameArg32(0));
    RUN(testBitXorSameArg32(3));
    RUN(testBitXorSameArg32(0xffffffff));
    RUN(testBitXorImms32(43, 43));
    RUN(testBitXorImms32(43, 0));
    RUN(testBitXorImms32(10, 3));
    RUN(testBitXorImms32(42, 0xffffffff));
    RUN(testBitXorArgImm32(43, 43));
    RUN(testBitXorArgImm32(43, 0));
    RUN(testBitXorArgImm32(10, 3));
    RUN(testBitXorArgImm32(42, 0xffffffff));
    RUN(testBitXorImmArg32(43, 43));
    RUN(testBitXorImmArg32(43, 0));
    RUN(testBitXorImmArg32(10, 3));
    RUN(testBitXorImmArg32(42, 0xffffffff));
    RUN(testBitXorBitXorArgImmImm32(2, 7, 3));
    RUN(testBitXorBitXorArgImmImm32(1, 6, 6));
    RUN(testBitXorBitXorArgImmImm32(0xffff, 24, 7));
    RUN(testBitXorImmBitXorArgImm32(7, 2, 3));
    RUN(testBitXorImmBitXorArgImm32(6, 1, 6));
    RUN(testBitXorImmBitXorArgImm32(24, 0xffff, 7));
    RUN_TERNARY(testBitXorAndAndArgs, int64Operands(), int64Operands(), int64Operands());
    RUN_BINARY(testBitXorAndSameArgs, int64Operands(), int64Operands());

    RUN_UNARY(testBitNotArg, int64Operands());
    RUN_UNARY(testBitNotImm, int64Operands());
    RUN_UNARY(testBitNotMem, int64Operands());
    RUN_UNARY(testBitNotArg32, int32Operands());
    RUN_UNARY(testBitNotImm32, int32Operands());
    RUN_UNARY(testBitNotMem32, int32Operands());
    RUN_BINARY(testNotOnBooleanAndBranch32, int32Operands(), int32Operands());
    RUN_BINARY(testBitNotOnBooleanAndBranch32, int32Operands(), int32Operands());

    RUN(testShlArgs(1, 0));
    RUN(testShlArgs(1, 1));
    RUN(testShlArgs(1, 62));
    RUN(testShlArgs(0xffffffffffffffff, 0));
    RUN(testShlArgs(0xffffffffffffffff, 1));
    RUN(testShlArgs(0xffffffffffffffff, 63));
    RUN(testShlImms(1, 0));
    RUN(testShlImms(1, 1));
    RUN(testShlImms(1, 62));
    RUN(testShlImms(1, 65));
    RUN(testShlImms(0xffffffffffffffff, 0));
    RUN(testShlImms(0xffffffffffffffff, 1));
    RUN(testShlImms(0xffffffffffffffff, 63));
    RUN(testShlArgImm(1, 0));
    RUN(testShlArgImm(1, 1));
    RUN(testShlArgImm(1, 62));
    RUN(testShlArgImm(1, 65));
    RUN(testShlArgImm(0xffffffffffffffff, 0));
    RUN(testShlArgImm(0xffffffffffffffff, 1));
    RUN(testShlArgImm(0xffffffffffffffff, 63));
    RUN(testShlSShrArgImm(1, 0));
    RUN(testShlSShrArgImm(1, 1));
    RUN(testShlSShrArgImm(1, 62));
    RUN(testShlSShrArgImm(1, 65));
    RUN(testShlSShrArgImm(0xffffffffffffffff, 0));
    RUN(testShlSShrArgImm(0xffffffffffffffff, 1));
    RUN(testShlSShrArgImm(0xffffffffffffffff, 63));
    RUN(testShlArg32(2));
    RUN(testShlArgs32(1, 0));
    RUN(testShlArgs32(1, 1));
    RUN(testShlArgs32(1, 62));
    RUN(testShlImms32(1, 33));
    RUN(testShlArgs32(0xffffffff, 0));
    RUN(testShlArgs32(0xffffffff, 1));
    RUN(testShlArgs32(0xffffffff, 63));
    RUN(testShlImms32(1, 0));
    RUN(testShlImms32(1, 1));
    RUN(testShlImms32(1, 62));
    RUN(testShlImms32(1, 33));
    RUN(testShlImms32(0xffffffff, 0));
    RUN(testShlImms32(0xffffffff, 1));
    RUN(testShlImms32(0xffffffff, 63));
    RUN(testShlArgImm32(1, 0));
    RUN(testShlArgImm32(1, 1));
    RUN(testShlArgImm32(1, 62));
    RUN(testShlArgImm32(1, 33));
    RUN(testShlArgImm32(0xffffffff, 0));
    RUN(testShlArgImm32(0xffffffff, 1));
    RUN(testShlArgImm32(0xffffffff, 63));
    RUN(testShlZShrArgImm32(1, 0));
    RUN(testShlZShrArgImm32(1, 1));
    RUN(testShlZShrArgImm32(1, 62));
    RUN(testShlZShrArgImm32(1, 33));
    RUN(testShlZShrArgImm32(0xffffffff, 0));
    RUN(testShlZShrArgImm32(0xffffffff, 1));
    RUN(testShlZShrArgImm32(0xffffffff, 63));

    RUN(testSShrArgs(1, 0));
    RUN(testSShrArgs(1, 1));
    RUN(testSShrArgs(1, 62));
    RUN(testSShrArgs(0xffffffffffffffff, 0));
    RUN(testSShrArgs(0xffffffffffffffff, 1));
    RUN(testSShrArgs(0xffffffffffffffff, 63));
    RUN(testSShrImms(1, 0));
    RUN(testSShrImms(1, 1));
    RUN(testSShrImms(1, 62));
    RUN(testSShrImms(1, 65));
    RUN(testSShrImms(0xffffffffffffffff, 0));
    RUN(testSShrImms(0xffffffffffffffff, 1));
    RUN(testSShrImms(0xffffffffffffffff, 63));
    RUN(testSShrArgImm(1, 0));
    RUN(testSShrArgImm(1, 1));
    RUN(testSShrArgImm(1, 62));
    RUN(testSShrArgImm(1, 65));
    RUN(testSShrArgImm(0xffffffffffffffff, 0));
    RUN(testSShrArgImm(0xffffffffffffffff, 1));
    RUN(testSShrArgImm(0xffffffffffffffff, 63));
    RUN(testSShrArg32(32));
    RUN(testSShrArgs32(1, 0));
    RUN(testSShrArgs32(1, 1));
    RUN(testSShrArgs32(1, 62));
    RUN(testSShrArgs32(1, 33));
    RUN(testSShrArgs32(0xffffffff, 0));
    RUN(testSShrArgs32(0xffffffff, 1));
    RUN(testSShrArgs32(0xffffffff, 63));
    RUN(testSShrImms32(1, 0));
    RUN(testSShrImms32(1, 1));
    RUN(testSShrImms32(1, 62));
    RUN(testSShrImms32(1, 33));
    RUN(testSShrImms32(0xffffffff, 0));
    RUN(testSShrImms32(0xffffffff, 1));
    RUN(testSShrImms32(0xffffffff, 63));
    RUN(testSShrArgImm32(1, 0));
    RUN(testSShrArgImm32(1, 1));
    RUN(testSShrArgImm32(1, 62));
    RUN(testSShrArgImm32(0xffffffff, 0));
    RUN(testSShrArgImm32(0xffffffff, 1));
    RUN(testSShrArgImm32(0xffffffff, 63));

    RUN(testZShrArgs(1, 0));
    RUN(testZShrArgs(1, 1));
    RUN(testZShrArgs(1, 62));
    RUN(testZShrArgs(0xffffffffffffffff, 0));
    RUN(testZShrArgs(0xffffffffffffffff, 1));
    RUN(testZShrArgs(0xffffffffffffffff, 63));
    RUN(testZShrImms(1, 0));
    RUN(testZShrImms(1, 1));
    RUN(testZShrImms(1, 62));
    RUN(testZShrImms(1, 65));
    RUN(testZShrImms(0xffffffffffffffff, 0));
    RUN(testZShrImms(0xffffffffffffffff, 1));
    RUN(testZShrImms(0xffffffffffffffff, 63));
    RUN(testZShrArgImm(1, 0));
    RUN(testZShrArgImm(1, 1));
    RUN(testZShrArgImm(1, 62));
    RUN(testZShrArgImm(1, 65));
    RUN(testZShrArgImm(0xffffffffffffffff, 0));
    RUN(testZShrArgImm(0xffffffffffffffff, 1));
    RUN(testZShrArgImm(0xffffffffffffffff, 63));
    RUN(testZShrArg32(32));
    RUN(testZShrArgs32(1, 0));
    RUN(testZShrArgs32(1, 1));
    RUN(testZShrArgs32(1, 62));
    RUN(testZShrArgs32(1, 33));
    RUN(testZShrArgs32(0xffffffff, 0));
    RUN(testZShrArgs32(0xffffffff, 1));
    RUN(testZShrArgs32(0xffffffff, 63));
    RUN(testZShrImms32(1, 0));
    RUN(testZShrImms32(1, 1));
    RUN(testZShrImms32(1, 62));
    RUN(testZShrImms32(1, 33));
    RUN(testZShrImms32(0xffffffff, 0));
    RUN(testZShrImms32(0xffffffff, 1));
    RUN(testZShrImms32(0xffffffff, 63));
    RUN(testZShrArgImm32(1, 0));
    RUN(testZShrArgImm32(1, 1));
    RUN(testZShrArgImm32(1, 62));
    RUN(testZShrArgImm32(0xffffffff, 0));
    RUN(testZShrArgImm32(0xffffffff, 1));
    RUN(testZShrArgImm32(0xffffffff, 63));

    RUN_UNARY(testClzArg64, int64Operands());
    RUN_UNARY(testClzMem64, int64Operands());
    RUN_UNARY(testClzArg32, int32Operands());
    RUN_UNARY(testClzMem32, int64Operands());

    RUN_UNARY(testAbsArg, floatingPointOperands<double>());
    RUN_UNARY(testAbsImm, floatingPointOperands<double>());
    RUN_UNARY(testAbsMem, floatingPointOperands<double>());
    RUN_UNARY(testAbsAbsArg, floatingPointOperands<double>());
    RUN_UNARY(testAbsNegArg, floatingPointOperands<double>());
    RUN_UNARY(testAbsBitwiseCastArg, floatingPointOperands<double>());
    RUN_UNARY(testBitwiseCastAbsBitwiseCastArg, floatingPointOperands<double>());
    RUN_UNARY(testAbsArg, floatingPointOperands<float>());
    RUN_UNARY(testAbsImm, floatingPointOperands<float>());
    RUN_UNARY(testAbsMem, floatingPointOperands<float>());
    RUN_UNARY(testAbsAbsArg, floatingPointOperands<float>());
    RUN_UNARY(testAbsNegArg, floatingPointOperands<float>());
    RUN_UNARY(testAbsBitwiseCastArg, floatingPointOperands<float>());
    RUN_UNARY(testBitwiseCastAbsBitwiseCastArg, floatingPointOperands<float>());
    RUN_UNARY(testAbsArgWithUselessDoubleConversion, floatingPointOperands<float>());
    RUN_UNARY(testAbsArgWithEffectfulDoubleConversion, floatingPointOperands<float>());

    RUN_UNARY(testCeilArg, floatingPointOperands<double>());
    RUN_UNARY(testCeilImm, floatingPointOperands<double>());
    RUN_UNARY(testCeilMem, floatingPointOperands<double>());
    RUN_UNARY(testCeilCeilArg, floatingPointOperands<double>());
    RUN_UNARY(testFloorCeilArg, floatingPointOperands<double>());
    RUN_UNARY(testCeilIToD64, int64Operands());
    RUN_UNARY(testCeilIToD32, int32Operands());
    RUN_UNARY(testCeilArg, floatingPointOperands<float>());
    RUN_UNARY(testCeilImm, floatingPointOperands<float>());
    RUN_UNARY(testCeilMem, floatingPointOperands<float>());
    RUN_UNARY(testCeilCeilArg, floatingPointOperands<float>());
    RUN_UNARY(testFloorCeilArg, floatingPointOperands<float>());
    RUN_UNARY(testCeilArgWithUselessDoubleConversion, floatingPointOperands<float>());
    RUN_UNARY(testCeilArgWithEffectfulDoubleConversion, floatingPointOperands<float>());

    RUN_UNARY(testFloorArg, floatingPointOperands<double>());
    RUN_UNARY(testFloorImm, floatingPointOperands<double>());
    RUN_UNARY(testFloorMem, floatingPointOperands<double>());
    RUN_UNARY(testFloorFloorArg, floatingPointOperands<double>());
    RUN_UNARY(testCeilFloorArg, floatingPointOperands<double>());
    RUN_UNARY(testFloorIToD64, int64Operands());
    RUN_UNARY(testFloorIToD32, int32Operands());
    RUN_UNARY(testFloorArg, floatingPointOperands<float>());
    RUN_UNARY(testFloorImm, floatingPointOperands<float>());
    RUN_UNARY(testFloorMem, floatingPointOperands<float>());
    RUN_UNARY(testFloorFloorArg, floatingPointOperands<float>());
    RUN_UNARY(testCeilFloorArg, floatingPointOperands<float>());
    RUN_UNARY(testFloorArgWithUselessDoubleConversion, floatingPointOperands<float>());
    RUN_UNARY(testFloorArgWithEffectfulDoubleConversion, floatingPointOperands<float>());

    RUN_UNARY(testSqrtArg, floatingPointOperands<double>());
    RUN_UNARY(testSqrtImm, floatingPointOperands<double>());
    RUN_UNARY(testSqrtMem, floatingPointOperands<double>());
    RUN_UNARY(testSqrtArg, floatingPointOperands<float>());
    RUN_UNARY(testSqrtImm, floatingPointOperands<float>());
    RUN_UNARY(testSqrtMem, floatingPointOperands<float>());
    RUN_UNARY(testSqrtArgWithUselessDoubleConversion, floatingPointOperands<float>());
    RUN_UNARY(testSqrtArgWithEffectfulDoubleConversion, floatingPointOperands<float>());

    RUN_BINARY(testCompareTwoFloatToDouble, floatingPointOperands<float>(), floatingPointOperands<float>());
    RUN_BINARY(testCompareOneFloatToDouble, floatingPointOperands<float>(), floatingPointOperands<double>());
    RUN_BINARY(testCompareFloatToDoubleThroughPhi, floatingPointOperands<float>(), floatingPointOperands<float>());
    RUN_UNARY(testDoubleToFloatThroughPhi, floatingPointOperands<float>());
    RUN(testReduceFloatToDoubleValidates());
    RUN_UNARY(testDoubleProducerPhiToFloatConversion, floatingPointOperands<float>());
    RUN_UNARY(testDoubleProducerPhiToFloatConversionWithDoubleConsumer, floatingPointOperands<float>());
    RUN_BINARY(testDoubleProducerPhiWithNonFloatConst, floatingPointOperands<float>(), floatingPointOperands<double>());

    RUN_UNARY(testDoubleArgToInt64BitwiseCast, floatingPointOperands<double>());
    RUN_UNARY(testDoubleImmToInt64BitwiseCast, floatingPointOperands<double>());
    RUN_UNARY(testTwoBitwiseCastOnDouble, floatingPointOperands<double>());
    RUN_UNARY(testBitwiseCastOnDoubleInMemory, floatingPointOperands<double>());
    RUN_UNARY(testBitwiseCastOnDoubleInMemoryIndexed, floatingPointOperands<double>());
    RUN_UNARY(testInt64BArgToDoubleBitwiseCast, int64Operands());
    RUN_UNARY(testInt64BImmToDoubleBitwiseCast, int64Operands());
    RUN_UNARY(testTwoBitwiseCastOnInt64, int64Operands());
    RUN_UNARY(testBitwiseCastOnInt64InMemory, int64Operands());
    RUN_UNARY(testBitwiseCastOnInt64InMemoryIndexed, int64Operands());
    RUN_UNARY(testFloatImmToInt32BitwiseCast, floatingPointOperands<float>());
    RUN_UNARY(testBitwiseCastOnFloatInMemory, floatingPointOperands<float>());
    RUN_UNARY(testInt32BArgToFloatBitwiseCast, int32Operands());
    RUN_UNARY(testInt32BImmToFloatBitwiseCast, int32Operands());
    RUN_UNARY(testTwoBitwiseCastOnInt32, int32Operands());
    RUN_UNARY(testBitwiseCastOnInt32InMemory, int32Operands());

    RUN_UNARY(testConvertDoubleToFloatArg, floatingPointOperands<double>());
    RUN_UNARY(testConvertDoubleToFloatImm, floatingPointOperands<double>());
    RUN_UNARY(testConvertDoubleToFloatMem, floatingPointOperands<double>());
    RUN_UNARY(testConvertFloatToDoubleArg, floatingPointOperands<float>());
    RUN_UNARY(testConvertFloatToDoubleImm, floatingPointOperands<float>());
    RUN_UNARY(testConvertFloatToDoubleMem, floatingPointOperands<float>());
    RUN_UNARY(testConvertDoubleToFloatToDoubleToFloat, floatingPointOperands<double>());
    RUN_UNARY(testStoreFloat, floatingPointOperands<double>());
    RUN_UNARY(testStoreDoubleConstantAsFloat, floatingPointOperands<double>());
    RUN_UNARY(testLoadFloatConvertDoubleConvertFloatStoreFloat, floatingPointOperands<float>());
    RUN_UNARY(testFroundArg, floatingPointOperands<double>());
    RUN_UNARY(testFroundMem, floatingPointOperands<double>());

    RUN(testIToD64Arg());
    RUN(testIToF64Arg());
    RUN(testIToD32Arg());
    RUN(testIToF32Arg());
    RUN(testIToD64Mem());
    RUN(testIToF64Mem());
    RUN(testIToD32Mem());
    RUN(testIToF32Mem());
    RUN_UNARY(testIToD64Imm, int64Operands());
    RUN_UNARY(testIToF64Imm, int64Operands());
    RUN_UNARY(testIToD32Imm, int32Operands());
    RUN_UNARY(testIToF32Imm, int32Operands());
    RUN(testIToDReducedToIToF64Arg());
    RUN(testIToDReducedToIToF32Arg());

    RUN(testStore32(44));
    RUN(testStoreConstant(49));
    RUN(testStoreConstantPtr(49));
    RUN(testStore8Arg());
    RUN(testStore8Imm());
    RUN(testStorePartial8BitRegisterOnX86());
    RUN(testStore16Arg());
    RUN(testStore16Imm());
    RUN(testTrunc((static_cast<int64_t>(1) << 40) + 42));
    RUN(testAdd1(45));
    RUN(testAdd1Ptr(51));
    RUN(testAdd1Ptr(static_cast<intptr_t>(0xbaadbeef)));
    RUN(testNeg32(52));
    RUN(testNegPtr(53));
    RUN(testStoreAddLoad32(46));
    RUN(testStoreRelAddLoadAcq32(46));
    RUN(testStoreAddLoadImm32(46));
    RUN(testStoreAddLoad64(4600));
    RUN(testStoreRelAddLoadAcq64(4600));
    RUN(testStoreAddLoadImm64(4600));
    RUN(testStoreAddLoad8(4, Load8Z));
    RUN(testStoreRelAddLoadAcq8(4, Load8Z));
    RUN(testStoreRelAddFenceLoadAcq8(4, Load8Z));
    RUN(testStoreAddLoadImm8(4, Load8Z));
    RUN(testStoreAddLoad8(4, Load8S));
    RUN(testStoreRelAddLoadAcq8(4, Load8S));
    RUN(testStoreAddLoadImm8(4, Load8S));
    RUN(testStoreAddLoad16(6, Load16Z));
    RUN(testStoreRelAddLoadAcq16(6, Load16Z));
    RUN(testStoreAddLoadImm16(6, Load16Z));
    RUN(testStoreAddLoad16(6, Load16S));
    RUN(testStoreRelAddLoadAcq16(6, Load16S));
    RUN(testStoreAddLoadImm16(6, Load16S));
    RUN(testStoreAddLoad32Index(46));
    RUN(testStoreAddLoadImm32Index(46));
    RUN(testStoreAddLoad64Index(4600));
    RUN(testStoreAddLoadImm64Index(4600));
    RUN(testStoreAddLoad8Index(4, Load8Z));
    RUN(testStoreAddLoadImm8Index(4, Load8Z));
    RUN(testStoreAddLoad8Index(4, Load8S));
    RUN(testStoreAddLoadImm8Index(4, Load8S));
    RUN(testStoreAddLoad16Index(6, Load16Z));
    RUN(testStoreAddLoadImm16Index(6, Load16Z));
    RUN(testStoreAddLoad16Index(6, Load16S));
    RUN(testStoreAddLoadImm16Index(6, Load16S));
    RUN(testStoreSubLoad(46));
    RUN(testStoreAddLoadInterference(52));
    RUN(testStoreAddAndLoad(47, 0xffff));
    RUN(testStoreAddAndLoad(470000, 0xffff));
    RUN(testStoreNegLoad32(54));
    RUN(testStoreNegLoadPtr(55));
    RUN(testAdd1Uncommuted(48));
    RUN(testLoadOffset());
    RUN(testLoadOffsetNotConstant());
    RUN(testLoadOffsetUsingAdd());
    RUN(testLoadOffsetUsingAddInterference());
    RUN(testLoadOffsetUsingAddNotConstant());
    RUN(testLoadAddrShift(0));
    RUN(testLoadAddrShift(1));
    RUN(testLoadAddrShift(2));
    RUN(testLoadAddrShift(3));
    RUN(testFramePointer());
    RUN(testOverrideFramePointer());
    RUN(testStackSlot());
    RUN(testLoadFromFramePointer());
    RUN(testStoreLoadStackSlot(50));
    
    RUN(testBranch());
    RUN(testBranchPtr());
    RUN(testDiamond());
    RUN(testBranchNotEqual());
    RUN(testBranchNotEqualCommute());
    RUN(testBranchNotEqualNotEqual());
    RUN(testBranchEqual());
    RUN(testBranchEqualEqual());
    RUN(testBranchEqualCommute());
    RUN(testBranchEqualEqual1());
    RUN_BINARY(testBranchEqualOrUnorderedArgs, floatingPointOperands<double>(), floatingPointOperands<double>());
    RUN_BINARY(testBranchEqualOrUnorderedArgs, floatingPointOperands<float>(), floatingPointOperands<float>());
    RUN_BINARY(testBranchNotEqualAndOrderedArgs, floatingPointOperands<double>(), floatingPointOperands<double>());
    RUN_BINARY(testBranchNotEqualAndOrderedArgs, floatingPointOperands<float>(), floatingPointOperands<float>());
    RUN_BINARY(testBranchEqualOrUnorderedDoubleArgImm, floatingPointOperands<double>(), floatingPointOperands<double>());
    RUN_BINARY(testBranchEqualOrUnorderedFloatArgImm, floatingPointOperands<float>(), floatingPointOperands<float>());
    RUN_BINARY(testBranchEqualOrUnorderedDoubleImms, floatingPointOperands<double>(), floatingPointOperands<double>());
    RUN_BINARY(testBranchEqualOrUnorderedFloatImms, floatingPointOperands<float>(), floatingPointOperands<float>());
    RUN_BINARY(testBranchEqualOrUnorderedFloatWithUselessDoubleConversion, floatingPointOperands<float>(), floatingPointOperands<float>());
    RUN_BINARY(testBranchNotEqualAndOrderedArgs, floatingPointOperands<double>(), floatingPointOperands<double>());
    RUN_BINARY(testBranchNotEqualAndOrderedArgs, floatingPointOperands<float>(), floatingPointOperands<float>());
    RUN(testBranchFold(42));
    RUN(testBranchFold(0));
    RUN(testDiamondFold(42));
    RUN(testDiamondFold(0));
    RUN(testBranchNotEqualFoldPtr(42));
    RUN(testBranchNotEqualFoldPtr(0));
    RUN(testBranchEqualFoldPtr(42));
    RUN(testBranchEqualFoldPtr(0));
    RUN(testBranchLoadPtr());
    RUN(testBranchLoad32());
    RUN(testBranchLoad8S());
    RUN(testBranchLoad8Z());
    RUN(testBranchLoad16S());
    RUN(testBranchLoad16Z());
    RUN(testBranch8WithLoad8ZIndex());

    RUN(testComplex(64, 128));
    RUN(testComplex(4, 128));
    RUN(testComplex(4, 256));
    RUN(testComplex(4, 384));

    RUN_BINARY(testBranchBitTest32TmpImm, int32Operands(), int32Operands());
    RUN_BINARY(testBranchBitTest32AddrImm, int32Operands(), int32Operands());
    RUN_BINARY(testBranchBitTest32TmpTmp, int32Operands(), int32Operands());
    RUN_BINARY(testBranchBitTest64TmpTmp, int64Operands(), int64Operands());
    RUN_BINARY(testBranchBitTest64AddrTmp, int64Operands(), int64Operands());
    RUN_BINARY(testBranchBitTestNegation, int64Operands(), int64Operands());
    RUN_BINARY(testBranchBitTestNegation2, int64Operands(), int64Operands());

    RUN(testSimplePatchpoint());
    RUN(testSimplePatchpointWithoutOuputClobbersGPArgs());
    RUN(testSimplePatchpointWithOuputClobbersGPArgs());
    RUN(testSimplePatchpointWithoutOuputClobbersFPArgs());
    RUN(testSimplePatchpointWithOuputClobbersFPArgs());
    RUN(testPatchpointWithEarlyClobber());
    RUN(testPatchpointCallArg());
    RUN(testPatchpointFixedRegister());
    RUN(testPatchpointAny(ValueRep::WarmAny));
    RUN(testPatchpointAny(ValueRep::ColdAny));
    RUN(testPatchpointGPScratch());
    RUN(testPatchpointFPScratch());
    RUN(testPatchpointLotsOfLateAnys());
    RUN(testPatchpointAnyImm(ValueRep::WarmAny));
    RUN(testPatchpointAnyImm(ValueRep::ColdAny));
    RUN(testPatchpointAnyImm(ValueRep::LateColdAny));
    RUN(testPatchpointManyImms());
    RUN(testPatchpointWithRegisterResult());
    RUN(testPatchpointWithStackArgumentResult());
    RUN(testPatchpointWithAnyResult());
    RUN(testSimpleCheck());
    RUN(testCheckFalse());
    RUN(testCheckTrue());
    RUN(testCheckLessThan());
    RUN(testCheckMegaCombo());
    RUN(testCheckTrickyMegaCombo());
    RUN(testCheckTwoMegaCombos());
    RUN(testCheckTwoNonRedundantMegaCombos());
    RUN(testCheckAddImm());
    RUN(testCheckAddImmCommute());
    RUN(testCheckAddImmSomeRegister());
    RUN(testCheckAdd());
    RUN(testCheckAdd64());
    RUN(testCheckAddFold(100, 200));
    RUN(testCheckAddFoldFail(2147483647, 100));
    RUN(testCheckAddArgumentAliasing64());
    RUN(testCheckAddArgumentAliasing32());
    RUN(testCheckAddSelfOverflow64());
    RUN(testCheckAddSelfOverflow32());
    RUN(testCheckSubImm());
    RUN(testCheckSubBadImm());
    RUN(testCheckSub());
    RUN(testCheckSub64());
    RUN(testCheckSubFold(100, 200));
    RUN(testCheckSubFoldFail(-2147483647, 100));
    RUN(testCheckNeg());
    RUN(testCheckNeg64());
    RUN(testCheckMul());
    RUN(testCheckMulMemory());
    RUN(testCheckMul2());
    RUN(testCheckMul64());
    RUN(testCheckMulFold(100, 200));
    RUN(testCheckMulFoldFail(2147483647, 100));
    RUN(testCheckMulArgumentAliasing64());
    RUN(testCheckMulArgumentAliasing32());

    RUN_BINARY([](int32_t a, int32_t b) { testCompare(Equal, a, b); }, int64Operands(), int64Operands());
    RUN_BINARY([](int32_t a, int32_t b) { testCompare(NotEqual, a, b); }, int64Operands(), int64Operands());
    RUN_BINARY([](int32_t a, int32_t b) { testCompare(LessThan, a, b); }, int64Operands(), int64Operands());
    RUN_BINARY([](int32_t a, int32_t b) { testCompare(GreaterThan, a, b); }, int64Operands(), int64Operands());
    RUN_BINARY([](int32_t a, int32_t b) { testCompare(LessEqual, a, b); }, int64Operands(), int64Operands());
    RUN_BINARY([](int32_t a, int32_t b) { testCompare(GreaterEqual, a, b); }, int64Operands(), int64Operands());
    RUN_BINARY([](int32_t a, int32_t b) { testCompare(Below, a, b); }, int64Operands(), int64Operands());
    RUN_BINARY([](int32_t a, int32_t b) { testCompare(Above, a, b); }, int64Operands(), int64Operands());
    RUN_BINARY([](int32_t a, int32_t b) { testCompare(BelowEqual, a, b); }, int64Operands(), int64Operands());
    RUN_BINARY([](int32_t a, int32_t b) { testCompare(AboveEqual, a, b); }, int64Operands(), int64Operands());
    RUN_BINARY([](int32_t a, int32_t b) { testCompare(BitAnd, a, b); }, int64Operands(), int64Operands());

    RUN(testEqualDouble(42, 42, true));
    RUN(testEqualDouble(0, -0, true));
    RUN(testEqualDouble(42, 43, false));
    RUN(testEqualDouble(PNaN, 42, false));
    RUN(testEqualDouble(42, PNaN, false));
    RUN(testEqualDouble(PNaN, PNaN, false));

    addLoadTests(filter, tasks);

    RUN(testSpillGP());
    RUN(testSpillFP());

    RUN(testInt32ToDoublePartialRegisterStall());
    RUN(testInt32ToDoublePartialRegisterWithoutStall());

    RUN(testCallSimple(1, 2));
    RUN(testCallRare(1, 2));
    RUN(testCallRareLive(1, 2, 3));
    RUN(testCallSimplePure(1, 2));
    RUN(testCallFunctionWithHellaArguments());
    RUN(testCallFunctionWithHellaArguments2());
    RUN(testCallFunctionWithHellaArguments3());

    RUN(testReturnDouble(0.0));
    RUN(testReturnDouble(negativeZero()));
    RUN(testReturnDouble(42.5));
    RUN_UNARY(testReturnFloat, floatingPointOperands<float>());

    RUN(testCallSimpleDouble(1, 2));
    RUN(testCallFunctionWithHellaDoubleArguments());
    RUN_BINARY(testCallSimpleFloat, floatingPointOperands<float>(), floatingPointOperands<float>());
    RUN(testCallFunctionWithHellaFloatArguments());

    RUN(testLinearScanWithCalleeOnStack());

    RUN(testChillDiv(4, 2, 2));
    RUN(testChillDiv(1, 0, 0));
    RUN(testChillDiv(0, 0, 0));
    RUN(testChillDiv(1, -1, -1));
    RUN(testChillDiv(-2147483647 - 1, 0, 0));
    RUN(testChillDiv(-2147483647 - 1, 1, -2147483647 - 1));
    RUN(testChillDiv(-2147483647 - 1, -1, -2147483647 - 1));
    RUN(testChillDiv(-2147483647 - 1, 2, -1073741824));
    RUN(testChillDiv64(4, 2, 2));
    RUN(testChillDiv64(1, 0, 0));
    RUN(testChillDiv64(0, 0, 0));
    RUN(testChillDiv64(1, -1, -1));
    RUN(testChillDiv64(-9223372036854775807ll - 1, 0, 0));
    RUN(testChillDiv64(-9223372036854775807ll - 1, 1, -9223372036854775807ll - 1));
    RUN(testChillDiv64(-9223372036854775807ll - 1, -1, -9223372036854775807ll - 1));
    RUN(testChillDiv64(-9223372036854775807ll - 1, 2, -4611686018427387904));
    RUN(testChillDivTwice(4, 2, 6, 2, 5));
    RUN(testChillDivTwice(4, 0, 6, 2, 3));
    RUN(testChillDivTwice(4, 2, 6, 0, 2));

    RUN_UNARY(testModArg, int64Operands());
    RUN_BINARY(testModArgs, int64Operands(), int64Operands());
    RUN_BINARY(testModImms, int64Operands(), int64Operands());
    RUN_UNARY(testModArg32, int32Operands());
    RUN_BINARY(testModArgs32, int32Operands(), int32Operands());
    RUN_BINARY(testModImms32, int32Operands(), int32Operands());
    RUN_UNARY(testChillModArg, int64Operands());
    RUN_BINARY(testChillModArgs, int64Operands(), int64Operands());
    RUN_BINARY(testChillModImms, int64Operands(), int64Operands());
    RUN_UNARY(testChillModArg32, int32Operands());
    RUN_BINARY(testChillModArgs32, int32Operands(), int32Operands());
    RUN_BINARY(testChillModImms32, int32Operands(), int32Operands());

    RUN(testSwitch(0, 1));
    RUN(testSwitch(1, 1));
    RUN(testSwitch(2, 1));
    RUN(testSwitch(2, 2));
    RUN(testSwitch(10, 1));
    RUN(testSwitch(10, 2));
    RUN(testSwitch(100, 1));
    RUN(testSwitch(100, 100));

    RUN(testSwitchSameCaseAsDefault());

    RUN(testSwitchChillDiv(0, 1));
    RUN(testSwitchChillDiv(1, 1));
    RUN(testSwitchChillDiv(2, 1));
    RUN(testSwitchChillDiv(2, 2));
    RUN(testSwitchChillDiv(10, 1));
    RUN(testSwitchChillDiv(10, 2));
    RUN(testSwitchChillDiv(100, 1));
    RUN(testSwitchChillDiv(100, 100));

    RUN(testSwitchTargettingSameBlock());
    RUN(testSwitchTargettingSameBlockFoldPathConstant());

    RUN(testTrunc(0));
    RUN(testTrunc(1));
    RUN(testTrunc(-1));
    RUN(testTrunc(1000000000000ll));
    RUN(testTrunc(-1000000000000ll));
    RUN(testTruncFold(0));
    RUN(testTruncFold(1));
    RUN(testTruncFold(-1));
    RUN(testTruncFold(1000000000000ll));
    RUN(testTruncFold(-1000000000000ll));
    
    RUN(testZExt32(0));
    RUN(testZExt32(1));
    RUN(testZExt32(-1));
    RUN(testZExt32(1000000000ll));
    RUN(testZExt32(-1000000000ll));
    RUN(testZExt32Fold(0));
    RUN(testZExt32Fold(1));
    RUN(testZExt32Fold(-1));
    RUN(testZExt32Fold(1000000000ll));
    RUN(testZExt32Fold(-1000000000ll));

    RUN(testSExt32(0));
    RUN(testSExt32(1));
    RUN(testSExt32(-1));
    RUN(testSExt32(1000000000ll));
    RUN(testSExt32(-1000000000ll));
    RUN(testSExt32Fold(0));
    RUN(testSExt32Fold(1));
    RUN(testSExt32Fold(-1));
    RUN(testSExt32Fold(1000000000ll));
    RUN(testSExt32Fold(-1000000000ll));

    RUN(testTruncZExt32(0));
    RUN(testTruncZExt32(1));
    RUN(testTruncZExt32(-1));
    RUN(testTruncZExt32(1000000000ll));
    RUN(testTruncZExt32(-1000000000ll));
    RUN(testTruncSExt32(0));
    RUN(testTruncSExt32(1));
    RUN(testTruncSExt32(-1));
    RUN(testTruncSExt32(1000000000ll));
    RUN(testTruncSExt32(-1000000000ll));

    RUN(testSExt8(0));
    RUN(testSExt8(1));
    RUN(testSExt8(42));
    RUN(testSExt8(-1));
    RUN(testSExt8(0xff));
    RUN(testSExt8(0x100));
    RUN(testSExt8Fold(0));
    RUN(testSExt8Fold(1));
    RUN(testSExt8Fold(42));
    RUN(testSExt8Fold(-1));
    RUN(testSExt8Fold(0xff));
    RUN(testSExt8Fold(0x100));
    RUN(testSExt8SExt8(0));
    RUN(testSExt8SExt8(1));
    RUN(testSExt8SExt8(42));
    RUN(testSExt8SExt8(-1));
    RUN(testSExt8SExt8(0xff));
    RUN(testSExt8SExt8(0x100));
    RUN(testSExt8SExt16(0));
    RUN(testSExt8SExt16(1));
    RUN(testSExt8SExt16(42));
    RUN(testSExt8SExt16(-1));
    RUN(testSExt8SExt16(0xff));
    RUN(testSExt8SExt16(0x100));
    RUN(testSExt8SExt16(0xffff));
    RUN(testSExt8SExt16(0x10000));
    RUN(testSExt8BitAnd(0, 0));
    RUN(testSExt8BitAnd(1, 0));
    RUN(testSExt8BitAnd(42, 0));
    RUN(testSExt8BitAnd(-1, 0));
    RUN(testSExt8BitAnd(0xff, 0));
    RUN(testSExt8BitAnd(0x100, 0));
    RUN(testSExt8BitAnd(0xffff, 0));
    RUN(testSExt8BitAnd(0x10000, 0));
    RUN(testSExt8BitAnd(0, 0xf));
    RUN(testSExt8BitAnd(1, 0xf));
    RUN(testSExt8BitAnd(42, 0xf));
    RUN(testSExt8BitAnd(-1, 0xf));
    RUN(testSExt8BitAnd(0xff, 0xf));
    RUN(testSExt8BitAnd(0x100, 0xf));
    RUN(testSExt8BitAnd(0xffff, 0xf));
    RUN(testSExt8BitAnd(0x10000, 0xf));
    RUN(testSExt8BitAnd(0, 0xff));
    RUN(testSExt8BitAnd(1, 0xff));
    RUN(testSExt8BitAnd(42, 0xff));
    RUN(testSExt8BitAnd(-1, 0xff));
    RUN(testSExt8BitAnd(0xff, 0xff));
    RUN(testSExt8BitAnd(0x100, 0xff));
    RUN(testSExt8BitAnd(0xffff, 0xff));
    RUN(testSExt8BitAnd(0x10000, 0xff));
    RUN(testSExt8BitAnd(0, 0x80));
    RUN(testSExt8BitAnd(1, 0x80));
    RUN(testSExt8BitAnd(42, 0x80));
    RUN(testSExt8BitAnd(-1, 0x80));
    RUN(testSExt8BitAnd(0xff, 0x80));
    RUN(testSExt8BitAnd(0x100, 0x80));
    RUN(testSExt8BitAnd(0xffff, 0x80));
    RUN(testSExt8BitAnd(0x10000, 0x80));
    RUN(testBitAndSExt8(0, 0xf));
    RUN(testBitAndSExt8(1, 0xf));
    RUN(testBitAndSExt8(42, 0xf));
    RUN(testBitAndSExt8(-1, 0xf));
    RUN(testBitAndSExt8(0xff, 0xf));
    RUN(testBitAndSExt8(0x100, 0xf));
    RUN(testBitAndSExt8(0xffff, 0xf));
    RUN(testBitAndSExt8(0x10000, 0xf));
    RUN(testBitAndSExt8(0, 0xff));
    RUN(testBitAndSExt8(1, 0xff));
    RUN(testBitAndSExt8(42, 0xff));
    RUN(testBitAndSExt8(-1, 0xff));
    RUN(testBitAndSExt8(0xff, 0xff));
    RUN(testBitAndSExt8(0x100, 0xff));
    RUN(testBitAndSExt8(0xffff, 0xff));
    RUN(testBitAndSExt8(0x10000, 0xff));
    RUN(testBitAndSExt8(0, 0xfff));
    RUN(testBitAndSExt8(1, 0xfff));
    RUN(testBitAndSExt8(42, 0xfff));
    RUN(testBitAndSExt8(-1, 0xfff));
    RUN(testBitAndSExt8(0xff, 0xfff));
    RUN(testBitAndSExt8(0x100, 0xfff));
    RUN(testBitAndSExt8(0xffff, 0xfff));
    RUN(testBitAndSExt8(0x10000, 0xfff));

    RUN(testSExt16(0));
    RUN(testSExt16(1));
    RUN(testSExt16(42));
    RUN(testSExt16(-1));
    RUN(testSExt16(0xffff));
    RUN(testSExt16(0x10000));
    RUN(testSExt16Fold(0));
    RUN(testSExt16Fold(1));
    RUN(testSExt16Fold(42));
    RUN(testSExt16Fold(-1));
    RUN(testSExt16Fold(0xffff));
    RUN(testSExt16Fold(0x10000));
    RUN(testSExt16SExt8(0));
    RUN(testSExt16SExt8(1));
    RUN(testSExt16SExt8(42));
    RUN(testSExt16SExt8(-1));
    RUN(testSExt16SExt8(0xffff));
    RUN(testSExt16SExt8(0x10000));
    RUN(testSExt16SExt16(0));
    RUN(testSExt16SExt16(1));
    RUN(testSExt16SExt16(42));
    RUN(testSExt16SExt16(-1));
    RUN(testSExt16SExt16(0xffff));
    RUN(testSExt16SExt16(0x10000));
    RUN(testSExt16SExt16(0xffffff));
    RUN(testSExt16SExt16(0x1000000));
    RUN(testSExt16BitAnd(0, 0));
    RUN(testSExt16BitAnd(1, 0));
    RUN(testSExt16BitAnd(42, 0));
    RUN(testSExt16BitAnd(-1, 0));
    RUN(testSExt16BitAnd(0xffff, 0));
    RUN(testSExt16BitAnd(0x10000, 0));
    RUN(testSExt16BitAnd(0xffffff, 0));
    RUN(testSExt16BitAnd(0x1000000, 0));
    RUN(testSExt16BitAnd(0, 0xf));
    RUN(testSExt16BitAnd(1, 0xf));
    RUN(testSExt16BitAnd(42, 0xf));
    RUN(testSExt16BitAnd(-1, 0xf));
    RUN(testSExt16BitAnd(0xffff, 0xf));
    RUN(testSExt16BitAnd(0x10000, 0xf));
    RUN(testSExt16BitAnd(0xffffff, 0xf));
    RUN(testSExt16BitAnd(0x1000000, 0xf));
    RUN(testSExt16BitAnd(0, 0xffff));
    RUN(testSExt16BitAnd(1, 0xffff));
    RUN(testSExt16BitAnd(42, 0xffff));
    RUN(testSExt16BitAnd(-1, 0xffff));
    RUN(testSExt16BitAnd(0xffff, 0xffff));
    RUN(testSExt16BitAnd(0x10000, 0xffff));
    RUN(testSExt16BitAnd(0xffffff, 0xffff));
    RUN(testSExt16BitAnd(0x1000000, 0xffff));
    RUN(testSExt16BitAnd(0, 0x8000));
    RUN(testSExt16BitAnd(1, 0x8000));
    RUN(testSExt16BitAnd(42, 0x8000));
    RUN(testSExt16BitAnd(-1, 0x8000));
    RUN(testSExt16BitAnd(0xffff, 0x8000));
    RUN(testSExt16BitAnd(0x10000, 0x8000));
    RUN(testSExt16BitAnd(0xffffff, 0x8000));
    RUN(testSExt16BitAnd(0x1000000, 0x8000));
    RUN(testBitAndSExt16(0, 0xf));
    RUN(testBitAndSExt16(1, 0xf));
    RUN(testBitAndSExt16(42, 0xf));
    RUN(testBitAndSExt16(-1, 0xf));
    RUN(testBitAndSExt16(0xffff, 0xf));
    RUN(testBitAndSExt16(0x10000, 0xf));
    RUN(testBitAndSExt16(0xffffff, 0xf));
    RUN(testBitAndSExt16(0x1000000, 0xf));
    RUN(testBitAndSExt16(0, 0xffff));
    RUN(testBitAndSExt16(1, 0xffff));
    RUN(testBitAndSExt16(42, 0xffff));
    RUN(testBitAndSExt16(-1, 0xffff));
    RUN(testBitAndSExt16(0xffff, 0xffff));
    RUN(testBitAndSExt16(0x10000, 0xffff));
    RUN(testBitAndSExt16(0xffffff, 0xffff));
    RUN(testBitAndSExt16(0x1000000, 0xffff));
    RUN(testBitAndSExt16(0, 0xfffff));
    RUN(testBitAndSExt16(1, 0xfffff));
    RUN(testBitAndSExt16(42, 0xfffff));
    RUN(testBitAndSExt16(-1, 0xfffff));
    RUN(testBitAndSExt16(0xffff, 0xfffff));
    RUN(testBitAndSExt16(0x10000, 0xfffff));
    RUN(testBitAndSExt16(0xffffff, 0xfffff));
    RUN(testBitAndSExt16(0x1000000, 0xfffff));

    RUN(testSExt32BitAnd(0, 0));
    RUN(testSExt32BitAnd(1, 0));
    RUN(testSExt32BitAnd(42, 0));
    RUN(testSExt32BitAnd(-1, 0));
    RUN(testSExt32BitAnd(0x80000000, 0));
    RUN(testSExt32BitAnd(0, 0xf));
    RUN(testSExt32BitAnd(1, 0xf));
    RUN(testSExt32BitAnd(42, 0xf));
    RUN(testSExt32BitAnd(-1, 0xf));
    RUN(testSExt32BitAnd(0x80000000, 0xf));
    RUN(testSExt32BitAnd(0, 0x80000000));
    RUN(testSExt32BitAnd(1, 0x80000000));
    RUN(testSExt32BitAnd(42, 0x80000000));
    RUN(testSExt32BitAnd(-1, 0x80000000));
    RUN(testSExt32BitAnd(0x80000000, 0x80000000));
    RUN(testBitAndSExt32(0, 0xf));
    RUN(testBitAndSExt32(1, 0xf));
    RUN(testBitAndSExt32(42, 0xf));
    RUN(testBitAndSExt32(-1, 0xf));
    RUN(testBitAndSExt32(0xffff, 0xf));
    RUN(testBitAndSExt32(0x10000, 0xf));
    RUN(testBitAndSExt32(0xffffff, 0xf));
    RUN(testBitAndSExt32(0x1000000, 0xf));
    RUN(testBitAndSExt32(0, 0xffff00000000llu));
    RUN(testBitAndSExt32(1, 0xffff00000000llu));
    RUN(testBitAndSExt32(42, 0xffff00000000llu));
    RUN(testBitAndSExt32(-1, 0xffff00000000llu));
    RUN(testBitAndSExt32(0x80000000, 0xffff00000000llu));

    RUN(testBasicSelect());
    RUN(testSelectTest());
    RUN(testSelectCompareDouble());
    RUN_BINARY(testSelectCompareFloat, floatingPointOperands<float>(), floatingPointOperands<float>());
    RUN_BINARY(testSelectCompareFloatToDouble, floatingPointOperands<float>(), floatingPointOperands<float>());
    RUN(testSelectDouble());
    RUN(testSelectDoubleTest());
    RUN(testSelectDoubleCompareDouble());
    RUN_BINARY(testSelectDoubleCompareFloat, floatingPointOperands<float>(), floatingPointOperands<float>());
    RUN_BINARY(testSelectFloatCompareFloat, floatingPointOperands<float>(), floatingPointOperands<float>());
    RUN(testSelectDoubleCompareDoubleWithAliasing());
    RUN(testSelectFloatCompareFloatWithAliasing());
    RUN(testSelectFold(42));
    RUN(testSelectFold(43));
    RUN(testSelectInvert());
    RUN(testCheckSelect());
    RUN(testCheckSelectCheckSelect());
    RUN(testCheckSelectAndCSE());
    RUN_BINARY(testPowDoubleByIntegerLoop, floatingPointOperands<double>(), int64Operands());

    RUN(testTruncOrHigh());
    RUN(testTruncOrLow());
    RUN(testBitAndOrHigh());
    RUN(testBitAndOrLow());

    RUN(testBranch64Equal(0, 0));
    RUN(testBranch64Equal(1, 1));
    RUN(testBranch64Equal(-1, -1));
    RUN(testBranch64Equal(1, -1));
    RUN(testBranch64Equal(-1, 1));
    RUN(testBranch64EqualImm(0, 0));
    RUN(testBranch64EqualImm(1, 1));
    RUN(testBranch64EqualImm(-1, -1));
    RUN(testBranch64EqualImm(1, -1));
    RUN(testBranch64EqualImm(-1, 1));
    RUN(testBranch64EqualMem(0, 0));
    RUN(testBranch64EqualMem(1, 1));
    RUN(testBranch64EqualMem(-1, -1));
    RUN(testBranch64EqualMem(1, -1));
    RUN(testBranch64EqualMem(-1, 1));
    RUN(testBranch64EqualMemImm(0, 0));
    RUN(testBranch64EqualMemImm(1, 1));
    RUN(testBranch64EqualMemImm(-1, -1));
    RUN(testBranch64EqualMemImm(1, -1));
    RUN(testBranch64EqualMemImm(-1, 1));

    RUN(testStore8Load8Z(0));
    RUN(testStore8Load8Z(123));
    RUN(testStore8Load8Z(12345));
    RUN(testStore8Load8Z(-123));

    RUN(testStore16Load16Z(0));
    RUN(testStore16Load16Z(123));
    RUN(testStore16Load16Z(12345));
    RUN(testStore16Load16Z(12345678));
    RUN(testStore16Load16Z(-123));

    RUN(testSShrShl32(42, 24, 24));
    RUN(testSShrShl32(-42, 24, 24));
    RUN(testSShrShl32(4200, 24, 24));
    RUN(testSShrShl32(-4200, 24, 24));
    RUN(testSShrShl32(4200000, 24, 24));
    RUN(testSShrShl32(-4200000, 24, 24));

    RUN(testSShrShl32(42, 16, 16));
    RUN(testSShrShl32(-42, 16, 16));
    RUN(testSShrShl32(4200, 16, 16));
    RUN(testSShrShl32(-4200, 16, 16));
    RUN(testSShrShl32(4200000, 16, 16));
    RUN(testSShrShl32(-4200000, 16, 16));

    RUN(testSShrShl32(42, 8, 8));
    RUN(testSShrShl32(-42, 8, 8));
    RUN(testSShrShl32(4200, 8, 8));
    RUN(testSShrShl32(-4200, 8, 8));
    RUN(testSShrShl32(4200000, 8, 8));
    RUN(testSShrShl32(-4200000, 8, 8));
    RUN(testSShrShl32(420000000, 8, 8));
    RUN(testSShrShl32(-420000000, 8, 8));

    RUN(testSShrShl64(42, 56, 56));
    RUN(testSShrShl64(-42, 56, 56));
    RUN(testSShrShl64(4200, 56, 56));
    RUN(testSShrShl64(-4200, 56, 56));
    RUN(testSShrShl64(4200000, 56, 56));
    RUN(testSShrShl64(-4200000, 56, 56));
    RUN(testSShrShl64(420000000, 56, 56));
    RUN(testSShrShl64(-420000000, 56, 56));
    RUN(testSShrShl64(42000000000, 56, 56));
    RUN(testSShrShl64(-42000000000, 56, 56));

    RUN(testSShrShl64(42, 48, 48));
    RUN(testSShrShl64(-42, 48, 48));
    RUN(testSShrShl64(4200, 48, 48));
    RUN(testSShrShl64(-4200, 48, 48));
    RUN(testSShrShl64(4200000, 48, 48));
    RUN(testSShrShl64(-4200000, 48, 48));
    RUN(testSShrShl64(420000000, 48, 48));
    RUN(testSShrShl64(-420000000, 48, 48));
    RUN(testSShrShl64(42000000000, 48, 48));
    RUN(testSShrShl64(-42000000000, 48, 48));

    RUN(testSShrShl64(42, 32, 32));
    RUN(testSShrShl64(-42, 32, 32));
    RUN(testSShrShl64(4200, 32, 32));
    RUN(testSShrShl64(-4200, 32, 32));
    RUN(testSShrShl64(4200000, 32, 32));
    RUN(testSShrShl64(-4200000, 32, 32));
    RUN(testSShrShl64(420000000, 32, 32));
    RUN(testSShrShl64(-420000000, 32, 32));
    RUN(testSShrShl64(42000000000, 32, 32));
    RUN(testSShrShl64(-42000000000, 32, 32));

    RUN(testSShrShl64(42, 24, 24));
    RUN(testSShrShl64(-42, 24, 24));
    RUN(testSShrShl64(4200, 24, 24));
    RUN(testSShrShl64(-4200, 24, 24));
    RUN(testSShrShl64(4200000, 24, 24));
    RUN(testSShrShl64(-4200000, 24, 24));
    RUN(testSShrShl64(420000000, 24, 24));
    RUN(testSShrShl64(-420000000, 24, 24));
    RUN(testSShrShl64(42000000000, 24, 24));
    RUN(testSShrShl64(-42000000000, 24, 24));

    RUN(testSShrShl64(42, 16, 16));
    RUN(testSShrShl64(-42, 16, 16));
    RUN(testSShrShl64(4200, 16, 16));
    RUN(testSShrShl64(-4200, 16, 16));
    RUN(testSShrShl64(4200000, 16, 16));
    RUN(testSShrShl64(-4200000, 16, 16));
    RUN(testSShrShl64(420000000, 16, 16));
    RUN(testSShrShl64(-420000000, 16, 16));
    RUN(testSShrShl64(42000000000, 16, 16));
    RUN(testSShrShl64(-42000000000, 16, 16));

    RUN(testSShrShl64(42, 8, 8));
    RUN(testSShrShl64(-42, 8, 8));
    RUN(testSShrShl64(4200, 8, 8));
    RUN(testSShrShl64(-4200, 8, 8));
    RUN(testSShrShl64(4200000, 8, 8));
    RUN(testSShrShl64(-4200000, 8, 8));
    RUN(testSShrShl64(420000000, 8, 8));
    RUN(testSShrShl64(-420000000, 8, 8));
    RUN(testSShrShl64(42000000000, 8, 8));
    RUN(testSShrShl64(-42000000000, 8, 8));

    RUN(testCheckMul64SShr());

    RUN_BINARY(testRotR, int32Operands(), int32Operands());
    RUN_BINARY(testRotR, int64Operands(), int32Operands());
    RUN_BINARY(testRotL, int32Operands(), int32Operands());
    RUN_BINARY(testRotL, int64Operands(), int32Operands());

    RUN_BINARY(testRotRWithImmShift, int32Operands(), int32Operands());
    RUN_BINARY(testRotRWithImmShift, int64Operands(), int32Operands());
    RUN_BINARY(testRotLWithImmShift, int32Operands(), int32Operands());
    RUN_BINARY(testRotLWithImmShift, int64Operands(), int32Operands());

    RUN(testComputeDivisionMagic<int32_t>(2, -2147483647, 0));
    RUN(testTrivialInfiniteLoop());
    RUN(testFoldPathEqual());
    
    RUN(testRShiftSelf32());
    RUN(testURShiftSelf32());
    RUN(testLShiftSelf32());
    RUN(testRShiftSelf64());
    RUN(testURShiftSelf64());
    RUN(testLShiftSelf64());

    RUN(testPatchpointDoubleRegs());
    RUN(testSpillDefSmallerThanUse());
    RUN(testSpillUseLargerThanDef());
    RUN(testLateRegister());
    RUN(testInterpreter());
    RUN(testReduceStrengthCheckBottomUseInAnotherBlock());
    RUN(testResetReachabilityDanglingReference());
    
    RUN(testEntrySwitchSimple());
    RUN(testEntrySwitchNoEntrySwitch());
    RUN(testEntrySwitchWithCommonPaths());
    RUN(testEntrySwitchWithCommonPathsAndNonTrivialEntrypoint());
    RUN(testEntrySwitchLoop());

    RUN(testSomeEarlyRegister());
    RUN(testPatchpointTerminalReturnValue(true));
    RUN(testPatchpointTerminalReturnValue(false));
    RUN(testTerminalPatchpointThatNeedsToBeSpilled());

    RUN(testMemoryFence());
    RUN(testStoreFence());
    RUN(testLoadFence());
    RUN(testTrappingLoad());
    RUN(testTrappingStore());
    RUN(testTrappingLoadAddStore());
    RUN(testTrappingLoadDCE());
    RUN(testTrappingStoreElimination());
    RUN(testMoveConstants());
    RUN(testPCOriginMapDoesntInsertNops());
    RUN(testPinRegisters());
    RUN(testReduceStrengthReassociation(true));
    RUN(testReduceStrengthReassociation(false));
    RUN(testAddShl32());
    RUN(testAddShl64());
    RUN(testAddShl65());
    RUN(testLoadBaseIndexShift2());
    RUN(testLoadBaseIndexShift32());
    RUN(testOptimizeMaterialization());
    RUN(testLICMPure());
    RUN(testLICMPureSideExits());
    RUN(testLICMPureWritesPinned());
    RUN(testLICMPureWrites());
    RUN(testLICMReadsLocalState());
    RUN(testLICMReadsPinned());
    RUN(testLICMReads());
    RUN(testLICMPureNotBackwardsDominant());
    RUN(testLICMPureFoiledByChild());
    RUN(testLICMPureNotBackwardsDominantFoiledByChild());
    RUN(testLICMExitsSideways());
    RUN(testLICMWritesLocalState());
    RUN(testLICMWrites());
    RUN(testLICMWritesPinned());
    RUN(testLICMFence());
    RUN(testLICMControlDependent());
    RUN(testLICMControlDependentNotBackwardsDominant());
    RUN(testLICMControlDependentSideExits());
    RUN(testLICMReadsPinnedWritesPinned());
    RUN(testLICMReadsWritesDifferentHeaps());
    RUN(testLICMReadsWritesOverlappingHeaps());
    RUN(testLICMDefaultCall());

    addAtomicTests(filter, tasks);
    RUN(testDepend32());
    RUN(testDepend64());

    RUN(testWasmBoundsCheck(0));
    RUN(testWasmBoundsCheck(100));
    RUN(testWasmBoundsCheck(10000));
    RUN(testWasmBoundsCheck(std::numeric_limits<unsigned>::max() - 5));

    RUN(testWasmAddress());
    
    RUN(testFastTLSLoad());
    RUN(testFastTLSStore());

    RUN(testDoubleLiteralComparison(bitwise_cast<double>(0x8000000000000001ull), bitwise_cast<double>(0x0000000000000000ull)));
    RUN(testDoubleLiteralComparison(bitwise_cast<double>(0x0000000000000000ull), bitwise_cast<double>(0x8000000000000001ull)));
    RUN(testDoubleLiteralComparison(125.3144446948241, 125.3144446948242));
    RUN(testDoubleLiteralComparison(125.3144446948242, 125.3144446948241));

    RUN(testFloatEqualOrUnorderedFolding());
    RUN(testFloatEqualOrUnorderedFoldingNaN());
    RUN(testFloatEqualOrUnorderedDontFold());
    
    RUN(testShuffleDoesntTrashCalleeSaves());
    RUN(testDemotePatchpointTerminal());

    RUN(testLoopWithMultipleHeaderEdges());

    RUN(testInfiniteLoopDoesntCauseBadHoisting());

    if (isX86()) {
        RUN(testBranchBitAndImmFusion(Identity, Int64, 1, Air::BranchTest32, Air::Arg::Tmp));
        RUN(testBranchBitAndImmFusion(Identity, Int64, 0xff, Air::BranchTest32, Air::Arg::Tmp));
        RUN(testBranchBitAndImmFusion(Trunc, Int32, 1, Air::BranchTest32, Air::Arg::Tmp));
        RUN(testBranchBitAndImmFusion(Trunc, Int32, 0xff, Air::BranchTest32, Air::Arg::Tmp));
        RUN(testBranchBitAndImmFusion(Load8S, Int32, 1, Air::BranchTest8, Air::Arg::Addr));
        RUN(testBranchBitAndImmFusion(Load8Z, Int32, 1, Air::BranchTest8, Air::Arg::Addr));
        RUN(testBranchBitAndImmFusion(Load, Int32, 1, Air::BranchTest32, Air::Arg::Addr));
        RUN(testBranchBitAndImmFusion(Load, Int64, 1, Air::BranchTest32, Air::Arg::Addr));
        RUN(testX86LeaAddAddShlLeft());
        RUN(testX86LeaAddAddShlRight());
        RUN(testX86LeaAddAdd());
        RUN(testX86LeaAddShlRight());
        RUN(testX86LeaAddShlLeftScale1());
        RUN(testX86LeaAddShlLeftScale2());
        RUN(testX86LeaAddShlLeftScale4());
        RUN(testX86LeaAddShlLeftScale8());
    }

    if (isARM64()) {
        RUN(testTernarySubInstructionSelection(Identity, Int64, Air::Sub64));
        RUN(testTernarySubInstructionSelection(Trunc, Int32, Air::Sub32));
    }

    RUN(testReportUsedRegistersLateUseFollowedByEarlyDefDoesNotMarkUseAsDead());

    if (tasks.isEmpty())
        usage();

    Lock lock;

    Vector<Ref<Thread>> threads;
    for (unsigned i = filter ? 1 : WTF::numberOfProcessorCores(); i--;) {
        threads.append(
            Thread::create(
                "testb3 thread",
                [&] () {
                    for (;;) {
                        RefPtr<SharedTask<void()>> task;
                        {
                            LockHolder locker(lock);
                            if (tasks.isEmpty())
                                return;
                            task = tasks.takeFirst();
                        }

                        task->run();
                    }
                }));
    }

    for (auto& thread : threads)
        thread->waitForCompletion();
    crashLock.lock();
    crashLock.unlock();
}

#else // ENABLE(B3_JIT)

static void run(const char*)
{
    dataLog("B3 JIT is not enabled.\n");
}

#endif // ENABLE(B3_JIT)

int main(int argc, char** argv)
{
    const char* filter = nullptr;
    switch (argc) {
    case 1:
        break;
    case 2:
        filter = argv[1];
        break;
    default:
        usage();
        break;
    }

    JSC::initializeThreading();
    
    for (unsigned i = 0; i <= 2; ++i) {
        JSC::Options::defaultB3OptLevel() = i;
        run(filter);
    }

    return 0;
}

#if OS(WINDOWS)
extern "C" __declspec(dllexport) int WINAPI dllLauncherEntryPoint(int argc, const char* argv[])
{
    return main(argc, const_cast<char**>(argv));
}
#endif
