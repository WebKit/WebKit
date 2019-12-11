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

void testBitOrBitOrArgImmImm32(int a, int b, int c)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* innerBitOr = root->appendNew<Value>(
        proc, BitOr, Origin(),
        root->appendNew<Value>(
            proc, Trunc, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)),
        root->appendNew<Const32Value>(proc, Origin(), b));
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, BitOr, Origin(),
            innerBitOr,
            root->appendNew<Const32Value>(proc, Origin(), c)));

    CHECK(compileAndRun<int>(proc, a) == ((a | b) | c));
}

void testBitOrImmBitOrArgImm32(int a, int b, int c)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* innerBitOr = root->appendNew<Value>(
        proc, BitOr, Origin(),
        root->appendNew<Value>(
            proc, Trunc, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)),
        root->appendNew<Const32Value>(proc, Origin(), c));
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, BitOr, Origin(),
            root->appendNew<Const32Value>(proc, Origin(), a),
            innerBitOr));

    CHECK(compileAndRun<int>(proc, b) == (a | (b | c)));
}

double bitOrDouble(double a, double b)
{
    return bitwise_cast<double>(bitwise_cast<uint64_t>(a) | bitwise_cast<uint64_t>(b));
}

void testBitOrArgDouble(double a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argument = root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR0);
    Value* result = root->appendNew<Value>(proc, BitOr, Origin(), argument, argument);
    root->appendNewControlValue(proc, Return, Origin(), result);

    CHECK(isIdentical(compileAndRun<double>(proc, a), bitOrDouble(a, a)));
}

void testBitOrArgsDouble(double a, double b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argumentA = root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR0);
    Value* argumentB = root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR1);
    Value* result = root->appendNew<Value>(proc, BitOr, Origin(), argumentA, argumentB);
    root->appendNewControlValue(proc, Return, Origin(), result);

    CHECK(isIdentical(compileAndRun<double>(proc, a, b), bitOrDouble(a, b)));
}

void testBitOrArgImmDouble(double a, double b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argumentA = root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR0);
    Value* argumentB = root->appendNew<ConstDoubleValue>(proc, Origin(), b);
    Value* result = root->appendNew<Value>(proc, BitOr, Origin(), argumentA, argumentB);
    root->appendNewControlValue(proc, Return, Origin(), result);

    CHECK(isIdentical(compileAndRun<double>(proc, a, b), bitOrDouble(a, b)));
}

void testBitOrImmsDouble(double a, double b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argumentA = root->appendNew<ConstDoubleValue>(proc, Origin(), a);
    Value* argumentB = root->appendNew<ConstDoubleValue>(proc, Origin(), b);
    Value* result = root->appendNew<Value>(proc, BitOr, Origin(), argumentA, argumentB);
    root->appendNewControlValue(proc, Return, Origin(), result);

    CHECK(isIdentical(compileAndRun<double>(proc), bitOrDouble(a, b)));
}

float bitOrFloat(float a, float b)
{
    return bitwise_cast<float>(bitwise_cast<uint32_t>(a) | bitwise_cast<uint32_t>(b));
}

void testBitOrArgFloat(float a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argument = root->appendNew<Value>(proc, BitwiseCast, Origin(),
        root->appendNew<Value>(proc, Trunc, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)));
    Value* result = root->appendNew<Value>(proc, BitOr, Origin(), argument, argument);
    root->appendNewControlValue(proc, Return, Origin(), result);

    CHECK(isIdentical(compileAndRun<float>(proc, bitwise_cast<int32_t>(a)), bitOrFloat(a, a)));
}

void testBitOrArgsFloat(float a, float b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argumentA = root->appendNew<Value>(proc, BitwiseCast, Origin(),
        root->appendNew<Value>(proc, Trunc, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)));
    Value* argumentB = root->appendNew<Value>(proc, BitwiseCast, Origin(),
        root->appendNew<Value>(proc, Trunc, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1)));
    Value* result = root->appendNew<Value>(proc, BitOr, Origin(), argumentA, argumentB);
    root->appendNewControlValue(proc, Return, Origin(), result);

    CHECK(isIdentical(compileAndRun<float>(proc, bitwise_cast<int32_t>(a), bitwise_cast<int32_t>(b)), bitOrFloat(a, b)));
}

void testBitOrArgImmFloat(float a, float b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argumentA = root->appendNew<Value>(proc, BitwiseCast, Origin(),
        root->appendNew<Value>(proc, Trunc, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)));
    Value* argumentB = root->appendNew<ConstFloatValue>(proc, Origin(), b);
    Value* result = root->appendNew<Value>(proc, BitOr, Origin(), argumentA, argumentB);
    root->appendNewControlValue(proc, Return, Origin(), result);

    CHECK(isIdentical(compileAndRun<float>(proc, bitwise_cast<int32_t>(a), bitwise_cast<int32_t>(b)), bitOrFloat(a, b)));
}

void testBitOrImmsFloat(float a, float b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argumentA = root->appendNew<ConstFloatValue>(proc, Origin(), a);
    Value* argumentB = root->appendNew<ConstFloatValue>(proc, Origin(), b);
    Value* result = root->appendNew<Value>(proc, BitOr, Origin(), argumentA, argumentB);
    root->appendNewControlValue(proc, Return, Origin(), result);

    CHECK(isIdentical(compileAndRun<float>(proc), bitOrFloat(a, b)));
}

void testBitOrArgsFloatWithUselessDoubleConversion(float a, float b)
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
    Value* doubleResult = root->appendNew<Value>(proc, BitOr, Origin(), argumentAasDouble, argumentBasDouble);
    Value* floatResult = root->appendNew<Value>(proc, DoubleToFloat, Origin(), doubleResult);
    root->appendNewControlValue(proc, Return, Origin(), floatResult);

    double doubleA = a;
    double doubleB = b;
    float expected = static_cast<float>(bitOrDouble(doubleA, doubleB));
    CHECK(isIdentical(compileAndRun<float>(proc, bitwise_cast<int32_t>(a), bitwise_cast<int32_t>(b)), expected));
}

void testBitXorArgs(int64_t a, int64_t b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, BitXor, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1)));

    CHECK(compileAndRun<int64_t>(proc, a, b) == (a ^ b));
}

void testBitXorSameArg(int64_t a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argument = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, BitXor, Origin(),
            argument,
            argument));

    CHECK(!compileAndRun<int64_t>(proc, a));
}

void testBitXorAndAndArgs(int64_t a, int64_t b, int64_t c)
{
    // We want to check every possible ordering of arguments (to properly check every path in B3ReduceStrength):
    // ((a & b) ^ (a & c))
    // ((a & b) ^ (c & a))
    // ((b & a) ^ (a & c))
    // ((b & a) ^ (c & a))
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
                proc, BitXor, Origin(),
                andAB,
                andAC));

        CHECK_EQ(compileAndRun<int64_t>(proc, a, b, c), ((a & b) ^ (a & c)));
    }
}

void testBitXorAndAndArgs32(int32_t a, int32_t b, int32_t c)
{
    // We want to check every possible ordering of arguments (to properly check every path in B3ReduceStrength):
    // ((a & b) ^ (a & c))
    // ((a & b) ^ (c & a))
    // ((b & a) ^ (a & c))
    // ((b & a) ^ (c & a))
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
                proc, BitXor, Origin(),
                andAB,
                andAC));

        CHECK_EQ(compileAndRun<int32_t>(proc, a, b, c), ((a & b) ^ (a & c)));
    }
}

void testBitXorAndSameArgs(int64_t a, int64_t b)
{
    // We want to check every possible ordering of arguments (to properly check every path in B3ReduceStrength):
    // ((a & b) ^ a)
    // ((b & a) ^ a)
    // (a ^ (a & b))
    // (a ^ (b & a))
    for (int i = 0; i < 4; ++i) {
        Procedure proc;
        BasicBlock* root = proc.addBlock();
        Value* argA = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
        Value* argB = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
        Value* andAB = i & 1 ? root->appendNew<Value>(proc, BitAnd, Origin(), argA, argB)
            : root->appendNew<Value>(proc, BitAnd, Origin(), argB, argA);
        Value* result = i & 2 ? root->appendNew<Value>(proc, BitXor, Origin(), andAB, argA)
            : root->appendNew<Value>(proc, BitXor, Origin(), argA, andAB);
        root->appendNewControlValue(proc, Return, Origin(), result);
    
        CHECK_EQ(compileAndRun<int64_t>(proc, a, b), ((a & b) ^ a));
    }
}

void testBitXorAndSameArgs32(int32_t a, int32_t b)
{
    // We want to check every possible ordering of arguments (to properly check every path in B3ReduceStrength):
    // ((a & b) ^ a)
    // ((b & a) ^ a)
    // (a ^ (a & b))
    // (a ^ (b & a))
    for (int i = 0; i < 4; ++i) {
        Procedure proc;
        BasicBlock* root = proc.addBlock();
        Value* argA = root->appendNew<Value>(proc, Trunc, Origin(), root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
        Value* argB = root->appendNew<Value>(proc, Trunc, Origin(), root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1));
        Value* andAB = i & 1 ? root->appendNew<Value>(proc, BitAnd, Origin(), argA, argB)
            : root->appendNew<Value>(proc, BitAnd, Origin(), argB, argA);
        Value* result = i & 2 ? root->appendNew<Value>(proc, BitXor, Origin(), andAB, argA)
            : root->appendNew<Value>(proc, BitXor, Origin(), argA, andAB);
        root->appendNewControlValue(proc, Return, Origin(), result);

        CHECK_EQ(compileAndRun<int32_t>(proc, a, b), ((a & b) ^ a));
    }
}

void testBitXorImms(int64_t a, int64_t b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, BitXor, Origin(),
            root->appendNew<Const64Value>(proc, Origin(), a),
            root->appendNew<Const64Value>(proc, Origin(), b)));

    CHECK(compileAndRun<int64_t>(proc) == (a ^ b));
}

void testBitXorArgImm(int64_t a, int64_t b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, BitXor, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
            root->appendNew<Const64Value>(proc, Origin(), b)));

    CHECK(compileAndRun<int64_t>(proc, a) == (a ^ b));
}

void testBitXorImmArg(int64_t a, int64_t b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, BitXor, Origin(),
            root->appendNew<Const64Value>(proc, Origin(), a),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)));

    CHECK(compileAndRun<int64_t>(proc, b) == (a ^ b));
}

void testBitXorBitXorArgImmImm(int64_t a, int64_t b, int64_t c)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* innerBitXor = root->appendNew<Value>(
        proc, BitXor, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
        root->appendNew<Const64Value>(proc, Origin(), b));
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, BitXor, Origin(),
            innerBitXor,
            root->appendNew<Const64Value>(proc, Origin(), c)));

    CHECK(compileAndRun<int64_t>(proc, a) == ((a ^ b) ^ c));
}

void testBitXorImmBitXorArgImm(int64_t a, int64_t b, int64_t c)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* innerBitXor = root->appendNew<Value>(
        proc, BitXor, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
        root->appendNew<Const64Value>(proc, Origin(), c));
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, BitXor, Origin(),
            root->appendNew<Const64Value>(proc, Origin(), a),
            innerBitXor));

    CHECK(compileAndRun<int64_t>(proc, b) == (a ^ (b ^ c)));
}

void testBitXorArgs32(int a, int b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, BitXor, Origin(),
            root->appendNew<Value>(
                proc, Trunc, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)),
            root->appendNew<Value>(
                proc, Trunc, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1))));

    CHECK(compileAndRun<int>(proc, a, b) == (a ^ b));
}

void testBitXorSameArg32(int a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argument = root->appendNew<Value>(
        proc, Trunc, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, BitXor, Origin(),
            argument,
            argument));

    CHECK(!compileAndRun<int>(proc, a));
}

void testBitXorImms32(int a, int b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, BitXor, Origin(),
            root->appendNew<Const32Value>(proc, Origin(), a),
            root->appendNew<Const32Value>(proc, Origin(), b)));

    CHECK(compileAndRun<int>(proc) == (a ^ b));
}

void testBitXorArgImm32(int a, int b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, BitXor, Origin(),
            root->appendNew<Value>(
                proc, Trunc, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)),
            root->appendNew<Const32Value>(proc, Origin(), b)));

    CHECK(compileAndRun<int>(proc, a) == (a ^ b));
}

void testBitXorImmArg32(int a, int b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, BitXor, Origin(),
            root->appendNew<Const32Value>(proc, Origin(), a),
            root->appendNew<Value>(
                proc, Trunc, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0))));

    CHECK(compileAndRun<int>(proc, b) == (a ^ b));
}

void testBitXorBitXorArgImmImm32(int a, int b, int c)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* innerBitXor = root->appendNew<Value>(
        proc, BitXor, Origin(),
        root->appendNew<Value>(
            proc, Trunc, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)),
        root->appendNew<Const32Value>(proc, Origin(), b));
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, BitXor, Origin(),
            innerBitXor,
            root->appendNew<Const32Value>(proc, Origin(), c)));

    CHECK(compileAndRun<int>(proc, a) == ((a ^ b) ^ c));
}

void testBitXorImmBitXorArgImm32(int a, int b, int c)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* innerBitXor = root->appendNew<Value>(
        proc, BitXor, Origin(),
        root->appendNew<Value>(
            proc, Trunc, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)),
        root->appendNew<Const32Value>(proc, Origin(), c));
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, BitXor, Origin(),
            root->appendNew<Const32Value>(proc, Origin(), a),
            innerBitXor));

    CHECK(compileAndRun<int>(proc, b) == (a ^ (b ^ c)));
}

void testBitNotArg(int64_t a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, BitXor, Origin(),
            root->appendNew<Const64Value>(proc, Origin(), -1),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)));

    CHECK(isIdentical(compileAndRun<int64_t>(proc, a), static_cast<int64_t>((static_cast<uint64_t>(a) ^ 0xffffffffffffffff))));
}

void testBitNotImm(int64_t a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, BitXor, Origin(),
            root->appendNew<Const64Value>(proc, Origin(), -1),
            root->appendNew<Const64Value>(proc, Origin(), a)));

    CHECK(isIdentical(compileAndRun<int64_t>(proc, a), static_cast<int64_t>((static_cast<uint64_t>(a) ^ 0xffffffffffffffff))));
}

void testBitNotMem(int64_t a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* address = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    MemoryValue* load = root->appendNew<MemoryValue>(proc, Load, Int64, Origin(), address);
    Value* notLoad = root->appendNew<Value>(proc, BitXor, Origin(),
        root->appendNew<Const64Value>(proc, Origin(), -1),
        load);
    root->appendNew<MemoryValue>(proc, Store, Origin(), notLoad, address);
    root->appendNewControlValue(proc, Return, Origin(), root->appendNew<Const32Value>(proc, Origin(), 0));

    int64_t input = a;
    compileAndRun<int32_t>(proc, &input);
    CHECK(isIdentical(input, static_cast<int64_t>((static_cast<uint64_t>(a) ^ 0xffffffffffffffff))));
}

void testBitNotArg32(int32_t a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argument = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(proc, BitXor, Origin(),
            root->appendNew<Const32Value>(proc, Origin(), -1),
            argument));
    CHECK(isIdentical(compileAndRun<int32_t>(proc, a), static_cast<int32_t>((static_cast<uint32_t>(a) ^ 0xffffffff))));
}

void testBitNotImm32(int32_t a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, BitXor, Origin(),
            root->appendNew<Const32Value>(proc, Origin(), -1),
            root->appendNew<Const32Value>(proc, Origin(), a)));

    CHECK(isIdentical(compileAndRun<int32_t>(proc, a), static_cast<int32_t>((static_cast<uint32_t>(a) ^ 0xffffffff))));
}

void testBitNotMem32(int32_t a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* address = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    MemoryValue* load = root->appendNew<MemoryValue>(proc, Load, Int32, Origin(), address);
    Value* notLoad = root->appendNew<Value>(proc, BitXor, Origin(),
        root->appendNew<Const32Value>(proc, Origin(), -1),
        load);
    root->appendNew<MemoryValue>(proc, Store, Origin(), notLoad, address);
    root->appendNewControlValue(proc, Return, Origin(), root->appendNew<Const32Value>(proc, Origin(), 0));

    int32_t input = a;
    compileAndRun<int32_t>(proc, &input);
    CHECK(isIdentical(input, static_cast<int32_t>((static_cast<uint32_t>(a) ^ 0xffffffff))));
}

void testNotOnBooleanAndBranch32(int64_t a, int64_t b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* thenCase = proc.addBlock();
    BasicBlock* elseCase = proc.addBlock();

    Value* arg1 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* arg2 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1));
    Value* argsAreEqual = root->appendNew<Value>(proc, Equal, Origin(), arg1, arg2);
    Value* argsAreNotEqual = root->appendNew<Value>(proc, BitXor, Origin(),
        root->appendNew<Const32Value>(proc, Origin(), 1),
        argsAreEqual);

    root->appendNewControlValue(
        proc, Branch, Origin(),
        argsAreNotEqual,
        FrequentedBlock(thenCase), FrequentedBlock(elseCase));

    thenCase->appendNewControlValue(
        proc, Return, Origin(),
        thenCase->appendNew<Const32Value>(proc, Origin(), 42));

    elseCase->appendNewControlValue(
        proc, Return, Origin(),
        elseCase->appendNew<Const32Value>(proc, Origin(), -42));

    int32_t expectedValue = (a != b) ? 42 : -42;
    CHECK(compileAndRun<int32_t>(proc, a, b) == expectedValue);
}

void testBitNotOnBooleanAndBranch32(int64_t a, int64_t b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* thenCase = proc.addBlock();
    BasicBlock* elseCase = proc.addBlock();

    Value* arg1 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* arg2 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1));
    Value* argsAreEqual = root->appendNew<Value>(proc, Equal, Origin(), arg1, arg2);
    Value* bitNotArgsAreEqual = root->appendNew<Value>(proc, BitXor, Origin(),
        root->appendNew<Const32Value>(proc, Origin(), -1),
        argsAreEqual);

    root->appendNewControlValue(proc, Branch, Origin(),
        bitNotArgsAreEqual, FrequentedBlock(thenCase), FrequentedBlock(elseCase));

    thenCase->appendNewControlValue(proc, Return, Origin(),
        thenCase->appendNew<Const32Value>(proc, Origin(), 42));

    elseCase->appendNewControlValue(proc, Return, Origin(),
        elseCase->appendNew<Const32Value>(proc, Origin(), -42));

    static constexpr int32_t expectedValue = 42;
    CHECK(compileAndRun<int32_t>(proc, a, b) == expectedValue);
}

void testShlArgs(int64_t a, int64_t b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, Shl, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
            root->appendNew<Value>(
                proc, Trunc, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1))));

    CHECK(compileAndRun<int64_t>(proc, a, b) == (a << b));
}

void testShlImms(int64_t a, int64_t b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, Shl, Origin(),
            root->appendNew<Const64Value>(proc, Origin(), a),
            root->appendNew<Const32Value>(proc, Origin(), b)));

    b = b & 0x3f; // to avoid undefined behaviour below
    CHECK(compileAndRun<int64_t>(proc) == (a << b));
}

void testShlArgImm(int64_t a, int64_t b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, Shl, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
            root->appendNew<Const32Value>(proc, Origin(), b)));

    b = b & 0x3f; // to avoid undefined behaviour below
    CHECK(compileAndRun<int64_t>(proc, a) == (a << b));
}

void testShlSShrArgImm(int64_t a, int64_t b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argA = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    Value* constB = root->appendNew<Const32Value>(proc, Origin(), b);
    Value* innerShift = root->appendNew<Value>(proc, SShr, Origin(), argA, constB);
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, Shl, Origin(),
            innerShift,
            constB));

    b = b & 0x3f; // to avoid undefined behaviour below
    CHECK(compileAndRun<int64_t>(proc, a) == ((a >> b) << b));
}

void testShlArg32(int32_t a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* value = root->appendNew<Value>(
        proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(proc, Shl, Origin(), value, value));

    CHECK(compileAndRun<int32_t>(proc, a) == (a << a));
}

void testShlArgs32(int32_t a, int32_t b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, Shl, Origin(),
            root->appendNew<Value>(
                proc, Trunc, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)),
            root->appendNew<Value>(
                proc, Trunc, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1))));

    CHECK(compileAndRun<int32_t>(proc, a, b) == (a << b));
}

void testShlImms32(int32_t a, int32_t b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, Shl, Origin(),
            root->appendNew<Const32Value>(proc, Origin(), a),
            root->appendNew<Const32Value>(proc, Origin(), b)));

    b = b & 0x1f; // to avoid undefined behaviour below
    CHECK(compileAndRun<int32_t>(proc) == (a << b));
}

void testShlArgImm32(int32_t a, int32_t b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, Shl, Origin(),
            root->appendNew<Value>(
                proc, Trunc, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)),
            root->appendNew<Const32Value>(proc, Origin(), b)));

    b = b & 0x1f; // to avoid undefined behaviour below
    CHECK(compileAndRun<int32_t>(proc, a) == (a << b));
}

void testShlZShrArgImm32(int32_t a, int32_t b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argA = root->appendNew<Value>(
        proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* constB = root->appendNew<Const32Value>(proc, Origin(), b);
    Value* innerShift = root->appendNew<Value>(proc, ZShr, Origin(), argA, constB);
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, Shl, Origin(),
            innerShift,
            constB));

    b = b & 0x1f; // to avoid undefined behaviour below
    CHECK(compileAndRun<int32_t>(proc, a) == static_cast<int32_t>((static_cast<uint32_t>(a) >> b) << b));
}

static void testSShrArgs(int64_t a, int64_t b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, SShr, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
            root->appendNew<Value>(
                proc, Trunc, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1))));

    CHECK(compileAndRun<int64_t>(proc, a, b) == (a >> b));
}

static void testSShrImms(int64_t a, int64_t b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, SShr, Origin(),
            root->appendNew<Const64Value>(proc, Origin(), a),
            root->appendNew<Const32Value>(proc, Origin(), b)));

    CHECK(compileAndRun<int64_t>(proc) == (a >> b));
}

static void testSShrArgImm(int64_t a, int64_t b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, SShr, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
            root->appendNew<Const32Value>(proc, Origin(), b)));

    CHECK(compileAndRun<int64_t>(proc, a) == (a >> b));
}

static void testSShrArg32(int32_t a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* value = root->appendNew<Value>(
        proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(proc, SShr, Origin(), value, value));

    CHECK(compileAndRun<int32_t>(proc, a) == (a >> (a & 31)));
}

static void testSShrArgs32(int32_t a, int32_t b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, SShr, Origin(),
            root->appendNew<Value>(
                proc, Trunc, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)),
            root->appendNew<Value>(
                proc, Trunc, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1))));

    CHECK(compileAndRun<int32_t>(proc, a, b) == (a >> b));
}

static void testSShrImms32(int32_t a, int32_t b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, SShr, Origin(),
            root->appendNew<Const32Value>(proc, Origin(), a),
            root->appendNew<Const32Value>(proc, Origin(), b)));

    CHECK(compileAndRun<int32_t>(proc) == (a >> b));
}

static void testSShrArgImm32(int32_t a, int32_t b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, SShr, Origin(),
            root->appendNew<Value>(
                proc, Trunc, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)),
            root->appendNew<Const32Value>(proc, Origin(), b)));

    CHECK(compileAndRun<int32_t>(proc, a) == (a >> b));
}

static void testZShrArgs(uint64_t a, uint64_t b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, ZShr, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
            root->appendNew<Value>(
                proc, Trunc, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1))));

    CHECK(compileAndRun<uint64_t>(proc, a, b) == (a >> b));
}

static void testZShrImms(uint64_t a, uint64_t b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, ZShr, Origin(),
            root->appendNew<Const64Value>(proc, Origin(), a),
            root->appendNew<Const32Value>(proc, Origin(), b)));

    CHECK(compileAndRun<uint64_t>(proc) == (a >> b));
}

static void testZShrArgImm(uint64_t a, uint64_t b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, ZShr, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
            root->appendNew<Const32Value>(proc, Origin(), b)));

    CHECK(compileAndRun<uint64_t>(proc, a) == (a >> b));
}

static void testZShrArg32(uint32_t a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* value = root->appendNew<Value>(
        proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(proc, ZShr, Origin(), value, value));

    CHECK(compileAndRun<uint32_t>(proc, a) == (a >> (a & 31)));
}

static void testZShrArgs32(uint32_t a, uint32_t b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, ZShr, Origin(),
            root->appendNew<Value>(
                proc, Trunc, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)),
            root->appendNew<Value>(
                proc, Trunc, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1))));

    CHECK(compileAndRun<uint32_t>(proc, a, b) == (a >> b));
}

static void testZShrImms32(uint32_t a, uint32_t b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, ZShr, Origin(),
            root->appendNew<Const32Value>(proc, Origin(), a),
            root->appendNew<Const32Value>(proc, Origin(), b)));

    CHECK(compileAndRun<uint32_t>(proc) == (a >> b));
}

static void testZShrArgImm32(uint32_t a, uint32_t b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, ZShr, Origin(),
            root->appendNew<Value>(
                proc, Trunc, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)),
            root->appendNew<Const32Value>(proc, Origin(), b)));

    CHECK(compileAndRun<uint32_t>(proc, a) == (a >> b));
}

template<typename IntegerType>
static unsigned countLeadingZero(IntegerType value)
{
    unsigned bitCount = sizeof(IntegerType) * 8;
    if (!value)
        return bitCount;

    unsigned counter = 0;
    while (!(static_cast<uint64_t>(value) & (1l << (bitCount - 1)))) {
        value <<= 1;
        ++counter;
    }
    return counter;
}

void testClzArg64(int64_t a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argument = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    Value* clzValue = root->appendNew<Value>(proc, Clz, Origin(), argument);
    root->appendNewControlValue(proc, Return, Origin(), clzValue);
    CHECK(compileAndRun<unsigned>(proc, a) == countLeadingZero(a));
}

void testClzMem64(int64_t a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* address = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    MemoryValue* value = root->appendNew<MemoryValue>(proc, Load, Int64, Origin(), address);
    Value* clzValue = root->appendNew<Value>(proc, Clz, Origin(), value);
    root->appendNewControlValue(proc, Return, Origin(), clzValue);
    CHECK(compileAndRun<unsigned>(proc, &a) == countLeadingZero(a));
}

void testClzArg32(int32_t a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argument = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* clzValue = root->appendNew<Value>(proc, Clz, Origin(), argument);
    root->appendNewControlValue(proc, Return, Origin(), clzValue);
    CHECK(compileAndRun<unsigned>(proc, a) == countLeadingZero(a));
}

void testClzMem32(int32_t a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* address = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    MemoryValue* value = root->appendNew<MemoryValue>(proc, Load, Int32, Origin(), address);
    Value* clzValue = root->appendNew<Value>(proc, Clz, Origin(), value);
    root->appendNewControlValue(proc, Return, Origin(), clzValue);
    CHECK(compileAndRun<unsigned>(proc, &a) == countLeadingZero(a));
}

void testAbsArg(double a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, Abs, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR0)));

    CHECK(isIdentical(compileAndRun<double>(proc, a), fabs(a)));
}

void testAbsImm(double a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argument = root->appendNew<ConstDoubleValue>(proc, Origin(), a);
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(proc, Abs, Origin(), argument));

    CHECK(isIdentical(compileAndRun<double>(proc), fabs(a)));
}

void testAbsMem(double a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* address = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    MemoryValue* loadDouble = root->appendNew<MemoryValue>(proc, Load, Double, Origin(), address);
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(proc, Abs, Origin(), loadDouble));

    CHECK(isIdentical(compileAndRun<double>(proc, &a), fabs(a)));
}

void testAbsAbsArg(double a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* firstAbs = root->appendNew<Value>(proc, Abs, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR0));
    Value* secondAbs = root->appendNew<Value>(proc, Abs, Origin(), firstAbs);
    root->appendNewControlValue(proc, Return, Origin(), secondAbs);

    CHECK(isIdentical(compileAndRun<double>(proc, a), fabs(fabs(a))));
}

void testAbsNegArg(double a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* neg = root->appendNew<Value>(proc, Neg, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR0));
    Value* abs = root->appendNew<Value>(proc, Abs, Origin(), neg);
    root->appendNewControlValue(proc, Return, Origin(), abs);

    CHECK(isIdentical(compileAndRun<double>(proc, a), fabs(- a)));
}

void testAbsBitwiseCastArg(double a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argumentAsInt64 = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    Value* argumentAsDouble = root->appendNew<Value>(proc, BitwiseCast, Origin(), argumentAsInt64);
    Value* absValue = root->appendNew<Value>(proc, Abs, Origin(), argumentAsDouble);
    root->appendNewControlValue(proc, Return, Origin(), absValue);

    CHECK(isIdentical(compileAndRun<double>(proc, bitwise_cast<int64_t>(a)), fabs(a)));
}

void testBitwiseCastAbsBitwiseCastArg(double a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argumentAsInt64 = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    Value* argumentAsDouble = root->appendNew<Value>(proc, BitwiseCast, Origin(), argumentAsInt64);
    Value* absValue = root->appendNew<Value>(proc, Abs, Origin(), argumentAsDouble);
    Value* resultAsInt64 = root->appendNew<Value>(proc, BitwiseCast, Origin(), absValue);

    root->appendNewControlValue(proc, Return, Origin(), resultAsInt64);

    int64_t expectedResult = bitwise_cast<int64_t>(fabs(a));
    CHECK(isIdentical(compileAndRun<int64_t>(proc, bitwise_cast<int64_t>(a)), expectedResult));
}

void testAbsArg(float a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argument32 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* argument = root->appendNew<Value>(proc, BitwiseCast, Origin(), argument32);
    Value* result = root->appendNew<Value>(proc, Abs, Origin(), argument);
    Value* result32 = root->appendNew<Value>(proc, BitwiseCast, Origin(), result);
    root->appendNewControlValue(proc, Return, Origin(), result32);

    CHECK(isIdentical(compileAndRun<int32_t>(proc, bitwise_cast<int32_t>(a)), bitwise_cast<int32_t>(static_cast<float>(fabs(a)))));
}

void testAbsImm(float a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argument = root->appendNew<ConstFloatValue>(proc, Origin(), a);
    Value* result = root->appendNew<Value>(proc, Abs, Origin(), argument);
    Value* result32 = root->appendNew<Value>(proc, BitwiseCast, Origin(), result);
    root->appendNewControlValue(proc, Return, Origin(), result32);

    CHECK(isIdentical(compileAndRun<int32_t>(proc, bitwise_cast<int32_t>(a)), bitwise_cast<int32_t>(static_cast<float>(fabs(a)))));
}

void testAbsMem(float a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* address = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    MemoryValue* loadFloat = root->appendNew<MemoryValue>(proc, Load, Float, Origin(), address);
    Value* result = root->appendNew<Value>(proc, Abs, Origin(), loadFloat);
    Value* result32 = root->appendNew<Value>(proc, BitwiseCast, Origin(), result);
    root->appendNewControlValue(proc, Return, Origin(), result32);

    CHECK(isIdentical(compileAndRun<int32_t>(proc, &a), bitwise_cast<int32_t>(static_cast<float>(fabs(a)))));
}

void testAbsAbsArg(float a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argument32 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* argument = root->appendNew<Value>(proc, BitwiseCast, Origin(), argument32);
    Value* firstAbs = root->appendNew<Value>(proc, Abs, Origin(), argument);
    Value* secondAbs = root->appendNew<Value>(proc, Abs, Origin(), firstAbs);
    root->appendNewControlValue(proc, Return, Origin(), secondAbs);

    CHECK(isIdentical(compileAndRun<float>(proc, bitwise_cast<int32_t>(a)), static_cast<float>(fabs(fabs(a)))));
}

void testAbsNegArg(float a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argument32 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* argument = root->appendNew<Value>(proc, BitwiseCast, Origin(), argument32);
    Value* neg = root->appendNew<Value>(proc, Neg, Origin(), argument);
    Value* abs = root->appendNew<Value>(proc, Abs, Origin(), neg);
    root->appendNewControlValue(proc, Return, Origin(), abs);

    CHECK(isIdentical(compileAndRun<float>(proc, bitwise_cast<int32_t>(a)), static_cast<float>(fabs(- a))));
}

void testAbsBitwiseCastArg(float a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argumentAsInt32 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* argumentAsfloat = root->appendNew<Value>(proc, BitwiseCast, Origin(), argumentAsInt32);
    Value* absValue = root->appendNew<Value>(proc, Abs, Origin(), argumentAsfloat);
    root->appendNewControlValue(proc, Return, Origin(), absValue);

    CHECK(isIdentical(compileAndRun<float>(proc, bitwise_cast<int32_t>(a)), static_cast<float>(fabs(a))));
}

void testBitwiseCastAbsBitwiseCastArg(float a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argumentAsInt32 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* argumentAsfloat = root->appendNew<Value>(proc, BitwiseCast, Origin(), argumentAsInt32);
    Value* absValue = root->appendNew<Value>(proc, Abs, Origin(), argumentAsfloat);
    Value* resultAsInt64 = root->appendNew<Value>(proc, BitwiseCast, Origin(), absValue);

    root->appendNewControlValue(proc, Return, Origin(), resultAsInt64);

    int32_t expectedResult = bitwise_cast<int32_t>(static_cast<float>(fabs(a)));
    CHECK(isIdentical(compileAndRun<int32_t>(proc, bitwise_cast<int32_t>(a)), expectedResult));
}

void testAbsArgWithUselessDoubleConversion(float a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argument32 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* floatValue = root->appendNew<Value>(proc, BitwiseCast, Origin(), argument32);
    Value* asDouble = root->appendNew<Value>(proc, FloatToDouble, Origin(), floatValue);
    Value* result = root->appendNew<Value>(proc, Abs, Origin(), asDouble);
    Value* floatResult = root->appendNew<Value>(proc, DoubleToFloat, Origin(), result);
    Value* result32 = root->appendNew<Value>(proc, BitwiseCast, Origin(), floatResult);
    root->appendNewControlValue(proc, Return, Origin(), result32);

    CHECK(isIdentical(compileAndRun<int32_t>(proc, bitwise_cast<int32_t>(a)), bitwise_cast<int32_t>(static_cast<float>(fabs(a)))));
}

void testAbsArgWithEffectfulDoubleConversion(float a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argument32 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* floatValue = root->appendNew<Value>(proc, BitwiseCast, Origin(), argument32);
    Value* asDouble = root->appendNew<Value>(proc, FloatToDouble, Origin(), floatValue);
    Value* result = root->appendNew<Value>(proc, Abs, Origin(), asDouble);
    Value* floatResult = root->appendNew<Value>(proc, DoubleToFloat, Origin(), result);
    Value* result32 = root->appendNew<Value>(proc, BitwiseCast, Origin(), floatResult);
    Value* doubleAddress = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
    root->appendNew<MemoryValue>(proc, Store, Origin(), result, doubleAddress);
    root->appendNewControlValue(proc, Return, Origin(), result32);

    double effect = 0;
    int32_t resultValue = compileAndRun<int32_t>(proc, bitwise_cast<int32_t>(a), &effect);
    CHECK(isIdentical(resultValue, bitwise_cast<int32_t>(static_cast<float>(fabs(a)))));
    CHECK(isIdentical(effect, static_cast<double>(fabs(a))));
}

void testCeilArg(double a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, Ceil, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR0)));

    CHECK(isIdentical(compileAndRun<double>(proc, a), ceil(a)));
}

void testCeilImm(double a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argument = root->appendNew<ConstDoubleValue>(proc, Origin(), a);
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(proc, Ceil, Origin(), argument));

    CHECK(isIdentical(compileAndRun<double>(proc), ceil(a)));
}

void testCeilMem(double a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* address = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    MemoryValue* loadDouble = root->appendNew<MemoryValue>(proc, Load, Double, Origin(), address);
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(proc, Ceil, Origin(), loadDouble));

    CHECK(isIdentical(compileAndRun<double>(proc, &a), ceil(a)));
}

void testCeilCeilArg(double a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* firstCeil = root->appendNew<Value>(proc, Ceil, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR0));
    Value* secondCeil = root->appendNew<Value>(proc, Ceil, Origin(), firstCeil);
    root->appendNewControlValue(proc, Return, Origin(), secondCeil);

    CHECK(isIdentical(compileAndRun<double>(proc, a), ceil(a)));
}

void testFloorCeilArg(double a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* firstCeil = root->appendNew<Value>(proc, Ceil, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR0));
    Value* wrappingFloor = root->appendNew<Value>(proc, Floor, Origin(), firstCeil);
    root->appendNewControlValue(proc, Return, Origin(), wrappingFloor);

    CHECK(isIdentical(compileAndRun<double>(proc, a), ceil(a)));
}

void testCeilIToD64(int64_t a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argument = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    Value* argumentAsDouble = root->appendNew<Value>(proc, IToD, Origin(), argument);

    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(proc, Ceil, Origin(), argumentAsDouble));

    CHECK(isIdentical(compileAndRun<double>(proc, a), ceil(static_cast<double>(a))));
}

void testCeilIToD32(int64_t a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argument = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* argumentAsDouble = root->appendNew<Value>(proc, IToD, Origin(), argument);

    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(proc, Ceil, Origin(), argumentAsDouble));

    CHECK(isIdentical(compileAndRun<double>(proc, a), ceil(static_cast<double>(a))));
}

void testCeilArg(float a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argument32 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* argument = root->appendNew<Value>(proc, BitwiseCast, Origin(), argument32);
    Value* result = root->appendNew<Value>(proc, Ceil, Origin(), argument);
    Value* result32 = root->appendNew<Value>(proc, BitwiseCast, Origin(), result);
    root->appendNewControlValue(proc, Return, Origin(), result32);

    CHECK(isIdentical(compileAndRun<int32_t>(proc, bitwise_cast<int32_t>(a)), bitwise_cast<int32_t>(ceilf(a))));
}

void testCeilImm(float a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argument = root->appendNew<ConstFloatValue>(proc, Origin(), a);
    Value* result = root->appendNew<Value>(proc, Ceil, Origin(), argument);
    Value* result32 = root->appendNew<Value>(proc, BitwiseCast, Origin(), result);
    root->appendNewControlValue(proc, Return, Origin(), result32);

    CHECK(isIdentical(compileAndRun<int32_t>(proc, bitwise_cast<int32_t>(a)), bitwise_cast<int32_t>(ceilf(a))));
}

void testCeilMem(float a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* address = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    MemoryValue* loadFloat = root->appendNew<MemoryValue>(proc, Load, Float, Origin(), address);
    Value* result = root->appendNew<Value>(proc, Ceil, Origin(), loadFloat);
    Value* result32 = root->appendNew<Value>(proc, BitwiseCast, Origin(), result);
    root->appendNewControlValue(proc, Return, Origin(), result32);

    CHECK(isIdentical(compileAndRun<int32_t>(proc, &a), bitwise_cast<int32_t>(ceilf(a))));
}

void testCeilCeilArg(float a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argument32 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* argument = root->appendNew<Value>(proc, BitwiseCast, Origin(), argument32);
    Value* firstCeil = root->appendNew<Value>(proc, Ceil, Origin(), argument);
    Value* secondCeil = root->appendNew<Value>(proc, Ceil, Origin(), firstCeil);
    root->appendNewControlValue(proc, Return, Origin(), secondCeil);

    CHECK(isIdentical(compileAndRun<float>(proc, bitwise_cast<int32_t>(a)), ceilf(a)));
}

void testFloorCeilArg(float a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argument32 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* argument = root->appendNew<Value>(proc, BitwiseCast, Origin(), argument32);
    Value* firstCeil = root->appendNew<Value>(proc, Ceil, Origin(), argument);
    Value* wrappingFloor = root->appendNew<Value>(proc, Floor, Origin(), firstCeil);
    root->appendNewControlValue(proc, Return, Origin(), wrappingFloor);

    CHECK(isIdentical(compileAndRun<float>(proc, bitwise_cast<int32_t>(a)), ceilf(a)));
}

void testCeilArgWithUselessDoubleConversion(float a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argument32 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* floatValue = root->appendNew<Value>(proc, BitwiseCast, Origin(), argument32);
    Value* asDouble = root->appendNew<Value>(proc, FloatToDouble, Origin(), floatValue);
    Value* result = root->appendNew<Value>(proc, Ceil, Origin(), asDouble);
    Value* floatResult = root->appendNew<Value>(proc, DoubleToFloat, Origin(), result);
    Value* result32 = root->appendNew<Value>(proc, BitwiseCast, Origin(), floatResult);
    root->appendNewControlValue(proc, Return, Origin(), result32);

    CHECK(isIdentical(compileAndRun<int32_t>(proc, bitwise_cast<int32_t>(a)), bitwise_cast<int32_t>(ceilf(a))));
}

void testCeilArgWithEffectfulDoubleConversion(float a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argument32 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* floatValue = root->appendNew<Value>(proc, BitwiseCast, Origin(), argument32);
    Value* asDouble = root->appendNew<Value>(proc, FloatToDouble, Origin(), floatValue);
    Value* result = root->appendNew<Value>(proc, Ceil, Origin(), asDouble);
    Value* floatResult = root->appendNew<Value>(proc, DoubleToFloat, Origin(), result);
    Value* result32 = root->appendNew<Value>(proc, BitwiseCast, Origin(), floatResult);
    Value* doubleAddress = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
    root->appendNew<MemoryValue>(proc, Store, Origin(), result, doubleAddress);
    root->appendNewControlValue(proc, Return, Origin(), result32);

    double effect = 0;
    int32_t resultValue = compileAndRun<int32_t>(proc, bitwise_cast<int32_t>(a), &effect);
    CHECK(isIdentical(resultValue, bitwise_cast<int32_t>(ceilf(a))));
    CHECK(isIdentical(effect, static_cast<double>(ceilf(a))));
}

void testFloorArg(double a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, Floor, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR0)));

    CHECK(isIdentical(compileAndRun<double>(proc, a), floor(a)));
}

void testFloorImm(double a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argument = root->appendNew<ConstDoubleValue>(proc, Origin(), a);
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(proc, Floor, Origin(), argument));

    CHECK(isIdentical(compileAndRun<double>(proc), floor(a)));
}

void testFloorMem(double a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* address = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    MemoryValue* loadDouble = root->appendNew<MemoryValue>(proc, Load, Double, Origin(), address);
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(proc, Floor, Origin(), loadDouble));

    CHECK(isIdentical(compileAndRun<double>(proc, &a), floor(a)));
}

void testFloorFloorArg(double a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* firstFloor = root->appendNew<Value>(proc, Floor, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR0));
    Value* secondFloor = root->appendNew<Value>(proc, Floor, Origin(), firstFloor);
    root->appendNewControlValue(proc, Return, Origin(), secondFloor);

    CHECK(isIdentical(compileAndRun<double>(proc, a), floor(a)));
}

void testCeilFloorArg(double a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* firstFloor = root->appendNew<Value>(proc, Floor, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR0));
    Value* wrappingCeil = root->appendNew<Value>(proc, Ceil, Origin(), firstFloor);
    root->appendNewControlValue(proc, Return, Origin(), wrappingCeil);

    CHECK(isIdentical(compileAndRun<double>(proc, a), floor(a)));
}

void testFloorIToD64(int64_t a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argument = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    Value* argumentAsDouble = root->appendNew<Value>(proc, IToD, Origin(), argument);

    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(proc, Floor, Origin(), argumentAsDouble));

    CHECK(isIdentical(compileAndRun<double>(proc, a), floor(static_cast<double>(a))));
}

void testFloorIToD32(int64_t a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argument = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* argumentAsDouble = root->appendNew<Value>(proc, IToD, Origin(), argument);

    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(proc, Floor, Origin(), argumentAsDouble));

    CHECK(isIdentical(compileAndRun<double>(proc, a), floor(static_cast<double>(a))));
}

void testFloorArg(float a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argument32 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* argument = root->appendNew<Value>(proc, BitwiseCast, Origin(), argument32);
    Value* result = root->appendNew<Value>(proc, Floor, Origin(), argument);
    Value* result32 = root->appendNew<Value>(proc, BitwiseCast, Origin(), result);
    root->appendNewControlValue(proc, Return, Origin(), result32);

    CHECK(isIdentical(compileAndRun<int32_t>(proc, bitwise_cast<int32_t>(a)), bitwise_cast<int32_t>(floorf(a))));
}

void testFloorImm(float a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argument = root->appendNew<ConstFloatValue>(proc, Origin(), a);
    Value* result = root->appendNew<Value>(proc, Floor, Origin(), argument);
    Value* result32 = root->appendNew<Value>(proc, BitwiseCast, Origin(), result);
    root->appendNewControlValue(proc, Return, Origin(), result32);

    CHECK(isIdentical(compileAndRun<int32_t>(proc, bitwise_cast<int32_t>(a)), bitwise_cast<int32_t>(floorf(a))));
}

void testFloorMem(float a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* address = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    MemoryValue* loadFloat = root->appendNew<MemoryValue>(proc, Load, Float, Origin(), address);
    Value* result = root->appendNew<Value>(proc, Floor, Origin(), loadFloat);
    Value* result32 = root->appendNew<Value>(proc, BitwiseCast, Origin(), result);
    root->appendNewControlValue(proc, Return, Origin(), result32);

    CHECK(isIdentical(compileAndRun<int32_t>(proc, &a), bitwise_cast<int32_t>(floorf(a))));
}

void testFloorFloorArg(float a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argument32 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* argument = root->appendNew<Value>(proc, BitwiseCast, Origin(), argument32);
    Value* firstFloor = root->appendNew<Value>(proc, Floor, Origin(), argument);
    Value* secondFloor = root->appendNew<Value>(proc, Floor, Origin(), firstFloor);
    root->appendNewControlValue(proc, Return, Origin(), secondFloor);

    CHECK(isIdentical(compileAndRun<float>(proc, bitwise_cast<int32_t>(a)), floorf(a)));
}

void testCeilFloorArg(float a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argument32 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* argument = root->appendNew<Value>(proc, BitwiseCast, Origin(), argument32);
    Value* firstFloor = root->appendNew<Value>(proc, Floor, Origin(), argument);
    Value* wrappingCeil = root->appendNew<Value>(proc, Ceil, Origin(), firstFloor);
    root->appendNewControlValue(proc, Return, Origin(), wrappingCeil);

    CHECK(isIdentical(compileAndRun<float>(proc, bitwise_cast<int32_t>(a)), floorf(a)));
}

void testFloorArgWithUselessDoubleConversion(float a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argument32 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* floatValue = root->appendNew<Value>(proc, BitwiseCast, Origin(), argument32);
    Value* asDouble = root->appendNew<Value>(proc, FloatToDouble, Origin(), floatValue);
    Value* result = root->appendNew<Value>(proc, Floor, Origin(), asDouble);
    Value* floatResult = root->appendNew<Value>(proc, DoubleToFloat, Origin(), result);
    Value* result32 = root->appendNew<Value>(proc, BitwiseCast, Origin(), floatResult);
    root->appendNewControlValue(proc, Return, Origin(), result32);

    CHECK(isIdentical(compileAndRun<int32_t>(proc, bitwise_cast<int32_t>(a)), bitwise_cast<int32_t>(floorf(a))));
}

void testFloorArgWithEffectfulDoubleConversion(float a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argument32 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* floatValue = root->appendNew<Value>(proc, BitwiseCast, Origin(), argument32);
    Value* asDouble = root->appendNew<Value>(proc, FloatToDouble, Origin(), floatValue);
    Value* result = root->appendNew<Value>(proc, Floor, Origin(), asDouble);
    Value* floatResult = root->appendNew<Value>(proc, DoubleToFloat, Origin(), result);
    Value* result32 = root->appendNew<Value>(proc, BitwiseCast, Origin(), floatResult);
    Value* doubleAddress = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
    root->appendNew<MemoryValue>(proc, Store, Origin(), result, doubleAddress);
    root->appendNewControlValue(proc, Return, Origin(), result32);

    double effect = 0;
    int32_t resultValue = compileAndRun<int32_t>(proc, bitwise_cast<int32_t>(a), &effect);
    CHECK(isIdentical(resultValue, bitwise_cast<int32_t>(floorf(a))));
    CHECK(isIdentical(effect, static_cast<double>(floorf(a))));
}

double correctSqrt(double value)
{
#if CPU(X86) || CPU(X86_64)
    double result;
    asm ("sqrtsd %1, %0" : "=x"(result) : "x"(value));
    return result;
#else    
    return sqrt(value);
#endif
}

void testSqrtArg(double a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, Sqrt, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR0)));

    CHECK(isIdentical(compileAndRun<double>(proc, a), correctSqrt(a)));
}

void testSqrtImm(double a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argument = root->appendNew<ConstDoubleValue>(proc, Origin(), a);
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(proc, Sqrt, Origin(), argument));

    CHECK(isIdentical(compileAndRun<double>(proc), correctSqrt(a)));
}

void testSqrtMem(double a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* address = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    MemoryValue* loadDouble = root->appendNew<MemoryValue>(proc, Load, Double, Origin(), address);
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(proc, Sqrt, Origin(), loadDouble));

    CHECK(isIdentical(compileAndRun<double>(proc, &a), correctSqrt(a)));
}

void testSqrtArg(float a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argument32 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* argument = root->appendNew<Value>(proc, BitwiseCast, Origin(), argument32);
    Value* result = root->appendNew<Value>(proc, Sqrt, Origin(), argument);
    Value* result32 = root->appendNew<Value>(proc, BitwiseCast, Origin(), result);
    root->appendNewControlValue(proc, Return, Origin(), result32);

    CHECK(isIdentical(compileAndRun<int32_t>(proc, bitwise_cast<int32_t>(a)), bitwise_cast<int32_t>(static_cast<float>(correctSqrt(a)))));
}

void testSqrtImm(float a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argument = root->appendNew<ConstFloatValue>(proc, Origin(), a);
    Value* result = root->appendNew<Value>(proc, Sqrt, Origin(), argument);
    Value* result32 = root->appendNew<Value>(proc, BitwiseCast, Origin(), result);
    root->appendNewControlValue(proc, Return, Origin(), result32);

    CHECK(isIdentical(compileAndRun<int32_t>(proc, bitwise_cast<int32_t>(a)), bitwise_cast<int32_t>(static_cast<float>(correctSqrt(a)))));
}

void testSqrtMem(float a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* address = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    MemoryValue* loadFloat = root->appendNew<MemoryValue>(proc, Load, Float, Origin(), address);
    Value* result = root->appendNew<Value>(proc, Sqrt, Origin(), loadFloat);
    Value* result32 = root->appendNew<Value>(proc, BitwiseCast, Origin(), result);
    root->appendNewControlValue(proc, Return, Origin(), result32);

    CHECK(isIdentical(compileAndRun<int32_t>(proc, &a), bitwise_cast<int32_t>(static_cast<float>(correctSqrt(a)))));
}

void testSqrtArgWithUselessDoubleConversion(float a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argument32 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* floatValue = root->appendNew<Value>(proc, BitwiseCast, Origin(), argument32);
    Value* asDouble = root->appendNew<Value>(proc, FloatToDouble, Origin(), floatValue);
    Value* result = root->appendNew<Value>(proc, Sqrt, Origin(), asDouble);
    Value* floatResult = root->appendNew<Value>(proc, DoubleToFloat, Origin(), result);
    Value* result32 = root->appendNew<Value>(proc, BitwiseCast, Origin(), floatResult);
    root->appendNewControlValue(proc, Return, Origin(), result32);

    CHECK(isIdentical(compileAndRun<int32_t>(proc, bitwise_cast<int32_t>(a)), bitwise_cast<int32_t>(static_cast<float>(correctSqrt(a)))));
}

void testSqrtArgWithEffectfulDoubleConversion(float a)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argument32 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* floatValue = root->appendNew<Value>(proc, BitwiseCast, Origin(), argument32);
    Value* asDouble = root->appendNew<Value>(proc, FloatToDouble, Origin(), floatValue);
    Value* result = root->appendNew<Value>(proc, Sqrt, Origin(), asDouble);
    Value* floatResult = root->appendNew<Value>(proc, DoubleToFloat, Origin(), result);
    Value* result32 = root->appendNew<Value>(proc, BitwiseCast, Origin(), floatResult);
    Value* doubleAddress = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
    root->appendNew<MemoryValue>(proc, Store, Origin(), result, doubleAddress);
    root->appendNewControlValue(proc, Return, Origin(), result32);

    double effect = 0;
    int32_t resultValue = compileAndRun<int32_t>(proc, bitwise_cast<int32_t>(a), &effect);
    CHECK(isIdentical(resultValue, bitwise_cast<int32_t>(static_cast<float>(correctSqrt(a)))));
    double expected = static_cast<double>(correctSqrt(a));
    CHECK(isIdentical(effect, expected));
}

void testCompareTwoFloatToDouble(float a, float b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();

    Value* arg1As32 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* arg1Float = root->appendNew<Value>(proc, BitwiseCast, Origin(), arg1As32);
    Value* arg1AsDouble = root->appendNew<Value>(proc, FloatToDouble, Origin(), arg1Float);

    Value* arg2As32 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1));
    Value* arg2Float = root->appendNew<Value>(proc, BitwiseCast, Origin(), arg2As32);
    Value* arg2AsDouble = root->appendNew<Value>(proc, FloatToDouble, Origin(), arg2Float);
    Value* equal = root->appendNew<Value>(proc, Equal, Origin(), arg1AsDouble, arg2AsDouble);

    root->appendNewControlValue(proc, Return, Origin(), equal);

    CHECK(compileAndRun<int64_t>(proc, bitwise_cast<int32_t>(a), bitwise_cast<int32_t>(b)) == (a == b));
}

void testCompareOneFloatToDouble(float a, double b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();

    Value* arg1As32 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* arg1Float = root->appendNew<Value>(proc, BitwiseCast, Origin(), arg1As32);
    Value* arg1AsDouble = root->appendNew<Value>(proc, FloatToDouble, Origin(), arg1Float);

    Value* arg2AsDouble = root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR0);
    Value* equal = root->appendNew<Value>(proc, Equal, Origin(), arg1AsDouble, arg2AsDouble);

    root->appendNewControlValue(proc, Return, Origin(), equal);

    CHECK(compileAndRun<int64_t>(proc, bitwise_cast<int32_t>(a), b) == (a == b));
}

void testCompareFloatToDoubleThroughPhi(float a, float b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* thenCase = proc.addBlock();
    BasicBlock* elseCase = proc.addBlock();
    BasicBlock* tail = proc.addBlock();

    Value* condition = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);

    Value* arg1As32 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1));
    Value* arg1Float = root->appendNew<Value>(proc, BitwiseCast, Origin(), arg1As32);
    Value* arg1AsDouble = root->appendNew<Value>(proc, FloatToDouble, Origin(), arg1Float);

    Value* arg2AsDouble = root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR0);
    Value* arg2AsFloat = root->appendNew<Value>(proc, DoubleToFloat, Origin(), arg2AsDouble);
    Value* arg2AsFRoundedDouble = root->appendNew<Value>(proc, FloatToDouble, Origin(), arg2AsFloat);

    root->appendNewControlValue(
        proc, Branch, Origin(),
        condition,
        FrequentedBlock(thenCase), FrequentedBlock(elseCase));

    UpsilonValue* thenValue = thenCase->appendNew<UpsilonValue>(proc, Origin(), arg1AsDouble);
    thenCase->appendNewControlValue(proc, Jump, Origin(), FrequentedBlock(tail));

    Value* elseConst = elseCase->appendNew<ConstDoubleValue>(proc, Origin(), 0.);
    UpsilonValue* elseValue = elseCase->appendNew<UpsilonValue>(proc, Origin(), elseConst);
    elseCase->appendNewControlValue(proc, Jump, Origin(), FrequentedBlock(tail));

    Value* doubleInput = tail->appendNew<Value>(proc, Phi, Double, Origin());
    thenValue->setPhi(doubleInput);
    elseValue->setPhi(doubleInput);
    Value* equal = tail->appendNew<Value>(proc, Equal, Origin(), doubleInput, arg2AsFRoundedDouble);
    tail->appendNewControlValue(proc, Return, Origin(), equal);

    auto code = compileProc(proc);
    int32_t integerA = bitwise_cast<int32_t>(a);
    double doubleB = b;
    CHECK(invoke<int64_t>(*code, 1, integerA, doubleB) == (a == b));
    CHECK(invoke<int64_t>(*code, 0, integerA, doubleB) == (b == 0));
}

void testDoubleToFloatThroughPhi(float value)
{
    // Simple case of:
    //     if (a) {
    //         x = DoubleAdd(a, b)
    //     else
    //         x = DoubleAdd(a, c)
    //     DoubleToFloat(x)
    //
    // Both Adds can be converted to float add.
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* thenCase = proc.addBlock();
    BasicBlock* elseCase = proc.addBlock();
    BasicBlock* tail = proc.addBlock();

    Value* condition = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    Value* argument32 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1));
    Value* floatValue = root->appendNew<Value>(proc, BitwiseCast, Origin(), argument32);
    Value* argAsDouble = root->appendNew<Value>(proc, FloatToDouble, Origin(), floatValue);

    root->appendNewControlValue(
        proc, Branch, Origin(),
        condition,
        FrequentedBlock(thenCase), FrequentedBlock(elseCase));

    Value* postitiveConst = thenCase->appendNew<ConstDoubleValue>(proc, Origin(), 42.5f);
    Value* thenAdd = thenCase->appendNew<Value>(proc, Add, Origin(), argAsDouble, postitiveConst);
    UpsilonValue* thenValue = thenCase->appendNew<UpsilonValue>(proc, Origin(), thenAdd);
    thenCase->appendNewControlValue(proc, Jump, Origin(), FrequentedBlock(tail));

    Value* elseConst = elseCase->appendNew<ConstDoubleValue>(proc, Origin(), M_PI);
    UpsilonValue* elseValue = elseCase->appendNew<UpsilonValue>(proc, Origin(), elseConst);
    elseCase->appendNewControlValue(proc, Jump, Origin(), FrequentedBlock(tail));

    Value* doubleInput = tail->appendNew<Value>(proc, Phi, Double, Origin());
    thenValue->setPhi(doubleInput);
    elseValue->setPhi(doubleInput);
    Value* floatResult = tail->appendNew<Value>(proc, DoubleToFloat, Origin(), doubleInput);
    tail->appendNewControlValue(proc, Return, Origin(), floatResult);

    auto code = compileProc(proc);
    CHECK(isIdentical(invoke<float>(*code, 1, bitwise_cast<int32_t>(value)), value + 42.5f));
    CHECK(isIdentical(invoke<float>(*code, 0, bitwise_cast<int32_t>(value)), static_cast<float>(M_PI)));
}

void testReduceFloatToDoubleValidates()
{
    // Simple case of:
    //     f = DoubleToFloat(Bitcast(argGPR0))
    //     if (a) {
    //         x = FloatConst()
    //     else
    //         x = FloatConst()
    //     p = Phi(x)
    //     a = Mul(p, p)
    //     b = Add(a, f)
    //     c = Add(p, b)
    //     Return(c)
    //
    // This should not crash in the validator after ReduceFloatToDouble.
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* thenCase = proc.addBlock();
    BasicBlock* elseCase = proc.addBlock();
    BasicBlock* tail = proc.addBlock();

    Value* condition = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    Value* thingy = root->appendNew<Value>(proc, BitwiseCast, Origin(), condition);
    thingy = root->appendNew<Value>(proc, DoubleToFloat, Origin(), thingy); // Make the phase think it has work to do.
    root->appendNewControlValue(
        proc, Branch, Origin(),
        condition,
        FrequentedBlock(thenCase), FrequentedBlock(elseCase));

    UpsilonValue* thenValue = thenCase->appendNew<UpsilonValue>(proc, Origin(),
        thenCase->appendNew<ConstFloatValue>(proc, Origin(), 11.5));
    thenCase->appendNewControlValue(proc, Jump, Origin(), FrequentedBlock(tail));

    UpsilonValue* elseValue = elseCase->appendNew<UpsilonValue>(proc, Origin(), 
        elseCase->appendNew<ConstFloatValue>(proc, Origin(), 10.5));
    elseCase->appendNewControlValue(proc, Jump, Origin(), FrequentedBlock(tail));

    Value* phi =  tail->appendNew<Value>(proc, Phi, Float, Origin());
    thenValue->setPhi(phi);
    elseValue->setPhi(phi);
    Value* result = tail->appendNew<Value>(proc, Mul, Origin(), 
            phi, phi);
    result = tail->appendNew<Value>(proc, Add, Origin(), 
            result,
            thingy);
    result = tail->appendNew<Value>(proc, Add, Origin(), 
            phi,
            result);
    tail->appendNewControlValue(proc, Return, Origin(), result);

    auto code = compileProc(proc);
    CHECK(isIdentical(invoke<float>(*code, 1), 11.5f * 11.5f + static_cast<float>(bitwise_cast<double>(static_cast<uint64_t>(1))) + 11.5f));
    CHECK(isIdentical(invoke<float>(*code, 0), 10.5f * 10.5f + static_cast<float>(bitwise_cast<double>(static_cast<uint64_t>(0))) + 10.5f));
}

void testDoubleProducerPhiToFloatConversion(float value)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* thenCase = proc.addBlock();
    BasicBlock* elseCase = proc.addBlock();
    BasicBlock* tail = proc.addBlock();

    Value* condition = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    Value* argument32 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1));
    Value* floatValue = root->appendNew<Value>(proc, BitwiseCast, Origin(), argument32);

    root->appendNewControlValue(
        proc, Branch, Origin(),
        condition,
        FrequentedBlock(thenCase), FrequentedBlock(elseCase));

    Value* asDouble = thenCase->appendNew<Value>(proc, FloatToDouble, Origin(), floatValue);
    UpsilonValue* thenValue = thenCase->appendNew<UpsilonValue>(proc, Origin(), asDouble);
    thenCase->appendNewControlValue(proc, Jump, Origin(), FrequentedBlock(tail));

    Value* constDouble = elseCase->appendNew<ConstDoubleValue>(proc, Origin(), 42.5);
    UpsilonValue* elseValue = elseCase->appendNew<UpsilonValue>(proc, Origin(), constDouble);
    elseCase->appendNewControlValue(proc, Jump, Origin(), FrequentedBlock(tail));

    Value* doubleInput = tail->appendNew<Value>(proc, Phi, Double, Origin());
    thenValue->setPhi(doubleInput);
    elseValue->setPhi(doubleInput);

    Value* argAsDoubleAgain = tail->appendNew<Value>(proc, FloatToDouble, Origin(), floatValue);
    Value* finalAdd = tail->appendNew<Value>(proc, Add, Origin(), doubleInput, argAsDoubleAgain);
    Value* floatResult = tail->appendNew<Value>(proc, DoubleToFloat, Origin(), finalAdd);
    tail->appendNewControlValue(proc, Return, Origin(), floatResult);

    auto code = compileProc(proc);
    CHECK(isIdentical(invoke<float>(*code, 1, bitwise_cast<int32_t>(value)), value + value));
    CHECK(isIdentical(invoke<float>(*code, 0, bitwise_cast<int32_t>(value)), 42.5f + value));
}

void testDoubleProducerPhiToFloatConversionWithDoubleConsumer(float value)
{
    // In this case, the Upsilon-Phi effectively contains a Float value, but it is used
    // as a Float and as a Double.
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* thenCase = proc.addBlock();
    BasicBlock* elseCase = proc.addBlock();
    BasicBlock* tail = proc.addBlock();

    Value* condition = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    Value* argument32 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1));
    Value* floatValue = root->appendNew<Value>(proc, BitwiseCast, Origin(), argument32);

    root->appendNewControlValue(
        proc, Branch, Origin(),
        condition,
        FrequentedBlock(thenCase), FrequentedBlock(elseCase));

    Value* asDouble = thenCase->appendNew<Value>(proc, FloatToDouble, Origin(), floatValue);
    UpsilonValue* thenValue = thenCase->appendNew<UpsilonValue>(proc, Origin(), asDouble);
    thenCase->appendNewControlValue(proc, Jump, Origin(), FrequentedBlock(tail));

    Value* constDouble = elseCase->appendNew<ConstDoubleValue>(proc, Origin(), 42.5);
    UpsilonValue* elseValue = elseCase->appendNew<UpsilonValue>(proc, Origin(), constDouble);
    elseCase->appendNewControlValue(proc, Jump, Origin(), FrequentedBlock(tail));

    Value* doubleInput = tail->appendNew<Value>(proc, Phi, Double, Origin());
    thenValue->setPhi(doubleInput);
    elseValue->setPhi(doubleInput);

    Value* argAsDoubleAgain = tail->appendNew<Value>(proc, FloatToDouble, Origin(), floatValue);
    Value* floatAdd = tail->appendNew<Value>(proc, Add, Origin(), doubleInput, argAsDoubleAgain);

    // FRound.
    Value* floatResult = tail->appendNew<Value>(proc, DoubleToFloat, Origin(), floatAdd);
    Value* doubleResult = tail->appendNew<Value>(proc, FloatToDouble, Origin(), floatResult);

    // This one *cannot* be eliminated
    Value* doubleAdd = tail->appendNew<Value>(proc, Add, Origin(), doubleInput, doubleResult);

    tail->appendNewControlValue(proc, Return, Origin(), doubleAdd);

    auto code = compileProc(proc);
    CHECK(isIdentical(invoke<double>(*code, 1, bitwise_cast<int32_t>(value)), (value + value) + static_cast<double>(value)));
    CHECK(isIdentical(invoke<double>(*code, 0, bitwise_cast<int32_t>(value)), static_cast<double>((42.5f + value) + 42.5f)));
}

void testDoubleProducerPhiWithNonFloatConst(float value, double constValue)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* thenCase = proc.addBlock();
    BasicBlock* elseCase = proc.addBlock();
    BasicBlock* tail = proc.addBlock();

    Value* condition = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    Value* argument32 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1));
    Value* floatValue = root->appendNew<Value>(proc, BitwiseCast, Origin(), argument32);

    root->appendNewControlValue(
        proc, Branch, Origin(),
        condition,
        FrequentedBlock(thenCase), FrequentedBlock(elseCase));

    Value* asDouble = thenCase->appendNew<Value>(proc, FloatToDouble, Origin(), floatValue);
    UpsilonValue* thenValue = thenCase->appendNew<UpsilonValue>(proc, Origin(), asDouble);
    thenCase->appendNewControlValue(proc, Jump, Origin(), FrequentedBlock(tail));

    Value* constDouble = elseCase->appendNew<ConstDoubleValue>(proc, Origin(), constValue);
    UpsilonValue* elseValue = elseCase->appendNew<UpsilonValue>(proc, Origin(), constDouble);
    elseCase->appendNewControlValue(proc, Jump, Origin(), FrequentedBlock(tail));

    Value* doubleInput = tail->appendNew<Value>(proc, Phi, Double, Origin());
    thenValue->setPhi(doubleInput);
    elseValue->setPhi(doubleInput);

    Value* argAsDoubleAgain = tail->appendNew<Value>(proc, FloatToDouble, Origin(), floatValue);
    Value* finalAdd = tail->appendNew<Value>(proc, Add, Origin(), doubleInput, argAsDoubleAgain);
    Value* floatResult = tail->appendNew<Value>(proc, DoubleToFloat, Origin(), finalAdd);
    tail->appendNewControlValue(proc, Return, Origin(), floatResult);

    auto code = compileProc(proc);
    CHECK(isIdentical(invoke<float>(*code, 1, bitwise_cast<int32_t>(value)), value + value));
    CHECK(isIdentical(invoke<float>(*code, 0, bitwise_cast<int32_t>(value)), static_cast<float>(constValue + value)));
}

void testDoubleArgToInt64BitwiseCast(double value)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argument = root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR0);

    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, BitwiseCast, Origin(), argument));

    CHECK(isIdentical(compileAndRun<int64_t>(proc, value), bitwise_cast<int64_t>(value)));
}

void testDoubleImmToInt64BitwiseCast(double value)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argument = root->appendNew<ConstDoubleValue>(proc, Origin(), value);

    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, BitwiseCast, Origin(), argument));

    CHECK(isIdentical(compileAndRun<int64_t>(proc), bitwise_cast<int64_t>(value)));
}

void testTwoBitwiseCastOnDouble(double value)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argument = root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR0);
    Value* first = root->appendNew<Value>(proc, BitwiseCast, Origin(), argument);
    Value* second = root->appendNew<Value>(proc, BitwiseCast, Origin(), first);
    root->appendNewControlValue(proc, Return, Origin(), second);

    CHECK(isIdentical(compileAndRun<double>(proc, value), value));
}

void testBitwiseCastOnDoubleInMemory(double value)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* address = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    MemoryValue* loadDouble = root->appendNew<MemoryValue>(proc, Load, Double, Origin(), address);
    Value* cast = root->appendNew<Value>(proc, BitwiseCast, Origin(), loadDouble);
    root->appendNewControlValue(proc, Return, Origin(), cast);

    CHECK(isIdentical(compileAndRun<int64_t>(proc, &value), bitwise_cast<int64_t>(value)));
}

void testBitwiseCastOnDoubleInMemoryIndexed(double value)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* base = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    Value* offset = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
    Value* scaledOffset = root->appendNew<Value>(proc, Shl, Origin(),
        offset,
        root->appendNew<Const32Value>(proc, Origin(), 3));
    Value* address = root->appendNew<Value>(proc, Add, Origin(), base, scaledOffset);
    MemoryValue* loadDouble = root->appendNew<MemoryValue>(proc, Load, Double, Origin(), address);
    Value* cast = root->appendNew<Value>(proc, BitwiseCast, Origin(), loadDouble);
    root->appendNewControlValue(proc, Return, Origin(), cast);

    CHECK(isIdentical(compileAndRun<int64_t>(proc, &value, 0), bitwise_cast<int64_t>(value)));
}

void testInt64BArgToDoubleBitwiseCast(int64_t value)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argument = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);

    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, BitwiseCast, Origin(), argument));

    CHECK(isIdentical(compileAndRun<double>(proc, value), bitwise_cast<double>(value)));
}

void testInt64BImmToDoubleBitwiseCast(int64_t value)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argument = root->appendNew<Const64Value>(proc, Origin(), value);

    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, BitwiseCast, Origin(), argument));

    CHECK(isIdentical(compileAndRun<double>(proc), bitwise_cast<double>(value)));
}

void testTwoBitwiseCastOnInt64(int64_t value)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argument = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    Value* first = root->appendNew<Value>(proc, BitwiseCast, Origin(), argument);
    Value* second = root->appendNew<Value>(proc, BitwiseCast, Origin(), first);
    root->appendNewControlValue(proc, Return, Origin(), second);

    CHECK(isIdentical(compileAndRun<int64_t>(proc, value), value));
}

void testBitwiseCastOnInt64InMemory(int64_t value)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* address = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    MemoryValue* loadDouble = root->appendNew<MemoryValue>(proc, Load, Int64, Origin(), address);
    Value* cast = root->appendNew<Value>(proc, BitwiseCast, Origin(), loadDouble);
    root->appendNewControlValue(proc, Return, Origin(), cast);

    CHECK(isIdentical(compileAndRun<double>(proc, &value), bitwise_cast<double>(value)));
}

void testBitwiseCastOnInt64InMemoryIndexed(int64_t value)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* base = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    Value* offset = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
    Value* scaledOffset = root->appendNew<Value>(proc, Shl, Origin(),
        offset,
        root->appendNew<Const32Value>(proc, Origin(), 3));
    Value* address = root->appendNew<Value>(proc, Add, Origin(), base, scaledOffset);
    MemoryValue* loadDouble = root->appendNew<MemoryValue>(proc, Load, Int64, Origin(), address);
    Value* cast = root->appendNew<Value>(proc, BitwiseCast, Origin(), loadDouble);
    root->appendNewControlValue(proc, Return, Origin(), cast);

    CHECK(isIdentical(compileAndRun<double>(proc, &value, 0), bitwise_cast<double>(value)));
}

void testFloatImmToInt32BitwiseCast(float value)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argument = root->appendNew<ConstFloatValue>(proc, Origin(), value);

    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, BitwiseCast, Origin(), argument));

    CHECK(isIdentical(compileAndRun<int32_t>(proc), bitwise_cast<int32_t>(value)));
}

void testBitwiseCastOnFloatInMemory(float value)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* address = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    MemoryValue* loadFloat = root->appendNew<MemoryValue>(proc, Load, Float, Origin(), address);
    Value* cast = root->appendNew<Value>(proc, BitwiseCast, Origin(), loadFloat);
    root->appendNewControlValue(proc, Return, Origin(), cast);

    CHECK(isIdentical(compileAndRun<int32_t>(proc, &value), bitwise_cast<int32_t>(value)));
}

void testInt32BArgToFloatBitwiseCast(int32_t value)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argument = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);

    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, BitwiseCast, Origin(), argument));

    CHECK(isIdentical(compileAndRun<float>(proc, value), bitwise_cast<float>(value)));
}

void testInt32BImmToFloatBitwiseCast(int32_t value)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argument = root->appendNew<Const64Value>(proc, Origin(), value);

    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, BitwiseCast, Origin(), argument));

    CHECK(isIdentical(compileAndRun<float>(proc), bitwise_cast<float>(value)));
}

void testTwoBitwiseCastOnInt32(int32_t value)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argument = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    Value* first = root->appendNew<Value>(proc, BitwiseCast, Origin(), argument);
    Value* second = root->appendNew<Value>(proc, BitwiseCast, Origin(), first);
    root->appendNewControlValue(proc, Return, Origin(), second);

    CHECK(isIdentical(compileAndRun<int32_t>(proc, value), value));
}

void testBitwiseCastOnInt32InMemory(int32_t value)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* address = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    MemoryValue* loadFloat = root->appendNew<MemoryValue>(proc, Load, Int32, Origin(), address);
    Value* cast = root->appendNew<Value>(proc, BitwiseCast, Origin(), loadFloat);
    root->appendNewControlValue(proc, Return, Origin(), cast);

    CHECK(isIdentical(compileAndRun<float>(proc, &value), bitwise_cast<float>(value)));
}

void testConvertDoubleToFloatArg(double value)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argument = root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR0);
    Value* asFloat = root->appendNew<Value>(proc, DoubleToFloat, Origin(), argument);
    root->appendNewControlValue(proc, Return, Origin(), asFloat);

    CHECK(isIdentical(compileAndRun<float>(proc, value), static_cast<float>(value)));
}

void testConvertDoubleToFloatImm(double value)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argument = root->appendNew<ConstDoubleValue>(proc, Origin(), value);
    Value* asFloat = root->appendNew<Value>(proc, DoubleToFloat, Origin(), argument);
    root->appendNewControlValue(proc, Return, Origin(), asFloat);

    CHECK(isIdentical(compileAndRun<float>(proc), static_cast<float>(value)));
}

void testConvertDoubleToFloatMem(double value)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* address = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    MemoryValue* loadedDouble = root->appendNew<MemoryValue>(proc, Load, Double, Origin(), address);
    Value* asFloat = root->appendNew<Value>(proc, DoubleToFloat, Origin(), loadedDouble);
    root->appendNewControlValue(proc, Return, Origin(), asFloat);

    CHECK(isIdentical(compileAndRun<float>(proc, &value), static_cast<float>(value)));
}

void testConvertFloatToDoubleArg(float value)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argument32 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* floatValue = root->appendNew<Value>(proc, BitwiseCast, Origin(), argument32);
    Value* asDouble = root->appendNew<Value>(proc, FloatToDouble, Origin(), floatValue);
    root->appendNewControlValue(proc, Return, Origin(), asDouble);

    CHECK(isIdentical(compileAndRun<double>(proc, bitwise_cast<int32_t>(value)), static_cast<double>(value)));
}

void testConvertFloatToDoubleImm(float value)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argument = root->appendNew<ConstFloatValue>(proc, Origin(), value);
    Value* asDouble = root->appendNew<Value>(proc, FloatToDouble, Origin(), argument);
    root->appendNewControlValue(proc, Return, Origin(), asDouble);

    CHECK(isIdentical(compileAndRun<double>(proc), static_cast<double>(value)));
}

void testConvertFloatToDoubleMem(float value)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* address = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    MemoryValue* loadedFloat = root->appendNew<MemoryValue>(proc, Load, Float, Origin(), address);
    Value* asDouble = root->appendNew<Value>(proc, FloatToDouble, Origin(), loadedFloat);
    root->appendNewControlValue(proc, Return, Origin(), asDouble);

    CHECK(isIdentical(compileAndRun<double>(proc, &value), static_cast<double>(value)));
}

void testConvertDoubleToFloatToDoubleToFloat(double value)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argument = root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR0);
    Value* asFloat = root->appendNew<Value>(proc, DoubleToFloat, Origin(), argument);
    Value* asDouble = root->appendNew<Value>(proc, FloatToDouble, Origin(), asFloat);
    Value* asFloatAgain = root->appendNew<Value>(proc, DoubleToFloat, Origin(), asDouble);
    root->appendNewControlValue(proc, Return, Origin(), asFloatAgain);

    CHECK(isIdentical(compileAndRun<float>(proc, value), static_cast<float>(value)));
}

void testLoadFloatConvertDoubleConvertFloatStoreFloat(float value)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* src = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    Value* dst = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
    MemoryValue* loadedFloat = root->appendNew<MemoryValue>(proc, Load, Float, Origin(), src);
    Value* asDouble = root->appendNew<Value>(proc, FloatToDouble, Origin(), loadedFloat);
    Value* asFloatAgain = root->appendNew<Value>(proc, DoubleToFloat, Origin(), asDouble);
    root->appendNew<MemoryValue>(proc, Store, Origin(), asFloatAgain, dst);

    root->appendNewControlValue(proc, Return, Origin(), root->appendNew<Const32Value>(proc, Origin(), 0));

    float input = value;
    float output = 0.;
    CHECK(!compileAndRun<int64_t>(proc, &input, &output));
    CHECK(isIdentical(input, output));
}

void testFroundArg(double value)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argument = root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR0);
    Value* asFloat = root->appendNew<Value>(proc, DoubleToFloat, Origin(), argument);
    Value* asDouble = root->appendNew<Value>(proc, FloatToDouble, Origin(), asFloat);
    root->appendNewControlValue(proc, Return, Origin(), asDouble);

    CHECK(isIdentical(compileAndRun<double>(proc, value), static_cast<double>(static_cast<float>(value))));
}

void testFroundMem(double value)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* address = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    MemoryValue* loadedDouble = root->appendNew<MemoryValue>(proc, Load, Double, Origin(), address);
    Value* asFloat = root->appendNew<Value>(proc, DoubleToFloat, Origin(), loadedDouble);
    Value* asDouble = root->appendNew<Value>(proc, FloatToDouble, Origin(), asFloat);
    root->appendNewControlValue(proc, Return, Origin(), asDouble);

    CHECK(isIdentical(compileAndRun<double>(proc, &value), static_cast<double>(static_cast<float>(value))));
}

void testIToD64Arg()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* src = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    Value* srcAsDouble = root->appendNew<Value>(proc, IToD, Origin(), src);
    root->appendNewControlValue(proc, Return, Origin(), srcAsDouble);

    auto code = compileProc(proc);
    for (auto testValue : int64Operands())
        CHECK(isIdentical(invoke<double>(*code, testValue.value), static_cast<double>(testValue.value)));
}

void testIToF64Arg()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* src = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    Value* srcAsFloat = root->appendNew<Value>(proc, IToF, Origin(), src);
    root->appendNewControlValue(proc, Return, Origin(), srcAsFloat);

    auto code = compileProc(proc);
    for (auto testValue : int64Operands())
        CHECK(isIdentical(invoke<float>(*code, testValue.value), static_cast<float>(testValue.value)));
}

void testIToD32Arg()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* src = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* srcAsDouble = root->appendNew<Value>(proc, IToD, Origin(), src);
    root->appendNewControlValue(proc, Return, Origin(), srcAsDouble);

    auto code = compileProc(proc);
    for (auto testValue : int32Operands())
        CHECK(isIdentical(invoke<double>(*code, testValue.value), static_cast<double>(testValue.value)));
}

void testIToF32Arg()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* src = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* srcAsFloat = root->appendNew<Value>(proc, IToF, Origin(), src);
    root->appendNewControlValue(proc, Return, Origin(), srcAsFloat);

    auto code = compileProc(proc);
    for (auto testValue : int32Operands())
        CHECK(isIdentical(invoke<float>(*code, testValue.value), static_cast<float>(testValue.value)));
}

void testIToD64Mem()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* address = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    MemoryValue* loadedSrc = root->appendNew<MemoryValue>(proc, Load, Int64, Origin(), address);
    Value* srcAsDouble = root->appendNew<Value>(proc, IToD, Origin(), loadedSrc);
    root->appendNewControlValue(proc, Return, Origin(), srcAsDouble);

    auto code = compileProc(proc);
    int64_t inMemoryValue;
    for (auto testValue : int64Operands()) {
        inMemoryValue = testValue.value;
        CHECK(isIdentical(invoke<double>(*code, &inMemoryValue), static_cast<double>(testValue.value)));
        CHECK(inMemoryValue == testValue.value);
    }
}

void testIToF64Mem()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* address = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    MemoryValue* loadedSrc = root->appendNew<MemoryValue>(proc, Load, Int64, Origin(), address);
    Value* srcAsFloat = root->appendNew<Value>(proc, IToF, Origin(), loadedSrc);
    root->appendNewControlValue(proc, Return, Origin(), srcAsFloat);

    auto code = compileProc(proc);
    int64_t inMemoryValue;
    for (auto testValue : int64Operands()) {
        inMemoryValue = testValue.value;
        CHECK(isIdentical(invoke<float>(*code, &inMemoryValue), static_cast<float>(testValue.value)));
        CHECK(inMemoryValue == testValue.value);
    }
}

void testIToD32Mem()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* address = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    MemoryValue* loadedSrc = root->appendNew<MemoryValue>(proc, Load, Int32, Origin(), address);
    Value* srcAsDouble = root->appendNew<Value>(proc, IToD, Origin(), loadedSrc);
    root->appendNewControlValue(proc, Return, Origin(), srcAsDouble);

    auto code = compileProc(proc);
    int32_t inMemoryValue;
    for (auto testValue : int32Operands()) {
        inMemoryValue = testValue.value;
        CHECK(isIdentical(invoke<double>(*code, &inMemoryValue), static_cast<double>(testValue.value)));
        CHECK(inMemoryValue == testValue.value);
    }
}

void testIToF32Mem()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* address = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    MemoryValue* loadedSrc = root->appendNew<MemoryValue>(proc, Load, Int32, Origin(), address);
    Value* srcAsFloat = root->appendNew<Value>(proc, IToF, Origin(), loadedSrc);
    root->appendNewControlValue(proc, Return, Origin(), srcAsFloat);

    auto code = compileProc(proc);
    int32_t inMemoryValue;
    for (auto testValue : int32Operands()) {
        inMemoryValue = testValue.value;
        CHECK(isIdentical(invoke<float>(*code, &inMemoryValue), static_cast<float>(testValue.value)));
        CHECK(inMemoryValue == testValue.value);
    }
}

void testIToD64Imm(int64_t value)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* src = root->appendNew<Const64Value>(proc, Origin(), value);
    Value* srcAsFloatingPoint = root->appendNew<Value>(proc, IToD, Origin(), src);
    root->appendNewControlValue(proc, Return, Origin(), srcAsFloatingPoint);
    CHECK(isIdentical(compileAndRun<double>(proc), static_cast<double>(value)));
}

void testIToF64Imm(int64_t value)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* src = root->appendNew<Const64Value>(proc, Origin(), value);
    Value* srcAsFloatingPoint = root->appendNew<Value>(proc, IToF, Origin(), src);
    root->appendNewControlValue(proc, Return, Origin(), srcAsFloatingPoint);
    CHECK(isIdentical(compileAndRun<float>(proc), static_cast<float>(value)));
}

void testIToD32Imm(int32_t value)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* src = root->appendNew<Const32Value>(proc, Origin(), value);
    Value* srcAsFloatingPoint = root->appendNew<Value>(proc, IToD, Origin(), src);
    root->appendNewControlValue(proc, Return, Origin(), srcAsFloatingPoint);
    CHECK(isIdentical(compileAndRun<double>(proc), static_cast<double>(value)));
}

void testIToF32Imm(int32_t value)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* src = root->appendNew<Const32Value>(proc, Origin(), value);
    Value* srcAsFloatingPoint = root->appendNew<Value>(proc, IToF, Origin(), src);
    root->appendNewControlValue(proc, Return, Origin(), srcAsFloatingPoint);
    CHECK(isIdentical(compileAndRun<float>(proc), static_cast<float>(value)));
}

void testIToDReducedToIToF64Arg()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* src = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    Value* srcAsDouble = root->appendNew<Value>(proc, IToD, Origin(), src);
    Value* floatResult = root->appendNew<Value>(proc, DoubleToFloat, Origin(), srcAsDouble);
    root->appendNewControlValue(proc, Return, Origin(), floatResult);

    auto code = compileProc(proc);
    for (auto testValue : int64Operands())
        CHECK(isIdentical(invoke<float>(*code, testValue.value), static_cast<float>(testValue.value)));
}

void testIToDReducedToIToF32Arg()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* src = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* srcAsDouble = root->appendNew<Value>(proc, IToD, Origin(), src);
    Value* floatResult = root->appendNew<Value>(proc, DoubleToFloat, Origin(), srcAsDouble);
    root->appendNewControlValue(proc, Return, Origin(), floatResult);

    auto code = compileProc(proc);
    for (auto testValue : int32Operands())
        CHECK(isIdentical(invoke<float>(*code, testValue.value), static_cast<float>(testValue.value)));
}

void testStore32(int value)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    int slot = 0xbaadbeef;
    root->appendNew<MemoryValue>(
        proc, Store, Origin(),
        root->appendNew<Value>(
            proc, Trunc, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)),
        root->appendNew<ConstPtrValue>(proc, Origin(), &slot), 0);
    root->appendNewControlValue(
        proc, Return, Origin(), root->appendNew<Const32Value>(proc, Origin(), 0));

    CHECK(!compileAndRun<int>(proc, value));
    CHECK(slot == value);
}

void testStoreConstant(int value)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    int slot = 0xbaadbeef;
    root->appendNew<MemoryValue>(
        proc, Store, Origin(),
        root->appendNew<Const32Value>(proc, Origin(), value),
        root->appendNew<ConstPtrValue>(proc, Origin(), &slot), 0);
    root->appendNewControlValue(
        proc, Return, Origin(), root->appendNew<Const32Value>(proc, Origin(), 0));

    CHECK(!compileAndRun<int>(proc));
    CHECK(slot == value);
}

void testStoreConstantPtr(intptr_t value)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    intptr_t slot;
#if CPU(ADDRESS64)
    slot = (static_cast<intptr_t>(0xbaadbeef) << 32) + static_cast<intptr_t>(0xbaadbeef);
#else
    slot = 0xbaadbeef;
#endif
    root->appendNew<MemoryValue>(
        proc, Store, Origin(),
        root->appendNew<ConstPtrValue>(proc, Origin(), value),
        root->appendNew<ConstPtrValue>(proc, Origin(), &slot), 0);
    root->appendNewControlValue(
        proc, Return, Origin(), root->appendNew<Const32Value>(proc, Origin(), 0));

    CHECK(!compileAndRun<int>(proc));
    CHECK(slot == value);
}

void testStore8Arg()
{
    { // Direct addressing.
        Procedure proc;
        BasicBlock* root = proc.addBlock();

        Value* value = root->appendNew<Value>(proc, Trunc, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
        Value* address = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);

        root->appendNew<MemoryValue>(proc, Store8, Origin(), value, address);
        root->appendNewControlValue(proc, Return, Origin(), value);

        int8_t storage = 0;
        CHECK(compileAndRun<int64_t>(proc, 42, &storage) == 42);
        CHECK(storage == 42);
    }

    { // Indexed addressing.
        Procedure proc;
        BasicBlock* root = proc.addBlock();

        Value* value = root->appendNew<Value>(proc, Trunc, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
        Value* base = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
        Value* offset = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR2);
        Value* displacement = root->appendNew<Const64Value>(proc, Origin(), -1);

        Value* baseDisplacement = root->appendNew<Value>(proc, Add, Origin(), displacement, base);
        Value* address = root->appendNew<Value>(proc, Add, Origin(), baseDisplacement, offset);

        root->appendNew<MemoryValue>(proc, Store8, Origin(), value, address);
        root->appendNewControlValue(proc, Return, Origin(), value);

        int8_t storage = 0;
        CHECK(compileAndRun<int64_t>(proc, 42, &storage, 1) == 42);
        CHECK(storage == 42);
    }
}

void testStore8Imm()
{
    { // Direct addressing.
        Procedure proc;
        BasicBlock* root = proc.addBlock();

        Value* value = root->appendNew<Const32Value>(proc, Origin(), 42);
        Value* address = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);

        root->appendNew<MemoryValue>(proc, Store8, Origin(), value, address);
        root->appendNewControlValue(proc, Return, Origin(), value);

        int8_t storage = 0;
        CHECK(compileAndRun<int64_t>(proc, &storage) == 42);
        CHECK(storage == 42);
    }

    { // Indexed addressing.
        Procedure proc;
        BasicBlock* root = proc.addBlock();

        Value* value = root->appendNew<Const32Value>(proc, Origin(), 42);
        Value* base = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
        Value* offset = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
        Value* displacement = root->appendNew<Const64Value>(proc, Origin(), -1);

        Value* baseDisplacement = root->appendNew<Value>(proc, Add, Origin(), displacement, base);
        Value* address = root->appendNew<Value>(proc, Add, Origin(), baseDisplacement, offset);

        root->appendNew<MemoryValue>(proc, Store8, Origin(), value, address);
        root->appendNewControlValue(proc, Return, Origin(), value);

        int8_t storage = 0;
        CHECK(compileAndRun<int64_t>(proc, &storage, 1) == 42);
        CHECK(storage == 42);
    }
}

void testStorePartial8BitRegisterOnX86()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();

    // We want to have this in ECX.
    Value* returnValue = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);

    // We want this suck in EDX.
    Value* whereToStore = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);

    // The patch point is there to help us force the hand of the compiler.
    PatchpointValue* patchpoint = root->appendNew<PatchpointValue>(proc, Int32, Origin());

    // For the value above to be materialized and give the allocator
    // a stronger insentive to name those register the way we need.
    patchpoint->append(ConstrainedValue(returnValue, ValueRep(GPRInfo::regT3)));
    patchpoint->append(ConstrainedValue(whereToStore, ValueRep(GPRInfo::regT2)));

    // We'll produce EDI.
    patchpoint->resultConstraints = { ValueRep::reg(GPRInfo::regT6) };

    // Give the allocator a good reason not to use any other register.
    RegisterSet clobberSet = RegisterSet::allGPRs();
    clobberSet.exclude(RegisterSet::stackRegisters());
    clobberSet.exclude(RegisterSet::reservedHardwareRegisters());
    clobberSet.clear(GPRInfo::regT3);
    clobberSet.clear(GPRInfo::regT2);
    clobberSet.clear(GPRInfo::regT6);
    patchpoint->clobberLate(clobberSet);

    // Set EDI.
    patchpoint->setGenerator(
        [&] (CCallHelpers& jit, const StackmapGenerationParams& params) {
            AllowMacroScratchRegisterUsage allowScratch(jit);
            jit.xor64(params[0].gpr(), params[0].gpr());
        });

    // If everything went well, we should have the big number in eax,
    // patchpoint == EDI and whereToStore = EDX.
    // Since EDI == 5, and AH = 5 on 8 bit store, this would go wrong
    // if we use X86 partial registers.
    root->appendNew<MemoryValue>(proc, Store8, Origin(), patchpoint, whereToStore);

    root->appendNewControlValue(proc, Return, Origin(), returnValue);

    int8_t storage = 0xff;
    CHECK(compileAndRun<int64_t>(proc, 0x12345678abcdef12, &storage) == 0x12345678abcdef12);
    CHECK(!storage);
}

void testStore16Arg()
{
    { // Direct addressing.
        Procedure proc;
        BasicBlock* root = proc.addBlock();

        Value* value = root->appendNew<Value>(proc, Trunc, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
        Value* address = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);

        root->appendNew<MemoryValue>(proc, Store16, Origin(), value, address);
        root->appendNewControlValue(proc, Return, Origin(), value);

        int16_t storage = -1;
        CHECK(compileAndRun<int64_t>(proc, 42, &storage) == 42);
        CHECK(storage == 42);
    }

    { // Indexed addressing.
        Procedure proc;
        BasicBlock* root = proc.addBlock();

        Value* value = root->appendNew<Value>(proc, Trunc, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
        Value* base = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
        Value* offset = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR2);
        Value* displacement = root->appendNew<Const64Value>(proc, Origin(), -1);

        Value* baseDisplacement = root->appendNew<Value>(proc, Add, Origin(), displacement, base);
        Value* address = root->appendNew<Value>(proc, Add, Origin(), baseDisplacement, offset);

        root->appendNew<MemoryValue>(proc, Store16, Origin(), value, address);
        root->appendNewControlValue(proc, Return, Origin(), value);

        int16_t storage = -1;
        CHECK(compileAndRun<int64_t>(proc, 42, &storage, 1) == 42);
        CHECK(storage == 42);
    }
}

void testStore16Imm()
{
    { // Direct addressing.
        Procedure proc;
        BasicBlock* root = proc.addBlock();

        Value* value = root->appendNew<Const32Value>(proc, Origin(), 42);
        Value* address = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);

        root->appendNew<MemoryValue>(proc, Store16, Origin(), value, address);
        root->appendNewControlValue(proc, Return, Origin(), value);

        int16_t storage = -1;
        CHECK(compileAndRun<int64_t>(proc, &storage) == 42);
        CHECK(storage == 42);
    }

    { // Indexed addressing.
        Procedure proc;
        BasicBlock* root = proc.addBlock();

        Value* value = root->appendNew<Const32Value>(proc, Origin(), 42);
        Value* base = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
        Value* offset = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
        Value* displacement = root->appendNew<Const64Value>(proc, Origin(), -1);

        Value* baseDisplacement = root->appendNew<Value>(proc, Add, Origin(), displacement, base);
        Value* address = root->appendNew<Value>(proc, Add, Origin(), baseDisplacement, offset);

        root->appendNew<MemoryValue>(proc, Store16, Origin(), value, address);
        root->appendNewControlValue(proc, Return, Origin(), value);

        int16_t storage = -1;
        CHECK(compileAndRun<int64_t>(proc, &storage, 1) == 42);
        CHECK(storage == 42);
    }
}

void testTrunc(int64_t value)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, Trunc, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)));

    CHECK(compileAndRun<int>(proc, value) == static_cast<int>(value));
}

void testAdd1(int value)
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
            root->appendNew<Const32Value>(proc, Origin(), 1)));

    CHECK(compileAndRun<int>(proc, value) == value + 1);
}

void testAdd1Ptr(intptr_t value)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, Add, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
            root->appendNew<ConstPtrValue>(proc, Origin(), 1)));

    CHECK(compileAndRun<intptr_t>(proc, value) == value + 1);
}

void testNeg32(int32_t value)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, Sub, Origin(),
            root->appendNew<Const32Value>(proc, Origin(), 0),
            root->appendNew<Value>(
                proc, Trunc, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0))));

    CHECK(compileAndRun<int32_t>(proc, value) == -value);
}

void testNegPtr(intptr_t value)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, Sub, Origin(),
            root->appendNew<ConstPtrValue>(proc, Origin(), 0),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)));

    CHECK(compileAndRun<intptr_t>(proc, value) == -value);
}

void testStoreAddLoad32(int amount)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    int slot = 37;
    ConstPtrValue* slotPtr = root->appendNew<ConstPtrValue>(proc, Origin(), &slot);
    root->appendNew<MemoryValue>(
        proc, Store, Origin(),
        root->appendNew<Value>(
            proc, Add, Origin(),
            root->appendNew<MemoryValue>(proc, Load, Int32, Origin(), slotPtr),
            root->appendNew<Value>(
                proc, Trunc, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0))),
        slotPtr, 0);
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Const32Value>(proc, Origin(), 0));

    CHECK(!compileAndRun<int>(proc, amount));
    CHECK(slot == 37 + amount);
}

// Make sure the compiler does not try to optimize anything out.
static NEVER_INLINE double zero()
{
    return 0.;
}

static double negativeZero()
{
    return -zero();
}

void addArgTests(const char* filter, Deque<RefPtr<SharedTask<void()>>>& tasks)
{
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
    RUN_BINARY(testMulImm32SignExtend, int32Operands(), int32Operands());
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
}

void addCallTests(const char* filter, Deque<RefPtr<SharedTask<void()>>>& tasks)
{
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
}

void addShrTests(const char* filter, Deque<RefPtr<SharedTask<void()>>>& tasks)
{
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
}

#endif // ENABLE(B3_JIT)
