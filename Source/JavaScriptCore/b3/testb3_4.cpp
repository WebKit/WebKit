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

void testStoreRelAddLoadAcq32(int amount)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    int slot = 37;
    ConstPtrValue* slotPtr = root->appendNew<ConstPtrValue>(proc, Origin(), &slot);
    root->appendNew<MemoryValue>(
        proc, Store, Origin(),
        root->appendNew<Value>(
            proc, Add, Origin(),
            root->appendNew<MemoryValue>(
                proc, Load, Int32, Origin(), slotPtr, 0, HeapRange(42), HeapRange(42)),
            root->appendNew<Value>(
                proc, Trunc, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0))),
        slotPtr, 0, HeapRange(42), HeapRange(42));
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Const32Value>(proc, Origin(), 0));

    auto code = compileProc(proc);
    if (isARM64()) {
        checkUsesInstruction(*code, "lda");
        checkUsesInstruction(*code, "stl");
    }
    if (isX86())
        checkUsesInstruction(*code, "xchg");
    CHECK(!invoke<int>(*code, amount));
    CHECK(slot == 37 + amount);
}

void testStoreAddLoadImm32(int amount)
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
        slotPtr, 0);
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Const32Value>(proc, Origin(), 0));

    CHECK(!compileAndRun<int>(proc));
    CHECK(slot == 37 + amount);
}

void testStoreAddLoad8(int amount, B3::Opcode loadOpcode)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    int8_t slot = 37;
    ConstPtrValue* slotPtr = root->appendNew<ConstPtrValue>(proc, Origin(), &slot);
    root->appendNew<MemoryValue>(
        proc, Store8, Origin(),
        root->appendNew<Value>(
            proc, Add, Origin(),
            root->appendNew<MemoryValue>(proc, loadOpcode, Origin(), slotPtr),
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

void testStoreRelAddLoadAcq8(int amount, B3::Opcode loadOpcode)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    int8_t slot = 37;
    ConstPtrValue* slotPtr = root->appendNew<ConstPtrValue>(proc, Origin(), &slot);
    root->appendNew<MemoryValue>(
        proc, Store8, Origin(),
        root->appendNew<Value>(
            proc, Add, Origin(),
            root->appendNew<MemoryValue>(
                proc, loadOpcode, Origin(), slotPtr, 0, HeapRange(42), HeapRange(42)),
            root->appendNew<Value>(
                proc, Trunc, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0))),
        slotPtr, 0, HeapRange(42), HeapRange(42));
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Const32Value>(proc, Origin(), 0));

    auto code = compileProc(proc);
    if (isARM64()) {
        checkUsesInstruction(*code, "lda");
        checkUsesInstruction(*code, "stl");
    }
    if (isX86())
        checkUsesInstruction(*code, "xchg");
    CHECK(!invoke<int>(*code, amount));
    CHECK(slot == 37 + amount);
}

void testStoreRelAddFenceLoadAcq8(int amount, B3::Opcode loadOpcode)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    int8_t slot = 37;
    ConstPtrValue* slotPtr = root->appendNew<ConstPtrValue>(proc, Origin(), &slot);
    Value* loadedValue = root->appendNew<MemoryValue>(
        proc, loadOpcode, Origin(), slotPtr, 0, HeapRange(42), HeapRange(42));
    PatchpointValue* patchpoint = root->appendNew<PatchpointValue>(proc, Void, Origin());
    patchpoint->clobber(RegisterSet::macroScratchRegisters());
    patchpoint->setGenerator(
        [&] (CCallHelpers& jit, const StackmapGenerationParams&) {
            AllowMacroScratchRegisterUsage allowScratch(jit);
            jit.store8(CCallHelpers::TrustedImm32(0xbeef), &slot);
        });
    patchpoint->effects = Effects::none();
    patchpoint->effects.fence = true;
    root->appendNew<MemoryValue>(
        proc, Store8, Origin(),
        root->appendNew<Value>(
            proc, Add, Origin(),
            loadedValue,
            root->appendNew<Value>(
                proc, Trunc, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0))),
        slotPtr, 0, HeapRange(42), HeapRange(42));
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Const32Value>(proc, Origin(), 0));

    auto code = compileProc(proc);
    if (isARM64()) {
        checkUsesInstruction(*code, "lda");
        checkUsesInstruction(*code, "stl");
    }
    if (isX86())
        checkUsesInstruction(*code, "xchg");
    CHECK(!invoke<int>(*code, amount));
    CHECK(slot == 37 + amount);
}

void testStoreAddLoadImm8(int amount, B3::Opcode loadOpcode)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    int8_t slot = 37;
    ConstPtrValue* slotPtr = root->appendNew<ConstPtrValue>(proc, Origin(), &slot);
    root->appendNew<MemoryValue>(
        proc, Store8, Origin(),
        root->appendNew<Value>(
            proc, Add, Origin(),
            root->appendNew<MemoryValue>(proc, loadOpcode, Origin(), slotPtr),
            root->appendNew<Const32Value>(proc, Origin(), amount)),
        slotPtr, 0);
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Const32Value>(proc, Origin(), 0));

    CHECK(!compileAndRun<int>(proc));
    CHECK(slot == 37 + amount);
}

void testStoreAddLoad16(int amount, B3::Opcode loadOpcode)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    int16_t slot = 37;
    ConstPtrValue* slotPtr = root->appendNew<ConstPtrValue>(proc, Origin(), &slot);
    root->appendNew<MemoryValue>(
        proc, Store16, Origin(),
        root->appendNew<Value>(
            proc, Add, Origin(),
            root->appendNew<MemoryValue>(proc, loadOpcode, Origin(), slotPtr),
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

void testStoreRelAddLoadAcq16(int amount, B3::Opcode loadOpcode)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    int16_t slot = 37;
    ConstPtrValue* slotPtr = root->appendNew<ConstPtrValue>(proc, Origin(), &slot);
    root->appendNew<MemoryValue>(
        proc, Store16, Origin(),
        root->appendNew<Value>(
            proc, Add, Origin(),
            root->appendNew<MemoryValue>(
                proc, loadOpcode, Origin(), slotPtr, 0, HeapRange(42), HeapRange(42)),
            root->appendNew<Value>(
                proc, Trunc, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0))),
        slotPtr, 0, HeapRange(42), HeapRange(42));
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Const32Value>(proc, Origin(), 0));

    auto code = compileProc(proc);
    if (isARM64()) {
        checkUsesInstruction(*code, "lda");
        checkUsesInstruction(*code, "stl");
    }
    if (isX86())
        checkUsesInstruction(*code, "xchg");
    CHECK(!invoke<int>(*code, amount));
    CHECK(slot == 37 + amount);
}

void testStoreAddLoadImm16(int amount, B3::Opcode loadOpcode)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    int16_t slot = 37;
    ConstPtrValue* slotPtr = root->appendNew<ConstPtrValue>(proc, Origin(), &slot);
    root->appendNew<MemoryValue>(
        proc, Store16, Origin(),
        root->appendNew<Value>(
            proc, Add, Origin(),
            root->appendNew<MemoryValue>(proc, loadOpcode, Origin(), slotPtr),
            root->appendNew<Const32Value>(proc, Origin(), amount)),
        slotPtr, 0);
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Const32Value>(proc, Origin(), 0));

    CHECK(!compileAndRun<int>(proc));
    CHECK(slot == 37 + amount);
}

void testStoreAddLoad64(int amount)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    int64_t slot = 37000000000ll;
    ConstPtrValue* slotPtr = root->appendNew<ConstPtrValue>(proc, Origin(), &slot);
    root->appendNew<MemoryValue>(
        proc, Store, Origin(),
        root->appendNew<Value>(
            proc, Add, Origin(),
            root->appendNew<MemoryValue>(proc, Load, Int64, Origin(), slotPtr),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)),
        slotPtr, 0);
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Const32Value>(proc, Origin(), 0));

    CHECK(!compileAndRun<int>(proc, amount));
    CHECK(slot == 37000000000ll + amount);
}

void testStoreRelAddLoadAcq64(int amount)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    int64_t slot = 37000000000ll;
    ConstPtrValue* slotPtr = root->appendNew<ConstPtrValue>(proc, Origin(), &slot);
    root->appendNew<MemoryValue>(
        proc, Store, Origin(),
        root->appendNew<Value>(
            proc, Add, Origin(),
            root->appendNew<MemoryValue>(
                proc, Load, Int64, Origin(), slotPtr, 0, HeapRange(42), HeapRange(42)),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)),
        slotPtr, 0, HeapRange(42), HeapRange(42));
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Const32Value>(proc, Origin(), 0));

    auto code = compileProc(proc);
    if (isARM64()) {
        checkUsesInstruction(*code, "lda");
        checkUsesInstruction(*code, "stl");
    }
    if (isX86())
        checkUsesInstruction(*code, "xchg");
    CHECK(!invoke<int>(*code, amount));
    CHECK(slot == 37000000000ll + amount);
}

void testStoreAddLoadImm64(int64_t amount)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    int64_t slot = 370000000000ll;
    ConstPtrValue* slotPtr = root->appendNew<ConstPtrValue>(proc, Origin(), &slot);
    root->appendNew<MemoryValue>(
        proc, Store, Origin(),
        root->appendNew<Value>(
            proc, Add, Origin(),
            root->appendNew<MemoryValue>(proc, Load, Int64, Origin(), slotPtr),
            root->appendNew<Const64Value>(proc, Origin(), amount)),
        slotPtr, 0);
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Const32Value>(proc, Origin(), 0));

    CHECK(!compileAndRun<int>(proc));
    CHECK(slot == 370000000000ll + amount);
}

void testStoreAddLoad32Index(int amount)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    int slot = 37;
    int* ptr = &slot;
    intptr_t zero = 0;
    Value* slotPtr = root->appendNew<Value>(
        proc, Add, Origin(),
        root->appendNew<MemoryValue>(
            proc, Load, pointerType(), Origin(),
            root->appendNew<ConstPtrValue>(proc, Origin(), &ptr)),
        root->appendNew<MemoryValue>(
            proc, Load, pointerType(), Origin(),
            root->appendNew<ConstPtrValue>(proc, Origin(), &zero)));
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

void testStoreAddLoadImm32Index(int amount)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    int slot = 37;
    int* ptr = &slot;
    intptr_t zero = 0;
    Value* slotPtr = root->appendNew<Value>(
        proc, Add, Origin(),
        root->appendNew<MemoryValue>(
            proc, Load, pointerType(), Origin(),
            root->appendNew<ConstPtrValue>(proc, Origin(), &ptr)),
        root->appendNew<MemoryValue>(
            proc, Load, pointerType(), Origin(),
            root->appendNew<ConstPtrValue>(proc, Origin(), &zero)));
    root->appendNew<MemoryValue>(
        proc, Store, Origin(),
        root->appendNew<Value>(
            proc, Add, Origin(),
            root->appendNew<MemoryValue>(proc, Load, Int32, Origin(), slotPtr),
            root->appendNew<Const32Value>(proc, Origin(), amount)),
        slotPtr, 0);
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Const32Value>(proc, Origin(), 0));

    CHECK(!compileAndRun<int>(proc));
    CHECK(slot == 37 + amount);
}

void testStoreAddLoad8Index(int amount, B3::Opcode loadOpcode)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    int8_t slot = 37;
    int8_t* ptr = &slot;
    intptr_t zero = 0;
    Value* slotPtr = root->appendNew<Value>(
        proc, Add, Origin(),
        root->appendNew<MemoryValue>(
            proc, Load, pointerType(), Origin(),
            root->appendNew<ConstPtrValue>(proc, Origin(), &ptr)),
        root->appendNew<MemoryValue>(
            proc, Load, pointerType(), Origin(),
            root->appendNew<ConstPtrValue>(proc, Origin(), &zero)));
    root->appendNew<MemoryValue>(
        proc, Store8, Origin(),
        root->appendNew<Value>(
            proc, Add, Origin(),
            root->appendNew<MemoryValue>(proc, loadOpcode, Origin(), slotPtr),
            root->appendNew<Value>(
                proc, Trunc, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0))),
        slotPtr);
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Const32Value>(proc, Origin(), 0));

    CHECK(!compileAndRun<int>(proc, amount));
    CHECK(slot == 37 + amount);
}

void testStoreAddLoadImm8Index(int amount, B3::Opcode loadOpcode)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    int8_t slot = 37;
    int8_t* ptr = &slot;
    intptr_t zero = 0;
    Value* slotPtr = root->appendNew<Value>(
        proc, Add, Origin(),
        root->appendNew<MemoryValue>(
            proc, Load, pointerType(), Origin(),
            root->appendNew<ConstPtrValue>(proc, Origin(), &ptr)),
        root->appendNew<MemoryValue>(
            proc, Load, pointerType(), Origin(),
            root->appendNew<ConstPtrValue>(proc, Origin(), &zero)));
    root->appendNew<MemoryValue>(
        proc, Store8, Origin(),
        root->appendNew<Value>(
            proc, Add, Origin(),
            root->appendNew<MemoryValue>(proc, loadOpcode, Origin(), slotPtr),
            root->appendNew<Const32Value>(proc, Origin(), amount)),
        slotPtr);
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Const32Value>(proc, Origin(), 0));

    CHECK(!compileAndRun<int>(proc));
    CHECK(slot == 37 + amount);
}

void testStoreAddLoad16Index(int amount, B3::Opcode loadOpcode)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    int16_t slot = 37;
    int16_t* ptr = &slot;
    intptr_t zero = 0;
    Value* slotPtr = root->appendNew<Value>(
        proc, Add, Origin(),
        root->appendNew<MemoryValue>(
            proc, Load, pointerType(), Origin(),
            root->appendNew<ConstPtrValue>(proc, Origin(), &ptr)),
        root->appendNew<MemoryValue>(
            proc, Load, pointerType(), Origin(),
            root->appendNew<ConstPtrValue>(proc, Origin(), &zero)));
    root->appendNew<MemoryValue>(
        proc, Store16, Origin(),
        root->appendNew<Value>(
            proc, Add, Origin(),
            root->appendNew<MemoryValue>(proc, loadOpcode, Origin(), slotPtr),
            root->appendNew<Value>(
                proc, Trunc, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0))),
        slotPtr);
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Const32Value>(proc, Origin(), 0));

    CHECK(!compileAndRun<int>(proc, amount));
    CHECK(slot == 37 + amount);
}

void testStoreAddLoadImm16Index(int amount, B3::Opcode loadOpcode)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    int16_t slot = 37;
    int16_t* ptr = &slot;
    intptr_t zero = 0;
    Value* slotPtr = root->appendNew<Value>(
        proc, Add, Origin(),
        root->appendNew<MemoryValue>(
            proc, Load, pointerType(), Origin(),
            root->appendNew<ConstPtrValue>(proc, Origin(), &ptr)),
        root->appendNew<MemoryValue>(
            proc, Load, pointerType(), Origin(),
            root->appendNew<ConstPtrValue>(proc, Origin(), &zero)));
    root->appendNew<MemoryValue>(
        proc, Store16, Origin(),
        root->appendNew<Value>(
            proc, Add, Origin(),
            root->appendNew<MemoryValue>(proc, loadOpcode, Origin(), slotPtr),
            root->appendNew<Const32Value>(proc, Origin(), amount)),
        slotPtr);
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Const32Value>(proc, Origin(), 0));

    CHECK(!compileAndRun<int>(proc));
    CHECK(slot == 37 + amount);
}

void testStoreAddLoad64Index(int amount)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    int64_t slot = 37000000000ll;
    int64_t* ptr = &slot;
    intptr_t zero = 0;
    Value* slotPtr = root->appendNew<Value>(
        proc, Add, Origin(),
        root->appendNew<MemoryValue>(
            proc, Load, pointerType(), Origin(),
            root->appendNew<ConstPtrValue>(proc, Origin(), &ptr)),
        root->appendNew<MemoryValue>(
            proc, Load, pointerType(), Origin(),
            root->appendNew<ConstPtrValue>(proc, Origin(), &zero)));
    root->appendNew<MemoryValue>(
        proc, Store, Origin(),
        root->appendNew<Value>(
            proc, Add, Origin(),
            root->appendNew<MemoryValue>(proc, Load, Int64, Origin(), slotPtr),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)),
        slotPtr, 0);
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Const32Value>(proc, Origin(), 0));

    CHECK(!compileAndRun<int>(proc, amount));
    CHECK(slot == 37000000000ll + amount);
}

void testStoreAddLoadImm64Index(int64_t amount)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    int64_t slot = 370000000000ll;
    int64_t* ptr = &slot;
    intptr_t zero = 0;
    Value* slotPtr = root->appendNew<Value>(
        proc, Add, Origin(),
        root->appendNew<MemoryValue>(
            proc, Load, pointerType(), Origin(),
            root->appendNew<ConstPtrValue>(proc, Origin(), &ptr)),
        root->appendNew<MemoryValue>(
            proc, Load, pointerType(), Origin(),
            root->appendNew<ConstPtrValue>(proc, Origin(), &zero)));
    root->appendNew<MemoryValue>(
        proc, Store, Origin(),
        root->appendNew<Value>(
            proc, Add, Origin(),
            root->appendNew<MemoryValue>(proc, Load, Int64, Origin(), slotPtr),
            root->appendNew<Const64Value>(proc, Origin(), amount)),
        slotPtr, 0);
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Const32Value>(proc, Origin(), 0));

    CHECK(!compileAndRun<int>(proc));
    CHECK(slot == 370000000000ll + amount);
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
        slotPtr, 0);
    root->appendNewControlValue(
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
        otherSlotPtr, 0);
    root->appendNew<MemoryValue>(
        proc, Store, Origin(),
        root->appendNew<Value>(
            proc, Add, Origin(),
            load, root->appendNew<Const32Value>(proc, Origin(), amount)),
        slotPtr, 0);
    root->appendNewControlValue(
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
        slotPtr, 0);
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Const32Value>(proc, Origin(), 0));

    CHECK(!compileAndRun<int>(proc));
    CHECK(slot == ((37 + amount) & mask));
}

void testStoreNegLoad32(int32_t value)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();

    int32_t slot = value;

    ConstPtrValue* slotPtr = root->appendNew<ConstPtrValue>(proc, Origin(), &slot);

    root->appendNew<MemoryValue>(
        proc, Store, Origin(),
        root->appendNew<Value>(
            proc, Sub, Origin(),
            root->appendNew<Const32Value>(proc, Origin(), 0),
            root->appendNew<MemoryValue>(proc, Load, Int32, Origin(), slotPtr)),
        slotPtr, 0);

    root->appendNewControlValue(
        proc, Return, Origin(), root->appendNew<Const32Value>(proc, Origin(), 0));

    CHECK(!compileAndRun<int32_t>(proc));
    CHECK(slot == -value);
}

void testStoreNegLoadPtr(intptr_t value)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();

    intptr_t slot = value;

    ConstPtrValue* slotPtr = root->appendNew<ConstPtrValue>(proc, Origin(), &slot);

    root->appendNew<MemoryValue>(
        proc, Store, Origin(),
        root->appendNew<Value>(
            proc, Sub, Origin(),
            root->appendNew<ConstPtrValue>(proc, Origin(), 0),
            root->appendNew<MemoryValue>(proc, Load, pointerType(), Origin(), slotPtr)),
        slotPtr, 0);

    root->appendNewControlValue(
        proc, Return, Origin(), root->appendNew<Const32Value>(proc, Origin(), 0));

    CHECK(!compileAndRun<int32_t>(proc));
    CHECK(slot == -value);
}

void testAdd1Uncommuted(int value)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
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
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, Add, Origin(),
            root->appendNew<MemoryValue>(proc, Load, Int32, Origin(), arrayPtr, 0),
            root->appendNew<MemoryValue>(proc, Load, Int32, Origin(), arrayPtr, static_cast<int32_t>(sizeof(int)))));

    CHECK(compileAndRun<int>(proc) == array[0] + array[1]);
}

void testLoadOffsetNotConstant()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    int array[] = { 1, 2 };
    Value* arrayPtr = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, Add, Origin(),
            root->appendNew<MemoryValue>(proc, Load, Int32, Origin(), arrayPtr, 0),
            root->appendNew<MemoryValue>(proc, Load, Int32, Origin(), arrayPtr, static_cast<int32_t>(sizeof(int)))));

    CHECK(compileAndRun<int>(proc, &array[0]) == array[0] + array[1]);
}

void testLoadOffsetUsingAdd()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    int array[] = { 1, 2 };
    ConstPtrValue* arrayPtr = root->appendNew<ConstPtrValue>(proc, Origin(), array);
    root->appendNewControlValue(
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
                    root->appendNew<ConstPtrValue>(proc, Origin(), static_cast<int32_t>(sizeof(int)))))));

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
            root->appendNew<ConstPtrValue>(proc, Origin(), static_cast<int32_t>(sizeof(int)))));
    root->appendNew<MemoryValue>(
        proc, Store, Origin(), theNumberOfTheBeast, otherArrayPtr, 0);
    root->appendNew<MemoryValue>(
        proc, Store, Origin(), theNumberOfTheBeast, otherArrayPtr, static_cast<int32_t>(sizeof(int)));
    root->appendNewControlValue(
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
    root->appendNewControlValue(
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
                    root->appendNew<ConstPtrValue>(proc, Origin(), static_cast<int32_t>(sizeof(int)))))));

    CHECK(compileAndRun<int>(proc, &array[0]) == array[0] + array[1]);
}

void testLoadAddrShift(unsigned shift)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    int slots[2];

    // Figure out which slot to use while having proper alignment for the shift.
    int* slot;
    uintptr_t arg;
    for (unsigned i = sizeof(slots)/sizeof(slots[0]); i--;) {
        slot = slots + i;
        arg = bitwise_cast<uintptr_t>(slot) >> shift;
        if (bitwise_cast<int*>(arg << shift) == slot)
            break;
    }

    *slot = 8675309;

    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<MemoryValue>(
            proc, Load, Int32, Origin(),
            root->appendNew<Value>(
                proc, Shl, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
                root->appendNew<Const32Value>(proc, Origin(), shift))));

    CHECK(compileAndRun<int>(proc, arg) == 8675309);
}

void testFramePointer()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(proc, FramePointer, Origin()));

    void* fp = compileAndRun<void*>(proc);
    CHECK(fp < &proc);
    CHECK(fp >= bitwise_cast<char*>(&proc) - 10000);
}

void testOverrideFramePointer()
{
    {
        Procedure proc;
        BasicBlock* root = proc.addBlock();

        // Add a stack slot to make the frame non trivial.
        root->appendNew<SlotBaseValue>(proc, Origin(), proc.addStackSlot(8));

        // Sub on x86 UseDef the source. If FP is not protected correctly, it will be overridden since it is the last visible use.
        Value* offset = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
        Value* fp = root->appendNew<Value>(proc, FramePointer, Origin());
        Value* result = root->appendNew<Value>(proc, Sub, Origin(), fp, offset);

        root->appendNewControlValue(proc, Return, Origin(), result);
        CHECK(compileAndRun<int64_t>(proc, 1));
    }
    {
        Procedure proc;
        BasicBlock* root = proc.addBlock();

        root->appendNew<SlotBaseValue>(proc, Origin(), proc.addStackSlot(8));

        Value* offset = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
        Value* fp = root->appendNew<Value>(proc, FramePointer, Origin());
        Value* offsetFP = root->appendNew<Value>(proc, BitAnd, Origin(), offset, fp);
        Value* arg = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
        Value* offsetArg = root->appendNew<Value>(proc, Add, Origin(), offset, arg);
        Value* result = root->appendNew<Value>(proc, Add, Origin(), offsetArg, offsetFP);

        root->appendNewControlValue(proc, Return, Origin(), result);
        CHECK(compileAndRun<int64_t>(proc, 1, 2));
    }
}

void testStackSlot()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<SlotBaseValue>(proc, Origin(), proc.addStackSlot(1)));

    void* stackSlot = compileAndRun<void*>(proc);
    CHECK(stackSlot < &proc);
    CHECK(stackSlot >= bitwise_cast<char*>(&proc) - 10000);
}

void testLoadFromFramePointer()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
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

    SlotBaseValue* stack =
        root->appendNew<SlotBaseValue>(proc, Origin(), proc.addStackSlot(sizeof(int)));

    root->appendNew<MemoryValue>(
        proc, Store, Origin(),
        root->appendNew<Value>(
            proc, Trunc, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)),
        stack, 0);

    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<MemoryValue>(proc, Load, Int32, Origin(), stack));

    CHECK(compileAndRun<int>(proc, value) == value);
}

void testStoreFloat(double input)
{
    // Simple store from an address in a register.
    {
        Procedure proc;
        BasicBlock* root = proc.addBlock();
        Value* argument = root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR0);
        Value* argumentAsFloat = root->appendNew<Value>(proc, DoubleToFloat, Origin(), argument);

        Value* destinationAddress = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
        root->appendNew<MemoryValue>(proc, Store, Origin(), argumentAsFloat, destinationAddress);

        root->appendNewControlValue(proc, Return, Origin(), root->appendNew<Const32Value>(proc, Origin(), 0));

        float output = 0.;
        CHECK(!compileAndRun<int64_t>(proc, input, &output));
        CHECK(isIdentical(static_cast<float>(input), output));
    }

    // Simple indexed store.
    {
        Procedure proc;
        BasicBlock* root = proc.addBlock();
        Value* argument = root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR0);
        Value* argumentAsFloat = root->appendNew<Value>(proc, DoubleToFloat, Origin(), argument);

        Value* destinationBaseAddress = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
        Value* index = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
        Value* scaledIndex = root->appendNew<Value>(
            proc, Shl, Origin(),
            index,
            root->appendNew<Const32Value>(proc, Origin(), 2));
        Value* destinationAddress = root->appendNew<Value>(proc, Add, Origin(), scaledIndex, destinationBaseAddress);

        root->appendNew<MemoryValue>(proc, Store, Origin(), argumentAsFloat, destinationAddress);

        root->appendNewControlValue(proc, Return, Origin(), root->appendNew<Const32Value>(proc, Origin(), 0));

        float output = 0.;
        CHECK(!compileAndRun<int64_t>(proc, input, &output - 1, 1));
        CHECK(isIdentical(static_cast<float>(input), output));
    }
}

void testStoreDoubleConstantAsFloat(double input)
{
    // Simple store from an address in a register.
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* value = root->appendNew<ConstDoubleValue>(proc, Origin(), input);
    Value* valueAsFloat = root->appendNew<Value>(proc, DoubleToFloat, Origin(), value);

    Value* destinationAddress = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);

    root->appendNew<MemoryValue>(proc, Store, Origin(), valueAsFloat, destinationAddress);

    root->appendNewControlValue(proc, Return, Origin(), root->appendNew<Const32Value>(proc, Origin(), 0));

    float output = 0.;
    CHECK(!compileAndRun<int64_t>(proc, input, &output));
    CHECK(isIdentical(static_cast<float>(input), output));
}

void testSpillGP()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();

    Vector<Value*> sources;
    sources.append(root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    sources.append(root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1));

    for (unsigned i = 0; i < 30; ++i) {
        sources.append(
            root->appendNew<Value>(proc, Add, Origin(), sources[sources.size() - 1], sources[sources.size() - 2])
        );
    }

    Value* total = root->appendNew<Const64Value>(proc, Origin(), 0);
    for (Value* value : sources)
        total = root->appendNew<Value>(proc, Add, Origin(), total, value);

    root->appendNewControlValue(proc, Return, Origin(), total);
    compileAndRun<int>(proc, 1, 2);
}

void testSpillFP()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();

    Vector<Value*> sources;
    sources.append(root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR0));
    sources.append(root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR1));

    for (unsigned i = 0; i < 30; ++i) {
        sources.append(
            root->appendNew<Value>(proc, Add, Origin(), sources[sources.size() - 1], sources[sources.size() - 2])
        );
    }

    Value* total = root->appendNew<ConstDoubleValue>(proc, Origin(), 0.);
    for (Value* value : sources)
        total = root->appendNew<Value>(proc, Add, Origin(), total, value);

    root->appendNewControlValue(proc, Return, Origin(), total);
    compileAndRun<double>(proc, 1.1, 2.5);
}

void testInt32ToDoublePartialRegisterStall()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* loop = proc.addBlock();
    BasicBlock* done = proc.addBlock();

    // Head.
    Value* total = root->appendNew<ConstDoubleValue>(proc, Origin(), 0.);
    Value* counter = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    UpsilonValue* originalTotal = root->appendNew<UpsilonValue>(proc, Origin(), total);
    UpsilonValue* originalCounter = root->appendNew<UpsilonValue>(proc, Origin(), counter);
    root->appendNewControlValue(proc, Jump, Origin(), FrequentedBlock(loop));

    // Loop.
    Value* loopCounter = loop->appendNew<Value>(proc, Phi, Int64, Origin());
    Value* loopTotal = loop->appendNew<Value>(proc, Phi, Double, Origin());
    originalCounter->setPhi(loopCounter);
    originalTotal->setPhi(loopTotal);

    Value* truncatedCounter = loop->appendNew<Value>(proc, Trunc, Origin(), loopCounter);
    Value* doubleCounter = loop->appendNew<Value>(proc, IToD, Origin(), truncatedCounter);
    Value* updatedTotal = loop->appendNew<Value>(proc, Add, Origin(), doubleCounter, loopTotal);
    UpsilonValue* updatedTotalUpsilon = loop->appendNew<UpsilonValue>(proc, Origin(), updatedTotal);
    updatedTotalUpsilon->setPhi(loopTotal);

    Value* decCounter = loop->appendNew<Value>(proc, Sub, Origin(), loopCounter, loop->appendNew<Const64Value>(proc, Origin(), 1));
    UpsilonValue* decCounterUpsilon = loop->appendNew<UpsilonValue>(proc, Origin(), decCounter);
    decCounterUpsilon->setPhi(loopCounter);
    loop->appendNewControlValue(
        proc, Branch, Origin(),
        decCounter,
        FrequentedBlock(loop), FrequentedBlock(done));

    // Tail.
    done->appendNewControlValue(proc, Return, Origin(), updatedTotal);
    CHECK(isIdentical(compileAndRun<double>(proc, 100000), 5000050000.));
}

void testInt32ToDoublePartialRegisterWithoutStall()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* loop = proc.addBlock();
    BasicBlock* done = proc.addBlock();

    // Head.
    Value* total = root->appendNew<ConstDoubleValue>(proc, Origin(), 0.);
    Value* counter = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    UpsilonValue* originalTotal = root->appendNew<UpsilonValue>(proc, Origin(), total);
    UpsilonValue* originalCounter = root->appendNew<UpsilonValue>(proc, Origin(), counter);
    uint64_t forPaddingInput;
    Value* forPaddingInputAddress = root->appendNew<ConstPtrValue>(proc, Origin(), &forPaddingInput);
    uint64_t forPaddingOutput;
    Value* forPaddingOutputAddress = root->appendNew<ConstPtrValue>(proc, Origin(), &forPaddingOutput);
    root->appendNewControlValue(proc, Jump, Origin(), FrequentedBlock(loop));

    // Loop.
    Value* loopCounter = loop->appendNew<Value>(proc, Phi, Int64, Origin());
    Value* loopTotal = loop->appendNew<Value>(proc, Phi, Double, Origin());
    originalCounter->setPhi(loopCounter);
    originalTotal->setPhi(loopTotal);

    Value* truncatedCounter = loop->appendNew<Value>(proc, Trunc, Origin(), loopCounter);
    Value* doubleCounter = loop->appendNew<Value>(proc, IToD, Origin(), truncatedCounter);
    Value* updatedTotal = loop->appendNew<Value>(proc, Add, Origin(), doubleCounter, loopTotal);

    // Add enough padding instructions to avoid a stall.
    Value* loadPadding = loop->appendNew<MemoryValue>(proc, Load, Int64, Origin(), forPaddingInputAddress);
    Value* padding = loop->appendNew<Value>(proc, BitXor, Origin(), loadPadding, loopCounter);
    padding = loop->appendNew<Value>(proc, Add, Origin(), padding, loopCounter);
    padding = loop->appendNew<Value>(proc, BitOr, Origin(), padding, loopCounter);
    padding = loop->appendNew<Value>(proc, Sub, Origin(), padding, loopCounter);
    padding = loop->appendNew<Value>(proc, BitXor, Origin(), padding, loopCounter);
    padding = loop->appendNew<Value>(proc, Add, Origin(), padding, loopCounter);
    padding = loop->appendNew<Value>(proc, BitOr, Origin(), padding, loopCounter);
    padding = loop->appendNew<Value>(proc, Sub, Origin(), padding, loopCounter);
    padding = loop->appendNew<Value>(proc, BitXor, Origin(), padding, loopCounter);
    padding = loop->appendNew<Value>(proc, Add, Origin(), padding, loopCounter);
    padding = loop->appendNew<Value>(proc, BitOr, Origin(), padding, loopCounter);
    padding = loop->appendNew<Value>(proc, Sub, Origin(), padding, loopCounter);
    loop->appendNew<MemoryValue>(proc, Store, Origin(), padding, forPaddingOutputAddress);

    UpsilonValue* updatedTotalUpsilon = loop->appendNew<UpsilonValue>(proc, Origin(), updatedTotal);
    updatedTotalUpsilon->setPhi(loopTotal);

    Value* decCounter = loop->appendNew<Value>(proc, Sub, Origin(), loopCounter, loop->appendNew<Const64Value>(proc, Origin(), 1));
    UpsilonValue* decCounterUpsilon = loop->appendNew<UpsilonValue>(proc, Origin(), decCounter);
    decCounterUpsilon->setPhi(loopCounter);
    loop->appendNewControlValue(
        proc, Branch, Origin(),
        decCounter,
        FrequentedBlock(loop), FrequentedBlock(done));

    // Tail.
    done->appendNewControlValue(proc, Return, Origin(), updatedTotal);
    CHECK(isIdentical(compileAndRun<double>(proc, 100000), 5000050000.));
}

void testBranch()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* thenCase = proc.addBlock();
    BasicBlock* elseCase = proc.addBlock();

    root->appendNewControlValue(
        proc, Branch, Origin(),
        root->appendNew<Value>(
            proc, Trunc, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)),
        FrequentedBlock(thenCase), FrequentedBlock(elseCase));

    thenCase->appendNewControlValue(
        proc, Return, Origin(),
        thenCase->appendNew<Const32Value>(proc, Origin(), 1));

    elseCase->appendNewControlValue(
        proc, Return, Origin(),
        elseCase->appendNew<Const32Value>(proc, Origin(), 0));

    auto code = compileProc(proc);
    CHECK(invoke<int>(*code, 42) == 1);
    CHECK(invoke<int>(*code, 0) == 0);
}

void testBranchPtr()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* thenCase = proc.addBlock();
    BasicBlock* elseCase = proc.addBlock();

    root->appendNewControlValue(
        proc, Branch, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
        FrequentedBlock(thenCase), FrequentedBlock(elseCase));

    thenCase->appendNewControlValue(
        proc, Return, Origin(),
        thenCase->appendNew<Const32Value>(proc, Origin(), 1));

    elseCase->appendNewControlValue(
        proc, Return, Origin(),
        elseCase->appendNew<Const32Value>(proc, Origin(), 0));

    auto code = compileProc(proc);
    CHECK(invoke<int>(*code, static_cast<intptr_t>(42)) == 1);
    CHECK(invoke<int>(*code, static_cast<intptr_t>(0)) == 0);
}

void testDiamond()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* thenCase = proc.addBlock();
    BasicBlock* elseCase = proc.addBlock();
    BasicBlock* done = proc.addBlock();

    root->appendNewControlValue(
        proc, Branch, Origin(),
        root->appendNew<Value>(
            proc, Trunc, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)),
        FrequentedBlock(thenCase), FrequentedBlock(elseCase));

    UpsilonValue* thenResult = thenCase->appendNew<UpsilonValue>(
        proc, Origin(), thenCase->appendNew<Const32Value>(proc, Origin(), 1));
    thenCase->appendNewControlValue(proc, Jump, Origin(), FrequentedBlock(done));

    UpsilonValue* elseResult = elseCase->appendNew<UpsilonValue>(
        proc, Origin(), elseCase->appendNew<Const32Value>(proc, Origin(), 0));
    elseCase->appendNewControlValue(proc, Jump, Origin(), FrequentedBlock(done));

    Value* phi = done->appendNew<Value>(proc, Phi, Int32, Origin());
    thenResult->setPhi(phi);
    elseResult->setPhi(phi);
    done->appendNewControlValue(proc, Return, Origin(), phi);

    auto code = compileProc(proc);
    CHECK(invoke<int>(*code, 42) == 1);
    CHECK(invoke<int>(*code, 0) == 0);
}

void testBranchNotEqual()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* thenCase = proc.addBlock();
    BasicBlock* elseCase = proc.addBlock();

    root->appendNewControlValue(
        proc, Branch, Origin(),
        root->appendNew<Value>(
            proc, NotEqual, Origin(),
            root->appendNew<Value>(
                proc, Trunc, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)),
            root->appendNew<Const32Value>(proc, Origin(), 0)),
        FrequentedBlock(thenCase), FrequentedBlock(elseCase));

    thenCase->appendNewControlValue(
        proc, Return, Origin(),
        thenCase->appendNew<Const32Value>(proc, Origin(), 1));

    elseCase->appendNewControlValue(
        proc, Return, Origin(),
        elseCase->appendNew<Const32Value>(proc, Origin(), 0));

    auto code = compileProc(proc);
    CHECK(invoke<int>(*code, 42) == 1);
    CHECK(invoke<int>(*code, 0) == 0);
}

void testBranchNotEqualCommute()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* thenCase = proc.addBlock();
    BasicBlock* elseCase = proc.addBlock();

    root->appendNewControlValue(
        proc, Branch, Origin(),
        root->appendNew<Value>(
            proc, NotEqual, Origin(),
            root->appendNew<Const32Value>(proc, Origin(), 0),
            root->appendNew<Value>(
                proc, Trunc, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0))),
        FrequentedBlock(thenCase), FrequentedBlock(elseCase));

    thenCase->appendNewControlValue(
        proc, Return, Origin(),
        thenCase->appendNew<Const32Value>(proc, Origin(), 1));

    elseCase->appendNewControlValue(
        proc, Return, Origin(),
        elseCase->appendNew<Const32Value>(proc, Origin(), 0));

    auto code = compileProc(proc);
    CHECK(invoke<int>(*code, 42) == 1);
    CHECK(invoke<int>(*code, 0) == 0);
}

void testBranchNotEqualNotEqual()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* thenCase = proc.addBlock();
    BasicBlock* elseCase = proc.addBlock();

    root->appendNewControlValue(
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

    thenCase->appendNewControlValue(
        proc, Return, Origin(),
        thenCase->appendNew<Const32Value>(proc, Origin(), 1));

    elseCase->appendNewControlValue(
        proc, Return, Origin(),
        elseCase->appendNew<Const32Value>(proc, Origin(), 0));

    auto code = compileProc(proc);
    CHECK(invoke<int>(*code, 42) == 1);
    CHECK(invoke<int>(*code, 0) == 0);
}

void testBranchEqual()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* thenCase = proc.addBlock();
    BasicBlock* elseCase = proc.addBlock();

    root->appendNewControlValue(
        proc, Branch, Origin(),
        root->appendNew<Value>(
            proc, Equal, Origin(),
            root->appendNew<Value>(
                proc, Trunc, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)),
            root->appendNew<Const32Value>(proc, Origin(), 0)),
        FrequentedBlock(thenCase), FrequentedBlock(elseCase));

    thenCase->appendNewControlValue(
        proc, Return, Origin(),
        thenCase->appendNew<Const32Value>(proc, Origin(), 0));

    elseCase->appendNewControlValue(
        proc, Return, Origin(),
        elseCase->appendNew<Const32Value>(proc, Origin(), 1));

    auto code = compileProc(proc);
    CHECK(invoke<int>(*code, 42) == 1);
    CHECK(invoke<int>(*code, 0) == 0);
}

void testBranchEqualEqual()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* thenCase = proc.addBlock();
    BasicBlock* elseCase = proc.addBlock();

    root->appendNewControlValue(
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

    thenCase->appendNewControlValue(
        proc, Return, Origin(),
        thenCase->appendNew<Const32Value>(proc, Origin(), 1));

    elseCase->appendNewControlValue(
        proc, Return, Origin(),
        elseCase->appendNew<Const32Value>(proc, Origin(), 0));

    auto code = compileProc(proc);
    CHECK(invoke<int>(*code, 42) == 1);
    CHECK(invoke<int>(*code, 0) == 0);
}

void testBranchEqualCommute()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* thenCase = proc.addBlock();
    BasicBlock* elseCase = proc.addBlock();

    root->appendNewControlValue(
        proc, Branch, Origin(),
        root->appendNew<Value>(
            proc, Equal, Origin(),
            root->appendNew<Const32Value>(proc, Origin(), 0),
            root->appendNew<Value>(
                proc, Trunc, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0))),
        FrequentedBlock(thenCase), FrequentedBlock(elseCase));

    thenCase->appendNewControlValue(
        proc, Return, Origin(),
        thenCase->appendNew<Const32Value>(proc, Origin(), 0));

    elseCase->appendNewControlValue(
        proc, Return, Origin(),
        elseCase->appendNew<Const32Value>(proc, Origin(), 1));

    auto code = compileProc(proc);
    CHECK(invoke<int>(*code, 42) == 1);
    CHECK(invoke<int>(*code, 0) == 0);
}

void testBranchEqualEqual1()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* thenCase = proc.addBlock();
    BasicBlock* elseCase = proc.addBlock();

    root->appendNewControlValue(
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

    thenCase->appendNewControlValue(
        proc, Return, Origin(),
        thenCase->appendNew<Const32Value>(proc, Origin(), 0));

    elseCase->appendNewControlValue(
        proc, Return, Origin(),
        elseCase->appendNew<Const32Value>(proc, Origin(), 1));

    auto code = compileProc(proc);
    CHECK(invoke<int>(*code, 42) == 1);
    CHECK(invoke<int>(*code, 0) == 0);
}

void testBranchEqualOrUnorderedArgs(double a, double b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* thenCase = proc.addBlock();
    BasicBlock* elseCase = proc.addBlock();

    Value* argumentA = root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR0);
    Value* argumentB = root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR1);
    root->appendNewControlValue(
        proc, Branch, Origin(),
        root->appendNew<Value>(
            proc, EqualOrUnordered, Origin(),
            argumentA,
            argumentB),
        FrequentedBlock(thenCase), FrequentedBlock(elseCase));

    thenCase->appendNewControlValue(
        proc, Return, Origin(),
        thenCase->appendNew<Const32Value>(proc, Origin(), 42));

    elseCase->appendNewControlValue(
        proc, Return, Origin(),
        elseCase->appendNew<Const32Value>(proc, Origin(), -13));

    int64_t expected = (std::isunordered(a, b) || a == b) ? 42 : -13;
    CHECK(compileAndRun<int64_t>(proc, a, b) == expected);
}

void testBranchEqualOrUnorderedArgs(float a, float b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* thenCase = proc.addBlock();
    BasicBlock* elseCase = proc.addBlock();

    Value* argumentA = root->appendNew<MemoryValue>(proc, Load, Float, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* argumentB = root->appendNew<MemoryValue>(proc, Load, Float, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1));

    root->appendNewControlValue(
        proc, Branch, Origin(),
        root->appendNew<Value>(
            proc, EqualOrUnordered, Origin(),
            argumentA,
            argumentB),
        FrequentedBlock(thenCase), FrequentedBlock(elseCase));

    thenCase->appendNewControlValue(
        proc, Return, Origin(),
        thenCase->appendNew<Const32Value>(proc, Origin(), 42));

    elseCase->appendNewControlValue(
        proc, Return, Origin(),
        elseCase->appendNew<Const32Value>(proc, Origin(), -13));

    int64_t expected = (std::isunordered(a, b) || a == b) ? 42 : -13;
    CHECK(compileAndRun<int64_t>(proc, &a, &b) == expected);
}

void testBranchNotEqualAndOrderedArgs(double a, double b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* thenCase = proc.addBlock();
    BasicBlock* elseCase = proc.addBlock();

    Value* argumentA = root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR0);
    Value* argumentB = root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR1);
    Value* equalOrUnordered = root->appendNew<Value>(
        proc, EqualOrUnordered, Origin(),
        argumentA,
        argumentB);
    Value* notEqualAndOrdered = root->appendNew<Value>(
        proc, Equal, Origin(),
        root->appendNew<Const32Value>(proc, Origin(), 0),
        equalOrUnordered);
    root->appendNewControlValue(
        proc, Branch, Origin(),
        notEqualAndOrdered,
        FrequentedBlock(thenCase), FrequentedBlock(elseCase));

    thenCase->appendNewControlValue(
        proc, Return, Origin(),
        thenCase->appendNew<Const32Value>(proc, Origin(), 42));

    elseCase->appendNewControlValue(
        proc, Return, Origin(),
        elseCase->appendNew<Const32Value>(proc, Origin(), -13));

    int64_t expected = (!std::isunordered(a, b) && a != b) ? 42 : -13;
    CHECK(compileAndRun<int64_t>(proc, a, b) == expected);
}

void testBranchNotEqualAndOrderedArgs(float a, float b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* thenCase = proc.addBlock();
    BasicBlock* elseCase = proc.addBlock();

    Value* argumentA = root->appendNew<MemoryValue>(proc, Load, Float, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* argumentB = root->appendNew<MemoryValue>(proc, Load, Float, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1));
    Value* equalOrUnordered = root->appendNew<Value>(
        proc, EqualOrUnordered, Origin(),
        argumentA,
        argumentB);
    Value* notEqualAndOrdered = root->appendNew<Value>(
        proc, Equal, Origin(),
        root->appendNew<Const32Value>(proc, Origin(), 0),
        equalOrUnordered);
    root->appendNewControlValue(
        proc, Branch, Origin(),
        notEqualAndOrdered,
        FrequentedBlock(thenCase), FrequentedBlock(elseCase));

    thenCase->appendNewControlValue(
        proc, Return, Origin(),
        thenCase->appendNew<Const32Value>(proc, Origin(), 42));

    elseCase->appendNewControlValue(
        proc, Return, Origin(),
        elseCase->appendNew<Const32Value>(proc, Origin(), -13));

    int64_t expected = (!std::isunordered(a, b) && a != b) ? 42 : -13;
    CHECK(compileAndRun<int64_t>(proc, &a, &b) == expected);
}

void testBranchEqualOrUnorderedDoubleArgImm(double a, double b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* thenCase = proc.addBlock();
    BasicBlock* elseCase = proc.addBlock();

    Value* argumentA = root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR0);
    Value* argumentB = root->appendNew<ConstDoubleValue>(proc, Origin(), b);
    root->appendNewControlValue(
        proc, Branch, Origin(),
        root->appendNew<Value>(
            proc, EqualOrUnordered, Origin(),
            argumentA,
            argumentB),
        FrequentedBlock(thenCase), FrequentedBlock(elseCase));

    thenCase->appendNewControlValue(
        proc, Return, Origin(),
        thenCase->appendNew<Const32Value>(proc, Origin(), 42));

    elseCase->appendNewControlValue(
        proc, Return, Origin(),
        elseCase->appendNew<Const32Value>(proc, Origin(), -13));

    int64_t expected = (std::isunordered(a, b) || a == b) ? 42 : -13;
    CHECK(compileAndRun<int64_t>(proc, a) == expected);
}

void testBranchEqualOrUnorderedFloatArgImm(float a, float b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* thenCase = proc.addBlock();
    BasicBlock* elseCase = proc.addBlock();

    Value* argumentA = root->appendNew<MemoryValue>(proc, Load, Float, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* argumentB = root->appendNew<ConstFloatValue>(proc, Origin(), b);

    root->appendNewControlValue(
        proc, Branch, Origin(),
        root->appendNew<Value>(
            proc, EqualOrUnordered, Origin(),
            argumentA,
            argumentB),
        FrequentedBlock(thenCase), FrequentedBlock(elseCase));

    thenCase->appendNewControlValue(
        proc, Return, Origin(),
        thenCase->appendNew<Const32Value>(proc, Origin(), 42));

    elseCase->appendNewControlValue(
        proc, Return, Origin(),
        elseCase->appendNew<Const32Value>(proc, Origin(), -13));

    int64_t expected = (std::isunordered(a, b) || a == b) ? 42 : -13;
    CHECK(compileAndRun<int64_t>(proc, &a) == expected);
}

void testBranchEqualOrUnorderedDoubleImms(double a, double b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* thenCase = proc.addBlock();
    BasicBlock* elseCase = proc.addBlock();

    Value* argumentA = root->appendNew<ConstDoubleValue>(proc, Origin(), a);
    Value* argumentB = root->appendNew<ConstDoubleValue>(proc, Origin(), b);
    root->appendNewControlValue(
        proc, Branch, Origin(),
        root->appendNew<Value>(
            proc, EqualOrUnordered, Origin(),
            argumentA,
            argumentB),
        FrequentedBlock(thenCase), FrequentedBlock(elseCase));

    thenCase->appendNewControlValue(
        proc, Return, Origin(),
        thenCase->appendNew<Const32Value>(proc, Origin(), 42));

    elseCase->appendNewControlValue(
        proc, Return, Origin(),
        elseCase->appendNew<Const32Value>(proc, Origin(), -13));

    int64_t expected = (std::isunordered(a, b) || a == b) ? 42 : -13;
    CHECK(compileAndRun<int64_t>(proc) == expected);
}

void testBranchEqualOrUnorderedFloatImms(float a, float b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* thenCase = proc.addBlock();
    BasicBlock* elseCase = proc.addBlock();

    Value* argumentA = root->appendNew<ConstFloatValue>(proc, Origin(), a);
    Value* argumentB = root->appendNew<ConstFloatValue>(proc, Origin(), b);

    root->appendNewControlValue(
        proc, Branch, Origin(),
        root->appendNew<Value>(
            proc, EqualOrUnordered, Origin(),
            argumentA,
            argumentB),
        FrequentedBlock(thenCase), FrequentedBlock(elseCase));

    thenCase->appendNewControlValue(
        proc, Return, Origin(),
        thenCase->appendNew<Const32Value>(proc, Origin(), 42));

    elseCase->appendNewControlValue(
        proc, Return, Origin(),
        elseCase->appendNew<Const32Value>(proc, Origin(), -13));

    int64_t expected = (std::isunordered(a, b) || a == b) ? 42 : -13;
    CHECK(compileAndRun<int64_t>(proc) == expected);
}

void testBranchEqualOrUnorderedFloatWithUselessDoubleConversion(float a, float b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* thenCase = proc.addBlock();
    BasicBlock* elseCase = proc.addBlock();

    Value* argument1 = root->appendNew<MemoryValue>(proc, Load, Float, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* argument2 = root->appendNew<MemoryValue>(proc, Load, Float, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1));
    Value* argument1AsDouble = root->appendNew<Value>(proc, FloatToDouble, Origin(), argument1);
    Value* argument2AsDouble = root->appendNew<Value>(proc, FloatToDouble, Origin(), argument2);

    root->appendNewControlValue(
        proc, Branch, Origin(),
        root->appendNew<Value>(
            proc, EqualOrUnordered, Origin(),
            argument1AsDouble,
            argument2AsDouble),
        FrequentedBlock(thenCase), FrequentedBlock(elseCase));

    thenCase->appendNewControlValue(
        proc, Return, Origin(),
        thenCase->appendNew<Const32Value>(proc, Origin(), 42));

    elseCase->appendNewControlValue(
        proc, Return, Origin(),
        elseCase->appendNew<Const32Value>(proc, Origin(), -13));

    int64_t expected = (std::isunordered(a, b) || a == b) ? 42 : -13;
    CHECK(compileAndRun<int64_t>(proc, &a, &b) == expected);
}

void testBranchFold(int value)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* thenCase = proc.addBlock();
    BasicBlock* elseCase = proc.addBlock();

    root->appendNewControlValue(
        proc, Branch, Origin(),
        root->appendNew<Const32Value>(proc, Origin(), value),
        FrequentedBlock(thenCase), FrequentedBlock(elseCase));

    thenCase->appendNewControlValue(
        proc, Return, Origin(),
        thenCase->appendNew<Const32Value>(proc, Origin(), 1));

    elseCase->appendNewControlValue(
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

    root->appendNewControlValue(
        proc, Branch, Origin(),
        root->appendNew<Const32Value>(proc, Origin(), value),
        FrequentedBlock(thenCase), FrequentedBlock(elseCase));

    UpsilonValue* thenResult = thenCase->appendNew<UpsilonValue>(
        proc, Origin(), thenCase->appendNew<Const32Value>(proc, Origin(), 1));
    thenCase->appendNewControlValue(proc, Jump, Origin(), FrequentedBlock(done));

    UpsilonValue* elseResult = elseCase->appendNew<UpsilonValue>(
        proc, Origin(), elseCase->appendNew<Const32Value>(proc, Origin(), 0));
    elseCase->appendNewControlValue(proc, Jump, Origin(), FrequentedBlock(done));

    Value* phi = done->appendNew<Value>(proc, Phi, Int32, Origin());
    thenResult->setPhi(phi);
    elseResult->setPhi(phi);
    done->appendNewControlValue(proc, Return, Origin(), phi);

    CHECK(compileAndRun<int>(proc) == !!value);
}

void testBranchNotEqualFoldPtr(intptr_t value)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* thenCase = proc.addBlock();
    BasicBlock* elseCase = proc.addBlock();

    root->appendNewControlValue(
        proc, Branch, Origin(),
        root->appendNew<Value>(
            proc, NotEqual, Origin(),
            root->appendNew<ConstPtrValue>(proc, Origin(), value),
            root->appendNew<ConstPtrValue>(proc, Origin(), 0)),
        FrequentedBlock(thenCase), FrequentedBlock(elseCase));

    thenCase->appendNewControlValue(
        proc, Return, Origin(),
        thenCase->appendNew<Const32Value>(proc, Origin(), 1));

    elseCase->appendNewControlValue(
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

    root->appendNewControlValue(
        proc, Branch, Origin(),
        root->appendNew<Value>(
            proc, Equal, Origin(),
            root->appendNew<ConstPtrValue>(proc, Origin(), value),
            root->appendNew<ConstPtrValue>(proc, Origin(), 0)),
        FrequentedBlock(thenCase), FrequentedBlock(elseCase));

    thenCase->appendNewControlValue(
        proc, Return, Origin(),
        thenCase->appendNew<Const32Value>(proc, Origin(), 1));

    elseCase->appendNewControlValue(
        proc, Return, Origin(),
        elseCase->appendNew<Const32Value>(proc, Origin(), 0));

    CHECK(compileAndRun<int>(proc) == !value);
}

void testBranchLoadPtr()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* thenCase = proc.addBlock();
    BasicBlock* elseCase = proc.addBlock();

    root->appendNewControlValue(
        proc, Branch, Origin(),
        root->appendNew<MemoryValue>(
            proc, Load, pointerType(), Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)),
        FrequentedBlock(thenCase), FrequentedBlock(elseCase));

    thenCase->appendNewControlValue(
        proc, Return, Origin(),
        thenCase->appendNew<Const32Value>(proc, Origin(), 1));

    elseCase->appendNewControlValue(
        proc, Return, Origin(),
        elseCase->appendNew<Const32Value>(proc, Origin(), 0));

    auto code = compileProc(proc);
    intptr_t cond;
    cond = 42;
    CHECK(invoke<int>(*code, &cond) == 1);
    cond = 0;
    CHECK(invoke<int>(*code, &cond) == 0);
}

void testBranchLoad32()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* thenCase = proc.addBlock();
    BasicBlock* elseCase = proc.addBlock();

    root->appendNewControlValue(
        proc, Branch, Origin(),
        root->appendNew<MemoryValue>(
            proc, Load, Int32, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)),
        FrequentedBlock(thenCase), FrequentedBlock(elseCase));

    thenCase->appendNewControlValue(
        proc, Return, Origin(),
        thenCase->appendNew<Const32Value>(proc, Origin(), 1));

    elseCase->appendNewControlValue(
        proc, Return, Origin(),
        elseCase->appendNew<Const32Value>(proc, Origin(), 0));

    auto code = compileProc(proc);
    int32_t cond;
    cond = 42;
    CHECK(invoke<int>(*code, &cond) == 1);
    cond = 0;
    CHECK(invoke<int>(*code, &cond) == 0);
}

void testBranchLoad8S()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* thenCase = proc.addBlock();
    BasicBlock* elseCase = proc.addBlock();

    root->appendNewControlValue(
        proc, Branch, Origin(),
        root->appendNew<MemoryValue>(
            proc, Load8S, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)),
        FrequentedBlock(thenCase), FrequentedBlock(elseCase));

    thenCase->appendNewControlValue(
        proc, Return, Origin(),
        thenCase->appendNew<Const32Value>(proc, Origin(), 1));

    elseCase->appendNewControlValue(
        proc, Return, Origin(),
        elseCase->appendNew<Const32Value>(proc, Origin(), 0));

    auto code = compileProc(proc);
    int8_t cond;
    cond = -1;
    CHECK(invoke<int>(*code, &cond) == 1);
    cond = 0;
    CHECK(invoke<int>(*code, &cond) == 0);
}

void testBranchLoad8Z()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* thenCase = proc.addBlock();
    BasicBlock* elseCase = proc.addBlock();

    root->appendNewControlValue(
        proc, Branch, Origin(),
        root->appendNew<MemoryValue>(
            proc, Load8Z, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)),
        FrequentedBlock(thenCase), FrequentedBlock(elseCase));

    thenCase->appendNewControlValue(
        proc, Return, Origin(),
        thenCase->appendNew<Const32Value>(proc, Origin(), 1));

    elseCase->appendNewControlValue(
        proc, Return, Origin(),
        elseCase->appendNew<Const32Value>(proc, Origin(), 0));

    auto code = compileProc(proc);
    uint8_t cond;
    cond = 1;
    CHECK(invoke<int>(*code, &cond) == 1);
    cond = 0;
    CHECK(invoke<int>(*code, &cond) == 0);
}

void testBranchLoad16S()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* thenCase = proc.addBlock();
    BasicBlock* elseCase = proc.addBlock();

    root->appendNewControlValue(
        proc, Branch, Origin(),
        root->appendNew<MemoryValue>(
            proc, Load16S, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)),
        FrequentedBlock(thenCase), FrequentedBlock(elseCase));

    thenCase->appendNewControlValue(
        proc, Return, Origin(),
        thenCase->appendNew<Const32Value>(proc, Origin(), 1));

    elseCase->appendNewControlValue(
        proc, Return, Origin(),
        elseCase->appendNew<Const32Value>(proc, Origin(), 0));

    auto code = compileProc(proc);
    int16_t cond;
    cond = -1;
    CHECK(invoke<int>(*code, &cond) == 1);
    cond = 0;
    CHECK(invoke<int>(*code, &cond) == 0);
}

void testBranchLoad16Z()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* thenCase = proc.addBlock();
    BasicBlock* elseCase = proc.addBlock();

    root->appendNewControlValue(
        proc, Branch, Origin(),
        root->appendNew<MemoryValue>(
            proc, Load16Z, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)),
        FrequentedBlock(thenCase), FrequentedBlock(elseCase));

    thenCase->appendNewControlValue(
        proc, Return, Origin(),
        thenCase->appendNew<Const32Value>(proc, Origin(), 1));

    elseCase->appendNewControlValue(
        proc, Return, Origin(),
        elseCase->appendNew<Const32Value>(proc, Origin(), 0));

    auto code = compileProc(proc);
    uint16_t cond;
    cond = 1;
    CHECK(invoke<int>(*code, &cond) == 1);
    cond = 0;
    CHECK(invoke<int>(*code, &cond) == 0);
}

void testBranch8WithLoad8ZIndex()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* thenCase = proc.addBlock();
    BasicBlock* elseCase = proc.addBlock();

    int logScale = 1;
    root->appendNewControlValue(
        proc, Branch, Origin(),
        root->appendNew<Value>(
            proc, Above, Origin(),
            root->appendNew<MemoryValue>(
                proc, Load8Z, Origin(),
                root->appendNew<Value>(
                    proc, Add, Origin(),
                    root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
                    root->appendNew<Value>(
                        proc, Shl, Origin(),
                        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1),
                        root->appendNew<Const32Value>(proc, Origin(), logScale)))),
            root->appendNew<Const32Value>(proc, Origin(), 250)),
        FrequentedBlock(thenCase), FrequentedBlock(elseCase));

    thenCase->appendNewControlValue(
        proc, Return, Origin(),
        thenCase->appendNew<Const32Value>(proc, Origin(), 1));

    elseCase->appendNewControlValue(
        proc, Return, Origin(),
        elseCase->appendNew<Const32Value>(proc, Origin(), 0));

    auto code = compileProc(proc);
    uint32_t cond;
    cond = 0xffffffffU; // All bytes are 0xff.
    CHECK(invoke<int>(*code, &cond - 2, (sizeof(uint32_t) * 2) >> logScale) == 1);
    cond = 0x00000000U; // All bytes are 0.
    CHECK(invoke<int>(*code, &cond - 2, (sizeof(uint32_t) * 2) >> logScale) == 0);
}

void testComplex(unsigned numVars, unsigned numConstructs)
{
    MonotonicTime before = MonotonicTime::now();

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
            unsigned predicateVarIndex = ((i >> 1) + 2) % numVars;
            unsigned thenIncVarIndex = ((i >> 1) + 0) % numVars;
            unsigned elseIncVarIndex = ((i >> 1) + 1) % numVars;

            BasicBlock* thenBlock = proc.addBlock();
            BasicBlock* elseBlock = proc.addBlock();
            BasicBlock* continuation = proc.addBlock();

            current->appendNewControlValue(
                proc, Branch, Origin(), vars[predicateVarIndex],
                FrequentedBlock(thenBlock), FrequentedBlock(elseBlock));

            UpsilonValue* thenThenResult = thenBlock->appendNew<UpsilonValue>(
                proc, Origin(),
                thenBlock->appendNew<Value>(proc, Add, Origin(), vars[thenIncVarIndex], one));
            UpsilonValue* thenElseResult = thenBlock->appendNew<UpsilonValue>(
                proc, Origin(), vars[elseIncVarIndex]);
            thenBlock->appendNewControlValue(proc, Jump, Origin(), FrequentedBlock(continuation));

            UpsilonValue* elseElseResult = elseBlock->appendNew<UpsilonValue>(
                proc, Origin(),
                elseBlock->appendNew<Value>(proc, Add, Origin(), vars[elseIncVarIndex], one));
            UpsilonValue* elseThenResult = elseBlock->appendNew<UpsilonValue>(
                proc, Origin(), vars[thenIncVarIndex]);
            elseBlock->appendNewControlValue(proc, Jump, Origin(), FrequentedBlock(continuation));

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
        
            Value* startIndex = vars[((i >> 1) + 1) % numVars];
            Value* startSum = current->appendNew<Const32Value>(proc, Origin(), 0);
            current->appendNewControlValue(
                proc, Branch, Origin(), startIndex,
                FrequentedBlock(loopEntry), FrequentedBlock(loopSkip));

            UpsilonValue* startIndexForBody = loopEntry->appendNew<UpsilonValue>(
                proc, Origin(), startIndex);
            UpsilonValue* startSumForBody = loopEntry->appendNew<UpsilonValue>(
                proc, Origin(), startSum);
            loopEntry->appendNewControlValue(proc, Jump, Origin(), FrequentedBlock(loopBody));

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
            loopBody->appendNewControlValue(
                proc, Branch, Origin(), newBodyIndex,
                FrequentedBlock(loopReentry), FrequentedBlock(loopExit));

            loopReentry->appendNew<UpsilonValue>(proc, Origin(), newBodyIndex, bodyIndex);
            loopReentry->appendNew<UpsilonValue>(proc, Origin(), newBodySum, bodySum);
            loopReentry->appendNewControlValue(proc, Jump, Origin(), FrequentedBlock(loopBody));

            UpsilonValue* exitSum = loopExit->appendNew<UpsilonValue>(proc, Origin(), newBodySum);
            loopExit->appendNewControlValue(proc, Jump, Origin(), FrequentedBlock(continuation));

            UpsilonValue* skipSum = loopSkip->appendNew<UpsilonValue>(proc, Origin(), startSum);
            loopSkip->appendNewControlValue(proc, Jump, Origin(), FrequentedBlock(continuation));

            Value* finalSum = continuation->appendNew<Value>(proc, Phi, Int32, Origin());
            exitSum->setPhi(finalSum);
            skipSum->setPhi(finalSum);

            current = continuation;
            vars[((i >> 1) + 0) % numVars] = finalSum;
        }
    }

    current->appendNewControlValue(proc, Return, Origin(), vars[0]);

    compileProc(proc);

    MonotonicTime after = MonotonicTime::now();
    dataLog(toCString("    That took ", (after - before).milliseconds(), " ms.\n"));
}

void testBranchBitTest32TmpImm(uint32_t value, uint32_t imm)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* thenCase = proc.addBlock();
    BasicBlock* elseCase = proc.addBlock();

    Value* testValue = root->appendNew<Value>(
        proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* bitOffset = root->appendNew<Const32Value>(proc, Origin(), imm);

    Value* one = root->appendNew<Const32Value>(proc, Origin(), 1);
    Value* bitTest = root->appendNew<Value>(
        proc, BitAnd, Origin(),
        root->appendNew<Value>(proc, SShr, Origin(), testValue, bitOffset),
        one);

    root->appendNewControlValue(
        proc, Branch, Origin(),
        bitTest,
        FrequentedBlock(thenCase), FrequentedBlock(elseCase));

    thenCase->appendNewControlValue(
        proc, Return, Origin(),
        thenCase->appendNew<Const32Value>(proc, Origin(), 1));

    elseCase->appendNewControlValue(
        proc, Return, Origin(),
        elseCase->appendNew<Const32Value>(proc, Origin(), 0));

    auto code = compileProc(proc);
    CHECK_EQ(invoke<uint32_t>(*code, value), (value>>(imm%32))&1);
}

void testBranchBitTest32AddrImm(uint32_t value, uint32_t imm)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* thenCase = proc.addBlock();
    BasicBlock* elseCase = proc.addBlock();

    Value* testValue = root->appendNew<MemoryValue>(
        proc, Load, Int32, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* bitOffset = root->appendNew<Const32Value>(proc, Origin(), imm);

    Value* one = root->appendNew<Const32Value>(proc, Origin(), 1);
    Value* bitTest = root->appendNew<Value>(
        proc, BitAnd, Origin(),
        root->appendNew<Value>(proc, SShr, Origin(), testValue, bitOffset),
        one);

    root->appendNewControlValue(
        proc, Branch, Origin(),
        bitTest,
        FrequentedBlock(thenCase), FrequentedBlock(elseCase));

    thenCase->appendNewControlValue(
        proc, Return, Origin(),
        thenCase->appendNew<Const32Value>(proc, Origin(), 1));

    elseCase->appendNewControlValue(
        proc, Return, Origin(),
        elseCase->appendNew<Const32Value>(proc, Origin(), 0));

    auto code = compileProc(proc);
    CHECK_EQ(invoke<uint32_t>(*code, &value), (value>>(imm%32))&1);
}

void testBranchBitTest32TmpTmp(uint32_t value, uint32_t value2)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* thenCase = proc.addBlock();
    BasicBlock* elseCase = proc.addBlock();

    Value* testValue = root->appendNew<Value>(
        proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* bitOffset = root->appendNew<Value>(
        proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1));

    Value* one = root->appendNew<Const32Value>(proc, Origin(), 1);
    Value* bitTest = root->appendNew<Value>(
        proc, BitAnd, Origin(),
        root->appendNew<Value>(proc, SShr, Origin(), testValue, bitOffset),
        one);

    root->appendNewControlValue(
        proc, Branch, Origin(),
        bitTest,
        FrequentedBlock(thenCase), FrequentedBlock(elseCase));

    thenCase->appendNewControlValue(
        proc, Return, Origin(),
        thenCase->appendNew<Const32Value>(proc, Origin(), 1));

    elseCase->appendNewControlValue(
        proc, Return, Origin(),
        elseCase->appendNew<Const32Value>(proc, Origin(), 0));

    auto code = compileProc(proc);
    CHECK_EQ(invoke<uint32_t>(*code, value, value2), (value>>(value2%32))&1);
}

void testBranchBitTest64TmpTmp(uint64_t value, uint64_t value2)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* thenCase = proc.addBlock();
    BasicBlock* elseCase = proc.addBlock();

    Value* testValue = root->appendNew<Value>(proc, BitXor, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
        root->appendNew<Const64Value>(proc, Origin(), -1l));
    Value* bitOffset = root->appendNew<Value>(
        proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1));

    Value* one = root->appendNew<Const64Value>(proc, Origin(), 1);
    Value* bitTest = root->appendNew<Value>(
        proc, BitAnd, Origin(),
        testValue,
        root->appendNew<Value>(proc, Shl, Origin(), one, bitOffset));

    root->appendNewControlValue(
        proc, Branch, Origin(),
        bitTest,
        FrequentedBlock(thenCase), FrequentedBlock(elseCase));

    thenCase->appendNewControlValue(
        proc, Return, Origin(),
        thenCase->appendNew<Const64Value>(proc, Origin(), 0));

    elseCase->appendNewControlValue(
        proc, Return, Origin(),
        elseCase->appendNew<Const64Value>(proc, Origin(), 1));

    auto code = compileProc(proc);
    CHECK_EQ(invoke<uint64_t>(*code, value, value2), (value>>(value2%64))&1);
}

void testBranchBitTest64AddrTmp(uint64_t value, uint64_t value2)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* thenCase = proc.addBlock();
    BasicBlock* elseCase = proc.addBlock();

    Value* testValue = root->appendNew<MemoryValue>(
        proc, Load, Int64, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* bitOffset = root->appendNew<Value>(
        proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1));

    Value* one = root->appendNew<Const64Value>(proc, Origin(), 1);
    Value* bitTest = root->appendNew<Value>(
        proc, BitAnd, Origin(),
        testValue,
        root->appendNew<Value>(proc, Shl, Origin(), one, bitOffset));

    root->appendNewControlValue(
        proc, Branch, Origin(),
        bitTest,
        FrequentedBlock(thenCase), FrequentedBlock(elseCase));

    thenCase->appendNewControlValue(
        proc, Return, Origin(),
        thenCase->appendNew<Const64Value>(proc, Origin(), 1));

    elseCase->appendNewControlValue(
        proc, Return, Origin(),
        elseCase->appendNew<Const64Value>(proc, Origin(), 0));

    auto code = compileProc(proc);
    CHECK_EQ(invoke<uint64_t>(*code, &value, value2), (value>>(value2%64))&1);
}

void testBranchBitTestNegation(uint64_t value, uint64_t value2)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* thenCase = proc.addBlock();
    BasicBlock* elseCase = proc.addBlock();

    Value* testValue = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    Value* bitOffset = root->appendNew<Value>(
        proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1));
    Value* shift = root->appendNew<Value>(proc, SShr, Origin(), testValue, bitOffset);

    Value* one = root->appendNew<Const64Value>(proc, Origin(), 1);
    Value* bitTest = root->appendNew<Value>(
        proc, BitAnd, Origin(),
        root->appendNew<Value>(proc, BitXor, Origin(), shift, root->appendNew<Const64Value>(proc, Origin(), -1l)),
        one);

    root->appendNewControlValue(
        proc, Branch, Origin(),
        bitTest,
        FrequentedBlock(thenCase), FrequentedBlock(elseCase));

    thenCase->appendNewControlValue(
        proc, Return, Origin(),
        thenCase->appendNew<Const64Value>(proc, Origin(), 0));

    elseCase->appendNewControlValue(
        proc, Return, Origin(),
        elseCase->appendNew<Const64Value>(proc, Origin(), 1));

    auto code = compileProc(proc);
    CHECK_EQ(invoke<uint64_t>(*code, value, value2), (value>>(value2%64))&1);
}

void testBranchBitTestNegation2(uint64_t value, uint64_t value2)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* thenCase = proc.addBlock();
    BasicBlock* elseCase = proc.addBlock();

    Value* testValue = root->appendNew<Value>(proc, BitXor, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
        root->appendNew<Const64Value>(proc, Origin(), -1l));
    Value* bitOffset = root->appendNew<Value>(
        proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1));
    Value* shift = root->appendNew<Value>(proc, SShr, Origin(), testValue, bitOffset);

    Value* one = root->appendNew<Const64Value>(proc, Origin(), 1);
    Value* bitTest = root->appendNew<Value>(
        proc, BitAnd, Origin(),
        shift,
        one);

    root->appendNewControlValue(
        proc, Branch, Origin(),
        bitTest,
        FrequentedBlock(thenCase), FrequentedBlock(elseCase));

    thenCase->appendNewControlValue(
        proc, Return, Origin(),
        thenCase->appendNew<Const64Value>(proc, Origin(), 0));

    elseCase->appendNewControlValue(
        proc, Return, Origin(),
        elseCase->appendNew<Const64Value>(proc, Origin(), 1));

    auto code = compileProc(proc);
    CHECK_EQ(invoke<uint64_t>(*code, value, value2), (value>>(value2%64))&1);
}

void testSimplePatchpoint()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* arg1 = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    Value* arg2 = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
    PatchpointValue* patchpoint = root->appendNew<PatchpointValue>(proc, Int32, Origin());
    patchpoint->append(ConstrainedValue(arg1, ValueRep::SomeRegister));
    patchpoint->append(ConstrainedValue(arg2, ValueRep::SomeRegister));
    patchpoint->setGenerator(
        [&] (CCallHelpers& jit, const StackmapGenerationParams& params) {
            AllowMacroScratchRegisterUsage allowScratch(jit);
            CHECK(params.size() == 3);
            CHECK(params[0].isGPR());
            CHECK(params[1].isGPR());
            CHECK(params[2].isGPR());
            add32(jit, params[1].gpr(), params[2].gpr(), params[0].gpr());
        });
    root->appendNewControlValue(proc, Return, Origin(), patchpoint);

    CHECK(compileAndRun<int>(proc, 1, 2) == 3);
}

void testSimplePatchpointWithoutOuputClobbersGPArgs()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* arg1 = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    Value* arg2 = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
    Value* const1 = root->appendNew<Const64Value>(proc, Origin(), 42);
    Value* const2 = root->appendNew<Const64Value>(proc, Origin(), 13);

    PatchpointValue* patchpoint = root->appendNew<PatchpointValue>(proc, Void, Origin());
    patchpoint->clobberLate(RegisterSet(GPRInfo::argumentGPR0, GPRInfo::argumentGPR1));
    patchpoint->append(ConstrainedValue(const1, ValueRep::SomeRegister));
    patchpoint->append(ConstrainedValue(const2, ValueRep::SomeRegister));
    patchpoint->setGenerator(
        [&] (CCallHelpers& jit, const StackmapGenerationParams& params) {
            AllowMacroScratchRegisterUsage allowScratch(jit);
            CHECK(params.size() == 2);
            CHECK(params[0].isGPR());
            CHECK(params[1].isGPR());
            jit.move(CCallHelpers::TrustedImm32(0x00ff00ff), params[0].gpr());
            jit.move(CCallHelpers::TrustedImm32(0x00ff00ff), params[1].gpr());
            jit.move(CCallHelpers::TrustedImm32(0x00ff00ff), GPRInfo::argumentGPR0);
            jit.move(CCallHelpers::TrustedImm32(0x00ff00ff), GPRInfo::argumentGPR1);
        });

    Value* result = root->appendNew<Value>(proc, Add, Origin(), arg1, arg2);
    root->appendNewControlValue(proc, Return, Origin(), result);

    CHECK(compileAndRun<int>(proc, 1, 2) == 3);
}

void testSimplePatchpointWithOuputClobbersGPArgs()
{
    // We can't predict where the output will be but we want to be sure it is not
    // one of the clobbered registers which is a bit hard to test.
    //
    // What we do is force the hand of our register allocator by clobbering absolutely
    // everything but 1. The only valid allocation is to give it to the result and
    // spill everything else.

    Procedure proc;
    if (proc.optLevel() < 1) {
        // FIXME: Air O0 allocator can't handle such programs. We rely on WasmAirIRGenerator
        // to not use any such constructs where the register allocator is cornered in such
        // a way.
        // https://bugs.webkit.org/show_bug.cgi?id=194633
        return;
    }
    BasicBlock* root = proc.addBlock();
    Value* arg1 = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    Value* arg2 = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
    Value* const1 = root->appendNew<Const64Value>(proc, Origin(), 42);
    Value* const2 = root->appendNew<Const64Value>(proc, Origin(), 13);

    PatchpointValue* patchpoint = root->appendNew<PatchpointValue>(proc, Int64, Origin());

    RegisterSet clobberAll = RegisterSet::allGPRs();
    clobberAll.exclude(RegisterSet::stackRegisters());
    clobberAll.exclude(RegisterSet::reservedHardwareRegisters());
    clobberAll.clear(GPRInfo::argumentGPR2);
    patchpoint->clobberLate(clobberAll);

    patchpoint->append(ConstrainedValue(const1, ValueRep::SomeRegister));
    patchpoint->append(ConstrainedValue(const2, ValueRep::SomeRegister));

    patchpoint->setGenerator(
        [&] (CCallHelpers& jit, const StackmapGenerationParams& params) {
            AllowMacroScratchRegisterUsage allowScratch(jit);
            CHECK(params.size() == 3);
            CHECK(params[0].isGPR());
            CHECK(params[1].isGPR());
            CHECK(params[2].isGPR());
            jit.move(params[1].gpr(), params[0].gpr());
            jit.add64(params[2].gpr(), params[0].gpr());

            clobberAll.forEach([&] (Reg reg) {
                jit.move(CCallHelpers::TrustedImm32(0x00ff00ff), reg.gpr());
            });
        });

    Value* result = root->appendNew<Value>(proc, Add, Origin(), patchpoint,
        root->appendNew<Value>(proc, Add, Origin(), arg1, arg2));
    root->appendNewControlValue(proc, Return, Origin(), result);

    CHECK(compileAndRun<int>(proc, 1, 2) == 58);
}

void testSimplePatchpointWithoutOuputClobbersFPArgs()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* arg1 = root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR0);
    Value* arg2 = root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR1);
    Value* const1 = root->appendNew<ConstDoubleValue>(proc, Origin(), 42.5);
    Value* const2 = root->appendNew<ConstDoubleValue>(proc, Origin(), 13.1);

    PatchpointValue* patchpoint = root->appendNew<PatchpointValue>(proc, Void, Origin());
    patchpoint->clobberLate(RegisterSet(FPRInfo::argumentFPR0, FPRInfo::argumentFPR1));
    patchpoint->append(ConstrainedValue(const1, ValueRep::SomeRegister));
    patchpoint->append(ConstrainedValue(const2, ValueRep::SomeRegister));
    patchpoint->setGenerator(
        [&] (CCallHelpers& jit, const StackmapGenerationParams& params) {
            AllowMacroScratchRegisterUsage allowScratch(jit);
            CHECK(params.size() == 2);
            CHECK(params[0].isFPR());
            CHECK(params[1].isFPR());
            jit.moveZeroToDouble(params[0].fpr());
            jit.moveZeroToDouble(params[1].fpr());
            jit.moveZeroToDouble(FPRInfo::argumentFPR0);
            jit.moveZeroToDouble(FPRInfo::argumentFPR1);
        });

    Value* result = root->appendNew<Value>(proc, Add, Origin(), arg1, arg2);
    root->appendNewControlValue(proc, Return, Origin(), result);

    CHECK(compileAndRun<double>(proc, 1.5, 2.5) == 4);
}

void testSimplePatchpointWithOuputClobbersFPArgs()
{
    Procedure proc;
    if (proc.optLevel() < 1) {
        // FIXME: Air O0 allocator can't handle such programs. We rely on WasmAirIRGenerator
        // to not use any such constructs where the register allocator is cornered in such
        // a way.
        // https://bugs.webkit.org/show_bug.cgi?id=194633
        return;
    }
    BasicBlock* root = proc.addBlock();
    Value* arg1 = root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR0);
    Value* arg2 = root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR1);
    Value* const1 = root->appendNew<ConstDoubleValue>(proc, Origin(), 42.5);
    Value* const2 = root->appendNew<ConstDoubleValue>(proc, Origin(), 13.1);

    PatchpointValue* patchpoint = root->appendNew<PatchpointValue>(proc, Double, Origin());

    RegisterSet clobberAll = RegisterSet::allFPRs();
    clobberAll.exclude(RegisterSet::stackRegisters());
    clobberAll.exclude(RegisterSet::reservedHardwareRegisters());
    clobberAll.clear(FPRInfo::argumentFPR2);
    patchpoint->clobberLate(clobberAll);

    patchpoint->append(ConstrainedValue(const1, ValueRep::SomeRegister));
    patchpoint->append(ConstrainedValue(const2, ValueRep::SomeRegister));

    patchpoint->setGenerator(
        [&] (CCallHelpers& jit, const StackmapGenerationParams& params) {
            AllowMacroScratchRegisterUsage allowScratch(jit);
            CHECK(params.size() == 3);
            CHECK(params[0].isFPR());
            CHECK(params[1].isFPR());
            CHECK(params[2].isFPR());
            jit.addDouble(params[1].fpr(), params[2].fpr(), params[0].fpr());

            clobberAll.forEach([&] (Reg reg) {
                jit.moveZeroToDouble(reg.fpr());
            });
        });

    Value* result = root->appendNew<Value>(proc, Add, Origin(), patchpoint,
        root->appendNew<Value>(proc, Add, Origin(), arg1, arg2));
    root->appendNewControlValue(proc, Return, Origin(), result);

    CHECK(compileAndRun<double>(proc, 1.5, 2.5) == 59.6);
}

void testPatchpointWithEarlyClobber()
{
    auto test = [] (GPRReg registerToClobber, bool arg1InArgGPR, bool arg2InArgGPR) {
        Procedure proc;
        BasicBlock* root = proc.addBlock();
        Value* arg1 = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
        Value* arg2 = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
    
        PatchpointValue* patchpoint = root->appendNew<PatchpointValue>(proc, Int32, Origin());
        patchpoint->append(ConstrainedValue(arg1, ValueRep::SomeRegister));
        patchpoint->append(ConstrainedValue(arg2, ValueRep::SomeRegister));
        patchpoint->clobberEarly(RegisterSet(registerToClobber));
        unsigned optLevel = proc.optLevel();
        patchpoint->setGenerator(
            [&] (CCallHelpers& jit, const StackmapGenerationParams& params) {
                if (optLevel > 1) {
                    CHECK((params[1].gpr() == GPRInfo::argumentGPR0) == arg1InArgGPR);
                    CHECK((params[2].gpr() == GPRInfo::argumentGPR1) == arg2InArgGPR);
                }
            
                add32(jit, params[1].gpr(), params[2].gpr(), params[0].gpr());
            });

        root->appendNewControlValue(proc, Return, Origin(), patchpoint);

        CHECK(compileAndRun<int>(proc, 1, 2) == 3);
    };

    test(GPRInfo::nonArgGPR0, true, true);
    test(GPRInfo::argumentGPR0, false, true);
    test(GPRInfo::argumentGPR1, true, false);
}

void testPatchpointCallArg()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* arg1 = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    Value* arg2 = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
    PatchpointValue* patchpoint = root->appendNew<PatchpointValue>(proc, Int32, Origin());
    patchpoint->append(ConstrainedValue(arg1, ValueRep::stackArgument(0)));
    patchpoint->append(ConstrainedValue(arg2, ValueRep::stackArgument(8)));
    patchpoint->setGenerator(
        [&] (CCallHelpers& jit, const StackmapGenerationParams& params) {
            AllowMacroScratchRegisterUsage allowScratch(jit);
            CHECK(params.size() == 3);
            CHECK(params[0].isGPR());
            CHECK(params[1].isStack());
            CHECK(params[2].isStack());
            jit.load32(
                CCallHelpers::Address(GPRInfo::callFrameRegister, params[1].offsetFromFP()),
                params[0].gpr());
            jit.add32(
                CCallHelpers::Address(GPRInfo::callFrameRegister, params[2].offsetFromFP()),
                params[0].gpr());
        });
    root->appendNewControlValue(proc, Return, Origin(), patchpoint);

    CHECK(compileAndRun<int>(proc, 1, 2) == 3);
}

void testPatchpointFixedRegister()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* arg1 = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    Value* arg2 = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
    PatchpointValue* patchpoint = root->appendNew<PatchpointValue>(proc, Int32, Origin());
    patchpoint->append(ConstrainedValue(arg1, ValueRep(GPRInfo::regT0)));
    patchpoint->append(ConstrainedValue(arg2, ValueRep(GPRInfo::regT1)));
    patchpoint->setGenerator(
        [&] (CCallHelpers& jit, const StackmapGenerationParams& params) {
            AllowMacroScratchRegisterUsage allowScratch(jit);
            CHECK(params.size() == 3);
            CHECK(params[0].isGPR());
            CHECK(params[1] == ValueRep(GPRInfo::regT0));
            CHECK(params[2] == ValueRep(GPRInfo::regT1));
            add32(jit, GPRInfo::regT0, GPRInfo::regT1, params[0].gpr());
        });
    root->appendNewControlValue(proc, Return, Origin(), patchpoint);

    CHECK(compileAndRun<int>(proc, 1, 2) == 3);
}

void testPatchpointAny(ValueRep rep)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* arg1 = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    Value* arg2 = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
    PatchpointValue* patchpoint = root->appendNew<PatchpointValue>(proc, Int32, Origin());
    patchpoint->append(ConstrainedValue(arg1, rep));
    patchpoint->append(ConstrainedValue(arg2, rep));
    patchpoint->setGenerator(
        [&] (CCallHelpers& jit, const StackmapGenerationParams& params) {
            AllowMacroScratchRegisterUsage allowScratch(jit);
            // We shouldn't have spilled the inputs, so we assert that they're in registers.
            CHECK(params.size() == 3);
            CHECK(params[0].isGPR());
            CHECK(params[1].isGPR());
            CHECK(params[2].isGPR());
            add32(jit, params[1].gpr(), params[2].gpr(), params[0].gpr());
        });
    root->appendNewControlValue(proc, Return, Origin(), patchpoint);

    CHECK(compileAndRun<int>(proc, 1, 2) == 3);
}

void testPatchpointGPScratch()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* arg1 = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    Value* arg2 = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
    PatchpointValue* patchpoint = root->appendNew<PatchpointValue>(proc, Int32, Origin());
    patchpoint->append(arg1, ValueRep::SomeRegister);
    patchpoint->append(arg2, ValueRep::SomeRegister);
    patchpoint->numGPScratchRegisters = 2;
    patchpoint->setGenerator(
        [&] (CCallHelpers& jit, const StackmapGenerationParams& params) {
            AllowMacroScratchRegisterUsage allowScratch(jit);
            // We shouldn't have spilled the inputs, so we assert that they're in registers.
            CHECK(params.size() == 3);
            CHECK(params[0].isGPR());
            CHECK(params[1].isGPR());
            CHECK(params[2].isGPR());
            CHECK(params.gpScratch(0) != InvalidGPRReg);
            CHECK(params.gpScratch(0) != params[0].gpr());
            CHECK(params.gpScratch(0) != params[1].gpr());
            CHECK(params.gpScratch(0) != params[2].gpr());
            CHECK(params.gpScratch(1) != InvalidGPRReg);
            CHECK(params.gpScratch(1) != params.gpScratch(0));
            CHECK(params.gpScratch(1) != params[0].gpr());
            CHECK(params.gpScratch(1) != params[1].gpr());
            CHECK(params.gpScratch(1) != params[2].gpr());
            CHECK(!params.unavailableRegisters().get(params.gpScratch(0)));
            CHECK(!params.unavailableRegisters().get(params.gpScratch(1)));
            add32(jit, params[1].gpr(), params[2].gpr(), params[0].gpr());
        });
    root->appendNewControlValue(proc, Return, Origin(), patchpoint);

    CHECK(compileAndRun<int>(proc, 1, 2) == 3);
}

void testPatchpointFPScratch()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* arg1 = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    Value* arg2 = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
    PatchpointValue* patchpoint = root->appendNew<PatchpointValue>(proc, Int32, Origin());
    patchpoint->append(arg1, ValueRep::SomeRegister);
    patchpoint->append(arg2, ValueRep::SomeRegister);
    patchpoint->numFPScratchRegisters = 2;
    patchpoint->setGenerator(
        [&] (CCallHelpers& jit, const StackmapGenerationParams& params) {
            AllowMacroScratchRegisterUsage allowScratch(jit);
            // We shouldn't have spilled the inputs, so we assert that they're in registers.
            CHECK(params.size() == 3);
            CHECK(params[0].isGPR());
            CHECK(params[1].isGPR());
            CHECK(params[2].isGPR());
            CHECK(params.fpScratch(0) != InvalidFPRReg);
            CHECK(params.fpScratch(1) != InvalidFPRReg);
            CHECK(params.fpScratch(1) != params.fpScratch(0));
            CHECK(!params.unavailableRegisters().get(params.fpScratch(0)));
            CHECK(!params.unavailableRegisters().get(params.fpScratch(1)));
            add32(jit, params[1].gpr(), params[2].gpr(), params[0].gpr());
        });
    root->appendNewControlValue(proc, Return, Origin(), patchpoint);

    CHECK(compileAndRun<int>(proc, 1, 2) == 3);
}

void testPatchpointLotsOfLateAnys()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Vector<int> things;
    for (unsigned i = 200; i--;)
        things.append(i);

    Vector<Value*> values;
    for (int& thing : things) {
        Value* value = root->appendNew<MemoryValue>(
            proc, Load, Int32, Origin(),
            root->appendNew<ConstPtrValue>(proc, Origin(), &thing));
        values.append(value);
    }

    PatchpointValue* patchpoint = root->appendNew<PatchpointValue>(proc, Int32, Origin());
    patchpoint->clobber(RegisterSet::macroScratchRegisters());
    for (Value* value : values)
        patchpoint->append(ConstrainedValue(value, ValueRep::LateColdAny));
    patchpoint->setGenerator(
        [&] (CCallHelpers& jit, const StackmapGenerationParams& params) {
            AllowMacroScratchRegisterUsage allowScratch(jit);
            // We shouldn't have spilled the inputs, so we assert that they're in registers.
            CHECK(params.size() == things.size() + 1);
            CHECK(params[0].isGPR());
            jit.move(CCallHelpers::TrustedImm32(0), params[0].gpr());
            for (unsigned i = 1; i < params.size(); ++i) {
                if (params[i].isGPR()) {
                    CHECK(params[i] != params[0]);
                    jit.add32(params[i].gpr(), params[0].gpr());
                } else {
                    CHECK(params[i].isStack());
                    jit.add32(CCallHelpers::Address(GPRInfo::callFrameRegister, params[i].offsetFromFP()), params[0].gpr());
                }
            }
        });
    root->appendNewControlValue(proc, Return, Origin(), patchpoint);

    CHECK(static_cast<size_t>(compileAndRun<int>(proc)) == (things.size() * (things.size() - 1)) / 2);
}

void testPatchpointAnyImm(ValueRep rep)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* arg1 = root->appendNew<Value>(
        proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* arg2 = root->appendNew<Const32Value>(proc, Origin(), 42);
    PatchpointValue* patchpoint = root->appendNew<PatchpointValue>(proc, Int32, Origin());
    patchpoint->append(ConstrainedValue(arg1, rep));
    patchpoint->append(ConstrainedValue(arg2, rep));
    patchpoint->setGenerator(
        [&] (CCallHelpers& jit, const StackmapGenerationParams& params) {
            AllowMacroScratchRegisterUsage allowScratch(jit);
            CHECK(params.size() == 3);
            CHECK(params[0].isGPR());
            CHECK(params[1].isGPR());
            CHECK(params[2].isConstant());
            CHECK(params[2].value() == 42);
            jit.add32(
                CCallHelpers::TrustedImm32(static_cast<int32_t>(params[2].value())),
                params[1].gpr(), params[0].gpr());
        });
    root->appendNewControlValue(proc, Return, Origin(), patchpoint);

    CHECK(compileAndRun<int>(proc, 1) == 43);
}

void addSExtTests(const char* filter, Deque<RefPtr<SharedTask<void()>>>& tasks)
{
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
}

#endif // ENABLE(B3_JIT)
