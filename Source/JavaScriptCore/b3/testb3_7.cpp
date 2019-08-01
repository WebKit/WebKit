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

void testPinRegisters()
{
    auto go = [&] (bool pin) {
        Procedure proc;
        RegisterSet csrs;
        csrs.merge(RegisterSet::calleeSaveRegisters());
        csrs.exclude(RegisterSet::stackRegisters());
        if (pin) {
            csrs.forEach(
                [&] (Reg reg) {
                    proc.pinRegister(reg);
                });
        }
        BasicBlock* root = proc.addBlock();
        Value* a = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
        Value* b = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
        Value* c = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR2);
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
                            usesCSRs |= csrs.get(tmp.reg());
                    });
            }
        }
        if (proc.optLevel() < 2) {
            // Our less good register allocators may use the
            // pinned CSRs in a move.
            usesCSRs = false;
        }
        for (const RegisterAtOffset& regAtOffset : proc.calleeSaveRegisterAtOffsetList())
            usesCSRs |= csrs.get(regAtOffset.reg());
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
    Value* result = root->appendNew<Value>(
        proc, Add, Origin(),
        root->appendNew<Value>(
            proc, Add, Origin(),
            root->appendNew<Value>(
                proc, Shl, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1),
                root->appendNew<Const32Value>(proc, Origin(), 2)),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)),
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
    Value* result = root->appendNew<Value>(
        proc, Add, Origin(),
        root->appendNew<Value>(
            proc, Add, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
            root->appendNew<Value>(
                proc, Shl, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1),
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
    Value* result = root->appendNew<Value>(
        proc, Add, Origin(),
        root->appendNew<Value>(
            proc, Add, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)),
        root->appendNew<ConstPtrValue>(proc, Origin(), 100));
    root->appendNew<Value>(proc, Return, Origin(), result);

    auto code = compileProc(proc);
    CHECK_EQ(invoke<intptr_t>(*code, 1, 2), (1 + 2) + 100);
    if (proc.optLevel() > 1) {
        checkDisassembly(
            *code,
            [&] (const char* disassembly) -> bool {
                return strstr(disassembly, "lea 0x64(%rdi,%rsi), %rax")
                    || strstr(disassembly, "lea 0x64(%rsi,%rdi), %rax");
            },
            "Expected to find something like lea 0x64(%rdi,%rsi), %rax but didn't!");
    }
}

void testX86LeaAddShlRight()
{
    // Add(Shl(@x, $c), @y)
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* result = root->appendNew<Value>(
        proc, Add, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
        root->appendNew<Value>(
            proc, Shl, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1),
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
    Value* result = root->appendNew<Value>(
        proc, Add, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
        root->appendNew<Value>(
            proc, Shl, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1),
            root->appendNew<Const32Value>(proc, Origin(), 0)));
    root->appendNew<Value>(proc, Return, Origin(), result);

    auto code = compileProc(proc);
    CHECK_EQ(invoke<intptr_t>(*code, 1, 2), 1 + 2);
    if (proc.optLevel() > 1) {
        checkDisassembly(
            *code,
            [&] (const char* disassembly) -> bool {
                return strstr(disassembly, "lea (%rdi,%rsi), %rax")
                    || strstr(disassembly, "lea (%rsi,%rdi), %rax");
            },
            "Expected to find something like lea (%rdi,%rsi), %rax but didn't!");
    }
}

void testX86LeaAddShlLeftScale2()
{
    // Add(Shl(@x, $c), @y)
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* result = root->appendNew<Value>(
        proc, Add, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
        root->appendNew<Value>(
            proc, Shl, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1),
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
    Value* result = root->appendNew<Value>(
        proc, Add, Origin(),
        root->appendNew<Value>(
            proc, Shl, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1),
            root->appendNew<Const32Value>(proc, Origin(), 2)),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
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
    Value* result = root->appendNew<Value>(
        proc, Add, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
        root->appendNew<Value>(
            proc, Shl, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1),
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
    Value* result = root->appendNew<Value>(
        proc, Add, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
        root->appendNew<Value>(
            proc, Shl, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1),
            root->appendNew<Const32Value>(proc, Origin(), 32)));
    root->appendNew<Value>(proc, Return, Origin(), result);

    auto code = compileProc(proc);
    CHECK_EQ(invoke<int64_t>(*code, 1, 2), 1 + (static_cast<int64_t>(2) << static_cast<int64_t>(32)));
}

void testAddShl64()
{
    // Add(Shl(@x, $c), @y)
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* result = root->appendNew<Value>(
        proc, Add, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
        root->appendNew<Value>(
            proc, Shl, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1),
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
    Value* result = root->appendNew<Value>(
        proc, Add, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
        root->appendNew<Value>(
            proc, Shl, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1),
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
    Value* arg1 = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    Value* arg2 = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);

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

    if (shouldBeVerbose()) {
        dataLog("IR before reduceStrength:\n");
        dataLog(proc);
    }

    reduceStrength(proc);

    if (shouldBeVerbose()) {
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

void testLoadBaseIndexShift2()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNew<Value>(
        proc, Return, Origin(),
        root->appendNew<MemoryValue>(
            proc, Load, Int32, Origin(),
            root->appendNew<Value>(
                proc, Add, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
                root->appendNew<Value>(
                    proc, Shl, Origin(),
                    root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1),
                    root->appendNew<Const32Value>(proc, Origin(), 2)))));
    auto code = compileProc(proc);
    if (isX86() && proc.optLevel() > 1)
        checkUsesInstruction(*code, "(%rdi,%rsi,4)");
    int32_t value = 12341234;
    char* ptr = bitwise_cast<char*>(&value);
    for (unsigned i = 0; i < 10; ++i)
        CHECK_EQ(invoke<int32_t>(*code, ptr - (static_cast<intptr_t>(1) << static_cast<intptr_t>(2)) * i, i), 12341234);
}

void testLoadBaseIndexShift32()
{
#if CPU(ADDRESS64)
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNew<Value>(
        proc, Return, Origin(),
        root->appendNew<MemoryValue>(
            proc, Load, Int32, Origin(),
            root->appendNew<Value>(
                proc, Add, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
                root->appendNew<Value>(
                    proc, Shl, Origin(),
                    root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1),
                    root->appendNew<Const32Value>(proc, Origin(), 32)))));
    auto code = compileProc(proc);
    int32_t value = 12341234;
    char* ptr = bitwise_cast<char*>(&value);
    for (unsigned i = 0; i < 10; ++i)
        CHECK_EQ(invoke<int32_t>(*code, ptr - (static_cast<intptr_t>(1) << static_cast<intptr_t>(32)) * i, i), 12341234);
#endif
}

void testOptimizeMaterialization()
{
    Procedure proc;
    if (proc.optLevel() < 2)
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

static std::array<int, 100> makeArrayForLoops()
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
                    loopHeader->appendNew<Value>(proc, ZExt32, Origin(), index),
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

static int oneFunction(int* callCount)
{
    (*callCount)++;
    return 1;
}

static void noOpFunction()
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
            return loop->appendNew<CCallValue>(
                proc, Int32, Origin(), Effects::none(),
                loop->appendNew<ConstPtrValue>(proc, Origin(), tagCFunctionPtr<void*>(oneFunction, B3CCallPtrTag)),
                loop->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
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
            Effects effects = Effects::none();
            effects.exitsSideways = true;
            loop->appendNew<CCallValue>(
                proc, Void, Origin(), effects,
                loop->appendNew<ConstPtrValue>(proc, Origin(), tagCFunctionPtr<void*>(noOpFunction, B3CCallPtrTag)));

            return loop->appendNew<CCallValue>(
                proc, Int32, Origin(), Effects::none(),
                loop->appendNew<ConstPtrValue>(proc, Origin(), tagCFunctionPtr<void*>(oneFunction, B3CCallPtrTag)),
                loop->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
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
            Effects effects = Effects::none();
            effects.writesPinned = true;
            loop->appendNew<CCallValue>(
                proc, Void, Origin(), effects,
                loop->appendNew<ConstPtrValue>(proc, Origin(), tagCFunctionPtr<void*>(noOpFunction, B3CCallPtrTag)));

            return loop->appendNew<CCallValue>(
                proc, Int32, Origin(), Effects::none(),
                loop->appendNew<ConstPtrValue>(proc, Origin(), tagCFunctionPtr<void*>(oneFunction, B3CCallPtrTag)),
                loop->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
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
            Effects effects = Effects::none();
            effects.writes = HeapRange(63479);
            loop->appendNew<CCallValue>(
                proc, Void, Origin(), effects,
                loop->appendNew<ConstPtrValue>(proc, Origin(), tagCFunctionPtr<void*>(noOpFunction, B3CCallPtrTag)));

            return loop->appendNew<CCallValue>(
                proc, Int32, Origin(), Effects::none(),
                loop->appendNew<ConstPtrValue>(proc, Origin(), tagCFunctionPtr<void*>(oneFunction, B3CCallPtrTag)),
                loop->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
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
            Effects effects = Effects::none();
            effects.readsLocalState = true;
            return loop->appendNew<CCallValue>(
                proc, Int32, Origin(), effects,
                loop->appendNew<ConstPtrValue>(proc, Origin(), tagCFunctionPtr<void*>(oneFunction, B3CCallPtrTag)),
                loop->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
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
            Effects effects = Effects::none();
            effects.readsPinned = true;
            return loop->appendNew<CCallValue>(
                proc, Int32, Origin(), effects,
                loop->appendNew<ConstPtrValue>(proc, Origin(), tagCFunctionPtr<void*>(oneFunction, B3CCallPtrTag)),
                loop->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
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
            Effects effects = Effects::none();
            effects.reads = HeapRange::top();
            return loop->appendNew<CCallValue>(
                proc, Int32, Origin(), effects,
                loop->appendNew<ConstPtrValue>(proc, Origin(), tagCFunctionPtr<void*>(oneFunction, B3CCallPtrTag)),
                loop->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
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
            return loop->appendNew<CCallValue>(
                proc, Int32, Origin(), Effects::none(),
                loop->appendNew<ConstPtrValue>(proc, Origin(), tagCFunctionPtr<void*>(oneFunction, B3CCallPtrTag)),
                loop->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
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
            return loop->appendNew<CCallValue>(
                proc, Int32, Origin(), Effects::none(),
                loop->appendNew<ConstPtrValue>(proc, Origin(), tagCFunctionPtr<void*>(oneFunction, B3CCallPtrTag)),
                loop->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
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
            return loop->appendNew<CCallValue>(
                proc, Int32, Origin(), Effects::none(),
                loop->appendNew<ConstPtrValue>(proc, Origin(), tagCFunctionPtr<void*>(oneFunction, B3CCallPtrTag)),
                loop->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
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
            Effects effects = Effects::none();
            effects.exitsSideways = true;
            return loop->appendNew<CCallValue>(
                proc, Int32, Origin(), effects,
                loop->appendNew<ConstPtrValue>(proc, Origin(), tagCFunctionPtr<void*>(oneFunction, B3CCallPtrTag)),
                loop->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
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
            Effects effects = Effects::none();
            effects.writesLocalState = true;
            return loop->appendNew<CCallValue>(
                proc, Int32, Origin(), effects,
                loop->appendNew<ConstPtrValue>(proc, Origin(), tagCFunctionPtr<void*>(oneFunction, B3CCallPtrTag)),
                loop->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
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
            Effects effects = Effects::none();
            effects.writes = HeapRange(666);
            return loop->appendNew<CCallValue>(
                proc, Int32, Origin(), effects,
                loop->appendNew<ConstPtrValue>(proc, Origin(), tagCFunctionPtr<void*>(oneFunction, B3CCallPtrTag)),
                loop->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
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
            Effects effects = Effects::none();
            effects.fence = true;
            return loop->appendNew<CCallValue>(
                proc, Int32, Origin(), effects,
                loop->appendNew<ConstPtrValue>(proc, Origin(), tagCFunctionPtr<void*>(oneFunction, B3CCallPtrTag)),
                loop->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
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
            Effects effects = Effects::none();
            effects.writesPinned = true;
            return loop->appendNew<CCallValue>(
                proc, Int32, Origin(), effects,
                loop->appendNew<ConstPtrValue>(proc, Origin(), tagCFunctionPtr<void*>(oneFunction, B3CCallPtrTag)),
                loop->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
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
            Effects effects = Effects::none();
            effects.controlDependent = true;
            return loop->appendNew<CCallValue>(
                proc, Int32, Origin(), effects,
                loop->appendNew<ConstPtrValue>(proc, Origin(), tagCFunctionPtr<void*>(oneFunction, B3CCallPtrTag)),
                loop->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
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
            Effects effects = Effects::none();
            effects.controlDependent = true;
            return loop->appendNew<CCallValue>(
                proc, Int32, Origin(), effects,
                loop->appendNew<ConstPtrValue>(proc, Origin(), tagCFunctionPtr<void*>(oneFunction, B3CCallPtrTag)),
                loop->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
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
            Effects effects = Effects::none();
            effects.exitsSideways = true;
            loop->appendNew<CCallValue>(
                proc, Void, Origin(), effects,
                loop->appendNew<ConstPtrValue>(proc, Origin(), tagCFunctionPtr<void*>(noOpFunction, B3CCallPtrTag)));
        
            effects = Effects::none();
            effects.controlDependent = true;
            return loop->appendNew<CCallValue>(
                proc, Int32, Origin(), effects,
                loop->appendNew<ConstPtrValue>(proc, Origin(), tagCFunctionPtr<void*>(oneFunction, B3CCallPtrTag)),
                loop->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
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
            Effects effects = Effects::none();
            effects.writesPinned = true;
            loop->appendNew<CCallValue>(
                proc, Void, Origin(), effects,
                loop->appendNew<ConstPtrValue>(proc, Origin(), tagCFunctionPtr<void*>(noOpFunction, B3CCallPtrTag)));
        
            effects = Effects::none();
            effects.readsPinned = true;
            return loop->appendNew<CCallValue>(
                proc, Int32, Origin(), effects,
                loop->appendNew<ConstPtrValue>(proc, Origin(), tagCFunctionPtr<void*>(oneFunction, B3CCallPtrTag)),
                loop->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
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
            Effects effects = Effects::none();
            effects.writes = HeapRange(6436);
            loop->appendNew<CCallValue>(
                proc, Void, Origin(), effects,
                loop->appendNew<ConstPtrValue>(proc, Origin(), tagCFunctionPtr<void*>(noOpFunction, B3CCallPtrTag)));
        
            effects = Effects::none();
            effects.reads = HeapRange(4886);
            return loop->appendNew<CCallValue>(
                proc, Int32, Origin(), effects,
                loop->appendNew<ConstPtrValue>(proc, Origin(), tagCFunctionPtr<void*>(oneFunction, B3CCallPtrTag)),
                loop->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
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
            Effects effects = Effects::none();
            effects.writes = HeapRange(6436, 74458);
            loop->appendNew<CCallValue>(
                proc, Void, Origin(), effects,
                loop->appendNew<ConstPtrValue>(proc, Origin(), tagCFunctionPtr<void*>(noOpFunction, B3CCallPtrTag)));
        
            effects = Effects::none();
            effects.reads = HeapRange(48864, 78239);
            return loop->appendNew<CCallValue>(
                proc, Int32, Origin(), effects,
                loop->appendNew<ConstPtrValue>(proc, Origin(), tagCFunctionPtr<void*>(oneFunction, B3CCallPtrTag)),
                loop->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
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
            return loop->appendNew<CCallValue>(
                proc, Int32, Origin(),
                loop->appendNew<ConstPtrValue>(proc, Origin(), tagCFunctionPtr<void*>(oneFunction, B3CCallPtrTag)),
                loop->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
        });

    unsigned callCount = 0;
    compileAndRun<void>(proc, &callCount);
    CHECK_EQ(callCount, 100u);
}

void testDepend32()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* ptr = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    Value* first = root->appendNew<MemoryValue>(proc, Load, Int32, Origin(), ptr, 0);
    Value* second = root->appendNew<MemoryValue>(
        proc, Load, Int32, Origin(),
        root->appendNew<Value>(
            proc, Add, Origin(), ptr,
            root->appendNew<Value>(
                proc, ZExt32, Origin(),
                root->appendNew<Value>(proc, Depend, Origin(), first))),
        4);
    root->appendNew<Value>(
        proc, Return, Origin(),
        root->appendNew<Value>(proc, Add, Origin(), first, second));

    int32_t values[2];
    values[0] = 42;
    values[1] = 0xbeef;

    auto code = compileProc(proc);
    if (isARM64())
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
    Value* ptr = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
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

    proc.setWasmBoundsCheckGenerator([=] (CCallHelpers& jit, GPRReg pinnedGPR) {
        CHECK_EQ(pinnedGPR, pinned);

        // This should always work because a function this simple should never have callee
        // saves.
        jit.move(CCallHelpers::TrustedImm32(42), GPRInfo::returnValueGPR);
        jit.emitFunctionEpilogue();
        jit.ret();
    });

    BasicBlock* root = proc.addBlock();
    Value* left = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    if (pointerType() != Int32)
        left = root->appendNew<Value>(proc, Trunc, Origin(), left);
    root->appendNew<WasmBoundsCheckValue>(proc, Origin(), pinned, left, offset);
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

    // Root
    Value* loopCountValue = root->appendNew<Value>(proc, Trunc, Origin(), root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* valueToStore = root->appendNew<Value>(proc, Trunc, Origin(), root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1));
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
    invoke<void>(*code, loopCount, numToStore, values.data());
    for (unsigned value : values)
        CHECK_EQ(numToStore, value);
}

void testFastTLSLoad()
{
#if ENABLE(FAST_TLS_JIT)
    _pthread_setspecific_direct(WTF_TESTING_KEY, bitwise_cast<void*>(static_cast<uintptr_t>(0xbeef)));

    Procedure proc;
    BasicBlock* root = proc.addBlock();

    PatchpointValue* patchpoint = root->appendNew<PatchpointValue>(proc, pointerType(), Origin());
    patchpoint->clobber(RegisterSet::macroScratchRegisters());
    patchpoint->setGenerator(
        [&] (CCallHelpers& jit, const StackmapGenerationParams& params) {
            AllowMacroScratchRegisterUsage allowScratch(jit);
            jit.loadFromTLSPtr(fastTLSOffsetForKey(WTF_TESTING_KEY), params[0].gpr());
        });

    root->appendNew<Value>(proc, Return, Origin(), patchpoint);

    CHECK_EQ(compileAndRun<uintptr_t>(proc), static_cast<uintptr_t>(0xbeef));
#endif
}

void testFastTLSStore()
{
#if ENABLE(FAST_TLS_JIT)
    Procedure proc;
    BasicBlock* root = proc.addBlock();

    PatchpointValue* patchpoint = root->appendNew<PatchpointValue>(proc, Void, Origin());
    patchpoint->clobber(RegisterSet::macroScratchRegisters());
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
    CHECK_EQ(bitwise_cast<uintptr_t>(_pthread_getspecific_direct(WTF_TESTING_KEY)), static_cast<uintptr_t>(0xdead));
#endif
}

static NEVER_INLINE bool doubleEq(double a, double b) { return a == b; }
static NEVER_INLINE bool doubleNeq(double a, double b) { return a != b; }
static NEVER_INLINE bool doubleGt(double a, double b) { return a > b; }
static NEVER_INLINE bool doubleGte(double a, double b) { return a >= b; }
static NEVER_INLINE bool doubleLt(double a, double b) { return a < b; }
static NEVER_INLINE bool doubleLte(double a, double b) { return a <= b; }

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

        CHECK(!!compileAndRun<int32_t>(proc) == std::get<1>(test)(a, b));
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
            CHECK(!!compileAndRun<int32_t>(proc) == expectedResult);
        }
    }
}

void testFloatEqualOrUnorderedFoldingNaN()
{
    StdList<float> nans = {
        bitwise_cast<float>(0xfffffffd),
        bitwise_cast<float>(0xfffffffe),
        bitwise_cast<float>(0xfffffff0),
        static_cast<float>(PNaN),
    };

    unsigned i = 0;
    for (float nan : nans) {
        RELEASE_ASSERT(std::isnan(nan));
        Procedure proc;
        BasicBlock* root = proc.addBlock();
        Value* a = root->appendNew<ConstFloatValue>(proc, Origin(), nan);
        Value* b = root->appendNew<Value>(proc, DoubleToFloat, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR0));

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
        Value* constA = root->appendNew<ConstFloatValue>(proc, Origin(), a);
        Value* b = root->appendNew<Value>(proc, DoubleToFloat, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR0));
        root->appendNewControlValue(proc, Return, Origin(),
            root->appendNew<Value>(
                proc, EqualOrUnordered, Origin(), constA, b));

        auto code = compileProc(proc);

        for (auto& second : floatingPointOperands<float>()) {
            float b = second.value;
            bool expectedResult = (a == b) || std::isunordered(a, b);
            CHECK(!!invoke<int32_t>(*code, static_cast<double>(b)) == expectedResult);
        }
    }
}

static void functionNineArgs(int32_t, void*, void*, void*, void*, void*, void*, void*, void*) { }

void testShuffleDoesntTrashCalleeSaves()
{
    Procedure proc;

    BasicBlock* root = proc.addBlock();
    BasicBlock* likely = proc.addBlock();
    BasicBlock* unlikely = proc.addBlock();

    RegisterSet regs = RegisterSet::allGPRs();
    regs.exclude(RegisterSet::stackRegisters());
    regs.exclude(RegisterSet::reservedHardwareRegisters());
    regs.exclude(RegisterSet::calleeSaveRegisters());
    regs.exclude(RegisterSet::argumentGPRS());

    unsigned i = 0;
    Vector<Value*> patches;
    for (Reg reg : regs) {
        ++i;
        PatchpointValue* patchpoint = root->appendNew<PatchpointValue>(proc, Int32, Origin());
        patchpoint->clobber(RegisterSet::macroScratchRegisters());
        RELEASE_ASSERT(reg.isGPR());
        patchpoint->resultConstraint = ValueRep::reg(reg.gpr());
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

    PatchpointValue* ptr = root->appendNew<PatchpointValue>(proc, Int64, Origin());
    ptr->clobber(RegisterSet::macroScratchRegisters());
    ptr->resultConstraint = ValueRep::reg(GPRInfo::regCS0);
    ptr->appendSomeRegister(arg1);
    ptr->setGenerator(
        [=] (CCallHelpers& jit, const StackmapGenerationParams& params) {
            AllowMacroScratchRegisterUsage allowScratch(jit);
            jit.move(params[1].gpr(), params[0].gpr());
        });

    Value* condition = root->appendNew<Value>(
        proc, Equal, Origin(), 
        ptr,
        root->appendNew<Const64Value>(proc, Origin(), 0));

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
        unlikely->appendNew<ConstPtrValue>(proc, Origin(), tagCFunctionPtr<void*>(functionNineArgs, B3CCallPtrTag)),
        constNumber, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8);

    PatchpointValue* voidPatch = unlikely->appendNew<PatchpointValue>(proc, Void, Origin());
    voidPatch->clobber(RegisterSet::macroScratchRegisters());
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
    CHECK(compileAndRun<int32_t>(proc, inputPtr) == 48);
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
        PatchpointValue* patchpoint = root->appendNew<PatchpointValue>(proc, Void, Origin());
        Value* const42 = root->appendNew<Const32Value>(proc, Origin(), 42);
        for (Reg reg : allRegs)
            patchpoint->append(const42, ValueRep::reg(reg));
        patchpoint->setGenerator([&] (CCallHelpers&, const StackmapGenerationParams&) { });
    }

    {
        PatchpointValue* patchpoint = root->appendNew<PatchpointValue>(proc, Void, Origin());
        Value* const10 = root->appendNew<Const32Value>(proc, Origin(), 10);
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
        patchpoint->resultConstraint = ValueRep::SomeEarlyRegister;
        patchpoint->setGenerator([&] (CCallHelpers&, const StackmapGenerationParams& params) {
            RELEASE_ASSERT(allRegs.contains(params[0].gpr()));
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

    Value* arg = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    root->appendNewControlValue(proc, Jump, Origin(), header);

    header->appendNewControlValue(
        proc, Branch, Origin(),
        header->appendNew<Value>(proc, Equal, Origin(),
            arg,
            header->appendNew<Const64Value>(proc, Origin(), 10)), header, loadBlock);

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
    RELEASE_ASSERT(!proc.calleeSaveRegisterAtOffsetList().size()); 
    invoke<void>(*code, static_cast<uint64_t>(55)); // Shouldn't crash dereferncing 55.
}

#endif // ENABLE(B3_JIT)
