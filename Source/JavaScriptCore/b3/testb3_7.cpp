/*
 * Copyright (C) 2015-2021 Apple Inc. All rights reserved.
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
#include <wtf/WasmSIMD128.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

#if ENABLE(B3_JIT)

void testPinRegisters()
{
    auto go = [&] (bool pin) {
        Procedure proc;
        RegisterSet csrs;
        csrs.merge(RegisterSet::calleeSaveRegisters());
        csrs.exclude(RegisterSet::stackRegisters());
#if CPU(ARM)
        // FIXME We should allow this to be used. See the note
        // in https://commits.webkit.org/257808@main for more
        // info about why masm is using scratch registers on
        // ARM-only.
        csrs.remove(MacroAssembler::addressTempRegister);
#endif // CPU(ARM)
        if (pin) {
            csrs.forEach(
                [&] (Reg reg) {
                    proc.pinRegister(reg);
                });
        }
        BasicBlock* root = proc.addBlock();
        auto arguments = cCallArgumentValues<intptr_t, intptr_t, intptr_t>(proc, root);
        Value* a = arguments[0];
        Value* b = arguments[1];
        Value* c = arguments[2];
        Value* d = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::regCS0);
        root->appendNew<CCallValue>(
            proc, Void, Origin(),
            root->appendNew<ConstPtrValue>(proc, Origin(), static_cast<intptr_t>(0x1234)));
        root->appendNew<CCallValue>(
            proc, Void, Origin(),
            root->appendNew<ConstPtrValue>(proc, Origin(), static_cast<intptr_t>(0x1235)),
            a, b, c);
        PatchpointValue* patchpoint = root->appendNew<PatchpointValue>(proc, Void, Origin());
        patchpoint->appendSomeRegister(d);
        unsigned optLevel = proc.optLevel();
        patchpoint->setGenerator(
            [&] (CCallHelpers&, const StackmapGenerationParams& params) {
                if (optLevel > 1)
                    CHECK_EQ(params[0].gpr(), GPRInfo::regCS0);
            });
        root->appendNew<Value>(proc, Return, Origin());
        auto code = compileProc(proc);
        bool usesCSRs = false;
        for (Air::BasicBlock* block : proc.code()) {
            for (Air::Inst& inst : *block) {
                if (inst.kind.opcode == Air::Patch && inst.origin == patchpoint)
                    continue;
                inst.forEachTmpFast(
                    [&] (Air::Tmp tmp) {
                        if (tmp.isReg())
                            usesCSRs |= csrs.contains(tmp.reg(), IgnoreVectors);
                    });
            }
        }
        if (proc.optLevel() < 2) {
            // Our less good register allocators may use the
            // pinned CSRs in a move.
            usesCSRs = false;
        }
        for (const RegisterAtOffset& regAtOffset : proc.calleeSaveRegisterAtOffsetList())
            usesCSRs |= csrs.contains(regAtOffset.reg(), IgnoreVectors);
        CHECK_EQ(usesCSRs, !pin);
    };

    go(true);
    go(false);
}

void testX86LeaAddAddShlLeft()
{
    // Add(Add(Shl(@x, $c), @y), $d)
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    auto arguments = cCallArgumentValues<intptr_t, intptr_t>(proc, root);
    Value* result = root->appendNew<Value>(
        proc, Add, Origin(),
        root->appendNew<Value>(
            proc, Add, Origin(),
            root->appendNew<Value>(
                proc, Shl, Origin(),
                arguments[1],
                root->appendNew<Const32Value>(proc, Origin(), 2)),
            arguments[0]),
        root->appendNew<ConstPtrValue>(proc, Origin(), 100));
    root->appendNew<Value>(proc, Return, Origin(), result);

    auto code = compileProc(proc);
    if (proc.optLevel() > 1)
        checkUsesInstruction(*code, "lea 0x64(%rdi,%rsi,4), %rax");
    else
        checkUsesInstruction(*code, "lea");
    CHECK_EQ(invoke<intptr_t>(*code, 1, 2), (1 + (2 << 2)) + 100);
}

void testX86LeaAddAddShlRight()
{
    // Add(Add(@x, Shl(@y, $c)), $d)
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    auto arguments = cCallArgumentValues<intptr_t, intptr_t>(proc, root);
    Value* result = root->appendNew<Value>(
        proc, Add, Origin(),
        root->appendNew<Value>(
            proc, Add, Origin(),
            arguments[0],
            root->appendNew<Value>(
                proc, Shl, Origin(),
                arguments[1],
                root->appendNew<Const32Value>(proc, Origin(), 2))),
        root->appendNew<ConstPtrValue>(proc, Origin(), 100));
    root->appendNew<Value>(proc, Return, Origin(), result);

    auto code = compileProc(proc);
    if (proc.optLevel() > 1)
        checkUsesInstruction(*code, "lea 0x64(%rdi,%rsi,4), %rax");
    else
        checkUsesInstruction(*code, "lea");
    CHECK_EQ(invoke<intptr_t>(*code, 1, 2), (1 + (2 << 2)) + 100);
}

void testX86LeaAddAdd()
{
    // Add(Add(@x, @y), $c)
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    auto arguments = cCallArgumentValues<intptr_t, intptr_t>(proc, root);
    Value* result = root->appendNew<Value>(
        proc, Add, Origin(),
        root->appendNew<Value>(
            proc, Add, Origin(),
            arguments[1],
            arguments[0]),
        root->appendNew<ConstPtrValue>(proc, Origin(), 100));
    root->appendNew<Value>(proc, Return, Origin(), result);

    auto code = compileProc(proc);
    CHECK_EQ(invoke<intptr_t>(*code, 1, 2), (1 + 2) + 100);
    if (proc.optLevel() > 1) {
        checkDisassembly(
            *code,
            [&] (const char* disassembly) -> bool {
                return strstr(disassembly, "lea 0x64(%rdi,%rsi,1), %rax")
                    || strstr(disassembly, "lea 0x64(%rsi,%rdi,1), %rax");
            },
            "Expected to find something like lea 0x64(%rdi,%rsi,1), %rax but didn't!"_s);
    }
}

void testX86LeaAddShlRight()
{
    // Add(Shl(@x, $c), @y)
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    auto arguments = cCallArgumentValues<intptr_t, intptr_t>(proc, root);
    Value* result = root->appendNew<Value>(
        proc, Add, Origin(),
        arguments[0],
        root->appendNew<Value>(
            proc, Shl, Origin(),
            arguments[1],
            root->appendNew<Const32Value>(proc, Origin(), 2)));
    root->appendNew<Value>(proc, Return, Origin(), result);

    auto code = compileProc(proc);
    if (proc.optLevel() > 1)
        checkUsesInstruction(*code, "lea (%rdi,%rsi,4), %rax");
    else
        checkUsesInstruction(*code, "lea");
    CHECK_EQ(invoke<intptr_t>(*code, 1, 2), 1 + (2 << 2));
}

void testX86LeaAddShlLeftScale1()
{
    // Add(Shl(@x, $c), @y)
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    auto arguments = cCallArgumentValues<intptr_t, intptr_t>(proc, root);
    Value* result = root->appendNew<Value>(
        proc, Add, Origin(),
        arguments[0],
        root->appendNew<Value>(
            proc, Shl, Origin(),
            arguments[1],
            root->appendNew<Const32Value>(proc, Origin(), 0)));
    root->appendNew<Value>(proc, Return, Origin(), result);

    auto code = compileProc(proc);
    CHECK_EQ(invoke<intptr_t>(*code, 1, 2), 1 + 2);
    if (proc.optLevel() > 1) {
        checkDisassembly(
            *code,
            [&] (const char* disassembly) -> bool {
                return strstr(disassembly, "lea (%rdi,%rsi,1), %rax")
                    || strstr(disassembly, "lea (%rsi,%rdi,1), %rax");
            },
            "Expected to find something like lea (%rdi,%rsi,1), %rax but didn't!");
    }
}

void testX86LeaAddShlLeftScale2()
{
    // Add(Shl(@x, $c), @y)
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    auto arguments = cCallArgumentValues<intptr_t, intptr_t>(proc, root);
    Value* result = root->appendNew<Value>(
        proc, Add, Origin(),
        arguments[0],
        root->appendNew<Value>(
            proc, Shl, Origin(),
            arguments[1],
            root->appendNew<Const32Value>(proc, Origin(), 1)));
    root->appendNew<Value>(proc, Return, Origin(), result);

    auto code = compileProc(proc);
    if (proc.optLevel() > 1)
        checkUsesInstruction(*code, "lea (%rdi,%rsi,2), %rax");
    else
        checkUsesInstruction(*code, "lea");
    CHECK_EQ(invoke<intptr_t>(*code, 1, 2), 1 + (2 << 1));
}

void testX86LeaAddShlLeftScale4()
{
    // Add(Shl(@x, $c), @y)
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    auto arguments = cCallArgumentValues<intptr_t, intptr_t>(proc, root);
    Value* result = root->appendNew<Value>(
        proc, Add, Origin(),
        root->appendNew<Value>(
            proc, Shl, Origin(),
            arguments[1],
            root->appendNew<Const32Value>(proc, Origin(), 2)),
        arguments[0]);
    root->appendNew<Value>(proc, Return, Origin(), result);

    auto code = compileProc(proc);
    if (proc.optLevel() > 1)
        checkUsesInstruction(*code, "lea (%rdi,%rsi,4), %rax");
    else
        checkUsesInstruction(*code, "lea");
    CHECK_EQ(invoke<intptr_t>(*code, 1, 2), 1 + (2 << 2));
}

void testX86LeaAddShlLeftScale8()
{
    // Add(Shl(@x, $c), @y)
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    auto arguments = cCallArgumentValues<intptr_t, intptr_t>(proc, root);
    Value* result = root->appendNew<Value>(
        proc, Add, Origin(),
        arguments[0],
        root->appendNew<Value>(
            proc, Shl, Origin(),
            arguments[1],
            root->appendNew<Const32Value>(proc, Origin(), 3)));
    root->appendNew<Value>(proc, Return, Origin(), result);

    auto code = compileProc(proc);
    if (proc.optLevel() > 1)
        checkUsesInstruction(*code, "lea (%rdi,%rsi,8), %rax");
    else
        checkUsesInstruction(*code, "lea");
    CHECK_EQ(invoke<intptr_t>(*code, 1, 2), 1 + (2 << 3));
}

void testAddShl32()
{
    // Add(Shl(@x, $c), @y)
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    auto arguments = cCallArgumentValues<int64_t, int64_t>(proc, root);
    Value* result = root->appendNew<Value>(
        proc, Add, Origin(),
        arguments[0],
        root->appendNew<Value>(
            proc, Shl, Origin(),
            arguments[1],
            root->appendNew<Const32Value>(proc, Origin(), 32)));
    root->appendNew<Value>(proc, Return, Origin(), result);

    auto code = compileProc(proc);
    CHECK_EQ(invoke<int64_t>(*code, static_cast<int64_t>(1), static_cast<int64_t>(2)), static_cast<int64_t>(1) + (static_cast<int64_t>(2) << static_cast<int32_t>(32)));
}

void testAddShl64()
{
    // Add(Shl(@x, $c), @y)
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    auto arguments = cCallArgumentValues<intptr_t, intptr_t>(proc, root);
    Value* result = root->appendNew<Value>(
        proc, Add, Origin(),
        arguments[0],
        root->appendNew<Value>(
            proc, Shl, Origin(),
            arguments[1],
            root->appendNew<Const32Value>(proc, Origin(), 64)));
    root->appendNew<Value>(proc, Return, Origin(), result);

    auto code = compileProc(proc);
    CHECK_EQ(invoke<intptr_t>(*code, 1, 2), 1 + 2);
}

void testAddShl65()
{
    // Add(Shl(@x, $c), @y)
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    auto arguments = cCallArgumentValues<intptr_t, intptr_t>(proc, root);
    Value* result = root->appendNew<Value>(
        proc, Add, Origin(),
        arguments[0],
        root->appendNew<Value>(
            proc, Shl, Origin(),
            arguments[1],
            root->appendNew<Const32Value>(proc, Origin(), 65)));
    root->appendNew<Value>(proc, Return, Origin(), result);

    auto code = compileProc(proc);
    CHECK_EQ(invoke<intptr_t>(*code, 1, 2), 1 + (2 << 1));
}

void testReduceStrengthReassociation(bool flip)
{
    // Add(Add(@x, $c), @y) -> Add(Add(@x, @y), $c)
    // and
    // Add(@y, Add(@x, $c)) -> Add(Add(@x, @y), $c)
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    auto arguments = cCallArgumentValues<int, int>(proc, root);
    Value* arg1 = arguments[0];
    Value* arg2 = arguments[1];

    Value* innerAdd = root->appendNew<Value>(
        proc, Add, Origin(), arg1,
        root->appendNew<ConstPtrValue>(proc, Origin(), 42));

    Value* outerAdd;
    if (flip)
        outerAdd = root->appendNew<Value>(proc, Add, Origin(), arg2, innerAdd);
    else
        outerAdd = root->appendNew<Value>(proc, Add, Origin(), innerAdd, arg2);

    root->appendNew<Value>(proc, Return, Origin(), outerAdd);

    proc.resetReachability();

    if (shouldBeVerbose(proc)) {
        dataLog("IR before reduceStrength:\n");
        dataLog(proc);
    }

    reduceStrength(proc);

    if (shouldBeVerbose(proc)) {
        dataLog("IR after reduceStrength:\n");
        dataLog(proc);
    }

    CHECK_EQ(root->last()->opcode(), Return);
    CHECK_EQ(root->last()->child(0)->opcode(), Add);
    CHECK(root->last()->child(0)->child(1)->isIntPtr(42));
    CHECK_EQ(root->last()->child(0)->child(0)->opcode(), Add);
    CHECK(
        (root->last()->child(0)->child(0)->child(0) == arg1 && root->last()->child(0)->child(0)->child(1) == arg2) ||
        (root->last()->child(0)->child(0)->child(0) == arg2 && root->last()->child(0)->child(0)->child(1) == arg1));
}

template<typename B3ContType, typename Type64, typename Type32>
void testReduceStrengthTruncConstant(Type64 filler, Type32 value)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();

    int64_t bits = std::bit_cast<int64_t>(filler);
    int32_t loBits = std::bit_cast<int32_t>(value);
    bits = ((bits >> 32) << 32) | loBits;
    Type64 testValue = std::bit_cast<Type64>(bits);

    Value* b2  = root->appendNew<B3ContType>(proc, Origin(), testValue);
    Value* b3  = root->appendNew<Value>(proc, JSC::B3::Trunc, Origin(), b2);
    root->appendNew<Value>(proc, Return, Origin(), b3);

    proc.resetReachability();

    reduceStrength(proc);

    CHECK_EQ(root->last()->opcode(), Return);
    if constexpr (std::same_as<B3ContType, ConstDoubleValue>) {
        CHECK_EQ(root->last()->child(0)->opcode(), ConstFloat);
        CHECK_EQ(std::bit_cast<int32_t>(root->last()->child(0)->asFloat()), std::bit_cast<int32_t>(value));
    } else
        CHECK(root->last()->child(0)->isInt32(value));
}

void testReduceStrengthTruncInt64Constant(int64_t filler, int32_t value)
{
    testReduceStrengthTruncConstant<Const64Value>(filler, value);
}

void testReduceStrengthTruncDoubleConstant(double filler, float value)
{
    testReduceStrengthTruncConstant<ConstDoubleValue>(filler, value);
}

void testReduceStrengthMulDoubleByTwo()
{
    // Mul(arg, 2.0) → Add(arg, arg)
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    auto arguments = cCallArgumentValues<double>(proc, root);
    Value* arg = arguments[0];

    Value* two = root->appendNew<ConstDoubleValue>(proc, Origin(), 2.0);
    Value* mul = root->appendNew<Value>(proc, Mul, Origin(), arg, two);
    root->appendNew<Value>(proc, Return, Origin(), mul);

    proc.resetReachability();
    reduceStrength(proc);

    CHECK_EQ(root->last()->opcode(), Return);
    CHECK_EQ(root->last()->child(0)->opcode(), Add);
    CHECK(root->last()->child(0)->child(0) == arg);
    CHECK(root->last()->child(0)->child(1) == arg);
}

void testReduceStrengthMulFloatByTwo()
{
    // Mul(arg, 2.0f) → Add(arg, arg)
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    auto arguments = cCallArgumentValues<int32_t>(proc, root);
    Value* argInt = arguments[0];
    Value* arg = root->appendNew<Value>(proc, BitwiseCast, Origin(), argInt);

    Value* two = root->appendNew<ConstFloatValue>(proc, Origin(), 2.0f);
    Value* mul = root->appendNew<Value>(proc, Mul, Origin(), arg, two);
    root->appendNew<Value>(proc, Return, Origin(), mul);

    proc.resetReachability();
    reduceStrength(proc);

    // Find the Return value - it may be through BitwiseCast or directly
    Value* returnValue = root->last();
    CHECK_EQ(returnValue->opcode(), Return);
    // The result should contain an Add(arg, arg) somewhere
    Value* result = returnValue->child(0);
    CHECK_EQ(result->opcode(), Add);
    CHECK(result->child(0) == arg);
    CHECK(result->child(1) == arg);
}

void testReduceStrengthMulDoubleByNegOne()
{
    // Mul(arg, -1.0) → Sub(ConstDouble(-0.0), arg)
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    auto arguments = cCallArgumentValues<double>(proc, root);
    Value* arg = arguments[0];

    Value* negOne = root->appendNew<ConstDoubleValue>(proc, Origin(), -1.0);
    Value* mul = root->appendNew<Value>(proc, Mul, Origin(), arg, negOne);
    root->appendNew<Value>(proc, Return, Origin(), mul);

    proc.resetReachability();
    reduceStrength(proc);

    CHECK_EQ(root->last()->opcode(), Return);
    CHECK_EQ(root->last()->child(0)->opcode(), Sub);
    CHECK(root->last()->child(0)->child(0)->hasDouble());
    CHECK(isIdentical(root->last()->child(0)->child(0)->asDouble(), -0.0));
    CHECK(root->last()->child(0)->child(1) == arg);
}

void testReduceStrengthMulFloatByNegOne()
{
    // Mul(arg, -1.0f) → Sub(ConstFloat(-0.0f), arg)
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    auto arguments = cCallArgumentValues<int32_t>(proc, root);
    Value* argInt = arguments[0];
    Value* arg = root->appendNew<Value>(proc, BitwiseCast, Origin(), argInt);

    Value* negOne = root->appendNew<ConstFloatValue>(proc, Origin(), -1.0f);
    Value* mul = root->appendNew<Value>(proc, Mul, Origin(), arg, negOne);
    root->appendNew<Value>(proc, Return, Origin(), mul);

    proc.resetReachability();
    reduceStrength(proc);

    CHECK_EQ(root->last()->opcode(), Return);
    Value* result = root->last()->child(0);
    CHECK_EQ(result->opcode(), Sub);
    CHECK(result->child(0)->hasFloat());
    CHECK(isIdentical(result->child(0)->asFloat(), -0.0f));
    CHECK(result->child(1) == arg);
}

void testReduceStrengthMulDoubleByNegTwo()
{
    // Mul(arg, -2.0) → Sub(ConstDouble(-0.0), Add(arg, arg))
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    auto arguments = cCallArgumentValues<double>(proc, root);
    Value* arg = arguments[0];

    Value* negTwo = root->appendNew<ConstDoubleValue>(proc, Origin(), -2.0);
    Value* mul = root->appendNew<Value>(proc, Mul, Origin(), arg, negTwo);
    root->appendNew<Value>(proc, Return, Origin(), mul);

    proc.resetReachability();
    reduceStrength(proc);

    CHECK_EQ(root->last()->opcode(), Return);
    Value* result = root->last()->child(0);
    CHECK_EQ(result->opcode(), Sub);
    CHECK(result->child(0)->hasDouble());
    CHECK(isIdentical(result->child(0)->asDouble(), -0.0));
    CHECK_EQ(result->child(1)->opcode(), Add);
    CHECK(result->child(1)->child(0) == arg);
    CHECK(result->child(1)->child(1) == arg);
}

void testReduceStrengthMulFloatByNegTwo()
{
    // Mul(arg, -2.0f) → Sub(ConstFloat(-0.0f), Add(arg, arg))
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    auto arguments = cCallArgumentValues<int32_t>(proc, root);
    Value* argInt = arguments[0];
    Value* arg = root->appendNew<Value>(proc, BitwiseCast, Origin(), argInt);

    Value* negTwo = root->appendNew<ConstFloatValue>(proc, Origin(), -2.0f);
    Value* mul = root->appendNew<Value>(proc, Mul, Origin(), arg, negTwo);
    root->appendNew<Value>(proc, Return, Origin(), mul);

    proc.resetReachability();
    reduceStrength(proc);

    CHECK_EQ(root->last()->opcode(), Return);
    Value* result = root->last()->child(0);
    CHECK_EQ(result->opcode(), Sub);
    CHECK(result->child(0)->hasFloat());
    CHECK(isIdentical(result->child(0)->asFloat(), -0.0f));
    CHECK_EQ(result->child(1)->opcode(), Add);
    CHECK(result->child(1)->child(0) == arg);
    CHECK(result->child(1)->child(1) == arg);
}

void testReduceStrengthDivDoubleByNegOne()
{
    // Div(arg, -1.0) → Sub(ConstDouble(-0.0), arg)
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    auto arguments = cCallArgumentValues<double>(proc, root);
    Value* arg = arguments[0];

    Value* negOne = root->appendNew<ConstDoubleValue>(proc, Origin(), -1.0);
    Value* div = root->appendNew<Value>(proc, Div, Origin(), arg, negOne);
    root->appendNew<Value>(proc, Return, Origin(), div);

    proc.resetReachability();
    reduceStrength(proc);

    CHECK_EQ(root->last()->opcode(), Return);
    Value* result = root->last()->child(0);
    CHECK_EQ(result->opcode(), Sub);
    CHECK(result->child(0)->hasDouble());
    CHECK(isIdentical(result->child(0)->asDouble(), -0.0));
    CHECK(result->child(1) == arg);
}

void testReduceStrengthDivFloatByNegOne()
{
    // Div(arg, -1.0f) → Sub(ConstFloat(-0.0f), arg)
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    auto arguments = cCallArgumentValues<int32_t>(proc, root);
    Value* argInt = arguments[0];
    Value* arg = root->appendNew<Value>(proc, BitwiseCast, Origin(), argInt);

    Value* negOne = root->appendNew<ConstFloatValue>(proc, Origin(), -1.0f);
    Value* div = root->appendNew<Value>(proc, Div, Origin(), arg, negOne);
    root->appendNew<Value>(proc, Return, Origin(), div);

    proc.resetReachability();
    reduceStrength(proc);

    CHECK_EQ(root->last()->opcode(), Return);
    Value* result = root->last()->child(0);
    CHECK_EQ(result->opcode(), Sub);
    CHECK(result->child(0)->hasFloat());
    CHECK(isIdentical(result->child(0)->asFloat(), -0.0f));
    CHECK(result->child(1) == arg);
}

void testReduceStrengthDivDoubleByTwo()
{
    // Div(arg, 2.0) → Mul(arg, ConstDouble(0.5))
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    auto arguments = cCallArgumentValues<double>(proc, root);
    Value* arg = arguments[0];

    Value* two = root->appendNew<ConstDoubleValue>(proc, Origin(), 2.0);
    Value* div = root->appendNew<Value>(proc, Div, Origin(), arg, two);
    root->appendNew<Value>(proc, Return, Origin(), div);

    proc.resetReachability();
    reduceStrength(proc);

    CHECK_EQ(root->last()->opcode(), Return);
    Value* result = root->last()->child(0);
    CHECK_EQ(result->opcode(), Mul);
    CHECK(result->child(0) == arg);
    CHECK(result->child(1)->hasDouble());
    CHECK(isIdentical(result->child(1)->asDouble(), 0.5));
}

void testReduceStrengthDivFloatByTwo()
{
    // Div(arg, 2.0f) → Mul(arg, ConstFloat(0.5f))
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    auto arguments = cCallArgumentValues<int32_t>(proc, root);
    Value* argInt = arguments[0];
    Value* arg = root->appendNew<Value>(proc, BitwiseCast, Origin(), argInt);

    Value* two = root->appendNew<ConstFloatValue>(proc, Origin(), 2.0f);
    Value* div = root->appendNew<Value>(proc, Div, Origin(), arg, two);
    root->appendNew<Value>(proc, Return, Origin(), div);

    proc.resetReachability();
    reduceStrength(proc);

    CHECK_EQ(root->last()->opcode(), Return);
    Value* result = root->last()->child(0);
    CHECK_EQ(result->opcode(), Mul);
    CHECK(result->child(0) == arg);
    CHECK(result->child(1)->hasFloat());
    CHECK(isIdentical(result->child(1)->asFloat(), 0.5f));
}

void testReduceStrengthDivDoubleByFour()
{
    // Div(arg, 4.0) → Mul(arg, ConstDouble(0.25))
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    auto arguments = cCallArgumentValues<double>(proc, root);
    Value* arg = arguments[0];

    Value* four = root->appendNew<ConstDoubleValue>(proc, Origin(), 4.0);
    Value* div = root->appendNew<Value>(proc, Div, Origin(), arg, four);
    root->appendNew<Value>(proc, Return, Origin(), div);

    proc.resetReachability();
    reduceStrength(proc);

    CHECK_EQ(root->last()->opcode(), Return);
    Value* result = root->last()->child(0);
    CHECK_EQ(result->opcode(), Mul);
    CHECK(result->child(0) == arg);
    CHECK(result->child(1)->hasDouble());
    CHECK(isIdentical(result->child(1)->asDouble(), 0.25));
}

void testReduceStrengthDivDoubleByNegTwo()
{
    // Div(arg, -2.0) → Mul(arg, ConstDouble(-0.5))
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    auto arguments = cCallArgumentValues<double>(proc, root);
    Value* arg = arguments[0];

    Value* negTwo = root->appendNew<ConstDoubleValue>(proc, Origin(), -2.0);
    Value* div = root->appendNew<Value>(proc, Div, Origin(), arg, negTwo);
    root->appendNew<Value>(proc, Return, Origin(), div);

    proc.resetReachability();
    reduceStrength(proc);

    CHECK_EQ(root->last()->opcode(), Return);
    Value* result = root->last()->child(0);
    CHECK_EQ(result->opcode(), Mul);
    CHECK(result->child(0) == arg);
    CHECK(result->child(1)->hasDouble());
    CHECK(isIdentical(result->child(1)->asDouble(), -0.5));
}

void testReduceStrengthDivFloatByFour()
{
    // Div(arg, 4.0f) → Mul(arg, ConstFloat(0.25f))
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    auto arguments = cCallArgumentValues<int32_t>(proc, root);
    Value* argInt = arguments[0];
    Value* arg = root->appendNew<Value>(proc, BitwiseCast, Origin(), argInt);

    Value* four = root->appendNew<ConstFloatValue>(proc, Origin(), 4.0f);
    Value* div = root->appendNew<Value>(proc, Div, Origin(), arg, four);
    root->appendNew<Value>(proc, Return, Origin(), div);

    proc.resetReachability();
    reduceStrength(proc);

    CHECK_EQ(root->last()->opcode(), Return);
    Value* result = root->last()->child(0);
    CHECK_EQ(result->opcode(), Mul);
    CHECK(result->child(0) == arg);
    CHECK(result->child(1)->hasFloat());
    CHECK(isIdentical(result->child(1)->asFloat(), 0.25f));
}

void testReduceStrengthDivFloatByNegTwo()
{
    // Div(arg, -2.0f) → Mul(arg, ConstFloat(-0.5f))
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    auto arguments = cCallArgumentValues<int32_t>(proc, root);
    Value* argInt = arguments[0];
    Value* arg = root->appendNew<Value>(proc, BitwiseCast, Origin(), argInt);

    Value* negTwo = root->appendNew<ConstFloatValue>(proc, Origin(), -2.0f);
    Value* div = root->appendNew<Value>(proc, Div, Origin(), arg, negTwo);
    root->appendNew<Value>(proc, Return, Origin(), div);

    proc.resetReachability();
    reduceStrength(proc);

    CHECK_EQ(root->last()->opcode(), Return);
    Value* result = root->last()->child(0);
    CHECK_EQ(result->opcode(), Mul);
    CHECK(result->child(0) == arg);
    CHECK(result->child(1)->hasFloat());
    CHECK(isIdentical(result->child(1)->asFloat(), -0.5f));
}

void testReduceStrengthBelowEqualZeroInt32()
{
    // BelowEqual(arg, 0) → Equal(arg, 0)
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    auto arguments = cCallArgumentValues<int32_t>(proc, root);
    Value* arg = arguments[0];
    Value* zero = root->appendNew<Const32Value>(proc, Origin(), 0);
    root->appendNew<Value>(proc, BelowEqual, Origin(), arg, zero);
    root->appendNew<Value>(proc, Return, Origin(), root->last());

    proc.resetReachability();
    reduceStrength(proc);

    CHECK_EQ(root->last()->opcode(), Return);
    Value* result = root->last()->child(0);
    CHECK_EQ(result->opcode(), Equal);
    CHECK(result->child(0) == arg);
    CHECK(result->child(1)->isInt(0));
}

void testReduceStrengthBelowEqualZeroInt64()
{
    // BelowEqual(arg, 0) → Equal(arg, 0)
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    auto arguments = cCallArgumentValues<int64_t>(proc, root);
    Value* arg = arguments[0];
    Value* zero = root->appendNew<Const64Value>(proc, Origin(), 0);
    root->appendNew<Value>(proc, BelowEqual, Origin(), arg, zero);
    root->appendNew<Value>(proc, Return, Origin(), root->last());

    proc.resetReachability();
    reduceStrength(proc);

    CHECK_EQ(root->last()->opcode(), Return);
    Value* result = root->last()->child(0);
    CHECK_EQ(result->opcode(), Equal);
    CHECK(result->child(0) == arg);
    CHECK(result->child(1)->isInt(0));
}

void testReduceStrengthBelowOneInt32()
{
    // Below(arg, 1) → Equal(arg, 0)
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    auto arguments = cCallArgumentValues<int32_t>(proc, root);
    Value* arg = arguments[0];
    Value* one = root->appendNew<Const32Value>(proc, Origin(), 1);
    root->appendNew<Value>(proc, Below, Origin(), arg, one);
    root->appendNew<Value>(proc, Return, Origin(), root->last());

    proc.resetReachability();
    reduceStrength(proc);

    CHECK_EQ(root->last()->opcode(), Return);
    Value* result = root->last()->child(0);
    CHECK_EQ(result->opcode(), Equal);
    CHECK(result->child(0) == arg);
    CHECK(result->child(1)->isInt(0));
}

void testReduceStrengthBelowOneInt64()
{
    // Below(arg, 1) → Equal(arg, 0)
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    auto arguments = cCallArgumentValues<int64_t>(proc, root);
    Value* arg = arguments[0];
    Value* one = root->appendNew<Const64Value>(proc, Origin(), 1);
    root->appendNew<Value>(proc, Below, Origin(), arg, one);
    root->appendNew<Value>(proc, Return, Origin(), root->last());

    proc.resetReachability();
    reduceStrength(proc);

    CHECK_EQ(root->last()->opcode(), Return);
    Value* result = root->last()->child(0);
    CHECK_EQ(result->opcode(), Equal);
    CHECK(result->child(0) == arg);
    CHECK(result->child(1)->isInt(0));
}

void testReduceStrengthAboveEqualOneInt32()
{
    // AboveEqual(arg, 1) → NotEqual(arg, 0)
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    auto arguments = cCallArgumentValues<int32_t>(proc, root);
    Value* arg = arguments[0];
    Value* one = root->appendNew<Const32Value>(proc, Origin(), 1);
    root->appendNew<Value>(proc, AboveEqual, Origin(), arg, one);
    root->appendNew<Value>(proc, Return, Origin(), root->last());

    proc.resetReachability();
    reduceStrength(proc);

    CHECK_EQ(root->last()->opcode(), Return);
    Value* result = root->last()->child(0);
    CHECK_EQ(result->opcode(), NotEqual);
    CHECK(result->child(0) == arg);
    CHECK(result->child(1)->isInt(0));
}

void testReduceStrengthAboveEqualOneInt64()
{
    // AboveEqual(arg, 1) → NotEqual(arg, 0)
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    auto arguments = cCallArgumentValues<int64_t>(proc, root);
    Value* arg = arguments[0];
    Value* one = root->appendNew<Const64Value>(proc, Origin(), 1);
    root->appendNew<Value>(proc, AboveEqual, Origin(), arg, one);
    root->appendNew<Value>(proc, Return, Origin(), root->last());

    proc.resetReachability();
    reduceStrength(proc);

    CHECK_EQ(root->last()->opcode(), Return);
    Value* result = root->last()->child(0);
    CHECK_EQ(result->opcode(), NotEqual);
    CHECK(result->child(0) == arg);
    CHECK(result->child(1)->isInt(0));
}

void testReduceStrengthAboveZeroInt32()
{
    // Above(arg, 0) → NotEqual(arg, 0)
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    auto arguments = cCallArgumentValues<int32_t>(proc, root);
    Value* arg = arguments[0];
    Value* zero = root->appendNew<Const32Value>(proc, Origin(), 0);
    root->appendNew<Value>(proc, Above, Origin(), arg, zero);
    root->appendNew<Value>(proc, Return, Origin(), root->last());

    proc.resetReachability();
    reduceStrength(proc);

    CHECK_EQ(root->last()->opcode(), Return);
    Value* result = root->last()->child(0);
    CHECK_EQ(result->opcode(), NotEqual);
    CHECK(result->child(0) == arg);
    CHECK(result->child(1)->isInt(0));
}

void testReduceStrengthAboveZeroInt64()
{
    // Above(arg, 0) → NotEqual(arg, 0)
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    auto arguments = cCallArgumentValues<int64_t>(proc, root);
    Value* arg = arguments[0];
    Value* zero = root->appendNew<Const64Value>(proc, Origin(), 0);
    root->appendNew<Value>(proc, Above, Origin(), arg, zero);
    root->appendNew<Value>(proc, Return, Origin(), root->last());

    proc.resetReachability();
    reduceStrength(proc);

    CHECK_EQ(root->last()->opcode(), Return);
    Value* result = root->last()->child(0);
    CHECK_EQ(result->opcode(), NotEqual);
    CHECK(result->child(0) == arg);
    CHECK(result->child(1)->isInt(0));
}

void testLoadBaseIndexShift2()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    auto arguments = cCallArgumentValues<intptr_t, intptr_t>(proc, root);
    root->appendNew<Value>(
        proc, Return, Origin(),
        root->appendNew<MemoryValue>(
            proc, Load, Int32, Origin(),
            root->appendNew<Value>(
                proc, Add, Origin(),
                arguments[0],
                root->appendNew<Value>(
                    proc, Shl, Origin(),
                    arguments[1],
                    root->appendNew<Const32Value>(proc, Origin(), 2)))));
    auto code = compileProc(proc);
    if (isX86() && proc.optLevel() > 1)
        checkUsesInstruction(*code, "(%rdi,%rsi,4)");
    int32_t value = 12341234;
    char* ptr = std::bit_cast<char*>(&value);
    for (unsigned i = 0; i < 10; ++i)
        CHECK_EQ(invoke<int32_t>(*code, ptr - (static_cast<intptr_t>(1) << static_cast<intptr_t>(2)) * i, i), 12341234);
}

void testLoadBaseIndexShift32()
{
#if CPU(ADDRESS64)
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    auto arguments = cCallArgumentValues<intptr_t, intptr_t>(proc, root);
    root->appendNew<Value>(
        proc, Return, Origin(),
        root->appendNew<MemoryValue>(
            proc, Load, Int32, Origin(),
            root->appendNew<Value>(
                proc, Add, Origin(),
                arguments[0],
                root->appendNew<Value>(
                    proc, Shl, Origin(),
                    arguments[1],
                    root->appendNew<Const32Value>(proc, Origin(), 32)))));
    auto code = compileProc(proc);
    int32_t value = 12341234;
    char* ptr = std::bit_cast<char*>(&value);
    for (unsigned i = 0; i < 10; ++i)
        CHECK_EQ(invoke<int32_t>(*code, ptr - (static_cast<intptr_t>(1) << static_cast<intptr_t>(32)) * i, i), 12341234);
#endif
}

void testOptimizeMaterialization()
{
    Procedure proc;
    if (proc.optLevel() < 2)
        return;
    if constexpr (is32Bit())
        return;

    BasicBlock* root = proc.addBlock();
    root->appendNew<CCallValue>(
        proc, Void, Origin(),
        root->appendNew<ConstPtrValue>(proc, Origin(), 0x123423453456llu),
        root->appendNew<ConstPtrValue>(proc, Origin(), 0x123423453456llu + 35));
    root->appendNew<Value>(proc, Return, Origin());

    auto code = compileProc(proc);
    bool found = false;
    for (Air::BasicBlock* block : proc.code()) {
        for (Air::Inst& inst : *block) {
            if (inst.kind.opcode != Air::Add64)
                continue;
            if (inst.args[0] != Air::Arg::imm(35))
                continue;
            found = true;
        }
    }
    CHECK(found);
}

template<typename Func>
void generateLoop(Procedure& proc, const Func& func)
{
    BasicBlock* root = proc.addBlock();
    BasicBlock* loop = proc.addBlock();
    BasicBlock* end = proc.addBlock();

    UpsilonValue* initialIndex = root->appendNew<UpsilonValue>(
        proc, Origin(), root->appendNew<Const32Value>(proc, Origin(), 0));
    root->appendNew<Value>(proc, Jump, Origin());
    root->setSuccessors(loop);

    Value* index = loop->appendNew<Value>(proc, Phi, Int32, Origin());
    initialIndex->setPhi(index);

    Value* one = func(loop, index);

    Value* nextIndex = loop->appendNew<Value>(proc, Add, Origin(), index, one);
    UpsilonValue* loopIndex = loop->appendNew<UpsilonValue>(proc, Origin(), nextIndex);
    loopIndex->setPhi(index);
    loop->appendNew<Value>(
        proc, Branch, Origin(),
        loop->appendNew<Value>(
            proc, LessThan, Origin(), nextIndex,
            loop->appendNew<Const32Value>(proc, Origin(), 100)));
    loop->setSuccessors(loop, end);

    end->appendNew<Value>(proc, Return, Origin());
}

static std::array<int, 100> NODELETE makeArrayForLoops()
{
    std::array<int, 100> result;
    for (unsigned i = 0; i < result.size(); ++i)
        result[i] = i & 1;
    return result;
}

template<typename Func>
void generateLoopNotBackwardsDominant(Procedure& proc, std::array<int, 100>& array, const Func& func)
{
    BasicBlock* root = proc.addBlock();
    BasicBlock* loopHeader = proc.addBlock();
    BasicBlock* loopCall = proc.addBlock();
    BasicBlock* loopFooter = proc.addBlock();
    BasicBlock* end = proc.addBlock();

    UpsilonValue* initialIndex = root->appendNew<UpsilonValue>(
        proc, Origin(), root->appendNew<Const32Value>(proc, Origin(), 0));
    // If you look carefully, you'll notice that this is an extremely sneaky use of Upsilon that demonstrates
    // the extent to which our SSA is different from normal-person SSA.
    UpsilonValue* defaultOne = root->appendNew<UpsilonValue>(
        proc, Origin(), root->appendNew<Const32Value>(proc, Origin(), 1));
    root->appendNew<Value>(proc, Jump, Origin());
    root->setSuccessors(loopHeader);

    Value* index = loopHeader->appendNew<Value>(proc, Phi, Int32, Origin());
    initialIndex->setPhi(index);

    // if (array[index])
    loopHeader->appendNew<Value>(
        proc, Branch, Origin(),
        loopHeader->appendNew<MemoryValue>(
            proc, Load, Int32, Origin(),
            loopHeader->appendNew<Value>(
                proc, Add, Origin(),
                loopHeader->appendNew<ConstPtrValue>(proc, Origin(), &array),
                loopHeader->appendNew<Value>(
                    proc, Mul, Origin(),
                    is32Bit() ? index : loopHeader->appendNew<Value>(proc, ZExt32, Origin(), index),
                    loopHeader->appendNew<ConstPtrValue>(proc, Origin(), sizeof(int))))));
    loopHeader->setSuccessors(loopCall, loopFooter);

    Value* functionCall = func(loopCall, index);
    UpsilonValue* oneFromFunction = loopCall->appendNew<UpsilonValue>(proc, Origin(), functionCall);
    loopCall->appendNew<Value>(proc, Jump, Origin());
    loopCall->setSuccessors(loopFooter);

    Value* one = loopFooter->appendNew<Value>(proc, Phi, Int32, Origin());
    defaultOne->setPhi(one);
    oneFromFunction->setPhi(one);
    Value* nextIndex = loopFooter->appendNew<Value>(proc, Add, Origin(), index, one);
    UpsilonValue* loopIndex = loopFooter->appendNew<UpsilonValue>(proc, Origin(), nextIndex);
    loopIndex->setPhi(index);
    loopFooter->appendNew<Value>(
        proc, Branch, Origin(),
        loopFooter->appendNew<Value>(
            proc, LessThan, Origin(), nextIndex,
            loopFooter->appendNew<Const32Value>(proc, Origin(), 100)));
    loopFooter->setSuccessors(loopHeader, end);

    end->appendNew<Value>(proc, Return, Origin());
}

extern "C" {
static JSC_DECLARE_NOEXCEPT_JIT_OPERATION_WITHOUT_WTF_INTERNAL(oneFunction, int, (int* callCount));
}
JSC_DEFINE_NOEXCEPT_JIT_OPERATION(oneFunction, int, (int* callCount))
{
    (*callCount)++;
    return 1;
}

extern "C" {
static JSC_DECLARE_NOEXCEPT_JIT_OPERATION_WITHOUT_WTF_INTERNAL(noOpFunction, void, ());
}
JSC_DEFINE_NOEXCEPT_JIT_OPERATION(noOpFunction, void, ())
{
}

void testLICMPure()
{
    Procedure proc;

    if (proc.optLevel() < 2)
        return;

    generateLoop(
        proc,
        [&] (BasicBlock* loop, Value*) -> Value* {
            auto arguments = cCallArgumentValues<intptr_t>(proc, loop);
            return loop->appendNew<CCallValue>(
                proc, Int32, Origin(), Effects::none(),
                loop->appendNew<ConstPtrValue>(proc, Origin(), tagCFunction<OperationPtrTag>(oneFunction)),
                arguments[0]);
        });

    unsigned callCount = 0;
    compileAndRun<void>(proc, &callCount);
    CHECK_EQ(callCount, 1u);
}

void testLICMPureSideExits()
{
    Procedure proc;
    if (proc.optLevel() < 2)
        return;
    generateLoop(
        proc,
        [&] (BasicBlock* loop, Value*) -> Value* {
            auto arguments = cCallArgumentValues<intptr_t>(proc, loop);
            Effects effects = Effects::none();
            effects.exitsSideways = true;
            effects.reads = HeapRange::top();
            loop->appendNew<CCallValue>(
                proc, Void, Origin(), effects,
                loop->appendNew<ConstPtrValue>(proc, Origin(), tagCFunction<OperationPtrTag>(noOpFunction)));

            return loop->appendNew<CCallValue>(
                proc, Int32, Origin(), Effects::none(),
                loop->appendNew<ConstPtrValue>(proc, Origin(), tagCFunction<OperationPtrTag>(oneFunction)),
                arguments[0]);
        });

    unsigned callCount = 0;
    compileAndRun<void>(proc, &callCount);
    CHECK_EQ(callCount, 1u);
}

void testLICMPureWritesPinned()
{
    Procedure proc;
    if (proc.optLevel() < 2)
        return;
    generateLoop(
        proc,
        [&] (BasicBlock* loop, Value*) -> Value* {
            auto arguments = cCallArgumentValues<unsigned*>(proc, loop);
            Effects effects = Effects::none();
            effects.writesPinned = true;
            loop->appendNew<CCallValue>(
                proc, Void, Origin(), effects,
                loop->appendNew<ConstPtrValue>(proc, Origin(), tagCFunction<OperationPtrTag>(noOpFunction)));

            return loop->appendNew<CCallValue>(
                proc, Int32, Origin(), Effects::none(),
                loop->appendNew<ConstPtrValue>(proc, Origin(), tagCFunction<OperationPtrTag>(oneFunction)),
                arguments[0]);
        });

    unsigned callCount = 0;
    compileAndRun<void>(proc, &callCount);
    CHECK_EQ(callCount, 1u);
}

void testLICMPureWrites()
{
    Procedure proc;
    if (proc.optLevel() < 2)
        return;
    generateLoop(
        proc,
        [&] (BasicBlock* loop, Value*) -> Value* {
            auto arguments = cCallArgumentValues<unsigned*>(proc, loop);
            Effects effects = Effects::none();
            effects.writes = HeapRange(63479);
            loop->appendNew<CCallValue>(
                proc, Void, Origin(), effects,
                loop->appendNew<ConstPtrValue>(proc, Origin(), tagCFunction<OperationPtrTag>(noOpFunction)));

            return loop->appendNew<CCallValue>(
                proc, Int32, Origin(), Effects::none(),
                loop->appendNew<ConstPtrValue>(proc, Origin(), tagCFunction<OperationPtrTag>(oneFunction)),
                arguments[0]);
        });

    unsigned callCount = 0;
    compileAndRun<void>(proc, &callCount);
    CHECK_EQ(callCount, 1u);
}

void testLICMReadsLocalState()
{
    Procedure proc;
    generateLoop(
        proc,
        [&] (BasicBlock* loop, Value*) -> Value* {
            auto arguments = cCallArgumentValues<unsigned*>(proc, loop);
            Effects effects = Effects::none();
            effects.readsLocalState = true;
            return loop->appendNew<CCallValue>(
                proc, Int32, Origin(), effects,
                loop->appendNew<ConstPtrValue>(proc, Origin(), tagCFunction<OperationPtrTag>(oneFunction)),
                arguments[0]);
        });

    unsigned callCount = 0;
    compileAndRun<void>(proc, &callCount);
    CHECK_EQ(callCount, 100u); // We'll fail to hoist because the loop has Upsilons.
}

void testLICMReadsPinned()
{
    Procedure proc;
    if (proc.optLevel() < 2)
        return;
    generateLoop(
        proc,
        [&] (BasicBlock* loop, Value*) -> Value* {
            auto arguments = cCallArgumentValues<unsigned*>(proc, loop);
            Effects effects = Effects::none();
            effects.readsPinned = true;
            return loop->appendNew<CCallValue>(
                proc, Int32, Origin(), effects,
                loop->appendNew<ConstPtrValue>(proc, Origin(), tagCFunction<OperationPtrTag>(oneFunction)),
                arguments[0]);
        });

    unsigned callCount = 0;
    compileAndRun<void>(proc, &callCount);
    CHECK_EQ(callCount, 1u);
}

void testLICMReads()
{
    Procedure proc;
    if (proc.optLevel() < 2)
        return;
    generateLoop(
        proc,
        [&] (BasicBlock* loop, Value*) -> Value* {
            auto arguments = cCallArgumentValues<unsigned*>(proc, loop);
            Effects effects = Effects::none();
            effects.reads = HeapRange::top();
            return loop->appendNew<CCallValue>(
                proc, Int32, Origin(), effects,
                loop->appendNew<ConstPtrValue>(proc, Origin(), tagCFunction<OperationPtrTag>(oneFunction)),
                arguments[0]);
        });

    unsigned callCount = 0;
    compileAndRun<void>(proc, &callCount);
    CHECK_EQ(callCount, 1u);
}

void testLICMPureNotBackwardsDominant()
{
    Procedure proc;
    if (proc.optLevel() < 2)
        return;
    auto array = makeArrayForLoops();
    generateLoopNotBackwardsDominant(
        proc, array,
        [&] (BasicBlock* loop, Value*) -> Value* {
            auto arguments = cCallArgumentValues<unsigned*>(proc, loop);
            return loop->appendNew<CCallValue>(
                proc, Int32, Origin(), Effects::none(),
                loop->appendNew<ConstPtrValue>(proc, Origin(), tagCFunction<OperationPtrTag>(oneFunction)),
                arguments[0]);
        });

    unsigned callCount = 0;
    compileAndRun<void>(proc, &callCount);
    CHECK_EQ(callCount, 1u);
}

void testLICMPureFoiledByChild()
{
    Procedure proc;
    generateLoop(
        proc,
        [&] (BasicBlock* loop, Value* index) -> Value* {
            auto arguments = cCallArgumentValues<unsigned*>(proc, loop);
            return loop->appendNew<CCallValue>(
                proc, Int32, Origin(), Effects::none(),
                loop->appendNew<ConstPtrValue>(proc, Origin(), tagCFunction<OperationPtrTag>(oneFunction)),
                arguments[0],
                index);
        });

    unsigned callCount = 0;
    compileAndRun<void>(proc, &callCount);
    CHECK_EQ(callCount, 100u);
}

void testLICMPureNotBackwardsDominantFoiledByChild()
{
    Procedure proc;
    if (proc.optLevel() < 2)
        return;
    auto array = makeArrayForLoops();
    generateLoopNotBackwardsDominant(
        proc, array,
        [&] (BasicBlock* loop, Value* index) -> Value* {
            auto arguments = cCallArgumentValues<unsigned*>(proc, loop);
            return loop->appendNew<CCallValue>(
                proc, Int32, Origin(), Effects::none(),
                loop->appendNew<ConstPtrValue>(proc, Origin(), tagCFunction<OperationPtrTag>(oneFunction)),
                arguments[0],
                index);
        });

    unsigned callCount = 0;
    compileAndRun<void>(proc, &callCount);
    CHECK_EQ(callCount, 50u);
}

void testLICMExitsSideways()
{
    Procedure proc;
    generateLoop(
        proc,
        [&] (BasicBlock* loop, Value*) -> Value* {
            auto arguments = cCallArgumentValues<unsigned*>(proc, loop);
            Effects effects = Effects::none();
            effects.exitsSideways = true;
            effects.reads = HeapRange::top();
            return loop->appendNew<CCallValue>(
                proc, Int32, Origin(), effects,
                loop->appendNew<ConstPtrValue>(proc, Origin(), tagCFunction<OperationPtrTag>(oneFunction)),
                arguments[0]);
        });

    unsigned callCount = 0;
    compileAndRun<void>(proc, &callCount);
    CHECK_EQ(callCount, 100u);
}

void testLICMWritesLocalState()
{
    Procedure proc;
    generateLoop(
        proc,
        [&] (BasicBlock* loop, Value*) -> Value* {
            auto arguments = cCallArgumentValues<unsigned*>(proc, loop);
            Effects effects = Effects::none();
            effects.writesLocalState = true;
            return loop->appendNew<CCallValue>(
                proc, Int32, Origin(), effects,
                loop->appendNew<ConstPtrValue>(proc, Origin(), tagCFunction<OperationPtrTag>(oneFunction)),
                arguments[0]);
        });

    unsigned callCount = 0;
    compileAndRun<void>(proc, &callCount);
    CHECK_EQ(callCount, 100u);
}

void testLICMWrites()
{
    Procedure proc;
    generateLoop(
        proc,
        [&] (BasicBlock* loop, Value*) -> Value* {
            auto arguments = cCallArgumentValues<unsigned*>(proc, loop);
            Effects effects = Effects::none();
            effects.writes = HeapRange(666);
            return loop->appendNew<CCallValue>(
                proc, Int32, Origin(), effects,
                loop->appendNew<ConstPtrValue>(proc, Origin(), tagCFunction<OperationPtrTag>(oneFunction)),
                arguments[0]);
        });

    unsigned callCount = 0;
    compileAndRun<void>(proc, &callCount);
    CHECK_EQ(callCount, 100u);
}

void testLICMFence()
{
    Procedure proc;
    generateLoop(
        proc,
        [&] (BasicBlock* loop, Value*) -> Value* {
            auto arguments = cCallArgumentValues<unsigned*>(proc, loop);
            Effects effects = Effects::none();
            effects.fence = true;
            return loop->appendNew<CCallValue>(
                proc, Int32, Origin(), effects,
                loop->appendNew<ConstPtrValue>(proc, Origin(), tagCFunction<OperationPtrTag>(oneFunction)),
                arguments[0]);
        });

    unsigned callCount = 0;
    compileAndRun<void>(proc, &callCount);
    CHECK_EQ(callCount, 100u);
}

void testLICMWritesPinned()
{
    Procedure proc;
    generateLoop(
        proc,
        [&] (BasicBlock* loop, Value*) -> Value* {
            auto arguments = cCallArgumentValues<unsigned*>(proc, loop);
            Effects effects = Effects::none();
            effects.writesPinned = true;
            return loop->appendNew<CCallValue>(
                proc, Int32, Origin(), effects,
                loop->appendNew<ConstPtrValue>(proc, Origin(), tagCFunction<OperationPtrTag>(oneFunction)),
                arguments[0]);
        });

    unsigned callCount = 0;
    compileAndRun<void>(proc, &callCount);
    CHECK_EQ(callCount, 100u);
}

void testLICMControlDependent()
{
    Procedure proc;
    if (proc.optLevel() < 2)
        return;
    generateLoop(
        proc,
        [&] (BasicBlock* loop, Value*) -> Value* {
            auto arguments = cCallArgumentValues<unsigned*>(proc, loop);
            Effects effects = Effects::none();
            effects.controlDependent = true;
            return loop->appendNew<CCallValue>(
                proc, Int32, Origin(), effects,
                loop->appendNew<ConstPtrValue>(proc, Origin(), tagCFunction<OperationPtrTag>(oneFunction)),
                arguments[0]);
        });

    unsigned callCount = 0;
    compileAndRun<void>(proc, &callCount);
    CHECK_EQ(callCount, 1u);
}

void testLICMControlDependentNotBackwardsDominant()
{
    Procedure proc;
    if (proc.optLevel() < 2)
        return;
    auto array = makeArrayForLoops();
    generateLoopNotBackwardsDominant(
        proc, array,
        [&] (BasicBlock* loop, Value*) -> Value* {
            auto arguments = cCallArgumentValues<unsigned*>(proc, loop);
            Effects effects = Effects::none();
            effects.controlDependent = true;
            return loop->appendNew<CCallValue>(
                proc, Int32, Origin(), effects,
                loop->appendNew<ConstPtrValue>(proc, Origin(), tagCFunction<OperationPtrTag>(oneFunction)),
                arguments[0]);
        });

    unsigned callCount = 0;
    compileAndRun<void>(proc, &callCount);
    CHECK_EQ(callCount, 50u);
}

void testLICMControlDependentSideExits()
{
    Procedure proc;
    generateLoop(
        proc,
        [&] (BasicBlock* loop, Value*) -> Value* {
            auto arguments = cCallArgumentValues<unsigned*>(proc, loop);
            Effects effects = Effects::none();
            effects.exitsSideways = true;
            effects.reads = HeapRange::top();
            loop->appendNew<CCallValue>(
                proc, Void, Origin(), effects,
                loop->appendNew<ConstPtrValue>(proc, Origin(), tagCFunction<OperationPtrTag>(noOpFunction)));
        
            effects = Effects::none();
            effects.controlDependent = true;
            return loop->appendNew<CCallValue>(
                proc, Int32, Origin(), effects,
                loop->appendNew<ConstPtrValue>(proc, Origin(), tagCFunction<OperationPtrTag>(oneFunction)),
                arguments[0]);
        });

    unsigned callCount = 0;
    compileAndRun<void>(proc, &callCount);
    CHECK_EQ(callCount, 100u);
}

void testLICMReadsPinnedWritesPinned()
{
    Procedure proc;
    generateLoop(
        proc,
        [&] (BasicBlock* loop, Value*) -> Value* {
            auto arguments = cCallArgumentValues<unsigned*>(proc, loop);
            Effects effects = Effects::none();
            effects.writesPinned = true;
            loop->appendNew<CCallValue>(
                proc, Void, Origin(), effects,
                loop->appendNew<ConstPtrValue>(proc, Origin(), tagCFunction<OperationPtrTag>(noOpFunction)));
        
            effects = Effects::none();
            effects.readsPinned = true;
            return loop->appendNew<CCallValue>(
                proc, Int32, Origin(), effects,
                loop->appendNew<ConstPtrValue>(proc, Origin(), tagCFunction<OperationPtrTag>(oneFunction)),
                arguments[0]);
        });

    unsigned callCount = 0;
    compileAndRun<void>(proc, &callCount);
    CHECK_EQ(callCount, 100u);
}

void testLICMReadsWritesDifferentHeaps()
{
    Procedure proc;
    if (proc.optLevel() < 2)
        return;
    generateLoop(
        proc,
        [&] (BasicBlock* loop, Value*) -> Value* {
            auto arguments = cCallArgumentValues<unsigned*>(proc, loop);
            Effects effects = Effects::none();
            effects.writes = HeapRange(6436);
            loop->appendNew<CCallValue>(
                proc, Void, Origin(), effects,
                loop->appendNew<ConstPtrValue>(proc, Origin(), tagCFunction<OperationPtrTag>(noOpFunction)));
        
            effects = Effects::none();
            effects.reads = HeapRange(4886);
            return loop->appendNew<CCallValue>(
                proc, Int32, Origin(), effects,
                loop->appendNew<ConstPtrValue>(proc, Origin(), tagCFunction<OperationPtrTag>(oneFunction)),
                arguments[0]);
        });

    unsigned callCount = 0;
    compileAndRun<void>(proc, &callCount);
    CHECK_EQ(callCount, 1u);
}

void testLICMReadsWritesOverlappingHeaps()
{
    Procedure proc;
    generateLoop(
        proc,
        [&] (BasicBlock* loop, Value*) -> Value* {
            auto arguments = cCallArgumentValues<unsigned*>(proc, loop);
            Effects effects = Effects::none();
            effects.writes = HeapRange(6436, 74458);
            loop->appendNew<CCallValue>(
                proc, Void, Origin(), effects,
                loop->appendNew<ConstPtrValue>(proc, Origin(), tagCFunction<OperationPtrTag>(noOpFunction)));
        
            effects = Effects::none();
            effects.reads = HeapRange(48864, 78239);
            return loop->appendNew<CCallValue>(
                proc, Int32, Origin(), effects,
                loop->appendNew<ConstPtrValue>(proc, Origin(), tagCFunction<OperationPtrTag>(oneFunction)),
                arguments[0]);
        });

    unsigned callCount = 0;
    compileAndRun<void>(proc, &callCount);
    CHECK_EQ(callCount, 100u);
}

void testLICMDefaultCall()
{
    Procedure proc;
    generateLoop(
        proc,
        [&] (BasicBlock* loop, Value*) -> Value* {
            auto arguments = cCallArgumentValues<unsigned*>(proc, loop);
            return loop->appendNew<CCallValue>(
                proc, Int32, Origin(),
                loop->appendNew<ConstPtrValue>(proc, Origin(), tagCFunction<OperationPtrTag>(oneFunction)),
                arguments[0]);
        });

    unsigned callCount = 0;
    compileAndRun<void>(proc, &callCount);
    CHECK_EQ(callCount, 100u);
}

void testDepend32()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    auto arguments = cCallArgumentValues<int32_t*>(proc, root);
    Value* ptr = arguments[0];
    Value* first = root->appendNew<MemoryValue>(proc, Load, Int32, Origin(), ptr, 0);
    Value* depend = root->appendNew<Value>(proc, Depend, Origin(), first);
    if constexpr (!is32Bit())
        depend = root->appendNew<Value>(proc, ZExt32, Origin(), depend);
    Value* second = root->appendNew<MemoryValue>(
        proc, Load, Int32, Origin(),
        root->appendNew<Value>(
            proc, Add, Origin(), ptr,
            depend),
        4);
    root->appendNew<Value>(
        proc, Return, Origin(),
        root->appendNew<Value>(proc, Add, Origin(), first, second));

    int32_t values[2];
    values[0] = 42;
    values[1] = 0xbeef;

    auto code = compileProc(proc);
    if (isARM64() || isARM_THUMB2())
        checkUsesInstruction(*code, "eor");
    else if (isX86()) {
        checkDoesNotUseInstruction(*code, "mfence");
        checkDoesNotUseInstruction(*code, "lock");
    }
    CHECK_EQ(invoke<int32_t>(*code, values), 42 + 0xbeef);
}

void testDepend64()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    auto arguments = cCallArgumentValues<int64_t*>(proc, root);
    Value* ptr = arguments[0];
    Value* first = root->appendNew<MemoryValue>(proc, Load, Int64, Origin(), ptr, 0);
    Value* second = root->appendNew<MemoryValue>(
        proc, Load, Int64, Origin(),
        root->appendNew<Value>(
            proc, Add, Origin(), ptr,
            root->appendNew<Value>(proc, Depend, Origin(), first)),
        8);
    root->appendNew<Value>(
        proc, Return, Origin(),
        root->appendNew<Value>(proc, Add, Origin(), first, second));

    int64_t values[2];
    values[0] = 42;
    values[1] = 0xbeef;

    auto code = compileProc(proc);
    if (isARM64())
        checkUsesInstruction(*code, "eor");
    else if (isX86()) {
        checkDoesNotUseInstruction(*code, "mfence");
        checkDoesNotUseInstruction(*code, "lock");
    }
    CHECK_EQ(invoke<int64_t>(*code, values), 42 + 0xbeef);
}

void testWasmBoundsCheck(unsigned offset)
{
    Procedure proc;
    if (proc.optLevel() < 1)
        return;
    GPRReg pinned = GPRInfo::argumentGPR1;
    proc.pinRegister(pinned);

    proc.setWasmBoundsCheckGenerator([=](CCallHelpers& jit, WasmBoundsCheckValue*, GPRReg pinnedGPR) {
        CHECK_EQ(pinnedGPR, pinned);

        // This should always work because a function this simple should never have callee
        // saves.
        jit.move(CCallHelpers::TrustedImm32(42), GPRInfo::returnValueGPR);
        jit.emitFunctionEpilogue();
        jit.ret();
    });

    BasicBlock* root = proc.addBlock();
    auto arguments = cCallArgumentValues<int32_t>(proc, root);
    root->appendNew<WasmBoundsCheckValue>(proc, Origin(), pinned, arguments[0], offset);
    Value* result = root->appendNew<Const32Value>(proc, Origin(), 0x42);
    root->appendNewControlValue(proc, Return, Origin(), result);

    auto code = compileProc(proc);
    uint32_t bound = 2 + offset;
    auto computeResult = [&] (uint32_t input) {
        return input + offset < bound ? 0x42 : 42;
    };

    CHECK_EQ(invoke<int32_t>(*code, 1, bound), computeResult(1));
    CHECK_EQ(invoke<int32_t>(*code, 3, bound), computeResult(3));
    CHECK_EQ(invoke<int32_t>(*code, 2, bound), computeResult(2));
}

void testWasmAddress()
{
    Procedure proc;
    GPRReg pinnedGPR = GPRInfo::argumentGPR2;
    proc.pinRegister(pinnedGPR);

    unsigned loopCount = 100;
    Vector<unsigned> values(loopCount);
    unsigned numToStore = 42;

    BasicBlock* root = proc.addBlock();
    BasicBlock* header = proc.addBlock();
    BasicBlock* body = proc.addBlock();
    BasicBlock* continuation = proc.addBlock();
    auto arguments = cCallArgumentValues<unsigned, unsigned, unsigned*>(proc, root);

    // Root
    Value* loopCountValue = arguments[0];
    Value* valueToStore = arguments[1];
    UpsilonValue* beginUpsilon = root->appendNew<UpsilonValue>(proc, Origin(), root->appendNew<Const32Value>(proc, Origin(), 0));
    root->appendNewControlValue(proc, Jump, Origin(), header);

    // Header
    Value* indexPhi = header->appendNew<Value>(proc, Phi, Int32, Origin());
    header->appendNewControlValue(proc, Branch, Origin(),
        header->appendNew<Value>(proc, Below, Origin(), indexPhi, loopCountValue),
        body, continuation);

    // Body
    Value* pointer = body->appendNew<Value>(proc, Mul, Origin(), indexPhi,
        body->appendNew<Const32Value>(proc, Origin(), sizeof(unsigned)));
    if (!is32Bit())
        pointer = body->appendNew<Value>(proc, ZExt32, Origin(), pointer);
    body->appendNew<MemoryValue>(proc, Store, Origin(), valueToStore,
        body->appendNew<WasmAddressValue>(proc, Origin(), pointer, pinnedGPR), 0);
    UpsilonValue* incUpsilon = body->appendNew<UpsilonValue>(proc, Origin(),
        body->appendNew<Value>(proc, Add, Origin(), indexPhi,
            body->appendNew<Const32Value>(proc, Origin(), 1)));
    body->appendNewControlValue(proc, Jump, Origin(), header);

    // Continuation
    continuation->appendNewControlValue(proc, Return, Origin());

    beginUpsilon->setPhi(indexPhi);
    incUpsilon->setPhi(indexPhi);


    auto code = compileProc(proc);
    invoke<void>(*code, loopCount, numToStore, values.span().data());
    for (unsigned value : values)
        CHECK_EQ(numToStore, value);
}

void testWasmAddressWithOffset()
{
    Procedure proc;
    GPRReg pinnedGPR = GPRInfo::argumentGPR2;
    proc.pinRegister(pinnedGPR);

    Vector<uint8_t> values(3);
    values[0] = 20;
    values[1] = 21;
    values[2] = 22;
    uint8_t numToStore = 42;

    BasicBlock* root = proc.addBlock();
    auto arguments = cCallArgumentValues<int32_t, uint32_t, uint8_t*>(proc, root);
    // Root
    Value* offset = arguments[0];
    Value* valueToStore = arguments[1];
    Value* pointer = offset;
    if (!is32Bit())
        pointer = root->appendNew<Value>(proc, ZExt32, Origin(), offset);
    root->appendNew<MemoryValue>(proc, Store8, Origin(), valueToStore, root->appendNew<WasmAddressValue>(proc, Origin(), pointer, pinnedGPR), 1);
    root->appendNewControlValue(proc, Return, Origin());

    auto code = compileProc(proc);
    invoke<void>(*code, 1, numToStore, values.span().data());
    CHECK_EQ(20U, values[0]);
    CHECK_EQ(21U, values[1]);
    CHECK_EQ(42U, values[2]);
}

void NODELETE testFastTLSLoad()
{
#if ENABLE(FAST_TLS_JIT)
    _pthread_setspecific_direct(WTF_TESTING_KEY, std::bit_cast<void*>(static_cast<uintptr_t>(0xbeef)));

    Procedure proc;
    BasicBlock* root = proc.addBlock();

    PatchpointValue* patchpoint = root->appendNew<PatchpointValue>(proc, pointerType(), Origin());
    patchpoint->clobber(RegisterSet::macroClobberedGPRs());
    patchpoint->setGenerator(
        [&] (CCallHelpers& jit, const StackmapGenerationParams& params) {
            AllowMacroScratchRegisterUsage allowScratch(jit);
            jit.loadFromTLSPtr(fastTLSOffsetForKey(WTF_TESTING_KEY), params[0].gpr());
        });

    root->appendNew<Value>(proc, Return, Origin(), patchpoint);

    CHECK_EQ(compileAndRun<uintptr_t>(proc), static_cast<uintptr_t>(0xbeef));
#endif
}

void NODELETE testFastTLSStore()
{
#if ENABLE(FAST_TLS_JIT)
    Procedure proc;
    BasicBlock* root = proc.addBlock();

    PatchpointValue* patchpoint = root->appendNew<PatchpointValue>(proc, Void, Origin());
    patchpoint->clobber(RegisterSet::macroClobberedGPRs());
    patchpoint->numGPScratchRegisters = 1;
    patchpoint->setGenerator(
        [&] (CCallHelpers& jit, const StackmapGenerationParams& params) {
            AllowMacroScratchRegisterUsage allowScratch(jit);
            GPRReg scratch = params.gpScratch(0);
            jit.move(CCallHelpers::TrustedImm32(0xdead), scratch);
            jit.storeToTLSPtr(scratch, fastTLSOffsetForKey(WTF_TESTING_KEY));
        });

    root->appendNewControlValue(proc, Return, Origin());

    compileAndRun<void>(proc);
    CHECK_EQ(std::bit_cast<uintptr_t>(_pthread_getspecific_direct(WTF_TESTING_KEY)), static_cast<uintptr_t>(0xdead));
#endif
}

static NEVER_INLINE bool NODELETE doubleEq(double a, double b) { return a == b; }
static NEVER_INLINE bool NODELETE doubleNeq(double a, double b) { return a != b; }
static NEVER_INLINE bool NODELETE doubleGt(double a, double b) { return a > b; }
static NEVER_INLINE bool NODELETE doubleGte(double a, double b) { return a >= b; }
static NEVER_INLINE bool NODELETE doubleLt(double a, double b) { return a < b; }
static NEVER_INLINE bool NODELETE doubleLte(double a, double b) { return a <= b; }

void testDoubleLiteralComparison(double a, double b)
{
    using Test = std::tuple<B3::Opcode, bool (*)(double, double)>;
    StdList<Test> tests = {
        Test { NotEqual, doubleNeq },
        Test { Equal, doubleEq },
        Test { EqualOrUnordered, doubleEq },
        Test { GreaterThan, doubleGt },
        Test { GreaterEqual, doubleGte },
        Test { LessThan, doubleLt },
        Test { LessEqual, doubleLte },
    };

    for (const Test& test : tests) {
        Procedure proc;
        BasicBlock* root = proc.addBlock();
        Value* valueA = root->appendNew<ConstDoubleValue>(proc, Origin(), a);
        Value* valueB = root->appendNew<ConstDoubleValue>(proc, Origin(), b);

        // This is here just to make reduceDoubleToFloat do things.
        Value* valueC = root->appendNew<ConstDoubleValue>(proc, Origin(), 0.0);
        Value* valueAsFloat = root->appendNew<Value>(proc, DoubleToFloat, Origin(), valueC);

        root->appendNewControlValue(
            proc, Return, Origin(),
                root->appendNew<Value>(proc, BitAnd, Origin(),
                    root->appendNew<Value>(proc, std::get<0>(test), Origin(), valueA, valueB),
                    root->appendNew<Value>(proc, Equal, Origin(), valueAsFloat, valueAsFloat)));

        CHECK_EQ(!!compileAndRun<int32_t>(proc), std::get<1>(test)(a, b));
    }
}

void testFloatEqualOrUnorderedFolding()
{
    for (auto& first : floatingPointOperands<float>()) {
        for (auto& second : floatingPointOperands<float>()) {
            float a = first.value;
            float b = second.value;
            bool expectedResult = (a == b) || std::isunordered(a, b);
            Procedure proc;
            BasicBlock* root = proc.addBlock();
            Value* constA = root->appendNew<ConstFloatValue>(proc, Origin(), a);
            Value* constB = root->appendNew<ConstFloatValue>(proc, Origin(), b);

            root->appendNewControlValue(proc, Return, Origin(),
                root->appendNew<Value>(
                    proc, EqualOrUnordered, Origin(),
                    constA,
                    constB));
            CHECK_EQ(!!compileAndRun<int32_t>(proc), expectedResult);
        }
    }
}

void testFloatEqualOrUnorderedFoldingNaN()
{
    StdList<float> nans = {
        std::bit_cast<float>(0xfffffffd),
        std::bit_cast<float>(0xfffffffe),
        std::bit_cast<float>(0xfffffff0),
        static_cast<float>(PNaN),
    };

    unsigned i = 0;
    for (float nan : nans) {
        RELEASE_ASSERT(std::isnan(nan));
        Procedure proc;
        BasicBlock* root = proc.addBlock();
        auto arguments = cCallArgumentValues<double>(proc, root);
        Value* a = root->appendNew<ConstFloatValue>(proc, Origin(), nan);
        Value* b = root->appendNew<Value>(proc, DoubleToFloat, Origin(), arguments[0]);

        if (i % 2)
            std::swap(a, b);
        ++i;
        root->appendNewControlValue(proc, Return, Origin(),
            root->appendNew<Value>(proc, EqualOrUnordered, Origin(), a, b));
        CHECK(!!compileAndRun<int32_t>(proc, static_cast<double>(1.0)));
    }
}

void testFloatEqualOrUnorderedDontFold()
{
    for (auto& first : floatingPointOperands<float>()) {
        float a = first.value;
        Procedure proc;
        BasicBlock* root = proc.addBlock();
        auto arguments = cCallArgumentValues<double>(proc, root);
        Value* constA = root->appendNew<ConstFloatValue>(proc, Origin(), a);
        Value* b = root->appendNew<Value>(proc, DoubleToFloat, Origin(), arguments[0]);
        root->appendNewControlValue(proc, Return, Origin(),
            root->appendNew<Value>(
                proc, EqualOrUnordered, Origin(), constA, b));

        auto code = compileProc(proc);

        for (auto& second : floatingPointOperands<float>()) {
            float b = second.value;
            bool expectedResult = (a == b) || std::isunordered(a, b);
            CHECK_EQ(!!invoke<int32_t>(*code, static_cast<double>(b)), expectedResult);
        }
    }
}

extern "C" {
static JSC_DECLARE_NOEXCEPT_JIT_OPERATION_WITHOUT_WTF_INTERNAL(functionNineArgs, void, (int32_t, void*, void*, void*, void*, void*, void*, void*, void*));
}
JSC_DEFINE_NOEXCEPT_JIT_OPERATION(functionNineArgs, void, (int32_t, void*, void*, void*, void*, void*, void*, void*, void*))
{
}

void testShuffleDoesntTrashCalleeSaves()
{
    Procedure proc;

    BasicBlock* root = proc.addBlock();
    BasicBlock* likely = proc.addBlock();
    BasicBlock* unlikely = proc.addBlock();

    auto regs = RegisterSet::registersToSaveForCCall(RegisterSet::allScalarRegisters());

    unsigned i = 0;
    Vector<Value*> patches;
    for (Reg reg : regs) {
        if (RegisterSet::argumentGPRs().contains(reg, IgnoreVectors) || !reg.isGPR())
            continue;
        ++i;
        PatchpointValue* patchpoint = root->appendNew<PatchpointValue>(proc, Int32, Origin());
        patchpoint->clobber(RegisterSet::macroClobberedGPRs());
        patchpoint->resultConstraints = { ValueRep::reg(reg.gpr()) };
        patchpoint->setGenerator(
            [=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
                AllowMacroScratchRegisterUsage allowScratch(jit);
                jit.move(CCallHelpers::TrustedImm32(i), params[0].gpr());
            });
        patches.append(patchpoint);
    }

    Value* arg1 = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::toArgumentRegister(0 % GPRInfo::numberOfArgumentRegisters));
    Value* arg2 = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::toArgumentRegister(1 % GPRInfo::numberOfArgumentRegisters));
    Value* arg3 = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::toArgumentRegister(2 % GPRInfo::numberOfArgumentRegisters));
    Value* arg4 = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::toArgumentRegister(3 % GPRInfo::numberOfArgumentRegisters));
    Value* arg5 = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::toArgumentRegister(4 % GPRInfo::numberOfArgumentRegisters));
    Value* arg6 = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::toArgumentRegister(5 % GPRInfo::numberOfArgumentRegisters));
    Value* arg7 = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::toArgumentRegister(6 % GPRInfo::numberOfArgumentRegisters));
    Value* arg8 = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::toArgumentRegister(7 % GPRInfo::numberOfArgumentRegisters));

    PatchpointValue* ptr = root->appendNew<PatchpointValue>(proc, pointerType(), Origin());
    ptr->clobber(RegisterSet::macroClobberedGPRs());
    ptr->resultConstraints = { ValueRep::reg(GPRInfo::regCS0) };
    ptr->appendSomeRegister(arg1);
    ptr->setGenerator(
        [=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
            AllowMacroScratchRegisterUsage allowScratch(jit);
            jit.move(params[1].gpr(), params[0].gpr());
        });

    Value* condition = root->appendNew<Value>(
        proc, Equal, Origin(), 
        ptr,
        root->appendNew<ConstPtrValue>(proc, Origin(), 0));

    root->appendNewControlValue(
        proc, Branch, Origin(),
        condition,
        FrequentedBlock(likely, FrequencyClass::Normal), FrequentedBlock(unlikely, FrequencyClass::Rare));

    // Never executes.
    Value* const42 = likely->appendNew<Const32Value>(proc, Origin(), 42);
    likely->appendNewControlValue(proc, Return, Origin(), const42);

    // Always executes.
    Value* constNumber = unlikely->appendNew<Const32Value>(proc, Origin(), 0x1);

    unlikely->appendNew<CCallValue>(
        proc, Void, Origin(),
        unlikely->appendNew<ConstPtrValue>(proc, Origin(), tagCFunction<OperationPtrTag>(functionNineArgs)),
        constNumber, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8);

    PatchpointValue* voidPatch = unlikely->appendNew<PatchpointValue>(proc, Void, Origin());
    voidPatch->clobber(RegisterSet::macroClobberedGPRs());
    for (Value* v : patches)
        voidPatch->appendSomeRegister(v);
    voidPatch->appendSomeRegister(arg1);
    voidPatch->appendSomeRegister(arg2);
    voidPatch->appendSomeRegister(arg3);
    voidPatch->appendSomeRegister(arg4);
    voidPatch->appendSomeRegister(arg5);
    voidPatch->appendSomeRegister(arg6);
    voidPatch->setGenerator([=] (CCallHelpers&, const StackmapGenerationParams&) { });

    unlikely->appendNewControlValue(proc, Return, Origin(),
        unlikely->appendNew<MemoryValue>(proc, Load, Int32, Origin(), ptr));

    int32_t* inputPtr = static_cast<int32_t*>(fastMalloc(sizeof(int32_t)));
    *inputPtr = 48;
    CHECK_EQ(compileAndRun<int32_t>(proc, inputPtr), 48);
    fastFree(inputPtr);
}

void testDemotePatchpointTerminal()
{
    Procedure proc;

    BasicBlock* root = proc.addBlock();
    BasicBlock* done = proc.addBlock();

    PatchpointValue* patchpoint = root->appendNew<PatchpointValue>(proc, Int32, Origin());
    patchpoint->effects.terminal = true;
    root->setSuccessors(done);

    done->appendNew<Value>(proc, Return, Origin(), patchpoint);

    proc.resetReachability();
    breakCriticalEdges(proc);
    IndexSet<Value*> valuesToDemote;
    valuesToDemote.add(patchpoint);
    demoteValues(proc, valuesToDemote);
    validate(proc);
}

void testReportUsedRegistersLateUseFollowedByEarlyDefDoesNotMarkUseAsDead()
{
    Procedure proc;
    if (proc.optLevel() < 2)
        return;
    BasicBlock* root = proc.addBlock();

    RegisterSet allRegs = RegisterSet::allGPRs();
    allRegs.exclude(RegisterSet::stackRegisters());
    allRegs.exclude(RegisterSet::reservedHardwareRegisters());

    {
        // Make every reg 42 (just needs to be a value other than 10).
        Value* const42 = root->appendNew<Const32Value>(proc, Origin(), 42);
        PatchpointValue* patchpoint = root->appendNew<PatchpointValue>(proc, Void, Origin());
        for (Reg reg : allRegs)
            patchpoint->append(const42, ValueRep::reg(reg));
        patchpoint->setGenerator([&] (CCallHelpers&, const StackmapGenerationParams&) { });
    }

    {
        Value* const10 = root->appendNew<Const32Value>(proc, Origin(), 10);
        PatchpointValue* patchpoint = root->appendNew<PatchpointValue>(proc, Void, Origin());
        for (Reg reg : allRegs)
            patchpoint->append(const10, ValueRep::lateReg(reg));
        patchpoint->setGenerator([&] (CCallHelpers& jit, const StackmapGenerationParams&) {
            for (Reg reg : allRegs) {
                auto done = jit.branch32(CCallHelpers::Equal, reg.gpr(), CCallHelpers::TrustedImm32(10));
                jit.breakpoint();
                done.link(&jit);
            }
        });
    }

    {
        PatchpointValue* patchpoint = root->appendNew<PatchpointValue>(proc, Int32, Origin());
        patchpoint->resultConstraints = { ValueRep::SomeEarlyRegister };
        patchpoint->setGenerator([&] (CCallHelpers&, const StackmapGenerationParams& params) {
            RELEASE_ASSERT(allRegs.contains(params[0].gpr(), IgnoreVectors));
        });
    }

    root->appendNewControlValue(proc, Return, Origin());

    compileAndRun<void>(proc);
}

void testInfiniteLoopDoesntCauseBadHoisting()
{
    Procedure proc;
    if (proc.optLevel() < 2)
        return;
    BasicBlock* root = proc.addBlock();
    BasicBlock* header = proc.addBlock();
    BasicBlock* loadBlock = proc.addBlock();
    BasicBlock* postLoadBlock = proc.addBlock();
    auto arguments = cCallArgumentValues<intptr_t>(proc, root);

    Value* arg = arguments[0];
    root->appendNewControlValue(proc, Jump, Origin(), header);

    header->appendNewControlValue(
        proc, Branch, Origin(),
        header->appendNew<Value>(proc, Equal, Origin(),
            arg,
            header->appendNew<ConstPtrValue>(proc, Origin(), 10)), header, loadBlock);

    PatchpointValue* patchpoint = loadBlock->appendNew<PatchpointValue>(proc, Void, Origin());
    patchpoint->effects = Effects::none();
    patchpoint->effects.writesLocalState = true; // Don't DCE this.
    patchpoint->setGenerator(
        [&] (CCallHelpers& jit, const StackmapGenerationParams&) {
            // This works because we don't have callee saves.
            jit.emitFunctionEpilogue();
            jit.ret();
        });

    Value* badLoad = loadBlock->appendNew<MemoryValue>(proc, Load, Int64, Origin(), arg, 0);

    loadBlock->appendNewControlValue(
        proc, Branch, Origin(),
        loadBlock->appendNew<Value>(proc, Equal, Origin(),
            badLoad,
            loadBlock->appendNew<Const64Value>(proc, Origin(), 45)), header, postLoadBlock);

    postLoadBlock->appendNewControlValue(proc, Return, Origin(), badLoad);

    // The patchpoint early ret() works because we don't have callee saves.
    auto code = compileProc(proc);
    RELEASE_ASSERT(!proc.calleeSaveRegisterAtOffsetList().registerCount());
    invoke<void>(*code, static_cast<intptr_t>(55)); // Shouldn't crash dereferncing 55.
}

static void testSimpleTuplePair(unsigned first, int64_t second)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();

    PatchpointValue* patchpoint = root->appendNew<PatchpointValue>(proc, proc.addTuple({ Int32, Int64 }), Origin());
    patchpoint->clobber(RegisterSet::macroClobberedGPRs());
    patchpoint->resultConstraints = { ValueRep::SomeRegister, ValueRep::SomeRegister };
    patchpoint->setGenerator([&] (CCallHelpers& jit, const StackmapGenerationParams& params) {
        AllowMacroScratchRegisterUsage allowScratch(jit);
        jit.move(CCallHelpers::TrustedImm32(first), params[0].gpr());
        jit.move(CCallHelpers::TrustedImmPtr(second), params[1].gpr());
    });
    Value* i32 = root->appendNew<Value>(proc, ZExt32, Origin(),
        root->appendNew<ExtractValue>(proc, Origin(), Int32, patchpoint, 0));
    Value* i64 = root->appendNew<ExtractValue>(proc, Origin(), Int64, patchpoint, 1);
    root->appendNew<Value>(proc, Return, Origin(), root->appendNew<Value>(proc, Add, Origin(), i32, i64));

    CHECK_EQ(compileAndRun<int64_t>(proc), first + second);
}

static void testSimpleTuplePairUnused(unsigned first, int64_t second)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();

    PatchpointValue* patchpoint = root->appendNew<PatchpointValue>(proc, proc.addTuple({ Int32, Int64, Double }), Origin());
    patchpoint->clobber(RegisterSet::macroClobberedGPRs());
    patchpoint->resultConstraints = { ValueRep::SomeRegister, ValueRep::SomeRegister, ValueRep::SomeRegister };
    patchpoint->setGenerator([&] (CCallHelpers& jit, const StackmapGenerationParams& params) {
        AllowMacroScratchRegisterUsage allowScratch(jit);
        jit.move(CCallHelpers::TrustedImm32(first), params[0].gpr());
#if !CPU(ARM_THUMB2) // FIXME
        jit.move(CCallHelpers::TrustedImm64(second), params[1].gpr());
        jit.move64ToDouble(CCallHelpers::Imm64(std::bit_cast<uint64_t>(0.0)), params[2].fpr());
#endif
    });
    Value* i32 = root->appendNew<Value>(proc, ZExt32, Origin(),
        root->appendNew<ExtractValue>(proc, Origin(), Int32, patchpoint, 0));
    Value* i64 = root->appendNew<ExtractValue>(proc, Origin(), Int64, patchpoint, 1);
    root->appendNew<Value>(proc, Return, Origin(), root->appendNew<Value>(proc, Add, Origin(), i32, i64));

    CHECK_EQ(compileAndRun<int64_t>(proc), first + second);
}

static void testSimpleTuplePairStack(unsigned first, int64_t second)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();

    PatchpointValue* patchpoint = root->appendNew<PatchpointValue>(proc, proc.addTuple({ Int32, Int64 }), Origin());
    patchpoint->clobber(RegisterSet::macroClobberedGPRs());
    patchpoint->resultConstraints = { ValueRep::SomeRegister, ValueRep::stackArgument(0) };
    patchpoint->setGenerator([&] (CCallHelpers& jit, const StackmapGenerationParams& params) {
        AllowMacroScratchRegisterUsage allowScratch(jit);
        jit.move(CCallHelpers::TrustedImm32(first), params[0].gpr());
#if !CPU(ARM_THUMB2) // FIXME
        jit.store64(CCallHelpers::TrustedImm64(second), CCallHelpers::Address(CCallHelpers::framePointerRegister, params[1].offsetFromFP()));
#endif
    });
    Value* i32 = root->appendNew<Value>(proc, ZExt32, Origin(),
        root->appendNew<ExtractValue>(proc, Origin(), Int32, patchpoint, 0));
    Value* i64 = root->appendNew<ExtractValue>(proc, Origin(), Int64, patchpoint, 1);
    root->appendNew<Value>(proc, Return, Origin(), root->appendNew<Value>(proc, Add, Origin(), i32, i64));

    CHECK_EQ(compileAndRun<int64_t>(proc), first + second);
}

template<B3::TypeKind kind, typename ResultType>
static void testBottomTupleValue()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();

    auto* tuple = root->appendNew<BottomTupleValue>(proc, Origin(), proc.addTuple({
        kind, kind, kind, kind, kind,
        kind, kind, kind, kind, kind,
        kind, kind, kind, kind, kind,
        kind, kind, kind, kind, kind,
        kind, kind, kind, kind, kind,
        kind, kind, kind, kind, kind,
        kind, kind, kind, kind, kind,
        kind, kind, kind, kind, kind,
        kind, kind, kind, kind, kind,
        kind, kind, kind, kind, kind,
    }));
    Value* result = root->appendNew<ExtractValue>(proc, Origin(), kind, tuple, 0);
    root->appendNew<Value>(proc, Return, Origin(), result);
    CHECK_EQ(compileAndRun<ResultType>(proc), 0);
}

template<B3::TypeKind kind, typename ResultType>
static void testTouchAllBottomTupleValue()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();

    Type tupleType = proc.addTuple({
        kind, kind, kind, kind, kind,
        kind, kind, kind, kind, kind,
        kind, kind, kind, kind, kind,
        kind, kind, kind, kind, kind,
        kind, kind, kind, kind, kind,
        kind, kind, kind, kind, kind,
        kind, kind, kind, kind, kind,
        kind, kind, kind, kind, kind,
        kind, kind, kind, kind, kind,
        kind, kind, kind, kind, kind,
    });
    auto* tuple = root->appendNew<BottomTupleValue>(proc, Origin(), tupleType);
    Value* result = root->appendNew<ExtractValue>(proc, Origin(), kind, tuple, 0);
    for (unsigned index = 1; index < proc.resultCount(tupleType); ++index)
        result = root->appendNew<Value>(proc, Add, Origin(), result, root->appendNew<ExtractValue>(proc, Origin(), kind, tuple, index));
    root->appendNew<Value>(proc, Return, Origin(), result);
    CHECK_EQ(compileAndRun<ResultType>(proc), 0);
}

template<bool shouldFixSSA>
static void tailDupedTuplePair(unsigned first, double second)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* truthy = proc.addBlock();
    BasicBlock* falsey = proc.addBlock();
    auto arguments = cCallArgumentValues<intptr_t>(proc, root);

    Type tupleType = proc.addTuple({ Int32, Double });
    Variable* var = proc.addVariable(tupleType);

    Value* test = arguments[0];
    PatchpointValue* patchpoint = root->appendNew<PatchpointValue>(proc, tupleType, Origin());
    patchpoint->clobber(RegisterSet::macroClobberedGPRs());
    patchpoint->resultConstraints = { ValueRep::SomeRegister, ValueRep::stackArgument(0) };
    patchpoint->setGenerator([&] (CCallHelpers& jit, const StackmapGenerationParams& params) {
        AllowMacroScratchRegisterUsage allowScratch(jit);
        jit.move(CCallHelpers::TrustedImm32(first), params[0].gpr());
#if !CPU(ARM_THUMB2) // FIXME
        jit.store64(CCallHelpers::TrustedImm64(std::bit_cast<uint64_t>(second)), CCallHelpers::Address(CCallHelpers::framePointerRegister, params[1].offsetFromFP()));
#endif
    });
    root->appendNew<VariableValue>(proc, Set, Origin(), var, patchpoint);
    root->appendNewControlValue(proc, Branch, Origin(), test, FrequentedBlock(truthy), FrequentedBlock(falsey));

    auto addDup = [&] (BasicBlock* block) {
        Value* tuple = block->appendNew<VariableValue>(proc, B3::Get, Origin(), var);
        Value* i32 = block->appendNew<Value>(proc, ZExt32, Origin(),
            block->appendNew<ExtractValue>(proc, Origin(), Int32, tuple, 0));
        i32 = block->appendNew<Value>(proc, IToD, Origin(), i32);
        Value* f64 = block->appendNew<ExtractValue>(proc, Origin(), Double, tuple, 1);
        block->appendNew<Value>(proc, Return, Origin(), block->appendNew<Value>(proc, Add, Origin(), i32, f64));
    };

    addDup(truthy);
    addDup(falsey);

    proc.resetReachability();
    if (shouldFixSSA)
        fixSSA(proc);
    CHECK_EQ(compileAndRun<double>(proc, first), first + second);
}

template<bool shouldFixSSA>
static void tuplePairVariableLoop(unsigned first, uint64_t second)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* body = proc.addBlock();
    BasicBlock* exit = proc.addBlock();
    auto arguments = cCallArgumentValues<intptr_t, intptr_t>(proc, root);

    Type tupleType = proc.addTuple({ Int32, Int64 });
    Variable* var = proc.addVariable(tupleType);

    {
        Value* first = arguments[0];
        Value* second = arguments[1];
        PatchpointValue* patchpoint = root->appendNew<PatchpointValue>(proc, tupleType, Origin());
        patchpoint->append({ first, ValueRep::SomeRegister });
        patchpoint->append({ second, ValueRep::SomeRegister });
        patchpoint->resultConstraints = { ValueRep::SomeEarlyRegister, ValueRep::SomeEarlyRegister };
        patchpoint->setGenerator([&] (CCallHelpers& jit, const StackmapGenerationParams& params) {
            jit.move(params[2].gpr(), params[0].gpr());
            jit.move(params[3].gpr(), params[1].gpr());
        });
        root->appendNew<VariableValue>(proc, Set, Origin(), var, patchpoint);
        root->appendNewControlValue(proc, Jump, Origin(), body);
    }

    {
        Value* tuple = body->appendNew<VariableValue>(proc, B3::Get, Origin(), var);
        Value* first = body->appendNew<ExtractValue>(proc, Origin(), Int32, tuple, 0);
        Value* second = body->appendNew<ExtractValue>(proc, Origin(), Int64, tuple, 1);
        PatchpointValue* patchpoint = body->appendNew<PatchpointValue>(proc, tupleType, Origin());
        patchpoint->clobber(RegisterSet::macroClobberedGPRs());
        patchpoint->append({ first, ValueRep::SomeRegister });
        patchpoint->append({ second, ValueRep::SomeRegister });
        patchpoint->resultConstraints = { ValueRep::SomeEarlyRegister, ValueRep::stackArgument(0) };
        patchpoint->setGenerator([&] (CCallHelpers& jit, const StackmapGenerationParams& params) {
            AllowMacroScratchRegisterUsage allowScratch(jit);
            CHECK(params[3].gpr() != params[0].gpr());
            CHECK(params[2].gpr() != params[0].gpr());
#if !CPU(ARM_THUMB2) // FIXME
            jit.add64(CCallHelpers::TrustedImm32(1), params[3].gpr(), params[0].gpr());
            jit.store64(params[0].gpr(), CCallHelpers::Address(CCallHelpers::framePointerRegister, params[1].offsetFromFP()));
#endif

            jit.move(params[2].gpr(), params[0].gpr());
            jit.urshift32(CCallHelpers::TrustedImm32(1), params[0].gpr());
        });
        body->appendNew<VariableValue>(proc, Set, Origin(), var, patchpoint);
        Value* condition = body->appendNew<ExtractValue>(proc, Origin(), Int32, patchpoint, 0);
        body->appendNewControlValue(proc, Branch, Origin(), condition, FrequentedBlock(body), FrequentedBlock(exit));
    }

    {
        Value* tuple = exit->appendNew<VariableValue>(proc, B3::Get, Origin(), var);
        Value* second = exit->appendNew<ExtractValue>(proc, Origin(), Int64, tuple, 1);
        exit->appendNew<Value>(proc, Return, Origin(), second);
    }

    proc.resetReachability();
    validate(proc);
    if (shouldFixSSA)
        fixSSA(proc);
    CHECK_EQ(compileAndRun<uint64_t>(proc, first, second), second + (first ? getMSBSet(first) : first) + 1);
}

template<bool shouldFixSSA>
static void tupleNestedLoop(intptr_t first, double second)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* outerLoop = proc.addBlock();
    BasicBlock* innerLoop = proc.addBlock();
    BasicBlock* outerContinuation = proc.addBlock();
    auto arguments = cCallArgumentValues<intptr_t, double>(proc, root);

    Type tupleType = proc.addTuple({ Int32, Double, Int32 });
    Variable* varOuter = proc.addVariable(tupleType);
    Variable* varInner = proc.addVariable(tupleType);
    Variable* tookInner = proc.addVariable(Int32);

    {
        Value* first = arguments[0];
        Value* second = arguments[1];
        PatchpointValue* patchpoint = root->appendNew<PatchpointValue>(proc, tupleType, Origin());
        patchpoint->append({ first, ValueRep::SomeRegisterWithClobber });
        patchpoint->append({ second, ValueRep::SomeRegisterWithClobber });
        patchpoint->resultConstraints = { ValueRep::SomeRegister, ValueRep::SomeRegister, ValueRep::SomeEarlyRegister };
        patchpoint->setGenerator([&] (CCallHelpers& jit, const StackmapGenerationParams& params) {
            jit.move(params[3].gpr(), params[0].gpr());
            jit.move(params[0].gpr(), params[2].gpr());
            jit.moveDouble(params[4].fpr(), params[1].fpr());
        });
        root->appendNew<VariableValue>(proc, Set, Origin(), varOuter, patchpoint);
        root->appendNew<VariableValue>(proc, Set, Origin(), tookInner, root->appendIntConstant(proc, Origin(), Int32, 0));
        root->appendNewControlValue(proc, Jump, Origin(), outerLoop);
    }

    {
        Value* tuple = outerLoop->appendNew<VariableValue>(proc, B3::Get, Origin(), varOuter);
        Value* first = outerLoop->appendNew<ExtractValue>(proc, Origin(), Int32, tuple, 0);
        Value* second = outerLoop->appendNew<ExtractValue>(proc, Origin(), Double, tuple, 1);
        Value* third = outerLoop->appendNew<VariableValue>(proc, B3::Get, Origin(), tookInner);
        PatchpointValue* patchpoint = outerLoop->appendNew<PatchpointValue>(proc, tupleType, Origin());
        patchpoint->clobber(RegisterSet::macroClobberedGPRs());
        patchpoint->append({ first, ValueRep::SomeRegisterWithClobber });
        patchpoint->append({ second, ValueRep::SomeRegisterWithClobber });
        patchpoint->append({ third, ValueRep::SomeRegisterWithClobber });
        patchpoint->resultConstraints = { ValueRep::SomeRegister, ValueRep::SomeRegister, ValueRep::SomeRegister };
        patchpoint->setGenerator([&] (CCallHelpers& jit, const StackmapGenerationParams& params) {
            AllowMacroScratchRegisterUsage allowScratch(jit);
            jit.move(params[3].gpr(), params[0].gpr());
            jit.moveConditionally32(CCallHelpers::Equal, params[5].gpr(), CCallHelpers::TrustedImm32(0), params[0].gpr(), params[5].gpr(), params[2].gpr());
            jit.moveDouble(params[4].fpr(), params[1].fpr());
        });
        outerLoop->appendNew<VariableValue>(proc, Set, Origin(), varOuter, patchpoint);
        outerLoop->appendNew<VariableValue>(proc, Set, Origin(), varInner, patchpoint);
        Value* condition = outerLoop->appendNew<ExtractValue>(proc, Origin(), Int32, patchpoint, 2);
        outerLoop->appendNewControlValue(proc, Branch, Origin(), condition, FrequentedBlock(outerContinuation), FrequentedBlock(innerLoop));
    }

    {
        Value* tuple = innerLoop->appendNew<VariableValue>(proc, B3::Get, Origin(), varInner);
        Value* first = innerLoop->appendNew<ExtractValue>(proc, Origin(), Int32, tuple, 0);
        Value* second = innerLoop->appendNew<ExtractValue>(proc, Origin(), Double, tuple, 1);
        PatchpointValue* patchpoint = innerLoop->appendNew<PatchpointValue>(proc, tupleType, Origin());
        patchpoint->clobber(RegisterSet::macroClobberedGPRs());
        patchpoint->append({ first, ValueRep::SomeRegisterWithClobber });
        patchpoint->append({ second, ValueRep::SomeRegisterWithClobber });
        patchpoint->resultConstraints = { ValueRep::SomeRegister, ValueRep::SomeRegister, ValueRep::SomeEarlyRegister };
        patchpoint->setGenerator([&] (CCallHelpers& jit, const StackmapGenerationParams& params) {
            AllowMacroScratchRegisterUsage allowScratch(jit);
            jit.move(params[3].gpr(), params[0].gpr());
            jit.move(CCallHelpers::TrustedImm32(0), params[2].gpr());
            jit.moveDouble(params[4].fpr(), params[1].fpr());
        });
        innerLoop->appendNew<VariableValue>(proc, Set, Origin(), varOuter, patchpoint);
        innerLoop->appendNew<VariableValue>(proc, Set, Origin(), varInner, patchpoint);
        Value* condition = innerLoop->appendNew<ExtractValue>(proc, Origin(), Int32, patchpoint, 2);
        innerLoop->appendNew<VariableValue>(proc, Set, Origin(), tookInner, innerLoop->appendIntConstant(proc, Origin(), Int32, 1));
        innerLoop->appendNewControlValue(proc, Branch, Origin(), condition, FrequentedBlock(innerLoop), FrequentedBlock(outerLoop));
    }

    {
        Value* tuple = outerContinuation->appendNew<VariableValue>(proc, B3::Get, Origin(), varInner);
        Value* first = outerContinuation->appendNew<ExtractValue>(proc, Origin(), Int32, tuple, 0);
        Value* second = outerContinuation->appendNew<ExtractValue>(proc, Origin(), Double, tuple, 1);
        Value* result = outerContinuation->appendNew<Value>(proc, Add, Origin(), second, outerContinuation->appendNew<Value>(proc, IToD, Origin(), first));
        outerContinuation->appendNewControlValue(proc, Return, Origin(), result);
    }

    proc.resetReachability();
    validate(proc);
    if (shouldFixSSA)
        fixSSA(proc);
    CHECK_EQ(compileAndRun<double>(proc, first, second), first + second);
}

void addTupleTests(const TestConfig* config, Deque<RefPtr<SharedTask<void()>>>& tasks)
{
    RUN_BINARY(testSimpleTuplePair, int32Operands(), int64Operands());
    RUN_BINARY(testSimpleTuplePairUnused, int32Operands(), int64Operands());
    RUN_BINARY(testSimpleTuplePairStack, int32Operands(), int64Operands());
    RUN((testBottomTupleValue<Int32, int32_t>)());
    RUN((testBottomTupleValue<Int64, int64_t>)());
    RUN((testBottomTupleValue<Float, float>)());
    RUN((testBottomTupleValue<Double, double>)());
    RUN((testTouchAllBottomTupleValue<Int32, int32_t>)());
    RUN((testTouchAllBottomTupleValue<Int64, int64_t>)());
    RUN((testTouchAllBottomTupleValue<Float, float>)());
    RUN((testTouchAllBottomTupleValue<Double, double>)());
    // use int64 as second argument because checking for NaN is annoying and doesn't really matter for this test.
    RUN_BINARY(tailDupedTuplePair<true>, int32Operands(), int64Operands());
    RUN_BINARY(tailDupedTuplePair<false>, int32Operands(), int64Operands());
    RUN_BINARY(tuplePairVariableLoop<true>, int32Operands(), int64Operands());
    RUN_BINARY(tuplePairVariableLoop<false>, int32Operands(), int64Operands());
    RUN_BINARY(tupleNestedLoop<true>, int32Operands(), int64Operands());
    RUN_BINARY(tupleNestedLoop<false>, int32Operands(), int64Operands());
}

template <typename FloatType>
static void testFMaxMin()
{
    auto checkResult = [&] (FloatType result, FloatType expected) {
        CHECK_EQ(std::isnan(result), std::isnan(expected));
        if (!std::isnan(expected)) {
            CHECK_EQ(result, expected);
            CHECK_EQ(std::signbit(result), std::signbit(expected));
        }
    };

    auto runArgTest = [&] (bool max, FloatType arg1, FloatType arg2) {
        Procedure proc;
        BasicBlock* root = proc.addBlock();
        auto arguments = cCallArgumentValues<FloatType, FloatType>(proc, root);
        Value* a = arguments[0];
        Value* b = arguments[1];
        Value* result = root->appendNew<Value>(proc, max ? FMax : FMin, Origin(), a, b);
        root->appendNewControlValue(proc, Return, Origin(), result);
        auto code = compileProc(proc);
        return invoke<FloatType>(*code, arg1, arg2);
    };

    auto runConstTest = [&] (bool max, FloatType arg1, FloatType arg2) {
        Procedure proc;
        BasicBlock* root = proc.addBlock();
        Value* a;
        Value* b;
        if constexpr (std::same_as<FloatType, float>) {
            a = root->appendNew<ConstFloatValue>(proc, Origin(), arg1);
            b = root->appendNew<ConstFloatValue>(proc, Origin(), arg2);
        } else {
            a = root->appendNew<ConstDoubleValue>(proc, Origin(), arg1);
            b = root->appendNew<ConstDoubleValue>(proc, Origin(), arg2);
        }
        Value* result = root->appendNew<Value>(proc, max ? FMax : FMin, Origin(), a, b);
        root->appendNewControlValue(proc, Return, Origin(), result);
        auto code = compileProc(proc);
        return invoke<FloatType>(*code, arg1, arg2);
    };

    auto runMinTest = [&] (FloatType a, FloatType b, FloatType expected) {
        checkResult(runArgTest(false, a, b), expected);
        checkResult(runArgTest(false, b, a), expected);
        checkResult(runConstTest(false, a, b), expected);
        checkResult(runConstTest(false, b, a), expected);
    };

    auto runMaxTest = [&] (FloatType a, FloatType b, FloatType expected) {
        checkResult(runArgTest(true, a, b), expected);
        checkResult(runConstTest(true, a, b), expected);
        checkResult(runArgTest(true, b, a), expected);
        checkResult(runConstTest(true, b, a), expected);
    };

    auto inf = std::numeric_limits<FloatType>::infinity();

    runMinTest(10.0, 0.0, 0.0);
    runMinTest(-10.0, 4.0, -10.0);
    runMinTest(4.1, 4.2, 4.1);
    runMinTest(-4.1, -4.2, -4.2);
    runMinTest(0.0, -0.0, -0.0);
    runMinTest(-0.0, -0.0, -0.0);
    runMinTest(0.0, 0.0, 0.0);
    runMinTest(-inf, 0, -inf);
    runMinTest(-inf, inf, -inf);
    runMinTest(inf, 42.0, 42.0);
    if constexpr (std::same_as<FloatType, float>) {
        runMinTest(0.0, std::nanf(""), std::nanf(""));
        runMinTest(std::nanf(""), 42.0, std::nanf(""));
    } else if constexpr (std::same_as<FloatType, double>) {
        runMinTest(0.0, std::nan(""), std::nan(""));
        runMinTest(std::nan(""), 42.0, std::nan(""));
    }


    runMaxTest(0.0, 10.0, 10.0);
    runMaxTest(-10.0, 4.0, 4.0);
    runMaxTest(4.1, 4.2, 4.2);
    runMaxTest(-4.1, -4.2, -4.1);
    runMaxTest(0.0, -0.0, 0.0);
    runMaxTest(-0.0, -0.0, -0.0);
    runMaxTest(0.0, 0.0, 0.0);
    runMaxTest(-inf, 0, 0);
    runMaxTest(-inf, inf, inf);
    runMaxTest(inf, 42.0, inf);
    if constexpr (std::same_as<FloatType, float>) {
        runMaxTest(0.0, std::nanf(""), std::nanf(""));
        runMaxTest(std::nanf(""), 42.0, std::nanf(""));
    } else if constexpr (std::same_as<FloatType, double>) {
        runMaxTest(0.0, std::nan(""), std::nan(""));
        runMaxTest(std::nan(""), 42.0, std::nan(""));
    }
}

void testFloatMaxMin()
{
    testFMaxMin<float>();
}

void testDoubleMaxMin()
{
    testFMaxMin<double>();
}

void testVectorOrConstants(v128_t lhs, v128_t rhs)
{
    alignas(16) v128_t vector;
    {
        Procedure proc;
        BasicBlock* root = proc.addBlock();
        auto arguments = cCallArgumentValues<void*>(proc, root);

        Value* address = arguments[0];
        Value* lhsConstant = root->appendNew<Const128Value>(proc, Origin(), lhs);
        Value* rhsConstant = root->appendNew<Const128Value>(proc, Origin(), rhs);
        Value* result = root->appendNew<SIMDValue>(proc, Origin(), VectorOr, B3::V128, SIMDLane::v128, SIMDSignMode::None, lhsConstant, rhsConstant);
        root->appendNew<MemoryValue>(proc, Store, Origin(), result, address);
        root->appendNewControlValue(proc, Return, Origin());

        compileAndRun<void>(proc, &vector);
        CHECK(bitEquals(vector, vectorOr(lhs, rhs)));
    }
    {
        Procedure proc;
        BasicBlock* root = proc.addBlock();
        auto arguments = cCallArgumentValues<void*>(proc, root);

        Value* address = arguments[0];
        Value* lhsConstant = root->appendNew<Const128Value>(proc, Origin(), lhs);
        Value* rhsConstant = root->appendNew<Const128Value>(proc, Origin(), rhs);
        Value* input = root->appendNew<MemoryValue>(proc, Load, V128, Origin(), address);
        Value* first = root->appendNew<SIMDValue>(proc, Origin(), VectorOr, B3::V128, SIMDLane::v128, SIMDSignMode::None, input, lhsConstant);
        Value* result = root->appendNew<SIMDValue>(proc, Origin(), VectorOr, B3::V128, SIMDLane::v128, SIMDSignMode::None, first, rhsConstant);
        root->appendNew<MemoryValue>(proc, Store, Origin(), result, address);
        root->appendNewControlValue(proc, Return, Origin());
        auto code = compileProc(proc);

        for (auto& operand : v128Operands()) {
            vector = operand.value;
            invoke<void>(*code, &vector);
            CHECK(bitEquals(vector, vectorOr(vectorOr(lhs, operand.value), rhs)));
        }
    }
}

void testVectorOrSelf()
{
    alignas(16) v128_t vector;
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    auto arguments = cCallArgumentValues<void*>(proc, root);

    Value* address = arguments[0];
    Value* input = root->appendNew<MemoryValue>(proc, Load, V128, Origin(), address);
    Value* result = root->appendNew<SIMDValue>(proc, Origin(), VectorOr, B3::V128, SIMDLane::v128, SIMDSignMode::None, input, input);
    root->appendNew<MemoryValue>(proc, Store, Origin(), result, address);
    root->appendNewControlValue(proc, Return, Origin());

    auto code = compileProc(proc);

    for (auto& operand : v128Operands()) {
        vector = operand.value;
        invoke<void>(*code, &vector);
        CHECK(bitEquals(vector, vectorOr(operand.value, operand.value)));
    }
}

void testVectorXorOrAllOnesToVectorAndXor()
{
    alignas(16) v128_t vectors[2];
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    auto arguments = cCallArgumentValues<void*>(proc, root);

    Value* address = arguments[0];
    Value* constant = root->appendNew<Const128Value>(proc, Origin(), vectorAllOnes());
    Value* input0 = root->appendNew<MemoryValue>(proc, Load, V128, Origin(), address);
    Value* input1 = root->appendNew<MemoryValue>(proc, Load, V128, Origin(), address, static_cast<int32_t>(sizeof(v128_t)));
    Value* result0 = root->appendNew<SIMDValue>(proc, Origin(), VectorXor, B3::V128, SIMDLane::v128, SIMDSignMode::None, input0, constant);
    Value* result1 = root->appendNew<SIMDValue>(proc, Origin(), VectorXor, B3::V128, SIMDLane::v128, SIMDSignMode::None, input1, constant);
    Value* result = root->appendNew<SIMDValue>(proc, Origin(), VectorOr, B3::V128, SIMDLane::v128, SIMDSignMode::None, result0, result1);
    root->appendNew<MemoryValue>(proc, Store, Origin(), result, address);
    root->appendNewControlValue(proc, Return, Origin());

    auto code = compileProc(proc);

    for (auto& operand0 : v128Operands()) {
        for (auto& operand1 : v128Operands()) {
            vectors[0] = operand0.value;
            vectors[1] = operand1.value;
            invoke<void>(*code, vectors);
            CHECK(bitEquals(vectors[0], vectorOr(vectorXor(operand0.value, vectorAllOnes()), vectorXor(operand1.value, vectorAllOnes()))));
        }
    }
}

void testVectorXorAndAllOnesToVectorOrXor()
{
    alignas(16) v128_t vectors[2];
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    auto arguments = cCallArgumentValues<void*>(proc, root);

    Value* address = arguments[0];
    Value* constant = root->appendNew<Const128Value>(proc, Origin(), vectorAllOnes());
    Value* input0 = root->appendNew<MemoryValue>(proc, Load, V128, Origin(), address);
    Value* input1 = root->appendNew<MemoryValue>(proc, Load, V128, Origin(), address, static_cast<int32_t>(sizeof(v128_t)));
    Value* result0 = root->appendNew<SIMDValue>(proc, Origin(), VectorXor, B3::V128, SIMDLane::v128, SIMDSignMode::None, input0, constant);
    Value* result1 = root->appendNew<SIMDValue>(proc, Origin(), VectorXor, B3::V128, SIMDLane::v128, SIMDSignMode::None, input1, constant);
    Value* result = root->appendNew<SIMDValue>(proc, Origin(), VectorAnd, B3::V128, SIMDLane::v128, SIMDSignMode::None, result0, result1);
    root->appendNew<MemoryValue>(proc, Store, Origin(), result, address);
    root->appendNewControlValue(proc, Return, Origin());

    auto code = compileProc(proc);

    for (auto& operand0 : v128Operands()) {
        for (auto& operand1 : v128Operands()) {
            vectors[0] = operand0.value;
            vectors[1] = operand1.value;
            invoke<void>(*code, vectors);
            CHECK(bitEquals(vectors[0], vectorAnd(vectorXor(operand0.value, vectorAllOnes()), vectorXor(operand1.value, vectorAllOnes()))));
        }
    }
}

void testVectorXorOrAllOnesConstantToVectorAndXor(v128_t constant)
{
    alignas(16) v128_t vector;
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    auto arguments = cCallArgumentValues<void*>(proc, root);

    Value* address = arguments[0];
    Value* allOnes = root->appendNew<Const128Value>(proc, Origin(), vectorAllOnes());
    Value* constant0 = root->appendNew<Const128Value>(proc, Origin(), constant);
    Value* input = root->appendNew<MemoryValue>(proc, Load, V128, Origin(), address);
    Value* result0 = root->appendNew<SIMDValue>(proc, Origin(), VectorXor, B3::V128, SIMDLane::v128, SIMDSignMode::None, input, allOnes);
    Value* result = root->appendNew<SIMDValue>(proc, Origin(), VectorOr, B3::V128, SIMDLane::v128, SIMDSignMode::None, result0, constant0);
    root->appendNew<MemoryValue>(proc, Store, Origin(), result, address);
    root->appendNewControlValue(proc, Return, Origin());

    auto code = compileProc(proc);

    for (auto& operand : v128Operands()) {
        vector = operand.value;
        invoke<void>(*code, &vector);
        CHECK(bitEquals(vector, vectorOr(vectorXor(operand.value, vectorAllOnes()), constant)));
    }
}

void testVectorXorAndAllOnesConstantToVectorOrXor(v128_t constant)
{
    alignas(16) v128_t vector;
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    auto arguments = cCallArgumentValues<void*>(proc, root);

    Value* address = arguments[0];
    Value* allOnes = root->appendNew<Const128Value>(proc, Origin(), vectorAllOnes());
    Value* constant0 = root->appendNew<Const128Value>(proc, Origin(), constant);
    Value* input = root->appendNew<MemoryValue>(proc, Load, V128, Origin(), address);
    Value* result0 = root->appendNew<SIMDValue>(proc, Origin(), VectorXor, B3::V128, SIMDLane::v128, SIMDSignMode::None, input, allOnes);
    Value* result = root->appendNew<SIMDValue>(proc, Origin(), VectorAnd, B3::V128, SIMDLane::v128, SIMDSignMode::None, result0, constant0);
    root->appendNew<MemoryValue>(proc, Store, Origin(), result, address);
    root->appendNewControlValue(proc, Return, Origin());

    auto code = compileProc(proc);

    for (auto& operand : v128Operands()) {
        vector = operand.value;
        invoke<void>(*code, &vector);
        CHECK(bitEquals(vector, vectorAnd(vectorXor(operand.value, vectorAllOnes()), constant)));
    }
}

void testVectorAndConstants(v128_t lhs, v128_t rhs)
{
    alignas(16) v128_t vector;
    {
        Procedure proc;
        BasicBlock* root = proc.addBlock();
        auto arguments = cCallArgumentValues<void*>(proc, root);

        Value* address = arguments[0];
        Value* lhsConstant = root->appendNew<Const128Value>(proc, Origin(), lhs);
        Value* rhsConstant = root->appendNew<Const128Value>(proc, Origin(), rhs);
        Value* result = root->appendNew<SIMDValue>(proc, Origin(), VectorAnd, B3::V128, SIMDLane::v128, SIMDSignMode::None, lhsConstant, rhsConstant);
        root->appendNew<MemoryValue>(proc, Store, Origin(), result, address);
        root->appendNewControlValue(proc, Return, Origin());

        compileAndRun<void>(proc, &vector);
        CHECK(bitEquals(vector, vectorAnd(lhs, rhs)));
    }
    {
        Procedure proc;
        BasicBlock* root = proc.addBlock();
        auto arguments = cCallArgumentValues<void*>(proc, root);

        Value* address = arguments[0];
        Value* lhsConstant = root->appendNew<Const128Value>(proc, Origin(), lhs);
        Value* rhsConstant = root->appendNew<Const128Value>(proc, Origin(), rhs);
        Value* input = root->appendNew<MemoryValue>(proc, Load, V128, Origin(), address);
        Value* first = root->appendNew<SIMDValue>(proc, Origin(), VectorAnd, B3::V128, SIMDLane::v128, SIMDSignMode::None, input, lhsConstant);
        Value* result = root->appendNew<SIMDValue>(proc, Origin(), VectorAnd, B3::V128, SIMDLane::v128, SIMDSignMode::None, first, rhsConstant);
        root->appendNew<MemoryValue>(proc, Store, Origin(), result, address);
        root->appendNewControlValue(proc, Return, Origin());
        auto code = compileProc(proc);

        for (auto& operand : v128Operands()) {
            vector = operand.value;
            invoke<void>(*code, &vector);
            CHECK(bitEquals(vector, vectorAnd(vectorAnd(lhs, operand.value), rhs)));
        }
    }
}

void testVectorAndSelf()
{
    alignas(16) v128_t vector;
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    auto arguments = cCallArgumentValues<void*>(proc, root);

    Value* address = arguments[0];
    Value* input = root->appendNew<MemoryValue>(proc, Load, V128, Origin(), address);
    Value* result = root->appendNew<SIMDValue>(proc, Origin(), VectorAnd, B3::V128, SIMDLane::v128, SIMDSignMode::None, input, input);
    root->appendNew<MemoryValue>(proc, Store, Origin(), result, address);
    root->appendNewControlValue(proc, Return, Origin());

    auto code = compileProc(proc);

    for (auto& operand : v128Operands()) {
        vector = operand.value;
        invoke<void>(*code, &vector);
        CHECK(bitEquals(vector, vectorAnd(operand.value, operand.value)));
    }
}

void testVectorXorConstants(v128_t lhs, v128_t rhs)
{
    alignas(16) v128_t vector;
    {
        Procedure proc;
        BasicBlock* root = proc.addBlock();
        auto arguments = cCallArgumentValues<void*>(proc, root);

        Value* address = arguments[0];
        Value* lhsConstant = root->appendNew<Const128Value>(proc, Origin(), lhs);
        Value* rhsConstant = root->appendNew<Const128Value>(proc, Origin(), rhs);
        Value* result = root->appendNew<SIMDValue>(proc, Origin(), VectorXor, B3::V128, SIMDLane::v128, SIMDSignMode::None, lhsConstant, rhsConstant);
        root->appendNew<MemoryValue>(proc, Store, Origin(), result, address);
        root->appendNewControlValue(proc, Return, Origin());

        compileAndRun<void>(proc, &vector);
        CHECK(bitEquals(vector, vectorXor(lhs, rhs)));
    }
    {
        Procedure proc;
        BasicBlock* root = proc.addBlock();
        auto arguments = cCallArgumentValues<void*>(proc, root);

        Value* address = arguments[0];
        Value* lhsConstant = root->appendNew<Const128Value>(proc, Origin(), lhs);
        Value* rhsConstant = root->appendNew<Const128Value>(proc, Origin(), rhs);
        Value* input = root->appendNew<MemoryValue>(proc, Load, V128, Origin(), address);
        Value* first = root->appendNew<SIMDValue>(proc, Origin(), VectorXor, B3::V128, SIMDLane::v128, SIMDSignMode::None, input, lhsConstant);
        Value* result = root->appendNew<SIMDValue>(proc, Origin(), VectorXor, B3::V128, SIMDLane::v128, SIMDSignMode::None, first, rhsConstant);
        root->appendNew<MemoryValue>(proc, Store, Origin(), result, address);
        root->appendNewControlValue(proc, Return, Origin());
        auto code = compileProc(proc);

        for (auto& operand : v128Operands()) {
            vector = operand.value;
            invoke<void>(*code, &vector);
            CHECK(bitEquals(vector, vectorXor(vectorXor(lhs, operand.value), rhs)));
        }
    }
}

void testVectorXorSelf()
{
    alignas(16) v128_t vector;
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    auto arguments = cCallArgumentValues<void*>(proc, root);

    Value* address = arguments[0];
    Value* input = root->appendNew<MemoryValue>(proc, Load, V128, Origin(), address);
    Value* result = root->appendNew<SIMDValue>(proc, Origin(), VectorXor, B3::V128, SIMDLane::v128, SIMDSignMode::None, input, input);
    root->appendNew<MemoryValue>(proc, Store, Origin(), result, address);
    root->appendNewControlValue(proc, Return, Origin());

    auto code = compileProc(proc);

    for (auto& operand : v128Operands()) {
        vector = operand.value;
        invoke<void>(*code, &vector);
        CHECK(bitEquals(vector, vectorXor(operand.value, operand.value)));
    }
}

void testVectorAndConstantConstant(v128_t lhs, v128_t rhs)
{
    alignas(16) v128_t vector;
    {
        Procedure proc;
        BasicBlock* root = proc.addBlock();
        auto arguments = cCallArgumentValues<void*>(proc, root);

        Value* address = arguments[0];
        Value* firstConstant = root->appendNew<Const128Value>(proc, Origin(), lhs);
        Value* secondConstant = root->appendNew<Const128Value>(proc, Origin(), rhs);
        Value* input = root->appendNew<MemoryValue>(proc, Load, V128, Origin(), address);
        Value* first = root->appendNew<SIMDValue>(proc, Origin(), VectorXor, B3::V128, SIMDLane::v128, SIMDSignMode::None, input, firstConstant);
        Value* result = root->appendNew<SIMDValue>(proc, Origin(), VectorAnd, B3::V128, SIMDLane::v128, SIMDSignMode::None, first, secondConstant);
        root->appendNew<MemoryValue>(proc, Store, Origin(), result, address);
        root->appendNewControlValue(proc, Return, Origin());
        auto code = compileProc(proc);

        for (auto& operand : v128Operands()) {
            vector = operand.value;
            invoke<void>(*code, &vector);
            CHECK(bitEquals(vector, vectorAnd(vectorXor(operand.value, lhs), rhs)));
        }
    }
    {
        Procedure proc;
        BasicBlock* root = proc.addBlock();
        auto arguments = cCallArgumentValues<void*>(proc, root);

        Value* address = arguments[0];
        Value* firstConstant = root->appendNew<Const128Value>(proc, Origin(), lhs);
        Value* secondConstant = root->appendNew<Const128Value>(proc, Origin(), rhs);
        Value* input = root->appendNew<MemoryValue>(proc, Load, V128, Origin(), address);
        Value* first = root->appendNew<SIMDValue>(proc, Origin(), VectorOr, B3::V128, SIMDLane::v128, SIMDSignMode::None, input, firstConstant);
        Value* result = root->appendNew<SIMDValue>(proc, Origin(), VectorAnd, B3::V128, SIMDLane::v128, SIMDSignMode::None, first, secondConstant);
        root->appendNew<MemoryValue>(proc, Store, Origin(), result, address);
        root->appendNewControlValue(proc, Return, Origin());
        auto code = compileProc(proc);

        for (auto& operand : v128Operands()) {
            vector = operand.value;
            invoke<void>(*code, &vector);
            CHECK(bitEquals(vector, vectorAnd(vectorOr(operand.value, lhs), rhs)));
        }
    }
}

void testVectorFmulByElementFloat()
{
    for (unsigned i = 0; i < 4; ++i) {
        Procedure proc;
        BasicBlock* root = proc.addBlock();
        auto arguments = cCallArgumentValues<void*, void*, void*>(proc, root);

        Value* address0 = arguments[0];
        Value* address1 = arguments[1];
        Value* address2 = arguments[2];

        Value* input0 = root->appendNew<MemoryValue>(proc, Load, V128, Origin(), address0);
        Value* input1 = root->appendNew<MemoryValue>(proc, Load, V128, Origin(), address1);
        Value* dup = root->appendNew<SIMDValue>(proc, Origin(), VectorDupElement, B3::V128, SIMDLane::f32x4, SIMDSignMode::None, static_cast<uint8_t>(i), input0);
        Value* result = root->appendNew<SIMDValue>(proc, Origin(), VectorMul, B3::V128, SIMDLane::f32x4, SIMDSignMode::None, input1, dup);

        root->appendNew<MemoryValue>(proc, Store, Origin(), result, address2);
        root->appendNewControlValue(proc, Return, Origin());

        auto code = compileProc(proc);

        auto checkFloat = [&](float a, float b) {
            if (std::isnan(a))
                CHECK(std::isnan(b));
            else
                CHECK_EQ(a, b);
        };

        for (auto& operand0 : floatingPointOperands<float>()) {
            for (auto& operand1 : floatingPointOperands<float>()) {
                for (auto& operand2 : floatingPointOperands<float>()) {
                    for (auto& operand3 : floatingPointOperands<float>()) {
                        alignas(16) v128_t vector0;
                        alignas(16) v128_t vector1;
                        alignas(16) v128_t vector2;
                        alignas(16) v128_t result;

                        vector0.f32x4[0] = operand0.value;
                        vector0.f32x4[1] = operand1.value;
                        vector0.f32x4[2] = operand2.value;
                        vector0.f32x4[3] = operand3.value;

                        vector1.f32x4[0] = operand3.value;
                        vector1.f32x4[1] = operand2.value;
                        vector1.f32x4[2] = operand1.value;
                        vector1.f32x4[3] = operand0.value;

                        result.f32x4[0] = vector0.f32x4[i] * vector1.f32x4[0];
                        result.f32x4[1] = vector0.f32x4[i] * vector1.f32x4[1];
                        result.f32x4[2] = vector0.f32x4[i] * vector1.f32x4[2];
                        result.f32x4[3] = vector0.f32x4[i] * vector1.f32x4[3];

                        invoke<void>(*code, &vector0, &vector1, &vector2);
                        checkFloat(result.f32x4[0], vector2.f32x4[0]);
                        checkFloat(result.f32x4[1], vector2.f32x4[1]);
                        checkFloat(result.f32x4[2], vector2.f32x4[2]);
                        checkFloat(result.f32x4[3], vector2.f32x4[3]);
                    }
                }
            }
        }
    }
}

void testVectorFmulByElementDouble()
{
    for (unsigned i = 0; i < 2; ++i) {
        Procedure proc;
        BasicBlock* root = proc.addBlock();
        auto arguments = cCallArgumentValues<void*, void*, void*>(proc, root);

        Value* address0 = arguments[0];
        Value* address1 = arguments[1];
        Value* address2 = arguments[2];

        Value* input0 = root->appendNew<MemoryValue>(proc, Load, V128, Origin(), address0);
        Value* input1 = root->appendNew<MemoryValue>(proc, Load, V128, Origin(), address1);
        Value* dup = root->appendNew<SIMDValue>(proc, Origin(), VectorDupElement, B3::V128, SIMDLane::f64x2, SIMDSignMode::None, static_cast<uint8_t>(i), input0);
        Value* result = root->appendNew<SIMDValue>(proc, Origin(), VectorMul, B3::V128, SIMDLane::f64x2, SIMDSignMode::None, input1, dup);

        root->appendNew<MemoryValue>(proc, Store, Origin(), result, address2);
        root->appendNewControlValue(proc, Return, Origin());

        auto code = compileProc(proc);

        auto checkDouble = [&](double a, double b) {
            if (std::isnan(a))
                CHECK(std::isnan(b));
            else
                CHECK_EQ(a, b);
        };

        for (auto& operand0 : floatingPointOperands<double>()) {
            for (auto& operand1 : floatingPointOperands<double>()) {
                for (auto& operand2 : floatingPointOperands<double>()) {
                    for (auto& operand3 : floatingPointOperands<double>()) {
                        alignas(16) v128_t vector0;
                        alignas(16) v128_t vector1;
                        alignas(16) v128_t vector2;
                        alignas(16) v128_t result;

                        vector0.f64x2[0] = operand0.value;
                        vector0.f64x2[1] = operand1.value;

                        vector1.f64x2[0] = operand2.value;
                        vector1.f64x2[1] = operand3.value;

                        result.f64x2[0] = vector0.f64x2[i] * vector1.f64x2[0];
                        result.f64x2[1] = vector0.f64x2[i] * vector1.f64x2[1];

                        invoke<void>(*code, &vector0, &vector1, &vector2);
                        checkDouble(result.f64x2[0], vector2.f64x2[0]);
                        checkDouble(result.f64x2[1], vector2.f64x2[1]);
                    }
                }
            }
        }
    }
}

void testVectorExtractLane0Float()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    auto arguments = cCallArgumentValues<void*>(proc, root);

    Value* address0 = arguments[0];
    Value* input0 = root->appendNew<MemoryValue>(proc, Load, V128, Origin(), address0);
    root->appendNewControlValue(proc, Return, Origin(), root->appendNew<SIMDValue>(proc, Origin(), VectorExtractLane, B3::Float, SIMDLane::f32x4, SIMDSignMode::None, static_cast<uint8_t>(0), input0));

    auto code = compileProc(proc);

    auto checkFloat = [&](float a, float b) {
        if (std::isnan(a))
            CHECK(std::isnan(b));
        else
            CHECK_EQ(a, b);
    };

    for (auto& operand0 : floatingPointOperands<float>()) {
        alignas(16) v128_t vector0;

        vector0.f32x4[0] = operand0.value;
        vector0.f32x4[1] = 1;
        vector0.f32x4[2] = 2;
        vector0.f32x4[3] = 3;

        float result = invoke<float>(*code, &vector0);
        checkFloat(result, operand0.value);
    }
}

void testVectorExtractLane0Double()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    auto arguments = cCallArgumentValues<void*>(proc, root);

    Value* address0 = arguments[0];
    Value* input0 = root->appendNew<MemoryValue>(proc, Load, V128, Origin(), address0);
    root->appendNewControlValue(proc, Return, Origin(), root->appendNew<SIMDValue>(proc, Origin(), VectorExtractLane, B3::Double, SIMDLane::f64x2, SIMDSignMode::None, static_cast<uint8_t>(0), input0));

    auto code = compileProc(proc);

    auto checkDouble = [&](double a, double b) {
        if (std::isnan(a))
            CHECK(std::isnan(b));
        else
            CHECK_EQ(a, b);
    };

    for (auto& operand0 : floatingPointOperands<double>()) {
        alignas(16) v128_t vector0;

        vector0.f64x2[0] = operand0.value;
        vector0.f64x2[1] = 32;

        double result = invoke<double>(*code, &vector0);
        checkDouble(result, operand0.value);
    }
}

void testVectorMulHigh()
{
    auto vectorMulHigh = [&](SIMDLane lane, SIMDSignMode signMode, v128_t lhs, v128_t rhs) {
        auto simdeLHS = std::bit_cast<simde_v128_t>(lhs);
        auto simdeRHS = std::bit_cast<simde_v128_t>(rhs);
        switch (lane) {
        case SIMDLane::i16x8:
            if (signMode == SIMDSignMode::Unsigned)
                return std::bit_cast<v128_t>(simde_wasm_u16x8_extmul_high_u8x16(simdeLHS, simdeRHS));
            return std::bit_cast<v128_t>(simde_wasm_i16x8_extmul_high_i8x16(simdeLHS, simdeRHS));
        case SIMDLane::i32x4:
            if (signMode == SIMDSignMode::Unsigned)
                return std::bit_cast<v128_t>(simde_wasm_u32x4_extmul_high_u16x8(simdeLHS, simdeRHS));
            return std::bit_cast<v128_t>(simde_wasm_i32x4_extmul_high_i16x8(simdeLHS, simdeRHS));
        case SIMDLane::i64x2:
            if (signMode == SIMDSignMode::Unsigned)
                return std::bit_cast<v128_t>(simde_wasm_u64x2_extmul_high_u32x4(simdeLHS, simdeRHS));
            return std::bit_cast<v128_t>(simde_wasm_i64x2_extmul_high_i32x4(simdeLHS, simdeRHS));
        default:
            return v128_t { };
        }
    };

    auto test = [&](SIMDLane lane, SIMDSignMode signMode) {
        alignas(16) v128_t vectors[2];
        Procedure proc;
        BasicBlock* root = proc.addBlock();
        auto arguments = cCallArgumentValues<void*>(proc, root);

        Value* address = arguments[0];
        Value* input0 = root->appendNew<MemoryValue>(proc, Load, V128, Origin(), address);
        Value* input1 = root->appendNew<MemoryValue>(proc, Load, V128, Origin(), address, static_cast<int32_t>(sizeof(v128_t)));
        Value* result = root->appendNew<SIMDValue>(proc, Origin(), VectorMulHigh, B3::V128, lane, signMode, input0, input1);
        root->appendNew<MemoryValue>(proc, Store, Origin(), result, address);
        root->appendNewControlValue(proc, Return, Origin());

        auto code = compileProc(proc);
        for (auto& operand0 : v128Operands()) {
            for (auto& operand1 : v128Operands()) {
                vectors[0] = operand0.value;
                vectors[1] = operand1.value;
                invoke<void>(*code, vectors);
                CHECK(bitEquals(vectors[0], vectorMulHigh(lane, signMode, operand0.value, operand1.value)));
            }
        }
    };

    test(SIMDLane::i16x8, SIMDSignMode::Signed);
    test(SIMDLane::i16x8, SIMDSignMode::Unsigned);
    test(SIMDLane::i32x4, SIMDSignMode::Signed);
    test(SIMDLane::i32x4, SIMDSignMode::Unsigned);
    test(SIMDLane::i64x2, SIMDSignMode::Signed);
    test(SIMDLane::i64x2, SIMDSignMode::Unsigned);
}

void testVectorMulLow()
{
    auto vectorMulLow = [&](SIMDLane lane, SIMDSignMode signMode, v128_t lhs, v128_t rhs) {
        auto simdeLHS = std::bit_cast<simde_v128_t>(lhs);
        auto simdeRHS = std::bit_cast<simde_v128_t>(rhs);
        switch (lane) {
        case SIMDLane::i16x8:
            if (signMode == SIMDSignMode::Unsigned)
                return std::bit_cast<v128_t>(simde_wasm_u16x8_extmul_low_u8x16(simdeLHS, simdeRHS));
            return std::bit_cast<v128_t>(simde_wasm_i16x8_extmul_low_i8x16(simdeLHS, simdeRHS));
        case SIMDLane::i32x4:
            if (signMode == SIMDSignMode::Unsigned)
                return std::bit_cast<v128_t>(simde_wasm_u32x4_extmul_low_u16x8(simdeLHS, simdeRHS));
            return std::bit_cast<v128_t>(simde_wasm_i32x4_extmul_low_i16x8(simdeLHS, simdeRHS));
        case SIMDLane::i64x2:
            if (signMode == SIMDSignMode::Unsigned)
                return std::bit_cast<v128_t>(simde_wasm_u64x2_extmul_low_u32x4(simdeLHS, simdeRHS));
            return std::bit_cast<v128_t>(simde_wasm_i64x2_extmul_low_i32x4(simdeLHS, simdeRHS));
        default:
            return v128_t { };
        }
    };

    auto test = [&](SIMDLane lane, SIMDSignMode signMode) {
        alignas(16) v128_t vectors[2];
        Procedure proc;
        BasicBlock* root = proc.addBlock();
        auto arguments = cCallArgumentValues<void*>(proc, root);

        Value* address = arguments[0];
        Value* input0 = root->appendNew<MemoryValue>(proc, Load, V128, Origin(), address);
        Value* input1 = root->appendNew<MemoryValue>(proc, Load, V128, Origin(), address, static_cast<int32_t>(sizeof(v128_t)));
        Value* result = root->appendNew<SIMDValue>(proc, Origin(), VectorMulLow, B3::V128, lane, signMode, input0, input1);
        root->appendNew<MemoryValue>(proc, Store, Origin(), result, address);
        root->appendNewControlValue(proc, Return, Origin());

        auto code = compileProc(proc);
        for (auto& operand0 : v128Operands()) {
            for (auto& operand1 : v128Operands()) {
                vectors[0] = operand0.value;
                vectors[1] = operand1.value;
                invoke<void>(*code, vectors);
                CHECK(bitEquals(vectors[0], vectorMulLow(lane, signMode, operand0.value, operand1.value)));
            }
        }
    };

    test(SIMDLane::i16x8, SIMDSignMode::Signed);
    test(SIMDLane::i16x8, SIMDSignMode::Unsigned);
    test(SIMDLane::i32x4, SIMDSignMode::Signed);
    test(SIMDLane::i32x4, SIMDSignMode::Unsigned);
    test(SIMDLane::i64x2, SIMDSignMode::Signed);
    test(SIMDLane::i64x2, SIMDSignMode::Unsigned);
}

void testVectorRelaxedMinMax()
{
    auto test = [&](SIMDLane lane, B3::Opcode opcode) {
        alignas(16) v128_t vectors[2];
        Procedure proc;
        BasicBlock* root = proc.addBlock();
        auto arguments = cCallArgumentValues<void*>(proc, root);

        Value* address = arguments[0];
        Value* input0 = root->appendNew<MemoryValue>(proc, Load, V128, Origin(), address);
        Value* input1 = root->appendNew<MemoryValue>(proc, Load, V128, Origin(), address, static_cast<int32_t>(sizeof(v128_t)));
        Value* result = root->appendNew<SIMDValue>(proc, Origin(), opcode, B3::V128, lane, SIMDSignMode::None, input0, input1);
        root->appendNew<MemoryValue>(proc, Store, Origin(), result, address);
        root->appendNewControlValue(proc, Return, Origin());

        auto code = compileProc(proc);
        for (auto& operand0 : v128Operands()) {
            for (auto& operand1 : v128Operands()) {
                vectors[0] = operand0.value;
                vectors[1] = operand1.value;
                invoke<void>(*code, vectors);

                // Relaxed min/max: results are implementation-defined for NaN and ±0 inputs.
                // Only verify lanes where both inputs are non-NaN and not a ±0 pair.
                if (lane == SIMDLane::f32x4) {
                    float a[4], b[4], r[4];
                    memcpy(a, &operand0.value, 16);
                    memcpy(b, &operand1.value, 16);
                    memcpy(r, &vectors[0], 16);
                    for (int i = 0; i < 4; i++) {
                        if (std::isnan(a[i]) || std::isnan(b[i]))
                            continue;
                        if (a[i] == 0.0f && b[i] == 0.0f)
                            continue;
                        float e = (opcode == VectorRelaxedMin) ? std::min(a[i], b[i]) : std::max(a[i], b[i]);
                        CHECK(r[i] == e);
                    }
                } else {
                    double a[2], b[2], r[2];
                    memcpy(a, &operand0.value, 16);
                    memcpy(b, &operand1.value, 16);
                    memcpy(r, &vectors[0], 16);
                    for (int i = 0; i < 2; i++) {
                        if (std::isnan(a[i]) || std::isnan(b[i]))
                            continue;
                        if (a[i] == 0.0 && b[i] == 0.0)
                            continue;
                        double e = (opcode == VectorRelaxedMin) ? std::min(a[i], b[i]) : std::max(a[i], b[i]);
                        CHECK(r[i] == e);
                    }
                }
            }
        }
    };

    test(SIMDLane::f32x4, VectorRelaxedMin);
    test(SIMDLane::f32x4, VectorRelaxedMax);
    test(SIMDLane::f64x2, VectorRelaxedMin);
    test(SIMDLane::f64x2, VectorRelaxedMax);
}

void testVectorRelaxedQ15Mulr()
{
    alignas(16) v128_t vectors[2];
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    auto arguments = cCallArgumentValues<void*>(proc, root);

    Value* address = arguments[0];
    Value* input0 = root->appendNew<MemoryValue>(proc, Load, V128, Origin(), address);
    Value* input1 = root->appendNew<MemoryValue>(proc, Load, V128, Origin(), address, static_cast<int32_t>(sizeof(v128_t)));
    Value* result = root->appendNew<SIMDValue>(proc, Origin(), VectorRelaxedQ15Mulr, B3::V128, SIMDLane::i16x8, SIMDSignMode::Signed, input0, input1);
    root->appendNew<MemoryValue>(proc, Store, Origin(), result, address);
    root->appendNewControlValue(proc, Return, Origin());

    auto code = compileProc(proc);
    for (auto& operand0 : v128Operands()) {
        for (auto& operand1 : v128Operands()) {
            vectors[0] = operand0.value;
            vectors[1] = operand1.value;
            invoke<void>(*code, vectors);

            v128_t expected { };
            int16_t a[8], b[8], r[8];
            memcpy(a, &operand0.value, 16);
            memcpy(b, &operand1.value, 16);
            bool hasOverflow = false;
            for (int i = 0; i < 8; i++) {
                int32_t product = static_cast<int32_t>(a[i]) * static_cast<int32_t>(b[i]);
                int32_t result = (product + 0x4000) >> 15;
                // Relaxed: overflow case (a[i]==b[i]==-32768) may saturate to 32767 or wrap to -32768
                if (result > 32767 || result < -32768)
                    hasOverflow = true;
                r[i] = static_cast<int16_t>(std::clamp(result, -32768, 32767));
            }
            memcpy(&expected, r, 16);
            if (!hasOverflow)
                CHECK(bitEquals(vectors[0], expected));
        }
    }
}

void testVectorRelaxedDotI8x16I7x16()
{
    alignas(16) v128_t vectors[2];
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    auto arguments = cCallArgumentValues<void*>(proc, root);

    Value* address = arguments[0];
    Value* input0 = root->appendNew<MemoryValue>(proc, Load, V128, Origin(), address);
    Value* input1 = root->appendNew<MemoryValue>(proc, Load, V128, Origin(), address, static_cast<int32_t>(sizeof(v128_t)));
    Value* result = root->appendNew<SIMDValue>(proc, Origin(), VectorRelaxedDotI8x16I7x16, B3::V128, SIMDLane::i16x8, SIMDSignMode::Signed, input0, input1);
    root->appendNew<MemoryValue>(proc, Store, Origin(), result, address);
    root->appendNewControlValue(proc, Return, Origin());

    auto code = compileProc(proc);
    for (auto& operand0 : v128Operands()) {
        for (auto& operand1 : v128Operands()) {
            vectors[0] = operand0.value;
            vectors[1] = operand1.value;
            invoke<void>(*code, vectors);

            // b is i7x16: spec requires b bytes in 0-127 range.
            // When b bytes have high bit set, x86 (unsigned) and ARM64 (signed)
            // interpret differently, giving implementation-defined results.
            // Skip test cases with out-of-range b values.
            uint8_t bRaw[16];
            memcpy(bRaw, &operand1.value, 16);
            bool bInRange = true;
            for (int i = 0; i < 16; i++) {
                if (bRaw[i] > 127) {
                    bInRange = false;
                    break;
                }
            }
            if (!bInRange)
                continue;

            v128_t expected { };
            int8_t a[16];
            int8_t b[16];
            int16_t r[8];
            memcpy(a, &operand0.value, 16);
            memcpy(b, &operand1.value, 16);
            for (int i = 0; i < 8; i++)
                r[i] = static_cast<int16_t>(static_cast<int32_t>(a[2 * i]) * static_cast<int32_t>(b[2 * i]) + static_cast<int32_t>(a[2 * i + 1]) * static_cast<int32_t>(b[2 * i + 1]));
            memcpy(&expected, r, 16);
            CHECK(bitEquals(vectors[0], expected));
        }
    }
}

void testVectorRelaxedDotI8x16I7x16Add()
{
    alignas(16) v128_t vectors[3];
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    auto arguments = cCallArgumentValues<void*>(proc, root);

    Value* address = arguments[0];
    Value* input0 = root->appendNew<MemoryValue>(proc, Load, V128, Origin(), address);
    Value* input1 = root->appendNew<MemoryValue>(proc, Load, V128, Origin(), address, static_cast<int32_t>(sizeof(v128_t)));
    Value* input2 = root->appendNew<MemoryValue>(proc, Load, V128, Origin(), address, static_cast<int32_t>(2 * sizeof(v128_t)));
    Value* result = root->appendNew<SIMDValue>(proc, Origin(), VectorRelaxedDotI8x16I7x16Add, B3::V128, SIMDLane::i32x4, SIMDSignMode::Signed, input0, input1, input2);
    root->appendNew<MemoryValue>(proc, Store, Origin(), result, address);
    root->appendNewControlValue(proc, Return, Origin());

    auto code = compileProc(proc);
    for (auto& operand0 : v128Operands()) {
        for (auto& operand1 : v128Operands()) {
            for (auto& operand2 : v128Operands()) {
                vectors[0] = operand0.value;
                vectors[1] = operand1.value;
                vectors[2] = operand2.value;
                invoke<void>(*code, vectors);

                // b is i7x16: skip out-of-range values (see testVectorRelaxedDotI8x16I7x16)
                uint8_t bRaw[16];
                memcpy(bRaw, &operand1.value, 16);
                bool bInRange = true;
                for (int i = 0; i < 16; i++) {
                    if (bRaw[i] > 127) {
                        bInRange = false;
                        break;
                    }
                }
                if (!bInRange)
                    continue;

                v128_t expected { };
                int8_t a[16], b[16];
                int32_t c[4], r[4];
                memcpy(a, &operand0.value, 16);
                memcpy(b, &operand1.value, 16);
                memcpy(c, &operand2.value, 16);
                for (int i = 0; i < 4; i++) {
                    int32_t sum = 0;
                    for (int j = 0; j < 4; j++)
                        sum += static_cast<int32_t>(a[4 * i + j]) * static_cast<int32_t>(b[4 * i + j]);
                    r[i] = sum + c[i];
                }
                memcpy(&expected, r, 16);
                CHECK(bitEquals(vectors[0], expected));
            }
        }
    }
}

void testInt52RoundTripUnary(int32_t constant)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    auto arguments = cCallArgumentValues<int32_t>(proc, root);
    Value* argA = arguments[0];
    Value* int52A = root->appendNew<Value>(proc, Shl, Origin(), root->appendNew<Value>(proc, SExt32, Origin(), argA), root->appendNew<Const32Value>(proc, Origin(), 12));
    Value* int52B = root->appendNew<Value>(proc, Shl, Origin(), root->appendNew<Value>(proc, SExt32, Origin(), root->appendNew<Const32Value>(proc, Origin(), constant)), root->appendNew<Const32Value>(proc, Origin(), 12));
    Value* node = root->appendNew<Value>(proc, Add, Origin(), int52A, int52B);
    Value* result = root->appendNew<Value>(proc, Trunc, Origin(), root->appendNew<Value>(proc, SShr, Origin(), node, root->appendNew<Const32Value>(proc, Origin(), 12)));
    root->appendNew<Value>(proc, Return, Origin(), result);
    auto code = compileProc(proc);

    for (auto value : int32Operands())
        CHECK_EQ(invoke<int32_t>(*code, value.value), static_cast<int32_t>(((static_cast<int64_t>(value.value) << 12) + (static_cast<int64_t>(constant) << 12)) >> 12));
}

void testInt52RoundTripBinary()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    auto arguments = cCallArgumentValues<int32_t, int32_t>(proc, root);
    Value* argA = arguments[0];
    Value* argB = arguments[1];
    Value* int52A = root->appendNew<Value>(proc, Shl, Origin(), root->appendNew<Value>(proc, SExt32, Origin(), argA), root->appendNew<Const32Value>(proc, Origin(), 12));
    Value* int52B = root->appendNew<Value>(proc, Shl, Origin(), root->appendNew<Value>(proc, SExt32, Origin(), argB), root->appendNew<Const32Value>(proc, Origin(), 12));
    Value* node = root->appendNew<Value>(proc, Add, Origin(), int52A, int52B);
    Value* result = root->appendNew<Value>(proc, Trunc, Origin(), root->appendNew<Value>(proc, SShr, Origin(), node, root->appendNew<Const32Value>(proc, Origin(), 12)));
    root->appendNew<Value>(proc, Return, Origin(), result);
    auto code = compileProc(proc);

    for (auto lhs : int32Operands()) {
        for (auto rhs : int32Operands())
            CHECK_EQ(invoke<int32_t>(*code, lhs.value, rhs.value), static_cast<int32_t>(((static_cast<int64_t>(lhs.value) << 12) + (static_cast<int64_t>(rhs.value) << 12)) >> 12));
    }
}

// Test that Trunc(SShr(Add(@a, unaligned-constant), $12)) produces the correct
// result when the constant is not 12-bit aligned. This pattern arises from
// WebAssembly's i32.wrap_i64(i64.shr_s(i64.add(@a, C), 12)) with arbitrary
// 64-bit values.
void testTruncSShrAddUnalignedConstant()
{
    // Use constant 2048 which is NOT 12-bit aligned (lower 12 bits are non-zero).
    int64_t constant = 2048;

    Procedure proc;
    BasicBlock* root = proc.addBlock();
    auto arguments = cCallArgumentValues<int64_t>(proc, root);
    Value* argA = arguments[0];
    Value* node = root->appendNew<Value>(proc, Add, Origin(), argA, root->appendNew<Const64Value>(proc, Origin(), constant));
    Value* shifted = root->appendNew<Value>(proc, SShr, Origin(), node, root->appendNew<Const32Value>(proc, Origin(), 12));
    Value* result = root->appendNew<Value>(proc, Trunc, Origin(), shifted);
    root->appendNew<Value>(proc, Return, Origin(), result);
    auto code = compileProc(proc);

    // a=2048, C=2048: (2048+2048)>>12 = 4096>>12 = 1
    CHECK_EQ(invoke<int32_t>(*code, static_cast<int64_t>(2048)), static_cast<int32_t>((2048LL + constant) >> 12));
    // a=0
    CHECK_EQ(invoke<int32_t>(*code, static_cast<int64_t>(0)), static_cast<int32_t>((0LL + constant) >> 12));
    // a=4095
    CHECK_EQ(invoke<int32_t>(*code, static_cast<int64_t>(4095)), static_cast<int32_t>((4095LL + constant) >> 12));
    // a=4096
    CHECK_EQ(invoke<int32_t>(*code, static_cast<int64_t>(4096)), static_cast<int32_t>((4096LL + constant) >> 12));
    // Large values
    CHECK_EQ(invoke<int32_t>(*code, static_cast<int64_t>(100000)), static_cast<int32_t>((100000LL + constant) >> 12));
    CHECK_EQ(invoke<int32_t>(*code, static_cast<int64_t>(-2048)), static_cast<int32_t>((-2048LL + constant) >> 12));
}

// Test XOR-and-rotate-right pattern matching for XAR instruction (SHA3).
// Pattern: VectorOr(VectorShl(VectorXor(a, b), shlConst), VectorShr(VectorXor(a, b), shrConst))
// Should be folded into a single XAR instruction on ARM64 with SHA3 support.
void testVectorXorRotateRight64()
{
    if constexpr (!isARM64())
        return;
    if (!isARM64_SHA3())
        return;

    // Test various rotation amounts used in BLAKE2b: 32, 24, 16, 63
    for (int32_t rotateRight : { 1, 16, 24, 32, 63 }) {
        int32_t shiftLeft = 64 - rotateRight;

        Procedure proc;
        BasicBlock* root = proc.addBlock();
        auto arguments = cCallArgumentValues<void*>(proc, root);

        Value* address = arguments[0];

        // Load two vectors
        Value* a = root->appendNew<MemoryValue>(proc, Load, V128, Origin(), address);
        Value* b = root->appendNew<MemoryValue>(proc, Load, V128, Origin(), address, static_cast<int32_t>(sizeof(v128_t)));

        // XOR
        Value* xorResult = root->appendNew<SIMDValue>(proc, Origin(), VectorXor, B3::V128, SIMDLane::v128, SIMDSignMode::None, a, b);

        // Shift left
        Value* shlAmount = root->appendNew<Const32Value>(proc, Origin(), shiftLeft);
        Value* shlResult = root->appendNew<SIMDValue>(proc, Origin(), VectorShl, B3::V128, SIMDLane::i64x2, SIMDSignMode::Unsigned, xorResult, shlAmount);

        // Shift right
        Value* shrAmount = root->appendNew<Const32Value>(proc, Origin(), rotateRight);
        Value* shrResult = root->appendNew<SIMDValue>(proc, Origin(), VectorShr, B3::V128, SIMDLane::i64x2, SIMDSignMode::Unsigned, xorResult, shrAmount);

        // OR = rotate
        Value* orResult = root->appendNew<SIMDValue>(proc, Origin(), VectorOr, B3::V128, SIMDLane::v128, SIMDSignMode::None, shlResult, shrResult);

        root->appendNew<MemoryValue>(proc, Store, Origin(), orResult, address, static_cast<int32_t>(2 * sizeof(v128_t)));
        root->appendNewControlValue(proc, Return, Origin());

        auto code = compileProc(proc);

        // Test with known values
        alignas(16) v128_t vectors[3];
        vectors[0].u64x2[0] = 0x0123456789ABCDEFULL;
        vectors[0].u64x2[1] = 0xFEDCBA9876543210ULL;
        vectors[1].u64x2[0] = 0xFF00FF00FF00FF00ULL;
        vectors[1].u64x2[1] = 0x00FF00FF00FF00FFULL;

        invoke<void>(*code, vectors);

        // Compute expected: (a XOR b) >>> rotateRight
        uint64_t xor0 = vectors[0].u64x2[0] ^ vectors[1].u64x2[0];
        uint64_t xor1 = vectors[0].u64x2[1] ^ vectors[1].u64x2[1];
        uint64_t expected0 = (xor0 >> rotateRight) | (xor0 << (64 - rotateRight));
        uint64_t expected1 = (xor1 >> rotateRight) | (xor1 << (64 - rotateRight));

        CHECK(vectors[2].u64x2[0] == expected0);
        CHECK(vectors[2].u64x2[1] == expected1);
    }
}

// Test dot(v, splat(1)) → extadd_pairwise strength reduction.
// i32x4.dot_i16x8_s(v, {1,1,...}) = i32x4.extadd_pairwise_i16x8_s(v)
void testVectorDotProductSplatOne()
{
    alignas(16) v128_t vectors[3];
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    auto arguments = cCallArgumentValues<void*>(proc, root);
    Value* address = arguments[0];
    Value* input = root->appendNew<MemoryValue>(proc, Load, V128, Origin(), address);

    // Create splat(1) as i16x8
    v128_t splatOne;
    splatOne.u64x2[0] = 0x0001000100010001ULL;
    splatOne.u64x2[1] = 0x0001000100010001ULL;
    Value* ones = root->appendNew<Const128Value>(proc, Origin(), splatOne);

    // dot_i16x8_s(input, splat(1)) should become extadd_pairwise
    Value* result = root->appendNew<SIMDValue>(proc, Origin(), VectorDotProduct, B3::V128, SIMDLane::i32x4, SIMDSignMode::Signed, input, ones);
    root->appendNew<MemoryValue>(proc, Store, Origin(), result, address, static_cast<int32_t>(sizeof(v128_t)));
    root->appendNewControlValue(proc, Return, Origin());

    auto code = compileProc(proc);

    // Test: input = {3, -2, 5, -7, 100, -100, 0, 1} as i16x8
    vectors[0].u16x8[0] = 3; vectors[0].u16x8[1] = static_cast<uint16_t>(-2);
    vectors[0].u16x8[2] = 5; vectors[0].u16x8[3] = static_cast<uint16_t>(-7);
    vectors[0].u16x8[4] = 100; vectors[0].u16x8[5] = static_cast<uint16_t>(-100);
    vectors[0].u16x8[6] = 0; vectors[0].u16x8[7] = 1;
    invoke<void>(*code, vectors);

    // Expected: pairwise sum of adjacent i16 pairs → i32
    // {3 + (-2), 5 + (-7), 100 + (-100), 0 + 1} = {1, -2, 0, 1}
    CHECK(vectors[1].u32x4[0] == static_cast<uint32_t>(1));
    CHECK(vectors[1].u32x4[1] == static_cast<uint32_t>(-2));
    CHECK(vectors[1].u32x4[2] == static_cast<uint32_t>(0));
    CHECK(vectors[1].u32x4[3] == static_cast<uint32_t>(1));

    // Also test commuted form: dot(splat(1), v)
    {
        Procedure proc2;
        BasicBlock* root2 = proc2.addBlock();
        auto args2 = cCallArgumentValues<void*>(proc2, root2);
        Value* addr = args2[0];
        Value* inp = root2->appendNew<MemoryValue>(proc2, Load, V128, Origin(), addr);
        Value* onesVal = root2->appendNew<Const128Value>(proc2, Origin(), splatOne);
        Value* res = root2->appendNew<SIMDValue>(proc2, Origin(), VectorDotProduct, B3::V128, SIMDLane::i32x4, SIMDSignMode::Signed, onesVal, inp);
        root2->appendNew<MemoryValue>(proc2, Store, Origin(), res, addr, static_cast<int32_t>(sizeof(v128_t)));
        root2->appendNewControlValue(proc2, Return, Origin());

        auto code2 = compileProc(proc2);
        invoke<void>(*code2, vectors);
        CHECK(vectors[1].u32x4[0] == static_cast<uint32_t>(1));
        CHECK(vectors[1].u32x4[1] == static_cast<uint32_t>(-2));
        CHECK(vectors[1].u32x4[2] == static_cast<uint32_t>(0));
        CHECK(vectors[1].u32x4[3] == static_cast<uint32_t>(1));
    }
}

// Test EOR3 (3-way XOR) pattern matching: VectorXor(VectorXor(a, b), c) → EOR3(a, b, c)
void testVectorXor3()
{
    if constexpr (!isARM64())
        return;
    if (!isARM64_SHA3())
        return;

    alignas(16) v128_t vectors[4];
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    auto arguments = cCallArgumentValues<void*>(proc, root);
    Value* address = arguments[0];
    Value* a = root->appendNew<MemoryValue>(proc, Load, V128, Origin(), address);
    Value* b = root->appendNew<MemoryValue>(proc, Load, V128, Origin(), address, static_cast<int32_t>(sizeof(v128_t)));
    Value* c = root->appendNew<MemoryValue>(proc, Load, V128, Origin(), address, static_cast<int32_t>(2 * sizeof(v128_t)));

    // a ^ b ^ c
    Value* xorAB = root->appendNew<SIMDValue>(proc, Origin(), VectorXor, B3::V128, SIMDLane::v128, SIMDSignMode::None, a, b);
    Value* xorABC = root->appendNew<SIMDValue>(proc, Origin(), VectorXor, B3::V128, SIMDLane::v128, SIMDSignMode::None, xorAB, c);
    root->appendNew<MemoryValue>(proc, Store, Origin(), xorABC, address, static_cast<int32_t>(3 * sizeof(v128_t)));
    root->appendNewControlValue(proc, Return, Origin());

    auto code = compileProc(proc);

    vectors[0].u64x2[0] = 0x0123456789ABCDEFULL;
    vectors[0].u64x2[1] = 0xFEDCBA9876543210ULL;
    vectors[1].u64x2[0] = 0xFF00FF00FF00FF00ULL;
    vectors[1].u64x2[1] = 0x00FF00FF00FF00FFULL;
    vectors[2].u64x2[0] = 0xAAAAAAAAAAAAAAAAULL;
    vectors[2].u64x2[1] = 0x5555555555555555ULL;
    invoke<void>(*code, vectors);

    CHECK(vectors[3].u64x2[0] == (vectors[0].u64x2[0] ^ vectors[1].u64x2[0] ^ vectors[2].u64x2[0]));
    CHECK(vectors[3].u64x2[1] == (vectors[0].u64x2[1] ^ vectors[1].u64x2[1] ^ vectors[2].u64x2[1]));

    // Also test commuted form: VectorXor(c, VectorXor(a, b))
    {
        Procedure proc2;
        BasicBlock* root2 = proc2.addBlock();
        auto args2 = cCallArgumentValues<void*>(proc2, root2);
        Value* addr = args2[0];
        Value* va = root2->appendNew<MemoryValue>(proc2, Load, V128, Origin(), addr);
        Value* vb = root2->appendNew<MemoryValue>(proc2, Load, V128, Origin(), addr, static_cast<int32_t>(sizeof(v128_t)));
        Value* vc = root2->appendNew<MemoryValue>(proc2, Load, V128, Origin(), addr, static_cast<int32_t>(2 * sizeof(v128_t)));
        Value* xorAB2 = root2->appendNew<SIMDValue>(proc2, Origin(), VectorXor, B3::V128, SIMDLane::v128, SIMDSignMode::None, va, vb);
        Value* result = root2->appendNew<SIMDValue>(proc2, Origin(), VectorXor, B3::V128, SIMDLane::v128, SIMDSignMode::None, vc, xorAB2);
        root2->appendNew<MemoryValue>(proc2, Store, Origin(), result, addr, static_cast<int32_t>(3 * sizeof(v128_t)));
        root2->appendNewControlValue(proc2, Return, Origin());

        auto code2 = compileProc(proc2);
        invoke<void>(*code2, vectors);
        CHECK(vectors[3].u64x2[0] == (vectors[0].u64x2[0] ^ vectors[1].u64x2[0] ^ vectors[2].u64x2[0]));
        CHECK(vectors[3].u64x2[1] == (vectors[0].u64x2[1] ^ vectors[1].u64x2[1] ^ vectors[2].u64x2[1]));
    }
}

// Test VectorUnzipEven/Odd B3 opcodes (reduced from VectorSwizzle in ReduceStrength).
void testVectorUnzipEven()
{
    // UZP1.4S(a, b): extract even 32-bit elements = {a[0], a[2], b[0], b[2]}
    alignas(16) v128_t vectors[3];
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    auto arguments = cCallArgumentValues<void*>(proc, root);
    Value* address = arguments[0];
    Value* a = root->appendNew<MemoryValue>(proc, Load, V128, Origin(), address);
    Value* b = root->appendNew<MemoryValue>(proc, Load, V128, Origin(), address, static_cast<int32_t>(sizeof(v128_t)));
    Value* result = root->appendNew<SIMDValue>(proc, Origin(), VectorUnzipEven, B3::V128, SIMDLane::i32x4, SIMDSignMode::None, a, b);
    root->appendNew<MemoryValue>(proc, Store, Origin(), result, address, static_cast<int32_t>(2 * sizeof(v128_t)));
    root->appendNewControlValue(proc, Return, Origin());

    auto code = compileProc(proc);
    vectors[0].u32x4[0] = 0x11; vectors[0].u32x4[1] = 0x22; vectors[0].u32x4[2] = 0x33; vectors[0].u32x4[3] = 0x44;
    vectors[1].u32x4[0] = 0x55; vectors[1].u32x4[1] = 0x66; vectors[1].u32x4[2] = 0x77; vectors[1].u32x4[3] = 0x88;
    invoke<void>(*code, vectors);
    // {a[0], a[2], b[0], b[2]}
    CHECK(vectors[2].u32x4[0] == 0x11);
    CHECK(vectors[2].u32x4[1] == 0x33);
    CHECK(vectors[2].u32x4[2] == 0x55);
    CHECK(vectors[2].u32x4[3] == 0x77);
}

void testVectorUnzipOdd()
{
    // UZP2.4S(a, b): extract odd 32-bit elements = {a[1], a[3], b[1], b[3]}
    alignas(16) v128_t vectors[3];
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    auto arguments = cCallArgumentValues<void*>(proc, root);
    Value* address = arguments[0];
    Value* a = root->appendNew<MemoryValue>(proc, Load, V128, Origin(), address);
    Value* b = root->appendNew<MemoryValue>(proc, Load, V128, Origin(), address, static_cast<int32_t>(sizeof(v128_t)));
    Value* result = root->appendNew<SIMDValue>(proc, Origin(), VectorUnzipOdd, B3::V128, SIMDLane::i32x4, SIMDSignMode::None, a, b);
    root->appendNew<MemoryValue>(proc, Store, Origin(), result, address, static_cast<int32_t>(2 * sizeof(v128_t)));
    root->appendNewControlValue(proc, Return, Origin());

    auto code = compileProc(proc);
    vectors[0].u32x4[0] = 0x11; vectors[0].u32x4[1] = 0x22; vectors[0].u32x4[2] = 0x33; vectors[0].u32x4[3] = 0x44;
    vectors[1].u32x4[0] = 0x55; vectors[1].u32x4[1] = 0x66; vectors[1].u32x4[2] = 0x77; vectors[1].u32x4[3] = 0x88;
    invoke<void>(*code, vectors);
    // {a[1], a[3], b[1], b[3]}
    CHECK(vectors[2].u32x4[0] == 0x22);
    CHECK(vectors[2].u32x4[1] == 0x44);
    CHECK(vectors[2].u32x4[2] == 0x66);
    CHECK(vectors[2].u32x4[3] == 0x88);
}

void testVectorZipLower()
{
    // ZIP1.4S(a, b): interleave low halves = {a[0], b[0], a[1], b[1]}
    alignas(16) v128_t vectors[3];
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    auto arguments = cCallArgumentValues<void*>(proc, root);
    Value* address = arguments[0];
    Value* a = root->appendNew<MemoryValue>(proc, Load, V128, Origin(), address);
    Value* b = root->appendNew<MemoryValue>(proc, Load, V128, Origin(), address, static_cast<int32_t>(sizeof(v128_t)));
    Value* result = root->appendNew<SIMDValue>(proc, Origin(), VectorZipLower, B3::V128, SIMDLane::i32x4, SIMDSignMode::None, a, b);
    root->appendNew<MemoryValue>(proc, Store, Origin(), result, address, static_cast<int32_t>(2 * sizeof(v128_t)));
    root->appendNewControlValue(proc, Return, Origin());

    auto code = compileProc(proc);
    vectors[0].u32x4[0] = 0x11; vectors[0].u32x4[1] = 0x22; vectors[0].u32x4[2] = 0x33; vectors[0].u32x4[3] = 0x44;
    vectors[1].u32x4[0] = 0x55; vectors[1].u32x4[1] = 0x66; vectors[1].u32x4[2] = 0x77; vectors[1].u32x4[3] = 0x88;
    invoke<void>(*code, vectors);
    // {a[0], b[0], a[1], b[1]}
    CHECK(vectors[2].u32x4[0] == 0x11);
    CHECK(vectors[2].u32x4[1] == 0x55);
    CHECK(vectors[2].u32x4[2] == 0x22);
    CHECK(vectors[2].u32x4[3] == 0x66);
}

void testVectorZipHigher()
{
    // ZIP2.4S(a, b): interleave high halves = {a[2], b[2], a[3], b[3]}
    alignas(16) v128_t vectors[3];
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    auto arguments = cCallArgumentValues<void*>(proc, root);
    Value* address = arguments[0];
    Value* a = root->appendNew<MemoryValue>(proc, Load, V128, Origin(), address);
    Value* b = root->appendNew<MemoryValue>(proc, Load, V128, Origin(), address, static_cast<int32_t>(sizeof(v128_t)));
    Value* result = root->appendNew<SIMDValue>(proc, Origin(), VectorZipHigher, B3::V128, SIMDLane::i32x4, SIMDSignMode::None, a, b);
    root->appendNew<MemoryValue>(proc, Store, Origin(), result, address, static_cast<int32_t>(2 * sizeof(v128_t)));
    root->appendNewControlValue(proc, Return, Origin());

    auto code = compileProc(proc);
    vectors[0].u32x4[0] = 0x11; vectors[0].u32x4[1] = 0x22; vectors[0].u32x4[2] = 0x33; vectors[0].u32x4[3] = 0x44;
    vectors[1].u32x4[0] = 0x55; vectors[1].u32x4[1] = 0x66; vectors[1].u32x4[2] = 0x77; vectors[1].u32x4[3] = 0x88;
    invoke<void>(*code, vectors);
    // {a[2], b[2], a[3], b[3]}
    CHECK(vectors[2].u32x4[0] == 0x33);
    CHECK(vectors[2].u32x4[1] == 0x77);
    CHECK(vectors[2].u32x4[2] == 0x44);
    CHECK(vectors[2].u32x4[3] == 0x88);
}

void testVectorTransposeEven()
{
    // TRN1.4S(a, b): {a[0], b[0], a[2], b[2]}
    alignas(16) v128_t vectors[3];
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    auto arguments = cCallArgumentValues<void*>(proc, root);
    Value* address = arguments[0];
    Value* a = root->appendNew<MemoryValue>(proc, Load, V128, Origin(), address);
    Value* b = root->appendNew<MemoryValue>(proc, Load, V128, Origin(), address, static_cast<int32_t>(sizeof(v128_t)));
    Value* result = root->appendNew<SIMDValue>(proc, Origin(), VectorTransposeEven, B3::V128, SIMDLane::i32x4, SIMDSignMode::None, a, b);
    root->appendNew<MemoryValue>(proc, Store, Origin(), result, address, static_cast<int32_t>(2 * sizeof(v128_t)));
    root->appendNewControlValue(proc, Return, Origin());

    auto code = compileProc(proc);
    vectors[0].u32x4[0] = 0x11; vectors[0].u32x4[1] = 0x22; vectors[0].u32x4[2] = 0x33; vectors[0].u32x4[3] = 0x44;
    vectors[1].u32x4[0] = 0x55; vectors[1].u32x4[1] = 0x66; vectors[1].u32x4[2] = 0x77; vectors[1].u32x4[3] = 0x88;
    invoke<void>(*code, vectors);
    CHECK(vectors[2].u32x4[0] == 0x11);
    CHECK(vectors[2].u32x4[1] == 0x55);
    CHECK(vectors[2].u32x4[2] == 0x33);
    CHECK(vectors[2].u32x4[3] == 0x77);
}

void testVectorTransposeOdd()
{
    // TRN2.4S(a, b): {a[1], b[1], a[3], b[3]}
    alignas(16) v128_t vectors[3];
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    auto arguments = cCallArgumentValues<void*>(proc, root);
    Value* address = arguments[0];
    Value* a = root->appendNew<MemoryValue>(proc, Load, V128, Origin(), address);
    Value* b = root->appendNew<MemoryValue>(proc, Load, V128, Origin(), address, static_cast<int32_t>(sizeof(v128_t)));
    Value* result = root->appendNew<SIMDValue>(proc, Origin(), VectorTransposeOdd, B3::V128, SIMDLane::i32x4, SIMDSignMode::None, a, b);
    root->appendNew<MemoryValue>(proc, Store, Origin(), result, address, static_cast<int32_t>(2 * sizeof(v128_t)));
    root->appendNewControlValue(proc, Return, Origin());

    auto code = compileProc(proc);
    vectors[0].u32x4[0] = 0x11; vectors[0].u32x4[1] = 0x22; vectors[0].u32x4[2] = 0x33; vectors[0].u32x4[3] = 0x44;
    vectors[1].u32x4[0] = 0x55; vectors[1].u32x4[1] = 0x66; vectors[1].u32x4[2] = 0x77; vectors[1].u32x4[3] = 0x88;
    invoke<void>(*code, vectors);
    CHECK(vectors[2].u32x4[0] == 0x22);
    CHECK(vectors[2].u32x4[1] == 0x66);
    CHECK(vectors[2].u32x4[2] == 0x44);
    CHECK(vectors[2].u32x4[3] == 0x88);
}

void testVectorReverse()
{
    // REV64.4S: reverse pairs of 32-bit elements within 64-bit lanes
    // {v[1], v[0], v[3], v[2]}
    alignas(16) v128_t vectors[2];
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    auto arguments = cCallArgumentValues<void*>(proc, root);
    Value* address = arguments[0];
    Value* input = root->appendNew<MemoryValue>(proc, Load, V128, Origin(), address);
    Value* result = root->appendNew<SIMDValue>(proc, Origin(), VectorReverse, B3::V128, SIMDLane::i32x4, SIMDSignMode::None, static_cast<uint8_t>(8), input);
    root->appendNew<MemoryValue>(proc, Store, Origin(), result, address, static_cast<int32_t>(sizeof(v128_t)));
    root->appendNewControlValue(proc, Return, Origin());

    auto code = compileProc(proc);
    vectors[0].u32x4[0] = 0xAA; vectors[0].u32x4[1] = 0xBB; vectors[0].u32x4[2] = 0xCC; vectors[0].u32x4[3] = 0xDD;
    invoke<void>(*code, vectors);
    CHECK(vectors[1].u32x4[0] == 0xBB);
    CHECK(vectors[1].u32x4[1] == 0xAA);
    CHECK(vectors[1].u32x4[2] == 0xDD);
    CHECK(vectors[1].u32x4[3] == 0xCC);
}

// Test that VectorShl(x, 1) is strength-reduced to VectorAdd(x, x).
void testVectorShlByOne()
{
    alignas(16) v128_t vectors[2];
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    auto arguments = cCallArgumentValues<void*>(proc, root);
    Value* address = arguments[0];
    Value* input = root->appendNew<MemoryValue>(proc, Load, V128, Origin(), address);

    // Build VectorShl(input, 1) — will become VectorAdd(input, input) after ReduceStrength.
    Value* shiftAmount = root->appendNew<Const32Value>(proc, Origin(), 1);
    Value* result = root->appendNew<SIMDValue>(proc, Origin(), VectorShl, B3::V128, SIMDLane::i64x2, SIMDSignMode::Unsigned, input, shiftAmount);

    root->appendNew<MemoryValue>(proc, Store, Origin(), result, address, static_cast<int32_t>(sizeof(v128_t)));
    root->appendNewControlValue(proc, Return, Origin());

    auto code = compileProc(proc);

    vectors[0].u64x2[0] = 0x0123456789ABCDEFULL;
    vectors[0].u64x2[1] = 0x8000000000000001ULL;
    invoke<void>(*code, vectors);
    CHECK(vectors[1].u64x2[0] == (0x0123456789ABCDEFULL << 1));
    CHECK(vectors[1].u64x2[1] == (0x8000000000000001ULL << 1));

    // Test with max value (overflow wraps)
    vectors[0].u64x2[0] = UINT64_MAX;
    vectors[0].u64x2[1] = 1;
    invoke<void>(*code, vectors);
    CHECK(vectors[1].u64x2[0] == (UINT64_MAX << 1));
    CHECK(vectors[1].u64x2[1] == 2);
}

template<typename T>
static void testVectorShlImmediateForLane(SIMDLane lane, unsigned shift, T inputVal)
{
    if constexpr (!isARM64())
        return;

    alignas(16) v128_t vectors[2];
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    auto arguments = cCallArgumentValues<void*>(proc, root);
    Value* address = arguments[0];
    Value* input = root->appendNew<MemoryValue>(proc, Load, V128, Origin(), address);
    Value* shiftAmount = root->appendNew<Const32Value>(proc, Origin(), shift);
    Value* result = root->appendNew<SIMDValue>(proc, Origin(), VectorShl, B3::V128, lane, SIMDSignMode::Unsigned, input, shiftAmount);
    root->appendNew<MemoryValue>(proc, Store, Origin(), result, address, static_cast<int32_t>(sizeof(v128_t)));
    root->appendNewControlValue(proc, Return, Origin());

    auto code = compileProc(proc);

    constexpr unsigned numLanes = sizeof(v128_t) / sizeof(T);
    for (unsigned i = 0; i < numLanes; ++i)
        reinterpret_cast<T*>(&vectors[0])[i] = inputVal;
    invoke<void>(*code, vectors);

    unsigned bitWidth = sizeof(T) * 8;
    unsigned maskedShift = shift & (bitWidth - 1);
    for (unsigned i = 0; i < numLanes; ++i) {
        T expected = static_cast<T>(inputVal << maskedShift);
        CHECK(reinterpret_cast<T*>(&vectors[1])[i] == expected);
    }
}

void testVectorShlImmediate()
{
    if constexpr (!isARM64())
        return;

    // i8x16
    testVectorShlImmediateForLane<uint8_t>(SIMDLane::i8x16, 1, static_cast<uint8_t>(0xAB));
    testVectorShlImmediateForLane<uint8_t>(SIMDLane::i8x16, 4, static_cast<uint8_t>(0x0F));
    testVectorShlImmediateForLane<uint8_t>(SIMDLane::i8x16, 7, static_cast<uint8_t>(0x01));

    // i16x8
    testVectorShlImmediateForLane<uint16_t>(SIMDLane::i16x8, 1, static_cast<uint16_t>(0xABCD));
    testVectorShlImmediateForLane<uint16_t>(SIMDLane::i16x8, 8, static_cast<uint16_t>(0x00FF));
    testVectorShlImmediateForLane<uint16_t>(SIMDLane::i16x8, 15, static_cast<uint16_t>(0x0001));

    // i32x4
    testVectorShlImmediateForLane<uint32_t>(SIMDLane::i32x4, 1, 0xDEADBEEFu);
    testVectorShlImmediateForLane<uint32_t>(SIMDLane::i32x4, 16, 0x0000FFFFu);
    testVectorShlImmediateForLane<uint32_t>(SIMDLane::i32x4, 31, 0x00000001u);

    // i64x2
    testVectorShlImmediateForLane<uint64_t>(SIMDLane::i64x2, 1, 0x0123456789ABCDEFull);
    testVectorShlImmediateForLane<uint64_t>(SIMDLane::i64x2, 32, 0x00000000FFFFFFFFull);
    testVectorShlImmediateForLane<uint64_t>(SIMDLane::i64x2, 63, 0x0000000000000001ull);
}

// VectorMulLow on lane L (L in {i16x8, i32x4, i64x2}) maps to NEON UMULL/SMULL
// on the low 64 bits of each input:
//   - i16x8 output: `umull.8h v.8b, v.8b` — 8 u8/i8 multiplicands per operand
//   - i32x4 output: `umull.4s v.4h, v.4h` — 4 u16/i16 multiplicands per operand
//   - i64x2 output: `umull.2d v.2s, v.2s` — 2 u32/i32 multiplicands per operand
// Only the low 64 bits of each input are read; the high 64 bits are ignored.
// The tests below use the matching v128_t member arrays (u8x16, u16x8, u32x4)
// to index into those low-half lanes.

// Accumulate `acc += mul(x, y)` on the chosen lane + sign mode, reading only
// the low half of x and y (which is what UMLAL consumes on ARM64 and what
// VectorMulLow computes). Used as the oracle for all three test patterns.
static void accumulateExtmulLow(v128_t& acc, const v128_t& x, const v128_t& y, SIMDLane outputLane, SIMDSignMode signMode)
{
    switch (outputLane) {
    case SIMDLane::i16x8:
        for (unsigned i = 0; i < 8; ++i) {
            uint16_t prod;
            if (signMode == SIMDSignMode::Unsigned)
                prod = static_cast<uint16_t>(static_cast<uint16_t>(x.u8x16[i]) * static_cast<uint16_t>(y.u8x16[i]));
            else {
                int16_t sprod = static_cast<int16_t>(static_cast<int8_t>(x.u8x16[i])) * static_cast<int16_t>(static_cast<int8_t>(y.u8x16[i]));
                prod = static_cast<uint16_t>(sprod);
            }
            acc.u16x8[i] = static_cast<uint16_t>(acc.u16x8[i] + prod);
        }
        break;
    case SIMDLane::i32x4:
        for (unsigned i = 0; i < 4; ++i) {
            uint32_t prod;
            if (signMode == SIMDSignMode::Unsigned)
                prod = static_cast<uint32_t>(x.u16x8[i]) * static_cast<uint32_t>(y.u16x8[i]);
            else {
                int32_t sprod = static_cast<int32_t>(static_cast<int16_t>(x.u16x8[i])) * static_cast<int32_t>(static_cast<int16_t>(y.u16x8[i]));
                prod = static_cast<uint32_t>(sprod);
            }
            acc.u32x4[i] = acc.u32x4[i] + prod;
        }
        break;
    case SIMDLane::i64x2:
        for (unsigned i = 0; i < 2; ++i) {
            uint64_t prod;
            if (signMode == SIMDSignMode::Unsigned)
                prod = static_cast<uint64_t>(x.u32x4[i]) * static_cast<uint64_t>(y.u32x4[i]);
            else {
                int64_t sprod = static_cast<int64_t>(static_cast<int32_t>(x.u32x4[i])) * static_cast<int64_t>(static_cast<int32_t>(y.u32x4[i]));
                prod = static_cast<uint64_t>(sprod);
            }
            acc.u64x2[i] = acc.u64x2[i] + prod;
        }
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

// Accumulate `acc += mul(x, y)` on the chosen lane + sign mode, reading only
// the high half of x and y (which is what UMLAL2/SMLAL2 consume on ARM64 and
// what VectorMulHigh computes). Mirror of accumulateExtmulLow but indexing
// the upper 64 bits.
static void accumulateExtmulHigh(v128_t& acc, const v128_t& x, const v128_t& y, SIMDLane outputLane, SIMDSignMode signMode)
{
    switch (outputLane) {
    case SIMDLane::i16x8:
        for (unsigned i = 0; i < 8; ++i) {
            uint16_t prod;
            if (signMode == SIMDSignMode::Unsigned)
                prod = static_cast<uint16_t>(static_cast<uint16_t>(x.u8x16[8 + i]) * static_cast<uint16_t>(y.u8x16[8 + i]));
            else {
                int16_t sprod = static_cast<int16_t>(static_cast<int8_t>(x.u8x16[8 + i])) * static_cast<int16_t>(static_cast<int8_t>(y.u8x16[8 + i]));
                prod = static_cast<uint16_t>(sprod);
            }
            acc.u16x8[i] = static_cast<uint16_t>(acc.u16x8[i] + prod);
        }
        break;
    case SIMDLane::i32x4:
        for (unsigned i = 0; i < 4; ++i) {
            uint32_t prod;
            if (signMode == SIMDSignMode::Unsigned)
                prod = static_cast<uint32_t>(x.u16x8[4 + i]) * static_cast<uint32_t>(y.u16x8[4 + i]);
            else {
                int32_t sprod = static_cast<int32_t>(static_cast<int16_t>(x.u16x8[4 + i])) * static_cast<int32_t>(static_cast<int16_t>(y.u16x8[4 + i]));
                prod = static_cast<uint32_t>(sprod);
            }
            acc.u32x4[i] = acc.u32x4[i] + prod;
        }
        break;
    case SIMDLane::i64x2:
        for (unsigned i = 0; i < 2; ++i) {
            uint64_t prod;
            if (signMode == SIMDSignMode::Unsigned)
                prod = static_cast<uint64_t>(x.u32x4[2 + i]) * static_cast<uint64_t>(y.u32x4[2 + i]);
            else {
                int64_t sprod = static_cast<int64_t>(static_cast<int32_t>(x.u32x4[2 + i])) * static_cast<int64_t>(static_cast<int32_t>(y.u32x4[2 + i]));
                prod = static_cast<uint64_t>(sprod);
            }
            acc.u64x2[i] = acc.u64x2[i] + prod;
        }
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

// Pattern A: VectorAdd(acc, VectorMulLow(x, y, lane)) -> single UMLAL.
void testVectorMulAddLowSimple()
{
    if constexpr (!isARM64())
        return;

    auto test = [&](SIMDLane lane, SIMDSignMode signMode) {
        alignas(16) v128_t vectors[4]; // acc, x, y, result
        Procedure proc;
        BasicBlock* root = proc.addBlock();
        auto arguments = cCallArgumentValues<void*>(proc, root);
        Value* address = arguments[0];
        Value* acc = root->appendNew<MemoryValue>(proc, Load, V128, Origin(), address);
        Value* x = root->appendNew<MemoryValue>(proc, Load, V128, Origin(), address, static_cast<int32_t>(sizeof(v128_t)));
        Value* y = root->appendNew<MemoryValue>(proc, Load, V128, Origin(), address, static_cast<int32_t>(2 * sizeof(v128_t)));
        Value* mul = root->appendNew<SIMDValue>(proc, Origin(), VectorMulLow, B3::V128, lane, signMode, x, y);
        Value* result = root->appendNew<SIMDValue>(proc, Origin(), VectorAdd, B3::V128, lane, SIMDSignMode::None, acc, mul);
        root->appendNew<MemoryValue>(proc, Store, Origin(), result, address, static_cast<int32_t>(3 * sizeof(v128_t)));
        root->appendNewControlValue(proc, Return, Origin());

        auto code = compileProc(proc);
        for (auto& accOp : v128Operands()) {
            for (auto& xOp : v128Operands()) {
                for (auto& yOp : v128Operands()) {
                    vectors[0] = accOp.value;
                    vectors[1] = xOp.value;
                    vectors[2] = yOp.value;
                    vectors[3] = v128_t { };
                    invoke<void>(*code, vectors);
                    v128_t expected = accOp.value;
                    accumulateExtmulLow(expected, xOp.value, yOp.value, lane, signMode);
                    CHECK(bitEquals(vectors[3], expected));
                }
            }
        }
    };

    for (SIMDLane lane : { SIMDLane::i16x8, SIMDLane::i32x4, SIMDLane::i64x2 }) {
        test(lane, SIMDSignMode::Unsigned);
        test(lane, SIMDSignMode::Signed);
    }
}

// Pattern B: VectorAdd(acc, VectorShl(VectorMulLow(x, y, lane), 1))
// After strength reduction this becomes VectorAdd(acc, VectorAdd(M, M)).
// -> two UMLALs with the same operands (argon2 fBlaMka shape).
void testVectorMulAddLowDoubled()
{
    if constexpr (!isARM64())
        return;

    auto test = [&](SIMDLane lane, SIMDSignMode signMode) {
        alignas(16) v128_t vectors[4];
        Procedure proc;
        BasicBlock* root = proc.addBlock();
        auto arguments = cCallArgumentValues<void*>(proc, root);
        Value* address = arguments[0];
        Value* acc = root->appendNew<MemoryValue>(proc, Load, V128, Origin(), address);
        Value* x = root->appendNew<MemoryValue>(proc, Load, V128, Origin(), address, static_cast<int32_t>(sizeof(v128_t)));
        Value* y = root->appendNew<MemoryValue>(proc, Load, V128, Origin(), address, static_cast<int32_t>(2 * sizeof(v128_t)));
        Value* mul = root->appendNew<SIMDValue>(proc, Origin(), VectorMulLow, B3::V128, lane, signMode, x, y);
        Value* one = root->appendNew<Const32Value>(proc, Origin(), 1);
        Value* doubled = root->appendNew<SIMDValue>(proc, Origin(), VectorShl, B3::V128, lane, SIMDSignMode::None, mul, one);
        Value* result = root->appendNew<SIMDValue>(proc, Origin(), VectorAdd, B3::V128, lane, SIMDSignMode::None, acc, doubled);
        root->appendNew<MemoryValue>(proc, Store, Origin(), result, address, static_cast<int32_t>(3 * sizeof(v128_t)));
        root->appendNewControlValue(proc, Return, Origin());

        auto code = compileProc(proc);
        for (auto& accOp : v128Operands()) {
            for (auto& xOp : v128Operands()) {
                for (auto& yOp : v128Operands()) {
                    vectors[0] = accOp.value;
                    vectors[1] = xOp.value;
                    vectors[2] = yOp.value;
                    vectors[3] = v128_t { };
                    invoke<void>(*code, vectors);
                    v128_t expected = accOp.value;
                    accumulateExtmulLow(expected, xOp.value, yOp.value, lane, signMode);
                    accumulateExtmulLow(expected, xOp.value, yOp.value, lane, signMode);
                    CHECK(bitEquals(vectors[3], expected));
                }
            }
        }
    };

    for (SIMDLane lane : { SIMDLane::i16x8, SIMDLane::i32x4, SIMDLane::i64x2 }) {
        test(lane, SIMDSignMode::Unsigned);
        test(lane, SIMDSignMode::Signed);
    }
}

// Pattern C: VectorAdd(acc, VectorAdd(M1, M2)) where M1, M2 are distinct muls
// -> two UMLALs with different operands. Mixes signed/unsigned across the pair
// to exercise per-mul sign mode.
void testVectorMulAddLowTwoMuls()
{
    if constexpr (!isARM64())
        return;

    auto test = [&](SIMDLane lane, SIMDSignMode signMode1, SIMDSignMode signMode2) {
        alignas(16) v128_t vectors[6]; // acc, x1, y1, x2, y2, result
        Procedure proc;
        BasicBlock* root = proc.addBlock();
        auto arguments = cCallArgumentValues<void*>(proc, root);
        Value* address = arguments[0];
        Value* acc = root->appendNew<MemoryValue>(proc, Load, V128, Origin(), address);
        Value* x1 = root->appendNew<MemoryValue>(proc, Load, V128, Origin(), address, static_cast<int32_t>(sizeof(v128_t)));
        Value* y1 = root->appendNew<MemoryValue>(proc, Load, V128, Origin(), address, static_cast<int32_t>(2 * sizeof(v128_t)));
        Value* x2 = root->appendNew<MemoryValue>(proc, Load, V128, Origin(), address, static_cast<int32_t>(3 * sizeof(v128_t)));
        Value* y2 = root->appendNew<MemoryValue>(proc, Load, V128, Origin(), address, static_cast<int32_t>(4 * sizeof(v128_t)));
        Value* mul1 = root->appendNew<SIMDValue>(proc, Origin(), VectorMulLow, B3::V128, lane, signMode1, x1, y1);
        Value* mul2 = root->appendNew<SIMDValue>(proc, Origin(), VectorMulLow, B3::V128, lane, signMode2, x2, y2);
        Value* sumOfMuls = root->appendNew<SIMDValue>(proc, Origin(), VectorAdd, B3::V128, lane, SIMDSignMode::None, mul1, mul2);
        Value* result = root->appendNew<SIMDValue>(proc, Origin(), VectorAdd, B3::V128, lane, SIMDSignMode::None, acc, sumOfMuls);
        root->appendNew<MemoryValue>(proc, Store, Origin(), result, address, static_cast<int32_t>(5 * sizeof(v128_t)));
        root->appendNewControlValue(proc, Return, Origin());

        auto code = compileProc(proc);
        auto operands = v128Operands();
        for (unsigned aIdx = 0; aIdx < operands.size(); aIdx += 3) {
            for (unsigned bIdx = 0; bIdx < operands.size(); bIdx += 2) {
                vectors[0] = operands[aIdx].value;
                vectors[1] = operands[bIdx].value;
                vectors[2] = operands[(bIdx + 1) % operands.size()].value;
                vectors[3] = operands[(bIdx + 3) % operands.size()].value;
                vectors[4] = operands[(bIdx + 5) % operands.size()].value;
                vectors[5] = v128_t { };
                invoke<void>(*code, vectors);
                v128_t expected = vectors[0];
                accumulateExtmulLow(expected, vectors[1], vectors[2], lane, signMode1);
                accumulateExtmulLow(expected, vectors[3], vectors[4], lane, signMode2);
                CHECK(bitEquals(vectors[5], expected));
            }
        }
    };

    for (SIMDLane lane : { SIMDLane::i16x8, SIMDLane::i32x4, SIMDLane::i64x2 }) {
        test(lane, SIMDSignMode::Unsigned, SIMDSignMode::Unsigned);
        test(lane, SIMDSignMode::Signed, SIMDSignMode::Signed);
        test(lane, SIMDSignMode::Unsigned, SIMDSignMode::Signed);
        test(lane, SIMDSignMode::Signed, SIMDSignMode::Unsigned);
    }
}

// End-to-end: reproduces the argon2 fBlaMka result for two i64 lanes.
//   result = x + y + 2 * (lo32(x) * lo32(y))
// Uses VectorSwizzle to pack the low-32 of each i64 lane, mirroring what
// clang emits for i64x2.extmul_low_i32x4_u(i8x16.shuffle(x), i8x16.shuffle(y)).
void testVectorMulAddLowBlaMka()
{
    if constexpr (!isARM64())
        return;

    alignas(16) v128_t vectors[3]; // x, y, result
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    auto arguments = cCallArgumentValues<void*>(proc, root);
    Value* address = arguments[0];
    Value* x = root->appendNew<MemoryValue>(proc, Load, V128, Origin(), address);
    Value* y = root->appendNew<MemoryValue>(proc, Load, V128, Origin(), address, static_cast<int32_t>(sizeof(v128_t)));

    // Swizzle that packs bytes [0..3, 8..11] of each operand into the low 8 bytes
    // (same pattern clang uses to feed i64x2.extmul_low_i32x4_u).
    v128_t swizzleMask;
    for (unsigned i = 0; i < 4; ++i)
        swizzleMask.u8x16[i] = i;
    for (unsigned i = 0; i < 4; ++i)
        swizzleMask.u8x16[4 + i] = 8 + i;
    for (unsigned i = 0; i < 4; ++i)
        swizzleMask.u8x16[8 + i] = i;
    for (unsigned i = 0; i < 4; ++i)
        swizzleMask.u8x16[12 + i] = 8 + i;
    Value* mask = root->appendNew<Const128Value>(proc, Origin(), swizzleMask);
    Value* xLow = root->appendNew<SIMDValue>(proc, Origin(), VectorSwizzle, B3::V128, SIMDLane::i8x16, SIMDSignMode::None, x, x, mask);
    Value* yLow = root->appendNew<SIMDValue>(proc, Origin(), VectorSwizzle, B3::V128, SIMDLane::i8x16, SIMDSignMode::None, y, y, mask);
    Value* mul = root->appendNew<SIMDValue>(proc, Origin(), VectorMulLow, B3::V128, SIMDLane::i64x2, SIMDSignMode::Unsigned, xLow, yLow);
    Value* one = root->appendNew<Const32Value>(proc, Origin(), 1);
    Value* doubled = root->appendNew<SIMDValue>(proc, Origin(), VectorShl, B3::V128, SIMDLane::i64x2, SIMDSignMode::None, mul, one);
    Value* xPlusY = root->appendNew<SIMDValue>(proc, Origin(), VectorAdd, B3::V128, SIMDLane::i64x2, SIMDSignMode::None, x, y);
    Value* result = root->appendNew<SIMDValue>(proc, Origin(), VectorAdd, B3::V128, SIMDLane::i64x2, SIMDSignMode::None, xPlusY, doubled);
    root->appendNew<MemoryValue>(proc, Store, Origin(), result, address, static_cast<int32_t>(2 * sizeof(v128_t)));
    root->appendNewControlValue(proc, Return, Origin());

    auto code = compileProc(proc);
    auto blamka = [](uint64_t xv, uint64_t yv) {
        const uint64_t m = 0xFFFFFFFFull;
        uint64_t xy = (xv & m) * (yv & m);
        return xv + yv + 2 * xy;
    };

    // Cover interesting combinations including values with non-zero high halves
    // (which must not affect the product since only low32 is used).
    uint64_t xValues[] = { 0, 1, 0xFFFFFFFFull, 0x0000000100000002ull, 0xDEADBEEFCAFEBABEull, 0x8000000080000000ull };
    uint64_t yValues[] = { 0, 1, 3, 0xFFFFFFFFull, 0x1234567890ABCDEFull, 0x7FFFFFFFFFFFFFFFull };
    for (uint64_t xv : xValues) {
        for (uint64_t yv : yValues) {
            vectors[0].u64x2[0] = xv;
            vectors[0].u64x2[1] = ~xv;
            vectors[1].u64x2[0] = yv;
            vectors[1].u64x2[1] = ~yv;
            vectors[2] = v128_t { };
            invoke<void>(*code, vectors);
            CHECK(vectors[2].u64x2[0] == blamka(xv, yv));
            CHECK(vectors[2].u64x2[1] == blamka(~xv, ~yv));
        }
    }
}

// Pattern A for the High half: VectorAdd(acc, VectorMulHigh(x, y, lane))
// -> single UMLAL2/SMLAL2. Only the upper 64 bits of each input are read.
void testVectorMulAddHighSimple()
{
    if constexpr (!isARM64())
        return;

    auto test = [&](SIMDLane lane, SIMDSignMode signMode) {
        alignas(16) v128_t vectors[4]; // acc, x, y, result
        Procedure proc;
        BasicBlock* root = proc.addBlock();
        auto arguments = cCallArgumentValues<void*>(proc, root);
        Value* address = arguments[0];
        Value* acc = root->appendNew<MemoryValue>(proc, Load, V128, Origin(), address);
        Value* x = root->appendNew<MemoryValue>(proc, Load, V128, Origin(), address, static_cast<int32_t>(sizeof(v128_t)));
        Value* y = root->appendNew<MemoryValue>(proc, Load, V128, Origin(), address, static_cast<int32_t>(2 * sizeof(v128_t)));
        Value* mul = root->appendNew<SIMDValue>(proc, Origin(), VectorMulHigh, B3::V128, lane, signMode, x, y);
        Value* result = root->appendNew<SIMDValue>(proc, Origin(), VectorAdd, B3::V128, lane, SIMDSignMode::None, acc, mul);
        root->appendNew<MemoryValue>(proc, Store, Origin(), result, address, static_cast<int32_t>(3 * sizeof(v128_t)));
        root->appendNewControlValue(proc, Return, Origin());

        auto code = compileProc(proc);
        for (auto& accOp : v128Operands()) {
            for (auto& xOp : v128Operands()) {
                for (auto& yOp : v128Operands()) {
                    vectors[0] = accOp.value;
                    vectors[1] = xOp.value;
                    vectors[2] = yOp.value;
                    vectors[3] = v128_t { };
                    invoke<void>(*code, vectors);
                    v128_t expected = accOp.value;
                    accumulateExtmulHigh(expected, xOp.value, yOp.value, lane, signMode);
                    CHECK(bitEquals(vectors[3], expected));
                }
            }
        }
    };

    for (SIMDLane lane : { SIMDLane::i16x8, SIMDLane::i32x4, SIMDLane::i64x2 }) {
        test(lane, SIMDSignMode::Unsigned);
        test(lane, SIMDSignMode::Signed);
    }
}

// Pattern B for the High half: VectorAdd(acc, VectorShl(VectorMulHigh(x, y), 1))
// After strength reduction this becomes VectorAdd(acc, VectorAdd(M, M))
// with the same node on both sides. -> two UMLAL2/SMLAL2s.
void testVectorMulAddHighDoubled()
{
    if constexpr (!isARM64())
        return;

    auto test = [&](SIMDLane lane, SIMDSignMode signMode) {
        alignas(16) v128_t vectors[4];
        Procedure proc;
        BasicBlock* root = proc.addBlock();
        auto arguments = cCallArgumentValues<void*>(proc, root);
        Value* address = arguments[0];
        Value* acc = root->appendNew<MemoryValue>(proc, Load, V128, Origin(), address);
        Value* x = root->appendNew<MemoryValue>(proc, Load, V128, Origin(), address, static_cast<int32_t>(sizeof(v128_t)));
        Value* y = root->appendNew<MemoryValue>(proc, Load, V128, Origin(), address, static_cast<int32_t>(2 * sizeof(v128_t)));
        Value* mul = root->appendNew<SIMDValue>(proc, Origin(), VectorMulHigh, B3::V128, lane, signMode, x, y);
        Value* one = root->appendNew<Const32Value>(proc, Origin(), 1);
        Value* doubled = root->appendNew<SIMDValue>(proc, Origin(), VectorShl, B3::V128, lane, SIMDSignMode::None, mul, one);
        Value* result = root->appendNew<SIMDValue>(proc, Origin(), VectorAdd, B3::V128, lane, SIMDSignMode::None, acc, doubled);
        root->appendNew<MemoryValue>(proc, Store, Origin(), result, address, static_cast<int32_t>(3 * sizeof(v128_t)));
        root->appendNewControlValue(proc, Return, Origin());

        auto code = compileProc(proc);
        for (auto& accOp : v128Operands()) {
            for (auto& xOp : v128Operands()) {
                for (auto& yOp : v128Operands()) {
                    vectors[0] = accOp.value;
                    vectors[1] = xOp.value;
                    vectors[2] = yOp.value;
                    vectors[3] = v128_t { };
                    invoke<void>(*code, vectors);
                    v128_t expected = accOp.value;
                    accumulateExtmulHigh(expected, xOp.value, yOp.value, lane, signMode);
                    accumulateExtmulHigh(expected, xOp.value, yOp.value, lane, signMode);
                    CHECK(bitEquals(vectors[3], expected));
                }
            }
        }
    };

    for (SIMDLane lane : { SIMDLane::i16x8, SIMDLane::i32x4, SIMDLane::i64x2 }) {
        test(lane, SIMDSignMode::Unsigned);
        test(lane, SIMDSignMode::Signed);
    }
}

// Pattern C for the High half: VectorAdd(acc, VectorAdd(M1, M2)) where M1, M2
// are distinct VectorMulHigh nodes. -> two UMLAL2/SMLAL2s with different operands.
void testVectorMulAddHighTwoMuls()
{
    if constexpr (!isARM64())
        return;

    auto test = [&](SIMDLane lane, SIMDSignMode signMode1, SIMDSignMode signMode2) {
        alignas(16) v128_t vectors[6]; // acc, x1, y1, x2, y2, result
        Procedure proc;
        BasicBlock* root = proc.addBlock();
        auto arguments = cCallArgumentValues<void*>(proc, root);
        Value* address = arguments[0];
        Value* acc = root->appendNew<MemoryValue>(proc, Load, V128, Origin(), address);
        Value* x1 = root->appendNew<MemoryValue>(proc, Load, V128, Origin(), address, static_cast<int32_t>(sizeof(v128_t)));
        Value* y1 = root->appendNew<MemoryValue>(proc, Load, V128, Origin(), address, static_cast<int32_t>(2 * sizeof(v128_t)));
        Value* x2 = root->appendNew<MemoryValue>(proc, Load, V128, Origin(), address, static_cast<int32_t>(3 * sizeof(v128_t)));
        Value* y2 = root->appendNew<MemoryValue>(proc, Load, V128, Origin(), address, static_cast<int32_t>(4 * sizeof(v128_t)));
        Value* mul1 = root->appendNew<SIMDValue>(proc, Origin(), VectorMulHigh, B3::V128, lane, signMode1, x1, y1);
        Value* mul2 = root->appendNew<SIMDValue>(proc, Origin(), VectorMulHigh, B3::V128, lane, signMode2, x2, y2);
        Value* sumOfMuls = root->appendNew<SIMDValue>(proc, Origin(), VectorAdd, B3::V128, lane, SIMDSignMode::None, mul1, mul2);
        Value* result = root->appendNew<SIMDValue>(proc, Origin(), VectorAdd, B3::V128, lane, SIMDSignMode::None, acc, sumOfMuls);
        root->appendNew<MemoryValue>(proc, Store, Origin(), result, address, static_cast<int32_t>(5 * sizeof(v128_t)));
        root->appendNewControlValue(proc, Return, Origin());

        auto code = compileProc(proc);
        auto operands = v128Operands();
        for (unsigned aIdx = 0; aIdx < operands.size(); aIdx += 3) {
            for (unsigned bIdx = 0; bIdx < operands.size(); bIdx += 2) {
                vectors[0] = operands[aIdx].value;
                vectors[1] = operands[bIdx].value;
                vectors[2] = operands[(bIdx + 1) % operands.size()].value;
                vectors[3] = operands[(bIdx + 3) % operands.size()].value;
                vectors[4] = operands[(bIdx + 5) % operands.size()].value;
                vectors[5] = v128_t { };
                invoke<void>(*code, vectors);
                v128_t expected = vectors[0];
                accumulateExtmulHigh(expected, vectors[1], vectors[2], lane, signMode1);
                accumulateExtmulHigh(expected, vectors[3], vectors[4], lane, signMode2);
                CHECK(bitEquals(vectors[5], expected));
            }
        }
    };

    for (SIMDLane lane : { SIMDLane::i16x8, SIMDLane::i32x4, SIMDLane::i64x2 }) {
        test(lane, SIMDSignMode::Unsigned, SIMDSignMode::Unsigned);
        test(lane, SIMDSignMode::Signed, SIMDSignMode::Signed);
        test(lane, SIMDSignMode::Unsigned, SIMDSignMode::Signed);
        test(lane, SIMDSignMode::Signed, SIMDSignMode::Unsigned);
    }
}

// The quantized-matmul bread-and-butter: VectorAdd(acc, VectorAdd(MulLow, MulHigh)).
// Both halves of a single (x, y) pair are consumed in one iteration, so we expect
// exactly one UMLAL + one UMLAL2 (or SMLAL + SMLAL2). Tests all four orderings
// (Low+High, High+Low) and both Low/High sign-mode combos since they are
// independent per-Mul.
void testVectorMulAddMixedLowHigh()
{
    if constexpr (!isARM64())
        return;

    auto test = [&](SIMDLane lane, SIMDSignMode signModeLow, SIMDSignMode signModeHigh, bool highFirst) {
        alignas(16) v128_t vectors[4]; // acc, x, y, result
        Procedure proc;
        BasicBlock* root = proc.addBlock();
        auto arguments = cCallArgumentValues<void*>(proc, root);
        Value* address = arguments[0];
        Value* acc = root->appendNew<MemoryValue>(proc, Load, V128, Origin(), address);
        Value* x = root->appendNew<MemoryValue>(proc, Load, V128, Origin(), address, static_cast<int32_t>(sizeof(v128_t)));
        Value* y = root->appendNew<MemoryValue>(proc, Load, V128, Origin(), address, static_cast<int32_t>(2 * sizeof(v128_t)));
        Value* mulLow = root->appendNew<SIMDValue>(proc, Origin(), VectorMulLow, B3::V128, lane, signModeLow, x, y);
        Value* mulHigh = root->appendNew<SIMDValue>(proc, Origin(), VectorMulHigh, B3::V128, lane, signModeHigh, x, y);
        Value* sumOfMuls = highFirst
            ? root->appendNew<SIMDValue>(proc, Origin(), VectorAdd, B3::V128, lane, SIMDSignMode::None, mulHigh, mulLow)
            : root->appendNew<SIMDValue>(proc, Origin(), VectorAdd, B3::V128, lane, SIMDSignMode::None, mulLow, mulHigh);
        Value* result = root->appendNew<SIMDValue>(proc, Origin(), VectorAdd, B3::V128, lane, SIMDSignMode::None, acc, sumOfMuls);
        root->appendNew<MemoryValue>(proc, Store, Origin(), result, address, static_cast<int32_t>(3 * sizeof(v128_t)));
        root->appendNewControlValue(proc, Return, Origin());

        auto code = compileProc(proc);
        for (auto& accOp : v128Operands()) {
            for (auto& xOp : v128Operands()) {
                for (auto& yOp : v128Operands()) {
                    vectors[0] = accOp.value;
                    vectors[1] = xOp.value;
                    vectors[2] = yOp.value;
                    vectors[3] = v128_t { };
                    invoke<void>(*code, vectors);
                    v128_t expected = accOp.value;
                    accumulateExtmulLow(expected, xOp.value, yOp.value, lane, signModeLow);
                    accumulateExtmulHigh(expected, xOp.value, yOp.value, lane, signModeHigh);
                    CHECK(bitEquals(vectors[3], expected));
                }
            }
        }
    };

    for (SIMDLane lane : { SIMDLane::i16x8, SIMDLane::i32x4, SIMDLane::i64x2 }) {
        for (bool highFirst : { false, true }) {
            test(lane, SIMDSignMode::Unsigned, SIMDSignMode::Unsigned, highFirst);
            test(lane, SIMDSignMode::Signed, SIMDSignMode::Signed, highFirst);
            test(lane, SIMDSignMode::Unsigned, SIMDSignMode::Signed, highFirst);
            test(lane, SIMDSignMode::Signed, SIMDSignMode::Unsigned, highFirst);
        }
    }
}

template<typename T, typename SignedT>
static void testVectorShrImmediateForLane(SIMDLane lane, SIMDSignMode signMode, unsigned shift, T inputVal)
{
    if constexpr (!isARM64())
        return;

    alignas(16) v128_t vectors[2];
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    auto arguments = cCallArgumentValues<void*>(proc, root);
    Value* address = arguments[0];
    Value* input = root->appendNew<MemoryValue>(proc, Load, V128, Origin(), address);
    Value* shiftAmount = root->appendNew<Const32Value>(proc, Origin(), shift);
    Value* result = root->appendNew<SIMDValue>(proc, Origin(), VectorShr, B3::V128, lane, signMode, input, shiftAmount);
    root->appendNew<MemoryValue>(proc, Store, Origin(), result, address, static_cast<int32_t>(sizeof(v128_t)));
    root->appendNewControlValue(proc, Return, Origin());

    auto code = compileProc(proc);

    constexpr unsigned numLanes = sizeof(v128_t) / sizeof(T);
    for (unsigned i = 0; i < numLanes; ++i)
        reinterpret_cast<T*>(&vectors[0])[i] = inputVal;
    invoke<void>(*code, vectors);

    unsigned bitWidth = sizeof(T) * 8;
    unsigned maskedShift = shift & (bitWidth - 1);
    for (unsigned i = 0; i < numLanes; ++i) {
        T expected;
        if (signMode == SIMDSignMode::Signed)
            expected = static_cast<T>(static_cast<SignedT>(inputVal) >> maskedShift);
        else
            expected = static_cast<T>(inputVal >> maskedShift);
        CHECK(reinterpret_cast<T*>(&vectors[1])[i] == expected);
    }
}

void testVectorShrImmediate()
{
    if constexpr (!isARM64())
        return;

    // i8x16 unsigned
    testVectorShrImmediateForLane<uint8_t, int8_t>(SIMDLane::i8x16, SIMDSignMode::Unsigned, 1, static_cast<uint8_t>(0xAB));
    testVectorShrImmediateForLane<uint8_t, int8_t>(SIMDLane::i8x16, SIMDSignMode::Unsigned, 4, static_cast<uint8_t>(0xFF));
    testVectorShrImmediateForLane<uint8_t, int8_t>(SIMDLane::i8x16, SIMDSignMode::Unsigned, 7, static_cast<uint8_t>(0x80));

    // i8x16 signed
    testVectorShrImmediateForLane<uint8_t, int8_t>(SIMDLane::i8x16, SIMDSignMode::Signed, 1, static_cast<uint8_t>(0xAB));
    testVectorShrImmediateForLane<uint8_t, int8_t>(SIMDLane::i8x16, SIMDSignMode::Signed, 4, static_cast<uint8_t>(0xFF));
    testVectorShrImmediateForLane<uint8_t, int8_t>(SIMDLane::i8x16, SIMDSignMode::Signed, 7, static_cast<uint8_t>(0x80));

    // i16x8 unsigned
    testVectorShrImmediateForLane<uint16_t, int16_t>(SIMDLane::i16x8, SIMDSignMode::Unsigned, 1, static_cast<uint16_t>(0xABCD));
    testVectorShrImmediateForLane<uint16_t, int16_t>(SIMDLane::i16x8, SIMDSignMode::Unsigned, 8, static_cast<uint16_t>(0xFF00));
    testVectorShrImmediateForLane<uint16_t, int16_t>(SIMDLane::i16x8, SIMDSignMode::Unsigned, 15, static_cast<uint16_t>(0x8000));

    // i16x8 signed
    testVectorShrImmediateForLane<uint16_t, int16_t>(SIMDLane::i16x8, SIMDSignMode::Signed, 1, static_cast<uint16_t>(0xABCD));
    testVectorShrImmediateForLane<uint16_t, int16_t>(SIMDLane::i16x8, SIMDSignMode::Signed, 8, static_cast<uint16_t>(0xFF00));

    // i32x4 unsigned
    testVectorShrImmediateForLane<uint32_t, int32_t>(SIMDLane::i32x4, SIMDSignMode::Unsigned, 1, 0xDEADBEEFu);
    testVectorShrImmediateForLane<uint32_t, int32_t>(SIMDLane::i32x4, SIMDSignMode::Unsigned, 16, 0xFFFF0000u);
    testVectorShrImmediateForLane<uint32_t, int32_t>(SIMDLane::i32x4, SIMDSignMode::Unsigned, 31, 0x80000000u);

    // i32x4 signed
    testVectorShrImmediateForLane<uint32_t, int32_t>(SIMDLane::i32x4, SIMDSignMode::Signed, 1, 0xDEADBEEFu);
    testVectorShrImmediateForLane<uint32_t, int32_t>(SIMDLane::i32x4, SIMDSignMode::Signed, 16, 0xFFFF0000u);

    // i64x2 unsigned
    testVectorShrImmediateForLane<uint64_t, int64_t>(SIMDLane::i64x2, SIMDSignMode::Unsigned, 1, 0x0123456789ABCDEFull);
    testVectorShrImmediateForLane<uint64_t, int64_t>(SIMDLane::i64x2, SIMDSignMode::Unsigned, 32, 0xFFFFFFFF00000000ull);
    testVectorShrImmediateForLane<uint64_t, int64_t>(SIMDLane::i64x2, SIMDSignMode::Unsigned, 63, 0x8000000000000000ull);

    // i64x2 signed
    testVectorShrImmediateForLane<uint64_t, int64_t>(SIMDLane::i64x2, SIMDSignMode::Signed, 1, 0x8000000000000000ull);
    testVectorShrImmediateForLane<uint64_t, int64_t>(SIMDLane::i64x2, SIMDSignMode::Signed, 32, 0xFFFFFFFF00000000ull);
    testVectorShrImmediateForLane<uint64_t, int64_t>(SIMDLane::i64x2, SIMDSignMode::Signed, 63, 0x8000000000000000ull);
}
// Helper: build a 3-child (binary) VectorSwizzle with the given byte pattern, verify result.
static void testBinarySwizzlePattern(const char*, const uint8_t pattern[16], v128_t inputA, v128_t inputB, v128_t expected)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    auto arguments = cCallArgumentValues<void*>(proc, root);
    Value* address = arguments[0];
    Value* a = root->appendNew<MemoryValue>(proc, Load, V128, Origin(), address);
    Value* b = root->appendNew<MemoryValue>(proc, Load, V128, Origin(), address, static_cast<int32_t>(sizeof(v128_t)));

    v128_t pat;
    for (unsigned i = 0; i < 16; ++i)
        pat.u8x16[i] = pattern[i];
    Value* patternConst = root->appendNew<Const128Value>(proc, Origin(), pat);
    Value* result = root->appendNew<SIMDValue>(proc, Origin(), VectorSwizzle, B3::V128, SIMDLane::i8x16, SIMDSignMode::None, a, b, patternConst);

    root->appendNew<MemoryValue>(proc, Store, Origin(), result, address, static_cast<int32_t>(2 * sizeof(v128_t)));
    root->appendNewControlValue(proc, Return, Origin());

    auto code = compileProc(proc);

    alignas(16) v128_t vectors[3];
    vectors[0] = inputA;
    vectors[1] = inputB;
    invoke<void>(*code, vectors);
    CHECK(bitEquals(vectors[2], expected));
}

// Helper: build a 2-child (unary) VectorSwizzle with the given byte pattern, verify result.
static void testUnarySwizzlePattern(const char*, const uint8_t pattern[16], v128_t input, v128_t expected)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    auto arguments = cCallArgumentValues<void*>(proc, root);
    Value* address = arguments[0];
    Value* inputVal = root->appendNew<MemoryValue>(proc, Load, V128, Origin(), address);

    v128_t pat;
    for (unsigned i = 0; i < 16; ++i)
        pat.u8x16[i] = pattern[i];
    Value* patternConst = root->appendNew<Const128Value>(proc, Origin(), pat);
    Value* result = root->appendNew<SIMDValue>(proc, Origin(), VectorSwizzle, B3::V128, SIMDLane::i8x16, SIMDSignMode::None, inputVal, patternConst);

    root->appendNew<MemoryValue>(proc, Store, Origin(), result, address, static_cast<int32_t>(sizeof(v128_t)));
    root->appendNewControlValue(proc, Return, Origin());

    auto code = compileProc(proc);

    alignas(16) v128_t vectors[2];
    vectors[0] = input;
    invoke<void>(*code, vectors);
    CHECK(bitEquals(vectors[1], expected));
}

// Test strength reduction of unary shuffle → VectorUnzipEven through VectorSwizzle.
// Pattern: VectorSwizzle(v, {0,1,2,3,8,9,10,11, 0,1,2,3,8,9,10,11}) → VectorUnzipEven(v, v)
void testVectorSwizzleToUnzipEven()
{
    alignas(16) v128_t vectors[2];
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    auto arguments = cCallArgumentValues<void*>(proc, root);
    Value* address = arguments[0];
    Value* input = root->appendNew<MemoryValue>(proc, Load, V128, Origin(), address);

    // UZP1.4S pattern as unary shuffle: extract even 32-bit elements, duplicated
    v128_t pattern;
    pattern.u8x16[0] = 0; pattern.u8x16[1] = 1; pattern.u8x16[2] = 2; pattern.u8x16[3] = 3;
    pattern.u8x16[4] = 8; pattern.u8x16[5] = 9; pattern.u8x16[6] = 10; pattern.u8x16[7] = 11;
    pattern.u8x16[8] = 0; pattern.u8x16[9] = 1; pattern.u8x16[10] = 2; pattern.u8x16[11] = 3;
    pattern.u8x16[12] = 8; pattern.u8x16[13] = 9; pattern.u8x16[14] = 10; pattern.u8x16[15] = 11;
    Value* patternConst = root->appendNew<Const128Value>(proc, Origin(), pattern);
    Value* result = root->appendNew<SIMDValue>(proc, Origin(), VectorSwizzle, B3::V128, SIMDLane::i8x16, SIMDSignMode::None, input, patternConst);

    root->appendNew<MemoryValue>(proc, Store, Origin(), result, address, static_cast<int32_t>(sizeof(v128_t)));
    root->appendNewControlValue(proc, Return, Origin());

    auto code = compileProc(proc);

    vectors[0].u32x4[0] = 0xAA; vectors[0].u32x4[1] = 0xBB; vectors[0].u32x4[2] = 0xCC; vectors[0].u32x4[3] = 0xDD;
    invoke<void>(*code, vectors);
    // UZP1.4S(v, v) = {v[0], v[2], v[0], v[2]}
    CHECK(vectors[1].u32x4[0] == 0xAA);
    CHECK(vectors[1].u32x4[1] == 0xCC);
    CHECK(vectors[1].u32x4[2] == 0xAA);
    CHECK(vectors[1].u32x4[3] == 0xCC);
}

// Test strength reduction of binary shuffle → VectorUnzipOdd through 3-child VectorSwizzle.
// Pattern: VectorSwizzle(a, b, {8..15, 24..31}) → VectorUnzipOdd.2D (UZP2)
void testVectorSwizzleBinaryToUnzipOdd()
{
    alignas(16) v128_t vectors[3];
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    auto arguments = cCallArgumentValues<void*>(proc, root);
    Value* address = arguments[0];
    Value* a = root->appendNew<MemoryValue>(proc, Load, V128, Origin(), address);
    Value* b = root->appendNew<MemoryValue>(proc, Load, V128, Origin(), address, static_cast<int32_t>(sizeof(v128_t)));

    // UZP2.2D pattern: {a[hi64], b[hi64]}
    v128_t pattern;
    for (unsigned i = 0; i < 8; ++i) pattern.u8x16[i] = 8 + i;
    for (unsigned i = 0; i < 8; ++i) pattern.u8x16[8 + i] = 24 + i;
    Value* patternConst = root->appendNew<Const128Value>(proc, Origin(), pattern);
    Value* result = root->appendNew<SIMDValue>(proc, Origin(), VectorSwizzle, B3::V128, SIMDLane::i8x16, SIMDSignMode::None, a, b, patternConst);

    root->appendNew<MemoryValue>(proc, Store, Origin(), result, address, static_cast<int32_t>(2 * sizeof(v128_t)));
    root->appendNewControlValue(proc, Return, Origin());

    auto code = compileProc(proc);

    vectors[0].u64x2[0] = 0x1111111111111111ULL;
    vectors[0].u64x2[1] = 0x2222222222222222ULL;
    vectors[1].u64x2[0] = 0x3333333333333333ULL;
    vectors[1].u64x2[1] = 0x4444444444444444ULL;
    invoke<void>(*code, vectors);
    // {a[hi64], b[hi64]}
    CHECK(vectors[2].u64x2[0] == 0x2222222222222222ULL);
    CHECK(vectors[2].u64x2[1] == 0x4444444444444444ULL);
}

// Test VectorExtractPair B3 opcode directly.
void testVectorExtractPair()
{
    // EXT #4: extract 4 bytes from concatenation
    {
        alignas(16) v128_t vectors[3];
        Procedure proc;
        BasicBlock* root = proc.addBlock();
        auto arguments = cCallArgumentValues<void*>(proc, root);
        Value* address = arguments[0];
        Value* a = root->appendNew<MemoryValue>(proc, Load, V128, Origin(), address);
        Value* b = root->appendNew<MemoryValue>(proc, Load, V128, Origin(), address, static_cast<int32_t>(sizeof(v128_t)));
        Value* result = root->appendNew<SIMDValue>(proc, Origin(), VectorExtractPair, B3::V128, SIMDLane::i8x16, SIMDSignMode::None, static_cast<uint8_t>(4), a, b);
        root->appendNew<MemoryValue>(proc, Store, Origin(), result, address, static_cast<int32_t>(2 * sizeof(v128_t)));
        root->appendNewControlValue(proc, Return, Origin());

        auto code = compileProc(proc);
        for (unsigned i = 0; i < 16; ++i) vectors[0].u8x16[i] = i;
        for (unsigned i = 0; i < 16; ++i) vectors[1].u8x16[i] = 16 + i;
        invoke<void>(*code, vectors);
        // EXT #4: {a[4..15], b[0..3]}
        for (unsigned i = 0; i < 12; ++i)
            CHECK(vectors[2].u8x16[i] == 4 + i);
        for (unsigned i = 0; i < 4; ++i)
            CHECK(vectors[2].u8x16[12 + i] == 16 + i);
    }

    // EXT #8: extract 8 bytes (common S64x2 swap-halves pattern)
    {
        alignas(16) v128_t vectors[3];
        Procedure proc;
        BasicBlock* root = proc.addBlock();
        auto arguments = cCallArgumentValues<void*>(proc, root);
        Value* address = arguments[0];
        Value* a = root->appendNew<MemoryValue>(proc, Load, V128, Origin(), address);
        Value* b = root->appendNew<MemoryValue>(proc, Load, V128, Origin(), address, static_cast<int32_t>(sizeof(v128_t)));
        Value* result = root->appendNew<SIMDValue>(proc, Origin(), VectorExtractPair, B3::V128, SIMDLane::i8x16, SIMDSignMode::None, static_cast<uint8_t>(8), a, b);
        root->appendNew<MemoryValue>(proc, Store, Origin(), result, address, static_cast<int32_t>(2 * sizeof(v128_t)));
        root->appendNewControlValue(proc, Return, Origin());

        auto code = compileProc(proc);
        for (unsigned i = 0; i < 16; ++i) vectors[0].u8x16[i] = i;
        for (unsigned i = 0; i < 16; ++i) vectors[1].u8x16[i] = 16 + i;
        invoke<void>(*code, vectors);
        // EXT #8: {a[8..15], b[0..7]}
        for (unsigned i = 0; i < 8; ++i)
            CHECK(vectors[2].u8x16[i] == 8 + i);
        for (unsigned i = 0; i < 8; ++i)
            CHECK(vectors[2].u8x16[8 + i] == 16 + i);
    }
}

// Test strength reduction of binary shuffle → VectorExtractPair through VectorSwizzle.
void testVectorSwizzleBinaryToEXT()
{
    v128_t a, b;
    for (unsigned i = 0; i < 16; ++i) a.u8x16[i] = i;
    for (unsigned i = 0; i < 16; ++i) b.u8x16[i] = 16 + i;

    // EXT #4: {a[4..15], b[0..3]} = byte indices {4,5,...,15, 16,17,18,19}
    {
        const uint8_t pat[] = { 4,5,6,7, 8,9,10,11, 12,13,14,15, 16,17,18,19 };
        v128_t exp;
        for (unsigned i = 0; i < 12; ++i) exp.u8x16[i] = 4 + i;
        for (unsigned i = 0; i < 4; ++i) exp.u8x16[12 + i] = 16 + i;
        testBinarySwizzlePattern("EXT #4", pat, a, b, exp);
    }

    // EXT #8: {a[8..15], b[0..7]}
    {
        const uint8_t pat[] = { 8,9,10,11,12,13,14,15, 16,17,18,19,20,21,22,23 };
        v128_t exp;
        for (unsigned i = 0; i < 8; ++i) exp.u8x16[i] = 8 + i;
        for (unsigned i = 0; i < 8; ++i) exp.u8x16[8 + i] = 16 + i;
        testBinarySwizzlePattern("EXT #8", pat, a, b, exp);
    }

    // EXT #1: {a[1..15], b[0]}
    {
        const uint8_t pat[] = { 1,2,3,4,5,6,7,8,9,10,11,12,13,14,15, 16 };
        v128_t exp;
        for (unsigned i = 0; i < 15; ++i) exp.u8x16[i] = 1 + i;
        exp.u8x16[15] = 16;
        testBinarySwizzlePattern("EXT #1", pat, a, b, exp);
    }

    // EXT #15: {a[15], b[0..14]}
    {
        const uint8_t pat[] = { 15, 16,17,18,19,20,21,22,23,24,25,26,27,28,29,30 };
        v128_t exp;
        exp.u8x16[0] = 15;
        for (unsigned i = 0; i < 15; ++i) exp.u8x16[1 + i] = 16 + i;
        testBinarySwizzlePattern("EXT #15", pat, a, b, exp);
    }

    // Swapped EXT: indices from child1 first, then child0 = EXT with swapped operands
    // Pattern {16..23, 0..7} = EXT #0 with swapped operands... actually that's
    // {b[0..7], a[0..7]} which is UZP1.2D(b, a), not EXT.
    // Let's test {20,21,...,31, 0,1,...,3} = swapped EXT #4
    {
        const uint8_t pat[] = { 20,21,22,23,24,25,26,27,28,29,30,31, 0,1,2,3 };
        v128_t exp;
        for (unsigned i = 0; i < 12; ++i) exp.u8x16[i] = 20 + i;
        for (unsigned i = 0; i < 4; ++i) exp.u8x16[12 + i] = i;
        testBinarySwizzlePattern("Swapped EXT #4", pat, a, b, exp);
    }
}

// Test strength reduction of unary shuffle → VectorExtractPair (S64x2 swap halves).
void testVectorSwizzleUnaryToEXT()
{
    v128_t v;
    for (unsigned i = 0; i < 16; ++i) v.u8x16[i] = i;

    // Test all 15 rotation offsets (EXT #1 through #15).
    for (unsigned offset = 1; offset <= 15; ++offset) {
        uint8_t pat[16];
        v128_t exp;
        for (unsigned i = 0; i < 16; ++i) {
            pat[i] = (offset + i) % 16;
            exp.u8x16[i] = (offset + i) % 16;
        }
        testUnarySwizzlePattern("Unary EXT", pat, v, exp);
    }
}

// Test all canonical binary shuffle patterns through VectorSwizzle strength reduction.
void testVectorSwizzleBinaryCanonical()
{
    // Input vectors: a = {0x11, 0x22, 0x33, 0x44} as i32x4, b = {0x55, 0x66, 0x77, 0x88}
    v128_t a, b;
    a.u32x4[0] = 0x11; a.u32x4[1] = 0x22; a.u32x4[2] = 0x33; a.u32x4[3] = 0x44;
    b.u32x4[0] = 0x55; b.u32x4[1] = 0x66; b.u32x4[2] = 0x77; b.u32x4[3] = 0x88;

    // UZP1.4S: {a[0], a[2], b[0], b[2]}
    {
        const uint8_t pat[] = { 0,1,2,3, 8,9,10,11, 16,17,18,19, 24,25,26,27 };
        v128_t exp; exp.u32x4[0] = 0x11; exp.u32x4[1] = 0x33; exp.u32x4[2] = 0x55; exp.u32x4[3] = 0x77;
        testBinarySwizzlePattern("UZP1.4S", pat, a, b, exp);
    }
    // UZP2.4S: {a[1], a[3], b[1], b[3]}
    {
        const uint8_t pat[] = { 4,5,6,7, 12,13,14,15, 20,21,22,23, 28,29,30,31 };
        v128_t exp; exp.u32x4[0] = 0x22; exp.u32x4[1] = 0x44; exp.u32x4[2] = 0x66; exp.u32x4[3] = 0x88;
        testBinarySwizzlePattern("UZP2.4S", pat, a, b, exp);
    }
    // ZIP1.4S: {a[0], b[0], a[1], b[1]}
    {
        const uint8_t pat[] = { 0,1,2,3, 16,17,18,19, 4,5,6,7, 20,21,22,23 };
        v128_t exp; exp.u32x4[0] = 0x11; exp.u32x4[1] = 0x55; exp.u32x4[2] = 0x22; exp.u32x4[3] = 0x66;
        testBinarySwizzlePattern("ZIP1.4S", pat, a, b, exp);
    }
    // ZIP2.4S: {a[2], b[2], a[3], b[3]}
    {
        const uint8_t pat[] = { 8,9,10,11, 24,25,26,27, 12,13,14,15, 28,29,30,31 };
        v128_t exp; exp.u32x4[0] = 0x33; exp.u32x4[1] = 0x77; exp.u32x4[2] = 0x44; exp.u32x4[3] = 0x88;
        testBinarySwizzlePattern("ZIP2.4S", pat, a, b, exp);
    }
    // TRN1.4S: {a[0], b[0], a[2], b[2]}
    {
        const uint8_t pat[] = { 0,1,2,3, 16,17,18,19, 8,9,10,11, 24,25,26,27 };
        v128_t exp; exp.u32x4[0] = 0x11; exp.u32x4[1] = 0x55; exp.u32x4[2] = 0x33; exp.u32x4[3] = 0x77;
        testBinarySwizzlePattern("TRN1.4S", pat, a, b, exp);
    }
    // TRN2.4S: {a[1], b[1], a[3], b[3]}
    {
        const uint8_t pat[] = { 4,5,6,7, 20,21,22,23, 12,13,14,15, 28,29,30,31 };
        v128_t exp; exp.u32x4[0] = 0x22; exp.u32x4[1] = 0x66; exp.u32x4[2] = 0x44; exp.u32x4[3] = 0x88;
        testBinarySwizzlePattern("TRN2.4S", pat, a, b, exp);
    }
    // UZP1.2D: {a[lo64], b[lo64]}
    {
        const uint8_t pat[] = { 0,1,2,3,4,5,6,7, 16,17,18,19,20,21,22,23 };
        v128_t exp; exp.u32x4[0] = 0x11; exp.u32x4[1] = 0x22; exp.u32x4[2] = 0x55; exp.u32x4[3] = 0x66;
        testBinarySwizzlePattern("UZP1.2D", pat, a, b, exp);
    }
    // UZP2.2D: {a[hi64], b[hi64]}
    {
        const uint8_t pat[] = { 8,9,10,11,12,13,14,15, 24,25,26,27,28,29,30,31 };
        v128_t exp; exp.u32x4[0] = 0x33; exp.u32x4[1] = 0x44; exp.u32x4[2] = 0x77; exp.u32x4[3] = 0x88;
        testBinarySwizzlePattern("UZP2.2D", pat, a, b, exp);
    }

    // 16-bit (8H) patterns: a16 has 8 distinct u16 values, b16 has 8 more
    v128_t a16, b16;
    a16.u16x8[0] = 0x11; a16.u16x8[1] = 0x22; a16.u16x8[2] = 0x33; a16.u16x8[3] = 0x44;
    a16.u16x8[4] = 0x55; a16.u16x8[5] = 0x66; a16.u16x8[6] = 0x77; a16.u16x8[7] = 0x88;
    b16.u16x8[0] = 0x99; b16.u16x8[1] = 0xAA; b16.u16x8[2] = 0xBB; b16.u16x8[3] = 0xCC;
    b16.u16x8[4] = 0xDD; b16.u16x8[5] = 0xEE; b16.u16x8[6] = 0xFF; b16.u16x8[7] = 0x100;

    // UZP1.8H: {a[0],a[2],a[4],a[6], b[0],b[2],b[4],b[6]}
    {
        const uint8_t pat[] = { 0,1, 4,5, 8,9, 12,13, 16,17, 20,21, 24,25, 28,29 };
        v128_t exp;
        exp.u16x8[0] = 0x11; exp.u16x8[1] = 0x33; exp.u16x8[2] = 0x55; exp.u16x8[3] = 0x77;
        exp.u16x8[4] = 0x99; exp.u16x8[5] = 0xBB; exp.u16x8[6] = 0xDD; exp.u16x8[7] = 0xFF;
        testBinarySwizzlePattern("UZP1.8H", pat, a16, b16, exp);
    }
    // UZP2.8H: {a[1],a[3],a[5],a[7], b[1],b[3],b[5],b[7]}
    {
        const uint8_t pat[] = { 2,3, 6,7, 10,11, 14,15, 18,19, 22,23, 26,27, 30,31 };
        v128_t exp;
        exp.u16x8[0] = 0x22; exp.u16x8[1] = 0x44; exp.u16x8[2] = 0x66; exp.u16x8[3] = 0x88;
        exp.u16x8[4] = 0xAA; exp.u16x8[5] = 0xCC; exp.u16x8[6] = 0xEE; exp.u16x8[7] = 0x100;
        testBinarySwizzlePattern("UZP2.8H", pat, a16, b16, exp);
    }
    // ZIP1.8H: {a[0],b[0],a[1],b[1],a[2],b[2],a[3],b[3]}
    {
        const uint8_t pat[] = { 0,1, 16,17, 2,3, 18,19, 4,5, 20,21, 6,7, 22,23 };
        v128_t exp;
        exp.u16x8[0] = 0x11; exp.u16x8[1] = 0x99; exp.u16x8[2] = 0x22; exp.u16x8[3] = 0xAA;
        exp.u16x8[4] = 0x33; exp.u16x8[5] = 0xBB; exp.u16x8[6] = 0x44; exp.u16x8[7] = 0xCC;
        testBinarySwizzlePattern("ZIP1.8H", pat, a16, b16, exp);
    }
    // ZIP2.8H: {a[4],b[4],a[5],b[5],a[6],b[6],a[7],b[7]}
    {
        const uint8_t pat[] = { 8,9, 24,25, 10,11, 26,27, 12,13, 28,29, 14,15, 30,31 };
        v128_t exp;
        exp.u16x8[0] = 0x55; exp.u16x8[1] = 0xDD; exp.u16x8[2] = 0x66; exp.u16x8[3] = 0xEE;
        exp.u16x8[4] = 0x77; exp.u16x8[5] = 0xFF; exp.u16x8[6] = 0x88; exp.u16x8[7] = 0x100;
        testBinarySwizzlePattern("ZIP2.8H", pat, a16, b16, exp);
    }
    // TRN1.8H: {a[0],b[0],a[2],b[2],a[4],b[4],a[6],b[6]}
    {
        const uint8_t pat[] = { 0,1, 16,17, 4,5, 20,21, 8,9, 24,25, 12,13, 28,29 };
        v128_t exp;
        exp.u16x8[0] = 0x11; exp.u16x8[1] = 0x99; exp.u16x8[2] = 0x33; exp.u16x8[3] = 0xBB;
        exp.u16x8[4] = 0x55; exp.u16x8[5] = 0xDD; exp.u16x8[6] = 0x77; exp.u16x8[7] = 0xFF;
        testBinarySwizzlePattern("TRN1.8H", pat, a16, b16, exp);
    }
    // TRN2.8H: {a[1],b[1],a[3],b[3],a[5],b[5],a[7],b[7]}
    {
        const uint8_t pat[] = { 2,3, 18,19, 6,7, 22,23, 10,11, 26,27, 14,15, 30,31 };
        v128_t exp;
        exp.u16x8[0] = 0x22; exp.u16x8[1] = 0xAA; exp.u16x8[2] = 0x44; exp.u16x8[3] = 0xCC;
        exp.u16x8[4] = 0x66; exp.u16x8[5] = 0xEE; exp.u16x8[6] = 0x88; exp.u16x8[7] = 0x100;
        testBinarySwizzlePattern("TRN2.8H", pat, a16, b16, exp);
    }

    // 8-bit (16B) patterns: a8 has bytes 0..15, b8 has bytes 16..31
    v128_t a8, b8;
    for (unsigned i = 0; i < 16; ++i) a8.u8x16[i] = i;
    for (unsigned i = 0; i < 16; ++i) b8.u8x16[i] = 16 + i;

    // UZP1.16B: {a[0],a[2],a[4],...,a[14], b[0],b[2],b[4],...,b[14]}
    {
        const uint8_t pat[] = { 0,2,4,6,8,10,12,14, 16,18,20,22,24,26,28,30 };
        v128_t exp;
        for (unsigned i = 0; i < 8; ++i) exp.u8x16[i] = i * 2;
        for (unsigned i = 0; i < 8; ++i) exp.u8x16[8 + i] = 16 + i * 2;
        testBinarySwizzlePattern("UZP1.16B", pat, a8, b8, exp);
    }
    // UZP2.16B: {a[1],a[3],a[5],...,a[15], b[1],b[3],b[5],...,b[15]}
    {
        const uint8_t pat[] = { 1,3,5,7,9,11,13,15, 17,19,21,23,25,27,29,31 };
        v128_t exp;
        for (unsigned i = 0; i < 8; ++i) exp.u8x16[i] = 1 + i * 2;
        for (unsigned i = 0; i < 8; ++i) exp.u8x16[8 + i] = 17 + i * 2;
        testBinarySwizzlePattern("UZP2.16B", pat, a8, b8, exp);
    }
    // ZIP1.16B: {a[0],b[0],a[1],b[1],a[2],b[2],a[3],b[3],a[4],b[4],a[5],b[5],a[6],b[6],a[7],b[7]}
    {
        const uint8_t pat[] = { 0,16, 1,17, 2,18, 3,19, 4,20, 5,21, 6,22, 7,23 };
        v128_t exp;
        for (unsigned i = 0; i < 8; ++i) { exp.u8x16[i * 2] = i; exp.u8x16[i * 2 + 1] = 16 + i; }
        testBinarySwizzlePattern("ZIP1.16B", pat, a8, b8, exp);
    }
    // ZIP2.16B: {a[8],b[8],a[9],b[9],...,a[15],b[15]}
    {
        const uint8_t pat[] = { 8,24, 9,25, 10,26, 11,27, 12,28, 13,29, 14,30, 15,31 };
        v128_t exp;
        for (unsigned i = 0; i < 8; ++i) { exp.u8x16[i * 2] = 8 + i; exp.u8x16[i * 2 + 1] = 24 + i; }
        testBinarySwizzlePattern("ZIP2.16B", pat, a8, b8, exp);
    }
    // TRN1.16B: {a[0],b[0],a[2],b[2],a[4],b[4],...,a[14],b[14]}
    {
        const uint8_t pat[] = { 0,16, 2,18, 4,20, 6,22, 8,24, 10,26, 12,28, 14,30 };
        v128_t exp;
        for (unsigned i = 0; i < 8; ++i) { exp.u8x16[i * 2] = i * 2; exp.u8x16[i * 2 + 1] = 16 + i * 2; }
        testBinarySwizzlePattern("TRN1.16B", pat, a8, b8, exp);
    }
    // TRN2.16B: {a[1],b[1],a[3],b[3],a[5],b[5],...,a[15],b[15]}
    {
        const uint8_t pat[] = { 1,17, 3,19, 5,21, 7,23, 9,25, 11,27, 13,29, 15,31 };
        v128_t exp;
        for (unsigned i = 0; i < 8; ++i) { exp.u8x16[i * 2] = 1 + i * 2; exp.u8x16[i * 2 + 1] = 17 + i * 2; }
        testBinarySwizzlePattern("TRN2.16B", pat, a8, b8, exp);
    }
}

// Test all canonical unary shuffle patterns through VectorSwizzle strength reduction.
void testVectorSwizzleUnaryCanonical()
{
    v128_t v;
    v.u32x4[0] = 0xAA; v.u32x4[1] = 0xBB; v.u32x4[2] = 0xCC; v.u32x4[3] = 0xDD;

    // UZP1.4S(v,v): {v[0], v[2], v[0], v[2]}
    {
        const uint8_t pat[] = { 0,1,2,3, 8,9,10,11, 0,1,2,3, 8,9,10,11 };
        v128_t exp; exp.u32x4[0] = 0xAA; exp.u32x4[1] = 0xCC; exp.u32x4[2] = 0xAA; exp.u32x4[3] = 0xCC;
        testUnarySwizzlePattern("unary UZP1.4S", pat, v, exp);
    }
    // UZP2.4S(v,v): {v[1], v[3], v[1], v[3]}
    {
        const uint8_t pat[] = { 4,5,6,7, 12,13,14,15, 4,5,6,7, 12,13,14,15 };
        v128_t exp; exp.u32x4[0] = 0xBB; exp.u32x4[1] = 0xDD; exp.u32x4[2] = 0xBB; exp.u32x4[3] = 0xDD;
        testUnarySwizzlePattern("unary UZP2.4S", pat, v, exp);
    }
    // ZIP1.4S(v,v): {v[0], v[0], v[1], v[1]}
    {
        const uint8_t pat[] = { 0,1,2,3, 0,1,2,3, 4,5,6,7, 4,5,6,7 };
        v128_t exp; exp.u32x4[0] = 0xAA; exp.u32x4[1] = 0xAA; exp.u32x4[2] = 0xBB; exp.u32x4[3] = 0xBB;
        testUnarySwizzlePattern("unary ZIP1.4S", pat, v, exp);
    }
    // ZIP2.4S(v,v): {v[2], v[2], v[3], v[3]}
    {
        const uint8_t pat[] = { 8,9,10,11, 8,9,10,11, 12,13,14,15, 12,13,14,15 };
        v128_t exp; exp.u32x4[0] = 0xCC; exp.u32x4[1] = 0xCC; exp.u32x4[2] = 0xDD; exp.u32x4[3] = 0xDD;
        testUnarySwizzlePattern("unary ZIP2.4S", pat, v, exp);
    }
    // TRN1.4S(v,v): {v[0], v[0], v[2], v[2]}
    {
        const uint8_t pat[] = { 0,1,2,3, 0,1,2,3, 8,9,10,11, 8,9,10,11 };
        v128_t exp; exp.u32x4[0] = 0xAA; exp.u32x4[1] = 0xAA; exp.u32x4[2] = 0xCC; exp.u32x4[3] = 0xCC;
        testUnarySwizzlePattern("unary TRN1.4S", pat, v, exp);
    }
    // TRN2.4S(v,v): {v[1], v[1], v[3], v[3]}
    {
        const uint8_t pat[] = { 4,5,6,7, 4,5,6,7, 12,13,14,15, 12,13,14,15 };
        v128_t exp; exp.u32x4[0] = 0xBB; exp.u32x4[1] = 0xBB; exp.u32x4[2] = 0xDD; exp.u32x4[3] = 0xDD;
        testUnarySwizzlePattern("unary TRN2.4S", pat, v, exp);
    }
    // REV64.4S: {v[1], v[0], v[3], v[2]}
    {
        const uint8_t pat[] = { 4,5,6,7, 0,1,2,3, 12,13,14,15, 8,9,10,11 };
        v128_t exp; exp.u32x4[0] = 0xBB; exp.u32x4[1] = 0xAA; exp.u32x4[2] = 0xDD; exp.u32x4[3] = 0xCC;
        testUnarySwizzlePattern("unary REV64.4S", pat, v, exp);
    }

    // Test 16-bit patterns
    v128_t v16;
    v16.u16x8[0] = 0x11; v16.u16x8[1] = 0x22; v16.u16x8[2] = 0x33; v16.u16x8[3] = 0x44;
    v16.u16x8[4] = 0x55; v16.u16x8[5] = 0x66; v16.u16x8[6] = 0x77; v16.u16x8[7] = 0x88;

    // REV32.8H: swap 16-bit elements within 32-bit groups {v[1],v[0],v[3],v[2],v[5],v[4],v[7],v[6]}
    {
        const uint8_t pat[] = { 2,3,0,1, 6,7,4,5, 10,11,8,9, 14,15,12,13 };
        v128_t exp;
        exp.u16x8[0] = 0x22; exp.u16x8[1] = 0x11; exp.u16x8[2] = 0x44; exp.u16x8[3] = 0x33;
        exp.u16x8[4] = 0x66; exp.u16x8[5] = 0x55; exp.u16x8[6] = 0x88; exp.u16x8[7] = 0x77;
        testUnarySwizzlePattern("unary REV32.8H", pat, v16, exp);
    }

    // Test 8-bit REV16: swap bytes within 16-bit groups
    v128_t v8;
    for (unsigned i = 0; i < 16; ++i)
        v8.u8x16[i] = i;
    {
        const uint8_t pat[] = { 1,0, 3,2, 5,4, 7,6, 9,8, 11,10, 13,12, 15,14 };
        v128_t exp;
        for (unsigned i = 0; i < 16; i += 2) { exp.u8x16[i] = i + 1; exp.u8x16[i + 1] = i; }
        testUnarySwizzlePattern("unary REV16.16B", pat, v8, exp);
    }

    // UZP1.2D(v,v): {lo64, lo64}
    {
        const uint8_t pat[] = { 0,1,2,3,4,5,6,7, 0,1,2,3,4,5,6,7 };
        v128_t exp;
        exp.u32x4[0] = 0xAA; exp.u32x4[1] = 0xBB; exp.u32x4[2] = 0xAA; exp.u32x4[3] = 0xBB;
        testUnarySwizzlePattern("unary UZP1.2D", pat, v, exp);
    }
    // UZP2.2D(v,v): {hi64, hi64}
    {
        const uint8_t pat[] = { 8,9,10,11,12,13,14,15, 8,9,10,11,12,13,14,15 };
        v128_t exp;
        exp.u32x4[0] = 0xCC; exp.u32x4[1] = 0xDD; exp.u32x4[2] = 0xCC; exp.u32x4[3] = 0xDD;
        testUnarySwizzlePattern("unary UZP2.2D", pat, v, exp);
    }

    // UZP1.8H(v,v): {v[0],v[2],v[4],v[6], v[0],v[2],v[4],v[6]}
    {
        const uint8_t pat[] = { 0,1, 4,5, 8,9, 12,13, 0,1, 4,5, 8,9, 12,13 };
        v128_t exp;
        exp.u16x8[0] = 0x11; exp.u16x8[1] = 0x33; exp.u16x8[2] = 0x55; exp.u16x8[3] = 0x77;
        exp.u16x8[4] = 0x11; exp.u16x8[5] = 0x33; exp.u16x8[6] = 0x55; exp.u16x8[7] = 0x77;
        testUnarySwizzlePattern("unary UZP1.8H", pat, v16, exp);
    }
    // UZP2.8H(v,v): {v[1],v[3],v[5],v[7], v[1],v[3],v[5],v[7]}
    {
        const uint8_t pat[] = { 2,3, 6,7, 10,11, 14,15, 2,3, 6,7, 10,11, 14,15 };
        v128_t exp;
        exp.u16x8[0] = 0x22; exp.u16x8[1] = 0x44; exp.u16x8[2] = 0x66; exp.u16x8[3] = 0x88;
        exp.u16x8[4] = 0x22; exp.u16x8[5] = 0x44; exp.u16x8[6] = 0x66; exp.u16x8[7] = 0x88;
        testUnarySwizzlePattern("unary UZP2.8H", pat, v16, exp);
    }
    // ZIP1.8H(v,v): {v[0],v[0],v[1],v[1],v[2],v[2],v[3],v[3]}
    {
        const uint8_t pat[] = { 0,1, 0,1, 2,3, 2,3, 4,5, 4,5, 6,7, 6,7 };
        v128_t exp;
        exp.u16x8[0] = 0x11; exp.u16x8[1] = 0x11; exp.u16x8[2] = 0x22; exp.u16x8[3] = 0x22;
        exp.u16x8[4] = 0x33; exp.u16x8[5] = 0x33; exp.u16x8[6] = 0x44; exp.u16x8[7] = 0x44;
        testUnarySwizzlePattern("unary ZIP1.8H", pat, v16, exp);
    }
    // ZIP2.8H(v,v): {v[4],v[4],v[5],v[5],v[6],v[6],v[7],v[7]}
    {
        const uint8_t pat[] = { 8,9, 8,9, 10,11, 10,11, 12,13, 12,13, 14,15, 14,15 };
        v128_t exp;
        exp.u16x8[0] = 0x55; exp.u16x8[1] = 0x55; exp.u16x8[2] = 0x66; exp.u16x8[3] = 0x66;
        exp.u16x8[4] = 0x77; exp.u16x8[5] = 0x77; exp.u16x8[6] = 0x88; exp.u16x8[7] = 0x88;
        testUnarySwizzlePattern("unary ZIP2.8H", pat, v16, exp);
    }
    // TRN1.8H(v,v): {v[0],v[0],v[2],v[2],v[4],v[4],v[6],v[6]}
    {
        const uint8_t pat[] = { 0,1, 0,1, 4,5, 4,5, 8,9, 8,9, 12,13, 12,13 };
        v128_t exp;
        exp.u16x8[0] = 0x11; exp.u16x8[1] = 0x11; exp.u16x8[2] = 0x33; exp.u16x8[3] = 0x33;
        exp.u16x8[4] = 0x55; exp.u16x8[5] = 0x55; exp.u16x8[6] = 0x77; exp.u16x8[7] = 0x77;
        testUnarySwizzlePattern("unary TRN1.8H", pat, v16, exp);
    }
    // TRN2.8H(v,v): {v[1],v[1],v[3],v[3],v[5],v[5],v[7],v[7]}
    {
        const uint8_t pat[] = { 2,3, 2,3, 6,7, 6,7, 10,11, 10,11, 14,15, 14,15 };
        v128_t exp;
        exp.u16x8[0] = 0x22; exp.u16x8[1] = 0x22; exp.u16x8[2] = 0x44; exp.u16x8[3] = 0x44;
        exp.u16x8[4] = 0x66; exp.u16x8[5] = 0x66; exp.u16x8[6] = 0x88; exp.u16x8[7] = 0x88;
        testUnarySwizzlePattern("unary TRN2.8H", pat, v16, exp);
    }

    // UZP1.16B(v,v): {v[0],v[2],v[4],...,v[14], v[0],v[2],v[4],...,v[14]}
    {
        const uint8_t pat[] = { 0,2,4,6,8,10,12,14, 0,2,4,6,8,10,12,14 };
        v128_t exp;
        for (unsigned i = 0; i < 8; ++i) exp.u8x16[i] = i * 2;
        for (unsigned i = 0; i < 8; ++i) exp.u8x16[8 + i] = i * 2;
        testUnarySwizzlePattern("unary UZP1.16B", pat, v8, exp);
    }
    // UZP2.16B(v,v): {v[1],v[3],v[5],...,v[15], v[1],v[3],v[5],...,v[15]}
    {
        const uint8_t pat[] = { 1,3,5,7,9,11,13,15, 1,3,5,7,9,11,13,15 };
        v128_t exp;
        for (unsigned i = 0; i < 8; ++i) exp.u8x16[i] = 1 + i * 2;
        for (unsigned i = 0; i < 8; ++i) exp.u8x16[8 + i] = 1 + i * 2;
        testUnarySwizzlePattern("unary UZP2.16B", pat, v8, exp);
    }
    // ZIP1.16B(v,v): {v[0],v[0],v[1],v[1],...,v[7],v[7]}
    {
        const uint8_t pat[] = { 0,0, 1,1, 2,2, 3,3, 4,4, 5,5, 6,6, 7,7 };
        v128_t exp;
        for (unsigned i = 0; i < 8; ++i) { exp.u8x16[i * 2] = i; exp.u8x16[i * 2 + 1] = i; }
        testUnarySwizzlePattern("unary ZIP1.16B", pat, v8, exp);
    }
    // ZIP2.16B(v,v): {v[8],v[8],v[9],v[9],...,v[15],v[15]}
    {
        const uint8_t pat[] = { 8,8, 9,9, 10,10, 11,11, 12,12, 13,13, 14,14, 15,15 };
        v128_t exp;
        for (unsigned i = 0; i < 8; ++i) { exp.u8x16[i * 2] = 8 + i; exp.u8x16[i * 2 + 1] = 8 + i; }
        testUnarySwizzlePattern("unary ZIP2.16B", pat, v8, exp);
    }
    // TRN1.16B(v,v): {v[0],v[0],v[2],v[2],...,v[14],v[14]}
    {
        const uint8_t pat[] = { 0,0, 2,2, 4,4, 6,6, 8,8, 10,10, 12,12, 14,14 };
        v128_t exp;
        for (unsigned i = 0; i < 8; ++i) { exp.u8x16[i * 2] = i * 2; exp.u8x16[i * 2 + 1] = i * 2; }
        testUnarySwizzlePattern("unary TRN1.16B", pat, v8, exp);
    }
    // TRN2.16B(v,v): {v[1],v[1],v[3],v[3],...,v[15],v[15]}
    {
        const uint8_t pat[] = { 1,1, 3,3, 5,5, 7,7, 9,9, 11,11, 13,13, 15,15 };
        v128_t exp;
        for (unsigned i = 0; i < 8; ++i) { exp.u8x16[i * 2] = 1 + i * 2; exp.u8x16[i * 2 + 1] = 1 + i * 2; }
        testUnarySwizzlePattern("unary TRN2.16B", pat, v8, exp);
    }
}

// Test VectorDupElement folding: when canonical ops have identical inputs (v, v)
// and lane is i64x2, they should be folded to VectorDupElement.
void testVectorCanonicalSameInputFolding()
{
    // Helper: build canonical op with same input, verify result equals DUP behavior.
    auto testSameInputOp = [](B3::Opcode op, SIMDLane lane, uint64_t lo, uint64_t hi, uint64_t expectedLo, uint64_t expectedHi) {
        alignas(16) v128_t vectors[2];
        Procedure proc;
        BasicBlock* root = proc.addBlock();
        auto arguments = cCallArgumentValues<void*>(proc, root);
        Value* address = arguments[0];
        Value* v = root->appendNew<MemoryValue>(proc, Load, V128, Origin(), address);
        Value* result = root->appendNew<SIMDValue>(proc, Origin(), op, B3::V128, lane, SIMDSignMode::None, v, v);
        root->appendNew<MemoryValue>(proc, Store, Origin(), result, address, static_cast<int32_t>(sizeof(v128_t)));
        root->appendNewControlValue(proc, Return, Origin());

        auto code = compileProc(proc);
        vectors[0].u64x2[0] = lo;
        vectors[0].u64x2[1] = hi;
        invoke<void>(*code, vectors);
        CHECK(vectors[1].u64x2[0] == expectedLo);
        CHECK(vectors[1].u64x2[1] == expectedHi);
    };

    uint64_t lo = 0x1111111111111111ULL;
    uint64_t hi = 0x2222222222222222ULL;

    // All of these with i64x2 and same input should produce DUP behavior:
    // UZP1.2D(v,v) = {lo, lo}
    testSameInputOp(VectorUnzipEven, SIMDLane::i64x2, lo, hi, lo, lo);
    // UZP2.2D(v,v) = {hi, hi}
    testSameInputOp(VectorUnzipOdd, SIMDLane::i64x2, lo, hi, hi, hi);
    // ZIP1.2D(v,v) = {lo, lo}
    testSameInputOp(VectorZipLower, SIMDLane::i64x2, lo, hi, lo, lo);
    // ZIP2.2D(v,v) = {hi, hi}
    testSameInputOp(VectorZipHigher, SIMDLane::i64x2, lo, hi, hi, hi);
    if constexpr (isARM64()) {
        // TRN1.2D(v,v) = {lo, lo} — ARM64 only
        testSameInputOp(VectorTransposeEven, SIMDLane::i64x2, lo, hi, lo, lo);
        // TRN2.2D(v,v) = {hi, hi}
        testSameInputOp(VectorTransposeOdd, SIMDLane::i64x2, lo, hi, hi, hi);
    }
}

// Test that VectorSwizzle DupElement patterns (unary) are correctly detected.
// These test the path: VectorSwizzle → (ReduceStrength) → VectorDupElement
void testVectorSwizzleToDupElement()
{
    v128_t v;
    v.u32x4[0] = 0xAA; v.u32x4[1] = 0xBB; v.u32x4[2] = 0xCC; v.u32x4[3] = 0xDD;

    // i64x2 DUP element 0: {lo64, lo64}
    {
        const uint8_t pat[] = { 0,1,2,3,4,5,6,7, 0,1,2,3,4,5,6,7 };
        v128_t exp; exp.u32x4[0] = 0xAA; exp.u32x4[1] = 0xBB; exp.u32x4[2] = 0xAA; exp.u32x4[3] = 0xBB;
        testUnarySwizzlePattern("DUP.2D element 0", pat, v, exp);
    }
    // i64x2 DUP element 1: {hi64, hi64}
    {
        const uint8_t pat[] = { 8,9,10,11,12,13,14,15, 8,9,10,11,12,13,14,15 };
        v128_t exp; exp.u32x4[0] = 0xCC; exp.u32x4[1] = 0xDD; exp.u32x4[2] = 0xCC; exp.u32x4[3] = 0xDD;
        testUnarySwizzlePattern("DUP.2D element 1", pat, v, exp);
    }
    // i32x4 DUP element 0: {v[0], v[0], v[0], v[0]}
    {
        const uint8_t pat[] = { 0,1,2,3, 0,1,2,3, 0,1,2,3, 0,1,2,3 };
        v128_t exp; exp.u32x4[0] = 0xAA; exp.u32x4[1] = 0xAA; exp.u32x4[2] = 0xAA; exp.u32x4[3] = 0xAA;
        testUnarySwizzlePattern("DUP.4S element 0", pat, v, exp);
    }
    // i32x4 DUP element 2: {v[2], v[2], v[2], v[2]}
    {
        const uint8_t pat[] = { 8,9,10,11, 8,9,10,11, 8,9,10,11, 8,9,10,11 };
        v128_t exp; exp.u32x4[0] = 0xCC; exp.u32x4[1] = 0xCC; exp.u32x4[2] = 0xCC; exp.u32x4[3] = 0xCC;
        testUnarySwizzlePattern("DUP.4S element 2", pat, v, exp);
    }
    // i16x8 DUP element 3
    {
        v128_t v16;
        for (unsigned i = 0; i < 8; ++i)
            v16.u16x8[i] = 0x10 * (i + 1);
        const uint8_t pat[] = { 6,7, 6,7, 6,7, 6,7, 6,7, 6,7, 6,7, 6,7 };
        v128_t exp;
        for (unsigned i = 0; i < 8; ++i)
            exp.u16x8[i] = 0x40;
        testUnarySwizzlePattern("DUP.8H element 3", pat, v16, exp);
    }
    // i8x16 DUP element 5
    {
        v128_t v8;
        for (unsigned i = 0; i < 16; ++i)
            v8.u8x16[i] = i + 1;
        const uint8_t pat[] = { 5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5 };
        v128_t exp;
        for (unsigned i = 0; i < 16; ++i)
            exp.u8x16[i] = 6;
        testUnarySwizzlePattern("DUP.16B element 5", pat, v8, exp);
    }
}

// Test shuffle-of-shuffle composition (ReduceSIMDShuffle).
// An inner unary shuffle fed into an outer binary shuffle should be composed
// into a single shuffle.
void testVectorSwizzleComposition()
{
    // Inner: reverse bytes within 32-bit elements (unary pattern)
    // Outer: take high 64 bits of inner result and low 64 bits of other
    // Composed: should be a single binary shuffle

    alignas(16) v128_t vectors[3];
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    auto arguments = cCallArgumentValues<void*>(proc, root);
    Value* address = arguments[0];
    Value* a = root->appendNew<MemoryValue>(proc, Load, V128, Origin(), address);
    Value* b = root->appendNew<MemoryValue>(proc, Load, V128, Origin(), address, static_cast<int32_t>(sizeof(v128_t)));

    // Inner: swap bytes within 32-bit groups of 'a' → {a[3],a[2],a[1],a[0], a[7],a[6],a[5],a[4], ...}
    v128_t innerPat;
    for (unsigned i = 0; i < 16; i += 4) {
        innerPat.u8x16[i] = i + 3;
        innerPat.u8x16[i + 1] = i + 2;
        innerPat.u8x16[i + 2] = i + 1;
        innerPat.u8x16[i + 3] = i;
    }
    Value* innerPatConst = root->appendNew<Const128Value>(proc, Origin(), innerPat);
    Value* innerResult = root->appendNew<SIMDValue>(proc, Origin(), VectorSwizzle, B3::V128, SIMDLane::i8x16, SIMDSignMode::None, a, innerPatConst);

    // Outer: UZP1.2D(b, inner) = {b[lo64], inner[lo64]}
    v128_t outerPat;
    for (unsigned i = 0; i < 8; ++i) outerPat.u8x16[i] = i;
    for (unsigned i = 0; i < 8; ++i) outerPat.u8x16[8 + i] = 16 + i;
    Value* outerPatConst = root->appendNew<Const128Value>(proc, Origin(), outerPat);
    Value* result = root->appendNew<SIMDValue>(proc, Origin(), VectorSwizzle, B3::V128, SIMDLane::i8x16, SIMDSignMode::None, b, innerResult, outerPatConst);

    root->appendNew<MemoryValue>(proc, Store, Origin(), result, address, static_cast<int32_t>(2 * sizeof(v128_t)));
    root->appendNewControlValue(proc, Return, Origin());

    auto code = compileProc(proc);
    for (unsigned i = 0; i < 16; ++i) vectors[0].u8x16[i] = i;       // a
    for (unsigned i = 0; i < 16; ++i) vectors[1].u8x16[i] = 0x80 + i; // b
    invoke<void>(*code, vectors);

    // Expected: {b[0..7], rev32(a)[0..7]} = {b[0..7], a[3],a[2],a[1],a[0],a[7],a[6],a[5],a[4]}
    for (unsigned i = 0; i < 8; ++i)
        CHECK(vectors[2].u8x16[i] == 0x80 + i);
    CHECK(vectors[2].u8x16[8] == 3);
    CHECK(vectors[2].u8x16[9] == 2);
    CHECK(vectors[2].u8x16[10] == 1);
    CHECK(vectors[2].u8x16[11] == 0);
    CHECK(vectors[2].u8x16[12] == 7);
    CHECK(vectors[2].u8x16[13] == 6);
    CHECK(vectors[2].u8x16[14] == 5);
    CHECK(vectors[2].u8x16[15] == 4);
}

void testVectorSwizzleUnaryComposition()
{
    // Compose two chained unary rotations: EXT4(EXT4(x)) = EXT8(x).
    // inner = VectorSwizzle(input, rotate4Pattern)
    // outer = VectorSwizzle(inner, rotate4Pattern)
    // After composition, this should be a single rotation by 8.

    alignas(16) v128_t vectors[2];
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    auto arguments = cCallArgumentValues<void*>(proc, root);
    Value* address = arguments[0];
    Value* input = root->appendNew<MemoryValue>(proc, Load, V128, Origin(), address);

    // Rotate by 4: {4,5,...,15,0,1,2,3}
    v128_t rotate4;
    for (unsigned i = 0; i < 16; ++i)
        rotate4.u8x16[i] = (4 + i) % 16;
    Value* rotate4Const = root->appendNew<Const128Value>(proc, Origin(), rotate4);

    Value* inner = root->appendNew<SIMDValue>(proc, Origin(), VectorSwizzle, B3::V128, SIMDLane::i8x16, SIMDSignMode::None, input, rotate4Const);
    Value* rotate4Const2 = root->appendNew<Const128Value>(proc, Origin(), rotate4);
    Value* outer = root->appendNew<SIMDValue>(proc, Origin(), VectorSwizzle, B3::V128, SIMDLane::i8x16, SIMDSignMode::None, inner, rotate4Const2);

    root->appendNew<MemoryValue>(proc, Store, Origin(), outer, address, static_cast<int32_t>(sizeof(v128_t)));
    root->appendNewControlValue(proc, Return, Origin());

    auto code = compileProc(proc);
    for (unsigned i = 0; i < 16; ++i) vectors[0].u8x16[i] = i;
    invoke<void>(*code, vectors);

    // Expected: rotation by 8 → {8,9,10,11,12,13,14,15,0,1,2,3,4,5,6,7}
    for (unsigned i = 0; i < 16; ++i)
        CHECK(vectors[1].u8x16[i] == ((8 + i) % 16));
}

void testVectorSwizzleCompositionMultiUse()
{
    // Test that composition works when inner has multiple consumers.
    // inner = VectorSwizzle(src, rev32Pattern)  (used by both outer shuffles)
    // outer1 = VectorSwizzle(other, inner, concatPattern)
    // outer2 = VectorSwizzle(inner, other, concatPattern2)

    alignas(16) v128_t vectors[4];
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    auto arguments = cCallArgumentValues<void*>(proc, root);
    Value* address = arguments[0];
    Value* src = root->appendNew<MemoryValue>(proc, Load, V128, Origin(), address);
    Value* other = root->appendNew<MemoryValue>(proc, Load, V128, Origin(), address, static_cast<int32_t>(sizeof(v128_t)));

    // Inner: reverse bytes within 32-bit groups
    v128_t innerPat;
    for (unsigned i = 0; i < 16; i += 4) {
        innerPat.u8x16[i] = i + 3;
        innerPat.u8x16[i + 1] = i + 2;
        innerPat.u8x16[i + 2] = i + 1;
        innerPat.u8x16[i + 3] = i;
    }
    Value* innerPatConst = root->appendNew<Const128Value>(proc, Origin(), innerPat);
    Value* inner = root->appendNew<SIMDValue>(proc, Origin(), VectorSwizzle, B3::V128, SIMDLane::i8x16, SIMDSignMode::None, src, innerPatConst);

    // outer1 = binary shuffle(other, inner): take lo64 from other, hi64 from inner
    // Pattern: {0..7 from child0, 24..31 from child1}
    v128_t outerPat1;
    for (unsigned i = 0; i < 8; ++i) outerPat1.u8x16[i] = i;         // child0[0..7]
    for (unsigned i = 0; i < 8; ++i) outerPat1.u8x16[8 + i] = 24 + i; // child1[8..15]
    Value* outerPatConst1 = root->appendNew<Const128Value>(proc, Origin(), outerPat1);
    Value* outer1 = root->appendNew<SIMDValue>(proc, Origin(), VectorSwizzle, B3::V128, SIMDLane::i8x16, SIMDSignMode::None, other, inner, outerPatConst1);

    // outer2 = binary shuffle(inner, other): take lo64 from inner, hi64 from other
    v128_t outerPat2;
    for (unsigned i = 0; i < 8; ++i) outerPat2.u8x16[i] = i;           // child0[0..7] = inner[0..7]
    for (unsigned i = 0; i < 8; ++i) outerPat2.u8x16[8 + i] = 24 + i;  // child1[8..15] = other[8..15]
    Value* outerPatConst2 = root->appendNew<Const128Value>(proc, Origin(), outerPat2);
    Value* outer2 = root->appendNew<SIMDValue>(proc, Origin(), VectorSwizzle, B3::V128, SIMDLane::i8x16, SIMDSignMode::None, inner, other, outerPatConst2);

    root->appendNew<MemoryValue>(proc, Store, Origin(), outer1, address, static_cast<int32_t>(2 * sizeof(v128_t)));
    root->appendNew<MemoryValue>(proc, Store, Origin(), outer2, address, static_cast<int32_t>(3 * sizeof(v128_t)));
    root->appendNewControlValue(proc, Return, Origin());

    auto code = compileProc(proc);
    for (unsigned i = 0; i < 16; ++i) vectors[0].u8x16[i] = i;        // src
    for (unsigned i = 0; i < 16; ++i) vectors[1].u8x16[i] = 0x80 + i;  // other
    invoke<void>(*code, vectors);

    // outer1: {other[0..7], rev32(src)[8..15]}
    for (unsigned i = 0; i < 8; ++i)
        CHECK(vectors[2].u8x16[i] == 0x80 + i);
    // rev32(src)[8..15] = {11,10,9,8, 15,14,13,12}
    CHECK(vectors[2].u8x16[8] == 11);
    CHECK(vectors[2].u8x16[9] == 10);
    CHECK(vectors[2].u8x16[10] == 9);
    CHECK(vectors[2].u8x16[11] == 8);
    CHECK(vectors[2].u8x16[12] == 15);
    CHECK(vectors[2].u8x16[13] == 14);
    CHECK(vectors[2].u8x16[14] == 13);
    CHECK(vectors[2].u8x16[15] == 12);

    // outer2: {rev32(src)[0..7], other[8..15]}
    // rev32(src)[0..7] = {3,2,1,0, 7,6,5,4}
    CHECK(vectors[3].u8x16[0] == 3);
    CHECK(vectors[3].u8x16[1] == 2);
    CHECK(vectors[3].u8x16[2] == 1);
    CHECK(vectors[3].u8x16[3] == 0);
    CHECK(vectors[3].u8x16[4] == 7);
    CHECK(vectors[3].u8x16[5] == 6);
    CHECK(vectors[3].u8x16[6] == 5);
    CHECK(vectors[3].u8x16[7] == 4);
    for (unsigned i = 8; i < 16; ++i)
        CHECK(vectors[3].u8x16[i] == 0x80 + i);
}

// Regression test for argon2id wasm SIMD miscompilation on x64 (rdar://...).
//
// OMGIRGenerator::addSIMDShuffle splits a wasm binary i8x16.shuffle on x64
// into two unary VectorSwizzles (one per source) plus a VectorOr. The split
// canonicalizes both patterns so that valid indices are in 0..15 and OOB
// uses 0xFF — without this canonicalization the rightImm-side pattern would
// keep the original wasm bytes 16..31 (relying on PSHUFB's index masking),
// which broke B3ReduceStrength's SwizzleSwizzle composition (composeUnaryShuffle
// treats bytes ≥ 16 as OOB per ARM64 TBL semantics, producing all-0xFF and
// thus all-zero PSHUFB output instead of the correctly shuffled bytes).
//
// This test reproduces the failing pattern shape directly in B3 IR using
// canonicalized patterns and verifies that composing two unary VectorSwizzles
// (rotr16 inner ∘ "lower-8-bytes-of-inner-then-OOB" outer) yields the correct
// PSHUFB result rather than zero.
void testVectorSwizzleCompositionRightImmOuter()
{
    alignas(16) v128_t vectors[2];
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    auto arguments = cCallArgumentValues<void*>(proc, root);
    Value* address = arguments[0];
    Value* input = root->appendNew<MemoryValue>(proc, Load, V128, Origin(), address);

    // Inner: rotr16 of i64x2 (the byte pattern argon2 uses for one of its
    // BLAKE2b rotations). Bytes 0..15.
    v128_t innerPat = v128_t::fromU8x16(2, 3, 4, 5, 6, 7, 0, 1, 10, 11, 12, 13, 14, 15, 8, 9);
    Value* innerPatConst = root->appendNew<Const128Value>(proc, Origin(), innerPat);
    Value* inner = root->appendNew<SIMDValue>(proc, Origin(), VectorSwizzle, B3::V128, SIMDLane::i8x16, SIMDSignMode::None, input, innerPatConst);

    // Outer: take low 8 bytes of inner_result and zero the rest. This is the
    // canonicalized form of what addSIMDShuffle now emits as the leftImm or
    // rightImm pattern for a binary shuffle that selects only one half from
    // a single side. Bytes 0..7 valid, bytes 8..15 OOB (0xFF).
    v128_t outerPat;
    for (unsigned i = 0; i < 8; ++i)
        outerPat.u8x16[i] = static_cast<uint8_t>(i);
    for (unsigned i = 8; i < 16; ++i)
        outerPat.u8x16[i] = 0xFF;
    Value* outerPatConst = root->appendNew<Const128Value>(proc, Origin(), outerPat);
    Value* outer = root->appendNew<SIMDValue>(proc, Origin(), VectorSwizzle, B3::V128, SIMDLane::i8x16, SIMDSignMode::None, inner, outerPatConst);

    root->appendNew<MemoryValue>(proc, Store, Origin(), outer, address, static_cast<int32_t>(sizeof(v128_t)));
    root->appendNewControlValue(proc, Return, Origin());

    auto code = compileProc(proc);
    for (unsigned i = 0; i < 16; ++i)
        vectors[0].u8x16[i] = static_cast<uint8_t>(0x10 + i);
    invoke<void>(*code, vectors);

    // Composition collapses to PSHUFB(input, composed) where
    //   composed[i] = innerPat[outerPat[i]] for outerPat[i] < 16, else 0xFF.
    // For i in 0..7: composed[i] = innerPat[i] = rotr16's bytes 0..7
    //                            = {2, 3, 4, 5, 6, 7, 0, 1} (input indices).
    // For i in 8..15: composed[i] = 0xFF → 0.
    // So result.bytes[0..7] = input[2..7, 0..1], result.bytes[8..15] = 0.
    static constexpr uint8_t expectedLowIndices[8] = { 2, 3, 4, 5, 6, 7, 0, 1 };
    for (unsigned i = 0; i < 8; ++i)
        CHECK(vectors[1].u8x16[i] == 0x10 + expectedLowIndices[i]);
    for (unsigned i = 8; i < 16; ++i)
        CHECK(vectors[1].u8x16[i] == 0);
}

// Helpers for testVectorSwizzleBinaryOnlyOneSide{Side0,Side1,WithOOB,AllOOB,Mixed}.
// Each builds a 3-child VectorSwizzle(a, b, pattern) with the given pattern.
//
// The 3-child (binary) VectorSwizzle is only lowered to Air on ARM64
// (VectorSwizzle2 → TBL2). On x86 the binary form would need to be reduced by
// B3ReduceStrength to a 2-child unary form before lowering, but at O0 the
// reductions don't run, so directly constructing a 3-child VectorSwizzle on
// x86 would crash. These tests are therefore registered only under
// `if (isARM64())` in testb3_1.cpp. The x86 side of isOnlyOneSideMask is
// exercised end-to-end by JSTests/wasm/stress/simd-shuffle-compose-rightimm.js
// (and the broader wasm SIMD tests), where OMGIRGenerator::addSIMDShuffle
// builds the equivalent unary forms and feeds them through the same
// B3ReduceStrength pipeline.
//
// Expected output is computed byte-by-byte using wasm i8x16.shuffle semantics:
//   result[i] = pattern[i] < 16 ? a[pattern[i]]
//             : pattern[i] < 32 ? b[pattern[i] - 16]
//             : 0 (OOB).
static void runBinarySwizzleAndCheck(v128_t pattern, v128_t aIn, v128_t bIn)
{
    alignas(16) v128_t vectors[3];
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    auto arguments = cCallArgumentValues<void*>(proc, root);
    Value* address = arguments[0];
    Value* a = root->appendNew<MemoryValue>(proc, Load, V128, Origin(), address);
    Value* b = root->appendNew<MemoryValue>(proc, Load, V128, Origin(), address, static_cast<int32_t>(sizeof(v128_t)));
    Value* patternConst = root->appendNew<Const128Value>(proc, Origin(), pattern);
    Value* result = root->appendNew<SIMDValue>(proc, Origin(), VectorSwizzle, B3::V128, SIMDLane::i8x16, SIMDSignMode::None, a, b, patternConst);
    root->appendNew<MemoryValue>(proc, Store, Origin(), result, address, static_cast<int32_t>(2 * sizeof(v128_t)));
    root->appendNewControlValue(proc, Return, Origin());

    auto code = compileProc(proc);
    vectors[0] = aIn;
    vectors[1] = bIn;
    invoke<void>(*code, vectors);

    for (unsigned i = 0; i < 16; ++i) {
        uint8_t idx = pattern.u8x16[i];
        uint8_t expected;
        if (idx < 16)
            expected = aIn.u8x16[idx];
        else if (idx < 32)
            expected = bIn.u8x16[idx - 16];
        else
            expected = 0;
        CHECK(vectors[2].u8x16[i] == expected);
    }
}

// Pattern reads only from `a` (no OOB). isOnlyOneSideMask must return
// {child=0, normalized=pattern} and the binary VectorSwizzle must collapse
// to a unary VectorSwizzle on `a`. On x64 the lowering is PSHUFB(a, pattern).
void testVectorSwizzleBinaryOnlyOneSideSide0()
{
    // Reverse bytes within 32-bit groups. Reads only from `a`.
    v128_t pattern;
    for (unsigned i = 0; i < 16; i += 4) {
        pattern.u8x16[i + 0] = static_cast<uint8_t>(i + 3);
        pattern.u8x16[i + 1] = static_cast<uint8_t>(i + 2);
        pattern.u8x16[i + 2] = static_cast<uint8_t>(i + 1);
        pattern.u8x16[i + 3] = static_cast<uint8_t>(i + 0);
    }
    v128_t a, b;
    for (unsigned i = 0; i < 16; ++i) a.u8x16[i] = static_cast<uint8_t>(0x10 + i);
    for (unsigned i = 0; i < 16; ++i) b.u8x16[i] = static_cast<uint8_t>(0x80 + i);
    runBinarySwizzleAndCheck(pattern, a, b);
}

// Pattern reads only from `b` (indices 16..31, no OOB). Must collapse to a
// unary VectorSwizzle on `b` with normalized pattern (idx - 16) and on x64
// must NOT mistake the high indices as OOB after composition (the bug fixed
// by canonicalising rightImm in addSIMDShuffle).
void testVectorSwizzleBinaryOnlyOneSideSide1()
{
    // Reverse bytes within 32-bit groups, but reading from `b`.
    v128_t pattern;
    for (unsigned i = 0; i < 16; i += 4) {
        pattern.u8x16[i + 0] = static_cast<uint8_t>(16 + i + 3);
        pattern.u8x16[i + 1] = static_cast<uint8_t>(16 + i + 2);
        pattern.u8x16[i + 2] = static_cast<uint8_t>(16 + i + 1);
        pattern.u8x16[i + 3] = static_cast<uint8_t>(16 + i + 0);
    }
    v128_t a, b;
    for (unsigned i = 0; i < 16; ++i) a.u8x16[i] = static_cast<uint8_t>(0x80 + i);
    for (unsigned i = 0; i < 16; ++i) b.u8x16[i] = static_cast<uint8_t>(0x10 + i);
    runBinarySwizzleAndCheck(pattern, a, b);
}

// Pattern reads from `a` for some bytes and is OOB (≥ 32) for others. Must
// collapse to a unary VectorSwizzle on `a` whose pattern has 0xFF in the OOB
// positions, producing zero bytes there.
void testVectorSwizzleBinaryOnlyOneSideSide0WithOOB()
{
    v128_t pattern;
    for (unsigned i = 0; i < 8; ++i)
        pattern.u8x16[i] = static_cast<uint8_t>(2 * i); // {0, 2, 4, 6, 8, 10, 12, 14} from `a`
    for (unsigned i = 8; i < 16; ++i)
        pattern.u8x16[i] = 0xFF; // OOB (≥ 32)
    v128_t a, b;
    for (unsigned i = 0; i < 16; ++i) a.u8x16[i] = static_cast<uint8_t>(0x10 + i);
    for (unsigned i = 0; i < 16; ++i) b.u8x16[i] = static_cast<uint8_t>(0x80 + i);
    runBinarySwizzleAndCheck(pattern, a, b);
}

// Pattern reads from `b` for some bytes and is OOB for others.
void testVectorSwizzleBinaryOnlyOneSideSide1WithOOB()
{
    v128_t pattern;
    for (unsigned i = 0; i < 8; ++i)
        pattern.u8x16[i] = 0xFF; // OOB
    for (unsigned i = 8; i < 16; ++i)
        pattern.u8x16[i] = static_cast<uint8_t>(16 + 2 * (i - 8)); // {16,18,20,22,24,26,28,30} from `b`
    v128_t a, b;
    for (unsigned i = 0; i < 16; ++i) a.u8x16[i] = static_cast<uint8_t>(0x80 + i);
    for (unsigned i = 0; i < 16; ++i) b.u8x16[i] = static_cast<uint8_t>(0x10 + i);
    runBinarySwizzleAndCheck(pattern, a, b);
}

// All bytes are OOB (≥ 32). isOnlyOneSideMask returns {child=2, all-ones}
// and the swizzle must collapse to a constant all-zero vector.
void testVectorSwizzleBinaryOnlyOneSideAllOOB()
{
    v128_t pattern;
    for (unsigned i = 0; i < 16; ++i)
        pattern.u8x16[i] = 0xFF;
    v128_t a, b;
    for (unsigned i = 0; i < 16; ++i) a.u8x16[i] = static_cast<uint8_t>(0x80 + i);
    for (unsigned i = 0; i < 16; ++i) b.u8x16[i] = static_cast<uint8_t>(0x70 + i);
    runBinarySwizzleAndCheck(pattern, a, b);
}

// Pattern uses bytes from BOTH sides — isOnlyOneSideMask must return nullopt.
// On x64 this still must compile correctly (other binary reductions like
// canonical UZP/ZIP/EXT or the input-equality normalization step kick in).
// Use S64x2UnzipEven canonical pattern {0..7, 16..23} to ensure a downstream
// reduction can handle it.
void testVectorSwizzleBinaryOnlyOneSideMixed()
{
    v128_t pattern;
    for (unsigned i = 0; i < 8; ++i)
        pattern.u8x16[i] = static_cast<uint8_t>(i); // a[0..7]
    for (unsigned i = 0; i < 8; ++i)
        pattern.u8x16[8 + i] = static_cast<uint8_t>(16 + i); // b[0..7]
    v128_t a, b;
    for (unsigned i = 0; i < 16; ++i) a.u8x16[i] = static_cast<uint8_t>(0x10 + i);
    for (unsigned i = 0; i < 16; ++i) b.u8x16[i] = static_cast<uint8_t>(0x80 + i);
    runBinarySwizzleAndCheck(pattern, a, b);
}

// Pattern is byte-reverse-of-side-1 with OOB scattered through it. Tests
// that the normalization in isOnlyOneSideMask correctly subtracts 16 from
// every valid byte even when the OOB sentinels are interleaved (rather than
// in a contiguous run).
void testVectorSwizzleBinaryOnlyOneSideSide1Scattered()
{
    v128_t pattern;
    // Alternating: b[0], OOB, b[2], OOB, ..., b[14], OOB.
    for (unsigned i = 0; i < 16; ++i) {
        if (i % 2 == 0)
            pattern.u8x16[i] = static_cast<uint8_t>(16 + i);
        else
            pattern.u8x16[i] = 0xFF;
    }
    v128_t a, b;
    for (unsigned i = 0; i < 16; ++i) a.u8x16[i] = static_cast<uint8_t>(0x80 + i);
    for (unsigned i = 0; i < 16; ++i) b.u8x16[i] = static_cast<uint8_t>(0x10 + i);
    runBinarySwizzleAndCheck(pattern, a, b);
}

// Test VectorShr(VectorZip{Lower,Higher}(x, x), 8) → VectorExtend{Low,High}(x, i16x8, Signed)
void testVectorShrZipToExtend()
{
    // Test ExtendLow: VectorShr(VectorZipLower(x, x), 8) → VectorExtendLow(x, i16x8, Signed)
    {
        alignas(16) v128_t vectors[2];
        Procedure proc;
        BasicBlock* root = proc.addBlock();
        auto arguments = cCallArgumentValues<void*>(proc, root);
        Value* address = arguments[0];
        Value* input = root->appendNew<MemoryValue>(proc, Load, V128, Origin(), address);

        Value* shiftAmount = root->appendNew<Const32Value>(proc, Origin(), 8);

        // VectorZipLower(input, input) on i8x16
        Value* zipped = root->appendNew<SIMDValue>(proc, Origin(), VectorZipLower, B3::V128, SIMDLane::i8x16, SIMDSignMode::None, input, input);
        // VectorShr(zipped, 8) on i16x8 Signed
        Value* shifted = root->appendNew<SIMDValue>(proc, Origin(), VectorShr, B3::V128, SIMDLane::i16x8, SIMDSignMode::Signed, zipped, shiftAmount);

        root->appendNew<MemoryValue>(proc, Store, Origin(), shifted, address, static_cast<int32_t>(sizeof(v128_t)));
        root->appendNewControlValue(proc, Return, Origin());

        auto code = compileProc(proc);

        // Input i8x16: {-5, 127, -128, 0, 1, -1, 42, -42, 99, 100, 101, 102, 103, 104, 105, 106}
        vectors[0].u8x16[0] = static_cast<uint8_t>(-5);   // 0xFB → sign-extend to 0xFFFB = -5
        vectors[0].u8x16[1] = 127;                          // 0x7F → 0x007F = 127
        vectors[0].u8x16[2] = static_cast<uint8_t>(-128); // 0x80 → 0xFF80 = -128
        vectors[0].u8x16[3] = 0;                            // 0x00 → 0x0000 = 0
        vectors[0].u8x16[4] = 1;                            // 0x01 → 0x0001 = 1
        vectors[0].u8x16[5] = static_cast<uint8_t>(-1);   // 0xFF → 0xFFFF = -1
        vectors[0].u8x16[6] = 42;                           // 0x2A → 0x002A = 42
        vectors[0].u8x16[7] = static_cast<uint8_t>(-42);  // 0xD6 → 0xFFD6 = -42
        // Upper 8 bytes (not used by ExtendLow, but fill them)
        for (unsigned i = 8; i < 16; ++i)
            vectors[0].u8x16[i] = 99 + (i - 8);

        invoke<void>(*code, vectors);

        // Result: sign-extended lower 8 i8 values to i16x8
        CHECK(vectors[1].u16x8[0] == static_cast<uint16_t>(-5));
        CHECK(vectors[1].u16x8[1] == static_cast<uint16_t>(127));
        CHECK(vectors[1].u16x8[2] == static_cast<uint16_t>(-128));
        CHECK(vectors[1].u16x8[3] == static_cast<uint16_t>(0));
        CHECK(vectors[1].u16x8[4] == static_cast<uint16_t>(1));
        CHECK(vectors[1].u16x8[5] == static_cast<uint16_t>(-1));
        CHECK(vectors[1].u16x8[6] == static_cast<uint16_t>(42));
        CHECK(vectors[1].u16x8[7] == static_cast<uint16_t>(-42));
    }

    // Test ExtendHigh: VectorShr(VectorZipHigher(x, x), 8) → VectorExtendHigh(x, i16x8, Signed)
    {
        alignas(16) v128_t vectors[2];
        Procedure proc;
        BasicBlock* root = proc.addBlock();
        auto arguments = cCallArgumentValues<void*>(proc, root);
        Value* address = arguments[0];
        Value* input = root->appendNew<MemoryValue>(proc, Load, V128, Origin(), address);

        Value* shiftAmount = root->appendNew<Const32Value>(proc, Origin(), 8);

        // VectorZipHigher(input, input) on i8x16
        Value* zipped = root->appendNew<SIMDValue>(proc, Origin(), VectorZipHigher, B3::V128, SIMDLane::i8x16, SIMDSignMode::None, input, input);
        // VectorShr(zipped, 8) on i16x8 Signed
        Value* shifted = root->appendNew<SIMDValue>(proc, Origin(), VectorShr, B3::V128, SIMDLane::i16x8, SIMDSignMode::Signed, zipped, shiftAmount);

        root->appendNew<MemoryValue>(proc, Store, Origin(), shifted, address, static_cast<int32_t>(sizeof(v128_t)));
        root->appendNewControlValue(proc, Return, Origin());

        auto code = compileProc(proc);

        // Input: lower 8 bytes don't matter for ExtendHigh, upper 8 bytes are tested
        for (unsigned i = 0; i < 8; ++i)
            vectors[0].u8x16[i] = 0;
        vectors[0].u8x16[8] = static_cast<uint8_t>(-10);   // 0xF6 → -10
        vectors[0].u8x16[9] = 50;                            // 0x32 → 50
        vectors[0].u8x16[10] = static_cast<uint8_t>(-128); // 0x80 → -128
        vectors[0].u8x16[11] = 127;                          // 0x7F → 127
        vectors[0].u8x16[12] = 0;                            // 0x00 → 0
        vectors[0].u8x16[13] = static_cast<uint8_t>(-1);   // 0xFF → -1
        vectors[0].u8x16[14] = 1;                            // 0x01 → 1
        vectors[0].u8x16[15] = static_cast<uint8_t>(-100); // 0x9C → -100

        invoke<void>(*code, vectors);

        // Result: sign-extended upper 8 i8 values to i16x8
        CHECK(vectors[1].u16x8[0] == static_cast<uint16_t>(-10));
        CHECK(vectors[1].u16x8[1] == static_cast<uint16_t>(50));
        CHECK(vectors[1].u16x8[2] == static_cast<uint16_t>(-128));
        CHECK(vectors[1].u16x8[3] == static_cast<uint16_t>(127));
        CHECK(vectors[1].u16x8[4] == static_cast<uint16_t>(0));
        CHECK(vectors[1].u16x8[5] == static_cast<uint16_t>(-1));
        CHECK(vectors[1].u16x8[6] == static_cast<uint16_t>(1));
        CHECK(vectors[1].u16x8[7] == static_cast<uint16_t>(-100));
    }
}

// Test VectorShr(VectorZip{Lower,Higher}(x, x), 16) -> VectorExtend{Low,High}(x, i32x4, Signed)
void testVectorShrZipToExtendI32()
{
    // Test ExtendLow: VectorShr(VectorZipLower(x, x), 16) on i32x4 -> VectorExtendLow(x, i32x4, Signed)
    {
        alignas(16) v128_t vectors[2];
        Procedure proc;
        BasicBlock* root = proc.addBlock();
        auto arguments = cCallArgumentValues<void*>(proc, root);
        Value* address = arguments[0];
        Value* input = root->appendNew<MemoryValue>(proc, Load, V128, Origin(), address);

        Value* shiftAmount = root->appendNew<Const32Value>(proc, Origin(), 16);

        // VectorZipLower(input, input) on i16x8
        Value* zipped = root->appendNew<SIMDValue>(proc, Origin(), VectorZipLower, B3::V128, SIMDLane::i16x8, SIMDSignMode::None, input, input);
        // VectorShr(zipped, 16) on i32x4 Signed
        Value* shifted = root->appendNew<SIMDValue>(proc, Origin(), VectorShr, B3::V128, SIMDLane::i32x4, SIMDSignMode::Signed, zipped, shiftAmount);

        root->appendNew<MemoryValue>(proc, Store, Origin(), shifted, address, static_cast<int32_t>(sizeof(v128_t)));
        root->appendNewControlValue(proc, Return, Origin());

        auto code = compileProc(proc);

        // Input i16x8: {-5, 32767, -32768, 0, 1, -1, 42, -42}
        vectors[0].u16x8[0] = static_cast<uint16_t>(-5);      // 0xFFFB -> sign-extend to 0xFFFFFFFB = -5
        vectors[0].u16x8[1] = 32767;                            // 0x7FFF -> 0x00007FFF = 32767
        vectors[0].u16x8[2] = static_cast<uint16_t>(-32768);  // 0x8000 -> 0xFFFF8000 = -32768
        vectors[0].u16x8[3] = 0;                                // 0x0000 -> 0x00000000 = 0
        // Upper 4 i16 values (not used by ExtendLow)
        vectors[0].u16x8[4] = 1;
        vectors[0].u16x8[5] = static_cast<uint16_t>(-1);
        vectors[0].u16x8[6] = 42;
        vectors[0].u16x8[7] = static_cast<uint16_t>(-42);

        invoke<void>(*code, vectors);

        // Result: sign-extended lower 4 i16 values to i32x4
        CHECK(vectors[1].u32x4[0] == static_cast<uint32_t>(-5));
        CHECK(vectors[1].u32x4[1] == static_cast<uint32_t>(32767));
        CHECK(vectors[1].u32x4[2] == static_cast<uint32_t>(-32768));
        CHECK(vectors[1].u32x4[3] == static_cast<uint32_t>(0));
    }

    // Test ExtendHigh: VectorShr(VectorZipHigher(x, x), 16) on i32x4 -> VectorExtendHigh(x, i32x4, Signed)
    {
        alignas(16) v128_t vectors[2];
        Procedure proc;
        BasicBlock* root = proc.addBlock();
        auto arguments = cCallArgumentValues<void*>(proc, root);
        Value* address = arguments[0];
        Value* input = root->appendNew<MemoryValue>(proc, Load, V128, Origin(), address);

        Value* shiftAmount = root->appendNew<Const32Value>(proc, Origin(), 16);

        // VectorZipHigher(input, input) on i16x8
        Value* zipped = root->appendNew<SIMDValue>(proc, Origin(), VectorZipHigher, B3::V128, SIMDLane::i16x8, SIMDSignMode::None, input, input);
        // VectorShr(zipped, 16) on i32x4 Signed
        Value* shifted = root->appendNew<SIMDValue>(proc, Origin(), VectorShr, B3::V128, SIMDLane::i32x4, SIMDSignMode::Signed, zipped, shiftAmount);

        root->appendNew<MemoryValue>(proc, Store, Origin(), shifted, address, static_cast<int32_t>(sizeof(v128_t)));
        root->appendNewControlValue(proc, Return, Origin());

        auto code = compileProc(proc);

        // Input: lower 4 i16 values don't matter for ExtendHigh
        for (unsigned i = 0; i < 4; ++i)
            vectors[0].u16x8[i] = 0;
        vectors[0].u16x8[4] = static_cast<uint16_t>(-10);     // 0xFFF6 -> -10
        vectors[0].u16x8[5] = 50;                               // 0x0032 -> 50
        vectors[0].u16x8[6] = static_cast<uint16_t>(-32768);  // 0x8000 -> -32768
        vectors[0].u16x8[7] = 32767;                            // 0x7FFF -> 32767

        invoke<void>(*code, vectors);

        // Result: sign-extended upper 4 i16 values to i32x4
        CHECK(vectors[1].u32x4[0] == static_cast<uint32_t>(-10));
        CHECK(vectors[1].u32x4[1] == static_cast<uint32_t>(50));
        CHECK(vectors[1].u32x4[2] == static_cast<uint32_t>(-32768));
        CHECK(vectors[1].u32x4[3] == static_cast<uint32_t>(32767));
    }
}

// Test VectorShr(VectorZip{Lower,Higher}(x, x), 32) -> VectorExtend{Low,High}(x, i64x2, Signed)
void testVectorShrZipToExtendI64()
{
    // Test ExtendLow: VectorShr(VectorZipLower(x, x), 32) on i64x2 -> VectorExtendLow(x, i64x2, Signed)
    {
        alignas(16) v128_t vectors[2];
        Procedure proc;
        BasicBlock* root = proc.addBlock();
        auto arguments = cCallArgumentValues<void*>(proc, root);
        Value* address = arguments[0];
        Value* input = root->appendNew<MemoryValue>(proc, Load, V128, Origin(), address);

        Value* shiftAmount = root->appendNew<Const32Value>(proc, Origin(), 32);

        // VectorZipLower(input, input) on i32x4
        Value* zipped = root->appendNew<SIMDValue>(proc, Origin(), VectorZipLower, B3::V128, SIMDLane::i32x4, SIMDSignMode::None, input, input);
        // VectorShr(zipped, 32) on i64x2 Signed
        Value* shifted = root->appendNew<SIMDValue>(proc, Origin(), VectorShr, B3::V128, SIMDLane::i64x2, SIMDSignMode::Signed, zipped, shiftAmount);

        root->appendNew<MemoryValue>(proc, Store, Origin(), shifted, address, static_cast<int32_t>(sizeof(v128_t)));
        root->appendNewControlValue(proc, Return, Origin());

        auto code = compileProc(proc);

        // Input i32x4: {-5, 2147483647}  (lower 2 used by ExtendLow)
        vectors[0].u32x4[0] = static_cast<uint32_t>(-5);           // 0xFFFFFFFB -> sign-extend to 0xFFFFFFFFFFFFFFFB = -5
        vectors[0].u32x4[1] = static_cast<uint32_t>(2147483647);   // 0x7FFFFFFF -> 0x000000007FFFFFFF
        // Upper 2 i32 values (not used by ExtendLow)
        vectors[0].u32x4[2] = static_cast<uint32_t>(-2147483648LL);
        vectors[0].u32x4[3] = 0;

        invoke<void>(*code, vectors);

        // Result: sign-extended lower 2 i32 values to i64x2
        CHECK(vectors[1].u64x2[0] == static_cast<uint64_t>(static_cast<int64_t>(-5)));
        CHECK(vectors[1].u64x2[1] == static_cast<uint64_t>(static_cast<int64_t>(2147483647)));
    }

    // Test ExtendHigh: VectorShr(VectorZipHigher(x, x), 32) on i64x2 -> VectorExtendHigh(x, i64x2, Signed)
    {
        alignas(16) v128_t vectors[2];
        Procedure proc;
        BasicBlock* root = proc.addBlock();
        auto arguments = cCallArgumentValues<void*>(proc, root);
        Value* address = arguments[0];
        Value* input = root->appendNew<MemoryValue>(proc, Load, V128, Origin(), address);

        Value* shiftAmount = root->appendNew<Const32Value>(proc, Origin(), 32);

        // VectorZipHigher(input, input) on i32x4
        Value* zipped = root->appendNew<SIMDValue>(proc, Origin(), VectorZipHigher, B3::V128, SIMDLane::i32x4, SIMDSignMode::None, input, input);
        // VectorShr(zipped, 32) on i64x2 Signed
        Value* shifted = root->appendNew<SIMDValue>(proc, Origin(), VectorShr, B3::V128, SIMDLane::i64x2, SIMDSignMode::Signed, zipped, shiftAmount);

        root->appendNew<MemoryValue>(proc, Store, Origin(), shifted, address, static_cast<int32_t>(sizeof(v128_t)));
        root->appendNewControlValue(proc, Return, Origin());

        auto code = compileProc(proc);

        // Input: lower 2 i32 values don't matter for ExtendHigh
        vectors[0].u32x4[0] = 0;
        vectors[0].u32x4[1] = 0;
        vectors[0].u32x4[2] = static_cast<uint32_t>(-2147483648LL);  // 0x80000000 -> -2147483648
        vectors[0].u32x4[3] = static_cast<uint32_t>(-1);             // 0xFFFFFFFF -> -1

        invoke<void>(*code, vectors);

        // Result: sign-extended upper 2 i32 values to i64x2
        CHECK(vectors[1].u64x2[0] == static_cast<uint64_t>(static_cast<int64_t>(-2147483648LL)));
        CHECK(vectors[1].u64x2[1] == static_cast<uint64_t>(static_cast<int64_t>(-1)));
    }
}

#endif // ENABLE(B3_JIT)

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
