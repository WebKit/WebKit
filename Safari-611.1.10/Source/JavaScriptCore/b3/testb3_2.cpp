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

void test42()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* const42 = root->appendNew<Const32Value>(proc, Origin(), 42);
    root->appendNewControlValue(proc, Return, Origin(), const42);

    CHECK(compileAndRun<int>(proc) == 42);
}

void testLoad42()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    int x = 42;
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<MemoryValue>(
            proc, Load, Int32, Origin(),
            root->appendNew<ConstPtrValue>(proc, Origin(), &x)));

    CHECK(compileAndRun<int>(proc) == 42);
}

void testLoadAcq42()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    int x = 42;
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<MemoryValue>(
            proc, Load, Int32, Origin(),
            root->appendNew<ConstPtrValue>(proc, Origin(), &x),
            0, HeapRange(42), HeapRange(42)));

    auto code = compileProc(proc);
    if (isARM64())
        checkUsesInstruction(*code, "lda");
    CHECK(invoke<int>(*code) == 42);
}

void testLoadWithOffsetImpl(int32_t offset64, int32_t offset32)
{
    {
        Procedure proc;
        BasicBlock* root = proc.addBlock();
        int64_t x = -42;
        Value* base = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
        root->appendNewControlValue(
            proc, Return, Origin(),
            root->appendNew<MemoryValue>(
                proc, Load, Int64, Origin(),
                base,
                offset64));

        char* address = reinterpret_cast<char*>(&x) - offset64;
        CHECK(compileAndRun<int64_t>(proc, address) == -42);
    }
    {
        Procedure proc;
        BasicBlock* root = proc.addBlock();
        int32_t x = -42;
        Value* base = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
        root->appendNewControlValue(
            proc, Return, Origin(),
            root->appendNew<MemoryValue>(
                proc, Load, Int32, Origin(),
                base,
                offset32));

        char* address = reinterpret_cast<char*>(&x) - offset32;
        CHECK(compileAndRun<int32_t>(proc, address) == -42);
    }
}

void testLoadOffsetImm9Max()
{
    testLoadWithOffsetImpl(255, 255);
}

void testLoadOffsetImm9MaxPlusOne()
{
    testLoadWithOffsetImpl(256, 256);
}

void testLoadOffsetImm9MaxPlusTwo()
{
    testLoadWithOffsetImpl(257, 257);
}

void testLoadOffsetImm9Min()
{
    testLoadWithOffsetImpl(-256, -256);
}

void testLoadOffsetImm9MinMinusOne()
{
    testLoadWithOffsetImpl(-257, -257);
}

void testLoadOffsetScaledUnsignedImm12Max()
{
    testLoadWithOffsetImpl(32760, 16380);
}

void testLoadOffsetScaledUnsignedOverImm12Max()
{
    testLoadWithOffsetImpl(32760, 32760);
    testLoadWithOffsetImpl(32761, 16381);
    testLoadWithOffsetImpl(32768, 16384);
}

static void testBitXorTreeArgs(int64_t a, int64_t b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argA = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    Value* argB = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
    Value* node = root->appendNew<Value>(proc, BitXor, Origin(), argA, argB);
    node = root->appendNew<Value>(proc, BitXor, Origin(), node, argB);
    node = root->appendNew<Value>(proc, BitXor, Origin(), node, argA);
    node = root->appendNew<Value>(proc, BitXor, Origin(), node, argB);
    root->appendNew<Value>(proc, Return, Origin(), node);

    CHECK_EQ(compileAndRun<int64_t>(proc, a, b), (((a ^ b) ^ b) ^ a) ^ b);
}

static void testBitXorTreeArgsEven(int64_t a, int64_t b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argA = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    Value* argB = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
    Value* node = root->appendNew<Value>(proc, BitXor, Origin(), argA, argB);
    node = root->appendNew<Value>(proc, BitXor, Origin(), node, argB);
    node = root->appendNew<Value>(proc, BitXor, Origin(), node, argA);
    root->appendNew<Value>(proc, Return, Origin(), node);

    CHECK_EQ(compileAndRun<int64_t>(proc, a, b), ((a ^ b) ^ b) ^ a);
}

static void testBitXorTreeArgImm(int64_t a, int64_t b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argA = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    Value* immB = root->appendNew<Const64Value>(proc, Origin(), b);
    Value* node = root->appendNew<Value>(proc, BitXor, Origin(), argA, immB);
    node = root->appendNew<Value>(proc, BitXor, Origin(), argA, node);
    node = root->appendNew<Value>(proc, BitXor, Origin(), argA, node);
    node = root->appendNew<Value>(proc, BitXor, Origin(), immB, node);
    root->appendNew<Value>(proc, Return, Origin(), node);

    CHECK_EQ(compileAndRun<int64_t>(proc, a), b ^ (a ^ (a ^ (a ^ b))));
}

void testAddTreeArg32(int32_t a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argA = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    argA = root->appendNew<Value>(proc, Trunc, Origin(), argA);
    Value* node = argA;
    int32_t expectedResult = a;
    for (unsigned i = 0; i < 20; ++i) {
        Value* otherNode;
        if (!(i % 3)) {
            otherNode = root->appendNew<Const32Value>(proc, Origin(), i);
            expectedResult += i;
        } else {
            otherNode = argA;
            expectedResult += a;
        }
        node = root->appendNew<Value>(proc, Add, Origin(), node, otherNode);
    }
    root->appendNew<Value>(proc, Return, Origin(), node);

    CHECK_EQ(compileAndRun<int32_t>(proc, a), expectedResult);
}

void testMulTreeArg32(int32_t a)
{
    // Fibonacci-like expression tree with multiplication instead of addition.
    // Verifies that we don't explode on heavily factored graphs.
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argA = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    argA = root->appendNew<Value>(proc, Trunc, Origin(), argA);
    Value* nodeA = argA;
    Value* nodeB = argA;
    int32_t expectedA = a, expectedResult = a;
    for (unsigned i = 0; i < 20; ++i) {
        Value* newNodeB = root->appendNew<Value>(proc, Mul, Origin(), nodeA, nodeB);
        nodeA = nodeB;
        nodeB = newNodeB;
        int32_t newExpectedResult = expectedA * expectedResult;
        expectedA = expectedResult;
        expectedResult = newExpectedResult;
    }
    root->appendNew<Value>(proc, Return, Origin(), nodeB);

    CHECK_EQ(compileAndRun<int32_t>(proc, a), expectedResult);
}

static void testBitAndTreeArg32(int32_t a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argA = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    argA = root->appendNew<Value>(proc, Trunc, Origin(), argA);
    Value* node = argA;
    for (unsigned i = 0; i < 8; ++i) {
        Value* constI = root->appendNew<Const32Value>(proc, Origin(), i | 42);
        Value* newBitAnd = root->appendNew<Value>(proc, BitAnd, Origin(), argA, constI);
        node = root->appendNew<Value>(proc, BitAnd, Origin(), node, newBitAnd);
    }
    root->appendNew<Value>(proc, Return, Origin(), node);

    CHECK_EQ(compileAndRun<int32_t>(proc, a), a & 42);
}

static void testBitOrTreeArg32(int32_t a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argA = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    argA = root->appendNew<Value>(proc, Trunc, Origin(), argA);
    Value* node = argA;
    for (unsigned i = 0; i < 8; ++i) {
        Value* constI = root->appendNew<Const32Value>(proc, Origin(), i);
        Value* newBitAnd = root->appendNew<Value>(proc, BitOr, Origin(), argA, constI);
        node = root->appendNew<Value>(proc, BitOr, Origin(), node, newBitAnd);
    }
    root->appendNew<Value>(proc, Return, Origin(), node);

    CHECK_EQ(compileAndRun<int32_t>(proc, a), a | 7);
}

void testArg(int argument)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));

    CHECK(compileAndRun<int>(proc, argument) == argument);
}

void testReturnConst64(int64_t value)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Const64Value>(proc, Origin(), value));

    CHECK(compileAndRun<int64_t>(proc) == value);
}

void testReturnVoid()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(proc, Return, Origin());
    compileAndRun<void>(proc);
}

void testAddArg(int a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* value = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(proc, Add, Origin(), value, value));

    CHECK(compileAndRun<int>(proc, a) == a + a);
}

void testAddArgs(int a, int b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, Add, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1)));

    CHECK(compileAndRun<int>(proc, a, b) == a + b);
}

void testAddArgImm(int a, int b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, Add, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
            root->appendNew<Const64Value>(proc, Origin(), b)));

    CHECK(compileAndRun<int>(proc, a) == a + b);
}

void testAddImmArg(int a, int b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, Add, Origin(),
            root->appendNew<Const64Value>(proc, Origin(), a),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)));

    CHECK(compileAndRun<int>(proc, b) == a + b);
}

void testAddArgMem(int64_t a, int64_t b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* address = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
    MemoryValue* load = root->appendNew<MemoryValue>(proc, Load, Int64, Origin(), address);
    Value* result = root->appendNew<Value>(proc, Add, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
        load);
    root->appendNew<MemoryValue>(proc, Store, Origin(), result, address);
    root->appendNewControlValue(proc, Return, Origin(), root->appendNew<Const32Value>(proc, Origin(), 0));

    int64_t inputOutput = b;
    CHECK(!compileAndRun<int64_t>(proc, a, &inputOutput));
    CHECK(inputOutput == a + b);
}

void testAddMemArg(int64_t a, int64_t b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* address = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    MemoryValue* load = root->appendNew<MemoryValue>(proc, Load, Int64, Origin(), address);
    Value* result = root->appendNew<Value>(proc, Add, Origin(),
        load,
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1));
    root->appendNewControlValue(proc, Return, Origin(), result);

    CHECK(compileAndRun<int64_t>(proc, &a, b) == a + b);
}

void testAddImmMem(int64_t a, int64_t b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* address = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    MemoryValue* load = root->appendNew<MemoryValue>(proc, Load, Int64, Origin(), address);
    Value* result = root->appendNew<Value>(proc, Add, Origin(),
        root->appendNew<Const64Value>(proc, Origin(), a),
        load);
    root->appendNew<MemoryValue>(proc, Store, Origin(), result, address);
    root->appendNewControlValue(proc, Return, Origin(), root->appendNew<Const32Value>(proc, Origin(), 0));

    int64_t inputOutput = b;
    CHECK(!compileAndRun<int>(proc, &inputOutput));
    CHECK(inputOutput == a + b);
}

void testAddArg32(int a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* value = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(proc, Add, Origin(), value, value));

    CHECK(compileAndRun<int>(proc, a) == a + a);
}

void testAddArgs32(int a, int b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, Add, Origin(),
            root->appendNew<Value>(
                proc, Trunc, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)),
            root->appendNew<Value>(
                proc, Trunc, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1))));

    CHECK(compileAndRun<int>(proc, a, b) == a + b);
}

void testAddArgMem32(int32_t a, int32_t b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* address = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
    MemoryValue* load = root->appendNew<MemoryValue>(proc, Load, Int32, Origin(), address);
    Value* argument = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* result = root->appendNew<Value>(proc, Add, Origin(), argument, load);
    root->appendNew<MemoryValue>(proc, Store, Origin(), result, address);
    root->appendNewControlValue(proc, Return, Origin(), root->appendNew<Const32Value>(proc, Origin(), 0));

    int32_t inputOutput = b;
    CHECK(!compileAndRun<int32_t>(proc, a, &inputOutput));
    CHECK(inputOutput == a + b);
}

void testAddMemArg32(int32_t a, int32_t b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* address = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    MemoryValue* load = root->appendNew<MemoryValue>(proc, Load, Int32, Origin(), address);
    Value* argument = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1));
    Value* result = root->appendNew<Value>(proc, Add, Origin(), load, argument);
    root->appendNewControlValue(proc, Return, Origin(), result);

    CHECK(compileAndRun<int32_t>(proc, &a, b) == a + b);
}

void testAddImmMem32(int32_t a, int32_t b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* address = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    MemoryValue* load = root->appendNew<MemoryValue>(proc, Load, Int32, Origin(), address);
    Value* result = root->appendNew<Value>(proc, Add, Origin(),
        root->appendNew<Const32Value>(proc, Origin(), a),
        load);
    root->appendNew<MemoryValue>(proc, Store, Origin(), result, address);
    root->appendNewControlValue(proc, Return, Origin(), root->appendNew<Const32Value>(proc, Origin(), 0));

    int32_t inputOutput = b;
    CHECK(!compileAndRun<int>(proc, &inputOutput));
    CHECK(inputOutput == a + b);
}

void testAddNeg1(int a, int b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, Add, Origin(),
            root->appendNew<Value>(proc, Neg, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1)));

    CHECK(compileAndRun<int>(proc, a, b) == (- a) + b);
}

void testAddNeg2(int a, int b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, Add, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
            root->appendNew<Value>(proc, Neg, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1))));

    CHECK(compileAndRun<int>(proc, a, b) == a + (- b));
}

void testAddArgZeroImmZDef()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* arg = root->appendNew<Value>(
        proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* constZero = root->appendNew<Const32Value>(proc, Origin(), 0);
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, Add, Origin(),
            arg,
            constZero));

    auto code = compileProc(proc, 0);
    CHECK(invoke<int64_t>(*code, 0x0123456789abcdef) == 0x89abcdef);
}

void testAddLoadTwice()
{
    auto test = [&] () {
        Procedure proc;
        BasicBlock* root = proc.addBlock();
        int32_t value = 42;
        Value* load = root->appendNew<MemoryValue>(
            proc, Load, Int32, Origin(),
            root->appendNew<ConstPtrValue>(proc, Origin(), &value));
        root->appendNewControlValue(
            proc, Return, Origin(),
            root->appendNew<Value>(proc, Add, Origin(), load, load));

        auto code = compileProc(proc);
        CHECK(invoke<int32_t>(*code) == 42 * 2);
    };

    test();
}

void testAddArgDouble(double a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* value = root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR0);
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(proc, Add, Origin(), value, value));

    CHECK(isIdentical(compileAndRun<double>(proc, a), a + a));
}

void testAddArgsDouble(double a, double b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* valueA = root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR0);
    Value* valueB = root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR1);
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(proc, Add, Origin(), valueA, valueB));

    CHECK(isIdentical(compileAndRun<double>(proc, a, b), a + b));
}

void testAddArgImmDouble(double a, double b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* valueA = root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR0);
    Value* valueB = root->appendNew<ConstDoubleValue>(proc, Origin(), b);
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(proc, Add, Origin(), valueA, valueB));

    CHECK(isIdentical(compileAndRun<double>(proc, a), a + b));
}

void testAddImmArgDouble(double a, double b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* valueA = root->appendNew<ConstDoubleValue>(proc, Origin(), a);
    Value* valueB = root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR0);
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(proc, Add, Origin(), valueA, valueB));

    CHECK(isIdentical(compileAndRun<double>(proc, b), a + b));
}

void testAddImmsDouble(double a, double b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* valueA = root->appendNew<ConstDoubleValue>(proc, Origin(), a);
    Value* valueB = root->appendNew<ConstDoubleValue>(proc, Origin(), b);
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(proc, Add, Origin(), valueA, valueB));

    CHECK(isIdentical(compileAndRun<double>(proc), a + b));
}

void testAddArgFloat(float a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argument32 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* floatValue = root->appendNew<Value>(proc, BitwiseCast, Origin(), argument32);
    Value* result = root->appendNew<Value>(proc, Add, Origin(), floatValue, floatValue);
    Value* result32 = root->appendNew<Value>(proc, BitwiseCast, Origin(), result);
    root->appendNewControlValue(proc, Return, Origin(), result32);


    CHECK(isIdentical(compileAndRun<int32_t>(proc, bitwise_cast<int32_t>(a)), bitwise_cast<int32_t>(a + a)));
}

void testAddArgsFloat(float a, float b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argument1int32 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* argument2int32 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1));
    Value* floatValue1 = root->appendNew<Value>(proc, BitwiseCast, Origin(), argument1int32);
    Value* floatValue2 = root->appendNew<Value>(proc, BitwiseCast, Origin(), argument2int32);
    Value* result = root->appendNew<Value>(proc, Add, Origin(), floatValue1, floatValue2);
    Value* result32 = root->appendNew<Value>(proc, BitwiseCast, Origin(), result);
    root->appendNewControlValue(proc, Return, Origin(), result32);

    CHECK(isIdentical(compileAndRun<int32_t>(proc, bitwise_cast<int32_t>(a), bitwise_cast<int32_t>(b)), bitwise_cast<int32_t>(a + b)));
}

void testAddFPRArgsFloat(float a, float b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argument1 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR0));
    Value* argument2 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR1));
    Value* result = root->appendNew<Value>(proc, Add, Origin(), argument1, argument2);
    root->appendNewControlValue(proc, Return, Origin(), result);

    CHECK(isIdentical(compileAndRun<float>(proc, a, b), a + b));
}

void testAddArgImmFloat(float a, float b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argument32 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* floatValue = root->appendNew<Value>(proc, BitwiseCast, Origin(), argument32);
    Value* constValue = root->appendNew<ConstFloatValue>(proc, Origin(), b);
    Value* result = root->appendNew<Value>(proc, Add, Origin(), floatValue, constValue);
    Value* result32 = root->appendNew<Value>(proc, BitwiseCast, Origin(), result);
    root->appendNewControlValue(proc, Return, Origin(), result32);

    CHECK(isIdentical(compileAndRun<int32_t>(proc, bitwise_cast<int32_t>(a)), bitwise_cast<int32_t>(a + b)));
}

void testAddImmArgFloat(float a, float b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argument32 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* floatValue = root->appendNew<Value>(proc, BitwiseCast, Origin(), argument32);
    Value* constValue = root->appendNew<ConstFloatValue>(proc, Origin(), a);
    Value* result = root->appendNew<Value>(proc, Add, Origin(), constValue, floatValue);
    Value* result32 = root->appendNew<Value>(proc, BitwiseCast, Origin(), result);
    root->appendNewControlValue(proc, Return, Origin(), result32);

    CHECK(isIdentical(compileAndRun<int32_t>(proc, bitwise_cast<int32_t>(b)), bitwise_cast<int32_t>(a + b)));
}

void testAddImmsFloat(float a, float b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* constValue1 = root->appendNew<ConstFloatValue>(proc, Origin(), a);
    Value* constValue2 = root->appendNew<ConstFloatValue>(proc, Origin(), b);
    Value* result = root->appendNew<Value>(proc, Add, Origin(), constValue1, constValue2);
    Value* result32 = root->appendNew<Value>(proc, BitwiseCast, Origin(), result);
    root->appendNewControlValue(proc, Return, Origin(), result32);

    CHECK(isIdentical(compileAndRun<int32_t>(proc), bitwise_cast<int32_t>(a + b)));
}

void testAddArgFloatWithUselessDoubleConversion(float a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argumentInt32 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* floatValue = root->appendNew<Value>(proc, BitwiseCast, Origin(), argumentInt32);
    Value* asDouble = root->appendNew<Value>(proc, FloatToDouble, Origin(), floatValue);
    Value* result = root->appendNew<Value>(proc, Add, Origin(), asDouble, asDouble);
    Value* floatResult = root->appendNew<Value>(proc, DoubleToFloat, Origin(), result);
    Value* result32 = root->appendNew<Value>(proc, BitwiseCast, Origin(), floatResult);
    root->appendNewControlValue(proc, Return, Origin(), result32);

    CHECK(isIdentical(compileAndRun<int32_t>(proc, bitwise_cast<int32_t>(a)), bitwise_cast<int32_t>(a + a)));
}

void testAddArgsFloatWithUselessDoubleConversion(float a, float b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argument1int32 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* argument2int32 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1));
    Value* floatValue1 = root->appendNew<Value>(proc, BitwiseCast, Origin(), argument1int32);
    Value* floatValue2 = root->appendNew<Value>(proc, BitwiseCast, Origin(), argument2int32);
    Value* asDouble1 = root->appendNew<Value>(proc, FloatToDouble, Origin(), floatValue1);
    Value* asDouble2 = root->appendNew<Value>(proc, FloatToDouble, Origin(), floatValue2);
    Value* result = root->appendNew<Value>(proc, Add, Origin(), asDouble1, asDouble2);
    Value* floatResult = root->appendNew<Value>(proc, DoubleToFloat, Origin(), result);
    Value* result32 = root->appendNew<Value>(proc, BitwiseCast, Origin(), floatResult);
    root->appendNewControlValue(proc, Return, Origin(), result32);

    CHECK(isIdentical(compileAndRun<int32_t>(proc, bitwise_cast<int32_t>(a), bitwise_cast<int32_t>(b)), bitwise_cast<int32_t>(a + b)));
}

void testAddArgsFloatWithEffectfulDoubleConversion(float a, float b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argument1int32 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* argument2int32 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1));
    Value* floatValue1 = root->appendNew<Value>(proc, BitwiseCast, Origin(), argument1int32);
    Value* floatValue2 = root->appendNew<Value>(proc, BitwiseCast, Origin(), argument2int32);
    Value* asDouble1 = root->appendNew<Value>(proc, FloatToDouble, Origin(), floatValue1);
    Value* asDouble2 = root->appendNew<Value>(proc, FloatToDouble, Origin(), floatValue2);
    Value* result = root->appendNew<Value>(proc, Add, Origin(), asDouble1, asDouble2);
    Value* floatResult = root->appendNew<Value>(proc, DoubleToFloat, Origin(), result);
    Value* doubleAddress = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR2);
    root->appendNew<MemoryValue>(proc, Store, Origin(), result, doubleAddress);
    Value* result32 = root->appendNew<Value>(proc, BitwiseCast, Origin(), floatResult);
    root->appendNewControlValue(proc, Return, Origin(), result32);

    double effect = 0;
    CHECK(isIdentical(compileAndRun<int32_t>(proc, bitwise_cast<int32_t>(a), bitwise_cast<int32_t>(b), &effect), bitwise_cast<int32_t>(a + b)));
    CHECK(isIdentical(effect, static_cast<double>(a) + static_cast<double>(b)));
}

void testAddMulMulArgs(int64_t a, int64_t b, int64_t c)
{
    // We want to check every possible ordering of arguments (to properly check every path in B3ReduceStrength):
    // ((a * b) + (a * c))
    // ((a * b) + (c * a))
    // ((b * a) + (a * c))
    // ((b * a) + (c * a))
    for (int i = 0; i < 4; ++i) {
        Procedure proc;
        BasicBlock* root = proc.addBlock();
        Value* argA = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
        Value* argB = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
        Value* argC = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR2);
        Value* mulAB = i & 2 ? root->appendNew<Value>(proc, Mul, Origin(), argA, argB)
            : root->appendNew<Value>(proc, Mul, Origin(), argB, argA);
        Value* mulAC = i & 1 ? root->appendNew<Value>(proc, Mul, Origin(), argA, argC)
            : root->appendNew<Value>(proc, Mul, Origin(), argC, argA);
        root->appendNew<Value>(proc, Return, Origin(),
            root->appendNew<Value>(proc, Add, Origin(),
                mulAB,
                mulAC));

        CHECK_EQ(compileAndRun<int64_t>(proc, a, b, c), ((a * b) + (a * c)));
    }
}

void testMulArg(int a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* value = root->appendNew<Value>(
        proc, Trunc, Origin(), root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(proc, Mul, Origin(), value, value));

    CHECK(compileAndRun<int>(proc, a) == a * a);
}

void testMulArgStore(int a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();

    int mulSlot;
    int valueSlot;

    Value* value = root->appendNew<Value>(
        proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* mul = root->appendNew<Value>(proc, Mul, Origin(), value, value);

    root->appendNew<MemoryValue>(
        proc, Store, Origin(), value,
        root->appendNew<ConstPtrValue>(proc, Origin(), &valueSlot), 0);
    root->appendNew<MemoryValue>(
        proc, Store, Origin(), mul,
        root->appendNew<ConstPtrValue>(proc, Origin(), &mulSlot), 0);

    root->appendNewControlValue(
        proc, Return, Origin(), root->appendNew<Const32Value>(proc, Origin(), 0));

    CHECK(!compileAndRun<int>(proc, a));
    CHECK(mulSlot == a * a);
    CHECK(valueSlot == a);
}

void testMulAddArg(int a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* value = root->appendNew<Value>(
        proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, Add, Origin(),
            root->appendNew<Value>(proc, Mul, Origin(), value, value),
            value));

    CHECK(compileAndRun<int>(proc, a) == a * a + a);
}

void testMulArgs(int a, int b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, Mul, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1)));

    CHECK(compileAndRun<int>(proc, a, b) == a * b);
}

void testMulArgNegArg(int a, int b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argA = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    Value* argB = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
    Value* negB = root->appendNew<Value>(proc, Neg, Origin(), argB);
    Value* result = root->appendNew<Value>(proc, Mul, Origin(), argA, negB);
    root->appendNew<Value>(proc, Return, Origin(), result);

    CHECK(compileAndRun<int>(proc, a, b) == a * (-b));
}

void testMulNegArgArg(int a, int b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argA = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    Value* argB = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
    Value* negA = root->appendNew<Value>(proc, Neg, Origin(), argA);
    Value* result = root->appendNew<Value>(proc, Mul, Origin(), negA, argB);
    root->appendNew<Value>(proc, Return, Origin(), result);

    CHECK(compileAndRun<int>(proc, a, b) == (-a) * b);
}

void testMulArgImm(int64_t a, int64_t b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, Mul, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
            root->appendNew<Const64Value>(proc, Origin(), b)));

    CHECK(compileAndRun<int64_t>(proc, a) == a * b);
}

void testMulImmArg(int a, int b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, Mul, Origin(),
            root->appendNew<Const64Value>(proc, Origin(), a),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)));

    CHECK(compileAndRun<int>(proc, b) == a * b);
}

void testMulArgs32(int a, int b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, Mul, Origin(),
            root->appendNew<Value>(
                proc, Trunc, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)),
            root->appendNew<Value>(
                proc, Trunc, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1))));

    CHECK(compileAndRun<int>(proc, a, b) == a * b);
}

void testMulArgs32SignExtend(int a, int b)
{
    Procedure proc;
    if (proc.optLevel() < 1)
        return;
    BasicBlock* root = proc.addBlock();
    Value* arg1 = root->appendNew<Value>(
        proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* arg2 = root->appendNew<Value>(
        proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1));
    Value* arg164 = root->appendNew<Value>(proc, SExt32, Origin(), arg1);
    Value* arg264 = root->appendNew<Value>(proc, SExt32, Origin(), arg2);
    Value* mul = root->appendNew<Value>(proc, Mul, Origin(), arg164, arg264);
    root->appendNewControlValue(proc, Return, Origin(), mul);

    auto code = compileProc(proc);

    CHECK(invoke<long int>(*code, a, b) == ((long int) a) * ((long int) b));
}

void testMulImm32SignExtend(const int a, int b)
{
    Procedure proc;
    if (proc.optLevel() < 1)
        return;
    BasicBlock* root = proc.addBlock();
    Value* arg1 = root->appendNew<Const64Value>(proc, Origin(), a);
    Value* arg2 = root->appendNew<Value>(
        proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* arg264 = root->appendNew<Value>(proc, SExt32, Origin(), arg2);
    Value* mul = root->appendNew<Value>(proc, Mul, Origin(), arg1, arg264);
    root->appendNewControlValue(proc, Return, Origin(), mul);

    auto code = compileProc(proc);

    CHECK_EQ(invoke<long int>(*code, b), ((long int) a) * ((long int) b));
}

void testMulLoadTwice()
{
    auto test = [&] () {
        Procedure proc;
        BasicBlock* root = proc.addBlock();
        int32_t value = 42;
        Value* load = root->appendNew<MemoryValue>(
            proc, Load, Int32, Origin(),
            root->appendNew<ConstPtrValue>(proc, Origin(), &value));
        root->appendNewControlValue(
            proc, Return, Origin(),
            root->appendNew<Value>(proc, Mul, Origin(), load, load));

        auto code = compileProc(proc);
        CHECK(invoke<int32_t>(*code) == 42 * 42);
    };

    test();
}

void testMulAddArgsLeft()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();

    Value* arg0 = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    Value* arg1 = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
    Value* arg2 = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR2);
    Value* multiplied = root->appendNew<Value>(proc, Mul, Origin(), arg0, arg1);
    Value* added = root->appendNew<Value>(proc, Add, Origin(), multiplied, arg2);
    root->appendNewControlValue(proc, Return, Origin(), added);

    auto code = compileProc(proc);

    auto testValues = int64Operands();
    for (auto a : testValues) {
        for (auto b : testValues) {
            for (auto c : testValues) {
                CHECK(invoke<int64_t>(*code, a.value, b.value, c.value) == a.value * b.value + c.value);
            }
        }
    }
}

void testMulAddArgsRight()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();

    Value* arg0 = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    Value* arg1 = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
    Value* arg2 = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR2);
    Value* multiplied = root->appendNew<Value>(proc, Mul, Origin(), arg1, arg2);
    Value* added = root->appendNew<Value>(proc, Add, Origin(), arg0, multiplied);
    root->appendNewControlValue(proc, Return, Origin(), added);

    auto code = compileProc(proc);

    auto testValues = int64Operands();
    for (auto a : testValues) {
        for (auto b : testValues) {
            for (auto c : testValues) {
                CHECK(invoke<int64_t>(*code, a.value, b.value, c.value) == a.value + b.value * c.value);
            }
        }
    }
}

void testMulAddArgsLeft32()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();

    Value* arg0 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* arg1 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1));
    Value* arg2 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR2));
    Value* multiplied = root->appendNew<Value>(proc, Mul, Origin(), arg0, arg1);
    Value* added = root->appendNew<Value>(proc, Add, Origin(), multiplied, arg2);
    root->appendNewControlValue(proc, Return, Origin(), added);

    auto code = compileProc(proc);

    auto testValues = int32Operands();
    for (auto a : testValues) {
        for (auto b : testValues) {
            for (auto c : testValues) {
                CHECK(invoke<int32_t>(*code, a.value, b.value, c.value) == a.value * b.value + c.value);
            }
        }
    }
}

void testMulAddArgsRight32()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();

    Value* arg0 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* arg1 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1));
    Value* arg2 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR2));
    Value* multiplied = root->appendNew<Value>(proc, Mul, Origin(), arg1, arg2);
    Value* added = root->appendNew<Value>(proc, Add, Origin(), arg0, multiplied);
    root->appendNewControlValue(proc, Return, Origin(), added);

    auto code = compileProc(proc);

    auto testValues = int32Operands();
    for (auto a : testValues) {
        for (auto b : testValues) {
            for (auto c : testValues) {
                CHECK(invoke<int32_t>(*code, a.value, b.value, c.value) == a.value + b.value * c.value);
            }
        }
    }
}

void testMulSubArgsLeft()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();

    Value* arg0 = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    Value* arg1 = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
    Value* arg2 = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR2);
    Value* multiplied = root->appendNew<Value>(proc, Mul, Origin(), arg0, arg1);
    Value* added = root->appendNew<Value>(proc, Sub, Origin(), multiplied, arg2);
    root->appendNewControlValue(proc, Return, Origin(), added);

    auto code = compileProc(proc);

    auto testValues = int64Operands();
    for (auto a : testValues) {
        for (auto b : testValues) {
            for (auto c : testValues) {
                CHECK(invoke<int64_t>(*code, a.value, b.value, c.value) == a.value * b.value - c.value);
            }
        }
    }
}

void testMulSubArgsRight()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();

    Value* arg0 = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    Value* arg1 = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
    Value* arg2 = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR2);
    Value* multiplied = root->appendNew<Value>(proc, Mul, Origin(), arg1, arg2);
    Value* added = root->appendNew<Value>(proc, Sub, Origin(), arg0, multiplied);
    root->appendNewControlValue(proc, Return, Origin(), added);

    auto code = compileProc(proc);

    auto testValues = int64Operands();
    for (auto a : testValues) {
        for (auto b : testValues) {
            for (auto c : testValues) {
                CHECK(invoke<int64_t>(*code, a.value, b.value, c.value) == a.value - b.value * c.value);
            }
        }
    }
}

void testMulSubArgsLeft32()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();

    Value* arg0 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* arg1 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1));
    Value* arg2 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR2));
    Value* multiplied = root->appendNew<Value>(proc, Mul, Origin(), arg0, arg1);
    Value* added = root->appendNew<Value>(proc, Sub, Origin(), multiplied, arg2);
    root->appendNewControlValue(proc, Return, Origin(), added);

    auto code = compileProc(proc);

    auto testValues = int32Operands();
    for (auto a : testValues) {
        for (auto b : testValues) {
            for (auto c : testValues) {
                CHECK(invoke<int32_t>(*code, a.value, b.value, c.value) == a.value * b.value - c.value);
            }
        }
    }
}

void testMulSubArgsRight32()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();

    Value* arg0 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* arg1 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1));
    Value* arg2 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR2));
    Value* multiplied = root->appendNew<Value>(proc, Mul, Origin(), arg1, arg2);
    Value* added = root->appendNew<Value>(proc, Sub, Origin(), arg0, multiplied);
    root->appendNewControlValue(proc, Return, Origin(), added);

    auto code = compileProc(proc);

    auto testValues = int32Operands();
    for (auto a : testValues) {
        for (auto b : testValues) {
            for (auto c : testValues) {
                CHECK(invoke<int32_t>(*code, a.value, b.value, c.value) == a.value - b.value * c.value);
            }
        }
    }
}

void testMulNegArgs()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();

    Value* arg0 = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    Value* arg1 = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
    Value* multiplied = root->appendNew<Value>(proc, Mul, Origin(), arg0, arg1);
    Value* zero = root->appendNew<Const64Value>(proc, Origin(), 0);
    Value* added = root->appendNew<Value>(proc, Sub, Origin(), zero, multiplied);
    root->appendNewControlValue(proc, Return, Origin(), added);

    auto code = compileProc(proc);

    auto testValues = int64Operands();
    for (auto a : testValues) {
        for (auto b : testValues) {
            CHECK(invoke<int64_t>(*code, a.value, b.value) == -(a.value * b.value));
        }
    }
}

void testMulNegArgs32()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();

    Value* arg0 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* arg1 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1));
    Value* multiplied = root->appendNew<Value>(proc, Mul, Origin(), arg0, arg1);
    Value* zero = root->appendNew<Const32Value>(proc, Origin(), 0);
    Value* added = root->appendNew<Value>(proc, Sub, Origin(), zero, multiplied);
    root->appendNewControlValue(proc, Return, Origin(), added);

    auto code = compileProc(proc);

    auto testValues = int32Operands();
    for (auto a : testValues) {
        for (auto b : testValues) {
            CHECK(invoke<int32_t>(*code, a.value, b.value) == -(a.value * b.value));
        }
    }
}

void testMulArgDouble(double a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* value = root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR0);
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(proc, Mul, Origin(), value, value));

    CHECK(isIdentical(compileAndRun<double>(proc, a), a * a));
}

void testMulArgsDouble(double a, double b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* valueA = root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR0);
    Value* valueB = root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR1);
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(proc, Mul, Origin(), valueA, valueB));

    CHECK(isIdentical(compileAndRun<double>(proc, a, b), a * b));
}

void testMulArgImmDouble(double a, double b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* valueA = root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR0);
    Value* valueB = root->appendNew<ConstDoubleValue>(proc, Origin(), b);
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(proc, Mul, Origin(), valueA, valueB));

    CHECK(isIdentical(compileAndRun<double>(proc, a), a * b));
}

void testMulImmArgDouble(double a, double b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* valueA = root->appendNew<ConstDoubleValue>(proc, Origin(), a);
    Value* valueB = root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR0);
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(proc, Mul, Origin(), valueA, valueB));

    CHECK(isIdentical(compileAndRun<double>(proc, b), a * b));
}

void testMulImmsDouble(double a, double b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* valueA = root->appendNew<ConstDoubleValue>(proc, Origin(), a);
    Value* valueB = root->appendNew<ConstDoubleValue>(proc, Origin(), b);
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(proc, Mul, Origin(), valueA, valueB));

    CHECK(isIdentical(compileAndRun<double>(proc), a * b));
}

void testMulArgFloat(float a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argument32 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* floatValue = root->appendNew<Value>(proc, BitwiseCast, Origin(), argument32);
    Value* result = root->appendNew<Value>(proc, Mul, Origin(), floatValue, floatValue);
    Value* result32 = root->appendNew<Value>(proc, BitwiseCast, Origin(), result);
    root->appendNewControlValue(proc, Return, Origin(), result32);


    CHECK(isIdentical(compileAndRun<int32_t>(proc, bitwise_cast<int32_t>(a)), bitwise_cast<int32_t>(a * a)));
}

void testMulArgsFloat(float a, float b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argument1int32 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* argument2int32 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1));
    Value* floatValue1 = root->appendNew<Value>(proc, BitwiseCast, Origin(), argument1int32);
    Value* floatValue2 = root->appendNew<Value>(proc, BitwiseCast, Origin(), argument2int32);
    Value* result = root->appendNew<Value>(proc, Mul, Origin(), floatValue1, floatValue2);
    Value* result32 = root->appendNew<Value>(proc, BitwiseCast, Origin(), result);
    root->appendNewControlValue(proc, Return, Origin(), result32);

    CHECK(isIdentical(compileAndRun<int32_t>(proc, bitwise_cast<int32_t>(a), bitwise_cast<int32_t>(b)), bitwise_cast<int32_t>(a * b)));
}

void testMulArgImmFloat(float a, float b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argument32 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* floatValue = root->appendNew<Value>(proc, BitwiseCast, Origin(), argument32);
    Value* constValue = root->appendNew<ConstFloatValue>(proc, Origin(), b);
    Value* result = root->appendNew<Value>(proc, Mul, Origin(), floatValue, constValue);
    Value* result32 = root->appendNew<Value>(proc, BitwiseCast, Origin(), result);
    root->appendNewControlValue(proc, Return, Origin(), result32);

    CHECK(isIdentical(compileAndRun<int32_t>(proc, bitwise_cast<int32_t>(a)), bitwise_cast<int32_t>(a * b)));
}

void testMulImmArgFloat(float a, float b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argument32 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* floatValue = root->appendNew<Value>(proc, BitwiseCast, Origin(), argument32);
    Value* constValue = root->appendNew<ConstFloatValue>(proc, Origin(), a);
    Value* result = root->appendNew<Value>(proc, Mul, Origin(), constValue, floatValue);
    Value* result32 = root->appendNew<Value>(proc, BitwiseCast, Origin(), result);
    root->appendNewControlValue(proc, Return, Origin(), result32);

    CHECK(isIdentical(compileAndRun<int32_t>(proc, bitwise_cast<int32_t>(b)), bitwise_cast<int32_t>(a * b)));
}

void testMulImmsFloat(float a, float b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* constValue1 = root->appendNew<ConstFloatValue>(proc, Origin(), a);
    Value* constValue2 = root->appendNew<ConstFloatValue>(proc, Origin(), b);
    Value* result = root->appendNew<Value>(proc, Mul, Origin(), constValue1, constValue2);
    Value* result32 = root->appendNew<Value>(proc, BitwiseCast, Origin(), result);
    root->appendNewControlValue(proc, Return, Origin(), result32);

    CHECK(isIdentical(compileAndRun<int32_t>(proc), bitwise_cast<int32_t>(a * b)));
}

void testMulArgFloatWithUselessDoubleConversion(float a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argumentInt32 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* floatValue = root->appendNew<Value>(proc, BitwiseCast, Origin(), argumentInt32);
    Value* asDouble = root->appendNew<Value>(proc, FloatToDouble, Origin(), floatValue);
    Value* result = root->appendNew<Value>(proc, Mul, Origin(), asDouble, asDouble);
    Value* floatResult = root->appendNew<Value>(proc, DoubleToFloat, Origin(), result);
    Value* result32 = root->appendNew<Value>(proc, BitwiseCast, Origin(), floatResult);
    root->appendNewControlValue(proc, Return, Origin(), result32);

    CHECK(isIdentical(compileAndRun<int32_t>(proc, bitwise_cast<int32_t>(a)), bitwise_cast<int32_t>(a * a)));
}

void testMulArgsFloatWithUselessDoubleConversion(float a, float b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argument1int32 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* argument2int32 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1));
    Value* floatValue1 = root->appendNew<Value>(proc, BitwiseCast, Origin(), argument1int32);
    Value* floatValue2 = root->appendNew<Value>(proc, BitwiseCast, Origin(), argument2int32);
    Value* asDouble1 = root->appendNew<Value>(proc, FloatToDouble, Origin(), floatValue1);
    Value* asDouble2 = root->appendNew<Value>(proc, FloatToDouble, Origin(), floatValue2);
    Value* result = root->appendNew<Value>(proc, Mul, Origin(), asDouble1, asDouble2);
    Value* floatResult = root->appendNew<Value>(proc, DoubleToFloat, Origin(), result);
    Value* result32 = root->appendNew<Value>(proc, BitwiseCast, Origin(), floatResult);
    root->appendNewControlValue(proc, Return, Origin(), result32);

    CHECK(isIdentical(compileAndRun<int32_t>(proc, bitwise_cast<int32_t>(a), bitwise_cast<int32_t>(b)), bitwise_cast<int32_t>(a * b)));
}

void testMulArgsFloatWithEffectfulDoubleConversion(float a, float b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argument1int32 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* argument2int32 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1));
    Value* floatValue1 = root->appendNew<Value>(proc, BitwiseCast, Origin(), argument1int32);
    Value* floatValue2 = root->appendNew<Value>(proc, BitwiseCast, Origin(), argument2int32);
    Value* asDouble1 = root->appendNew<Value>(proc, FloatToDouble, Origin(), floatValue1);
    Value* asDouble2 = root->appendNew<Value>(proc, FloatToDouble, Origin(), floatValue2);
    Value* result = root->appendNew<Value>(proc, Mul, Origin(), asDouble1, asDouble2);
    Value* floatResult = root->appendNew<Value>(proc, DoubleToFloat, Origin(), result);
    Value* doubleMulress = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR2);
    root->appendNew<MemoryValue>(proc, Store, Origin(), result, doubleMulress);
    Value* result32 = root->appendNew<Value>(proc, BitwiseCast, Origin(), floatResult);
    root->appendNewControlValue(proc, Return, Origin(), result32);

    double effect = 0;
    CHECK(isIdentical(compileAndRun<int32_t>(proc, bitwise_cast<int32_t>(a), bitwise_cast<int32_t>(b), &effect), bitwise_cast<int32_t>(a * b)));
    CHECK(isIdentical(effect, static_cast<double>(a) * static_cast<double>(b)));
}

void testDivArgDouble(double a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* value = root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR0);
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(proc, Div, Origin(), value, value));

    CHECK(isIdentical(compileAndRun<double>(proc, a), a / a));
}

void testDivArgsDouble(double a, double b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* valueA = root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR0);
    Value* valueB = root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR1);
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(proc, Div, Origin(), valueA, valueB));

    CHECK(isIdentical(compileAndRun<double>(proc, a, b), a / b));
}

void testDivArgImmDouble(double a, double b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* valueA = root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR0);
    Value* valueB = root->appendNew<ConstDoubleValue>(proc, Origin(), b);
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(proc, Div, Origin(), valueA, valueB));

    CHECK(isIdentical(compileAndRun<double>(proc, a), a / b));
}

void testDivImmArgDouble(double a, double b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* valueA = root->appendNew<ConstDoubleValue>(proc, Origin(), a);
    Value* valueB = root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR0);
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(proc, Div, Origin(), valueA, valueB));

    CHECK(isIdentical(compileAndRun<double>(proc, b), a / b));
}

void testDivImmsDouble(double a, double b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* valueA = root->appendNew<ConstDoubleValue>(proc, Origin(), a);
    Value* valueB = root->appendNew<ConstDoubleValue>(proc, Origin(), b);
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(proc, Div, Origin(), valueA, valueB));

    CHECK(isIdentical(compileAndRun<double>(proc), a / b));
}

void testDivArgFloat(float a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argument32 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* floatValue = root->appendNew<Value>(proc, BitwiseCast, Origin(), argument32);
    Value* result = root->appendNew<Value>(proc, Div, Origin(), floatValue, floatValue);
    Value* result32 = root->appendNew<Value>(proc, BitwiseCast, Origin(), result);
    root->appendNewControlValue(proc, Return, Origin(), result32);


    CHECK(isIdentical(compileAndRun<int32_t>(proc, bitwise_cast<int32_t>(a)), bitwise_cast<int32_t>(a / a)));
}

void testDivArgsFloat(float a, float b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argument1int32 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* argument2int32 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1));
    Value* floatValue1 = root->appendNew<Value>(proc, BitwiseCast, Origin(), argument1int32);
    Value* floatValue2 = root->appendNew<Value>(proc, BitwiseCast, Origin(), argument2int32);
    Value* result = root->appendNew<Value>(proc, Div, Origin(), floatValue1, floatValue2);
    Value* result32 = root->appendNew<Value>(proc, BitwiseCast, Origin(), result);
    root->appendNewControlValue(proc, Return, Origin(), result32);

    CHECK(isIdentical(compileAndRun<int32_t>(proc, bitwise_cast<int32_t>(a), bitwise_cast<int32_t>(b)), bitwise_cast<int32_t>(a / b)));
}

void testDivArgImmFloat(float a, float b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argument32 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* floatValue = root->appendNew<Value>(proc, BitwiseCast, Origin(), argument32);
    Value* constValue = root->appendNew<ConstFloatValue>(proc, Origin(), b);
    Value* result = root->appendNew<Value>(proc, Div, Origin(), floatValue, constValue);
    Value* result32 = root->appendNew<Value>(proc, BitwiseCast, Origin(), result);
    root->appendNewControlValue(proc, Return, Origin(), result32);

    CHECK(isIdentical(compileAndRun<int32_t>(proc, bitwise_cast<int32_t>(a)), bitwise_cast<int32_t>(a / b)));
}

void testDivImmArgFloat(float a, float b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argument32 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* floatValue = root->appendNew<Value>(proc, BitwiseCast, Origin(), argument32);
    Value* constValue = root->appendNew<ConstFloatValue>(proc, Origin(), a);
    Value* result = root->appendNew<Value>(proc, Div, Origin(), constValue, floatValue);
    Value* result32 = root->appendNew<Value>(proc, BitwiseCast, Origin(), result);
    root->appendNewControlValue(proc, Return, Origin(), result32);

    CHECK(isIdentical(compileAndRun<int32_t>(proc, bitwise_cast<int32_t>(b)), bitwise_cast<int32_t>(a / b)));
}

void testDivImmsFloat(float a, float b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* constValue1 = root->appendNew<ConstFloatValue>(proc, Origin(), a);
    Value* constValue2 = root->appendNew<ConstFloatValue>(proc, Origin(), b);
    Value* result = root->appendNew<Value>(proc, Div, Origin(), constValue1, constValue2);
    Value* result32 = root->appendNew<Value>(proc, BitwiseCast, Origin(), result);
    root->appendNewControlValue(proc, Return, Origin(), result32);

    CHECK(isIdentical(compileAndRun<int32_t>(proc), bitwise_cast<int32_t>(a / b)));
}

void testModArgDouble(double a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* value = root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR0);
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(proc, Mod, Origin(), value, value));

    CHECK(isIdentical(compileAndRun<double>(proc, a), fmod(a, a)));
}

void testModArgsDouble(double a, double b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* valueA = root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR0);
    Value* valueB = root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR1);
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(proc, Mod, Origin(), valueA, valueB));

    CHECK(isIdentical(compileAndRun<double>(proc, a, b), fmod(a, b)));
}

void testModArgImmDouble(double a, double b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* valueA = root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR0);
    Value* valueB = root->appendNew<ConstDoubleValue>(proc, Origin(), b);
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(proc, Mod, Origin(), valueA, valueB));

    CHECK(isIdentical(compileAndRun<double>(proc, a), fmod(a, b)));
}

void testModImmArgDouble(double a, double b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* valueA = root->appendNew<ConstDoubleValue>(proc, Origin(), a);
    Value* valueB = root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR0);
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(proc, Mod, Origin(), valueA, valueB));

    CHECK(isIdentical(compileAndRun<double>(proc, b), fmod(a, b)));
}

void testModImmsDouble(double a, double b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* valueA = root->appendNew<ConstDoubleValue>(proc, Origin(), a);
    Value* valueB = root->appendNew<ConstDoubleValue>(proc, Origin(), b);
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(proc, Mod, Origin(), valueA, valueB));

    CHECK(isIdentical(compileAndRun<double>(proc), fmod(a, b)));
}

void testModArgFloat(float a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argument32 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* floatValue = root->appendNew<Value>(proc, BitwiseCast, Origin(), argument32);
    Value* result = root->appendNew<Value>(proc, Mod, Origin(), floatValue, floatValue);
    Value* result32 = root->appendNew<Value>(proc, BitwiseCast, Origin(), result);
    root->appendNewControlValue(proc, Return, Origin(), result32);


    CHECK(isIdentical(compileAndRun<int32_t>(proc, bitwise_cast<int32_t>(a)), bitwise_cast<int32_t>(static_cast<float>(fmod(a, a)))));
}

void testModArgsFloat(float a, float b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argument1int32 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* argument2int32 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1));
    Value* floatValue1 = root->appendNew<Value>(proc, BitwiseCast, Origin(), argument1int32);
    Value* floatValue2 = root->appendNew<Value>(proc, BitwiseCast, Origin(), argument2int32);
    Value* result = root->appendNew<Value>(proc, Mod, Origin(), floatValue1, floatValue2);
    Value* result32 = root->appendNew<Value>(proc, BitwiseCast, Origin(), result);
    root->appendNewControlValue(proc, Return, Origin(), result32);

    CHECK(isIdentical(compileAndRun<int32_t>(proc, bitwise_cast<int32_t>(a), bitwise_cast<int32_t>(b)), bitwise_cast<int32_t>(static_cast<float>(fmod(a, b)))));
}

void testModArgImmFloat(float a, float b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argument32 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* floatValue = root->appendNew<Value>(proc, BitwiseCast, Origin(), argument32);
    Value* constValue = root->appendNew<ConstFloatValue>(proc, Origin(), b);
    Value* result = root->appendNew<Value>(proc, Mod, Origin(), floatValue, constValue);
    Value* result32 = root->appendNew<Value>(proc, BitwiseCast, Origin(), result);
    root->appendNewControlValue(proc, Return, Origin(), result32);

    CHECK(isIdentical(compileAndRun<int32_t>(proc, bitwise_cast<int32_t>(a)), bitwise_cast<int32_t>(static_cast<float>(fmod(a, b)))));
}

void testModImmArgFloat(float a, float b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argument32 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* floatValue = root->appendNew<Value>(proc, BitwiseCast, Origin(), argument32);
    Value* constValue = root->appendNew<ConstFloatValue>(proc, Origin(), a);
    Value* result = root->appendNew<Value>(proc, Mod, Origin(), constValue, floatValue);
    Value* result32 = root->appendNew<Value>(proc, BitwiseCast, Origin(), result);
    root->appendNewControlValue(proc, Return, Origin(), result32);

    CHECK(isIdentical(compileAndRun<int32_t>(proc, bitwise_cast<int32_t>(b)), bitwise_cast<int32_t>(static_cast<float>(fmod(a, b)))));
}

void testModImmsFloat(float a, float b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* constValue1 = root->appendNew<ConstFloatValue>(proc, Origin(), a);
    Value* constValue2 = root->appendNew<ConstFloatValue>(proc, Origin(), b);
    Value* result = root->appendNew<Value>(proc, Mod, Origin(), constValue1, constValue2);
    Value* result32 = root->appendNew<Value>(proc, BitwiseCast, Origin(), result);
    root->appendNewControlValue(proc, Return, Origin(), result32);

    CHECK(isIdentical(compileAndRun<int32_t>(proc), bitwise_cast<int32_t>(static_cast<float>(fmod(a, b)))));
}

void testDivArgFloatWithUselessDoubleConversion(float a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argumentInt32 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* floatValue = root->appendNew<Value>(proc, BitwiseCast, Origin(), argumentInt32);
    Value* asDouble = root->appendNew<Value>(proc, FloatToDouble, Origin(), floatValue);
    Value* result = root->appendNew<Value>(proc, Div, Origin(), asDouble, asDouble);
    Value* floatResult = root->appendNew<Value>(proc, DoubleToFloat, Origin(), result);
    Value* result32 = root->appendNew<Value>(proc, BitwiseCast, Origin(), floatResult);
    root->appendNewControlValue(proc, Return, Origin(), result32);

    CHECK(isIdentical(compileAndRun<int32_t>(proc, bitwise_cast<int32_t>(a)), bitwise_cast<int32_t>(a / a)));
}

void testDivArgsFloatWithUselessDoubleConversion(float a, float b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argument1int32 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* argument2int32 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1));
    Value* floatValue1 = root->appendNew<Value>(proc, BitwiseCast, Origin(), argument1int32);
    Value* floatValue2 = root->appendNew<Value>(proc, BitwiseCast, Origin(), argument2int32);
    Value* asDouble1 = root->appendNew<Value>(proc, FloatToDouble, Origin(), floatValue1);
    Value* asDouble2 = root->appendNew<Value>(proc, FloatToDouble, Origin(), floatValue2);
    Value* result = root->appendNew<Value>(proc, Div, Origin(), asDouble1, asDouble2);
    Value* floatResult = root->appendNew<Value>(proc, DoubleToFloat, Origin(), result);
    Value* result32 = root->appendNew<Value>(proc, BitwiseCast, Origin(), floatResult);
    root->appendNewControlValue(proc, Return, Origin(), result32);

    CHECK(isIdentical(compileAndRun<int32_t>(proc, bitwise_cast<int32_t>(a), bitwise_cast<int32_t>(b)), bitwise_cast<int32_t>(a / b)));
}

void testDivArgsFloatWithEffectfulDoubleConversion(float a, float b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argument1int32 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* argument2int32 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1));
    Value* floatValue1 = root->appendNew<Value>(proc, BitwiseCast, Origin(), argument1int32);
    Value* floatValue2 = root->appendNew<Value>(proc, BitwiseCast, Origin(), argument2int32);
    Value* asDouble1 = root->appendNew<Value>(proc, FloatToDouble, Origin(), floatValue1);
    Value* asDouble2 = root->appendNew<Value>(proc, FloatToDouble, Origin(), floatValue2);
    Value* result = root->appendNew<Value>(proc, Div, Origin(), asDouble1, asDouble2);
    Value* floatResult = root->appendNew<Value>(proc, DoubleToFloat, Origin(), result);
    Value* doubleDivress = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR2);
    root->appendNew<MemoryValue>(proc, Store, Origin(), result, doubleDivress);
    Value* result32 = root->appendNew<Value>(proc, BitwiseCast, Origin(), floatResult);
    root->appendNewControlValue(proc, Return, Origin(), result32);

    double effect = 0;
    CHECK(isIdentical(compileAndRun<int32_t>(proc, bitwise_cast<int32_t>(a), bitwise_cast<int32_t>(b), &effect), bitwise_cast<int32_t>(a / b)));
    CHECK(isIdentical(effect, static_cast<double>(a) / static_cast<double>(b)));
}

void testUDivArgsInt32(uint32_t a, uint32_t b)
{
    // UDiv with denominator == 0 is invalid.
    if (!b)
        return;

    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argument1 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* argument2 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1));
    Value* result = root->appendNew<Value>(proc, UDiv, Origin(), argument1, argument2);
    root->appendNew<Value>(proc, Return, Origin(), result);

    CHECK_EQ(compileAndRun<uint32_t>(proc, a, b), a / b);
}

void testUDivArgsInt64(uint64_t a, uint64_t b)
{
    // UDiv with denominator == 0 is invalid.
    if (!b)
        return;

    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argument1 = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    Value* argument2 = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
    Value* result = root->appendNew<Value>(proc, UDiv, Origin(), argument1, argument2);
    root->appendNew<Value>(proc, Return, Origin(), result);

    CHECK_EQ(compileAndRun<uint64_t>(proc, a, b), a / b);
}

void testUModArgsInt32(uint32_t a, uint32_t b)
{
    // UMod with denominator == 0 is invalid.
    if (!b)
        return;

    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argument1 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* argument2 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1));
    Value* result = root->appendNew<Value>(proc, UMod, Origin(), argument1, argument2);
    root->appendNew<Value>(proc, Return, Origin(), result);

    CHECK_EQ(compileAndRun<uint32_t>(proc, a, b), a % b);
}

void testUModArgsInt64(uint64_t a, uint64_t b)
{
    // UMod with denominator == 0 is invalid.
    if (!b)
        return;

    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argument1 = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    Value* argument2 = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
    Value* result = root->appendNew<Value>(proc, UMod, Origin(), argument1, argument2);
    root->appendNew<Value>(proc, Return, Origin(), result);

    CHECK_EQ(compileAndRun<uint64_t>(proc, a, b), a % b);
}

void testSubArg(int a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* value = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(proc, Sub, Origin(), value, value));

    CHECK(!compileAndRun<int>(proc, a));
}

void testSubArgs(int a, int b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, Sub, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1)));

    CHECK(compileAndRun<int>(proc, a, b) == a - b);
}

void testSubArgImm(int64_t a, int64_t b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, Sub, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
            root->appendNew<Const64Value>(proc, Origin(), b)));

    CHECK(compileAndRun<int64_t>(proc, a) == a - b);
}

void testSubNeg(int a, int b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, Sub, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
            root->appendNew<Value>(proc, Neg, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1))));

    CHECK(compileAndRun<int>(proc, a, b) == a - (- b));
}

void testNegSub(int a, int b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, Neg, Origin(),
            root->appendNew<Value>(proc, Sub, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1))));

    CHECK(compileAndRun<int>(proc, a, b) == -(a - b));
}

void testNegValueSubOne(int a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argument = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    Value* negArgument = root->appendNew<Value>(proc, Sub, Origin(),
        root->appendNew<Const64Value>(proc, Origin(), 0),
        argument);
    Value* negArgumentMinusOne = root->appendNew<Value>(proc, Sub, Origin(),
        negArgument,
        root->appendNew<Const64Value>(proc, Origin(), 1));
    root->appendNewControlValue(proc, Return, Origin(), negArgumentMinusOne);
    CHECK(compileAndRun<int>(proc, a) == -a - 1);
}

void testSubSub(int a, int b, int c)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, Sub, Origin(),
            root->appendNew<Value>(proc, Sub, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1)),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR2)));

    CHECK(compileAndRun<int>(proc, a, b, c) == (a-b)-c);
}

void testSubSub2(int a, int b, int c)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, Sub, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
            root->appendNew<Value>(proc, Sub, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR2))));

    CHECK(compileAndRun<int>(proc, a, b, c) == a-(b-c));
}

void testSubAdd(int a, int b, int c)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, Sub, Origin(),
            root->appendNew<Value>(proc, Add, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1)),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR2)));

    CHECK(compileAndRun<int>(proc, a, b, c) == (a+b)-c);
}

void testSubFirstNeg(int a, int b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, Sub, Origin(),
            root->appendNew<Value>(proc, Neg, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1)));

    CHECK(compileAndRun<int>(proc, a, b) == (-a)-b);
}

void testSubImmArg(int a, int b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, Sub, Origin(),
            root->appendNew<Const64Value>(proc, Origin(), a),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)));

    CHECK(compileAndRun<int>(proc, b) == a - b);
}

void testSubArgMem(int64_t a, int64_t b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* address = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
    MemoryValue* load = root->appendNew<MemoryValue>(proc, Load, Int64, Origin(), address);
    Value* result = root->appendNew<Value>(proc, Sub, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
        load);
    root->appendNewControlValue(proc, Return, Origin(), result);

    CHECK(compileAndRun<int64_t>(proc, a, &b) == a - b);
}

void testSubMemArg(int64_t a, int64_t b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* address = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    MemoryValue* load = root->appendNew<MemoryValue>(proc, Load, Int64, Origin(), address);
    Value* result = root->appendNew<Value>(proc, Sub, Origin(),
        load,
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1));
    root->appendNew<MemoryValue>(proc, Store, Origin(), result, address);
    root->appendNewControlValue(proc, Return, Origin(), root->appendNew<Const32Value>(proc, Origin(), 0));

    int64_t inputOutput = a;
    CHECK(!compileAndRun<int64_t>(proc, &inputOutput, b));
    CHECK(inputOutput == a - b);
}

void testSubImmMem(int64_t a, int64_t b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* address = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    MemoryValue* load = root->appendNew<MemoryValue>(proc, Load, Int64, Origin(), address);
    Value* result = root->appendNew<Value>(proc, Sub, Origin(),
        root->appendNew<Const64Value>(proc, Origin(), a),
        load);
    root->appendNew<MemoryValue>(proc, Store, Origin(), result, address);
    root->appendNewControlValue(proc, Return, Origin(), root->appendNew<Const32Value>(proc, Origin(), 0));

    int64_t inputOutput = b;
    CHECK(!compileAndRun<int>(proc, &inputOutput));
    CHECK(inputOutput == a - b);
}

void testSubMemImm(int64_t a, int64_t b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* address = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    MemoryValue* load = root->appendNew<MemoryValue>(proc, Load, Int64, Origin(), address);
    Value* result = root->appendNew<Value>(proc, Sub, Origin(),
        load,
        root->appendNew<Const64Value>(proc, Origin(), b));
    root->appendNew<MemoryValue>(proc, Store, Origin(), result, address);
    root->appendNewControlValue(proc, Return, Origin(), root->appendNew<Const32Value>(proc, Origin(), 0));

    int64_t inputOutput = a;
    CHECK(!compileAndRun<int>(proc, &inputOutput));
    CHECK(inputOutput == a - b);
}


void testSubArgs32(int a, int b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, Sub, Origin(),
            root->appendNew<Value>(
                proc, Trunc, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)),
            root->appendNew<Value>(
                proc, Trunc, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1))));

    CHECK(compileAndRun<int>(proc, a, b) == a - b);
}

void testSubArgImm32(int a, int b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, Sub, Origin(),
            root->appendNew<Value>(
                proc, Trunc, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)),
            root->appendNew<Const32Value>(proc, Origin(), b)));

    CHECK(compileAndRun<int>(proc, a) == a - b);
}

void testSubImmArg32(int a, int b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, Sub, Origin(),
            root->appendNew<Const32Value>(proc, Origin(), a),
            root->appendNew<Value>(
                proc, Trunc, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0))));

    CHECK(compileAndRun<int>(proc, b) == a - b);
}

void testSubMemArg32(int32_t a, int32_t b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* address = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    MemoryValue* load = root->appendNew<MemoryValue>(proc, Load, Int32, Origin(), address);
    Value* argument = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1));
    Value* result = root->appendNew<Value>(proc, Sub, Origin(), load, argument);
    root->appendNew<MemoryValue>(proc, Store, Origin(), result, address);
    root->appendNewControlValue(proc, Return, Origin(), root->appendNew<Const32Value>(proc, Origin(), 0));

    int32_t inputOutput = a;
    CHECK(!compileAndRun<int32_t>(proc, &inputOutput, b));
    CHECK(inputOutput == a - b);
}

void testSubArgMem32(int32_t a, int32_t b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* address = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
    MemoryValue* load = root->appendNew<MemoryValue>(proc, Load, Int32, Origin(), address);
    Value* argument = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* result = root->appendNew<Value>(proc, Sub, Origin(), argument, load);
    root->appendNewControlValue(proc, Return, Origin(), result);

    CHECK(compileAndRun<int32_t>(proc, a, &b) == a - b);
}

void testSubImmMem32(int32_t a, int32_t b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* address = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    MemoryValue* load = root->appendNew<MemoryValue>(proc, Load, Int32, Origin(), address);
    Value* result = root->appendNew<Value>(proc, Sub, Origin(),
        root->appendNew<Const32Value>(proc, Origin(), a),
        load);
    root->appendNew<MemoryValue>(proc, Store, Origin(), result, address);
    root->appendNewControlValue(proc, Return, Origin(), root->appendNew<Const32Value>(proc, Origin(), 0));

    int32_t inputOutput = b;
    CHECK(!compileAndRun<int>(proc, &inputOutput));
    CHECK(inputOutput == a - b);
}

void testSubMemImm32(int32_t a, int32_t b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* address = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    MemoryValue* load = root->appendNew<MemoryValue>(proc, Load, Int32, Origin(), address);
    Value* result = root->appendNew<Value>(proc, Sub, Origin(),
        load,
        root->appendNew<Const32Value>(proc, Origin(), b));
    root->appendNew<MemoryValue>(proc, Store, Origin(), result, address);
    root->appendNewControlValue(proc, Return, Origin(), root->appendNew<Const32Value>(proc, Origin(), 0));

    int32_t inputOutput = a;
    CHECK(!compileAndRun<int>(proc, &inputOutput));
    CHECK(inputOutput == a - b);
}

void testNegValueSubOne32(int a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argument = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* negArgument = root->appendNew<Value>(proc, Sub, Origin(),
        root->appendNew<Const32Value>(proc, Origin(), 0),
        argument);
    Value* negArgumentMinusOne = root->appendNew<Value>(proc, Sub, Origin(),
        negArgument,
        root->appendNew<Const32Value>(proc, Origin(), 1));
    root->appendNewControlValue(proc, Return, Origin(), negArgumentMinusOne);
    CHECK(compileAndRun<int>(proc, a) == -a - 1);
}

void testNegMulArgImm(int64_t a, int64_t b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argument = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    Value* constant = root->appendNew<Const64Value>(proc, Origin(), b);
    Value* mul = root->appendNew<Value>(proc, Mul, Origin(), argument, constant);
    Value* result = root->appendNew<Value>(proc, Neg, Origin(), mul);
    root->appendNew<Value>(proc, Return, Origin(), result);

    CHECK(compileAndRun<int64_t>(proc, a) == -(a * b));
}

void testSubMulMulArgs(int64_t a, int64_t b, int64_t c)
{
    // We want to check every possible ordering of arguments (to properly check every path in B3ReduceStrength):
    // ((a * b) - (a * c))
    // ((a * b) - (c * a))
    // ((b * a) - (a * c))
    // ((b * a) - (c * a))
    for (int i = 0; i < 4; ++i) {
        Procedure proc;
        BasicBlock* root = proc.addBlock();
        Value* argA = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
        Value* argB = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
        Value* argC = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR2);
        Value* mulAB = i & 2 ? root->appendNew<Value>(proc, Mul, Origin(), argA, argB)
            : root->appendNew<Value>(proc, Mul, Origin(), argB, argA);
        Value* mulAC = i & 1 ? root->appendNew<Value>(proc, Mul, Origin(), argA, argC)
            : root->appendNew<Value>(proc, Mul, Origin(), argC, argA);
        root->appendNew<Value>(proc, Return, Origin(),
            root->appendNew<Value>(proc, Sub, Origin(),
                mulAB,
                mulAC));

        CHECK_EQ(compileAndRun<int64_t>(proc, a, b, c), ((a * b) - (a * c)));
    }
}

void testSubArgDouble(double a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* value = root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR0);
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(proc, Sub, Origin(), value, value));

    CHECK(isIdentical(compileAndRun<double>(proc, a), a - a));
}

void testSubArgsDouble(double a, double b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* valueA = root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR0);
    Value* valueB = root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR1);
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(proc, Sub, Origin(), valueA, valueB));

    CHECK(isIdentical(compileAndRun<double>(proc, a, b), a - b));
}

void testSubArgImmDouble(double a, double b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* valueA = root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR0);
    Value* valueB = root->appendNew<ConstDoubleValue>(proc, Origin(), b);
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(proc, Sub, Origin(), valueA, valueB));

    CHECK(isIdentical(compileAndRun<double>(proc, a), a - b));
}

void testSubImmArgDouble(double a, double b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* valueA = root->appendNew<ConstDoubleValue>(proc, Origin(), a);
    Value* valueB = root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR0);
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(proc, Sub, Origin(), valueA, valueB));

    CHECK(isIdentical(compileAndRun<double>(proc, b), a - b));
}

void testSubImmsDouble(double a, double b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* valueA = root->appendNew<ConstDoubleValue>(proc, Origin(), a);
    Value* valueB = root->appendNew<ConstDoubleValue>(proc, Origin(), b);
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(proc, Sub, Origin(), valueA, valueB));

    CHECK(isIdentical(compileAndRun<double>(proc), a - b));
}

void testSubArgFloat(float a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argument32 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* floatValue = root->appendNew<Value>(proc, BitwiseCast, Origin(), argument32);
    Value* result = root->appendNew<Value>(proc, Sub, Origin(), floatValue, floatValue);
    Value* result32 = root->appendNew<Value>(proc, BitwiseCast, Origin(), result);
    root->appendNewControlValue(proc, Return, Origin(), result32);


    CHECK(isIdentical(compileAndRun<int32_t>(proc, bitwise_cast<int32_t>(a)), bitwise_cast<int32_t>(a - a)));
}

void testSubArgsFloat(float a, float b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argument1int32 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* argument2int32 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1));
    Value* floatValue1 = root->appendNew<Value>(proc, BitwiseCast, Origin(), argument1int32);
    Value* floatValue2 = root->appendNew<Value>(proc, BitwiseCast, Origin(), argument2int32);
    Value* result = root->appendNew<Value>(proc, Sub, Origin(), floatValue1, floatValue2);
    Value* result32 = root->appendNew<Value>(proc, BitwiseCast, Origin(), result);
    root->appendNewControlValue(proc, Return, Origin(), result32);

    CHECK(isIdentical(compileAndRun<int32_t>(proc, bitwise_cast<int32_t>(a), bitwise_cast<int32_t>(b)), bitwise_cast<int32_t>(a - b)));
}

void testSubArgImmFloat(float a, float b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argument32 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* floatValue = root->appendNew<Value>(proc, BitwiseCast, Origin(), argument32);
    Value* constValue = root->appendNew<ConstFloatValue>(proc, Origin(), b);
    Value* result = root->appendNew<Value>(proc, Sub, Origin(), floatValue, constValue);
    Value* result32 = root->appendNew<Value>(proc, BitwiseCast, Origin(), result);
    root->appendNewControlValue(proc, Return, Origin(), result32);

    CHECK(isIdentical(compileAndRun<int32_t>(proc, bitwise_cast<int32_t>(a)), bitwise_cast<int32_t>(a - b)));
}

void testSubImmArgFloat(float a, float b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argument32 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* floatValue = root->appendNew<Value>(proc, BitwiseCast, Origin(), argument32);
    Value* constValue = root->appendNew<ConstFloatValue>(proc, Origin(), a);
    Value* result = root->appendNew<Value>(proc, Sub, Origin(), constValue, floatValue);
    Value* result32 = root->appendNew<Value>(proc, BitwiseCast, Origin(), result);
    root->appendNewControlValue(proc, Return, Origin(), result32);

    CHECK(isIdentical(compileAndRun<int32_t>(proc, bitwise_cast<int32_t>(b)), bitwise_cast<int32_t>(a - b)));
}

void testSubImmsFloat(float a, float b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* constValue1 = root->appendNew<ConstFloatValue>(proc, Origin(), a);
    Value* constValue2 = root->appendNew<ConstFloatValue>(proc, Origin(), b);
    Value* result = root->appendNew<Value>(proc, Sub, Origin(), constValue1, constValue2);
    Value* result32 = root->appendNew<Value>(proc, BitwiseCast, Origin(), result);
    root->appendNewControlValue(proc, Return, Origin(), result32);

    CHECK(isIdentical(compileAndRun<int32_t>(proc), bitwise_cast<int32_t>(a - b)));
}

void testSubArgFloatWithUselessDoubleConversion(float a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argumentInt32 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* floatValue = root->appendNew<Value>(proc, BitwiseCast, Origin(), argumentInt32);
    Value* asDouble = root->appendNew<Value>(proc, FloatToDouble, Origin(), floatValue);
    Value* result = root->appendNew<Value>(proc, Sub, Origin(), asDouble, asDouble);
    Value* floatResult = root->appendNew<Value>(proc, DoubleToFloat, Origin(), result);
    Value* result32 = root->appendNew<Value>(proc, BitwiseCast, Origin(), floatResult);
    root->appendNewControlValue(proc, Return, Origin(), result32);

    CHECK(isIdentical(compileAndRun<int32_t>(proc, bitwise_cast<int32_t>(a)), bitwise_cast<int32_t>(a - a)));
}

void testSubArgsFloatWithUselessDoubleConversion(float a, float b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argument1int32 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* argument2int32 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1));
    Value* floatValue1 = root->appendNew<Value>(proc, BitwiseCast, Origin(), argument1int32);
    Value* floatValue2 = root->appendNew<Value>(proc, BitwiseCast, Origin(), argument2int32);
    Value* asDouble1 = root->appendNew<Value>(proc, FloatToDouble, Origin(), floatValue1);
    Value* asDouble2 = root->appendNew<Value>(proc, FloatToDouble, Origin(), floatValue2);
    Value* result = root->appendNew<Value>(proc, Sub, Origin(), asDouble1, asDouble2);
    Value* floatResult = root->appendNew<Value>(proc, DoubleToFloat, Origin(), result);
    Value* result32 = root->appendNew<Value>(proc, BitwiseCast, Origin(), floatResult);
    root->appendNewControlValue(proc, Return, Origin(), result32);

    CHECK(isIdentical(compileAndRun<int32_t>(proc, bitwise_cast<int32_t>(a), bitwise_cast<int32_t>(b)), bitwise_cast<int32_t>(a - b)));
}

void testSubArgsFloatWithEffectfulDoubleConversion(float a, float b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argument1int32 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* argument2int32 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1));
    Value* floatValue1 = root->appendNew<Value>(proc, BitwiseCast, Origin(), argument1int32);
    Value* floatValue2 = root->appendNew<Value>(proc, BitwiseCast, Origin(), argument2int32);
    Value* asDouble1 = root->appendNew<Value>(proc, FloatToDouble, Origin(), floatValue1);
    Value* asDouble2 = root->appendNew<Value>(proc, FloatToDouble, Origin(), floatValue2);
    Value* result = root->appendNew<Value>(proc, Sub, Origin(), asDouble1, asDouble2);
    Value* floatResult = root->appendNew<Value>(proc, DoubleToFloat, Origin(), result);
    Value* doubleSubress = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR2);
    root->appendNew<MemoryValue>(proc, Store, Origin(), result, doubleSubress);
    Value* result32 = root->appendNew<Value>(proc, BitwiseCast, Origin(), floatResult);
    root->appendNewControlValue(proc, Return, Origin(), result32);

    double effect = 0;
    CHECK(isIdentical(compileAndRun<int32_t>(proc, bitwise_cast<int32_t>(a), bitwise_cast<int32_t>(b), &effect), bitwise_cast<int32_t>(a - b)));
    CHECK(isIdentical(effect, static_cast<double>(a) - static_cast<double>(b)));
}

void testTernarySubInstructionSelection(B3::Opcode valueModifier, Type valueType, Air::Opcode expectedOpcode)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();

    Value* left = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    Value* right = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);

    if (valueModifier == Trunc) {
        left = root->appendNew<Value>(proc, valueModifier, valueType, Origin(), left);
        right = root->appendNew<Value>(proc, valueModifier, valueType, Origin(), right);
    }

    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(proc, Sub, Origin(), left, right));

    lowerToAirForTesting(proc);

    auto block = proc.code()[0];
    unsigned numberOfSubInstructions = 0;
    for (auto instruction : *block) {
        if (instruction.kind.opcode == expectedOpcode) {
            CHECK_EQ(instruction.args.size(), 3ul);
            CHECK_EQ(instruction.args[0].kind(), Air::Arg::Tmp);
            CHECK_EQ(instruction.args[1].kind(), Air::Arg::Tmp);
            CHECK_EQ(instruction.args[2].kind(), Air::Arg::Tmp);
            numberOfSubInstructions++;
        }
    }
    CHECK_EQ(numberOfSubInstructions, 1ul);
}

void testNegDouble(double a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, Neg, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR0)));

    CHECK(isIdentical(compileAndRun<double>(proc, a), -a));
}

void testNegFloat(float a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argument32 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* floatValue = root->appendNew<Value>(proc, BitwiseCast, Origin(), argument32);
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(proc, Neg, Origin(), floatValue));

    CHECK(isIdentical(compileAndRun<float>(proc, bitwise_cast<int32_t>(a)), -a));
}

void testNegFloatWithUselessDoubleConversion(float a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argumentInt32 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* floatValue = root->appendNew<Value>(proc, BitwiseCast, Origin(), argumentInt32);
    Value* asDouble = root->appendNew<Value>(proc, FloatToDouble, Origin(), floatValue);
    Value* result = root->appendNew<Value>(proc, Neg, Origin(), asDouble);
    Value* floatResult = root->appendNew<Value>(proc, DoubleToFloat, Origin(), result);
    root->appendNewControlValue(proc, Return, Origin(), floatResult);

    CHECK(isIdentical(compileAndRun<float>(proc, bitwise_cast<int32_t>(a)), -a));
}

static void testBitAndArgs(int64_t a, int64_t b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, BitAnd, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1)));

    CHECK(compileAndRun<int64_t>(proc, a, b) == (a & b));
}

static void testBitAndSameArg(int64_t a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argument = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, BitAnd, Origin(),
            argument,
            argument));

    CHECK(compileAndRun<int64_t>(proc, a) == a);
}

static void testBitAndNotNot(int64_t a, int64_t b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argA = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    Value* argB = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
    Value* notA = root->appendNew<Value>(proc, BitXor, Origin(), argA, root->appendNew<Const64Value>(proc, Origin(), -1));
    Value* notB = root->appendNew<Value>(proc, BitXor, Origin(), argB, root->appendNew<Const64Value>(proc, Origin(), -1));
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, BitAnd, Origin(),
            notA,
            notB));

    CHECK_EQ(compileAndRun<int64_t>(proc, a, b), (~a & ~b));
}

static void testBitAndNotNot32(int32_t a, int32_t b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argA = root->appendNew<Value>(proc, Trunc, Origin(), root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* argB = root->appendNew<Value>(proc, Trunc, Origin(), root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1));
    Value* notA = root->appendNew<Value>(proc, BitXor, Origin(), argA, root->appendNew<Const32Value>(proc, Origin(), -1));
    Value* notB = root->appendNew<Value>(proc, BitXor, Origin(), argB, root->appendNew<Const32Value>(proc, Origin(), -1));
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, BitAnd, Origin(),
            notA,
            notB));

    CHECK_EQ(compileAndRun<int32_t>(proc, a, b), (~a & ~b));
}

static void testBitAndNotImm(int64_t a, int64_t b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argA = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    Value* notA = root->appendNew<Value>(proc, BitXor, Origin(), argA, root->appendNew<Const64Value>(proc, Origin(), -1));
    Value* cstB = root->appendNew<Const64Value>(proc, Origin(), b);
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, BitAnd, Origin(),
            notA,
            cstB));

    CHECK_EQ(compileAndRun<int64_t>(proc, a), (~a & b));
}

static void testBitAndNotImm32(int32_t a, int32_t b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argA = root->appendNew<Value>(proc, Trunc, Origin(), root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* notA = root->appendNew<Value>(proc, BitXor, Origin(), argA, root->appendNew<Const32Value>(proc, Origin(), -1));
    Value* cstB = root->appendNew<Const32Value>(proc, Origin(), b);
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, BitAnd, Origin(),
            notA,
            cstB));

    CHECK_EQ(compileAndRun<int32_t>(proc, a), (~a & b));
}

static void testBitAndImms(int64_t a, int64_t b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, BitAnd, Origin(),
            root->appendNew<Const64Value>(proc, Origin(), a),
            root->appendNew<Const64Value>(proc, Origin(), b)));

    CHECK(compileAndRun<int64_t>(proc) == (a & b));
}

static void testBitAndArgImm(int64_t a, int64_t b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, BitAnd, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
            root->appendNew<Const64Value>(proc, Origin(), b)));

    CHECK(compileAndRun<int64_t>(proc, a) == (a & b));
}

static void testBitAndImmArg(int64_t a, int64_t b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, BitAnd, Origin(),
            root->appendNew<Const64Value>(proc, Origin(), a),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)));

    CHECK(compileAndRun<int64_t>(proc, b) == (a & b));
}

static void testBitAndBitAndArgImmImm(int64_t a, int64_t b, int64_t c)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* innerBitAnd = root->appendNew<Value>(
        proc, BitAnd, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
        root->appendNew<Const64Value>(proc, Origin(), b));
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, BitAnd, Origin(),
            innerBitAnd,
            root->appendNew<Const64Value>(proc, Origin(), c)));

    CHECK(compileAndRun<int64_t>(proc, a) == ((a & b) & c));
}

static void testBitAndImmBitAndArgImm(int64_t a, int64_t b, int64_t c)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* innerBitAnd = root->appendNew<Value>(
        proc, BitAnd, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
        root->appendNew<Const64Value>(proc, Origin(), c));
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, BitAnd, Origin(),
            root->appendNew<Const64Value>(proc, Origin(), a),
            innerBitAnd));

    CHECK(compileAndRun<int64_t>(proc, b) == (a & (b & c)));
}

static void testBitAndArgs32(int a, int b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, BitAnd, Origin(),
            root->appendNew<Value>(
                proc, Trunc, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)),
            root->appendNew<Value>(
                proc, Trunc, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1))));

    CHECK(compileAndRun<int>(proc, a, b) == (a & b));
}

static void testBitAndSameArg32(int a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argument = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, BitAnd, Origin(),
            argument,
            argument));

    CHECK(compileAndRun<int>(proc, a) == a);
}

static void testBitAndImms32(int a, int b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, BitAnd, Origin(),
            root->appendNew<Const32Value>(proc, Origin(), a),
            root->appendNew<Const32Value>(proc, Origin(), b)));

    CHECK(compileAndRun<int>(proc) == (a & b));
}

static void testBitAndArgImm32(int a, int b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, BitAnd, Origin(),
            root->appendNew<Value>(
                proc, Trunc, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)),
            root->appendNew<Const32Value>(proc, Origin(), b)));

    CHECK(compileAndRun<int>(proc, a) == (a & b));
}

static void testBitAndImmArg32(int a, int b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, BitAnd, Origin(),
            root->appendNew<Const32Value>(proc, Origin(), a),
            root->appendNew<Value>(
                proc, Trunc, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0))));

    CHECK(compileAndRun<int>(proc, b) == (a & b));
}

static void testBitAndBitAndArgImmImm32(int a, int b, int c)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* innerBitAnd = root->appendNew<Value>(
        proc, BitAnd, Origin(),
        root->appendNew<Value>(
            proc, Trunc, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)),
        root->appendNew<Const32Value>(proc, Origin(), b));
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, BitAnd, Origin(),
            innerBitAnd,
            root->appendNew<Const32Value>(proc, Origin(), c)));

    CHECK(compileAndRun<int>(proc, a) == ((a & b) & c));
}

static void testBitAndImmBitAndArgImm32(int a, int b, int c)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* innerBitAnd = root->appendNew<Value>(
        proc, BitAnd, Origin(),
        root->appendNew<Value>(
            proc, Trunc, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)),
        root->appendNew<Const32Value>(proc, Origin(), c));
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, BitAnd, Origin(),
            root->appendNew<Const32Value>(proc, Origin(), a),
            innerBitAnd));

    CHECK(compileAndRun<int>(proc, b) == (a & (b & c)));
}

static void testBitAndWithMaskReturnsBooleans(int64_t a, int64_t b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* arg0 = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    Value* arg1 = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
    Value* equal = root->appendNew<Value>(proc, Equal, Origin(), arg0, arg1);
    Value* maskedEqual = root->appendNew<Value>(proc, BitAnd, Origin(),
        root->appendNew<Const32Value>(proc, Origin(), 0x5),
        equal);
    Value* inverted = root->appendNew<Value>(proc, BitXor, Origin(),
        root->appendNew<Const32Value>(proc, Origin(), 0x1),
        maskedEqual);
    Value* select = root->appendNew<Value>(proc, Select, Origin(), inverted,
        root->appendNew<Const64Value>(proc, Origin(), 42),
        root->appendNew<Const64Value>(proc, Origin(), -5));

    root->appendNewControlValue(proc, Return, Origin(), select);

    int64_t expected = (a == b) ? -5 : 42;
    CHECK(compileAndRun<int64_t>(proc, a, b) == expected);
}

static double bitAndDouble(double a, double b)
{
    return bitwise_cast<double>(bitwise_cast<uint64_t>(a) & bitwise_cast<uint64_t>(b));
}

static void testBitAndArgDouble(double a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argument = root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR0);
    Value* result = root->appendNew<Value>(proc, BitAnd, Origin(), argument, argument);
    root->appendNewControlValue(proc, Return, Origin(), result);

    CHECK(isIdentical(compileAndRun<double>(proc, a), bitAndDouble(a, a)));
}

static void testBitAndArgsDouble(double a, double b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argumentA = root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR0);
    Value* argumentB = root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR1);
    Value* result = root->appendNew<Value>(proc, BitAnd, Origin(), argumentA, argumentB);
    root->appendNewControlValue(proc, Return, Origin(), result);

    CHECK(isIdentical(compileAndRun<double>(proc, a, b), bitAndDouble(a, b)));
}

static void testBitAndArgImmDouble(double a, double b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argumentA = root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR0);
    Value* argumentB = root->appendNew<ConstDoubleValue>(proc, Origin(), b);
    Value* result = root->appendNew<Value>(proc, BitAnd, Origin(), argumentA, argumentB);
    root->appendNewControlValue(proc, Return, Origin(), result);

    CHECK(isIdentical(compileAndRun<double>(proc, a, b), bitAndDouble(a, b)));
}

static void testBitAndImmsDouble(double a, double b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argumentA = root->appendNew<ConstDoubleValue>(proc, Origin(), a);
    Value* argumentB = root->appendNew<ConstDoubleValue>(proc, Origin(), b);
    Value* result = root->appendNew<Value>(proc, BitAnd, Origin(), argumentA, argumentB);
    root->appendNewControlValue(proc, Return, Origin(), result);

    CHECK(isIdentical(compileAndRun<double>(proc), bitAndDouble(a, b)));
}

static float bitAndFloat(float a, float b)
{
    return bitwise_cast<float>(bitwise_cast<uint32_t>(a) & bitwise_cast<uint32_t>(b));
}

static void testBitAndArgFloat(float a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argument = root->appendNew<Value>(proc, BitwiseCast, Origin(),
        root->appendNew<Value>(proc, Trunc, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)));
    Value* result = root->appendNew<Value>(proc, BitAnd, Origin(), argument, argument);
    root->appendNewControlValue(proc, Return, Origin(), result);

    CHECK(isIdentical(compileAndRun<float>(proc, bitwise_cast<int32_t>(a)), bitAndFloat(a, a)));
}

static void testBitAndArgsFloat(float a, float b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argumentA = root->appendNew<Value>(proc, BitwiseCast, Origin(),
        root->appendNew<Value>(proc, Trunc, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)));
    Value* argumentB = root->appendNew<Value>(proc, BitwiseCast, Origin(),
        root->appendNew<Value>(proc, Trunc, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1)));
    Value* result = root->appendNew<Value>(proc, BitAnd, Origin(), argumentA, argumentB);
    root->appendNewControlValue(proc, Return, Origin(), result);

    CHECK(isIdentical(compileAndRun<float>(proc, bitwise_cast<int32_t>(a), bitwise_cast<int32_t>(b)), bitAndFloat(a, b)));
}

static void testBitAndArgImmFloat(float a, float b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argumentA = root->appendNew<Value>(proc, BitwiseCast, Origin(),
        root->appendNew<Value>(proc, Trunc, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)));
    Value* argumentB = root->appendNew<ConstFloatValue>(proc, Origin(), b);
    Value* result = root->appendNew<Value>(proc, BitAnd, Origin(), argumentA, argumentB);
    root->appendNewControlValue(proc, Return, Origin(), result);

    CHECK(isIdentical(compileAndRun<float>(proc, bitwise_cast<int32_t>(a), bitwise_cast<int32_t>(b)), bitAndFloat(a, b)));
}

static void testBitAndImmsFloat(float a, float b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argumentA = root->appendNew<ConstFloatValue>(proc, Origin(), a);
    Value* argumentB = root->appendNew<ConstFloatValue>(proc, Origin(), b);
    Value* result = root->appendNew<Value>(proc, BitAnd, Origin(), argumentA, argumentB);
    root->appendNewControlValue(proc, Return, Origin(), result);

    CHECK(isIdentical(compileAndRun<float>(proc), bitAndFloat(a, b)));
}

static void testBitAndArgsFloatWithUselessDoubleConversion(float a, float b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argumentA = root->appendNew<Value>(proc, BitwiseCast, Origin(),
        root->appendNew<Value>(proc, Trunc, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)));
    Value* argumentB = root->appendNew<Value>(proc, BitwiseCast, Origin(),
        root->appendNew<Value>(proc, Trunc, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1)));
    Value* argumentAasDouble = root->appendNew<Value>(proc, FloatToDouble, Origin(), argumentA);
    Value* argumentBasDouble = root->appendNew<Value>(proc, FloatToDouble, Origin(), argumentB);
    Value* doubleResult = root->appendNew<Value>(proc, BitAnd, Origin(), argumentAasDouble, argumentBasDouble);
    Value* floatResult = root->appendNew<Value>(proc, DoubleToFloat, Origin(), doubleResult);
    root->appendNewControlValue(proc, Return, Origin(), floatResult);

    double doubleA = a;
    double doubleB = b;
    float expected = static_cast<float>(bitAndDouble(doubleA, doubleB));
    CHECK(isIdentical(compileAndRun<float>(proc, bitwise_cast<int32_t>(a), bitwise_cast<int32_t>(b)), expected));
}

static void testBitOrArgs(int64_t a, int64_t b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, BitOr, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1)));

    CHECK(compileAndRun<int64_t>(proc, a, b) == (a | b));
}

static void testBitOrSameArg(int64_t a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argument = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, BitOr, Origin(),
            argument,
            argument));

    CHECK(compileAndRun<int64_t>(proc, a) == a);
}

static void testBitOrAndAndArgs(int64_t a, int64_t b, int64_t c)
{
    // We want to check every possible ordering of arguments (to properly check every path in B3ReduceStrength):
    // ((a & b) | (a & c))
    // ((a & b) | (c & a))
    // ((b & a) | (a & c))
    // ((b & a) | (c & a))
    for (int i = 0; i < 4; ++i) {
        Procedure proc;
        BasicBlock* root = proc.addBlock();
        Value* argA = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
        Value* argB = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
        Value* argC = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR2);
        Value* andAB = i & 2 ? root->appendNew<Value>(proc, BitAnd, Origin(), argA, argB)
            : root->appendNew<Value>(proc, BitAnd, Origin(), argB, argA);
        Value* andAC = i & 1 ? root->appendNew<Value>(proc, BitAnd, Origin(), argA, argC)
            : root->appendNew<Value>(proc, BitAnd, Origin(), argC, argA);
        root->appendNewControlValue(
            proc, Return, Origin(),
            root->appendNew<Value>(
                proc, BitOr, Origin(),
                andAB,
                andAC));

        CHECK_EQ(compileAndRun<int64_t>(proc, a, b, c), ((a & b) | (a & c)));
    }
}

static void testBitOrAndAndArgs32(int32_t a, int32_t b, int32_t c)
{
    // We want to check every possible ordering of arguments (to properly check every path in B3ReduceStrength):
    // ((a & b) | (a & c))
    // ((a & b) | (c & a))
    // ((b & a) | (a & c))
    // ((b & a) | (c & a))
    for (int i = 0; i < 4; ++i) {
        Procedure proc;
        BasicBlock* root = proc.addBlock();
        Value* argA = root->appendNew<Value>(proc, Trunc, Origin(), root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
        Value* argB = root->appendNew<Value>(proc, Trunc, Origin(), root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1));
        Value* argC = root->appendNew<Value>(proc, Trunc, Origin(), root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR2));
        Value* andAB = i & 2 ? root->appendNew<Value>(proc, BitAnd, Origin(), argA, argB)
            : root->appendNew<Value>(proc, BitAnd, Origin(), argB, argA);
        Value* andAC = i & 1 ? root->appendNew<Value>(proc, BitAnd, Origin(), argA, argC)
            : root->appendNew<Value>(proc, BitAnd, Origin(), argC, argA);
        root->appendNewControlValue(
            proc, Return, Origin(),
            root->appendNew<Value>(
                proc, BitOr, Origin(),
                andAB,
                andAC));

        CHECK_EQ(compileAndRun<int32_t>(proc, a, b, c), ((a & b) | (a & c)));
    }
}

static void testBitOrAndSameArgs(int64_t a, int64_t b)
{
    // We want to check every possible ordering of arguments (to properly check every path in B3ReduceStrength):
    // ((a & b) | a)
    // ((b & a) | a)
    // (a | (a & b))
    // (a | (b & a))
    for (int i = 0; i < 4; ++i) {
        Procedure proc;
        BasicBlock* root = proc.addBlock();
        Value* argA = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
        Value* argB = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
        Value* andAB = i & 1 ? root->appendNew<Value>(proc, BitAnd, Origin(), argA, argB)
            : root->appendNew<Value>(proc, BitAnd, Origin(), argB, argA);
        Value* result = i & 2 ? root->appendNew<Value>(proc, BitOr, Origin(), andAB, argA)
            : root->appendNew<Value>(proc, BitOr, Origin(), argA, andAB);
        root->appendNewControlValue(proc, Return, Origin(), result);

        CHECK_EQ(compileAndRun<int64_t>(proc, a, b), ((a & b) | a));
    }
}

static void testBitOrAndSameArgs32(int32_t a, int32_t b)
{
    // We want to check every possible ordering of arguments (to properly check every path in B3ReduceStrength):
    // ((a & b) | a)
    // ((b & a) | a)
    // (a | (a & b))
    // (a | (b & a))
    for (int i = 0; i < 4; ++i) {
        Procedure proc;
        BasicBlock* root = proc.addBlock();
        Value* argA = root->appendNew<Value>(proc, Trunc, Origin(), root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
        Value* argB = root->appendNew<Value>(proc, Trunc, Origin(), root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1));
        Value* andAB = i & 1 ? root->appendNew<Value>(proc, BitAnd, Origin(), argA, argB)
            : root->appendNew<Value>(proc, BitAnd, Origin(), argB, argA);
        Value* result = i & 2 ? root->appendNew<Value>(proc, BitOr, Origin(), andAB, argA)
            : root->appendNew<Value>(proc, BitOr, Origin(), argA, andAB);
        root->appendNewControlValue(proc, Return, Origin(), result);

        CHECK_EQ(compileAndRun<int32_t>(proc, a, b), ((a & b) | a));
    }
}

static void testBitOrNotNot(int64_t a, int64_t b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argA = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    Value* argB = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
    Value* notA = root->appendNew<Value>(proc, BitXor, Origin(), argA, root->appendNew<Const64Value>(proc, Origin(), -1));
    Value* notB = root->appendNew<Value>(proc, BitXor, Origin(), argB, root->appendNew<Const64Value>(proc, Origin(), -1));
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, BitOr, Origin(),
            notA,
            notB));

    CHECK_EQ(compileAndRun<int64_t>(proc, a, b), (~a | ~b));
}

static void testBitOrNotNot32(int32_t a, int32_t b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argA = root->appendNew<Value>(proc, Trunc, Origin(), root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* argB = root->appendNew<Value>(proc, Trunc, Origin(), root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1));
    Value* notA = root->appendNew<Value>(proc, BitXor, Origin(), argA, root->appendNew<Const32Value>(proc, Origin(), -1));
    Value* notB = root->appendNew<Value>(proc, BitXor, Origin(), argB, root->appendNew<Const32Value>(proc, Origin(), -1));
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, BitOr, Origin(),
            notA,
            notB));

    CHECK_EQ(compileAndRun<int32_t>(proc, a, b), (~a | ~b));
}

static void testBitOrNotImm(int64_t a, int64_t b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argA = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    Value* notA = root->appendNew<Value>(proc, BitXor, Origin(), argA, root->appendNew<Const64Value>(proc, Origin(), -1));
    Value* cstB = root->appendNew<Const64Value>(proc, Origin(), b);
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, BitOr, Origin(),
            notA,
            cstB));

    CHECK_EQ(compileAndRun<int64_t>(proc, a, b), (~a | b));
}

static void testBitOrNotImm32(int32_t a, int32_t b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argA = root->appendNew<Value>(proc, Trunc, Origin(), root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* notA = root->appendNew<Value>(proc, BitXor, Origin(), argA, root->appendNew<Const32Value>(proc, Origin(), -1));
    Value* cstB = root->appendNew<Const32Value>(proc, Origin(), b);
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, BitOr, Origin(),
            notA,
            cstB));

    CHECK_EQ(compileAndRun<int32_t>(proc, a), (~a | b));
}

static void testBitOrImms(int64_t a, int64_t b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, BitOr, Origin(),
            root->appendNew<Const64Value>(proc, Origin(), a),
            root->appendNew<Const64Value>(proc, Origin(), b)));

    CHECK(compileAndRun<int64_t>(proc) == (a | b));
}

static void testBitOrArgImm(int64_t a, int64_t b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, BitOr, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
            root->appendNew<Const64Value>(proc, Origin(), b)));

    CHECK(compileAndRun<int64_t>(proc, a) == (a | b));
}

static void testBitOrImmArg(int64_t a, int64_t b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, BitOr, Origin(),
            root->appendNew<Const64Value>(proc, Origin(), a),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)));

    CHECK(compileAndRun<int64_t>(proc, b) == (a | b));
}

static void testBitOrBitOrArgImmImm(int64_t a, int64_t b, int64_t c)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* innerBitOr = root->appendNew<Value>(
        proc, BitOr, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
        root->appendNew<Const64Value>(proc, Origin(), b));
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, BitOr, Origin(),
            innerBitOr,
            root->appendNew<Const64Value>(proc, Origin(), c)));

    CHECK(compileAndRun<int64_t>(proc, a) == ((a | b) | c));
}

static void testBitOrImmBitOrArgImm(int64_t a, int64_t b, int64_t c)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* innerBitOr = root->appendNew<Value>(
        proc, BitOr, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
        root->appendNew<Const64Value>(proc, Origin(), c));
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, BitOr, Origin(),
            root->appendNew<Const64Value>(proc, Origin(), a),
            innerBitOr));

    CHECK(compileAndRun<int64_t>(proc, b) == (a | (b | c)));
}

static void testBitOrArgs32(int a, int b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, BitOr, Origin(),
            root->appendNew<Value>(
                proc, Trunc, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)),
            root->appendNew<Value>(
                proc, Trunc, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1))));

    CHECK(compileAndRun<int>(proc, a, b) == (a | b));
}

static void testBitOrSameArg32(int a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argument = root->appendNew<Value>(
        proc, Trunc, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, BitOr, Origin(),
            argument,
            argument));

    CHECK(compileAndRun<int>(proc, a) == a);
}

static void testBitOrImms32(int a, int b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, BitOr, Origin(),
            root->appendNew<Const32Value>(proc, Origin(), a),
            root->appendNew<Const32Value>(proc, Origin(), b)));

    CHECK(compileAndRun<int>(proc) == (a | b));
}

static void testBitOrArgImm32(int a, int b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, BitOr, Origin(),
            root->appendNew<Value>(
                proc, Trunc, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)),
            root->appendNew<Const32Value>(proc, Origin(), b)));

    CHECK(compileAndRun<int>(proc, a) == (a | b));
}

static void testBitOrImmArg32(int a, int b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, BitOr, Origin(),
            root->appendNew<Const32Value>(proc, Origin(), a),
            root->appendNew<Value>(
                proc, Trunc, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0))));

    CHECK(compileAndRun<int>(proc, b) == (a | b));
}

void addBitTests(const char* filter, Deque<RefPtr<SharedTask<void()>>>& tasks)
{
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
    RUN_BINARY(testBitAndNotNot32, int32Operands(), int32Operands());
    RUN_BINARY(testBitAndNotImm, int64Operands(), int64Operands());
    RUN_BINARY(testBitAndNotImm32, int32Operands(), int32Operands());
    
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
    RUN_TERNARY(testBitOrAndAndArgs32, int32Operands(), int32Operands(), int32Operands());
    RUN_BINARY(testBitOrAndSameArgs, int64Operands(), int64Operands());
    RUN_BINARY(testBitOrAndSameArgs32, int32Operands(), int32Operands());
    RUN_BINARY(testBitOrNotNot, int64Operands(), int64Operands());
    RUN_BINARY(testBitOrNotNot32, int32Operands(), int32Operands());
    RUN_BINARY(testBitOrNotImm, int64Operands(), int64Operands());
    RUN_BINARY(testBitOrNotImm32, int32Operands(), int32Operands());
    
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
    RUN_TERNARY(testBitXorAndAndArgs32, int32Operands(), int32Operands(), int32Operands());
    RUN_BINARY(testBitXorAndSameArgs, int64Operands(), int64Operands());
    RUN_BINARY(testBitXorAndSameArgs32, int32Operands(), int32Operands());
    
    RUN_UNARY(testBitNotArg, int64Operands());
    RUN_UNARY(testBitNotImm, int64Operands());
    RUN_UNARY(testBitNotMem, int64Operands());
    RUN_UNARY(testBitNotArg32, int32Operands());
    RUN_UNARY(testBitNotImm32, int32Operands());
    RUN_UNARY(testBitNotMem32, int32Operands());
    RUN_BINARY(testNotOnBooleanAndBranch32, int32Operands(), int32Operands());
    RUN_BINARY(testBitNotOnBooleanAndBranch32, int32Operands(), int32Operands());
    
    RUN_BINARY(testBitXorTreeArgs, int64Operands(), int64Operands());
    RUN_BINARY(testBitXorTreeArgsEven, int64Operands(), int64Operands());
    RUN_BINARY(testBitXorTreeArgImm, int64Operands(), int64Operands());
    RUN_UNARY(testBitAndTreeArg32, int32Operands());
    RUN_UNARY(testBitOrTreeArg32, int32Operands());
}

#endif // ENABLE(B3_JIT)
