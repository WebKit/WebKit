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

#if ENABLE(B3_JIT) && !CPU(ARM)

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

void testLoadZeroExtendIndexAddress()
{
    if (Options::defaultB3OptLevel() < 2)
        return;

    auto test32 = [&] (uint32_t index, int32_t num, int32_t amount) {
        Procedure proc;
        BasicBlock* root = proc.addBlock();

        Value* baseValue = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
        Value* index32 = root->appendNew<Value>(
            proc, Trunc, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1));
        Value* index64 = root->appendNew<Value>(proc, ZExt32, Origin(), index32);
        Value* scale = root->appendNew<Const32Value>(proc, Origin(), amount);
        Value* address = root->appendNew<Value>(
            proc, Add, Origin(), baseValue, 
            root->appendNew<Value>(proc, Shl, Origin(), index64, scale));

        root->appendNew<Value>(
            proc, Return, Origin(), 
            root->appendNew<MemoryValue>(proc, Load, Int32, Origin(), address));

        auto code = compileProc(proc);
        if (isARM64()) {
            std::string regex(amount == 2 ? ".*ldr.*uxtw#2.*" : ".*ldr.*[.*,.*].*");
            checkUsesInstruction(*code, regex.c_str(), true);
        }
        intptr_t addr = bitwise_cast<intptr_t>(&num);
        intptr_t base = addr - (static_cast<intptr_t>(index) << static_cast<intptr_t>(amount));
        CHECK_EQ(invoke<int32_t>(*code, base, index), num);
    };

    for (auto index : int32Operands()) {
        for (auto num : int32Operands()) {
            for (int32_t amount = 0; amount < 10; ++amount)
                test32(index.value, num.value, amount);
        }
    }

    auto test64 = [&] (uint32_t index, int64_t num, int32_t amount) {
        Procedure proc;
        BasicBlock* root = proc.addBlock();

        Value* baseValue = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
        Value* index32 = root->appendNew<Value>(
            proc, Trunc, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1));
        Value* index64 = root->appendNew<Value>(proc, ZExt32, Origin(), index32);
        Value* scale = root->appendNew<Const32Value>(proc, Origin(), amount);
        Value* address = root->appendNew<Value>(
            proc, Add, Origin(), baseValue, 
            root->appendNew<Value>(proc, Shl, Origin(), index64, scale));

        root->appendNew<Value>(
            proc, Return, Origin(), 
            root->appendNew<MemoryValue>(proc, Load, Int64, Origin(), address));

        auto code = compileProc(proc);
        if (isARM64()) {
            std::string regex(amount == 3 ? ".*ldr.*uxtw#3.*" : ".*ldr.*[.*,.*].*");
            checkUsesInstruction(*code, regex.c_str(), true);
        }
        intptr_t addr = bitwise_cast<intptr_t>(&num);
        intptr_t base = addr - (static_cast<intptr_t>(index) << static_cast<intptr_t>(amount));
        CHECK_EQ(invoke<int64_t>(*code, base, index), num);
    };

    for (auto index : int32Operands()) {
        for (auto num : int64Operands()) {
            for (int32_t amount = 0; amount < 10; ++amount)
                test64(index.value, num.value, amount);
        }
    }
}

void testLoadSignExtendIndexAddress()
{
    if (Options::defaultB3OptLevel() < 2)
        return;

    auto test32 = [&] (int32_t index, int32_t num, int32_t amount) {
        Procedure proc;
        BasicBlock* root = proc.addBlock();

        Value* baseValue = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
        Value* index32 = root->appendNew<Value>(
            proc, Trunc, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1));
        Value* index64 = root->appendNew<Value>(proc, SExt32, Origin(), index32);
        Value* scale = root->appendNew<Const32Value>(proc, Origin(), amount);
        Value* address = root->appendNew<Value>(
            proc, Add, Origin(), baseValue, 
            root->appendNew<Value>(proc, Shl, Origin(), index64, scale));

        root->appendNew<Value>(
            proc, Return, Origin(), 
            root->appendNew<MemoryValue>(proc, Load, Int32, Origin(), address));

        auto code = compileProc(proc);
        if (isARM64()) {
            std::string regex(amount == 2 ? ".*ldr.*sxtw#2.*" : ".*ldr.*[.*,.*].*");
            checkUsesInstruction(*code, regex.c_str(), true);
        }
        intptr_t addr = bitwise_cast<intptr_t>(&num);
        intptr_t base = addr - (static_cast<intptr_t>(index) << static_cast<intptr_t>(amount));
        CHECK_EQ(invoke<int32_t>(*code, base, index), num);
    };

    for (auto index : int32Operands()) {
        for (auto num : int32Operands()) {
            for (int32_t amount = 0; amount < 10; ++amount)
                test32(index.value, num.value, amount);
        }
    }

    auto test64 = [&] (int32_t index, int64_t num, int32_t amount) {
        Procedure proc;
        BasicBlock* root = proc.addBlock();

        Value* baseValue = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
        Value* index32 = root->appendNew<Value>(
            proc, Trunc, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1));
        Value* index64 = root->appendNew<Value>(proc, SExt32, Origin(), index32);
        Value* scale = root->appendNew<Const32Value>(proc, Origin(), amount);
        Value* address = root->appendNew<Value>(
            proc, Add, Origin(), baseValue, 
            root->appendNew<Value>(proc, Shl, Origin(), index64, scale));

        root->appendNew<Value>(
            proc, Return, Origin(), 
            root->appendNew<MemoryValue>(proc, Load, Int64, Origin(), address));

        auto code = compileProc(proc);
        if (isARM64()) {
            std::string regex(amount == 3 ? ".*ldr.*sxtw#3.*" : ".*ldr.*[.*,.*].*");
            checkUsesInstruction(*code, regex.c_str(), true);
        }
        intptr_t addr = bitwise_cast<intptr_t>(&num);
        intptr_t base = addr - (static_cast<intptr_t>(index) << static_cast<intptr_t>(amount));
        CHECK_EQ(invoke<int64_t>(*code, base, index), num);
    };

    for (auto index : int32Operands()) {
        for (auto num : int64Operands()) {
            for (int32_t amount = 0; amount < 10; ++amount)
                test64(index.value, num.value, amount);
        }
    }
}

void testStoreZeroExtendIndexAddress()
{
    if (Options::defaultB3OptLevel() < 2)
        return;

    auto test32 = [&] (uint32_t index, int32_t num, int32_t amount) {
        Procedure proc;
        BasicBlock* root = proc.addBlock();

        Value* value = root->appendNew<Const32Value>(proc, Origin(), num);
        Value* base = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
        Value* index32 = root->appendNew<Value>(
            proc, Trunc, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1));
        Value* index64 = root->appendNew<Value>(proc, ZExt32, Origin(), index32);
        Value* scale = root->appendNew<Const32Value>(proc, Origin(), amount);
        Value* address = root->appendNew<Value>(
            proc, Add, Origin(), base, 
            root->appendNew<Value>(proc, Shl, Origin(), index64, scale));

        root->appendNew<MemoryValue>(proc, Store, Origin(), value, address);
        root->appendNew<Value>(proc, Return, Origin(), value);

        auto code = compileProc(proc);
        if (isARM64()) {
            std::string regex(amount == 2 ? ".*str.*uxtw#2.*" : ".*str.*[.*,.*].*");
            checkUsesInstruction(*code, regex.c_str(), true);
        }
        int32_t slot = 12341234;
        intptr_t addr = bitwise_cast<intptr_t>(&slot);
        invoke<int32_t>(*code, addr - (static_cast<intptr_t>(index) << static_cast<intptr_t>(amount)), index);
        CHECK_EQ(slot, num);
    };

    for (auto index : int32Operands()) {
        for (auto num : int32Operands()) {
            for (int32_t amount = 0; amount < 10; ++amount)
                test32(index.value, num.value, amount);
        }
    }

    auto test64 = [&] (uint32_t index, int64_t num, int32_t amount) {
        Procedure proc;
        BasicBlock* root = proc.addBlock();

        Value* value = root->appendNew<Const64Value>(proc, Origin(), num);
        Value* base = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
        Value* index32 = root->appendNew<Value>(
            proc, Trunc, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1));
        Value* index64 = root->appendNew<Value>(proc, ZExt32, Origin(), index32);
        Value* scale = root->appendNew<Const32Value>(proc, Origin(), amount);
        Value* address = root->appendNew<Value>(
            proc, Add, Origin(), base, 
            root->appendNew<Value>(proc, Shl, Origin(), index64, scale));

        root->appendNew<MemoryValue>(proc, Store, Origin(), value, address);
        root->appendNew<Value>(proc, Return, Origin(), value);

        auto code = compileProc(proc);
        if (isARM64() && amount == 3) {
            std::string regex(amount == 3 ? ".*str.*uxtw#3.*" : ".*str.*[.*,.*].*");
            checkUsesInstruction(*code, regex.c_str(), true);
        }
        int64_t slot = 12341234;
        intptr_t addr = bitwise_cast<intptr_t>(&slot);
        invoke<int64_t>(*code, addr - (static_cast<intptr_t>(index) << static_cast<intptr_t>(amount)), index);
        CHECK_EQ(slot, num);
    };

    for (auto index : int32Operands()) {
        for (auto num : int64Operands()) {
            for (int32_t amount = 0; amount < 10; ++amount)
                test64(index.value, num.value, amount);
        }
    }
}

void testStoreSignExtendIndexAddress()
{
    if (Options::defaultB3OptLevel() < 2)
        return;

    auto test32 = [&] (int32_t index, int32_t num, int32_t amount) {
        Procedure proc;
        BasicBlock* root = proc.addBlock();

        Value* value = root->appendNew<Const32Value>(proc, Origin(), num);
        Value* base = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
        Value* index32 = root->appendNew<Value>(
            proc, Trunc, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1));
        Value* index64 = root->appendNew<Value>(proc, SExt32, Origin(), index32);
        Value* scale = root->appendNew<Const32Value>(proc, Origin(), amount);
        Value* address = root->appendNew<Value>(
            proc, Add, Origin(), base, 
            root->appendNew<Value>(proc, Shl, Origin(), index64, scale));

        root->appendNew<MemoryValue>(proc, Store, Origin(), value, address);
        root->appendNew<Value>(proc, Return, Origin(), value);

        auto code = compileProc(proc);
        if (isARM64()) {
            std::string regex(amount == 2 ? ".*str.*sxtw#2.*" : ".*str.*[.*,.*].*");
            checkUsesInstruction(*code, regex.c_str(), true);
        }
        int32_t slot = 12341234;
        intptr_t addr = bitwise_cast<intptr_t>(&slot);
        invoke<int32_t>(*code, addr - (static_cast<intptr_t>(index) << static_cast<intptr_t>(amount)), index);
        CHECK_EQ(slot, num);
    };

    for (auto index : int32Operands()) {
        for (auto num : int32Operands()) {
            for (int32_t amount = 0; amount < 10; ++amount)
                test32(index.value, num.value, amount);
        }
    }

    auto test64 = [&] (int32_t index, int64_t num, int32_t amount) {
        Procedure proc;
        BasicBlock* root = proc.addBlock();

        Value* value = root->appendNew<Const64Value>(proc, Origin(), num);
        Value* base = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
        Value* index32 = root->appendNew<Value>(
            proc, Trunc, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1));
        Value* index64 = root->appendNew<Value>(proc, SExt32, Origin(), index32);
        Value* scale = root->appendNew<Const32Value>(proc, Origin(), amount);
        Value* address = root->appendNew<Value>(
            proc, Add, Origin(), base, 
            root->appendNew<Value>(proc, Shl, Origin(), index64, scale));

        root->appendNew<MemoryValue>(proc, Store, Origin(), value, address);
        root->appendNew<Value>(proc, Return, Origin(), value);

        auto code = compileProc(proc);
        if (isARM64()) {
            std::string regex(amount == 3 ? ".*str.*sxtw#3.*" : ".*str.*[.*,.*].*");
            checkUsesInstruction(*code, regex.c_str(), true);
        }
        int64_t slot = 12341234;
        intptr_t addr = bitwise_cast<intptr_t>(&slot);
        invoke<int64_t>(*code, addr - (static_cast<intptr_t>(index) << static_cast<intptr_t>(amount)), index);
        CHECK_EQ(slot, num);
    };

    for (auto index : int32Operands()) {
        for (auto num : int64Operands()) {
            for (int32_t amount = 0; amount < 10; ++amount)
                test64(index.value, num.value, amount);
        }
    }
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

    auto code = compileProc(proc);
    if (isARM64())
        checkUsesInstruction(*code, "madd");
    CHECK(invoke<int64_t>(*code, a, a, a) == a * a + a);
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

void testMulArgs32SignExtend()
{
    Procedure proc;
    if (proc.optLevel() < 2)
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
    if (isARM64())
        checkUsesInstruction(*code, "smull");

    for (auto nOperand : int32Operands()) {
        for (auto mOperand : int32Operands()) {
            int32_t n = nOperand.value;
            int32_t m = mOperand.value;
            CHECK_EQ(invoke<int64_t>(*code, n, m), static_cast<int64_t>(n) * static_cast<int64_t>(m));
        }
    }
}

void testMulArgs32ZeroExtend()
{
    Procedure proc;
    if (proc.optLevel() < 2)
        return;
    BasicBlock* root = proc.addBlock();
    Value* arg1 = root->appendNew<Value>(
        proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* arg2 = root->appendNew<Value>(
        proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1));
    Value* left = root->appendNew<Value>(proc, ZExt32, Origin(), arg1);
    Value* right = root->appendNew<Value>(proc, ZExt32, Origin(), arg2);
    Value* mul = root->appendNew<Value>(proc, Mul, Origin(), left, right);
    root->appendNewControlValue(proc, Return, Origin(), mul);

    auto code = compileProc(proc);
    if (isARM64())
        checkUsesInstruction(*code, "umull");

    for (auto nOperand : int32Operands()) {
        for (auto mOperand : int32Operands()) {
            uint32_t n = nOperand.value;
            uint32_t m = mOperand.value;
            CHECK_EQ(invoke<uint64_t>(*code, n, m), static_cast<uint64_t>(n) * static_cast<uint64_t>(m));
        }
    }
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
    if (isARM64())
        checkUsesInstruction(*code, "madd");

    auto testValues = int64Operands();
    for (auto a : testValues) {
        for (auto b : testValues) {
            for (auto c : testValues)
                CHECK_EQ(invoke<int64_t>(*code, a.value, b.value, c.value), a.value * b.value + c.value);
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
    if (isARM64())
        checkUsesInstruction(*code, "madd");

    auto testValues = int64Operands();
    for (auto a : testValues) {
        for (auto b : testValues) {
            for (auto c : testValues)
                CHECK_EQ(invoke<int64_t>(*code, a.value, b.value, c.value), a.value + b.value * c.value);
        }
    }
}

void testMulAddSignExtend32ArgsLeft()
{
    // d = SExt32(n) * SExt32(m) + a
    Procedure proc;
    BasicBlock* root = proc.addBlock();

    Value* nValue = root->appendNew<Value>(
        proc, SExt32, Origin(),
        root->appendNew<Value>(
            proc, Trunc, Origin(), 
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)));
    Value* mValue = root->appendNew<Value>(
        proc, SExt32, Origin(),
        root->appendNew<Value>(
            proc, Trunc, Origin(), 
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1)));
    Value* aValue = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR2);

    Value* mulValue = root->appendNew<Value>(proc, Mul, Origin(), nValue, mValue);
    Value* addValue = root->appendNew<Value>(proc, Add, Origin(), mulValue, aValue);
    root->appendNewControlValue(proc, Return, Origin(), addValue);

    auto code = compileProc(proc);
    if (isARM64())
        checkUsesInstruction(*code, "smaddl");

    for (auto n : int32Operands()) {
        for (auto m : int32Operands()) {
            for (auto a : int64Operands()) {
                int64_t lhs = invoke<int64_t>(*code, n.value, m.value, a.value);
                int64_t rhs = static_cast<int64_t>(n.value) * static_cast<int64_t>(m.value) + a.value;
                CHECK(lhs == rhs);
            }
        }
    }
}

void testMulAddSignExtend32ArgsRight()
{
    // d = a + SExt32(n) * SExt32(m)
    Procedure proc;
    BasicBlock* root = proc.addBlock();

    Value* aValue = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    Value* nValue = root->appendNew<Value>(
        proc, SExt32, Origin(),
        root->appendNew<Value>(
            proc, Trunc, Origin(), 
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1)));
    Value* mValue = root->appendNew<Value>(
        proc, SExt32, Origin(),
        root->appendNew<Value>(
            proc, Trunc, Origin(), 
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR2)));

    Value* mulValue = root->appendNew<Value>(proc, Mul, Origin(), nValue, mValue);
    Value* addValue = root->appendNew<Value>(proc, Add, Origin(), aValue, mulValue);
    root->appendNewControlValue(proc, Return, Origin(), addValue);

    auto code = compileProc(proc);
    if (isARM64())
        checkUsesInstruction(*code, "smaddl");

    for (auto a : int64Operands()) {
        for (auto n : int32Operands()) {
            for (auto m : int32Operands()) {
                int64_t lhs = invoke<int64_t>(*code, a.value, n.value, m.value);
                int64_t rhs = a.value + static_cast<int64_t>(n.value) * static_cast<int64_t>(m.value);
                CHECK(lhs == rhs);
            }
        }
    }
}

void testMulAddZeroExtend32ArgsLeft()
{
    // d = ZExt32(n) * ZExt32(m) + a
    Procedure proc;
    BasicBlock* root = proc.addBlock();

    Value* nValue = root->appendNew<Value>(
        proc, ZExt32, Origin(),
        root->appendNew<Value>(
            proc, Trunc, Origin(), 
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)));
    Value* mValue = root->appendNew<Value>(
        proc, ZExt32, Origin(),
        root->appendNew<Value>(
            proc, Trunc, Origin(), 
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1)));
    Value* aValue = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR2);

    Value* mulValue = root->appendNew<Value>(proc, Mul, Origin(), nValue, mValue);
    Value* addValue = root->appendNew<Value>(proc, Add, Origin(), mulValue, aValue);
    root->appendNewControlValue(proc, Return, Origin(), addValue);

    auto code = compileProc(proc);
    if (isARM64())
        checkUsesInstruction(*code, "umaddl");

    for (auto n : int32Operands()) {
        for (auto m : int32Operands()) {
            for (auto a : int64Operands()) {
                uint32_t un = n.value;
                uint32_t um = m.value;
                int64_t lhs = invoke<int64_t>(*code, un, um, a.value);
                int64_t rhs = static_cast<int64_t>(un) * static_cast<int64_t>(um) + a.value;
                CHECK(lhs == rhs);
            }
        }
    }
}

void testMulAddZeroExtend32ArgsRight()
{
    // d = a + ZExt32(n) * ZExt32(m)
    Procedure proc;
    BasicBlock* root = proc.addBlock();

    Value* aValue = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    Value* nValue = root->appendNew<Value>(
        proc, ZExt32, Origin(),
        root->appendNew<Value>(
            proc, Trunc, Origin(), 
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1)));
    Value* mValue = root->appendNew<Value>(
        proc, ZExt32, Origin(),
        root->appendNew<Value>(
            proc, Trunc, Origin(), 
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR2)));

    Value* mulValue = root->appendNew<Value>(proc, Mul, Origin(), nValue, mValue);
    Value* addValue = root->appendNew<Value>(proc, Add, Origin(), aValue, mulValue);
    root->appendNewControlValue(proc, Return, Origin(), addValue);

    auto code = compileProc(proc);
    if (isARM64())
        checkUsesInstruction(*code, "umaddl");

    for (auto a : int64Operands()) {
        for (auto n : int32Operands()) {
            for (auto m : int32Operands()) {
                uint32_t un = n.value;
                uint32_t um = m.value;
                int64_t lhs = invoke<int64_t>(*code, a.value, un, um);
                int64_t rhs = a.value + static_cast<int64_t>(un) * static_cast<int64_t>(um);
                CHECK(lhs == rhs);
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
    if (isARM64())
        checkUsesInstruction(*code, "madd");

    auto testValues = int32Operands();
    for (auto a : testValues) {
        for (auto b : testValues) {
            for (auto c : testValues)
                CHECK(invoke<int32_t>(*code, a.value, b.value, c.value) == a.value * b.value + c.value);
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
    if (isARM64())
        checkUsesInstruction(*code, "madd");

    auto testValues = int32Operands();
    for (auto a : testValues) {
        for (auto b : testValues) {
            for (auto c : testValues)
                CHECK(invoke<int32_t>(*code, a.value, b.value, c.value) == a.value + b.value * c.value);
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
    Value* subtracted = root->appendNew<Value>(proc, Sub, Origin(), multiplied, arg2);
    root->appendNewControlValue(proc, Return, Origin(), subtracted);

    auto code = compileProc(proc);
    if (isARM64())
        checkDoesNotUseInstruction(*code, "msub");

    auto testValues = int64Operands();
    for (auto a : testValues) {
        for (auto b : testValues) {
            for (auto c : testValues)
                CHECK_EQ(invoke<int64_t>(*code, a.value, b.value, c.value), a.value * b.value - c.value);
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
    Value* subtracted = root->appendNew<Value>(proc, Sub, Origin(), arg0, multiplied);
    root->appendNewControlValue(proc, Return, Origin(), subtracted);

    auto code = compileProc(proc);
    if (isARM64())
        checkUsesInstruction(*code, "msub");

    auto testValues = int64Operands();
    for (auto a : testValues) {
        for (auto b : testValues) {
            for (auto c : testValues)
                CHECK_EQ(invoke<int64_t>(*code, a.value, b.value, c.value), a.value - b.value * c.value);
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
    Value* subtracted = root->appendNew<Value>(proc, Sub, Origin(), multiplied, arg2);
    root->appendNewControlValue(proc, Return, Origin(), subtracted);

    auto code = compileProc(proc);
    if (isARM64())
        checkDoesNotUseInstruction(*code, "msub");

    auto testValues = int32Operands();
    for (auto a : testValues) {
        for (auto b : testValues) {
            for (auto c : testValues)
                CHECK(invoke<int32_t>(*code, a.value, b.value, c.value) == a.value * b.value - c.value);
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
    Value* subtracted = root->appendNew<Value>(proc, Sub, Origin(), arg0, multiplied);
    root->appendNewControlValue(proc, Return, Origin(), subtracted);

    auto code = compileProc(proc);
    if (isARM64())
        checkUsesInstruction(*code, "msub");

    auto testValues = int32Operands();
    for (auto a : testValues) {
        for (auto b : testValues) {
            for (auto c : testValues)
                CHECK_EQ(invoke<int32_t>(*code, a.value, b.value, c.value), a.value - b.value * c.value);
        }
    }
}

void testMulSubSignExtend32()
{
    // d = a - SExt32(n) * SExt32(m)
    Procedure proc;
    BasicBlock* root = proc.addBlock();

    Value* aValue = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    Value* nValue = root->appendNew<Value>(
        proc, SExt32, Origin(),
        root->appendNew<Value>(
            proc, Trunc, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1)));
    Value* mValue = root->appendNew<Value>(
        proc, SExt32, Origin(),
        root->appendNew<Value>(
            proc, Trunc, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR2)));

    Value* mulValue = root->appendNew<Value>(proc, Mul, Origin(), nValue, mValue);
    Value* subValue = root->appendNew<Value>(proc, Sub, Origin(), aValue, mulValue);
    root->appendNewControlValue(proc, Return, Origin(), subValue);

    auto code = compileProc(proc);
    if (isARM64())
        checkUsesInstruction(*code, "smsubl");

    for (auto a : int64Operands()) {
        for (auto n : int32Operands()) {
            for (auto m : int32Operands()) {
                int64_t lhs = invoke<int64_t>(*code, a.value, n.value, m.value);
                int64_t rhs = a.value - static_cast<int64_t>(n.value) * static_cast<int64_t>(m.value);
                CHECK(lhs == rhs);
            }
        }
    }
}

void testMulSubZeroExtend32()
{
    // d = a - ZExt32(n) * ZExt32(m)
    Procedure proc;
    BasicBlock* root = proc.addBlock();

    Value* aValue = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    Value* nValue = root->appendNew<Value>(
        proc, ZExt32, Origin(),
        root->appendNew<Value>(
            proc, Trunc, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1)));
    Value* mValue = root->appendNew<Value>(
        proc, ZExt32, Origin(),
        root->appendNew<Value>(
            proc, Trunc, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR2)));

    Value* mulValue = root->appendNew<Value>(proc, Mul, Origin(), nValue, mValue);
    Value* subValue = root->appendNew<Value>(proc, Sub, Origin(), aValue, mulValue);
    root->appendNewControlValue(proc, Return, Origin(), subValue);

    auto code = compileProc(proc);
    if (isARM64())
        checkUsesInstruction(*code, "umsubl");

    for (auto a : int64Operands()) {
        for (auto n : int32Operands()) {
            for (auto m : int32Operands()) {
                uint32_t un = n.value;
                uint32_t um = m.value;
                int64_t lhs = invoke<int64_t>(*code, a.value, un, um);
                int64_t rhs = a.value - static_cast<int64_t>(un) * static_cast<int64_t>(um);
                CHECK(lhs == rhs);
            }
        }
    }
}

void testMulNegArgArg(int32_t a, int32_t b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();

    Value* argA = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    Value* argB = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
    Value* negA = root->appendNew<Value>(proc, Neg, Origin(), argA);
    Value* result = root->appendNew<Value>(proc, Mul, Origin(), negA, argB);
    root->appendNew<Value>(proc, Return, Origin(), result);

    auto code = compileProc(proc);
    if (isARM64() && JSC::Options::defaultB3OptLevel() > 1)
        checkUsesInstruction(*code, "mneg");
    CHECK_EQ(invoke<int32_t>(*code, a, b), (-a) * b);
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
    if (isARM64() && JSC::Options::defaultB3OptLevel() > 1)
        checkUsesInstruction(*code, "mneg");

    auto testValues = int64Operands();
    for (auto a : testValues) {
        for (auto b : testValues) {
            CHECK_EQ(invoke<int64_t>(*code, a.value, b.value), -(a.value * b.value));
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
    if (isARM64() && JSC::Options::defaultB3OptLevel() > 1)
        checkUsesInstruction(*code, "mneg");

    auto testValues = int32Operands();
    for (auto a : testValues) {
        for (auto b : testValues)
            CHECK_EQ(invoke<int32_t>(*code, a.value, b.value), -(a.value * b.value));
    }
}

void testMulNegSignExtend32()
{
    // d = - (SExt32(n) * SExt32(m))
    Procedure proc;
    BasicBlock* root = proc.addBlock();

    Value* nValue = root->appendNew<Value>(
        proc, SExt32, Origin(),
        root->appendNew<Value>(
            proc, Trunc, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)));
    Value* mValue = root->appendNew<Value>(
        proc, SExt32, Origin(),
        root->appendNew<Value>(
            proc, Trunc, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1)));

    Value* mulValue = root->appendNew<Value>(proc, Mul, Origin(), nValue, mValue);
    Value* negValue = root->appendNew<Value>(proc, Neg, Origin(), mulValue);
    root->appendNewControlValue(proc, Return, Origin(), negValue);

    auto code = compileProc(proc);
    if (isARM64())
        checkUsesInstruction(*code, "smnegl");

    for (auto n : int32Operands()) {
        for (auto m : int32Operands()) {
            int64_t lhs = invoke<int64_t>(*code, n.value, m.value);
            int64_t rhs = -(static_cast<int64_t>(n.value) * static_cast<int64_t>(m.value));
            CHECK(lhs == rhs);
        }
    }
}

void testMulNegZeroExtend32()
{
    // d = - (ZExt32(n) * ZExt32(m))
    Procedure proc;
    BasicBlock* root = proc.addBlock();

    Value* nValue = root->appendNew<Value>(
        proc, ZExt32, Origin(),
        root->appendNew<Value>(
            proc, Trunc, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)));
    Value* mValue = root->appendNew<Value>(
        proc, ZExt32, Origin(),
        root->appendNew<Value>(
            proc, Trunc, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1)));

    Value* mulValue = root->appendNew<Value>(proc, Mul, Origin(), nValue, mValue);
    Value* negValue = root->appendNew<Value>(proc, Neg, Origin(), mulValue);
    root->appendNewControlValue(proc, Return, Origin(), negValue);

    auto code = compileProc(proc);
    if (isARM64())
        checkUsesInstruction(*code, "umnegl");

    for (auto n : int32Operands()) {
        for (auto m : int32Operands()) {
            uint32_t un = n.value;
            uint32_t um = m.value;
            int64_t lhs = invoke<int64_t>(*code, un, um);
            int64_t rhs = -(static_cast<int64_t>(un) * static_cast<int64_t>(um));
            CHECK(lhs == rhs);
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

void testSubArgs32ZeroExtend(int a, int b)
{
    Procedure proc;
    if (proc.optLevel() < 1)
        return;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, ZExt32, Origin(),
            root->appendNew<Value>(
                proc, Sub, Origin(),
                root->appendNew<Value>(
                    proc, Trunc, Origin(),
                    root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)),
                root->appendNew<Value>(
                    proc, Trunc, Origin(),
                    root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1)))));

    CHECK(compileAndRun<uint64_t>(proc, a, b) == static_cast<uint64_t>(static_cast<uint32_t>(a - b)));
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

void testUbfx32ShiftAnd()
{
    // Test Pattern: (src >> lsb) & mask
    // where: mask = (1 << width) - 1
    if (JSC::Options::defaultB3OptLevel() < 2)
        return;
    uint32_t src = 0xffffffff;
    Vector<uint32_t> lsbs = { 1, 14, 30 };
    Vector<uint32_t> widths = { 30, 17, 1 };

    auto test = [&] (uint32_t lsb, uint32_t mask) -> uint32_t {
        Procedure proc;
        BasicBlock* root = proc.addBlock();

        Value* srcValue = root->appendNew<Value>(
            proc, Trunc, Origin(), 
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
        Value* lsbValue = root->appendNew<Const32Value>(proc, Origin(), lsb);
        Value* maskValue = root->appendNew<Const32Value>(proc, Origin(), mask);

        Value* left = root->appendNew<Value>(proc, ZShr, Origin(), srcValue, lsbValue);
        root->appendNewControlValue(
            proc, Return, Origin(),
            root->appendNew<Value>(proc, BitAnd, Origin(), left, maskValue));

        auto code = compileProc(proc);
        if (isARM64())
            checkUsesInstruction(*code, "ubfx");
        return invoke<uint32_t>(*code, src);
    };

    auto generateMask = [&] (uint32_t width) -> uint32_t {
        return (1U << width) - 1U;
    };

    for (size_t i = 0; i < lsbs.size(); ++i) {
        uint32_t lsb = lsbs.at(i);
        uint32_t mask = generateMask(widths.at(i));
        CHECK(test(lsb, mask) == ((src >> lsb) & mask));
    }
}

void testUbfx32AndShift()
{
    // Test Pattern: mask & (src >> lsb)
    // Where: mask = (1 << width) - 1
    if (JSC::Options::defaultB3OptLevel() < 2)
        return;
    uint32_t src = 0xffffffff;
    Vector<uint32_t> lsbs = { 1, 14, 30 };
    Vector<uint32_t> widths = { 30, 17, 1 };

    auto test = [&] (uint32_t lsb, uint32_t mask) -> uint32_t {
        Procedure proc;
        BasicBlock* root = proc.addBlock();

        Value* srcValue = root->appendNew<Value>(
            proc, Trunc, Origin(), 
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
        Value* lsbValue = root->appendNew<Const32Value>(proc, Origin(), lsb);
        Value* maskValue = root->appendNew<Const32Value>(proc, Origin(), mask);

        Value* right = root->appendNew<Value>(proc, ZShr, Origin(), srcValue, lsbValue);
        root->appendNewControlValue(
            proc, Return, Origin(),
            root->appendNew<Value>(proc, BitAnd, Origin(), maskValue, right));

        auto code = compileProc(proc);
        if (isARM64())
            checkUsesInstruction(*code, "ubfx");
        return invoke<uint32_t>(*code, src);
    };

    auto generateMask = [&] (uint32_t width) -> uint32_t {
        return (1U << width) - 1U;
    };

    for (size_t i = 0; i < lsbs.size(); ++i) {
        uint32_t lsb = lsbs.at(i);
        uint32_t mask = generateMask(widths.at(i));
        CHECK(test(lsb, mask) == (mask & (src >> lsb)));
    }
}

void testUbfx64ShiftAnd()
{
    // Test Pattern: (src >> lsb) & mask
    // where: mask = (1 << width) - 1
    if (JSC::Options::defaultB3OptLevel() < 2)
        return;
    uint64_t src = 0xffffffffffffffff;
    Vector<uint64_t> lsbs = { 1, 30, 62 };
    Vector<uint64_t> widths = { 62, 33, 1 };

    auto test = [&] (uint64_t lsb, uint64_t mask) -> uint64_t {
        Procedure proc;
        BasicBlock* root = proc.addBlock();

        Value* srcValue = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
        Value* lsbValue = root->appendNew<Const32Value>(proc, Origin(), lsb);
        Value* maskValue = root->appendNew<Const64Value>(proc, Origin(), mask);

        Value* left = root->appendNew<Value>(proc, ZShr, Origin(), srcValue, lsbValue);
        root->appendNewControlValue(
            proc, Return, Origin(),
            root->appendNew<Value>(proc, BitAnd, Origin(), left, maskValue));

        auto code = compileProc(proc);
        if (isARM64())
            checkUsesInstruction(*code, "ubfx");
        return invoke<uint64_t>(*code, src);
    };

    auto generateMask = [&] (uint64_t width) -> uint64_t {
        return (1ULL << width) - 1ULL;
    };

    for (size_t i = 0; i < lsbs.size(); ++i) {
        uint64_t lsb = lsbs.at(i);
        uint64_t mask = generateMask(widths.at(i));
        CHECK(test(lsb, mask) == ((src >> lsb) & mask));
    }
}

void testUbfx64AndShift()
{
    // Test Pattern: mask & (src >> lsb)
    // Where: mask = (1 << width) - 1
    if (JSC::Options::defaultB3OptLevel() < 2)
        return;
    uint64_t src = 0xffffffffffffffff;
    Vector<uint64_t> lsbs = { 1, 30, 62 };
    Vector<uint64_t> widths = { 62, 33, 1 };

    auto test = [&] (uint64_t lsb, uint64_t mask) -> uint64_t {
        Procedure proc;
        BasicBlock* root = proc.addBlock();

        Value* srcValue = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
        Value* lsbValue = root->appendNew<Const32Value>(proc, Origin(), lsb);
        Value* maskValue = root->appendNew<Const64Value>(proc, Origin(), mask);

        Value* right = root->appendNew<Value>(proc, ZShr, Origin(), srcValue, lsbValue);
        root->appendNewControlValue(
            proc, Return, Origin(),
            root->appendNew<Value>(proc, BitAnd, Origin(), maskValue, right));

        auto code = compileProc(proc);
        if (isARM64())
            checkUsesInstruction(*code, "ubfx");
        return invoke<uint64_t>(*code, src);
    };

    auto generateMask = [&] (uint64_t width) -> uint64_t {
        return (1ULL << width) - 1ULL;
    };

    for (size_t i = 0; i < lsbs.size(); ++i) {
        uint64_t lsb = lsbs.at(i);
        uint64_t mask = generateMask(widths.at(i));
        CHECK(test(lsb, mask) == (mask & (src >> lsb)));
    }
}

void testUbfiz32AndShiftValueMask()
{
    // Test Pattern: d = (n & mask) << lsb 
    // Where: mask = (1 << width) - 1
    if (JSC::Options::defaultB3OptLevel() < 2)
        return;
    uint32_t n = 0xffffffff;
    Vector<uint32_t> lsbs = { 1, 14, 30 };
    Vector<uint32_t> widths = { 30, 17, 1 };

    auto test = [&] (uint32_t lsb, uint32_t mask) -> uint32_t {
        Procedure proc;
        BasicBlock* root = proc.addBlock();

        Value* nValue = root->appendNew<Value>(
            proc, Trunc, Origin(), 
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
        Value* lsbValue = root->appendNew<Const32Value>(proc, Origin(), lsb);
        Value* maskValue = root->appendNew<Const32Value>(proc, Origin(), mask);

        Value* left = root->appendNew<Value>(proc, BitAnd, Origin(), nValue, maskValue);
        root->appendNewControlValue(
            proc, Return, Origin(),
            root->appendNew<Value>(proc, Shl, Origin(), left, lsbValue));

        auto code = compileProc(proc);
        if (isARM64())
            checkUsesInstruction(*code, "ubfiz");
        return invoke<uint32_t>(*code, n);
    };

    auto generateMask = [&] (uint32_t width) -> uint32_t {
        return (1U << width) - 1U;
    };

    for (size_t i = 0; i < lsbs.size(); ++i) {
        uint32_t lsb = lsbs.at(i);
        uint32_t mask = generateMask(widths.at(i));
        CHECK(test(lsb, mask) == ((n & mask) << lsb));
    }
}

void testUbfiz32AndShiftMaskValue()
{
    // Test Pattern: d = (mask & n) << lsb 
    // Where: mask = (1 << width) - 1
    if (JSC::Options::defaultB3OptLevel() < 2)
        return;
    uint32_t n = 0xffffffff;
    Vector<uint32_t> lsbs = { 1, 14, 30 };
    Vector<uint32_t> widths = { 30, 17, 1 };

    auto test = [&] (uint32_t lsb, uint32_t mask) -> uint32_t {
        Procedure proc;
        BasicBlock* root = proc.addBlock();

        Value* nValue = root->appendNew<Value>(
            proc, Trunc, Origin(), 
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
        Value* lsbValue = root->appendNew<Const32Value>(proc, Origin(), lsb);
        Value* maskValue = root->appendNew<Const32Value>(proc, Origin(), mask);

        Value* left = root->appendNew<Value>(proc, BitAnd, Origin(), maskValue, nValue);
        root->appendNewControlValue(
            proc, Return, Origin(),
            root->appendNew<Value>(proc, Shl, Origin(), left, lsbValue));

        auto code = compileProc(proc);
        if (isARM64())
            checkUsesInstruction(*code, "ubfiz");
        return invoke<uint32_t>(*code, n);
    };

    auto generateMask = [&] (uint32_t width) -> uint32_t {
        return (1U << width) - 1U;
    };

    for (size_t i = 0; i < lsbs.size(); ++i) {
        uint32_t lsb = lsbs.at(i);
        uint32_t mask = generateMask(widths.at(i));
        CHECK(test(lsb, mask) == ((mask & n) << lsb));
    }
}

void testUbfiz32ShiftAnd()
{
    // Test Pattern: d = (n << lsb) & maskShift
    // Where: maskShift = mask << lsb
    //        mask = (1 << width) - 1
    if (JSC::Options::defaultB3OptLevel() < 2)
        return;
    uint32_t n = 0xffffffff;
    Vector<uint32_t> lsbs = { 1, 14, 30 };
    Vector<uint32_t> widths = { 30, 17, 1 };

    auto test = [&] (uint32_t lsb, uint32_t maskShift) -> uint32_t {
        Procedure proc;
        BasicBlock* root = proc.addBlock();

        Value* nValue = root->appendNew<Value>(
            proc, Trunc, Origin(), 
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
        Value* lsbValue = root->appendNew<Const32Value>(proc, Origin(), lsb);
        Value* maskShiftValue = root->appendNew<Const32Value>(proc, Origin(), maskShift);

        Value* left = root->appendNew<Value>(proc, Shl, Origin(), nValue, lsbValue);
        root->appendNewControlValue(
            proc, Return, Origin(),
            root->appendNew<Value>(proc, BitAnd, Origin(), left, maskShiftValue));

        auto code = compileProc(proc);
        if (isARM64())
            checkUsesInstruction(*code, "ubfiz");
        return invoke<uint32_t>(*code, n);
    };

    auto generateMaskShift = [&] (uint32_t width, uint32_t lsb) -> uint32_t {
        return ((1U << width) - 1U) << lsb;
    };

    for (size_t i = 0; i < lsbs.size(); ++i) {
        uint32_t lsb = lsbs.at(i);
        uint32_t maskShift = generateMaskShift(widths.at(i), lsb);
        CHECK(test(lsb, maskShift) == ((n << lsb) & maskShift));
    }
}

void testUbfiz32AndShift()
{
    // Test Pattern: d = maskShift & (n << lsb)
    // Where: maskShift = mask << lsb
    //        mask = (1 << width) - 1
    if (JSC::Options::defaultB3OptLevel() < 2)
        return;
    uint32_t n = 0xffffffff;
    Vector<uint32_t> lsbs = { 1, 14, 30 };
    Vector<uint32_t> widths = { 30, 17, 1 };

    auto test = [&] (uint32_t lsb, uint32_t maskShift) -> uint32_t {
        Procedure proc;
        BasicBlock* root = proc.addBlock();

        Value* nValue = root->appendNew<Value>(
            proc, Trunc, Origin(), 
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
        Value* lsbValue = root->appendNew<Const32Value>(proc, Origin(), lsb);
        Value* maskShiftValue = root->appendNew<Const32Value>(proc, Origin(), maskShift);

        Value* right = root->appendNew<Value>(proc, Shl, Origin(), nValue, lsbValue);
        root->appendNewControlValue(
            proc, Return, Origin(),
            root->appendNew<Value>(proc, BitAnd, Origin(), maskShiftValue, right));

        auto code = compileProc(proc);
        if (isARM64())
            checkUsesInstruction(*code, "ubfiz");
        return invoke<uint32_t>(*code, n);
    };

    auto generateMaskShift = [&] (uint32_t width, uint32_t lsb) -> uint32_t {
        return ((1U << width) - 1U) << lsb;
    };

    for (size_t i = 0; i < lsbs.size(); ++i) {
        uint32_t lsb = lsbs.at(i);
        uint32_t maskShift = generateMaskShift(widths.at(i), lsb);
        CHECK(test(lsb, maskShift) == (maskShift & (n << lsb)));
    }
}

void testUbfiz64AndShiftValueMask()
{
    // Test Pattern: d = (n & mask) << lsb 
    // Where: mask = (1 << width) - 1
    if (JSC::Options::defaultB3OptLevel() < 2)
        return;
    uint64_t n = 0xffffffffffffffff;
    Vector<uint64_t> lsbs = { 1, 30, 62 };
    Vector<uint64_t> widths = { 62, 33, 1 };

    auto test = [&] (uint64_t lsb, uint64_t mask) -> uint64_t {
        Procedure proc;
        BasicBlock* root = proc.addBlock();

        Value* nValue = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
        Value* lsbValue = root->appendNew<Const32Value>(proc, Origin(), lsb);
        Value* maskValue = root->appendNew<Const64Value>(proc, Origin(), mask);

        Value* left = root->appendNew<Value>(proc, BitAnd, Origin(), nValue, maskValue);
        root->appendNewControlValue(
            proc, Return, Origin(),
            root->appendNew<Value>(proc, Shl, Origin(), left, lsbValue));

        auto code = compileProc(proc);
        if (isARM64())
            checkUsesInstruction(*code, "ubfiz");
        return invoke<uint64_t>(*code, n);
    };

    auto generateMask = [&] (uint64_t width) -> uint64_t {
        return (1ULL << width) - 1ULL;
    };

    for (size_t i = 0; i < lsbs.size(); ++i) {
        uint64_t lsb = lsbs.at(i);
        uint64_t mask = generateMask(widths.at(i));
        CHECK(test(lsb, mask) == ((n & mask) << lsb));
    }
}

void testUbfiz64AndShiftMaskValue()
{
    // Test Pattern: d = (mask & n) << lsb 
    // Where: mask = (1 << width) - 1
    if (JSC::Options::defaultB3OptLevel() < 2)
        return;
    uint64_t n = 0xffffffffffffffff;
    Vector<uint64_t> lsbs = { 1, 30, 62 };
    Vector<uint64_t> widths = { 62, 33, 1 };

    auto test = [&] (uint64_t lsb, uint64_t mask) -> uint64_t {
        Procedure proc;
        BasicBlock* root = proc.addBlock();

        Value* nValue = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
        Value* lsbValue = root->appendNew<Const32Value>(proc, Origin(), lsb);
        Value* maskValue = root->appendNew<Const64Value>(proc, Origin(), mask);

        Value* left = root->appendNew<Value>(proc, BitAnd, Origin(), maskValue, nValue);
        root->appendNewControlValue(
            proc, Return, Origin(),
            root->appendNew<Value>(proc, Shl, Origin(), left, lsbValue));

        auto code = compileProc(proc);
        if (isARM64())
            checkUsesInstruction(*code, "ubfiz");
        return invoke<uint64_t>(*code, n);
    };

    auto generateMask = [&] (uint64_t width) -> uint64_t {
        return (1ULL << width) - 1ULL;
    };

    for (size_t i = 0; i < lsbs.size(); ++i) {
        uint64_t lsb = lsbs.at(i);
        uint64_t mask = generateMask(widths.at(i));
        CHECK(test(lsb, mask) == ((mask & n) << lsb));
    }
}

void testUbfiz64ShiftAnd()
{
    // Test Pattern: d = (n << lsb) & maskShift
    // Where: maskShift = mask << lsb
    //        mask = (1 << width) - 1
    if (JSC::Options::defaultB3OptLevel() < 2)
        return;
    uint64_t n = 0xffffffffffffffff;
    Vector<uint64_t> lsbs = { 1, 30, 62 };
    Vector<uint64_t> widths = { 62, 33, 1 };

    auto test = [&] (uint64_t lsb, uint64_t maskShift) -> uint64_t {
        Procedure proc;
        BasicBlock* root = proc.addBlock();

        Value* nValue = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
        Value* lsbValue = root->appendNew<Const32Value>(proc, Origin(), lsb);
        Value* maskShiftValue = root->appendNew<Const64Value>(proc, Origin(), maskShift);

        Value* left = root->appendNew<Value>(proc, Shl, Origin(), nValue, lsbValue);
        root->appendNewControlValue(
            proc, Return, Origin(),
            root->appendNew<Value>(proc, BitAnd, Origin(), left, maskShiftValue));

        auto code = compileProc(proc);
        if (isARM64())
            checkUsesInstruction(*code, "ubfiz");
        return invoke<uint64_t>(*code, n);
    };

    auto generateMaskShift = [&] (uint64_t width, uint64_t lsb) -> uint64_t {
        return ((1ULL << width) - 1ULL) << lsb;
    };

    for (size_t i = 0; i < lsbs.size(); ++i) {
        uint64_t lsb = lsbs.at(i);
        uint64_t maskShift = generateMaskShift(widths.at(i), lsb);
        CHECK(test(lsb, maskShift) == ((n << lsb) & maskShift));
    }
}

void testUbfiz64AndShift()
{
    // Test Pattern: d = maskShift & (n << lsb)
    // Where: maskShift = mask << lsb
    //        mask = (1 << width) - 1
    if (JSC::Options::defaultB3OptLevel() < 2)
        return;
    uint64_t n = 0xffffffffffffffff;
    Vector<uint64_t> lsbs = { 1, 30, 62 };
    Vector<uint64_t> widths = { 62, 33, 1 };

    auto test = [&] (uint64_t lsb, uint64_t maskShift) -> uint64_t {
        Procedure proc;
        BasicBlock* root = proc.addBlock();

        Value* nValue = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
        Value* lsbValue = root->appendNew<Const32Value>(proc, Origin(), lsb);
        Value* maskShiftValue = root->appendNew<Const64Value>(proc, Origin(), maskShift);

        Value* right = root->appendNew<Value>(proc, Shl, Origin(), nValue, lsbValue);
        root->appendNewControlValue(
            proc, Return, Origin(),
            root->appendNew<Value>(proc, BitAnd, Origin(), maskShiftValue, right));

        auto code = compileProc(proc);
        if (isARM64())
            checkUsesInstruction(*code, "ubfiz");
        return invoke<uint64_t>(*code, n);
    };

    auto generateMaskShift = [&] (uint64_t width, uint64_t lsb) -> uint64_t {
        return ((1ULL << width) - 1ULL) << lsb;
    };

    for (size_t i = 0; i < lsbs.size(); ++i) {
        uint64_t lsb = lsbs.at(i);
        uint64_t maskShift = generateMaskShift(widths.at(i), lsb);
        CHECK(test(lsb, maskShift) == (maskShift & (n << lsb)));
    }
}

void testInsertBitField32()
{
    if (JSC::Options::defaultB3OptLevel() < 2)
        return;
    uint32_t d = 0xf0f0f0f0;
    uint32_t n = 0xffffffff;
    Vector<uint32_t> lsbs = { 2, 2, 14, 30, 30 };
    Vector<uint32_t> widths = { 30, 3, 17, 1, 2 };

    // Test Pattern: d = ((n & mask1) << lsb) | (d & mask2)
    // Where: mask1 = ((1 << width) - 1)
    //        mask2 = ~(mask1 << lsb)
    auto test1 = [&] (uint32_t lsb, uint32_t mask1, uint32_t mask2) -> uint32_t {
        Procedure proc;
        BasicBlock* root = proc.addBlock();

        Value* dValue = root->appendNew<Value>(
            proc, Trunc, Origin(), 
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
        Value* nValue = root->appendNew<Value>(
            proc, Trunc, Origin(), 
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1));
        Value* lsbValue = root->appendNew<Const32Value>(proc, Origin(), lsb);
        Value* maskValue1 = root->appendNew<Const32Value>(proc, Origin(), mask1);
        Value* maskValue2 = root->appendNew<Const32Value>(proc, Origin(), mask2);

        Value* leftAndValue = root->appendNew<Value>(proc, BitAnd, Origin(), nValue, maskValue1);
        Value* shiftValue = root->appendNew<Value>(proc, Shl, Origin(), leftAndValue, lsbValue);
        Value* rightAndValue = root->appendNew<Value>(proc, BitAnd, Origin(), dValue, maskValue2);
        root->appendNewControlValue(
            proc, Return, Origin(),
            root->appendNew<Value>(proc, BitOr, Origin(), shiftValue, rightAndValue));

        auto code = compileProc(proc);
        if (isARM64())
            checkUsesInstruction(*code, "bfi");
        return invoke<uint32_t>(*code, d, n);
    };

    for (size_t i = 0; i < lsbs.size(); ++i) {
        uint32_t lsb = lsbs.at(i);
        uint32_t mask1 = (1U << widths.at(i)) - 1U;
        uint32_t mask2 = ~(mask1 << lsb);
        CHECK(test1(lsb, mask1, mask2) == (((n & mask1) << lsb) | (d & mask2)));
    }

    // Test Pattern: d = (d & mask2) | ((n & mask1) << lsb) 
    // Where: mask1 = ((1 << width) - 1)
    //        mask2 = ~(mask1 << lsb)
    auto test2 = [&] (uint32_t lsb, uint32_t mask1, uint32_t mask2) -> uint32_t {
        Procedure proc;
        BasicBlock* root = proc.addBlock();

        Value* dValue = root->appendNew<Value>(
            proc, Trunc, Origin(), 
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
        Value* nValue = root->appendNew<Value>(
            proc, Trunc, Origin(), 
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1));
        Value* lsbValue = root->appendNew<Const32Value>(proc, Origin(), lsb);
        Value* maskValue1 = root->appendNew<Const32Value>(proc, Origin(), mask1);
        Value* maskValue2 = root->appendNew<Const32Value>(proc, Origin(), mask2);

        Value* leftAndValue = root->appendNew<Value>(proc, BitAnd, Origin(), dValue, maskValue2);
        Value* rightAndValue = root->appendNew<Value>(proc, BitAnd, Origin(), nValue, maskValue1);
        Value* shiftValue = root->appendNew<Value>(proc, Shl, Origin(), rightAndValue, lsbValue);
        root->appendNewControlValue(
            proc, Return, Origin(),
            root->appendNew<Value>(proc, BitOr, Origin(), leftAndValue, shiftValue));

        auto code = compileProc(proc);
        if (isARM64())
            checkUsesInstruction(*code, "bfi");
        return invoke<uint32_t>(*code, d, n);
    };

    for (size_t i = 0; i < lsbs.size(); ++i) {
        uint32_t lsb = lsbs.at(i);
        uint32_t mask1 = (1U << widths.at(i)) - 1U;
        uint32_t mask2 = ~(mask1 << lsb);
        CHECK(test2(lsb, mask1, mask2) == ((d & mask2) | ((n & mask1) << lsb)));
    }

    // Test use role on destination register of BFI
    uint32_t dA = 0x0000f0f0;
    uint32_t dB = 0xf0f00000;

    auto test3 = [&] (uint32_t lsb, uint32_t mask1, uint32_t mask2) -> uint32_t {
        Procedure proc;
        BasicBlock* root = proc.addBlock();

        Value* dAValue = root->appendNew<Value>(
            proc, Trunc, Origin(), 
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
        Value* dBValue = root->appendNew<Value>(
            proc, Trunc, Origin(), 
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1));
        Value* nValue = root->appendNew<Value>(
            proc, Trunc, Origin(), 
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR2));
        Value* lsbValue = root->appendNew<Const32Value>(proc, Origin(), lsb);
        Value* maskValue1 = root->appendNew<Const32Value>(proc, Origin(), mask1);
        Value* maskValue2 = root->appendNew<Const32Value>(proc, Origin(), mask2);

        // d = dA + dB
        Value* dValue = root->appendNew<Value>(proc, Add, Origin(), dAValue, dBValue);

        // d = (d & mask2) | ((n & mask1) << lsb) 
        Value* leftAndValue = root->appendNew<Value>(proc, BitAnd, Origin(), nValue, maskValue1);
        Value* shiftValue = root->appendNew<Value>(proc, Shl, Origin(), leftAndValue, lsbValue);
        Value* rightAndValue = root->appendNew<Value>(proc, BitAnd, Origin(), dValue, maskValue2);
        Value* orValue = root->appendNew<Value>(proc, BitOr, Origin(), shiftValue, rightAndValue);

        // v3 = ((mask1 + mask2) + dB) + dA
        Value* value1 = root->appendNew<Value>(proc, Add, Origin(), maskValue1, maskValue2);
        Value* value2 = root->appendNew<Value>(proc, Add, Origin(), value1, dBValue);
        Value* value3 = root->appendNew<Value>(proc, Add, Origin(), value2, dAValue);

        root->appendNewControlValue(
            proc, Return, Origin(), 
            root->appendNew<Value>(proc, Add, Origin(), value3, orValue));

        auto code = compileProc(proc);
        if (isARM64())
            checkUsesInstruction(*code, "bfi");
        return invoke<uint32_t>(*code, dA, dB, n);
    };

    for (size_t i = 0; i < lsbs.size(); ++i) {
        uint32_t lsb = lsbs.at(i);
        uint32_t mask1 = (1U << widths.at(i)) - 1U;
        uint32_t mask2 = ~(mask1 << lsb);

        uint32_t lhs3 = test3(lsb, mask1, mask2);
        uint32_t dv = dA + dB;
        dv = (dv & mask2) | ((n & mask1) << lsb);
        uint32_t v3 = ((mask1 + mask2) + dB) + dA;
        uint32_t rhs3 = v3 + dv;
        CHECK(lhs3 == rhs3);
    }
}

void testInsertBitField64()
{
    if (JSC::Options::defaultB3OptLevel() < 2)
        return;
    uint64_t d = 0xf0f0f0f0f0f0f0f0;
    uint64_t n = 0xffffffffffffffff;
    Vector<uint64_t> lsbs = { 2, 30, 14, 62, 62 };
    Vector<uint64_t> widths = { 62, 33, 17, 1, 2 };

    // Test Pattern: d = ((n & mask1) << lsb) | (d & mask2)
    // Where: mask1 = ((1 << width) - 1)
    //        mask2 = ~(mask1 << lsb)
    auto test1 = [&] (uint64_t lsb, uint64_t mask1, uint64_t mask2) -> uint64_t {
        Procedure proc;
        BasicBlock* root = proc.addBlock();

        Value* dValue = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
        Value* nValue = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
        Value* lsbValue = root->appendNew<Const32Value>(proc, Origin(), lsb);
        Value* maskValue1 = root->appendNew<Const64Value>(proc, Origin(), mask1);
        Value* maskValue2 = root->appendNew<Const64Value>(proc, Origin(), mask2);

        Value* leftAndValue = root->appendNew<Value>(proc, BitAnd, Origin(), nValue, maskValue1);
        Value* shiftValue = root->appendNew<Value>(proc, Shl, Origin(), leftAndValue, lsbValue);
        Value* rightAndValue = root->appendNew<Value>(proc, BitAnd, Origin(), dValue, maskValue2);
        root->appendNewControlValue(
            proc, Return, Origin(),
            root->appendNew<Value>(proc, BitOr, Origin(), shiftValue, rightAndValue));

        auto code = compileProc(proc);
        if (isARM64())
            checkUsesInstruction(*code, "bfi");
        return invoke<uint64_t>(*code, d, n);
    };

    // Test Pattern: d = (d & mask2) | ((n & mask1) << lsb) 
    // Where: mask1 = ((1 << width) - 1)
    //        mask2 = ~(mask1 << lsb)
    auto test2 = [&] (uint64_t lsb, uint64_t mask1, uint64_t mask2) -> uint64_t {
        Procedure proc;
        BasicBlock* root = proc.addBlock();

        Value* dValue = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
        Value* nValue = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
        Value* lsbValue = root->appendNew<Const32Value>(proc, Origin(), lsb);
        Value* maskValue1 = root->appendNew<Const64Value>(proc, Origin(), mask1);
        Value* maskValue2 = root->appendNew<Const64Value>(proc, Origin(), mask2);

        Value* leftAndValue = root->appendNew<Value>(proc, BitAnd, Origin(), dValue, maskValue2);
        Value* rightAndValue = root->appendNew<Value>(proc, BitAnd, Origin(), nValue, maskValue1);
        Value* shiftValue = root->appendNew<Value>(proc, Shl, Origin(), rightAndValue, lsbValue);
        root->appendNewControlValue(
            proc, Return, Origin(),
            root->appendNew<Value>(proc, BitOr, Origin(), leftAndValue, shiftValue));

        auto code = compileProc(proc);
        if (isARM64())
            checkUsesInstruction(*code, "bfi");
        return invoke<uint64_t>(*code, d, n);
    };

    for (size_t i = 0; i < lsbs.size(); ++i) {
        uint64_t lsb = lsbs.at(i);
        uint64_t mask1 = (1ULL << widths.at(i)) - 1ULL;
        uint64_t mask2 = ~(mask1 << lsb);
        CHECK(test1(lsb, mask1, mask2) == (((n & mask1) << lsb) | (d & mask2)));
        CHECK(test2(lsb, mask1, mask2) == ((d & mask2) | ((n & mask1) << lsb)));
    }

    // Test use role on destination register of BFI
    uint64_t dA = 0x00000000f0f0f0f0;
    uint64_t dB = 0xf0f0f0f000000000;

    auto test3 = [&] (uint64_t lsb, uint64_t mask1, uint64_t mask2) -> uint64_t {
        Procedure proc;
        BasicBlock* root = proc.addBlock();

        Value* dAValue = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
        Value* dBValue = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
        Value* nValue = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR2);
        Value* lsbValue = root->appendNew<Const32Value>(proc, Origin(), lsb);
        Value* maskValue1 = root->appendNew<Const64Value>(proc, Origin(), mask1);
        Value* maskValue2 = root->appendNew<Const64Value>(proc, Origin(), mask2);

        // d = dA + dB
        Value* dValue = root->appendNew<Value>(proc, Add, Origin(), dAValue, dBValue);

        // d = (d & mask2) | ((n & mask1) << lsb) 
        Value* leftAndValue = root->appendNew<Value>(proc, BitAnd, Origin(), nValue, maskValue1);
        Value* shiftValue = root->appendNew<Value>(proc, Shl, Origin(), leftAndValue, lsbValue);
        Value* rightAndValue = root->appendNew<Value>(proc, BitAnd, Origin(), dValue, maskValue2);
        Value* orValue = root->appendNew<Value>(proc, BitOr, Origin(), shiftValue, rightAndValue);

        // v3 = ((mask1 + mask2) + dB) + dA
        Value* value1 = root->appendNew<Value>(proc, Add, Origin(), maskValue1, maskValue2);
        Value* value2 = root->appendNew<Value>(proc, Add, Origin(), value1, dBValue);
        Value* value3 = root->appendNew<Value>(proc, Add, Origin(), value2, dAValue);

        root->appendNewControlValue(
            proc, Return, Origin(), 
            root->appendNew<Value>(proc, Add, Origin(), value3, orValue));

        auto code = compileProc(proc);
        if (isARM64())
            checkUsesInstruction(*code, "bfi");
        return invoke<uint64_t>(*code, dA, dB, n);
    };

    for (size_t i = 0; i < lsbs.size(); ++i) {
        uint64_t lsb = lsbs.at(i);
        uint64_t mask1 = (1ULL << widths.at(i)) - 1ULL;
        uint64_t mask2 = ~(mask1 << lsb);

        uint64_t lhs3 = test3(lsb, mask1, mask2);
        uint64_t dv = dA + dB;
        dv = (dv & mask2) | ((n & mask1) << lsb);
        uint64_t v3 = ((mask1 + mask2) + dB) + dA;
        uint64_t rhs3 = v3 + dv;
        CHECK(lhs3 == rhs3);
    }
}

void testExtractInsertBitfieldAtLowEnd32()
{
    if (JSC::Options::defaultB3OptLevel() < 2)
        return;
    uint32_t d = 0xf0f0f0f0;
    uint32_t n = 0xffffffff;
    Vector<uint32_t> lsbs = { 1, 10, 14, 30 };
    Vector<uint32_t> widths = { 30, 11, 17, 1 };

    // BFXIL Pattern: d = ((n >> lsb) & mask1) | (d & mask2)
    // Where: mask1 = ((1 << width) - 1)
    //        mask2 = ~mask1
    auto test1 = [&] (uint32_t lsb, uint32_t mask1, uint32_t mask2) -> uint32_t {
        Procedure proc;
        BasicBlock* root = proc.addBlock();

        Value* dValue = root->appendNew<Value>(
            proc, Trunc, Origin(), 
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
        Value* nValue = root->appendNew<Value>(
            proc, Trunc, Origin(), 
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1));
        Value* lsbValue = root->appendNew<Const32Value>(proc, Origin(), lsb);
        Value* maskValue1 = root->appendNew<Const32Value>(proc, Origin(), mask1);
        Value* maskValue2 = root->appendNew<Const32Value>(proc, Origin(), mask2);

        Value* shiftValue = root->appendNew<Value>(proc, ZShr, Origin(), nValue, lsbValue);
        Value* leftAndValue = root->appendNew<Value>(proc, BitAnd, Origin(), shiftValue, maskValue1);
        Value* rightAndValue = root->appendNew<Value>(proc, BitAnd, Origin(), dValue, maskValue2);
        root->appendNewControlValue(
            proc, Return, Origin(),
            root->appendNew<Value>(proc, BitOr, Origin(), leftAndValue, rightAndValue));

        auto code = compileProc(proc);
        if (isARM64())
            checkUsesInstruction(*code, "bfxil");
        return invoke<uint32_t>(*code, d, n);
    };

    for (size_t i = 0; i < lsbs.size(); ++i) {
        uint32_t lsb = lsbs.at(i);
        uint32_t mask1 = (1U << widths.at(i)) - 1U;
        uint32_t mask2 = ~mask1;
        CHECK(test1(lsb, mask1, mask2) == (((n >> lsb) & mask1) | (d & mask2)));
    }

    // BFXIL Pattern: d = ((n & mask1) >> lsb) | (d & mask2)
    // Where: mask1 = ((1 << width) - 1) << lsb
    //        mask2 = ~(mask1 >> lsb)
    auto test2 = [&] (uint32_t lsb, uint32_t mask1, uint32_t mask2) -> uint32_t {
        Procedure proc;
        BasicBlock* root = proc.addBlock();

        Value* dValue = root->appendNew<Value>(
            proc, Trunc, Origin(), 
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
        Value* nValue = root->appendNew<Value>(
            proc, Trunc, Origin(), 
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1));
        Value* lsbValue = root->appendNew<Const32Value>(proc, Origin(), lsb);
        Value* maskValue1 = root->appendNew<Const32Value>(proc, Origin(), mask1);
        Value* maskValue2 = root->appendNew<Const32Value>(proc, Origin(), mask2);

        Value* leftAndValue = root->appendNew<Value>(proc, BitAnd, Origin(), nValue, maskValue1);
        Value* rightAndValue = root->appendNew<Value>(proc, BitAnd, Origin(), dValue, maskValue2);
        Value* shiftValue = root->appendNew<Value>(proc, ZShr, Origin(), leftAndValue, lsbValue);
        root->appendNewControlValue(
            proc, Return, Origin(),
            root->appendNew<Value>(proc, BitOr, Origin(), shiftValue, rightAndValue));

        auto code = compileProc(proc);
        if (isARM64())
            checkUsesInstruction(*code, "bfxil");
        return invoke<uint32_t>(*code, d, n);
    };

    for (size_t i = 0; i < lsbs.size(); ++i) {
        uint32_t lsb = lsbs.at(i);
        uint32_t mask1 = ((1U << widths.at(i)) - 1U) << lsb;
        uint32_t mask2 = ~(mask1 >> lsb);
        CHECK(test2(lsb, mask1, mask2) == (((n & mask1) >> lsb) | (d & mask2)));
    }

    // Test use role on destination register of BFXIL
    uint32_t dA = 0x0000f0f0;
    uint32_t dB = 0xf0f00000;

    auto test3 = [&] (uint32_t lsb, uint32_t mask1, uint32_t mask2) -> uint32_t {
        Procedure proc;
        BasicBlock* root = proc.addBlock();

        Value* dAValue = root->appendNew<Value>(
            proc, Trunc, Origin(), 
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
        Value* dBValue = root->appendNew<Value>(
            proc, Trunc, Origin(), 
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1));
        Value* nValue = root->appendNew<Value>(
            proc, Trunc, Origin(), 
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR2));
        Value* lsbValue = root->appendNew<Const32Value>(proc, Origin(), lsb);
        Value* maskValue1 = root->appendNew<Const32Value>(proc, Origin(), mask1);
        Value* maskValue2 = root->appendNew<Const32Value>(proc, Origin(), mask2);

        // d = dA + dB
        Value* dValue = root->appendNew<Value>(proc, Add, Origin(), dAValue, dBValue);

        // d = d = ((n >> lsb) & mask1) | (d & mask2)
        Value* shiftValue = root->appendNew<Value>(proc, ZShr, Origin(), nValue, lsbValue);
        Value* leftAndValue = root->appendNew<Value>(proc, BitAnd, Origin(), shiftValue, maskValue1);
        Value* rightAndValue = root->appendNew<Value>(proc, BitAnd, Origin(), dValue, maskValue2);
        Value* orValue = root->appendNew<Value>(proc, BitOr, Origin(), leftAndValue, rightAndValue);

        // v3 = ((mask1 + mask2) + dB) + dA
        Value* value1 = root->appendNew<Value>(proc, Add, Origin(), maskValue1, maskValue2);
        Value* value2 = root->appendNew<Value>(proc, Add, Origin(), value1, dBValue);
        Value* value3 = root->appendNew<Value>(proc, Add, Origin(), value2, dAValue);

        root->appendNewControlValue(
            proc, Return, Origin(), 
            root->appendNew<Value>(proc, Add, Origin(), value3, orValue));

        auto code = compileProc(proc);
        if (isARM64())
            checkUsesInstruction(*code, "bfxil");
        return invoke<uint32_t>(*code, dA, dB, n);
    };

    for (size_t i = 0; i < lsbs.size(); ++i) {
        uint32_t lsb = lsbs.at(i);
        uint32_t mask1 = (1U << widths.at(i)) - 1U;
        uint32_t mask2 = ~mask1;

        uint32_t lhs3 = test3(lsb, mask1, mask2);
        uint32_t dv = dA + dB;
        dv = ((n >> lsb) & mask1) | (dv & mask2);
        uint32_t v3 = ((mask1 + mask2) + dB) + dA;
        uint32_t rhs3 = v3 + dv;
        CHECK(lhs3 == rhs3);
    }
}

void testExtractInsertBitfieldAtLowEnd64()
{
    if (JSC::Options::defaultB3OptLevel() < 2)
        return;
    uint64_t d = 0xf0f0f0f0f0f0f0f0;
    uint64_t n = 0xffffffffffffffff;
    Vector<uint64_t> lsbs = { 1, 30, 14, 62 };
    Vector<uint64_t> widths = { 62, 33, 17, 1 };

    // BFXIL Pattern: d = ((n >> lsb) & mask1) | (d & mask2)
    // Where: mask1 = ((1 << width) - 1)
    //        mask2 = ~mask1
    auto test1 = [&] (uint64_t lsb, uint64_t mask1, uint64_t mask2) -> uint64_t {
        Procedure proc;
        BasicBlock* root = proc.addBlock();

        Value* dValue = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
        Value* nValue = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
        Value* lsbValue = root->appendNew<Const32Value>(proc, Origin(), lsb);
        Value* maskValue1 = root->appendNew<Const64Value>(proc, Origin(), mask1);
        Value* maskValue2 = root->appendNew<Const64Value>(proc, Origin(), mask2);

        Value* shiftValue = root->appendNew<Value>(proc, ZShr, Origin(), nValue, lsbValue);
        Value* leftAndValue = root->appendNew<Value>(proc, BitAnd, Origin(), shiftValue, maskValue1);
        Value* rightAndValue = root->appendNew<Value>(proc, BitAnd, Origin(), dValue, maskValue2);
        root->appendNewControlValue(
            proc, Return, Origin(),
            root->appendNew<Value>(proc, BitOr, Origin(), leftAndValue, rightAndValue));

        auto code = compileProc(proc);
        if (isARM64())
            checkUsesInstruction(*code, "bfxil");
        return invoke<uint64_t>(*code, d, n);
    };

    for (size_t i = 0; i < lsbs.size(); ++i) {
        uint64_t lsb = lsbs.at(i);
        uint64_t mask1 = (1ULL << widths.at(i)) - 1ULL;
        uint64_t mask2 = ~mask1;
        CHECK(test1(lsb, mask1, mask2) == (((n >> lsb) & mask1) | (d & mask2)));
    }

    // BFXIL Pattern: d = ((n & mask1) >> lsb) | (d & mask2)
    // Where: mask1 = ((1 << width) - 1) << lsb
    //        mask2 = ~(mask1 >> lsb)
    auto test2 = [&] (uint64_t lsb, uint64_t mask1, uint64_t mask2) -> uint64_t {
        Procedure proc;
        BasicBlock* root = proc.addBlock();

        Value* dValue = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
        Value* nValue = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
        Value* lsbValue = root->appendNew<Const32Value>(proc, Origin(), lsb);
        Value* maskValue1 = root->appendNew<Const64Value>(proc, Origin(), mask1);
        Value* maskValue2 = root->appendNew<Const64Value>(proc, Origin(), mask2);

        Value* leftAndValue = root->appendNew<Value>(proc, BitAnd, Origin(), nValue, maskValue1);
        Value* rightAndValue = root->appendNew<Value>(proc, BitAnd, Origin(), dValue, maskValue2);
        Value* shiftValue = root->appendNew<Value>(proc, ZShr, Origin(), leftAndValue, lsbValue);
        root->appendNewControlValue(
            proc, Return, Origin(),
            root->appendNew<Value>(proc, BitOr, Origin(), shiftValue, rightAndValue));

        auto code = compileProc(proc);
        if (isARM64())
            checkUsesInstruction(*code, "bfxil");
        return invoke<uint64_t>(*code, d, n);
    };

    for (size_t i = 0; i < lsbs.size(); ++i) {
        uint64_t lsb = lsbs.at(i);
        uint64_t mask1 = ((1ULL << widths.at(i)) - 1ULL) << lsb;
        uint64_t mask2 = ~(mask1 >> lsb);
        CHECK(test2(lsb, mask1, mask2) == (((n & mask1) >> lsb) | (d & mask2)));
    }

    // Test use role on destination register of BFXIL
    uint64_t dA = 0x00000000f0f0f0f0;
    uint64_t dB = 0xf0f0f0f000000000;

    auto test3 = [&] (uint64_t lsb, uint64_t mask1, uint64_t mask2) -> uint64_t {
        Procedure proc;
        BasicBlock* root = proc.addBlock();

        Value* dAValue = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
        Value* dBValue = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
        Value* nValue = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR2);
        Value* lsbValue = root->appendNew<Const32Value>(proc, Origin(), lsb);
        Value* maskValue1 = root->appendNew<Const64Value>(proc, Origin(), mask1);
        Value* maskValue2 = root->appendNew<Const64Value>(proc, Origin(), mask2);

        // d = dA + dB
        Value* dValue = root->appendNew<Value>(proc, Add, Origin(), dAValue, dBValue);

        // d = d = ((n >> lsb) & mask1) | (d & mask2)
        Value* shiftValue = root->appendNew<Value>(proc, ZShr, Origin(), nValue, lsbValue);
        Value* leftAndValue = root->appendNew<Value>(proc, BitAnd, Origin(), shiftValue, maskValue1);
        Value* rightAndValue = root->appendNew<Value>(proc, BitAnd, Origin(), dValue, maskValue2);
        Value* orValue = root->appendNew<Value>(proc, BitOr, Origin(), leftAndValue, rightAndValue);

        // v3 = ((mask1 + mask2) + dB) + dA
        Value* value1 = root->appendNew<Value>(proc, Add, Origin(), maskValue1, maskValue2);
        Value* value2 = root->appendNew<Value>(proc, Add, Origin(), value1, dBValue);
        Value* value3 = root->appendNew<Value>(proc, Add, Origin(), value2, dAValue);

        root->appendNewControlValue(
            proc, Return, Origin(), 
            root->appendNew<Value>(proc, Add, Origin(), value3, orValue));

        auto code = compileProc(proc);
        if (isARM64())
            checkUsesInstruction(*code, "bfxil");
        return invoke<uint64_t>(*code, dA, dB, n);
    };

    for (size_t i = 0; i < lsbs.size(); ++i) {
        uint64_t lsb = lsbs.at(i);
        uint64_t mask1 = (1ULL << widths.at(i)) - 1ULL;
        uint64_t mask2 = ~mask1;

        uint64_t lhs3 = test3(lsb, mask1, mask2);
        uint64_t dv = dA + dB;
        dv = ((n >> lsb) & mask1) | (dv & mask2);
        uint64_t v3 = ((mask1 + mask2) + dB) + dA;
        uint64_t rhs3 = v3 + dv;
        CHECK(lhs3 == rhs3);
    }
}

void testBIC32()
{
    if (JSC::Options::defaultB3OptLevel() < 2)
        return;

    // Test Pattern: d = n & (-m - 1)
    auto test1 = [&] (int32_t n, int32_t m) -> int32_t {
        Procedure proc;
        BasicBlock* root = proc.addBlock();

        Value* nValue = root->appendNew<Value>(
            proc, Trunc, Origin(), 
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
        Value* mValue = root->appendNew<Value>(
            proc, Trunc, Origin(), 
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1));
        Value* const1 = root->appendNew<Const32Value>(proc, Origin(), 1);

        Value* negValue = root->appendNew<Value>(proc, Neg, Origin(), mValue);
        Value* subValue = root->appendNew<Value>(proc, Sub, Origin(), negValue, const1);
        root->appendNewControlValue(
            proc, Return, Origin(), 
            root->appendNew<Value>(proc, BitAnd, Origin(), nValue, subValue));

        auto code = compileProc(proc);
        if (isARM64())
            checkUsesInstruction(*code, "bic");
        return invoke<int32_t>(*code, n, m);
    };

    // Test Pattern: d = n & (m ^ -1)
    auto test2 = [&] (int32_t n, int32_t m) -> int32_t {
        Procedure proc;
        BasicBlock* root = proc.addBlock();

        Value* nValue = root->appendNew<Value>(
            proc, Trunc, Origin(), 
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
        Value* mValue = root->appendNew<Value>(
            proc, Trunc, Origin(), 
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1));
        Value* minusOneValue = root->appendNew<Const32Value>(proc, Origin(), -1);

        Value* xorValue = root->appendNew<Value>(proc, BitXor, Origin(), mValue, minusOneValue);
        root->appendNewControlValue(
            proc, Return, Origin(), 
            root->appendNew<Value>(proc, BitAnd, Origin(), nValue, xorValue));

        auto code = compileProc(proc);
        if (isARM64())
            checkUsesInstruction(*code, "bic");
        return invoke<int32_t>(*code, n, m);
    };

    for (auto n : int32Operands()) {
        for (auto m : int32Operands()) {
            CHECK(test1(n.value, m.value) == (n.value & (-m.value - 1)));
            CHECK(test2(n.value, m.value) == (n.value & (m.value ^ -1)));
        }
    }
}

void testBIC64()
{
    if (JSC::Options::defaultB3OptLevel() < 2)
        return;

    // Test Pattern: d = n & (-m - 1)
    auto test1 = [&] (int64_t n, int64_t m) -> int64_t {
        Procedure proc;
        BasicBlock* root = proc.addBlock();

        Value* nValue = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
        Value* mValue = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
        Value* const1 = root->appendNew<Const64Value>(proc, Origin(), 1);

        Value* negValue = root->appendNew<Value>(proc, Neg, Origin(), mValue);
        Value* subValue = root->appendNew<Value>(proc, Sub, Origin(), negValue, const1);
        root->appendNewControlValue(
            proc, Return, Origin(), 
            root->appendNew<Value>(proc, BitAnd, Origin(), nValue, subValue));

        auto code = compileProc(proc);
        if (isARM64())
            checkUsesInstruction(*code, "bic");
        return invoke<int64_t>(*code, n, m);
    };

    // Test Pattern: d = n & (m ^ -1)
    auto test2 = [&] (int64_t n, int64_t m) -> int64_t {
        Procedure proc;
        BasicBlock* root = proc.addBlock();

        Value* nValue = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
        Value* mValue = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
        Value* minusOneValue = root->appendNew<Const64Value>(proc, Origin(), -1);

        Value* xorValue = root->appendNew<Value>(proc, BitXor, Origin(), mValue, minusOneValue);
        root->appendNewControlValue(
            proc, Return, Origin(), 
            root->appendNew<Value>(proc, BitAnd, Origin(), nValue, xorValue));

        auto code = compileProc(proc);
        if (isARM64())
            checkUsesInstruction(*code, "bic");
        return invoke<int64_t>(*code, n, m);
    };

    for (auto n : int64Operands()) {
        for (auto m : int64Operands()) {
            CHECK(test1(n.value, m.value) == (n.value & (-m.value - 1LL)));
            CHECK(test2(n.value, m.value) == (n.value & (m.value ^ -1LL)));
        }
    }
}

void testOrNot32()
{
    if (JSC::Options::defaultB3OptLevel() < 2)
        return;

    // Test Pattern: d = n | (-m - 1)
    auto test1 = [&] (int32_t n, int32_t m) -> int32_t {
        Procedure proc;
        BasicBlock* root = proc.addBlock();

        Value* nValue = root->appendNew<Value>(
            proc, Trunc, Origin(), 
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
        Value* mValue = root->appendNew<Value>(
            proc, Trunc, Origin(), 
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1));
        Value* const1 = root->appendNew<Const32Value>(proc, Origin(), 1);

        Value* negValue = root->appendNew<Value>(proc, Neg, Origin(), mValue);
        Value* subValue = root->appendNew<Value>(proc, Sub, Origin(), negValue, const1);
        root->appendNewControlValue(
            proc, Return, Origin(), 
            root->appendNew<Value>(proc, BitOr, Origin(), nValue, subValue));

        auto code = compileProc(proc);

        if (isARM64())
            checkUsesInstruction(*code, "orn");
        return invoke<int32_t>(*code, n, m);
    };

    // Test Pattern: d = n | (m ^ -1)
    auto test2 = [&] (int32_t n, int32_t m) -> int32_t {
        Procedure proc;
        BasicBlock* root = proc.addBlock();

        Value* nValue = root->appendNew<Value>(
            proc, Trunc, Origin(), 
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
        Value* mValue = root->appendNew<Value>(
            proc, Trunc, Origin(), 
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1));
        Value* minusOneValue = root->appendNew<Const32Value>(proc, Origin(), -1);

        Value* xorValue = root->appendNew<Value>(proc, BitXor, Origin(), mValue, minusOneValue);
        root->appendNewControlValue(
            proc, Return, Origin(), 
            root->appendNew<Value>(proc, BitOr, Origin(), nValue, xorValue));

        auto code = compileProc(proc);
        if (isARM64())
            checkUsesInstruction(*code, "orn");
        return invoke<int32_t>(*code, n, m);
    };

    for (auto n : int32Operands()) {
        for (auto m : int32Operands()) {
            CHECK(test1(n.value, m.value) == (n.value | (-m.value - 1)));
            CHECK(test2(n.value, m.value) == (n.value | (m.value ^ -1)));
        }
    }
}

void testOrNot64()
{
    if (JSC::Options::defaultB3OptLevel() < 2)
        return;

    // Test Pattern: d = n | (-m - 1)
    auto test1 = [&] (int64_t n, int64_t m) -> int64_t {
        Procedure proc;
        BasicBlock* root = proc.addBlock();

        Value* nValue = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
        Value* mValue = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
        Value* const1 = root->appendNew<Const64Value>(proc, Origin(), 1);

        Value* negValue = root->appendNew<Value>(proc, Neg, Origin(), mValue);
        Value* subValue = root->appendNew<Value>(proc, Sub, Origin(), negValue, const1);
        root->appendNewControlValue(
            proc, Return, Origin(), 
            root->appendNew<Value>(proc, BitOr, Origin(), nValue, subValue));

        auto code = compileProc(proc);
        if (isARM64())
            checkUsesInstruction(*code, "orn");
        return invoke<int64_t>(*code, n, m);
    };

    // Test Pattern: d = n | (m ^ -1)
    auto test2 = [&] (int64_t n, int64_t m) -> int64_t {
        Procedure proc;
        BasicBlock* root = proc.addBlock();

        Value* nValue = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
        Value* mValue = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
        Value* minusOneValue = root->appendNew<Const64Value>(proc, Origin(), -1);

        Value* xorValue = root->appendNew<Value>(proc, BitXor, Origin(), mValue, minusOneValue);
        root->appendNewControlValue(
            proc, Return, Origin(), 
            root->appendNew<Value>(proc, BitOr, Origin(), nValue, xorValue));

        auto code = compileProc(proc);
        if (isARM64())
            checkUsesInstruction(*code, "orn");
        return invoke<int64_t>(*code, n, m);
    };

    for (auto n : int64Operands()) {
        for (auto m : int64Operands()) {
            CHECK(test1(n.value, m.value) == (n.value | (-m.value - 1LL)));
            CHECK(test2(n.value, m.value) == (n.value | (m.value ^ -1LL)));
        }
    }
}

void testXorNot32()
{
    if (JSC::Options::defaultB3OptLevel() < 2)
        return;

    // Test Pattern: d = n ^ (m ^ -1)
    auto test = [&] (int32_t n, int32_t m) -> int32_t {
        Procedure proc;
        BasicBlock* root = proc.addBlock();

        Value* nValue = root->appendNew<Value>(
            proc, Trunc, Origin(), 
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
        Value* mValue = root->appendNew<Value>(
            proc, Trunc, Origin(), 
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1));
        Value* minusOneValue = root->appendNew<Const32Value>(proc, Origin(), -1);

        Value* xorValue = root->appendNew<Value>(proc, BitXor, Origin(), mValue, minusOneValue);
        root->appendNewControlValue(
            proc, Return, Origin(), 
            root->appendNew<Value>(proc, BitXor, Origin(), nValue, xorValue));

        auto code = compileProc(proc);
        if (isARM64())
            checkUsesInstruction(*code, "eon");
        return invoke<int32_t>(*code, n, m);
    };

    for (auto n : int32Operands()) {
        for (auto m : int32Operands())
            CHECK(test(n.value, m.value) == (n.value ^ (m.value ^ -1)));
    }
}

void testXorNot64()
{
    if (JSC::Options::defaultB3OptLevel() < 2)
        return;

    // Test Pattern: d = n ^ (m ^ -1)
    auto test = [&] (int64_t n, int64_t m) -> int64_t {
        Procedure proc;
        BasicBlock* root = proc.addBlock();

        Value* nValue = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
        Value* mValue = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
        Value* minusOneValue = root->appendNew<Const64Value>(proc, Origin(), -1);

        Value* xorValue = root->appendNew<Value>(proc, BitXor, Origin(), mValue, minusOneValue);
        root->appendNewControlValue(
            proc, Return, Origin(), 
            root->appendNew<Value>(proc, BitXor, Origin(), nValue, xorValue));

        auto code = compileProc(proc);
        if (isARM64())
            checkUsesInstruction(*code, "eon");
        return invoke<int64_t>(*code, n, m);
    };

    for (auto n : int64Operands()) {
        for (auto m : int64Operands())
            CHECK(test(n.value, m.value) == (n.value ^ (m.value ^ -1LL)));
    }
}

void testXorNotWithLeftShift32()
{
    if (JSC::Options::defaultB3OptLevel() < 2)
        return;
    Vector<int32_t> amounts = { 1, 17, 31 };

    // Test Pattern: d = n ^ ((m << amount) ^ -1)
    auto test = [&] (int32_t n, int32_t m, int32_t amount) -> int32_t {
        Procedure proc;
        BasicBlock* root = proc.addBlock();

        Value* nValue = root->appendNew<Value>(
            proc, Trunc, Origin(), 
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
        Value* mValue = root->appendNew<Value>(
            proc, Trunc, Origin(), 
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1));
        Value* amountValue = root->appendNew<Const32Value>(proc, Origin(), amount);
        Value* minusOneValue = root->appendNew<Const32Value>(proc, Origin(), -1);

        Value* shiftValue = root->appendNew<Value>(proc, Shl, Origin(), mValue, amountValue);
        Value* xorValue = root->appendNew<Value>(proc, BitXor, Origin(), shiftValue, minusOneValue);

        root->appendNewControlValue(
            proc, Return, Origin(), 
            root->appendNew<Value>(proc, BitXor, Origin(), nValue, xorValue));

        auto code = compileProc(proc);
        if (isARM64()) {
            std::string regex(".*eon.*,.*,.*,.*lsl #");
            regex += std::to_string(amount) + ".*";
            checkUsesInstruction(*code, regex.c_str(), true);
        }
        return invoke<int32_t>(*code, n, m);
    };

    for (auto nOperand : int32Operands()) {
        for (auto mOperand : int32Operands()) {
            for (auto amount : amounts) {
                int32_t n = nOperand.value;
                int32_t m = mOperand.value;
                CHECK_EQ(test(n, m, amount), n ^ ((m << amount) ^ -1));
            }
        }
    }
}

void testXorNotWithRightShift32()
{
    if (JSC::Options::defaultB3OptLevel() < 2)
        return;
    Vector<int32_t> amounts = { 1, 17, 31 };

    // Test Pattern: d = n ^ ((m >> amount) ^ -1)
    auto test = [&] (int32_t n, int32_t m, int32_t amount) -> int32_t {
        Procedure proc;
        BasicBlock* root = proc.addBlock();

        Value* nValue = root->appendNew<Value>(
            proc, Trunc, Origin(), 
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
        Value* mValue = root->appendNew<Value>(
            proc, Trunc, Origin(), 
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1));
        Value* amountValue = root->appendNew<Const32Value>(proc, Origin(), amount);
        Value* minusOneValue = root->appendNew<Const32Value>(proc, Origin(), -1);

        Value* shiftValue = root->appendNew<Value>(proc, SShr, Origin(), mValue, amountValue);
        Value* xorValue = root->appendNew<Value>(proc, BitXor, Origin(), shiftValue, minusOneValue);

        root->appendNewControlValue(
            proc, Return, Origin(), 
            root->appendNew<Value>(proc, BitXor, Origin(), nValue, xorValue));

        auto code = compileProc(proc);
        if (isARM64()) {
            std::string regex(".*eon.*,.*,.*,.*asr #");
            regex += std::to_string(amount) + ".*";
            checkUsesInstruction(*code, regex.c_str(), true);
        }
        return invoke<int32_t>(*code, n, m);
    };

    for (auto nOperand : int32Operands()) {
        for (auto mOperand : int32Operands()) {
            for (auto amount : amounts) {
                int32_t n = nOperand.value;
                int32_t m = mOperand.value;
                CHECK_EQ(test(n, m, amount), n ^ ((m >> amount) ^ -1));
            }
        }
    }
}

void testXorNotWithUnsignedRightShift32()
{
    if (JSC::Options::defaultB3OptLevel() < 2)
        return;
    Vector<uint32_t> amounts = { 1, 17, 31 };

    // Test Pattern: d = n ^ ((m >> amount) ^ -1)
    auto test = [&] (int32_t n, int32_t m, int32_t amount) -> uint32_t {
        Procedure proc;
        BasicBlock* root = proc.addBlock();

        Value* nValue = root->appendNew<Value>(
            proc, Trunc, Origin(), 
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
        Value* mValue = root->appendNew<Value>(
            proc, Trunc, Origin(), 
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1));
        Value* amountValue = root->appendNew<Const32Value>(proc, Origin(), amount);
        Value* minusOneValue = root->appendNew<Const32Value>(proc, Origin(), -1);

        Value* shiftValue = root->appendNew<Value>(proc, ZShr, Origin(), mValue, amountValue);
        Value* xorValue = root->appendNew<Value>(proc, BitXor, Origin(), shiftValue, minusOneValue);

        root->appendNewControlValue(
            proc, Return, Origin(), 
            root->appendNew<Value>(proc, BitXor, Origin(), nValue, xorValue));

        auto code = compileProc(proc);
        if (isARM64()) {
            std::string regex(".*eon.*,.*,.*,.*lsr #");
            regex += std::to_string(amount) + ".*";
            checkUsesInstruction(*code, regex.c_str(), true);
        }
        return invoke<uint32_t>(*code, n, m);
    };

    for (auto nOperand : int32Operands()) {
        for (auto mOperand : int32Operands()) {
            for (auto amount : amounts) {
                uint32_t n = nOperand.value;
                uint32_t m = mOperand.value;
                CHECK_EQ(test(n, m, amount), n ^ ((m >> amount) ^ -1));
            }
        }
    }
}

void testXorNotWithLeftShift64()
{
    if (JSC::Options::defaultB3OptLevel() < 2)
        return;
    Vector<int32_t> amounts = { 1, 34, 63 };

    // Test Pattern: d = n ^ ((m << amount) ^ -1)
    auto test = [&] (int64_t n, int64_t m, int32_t amount) -> int64_t {
        Procedure proc;
        BasicBlock* root = proc.addBlock();

        Value* nValue = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
        Value* mValue = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
        Value* amountValue = root->appendNew<Const32Value>(proc, Origin(), amount);
        Value* minusOneValue = root->appendNew<Const64Value>(proc, Origin(), -1);

        Value* shiftValue = root->appendNew<Value>(proc, Shl, Origin(), mValue, amountValue);
        Value* xorValue = root->appendNew<Value>(proc, BitXor, Origin(), shiftValue, minusOneValue);

        root->appendNewControlValue(
            proc, Return, Origin(), 
            root->appendNew<Value>(proc, BitXor, Origin(), nValue, xorValue));

        auto code = compileProc(proc);
        if (isARM64()) {
            std::string regex(".*eon.*,.*,.*,.*lsl #");
            regex += std::to_string(amount) + ".*";
            checkUsesInstruction(*code, regex.c_str(), true);
        }
        return invoke<int64_t>(*code, n, m);
    };

    for (auto nOperand : int64Operands()) {
        for (auto mOperand : int64Operands()) {
            for (auto amount : amounts) {
                int64_t n = nOperand.value;
                int64_t m = mOperand.value;
                CHECK_EQ(test(n, m, amount), n ^ ((m << amount) ^ -1));
            }
        }
    }
}

void testXorNotWithRightShift64()
{
    if (JSC::Options::defaultB3OptLevel() < 2)
        return;
    Vector<int32_t> amounts = { 1, 34, 63 };

    // Test Pattern: d = n ^ ((m >> amount) ^ -1)
    auto test = [&] (int64_t n, int64_t m, int32_t amount) -> int64_t {
        Procedure proc;
        BasicBlock* root = proc.addBlock();

        Value* nValue = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
        Value* mValue = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
        Value* amountValue = root->appendNew<Const32Value>(proc, Origin(), amount);
        Value* minusOneValue = root->appendNew<Const64Value>(proc, Origin(), -1);

        Value* shiftValue = root->appendNew<Value>(proc, SShr, Origin(), mValue, amountValue);
        Value* xorValue = root->appendNew<Value>(proc, BitXor, Origin(), shiftValue, minusOneValue);

        root->appendNewControlValue(
            proc, Return, Origin(), 
            root->appendNew<Value>(proc, BitXor, Origin(), nValue, xorValue));

        auto code = compileProc(proc);
        if (isARM64()) {
            std::string regex(".*eon.*,.*,.*,.*asr #");
            regex += std::to_string(amount) + ".*";
            checkUsesInstruction(*code, regex.c_str(), true);
        }
        return invoke<int64_t>(*code, n, m);
    };

    for (auto nOperand : int64Operands()) {
        for (auto mOperand : int64Operands()) {
            for (auto amount : amounts) {
                int64_t n = nOperand.value;
                int64_t m = mOperand.value;
                CHECK_EQ(test(n, m, amount), n ^ ((m >> amount) ^ -1));
            }
        }
    }
}

void testXorNotWithUnsignedRightShift64()
{
    if (JSC::Options::defaultB3OptLevel() < 2)
        return;
    Vector<uint32_t> amounts = { 1, 17, 31 };

    // Test Pattern: d = n ^ ((m >> amount) ^ -1)
    auto test = [&] (int64_t n, int64_t m, int32_t amount) -> uint64_t {
        Procedure proc;
        BasicBlock* root = proc.addBlock();

        Value* nValue = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
        Value* mValue = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
        Value* amountValue = root->appendNew<Const32Value>(proc, Origin(), amount);
        Value* minusOneValue = root->appendNew<Const64Value>(proc, Origin(), -1);

        Value* shiftValue = root->appendNew<Value>(proc, ZShr, Origin(), mValue, amountValue);
        Value* xorValue = root->appendNew<Value>(proc, BitXor, Origin(), shiftValue, minusOneValue);

        root->appendNewControlValue(
            proc, Return, Origin(), 
            root->appendNew<Value>(proc, BitXor, Origin(), nValue, xorValue));

        auto code = compileProc(proc);
        if (isARM64()) {
            std::string regex(".*eon.*,.*,.*,.*lsr #");
            regex += std::to_string(amount) + ".*";
            checkUsesInstruction(*code, regex.c_str(), true);
        }
        return invoke<uint64_t>(*code, n, m);
    };

    for (auto nOperand : int64Operands()) {
        for (auto mOperand : int64Operands()) {
            for (auto amount : amounts) {
                uint64_t n = nOperand.value;
                uint64_t m = mOperand.value;
                CHECK_EQ(test(n, m, amount), n ^ ((m >> amount) ^ -1));
            }
        }
    }
}

void testBitfieldZeroExtend32()
{
    if (JSC::Options::defaultB3OptLevel() < 2)
        return;
    Vector<uint32_t> amounts = { 0, 14, 31 };

    // Turn this: ZShr(Shl(n, amount)), amount)
    // Into this: BitAnd(n, mask)
    // Conditions:
    // 1. 0 <= amount < datasize
    // 2. width = datasize - amount
    // 3. mask is !(mask & (mask + 1)) where bitCount(mask) == width
    auto test = [&] (uint32_t n, uint32_t amount) -> uint32_t {
        Procedure proc;
        BasicBlock* root = proc.addBlock();

        Value* nValue = root->appendNew<Value>(
            proc, Trunc, Origin(), 
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
        Value* amountValue = root->appendNew<Const32Value>(proc, Origin(), amount);
        Value* shlValue = root->appendNew<Value>(proc, Shl, Origin(), nValue, amountValue);
        Value* zshrValue = root->appendNew<Value>(proc, ZShr, Origin(), shlValue, amountValue);
        root->appendNewControlValue(proc, Return, Origin(), zshrValue);

        auto code = compileProc(proc);
        if (isARM64() && amount > 0)
            checkUsesInstruction(*code, "and");
        return invoke<uint32_t>(*code, n, amount);
    };

    uint32_t datasize = CHAR_BIT * sizeof(uint32_t);
    for (auto nOperand : int32Operands()) {
        for (auto amount : amounts) {
            uint32_t n = nOperand.value;
            uint32_t width = datasize - amount;
            uint32_t mask = width == datasize ? std::numeric_limits<uint32_t>::max() : (1U << width) - 1U;
            uint32_t lhs = test(n, amount);
            uint32_t rhs = (n & mask);
            CHECK(lhs == rhs);
        }
    }
}

void testBitfieldZeroExtend64()
{
    if (JSC::Options::defaultB3OptLevel() < 2)
        return;
    Vector<uint64_t> amounts = { 0, 34, 63 };

    auto test = [&] (uint64_t n, uint64_t amount) -> uint64_t {
        Procedure proc;
        BasicBlock* root = proc.addBlock();

        Value* nValue = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
        Value* amountValue = root->appendNew<Const32Value>(proc, Origin(), amount);
        Value* shlValue = root->appendNew<Value>(proc, Shl, Origin(), nValue, amountValue);
        Value* zshrValue = root->appendNew<Value>(proc, ZShr, Origin(), shlValue, amountValue);
        root->appendNewControlValue(proc, Return, Origin(), zshrValue);

        auto code = compileProc(proc);
        if (isARM64() && amount > 0)
            checkUsesInstruction(*code, "and");
        return invoke<uint64_t>(*code, n, amount);
    };

    uint64_t datasize = CHAR_BIT * sizeof(uint64_t);
    for (auto nOperand : int64Operands()) {
        for (auto amount : amounts) {
            uint64_t n = nOperand.value;
            uint64_t width = datasize - amount;
            uint64_t mask = width == datasize ? std::numeric_limits<uint64_t>::max() : (1ULL << width) - 1ULL;
            uint64_t lhs = test(n, amount);
            uint64_t rhs = (n & mask);
            CHECK(lhs == rhs);
        }
    }
}

void testExtractRegister32()
{
    if (JSC::Options::defaultB3OptLevel() < 2)
        return;    
    Vector<uint32_t> lowWidths = { 0, 17, 31 };

    // Test Pattern: ((n & mask1) << highWidth) | ((m & mask2) >> lowWidth)
    // Where: highWidth = datasize - lowWidth
    //        mask1 = (1 << lowWidth) - 1
    //        mask2 = ~mask1
    auto test = [&] (uint32_t n, uint32_t m, uint32_t mask1, uint32_t mask2, uint32_t highWidth, uint32_t lowWidth) -> uint32_t {
        Procedure proc;
        BasicBlock* root = proc.addBlock();

        Value* nValue = root->appendNew<Value>(
            proc, Trunc, Origin(), 
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
        Value* mValue = root->appendNew<Value>(
            proc, Trunc, Origin(), 
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1));
        Value* mask1Value = root->appendNew<Const32Value>(proc, Origin(), mask1);
        Value* mask2Value = root->appendNew<Const32Value>(proc, Origin(), mask2);
        Value* highWidthValue = root->appendNew<Const32Value>(proc, Origin(), highWidth);
        Value* lowWidthValue = root->appendNew<Const32Value>(proc, Origin(), lowWidth);

        Value* leftAndValue = root->appendNew<Value>(proc, BitAnd, Origin(), nValue, mask1Value);
        Value* left = root->appendNew<Value>(proc, Shl, Origin(), leftAndValue, highWidthValue);

        Value* rightAndValue = root->appendNew<Value>(proc, BitAnd, Origin(), mValue, mask2Value);
        Value* right = root->appendNew<Value>(proc, ZShr, Origin(), rightAndValue, lowWidthValue);

        root->appendNewControlValue(
            proc, Return, Origin(), 
            root->appendNew<Value>(proc, BitOr, Origin(), left, right));

        auto code = compileProc(proc);
        if (isARM64() && lowWidth > 0)
            checkUsesInstruction(*code, "extr");
        return invoke<uint32_t>(*code, n, m);
    };

    uint32_t datasize = CHAR_BIT * sizeof(uint32_t);
    for (auto nOperand : int32Operands()) {
        for (auto mOperand : int32Operands()) {
            for (auto lowWidth : lowWidths) {
                uint32_t n = nOperand.value;
                uint32_t m = mOperand.value;
                uint32_t highWidth = datasize - lowWidth;
                uint32_t mask1 = (1U << lowWidth) - 1U;
                uint32_t mask2 = ~mask1;
                uint32_t left = highWidth == datasize ? 0U : ((n & mask1) << highWidth);
                uint32_t right = ((m & mask2) >> lowWidth);
                uint32_t rhs = left | right;
                uint32_t lhs = test(n, m, mask1, mask2, highWidth, lowWidth);
                CHECK(lhs == rhs);
            }
        }
    }
}

void testExtractRegister64()
{
    if (JSC::Options::defaultB3OptLevel() < 2)
        return;    
    Vector<uint64_t> lowWidths = { 0, 34, 63 };

    // Test Pattern: ((n & mask1) << highWidth) | ((m & mask2) >> lowWidth)
    // Where: highWidth = datasize - lowWidth
    //        mask1 = (1 << lowWidth) - 1
    //        mask2 = ~mask1
    auto test = [&] (uint64_t n, uint64_t m, uint64_t mask1, uint64_t mask2, uint64_t highWidth, uint64_t lowWidth) -> uint64_t {
        Procedure proc;
        BasicBlock* root = proc.addBlock();

        Value* nValue = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
        Value* mValue = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
        Value* mask1Value = root->appendNew<Const64Value>(proc, Origin(), mask1);
        Value* mask2Value = root->appendNew<Const64Value>(proc, Origin(), mask2);
        Value* highWidthValue = root->appendNew<Const32Value>(proc, Origin(), highWidth);
        Value* lowWidthValue = root->appendNew<Const32Value>(proc, Origin(), lowWidth);

        Value* leftAndValue = root->appendNew<Value>(proc, BitAnd, Origin(), nValue, mask1Value);
        Value* left = root->appendNew<Value>(proc, Shl, Origin(), leftAndValue, highWidthValue);

        Value* rightAndValue = root->appendNew<Value>(proc, BitAnd, Origin(), mValue, mask2Value);
        Value* right = root->appendNew<Value>(proc, ZShr, Origin(), rightAndValue, lowWidthValue);

        root->appendNewControlValue(
            proc, Return, Origin(), 
            root->appendNew<Value>(proc, BitOr, Origin(), left, right));

        auto code = compileProc(proc);
        if (isARM64() && lowWidth > 0)
            checkUsesInstruction(*code, "extr");
        return invoke<uint64_t>(*code, n, m);
    };

    uint64_t datasize = CHAR_BIT * sizeof(uint64_t);
    for (auto nOperand : int64Operands()) {
        for (auto mOperand : int64Operands()) {
            for (auto lowWidth : lowWidths) {
                uint64_t n = nOperand.value;
                uint64_t m = mOperand.value;
                uint64_t highWidth = datasize - lowWidth;
                uint64_t mask1 = (1ULL << lowWidth) - 1ULL;
                uint64_t mask2 = ~mask1;
                uint64_t left = highWidth == datasize ? 0ULL : ((n & mask1) << highWidth);
                uint64_t right = ((m & mask2) >> lowWidth);
                uint64_t rhs = left | right;
                uint64_t lhs = test(n, m, mask1, mask2, highWidth, lowWidth);
                CHECK(lhs == rhs);
            }
        }
    }
}

void testAddWithLeftShift32()
{
    if (JSC::Options::defaultB3OptLevel() < 2)
        return;
    Vector<int32_t> amounts = { 1, 17, 31 };

    // Test Pattern: d = n + (m << amount)
    auto test = [&] (int32_t n, int32_t m, int32_t amount) -> int32_t {
        Procedure proc;
        BasicBlock* root = proc.addBlock();

        Value* nValue = root->appendNew<Value>(
            proc, Trunc, Origin(), 
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
        Value* mValue = root->appendNew<Value>(
            proc, Trunc, Origin(), 
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1));
        Value* amountValue = root->appendNew<Const32Value>(proc, Origin(), amount);

        Value* shiftValue = root->appendNew<Value>(proc, Shl, Origin(), mValue, amountValue);
        root->appendNewControlValue(
            proc, Return, Origin(), 
            root->appendNew<Value>(proc, Add, Origin(), nValue, shiftValue));

        auto code = compileProc(proc);
        if (isARM64()) {
            std::string regex(".*add.*,.*,.*,.*lsl #");
            regex += std::to_string(amount) + ".*";
            checkUsesInstruction(*code, regex.c_str(), true);
        }
        return invoke<int32_t>(*code, n, m);
    };

    for (auto nOperand : int32Operands()) {
        for (auto mOperand : int32Operands()) {
            for (auto amount : amounts) {
                int32_t n = nOperand.value;
                int32_t m = mOperand.value;
                CHECK_EQ(test(n, m, amount), n + (m << amount));
            }
        }
    }
}

void testAddWithRightShift32()
{
    if (JSC::Options::defaultB3OptLevel() < 2)
        return;
    Vector<int32_t> amounts = { 1, 17, 31 };

    // Test Pattern: d = n + (m >> amount)
    auto test = [&] (int32_t n, int32_t m, int32_t amount) -> int32_t {
        Procedure proc;
        BasicBlock* root = proc.addBlock();

        Value* nValue = root->appendNew<Value>(
            proc, Trunc, Origin(), 
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
        Value* mValue = root->appendNew<Value>(
            proc, Trunc, Origin(), 
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1));
        Value* amountValue = root->appendNew<Const32Value>(proc, Origin(), amount);

        Value* shiftValue = root->appendNew<Value>(proc, SShr, Origin(), mValue, amountValue);
        root->appendNewControlValue(
            proc, Return, Origin(), 
            root->appendNew<Value>(proc, Add, Origin(), nValue, shiftValue));

        auto code = compileProc(proc);
        if (isARM64()) {
            std::string regex(".*add.*,.*,.*,.*asr #");
            regex += std::to_string(amount) + ".*";
            checkUsesInstruction(*code, regex.c_str(), true);
        }
        return invoke<int32_t>(*code, n, m);
    };

    for (auto nOperand : int32Operands()) {
        for (auto mOperand : int32Operands()) {
            for (auto amount : amounts) {
                int32_t n = nOperand.value;
                int32_t m = mOperand.value;
                CHECK_EQ(test(n, m, amount), n + (m >> amount));
            }
        }
    }
}

void testAddWithUnsignedRightShift32()
{
    if (JSC::Options::defaultB3OptLevel() < 2)
        return;
    Vector<uint32_t> amounts = { 1, 17, 31 };

    // Test Pattern: d = n + (m >> amount)
    auto test = [&] (uint32_t n, uint32_t m, uint32_t amount) -> uint32_t {
        Procedure proc;
        BasicBlock* root = proc.addBlock();

        Value* nValue = root->appendNew<Value>(
            proc, Trunc, Origin(), 
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
        Value* mValue = root->appendNew<Value>(
            proc, Trunc, Origin(), 
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1));
        Value* amountValue = root->appendNew<Const32Value>(proc, Origin(), amount);

        Value* shiftValue = root->appendNew<Value>(proc, ZShr, Origin(), mValue, amountValue);
        root->appendNewControlValue(
            proc, Return, Origin(), 
            root->appendNew<Value>(proc, Add, Origin(), nValue, shiftValue));

        auto code = compileProc(proc);
        if (isARM64()) {
            std::string regex(".*add.*,.*,.*,.*lsr #");
            regex += std::to_string(amount) + ".*";
            checkUsesInstruction(*code, regex.c_str(), true);
        }
        return invoke<uint32_t>(*code, n, m);
    };

    for (auto nOperand : int32Operands()) {
        for (auto mOperand : int32Operands()) {
            for (auto amount : amounts) {
                uint32_t n = nOperand.value;
                uint32_t m = mOperand.value;
                CHECK_EQ(test(n, m, amount), n + (m >> amount));
            }
        }
    }
}

void testAddWithLeftShift64()
{
    if (JSC::Options::defaultB3OptLevel() < 2)
        return;
    Vector<int32_t> amounts = { 1, 34, 63 };

    // Test Pattern: d = n + (m << amount)
    auto test = [&] (int64_t n, int64_t m, int32_t amount) -> int64_t {
        Procedure proc;
        BasicBlock* root = proc.addBlock();

        Value* nValue = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
        Value* mValue = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
        Value* amountValue = root->appendNew<Const32Value>(proc, Origin(), amount);

        Value* shiftValue = root->appendNew<Value>(proc, Shl, Origin(), mValue, amountValue);
        root->appendNewControlValue(
            proc, Return, Origin(), 
            root->appendNew<Value>(proc, Add, Origin(), nValue, shiftValue));

        auto code = compileProc(proc);
        if (isARM64()) {
            std::string regex(".*add.*,.*,.*,.*lsl #");
            regex += std::to_string(amount) + ".*";
            checkUsesInstruction(*code, regex.c_str(), true);
        }
        return invoke<int64_t>(*code, n, m);
    };

    for (auto nOperand : int64Operands()) {
        for (auto mOperand : int64Operands()) {
            for (auto amount : amounts) {
                int64_t n = nOperand.value;
                int64_t m = mOperand.value;
                CHECK_EQ(test(n, m, amount), n + (m << amount));
            }
        }
    }
}

void testAddWithRightShift64()
{
    if (JSC::Options::defaultB3OptLevel() < 2)
        return;
    Vector<int32_t> amounts = { 1, 34, 63 };

    // Test Pattern: d = n + (m >> amount)
    auto test = [&] (int64_t n, int64_t m, int32_t amount) -> int64_t {
        Procedure proc;
        BasicBlock* root = proc.addBlock();

        Value* nValue = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
        Value* mValue = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
        Value* amountValue = root->appendNew<Const32Value>(proc, Origin(), amount);

        Value* shiftValue = root->appendNew<Value>(proc, SShr, Origin(), mValue, amountValue);
        root->appendNewControlValue(
            proc, Return, Origin(), 
            root->appendNew<Value>(proc, Add, Origin(), nValue, shiftValue));

        auto code = compileProc(proc);
        if (isARM64()) {
            std::string regex(".*add.*,.*,.*,.*asr #");
            regex += std::to_string(amount) + ".*";
            checkUsesInstruction(*code, regex.c_str(), true);
        }
        return invoke<int64_t>(*code, n, m);
    };

    for (auto nOperand : int64Operands()) {
        for (auto mOperand : int64Operands()) {
            for (auto amount : amounts) {
                int64_t n = nOperand.value;
                int64_t m = mOperand.value;
                CHECK_EQ(test(n, m, amount), n + (m >> amount));
            }
        }
    }
}

void testAddWithUnsignedRightShift64()
{
    if (JSC::Options::defaultB3OptLevel() < 2)
        return;
    Vector<uint32_t> amounts = { 1, 17, 31 };

    // Test Pattern: d = n + (m >> amount)
    auto test = [&] (uint64_t n, uint64_t m, uint32_t amount) -> uint64_t {
        Procedure proc;
        BasicBlock* root = proc.addBlock();

        Value* nValue = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
        Value* mValue = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
        Value* amountValue = root->appendNew<Const32Value>(proc, Origin(), amount);

        Value* shiftValue = root->appendNew<Value>(proc, ZShr, Origin(), mValue, amountValue);
        root->appendNewControlValue(
            proc, Return, Origin(), 
            root->appendNew<Value>(proc, Add, Origin(), nValue, shiftValue));

        auto code = compileProc(proc);
        if (isARM64()) {
            std::string regex(".*add.*,.*,.*,.*lsr #");
            regex += std::to_string(amount) + ".*";
            checkUsesInstruction(*code, regex.c_str(), true);
        }
        return invoke<uint64_t>(*code, n, m);
    };

    for (auto nOperand : int64Operands()) {
        for (auto mOperand : int64Operands()) {
            for (auto amount : amounts) {
                uint64_t n = nOperand.value;
                uint64_t m = mOperand.value;
                CHECK_EQ(test(n, m, amount), n + (m >> amount));
            }
        }
    }
}

void testSubWithLeftShift32()
{
    if (JSC::Options::defaultB3OptLevel() < 2)
        return;
    Vector<int32_t> amounts = { 1, 17, 31 };

    // Test Pattern: d = n - (m << amount)
    auto test = [&] (int32_t n, int32_t m, int32_t amount) -> int32_t {
        Procedure proc;
        BasicBlock* root = proc.addBlock();

        Value* nValue = root->appendNew<Value>(
            proc, Trunc, Origin(), 
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
        Value* mValue = root->appendNew<Value>(
            proc, Trunc, Origin(), 
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1));
        Value* amountValue = root->appendNew<Const32Value>(proc, Origin(), amount);

        Value* shiftValue = root->appendNew<Value>(proc, Shl, Origin(), mValue, amountValue);
        root->appendNewControlValue(
            proc, Return, Origin(), 
            root->appendNew<Value>(proc, Sub, Origin(), nValue, shiftValue));

        auto code = compileProc(proc);
        if (isARM64()) {
            std::string regex(".*sub.*,.*,.*,.*lsl #");
            regex += std::to_string(amount) + ".*";
            checkUsesInstruction(*code, regex.c_str(), true);
        }
        return invoke<int32_t>(*code, n, m);
    };

    for (auto nOperand : int32Operands()) {
        for (auto mOperand : int32Operands()) {
            for (auto amount : amounts) {
                int32_t n = nOperand.value;
                int32_t m = mOperand.value;
                CHECK_EQ(test(n, m, amount), n - (m << amount));
            }
        }
    }
}

void testSubWithRightShift32()
{
    if (JSC::Options::defaultB3OptLevel() < 2)
        return;
    Vector<int32_t> amounts = { 1, 17, 31 };

    // Test Pattern: d = n - (m >> amount)
    auto test = [&] (int32_t n, int32_t m, int32_t amount) -> int32_t {
        Procedure proc;
        BasicBlock* root = proc.addBlock();

        Value* nValue = root->appendNew<Value>(
            proc, Trunc, Origin(), 
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
        Value* mValue = root->appendNew<Value>(
            proc, Trunc, Origin(), 
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1));
        Value* amountValue = root->appendNew<Const32Value>(proc, Origin(), amount);

        Value* shiftValue = root->appendNew<Value>(proc, SShr, Origin(), mValue, amountValue);
        root->appendNewControlValue(
            proc, Return, Origin(), 
            root->appendNew<Value>(proc, Sub, Origin(), nValue, shiftValue));

        auto code = compileProc(proc);
        if (isARM64()) {
            std::string regex(".*sub.*,.*,.*,.*asr #");
            regex += std::to_string(amount) + ".*";
            checkUsesInstruction(*code, regex.c_str(), true);
        }
        return invoke<int32_t>(*code, n, m);
    };

    for (auto nOperand : int32Operands()) {
        for (auto mOperand : int32Operands()) {
            for (auto amount : amounts) {
                int32_t n = nOperand.value;
                int32_t m = mOperand.value;
                CHECK_EQ(test(n, m, amount), n - (m >> amount));
            }
        }
    }
}

void testSubWithUnsignedRightShift32()
{
    if (JSC::Options::defaultB3OptLevel() < 2)
        return;
    Vector<uint32_t> amounts = { 1, 17, 31 };

    // Test Pattern: d = n - (m >> amount)
    auto test = [&] (uint32_t n, uint32_t m, uint32_t amount) -> uint32_t {
        Procedure proc;
        BasicBlock* root = proc.addBlock();

        Value* nValue = root->appendNew<Value>(
            proc, Trunc, Origin(), 
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
        Value* mValue = root->appendNew<Value>(
            proc, Trunc, Origin(), 
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1));
        Value* amountValue = root->appendNew<Const32Value>(proc, Origin(), amount);

        Value* shiftValue = root->appendNew<Value>(proc, ZShr, Origin(), mValue, amountValue);
        root->appendNewControlValue(
            proc, Return, Origin(), 
            root->appendNew<Value>(proc, Sub, Origin(), nValue, shiftValue));

        auto code = compileProc(proc);
        if (isARM64()) {
            std::string regex(".*sub.*,.*,.*,.*lsr #");
            regex += std::to_string(amount) + ".*";
            checkUsesInstruction(*code, regex.c_str(), true);
        }
        return invoke<uint32_t>(*code, n, m);
    };

    for (auto nOperand : int32Operands()) {
        for (auto mOperand : int32Operands()) {
            for (auto amount : amounts) {
                uint32_t n = nOperand.value;
                uint32_t m = mOperand.value;
                CHECK_EQ(test(n, m, amount), n - (m >> amount));
            }
        }
    }
}

void testSubWithLeftShift64()
{
    if (JSC::Options::defaultB3OptLevel() < 2)
        return;
    Vector<int32_t> amounts = { 1, 34, 63 };

    // Test Pattern: d = n - (m << amount)
    auto test = [&] (int64_t n, int64_t m, int32_t amount) -> int64_t {
        Procedure proc;
        BasicBlock* root = proc.addBlock();

        Value* nValue = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
        Value* mValue = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
        Value* amountValue = root->appendNew<Const32Value>(proc, Origin(), amount);

        Value* shiftValue = root->appendNew<Value>(proc, Shl, Origin(), mValue, amountValue);
        root->appendNewControlValue(
            proc, Return, Origin(), 
            root->appendNew<Value>(proc, Sub, Origin(), nValue, shiftValue));

        auto code = compileProc(proc);
        if (isARM64()) {
            std::string regex(".*sub.*,.*,.*,.*lsl #");
            regex += std::to_string(amount) + ".*";
            checkUsesInstruction(*code, regex.c_str(), true);
        }
        return invoke<int64_t>(*code, n, m);
    };

    for (auto nOperand : int64Operands()) {
        for (auto mOperand : int64Operands()) {
            for (auto amount : amounts) {
                int64_t n = nOperand.value;
                int64_t m = mOperand.value;
                CHECK_EQ(test(n, m, amount), n - (m << amount));
            }
        }
    }
}

void testSubWithRightShift64()
{
    if (JSC::Options::defaultB3OptLevel() < 2)
        return;
    Vector<int32_t> amounts = { 1, 34, 63 };

    // Test Pattern: d = n - (m >> amount)
    auto test = [&] (int64_t n, int64_t m, int32_t amount) -> int64_t {
        Procedure proc;
        BasicBlock* root = proc.addBlock();

        Value* nValue = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
        Value* mValue = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
        Value* amountValue = root->appendNew<Const32Value>(proc, Origin(), amount);

        Value* shiftValue = root->appendNew<Value>(proc, SShr, Origin(), mValue, amountValue);
        root->appendNewControlValue(
            proc, Return, Origin(), 
            root->appendNew<Value>(proc, Sub, Origin(), nValue, shiftValue));

        auto code = compileProc(proc);
        if (isARM64()) {
            std::string regex(".*sub.*,.*,.*,.*asr #");
            regex += std::to_string(amount) + ".*";
            checkUsesInstruction(*code, regex.c_str(), true);
        }
        return invoke<int64_t>(*code, n, m);
    };

    for (auto nOperand : int64Operands()) {
        for (auto mOperand : int64Operands()) {
            for (auto amount : amounts) {
                int64_t n = nOperand.value;
                int64_t m = mOperand.value;
                CHECK_EQ(test(n, m, amount), n - (m >> amount));
            }
        }
    }
}

void testSubWithUnsignedRightShift64()
{
    if (JSC::Options::defaultB3OptLevel() < 2)
        return;
    Vector<int32_t> amounts = { 1, 34, 63 };

    // Test Pattern: d = n - (m >> amount)
    auto test = [&] (uint64_t n, uint64_t m, uint32_t amount) -> uint64_t {
        Procedure proc;
        BasicBlock* root = proc.addBlock();

        Value* nValue = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
        Value* mValue = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
        Value* amountValue = root->appendNew<Const32Value>(proc, Origin(), amount);

        Value* shiftValue = root->appendNew<Value>(proc, ZShr, Origin(), mValue, amountValue);
        root->appendNewControlValue(
            proc, Return, Origin(), 
            root->appendNew<Value>(proc, Sub, Origin(), nValue, shiftValue));

        auto code = compileProc(proc);
        if (isARM64()) {
            std::string regex(".*sub.*,.*,.*,.*lsr #");
            regex += std::to_string(amount) + ".*";
            checkUsesInstruction(*code, regex.c_str(), true);
        }
        return invoke<uint64_t>(*code, n, m);
    };

    for (auto nOperand : int64Operands()) {
        for (auto mOperand : int64Operands()) {
            for (auto amount : amounts) {
                uint64_t n = nOperand.value;
                uint64_t m = mOperand.value;
                CHECK_EQ(test(n, m, amount), n - (m >> amount));
            }
        }
    }
}

void testAndLeftShift32()
{
    if (JSC::Options::defaultB3OptLevel() < 2)
        return;
    Vector<int32_t> amounts = { 1, 17, 31 };

    // Test Pattern: d = n & (m << amount)
    auto test = [&] (int32_t n, int32_t m, int32_t amount) -> int32_t {
        Procedure proc;
        BasicBlock* root = proc.addBlock();

        Value* nValue = root->appendNew<Value>(
            proc, Trunc, Origin(), 
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
        Value* mValue = root->appendNew<Value>(
            proc, Trunc, Origin(), 
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1));
        Value* amountValue = root->appendNew<Const32Value>(proc, Origin(), amount);

        Value* shiftValue = root->appendNew<Value>(proc, Shl, Origin(), mValue, amountValue);
        root->appendNewControlValue(
            proc, Return, Origin(), 
            root->appendNew<Value>(proc, BitAnd, Origin(), nValue, shiftValue));

        auto code = compileProc(proc);
        if (isARM64()) {
            std::string regex(".*and.*,.*,.*,.*lsl #");
            regex += std::to_string(amount) + ".*";
            checkUsesInstruction(*code, regex.c_str(), true);
        }
        return invoke<int32_t>(*code, n, m);
    };

    for (auto nOperand : int32Operands()) {
        for (auto mOperand : int32Operands()) {
            for (auto amount : amounts) {
                int32_t n = nOperand.value;
                int32_t m = mOperand.value;
                CHECK_EQ(test(n, m, amount), n & (m << amount));
            }
        }
    }
}

void testAndRightShift32()
{
    if (JSC::Options::defaultB3OptLevel() < 2)
        return;
    Vector<int32_t> amounts = { 1, 17, 31 };

    // Test Pattern: d = n & (m >> amount)
    auto test = [&] (int32_t n, int32_t m, int32_t amount) -> int32_t {
        Procedure proc;
        BasicBlock* root = proc.addBlock();

        Value* nValue = root->appendNew<Value>(
            proc, Trunc, Origin(), 
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
        Value* mValue = root->appendNew<Value>(
            proc, Trunc, Origin(), 
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1));
        Value* amountValue = root->appendNew<Const32Value>(proc, Origin(), amount);

        Value* shiftValue = root->appendNew<Value>(proc, SShr, Origin(), mValue, amountValue);
        root->appendNewControlValue(
            proc, Return, Origin(), 
            root->appendNew<Value>(proc, BitAnd, Origin(), nValue, shiftValue));

        auto code = compileProc(proc);
        if (isARM64()) {
            std::string regex(".*and.*,.*,.*,.*asr #");
            regex += std::to_string(amount) + ".*";
            checkUsesInstruction(*code, regex.c_str(), true);
        }
        return invoke<int32_t>(*code, n, m);
    };

    for (auto nOperand : int32Operands()) {
        for (auto mOperand : int32Operands()) {
            for (auto amount : amounts) {
                int32_t n = nOperand.value;
                int32_t m = mOperand.value;
                CHECK_EQ(test(n, m, amount), n & (m >> amount));
            }
        }
    }
}

void testAndUnsignedRightShift32()
{
    if (JSC::Options::defaultB3OptLevel() < 2)
        return;
    Vector<uint32_t> amounts = { 1, 17, 31 };

    // Test Pattern: d = n & (m >> amount)
    auto test = [&] (uint32_t n, uint32_t m, uint32_t amount) -> uint32_t {
        Procedure proc;
        BasicBlock* root = proc.addBlock();

        Value* nValue = root->appendNew<Value>(
            proc, Trunc, Origin(), 
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
        Value* mValue = root->appendNew<Value>(
            proc, Trunc, Origin(), 
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1));
        Value* amountValue = root->appendNew<Const32Value>(proc, Origin(), amount);

        Value* shiftValue = root->appendNew<Value>(proc, ZShr, Origin(), mValue, amountValue);
        root->appendNewControlValue(
            proc, Return, Origin(), 
            root->appendNew<Value>(proc, BitAnd, Origin(), nValue, shiftValue));

        auto code = compileProc(proc);
        if (isARM64()) {
            std::string regex(".*and.*,.*,.*,.*lsr #");
            regex += std::to_string(amount) + ".*";
            checkUsesInstruction(*code, regex.c_str(), true);
        }
        return invoke<uint32_t>(*code, n, m);
    };

    for (auto nOperand : int32Operands()) {
        for (auto mOperand : int32Operands()) {
            for (auto amount : amounts) {
                uint32_t n = nOperand.value;
                uint32_t m = mOperand.value;
                CHECK_EQ(test(n, m, amount), n & (m >> amount));
            }
        }
    }
}

void testAndLeftShift64()
{
    if (JSC::Options::defaultB3OptLevel() < 2)
        return;
    Vector<int32_t> amounts = { 1, 34, 63 };

    // Test Pattern: d = n & (m << amount)
    auto test = [&] (int64_t n, int64_t m, int32_t amount) -> int64_t {
        Procedure proc;
        BasicBlock* root = proc.addBlock();

        Value* nValue = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
        Value* mValue = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
        Value* amountValue = root->appendNew<Const32Value>(proc, Origin(), amount);

        Value* shiftValue = root->appendNew<Value>(proc, Shl, Origin(), mValue, amountValue);
        root->appendNewControlValue(
            proc, Return, Origin(), 
            root->appendNew<Value>(proc, BitAnd, Origin(), nValue, shiftValue));

        auto code = compileProc(proc);
        if (isARM64()) {
            std::string regex(".*and.*,.*,.*,.*lsl #");
            regex += std::to_string(amount) + ".*";
            checkUsesInstruction(*code, regex.c_str(), true);
        }
        return invoke<int64_t>(*code, n, m);
    };

    for (auto nOperand : int64Operands()) {
        for (auto mOperand : int64Operands()) {
            for (auto amount : amounts) {
                int64_t n = nOperand.value;
                int64_t m = mOperand.value;
                CHECK_EQ(test(n, m, amount), n & (m << amount));
            }
        }
    }
}

void testAndRightShift64()
{
    if (JSC::Options::defaultB3OptLevel() < 2)
        return;
    Vector<int32_t> amounts = { 1, 34, 63 };

    // Test Pattern: d = n & (m >> amount)
    auto test = [&] (int64_t n, int64_t m, int32_t amount) -> int64_t {
        Procedure proc;
        BasicBlock* root = proc.addBlock();

        Value* nValue = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
        Value* mValue = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
        Value* amountValue = root->appendNew<Const32Value>(proc, Origin(), amount);

        Value* shiftValue = root->appendNew<Value>(proc, SShr, Origin(), mValue, amountValue);
        root->appendNewControlValue(
            proc, Return, Origin(), 
            root->appendNew<Value>(proc, BitAnd, Origin(), nValue, shiftValue));

        auto code = compileProc(proc);
        if (isARM64()) {
            std::string regex(".*and.*,.*,.*,.*asr #");
            regex += std::to_string(amount) + ".*";
            checkUsesInstruction(*code, regex.c_str(), true);
        }
        return invoke<int64_t>(*code, n, m);
    };

    for (auto nOperand : int64Operands()) {
        for (auto mOperand : int64Operands()) {
            for (auto amount : amounts) {
                int64_t n = nOperand.value;
                int64_t m = mOperand.value;
                CHECK_EQ(test(n, m, amount), n & (m >> amount));
            }
        }
    }
}

void testAndUnsignedRightShift64()
{
    if (JSC::Options::defaultB3OptLevel() < 2)
        return;
    Vector<uint32_t> amounts = { 1, 17, 31 };

    // Test Pattern: d = n & (m >> amount)
    auto test = [&] (uint64_t n, uint64_t m, uint32_t amount) -> uint64_t {
        Procedure proc;
        BasicBlock* root = proc.addBlock();

        Value* nValue = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
        Value* mValue = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
        Value* amountValue = root->appendNew<Const32Value>(proc, Origin(), amount);

        Value* shiftValue = root->appendNew<Value>(proc, ZShr, Origin(), mValue, amountValue);
        root->appendNewControlValue(
            proc, Return, Origin(), 
            root->appendNew<Value>(proc, BitAnd, Origin(), nValue, shiftValue));

        auto code = compileProc(proc);
        if (isARM64()) {
            std::string regex(".*and.*,.*,.*,.*lsr #");
            regex += std::to_string(amount) + ".*";
            checkUsesInstruction(*code, regex.c_str(), true);
        }
        return invoke<uint64_t>(*code, n, m);
    };

    for (auto nOperand : int64Operands()) {
        for (auto mOperand : int64Operands()) {
            for (auto amount : amounts) {
                uint64_t n = nOperand.value;
                uint64_t m = mOperand.value;
                CHECK_EQ(test(n, m, amount), n & (m >> amount));
            }
        }
    }
}

void testXorLeftShift32()
{
    if (JSC::Options::defaultB3OptLevel() < 2)
        return;
    Vector<int32_t> amounts = { 1, 17, 31 };

    // Test Pattern: d = n ^ (m << amount)
    auto test = [&] (int32_t n, int32_t m, int32_t amount) -> int32_t {
        Procedure proc;
        BasicBlock* root = proc.addBlock();

        Value* nValue = root->appendNew<Value>(
            proc, Trunc, Origin(), 
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
        Value* mValue = root->appendNew<Value>(
            proc, Trunc, Origin(), 
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1));
        Value* amountValue = root->appendNew<Const32Value>(proc, Origin(), amount);

        Value* shiftValue = root->appendNew<Value>(proc, Shl, Origin(), mValue, amountValue);
        root->appendNewControlValue(
            proc, Return, Origin(), 
            root->appendNew<Value>(proc, BitXor, Origin(), nValue, shiftValue));

        auto code = compileProc(proc);
        if (isARM64()) {
            std::string regex(".*eor.*,.*,.*,.*lsl #");
            regex += std::to_string(amount) + ".*";
            checkUsesInstruction(*code, regex.c_str(), true);
        }
        return invoke<int32_t>(*code, n, m);
    };

    for (auto nOperand : int32Operands()) {
        for (auto mOperand : int32Operands()) {
            for (auto amount : amounts) {
                int32_t n = nOperand.value;
                int32_t m = mOperand.value;
                CHECK_EQ(test(n, m, amount), n ^ (m << amount));
            }
        }
    }
}

void testXorRightShift32()
{
    if (JSC::Options::defaultB3OptLevel() < 2)
        return;
    Vector<int32_t> amounts = { 1, 17, 31 };

    // Test Pattern: d = n ^ (m >> amount)
    auto test = [&] (int32_t n, int32_t m, int32_t amount) -> int32_t {
        Procedure proc;
        BasicBlock* root = proc.addBlock();

        Value* nValue = root->appendNew<Value>(
            proc, Trunc, Origin(), 
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
        Value* mValue = root->appendNew<Value>(
            proc, Trunc, Origin(), 
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1));
        Value* amountValue = root->appendNew<Const32Value>(proc, Origin(), amount);

        Value* shiftValue = root->appendNew<Value>(proc, SShr, Origin(), mValue, amountValue);
        root->appendNewControlValue(
            proc, Return, Origin(), 
            root->appendNew<Value>(proc, BitXor, Origin(), nValue, shiftValue));

        auto code = compileProc(proc);
        if (isARM64()) {
            std::string regex(".*eor.*,.*,.*,.*asr #");
            regex += std::to_string(amount) + ".*";
            checkUsesInstruction(*code, regex.c_str(), true);
        }
        return invoke<int32_t>(*code, n, m);
    };

    for (auto nOperand : int32Operands()) {
        for (auto mOperand : int32Operands()) {
            for (auto amount : amounts) {
                int32_t n = nOperand.value;
                int32_t m = mOperand.value;
                CHECK_EQ(test(n, m, amount), n ^ (m >> amount));
            }
        }
    }
}

void testXorUnsignedRightShift32()
{
    if (JSC::Options::defaultB3OptLevel() < 2)
        return;
    Vector<uint32_t> amounts = { 1, 17, 31 };

    // Test Pattern: d = n ^ (m >> amount)
    auto test = [&] (uint32_t n, uint32_t m, uint32_t amount) -> uint32_t {
        Procedure proc;
        BasicBlock* root = proc.addBlock();

        Value* nValue = root->appendNew<Value>(
            proc, Trunc, Origin(), 
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
        Value* mValue = root->appendNew<Value>(
            proc, Trunc, Origin(), 
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1));
        Value* amountValue = root->appendNew<Const32Value>(proc, Origin(), amount);

        Value* shiftValue = root->appendNew<Value>(proc, ZShr, Origin(), mValue, amountValue);
        root->appendNewControlValue(
            proc, Return, Origin(), 
            root->appendNew<Value>(proc, BitXor, Origin(), nValue, shiftValue));

        auto code = compileProc(proc);
        if (isARM64()) {
            std::string regex(".*eor.*,.*,.*,.*lsr #");
            regex += std::to_string(amount) + ".*";
            checkUsesInstruction(*code, regex.c_str(), true);
        }
        return invoke<uint32_t>(*code, n, m);
    };

    for (auto nOperand : int32Operands()) {
        for (auto mOperand : int32Operands()) {
            for (auto amount : amounts) {
                uint32_t n = nOperand.value;
                uint32_t m = mOperand.value;
                CHECK_EQ(test(n, m, amount), n ^ (m >> amount));
            }
        }
    }
}

void testXorLeftShift64()
{
    if (JSC::Options::defaultB3OptLevel() < 2)
        return;
    Vector<int32_t> amounts = { 1, 34, 63 };

    // Test Pattern: d = n ^ (m << amount)
    auto test = [&] (int64_t n, int64_t m, int32_t amount) -> int64_t {
        Procedure proc;
        BasicBlock* root = proc.addBlock();

        Value* nValue = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
        Value* mValue = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
        Value* amountValue = root->appendNew<Const32Value>(proc, Origin(), amount);

        Value* shiftValue = root->appendNew<Value>(proc, Shl, Origin(), mValue, amountValue);
        root->appendNewControlValue(
            proc, Return, Origin(), 
            root->appendNew<Value>(proc, BitXor, Origin(), nValue, shiftValue));

        auto code = compileProc(proc);
        if (isARM64()) {
            std::string regex(".*eor.*,.*,.*,.*lsl #");
            regex += std::to_string(amount) + ".*";
            checkUsesInstruction(*code, regex.c_str(), true);
        }
        return invoke<int64_t>(*code, n, m);
    };

    for (auto nOperand : int64Operands()) {
        for (auto mOperand : int64Operands()) {
            for (auto amount : amounts) {
                int64_t n = nOperand.value;
                int64_t m = mOperand.value;
                CHECK_EQ(test(n, m, amount), n ^ (m << amount));
            }
        }
    }
}

void testXorRightShift64()
{
    if (JSC::Options::defaultB3OptLevel() < 2)
        return;
    Vector<int32_t> amounts = { 1, 34, 63 };

    // Test Pattern: d = n ^ (m >> amount)
    auto test = [&] (int64_t n, int64_t m, int32_t amount) -> int64_t {
        Procedure proc;
        BasicBlock* root = proc.addBlock();

        Value* nValue = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
        Value* mValue = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
        Value* amountValue = root->appendNew<Const32Value>(proc, Origin(), amount);

        Value* shiftValue = root->appendNew<Value>(proc, SShr, Origin(), mValue, amountValue);
        root->appendNewControlValue(
            proc, Return, Origin(), 
            root->appendNew<Value>(proc, BitXor, Origin(), nValue, shiftValue));

        auto code = compileProc(proc);
        if (isARM64()) {
            std::string regex(".*eor.*,.*,.*,.*asr #");
            regex += std::to_string(amount) + ".*";
            checkUsesInstruction(*code, regex.c_str(), true);
        }
        return invoke<int64_t>(*code, n, m);
    };

    for (auto nOperand : int64Operands()) {
        for (auto mOperand : int64Operands()) {
            for (auto amount : amounts) {
                int64_t n = nOperand.value;
                int64_t m = mOperand.value;
                CHECK_EQ(test(n, m, amount), n ^ (m >> amount));
            }
        }
    }
}

void testXorUnsignedRightShift64()
{
    if (JSC::Options::defaultB3OptLevel() < 2)
        return;
    Vector<uint32_t> amounts = { 1, 17, 31 };

    // Test Pattern: d = n ^ (m >> amount)
    auto test = [&] (uint64_t n, uint64_t m, uint32_t amount) -> uint64_t {
        Procedure proc;
        BasicBlock* root = proc.addBlock();

        Value* nValue = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
        Value* mValue = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
        Value* amountValue = root->appendNew<Const32Value>(proc, Origin(), amount);

        Value* shiftValue = root->appendNew<Value>(proc, ZShr, Origin(), mValue, amountValue);
        root->appendNewControlValue(
            proc, Return, Origin(), 
            root->appendNew<Value>(proc, BitXor, Origin(), nValue, shiftValue));

        auto code = compileProc(proc);
        if (isARM64()) {
            std::string regex(".*eor.*,.*,.*,.*lsr #");
            regex += std::to_string(amount) + ".*";
            checkUsesInstruction(*code, regex.c_str(), true);
        }
        return invoke<uint64_t>(*code, n, m);
    };

    for (auto nOperand : int64Operands()) {
        for (auto mOperand : int64Operands()) {
            for (auto amount : amounts) {
                uint64_t n = nOperand.value;
                uint64_t m = mOperand.value;
                CHECK_EQ(test(n, m, amount), n ^ (m >> amount));
            }
        }
    }
}

void testOrLeftShift32()
{
    if (JSC::Options::defaultB3OptLevel() < 2)
        return;
    Vector<int32_t> amounts = { 1, 17, 31 };

    // Test Pattern: d = n | (m << amount)
    auto test = [&] (int32_t n, int32_t m, int32_t amount) -> int32_t {
        Procedure proc;
        BasicBlock* root = proc.addBlock();

        Value* nValue = root->appendNew<Value>(
            proc, Trunc, Origin(), 
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
        Value* mValue = root->appendNew<Value>(
            proc, Trunc, Origin(), 
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1));
        Value* amountValue = root->appendNew<Const32Value>(proc, Origin(), amount);

        Value* shiftValue = root->appendNew<Value>(proc, Shl, Origin(), mValue, amountValue);
        root->appendNewControlValue(
            proc, Return, Origin(), 
            root->appendNew<Value>(proc, BitOr, Origin(), nValue, shiftValue));

        auto code = compileProc(proc);
        if (isARM64()) {
            std::string regex(".*orr.*,.*,.*,.*lsl #");
            regex += std::to_string(amount) + ".*";
            checkUsesInstruction(*code, regex.c_str(), true);
        }
        return invoke<int32_t>(*code, n, m);
    };

    for (auto nOperand : int32Operands()) {
        for (auto mOperand : int32Operands()) {
            for (auto amount : amounts) {
                int32_t n = nOperand.value;
                int32_t m = mOperand.value;
                CHECK_EQ(test(n, m, amount), n | (m << amount));
            }
        }
    }
}

void testOrRightShift32()
{
    if (JSC::Options::defaultB3OptLevel() < 2)
        return;
    Vector<int32_t> amounts = { 1, 17, 31 };

    // Test Pattern: d = n | (m >> amount)
    auto test = [&] (int32_t n, int32_t m, int32_t amount) -> int32_t {
        Procedure proc;
        BasicBlock* root = proc.addBlock();

        Value* nValue = root->appendNew<Value>(
            proc, Trunc, Origin(), 
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
        Value* mValue = root->appendNew<Value>(
            proc, Trunc, Origin(), 
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1));
        Value* amountValue = root->appendNew<Const32Value>(proc, Origin(), amount);

        Value* shiftValue = root->appendNew<Value>(proc, SShr, Origin(), mValue, amountValue);
        root->appendNewControlValue(
            proc, Return, Origin(), 
            root->appendNew<Value>(proc, BitOr, Origin(), nValue, shiftValue));

        auto code = compileProc(proc);
        if (isARM64()) {
            std::string regex(".*orr.*,.*,.*,.*asr #");
            regex += std::to_string(amount) + ".*";
            checkUsesInstruction(*code, regex.c_str(), true);
        }
        return invoke<int32_t>(*code, n, m);
    };

    for (auto nOperand : int32Operands()) {
        for (auto mOperand : int32Operands()) {
            for (auto amount : amounts) {
                int32_t n = nOperand.value;
                int32_t m = mOperand.value;
                CHECK_EQ(test(n, m, amount), n | (m >> amount));
            }
        }
    }
}

void testOrUnsignedRightShift32()
{
    if (JSC::Options::defaultB3OptLevel() < 2)
        return;
    Vector<uint32_t> amounts = { 1, 17, 31 };

    // Test Pattern: d = n | (m >> amount)
    auto test = [&] (uint32_t n, uint32_t m, uint32_t amount) -> uint32_t {
        Procedure proc;
        BasicBlock* root = proc.addBlock();

        Value* nValue = root->appendNew<Value>(
            proc, Trunc, Origin(), 
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
        Value* mValue = root->appendNew<Value>(
            proc, Trunc, Origin(), 
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1));
        Value* amountValue = root->appendNew<Const32Value>(proc, Origin(), amount);

        Value* shiftValue = root->appendNew<Value>(proc, ZShr, Origin(), mValue, amountValue);
        root->appendNewControlValue(
            proc, Return, Origin(), 
            root->appendNew<Value>(proc, BitOr, Origin(), nValue, shiftValue));

        auto code = compileProc(proc);
        if (isARM64()) {
            std::string regex(".*orr.*,.*,.*,.*lsr #");
            regex += std::to_string(amount) + ".*";
            checkUsesInstruction(*code, regex.c_str(), true);
        }
        return invoke<uint32_t>(*code, n, m);
    };

    for (auto nOperand : int32Operands()) {
        for (auto mOperand : int32Operands()) {
            for (auto amount : amounts) {
                uint32_t n = nOperand.value;
                uint32_t m = mOperand.value;
                CHECK_EQ(test(n, m, amount), n | (m >> amount));
            }
        }
    }
}

void testOrLeftShift64()
{
    if (JSC::Options::defaultB3OptLevel() < 2)
        return;
    Vector<int32_t> amounts = { 1, 34, 63 };

    // Test Pattern: d = n | (m << amount)
    auto test = [&] (int64_t n, int64_t m, int32_t amount) -> int64_t {
        Procedure proc;
        BasicBlock* root = proc.addBlock();

        Value* nValue = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
        Value* mValue = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
        Value* amountValue = root->appendNew<Const32Value>(proc, Origin(), amount);

        Value* shiftValue = root->appendNew<Value>(proc, Shl, Origin(), mValue, amountValue);
        root->appendNewControlValue(
            proc, Return, Origin(), 
            root->appendNew<Value>(proc, BitOr, Origin(), nValue, shiftValue));

        auto code = compileProc(proc);
        if (isARM64()) {
            std::string regex(".*orr.*,.*,.*,.*lsl #");
            regex += std::to_string(amount) + ".*";
            checkUsesInstruction(*code, regex.c_str(), true);
        }
        return invoke<int64_t>(*code, n, m);
    };

    for (auto nOperand : int64Operands()) {
        for (auto mOperand : int64Operands()) {
            for (auto amount : amounts) {
                int64_t n = nOperand.value;
                int64_t m = mOperand.value;
                CHECK_EQ(test(n, m, amount), n | (m << amount));
            }
        }
    }
}

void testOrRightShift64()
{
    if (JSC::Options::defaultB3OptLevel() < 2)
        return;
    Vector<int32_t> amounts = { 1, 34, 63 };

    // Test Pattern: d = n | (m >> amount)
    auto test = [&] (int64_t n, int64_t m, int32_t amount) -> int64_t {
        Procedure proc;
        BasicBlock* root = proc.addBlock();

        Value* nValue = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
        Value* mValue = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
        Value* amountValue = root->appendNew<Const32Value>(proc, Origin(), amount);

        Value* shiftValue = root->appendNew<Value>(proc, SShr, Origin(), mValue, amountValue);
        root->appendNewControlValue(
            proc, Return, Origin(), 
            root->appendNew<Value>(proc, BitOr, Origin(), nValue, shiftValue));

        auto code = compileProc(proc);
        if (isARM64()) {
            std::string regex(".*orr.*,.*,.*,.*asr #");
            regex += std::to_string(amount) + ".*";
            checkUsesInstruction(*code, regex.c_str(), true);
        }
        return invoke<int64_t>(*code, n, m);
    };

    for (auto nOperand : int64Operands()) {
        for (auto mOperand : int64Operands()) {
            for (auto amount : amounts) {
                int64_t n = nOperand.value;
                int64_t m = mOperand.value;
                CHECK_EQ(test(n, m, amount), n | (m >> amount));
            }
        }
    }
}

void testOrUnsignedRightShift64()
{
    if (JSC::Options::defaultB3OptLevel() < 2)
        return;
    Vector<uint32_t> amounts = { 1, 17, 31 };

    // Test Pattern: d = n | (m >> amount)
    auto test = [&] (uint64_t n, uint64_t m, uint32_t amount) -> uint64_t {
        Procedure proc;
        BasicBlock* root = proc.addBlock();

        Value* nValue = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
        Value* mValue = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
        Value* amountValue = root->appendNew<Const32Value>(proc, Origin(), amount);

        Value* shiftValue = root->appendNew<Value>(proc, ZShr, Origin(), mValue, amountValue);
        root->appendNewControlValue(
            proc, Return, Origin(), 
            root->appendNew<Value>(proc, BitOr, Origin(), nValue, shiftValue));

        auto code = compileProc(proc);
        if (isARM64()) {
            std::string regex(".*orr.*,.*,.*,.*lsr #");
            regex += std::to_string(amount) + ".*";
            checkUsesInstruction(*code, regex.c_str(), true);
        }
        return invoke<uint64_t>(*code, n, m);
    };

    for (auto nOperand : int64Operands()) {
        for (auto mOperand : int64Operands()) {
            for (auto amount : amounts) {
                uint64_t n = nOperand.value;
                uint64_t m = mOperand.value;
                CHECK_EQ(test(n, m, amount), n | (m >> amount));
            }
        }
    }
}

void testBitAndZeroShiftRightArgImmMask32()
{
    // Turn this: (tmp >> imm) & mask 
    // Into this: tmp >> imm
    uint32_t tmp = 0xffffffff;
    Vector<uint32_t> imms = { 4, 28, 31 };
    Vector<uint32_t> masks = { 0x0fffffff, 0xf, 0xffff };

    auto test = [&] (uint32_t imm, uint32_t mask) {
        Procedure proc;
        BasicBlock* root = proc.addBlock();

        Value* tmpValue = root->appendNew<Value>(
            proc, Trunc, Origin(), 
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
        Value* immValue = root->appendNew<Const32Value>(proc, Origin(), imm);
        Value* leftValue = root->appendNew<Value>(proc, ZShr, Origin(), tmpValue, immValue);
        Value* rightValue = root->appendNew<Const32Value>(proc, Origin(), mask);
        root->appendNewControlValue(
            proc, Return, Origin(),
            root->appendNew<Value>(proc, BitAnd, Origin(), leftValue, rightValue));

        auto code = compileProc(proc);
        if (isARM64())
            checkUsesInstruction(*code, "lsr");
        uint32_t lhs = invoke<uint32_t>(*code, tmp);
        uint32_t rhs = tmp >> imm;
        CHECK(lhs == rhs);
    };

    for (size_t i = 0; i < imms.size(); ++i)
        test(imms.at(i), masks.at(i));
}

void testBitAndZeroShiftRightArgImmMask64()
{
    // Turn this: (tmp >> imm) & mask 
    // Into this: tmp >> imm
    uint64_t tmp = 0xffffffffffffffff;
    Vector<uint64_t> imms = { 4, 60, 63 };
    Vector<uint64_t> masks = { 0x0fffffffffffffff, 0xf, 0xffff };

    auto test = [&] (uint64_t imm, uint64_t mask) {
        Procedure proc;
        BasicBlock* root = proc.addBlock();

        Value* tmpValue = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
        Value* immValue = root->appendNew<Const32Value>(proc, Origin(), imm);
        Value* leftValue = root->appendNew<Value>(proc, ZShr, Origin(), tmpValue, immValue);
        Value* rightValue = root->appendNew<Const64Value>(proc, Origin(), mask);
        root->appendNewControlValue(
            proc, Return, Origin(),
            root->appendNew<Value>(proc, BitAnd, Origin(), leftValue, rightValue));

        auto code = compileProc(proc);
        if (isARM64())
            checkUsesInstruction(*code, "lsr");
        uint64_t lhs = invoke<uint64_t>(*code, tmp);
        uint64_t rhs = tmp >> imm;
        CHECK(lhs == rhs);
    };

    for (size_t i = 0; i < imms.size(); ++i)
        test(imms.at(i), masks.at(i));
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
    RUN(testUbfx32ShiftAnd());
    RUN(testUbfx32AndShift());
    RUN(testUbfx64ShiftAnd());
    RUN(testUbfx64AndShift());
    RUN(testUbfiz32AndShiftValueMask());
    RUN(testUbfiz32AndShiftMaskValue());
    RUN(testUbfiz32ShiftAnd());
    RUN(testUbfiz32AndShift());
    RUN(testUbfiz64AndShiftValueMask());
    RUN(testUbfiz64AndShiftMaskValue());
    RUN(testUbfiz64ShiftAnd());
    RUN(testUbfiz64AndShift());
    RUN(testInsertBitField32());
    RUN(testInsertBitField64());
    RUN(testExtractInsertBitfieldAtLowEnd32());
    RUN(testExtractInsertBitfieldAtLowEnd64());
    RUN(testBIC32());
    RUN(testBIC64());
    RUN(testOrNot32());
    RUN(testOrNot64());
    RUN(testXorNot32());
    RUN(testXorNot64());
    RUN(testXorNotWithLeftShift32());
    RUN(testXorNotWithRightShift32());
    RUN(testXorNotWithUnsignedRightShift32());
    RUN(testXorNotWithLeftShift64());
    RUN(testXorNotWithRightShift64());
    RUN(testXorNotWithUnsignedRightShift64());
    RUN(testBitfieldZeroExtend32());
    RUN(testBitfieldZeroExtend64());
    RUN(testExtractRegister32());
    RUN(testExtractRegister64());
    RUN(testInsertSignedBitfieldInZero32());
    RUN(testInsertSignedBitfieldInZero64());
    RUN(testExtractSignedBitfield32());
    RUN(testExtractSignedBitfield64());
    RUN(testAddWithLeftShift32());
    RUN(testAddWithRightShift32());
    RUN(testAddWithUnsignedRightShift32());
    RUN(testAddWithLeftShift64());
    RUN(testAddWithRightShift64());
    RUN(testAddWithUnsignedRightShift64());
    RUN(testSubWithLeftShift32());
    RUN(testSubWithRightShift32());
    RUN(testSubWithUnsignedRightShift32());
    RUN(testSubWithLeftShift64());
    RUN(testSubWithRightShift64());
    RUN(testSubWithUnsignedRightShift64());

    RUN(testAndLeftShift32());
    RUN(testAndRightShift32());
    RUN(testAndUnsignedRightShift32());
    RUN(testAndLeftShift64());
    RUN(testAndRightShift64());
    RUN(testAndUnsignedRightShift64());

    RUN(testXorLeftShift32());
    RUN(testXorRightShift32());
    RUN(testXorUnsignedRightShift32());
    RUN(testXorLeftShift64());
    RUN(testXorRightShift64());
    RUN(testXorUnsignedRightShift64());

    RUN(testOrLeftShift32());
    RUN(testOrRightShift32());
    RUN(testOrUnsignedRightShift32());
    RUN(testOrLeftShift64());
    RUN(testOrRightShift64());
    RUN(testOrUnsignedRightShift64());

    RUN(testBitAndZeroShiftRightArgImmMask32());
    RUN(testBitAndZeroShiftRightArgImmMask64());
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

    RUN(testLoadZeroExtendIndexAddress());
    RUN(testLoadSignExtendIndexAddress());
    RUN(testStoreZeroExtendIndexAddress());
    RUN(testStoreSignExtendIndexAddress());
}

#endif // ENABLE(B3_JIT)
