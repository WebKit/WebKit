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

bool shouldRun(const char* filter, const char* testName)
{
    // FIXME: These tests fail <https://bugs.webkit.org/show_bug.cgi?id=199330>.
    if (!filter && isARM64()) {
        for (auto& failingTest : {
            "testNegFloatWithUselessDoubleConversion",
            "testPinRegisters",
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

    RUN_UNARY(testAddTreeArg32, int32Operands());
    RUN_UNARY(testMulTreeArg32, int32Operands());

    addArgTests(filter, tasks);

    RUN_UNARY(testNegDouble, floatingPointOperands<double>());
    RUN_UNARY(testNegFloat, floatingPointOperands<float>());
    RUN_UNARY(testNegFloatWithUselessDoubleConversion, floatingPointOperands<float>());

    addBitTests(filter, tasks);

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

    addShrTests(filter, tasks);

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

    RUN_UNARY(testCheckAddRemoveCheckWithSExt8, int8Operands());
    RUN_UNARY(testCheckAddRemoveCheckWithSExt16, int16Operands());
    RUN_UNARY(testCheckAddRemoveCheckWithSExt32, int32Operands());
    RUN_UNARY(testCheckAddRemoveCheckWithZExt32, int32Operands());

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
    RUN(testPatchpointManyWarmAnyImms());
    RUN(testPatchpointManyColdAnyImms());
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
    RUN(testCheckSubBitAnd());
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
    addTupleTests(filter, tasks);

    addCopyTests(filter, tasks);

    RUN(testSpillGP());
    RUN(testSpillFP());

    RUN(testInt32ToDoublePartialRegisterStall());
    RUN(testInt32ToDoublePartialRegisterWithoutStall());

    addCallTests(filter, tasks);

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

    addSExtTests(filter, tasks);

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

    addSShrShTests(filter, tasks);

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

    // FIXME: Re-enable B3 hoistLoopInvariantValues
    // https://bugs.webkit.org/show_bug.cgi?id=212651
    Options::useB3HoistLoopInvariantValues() = true;

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

    JSC::Config::configureForTesting();

    WTF::initializeMainThread();
    JSC::initialize();
    
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
