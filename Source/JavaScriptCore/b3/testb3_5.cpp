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

#if ENABLE(B3_JIT)

void testPatchpointManyWarmAnyImms()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* arg1 = root->appendNew<Const32Value>(proc, Origin(), 42);
    Value* arg2 = root->appendNew<Const64Value>(proc, Origin(), 43);
    Value* arg3 = root->appendNew<Const64Value>(proc, Origin(), 43000000000000ll);
    Value* arg4 = root->appendNew<ConstDoubleValue>(proc, Origin(), 42.5);
    Value* arg5 = root->appendNew<ConstFloatValue>(proc, Origin(), -42.5f);
    PatchpointValue* patchpoint = root->appendNew<PatchpointValue>(proc, Void, Origin());
    patchpoint->append(ConstrainedValue(arg1, ValueRep::WarmAny));
    patchpoint->append(ConstrainedValue(arg2, ValueRep::WarmAny));
    patchpoint->append(ConstrainedValue(arg3, ValueRep::WarmAny));
    patchpoint->append(ConstrainedValue(arg4, ValueRep::WarmAny));
    patchpoint->append(ConstrainedValue(arg5, ValueRep::WarmAny));
    patchpoint->setGenerator(
        [&] (CCallHelpers&, const StackmapGenerationParams& params) {
            CHECK(params.size() == 5);
            CHECK(params[0] == ValueRep::constant(42));
            CHECK(params[1] == ValueRep::constant(43));
            CHECK(params[2] == ValueRep::constant(43000000000000ll));
            CHECK(params[3] == ValueRep::constantDouble(42.5));
            CHECK(params[4] == ValueRep::constantFloat(-42.5f));
            CHECK(params[4].floatValue() == -42.5f);
        });
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Const32Value>(proc, Origin(), 0));

    CHECK(!compileAndRun<int>(proc));
}

void testPatchpointManyColdAnyImms()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* arg1 = root->appendNew<Const32Value>(proc, Origin(), 42);
    Value* arg2 = root->appendNew<Const64Value>(proc, Origin(), 43);
    Value* arg3 = root->appendNew<Const64Value>(proc, Origin(), 43000000000000ll);
    Value* arg4 = root->appendNew<ConstDoubleValue>(proc, Origin(), 42.5);
    Value* arg5 = root->appendNew<ConstFloatValue>(proc, Origin(), -42.5f);
    PatchpointValue* patchpoint = root->appendNew<PatchpointValue>(proc, Void, Origin());
    patchpoint->append(ConstrainedValue(arg1, ValueRep::ColdAny));
    patchpoint->append(ConstrainedValue(arg2, ValueRep::ColdAny));
    patchpoint->append(ConstrainedValue(arg3, ValueRep::ColdAny));
    patchpoint->append(ConstrainedValue(arg4, ValueRep::ColdAny));
    patchpoint->append(ConstrainedValue(arg5, ValueRep::ColdAny));
    patchpoint->setGenerator(
        [&] (CCallHelpers&, const StackmapGenerationParams& params) {
            CHECK(params.size() == 5);
            CHECK(params[0] == ValueRep::constant(42));
            CHECK(params[1] == ValueRep::constant(43));
            CHECK(params[2] == ValueRep::constant(43000000000000ll));
            CHECK(params[3] == ValueRep::constantDouble(42.5));
            CHECK(params[4] == ValueRep::constantFloat(-42.5f));
            CHECK(params[4].floatValue() == -42.5f);
        });
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Const32Value>(proc, Origin(), 0));

    CHECK(!compileAndRun<int>(proc));
}

void testPatchpointWithRegisterResult()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* arg1 = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    Value* arg2 = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
    PatchpointValue* patchpoint = root->appendNew<PatchpointValue>(proc, Int32, Origin());
    patchpoint->append(ConstrainedValue(arg1, ValueRep::SomeRegister));
    patchpoint->append(ConstrainedValue(arg2, ValueRep::SomeRegister));
    patchpoint->resultConstraints = { ValueRep::reg(GPRInfo::nonArgGPR0) };
    patchpoint->setGenerator(
        [&] (CCallHelpers& jit, const StackmapGenerationParams& params) {
            AllowMacroScratchRegisterUsage allowScratch(jit);
            CHECK(params.size() == 3);
            CHECK(params[0] == ValueRep::reg(GPRInfo::nonArgGPR0));
            CHECK(params[1].isGPR());
            CHECK(params[2].isGPR());
            add32(jit, params[1].gpr(), params[2].gpr(), GPRInfo::nonArgGPR0);
        });
    root->appendNewControlValue(proc, Return, Origin(), patchpoint);

    CHECK(compileAndRun<int>(proc, 1, 2) == 3);
}

void testPatchpointWithStackArgumentResult()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* arg1 = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    Value* arg2 = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
    PatchpointValue* patchpoint = root->appendNew<PatchpointValue>(proc, Int32, Origin());
    patchpoint->append(ConstrainedValue(arg1, ValueRep::SomeRegister));
    patchpoint->append(ConstrainedValue(arg2, ValueRep::SomeRegister));
    patchpoint->resultConstraints = { ValueRep::stackArgument(0) };
    patchpoint->clobber(RegisterSet::macroScratchRegisters());
    patchpoint->setGenerator(
        [&] (CCallHelpers& jit, const StackmapGenerationParams& params) {
            AllowMacroScratchRegisterUsage allowScratch(jit);
            CHECK(params.size() == 3);
            CHECK(params[0] == ValueRep::stack(-static_cast<intptr_t>(proc.frameSize())));
            CHECK(params[1].isGPR());
            CHECK(params[2].isGPR());
            jit.add32(params[1].gpr(), params[2].gpr(), jit.scratchRegister());
            jit.store32(jit.scratchRegister(), CCallHelpers::Address(CCallHelpers::stackPointerRegister, 0));
        });
    root->appendNewControlValue(proc, Return, Origin(), patchpoint);

    CHECK(compileAndRun<int>(proc, 1, 2) == 3);
}

void testPatchpointWithAnyResult()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* arg1 = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    Value* arg2 = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
    PatchpointValue* patchpoint = root->appendNew<PatchpointValue>(proc, Double, Origin());
    patchpoint->append(ConstrainedValue(arg1, ValueRep::SomeRegister));
    patchpoint->append(ConstrainedValue(arg2, ValueRep::SomeRegister));
    patchpoint->resultConstraints = { ValueRep::WarmAny };
    patchpoint->clobberLate(RegisterSet::allFPRs());
    patchpoint->clobber(RegisterSet::macroScratchRegisters());
    patchpoint->clobber(RegisterSet(GPRInfo::regT0));
    patchpoint->setGenerator(
        [&] (CCallHelpers& jit, const StackmapGenerationParams& params) {
            AllowMacroScratchRegisterUsage allowScratch(jit);
            CHECK(params.size() == 3);
            CHECK(params[0].isStack());
            CHECK(params[1].isGPR());
            CHECK(params[2].isGPR());
            add32(jit, params[1].gpr(), params[2].gpr(), GPRInfo::regT0);
            jit.convertInt32ToDouble(GPRInfo::regT0, FPRInfo::fpRegT0);
            jit.storeDouble(FPRInfo::fpRegT0, CCallHelpers::Address(GPRInfo::callFrameRegister, params[0].offsetFromFP()));
        });
    root->appendNewControlValue(proc, Return, Origin(), patchpoint);

    CHECK(compileAndRun<double>(proc, 1, 2) == 3);
}

void testSimpleCheck()
{
    Procedure proc;
    if (proc.optLevel() < 1)
        return;
    BasicBlock* root = proc.addBlock();
    Value* arg = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    CheckValue* check = root->appendNew<CheckValue>(proc, Check, Origin(), arg);
    check->setGenerator(
        [&] (CCallHelpers& jit, const StackmapGenerationParams& params) {
            AllowMacroScratchRegisterUsage allowScratch(jit);
            CHECK(!params.size());

            // This should always work because a function this simple should never have callee
            // saves.
            jit.move(CCallHelpers::TrustedImm32(42), GPRInfo::returnValueGPR);
            jit.emitFunctionEpilogue();
            jit.ret();
        });
    root->appendNewControlValue(
        proc, Return, Origin(), root->appendNew<Const32Value>(proc, Origin(), 0));

    auto code = compileProc(proc);

    CHECK(invoke<int>(*code, 0) == 0);
    CHECK(invoke<int>(*code, 1) == 42);
}

void testCheckFalse()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    CheckValue* check = root->appendNew<CheckValue>(
        proc, Check, Origin(), root->appendNew<Const32Value>(proc, Origin(), 0));
    unsigned optLevel = proc.optLevel();
    check->setGenerator(
        [&] (CCallHelpers&, const StackmapGenerationParams&) {
            if (optLevel > 1)
                CHECK(!"This should not have executed");
        });
    root->appendNewControlValue(
        proc, Return, Origin(), root->appendNew<Const32Value>(proc, Origin(), 0));

    auto code = compileProc(proc);

    CHECK(invoke<int>(*code) == 0);
}

void testCheckTrue()
{
    Procedure proc;
    if (proc.optLevel() < 1)
        return;
    BasicBlock* root = proc.addBlock();
    CheckValue* check = root->appendNew<CheckValue>(
        proc, Check, Origin(), root->appendNew<Const32Value>(proc, Origin(), 1));
    unsigned optLevel = proc.optLevel();
    check->setGenerator(
        [&] (CCallHelpers& jit, const StackmapGenerationParams& params) {
            AllowMacroScratchRegisterUsage allowScratch(jit);
            if (optLevel > 1)
                CHECK(params.value()->opcode() == Patchpoint);
            CHECK(!params.size());

            // This should always work because a function this simple should never have callee
            // saves.
            jit.move(CCallHelpers::TrustedImm32(42), GPRInfo::returnValueGPR);
            jit.emitFunctionEpilogue();
            jit.ret();
        });
    root->appendNewControlValue(
        proc, Return, Origin(), root->appendNew<Const32Value>(proc, Origin(), 0));

    auto code = compileProc(proc);

    CHECK(invoke<int>(*code) == 42);
}

void testCheckLessThan()
{
    Procedure proc;
    if (proc.optLevel() < 1)
        return;
    BasicBlock* root = proc.addBlock();
    Value* arg = root->appendNew<Value>(
        proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    CheckValue* check = root->appendNew<CheckValue>(
        proc, Check, Origin(),
        root->appendNew<Value>(
            proc, LessThan, Origin(), arg,
            root->appendNew<Const32Value>(proc, Origin(), 42)));
    check->setGenerator(
        [&] (CCallHelpers& jit, const StackmapGenerationParams& params) {
            AllowMacroScratchRegisterUsage allowScratch(jit);
            CHECK(!params.size());

            // This should always work because a function this simple should never have callee
            // saves.
            jit.move(CCallHelpers::TrustedImm32(42), GPRInfo::returnValueGPR);
            jit.emitFunctionEpilogue();
            jit.ret();
        });
    root->appendNewControlValue(
        proc, Return, Origin(), root->appendNew<Const32Value>(proc, Origin(), 0));

    auto code = compileProc(proc);

    CHECK(invoke<int>(*code, 42) == 0);
    CHECK(invoke<int>(*code, 1000) == 0);
    CHECK(invoke<int>(*code, 41) == 42);
    CHECK(invoke<int>(*code, 0) == 42);
    CHECK(invoke<int>(*code, -1) == 42);
}

void testCheckMegaCombo()
{
    Procedure proc;
    if (proc.optLevel() < 1)
        return;
    BasicBlock* root = proc.addBlock();
    Value* base = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    Value* index = root->appendNew<Value>(
        proc, ZExt32, Origin(),
        root->appendNew<Value>(
            proc, Trunc, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1)));

    Value* ptr = root->appendNew<Value>(
        proc, Add, Origin(), base,
        root->appendNew<Value>(
            proc, Shl, Origin(), index,
            root->appendNew<Const32Value>(proc, Origin(), 1)));

    CheckValue* check = root->appendNew<CheckValue>(
        proc, Check, Origin(),
        root->appendNew<Value>(
            proc, LessThan, Origin(),
            root->appendNew<MemoryValue>(proc, Load8S, Origin(), ptr),
            root->appendNew<Const32Value>(proc, Origin(), 42)));
    check->setGenerator(
        [&] (CCallHelpers& jit, const StackmapGenerationParams& params) {
            AllowMacroScratchRegisterUsage allowScratch(jit);
            CHECK(!params.size());

            // This should always work because a function this simple should never have callee
            // saves.
            jit.move(CCallHelpers::TrustedImm32(42), GPRInfo::returnValueGPR);
            jit.emitFunctionEpilogue();
            jit.ret();
        });
    root->appendNewControlValue(
        proc, Return, Origin(), root->appendNew<Const32Value>(proc, Origin(), 0));

    auto code = compileProc(proc);

    int8_t value;
    value = 42;
    CHECK(invoke<int>(*code, &value - 2, 1) == 0);
    value = 127;
    CHECK(invoke<int>(*code, &value - 2, 1) == 0);
    value = 41;
    CHECK(invoke<int>(*code, &value - 2, 1) == 42);
    value = 0;
    CHECK(invoke<int>(*code, &value - 2, 1) == 42);
    value = -1;
    CHECK(invoke<int>(*code, &value - 2, 1) == 42);
}

void testCheckTrickyMegaCombo()
{
    Procedure proc;
    if (proc.optLevel() < 1)
        return;
    BasicBlock* root = proc.addBlock();
    Value* base = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    Value* index = root->appendNew<Value>(
        proc, ZExt32, Origin(),
        root->appendNew<Value>(
            proc, Add, Origin(),
            root->appendNew<Value>(
                proc, Trunc, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1)),
            root->appendNew<Const32Value>(proc, Origin(), 1)));

    Value* ptr = root->appendNew<Value>(
        proc, Add, Origin(), base,
        root->appendNew<Value>(
            proc, Shl, Origin(), index,
            root->appendNew<Const32Value>(proc, Origin(), 1)));

    CheckValue* check = root->appendNew<CheckValue>(
        proc, Check, Origin(),
        root->appendNew<Value>(
            proc, LessThan, Origin(),
            root->appendNew<MemoryValue>(proc, Load8S, Origin(), ptr),
            root->appendNew<Const32Value>(proc, Origin(), 42)));
    check->setGenerator(
        [&] (CCallHelpers& jit, const StackmapGenerationParams& params) {
            AllowMacroScratchRegisterUsage allowScratch(jit);
            CHECK(!params.size());

            // This should always work because a function this simple should never have callee
            // saves.
            jit.move(CCallHelpers::TrustedImm32(42), GPRInfo::returnValueGPR);
            jit.emitFunctionEpilogue();
            jit.ret();
        });
    root->appendNewControlValue(
        proc, Return, Origin(), root->appendNew<Const32Value>(proc, Origin(), 0));

    auto code = compileProc(proc);

    int8_t value;
    value = 42;
    CHECK(invoke<int>(*code, &value - 2, 0) == 0);
    value = 127;
    CHECK(invoke<int>(*code, &value - 2, 0) == 0);
    value = 41;
    CHECK(invoke<int>(*code, &value - 2, 0) == 42);
    value = 0;
    CHECK(invoke<int>(*code, &value - 2, 0) == 42);
    value = -1;
    CHECK(invoke<int>(*code, &value - 2, 0) == 42);
}

void testCheckTwoMegaCombos()
{
    Procedure proc;
    if (proc.optLevel() < 1)
        return;
    BasicBlock* root = proc.addBlock();
    Value* base = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    Value* index = root->appendNew<Value>(
        proc, ZExt32, Origin(),
        root->appendNew<Value>(
            proc, Trunc, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1)));

    Value* ptr = root->appendNew<Value>(
        proc, Add, Origin(), base,
        root->appendNew<Value>(
            proc, Shl, Origin(), index,
            root->appendNew<Const32Value>(proc, Origin(), 1)));

    Value* predicate = root->appendNew<Value>(
        proc, LessThan, Origin(),
        root->appendNew<MemoryValue>(proc, Load8S, Origin(), ptr),
        root->appendNew<Const32Value>(proc, Origin(), 42));

    CheckValue* check = root->appendNew<CheckValue>(proc, Check, Origin(), predicate);
    check->setGenerator(
        [&] (CCallHelpers& jit, const StackmapGenerationParams& params) {
            AllowMacroScratchRegisterUsage allowScratch(jit);
            CHECK(!params.size());

            // This should always work because a function this simple should never have callee
            // saves.
            jit.move(CCallHelpers::TrustedImm32(42), GPRInfo::returnValueGPR);
            jit.emitFunctionEpilogue();
            jit.ret();
        });
    CheckValue* check2 = root->appendNew<CheckValue>(proc, Check, Origin(), predicate);
    check2->setGenerator(
        [&] (CCallHelpers& jit, const StackmapGenerationParams& params) {
            AllowMacroScratchRegisterUsage allowScratch(jit);
            CHECK(!params.size());

            // This should always work because a function this simple should never have callee
            // saves.
            jit.move(CCallHelpers::TrustedImm32(43), GPRInfo::returnValueGPR);
            jit.emitFunctionEpilogue();
            jit.ret();
        });
    root->appendNewControlValue(
        proc, Return, Origin(), root->appendNew<Const32Value>(proc, Origin(), 0));

    auto code = compileProc(proc);

    int8_t value;
    value = 42;
    CHECK(invoke<int>(*code, &value - 2, 1) == 0);
    value = 127;
    CHECK(invoke<int>(*code, &value - 2, 1) == 0);
    value = 41;
    CHECK(invoke<int>(*code, &value - 2, 1) == 42);
    value = 0;
    CHECK(invoke<int>(*code, &value - 2, 1) == 42);
    value = -1;
    CHECK(invoke<int>(*code, &value - 2, 1) == 42);
}

void testCheckTwoNonRedundantMegaCombos()
{
    Procedure proc;
    if (proc.optLevel() < 1)
        return;

    BasicBlock* root = proc.addBlock();
    BasicBlock* thenCase = proc.addBlock();
    BasicBlock* elseCase = proc.addBlock();

    Value* base = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    Value* index = root->appendNew<Value>(
        proc, ZExt32, Origin(),
        root->appendNew<Value>(
            proc, Trunc, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1)));
    Value* branchPredicate = root->appendNew<Value>(
        proc, BitAnd, Origin(),
        root->appendNew<Value>(
            proc, Trunc, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR2)),
        root->appendNew<Const32Value>(proc, Origin(), 0xff));

    Value* ptr = root->appendNew<Value>(
        proc, Add, Origin(), base,
        root->appendNew<Value>(
            proc, Shl, Origin(), index,
            root->appendNew<Const32Value>(proc, Origin(), 1)));

    Value* checkPredicate = root->appendNew<Value>(
        proc, LessThan, Origin(),
        root->appendNew<MemoryValue>(proc, Load8S, Origin(), ptr),
        root->appendNew<Const32Value>(proc, Origin(), 42));

    root->appendNewControlValue(
        proc, Branch, Origin(), branchPredicate,
        FrequentedBlock(thenCase), FrequentedBlock(elseCase));

    CheckValue* check = thenCase->appendNew<CheckValue>(proc, Check, Origin(), checkPredicate);
    check->setGenerator(
        [&] (CCallHelpers& jit, const StackmapGenerationParams& params) {
            AllowMacroScratchRegisterUsage allowScratch(jit);
            CHECK(!params.size());

            // This should always work because a function this simple should never have callee
            // saves.
            jit.move(CCallHelpers::TrustedImm32(42), GPRInfo::returnValueGPR);
            jit.emitFunctionEpilogue();
            jit.ret();
        });
    thenCase->appendNewControlValue(
        proc, Return, Origin(), thenCase->appendNew<Const32Value>(proc, Origin(), 43));

    CheckValue* check2 = elseCase->appendNew<CheckValue>(proc, Check, Origin(), checkPredicate);
    check2->setGenerator(
        [&] (CCallHelpers& jit, const StackmapGenerationParams& params) {
            AllowMacroScratchRegisterUsage allowScratch(jit);
            CHECK(!params.size());

            // This should always work because a function this simple should never have callee
            // saves.
            jit.move(CCallHelpers::TrustedImm32(44), GPRInfo::returnValueGPR);
            jit.emitFunctionEpilogue();
            jit.ret();
        });
    elseCase->appendNewControlValue(
        proc, Return, Origin(), elseCase->appendNew<Const32Value>(proc, Origin(), 45));

    auto code = compileProc(proc);

    int8_t value;

    value = 42;
    CHECK(invoke<int>(*code, &value - 2, 1, true) == 43);
    value = 127;
    CHECK(invoke<int>(*code, &value - 2, 1, true) == 43);
    value = 41;
    CHECK(invoke<int>(*code, &value - 2, 1, true) == 42);
    value = 0;
    CHECK(invoke<int>(*code, &value - 2, 1, true) == 42);
    value = -1;
    CHECK(invoke<int>(*code, &value - 2, 1, true) == 42);

    value = 42;
    CHECK(invoke<int>(*code, &value - 2, 1, false) == 45);
    value = 127;
    CHECK(invoke<int>(*code, &value - 2, 1, false) == 45);
    value = 41;
    CHECK(invoke<int>(*code, &value - 2, 1, false) == 44);
    value = 0;
    CHECK(invoke<int>(*code, &value - 2, 1, false) == 44);
    value = -1;
    CHECK(invoke<int>(*code, &value - 2, 1, false) == 44);
}

void testCheckAddImm()
{
    Procedure proc;
    if (proc.optLevel() < 1)
        return;
    BasicBlock* root = proc.addBlock();
    Value* arg1 = root->appendNew<Value>(
        proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* arg2 = root->appendNew<Const32Value>(proc, Origin(), 42);
    CheckValue* checkAdd = root->appendNew<CheckValue>(proc, CheckAdd, Origin(), arg1, arg2);
    checkAdd->append(arg1);
    checkAdd->append(arg2);
    checkAdd->setGenerator(
        [&] (CCallHelpers& jit, const StackmapGenerationParams& params) {
            AllowMacroScratchRegisterUsage allowScratch(jit);
            CHECK(params.size() == 2);
            CHECK(params[0].isGPR());
            CHECK(params[1].isConstant());
            CHECK(params[1].value() == 42);
            jit.convertInt32ToDouble(params[0].gpr(), FPRInfo::fpRegT0);
            jit.convertInt32ToDouble(CCallHelpers::TrustedImm32(42), FPRInfo::fpRegT1);
            jit.addDouble(FPRInfo::fpRegT1, FPRInfo::fpRegT0);
            jit.emitFunctionEpilogue();
            jit.ret();
        });
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(proc, IToD, Origin(), checkAdd));

    auto code = compileProc(proc);

    CHECK(invoke<double>(*code, 0) == 42.0);
    CHECK(invoke<double>(*code, 1) == 43.0);
    CHECK(invoke<double>(*code, 42) == 84.0);
    CHECK(invoke<double>(*code, 2147483647) == 2147483689.0);
}

void testCheckAddImmCommute()
{
    Procedure proc;
    if (proc.optLevel() < 1)
        return;
    BasicBlock* root = proc.addBlock();
    Value* arg1 = root->appendNew<Value>(
        proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* arg2 = root->appendNew<Const32Value>(proc, Origin(), 42);
    CheckValue* checkAdd = root->appendNew<CheckValue>(proc, CheckAdd, Origin(), arg2, arg1);
    checkAdd->append(arg1);
    checkAdd->append(arg2);
    checkAdd->setGenerator(
        [&] (CCallHelpers& jit, const StackmapGenerationParams& params) {
            AllowMacroScratchRegisterUsage allowScratch(jit);
            CHECK(params.size() == 2);
            CHECK(params[0].isGPR());
            CHECK(params[1].isConstant());
            CHECK(params[1].value() == 42);
            jit.convertInt32ToDouble(params[0].gpr(), FPRInfo::fpRegT0);
            jit.convertInt32ToDouble(CCallHelpers::TrustedImm32(42), FPRInfo::fpRegT1);
            jit.addDouble(FPRInfo::fpRegT1, FPRInfo::fpRegT0);
            jit.emitFunctionEpilogue();
            jit.ret();
        });
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(proc, IToD, Origin(), checkAdd));

    auto code = compileProc(proc);

    CHECK(invoke<double>(*code, 0) == 42.0);
    CHECK(invoke<double>(*code, 1) == 43.0);
    CHECK(invoke<double>(*code, 42) == 84.0);
    CHECK(invoke<double>(*code, 2147483647) == 2147483689.0);
}

void testCheckAddImmSomeRegister()
{
    Procedure proc;
    if (proc.optLevel() < 1)
        return;
    BasicBlock* root = proc.addBlock();
    Value* arg1 = root->appendNew<Value>(
        proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* arg2 = root->appendNew<Const32Value>(proc, Origin(), 42);
    CheckValue* checkAdd = root->appendNew<CheckValue>(proc, CheckAdd, Origin(), arg1, arg2);
    checkAdd->appendSomeRegister(arg1);
    checkAdd->appendSomeRegister(arg2);
    checkAdd->setGenerator(
        [&] (CCallHelpers& jit, const StackmapGenerationParams& params) {
            AllowMacroScratchRegisterUsage allowScratch(jit);
            CHECK(params.size() == 2);
            CHECK(params[0].isGPR());
            CHECK(params[1].isGPR());
            jit.convertInt32ToDouble(params[0].gpr(), FPRInfo::fpRegT0);
            jit.convertInt32ToDouble(params[1].gpr(), FPRInfo::fpRegT1);
            jit.addDouble(FPRInfo::fpRegT1, FPRInfo::fpRegT0);
            jit.emitFunctionEpilogue();
            jit.ret();
        });
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(proc, IToD, Origin(), checkAdd));

    auto code = compileProc(proc);

    CHECK(invoke<double>(*code, 0) == 42.0);
    CHECK(invoke<double>(*code, 1) == 43.0);
    CHECK(invoke<double>(*code, 42) == 84.0);
    CHECK(invoke<double>(*code, 2147483647) == 2147483689.0);
}

void testCheckAdd()
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
    CheckValue* checkAdd = root->appendNew<CheckValue>(proc, CheckAdd, Origin(), arg1, arg2);
    checkAdd->appendSomeRegister(arg1);
    checkAdd->appendSomeRegister(arg2);
    checkAdd->setGenerator(
        [&] (CCallHelpers& jit, const StackmapGenerationParams& params) {
            AllowMacroScratchRegisterUsage allowScratch(jit);
            CHECK(params.size() == 2);
            CHECK(params[0].isGPR());
            CHECK(params[1].isGPR());
            jit.convertInt32ToDouble(params[0].gpr(), FPRInfo::fpRegT0);
            jit.convertInt32ToDouble(params[1].gpr(), FPRInfo::fpRegT1);
            jit.addDouble(FPRInfo::fpRegT1, FPRInfo::fpRegT0);
            jit.emitFunctionEpilogue();
            jit.ret();
        });
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(proc, IToD, Origin(), checkAdd));

    auto code = compileProc(proc);

    CHECK(invoke<double>(*code, 0, 42) == 42.0);
    CHECK(invoke<double>(*code, 1, 42) == 43.0);
    CHECK(invoke<double>(*code, 42, 42) == 84.0);
    CHECK(invoke<double>(*code, 2147483647, 42) == 2147483689.0);
}

void testCheckAdd64()
{
    Procedure proc;
    if (proc.optLevel() < 1)
        return;
    BasicBlock* root = proc.addBlock();
    Value* arg1 = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    Value* arg2 = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
    CheckValue* checkAdd = root->appendNew<CheckValue>(proc, CheckAdd, Origin(), arg1, arg2);
    checkAdd->appendSomeRegister(arg1);
    checkAdd->appendSomeRegister(arg2);
    checkAdd->setGenerator(
        [&] (CCallHelpers& jit, const StackmapGenerationParams& params) {
            AllowMacroScratchRegisterUsage allowScratch(jit);
            CHECK(params.size() == 2);
            CHECK(params[0].isGPR());
            CHECK(params[1].isGPR());
            jit.convertInt64ToDouble(params[0].gpr(), FPRInfo::fpRegT0);
            jit.convertInt64ToDouble(params[1].gpr(), FPRInfo::fpRegT1);
            jit.addDouble(FPRInfo::fpRegT1, FPRInfo::fpRegT0);
            jit.emitFunctionEpilogue();
            jit.ret();
        });
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(proc, IToD, Origin(), checkAdd));

    auto code = compileProc(proc);

    CHECK(invoke<double>(*code, 0ll, 42ll) == 42.0);
    CHECK(invoke<double>(*code, 1ll, 42ll) == 43.0);
    CHECK(invoke<double>(*code, 42ll, 42ll) == 84.0);
    CHECK(invoke<double>(*code, 9223372036854775807ll, 42ll) == static_cast<double>(9223372036854775807ll) + 42.0);
}

void testCheckAddFold(int a, int b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* arg1 = root->appendNew<Const32Value>(proc, Origin(), a);
    Value* arg2 = root->appendNew<Const32Value>(proc, Origin(), b);
    CheckValue* checkAdd = root->appendNew<CheckValue>(proc, CheckAdd, Origin(), arg1, arg2);
    unsigned optLevel = proc.optLevel();
    checkAdd->setGenerator(
        [&] (CCallHelpers&, const StackmapGenerationParams&) {
            if (optLevel > 1)
                CHECK(!"Should have been folded");
        });
    root->appendNewControlValue(proc, Return, Origin(), checkAdd);

    auto code = compileProc(proc);

    CHECK(invoke<int>(*code) == a + b);
}

void testCheckAddFoldFail(int a, int b)
{
    Procedure proc;
    if (proc.optLevel() < 1)
        return;
    BasicBlock* root = proc.addBlock();
    Value* arg1 = root->appendNew<Const32Value>(proc, Origin(), a);
    Value* arg2 = root->appendNew<Const32Value>(proc, Origin(), b);
    CheckValue* checkAdd = root->appendNew<CheckValue>(proc, CheckAdd, Origin(), arg1, arg2);
    checkAdd->setGenerator(
        [&] (CCallHelpers& jit, const StackmapGenerationParams&) {
            AllowMacroScratchRegisterUsage allowScratch(jit);
            jit.move(CCallHelpers::TrustedImm32(42), GPRInfo::returnValueGPR);
            jit.emitFunctionEpilogue();
            jit.ret();
        });
    root->appendNewControlValue(proc, Return, Origin(), checkAdd);

    auto code = compileProc(proc);

    CHECK(invoke<int>(*code) == 42);
}

void testCheckAddArgumentAliasing64()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* arg1 = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    Value* arg2 = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
    Value* arg3 = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR2);

    // Pretend to use all the args.
    PatchpointValue* useArgs = root->appendNew<PatchpointValue>(proc, Void, Origin());
    useArgs->append(ConstrainedValue(arg1, ValueRep::SomeRegister));
    useArgs->append(ConstrainedValue(arg2, ValueRep::SomeRegister));
    useArgs->append(ConstrainedValue(arg3, ValueRep::SomeRegister));
    useArgs->setGenerator([&] (CCallHelpers&, const StackmapGenerationParams&) { });

    // Last use of first arg (here, arg1).
    CheckValue* checkAdd1 = root->appendNew<CheckValue>(proc, CheckAdd, Origin(), arg1, arg2);
    checkAdd1->setGenerator([&] (CCallHelpers& jit, const StackmapGenerationParams&) { jit.oops(); });

    // Last use of second arg (here, arg2).
    CheckValue* checkAdd2 = root->appendNew<CheckValue>(proc, CheckAdd, Origin(), arg3, arg2);
    checkAdd2->setGenerator([&] (CCallHelpers& jit, const StackmapGenerationParams&) { jit.oops(); });

    // Keep arg3 live.
    PatchpointValue* keepArg2Live = root->appendNew<PatchpointValue>(proc, Void, Origin());
    keepArg2Live->append(ConstrainedValue(arg2, ValueRep::SomeRegister));
    keepArg2Live->setGenerator([&] (CCallHelpers&, const StackmapGenerationParams&) { });

    // Only use of checkAdd1 and checkAdd2.
    CheckValue* checkAdd3 = root->appendNew<CheckValue>(proc, CheckAdd, Origin(), checkAdd1, checkAdd2);
    checkAdd3->setGenerator([&] (CCallHelpers& jit, const StackmapGenerationParams&) { jit.oops(); });

    root->appendNewControlValue(proc, Return, Origin(), checkAdd3);

    CHECK(compileAndRun<int64_t>(proc, 1, 2, 3) == 8);
}

void testCheckAddArgumentAliasing32()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* arg1 = root->appendNew<Value>(
        proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* arg2 = root->appendNew<Value>(
        proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1));
    Value* arg3 = root->appendNew<Value>(
        proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR2));

    // Pretend to use all the args.
    PatchpointValue* useArgs = root->appendNew<PatchpointValue>(proc, Void, Origin());
    useArgs->append(ConstrainedValue(arg1, ValueRep::SomeRegister));
    useArgs->append(ConstrainedValue(arg2, ValueRep::SomeRegister));
    useArgs->append(ConstrainedValue(arg3, ValueRep::SomeRegister));
    useArgs->setGenerator([&] (CCallHelpers&, const StackmapGenerationParams&) { });

    // Last use of first arg (here, arg1).
    CheckValue* checkAdd1 = root->appendNew<CheckValue>(proc, CheckAdd, Origin(), arg1, arg2);
    checkAdd1->setGenerator([&] (CCallHelpers& jit, const StackmapGenerationParams&) { jit.oops(); });

    // Last use of second arg (here, arg3).
    CheckValue* checkAdd2 = root->appendNew<CheckValue>(proc, CheckAdd, Origin(), arg2, arg3);
    checkAdd2->setGenerator([&] (CCallHelpers& jit, const StackmapGenerationParams&) { jit.oops(); });

    // Keep arg3 live.
    PatchpointValue* keepArg2Live = root->appendNew<PatchpointValue>(proc, Void, Origin());
    keepArg2Live->append(ConstrainedValue(arg2, ValueRep::SomeRegister));
    keepArg2Live->setGenerator([&] (CCallHelpers&, const StackmapGenerationParams&) { });

    // Only use of checkAdd1 and checkAdd2.
    CheckValue* checkAdd3 = root->appendNew<CheckValue>(proc, CheckAdd, Origin(), checkAdd1, checkAdd2);
    checkAdd3->setGenerator([&] (CCallHelpers& jit, const StackmapGenerationParams&) { jit.oops(); });

    root->appendNewControlValue(proc, Return, Origin(), checkAdd3);

    CHECK(compileAndRun<int32_t>(proc, 1, 2, 3) == 8);
}

void testCheckAddSelfOverflow64()
{
    Procedure proc;
    if (proc.optLevel() < 1)
        return;
    BasicBlock* root = proc.addBlock();
    Value* arg = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    CheckValue* checkAdd = root->appendNew<CheckValue>(proc, CheckAdd, Origin(), arg, arg);
    checkAdd->append(arg);
    checkAdd->setGenerator(
        [&] (CCallHelpers& jit, const StackmapGenerationParams& params) {
            AllowMacroScratchRegisterUsage allowScratch(jit);
            jit.move(params[0].gpr(), GPRInfo::returnValueGPR);
            jit.emitFunctionEpilogue();
            jit.ret();
        });

    // Make sure the arg is not the destination of the operation.
    PatchpointValue* opaqueUse = root->appendNew<PatchpointValue>(proc, Void, Origin());
    opaqueUse->append(ConstrainedValue(arg, ValueRep::SomeRegister));
    opaqueUse->setGenerator([&] (CCallHelpers&, const StackmapGenerationParams&) { });

    root->appendNewControlValue(proc, Return, Origin(), checkAdd);

    auto code = compileProc(proc);

    CHECK(invoke<int64_t>(*code, 0ll) == 0);
    CHECK(invoke<int64_t>(*code, 1ll) == 2);
    CHECK(invoke<int64_t>(*code, std::numeric_limits<int64_t>::max()) == std::numeric_limits<int64_t>::max());
}

void testCheckAddSelfOverflow32()
{
    Procedure proc;
    if (proc.optLevel() < 1)
        return;
    BasicBlock* root = proc.addBlock();
    Value* arg = root->appendNew<Value>(
        proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    CheckValue* checkAdd = root->appendNew<CheckValue>(proc, CheckAdd, Origin(), arg, arg);
    checkAdd->append(arg);
    checkAdd->setGenerator(
        [&] (CCallHelpers& jit, const StackmapGenerationParams& params) {
            AllowMacroScratchRegisterUsage allowScratch(jit);
            jit.move(params[0].gpr(), GPRInfo::returnValueGPR);
            jit.emitFunctionEpilogue();
            jit.ret();
        });

    // Make sure the arg is not the destination of the operation.
    PatchpointValue* opaqueUse = root->appendNew<PatchpointValue>(proc, Void, Origin());
    opaqueUse->append(ConstrainedValue(arg, ValueRep::SomeRegister));
    opaqueUse->setGenerator([&] (CCallHelpers&, const StackmapGenerationParams&) { });

    root->appendNewControlValue(proc, Return, Origin(), checkAdd);

    auto code = compileProc(proc);

    CHECK(invoke<int32_t>(*code, 0ll) == 0);
    CHECK(invoke<int32_t>(*code, 1ll) == 2);
    CHECK(invoke<int32_t>(*code, std::numeric_limits<int32_t>::max()) == std::numeric_limits<int32_t>::max());
}

void testCheckAddRemoveCheckWithSExt8(int8_t value)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* arg = root->appendNew<Value>(proc, SExt32, Origin(), root->appendNew<Value>(proc, SExt8, Origin(), root->appendNew<Value>(proc, Trunc, Origin(), root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0))));
    CheckValue* checkAdd = root->appendNew<CheckValue>(proc, CheckAdd, Origin(), arg, arg);
    checkAdd->setGenerator(
        [&] (CCallHelpers& jit, const StackmapGenerationParams&) {
            AllowMacroScratchRegisterUsage allowScratch(jit);
            jit.abortWithReason(B3Oops);
        });
    root->appendNewControlValue(proc, Return, Origin(), checkAdd);

    auto code = compileProc(proc);

    CHECK(invoke<int64_t>(*code, value) == 2ll * static_cast<int32_t>(value));
}

void testCheckAddRemoveCheckWithSExt16(int16_t value)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* arg = root->appendNew<Value>(proc, SExt32, Origin(), root->appendNew<Value>(proc, SExt16, Origin(), root->appendNew<Value>(proc, Trunc, Origin(), root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0))));
    CheckValue* checkAdd = root->appendNew<CheckValue>(proc, CheckAdd, Origin(), arg, arg);
    checkAdd->setGenerator(
        [&] (CCallHelpers& jit, const StackmapGenerationParams&) {
            AllowMacroScratchRegisterUsage allowScratch(jit);
            jit.abortWithReason(B3Oops);
        });
    root->appendNewControlValue(proc, Return, Origin(), checkAdd);

    auto code = compileProc(proc);

    CHECK(invoke<int64_t>(*code, value) == 2ll * static_cast<int32_t>(value));
}

void testCheckAddRemoveCheckWithSExt32(int32_t value)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* arg = root->appendNew<Value>(proc, SExt32, Origin(), root->appendNew<Value>(proc, Trunc, Origin(), root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)));
    CheckValue* checkAdd = root->appendNew<CheckValue>(proc, CheckAdd, Origin(), arg, arg);
    checkAdd->setGenerator(
        [&] (CCallHelpers& jit, const StackmapGenerationParams&) {
            AllowMacroScratchRegisterUsage allowScratch(jit);
            jit.abortWithReason(B3Oops);
        });
    root->appendNewControlValue(proc, Return, Origin(), checkAdd);

    auto code = compileProc(proc);

    CHECK(invoke<int64_t>(*code, value) == 2ll * value);
}

void testCheckAddRemoveCheckWithZExt32(int32_t value)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* arg = root->appendNew<Value>(proc, ZExt32, Origin(), root->appendNew<Value>(proc, Trunc, Origin(), root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)));
    CheckValue* checkAdd = root->appendNew<CheckValue>(proc, CheckAdd, Origin(), arg, arg);
    checkAdd->setGenerator(
        [&] (CCallHelpers& jit, const StackmapGenerationParams&) {
            AllowMacroScratchRegisterUsage allowScratch(jit);
            jit.abortWithReason(B3Oops);
        });
    root->appendNewControlValue(proc, Return, Origin(), checkAdd);

    auto code = compileProc(proc);

    CHECK(invoke<uint64_t>(*code, value) == static_cast<uint64_t>(2 * static_cast<uint64_t>(static_cast<uint32_t>(value))));
}

void testCheckSubImm()
{
    Procedure proc;
    if (proc.optLevel() < 1)
        return;
    BasicBlock* root = proc.addBlock();
    Value* arg1 = root->appendNew<Value>(
        proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* arg2 = root->appendNew<Const32Value>(proc, Origin(), 42);
    CheckValue* checkSub = root->appendNew<CheckValue>(proc, CheckSub, Origin(), arg1, arg2);
    checkSub->append(arg1);
    checkSub->append(arg2);
    checkSub->setGenerator(
        [&] (CCallHelpers& jit, const StackmapGenerationParams& params) {
            AllowMacroScratchRegisterUsage allowScratch(jit);
            CHECK(params.size() == 2);
            CHECK(params[0].isGPR());
            CHECK(params[1].isConstant());
            CHECK(params[1].value() == 42);
            jit.convertInt32ToDouble(params[0].gpr(), FPRInfo::fpRegT0);
            jit.convertInt32ToDouble(CCallHelpers::TrustedImm32(42), FPRInfo::fpRegT1);
            jit.subDouble(FPRInfo::fpRegT1, FPRInfo::fpRegT0);
            jit.emitFunctionEpilogue();
            jit.ret();
        });
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(proc, IToD, Origin(), checkSub));

    auto code = compileProc(proc);

    CHECK(invoke<double>(*code, 0) == -42.0);
    CHECK(invoke<double>(*code, 1) == -41.0);
    CHECK(invoke<double>(*code, 42) == 0.0);
    CHECK(invoke<double>(*code, -2147483647) == -2147483689.0);
}

void testCheckSubBadImm()
{
    Procedure proc;
    if (proc.optLevel() < 1)
        return;
    BasicBlock* root = proc.addBlock();
    Value* arg1 = root->appendNew<Value>(
        proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    int32_t badImm = std::numeric_limits<int>::min();
    Value* arg2 = root->appendNew<Const32Value>(proc, Origin(), badImm);
    CheckValue* checkSub = root->appendNew<CheckValue>(proc, CheckSub, Origin(), arg1, arg2);
    checkSub->append(arg1);
    checkSub->append(arg2);
    checkSub->setGenerator(
        [&] (CCallHelpers& jit, const StackmapGenerationParams& params) {
            AllowMacroScratchRegisterUsage allowScratch(jit);
            CHECK(params.size() == 2);
            CHECK(params[0].isGPR());
            jit.convertInt32ToDouble(params[0].gpr(), FPRInfo::fpRegT0);

            if (params[1].isConstant()) {
                CHECK(params[1].value() == badImm);
                jit.convertInt32ToDouble(CCallHelpers::TrustedImm32(badImm), FPRInfo::fpRegT1);
            } else {
                CHECK(params[1].isGPR());
                jit.convertInt32ToDouble(params[1].gpr(), FPRInfo::fpRegT1);
            }
            jit.subDouble(FPRInfo::fpRegT1, FPRInfo::fpRegT0);
            jit.emitFunctionEpilogue();
            jit.ret();
        });
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(proc, IToD, Origin(), checkSub));

    auto code = compileProc(proc);

    CHECK(invoke<double>(*code, 0) == -static_cast<double>(badImm));
    CHECK(invoke<double>(*code, -1) == -static_cast<double>(badImm) - 1);
    CHECK(invoke<double>(*code, 1) == -static_cast<double>(badImm) + 1);
    CHECK(invoke<double>(*code, 42) == -static_cast<double>(badImm) + 42);
}

void testCheckSub()
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
    CheckValue* checkSub = root->appendNew<CheckValue>(proc, CheckSub, Origin(), arg1, arg2);
    checkSub->append(arg1);
    checkSub->append(arg2);
    checkSub->setGenerator(
        [&] (CCallHelpers& jit, const StackmapGenerationParams& params) {
            AllowMacroScratchRegisterUsage allowScratch(jit);
            CHECK(params.size() == 2);
            CHECK(params[0].isGPR());
            CHECK(params[1].isGPR());
            jit.convertInt32ToDouble(params[0].gpr(), FPRInfo::fpRegT0);
            jit.convertInt32ToDouble(params[1].gpr(), FPRInfo::fpRegT1);
            jit.subDouble(FPRInfo::fpRegT1, FPRInfo::fpRegT0);
            jit.emitFunctionEpilogue();
            jit.ret();
        });
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(proc, IToD, Origin(), checkSub));

    auto code = compileProc(proc);

    CHECK(invoke<double>(*code, 0, 42) == -42.0);
    CHECK(invoke<double>(*code, 1, 42) == -41.0);
    CHECK(invoke<double>(*code, 42, 42) == 0.0);
    CHECK(invoke<double>(*code, -2147483647, 42) == -2147483689.0);
}

void testCheckSubBitAnd()
{
    Procedure proc;
    if (proc.optLevel() < 1)
        return;
    BasicBlock* root = proc.addBlock();
    Value* zero = root->appendNew<Const32Value>(proc, Origin(), 0);
    Value* arg1 = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    Value* truncatedArg1 = root->appendNew<Value>(proc, Trunc, Origin(), arg1);
    Value* minusTwo = root->appendNew<Const32Value>(proc, Origin(), -2);
    Value* bitAnd = root->appendNew<Value>(proc, BitAnd, Origin(), truncatedArg1, minusTwo);
    CheckValue* checkSub = root->appendNew<CheckValue>(proc, CheckSub, Origin(), zero, bitAnd);
    checkSub->setGenerator([&] (CCallHelpers& jit, const StackmapGenerationParams&) {
        AllowMacroScratchRegisterUsage allowScratch(jit);
        jit.move(CCallHelpers::TrustedImm32(42), GPRInfo::returnValueGPR);
        jit.emitFunctionEpilogue();
        jit.ret();
    });
    root->appendNewControlValue(proc, Return, Origin(), checkSub);

    auto code = compileProc(proc);

    CHECK_EQ(invoke<int>(*code, 1), 0);
    CHECK_EQ(invoke<int>(*code, 2), -2);
    CHECK_EQ(invoke<int>(*code, 3), -2);
    CHECK_EQ(invoke<int>(*code, -1), 2);
    CHECK_EQ(invoke<int>(*code, -2), 2);
    CHECK_EQ(invoke<int>(*code, -3), 4);
    CHECK_EQ(invoke<int>(*code, INT_MAX), -(INT_MAX - 1));
    CHECK_EQ(invoke<int>(*code, INT_MIN), 42);
}

NEVER_INLINE double doubleSub(double a, double b)
{
    return a - b;
}

void testCheckSub64()
{
    Procedure proc;
    if (proc.optLevel() < 1)
        return;
    BasicBlock* root = proc.addBlock();
    Value* arg1 = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    Value* arg2 = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
    CheckValue* checkSub = root->appendNew<CheckValue>(proc, CheckSub, Origin(), arg1, arg2);
    checkSub->append(arg1);
    checkSub->append(arg2);
    checkSub->setGenerator(
        [&] (CCallHelpers& jit, const StackmapGenerationParams& params) {
            AllowMacroScratchRegisterUsage allowScratch(jit);
            CHECK(params.size() == 2);
            CHECK(params[0].isGPR());
            CHECK(params[1].isGPR());
            jit.convertInt64ToDouble(params[0].gpr(), FPRInfo::fpRegT0);
            jit.convertInt64ToDouble(params[1].gpr(), FPRInfo::fpRegT1);
            jit.subDouble(FPRInfo::fpRegT1, FPRInfo::fpRegT0);
            jit.emitFunctionEpilogue();
            jit.ret();
        });
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(proc, IToD, Origin(), checkSub));

    auto code = compileProc(proc);

    CHECK(invoke<double>(*code, 0ll, 42ll) == -42.0);
    CHECK(invoke<double>(*code, 1ll, 42ll) == -41.0);
    CHECK(invoke<double>(*code, 42ll, 42ll) == 0.0);
    CHECK(invoke<double>(*code, -9223372036854775807ll, 42ll) == doubleSub(static_cast<double>(-9223372036854775807ll), 42.0));
}

void testCheckSubFold(int a, int b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* arg1 = root->appendNew<Const32Value>(proc, Origin(), a);
    Value* arg2 = root->appendNew<Const32Value>(proc, Origin(), b);
    CheckValue* checkSub = root->appendNew<CheckValue>(proc, CheckSub, Origin(), arg1, arg2);
    unsigned optLevel = proc.optLevel();
    checkSub->setGenerator(
        [&] (CCallHelpers&, const StackmapGenerationParams&) {
            if (optLevel > 1)
                CHECK(!"Should have been folded");
        });
    root->appendNewControlValue(proc, Return, Origin(), checkSub);

    auto code = compileProc(proc);

    CHECK(invoke<int>(*code) == a - b);
}

void testCheckSubFoldFail(int a, int b)
{
    Procedure proc;
    if (proc.optLevel() < 1)
        return;
    BasicBlock* root = proc.addBlock();
    Value* arg1 = root->appendNew<Const32Value>(proc, Origin(), a);
    Value* arg2 = root->appendNew<Const32Value>(proc, Origin(), b);
    CheckValue* checkSub = root->appendNew<CheckValue>(proc, CheckSub, Origin(), arg1, arg2);
    checkSub->setGenerator(
        [&] (CCallHelpers& jit, const StackmapGenerationParams&) {
            AllowMacroScratchRegisterUsage allowScratch(jit);
            jit.move(CCallHelpers::TrustedImm32(42), GPRInfo::returnValueGPR);
            jit.emitFunctionEpilogue();
            jit.ret();
        });
    root->appendNewControlValue(proc, Return, Origin(), checkSub);

    auto code = compileProc(proc);

    CHECK(invoke<int>(*code) == 42);
}

void testCheckNeg()
{
    Procedure proc;
    if (proc.optLevel() < 1)
        return;
    BasicBlock* root = proc.addBlock();
    Value* arg1 = root->appendNew<Const32Value>(proc, Origin(), 0);
    Value* arg2 = root->appendNew<Value>(
        proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    CheckValue* checkNeg = root->appendNew<CheckValue>(proc, CheckSub, Origin(), arg1, arg2);
    checkNeg->append(arg2);
    checkNeg->setGenerator(
        [&] (CCallHelpers& jit, const StackmapGenerationParams& params) {
            AllowMacroScratchRegisterUsage allowScratch(jit);
            CHECK(params.size() == 1);
            CHECK(params[0].isGPR());
            jit.convertInt32ToDouble(params[0].gpr(), FPRInfo::fpRegT1);
            jit.negateDouble(FPRInfo::fpRegT1, FPRInfo::fpRegT0);
            jit.emitFunctionEpilogue();
            jit.ret();
        });
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(proc, IToD, Origin(), checkNeg));

    auto code = compileProc(proc);

    CHECK(invoke<double>(*code, 0) == 0.0);
    CHECK(invoke<double>(*code, 1) == -1.0);
    CHECK(invoke<double>(*code, 42) == -42.0);
    CHECK(invoke<double>(*code, -2147483647 - 1) == 2147483648.0);
}

void testCheckNeg64()
{
    Procedure proc;
    if (proc.optLevel() < 1)
        return;
    BasicBlock* root = proc.addBlock();
    Value* arg1 = root->appendNew<Const64Value>(proc, Origin(), 0);
    Value* arg2 = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    CheckValue* checkNeg = root->appendNew<CheckValue>(proc, CheckSub, Origin(), arg1, arg2);
    checkNeg->append(arg2);
    checkNeg->setGenerator(
        [&] (CCallHelpers& jit, const StackmapGenerationParams& params) {
            AllowMacroScratchRegisterUsage allowScratch(jit);
            CHECK(params.size() == 1);
            CHECK(params[0].isGPR());
            jit.convertInt64ToDouble(params[0].gpr(), FPRInfo::fpRegT1);
            jit.negateDouble(FPRInfo::fpRegT1, FPRInfo::fpRegT0);
            jit.emitFunctionEpilogue();
            jit.ret();
        });
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(proc, IToD, Origin(), checkNeg));

    auto code = compileProc(proc);

    CHECK(invoke<double>(*code, 0ll) == 0.0);
    CHECK(invoke<double>(*code, 1ll) == -1.0);
    CHECK(invoke<double>(*code, 42ll) == -42.0);
    CHECK(invoke<double>(*code, -9223372036854775807ll - 1) == 9223372036854775808.0);
}

void testCheckMul()
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
    CheckValue* checkMul = root->appendNew<CheckValue>(proc, CheckMul, Origin(), arg1, arg2);
    checkMul->append(arg1);
    checkMul->append(arg2);
    checkMul->setGenerator(
        [&] (CCallHelpers& jit, const StackmapGenerationParams& params) {
            AllowMacroScratchRegisterUsage allowScratch(jit);
            CHECK(params.size() == 2);
            CHECK(params[0].isGPR());
            CHECK(params[1].isGPR());
            jit.convertInt32ToDouble(params[0].gpr(), FPRInfo::fpRegT0);
            jit.convertInt32ToDouble(params[1].gpr(), FPRInfo::fpRegT1);
            jit.mulDouble(FPRInfo::fpRegT1, FPRInfo::fpRegT0);
            jit.emitFunctionEpilogue();
            jit.ret();
        });
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(proc, IToD, Origin(), checkMul));

    auto code = compileProc(proc);

    CHECK(invoke<double>(*code, 0, 42) == 0.0);
    CHECK(invoke<double>(*code, 1, 42) == 42.0);
    CHECK(invoke<double>(*code, 42, 42) == 42.0 * 42.0);
    CHECK(invoke<double>(*code, 2147483647, 42) == 2147483647.0 * 42.0);
}

void testCheckMulMemory()
{
    Procedure proc;
    if (proc.optLevel() < 1)
        return;
    BasicBlock* root = proc.addBlock();

    int left;
    int right;

    Value* arg1 = root->appendNew<MemoryValue>(
        proc, Load, Int32, Origin(),
        root->appendNew<ConstPtrValue>(proc, Origin(), &left));
    Value* arg2 = root->appendNew<MemoryValue>(
        proc, Load, Int32, Origin(),
        root->appendNew<ConstPtrValue>(proc, Origin(), &right));
    CheckValue* checkMul = root->appendNew<CheckValue>(proc, CheckMul, Origin(), arg1, arg2);
    checkMul->append(arg1);
    checkMul->append(arg2);
    checkMul->setGenerator(
        [&] (CCallHelpers& jit, const StackmapGenerationParams& params) {
            AllowMacroScratchRegisterUsage allowScratch(jit);
            CHECK(params.size() == 2);
            CHECK(params[0].isGPR());
            CHECK(params[1].isGPR());
            jit.convertInt32ToDouble(params[0].gpr(), FPRInfo::fpRegT0);
            jit.convertInt32ToDouble(params[1].gpr(), FPRInfo::fpRegT1);
            jit.mulDouble(FPRInfo::fpRegT1, FPRInfo::fpRegT0);
            jit.emitFunctionEpilogue();
            jit.ret();
        });
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(proc, IToD, Origin(), checkMul));

    auto code = compileProc(proc);

    left = 0;
    right = 42;
    CHECK(invoke<double>(*code) == 0.0);

    left = 1;
    right = 42;
    CHECK(invoke<double>(*code) == 42.0);

    left = 42;
    right = 42;
    CHECK(invoke<double>(*code) == 42.0 * 42.0);

    left = 2147483647;
    right = 42;
    CHECK(invoke<double>(*code) == 2147483647.0 * 42.0);
}

void testCheckMul2()
{
    Procedure proc;
    if (proc.optLevel() < 1)
        return;
    BasicBlock* root = proc.addBlock();
    Value* arg1 = root->appendNew<Value>(
        proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* arg2 = root->appendNew<Const32Value>(proc, Origin(), 2);
    CheckValue* checkMul = root->appendNew<CheckValue>(proc, CheckMul, Origin(), arg1, arg2);
    checkMul->append(arg1);
    checkMul->append(arg2);
    checkMul->setGenerator(
        [&] (CCallHelpers& jit, const StackmapGenerationParams& params) {
            AllowMacroScratchRegisterUsage allowScratch(jit);
            CHECK(params.size() == 2);
            CHECK(params[0].isGPR());
            CHECK(params[1].isConstant());
            CHECK(params[1].value() == 2);
            jit.convertInt32ToDouble(params[0].gpr(), FPRInfo::fpRegT0);
            jit.convertInt32ToDouble(CCallHelpers::TrustedImm32(2), FPRInfo::fpRegT1);
            jit.mulDouble(FPRInfo::fpRegT1, FPRInfo::fpRegT0);
            jit.emitFunctionEpilogue();
            jit.ret();
        });
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(proc, IToD, Origin(), checkMul));

    auto code = compileProc(proc);

    CHECK(invoke<double>(*code, 0) == 0.0);
    CHECK(invoke<double>(*code, 1) == 2.0);
    CHECK(invoke<double>(*code, 42) == 42.0 * 2.0);
    CHECK(invoke<double>(*code, 2147483647) == 2147483647.0 * 2.0);
}

void testCheckMul64()
{
    Procedure proc;
    if (proc.optLevel() < 1)
        return;
    BasicBlock* root = proc.addBlock();
    Value* arg1 = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    Value* arg2 = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
    CheckValue* checkMul = root->appendNew<CheckValue>(proc, CheckMul, Origin(), arg1, arg2);
    checkMul->append(arg1);
    checkMul->append(arg2);
    checkMul->setGenerator(
        [&] (CCallHelpers& jit, const StackmapGenerationParams& params) {
            AllowMacroScratchRegisterUsage allowScratch(jit);
            CHECK(params.size() == 2);
            CHECK(params[0].isGPR());
            CHECK(params[1].isGPR());
            jit.convertInt64ToDouble(params[0].gpr(), FPRInfo::fpRegT0);
            jit.convertInt64ToDouble(params[1].gpr(), FPRInfo::fpRegT1);
            jit.mulDouble(FPRInfo::fpRegT1, FPRInfo::fpRegT0);
            jit.emitFunctionEpilogue();
            jit.ret();
        });
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(proc, IToD, Origin(), checkMul));

    auto code = compileProc(proc);

    CHECK(invoke<double>(*code, 0, 42) == 0.0);
    CHECK(invoke<double>(*code, 1, 42) == 42.0);
    CHECK(invoke<double>(*code, 42, 42) == 42.0 * 42.0);
    CHECK(invoke<double>(*code, 9223372036854775807ll, 42) == static_cast<double>(9223372036854775807ll) * 42.0);
}

void testCheckMulFold(int a, int b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* arg1 = root->appendNew<Const32Value>(proc, Origin(), a);
    Value* arg2 = root->appendNew<Const32Value>(proc, Origin(), b);
    CheckValue* checkMul = root->appendNew<CheckValue>(proc, CheckMul, Origin(), arg1, arg2);
    unsigned optLevel = proc.optLevel();
    checkMul->setGenerator(
        [&] (CCallHelpers&, const StackmapGenerationParams&) {
            if (optLevel > 1)
                CHECK(!"Should have been folded");
        });
    root->appendNewControlValue(proc, Return, Origin(), checkMul);

    auto code = compileProc(proc);

    CHECK(invoke<int>(*code) == a * b);
}

void testCheckMulFoldFail(int a, int b)
{
    Procedure proc;
    if (proc.optLevel() < 1)
        return;
    BasicBlock* root = proc.addBlock();
    Value* arg1 = root->appendNew<Const32Value>(proc, Origin(), a);
    Value* arg2 = root->appendNew<Const32Value>(proc, Origin(), b);
    CheckValue* checkMul = root->appendNew<CheckValue>(proc, CheckMul, Origin(), arg1, arg2);
    checkMul->setGenerator(
        [&] (CCallHelpers& jit, const StackmapGenerationParams&) {
            AllowMacroScratchRegisterUsage allowScratch(jit);
            jit.move(CCallHelpers::TrustedImm32(42), GPRInfo::returnValueGPR);
            jit.emitFunctionEpilogue();
            jit.ret();
        });
    root->appendNewControlValue(proc, Return, Origin(), checkMul);

    auto code = compileProc(proc);

    CHECK(invoke<int>(*code) == 42);
}

void testCheckMulArgumentAliasing64()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* arg1 = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    Value* arg2 = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
    Value* arg3 = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR2);

    // Pretend to use all the args.
    PatchpointValue* useArgs = root->appendNew<PatchpointValue>(proc, Void, Origin());
    useArgs->append(ConstrainedValue(arg1, ValueRep::SomeRegister));
    useArgs->append(ConstrainedValue(arg2, ValueRep::SomeRegister));
    useArgs->append(ConstrainedValue(arg3, ValueRep::SomeRegister));
    useArgs->setGenerator([&] (CCallHelpers&, const StackmapGenerationParams&) { });

    // Last use of first arg (here, arg1).
    CheckValue* checkMul1 = root->appendNew<CheckValue>(proc, CheckMul, Origin(), arg1, arg2);
    checkMul1->setGenerator([&] (CCallHelpers& jit, const StackmapGenerationParams&) { jit.oops(); });

    // Last use of second arg (here, arg2).
    CheckValue* checkMul2 = root->appendNew<CheckValue>(proc, CheckMul, Origin(), arg3, arg2);
    checkMul2->setGenerator([&] (CCallHelpers& jit, const StackmapGenerationParams&) { jit.oops(); });

    // Keep arg3 live.
    PatchpointValue* keepArg2Live = root->appendNew<PatchpointValue>(proc, Void, Origin());
    keepArg2Live->append(ConstrainedValue(arg2, ValueRep::SomeRegister));
    keepArg2Live->setGenerator([&] (CCallHelpers&, const StackmapGenerationParams&) { });

    // Only use of checkMul1 and checkMul2.
    CheckValue* checkMul3 = root->appendNew<CheckValue>(proc, CheckMul, Origin(), checkMul1, checkMul2);
    checkMul3->setGenerator([&] (CCallHelpers& jit, const StackmapGenerationParams&) { jit.oops(); });

    root->appendNewControlValue(proc, Return, Origin(), checkMul3);

    CHECK(compileAndRun<int64_t>(proc, 2, 3, 4) == 72);
}

void testCheckMulArgumentAliasing32()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* arg1 = root->appendNew<Value>(
        proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* arg2 = root->appendNew<Value>(
        proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1));
    Value* arg3 = root->appendNew<Value>(
        proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR2));

    // Pretend to use all the args.
    PatchpointValue* useArgs = root->appendNew<PatchpointValue>(proc, Void, Origin());
    useArgs->append(ConstrainedValue(arg1, ValueRep::SomeRegister));
    useArgs->append(ConstrainedValue(arg2, ValueRep::SomeRegister));
    useArgs->append(ConstrainedValue(arg3, ValueRep::SomeRegister));
    useArgs->setGenerator([&] (CCallHelpers&, const StackmapGenerationParams&) { });

    // Last use of first arg (here, arg1).
    CheckValue* checkMul1 = root->appendNew<CheckValue>(proc, CheckMul, Origin(), arg1, arg2);
    checkMul1->setGenerator([&] (CCallHelpers& jit, const StackmapGenerationParams&) { jit.oops(); });

    // Last use of second arg (here, arg3).
    CheckValue* checkMul2 = root->appendNew<CheckValue>(proc, CheckMul, Origin(), arg2, arg3);
    checkMul2->setGenerator([&] (CCallHelpers& jit, const StackmapGenerationParams&) { jit.oops(); });

    // Keep arg3 live.
    PatchpointValue* keepArg2Live = root->appendNew<PatchpointValue>(proc, Void, Origin());
    keepArg2Live->append(ConstrainedValue(arg2, ValueRep::SomeRegister));
    keepArg2Live->setGenerator([&] (CCallHelpers&, const StackmapGenerationParams&) { });

    // Only use of checkMul1 and checkMul2.
    CheckValue* checkMul3 = root->appendNew<CheckValue>(proc, CheckMul, Origin(), checkMul1, checkMul2);
    checkMul3->setGenerator([&] (CCallHelpers& jit, const StackmapGenerationParams&) { jit.oops(); });

    root->appendNewControlValue(proc, Return, Origin(), checkMul3);

    CHECK(compileAndRun<int32_t>(proc, 2, 3, 4) == 72);
}

void testCheckMul64SShr()
{
    Procedure proc;
    if (proc.optLevel() < 1)
        return;
    BasicBlock* root = proc.addBlock();
    Value* arg1 = root->appendNew<Value>(
        proc, SShr, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
        root->appendNew<Const32Value>(proc, Origin(), 1));
    Value* arg2 = root->appendNew<Value>(
        proc, SShr, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1),
        root->appendNew<Const32Value>(proc, Origin(), 1));
    CheckValue* checkMul = root->appendNew<CheckValue>(proc, CheckMul, Origin(), arg1, arg2);
    checkMul->append(arg1);
    checkMul->append(arg2);
    checkMul->setGenerator(
        [&] (CCallHelpers& jit, const StackmapGenerationParams& params) {
            AllowMacroScratchRegisterUsage allowScratch(jit);
            CHECK(params.size() == 2);
            CHECK(params[0].isGPR());
            CHECK(params[1].isGPR());
            jit.convertInt64ToDouble(params[0].gpr(), FPRInfo::fpRegT0);
            jit.convertInt64ToDouble(params[1].gpr(), FPRInfo::fpRegT1);
            jit.mulDouble(FPRInfo::fpRegT1, FPRInfo::fpRegT0);
            jit.emitFunctionEpilogue();
            jit.ret();
        });
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(proc, IToD, Origin(), checkMul));

    auto code = compileProc(proc);

    CHECK(invoke<double>(*code, 0ll, 42ll) == 0.0);
    CHECK(invoke<double>(*code, 1ll, 42ll) == 0.0);
    CHECK(invoke<double>(*code, 42ll, 42ll) == (42.0 / 2.0) * (42.0 / 2.0));
    CHECK(invoke<double>(*code, 10000000000ll, 10000000000ll) == 25000000000000000000.0);
}

template<typename LeftFunctor, typename RightFunctor, typename InputType>
void genericTestCompare(
    B3::Opcode opcode, const LeftFunctor& leftFunctor, const RightFunctor& rightFunctor,
    InputType left, InputType right, int result)
{
    // Using a compare.
    {
        Procedure proc;
        BasicBlock* root = proc.addBlock();

        Value* leftValue = leftFunctor(root, proc);
        Value* rightValue = rightFunctor(root, proc);
        Value* comparisonResult = root->appendNew<Value>(proc, opcode, Origin(), leftValue, rightValue);
    
        root->appendNewControlValue(
            proc, Return, Origin(),
            root->appendNew<Value>(
                proc, NotEqual, Origin(),
                comparisonResult,
                root->appendIntConstant(proc, Origin(), comparisonResult->type(), 0)));

        CHECK(compileAndRun<int>(proc, left, right) == result);
    }

    // Using a branch.
    {
        Procedure proc;
        BasicBlock* root = proc.addBlock();
        BasicBlock* thenCase = proc.addBlock();
        BasicBlock* elseCase = proc.addBlock();

        Value* leftValue = leftFunctor(root, proc);
        Value* rightValue = rightFunctor(root, proc);

        root->appendNewControlValue(
            proc, Branch, Origin(),
            root->appendNew<Value>(proc, opcode, Origin(), leftValue, rightValue),
            FrequentedBlock(thenCase), FrequentedBlock(elseCase));

        // We use a patchpoint on the then case to ensure that this doesn't get if-converted.
        PatchpointValue* patchpoint = thenCase->appendNew<PatchpointValue>(proc, Int32, Origin());
        patchpoint->setGenerator(
            [&] (CCallHelpers& jit, const StackmapGenerationParams& params) {
                AllowMacroScratchRegisterUsage allowScratch(jit);
                CHECK(params.size() == 1);
                CHECK(params[0].isGPR());
                jit.move(CCallHelpers::TrustedImm32(1), params[0].gpr());
            });
        thenCase->appendNewControlValue(proc, Return, Origin(), patchpoint);

        elseCase->appendNewControlValue(
            proc, Return, Origin(),
            elseCase->appendNew<Const32Value>(proc, Origin(), 0));

        CHECK(compileAndRun<int>(proc, left, right) == result);
    }
}

template<typename InputType>
InputType modelCompare(B3::Opcode opcode, InputType left, InputType right)
{
    switch (opcode) {
    case Equal:
        return left == right;
    case NotEqual:
        return left != right;
    case LessThan:
        return left < right;
    case GreaterThan:
        return left > right;
    case LessEqual:
        return left <= right;
    case GreaterEqual:
        return left >= right;
    case Above:
        return static_cast<typename std::make_unsigned<InputType>::type>(left) >
            static_cast<typename std::make_unsigned<InputType>::type>(right);
    case Below:
        return static_cast<typename std::make_unsigned<InputType>::type>(left) <
            static_cast<typename std::make_unsigned<InputType>::type>(right);
    case AboveEqual:
        return static_cast<typename std::make_unsigned<InputType>::type>(left) >=
            static_cast<typename std::make_unsigned<InputType>::type>(right);
    case BelowEqual:
        return static_cast<typename std::make_unsigned<InputType>::type>(left) <=
            static_cast<typename std::make_unsigned<InputType>::type>(right);
    case BitAnd:
        return !!(left & right);
    default:
        RELEASE_ASSERT_NOT_REACHED();
        return 0;
    }
}

template<typename T>
void testCompareLoad(B3::Opcode opcode, B3::Opcode loadOpcode, int left, int right)
{
    int result = modelCompare(opcode, modelLoad<T>(left), right);

    // Test addr-to-tmp
    int slot = left;
    genericTestCompare(
        opcode,
        [&] (BasicBlock* block, Procedure& proc) {
            return block->appendNew<MemoryValue>(
                proc, loadOpcode, Int32, Origin(),
                block->appendNew<ConstPtrValue>(proc, Origin(), &slot));
        },
        [&] (BasicBlock* block, Procedure& proc) {
            return block->appendNew<Value>(
                proc, Trunc, Origin(),
                block->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1));
        },
        left, right, result);

    // Test addr-to-imm
    slot = left;
    genericTestCompare(
        opcode,
        [&] (BasicBlock* block, Procedure& proc) {
            return block->appendNew<MemoryValue>(
                proc, loadOpcode, Int32, Origin(),
                block->appendNew<ConstPtrValue>(proc, Origin(), &slot));
        },
        [&] (BasicBlock* block, Procedure& proc) {
            return block->appendNew<Const32Value>(proc, Origin(), right);
        },
        left, right, result);

    result = modelCompare(opcode, left, modelLoad<T>(right));

    // Test tmp-to-addr
    slot = right;
    genericTestCompare(
        opcode,
        [&] (BasicBlock* block, Procedure& proc) {
            return block->appendNew<Value>(
                proc, Trunc, Origin(),
                block->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
        },
        [&] (BasicBlock* block, Procedure& proc) {
            return block->appendNew<MemoryValue>(
                proc, loadOpcode, Int32, Origin(),
                block->appendNew<ConstPtrValue>(proc, Origin(), &slot));
        },
        left, right, result);

    // Test imm-to-addr
    slot = right;
    genericTestCompare(
        opcode,
        [&] (BasicBlock* block, Procedure& proc) {
            return block->appendNew<Const32Value>(proc, Origin(), left);
        },
        [&] (BasicBlock* block, Procedure& proc) {
            return block->appendNew<MemoryValue>(
                proc, loadOpcode, Int32, Origin(),
                block->appendNew<ConstPtrValue>(proc, Origin(), &slot));
        },
        left, right, result);

    // Test addr-to-addr, with the same addr.
    slot = left;
    Value* value;
    genericTestCompare(
        opcode,
        [&] (BasicBlock* block, Procedure& proc) {
            value = block->appendNew<MemoryValue>(
                proc, loadOpcode, Int32, Origin(),
                block->appendNew<ConstPtrValue>(proc, Origin(), &slot));
            return value;
        },
        [&] (BasicBlock*, Procedure&) {
            return value;
        },
        left, left, modelCompare(opcode, modelLoad<T>(left), modelLoad<T>(left)));
}

void testCompareImpl(B3::Opcode opcode, int64_t left, int64_t right)
{
    int64_t result = modelCompare(opcode, left, right);
    int32_t int32Result = modelCompare(opcode, static_cast<int32_t>(left), static_cast<int32_t>(right));

    // Test tmp-to-tmp.
    genericTestCompare(
        opcode,
        [&] (BasicBlock* block, Procedure& proc) {
            return block->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
        },
        [&] (BasicBlock* block, Procedure& proc) {
            return block->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
        },
        left, right, result);
    genericTestCompare(
        opcode,
        [&] (BasicBlock* block, Procedure& proc) {
            return block->appendNew<Value>(
                proc, Trunc, Origin(),
                block->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
        },
        [&] (BasicBlock* block, Procedure& proc) {
            return block->appendNew<Value>(
                proc, Trunc, Origin(),
                block->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1));
        },
        left, right, int32Result);

    // Test imm-to-tmp.
    genericTestCompare(
        opcode,
        [&] (BasicBlock* block, Procedure& proc) {
            return block->appendNew<Const64Value>(proc, Origin(), left);
        },
        [&] (BasicBlock* block, Procedure& proc) {
            return block->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
        },
        left, right, result);
    genericTestCompare(
        opcode,
        [&] (BasicBlock* block, Procedure& proc) {
            return block->appendNew<Const32Value>(proc, Origin(), left);
        },
        [&] (BasicBlock* block, Procedure& proc) {
            return block->appendNew<Value>(
                proc, Trunc, Origin(),
                block->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1));
        },
        left, right, int32Result);

    // Test tmp-to-imm.
    genericTestCompare(
        opcode,
        [&] (BasicBlock* block, Procedure& proc) {
            return block->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
        },
        [&] (BasicBlock* block, Procedure& proc) {
            return block->appendNew<Const64Value>(proc, Origin(), right);
        },
        left, right, result);
    genericTestCompare(
        opcode,
        [&] (BasicBlock* block, Procedure& proc) {
            return block->appendNew<Value>(
                proc, Trunc, Origin(),
                block->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
        },
        [&] (BasicBlock* block, Procedure& proc) {
            return block->appendNew<Const32Value>(proc, Origin(), right);
        },
        left, right, int32Result);

    // Test imm-to-imm.
    genericTestCompare(
        opcode,
        [&] (BasicBlock* block, Procedure& proc) {
            return block->appendNew<Const64Value>(proc, Origin(), left);
        },
        [&] (BasicBlock* block, Procedure& proc) {
            return block->appendNew<Const64Value>(proc, Origin(), right);
        },
        left, right, result);
    genericTestCompare(
        opcode,
        [&] (BasicBlock* block, Procedure& proc) {
            return block->appendNew<Const32Value>(proc, Origin(), left);
        },
        [&] (BasicBlock* block, Procedure& proc) {
            return block->appendNew<Const32Value>(proc, Origin(), right);
        },
        left, right, int32Result);

    testCompareLoad<int32_t>(opcode, Load, left, right);
    testCompareLoad<int8_t>(opcode, Load8S, left, right);
    testCompareLoad<uint8_t>(opcode, Load8Z, left, right);
    testCompareLoad<int16_t>(opcode, Load16S, left, right);
    testCompareLoad<uint16_t>(opcode, Load16Z, left, right);
}

void testCompare(B3::Opcode opcode, int64_t left, int64_t right)
{
    testCompareImpl(opcode, left, right);
    testCompareImpl(opcode, left, right + 1);
    testCompareImpl(opcode, left, right - 1);
}

void testEqualDouble(double left, double right, bool result)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, Equal, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR0),
            root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR1)));

    CHECK(compileAndRun<bool>(proc, left, right) == result);
}

static JSC_DECLARE_JIT_OPERATION_WITHOUT_WTF_INTERNAL(simpleFunction, int, (int, int));
JSC_DEFINE_JIT_OPERATION(simpleFunction, int, (int a, int b))
{
    return a + b;
}

void testCallSimple(int a, int b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<CCallValue>(
            proc, Int32, Origin(),
            root->appendNew<ConstPtrValue>(proc, Origin(), tagCFunction<OperationPtrTag>(simpleFunction)),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1)));

    CHECK(compileAndRun<int>(proc, a, b) == a + b);
}

void testCallRare(int a, int b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* common = proc.addBlock();
    BasicBlock* rare = proc.addBlock();

    root->appendNewControlValue(
        proc, Branch, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
        FrequentedBlock(rare, FrequencyClass::Rare),
        FrequentedBlock(common));

    common->appendNewControlValue(
        proc, Return, Origin(), common->appendNew<Const32Value>(proc, Origin(), 0));

    rare->appendNewControlValue(
        proc, Return, Origin(),
        rare->appendNew<CCallValue>(
            proc, Int32, Origin(),
            rare->appendNew<ConstPtrValue>(proc, Origin(), tagCFunction<OperationPtrTag>(simpleFunction)),
            rare->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1),
            rare->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR2)));

    CHECK(compileAndRun<int>(proc, true, a, b) == a + b);
}

void testCallRareLive(int a, int b, int c)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* common = proc.addBlock();
    BasicBlock* rare = proc.addBlock();

    root->appendNewControlValue(
        proc, Branch, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
        FrequentedBlock(rare, FrequencyClass::Rare),
        FrequentedBlock(common));

    common->appendNewControlValue(
        proc, Return, Origin(), common->appendNew<Const32Value>(proc, Origin(), 0));

    rare->appendNewControlValue(
        proc, Return, Origin(),
        rare->appendNew<Value>(
            proc, Add, Origin(),
            rare->appendNew<CCallValue>(
                proc, Int32, Origin(),
                rare->appendNew<ConstPtrValue>(proc, Origin(), tagCFunction<OperationPtrTag>(simpleFunction)),
                rare->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1),
                rare->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR2)),
            rare->appendNew<Value>(
                proc, Trunc, Origin(),
                rare->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR3))));

    CHECK(compileAndRun<int>(proc, true, a, b, c) == a + b + c);
}

void testCallSimplePure(int a, int b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<CCallValue>(
            proc, Int32, Origin(), Effects::none(),
            root->appendNew<ConstPtrValue>(proc, Origin(), tagCFunction<OperationPtrTag>(simpleFunction)),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1)));

    CHECK(compileAndRun<int>(proc, a, b) == a + b);
}


static JSC_DECLARE_JIT_OPERATION_WITHOUT_WTF_INTERNAL(functionWithHellaArguments, int, (int a, int b, int c, int d, int e, int f, int g, int h, int i, int j, int k, int l, int m, int n, int o, int p, int q, int r, int s, int t, int u, int v, int w, int x, int y, int z));
JSC_DEFINE_JIT_OPERATION(functionWithHellaArguments, int, (int a, int b, int c, int d, int e, int f, int g, int h, int i, int j, int k, int l, int m, int n, int o, int p, int q, int r, int s, int t, int u, int v, int w, int x, int y, int z))
{
    return (a << 0) + (b << 1) + (c << 2) + (d << 3) + (e << 4) + (f << 5) + (g << 6) + (h << 7) + (i << 8) + (j << 9) + (k << 10) + (l << 11) + (m << 12) + (n << 13) + (o << 14) + (p << 15) + (q << 16) + (r << 17) + (s << 18) + (t << 19) + (u << 20) + (v << 21) + (w << 22) + (x << 23) + (y << 24) + (z << 25);
}

void testCallFunctionWithHellaArguments()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();

    Vector<Value*> args;
    for (unsigned i = 0; i < 26; ++i)
        args.append(root->appendNew<Const32Value>(proc, Origin(), i + 1));

    CCallValue* call = root->appendNew<CCallValue>(
        proc, Int32, Origin(),
        root->appendNew<ConstPtrValue>(proc, Origin(), tagCFunction<OperationPtrTag>(functionWithHellaArguments)));
    call->appendArgs(args);

    root->appendNewControlValue(proc, Return, Origin(), call);

    CHECK(compileAndRun<int>(proc) == functionWithHellaArguments(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26));
}

static JSC_DECLARE_JIT_OPERATION_WITHOUT_WTF_INTERNAL(functionWithHellaArguments2, uint64_t, (uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f, uint64_t g, uint64_t h, uint64_t i, uint64_t j, uint64_t k, uint64_t l, uint64_t m, uint64_t n, uint64_t o, uint64_t p, uint64_t q, uint64_t r, uint64_t s, uint64_t t, uint64_t u, uint64_t v, uint64_t w, uint64_t x, uint64_t y, uint64_t z));
JSC_DEFINE_JIT_OPERATION(functionWithHellaArguments2, uint64_t, (uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f, uint64_t g, uint64_t h, uint64_t i, uint64_t j, uint64_t k, uint64_t l, uint64_t m, uint64_t n, uint64_t o, uint64_t p, uint64_t q, uint64_t r, uint64_t s, uint64_t t, uint64_t u, uint64_t v, uint64_t w, uint64_t x, uint64_t y, uint64_t z))
{
    return (a << 0) + (b << 1) + (c << 2) + (d << 3) + (e << 4) + (f << 5) + (g << 6) + (h << 7) + (i << 8) + (j << 9) + (k << 10) + (l << 11) + (m << 12) + (n << 13) + (o << 14) + (p << 15) + (q << 16) + (r << 17) + (s << 18) + (t << 19) + (u << 20) + (v << 21) + (w << 22) + (x << 23) + (y << 24) + (z << 25);
}

void testCallFunctionWithHellaArguments2()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();

    uint64_t limit = static_cast<uint64_t>((1 << 12) - 1); // UINT12_MAX, for arm64 testing.

    Vector<Value*> args;
    for (unsigned i = 0; i < 26; ++i)
        args.append(root->appendNew<Const64Value>(proc, Origin(), limit - i));

    CCallValue* call = root->appendNew<CCallValue>(
        proc, Int64, Origin(),
        root->appendNew<ConstPtrValue>(proc, Origin(), tagCFunction<OperationPtrTag>(functionWithHellaArguments2)));
    call->appendArgs(args);

    root->appendNewControlValue(proc, Return, Origin(), call);

    auto a = compileAndRun<uint64_t>(proc);
    auto b = functionWithHellaArguments2(limit, limit-1, limit-2, limit-3, limit-4, limit-5, limit-6, limit-7, limit-8, limit-9, limit-10, limit-11, limit-12, limit-13, limit-14, limit-15, limit-16, limit-17, limit-18, limit-19, limit-20, limit-21, limit-22, limit-23, limit-24, limit-25);
    CHECK(a == b);
}

static JSC_DECLARE_JIT_OPERATION_WITHOUT_WTF_INTERNAL(functionWithHellaArguments3, int, (int arg0,int arg1,int arg2,int arg3,int arg4,int arg5,int arg6,int arg7,int arg8,int arg9,int arg10,int arg11,int arg12,int arg13,int arg14,int arg15,int arg16,int arg17,int arg18,int arg19,int arg20,int arg21,int arg22,int arg23,int arg24,int arg25,int arg26,int arg27,int arg28,int arg29,int arg30,int arg31,int arg32,int arg33,int arg34,int arg35,int arg36,int arg37,int arg38,int arg39,int arg40,int arg41,int arg42,int arg43,int arg44,int arg45,int arg46,int arg47,int arg48,int arg49,int arg50,int arg51,int arg52,int arg53,int arg54,int arg55,int arg56,int arg57,int arg58,int arg59,int arg60,int arg61,int arg62,int arg63,int arg64,int arg65,int arg66,int arg67,int arg68,int arg69,int arg70,int arg71,int arg72,int arg73,int arg74,int arg75,int arg76,int arg77,int arg78,int arg79,int arg80,int arg81,int arg82,int arg83,int arg84,int arg85,int arg86,int arg87,int arg88,int arg89,int arg90,int arg91,int arg92,int arg93,int arg94,int arg95,int arg96,int arg97,int arg98,int arg99,int arg100,int arg101,int arg102,int arg103,int arg104,int arg105,int arg106,int arg107,int arg108,int arg109,int arg110,int arg111,int arg112,int arg113,int arg114,int arg115,int arg116,int arg117,int arg118,int arg119,int arg120,int arg121,int arg122,int arg123,int arg124,int arg125,int arg126,int arg127,int arg128,int arg129,int arg130,int arg131,int arg132,int arg133,int arg134,int arg135,int arg136,int arg137,int arg138,int arg139,int arg140,int arg141,int arg142,int arg143,int arg144,int arg145,int arg146,int arg147,int arg148,int arg149,int arg150,int arg151,int arg152,int arg153,int arg154,int arg155,int arg156,int arg157,int arg158,int arg159,int arg160,int arg161,int arg162,int arg163,int arg164,int arg165,int arg166,int arg167,int arg168,int arg169,int arg170,int arg171,int arg172,int arg173,int arg174,int arg175,int arg176,int arg177,int arg178,int arg179,int arg180,int arg181,int arg182,int arg183,int arg184,int arg185,int arg186,int arg187,int arg188,int arg189,int arg190,int arg191,int arg192,int arg193,int arg194,int arg195,int arg196,int arg197,int arg198,int arg199,int arg200,int arg201,int arg202,int arg203,int arg204,int arg205,int arg206,int arg207,int arg208,int arg209,int arg210,int arg211,int arg212,int arg213,int arg214,int arg215,int arg216,int arg217,int arg218,int arg219,int arg220,int arg221,int arg222,int arg223,int arg224,int arg225,int arg226,int arg227,int arg228,int arg229,int arg230,int arg231,int arg232,int arg233,int arg234,int arg235,int arg236,int arg237,int arg238,int arg239,int arg240,int arg241,int arg242,int arg243,int arg244,int arg245,int arg246,int arg247,int arg248,int arg249,int arg250,int arg251,int arg252,int arg253,int arg254,int arg255,int arg256,int arg257,int arg258,int arg259,int arg260,int arg261,int arg262,int arg263,int arg264,int arg265,int arg266,int arg267,int arg268,int arg269,int arg270,int arg271,int arg272,int arg273,int arg274,int arg275,int arg276,int arg277,int arg278,int arg279,int arg280,int arg281,int arg282,int arg283,int arg284,int arg285,int arg286,int arg287,int arg288,int arg289,int arg290,int arg291,int arg292,int arg293,int arg294,int arg295,int arg296,int arg297,int arg298,int arg299,int arg300,int arg301,int arg302,int arg303,int arg304,int arg305,int arg306,int arg307,int arg308,int arg309,int arg310,int arg311,int arg312,int arg313,int arg314,int arg315,int arg316,int arg317,int arg318,int arg319,int arg320,int arg321,int arg322,int arg323,int arg324,int arg325,int arg326,int arg327,int arg328,int arg329,int arg330,int arg331,int arg332,int arg333,int arg334,int arg335,int arg336,int arg337,int arg338,int arg339,int arg340,int arg341,int arg342,int arg343,int arg344,int arg345,int arg346,int arg347,int arg348,int arg349,int arg350,int arg351,int arg352,int arg353,int arg354,int arg355,int arg356,int arg357,int arg358,int arg359,int arg360,int arg361,int arg362,int arg363,int arg364,int arg365,int arg366,int arg367,int arg368,int arg369,int arg370,int arg371,int arg372,int arg373,int arg374,int arg375,int arg376,int arg377,int arg378,int arg379,int arg380,int arg381,int arg382,int arg383,int arg384,int arg385,int arg386,int arg387,int arg388,int arg389,int arg390,int arg391,int arg392,int arg393,int arg394,int arg395,int arg396,int arg397,int arg398,int arg399,int arg400,int arg401,int arg402,int arg403,int arg404,int arg405,int arg406,int arg407,int arg408,int arg409,int arg410,int arg411,int arg412,int arg413,int arg414,int arg415,int arg416,int arg417,int arg418,int arg419,int arg420,int arg421,int arg422,int arg423,int arg424,int arg425,int arg426,int arg427,int arg428,int arg429,int arg430,int arg431,int arg432,int arg433,int arg434,int arg435,int arg436,int arg437,int arg438,int arg439,int arg440,int arg441,int arg442,int arg443,int arg444,int arg445,int arg446,int arg447,int arg448,int arg449,int arg450,int arg451,int arg452,int arg453,int arg454,int arg455,int arg456,int arg457,int arg458,int arg459,int arg460,int arg461,int arg462,int arg463,int arg464,int arg465,int arg466,int arg467,int arg468,int arg469,int arg470,int arg471,int arg472,int arg473,int arg474,int arg475,int arg476,int arg477,int arg478,int arg479,int arg480,int arg481,int arg482,int arg483,int arg484,int arg485,int arg486,int arg487,int arg488,int arg489,int arg490,int arg491,int arg492,int arg493,int arg494,int arg495,int arg496,int arg497,int arg498,int arg499));
JSC_DEFINE_JIT_OPERATION(functionWithHellaArguments3, int, (int arg0,int arg1,int arg2,int arg3,int arg4,int arg5,int arg6,int arg7,int arg8,int arg9,int arg10,int arg11,int arg12,int arg13,int arg14,int arg15,int arg16,int arg17,int arg18,int arg19,int arg20,int arg21,int arg22,int arg23,int arg24,int arg25,int arg26,int arg27,int arg28,int arg29,int arg30,int arg31,int arg32,int arg33,int arg34,int arg35,int arg36,int arg37,int arg38,int arg39,int arg40,int arg41,int arg42,int arg43,int arg44,int arg45,int arg46,int arg47,int arg48,int arg49,int arg50,int arg51,int arg52,int arg53,int arg54,int arg55,int arg56,int arg57,int arg58,int arg59,int arg60,int arg61,int arg62,int arg63,int arg64,int arg65,int arg66,int arg67,int arg68,int arg69,int arg70,int arg71,int arg72,int arg73,int arg74,int arg75,int arg76,int arg77,int arg78,int arg79,int arg80,int arg81,int arg82,int arg83,int arg84,int arg85,int arg86,int arg87,int arg88,int arg89,int arg90,int arg91,int arg92,int arg93,int arg94,int arg95,int arg96,int arg97,int arg98,int arg99,int arg100,int arg101,int arg102,int arg103,int arg104,int arg105,int arg106,int arg107,int arg108,int arg109,int arg110,int arg111,int arg112,int arg113,int arg114,int arg115,int arg116,int arg117,int arg118,int arg119,int arg120,int arg121,int arg122,int arg123,int arg124,int arg125,int arg126,int arg127,int arg128,int arg129,int arg130,int arg131,int arg132,int arg133,int arg134,int arg135,int arg136,int arg137,int arg138,int arg139,int arg140,int arg141,int arg142,int arg143,int arg144,int arg145,int arg146,int arg147,int arg148,int arg149,int arg150,int arg151,int arg152,int arg153,int arg154,int arg155,int arg156,int arg157,int arg158,int arg159,int arg160,int arg161,int arg162,int arg163,int arg164,int arg165,int arg166,int arg167,int arg168,int arg169,int arg170,int arg171,int arg172,int arg173,int arg174,int arg175,int arg176,int arg177,int arg178,int arg179,int arg180,int arg181,int arg182,int arg183,int arg184,int arg185,int arg186,int arg187,int arg188,int arg189,int arg190,int arg191,int arg192,int arg193,int arg194,int arg195,int arg196,int arg197,int arg198,int arg199,int arg200,int arg201,int arg202,int arg203,int arg204,int arg205,int arg206,int arg207,int arg208,int arg209,int arg210,int arg211,int arg212,int arg213,int arg214,int arg215,int arg216,int arg217,int arg218,int arg219,int arg220,int arg221,int arg222,int arg223,int arg224,int arg225,int arg226,int arg227,int arg228,int arg229,int arg230,int arg231,int arg232,int arg233,int arg234,int arg235,int arg236,int arg237,int arg238,int arg239,int arg240,int arg241,int arg242,int arg243,int arg244,int arg245,int arg246,int arg247,int arg248,int arg249,int arg250,int arg251,int arg252,int arg253,int arg254,int arg255,int arg256,int arg257,int arg258,int arg259,int arg260,int arg261,int arg262,int arg263,int arg264,int arg265,int arg266,int arg267,int arg268,int arg269,int arg270,int arg271,int arg272,int arg273,int arg274,int arg275,int arg276,int arg277,int arg278,int arg279,int arg280,int arg281,int arg282,int arg283,int arg284,int arg285,int arg286,int arg287,int arg288,int arg289,int arg290,int arg291,int arg292,int arg293,int arg294,int arg295,int arg296,int arg297,int arg298,int arg299,int arg300,int arg301,int arg302,int arg303,int arg304,int arg305,int arg306,int arg307,int arg308,int arg309,int arg310,int arg311,int arg312,int arg313,int arg314,int arg315,int arg316,int arg317,int arg318,int arg319,int arg320,int arg321,int arg322,int arg323,int arg324,int arg325,int arg326,int arg327,int arg328,int arg329,int arg330,int arg331,int arg332,int arg333,int arg334,int arg335,int arg336,int arg337,int arg338,int arg339,int arg340,int arg341,int arg342,int arg343,int arg344,int arg345,int arg346,int arg347,int arg348,int arg349,int arg350,int arg351,int arg352,int arg353,int arg354,int arg355,int arg356,int arg357,int arg358,int arg359,int arg360,int arg361,int arg362,int arg363,int arg364,int arg365,int arg366,int arg367,int arg368,int arg369,int arg370,int arg371,int arg372,int arg373,int arg374,int arg375,int arg376,int arg377,int arg378,int arg379,int arg380,int arg381,int arg382,int arg383,int arg384,int arg385,int arg386,int arg387,int arg388,int arg389,int arg390,int arg391,int arg392,int arg393,int arg394,int arg395,int arg396,int arg397,int arg398,int arg399,int arg400,int arg401,int arg402,int arg403,int arg404,int arg405,int arg406,int arg407,int arg408,int arg409,int arg410,int arg411,int arg412,int arg413,int arg414,int arg415,int arg416,int arg417,int arg418,int arg419,int arg420,int arg421,int arg422,int arg423,int arg424,int arg425,int arg426,int arg427,int arg428,int arg429,int arg430,int arg431,int arg432,int arg433,int arg434,int arg435,int arg436,int arg437,int arg438,int arg439,int arg440,int arg441,int arg442,int arg443,int arg444,int arg445,int arg446,int arg447,int arg448,int arg449,int arg450,int arg451,int arg452,int arg453,int arg454,int arg455,int arg456,int arg457,int arg458,int arg459,int arg460,int arg461,int arg462,int arg463,int arg464,int arg465,int arg466,int arg467,int arg468,int arg469,int arg470,int arg471,int arg472,int arg473,int arg474,int arg475,int arg476,int arg477,int arg478,int arg479,int arg480,int arg481,int arg482,int arg483,int arg484,int arg485,int arg486,int arg487,int arg488,int arg489,int arg490,int arg491,int arg492,int arg493,int arg494,int arg495,int arg496,int arg497,int arg498,int arg499))
{ return arg0+arg1+arg2+arg3+arg4+arg5+arg6+arg7+arg8+arg9+arg10+arg11+arg12+arg13+arg14+arg15+arg16+arg17+arg18+arg19+arg20+arg21+arg22+arg23+arg24+arg25+arg26+arg27+arg28+arg29+arg30+arg31+arg32+arg33+arg34+arg35+arg36+arg37+arg38+arg39+arg40+arg41+arg42+arg43+arg44+arg45+arg46+arg47+arg48+arg49+arg50+arg51+arg52+arg53+arg54+arg55+arg56+arg57+arg58+arg59+arg60+arg61+arg62+arg63+arg64+arg65+arg66+arg67+arg68+arg69+arg70+arg71+arg72+arg73+arg74+arg75+arg76+arg77+arg78+arg79+arg80+arg81+arg82+arg83+arg84+arg85+arg86+arg87+arg88+arg89+arg90+arg91+arg92+arg93+arg94+arg95+arg96+arg97+arg98+arg99+arg100+arg101+arg102+arg103+arg104+arg105+arg106+arg107+arg108+arg109+arg110+arg111+arg112+arg113+arg114+arg115+arg116+arg117+arg118+arg119+arg120+arg121+arg122+arg123+arg124+arg125+arg126+arg127+arg128+arg129+arg130+arg131+arg132+arg133+arg134+arg135+arg136+arg137+arg138+arg139+arg140+arg141+arg142+arg143+arg144+arg145+arg146+arg147+arg148+arg149+arg150+arg151+arg152+arg153+arg154+arg155+arg156+arg157+arg158+arg159+arg160+arg161+arg162+arg163+arg164+arg165+arg166+arg167+arg168+arg169+arg170+arg171+arg172+arg173+arg174+arg175+arg176+arg177+arg178+arg179+arg180+arg181+arg182+arg183+arg184+arg185+arg186+arg187+arg188+arg189+arg190+arg191+arg192+arg193+arg194+arg195+arg196+arg197+arg198+arg199+arg200+arg201+arg202+arg203+arg204+arg205+arg206+arg207+arg208+arg209+arg210+arg211+arg212+arg213+arg214+arg215+arg216+arg217+arg218+arg219+arg220+arg221+arg222+arg223+arg224+arg225+arg226+arg227+arg228+arg229+arg230+arg231+arg232+arg233+arg234+arg235+arg236+arg237+arg238+arg239+arg240+arg241+arg242+arg243+arg244+arg245+arg246+arg247+arg248+arg249+arg250+arg251+arg252+arg253+arg254+arg255+arg256+arg257+arg258+arg259+arg260+arg261+arg262+arg263+arg264+arg265+arg266+arg267+arg268+arg269+arg270+arg271+arg272+arg273+arg274+arg275+arg276+arg277+arg278+arg279+arg280+arg281+arg282+arg283+arg284+arg285+arg286+arg287+arg288+arg289+arg290+arg291+arg292+arg293+arg294+arg295+arg296+arg297+arg298+arg299+arg300+arg301+arg302+arg303+arg304+arg305+arg306+arg307+arg308+arg309+arg310+arg311+arg312+arg313+arg314+arg315+arg316+arg317+arg318+arg319+arg320+arg321+arg322+arg323+arg324+arg325+arg326+arg327+arg328+arg329+arg330+arg331+arg332+arg333+arg334+arg335+arg336+arg337+arg338+arg339+arg340+arg341+arg342+arg343+arg344+arg345+arg346+arg347+arg348+arg349+arg350+arg351+arg352+arg353+arg354+arg355+arg356+arg357+arg358+arg359+arg360+arg361+arg362+arg363+arg364+arg365+arg366+arg367+arg368+arg369+arg370+arg371+arg372+arg373+arg374+arg375+arg376+arg377+arg378+arg379+arg380+arg381+arg382+arg383+arg384+arg385+arg386+arg387+arg388+arg389+arg390+arg391+arg392+arg393+arg394+arg395+arg396+arg397+arg398+arg399+arg400+arg401+arg402+arg403+arg404+arg405+arg406+arg407+arg408+arg409+arg410+arg411+arg412+arg413+arg414+arg415+arg416+arg417+arg418+arg419+arg420+arg421+arg422+arg423+arg424+arg425+arg426+arg427+arg428+arg429+arg430+arg431+arg432+arg433+arg434+arg435+arg436+arg437+arg438+arg439+arg440+arg441+arg442+arg443+arg444+arg445+arg446+arg447+arg448+arg449+arg450+arg451+arg452+arg453+arg454+arg455+arg456+arg457+arg458+arg459+arg460+arg461+arg462+arg463+arg464+arg465+arg466+arg467+arg468+arg469+arg470+arg471+arg472+arg473+arg474+arg475+arg476+arg477+arg478+arg479+arg480+arg481+arg482+arg483+arg484+arg485+arg486+arg487+arg488+arg489+arg490+arg491+arg492+arg493+arg494+arg495+arg496+arg497+arg498+arg499;}
void testCallFunctionWithHellaArguments3()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();

    Vector<Value*> args;
    for (unsigned i = 0; i < 500; ++i)
        args.append(root->appendNew<Const32Value>(proc, Origin(), 4095 - 5000 + i - 1));

    CCallValue* call = root->appendNew<CCallValue>(
        proc, Int32, Origin(),
        root->appendNew<ConstPtrValue>(proc, Origin(), tagCFunction<OperationPtrTag>(functionWithHellaArguments3)));
    call->appendArgs(args);

    root->appendNewControlValue(proc, Return, Origin(), call);

    std::unique_ptr<Compilation> compilation = compileProc(proc);
    CHECK_EQ(invoke<int>(*compilation), invoke<int>(*compilation));
    CHECK_EQ(invoke<int>(*compilation), -328250);
    CHECK_EQ(invoke<int>(*compilation), invoke<int>(*compilation));
    CHECK_EQ(invoke<int>(*compilation), -328250);
    CHECK_EQ(invoke<int>(*compilation), invoke<int>(*compilation));
    CHECK_EQ(invoke<int>(*compilation), -328250);
    CHECK_EQ(invoke<int>(*compilation), invoke<int>(*compilation));
    CHECK_EQ(invoke<int>(*compilation), -328250);
}

void testReturnDouble(double value)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<ConstDoubleValue>(proc, Origin(), value));

    CHECK(isIdentical(compileAndRun<double>(proc), value));
}

void testReturnFloat(float value)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<ConstFloatValue>(proc, Origin(), value));

    CHECK(isIdentical(compileAndRun<float>(proc), value));
}

static JSC_DECLARE_JIT_OPERATION_WITHOUT_WTF_INTERNAL(simpleFunctionDouble, double, (double a, double b));
JSC_DEFINE_JIT_OPERATION(simpleFunctionDouble, double, (double a, double b))
{
    return a + b;
}

void testCallSimpleDouble(double a, double b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<CCallValue>(
            proc, Double, Origin(),
            root->appendNew<ConstPtrValue>(proc, Origin(), tagCFunction<OperationPtrTag>(simpleFunctionDouble)),
            root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR0),
            root->appendNew<ArgumentRegValue>(proc, Origin(), FPRInfo::argumentFPR1)));

    CHECK(compileAndRun<double>(proc, a, b) == a + b);
}

static JSC_DECLARE_JIT_OPERATION_WITHOUT_WTF_INTERNAL(simpleFunctionFloat, float, (float a, float b));
JSC_DEFINE_JIT_OPERATION(simpleFunctionFloat, float, (float a, float b))
{
    return a + b;
}

void testCallSimpleFloat(float a, float b)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    Value* argument1int32 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* argument2int32 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1));
    Value* floatValue1 = root->appendNew<Value>(proc, BitwiseCast, Origin(), argument1int32);
    Value* floatValue2 = root->appendNew<Value>(proc, BitwiseCast, Origin(), argument2int32);
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<CCallValue>(
            proc, Float, Origin(),
            root->appendNew<ConstPtrValue>(proc, Origin(), tagCFunction<OperationPtrTag>(simpleFunctionFloat)),
            floatValue1,
            floatValue2));

    CHECK(isIdentical(compileAndRun<float>(proc, bitwise_cast<int32_t>(a), bitwise_cast<int32_t>(b)), a + b));
}

static JSC_DECLARE_JIT_OPERATION_WITHOUT_WTF_INTERNAL(functionWithHellaDoubleArguments, double, (double a, double b, double c, double d, double e, double f, double g, double h, double i, double j, double k, double l, double m, double n, double o, double p, double q, double r, double s, double t, double u, double v, double w, double x, double y, double z));
JSC_DEFINE_JIT_OPERATION(functionWithHellaDoubleArguments, double, (double a, double b, double c, double d, double e, double f, double g, double h, double i, double j, double k, double l, double m, double n, double o, double p, double q, double r, double s, double t, double u, double v, double w, double x, double y, double z))
{
    return a * pow(2, 0) + b * pow(2, 1) + c * pow(2, 2) + d * pow(2, 3) + e * pow(2, 4) + f * pow(2, 5) + g * pow(2, 6) + h * pow(2, 7) + i * pow(2, 8) + j * pow(2, 9) + k * pow(2, 10) + l * pow(2, 11) + m * pow(2, 12) + n * pow(2, 13) + o * pow(2, 14) + p * pow(2, 15) + q * pow(2, 16) + r * pow(2, 17) + s * pow(2, 18) + t * pow(2, 19) + u * pow(2, 20) + v * pow(2, 21) + w * pow(2, 22) + x * pow(2, 23) + y * pow(2, 24) + z * pow(2, 25);
}

void testCallFunctionWithHellaDoubleArguments()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();

    Vector<Value*> args;
    for (unsigned i = 0; i < 26; ++i)
        args.append(root->appendNew<ConstDoubleValue>(proc, Origin(), i + 1));

    CCallValue* call = root->appendNew<CCallValue>(
        proc, Double, Origin(),
        root->appendNew<ConstPtrValue>(proc, Origin(), tagCFunction<OperationPtrTag>(functionWithHellaDoubleArguments)));
    call->appendArgs(args);

    root->appendNewControlValue(proc, Return, Origin(), call);

    CHECK(compileAndRun<double>(proc) == functionWithHellaDoubleArguments(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26));
}

static JSC_DECLARE_JIT_OPERATION_WITHOUT_WTF_INTERNAL(functionWithHellaFloatArguments, float, (float a, float b, float c, float d, float e, float f, float g, float h, float i, float j, float k, float l, float m, float n, float o, float p, float q, float r, float s, float t, float u, float v, float w, float x, float y, float z));
JSC_DEFINE_JIT_OPERATION(functionWithHellaFloatArguments, float, (float a, float b, float c, float d, float e, float f, float g, float h, float i, float j, float k, float l, float m, float n, float o, float p, float q, float r, float s, float t, float u, float v, float w, float x, float y, float z))
{
    return a * pow(2, 0) + b * pow(2, 1) + c * pow(2, 2) + d * pow(2, 3) + e * pow(2, 4) + f * pow(2, 5) + g * pow(2, 6) + h * pow(2, 7) + i * pow(2, 8) + j * pow(2, 9) + k * pow(2, 10) + l * pow(2, 11) + m * pow(2, 12) + n * pow(2, 13) + o * pow(2, 14) + p * pow(2, 15) + q * pow(2, 16) + r * pow(2, 17) + s * pow(2, 18) + t * pow(2, 19) + u * pow(2, 20) + v * pow(2, 21) + w * pow(2, 22) + x * pow(2, 23) + y * pow(2, 24) + z * pow(2, 25);
}

void testCallFunctionWithHellaFloatArguments()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();

    Vector<Value*> args;
    for (unsigned i = 0; i < 26; ++i)
        args.append(root->appendNew<ConstFloatValue>(proc, Origin(), i + 1));

    CCallValue* call = root->appendNew<CCallValue>(
        proc, Float, Origin(),
        root->appendNew<ConstPtrValue>(proc, Origin(), tagCFunction<OperationPtrTag>(functionWithHellaFloatArguments)));
    call->appendArgs(args);

    root->appendNewControlValue(proc, Return, Origin(), call);

    CHECK(compileAndRun<float>(proc) == functionWithHellaFloatArguments(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26));
}

void testLinearScanWithCalleeOnStack()
{
    // This tests proper CCall generation when compiling with a lower optimization
    // level and operating with a callee argument that's spilt on the stack.
    // On ARM64, this caused an assert in MacroAssemblerARM64 because of disallowed
    // use of the scratch register.
    // https://bugs.webkit.org/show_bug.cgi?id=170672

    Procedure proc;
    BasicBlock* root = proc.addBlock();

    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<CCallValue>(
            proc, Int32, Origin(),
            root->appendNew<ConstPtrValue>(proc, Origin(), tagCFunction<OperationPtrTag>(simpleFunction)),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1)));

    // Force the linear scan algorithm to spill everything.
    auto original = Options::airLinearScanSpillsEverything();
    Options::airLinearScanSpillsEverything() = true;

    // Compiling with 1 as the optimization level enforces the use of linear scan
    // for register allocation.
    auto code = compileProc(proc);
    CHECK_EQ(invoke<int>(*code, 41, 1), 42);

    Options::airLinearScanSpillsEverything() = original;
}

void testChillDiv(int num, int den, int res)
{
    // Test non-constant.
    {
        Procedure proc;
        BasicBlock* root = proc.addBlock();
    
        root->appendNewControlValue(
            proc, Return, Origin(),
            root->appendNew<Value>(
                proc, chill(Div), Origin(),
                root->appendNew<Value>(
                    proc, Trunc, Origin(),
                    root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)),
                root->appendNew<Value>(
                    proc, Trunc, Origin(),
                    root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1))));

        CHECK(compileAndRun<int>(proc, num, den) == res);
    }

    // Test constant.
    {
        Procedure proc;
        BasicBlock* root = proc.addBlock();
    
        root->appendNewControlValue(
            proc, Return, Origin(),
            root->appendNew<Value>(
                proc, chill(Div), Origin(),
                root->appendNew<Const32Value>(proc, Origin(), num),
                root->appendNew<Const32Value>(proc, Origin(), den)));
    
        CHECK(compileAndRun<int>(proc) == res);
    }
}

void testChillDivTwice(int num1, int den1, int num2, int den2, int res)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();

    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, Add, Origin(),
            root->appendNew<Value>(
                proc, chill(Div), Origin(),
                root->appendNew<Value>(
                    proc, Trunc, Origin(),
                    root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)),
                root->appendNew<Value>(
                    proc, Trunc, Origin(),
                    root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1))),
            root->appendNew<Value>(
                proc, chill(Div), Origin(),
                root->appendNew<Value>(
                    proc, Trunc, Origin(),
                    root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR2)),
                root->appendNew<Value>(
                    proc, Trunc, Origin(),
                    root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR3)))));

    CHECK(compileAndRun<int>(proc, num1, den1, num2, den2) == res);
}

void testChillDiv64(int64_t num, int64_t den, int64_t res)
{
    if (!is64Bit())
        return;

    // Test non-constant.
    {
        Procedure proc;
        BasicBlock* root = proc.addBlock();
    
        root->appendNewControlValue(
            proc, Return, Origin(),
            root->appendNew<Value>(
                proc, chill(Div), Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1)));
    
        CHECK(compileAndRun<int64_t>(proc, num, den) == res);
    }

    // Test constant.
    {
        Procedure proc;
        BasicBlock* root = proc.addBlock();
    
        root->appendNewControlValue(
            proc, Return, Origin(),
            root->appendNew<Value>(
                proc, chill(Div), Origin(),
                root->appendNew<Const64Value>(proc, Origin(), num),
                root->appendNew<Const64Value>(proc, Origin(), den)));
    
        CHECK(compileAndRun<int64_t>(proc) == res);
    }
}

void testModArg(int64_t value)
{
    if (!value)
        return;

    Procedure proc;
    BasicBlock* root = proc.addBlock();

    Value* argument = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    Value* result = root->appendNew<Value>(proc, Mod, Origin(), argument, argument);
    root->appendNewControlValue(proc, Return, Origin(), result);

    CHECK(!compileAndRun<int64_t>(proc, value));
}

void testModArgs(int64_t numerator, int64_t denominator)
{
    if (!denominator)
        return;
    if (numerator == std::numeric_limits<int64_t>::min() && denominator == -1)
        return;

    Procedure proc;
    BasicBlock* root = proc.addBlock();

    Value* argument1 = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    Value* argument2 = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
    Value* result = root->appendNew<Value>(proc, Mod, Origin(), argument1, argument2);
    root->appendNewControlValue(proc, Return, Origin(), result);

    CHECK(compileAndRun<int64_t>(proc, numerator, denominator) == numerator % denominator);
}

void testModImms(int64_t numerator, int64_t denominator)
{
    if (!denominator)
        return;
    if (numerator == std::numeric_limits<int64_t>::min() && denominator == -1)
        return;

    Procedure proc;
    BasicBlock* root = proc.addBlock();

    Value* argument1 = root->appendNew<Const64Value>(proc, Origin(), numerator);
    Value* argument2 = root->appendNew<Const64Value>(proc, Origin(), denominator);
    Value* result = root->appendNew<Value>(proc, Mod, Origin(), argument1, argument2);
    root->appendNewControlValue(proc, Return, Origin(), result);

    CHECK(compileAndRun<int64_t>(proc, numerator, denominator) == numerator % denominator);
}

void testModArg32(int32_t value)
{
    if (!value)
        return;

    Procedure proc;
    BasicBlock* root = proc.addBlock();

    Value* argument = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* result = root->appendNew<Value>(proc, Mod, Origin(), argument, argument);
    root->appendNewControlValue(proc, Return, Origin(), result);

    CHECK(!compileAndRun<int32_t>(proc, value));
}

void testModArgs32(int32_t numerator, int32_t denominator)
{
    if (!denominator)
        return;
    if (numerator == std::numeric_limits<int32_t>::min() && denominator == -1)
        return;

    Procedure proc;
    BasicBlock* root = proc.addBlock();

    Value* argument1 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* argument2 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1));
    Value* result = root->appendNew<Value>(proc, Mod, Origin(), argument1, argument2);
    root->appendNewControlValue(proc, Return, Origin(), result);

    CHECK(compileAndRun<int32_t>(proc, numerator, denominator) == numerator % denominator);
}

void testModImms32(int32_t numerator, int32_t denominator)
{
    if (!denominator)
        return;
    if (numerator == std::numeric_limits<int32_t>::min() && denominator == -1)
        return;

    Procedure proc;
    BasicBlock* root = proc.addBlock();

    Value* argument1 = root->appendNew<Const32Value>(proc, Origin(), numerator);
    Value* argument2 = root->appendNew<Const32Value>(proc, Origin(), denominator);
    Value* result = root->appendNew<Value>(proc, Mod, Origin(), argument1, argument2);
    root->appendNewControlValue(proc, Return, Origin(), result);

    CHECK(compileAndRun<int32_t>(proc, numerator, denominator) == numerator % denominator);
}

void testChillModArg(int64_t value)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();

    Value* argument = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    Value* result = root->appendNew<Value>(proc, chill(Mod), Origin(), argument, argument);
    root->appendNewControlValue(proc, Return, Origin(), result);

    CHECK(!compileAndRun<int64_t>(proc, value));
}

void testChillModArgs(int64_t numerator, int64_t denominator)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();

    Value* argument1 = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    Value* argument2 = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
    Value* result = root->appendNew<Value>(proc, chill(Mod), Origin(), argument1, argument2);
    root->appendNewControlValue(proc, Return, Origin(), result);

    CHECK(compileAndRun<int64_t>(proc, numerator, denominator) == chillMod(numerator, denominator));
}

void testChillModImms(int64_t numerator, int64_t denominator)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();

    Value* argument1 = root->appendNew<Const64Value>(proc, Origin(), numerator);
    Value* argument2 = root->appendNew<Const64Value>(proc, Origin(), denominator);
    Value* result = root->appendNew<Value>(proc, chill(Mod), Origin(), argument1, argument2);
    root->appendNewControlValue(proc, Return, Origin(), result);

    CHECK(compileAndRun<int64_t>(proc, numerator, denominator) == chillMod(numerator, denominator));
}

void testChillModArg32(int32_t value)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();

    Value* argument = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* result = root->appendNew<Value>(proc, chill(Mod), Origin(), argument, argument);
    root->appendNewControlValue(proc, Return, Origin(), result);

    CHECK(!compileAndRun<int32_t>(proc, value));
}

void testChillModArgs32(int32_t numerator, int32_t denominator)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();

    Value* argument1 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    Value* argument2 = root->appendNew<Value>(proc, Trunc, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1));
    Value* result = root->appendNew<Value>(proc, chill(Mod), Origin(), argument1, argument2);
    root->appendNewControlValue(proc, Return, Origin(), result);

    CHECK(compileAndRun<int32_t>(proc, numerator, denominator) == chillMod(numerator, denominator));
}

void testChillModImms32(int32_t numerator, int32_t denominator)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();

    Value* argument1 = root->appendNew<Const32Value>(proc, Origin(), numerator);
    Value* argument2 = root->appendNew<Const32Value>(proc, Origin(), denominator);
    Value* result = root->appendNew<Value>(proc, chill(Mod), Origin(), argument1, argument2);
    root->appendNewControlValue(proc, Return, Origin(), result);

    CHECK(compileAndRun<int32_t>(proc, numerator, denominator) == chillMod(numerator, denominator));
}

void testLoopWithMultipleHeaderEdges()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    BasicBlock* innerHeader = proc.addBlock();
    BasicBlock* innerEnd = proc.addBlock();
    BasicBlock* outerHeader = proc.addBlock();
    BasicBlock* outerEnd = proc.addBlock();
    BasicBlock* end = proc.addBlock();

    auto* ne42 = outerHeader->appendNew<Value>(
        proc, NotEqual, Origin(),
        root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0),
        root->appendNew<ConstPtrValue>(proc, Origin(), 42));
    outerHeader->appendNewControlValue(
        proc, Branch, Origin(),
        ne42,
        FrequentedBlock(innerHeader), FrequentedBlock(outerEnd));
    outerEnd->appendNewControlValue(
        proc, Branch, Origin(),
        root->appendNew<Value>(
            proc, Trunc, Origin(),
            root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)),
        FrequentedBlock(outerHeader), FrequentedBlock(end));

    SwitchValue* switchValue = innerHeader->appendNew<SwitchValue>(
        proc, Origin(), root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1));
    switchValue->setFallThrough(FrequentedBlock(innerEnd));
    for (unsigned i = 0; i < 20; ++i) {
        switchValue->appendCase(SwitchCase(i, FrequentedBlock(innerHeader)));
    }

    root->appendNewControlValue(proc, Jump, Origin(), FrequentedBlock(outerHeader));

    innerEnd->appendNewControlValue(proc, Jump, Origin(), FrequentedBlock(outerEnd));
    end->appendNewControlValue(
        proc, Return, Origin(),
        end->appendNew<Const32Value>(proc, Origin(), 5678));

    auto code = compileProc(proc); // This shouldn't crash in computing NaturalLoops.
    CHECK(invoke<int32_t>(*code, 0, 12345) == 5678);
}

void testSwitch(unsigned degree, unsigned gap)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();

    BasicBlock* terminate = proc.addBlock();
    terminate->appendNewControlValue(
        proc, Return, Origin(),
        terminate->appendNew<Const32Value>(proc, Origin(), 0));

    SwitchValue* switchValue = root->appendNew<SwitchValue>(
        proc, Origin(), root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    switchValue->setFallThrough(FrequentedBlock(terminate));

    for (unsigned i = 0; i < degree; ++i) {
        BasicBlock* newBlock = proc.addBlock();
        newBlock->appendNewControlValue(
            proc, Return, Origin(),
            newBlock->appendNew<ArgumentRegValue>(
                proc, Origin(), (i & 1) ? GPRInfo::argumentGPR2 : GPRInfo::argumentGPR1));
        switchValue->appendCase(SwitchCase(gap * i, FrequentedBlock(newBlock)));
    }

    auto code = compileProc(proc);

    for (unsigned i = 0; i < degree; ++i) {
        CHECK(invoke<int32_t>(*code, i * gap, 42, 11) == ((i & 1) ? 11 : 42));
        if (gap > 1) {
            CHECK(!invoke<int32_t>(*code, i * gap + 1, 42, 11));
            CHECK(!invoke<int32_t>(*code, i * gap - 1, 42, 11));
        }
    }

    CHECK(!invoke<int32_t>(*code, -1, 42, 11));
    CHECK(!invoke<int32_t>(*code, degree * gap, 42, 11));
    CHECK(!invoke<int32_t>(*code, degree * gap + 1, 42, 11));
}

void testSwitchSameCaseAsDefault()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();

    BasicBlock* return10 = proc.addBlock();
    return10->appendNewControlValue(
        proc, Return, Origin(),
        return10->appendNew<Const32Value>(proc, Origin(), 10));

    Value* switchOperand = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);

    BasicBlock* caseAndDefault = proc.addBlock();
    caseAndDefault->appendNewControlValue(
        proc, Return, Origin(), 
            caseAndDefault->appendNew<Value>(
                proc, Equal, Origin(),
                switchOperand, caseAndDefault->appendNew<ConstPtrValue>(proc, Origin(), 0)));

    SwitchValue* switchValue = root->appendNew<SwitchValue>(proc, Origin(), switchOperand);

    switchValue->appendCase(SwitchCase(100, FrequentedBlock(return10)));

    // Because caseAndDefault is reached both as default case, and when it's 0,
    // we should not incorrectly optimize and assume that switchOperand==0.
    switchValue->appendCase(SwitchCase(0, FrequentedBlock(caseAndDefault)));
    switchValue->setFallThrough(FrequentedBlock(caseAndDefault));

    auto code = compileProc(proc);

    CHECK(invoke<int32_t>(*code, 100) == 10);
    CHECK(invoke<int32_t>(*code, 0) == 1);
    CHECK(invoke<int32_t>(*code, 1) == 0);
    CHECK(invoke<int32_t>(*code, 2) == 0);
    CHECK(invoke<int32_t>(*code, 99) == 0);
    CHECK(invoke<int32_t>(*code, 0xbaadbeef) == 0);
}

void testSwitchChillDiv(unsigned degree, unsigned gap)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();

    Value* left = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR1);
    Value* right = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR2);

    BasicBlock* terminate = proc.addBlock();
    terminate->appendNewControlValue(
        proc, Return, Origin(),
        terminate->appendNew<Const32Value>(proc, Origin(), 0));

    SwitchValue* switchValue = root->appendNew<SwitchValue>(
        proc, Origin(), root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    switchValue->setFallThrough(FrequentedBlock(terminate));

    for (unsigned i = 0; i < degree; ++i) {
        BasicBlock* newBlock = proc.addBlock();

        newBlock->appendNewControlValue(
            proc, Return, Origin(),
            newBlock->appendNew<Value>(
                proc, chill(Div), Origin(), (i & 1) ? right : left, (i & 1) ? left : right));
    
        switchValue->appendCase(SwitchCase(gap * i, FrequentedBlock(newBlock)));
    }

    auto code = compileProc(proc);

    for (unsigned i = 0; i < degree; ++i) {
        dataLog("i = ", i, "\n");
        int32_t result = invoke<int32_t>(*code, i * gap, 42, 11);
        dataLog("result = ", result, "\n");
        CHECK(result == ((i & 1) ? 11/42 : 42/11));
        if (gap > 1) {
            CHECK(!invoke<int32_t>(*code, i * gap + 1, 42, 11));
            CHECK(!invoke<int32_t>(*code, i * gap - 1, 42, 11));
        }
    }

    CHECK(!invoke<int32_t>(*code, -1, 42, 11));
    CHECK(!invoke<int32_t>(*code, degree * gap, 42, 11));
    CHECK(!invoke<int32_t>(*code, degree * gap + 1, 42, 11));
}

void testSwitchTargettingSameBlock()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();

    BasicBlock* terminate = proc.addBlock();
    terminate->appendNewControlValue(
        proc, Return, Origin(),
        terminate->appendNew<Const32Value>(proc, Origin(), 5));

    SwitchValue* switchValue = root->appendNew<SwitchValue>(
        proc, Origin(), root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0));
    switchValue->setFallThrough(FrequentedBlock(terminate));

    BasicBlock* otherTarget = proc.addBlock();
    otherTarget->appendNewControlValue(
        proc, Return, Origin(),
        otherTarget->appendNew<Const32Value>(proc, Origin(), 42));
    switchValue->appendCase(SwitchCase(3, FrequentedBlock(otherTarget)));
    switchValue->appendCase(SwitchCase(13, FrequentedBlock(otherTarget)));

    auto code = compileProc(proc);

    for (unsigned i = 0; i < 20; ++i) {
        int32_t expected = (i == 3 || i == 13) ? 42 : 5;
        CHECK(invoke<int32_t>(*code, i) == expected);
    }
}

void testSwitchTargettingSameBlockFoldPathConstant()
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();

    BasicBlock* terminate = proc.addBlock();
    terminate->appendNewControlValue(
        proc, Return, Origin(),
        terminate->appendNew<Const32Value>(proc, Origin(), 42));

    Value* argument = root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0);
    SwitchValue* switchValue = root->appendNew<SwitchValue>(proc, Origin(), argument);
    switchValue->setFallThrough(FrequentedBlock(terminate));

    BasicBlock* otherTarget = proc.addBlock();
    otherTarget->appendNewControlValue(
        proc, Return, Origin(), argument);
    switchValue->appendCase(SwitchCase(3, FrequentedBlock(otherTarget)));
    switchValue->appendCase(SwitchCase(13, FrequentedBlock(otherTarget)));

    auto code = compileProc(proc);

    for (unsigned i = 0; i < 20; ++i) {
        int32_t expected = (i == 3 || i == 13) ? i : 42;
        CHECK(invoke<int32_t>(*code, i) == expected);
    }
}

void testTruncFold(int64_t value)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, Trunc, Origin(),
            root->appendNew<Const64Value>(proc, Origin(), value)));

    CHECK(compileAndRun<int>(proc) == static_cast<int>(value));
}

void testZExt32(int32_t value)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, ZExt32, Origin(),
            root->appendNew<Value>(
                proc, Trunc, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0))));

    CHECK(compileAndRun<uint64_t>(proc, value) == static_cast<uint64_t>(static_cast<uint32_t>(value)));
}

void testZExt32Fold(int32_t value)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, ZExt32, Origin(),
            root->appendNew<Const32Value>(proc, Origin(), value)));

    CHECK(compileAndRun<uint64_t>(proc, value) == static_cast<uint64_t>(static_cast<uint32_t>(value)));
}

void testSExt32(int32_t value)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, SExt32, Origin(),
            root->appendNew<Value>(
                proc, Trunc, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0))));

    CHECK(compileAndRun<int64_t>(proc, value) == static_cast<int64_t>(value));
}

void testSExt32Fold(int32_t value)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, SExt32, Origin(),
            root->appendNew<Const32Value>(proc, Origin(), value)));

    CHECK(compileAndRun<int64_t>(proc, value) == static_cast<int64_t>(value));
}

void testTruncZExt32(int32_t value)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, Trunc, Origin(),
            root->appendNew<Value>(
                proc, ZExt32, Origin(),
                root->appendNew<Value>(
                    proc, Trunc, Origin(),
                    root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)))));

    CHECK(compileAndRun<int32_t>(proc, value) == value);
}

void testTruncSExt32(int32_t value)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, Trunc, Origin(),
            root->appendNew<Value>(
                proc, SExt32, Origin(),
                root->appendNew<Value>(
                    proc, Trunc, Origin(),
                    root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)))));

    CHECK(compileAndRun<int32_t>(proc, value) == value);
}

void testSExt8(int32_t value)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, SExt8, Origin(),
            root->appendNew<Value>(
                proc, Trunc, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0))));

    CHECK(compileAndRun<int32_t>(proc, value) == static_cast<int32_t>(static_cast<int8_t>(value)));
}

void testSExt8Fold(int32_t value)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, SExt8, Origin(),
            root->appendNew<Const32Value>(proc, Origin(), value)));

    CHECK(compileAndRun<int32_t>(proc) == static_cast<int32_t>(static_cast<int8_t>(value)));
}

void testSExt8SExt8(int32_t value)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, SExt8, Origin(),
            root->appendNew<Value>(
                proc, SExt8, Origin(),
                root->appendNew<Value>(
                    proc, Trunc, Origin(),
                    root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)))));

    CHECK(compileAndRun<int32_t>(proc, value) == static_cast<int32_t>(static_cast<int8_t>(value)));
}

void testSExt8SExt16(int32_t value)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, SExt8, Origin(),
            root->appendNew<Value>(
                proc, SExt16, Origin(),
                root->appendNew<Value>(
                    proc, Trunc, Origin(),
                    root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)))));

    CHECK(compileAndRun<int32_t>(proc, value) == static_cast<int32_t>(static_cast<int8_t>(value)));
}

void testSExt8BitAnd(int32_t value, int32_t mask)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, SExt8, Origin(),
            root->appendNew<Value>(
                proc, BitAnd, Origin(),
                root->appendNew<Value>(
                    proc, Trunc, Origin(),
                    root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)),
                root->appendNew<Const32Value>(proc, Origin(), mask))));

    CHECK(compileAndRun<int32_t>(proc, value) == static_cast<int32_t>(static_cast<int8_t>(value & mask)));
}

void testBitAndSExt8(int32_t value, int32_t mask)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, BitAnd, Origin(),
            root->appendNew<Value>(
                proc, SExt8, Origin(),
                root->appendNew<Value>(
                    proc, Trunc, Origin(),
                    root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0))),
            root->appendNew<Const32Value>(proc, Origin(), mask)));

    CHECK(compileAndRun<int32_t>(proc, value) == (static_cast<int32_t>(static_cast<int8_t>(value)) & mask));
}

void testSExt16(int32_t value)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, SExt16, Origin(),
            root->appendNew<Value>(
                proc, Trunc, Origin(),
                root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0))));

    CHECK(compileAndRun<int32_t>(proc, value) == static_cast<int32_t>(static_cast<int16_t>(value)));
}

void testSExt16Fold(int32_t value)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, SExt16, Origin(),
            root->appendNew<Const32Value>(proc, Origin(), value)));

    CHECK(compileAndRun<int32_t>(proc) == static_cast<int32_t>(static_cast<int16_t>(value)));
}

void testSExt16SExt16(int32_t value)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, SExt16, Origin(),
            root->appendNew<Value>(
                proc, SExt16, Origin(),
                root->appendNew<Value>(
                    proc, Trunc, Origin(),
                    root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)))));

    CHECK(compileAndRun<int32_t>(proc, value) == static_cast<int32_t>(static_cast<int16_t>(value)));
}

void testSExt16SExt8(int32_t value)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, SExt16, Origin(),
            root->appendNew<Value>(
                proc, SExt8, Origin(),
                root->appendNew<Value>(
                    proc, Trunc, Origin(),
                    root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)))));

    CHECK(compileAndRun<int32_t>(proc, value) == static_cast<int32_t>(static_cast<int8_t>(value)));
}

void testSExt16BitAnd(int32_t value, int32_t mask)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, SExt16, Origin(),
            root->appendNew<Value>(
                proc, BitAnd, Origin(),
                root->appendNew<Value>(
                    proc, Trunc, Origin(),
                    root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)),
                root->appendNew<Const32Value>(proc, Origin(), mask))));

    CHECK(compileAndRun<int32_t>(proc, value) == static_cast<int32_t>(static_cast<int16_t>(value & mask)));
}

void testBitAndSExt16(int32_t value, int32_t mask)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, BitAnd, Origin(),
            root->appendNew<Value>(
                proc, SExt16, Origin(),
                root->appendNew<Value>(
                    proc, Trunc, Origin(),
                    root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0))),
            root->appendNew<Const32Value>(proc, Origin(), mask)));

    CHECK(compileAndRun<int32_t>(proc, value) == (static_cast<int32_t>(static_cast<int16_t>(value)) & mask));
}

void testSExt32BitAnd(int32_t value, int32_t mask)
{
    Procedure proc;
    BasicBlock* root = proc.addBlock();
    root->appendNewControlValue(
        proc, Return, Origin(),
        root->appendNew<Value>(
            proc, SExt32, Origin(),
            root->appendNew<Value>(
                proc, BitAnd, Origin(),
                root->appendNew<Value>(
                    proc, Trunc, Origin(),
                    root->appendNew<ArgumentRegValue>(proc, Origin(), GPRInfo::argumentGPR0)),
                root->appendNew<Const32Value>(proc, Origin(), mask))));

    CHECK(compileAndRun<int64_t>(proc, value) == static_cast<int64_t>(value & mask));
}

#endif // ENABLE(B3_JIT)
