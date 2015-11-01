/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#include "B3ArgumentRegValue.h"
#include "B3BasicBlockInlines.h"
#include "B3Const32Value.h"
#include "B3ConstPtrValue.h"
#include "B3ControlValue.h"
#include "B3Generate.h"
#include "B3MemoryValue.h"
#include "B3Procedure.h"
#include "B3StackSlotValue.h"
#include "B3UpsilonValue.h"
#include "B3ValueInlines.h"
#include "CCallHelpers.h"
#include "InitializeThreading.h"
#include "JSCInlines.h"
#include "LinkBuffer.h"
#include "VM.h"

// We don't have a NO_RETURN_DUE_TO_EXIT, nor should we. That's ridiculous.
static bool hiddenTruthBecauseNoReturnIsStupid() { return true; }

static void usage()
{
    dataLog("Usage: testb3 [<filter>]\n");
    if (hiddenTruthBecauseNoReturnIsStupid())
        exit(1);
}

#if ENABLE(B3_JIT)

using namespace JSC;
using namespace JSC::B3;

namespace {

// Nothing fancy for now; we just use the existing WTF assertion machinery.
#define CHECK(x) RELEASE_ASSERT(x)

VM* vm;

MacroAssemblerCodeRef compile(Procedure& procedure)
{
    CCallHelpers jit(vm);
    generate(procedure, jit);
    LinkBuffer linkBuffer(*vm, jit, nullptr);
    return FINALIZE_CODE(linkBuffer, ("testb3"));
}

template<typename T, typename... Arguments>
T invoke(const MacroAssemblerCodeRef& code, Arguments... arguments)
{
    T (*function)(Arguments...) = bitwise_cast<T(*)(Arguments...)>(code.code().executableAddress());
    return function(arguments...);
}

template<typename T, typename... Arguments>
T compileAndRun(Procedure& procedure, Arguments... arguments)
{
    return invoke<T>(compile(procedure), arguments...);
}

void test42()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* const42 = root->appendNew<Const32Value>(proc, Origin(), 42);
    root->appendNew<ControlValue>(proc, Return, Origin(), const42);

    CHECK(compileAndRun<int>(proc) == 42);
}

void testLoad42()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    int x = 42;
    root->appendNew<ControlValue>(
        proc, Return, Origin(),
        root->appendNew<MemoryValue>(
            proc, Load, Int32, Origin(),
            root->appendNew<ConstPtrValue>(proc, Origin(), &x)));

    CHECK(compileAndRun<int>(proc) == 42);
}

void testArg(int argument)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNew<ControlValue>(
        proc, Return, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));

    CHECK(compileAndRun<int>(proc, argument) == argument);
}

void testAddArgs(int a, int b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNew<ControlValue>(
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
    root->appendNew<ControlValue>(
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
    root->appendNew<ControlValue>(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, Add, Origin(),
            root->appendNew<Const64Value>(proc, Origin(), a),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)));

    CHECK(compileAndRun<int>(proc, b) == a + b);
}

void testAddArgs32(int a, int b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNew<ControlValue>(
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

void testSubArgs(int a, int b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNew<ControlValue>(
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
    root->appendNew<ControlValue>(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, Sub, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
            root->appendNew<Const64Value>(proc, Origin(), b)));

    CHECK(compileAndRun<int64_t>(proc, a) == a - b);
}

void testSubImmArg(int a, int b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNew<ControlValue>(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, Sub, Origin(),
            root->appendNew<Const64Value>(proc, Origin(), a),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)));

    CHECK(compileAndRun<int>(proc, b) == a - b);
}

void testSubArgs32(int a, int b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNew<ControlValue>(
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
    root->appendNew<ControlValue>(
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
    root->appendNew<ControlValue>(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, Sub, Origin(),
            root->appendNew<Const32Value>(proc, Origin(), a),
            root->appendNew<Value>(
                proc, Trunc, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0))));

    CHECK(compileAndRun<int>(proc, b) == a - b);
}

void testStore(int value)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    int slot = 0xbaadbeef;
    root->appendNew<MemoryValue>(
        proc, Store, Origin(),
        root->appendNew<Value>(
            proc, Trunc, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)),
        root->appendNew<ConstPtrValue>(proc, Origin(), &slot));
    root->appendNew<ControlValue>(
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
        root->appendNew<ConstPtrValue>(proc, Origin(), &slot));
    root->appendNew<ControlValue>(
        proc, Return, Origin(), root->appendNew<Const32Value>(proc, Origin(), 0));

    CHECK(!compileAndRun<int>(proc));
    CHECK(slot == value);
}

void testStoreConstantPtr(intptr_t value)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    intptr_t slot;
    if (is64Bit())
        slot = (static_cast<intptr_t>(0xbaadbeef) << 32) + static_cast<intptr_t>(0xbaadbeef);
    else
        slot = 0xbaadbeef;
    root->appendNew<MemoryValue>(
        proc, Store, Origin(),
        root->appendNew<ConstPtrValue>(proc, Origin(), value),
        root->appendNew<ConstPtrValue>(proc, Origin(), &slot));
    root->appendNew<ControlValue>(
        proc, Return, Origin(), root->appendNew<Const32Value>(proc, Origin(), 0));

    CHECK(!compileAndRun<int>(proc));
    CHECK(slot == value);
}

void testTrunc(int64_t value)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNew<ControlValue>(
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
    root->appendNew<ControlValue>(
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
    root->appendNew<ControlValue>(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, Add, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
            root->appendNew<ConstPtrValue>(proc, Origin(), 1)));

    CHECK(compileAndRun<intptr_t>(proc, value) == value + 1);
}

void testStoreAddLoad(int amount)
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
            root->appendNew<Const32Value>(proc, Origin(), amount)),
        slotPtr);
    root->appendNew<ControlValue>(
        proc, Return, Origin(),
        root->appendNew<Const32Value>(proc, Origin(), 0));

    CHECK(!compileAndRun<int>(proc));
    CHECK(slot == 37 + amount);
}

void testStoreSubLoad(int amount)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    int32_t startValue = std::numeric_limits<int32_t>::min();
    int32_t slot = startValue;
    ConstPtrValue* slotPtr = root->appendNew<ConstPtrValue>(proc, Origin(), &slot);
    root->appendNew<MemoryValue>(
        proc, Store, Origin(),
        root->appendNew<Value>(
            proc, Sub, Origin(),
            root->appendNew<MemoryValue>(proc, Load, Int32, Origin(), slotPtr),
            root->appendNew<Value>(
                proc, Trunc, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0))),
        slotPtr);
    root->appendNew<ControlValue>(
        proc, Return, Origin(),
        root->appendNew<Const32Value>(proc, Origin(), 0));

    CHECK(!compileAndRun<int>(proc, amount));
    CHECK(slot == startValue - amount);
}

void testStoreAddLoadInterference(int amount)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    int slot = 37;
    ConstPtrValue* slotPtr = root->appendNew<ConstPtrValue>(proc, Origin(), &slot);
    ArgumentRegValue* otherSlotPtr =
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    MemoryValue* load = root->appendNew<MemoryValue>(proc, Load, Int32, Origin(), slotPtr);
    root->appendNew<MemoryValue>(
        proc, Store, Origin(),
        root->appendNew<Const32Value>(proc, Origin(), 666),
        otherSlotPtr);
    root->appendNew<MemoryValue>(
        proc, Store, Origin(),
        root->appendNew<Value>(
            proc, Add, Origin(),
            load, root->appendNew<Const32Value>(proc, Origin(), amount)),
        slotPtr);
    root->appendNew<ControlValue>(
        proc, Return, Origin(),
        root->appendNew<Const32Value>(proc, Origin(), 0));

    CHECK(!compileAndRun<int>(proc, &slot));
    CHECK(slot == 37 + amount);
}

void testStoreAddAndLoad(int amount, int mask)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    int slot = 37;
    ConstPtrValue* slotPtr = root->appendNew<ConstPtrValue>(proc, Origin(), &slot);
    root->appendNew<MemoryValue>(
        proc, Store, Origin(),
        root->appendNew<Value>(
            proc, BitAnd, Origin(),
            root->appendNew<Value>(
                proc, Add, Origin(),
                root->appendNew<MemoryValue>(proc, Load, Int32, Origin(), slotPtr),
                root->appendNew<Const32Value>(proc, Origin(), amount)),
            root->appendNew<Const32Value>(proc, Origin(), mask)),
        slotPtr);
    root->appendNew<ControlValue>(
        proc, Return, Origin(),
        root->appendNew<Const32Value>(proc, Origin(), 0));

    CHECK(!compileAndRun<int>(proc));
    CHECK(slot == ((37 + amount) & mask));
}

void testAdd1Uncommuted(int value)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNew<ControlValue>(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, Add, Origin(),
            root->appendNew<Const32Value>(proc, Origin(), 1),
            root->appendNew<Value>(
                proc, Trunc, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0))));

    CHECK(compileAndRun<int>(proc, value) == value + 1);
}

void testLoadOffset()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    int array[] = { 1, 2 };
    ConstPtrValue* arrayPtr = root->appendNew<ConstPtrValue>(proc, Origin(), array);
    root->appendNew<ControlValue>(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, Add, Origin(),
            root->appendNew<MemoryValue>(proc, Load, Int32, Origin(), arrayPtr, 0),
            root->appendNew<MemoryValue>(proc, Load, Int32, Origin(), arrayPtr, sizeof(int))));

    CHECK(compileAndRun<int>(proc) == array[0] + array[1]);
}

void testLoadOffsetNotConstant()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    int array[] = { 1, 2 };
    Value* arrayPtr = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    root->appendNew<ControlValue>(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, Add, Origin(),
            root->appendNew<MemoryValue>(proc, Load, Int32, Origin(), arrayPtr, 0),
            root->appendNew<MemoryValue>(proc, Load, Int32, Origin(), arrayPtr, sizeof(int))));

    CHECK(compileAndRun<int>(proc, &array[0]) == array[0] + array[1]);
}

void testLoadOffsetUsingAdd()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    int array[] = { 1, 2 };
    ConstPtrValue* arrayPtr = root->appendNew<ConstPtrValue>(proc, Origin(), array);
    root->appendNew<ControlValue>(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, Add, Origin(),
            root->appendNew<MemoryValue>(
                proc, Load, Int32, Origin(),
                root->appendNew<Value>(
                    proc, Add, Origin(), arrayPtr,
                    root->appendNew<ConstPtrValue>(proc, Origin(), 0))),
            root->appendNew<MemoryValue>(
                proc, Load, Int32, Origin(),
                root->appendNew<Value>(
                    proc, Add, Origin(), arrayPtr,
                    root->appendNew<ConstPtrValue>(proc, Origin(), sizeof(int))))));
    
    CHECK(compileAndRun<int>(proc) == array[0] + array[1]);
}

void testLoadOffsetUsingAddInterference()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    int array[] = { 1, 2 };
    ConstPtrValue* arrayPtr = root->appendNew<ConstPtrValue>(proc, Origin(), array);
    ArgumentRegValue* otherArrayPtr =
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    Const32Value* theNumberOfTheBeast = root->appendNew<Const32Value>(proc, Origin(), 666);
    MemoryValue* left = root->appendNew<MemoryValue>(
        proc, Load, Int32, Origin(),
        root->appendNew<Value>(
            proc, Add, Origin(), arrayPtr,
            root->appendNew<ConstPtrValue>(proc, Origin(), 0)));
    MemoryValue* right = root->appendNew<MemoryValue>(
        proc, Load, Int32, Origin(),
        root->appendNew<Value>(
            proc, Add, Origin(), arrayPtr,
            root->appendNew<ConstPtrValue>(proc, Origin(), sizeof(int))));
    root->appendNew<MemoryValue>(
        proc, Store, Origin(), theNumberOfTheBeast, otherArrayPtr, 0);
    root->appendNew<MemoryValue>(
        proc, Store, Origin(), theNumberOfTheBeast, otherArrayPtr, sizeof(int));
    root->appendNew<ControlValue>(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, Add, Origin(), left, right));
    
    CHECK(compileAndRun<int>(proc, &array[0]) == 1 + 2);
    CHECK(array[0] == 666);
    CHECK(array[1] == 666);
}

void testLoadOffsetUsingAddNotConstant()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    int array[] = { 1, 2 };
    Value* arrayPtr = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    root->appendNew<ControlValue>(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, Add, Origin(),
            root->appendNew<MemoryValue>(
                proc, Load, Int32, Origin(),
                root->appendNew<Value>(
                    proc, Add, Origin(), arrayPtr,
                    root->appendNew<ConstPtrValue>(proc, Origin(), 0))),
            root->appendNew<MemoryValue>(
                proc, Load, Int32, Origin(),
                root->appendNew<Value>(
                    proc, Add, Origin(), arrayPtr,
                    root->appendNew<ConstPtrValue>(proc, Origin(), sizeof(int))))));
    
    CHECK(compileAndRun<int>(proc, &array[0]) == array[0] + array[1]);
}

void testFramePointer()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNew<ControlValue>(
        proc, Return, Origin(),
        root->appendNew<Value>(proc, FramePointer, Origin()));

    void* fp = compileAndRun<void*>(proc);
    CHECK(fp < &proc);
    CHECK(fp >= bitwise_cast<char*>(&proc) - 10000);
}

void testStackSlot()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNew<ControlValue>(
        proc, Return, Origin(),
        root->appendNew<StackSlotValue>(proc, Origin(), 1, StackSlotKind::Anonymous));

    void* stackSlot = compileAndRun<void*>(proc);
    CHECK(stackSlot < &proc);
    CHECK(stackSlot >= bitwise_cast<char*>(&proc) - 10000);
}

void testLoadFromFramePointer()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNew<ControlValue>(
        proc, Return, Origin(),
        root->appendNew<MemoryValue>(
            proc, Load, pointerType(), Origin(),
            root->appendNew<Value>(proc, FramePointer, Origin())));

    void* fp = compileAndRun<void*>(proc);
    void* myFP = __builtin_frame_address(0);
    CHECK(fp <= myFP);
    CHECK(fp >= bitwise_cast<char*>(myFP) - 10000);
}

void testStoreLoadStackSlot(int value)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();

    StackSlotValue* stack = root->appendNew<StackSlotValue>(
        proc, Origin(), sizeof(int), StackSlotKind::Anonymous);

    root->appendNew<MemoryValue>(
        proc, Store, Origin(),
        root->appendNew<Value>(
            proc, Trunc, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)),
        stack);
    
    root->appendNew<ControlValue>(
        proc, Return, Origin(),
        root->appendNew<MemoryValue>(proc, Load, Int32, Origin(), stack));

    CHECK(compileAndRun<int>(proc, value) == value);
}

void testBranch()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* thenCase = proc.addBlock();
    BasicBlock* elseCase = proc.addBlock();

    root->appendNew<ControlValue>(
        proc, Branch, Origin(),
        root->appendNew<Value>(
            proc, Trunc, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)),
        FrequentedBlock(thenCase), FrequentedBlock(elseCase));

    thenCase->appendNew<ControlValue>(
        proc, Return, Origin(),
        thenCase->appendNew<Const32Value>(proc, Origin(), 1));

    elseCase->appendNew<ControlValue>(
        proc, Return, Origin(),
        elseCase->appendNew<Const32Value>(proc, Origin(), 0));

    auto code = compile(proc);
    CHECK(invoke<int>(code, 42) == 1);
    CHECK(invoke<int>(code, 0) == 0);
}

void testBranchPtr()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* thenCase = proc.addBlock();
    BasicBlock* elseCase = proc.addBlock();

    root->appendNew<ControlValue>(
        proc, Branch, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
        FrequentedBlock(thenCase), FrequentedBlock(elseCase));

    thenCase->appendNew<ControlValue>(
        proc, Return, Origin(),
        thenCase->appendNew<Const32Value>(proc, Origin(), 1));

    elseCase->appendNew<ControlValue>(
        proc, Return, Origin(),
        elseCase->appendNew<Const32Value>(proc, Origin(), 0));

    auto code = compile(proc);
    CHECK(invoke<int>(code, static_cast<intptr_t>(42)) == 1);
    CHECK(invoke<int>(code, static_cast<intptr_t>(0)) == 0);
}

void testDiamond()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* thenCase = proc.addBlock();
    BasicBlock* elseCase = proc.addBlock();
    BasicBlock* done = proc.addBlock();

    root->appendNew<ControlValue>(
        proc, Branch, Origin(),
        root->appendNew<Value>(
            proc, Trunc, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)),
        FrequentedBlock(thenCase), FrequentedBlock(elseCase));

    UpsilonValue* thenResult = thenCase->appendNew<UpsilonValue>(
        proc, Origin(), thenCase->appendNew<Const32Value>(proc, Origin(), 1));
    thenCase->appendNew<ControlValue>(proc, Jump, Origin(), FrequentedBlock(done));

    UpsilonValue* elseResult = elseCase->appendNew<UpsilonValue>(
        proc, Origin(), elseCase->appendNew<Const32Value>(proc, Origin(), 0));
    elseCase->appendNew<ControlValue>(proc, Jump, Origin(), FrequentedBlock(done));

    Value* phi = done->appendNew<Value>(proc, Phi, Int32, Origin());
    thenResult->setPhi(phi);
    elseResult->setPhi(phi);
    done->appendNew<ControlValue>(proc, Return, Origin(), phi);

    auto code = compile(proc);
    CHECK(invoke<int>(code, 42) == 1);
    CHECK(invoke<int>(code, 0) == 0);
}

void testBranchNotEqual()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* thenCase = proc.addBlock();
    BasicBlock* elseCase = proc.addBlock();

    root->appendNew<ControlValue>(
        proc, Branch, Origin(),
        root->appendNew<Value>(
            proc, NotEqual, Origin(),
            root->appendNew<Value>(
                proc, Trunc, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)),
            root->appendNew<Const32Value>(proc, Origin(), 0)),
        FrequentedBlock(thenCase), FrequentedBlock(elseCase));

    thenCase->appendNew<ControlValue>(
        proc, Return, Origin(),
        thenCase->appendNew<Const32Value>(proc, Origin(), 1));

    elseCase->appendNew<ControlValue>(
        proc, Return, Origin(),
        elseCase->appendNew<Const32Value>(proc, Origin(), 0));

    auto code = compile(proc);
    CHECK(invoke<int>(code, 42) == 1);
    CHECK(invoke<int>(code, 0) == 0);
}

void testBranchNotEqualCommute()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* thenCase = proc.addBlock();
    BasicBlock* elseCase = proc.addBlock();

    root->appendNew<ControlValue>(
        proc, Branch, Origin(),
        root->appendNew<Value>(
            proc, NotEqual, Origin(),
            root->appendNew<Const32Value>(proc, Origin(), 0),
            root->appendNew<Value>(
                proc, Trunc, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0))),
        FrequentedBlock(thenCase), FrequentedBlock(elseCase));

    thenCase->appendNew<ControlValue>(
        proc, Return, Origin(),
        thenCase->appendNew<Const32Value>(proc, Origin(), 1));

    elseCase->appendNew<ControlValue>(
        proc, Return, Origin(),
        elseCase->appendNew<Const32Value>(proc, Origin(), 0));

    auto code = compile(proc);
    CHECK(invoke<int>(code, 42) == 1);
    CHECK(invoke<int>(code, 0) == 0);
}

void testBranchNotEqualNotEqual()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* thenCase = proc.addBlock();
    BasicBlock* elseCase = proc.addBlock();

    root->appendNew<ControlValue>(
        proc, Branch, Origin(),
        root->appendNew<Value>(
            proc, NotEqual, Origin(),
            root->appendNew<Value>(
                proc, NotEqual, Origin(),
                root->appendNew<Value>(
                    proc, Trunc, Origin(),
                    root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)),
                root->appendNew<Const32Value>(proc, Origin(), 0)),
            root->appendNew<Const32Value>(proc, Origin(), 0)),
        FrequentedBlock(thenCase), FrequentedBlock(elseCase));

    thenCase->appendNew<ControlValue>(
        proc, Return, Origin(),
        thenCase->appendNew<Const32Value>(proc, Origin(), 1));

    elseCase->appendNew<ControlValue>(
        proc, Return, Origin(),
        elseCase->appendNew<Const32Value>(proc, Origin(), 0));

    auto code = compile(proc);
    CHECK(invoke<int>(code, 42) == 1);
    CHECK(invoke<int>(code, 0) == 0);
}

void testBranchEqual()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* thenCase = proc.addBlock();
    BasicBlock* elseCase = proc.addBlock();

    root->appendNew<ControlValue>(
        proc, Branch, Origin(),
        root->appendNew<Value>(
            proc, Equal, Origin(),
            root->appendNew<Value>(
                proc, Trunc, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)),
            root->appendNew<Const32Value>(proc, Origin(), 0)),
        FrequentedBlock(thenCase), FrequentedBlock(elseCase));

    thenCase->appendNew<ControlValue>(
        proc, Return, Origin(),
        thenCase->appendNew<Const32Value>(proc, Origin(), 0));

    elseCase->appendNew<ControlValue>(
        proc, Return, Origin(),
        elseCase->appendNew<Const32Value>(proc, Origin(), 1));

    auto code = compile(proc);
    CHECK(invoke<int>(code, 42) == 1);
    CHECK(invoke<int>(code, 0) == 0);
}

void testBranchEqualEqual()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* thenCase = proc.addBlock();
    BasicBlock* elseCase = proc.addBlock();

    root->appendNew<ControlValue>(
        proc, Branch, Origin(),
        root->appendNew<Value>(
            proc, Equal, Origin(),
            root->appendNew<Value>(
                proc, Equal, Origin(),
                root->appendNew<Value>(
                    proc, Trunc, Origin(),
                    root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)),
                root->appendNew<Const32Value>(proc, Origin(), 0)),
            root->appendNew<Const32Value>(proc, Origin(), 0)),
        FrequentedBlock(thenCase), FrequentedBlock(elseCase));

    thenCase->appendNew<ControlValue>(
        proc, Return, Origin(),
        thenCase->appendNew<Const32Value>(proc, Origin(), 1));

    elseCase->appendNew<ControlValue>(
        proc, Return, Origin(),
        elseCase->appendNew<Const32Value>(proc, Origin(), 0));

    auto code = compile(proc);
    CHECK(invoke<int>(code, 42) == 1);
    CHECK(invoke<int>(code, 0) == 0);
}

void testBranchEqualCommute()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* thenCase = proc.addBlock();
    BasicBlock* elseCase = proc.addBlock();

    root->appendNew<ControlValue>(
        proc, Branch, Origin(),
        root->appendNew<Value>(
            proc, Equal, Origin(),
            root->appendNew<Const32Value>(proc, Origin(), 0),
            root->appendNew<Value>(
                proc, Trunc, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0))),
        FrequentedBlock(thenCase), FrequentedBlock(elseCase));

    thenCase->appendNew<ControlValue>(
        proc, Return, Origin(),
        thenCase->appendNew<Const32Value>(proc, Origin(), 0));

    elseCase->appendNew<ControlValue>(
        proc, Return, Origin(),
        elseCase->appendNew<Const32Value>(proc, Origin(), 1));

    auto code = compile(proc);
    CHECK(invoke<int>(code, 42) == 1);
    CHECK(invoke<int>(code, 0) == 0);
}

void testBranchEqualEqual1()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* thenCase = proc.addBlock();
    BasicBlock* elseCase = proc.addBlock();

    root->appendNew<ControlValue>(
        proc, Branch, Origin(),
        root->appendNew<Value>(
            proc, Equal, Origin(),
            root->appendNew<Value>(
                proc, Equal, Origin(),
                root->appendNew<Value>(
                    proc, Trunc, Origin(),
                    root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)),
                root->appendNew<Const32Value>(proc, Origin(), 0)),
            root->appendNew<Const32Value>(proc, Origin(), 1)),
        FrequentedBlock(thenCase), FrequentedBlock(elseCase));

    thenCase->appendNew<ControlValue>(
        proc, Return, Origin(),
        thenCase->appendNew<Const32Value>(proc, Origin(), 0));

    elseCase->appendNew<ControlValue>(
        proc, Return, Origin(),
        elseCase->appendNew<Const32Value>(proc, Origin(), 1));

    auto code = compile(proc);
    CHECK(invoke<int>(code, 42) == 1);
    CHECK(invoke<int>(code, 0) == 0);
}

void testBranchFold(int value)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* thenCase = proc.addBlock();
    BasicBlock* elseCase = proc.addBlock();

    root->appendNew<ControlValue>(
        proc, Branch, Origin(),
        root->appendNew<Const32Value>(proc, Origin(), value),
        FrequentedBlock(thenCase), FrequentedBlock(elseCase));

    thenCase->appendNew<ControlValue>(
        proc, Return, Origin(),
        thenCase->appendNew<Const32Value>(proc, Origin(), 1));

    elseCase->appendNew<ControlValue>(
        proc, Return, Origin(),
        elseCase->appendNew<Const32Value>(proc, Origin(), 0));

    CHECK(compileAndRun<int>(proc) == !!value);
}

void testDiamondFold(int value)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* thenCase = proc.addBlock();
    BasicBlock* elseCase = proc.addBlock();
    BasicBlock* done = proc.addBlock();

    root->appendNew<ControlValue>(
        proc, Branch, Origin(),
        root->appendNew<Const32Value>(proc, Origin(), value),
        FrequentedBlock(thenCase), FrequentedBlock(elseCase));

    UpsilonValue* thenResult = thenCase->appendNew<UpsilonValue>(
        proc, Origin(), thenCase->appendNew<Const32Value>(proc, Origin(), 1));
    thenCase->appendNew<ControlValue>(proc, Jump, Origin(), FrequentedBlock(done));

    UpsilonValue* elseResult = elseCase->appendNew<UpsilonValue>(
        proc, Origin(), elseCase->appendNew<Const32Value>(proc, Origin(), 0));
    elseCase->appendNew<ControlValue>(proc, Jump, Origin(), FrequentedBlock(done));

    Value* phi = done->appendNew<Value>(proc, Phi, Int32, Origin());
    thenResult->setPhi(phi);
    elseResult->setPhi(phi);
    done->appendNew<ControlValue>(proc, Return, Origin(), phi);

    CHECK(compileAndRun<int>(proc) == !!value);
}

void testBranchNotEqualFoldPtr(intptr_t value)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* thenCase = proc.addBlock();
    BasicBlock* elseCase = proc.addBlock();

    root->appendNew<ControlValue>(
        proc, Branch, Origin(),
        root->appendNew<Value>(
            proc, NotEqual, Origin(),
            root->appendNew<ConstPtrValue>(proc, Origin(), value),
            root->appendNew<ConstPtrValue>(proc, Origin(), 0)),
        FrequentedBlock(thenCase), FrequentedBlock(elseCase));

    thenCase->appendNew<ControlValue>(
        proc, Return, Origin(),
        thenCase->appendNew<Const32Value>(proc, Origin(), 1));

    elseCase->appendNew<ControlValue>(
        proc, Return, Origin(),
        elseCase->appendNew<Const32Value>(proc, Origin(), 0));

    CHECK(compileAndRun<int>(proc) == !!value);
}

void testBranchEqualFoldPtr(intptr_t value)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* thenCase = proc.addBlock();
    BasicBlock* elseCase = proc.addBlock();

    root->appendNew<ControlValue>(
        proc, Branch, Origin(),
        root->appendNew<Value>(
            proc, Equal, Origin(),
            root->appendNew<ConstPtrValue>(proc, Origin(), value),
            root->appendNew<ConstPtrValue>(proc, Origin(), 0)),
        FrequentedBlock(thenCase), FrequentedBlock(elseCase));

    thenCase->appendNew<ControlValue>(
        proc, Return, Origin(),
        thenCase->appendNew<Const32Value>(proc, Origin(), 1));

    elseCase->appendNew<ControlValue>(
        proc, Return, Origin(),
        elseCase->appendNew<Const32Value>(proc, Origin(), 0));

    CHECK(compileAndRun<int>(proc) == !value);
}

void testComplex(unsigned numVars, unsigned numConstructs)
{
    double before = monotonicallyIncreasingTimeMS();
    
    Procedure proc;
    BasicBlock* current = proc.addBlock();

    Const32Value* one = current->appendNew<Const32Value>(proc, Origin(), 1);

    Vector<int32_t> varSlots;
    for (unsigned i = numVars; i--;)
        varSlots.append(i);

    Vector<Value*> vars;
    for (int32_t& varSlot : varSlots) {
        Value* varSlotPtr = current->appendNew<ConstPtrValue>(proc, Origin(), &varSlot);
        vars.append(current->appendNew<MemoryValue>(proc, Load, Int32, Origin(), varSlotPtr));
    }

    for (unsigned i = 0; i < numConstructs; ++i) {
        if (i & 1) {
            // Control flow diamond.
            unsigned predicateVarIndex = (i >> 1) % numVars;
            unsigned thenIncVarIndex = ((i >> 1) + 1) % numVars;
            unsigned elseIncVarIndex = ((i >> 1) + 2) % numVars;

            BasicBlock* thenBlock = proc.addBlock();
            BasicBlock* elseBlock = proc.addBlock();
            BasicBlock* continuation = proc.addBlock();

            current->appendNew<ControlValue>(
                proc, Branch, Origin(), vars[predicateVarIndex],
                FrequentedBlock(thenBlock), FrequentedBlock(elseBlock));

            UpsilonValue* thenThenResult = thenBlock->appendNew<UpsilonValue>(
                proc, Origin(),
                thenBlock->appendNew<Value>(proc, Add, Origin(), vars[thenIncVarIndex], one));
            UpsilonValue* thenElseResult = thenBlock->appendNew<UpsilonValue>(
                proc, Origin(), vars[elseIncVarIndex]);
            thenBlock->appendNew<ControlValue>(proc, Jump, Origin(), FrequentedBlock(continuation));

            UpsilonValue* elseElseResult = elseBlock->appendNew<UpsilonValue>(
                proc, Origin(),
                elseBlock->appendNew<Value>(proc, Add, Origin(), vars[elseIncVarIndex], one));
            UpsilonValue* elseThenResult = elseBlock->appendNew<UpsilonValue>(
                proc, Origin(), vars[thenIncVarIndex]);
            elseBlock->appendNew<ControlValue>(proc, Jump, Origin(), FrequentedBlock(continuation));

            Value* thenPhi = continuation->appendNew<Value>(proc, Phi, Int32, Origin());
            thenThenResult->setPhi(thenPhi);
            elseThenResult->setPhi(thenPhi);
            vars[thenIncVarIndex] = thenPhi;
            
            Value* elsePhi = continuation->appendNew<Value>(proc, Phi, Int32, Origin());
            thenElseResult->setPhi(elsePhi);
            elseElseResult->setPhi(elsePhi);
            vars[elseIncVarIndex] = thenPhi;
            
            current = continuation;
        } else {
            // Loop.

            BasicBlock* loopEntry = proc.addBlock();
            BasicBlock* loopReentry = proc.addBlock();
            BasicBlock* loopBody = proc.addBlock();
            BasicBlock* loopExit = proc.addBlock();
            BasicBlock* loopSkip = proc.addBlock();
            BasicBlock* continuation = proc.addBlock();
            
            Value* startIndex = vars[(i >> 1) % numVars];
            Value* startSum = current->appendNew<Const32Value>(proc, Origin(), 0);
            current->appendNew<ControlValue>(
                proc, Branch, Origin(), startIndex,
                FrequentedBlock(loopEntry), FrequentedBlock(loopSkip));

            UpsilonValue* startIndexForBody = loopEntry->appendNew<UpsilonValue>(
                proc, Origin(), startIndex);
            UpsilonValue* startSumForBody = loopEntry->appendNew<UpsilonValue>(
                proc, Origin(), startSum);
            loopEntry->appendNew<ControlValue>(proc, Jump, Origin(), FrequentedBlock(loopBody));

            Value* bodyIndex = loopBody->appendNew<Value>(proc, Phi, Int32, Origin());
            startIndexForBody->setPhi(bodyIndex);
            Value* bodySum = loopBody->appendNew<Value>(proc, Phi, Int32, Origin());
            startSumForBody->setPhi(bodySum);
            Value* newBodyIndex = loopBody->appendNew<Value>(proc, Sub, Origin(), bodyIndex, one);
            Value* newBodySum = loopBody->appendNew<Value>(
                proc, Add, Origin(),
                bodySum,
                loopBody->appendNew<MemoryValue>(
                    proc, Load, Int32, Origin(),
                    loopBody->appendNew<Value>(
                        proc, Add, Origin(),
                        loopBody->appendNew<ConstPtrValue>(proc, Origin(), varSlots.data()),
                        loopBody->appendNew<Value>(
                            proc, Shl, Origin(),
                            loopBody->appendNew<Value>(
                                proc, ZExt32, Origin(),
                                loopBody->appendNew<Value>(
                                    proc, BitAnd, Origin(),
                                    newBodyIndex,
                                    loopBody->appendNew<Const32Value>(
                                        proc, Origin(), numVars - 1))),
                            loopBody->appendNew<Const32Value>(proc, Origin(), 2)))));
            loopBody->appendNew<ControlValue>(
                proc, Branch, Origin(), newBodyIndex,
                FrequentedBlock(loopReentry), FrequentedBlock(loopExit));

            loopReentry->appendNew<UpsilonValue>(proc, Origin(), newBodyIndex, bodyIndex);
            loopReentry->appendNew<UpsilonValue>(proc, Origin(), newBodySum, bodySum);
            loopReentry->appendNew<ControlValue>(proc, Jump, Origin(), FrequentedBlock(loopBody));

            UpsilonValue* exitSum = loopExit->appendNew<UpsilonValue>(proc, Origin(), newBodySum);
            loopExit->appendNew<ControlValue>(proc, Jump, Origin(), FrequentedBlock(continuation));

            UpsilonValue* skipSum = loopSkip->appendNew<UpsilonValue>(proc, Origin(), startSum);
            loopSkip->appendNew<ControlValue>(proc, Jump, Origin(), FrequentedBlock(continuation));

            Value* finalSum = continuation->appendNew<Value>(proc, Phi, Int32, Origin());
            exitSum->setPhi(finalSum);
            skipSum->setPhi(finalSum);

            current = continuation;
            vars[((i >> 1) + 1) % numVars] = finalSum;
        }
    }

    current->appendNew<ControlValue>(proc, Return, Origin(), vars[0]);

    compile(proc);

    double after = monotonicallyIncreasingTimeMS();
    dataLog("    That took ", after - before, " ms.\n");
}

#define RUN(test) do {                          \
        if (!shouldRun(#test))                  \
            break;                              \
        dataLog(#test ":\n");                   \
        test;                                   \
        dataLog("    OK!\n");                   \
        didRun++;                               \
    } while (false);

void run(const char* filter)
{
    JSC::initializeThreading();
    vm = &VM::create(LargeHeap).leakRef();

    auto shouldRun = [&] (const char* testName) -> bool {
        return !!strcasestr(testName, filter);
    };
    unsigned didRun = 0;

    RUN(test42());
    RUN(testLoad42());
    RUN(testArg(43));

    RUN(testAddArgs(1, 1));
    RUN(testAddArgs(1, 2));
    RUN(testAddArgImm(1, 2));
    RUN(testAddArgImm(0, 2));
    RUN(testAddArgImm(1, 0));
    RUN(testAddImmArg(1, 2));
    RUN(testAddImmArg(0, 2));
    RUN(testAddImmArg(1, 0));
    RUN(testAddArgs32(1, 1));
    RUN(testAddArgs32(1, 2));

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

    RUN(testStore(44));
    RUN(testStoreConstant(49));
    RUN(testStoreConstantPtr(49));
    RUN(testTrunc((static_cast<int64_t>(1) << 40) + 42));
    RUN(testAdd1(45));
    RUN(testAdd1Ptr(51));
    RUN(testAdd1Ptr(bitwise_cast<intptr_t>(vm)));
    RUN(testStoreAddLoad(46));
    RUN(testStoreSubLoad(46));
    RUN(testStoreAddLoadInterference(52));
    RUN(testStoreAddAndLoad(47, 0xffff));
    RUN(testStoreAddAndLoad(470000, 0xffff));
    RUN(testAdd1Uncommuted(48));
    RUN(testLoadOffset());
    RUN(testLoadOffsetNotConstant());
    RUN(testLoadOffsetUsingAdd());
    RUN(testLoadOffsetUsingAddInterference());
    RUN(testLoadOffsetUsingAddNotConstant());
    RUN(testFramePointer());
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
    RUN(testBranchFold(42));
    RUN(testBranchFold(0));
    RUN(testDiamondFold(42));
    RUN(testDiamondFold(0));
    RUN(testBranchNotEqualFoldPtr(42));
    RUN(testBranchNotEqualFoldPtr(0));
    RUN(testBranchEqualFoldPtr(42));
    RUN(testBranchEqualFoldPtr(0));

    RUN(testComplex(64, 128));
    RUN(testComplex(64, 256));
    RUN(testComplex(64, 384));
    RUN(testComplex(4, 128));
    RUN(testComplex(4, 256));
    RUN(testComplex(4, 384));

    if (!didRun)
        usage();
}

} // anonymous namespace

#else // ENABLE(B3_JIT)

static void run(const char*)
{
    dataLog("B3 JIT is not enabled.\n");
}

#endif // ENABLE(B3_JIT)

int main(int argc, char** argv)
{
    const char* filter = "";
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
    
    run(filter);
    return 0;
}

