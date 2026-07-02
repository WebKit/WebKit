/*
 * Copyright (C) 2015-2025 Apple Inc. All rights reserved.
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

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

#if ENABLE(B3_JIT)

Lock crashLock;

bool shouldRun(const TestConfig* config, const char* testName)
{
    if (config->mode == TestConfig::Mode::ListTests) {
        dataLogLn(testName);
        return false;
    }

    const auto* filter = config->filter;
    // FIXME: These tests fail <https://bugs.webkit.org/show_bug.cgi?id=199330>.
    if (!filter && isARM64()) {
        for (auto& failingTest : {
            "testNegFloatWithUselessDoubleConversion",
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
    auto arguments = cCallArgumentValues<T, int32_t>(proc, root);

    Value* value = arguments[0];
    Value* amount = arguments[1];
    root->appendNewControlValue(proc, Return, Origin(),
        root->appendNew<Value>(proc, RotR, Origin(), value, amount));

    CHECK_EQ(compileAndRun<T>(proc, valueInt, shift), rotateRight(valueInt, shift));
}

template<typename T>
void testRotL(T valueInt, int32_t shift)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    auto arguments = cCallArgumentValues<T, int32_t>(proc, root);
    
    Value* value = arguments[0];
    Value* ammount = arguments[1];
    root->appendNewControlValue(proc, Return, Origin(),
        root->appendNew<Value>(proc, RotL, Origin(), value, ammount));
    
    CHECK_EQ(compileAndRun<T>(proc, valueInt, shift), rotateLeft(valueInt, shift));
}


template<typename T>
void testRotRWithImmShift(T valueInt, int32_t shift)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    auto arguments = cCallArgumentValues<T>(proc, root);
    
    Value* value = arguments[0];
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
    auto arguments = cCallArgumentValues<T>(proc, root);

    Value* value = arguments[0];
    Value* ammount = root->appendIntConstant(proc, Origin(), Int32, shift);
    root->appendNewControlValue(proc, Return, Origin(),
        root->appendNew<Value>(proc, RotL, Origin(), value, ammount));

    CHECK_EQ(compileAndRun<T>(proc, valueInt, shift), rotateLeft(valueInt, shift));
}

// Tests for scalar rotate-from-shift-xor-or synthesis added in
// B3ReduceStrength::handleRotateFromShiftXorOr (direct-sibling match) and
// B3OptimizeAssociativeExpressionTrees::optimizeRootedTree (chained XOR/OR).
// The value computed is invariant under the fold, so these tests cover both
// the folded and unfolded paths by construction.
template<typename T>
void testRotRFromShiftOr(T valueInt, int32_t shift)
{
    constexpr uint32_t width = sizeof(T) * 8;
    uint32_t normalizedShift = static_cast<uint32_t>(shift) % width;
    if (normalizedShift == 0)
        return;

    Procedure proc;
    BasicBlock* root = proc.addBlock();
    auto arguments = cCallArgumentValues<T>(proc, root);

    Value* value = arguments[0];
    Value* shlAmount = root->appendIntConstant(proc, Origin(), Int32, width - normalizedShift);
    Value* zshrAmount = root->appendIntConstant(proc, Origin(), Int32, normalizedShift);
    Value* shl = root->appendNew<Value>(proc, Shl, Origin(), value, shlAmount);
    Value* zshr = root->appendNew<Value>(proc, ZShr, Origin(), value, zshrAmount);
    root->appendNewControlValue(proc, Return, Origin(),
        root->appendNew<Value>(proc, BitOr, Origin(), shl, zshr));

    CHECK_EQ(compileAndRun<T>(proc, valueInt), rotateRight(valueInt, static_cast<int32_t>(normalizedShift)));
}

template<typename T>
void testRotRFromShiftXor(T valueInt, int32_t shift)
{
    constexpr uint32_t width = sizeof(T) * 8;
    uint32_t normalizedShift = static_cast<uint32_t>(shift) % width;
    if (normalizedShift == 0)
        return;

    Procedure proc;
    BasicBlock* root = proc.addBlock();
    auto arguments = cCallArgumentValues<T>(proc, root);

    Value* value = arguments[0];
    Value* shlAmount = root->appendIntConstant(proc, Origin(), Int32, width - normalizedShift);
    Value* zshrAmount = root->appendIntConstant(proc, Origin(), Int32, normalizedShift);
    Value* shl = root->appendNew<Value>(proc, Shl, Origin(), value, shlAmount);
    Value* zshr = root->appendNew<Value>(proc, ZShr, Origin(), value, zshrAmount);
    root->appendNewControlValue(proc, Return, Origin(),
        root->appendNew<Value>(proc, BitXor, Origin(), shl, zshr));

    CHECK_EQ(compileAndRun<T>(proc, valueInt), rotateRight(valueInt, static_cast<int32_t>(normalizedShift)));
}

// Reversed operand order: ZShr first, Shl second. The fold must handle both orderings.
template<typename T>
void testRotRFromShiftXorReversed(T valueInt, int32_t shift)
{
    constexpr uint32_t width = sizeof(T) * 8;
    uint32_t normalizedShift = static_cast<uint32_t>(shift) % width;
    if (normalizedShift == 0)
        return;

    Procedure proc;
    BasicBlock* root = proc.addBlock();
    auto arguments = cCallArgumentValues<T>(proc, root);

    Value* value = arguments[0];
    Value* shlAmount = root->appendIntConstant(proc, Origin(), Int32, width - normalizedShift);
    Value* zshrAmount = root->appendIntConstant(proc, Origin(), Int32, normalizedShift);
    Value* zshr = root->appendNew<Value>(proc, ZShr, Origin(), value, zshrAmount);
    Value* shl = root->appendNew<Value>(proc, Shl, Origin(), value, shlAmount);
    root->appendNewControlValue(proc, Return, Origin(),
        root->appendNew<Value>(proc, BitXor, Origin(), zshr, shl));

    CHECK_EQ(compileAndRun<T>(proc, valueInt), rotateRight(valueInt, static_cast<int32_t>(normalizedShift)));
}

// SHA-256 Sigma1 pattern for 32-bit inputs:
//   (x >>> 6) ^ (x >>> 11) ^ (x >>> 25) ^ (x << 26) ^ (x << 21) ^ (x << 7)
// Pairs: (>>>6, <<26), (>>>11, <<21), (>>>25, <<7). Each pair is a rotate-right.
// Exercises the multi-leaf path in OptimizeAssociativeExpressionTrees::optimizeRootedTree.
void testRotRFromShiftXorChainSHA256Sigma1_32(int32_t valueInt)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    auto arguments = cCallArgumentValues<int32_t>(proc, root);

    Value* x = arguments[0];
    auto buildShift = [&](B3::Opcode op, int32_t amt) -> Value* {
        Value* amount = root->appendIntConstant(proc, Origin(), Int32, amt);
        return root->appendNew<Value>(proc, op, Origin(), x, amount);
    };
    Value* zshr6 = buildShift(ZShr, 6);
    Value* zshr11 = buildShift(ZShr, 11);
    Value* zshr25 = buildShift(ZShr, 25);
    Value* shl26 = buildShift(Shl, 26);
    Value* shl21 = buildShift(Shl, 21);
    Value* shl7 = buildShift(Shl, 7);
    Value* xor0 = root->appendNew<Value>(proc, BitXor, Origin(), zshr6, zshr11);
    Value* xor1 = root->appendNew<Value>(proc, BitXor, Origin(), xor0, zshr25);
    Value* xor2 = root->appendNew<Value>(proc, BitXor, Origin(), xor1, shl26);
    Value* xor3 = root->appendNew<Value>(proc, BitXor, Origin(), xor2, shl21);
    Value* xor4 = root->appendNew<Value>(proc, BitXor, Origin(), xor3, shl7);
    root->appendNewControlValue(proc, Return, Origin(), xor4);

    int32_t expected = rotateRight(valueInt, 6) ^ rotateRight(valueInt, 11) ^ rotateRight(valueInt, 25);
    CHECK_EQ(compileAndRun<int32_t>(proc, valueInt), expected);
}

// Analogous pattern at 64 bits: (x >>> 14) ^ (x >>> 18) ^ (x >>> 41) ^ (x << 50) ^ (x << 46) ^ (x << 23)
// Pairs: (>>>14, <<50), (>>>18, <<46), (>>>41, <<23). Each pair is a 64-bit rotate-right.
void testRotRFromShiftXorChainSHA512Sigma1_64(int64_t valueInt)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    auto arguments = cCallArgumentValues<int64_t>(proc, root);

    Value* x = arguments[0];
    auto buildShift = [&](B3::Opcode op, int32_t amt) -> Value* {
        Value* amount = root->appendIntConstant(proc, Origin(), Int32, amt);
        return root->appendNew<Value>(proc, op, Origin(), x, amount);
    };
    Value* zshr14 = buildShift(ZShr, 14);
    Value* zshr18 = buildShift(ZShr, 18);
    Value* zshr41 = buildShift(ZShr, 41);
    Value* shl50 = buildShift(Shl, 50);
    Value* shl46 = buildShift(Shl, 46);
    Value* shl23 = buildShift(Shl, 23);
    Value* xor0 = root->appendNew<Value>(proc, BitXor, Origin(), zshr14, zshr18);
    Value* xor1 = root->appendNew<Value>(proc, BitXor, Origin(), xor0, zshr41);
    Value* xor2 = root->appendNew<Value>(proc, BitXor, Origin(), xor1, shl50);
    Value* xor3 = root->appendNew<Value>(proc, BitXor, Origin(), xor2, shl46);
    Value* xor4 = root->appendNew<Value>(proc, BitXor, Origin(), xor3, shl23);
    root->appendNewControlValue(proc, Return, Origin(), xor4);

    int64_t expected = rotateRight(valueInt, 14) ^ rotateRight(valueInt, 18) ^ rotateRight(valueInt, 41);
    CHECK_EQ(compileAndRun<int64_t>(proc, valueInt), expected);
}

// SHA-256 lowercase sigma0 pattern: (x >>> 7) ^ (x >>> 18) ^ (x >>> 3) ^ (x << 25) ^ (x << 14)
// Pairs: (>>>7, <<25), (>>>18, <<14). The lone (>>>3) stays as a raw shift.
// Verifies that the fold only consumes matching halves and leaves unrelated shifts alone.
void testRotRFromShiftXorChainSHA256sigma0_32(int32_t valueInt)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    auto arguments = cCallArgumentValues<int32_t>(proc, root);

    Value* x = arguments[0];
    auto buildShift = [&](B3::Opcode op, int32_t amt) -> Value* {
        Value* amount = root->appendIntConstant(proc, Origin(), Int32, amt);
        return root->appendNew<Value>(proc, op, Origin(), x, amount);
    };
    Value* zshr7 = buildShift(ZShr, 7);
    Value* zshr18 = buildShift(ZShr, 18);
    Value* zshr3 = buildShift(ZShr, 3);
    Value* shl25 = buildShift(Shl, 25);
    Value* shl14 = buildShift(Shl, 14);
    Value* xor0 = root->appendNew<Value>(proc, BitXor, Origin(), zshr7, zshr18);
    Value* xor1 = root->appendNew<Value>(proc, BitXor, Origin(), xor0, zshr3);
    Value* xor2 = root->appendNew<Value>(proc, BitXor, Origin(), xor1, shl25);
    Value* xor3 = root->appendNew<Value>(proc, BitXor, Origin(), xor2, shl14);
    root->appendNewControlValue(proc, Return, Origin(), xor3);

    int32_t expected = rotateRight(valueInt, 7) ^ rotateRight(valueInt, 18) ^ (static_cast<uint32_t>(valueInt) >> 3);
    CHECK_EQ(compileAndRun<int32_t>(proc, valueInt), expected);
}

// BitOr analogue of the SHA-256 Sigma1 pattern for Int32:
//   (x >>> 6) | (x >>> 11) | (x >>> 25) | (x << 26) | (x << 21) | (x << 7)
// Each rotate pair has non-overlapping bits, so OR is equivalent to XOR here.
// Exercises the BitOr path of the multi-leaf handling in
// OptimizeAssociativeExpressionTrees::optimizeRootedTree.
void testRotRFromShiftOrChainSHA256Sigma1_32(int32_t valueInt)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    auto arguments = cCallArgumentValues<int32_t>(proc, root);

    Value* x = arguments[0];
    auto buildShift = [&](B3::Opcode op, int32_t amt) -> Value* {
        Value* amount = root->appendIntConstant(proc, Origin(), Int32, amt);
        return root->appendNew<Value>(proc, op, Origin(), x, amount);
    };
    Value* zshr6 = buildShift(ZShr, 6);
    Value* zshr11 = buildShift(ZShr, 11);
    Value* zshr25 = buildShift(ZShr, 25);
    Value* shl26 = buildShift(Shl, 26);
    Value* shl21 = buildShift(Shl, 21);
    Value* shl7 = buildShift(Shl, 7);
    Value* or0 = root->appendNew<Value>(proc, BitOr, Origin(), zshr6, zshr11);
    Value* or1 = root->appendNew<Value>(proc, BitOr, Origin(), or0, zshr25);
    Value* or2 = root->appendNew<Value>(proc, BitOr, Origin(), or1, shl26);
    Value* or3 = root->appendNew<Value>(proc, BitOr, Origin(), or2, shl21);
    Value* or4 = root->appendNew<Value>(proc, BitOr, Origin(), or3, shl7);
    root->appendNewControlValue(proc, Return, Origin(), or4);

    int32_t expected = rotateRight(valueInt, 6) | rotateRight(valueInt, 11) | rotateRight(valueInt, 25);
    CHECK_EQ(compileAndRun<int32_t>(proc, valueInt), expected);
}

// Use-count safety: when a shift Value inside the XOR/OR tree is ALSO used by an
// external consumer, the fold must not mutate that shift. It can only replace
// the pointer inside its local leaf vector. Here Shl(x, 26) is used both inside
// the SHA-256 Sigma1 chain and as an operand of the outer Add, so its useCount
// is >= 2 and the original Shl must survive.
void testRotRFromShiftXorChainSharedShiftOperand(int32_t valueInt)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    auto arguments = cCallArgumentValues<int32_t>(proc, root);

    Value* x = arguments[0];
    auto buildShift = [&](B3::Opcode op, int32_t amt) -> Value* {
        Value* amount = root->appendIntConstant(proc, Origin(), Int32, amt);
        return root->appendNew<Value>(proc, op, Origin(), x, amount);
    };
    Value* zshr6 = buildShift(ZShr, 6);
    Value* zshr11 = buildShift(ZShr, 11);
    Value* zshr25 = buildShift(ZShr, 25);
    Value* shl26 = buildShift(Shl, 26);
    Value* shl21 = buildShift(Shl, 21);
    Value* shl7 = buildShift(Shl, 7);
    Value* xor0 = root->appendNew<Value>(proc, BitXor, Origin(), zshr6, zshr11);
    Value* xor1 = root->appendNew<Value>(proc, BitXor, Origin(), xor0, zshr25);
    Value* xor2 = root->appendNew<Value>(proc, BitXor, Origin(), xor1, shl26);
    Value* xor3 = root->appendNew<Value>(proc, BitXor, Origin(), xor2, shl21);
    Value* xor4 = root->appendNew<Value>(proc, BitXor, Origin(), xor3, shl7);
    // shl26 is used twice: once inside the XOR chain, once as an Add operand.
    root->appendNewControlValue(proc, Return, Origin(),
        root->appendNew<Value>(proc, Add, Origin(), xor4, shl26));

    int32_t sigma1 = rotateRight(valueInt, 6) ^ rotateRight(valueInt, 11) ^ rotateRight(valueInt, 25);
    int32_t expected = static_cast<int32_t>(sigma1 + (static_cast<uint32_t>(valueInt) << 26));
    CHECK_EQ(compileAndRun<int32_t>(proc, valueInt), expected);
}

// Negative test: two shifts on DIFFERENT base values must NOT be folded into a rotate.
// This exists to guard against accidental matches when the fold is extended; the
// computed value depends on the distinction between x and y.
void testShiftOrDifferentBasesNoRotate(int32_t xValue, int32_t yValue, int32_t shift)
{
    constexpr uint32_t width = 32;
    uint32_t normalizedShift = static_cast<uint32_t>(shift) % width;
    if (normalizedShift == 0)
        return;

    Procedure proc;
    BasicBlock* root = proc.addBlock();
    auto arguments = cCallArgumentValues<int32_t, int32_t>(proc, root);

    Value* x = arguments[0];
    Value* y = arguments[1];
    Value* shlAmount = root->appendIntConstant(proc, Origin(), Int32, width - normalizedShift);
    Value* zshrAmount = root->appendIntConstant(proc, Origin(), Int32, normalizedShift);
    Value* shl = root->appendNew<Value>(proc, Shl, Origin(), x, shlAmount);
    Value* zshr = root->appendNew<Value>(proc, ZShr, Origin(), y, zshrAmount);
    root->appendNewControlValue(proc, Return, Origin(),
        root->appendNew<Value>(proc, BitOr, Origin(), shl, zshr));

    int32_t expected = static_cast<int32_t>(
        (static_cast<uint32_t>(xValue) << (width - normalizedShift))
        | (static_cast<uint32_t>(yValue) >> normalizedShift));
    CHECK_EQ(compileAndRun<int32_t>(proc, xValue, yValue), expected);
}

// Shifts whose amounts do not sum to the type width must NOT be folded into a rotate.
void testShiftOrMismatchedAmountsNoRotate(int32_t valueInt)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    auto arguments = cCallArgumentValues<int32_t>(proc, root);

    Value* value = arguments[0];
    // N + M = 5 + 10 = 15, not 32 - so this is NOT a rotate.
    Value* shlAmount = root->appendIntConstant(proc, Origin(), Int32, 5);
    Value* zshrAmount = root->appendIntConstant(proc, Origin(), Int32, 10);
    Value* shl = root->appendNew<Value>(proc, Shl, Origin(), value, shlAmount);
    Value* zshr = root->appendNew<Value>(proc, ZShr, Origin(), value, zshrAmount);
    root->appendNewControlValue(proc, Return, Origin(),
        root->appendNew<Value>(proc, BitOr, Origin(), shl, zshr));

    int32_t expected = static_cast<int32_t>(
        (static_cast<uint32_t>(valueInt) << 5) | (static_cast<uint32_t>(valueInt) >> 10));
    CHECK_EQ(compileAndRun<int32_t>(proc, valueInt), expected);
}

template<typename T>
void testComputeDivisionMagic(T value, T magicMultiplier, unsigned shift)
{
    DivisionMagic<T> magic = computeSignedDivisionMagic(value);
    CHECK_EQ(magic.magicMultiplier, magicMultiplier);
    CHECK_EQ(magic.shift, shift);
}

void run(const TestConfig* config)
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

    addArgTests(config, tasks);

    RUN_UNARY(testNegDouble, floatingPointOperands<double>());
    RUN_UNARY(testNegFloat, floatingPointOperands<float>());
    RUN_UNARY(testNegFloatWithUselessDoubleConversion, floatingPointOperands<float>());
    RUN(testImpureNaN());

    addBitTests(config, tasks);

    RUN(testShlArgs(1, 0));
    RUN(testShlArgs(1, 1));
    RUN(testShlArgs(1, 32));
    RUN(testShlArgs(1, 62));
    RUN(testShlArgs(0xffffffffffffffff, 0));
    RUN(testShlArgs(0xffffffffffffffff, 1));
    RUN(testShlArgs(0xffffffffffffffff, 63));
    RUN(testShlImms(1, 0));
    RUN(testShlImms(1, 1));
    RUN(testShlImms(1, 32));
    RUN(testShlImms(1, 62));
    RUN(testShlImms(1, 65));
    RUN(testShlImms(0xffffffffffffffff, 0));
    RUN(testShlImms(0xffffffffffffffff, 1));
    RUN(testShlImms(0xffffffffffffffff, 63));
    RUN(testShlArgImm(1, 0));
    RUN(testShlArgImm(1, 1));
    RUN(testShlArgImm(1, 32));
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

    addShrTests(config, tasks);

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

    RUN_UNARY(testFTruncArg, floatingPointOperands<double>());
    RUN_UNARY(testFTruncImm, floatingPointOperands<double>());
    RUN_UNARY(testFTruncMem, floatingPointOperands<double>());
    RUN_UNARY(testFTruncArg, floatingPointOperands<float>());
    RUN_UNARY(testFTruncImm, floatingPointOperands<float>());
    RUN_UNARY(testFTruncMem, floatingPointOperands<float>());

    RUN_UNARY(testSqrtArg, floatingPointOperands<double>());
    RUN_UNARY(testSqrtImm, floatingPointOperands<double>());
    RUN_UNARY(testSqrtMem, floatingPointOperands<double>());
    RUN_UNARY(testSqrtArg, floatingPointOperands<float>());
    RUN_UNARY(testSqrtImm, floatingPointOperands<float>());
    RUN_UNARY(testSqrtMem, floatingPointOperands<float>());
    RUN_UNARY(testSqrtArgWithUselessDoubleConversion, floatingPointOperands<float>());
    RUN_UNARY(testSqrtArgWithEffectfulDoubleConversion, floatingPointOperands<float>());

    RUN(testPurifyNaN());

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
    RUN_UNARY(testConvertDoubleToFloatToDouble, floatingPointOperands<double>());
    RUN_UNARY(testConvertDoubleToFloatToDoubleToFloat, floatingPointOperands<double>());
    RUN_UNARY(testConvertDoubleToFloatEqual, floatingPointOperands<double>());
    RUN_UNARY(testStoreDouble, floatingPointOperands<double>());
    RUN_UNARY(testStoreDoubleConstant, floatingPointOperands<double>());
    RUN_UNARY(testStoreFloat, floatingPointOperands<double>());
    RUN_UNARY(testStoreFloatConstant, floatingPointOperands<double>());
    RUN_UNARY(testStoreDoubleConstantAsFloat, floatingPointOperands<double>());
    RUN_UNARY(testLoadFloatConvertDoubleConvertFloatStoreFloat, floatingPointOperands<float>());
    RUN_UNARY(testFroundArg, floatingPointOperands<double>());
    RUN_UNARY(testFroundMem, floatingPointOperands<double>());

    RUN(testIToD64Arg());
    RUN(testIToF64Arg());
    RUN(testIToD32Arg());
    RUN(testIToF32Arg());
    RUN(testIToDU32Arg());
    RUN(testIToFU32Arg());
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
    RUN_UNARY(testInt52RoundTripUnary, int32Operands());
    RUN(testInt52RoundTripBinary());
    RUN(testTruncSShrAddUnalignedConstant());

#if !CPU(ARM)
    RUN_UNARY(testCheckAddRemoveCheckWithSExt8, int8Operands());
    RUN_UNARY(testCheckAddRemoveCheckWithSExt16, int16Operands());
    RUN_UNARY(testCheckAddRemoveCheckWithSExt32, int32Operands());
    RUN_UNARY(testCheckAddRemoveCheckWithZExt32, int32Operands());
#endif

    RUN(testStoreZeroReg());
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
    RUN(testSimplePatchpointWithoutOuputClobbersFPArgs());
    RUN(testSimplePatchpointWithOuputClobbersGPArgs());
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
    if constexpr (!is32Bit()) {
        // Can't handle ConstDoubleValue arguments to patchpoints on 32 bits.
        RUN(testPatchpointManyWarmAnyImms());
        RUN(testPatchpointManyColdAnyImms());
    }
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
#if !CPU(ARM)
    RUN(testCheckAddImm());
    RUN(testCheckAddImmCommute());
    RUN(testCheckAddImmSomeRegister());
    RUN(testCheckAdd());
    RUN(testCheckAdd64());
    RUN(testCheckAdd64Range());
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
#endif

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

    addLoadTests(config, tasks);
    addTupleTests(config, tasks);

    RUN(testSpillGP());
    RUN(testSpillFP());

    RUN(testWasmAddressDoesNotCSE());
    RUN(testStoreAfterClobberDifferentWidth());
    RUN(testStoreAfterClobberExitsSideways());
    RUN(testStoreAfterClobberDifferentWidthSuccessor());
    RUN(testStoreAfterClobberExitsSidewaysSuccessor());
    RUN(testNarrowLoad());
    RUN(testNarrowLoadClobber());
    RUN(testNarrowLoadClobberNarrow());
    RUN(testNarrowLoadNotClobber());
    RUN(testNarrowLoadUpper());

    RUN(testInt32ToDoublePartialRegisterStall());
    RUN(testInt32ToDoublePartialRegisterWithoutStall());

    addCallTests(config, tasks);

    RUN(testForcedSpillCalleeOnStack());

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

    addSExtTests(config, tasks);

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
    RUN(testSelectInt32WithZeroElse());
    RUN(testSelectInt64WithZeroElse());
    RUN(testSelectInt32ImmWithZeroElse());
    RUN(testSelectTestWithZeroElse());
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

    addSShrShTests(config, tasks);

    RUN(testCheckMul64SShr());

    RUN_BINARY(testRotR, int32Operands(), int32Operands());
    RUN_BINARY(testRotR, int64Operands(), int32Operands());
    RUN_BINARY(testRotL, int32Operands(), int32Operands());
    RUN_BINARY(testRotL, int64Operands(), int32Operands());

    RUN_BINARY(testRotRWithImmShift, int32Operands(), int32Operands());
    RUN_BINARY(testRotRWithImmShift, int64Operands(), int32Operands());
    RUN_BINARY(testRotLWithImmShift, int32Operands(), int32Operands());
    RUN_BINARY(testRotLWithImmShift, int64Operands(), int32Operands());

    RUN_BINARY(testRotRFromShiftOr<int32_t>, int32Operands(), int32Operands());
    RUN_BINARY(testRotRFromShiftOr<int64_t>, int64Operands(), int32Operands());
    RUN_BINARY(testRotRFromShiftXor<int32_t>, int32Operands(), int32Operands());
    RUN_BINARY(testRotRFromShiftXor<int64_t>, int64Operands(), int32Operands());
    RUN_BINARY(testRotRFromShiftXorReversed<int32_t>, int32Operands(), int32Operands());
    RUN_BINARY(testRotRFromShiftXorReversed<int64_t>, int64Operands(), int32Operands());
    RUN_UNARY(testRotRFromShiftXorChainSHA256Sigma1_32, int32Operands());
    RUN_UNARY(testRotRFromShiftXorChainSHA512Sigma1_64, int64Operands());
    RUN_UNARY(testRotRFromShiftXorChainSHA256sigma0_32, int32Operands());
    RUN_UNARY(testRotRFromShiftOrChainSHA256Sigma1_32, int32Operands());
    RUN_UNARY(testRotRFromShiftXorChainSharedShiftOperand, int32Operands());
    RUN_TERNARY(testShiftOrDifferentBasesNoRotate, int32Operands(), int32Operands(), int32Operands());
    RUN_UNARY(testShiftOrMismatchedAmountsNoRotate, int32Operands());

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
    RUN(testMoveConstantsWithLargeOffsets());
    if (Options::useWasmSIMD())
        RUN(testMoveConstantsSIMD());
    RUN(testPCOriginMapDoesntInsertNops());
    RUN(testPinRegisters());
    RUN(testReduceStrengthReassociation(true));
    RUN(testReduceStrengthReassociation(false));
    RUN_BINARY(testReduceStrengthTruncInt64Constant, int64Operands(), int32Operands());
    RUN_BINARY(testReduceStrengthTruncDoubleConstant, floatingPointOperands<double>(), floatingPointOperands<float>());
    RUN(testReduceStrengthMulDoubleByTwo());
    RUN(testReduceStrengthMulFloatByTwo());
    RUN(testReduceStrengthMulDoubleByNegOne());
    RUN(testReduceStrengthMulFloatByNegOne());
    RUN(testReduceStrengthMulDoubleByNegTwo());
    RUN(testReduceStrengthMulFloatByNegTwo());
    RUN(testReduceStrengthDivDoubleByNegOne());
    RUN(testReduceStrengthDivFloatByNegOne());
    RUN(testReduceStrengthDivDoubleByTwo());
    RUN(testReduceStrengthDivFloatByTwo());
    RUN(testReduceStrengthDivDoubleByFour());
    RUN(testReduceStrengthDivFloatByFour());
    RUN(testReduceStrengthDivDoubleByNegTwo());
    RUN(testReduceStrengthDivFloatByNegTwo());
    RUN(testReduceStrengthBelowEqualZeroInt32());
    RUN(testReduceStrengthBelowEqualZeroInt64());
    RUN(testReduceStrengthBelowOneInt32());
    RUN(testReduceStrengthBelowOneInt64());
    RUN(testReduceStrengthAboveEqualOneInt32());
    RUN(testReduceStrengthAboveEqualOneInt64());
    RUN(testReduceStrengthAboveZeroInt32());
    RUN(testReduceStrengthAboveZeroInt64());
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

    addAtomicTests(config, tasks);
    RUN(testDepend32());
    if constexpr (!is32Bit()) {
        // Test only applicable on 64-bits.
        RUN(testDepend64());
    }

    RUN(testWasmBoundsCheck(0));
    RUN(testWasmBoundsCheck(100));
    RUN(testWasmBoundsCheck(10000));
    RUN(testWasmBoundsCheck(std::numeric_limits<unsigned>::max() - 5));

    RUN(testWasmAddress());
    RUN(testWasmAddressWithOffset());
    
    RUN(testFastTLSLoad());
    RUN(testFastTLSStore());

    RUN(testDoubleLiteralComparison(std::bit_cast<double>(0x8000000000000001ull), std::bit_cast<double>(0x0000000000000000ull)));
    RUN(testDoubleLiteralComparison(std::bit_cast<double>(0x0000000000000000ull), std::bit_cast<double>(0x8000000000000001ull)));
    RUN(testDoubleLiteralComparison(125.3144446948241, 125.3144446948242));
    RUN(testDoubleLiteralComparison(125.3144446948242, 125.3144446948241));

    RUN(testFloatEqualOrUnorderedFolding());
    RUN(testFloatEqualOrUnorderedFoldingNaN());
    RUN(testFloatEqualOrUnorderedDontFold());

    RUN(testShuffleDoesntTrashCalleeSaves());
    RUN(testDemotePatchpointTerminal());

    RUN(testLoopWithMultipleHeaderEdges());

    RUN(testInfiniteLoopDoesntCauseBadHoisting());

    RUN(testFloatMaxMin());
    RUN(testDoubleMaxMin());

    RUN(testConstDoubleMove());
    RUN(testConstFloatMove());

    RUN(testConstDoubleZero());
    RUN(testConstDoubleNegativeZero());
    RUN(testConstFloatZero());
    RUN(testConstFloatNegativeZero());
    RUN(testConstDoubleAddZero());
    RUN(testConstFloatAddZero());
    RUN(testConstDoubleCompareZero());
    RUN(testConstFloatCompareZero());
    RUN(testConstDoubleSelectZero());
    RUN(testConstFloatSelectZero());
    RUN(testConstDoubleMultipleZeroUses());
    RUN(testConstFloatMultipleZeroUses());

    RUN(testLoadImmutable());

    // ARM64 conditional compare (ccmp) tests
    RUN(testCCmpAnd32(1, 1, 2, 2));  // both true
    RUN(testCCmpAnd32(1, 2, 2, 2));  // first false
    RUN(testCCmpAnd32(1, 1, 2, 3));  // second false
    RUN(testCCmpAnd32(1, 2, 2, 3));  // both false

    RUN(testCCmpAnd64(1, 1, 2, 2));  // both true
    RUN(testCCmpAnd64(1, 2, 2, 2));  // first false
    RUN(testCCmpAnd64(1, 1, 2, 3));  // second false
    RUN(testCCmpAnd64(1, 2, 2, 3));  // both false

    RUN(testCCmpOr32(1, 1, 2, 2));   // both true
    RUN(testCCmpOr32(1, 1, 2, 3));   // first true
    RUN(testCCmpOr32(1, 2, 2, 2));   // second true
    RUN(testCCmpOr32(1, 2, 2, 3));   // both false

    RUN(testCCmpOr64(1, 1, 2, 2));   // both true
    RUN(testCCmpOr64(1, 1, 2, 3));   // first true
    RUN(testCCmpOr64(1, 2, 2, 2));   // second true
    RUN(testCCmpOr64(1, 2, 2, 3));   // both false

    // 3-comparison chain tests
    RUN(testCCmpAndAnd32(1, 1, 2, 2, 3, 3));  // all true
    RUN(testCCmpAndAnd32(1, 1, 2, 2, 3, 4));  // first two true, last false
    RUN(testCCmpAndAnd32(1, 1, 2, 3, 3, 3));  // first true, second false
    RUN(testCCmpAndAnd32(1, 2, 2, 2, 3, 3));  // first false
    RUN(testCCmpAndAnd32(1, 2, 2, 3, 3, 4));  // all false

    RUN(testCCmpOrOr32(1, 1, 2, 2, 3, 3));   // all true
    RUN(testCCmpOrOr32(1, 1, 2, 3, 3, 4));   // first true
    RUN(testCCmpOrOr32(1, 2, 2, 2, 3, 4));   // second true
    RUN(testCCmpOrOr32(1, 2, 2, 3, 3, 3));   // third true
    RUN(testCCmpOrOr32(1, 2, 2, 3, 3, 4));   // all false

    RUN(testCCmpAndOr32(1, 1, 2, 2, 3, 4));  // (true && true) || false = true
    RUN(testCCmpAndOr32(1, 1, 2, 3, 3, 3));  // (true && false) || true = true
    RUN(testCCmpAndOr32(1, 2, 2, 2, 3, 3));  // (false && true) || true = true
    RUN(testCCmpAndOr32(1, 2, 2, 3, 3, 4));  // (false && false) || false = false
    RUN(testCCmpAndOr32(1, 1, 2, 2, 3, 3));  // (true && true) || true = true

    // Tests for ccmn (negative immediates) and large immediates
    RUN(testCCmnAnd32WithNegativeImm(15, -5));  // both true
    RUN(testCCmnAnd32WithNegativeImm(5, -5));   // first false
    RUN(testCCmnAnd32WithNegativeImm(15, 0));   // second false
    RUN(testCCmnAnd32WithNegativeImm(5, 0));    // both false

    RUN(testCCmnAnd64WithNegativeImm(15, -31)); // both true
    RUN(testCCmnAnd64WithNegativeImm(5, -31));  // first false
    RUN(testCCmnAnd64WithNegativeImm(15, 0));   // second false
    RUN(testCCmnAnd64WithNegativeImm(5, 0));    // both false

    RUN(testCCmpWithLargePositiveImm(15, 100)); // both true
    RUN(testCCmpWithLargePositiveImm(5, 100));  // first false
    RUN(testCCmpWithLargePositiveImm(15, 0));   // second false
    RUN(testCCmpWithLargePositiveImm(5, 0));    // both false

    RUN(testCCmpWithLargeNegativeImm(15, -100)); // both true
    RUN(testCCmpWithLargeNegativeImm(5, -100));  // first false
    RUN(testCCmpWithLargeNegativeImm(15, 0));    // second false
    RUN(testCCmpWithLargeNegativeImm(5, 0));     // both false

    // Tests for ccmp optimizations
    RUN(testCCmpSmartOperandOrdering32(5, 1000));    // both true
    RUN(testCCmpSmartOperandOrdering32(5, 999));     // first true, second false
    RUN(testCCmpSmartOperandOrdering32(4, 1000));    // first false, second true
    RUN(testCCmpSmartOperandOrdering32(4, 999));     // both false

    RUN(testCCmpSmartOperandOrdering64(10, 5000));   // both true
    RUN(testCCmpSmartOperandOrdering64(10, 4999));   // first true, second false
    RUN(testCCmpSmartOperandOrdering64(9, 5000));    // first false, second true
    RUN(testCCmpSmartOperandOrdering64(9, 4999));    // both false

    RUN(testCCmpOperandCommutation32(15, 101));      // both true
    RUN(testCCmpOperandCommutation32(15, 100));      // first true, second false
    RUN(testCCmpOperandCommutation32(14, 101));      // first false, second true
    RUN(testCCmpOperandCommutation32(14, 100));      // both false

    RUN(testCCmpOperandCommutation64(49, 20));       // both true
    RUN(testCCmpOperandCommutation64(49, 21));       // first true, second false
    RUN(testCCmpOperandCommutation64(50, 20));       // first false, second true
    RUN(testCCmpOperandCommutation64(50, 21));       // both false

    RUN(testCCmpCombinedOptimizations(10, 2000));    // both true
    RUN(testCCmpCombinedOptimizations(10, 1999));    // first true, second false
    RUN(testCCmpCombinedOptimizations(9, 2000));     // first false, second true
    RUN(testCCmpCombinedOptimizations(9, 1999));     // both false

    RUN(testCCmpZeroRegisterOptimization32(0, 6));   // both true
    RUN(testCCmpZeroRegisterOptimization32(0, 5));   // first true, second false
    RUN(testCCmpZeroRegisterOptimization32(1, 6));   // first false, second true
    RUN(testCCmpZeroRegisterOptimization32(1, 5));   // both false

    RUN(testCCmpZeroRegisterOptimization64(0, 99));  // both true
    RUN(testCCmpZeroRegisterOptimization64(0, 100)); // first true, second false
    RUN(testCCmpZeroRegisterOptimization64(1, 99));  // first false, second true
    RUN(testCCmpZeroRegisterOptimization64(1, 100)); // both false

    // Mixed AND/OR tests - now supported with tree canonicalization
    RUN(testCCmpMixedAndOr32(5, 5, 5));              // AND true, OR false -> true
    RUN(testCCmpMixedAndOr32(101, 5, 5));            // AND false, OR true -> true
    RUN(testCCmpMixedAndOr32(5, 6, 5));              // AND false, OR false -> false
    RUN(testCCmpMixedAndOr32(50, 50, 50));           // AND true, OR false -> true

    RUN(testCCmpMixedOrAnd32(-1, 10, 10));           // OR true, AND false -> true
    RUN(testCCmpMixedOrAnd32(0, 60, 60));            // OR false, AND true -> true
    RUN(testCCmpMixedOrAnd32(0, 10, 20));            // OR false, AND false -> false
    RUN(testCCmpMixedOrAnd32(-5, 40, 40));           // OR true, AND false -> true

    // Negation tests - V8's (chain) == 0 optimization
    RUN(testCCmpNegatedAnd32(15, 20));               // !(true && true) = false
    RUN(testCCmpNegatedAnd32(15, 10));               // !(true && false) = true
    RUN(testCCmpNegatedAnd32(5, 20));                // !(false && true) = true
    RUN(testCCmpNegatedAnd32(5, 10));                // !(false && false) = true

    RUN(testCCmpNegatedOr32(3, 50));                 // !(true || false) = false
    RUN(testCCmpNegatedOr32(3, 100));                // !(true || true) = false
    RUN(testCCmpNegatedOr32(10, 100));               // !(false || true) = false
    RUN(testCCmpNegatedOr32(10, 50));                // !(false || false) = true

    // Mixed-width compare chain tests (per-ccmp width handling)
    RUN(testCCmpMixedWidth32And64(5, 1000, 10));    // all match
    RUN(testCCmpMixedWidth32And64(5, 1000, 9));     // last doesn't match
    RUN(testCCmpMixedWidth32And64(5, 999, 10));     // middle doesn't match
    RUN(testCCmpMixedWidth32And64(4, 1000, 10));    // first doesn't match
    RUN(testCCmpMixedWidth64And32(5000, 10));       // both match
    RUN(testCCmpMixedWidth64And32(5000, 9));        // second doesn't match
    RUN(testCCmpMixedWidth64And32(4999, 10));       // first doesn't match

    // ARM64 fccmp tests (floating-point conditional compare)
    RUN(testFCCmpAndDouble(1.0, 1.0, 2.0, 2.0));    // both true
    RUN(testFCCmpAndDouble(1.0, 2.0, 2.0, 2.0));    // first false
    RUN(testFCCmpAndDouble(1.0, 1.0, 2.0, 3.0));    // second false
    RUN(testFCCmpAndDouble(1.0, 2.0, 2.0, 3.0));    // both false

    RUN(testFCCmpOrDouble(1.0, 1.0, 2.0, 2.0));     // both true
    RUN(testFCCmpOrDouble(1.0, 1.0, 2.0, 3.0));     // first true
    RUN(testFCCmpOrDouble(1.0, 2.0, 2.0, 2.0));     // second true
    RUN(testFCCmpOrDouble(1.0, 2.0, 2.0, 3.0));     // both false

    RUN(testFCCmpAndFloat(1.0f, 1.0f, 2.0f, 2.0f)); // both true
    RUN(testFCCmpAndFloat(1.0f, 2.0f, 2.0f, 2.0f)); // first false
    RUN(testFCCmpAndFloat(1.0f, 1.0f, 2.0f, 3.0f)); // second false
    RUN(testFCCmpAndFloat(1.0f, 2.0f, 2.0f, 3.0f)); // both false

    RUN(testFCCmpOrFloat(1.0f, 1.0f, 2.0f, 2.0f));  // both true
    RUN(testFCCmpOrFloat(1.0f, 1.0f, 2.0f, 3.0f));  // first true
    RUN(testFCCmpOrFloat(1.0f, 2.0f, 2.0f, 2.0f));  // second true
    RUN(testFCCmpOrFloat(1.0f, 2.0f, 2.0f, 3.0f));  // both false

    RUN(testFCCmpAndAndDouble(1.0, 1.0, 2.0, 2.0, 3.0, 3.0));  // all true
    RUN(testFCCmpAndAndDouble(1.0, 1.0, 2.0, 2.0, 3.0, 4.0));  // first two true, last false
    RUN(testFCCmpAndAndDouble(1.0, 1.0, 2.0, 3.0, 3.0, 3.0));  // first true, second false
    RUN(testFCCmpAndAndDouble(1.0, 2.0, 2.0, 2.0, 3.0, 3.0));  // first false
    RUN(testFCCmpAndAndDouble(1.0, 2.0, 2.0, 3.0, 3.0, 4.0));  // all false

    RUN(testFCCmpMixedIntDouble(5, 5, 1.0, 2.0));   // int true, double true
    RUN(testFCCmpMixedIntDouble(5, 6, 1.0, 2.0));   // int false, double true
    RUN(testFCCmpMixedIntDouble(5, 5, 2.0, 1.0));   // int true, double false
    RUN(testFCCmpMixedIntDouble(5, 6, 2.0, 1.0));   // int false, double false

    RUN(testFCCmpMixedDoubleInt(1.0, 2.0, 5, 5));   // double true, int true
    RUN(testFCCmpMixedDoubleInt(2.0, 1.0, 5, 5));   // double false, int true
    RUN(testFCCmpMixedDoubleInt(1.0, 2.0, 5, 6));   // double true, int false
    RUN(testFCCmpMixedDoubleInt(2.0, 1.0, 5, 6));   // double false, int false

    RUN(testFCCmpLessThanAndDouble(1.0, 2.0, 3.0, 4.0));  // both true
    RUN(testFCCmpLessThanAndDouble(2.0, 1.0, 3.0, 4.0));  // first false
    RUN(testFCCmpLessThanAndDouble(1.0, 2.0, 4.0, 3.0));  // second false
    RUN(testFCCmpLessThanAndDouble(2.0, 1.0, 4.0, 3.0));  // both false

    RUN(testFCCmpGreaterEqualOrDouble(2.0, 1.0, 4.0, 3.0));  // both true
    RUN(testFCCmpGreaterEqualOrDouble(2.0, 1.0, 3.0, 4.0));  // first true
    RUN(testFCCmpGreaterEqualOrDouble(1.0, 2.0, 4.0, 3.0));  // second true
    RUN(testFCCmpGreaterEqualOrDouble(1.0, 2.0, 3.0, 4.0));  // both false

    // NaN tests
    RUN(testFCCmpNaN(1.0, 1.0, std::numeric_limits<double>::quiet_NaN(), 1.0));  // second has NaN
    RUN(testFCCmpNaN(std::numeric_limits<double>::quiet_NaN(), 1.0, 1.0, 1.0));  // first has NaN
    RUN(testFCCmpNaN(std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN(), 1.0, 1.0));  // NaN == NaN is false

    // Negated fccmp
    RUN(testFCCmpNegatedAndDouble(1.0, 2.0, 3.0, 4.0));  // !(true && true) = false
    RUN(testFCCmpNegatedAndDouble(2.0, 1.0, 3.0, 4.0));  // !(false && true) = true
    RUN(testFCCmpNegatedAndDouble(1.0, 2.0, 4.0, 3.0));  // !(true && false) = true
    RUN(testFCCmpNegatedAndDouble(2.0, 1.0, 4.0, 3.0));  // !(false && false) = true

    RUN_UNARY(testSShrCompare32, int32OperandsMore());
    RUN_UNARY(testSShrCompare64, int64OperandsMore());

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
        RUN(testVectorTransposeEven());
        RUN(testVectorTransposeOdd());
        RUN(testVectorSwizzleBinaryToUnzipOdd());
        RUN(testVectorSwizzleBinaryToEXT());
        RUN(testVectorSwizzleBinaryCanonical());
        RUN(testVectorSwizzleComposition());
        RUN(testVectorSwizzleCompositionMultiUse());
        RUN(testVectorSwizzleBinaryOnlyOneSideSide0());
        RUN(testVectorSwizzleBinaryOnlyOneSideSide1());
        RUN(testVectorSwizzleBinaryOnlyOneSideSide0WithOOB());
        RUN(testVectorSwizzleBinaryOnlyOneSideSide1WithOOB());
        RUN(testVectorSwizzleBinaryOnlyOneSideAllOOB());
        RUN(testVectorSwizzleBinaryOnlyOneSideMixed());
        RUN(testVectorSwizzleBinaryOnlyOneSideSide1Scattered());
    }

    RUN(testReportUsedRegistersLateUseFollowedByEarlyDefDoesNotMarkUseAsDead());

    if (isARM64() || isX86()) {
        RUN(testVectorXorOrAllOnesToVectorAndXor());
        RUN(testVectorXorAndAllOnesToVectorOrXor());
        RUN(testVectorOrSelf());
        RUN(testVectorAndSelf());
        RUN(testVectorXorSelf());
        RUN(testVectorExtractLane0Float());
        RUN(testVectorExtractLane0Double());
        RUN(testVectorMulHigh());
        RUN(testVectorMulLow());
        RUN(testVectorMulAddLowSimple());
        RUN(testVectorMulAddLowDoubled());
        RUN(testVectorMulAddLowTwoMuls());
        RUN(testVectorMulAddLowBlaMka());
        RUN(testVectorMulAddHighSimple());
        RUN(testVectorMulAddHighDoubled());
        RUN(testVectorMulAddHighTwoMuls());
        RUN(testVectorMulAddMixedLowHigh());
        RUN(testVectorRelaxedMinMax());
        RUN(testVectorRelaxedQ15Mulr());
        RUN(testVectorRelaxedDotI8x16I7x16());
        RUN(testVectorRelaxedDotI8x16I7x16Add());
        RUN(testVectorDotProductSplatOne());
        RUN(testVectorShrZipToExtend());
        RUN(testVectorShrZipToExtendI32());
        RUN(testVectorShrZipToExtendI64());
        RUN_UNARY(testVectorXorOrAllOnesConstantToVectorAndXor, v128Operands());
        RUN_UNARY(testVectorXorAndAllOnesConstantToVectorOrXor, v128Operands());
        RUN_BINARY(testVectorOrConstants, v128Operands(), v128Operands());
        RUN_BINARY(testVectorAndConstants, v128Operands(), v128Operands());
        RUN_BINARY(testVectorXorConstants, v128Operands(), v128Operands());
        RUN_BINARY(testVectorAndConstantConstant, v128Operands(), v128Operands());
        RUN(testVectorFmulByElementFloat());
        RUN(testVectorFmulByElementDouble());
        RUN(testVectorXorRotateRight64());
        RUN(testVectorXor3());
        RUN(testVectorShlImmediate());
        RUN(testVectorShrImmediate());
        RUN(testVectorUnzipEven());
        RUN(testVectorUnzipOdd());
        RUN(testVectorZipLower());
        RUN(testVectorZipHigher());
        RUN(testVectorReverse());
        RUN(testVectorShlByOne());
        RUN(testVectorSwizzleToUnzipEven());
        RUN(testVectorSwizzleUnaryCanonical());
        RUN(testVectorExtractPair());
        RUN(testVectorSwizzleUnaryToEXT());
        RUN(testVectorCanonicalSameInputFolding());
        RUN(testVectorSwizzleToDupElement());
        RUN(testVectorSwizzleUnaryComposition());
        RUN(testVectorSwizzleCompositionRightImmOuter());
        RUN(testMulHigh32());
        RUN(testMulHigh64());
        RUN(testUMulHigh32());
        RUN(testUMulHigh64());
        RUN(testMemoryCopy());
        RUN(testMemoryFill());
        RUN(testMemoryCopyConstant());
        RUN(testMemoryFillConstant());
    }

    Lock lock;

    Vector<Ref<Thread>> threads;
    for (unsigned i = config->workerThreadCount; i--;) {
        threads.append(
            Thread::create(
                "testb3 thread"_s,
                [&] () {
                    for (;;) {
                        RefPtr<SharedTask<void()>> task;
                        {
                            Locker locker { lock };
                            if (tasks.isEmpty())
                                return;
                            task = tasks.takeFirst();
                        }

                        B3_TEST_ARENA_LIFETIME
                        task->run();
                    }
                }));
    }

    for (auto& thread : threads)
        thread->waitForCompletion();
    crashLock.lock();
    crashLock.unlock();
}

bool g_dumpB3AfterGeneration = false;

#if ENABLE(JIT_OPERATION_VALIDATION) || ENABLE(JIT_OPERATION_DISASSEMBLY)
extern const JSC::JITOperationAnnotation startOfJITOperationsInTestB3 __asm__("section$start$__DATA_CONST$__jsc_ops");
extern const JSC::JITOperationAnnotation endOfJITOperationsInTestB3 __asm__("section$end$__DATA_CONST$__jsc_ops");
#endif

int main(int argc, char** argv)
{
    TestConfig config;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-filter")) {
            if (i + 1 < argc) {
                config.filter = argv[i + 1];
                i += 1;
            } else
                usage();
        } else if (!strcmp(argv[i], "-list"))
            config.mode = TestConfig::Mode::ListTests;
        else if (!strcmp(argv[i], "-printir"))
            g_dumpB3AfterGeneration = true;
        else {
            // for backwards compatibility
            config.filter = argv[i];
            break;
        }
    }

    config.workerThreadCount = config.filter ? 1 : WTF::numberOfProcessorCores();

    JSC::Config::configureForTesting();

    WTF::initializeMainThread();
    JSC::initialize([] {
        JSC::Options::useJITCage() = false;
    });

#if ENABLE(JIT_OPERATION_VALIDATION)
    JSC::JITOperationList::populatePointersInEmbedder(&startOfJITOperationsInTestB3, &endOfJITOperationsInTestB3);
#endif
#if ENABLE(JIT_OPERATION_DISASSEMBLY)
    if (JSC::Options::needDisassemblySupport()) [[unlikely]]
        JSC::JITOperationList::populateDisassemblyLabelsInEmbedder(&startOfJITOperationsInTestB3, &endOfJITOperationsInTestB3);
#endif

    for (unsigned i = 0; i <= 2; ++i) {
        JSC::Options::defaultB3OptLevel() = i;
        run(&config);
    }

    return 0;
}

#if OS(WINDOWS)
extern "C" __declspec(dllexport) int WINAPI dllLauncherEntryPoint(int argc, const char* argv[])
{
    return main(argc, const_cast<char**>(argv));
}
#endif

#else // ENABLE(B3_JIT)

int main(int, char**)
{
    WTF::initializeMainThread();
    dataLog("B3 JIT is not enabled.\n");
}

#endif // ENABLE(B3_JIT)

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
